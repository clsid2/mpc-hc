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
#include "TrimController.h"
#include "TrimExportDlg.h"
#include "MainFrm.h"
#include "mplayerc.h"
#include "PathUtils.h"

BEGIN_MESSAGE_MAP(CTrimController, CCmdTarget)
    ON_COMMAND(ID_FILE_TRIM_VIDEO, OnFileTrimVideo)
    ON_UPDATE_COMMAND_UI(ID_FILE_TRIM_VIDEO, OnUpdateFileTrimVideo)
    ON_COMMAND(ID_TRIM_SAVE_CLIP, OnSaveClip)
    ON_UPDATE_COMMAND_UI(ID_TRIM_SAVE_CLIP, OnUpdateSaveClip)
    ON_COMMAND(ID_TRIM_PLAY_SELECTION, OnPlaySelection)
    ON_UPDATE_COMMAND_UI(ID_TRIM_PLAY_SELECTION, OnUpdatePlaySelection)
    ON_COMMAND_RANGE(ID_TRIM_SET_START, ID_TRIM_SET_END, OnSetBoundary)
    ON_UPDATE_COMMAND_UI_RANGE(ID_TRIM_SET_START, ID_TRIM_SET_END, OnUpdateSetBoundary)
END_MESSAGE_MAP()

CTrimController::CTrimController(CMainFrame& frame)
    : m_frame(frame)
{
}

CTrimController::~CTrimController()
{
}

bool CTrimController::CanTrim() const
{
    CMainFrame& f = m_frame;
    if (f.GetLoadState() != MLS::LOADED || f.GetPlaybackMode() != PM_FILE || f.m_fAudioOnly
            || !f.m_graph.m_pMS || !f.m_wndSeekBar.HasDuration()) {
        return false;
    }
    DWORD caps = AM_SEEKING_CanSeekAbsolute | AM_SEEKING_CanGetDuration;
    if (f.m_graph.m_pMS->CheckCapabilities(&caps) != S_OK) {
        return false;
    }
    CString source = CurrentSource();
    return !source.IsEmpty() && !PathUtils::IsURL(source) && PathUtils::IsFile(source);
}

CString CTrimController::CurrentSource() const
{
    CPlaylistItem item(true);
    if (!m_frame.m_wndPlaylistBar.GetCur(item, true) || item.m_fns.empty() || item.m_bYoutubeDL) {
        return CString();
    }
    return item.m_fns.front();
}

void CTrimController::Enter()
{
    REFERENCE_TIME duration = 0;
    if (!CanTrim() || FAILED(m_frame.m_graph.m_pMS->GetDuration(&duration)) || duration <= 0) {
        return;
    }
    m_selection.Reset(duration);
    m_entryPosition = m_frame.GetPos();
    m_bEntryRunning = m_frame.GetMediaState() == State_Running;
    m_bActive = true;
    InternalSeek(m_entryPosition);    // a position past the last frame is pulled back into it

    m_bShowedSeekbar = !m_frame.m_controls.ControlChecked(CMainFrameControls::Toolbar::SEEKBAR);
    if (m_bShowedSeekbar) {
        m_frame.m_controls.ToggleControl(CMainFrameControls::Toolbar::SEEKBAR);
    }
    m_frame.m_OSD.DisplayMessage(OSD_TOPLEFT, ResStr(IDS_TRIM_OSD_ENTER), 4000);
    m_frame.AdjustStreamPosPoller(true);
    Refresh();
}

void CTrimController::Exit(bool bRestorePlayback)
{
    if (!m_bActive) {
        return;
    }
    m_bActive = false;
    m_frame.AdjustStreamPosPoller(true);
    m_frame.m_wndSeekBar.CancelTrimDrag();
    if (m_bShowedSeekbar && m_frame.m_controls.ControlChecked(CMainFrameControls::Toolbar::SEEKBAR)) {
        m_frame.m_controls.ToggleControl(CMainFrameControls::Toolbar::SEEKBAR);
    }
    m_bShowedSeekbar = false;
    Refresh();

    if (bRestorePlayback && m_frame.GetLoadState() == MLS::LOADED) {
        InternalSeek(m_entryPosition);
        if (m_bEntryRunning) {
            m_frame.SendMessage(WM_COMMAND, ID_PLAY_PLAY);
        } else {
            m_frame.MediaControlPause();
        }
    }
}

void CTrimController::Refresh()
{
    m_frame.m_wndSeekBar.OnTrimChanged();
}


void CTrimController::InternalSeek(REFERENCE_TIME position)
{
    m_bInternalSeek = true;
    m_frame.DoSeekTo(position, false);
    m_bInternalSeek = false;
}

REFERENCE_TIME CTrimController::ConfineSeek(REFERENCE_TIME position) const
{
    // While trimming the playhead never leaves [Start, End): the excluded parts are not
    // part of the clip. End is exclusive, so its last reachable position is End - 1.
    if (!m_bActive) {
        return position;
    }
    return std::clamp(position, m_selection.Start(), std::max(m_selection.Start(), m_selection.End() - 1));
}

void CTrimController::LoopToStart()
{
    // Reaching End restarts the selection, like the trim view of the Windows Photos app.
    if (m_selection.IsExportable()) {
        InternalSeek(m_selection.Start());
    } else {
        m_frame.MediaControlPause();
    }
}

void CTrimController::PreviewBoundary(Part part)
{
    // The end is exclusive, so its preview is the frame shown just before it. SeekTo
    // coalesces requests that arrive faster than the graph can seek.
    const REFERENCE_TIME position = part == Part::End ? std::max(m_selection.Start(), m_selection.End() - 1) : m_selection.Start();
    m_frame.SeekTo(position, false);
}

void CTrimController::BeginBoundaryDrag(Part part)
{
    m_selection.SetFocus(part);
    if (m_frame.GetMediaState() == State_Running) {
        m_frame.MediaControlPause();
    }
}

void CTrimController::MoveBoundary(Part part, REFERENCE_TIME position)
{
    if (m_selection.SetBoundary(part, position)) {
        const REFERENCE_TIME preview = part == Part::End ? std::max(m_selection.Start(), m_selection.End() - 1) : m_selection.Start();
        m_frame.m_wndSeekBar.SetTrimPreviewPos(preview);
    }
}

void CTrimController::EndBoundaryDrag(Part part)
{
    PreviewBoundary(part);
}

void CTrimController::SetFocus(Part part)
{
    m_selection.SetFocus(part);
}

bool CTrimController::PreTranslateKey(const MSG* pMsg)
{
    if (!m_bActive || pMsg->message != WM_KEYDOWN) {
        return false;
    }
    // Only keys aimed at the video area or the seekbar; docked panels keep theirs.
    const HWND target = pMsg->hwnd;
    if (target != m_frame.m_hWnd && target != m_frame.m_wndView.m_hWnd && !::IsChild(m_frame.m_wndView.m_hWnd, target)
            && target != m_frame.m_wndSeekBar.m_hWnd) {
        return false;
    }
    if (GetKeyState(VK_CONTROL) < 0 || GetKeyState(VK_MENU) < 0) {
        return false;
    }
    const bool shift = GetKeyState(VK_SHIFT) < 0;

    switch (pMsg->wParam) {
        case VK_ESCAPE:
            if (!m_frame.m_wndSeekBar.CancelTrimDrag()) {
                Exit(true);
            }
            return true;
        case VK_TAB:
            m_selection.CycleFocus(shift);
            Refresh();
            return true;
        case VK_RETURN:
            OnPlaySelection();
            return true;
        case VK_LEFT:
        case VK_RIGHT: {
            const Part focus = m_selection.Focus();
            if (focus != Part::Start && focus != Part::End) {
                return false;   // the playhead keeps the user's seek shortcuts
            }
            const REFERENCE_TIME frame = std::max<REFERENCE_TIME>(std::llround(m_frame.GetAvgTimePerFrame() * 10000000.0), 10000);
            const REFERENCE_TIME step = shift ? 10000000 : frame;
            BeginBoundaryDrag(focus);
            m_selection.SetBoundary(focus, m_selection.Boundary(focus) + (pMsg->wParam == VK_LEFT ? -step : step));
            EndBoundaryDrag(focus);
            Refresh();
            return true;
        }
    }
    return false;
}

bool CTrimController::OnGraphComplete()
{
    // Frame stepping onto the stop position keeps the player's own end-of-step handling.
    if (!m_bActive || m_frame.m_fFrameSteppingActive) {
        return false;
    }
    // An editing session never advances the playlist or runs the end-of-file action.
    LoopToStart();
    return true;
}

void CTrimController::OnPositionPoll(REFERENCE_TIME position)
{
    // DoSeekTo polls the position itself, which would re-enter here.
    if (!m_bActive || m_bInternalSeek || m_frame.m_wndSeekBar.DraggingThumb() || m_frame.m_wndSeekBar.DraggingTrim()) {
        return;
    }
    // Splitters may report a position slightly before the one sought, so only a clear
    // excursion is pulled back (e.g. Stop, which rewinds to zero); re-seeking on every
    // poll would freeze playback at Start.
    // The loop is driven from here rather than by a graph stop position: splitters stop
    // reading at the first packet past it, so with interleaved files the audio would fall
    // silent before the last video frames. The poller runs at its fastest while trimming.
    const REFERENCE_TIME tolerance = 5000000;
    if (position >= m_selection.End() && m_frame.GetMediaState() == State_Running) {
        LoopToStart();
    } else if (position < m_selection.Start() - tolerance || position > m_selection.End() + tolerance) {
        InternalSeek(ConfineSeek(position));
    }
}

void CTrimController::OnMediaClosing()
{
    Exit(false);
}

bool CTrimController::ConfirmClose()
{
    if (!m_pExportDlg) {
        return true;
    }
    if (m_pExportDlg->IsExporting()
            && AfxMessageBox(IDS_TRIM_EXIT_CONFIRM, MB_YESNO | MB_ICONQUESTION | MB_DEFBUTTON2) != IDYES) {
        return false;
    }
    // The dialog runs a modal loop below this call; close once it has unwound.
    m_pExportDlg->Abort();
    m_frame.PostMessage(WM_CLOSE);
    return false;
}

void CTrimController::AppendStatus(CString& msg) const
{
    if (m_bActive) {
        CString text;
        text.Format(IDS_TRIM_STATUSBAR, CTrimSelection::FormatTime(m_selection.Start()).c_str(),
                    CTrimSelection::FormatTime(m_selection.End()).c_str(), CTrimSelection::FormatTime(m_selection.Length()).c_str());
        msg.Append(L"\u2001[" + text + L"]");
    }
}

void CTrimController::ShowContextMenu(CWnd* pOwner, CPoint screenPoint)
{
    CMPCThemeMenu menu;
    menu.CreatePopupMenu();
    const UINT exportable = m_selection.IsExportable() ? MF_ENABLED : MF_GRAYED;
    menu.AppendMenu(MF_STRING | exportable, ID_TRIM_PLAY_SELECTION, ResStr(IDS_TRIM_PLAY_SELECTION) + _T("\tEnter"));
    menu.AppendMenu(MF_SEPARATOR);
    menu.AppendMenu(MF_STRING, ID_TRIM_SET_START, ResStr(IDS_TRIM_SET_START));
    menu.AppendMenu(MF_STRING, ID_TRIM_SET_END, ResStr(IDS_TRIM_SET_END));
    menu.AppendMenu(MF_SEPARATOR);
    menu.AppendMenu(MF_STRING | exportable, ID_TRIM_SAVE_CLIP, ResStr(IDS_TRIM_SAVE_CLIP) + _T("..."));
    menu.AppendMenu(MF_SEPARATOR);
    menu.AppendMenu(MF_STRING, ID_FILE_TRIM_VIDEO, ResStr(IDS_TRIM_CLOSE_MODE) + _T("\tEsc"));
    menu.fulfillThemeReqs();
    const UINT id = menu.TrackPopupMenu(TPM_LEFTBUTTON | TPM_RETURNCMD, screenPoint.x, screenPoint.y, pOwner);
    menu.DestroyMenu();
    if (id) {
        m_frame.PostMessage(WM_COMMAND, id);
    }
}

bool CTrimController::IsExporting() const
{
    return m_pExportDlg && m_pExportDlg->IsExporting();
}

void CTrimController::OnFileTrimVideo()
{
    if (m_bActive) {
        Exit(true);
    } else {
        Enter();
    }
}

void CTrimController::OnUpdateFileTrimVideo(CCmdUI* pCmdUI)
{
    pCmdUI->Enable(m_bActive || CanTrim());
    pCmdUI->SetCheck(m_bActive);
}

void CTrimController::OnSaveClip()
{
    if (!m_bActive || !m_selection.IsExportable() || m_pExportDlg) {
        return;
    }
    const CString source = CurrentSource();
    if (source.IsEmpty()) {
        return;
    }
    CTrimExportDlg dlg(std::wstring(source.GetString()), m_selection.Start(), m_selection.End(), &m_frame);
    m_pExportDlg = &dlg;
    dlg.DoModal();
    m_pExportDlg = nullptr;
}

void CTrimController::OnUpdateSaveClip(CCmdUI* pCmdUI)
{
    pCmdUI->Enable(m_bActive && m_selection.IsExportable() && !m_pExportDlg);
}

void CTrimController::OnPlaySelection()
{
    if (!m_bActive || !m_selection.IsExportable() || m_frame.GetLoadState() != MLS::LOADED) {
        return;
    }
    InternalSeek(m_selection.Start());
    m_frame.SendMessage(WM_COMMAND, ID_PLAY_PLAY);   // the full play path, which also starts a stopped graph
}

void CTrimController::OnUpdatePlaySelection(CCmdUI* pCmdUI)
{
    pCmdUI->Enable(m_bActive && m_selection.IsExportable());
}

void CTrimController::OnSetBoundary(UINT nID)
{
    if (!m_bActive) {
        return;
    }
    // The playhead marks the frame on screen: Start includes it as the first frame and
    // End, being exclusive, is placed just past its start so that it is the last one.
    const REFERENCE_TIME position = m_frame.GetPos();
    if (nID == ID_TRIM_SET_START) {
        m_selection.SetStart(position);
        m_selection.SetFocus(Part::Start);
    } else {
        m_selection.SetEnd(position + 1);
        m_selection.SetFocus(Part::End);
    }
    Refresh();
}

void CTrimController::OnUpdateSetBoundary(CCmdUI* pCmdUI)
{
    pCmdUI->Enable(m_bActive);
}
