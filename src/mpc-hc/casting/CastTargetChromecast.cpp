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
#include "CastTargetChromecast.h"

#define CAST_MSGWND_CLASS   _T("MPCHCCastTarget")
#define WM_CAST_SESSION     (WM_APP + 0)

// how long StopCasting() lets the polite media STOP reach the device before
// the connection is torn down regardless
#define STOP_MEDIA_TIMEOUT_MS 1000

CChromecastTarget::~CChromecastTarget()
{
    m_session.Stop();
    m_server.Stop();
    m_discovery.Stop();
    if (m_hMsgWnd) {
        DestroyWindow(m_hMsgWnd);
        m_hMsgWnd = nullptr;
    }
}

LRESULT CALLBACK CChromecastTarget::MsgWndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    auto* pThis = reinterpret_cast<CChromecastTarget*>(GetWindowLongPtr(hWnd, GWLP_USERDATA));
    if (pThis && uMsg == WM_CAST_SESSION) {
        pThis->OnSessionStateChanged();
        return 0;
    }
    return DefWindowProc(hWnd, uMsg, wParam, lParam);
}

bool CChromecastTarget::EnsureMessageWindow()
{
    if (m_hMsgWnd) {
        return true;
    }

    const HINSTANCE hInstance = AfxGetInstanceHandle();
    WNDCLASS wc = {};
    if (!GetClassInfo(hInstance, CAST_MSGWND_CLASS, &wc)) {
        wc.lpfnWndProc = MsgWndProc;
        wc.hInstance = hInstance;
        wc.lpszClassName = CAST_MSGWND_CLASS;
        if (!RegisterClass(&wc)) {
            return false;
        }
    }

    m_hMsgWnd = CreateWindowEx(0, CAST_MSGWND_CLASS, nullptr, 0, 0, 0, 0, 0,
                               HWND_MESSAGE, nullptr, hInstance, nullptr);
    if (m_hMsgWnd) {
        SetWindowLongPtr(m_hMsgWnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(this));
    }
    return m_hMsgWnd != nullptr;
}

CString CChromecastTarget::DeviceKey(const CastDevice& dev)
{
    if (!dev.id.IsEmpty()) {
        return dev.id;
    }
    CString key;
    key.Format(_T("%s:%u"), dev.ipAddress.GetString(), dev.port);
    return key;
}

CString CChromecastTarget::DeviceDisplayName(const CastDevice& dev)
{
    if (!dev.friendlyName.IsEmpty()) {
        return dev.friendlyName;
    }
    return !dev.model.IsEmpty() ? dev.model : dev.ipAddress;
}

bool CChromecastTarget::StartDiscovery()
{
    return m_discovery.Start();
}

void CChromecastTarget::StopDiscovery()
{
    m_discovery.Stop();
}

CastTargetDevice CChromecastTarget::ToTargetDevice(const CastDevice& dev)
{
    CastTargetDevice d;
    d.protocol = CastProtocol::Chromecast;
    d.name = DeviceDisplayName(dev);
    d.model = dev.model;
    d.id = DeviceKey(dev);
    d.address = dev.ipAddress;
    d.port = dev.port;
    d.supportsVideo = dev.SupportsVideo();
    d.supportsAudio = dev.SupportsAudio();
    return d;
}

std::vector<CastTargetDevice> CChromecastTarget::GetDevices()
{
    std::vector<CastTargetDevice> devices;
    for (const CastDevice& dev : m_discovery.GetDevices()) {
        devices.emplace_back(ToTargetDevice(dev));
    }
    return devices;
}

bool CChromecastTarget::ProbeAddress(CastProtocol protocol, const CString& address, UINT /*port*/,
                                     DWORD timeoutMs, CastTargetDevice& device)
{
    CastDevice dev;
    if (protocol != CastProtocol::Chromecast || address.IsEmpty()
            || !CCastDiscovery::ProbeAddress(address, timeoutMs, dev)) {
        return false;
    }
    device = ToTargetDevice(dev);
    return true;
}

bool CChromecastTarget::SearchById(const CString& id, DWORD timeoutMs, CastDevice& device)
{
    const bool wasRunning = m_discovery.IsRunning();
    if (!wasRunning && !m_discovery.Start()) {
        return false;
    }

    bool found = false;
    const ULONGLONG deadline = GetTickCount64() + timeoutMs;
    for (;;) {
        for (const CastDevice& dev : m_discovery.GetDevices()) {
            if (!dev.id.IsEmpty() && dev.id == id) {
                device = dev;
                found = true;
                break;
            }
        }
        if (found || GetTickCount64() >= deadline) {
            break;
        }
        Sleep(150);
    }

    if (!wasRunning) {
        m_discovery.Stop(); // the copy above survives it
    }
    return found;
}

void CChromecastTarget::SetNotifyWindow(HWND hNotifyWnd, UINT stateMsg)
{
    m_hNotifyWnd = hNotifyWnd;
    m_stateMsg = stateMsg;
}

void CChromecastTarget::NotifyState(CastTargetState state)
{
    m_lastNotifiedState = state;
    if (m_hNotifyWnd && m_stateMsg) {
        PostMessage(m_hNotifyWnd, m_stateMsg, static_cast<WPARAM>(state), (LPARAM)m_generation);
    }
}

bool CChromecastTarget::Connect(const CString& deviceId)
{
    if (m_casting || !EnsureMessageWindow()) {
        return false;
    }

    for (const CastDevice& dev : m_discovery.GetDevices()) {
        if (DeviceKey(dev) == deviceId) {
            return StartSession(dev, deviceId, DeviceDisplayName(dev));
        }
    }
    return false; // the device is gone from the discovery snapshot
}

bool CChromecastTarget::StartSession(const CastDevice& dev, const CString& deviceId, const CString& deviceName)
{
    m_session.SetNotifyWindow(m_hMsgWnd, WM_CAST_SESSION);
    if (!m_session.Start(dev)) {
        return false;
    }
    if (!m_server.IsRunning() && !m_server.Start()) {
        m_session.Stop();
        return false;
    }
    m_server.SetAllowedPeer(CStringA(dev.ipAddress));
    m_deviceId = deviceId;
    m_deviceName = deviceName;
    m_casting = true;
    m_failed = false;
    // notifications of the previous session no longer apply
    m_generation = CastNextSessionGeneration();
    m_lastNotifiedState = CastTargetState::Connecting;
    return true;
}

bool CChromecastTarget::ConnectSaved(CastSavedDevice& saved, DWORD directMs, DWORD searchMs)
{
    if (m_casting || !EnsureMessageWindow()) {
        return false;
    }

    // The address the device was last seen at is asked first; only when it
    // does not answer, or answers as somebody else, is a full search worth
    // the wait. Either way what the device says about itself is written back,
    // so the saved entry follows the device instead of rotting.
    CastDevice dev;
    bool found = !saved.address.IsEmpty() && CCastDiscovery::ProbeAddress(saved.address, directMs, dev)
                 && (saved.id.IsEmpty() || dev.id.IsEmpty() || dev.id == saved.id);
    if (!found && !saved.id.IsEmpty()) {
        found = SearchById(saved.id, searchMs, dev);
    }
    if (!found) {
        return false;
    }

    saved.address = dev.ipAddress;
    saved.port = dev.port;
    if (!dev.friendlyName.IsEmpty()) {
        saved.name = dev.friendlyName;
    }
    if (!dev.model.IsEmpty()) {
        // an entry saved before the model was recorded learns it here
        saved.model = dev.model;
    }
    if (dev.capabilities) {
        saved.supportsVideo = dev.SupportsVideo();
        saved.supportsAudio = dev.SupportsAudio();
    }
    return StartSession(dev, saved.id, saved.DisplayName());
}

// What the default media receiver plays, after
// https://developers.google.com/cast/docs/media. Containers are the same
// everywhere and are settled by CCastMediaServer::IsCastableFile(); the
// audio codecs are too. Video is the one thing that differs by device
// generation, and only over HEVC and AV1 -- H.264, VP8 and VP9 are on every
// device that has a screen at all.
//
// The table is deliberately small and deliberately optimistic: a model it
// does not recognize is allowed everything. A receiver that refuses the file
// answers LOAD_FAILED, which the session reports, so guessing wrong that way
// costs one visible failed attempt; guessing wrong the other way silently
// refuses content a device released after this table was written plays
// perfectly well, and nobody would ever find out why. Resolution and frame
// rate limits are not modelled for the same reason.
bool CChromecastTarget::ReceiverCanPlay(const CString& path, const CastMediaInfo& info, const CString& model)
{
    if (!CCastMediaServer::IsCastableFile(path)) {
        return false;
    }

    switch (info.audio) {
        case CastMediaInfo::Audio::AC3:
        case CastMediaInfo::Audio::EAC3:
        case CastMediaInfo::Audio::DTS:
        case CastMediaInfo::Audio::TrueHD:
        case CastMediaInfo::Audio::WMA:
            // AC-3 and E-AC-3 reach a receiver only as passthrough, which a
            // receiver application has to ask for and ours is not one; DTS and
            // TrueHD not even that way.
            return false;
        case CastMediaInfo::Audio::AAC:
            // the receiver decodes stereo AAC; more channels than that is
            // where it stops
            if (info.channels > 2) {
                return false;
            }
            break;
        default:
            // FLAC, MP3, Opus, Vorbis, LPCM and anything unrecognized
            break;
    }

    if (info.video == CastMediaInfo::Video::MPEG2) {
        return false; // MP2T is a container the receiver takes, MPEG-2 video is not
    }
    if (info.video != CastMediaInfo::Video::HEVC && info.video != CastMediaInfo::Video::AV1) {
        return true;
    }

    // Only the models that genuinely decide it, most specific name first: the
    // plain "Chromecast" of the first three generations is a prefix of every
    // later dongle's name.
    static const struct {
        LPCTSTR model;
        bool hevc;
        bool av1;
    } models[] = {
        { _T("Google TV Streamer"), true,  true  },
        { _T("Chromecast Ultra"),   true,  false },
        { _T("Google TV"),          true,  false }, // Chromecast with Google TV, 4K and HD
        { _T("Chromecast HD"),      true,  false },
        { _T("Nest Hub"),           false, false }, // and the Google Home Hub it was named after
        { _T("Home Hub"),           false, false },
        { _T("Chromecast"),         false, false }, // 1st to 3rd generation
    };

    for (const auto& entry : models) {
        if (model.Find(entry.model) >= 0) {
            return info.video == CastMediaInfo::Video::HEVC ? entry.hevc : entry.av1;
        }
    }
    return true; // an unknown model is given the benefit of the doubt
}

bool CChromecastTarget::CanCastFileSaved(const CastSavedDevice& saved, const CString& path,
                                         const CastMediaInfo& info)
{
    if (!ReceiverCanPlay(path, info, saved.model)) {
        return false;
    }
    // A device without video, a speaker or a display-less Nest, only takes audio.
    const CStringA mime = CCastMediaServer::MimeForFile(path);
    return mime.Left(6).CompareNoCase("video/") != 0 || saved.supportsVideo;
}

bool CChromecastTarget::CanCastFile(const CString& deviceId, const CString& path, const CastMediaInfo& info)
{
    for (const CastDevice& dev : m_discovery.GetDevices()) {
        if (DeviceKey(dev) == deviceId) {
            if (!ReceiverCanPlay(path, info, dev.model)) {
                return false;
            }
            // A device without video, a speaker or a display-less Nest, only
            // takes audio.
            const CStringA mime = CCastMediaServer::MimeForFile(path);
            return mime.Left(6).CompareNoCase("video/") != 0 || dev.SupportsVideo();
        }
    }
    return false; // the device is gone from the discovery snapshot
}

void CChromecastTarget::LoadMedia(const CString& filePath, const CString& title, double durationSec, double startSec,
                                  const CastMediaInfo& /*info*/)
{
    if (!m_casting) {
        return;
    }

    // A Chromecast is told what it is playing over the cast protocol and never
    // asks for DLNA content features, so the server is given none.
    m_mime = CCastMediaServer::MimeForFile(filePath);
    m_server.SetFile(filePath, m_mime);
    m_pendingTitle = title;
    m_pendingDuration = durationSec;
    m_pendingSeek = startSec >= 1.0 ? startSec : -1.0;

    switch (m_session.GetState()) {
        case CastSessionState::Ready:
        case CastSessionState::Loading:
        case CastSessionState::Buffering:
        case CastSessionState::Playing:
        case CastSessionState::Paused:
        case CastSessionState::Stopped:
            SendLoad();
            break;
        default:
            // the session is still connecting; the LOAD is sent once it
            // reaches Ready
            m_loadPending = true;
            break;
    }
}

void CChromecastTarget::SendLoad()
{
    m_loadPending = false;
    const CStringA url = m_server.GetURLForHost(CStringA(m_session.GetLocalAddress()));
    if (url.IsEmpty()) {
        // no file registered or no local address: the session would sit there
        // casting nothing, so report the failure and let the UI tear it down
        TRACE(_T("ChromecastTarget: no media URL to load\n"));
        m_failed = true;
        NotifyState(CastTargetState::Failed);
        return;
    }
    m_session.Load(CString(url), CString(m_mime), m_pendingDuration, m_pendingTitle);
}

void CChromecastTarget::Play()
{
    m_session.Play();
}

void CChromecastTarget::Pause()
{
    m_session.Pause();
}

void CChromecastTarget::Seek(double seconds)
{
    m_session.Seek(seconds);
}

void CChromecastTarget::SetVolume(double level, bool muted)
{
    m_session.SetVolume(level, muted);
}

void CChromecastTarget::StopCasting()
{
    // Stop() only closes the virtual connections, which leaves our media on
    // the receiver, so the media STOP is sent first and given a short window
    // to go out - the queued command would never run if we joined right away.
    m_session.StopMediaAndWait(STOP_MEDIA_TIMEOUT_MS);
    m_session.Stop();
    m_server.ClearFile();
    m_server.ClearAllowedPeer();
    m_server.Stop();
    m_casting = false;
    m_failed = false;
    m_loadPending = false;
    m_pendingSeek = -1.0;
    m_deviceId.Empty();
    m_deviceName.Empty();
    m_lastNotifiedState = CastTargetState::Idle;
}

CastTargetState CChromecastTarget::SimplifyState(CastSessionState state)
{
    switch (state) {
        case CastSessionState::Disconnected:
            return CastTargetState::Idle;
        case CastSessionState::Authenticating:
        case CastSessionState::Connecting:
        case CastSessionState::Connected:
        case CastSessionState::Launching:
        case CastSessionState::Ready:
            return CastTargetState::Connecting;
        case CastSessionState::Loading:
            return CastTargetState::Loading;
        case CastSessionState::Buffering:
            return CastTargetState::Buffering;
        case CastSessionState::Playing:
            return CastTargetState::Playing;
        case CastSessionState::Paused:
            return CastTargetState::Paused;
        case CastSessionState::Stopping:
        case CastSessionState::Stopped:
            return CastTargetState::Ended;
        case CastSessionState::TakenOver:
            return CastTargetState::TakenOver;
        case CastSessionState::LoadFailed:
        case CastSessionState::Dead:
        default:
            return CastTargetState::Failed;
    }
}

CastTargetState CChromecastTarget::GetState() const
{
    if (!m_casting) {
        return CastTargetState::Idle;
    }
    if (m_failed) {
        return CastTargetState::Failed;
    }
    return SimplifyState(m_session.GetState());
}

void CChromecastTarget::OnSessionStateChanged()
{
    if (!m_casting) {
        return;
    }

    const CastSessionState state = m_session.GetState();

    if (state == CastSessionState::Ready && m_loadPending) {
        SendLoad();
    } else if (m_pendingSeek >= 0.0
               && (state == CastSessionState::Playing || state == CastSessionState::Paused
                   || state == CastSessionState::Buffering)) {
        // the first non-IDLE media status has arrived, so the session knows
        // the mediaSessionId and the initial seek can go out
        m_session.Seek(m_pendingSeek);
        m_pendingSeek = -1.0;
    }

    const CastTargetState simplified = SimplifyState(state);
    if (simplified != m_lastNotifiedState) {
        NotifyState(simplified);
    }
}
