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
#include <cstdint>
#include <string>
#include <thread>

// Clip export through Media Foundation. It reads the source file on its own and never
// touches the playback graph. All times are timeline times in 100 ns units; the
// mapping to source timestamps is resolved here.
namespace TrimExport
{
    enum class Mode {
        Fast,       // stream copy from the keyframe at or before the start
        Precise,    // decode and re-encode to H.264/AAC, frame-exact boundaries
    };

    enum class Status {
        Ok,
        Cancelled,
        MediaFoundationMissing,
        SourceUnreadable,
        NoVideo,
        FastContainer,
        FastVideoCodec,
        FastAudioCodec,
        PreciseVideo,
        PreciseAudio,
        EncoderMissing,
        EmptyRange,
        DestinationExists,
        DestinationIsSource,
        AccessDenied,
        DiskFull,
        WriteFailed,
        ValidationFailed,
        Unexpected,
    };

    struct SourceInfo {
        Status status = Status::Unexpected;
        HRESULT hr = S_OK;
        Status fast = Status::Unexpected;       // Ok when Fast Export can be used
        Status precise = Status::Unexpected;    // Ok when Precise Export can be used
        bool hasAudio = false;
        int audioStreams = 0;
        int64_t fastStart = 0;                  // keyframe Fast Export starts from
    };

    struct Request {
        std::wstring source;
        std::wstring destination;
        int64_t start = 0;
        int64_t end = 0;
        Mode mode = Mode::Fast;
        bool replaceExisting = false;
    };

    struct Result {
        Status status = Status::Unexpected;
        HRESULT hr = S_OK;
        int64_t effectiveStart = 0;
        int64_t effectiveEnd = 0;
    };

    bool IsAvailable();

    // Blocking; call it off the UI thread.
    SourceInfo Probe(const std::wstring& source, int64_t start);

    bool IsSameFile(const std::wstring& a, const std::wstring& b);

    // One export on a worker thread. Progress is posted to the window as `message` with
    // wParam in permille, then completion with wParam == kDone; the result is valid from then on.
    class CJob
    {
    public:
        static constexpr WPARAM kDone = WPARAM(-1);

        CJob(const Request& request, HWND notifyWnd, UINT message);
        ~CJob();

        CJob(const CJob&) = delete;
        CJob& operator=(const CJob&) = delete;

        void Cancel() { m_cancel = true; }
        void Wait();
        const Request& GetRequest() const { return m_request; }
        const Result& GetResult() const { return m_result; }

    private:
        void Run();

        const Request m_request;
        const HWND m_notifyWnd;
        const UINT m_message;
        std::atomic<bool> m_cancel = false;
        Result m_result;
        std::thread m_thread;
    };
}
