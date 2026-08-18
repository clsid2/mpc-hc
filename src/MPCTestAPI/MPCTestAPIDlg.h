/*
 * (C) 2008-2013 see Authors.txt
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

// MPCTestAPIDlg.h : header file
//

#pragma once

#include "../mpc-hc/MpcApi.h"
#include "HScrollListBox.h"
#include "PlayerStateSnapshot.h"


/////////////////////////////////////////////////////////////////////////////
// CRegisterCopyDataDlg dialog

class CRegisterCopyDataDlg : public CDialog
{
    // Construction
public:
    HWND m_RemoteWindow;
    CRegisterCopyDataDlg(CWnd* pParent = nullptr); // standard constructor

    // Dialog Data
    //{{AFX_DATA(CRegisterCopyDataDlg)
    enum { IDD = IDD_REGISTERCOPYDATA_DIALOG };
    // NOTE: the ClassWizard will add data members here
    //}}AFX_DATA

    // ClassWizard generated virtual function overrides
    //{{AFX_VIRTUAL(CRegisterCopyDataDlg)
protected:
    virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
    //}}AFX_VIRTUAL

    // Implementation
protected:
    HICON   m_hIcon;
    HWND    m_hWndMPC;
    CPlayerStateSnapshot m_playerStateSnapshot;

    // Generated message map functions
    //{{AFX_MSG(CRegisterCopyDataDlg)
    virtual BOOL OnInitDialog();
    afx_msg void OnSysCommand(UINT nID, LPARAM lParam);
    afx_msg void OnPaint();
    afx_msg HCURSOR OnQueryDragIcon();
    afx_msg void OnButtonFindwindow();
    afx_msg BOOL OnCopyData(CWnd* pWnd, COPYDATASTRUCT* pCopyDataStruct);
    afx_msg LRESULT OnApiIntMessage(WPARAM wParam, LPARAM lParam);
    afx_msg void OnTimer(UINT_PTR nIDEvent);
    //}}AFX_MSG
    DECLARE_MESSAGE_MAP()
public:
    CString     m_strMPCPath;
    CHScrollListBox m_listBox;
    CString     m_txtCommand;
    int         m_nCommandType;
    afx_msg void OnBnClickedButtonSendcommand();
    void        OnOK() override;
    void        Senddata(MPCAPI_COMMAND nCmd, LPCTSTR strCommand);
    void        StartPlayerStateQuery();
    void        CompletePlayerStateQuery();
};
