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

std::vector<CastTargetDevice> CChromecastTarget::GetDevices()
{
    std::vector<CastTargetDevice> devices;
    for (const CastDevice& dev : m_discovery.GetDevices()) {
        CastTargetDevice d;
        d.name = DeviceDisplayName(dev);
        d.id = DeviceKey(dev);
        d.address = dev.ipAddress;
        d.supportsVideo = dev.SupportsVideo();
        d.supportsAudio = dev.SupportsAudio();
        devices.emplace_back(std::move(d));
    }
    return devices;
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
            m_deviceName = DeviceDisplayName(dev);
            m_casting = true;
            m_failed = false;
            // notifications of the previous session no longer apply
            m_generation = CastNextSessionGeneration();
            m_lastNotifiedState = CastTargetState::Connecting;
            return true;
        }
    }
    return false; // the device is gone from the discovery snapshot
}

bool CChromecastTarget::CanCastFile(const CString& deviceId, const CString& path)
{
    if (!CCastMediaServer::IsCastableFile(path)) {
        return false;
    }
    // A device without video, a speaker or a display-less Nest, only takes
    // audio; every receiver plays the same formats otherwise.
    const CStringA mime = CCastMediaServer::MimeForFile(path);
    if (mime.Left(6).CompareNoCase("video/") == 0) {
        for (const CastDevice& dev : m_discovery.GetDevices()) {
            if (DeviceKey(dev) == deviceId) {
                return dev.SupportsVideo();
            }
        }
    }
    return true;
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
