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

#include "TrimSelection.h"

class CMainFrame;
class CTrimExportDlg;

// Trim mode: the selection edited on the seekbar, its preview through the playback graph
// and the Save Clip workflow. Composed into CMainFrame, which routes its commands here
// from OnCmdMsg and forwards the few playback events it needs.
class CTrimController : public CCmdTarget
{
public:
    using Part = CTrimSelection::Part;

    explicit CTrimController(CMainFrame& frame);
    ~CTrimController();

    bool IsActive() const { return m_bActive; }
    const CTrimSelection& Selection() const { return m_selection; }

    // Seekbar interaction.
    void BeginBoundaryDrag(Part part);
    void MoveBoundary(Part part, REFERENCE_TIME position);
    void EndBoundaryDrag(Part part);
    void SetFocus(Part part);
    void ShowContextMenu(CWnd* pOwner, CPoint screenPoint);

    // Main frame hooks.
    bool PreTranslateKey(const MSG* pMsg);
    bool OnGraphComplete();
    void OnPositionPoll(REFERENCE_TIME position);
    REFERENCE_TIME ConfineSeek(REFERENCE_TIME position) const;
    void OnMediaClosing();
    bool ConfirmClose();
    void AppendStatus(CString& msg) const;

private:
    CMainFrame& m_frame;
    CTrimSelection m_selection;
    bool m_bActive = false;
    bool m_bInternalSeek = false;
    bool m_bShowedSeekbar = false;
    REFERENCE_TIME m_entryPosition = 0;
    bool m_bEntryRunning = false;
    CTrimExportDlg* m_pExportDlg = nullptr;    // while the modal Save Clip dialog runs

    bool CanTrim() const;
    CString CurrentSource() const;
    void Enter();
    void Exit(bool bRestorePlayback);
    void Refresh();
    void PreviewBoundary(Part part);
    void LoopToStart();
    void InternalSeek(REFERENCE_TIME position);
    bool IsExporting() const;

    afx_msg void OnFileTrimVideo();
    afx_msg void OnUpdateFileTrimVideo(CCmdUI* pCmdUI);
    afx_msg void OnSaveClip();
    afx_msg void OnUpdateSaveClip(CCmdUI* pCmdUI);
    afx_msg void OnPlaySelection();
    afx_msg void OnUpdatePlaySelection(CCmdUI* pCmdUI);
    afx_msg void OnSetBoundary(UINT nID);
    afx_msg void OnUpdateSetBoundary(CCmdUI* pCmdUI);

    DECLARE_MESSAGE_MAP()
};
