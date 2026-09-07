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

#pragma once

#include <memory>
#include <vector>

class CCastMediaServer;
struct CastMediaInfo;

// Serving a still-transcoding file to a cast device as HLS: one worker drives
// Media Foundation's fragmented-MP4 sink into a temp file while a segmenter
// tails that file and publishes the playlist, the init segment and ~6 s media
// segments as server resources, so playback starts before the transcode ends.
// The media server's HLS resource table is typed in terms of Part below, which
// is why this header exists before the engine does; the class itself is
// implemented with the engine in a later phase.
class CCastHlsStream
{
public:
    // One piece of a published resource's body. A set blob is served from
    // memory; otherwise the bytes are len bytes at fileOffset of the raw
    // fragmented MP4 the stream is writing. Segments are a patched moof from
    // memory followed by file ranges of untouched mdat payload, so the movie
    // is never copied a second time just to serve it.
    struct Part {
        std::shared_ptr<std::vector<BYTE>> blob;
        ULONGLONG fileOffset = 0;
        ULONGLONG len = 0;
    };

    // The fragments whose video starts inside one kSegmentSec window: closed
    // and published as a unit when the next window opens.
    struct Segment {
        std::vector<Part> parts;
        ULONGLONG bytes = 0;
        double startSec = 0.0;
        double durSec = 0.0;
        bool published = false;
    };

    static constexpr double kSegmentSec = 6.0;

    bool Start(const CString& srcPath, const CastMediaInfo& info, int targetChannels,
               double durationSec, CCastMediaServer& server, HWND hNotify, UINT notifyMsg);
    void Abort();                    // signal cancel, join worker (bounded), delete temps
    bool IsRunning() const;
    bool Failed() const;
    CString FailureReason() const;
    double ProducedSeconds() const;  // end of the last published segment
    CStringA PlaylistText() const;   // built once in Start(): the full VOD playlist

private:
    CCastHlsStream(const CCastHlsStream&) = delete;
    CCastHlsStream& operator=(const CCastHlsStream&) = delete;

    static DWORD WINAPI StaticThreadProc(LPVOID lpParam);
    DWORD Run();
    bool Poll();                     // parse whatever landed in the raw file since last time
};
