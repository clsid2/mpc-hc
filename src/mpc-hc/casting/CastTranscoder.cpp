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
#include <atomic>
#include <vector>
#include <algorithm>

#pragma comment(lib, "mfplat.lib")
#pragma comment(lib, "mfreadwrite.lib")
#pragma comment(lib, "mfuuid.lib")
#pragma comment(lib, "mf.lib") // IMFByteStream/MFCreateFile and the fragmented MP4 sink

// The 8.1 SDK the player builds against does not know this attribute (the
// GUID is from the Windows 10 SDK's mfidl.h); the MPEG-4 sink honours it at
// run time on Windows 10. 100-ns units. Named distinctly so an SDK that does
// declare the real one cannot collide with it.
static const GUID kMpeg4SinkMinFragmentDuration =
{ 0xa30b570c, 0x8efd, 0x45e8, { 0x94, 0xfe, 0x27, 0xc8, 0x4b, 0x5b, 0xdf, 0xf6 } };

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

    // The speaker mask for a channel count; Media Foundation rejects a
    // multichannel PCM type that does not carry one. Stereo needs none.
    // Raw KSAUDIO speaker masks, to avoid pulling in ksmedia.h: FL=0x1 FR=0x2
    // FC=0x4 LFE=0x8 BL=0x10 BR=0x20 SL=0x200 SR=0x400.
    UINT32 ChannelMask(int ch)
    {
        switch (ch) {
            case 1: return 0x4;                   // front centre
            case 2: return 0x1 | 0x2;             // L R
            case 6: return 0x3F;                  // 5.1: FL FR FC LFE BL BR
            case 8: return 0x3F | 0x200 | 0x400;  // 7.1 surround
            default: return 0;
        }
    }

    // Media Foundation's MP4 source does not put the HEVC sequence header on the
    // native video type the way it does for H.264, so the MP4 sink writes an
    // unusable sample entry (the video track comes out undecodable). Recover the
    // VPS/SPS/PPS from the source file's hvcC box, as Annex-B, to hand back as
    // MF_MT_MPEG_SEQUENCE_HEADER. The hvcC lives in the moov, at the file head on
    // a fast-start file or the tail otherwise, so both ends are scanned. (The
    // fragmented sink refuses HEVC outright regardless, which is why HEVC never
    // takes the HLS path -- only this complete-file one.)
    bool ExtractHevcSequenceHeader(const CString& srcPath, std::vector<BYTE>& seq)
    {
        seq.clear();
        CFile file;
        if (!file.Open(srcPath, CFile::modeRead | CFile::shareDenyNone)) {
            return false;
        }
        const ULONGLONG total = file.GetLength();
        auto scan = [&](ULONGLONG at, DWORD want) -> bool {
            std::vector<BYTE> buf(want);
            file.Seek((LONGLONG)at, CFile::begin);
            const UINT got = file.Read(buf.data(), want);
            for (UINT i = 4; i + 8 <= got; i++) {
                if (buf[i] != 'h' || buf[i + 1] != 'v' || buf[i + 2] != 'c' || buf[i + 3] != 'C') {
                    continue;
                }
                const DWORD boxSize = (buf[i - 4] << 24) | (buf[i - 3] << 16) | (buf[i - 2] << 8) | buf[i - 1];
                if (boxSize < 8 + 23 || (ULONGLONG)(i - 4) + boxSize > got) {
                    continue;
                }
                const BYTE* p = &buf[i + 4]; // hvcC payload
                const DWORD payloadLen = boxSize - 8;
                if (p[0] != 1) { // configurationVersion
                    continue;
                }
                DWORD off = 22;
                if (off >= payloadLen) {
                    continue;
                }
                const BYTE numArrays = p[off++];
                std::vector<BYTE> out;
                bool bad = false;
                for (BYTE a = 0; a < numArrays && !bad; a++) {
                    if (off + 3 > payloadLen) { bad = true; break; }
                    const BYTE nalType = p[off] & 0x3f;
                    const WORD numNalus = (WORD)((p[off + 1] << 8) | p[off + 2]);
                    off += 3;
                    for (WORD n = 0; n < numNalus && !bad; n++) {
                        if (off + 2 > payloadLen) { bad = true; break; }
                        const WORD len = (WORD)((p[off] << 8) | p[off + 1]);
                        off += 2;
                        if (off + len > payloadLen) { bad = true; break; }
                        if (nalType == 32 || nalType == 33 || nalType == 34) { // VPS/SPS/PPS
                            static const BYTE startCode[4] = { 0, 0, 0, 1 };
                            out.insert(out.end(), startCode, startCode + 4);
                            out.insert(out.end(), p + off, p + off + len);
                        }
                        off += len;
                    }
                }
                if (!bad && !out.empty()) {
                    seq = std::move(out);
                    return true;
                }
            }
            return false;
        };
        const DWORD window = 16 * 1024 * 1024;
        if (scan(0, (DWORD)std::min<ULONGLONG>(window, total))) {
            return true;
        }
        return total > window && scan(total - window, window);
    }

    // If videoNative is HEVC and carries no sequence header, recover one from the
    // source so the MP4 sink writes a track that actually decodes.
    void EnsureHevcSequenceHeader(IMFMediaType* videoNative, const CString& srcPath)
    {
        if (!videoNative) {
            return;
        }
        GUID subtype = GUID_NULL;
        videoNative->GetGUID(MF_MT_SUBTYPE, &subtype);
        if (subtype != MFVideoFormat_HEVC && subtype != MFVideoFormat_HEVC_ES) {
            return;
        }
        UINT32 have = 0;
        if (SUCCEEDED(videoNative->GetBlobSize(MF_MT_MPEG_SEQUENCE_HEADER, &have)) && have > 0) {
            return;
        }
        std::vector<BYTE> seq;
        if (ExtractHevcSequenceHeader(srcPath, seq)) {
            videoNative->SetBlob(MF_MT_MPEG_SEQUENCE_HEADER, seq.data(), (UINT32)seq.size());
            CASTING_LOG(_T("downmix: recovered the %d-byte HEVC sequence header the source omitted"),
                        (int)seq.size());
        } else {
            CASTING_LOG(_T("downmix: could not recover the HEVC sequence header; the video may not decode"));
        }
    }

    // Builds the MP4 output both engines write into. The non-fragmented variant
    // is the plain sink writer over a file URL. The fragmented one goes through
    // MFCreateFMPEG4MediaSink on a byte stream -- the moov lands up front and
    // samples are cut into moof/mdat fragments as they arrive, which is what
    // lets a segmenter read the file while it is still being written; the sink
    // comes back separately because it must be ShutDown after Finalize or the
    // file stays open. Video is optional for the complete-file path (an
    // audio-only downmix is a legitimate plain MP4) but required for the
    // fragmented one; stream 0 is video, stream 1 audio there.
    bool CreateMp4Writer(const CString& outPath, bool fragmented, IMFMediaType* videoNative,
                         IMFMediaType* pcmActual, UINT32 sampleRate, UINT32 chans,
                         CComPtr<IMFSinkWriter>& writer, CComPtr<IMFMediaSink>& sink,
                         DWORD& outVideo, DWORD& outAudio, CString* pError)
    {
        auto fail = [&](const TCHAR* why) -> bool {
            if (pError) {
                *pError = why;
            }
            return false;
        };

        // Audio out: AAC. The sink writer inserts the AAC encoder to bridge the
        // PCM input to it. ~192 kbps regardless of channel count is plenty for
        // a downmix.
        CComPtr<IMFMediaType> aac;
        MFCreateMediaType(&aac);
        aac->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Audio);
        aac->SetGUID(MF_MT_SUBTYPE, MFAudioFormat_AAC);
        aac->SetUINT32(MF_MT_AUDIO_BITS_PER_SAMPLE, 16);
        aac->SetUINT32(MF_MT_AUDIO_SAMPLES_PER_SECOND, sampleRate);
        aac->SetUINT32(MF_MT_AUDIO_NUM_CHANNELS, chans);
        if (ChannelMask((int)chans)) {
            aac->SetUINT32(MF_MT_AUDIO_CHANNEL_MASK, ChannelMask((int)chans));
        }
        aac->SetUINT32(MF_MT_AUDIO_AVG_BYTES_PER_SECOND, 24000);
        aac->SetUINT32(MF_MT_AAC_PAYLOAD_TYPE, 0);

        if (!fragmented) {
            if (FAILED(MFCreateSinkWriterFromURL(outPath, nullptr, nullptr, &writer))) {
                return fail(_T("the transcoded file could not be created"));
            }
            outVideo = outAudio = (DWORD)-1;
            if (videoNative) {
                if (FAILED(writer->AddStream(videoNative, &outVideo))
                        || FAILED(writer->SetInputMediaType(outVideo, videoNative, nullptr))) {
                    return fail(_T("the video track could not be added to the output"));
                }
            }
            if (FAILED(writer->AddStream(aac, &outAudio))
                    || FAILED(writer->SetInputMediaType(outAudio, pcmActual, nullptr))) {
                return fail(_T("the receiver's audio format is not available on this system"));
            }
        } else {
            if (!videoNative) {
                return fail(_T("the fragmented output needs a video track"));
            }
            CComPtr<IMFByteStream> byteStream;
            if (FAILED(MFCreateFile(MF_ACCESSMODE_READWRITE, MF_OPENMODE_DELETE_IF_EXIST,
                                    MF_FILEFLAGS_NONE, outPath, &byteStream))) {
                return fail(_T("the fragmented output file could not be created"));
            }
            if (FAILED(MFCreateFMPEG4MediaSink(byteStream, videoNative, aac, &sink))) {
                return fail(_T("the fragmented MP4 sink refused the media types (H.264 video only)"));
            }
            // Bigger fragments mean less surgery per byte served; a failed set
            // only leaves the sink at its default cutting.
            if (CComQIPtr<IMFAttributes> sinkAttrs = sink) {
                sinkAttrs->SetUINT64(kMpeg4SinkMinFragmentDuration, 20000000); // 2 s
            }
            CComPtr<IMFAttributes> writerAttrs;
            MFCreateAttributes(&writerAttrs, 1);
            writerAttrs->SetUINT32(MF_LOW_LATENCY, FALSE); // fragments cut whole, not early
            if (FAILED(MFCreateSinkWriterFromMediaSink(sink, writerAttrs, &writer))
                    || FAILED(writer->SetInputMediaType(0, videoNative, nullptr))
                    || FAILED(writer->SetInputMediaType(1, pcmActual, nullptr))) {
                sink->Shutdown();
                sink.Release();
                return fail(_T("the fragmented MP4 sink writer could not be created"));
            }
            outVideo = 0;
            outAudio = 1;
        }

        if (FAILED(writer->BeginWriting())) {
            return fail(_T("the transcode could not be started"));
        }
        return true;
    }
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
// below; the orchestrator routes everything else to LAV. fragmented selects the
// output half (see CreateMp4Writer); prog is polled between batches of samples.
static bool MfDownmixToMp4(const CString& srcPath, int targetChannels, const CString& outPath,
                           bool fragmented, const CastTranscodeProgress& prog, CString* pError)
{
    CComPtr<IMFSinkWriter> writer; // released by fail() before the output is deleted
    CComPtr<IMFMediaSink> sink;    // fragmented mode only

    auto fail = [&](const CString & why) -> bool {
        if (pError) { *pError = why; }
        CASTING_LOG(_T("downmix (MF): %s"), why.GetString());
        writer.Release(); // let go of the output before trying to delete it
        if (sink) {
            sink->Shutdown();
            sink.Release();
        }
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
    if (fragmented && videoIdx == (DWORD)-1) {
        return fail(_T("the file has no video track to fragment"));
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
        EnsureHevcSequenceHeader(videoNative, srcPath);
    }

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
        if (ChannelMask(ch)) {
            pcmReq->SetUINT32(MF_MT_AUDIO_CHANNEL_MASK, ChannelMask(ch));
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

    DWORD outVideo = (DWORD)-1, outAudio = (DWORD)-1;
    {
        CString err;
        if (!CreateMp4Writer(outPath, fragmented, videoNative, pcmActual, sampleRate, chans,
                             writer, sink, outVideo, outAudio, &err)) {
            return fail(err);
        }
    }

    // Pump every sample to the matching output stream. Timestamps ride along on
    // the samples, so the two tracks stay in step. Every so often the caller
    // gets a look in: cancellation first, then whatever it wants to do as the
    // output grows (the HLS segmenter tails the file here).
    DWORD pumped = 0;
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
        if (++pumped % 32 == 0) {
            if (prog.hCancel && WaitForSingleObject(prog.hCancel, 0) == WAIT_OBJECT_0) {
                return fail(_T("cancelled"));
            }
            if (prog.onProgress) {
                prog.onProgress();
            }
        }
    }

    if (FAILED(hr = writer->Finalize())) {
        return fail(_T("the transcoded file could not be finalized"));
    }
    if (sink) {
        sink->Shutdown(); // closes the byte stream so the file is complete on disk
    }

    CASTING_LOG(_T("downmix (MF): wrote %d-channel AAC copy of the file's video%s"),
                (int)chans, fragmented ? _T(", fragmented") : _T(""));
    return true;
}

// Muxes one file's video (copied) with a separate WAV's or FLAC's audio
// (re-encoded to AAC) into an MP4. The LAV engine's second half: LAV writes the
// downmixed FLAC, this puts it back together with the original's untouched
// picture. fragmented and prog as in MfDownmixToMp4.
static bool MfMuxVideoAndWav(const CString& videoSrc, const CString& wavPath, const CString& outPath,
                             bool fragmented, const CastTranscodeProgress& prog, CString* pError)
{
    CComPtr<IMFSinkWriter> writer; // released by fail() before the output is deleted
    CComPtr<IMFMediaSink> sink;    // fragmented mode only

    auto fail = [&](const CString & why) -> bool {
        if (pError) { *pError = why; }
        CASTING_LOG(_T("downmix (mux): %s"), why.GetString());
        writer.Release(); // let go of the output before trying to delete it
        if (sink) {
            sink->Shutdown();
            sink.Release();
        }
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
    EnsureHevcSequenceHeader(videoNative, videoSrc);

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

    DWORD outVideo = (DWORD)-1, outAudio = (DWORD)-1;
    {
        CString err;
        if (!CreateMp4Writer(outPath, fragmented, videoNative, pcmActual, sampleRate, chans,
                             writer, sink, outVideo, outAudio, &err)) {
            return fail(err);
        }
    }

    // Interleave the two sources by timestamp so the MP4 stays in step. As in
    // the single-pass engine, every batch of samples gives the caller its turn:
    // cancel check first, then the progress callback.
    bool vEnd = false, aEnd = false;
    LONGLONG vT = 0, aT = 0;
    CComPtr<IMFSample> vS, aS;
    DWORD pumped = 0;
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
        if (++pumped % 32 == 0) {
            if (prog.hCancel && WaitForSingleObject(prog.hCancel, 0) == WAIT_OBJECT_0) {
                return fail(_T("cancelled"));
            }
            if (prog.onProgress) {
                prog.onProgress();
            }
        }
    }

    if (FAILED(writer->Finalize())) {
        return fail(_T("the output file could not be finalized"));
    }
    if (sink) {
        sink->Shutdown(); // closes the byte stream so the file is complete on disk
    }
    CASTING_LOG(_T("downmix (mux): remuxed the copied video with %d-channel AAC%s"),
                (int)chans, fragmented ? _T(", fragmented") : _T(""));
    return true;
}

// The routing both public entry points share: Media Foundation decodes these
// itself at 5.1 or below, in one pass and with no temp file. Everything else --
// the surround-only codecs, and 7.1, which MF's AAC decoder cannot read at all
// -- goes through LAV to a FLAC that is then muxed back against the copied
// video.
static bool DownmixToMp4Common(const CString& srcPath, const CastMediaInfo& info, int targetChannels,
                               const CString& outPath, bool fragmented,
                               const CastTranscodeProgress& prog, CString* pError)
{
    if (targetChannels < 1) {
        targetChannels = 2;
    }
    const bool mfCanDecode = info.channels <= 6
                             && (info.audio == CastMediaInfo::Audio::AAC
                                 || info.audio == CastMediaInfo::Audio::MP3
                                 || info.audio == CastMediaInfo::Audio::FLAC
                                 || info.audio == CastMediaInfo::Audio::LPCM);
    if (mfCanDecode) {
        return MfDownmixToMp4(srcPath, targetChannels, outPath, fragmented, prog, pError);
    }

    TCHAR tempDir[MAX_PATH] = { 0 };
    GetTempPath(MAX_PATH, tempDir);
    CString flac;
    if (fragmented) {
        // Its own name per run: a quick file switch must not overwrite the temp
        // a still-aborting previous worker is holding. The complete-file path
        // has no such race -- it never overlaps itself.
        static std::atomic<ULONG> hlsGeneration(0);
        flac.Format(_T("%smpc-casthls-%u-%u.flac"), tempDir, GetCurrentProcessId(),
                    ++hlsGeneration);
    } else {
        flac.Format(_T("%smpc-castdownmix-%u.flac"), tempDir, GetCurrentProcessId());
    }
    CASTING_LOG(_T("downmix: %d-channel %s goes through LAV"), info.channels,
                CastAudioCodecName(info.audio));
    if (!CastLavDecodeToFlac(srcPath, targetChannels, flac, pError, prog.hCancel)) {
        return false;
    }
    const bool ok = MfMuxVideoAndWav(srcPath, flac, outPath, fragmented, prog, pError);
    DeleteFile(flac);
    return ok;
}

bool CastDownmixToMp4(const CString& srcPath, const CastMediaInfo& info, int targetChannels,
                      const CString& outPath, CString* pError)
{
    return DownmixToMp4Common(srcPath, info, targetChannels, outPath, false,
                              CastTranscodeProgress(), pError);
}

bool CastDownmixToFragmentedMp4(const CString& srcPath, const CastMediaInfo& info, int targetChannels,
                                const CString& outPath, const CastTranscodeProgress& prog, CString* pError)
{
    // H.264 video only, and there must be video at all: the fragmented MP4 sink
    // refuses HEVC outright (proven in Phase 0), and a file with no picture has
    // nothing to show progressively -- both stay on the complete-file path.
    if (info.video != CastMediaInfo::Video::H264) {
        const CString why = info.video == CastMediaInfo::Video::HEVC
                            ? _T("the fragmented transcode does not take HEVC video")
                            : _T("the fragmented transcode needs an H.264 video track");
        if (pError) {
            *pError = why;
        }
        CASTING_LOG(_T("downmix (fmp4): %s"), why.GetString());
        return false;
    }
    return DownmixToMp4Common(srcPath, info, targetChannels, outPath, true, prog, pError);
}
