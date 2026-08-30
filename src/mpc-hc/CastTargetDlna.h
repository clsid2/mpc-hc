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

#include "CastTarget.h"
#include "DlnaDiscovery.h"
#include "CastMediaServer.h"
#include "DlnaVendorHook.h"
#include <memory>
#include <mutex>
#include <vector>

// The DLNA/UPnP-AV implementation of CCastTarget: composes the SSDP discovery
// with the local HTTP media server and drives the renderer's AVTransport and
// RenderingControl services over SOAP. UPnP eventing is not used; the device
// is polled instead, on the same worker thread that runs the commands, so the
// UI thread never waits for a SOAP exchange.
class CDlnaTarget : public CCastTarget
{
public:
    CDlnaTarget() = default;
    ~CDlnaTarget();

    CDlnaTarget(const CDlnaTarget&) = delete;
    CDlnaTarget& operator=(const CDlnaTarget&) = delete;

    // CCastTarget
    bool StartDiscovery() override;
    void StopDiscovery() override;
    bool IsDiscoveryRunning() const override { return m_discovery.IsRunning(); }
    std::vector<CastTargetDevice> GetDevices() override;

    void SetNotifyWindow(HWND hNotifyWnd, UINT stateMsg) override;

    bool Connect(const CString& deviceId) override;
    bool IsCasting() const override { return m_casting; }
    CString GetDeviceId() const override { return m_deviceId; }
    CString GetDeviceName() const override { return m_deviceName; }
    UINT GetSessionGeneration() const override { return m_generation; }

    bool CanCastFile(const CString& deviceId, const CString& path) override;

    void LoadMedia(const CString& filePath, const CString& title, double durationSec, double startSec,
                   const CastMediaInfo& info) override;

    void Play() override;
    void Pause() override;
    void Seek(double seconds) override;
    bool CanSeek() const override;
    void SetVolume(double level, bool muted) override;
    void StopCasting() override;

    CastTargetState GetState() const override;
    CString GetFailureReason() const override;
    double GetPosition() const override;
    double GetDuration() const override;

    // The "contentFeatures.dlna.org" value for a file: a DLNA.ORG_PN profile name whenever
    // the container and the codecs the local graph reports name one beyond doubt, followed
    // by the operation, conversion and flags a seekable untranscoded stream advertises.
    // The same string goes into the DIDL <res> protocolInfo and into the HTTP response, and
    // the two have to agree. Public so that it can be exercised on its own.
    static CStringA ContentFeatures(const CString& path, const CStringA& mime, const CastMediaInfo& info);

private:
    struct Command {
        enum class Type { Load, Play, Pause, Seek, SetVolume, Stop };
        Type type;
        CString url, mime, title;  // Load
        CStringA features;         // Load, the contentFeatures the DIDL must carry
        ULONGLONG size = 0;        // Load, served byte count, 0 when unknown
        double duration = 0.0;     // Load
        double param = 0.0;        // Load start / Seek position / SetVolume level
        bool muted = false;        // SetVolume
    };

    static DWORD WINAPI StaticThreadProc(LPVOID lpParam);
    DWORD ThreadProc();
    void StopWorker();
    void QueueCommand(Command&& cmd);

    // worker thread only
    void ProcessCommands();
    void RunCommand(const Command& cmd);
    void Poll();
    bool AvTransport(const CStringA& action, const CStringA& args, CStringA& response);
    void DetermineSeekModes();
    double ClampSeekTarget(double seconds) const;
    bool SeekDevice(double& seconds);
    void SetCanSeek(bool canSeek);
    void SetState(CastTargetState state);
    void Fail(const CString& reason);
    void UpdatePosition(double position, double duration, bool fromPoll = false);

    static bool SinkAccepts(const CStringA& sink, const CStringA& mime);
    static CStringA BuildMetadata(const Command& cmd);
    static CStringA FormatDuration(double seconds, bool withMilliseconds);
    static double ParseDuration(const CStringA& text); // < 0 when not a time

    CDlnaDiscovery m_discovery;
    CCastMediaServer m_server;

    // UI thread only
    bool m_casting = false;
    UINT m_generation = 0; // bumped by every Connect(), carried by notifications
    CString m_deviceId;
    CString m_deviceName;
    CString m_deviceAddress; // the device's own dotted IPv4 address
    CString m_localAddress; // the address of ours this device can reach
    // Made before the worker starts and destroyed after it is joined, so the
    // worker may use it without any further synchronization.
    std::unique_ptr<CDlnaVendorHook> m_vendorHook;

    // shared state, guarded by m_mutex
    mutable std::mutex m_mutex;
    CastTargetState m_state = CastTargetState::Idle;
    CString m_failReason;
    bool m_canSeek = true;         // cleared once the device is found unable to seek
    double m_position = 0.0;       // seconds, as last reported by the device
    double m_duration = 0.0;       // seconds, 0 while unknown
    ULONGLONG m_positionTick = 0;  // GetTickCount64() when m_position was read
    bool m_positionAdvancing = false; // the device clock was seen to move
    HWND m_hNotifyWnd = nullptr;
    UINT m_stateMsg = 0;
    std::vector<Command> m_commands;

    // worker thread only
    CString m_controlURL;   // AVTransport
    CString m_scpdURL;      // AVTransport service description, may be empty
    CString m_volumeURL;    // RenderingControl, may be empty
    CString m_mediaURL;     // what we handed to the device, for takeover detection
    double m_localDuration = 0.0; // the length our own graph knows, 0 when it does not
    bool m_hasPlayed = false;   // the device reported PLAYING at least once
    bool m_stopIssued = false;  // our own STOP, so the STOPPED it causes is not an end
    double m_pendingSeek = -1.0; // seconds, applied once playback has started
    int m_pollFailures = 0;
    // Seek units the device accepts, in the order they are tried. Taken from
    // the AVTransport service description once per session; a unit the device
    // then faults on is dropped and not tried again.
    std::vector<CStringA> m_seekModes;
    bool m_seekModesKnown = false;

    HANDLE m_hThread = nullptr;
    HANDLE m_hStopEvent = nullptr;
    HANDLE m_hCommandEvent = nullptr;
    HANDLE m_hStopSentEvent = nullptr; // signalled once a media STOP went out
};
