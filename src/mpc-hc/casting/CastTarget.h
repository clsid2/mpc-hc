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

// Kept beside the states it names, for the log.
inline LPCTSTR CastTargetStateName(CastTargetState state)
{
    switch (state) {
        case CastTargetState::Idle:
            return _T("Idle");
        case CastTargetState::Connecting:
            return _T("Connecting");
        case CastTargetState::Loading:
            return _T("Loading");
        case CastTargetState::Playing:
            return _T("Playing");
        case CastTargetState::Paused:
            return _T("Paused");
        case CastTargetState::Buffering:
            return _T("Buffering");
        case CastTargetState::Failed:
            return _T("Failed");
        case CastTargetState::TakenOver:
            return _T("TakenOver");
        case CastTargetState::Ended:
            return _T("Ended");
        default:
            return _T("?");
    }
}

// What MediaInfo says about the file that is about to be cast. A protocol
// that has to name the content precisely -- DLNA renderers from Samsung and
// Sony refuse or mishandle a stream that carries no profile name -- derives
// that name from this, and a Chromecast is judged against what its receiver
// plays. Every field may be left Unknown or zero, which simply means nothing
// is claimed about it.
struct CastMediaInfo {
    enum class Video { Unknown, H264, HEVC, MPEG2, VP8, VP9, AV1 };
    enum class Audio { Unknown, AAC, MP3, WMA, FLAC, Opus, Vorbis, LPCM, AC3, EAC3, DTS, TrueHD };

    Video video = Video::Unknown;
    Audio audio = Audio::Unknown;
    int width = 0;      // coded video size, 0 when unknown
    int height = 0;
    int sampleRate = 0; // audio, 0 when unknown
    int channels = 0;
};

// The names of what MediaInfo found, kept beside the values they name so
// that the two cannot drift apart. Only the log has any use for them.
inline LPCTSTR CastVideoCodecName(CastMediaInfo::Video video)
{
    switch (video) {
        case CastMediaInfo::Video::H264:
            return _T("H.264");
        case CastMediaInfo::Video::HEVC:
            return _T("HEVC");
        case CastMediaInfo::Video::MPEG2:
            return _T("MPEG-2");
        case CastMediaInfo::Video::VP8:
            return _T("VP8");
        case CastMediaInfo::Video::VP9:
            return _T("VP9");
        case CastMediaInfo::Video::AV1:
            return _T("AV1");
        default:
            return _T("unrecognized");
    }
}

inline LPCTSTR CastAudioCodecName(CastMediaInfo::Audio audio)
{
    switch (audio) {
        case CastMediaInfo::Audio::AAC:
            return _T("AAC");
        case CastMediaInfo::Audio::MP3:
            return _T("MP3");
        case CastMediaInfo::Audio::WMA:
            return _T("WMA");
        case CastMediaInfo::Audio::FLAC:
            return _T("FLAC");
        case CastMediaInfo::Audio::Opus:
            return _T("Opus");
        case CastMediaInfo::Audio::Vorbis:
            return _T("Vorbis");
        case CastMediaInfo::Audio::LPCM:
            return _T("LPCM");
        case CastMediaInfo::Audio::AC3:
            return _T("AC-3");
        case CastMediaInfo::Audio::EAC3:
            return _T("E-AC-3");
        case CastMediaInfo::Audio::DTS:
            return _T("DTS");
        case CastMediaInfo::Audio::TrueHD:
            return _T("TrueHD");
        default:
            return _T("unrecognized");
    }
}

// What MediaInfo made of the file, in one line for the log. The file name is
// the caller's to add: a log that gets pasted in public carries the name of
// the file and never the path to it.
inline CString CastDescribeMedia(const CastMediaInfo& info)
{
    // Nothing said about a stream at all reads as no stream, which for a file
    // that has one is itself the thing to notice: a track MediaInfo could not
    // describe is a track the device is not being told about either.
    CString text;
    if (info.video == CastMediaInfo::Video::Unknown && info.width == 0 && info.height == 0) {
        text = _T("no video");
    } else {
        text.Format(_T("video %s"), CastVideoCodecName(info.video));
        if (info.width > 0 && info.height > 0) {
            text.AppendFormat(_T(" %dx%d"), info.width, info.height);
        }
    }
    if (info.audio == CastMediaInfo::Audio::Unknown && info.sampleRate == 0 && info.channels == 0) {
        text += _T(", no audio");
    } else {
        text.AppendFormat(_T(", audio %s"), CastAudioCodecName(info.audio));
        if (info.sampleRate > 0) {
            text.AppendFormat(_T(" %d Hz"), info.sampleRate);
        }
        if (info.channels > 0) {
            text.AppendFormat(_T(" %d ch"), info.channels);
        }
    }
    return text;
}

// Whether the MediaInfo library is there to be asked. It is external and
// loaded at run time, and casting has no second way of finding out what a
// file holds, so a cast is refused outright rather than started with the
// content misnamed.
bool CastMediaInfoAvailable();

// Reads the file with MediaInfo. Comes back with everything Unknown when the
// library is missing or the file cannot be parsed.
CastMediaInfo GetCastMediaInfo(const CString& path);

enum class CastProtocol { Chromecast, Dlna };

struct CastTargetDevice {
    CastProtocol protocol = CastProtocol::Chromecast;
    CString name;    // friendly name as the device advertises it
    CString model;   // Chromecast TXT record "md", empty on DLNA
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

// Writes to the casting log what the device says it can play (DLNA, which
// answers for itself) or what our own table says it can (Chromecast, which is
// never asked), as a verdict per file type casting knows about. Costs nothing
// while that log is switched off.
void CastLogDeviceCapabilities(const CastTargetDevice& device);

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

    // Set from the options: with this on, CanCastFile() judges nothing about
    // the format and every file is offered to every device. What answers then
    // is the device itself, with a LOAD_FAILED or a refused SetAVTransportURI.
    // Shared by every target, so it is set once beside the media server port.
    static inline bool ignoreFormatSupport = false;

    // Device discovery; runs in the background once started. The device list
    // is polled with GetDevices() whenever it is about to be shown.
    virtual bool StartDiscovery() = 0;
    virtual void StopDiscovery() = 0;
    virtual std::vector<CastTargetDevice> GetDevices() = 0;

    // One discovery serves everything that wants one -- the device dialog and
    // a /castto search can be after one at the same time -- and stopping it
    // throws the device list away. So it is held rather than switched: it runs
    // while anything holds it and is stopped by the last release, and no
    // holder can end another's. Acquire() answers whether anything is running
    // to hold; only a caller it answered yes to releases.
    bool AcquireDiscovery();
    void ReleaseDiscovery();

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
    // named because capabilities differ per device: a DLNA renderer reports
    // what it accepts, and a Chromecast plays a codec or not by generation.
    // info is what MediaInfo knows about the file; a default one says nothing
    // about it, and then only the container is judged.
    virtual bool CanCastFile(const CString& deviceId, const CString& path, const CastMediaInfo& info) = 0;

    // The same question for a device from the saved list, answered from what
    // was recorded about it rather than from a discovery that is not running.
    virtual bool CanCastFileSaved(const CastSavedDevice& saved, const CString& path,
                                  const CastMediaInfo& info) = 0;

    // Serves filePath to the device and loads it, seeking to startSec once
    // the device reports playback. info is what MediaInfo knows about
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

private:
    int m_discoveryHolders = 0;
};
