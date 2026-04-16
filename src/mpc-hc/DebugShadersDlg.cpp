/*
 * (C) 2013-2016 see Authors.txt
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
#include "DebugShadersDlg.h"
#include "mplayerc.h"
#include "PathUtils.h"
#include "SettingsDefines.h"
#include "AppSettings.h"

CModelessDialog::CModelessDialog(UINT nIDTemplate, CWnd* pParent)
    : CMPCThemeModelessResizableDialog(nIDTemplate, pParent)
{
}

BOOL CModelessDialog::DestroyWindow()
{
    CWnd* pMainWnd = AfxGetApp()->m_pMainWnd;
    if (pMainWnd) {
        pMainWnd->SetActiveWindow();
    }
    return __super::DestroyWindow();
}

void CModelessDialog::OnCancel()
{
    DestroyWindow();
}

void CModelessDialog::OnOK()
{
    // don't close the dialog on Enter key
}

CDebugShadersDlg::CDebugShadersDlg()
    : CModelessDialog(IDD, GetDesktopWindow())
    , m_timerOneTime(this, TIMER_ONETIME_START, TIMER_ONETIME_END - TIMER_ONETIME_START + 1)
    , m_Compiler(nullptr)
{
    Create(IDD, GetDesktopWindow());
}

BOOL CDebugShadersDlg::OnInitDialog()
{
    EnableSaveRestoreKey(IDS_R_DEBUG_SHADERS);

    __super::OnInitDialog();

    EventRouter::EventSelection receives;
    receives.insert(MpcEvent::SHADER_LIST_CHANGED);
    GetEventd().Connect(m_eventc, receives, std::bind(&CDebugShadersDlg::EventCallback, this, std::placeholders::_1));

    // Set window icon
    VERIFY(SetIcon(AfxGetMainWnd()->GetIcon(true), true) == nullptr);

    // Setup window auto-resize and restore last position
    SetSizeGripVisibility(FALSE);
    SetMinTrackSize(CSize(360, 100));
    SetupAnchors();

    CWinApp* pApp = AfxGetApp();

    // Restore controls' old state
    m_iVersion = pApp->GetProfileInt(IDS_R_DEBUG_SHADERS, IDS_RS_DEBUG_SHADERS_LASTVERSION, ps_3_0);
    VERIFY(UpdateData(FALSE));
    CString oldpath = pApp->GetProfileString(IDS_R_DEBUG_SHADERS, IDS_RS_DEBUG_SHADERS_LASTFILE);
    if (!oldpath.IsEmpty()) {
        ASSERT(m_Shaders.GetCount() == 0);
        Shader t;
        t.filePath = oldpath;
        m_list.clear();
        m_list.push_back(t);
        int sel = AppendShader(0, oldpath);
        if (sel >= 0) {
            VERIFY(m_Shaders.SetCurSel(sel) != CB_ERR);
        } else {
            ASSERT(FALSE);
        }
    }

    // Put WM_PAINT message before WM_APP_RECOMPILE_SHADER
    UpdateWindow();

    // Load new shader list
    OnListRefresh();

    // We need to trigger shader compilation manually when
    // old state's selected shader is present in current shader list.
    // Otherwise it's triggered by OnListRefresh()
    int sel = m_Shaders.GetCurSel();
    if (sel != CB_ERR) {
        CString path = GetShaderPath(sel);
        ASSERT(!path.IsEmpty());
        if (oldpath == path) {
            UpdateNotifierState();
            VERIFY(PostMessage(WM_APP_RECOMPILE_SHADER));
        }
    }

    // Display first-run dialog
    if (pApp->GetProfileInt(IDS_R_DEBUG_SHADERS, IDS_RS_DEBUG_SHADERS_FIRSTRUN, 1)) {
        CString msg;
        if (msg.LoadString(IDS_DEBUGSHADERS_FIRSTRUN_MSG)) {
            AfxMessageBox(msg, MB_ICONINFORMATION);
        } else {
            ASSERT(FALSE);
        }
        VERIFY(pApp->WriteProfileInt(IDS_R_DEBUG_SHADERS, IDS_RS_DEBUG_SHADERS_FIRSTRUN, 0));
    }
    EnableThemedDialogTooltips(this);

    return TRUE;
}

BOOL CDebugShadersDlg::DestroyWindow()
{
    VERIFY(UpdateData(TRUE));
    VERIFY(AfxGetApp()->WriteProfileInt(IDS_R_DEBUG_SHADERS, IDS_RS_DEBUG_SHADERS_LASTVERSION, m_iVersion));
    CString path;
    int sel = m_Shaders.GetCurSel();
    if (sel != CB_ERR) {
        path = GetShaderPath(sel);
    }
    VERIFY(AfxGetApp()->WriteProfileString(IDS_R_DEBUG_SHADERS, IDS_RS_DEBUG_SHADERS_LASTFILE, path));
    return __super::DestroyWindow();
}

void CDebugShadersDlg::EventCallback(MpcEvent ev)
{
    switch (ev) {
        case MpcEvent::SHADER_LIST_CHANGED:
            if (m_hWnd) {
                OnListRefresh();
            }
            break;
        default:
            ASSERT(FALSE);
    }
}

BOOL CDebugShadersDlg::OnToolTipNotify(UINT id, NMHDR* pNMHDR, LRESULT* pResult) {
    TOOLTIPTEXT* pTTT = (TOOLTIPTEXT*)pNMHDR;

    UINT_PTR nID = pNMHDR->idFrom;
    if (pTTT->uFlags & TTF_IDISHWND) {
        nID = ::GetDlgCtrlID((HWND)nID);
    }

    bool bRet = false;

    static CString strTipText;

    switch (nID) {
    case IDC_COMBO1:
        int sel = m_Shaders.GetCurSel();
        if (sel != CB_ERR) {
            strTipText = GetShaderPath(sel);
        }
        bRet = true;
        break;
    }

    if (bRet) {
        pTTT->lpszText = (LPWSTR)(LPCWSTR)strTipText;
        PlaceThemedDialogTooltip(nID);
    }

    return bRet;
}

FileChangeNotifier::FileSet CDebugShadersDlg::GetWatchedList()
{
    FileChangeNotifier::FileSet ret;
    int sel = m_Shaders.GetCurSel();
    if (sel != CB_ERR) {
        CString path = GetShaderPath(sel);
        ret.insert(path);
    }
    return ret;
}

void CDebugShadersDlg::WatchedFilesChanged(const FileSet& changes)
{
    m_timerOneTime.Subscribe(TimerOneTimeSubscriber::SELECTED_SHADER_CHANGE_COOLDOWN,
                             std::bind(&CDebugShadersDlg::ShaderFileChangedCooldownCallback, this), 50);
}

void CDebugShadersDlg::ShaderFileChangedCooldownCallback()
{
    VERIFY(PostMessage(WM_APP_RECOMPILE_SHADER));
}

void CDebugShadersDlg::OnListRefresh()
{
    const auto& s = AfxGetAppSettings();
    CString path;
    int oldSel = m_Shaders.GetCurSel();
    if (oldSel != CB_ERR) {
        path = GetShaderPath(oldSel);
        ASSERT(!path.IsEmpty());
    }
    m_list = ShaderList::GetDefaultShaders();
    m_list.insert(m_list.cend(), s.m_ShadersExtraList.cbegin(), s.m_ShadersExtraList.cend());
    m_Shaders.ResetContent();
    for (std::size_t i = 0; i != m_list.size(); ++i) {
        const auto& shader = m_list[i];
        ASSERT(!shader.filePath.IsEmpty());
        int idx = AppendShader((int)i, shader.filePath);
        if (idx >= 0) {
            if (shader.filePath == path) {
                VERIFY(m_Shaders.SetCurSel(idx) != CB_ERR);
            }
        } else {
            ASSERT(FALSE);
        }
    }
    if (m_Shaders.GetCurSel() == CB_ERR) {
        if (m_Shaders.GetCount() > 0) {
            VERIFY(m_Shaders.SetCurSel(0) != CB_ERR);
        }
        UpdateNotifierState();
        VERIFY(PostMessage(WM_APP_RECOMPILE_SHADER));
    }
}

int CDebugShadersDlg::AppendShader(int loc, CString filePath) {
    int idx = m_Shaders.InsertString(-1, PathUtils::BaseName(filePath));
    m_Shaders.SetItemData(idx, loc);
    return idx;
}

CString CDebugShadersDlg::GetShaderPath(int loc) {
    int curIndex = m_Shaders.GetItemData(loc);
    return m_list[curIndex].filePath;
}

void CDebugShadersDlg::DoDataExchange(CDataExchange* pDX)
{
    CModelessDialog::DoDataExchange(pDX);
    DDX_Control(pDX, IDC_COMBO1, m_Shaders);
    DDX_Control(pDX, IDC_EDIT1, m_DebugInfo);
    DDX_Radio(pDX, IDC_RADIO1, m_iVersion);
    fulfillThemeReqs();
}

void CDebugShadersDlg::SetupAnchors()
{
    AddAnchor(IDC_COMBO1, TOP_LEFT, TOP_RIGHT);
    AddAnchor(IDC_STATIC1, TOP_LEFT, BOTTOM_RIGHT);
    AddAnchor(IDC_EDIT1, TOP_LEFT, BOTTOM_RIGHT);
    AddAnchor(IDC_RADIO1, TOP_RIGHT);
    AddAnchor(IDC_RADIO2, TOP_RIGHT);
    AddAnchor(IDC_RADIO3, TOP_RIGHT);
    AddAnchor(IDC_RADIO4, TOP_RIGHT);
}

BOOL CDebugShadersDlg::PreTranslateMessage(MSG* pMsg)
{
    RelayThemedDialogTooltip(pMsg);
    return __super::PreTranslateMessage(pMsg);
}

void CDebugShadersDlg::OnTimer(UINT_PTR nIDEvent)
{
    if (nIDEvent >= TIMER_ONETIME_START && nIDEvent <= TIMER_ONETIME_END) {
        m_timerOneTime.NotifySubscribers(nIDEvent);
    } else {
        __super::OnTimer(nIDEvent);
    }
}

void CDebugShadersDlg::OnRecompileShader()
{
    m_timerOneTime.Unsubscribe(TimerOneTimeSubscriber::SELECTED_SHADER_CHANGE_COOLDOWN);
    MSG msg;
    if (PeekMessage(&msg, m_hWnd, WM_APP_RECOMPILE_SHADER, WM_APP_RECOMPILE_SHADER, PM_NOREMOVE | PM_NOYIELD)) {
        return;
    }
    int sel = m_Shaders.GetCurSel();
    if (sel != CB_ERR) {
        CString filePath = GetShaderPath(sel);
        if (PathUtils::IsFile(filePath)) {
            CStringA profile;
            switch (m_iVersion) {
                case ps_2_0:
                    profile = "ps_2_0";
                    break;
                case ps_2_b:
                    profile = "ps_2_b";
                    break;
                case ps_2_a:
                    profile = "ps_2_a";
                    break;
                case ps_3_0:
                    profile = "ps_3_0";
                    break;
                default:
                    ASSERT(FALSE);
                    profile = "ps_3_0";
                    break;
            }
            CString disasm, compilerMsg;
            if (SUCCEEDED(m_Compiler.CompileShaderFromFile(filePath, "main", profile,
                                                           D3DCOMPILE_DEBUG, nullptr, &disasm, &compilerMsg))) {
                if (!compilerMsg.IsEmpty()) {
                    compilerMsg += _T("\n");
                }
                compilerMsg += disasm;
            }
            compilerMsg.Replace(_T("\n"), _T("\r\n"));
            m_DebugInfo.SetWindowText(compilerMsg);
        } else {
            m_DebugInfo.SetWindowText(_T("File not found"));
        }
    } else {
        m_DebugInfo.SetWindowText(_T(""));
    }
}

void CDebugShadersDlg::OnSelChange()
{
    UpdateNotifierState();
    VERIFY(PostMessage(WM_APP_RECOMPILE_SHADER));
}

void CDebugShadersDlg::OnVersionClicked()
{
    auto old = m_iVersion;
    VERIFY(UpdateData(TRUE));
    if (old != m_iVersion) {
        VERIFY(PostMessage(WM_APP_RECOMPILE_SHADER));
    }
}

BEGIN_MESSAGE_MAP(CDebugShadersDlg, CModelessDialog)
    ON_WM_TIMER()
    ON_NOTIFY_EX(TTN_NEEDTEXT, 0, OnToolTipNotify)
    ON_CBN_SELCHANGE(IDC_COMBO1, OnSelChange)
    ON_BN_CLICKED(IDC_RADIO1, OnVersionClicked)
    ON_BN_CLICKED(IDC_RADIO2, OnVersionClicked)
    ON_BN_CLICKED(IDC_RADIO3, OnVersionClicked)
    ON_BN_CLICKED(IDC_RADIO4, OnVersionClicked)
    ON_MESSAGE_VOID(WM_APP_RECOMPILE_SHADER, OnRecompileShader)
END_MESSAGE_MAP()
