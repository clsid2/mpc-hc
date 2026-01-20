/*
 * (C) 2003-2006 Gabest
 * (C) 2006-2015, 2017 see Authors.txt
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
#include "FileAssoc.h"
#include "PPagePlayer.h"


// CPPagePlayer dialog

IMPLEMENT_DYNAMIC(CPPagePlayer, CMPCThemePPageBase)
CPPagePlayer::CPPagePlayer()
    : CMPCThemePPageBase(CPPagePlayer::IDD, CPPagePlayer::IDD)
    , m_iAllowMultipleInst(0)
    , m_iTitleBarTextStyle(0)
    , m_bTitleBarTextTitle(0)
    , m_fRememberWindowPos(FALSE)
    , m_fRememberWindowSize(FALSE)
    , m_fSavePnSZoom(FALSE)
    , m_fUseIni(FALSE)
    , m_fTrayIcon(FALSE)
    , m_fKeepHistory(FALSE)
    , m_fHideCDROMsSubMenu(FALSE)
    , m_priority(FALSE)
    , m_fRememberDVDPos(FALSE)
    , m_fRememberFilePos(FALSE)
    , m_bRememberPlaylistItems(TRUE)
    , m_bEnableCoverArt(TRUE)
    , m_dwCheckIniLastTick(0)
{
}

CPPagePlayer::~CPPagePlayer()
{
}

void CPPagePlayer::DoDataExchange(CDataExchange* pDX)
{
    __super::DoDataExchange(pDX);
    DDX_Control(pDX, IDI_SINGLE, m_iconSingle);
    DDX_Control(pDX, IDI_MULTI, m_iconMulti);
    DDX_Radio(pDX, IDC_RADIO1, m_iAllowMultipleInst);
    DDX_Radio(pDX, IDC_RADIO3, m_iTitleBarTextStyle);
    DDX_Check(pDX, IDC_CHECK13, m_bTitleBarTextTitle);
    DDX_Check(pDX, IDC_CHECK3, m_fTrayIcon);
    DDX_Check(pDX, IDC_CHECK6, m_fRememberWindowPos);
    DDX_Check(pDX, IDC_CHECK7, m_fRememberWindowSize);
    DDX_Check(pDX, IDC_CHECK11, m_fSavePnSZoom);
    DDX_Check(pDX, IDC_CHECK8, m_fUseIni);
    DDX_Check(pDX, IDC_CHECK1, m_fKeepHistory);
    DDX_Check(pDX, IDC_CHECK10, m_fHideCDROMsSubMenu);
    DDX_Check(pDX, IDC_CHECK9, m_priority);
    DDX_Check(pDX, IDC_DVD_POS, m_fRememberDVDPos);
    DDX_Check(pDX, IDC_FILE_POS, m_fRememberFilePos);
    DDX_Check(pDX, IDC_CHECK2, m_bRememberPlaylistItems);
    DDX_Check(pDX, IDC_CHECK14, m_bEnableCoverArt);
}

BEGIN_MESSAGE_MAP(CPPagePlayer, CMPCThemePPageBase)
    ON_UPDATE_COMMAND_UI(IDC_CHECK13, OnUpdateCheck13)
    ON_UPDATE_COMMAND_UI(IDC_DVD_POS, OnUpdatePos)
    ON_UPDATE_COMMAND_UI(IDC_FILE_POS, OnUpdatePos)
    ON_UPDATE_COMMAND_UI(IDC_CHECK8, OnUpdateSaveToIni)
END_MESSAGE_MAP()

// CPPagePlayer message handlers

BOOL CPPagePlayer::OnInitDialog()
{
    __super::OnInitDialog();

    m_iconSingle.SetIcon((HICON)LoadImage(AfxGetInstanceHandle(), MAKEINTRESOURCE(IDI_SINGLE), IMAGE_ICON, 0, 0, LR_DEFAULTSIZE | LR_SHARED));
    m_iconMulti.SetIcon((HICON)LoadImage(AfxGetInstanceHandle(), MAKEINTRESOURCE(IDI_MULTI), IMAGE_ICON, 0, 0, LR_DEFAULTSIZE | LR_SHARED));

    const CAppSettings& s = AfxGetAppSettings();

    m_iAllowMultipleInst = s.fAllowMultipleInst;
    m_iTitleBarTextStyle = s.iTitleBarTextStyle;
    m_bTitleBarTextTitle = s.fTitleBarTextTitle;
    m_fTrayIcon = s.fTrayIcon;
    m_fRememberWindowPos = s.fRememberWindowPos;
    m_fRememberWindowSize = s.fRememberWindowSize;
    m_fSavePnSZoom = s.fSavePnSZoom;
    m_fUseIni = AfxGetMyApp()->IsIniValid();
    m_fKeepHistory = s.fKeepHistory;
    m_fHideCDROMsSubMenu = s.fHideCDROMsSubMenu;
    m_priority = s.dwPriority != NORMAL_PRIORITY_CLASS;
    m_fRememberDVDPos = s.fRememberDVDPos;
    m_fRememberFilePos = s.fRememberFilePos;
    m_bRememberPlaylistItems = s.bRememberPlaylistItems;
    m_bEnableCoverArt = s.bEnableCoverArt;


    UpdateData(FALSE);

    GetDlgItem(IDC_FILE_POS)->EnableWindow(s.fKeepHistory);
    GetDlgItem(IDC_DVD_POS)->EnableWindow(s.fKeepHistory);

    return TRUE;  // return TRUE unless you set the focus to a control
    // EXCEPTION: OCX Property Pages should return FALSE
}

BOOL CPPagePlayer::OnApply()
{
    UpdateData();

    CAppSettings& s = AfxGetAppSettings();

    s.fAllowMultipleInst = !!m_iAllowMultipleInst;
    s.iTitleBarTextStyle = m_iTitleBarTextStyle;
    s.fTitleBarTextTitle = !!m_bTitleBarTextTitle;
    s.fTrayIcon = !!m_fTrayIcon;
    s.fRememberWindowPos = !!m_fRememberWindowPos;
    s.fRememberWindowSize = !!m_fRememberWindowSize;
    s.fSavePnSZoom = !!m_fSavePnSZoom;
    s.fKeepHistory = !!m_fKeepHistory;
    s.fHideCDROMsSubMenu = !!m_fHideCDROMsSubMenu;
    s.dwPriority = m_priority ? ABOVE_NORMAL_PRIORITY_CLASS : NORMAL_PRIORITY_CLASS;
    s.fRememberDVDPos = !!m_fRememberDVDPos;
    s.fRememberFilePos = !!m_fRememberFilePos;
    s.bRememberPlaylistItems = !!m_bRememberPlaylistItems;
    s.bEnableCoverArt = !!m_bEnableCoverArt;

    if (!m_fKeepHistory) {
        s.ClearRecentFiles();

        // Ensure no new items are added in Windows recent menu and in the "Recent" jump list
        s.fileAssoc.SetNoRecentDocs(true, true);
    } else {
        // Re-enable Windows recent menu and the "Recent" jump list if needed
        s.fileAssoc.SetNoRecentDocs(false, true);
    }

    // Check if the settings location needs to be changed
    if (AfxGetMyApp()->IsIniValid() != !!m_fUseIni) {
        AfxGetMyApp()->ChangeSettingsLocation(!!m_fUseIni);
    }

    // There is no main frame when the option dialog is displayed stand-alone
    if (CMainFrame* pMainFrame = AfxGetMainFrame()) {
        pMainFrame->ShowTrayIcon(s.fTrayIcon);
        pMainFrame->UpdateControlState(CMainFrame::UPDATE_MEDIA_ART);
        pMainFrame->UpdateControlState(CMainFrame::UPDATE_WINDOW_TITLE);
    }

    ::SetPriorityClass(::GetCurrentProcess(), s.dwPriority);

    GetDlgItem(IDC_FILE_POS)->EnableWindow(s.fKeepHistory);
    GetDlgItem(IDC_DVD_POS)->EnableWindow(s.fKeepHistory);

    return __super::OnApply();
}

void CPPagePlayer::OnUpdateCheck13(CCmdUI* pCmdUI)
{
    UpdateData();

    pCmdUI->Enable(m_iTitleBarTextStyle == 1);
}

void CPPagePlayer::OnUpdatePos(CCmdUI* pCmdUI)
{
    UpdateData();

    pCmdUI->Enable(!!m_fKeepHistory);
}

void CPPagePlayer::OnUpdateSaveToIni(CCmdUI* pCmdUI)
{
    ULONGLONG dwTick = GetTickCount64();
    // run this check no often than once per second
    if (dwTick - m_dwCheckIniLastTick >= 1000ULL) {
        CPath iniPath = AfxGetMyApp()->GetIniPath();
        HANDLE hFile = CreateFile(iniPath, GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, nullptr);
        if (hFile == INVALID_HANDLE_VALUE) {
            CPath iniDirPath(iniPath);
            VERIFY(iniDirPath.RemoveFileSpec());
            HANDLE hDir = CreateFile(iniDirPath, GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, nullptr);
            // gray-out "save to .ini" option when we don't have writing permissions in the target directory
            pCmdUI->Enable(hDir != INVALID_HANDLE_VALUE);
            CloseHandle(hDir);
        }
        CloseHandle(hFile);
        m_dwCheckIniLastTick = dwTick;
    }
}
