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
#include "CastHlsStream.h"
#include "CastMediaServer.h"
#include "CastTranscoder.h"
#include "Logger.h"
#include <mfapi.h>
#include <algorithm>
#include <cmath>

#pragma comment(lib, "mfplat.lib")

namespace
{
    UINT32 Be32(const BYTE* p)
    {
        return (UINT32(p[0]) << 24) | (UINT32(p[1]) << 16) | (UINT32(p[2]) << 8) | UINT32(p[3]);
    }

    ULONGLONG Be64(const BYTE* p)
    {
        return (ULONGLONG(Be32(p)) << 32) | Be32(p + 4);
    }

    void Put32(BYTE* p, UINT32 v)
    {
        p[0] = BYTE(v >> 24);
        p[1] = BYTE(v >> 16);
        p[2] = BYTE(v >> 8);
        p[3] = BYTE(v);
    }

    void Put64(BYTE* p, ULONGLONG v)
    {
        Put32(p, UINT32(v >> 32));
        Put32(p + 4, UINT32(v));
    }

    bool IsType(const BYTE* p, const char* type)
    {
        return memcmp(p, type, 4) == 0;
    }

    // One step of a child-box walk, mirroring the Phase 0 rewriter's
    // sub_boxes(): pos starts at the parent's first child and advances over
    // whole children; a malformed child ends the walk. Offsets are relative to
    // whatever buffer base points at (a whole moov or moof held in memory).
    bool NextSubBox(const BYTE* base, ULONGLONG& pos, ULONGLONG end,
                    ULONGLONG& childOff, UINT32& childSize)
    {
        if (pos + 8 > end) {
            return false;
        }
        const UINT32 size = Be32(base + pos);
        if (size < 8 || pos + size > end) {
            return false;
        }
        childOff = pos;
        childSize = size;
        pos += size;
        return true;
    }

    // What the segmenter needs to know about one trun: where its bytes are, and
    // everything required to re-aim its data_offset once the moof is rebuilt.
    struct TrunInfo {
        ULONGLONG off = 0;          // box start, relative to the moof buffer
        UINT32 size = 0;
        ULONGLONG doffFieldOff = 0; // same space; 0 when the trun has no data_offset
        INT64 doff = 0;             // original value (signed on the wire)
        UINT64 durSum = 0;          // sum of per-sample durations, timescale units
    };

    struct TrafInfo {
        UINT track = 0;
        bool hasBase = false;       // the tfhd carried an explicit base_data_offset
        ULONGLONG base = 0;         // ... and this is it, file-absolute
        std::vector<TrunInfo> truns;
    };

    // Parses a complete moof held in memory (offsets relative to its first
    // byte), the port of the rewriter's parse_frag(). False means the box walk
    // wanted to read outside the buffer -- impossible for anything Media
    // Foundation wrote, and grounds to fail the stream rather than serve a
    // wrong byte.
    bool ParseMoof(const BYTE* base, ULONGLONG moofSize, UINT32& seq,
                   ULONGLONG& mfhdOff, std::vector<TrafInfo>& trafs)
    {
        ULONGLONG pos = 8, off = 0;
        UINT32 size = 0;
        bool mfhdSeen = false;
        while (NextSubBox(base, pos, moofSize, off, size)) {
            if (IsType(base + off + 4, "mfhd")) {
                if (off + 16 > moofSize) {
                    return false; // the sequence header is always 16 bytes
                }
                mfhdSeen = true;
                mfhdOff = off;
                seq = Be32(base + off + 12);
            } else if (IsType(base + off + 4, "traf")) {
                const ULONGLONG trafEnd = off + size;
                TrafInfo traf;
                ULONGLONG tpos = off + 8, coff = 0;
                UINT32 csize = 0;
                while (NextSubBox(base, tpos, trafEnd, coff, csize)) {
                    if (IsType(base + coff + 4, "tfhd")) {
                        const UINT32 flags = Be32(base + coff + 8) & 0xffffff;
                        // header + version/flags + track_ID, then whichever
                        // optional fields the flags announce
                        const ULONGLONG need = 16
                                              + (flags & 0x000001 ? 8 : 0)
                                              + (flags & 0x000002 ? 4 : 0)
                                              + (flags & 0x000008 ? 4 : 0)
                                              + (flags & 0x000010 ? 4 : 0)
                                              + (flags & 0x000020 ? 4 : 0);
                        if (csize < need) {
                            return false;
                        }
                        ULONGLONG r = coff + 12; // past version/flags
                        traf.track = Be32(base + r);
                        r += 4;
                        if (flags & 0x000001) {
                            traf.hasBase = true;
                            traf.base = Be64(base + r);
                            r += 8;
                        }
                    } else if (IsType(base + coff + 4, "trun")) {
                        const UINT32 flags = Be32(base + coff + 8) & 0xffffff;
                        const UINT32 count = Be32(base + coff + 12);
                        const ULONGLONG stride = 4 * (
                            + ((flags & 0x000100) ? 1 : 0)
                            + ((flags & 0x000200) ? 1 : 0)
                            + ((flags & 0x000400) ? 1 : 0)
                            + ((flags & 0x000800) ? 1 : 0));
                        if (csize < 16 + (flags & 0x000001 ? 4 : 0) + (flags & 0x000004 ? 4 : 0)
                                + ULONGLONG(count) * stride) {
                            return false;
                        }
                        TrunInfo tr;
                        tr.off = coff;
                        tr.size = csize;
                        ULONGLONG r = coff + 16;
                        if (flags & 0x000001) {
                            tr.doffFieldOff = r;
                            tr.doff = INT32(Be32(base + r)); // signed on the wire
                            r += 4;
                        }
                        for (UINT32 i = 0; i < count; i++) {
                            if (flags & 0x000100) {
                                tr.durSum += Be32(base + r);
                                r += 4;
                            }
                            if (flags & 0x000200) {
                                r += 4;
                            }
                            if (flags & 0x000400) {
                                r += 4;
                            }
                            if (flags & 0x000800) {
                                r += 4;
                            }
                        }
                        traf.truns.push_back(tr);
                    }
                }
                trafs.push_back(std::move(traf));
            }
        }
        return mfhdSeen;
    }
}

CCastHlsStream::CCastHlsStream()
{
}

DWORD WINAPI CCastHlsStream::StaticThreadProc(LPVOID lpParam)
{
    SetThreadName(DWORD(-1), "CastHlsStream Thread");
    return ((CCastHlsStream*)lpParam)->Run();
}

bool CCastHlsStream::Start(const CString& srcPath, const CastMediaInfo& info, int targetChannels,
                           double durationSec, CCastMediaServer& server, HWND hNotify, UINT notifyMsg)
{
    if (m_hThread) {
        ASSERT(FALSE); // one stream per object; Abort() before starting again
        return false;
    }
    // The fragmented engine only takes H.264 video -- its sink refuses HEVC
    // outright, and a file with no picture has nothing to show progressively.
    // Both stay on the complete-file transcode, which is the caller's fallback.
    if (info.video != CastMediaInfo::Video::H264) {
        return false;
    }
    // The playlist announces windows across the whole duration; without one
    // there is nothing honest to announce.
    m_durationSec = durationSec > 0.0 ? durationSec : info.durationSec;
    if (m_durationSec <= 0.0) {
        return false;
    }

    m_srcPath = srcPath;
    m_info = info;
    m_targetChannels = targetChannels < 1 ? 2 : targetChannels;
    m_server = &server;
    m_hNotify = hNotify;
    m_notifyMsg = notifyMsg;

    // The short tail folds into the last window rather than earning a segment
    // of its own.
    const int count = int(std::ceil(m_durationSec / kSegmentSec - 1e-9));
    m_segmentCount = std::max(1, count);
    m_segmentDone.assign(m_segmentCount, false);

    // The raw output's own name, qualified per run: a quick file switch during
    // a long transcode must not aim two workers at one temp file.
    static std::atomic<ULONG> generation(0);
    TCHAR tempDir[MAX_PATH] = { 0 };
    GetTempPath(MAX_PATH, tempDir);
    m_rawPath.Format(_T("%smpc-casthls-%u-%u.mp4"), tempDir, GetCurrentProcessId(), ++generation);

    m_playlist = BuildPlaylist();

    // Everything the device needs to know about the stream is known now, so
    // the whole VOD playlist goes up immediately; requests for segments that
    // do not exist yet park in the server instead of failing.
    server.BeginHls(m_rawPath);
    {
        auto blob = std::make_shared<std::vector<BYTE>>(m_playlist.GetString(),
                                                        m_playlist.GetString() + m_playlist.GetLength());
        Part part;
        part.blob = blob;
        part.len = blob->size();
        std::vector<Part> parts;
        parts.push_back(std::move(part));
        server.PublishResource("media.m3u8", "application/vnd.apple.mpegurl", std::move(parts), blob->size());
    }

    m_hCancel = CreateEvent(nullptr, TRUE, FALSE, nullptr);
    if (!m_hCancel) {
        server.EndHls();
        return false;
    }
    m_running = true;
    m_hThread = CreateThread(nullptr, 0, StaticThreadProc, this, 0, nullptr);
    if (!m_hThread) {
        m_running = false;
        CloseHandle(m_hCancel);
        m_hCancel = nullptr;
        server.EndHls();
        return false;
    }
    CASTING_LOG(_T("hls: streaming the transcode as %d segments of 6 s"), m_segmentCount);
    return true;
}

// The whole VOD playlist, computed up front from the known duration. The
// EXTINF values are the window lengths, not measured fragment sums: for every
// window but the last that is the same number, and the tail window carries
// whatever remains.
CStringA CCastHlsStream::BuildPlaylist() const
{
    // Fixed point, not %.3f: a locale with a comma decimal separator must not
    // leak into the manifest.
    auto extinf = [](double sec) -> CStringA {
        const int ms = int(sec * 1000.0 + (sec >= 0.0 ? 0.5 : -0.5));
        CStringA s;
        s.Format("%d.%03d,", ms / 1000, ms % 1000);
        return s;
    };
    const int targetDuration = int(std::ceil(kSegmentSec));
    CStringA text;
    text += "#EXTM3U\n";
    text += "#EXT-X-VERSION:7\n";
    text.AppendFormat("#EXT-X-TARGETDURATION:%d\n", targetDuration);
    text += "#EXT-X-MEDIA-SEQUENCE:0\n";
    text += "#EXT-X-PLAYLIST-TYPE:VOD\n";
    text += "#EXT-X-MAP:URI=\"init.mp4\"\n";
    for (int i = 0; i < m_segmentCount; i++) {
        const double sec = i + 1 < m_segmentCount
                           ? kSegmentSec
                           : m_durationSec - (m_segmentCount - 1) * kSegmentSec;
        text += "#EXTINF:" + extinf(sec);
        text.AppendFormat("\nseg%05d.m4s\n", i);
    }
    text += "#EXT-X-ENDLIST\n";
    return text;
}

DWORD CCastHlsStream::Run()
{
    // The multithreaded apartment, not an STA: the transcode pulls in Media
    // Foundation and DirectShow machinery that must never find itself in a
    // message-less single-threaded apartment.
    const HRESULT hrCo = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    const bool mf = SUCCEEDED(MFStartup(MF_VERSION));

    CastTranscodeProgress prog;
    prog.hCancel = m_hCancel;
    // The segmenter runs between batches of samples, on this thread only. A
    // parse failure cancels the transcode instead of letting it finish into a
    // file half of which can no longer be served.
    prog.onProgress = [this]() {
        if (!Poll() && m_hCancel) {
            SetEvent(m_hCancel);
        }
    };

    CString why;
    const bool transcodeOk = CastDownmixToFragmentedMp4(m_srcPath, m_info, m_targetChannels,
                                                         m_rawPath, prog, &why);
    // The pump's last look at the file was up to 32 samples before the end;
    // the file is final now, so sweep the tail once more before closing up.
    const bool pollOk = Poll();
    if (!pollOk) {
        std::lock_guard<std::mutex> lock(m_stateMutex);
        if (!m_failReason.IsEmpty()) {
            why = m_failReason;
        }
    }

    Finish(transcodeOk && pollOk, why);
    PostMessage(m_hNotify, m_notifyMsg,
                WPARAM(transcodeOk && pollOk ? NotifyComplete : NotifyFailed), 0);

    if (mf) {
        MFShutdown();
    }
    if (SUCCEEDED(hrCo)) {
        CoUninitialize();
    }
    m_running = false;
    return 0;
}

void CCastHlsStream::Finish(bool ok, const CString& why)
{
    CloseOpenSegment();
    // Whatever the playlist announced and the transcode never produced must
    // stop parking requests: a device asking for it is answered 404 now.
    int missing = 0;
    for (int i = 0; i < m_segmentCount; i++) {
        if (!m_segmentDone[i]) {
            CStringA name;
            name.Format("seg%05d.m4s", i);
            m_server->FailResource(name);
            m_segmentDone[i] = true;
            missing++;
        }
    }
    if (!m_initPublished) {
        m_server->FailResource("init.mp4");
    }
    if (ok) {
        CASTING_LOG(_T("hls: the stream is complete; %d segment(s) served"), m_publishedSegments);
    } else {
        std::lock_guard<std::mutex> lock(m_stateMutex);
        m_failed = true;
        m_failReason = why.IsEmpty() ? CString(_T("the transcoded stream stopped early")) : why;
        CASTING_LOG(_T("hls: the stream failed after %d segment(s): %s"),
                    m_publishedSegments, m_failReason.GetString());
    }
}

// Tails the raw file: every top-level box that has landed whole is consumed.
// ftyp is kept for the init segment, the moov is parsed and published as
// init.mp4, and each complete moof+mdat pair is cut into a media segment. A
// box whose bytes have not all arrived yet ends the pass; the next Poll()
// resumes at it. False means the file no longer parses -- permanent.
bool CCastHlsStream::Poll()
{
    if (!m_hFile) {
        // The file appears when the transcoder opens its output; until then
        // there is simply nothing to read. The engine writes through its own
        // handle, so sharing everything but write-claim suits both sides.
        m_hFile = CreateFile(m_rawPath, GENERIC_READ,
                             FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                             nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (m_hFile == INVALID_HANDLE_VALUE) {
            return true; // not there yet
        }
    }

    for (;;) {
        LARGE_INTEGER li{};
        if (!GetFileSizeEx(m_hFile, &li) || li.QuadPart < 0) {
            return FailParse(_T("the transcoded output could not be measured"));
        }
        const ULONGLONG fileSize = ULONGLONG(li.QuadPart);
        if (m_parseOffset + 8 > fileSize) {
            break;
        }
        BYTE hdr[16];
        if (!ReadAt(m_parseOffset, 8, hdr)) {
            return FailParse(_T("the transcoded output could not be read"));
        }
        ULONGLONG size64 = Be32(hdr);
        ULONGLONG hdrSize = 8;
        if (size64 == 1) {
            if (m_parseOffset + 16 > fileSize) {
                break;
            }
            if (!ReadAt(m_parseOffset + 8, 8, hdr + 8)) {
                return FailParse(_T("the transcoded output could not be read"));
            }
            size64 = Be64(hdr + 8);
            hdrSize = 16;
        } else if (size64 == 0) {
            size64 = fileSize - m_parseOffset; // to the end of the file
        }
        if (size64 < hdrSize || m_parseOffset + size64 > fileSize) {
            break; // the box is still arriving
        }
        const BYTE* const type = hdr + 4;
        const ULONGLONG boxOff = m_parseOffset;

        if (IsType(type, "ftyp")) {
            std::vector<BYTE> ftyp((size_t)size64);
            if (!ReadAt(boxOff, (ULONG)size64, ftyp.data())) {
                return FailParse(_T("the file type box could not be read"));
            }
            m_ftyp = std::move(ftyp);
            m_parseOffset = boxOff + size64;
        } else if (IsType(type, "moov")) {
            if (size64 > 4ull * 1024 * 1024) {
                return FailParse(_T("the movie header is implausibly large"));
            }
            std::vector<BYTE> moov((size_t)size64);
            if (!ReadAt(boxOff, (ULONG)size64, moov.data())) {
                return FailParse(_T("the movie header could not be read"));
            }
            if (!m_initPublished) {
                if (m_ftyp.empty()) {
                    return FailParse(_T("the movie header arrived before the file type box"));
                }
                if (!ParseMoovTracks(moov.data(), size64) || !m_videoTrack) {
                    return FailParse(_T("the movie header's tracks could not be parsed"));
                }
                // init.mp4 is exactly ftyp + moov, as the Phase 0 rewriter
                // emitted it; uuid/pdin simply do not appear.
                auto init = std::make_shared<std::vector<BYTE>>();
                init->reserve(m_ftyp.size() + moov.size());
                init->insert(init->end(), m_ftyp.begin(), m_ftyp.end());
                init->insert(init->end(), moov.begin(), moov.end());
                Part part;
                part.blob = init;
                part.len = init->size();
                std::vector<Part> parts;
                parts.push_back(std::move(part));
                m_server->PublishResource("init.mp4", "video/mp4", std::move(parts), init->size());
                m_initPublished = true;
                CASTING_LOG(_T("hls: published the init segment (%I64u bytes)"), (ULONGLONG)init->size());
                CheckReady();
            }
            m_parseOffset = boxOff + size64;
        } else if (IsType(type, "moof")) {
            if (size64 > 16ull * 1024 * 1024) {
                return FailParse(_T("a movie fragment is implausibly large"));
            }
            // The mdat that follows is part of the unit: nothing is published
            // until both boxes have landed whole.
            const ULONGLONG mdatOff = boxOff + size64;
            if (mdatOff + 8 > fileSize) {
                break;
            }
            BYTE mdatHdr[8];
            if (!ReadAt(mdatOff, 8, mdatHdr)) {
                return FailParse(_T("the fragment's payload could not be read"));
            }
            if (!IsType(mdatHdr + 4, "mdat")) {
                return FailParse(_T("a movie fragment was not followed by its payload"));
            }
            const ULONGLONG mdatSize = Be32(mdatHdr);
            if (mdatSize == 1) {
                return FailParse(_T("an oversized payload box is not supported"));
            }
            if (mdatSize < 8 || mdatOff + mdatSize > fileSize) {
                break; // the payload is still arriving
            }
            const ULONGLONG mdatPayload = mdatOff + 8;

            std::vector<BYTE> moofBuf((size_t)size64);
            if (!ReadAt(boxOff, (ULONG)size64, moofBuf.data())) {
                return FailParse(_T("a movie fragment could not be read"));
            }
            UINT32 seq = 0;
            ULONGLONG mfhdOff = 0;
            std::vector<TrafInfo> trafs;
            if (!ParseMoof(moofBuf.data(), size64, seq, mfhdOff, trafs) || trafs.empty()) {
                return FailParse(_T("a movie fragment could not be parsed"));
            }
            if (!m_videoTrack) {
                return FailParse(_T("a fragment arrived before the movie header was parsed"));
            }

            // --- rebuild the moof, the rewriter's surgery ---
            // Sizes first, because they do not depend on any data_offset:
            // new traf = 8 hdr + tfhd(16) + tfdt(20) + the original truns;
            // new moof = 8 hdr + mfhd(16) + the new trafs.
            ULONGLONG newMoofSize = 8 + 16;
            for (const TrafInfo& traf : trafs) {
                ULONGLONG trunBytes = 0;
                for (const TrunInfo& tr : traf.truns) {
                    trunBytes += tr.size;
                }
                newMoofSize += 8 + 16 + 20 + trunBytes;
            }
            if (newMoofSize > 0x7fffffffull) {
                return FailParse(_T("a rebuilt movie fragment is implausibly large"));
            }

            std::vector<BYTE> moofNew((size_t)newMoofSize);
            BYTE* const m = moofNew.data();
            Put32(m, UINT32(newMoofSize));
            memcpy(m + 4, "moof", 4);
            memcpy(m + 8, moofBuf.data() + mfhdOff, 16); // mfhd verbatim

            ULONGLONG videoTfdt = 0;
            bool sawVideoTraf = false;
            size_t w = 8 + 16;
            for (const TrafInfo& traf : trafs) {
                auto ti = m_tracks.find(traf.track);
                if (ti == m_tracks.end()) {
                    return FailParse(_T("a fragment names a track the movie header does not"));
                }
                // Captured before the running time advances: this is the tfdt
                // the rebuilt traf carries.
                const ULONGLONG tfdtVal = ti->second.decodeTime;
                // Where this traf's sample data starts in the file: the tfhd's
                // explicit base when it has one, else the mdat payload (the
                // general form; Media Foundation writes base == payload start).
                const ULONGLONG dataBase = traf.hasBase ? traf.base : mdatPayload;
                ULONGLONG trunBytes = 0;
                UINT64 durSum = 0;
                for (const TrunInfo& tr : traf.truns) {
                    trunBytes += tr.size;
                    durSum += tr.durSum;
                }
                const UINT32 trafSize = UINT32(8 + 16 + 20 + trunBytes);
                BYTE* const t = m + w;
                Put32(t, trafSize);
                memcpy(t + 4, "traf", 4);
                Put32(t + 8, 16);
                memcpy(t + 12, "tfhd", 4);
                Put32(t + 16, 0x020000); // version 0, default-base-is-moof
                Put32(t + 20, traf.track);
                Put32(t + 24, 20);
                memcpy(t + 28, "tfdt", 4);
                Put32(t + 32, 1u << 24); // version 1
                Put64(t + 36, tfdtVal);
                size_t q = w + 44;
                for (const TrunInfo& tr : traf.truns) {
                    memcpy(m + q, moofBuf.data() + tr.off, (size_t)tr.size);
                    if (tr.doffFieldOff) {
                        // default-base-is-moof makes the field relative to the
                        // new moof's start: add its whole size and the copied
                        // mdat header, keeping whatever shift the original
                        // absolute base had against the payload start.
                        const INT64 newDoff = INT64(newMoofSize) + 8
                                              + (INT64(dataBase) + tr.doff - INT64(mdatPayload));
                        if (newDoff < INT32_MIN || newDoff > INT32_MAX) {
                            return FailParse(_T("a fragment's sample data offset does not fit"));
                        }
                        Put32(m + q + size_t(tr.doffFieldOff - tr.off), UINT32(INT32(newDoff)));
                    }
                    q += tr.size;
                }
                if (traf.track == m_videoTrack) {
                    videoTfdt = tfdtVal;
                    sawVideoTraf = true;
                }
                ti->second.decodeTime += durSum;
                w += trafSize;
            }
            if (!sawVideoTraf) {
                return FailParse(_T("a movie fragment carries no video track run"));
            }

            // Group by the fragment's video start time: window k collects the
            // fragments whose video begins in [k*D, (k+1)*D).
            const TrackState& vts = m_tracks[m_videoTrack];
            const double vStart = double(videoTfdt) / vts.timescale;
            const double vDur = double(vts.decodeTime - videoTfdt) / vts.timescale;
            int k = int(std::floor(vStart / kSegmentSec));
            if (k < 0) {
                k = 0;
            }
            if (k >= m_segmentCount) {
                k = m_segmentCount - 1; // the tail folds into the last segment
            }

            if (m_openIndex >= 0 && k != m_openIndex) {
                if (k < m_openIndex) {
                    return FailParse(_T("a fragment's video time went backwards"));
                }
                CloseOpenSegment();
                // Windows no fragment ever started in: impossible while
                // fragments are contiguous, but the announced table stays
                // honest.
                for (int missing = m_openIndex + 1; missing < k; missing++) {
                    CStringA name;
                    name.Format("seg%05d.m4s", missing);
                    m_server->FailResource(name);
                    m_segmentDone[missing] = true;
                }
            }
            if (m_openIndex < 0) {
                m_openIndex = k;
                m_openStartSec = vStart;
                m_openDurSec = 0.0;
                m_openBytes = 0;
            }

            // The fragment itself: the patched moof from memory, the mdat box
            // header verbatim, and the payload as a range of the raw file --
            // the movie is never copied a second time just to serve it.
            Part moofPart;
            moofPart.blob = std::make_shared<std::vector<BYTE>>(std::move(moofNew));
            moofPart.len = moofPart.blob->size();
            Part hdrPart;
            hdrPart.blob = std::make_shared<std::vector<BYTE>>(mdatHdr, mdatHdr + 8);
            hdrPart.len = 8;
            Part payloadPart;
            payloadPart.fileOffset = mdatPayload;
            payloadPart.len = mdatSize - 8;
            m_openParts.push_back(std::move(moofPart));
            m_openParts.push_back(std::move(hdrPart));
            m_openParts.push_back(std::move(payloadPart));
            m_openBytes += newMoofSize + mdatSize;
            m_openDurSec += vDur;

            m_parseOffset = mdatOff + mdatSize;
        } else {
            // uuid, pdin, free, mfra, a bare mdat: nothing the segmenter needs.
            m_parseOffset = boxOff + size64;
        }
    }
    return true;
}

// Fills m_tracks/m_videoTrack from the moov, the port of the rewriter's
// parse_moov(): track ID from tkhd, timescale from mdhd, video-ness from the
// hdlr fourcc, with the version-dependent offsets both header boxes have.
bool CCastHlsStream::ParseMoovTracks(const BYTE* moov, ULONGLONG moovSize)
{
    ULONGLONG pos = 8, off = 0;
    UINT32 size = 0;
    while (NextSubBox(moov, pos, moovSize, off, size)) {
        if (!IsType(moov + off + 4, "trak")) {
            continue;
        }
        UINT trackId = 0;
        UINT32 timescale = 0;
        bool isVideo = false;
        ULONGLONG tpos = off + 8, toff = 0;
        UINT32 tsize = 0;
        while (NextSubBox(moov, tpos, off + size, toff, tsize)) {
            const BYTE* const ttype = moov + toff + 4;
            if (IsType(ttype, "tkhd")) {
                const BYTE v = moov[toff + 8];
                const ULONGLONG idOff = toff + 8 + (v ? 20 : 12);
                if (idOff + 4 <= toff + tsize) {
                    trackId = Be32(moov + idOff);
                }
            } else if (IsType(ttype, "mdia")) {
                ULONGLONG mpos = toff + 8, moff = 0;
                UINT32 msize = 0;
                while (NextSubBox(moov, mpos, toff + tsize, moff, msize)) {
                    const BYTE* const mtype = moov + moff + 4;
                    if (IsType(mtype, "mdhd")) {
                        const BYTE v = moov[moff + 8];
                        const ULONGLONG tsOff = moff + 8 + (v ? 20 : 12);
                        if (tsOff + 4 <= moff + msize) {
                            timescale = Be32(moov + tsOff);
                        }
                    } else if (IsType(mtype, "hdlr") && msize >= 20) {
                        isVideo = isVideo || IsType(moov + moff + 16, "vide");
                    }
                }
            }
        }
        if (trackId && timescale) {
            TrackState& ts = m_tracks[trackId]; // decodeTime starts at zero
            ts.timescale = timescale;
            ts.isVideo = isVideo;
            if (isVideo) {
                m_videoTrack = trackId;
            }
        }
    }
    return !m_tracks.empty();
}

void CCastHlsStream::CloseOpenSegment()
{
    if (m_openIndex < 0 || m_openParts.empty()) {
        return;
    }
    CStringA name;
    name.Format("seg%05d.m4s", m_openIndex);
    m_server->PublishResource(name, "video/iso.segment", std::move(m_openParts), m_openBytes);
    m_openParts.clear();
    m_openParts.shrink_to_fit();
    m_segmentDone[m_openIndex] = true;
    m_publishedSegments++;
    {
        std::lock_guard<std::mutex> lock(m_stateMutex);
        m_producedSec = m_openStartSec + m_openDurSec;
    }
    CASTING_LOG(_T("hls: published segment %d (%d ms of video, %I64u bytes)"),
                m_openIndex, int(m_openDurSec * 1000.0 + 0.5), m_openBytes);
    m_openIndex = -1;
    CheckReady();
}

void CCastHlsStream::CheckReady()
{
    if (m_readyPosted || !m_initPublished) {
        return;
    }
    const int needed = std::min(2, m_segmentCount);
    if (m_publishedSegments < needed) {
        return;
    }
    m_readyPosted = true;
    PostMessage(m_hNotify, m_notifyMsg, WPARAM(NotifyReady), 0);
    CASTING_LOG(_T("hls: the init segment and %d segment(s) are up; worth loading"), needed);
}

bool CCastHlsStream::FailParse(const CString& why)
{
    CASTING_LOG(_T("hls: %s"), why.GetString());
    std::lock_guard<std::mutex> lock(m_stateMutex);
    if (m_failReason.IsEmpty()) {
        m_failReason = why;
    }
    return false;
}

bool CCastHlsStream::ReadAt(ULONGLONG offset, ULONG len, BYTE* buf)
{
    LARGE_INTEGER to{};
    to.QuadPart = LONGLONG(offset);
    DWORD got = 0;
    return SetFilePointerEx(m_hFile, to, nullptr, FILE_BEGIN)
           && ReadFile(m_hFile, buf, len, &got, nullptr)
           && got == len;
}

void CCastHlsStream::Abort()
{
    if (!m_hThread) {
        return; // never started, or already aborted
    }
    if (m_hCancel) {
        SetEvent(m_hCancel);
    }
    // Bounded like the media server's Stop(): the worker checks its cancel
    // between sample batches, so it should notice almost immediately. If it
    // does not, something is wedged -- assert that in debug builds and still
    // wait, because destroying a live worker's object would be worse.
    if (WaitForSingleObject(m_hThread, 10000) != WAIT_OBJECT_0) {
        ASSERT(FALSE);
        WaitForSingleObject(m_hThread, INFINITE);
    }
    CloseHandle(m_hThread);
    m_hThread = nullptr;
    if (m_hCancel) {
        CloseHandle(m_hCancel);
        m_hCancel = nullptr;
    }
    if (m_hFile) {
        CloseHandle(m_hFile);
        m_hFile = nullptr;
    }
    // The transcoder cleans its own FLAC intermediate on every exit path; the
    // raw fragmented MP4 is this stream's to delete.
    DeleteFile(m_rawPath);
    m_running = false;
}

CCastHlsStream::~CCastHlsStream()
{
    Abort();
}

bool CCastHlsStream::IsRunning() const
{
    return m_running;
}

bool CCastHlsStream::Failed() const
{
    std::lock_guard<std::mutex> lock(m_stateMutex);
    return m_failed;
}

CString CCastHlsStream::FailureReason() const
{
    std::lock_guard<std::mutex> lock(m_stateMutex);
    return m_failReason;
}

double CCastHlsStream::ProducedSeconds() const
{
    std::lock_guard<std::mutex> lock(m_stateMutex);
    return m_producedSec;
}

CStringA CCastHlsStream::PlaylistText() const
{
    return m_playlist;
}
