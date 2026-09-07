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
#include <dvdmedia.h> // MPEG2VIDEOINFO, the splitter video pin's format block
#include <atomic>
#include <mutex>
#include <thread>
#include <vector>

// The E-AC-3 remux's output side: FFmpeg's encoder and resampler take the
// place the sink-writer remux gives Media Foundation, and the MP4 they land
// in is written by hand below -- a small non-fragmented box writer, so no
// muxer library is linked for its sake. Headers as the rest of the player
// includes them (the thirdparty include paths the vcxproj carries for this).
extern "C" {
#include <ffmpeg/libavcodec/avcodec.h>
#include <ffmpeg/libavutil/channel_layout.h>
#include <ffmpeg/libavutil/mathematics.h>
#include <ffmpeg/libavutil/mem.h>
#include <ffmpeg/libavutil/opt.h>
#include <ffmpeg/libswresample/swresample.h>
}

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
    // The splitter exposes one output pin per stream; this finds the first one
    // offering the wanted major type. Enumerating a pin's offered types is what
    // tells a video pin from an audio one before anything is connected.
    CComPtr<IPin> SplitterOutputPin(IBaseFilter* pFilter, const GUID& majortype)
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
            bool wants = false;
            while (pTypes->Next(1, &pmt, nullptr) == S_OK && pmt) {
                wants = pmt->majortype == majortype;
                DeleteMediaType(pmt);
                pmt = nullptr;
                if (wants) {
                    break;
                }
            }
            if (wants) {
                return pPin;
            }
        }
        return nullptr;
    }

    // What the splitter names its compressed video by: the fourcc sits in the
    // subtype GUID's Data1. AVC1/avc1/H264/h264 are all H.264, HVC1/hvc1/HEVC/
    // hev1 all HEVC; anything else (VP9, AV1...) has no place in an MP4.
    enum class CompressedVideo { None, H264, HEVC };

    CompressedVideo ClassifyCompressedVideo(const GUID& subtype)
    {
        switch (subtype.Data1) {
            case 0x31435641: case 0x31637661: // 'AVC1' 'avc1'
            case 0x34363248: case 0x34363268: // 'H264' 'h264'
                return CompressedVideo::H264;
            case 0x31435648: case 0x31637668: // 'HVC1' 'hvc1'
            case 0x43564548: case 0x31766568: // 'HEVC' 'hev1'
                return CompressedVideo::HEVC;
            default:
                return CompressedVideo::None;
        }
    }

    // The reorder depth the decode-timestamp synthesis below assumes: eight
    // frames covers the x264/x265 defaults with room to spare. The cost is an
    // eight-frame composition offset at the head of the output, which no
    // player minds.
    const int kDtsReorderDepth = 8;

    // ~29.97 fps, for a track that declares no frame rate at all.
    const REFERENCE_TIME kDefaultFrameDuration = 333667;

    // A track's frame duration as the DTS synthesis needs it: the declared
    // average frame time, or a sane guess when the track carries none.
    REFERENCE_TIME FrameDurationFrom(const AM_MEDIA_TYPE& amt)
    {
        if (amt.formattype == FORMAT_MPEG2Video && amt.pbFormat
                && amt.cbFormat >= FIELD_OFFSET(MPEG2VIDEOINFO, dwSequenceHeader)) {
            const REFERENCE_TIME atpf =
                reinterpret_cast<const MPEG2VIDEOINFO*>(amt.pbFormat)->hdr.AvgTimePerFrame;
            if (atpf > 0) {
                return atpf;
            }
        }
        return kDefaultFrameDuration;
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

    // One sink writer with a video and an audio stream, shared by the two
    // renderers below. The splitter hands video and audio to separate
    // streaming threads, so every WriteSample runs under one mutex; the writer
    // itself is created and finalized on the graph-building thread only,
    // between which the renderers hold their own references to it. ok records
    // a failure either streaming thread hits -- which is also what the run
    // loop watches, because a failed sink parks its LAV pin and no event ever
    // comes. stopped is the builder's say-so that the run is over, so a sample
    // arriving late is dropped rather than written to a writer being torn
    // down. timeShift is the video DTS synthesis's head shift, which the audio
    // applies as well so picture and sound keep their relative timing.
    struct RemuxWriter {
        CComPtr<IMFSinkWriter> writer;
        DWORD videoStream = (DWORD)-1;
        DWORD audioStream = (DWORD)-1;
        REFERENCE_TIME timeShift = 0;
        std::mutex writeMutex;
        std::atomic<bool> ok { true };
        std::atomic<bool> stopped { false };
    };

    // While the pin is unconnected, anything acceptable goes; once it is
    // connected, only the connected type is accepted again. A mid-stream
    // proposal (the LAV filters offer one on a resolution change) must not
    // reconfigure the stream under the writer, so the new type has to be
    // byte-identical or it is refused.
    bool MediaTypeUnchanged(CBasePin* pPin, const CMediaType* pmt)
    {
        if (!pPin || !pPin->IsConnected()) {
            return true;
        }
        AM_MEDIA_TYPE cur;
        if (FAILED(pPin->ConnectionMediaType(&cur))) {
            return false;
        }
        const bool identical = cur.majortype == pmt->majortype
                               && cur.subtype == pmt->subtype
                               && cur.formattype == pmt->formattype
                               && cur.cbFormat == pmt->cbFormat
                               && (cur.cbFormat == 0
                                   || memcmp(cur.pbFormat, pmt->pbFormat, cur.cbFormat) == 0);
        FreeMediaType(cur);
        return identical;
    }

    // Turns a length-prefixed (AVCC/HVCC) sample into Annex-B in place: the
    // total size never changes, so each 4-byte big-endian NAL length is simply
    // overwritten with a 00 00 00 01 start code -- the sample form Media
    // Foundation's H.264 and HEVC contract wants. The lengths must account for
    // the sample exactly; anything else means the sample is not in the form
    // its pin declared, and false refuses it rather than let the sink write
    // garbage.
    bool NalLengthsToStartCodes(BYTE* p, size_t len)
    {
        static const BYTE startCode[4] = { 0, 0, 0, 1 };
        size_t off = 0;
        while (off < len) {
            if (off + 4 > len) {
                return false;
            }
            const DWORD nalLen = ((DWORD)p[off] << 24) | ((DWORD)p[off + 1] << 16)
                                 | ((DWORD)p[off + 2] << 8) | (DWORD)p[off + 3];
            if (nalLen == 0 || off + 4 + nalLen > len) {
                return false;
            }
            memcpy(p + off, startCode, 4);
            off += 4 + nalLen;
        }
        return true;
    }

    // The video half of the remux: the compressed samples off the splitter's
    // video pin, rewritten rather than decoded. The pin delivers
    // length-prefixed (AVCC/HVCC) NALUs -- dwFlags in the MPEG2VIDEOINFO is
    // the length size, 4 -- while the MP4 sink consumes Annex-B, so every
    // sample is converted in place first. The samples arrive in decode order
    // carrying presentation times only, so a decode timestamp is synthesized
    // on a frame-rate clock run kDtsReorderDepth frames behind the first
    // presentation time, and every timestamp -- the audio's included, through
    // the shared timeShift -- moves forward by those frames so the first
    // decode time is the first presentation time, never negative.
    class __declspec(uuid("9C5D2A47-3E18-4F0B-8C6D-5A72E1B40D93"))
        CCompressedVideoSink : public CNullRenderer
    {
        RemuxWriter* m_mux;
        CComPtr<IMFSinkWriter> m_writer; // taken once the writer exists, before the run
        DWORD m_stream = (DWORD)-1;
        DWORD m_nalLengthSize = 0; // 0 until a type is accepted; 4 is all LAV offers
        REFERENCE_TIME m_frameDur = kDefaultFrameDuration;
        bool m_sawKeyframe = false;
        bool m_haveFirstPTS = false;
        LONGLONG m_firstPTS = 0;
        LONGLONG m_frameIndex = 0;

    public:
        CCompressedVideoSink(RemuxWriter* mux, HRESULT* phr)
            : CNullRenderer(__uuidof(CCompressedVideoSink), NAME("MPC cast video copy sink"), nullptr, phr)
            , m_mux(mux) {}

        // The writer exists only once both pins are connected and the output
        // type is built; each sink takes its own reference here, held until
        // the graph lets the sink go, so a sample arriving late can never be
        // a write to a released writer.
        void SetWriter(IMFSinkWriter* writer, DWORD stream)
        {
            m_writer = writer;
            m_stream = stream;
        }

        HRESULT CheckMediaType(const CMediaType* pmt) override
        {
            if (pmt->majortype != MEDIATYPE_Video || pmt->formattype != FORMAT_MPEG2Video
                    || !pmt->pbFormat
                    || pmt->cbFormat < FIELD_OFFSET(MPEG2VIDEOINFO, dwSequenceHeader)
                    || ClassifyCompressedVideo(pmt->subtype) == CompressedVideo::None) {
                return VFW_E_TYPE_NOT_ACCEPTED;
            }
            if (!MediaTypeUnchanged(m_pInputPin, pmt)) {
                return VFW_E_TYPE_NOT_ACCEPTED;
            }
            // What the connected type declares about its samples; the render
            // below converts nothing unless this says 4-byte lengths.
            const MPEG2VIDEOINFO& m2 = *reinterpret_cast<const MPEG2VIDEOINFO*>(pmt->pbFormat);
            m_nalLengthSize = m2.dwFlags;
            m_frameDur = FrameDurationFrom(*pmt);
            return S_OK;
        }

        HRESULT DoRenderSample(IMediaSample* pSample) override
        {
            if (!m_mux || m_mux->stopped || !m_mux->ok || !m_writer) {
                return S_OK;
            }
            // Anything cut before the first keyframe cannot be written where a
            // player would start; drop it.
            if (!m_sawKeyframe) {
                if (pSample->IsSyncPoint() != S_OK) {
                    return S_OK;
                }
                m_sawKeyframe = true;
            }

            BYTE* pData = nullptr;
            const long len = pSample->GetActualDataLength();
            if (FAILED(pSample->GetPointer(&pData)) || !pData || len <= 0) {
                return S_OK;
            }
            if (m_nalLengthSize != 4 || !NalLengthsToStartCodes(pData, (size_t)len)) {
                m_mux->ok = false;
                CASTING_LOG(_T("remux (LAV): a video sample was not 4-byte length prefixed"));
                return E_FAIL;
            }

            REFERENCE_TIME t0 = 0, t1 = 0;
            if (pSample->GetTime(&t0, &t1) != S_OK) {
                m_mux->ok = false;
                return E_FAIL;
            }
            if (!m_haveFirstPTS) {
                m_haveFirstPTS = true;
                m_firstPTS = t0;
            }
            // DTS on a frame-rate clock, D frames behind, everything shifted
            // forward by those D frames (see the class comment).
            const LONGLONG dts = m_firstPTS + m_frameIndex * m_frameDur;
            const LONGLONG pts = t0 + (LONGLONG)kDtsReorderDepth * m_frameDur;
            m_frameIndex++;
            if (pts < dts) {
                m_mux->ok = false;
                CASTING_LOG(_T("remux (LAV): a presentation time fell behind the synthesized decode order"));
                return E_FAIL;
            }
            // The splitter stamps one 100 ns tick when a block's duration is
            // unknown; a frame lasts a frame then, not 100 ns.
            REFERENCE_TIME dur = m_frameDur;
            if (t1 > t0 && t1 - t0 >= m_frameDur / 2) {
                dur = t1 - t0;
            }

            CComPtr<IMFMediaBuffer> buf;
            if (FAILED(MFCreateMemoryBuffer(len, &buf))) {
                m_mux->ok = false;
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
            s->SetSampleTime(pts);
            s->SetSampleDuration(dur);
            s->SetUINT64(MFSampleExtension_DecodeTimestamp, dts);
            if (pSample->IsSyncPoint() == S_OK) {
                s->SetUINT32(MFSampleExtension_CleanPoint, 1);
            }
            std::lock_guard<std::mutex> lock(m_mux->writeMutex);
            if (m_mux->stopped || !m_mux->ok) {
                return S_OK;
            }
            if (FAILED(m_writer->WriteSample(m_stream, s))) {
                m_mux->ok = false;
                return E_FAIL;
            }
            return S_OK;
        }
    };

    // The audio half of the remux: shaped after CFlacSinkRenderer above, but
    // the writer is not its own -- the PCM it receives is one stream of the
    // shared writer, which encodes it to AAC as it flows -- so it neither opens
    // nor finalizes anything. Its times move by the same head shift the
    // video's DTS synthesis applied, which is what keeps the two tracks in
    // step in the output.
    class __declspec(uuid("D48B7C21-6F94-4A3E-9B0C-7E15D2F8A462"))
        CPcmToAacSink : public CNullRenderer
    {
        RemuxWriter* m_mux;
        CComPtr<IMFSinkWriter> m_writer; // taken once the writer exists, before the run
        DWORD m_stream = (DWORD)-1;

    public:
        CPcmToAacSink(RemuxWriter* mux, HRESULT* phr)
            : CNullRenderer(__uuidof(CPcmToAacSink), NAME("MPC cast PCM sink"), nullptr, phr)
            , m_mux(mux) {}

        // As in the video sink above: its own reference, taken once the
        // writer exists.
        void SetWriter(IMFSinkWriter* writer, DWORD stream)
        {
            m_writer = writer;
            m_stream = stream;
        }

        HRESULT CheckMediaType(const CMediaType* pmt) override
        {
            if (pmt->majortype != MEDIATYPE_Audio || pmt->subtype != MEDIASUBTYPE_PCM
                    || pmt->formattype != FORMAT_WaveFormatEx) {
                return VFW_E_TYPE_NOT_ACCEPTED;
            }
            if (!MediaTypeUnchanged(m_pInputPin, pmt)) {
                return VFW_E_TYPE_NOT_ACCEPTED;
            }
            return S_OK;
        }

        HRESULT DoRenderSample(IMediaSample* pSample) override
        {
            if (!m_mux || m_mux->stopped || !m_mux->ok || !m_writer) {
                return S_OK;
            }
            BYTE* pData = nullptr;
            const long len = pSample->GetActualDataLength();
            if (FAILED(pSample->GetPointer(&pData)) || !pData || len <= 0) {
                return S_OK;
            }
            CComPtr<IMFMediaBuffer> buf;
            if (FAILED(MFCreateMemoryBuffer(len, &buf))) {
                m_mux->ok = false;
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
                s->SetSampleTime(t0 + m_mux->timeShift);
                if (t1 > t0) {
                    s->SetSampleDuration(t1 - t0);
                }
            }
            std::lock_guard<std::mutex> lock(m_mux->writeMutex);
            if (m_mux->stopped || !m_mux->ok) {
                return S_OK;
            }
            if (FAILED(m_writer->WriteSample(m_stream, s))) {
                m_mux->ok = false;
                return E_FAIL;
            }
            return S_OK;
        }
    };

    // Translates the splitter video pin's MPEG2VIDEOINFO into the Media
    // Foundation type the sink writer takes for a copied track. The same type
    // serves as the stream's output and input, so the writer muxes rather than
    // transforms; the pin's length-prefixed samples reach it as Annex-B, the
    // conversion the video sink does in place. The format block's
    // dwSequenceHeader carries the parameter sets -- H.264 as a run of
    // two-byte-length NALs, HEVC as a whole hvcC record -- which become the
    // Annex-B blob MF_MT_MPEG_SEQUENCE_HEADER wants; without it the sink
    // writes a track that does not decode, so a track we cannot read them
    // from is refused outright.
    bool MakeVideoTypeFromPin(const AM_MEDIA_TYPE& amt, CComPtr<IMFMediaType>& mt, CString* pWhy)
    {
        auto fail = [&](const TCHAR* why) -> bool {
            if (pWhy) {
                *pWhy = why;
            }
            return false;
        };

        if (amt.formattype != FORMAT_MPEG2Video || !amt.pbFormat
                || amt.cbFormat < FIELD_OFFSET(MPEG2VIDEOINFO, dwSequenceHeader)) {
            return fail(_T("the video track's format could not be read"));
        }
        const MPEG2VIDEOINFO& m2 = *reinterpret_cast<const MPEG2VIDEOINFO*>(amt.pbFormat);
        const CompressedVideo kind = ClassifyCompressedVideo(amt.subtype);
        if (kind == CompressedVideo::None) {
            return fail(_T("the video track is not H.264 or HEVC"));
        }
        // Only the length-prefixed sample format is taken. The H264/HEVC
        // fourccs mean Annex-B samples with dwFlags 0 -- a form mkvmerge and
        // ffmpeg never write -- and a wrong dwFlags means an unknown layout;
        // neither is guessed at, the remux is refused.
        if (amt.subtype.Data1 != 0x31435641 /* 'AVC1' */
                && amt.subtype.Data1 != 0x31435648 /* 'HVC1' */) {
            return fail(_T("the video track's samples are not in the length-prefixed form this remux takes"));
        }
        if (m2.dwFlags != 4) {
            return fail(_T("the video track's NAL units are not 4-byte length prefixed"));
        }
        if (amt.cbFormat < FIELD_OFFSET(MPEG2VIDEOINFO, dwSequenceHeader) + m2.cbSequenceHeader) {
            return fail(_T("the video track's format could not be read"));
        }

        LONG w = m2.hdr.bmiHeader.biWidth;
        LONG h = m2.hdr.bmiHeader.biHeight;
        if ((w <= 0 || h <= 0) && m2.hdr.rcSource.right > m2.hdr.rcSource.left
                && m2.hdr.rcSource.bottom > m2.hdr.rcSource.top) {
            w = m2.hdr.rcSource.right - m2.hdr.rcSource.left;
            h = m2.hdr.rcSource.bottom - m2.hdr.rcSource.top;
        }
        if (h < 0) { // a negative height only says top-down; the size is its abs
            h = -h;
        }
        if (w <= 0 || h <= 0) {
            return fail(_T("the video track has no size"));
        }

        std::vector<BYTE> seq; // the parameter sets as Annex-B
        const BYTE* seqHeader = reinterpret_cast<const BYTE*>(m2.dwSequenceHeader); // DWORD[1]
        if (kind == CompressedVideo::H264) {
            static const BYTE startCode[4] = { 0, 0, 0, 1 };
            DWORD off = 0;
            while (off + 2 <= m2.cbSequenceHeader) {
                const WORD nalLen = (WORD)((seqHeader[off] << 8) | seqHeader[off + 1]);
                off += 2;
                if (off + nalLen > m2.cbSequenceHeader) {
                    break;
                }
                seq.insert(seq.end(), startCode, startCode + 4);
                seq.insert(seq.end(), seqHeader + off, seqHeader + off + nalLen);
                off += nalLen;
            }
        } else {
            CastHvcCToAnnexB(seqHeader, m2.cbSequenceHeader, seq);
        }
        if (seq.empty()) {
            return fail(_T("the video track's parameter sets could not be read"));
        }

        CComPtr<IMFMediaType> type;
        if (FAILED(MFCreateMediaType(&type))) {
            return fail(_T("the video track's output type could not be built"));
        }
        type->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Video);
        type->SetGUID(MF_MT_SUBTYPE,
                      kind == CompressedVideo::H264 ? MFVideoFormat_H264 : MFVideoFormat_HEVC);
        MFSetAttributeSize(type, MF_MT_FRAME_SIZE, (UINT32)w, (UINT32)h);
        type->SetUINT32(MF_MT_INTERLACE_MODE, MFVideoInterlace_Progressive);
        MFSetAttributeRatio(type, MF_MT_PIXEL_ASPECT_RATIO, 1, 1);
        if (m2.hdr.AvgTimePerFrame > 0) {
            MFSetAttributeRatio(type, MF_MT_FRAME_RATE, 10000000, (UINT32)m2.hdr.AvgTimePerFrame);
        } else {
            MFSetAttributeRatio(type, MF_MT_FRAME_RATE, 30000, 1000);
        }
        type->SetBlob(MF_MT_MPEG_SEQUENCE_HEADER, seq.data(), (UINT32)seq.size());
        mt = type;
        return true;
    }

    // Reads the PCM the LAV audio decoder settled on off the sink's connected
    // input pin, as the input type the AAC stream takes. rate and chans come
    // back for the AAC output type beside it.
    bool MakePcmTypeFromPin(IPin* pPin, CComPtr<IMFMediaType>& mt, UINT32& rate, UINT32& chans)
    {
        if (!pPin) {
            return false;
        }
        AM_MEDIA_TYPE amt;
        if (FAILED(pPin->ConnectionMediaType(&amt))) {
            return false;
        }
        if (amt.formattype != FORMAT_WaveFormatEx || !amt.pbFormat
                || amt.cbFormat < sizeof(WAVEFORMATEX)) {
            FreeMediaType(amt);
            return false;
        }
        const WAVEFORMATEX wfe = *reinterpret_cast<const WAVEFORMATEX*>(amt.pbFormat);
        FreeMediaType(amt);

        CComPtr<IMFMediaType> type;
        if (FAILED(MFCreateMediaType(&type))) {
            return false;
        }
        type->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Audio);
        type->SetGUID(MF_MT_SUBTYPE, MFAudioFormat_PCM);
        type->SetUINT32(MF_MT_AUDIO_BITS_PER_SAMPLE, wfe.wBitsPerSample);
        type->SetUINT32(MF_MT_AUDIO_SAMPLES_PER_SECOND, wfe.nSamplesPerSec);
        type->SetUINT32(MF_MT_AUDIO_NUM_CHANNELS, wfe.nChannels);
        type->SetUINT32(MF_MT_AUDIO_BLOCK_ALIGNMENT, wfe.nBlockAlign);
        type->SetUINT32(MF_MT_AUDIO_AVG_BYTES_PER_SECOND, wfe.nAvgBytesPerSec);
        mt = type;
        rate = wfe.nSamplesPerSec;
        chans = wfe.nChannels;
        return true;
    }

    // DShow's clock, the time base every pin timestamp is in, as the AVRational
    // the rescaling calls below take.
    const AVRational kDShowTimeBase = { 1, 10000000 };

    // The output's own clocks. An MP4 track carries its timescale beside its
    // timeline; the video track runs on the broadcast-standard 90 kHz (every
    // frame rate lands on it), the audio track on its sample rate, and the
    // movie rounds the longest of them into its 1 ms.
    const uint32_t kMp4VideoTimescale = 90000;
    const uint32_t kMp4MovieTimescale = 1000;

    // Big-endian appends -- the byte order every MP4 box's fields come in.
    void Mp4Put16(std::vector<BYTE>& v, uint32_t x)
    {
        v.push_back(BYTE(x >> 8));
        v.push_back(BYTE(x));
    }

    void Mp4Put24(std::vector<BYTE>& v, uint32_t x)
    {
        v.push_back(BYTE(x >> 16));
        v.push_back(BYTE(x >> 8));
        v.push_back(BYTE(x));
    }

    void Mp4Put32(std::vector<BYTE>& v, uint32_t x)
    {
        Mp4Put16(v, x >> 16);
        Mp4Put16(v, x);
    }

    void Mp4Put64(std::vector<BYTE>& v, uint64_t x)
    {
        Mp4Put32(v, (uint32_t)(x >> 32));
        Mp4Put32(v, (uint32_t)x);
    }

    void Mp4Fourcc(std::vector<BYTE>& v, const char* s)
    {
        v.insert(v.end(), s, s + 4);
    }

    // One sample as the output's tables need it: where its bytes sit in the
    // file, and where in time -- already on its track's timescale, rescaled
    // where the sink delivered it. dts and pts are the clocks the sinks
    // compute (the synthesized one for video, the encoder's for audio); dur
    // is the sample's own duration.
    struct Eac3Mp4Sample {
        uint64_t fileOffset = 0;
        uint32_t size = 0;
        int64_t dts = 0;
        int64_t pts = 0;
        int64_t dur = 0;
        bool keyframe = false;
    };

    // What the video track's sample entry needs, filled once from the
    // connected pin's format. The compressed samples pass through untouched,
    // so this is only a description -- codec, size, and the parameter sets
    // the format block's dwSequenceHeader carries, as the entry's
    // configuration box (an avcC assembled for H.264, a real hvcC taken
    // verbatim for HEVC).
    struct Eac3Mp4Video {
        bool present = false;
        CompressedVideo kind = CompressedVideo::None;
        uint32_t width = 0;
        uint32_t height = 0;
        std::vector<BYTE> config; // the avcC / hvcC record
    };

    // The output side of the E-AC-3 remux: a hand-rolled MP4 writer. The file
    // opens as ftyp then an mdat whose 64-bit size is a placeholder -- the
    // video and audio streaming threads append their samples' bytes to it as
    // they arrive, each recording its offset and timing in its track's table
    // -- and the close patches the mdat's size and appends the moov built
    // from those tables. Both threads write, so every append runs under one
    // mutex -- the same shape as RemuxWriter's writer guard. ok and stopped
    // mean what they mean there. timeShift is the video DTS synthesis's head
    // shift, applied to the audio as well so picture and sound keep their
    // relative timing.
    struct Eac3Mux {
        HANDLE file = INVALID_HANDLE_VALUE; // the MP4 being written
        uint64_t mdatPos = 0;               // the mdat box's offset in the file
        uint64_t dataEnd = 0;               // just past the last sample byte
        std::vector<BYTE> writeBuf;         // sample bytes not yet handed to the OS
        Eac3Mp4Video video;                 // absent when the file has no video
        std::vector<Eac3Mp4Sample> videoSamples;
        std::vector<Eac3Mp4Sample> audioSamples;
        uint32_t audioTimescale = 0;        // the encoder's sample rate
        AVCodecContext* actx = nullptr;     // the E-AC-3 encoder
        SwrContext* swr = nullptr;          // the pin's s16 PCM to the encoder's format
        int srcChannels = 0;            // the decoded PCM's channel count (swr's input;
                                        // differs from the encoder's when 7.1 is folded to 5.1)
        REFERENCE_TIME timeShift = 0;
        std::mutex writeMutex;
        std::atomic<bool> ok { true };
        std::atomic<bool> stopped { false };

        bool IsOpen() const { return file != INVALID_HANDLE_VALUE; }

        // Appends a sample's bytes to the mdat and its entry to its track's
        // table, in one motion under the write mutex so the two tracks'
        // bytes never overlap. Only ever called with writeMutex held.
        bool WriteSample(const void* data, uint32_t size, int64_t dts, int64_t pts,
                         int64_t dur, bool keyframe, std::vector<Eac3Mp4Sample>& track)
        {
            if (!IsOpen()) {
                return false;
            }
            Eac3Mp4Sample s;
            s.fileOffset = dataEnd;
            s.size = size;
            s.dts = dts;
            s.pts = pts;
            s.dur = dur;
            s.keyframe = keyframe;
            track.push_back(s);
            const BYTE* p = static_cast<const BYTE*>(data);
            writeBuf.insert(writeBuf.end(), p, p + size);
            dataEnd += size;
            // Kept small: a receiver tailing the growing file stays fed.
            return writeBuf.size() < 64 * 1024 || Flush();
        }

        // Writes what has accumulated. The OS refusing bytes is a failed
        // remux -- the callers set ok to false.
        bool Flush()
        {
            if (!IsOpen()) {
                return writeBuf.empty();
            }
            size_t wrote = 0;
            while (wrote < writeBuf.size()) {
                DWORD n = 0;
                if (!WriteFile(file, writeBuf.data() + wrote,
                               (DWORD)(writeBuf.size() - wrote), &n, nullptr) || !n) {
                    return false;
                }
                wrote += n;
            }
            writeBuf.clear();
            return true;
        }
    };

    // Tears the output down in the opposite order it was built in, and closes
    // the file first so the DeleteFile that follows a failure can succeed.
    // Called only when the graph is stopped, never racing a streaming thread.
    void CloseEac3Output(Eac3Mux& mux)
    {
        if (mux.file != INVALID_HANDLE_VALUE) {
            CloseHandle(mux.file);
            mux.file = INVALID_HANDLE_VALUE;
        }
        if (mux.swr) {
            swr_free(&mux.swr);
        }
        if (mux.actx) {
            avcodec_free_context(&mux.actx);
        }
    }

    // The video half of the E-AC-3 remux: the compressed samples off the
    // splitter's video pin, forwarded rather than decoded or rewritten. Unlike
    // the sink-writer remux there is no NALU conversion at all -- the length-
    // prefixed samples the pin delivers are exactly what an MP4 holds, with
    // the track's extradata beside them. The decode timestamp synthesis is the
    // sink-writer remux's, unchanged: a frame-rate clock run kDtsReorderDepth
    // frames behind the first presentation time, everything shifted forward
    // by those frames.
    class __declspec(uuid("A7C41E93-6B2D-4E8F-9A45-3D58C7B1E204"))
        CCompressedVideoCopySink : public CNullRenderer
    {
        Eac3Mux* m_mux;
        REFERENCE_TIME m_frameDur = kDefaultFrameDuration;
        bool m_sawKeyframe = false;
        bool m_haveFirstPTS = false;
        LONGLONG m_firstPTS = 0;
        LONGLONG m_frameIndex = 0;

    public:
        CCompressedVideoCopySink(Eac3Mux* mux, HRESULT* phr)
            : CNullRenderer(__uuidof(CCompressedVideoCopySink), NAME("MPC cast video copy sink"), nullptr, phr)
            , m_mux(mux) {}

        HRESULT CheckMediaType(const CMediaType* pmt) override
        {
            if (pmt->majortype != MEDIATYPE_Video || pmt->formattype != FORMAT_MPEG2Video
                    || !pmt->pbFormat
                    || pmt->cbFormat < FIELD_OFFSET(MPEG2VIDEOINFO, dwSequenceHeader)
                    || ClassifyCompressedVideo(pmt->subtype) == CompressedVideo::None) {
                return VFW_E_TYPE_NOT_ACCEPTED;
            }
            if (!MediaTypeUnchanged(m_pInputPin, pmt)) {
                return VFW_E_TYPE_NOT_ACCEPTED;
            }
            m_frameDur = FrameDurationFrom(*pmt);
            return S_OK;
        }

        HRESULT DoRenderSample(IMediaSample* pSample) override
        {
            if (!m_mux || m_mux->stopped || !m_mux->ok || !m_mux->IsOpen()) {
                return S_OK;
            }
            // Anything cut before the first keyframe cannot be written where a
            // player would start; drop it.
            if (!m_sawKeyframe) {
                if (pSample->IsSyncPoint() != S_OK) {
                    return S_OK;
                }
                m_sawKeyframe = true;
            }

            BYTE* pData = nullptr;
            const long len = pSample->GetActualDataLength();
            if (FAILED(pSample->GetPointer(&pData)) || !pData || len <= 0) {
                return S_OK;
            }
            REFERENCE_TIME t0 = 0, t1 = 0;
            if (pSample->GetTime(&t0, &t1) != S_OK) {
                m_mux->ok = false;
                return E_FAIL;
            }
            if (!m_haveFirstPTS) {
                m_haveFirstPTS = true;
                m_firstPTS = t0;
            }
            // DTS on a frame-rate clock, D frames behind, everything shifted
            // forward by those frames (see the sink-writer remux's sink).
            const LONGLONG dts = m_firstPTS + m_frameIndex * m_frameDur;
            const LONGLONG pts = t0 + (LONGLONG)kDtsReorderDepth * m_frameDur;
            m_frameIndex++;
            if (pts < dts) {
                m_mux->ok = false;
                CASTING_LOG(_T("remux E-AC-3 (LAV): a presentation time fell behind the synthesized decode order"));
                return E_FAIL;
            }
            // The splitter stamps one 100 ns tick when a block's duration is
            // unknown; a frame lasts a frame then, not 100 ns.
            REFERENCE_TIME dur = m_frameDur;
            if (t1 > t0 && t1 - t0 >= m_frameDur / 2) {
                dur = t1 - t0;
            }

            // The sample's bytes go to the mdat under the same lock the audio
            // encode takes: one writer, two threads. Its times move onto the
            // video track's timescale first, rescaled as they would have been
            // for a stream's time base.
            const AVRational videoTimeBase = { 1, kMp4VideoTimescale };
            const int64_t sampleDts = av_rescale_q(dts, kDShowTimeBase, videoTimeBase);
            const int64_t samplePts = av_rescale_q(pts, kDShowTimeBase, videoTimeBase);
            const int64_t sampleDur = av_rescale_q(dur, kDShowTimeBase, videoTimeBase);
            std::lock_guard<std::mutex> lock(m_mux->writeMutex);
            if (m_mux->stopped || !m_mux->ok) {
                return S_OK;
            }
            if (!m_mux->WriteSample(pData, (uint32_t)len, sampleDts, samplePts, sampleDur,
                                    pSample->IsSyncPoint() == S_OK, m_mux->videoSamples)) {
                m_mux->ok = false;
                CASTING_LOG(_T("remux E-AC-3 (LAV): the muxer refused a video sample"));
                return E_FAIL;
            }
            return S_OK;
        }
    };

    // The audio half of the E-AC-3 remux, shaped after CPcmToAacSink above.
    // The PCM it receives is not handed to a sink writer but resampled to the
    // E-AC-3 encoder's planar float format and encoded as it flows, in the
    // frame_size blocks the encoder dictates; what arrives short of a block is
    // buffered here until the rest of it comes. Finish pads the tail with
    // silence and drains the encoder once the graph has run to the end. The
    // encoded frames are clocked by sample count from the first sample's
    // timestamp -- decoded PCM is a contiguous run, so the count is the clock
    // -- moved by the video's head shift like the sink-writer remux's audio.
    class __declspec(uuid("5F8D2B64-C31E-47A9-8E0D-92B4A6C7D513"))
        CPcmToEac3Sink : public CNullRenderer
    {
        Eac3Mux* m_mux;
        std::vector<BYTE> m_pending; // whole s16 frames, short of one encoder block
        bool m_haveAnchor = false;
        REFERENCE_TIME m_anchorPts = 0; // 100 ns, the timestamp of m_pending's first sample
        LONGLONG m_anchorSample = 0;    // samples consumed when that anchor was taken
        LONGLONG m_samplesConsumed = 0; // samples pulled off the front so far

    public:
        CPcmToEac3Sink(Eac3Mux* mux, HRESULT* phr)
            : CNullRenderer(__uuidof(CPcmToEac3Sink), NAME("MPC cast E-AC-3 PCM sink"), nullptr, phr)
            , m_mux(mux) {}

        HRESULT CheckMediaType(const CMediaType* pmt) override
        {
            if (pmt->majortype != MEDIATYPE_Audio || pmt->subtype != MEDIASUBTYPE_PCM
                    || pmt->formattype != FORMAT_WaveFormatEx) {
                return VFW_E_TYPE_NOT_ACCEPTED;
            }
            if (!MediaTypeUnchanged(m_pInputPin, pmt)) {
                return VFW_E_TYPE_NOT_ACCEPTED;
            }
            return S_OK;
        }

        HRESULT DoRenderSample(IMediaSample* pSample) override
        {
            if (!m_mux || m_mux->stopped || !m_mux->ok || !m_mux->IsOpen()) {
                return S_OK;
            }
            BYTE* pData = nullptr;
            const long len = pSample->GetActualDataLength();
            if (FAILED(pSample->GetPointer(&pData)) || !pData || len <= 0) {
                return S_OK;
            }
            if (m_pending.empty()) {
                REFERENCE_TIME t0 = 0, t1 = 0;
                m_anchorPts = pSample->GetTime(&t0, &t1) == S_OK ? t0 : 0;
                m_anchorSample = m_samplesConsumed;
                m_haveAnchor = true;
            }
            m_pending.insert(m_pending.end(), pData, pData + len);
            return EncodePending(false) ? S_OK : E_FAIL;
        }

        // Pads what is left with silence, encodes it, and drains the encoder.
        // The graph-building thread calls it after the run ends and the graph
        // is stopped, so nothing races the streaming thread's EncodePending.
        void Finish()
        {
            if (!m_mux || !m_mux->ok || !m_haveAnchor || !m_mux->actx) {
                return;
            }
            EncodePending(true);
        }

    private:
        bool EncodePending(bool flush);
        bool WriteEncodedPackets(); // under writeMutex
    };

    // Encodes whole blocks out of m_pending, oldest first, and leaves the
    // unconsumed remainder in place. flush additionally pads a short tail to a
    // full block and then closes the encoder down, so its last held frames
    // land in the file. Everything that touches the encoder, the resampler or
    // the muxer runs under the mux's mutex, beside the video samples.
    bool CPcmToEac3Sink::EncodePending(bool flush)
    {
        AVCodecContext* actx = m_mux->actx;
        if (!actx || !m_mux->swr || !m_mux->IsOpen()) {
            return false;
        }
        // The buffered PCM is the decoded source's own channel count, which is
        // what swr takes as input; the encoder's channel count may be smaller
        // (7.1 folded to 5.1), so the block stride is the source's, not the
        // encoder's.
        const int srcChannels = m_mux->srcChannels > 0 ? m_mux->srcChannels
                                                       : actx->ch_layout.nb_channels;
        const int frameBytes = actx->frame_size * srcChannels * (int)sizeof(short);
        while (m_pending.size() >= (size_t)frameBytes || (flush && !m_pending.empty())) {
            if (m_pending.size() < (size_t)frameBytes) {
                m_pending.resize(frameBytes, 0); // a tail short of one block rides out on silence
            }
            // The block in hand starts a sample count away from the anchor;
            // anchored on DShow's clock and moved by the video's head shift.
            const LONGLONG pts100 = m_anchorPts + m_mux->timeShift
                                  + (m_samplesConsumed - m_anchorSample) * 10000000
                                        / actx->sample_rate;
            std::lock_guard<std::mutex> lock(m_mux->writeMutex);
            // stopped drops a late streaming-thread sample, but the builder's
            // own flush runs after the stop on purpose: it is not late, it is
            // the tail.
            if ((m_mux->stopped && !flush) || !m_mux->ok) {
                return true;
            }
            AVFrame* frame = av_frame_alloc();
            if (!frame) {
                m_mux->ok = false;
                return false;
            }
            frame->format = actx->sample_fmt;
            frame->nb_samples = actx->frame_size;
            frame->sample_rate = actx->sample_rate;
            av_channel_layout_copy(&frame->ch_layout, &actx->ch_layout);
            frame->pts = av_rescale_q(pts100, kDShowTimeBase,
                                      AVRational{ 1, actx->sample_rate });
            bool okFrame = av_frame_get_buffer(frame, 0) >= 0;
            if (okFrame) {
                const uint8_t* in[1] = { m_pending.data() };
                okFrame = swr_convert(m_mux->swr, frame->data, actx->frame_size,
                                      in, actx->frame_size) == actx->frame_size;
            }
            if (okFrame) {
                okFrame = avcodec_send_frame(actx, frame) >= 0;
            }
            av_frame_free(&frame);
            if (!okFrame) {
                m_mux->ok = false;
                CASTING_LOG(_T("remux E-AC-3 (LAV): a PCM block could not be encoded"));
                return false;
            }
            if (!WriteEncodedPackets()) {
                return false;
            }
            m_pending.erase(m_pending.begin(), m_pending.begin() + frameBytes);
            m_samplesConsumed += actx->frame_size;
        }
        if (flush) {
            std::lock_guard<std::mutex> lock(m_mux->writeMutex);
            if (!m_mux->ok) {
                return true;
            }
            avcodec_send_frame(actx, nullptr); // the encoder gives up what it held
            if (!WriteEncodedPackets()) {
                return false;
            }
        }
        return true;
    }

    // Drains the encoder's ready packets into the output. Only ever called
    // with the mux's mutex held.
    bool CPcmToEac3Sink::WriteEncodedPackets()
    {
        AVPacket* pkt = av_packet_alloc();
        if (!pkt) {
            m_mux->ok = false;
            return false;
        }
        for (;;) {
            const int ret = avcodec_receive_packet(m_mux->actx, pkt);
            if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
                break;
            }
            if (ret < 0) {
                m_mux->ok = false;
                av_packet_free(&pkt);
                CASTING_LOG(_T("remux E-AC-3 (LAV): the encoder failed mid-stream"));
                return false;
            }
            // The packets come off the encoder clocked in samples, which is
            // the audio track's timescale itself, so their times go to the
            // table exactly as they are. Audio never reorders; a packet
            // without a pts (never happens, every frame is sent with one)
            // continues from the last sample's end so the table stays a clock.
            int64_t ts = pkt->pts;
            if (ts == AV_NOPTS_VALUE) {
                const auto& tail = m_mux->audioSamples;
                ts = tail.empty() ? 0 : tail.back().dts + tail.back().dur;
            }
            int64_t dur = pkt->duration;
            if (!dur) {
                dur = m_mux->actx->frame_size;
            }
            if (!m_mux->WriteSample(pkt->data, pkt->size, ts, ts, dur, false,
                                    m_mux->audioSamples)) {
                m_mux->ok = false;
                av_packet_free(&pkt);
                CASTING_LOG(_T("remux E-AC-3 (LAV): the muxer refused an audio packet"));
                return false;
            }
        }
        av_packet_free(&pkt);
        return true;
    }

    // Fills the mux's video description in from the connected pin's format: the
    // compressed samples pass through untouched, so the description is only
    // what the track's sample entry will say -- codec, size, and the parameter
    // sets the format block's dwSequenceHeader carries, taken byte-for-byte
    // as the entry's configuration box. No Annex-B form is built here, unlike
    // the sink-writer remux's type builder: the MP4 takes the samples
    // length-prefixed, the way the pin delivers them, with the blob beside
    // them.
    bool MakeVideoStreamFromPin(const AM_MEDIA_TYPE& amt, Eac3Mux& mux, CString* pWhy)
    {
        auto fail = [&](const TCHAR* why) -> bool {
            if (pWhy) {
                *pWhy = why;
            }
            return false;
        };

        if (amt.formattype != FORMAT_MPEG2Video || !amt.pbFormat
                || amt.cbFormat < FIELD_OFFSET(MPEG2VIDEOINFO, dwSequenceHeader)) {
            return fail(_T("the video track's format could not be read"));
        }
        const MPEG2VIDEOINFO& m2 = *reinterpret_cast<const MPEG2VIDEOINFO*>(amt.pbFormat);
        const CompressedVideo kind = ClassifyCompressedVideo(amt.subtype);
        if (kind == CompressedVideo::None) {
            return fail(_T("the video track is not H.264 or HEVC"));
        }
        // Only the length-prefixed sample form is taken, as in the sink-writer
        // remux: the H264/HEVC fourccs mean Annex-B samples, and a wrong
        // dwFlags means an unknown length size; neither is guessed at.
        if (amt.subtype.Data1 != 0x31435641 /* 'AVC1' */
                && amt.subtype.Data1 != 0x31435648 /* 'HVC1' */) {
            return fail(_T("the video track's samples are not in the length-prefixed form this remux takes"));
        }
        if (m2.dwFlags != 4) {
            return fail(_T("the video track's NAL units are not 4-byte length prefixed"));
        }
        if (amt.cbFormat < FIELD_OFFSET(MPEG2VIDEOINFO, dwSequenceHeader) + m2.cbSequenceHeader) {
            return fail(_T("the video track's format could not be read"));
        }

        LONG w = m2.hdr.bmiHeader.biWidth;
        LONG h = m2.hdr.bmiHeader.biHeight;
        if ((w <= 0 || h <= 0) && m2.hdr.rcSource.right > m2.hdr.rcSource.left
                && m2.hdr.rcSource.bottom > m2.hdr.rcSource.top) {
            w = m2.hdr.rcSource.right - m2.hdr.rcSource.left;
            h = m2.hdr.rcSource.bottom - m2.hdr.rcSource.top;
        }
        if (h < 0) { // a negative height only says top-down; the size is its abs
            h = -h;
        }
        if (w <= 0 || h <= 0) {
            return fail(_T("the video track has no size"));
        }

        // The parameter sets the track is declared with; without them the
        // output track does not decode, so a track that carries none -- or an
        // H.264 run of two-byte lengths that does not parse -- is refused.
        const BYTE* seqHeader = reinterpret_cast<const BYTE*>(m2.dwSequenceHeader); // DWORD[1]
        if (m2.cbSequenceHeader == 0) {
            return fail(_T("the video track's parameter sets could not be read"));
        }
        if (kind == CompressedVideo::H264) {
            DWORD off = 0;
            bool any = false;
            while (off + 2 <= m2.cbSequenceHeader) {
                const WORD nalLen = (WORD)((seqHeader[off] << 8) | seqHeader[off + 1]);
                off += 2;
                if (off + nalLen > m2.cbSequenceHeader) {
                    break;
                }
                off += nalLen;
                any = true;
            }
            if (!any) {
                return fail(_T("the video track's parameter sets could not be read"));
            }
        }

        // The track's configuration record, as the sample entry will carry
        // it. HEVC's is a real hvcC on the pin, taken verbatim. H.264's is
        // not: dwSequenceHeader is a run of two-byte-length SPS/PPS NALs,
        // which no MP4 reader accepts as an avcC (a valid one starts with
        // configurationVersion 1, and one that does not makes a reader treat
        // the length-prefixed packets as Annex-B). So an avcC is assembled
        // from those parameter sets.
        std::vector<BYTE> config;
        if (kind == CompressedVideo::H264) {
            std::vector<std::vector<BYTE>> sps, pps;
            DWORD off = 0;
            while (off + 2 <= m2.cbSequenceHeader) {
                const WORD nalLen = (WORD)((seqHeader[off] << 8) | seqHeader[off + 1]);
                off += 2;
                if (nalLen == 0 || off + nalLen > m2.cbSequenceHeader) {
                    break;
                }
                const BYTE nalType = seqHeader[off] & 0x1F;
                if (nalType == 7) {
                    sps.emplace_back(seqHeader + off, seqHeader + off + nalLen);
                } else if (nalType == 8) {
                    pps.emplace_back(seqHeader + off, seqHeader + off + nalLen);
                }
                off += nalLen;
            }
            if (sps.empty() || pps.empty() || sps[0].size() < 4) {
                return fail(_T("the video track's H.264 parameter sets could not be read"));
            }
            config.push_back(1);            // configurationVersion
            config.push_back(sps[0][1]);    // AVCProfileIndication
            config.push_back(sps[0][2]);    // profile_compatibility
            config.push_back(sps[0][3]);    // AVCLevelIndication
            config.push_back(0xFF);         // reserved(6) | lengthSizeMinusOne=3 (4-byte prefixes)
            config.push_back((BYTE)(0xE0 | (sps.size() & 0x1F))); // reserved(3) | numOfSPS
            for (const auto& s : sps) {
                config.push_back((BYTE)(s.size() >> 8));
                config.push_back((BYTE)(s.size() & 0xFF));
                config.insert(config.end(), s.begin(), s.end());
            }
            config.push_back((BYTE)pps.size()); // numOfPPS
            for (const auto& p : pps) {
                config.push_back((BYTE)(p.size() >> 8));
                config.push_back((BYTE)(p.size() & 0xFF));
                config.insert(config.end(), p.begin(), p.end());
            }
        } else {
            config.assign(seqHeader, seqHeader + m2.cbSequenceHeader);
        }

        mux.video.present = true;
        mux.video.kind = kind;
        mux.video.width = (uint32_t)w;
        mux.video.height = (uint32_t)h;
        mux.video.config = std::move(config);
        return true;
    }

    // Opens the E-AC-3 encoder the audio track encodes with, from the PCM the
    // LAV audio decoder settled on: the source's own channel count and layout
    // -- this engine exists because a surround-capable device is being handed
    // its surround -- with swresample bridging the pin's s16 to the encoder's
    // planar float. The encoder takes up to 5.1, so 6.1/7.1 is downmixed to
    // 5.1 here rather than lost. A layout it still refuses (an exotic one) is a
    // clean failure, logged, so the caller can fall back to the stereo AAC engine.
    bool OpenEac3EncoderFromPin(const AM_MEDIA_TYPE& amt, Eac3Mux& mux, CString* pWhy)
    {
        auto fail = [&](const TCHAR* why) -> bool {
            if (pWhy) {
                *pWhy = why;
            }
            if (mux.swr) {
                swr_free(&mux.swr);
            }
            if (mux.actx) {
                avcodec_free_context(&mux.actx);
            }
            return false;
        };

        if (amt.formattype != FORMAT_WaveFormatEx || !amt.pbFormat
                || amt.cbFormat < sizeof(WAVEFORMATEX)) {
            return fail(_T("the decoded audio format could not be read"));
        }
        const WAVEFORMATEX& wfe = *reinterpret_cast<const WAVEFORMATEX*>(amt.pbFormat);
        if (wfe.wBitsPerSample != 16) {
            return fail(_T("the decoded audio is not the 16-bit PCM this remux takes"));
        }
        const int nch = wfe.nChannels;

        // The pin's own channel order when it declares one (multichannel PCM
        // arrives as WAVE_FORMAT_EXTENSIBLE with a mask), FFmpeg's default for
        // that channel count otherwise. The encoder is opened on the same
        // layout, so the resample is a plain format change.
        AVChannelLayout layout {};
        bool haveLayout = false;
        if (amt.cbFormat >= sizeof(WAVEFORMATEXTENSIBLE)
                && wfe.wFormatTag == WAVE_FORMAT_EXTENSIBLE) {
            const WAVEFORMATEXTENSIBLE& wfex =
                *reinterpret_cast<const WAVEFORMATEXTENSIBLE*>(amt.pbFormat);
            haveLayout = wfex.dwChannelMask
                         && av_channel_layout_from_mask(&layout, wfex.dwChannelMask) == 0
                         && layout.nb_channels == nch;
        }
        if (!haveLayout) {
            av_channel_layout_default(&layout, nch);
        }

        const AVCodec* enc = avcodec_find_encoder(AV_CODEC_ID_EAC3);
        if (!enc) {
            return fail(_T("this build of the player has no E-AC-3 encoder"));
        }
        mux.actx = avcodec_alloc_context3(enc);
        if (!mux.actx) {
            return fail(_T("the E-AC-3 encoder could not be created"));
        }
        mux.actx->sample_rate = wfe.nSamplesPerSec;
        // The encoder's supported sample formats, asked through the API this
        // FFmpeg keeps (7.1 removed the AVCodec::sample_fmts array): E-AC-3
        // takes planar float, and the query should say so. A null list means
        // the encoder takes anything, and a failed ask keeps the default --
        // either way avcodec_open2 below is what catches a wrong one.
        const enum AVSampleFormat* sfmts = nullptr;
        enum AVSampleFormat chosen = AV_SAMPLE_FMT_FLTP;
        if (avcodec_get_supported_config(nullptr, enc, AV_CODEC_CONFIG_SAMPLE_FORMAT, 0,
                                         reinterpret_cast<const void**>(&sfmts), nullptr) == 0
                && sfmts && sfmts[0] != AV_SAMPLE_FMT_NONE) {
            chosen = sfmts[0];
        }
        mux.actx->sample_fmt = chosen;
        // The E-AC-3 encoder takes up to 5.1. A 6.1/7.1 source is downmixed to
        // 5.1 rather than folded all the way to stereo -- a device asked for
        // E-AC-3 surround still gets surround. swresample below builds the
        // downmix matrix from the mismatch between the source layout (its input)
        // and the encoder's 5.1 (its output); six channels or fewer keep the
        // source's own layout untouched.
        const bool downmixToFive1 = nch > 6;
        if (downmixToFive1) {
            AVChannelLayout five1 = AV_CHANNEL_LAYOUT_5POINT1;
            av_channel_layout_copy(&mux.actx->ch_layout, &five1);
        } else {
            av_channel_layout_copy(&mux.actx->ch_layout, &layout);
        }
        mux.actx->bit_rate = 384000;
        if (avcodec_open2(mux.actx, enc, nullptr) < 0) {
            CASTING_LOG(_T("remux E-AC-3 (LAV): the encoder would not open for %d channels at %u Hz"),
                        mux.actx->ch_layout.nb_channels, wfe.nSamplesPerSec);
            return fail(_T("the E-AC-3 encoder would not open for this audio layout"));
        }
        if (downmixToFive1) {
            CASTING_LOG(_T("remux E-AC-3 (LAV): %d-channel audio downmixed to 5.1 for the E-AC-3 encoder"),
                        nch);
        }
        mux.srcChannels = nch; // swr's input width; the encoder's may be smaller (7.1 -> 5.1)

        if (swr_alloc_set_opts2(&mux.swr, &mux.actx->ch_layout, mux.actx->sample_fmt,
                                mux.actx->sample_rate, &layout, AV_SAMPLE_FMT_S16,
                                wfe.nSamplesPerSec, 0, nullptr) < 0
                || swr_init(mux.swr) < 0) {
            return fail(_T("the PCM could not be resampled for the E-AC-3 encoder"));
        }

        mux.audioTimescale = (uint32_t)mux.actx->sample_rate;
        return true;
    }

    // ===== The MP4, written out at close =====
    //
    // ftyp, then the mdat the samples streamed into, then the moov -- the box
    // tree a non-fragmented MP4 is. The receiver reads the file with Range
    // requests, so the moov may sit at the end: nothing needs it before the
    // samples are all on disk.

    // Appends a box: a 32-bit size, the type, the payload. Every box this
    // writer emits is small enough for the 32-bit size; only the mdat, whose
    // end is not known when its header is written, needs the 64-bit form.
    void Mp4Box(std::vector<BYTE>& v, const char* type, const std::vector<BYTE>& payload)
    {
        Mp4Put32(v, (uint32_t)(8 + payload.size()));
        Mp4Fourcc(v, type);
        v.insert(v.end(), payload.begin(), payload.end());
    }

    // The identity transformation matrix every tkhd and mvhd carries.
    void Mp4IdentityMatrix(std::vector<BYTE>& v)
    {
        Mp4Put32(v, 0x00010000);
        Mp4Put32(v, 0);
        Mp4Put32(v, 0);
        Mp4Put32(v, 0);
        Mp4Put32(v, 0x00010000);
        Mp4Put32(v, 0);
        Mp4Put32(v, 0);
        Mp4Put32(v, 0);
        Mp4Put32(v, 0x40000000);
    }

    // MSB-first bit packing, the shape the dec3 box's fields come in.
    struct Mp4Bits {
        std::vector<BYTE>& out;
        uint32_t acc = 0;
        int held = 0;
        explicit Mp4Bits(std::vector<BYTE>& o) : out(o) {}

        void Put(int n, uint32_t value)
        {
            acc = (acc << n) | (value & ((1u << n) - 1));
            held += n;
            while (held >= 8) {
                held -= 8;
                out.push_back(BYTE((acc >> held) & 0xff));
            }
        }

        // Pads the tail with zero bits to the byte boundary.
        void Flush()
        {
            if (held) {
                out.push_back(BYTE((acc << (8 - held)) & 0xff));
                held = 0;
            }
        }
    };

    // A/52's acmod -- the channel configuration the bitstream itself declares,
    // which dec3 repeats: front count and surround count read off the layout's
    // channel positions, mapped to the table's codes. The layout is what the
    // encoder was opened on, so what it is encoding; lfeon comes back beside
    // it, the LFE channel being a flag of its own rather than an acmod bit.
    int Mp4AcmodFromLayout(const AVChannelLayout* layout, bool& lfeon)
    {
        uint64_t mask = 0;
        if (layout->order == AV_CHANNEL_ORDER_NATIVE) {
            mask = layout->u.mask;
        } else if (layout->order == AV_CHANNEL_ORDER_CUSTOM && layout->u.map) {
            for (int i = 0; i < layout->nb_channels; i++) {
                mask |= (uint64_t)1 << av_channel_layout_channel_from_index(layout, i);
            }
        }
        if (!mask) { // an unspecified layout: count is all there is to go on
            return layout->nb_channels == 1 ? 1
                                           : (layout->nb_channels == 2 ? 2 : 7);
        }
        lfeon = (mask & AV_CH_LOW_FREQUENCY) != 0;
        const int fronts = (mask & AV_CH_FRONT_CENTER ? 1 : 0)
                           + (mask & (AV_CH_FRONT_LEFT | AV_CH_FRONT_RIGHT) ? 2 : 0);
        const int surs = (mask & (AV_CH_SIDE_LEFT | AV_CH_SIDE_RIGHT
                                  | AV_CH_BACK_LEFT | AV_CH_BACK_RIGHT) ? 2 : 0)
                         + (mask & AV_CH_BACK_CENTER ? 1 : 0);
        if (fronts >= 3) {
            return surs >= 2 ? 7 : (surs == 1 ? 5 : 3);
        }
        if (fronts == 2) {
            return surs >= 2 ? 6 : (surs == 1 ? 4 : 2);
        }
        return 1; // center only, or nothing the table can name
    }

    // The dec3 box's payload, synthesized from what the encoder was opened
    // with rather than parsed out of the bitstream: the encode is one
    // independent substream with no dependent substreams, so the fields are
    // simply the encoder's own parameters -- data_rate in kbit/s, fscod the
    // sample rate's A/52 code, bsid 16 saying E-AC-3, acmod/lfeon the layout.
    std::vector<BYTE> Mp4Dec3Payload(const AVCodecContext* actx)
    {
        uint32_t fscod = 0;
        switch (actx->sample_rate) {
            case 44100: fscod = 1; break;
            case 32000: fscod = 2; break;
            default: fscod = 0; break; // 48000; the encoder opens at no other rate
        }
        bool lfeon = false;
        const int acmod = Mp4AcmodFromLayout(&actx->ch_layout, lfeon);

        std::vector<BYTE> p;
        Mp4Bits bits(p);
        bits.Put(13, (uint32_t)(actx->bit_rate / 1000)); // data_rate
        bits.Put(3, 0);                                  // num_ind_sub - 1
        bits.Put(2, fscod);                              // substream 0
        bits.Put(5, 16);                                 // bsid
        bits.Put(1, 0);                                  // reserved
        bits.Put(1, 0);                                  // asvc
        bits.Put(3, 0);                                  // bsmod
        bits.Put(3, (uint32_t)acmod);
        bits.Put(1, lfeon ? 1 : 0);
        bits.Put(3, 0);                                  // reserved
        bits.Put(4, 0);                                  // num_dep_sub
        bits.Put(1, 0);                                  // reserved (num_dep_sub == 0)
        bits.Flush();
        return p;
    }

    // The audio sample entry: an ec-3 one, with the synthesized dec3 beside
    // it. channels, sample size and rate are the encoder's; the rate is the
    // field's 16.16 fixed-point form.
    std::vector<BYTE> Mp4Ec3SampleEntry(const AVCodecContext* actx)
    {
        std::vector<BYTE> e;
        Mp4Put32(e, 0);                                  // reserved
        Mp4Put16(e, 0);                                  // reserved
        Mp4Put16(e, 1);                                  // data reference index
        Mp4Put16(e, 0);                                  // entry version
        Mp4Put16(e, 0);                                  // revision level
        Mp4Put32(e, 0);                                  // vendor
        Mp4Put16(e, (uint32_t)actx->ch_layout.nb_channels);
        Mp4Put16(e, 16);                                 // sample size
        Mp4Put16(e, 0);                                  // predefined (compression id)
        Mp4Put16(e, 0);                                  // reserved (packet size)
        Mp4Put32(e, (uint32_t)actx->sample_rate << 16);  // 16.16 fixed
        Mp4Box(e, "dec3", Mp4Dec3Payload(actx));
        std::vector<BYTE> entry;
        Mp4Box(entry, "ec-3", e);
        return entry;
    }

    // The video sample entry: an avc1 or hvc1 one, with the configuration
    // record the description holds riding beside the generic fields as its
    // own box.
    std::vector<BYTE> Mp4VideoSampleEntry(const Eac3Mp4Video& v)
    {
        std::vector<BYTE> e;
        Mp4Put32(e, 0);                              // reserved
        Mp4Put16(e, 0);                              // reserved
        Mp4Put16(e, 1);                              // data reference index
        Mp4Put16(e, 0);                              // pre_defined
        Mp4Put16(e, 0);                              // reserved
        e.insert(e.end(), 12, 0);                    // pre_defined
        Mp4Put16(e, v.width);
        Mp4Put16(e, v.height);
        Mp4Put32(e, 0x00480000);                     // 72 dpi, horizontal
        Mp4Put32(e, 0x00480000);                     // 72 dpi, vertical
        Mp4Put32(e, 0);                              // data size
        Mp4Put16(e, 1);                              // frame count
        e.insert(e.end(), 32, 0);                    // compressor name, empty
        Mp4Put16(e, 0x0018);                         // depth
        Mp4Put16(e, 0xffff);                         // pre_defined
        Mp4Box(e, v.kind == CompressedVideo::H264 ? "avcC" : "hvcC", v.config);
        std::vector<BYTE> entry;
        Mp4Box(entry, v.kind == CompressedVideo::H264 ? "avc1" : "hvc1", e);
        return entry;
    }

    // A track's duration on its own clock: from its first sample's dts to the
    // end of its last sample. Every table below is written in differences,
    // which is what keeps a player's reconstruction of the timeline equal to
    // the one the sinks recorded, whatever head shift was applied to both.
    int64_t Mp4TrackDuration(const std::vector<Eac3Mp4Sample>& samples)
    {
        const Eac3Mp4Sample& first = samples.front();
        const Eac3Mp4Sample& last = samples.back();
        return last.dts + last.dur - first.dts;
    }

    // Builds a trak. samples is the track's table, entry its ready stsd
    // sample entry -- the one part that differs between video and audio;
    // everything else every MP4 track carries is derived here. width/height
    // are zero for audio, pixels for video.
    std::vector<BYTE> Mp4TrakBox(const std::vector<Eac3Mp4Sample>& samples,
                                 const std::vector<BYTE>& entry, bool isVideo,
                                 uint32_t trackId, uint32_t width, uint32_t height,
                                 uint32_t timescale)
    {
        const int64_t trackDur = Mp4TrackDuration(samples);
        const int64_t movieDur = av_rescale_rnd(trackDur, kMp4MovieTimescale, timescale,
                                                AV_ROUND_UP);

        std::vector<BYTE> trakPayload;
        {
            // tkhd: the track's presence, identity, and duration on the
            // movie's clock. Version 1 (64-bit times and durations) only when
            // the 32-bit field would not hold.
            std::vector<BYTE> tkhd;
            const bool big = movieDur > 0x7fffffff;
            tkhd.push_back(BYTE(big ? 1 : 0));
            Mp4Put24(tkhd, 3);                     // enabled | in_movie
            if (big) {
                Mp4Put64(tkhd, 0);                 // creation time
                Mp4Put64(tkhd, 0);                 // modification time
            } else {
                Mp4Put32(tkhd, 0);
                Mp4Put32(tkhd, 0);
            }
            Mp4Put32(tkhd, trackId);
            Mp4Put32(tkhd, 0);                     // reserved
            if (big) {
                Mp4Put64(tkhd, movieDur);
            } else {
                Mp4Put32(tkhd, (uint32_t)movieDur);
            }
            tkhd.insert(tkhd.end(), 8, 0);         // reserved
            Mp4Put16(tkhd, 0);                     // layer
            Mp4Put16(tkhd, 0);                     // alternate group
            Mp4Put16(tkhd, isVideo ? 0 : 0x0100);  // volume: full for audio
            Mp4Put16(tkhd, 0);                     // reserved
            Mp4IdentityMatrix(tkhd);
            Mp4Put32(tkhd, width << 16);           // width, 16.16 fixed
            Mp4Put32(tkhd, height << 16);          // height, 16.16 fixed
            Mp4Box(trakPayload, "tkhd", tkhd);
        }
        {
            std::vector<BYTE> mdia;
            {
                // mdhd: the track's own clock and span. The language is left
                // 'und' -- the graph carries nothing to say otherwise with.
                std::vector<BYTE> mdhd;
                const bool big = trackDur > 0x7fffffff;
                mdhd.push_back(BYTE(big ? 1 : 0));
                Mp4Put24(mdhd, 0);
                if (big) {
                    Mp4Put64(mdhd, 0);             // creation time
                    Mp4Put64(mdhd, 0);             // modification time
                } else {
                    Mp4Put32(mdhd, 0);
                    Mp4Put32(mdhd, 0);
                }
                Mp4Put32(mdhd, timescale);
                if (big) {
                    Mp4Put64(mdhd, trackDur);
                } else {
                    Mp4Put32(mdhd, (uint32_t)trackDur);
                }
                Mp4Put16(mdhd, 0x55c4);            // language 'und'
                Mp4Put16(mdhd, 0);                 // pre_defined
                Mp4Box(mdia, "mdhd", mdhd);

                std::vector<BYTE> hdlr;
                Mp4Put32(hdlr, 0);                 // version & flags
                Mp4Put32(hdlr, 0);                 // pre_defined
                Mp4Fourcc(hdlr, isVideo ? "vide" : "soun");
                hdlr.insert(hdlr.end(), 12, 0);    // reserved
                const char* name = isVideo ? "VideoHandler" : "SoundHandler";
                hdlr.insert(hdlr.end(), name, name + strlen(name));
                hdlr.push_back(0);                 // the c string's terminator
                Mp4Box(mdia, "hdlr", hdlr);
            }
            {
                std::vector<BYTE> minf;
                {
                    // The media's header box: vmhd for video, smhd for audio.
                    std::vector<BYTE> mh;
                    if (isVideo) {
                        mh.push_back(1);           // vmhd, version 1
                        Mp4Put24(mh, 0);
                        mh.insert(mh.end(), 8, 0); // graphicsmode + opcolor
                    } else {
                        Mp4Put32(mh, 0);           // smhd, version 0
                        Mp4Put16(mh, 0);           // balance
                        Mp4Put16(mh, 0);           // reserved
                    }
                    Mp4Box(minf, isVideo ? "vmhd" : "smhd", mh);
                }
                {
                    // dinf/dref: the one data reference, pointing back into
                    // this file -- the mdat the samples already sit in.
                    std::vector<BYTE> dref;
                    Mp4Put32(dref, 0);             // version & flags
                    Mp4Put32(dref, 1);             // one entry
                    Mp4Put32(dref, 12);            // the url box's size
                    Mp4Fourcc(dref, "url ");
                    Mp4Put32(dref, 1);             // self-contained
                    std::vector<BYTE> dinf;
                    Mp4Box(dinf, "dref", dref);
                    Mp4Box(minf, "dinf", dinf);
                }
                {
                    // stbl: the tables a player walks to find and time the
                    // samples. stsd declares the entry the bytes decode with;
                    // stts, stss, ctts, stsc, stsz and stco/co64 locate them.
                    std::vector<BYTE> stbl;
                    {
                        std::vector<BYTE> stsd;
                        Mp4Put32(stsd, 0);         // version & flags
                        Mp4Put32(stsd, 1);         // one entry
                        stsd.insert(stsd.end(), entry.begin(), entry.end());
                        Mp4Box(stbl, "stsd", stsd);
                    }
                    {
                        // stts: the dts steps as run lengths; the last sample
                        // keeps its own duration, closing the track where its
                        // content ends.
                        std::vector<std::pair<uint32_t, int64_t>> runs;
                        for (size_t i = 0; i < samples.size(); i++) {
                            const int64_t d = i + 1 < samples.size()
                                ? samples[i + 1].dts - samples[i].dts
                                : samples[i].dur;
                            if (!runs.empty() && runs.back().second == d) {
                                runs.back().first++;
                            } else {
                                runs.emplace_back(1, d);
                            }
                        }
                        std::vector<BYTE> stts;
                        Mp4Put32(stts, 0);
                        Mp4Put32(stts, (uint32_t)runs.size());
                        for (const auto& r : runs) {
                            Mp4Put32(stts, r.first);
                            Mp4Put32(stts, (uint32_t)r.second);
                        }
                        Mp4Box(stbl, "stts", stts);
                    }
                    if (isVideo) {
                        // stss: the keyframes, as sample numbers. Left out
                        // when every sample is one -- a track nothing else
                        // depends on.
                        size_t keys = 0;
                        for (const auto& s : samples) {
                            keys += s.keyframe;
                        }
                        if (keys > 0 && keys < samples.size()) {
                            std::vector<BYTE> stss;
                            Mp4Put32(stss, 0);
                            Mp4Put32(stss, (uint32_t)keys);
                            for (size_t i = 0; i < samples.size(); i++) {
                                if (samples[i].keyframe) {
                                    Mp4Put32(stss, (uint32_t)i + 1);
                                }
                            }
                            Mp4Box(stbl, "stss", stss);
                        }

                        // ctts: each sample's presentation is this far past
                        // its decode time, run-length compressed. Written
                        // only when the two ever differ, and signed only if
                        // an offset went negative -- which the synthesis
                        // above refuses, so in practice version 0.
                        bool anyOffset = false, negOffset = false;
                        for (const auto& s : samples) {
                            anyOffset = anyOffset || s.pts != s.dts;
                            negOffset = negOffset || s.pts < s.dts;
                        }
                        if (anyOffset) {
                            std::vector<std::pair<uint32_t, int64_t>> runs;
                            for (const auto& s : samples) {
                                const int64_t off = s.pts - s.dts;
                                if (!runs.empty() && runs.back().second == off) {
                                    runs.back().first++;
                                } else {
                                    runs.emplace_back(1, off);
                                }
                            }
                            std::vector<BYTE> ctts;
                            ctts.push_back(BYTE(negOffset ? 1 : 0)); // version 1 = signed
                            Mp4Put24(ctts, 0);
                            Mp4Put32(ctts, (uint32_t)runs.size());
                            for (const auto& r : runs) {
                                Mp4Put32(ctts, r.first);
                                Mp4Put32(ctts, (uint32_t)r.second); // two's complement when signed
                            }
                            Mp4Box(stbl, "ctts", ctts);
                        }
                    }
                    {
                        // stsc/stco: chunks. The two tracks' samples
                        // interleaved as their threads delivered them; every
                        // maximal run of this track's samples contiguous in
                        // the file is one chunk.
                        std::vector<std::pair<uint32_t, uint32_t>> chunks; // first chunk -> samples in it
                        std::vector<uint64_t> offsets;
                        for (size_t i = 0; i < samples.size(); i++) {
                            const bool startsChunk = i == 0
                                || samples[i].fileOffset
                                   != samples[i - 1].fileOffset + samples[i - 1].size;
                            if (startsChunk) {
                                chunks.emplace_back((uint32_t)(offsets.size() + 1), 1);
                                offsets.push_back(samples[i].fileOffset);
                            } else {
                                chunks.back().second++;
                            }
                        }
                        {
                            std::vector<BYTE> stsc;
                            Mp4Put32(stsc, 0);
                            Mp4Put32(stsc, (uint32_t)chunks.size());
                            for (const auto& c : chunks) {
                                Mp4Put32(stsc, c.first);   // first chunk
                                Mp4Put32(stsc, c.second);  // samples per chunk
                                Mp4Put32(stsc, 1);         // sample description index
                            }
                            Mp4Box(stbl, "stsc", stsc);
                        }
                        {
                            bool big = false;
                            for (uint64_t off : offsets) {
                                big = big || off > 0xffffffffu;
                            }
                            std::vector<BYTE> stco;
                            Mp4Put32(stco, 0);
                            Mp4Put32(stco, (uint32_t)offsets.size());
                            for (uint64_t off : offsets) {
                                if (big) {
                                    Mp4Put64(stco, off);
                                } else {
                                    Mp4Put32(stco, (uint32_t)off);
                                }
                            }
                            Mp4Box(stbl, big ? "co64" : "stco", stco);
                        }
                    }
                    {
                        // stsz: per-sample sizes, collapsed into the constant
                        // field when they do not vary (E-AC-3 frames never
                        // do at a fixed bitrate).
                        const uint32_t size0 = samples[0].size;
                        bool constant = true;
                        for (const auto& s : samples) {
                            constant = constant && s.size == size0;
                        }
                        std::vector<BYTE> stsz;
                        Mp4Put32(stsz, 0);
                        Mp4Put32(stsz, constant ? size0 : 0);
                        Mp4Put32(stsz, (uint32_t)samples.size());
                        if (!constant) {
                            for (const auto& s : samples) {
                                Mp4Put32(stsz, s.size);
                            }
                        }
                        Mp4Box(stbl, "stsz", stsz);
                    }
                    Mp4Box(minf, "stbl", stbl);
                }
                Mp4Box(mdia, "minf", minf);
            }
            Mp4Box(trakPayload, "mdia", mdia);
        }
        std::vector<BYTE> trak;
        Mp4Box(trak, "trak", trakPayload);
        return trak;
    }

    // Creates the output file and writes what can exist before any sample
    // does: the ftyp, then the mdat's 16-byte 64-bit header with a zero
    // largesize to be patched at close -- the samples are simply appended as
    // the two threads deliver them. The file is shared-read so the media
    // server can tail it while it grows. The brands say what the moov will
    // hold: the generic ISO set, avc1 when the video track is H.264, dby1 for
    // the Dolby audio.
    bool OpenEac3Output(Eac3Mux& mux, LPCWSTR path)
    {
        mux.file = CreateFileW(path, GENERIC_WRITE, FILE_SHARE_READ, nullptr,
                               CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (mux.file == INVALID_HANDLE_VALUE) {
            return false;
        }
        std::vector<BYTE> head;
        {
            std::vector<BYTE> ftyp;
            Mp4Fourcc(ftyp, "isom");   // major brand
            Mp4Put32(ftyp, 0x200);     // minor version
            Mp4Fourcc(ftyp, "isom");   // compatible brands, the major one first
            Mp4Fourcc(ftyp, "dby1");   // the E-AC-3 track
            Mp4Fourcc(ftyp, "iso2");
            if (mux.video.present && mux.video.kind == CompressedVideo::H264) {
                Mp4Fourcc(ftyp, "avc1");
            }
            Mp4Fourcc(ftyp, "mp41");
            Mp4Box(head, "ftyp", ftyp);
        }
        mux.mdatPos = head.size();     // where the mdat's header starts
        Mp4Put32(head, 1);             // size 1: the box's size is the 64-bit one
        Mp4Fourcc(head, "mdat");
        Mp4Put64(head, 0);             // largesize, patched at close
        mux.dataEnd = head.size();     // just past the mdat's header
        mux.writeBuf.reserve(128 * 1024);
        DWORD wrote = 0;
        while (wrote < head.size()) {
            DWORD n = 0;
            if (!WriteFile(mux.file, head.data() + wrote,
                           (DWORD)(head.size() - wrote), &n, nullptr) || !n) {
                return false;
            }
            wrote += n;
        }
        return true;
    }

    // Closes the output the way a finished MP4 closes: the last sample bytes
    // to disk, the mdat's 64-bit size patched, the moov -- built from the
    // tables the two tracks filled -- appended after it. The builder calls it
    // once the graph has stopped and the encoder has been drained, with
    // nothing racing. A track that collected no samples gets no trak, as a
    // player reading a moov expects.
    bool FinishEac3Output(Eac3Mux& mux)
    {
        if (!mux.IsOpen() || !mux.Flush()) {
            return false;
        }
        if (mux.videoSamples.empty() && mux.audioSamples.empty()) {
            return false; // nothing was ever written; the run has failed elsewhere
        }
        if (!mux.audioSamples.empty() && !mux.actx) {
            return false; // the audio entry is built from the encoder
        }

        std::vector<BYTE> moovPayload;
        int64_t movieDur = 0;
        uint32_t nextTrackId = 1;
        std::vector<BYTE> videoTrak, audioTrak;
        if (!mux.videoSamples.empty()) {
            const int64_t d = av_rescale_rnd(Mp4TrackDuration(mux.videoSamples),
                                             kMp4MovieTimescale, kMp4VideoTimescale,
                                             AV_ROUND_UP);
            if (d > movieDur) {
                movieDur = d;
            }
            videoTrak = Mp4TrakBox(mux.videoSamples, Mp4VideoSampleEntry(mux.video), true,
                                   nextTrackId++, mux.video.width, mux.video.height,
                                   kMp4VideoTimescale);
        }
        if (!mux.audioSamples.empty()) {
            const int64_t d = av_rescale_rnd(Mp4TrackDuration(mux.audioSamples),
                                             kMp4MovieTimescale, mux.audioTimescale,
                                             AV_ROUND_UP);
            if (d > movieDur) {
                movieDur = d;
            }
            audioTrak = Mp4TrakBox(mux.audioSamples, Mp4Ec3SampleEntry(mux.actx), false,
                                   nextTrackId++, 0, 0, mux.audioTimescale);
        }
        {
            // mvhd: the movie's own clock, as long as its longest track.
            std::vector<BYTE> mvhd;
            const bool big = movieDur > 0x7fffffff;
            mvhd.push_back(BYTE(big ? 1 : 0));
            Mp4Put24(mvhd, 0);
            if (big) {
                Mp4Put64(mvhd, 0);     // creation time
                Mp4Put64(mvhd, 0);     // modification time
            } else {
                Mp4Put32(mvhd, 0);
                Mp4Put32(mvhd, 0);
            }
            Mp4Put32(mvhd, kMp4MovieTimescale);
            if (big) {
                Mp4Put64(mvhd, movieDur);
            } else {
                Mp4Put32(mvhd, (uint32_t)movieDur);
            }
            Mp4Put32(mvhd, 0x00010000);    // preferred rate, 1.0
            Mp4Put16(mvhd, 0x0100);        // preferred volume, 1.0
            mvhd.insert(mvhd.end(), 10, 0); // reserved
            Mp4IdentityMatrix(mvhd);
            mvhd.insert(mvhd.end(), 24, 0); // pre_defined
            Mp4Put32(mvhd, nextTrackId);
            Mp4Box(moovPayload, "mvhd", mvhd);
        }
        moovPayload.insert(moovPayload.end(), videoTrak.begin(), videoTrak.end());
        moovPayload.insert(moovPayload.end(), audioTrak.begin(), audioTrak.end());
        std::vector<BYTE> moov;
        Mp4Box(moov, "moov", moovPayload);

        // The mdat's largesize covers its own 16-byte header as well.
        std::vector<BYTE> sizeField;
        Mp4Put64(sizeField, mux.dataEnd - mux.mdatPos);
        LARGE_INTEGER at = {};
        at.QuadPart = (LONGLONG)(mux.mdatPos + 8);
        DWORD n = 0;
        if (!SetFilePointerEx(mux.file, at, nullptr, FILE_BEGIN)
                || !WriteFile(mux.file, sizeField.data(), 8, &n, nullptr) || n != 8) {
            return false;
        }
        at.QuadPart = 0;
        if (!SetFilePointerEx(mux.file, at, nullptr, FILE_END)) {
            return false;
        }
        size_t wrote = 0;
        while (wrote < moov.size()) {
            n = 0;
            if (!WriteFile(mux.file, moov.data() + wrote,
                           (DWORD)(moov.size() - wrote), &n, nullptr) || !n) {
                return false;
            }
            wrote += n;
        }
        return true;
    }
}

// Builds and runs the decode graph. Runs on its own thread (see below) so its
// COM apartment is not the caller's UI STA -- a DirectShow graph built there and
// waited on with WaitForCompletion deadlocks, since the wait blocks the very
// thread the graph needs to deliver its completion to.
static bool DecodeGraph(const CString& srcPath, int targetChannels, const CString& outPath,
                        CString* pError, HANDLE hCancel, bool preserveLayout)
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

    // Configure the audio decoder, isolated from the user's saved settings.
    // Normally it downmixes to stereo -- the LAV engine's floor, which makes the
    // sound audible on a device that could not output the original layout. When
    // the layout is to be preserved (a surround-capable renderer that takes
    // FLAC), mixing is turned off so the source's own channels pass through
    // untouched; SetOutputStandardLayout still puts them in the standard order a
    // multichannel FLAC is expected to carry.
    if (CComQIPtr<ILAVAudioSettings> pLav = pAudio) {
        pLav->SetRuntimeConfig(TRUE);
        if (preserveLayout) {
            pLav->SetMixingEnabled(FALSE);
        } else {
            pLav->SetMixingEnabled(TRUE);
            pLav->SetMixingLayout(0x3); // stereo (FL FR)
            pLav->SetMixingFlags(LAV_MIXING_FLAG_CLIP_PROTECTION | LAV_MIXING_FLAG_NORMALIZE_MATRIX);
        }
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
    CComPtr<IPin> pSplitterAudio = SplitterOutputPin(pSplitter, MEDIATYPE_Audio);
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

    // Run to the end of the file. The source is finite, so this completes --
    // unless the caller cancels, which is why the wait is sliced short rather
    // than INFINITE: a cancel event can only be noticed between the slices.
    if (FAILED(hr = pControl->Run())) {
        return fail(_T("the decode graph would not run"));
    }
    long evCode = 0;
    for (;;) {
        if (hCancel && WaitForSingleObject(hCancel, 0) == WAIT_OBJECT_0) {
            pControl->Stop();
            pSink->FinalizeWriter(); // releases the file so the fail() delete works
            return fail(_T("cancelled"));
        }
        // E_ABORT is the slice elapsing; anything else is an event (or an error)
        const HRESULT waitHr = pEvent->WaitForCompletion(200, &evCode);
        if (waitHr != E_ABORT) {
            break;
        }
    }
    pControl->Stop();
    pSink->FinalizeWriter();

    if (evCode != EC_COMPLETE || !pSink->Ok()) {
        return fail(_T("the decode did not finish cleanly"));
    }

    CASTING_LOG(_T("downmix (LAV): decoded the audio to a stereo FLAC"));
    return true;
}

bool CastLavDecodeToFlac(const CString& srcPath, int targetChannels, const CString& outFlacPath,
                         CString* pError, HANDLE hCancel, bool preserveLayout)
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
        ok = DecodeGraph(srcPath, targetChannels, outFlacPath, &err, hCancel, preserveLayout);
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

// Builds and runs the remux graph. Runs on its own thread (see below) for the
// same apartment reasons as the decode graph: the LAV splitter reads a
// container Media Foundation cannot, the compressed video goes to one sink
// byte-for-byte, and the decoded, downmixed audio goes to the other -- both
// streams of one sink writer, which is opened between the pin connections and
// the run, and finalized once after it.
static bool RemuxGraph(const CString& srcPath, int targetChannels, const CString& outPath,
                       bool fragmented, const CastTranscodeProgress& prog, CString* pError)
{
    // The LAV engine's floor is stereo, as in the decode graph above.
    UNREFERENCED_PARAMETER(targetChannels);

    RemuxWriter mux;
    CComPtr<IMFMediaSink> sink; // fragmented mode only

    auto fail = [&](const CString & why) -> bool {
        if (pError) { *pError = why; }
        CASTING_LOG(_T("remux (LAV): %s"), why.GetString());
        mux.stopped = true; // the sinks write nothing past this point
        mux.writer.Release(); // let go of the output before trying to delete it
        if (sink) {
            sink->Shutdown();
            sink.Release();
        }
        DeleteFile(outPath); // never leave a half-written file the server might serve
        return false;
    };

    HRESULT hr = S_OK;

    CComPtr<IGraphBuilder> pGraph;
    if (FAILED(pGraph.CoCreateInstance(CLSID_FilterGraph))) {
        return fail(_T("a filter graph could not be created"));
    }
    // No reference clock: a file-to-file remux should run flat-out, not pace
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
    // settings, exactly as the decode graph does.
    if (CComQIPtr<ILAVAudioSettings> pLav = pAudio) {
        pLav->SetRuntimeConfig(TRUE);
        pLav->SetMixingEnabled(TRUE);
        pLav->SetMixingLayout(0x3); // stereo (FL FR)
        pLav->SetMixingFlags(LAV_MIXING_FLAG_CLIP_PROTECTION | LAV_MIXING_FLAG_NORMALIZE_MATRIX);
        pLav->SetSampleFormat(SampleFormat_16, TRUE);
        pLav->SetOutputStandardLayout(TRUE);
    }

    CComPtr<IPin> pSplitterVideo = SplitterOutputPin(pSplitter, MEDIATYPE_Video);
    CComPtr<IPin> pSplitterAudio = SplitterOutputPin(pSplitter, MEDIATYPE_Audio);
    if (!pSplitterAudio) {
        return fail(_T("the file has no audio track"));
    }

    // The two sinks. Like the FLAC sink above, they go into the graph through
    // their non-delegating unknown -- casting the object to IBaseFilter directly
    // picks the wrong vtable and the graph hangs.
    CCompressedVideoSink* pVideoSink = nullptr;
    CComPtr<IUnknown> pVideoSinkUnk;
    CComQIPtr<IBaseFilter> pVideoSinkBF;
    if (pSplitterVideo) {
        HRESULT videoSinkHr = S_OK;
        pVideoSink = DEBUG_NEW CCompressedVideoSink(&mux, &videoSinkHr);
        pVideoSinkUnk = (IUnknown*)(INonDelegatingUnknown*)pVideoSink;
        if (FAILED(videoSinkHr) || !pVideoSinkUnk) {
            return fail(_T("the video copy sink could not be created"));
        }
        pVideoSinkBF = pVideoSinkUnk;
        if (!pVideoSinkBF || FAILED(pGraph->AddFilter(pVideoSinkBF, L"Cast Video Copy Sink"))) {
            return fail(_T("the video copy sink could not be added to the graph"));
        }
    } else if (fragmented) {
        // A fragmented output has nothing to show progressively without one.
        return fail(_T("the file has no video track to fragment"));
    }

    HRESULT pcmSinkHr = S_OK;
    CPcmToAacSink* pPcmSink = DEBUG_NEW CPcmToAacSink(&mux, &pcmSinkHr);
    CComPtr<IUnknown> pPcmSinkUnk = (IUnknown*)(INonDelegatingUnknown*)pPcmSink;
    if (FAILED(pcmSinkHr) || !pPcmSinkUnk) {
        return fail(_T("the PCM sink could not be created"));
    }
    CComQIPtr<IBaseFilter> pPcmSinkBF = pPcmSinkUnk;
    if (!pPcmSinkBF || FAILED(pGraph->AddFilter(pPcmSinkBF, L"Cast PCM Sink"))) {
        return fail(_T("the PCM sink could not be added to the graph"));
    }

    // Wire it up: splitter video -> copy sink, splitter audio -> decoder ->
    // PCM sink. Direct, so the graph builder can neither insert a video
    // decoder -- the point is that the compressed samples are never touched --
    // nor substitute another audio decoder for LAV.
    if (pSplitterVideo) {
        if (FAILED(hr = pGraph->ConnectDirect(pSplitterVideo,
                                              GetFirstPin(pVideoSinkBF, PINDIR_INPUT), nullptr))) {
            return fail(_T("the video track would not connect for copying"));
        }
    }
    if (FAILED(hr = pGraph->ConnectDirect(pSplitterAudio, GetFirstPin(pAudio, PINDIR_INPUT), nullptr))) {
        return fail(_T("the audio decoder would not accept the track"));
    }
    if (FAILED(hr = pGraph->ConnectDirect(GetFirstPin(pAudio, PINDIR_OUTPUT),
                                          GetFirstPin(pPcmSinkBF, PINDIR_INPUT), nullptr))) {
        return fail(_T("the PCM sink would not accept the decoded audio"));
    }

    // Open the shared writer now, from the types the pins actually settled on
    // -- which only exist after the connects -- on this thread and before the
    // graph runs: video first, audio second.
    CComPtr<IMFMediaType> videoType;
    if (pSplitterVideo) {
        CComPtr<IPin> pVideoIn = GetFirstPin(pVideoSinkBF, PINDIR_INPUT);
        AM_MEDIA_TYPE amt;
        if (!pVideoIn || FAILED(pVideoIn->ConnectionMediaType(&amt))) {
            return fail(_T("the video track's format could not be read"));
        }
        CString why;
        const bool okType = MakeVideoTypeFromPin(amt, videoType, &why);
        const REFERENCE_TIME frameDur = FrameDurationFrom(amt);
        FreeMediaType(amt);
        if (!okType) {
            return fail(why);
        }
        // The head shift the video's DTS synthesis applies; the audio moves by
        // it too (RemuxWriter::timeShift) so the tracks stay in step.
        mux.timeShift = (LONGLONG)kDtsReorderDepth * frameDur;
    }
    CComPtr<IMFMediaType> pcmType;
    UINT32 sampleRate = 48000, chans = 2;
    if (!MakePcmTypeFromPin(GetFirstPin(pPcmSinkBF, PINDIR_INPUT), pcmType, sampleRate, chans)) {
        return fail(_T("the decoded audio format could not be read"));
    }
    {
        CString err;
        if (!CastCreateMp4Writer(outPath, fragmented, videoType, pcmType, sampleRate, chans,
                                 &mux.writer, &sink, mux.videoStream, mux.audioStream, &err)) {
            return fail(err);
        }
    }
    // The sinks take their own references now that the writer exists; they
    // hold them until the graph lets the sinks go, so the writer outlives the
    // builder's own reference to it.
    if (pVideoSink) {
        pVideoSink->SetWriter(mux.writer, mux.videoStream);
    }
    pPcmSink->SetWriter(mux.writer, mux.audioStream);

    // Run to the end of the file, sliced short so a cancel event can be
    // noticed between the slices; the caller's progress callback gets its turn
    // on every slice too, which is the HLS segmenter's chance to tail the
    // output as it grows.
    if (FAILED(hr = pControl->Run())) {
        return fail(_T("the remux graph would not run"));
    }
    long evCode = 0;
    for (;;) {
        if (prog.hCancel && WaitForSingleObject(prog.hCancel, 0) == WAIT_OBJECT_0) {
            pControl->Stop();
            return fail(_T("cancelled"));
        }
        if (prog.onProgress) {
            prog.onProgress();
        }
        // E_ABORT is the slice elapsing; anything else is an event (or an error)
        const HRESULT waitHr = pEvent->WaitForCompletion(200, &evCode);
        if (waitHr != E_ABORT) {
            break;
        }
        // A sink that failed returns E_FAIL, which parks its LAV pin: that pin
        // never delivers its end of stream, so EC_COMPLETE would never fire
        // and this loop would spin until cancelled. Fail now instead.
        if (!mux.ok) {
            evCode = 0;
            break;
        }
    }
    pControl->Stop();
    mux.stopped = true; // whatever arrives now is dropped, not written
    if (!mux.ok) {
        return fail(_T("the remux failed while writing a sample"));
    }
    if (evCode != EC_COMPLETE) {
        return fail(_T("the remux did not finish cleanly"));
    }
    if (FAILED(mux.writer->Finalize())) {
        return fail(_T("the output file could not be finalized"));
    }
    if (sink) {
        sink->Shutdown(); // closes the byte stream so the file is complete on disk
    }

    CASTING_LOG(_T("remux (LAV): copied the video and wrote %d-channel AAC"), (int)chans);
    return true;
}

bool CastLavRemuxToMp4(const CString& srcPath, const CastMediaInfo& info, int targetChannels,
                       const CString& outPath, bool fragmented, const CastTranscodeProgress& prog,
                       CString* pError)
{
    // The same worker-thread wrapper as CastLavDecodeToFlac: the graph runs in
    // its own multithreaded apartment and the caller blocks on the join, so
    // nothing the graph does has to marshal into a thread waiting for it.
    CASTING_LOG(_T("remux (LAV): %s"), CastDescribeMedia(info).GetString());
    bool ok = false;
    CString err;
    std::thread worker([&]() {
        const HRESULT hrCo = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
        const bool mf = SUCCEEDED(MFStartup(MF_VERSION)); // the shared sink writer needs it up
        ok = RemuxGraph(srcPath, targetChannels, outPath, fragmented, prog, &err);
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

// Builds and runs the E-AC-3 remux graph: the LAV graph machinery of the
// sink-writer remux above, verbatim, with the E-AC-3 engine on the output
// side in place of the shared sink writer -- a hand-rolled MP4 writer whose
// video track takes the compressed samples as the pin delivers them and
// whose audio track is the E-AC-3 encoding of the decoded PCM, the source's
// own layout kept. Runs on its own thread (see below) for the same apartment
// reasons as the others.
static bool Eac3RemuxGraph(const CString& srcPath, int targetChannels, const CString& outPath,
                           const CastTranscodeProgress& prog, CString* pError)
{
    // Surround is the point of this engine; targetChannels is what routed the
    // call here and has nothing to do inside the graph.
    UNREFERENCED_PARAMETER(targetChannels);

    Eac3Mux mux;

    auto fail = [&](const CString & why) -> bool {
        if (pError) { *pError = why; }
        CASTING_LOG(_T("remux E-AC-3 (LAV): %s"), why.GetString());
        mux.stopped = true;      // the sinks write nothing past this point
        CloseEac3Output(mux);    // closes the file so the delete below works
        DeleteFile(outPath);     // never leave a half-written file the server might serve
        return false;
    };

    HRESULT hr = S_OK;

    CComPtr<IGraphBuilder> pGraph;
    if (FAILED(pGraph.CoCreateInstance(CLSID_FilterGraph))) {
        return fail(_T("a filter graph could not be created"));
    }
    // No reference clock: a file-to-file remux should run flat-out, not pace
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

    // Keep the source's own channels: this engine exists because a device that
    // plays E-AC-3 wants its surround handed over as surround. The LAV decoder
    // is configured exactly as the preserve-layout FLAC decode is -- mixing
    // off, 16-bit samples, the channels in the standard order.
    if (CComQIPtr<ILAVAudioSettings> pLav = pAudio) {
        pLav->SetRuntimeConfig(TRUE);
        pLav->SetMixingEnabled(FALSE);
        pLav->SetSampleFormat(SampleFormat_16, TRUE);
        pLav->SetOutputStandardLayout(TRUE);
    }

    CComPtr<IPin> pSplitterVideo = SplitterOutputPin(pSplitter, MEDIATYPE_Video);
    CComPtr<IPin> pSplitterAudio = SplitterOutputPin(pSplitter, MEDIATYPE_Audio);
    if (!pSplitterAudio) {
        return fail(_T("the file has no audio track"));
    }

    // The two sinks. Like the FLAC sink above, they go into the graph through
    // their non-delegating unknown -- casting the object to IBaseFilter directly
    // picks the wrong vtable and the graph hangs.
    CCompressedVideoCopySink* pVideoSink = nullptr;
    CComPtr<IUnknown> pVideoSinkUnk;
    CComQIPtr<IBaseFilter> pVideoSinkBF;
    if (pSplitterVideo) {
        HRESULT videoSinkHr = S_OK;
        pVideoSink = DEBUG_NEW CCompressedVideoCopySink(&mux, &videoSinkHr);
        pVideoSinkUnk = (IUnknown*)(INonDelegatingUnknown*)pVideoSink;
        if (FAILED(videoSinkHr) || !pVideoSinkUnk) {
            return fail(_T("the video copy sink could not be created"));
        }
        pVideoSinkBF = pVideoSinkUnk;
        if (!pVideoSinkBF || FAILED(pGraph->AddFilter(pVideoSinkBF, L"Cast Video Copy Sink"))) {
            return fail(_T("the video copy sink could not be added to the graph"));
        }
    }

    HRESULT pcmSinkHr = S_OK;
    CPcmToEac3Sink* pPcmSink = DEBUG_NEW CPcmToEac3Sink(&mux, &pcmSinkHr);
    CComPtr<IUnknown> pPcmSinkUnk = (IUnknown*)(INonDelegatingUnknown*)pPcmSink;
    if (FAILED(pcmSinkHr) || !pPcmSinkUnk) {
        return fail(_T("the PCM sink could not be created"));
    }
    CComQIPtr<IBaseFilter> pPcmSinkBF = pPcmSinkUnk;
    if (!pPcmSinkBF || FAILED(pGraph->AddFilter(pPcmSinkBF, L"Cast E-AC-3 PCM Sink"))) {
        return fail(_T("the PCM sink could not be added to the graph"));
    }

    // Wire it up: splitter video -> copy sink, splitter audio -> decoder ->
    // PCM sink. Direct, so the graph builder can neither insert a video
    // decoder -- the point is that the compressed samples are never touched --
    // nor substitute another audio decoder for LAV.
    if (pSplitterVideo) {
        if (FAILED(hr = pGraph->ConnectDirect(pSplitterVideo,
                                              GetFirstPin(pVideoSinkBF, PINDIR_INPUT), nullptr))) {
            return fail(_T("the video track would not connect for copying"));
        }
    }
    if (FAILED(hr = pGraph->ConnectDirect(pSplitterAudio, GetFirstPin(pAudio, PINDIR_INPUT), nullptr))) {
        return fail(_T("the audio decoder would not accept the track"));
    }
    if (FAILED(hr = pGraph->ConnectDirect(GetFirstPin(pAudio, PINDIR_OUTPUT),
                                          GetFirstPin(pPcmSinkBF, PINDIR_INPUT), nullptr))) {
        return fail(_T("the PCM sink would not accept the decoded audio"));
    }

    // Open the output now, from the types the pins actually settled on --
    // which only exist after the connects -- on this thread and before the
    // graph runs: the video track's description first, then the E-AC-3
    // encoder, then the file itself, ftyp and the mdat's header.
    if (pSplitterVideo) {
        CComPtr<IPin> pVideoIn = GetFirstPin(pVideoSinkBF, PINDIR_INPUT);
        AM_MEDIA_TYPE amt;
        if (!pVideoIn || FAILED(pVideoIn->ConnectionMediaType(&amt))) {
            return fail(_T("the video track's format could not be read"));
        }
        CString why;
        const bool okVideo = MakeVideoStreamFromPin(amt, mux, &why);
        const REFERENCE_TIME frameDur = FrameDurationFrom(amt);
        FreeMediaType(amt);
        if (!okVideo) {
            return fail(why);
        }
        // The head shift the video's DTS synthesis applies; the audio moves by
        // it too (Eac3Mux::timeShift) so the tracks stay in step.
        mux.timeShift = (LONGLONG)kDtsReorderDepth * frameDur;
    }
    {
        CComPtr<IPin> pPcmIn = GetFirstPin(pPcmSinkBF, PINDIR_INPUT);
        AM_MEDIA_TYPE amt;
        if (!pPcmIn || FAILED(pPcmIn->ConnectionMediaType(&amt))) {
            return fail(_T("the decoded audio format could not be read"));
        }
        CString why;
        const bool okEnc = OpenEac3EncoderFromPin(amt, mux, &why);
        FreeMediaType(amt);
        if (!okEnc) {
            return fail(why);
        }
    }
    if (!OpenEac3Output(mux, CStringW(outPath))) {
        return fail(_T("the output file could not be created"));
    }

    // Run to the end of the file, sliced short so a cancel event can be
    // noticed between the slices; the caller's progress callback gets its
    // turn on every slice too.
    if (FAILED(hr = pControl->Run())) {
        return fail(_T("the remux graph would not run"));
    }
    long evCode = 0;
    for (;;) {
        if (prog.hCancel && WaitForSingleObject(prog.hCancel, 0) == WAIT_OBJECT_0) {
            pControl->Stop();
            return fail(_T("cancelled"));
        }
        if (prog.onProgress) {
            prog.onProgress();
        }
        // E_ABORT is the slice elapsing; anything else is an event (or an error)
        const HRESULT waitHr = pEvent->WaitForCompletion(200, &evCode);
        if (waitHr != E_ABORT) {
            break;
        }
        // A sink that failed returns E_FAIL, which parks its LAV pin: that pin
        // never delivers its end of stream, so EC_COMPLETE would never fire
        // and this loop would spin until cancelled. Fail now instead.
        if (!mux.ok) {
            evCode = 0;
            break;
        }
    }
    pControl->Stop();
    mux.stopped = true; // whatever arrives now is dropped, not written
    if (!mux.ok) {
        return fail(_T("the remux failed while writing a sample"));
    }
    if (evCode != EC_COMPLETE) {
        return fail(_T("the remux did not finish cleanly"));
    }
    const int outChans = mux.actx ? mux.actx->ch_layout.nb_channels : 0;
    pPcmSink->Finish(); // pad, flush and drain the E-AC-3 encoder
    if (!mux.ok) {
        return fail(_T("the audio could not be encoded to the end"));
    }
    if (!FinishEac3Output(mux)) {
        return fail(_T("the output file could not be finalized"));
    }
    CloseEac3Output(mux);

    CASTING_LOG(_T("remux E-AC-3 (LAV): copied the video and wrote %d-channel E-AC-3"), outChans);
    return true;
}

bool CastLavRemuxToEac3Mp4(const CString& srcPath, const CastMediaInfo& info, int targetChannels,
                           const CString& outPath, const CastTranscodeProgress& prog,
                           CString* pError)
{
    // The same worker-thread wrapper as CastLavRemuxToMp4: the graph runs in
    // its own multithreaded apartment and the caller blocks on the join, so
    // nothing the graph does has to marshal into a thread waiting for it.
    CASTING_LOG(_T("remux E-AC-3 (LAV): %s"), CastDescribeMedia(info).GetString());
    bool ok = false;
    CString err;
    std::thread worker([&]() {
        const HRESULT hrCo = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
        const bool mf = SUCCEEDED(MFStartup(MF_VERSION)); // the wrapper this clones raises it; FFmpeg needs none
        ok = Eac3RemuxGraph(srcPath, targetChannels, outPath, prog, &err);
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
