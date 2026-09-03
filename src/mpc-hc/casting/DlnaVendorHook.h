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

// UPnP/AVTransport has no notion of power. An AV receiver in network standby
// answers SOAP perfectly, accepts SetAVTransportURI and even reports PLAYING,
// yet never fetches the media URL because its amplifier is asleep. What wakes
// such a unit is the manufacturer's own API, which it advertises next to the
// standard part of its device description. All of that lives here so that the
// DLNA target keeps speaking nothing but UPnP.

// The vendor extensions found in a device description. Every field is empty
// for a device that advertises none, which is the normal case.
struct DlnaVendorInfo {
    CString yamahaExtendedControlURL; // MusicCast (YXC) base, e.g. http://host:80/YamahaExtendedControl/v1/
    CString yamahaRemoteControlURL;   // the older XML remote control service, if offered

    bool IsEmpty() const {
        return yamahaExtendedControlURL.IsEmpty() && yamahaRemoteControlURL.IsEmpty();
    }
};

class CDlnaVendorHook
{
public:
    virtual ~CDlnaVendorHook() = default;

    // Called on the cast worker thread before the first SetAVTransportURI, to
    // get the device into a state where it will actually play what it is
    // handed. Best effort throughout: an unreachable or unhappy device is left
    // alone and the plain DLNA path carries on. hAbort, when signalled, ends
    // the attempt at once; it may be null.
    virtual void PrepareForCasting(HANDLE hAbort) = 0;
};

namespace DlnaVendor
{
    // Scans a device description for the vendor blocks we know. Absence is the
    // normal case and is silent.
    // deviceIp is the address the description was fetched from: a vendor
    // API is only taken when it is on that device
    void ParseDescription(const CStringA& xml, const CString& deviceIp, DlnaVendorInfo& info);

    // Null unless the device advertised something we know how to drive
    std::unique_ptr<CDlnaVendorHook> CreateHook(const DlnaVendorInfo& info);

    // Sends a wake-on-LAN magic packet to whatever MAC the machine's ARP cache
    // holds for the device. This is what brings an LG TV back from "Mobile TV
    // On" and several other sets out of standby, and it is what every renderer
    // gets, vendor block or not. Best effort throughout: the cache is only
    // read, never populated, so nothing here waits on the network, and a
    // device that is already awake ignores the packet.
    void WakeOnLan(const CString& deviceIp);
}
