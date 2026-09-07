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
#include <thread>

#include "CastTarget.h"
#include "CastDiscovery.h"
#include "CastSession.h"
#include "CastMediaServer.h"
#include "CastHlsStream.h"

// The Chromecast implementation of CCastTarget: composes the mDNS discovery,
// the CastV2 session and the local HTTP media server, and owns the glue
// between them. Raw session and discovery notifications arrive at an internal
// message-only window (all on the UI thread), where deferred work is executed
// (the LOAD once the session is up, or once a still-transcoding file's first
// segments exist; the initial seek once the device reports playback) before
// the simplified state is forwarded to the notify window.
class CChromecastTarget : public CCastTarget
{
public:
    CChromecastTarget() = default;
    ~CChromecastTarget();

    CChromecastTarget(const CChromecastTarget&) = delete;
    CChromecastTarget& operator=(const CChromecastTarget&) = delete;

    // CCastTarget
    bool StartDiscovery() override;
    void StopDiscovery() override;
    std::vector<CastTargetDevice> GetDevices() override;

    void SetNotifyWindow(HWND hNotifyWnd, UINT stateMsg) override;

    // port is unused: the mDNS query a Chromecast answers always goes to 5353
    bool ProbeAddress(CastProtocol protocol, const CString& address, UINT port,
                      DWORD timeoutMs, CastTargetDevice& device) override;

    bool Connect(const CString& deviceId) override;
    bool ConnectSaved(CastSavedDevice& saved, DWORD directMs, DWORD searchMs) override;
    bool IsCasting() const override { return m_casting; }
    CString GetDeviceId() const override { return m_deviceId; }
    CString GetDeviceName() const override { return m_deviceName; }
    UINT GetSessionGeneration() const override { return m_generation; }

    bool CanCastFile(const CString& deviceId, const CString& path, const CastMediaInfo& info) override;
    bool CanCastFileSaved(const CastSavedDevice& saved, const CString& path, const CastMediaInfo& info) override;

    // Whether the default media receiver of a device of this model plays the
    // file, container and codecs both. model is the mDNS "md" record and may
    // be empty. pRefusal, when given, is filled in with why not, which is the
    // one thing a report of "it did not work" has to carry. Public so that the
    // decision can be exercised on its own.
    // maxAudioChannels caps the audio the device is known to output (0 = no
    // cap); it is last so existing callers need not pass it.
    static bool ReceiverCanPlay(const CString& path, const CastMediaInfo& info, const CString& model,
                                CString* pRefusal = nullptr, int maxAudioChannels = 0);

    void LoadMedia(const CString& filePath, const CString& title, double durationSec, double startSec,
                   const CastMediaInfo& info) override;

    void Play() override;
    void Pause() override;
    void Seek(double seconds) override;
    void SetVolume(double level, bool muted) override;
    void StopCasting() override;

    CastTargetState GetState() const override;
    double GetPosition() const override { return m_session.GetPosition(); }
    double GetDuration() const override { return m_session.GetDuration(); }
    CString GetFailureReason() const override;

private:
    static LRESULT CALLBACK MsgWndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
    bool EnsureMessageWindow();
    void OnSessionStateChanged();
    void OnHlsEvent(int notify); // WM_CAST_HLS, from the streaming transcode
    void OnDownmixDone(bool ok); // WM_CAST_DOWNMIX, from the whole-file transcode worker
    // Registers servePath with the media server and sends (or defers) the LOAD.
    // Shared by the direct no-transcode path and the transcode completions.
    void ServeFileAndLoad(const CString& servePath);
    void NotifyState(CastTargetState state);
    void SendLoad();
    // A seek ahead of what the streaming transcode has produced so far, held
    // back to just behind it; a no-op once the transcode has finished.
    double ClampHlsSeek(double seconds) const;
    static CastTargetState SimplifyState(CastSessionState state);
    static CString DeviceKey(const CastDevice& dev);
    static CString DeviceDisplayName(const CastDevice& dev);
    static CastTargetDevice ToTargetDevice(const CastDevice& dev);
    static void LogVerdict(const CString& name, const CString& model, const CStringA& mime,
                           bool ok, const CString& refusal);
    // Looks for one device by its stable id with a full discovery, for at most
    // timeoutMs. A discovery that was already running is left running.
    bool SearchById(const CString& id, DWORD timeoutMs, CastDevice& device);
    bool StartSession(const CastDevice& dev, const CString& deviceId, const CString& deviceName);

    CCastDiscovery m_discovery;
    CCastSession m_session;
    CCastMediaServer m_server;

    HWND m_hMsgWnd = nullptr;    // message-only window for session notifications
    HWND m_hNotifyWnd = nullptr; // the UI window fed with simplified notifications
    UINT m_stateMsg = 0;

    bool m_casting = false;
    bool m_failed = false; // failed outside the session, e.g. nothing to serve
    UINT m_generation = 0; // bumped by every Connect(), carried by notifications
    CString m_deviceId;
    CString m_deviceName;
    CastTargetState m_lastNotifiedState = CastTargetState::Idle;

    // work deferred until the session is far enough along
    bool m_loadPending = false;
    CString m_pendingTitle;
    double m_pendingDuration = 0.0;
    double m_pendingSeek = -1.0; // seconds, < 0 = none
    CStringA m_mime;

    // Set when the session connects, so the load can judge the file against the
    // device without the saved entry in hand: the model drives the receiver
    // rules, the cap is the user's per-device channel limit (0 = none).
    CString m_model;
    int m_maxAudioChannels = 0;
    // The transcode behind the file being served. An H.264 file is handed to
    // the device as an HLS stream that is produced while the device plays it
    // (m_hls); everything the stream cannot take -- HEVC video, which its
    // fragmented-MP4 sink refuses, a file with no picture, a stream that would
    // not start -- is transcoded whole first (m_downmixTemp) and served only
    // once finished. Both are torn down when the next file loads, the session
    // stops, or the target goes away.
    std::unique_ptr<CCastHlsStream> m_hls;
    bool m_hlsPending = false; // the load waits on the stream's first segments
    bool m_hlsReady = false;   // those segments are up; the LOAD may be sent
    CString m_downmixTemp;     // the complete-file transcode's copy, if any
    CString m_failReason;      // detail for the Failed state
    // The complete-file transcode runs on this worker so it does not freeze the
    // UI thread that LoadMedia is called on; it posts WM_CAST_DOWNMIX when done.
    // m_downmixCancel is signalled to abandon it (a new load, a stop, teardown),
    // which AbandonTranscode then joins on. m_downmixSrc is the original file,
    // served as-is if the transcode fails; m_downmixError carries the reason
    // from the worker to the completion handler (safe across the PostMessage).
    std::thread m_downmixThread;
    HANDLE m_downmixCancel = nullptr;
    bool m_downmixPending = false;
    CString m_downmixSrc;
    CString m_downmixError;
    void AbandonTranscode();
};
