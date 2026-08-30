/*
 * (C) 2003-2006 Gabest
 * (C) 2006-2014, 2017 see Authors.txt
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

#include "PPageBase.h"
#include "EventDispatcher.h"
#include "CMPCThemePPageBase.h"
#include "CMPCThemeComboBox.h"


// CPPagePlayer dialog

class CPPagePlayer : public CMPCThemePPageBase
{
    DECLARE_DYNAMIC(CPPagePlayer)

public:
    CPPagePlayer();
    virtual ~CPPagePlayer();

    int m_iAllowMultipleInst;
    int m_iTitleBarTextStyle;
    BOOL m_bTitleBarTextTitle;
    BOOL m_fRememberWindowPos;
    BOOL m_fRememberWindowSize;
    BOOL m_fSavePnSZoom;
    BOOL m_fUseIni;
    BOOL m_fTrayIcon;
    BOOL m_fKeepHistory;
    BOOL m_fHideCDROMsSubMenu;
    BOOL m_priority;
    BOOL m_fRememberDVDPos;
    BOOL m_fRememberFilePos;
    BOOL m_bRememberPlaylistItems;
    BOOL m_bEnableCoverArt;
    BOOL m_bEnableCasting;

    ULONGLONG m_dwCheckIniLastTick;

    // Dialog Data
    enum { IDD = IDD_PPAGEPLAYER };

protected:
    CStatic m_iconSingle;
    CStatic m_iconMulti;

    virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
    virtual BOOL OnInitDialog();
    virtual BOOL OnApply();

    DECLARE_MESSAGE_MAP()

public:
    afx_msg void OnUpdateTimeout(CCmdUI* pCmdUI);
    afx_msg void OnUpdateCheck13(CCmdUI* pCmdUI);
    afx_msg void OnUpdatePos(CCmdUI* pCmdUI);
    void OnUpdateSaveToIni(CCmdUI* pCmdUI);
};
