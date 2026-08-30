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
#include "CastTargetMulti.h"

// The prefix a device id carries, and the label a device gets when its name
// alone would not tell it apart from one reached over the other protocol.
#define CHROMECAST_PREFIX _T("cc:")
#define CHROMECAST_LABEL  _T(" (Chromecast)")
#define DLNA_PREFIX       _T("dlna:")
#define DLNA_LABEL        _T(" (DLNA)")

bool CCastTargetMulti::StartDiscovery()
{
    // both are started; one protocol failing must not hide the other
    const bool chromecast = m_chromecast.StartDiscovery();
    const bool dlna = m_dlna.StartDiscovery();
    return chromecast || dlna;
}

void CCastTargetMulti::StopDiscovery()
{
    m_chromecast.StopDiscovery();
    m_dlna.StopDiscovery();
}

bool CCastTargetMulti::IsDiscoveryRunning() const
{
    // "not fully running" so that the lazy start retries the one that failed
    return m_chromecast.IsDiscoveryRunning() && m_dlna.IsDiscoveryRunning();
}

void CCastTargetMulti::AppendDevices(CCastTarget& target, LPCTSTR prefix, std::vector<CastTargetDevice>& devices)
{
    for (CastTargetDevice& dev : target.GetDevices()) {
        dev.id = prefix + dev.id;
        devices.emplace_back(std::move(dev));
    }
}

std::vector<CastTargetDevice> CCastTargetMulti::GetDevices()
{
    std::vector<CastTargetDevice> devices;
    AppendDevices(m_chromecast, CHROMECAST_PREFIX, devices);
    AppendDevices(m_dlna, DLNA_PREFIX, devices);

    // A device that speaks both protocols is announced under the same name
    // twice, and the menu would then offer two identical entries. Only those
    // are told apart by their protocol; the rest keep a clean label.
    std::vector<bool> ambiguous(devices.size(), false);
    for (size_t i = 0; i < devices.size(); i++) {
        for (size_t j = i + 1; j < devices.size(); j++) {
            if (devices[i].name == devices[j].name) {
                ambiguous[i] = ambiguous[j] = true;
            }
        }
    }
    const int prefixLen = (int)_tcslen(CHROMECAST_PREFIX);
    for (size_t i = 0; i < devices.size(); i++) {
        if (ambiguous[i]) {
            devices[i].name += devices[i].id.Left(prefixLen) == CHROMECAST_PREFIX ? CHROMECAST_LABEL : DLNA_LABEL;
        }
    }
    return devices;
}

void CCastTargetMulti::SetNotifyWindow(HWND hNotifyWnd, UINT stateMsg)
{
    m_chromecast.SetNotifyWindow(hNotifyWnd, stateMsg);
    m_dlna.SetNotifyWindow(hNotifyWnd, stateMsg);
}

CCastTarget* CCastTargetMulti::TargetFor(const CString& deviceId, CString& targetDeviceId)
{
    const int chromecastPrefix = (int)_tcslen(CHROMECAST_PREFIX);
    if (deviceId.Left(chromecastPrefix) == CHROMECAST_PREFIX) {
        targetDeviceId = deviceId.Mid(chromecastPrefix);
        return &m_chromecast;
    }
    const int dlnaPrefix = (int)_tcslen(DLNA_PREFIX);
    if (deviceId.Left(dlnaPrefix) == DLNA_PREFIX) {
        targetDeviceId = deviceId.Mid(dlnaPrefix);
        return &m_dlna;
    }
    targetDeviceId.Empty();
    return nullptr;
}

bool CCastTargetMulti::Connect(const CString& deviceId)
{
    if (IsCasting()) {
        return false;
    }

    CString targetDeviceId;
    CCastTarget* pTarget = TargetFor(deviceId, targetDeviceId);
    if (!pTarget || !pTarget->Connect(targetDeviceId)) {
        return false;
    }
    m_pActive = pTarget;
    m_activeId = deviceId;
    return true;
}

bool CCastTargetMulti::CanCastFile(const CString& deviceId, const CString& path)
{
    CString targetDeviceId;
    CCastTarget* pTarget = TargetFor(deviceId, targetDeviceId);
    return pTarget && pTarget->CanCastFile(targetDeviceId, path);
}

void CCastTargetMulti::LoadMedia(const CString& filePath, const CString& title, double durationSec, double startSec,
                                 const CastMediaInfo& info)
{
    if (m_pActive) {
        m_pActive->LoadMedia(filePath, title, durationSec, startSec, info);
    }
}

void CCastTargetMulti::Play()
{
    if (m_pActive) {
        m_pActive->Play();
    }
}

void CCastTargetMulti::Pause()
{
    if (m_pActive) {
        m_pActive->Pause();
    }
}

void CCastTargetMulti::Seek(double seconds)
{
    if (m_pActive) {
        m_pActive->Seek(seconds);
    }
}

void CCastTargetMulti::SetVolume(double level, bool muted)
{
    if (m_pActive) {
        m_pActive->SetVolume(level, muted);
    }
}

void CCastTargetMulti::StopCasting()
{
    if (m_pActive) {
        m_pActive->StopCasting();
        m_pActive = nullptr;
    }
    m_activeId.Empty();
}

CastTargetState CCastTargetMulti::GetState() const
{
    return m_pActive ? m_pActive->GetState() : CastTargetState::Idle;
}

CString CCastTargetMulti::GetFailureReason() const
{
    return m_pActive ? m_pActive->GetFailureReason() : CString();
}
