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
#include "CastTarget.h"
#include "MediaInfo/MediaInfoDLL.h"
#include <mfapi.h>
#include <mfidl.h>
#include <mfreadwrite.h>
#include <mferror.h>
#include <cmath>
#pragma comment(lib, "mfplat.lib")
#pragma comment(lib, "mfreadwrite.lib")
#pragma comment(lib, "mfuuid.lib")

// MediaInfo is what a file about to be cast is read with. It is an external
// library, loaded at run time, that the installer and the archive both ship
// beside the player, so it is there in every build the user is handed; a
// source build without it simply cannot cast, and says so.

namespace
{
    // Numeric fields come back as decimal text, and a stream that has more
    // than one value for one of them lists them all ("48000 / 24000"), the
    // first being the one that is played. Anything else, including a field
    // the file does not have, reads as zero.
    int MediaInfoInt(const CString& value)
    {
        int n = 0;
        for (int i = 0; i < value.GetLength() && value[i] >= _T('0') && value[i] <= _T('9'); i++) {
            n = n * 10 + (value[i] - _T('0'));
        }
        return n;
    }
}

bool CastMediaInfoAvailable()
{
    MediaInfoDLL::MediaInfo mi;
    return mi.IsReady();
}

// Decodes the first audio stream to float PCM with the Media Foundation Source
// Reader -- no DirectShow graph, no LAV, nothing to register -- and reports
// whether the first few seconds carry real signal. A codec the system has no
// decoder for comes back NotDecodable, which is the truthful "we cannot check
// this here" rather than a claim either way. Validated against ffmpeg's
// volumedetect: real audio peaks near 0 dB, silence sits at the sample floor.
static void MeasureCastAudioLevel(const CString& path, CastMediaInfo& info)
{
    // Balanced with the MFShutdown below; harmless if MF is already started
    // elsewhere in the process, since the count is what is tracked.
    if (FAILED(MFStartup(MF_VERSION, MFSTARTUP_LITE))) {
        return;
    }
    struct MFScope { ~MFScope() { MFShutdown(); } } mfScope;

    const DWORD firstAudio = (DWORD)MF_SOURCE_READER_FIRST_AUDIO_STREAM;
    const DWORD allStreams = (DWORD)MF_SOURCE_READER_ALL_STREAMS;

    IMFSourceReader* reader = nullptr;
    if (FAILED(MFCreateSourceReaderFromURL(path, nullptr, &reader)) || !reader) {
        info.audioCheck = CastMediaInfo::AudioCheck::NotDecodable;
        return;
    }

    reader->SetStreamSelection(allStreams, FALSE);
    reader->SetStreamSelection(firstAudio, TRUE);

    IMFMediaType* nativeType = nullptr;
    if (FAILED(reader->GetNativeMediaType(firstAudio, 0, &nativeType))) {
        info.audioCheck = CastMediaInfo::AudioCheck::NoTrack;
        reader->Release();
        return;
    }
    nativeType->Release();

    IMFMediaType* pcm = nullptr;
    MFCreateMediaType(&pcm);
    pcm->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Audio);
    pcm->SetGUID(MF_MT_SUBTYPE, MFAudioFormat_Float);
    const HRESULT hrType = reader->SetCurrentMediaType(firstAudio, nullptr, pcm);
    pcm->Release();
    if (FAILED(hrType)) {
        info.audioCheck = CastMediaInfo::AudioCheck::NotDecodable;
        reader->Release();
        return;
    }

    double peak = 0.0;
    const LONGLONG limit = 30000000LL; // three seconds of the stream, in 100 ns
    for (;;) {
        DWORD flags = 0;
        LONGLONG ts = 0;
        IMFSample* sample = nullptr;
        if (FAILED(reader->ReadSample(firstAudio, 0, nullptr, &flags, &ts, &sample))) {
            break;
        }
        if (flags & MF_SOURCE_READERF_ENDOFSTREAM) {
            if (sample) {
                sample->Release();
            }
            break;
        }
        if (sample) {
            IMFMediaBuffer* buf = nullptr;
            if (SUCCEEDED(sample->ConvertToContiguousBuffer(&buf)) && buf) {
                BYTE* data = nullptr;
                DWORD len = 0;
                if (SUCCEEDED(buf->Lock(&data, nullptr, &len))) {
                    const float* f = reinterpret_cast<const float*>(data);
                    for (size_t i = 0, n = len / sizeof(float); i < n; i++) {
                        const double a = std::fabs((double)f[i]);
                        if (a > peak) {
                            peak = a;
                        }
                    }
                    buf->Unlock();
                }
                buf->Release();
            }
            sample->Release();
        }
        if (ts >= limit) {
            break;
        }
    }
    reader->Release();

    info.audioPeakDb = peak <= 1e-7 ? -144.0 : 20.0 * std::log10(peak);
    // A generous floor: real programme material peaks far above this, while a
    // dead or empty track sits at or below it.
    info.audioCheck = info.audioPeakDb > -60.0
                      ? CastMediaInfo::AudioCheck::RealSignal
                      : CastMediaInfo::AudioCheck::Silent;
}

CastMediaInfo GetCastMediaInfo(const CString& path)
{
    CastMediaInfo info;

    MediaInfoDLL::MediaInfo mi;
    MediaInfoDLL::String file = path;
    if (!mi.IsReady() || mi.Open(file) == 0) {
        return info;
    }

    auto field = [&mi](MediaInfoDLL::stream_t stream, const MediaInfoDLL::String & name) {
        return CString(mi.Get(stream, 0, name).c_str());
    };

    // Only the codecs a DLNA profile is ever named for, or a Chromecast is
    // judged on, are recognized; everything else is left Unknown, which is
    // what keeps a renderer from being handed a profile name the stream does
    // not live up to, and a Chromecast from being refused a file over a codec
    // nothing is actually known about.
    const CString videoFormat = field(MediaInfoDLL::Stream_Video, __T("Format"));
    if (videoFormat.CompareNoCase(_T("AVC")) == 0) {
        info.video = CastMediaInfo::Video::H264;
    } else if (videoFormat.CompareNoCase(_T("HEVC")) == 0) {
        info.video = CastMediaInfo::Video::HEVC;
    } else if (videoFormat.CompareNoCase(_T("VP8")) == 0) {
        info.video = CastMediaInfo::Video::VP8;
    } else if (videoFormat.CompareNoCase(_T("VP9")) == 0) {
        info.video = CastMediaInfo::Video::VP9;
    } else if (videoFormat.CompareNoCase(_T("AV1")) == 0) {
        info.video = CastMediaInfo::Video::AV1;
    } else if (videoFormat.CompareNoCase(_T("MPEG Video")) == 0
               && field(MediaInfoDLL::Stream_Video, __T("Format_Version")).CompareNoCase(_T("Version 2")) == 0) {
        info.video = CastMediaInfo::Video::MPEG2;
    }
    if (info.video != CastMediaInfo::Video::Unknown) {
        info.width = MediaInfoInt(field(MediaInfoDLL::Stream_Video, __T("Width")));
        info.height = MediaInfoInt(field(MediaInfoDLL::Stream_Video, __T("Height")));
        info.videoBitDepth = MediaInfoInt(field(MediaInfoDLL::Stream_Video, __T("BitDepth")));
        info.videoProfile = field(MediaInfoDLL::Stream_Video, __T("Format_Profile"));
    }

    const CString audioFormat = field(MediaInfoDLL::Stream_Audio, __T("Format"));
    if (audioFormat.Left(3).CompareNoCase(_T("AAC")) == 0) {
        // the plain name, and the "AAC LC" an older library gives the same codec
        info.audio = CastMediaInfo::Audio::AAC;
    } else if (audioFormat.CompareNoCase(_T("MPEG Audio")) == 0
               && field(MediaInfoDLL::Stream_Audio, __T("Format_Profile")).CompareNoCase(_T("Layer 3")) == 0) {
        info.audio = CastMediaInfo::Audio::MP3;
    } else if (audioFormat.CompareNoCase(_T("WMA")) == 0) {
        // WMAFULL covers plain WMA; Pro and Lossless are a different codec with
        // no profile of ours, and MediaInfo names the three apart
        const CString profile = field(MediaInfoDLL::Stream_Audio, __T("Format_Profile"));
        if (profile.CompareNoCase(_T("Pro")) != 0 && profile.CompareNoCase(_T("Lossless")) != 0) {
            info.audio = CastMediaInfo::Audio::WMA;
        }
    } else if (audioFormat.CompareNoCase(_T("FLAC")) == 0) {
        info.audio = CastMediaInfo::Audio::FLAC;
    } else if (audioFormat.CompareNoCase(_T("Opus")) == 0) {
        info.audio = CastMediaInfo::Audio::Opus;
    } else if (audioFormat.CompareNoCase(_T("Vorbis")) == 0) {
        info.audio = CastMediaInfo::Audio::Vorbis;
    } else if (audioFormat.CompareNoCase(_T("PCM")) == 0) {
        info.audio = CastMediaInfo::Audio::LPCM;
    } else if (audioFormat.Left(6).CompareNoCase(_T("E-AC-3")) == 0) {
        // "E-AC-3 JOC" is Atmos, which the same decoder is asked for
        info.audio = CastMediaInfo::Audio::EAC3;
    } else if (audioFormat.Left(4).CompareNoCase(_T("AC-3")) == 0) {
        info.audio = CastMediaInfo::Audio::AC3;
    } else if (audioFormat.Left(3).CompareNoCase(_T("DTS")) == 0) {
        // the core and every DTS-HD extension carry the one format name
        info.audio = CastMediaInfo::Audio::DTS;
    } else if (audioFormat.Left(3).CompareNoCase(_T("MLP")) == 0
               || audioFormat.Find(_T("TrueHD")) >= 0) {
        // "MLP FBA" is what a current library calls TrueHD; an older one
        // spells the name out
        info.audio = CastMediaInfo::Audio::TrueHD;
    }
    if (info.audio != CastMediaInfo::Audio::Unknown) {
        info.sampleRate = MediaInfoInt(field(MediaInfoDLL::Stream_Audio, __T("SamplingRate")));
        info.channels = MediaInfoInt(field(MediaInfoDLL::Stream_Audio, __T("Channel(s)")));
        // The commercial name carries the DTS-HD MA / DTS:X / Atmos wording a
        // renderer or a device support list is described in; the plain profile
        // is the fallback when there is no commercial name.
        const CString commercial = field(MediaInfoDLL::Stream_Audio, __T("Format_Commercial_IfAny"));
        info.audioProfile = commercial.IsEmpty()
                            ? field(MediaInfoDLL::Stream_Audio, __T("Format_Profile"))
                            : commercial;
    }

    // How long the file is, which a session that never had the player's graph
    // open on it has no other way of knowing. General "Duration" is in
    // milliseconds.
    info.durationSec = MediaInfoInt(field(MediaInfoDLL::Stream_General, __T("Duration"))) / 1000.0;

    mi.Close();

    // Decode a few seconds of the audio here to see whether it is real or dead,
    // but only when there is an audio track: a video-only file has nothing to
    // check, and this is a decode we would rather not spend otherwise.
    const bool hasAudioTrack = info.audio != CastMediaInfo::Audio::Unknown || info.channels > 0
                               || !field(MediaInfoDLL::Stream_Audio, __T("Format")).IsEmpty();
    if (hasAudioTrack) {
        MeasureCastAudioLevel(path, info);
    } else {
        info.audioCheck = CastMediaInfo::AudioCheck::NoTrack;
    }

    TRACE(_T("CastMediaInfo: video %d %dx%d %d-bit, audio %d %d Hz %d ch\n"), (int)info.video,
          info.width, info.height, info.videoBitDepth, (int)info.audio, info.sampleRate, info.channels);
    return info;
}

bool CCastTarget::AcquireDiscovery()
{
    // Started even when it is already running: one protocol may have failed to
    // start where the other did, and starting again is what retries it.
    if (!StartDiscovery()) {
        return false; // nothing runs, so there is nothing to hold or release
    }
    m_discoveryHolders++;
    return true;
}

void CCastTarget::ReleaseDiscovery()
{
    ASSERT(m_discoveryHolders > 0);
    if (m_discoveryHolders > 0 && --m_discoveryHolders == 0) {
        StopDiscovery();
    }
}
