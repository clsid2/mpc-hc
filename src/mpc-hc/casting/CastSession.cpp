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
#include "CastSession.h"
#include "CastMediaServer.h" // the media URL is masked before it is logged
#include "Logger.h"
#include <ws2tcpip.h>
#define SECURITY_WIN32
#include <security.h>
#include <schannel.h>
#include <algorithm>
#include <climits>
#include "rapidjson/include/rapidjson/document.h"

#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib, "secur32.lib")

#define CAST_DEFAULT_PORT     8009
#define CAST_SOURCE_ID        "sender-mpc"
#define CAST_RECEIVER_ID      "receiver-0"
#define CAST_APP_ID           "CC1AD845" // default media receiver
#define CAST_NS_DEVICEAUTH    "urn:x-cast:com.google.cast.tp.deviceauth"
#define CAST_NS_CONNECTION    "urn:x-cast:com.google.cast.tp.connection"
#define CAST_NS_HEARTBEAT     "urn:x-cast:com.google.cast.tp.heartbeat"
#define CAST_NS_RECEIVER      "urn:x-cast:com.google.cast.receiver"
#define CAST_NS_MEDIA         "urn:x-cast:com.google.cast.media"

#define CAST_MAX_MESSAGE_SIZE (10 * 1024) // larger inbound frames indicate a corrupt stream

#define CONNECT_TIMEOUT_MS      10000ull
#define SEND_TIMEOUT_MS         10000ull
#define HEARTBEAT_SILENCE_MS    6000ull // probe with our own PING after this much silence
#define MEDIA_POLL_INTERVAL_MS  4000ull
#define BRINGUP_TIMEOUT_MS      30000ull // a receiver app that never comes up
#define POLITE_CLOSE_DRAIN_MS   200ull   // flush the farewell before the socket goes
#define MAX_OUTSTANDING_REQUESTS 16     // unanswered requests are eventually forgotten
#define MAX_LOGGED_PAYLOAD      600     // a status blob is cut short in the log, not in the session

// Schannel state, kept out of the header so that it does not pull the SSPI
// headers into every consumer.
struct CCastSession::TlsContext {
    CredHandle hCred = {};
    CtxtHandle hCtxt = {};
    bool credValid = false;
    bool ctxtValid = false;
    SecPkgContext_StreamSizes sizes = {};
};

// One decoded CastMessage protobuf (cast_channel.proto, proto2):
// 1 = protocol_version (enum, always 0 = CASTV2_1_0), 2 = source_id (string),
// 3 = destination_id (string), 4 = namespace (string), 5 = payload_type
// (enum, 0 = STRING, 1 = BINARY), 6 = payload_utf8 (string), 7 = payload_binary (bytes)
struct CCastSession::CastMessage {
    ULONGLONG protocolVersion = 0;
    CStringA sourceId;
    CStringA destinationId;
    CStringA ns;
    ULONGLONG payloadType = 0;
    CStringA payloadUtf8;
    std::vector<BYTE> payloadBinary;
};

namespace
{
    // --- minimal protobuf wire format helpers (varint and length-delimited only) ---

    void PbPutVarint(std::vector<BYTE>& out, ULONGLONG v)
    {
        do {
            BYTE b = v & 0x7F;
            v >>= 7;
            if (v) {
                b |= 0x80;
            }
            out.emplace_back(b);
        } while (v);
    }

    void PbPutVarintField(std::vector<BYTE>& out, int field, ULONGLONG v)
    {
        PbPutVarint(out, ((ULONGLONG)field << 3) | 0); // wire type 0 = varint
        PbPutVarint(out, v);
    }

    void PbPutBytesField(std::vector<BYTE>& out, int field, const BYTE* data, size_t len)
    {
        PbPutVarint(out, ((ULONGLONG)field << 3) | 2); // wire type 2 = length-delimited
        PbPutVarint(out, len);
        out.insert(out.end(), data, data + len);
    }

    void PbPutStringField(std::vector<BYTE>& out, int field, const CStringA& s)
    {
        PbPutBytesField(out, field, reinterpret_cast<const BYTE*>(s.GetString()), (size_t)s.GetLength());
    }

    // Reads one varint, bounds-checked against len since this parses untrusted
    // network input. At most 10 bytes (64 bits).
    bool PbReadVarint(const BYTE* data, size_t len, size_t& pos, ULONGLONG& v)
    {
        v = 0;
        for (int shift = 0; shift < 64; shift += 7) {
            if (pos >= len) {
                return false;
            }
            BYTE b = data[pos++];
            v |= (ULONGLONG)(b & 0x7F) << shift;
            if (!(b & 0x80)) {
                return true;
            }
        }
        return false; // varint longer than 10 bytes
    }

    // --- hand-built JSON output, same approach as WebClientSocket.cpp ---

    // Escape a UTF-8 string so that it can be used as a JSON string value.
    CStringA JSONEscape(const CStringA& str)
    {
        CStringA escaped;

        for (int i = 0, len = str.GetLength(); i < len; i++) {
            unsigned char c = (unsigned char)str[i];
            switch (c) {
                case '\"':
                    escaped += "\\\"";
                    break;
                case '\\':
                    escaped += "\\\\";
                    break;
                case '\b':
                    escaped += "\\b";
                    break;
                case '\f':
                    escaped += "\\f";
                    break;
                case '\n':
                    escaped += "\\n";
                    break;
                case '\r':
                    escaped += "\\r";
                    break;
                case '\t':
                    escaped += "\\t";
                    break;
                default:
                    if (c < 0x20) {
                        escaped.AppendFormat("\\u%04x", c);
                    } else {
                        escaped += (char)c;
                    }
                    break;
            }
        }

        return escaped;
    }

    // Convert a native string into a quoted, escaped UTF-8 JSON string literal.
    CStringA JSONString(const CString& str)
    {
        return "\"" + JSONEscape(UTF16To8(str)) + "\"";
    }

    // --- rapidjson extraction helpers for inbound payloads ---

    CStringA GetJsonString(const rapidjson::Value& v, const char* name)
    {
        auto it = v.FindMember(name);
        if (it != v.MemberEnd() && it->value.IsString()) {
            return CStringA(it->value.GetString());
        }
        return CStringA();
    }

    int GetJsonInt(const rapidjson::Value& v, const char* name, int def)
    {
        auto it = v.FindMember(name);
        if (it != v.MemberEnd()) {
            if (it->value.IsInt()) {
                return it->value.GetInt();
            } else if (it->value.IsNumber()) {
                // a device is free to send a number an int could never hold,
                // and converting that one would be undefined, so it counts as
                // no answer at all
                const double value = it->value.GetDouble();
                if (value >= (double)INT_MIN && value <= (double)INT_MAX) {
                    return (int)value;
                }
            }
        }
        return def;
    }

    double GetJsonDouble(const rapidjson::Value& v, const char* name, double def)
    {
        auto it = v.FindMember(name);
        if (it != v.MemberEnd() && it->value.IsNumber()) {
            return it->value.GetDouble();
        }
        return def;
    }

    // A payload as the log wants it: the token out of the media URL, because
    // the log gets pasted in public, and a long status blob cut short, because
    // one has to be readable to be worth pasting.
    CStringA LogPayload(const CStringA& json)
    {
        const CStringA masked = CCastMediaServer::MaskURLToken(json);
        return masked.GetLength() <= MAX_LOGGED_PAYLOAD ? masked
               : masked.Left(MAX_LOGGED_PAYLOAD) + "...";
    }

    // The heartbeat and the status polling are the two things that happen
    // several times a minute for as long as a cast lasts; logging either would
    // bury everything that matters.
    bool WorthLogging(const CStringA& ns, const CStringA& payload)
    {
        return ns != CAST_NS_HEARTBEAT && payload.Find("\"GET_STATUS\"") < 0
               && payload.Find("\"MEDIA_STATUS\"") < 0;
    }

    // only referenced by TRACE, kept unconditional so release builds still parse
    LPCTSTR StateName(CastSessionState state)
    {
        switch (state) {
            case CastSessionState::Disconnected:
                return _T("Disconnected");
            case CastSessionState::Authenticating:
                return _T("Authenticating");
            case CastSessionState::Connecting:
                return _T("Connecting");
            case CastSessionState::Connected:
                return _T("Connected");
            case CastSessionState::Launching:
                return _T("Launching");
            case CastSessionState::Ready:
                return _T("Ready");
            case CastSessionState::Loading:
                return _T("Loading");
            case CastSessionState::Buffering:
                return _T("Buffering");
            case CastSessionState::Playing:
                return _T("Playing");
            case CastSessionState::Paused:
                return _T("Paused");
            case CastSessionState::Stopping:
                return _T("Stopping");
            case CastSessionState::Stopped:
                return _T("Stopped");
            case CastSessionState::LoadFailed:
                return _T("LoadFailed");
            case CastSessionState::TakenOver:
                return _T("TakenOver");
            case CastSessionState::Dead:
                return _T("Dead");
        }
        return _T("?");
    }

    bool IsMediaActiveState(CastSessionState state)
    {
        switch (state) {
            case CastSessionState::Loading:
            case CastSessionState::Buffering:
            case CastSessionState::Playing:
            case CastSessionState::Paused:
            case CastSessionState::Stopping:
                return true;
            default:
                return false;
        }
    }

    // Whether the media status has to be polled. While paused nothing can
    // change on its own and the receiver pushes any change it makes anyway.
    bool NeedsMediaPolling(CastSessionState state)
    {
        return IsMediaActiveState(state) && state != CastSessionState::Paused;
    }
}

CCastSession::CCastSession()
{
}

CCastSession::~CCastSession()
{
    Stop();
}

bool CCastSession::Start(const CastDevice& device)
{
    if (m_hThread || device.ipAddress.IsEmpty()) {
        return false;
    }

    m_ip = device.ipAddress;
    m_port = device.port ? device.port : CAST_DEFAULT_PORT;

    // reset per-session protocol state
    m_transportId.Empty();
    m_sessionId.Empty();
    m_requestId = 0;
    m_outstandingRequests.clear();
    m_loadRequestId = 0;
    m_mediaSessionId = 0;
    m_stopMediaDeferred = false;
    m_playPauseDeferred = false;
    m_deferredSeek = -1.0;
    m_dead = false;
    m_recvBuf.clear();
    m_plainBuf.clear();
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_state = CastSessionState::Disconnected;
        m_mediaTime = 0.0;
        m_mediaDuration = 0.0;
        m_mediaTimeTick = 0;
        m_localAddress.Empty();
        m_commands.clear();
    }

    m_hStopEvent = CreateEvent(nullptr, TRUE, FALSE, nullptr);
    m_hCommandEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
    m_hMediaStopSentEvent = CreateEvent(nullptr, TRUE, FALSE, nullptr);
    if (!m_hStopEvent || !m_hCommandEvent || !m_hMediaStopSentEvent) {
        Stop();
        return false;
    }

    m_hThread = ::CreateThread(nullptr, 0, StaticThreadProc, (LPVOID)this, 0, nullptr);
    if (!m_hThread) {
        Stop();
        return false;
    }

    return true;
}

void CCastSession::Stop()
{
    if (m_hThread) {
        SetEvent(m_hStopEvent);
        // The session thread dereferences this object throughout, so nothing
        // could be detached and leaked if it overran: the join has to be
        // unconditional or the object is freed underneath a live thread. Every
        // blocking operation it performs watches the stop event, so the first
        // wait is only a debug tripwire.
        if (WaitForSingleObject(m_hThread, 10000) != WAIT_OBJECT_0) {
            ASSERT(FALSE);
            WaitForSingleObject(m_hThread, INFINITE);
        }
        CloseHandle(m_hThread);
        m_hThread = nullptr;
    }

    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_hStopEvent) {
        CloseHandle(m_hStopEvent);
        m_hStopEvent = nullptr;
    }
    if (m_hCommandEvent) {
        CloseHandle(m_hCommandEvent);
        m_hCommandEvent = nullptr;
    }
    if (m_hMediaStopSentEvent) {
        CloseHandle(m_hMediaStopSentEvent);
        m_hMediaStopSentEvent = nullptr;
    }
    m_commands.clear();
    m_state = CastSessionState::Disconnected;
    m_localAddress.Empty();
}

CastSessionState CCastSession::GetState() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_state;
}

double CCastSession::GetPosition() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    double pos = m_mediaTime;
    // extrapolate from the last device status while playing, freeze otherwise
    if (m_state == CastSessionState::Playing && m_mediaTimeTick) {
        pos += (GetTickCount64() - m_mediaTimeTick) / 1000.0;
        if (m_mediaDuration > 0.0 && pos > m_mediaDuration) {
            pos = m_mediaDuration;
        }
    }
    return pos;
}

double CCastSession::GetDuration() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_mediaDuration;
}

CString CCastSession::GetLocalAddress() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_localAddress;
}

void CCastSession::SetNotifyWindow(HWND hWnd, UINT uMsg)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_hNotifyWnd = hWnd;
    m_uNotifyMsg = uMsg;
}

void CCastSession::SetState(CastSessionState state)
{
    HWND hWnd = nullptr;
    UINT uMsg = 0;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (m_state == state) {
            return;
        }
        m_state = state;
        hWnd = m_hNotifyWnd;
        uMsg = m_uNotifyMsg;
    }
    TRACE(_T("CastSession: state -> %s\n"), StateName(state));
    CASTING_LOG(_T("session: state -> %s"), StateName(state));
    if (hWnd) {
        ::PostMessage(hWnd, uMsg, (WPARAM)state, 0);
    }
}

void CCastSession::UpdateMediaTime(double currentTime, double duration)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    if (currentTime >= 0.0) {
        m_mediaTime = currentTime;
        m_mediaTimeTick = GetTickCount64();
    }
    if (duration > 0.0) {
        m_mediaDuration = duration;
    }
}

// --- media commands, queued to the session thread ---

void CCastSession::QueueCommand(Command&& cmd)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_hCommandEvent) { // no session running, nothing to queue to
        m_commands.emplace_back(std::move(cmd));
        SetEvent(m_hCommandEvent);
    }
}

void CCastSession::Load(const CString& url, const CString& mime, double durationSec, const CString& title)
{
    Command cmd;
    cmd.type = Command::Type::Load;
    cmd.url = url;
    cmd.mime = mime;
    cmd.title = title;
    cmd.param = durationSec;
    QueueCommand(std::move(cmd));
}

void CCastSession::Play()
{
    Command cmd;
    cmd.type = Command::Type::Play;
    QueueCommand(std::move(cmd));
}

void CCastSession::Pause()
{
    Command cmd;
    cmd.type = Command::Type::Pause;
    QueueCommand(std::move(cmd));
}

void CCastSession::StopMedia()
{
    Command cmd;
    cmd.type = Command::Type::Stop;
    QueueCommand(std::move(cmd));
}

void CCastSession::Seek(double seconds)
{
    Command cmd;
    cmd.type = Command::Type::Seek;
    cmd.param = seconds;
    QueueCommand(std::move(cmd));
}

void CCastSession::SetVolume(double level, bool muted)
{
    Command cmd;
    cmd.type = Command::Type::SetVolume;
    cmd.param = std::max(0.0, std::min(1.0, level));
    cmd.muted = muted;
    QueueCommand(std::move(cmd));
}

bool CCastSession::StopMediaAndWait(DWORD timeoutMs)
{
    HANDLE hSent;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (!m_hThread || !m_hMediaStopSentEvent) {
            return false;
        }
        hSent = m_hMediaStopSentEvent;
        ResetEvent(hSent);
    }
    StopMedia();
    // the session thread signals the event once the STOP has gone out, or
    // immediately when there is nothing to stop
    return WaitForSingleObject(hSent, timeoutMs) == WAIT_OBJECT_0;
}

// --- session thread ---

DWORD WINAPI CCastSession::StaticThreadProc(LPVOID lpParam)
{
    SetThreadName(DWORD(-1), "CastSession Thread");
    return ((CCastSession*)lpParam)->ThreadProc();
}

DWORD CCastSession::ThreadProc()
{
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        SetState(CastSessionState::Dead);
        return DWORD_ERROR;
    }

    m_hSocketEvent = WSACreateEvent();
    if (m_hSocketEvent == WSA_INVALID_EVENT) {
        SetState(CastSessionState::Dead);
        WSACleanup();
        return DWORD_ERROR;
    }

    if (!ConnectSocket() || !TlsHandshake()) {
        if (WaitForSingleObject(m_hStopEvent, 0) != WAIT_OBJECT_0) {
            SetState(CastSessionState::Dead);
        }
        TeardownConnection(false);
        WSACloseEvent(m_hSocketEvent);
        m_hSocketEvent = WSA_INVALID_EVENT;
        WSACleanup();
        return DWORD_ERROR;
    }

    // the local IPv4 of the control connection is the address the device can
    // reach us at; the media server of phase 3 builds its URLs from it
    sockaddr_in local;
    int localLen = sizeof(local);
    if (getsockname(m_socket, (sockaddr*)&local, &localLen) == 0) {
        char buf[16] = { 0 };
        if (inet_ntop(AF_INET, (PVOID)&local.sin_addr, buf, sizeof(buf))) {
            std::lock_guard<std::mutex> lock(m_mutex);
            m_localAddress = buf;
        }
    }

    // step 1 of the bring-up: device auth with an empty challenge
    // (DeviceAuthMessage, field 1 "challenge" = empty embedded message)
    SetState(CastSessionState::Authenticating);
    static const BYTE authChallenge[] = { 0x0A, 0x00 };
    SendCastMessage(CAST_NS_DEVICEAUTH, CAST_RECEIVER_ID, 1, authChallenge, sizeof(authChallenge));

    m_lastRecvTick = GetTickCount64();
    m_pingSent = false;
    m_lastPollTick = m_lastRecvTick;
    m_bringUpTick = 0;

    HANDLE handles[] = { m_hStopEvent, m_hCommandEvent, m_hSocketEvent };
    bool stopped = false;
    while (!m_dead) {
        // sleep until the nearest of the two periodic deadlines, rather than
        // waking up on a fixed tick that mostly has nothing to do
        const ULONGLONG tick = GetTickCount64();
        ULONGLONG deadline = (m_pingSent ? m_pingSentTick : m_lastRecvTick) + HEARTBEAT_SILENCE_MS;
        if (!m_loadRequestId && !m_transportId.IsEmpty() && NeedsMediaPolling(GetState())) {
            deadline = std::min(deadline, m_lastPollTick + MEDIA_POLL_INTERVAL_MS);
        }

        DWORD ret = WaitForMultipleObjects(_countof(handles), handles, FALSE,
                                           (DWORD)(deadline > tick ? deadline - tick : 0));
        if (ret == WAIT_OBJECT_0) {
            stopped = true;
            break;
        } else if (ret == WAIT_OBJECT_0 + 1) {
            ProcessCommands();
        } else if (ret == WAIT_OBJECT_0 + 2) {
            WSANETWORKEVENTS ne;
            ZeroMemory(&ne, sizeof(ne));
            WSAEnumNetworkEvents(m_socket, m_hSocketEvent, &ne);
            int res = TlsRecvDecrypt();
            ProcessPlainBuffer();
            if (res < 0 || (ne.lNetworkEvents & FD_CLOSE)) {
                m_dead = true;
            }
        }

        const ULONGLONG now = GetTickCount64();

        // heartbeat: the device normally PINGs every ~5 s and we PONG. After
        // 6 s of silence probe with our own PING + GET_STATUS; after a second
        // silent 6 s window declare the connection dead.
        if (!m_dead && now - m_lastRecvTick >= HEARTBEAT_SILENCE_MS) {
            if (!m_pingSent) {
                SendJson(CAST_NS_HEARTBEAT, CAST_RECEIVER_ID, "{\"type\":\"PING\"}");
                SendReceiverGetStatus();
                m_pingSent = true;
                m_pingSentTick = now;
            } else if (now - m_pingSentTick >= HEARTBEAT_SILENCE_MS) {
                TRACE(_T("CastSession: no heartbeat, connection is dead\n"));
                CASTING_LOG(_T("session: the device stopped answering the heartbeat"));
                m_dead = true;
            }
        }

        // The receiver app either comes up or the device is not usable: a
        // bring-up that never completes would otherwise sit there connecting
        // for as long as the device keeps the heartbeat alive.
        if (!m_dead && m_transportId.IsEmpty()) {
            if (!m_bringUpTick) {
                m_bringUpTick = now;
            } else if (now - m_bringUpTick >= BRINGUP_TIMEOUT_MS) {
                TRACE(_T("CastSession: the receiver app never came up\n"));
                CASTING_LOG(_T("session: the receiver application never came up"));
                m_dead = true;
            }
        } else {
            m_bringUpTick = 0;
        }

        // poll the media status while playback can advance on its own; never
        // while a LOAD is outstanding, so that its response cannot be lost
        if (!m_dead && !m_loadRequestId && !m_transportId.IsEmpty() && NeedsMediaPolling(GetState())
                && now - m_lastPollTick >= MEDIA_POLL_INTERVAL_MS) {
            SendMediaGetStatus();
            m_lastPollTick = now;
        }
    }

    if (m_dead) {
        SetState(CastSessionState::Dead);
    }
    TeardownConnection(stopped && !m_dead);
    WSACloseEvent(m_hSocketEvent);
    m_hSocketEvent = WSA_INVALID_EVENT;
    WSACleanup();
    return 0;
}

bool CCastSession::ConnectSocket()
{
    m_socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (m_socket == INVALID_SOCKET) {
        return false;
    }

    // WSAEventSelect also puts the socket into non-blocking mode
    if (WSAEventSelect(m_socket, m_hSocketEvent, FD_CONNECT | FD_READ | FD_WRITE | FD_CLOSE) == SOCKET_ERROR) {
        return false;
    }

    sockaddr_in addr;
    ZeroMemory(&addr, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons((u_short)m_port);
    if (InetPtonW(AF_INET, m_ip.GetString(), &addr.sin_addr) != 1) {
        return false;
    }

    if (connect(m_socket, (sockaddr*)&addr, sizeof(addr)) == SOCKET_ERROR
            && WSAGetLastError() != WSAEWOULDBLOCK) {
        return false;
    }

    const ULONGLONG deadline = GetTickCount64() + CONNECT_TIMEOUT_MS;
    for (;;) {
        ULONGLONG now = GetTickCount64();
        if (now >= deadline) {
            return false;
        }
        HANDLE handles[] = { m_hStopEvent, m_hSocketEvent };
        DWORD ret = WaitForMultipleObjects(_countof(handles), handles, FALSE,
                                           (DWORD)std::min<ULONGLONG>(500, deadline - now));
        if (ret == WAIT_OBJECT_0) {
            return false; // stop requested
        } else if (ret == WAIT_OBJECT_0 + 1) {
            WSANETWORKEVENTS ne;
            ZeroMemory(&ne, sizeof(ne));
            if (WSAEnumNetworkEvents(m_socket, m_hSocketEvent, &ne) == SOCKET_ERROR) {
                return false;
            }
            if (ne.lNetworkEvents & FD_CONNECT) {
                return ne.iErrorCode[FD_CONNECT_BIT] == 0;
            }
            if (ne.lNetworkEvents & FD_CLOSE) {
                return false;
            }
        }
    }
}

bool CCastSession::TlsHandshake()
{
    m_tls = std::make_unique<TlsContext>();

    // Cast devices use self-signed, frequently rotating certificates whose
    // subject does not match the IP address, so certificate validation must
    // be disabled and the chain is deliberately never verified.
    SCHANNEL_CRED cred;
    ZeroMemory(&cred, sizeof(cred));
    cred.dwVersion = SCHANNEL_CRED_VERSION;
    cred.grbitEnabledProtocols = SP_PROT_TLS1_2_CLIENT;
    cred.dwFlags = SCH_CRED_MANUAL_CRED_VALIDATION | SCH_CRED_NO_DEFAULT_CREDS;

    if (AcquireCredentialsHandle(nullptr, const_cast<LPTSTR>(UNISP_NAME), SECPKG_CRED_OUTBOUND,
                                 nullptr, &cred, nullptr, nullptr, &m_tls->hCred, nullptr) != SEC_E_OK) {
        return false;
    }
    m_tls->credValid = true;

    const DWORD dwReqFlags = ISC_REQ_SEQUENCE_DETECT | ISC_REQ_REPLAY_DETECT | ISC_REQ_CONFIDENTIALITY
                             | ISC_REQ_ALLOCATE_MEMORY | ISC_REQ_STREAM | ISC_REQ_MANUAL_CRED_VALIDATION;
    DWORD dwRetFlags = 0;

    SecBuffer outBuf = { 0, SECBUFFER_TOKEN, nullptr };
    SecBufferDesc outDesc = { SECBUFFER_VERSION, 1, &outBuf };

    SECURITY_STATUS status = InitializeSecurityContext(&m_tls->hCred, nullptr,
                                                       const_cast<LPWSTR>(m_ip.GetString()),
                                                       dwReqFlags, 0, 0, nullptr, 0,
                                                       &m_tls->hCtxt, &outDesc, &dwRetFlags, nullptr);
    if (status != SEC_I_CONTINUE_NEEDED) {
        return false;
    }
    m_tls->ctxtValid = true;

    bool sent = SocketSendAll((const BYTE*)outBuf.pvBuffer, outBuf.cbBuffer);
    FreeContextBuffer(outBuf.pvBuffer);
    if (!sent) {
        return false;
    }

    std::vector<BYTE> buf;
    const ULONGLONG deadline = GetTickCount64() + CONNECT_TIMEOUT_MS;

    for (;;) {
        if (buf.empty() || status == SEC_E_INCOMPLETE_MESSAGE) {
            if (!RecvWait(buf, deadline)) {
                return false;
            }
        }

        SecBuffer inBufs[2] = {
            { (ULONG)buf.size(), SECBUFFER_TOKEN, buf.data() },
            { 0, SECBUFFER_EMPTY, nullptr },
        };
        SecBufferDesc inDesc = { SECBUFFER_VERSION, _countof(inBufs), inBufs };
        outBuf = { 0, SECBUFFER_TOKEN, nullptr };
        outDesc = { SECBUFFER_VERSION, 1, &outBuf };

        status = InitializeSecurityContext(&m_tls->hCred, &m_tls->hCtxt, nullptr, dwReqFlags,
                                           0, 0, &inDesc, 0, nullptr, &outDesc, &dwRetFlags, nullptr);

        if (outBuf.pvBuffer && outBuf.cbBuffer) {
            sent = SocketSendAll((const BYTE*)outBuf.pvBuffer, outBuf.cbBuffer);
            FreeContextBuffer(outBuf.pvBuffer);
            if (!sent) {
                return false;
            }
        }

        if (status == SEC_E_INCOMPLETE_MESSAGE) {
            continue; // keep the partial record, receive more
        }

        // consume the processed input, keeping any extra unprocessed bytes
        if (inBufs[1].BufferType == SECBUFFER_EXTRA && inBufs[1].cbBuffer > 0) {
            size_t extra = inBufs[1].cbBuffer;
            memmove(buf.data(), buf.data() + buf.size() - extra, extra);
            buf.resize(extra);
        } else {
            buf.clear();
        }

        if (status == SEC_E_OK) {
            if (!buf.empty()) {
                // ciphertext that already arrived past the handshake
                m_recvBuf = std::move(buf);
            }
            return QueryContextAttributes(&m_tls->hCtxt, SECPKG_ATTR_STREAM_SIZES, &m_tls->sizes) == SEC_E_OK;
        }
        if (status != SEC_I_CONTINUE_NEEDED) {
            TRACE(_T("CastSession: TLS handshake failed (0x%08lx)\n"), status);
            CASTING_LOG(_T("session: the TLS handshake with the device failed (0x%08lx)"), status);
            return false;
        }
    }
}

// Waits until at least one byte can be received, then appends everything
// currently available to buf. Returns false on stop request, timeout,
// connection close or error.
bool CCastSession::RecvWait(std::vector<BYTE>& buf, ULONGLONG deadline)
{
    for (;;) {
        BYTE tmp[4096];
        int r = SocketRecv(tmp, sizeof(tmp));
        if (r < 0) {
            return false;
        }
        if (r > 0) {
            buf.insert(buf.end(), tmp, tmp + r);
            while ((r = SocketRecv(tmp, sizeof(tmp))) > 0) {
                buf.insert(buf.end(), tmp, tmp + r);
            }
            return true;
        }

        ULONGLONG now = GetTickCount64();
        if (now >= deadline) {
            return false;
        }
        HANDLE handles[] = { m_hStopEvent, m_hSocketEvent };
        DWORD ret = WaitForMultipleObjects(_countof(handles), handles, FALSE,
                                           (DWORD)std::min<ULONGLONG>(500, deadline - now));
        if (ret == WAIT_OBJECT_0) {
            return false; // stop requested
        } else if (ret == WAIT_OBJECT_0 + 1) {
            WSANETWORKEVENTS ne;
            ZeroMemory(&ne, sizeof(ne));
            WSAEnumNetworkEvents(m_socket, m_hSocketEvent, &ne);
        }
    }
}

// Non-blocking receive: > 0 = bytes received, 0 = nothing available,
// < 0 = connection closed or failed.
int CCastSession::SocketRecv(BYTE* buf, int len)
{
    int r = recv(m_socket, (char*)buf, len, 0);
    if (r > 0) {
        return r;
    }
    if (r == 0) {
        return -1; // gracefully closed
    }
    return WSAGetLastError() == WSAEWOULDBLOCK ? 0 : -1;
}

bool CCastSession::SocketSendAll(const BYTE* data, size_t len)
{
    const ULONGLONG deadline = GetTickCount64() + SEND_TIMEOUT_MS;
    size_t sent = 0;
    while (sent < len) {
        int r = send(m_socket, (const char*)data + sent, (int)(len - sent), 0);
        if (r != SOCKET_ERROR) {
            sent += r;
            continue;
        }
        if (WSAGetLastError() != WSAEWOULDBLOCK) {
            return false;
        }
        // rare: the send buffer is full, wait for it to drain
        if (WaitForSingleObject(m_hStopEvent, 0) == WAIT_OBJECT_0 || GetTickCount64() >= deadline) {
            return false;
        }
        fd_set wfds;
        FD_ZERO(&wfds);
        FD_SET(m_socket, &wfds);
        timeval tv = { 0, 100 * 1000 }; // 100 ms
        if (select(0, nullptr, &wfds, nullptr, &tv) == SOCKET_ERROR) {
            return false;
        }
    }
    return true;
}

bool CCastSession::TlsSend(const BYTE* data, size_t len)
{
    const SecPkgContext_StreamSizes& sizes = m_tls->sizes;
    // a full record's worth of scratch space, kept across messages: every
    // outbound message would otherwise allocate ~16-21 KB
    std::vector<BYTE>& buf = m_sendBuf;
    buf.resize((size_t)sizes.cbHeader + sizes.cbMaximumMessage + sizes.cbTrailer);

    while (len > 0) {
        const ULONG chunk = (ULONG)std::min<size_t>(len, sizes.cbMaximumMessage);
        memcpy(buf.data() + sizes.cbHeader, data, chunk);

        SecBuffer bufs[4] = {
            { sizes.cbHeader, SECBUFFER_STREAM_HEADER, buf.data() },
            { chunk, SECBUFFER_DATA, buf.data() + sizes.cbHeader },
            { sizes.cbTrailer, SECBUFFER_STREAM_TRAILER, buf.data() + sizes.cbHeader + chunk },
            { 0, SECBUFFER_EMPTY, nullptr },
        };
        SecBufferDesc desc = { SECBUFFER_VERSION, _countof(bufs), bufs };
        if (EncryptMessage(&m_tls->hCtxt, 0, &desc, 0) != SEC_E_OK) {
            return false;
        }
        if (!SocketSendAll(buf.data(), (size_t)bufs[0].cbBuffer + bufs[1].cbBuffer + bufs[2].cbBuffer)) {
            return false;
        }
        data += chunk;
        len -= chunk;
    }
    return true;
}

// Pulls everything available off the socket and decrypts all complete TLS
// records into m_plainBuf. One CastV2 message can span records and one record
// can hold several messages; leftover ciphertext (SECBUFFER_EXTRA and partial
// records) stays in m_recvBuf. Returns < 0 on connection loss or TLS failure.
int CCastSession::TlsRecvDecrypt()
{
    BYTE tmp[8192];
    int r;
    while ((r = SocketRecv(tmp, sizeof(tmp))) > 0) {
        m_recvBuf.insert(m_recvBuf.end(), tmp, tmp + r);
    }
    const bool closed = (r < 0);

    while (!m_recvBuf.empty()) {
        SecBuffer bufs[4] = {
            { (ULONG)m_recvBuf.size(), SECBUFFER_DATA, m_recvBuf.data() },
            { 0, SECBUFFER_EMPTY, nullptr },
            { 0, SECBUFFER_EMPTY, nullptr },
            { 0, SECBUFFER_EMPTY, nullptr },
        };
        SecBufferDesc desc = { SECBUFFER_VERSION, _countof(bufs), bufs };

        SECURITY_STATUS status = DecryptMessage(&m_tls->hCtxt, &desc, 0, nullptr);
        if (status == SEC_E_INCOMPLETE_MESSAGE) {
            // partial record; sanity-cap the buffer at one full record
            const SecPkgContext_StreamSizes& sizes = m_tls->sizes;
            if (m_recvBuf.size() > (size_t)sizes.cbHeader + sizes.cbMaximumMessage + sizes.cbTrailer) {
                return -1;
            }
            break;
        }
        if (status != SEC_E_OK) {
            // includes SEC_I_CONTEXT_EXPIRED and SEC_I_RENEGOTIATE, neither of
            // which is expected from a cast device
            TRACE(_T("CastSession: DecryptMessage failed (0x%08lx)\n"), status);
            return -1;
        }

        const BYTE* extraPtr = nullptr;
        size_t extraLen = 0;
        for (const SecBuffer& b : bufs) {
            if (b.BufferType == SECBUFFER_DATA && b.cbBuffer) {
                const BYTE* p = (const BYTE*)b.pvBuffer;
                m_plainBuf.insert(m_plainBuf.end(), p, p + b.cbBuffer);
            } else if (b.BufferType == SECBUFFER_EXTRA && b.cbBuffer) {
                extraPtr = (const BYTE*)b.pvBuffer;
                extraLen = b.cbBuffer;
            }
        }
        if (extraLen) {
            // the extra data points into m_recvBuf itself
            memmove(m_recvBuf.data(), extraPtr, extraLen);
            m_recvBuf.resize(extraLen);
        } else {
            m_recvBuf.clear();
        }
    }

    return closed ? -1 : 0;
}

void CCastSession::TeardownConnection(bool polite)
{
    if (polite && m_tls && m_tls->ctxtValid && m_socket != INVALID_SOCKET) {
        // A media STOP still waiting for a mediaSessionId will never be sent
        // now, and dropping the connection with it undone leaves our media
        // playing on the device. Closing the receiver application needs no
        // mediaSessionId and is what returns the device to its idle screen.
        if (m_stopMediaDeferred && !m_sessionId.IsEmpty()) {
            CStringA json;
            json.Format("{\"type\":\"STOP\",\"sessionId\":\"%s\",\"requestId\":%d}",
                        m_sessionId.GetString(), NextRequestId());
            SendJson(CAST_NS_RECEIVER, CAST_RECEIVER_ID, json);
            m_stopMediaDeferred = false;
        }
        // politely close the virtual connections before dropping the socket
        if (!m_transportId.IsEmpty()) {
            SendJson(CAST_NS_CONNECTION, m_transportId, "{\"type\":\"CLOSE\"}");
        }
        SendJson(CAST_NS_CONNECTION, CAST_RECEIVER_ID, "{\"type\":\"CLOSE\"}");

        // Closing a socket that still holds unread data resets the connection,
        // and the reset discards everything just written along with it, so the
        // farewell has to be flushed with a shutdown and whatever the device
        // sent in the meantime drained before the handle goes.
        shutdown(m_socket, SD_SEND);
        const ULONGLONG deadline = GetTickCount64() + POLITE_CLOSE_DRAIN_MS;
        BYTE discard[4096];
        while (SocketRecv(discard, sizeof(discard)) >= 0 && GetTickCount64() < deadline) {
            Sleep(5);
        }
    }

    if (m_socket != INVALID_SOCKET) {
        closesocket(m_socket);
        m_socket = INVALID_SOCKET;
    }
    if (m_tls) {
        if (m_tls->ctxtValid) {
            DeleteSecurityContext(&m_tls->hCtxt);
        }
        if (m_tls->credValid) {
            FreeCredentialsHandle(&m_tls->hCred);
        }
        m_tls.reset();
    }
    m_recvBuf.clear();
    m_plainBuf.clear();
}

// --- CastV2 protocol ---

int CCastSession::NextRequestId()
{
    // Every request in flight is remembered, so that a long-running one (a
    // LOAD above all) is never superseded by a routine status poll and its
    // response - including LOAD_FAILED - is not dropped. Requests that are
    // never answered are eventually forgotten to bound the set.
    m_outstandingRequests.insert(++m_requestId);
    while (m_outstandingRequests.size() > MAX_OUTSTANDING_REQUESTS) {
        m_outstandingRequests.erase(m_outstandingRequests.begin()); // the oldest
    }
    return m_requestId;
}

bool CCastSession::SendCastMessage(const CStringA& ns, const CStringA& destination, int payloadType,
                                   const BYTE* payload, size_t len)
{
    std::vector<BYTE> body;
    body.reserve(len + ns.GetLength() + 64);
    PbPutVarintField(body, 1, 0); // protocol_version = CASTV2_1_0
    PbPutStringField(body, 2, CAST_SOURCE_ID);
    PbPutStringField(body, 3, destination);
    PbPutStringField(body, 4, ns);
    PbPutVarintField(body, 5, (ULONGLONG)payloadType);
    if (payloadType == 0) {
        PbPutBytesField(body, 6, payload, len); // payload_utf8
    } else {
        PbPutBytesField(body, 7, payload, len); // payload_binary
    }

    // 4-byte big-endian length prefix covering the protobuf only
    std::vector<BYTE> frame;
    frame.reserve(4 + body.size());
    const UINT bodyLen = (UINT)body.size();
    frame.emplace_back((BYTE)(bodyLen >> 24));
    frame.emplace_back((BYTE)(bodyLen >> 16));
    frame.emplace_back((BYTE)(bodyLen >> 8));
    frame.emplace_back((BYTE)bodyLen);
    frame.insert(frame.end(), body.cbegin(), body.cend());

    if (!TlsSend(frame.data(), frame.size())) {
        m_dead = true;
        return false;
    }
    return true;
}

bool CCastSession::SendJson(const CStringA& ns, const CStringA& destination, const CStringA& json)
{
    TRACE(_T("CastSession: -> [%hs] %hs\n"), destination.GetString(), json.GetString());
    if (WorthLogging(ns, json)) {
        CASTING_LOG(_T("session: -> %hs"), LogPayload(json).GetString());
    }
    return SendCastMessage(ns, destination, 0, reinterpret_cast<const BYTE*>(json.GetString()),
                           (size_t)json.GetLength());
}

void CCastSession::SendReceiverGetStatus()
{
    CStringA json;
    json.Format("{\"type\":\"GET_STATUS\",\"requestId\":%d}", NextRequestId());
    SendJson(CAST_NS_RECEIVER, CAST_RECEIVER_ID, json);
}

void CCastSession::SendMediaGetStatus()
{
    CStringA json;
    if (m_mediaSessionId) {
        json.Format("{\"type\":\"GET_STATUS\",\"mediaSessionId\":%d,\"requestId\":%d}",
                    m_mediaSessionId, NextRequestId());
    } else {
        json.Format("{\"type\":\"GET_STATUS\",\"requestId\":%d}", NextRequestId());
    }
    SendJson(CAST_NS_MEDIA, m_transportId, json);
}

void CCastSession::SendMediaStop()
{
    CStringA json;
    json.Format("{\"type\":\"STOP\",\"mediaSessionId\":%d,\"requestId\":%d}",
                m_mediaSessionId, NextRequestId());
    // A STOP that never made it onto the wire leaves the receiver playing, so
    // nobody waiting for it may be told it went out. The connection is dead in
    // that case and the wait ends with the session instead.
    if (!SendJson(CAST_NS_MEDIA, m_transportId, json)) {
        return;
    }
    SetState(CastSessionState::Stopping);
    if (m_hMediaStopSentEvent) {
        SetEvent(m_hMediaStopSentEvent); // releases a waiting StopMediaAndWait()
    }
}

// The receiver app is gone or was never joined: there is no media session
// left, no point in replaying anything and nobody should wait for a stop.
void CCastSession::ResetMediaSession()
{
    m_transportId.Empty();
    m_sessionId.Empty();
    m_mediaSessionId = 0;
    m_loadRequestId = 0;
    m_stopMediaDeferred = false;
    m_playPauseDeferred = false;
    m_deferredSeek = -1.0;
    if (m_hMediaStopSentEvent) {
        SetEvent(m_hMediaStopSentEvent);
    }
}

// Replays the commands that arrived before the device had given us a
// mediaSessionId. Returns true when a deferred STOP ended the media session,
// in which case the caller must not act on the status any further.
bool CCastSession::SendDeferredCommands()
{
    if (m_stopMediaDeferred) {
        m_stopMediaDeferred = false;
        m_playPauseDeferred = false;
        m_deferredSeek = -1.0;
        SendMediaStop();
        return true;
    }

    if (m_deferredSeek >= 0.0) {
        CStringA json;
        json.Format("{\"type\":\"SEEK\",\"mediaSessionId\":%d,\"currentTime\":%.6f,"
                    "\"resumeState\":\"PLAYBACK_START\",\"requestId\":%d}",
                    m_mediaSessionId, m_deferredSeek, NextRequestId());
        SendJson(CAST_NS_MEDIA, m_transportId, json);
        m_deferredSeek = -1.0;
    }
    if (m_playPauseDeferred) {
        CStringA json;
        json.Format("{\"type\":\"%s\",\"mediaSessionId\":%d,\"requestId\":%d}",
                    m_deferredPlay ? "PLAY" : "PAUSE", m_mediaSessionId, NextRequestId());
        SendJson(CAST_NS_MEDIA, m_transportId, json);
        m_playPauseDeferred = false;
    }
    return false;
}

// Splits the decrypted stream into CastV2 frames, strictly by the length prefix.
void CCastSession::ProcessPlainBuffer()
{
    size_t pos = 0;
    while (!m_dead && m_plainBuf.size() - pos >= 4) {
        const BYTE* p = m_plainBuf.data() + pos;
        const UINT msgLen = ((UINT)p[0] << 24) | ((UINT)p[1] << 16) | ((UINT)p[2] << 8) | p[3];
        if (msgLen > CAST_MAX_MESSAGE_SIZE) {
            TRACE(_T("CastSession: oversized frame (%u bytes), killing the connection\n"), msgLen);
            CASTING_LOG(_T("session: the device sent a %u byte frame, which cannot be one of ours"), msgLen);
            m_dead = true;
            break;
        }
        if (m_plainBuf.size() - pos - 4 < msgLen) {
            break; // incomplete frame, wait for more data
        }

        CastMessage msg;
        size_t msgPos = 0;
        const BYTE* body = p + 4;
        bool valid = true;
        while (valid && msgPos < msgLen) {
            ULONGLONG tag;
            if (!PbReadVarint(body, msgLen, msgPos, tag)) {
                valid = false;
                break;
            }
            const int field = (int)(tag >> 3);
            const int wireType = (int)(tag & 7);
            if (wireType == 0) { // varint
                ULONGLONG v;
                if (!PbReadVarint(body, msgLen, msgPos, v)) {
                    valid = false;
                    break;
                }
                if (field == 1) {
                    msg.protocolVersion = v;
                } else if (field == 5) {
                    msg.payloadType = v;
                }
            } else if (wireType == 2) { // length-delimited
                ULONGLONG fieldLen;
                if (!PbReadVarint(body, msgLen, msgPos, fieldLen) || fieldLen > msgLen - msgPos) {
                    valid = false;
                    break;
                }
                const char* s = reinterpret_cast<const char*>(body + msgPos);
                switch (field) {
                    case 2:
                        msg.sourceId = CStringA(s, (int)fieldLen);
                        break;
                    case 3:
                        msg.destinationId = CStringA(s, (int)fieldLen);
                        break;
                    case 4:
                        msg.ns = CStringA(s, (int)fieldLen);
                        break;
                    case 6:
                        msg.payloadUtf8 = CStringA(s, (int)fieldLen);
                        break;
                    case 7:
                        msg.payloadBinary.assign(body + msgPos, body + msgPos + fieldLen);
                        break;
                }
                msgPos += (size_t)fieldLen;
            } else {
                valid = false; // only wire types 0 and 2 exist in cast_channel.proto
            }
        }

        if (!valid) {
            TRACE(_T("CastSession: malformed protobuf, killing the connection\n"));
            CASTING_LOG(_T("session: the device sent a frame we could not parse"));
            m_dead = true;
            break;
        }

        m_lastRecvTick = GetTickCount64();
        m_pingSent = false;
        OnCastMessage(msg);
        pos += 4 + msgLen;
    }

    if (m_dead) {
        m_plainBuf.clear();
    } else if (pos) {
        m_plainBuf.erase(m_plainBuf.begin(), m_plainBuf.begin() + pos);
    }
}

void CCastSession::OnCastMessage(const CastMessage& msg)
{
    if (msg.ns == CAST_NS_DEVICEAUTH) {
        OnDeviceAuthResponse(msg);
        return;
    }
    if (msg.payloadType != 0) {
        return; // only the auth namespace uses binary payloads
    }

    rapidjson::Document d;
    d.Parse(msg.payloadUtf8.GetString());
    if (d.HasParseError() || !d.IsObject()) {
        TRACE(_T("CastSession: unparsable payload on %hs\n"), msg.ns.GetString());
        return;
    }

    const CStringA type = GetJsonString(d, "type");
    TRACE(_T("CastSession: <- [%hs] %hs\n"), msg.sourceId.GetString(), msg.payloadUtf8.GetString());
    if (WorthLogging(msg.ns, msg.payloadUtf8)) {
        CASTING_LOG(_T("session: <- %hs"), LogPayload(msg.payloadUtf8).GetString());
    }

    // while requests of ours are in flight, a reply carrying a requestId that
    // is none of them belongs to another sender and is dropped; unsolicited
    // messages (requestId 0) are always processed
    const int requestId = GetJsonInt(d, "requestId", 0);
    if (requestId != 0 && !m_outstandingRequests.empty()) {
        auto it = m_outstandingRequests.find(requestId);
        if (it == m_outstandingRequests.end()) {
            return;
        }
        m_outstandingRequests.erase(it);
    }

    if (msg.ns == CAST_NS_HEARTBEAT) {
        if (type == "PING") {
            SendJson(CAST_NS_HEARTBEAT,
                     msg.sourceId.IsEmpty() ? CStringA(CAST_RECEIVER_ID) : msg.sourceId,
                     "{\"type\":\"PONG\"}");
        }
        // PONG only refreshes m_lastRecvTick, which every message does
    } else if (msg.ns == CAST_NS_CONNECTION) {
        if (type == "CLOSE") {
            // The receiver app closed our virtual connection; the socket is
            // still fine and the app can be relaunched. With media of ours on
            // it, though, the app going away is the device being handed to
            // somebody else: reporting that ends the session, where dropping
            // back to Connected would leave it connecting for good.
            TRACE(_T("CastSession: app connection closed\n"));
            CASTING_LOG(_T("session: the receiver application closed our connection"));
            const bool hadMedia = IsMediaActiveState(GetState());
            ResetMediaSession();
            SetState(hadMedia ? CastSessionState::TakenOver : CastSessionState::Connected);
        }
    } else if (msg.ns == CAST_NS_RECEIVER) {
        OnReceiverMessage(type, d);
    } else if (msg.ns == CAST_NS_MEDIA) {
        OnMediaMessage(type, d, requestId);
    }
}

void CCastSession::OnDeviceAuthResponse(const CastMessage& msg)
{
    // DeviceAuthMessage: 1 = challenge, 2 = response, 3 = error. Accept any
    // well-formed response without an error; the signature is not verified.
    bool hasResponse = false;
    bool hasError = false;
    bool valid = true;
    const BYTE* data = msg.payloadBinary.data();
    const size_t len = msg.payloadBinary.size();
    size_t pos = 0;
    while (valid && pos < len) {
        ULONGLONG tag;
        if (!PbReadVarint(data, len, pos, tag)) {
            valid = false;
            break;
        }
        const int field = (int)(tag >> 3);
        const int wireType = (int)(tag & 7);
        if (wireType == 0) {
            ULONGLONG v;
            valid = PbReadVarint(data, len, pos, v);
        } else if (wireType == 2) {
            ULONGLONG fieldLen;
            if (!PbReadVarint(data, len, pos, fieldLen) || fieldLen > len - pos) {
                valid = false;
                break;
            }
            if (field == 2) {
                hasResponse = true;
            } else if (field == 3) {
                hasError = true;
            }
            pos += (size_t)fieldLen;
        } else {
            valid = false;
        }
    }

    if (!valid || !hasResponse || hasError) {
        TRACE(_T("CastSession: device auth failed\n"));
        CASTING_LOG(_T("session: the device refused our authentication"));
        m_dead = true;
        return;
    }

    // step 2 of the bring-up: connect to the platform receiver and query its status
    SetState(CastSessionState::Connecting);
    SendJson(CAST_NS_CONNECTION, CAST_RECEIVER_ID, "{\"type\":\"CONNECT\"}");
    SendReceiverGetStatus();
}

void CCastSession::OnReceiverMessage(const CStringA& type, const rapidjson::Value& d)
{
    if (type == "RECEIVER_STATUS") {
        // find our application and its transportId in the status
        CStringA transportId, sessionId;
        auto itStatus = d.FindMember("status");
        if (itStatus != d.MemberEnd() && itStatus->value.IsObject()) {
            auto itApps = itStatus->value.FindMember("applications");
            if (itApps != itStatus->value.MemberEnd() && itApps->value.IsArray()) {
                for (const auto& app : itApps->value.GetArray()) {
                    if (app.IsObject() && GetJsonString(app, "appId") == CAST_APP_ID) {
                        transportId = GetJsonString(app, "transportId");
                        sessionId = GetJsonString(app, "sessionId");
                        break;
                    }
                }
            }
        }

        CastSessionState state = GetState();
        if (state == CastSessionState::Connecting) {
            SetState(CastSessionState::Connected);
            state = CastSessionState::Connected;
        }

        if (state == CastSessionState::Connected || state == CastSessionState::Launching) {
            if (!transportId.IsEmpty()) {
                // step 4 of the bring-up: second CONNECT addressed to the app;
                // only afterwards does the MEDIA namespace work
                m_transportId = transportId;
                m_sessionId = sessionId;
                SendJson(CAST_NS_CONNECTION, m_transportId, "{\"type\":\"CONNECT\"}");
                SetState(CastSessionState::Ready);
            } else if (state == CastSessionState::Connected) {
                // step 3: the app is not running yet, launch it
                CStringA json;
                json.Format("{\"type\":\"LAUNCH\",\"appId\":\"" CAST_APP_ID "\",\"requestId\":%d}",
                            NextRequestId());
                SendJson(CAST_NS_RECEIVER, CAST_RECEIVER_ID, json);
                SetState(CastSessionState::Launching);
            }
        } else if (!m_transportId.IsEmpty() && transportId.IsEmpty()) {
            // our app is gone from the receiver status
            TRACE(_T("CastSession: receiver app is gone\n"));
            CASTING_LOG(_T("session: our application is no longer running on the device"));
            ResetMediaSession();
            SetState(CastSessionState::Connected);
        }
    } else if (type == "LAUNCH_ERROR") {
        TRACE(_T("CastSession: LAUNCH_ERROR\n"));
        CASTING_LOG(_T("session: the device refused to launch the media receiver (LAUNCH_ERROR)"));
        m_dead = true;
    }
}

void CCastSession::OnMediaMessage(const CStringA& type, const rapidjson::Value& d, int requestId)
{
    // Whatever the receiver says on the media namespace, a pending LOAD has
    // been resolved by it and the status poll may resume. A refusal carrying
    // the requestId of that very LOAD is the load failing, while one carrying
    // any other belongs to a request the device has already moved past.
    const bool answersLoad = m_loadRequestId != 0 && requestId == m_loadRequestId;
    m_loadRequestId = 0;

    if (type == "MEDIA_STATUS") {
        auto itStatus = d.FindMember("status");
        if (itStatus == d.MemberEnd() || !itStatus->value.IsArray()) {
            return;
        }
        for (const auto& status : itStatus->value.GetArray()) {
            if (!status.IsObject()) {
                continue;
            }
            const int mediaSessionId = GetJsonInt(status, "mediaSessionId", 0);
            // gate everything by the known mediaSessionId so that statuses of
            // a usurping sender's session are refused
            if (m_mediaSessionId != 0 && mediaSessionId != 0 && mediaSessionId != m_mediaSessionId) {
                continue;
            }
            const CStringA playerState = GetJsonString(status, "playerState");
            const CastSessionState state = GetState();

            if (playerState == "IDLE") {
                const CStringA idleReason = GetJsonString(status, "idleReason");
                CASTING_LOG(_T("session: the device reports IDLE%s%hs"),
                            idleReason.IsEmpty() ? _T("") : _T(", idleReason "), idleReason.GetString());
                if (idleReason == "INTERRUPTED") {
                    if (state == CastSessionState::Ready || state == CastSessionState::Loading
                            || state == CastSessionState::LoadFailed) {
                        // our own new LOAD interrupted the previous session
                    } else {
                        // another sender took the receiver over; keep the stale
                        // mediaSessionId so its statuses stay refused
                        SetState(CastSessionState::TakenOver);
                    }
                } else if (idleReason == "ERROR") {
                    SetState(CastSessionState::LoadFailed);
                } else if (idleReason == "FINISHED") {
                    SetState(CastSessionState::Stopped); // end of media
                } else if (idleReason == "CANCELLED") {
                    SetState(CastSessionState::Stopped);
                } else if (state == CastSessionState::Buffering) {
                    // a load failure that manifests as a bare IDLE status
                    SetState(CastSessionState::LoadFailed);
                }
                continue;
            }

            // first non-IDLE status of a new load carries the mediaSessionId
            if (m_mediaSessionId == 0 && mediaSessionId != 0) {
                CASTING_LOG(_T("session: the device took the media, playerState %hs, mediaSessionId %d"),
                            playerState.GetString(), mediaSessionId);
                m_mediaSessionId = mediaSessionId;
                if (SendDeferredCommands()) {
                    continue; // a deferred STOP ended this media session
                }
            }

            double duration = 0.0;
            auto itMedia = status.FindMember("media");
            if (itMedia != status.MemberEnd() && itMedia->value.IsObject()) {
                duration = GetJsonDouble(itMedia->value, "duration", 0.0);
            }
            UpdateMediaTime(GetJsonDouble(status, "currentTime", -1.0), duration);

            if (playerState == "PLAYING") {
                SetState(CastSessionState::Playing);
            } else if (playerState == "PAUSED") {
                SetState(CastSessionState::Paused);
            } else if (playerState == "BUFFERING") {
                SetState(CastSessionState::Buffering);
            }
        }
    } else if (type == "LOAD_FAILED") {
        SetState(CastSessionState::LoadFailed);
    } else if (type == "LOAD_CANCELLED") {
        // A newer LOAD superseded an earlier one, which only our own can have
        // done, and the status of that newer one follows. The load still
        // outstanding being cancelled means no media will ever play.
        if (answersLoad) {
            SetState(CastSessionState::LoadFailed);
        }
    } else if (type == "INVALID_REQUEST" || type == "ERROR") {
        TRACE(_T("CastSession: media request failed (%hs)\n"), type.GetString());
        CASTING_LOG(_T("session: the device refused a media request (%hs)%s"), type.GetString(),
                    answersLoad ? _T(", which was the LOAD") : _T(""));
        // a refused LOAD would otherwise leave the session loading forever
        if (answersLoad) {
            SetState(CastSessionState::LoadFailed);
        }
    }
}

void CCastSession::ProcessCommands()
{
    std::vector<Command> commands;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        commands.swap(m_commands);
    }

    for (const Command& cmd : commands) {
        switch (cmd.type) {
            case Command::Type::Load: {
                if (m_transportId.IsEmpty()) {
                    TRACE(_T("CastSession: LOAD ignored, no receiver app\n"));
                    CASTING_LOG(_T("session: the LOAD was dropped, no receiver application is running"));
                    break;
                }
                m_mediaSessionId = 0;
                m_stopMediaDeferred = false;
                m_playPauseDeferred = false;
                m_deferredSeek = -1.0;
                {
                    std::lock_guard<std::mutex> lock(m_mutex);
                    m_mediaTime = 0.0;
                    m_mediaTimeTick = 0;
                    m_mediaDuration = cmd.param > 0.0 ? cmd.param : 0.0;
                }
                CStringA json;
                json.Format("{\"type\":\"LOAD\",\"media\":{\"contentId\":%s,"
                            "\"streamType\":\"BUFFERED\",\"contentType\":%s",
                            JSONString(cmd.url).GetString(), JSONString(cmd.mime).GetString());
                if (cmd.param > 0.0) {
                    json.AppendFormat(",\"duration\":%.6f", cmd.param);
                }
                m_loadRequestId = NextRequestId();
                json.AppendFormat(",\"metadata\":{\"metadataType\":0,\"title\":%s}},\"requestId\":%d}",
                                  JSONString(cmd.title).GetString(), m_loadRequestId);
                SendJson(CAST_NS_MEDIA, m_transportId, json);
                SetState(CastSessionState::Loading);
                break;
            }
            case Command::Type::Play:
            case Command::Type::Pause: {
                if (m_transportId.IsEmpty()) {
                    break;
                }
                if (!m_mediaSessionId) {
                    // the UI has already committed to the new state, so the
                    // intent is kept and replayed once the device answers;
                    // the latest of a play/pause pair wins
                    m_playPauseDeferred = true;
                    m_deferredPlay = cmd.type == Command::Type::Play;
                    break;
                }
                CStringA json;
                json.Format("{\"type\":\"%s\",\"mediaSessionId\":%d,\"requestId\":%d}",
                            cmd.type == Command::Type::Play ? "PLAY" : "PAUSE",
                            m_mediaSessionId, NextRequestId());
                SendJson(CAST_NS_MEDIA, m_transportId, json);
                break;
            }
            case Command::Type::Stop:
                if (m_transportId.IsEmpty() || !IsMediaActiveState(GetState())) {
                    // nothing is loaded, release anyone waiting for the stop
                    if (m_hMediaStopSentEvent) {
                        SetEvent(m_hMediaStopSentEvent);
                    }
                    break;
                }
                if (!m_mediaSessionId) {
                    // defer until the first media status delivers the sessionId
                    m_stopMediaDeferred = true;
                } else {
                    SendMediaStop();
                }
                break;
            case Command::Type::Seek: {
                if (m_transportId.IsEmpty()) {
                    break;
                }
                if (!m_mediaSessionId) {
                    m_deferredSeek = cmd.param; // only the latest target matters
                    break;
                }
                CStringA json;
                json.Format("{\"type\":\"SEEK\",\"mediaSessionId\":%d,\"currentTime\":%.6f,"
                            "\"resumeState\":\"PLAYBACK_START\",\"requestId\":%d}",
                            m_mediaSessionId, cmd.param, NextRequestId());
                SendJson(CAST_NS_MEDIA, m_transportId, json);
                break;
            }
            case Command::Type::SetVolume: {
                CStringA json;
                json.Format("{\"type\":\"SET_VOLUME\",\"volume\":{\"level\":%.6f,\"muted\":%s},"
                            "\"requestId\":%d}",
                            cmd.param, cmd.muted ? "true" : "false", NextRequestId());
                SendJson(CAST_NS_RECEIVER, CAST_RECEIVER_ID, json);
                break;
            }
        }
        if (m_dead) {
            break;
        }
    }
}
