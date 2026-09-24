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
#include "PPageWebServer.h"
#include "PathUtils.h"
#include <afxglobals.h>
#include "WebServer.h"
#include "CMPCThemeMsgBox.h"

// CPPageWebServer dialog

IMPLEMENT_DYNAMIC(CPPageWebServer, CMPCThemePPageBase)
CPPageWebServer::CPPageWebServer()
    : CMPCThemePPageBase(CPPageWebServer::IDD, CPPageWebServer::IDD)
    , m_fEnableWebServer(FALSE)
    , m_nWebServerPort(0)
    , m_launch(_T("http://localhost:13579/"))
    , m_fWebServerUseCompression(FALSE)
    , m_fWebServerLocalhostOnly(FALSE)
    , m_bWebUIEnablePreview(FALSE)
    , m_fWebServerPrintDebugInfo(FALSE)
    , m_fWebRoot(FALSE)
{
}

CPPageWebServer::~CPPageWebServer()
{
}

void CPPageWebServer::DoDataExchange(CDataExchange* pDX)
{
    __super::DoDataExchange(pDX);
    DDX_Check(pDX, IDC_CHECK1, m_fEnableWebServer);
    DDX_Text(pDX, IDC_EDIT1, m_nWebServerPort);
    DDX_Control(pDX, IDC_EDIT1, m_nWebServerPortCtrl);
    DDV_MinMaxInt(pDX, m_nWebServerPort, 1, 65535);
    DDX_Control(pDX, IDC_STATIC1, m_launch);
    DDX_Check(pDX, IDC_CHECK3, m_fWebServerUseCompression);
    DDX_Check(pDX, IDC_CHECK5, m_fWebServerLocalhostOnly);
    DDX_Check(pDX, IDC_CHECK6, m_bWebUIEnablePreview);
    DDX_Check(pDX, IDC_CHECK2, m_fWebServerPrintDebugInfo);
    DDX_Check(pDX, IDC_CHECK4, m_fWebRoot);
    DDX_Text(pDX, IDC_EDIT2, m_WebRoot);
    DDX_Text(pDX, IDC_EDIT3, m_WebServerCGI);
    DDX_Text(pDX, IDC_EDIT9, m_WebDefIndex);
}

BOOL CPPageWebServer::PreTranslateMessage(MSG* pMsg)
{
    if (pMsg->message == WM_LBUTTONDOWN && pMsg->hwnd == m_launch.m_hWnd) {
        UpdateData();

        if (m_fEnableWebServer) {
            if (AfxGetAppSettings().nWebServerPort != m_nWebServerPort) {
                AfxMessageBox(IDS_WEBSERVER_ERROR_TEST, MB_ICONEXCLAMATION | MB_OK, 0);
                return TRUE;
            }
        }
    }

    return CMPCThemePPageBase::PreTranslateMessage(pMsg);
}

BOOL CPPageWebServer::OnInitDialog()
{
    __super::OnInitDialog();

    const CAppSettings& s = AfxGetAppSettings();

    m_fEnableWebServer = s.fEnableWebServer;
    m_nWebServerPort = s.nWebServerPort;
    m_fWebServerUseCompression = s.fWebServerUseCompression;
    m_fWebServerLocalhostOnly = s.fWebServerLocalhostOnly;
    m_bWebUIEnablePreview = s.bWebUIEnablePreview;
    m_fWebServerPrintDebugInfo = s.fWebServerPrintDebugInfo;
    m_fWebRoot = s.strWebRoot.Find('*') < 0;
    m_WebRoot = s.strWebRoot;
    m_WebRoot.TrimLeft(_T('*'));
    m_WebDefIndex = s.strWebDefIndex;
    m_WebServerCGI = s.strWebServerCGI;

    m_launch.EnableWindow(m_fEnableWebServer);

    UpdateData(FALSE);

    OnEnChangeEdit1();

    return TRUE;  // return TRUE unless you set the focus to a control
    // EXCEPTION: OCX Property Pages should return FALSE
}

BOOL CPPageWebServer::OnApply()
{
    UpdateData();

    CAppSettings& s = AfxGetAppSettings();

    CString NewWebRoot = m_WebRoot;
    if (!m_fWebRoot) {
        NewWebRoot = _T("*") + NewWebRoot;
    }

    bool fRestart = s.nWebServerPort != m_nWebServerPort
                    || s.strWebRoot != NewWebRoot || s.strWebServerCGI != m_WebServerCGI;

    s.fEnableWebServer = !!m_fEnableWebServer;
    s.nWebServerPort = m_nWebServerPort;
    s.fWebServerUseCompression = !!m_fWebServerUseCompression;
    s.fWebServerLocalhostOnly = !!m_fWebServerLocalhostOnly;
    s.bWebUIEnablePreview = !!m_bWebUIEnablePreview;
    s.fWebServerPrintDebugInfo = !!m_fWebServerPrintDebugInfo;
    s.strWebRoot = NewWebRoot;
    s.strWebDefIndex = m_WebDefIndex;
    s.strWebServerCGI = m_WebServerCGI;

    if (CMainFrame* pMainFrame = AfxGetMainFrame()) {
        if (m_fEnableWebServer) {
            if (fRestart) {
                pMainFrame->StopWebServer();
            }
            pMainFrame->StartWebServer(m_nWebServerPort);
        } else {
            pMainFrame->StopWebServer();
        }
    }

    m_launch.EnableWindow(m_fEnableWebServer);

    return __super::OnApply();
}

CString CPPageWebServer::GetCurWebRoot()
{
    CString WebRoot;
    GetDlgItem(IDC_EDIT2)->GetWindowText(WebRoot);
    WebRoot.Replace('/', '\\');

    CLongPath path;
    path.Combine(PathUtils::GetProgramPath(), WebRoot);
    return path.IsDirectory() ? (LPCTSTR)path : _T("");
}

static int __stdcall BrowseCtrlCallback(HWND hwnd, UINT uMsg, LPARAM lParam, LPARAM lpData)
{
    if (uMsg == BFFM_INITIALIZED && lpData) {
        ::SendMessage(hwnd, BFFM_SETSELECTION, TRUE, lpData);
    }
    return 0;
}

bool CPPageWebServer::PickDir(CString& dir)
{
    CString strTitle(StrRes(IDS_PPAGEWEBSERVER_0));
    bool success = false;

    CFileDialog dlg(TRUE);
    IFileOpenDialog* openDlgPtr = dlg.GetIFileOpenDialog();

    if (openDlgPtr != nullptr) {
        openDlgPtr->SetTitle(strTitle);
        openDlgPtr->SetOptions(FOS_PICKFOLDERS | FOS_FORCEFILESYSTEM | FOS_PATHMUSTEXIST);

        if (!dir.IsEmpty()) {
            CComPtr<IShellItem> psiFolder;
            if (SUCCEEDED(afxGlobalData.ShellCreateItemFromParsingName(dir, nullptr, IID_PPV_ARGS(&psiFolder)))) {
                openDlgPtr->SetFolder(psiFolder);
            }
        }

        if (SUCCEEDED(openDlgPtr->Show(m_hWnd))) {
            dir = dlg.GetFolderPath();
            success = true;
        }

        openDlgPtr->Release();
    }

    return success;
}

BEGIN_MESSAGE_MAP(CPPageWebServer, CMPCThemePPageBase)
    ON_EN_CHANGE(IDC_EDIT1, OnEnChangeEdit1)
    ON_BN_CLICKED(IDC_BUTTON1, OnBnClickedButton1)
    ON_BN_CLICKED(IDC_BUTTON2, OnBnClickedButton2)
    ON_UPDATE_COMMAND_UI(IDC_BUTTON1, OnUpdateButton2)
    ON_BN_CLICKED(IDC_CHECK6, OnEnablePreviewChecked)
END_MESSAGE_MAP()


// CPPageWebServer message handlers

void CPPageWebServer::OnEnChangeEdit1()
{
    UpdateData();

    CString link;
    link.Format(_T("http://localhost:%d/"), m_nWebServerPort);
    m_launch.m_link = link;

    SetModified();
}

void CPPageWebServer::OnBnClickedButton1()
{
    CString dir = GetCurWebRoot();
    if (PickDir(dir)) {
        CLongPath path;
        if (path.RelativePathTo(PathUtils::GetProgramPath(), FILE_ATTRIBUTE_DIRECTORY, dir, FILE_ATTRIBUTE_DIRECTORY)) {
            dir = (LPCTSTR)path;
        }
        m_WebRoot = dir;

        UpdateData(FALSE);
        SetModified();
    }
}

void CPPageWebServer::OnBnClickedButton2()
{
    CString dir;
    if (PickDir(dir)) {
        dir += _T("\\");
        CWebServer::Deploy(dir);
    }
}

void CPPageWebServer::OnUpdateButton2(CCmdUI* pCmdUI)
{
    pCmdUI->Enable(GetDlgItem(IDC_EDIT2)->GetWindowTextLength() > 0);
}

void CPPageWebServer::OnEnablePreviewChecked()
{
    if (IsDlgButtonChecked(IDC_CHECK6)
            && (CMPCThemeMsgBox::MessageBox(this, ResStr(IDS_WEBUI_PREVIEW_WARNING), nullptr, MB_ICONQUESTION | MB_YESNO | MB_DEFBUTTON2) == IDNO)) {
        CheckDlgButton(IDC_CHECK6, BST_UNCHECKED);
    } else {
        SetModified();
    }
}
