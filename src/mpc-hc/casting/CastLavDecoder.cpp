/*
 * (C) 2026 see Authors.txt
 *
 * This file is part of MPC-HC.
 *
 * MPC-HC is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * MPC-HC is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

#include "stdafx.h"
#include "CastLavDecoder.h"
#include "DSUtil.h"
#include "FGFilterLAV.h"
#include "../filters/Filters.h"
#include "Logger.h"
#include <thread>

namespace
{
    // The splitter exposes a video and an audio output pin; we want the audio one.
    CComPtr<IPin> AudioOutputPin(IBaseFilter* pFilter)
    {
        CComPtr<IEnumPins> pEnum;
        if (!pFilter || FAILED(pFilter->EnumPins(&pEnum))) {
            return nullptr;
        }
        for (CComPtr<IPin> pPin; pEnum->Next(1, &pPin, nullptr) == S_OK; pPin = nullptr) {
            PIN_DIRECTION dir;
            if (FAILED(pPin->QueryDirection(&dir)) || dir != PINDIR_OUTPUT) {
                continue;
            }
            CComPtr<IEnumMediaTypes> pTypes;
            if (FAILED(pPin->EnumMediaTypes(&pTypes))) {
                continue;
            }
            AM_MEDIA_TYPE* pmt = nullptr;
            bool isAudio = false;
            while (pTypes->Next(1, &pmt, nullptr) == S_OK && pmt) {
                isAudio = pmt->majortype == MEDIATYPE_Audio;
                DeleteMediaType(pmt);
                pmt = nullptr;
                if (isAudio) {
                    break;
                }
            }
            if (isAudio) {
                return pPin;
            }
        }
        return nullptr;
    }
}

// Builds and runs the decode graph. Runs on its own thread (see below) so its
// COM apartment is not the caller's UI STA -- a DirectShow graph built there and
// waited on with WaitForCompletion deadlocks, since the wait blocks the very
// thread the graph needs to deliver its completion to.
static bool DecodeGraph(const CString& srcPath, int targetChannels, const CString& outWavPath,
                        CString* pError)
{
    auto fail = [&](const CString & why) -> bool {
        if (pError) { *pError = why; }
        CASTING_LOG(_T("downmix (LAV): %s"), why.GetString());
        DeleteFile(outWavPath);
        return false;
    };

    HRESULT hr = S_OK;

    // The graph, and the two controls used to run it to the end of the file.
    CComPtr<IGraphBuilder> pGraph;
    if (FAILED(pGraph.CoCreateInstance(CLSID_FilterGraph))) {
        return fail(_T("a filter graph could not be created"));
    }
    // No reference clock: this is a file-to-file transcode, so let it run as fast
    // as it can rather than pacing the audio out in real time.
    if (CComQIPtr<IMediaFilter> pMediaFilter = pGraph) {
        pMediaFilter->SetSyncSource(nullptr);
    }
    CComQIPtr<IMediaControl> pControl = pGraph;
    CComQIPtr<IMediaEvent> pEvent = pGraph;
    if (!pControl || !pEvent) {
        return fail(_T("the graph is missing its control interfaces"));
    }

    // The player's own splitter/source and audio decoder, loaded straight from
    // the bundled .ax without relying on them being registered.
    CComPtr<IBaseFilter> pSplitter, pAudio;
    if (FAILED(LoadExternalFilter(CFGFilterLAV::GetFilterPath(CFGFilterLAV::SPLITTER_SOURCE),
                                  GUID_LAVSplitterSource, &pSplitter)) || !pSplitter) {
        return fail(_T("the LAV splitter could not be loaded"));
    }
    if (FAILED(LoadExternalFilter(CFGFilterLAV::GetFilterPath(CFGFilterLAV::AUDIO_DECODER),
                                  GUID_LAVAudio, &pAudio)) || !pAudio) {
        return fail(_T("the LAV audio decoder could not be loaded"));
    }

    if (FAILED(pGraph->AddFilter(pSplitter, L"LAV Splitter Source"))
            || FAILED(pGraph->AddFilter(pAudio, L"LAV Audio Decoder"))) {
        return fail(_T("the LAV filters could not be added to the graph"));
    }

    CComQIPtr<IFileSourceFilter> pSource = pSplitter;
    if (!pSource || FAILED(pSource->Load(srcPath, nullptr))) {
        return fail(_T("the file could not be opened by the splitter"));
    }

    // Make the audio decoder downmix to stereo, isolated from the user's saved
    // settings. Stereo is the LAV engine's floor -- it makes the sound audible on
    // a device that could not output the original layout, which is the point;
    // preserving 5.1 through this path is a later refinement.
    UNREFERENCED_PARAMETER(targetChannels);
    if (CComQIPtr<ILAVAudioSettings> pLav = pAudio) {
        pLav->SetRuntimeConfig(TRUE);
        pLav->SetMixingEnabled(TRUE);
        pLav->SetMixingLayout(0x3); // stereo (FL FR)
        pLav->SetMixingFlags(LAV_MIXING_FLAG_CLIP_PROTECTION | LAV_MIXING_FLAG_NORMALIZE_MATRIX);
        pLav->SetSampleFormat(SampleFormat_16, TRUE);
        pLav->SetOutputStandardLayout(TRUE);
    }

    // The WAV sink: WavDest turns the decoder's PCM into a RIFF/WAVE stream, the
    // File Writer puts it on disk.
    // The DShow base class delegates its IUnknown, so reach IBaseFilter through
    // the non-delegating unknown -- a plain cast of the C++ object gives a wrong
    // vtable and any call through it hangs.
    CComPtr<IUnknown> pWavUnk = (IUnknown*)(INonDelegatingUnknown*)DEBUG_NEW CWavDestFilter(nullptr, &hr);
    if (FAILED(hr) || !pWavUnk) {
        return fail(_T("the WAV muxer could not be created"));
    }
    CComQIPtr<IBaseFilter> pWavDest = pWavUnk;
    if (!pWavDest) {
        return fail(_T("the WAV muxer has no filter interface"));
    }
    CComPtr<IBaseFilter> pWriter;
    if (FAILED(pWriter.CoCreateInstance(CLSID_FileWriter))) {
        return fail(_T("the file writer could not be created"));
    }
    if (CComQIPtr<IFileSinkFilter2> pSink = pWriter) {
        pSink->SetFileName(CStringW(outWavPath), nullptr);
        pSink->SetMode(AM_FILE_OVERWRITE);
    } else {
        return fail(_T("the file writer has no sink interface"));
    }
    if (FAILED(pGraph->AddFilter(pWavDest, L"WavDest")) || FAILED(pGraph->AddFilter(pWriter, L"File Writer"))) {
        return fail(_T("the WAV sink could not be added to the graph"));
    }

    // Wire it up: splitter audio -> decoder -> WavDest -> file. Direct, so the
    // graph builder cannot substitute some other decoder for LAV.
    CComPtr<IPin> pSplitterAudio = AudioOutputPin(pSplitter);
    if (!pSplitterAudio) {
        return fail(_T("the file has no audio track"));
    }
    if (FAILED(hr = pGraph->ConnectDirect(pSplitterAudio, GetFirstPin(pAudio, PINDIR_INPUT), nullptr))) {
        return fail(_T("the audio decoder would not accept the track"));
    }
    // Let the decoder offer its own PCM type (16-bit stereo at the source rate,
    // per the settings above) and the muxer take it -- forcing a type here made
    // LAV deliver at the wrong rate, doubling the running time.
    if (FAILED(hr = pGraph->ConnectDirect(GetFirstPin(pAudio, PINDIR_OUTPUT),
                                          GetFirstPin(pWavDest, PINDIR_INPUT), nullptr))) {
        return fail(_T("the WAV muxer would not accept the decoded audio"));
    }
    if (FAILED(hr = pGraph->ConnectDirect(GetFirstPin(pWavDest, PINDIR_OUTPUT),
                                          GetFirstPin(pWriter, PINDIR_INPUT), nullptr))) {
        return fail(_T("the file writer would not accept the WAV stream"));
    }

    // Run to the end of the file. The source is finite, so this completes.
    if (FAILED(hr = pControl->Run())) {
        return fail(_T("the decode graph would not run"));
    }
    long evCode = 0;
    pEvent->WaitForCompletion(INFINITE, &evCode);
    pControl->Stop();
    if (evCode != EC_COMPLETE) {
        return fail(_T("the decode did not finish cleanly"));
    }

    CASTING_LOG(_T("downmix (LAV): decoded the audio to a %d-channel WAV"), targetChannels >= 6 ? 6 : 2);
    return true;
}

bool CastLavDecodeToWav(const CString& srcPath, int targetChannels, const CString& outWavPath,
                        CString* pError)
{
    // The graph runs on this worker thread, in its own COM apartment, and the
    // caller blocks on the join. The caller's UI thread is thus free of the
    // graph entirely, so nothing the graph does has to marshal into a thread
    // that is blocked waiting for it.
    bool ok = false;
    CString err;
    std::thread worker([&]() {
        // The multithreaded apartment, not an STA: a headless transcode graph has
        // no message pump, and DirectShow in an STA without one deadlocks.
        const HRESULT hrCo = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
        ok = DecodeGraph(srcPath, targetChannels, outWavPath, &err);
        if (SUCCEEDED(hrCo)) {
            CoUninitialize();
        }
    });
    worker.join();
    if (!ok && pError) {
        *pError = err;
    }
    return ok;
}
