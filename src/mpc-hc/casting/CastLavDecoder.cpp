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
#include "NullRenderers.h"
#include "Logger.h"
#include <mfapi.h>
#include <mfidl.h>
#include <mfreadwrite.h>
#include <thread>

#pragma comment(lib, "mfplat.lib")
#pragma comment(lib, "mfreadwrite.lib")
#pragma comment(lib, "mfuuid.lib")

#ifndef MFAudioFormat_FLAC
// The player targets a Windows version whose mfapi.h guards this out, but the
// FLAC encoder MFT is there at run time on Windows 10. WAVE_FORMAT_FLAC 0xF1AC.
static const GUID MFAudioFormat_FLAC =
{ 0x0000F1AC, 0x0000, 0x0010, { 0x80, 0x00, 0x00, 0xAA, 0x00, 0x38, 0x9B, 0x71 } };
#endif

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

    // A DirectShow renderer that hands the PCM it receives straight to a Media
    // Foundation sink writer, which encodes it to FLAC. This is why the audio the
    // player's LAV decoder produces never lands on disk as a giant PCM WAV -- it
    // is compressed losslessly as it flows. The sink writer is opened and closed
    // on the caller's thread (StartWriter/FinalizeWriter); only WriteSample runs
    // on the streaming thread, which it tolerates.
    class __declspec(uuid("6B3F1E22-9C4A-4E2B-8B3D-2E7A1C9F5D40"))
        CFlacSinkRenderer : public CNullRenderer
    {
        CComPtr<IMFSinkWriter> m_writer;
        DWORD m_stream = 0;
        CStringW m_path;
        bool m_ok = true;

    public:
        CFlacSinkRenderer(LPCWSTR path, HRESULT* phr)
            : CNullRenderer(__uuidof(CFlacSinkRenderer), NAME("MPC FLAC sink"), nullptr, phr)
            , m_path(path) {}

        bool Ok() const { return m_ok; }

        HRESULT CheckMediaType(const CMediaType* pmt) override
        {
            if (pmt->majortype != MEDIATYPE_Audio || pmt->subtype != MEDIASUBTYPE_PCM
                    || pmt->formattype != FORMAT_WaveFormatEx) {
                return VFW_E_TYPE_NOT_ACCEPTED;
            }
            return S_OK;
        }

        // Opens the FLAC writer from the negotiated input format. Called after the
        // pins connect and before the graph runs, on the graph-building thread.
        HRESULT StartWriter()
        {
            AM_MEDIA_TYPE amt;
            if (FAILED(m_pInputPin->ConnectionMediaType(&amt))) {
                m_ok = false;
                return E_FAIL;
            }
            if (amt.formattype != FORMAT_WaveFormatEx || !amt.pbFormat
                    || amt.cbFormat < sizeof(WAVEFORMATEX)) {
                FreeMediaType(amt);
                m_ok = false;
                return E_FAIL;
            }
            const WAVEFORMATEX wfe = *reinterpret_cast<const WAVEFORMATEX*>(amt.pbFormat);
            FreeMediaType(amt);

            CComPtr<IMFMediaType> pcm;
            MFCreateMediaType(&pcm);
            pcm->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Audio);
            pcm->SetGUID(MF_MT_SUBTYPE, MFAudioFormat_PCM);
            pcm->SetUINT32(MF_MT_AUDIO_BITS_PER_SAMPLE, wfe.wBitsPerSample);
            pcm->SetUINT32(MF_MT_AUDIO_SAMPLES_PER_SECOND, wfe.nSamplesPerSec);
            pcm->SetUINT32(MF_MT_AUDIO_NUM_CHANNELS, wfe.nChannels);
            pcm->SetUINT32(MF_MT_AUDIO_BLOCK_ALIGNMENT, wfe.nBlockAlign);
            pcm->SetUINT32(MF_MT_AUDIO_AVG_BYTES_PER_SECOND, wfe.nAvgBytesPerSec);

            if (FAILED(MFCreateSinkWriterFromURL(m_path, nullptr, nullptr, &m_writer))) {
                m_ok = false;
                return E_FAIL;
            }
            CComPtr<IMFMediaType> flac;
            MFCreateMediaType(&flac);
            flac->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Audio);
            flac->SetGUID(MF_MT_SUBTYPE, MFAudioFormat_FLAC);
            flac->SetUINT32(MF_MT_AUDIO_BITS_PER_SAMPLE, wfe.wBitsPerSample);
            flac->SetUINT32(MF_MT_AUDIO_SAMPLES_PER_SECOND, wfe.nSamplesPerSec);
            flac->SetUINT32(MF_MT_AUDIO_NUM_CHANNELS, wfe.nChannels);
            if (FAILED(m_writer->AddStream(flac, &m_stream))
                    || FAILED(m_writer->SetInputMediaType(m_stream, pcm, nullptr))
                    || FAILED(m_writer->BeginWriting())) {
                m_ok = false;
                return E_FAIL;
            }
            return S_OK;
        }

        HRESULT DoRenderSample(IMediaSample* pSample) override
        {
            if (!m_ok || !m_writer) {
                return S_OK;
            }
            BYTE* pData = nullptr;
            const long len = pSample->GetActualDataLength();
            if (FAILED(pSample->GetPointer(&pData)) || !pData || len <= 0) {
                return S_OK;
            }
            CComPtr<IMFMediaBuffer> buf;
            if (FAILED(MFCreateMemoryBuffer(len, &buf))) {
                m_ok = false;
                return E_FAIL;
            }
            BYTE* p = nullptr;
            buf->Lock(&p, nullptr, nullptr);
            memcpy(p, pData, len);
            buf->Unlock();
            buf->SetCurrentLength(len);
            CComPtr<IMFSample> s;
            MFCreateSample(&s);
            s->AddBuffer(buf);
            REFERENCE_TIME t0 = 0, t1 = 0;
            if (pSample->GetTime(&t0, &t1) == S_OK) {
                s->SetSampleTime(t0);
                if (t1 > t0) {
                    s->SetSampleDuration(t1 - t0);
                }
            }
            if (FAILED(m_writer->WriteSample(m_stream, s))) {
                m_ok = false;
                return E_FAIL;
            }
            return S_OK;
        }

        // Called on the graph-building thread after the run completes.
        void FinalizeWriter()
        {
            if (m_writer) {
                if (FAILED(m_writer->Finalize())) {
                    m_ok = false;
                }
                m_writer.Release();
            }
        }
    };
}

// Builds and runs the decode graph. Runs on its own thread (see below) so its
// COM apartment is not the caller's UI STA -- a DirectShow graph built there and
// waited on with WaitForCompletion deadlocks, since the wait blocks the very
// thread the graph needs to deliver its completion to.
static bool DecodeGraph(const CString& srcPath, int targetChannels, const CString& outPath,
                        CString* pError)
{
    auto fail = [&](const CString & why) -> bool {
        if (pError) { *pError = why; }
        CASTING_LOG(_T("downmix (LAV): %s"), why.GetString());
        DeleteFile(outPath);
        return false;
    };

    UNREFERENCED_PARAMETER(targetChannels);
    HRESULT hr = S_OK;

    CComPtr<IGraphBuilder> pGraph;
    if (FAILED(pGraph.CoCreateInstance(CLSID_FilterGraph))) {
        return fail(_T("a filter graph could not be created"));
    }
    // No reference clock: a file-to-file transcode should run flat-out, not pace
    // the audio in real time.
    if (CComQIPtr<IMediaFilter> pMediaFilter = pGraph) {
        pMediaFilter->SetSyncSource(nullptr);
    }
    CComQIPtr<IMediaControl> pControl = pGraph;
    CComQIPtr<IMediaEvent> pEvent = pGraph;
    if (!pControl || !pEvent) {
        return fail(_T("the graph is missing its control interfaces"));
    }

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
    // a device that could not output the original layout, which is the point.
    if (CComQIPtr<ILAVAudioSettings> pLav = pAudio) {
        pLav->SetRuntimeConfig(TRUE);
        pLav->SetMixingEnabled(TRUE);
        pLav->SetMixingLayout(0x3); // stereo (FL FR)
        pLav->SetMixingFlags(LAV_MIXING_FLAG_CLIP_PROTECTION | LAV_MIXING_FLAG_NORMALIZE_MATRIX);
        pLav->SetSampleFormat(SampleFormat_16, TRUE);
        pLav->SetOutputStandardLayout(TRUE);
    }

    // The FLAC sink: our own renderer, which compresses the PCM as it arrives.
    HRESULT sinkHr = S_OK;
    CFlacSinkRenderer* pSink = DEBUG_NEW CFlacSinkRenderer(CStringW(outPath), &sinkHr);
    CComPtr<IUnknown> pSinkUnk = (IUnknown*)(INonDelegatingUnknown*)pSink;
    if (FAILED(sinkHr) || !pSinkUnk) {
        return fail(_T("the FLAC sink could not be created"));
    }
    CComQIPtr<IBaseFilter> pSinkBF = pSinkUnk;
    if (!pSinkBF || FAILED(pGraph->AddFilter(pSinkBF, L"FLAC Sink"))) {
        return fail(_T("the FLAC sink could not be added to the graph"));
    }

    // Wire it up: splitter audio -> decoder -> FLAC sink. Direct, so the graph
    // builder cannot substitute another decoder for LAV.
    CComPtr<IPin> pSplitterAudio = AudioOutputPin(pSplitter);
    if (!pSplitterAudio) {
        return fail(_T("the file has no audio track"));
    }
    if (FAILED(hr = pGraph->ConnectDirect(pSplitterAudio, GetFirstPin(pAudio, PINDIR_INPUT), nullptr))) {
        return fail(_T("the audio decoder would not accept the track"));
    }
    if (FAILED(hr = pGraph->ConnectDirect(GetFirstPin(pAudio, PINDIR_OUTPUT),
                                          GetFirstPin(pSinkBF, PINDIR_INPUT), nullptr))) {
        return fail(_T("the FLAC sink would not accept the decoded audio"));
    }

    // Open the FLAC writer now, from the negotiated input format, on this thread.
    if (FAILED(pSink->StartWriter())) {
        return fail(_T("the FLAC writer could not be started"));
    }

    // Run to the end of the file. The source is finite, so this completes.
    if (FAILED(hr = pControl->Run())) {
        return fail(_T("the decode graph would not run"));
    }
    long evCode = 0;
    pEvent->WaitForCompletion(INFINITE, &evCode);
    pControl->Stop();
    pSink->FinalizeWriter();

    if (evCode != EC_COMPLETE || !pSink->Ok()) {
        return fail(_T("the decode did not finish cleanly"));
    }

    CASTING_LOG(_T("downmix (LAV): decoded the audio to a stereo FLAC"));
    return true;
}

bool CastLavDecodeToFlac(const CString& srcPath, int targetChannels, const CString& outFlacPath,
                         CString* pError)
{
    // The graph runs on this worker thread, in its own COM apartment, and the
    // caller blocks on the join. The caller's UI thread is thus free of the graph
    // entirely, so nothing the graph does has to marshal into a thread that is
    // blocked waiting for it.
    bool ok = false;
    CString err;
    std::thread worker([&]() {
        // The multithreaded apartment, not an STA: a headless transcode graph has
        // no message pump, and DirectShow in an STA without one deadlocks.
        const HRESULT hrCo = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
        const bool mf = SUCCEEDED(MFStartup(MF_VERSION)); // the FLAC sink needs it up
        ok = DecodeGraph(srcPath, targetChannels, outFlacPath, &err);
        if (mf) {
            MFShutdown();
        }
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
