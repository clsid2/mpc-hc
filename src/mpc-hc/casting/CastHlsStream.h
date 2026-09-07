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

#include <atomic>
#include <map>
#include <memory>
#include <mutex>
#include <vector>

#include "CastTarget.h"

class CCastMediaServer;

// Serving a still-transcoding file to a cast device as HLS: one worker drives
// Media Foundation's fragmented-MP4 sink into a temp file while a segmenter
// tails that file and publishes the playlist, the init segment and ~6 s media
// segments as server resources, so playback starts before the transcode ends.
// The media server's HLS resource table is typed in terms of Part below.
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

    static constexpr double kSegmentSec = 6.0;

    // What Run() posts to the window Start() was given, as the wParam: Ready
    // once the init segment and the first segments are up (worth loading),
    // Failed when the transcode or the segmenter gave up, Complete when the
    // whole file has been produced and served.
    enum Notify {
        NotifyReady,
        NotifyFailed,
        NotifyComplete,
    };

    bool Start(const CString& srcPath, const CastMediaInfo& info, int targetChannels,
               double durationSec, CCastMediaServer& server, HWND hNotify, UINT notifyMsg);
    void Abort();                    // signal cancel, join worker (bounded), delete temps
    bool IsRunning() const;
    bool Failed() const;
    CString FailureReason() const;
    double ProducedSeconds() const;  // end of the last published segment
    CStringA PlaylistText() const;   // built once in Start(): the full VOD playlist

    CCastHlsStream(); // the deleted copy constructor suppresses the implicit default one
    ~CCastHlsStream();

private:
    CCastHlsStream(const CCastHlsStream&) = delete;
    CCastHlsStream& operator=(const CCastHlsStream&) = delete;

    // One track of the movie header: what the segmenter needs to place and
    // time fragments. decodeTime runs in the track's timescale units.
    struct TrackState {
        UINT32 timescale = 0;
        bool isVideo = false;
        ULONGLONG decodeTime = 0;
    };

    static DWORD WINAPI StaticThreadProc(LPVOID lpParam);
    DWORD Run();
    bool Poll();                     // parse whatever landed in the raw file since last time
    void Finish(bool ok, const CString& why);
    void CloseOpenSegment();         // publish the segments-parts accumulated so far
    void CheckReady();               // post NotifyReady once init + first segments are up
    bool FailParse(const CString& why); // remember the reason, return false for Poll()
    bool ReadAt(ULONGLONG offset, ULONG len, BYTE* buf);
    bool ParseMoovTracks(const BYTE* moov, ULONGLONG moovSize);
    CStringA BuildPlaylist() const;

    // Set once by Start(), read-only afterwards.
    CString m_srcPath;
    CastMediaInfo m_info;
    int m_targetChannels = 2;
    double m_durationSec = 0.0;
    CCastMediaServer* m_server = nullptr;
    HWND m_hNotify = nullptr;
    UINT m_notifyMsg = 0;
    CString m_rawPath;               // the growing fragmented MP4
    CStringA m_playlist;             // the full VOD playlist, published in Start()

    // Worker plumbing.
    HANDLE m_hThread = nullptr;
    HANDLE m_hCancel = nullptr;      // manual-reset; set by Abort()
    std::atomic<bool> m_running{ false };

    // Shared outcome, guarded by m_stateMutex.
    mutable std::mutex m_stateMutex;
    bool m_failed = false;
    CString m_failReason;
    double m_producedSec = 0.0;

    // Segmenter state; touched only on the worker thread (or the Start caller
    // before the thread exists).
    HANDLE m_hFile = nullptr;        // read handle onto m_rawPath, opened lazily
    ULONGLONG m_parseOffset = 0;     // next top-level box to consider
    std::vector<BYTE> m_ftyp;        // kept for the init segment
    std::map<UINT, TrackState> m_tracks;
    UINT m_videoTrack = 0;
    bool m_initPublished = false;
    bool m_readyPosted = false;
    int m_segmentCount = 0;          // windows the playlist announced
    std::vector<bool> m_segmentDone; // published or explicitly failed
    int m_publishedSegments = 0;
    int m_openIndex = -1;            // the segment now accumulating fragments
    std::vector<Part> m_openParts;
    ULONGLONG m_openBytes = 0;
    double m_openStartSec = 0.0;
    double m_openDurSec = 0.0;
};
