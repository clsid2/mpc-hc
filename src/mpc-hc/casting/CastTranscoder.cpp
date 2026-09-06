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
#include "CastTranscoder.h"
#include "CastLavDecoder.h"
#include "Logger.h"
#include <mfapi.h>
#include <mfidl.h>
#include <mfreadwrite.h>
#include <mferror.h>
#include <atlbase.h>

#pragma comment(lib, "mfplat.lib")
#pragma comment(lib, "mfreadwrite.lib")
#pragma comment(lib, "mfuuid.lib")

namespace
{
    CString FileExtLower(const CString& path)
    {
        const int dot = path.ReverseFind(_T('.'));
        if (dot < 0) {
            return CString();
        }
        CString ext = path.Mid(dot);
        ext.MakeLower();
        return ext;
    }

    // Media Foundation has to be up for the whole transcode; RAII so an early
    // return cannot leave it started.
    struct MFScope {
        bool ok = false;
        MFScope() { ok = SUCCEEDED(MFStartup(MF_VERSION)); }
        ~MFScope() { if (ok) { MFShutdown(); } }
    };
}

bool CastCanDownmix(const CString& srcPath, const CastMediaInfo& info)
{
    // Container Media Foundation can demux. Matroska and WebM are the common
    // ones it cannot, and those are exactly what the FFmpeg engine is for.
    const CString ext = FileExtLower(srcPath);
    if (ext != _T(".mp4") && ext != _T(".m4v") && ext != _T(".mov")
            && ext != _T(".m4a") && ext != _T(".3gp") && ext != _T(".3g2")) {
        return false;
    }

    // Audio codec one of the two engines can decode. MF handles the first four
    // at 5.1 or below; the surround-only codecs and 7.1 go through LAV. WMA,
    // Opus and Vorbis are left out: rare in an MP4, and not worth the surface.
    switch (info.audio) {
        case CastMediaInfo::Audio::AAC:
        case CastMediaInfo::Audio::MP3:
        case CastMediaInfo::Audio::FLAC:
        case CastMediaInfo::Audio::LPCM:
        case CastMediaInfo::Audio::AC3:
        case CastMediaInfo::Audio::EAC3:
        case CastMediaInfo::Audio::DTS:
        case CastMediaInfo::Audio::TrueHD:
            break;
        default:
            return false;
    }

    // Video, if there is any, has to copy into MP4 as-is: only H.264 and HEVC
    // do. A file with a video track in anything else is left alone.
    const bool hasVideo = info.video != CastMediaInfo::Video::Unknown || info.width > 0;
    if (hasVideo && info.video != CastMediaInfo::Video::H264 && info.video != CastMediaInfo::Video::HEVC) {
        return false;
    }
    return true;
}

// The Media Foundation single-pass engine: decode + downmix + re-encode + mux in
// one SourceReader -> SinkWriter run. Used for the codecs MF can read at 5.1 or
// below; the orchestrator routes everything else to LAV.
static bool MfDownmixToMp4(const CString& srcPath, int targetChannels, const CString& outPath,
                           CString* pError)
{
    auto fail = [&](const CString & why) -> bool {
        if (pError) { *pError = why; }
        CASTING_LOG(_T("downmix (MF): %s"), why.GetString());
        DeleteFile(outPath); // never leave a half-written file the server might serve
        return false;
    };

    if (targetChannels < 1) {
        targetChannels = 2;
    }

    MFScope mf;
    if (!mf.ok) {
        return fail(_T("Media Foundation could not be started"));
    }

    HRESULT hr;
    CComPtr<IMFAttributes> readerAttrs;
    if (FAILED(hr = MFCreateAttributes(&readerAttrs, 1))) {
        return fail(_T("could not create reader attributes"));
    }
    // We copy the video, so keep the reader from spinning up video processing.
    readerAttrs->SetUINT32(MF_SOURCE_READER_ENABLE_VIDEO_PROCESSING, FALSE);

    CComPtr<IMFSourceReader> reader;
    if (FAILED(hr = MFCreateSourceReaderFromURL(srcPath, readerAttrs, &reader))) {
        return fail(_T("the file could not be opened for transcoding"));
    }

    // Find the first video and audio streams, and hold the video's native
    // (compressed) type to reuse for a straight copy.
    DWORD videoIdx = (DWORD)-1, audioIdx = (DWORD)-1;
    CComPtr<IMFMediaType> videoNative;
    for (DWORD i = 0; ; i++) {
        CComPtr<IMFMediaType> nt;
        hr = reader->GetNativeMediaType(i, 0, &nt);
        if (hr == MF_E_INVALIDSTREAMNUMBER) {
            break;
        }
        if (FAILED(hr)) {
            continue;
        }
        GUID major = GUID_NULL;
        nt->GetGUID(MF_MT_MAJOR_TYPE, &major);
        if (major == MFMediaType_Video && videoIdx == (DWORD)-1) {
            videoIdx = i;
            videoNative = nt;
        } else if (major == MFMediaType_Audio && audioIdx == (DWORD)-1) {
            audioIdx = i;
        }
    }
    if (audioIdx == (DWORD)-1) {
        return fail(_T("the file has no audio track to downmix"));
    }

    reader->SetStreamSelection(MF_SOURCE_READER_ALL_STREAMS, FALSE);
    if (videoIdx != (DWORD)-1) {
        reader->SetStreamSelection(videoIdx, TRUE);
    }
    reader->SetStreamSelection(audioIdx, TRUE);

    // Video: request its own native type back, which keeps the samples encoded.
    if (videoIdx != (DWORD)-1) {
        if (FAILED(hr = reader->SetCurrentMediaType(videoIdx, nullptr, videoNative))) {
            return fail(_T("the video track could not be set up for copying"));
        }
    }

    // The speaker mask for a channel count; Media Foundation rejects a
    // multichannel PCM type that does not carry one. Stereo needs none.
    // Raw KSAUDIO speaker masks, to avoid pulling in ksmedia.h: FL=0x1 FR=0x2
    // FC=0x4 LFE=0x8 BL=0x10 BR=0x20 SL=0x200 SR=0x400.
    auto channelMask = [](int ch) -> UINT32 {
        switch (ch) {
            case 1: return 0x4;                   // front centre
            case 2: return 0x1 | 0x2;             // L R
            case 6: return 0x3F;                  // 5.1: FL FR FC LFE BL BR
            case 8: return 0x3F | 0x200 | 0x400;  // 7.1 surround
            default: return 0;
        }
    };

    // Audio: ask for PCM at the target channel count; the reader inserts the
    // decoder and the resampler, and the resampler applies the standard down-mix
    // matrix (centre and surrounds folded into the kept channels). Not every
    // source and system downmixes to 5.1, so stereo -- which always works -- is
    // the fallback: sound at all beats the exact layout.
    const int wanted[] = { targetChannels, 2 };
    bool audioSet = false;
    for (int attempt = 0; attempt < 2 && !audioSet; attempt++) {
        const int ch = wanted[attempt];
        if (attempt == 1 && ch == wanted[0]) {
            break; // stereo was already the target
        }
        CComPtr<IMFMediaType> pcmReq;
        MFCreateMediaType(&pcmReq);
        pcmReq->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Audio);
        pcmReq->SetGUID(MF_MT_SUBTYPE, MFAudioFormat_PCM);
        pcmReq->SetUINT32(MF_MT_AUDIO_BITS_PER_SAMPLE, 16);
        pcmReq->SetUINT32(MF_MT_AUDIO_NUM_CHANNELS, (UINT32)ch);
        if (channelMask(ch)) {
            pcmReq->SetUINT32(MF_MT_AUDIO_CHANNEL_MASK, channelMask(ch));
        }
        if (SUCCEEDED(reader->SetCurrentMediaType(audioIdx, nullptr, pcmReq))) {
            audioSet = true;
        }
    }
    if (!audioSet) {
        return fail(_T("the audio track could not be downmixed"));
    }

    CComPtr<IMFMediaType> pcmActual;
    if (FAILED(hr = reader->GetCurrentMediaType(audioIdx, &pcmActual))) {
        return fail(_T("the downmixed audio format could not be read back"));
    }
    UINT32 sampleRate = 48000, chans = (UINT32)targetChannels;
    pcmActual->GetUINT32(MF_MT_AUDIO_SAMPLES_PER_SECOND, &sampleRate);
    pcmActual->GetUINT32(MF_MT_AUDIO_NUM_CHANNELS, &chans);

    CComPtr<IMFSinkWriter> writer;
    if (FAILED(hr = MFCreateSinkWriterFromURL(outPath, nullptr, nullptr, &writer))) {
        return fail(_T("the transcoded file could not be created"));
    }

    DWORD outVideo = (DWORD)-1, outAudio = (DWORD)-1;
    if (videoIdx != (DWORD)-1) {
        if (FAILED(writer->AddStream(videoNative, &outVideo))
                || FAILED(writer->SetInputMediaType(outVideo, videoNative, nullptr))) {
            return fail(_T("the video track could not be added to the output"));
        }
    }

    // Audio out: AAC. The sink writer inserts the AAC encoder to bridge the PCM
    // input to it. ~192 kbps regardless of channel count is plenty for a downmix.
    CComPtr<IMFMediaType> aac;
    MFCreateMediaType(&aac);
    aac->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Audio);
    aac->SetGUID(MF_MT_SUBTYPE, MFAudioFormat_AAC);
    aac->SetUINT32(MF_MT_AUDIO_BITS_PER_SAMPLE, 16);
    aac->SetUINT32(MF_MT_AUDIO_SAMPLES_PER_SECOND, sampleRate);
    aac->SetUINT32(MF_MT_AUDIO_NUM_CHANNELS, chans);
    if (channelMask((int)chans)) {
        aac->SetUINT32(MF_MT_AUDIO_CHANNEL_MASK, channelMask((int)chans));
    }
    aac->SetUINT32(MF_MT_AUDIO_AVG_BYTES_PER_SECOND, 24000);
    aac->SetUINT32(MF_MT_AAC_PAYLOAD_TYPE, 0);
    if (FAILED(writer->AddStream(aac, &outAudio))
            || FAILED(hr = writer->SetInputMediaType(outAudio, pcmActual, nullptr))) {
        return fail(_T("the receiver's audio format is not available on this system"));
    }

    if (FAILED(hr = writer->BeginWriting())) {
        return fail(_T("the transcode could not be started"));
    }

    // Pump every sample to the matching output stream. Timestamps ride along on
    // the samples, so the two tracks stay in step.
    for (;;) {
        DWORD streamIndex = 0, flags = 0;
        LONGLONG ts = 0;
        CComPtr<IMFSample> sample;
        hr = reader->ReadSample(MF_SOURCE_READER_ANY_STREAM, 0, &streamIndex, &flags, &ts, &sample);
        if (FAILED(hr)) {
            return fail(_T("a sample could not be read while transcoding"));
        }
        if (flags & MF_SOURCE_READERF_ENDOFSTREAM) {
            break;
        }
        if (!sample) {
            continue;
        }
        if (streamIndex == videoIdx && outVideo != (DWORD)-1) {
            hr = writer->WriteSample(outVideo, sample);
        } else if (streamIndex == audioIdx) {
            hr = writer->WriteSample(outAudio, sample);
        } else {
            hr = S_OK;
        }
        if (FAILED(hr)) {
            return fail(_T("a sample could not be written while transcoding"));
        }
    }

    if (FAILED(hr = writer->Finalize())) {
        return fail(_T("the transcoded file could not be finalized"));
    }

    CASTING_LOG(_T("downmix (MF): wrote %d-channel AAC copy of the file's video"), (int)chans);
    return true;
}

// Muxes one file's video (copied) with a separate WAV's audio (re-encoded to
// AAC) into an MP4. The LAV engine's second half: LAV writes the downmixed WAV,
// this puts it back together with the original's untouched picture.
static bool MfMuxVideoAndWav(const CString& videoSrc, const CString& wavPath, const CString& outPath,
                             CString* pError)
{
    auto fail = [&](const CString & why) -> bool {
        if (pError) { *pError = why; }
        CASTING_LOG(_T("downmix (mux): %s"), why.GetString());
        DeleteFile(outPath);
        return false;
    };

    MFScope mf;
    if (!mf.ok) {
        return fail(_T("Media Foundation could not be started"));
    }
    HRESULT hr;

    CComPtr<IMFAttributes> readerAttrs;
    MFCreateAttributes(&readerAttrs, 1);
    readerAttrs->SetUINT32(MF_SOURCE_READER_ENABLE_VIDEO_PROCESSING, FALSE);
    CComPtr<IMFSourceReader> rv;
    if (FAILED(MFCreateSourceReaderFromURL(videoSrc, readerAttrs, &rv))) {
        return fail(_T("the video source could not be opened"));
    }
    DWORD videoIdx = (DWORD)-1;
    CComPtr<IMFMediaType> videoNative;
    for (DWORD i = 0; ; i++) {
        CComPtr<IMFMediaType> nt;
        hr = rv->GetNativeMediaType(i, 0, &nt);
        if (hr == MF_E_INVALIDSTREAMNUMBER) {
            break;
        }
        if (FAILED(hr)) {
            continue;
        }
        GUID major = GUID_NULL;
        nt->GetGUID(MF_MT_MAJOR_TYPE, &major);
        if (major == MFMediaType_Video) {
            videoIdx = i;
            videoNative = nt;
            break;
        }
    }
    if (videoIdx == (DWORD)-1) {
        return fail(_T("the source has no video to keep"));
    }
    rv->SetStreamSelection(MF_SOURCE_READER_ALL_STREAMS, FALSE);
    rv->SetStreamSelection(videoIdx, TRUE);
    if (FAILED(rv->SetCurrentMediaType(videoIdx, nullptr, videoNative))) {
        return fail(_T("the video could not be set up for copying"));
    }

    CComPtr<IMFSourceReader> ra;
    if (FAILED(MFCreateSourceReaderFromURL(wavPath, nullptr, &ra))) {
        return fail(_T("the decoded audio could not be opened"));
    }
    ra->SetStreamSelection(MF_SOURCE_READER_ALL_STREAMS, FALSE);
    ra->SetStreamSelection(MF_SOURCE_READER_FIRST_AUDIO_STREAM, TRUE);
    // Ask for 16-bit PCM so the AAC encoder gets what it wants regardless of the
    // sample format the decoder chose for the WAV.
    CComPtr<IMFMediaType> pcmReq;
    MFCreateMediaType(&pcmReq);
    pcmReq->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Audio);
    pcmReq->SetGUID(MF_MT_SUBTYPE, MFAudioFormat_PCM);
    pcmReq->SetUINT32(MF_MT_AUDIO_BITS_PER_SAMPLE, 16);
    ra->SetCurrentMediaType(MF_SOURCE_READER_FIRST_AUDIO_STREAM, nullptr, pcmReq);
    CComPtr<IMFMediaType> pcmActual;
    if (FAILED(ra->GetCurrentMediaType(MF_SOURCE_READER_FIRST_AUDIO_STREAM, &pcmActual))) {
        return fail(_T("the decoded audio format could not be read"));
    }
    UINT32 sampleRate = 48000, chans = 2;
    pcmActual->GetUINT32(MF_MT_AUDIO_SAMPLES_PER_SECOND, &sampleRate);
    pcmActual->GetUINT32(MF_MT_AUDIO_NUM_CHANNELS, &chans);

    CComPtr<IMFSinkWriter> writer;
    if (FAILED(MFCreateSinkWriterFromURL(outPath, nullptr, nullptr, &writer))) {
        return fail(_T("the output file could not be created"));
    }
    DWORD outVideo = (DWORD)-1, outAudio = (DWORD)-1;
    if (FAILED(writer->AddStream(videoNative, &outVideo))
            || FAILED(writer->SetInputMediaType(outVideo, videoNative, nullptr))) {
        return fail(_T("the video could not be added to the output"));
    }
    CComPtr<IMFMediaType> aac;
    MFCreateMediaType(&aac);
    aac->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Audio);
    aac->SetGUID(MF_MT_SUBTYPE, MFAudioFormat_AAC);
    aac->SetUINT32(MF_MT_AUDIO_BITS_PER_SAMPLE, 16);
    aac->SetUINT32(MF_MT_AUDIO_SAMPLES_PER_SECOND, sampleRate);
    aac->SetUINT32(MF_MT_AUDIO_NUM_CHANNELS, chans);
    aac->SetUINT32(MF_MT_AUDIO_AVG_BYTES_PER_SECOND, 24000);
    aac->SetUINT32(MF_MT_AAC_PAYLOAD_TYPE, 0);
    if (FAILED(writer->AddStream(aac, &outAudio))
            || FAILED(writer->SetInputMediaType(outAudio, pcmActual, nullptr))) {
        return fail(_T("the receiver's audio format is not available on this system"));
    }
    if (FAILED(writer->BeginWriting())) {
        return fail(_T("the mux could not be started"));
    }

    // Interleave the two sources by timestamp so the MP4 stays in step.
    bool vEnd = false, aEnd = false;
    LONGLONG vT = 0, aT = 0;
    CComPtr<IMFSample> vS, aS;
    for (;;) {
        if (!vEnd && !vS) {
            DWORD si, fl = 0;
            LONGLONG ts = 0;
            if (FAILED(rv->ReadSample(videoIdx, 0, &si, &fl, &ts, &vS))) {
                return fail(_T("a video sample could not be read"));
            }
            if (fl & MF_SOURCE_READERF_ENDOFSTREAM) {
                vEnd = true;
            } else if (vS) {
                vT = ts;
            }
        }
        if (!aEnd && !aS) {
            DWORD si, fl = 0;
            LONGLONG ts = 0;
            if (FAILED(ra->ReadSample(MF_SOURCE_READER_FIRST_AUDIO_STREAM, 0, &si, &fl, &ts, &aS))) {
                return fail(_T("a decoded audio sample could not be read"));
            }
            if (fl & MF_SOURCE_READERF_ENDOFSTREAM) {
                aEnd = true;
            } else if (aS) {
                aT = ts;
            }
        }
        if (!vS && !aS) {
            if (vEnd && aEnd) {
                break;
            }
            // Neither has a sample yet neither has ended: a stream tick or gap.
            // Keep reading rather than spinning tightly.
            if (!vEnd || !aEnd) {
                continue;
            }
            break;
        }
        if (vS && (!aS || vT <= aT)) {
            if (FAILED(writer->WriteSample(outVideo, vS))) {
                return fail(_T("a video sample could not be written"));
            }
            vS.Release();
        } else {
            if (FAILED(writer->WriteSample(outAudio, aS))) {
                return fail(_T("an audio sample could not be written"));
            }
            aS.Release();
        }
    }

    if (FAILED(writer->Finalize())) {
        return fail(_T("the output file could not be finalized"));
    }
    CASTING_LOG(_T("downmix (mux): remuxed the copied video with %d-channel AAC"), (int)chans);
    return true;
}

bool CastDownmixToMp4(const CString& srcPath, const CastMediaInfo& info, int targetChannels,
                      const CString& outPath, CString* pError)
{
    if (targetChannels < 1) {
        targetChannels = 2;
    }
    // Media Foundation decodes these itself at 5.1 or below, in one pass and with
    // no temp file. Everything else -- the surround-only codecs, and 7.1, which
    // MF's AAC decoder cannot read at all -- goes through LAV to a WAV that is
    // then muxed back against the copied video.
    const bool mfCanDecode = info.channels <= 6
                             && (info.audio == CastMediaInfo::Audio::AAC
                                 || info.audio == CastMediaInfo::Audio::MP3
                                 || info.audio == CastMediaInfo::Audio::FLAC
                                 || info.audio == CastMediaInfo::Audio::LPCM);
    if (mfCanDecode) {
        return MfDownmixToMp4(srcPath, targetChannels, outPath, pError);
    }

    TCHAR tempDir[MAX_PATH] = { 0 };
    GetTempPath(MAX_PATH, tempDir);
    CString wav;
    wav.Format(_T("%smpc-castdownmix-%u.wav"), tempDir, GetCurrentProcessId());
    CASTING_LOG(_T("downmix: %d-channel %s goes through LAV"), info.channels,
                CastAudioCodecName(info.audio));
    if (!CastLavDecodeToWav(srcPath, targetChannels, wav, pError)) {
        return false;
    }
    const bool ok = MfMuxVideoAndWav(srcPath, wav, outPath, pError);
    DeleteFile(wav);
    return ok;
}
