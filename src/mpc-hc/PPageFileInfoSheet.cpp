/*
 * (C) 2003-2006 Gabest
 * (C) 2006-2014, 2016 see Authors.txt
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
#include "mplayerc.h"
#include "MainFrm.h"
#include "PPageFileInfoSheet.h"
#include "PPageFileMediaInfo.h"
#include "CMPCTheme.h"


// CPPageFileInfoSheet

IMPLEMENT_DYNAMIC(CPPageFileInfoSheet, CMPCThemeResizablePropertySheet)
CPPageFileInfoSheet::CPPageFileInfoSheet(CString path, CString ydlsrc, CMainFrame* pMainFrame, CWnd* pParentWnd)
    : CMPCThemeResizablePropertySheet(IDS_PROPSHEET_PROPERTIES, pParentWnd, 0)
    , m_clip(path, ydlsrc, pMainFrame->m_pGB, pMainFrame->m_pFSF, pMainFrame->m_pDVDI)
    , m_details(path, ydlsrc, pMainFrame->m_pGB, pMainFrame->m_pCAP, pMainFrame->m_pFSF, pMainFrame->m_pDVDI)
    , m_res(path, pMainFrame->m_pGB, pMainFrame->m_pFSF)
    , m_mi(path, pMainFrame->m_pFSF, pMainFrame->m_pDVDI, pMainFrame)
    , m_path(path)
{
    AddPage(&m_details);
    AddPage(&m_clip);

    if (m_res.HasResources()) {
        AddPage(&m_res);
    }

    if (CPPageFileMediaInfo::HasMediaInfo()) {
        AddPage(&m_mi);
    }
}

CPPageFileInfoSheet::~CPPageFileInfoSheet()
{
}


BEGIN_MESSAGE_MAP(CPPageFileInfoSheet, CMPCThemeResizablePropertySheet)
    ON_BN_CLICKED(IDC_BUTTON_MI, OnSaveAs)
END_MESSAGE_MAP()

// CPPageFileInfoSheet message handlers

BOOL CPPageFileInfoSheet::OnInitDialog()
{
    __super::OnInitDialog();

    GetDlgItem(IDCANCEL)->ShowWindow(SW_HIDE);
    GetDlgItem(ID_APPLY_NOW)->ShowWindow(SW_HIDE);
    GetDlgItem(IDOK)->SetWindowText(ResStr(IDS_AG_CLOSE));

    CRect r;
    GetDlgItem(ID_APPLY_NOW)->GetWindowRect(&r);
    ScreenToClient(r);
    RemoveAnchor(IDOK); //otherwise it crashes when we add it later
    AddAnchor(IDOK, BOTTOM_LEFT);
    GetDlgItem(IDOK)->MoveWindow(r);

    r.MoveToX(5);
    r.right += 24;
    m_Button_MI.Create(ResStr(IDS_AG_SAVE_AS), WS_CHILD | BS_PUSHBUTTON | WS_VISIBLE, r, this, IDC_BUTTON_MI);
    m_Button_MI.SetFont(GetFont());
    m_Button_MI.ShowWindow(SW_HIDE);

    GetTabControl()->SetFocus();

    CMPCThemeUtil::enableWindows10DarkFrame(this);

    AddAnchor(IDC_BUTTON_MI, BOTTOM_LEFT);

    return FALSE;  // return TRUE unless you set the focus to a control
}

void CPPageFileInfoSheet::OnSaveAs()
{
    m_mi.OnSaveAs();
}

