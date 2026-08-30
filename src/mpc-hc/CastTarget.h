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

#include <atomic>
#include <vector>

// Phase 4 of Chromecast support: the protocol-agnostic interface between the
// player UI and a cast target. The UI only sees generic devices and a
// simplified playback state; all protocol glue (Chromecast now, DLNA later)
// lives in the implementations.

enum class CastTargetState {
    Idle,       // no active session
    Connecting, // connecting to the device
    Loading,    // media handed to the device, waiting for playback to start
    Playing,
    Paused,
    Buffering,
    Failed,     // connection or load failed
    TakenOver,  // another sender took the device over
    Ended,      // media finished or was stopped on the device
};

// What the player's own graph knows about the file that is about to be cast.
// A protocol that has to name the content precisely -- DLNA renderers from
// Samsung and Sony refuse or mishandle a stream that carries no profile name --
// derives that name from this. Every field may be left Unknown or zero, which
// simply means nothing is claimed about it.
struct CastMediaInfo {
    enum class Video { Unknown, H264, MPEG2 };
    enum class Audio { Unknown, AAC, MP3, WMA };

    Video video = Video::Unknown;
    Audio audio = Audio::Unknown;
    int width = 0;      // coded video size, 0 when unknown
    int height = 0;
    int sampleRate = 0; // audio, 0 when unknown
    int channels = 0;
};

enum class CastProtocol { Chromecast, Dlna };

struct CastTargetDevice {
    CastProtocol protocol = CastProtocol::Chromecast;
    CString name;    // friendly name as the device advertises it
    CString id;      // stable identifier, passed back to Connect()
    CString address; // dotted IPv4 address, so a device can also be named by it
    UINT port = 0;   // 0 when the protocol has a fixed one
    CString location; // DLNA description URL, empty for Chromecast
    CString formats;  // DLNA ConnectionManager Sink list, empty when unknown
    bool supportsVideo = false;
    bool supportsAudio = false;
};

// A device the user has kept. The cast submenu is built from these alone, so
// an entry has to carry everything the menu needs to judge the device against
// the loaded file, and everything a session needs to reach it again with no
// discovery running. The advertised name is kept beside the one the user gave
// it, so that a rescan never overwrites what was typed.
struct CastSavedDevice : CastTargetDevice {
    CString userName; // name the user gave it, empty when there is none

    CString DisplayName() const { return userName.IsEmpty() ? name : userName; }
};

// Session generations are drawn from one counter shared by every target, so a
// notification queued by one protocol can never be mistaken for a live session
// of another once both feed the same menu.
inline UINT CastNextSessionGeneration()
{
    static std::atomic<UINT> counter(0);
    return ++counter;
}

class CCastTarget
{
public:
    virtual ~CCastTarget() = default;

    // Device discovery; runs in the background once started. The device list
    // is polled with GetDevices() whenever it is about to be shown.
    virtual bool StartDiscovery() = 0;
    virtual void StopDiscovery() = 0;
    virtual bool IsDiscoveryRunning() const = 0;
    virtual std::vector<CastTargetDevice> GetDevices() = 0;

    // stateMsg is posted to hNotifyWnd with the new CastTargetState in wParam
    // and the session generation it belongs to in lParam whenever the
    // simplified state changes.
    virtual void SetNotifyWindow(HWND hNotifyWnd, UINT stateMsg) = 0;

    // Looks for a device at one address only, with a unicast query rather than
    // the multicast discovery, and without starting a thread. This is how a
    // renderer that answers a multicast search unreliably, or one on a network
    // the multicast never reaches, can still be named. protocol says which
    // protocol to ask; a target that does not speak it says so at once.
    // Blocks the calling thread for at most timeoutMs.
    virtual bool ProbeAddress(CastProtocol protocol, const CString& address, UINT port,
                              DWORD timeoutMs, CastTargetDevice& device) = 0;

    // Session control; all calls are made from the UI thread.
    virtual bool Connect(const CString& deviceId) = 0;

    // Connects to a device out of the saved list instead of out of a live
    // discovery snapshot. The stored address is tried first; when it does not
    // answer, one short search re-resolves the device by its stable id, so a
    // device that DHCP moved is found again rather than declared broken.
    // saved is updated in place with what the device turned out to be. Both
    // budgets are in milliseconds and both are spent on the calling thread.
    virtual bool ConnectSaved(CastSavedDevice& saved, DWORD directMs, DWORD searchMs) = 0;
    virtual bool IsCasting() const = 0; // Connect() succeeded and StopCasting() not called yet
    virtual CString GetDeviceId() const = 0;   // active device, empty when not casting
    virtual CString GetDeviceName() const = 0; // active device display name

    // Incremented by every Connect(); a notification whose generation is no
    // longer the current one was queued by a session that has been superseded.
    virtual UINT GetSessionGeneration() const = 0;

    // Whether the device can play the file without transcoding. The device is
    // named because capabilities differ per device on protocols that report
    // them (DLNA), while every Chromecast receiver is the same.
    virtual bool CanCastFile(const CString& deviceId, const CString& path) = 0;

    // The same question for a device from the saved list, answered from what
    // was recorded about it rather than from a discovery that is not running.
    virtual bool CanCastFileSaved(const CastSavedDevice& saved, const CString& path) = 0;

    // Serves filePath to the device and loads it, seeking to startSec once
    // the device reports playback. info is what the local graph knows about
    // the file and may be left default.
    virtual void LoadMedia(const CString& filePath, const CString& title, double durationSec, double startSec,
                           const CastMediaInfo& info) = 0;

    virtual void Play() = 0;
    virtual void Pause() = 0;
    virtual void Seek(double seconds) = 0;
    // False once the device has been found not to support seeking at all, so
    // the UI can disable the seekbar instead of pretending it works
    virtual bool CanSeek() const { return true; }
    virtual void SetVolume(double level, bool muted) = 0; // level 0.0 - 1.0
    virtual void StopCasting() = 0; // polite stop and disconnect, back to Idle

    virtual CastTargetState GetState() const = 0;
    // Human-readable detail for the Failed state, empty when there is none
    virtual CString GetFailureReason() const { return CString(); }
    virtual double GetPosition() const = 0; // seconds, device-derived
    virtual double GetDuration() const = 0; // seconds, 0 while unknown
};
