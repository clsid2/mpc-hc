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

    // Only the codecs a DLNA profile is ever named for are recognized;
    // everything else is left Unknown, which is what keeps a renderer from
    // being handed a profile name the stream does not live up to.
    const CString videoFormat = field(MediaInfoDLL::Stream_Video, __T("Format"));
    if (videoFormat.CompareNoCase(_T("AVC")) == 0) {
        info.video = CastMediaInfo::Video::H264;
    } else if (videoFormat.CompareNoCase(_T("MPEG Video")) == 0
               && field(MediaInfoDLL::Stream_Video, __T("Format_Version")).CompareNoCase(_T("Version 2")) == 0) {
        info.video = CastMediaInfo::Video::MPEG2;
    }
    if (info.video != CastMediaInfo::Video::Unknown) {
        info.width = MediaInfoInt(field(MediaInfoDLL::Stream_Video, __T("Width")));
        info.height = MediaInfoInt(field(MediaInfoDLL::Stream_Video, __T("Height")));
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
    }
    if (info.audio != CastMediaInfo::Audio::Unknown) {
        info.sampleRate = MediaInfoInt(field(MediaInfoDLL::Stream_Audio, __T("SamplingRate")));
        info.channels = MediaInfoInt(field(MediaInfoDLL::Stream_Audio, __T("Channel(s)")));
    }

    mi.Close();

    TRACE(_T("CastMediaInfo: video %d %dx%d, audio %d %d Hz %d ch\n"), (int)info.video,
          info.width, info.height, (int)info.audio, info.sampleRate, info.channels);
    return info;
}
