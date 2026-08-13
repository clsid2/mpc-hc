/*
 * (C) 2008-2015, 2017 see Authors.txt
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

// MPCTestAPIDlg.cpp : implementation file
//

#include "stdafx.h"
#include "MPCTestAPI.h"
#include "MPCTestAPIDlg.h"
#include <psapi.h>


LPCTSTR GetMPCCommandName(MPCAPI_COMMAND nCmd)
{
    switch (nCmd) {
        case CMD_CONNECT:
            return _T("CMD_CONNECT");
        case CMD_STATE:
            return _T("CMD_STATE");
        case CMD_PLAYMODE:
            return _T("CMD_PLAYMODE");
        case CMD_NOWPLAYING:
            return _T("CMD_NOWPLAYING");
        case CMD_LISTSUBTITLETRACKS:
            return _T("CMD_LISTSUBTITLETRACKS");
        case CMD_LISTAUDIOTRACKS:
            return _T("CMD_LISTAUDIOTRACKS");
        case CMD_PLAYLIST:
            return _T("CMD_PLAYLIST");
        default:
            return _T("CMD_UNK");
    }
}

/////////////////////////////////////////////////////////////////////////////
// CAboutDlg dialog used for App About

class CAboutDlg : public CDialog
{
public:
    CAboutDlg();

    // Dialog Data
    //{{AFX_DATA(CAboutDlg)
    enum { IDD = IDD_ABOUTBOX };
    //}}AFX_DATA

    // ClassWizard generated virtual function overrides
    //{{AFX_VIRTUAL(CAboutDlg)
protected:
    virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
    //}}AFX_VIRTUAL

    // Implementation
protected:
    //{{AFX_MSG(CAboutDlg)
    //}}AFX_MSG
    DECLARE_MESSAGE_MAP()
};

CAboutDlg::CAboutDlg() : CDialog(CAboutDlg::IDD)
{
    //{{AFX_DATA_INIT(CAboutDlg)
    //}}AFX_DATA_INIT
}

void CAboutDlg::DoDataExchange(CDataExchange* pDX)
{
    CDialog::DoDataExchange(pDX);
    //{{AFX_DATA_MAP(CAboutDlg)
    //}}AFX_DATA_MAP
}

BEGIN_MESSAGE_MAP(CAboutDlg, CDialog)
    //{{AFX_MSG_MAP(CAboutDlg)
    // No message handlers
    //}}AFX_MSG_MAP
END_MESSAGE_MAP()


/////////////////////////////////////////////////////////////////////////////
// CRegisterCopyDataDlg dialog

CRegisterCopyDataDlg::CRegisterCopyDataDlg(CWnd* pParent /*=nullptr*/)
    : CDialog(CRegisterCopyDataDlg::IDD, pParent)
    , m_RemoteWindow(nullptr)
    , m_hWndMPC(nullptr)
    , m_nCommandType(0)
{
    //{{AFX_DATA_INIT(CRegisterCopyDataDlg)
    // NOTE: the ClassWizard will add member initialization here
    //}}AFX_DATA_INIT
    // Note that LoadIcon does not require a subsequent DestroyIcon in Win32
    m_hIcon = AfxGetApp()->LoadIcon(IDR_MAINFRAME);
}

void CRegisterCopyDataDlg::DoDataExchange(CDataExchange* pDX)
{
    CDialog::DoDataExchange(pDX);
    //{{AFX_DATA_MAP(CRegisterCopyDataDlg)
    // NOTE: the ClassWizard will add DDX and DDV calls here
    //}}AFX_DATA_MAP
    DDX_Text(pDX, IDC_EDIT1, m_strMPCPath);
    DDX_Control(pDX, IDC_LOGLIST, m_listBox);
    DDX_Text(pDX, IDC_EDIT2, m_txtCommand);
    DDX_CBIndex(pDX, IDC_COMBO1, m_nCommandType);
}

BEGIN_MESSAGE_MAP(CRegisterCopyDataDlg, CDialog)
    //{{AFX_MSG_MAP(CRegisterCopyDataDlg)
    ON_WM_SYSCOMMAND()
    ON_WM_PAINT()
    ON_WM_QUERYDRAGICON()
    ON_BN_CLICKED(IDC_BUTTON_FINDWINDOW, OnButtonFindwindow)
    ON_WM_COPYDATA()
    //}}AFX_MSG_MAP
    ON_BN_CLICKED(IDC_BUTTON_SENDCOMMAND, &CRegisterCopyDataDlg::OnBnClickedButtonSendcommand)
END_MESSAGE_MAP()


/////////////////////////////////////////////////////////////////////////////
// CRegisterCopyDataDlg message handlers

BOOL CRegisterCopyDataDlg::OnInitDialog()
{
    CDialog::OnInitDialog();

    // Add "About..." menu item to system menu.

    // IDM_ABOUTBOX must be in the system command range.
    ASSERT((IDM_ABOUTBOX & 0xFFF0) == IDM_ABOUTBOX);
    ASSERT(IDM_ABOUTBOX < 0xF000);

    CMenu* pSysMenu = GetSystemMenu(FALSE);
    if (pSysMenu != nullptr) {
        CString strAboutMenu;
        if (strAboutMenu.LoadString(IDS_ABOUTBOX)) {
            pSysMenu->AppendMenu(MF_SEPARATOR);
            pSysMenu->AppendMenu(MF_STRING, IDM_ABOUTBOX, strAboutMenu);
        }
    }

    // Set the icon for this dialog.  The framework does this automatically
    // when the application's main window is not a dialog
    SetIcon(m_hIcon, TRUE);         // Set big icon
    SetIcon(m_hIcon, FALSE);        // Set small icon

    // TODO: Add extra initialization here

#if (_MSC_VER < 1910)
    m_strMPCPath = _T("..\\..\\..\\..\\bin15\\");
#else
    m_strMPCPath = _T("..\\..\\..\\..\\bin\\");
#endif

#if defined(_WIN64)
    m_strMPCPath += _T("mpc-hc_x64");
#else
    m_strMPCPath += _T("mpc-hc_x86");
#endif // _WIN64

#if defined(_DEBUG)
    m_strMPCPath += _T("_Debug\\");
#else
    m_strMPCPath += _T("\\");
#endif // _DEBUG

#if defined(_WIN64)
    m_strMPCPath += _T("mpc-hc64.exe");
#else
    m_strMPCPath += _T("mpc-hc.exe");
#endif // _WIN64

    // Keep this feature-specific addition independent of the existing resource/index table.
    VERIFY(SendDlgItemMessage(IDC_COMBO1, CB_ADDSTRING, 0,
                              reinterpret_cast<LPARAM>(_T("CMD_STATUSSHOWMESSAGE"))) == 23);

    UpdateData(FALSE);

    return TRUE;  // return TRUE unless you set the focus to a control
}

void CRegisterCopyDataDlg::OnSysCommand(UINT nID, LPARAM lParam)
{
    if ((nID & 0xFFF0) == IDM_ABOUTBOX) {
        CAboutDlg dlgAbout;
        dlgAbout.DoModal();
    } else {
        CDialog::OnSysCommand(nID, lParam);
    }
}

// If you add a minimize button to your dialog, you will need the code below
//  to draw the icon.  For MFC applications using the document/view model,
//  this is automatically done for you by the framework.

void CRegisterCopyDataDlg::OnPaint()
{
    if (IsIconic()) {
        CPaintDC dc(this); // device context for painting

        SendMessage(WM_ICONERASEBKGND, (WPARAM)dc.GetSafeHdc(), 0);

        // Center icon in client rectangle
        int cxIcon = GetSystemMetrics(SM_CXICON);
        int cyIcon = GetSystemMetrics(SM_CYICON);
        CRect rect;
        GetClientRect(&rect);
        int x = (rect.Width() - cxIcon + 1) / 2;
        int y = (rect.Height() - cyIcon + 1) / 2;

        // Draw the icon
        dc.DrawIcon(x, y, m_hIcon);
    } else {
        CDialog::OnPaint();
    }
}

// The system calls this to obtain the cursor to display while the user drags
//  the minimized window.
HCURSOR CRegisterCopyDataDlg::OnQueryDragIcon()
{
    return (HCURSOR)m_hIcon;
}

void CRegisterCopyDataDlg::OnButtonFindwindow()
{
    CString             strExec;
    STARTUPINFO         StartupInfo;
    PROCESS_INFORMATION ProcessInfo;

    strExec.Format(_T("%s /slave %d"), (LPCTSTR)m_strMPCPath, PtrToInt(GetSafeHwnd()));
    UpdateData(TRUE);

    ZeroMemory(&StartupInfo, sizeof(StartupInfo));
    StartupInfo.cb = sizeof(StartupInfo);
    GetStartupInfo(&StartupInfo);
    if (CreateProcess(nullptr, (LPTSTR)(LPCTSTR)strExec, nullptr, nullptr, FALSE, 0, nullptr, nullptr, &StartupInfo, &ProcessInfo)) {
        CloseHandle(ProcessInfo.hProcess);
        CloseHandle(ProcessInfo.hThread);
    }
}

void CRegisterCopyDataDlg::Senddata(MPCAPI_COMMAND nCmd, LPCTSTR strCommand)
{
    if (m_hWndMPC) {
        COPYDATASTRUCT MyCDS;

        MyCDS.dwData = nCmd;
        MyCDS.cbData = (DWORD)(_tcslen(strCommand) + 1) * sizeof(TCHAR);
        MyCDS.lpData = (LPVOID) strCommand;

        ::SendMessage(m_hWndMPC, WM_COPYDATA, (WPARAM)GetSafeHwnd(), (LPARAM)&MyCDS);
    }
}

BOOL CRegisterCopyDataDlg::OnCopyData(CWnd* pWnd, COPYDATASTRUCT* pCopyDataStruct)
{
    CString strMsg;

    if (pCopyDataStruct->dwData == CMD_CONNECT) {
        m_hWndMPC = (HWND)IntToPtr(_ttoi((LPCTSTR)pCopyDataStruct->lpData));
    }

    strMsg.Format(_T("%s : %s"), GetMPCCommandName((MPCAPI_COMMAND)pCopyDataStruct->dwData), (LPCTSTR)pCopyDataStruct->lpData);
    m_listBox.InsertString(0, strMsg);
    return CDialog::OnCopyData(pWnd, pCopyDataStruct);
}

void CRegisterCopyDataDlg::OnBnClickedButtonSendcommand()
{
    CString strEmpty(_T(""));
    UpdateData(TRUE);

    switch (m_nCommandType) {
        case 0:
            Senddata(CMD_OPENFILE, m_txtCommand);
            break;
        case 1:
            Senddata(CMD_STOP, strEmpty);
            break;
        case 2:
            Senddata(CMD_CLOSEFILE, strEmpty);
            break;
        case 3:
            Senddata(CMD_PLAYPAUSE, strEmpty);
            break;
        case 4:
            Senddata(CMD_ADDTOPLAYLIST, m_txtCommand);
            break;
        case 5:
            Senddata(CMD_STARTPLAYLIST, strEmpty);
            break;
        case 6:
            Senddata(CMD_CLEARPLAYLIST, strEmpty);
            break;
        case 7:
            Senddata(CMD_SETPOSITION, m_txtCommand);
            break;
        case 8:
            Senddata(CMD_SETAUDIODELAY, m_txtCommand);
            break;
        case 9:
            Senddata(CMD_SETSUBTITLEDELAY, m_txtCommand);
            break;
        case 10:
            Senddata(CMD_GETAUDIOTRACKS, strEmpty);
            break;
        case 11:
            Senddata(CMD_GETSUBTITLETRACKS, strEmpty);
            break;
        case 12:
            Senddata(CMD_GETPLAYLIST, strEmpty);
            break;
        case 13:
            Senddata(CMD_SETINDEXPLAYLIST, m_txtCommand);
            break;
        case 14:
            Senddata(CMD_SETAUDIOTRACK, m_txtCommand);
            break;
        case 15:
            Senddata(CMD_SETSUBTITLETRACK, m_txtCommand);
            break;
        case 16:
            Senddata(CMD_TOGGLEFULLSCREEN, m_txtCommand);
            break;
        case 17:
            Senddata(CMD_JUMPFORWARDMED, m_txtCommand);
            break;
        case 18:
            Senddata(CMD_JUMPBACKWARDMED, m_txtCommand);
            break;
        case 19:
            Senddata(CMD_INCREASEVOLUME, m_txtCommand);
            break;
        case 20:
            Senddata(CMD_DECREASEVOLUME, m_txtCommand);
            break;
        case 21:
            //Senddata(CMD_SHADER_TOGGLE, m_txtCommand);
            break;
        case 22:
            Senddata(CMD_CLOSEAPP, m_txtCommand);
            break;
        case 23:
            Senddata(CMD_STATUSSHOWMESSAGE, m_txtCommand);
            break;
    }
}
