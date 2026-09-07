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
}

// Builds and runs the decode graph. Runs on its own thread (see below) so its
// COM apartment is not the caller's UI STA -- a DirectShow graph built there and
// waited on with WaitForCompletion deadlocks, since the wait blocks the very
// thread the graph needs to deliver its completion to.
static bool DecodeGraph(const CString& srcPath, int targetChannels, const CString& outPath,
                        CString* pError, HANDLE hCancel)
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
                         CString* pError, HANDLE hCancel)
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
        ok = DecodeGraph(srcPath, targetChannels, outFlacPath, &err, hCancel);
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
