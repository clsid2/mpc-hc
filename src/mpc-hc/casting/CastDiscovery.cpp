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
#include "CastDiscovery.h"
#include <ws2tcpip.h>
#include <iphlpapi.h>
#include <algorithm>
#include <map>

#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib, "iphlpapi.lib")

#define MDNS_PORT           5353
#define MDNS_GROUP          0xE00000FBul // 224.0.0.251
#define CAST_SERVICE_NAME   "_googlecast._tcp.local"
#define CAST_SERVICE_SUFFIX "._googlecast._tcp.local"

#define QUERY_INTERVAL_MS   15000ull // re-send the mDNS query every 15 s
#define DEVICE_STALE_MS     50000ull // drop devices unseen for 3 intervals + 5 s

// A LAN with more cast devices than this is not a use case. The cap matters
// less for the memory than for what it bounds: announcements are unsolicited
// datagrams from any source, and one packet can carry a hundred instances the
// list would otherwise all take, each of which the menu then copies and the
// worker walks under the lock.
#define MAX_DEVICES         64

// DNS record types (named to avoid clashing with the windns.h DNS_TYPE_* macros)
#define DNS_RECTYPE_A   1
#define DNS_RECTYPE_PTR 12
#define DNS_RECTYPE_TXT 16
#define DNS_RECTYPE_SRV 33

namespace
{
    // Reads a (possibly compressed) DNS name starting at pos. On success,
    // advances pos past the bytes consumed at the original location and
    // returns the dotted name. Every access is bounds-checked against len
    // since this parses untrusted network input.
    bool ReadDnsName(const BYTE* packet, int len, int& pos, CStringA& name)
    {
        name.Empty();
        int p = pos;
        bool jumped = false;
        int jumps = 0;
        for (;;) {
            if (p < 0 || p >= len) {
                return false;
            }
            BYTE labelLen = packet[p];
            if (labelLen == 0) {
                if (!jumped) {
                    pos = p + 1;
                }
                return true;
            } else if ((labelLen & 0xC0) == 0xC0) {
                // compression pointer
                if (p + 1 >= len) {
                    return false;
                }
                int offset = ((labelLen & 0x3F) << 8) | packet[p + 1];
                if (!jumped) {
                    pos = p + 2;
                }
                jumped = true;
                if (++jumps > 32 || offset >= len) {
                    return false;
                }
                p = offset;
            } else if ((labelLen & 0xC0) == 0) {
                if (p + 1 + labelLen > len) {
                    return false;
                }
                if (!name.IsEmpty()) {
                    name += '.';
                }
                name += CStringA(reinterpret_cast<const char*>(&packet[p + 1]), labelLen);
                if (name.GetLength() > 1024) { // sanity limit
                    return false;
                }
                p += 1 + labelLen;
            } else {
                return false; // reserved label type
            }
        }
    }
}

std::vector<IN_ADDR> CastEnumLocalIPv4Interfaces()
{
    std::vector<IN_ADDR> addresses;

    ULONG size = 16 * 1024;
    std::vector<BYTE> buffer;
    ULONG ret = ERROR_BUFFER_OVERFLOW;
    for (int attempt = 0; attempt < 3 && ret == ERROR_BUFFER_OVERFLOW; attempt++) {
        buffer.resize(size);
        ret = GetAdaptersAddresses(AF_INET, GAA_FLAG_SKIP_ANYCAST | GAA_FLAG_SKIP_MULTICAST
                                   | GAA_FLAG_SKIP_DNS_SERVER | GAA_FLAG_SKIP_FRIENDLY_NAME,
                                   nullptr, (IP_ADAPTER_ADDRESSES*)buffer.data(), &size);
    }
    if (ret != NO_ERROR) {
        return addresses;
    }

    for (auto* adapter = (IP_ADAPTER_ADDRESSES*)buffer.data(); adapter; adapter = adapter->Next) {
        if (adapter->OperStatus != IfOperStatusUp || adapter->IfType == IF_TYPE_SOFTWARE_LOOPBACK
                || !(adapter->Flags & IP_ADAPTER_IPV4_ENABLED) || (adapter->Flags & IP_ADAPTER_RECEIVE_ONLY)) {
            continue;
        }
        for (auto* unicast = adapter->FirstUnicastAddress; unicast; unicast = unicast->Next) {
            if (!unicast->Address.lpSockaddr || unicast->Address.lpSockaddr->sa_family != AF_INET) {
                continue;
            }
            const IN_ADDR addr = ((sockaddr_in*)unicast->Address.lpSockaddr)->sin_addr;
            const ULONG host = ntohl(addr.s_addr);
            if (host == 0 || (host >> 24) == 127 || (host & 0xFFFF0000ul) == 0xA9FE0000ul) {
                continue; // unspecified, loopback or an unconfigured 169.254 address
            }
            if (std::none_of(addresses.cbegin(), addresses.cend(), [&](const IN_ADDR & a) {
            return a.s_addr == addr.s_addr;
            })) {
                addresses.emplace_back(addr);
            }
        }
    }
    return addresses;
}

CCastDiscovery::CCastDiscovery()
{
}

CCastDiscovery::~CCastDiscovery()
{
    Stop();
}

bool CCastDiscovery::Start()
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

void CCastDiscovery::Stop()
{
    if (m_hThread) {
        SetEvent(m_hStopEvent);
        // The worker dereferences this object, so nothing could be detached
        // and leaked if it overran: the join has to be unconditional or the
        // object is freed underneath a live thread. The worker waits at most
        // one second between checks of the stop event, so the first wait is
        // only a debug tripwire.
        if (WaitForSingleObject(m_hThread, 10000) != WAIT_OBJECT_0) {
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

    std::lock_guard<std::mutex> lock(m_mutex);
    m_devices.clear();
}

std::vector<CastDevice> CCastDiscovery::GetDevices()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_devices;
}

DWORD WINAPI CCastDiscovery::StaticThreadProc(LPVOID lpParam)
{
    SetThreadName(DWORD(-1), "CastDiscovery Thread");
    return ((CCastDiscovery*)lpParam)->ThreadProc();
}

DWORD CCastDiscovery::ThreadProc()
{
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        return DWORD_ERROR;
    }

    SOCKET sock = OpenSocket();
    if (sock == INVALID_SOCKET) {
        WSACleanup();
        return DWORD_ERROR;
    }

    // WSAEventSelect also puts the socket into non-blocking mode
    WSAEVENT hSocketEvent = WSACreateEvent();
    if (hSocketEvent == WSA_INVALID_EVENT || WSAEventSelect(sock, hSocketEvent, FD_READ) == SOCKET_ERROR) {
        if (hSocketEvent != WSA_INVALID_EVENT) {
            WSACloseEvent(hSocketEvent);
        }
        closesocket(sock);
        WSACleanup();
        return DWORD_ERROR;
    }

    SendQuery(sock);
    ULONGLONG lastQuery = GetTickCount64();

    HANDLE handles[] = { m_hStopEvent, hSocketEvent };
    for (;;) {
        // sleep until the next query is due or the nearest device goes stale,
        // instead of ticking once a second for nothing
        const ULONGLONG tick = GetTickCount64();
        ULONGLONG deadline = lastQuery + QUERY_INTERVAL_MS;
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            for (const CastDevice& d : m_devices) {
                deadline = std::min(deadline, d.lastSeen + DEVICE_STALE_MS);
            }
        }

        DWORD ret = WaitForMultipleObjects(_countof(handles), handles, FALSE,
                                           (DWORD)(deadline > tick ? deadline - tick : 0));
        if (ret == WAIT_OBJECT_0) {
            break; // stop requested
        } else if (ret == WAIT_OBJECT_0 + 1) {
            WSAResetEvent(hSocketEvent);
            // drain all pending datagrams
            for (;;) {
                BYTE buf[4096];
                sockaddr_in from;
                int fromLen = sizeof(from);
                int len = recvfrom(sock, (char*)buf, sizeof(buf), 0, (sockaddr*)&from, &fromLen);
                if (len <= 0) {
                    break; // WSAEWOULDBLOCK when drained, or an error
                }
                ParseResponse(buf, len, from.sin_addr);
            }
        }

        ULONGLONG now = GetTickCount64();
        if (now - lastQuery >= QUERY_INTERVAL_MS) {
            SendQuery(sock);
            lastQuery = now;
        }
        ExpireStaleDevices();
    }

    closesocket(sock);
    WSACloseEvent(hSocketEvent);
    WSACleanup();
    return 0;
}

SOCKET CCastDiscovery::OpenSocket()
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
    addr.sin_port = htons(MDNS_PORT);
    if (bind(sock, (sockaddr*)&addr, sizeof(addr)) == SOCKET_ERROR) {
        // Port 5353 may be held exclusively by another responder. Fall back
        // to an ephemeral port and rely on the QU bit in our queries to get
        // unicast responses.
        addr.sin_port = 0;
        if (bind(sock, (sockaddr*)&addr, sizeof(addr)) == SOCKET_ERROR) {
            closesocket(sock);
            return INVALID_SOCKET;
        }
    }

    // Joining on INADDR_ANY picks a single routing-table interface, which is
    // regularly a virtual switch on a machine with Hyper-V, WSL or VPN
    // adapters, so every interface is joined explicitly.
    bool joined = false;
    for (const IN_ADDR& ifAddr : CastEnumLocalIPv4Interfaces()) {
        ip_mreq mreq;
        ZeroMemory(&mreq, sizeof(mreq));
        mreq.imr_multiaddr.s_addr = htonl(MDNS_GROUP);
        mreq.imr_interface = ifAddr;
        joined |= setsockopt(sock, IPPROTO_IP, IP_ADD_MEMBERSHIP, (const char*)&mreq, sizeof(mreq)) == 0;
    }
    if (!joined) {
        ip_mreq mreq;
        ZeroMemory(&mreq, sizeof(mreq));
        mreq.imr_multiaddr.s_addr = htonl(MDNS_GROUP);
        mreq.imr_interface.s_addr = htonl(INADDR_ANY);
        setsockopt(sock, IPPROTO_IP, IP_ADD_MEMBERSHIP, (const char*)&mreq, sizeof(mreq));
    }

    DWORD ttl = 255; // mDNS convention
    setsockopt(sock, IPPROTO_IP, IP_MULTICAST_TTL, (const char*)&ttl, sizeof(ttl));

    return sock;
}

int CCastDiscovery::BuildQuery(BYTE* packet)
{
    // Standard query with a single PTR question for _googlecast._tcp.local,
    // QU bit set to request unicast responses
    int pos = 0;

    static const BYTE header[12] = { 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0 };
    memcpy(packet, header, sizeof(header));
    pos += sizeof(header);

    static const char* const labels[] = { "_googlecast", "_tcp", "local" };
    for (const char* label : labels) {
        size_t labelLen = strlen(label);
        packet[pos++] = (BYTE)labelLen;
        memcpy(&packet[pos], label, labelLen);
        pos += (int)labelLen;
    }
    packet[pos++] = 0;

    packet[pos++] = 0; // QTYPE = PTR
    packet[pos++] = DNS_RECTYPE_PTR;
    packet[pos++] = 0x80; // QCLASS = IN with the QU (unicast response) bit
    packet[pos++] = 0x01;

    return pos;
}

bool CCastDiscovery::SendQuery(SOCKET sock)
{
    BYTE packet[64];
    const int pos = BuildQuery(packet);

    sockaddr_in to;
    ZeroMemory(&to, sizeof(to));
    to.sin_family = AF_INET;
    to.sin_addr.s_addr = htonl(MDNS_GROUP);
    to.sin_port = htons(MDNS_PORT);

    // The query goes out of every interface for the same reason the group is
    // joined on all of them: one INADDR_ANY send reaches a single, routing
    // table chosen network, which need not be the one with the devices on it.
    const std::vector<IN_ADDR> interfaces = CastEnumLocalIPv4Interfaces();
    if (interfaces.empty()) {
        return sendto(sock, (const char*)packet, pos, 0, (sockaddr*)&to, sizeof(to)) == pos;
    }
    bool sent = false;
    for (const IN_ADDR& ifAddr : interfaces) {
        if (setsockopt(sock, IPPROTO_IP, IP_MULTICAST_IF, (const char*)&ifAddr, sizeof(ifAddr)) != 0) {
            continue;
        }
        sent |= sendto(sock, (const char*)packet, pos, 0, (sockaddr*)&to, sizeof(to)) == pos;
    }
    return sent;
}

bool CCastDiscovery::ProbeAddress(const CString& ip, DWORD timeoutMs, CastDevice& device)
{
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        return false;
    }

    // its own list, so that a probe never disturbs a discovery that is running
    CCastDiscovery probe;

    SOCKET sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (sock != INVALID_SOCKET) {
        sockaddr_in local;
        ZeroMemory(&local, sizeof(local));
        local.sin_family = AF_INET;
        local.sin_addr.s_addr = htonl(INADDR_ANY);
        local.sin_port = 0; // ephemeral: the QU bit asks for the answer here

        sockaddr_in to;
        ZeroMemory(&to, sizeof(to));
        to.sin_family = AF_INET;
        to.sin_port = htons(MDNS_PORT);

        if (bind(sock, (sockaddr*)&local, sizeof(local)) == 0
                && inet_pton(AF_INET, CStringA(ip), &to.sin_addr) == 1) {
            BYTE packet[64];
            const int len = BuildQuery(packet);
            // twice: this is one datagram to one host and nothing resends it
            sendto(sock, (const char*)packet, len, 0, (sockaddr*)&to, sizeof(to));
            sendto(sock, (const char*)packet, len, 0, (sockaddr*)&to, sizeof(to));

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
                BYTE buf[4096];
                sockaddr_in from;
                int fromLen = sizeof(from);
                const int got = recvfrom(sock, (char*)buf, sizeof(buf), 0, (sockaddr*)&from, &fromLen);
                if (got <= 0) {
                    break;
                }
                if (from.sin_addr.s_addr != to.sin_addr.s_addr) {
                    continue; // an announcement from somebody else on the group
                }
                probe.ParseResponse(buf, got, from.sin_addr);
                if (!probe.m_devices.empty()) {
                    break; // the host answered, there is nothing else to wait for
                }
            }
        }
        closesocket(sock);
    }
    WSACleanup();

    if (probe.m_devices.empty()) {
        return false;
    }
    device = probe.m_devices.front();
    return true;
}

void CCastDiscovery::ParseResponse(const BYTE* packet, int len, const IN_ADDR& srcAddr)
{
    if (len < 12) {
        return;
    }

    UINT flags = (packet[2] << 8) | packet[3];
    if (!(flags & 0x8000)) {
        return; // not a response
    }
    int qdCount = (packet[4] << 8) | packet[5];
    int recordCount = ((packet[6] << 8) | packet[7])    // answers
                      + ((packet[8] << 8) | packet[9])  // authority
                      + ((packet[10] << 8) | packet[11]); // additional

    int pos = 12;

    // skip the question section
    for (int i = 0; i < qdCount; i++) {
        CStringA dummy;
        if (!ReadDnsName(packet, len, pos, dummy)) {
            return;
        }
        pos += 4; // QTYPE + QCLASS
        if (pos > len) {
            return;
        }
    }

    struct SrvData {
        CStringA target;
        UINT port;
    };
    std::vector<CStringA> instances;              // from PTR records
    std::map<CStringA, SrvData> srvRecords;       // instance -> target host + port
    std::map<CStringA, CStringA> aRecords;        // hostname -> IPv4 address
    struct TxtData {
        CStringA fn, md, id;
        UINT ca = 0;
    };
    std::map<CStringA, TxtData> txtRecords;       // instance -> TXT keys

    for (int i = 0; i < recordCount; i++) {
        CStringA rname;
        if (!ReadDnsName(packet, len, pos, rname)) {
            return;
        }
        if (pos + 10 > len) {
            return;
        }
        UINT type = (packet[pos] << 8) | packet[pos + 1];
        // class at pos+2 (cache-flush bit ignored), TTL at pos+4 (a simple
        // staleness window is used instead)
        int rdLen = (packet[pos + 8] << 8) | packet[pos + 9];
        pos += 10;
        if (pos + rdLen > len) {
            return;
        }
        const int rdEnd = pos + rdLen;

        switch (type) {
            case DNS_RECTYPE_PTR:
                if (rname.CompareNoCase(CAST_SERVICE_NAME) == 0) {
                    int p = pos;
                    CStringA instance;
                    if (ReadDnsName(packet, len, p, instance) && p <= rdEnd) {
                        instances.emplace_back(instance);
                    }
                }
                break;
            case DNS_RECTYPE_SRV:
                if (rdLen >= 6) {
                    // priority(2), weight(2), port(2), target
                    UINT port = (packet[pos + 4] << 8) | packet[pos + 5];
                    int p = pos + 6;
                    CStringA target;
                    if (ReadDnsName(packet, len, p, target) && p <= rdEnd) {
                        srvRecords[rname] = { target, port };
                    }
                }
                break;
            case DNS_RECTYPE_TXT: {
                TxtData txt;
                int p = pos;
                while (p < rdEnd) {
                    int entryLen = packet[p++];
                    if (p + entryLen > rdEnd) {
                        break;
                    }
                    CStringA entry(reinterpret_cast<const char*>(&packet[p]), entryLen);
                    p += entryLen;
                    int eq = entry.Find('=');
                    if (eq > 0) {
                        CStringA key = entry.Left(eq);
                        CStringA value = entry.Mid(eq + 1);
                        if (key == "fn") {
                            txt.fn = value;
                        } else if (key == "md") {
                            txt.md = value;
                        } else if (key == "id") {
                            txt.id = value;
                        } else if (key == "ca") {
                            // capability bitmask: 0x01 = video, 0x04 = audio
                            txt.ca = (UINT)atoi(value);
                        }
                    }
                }
                txtRecords[rname] = txt;
                break;
            }
            case DNS_RECTYPE_A:
                if (rdLen == 4) {
                    CStringA ip;
                    ip.Format("%u.%u.%u.%u", packet[pos], packet[pos + 1], packet[pos + 2], packet[pos + 3]);
                    aRecords[rname] = ip;
                }
                break;
        }
        pos = rdEnd;
    }

    // Some responses omit the PTR record, so also treat any SRV record for
    // the cast service as an instance.
    for (const auto& srv : srvRecords) {
        const CStringA& name = srv.first;
        int suffixLen = (int)strlen(CAST_SERVICE_SUFFIX);
        if (name.GetLength() > suffixLen && name.Right(suffixLen).CompareNoCase(CAST_SERVICE_SUFFIX) == 0
                && std::find(instances.cbegin(), instances.cend(), name) == instances.cend()) {
            instances.emplace_back(name);
        }
    }

    for (const CStringA& instance : instances) {
        auto itSrv = srvRecords.find(instance);
        if (itSrv == srvRecords.end()) {
            continue; // no SRV record, cannot reach the device yet
        }

        CStringA ip;
        auto itA = aRecords.find(itSrv->second.target);
        if (itA != aRecords.end()) {
            ip = itA->second;
        } else {
            // no A record in this packet, fall back to the sender address
            char buf[16] = { 0 };
            if (!inet_ntop(AF_INET, (PVOID)&srcAddr, buf, sizeof(buf))) {
                continue;
            }
            ip = buf;
        }

        // Only what this packet actually carries goes into the record; the
        // TXT keys are frequently absent from an announcement that renews
        // just SRV and A.
        CastDevice dev;
        dev.ipAddress = ip;
        dev.port = itSrv->second.port;
        auto itTxt = txtRecords.find(instance);
        if (itTxt != txtRecords.end()) {
            dev.friendlyName = UTF8To16(itTxt->second.fn);
            dev.model = UTF8To16(itTxt->second.md);
            dev.id = UTF8To16(itTxt->second.id);
            dev.capabilities = itTxt->second.ca;
        }
        dev.lastSeen = GetTickCount64();

        // the instance label is only a stand-in until a TXT record tells us
        // the real friendly name
        const int dot = instance.Find('.');
        UpdateDevice(dev, UTF8To16(dot > 0 ? instance.Left(dot) : instance));
    }
}

// Merges one announcement into the known device list. Records are merged field
// by field: a packet that carries no TXT section must never erase the name,
// id or capabilities an earlier one taught us, or the device would drop out of
// the menu and lose its identity.
void CCastDiscovery::UpdateDevice(const CastDevice& dev, const CString& fallbackName)
{
    std::lock_guard<std::mutex> lock(m_mutex);

    auto it = std::find_if(m_devices.begin(), m_devices.end(), [&](const CastDevice& d) {
        if (!dev.id.IsEmpty() && !d.id.IsEmpty()) {
            return d.id == dev.id;
        }
        return d.ipAddress == dev.ipAddress && d.port == dev.port;
    });

    if (it == m_devices.end()) {
        if (m_devices.size() >= MAX_DEVICES) {
            return; // the ones already known keep being updated
        }
        m_devices.emplace_back(dev);
        CastDevice& added = m_devices.back();
        if (added.friendlyName.IsEmpty()) {
            added.friendlyName = fallbackName;
        }
        TRACE(_T("CastDiscovery: found \"%s\" at %s:%u (%s)\n"), added.friendlyName.GetString(),
              added.ipAddress.GetString(), added.port, added.model.GetString());
        return;
    }

    auto merge = [](CString& dst, const CString& src) {
        if (!src.IsEmpty()) {
            dst = src;
        }
    };
    merge(it->friendlyName, dev.friendlyName);
    merge(it->model, dev.model);
    merge(it->id, dev.id);
    merge(it->ipAddress, dev.ipAddress);
    if (dev.port) {
        it->port = dev.port;
    }
    if (dev.capabilities) {
        it->capabilities = dev.capabilities;
    }
    it->lastSeen = dev.lastSeen;
}

void CCastDiscovery::ExpireStaleDevices()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    const ULONGLONG now = GetTickCount64();
    m_devices.erase(std::remove_if(m_devices.begin(), m_devices.end(), [now](const CastDevice& d) {
        return now - d.lastSeen > DEVICE_STALE_MS;
    }), m_devices.end());
}
