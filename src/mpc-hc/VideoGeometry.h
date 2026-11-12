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

class CMainFrame;
struct CPlayerGraph;

int nearest90(int angle);

// Zoom, video frame, pan and scan, rotation and aspect ratio commands, and
// the pan, zoom and angle state they work on. Composed into CMainFrame, which
// routes their commands here from OnCmdMsg. Window sizing (ZoomVideoWindow
// and its lock flags) and MoveVideoWindow stay in CMainFrame, which reads the
// state below when it lays out the video.
class CVideoGeometry : public CCmdTarget
{
    CMainFrame& m_frame;
    CPlayerGraph& m_graph;

public:
    explicit CVideoGeometry(CMainFrame& frame);

    double m_ZoomX, m_ZoomY, m_PosX, m_PosY;
    int m_AngleX, m_AngleY, m_AngleZ;
    int m_iDefRotation;

    bool PerformFlipRotate();
    void ApplyPanNScanPresetString();

    afx_msg void OnViewZoom(UINT nID);
    afx_msg void OnUpdateViewZoom(CCmdUI* pCmdUI);
    afx_msg void OnViewZoomAutoFit();
    afx_msg void OnViewZoomAutoFitLarger();
    afx_msg void OnViewModifySize(UINT nID);
    afx_msg void OnViewDefaultVideoFrame(UINT nID);
    afx_msg void OnUpdateViewDefaultVideoFrame(CCmdUI* pCmdUI);
    afx_msg void OnViewSwitchVideoFrame();
    afx_msg void OnViewCompMonDeskARDiff();
    afx_msg void OnUpdateViewCompMonDeskARDiff(CCmdUI* pCmdUI);
    afx_msg void OnViewPanNScan(UINT nID);
    afx_msg void OnUpdateViewPanNScan(CCmdUI* pCmdUI);
    afx_msg void OnViewPanNScanPresets(UINT nID);
    afx_msg void OnUpdateViewPanNScanPresets(CCmdUI* pCmdUI);
    afx_msg void OnViewRotate(UINT nID);
    afx_msg void OnUpdateViewRotate(CCmdUI* pCmdUI);
    afx_msg void OnViewAspectRatio(UINT nID);
    afx_msg void OnUpdateViewAspectRatio(CCmdUI* pCmdUI);
    afx_msg void OnViewAspectRatioNext();

    DECLARE_MESSAGE_MAP()

private:
    void ValidatePanNScanParameters();
};
