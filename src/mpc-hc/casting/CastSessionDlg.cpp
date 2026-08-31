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
#include "CastSessionDlg.h"
#include "mplayerc.h"
#include "MainFrm.h"
#include "SettingsDefines.h"
#include "AppSettings.h"
#include <DSUtil.h>
#include <algorithm>

#define CAST_SESSION_TIMER 1
#define CAST_SESSION_MS    500

// A seek is answered by the device a status or two later. Until then the
// seekbar keeps showing what was asked for, so that the thumb does not spring
// back to where the device still thinks it is.
#define CAST_SEEK_SETTLE_MS 1500

// How far the device has to move before the history entry is written again.
// The same 30 seconds the local position poller uses.
#define CAST_HISTORY_STEP  30.0

CCastSessionDlg::CCastSessionDlg(CCastTarget* pTarget, CWnd* pParent)
    : CModelessDialog(IDD, pParent)
    , m_pTarget(pTarget)
{
}

CCastSessionDlg::~CCastSessionDlg()
{
    HoldSystemAwake(false);
}

void CCastSessionDlg::DoDataExchange(CDataExchange* pDX)
{
    __super::DoDataExchange(pDX);
    DDX_Control(pDX, IDC_CASTSESS_SEEK, m_seek);
    DDX_Control(pDX, IDC_CASTSESS_VOLUME, m_volume);
    fulfillThemeReqs();
}

BEGIN_MESSAGE_MAP(CCastSessionDlg, CModelessDialog)
    ON_WM_DESTROY()
    ON_WM_TIMER()
    ON_WM_HSCROLL()
    ON_BN_CLICKED(IDC_CASTSESS_PLAYPAUSE, OnPlayPause)
    ON_BN_CLICKED(IDC_CASTSESS_STOP, OnStop)
    ON_MESSAGE(WM_CAST_STATE_CHANGED, OnCastStateChanged)
END_MESSAGE_MAP()

BOOL CCastSessionDlg::OnInitDialog()
{
    EnableSaveRestoreKey(IDS_R_DLG_CAST_SESSION);

    __super::OnInitDialog();

    SetupAnchors();

    m_seek.SetRange(0, 0, TRUE);
    m_seek.SetPageSize(30);
    m_seek.SetJumpToClick(); // a click seeks to where it landed, as the player's own seekbar does
    m_seek.EnableWindow(FALSE);

    // The device has no volume to report back, so the slider starts where the
    // player's own is and only ever tells the device what the user does here.
    m_volume.SetRange(0, 100, TRUE);
    m_volume.SetPageSize(5);
    m_volume.SetPos(AfxGetAppSettings().nVolume);

    // Every notification from the session belongs to this window from now on.
    if (m_pTarget) {
        m_pTarget->SetNotifyWindow(m_hWnd, WM_CAST_STATE_CHANGED);
    }

    UpdateTransport();
    UpdatePosition();

    return TRUE;
}

void CCastSessionDlg::SetupAnchors()
{
    AddAnchor(IDC_CASTSESS_DEVICE, TOP_LEFT, TOP_RIGHT);
    AddAnchor(IDC_CASTSESS_TITLE, TOP_LEFT, TOP_RIGHT);
    AddAnchor(IDC_CASTSESS_SEEK, TOP_LEFT, TOP_RIGHT);
    AddAnchor(IDC_CASTSESS_TIME, TOP_LEFT);
    AddAnchor(IDC_CASTSESS_STATUS, TOP_RIGHT);
    AddAnchor(IDC_CASTSESS_PLAYPAUSE, TOP_LEFT);
    AddAnchor(IDC_CASTSESS_STOP, TOP_LEFT);
    AddAnchor(IDC_CASTSESS_VOLLABEL, TOP_RIGHT);
    AddAnchor(IDC_CASTSESS_VOLUME, TOP_RIGHT);
    AddAnchor(IDCANCEL, TOP_RIGHT);
}

TrackSizeConstraints CCastSessionDlg::GetTrackSizeConstraints() const
{
    // Only the width is worth resizing, for a long title; nothing here grows
    // usefully in height.
    TrackSizeConstraints constraints;
    constraints.max.enabled = true;
    constraints.max.xMultiplier = 2.0;
    return constraints;
}

void CCastSessionDlg::OnDestroy()
{
    KillTimer(CAST_SESSION_TIMER);
    // Closing this window is what ends a cast, so nothing is left running and
    // no notification is left pointing at a window that is going away.
    EndSession(0);
    if (m_pTarget) {
        m_pTarget->SetNotifyWindow(nullptr, 0);
    }
    HoldSystemAwake(false);

    __super::OnDestroy();
}

void CCastSessionDlg::StartSession(const CastSessionMedia& media)
{
    if (!m_pTarget) {
        return;
    }
    EndSession(0, false); // a session being replaced needs no epilogue

    m_media = media;
    m_nLoops = 0;
    m_bSeekDrag = false;
    m_seekSettleUntil = 0;
    m_lastRemembered = media.startSec;
    m_bSessionLive = true;

    CString strDevice;
    strDevice.Format(IDS_CAST_CASTING_TO, m_pTarget->GetDeviceName().GetString());
    SetDlgItemText(IDC_CASTSESS_DEVICE, strDevice);
    SetDlgItemText(IDC_CASTSESS_TITLE, m_media.title);

    m_pTarget->LoadMedia(m_media.path, m_media.title, m_media.durationSec, m_media.startSec, m_media.info);

    // The file is served out of this process, so the machine has to stay awake
    // for as long as the session lasts.
    HoldSystemAwake(true);

    UpdateTransport();
    UpdatePosition();
    VERIFY(SetTimer(CAST_SESSION_TIMER, CAST_SESSION_MS, nullptr));

    ShowWindow(SW_SHOW);
    SetForegroundWindow();
}

void CCastSessionDlg::EndSession(UINT nStatus /*= IDS_CAST_STOPPED*/, bool bSyncLocalGraph /*= true*/)
{
    if (!m_bSessionLive) {
        if (nStatus) {
            SetStatusText(nStatus);
        }
        return;
    }
    m_bSessionLive = false;
    KillTimer(CAST_SESSION_TIMER);

    const double lastPos = m_pTarget ? m_pTarget->GetPosition() : 0.0;
    if (m_pTarget) {
        m_pTarget->StopCasting();
    }
    HoldSystemAwake(false);

    // What was watched on the device is what this file has to resume from.
    RememberPosition(lastPos);

    // When the player still has that file open, its graph is put there too:
    // the position it is left at is the one it writes to the history when the
    // file is finally closed, and it would otherwise write the beginning.
    if (bSyncLocalGraph && lastPos > 0.0 && !m_media.path.IsEmpty()) {
        CMainFrame* pFrame = (CMainFrame*)AfxGetMainWnd();
        if (pFrame && pFrame->GetLoadState() == MLS::LOADED
                && pFrame->lastOpenFile == m_media.path) {
            pFrame->DoSeekTo(REFERENCE_TIME(lastPos * 10000000.0), false);
        }
    }

    UpdateTransport();
    if (nStatus) {
        SetStatusText(nStatus);
    }
}

bool CCastSessionDlg::IsCasting() const
{
    return m_bSessionLive && m_pTarget && m_pTarget->IsCasting();
}

CString CCastSessionDlg::GetActiveDeviceId() const
{
    return IsCasting() ? m_pTarget->GetDeviceId() : CString();
}

double CCastSessionDlg::GetPosition() const
{
    return m_pTarget ? m_pTarget->GetPosition() : 0.0;
}

void CCastSessionDlg::SetStatusText(UINT nID, const CString& detail /*= CString()*/)
{
    CString text(ResStr(nID));
    if (!detail.IsEmpty()) {
        text.AppendFormat(_T(" (%s)"), detail.GetString());
    }
    SetDlgItemText(IDC_CASTSESS_STATUS, text);
}

void CCastSessionDlg::UpdateTransport()
{
    const CastTargetState state = m_pTarget ? m_pTarget->GetState() : CastTargetState::Idle;
    const bool bPlaying = m_bSessionLive
                          && (state == CastTargetState::Playing || state == CastTargetState::Buffering
                              || state == CastTargetState::Loading || state == CastTargetState::Connecting);

    SetDlgItemText(IDC_CASTSESS_PLAYPAUSE, ResStr(bPlaying ? IDS_AG_PAUSE : IDS_AG_PLAY));
    GetDlgItem(IDC_CASTSESS_PLAYPAUSE)->EnableWindow(m_bSessionLive);
    GetDlgItem(IDC_CASTSESS_STOP)->EnableWindow(m_bSessionLive);
    GetDlgItem(IDC_CASTSESS_VOLUME)->EnableWindow(m_bSessionLive);

    if (!m_bSessionLive) {
        return; // whatever ended the session has already said so
    }

    switch (state) {
        case CastTargetState::Connecting:
            SetStatusText(IDS_CAST_CONNECTING);
            break;
        case CastTargetState::Loading:
        case CastTargetState::Buffering:
            SetStatusText(IDS_CAST_BUFFERING);
            break;
        case CastTargetState::Playing:
            SetStatusText(IDS_CONTROLS_PLAYING);
            break;
        case CastTargetState::Paused:
            SetStatusText(IDS_CONTROLS_PAUSED);
            break;
        default:
            break;
    }
}

void CCastSessionDlg::UpdatePosition(bool bPlayedOut /*= false*/)
{
    double duration = m_pTarget ? m_pTarget->GetDuration() : 0.0;
    if (duration <= 0.0) {
        duration = m_media.durationSec; // the device has not said yet, the graph had
    }
    double position = m_pTarget ? m_pTarget->GetPosition() : 0.0;
    if (GetTickCount64() < m_seekSettleUntil) {
        position = m_seekRequested;
    }

    const int nDuration = std::max(0, (int)(duration + 0.5));
    // A device that reached the end of the media last said where it was a poll
    // before it got there, and nothing follows that status; without this the
    // window stops a second short of a file that finished.
    const int nPosition = bPlayedOut ? nDuration : std::max(0, std::min(nDuration, (int)position));

    if (m_seek.GetRangeMax() != nDuration) {
        m_seek.SetRange(0, nDuration, TRUE);
    }
    // A device that turned out not to be able to seek gets a dead seekbar
    // rather than one that ignores every drag.
    const bool bSeekable = m_bSessionLive && nDuration > 0 && m_pTarget && m_pTarget->CanSeek();
    if (!!m_seek.IsWindowEnabled() != bSeekable) {
        m_seek.EnableWindow(bSeekable);
    }
    if (!m_bSeekDrag) {
        m_seek.SetPos(nPosition);
    }

    CString text;
    text.Format(_T("%s / %s"),
                ReftimeToString4(REFERENCE_TIME(nPosition) * 10000000LL, false).GetString(),
                ReftimeToString4(REFERENCE_TIME(nDuration) * 10000000LL, false).GetString());
    SetDlgItemText(IDC_CASTSESS_TIME, text);
}

void CCastSessionDlg::RememberPosition(double seconds)
{
    if (!m_media.rememberPosition || m_media.path.IsEmpty()) {
        return;
    }
    REFERENCE_TIME rtPos = REFERENCE_TIME(seconds * 10000000.0);
    const REFERENCE_TIME rtDur = REFERENCE_TIME(m_media.durationSec * 10000000.0);
    if (rtPos < 0 || (rtDur > 0 && (rtPos >= rtDur || rtDur - rtPos < 50000000LL))) {
        rtPos = 0; // at the end of the file there is nothing to resume from
    }
    AfxGetAppSettings().MRU.UpdateFilePosition(m_media.path, rtPos);
}

// The file is served to the device out of this process, so the machine must not
// sleep while a session is running. A power request says so without fighting
// the player's own SetThreadExecutionState(): both are held at once, and the
// system sleeps only once neither is.
void CCastSessionDlg::HoldSystemAwake(bool hold)
{
    if (hold == (m_hPowerRequest != nullptr)) {
        return;
    }
    if (hold) {
        REASON_CONTEXT reason;
        ZeroMemory(&reason, sizeof(reason));
        reason.Version = POWER_REQUEST_CONTEXT_VERSION;
        reason.Flags = POWER_REQUEST_CONTEXT_SIMPLE_STRING;
        reason.Reason.SimpleReasonString = const_cast<LPWSTR>(L"Casting to a device");

        HANDLE hRequest = PowerCreateRequest(&reason);
        if (hRequest == INVALID_HANDLE_VALUE) {
            return;
        }
        if (!PowerSetRequest(hRequest, PowerRequestSystemRequired)) {
            CloseHandle(hRequest);
            return;
        }
        m_hPowerRequest = hRequest;
    } else {
        PowerClearRequest(m_hPowerRequest, PowerRequestSystemRequired);
        CloseHandle(m_hPowerRequest);
        m_hPowerRequest = nullptr;
    }
}

void CCastSessionDlg::OnTimer(UINT_PTR nIDEvent)
{
    if (nIDEvent == CAST_SESSION_TIMER) {
        UpdatePosition();

        // The history follows the device rather than being written only at the
        // end, so that a session that never got to end still left a position.
        const double position = GetPosition();
        if (m_bSessionLive && std::abs(position - m_lastRemembered) > CAST_HISTORY_STEP) {
            m_lastRemembered = position;
            RememberPosition(position);
        }
        return;
    }
    __super::OnTimer(nIDEvent);
}

void CCastSessionDlg::OnHScroll(UINT nSBCode, UINT nPos, CScrollBar* pScrollBar)
{
    if (pScrollBar == (CScrollBar*)&m_seek) {
        if (nSBCode == TB_THUMBTRACK) {
            m_bSeekDrag = true;
            return; // the thumb is the user's until it is let go
        }
        if (nSBCode == SB_ENDSCROLL) {
            return;
        }
        m_bSeekDrag = false;
        if (m_bSessionLive && m_pTarget && m_pTarget->CanSeek()) {
            m_seekRequested = m_seek.GetPos();
            m_seekSettleUntil = GetTickCount64() + CAST_SEEK_SETTLE_MS;
            m_pTarget->Seek(m_seekRequested);
            UpdatePosition();
        }
        return;
    }

    if (pScrollBar == (CScrollBar*)&m_volume) {
        // Every drag position would be a message to the device; only what the
        // user settles on is sent.
        if (nSBCode != TB_THUMBTRACK && nSBCode != SB_ENDSCROLL && m_bSessionLive && m_pTarget) {
            m_pTarget->SetVolume(m_volume.GetPos() / 100.0, m_volume.GetPos() == 0);
        }
        return;
    }

    __super::OnHScroll(nSBCode, nPos, pScrollBar);
}

void CCastSessionDlg::OnPlayPause()
{
    if (!m_bSessionLive || !m_pTarget) {
        return;
    }
    // Connecting and Loading count as playing: the device is on its way to it
    // and the session keeps the intent until it has something to apply it to.
    if (m_pTarget->GetState() == CastTargetState::Paused) {
        m_pTarget->Play();
    } else {
        m_pTarget->Pause();
    }
    UpdateTransport();
}

void CCastSessionDlg::OnStop()
{
    EndSession();
    DestroyWindow(); // stopping is done with the session, and so is this window
}

LRESULT CCastSessionDlg::OnCastStateChanged(WPARAM /*wParam*/, LPARAM lParam)
{
    // The state in wParam is a snapshot from when the notification was queued;
    // one left over from a session that has since been replaced must not tear
    // down the session that replaced it, so the generation is checked and the
    // state is taken live.
    if (!IsCasting() || (UINT)lParam != m_pTarget->GetSessionGeneration()) {
        return 0;
    }

    switch (m_pTarget->GetState()) {
        case CastTargetState::Ended: {
            const CAppSettings& s = AfxGetAppSettings();
            ++m_nLoops;
            if (s.fLoopForever || m_nLoops < s.nLoops) {
                // Repeat on the device: the receiver has unloaded the media, so
                // it is handed the file again from the start.
                m_pTarget->LoadMedia(m_media.path, m_media.title, m_media.durationSec, 0.0, m_media.info);
                break;
            }
            // The file played out, so there is no position left to resume from;
            // the player is not involved and its after-playback actions, which
            // are about its own playlist, are not either. The window is left
            // showing the end of the file, which is where the device got to.
            UpdatePosition(true);
            EndSession(IDS_CAST_STOPPED, false);
            RememberPosition(0.0);
            break;
        }
        case CastTargetState::TakenOver:
            // Another sender loaded its own media into the device.
            EndSession(IDS_CAST_TAKEN_OVER);
            break;
        case CastTargetState::Failed: {
            const CString reason = m_pTarget->GetFailureReason();
            EndSession(0);
            SetStatusText(IDS_CAST_FAILED, reason);
            break;
        }
        default:
            UpdateTransport();
            UpdatePosition();
            break;
    }
    return 0;
}
