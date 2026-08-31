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
#include "DlnaDiscovery.h"
#include "Logger.h"
#include "CastDiscovery.h" // CastEnumLocalIPv4Interfaces()
#include <ws2tcpip.h>
#include <algorithm>
#include <climits>

#pragma comment(lib, "ws2_32.lib")

#define SSDP_PORT           1900
#define SSDP_GROUP          0xEFFFFFFAul // 239.255.255.250
#define SSDP_MULTICAST_TTL  4            // the UPnP default hop count
#define SSDP_TARGET         "urn:schemas-upnp-org:device:MediaRenderer:1"

#define SEARCH_INTERVAL_MS  20000ull // re-send the M-SEARCH every 20 s
#define DEVICE_STALE_MS     65000ull // drop devices unseen for 3 intervals + 5 s
#define PROBE_RETRY_MS      60000ull // a description that failed is not refetched sooner

#define MAX_DEVICES         32 // a LAN with more renderers than this is not a use case
#define MAX_PROBE_QUEUE     32
#define MAX_SSDP_PACKET     4096
#define MAX_SINK_LENGTH     16384 // the Sink list of a chatty renderer is long

#define HTTP_CONNECT_TIMEOUT_MS 2000
#define HTTP_IO_TIMEOUT_MS      2000 // per send/recv, so an abort is noticed quickly
#define HTTP_TOTAL_TIMEOUT_MS   6000 // whole exchange
#define HTTP_MAX_RESPONSE       (256 * 1024)

namespace
{
    struct HttpUrl {
        CStringA host;
        USHORT port = 80;
        CStringA path;
    };

    bool ParseHttpUrl(const CString& url, HttpUrl& out)
    {
        CStringA u(url);
        u.Trim();
        if (u.GetLength() < 8 || u.GetLength() > 2048 || u.Left(7).CompareNoCase("http://") != 0) {
            return false;
        }
        const CStringA rest = u.Mid(7);
        const int slash = rest.Find('/');
        CStringA authority = slash >= 0 ? rest.Left(slash) : rest;
        out.path = slash >= 0 ? rest.Mid(slash) : CStringA("/");

        const int at = authority.Find('@');
        if (at >= 0) {
            authority = authority.Mid(at + 1);
        }
        const int colon = authority.ReverseFind(':');
        if (colon >= 0) {
            const CStringA portStr = authority.Mid(colon + 1);
            if (portStr.IsEmpty() || portStr.GetLength() > 5) {
                return false;
            }
            for (int i = 0; i < portStr.GetLength(); i++) {
                if (portStr[i] < '0' || portStr[i] > '9') {
                    return false;
                }
            }
            const long port = atol(portStr);
            if (port <= 0 || port > 65535) {
                return false;
            }
            out.port = (USHORT)port;
            authority = authority.Left(colon);
        }
        if (authority.IsEmpty() || authority.GetLength() > 255) {
            return false;
        }
        out.host = authority;
        return true;
    }

    // Returns the value of the named header from an HTTP header block, or an
    // empty string. The block may start with a status or request line; that
    // line has no colon-terminated name and is skipped by the comparison.
    CStringA GetHeader(const CStringA& head, const char* name)
    {
        const int nameLen = (int)strlen(name);
        int pos = 0;
        while (pos >= 0 && pos < head.GetLength()) {
            const int lineEnd = head.Find("\r\n", pos);
            const CStringA line = lineEnd >= 0 ? head.Mid(pos, lineEnd - pos) : head.Mid(pos);
            const int colon = line.Find(':');
            if (colon == nameLen && line.Left(nameLen).CompareNoCase(name) == 0) {
                CStringA value = line.Mid(colon + 1);
                value.Trim();
                return value;
            }
            pos = lineEnd < 0 ? -1 : lineEnd + 2;
        }
        return CStringA();
    }

    CStringA DeChunk(const CStringA& in)
    {
        CStringA out;
        int pos = 0;
        while (pos < in.GetLength()) {
            const int eol = in.Find("\r\n", pos);
            if (eol < 0) {
                break;
            }
            CStringA sizeLine = in.Mid(pos, eol - pos);
            const int semi = sizeLine.Find(';');
            if (semi >= 0) {
                sizeLine = sizeLine.Left(semi);
            }
            sizeLine.Trim();
            char* endPtr = nullptr;
            const unsigned long size = strtoul(sizeLine, &endPtr, 16);
            if (sizeLine.IsEmpty() || endPtr == sizeLine.GetString()) {
                break; // malformed chunk header
            }
            pos = eol + 2;
            if (size == 0) {
                break; // last chunk
            }
            const int avail = std::min<int>((int)std::min<unsigned long>(size, INT_MAX), in.GetLength() - pos);
            if (avail <= 0) {
                break;
            }
            out += in.Mid(pos, avail);
            if (out.GetLength() >= HTTP_MAX_RESPONSE) {
                break;
            }
            pos += avail + 2; // every chunk is followed by a CRLF
        }
        return out;
    }

    // Reads the tag name starting at nameStart and returns its local part
    // (everything after the last colon), empty when no name is there.
    CStringA TagLocalName(const CStringA& xml, int nameStart, int& nameEnd)
    {
        const int len = xml.GetLength();
        int p = nameStart;
        while (p < len && p - nameStart < 128) {
            const char ch = xml[p];
            if (ch == ' ' || ch == '\t' || ch == '\r' || ch == '\n' || ch == '>' || ch == '/') {
                break;
            }
            p++;
        }
        nameEnd = p;
        if (p <= nameStart) {
            return CStringA();
        }
        const CStringA name = xml.Mid(nameStart, p - nameStart);
        const int colon = name.ReverseFind(':');
        return colon >= 0 ? name.Mid(colon + 1) : name;
    }

    // Skips the comment, CDATA section, declaration or processing instruction
    // starting at pos and returns the index just past it, or -1.
    int SkipMarkup(const CStringA& xml, int pos)
    {
        if (xml.Mid(pos, 4) == "<!--") {
            const int end = xml.Find("-->", pos + 4);
            return end < 0 ? -1 : end + 3;
        }
        if (xml.Mid(pos, 9) == "<![CDATA[") {
            const int end = xml.Find("]]>", pos + 9);
            return end < 0 ? -1 : end + 3;
        }
        const int end = xml.Find('>', pos);
        return end < 0 ? -1 : end + 1;
    }
}

// --- shared helpers ---

bool Dlna::FindElement(const CStringA& xml, const char* localName, int from,
                       int& innerStart, int& innerEnd, int& next)
{
    const int len = xml.GetLength();
    int pos = std::max(from, 0);
    while (pos < len) {
        const int lt = xml.Find('<', pos);
        if (lt < 0 || lt + 1 >= len) {
            return false;
        }
        const char kind = xml[lt + 1];
        if (kind == '!' || kind == '?') {
            pos = SkipMarkup(xml, lt);
            if (pos < 0) {
                return false;
            }
            continue;
        }
        if (kind == '/') {
            pos = lt + 2;
            continue;
        }
        int nameEnd = lt + 1;
        const CStringA name = TagLocalName(xml, lt + 1, nameEnd);
        if (name.IsEmpty()) {
            pos = lt + 1;
            continue;
        }
        const int gt = xml.Find('>', nameEnd);
        if (gt < 0) {
            return false;
        }
        if (name != localName) {
            pos = gt + 1;
            continue;
        }

        innerStart = gt + 1;
        if (xml[gt - 1] == '/') { // <tag/>
            innerEnd = innerStart;
            next = gt + 1;
            return true;
        }

        // Scan to the matching end tag, counting nested elements of the same
        // name: a device description nests <device> inside <deviceList>.
        int depth = 1;
        int p = innerStart;
        while (p < len) {
            const int lt2 = xml.Find('<', p);
            if (lt2 < 0 || lt2 + 1 >= len) {
                return false;
            }
            const char kind2 = xml[lt2 + 1];
            if (kind2 == '!' || kind2 == '?') {
                p = SkipMarkup(xml, lt2);
                if (p < 0) {
                    return false;
                }
                continue;
            }
            const bool endTag = kind2 == '/';
            int nameEnd2 = lt2 + (endTag ? 2 : 1);
            const CStringA name2 = TagLocalName(xml, nameEnd2, nameEnd2);
            const int gt2 = xml.Find('>', nameEnd2);
            if (gt2 < 0) {
                return false;
            }
            if (name2 == localName) {
                if (endTag) {
                    if (--depth == 0) {
                        innerEnd = lt2;
                        next = gt2 + 1;
                        return true;
                    }
                } else if (xml[gt2 - 1] != '/') {
                    depth++;
                }
            }
            p = gt2 + 1;
        }
        return false; // unterminated element
    }
    return false;
}

CStringA Dlna::GetElementText(const CStringA& xml, const char* localName, int from)
{
    int innerStart = 0, innerEnd = 0, next = 0;
    if (!FindElement(xml, localName, from, innerStart, innerEnd, next)) {
        return CStringA();
    }
    CStringA text = xml.Mid(innerStart, innerEnd - innerStart);
    text.Trim();
    return XmlUnescape(text);
}

CStringA Dlna::XmlUnescape(const CStringA& s)
{
    if (s.Find('&') < 0) {
        return s;
    }
    CStringA out;
    for (int i = 0; i < s.GetLength();) {
        if (s[i] != '&') {
            out += s[i++];
            continue;
        }
        const int semi = s.Find(';', i);
        if (semi < 0 || semi - i > 10) {
            out += s[i++];
            continue;
        }
        const CStringA entity = s.Mid(i + 1, semi - i - 1);
        if (entity == "amp") {
            out += '&';
        } else if (entity == "lt") {
            out += '<';
        } else if (entity == "gt") {
            out += '>';
        } else if (entity == "quot") {
            out += '"';
        } else if (entity == "apos") {
            out += '\'';
        } else if (entity.GetLength() > 1 && entity[0] == '#') {
            const bool hex = entity[1] == 'x' || entity[1] == 'X';
            const long code = strtol(entity.Mid(hex ? 2 : 1), nullptr, hex ? 16 : 10);
            // only ASCII is decoded; the values that matter here are markup
            out += (code > 0 && code < 0x80) ? (char)code : '?';
        } else {
            out += s.Mid(i, semi - i + 1); // unknown entity, left as it came
        }
        i = semi + 1;
    }
    return out;
}

CStringA Dlna::XmlEscape(const CStringA& s)
{
    CStringA out;
    for (int i = 0; i < s.GetLength(); i++) {
        switch (s[i]) {
            case '&':
                out += "&amp;";
                break;
            case '<':
                out += "&lt;";
                break;
            case '>':
                out += "&gt;";
                break;
            case '"':
                out += "&quot;";
                break;
            case '\'':
                out += "&apos;";
                break;
            default:
                out += s[i];
                break;
        }
    }
    return out;
}

CString Dlna::ResolveURL(const CString& base, const CString& relative)
{
    CString rel(relative);
    rel.Trim();
    if (rel.IsEmpty()) {
        return CString();
    }
    if (rel.Left(7).CompareNoCase(_T("http://")) == 0 || rel.Left(8).CompareNoCase(_T("https://")) == 0) {
        return rel;
    }

    CString b(base);
    b.Trim();
    if (b.Left(7).CompareNoCase(_T("http://")) != 0) {
        return CString();
    }
    const int slash = b.Find(_T('/'), 7);
    const CString origin = slash < 0 ? b : b.Left(slash);
    if (rel[0] == _T('/')) {
        return origin + rel;
    }
    CString dir = slash < 0 ? CString(_T("/")) : b.Mid(slash);
    const int lastSlash = dir.ReverseFind(_T('/'));
    dir = lastSlash >= 0 ? dir.Left(lastSlash + 1) : CString(_T("/"));
    return origin + dir + rel;
}

CString Dlna::LocalAddressFor(const CString& deviceIp)
{
    CString result;
    sockaddr_in addr;
    ZeroMemory(&addr, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(SSDP_PORT);
    if (inet_pton(AF_INET, CStringA(deviceIp), &addr.sin_addr) != 1) {
        return result;
    }

    SOCKET sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (sock == INVALID_SOCKET) {
        return result;
    }
    // Nothing is sent: connecting a datagram socket only resolves the route,
    // which is what tells us the address of ours the device can reach.
    if (connect(sock, (sockaddr*)&addr, sizeof(addr)) == 0) {
        sockaddr_in local;
        ZeroMemory(&local, sizeof(local));
        int localLen = sizeof(local);
        char buf[16] = { 0 };
        if (getsockname(sock, (sockaddr*)&local, &localLen) == 0
                && inet_ntop(AF_INET, &local.sin_addr, buf, sizeof(buf))) {
            result = CString(buf);
        }
    }
    closesocket(sock);
    return result;
}

bool Dlna::HttpRequest(const CString& url, const CStringA& method, const CStringA& extraHeaders,
                       const CStringA& body, CStringA& response, int& statusCode, HANDLE hAbort,
                       DWORD budgetMs)
{
    response.Empty();
    statusCode = 0;

    // Every wait below is clamped to what the caller allowed. Without that, a
    // device that accepts the connection and then goes quiet costs the whole
    // connect timeout plus the whole exchange timeout however little time the
    // caller had to spend on it.
    const ULONGLONG deadline = GetTickCount64()
                               + (budgetMs ? std::min<DWORD>(budgetMs, HTTP_TOTAL_TIMEOUT_MS)
                                  : HTTP_TOTAL_TIMEOUT_MS);
    auto remaining = [&deadline]() -> DWORD {
        const ULONGLONG now = GetTickCount64();
        return now >= deadline ? 0 : (DWORD)(deadline - now);
    };

    HttpUrl target;
    if (!ParseHttpUrl(url, target)) {
        return false;
    }

    sockaddr_in addr;
    ZeroMemory(&addr, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(target.port);
    if (inet_pton(AF_INET, target.host, &addr.sin_addr) != 1) {
        // a LOCATION header normally carries a literal address; resolving a
        // name is only a fallback
        addrinfo hints;
        ZeroMemory(&hints, sizeof(hints));
        hints.ai_family = AF_INET;
        hints.ai_socktype = SOCK_STREAM;
        addrinfo* res = nullptr;
        if (getaddrinfo(target.host, nullptr, &hints, &res) != 0 || !res) {
            return false;
        }
        addr.sin_addr = ((sockaddr_in*)res->ai_addr)->sin_addr;
        freeaddrinfo(res);
    }

    SOCKET sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (sock == INVALID_SOCKET) {
        return false;
    }

    // A non-blocking connect keeps an unreachable device from stalling the
    // worker for the system's whole SYN retry period.
    ULONG nonBlocking = 1;
    ioctlsocket(sock, FIONBIO, &nonBlocking);
    if (connect(sock, (sockaddr*)&addr, sizeof(addr)) == SOCKET_ERROR) {
        if (WSAGetLastError() != WSAEWOULDBLOCK) {
            closesocket(sock);
            return false;
        }
        fd_set writeSet, exceptSet;
        FD_ZERO(&writeSet);
        FD_SET(sock, &writeSet);
        FD_ZERO(&exceptSet);
        FD_SET(sock, &exceptSet);
        const DWORD connectMs = std::min<DWORD>(HTTP_CONNECT_TIMEOUT_MS, remaining());
        timeval tv = { (long)(connectMs / 1000), (long)((connectMs % 1000) * 1000) };
        if (select(0, nullptr, &writeSet, &exceptSet, &tv) <= 0 || !FD_ISSET(sock, &writeSet)) {
            closesocket(sock);
            return false;
        }
    }
    nonBlocking = 0;
    ioctlsocket(sock, FIONBIO, &nonBlocking);
    // never zero, which the socket would read as "wait forever"
    DWORD ioTimeout = std::max<DWORD>(1, std::min<DWORD>(HTTP_IO_TIMEOUT_MS, remaining()));
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, (const char*)&ioTimeout, sizeof(ioTimeout));
    setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, (const char*)&ioTimeout, sizeof(ioTimeout));

    CStringA request;
    request.Format("%s %s HTTP/1.1\r\n"
                   "HOST: %s:%u\r\n"
                   "USER-AGENT: Windows UPnP/1.0 MPC-HC\r\n"
                   "CONNECTION: close\r\n"
                   "%s",
                   method.GetString(), target.path.GetString(), target.host.GetString(),
                   target.port, extraHeaders.GetString());
    if (!body.IsEmpty()) {
        request.AppendFormat("CONTENT-LENGTH: %d\r\n", body.GetLength());
    }
    request += "\r\n";
    request += body;

    auto expired = [&]() {
        return remaining() == 0
               || (hAbort && WaitForSingleObject(hAbort, 0) == WAIT_OBJECT_0);
    };

    bool ok = true;
    for (int sent = 0; ok && sent < request.GetLength();) {
        const int n = send(sock, request.GetString() + sent, request.GetLength() - sent, 0);
        if (n <= 0 || expired()) {
            ok = false;
        } else {
            sent += n;
        }
    }

    CStringA raw;
    int headerEnd = -1;
    int contentLength = -1;
    bool chunked = false;
    while (ok) {
        char buf[8192];
        const int n = recv(sock, buf, sizeof(buf), 0);
        if (n == 0) {
            break; // the peer closed, which ends a Connection: close response
        }
        if (n < 0) {
            break; // timed out; whatever arrived is all we are going to get
        }
        raw.Append(buf, n);
        if (raw.GetLength() > HTTP_MAX_RESPONSE) {
            break;
        }
        if (headerEnd < 0) {
            headerEnd = raw.Find("\r\n\r\n");
            if (headerEnd >= 0) {
                const CStringA head = raw.Left(headerEnd);
                chunked = GetHeader(head, "Transfer-Encoding").CompareNoCase("chunked") == 0;
                const CStringA length = GetHeader(head, "Content-Length");
                if (!chunked && !length.IsEmpty()) {
                    contentLength = atoi(length);
                }
            }
        }
        if (headerEnd >= 0 && contentLength >= 0 && raw.GetLength() - (headerEnd + 4) >= contentLength) {
            break; // the whole body is in, no need to wait for the close
        }
        if (expired()) {
            break;
        }
    }
    closesocket(sock);

    if (headerEnd < 0) {
        return false;
    }

    const CStringA head = raw.Left(headerEnd);
    CStringA content = raw.Mid(headerEnd + 4);
    const int sp = head.Find(' ');
    statusCode = sp > 0 ? atoi(head.Mid(sp + 1)) : 0;
    if (chunked) {
        content = DeChunk(content);
    } else if (contentLength >= 0 && contentLength < content.GetLength()) {
        content = content.Left(contentLength);
    }
    response = content;
    return statusCode != 0;
}

bool Dlna::HttpGet(const CString& url, CStringA& response, HANDLE hAbort, DWORD budgetMs)
{
    int statusCode = 0;
    return HttpRequest(url, "GET", CStringA(), CStringA(), response, statusCode, hAbort, budgetMs)
           && statusCode == 200;
}

bool Dlna::SoapCall(const CString& controlURL, const CStringA& serviceType, const CStringA& action,
                    const CStringA& argsXml, CStringA& response, CString& fault, HANDLE hAbort,
                    SoapStatus* pStatus, DWORD budgetMs)
{
    fault.Empty();
    if (pStatus) {
        *pStatus = SoapStatus();
    }
    if (controlURL.IsEmpty()) {
        return false;
    }

    CStringA envelope;
    envelope.Format("<?xml version=\"1.0\" encoding=\"utf-8\"?>"
                    "<s:Envelope xmlns:s=\"http://schemas.xmlsoap.org/soap/envelope/\""
                    " s:encodingStyle=\"http://schemas.xmlsoap.org/soap/encoding/\">"
                    "<s:Body><u:%s xmlns:u=\"%s\">%s</u:%s></s:Body></s:Envelope>",
                    action.GetString(), serviceType.GetString(), argsXml.GetString(), action.GetString());

    CStringA headers;
    headers.Format("CONTENT-TYPE: text/xml; charset=\"utf-8\"\r\n"
                   "SOAPACTION: \"%s#%s\"\r\n", serviceType.GetString(), action.GetString());

    int statusCode = 0;
    if (!HttpRequest(controlURL, "POST", headers, envelope, response, statusCode, hAbort, budgetMs)) {
        fault = _T("no response");
        return false;
    }
    if (pStatus) {
        pStatus->httpStatus = statusCode;
    }
    // UPnP requires a fault to come with HTTP 500, but a renderer that answers
    // 200 and puts the fault in the body has still refused the action, so the
    // body decides as much as the status line does.
    int faultStart = 0, faultEnd = 0, faultNext = 0;
    if (statusCode != 200 || FindElement(response, "Fault", 0, faultStart, faultEnd, faultNext)) {
        // a UPnP fault carries the interesting part in errorDescription
        CStringA text = GetElementText(response, "errorDescription");
        if (text.IsEmpty()) {
            text = GetElementText(response, "faultstring");
        }
        if (text.IsEmpty()) {
            text.Format("HTTP %d", statusCode);
        }
        fault = UTF8To16(text);
        const CStringA code = GetElementText(response, "errorCode");
        if (!code.IsEmpty()) {
            if (pStatus) {
                pStatus->errorCode = atoi(code);
            }
            fault.AppendFormat(_T(" (%s)"), UTF8To16(code).GetString());
        }
        TRACE(_T("DlnaSoap: %hs failed: %s\n"), action.GetString(), fault.GetString());
        // The UPnP errorCode is inside fault already; the HTTP status beside
        // it is what says whether the device refused the action or never
        // understood the request at all.
        CASTING_LOG(_T("dlna: %hs was refused with HTTP %d: %s"), action.GetString(),
                    statusCode, fault.GetString());
        return false;
    }
    return true;
}

// --- discovery ---

CDlnaDiscovery::CDlnaDiscovery()
{
}

CDlnaDiscovery::~CDlnaDiscovery()
{
    Stop();
}

bool CDlnaDiscovery::Start()
{
    if (m_hThread) {
        return true;
    }

    m_hStopEvent = CreateEvent(nullptr, TRUE, FALSE, nullptr);
    if (!m_hStopEvent) {
        return false;
    }

    m_hThread = ::CreateThread(nullptr, 0, StaticThreadProc, (LPVOID)this, 0, nullptr);
    if (!m_hThread) {
        CloseHandle(m_hStopEvent);
        m_hStopEvent = nullptr;
        return false;
    }

    return true;
}

void CDlnaDiscovery::Stop()
{
    if (m_hThread) {
        SetEvent(m_hStopEvent);
        // The worker dereferences this object, so nothing could be detached
        // and leaked if it overran: the join has to be unconditional or the
        // object is freed underneath a live thread. Every blocking operation
        // it performs is bounded and watches the stop event, so the first
        // wait is only a debug tripwire.
        if (WaitForSingleObject(m_hThread, 15000) != WAIT_OBJECT_0) {
            ASSERT(FALSE);
            WaitForSingleObject(m_hThread, INFINITE);
        }
        CloseHandle(m_hThread);
        m_hThread = nullptr;
    }
    if (m_hStopEvent) {
        CloseHandle(m_hStopEvent);
        m_hStopEvent = nullptr;
    }

    m_probeQueue.clear();
    m_failedProbes.clear();

    std::lock_guard<std::mutex> lock(m_mutex);
    m_devices.clear();
}

std::vector<DlnaDevice> CDlnaDiscovery::GetDevices()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_devices;
}

// Works off everything a probe queued -- the description itself, and the
// format list that follows it. Only ever called on a private instance.
void CDlnaDiscovery::DrainProbes()
{
    for (int guard = 0; !m_probeQueue.empty() && guard < MAX_PROBE_QUEUE; guard++) {
        const ProbeTask task = m_probeQueue.front();
        m_probeQueue.pop_front();
        RunProbe(task);
    }
}

bool CDlnaDiscovery::ProbeLocation(const CString& location, const CString& ip, DWORD timeoutMs,
                                   DlnaDevice& device)
{
    CDlnaDiscovery probe; // its own list; nothing is started

    // This runs on the caller's thread, so the whole probe -- the description
    // and the format list behind it -- is held to what the caller allowed. A
    // device that accepts the connection and then stops answering is what this
    // is for: without it the caller waits out both HTTP timeouts instead.
    probe.m_probeDeadline = GetTickCount64() + timeoutMs;

    ProbeTask task;
    task.type = ProbeTask::Type::Describe;
    task.location = location;
    task.ip = ip;
    probe.RunProbe(task);
    probe.DrainProbes();

    if (probe.m_devices.empty()) {
        return false;
    }
    device = probe.m_devices.front();
    return true;
}

bool CDlnaDiscovery::ProbeAddress(const CString& ip, UINT port, DWORD timeoutMs, DlnaDevice& device)
{
    if (port == 0) {
        port = SSDP_PORT;
    }

    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        return false;
    }

    CDlnaDiscovery probe;

    SOCKET sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (sock != INVALID_SOCKET) {
        sockaddr_in local;
        ZeroMemory(&local, sizeof(local));
        local.sin_family = AF_INET;
        local.sin_addr.s_addr = htonl(INADDR_ANY);
        local.sin_port = 0;

        sockaddr_in to;
        ZeroMemory(&to, sizeof(to));
        to.sin_family = AF_INET;
        to.sin_port = htons((u_short)port);

        if (bind(sock, (sockaddr*)&local, sizeof(local)) == 0
                && inet_pton(AF_INET, CStringA(ip), &to.sin_addr) == 1) {
            // A unicast M-SEARCH names the host it is sent to in HOST, and asks
            // for the answer at once: there is no group of devices to spread
            // their replies over.
            CStringA packet;
            packet.Format("M-SEARCH * HTTP/1.1\r\n"
                          "HOST: %s:%u\r\n"
                          "MAN: \"ssdp:discover\"\r\n"
                          "MX: 1\r\n"
                          "ST: %s\r\n"
                          "USER-AGENT: Windows UPnP/1.0 MPC-HC\r\n"
                          "\r\n", CStringA(ip).GetString(), port, SSDP_TARGET);
            sendto(sock, packet, packet.GetLength(), 0, (sockaddr*)&to, sizeof(to));
            sendto(sock, packet, packet.GetLength(), 0, (sockaddr*)&to, sizeof(to));

            const ULONGLONG deadline = GetTickCount64() + timeoutMs;
            for (ULONGLONG now = GetTickCount64(); now < deadline; now = GetTickCount64()) {
                fd_set readSet;
                FD_ZERO(&readSet);
                FD_SET(sock, &readSet);
                const DWORD left = (DWORD)(deadline - now);
                timeval tv = { (long)(left / 1000), (long)((left % 1000) * 1000) };
                if (select(0, &readSet, nullptr, nullptr, &tv) != 1) {
                    break;
                }
                char buf[MAX_SSDP_PACKET];
                sockaddr_in from;
                int fromLen = sizeof(from);
                const int len = recvfrom(sock, buf, sizeof(buf), 0, (sockaddr*)&from, &fromLen);
                if (len <= 0) {
                    break;
                }
                if (from.sin_addr.s_addr != to.sin_addr.s_addr) {
                    continue;
                }
                probe.ParseSsdp(CStringA(buf, len), from.sin_addr);
                if (!probe.m_probeQueue.empty()) {
                    break; // the host answered; describing it is all that is left
                }
            }
        }
        closesocket(sock);
    }
    WSACleanup();

    // The search had its timeout; the description the answer points at gets
    // the same again rather than the HTTP timeouts in full, so a host that
    // answers a search and then goes quiet cannot hold the caller much longer
    // than it asked to wait.
    probe.m_probeDeadline = GetTickCount64() + timeoutMs;
    probe.DrainProbes();
    if (probe.m_devices.empty()) {
        return false;
    }
    device = probe.m_devices.front();
    return true;
}

DWORD WINAPI CDlnaDiscovery::StaticThreadProc(LPVOID lpParam)
{
    SetThreadName(DWORD(-1), "DlnaDiscovery Thread");
    return ((CDlnaDiscovery*)lpParam)->ThreadProc();
}

DWORD CDlnaDiscovery::ThreadProc()
{
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        return DWORD_ERROR;
    }

    SOCKET searchSock = OpenSearchSocket();
    if (searchSock == INVALID_SOCKET) {
        WSACleanup();
        return DWORD_ERROR;
    }
    // Listening for unsolicited announcements needs port 1900, which another
    // UPnP stack may already hold exclusively. It is a bonus, not a
    // requirement: without it devices are still found by the M-SEARCH.
    SOCKET notifySock = OpenNotifySocket();

    // WSAEventSelect also puts the sockets into non-blocking mode
    WSAEVENT hSearchEvent = WSACreateEvent();
    WSAEVENT hNotifyEvent = notifySock != INVALID_SOCKET ? WSACreateEvent() : WSA_INVALID_EVENT;
    if (hSearchEvent == WSA_INVALID_EVENT || WSAEventSelect(searchSock, hSearchEvent, FD_READ) == SOCKET_ERROR) {
        if (hSearchEvent != WSA_INVALID_EVENT) {
            WSACloseEvent(hSearchEvent);
        }
        if (hNotifyEvent != WSA_INVALID_EVENT) {
            WSACloseEvent(hNotifyEvent);
        }
        if (notifySock != INVALID_SOCKET) {
            closesocket(notifySock);
        }
        closesocket(searchSock);
        WSACleanup();
        return DWORD_ERROR;
    }
    if (hNotifyEvent != WSA_INVALID_EVENT
            && WSAEventSelect(notifySock, hNotifyEvent, FD_READ) == SOCKET_ERROR) {
        WSACloseEvent(hNotifyEvent);
        hNotifyEvent = WSA_INVALID_EVENT;
        closesocket(notifySock);
        notifySock = INVALID_SOCKET;
    }

    SendSearch(searchSock);
    ULONGLONG lastSearch = GetTickCount64();

    std::vector<HANDLE> handles;
    handles.emplace_back(m_hStopEvent);
    handles.emplace_back(hSearchEvent);
    if (hNotifyEvent != WSA_INVALID_EVENT) {
        handles.emplace_back(hNotifyEvent);
    }

    for (;;) {
        // sleep until the next search is due or the nearest device goes stale,
        // but never while there is still a description to fetch
        DWORD timeout = 0;
        if (m_probeQueue.empty()) {
            const ULONGLONG tick = GetTickCount64();
            ULONGLONG deadline = lastSearch + SEARCH_INTERVAL_MS;
            {
                std::lock_guard<std::mutex> lock(m_mutex);
                for (const DlnaDevice& d : m_devices) {
                    deadline = std::min(deadline, d.lastSeen + DEVICE_STALE_MS);
                }
            }
            timeout = (DWORD)(deadline > tick ? deadline - tick : 0);
        }

        const DWORD ret = WaitForMultipleObjects((DWORD)handles.size(), handles.data(), FALSE, timeout);
        if (ret == WAIT_OBJECT_0) {
            break; // stop requested
        } else if (ret == WAIT_OBJECT_0 + 1) {
            WSAResetEvent(hSearchEvent);
            ReceivePackets(searchSock);
        } else if (ret == WAIT_OBJECT_0 + 2) {
            WSAResetEvent(hNotifyEvent);
            ReceivePackets(notifySock);
        }

        const ULONGLONG now = GetTickCount64();
        if (now - lastSearch >= SEARCH_INTERVAL_MS) {
            SendSearch(searchSock);
            lastSearch = now;
        }

        // one HTTP exchange per pass, so a stop is never held up by more than
        // a single unresponsive device
        if (!m_probeQueue.empty()) {
            const ProbeTask task = m_probeQueue.front();
            m_probeQueue.pop_front();
            RunProbe(task);
        }

        ExpireStaleDevices();
    }

    if (hNotifyEvent != WSA_INVALID_EVENT) {
        WSACloseEvent(hNotifyEvent);
    }
    if (notifySock != INVALID_SOCKET) {
        closesocket(notifySock);
    }
    WSACloseEvent(hSearchEvent);
    closesocket(searchSock);
    WSACleanup();
    return 0;
}

SOCKET CDlnaDiscovery::OpenSearchSocket()
{
    SOCKET sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (sock == INVALID_SOCKET) {
        return INVALID_SOCKET;
    }

    sockaddr_in addr;
    ZeroMemory(&addr, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = 0; // the unicast M-SEARCH answers come back to this port
    if (bind(sock, (sockaddr*)&addr, sizeof(addr)) == SOCKET_ERROR) {
        closesocket(sock);
        return INVALID_SOCKET;
    }

    DWORD ttl = SSDP_MULTICAST_TTL;
    setsockopt(sock, IPPROTO_IP, IP_MULTICAST_TTL, (const char*)&ttl, sizeof(ttl));
    return sock;
}

SOCKET CDlnaDiscovery::OpenNotifySocket()
{
    SOCKET sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (sock == INVALID_SOCKET) {
        return INVALID_SOCKET;
    }

    BOOL reuse = TRUE;
    setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, (const char*)&reuse, sizeof(reuse));

    sockaddr_in addr;
    ZeroMemory(&addr, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(SSDP_PORT);
    if (bind(sock, (sockaddr*)&addr, sizeof(addr)) == SOCKET_ERROR) {
        closesocket(sock);
        return INVALID_SOCKET;
    }

    // Joining on INADDR_ANY picks a single routing-table interface, which on a
    // machine with Hyper-V, WSL or VPN adapters is regularly not the one the
    // media devices are on, so every interface is joined explicitly.
    const std::vector<IN_ADDR> interfaces = CastEnumLocalIPv4Interfaces();
    bool joined = false;
    for (const IN_ADDR& ifAddr : interfaces) {
        ip_mreq mreq;
        ZeroMemory(&mreq, sizeof(mreq));
        mreq.imr_multiaddr.s_addr = htonl(SSDP_GROUP);
        mreq.imr_interface = ifAddr;
        joined |= setsockopt(sock, IPPROTO_IP, IP_ADD_MEMBERSHIP, (const char*)&mreq, sizeof(mreq)) == 0;
    }
    if (!joined) {
        ip_mreq mreq;
        ZeroMemory(&mreq, sizeof(mreq));
        mreq.imr_multiaddr.s_addr = htonl(SSDP_GROUP);
        mreq.imr_interface.s_addr = htonl(INADDR_ANY);
        setsockopt(sock, IPPROTO_IP, IP_ADD_MEMBERSHIP, (const char*)&mreq, sizeof(mreq));
    }
    return sock;
}

void CDlnaDiscovery::SendSearch(SOCKET sock)
{
    CStringA packet;
    packet.Format("M-SEARCH * HTTP/1.1\r\n"
                  "HOST: 239.255.255.250:%d\r\n"
                  "MAN: \"ssdp:discover\"\r\n"
                  "MX: 2\r\n"
                  "ST: %s\r\n"
                  "USER-AGENT: Windows UPnP/1.0 MPC-HC\r\n"
                  "\r\n", SSDP_PORT, SSDP_TARGET);

    sockaddr_in to;
    ZeroMemory(&to, sizeof(to));
    to.sin_family = AF_INET;
    to.sin_addr.s_addr = htonl(SSDP_GROUP);
    to.sin_port = htons(SSDP_PORT);

    // The query goes out of every interface: a socket bound to INADDR_ANY
    // sends multicast through one routing-table choice only, which misses the
    // real LAN whenever a virtual adapter outranks it.
    const std::vector<IN_ADDR> interfaces = CastEnumLocalIPv4Interfaces();
    if (interfaces.empty()) {
        sendto(sock, packet, packet.GetLength(), 0, (sockaddr*)&to, sizeof(to));
        return;
    }
    for (const IN_ADDR& ifAddr : interfaces) {
        if (setsockopt(sock, IPPROTO_IP, IP_MULTICAST_IF, (const char*)&ifAddr, sizeof(ifAddr)) != 0) {
            continue;
        }
        // sent twice: SSDP is unreliable by design and each device answers once
        sendto(sock, packet, packet.GetLength(), 0, (sockaddr*)&to, sizeof(to));
        sendto(sock, packet, packet.GetLength(), 0, (sockaddr*)&to, sizeof(to));
    }
}

void CDlnaDiscovery::ReceivePackets(SOCKET sock)
{
    for (;;) {
        char buf[MAX_SSDP_PACKET];
        sockaddr_in from;
        int fromLen = sizeof(from);
        const int len = recvfrom(sock, buf, sizeof(buf), 0, (sockaddr*)&from, &fromLen);
        if (len <= 0) {
            break; // WSAEWOULDBLOCK when drained, or an error
        }
        ParseSsdp(CStringA(buf, len), from.sin_addr);
    }
}

void CDlnaDiscovery::ParseSsdp(const CStringA& packet, const IN_ADDR& srcAddr)
{
    const int firstLineEnd = packet.Find("\r\n");
    if (firstLineEnd <= 0) {
        return;
    }
    const CStringA firstLine = packet.Left(firstLineEnd);
    const CStringA headers = packet.Mid(firstLineEnd + 2);

    bool byebye = false;
    if (firstLine.Left(4).CompareNoCase("HTTP") == 0) {
        if (GetHeader(headers, "ST").CompareNoCase(SSDP_TARGET) != 0) {
            return; // an answer to somebody else's search
        }
    } else if (firstLine.Left(6).CompareNoCase("NOTIFY") == 0) {
        if (GetHeader(headers, "NT").CompareNoCase(SSDP_TARGET) != 0) {
            return;
        }
        const CStringA nts = GetHeader(headers, "NTS");
        byebye = nts.CompareNoCase("ssdp:byebye") == 0;
        if (!byebye && nts.CompareNoCase("ssdp:alive") != 0) {
            return;
        }
    } else {
        return; // an M-SEARCH from another controller on the group
    }

    // the USN of a device advertisement is "uuid:<udn>::<target>"
    CStringA usn = GetHeader(headers, "USN");
    const int sep = usn.Find("::");
    if (sep >= 0) {
        usn = usn.Left(sep);
    }
    const CString udn = UTF8To16(usn);

    if (byebye) {
        if (!udn.IsEmpty()) {
            RemoveDevice(udn);
        }
        return;
    }

    const CString location = UTF8To16(GetHeader(headers, "LOCATION"));
    if (location.IsEmpty()) {
        return;
    }

    // a device that has been described already only needs its clock reset
    if (!udn.IsEmpty()) {
        bool known;
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            known = std::any_of(m_devices.cbegin(), m_devices.cend(), [&](const DlnaDevice & d) {
                return d.udn == udn;
            });
        }
        if (known) {
            RefreshDevice(udn);
            return;
        }
    }

    char ipBuf[16] = { 0 };
    if (!inet_ntop(AF_INET, (PVOID)&srcAddr, ipBuf, sizeof(ipBuf))) {
        return;
    }

    ProbeTask task;
    task.type = ProbeTask::Type::Describe;
    task.location = location;
    task.ip = CString(ipBuf);
    task.udn = udn;
    QueueProbe(std::move(task));
}

void CDlnaDiscovery::QueueProbe(ProbeTask&& task)
{
    const ULONGLONG now = GetTickCount64();

    if (task.type == ProbeTask::Type::Describe) {
        // a description that could not be turned into a device is not retried
        // on every announcement, or one dead device would monopolize the queue
        m_failedProbes.erase(std::remove_if(m_failedProbes.begin(), m_failedProbes.end(),
        [now](const FailedProbe & f) {
            return now - f.tick > PROBE_RETRY_MS;
        }), m_failedProbes.end());
        if (std::any_of(m_failedProbes.cbegin(), m_failedProbes.cend(), [&](const FailedProbe & f) {
        return f.location == task.location;
        })) {
            return;
        }
    }

    const bool queued = std::any_of(m_probeQueue.cbegin(), m_probeQueue.cend(), [&](const ProbeTask & t) {
        return t.type == task.type && t.location == task.location && t.udn == task.udn;
    });
    if (queued || m_probeQueue.size() >= MAX_PROBE_QUEUE) {
        return;
    }
    m_probeQueue.emplace_back(std::move(task));
}

// Milliseconds a bounded probe has left, and 0 when it is not bounded at all,
// which is what a call gets when it may take its own timeouts in full.
DWORD CDlnaDiscovery::ProbeBudget() const
{
    if (m_probeDeadline == 0) {
        return 0;
    }
    const ULONGLONG now = GetTickCount64();
    return now >= m_probeDeadline ? 1 : (DWORD)(m_probeDeadline - now);
}

void CDlnaDiscovery::RunProbe(const ProbeTask& task)
{
    if (m_probeDeadline != 0 && GetTickCount64() >= m_probeDeadline) {
        // The caller's time is up. Whatever the probe has learned by now is
        // kept: a device found without its format list is still a device, and
        // what is missing is picked up the next time it is connected to.
        return;
    }

    if (task.type == ProbeTask::Type::ProtocolInfo) {
        CStringA response;
        CString fault;
        if (!Dlna::SoapCall(task.controlURL, "urn:schemas-upnp-org:service:ConnectionManager:1",
                            "GetProtocolInfo", CStringA(), response, fault, m_hStopEvent,
                            nullptr, ProbeBudget())) {
            return; // the device keeps its permissive defaults
        }
        CStringA sink = Dlna::GetElementText(response, "Sink");
        if (sink.IsEmpty()) {
            return;
        }
        if (sink.GetLength() > MAX_SINK_LENGTH) {
            sink = sink.Left(MAX_SINK_LENGTH);
        }
        CStringA lower(sink);
        lower.MakeLower();
        const bool video = lower.Find("video/") >= 0;
        const bool audio = lower.Find("audio/") >= 0;
        if (!video && !audio) {
            return; // nothing recognizable; staying permissive is friendlier
        }

        std::lock_guard<std::mutex> lock(m_mutex);
        for (DlnaDevice& d : m_devices) {
            if (d.udn == task.udn) {
                d.sinkProtocolInfo = sink;
                d.supportsVideo = video;
                d.supportsAudio = audio;
                TRACE(_T("DlnaDiscovery: \"%s\" accepts %s\n"), d.friendlyName.GetString(),
                      video ? (audio ? _T("audio and video") : _T("video")) : _T("audio only"));
                CASTING_LOG(_T("discovery: DLNA \"%s\" answered GetProtocolInfo with %d characters ")
                            _T("of sink list (%s)"), d.friendlyName.GetString(), sink.GetLength(),
                            video ? (audio ? _T("audio and video") : _T("video only")) : _T("audio only"));
                break;
            }
        }
        return;
    }

    CStringA xml;
    if (!Dlna::HttpGet(task.location, xml, m_hStopEvent, ProbeBudget())) {
        // A device that announces itself and then will not describe itself is
        // typically half asleep; a magic packet costs one datagram and the
        // next announcement it sends gets probed again.
        DlnaVendor::WakeOnLan(task.ip);
        m_failedProbes.push_back({ task.location, GetTickCount64() });
        return;
    }

    // the renderer may be an embedded device, so the <device> carrying the
    // MediaRenderer type is looked for at any depth
    CStringA deviceBlock;
    for (int pos = 0;;) {
        int innerStart = 0, innerEnd = 0, next = 0;
        if (!Dlna::FindElement(xml, "device", pos, innerStart, innerEnd, next)) {
            break;
        }
        const CStringA block = xml.Mid(innerStart, innerEnd - innerStart);
        if (Dlna::GetElementText(block, "deviceType").Find("MediaRenderer") >= 0) {
            deviceBlock = block;
            break;
        }
        pos = innerStart; // past the opening tag, so this always advances
    }
    if (deviceBlock.IsEmpty()) {
        m_failedProbes.push_back({ task.location, GetTickCount64() });
        return;
    }

    DlnaDevice dev;
    dev.location = task.location;
    dev.ipAddress = task.ip;
    dev.friendlyName = UTF8To16(Dlna::GetElementText(deviceBlock, "friendlyName"));
    dev.manufacturer = UTF8To16(Dlna::GetElementText(deviceBlock, "manufacturer"));
    dev.modelName = UTF8To16(Dlna::GetElementText(deviceBlock, "modelName"));
    dev.udn = UTF8To16(Dlna::GetElementText(deviceBlock, "UDN"));
    if (dev.udn.IsEmpty()) {
        dev.udn = task.udn; // fall back to the identity from the USN
    }

    // the vendor block, when there is one, is a sibling of <device>
    DlnaVendor::ParseDescription(xml, dev.vendor);

    CString base = UTF8To16(Dlna::GetElementText(xml, "URLBase"));
    if (base.IsEmpty()) {
        base = task.location;
    }
    for (int pos = 0;;) {
        int innerStart = 0, innerEnd = 0, next = 0;
        if (!Dlna::FindElement(deviceBlock, "service", pos, innerStart, innerEnd, next)) {
            break;
        }
        const CStringA service = deviceBlock.Mid(innerStart, innerEnd - innerStart);
        const CStringA type = Dlna::GetElementText(service, "serviceType");
        const CString controlURL = Dlna::ResolveURL(base, UTF8To16(Dlna::GetElementText(service, "controlURL")));
        if (!controlURL.IsEmpty()) {
            if (type.Find("AVTransport") >= 0) {
                dev.avTransportURL = controlURL;
                // the seek units a renderer accepts are only stated there
                dev.avTransportSCPDURL =
                    Dlna::ResolveURL(base, UTF8To16(Dlna::GetElementText(service, "SCPDURL")));
            } else if (type.Find("RenderingControl") >= 0) {
                dev.renderingControlURL = controlURL;
            } else if (type.Find("ConnectionManager") >= 0) {
                dev.connectionManagerURL = controlURL;
            }
        }
        pos = next;
    }

    if (dev.udn.IsEmpty() || dev.avTransportURL.IsEmpty()) {
        m_failedProbes.push_back({ task.location, GetTickCount64() }); // not controllable
        return;
    }
    if (dev.friendlyName.IsEmpty()) {
        dev.friendlyName = dev.modelName.IsEmpty() ? dev.ipAddress : dev.modelName;
    }
    dev.lastSeen = GetTickCount64();

    {
        std::lock_guard<std::mutex> lock(m_mutex);
        auto it = std::find_if(m_devices.begin(), m_devices.end(), [&](const DlnaDevice & d) {
            return d.udn == dev.udn;
        });
        if (it != m_devices.end()) {
            // a re-description must not throw away what GetProtocolInfo taught us
            const CStringA sink = it->sinkProtocolInfo;
            const bool video = it->supportsVideo, audio = it->supportsAudio;
            *it = dev;
            if (!sink.IsEmpty()) {
                it->sinkProtocolInfo = sink;
                it->supportsVideo = video;
                it->supportsAudio = audio;
            }
        } else if (m_devices.size() < MAX_DEVICES) {
            m_devices.emplace_back(dev);
            TRACE(_T("DlnaDiscovery: found \"%s\" at %s (%s %s)\n"), dev.friendlyName.GetString(),
                  dev.ipAddress.GetString(), dev.manufacturer.GetString(), dev.modelName.GetString());
            // Verbatim from the device description, because a renderer that
            // misbehaves is identified by exactly these three fields.
            CASTING_LOG(_T("discovery: DLNA \"%s\" at %s, manufacturer=\"%s\" model=\"%s\" ")
                        _T("description=%s%s%s"),
                        dev.friendlyName.GetString(), dev.ipAddress.GetString(),
                        dev.manufacturer.GetString(), dev.modelName.GetString(),
                        dev.location.GetString(),
                        dev.renderingControlURL.IsEmpty() ? _T(", no RenderingControl") : _T(""),
                        dev.avTransportSCPDURL.IsEmpty() ? _T(", no AVTransport description") : _T(""));
        } else {
            return;
        }
    }

    if (!dev.connectionManagerURL.IsEmpty()) {
        ProbeTask followUp;
        followUp.type = ProbeTask::Type::ProtocolInfo;
        followUp.udn = dev.udn;
        followUp.controlURL = dev.connectionManagerURL;
        QueueProbe(std::move(followUp));
    }
}

void CDlnaDiscovery::RefreshDevice(const CString& udn)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    for (DlnaDevice& d : m_devices) {
        if (d.udn == udn) {
            d.lastSeen = GetTickCount64();
            break;
        }
    }
}

void CDlnaDiscovery::RemoveDevice(const CString& udn)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_devices.erase(std::remove_if(m_devices.begin(), m_devices.end(), [&](const DlnaDevice & d) {
        return d.udn == udn;
    }), m_devices.end());
}

void CDlnaDiscovery::ExpireStaleDevices()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    const ULONGLONG now = GetTickCount64();
    m_devices.erase(std::remove_if(m_devices.begin(), m_devices.end(), [now](const DlnaDevice & d) {
        return now - d.lastSeen > DEVICE_STALE_MS;
    }), m_devices.end());
}
