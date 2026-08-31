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
#include "CastMediaServer.h"
#include <PathUtils.h>
#include <ws2tcpip.h>
#include <bcrypt.h>
#include <algorithm>
#include <climits>
#include <vector>

#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib, "bcrypt.lib")

#define MAX_CLIENTS            8            // the device holds only a few connections
#define MAX_HEADER_SIZE        (16 * 1024)  // larger request headers get a 431
#define SEND_CHUNK_SIZE        (256 * 1024) // file bytes buffered per send round
#define CLIENT_IDLE_TIMEOUT_MS 30000ull
#define FILE_READ_TIMEOUT_MS   5000         // a read that takes longer is abandoned

// DLNA renderers are fussier than a Chromecast about who they are talking to
#define HTTP_SERVER_HEADER     "Server: Windows UPnP/1.0 DLNA/1.50 MPC-HC\r\n"

// Per-connection state. Each connection reads its file through its own handle
// so that concurrent range requests do not fight over one file position, and
// keeps that handle open across keep-alive requests.
struct CCastMediaServer::Client {
    SOCKET sock = INVALID_SOCKET;
    WSAEVENT hEvent = WSA_INVALID_EVENT;
    CStringA recvBuf;            // request bytes not yet consumed
    HANDLE hFile = INVALID_HANDLE_VALUE;
    CString filePath;            // file hFile was opened for
    HANDLE hReadEvent = nullptr; // completion event of the overlapped read
    ULONGLONG fileOffset = 0;    // next file position to read
    ULONGLONG bytesRemaining = 0; // body bytes not yet moved into outBuf
    std::vector<BYTE> outBuf;    // response bytes not yet accepted by send()
    size_t outPos = 0;
    bool sending = false;        // a response is in progress
    bool closeAfterResponse = false;
    bool remoteClosed = false;   // peer sent FIN; finish the response, then close
    ULONGLONG lastActivity = 0;  // GetTickCount64() of the last successful I/O
};

namespace
{
    enum class RangeResult {
        None,          // no Range header, or one that must be ignored (invalid syntax)
        Valid,         // start/end describe a satisfiable byte range
        Unsatisfiable, // respond with 416
    };

    // Parses a Range request header value against the file size. Multi-range
    // requests are answered with the first range only (multipart/byteranges
    // is deliberately not implemented; the cast device never asks for it).
    RangeResult ParseRangeHeader(CStringA value, ULONGLONG fileSize, ULONGLONG& start, ULONGLONG& end)
    {
        value.Trim();
        if (value.Left(6).CompareNoCase("bytes=") != 0) {
            return RangeResult::None;
        }
        value = value.Mid(6);
        int comma = value.Find(',');
        if (comma >= 0) {
            value = value.Left(comma);
        }
        value.Trim();

        int dash = value.Find('-');
        if (dash < 0) {
            return RangeResult::None;
        }
        CStringA firstStr = value.Left(dash);
        CStringA lastStr = value.Mid(dash + 1);
        firstStr.Trim();
        lastStr.Trim();

        auto isDigits = [](const CStringA& s) {
            if (s.IsEmpty() || s.GetLength() > 19) { // ULONGLONG has at most 20 digits
                return false;
            }
            for (int i = 0; i < s.GetLength(); i++) {
                if (s[i] < '0' || s[i] > '9') {
                    return false;
                }
            }
            return true;
        };

        if (firstStr.IsEmpty()) {
            // suffix range: the last N bytes of the file
            if (!isDigits(lastStr)) {
                return RangeResult::None;
            }
            ULONGLONG suffixLen = _strtoui64(lastStr, nullptr, 10);
            if (suffixLen == 0 || fileSize == 0) {
                return RangeResult::Unsatisfiable;
            }
            start = suffixLen >= fileSize ? 0 : fileSize - suffixLen;
            end = fileSize - 1;
            return RangeResult::Valid;
        }

        if (!isDigits(firstStr)) {
            return RangeResult::None;
        }
        start = _strtoui64(firstStr, nullptr, 10);
        if (lastStr.IsEmpty()) {
            end = ULLONG_MAX;
        } else {
            if (!isDigits(lastStr)) {
                return RangeResult::None;
            }
            end = _strtoui64(lastStr, nullptr, 10);
            if (end < start) {
                return RangeResult::None; // invalid spec, ignore the header
            }
        }
        if (start >= fileSize) {
            return RangeResult::Unsatisfiable;
        }
        end = std::min(end, fileSize - 1);
        return RangeResult::Valid;
    }

    // Returns the value of the named header, or an empty string. The header
    // block is the request without the terminating blank line.
    CStringA GetHeaderValue(const CStringA& header, const char* name)
    {
        int nameLen = (int)strlen(name);
        int pos = header.Find("\r\n");
        while (pos >= 0) {
            pos += 2;
            int lineEnd = header.Find("\r\n", pos);
            CStringA line = lineEnd >= 0 ? header.Mid(pos, lineEnd - pos) : header.Mid(pos);
            int colon = line.Find(':');
            if (colon == nameLen && line.Left(nameLen).CompareNoCase(name) == 0) {
                CStringA value = line.Mid(colon + 1);
                value.Trim();
                return value;
            }
            pos = lineEnd;
        }
        return CStringA();
    }

    // Current time as an HTTP-date; DLNA requires a Date header on responses
    CStringA HttpDate()
    {
        static const char* const days[] = { "Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat" };
        static const char* const months[] = { "Jan", "Feb", "Mar", "Apr", "May", "Jun",
                                              "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"
                                            };
        SYSTEMTIME st;
        GetSystemTime(&st);
        CStringA date;
        date.Format("Date: %s, %02u %s %04u %02u:%02u:%02u GMT\r\n",
                    days[st.wDayOfWeek % 7], st.wDay, months[(st.wMonth - 1) % 12],
                    st.wYear, st.wHour, st.wMinute, st.wSecond);
        return date;
    }

    // Lowercase alphanumeric file extension without the dot, empty if none
    CStringA FileExtLower(const CString& path)
    {
        const CString ext = PathUtils::FileExt(path).Mid(1); // drops the dot
        CStringA extA;
        for (int i = 0; i < ext.GetLength() && i < 8; i++) {
            TCHAR ch = ext[i];
            if ((ch >= _T('a') && ch <= _T('z')) || (ch >= _T('0') && ch <= _T('9'))) {
                extA += (char)ch;
            } else if (ch >= _T('A') && ch <= _T('Z')) {
                extA += (char)(ch - _T('A') + 'a');
            } else {
                return CStringA();
            }
        }
        return extA;
    }
}

CCastMediaServer::CCastMediaServer()
{
}

CCastMediaServer::~CCastMediaServer()
{
    Stop();
}

UINT CCastMediaServer::preferredPort = 13580;

bool CCastMediaServer::Start()
{
    if (m_hThread) {
        return true;
    }

    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        return false;
    }
    m_bWsaInitialized = true;

    m_listenSocket = OpenListenSocket();
    if (m_listenSocket == INVALID_SOCKET) {
        Stop();
        return false;
    }

    m_hStopEvent = CreateEvent(nullptr, TRUE, FALSE, nullptr);
    if (!m_hStopEvent) {
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

void CCastMediaServer::Stop()
{
    if (m_hThread) {
        SetEvent(m_hStopEvent);
        // The worker dereferences this object throughout (the listening socket,
        // the stop event, the guarded file registration), so there is nothing
        // that could be detached and leaked if it overran: the join has to be
        // unconditional or the object is freed underneath a live thread. Every
        // blocking operation the worker performs is bounded and watches the
        // stop event, so the first wait is only a debug tripwire.
        if (WaitForSingleObject(m_hThread, 10000) != WAIT_OBJECT_0) {
            ASSERT(FALSE);
            WaitForSingleObject(m_hThread, INFINITE);
        }
        CloseHandle(m_hThread);
        m_hThread = nullptr;
    }
    if (m_listenSocket != INVALID_SOCKET) {
        closesocket(m_listenSocket);
        m_listenSocket = INVALID_SOCKET;
    }
    if (m_hStopEvent) {
        CloseHandle(m_hStopEvent);
        m_hStopEvent = nullptr;
    }
    if (m_bWsaInitialized) {
        WSACleanup();
        m_bWsaInitialized = false;
    }

    std::lock_guard<std::mutex> lock(m_mutex);
    m_filePath.Empty();
    m_mime.Empty();
    m_contentFeatures.Empty();
    m_urlPath.Empty();
    m_allowedPeer.Empty();
    m_port = 0;
}

UINT CCastMediaServer::GetPort() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_port;
}

void CCastMediaServer::SetFile(const CString& filePath, const CStringA& mime, const CStringA& contentFeatures)
{
    // Randomize the URL path on every registration: cast devices cache media
    // by URL, and a fresh path also invalidates any previously handed-out URL.
    BYTE rnd[8];
    if (BCryptGenRandom(nullptr, rnd, sizeof(rnd), BCRYPT_USE_SYSTEM_PREFERRED_RNG) != 0 /*STATUS_SUCCESS*/) {
        // extremely unlikely; degrade to a time-based token
        ULONGLONG fallback = GetTickCount64();
        LARGE_INTEGER qpc;
        QueryPerformanceCounter(&qpc);
        fallback ^= (ULONGLONG)qpc.QuadPart * 0x9E3779B97F4A7C15ull;
        memcpy(rnd, &fallback, sizeof(rnd));
    }
    CStringA token;
    for (BYTE b : rnd) {
        token.AppendFormat("%02x", b);
    }

    CStringA ext = FileExtLower(filePath);
    if (ext.IsEmpty()) {
        ext = "bin";
    }

    std::lock_guard<std::mutex> lock(m_mutex);
    m_filePath = filePath;
    m_mime = mime;
    m_contentFeatures = contentFeatures;
    m_urlPath.Format("/cast/%s/media.%s", token.GetString(), ext.GetString());
}

void CCastMediaServer::ClearFile()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_filePath.Empty();
    m_mime.Empty();
    m_contentFeatures.Empty();
    m_urlPath.Empty();
}

CStringA CCastMediaServer::GetURLForHost(const CStringA& localIp) const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_urlPath.IsEmpty() || localIp.IsEmpty() || !m_port) {
        return CStringA();
    }
    CStringA url;
    url.Format("http://%s:%u%s", localIp.GetString(), m_port, m_urlPath.GetString());
    return url;
}

void CCastMediaServer::SetAllowedPeer(const CStringA& ip)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_allowedPeer = ip;
}

void CCastMediaServer::ClearAllowedPeer()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_allowedPeer.Empty();
}

const char* const CCastMediaServer::dlnaContentFeatures =
    "DLNA.ORG_OP=01;DLNA.ORG_CI=0;DLNA.ORG_FLAGS=01700000000000000000000000000000";

bool CCastMediaServer::IsCastableFile(const CString& path)
{
    // The containers the default media receiver lists (MP2T, MP3, MP4, OGG,
    // WAV, WebM), audio ones included so that a speaker with Chromecast built
    // in can be cast to. MKV is excluded: the receiver does not accept it.
    // .m2ts and .mts are left out on purpose -- they are the 192-byte
    // timestamped variant of MPEG-2 TS, which is a different thing from the
    // 188-byte transport stream MP2T means. What is inside the container is
    // judged separately, by the target that knows the device.
    const CStringA ext = FileExtLower(path);
    return ext == "mp4" || ext == "m4v" || ext == "webm" || ext == "ts"
           || ext == "mp3" || ext == "m4a" || ext == "aac" || ext == "flac"
           || ext == "wav" || ext == "ogg" || ext == "opus";
}

CStringA CCastMediaServer::MimeForFile(const CString& path)
{
    // Feeds the Content-Type header, and on DLNA also the matching against
    // what the renderer reports it accepts, so the table covers more than a
    // Chromecast can play.
    static const struct {
        const char* ext;
        const char* mime;
    } types[] = {
        { "mp4",  "video/mp4" },
        { "m4v",  "video/mp4" },
        { "webm", "video/webm" },
        { "mkv",  "video/x-matroska" },
        { "avi",  "video/x-msvideo" },
        { "ts",   "video/mp2t" },
        { "m2ts", "video/mp2t" },
        { "mts",  "video/mp2t" },
        { "mpg",  "video/mpeg" },
        { "mpeg", "video/mpeg" },
        { "mov",  "video/quicktime" },
        { "mp3",  "audio/mpeg" },
        { "m4a",  "audio/mp4" },
        { "aac",  "audio/aac" },
        { "flac", "audio/flac" },
        { "wav",  "audio/wav" },
        { "ogg",  "audio/ogg" },
        { "oga",  "audio/ogg" },
        { "opus", "audio/opus" },
        { "wma",  "audio/x-ms-wma" },
    };

    const CStringA ext = FileExtLower(path);
    for (const auto& type : types) {
        if (ext == type.ext) {
            return type.mime;
        }
    }
    return "application/octet-stream";
}

// --- server thread ---

DWORD WINAPI CCastMediaServer::StaticThreadProc(LPVOID lpParam)
{
    SetThreadName(DWORD(-1), "CastMediaServer Thread");
    return ((CCastMediaServer*)lpParam)->ThreadProc();
}

DWORD CCastMediaServer::ThreadProc()
{
    // WSAEventSelect also puts the socket into non-blocking mode
    WSAEVENT hListenEvent = WSACreateEvent();
    if (hListenEvent == WSA_INVALID_EVENT
            || WSAEventSelect(m_listenSocket, hListenEvent, FD_ACCEPT) == SOCKET_ERROR) {
        if (hListenEvent != WSA_INVALID_EVENT) {
            WSACloseEvent(hListenEvent);
        }
        return DWORD_ERROR;
    }

    std::list<Client> clients;
    std::vector<HANDLE> handles;
    for (;;) {
        handles.clear();
        handles.emplace_back(m_hStopEvent);
        handles.emplace_back(hListenEvent);
        for (const Client& c : clients) {
            handles.emplace_back(c.hEvent);
        }

        // sleep until the nearest idle timeout expires; with no connection open
        // there is no deadline at all and only an event can wake the thread
        DWORD timeout = INFINITE;
        const ULONGLONG tick = GetTickCount64();
        for (const Client& c : clients) {
            const ULONGLONG deadline = c.lastActivity + CLIENT_IDLE_TIMEOUT_MS;
            timeout = (DWORD)std::min<ULONGLONG>(timeout, deadline > tick ? deadline - tick : 0);
        }

        DWORD ret = WaitForMultipleObjects((DWORD)handles.size(), handles.data(), FALSE, timeout);
        if (ret == WAIT_OBJECT_0) {
            break; // stop requested
        } else if (ret == WAIT_OBJECT_0 + 1) {
            WSANETWORKEVENTS ne;
            WSAEnumNetworkEvents(m_listenSocket, hListenEvent, &ne);
            AcceptClients(clients);
        } else if (ret > WAIT_OBJECT_0 + 1 && ret < WAIT_OBJECT_0 + handles.size()) {
            auto it = clients.begin();
            std::advance(it, ret - WAIT_OBJECT_0 - 2);
            OnClientEvent(*it);
        }

        // drop timed-out connections and purge closed ones
        const ULONGLONG now = GetTickCount64();
        for (auto it = clients.begin(); it != clients.end();) {
            if (it->sock != INVALID_SOCKET && now - it->lastActivity > CLIENT_IDLE_TIMEOUT_MS) {
                CloseClient(*it);
            }
            it = it->sock == INVALID_SOCKET ? clients.erase(it) : std::next(it);
        }
    }

    for (Client& c : clients) {
        CloseClient(c);
    }
    WSACloseEvent(hListenEvent);
    return 0;
}

SOCKET CCastMediaServer::OpenListenSocket()
{
    SOCKET sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (sock == INVALID_SOCKET) {
        return INVALID_SOCKET;
    }

    sockaddr_in addr;
    ZeroMemory(&addr, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons((USHORT)preferredPort);
    if (bind(sock, (sockaddr*)&addr, sizeof(addr)) == SOCKET_ERROR) {
        // the preferred port is taken, fall back to an ephemeral one
        addr.sin_port = 0;
        if (bind(sock, (sockaddr*)&addr, sizeof(addr)) == SOCKET_ERROR) {
            closesocket(sock);
            return INVALID_SOCKET;
        }
    }
    if (listen(sock, SOMAXCONN) == SOCKET_ERROR) {
        closesocket(sock);
        return INVALID_SOCKET;
    }

    int addrLen = sizeof(addr);
    if (getsockname(sock, (sockaddr*)&addr, &addrLen) == SOCKET_ERROR) {
        closesocket(sock);
        return INVALID_SOCKET;
    }

    std::lock_guard<std::mutex> lock(m_mutex);
    m_port = ntohs(addr.sin_port);
    return sock;
}

void CCastMediaServer::AcceptClients(std::list<Client>& clients)
{
    for (;;) {
        sockaddr_in addr;
        int addrLen = sizeof(addr);
        SOCKET sock = accept(m_listenSocket, (sockaddr*)&addr, &addrLen);
        if (sock == INVALID_SOCKET) {
            break; // WSAEWOULDBLOCK when drained, or an error
        }

        char ipBuf[16] = { 0 };
        inet_ntop(AF_INET, (PVOID)&addr.sin_addr, ipBuf, sizeof(ipBuf));
        CStringA allowedPeer;
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            allowedPeer = m_allowedPeer;
        }
        if (!allowedPeer.IsEmpty() && allowedPeer != ipBuf) {
            TRACE(_T("CastMediaServer: rejected connection from %hs\n"), ipBuf);
            closesocket(sock);
            continue;
        }
        if (clients.size() >= MAX_CLIENTS) {
            closesocket(sock);
            continue;
        }

        WSAEVENT hEvent = WSACreateEvent();
        HANDLE hReadEvent = CreateEvent(nullptr, TRUE, FALSE, nullptr);
        if (hEvent == WSA_INVALID_EVENT || !hReadEvent
                || WSAEventSelect(sock, hEvent, FD_READ | FD_WRITE | FD_CLOSE) == SOCKET_ERROR) {
            if (hEvent != WSA_INVALID_EVENT) {
                WSACloseEvent(hEvent);
            }
            if (hReadEvent) {
                CloseHandle(hReadEvent);
            }
            closesocket(sock);
            continue;
        }

        clients.emplace_back();
        Client& c = clients.back();
        c.sock = sock;
        c.hEvent = hEvent;
        c.hReadEvent = hReadEvent;
        c.lastActivity = GetTickCount64();
    }
}

void CCastMediaServer::CloseClient(Client& client)
{
    // Disconnects mid-transfer are routine (the device aborts reads when
    // seeking), so connections are torn down silently.
    if (client.sock != INVALID_SOCKET) {
        closesocket(client.sock);
        client.sock = INVALID_SOCKET;
    }
    if (client.hEvent != WSA_INVALID_EVENT) {
        WSACloseEvent(client.hEvent);
        client.hEvent = WSA_INVALID_EVENT;
    }
    if (client.hFile != INVALID_HANDLE_VALUE) {
        CloseHandle(client.hFile);
        client.hFile = INVALID_HANDLE_VALUE;
        client.filePath.Empty();
    }
    if (client.hReadEvent) {
        CloseHandle(client.hReadEvent);
        client.hReadEvent = nullptr;
    }
}

// Opens the registered file for this connection, reusing the handle already
// held when a keep-alive connection asks for the same file again.
bool CCastMediaServer::EnsureFileOpen(Client& client, const CString& filePath)
{
    if (client.hFile != INVALID_HANDLE_VALUE) {
        if (client.filePath == filePath) {
            return true;
        }
        CloseHandle(client.hFile);
        client.hFile = INVALID_HANDLE_VALUE;
        client.filePath.Empty();
    }

    // FILE_FLAG_OVERLAPPED keeps every read interruptible by the stop event.
    // No sequential hint: the device probes the file with random-access range
    // requests rather than reading it front to back.
    HANDLE hFile = CreateFile(filePath, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_DELETE,
                              nullptr, OPEN_EXISTING, FILE_FLAG_OVERLAPPED, nullptr);
    if (hFile == INVALID_HANDLE_VALUE) {
        return false;
    }
    client.hFile = hFile;
    client.filePath = filePath;
    return true;
}

void CCastMediaServer::OnClientEvent(Client& client)
{
    WSANETWORKEVENTS ne;
    ZeroMemory(&ne, sizeof(ne));
    WSAEnumNetworkEvents(client.sock, client.hEvent, &ne);

    // drain everything the peer sent
    for (;;) {
        char buf[4096];
        int len = recv(client.sock, buf, sizeof(buf), 0);
        if (len > 0) {
            if (client.recvBuf.GetLength() + len > 2 * MAX_HEADER_SIZE) {
                CloseClient(client); // flooding, no well-formed request pipelines this much
                return;
            }
            client.recvBuf.Append(buf, len);
            client.lastActivity = GetTickCount64();
        } else if (len == 0) {
            client.remoteClosed = true;
            break;
        } else {
            if (WSAGetLastError() != WSAEWOULDBLOCK) {
                CloseClient(client);
                return;
            }
            break;
        }
    }
    if (ne.lNetworkEvents & FD_CLOSE) {
        client.remoteClosed = true;
    }

    if (ne.lNetworkEvents & FD_WRITE) {
        PumpSend(client);
    }
    if (client.sock != INVALID_SOCKET) {
        ProcessRequests(client);
    }
    if (client.sock != INVALID_SOCKET && client.remoteClosed && !client.sending) {
        // the peer sent FIN and no response is pending anymore
        CloseClient(client);
    }
}

void CCastMediaServer::ProcessRequests(Client& client)
{
    // handle buffered requests one at a time; the next one (keep-alive or
    // pipelined) is only parsed once the current response has been sent
    while (client.sock != INVALID_SOCKET && !client.sending) {
        int headerEnd = client.recvBuf.Find("\r\n\r\n");
        if (headerEnd < 0) {
            if (client.recvBuf.GetLength() > MAX_HEADER_SIZE) {
                SendSimpleResponse(client, 431, "Request Header Fields Too Large", CStringA(), true);
            }
            return;
        }
        CStringA header = client.recvBuf.Left(headerEnd);
        client.recvBuf.Delete(0, headerEnd + 4);
        HandleRequest(client, header);
    }
}

void CCastMediaServer::HandleRequest(Client& client, const CStringA& header)
{
    int lineEnd = header.Find("\r\n");
    CStringA requestLine = lineEnd >= 0 ? header.Left(lineEnd) : header;
    int sp1 = requestLine.Find(' ');
    int sp2 = sp1 >= 0 ? requestLine.Find(' ', sp1 + 1) : -1;
    if (sp1 <= 0 || sp2 <= sp1 + 1) {
        SendSimpleResponse(client, 400, "Bad Request", CStringA(), true);
        return;
    }
    const CStringA method = requestLine.Left(sp1);
    const CStringA target = requestLine.Mid(sp1 + 1, sp2 - sp1 - 1);
    const CStringA version = requestLine.Mid(sp2 + 1);

    // HTTP/1.1 defaults to keep-alive, HTTP/1.0 to close
    bool keepAlive = version.CompareNoCase("HTTP/1.0") != 0;
    CStringA connection = GetHeaderValue(header, "Connection");
    connection.MakeLower();
    if (connection.Find("close") >= 0) {
        keepAlive = false;
    } else if (connection.Find("keep-alive") >= 0) {
        keepAlive = true;
    }
    client.closeAfterResponse = !keepAlive;

    if (method != "GET" && method != "HEAD") {
        SendSimpleResponse(client, 405, "Method Not Allowed", "Allow: GET, HEAD\r\n", !keepAlive);
        return;
    }

    // The request path is compared against the registered randomized path
    // exactly; nothing is ever resolved against the filesystem from request
    // input, so path traversal is structurally impossible.
    CString filePath;
    CStringA mime, contentFeatures;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (m_urlPath.IsEmpty() || target != m_urlPath) {
            filePath.Empty();
        } else {
            filePath = m_filePath;
            mime = m_mime;
            contentFeatures = m_contentFeatures;
        }
    }
    if (filePath.IsEmpty()) {
        SendSimpleResponse(client, 404, "Not Found", CStringA(), !keepAlive);
        return;
    }

    if (!EnsureFileOpen(client, filePath)) {
        SendSimpleResponse(client, 404, "Not Found", CStringA(), !keepAlive);
        return;
    }
    LARGE_INTEGER liSize;
    if (!GetFileSizeEx(client.hFile, &liSize) || liSize.QuadPart < 0) {
        SendSimpleResponse(client, 500, "Internal Server Error", CStringA(), !keepAlive);
        return;
    }
    const ULONGLONG fileSize = (ULONGLONG)liSize.QuadPart;

    ULONGLONG start = 0, end = fileSize ? fileSize - 1 : 0;
    RangeResult range = ParseRangeHeader(GetHeaderValue(header, "Range"), fileSize, start, end);
    if (range == RangeResult::Unsatisfiable) {
        CStringA contentRange;
        contentRange.Format("Content-Range: bytes */%I64u\r\n", fileSize);
        SendSimpleResponse(client, 416, "Range Not Satisfiable", contentRange, !keepAlive);
        return;
    }
    const ULONGLONG bodyLen = fileSize ? end - start + 1 : 0;

    CStringA responseHeader;
    if (range == RangeResult::Valid) {
        responseHeader.Format("HTTP/1.1 206 Partial Content\r\n"
                              "Content-Range: bytes %I64u-%I64u/%I64u\r\n", start, end, fileSize);
    } else {
        responseHeader = "HTTP/1.1 200 OK\r\n";
    }
    // A DLNA renderer asks what the resource supports and how it wants it
    // transferred, and will not play without the answers. A Chromecast sends
    // neither header and so is served exactly what it was served before.
    CStringA dlnaHeaders;
    if (GetHeaderValue(header, "getcontentFeatures.dlna.org") == "1") {
        dlnaHeaders.AppendFormat("contentFeatures.dlna.org: %s\r\n",
                                 contentFeatures.IsEmpty() ? dlnaContentFeatures : contentFeatures.GetString());
    }
    if (!GetHeaderValue(header, "transferMode.dlna.org").IsEmpty()) {
        const bool streaming = mime.Left(6).CompareNoCase("audio/") == 0
                               || mime.Left(6).CompareNoCase("video/") == 0;
        dlnaHeaders.AppendFormat("transferMode.dlna.org: %s\r\n", streaming ? "Streaming" : "Interactive");
    }

    responseHeader.AppendFormat(HTTP_SERVER_HEADER
                                "%s"
                                "Accept-Ranges: bytes\r\n"
                                "Content-Type: %s\r\n"
                                "Content-Length: %I64u\r\n"
                                "%s"
                                "Connection: %s\r\n"
                                "\r\n",
                                HttpDate().GetString(),
                                mime.IsEmpty() ? "application/octet-stream" : mime.GetString(),
                                bodyLen, dlnaHeaders.GetString(), keepAlive ? "keep-alive" : "close");

    TRACE(_T("CastMediaServer: %hs %hs -> %hs"), method.GetString(), target.GetString(),
          responseHeader.Left(responseHeader.Find('\r') + 2).GetString());

    if (method != "HEAD" && bodyLen != 0) {
        client.fileOffset = start;
        client.bytesRemaining = bodyLen;
    }
    QueueResponse(client, responseHeader);
}

void CCastMediaServer::SendSimpleResponse(Client& client, int status, const CStringA& statusText,
                                          const CStringA& extraHeaders, bool closeConnection)
{
    if (closeConnection) {
        client.closeAfterResponse = true;
    }
    CStringA responseHeader;
    responseHeader.Format("HTTP/1.1 %d %s\r\n"
                          HTTP_SERVER_HEADER
                          "%s"
                          "%s"
                          "Content-Length: 0\r\n"
                          "Connection: %s\r\n"
                          "\r\n",
                          status, statusText.GetString(), HttpDate().GetString(),
                          extraHeaders.GetString(),
                          client.closeAfterResponse ? "close" : "keep-alive");
    QueueResponse(client, responseHeader);
}

void CCastMediaServer::QueueResponse(Client& client, const CStringA& headerBytes)
{
    ASSERT(!client.sending && client.outPos >= client.outBuf.size());
    const BYTE* data = reinterpret_cast<const BYTE*>(headerBytes.GetString());
    client.outBuf.assign(data, data + headerBytes.GetLength());
    client.outPos = 0;
    client.sending = true;
    PumpSend(client);
}

bool CCastMediaServer::RefillOutBuf(Client& client)
{
    const DWORD toRead = (DWORD)std::min<ULONGLONG>(SEND_CHUNK_SIZE, client.bytesRemaining);
    client.outBuf.resize(toRead);
    client.outPos = 0;

    OVERLAPPED ov;
    ZeroMemory(&ov, sizeof(ov));
    ov.Offset = (DWORD)client.fileOffset;
    ov.OffsetHigh = (DWORD)(client.fileOffset >> 32);
    ov.hEvent = client.hReadEvent;
    ResetEvent(client.hReadEvent);

    DWORD read = 0;
    if (!ReadFile(client.hFile, client.outBuf.data(), toRead, &read, &ov)) {
        if (GetLastError() != ERROR_IO_PENDING) {
            return false;
        }
        // The media file can live on an unresponsive network share, so the read
        // is waited on together with the stop event and abandoned if either the
        // shutdown or the timeout comes first.
        HANDLE handles[] = { m_hStopEvent, client.hReadEvent };
        if (WaitForMultipleObjects(_countof(handles), handles, FALSE, FILE_READ_TIMEOUT_MS) != WAIT_OBJECT_0 + 1) {
            CancelIoEx(client.hFile, &ov);
            // the cancellation must complete before the OVERLAPPED and the
            // target buffer go out of scope
            GetOverlappedResult(client.hFile, &ov, &read, TRUE);
            return false;
        }
        if (!GetOverlappedResult(client.hFile, &ov, &read, FALSE)) {
            return false;
        }
    }
    if (read == 0) {
        // the file shrank below the promised Content-Length; the connection is
        // closed so the client sees a short body
        return false;
    }
    client.outBuf.resize(read);
    client.fileOffset += read;
    client.bytesRemaining -= read;
    return true;
}

void CCastMediaServer::PumpSend(Client& client)
{
    while (client.sending) {
        if (client.outPos >= client.outBuf.size()) {
            if (client.bytesRemaining == 0) {
                // response complete; the file handle stays open for the next
                // request on this connection
                client.outBuf.clear();
                client.outPos = 0;
                client.sending = false;
                if (client.closeAfterResponse) {
                    CloseClient(client);
                }
                return;
            }
            if (client.hFile == INVALID_HANDLE_VALUE || !RefillOutBuf(client)) {
                CloseClient(client);
                return;
            }
        }
        int sent = send(client.sock, reinterpret_cast<const char*>(client.outBuf.data()) + client.outPos,
                        (int)(client.outBuf.size() - client.outPos), 0);
        if (sent == SOCKET_ERROR) {
            if (WSAGetLastError() != WSAEWOULDBLOCK) {
                CloseClient(client);
            }
            return; // FD_WRITE resumes the send when buffer space frees up
        }
        client.outPos += sent;
        client.lastActivity = GetTickCount64();
    }
}
