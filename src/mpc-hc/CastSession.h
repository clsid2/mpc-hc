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

#include "CastDiscovery.h"
#include <memory>
#include <set>
#include "rapidjson/include/rapidjson/fwd.h"

enum class CastSessionState {
    Disconnected,   // no session
    Authenticating, // TLS is up, waiting for the device auth response
    Connecting,     // CONNECT sent to the platform receiver, waiting for its status
    Connected,      // platform connection up, media receiver app not joined
    Launching,      // LAUNCH sent, waiting for the app to come up
    Ready,          // joined the media receiver app, no media loaded
    Loading,        // LOAD sent, waiting for the first media status
    Buffering,
    Playing,
    Paused,
    Stopping,       // STOP sent, waiting for confirmation
    Stopped,        // media stopped or finished
    LoadFailed,     // the device rejected or failed to load the media
    TakenOver,      // another sender loaded media into our receiver app
    Dead,           // connection lost; reconnect requires a fresh Start()
};

// Phase 2 of Chromecast support: the CastV2 control channel to a single
// device. Speaks the length-prefixed protobuf protocol over TLS (Schannel)
// on port 8009: device auth, virtual connections, heartbeat, receiver app
// launch and media commands. Later phases add the local HTTP media server
// and the UI wiring.
class CCastSession
{
public:
    CCastSession();
    ~CCastSession();

    CCastSession(const CCastSession&) = delete;
    CCastSession& operator=(const CCastSession&) = delete;

    bool Start(const CastDevice& device);
    void Stop();
    bool IsRunning() const { return m_hThread != nullptr; }

    // Media commands; asynchronous, executed on the session thread.
    void Load(const CString& url, const CString& mime, double durationSec, const CString& title);
    void Play();
    void Pause();
    void StopMedia();
    void Seek(double seconds);
    void SetVolume(double level, bool muted);

    // Queues a media STOP and waits up to timeoutMs for the session thread to
    // have sent it, so that the receiver returns to its idle screen before the
    // connection is torn down. Returns false when the stop could not be sent
    // or did not go out in time; there being nothing to stop counts as sent,
    // since the receiver is already where the caller wants it.
    bool StopMediaAndWait(DWORD timeoutMs);

    // Thread-safe status accessors
    CastSessionState GetState() const;
    double GetPosition() const; // seconds, extrapolated from the last device status while playing
    double GetDuration() const; // seconds, 0 while unknown
    CString GetLocalAddress() const; // local IPv4 of the control connection, for building media URLs

    // Optional change notification: uMsg is posted to hWnd with the new state
    // in wParam whenever the session state changes. Pass nullptr to unregister.
    void SetNotifyWindow(HWND hWnd, UINT uMsg);

private:
    struct TlsContext;  // Schannel handles and stream sizes, defined in the .cpp
    struct CastMessage; // decoded CastV2 protobuf message, defined in the .cpp

    struct Command {
        enum class Type { Load, Play, Pause, Stop, Seek, SetVolume };
        Type type;
        CString url, mime, title; // Load
        double param = 0.0;       // Load duration / Seek position / SetVolume level
        bool muted = false;       // SetVolume
    };

    static DWORD WINAPI StaticThreadProc(LPVOID lpParam);
    DWORD ThreadProc();

    void QueueCommand(Command&& cmd);

    // networking, session thread only
    bool ConnectSocket();
    bool TlsHandshake();
    bool RecvWait(std::vector<BYTE>& buf, ULONGLONG deadline);
    int SocketRecv(BYTE* buf, int len);
    bool SocketSendAll(const BYTE* data, size_t len);
    bool TlsSend(const BYTE* data, size_t len);
    int TlsRecvDecrypt();
    void TeardownConnection(bool polite);

    // protocol, session thread only
    int NextRequestId();
    bool SendCastMessage(const CStringA& ns, const CStringA& destination, int payloadType,
                         const BYTE* payload, size_t len);
    bool SendJson(const CStringA& ns, const CStringA& destination, const CStringA& json);
    void SendReceiverGetStatus();
    void SendMediaGetStatus();
    void SendMediaStop();
    void ResetMediaSession();
    bool SendDeferredCommands();
    void ProcessPlainBuffer();
    void OnCastMessage(const CastMessage& msg);
    void OnDeviceAuthResponse(const CastMessage& msg);
    void OnReceiverMessage(const CStringA& type, const rapidjson::Value& d);
    void OnMediaMessage(const CStringA& type, const rapidjson::Value& d, int requestId);
    void ProcessCommands();
    void SetState(CastSessionState state);
    void UpdateMediaTime(double currentTime, double duration);

    // shared state, guarded by m_mutex
    mutable std::mutex m_mutex;
    CastSessionState m_state = CastSessionState::Disconnected;
    double m_mediaTime = 0.0;      // seconds, device-derived
    double m_mediaDuration = 0.0;  // seconds, 0 while unknown
    ULONGLONG m_mediaTimeTick = 0; // GetTickCount64() when m_mediaTime was received
    CString m_localAddress;
    HWND m_hNotifyWnd = nullptr;
    UINT m_uNotifyMsg = 0;
    std::vector<Command> m_commands;

    // session thread only
    CString m_ip;
    UINT m_port = 0;
    SOCKET m_socket = INVALID_SOCKET;
    WSAEVENT m_hSocketEvent = WSA_INVALID_EVENT;
    std::unique_ptr<TlsContext> m_tls;
    std::vector<BYTE> m_recvBuf;    // ciphertext not yet decrypted
    std::vector<BYTE> m_plainBuf;   // decrypted stream not yet framed
    std::vector<BYTE> m_sendBuf;    // reused TLS encryption buffer
    CStringA m_transportId;         // transport of the running media receiver app
    CStringA m_sessionId;           // receiver session of that app, for a receiver STOP
    int m_requestId = 0;            // last used requestId, monotonic from 1
    std::set<int> m_outstandingRequests; // requestIds still awaiting a response
    int m_loadRequestId = 0;        // the outstanding LOAD, 0 = none
    int m_mediaSessionId = 0;       // 0 = unknown

    // commands issued before the device delivered a mediaSessionId; they are
    // replayed once it arrives, keeping only the latest intent of each kind
    bool m_stopMediaDeferred = false;
    bool m_playPauseDeferred = false;
    bool m_deferredPlay = false;    // meaningful while m_playPauseDeferred
    double m_deferredSeek = -1.0;   // seconds, < 0 = none

    bool m_dead = false;
    ULONGLONG m_bringUpTick = 0;   // since when the receiver app has been missing
    ULONGLONG m_lastRecvTick = 0;
    ULONGLONG m_pingSentTick = 0;
    bool m_pingSent = false;
    ULONGLONG m_lastPollTick = 0;

    HANDLE m_hThread = nullptr;
    HANDLE m_hStopEvent = nullptr;
    HANDLE m_hCommandEvent = nullptr;
    HANDLE m_hMediaStopSentEvent = nullptr; // signalled once a media STOP went out
};
