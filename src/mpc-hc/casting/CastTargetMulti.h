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
#include "CastTargetChromecast.h"
#include "CastTargetDlna.h"

// Presents the Chromecast and the DLNA target to the player as one cast
// target, so that the UI keeps talking to a single CCastTarget and knows
// nothing about either protocol. Both discoveries run together and their
// devices land in the same menu; the protocol a device belongs to travels in
// its id, and every session call is routed to the target that owns it.
class CCastTargetMulti : public CCastTarget
{
public:
    CCastTargetMulti() = default;

    CCastTargetMulti(const CCastTargetMulti&) = delete;
    CCastTargetMulti& operator=(const CCastTargetMulti&) = delete;

    // CCastTarget
    bool StartDiscovery() override;
    void StopDiscovery() override;
    std::vector<CastTargetDevice> GetDevices() override;

    void SetNotifyWindow(HWND hNotifyWnd, UINT stateMsg) override;

    bool ProbeAddress(CastProtocol protocol, const CString& address, UINT port,
                      DWORD timeoutMs, CastTargetDevice& device) override;

    bool Connect(const CString& deviceId) override;
    bool ConnectSaved(CastSavedDevice& saved, DWORD directMs, DWORD searchMs) override;
    bool IsCasting() const override { return m_pActive && m_pActive->IsCasting(); }
    CString GetDeviceId() const override { return m_activeId; }
    CString GetDeviceName() const override { return m_pActive ? m_pActive->GetDeviceName() : CString(); }
    UINT GetSessionGeneration() const override { return m_pActive ? m_pActive->GetSessionGeneration() : 0; }

    bool CanCastFile(const CString& deviceId, const CString& path, const CastMediaInfo& info) override;
    bool CanCastFileSaved(const CastSavedDevice& saved, const CString& path, const CastMediaInfo& info) override;

    void LoadMedia(const CString& filePath, const CString& title, double durationSec, double startSec,
                   const CastMediaInfo& info) override;

    void Play() override;
    void Pause() override;
    void Seek(double seconds) override;
    bool CanSeek() const override { return !m_pActive || m_pActive->CanSeek(); }
    void SetVolume(double level, bool muted) override;
    void StopCasting() override;

    CastTargetState GetState() const override;
    CString GetFailureReason() const override;
    double GetPosition() const override { return m_pActive ? m_pActive->GetPosition() : 0.0; }
    double GetDuration() const override { return m_pActive ? m_pActive->GetDuration() : 0.0; }

private:
    // Resolves the target a prefixed device id belongs to and hands back the
    // id as that target knows it. Returns null for an id from neither.
    CCastTarget* TargetFor(const CString& deviceId, CString& targetDeviceId);
    // Routes on the protocol a saved entry names, and hands back its id with
    // the prefix taken off when it carries one.
    CCastTarget* TargetFor(CastProtocol protocol, const CString& deviceId, CString& targetDeviceId);
    static LPCTSTR PrefixFor(CastProtocol protocol);
    void AppendDevices(CCastTarget& target, LPCTSTR prefix, std::vector<CastTargetDevice>& devices);

    CChromecastTarget m_chromecast;
    CDlnaTarget m_dlna;

    CCastTarget* m_pActive = nullptr; // the target of the running session
    CString m_activeId;               // its device id, prefix included
};
