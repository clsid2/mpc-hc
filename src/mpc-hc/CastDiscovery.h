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

#include <winsock2.h>
#include <mutex>
#include <vector>

struct CastDevice {
    CString friendlyName;   // TXT record "fn"
    CString model;          // TXT record "md", verbatim (needed for per-model quirks)
    CString id;             // TXT record "id"
    CString ipAddress;      // dotted IPv4 address
    UINT port = 0;          // SRV record port, normally 8009
    UINT capabilities = 0;  // TXT record "ca", capability bitmask
    ULONGLONG lastSeen = 0; // GetTickCount64() when the device last answered

    bool SupportsVideo() const { return !!(capabilities & 0x01); }
    bool SupportsAudio() const { return !!(capabilities & 0x04); }
};

// Every usable IPv4 interface of this machine (up, not loopback, not a
// link-local autoconfiguration address). Multicast discovery has to query all
// of them: a socket bound to INADDR_ANY sends through the one interface the
// routing table picks, which on a machine with Hyper-V, WSL or VPN adapters is
// regularly a virtual switch with no media devices behind it. Shared with the
// DLNA discovery.
std::vector<IN_ADDR> CastEnumLocalIPv4Interfaces();

// Phase 1 of Chromecast support: discovers Google Cast devices on the local
// network via mDNS/DNS-SD queries for _googlecast._tcp.local. Later phases
// build on this class (CastV2 control channel, media server, UI wiring).
class CCastDiscovery
{
public:
    CCastDiscovery();
    ~CCastDiscovery();

    CCastDiscovery(const CCastDiscovery&) = delete;
    CCastDiscovery& operator=(const CCastDiscovery&) = delete;

    bool Start();
    void Stop();
    bool IsRunning() const { return m_hThread != nullptr; }

    // Thread-safe snapshot of the currently known devices
    std::vector<CastDevice> GetDevices();

private:
    static DWORD WINAPI StaticThreadProc(LPVOID lpParam);
    DWORD ThreadProc();

    SOCKET OpenSocket();
    bool SendQuery(SOCKET sock);
    void ParseResponse(const BYTE* packet, int len, const IN_ADDR& srcAddr);
    void UpdateDevice(const CastDevice& dev, const CString& fallbackName);
    void ExpireStaleDevices();

    std::mutex m_mutex; // guards m_devices
    std::vector<CastDevice> m_devices;

    HANDLE m_hThread = nullptr;
    HANDLE m_hStopEvent = nullptr;
};
