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

#include "../DSUtil/HdmvClipInfo.h"

class CMainFrame;
struct CPlayerGraph;
class OpenDVDData;

// DVD and Blu-ray: opening a disc or a BDMV folder, the DVD chapters, menus
// and the angle, audio and subtitle commands, and the Blu-ray playlist and
// meta state. Composed into CMainFrame, which routes its commands here from
// OnCmdMsg. File chapters and the window title stay in CMainFrame, because
// they serve every file.
class CDiscController : public CCmdTarget
{
    CMainFrame& m_frame;
    CPlayerGraph& m_graph;

public:
    explicit CDiscController(CMainFrame& frame);

    // DVD
    bool        m_fValidDVDOpen;
    DVD_DOMAIN  m_iDVDDomain;
    DWORD       m_iDVDTitle;
    bool        m_bDVDStillOn;

    void OpenDVD(OpenDVDData* pODD);
    void SetupDVDChapters();
    bool SeekToDVDChapter(int iChapter, bool bRelative = false);
    LPCTSTR GetDVDAudioFormatName(const DVD_AudioAttributes& ATR) const;

    // Blu-ray
    CString m_LastOpenBDPath;
    CHdmvClipInfo::HdmvPlaylist m_MPLSPlaylist;
    bool m_bIsBDPlay;
    bool m_bHasBDMeta;
    CAtlList<CHdmvClipInfo::BDMVMeta> m_BDMeta;

    bool OpenBD(CString Path);
    void OpenDVDOrBD(CStringW path);
    CHdmvClipInfo::BDMVMeta GetBDMVMeta();

    afx_msg void OnFileOpendvd();
    afx_msg void OnDvdAngle(UINT nID);
    afx_msg void OnDvdAudio(UINT nID);
    afx_msg void OnDvdSub(UINT nID);
    afx_msg void OnDvdSubOnOff();
    afx_msg void OnNavigateMenu(UINT nID);
    afx_msg void OnUpdateNavigateMenu(CCmdUI* pCmdUI);

    DECLARE_MESSAGE_MAP()
};
