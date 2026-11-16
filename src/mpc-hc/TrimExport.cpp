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
#include "TrimExport.h"
#include <mfapi.h>
#include <mfidl.h>
#include <mferror.h>
#include <mfreadwrite.h>
#include <codecapi.h>
#include <algorithm>
#include <deque>
#include <optional>
#include <random>
#include <vector>

namespace TrimExport
{
    namespace
    {
        // The SDK declares these for Windows 8 and later only; Windows 7 ignores unknown attributes.
        const GUID kReaderAdvancedVideoProcessing = { 0x0f81da2c, 0xb537, 0x4672, { 0xa8, 0xb2, 0xa6, 0x81, 0xb1, 0x73, 0x07, 0xa3 } };
        const GUID kVideoRotation = { 0xc380465d, 0x2271, 0x428c, { 0x9b, 0x83, 0xec, 0xea, 0x3b, 0x4a, 0x85, 0xc1 } };

        constexpr DWORD kNoStream = DWORD(-1);
        constexpr int64_t kSecond = 10000000;
        // The MPEG-4 source positions audio by chunk, so a seek may land audio after the video
        // keyframe; reading from earlier and discarding guarantees coverage of the cut.
        constexpr int64_t kSeekPreroll = 2 * kSecond;
        // Upper bound on frames a reference picture may precede in decode order.
        constexpr size_t kMaxReorderDepth = 16;

        class CMFSession
        {
        public:
            CMFSession()
                : m_com(SUCCEEDED(CoInitializeEx(nullptr, COINIT_MULTITHREADED)))
                , m_hr(MFStartup(MF_VERSION, MFSTARTUP_NOSOCKET))
            {
            }
            ~CMFSession()
            {
                if (SUCCEEDED(m_hr)) {
                    MFShutdown();
                }
                if (m_com) {
                    CoUninitialize();
                }
            }
            HRESULT GetHResult() const { return m_hr; }

        private:
            const bool m_com;
            const HRESULT m_hr;
        };

        Status StatusFromHResult(HRESULT hr, Status fallback)
        {
            switch (hr) {
                case HRESULT_FROM_WIN32(ERROR_DISK_FULL):
                case HRESULT_FROM_WIN32(ERROR_HANDLE_DISK_FULL):
                    return Status::DiskFull;
                case E_ACCESSDENIED:
                case HRESULT_FROM_WIN32(ERROR_SHARING_VIOLATION):
                case HRESULT_FROM_WIN32(ERROR_LOCK_VIOLATION):
                case HRESULT_FROM_WIN32(ERROR_WRITE_PROTECT):
                    return Status::AccessDenied;
                default:
                    return fallback;
            }
        }

        struct Streams {
            DWORD video = kNoStream;
            DWORD audio = kNoStream;
            int audioCount = 0;
            CComPtr<IMFMediaType> videoType;
            CComPtr<IMFMediaType> audioType;
        };

        GUID Subtype(IMFMediaType* type)
        {
            GUID subtype = GUID_NULL;
            if (type) {
                type->GetGUID(MF_MT_SUBTYPE, &subtype);
            }
            return subtype;
        }

        HRESULT CreateReader(const std::wstring& path, bool decode, CComPtr<IMFSourceReader>& reader)
        {
            CComPtr<IMFAttributes> attributes;
            HRESULT hr = MFCreateAttributes(&attributes, 1);
            if (SUCCEEDED(hr) && decode) {
                hr = attributes->SetUINT32(kReaderAdvancedVideoProcessing, TRUE);
            }
            if (SUCCEEDED(hr)) {
                hr = MFCreateSourceReaderFromURL(path.c_str(), attributes, &reader);
            }
            return hr;
        }

        // Selects the first video and the first audio stream and nothing else.
        HRESULT SelectStreams(IMFSourceReader* reader, Streams& streams)
        {
            HRESULT hr = reader->SetStreamSelection(DWORD(MF_SOURCE_READER_ALL_STREAMS), FALSE);
            for (DWORD i = 0; SUCCEEDED(hr); ++i) {
                CComPtr<IMFMediaType> type;
                if (FAILED(reader->GetNativeMediaType(i, 0, &type))) {
                    break;
                }
                GUID major = GUID_NULL;
                type->GetMajorType(&major);
                if (major == MFMediaType_Video && streams.video == kNoStream) {
                    streams.video = i;
                    streams.videoType = type;
                    hr = reader->SetStreamSelection(i, TRUE);
                } else if (major == MFMediaType_Audio) {
                    if (streams.audio == kNoStream) {
                        streams.audio = i;
                        streams.audioType = type;
                        hr = reader->SetStreamSelection(i, TRUE);
                    }
                    ++streams.audioCount;
                }
            }
            return SUCCEEDED(hr) && streams.video == kNoStream ? MF_E_INVALIDSTREAMNUMBER : hr;
        }

        HRESULT Seek(IMFSourceReader* reader, int64_t position)
        {
            PROPVARIANT var;
            PropVariantInit(&var);
            var.vt = VT_I8;
            var.hVal.QuadPart = std::max<int64_t>(position, 0);
            return reader->SetCurrentPosition(GUID_NULL, var);
        }

        int64_t GetDuration(IMFSourceReader* reader)
        {
            PROPVARIANT var;
            PropVariantInit(&var);
            int64_t duration = 0;
            if (SUCCEEDED(reader->GetPresentationAttribute(DWORD(MF_SOURCE_READER_MEDIASOURCE), MF_PD_DURATION, &var))
                    && var.vt == VT_UI8) {
                duration = int64_t(var.uhVal.QuadPart);
            }
            PropVariantClear(&var);
            return duration;
        }

        uint64_t GetFileSize(IMFSourceReader* reader)
        {
            PROPVARIANT var;
            PropVariantInit(&var);
            uint64_t size = 0;
            if (SUCCEEDED(reader->GetPresentationAttribute(DWORD(MF_SOURCE_READER_MEDIASOURCE), MF_PD_TOTAL_FILE_SIZE, &var))
                    && var.vt == VT_UI8) {
                size = var.uhVal.QuadPart;
            }
            PropVariantClear(&var);
            return size;
        }

        // The playback timeline starts at the earliest timestamp of the selected streams,
        // which is how the splitter presents it.
        HRESULT ReadTimelineOffset(IMFSourceReader* reader, const Streams& streams, int64_t& offset)
        {
            offset = 0;
            HRESULT hr = Seek(reader, 0);
            std::optional<int64_t> earliest;
            int videoSamples = 0;
            bool audioSeen = streams.audio == kNoStream;
            for (int i = 0; SUCCEEDED(hr) && i < 64 && (videoSamples < int(kMaxReorderDepth) || !audioSeen); ++i) {
                DWORD index = 0, flags = 0;
                LONGLONG time = 0;
                CComPtr<IMFSample> sample;
                hr = reader->ReadSample(DWORD(MF_SOURCE_READER_ANY_STREAM), 0, &index, &flags, &time, &sample);
                if (FAILED(hr) || (flags & MF_SOURCE_READERF_ENDOFSTREAM)) {
                    break;
                }
                if (!sample) {
                    continue;
                }
                earliest = std::min(earliest.value_or(time), int64_t(time));
                if (index == streams.video) {
                    ++videoSamples;
                } else {
                    audioSeen = true;
                }
            }
            offset = earliest.value_or(0);
            return hr;
        }

        // Presentation time of the last video keyframe at or before `target`. Only the video
        // stream may be selected.
        HRESULT FindKeyframe(IMFSourceReader* reader, DWORD video, int64_t target, int64_t& keyframe)
        {
            for (int64_t preroll = kSeekPreroll;; preroll *= 2) {
                const int64_t seekTo = std::max<int64_t>(target - preroll, 0);
                HRESULT hr = Seek(reader, seekTo);
                std::optional<int64_t> found, first;
                while (SUCCEEDED(hr)) {
                    DWORD flags = 0;
                    LONGLONG time = 0;
                    CComPtr<IMFSample> sample;
                    hr = reader->ReadSample(video, 0, nullptr, &flags, &time, &sample);
                    if (FAILED(hr) || (flags & MF_SOURCE_READERF_ENDOFSTREAM)) {
                        break;
                    }
                    if (!sample || !MFGetAttributeUINT32(sample, MFSampleExtension_CleanPoint, FALSE)) {
                        if (sample && time > target) {
                            break;
                        }
                        continue;
                    }
                    if (!first) {
                        first = time;
                    }
                    if (time > target) {
                        break;
                    }
                    found = time;
                }
                if (FAILED(hr)) {
                    return hr;
                }
                if (found) {
                    keyframe = *found;
                    return S_OK;
                }
                if (seekTo == 0) {
                    if (!first) {
                        return MF_E_END_OF_STREAM;
                    }
                    keyframe = *first;
                    return S_OK;
                }
            }
        }

        bool IsFastContainer(const std::wstring& path)
        {
            const wchar_t* ext = PathFindExtensionW(path.c_str());
            for (const wchar_t* supported : { L".mp4", L".m4v", L".mov" }) {
                if (_wcsicmp(ext, supported) == 0) {
                    return true;
                }
            }
            return false;
        }

        Status CheckFast(const std::wstring& path, const Streams& streams)
        {
            if (!IsFastContainer(path)) {
                return Status::FastContainer;
            }
            if (Subtype(streams.videoType) != MFVideoFormat_H264) {
                return Status::FastVideoCodec;
            }
            if (streams.audio != kNoStream && Subtype(streams.audioType) != MFAudioFormat_AAC) {
                return Status::FastAudioCodec;
            }
            return Status::Ok;
        }

        bool HasEncoder(const GUID& category, const GUID& major, const GUID& subtype)
        {
            MFT_REGISTER_TYPE_INFO output = { major, subtype };
            IMFActivate** activates = nullptr;
            UINT32 count = 0;
            if (FAILED(MFTEnumEx(category, MFT_ENUM_FLAG_SYNCMFT | MFT_ENUM_FLAG_ASYNCMFT | MFT_ENUM_FLAG_LOCALMFT | MFT_ENUM_FLAG_SORTANDFILTER,
                                 nullptr, &output, &activates, &count))) {
                return false;
            }
            for (UINT32 i = 0; i < count; ++i) {
                activates[i]->Release();
            }
            CoTaskMemFree(activates);
            return count > 0;
        }

        struct VideoFormat {
            UINT32 frameWidth = 0;      // allocated size of decoded frames
            UINT32 frameHeight = 0;
            LONG stride = 0;
            UINT32 left = 0;            // visible region, even-aligned for NV12
            UINT32 top = 0;
            UINT32 width = 0;
            UINT32 height = 0;
            UINT32 fpsNum = 0;
            UINT32 fpsDen = 0;
            UINT32 parNum = 1;
            UINT32 parDen = 1;
        };

        struct AudioFormat {
            UINT32 rate = 0;
            UINT32 channels = 0;
        };

        bool IsHdrTransfer(IMFMediaType* type)
        {
            const UINT32 transfer = MFGetAttributeUINT32(type, MF_MT_TRANSFER_FUNCTION, MFVideoTransFunc_Unknown);
            return transfer == MFVideoTransFunc_2084 || transfer == MFVideoTransFunc_HLG;
        }

        HRESULT ReadVideoFormat(IMFMediaType* current, IMFMediaType* native, VideoFormat& format)
        {
            if (Subtype(current) != MFVideoFormat_NV12) {
                return MF_E_INVALIDMEDIATYPE;
            }
            HRESULT hr = MFGetAttributeSize(current, MF_MT_FRAME_SIZE, &format.frameWidth, &format.frameHeight);
            if (FAILED(hr)) {
                return hr;
            }
            const UINT32 interlace = MFGetAttributeUINT32(current, MF_MT_INTERLACE_MODE, MFVideoInterlace_Progressive);
            if (interlace >= MFVideoInterlace_FieldInterleavedUpperFirst && interlace <= MFVideoInterlace_FieldSingleLower) {
                return MF_E_INVALIDMEDIATYPE;
            }
            format.stride = LONG(MFGetAttributeUINT32(current, MF_MT_DEFAULT_STRIDE, format.frameWidth));
            if (format.stride < LONG(format.frameWidth)) {
                format.stride = LONG(format.frameWidth);
            }

            MFVideoArea area = {};
            UINT32 areaSize = 0;
            if (SUCCEEDED(current->GetBlob(MF_MT_MINIMUM_DISPLAY_APERTURE, reinterpret_cast<UINT8*>(&area), sizeof(area), &areaSize))
                    && areaSize == sizeof(area) && area.Area.cx > 0 && area.Area.cy > 0) {
                format.left = UINT32(std::max<LONG>(area.OffsetX.value, 0));
                format.top = UINT32(std::max<LONG>(area.OffsetY.value, 0));
                format.width = std::min<UINT32>(UINT32(area.Area.cx), format.frameWidth - format.left);
                format.height = std::min<UINT32>(UINT32(area.Area.cy), format.frameHeight - format.top);
            } else {
                format.width = format.frameWidth;
                format.height = format.frameHeight;
            }
            format.left &= ~1u;
            format.top &= ~1u;
            format.width &= ~1u;
            format.height &= ~1u;
            if (format.width < 16 || format.height < 16) {
                return MF_E_INVALIDMEDIATYPE;
            }

            if (FAILED(MFGetAttributeRatio(native, MF_MT_FRAME_RATE, &format.fpsNum, &format.fpsDen))
                    || !format.fpsNum || !format.fpsDen) {
                MFGetAttributeRatio(current, MF_MT_FRAME_RATE, &format.fpsNum, &format.fpsDen);
            }
            if (!format.fpsNum || !format.fpsDen) {
                format.fpsNum = 30;
                format.fpsDen = 1;
            }
            if (FAILED(MFGetAttributeRatio(current, MF_MT_PIXEL_ASPECT_RATIO, &format.parNum, &format.parDen))
                    || !format.parNum || !format.parDen) {
                format.parNum = format.parDen = 1;
            }
            return S_OK;
        }

        HRESULT ConfigureVideoDecoding(IMFSourceReader* reader, const Streams& streams, VideoFormat& format)
        {
            if (IsHdrTransfer(streams.videoType)) {
                return MF_E_INVALIDMEDIATYPE;
            }
            CComPtr<IMFMediaType> request, current;
            HRESULT hr = MFCreateMediaType(&request);
            if (SUCCEEDED(hr)) {
                request->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Video);
                request->SetGUID(MF_MT_SUBTYPE, MFVideoFormat_NV12);
                hr = reader->SetCurrentMediaType(streams.video, nullptr, request);
            }
            if (SUCCEEDED(hr)) {
                hr = reader->GetCurrentMediaType(streams.video, &current);
            }
            return SUCCEEDED(hr) ? ReadVideoFormat(current, streams.videoType, format) : hr;
        }

        HRESULT ConfigureAudioDecoding(IMFSourceReader* reader, const Streams& streams, AudioFormat& format)
        {
            auto request = [&](UINT32 rate, UINT32 channels) {
                CComPtr<IMFMediaType> type;
                HRESULT hr = MFCreateMediaType(&type);
                if (SUCCEEDED(hr)) {
                    type->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Audio);
                    type->SetGUID(MF_MT_SUBTYPE, MFAudioFormat_PCM);
                    type->SetUINT32(MF_MT_AUDIO_BITS_PER_SAMPLE, 16);
                    if (rate) {
                        type->SetUINT32(MF_MT_AUDIO_SAMPLES_PER_SECOND, rate);
                        type->SetUINT32(MF_MT_AUDIO_NUM_CHANNELS, channels);
                        type->SetUINT32(MF_MT_AUDIO_BLOCK_ALIGNMENT, channels * 2);
                        type->SetUINT32(MF_MT_AUDIO_AVG_BYTES_PER_SECOND, rate * channels * 2);
                    }
                    hr = reader->SetCurrentMediaType(streams.audio, nullptr, type);
                }
                CComPtr<IMFMediaType> current;
                if (SUCCEEDED(hr)) {
                    hr = reader->GetCurrentMediaType(streams.audio, &current);
                }
                if (SUCCEEDED(hr)) {
                    format.rate = MFGetAttributeUINT32(current, MF_MT_AUDIO_SAMPLES_PER_SECOND, 0);
                    format.channels = MFGetAttributeUINT32(current, MF_MT_AUDIO_NUM_CHANNELS, 0);
                    if (MFGetAttributeUINT32(current, MF_MT_AUDIO_BITS_PER_SAMPLE, 0) != 16) {
                        hr = MF_E_INVALIDMEDIATYPE;
                    }
                }
                return hr;
            };

            // The AAC encoder takes 44.1/48 kHz mono or stereo; anything else is converted.
            HRESULT hr = request(0, 0);
            if (SUCCEEDED(hr) && (format.rate == 44100 || format.rate == 48000) && (format.channels == 1 || format.channels == 2)) {
                return S_OK;
            }
            const UINT32 rate = SUCCEEDED(hr) && format.rate == 44100 ? 44100 : 48000;
            const UINT32 channels = SUCCEEDED(hr) && format.channels == 1 ? 1 : 2;
            hr = request(rate, channels);
            if (SUCCEEDED(hr) && (format.rate != rate || format.channels != channels)) {
                hr = MF_E_INVALIDMEDIATYPE;
            }
            return hr;
        }

        Status CheckPrecise(const std::wstring& path, const Streams& compressed)
        {
            if (!HasEncoder(MFT_CATEGORY_VIDEO_ENCODER, MFMediaType_Video, MFVideoFormat_H264)
                    || (compressed.audio != kNoStream && !HasEncoder(MFT_CATEGORY_AUDIO_ENCODER, MFMediaType_Audio, MFAudioFormat_AAC))) {
                return Status::EncoderMissing;
            }
            CComPtr<IMFSourceReader> reader;
            Streams streams;
            if (FAILED(CreateReader(path, true, reader)) || FAILED(SelectStreams(reader, streams))) {
                return Status::SourceUnreadable;
            }
            VideoFormat video;
            if (FAILED(ConfigureVideoDecoding(reader, streams, video))) {
                return Status::PreciseVideo;
            }
            AudioFormat audio;
            if (streams.audio != kNoStream && FAILED(ConfigureAudioDecoding(reader, streams, audio))) {
                return Status::PreciseAudio;
            }
            return Status::Ok;
        }

        std::wstring DirectoryOf(const std::wstring& path)
        {
            return path.substr(0, size_t(PathFindFileNameW(path.c_str()) - path.c_str()));
        }

        HRESULT CreateTempFile(const std::wstring& destination, std::wstring& tempPath, CComPtr<IMFByteStream>& stream)
        {
            const std::wstring directory = DirectoryOf(destination);
            const wchar_t* name = PathFindFileNameW(destination.c_str());
            std::random_device random;
            HRESULT hr = E_FAIL;
            for (int attempt = 0; attempt < 16; ++attempt) {
                wchar_t suffix[16];
                swprintf_s(suffix, L".%08x.tmp", unsigned(random()));
                tempPath = directory + L"~" + name + suffix;
                stream.Release();
                hr = MFCreateFile(MF_ACCESSMODE_WRITE, MF_OPENMODE_FAIL_IF_EXIST, MF_FILEFLAGS_NONE, tempPath.c_str(), &stream);
                if (hr != HRESULT_FROM_WIN32(ERROR_FILE_EXISTS) && hr != HRESULT_FROM_WIN32(ERROR_ALREADY_EXISTS)) {
                    break;
                }
            }
            if (FAILED(hr)) {
                tempPath.clear();
            }
            return hr;
        }

        HRESULT CreateWriter(IMFByteStream* stream, CComPtr<IMFSinkWriter>& writer)
        {
            CComPtr<IMFAttributes> attributes;
            HRESULT hr = MFCreateAttributes(&attributes, 1);
            if (SUCCEEDED(hr)) {
                hr = attributes->SetGUID(MF_TRANSCODE_CONTAINERTYPE, MFTranscodeContainerType_MPEG4);
            }
            if (SUCCEEDED(hr)) {
                hr = MFCreateSinkWriterFromURL(nullptr, stream, attributes, &writer);
            }
            return hr;
        }

        void CopyAttribute(IMFAttributes* from, IMFAttributes* to, REFGUID key)
        {
            PROPVARIANT var;
            PropVariantInit(&var);
            if (SUCCEEDED(from->GetItem(key, &var))) {
                to->SetItem(key, var);
            }
            PropVariantClear(&var);
        }

        void CopyColorAttributes(IMFAttributes* from, IMFAttributes* to)
        {
            for (const GUID* key : { &MF_MT_VIDEO_PRIMARIES, &MF_MT_TRANSFER_FUNCTION, &MF_MT_YUV_MATRIX, &MF_MT_VIDEO_NOMINAL_RANGE, &MF_MT_VIDEO_CHROMA_SITING }) {
                CopyAttribute(from, to, *key);
            }
        }

        UINT32 ChooseVideoBitrate(IMFSourceReader* reader, const Streams& streams, const VideoFormat& format)
        {
            uint64_t bitrate = MFGetAttributeUINT32(streams.videoType, MF_MT_AVG_BITRATE, 0);
            if (!bitrate) {
                const int64_t duration = GetDuration(reader);
                if (duration > 0) {
                    bitrate = GetFileSize(reader) * 8 * kSecond / uint64_t(duration);
                }
            }
            // Re-encoding at less than the source rate visibly compounds generation loss, and
            // very low-rate sources still need a sane floor for the encoder.
            const uint64_t pixelRate = uint64_t(format.width) * format.height * format.fpsNum / format.fpsDen;
            const uint64_t floor = pixelRate / 12;
            const uint64_t ceiling = format.width * format.height > 1920 * 1088 ? 100000000 : 50000000;
            return UINT32(std::clamp<uint64_t>(std::max(bitrate, floor), 1000000, ceiling));
        }

        HRESULT AddVideoEncoderStream(IMFSinkWriter* writer, UINT32 bitrate, UINT32 profile, const Streams& streams, const VideoFormat& format, DWORD& index)
        {
            auto setCommon = [&](IMFMediaType* type) {
                type->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Video);
                type->SetUINT32(MF_MT_INTERLACE_MODE, MFVideoInterlace_Progressive);
                MFSetAttributeSize(type, MF_MT_FRAME_SIZE, format.width, format.height);
                MFSetAttributeRatio(type, MF_MT_FRAME_RATE, format.fpsNum, format.fpsDen);
                MFSetAttributeRatio(type, MF_MT_PIXEL_ASPECT_RATIO, format.parNum, format.parDen);
                CopyColorAttributes(streams.videoType, type);
            };

            CComPtr<IMFMediaType> input;
            HRESULT hr = MFCreateMediaType(&input);
            if (SUCCEEDED(hr)) {
                setCommon(input);
                input->SetGUID(MF_MT_SUBTYPE, MFVideoFormat_NV12);
                input->SetUINT32(MF_MT_DEFAULT_STRIDE, format.width);
                input->SetUINT32(MF_MT_ALL_SAMPLES_INDEPENDENT, TRUE);
            }

            CComPtr<IMFAttributes> parameters;
            if (SUCCEEDED(hr)) {
                hr = MFCreateAttributes(&parameters, 4);
            }
            if (SUCCEEDED(hr)) {
                parameters->SetUINT32(CODECAPI_AVEncCommonRateControlMode, eAVEncCommonRateControlMode_UnconstrainedVBR);
                parameters->SetUINT32(CODECAPI_AVEncCommonMeanBitRate, bitrate);
                parameters->SetUINT32(CODECAPI_AVEncMPVGOPSize, std::clamp<UINT32>(2 * format.fpsNum / format.fpsDen, 1, 600));
                // The MPEG-4 sink writes no edit list, so B-frame reordering delay would shift
                // the video track against the audio by one frame.
                parameters->SetUINT32(CODECAPI_AVEncMPVDefaultBPictureCount, 0);
            }

            CComPtr<IMFMediaType> output;
            if (SUCCEEDED(hr)) {
                hr = MFCreateMediaType(&output);
            }
            if (SUCCEEDED(hr)) {
                setCommon(output);
                output->SetGUID(MF_MT_SUBTYPE, MFVideoFormat_H264);
                output->SetUINT32(MF_MT_AVG_BITRATE, bitrate);
                output->SetUINT32(MF_MT_MPEG2_PROFILE, profile);
                CopyAttribute(streams.videoType, output, kVideoRotation);
                hr = writer->AddStream(output, &index);
            }
            if (SUCCEEDED(hr)) {
                hr = writer->SetInputMediaType(index, input, parameters);
            }
            return hr;
        }

        HRESULT AddAudioEncoderStream(IMFSinkWriter* writer, const AudioFormat& format, DWORD& index)
        {
            CComPtr<IMFMediaType> output, input;
            HRESULT hr = MFCreateMediaType(&output);
            if (SUCCEEDED(hr)) {
                output->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Audio);
                output->SetGUID(MF_MT_SUBTYPE, MFAudioFormat_AAC);
                output->SetUINT32(MF_MT_AUDIO_BITS_PER_SAMPLE, 16);
                output->SetUINT32(MF_MT_AUDIO_SAMPLES_PER_SECOND, format.rate);
                output->SetUINT32(MF_MT_AUDIO_NUM_CHANNELS, format.channels);
                // 96 kbit/s per channel: the encoder accepts 12000, 16000, 20000 or 24000 bytes/s.
                output->SetUINT32(MF_MT_AUDIO_AVG_BYTES_PER_SECOND, format.channels == 1 ? 12000 : 24000);
                hr = writer->AddStream(output, &index);
            }
            if (SUCCEEDED(hr)) {
                hr = MFCreateMediaType(&input);
            }
            if (SUCCEEDED(hr)) {
                input->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Audio);
                input->SetGUID(MF_MT_SUBTYPE, MFAudioFormat_PCM);
                input->SetUINT32(MF_MT_AUDIO_BITS_PER_SAMPLE, 16);
                input->SetUINT32(MF_MT_AUDIO_SAMPLES_PER_SECOND, format.rate);
                input->SetUINT32(MF_MT_AUDIO_NUM_CHANNELS, format.channels);
                input->SetUINT32(MF_MT_AUDIO_BLOCK_ALIGNMENT, format.channels * 2);
                input->SetUINT32(MF_MT_AUDIO_AVG_BYTES_PER_SECOND, format.rate * format.channels * 2);
                input->SetUINT32(MF_MT_ALL_SAMPLES_INDEPENDENT, TRUE);
                hr = writer->SetInputMediaType(index, input, nullptr);
            }
            return hr;
        }

        HRESULT CreateSample(DWORD size, CComPtr<IMFSample>& sample, CComPtr<IMFMediaBuffer>& buffer)
        {
            HRESULT hr = MFCreateMemoryBuffer(size, &buffer);
            if (SUCCEEDED(hr)) {
                hr = MFCreateSample(&sample);
            }
            if (SUCCEEDED(hr)) {
                hr = sample->AddBuffer(buffer);
            }
            return hr;
        }

        // Copies the visible region of a decoded NV12 frame into a tightly packed one.
        HRESULT PackFrame(IMFSample* decoded, const VideoFormat& format, CComPtr<IMFSample>& packed)
        {
            CComPtr<IMFMediaBuffer> source;
            HRESULT hr = decoded->ConvertToContiguousBuffer(&source);
            if (FAILED(hr)) {
                return hr;
            }

            BYTE* data = nullptr;
            LONG pitch = 0;
            DWORD length = 0;
            CComQIPtr<IMF2DBuffer> buffer2d(source);
            if (buffer2d) {
                hr = buffer2d->Lock2D(&data, &pitch);
            } else {
                hr = source->Lock(&data, nullptr, &length);
                pitch = format.stride;
                if (SUCCEEDED(hr) && length < DWORD(pitch) * format.frameHeight * 3 / 2) {
                    source->Unlock();
                    hr = MF_E_BUFFERTOOSMALL;
                }
            }
            if (FAILED(hr)) {
                return hr;
            }
            if (pitch <= 0) {
                buffer2d ? buffer2d->Unlock2D() : source->Unlock();
                return MF_E_INVALIDMEDIATYPE;
            }

            const DWORD size = format.width * format.height * 3 / 2;
            CComPtr<IMFMediaBuffer> target;
            hr = CreateSample(size, packed, target);
            BYTE* out = nullptr;
            if (SUCCEEDED(hr)) {
                hr = target->Lock(&out, nullptr, nullptr);
            }
            if (SUCCEEDED(hr)) {
                const BYTE* luma = data + size_t(format.top) * pitch + format.left;
                for (UINT32 y = 0; y < format.height; ++y) {
                    memcpy(out + size_t(y) * format.width, luma + size_t(y) * pitch, format.width);
                }
                const BYTE* chroma = data + size_t(format.frameHeight) * pitch + size_t(format.top / 2) * pitch + format.left;
                BYTE* outChroma = out + size_t(format.width) * format.height;
                for (UINT32 y = 0; y < format.height / 2; ++y) {
                    memcpy(outChroma + size_t(y) * format.width, chroma + size_t(y) * pitch, format.width);
                }
                target->Unlock();
                hr = target->SetCurrentLength(size);
            }
            buffer2d ? buffer2d->Unlock2D() : source->Unlock();
            return hr;
        }

        HRESULT DecodeOneFrame(IMFSourceReader* reader, DWORD video)
        {
            for (int i = 0; i < 256; ++i) {
                DWORD flags = 0;
                LONGLONG time = 0;
                CComPtr<IMFSample> sample;
                HRESULT hr = reader->ReadSample(video, 0, nullptr, &flags, &time, &sample);
                if (FAILED(hr)) {
                    return hr;
                }
                if (flags & MF_SOURCE_READERF_ENDOFSTREAM) {
                    return MF_E_END_OF_STREAM;
                }
                if (sample) {
                    return S_OK;
                }
            }
            return MF_E_END_OF_STREAM;
        }

        // A finalized MP4 with the expected tracks, a plausible duration and decodable video
        // at both ends.
        bool ValidateOutput(const std::wstring& path, bool expectAudio, int64_t expectedDuration)
        {
            CComPtr<IMFByteStream> stream;
            if (FAILED(MFCreateFile(MF_ACCESSMODE_READ, MF_OPENMODE_FAIL_IF_NOT_EXIST, MF_FILEFLAGS_NONE, path.c_str(), &stream))) {
                return false;
            }
            if (CComQIPtr<IMFAttributes> attributes = stream) {
                attributes->SetString(MF_BYTESTREAM_CONTENT_TYPE, L"video/mp4");
            }
            bool valid = false;
            {
                CComPtr<IMFAttributes> attributes;
                CComPtr<IMFSourceReader> reader;
                Streams streams;
                VideoFormat format;
                if (SUCCEEDED(MFCreateAttributes(&attributes, 1))
                        && SUCCEEDED(attributes->SetUINT32(kReaderAdvancedVideoProcessing, TRUE))
                        && SUCCEEDED(MFCreateSourceReaderFromByteStream(stream, attributes, &reader))
                        && SUCCEEDED(SelectStreams(reader, streams))
                        && Subtype(streams.videoType) == MFVideoFormat_H264
                        && (streams.audio != kNoStream) == expectAudio
                        && (!expectAudio || Subtype(streams.audioType) == MFAudioFormat_AAC)) {
                    const int64_t duration = GetDuration(reader);
                    const int64_t tolerance = std::max<int64_t>(kSecond / 2, expectedDuration / 100);
                    reader->SetStreamSelection(DWORD(MF_SOURCE_READER_ALL_STREAMS), FALSE);
                    reader->SetStreamSelection(streams.video, TRUE);
                    // A stream-copied format the system cannot decode is still checked for
                    // readable samples at both ends.
                    ConfigureVideoDecoding(reader, streams, format);
                    valid = std::abs(duration - expectedDuration) <= tolerance
                            && SUCCEEDED(DecodeOneFrame(reader, streams.video))
                            && SUCCEEDED(Seek(reader, duration - kSecond))
                            && SUCCEEDED(DecodeOneFrame(reader, streams.video));
                }
            }
            stream->Close();
            return valid;
        }

        // MP4 edit lists. A stream copy has to begin on a keyframe; an edit list tells the
        // player to decode from there but to present only [Start, End), as ffmpeg does for
        // "-ss ... -c copy". Only the movie box is rewritten: the Media Foundation sink places
        // it after the media data, so no chunk offset moves.
        namespace Mp4
        {
            uint32_t Get32(const BYTE* p)
            {
                return uint32_t(p[0]) << 24 | uint32_t(p[1]) << 16 | uint32_t(p[2]) << 8 | p[3];
            }

            uint64_t Get64(const BYTE* p)
            {
                return uint64_t(Get32(p)) << 32 | Get32(p + 4);
            }

            void Put32(BYTE* p, uint32_t v)
            {
                p[0] = BYTE(v >> 24);
                p[1] = BYTE(v >> 16);
                p[2] = BYTE(v >> 8);
                p[3] = BYTE(v);
            }

            void Put64(BYTE* p, uint64_t v)
            {
                Put32(p, uint32_t(v >> 32));
                Put32(p + 4, uint32_t(v));
            }

            constexpr uint32_t Tag(const char (&s)[5])
            {
                return uint32_t(BYTE(s[0])) << 24 | uint32_t(BYTE(s[1])) << 16 | uint32_t(BYTE(s[2])) << 8 | BYTE(s[3]);
            }

            struct Box {
                uint32_t type;
                size_t begin;       // header start
                size_t body;        // payload start
                size_t end;
            };

            bool Children(const std::vector<BYTE>& data, size_t begin, size_t end, std::vector<Box>& boxes)
            {
                boxes.clear();
                while (begin + 8 <= end) {
                    uint64_t size = Get32(&data[begin]);
                    size_t header = 8;
                    if (size == 1) {
                        if (begin + 16 > end) {
                            return false;
                        }
                        size = Get64(&data[begin + 8]);
                        header = 16;
                    } else if (size == 0) {
                        size = end - begin;
                    }
                    if (size < header || size > end - begin) {
                        return false;
                    }
                    boxes.push_back({ Get32(&data[begin + 4]), begin, begin + header, begin + size_t(size) });
                    begin += size_t(size);
                }
                return begin == end;
            }

            const Box* Find(const std::vector<Box>& boxes, uint32_t type)
            {
                auto it = std::find_if(boxes.begin(), boxes.end(), [type](const Box& b) { return b.type == type; });
                return it == boxes.end() ? nullptr : &*it;
            }

            // mvhd and mdhd share the layout of their timescale field.
            bool Timescale(const std::vector<BYTE>& data, const Box& box, uint32_t& timescale)
            {
                const size_t at = box.body + (data[box.body] == 1 ? 20 : 12);
                if (at + 4 > box.end) {
                    return false;
                }
                timescale = Get32(&data[at]);
                return timescale != 0;
            }

            void SetDuration(std::vector<BYTE>& data, const Box& box, size_t v0Offset, size_t v1Offset, uint64_t duration)
            {
                if (data[box.body] == 1) {
                    if (box.body + v1Offset + 8 <= box.end) {
                        Put64(&data[box.body + v1Offset], duration);
                    }
                } else if (box.body + v0Offset + 4 <= box.end) {
                    Put32(&data[box.body + v0Offset], uint32_t(std::min<uint64_t>(duration, UINT32_MAX)));
                }
            }

            std::vector<BYTE> EditBox(uint64_t segment, uint64_t mediaTime)
            {
                const bool wide = segment > UINT32_MAX || mediaTime > INT32_MAX;
                const size_t elstSize = 16 + (wide ? 20 : 12);
                std::vector<BYTE> box(8 + elstSize);
                Put32(&box[0], uint32_t(box.size()));
                Put32(&box[4], Tag("edts"));
                BYTE* elst = &box[8];
                Put32(elst, uint32_t(elstSize));
                Put32(elst + 4, Tag("elst"));
                elst[8] = wide ? 1 : 0;
                Put32(elst + 12, 1);
                if (wide) {
                    Put64(elst + 16, segment);
                    Put64(elst + 24, mediaTime);
                    Put32(elst + 32, 0x00010000);
                } else {
                    Put32(elst + 16, uint32_t(segment));
                    Put32(elst + 20, uint32_t(mediaTime));
                    Put32(elst + 24, 0x00010000);
                }
                return box;
            }

            uint64_t Scale(int64_t time, uint32_t timescale)
            {
                return uint64_t((time * timescale + kSecond / 2) / kSecond);
            }

            // Returns the movie box with one edit per track presenting [lead, lead + length)
            // of the track's media timeline, and the matching movie and track durations.
            std::optional<std::vector<BYTE>> EditMovie(std::vector<BYTE>& moov, int64_t lead, int64_t length)
            {
                std::vector<Box> top, trak, mdia;
                if (moov.size() < 8 || !Children(moov, 8, moov.size(), top)) {
                    return std::nullopt;
                }
                const Box* mvhd = Find(top, Tag("mvhd"));
                uint32_t movieScale = 0;
                if (!mvhd || !Timescale(moov, *mvhd, movieScale)) {
                    return std::nullopt;
                }
                const uint64_t segment = Scale(length, movieScale);
                SetDuration(moov, *mvhd, 16, 24, segment);

                std::vector<BYTE> out(8);
                Put32(&out[4], Tag("moov"));
                for (const Box& box : top) {
                    if (box.type != Tag("trak")) {
                        out.insert(out.end(), moov.begin() + box.begin, moov.begin() + box.end);
                        continue;
                    }
                    const Box* tkhd = nullptr;
                    const Box* media = nullptr;
                    const Box* mdhd = nullptr;
                    uint32_t mediaScale = 0;
                    if (!Children(moov, box.body, box.end, trak) || !(tkhd = Find(trak, Tag("tkhd")))
                            || !(media = Find(trak, Tag("mdia"))) || !Children(moov, media->body, media->end, mdia)
                            || !(mdhd = Find(mdia, Tag("mdhd"))) || !Timescale(moov, *mdhd, mediaScale)) {
                        return std::nullopt;
                    }
                    SetDuration(moov, *tkhd, 20, 28, segment);
                    const size_t start = out.size();
                    out.resize(start + 8);
                    Put32(&out[start + 4], Tag("trak"));
                    for (const Box& child : trak) {
                        if (child.type == Tag("edts")) {
                            continue;
                        }
                        out.insert(out.end(), moov.begin() + child.begin, moov.begin() + child.end);
                        if (child.type == Tag("tkhd")) {
                            const std::vector<BYTE> edts = EditBox(segment, Scale(lead, mediaScale));
                            out.insert(out.end(), edts.begin(), edts.end());
                        }
                    }
                    Put32(&out[start], uint32_t(out.size() - start));
                }
                Put32(&out[0], uint32_t(out.size()));
                return out;
            }
        }

        // Presents [lead, lead + length) of every track of the finalized file at path.
        HRESULT ApplyEditList(const std::wstring& path, int64_t lead, int64_t length)
        {
            CHandle file(CreateFileW(path.c_str(), GENERIC_READ | GENERIC_WRITE, 0, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr));
            if (file == INVALID_HANDLE_VALUE) {
                file.Detach();
                return HRESULT_FROM_WIN32(GetLastError());
            }
            LARGE_INTEGER fileSize = {};
            if (!GetFileSizeEx(file, &fileSize)) {
                return HRESULT_FROM_WIN32(GetLastError());
            }
            auto readAt = [&](uint64_t offset, BYTE* buffer, size_t size) {
                OVERLAPPED at = {};
                at.Offset = DWORD(offset);
                at.OffsetHigh = DWORD(offset >> 32);
                DWORD read = 0;
                return ReadFile(file, buffer, DWORD(size), &read, &at) && read == size;
            };

            // The movie box must follow the media data at the top level.
            const uint64_t total = uint64_t(fileSize.QuadPart);
            uint64_t position = 0, moovAt = 0, moovSize = 0;
            bool sawMedia = false;
            while (position + 8 <= total) {
                BYTE header[16];
                if (!readAt(position, header, 8)) {
                    return E_FAIL;
                }
                uint64_t size = Mp4::Get32(header);
                const uint32_t type = Mp4::Get32(header + 4);
                if (size == 1) {
                    if (!readAt(position + 8, header + 8, 8)) {
                        return E_FAIL;
                    }
                    size = Mp4::Get64(header + 8);
                } else if (size == 0) {
                    size = total - position;
                }
                if (size < 8 || size > total - position) {
                    return E_FAIL;
                }
                if (type == Mp4::Tag("mdat")) {
                    if (moovAt) {
                        return E_NOTIMPL;   // media after the movie box would move
                    }
                    sawMedia = true;
                } else if (type == Mp4::Tag("moov")) {
                    if (!sawMedia || moovAt || size > 256 * 1024 * 1024) {
                        return E_NOTIMPL;
                    }
                    moovAt = position;
                    moovSize = size;
                }
                position += size;
            }
            if (!moovAt) {
                return E_FAIL;
            }

            std::vector<BYTE> moov(static_cast<size_t>(moovSize));
            std::vector<BYTE> rest(static_cast<size_t>(total - moovAt - moovSize));
            if (!readAt(moovAt, moov.data(), moov.size()) || Mp4::Get32(moov.data()) != moovSize
                    || (!rest.empty() && !readAt(moovAt + moovSize, rest.data(), rest.size()))) {
                return E_FAIL;
            }
            const auto edited = Mp4::EditMovie(moov, lead, length);
            if (!edited) {
                return E_FAIL;
            }
            LARGE_INTEGER to = {};
            to.QuadPart = LONGLONG(moovAt);
            DWORD written = 0;
            if (!SetFilePointerEx(file, to, nullptr, FILE_BEGIN)
                    || !WriteFile(file, edited->data(), DWORD(edited->size()), &written, nullptr) || written != edited->size()
                    || (!rest.empty() && (!WriteFile(file, rest.data(), DWORD(rest.size()), &written, nullptr) || written != rest.size()))
                    || !SetEndOfFile(file) || !FlushFileBuffers(file)) {
                return HRESULT_FROM_WIN32(GetLastError());
            }
            return S_OK;
        }

        bool PathExists(const std::wstring& path)
        {
            return GetFileAttributesW(path.c_str()) != INVALID_FILE_ATTRIBUTES;
        }

        Status CheckDestination(const Request& request)
        {
            const DWORD attributes = GetFileAttributesW(request.destination.c_str());
            if (attributes == INVALID_FILE_ATTRIBUTES) {
                return Status::Ok;
            }
            if (IsSameFile(request.destination, request.source)) {
                return Status::DestinationIsSource;
            }
            if (!request.replaceExisting || (attributes & (FILE_ATTRIBUTE_DIRECTORY | FILE_ATTRIBUTE_READONLY))) {
                return attributes & FILE_ATTRIBUTE_READONLY ? Status::AccessDenied : Status::DestinationExists;
            }
            return Status::Ok;
        }

        bool HasFreeSpace(const std::wstring& destination, uint64_t required)
        {
            ULARGE_INTEGER available = {};
            if (!GetDiskFreeSpaceExW(DirectoryOf(destination).c_str(), &available, nullptr, nullptr)) {
                return true;
            }
            return available.QuadPart > required + required / 20 + 4 * 1024 * 1024;
        }
    }

    bool IsAvailable()
    {
        static const bool available = [] {
            wchar_t system[MAX_PATH];
            const UINT length = GetSystemDirectoryW(system, _countof(system));
            if (!length || length >= _countof(system)) {
                return false;
            }
            for (const wchar_t* name : { L"mfplat.dll", L"mfreadwrite.dll" }) {
                const std::wstring path = std::wstring(system) + L"\\" + name;
                HMODULE module = LoadLibraryExW(path.c_str(), nullptr, 0);
                if (!module) {
                    return false;
                }
                FreeLibrary(module);
            }
            return true;
        }();
        return available;
    }

    bool IsSameFile(const std::wstring& a, const std::wstring& b)
    {
        auto open = [](const std::wstring& path) {
            return CreateFileW(path.c_str(), 0, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr,
                               OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, nullptr);
        };
        HANDLE first = open(a);
        if (first == INVALID_HANDLE_VALUE) {
            return false;
        }
        HANDLE second = open(b);
        bool same = false;
        if (second != INVALID_HANDLE_VALUE) {
            BY_HANDLE_FILE_INFORMATION infoA, infoB;
            same = GetFileInformationByHandle(first, &infoA) && GetFileInformationByHandle(second, &infoB)
                   && infoA.dwVolumeSerialNumber == infoB.dwVolumeSerialNumber
                   && infoA.nFileIndexHigh == infoB.nFileIndexHigh
                   && infoA.nFileIndexLow == infoB.nFileIndexLow;
            CloseHandle(second);
        }
        CloseHandle(first);
        return same;
    }

    SourceInfo Probe(const std::wstring& source, int64_t start)
    {
        SourceInfo info;
        if (!IsAvailable()) {
            info.status = Status::MediaFoundationMissing;
            return info;
        }
        CMFSession session;
        if (FAILED(info.hr = session.GetHResult())) {
            info.status = Status::MediaFoundationMissing;
            return info;
        }

        CComPtr<IMFSourceReader> reader;
        Streams streams;
        info.hr = CreateReader(source, false, reader);
        if (SUCCEEDED(info.hr)) {
            info.hr = SelectStreams(reader, streams);
        }
        if (FAILED(info.hr)) {
            info.status = info.hr == MF_E_INVALIDSTREAMNUMBER ? Status::NoVideo : Status::SourceUnreadable;
            return info;
        }
        info.hasAudio = streams.audio != kNoStream;
        info.audioStreams = streams.audioCount;

        int64_t offset = 0, keyframe = 0;
        info.hr = ReadTimelineOffset(reader, streams, offset);
        if (SUCCEEDED(info.hr) && streams.audio != kNoStream) {
            info.hr = reader->SetStreamSelection(streams.audio, FALSE);
        }
        if (SUCCEEDED(info.hr)) {
            info.hr = FindKeyframe(reader, streams.video, start + offset, keyframe);
        }
        if (FAILED(info.hr)) {
            info.status = Status::SourceUnreadable;
            return info;
        }
        info.fastStart = std::max<int64_t>(keyframe - offset, 0);
        info.fast = CheckFast(source, streams);
        info.precise = CheckPrecise(source, streams);
        info.status = Status::Ok;
        return info;
    }

    namespace
    {
        class CExporter
        {
        public:
            CExporter(const Request& request, const std::atomic<bool>& cancel, HWND notifyWnd, UINT message)
                : m_request(request)
                , m_cancel(cancel)
                , m_notifyWnd(notifyWnd)
                , m_message(message)
            {
            }

            ~CExporter()
            {
                Discard();
            }

            Result Run();

        private:
            Status Open();
            Status RunFast();
            Status RunPrecise();
            Status ResetWriter();
            Status OnFormatChanged(DWORD index, VideoFormat& video);
            Status Publish(bool expectAudio);
            void Discard();
            Status Fail(HRESULT hr, Status fallback)
            {
                m_result.hr = hr;
                return StatusFromHResult(hr, fallback);
            }
            void ReportProgress(int64_t position);

            HRESULT WritePcm(const BYTE* data, int64_t firstFrame, int64_t frames);
            HRESULT WriteSilence(int64_t frames);
            HRESULT WriteAudioBlock(int64_t time, const std::vector<BYTE>& pcm, int64_t limit);

            const Request& m_request;
            const std::atomic<bool>& m_cancel;
            const HWND m_notifyWnd;
            const UINT m_message;
            Result m_result;

            CComPtr<IMFSourceReader> m_reader;
            Streams m_streams;
            int64_t m_offset = 0;           // source timestamp of timeline zero
            int64_t m_start = 0;            // requested range in source timestamps
            int64_t m_end = 0;
            int64_t m_progressBase = 0;
            int m_lastPermille = -1;
            std::optional<std::pair<int64_t, int64_t>> m_edit;    // lead and length of a stream copy

            std::wstring m_tempPath;
            CComPtr<IMFByteStream> m_tempStream;
            CComPtr<IMFSinkWriter> m_writer;
            DWORD m_videoOut = kNoStream;
            DWORD m_audioOut = kNoStream;

            AudioFormat m_audio;
            int64_t m_firstVideo = 0;       // source timestamp mapped to output zero
            int64_t m_audioWritten = 0;     // output audio position in sample frames
        };

        void CExporter::ReportProgress(int64_t position)
        {
            const int64_t span = std::max<int64_t>(m_end - m_progressBase, 1);
            const int permille = int(std::clamp<int64_t>((position - m_progressBase) * 1000 / span, 0, 1000));
            if (permille != m_lastPermille) {
                m_lastPermille = permille;
                PostMessage(m_notifyWnd, m_message, WPARAM(permille), 0);
            }
        }

        void CExporter::Discard()
        {
            m_writer.Release();
            if (m_tempStream) {
                m_tempStream->Close();
                m_tempStream.Release();
            }
            if (!m_tempPath.empty()) {
                DeleteFileW(m_tempPath.c_str());
                m_tempPath.clear();
            }
        }

        Status CExporter::Open()
        {
            const Status destination = CheckDestination(m_request);
            if (destination != Status::Ok) {
                return destination;
            }
            if (m_request.end <= m_request.start) {
                return Status::EmptyRange;
            }

            HRESULT hr = CreateReader(m_request.source, false, m_reader);
            if (SUCCEEDED(hr)) {
                hr = SelectStreams(m_reader, m_streams);
            }
            if (SUCCEEDED(hr)) {
                hr = ReadTimelineOffset(m_reader, m_streams, m_offset);
            }
            if (FAILED(hr)) {
                return Fail(hr, hr == MF_E_INVALIDSTREAMNUMBER ? Status::NoVideo : Status::SourceUnreadable);
            }
            m_start = m_request.start + m_offset;
            m_end = m_request.end + m_offset;

            hr = CreateTempFile(m_request.destination, m_tempPath, m_tempStream);
            if (SUCCEEDED(hr)) {
                hr = CreateWriter(m_tempStream, m_writer);
            }
            return SUCCEEDED(hr) ? Status::Ok : Fail(hr, Status::WriteFailed);
        }

        Result CExporter::Run()
        {
            Status status = Open();
            if (status == Status::Ok) {
                status = m_request.mode == Mode::Fast ? RunFast() : RunPrecise();
            }
            if (status == Status::Ok && m_cancel) {
                status = Status::Cancelled;
            }
            if (status == Status::Ok) {
                status = Publish(m_streams.audio != kNoStream);
            }
            if (status != Status::Ok) {
                Discard();
            }
            m_result.status = status;
            return m_result;
        }

        Status CExporter::RunFast()
        {
            Status status = CheckFast(m_request.source, m_streams);
            if (status != Status::Ok) {
                return status;
            }
            const bool hasAudio = m_streams.audio != kNoStream;
            const uint64_t fileSize = GetFileSize(m_reader);
            const int64_t duration = GetDuration(m_reader);
            if (fileSize && duration > 0
                    && !HasFreeSpace(m_request.destination, fileSize * uint64_t(std::min(m_end - m_start + kSeekPreroll, duration)) / uint64_t(duration))) {
                return Status::DiskFull;
            }

            int64_t keyframe = 0;
            HRESULT hr = hasAudio ? m_reader->SetStreamSelection(m_streams.audio, FALSE) : S_OK;
            if (SUCCEEDED(hr)) {
                hr = FindKeyframe(m_reader, m_streams.video, m_start, keyframe);
            }
            if (SUCCEEDED(hr) && hasAudio) {
                hr = m_reader->SetStreamSelection(m_streams.audio, TRUE);
            }
            if (FAILED(hr)) {
                return Fail(hr, Status::SourceUnreadable);
            }

            hr = m_writer->AddStream(m_streams.videoType, &m_videoOut);
            if (SUCCEEDED(hr)) {
                hr = m_writer->SetInputMediaType(m_videoOut, m_streams.videoType, nullptr);
            }
            if (SUCCEEDED(hr) && hasAudio) {
                hr = m_writer->AddStream(m_streams.audioType, &m_audioOut);
                if (SUCCEEDED(hr)) {
                    hr = m_writer->SetInputMediaType(m_audioOut, m_streams.audioType, nullptr);
                }
            }
            if (SUCCEEDED(hr)) {
                hr = m_writer->BeginWriting();
            }
            if (SUCCEEDED(hr)) {
                hr = Seek(m_reader, keyframe - kSeekPreroll);
            }
            if (FAILED(hr)) {
                return Fail(hr, Status::WriteFailed);
            }
            m_progressBase = keyframe;

            // Samples at or past the end are held back: one that a later sample still needs
            // as a reference is flushed when such a sample arrives, the rest are dropped.
            std::deque<CComPtr<IMFSample>> heldVideo, heldAudio;
            bool videoStarted = false, videoDone = false, audioDone = !hasAudio;
            int64_t videoEnd = keyframe;

            auto write = [&](DWORD stream, IMFSample* sample) {
                LONGLONG time = 0;
                sample->GetSampleTime(&time);
                HRESULT hr = sample->SetSampleTime(time - keyframe);
                return SUCCEEDED(hr) ? m_writer->WriteSample(stream, sample) : hr;
            };
            auto writeVideo = [&](IMFSample* sample) {
                LONGLONG time = 0, length = 0;
                sample->GetSampleTime(&time);
                sample->GetSampleDuration(&length);
                videoEnd = std::max<int64_t>(videoEnd, time + length);
                return write(m_videoOut, sample);
            };
            auto finishVideo = [&]() {
                videoDone = true;
                heldVideo.clear();
                m_reader->SetStreamSelection(m_streams.video, FALSE);
                HRESULT hr = S_OK;
                for (auto& sample : heldAudio) {
                    LONGLONG time = 0;
                    sample->GetSampleTime(&time);
                    if (time >= videoEnd) {
                        audioDone = true;
                        break;
                    }
                    if (FAILED(hr = write(m_audioOut, sample))) {
                        break;
                    }
                }
                heldAudio.clear();
                if (audioDone && hasAudio) {
                    m_reader->SetStreamSelection(m_streams.audio, FALSE);
                }
                return hr;
            };

            while (!(videoDone && audioDone)) {
                if (m_cancel) {
                    return Status::Cancelled;
                }
                DWORD index = 0, flags = 0;
                LONGLONG time = 0;
                CComPtr<IMFSample> sample;
                hr = m_reader->ReadSample(DWORD(MF_SOURCE_READER_ANY_STREAM), 0, &index, &flags, &time, &sample);
                if (FAILED(hr)) {
                    return Fail(hr, Status::SourceUnreadable);
                }
                if (flags & MF_SOURCE_READERF_ENDOFSTREAM) {
                    if (index == m_streams.video && !videoDone) {
                        hr = finishVideo();
                    } else if (index == m_streams.audio) {
                        audioDone = true;
                    }
                    if (FAILED(hr)) {
                        return Fail(hr, Status::WriteFailed);
                    }
                    continue;
                }
                if (!sample) {
                    continue;
                }

                if (index == m_streams.video && !videoDone) {
                    if (!videoStarted) {
                        if (time != keyframe || !MFGetAttributeUINT32(sample, MFSampleExtension_CleanPoint, FALSE)) {
                            continue;
                        }
                        sample->SetUINT32(MFSampleExtension_Discontinuity, TRUE);
                        videoStarted = true;
                    }
                    if (time < keyframe) {
                        continue;   // leading picture of an open GOP, undecodable without its predecessor
                    }
                    if (time >= m_end) {
                        heldVideo.push_back(sample);
                        if (MFGetAttributeUINT32(sample, MFSampleExtension_CleanPoint, FALSE) || heldVideo.size() > kMaxReorderDepth) {
                            hr = finishVideo();
                        }
                    } else {
                        for (auto& held : heldVideo) {
                            if (FAILED(hr = writeVideo(held))) {
                                break;
                            }
                        }
                        heldVideo.clear();
                        if (SUCCEEDED(hr)) {
                            hr = writeVideo(sample);
                        }
                        ReportProgress(time);
                    }
                } else if (index == m_streams.audio && !audioDone) {
                    // An audio frame cannot start before zero, so the one straddling the
                    // keyframe is dropped: the track then starts less than one frame late.
                    if (time < keyframe) {
                        continue;
                    }
                    if (videoDone) {
                        if (time >= videoEnd) {
                            audioDone = true;
                            m_reader->SetStreamSelection(m_streams.audio, FALSE);
                        } else {
                            hr = write(m_audioOut, sample);
                        }
                    } else if (time >= m_end) {
                        heldAudio.push_back(sample);
                    } else {
                        hr = write(m_audioOut, sample);
                    }
                }
                if (FAILED(hr)) {
                    return Fail(hr, Status::WriteFailed);
                }
            }

            const int64_t end = std::min(m_end, videoEnd);
            if (!videoStarted || end <= m_start) {
                return Status::EmptyRange;
            }
            hr = m_writer->Finalize();
            if (FAILED(hr)) {
                return Fail(hr, Status::WriteFailed);
            }
            // The output timeline starts at the keyframe; the edit list skips to Start.
            m_edit.emplace(m_start - keyframe, end - m_start);
            m_result.effectiveStart = m_start - m_offset;
            m_result.effectiveEnd = end - m_offset;
            return Status::Ok;
        }

        HRESULT CExporter::WritePcm(const BYTE* data, int64_t firstFrame, int64_t frames)
        {
            const DWORD blockAlign = m_audio.channels * 2;
            CComPtr<IMFSample> sample;
            CComPtr<IMFMediaBuffer> buffer;
            HRESULT hr = CreateSample(DWORD(frames * blockAlign), sample, buffer);
            BYTE* out = nullptr;
            if (SUCCEEDED(hr)) {
                hr = buffer->Lock(&out, nullptr, nullptr);
            }
            if (SUCCEEDED(hr)) {
                if (data) {
                    memcpy(out, data + firstFrame * blockAlign, size_t(frames) * blockAlign);
                } else {
                    memset(out, 0, size_t(frames) * blockAlign);
                }
                buffer->Unlock();
                hr = buffer->SetCurrentLength(DWORD(frames * blockAlign));
            }
            if (SUCCEEDED(hr)) {
                hr = sample->SetSampleTime(m_audioWritten * kSecond / m_audio.rate);
            }
            if (SUCCEEDED(hr)) {
                hr = sample->SetSampleDuration((m_audioWritten + frames) * kSecond / m_audio.rate - m_audioWritten * kSecond / m_audio.rate);
            }
            if (SUCCEEDED(hr)) {
                hr = m_writer->WriteSample(m_audioOut, sample);
            }
            if (SUCCEEDED(hr)) {
                m_audioWritten += frames;
            }
            return hr;
        }

        HRESULT CExporter::WriteSilence(int64_t frames)
        {
            HRESULT hr = S_OK;
            while (SUCCEEDED(hr) && frames > 0) {
                const int64_t chunk = std::min<int64_t>(frames, m_audio.rate / 10);
                hr = WritePcm(nullptr, 0, chunk);
                frames -= chunk;
            }
            return hr;
        }

        // Places a decoded block on the output sample grid. Gaps, including a track that starts
        // after the first frame, become silence so that audio stays locked to the video timeline.
        HRESULT CExporter::WriteAudioBlock(int64_t time, const std::vector<BYTE>& pcm, int64_t limit)
        {
            const int64_t frames = int64_t(pcm.size() / (m_audio.channels * 2));
            auto toFrames = [&](int64_t t) {
                const int64_t scaled = (t - m_firstVideo) * m_audio.rate;
                return scaled >= 0 ? (scaled + kSecond / 2) / kSecond : -((-scaled + kSecond / 2) / kSecond);
            };
            const int64_t blockStart = toFrames(time);
            const int64_t begin = std::max(blockStart, m_audioWritten);
            const int64_t end = std::min(blockStart + frames, toFrames(limit));
            if (begin >= end) {
                return S_OK;
            }
            HRESULT hr = WriteSilence(begin - m_audioWritten);
            if (SUCCEEDED(hr)) {
                hr = WritePcm(pcm.data(), begin - blockStart, end - begin);
            }
            return hr;
        }

        Status CExporter::ResetWriter()
        {
            m_writer.Release();
            if (m_tempStream) {
                m_tempStream->Close();
                m_tempStream.Release();
            }
            if (!m_tempPath.empty()) {
                DeleteFileW(m_tempPath.c_str());
                m_tempPath.clear();
            }
            HRESULT hr = CreateTempFile(m_request.destination, m_tempPath, m_tempStream);
            if (SUCCEEDED(hr)) {
                hr = CreateWriter(m_tempStream, m_writer);
            }
            return SUCCEEDED(hr) ? Status::Ok : Fail(hr, Status::WriteFailed);
        }

        Status CExporter::RunPrecise()
        {
            // Decoding needs a reader of its own; the compressed one only resolved the offset.
            m_reader.Release();
            m_streams = Streams();
            HRESULT hr = CreateReader(m_request.source, true, m_reader);
            if (SUCCEEDED(hr)) {
                hr = SelectStreams(m_reader, m_streams);
            }
            if (FAILED(hr)) {
                return Fail(hr, Status::SourceUnreadable);
            }
            const bool hasAudio = m_streams.audio != kNoStream;
            if (!HasEncoder(MFT_CATEGORY_VIDEO_ENCODER, MFMediaType_Video, MFVideoFormat_H264)
                    || (hasAudio && !HasEncoder(MFT_CATEGORY_AUDIO_ENCODER, MFMediaType_Audio, MFAudioFormat_AAC))) {
                return Status::EncoderMissing;
            }

            VideoFormat video;
            if (FAILED(hr = ConfigureVideoDecoding(m_reader, m_streams, video))) {
                return Fail(hr, Status::PreciseVideo);
            }
            if (hasAudio && FAILED(hr = ConfigureAudioDecoding(m_reader, m_streams, m_audio))) {
                return Fail(hr, Status::PreciseAudio);
            }

            const UINT32 bitrate = ChooseVideoBitrate(m_reader, m_streams, video);
            if (!HasFreeSpace(m_request.destination, uint64_t(bitrate + 192000) / 8 * uint64_t(m_end - m_start) / kSecond)) {
                return Status::DiskFull;
            }

            // High profile needs Windows 8 or later. A rejected stream cannot be removed from
            // a sink writer, so the Main profile fallback starts over with a new one.
            hr = AddVideoEncoderStream(m_writer, bitrate, eAVEncH264VProfile_High, m_streams, video, m_videoOut);
            if (FAILED(hr)) {
                const Status reset = ResetWriter();
                if (reset != Status::Ok) {
                    return reset;
                }
                hr = AddVideoEncoderStream(m_writer, bitrate, eAVEncH264VProfile_Main, m_streams, video, m_videoOut);
            }
            if (SUCCEEDED(hr) && hasAudio) {
                hr = AddAudioEncoderStream(m_writer, m_audio, m_audioOut);
            }
            if (FAILED(hr)) {
                return Fail(hr, Status::EncoderMissing);
            }
            if (FAILED(hr = m_writer->BeginWriting()) || FAILED(hr = Seek(m_reader, m_start - kSeekPreroll))) {
                return Fail(hr, Status::WriteFailed);
            }
            m_progressBase = m_start;

            struct PendingFrame {
                CComPtr<IMFSample> sample;
                int64_t time = 0;
                int64_t duration = 0;
            };
            struct AudioBlock {
                int64_t time = 0;
                int64_t end = 0;
                std::vector<BYTE> pcm;
            };
            std::optional<PendingFrame> pending;
            std::deque<AudioBlock> heldAudio;
            bool started = false, videoDone = false;
            bool audioEnded = !hasAudio, audioDone = !hasAudio;
            int64_t videoEnd = 0;
            int64_t skippedUntil = INT64_MIN;   // end of the last frame rejected before the start
            const int64_t nominalFrame = std::max<int64_t>(int64_t(video.fpsDen) * kSecond / video.fpsNum, 1);

            // A frame is kept when its display interval [time, next) overlaps [start, end).
            auto decide = [&](const PendingFrame& frame, int64_t next) -> HRESULT {
                if (frame.time >= m_end) {
                    videoDone = true;
                    return S_OK;
                }
                if (next <= m_start) {
                    skippedUntil = next;
                    return S_OK;
                }
                if (!started) {
                    started = true;
                    m_firstVideo = frame.time;
                }
                CComPtr<IMFSample> packed;
                HRESULT hr = PackFrame(frame.sample, video, packed);
                if (SUCCEEDED(hr)) {
                    hr = packed->SetSampleTime(frame.time - m_firstVideo);
                }
                if (SUCCEEDED(hr)) {
                    hr = packed->SetSampleDuration(next - frame.time);
                }
                if (SUCCEEDED(hr)) {
                    hr = m_writer->WriteSample(m_videoOut, packed);
                }
                videoEnd = next;
                ReportProgress(frame.time);
                return hr;
            };

            // Audio is placed once the first frame has fixed the output origin; audio past the
            // requested end waits until the last frame has fixed the output end.
            auto flushAudio = [&]() -> HRESULT {
                while (!started && !heldAudio.empty() && heldAudio.front().end <= skippedUntil) {
                    heldAudio.pop_front();
                }
                HRESULT hr = S_OK;
                while (started && !heldAudio.empty() && (videoDone || heldAudio.front().end <= m_end)) {
                    const AudioBlock& block = heldAudio.front();
                    if (videoDone && block.time >= videoEnd) {
                        audioDone = true;
                        heldAudio.clear();
                        break;
                    }
                    if (FAILED(hr = WriteAudioBlock(block.time, block.pcm, videoDone ? videoEnd : m_end))) {
                        break;
                    }
                    heldAudio.pop_front();
                }
                if (videoDone && (audioEnded || audioDone)) {
                    audioDone = true;
                }
                return hr;
            };

            while (!(videoDone && audioDone)) {
                if (m_cancel) {
                    return Status::Cancelled;
                }
                DWORD index = 0, flags = 0;
                LONGLONG time = 0;
                CComPtr<IMFSample> sample;
                hr = m_reader->ReadSample(DWORD(MF_SOURCE_READER_ANY_STREAM), 0, &index, &flags, &time, &sample);
                if (FAILED(hr)) {
                    return Fail(hr, Status::SourceUnreadable);
                }
                if (flags & MF_SOURCE_READERF_CURRENTMEDIATYPECHANGED) {
                    const Status changed = OnFormatChanged(index, video);
                    if (changed != Status::Ok) {
                        return changed;
                    }
                }
                const bool endOfStream = (flags & MF_SOURCE_READERF_ENDOFSTREAM) != 0;

                if (index == m_streams.video && !videoDone) {
                    if (endOfStream) {
                        if (pending) {
                            hr = decide(*pending, pending->time + (pending->duration > 0 ? pending->duration : nominalFrame));
                        }
                        videoDone = true;
                    } else if (sample && (!pending || time > pending->time)) {
                        LONGLONG length = 0;
                        sample->GetSampleDuration(&length);
                        if (pending) {
                            hr = decide(*pending, time);
                        }
                        pending = PendingFrame{ sample, time, length };
                    }
                    if (videoDone) {
                        pending.reset();
                        m_reader->SetStreamSelection(m_streams.video, FALSE);
                    }
                } else if (index == m_streams.audio && !audioEnded) {
                    if (endOfStream) {
                        audioEnded = true;
                    } else if (sample) {
                        CComPtr<IMFMediaBuffer> buffer;
                        BYTE* data = nullptr;
                        DWORD length = 0;
                        if (SUCCEEDED(hr = sample->ConvertToContiguousBuffer(&buffer)) && SUCCEEDED(hr = buffer->Lock(&data, nullptr, &length))) {
                            const DWORD blockAlign = m_audio.channels * 2;
                            const int64_t frames = length / blockAlign;
                            heldAudio.push_back(AudioBlock{ time, time + frames * kSecond / m_audio.rate,
                                                            std::vector<BYTE>(data, data + frames * blockAlign) });
                            buffer->Unlock();
                        }
                    }
                }
                if (SUCCEEDED(hr)) {
                    hr = flushAudio();
                }
                if (FAILED(hr)) {
                    return Fail(hr, Status::WriteFailed);
                }
                if (audioDone && hasAudio && !audioEnded) {
                    audioEnded = true;
                    m_reader->SetStreamSelection(m_streams.audio, FALSE);
                }
            }

            if (!started) {
                return Status::EmptyRange;
            }
            hr = m_writer->Finalize();
            if (FAILED(hr)) {
                return Fail(hr, Status::WriteFailed);
            }
            m_result.effectiveStart = m_firstVideo - m_offset;
            m_result.effectiveEnd = videoEnd - m_offset;
            return Status::Ok;
        }

        // Decoders may report the real stream format only with their first output.
        Status CExporter::OnFormatChanged(DWORD index, VideoFormat& video)
        {
            if (index == m_streams.video) {
                CComPtr<IMFMediaType> current;
                VideoFormat updated;
                if (FAILED(m_reader->GetCurrentMediaType(index, &current))
                        || FAILED(ReadVideoFormat(current, m_streams.videoType, updated))
                        || updated.width != video.width || updated.height != video.height) {
                    return Status::PreciseVideo;
                }
                video = updated;
            } else if (index == m_streams.audio) {
                CComPtr<IMFMediaType> current;
                if (FAILED(m_reader->GetCurrentMediaType(index, &current))
                        || MFGetAttributeUINT32(current, MF_MT_AUDIO_SAMPLES_PER_SECOND, 0) != m_audio.rate
                        || MFGetAttributeUINT32(current, MF_MT_AUDIO_NUM_CHANNELS, 0) != m_audio.channels
                        || MFGetAttributeUINT32(current, MF_MT_AUDIO_BITS_PER_SAMPLE, 0) != 16) {
                    return Status::PreciseAudio;
                }
            }
            return Status::Ok;
        }

        Status CExporter::Publish(bool expectAudio)
        {
            if (m_cancel) {
                return Status::Cancelled;
            }
            // Finalize has already closed the byte stream through the media sink.
            m_writer.Release();
            m_reader.Release();
            m_tempStream->Close();
            m_tempStream.Release();
            if (m_edit) {
                const HRESULT hr = ApplyEditList(m_tempPath, m_edit->first, m_edit->second);
                if (FAILED(hr)) {
                    return Fail(hr, Status::WriteFailed);
                }
            }
            if (m_cancel) {
                return Status::Cancelled;
            }
            const bool valid = ValidateOutput(m_tempPath, expectAudio, m_result.effectiveEnd - m_result.effectiveStart);
            if (m_cancel) {
                return Status::Cancelled;
            }
            if (!valid) {
                return Status::ValidationFailed;
            }
            const Status destination = CheckDestination(m_request);
            if (destination != Status::Ok) {
                return destination;
            }
            const DWORD flags = MOVEFILE_WRITE_THROUGH | (m_request.replaceExisting ? MOVEFILE_REPLACE_EXISTING : 0);
            if (m_cancel) {
                return Status::Cancelled;
            }
            if (!MoveFileExW(m_tempPath.c_str(), m_request.destination.c_str(), flags)) {
                const DWORD error = GetLastError();
                return error == ERROR_ALREADY_EXISTS || error == ERROR_FILE_EXISTS
                       ? Status::DestinationExists : Fail(HRESULT_FROM_WIN32(error), Status::WriteFailed);
            }
            m_tempPath.clear();
            return Status::Ok;
        }
    }

    CJob::CJob(const Request& request, HWND notifyWnd, UINT message)
        : m_request(request)
        , m_notifyWnd(notifyWnd)
        , m_message(message)
    {
        m_thread = std::thread(&CJob::Run, this);
    }

    CJob::~CJob()
    {
        Cancel();
        Wait();
    }

    void CJob::Wait()
    {
        if (m_thread.joinable()) {
            m_thread.join();
        }
    }

    void CJob::Run()
    {
        if (!IsAvailable()) {
            m_result.status = Status::MediaFoundationMissing;
        } else {
            CMFSession session;
            if (FAILED(session.GetHResult())) {
                m_result.status = Status::MediaFoundationMissing;
                m_result.hr = session.GetHResult();
            } else {
                try {
                    CExporter exporter(m_request, m_cancel, m_notifyWnd, m_message);
                    m_result = exporter.Run();
                } catch (const std::bad_alloc&) {
                    m_result.status = Status::Unexpected;
                    m_result.hr = E_OUTOFMEMORY;
                }
            }
        }
        PostMessage(m_notifyWnd, m_message, kDone, 0);
    }
}
