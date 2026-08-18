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


// Client-side pseudo-command: composes a state snapshot from several GET replies.
static const DWORD_PTR CLIENT_QUERYPLAYERSTATE_TOKEN = static_cast<DWORD_PTR>(-2);
static const MPCAPI_COMMAND CLIENT_QUERYPLAYERSTATE_CMD = static_cast<MPCAPI_COMMAND>(CLIENT_QUERYPLAYERSTATE_TOKEN);
static const UINT_PTR PLAYER_STATE_QUERY_TIMER_ID = 0x5150;

struct APICommandEntry {
    LPCTSTR label;
    MPCAPI_COMMAND command;
    bool usesParameter;
};

// Non-blocking integer message channel (see MpcApi.h). Both sides register the same name.
static const UINT WM_MPCAPI_INT = RegisterWindowMessage(MPCAPI_INT_MESSAGE_NAME);

static const APICommandEntry apiCommands[] = {
    { _T("CMD_OPENFILE"), CMD_OPENFILE, true },
    { _T("CMD_STOP"), CMD_STOP, false },
    { _T("CMD_CLOSEFILE"), CMD_CLOSEFILE, false },
    { _T("CMD_PLAYPAUSE"), CMD_PLAYPAUSE, false },
    { _T("CMD_PLAY"), CMD_PLAY, false },
    { _T("CMD_PAUSE"), CMD_PAUSE, false },
    { _T("CMD_ADDTOPLAYLIST"), CMD_ADDTOPLAYLIST, true },
    { _T("CMD_STARTPLAYLIST"), CMD_STARTPLAYLIST, false },
    { _T("CMD_CLEARPLAYLIST"), CMD_CLEARPLAYLIST, false },
    { _T("CMD_SETPOSITION"), CMD_SETPOSITION, true },
    { _T("CMD_SETAUDIODELAY"), CMD_SETAUDIODELAY, true },
    { _T("CMD_SETSUBTITLEDELAY"), CMD_SETSUBTITLEDELAY, true },
    { _T("CMD_GETAUDIOTRACKS"), CMD_GETAUDIOTRACKS, false },
    { _T("CMD_GETSUBTITLETRACKS"), CMD_GETSUBTITLETRACKS, false },
    { _T("CMD_GETPLAYLIST"), CMD_GETPLAYLIST, false },
    { _T("CMD_SETINDEXPLAYLIST"), CMD_SETINDEXPLAYLIST, true },
    { _T("CMD_SETAUDIOTRACK"), CMD_SETAUDIOTRACK, true },
    { _T("CMD_SETSUBTITLETRACK"), CMD_SETSUBTITLETRACK, true },
    { _T("CMD_GETCURRENTAUDIOTRACK"), CMD_GETCURRENTAUDIOTRACK, false },
    { _T("CMD_GETCURRENTSUBTITLETRACK"), CMD_GETCURRENTSUBTITLETRACK, false },
    { _T("CMD_GETCURRENTPOSITION"), CMD_GETCURRENTPOSITION, false },
    { _T("CMD_GETNOWPLAYING"), CMD_GETNOWPLAYING, false },
    { _T("CMD_JUMPOFNSECONDS"), CMD_JUMPOFNSECONDS, true },
    { _T("CMD_TOGGLEFULLSCREEN"), CMD_TOGGLEFULLSCREEN, false },
    { _T("CMD_JUMPFORWARDMED"), CMD_JUMPFORWARDMED, false },
    { _T("CMD_JUMPBACKWARDMED"), CMD_JUMPBACKWARDMED, false },
    { _T("CMD_INCREASEVOLUME"), CMD_INCREASEVOLUME, false },
    { _T("CMD_DECREASEVOLUME"), CMD_DECREASEVOLUME, false },
    { _T("CMD_GETVERSION"), CMD_GETVERSION, false },
    { _T("CMD_SHADER_TOGGLE"), CMD_SHADER_TOGGLE, false },
    { _T("CMD_CLOSEAPP"), CMD_CLOSEAPP, false },
    { _T("CMD_SETSPEED"), CMD_SETSPEED, true },
    { _T("CMD_SETPANSCAN"), CMD_SETPANSCAN, true },
    { _T("CMD_STATUSSHOWMESSAGE"), CMD_STATUSSHOWMESSAGE, true },
    { _T("CMD_SETVOLUME (0-100)"), CMD_SETVOLUME, true },
    { _T("CMD_SETMUTE (0/1)"), CMD_SETMUTE, true },
    { _T("CMD_GETVOLUME"), CMD_GETVOLUME, false },
    { _T("CMD_GETMUTE"), CMD_GETMUTE, false },
    { _T("CLIENT_QUERYPLAYERSTATE (non-atomic)"), CLIENT_QUERYPLAYERSTATE_CMD, false },
};

LPCTSTR GetMPCCommandName(MPCAPI_COMMAND nCmd)
{
    switch (nCmd) {
        case CMD_CONNECT:
            return _T("CMD_CONNECT");
        case CMD_DISCONNECT:
            return _T("CMD_DISCONNECT");
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
        case CMD_CURRENTPOSITION:
            return _T("CMD_CURRENTPOSITION");
        case CMD_NOTIFYSEEK:
            return _T("CMD_NOTIFYSEEK");
        case CMD_NOTIFYENDOFSTREAM:
            return _T("CMD_NOTIFYENDOFSTREAM");
        case CMD_VERSION:
            return _T("CMD_VERSION");
        case CMD_CURRENTAUDIOTRACK:
            return _T("CMD_CURRENTAUDIOTRACK");
        case CMD_CURRENTSUBTITLETRACK:
            return _T("CMD_CURRENTSUBTITLETRACK");
        case CMD_CURRENTVOLUME:
            return _T("CMD_CURRENTVOLUME");
        case CMD_CURRENTMUTE:
            return _T("CMD_CURRENTMUTE");
        default:
            static CString strResult;
            strResult.Format(_T("UNKNOWN (0x%08X)"), (unsigned int)nCmd);
            return strResult;
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
    ON_WM_TIMER()
    ON_REGISTERED_MESSAGE(WM_MPCAPI_INT, OnApiIntMessage)
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

    CComboBox* pCombo = static_cast<CComboBox*>(GetDlgItem(IDC_COMBO1));
    if (pCombo) {
        pCombo->ResetContent();
        for (const auto& entry : apiCommands) {
            pCombo->AddString(entry.label);
        }
    }

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
    UpdateData(TRUE);

    CString commandLine;
    commandLine.Format(_T("\"%s\" /slave %d"), m_strMPCPath.GetString(), PtrToInt(GetSafeHwnd()));

    STARTUPINFO startupInfo = { sizeof(startupInfo) };
    PROCESS_INFORMATION processInfo = {};
    const BOOL created = CreateProcess(m_strMPCPath.GetString(), commandLine.GetBuffer(), nullptr, nullptr,
                                       FALSE, 0, nullptr, nullptr, &startupInfo, &processInfo);
    const DWORD error = created ? ERROR_SUCCESS : GetLastError();
    commandLine.ReleaseBuffer();

    if (!created) {
        LPTSTR systemMessage = nullptr;
        CString detail;
        if (FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
                          nullptr, error, 0, reinterpret_cast<LPTSTR>(&systemMessage), 0, nullptr) && systemMessage) {
            detail = systemMessage;
            detail.Trim();
            LocalFree(systemMessage);
        }
        if (detail.IsEmpty()) {
            detail = _T("No system error message is available.");
        }

        CString message;
        message.Format(_T("Failed to start MPC-HC.\nError %lu: %s"), error, detail.GetString());
        AfxMessageBox(message, MB_ICONERROR | MB_OK);
        return;
    }

    CloseHandle(processInfo.hProcess);
    CloseHandle(processInfo.hThread);
}

void CRegisterCopyDataDlg::Senddata(MPCAPI_COMMAND nCmd, LPCTSTR strCommand)
{
    if (m_hWndMPC && IsWindow(m_hWndMPC)) {
        COPYDATASTRUCT MyCDS;

        MyCDS.dwData = nCmd;
        MyCDS.cbData = (DWORD)(_tcslen(strCommand) + 1) * sizeof(TCHAR);
        MyCDS.lpData = (LPVOID) strCommand;

        DWORD_PTR result = 0;
        SendMessageTimeout(m_hWndMPC, WM_COPYDATA, (WPARAM)GetSafeHwnd(), (LPARAM)&MyCDS,
                           SMTO_ABORTIFHUNG | SMTO_ERRORONEXIT, 1000, &result);
    }
}

BOOL CRegisterCopyDataDlg::OnCopyData(CWnd* pWnd, COPYDATASTRUCT* pCopyDataStruct)
{
    if (!pCopyDataStruct || !pCopyDataStruct->lpData
            || pCopyDataStruct->cbData < sizeof(TCHAR)
            || pCopyDataStruct->cbData % sizeof(TCHAR) != 0) {
        return FALSE;
    }

    const LPCTSTR value = static_cast<LPCTSTR>(pCopyDataStruct->lpData);
    const size_t length = pCopyDataStruct->cbData / sizeof(TCHAR);
    if (value[length - 1] != _T('\0') || wmemchr(value, L'\0', length - 1)) {
        return FALSE;
    }

    const MPCAPI_COMMAND command = (MPCAPI_COMMAND)pCopyDataStruct->dwData;
    const HWND hSender = pWnd ? pWnd->GetSafeHwnd() : nullptr;
    if (command == CMD_CONNECT && hSender && IsWindow(hSender)) {
        m_hWndMPC = hSender;
        Senddata(CMD_GETVERSION, _T(""));
        // Opt into the non-blocking integer channel: MPC-HC will then send volume/mute
        // change notifications through it instead of WM_COPYDATA.
        if (WM_MPCAPI_INT) {
            ::PostMessage(m_hWndMPC, WM_MPCAPI_INT, (WPARAM)GetSafeHwnd(),
                          MPCAPI_INT_MAKELPARAM(MPCAPI_INT_VERSION, MPCINT_HELLO));
        }
    } else if (command == CMD_DISCONNECT && hSender == m_hWndMPC) {
        m_hWndMPC = nullptr;
    }

    if (!m_playerStateSnapshot.Capture(command, value)) {
        CString strMsg;
        strMsg.Format(_T("%s : %s"), GetMPCCommandName(command), value);
        m_listBox.InsertString(0, strMsg);
    }
    return TRUE;
}

LRESULT CRegisterCopyDataDlg::OnApiIntMessage(WPARAM wParam, LPARAM lParam)
{
    // Integer channel notification from MPC-HC (see MpcApi.h pack/unpack macros).
    const WORD command = MPCAPI_INT_COMMAND_OF(lParam);
    const int value = MPCAPI_INT_VALUE_OF(lParam);
    CString strMsg;
    strMsg.Format(_T("INT cmd=%u : %d"), command, value);
    m_listBox.InsertString(0, strMsg);
    return 0;
}

void CRegisterCopyDataDlg::OnTimer(UINT_PTR nIDEvent)
{
    if (nIDEvent == PLAYER_STATE_QUERY_TIMER_ID) {
        KillTimer(PLAYER_STATE_QUERY_TIMER_ID);
        CompletePlayerStateQuery();
        return;
    }

    CDialog::OnTimer(nIDEvent);
}

void CRegisterCopyDataDlg::StartPlayerStateQuery()
{
    if (m_playerStateSnapshot.IsActive()) {
        KillTimer(PLAYER_STATE_QUERY_TIMER_ID);
        CompletePlayerStateQuery();
    }

    m_playerStateSnapshot.Begin();
    const UINT_PTR timer = SetTimer(PLAYER_STATE_QUERY_TIMER_ID,
                                    CPlayerStateSnapshot::COLLECTION_WINDOW_MS, nullptr);

    for (size_t i = 0; i < CPlayerStateSnapshot::GetRequestCount(); i++) {
        Senddata(CPlayerStateSnapshot::GetRequest(i), _T(""));
    }

    if (!timer) {
        CompletePlayerStateQuery();
    }
}

void CRegisterCopyDataDlg::CompletePlayerStateQuery()
{
    if (m_playerStateSnapshot.IsActive()) {
        m_listBox.InsertString(0, m_playerStateSnapshot.Complete());
    }
}

void CRegisterCopyDataDlg::OnBnClickedButtonSendcommand()
{
    UpdateData(TRUE);

    if (m_nCommandType >= 0 && m_nCommandType < _countof(apiCommands)) {
        const APICommandEntry& entry = apiCommands[m_nCommandType];
        LPCTSTR param = entry.usesParameter ? m_txtCommand.GetString() : _T("");
        if (entry.command == CLIENT_QUERYPLAYERSTATE_CMD) {
            StartPlayerStateQuery();
        } else {
            Senddata(entry.command, param);
        }
    }
}

void CRegisterCopyDataDlg::OnOK()
{
    CWnd* pFocus = GetFocus();
    if (!pFocus) {
        return;
    }

    const int controlId = pFocus->GetDlgCtrlID();
    if (controlId == IDC_EDIT1 || controlId == IDC_BUTTON_FINDWINDOW) {
        OnButtonFindwindow();
    } else if (controlId == IDC_EDIT2 || controlId == IDC_COMBO1 || controlId == IDC_BUTTON_SENDCOMMAND) {
        OnBnClickedButtonSendcommand();
    }
}
