/*
 * (C) 2003-2006 Gabest
 * (C) 2006-2017 see Authors.txt
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
#include "MainFrm.h"
#include "mplayerc.h"
#include "PPageFullscreen.h"
#include "CMPCTheme.h"

#include "Monitors.h"
#include "MultiMonitor.h"

#define SANE_TIMEOUT_FOR_SHOW_CONTROLS_ON_MOUSE_MOVE 200

// CPPagePlayer dialog

IMPLEMENT_DYNAMIC(CPPageFullscreen, CMPCThemePPageBase)
CPPageFullscreen::CPPageFullscreen()
    : CMPCThemePPageBase(CPPageFullscreen::IDD, CPPageFullscreen::IDD)
    , m_iFullScreenMonitor(0)
    , m_bLaunchFullscreen(FALSE)
    , m_fExitFullScreenAtTheEnd(FALSE)
    , m_bHideFullscreenControls(FALSE)
    , m_uHideFullscreenControlsDelay(0)
    , m_bHideFullscreenDockedPanels(FALSE)
    , m_nCurrentDisplayModeIndex(0)
    , m_bAutoChangeFSModeEnabled(FALSE)
    , m_bAutoChangeFSModeApplyDefModeAtFSExist(TRUE)
    , m_bAutoChangeFSModeRestoreResAfterProgExit(TRUE)
    , m_uAutoChangeFullscrResDelay(0)
    , m_list(0)
{
    m_FullScreenSeparateControlsText = CStringW(StrRes(IDS_PPAGEADVANCED_FULLSCREEN_SEPARATE_CONTROLS));
}

CPPageFullscreen::~CPPageFullscreen()
{
}

void CPPageFullscreen::ModesUpdate()
{
    DisplayMode currentDisplayMode;
    if (!CMainFrame::GetCurDispMode(m_fullScreenMonitorID, currentDisplayMode)) {
        ASSERT(FALSE);
        return;
    }

    m_list.SetRedraw(FALSE);
    m_list.DeleteAllItems();
    m_displayModes.clear();
    m_displayModesString.RemoveAll();
    m_nCurrentDisplayModeIndex = 0;

    // Get the full list of available display modes
    for (int i = 0;; i++) {
        DisplayMode dm;
        if (!CMainFrame::GetDispMode(m_fullScreenMonitorID, i, dm)) {
            break;
        }
        if (dm.bpp != 32 || dm.size.cx < 640) {
            continue; // skip low resolution and non 32bpp mode
        }

        m_displayModes.emplace_back(dm);
    }
    ASSERT(!m_displayModes.empty());

    // Sort the available display modes
    std::sort(m_displayModes.begin(), m_displayModes.end());
    // Then deduplicate them
    m_displayModes.erase(std::unique(m_displayModes.begin(), m_displayModes.end()), m_displayModes.end());

    // Generate the corresponding string representation
    auto formatStringFromDisplayMode = [](const DisplayMode & dm) {
        CString strDisplayMode;
        strDisplayMode.Format(_T("[ %d ] @ %ldx%ld %c"),
                              dm.freq, dm.size.cx, dm.size.cy,
                              (dm.dwDisplayFlags & DM_INTERLACED) ? _T('i') : _T('p'));
        return strDisplayMode;
    };
    m_CurrentDisplayModeString = formatStringFromDisplayMode(currentDisplayMode);
    for (const auto& dm : m_displayModes) {
        m_displayModesString.AddTail(formatStringFromDisplayMode(dm));

        if (currentDisplayMode == dm) {
            m_nCurrentDisplayModeIndex = m_displayModes.size() - 1;
        }
    }

    // Populate the vector with default modes on first initialization
    if (m_autoChangeFSModes.empty()) {
        auto addMode = [this, &currentDisplayMode](double dFRStart, double dFRStop) {
            m_autoChangeFSModes.emplace_back(true, dFRStart, dFRStop, 0, currentDisplayMode);
        };
        addMode(0.0, 0.0); // Default mode
        addMode(23.500, 23.981);
        addMode(23.982, 24.499);
        addMode(24.500, 25.499);
        addMode(29.500, 29.981);
        addMode(29.982, 30.499);
        addMode(49.500, 50.499);
        addMode(59.500, 59.945);
        addMode(59.946, 60.499);
    }

    auto findDisplayMode = [this](const DisplayMode & dm) {
        auto it = std::lower_bound(m_displayModes.cbegin(), m_displayModes.cend(), dm);

        if (it != m_displayModes.cend() && !(dm < *it)) {
            return int(it - m_displayModes.cbegin());
        } else {
            return -1;
        }
    };

    int nItem = 0;
    for (const auto& mode : m_autoChangeFSModes) {
        CString strItemPos;
        strItemPos.Format(_T("%02d"), nItem);
        VERIFY(m_list.InsertItem(nItem, strItemPos) == nItem);

        m_list.SetCheck(nItem, mode.bChecked);

        // Find the corresponding display mode index
        int iDisplayMode = findDisplayMode(mode.dm);
        if (iDisplayMode < 0) {
            iDisplayMode = (int)m_nCurrentDisplayModeIndex;
        }
        m_list.SetItemData(nItem, (DWORD_PTR)iDisplayMode);

        m_list.SetItemText(nItem, COL_DISPLAY_MODE, formatStringFromDisplayMode(mode.dm));

        TCHAR buffer[11];
        if (nItem == 0) { // Special case for default mode
            VERIFY(m_list.SetItemText(nItem, COL_N, ResStr(IDS_PPAGE_FS_DEFAULT)));
            VERIFY(m_list.SetItemText(nItem, COL_FRAMERATE_START, ResStr(IDS_PPAGE_FS_OTHER)));
            VERIFY(m_list.SetItemText(nItem, COL_FRAMERATE_STOP, ResStr(IDS_PPAGE_FS_OTHER)));
            const auto& s = AfxGetAppSettings();
            _itot_s(s.fAudioTimeShift ? s.iAudioTimeShift : 0, buffer, 10);
            VERIFY(m_list.SetItemText(nItem, COL_AUDIO_DELAY, buffer));
        } else {
            CString strFrameRate;
            strFrameRate.Format(_T("%.3f"), mode.dFrameRateStart);
            VERIFY(m_list.SetItemText(nItem, COL_FRAMERATE_START, strFrameRate));
            strFrameRate.Format(_T("%.3f"), mode.dFrameRateStop);
            VERIFY(m_list.SetItemText(nItem, COL_FRAMERATE_STOP, strFrameRate));
            _itot_s(mode.msAudioDelay, buffer, 10);
            VERIFY(m_list.SetItemText(nItem, COL_AUDIO_DELAY, buffer));
        }

        nItem++;
    }

    for (int i = 0; i <= COL_DISPLAY_MODE; i++) {
        m_list.SetColumnWidth(i, LVSCW_AUTOSIZE);
        int nColumnWidth = m_list.GetColumnWidth(i);
        m_list.SetColumnWidth(i, LVSCW_AUTOSIZE_USEHEADER);
        int nHeaderWidth = m_list.GetColumnWidth(i);
        m_list.SetColumnWidth(i, std::max(nColumnWidth, nHeaderWidth));
    }

    m_list.SetRedraw(TRUE);
}

void CPPageFullscreen::DoDataExchange(CDataExchange* pDX)
{
    __super::DoDataExchange(pDX);
    DDX_Check(pDX, IDC_CHECK1, m_bLaunchFullscreen);
    DDX_CBIndex(pDX, IDC_COMBO1, m_iFullScreenMonitor);
    DDX_Control(pDX, IDC_COMBO1, m_fullScreenMonitorCtrl);
    DDX_Control(pDX, IDC_LIST1, m_list);
    DDX_Check(pDX, IDC_CHECK4, m_bHideFullscreenControls);
    DDX_Control(pDX, IDC_COMBO2, m_hidePolicy);
    DDX_Text(pDX, IDC_EDIT1, m_uHideFullscreenControlsDelay);
    DDX_Check(pDX, IDC_CHECK6, m_bHideFullscreenDockedPanels);
    DDX_Check(pDX, IDC_CHECK5, m_fExitFullScreenAtTheEnd);
    DDX_Check(pDX, IDC_CHECK2, m_bAutoChangeFSModeEnabled);
    DDX_Check(pDX, IDC_CHECK3, m_bAutoChangeFSModeApplyDefModeAtFSExist);
    DDX_Check(pDX, IDC_RESTORERESCHECK, m_bAutoChangeFSModeRestoreResAfterProgExit);
    DDX_Text(pDX, IDC_EDIT2, m_uAutoChangeFullscrResDelay);
    DDX_Control(pDX, IDC_SPIN1, m_delaySpinner);
    DDX_Check(pDX, IDC_CHECK7, m_bFullScreenSeparateControls);
}

BEGIN_MESSAGE_MAP(CPPageFullscreen, CMPCThemePPageBase)
    ON_CBN_SELCHANGE(IDC_COMBO1, OnUpdateFullScreenMonitor)
    ON_UPDATE_COMMAND_UI(IDC_COMBO2, OnUpdateHideControls)
    ON_UPDATE_COMMAND_UI(IDC_CHECK6, OnUpdateHideControls)
    ON_CBN_SELCHANGE(IDC_COMBO2, OnHideControlsPolicyChange)
    ON_UPDATE_COMMAND_UI(IDC_EDIT1, OnUpdateHideDelay)
    ON_UPDATE_COMMAND_UI(IDC_STATIC1, OnUpdateHideDelay)
    ON_UPDATE_COMMAND_UI(IDC_LIST1, OnUpdateAutoChangeFullscreenMode)
    ON_UPDATE_COMMAND_UI(IDC_CHECK3, OnUpdateAutoChangeFullscreenMode)
    ON_UPDATE_COMMAND_UI(IDC_RESTORERESCHECK, OnUpdateAutoChangeFullscreenMode)
    ON_UPDATE_COMMAND_UI(IDC_BUTTON1, OnUpdateAutoChangeFullscreenMode)
    ON_UPDATE_COMMAND_UI(IDC_EDIT2, OnUpdateAutoChangeFullscreenMode)
    ON_UPDATE_COMMAND_UI(IDC_SPIN1, OnUpdateAutoChangeFullscreenMode)
    ON_UPDATE_COMMAND_UI(IDC_STATIC2, OnUpdateAutoChangeFullscreenMode)
    ON_CLBN_CHKCHANGE(IDC_LIST1, OnListCheckChange)
    ON_BN_CLICKED(IDC_BUTTON1, OnAdd)
    ON_BN_CLICKED(IDC_BUTTON2, OnRemove)
    ON_UPDATE_COMMAND_UI(IDC_BUTTON2, OnUpdateRemove)
    ON_BN_CLICKED(IDC_BUTTON3, OnMoveUp)
    ON_UPDATE_COMMAND_UI(IDC_BUTTON3, OnUpdateUp)
    ON_BN_CLICKED(IDC_BUTTON4, OnMoveDown)
    ON_UPDATE_COMMAND_UI(IDC_BUTTON4, OnUpdateDown)
    ON_NOTIFY(LVN_BEGINLABELEDIT, IDC_LIST1, OnListBeginEdit)
    ON_NOTIFY(LVN_DOLABELEDIT, IDC_LIST1, OnListDoEdit)
    ON_NOTIFY(LVN_ENDLABELEDIT, IDC_LIST1, OnListEndEdit)
    ON_NOTIFY(NM_CUSTOMDRAW, IDC_LIST1, OnListCustomDraw)
    ON_NOTIFY_EX(TTN_NEEDTEXT, 0, OnToolTipNotify)
END_MESSAGE_MAP()

// CPPagePlayer message handlers

BOOL CPPageFullscreen::OnInitDialog()
{
    __super::OnInitDialog();

    SetHandCursor(m_hWnd, IDC_COMBO1);

    CAppSettings& s = AfxGetAppSettings();

    m_fullScreenMonitorID = s.strFullScreenMonitorID;
    m_fullScreenMonitorDeviceName = s.strFullScreenMonitorDeviceName;

    m_bLaunchFullscreen = s.fLaunchfullscreen;
    m_fExitFullScreenAtTheEnd = s.fExitFullScreenAtTheEnd;

    m_autoChangeFSModes = s.autoChangeFSMode.modes;
    m_bAutoChangeFSModeEnabled = s.autoChangeFSMode.bEnabled;
    m_bAutoChangeFSModeApplyDefModeAtFSExist = s.autoChangeFSMode.bApplyDefaultModeAtFSExit;
    m_bAutoChangeFSModeRestoreResAfterProgExit = s.autoChangeFSMode.bRestoreResAfterProgExit;
    m_uAutoChangeFullscrResDelay = s.autoChangeFSMode.uDelay;

    CMonitors monitors;

    CString currentMonitorName;
    monitors.GetNearestMonitor(AfxGetMainWnd()).GetName(currentMonitorName);

    CMonitor fullscreenMonitor = monitors.GetMonitor(s.strFullScreenMonitorID, s.strFullScreenMonitorDeviceName);

    m_fullScreenMonitorCtrl.AddString(ResStr(IDS_FULLSCREENMONITOR_CURRENT));
    m_monitorDisplayNames.emplace_back(_T("Current"));
    m_monitorDeviceNames.emplace_back(_T(""));
    m_iFullScreenMonitor = 0;

    for (int i = 0; i < monitors.GetCount(); i++) {
        CMonitor monitor = monitors.GetMonitor(i);

        if (monitor.IsMonitor()) {
            CString displayName, deviceName;
            monitor.GetNames(displayName, deviceName);

            CString str = displayName;

            if (!deviceName.IsEmpty()) {
                str.Append(_T(" - ") + deviceName);
            }

            if (displayName == currentMonitorName) {
                str.AppendFormat(_T(" - [%s]"), ResStr(IDS_FULLSCREENMONITOR_CURRENT).GetString());
            }

            m_fullScreenMonitorCtrl.AddString(str);
            m_monitorDisplayNames.emplace_back(displayName);
            m_monitorDeviceNames.emplace_back(deviceName);

            if (m_iFullScreenMonitor == 0 && monitor == fullscreenMonitor) {
                m_fullScreenMonitorID = displayName;
                m_fullScreenMonitorDeviceName = deviceName;
                m_iFullScreenMonitor = m_fullScreenMonitorCtrl.GetCount() - 1;
                s.strFullScreenMonitorID = m_fullScreenMonitorID;
            }
        }
    }

    if (m_fullScreenMonitorCtrl.GetCount() > 2) {
        GetDlgItem(IDC_COMBO1)->EnableWindow(TRUE);
    } else {
        m_iFullScreenMonitor = 0;
        GetDlgItem(IDC_COMBO1)->EnableWindow(FALSE);
    }

    m_list.SetExtendedStyle(m_list.GetExtendedStyle() /*| LVS_EX_FULLROWSELECT | LVS_EX_DOUBLEBUFFER
                            | LVS_EX_GRIDLINES */ | LVS_EX_BORDERSELECT | LVS_EX_ONECLICKACTIVATE | LVS_EX_CHECKBOXES | LVS_EX_FLATSB);
    m_list.setAdditionalStyles(LVS_EX_GRIDLINES | LVS_EX_FULLROWSELECT | LVS_EX_DOUBLEBUFFER);
    m_list.setAdditionalStyles(WS_CLIPCHILDREN, false);
    m_list.InsertColumn(COL_N, ResStr(IDS_PPAGE_FS_CLN_ON_OFF), LVCFMT_LEFT, 60);
    m_list.InsertColumn(COL_FRAMERATE_START, ResStr(IDS_PPAGE_FS_CLN_FROM_FPS), LVCFMT_RIGHT, 60);
    m_list.InsertColumn(COL_FRAMERATE_STOP, ResStr(IDS_PPAGE_FS_CLN_TO_FPS), LVCFMT_RIGHT, 60);
    m_list.InsertColumn(COL_DISPLAY_MODE, ResStr(IDS_PPAGE_FS_CLN_DISPLAY_MODE), LVCFMT_LEFT, 135);
    m_list.InsertColumn(COL_AUDIO_DELAY, ResStr(IDS_PPAGE_FS_CLN_AUDIO_DELAY), LVCFMT_LEFT, 110);
    m_list.setCheckedColors((COLORREF) - 1, (COLORREF) - 1, CMPCTheme::ContentTextDisabledFGColorFade); //for mpc theme highlighting since nmcustdraw will be ignored on CMPCThemelistctrl

    m_bHideFullscreenControls = s.bHideFullscreenControls;
    m_uHideFullscreenControlsDelay = s.uHideFullscreenControlsDelay;
    m_bHideFullscreenDockedPanels = s.bHideFullscreenDockedPanels;

    auto addHidePolicy = [&](LPCTSTR lpszName, CAppSettings::HideFullscreenControlsPolicy ePolicy) {
        int n = m_hidePolicy.InsertString(-1, lpszName);
        if (n >= 0) {
            VERIFY(m_hidePolicy.SetItemData(n, static_cast<DWORD_PTR>(ePolicy)) != CB_ERR);
            if (ePolicy == s.eHideFullscreenControlsPolicy) {
                VERIFY(m_hidePolicy.SetCurSel(n) == n);
            }
        } else {
            ASSERT(FALSE);
        }
    };
    auto loadString = [&](UINT nID) {
        CString ret;
        VERIFY(ret.LoadString(nID));
        return ret;
    };
    addHidePolicy(loadString(IDS_PPAGEFULLSCREEN_SHOWNEVER), CAppSettings::HideFullscreenControlsPolicy::SHOW_NEVER);
    addHidePolicy(loadString(IDS_PPAGEFULLSCREEN_SHOWMOVED), CAppSettings::HideFullscreenControlsPolicy::SHOW_WHEN_CURSOR_MOVED);
    addHidePolicy(loadString(IDS_PPAGEFULLSCREEN_SHOHHOVERED), CAppSettings::HideFullscreenControlsPolicy::SHOW_WHEN_HOVERED);

    m_delaySpinner.SetRange32(0, 9);

    m_bFullScreenSeparateControls = s.bFullscreenSeparateControls;

    CorrectComboListWidth(m_fullScreenMonitorCtrl);
    CorrectComboListWidth(m_hidePolicy);
    CorrectComboBoxHeaderWidth(GetDlgItem(IDC_CHECK2));
    CorrectComboBoxHeaderWidth(GetDlgItem(IDC_CHECK4));

    ModesUpdate();

    EnableThemedDialogTooltips(this);

    UpdateData(FALSE);

    return TRUE;  // return TRUE unless you set the focus to a control
    // EXCEPTION: OCX Property Pages should return FALSE
}

BOOL CPPageFullscreen::OnApply()
{
    UpdateData();

    CAppSettings& s = AfxGetAppSettings();

    s.strFullScreenMonitorID = m_fullScreenMonitorID;
    s.strFullScreenMonitorDeviceName = m_fullScreenMonitorDeviceName;

    s.fLaunchfullscreen = !!m_bLaunchFullscreen;
    s.fExitFullScreenAtTheEnd = !!m_fExitFullScreenAtTheEnd;

    s.bHideFullscreenControls = !!m_bHideFullscreenControls;
    {
        int n = m_hidePolicy.GetCurSel();
        if (n != CB_ERR) {
            s.eHideFullscreenControlsPolicy =
                static_cast<CAppSettings::HideFullscreenControlsPolicy>(m_hidePolicy.GetItemData(n));
        } else {
            ASSERT(FALSE);
        }
    }
    if (s.eHideFullscreenControlsPolicy == CAppSettings::HideFullscreenControlsPolicy::SHOW_WHEN_CURSOR_MOVED &&
            m_uHideFullscreenControlsDelay > 0 && m_uHideFullscreenControlsDelay < SANE_TIMEOUT_FOR_SHOW_CONTROLS_ON_MOUSE_MOVE) {
        m_uHideFullscreenControlsDelay = SANE_TIMEOUT_FOR_SHOW_CONTROLS_ON_MOUSE_MOVE;
        UpdateData(FALSE);
    }
    s.uHideFullscreenControlsDelay = m_uHideFullscreenControlsDelay;
    s.bHideFullscreenDockedPanels = !!m_bHideFullscreenDockedPanels;

    s.autoChangeFSMode.bEnabled = !!m_bAutoChangeFSModeEnabled;
    s.autoChangeFSMode.bApplyDefaultModeAtFSExit = !!m_bAutoChangeFSModeApplyDefModeAtFSExist;
    s.autoChangeFSMode.bRestoreResAfterProgExit = !!m_bAutoChangeFSModeRestoreResAfterProgExit;
    s.autoChangeFSMode.uDelay = m_uAutoChangeFullscrResDelay;

    m_autoChangeFSModes.clear();
    if (!m_displayModes.empty()) {
        for (int nItem = 0, count = m_list.GetItemCount(); nItem < count; nItem++) {
            double dFRStart, dFRStop;
            int msAudioDelay;
            if (nItem == 0) { // Special case for default mode
                dFRStart = 0.0;
                dFRStop = 0.0;
                msAudioDelay = s.iAudioTimeShift;
            } else {
                dFRStart = _tcstod(m_list.GetItemText(nItem, COL_FRAMERATE_START), nullptr);
                dFRStop = _tcstod(m_list.GetItemText(nItem, COL_FRAMERATE_STOP), nullptr);
                msAudioDelay = _tcstol(m_list.GetItemText(nItem, COL_AUDIO_DELAY), nullptr, 10);
            }

            m_autoChangeFSModes.emplace_back(!!m_list.GetCheck(nItem), dFRStart, dFRStop, msAudioDelay, m_displayModes[m_list.GetItemData(nItem)]);
        }
    }
    s.autoChangeFSMode.modes = m_autoChangeFSModes;

    s.bFullscreenSeparateControls = m_bFullScreenSeparateControls;

    // There is no main frame when the option dialog is displayed stand-alone
    if (CMainFrame* pMainFrame = AfxGetMainFrame()) {
        pMainFrame->UpdateControlState(CMainFrame::UPDATE_CONTROLS_VISIBILITY);
    }

    return __super::OnApply();
}

void CPPageFullscreen::OnUpdateFullScreenMonitor()
{
    int iPos = m_fullScreenMonitorCtrl.GetCurSel();
    if (iPos != CB_ERR) {
        m_fullScreenMonitorID = m_monitorDisplayNames[iPos];
        m_fullScreenMonitorDeviceName = m_monitorDeviceNames[iPos];
        if (AfxGetAppSettings().strFullScreenMonitorID != m_fullScreenMonitorID) {
            m_bAutoChangeFSModeEnabled = false;
        }

        ModesUpdate();
        SetModified();
    } else {
        ASSERT(FALSE);
    }
}

void CPPageFullscreen::OnUpdateHideControls(CCmdUI* pCmdUI)
{
    pCmdUI->Enable(IsDlgButtonChecked(IDC_CHECK4));
}

void CPPageFullscreen::OnHideControlsPolicyChange()
{
    UpdateData(TRUE);
    if (m_hidePolicy.GetItemData(m_hidePolicy.GetCurSel()) ==
            static_cast<int>(CAppSettings::HideFullscreenControlsPolicy::SHOW_WHEN_CURSOR_MOVED) &&
            m_uHideFullscreenControlsDelay > 0 && m_uHideFullscreenControlsDelay < SANE_TIMEOUT_FOR_SHOW_CONTROLS_ON_MOUSE_MOVE) {
        m_uHideFullscreenControlsDelay = SANE_TIMEOUT_FOR_SHOW_CONTROLS_ON_MOUSE_MOVE;
        UpdateData(FALSE);
    }
    SetModified();
}

void CPPageFullscreen::OnUpdateHideDelay(CCmdUI* pCmdUI)
{
    int n = m_hidePolicy.GetCurSel();
    pCmdUI->Enable(IsDlgButtonChecked(IDC_CHECK4) && n != CB_ERR && n != 0);
}

void CPPageFullscreen::OnUpdateAutoChangeFullscreenMode(CCmdUI* pCmdUI)
{
    pCmdUI->Enable(!!IsDlgButtonChecked(IDC_CHECK2));
}

void CPPageFullscreen::OnListCheckChange()
{
    SetModified();
}

void CPPageFullscreen::OnAdd()
{
    POSITION pos = m_list.GetFirstSelectedItemPosition();
    int nItem = pos ? (m_list.GetNextSelectedItem(pos) + 1) : 0;
    if (nItem <= 0) {
        nItem = m_list.GetItemCount();
    }

    CString strItemPos;
    strItemPos.Format(_T("%02d"), nItem);
    VERIFY(m_list.InsertItem(nItem, strItemPos) == nItem);
    VERIFY(m_list.SetItemText(nItem, COL_FRAMERATE_START, _T("1.000")));
    VERIFY(m_list.SetItemText(nItem, COL_FRAMERATE_STOP, _T("1.000")));
    VERIFY(m_list.SetItemText(nItem, COL_DISPLAY_MODE, m_CurrentDisplayModeString));
    VERIFY(m_list.SetItemText(nItem, COL_AUDIO_DELAY, _T("0")));
    VERIFY(m_list.SetItemData(nItem, (DWORD_PTR)m_nCurrentDisplayModeIndex));
    m_list.SetCheck(nItem, FALSE);
    VERIFY(m_list.SetItemState(nItem, LVIS_SELECTED | LVIS_FOCUSED, LVIS_SELECTED | LVIS_FOCUSED));
    m_list.SetFocus();

    SetModified();
}


void CPPageFullscreen::OnRemove()
{
    if (POSITION pos = m_list.GetFirstSelectedItemPosition()) {
        int nItem = m_list.GetNextSelectedItem(pos);
        if (nItem <= 0 || nItem >= m_list.GetItemCount()) {
            return;
        }

        // Remove the item
        VERIFY(m_list.DeleteItem(nItem));
        // Select the next one
        nItem = std::min(nItem, m_list.GetItemCount() - 1);
        m_list.SetSelectionMark(nItem);
        VERIFY(m_list.SetItemState(nItem, LVIS_SELECTED | LVIS_FOCUSED, LVIS_SELECTED | LVIS_FOCUSED));
        m_list.SetFocus();
        // Update all items that were after the removed one
        for (int count = m_list.GetItemCount(); nItem < count; nItem++) {
            CString strItemPos;
            strItemPos.Format(_T("%02d"), nItem);
            VERIFY(m_list.SetItemText(nItem, COL_N, strItemPos));
        }

        SetModified();
    }
}

void CPPageFullscreen::OnUpdateRemove(CCmdUI* pCmdUI)
{
    POSITION pos = m_list.GetFirstSelectedItemPosition();
    int nItem = pos ? m_list.GetNextSelectedItem(pos) : -1;
    pCmdUI->Enable(!!IsDlgButtonChecked(IDC_CHECK2) && nItem > 0);
}

void CPPageFullscreen::OnMoveUp()
{
    if (POSITION pos = m_list.GetFirstSelectedItemPosition()) {
        int nItem = m_list.GetNextSelectedItem(pos);
        if (nItem <= 1 || nItem >= m_list.GetItemCount()) {
            return;
        }

        // Move the item up
        CString strFRStart = m_list.GetItemText(nItem, COL_FRAMERATE_START);
        CString strFRStop = m_list.GetItemText(nItem, COL_FRAMERATE_STOP);
        CString strDM = m_list.GetItemText(nItem, COL_DISPLAY_MODE);
        CString strAudioDelay = m_list.GetItemText(nItem, COL_AUDIO_DELAY);
        BOOL nCheckCur = m_list.GetCheck(nItem);
        DWORD_PTR data = m_list.GetItemData(nItem);
        VERIFY(m_list.DeleteItem(nItem));

        nItem--;
        CString strItemPos;
        strItemPos.Format(_T("%02d"), nItem);
        VERIFY(m_list.InsertItem(nItem, strItemPos) == nItem);
        VERIFY(m_list.SetItemText(nItem, COL_FRAMERATE_START, strFRStart));
        VERIFY(m_list.SetItemText(nItem, COL_FRAMERATE_STOP, strFRStop));
        VERIFY(m_list.SetItemText(nItem, COL_DISPLAY_MODE, strDM));
        VERIFY(m_list.SetItemText(nItem, COL_AUDIO_DELAY, strAudioDelay));
        VERIFY(m_list.SetItemData(nItem, data));
        m_list.SetCheck(nItem, nCheckCur);
        m_list.SetSelectionMark(nItem);
        m_list.SetItemState(nItem, LVIS_SELECTED | LVIS_FOCUSED, LVIS_SELECTED | LVIS_FOCUSED);
        m_list.SetFocus();

        // Update the item that got moved down if any
        nItem++;
        strItemPos.Format(_T("%02d"), nItem);
        VERIFY(m_list.SetItemText(nItem, COL_N, strItemPos));

        SetModified();
    }
}

void CPPageFullscreen::OnUpdateUp(CCmdUI* pCmdUI)
{
    POSITION pos = m_list.GetFirstSelectedItemPosition();
    int nItem = pos ? m_list.GetNextSelectedItem(pos) : -1;
    pCmdUI->Enable(!!IsDlgButtonChecked(IDC_CHECK2) && nItem > 1);
}

void CPPageFullscreen::OnMoveDown()
{
    if (POSITION pos = m_list.GetFirstSelectedItemPosition()) {
        int nItem = m_list.GetNextSelectedItem(pos);
        if (nItem <= 0 || nItem >= m_list.GetItemCount() - 1) {
            return;
        }

        // Move the item down
        CString strFRStart = m_list.GetItemText(nItem, COL_FRAMERATE_START);
        CString strFRStop = m_list.GetItemText(nItem, COL_FRAMERATE_STOP);
        CString strDM = m_list.GetItemText(nItem, COL_DISPLAY_MODE);
        CString strAudioDelay = m_list.GetItemText(nItem, COL_AUDIO_DELAY);
        BOOL nCheckCur = m_list.GetCheck(nItem);
        DWORD_PTR data = m_list.GetItemData(nItem);
        VERIFY(m_list.DeleteItem(nItem));

        nItem++;
        CString strItemPos;
        strItemPos.Format(_T("%02d"), nItem);
        VERIFY(m_list.InsertItem(nItem, strItemPos) == nItem);
        VERIFY(m_list.SetItemText(nItem, COL_FRAMERATE_START, strFRStart));
        VERIFY(m_list.SetItemText(nItem, COL_FRAMERATE_STOP, strFRStop));
        VERIFY(m_list.SetItemText(nItem, COL_DISPLAY_MODE, strDM));
        VERIFY(m_list.SetItemText(nItem, COL_AUDIO_DELAY, strAudioDelay));
        VERIFY(m_list.SetItemData(nItem, data));
        m_list.SetCheck(nItem, nCheckCur);
        m_list.SetSelectionMark(nItem);
        m_list.SetItemState(nItem, LVIS_SELECTED | LVIS_FOCUSED, LVIS_SELECTED | LVIS_FOCUSED);
        m_list.SetFocus();

        // Update the item that got moved up if any
        nItem--;
        strItemPos.Format(_T("%02d"), nItem);
        VERIFY(m_list.SetItemText(nItem, COL_N, strItemPos));

        SetModified();
    }
}

void CPPageFullscreen::OnUpdateDown(CCmdUI* pCmdUI)
{
    POSITION pos = m_list.GetFirstSelectedItemPosition();
    int nItem = pos ? m_list.GetNextSelectedItem(pos) : -1;
    pCmdUI->Enable(!!IsDlgButtonChecked(IDC_CHECK2) && nItem > 0 && nItem < m_list.GetItemCount() - 1);

}

void CPPageFullscreen::OnListBeginEdit(NMHDR* pNMHDR, LRESULT* pResult)
{
    LV_DISPINFO* pDispInfo = (LV_DISPINFO*)pNMHDR;
    LV_ITEM* pItem = &pDispInfo->item;

    if (pItem->iItem < 0) {
        *pResult = FALSE;
    } else {
        *pResult = TRUE;
    }
}

void CPPageFullscreen::OnListDoEdit(NMHDR* pNMHDR, LRESULT* pResult)
{
    LV_DISPINFO* pDispInfo = (LV_DISPINFO*)pNMHDR;
    LV_ITEM* pItem = &pDispInfo->item;

    *pResult = FALSE;
    if (pItem->iItem < 0) {
        return;
    }

    switch (pItem->iSubItem) {
        case COL_DISPLAY_MODE:
            m_list.ShowInPlaceComboBox(pItem->iItem, pItem->iSubItem, m_displayModesString, (int)m_list.GetItemData(pItem->iItem));
            break;
        case COL_FRAMERATE_START:
        case COL_FRAMERATE_STOP:
            if (pItem->iItem != 0) {
                m_list.ShowInPlaceFloatEdit(pItem->iItem, pItem->iSubItem);
            }
            break;
        case COL_AUDIO_DELAY:
            if (pItem->iItem != 0) {
                m_list.ShowInPlaceEdit(pItem->iItem, pItem->iSubItem);
            }
            break;
    }

    m_list.RedrawWindow();
    *pResult = TRUE;
}

void CPPageFullscreen::OnListEndEdit(NMHDR* pNMHDR, LRESULT* pResult)
{
    LV_DISPINFO* pDispInfo = (LV_DISPINFO*)pNMHDR;
    LV_ITEM* pItem = &pDispInfo->item;

    *pResult = FALSE;
    if (!m_list.m_fInPlaceDirty) {
        return;
    }
    if (pItem->iItem < 0) {
        return;
    }

    switch (pItem->iSubItem) {
        case COL_DISPLAY_MODE:
            if (pItem->lParam >= 0) {
                VERIFY(m_list.SetItemData(pItem->iItem, (DWORD_PTR)(int)pItem->lParam));
                m_list.SetItemText(pItem->iItem, pItem->iSubItem, pItem->pszText);
            }
            break;
        case COL_FRAMERATE_START:
        case COL_FRAMERATE_STOP:
            if (pItem->pszText) {
                CString str = pItem->pszText;
                double dFR = std::min(std::max(_tcstod(str, nullptr), 1.0), 125.999);
                str.Format(_T("%.3f"), dFR);
                m_list.SetItemText(pItem->iItem, pItem->iSubItem, str);
            }
            break;
        case COL_AUDIO_DELAY:
            if (pItem->pszText) {
                TCHAR buffer[11];
                int msAudioDelay = _tcstol(pItem->pszText, nullptr, 10);
                _itot_s(msAudioDelay, buffer, 10);
                m_list.SetItemText(pItem->iItem, pItem->iSubItem, buffer);
            }
            break;
    }

    *pResult = TRUE;

    SetModified();
}

void CPPageFullscreen::OnListCustomDraw(NMHDR* pNMHDR, LRESULT* pResult)
{
    *pResult = CDRF_DODEFAULT;

    //this custom draw is used only in classic mode
    if (!AppNeedsThemedControls()) {
        NMLVCUSTOMDRAW* pLVCD = reinterpret_cast<NMLVCUSTOMDRAW*>(pNMHDR);

        if (CDDS_PREPAINT == pLVCD->nmcd.dwDrawStage) {
            *pResult = CDRF_NOTIFYITEMDRAW;
        } else if (CDDS_ITEMPREPAINT == pLVCD->nmcd.dwDrawStage) {
            *pResult = CDRF_NOTIFYSUBITEMDRAW;
        } else if ((CDDS_ITEMPREPAINT | CDDS_SUBITEM) == pLVCD->nmcd.dwDrawStage) {
            COLORREF crText;
            if (m_list.GetCheck((int)pLVCD->nmcd.dwItemSpec)) {
                crText = RGB(0, 0, 0);
            } else {
                crText = RGB(128, 128, 128);
            }
            pLVCD->clrText = crText;
            *pResult = CDRF_DODEFAULT;
        }
    }
}

BOOL CPPageFullscreen::OnToolTipNotify(UINT id, NMHDR* pNMH, LRESULT* pResult)
{
    LPTOOLTIPTEXT pTTT = reinterpret_cast<LPTOOLTIPTEXT>(pNMH);

    UINT_PTR nID = pNMH->idFrom;
    if (pTTT->uFlags & TTF_IDISHWND) {
        nID = ::GetDlgCtrlID((HWND)nID);
    }

    BOOL bRet = FALSE;

    switch (nID) {
        case IDC_COMBO2:
            bRet = FillComboToolTip(m_hidePolicy, pTTT);
            break;
        case IDC_CHECK7:
            bRet = true;
            pTTT->lpszText = (LPTSTR)(LPCTSTR)m_FullScreenSeparateControlsText;
            break;
    }

    if (bRet) {
        PlaceThemedDialogTooltip(nID);
    }

    return bRet;
}
