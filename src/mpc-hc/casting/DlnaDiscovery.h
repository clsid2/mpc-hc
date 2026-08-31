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

#include "DlnaVendorHook.h"
#include <winsock2.h>
#include <deque>
#include <mutex>
#include <vector>

struct DlnaDevice {
    CString friendlyName;
    CString manufacturer;
    CString modelName;
    CString udn;                  // "uuid:..." from the description, the stable id
    CString location;             // description URL advertised in the SSDP LOCATION header
    CString ipAddress;            // dotted IPv4 address the device answered from
    CString avTransportURL;       // absolute control URL; a device without one is unusable
    CString avTransportSCPDURL;   // its service description, empty when not offered
    CString renderingControlURL;  // absolute control URL, empty when not offered
    CString connectionManagerURL; // absolute control URL, empty when not offered
    CStringA sinkProtocolInfo;    // ConnectionManager Sink list, empty while unknown
    DlnaVendorInfo vendor;        // manufacturer extensions, empty for most devices
    // Until the sink list is known both are assumed, so a renderer that does
    // not answer GetProtocolInfo still shows up for every kind of media.
    bool supportsVideo = true;
    bool supportsAudio = true;
    ULONGLONG lastSeen = 0;       // GetTickCount64() when the device last announced itself
};

// The DLNA/UPnP-AV counterpart of CCastDiscovery: finds MediaRenderer devices
// on the local network with SSDP (M-SEARCH plus NOTIFY announcements), then
// fetches and parses each device description to learn its control URLs, and
// asks its ConnectionManager which formats it accepts.
class CDlnaDiscovery
{
public:
    CDlnaDiscovery();
    ~CDlnaDiscovery();

    CDlnaDiscovery(const CDlnaDiscovery&) = delete;
    CDlnaDiscovery& operator=(const CDlnaDiscovery&) = delete;

    bool Start();
    void Stop();
    bool IsRunning() const { return m_hThread != nullptr; }

    // Thread-safe snapshot of the currently known devices
    std::vector<DlnaDevice> GetDevices();

    // Describes one device directly, without the worker: an M-SEARCH sent
    // unicast to a single host rather than to the SSDP group, then that host's
    // description and format list fetched from it. Nothing is started and no
    // state is shared, so a probe never disturbs a discovery that is running.
    // The search waits at most timeoutMs, and the description fetched from
    // whatever answers it gets no longer than that again, so the call always
    // ends in about twice what it was asked to wait.
    static bool ProbeAddress(const CString& ip, UINT port, DWORD timeoutMs, DlnaDevice& device);

    // The same for a device whose description URL is already known, which is
    // how a saved device is found again at the place it was last seen. There
    // is no search to wait for here, so timeoutMs is what the description and
    // the format list that follows it have between them.
    static bool ProbeLocation(const CString& location, const CString& ip, DWORD timeoutMs,
                              DlnaDevice& device);

private:
    // A device description that has been announced but not fetched yet, or a
    // device whose accepted formats are still to be asked for. Exactly one of
    // these is worked off per loop iteration so that shutdown is never held up
    // by more than a single HTTP exchange.
    struct ProbeTask {
        enum class Type { Describe, ProtocolInfo };
        Type type;
        CString location; // Describe
        CString ip;
        CString udn;      // ProtocolInfo
        CString controlURL;
    };

    static DWORD WINAPI StaticThreadProc(LPVOID lpParam);
    DWORD ThreadProc();

    SOCKET OpenSearchSocket();
    SOCKET OpenNotifySocket();
    void SendSearch(SOCKET sock);
    void ReceivePackets(SOCKET sock);
    void ParseSsdp(const CStringA& packet, const IN_ADDR& srcAddr);
    void QueueProbe(ProbeTask&& task);
    void RunProbe(const ProbeTask& task);
    void DrainProbes();
    void RefreshDevice(const CString& udn);
    void RemoveDevice(const CString& udn);
    void ExpireStaleDevices();

    std::mutex m_mutex; // guards m_devices
    std::vector<DlnaDevice> m_devices;

    // A description URL that could not be turned into a usable device. It is
    // remembered for a while so that an unreachable or uninteresting device
    // does not get refetched on every announcement it sends.
    struct FailedProbe {
        CString location;
        ULONGLONG tick;
    };

    // worker thread only
    std::deque<ProbeTask> m_probeQueue;
    std::vector<FailedProbe> m_failedProbes;

    // When a probe runs on the caller's thread it is only allowed so long, and
    // the HTTP exchanges it makes have to fit inside that. Zero on the worker,
    // which has its stop event to end it instead. Probe thread only.
    ULONGLONG m_probeDeadline = 0;
    DWORD ProbeBudget() const;

    HANDLE m_hThread = nullptr;
    HANDLE m_hStopEvent = nullptr;
};

// Helpers shared by the discovery and the DLNA cast target. The HTTP client is
// a small raw-socket one rather than WinInet: these calls run on plain worker
// threads, must be abortable the instant the stop event is signalled, and must
// not be routed through a configured proxy on their way to a LAN device.
namespace Dlna
{
    // Performs one HTTP/1.1 exchange with an http:// URL. hAbort may be null;
    // when it is signalled the exchange is given up at once. The response body
    // is capped, so a hostile device cannot exhaust memory here. budgetMs
    // shortens the built-in timeouts for a caller that cannot wait that long,
    // and 0 means the exchange gets them in full.
    bool HttpRequest(const CString& url, const CStringA& method, const CStringA& extraHeaders,
                     const CStringA& body, CStringA& response, int& statusCode, HANDLE hAbort,
                     DWORD budgetMs = 0);
    bool HttpGet(const CString& url, CStringA& response, HANDLE hAbort, DWORD budgetMs = 0);

    // How a device answered an action it refused: the HTTP status the answer
    // came with -- zero when the device said nothing at all, which tells a
    // refusal from a lost packet -- and the UPnP errorCode out of the fault
    // body, zero when it carried none. The code is what says whether the
    // action itself is unsupported or only this one call of it failed.
    struct SoapStatus {
        int httpStatus = 0;
        int errorCode = 0;
    };

    // Invokes a UPnP action over SOAP. Returns false on a transport error or a
    // SOAP fault; fault then holds the UPnP error the device reported and
    // pStatus, when given, the machine-readable part of it.
    bool SoapCall(const CString& controlURL, const CStringA& serviceType, const CStringA& action,
                  const CStringA& argsXml, CStringA& response, CString& fault, HANDLE hAbort,
                  SoapStatus* pStatus = nullptr, DWORD budgetMs = 0);

    // Local IPv4 address that the given device would see us at, empty on error
    CString LocalAddressFor(const CString& deviceIp);

    // Tolerant, bounds-safe extraction from untrusted device XML. Namespace
    // prefixes are ignored: only the local part of a tag name is compared.
    // Comments and CDATA sections are skipped rather than interpreted.
    bool FindElement(const CStringA& xml, const char* localName, int from,
                     int& innerStart, int& innerEnd, int& next);
    CStringA GetElementText(const CStringA& xml, const char* localName, int from = 0);
    CStringA XmlUnescape(const CStringA& s);
    CStringA XmlEscape(const CStringA& s);

    // Resolves a possibly relative URL from a device description against the
    // description's own URL (or its URLBase element)
    CString ResolveURL(const CString& base, const CString& relative);
}
