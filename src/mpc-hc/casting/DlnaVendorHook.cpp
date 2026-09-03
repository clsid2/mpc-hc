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
#include "DlnaVendorHook.h"
#include "DlnaDiscovery.h" // the shared HTTP client and XML scanner
#include "rapidjson/include/rapidjson/document.h"
#include <ws2tcpip.h>
#include <iphlpapi.h>

#pragma comment(lib, "iphlpapi.lib")

// Yamaha MusicCast, advertised as X_YamahaExtendedControl: plain HTTP GETs
// answering JSON, on the port the vendor block gives (80), not the UPnP one.
#define YXC_GET_STATUS  "main/getStatus"
#define YXC_SET_POWER   "main/setPower?power=on"
#define YXC_SET_INPUT   "main/setInput?input=server"
#define YXC_NET_INPUT   "server" // the input a pushed http-get stream plays on

#define POWER_ON_TIMEOUT_MS 8000 // an amplifier leaving standby takes seconds
#define POWER_POLL_MS       500
#define INPUT_SETTLE_MS     1000

namespace
{
    // Waits, but gives up the moment the caller wants out. hAbort may be null.
    bool Aborted(HANDLE hAbort, DWORD ms)
    {
        if (!hAbort) {
            Sleep(ms);
            return false;
        }
        return WaitForSingleObject(hAbort, ms) == WAIT_OBJECT_0;
    }

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
        if (it != v.MemberEnd() && it->value.IsInt()) {
            return it->value.GetInt();
        }
        return def;
    }

    // Brings a MusicCast receiver out of network standby and onto the input a
    // pushed stream plays on. Volume and mute are never touched: what comes
    // out of the speakers is the owner's business, not ours.
    class CYamahaMusicCastHook final : public CDlnaVendorHook
    {
    public:
        explicit CYamahaMusicCastHook(const CString& baseURL) : m_baseURL(baseURL) {}

        void PrepareForCasting(HANDLE hAbort) override;

    private:
        bool Call(const CStringA& path, rapidjson::Document& doc, HANDLE hAbort) const;
        bool GetStatus(CStringA& power, CStringA& input, HANDLE hAbort) const;

        CString m_baseURL; // ends with a slash
    };

    bool CYamahaMusicCastHook::Call(const CStringA& path, rapidjson::Document& doc, HANDLE hAbort) const
    {
        CStringA json;
        if (!Dlna::HttpGet(m_baseURL + CString(path), json, hAbort)) {
            return false;
        }
        doc.Parse(json.GetString());
        if (doc.HasParseError() || !doc.IsObject()) {
            return false;
        }
        const int code = GetJsonInt(doc, "response_code", -1);
        if (code != 0) {
            TRACE(_T("DlnaVendor: MusicCast refused %hs with response_code %d\n"), path.GetString(), code);
            return false;
        }
        return true;
    }

    bool CYamahaMusicCastHook::GetStatus(CStringA& power, CStringA& input, HANDLE hAbort) const
    {
        rapidjson::Document doc;
        if (!Call(YXC_GET_STATUS, doc, hAbort)) {
            return false;
        }
        power = GetJsonString(doc, "power");
        input = GetJsonString(doc, "input");
        return !power.IsEmpty();
    }

    void CYamahaMusicCastHook::PrepareForCasting(HANDLE hAbort)
    {
        CStringA power, input;
        if (!GetStatus(power, input, hAbort)) {
            return; // no usable vendor API after all; the plain DLNA path stands
        }
        if (power == "on" && input == YXC_NET_INPUT) {
            TRACE(_T("DlnaVendor: MusicCast is on and on input %hs, nothing to prepare\n"),
                  input.GetString());
            return;
        }

        if (power != "on") {
            TRACE(_T("DlnaVendor: MusicCast reports power=%hs, switching it on\n"), power.GetString());
            rapidjson::Document doc;
            if (!Call(YXC_SET_POWER, doc, hAbort)) {
                return;
            }
            // The unit answers getStatus throughout, so it is asked until it
            // admits to being on rather than waited on blindly.
            const ULONGLONG deadline = GetTickCount64() + POWER_ON_TIMEOUT_MS;
            do {
                if (Aborted(hAbort, POWER_POLL_MS) || !GetStatus(power, input, hAbort)) {
                    return;
                }
            } while (power != "on" && GetTickCount64() < deadline);
            if (power != "on") {
                TRACE(_T("DlnaVendor: MusicCast did not come out of standby in time\n"));
                return; // the load goes out anyway; it may still be heard
            }
        }

        if (input != YXC_NET_INPUT) {
            TRACE(_T("DlnaVendor: MusicCast is on input %hs, switching to %hs\n"),
                  input.GetString(), YXC_NET_INPUT);
            rapidjson::Document doc;
            if (Call(YXC_SET_INPUT, doc, hAbort)) {
                Aborted(hAbort, INPUT_SETTLE_MS); // let the switch settle
            }
        }

        // Neither the power nor the input is put back when casting stops: that
        // is how AirPlay and MusicCast itself leave a unit, and switching a
        // receiver off behind the user's back is worse than leaving it on.
    }
}

void DlnaVendor::ParseDescription(const CStringA& xml, const CString& deviceIp, DlnaVendorInfo& info)
{
    info = DlnaVendorInfo();

    // <yamaha:X_device> sits beside <device>, not inside it, and carries its
    // own URL base. That base is deliberately not used for the standard
    // control URLs, which have to keep resolving against the description's own
    // address and port. And like everything else a device says about where to
    // reach it, what it resolves to has to be on the device: a description
    // naming an API on some other host gets no hook at all.
    int innerStart = 0, innerEnd = 0, next = 0;
    if (!Dlna::FindElement(xml, "X_device", 0, innerStart, innerEnd, next)) {
        return;
    }
    const CStringA block = xml.Mid(innerStart, innerEnd - innerStart);
    const CString base = UTF8To16(Dlna::GetElementText(block, "X_URLBase"));
    if (base.IsEmpty()) {
        return; // without it there is no telling which port the API is on
    }

    for (int pos = 0;;) {
        if (!Dlna::FindElement(block, "X_service", pos, innerStart, innerEnd, next)) {
            break;
        }
        const CStringA service = block.Mid(innerStart, innerEnd - innerStart);
        const CStringA specType = Dlna::GetElementText(service, "X_specType");
        if (specType.Find("X_YamahaExtendedControl") >= 0) {
            CString url = Dlna::ResolveURL(base, UTF8To16(Dlna::GetElementText(service, "X_yxcControlURL")));
            if (!url.IsEmpty() && Dlna::UrlIsOnDevice(url, deviceIp)) {
                if (url.Right(1) != _T("/")) {
                    url += _T('/');
                }
                info.yamahaExtendedControlURL = url;
            }
        } else if (specType.Find("X_YamahaRemoteControl") >= 0) {
            const CString url =
                Dlna::ResolveURL(base, UTF8To16(Dlna::GetElementText(service, "X_controlURL")));
            if (Dlna::UrlIsOnDevice(url, deviceIp)) {
                info.yamahaRemoteControlURL = url;
            }
        }
        pos = next;
    }

    if (!info.yamahaExtendedControlURL.IsEmpty()) {
        TRACE(_T("DlnaVendor: MusicCast API at %s\n"), info.yamahaExtendedControlURL.GetString());
    }
}

void DlnaVendor::WakeOnLan(const CString& deviceIp)
{
    IN_ADDR device;
    if (InetPton(AF_INET, deviceIp, &device) != 1) {
        return;
    }

    // Only the neighbour cache is consulted. Resolving an address that is not
    // in it would mean an ARP exchange with a device that is by definition not
    // answering, and this must never hold the caller up.
    BYTE mac[6];
    bool found = false;
    MIB_IPNET_TABLE2* pTable = nullptr;
    if (GetIpNetTable2(AF_INET, &pTable) == NO_ERROR && pTable) {
        for (ULONG i = 0; i < pTable->NumEntries && !found; i++) {
            const MIB_IPNET_ROW2& row = pTable->Table[i];
            if (row.Address.si_family == AF_INET
                    && row.Address.Ipv4.sin_addr.S_un.S_addr == device.S_un.S_addr
                    && row.PhysicalAddressLength == sizeof(mac)) {
                memcpy(mac, row.PhysicalAddress, sizeof(mac));
                found = true;
            }
        }
    }
    if (pTable) {
        FreeMibTable(pTable);
    }
    if (!found) {
        return;
    }

    BYTE magic[6 + 16 * sizeof(mac)];
    memset(magic, 0xFF, 6);
    for (int i = 0; i < 16; i++) {
        memcpy(magic + 6 + i * sizeof(mac), mac, sizeof(mac));
    }

    SOCKET sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (sock == INVALID_SOCKET) {
        return;
    }
    // This machine is usually multi-homed, and a broadcast left to the routing
    // table goes out whichever interface it picks -- not necessarily the one
    // the device is on. Binding the source address settles that.
    IN_ADDR local;
    if (InetPton(AF_INET, Dlna::LocalAddressFor(deviceIp), &local) == 1) {
        sockaddr_in from;
        ZeroMemory(&from, sizeof(from));
        from.sin_family = AF_INET;
        from.sin_addr = local;
        bind(sock, (sockaddr*)&from, sizeof(from));
    }
    const BOOL broadcast = TRUE;
    setsockopt(sock, SOL_SOCKET, SO_BROADCAST, (const char*)&broadcast, sizeof(broadcast));

    // Broadcast is what reaches a set whose interface has gone quiet; the
    // unicast copy is for one that still answers ARP but is asleep behind it.
    sockaddr_in to;
    ZeroMemory(&to, sizeof(to));
    to.sin_family = AF_INET;
    to.sin_port = htons(9); // the discard port, where magic packets live
    for (ULONG address : { INADDR_BROADCAST, device.S_un.S_addr }) {
        to.sin_addr.S_un.S_addr = address;
        sendto(sock, (const char*)magic, sizeof(magic), 0, (sockaddr*)&to, sizeof(to));
    }
    closesocket(sock);

    TRACE(_T("DlnaVendor: wake-on-LAN sent to %s (%02x:%02x:%02x:%02x:%02x:%02x)\n"), deviceIp.GetString(),
          mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
}

std::unique_ptr<CDlnaVendorHook> DlnaVendor::CreateHook(const DlnaVendorInfo& info)
{
    // The parsed block is what selects the implementation: only a Yamaha
    // description can have filled this in.
    if (!info.yamahaExtendedControlURL.IsEmpty()) {
        return std::make_unique<CYamahaMusicCastHook>(info.yamahaExtendedControlURL);
    }
    return nullptr;
}
