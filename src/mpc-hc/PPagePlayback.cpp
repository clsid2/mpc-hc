/*
 * (C) 2003-2006 Gabest
 * (C) 2006-2016 see Authors.txt
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
#include "PPagePlayback.h"
#include "Monitors.h"
#include "MultiMonitor.h"

// CPPagePlayback dialog

IMPLEMENT_DYNAMIC(CPPagePlayback, CMPCThemePPageBase)
CPPagePlayback::CPPagePlayback()
    : CMPCThemePPageBase(CPPagePlayback::IDD, CPPagePlayback::IDD)
    , m_oldVolume(0)
    , m_nVolume(0)
    , m_nBalance(0)
    , m_nVolumeStep(0)
    , m_nSpeedStep(0)
    , m_iLoopForever(0)
    , m_iLoopMode(0)
    , m_nLoops(0)
    , m_iAfterPlayback(0)
    , m_iZoomLevel(1)
    , verticalAlignVideo(0)
    , m_iRememberZoomLevel(FALSE)
    , m_nAutoFitFactorMin(DEF_MIN_AUTOFIT_SCALE_FACTOR)
    , m_nAutoFitFactorMax(DEF_MAX_AUTOFIT_SCALE_FACTOR)
    , m_fAutoloadAudio(FALSE)
    , m_fEnableWorkerThreadForOpening(FALSE)
    , m_fReportFailedPins(FALSE)
    , m_fAllowOverridingExternalSplitterChoice(FALSE)
    , m_bInitDialogComplete(false)
{
}

CPPagePlayback::~CPPagePlayback()
{
}

void CPPagePlayback::DoDataExchange(CDataExchange* pDX)
{
    __super::DoDataExchange(pDX);
    DDX_Control(pDX, IDC_SLIDER1, m_volumectrl);
    DDX_Control(pDX, IDC_SLIDER2, m_balancectrl);
    DDX_Control(pDX, IDC_COMBO1, m_zoomlevelctrl);
    DDX_Control(pDX, IDC_COMBO4, verticalAlignVideoCombo);
    DDX_Control(pDX, IDC_COMBO2, m_afterPlayback);
    DDX_Slider(pDX, IDC_SLIDER1, m_nVolume);
    DDX_Slider(pDX, IDC_SLIDER2, m_nBalance);
    DDX_Radio(pDX, IDC_RADIO1, m_iLoopForever);
    DDX_Text(pDX, IDC_EDIT1, m_nLoops);
    DDX_CBIndex(pDX, IDC_COMBO2, m_iAfterPlayback);
    DDX_CBIndex(pDX, IDC_COMBO1, m_iZoomLevel);
    DDX_CBIndex(pDX, IDC_COMBO4, verticalAlignVideo);
    DDX_Check(pDX, IDC_CHECK5, m_iRememberZoomLevel);
    DDX_Check(pDX, IDC_CHECK2, m_fAutoloadAudio);
    DDX_Check(pDX, IDC_CHECK7, m_fEnableWorkerThreadForOpening);
    DDX_Check(pDX, IDC_CHECK6, m_fReportFailedPins);
    DDX_Text(pDX, IDC_EDIT2, m_subtitlesLanguageOrder);
    DDX_Text(pDX, IDC_EDIT3, m_audiosLanguageOrder);
    DDX_Check(pDX, IDC_CHECK4, m_fAllowOverridingExternalSplitterChoice);
    DDX_Text(pDX, IDC_VOLUMESTEP, m_nVolumeStep);
    DDX_Text(pDX, IDC_EDIT4, m_nAutoFitFactorMin);
    DDX_Text(pDX, IDC_EDIT5, m_nAutoFitFactorMax);
    DDX_Control(pDX, IDC_VOLUMESTEP_SPIN, m_VolumeStepCtrl);
    DDX_Control(pDX, IDC_SPEEDSTEP_SPIN, m_SpeedStepCtrl);
    DDX_Control(pDX, IDC_SPIN1, m_AutoFitFactorMinCtrl);
    DDX_Control(pDX, IDC_SPIN2, m_AutoFitFactorMaxCtrl);
    DDX_Control(pDX, IDC_COMBO3, m_LoopMode);
    DDX_CBIndex(pDX, IDC_COMBO3, m_iLoopMode);
}

BEGIN_MESSAGE_MAP(CPPagePlayback, CMPCThemePPageBase)
    ON_WM_HSCROLL()
    ON_CONTROL_RANGE(BN_CLICKED, IDC_RADIO1, IDC_RADIO2, OnBnClickedRadio12)
    ON_EN_CHANGE(IDC_EDIT4, OnChangeFitFactorMin)
    ON_EN_CHANGE(IDC_EDIT5, OnChangeFitFactorMax)
    ON_UPDATE_COMMAND_UI(IDC_EDIT1, OnUpdateLoopNum)
    ON_UPDATE_COMMAND_UI(IDC_STATIC1, OnUpdateLoopNum)
    ON_UPDATE_COMMAND_UI(IDC_COMBO1, OnUpdateAutoZoomCombo)
    ON_UPDATE_COMMAND_UI(IDC_COMBO2, OnUpdateAfterPlayback)
    ON_UPDATE_COMMAND_UI(IDC_SPEEDSTEP_SPIN, OnUpdateSpeedStep)
    ON_STN_DBLCLK(IDC_STATIC_BALANCE, OnBalanceTextDblClk)
    ON_NOTIFY_EX(TTN_NEEDTEXT, 0, OnToolTipNotify)
END_MESSAGE_MAP()


// CPPagePlayback message handlers

BOOL CPPagePlayback::OnInitDialog()
{
    __super::OnInitDialog();

    SetHandCursor(m_hWnd, IDC_COMBO1);

    const CAppSettings& s = AfxGetAppSettings();

    m_volumectrl.SetRange(0, 100);
    m_volumectrl.SetTicFreq(10);
    m_balancectrl.SetRange(-100, 100);
    m_balancectrl.SetTicFreq(20);
    m_balancectrl.SetLockToZero();
    m_nVolume = m_oldVolume = s.nVolume;
    m_nBalance = s.nBalance;
    m_nVolumeStep = s.nVolumeStep;
    m_VolumeStepCtrl.SetRange32(1, 25);
    m_nSpeedStep = s.nSpeedStep;
    m_SpeedStepCtrl.SetPos32(m_nSpeedStep);
    m_SpeedStepCtrl.SetRange32(0, 75);
    m_iLoopForever = s.fLoopForever ? 1 : 0;
    m_iLoopMode = static_cast<int>(s.eLoopMode);
    m_nLoops = s.nLoops;
    m_iAfterPlayback = static_cast<int>(s.eAfterPlayback);
    m_iZoomLevel = s.iZoomLevel + 1;
    verticalAlignVideo = static_cast<int>(s.iVerticalAlignVideo);
    m_iRememberZoomLevel = s.fRememberZoomLevel;
    m_nAutoFitFactorMin = s.nAutoFitFactorMin;
    m_AutoFitFactorMinCtrl.SetPos32(m_nAutoFitFactorMin);
    m_AutoFitFactorMinCtrl.SetRange32(MIN_AUTOFIT_SCALE_FACTOR, m_nAutoFitFactorMax);
    m_nAutoFitFactorMax = s.nAutoFitFactorMax;
    m_AutoFitFactorMaxCtrl.SetPos32(m_nAutoFitFactorMax);
    m_AutoFitFactorMaxCtrl.SetRange32(m_nAutoFitFactorMin, MAX_AUTOFIT_SCALE_FACTOR);
    m_fAutoloadAudio = s.fAutoloadAudio;
    m_fEnableWorkerThreadForOpening = s.fEnableWorkerThreadForOpening;
    m_fReportFailedPins = s.fReportFailedPins;
    m_subtitlesLanguageOrder = s.strSubtitlesLanguageOrder;
    m_audiosLanguageOrder = s.strAudiosLanguageOrder;
    m_fAllowOverridingExternalSplitterChoice = s.bAllowOverridingExternalSplitterChoice;

    m_zoomlevelctrl.AddString(ResStr(IDS_ZOOM_25));
    m_zoomlevelctrl.AddString(ResStr(IDS_ZOOM_50));
    m_zoomlevelctrl.AddString(ResStr(IDS_ZOOM_100));
    m_zoomlevelctrl.AddString(ResStr(IDS_ZOOM_200));
    m_zoomlevelctrl.AddString(ResStr(IDS_ZOOM_AUTOFIT));
    //m_zoomlevelctrl.AddString(ResStr(IDS_ZOOM_AUTOFIT_LARGER));
    CorrectComboListWidth(m_zoomlevelctrl);

    verticalAlignVideoCombo.AddString(ResStr(IDS_VERTICAL_ALIGN_VIDEO_MIDDLE));
    verticalAlignVideoCombo.AddString(ResStr(IDS_VERTICAL_ALIGN_VIDEO_TOP));
    verticalAlignVideoCombo.AddString(ResStr(IDS_VERTICAL_ALIGN_VIDEO_BOTTOM));
    CorrectComboListWidth(verticalAlignVideoCombo);

    m_afterPlayback.AddString(ResStr(IDS_AFTER_PLAYBACK_DO_NOTHING));
    m_afterPlayback.AddString(ResStr(IDS_AFTER_PLAYBACK_PLAY_NEXT));
    m_afterPlayback.AddString(ResStr(IDS_AFTER_PLAYBACK_REWIND));
    m_afterPlayback.AddString(ResStr(IDS_AFTER_PLAYBACK_MONITOROFF));
    m_afterPlayback.AddString(ResStr(IDS_AFTER_PLAYBACK_CLOSE));
    m_afterPlayback.AddString(ResStr(IDS_AFTER_PLAYBACK_EXIT));
    CorrectComboListWidth(m_afterPlayback);

    m_LoopMode.AddString(ResStr(IDS_PLAY_LOOPMODE_FILE));
    m_LoopMode.AddString(ResStr(IDS_PLAY_LOOPMODE_PLAYLIST));
    CorrectComboListWidth(m_LoopMode);

    // set the spinner acceleration value
    UDACCEL accel = { 0, 5 };
    m_SpeedStepCtrl.SetAccel(1, &accel);

    AdjustDynamicWidgets();
    EnableThemedDialogTooltips(this);
    CreateToolTip();

    m_wndToolTip.AddTool(GetDlgItem(IDC_EDIT2), ResStr(IDS_LANG_PREF_EXAMPLE));
    m_wndToolTip.AddTool(GetDlgItem(IDC_EDIT3), ResStr(IDS_LANG_PREF_EXAMPLE));
    m_wndToolTip.AddTool(GetDlgItem(IDC_CHECK4), ResStr(IDS_OVERRIDE_EXT_SPLITTER_CHOICE));

    UpdateData(FALSE);
    m_bInitDialogComplete = true; //UpdateData is dangerous to call before this, some events fire as soon as control is created (ON_EN_CHANGE)
    return TRUE;  // return TRUE unless you set the focus to a control
    // EXCEPTION: OCX Property Pages should return FALSE
}

BOOL CPPagePlayback::OnApply()
{
    UpdateData();

    CAppSettings& s = AfxGetAppSettings();

    s.nVolume = m_oldVolume = m_nVolume;
    s.nBalance = m_nBalance;
    s.nVolumeStep = std::min(std::max(m_nVolumeStep, 1), 100);
    s.nSpeedStep = m_nSpeedStep;
    s.fLoopForever = !!m_iLoopForever;
    s.eLoopMode = static_cast<CAppSettings::LoopMode>(m_iLoopMode);
    s.nLoops = m_nLoops;
    s.eAfterPlayback = static_cast<CAppSettings::AfterPlayback>(m_iAfterPlayback);
    s.iZoomLevel = m_iZoomLevel - 1;
    auto oldVerticalAlignVideo = s.iVerticalAlignVideo;
    s.iVerticalAlignVideo = static_cast<CAppSettings::verticalAlignVideoType>(verticalAlignVideo);
    s.fRememberZoomLevel = !!m_iRememberZoomLevel;
    s.nAutoFitFactorMin = m_nAutoFitFactorMin = std::min(std::max(m_nAutoFitFactorMin, 25), 100);
    s.nAutoFitFactorMax = m_nAutoFitFactorMax = std::min(std::max(m_nAutoFitFactorMax, 25), 100);
    s.fAutoloadAudio = !!m_fAutoloadAudio;
    s.fEnableWorkerThreadForOpening = !!m_fEnableWorkerThreadForOpening;
    s.fReportFailedPins = !!m_fReportFailedPins;
    s.strSubtitlesLanguageOrder = m_subtitlesLanguageOrder;
    s.strAudiosLanguageOrder = m_audiosLanguageOrder;
    s.bAllowOverridingExternalSplitterChoice = !!m_fAllowOverridingExternalSplitterChoice;

    // There is no main frame when the option dialog is displayed stand-alone
    if (CMainFrame* pMainFrame = AfxGetMainFrame()) {
        pMainFrame->UpdateControlState(CMainFrame::UPDATE_VOLUME_STEP);
        if (s.iVerticalAlignVideo != oldVerticalAlignVideo) {
            pMainFrame->MoveVideoWindow();
        }
    }

    return __super::OnApply();
}

void CPPagePlayback::OnHScroll(UINT nSBCode, UINT nPos, CScrollBar* pScrollBar)
{
    if (*pScrollBar == m_volumectrl) {
        UpdateData();
        ((CMainFrame*)GetParentFrame())->m_wndToolBar.Volume = m_nVolume; // nice shortcut...
    } else if (*pScrollBar == m_balancectrl) {
        UpdateData();
        ((CMainFrame*)GetParentFrame())->SetBalance(m_nBalance); // see prev note...
    }
    RedrawDialogTooltipIfVisible(); //if the scroll is caused by a wheel or arrows, the default tooltip may be active due to hover, in which case, we want to update

    SetModified();

    __super::OnHScroll(nSBCode, nPos, pScrollBar);
}

void CPPagePlayback::OnBnClickedRadio12(UINT nID)
{
    SetModified();
}

void CPPagePlayback::OnUpdateLoopNum(CCmdUI* pCmdUI)
{
    pCmdUI->Enable(!!IsDlgButtonChecked(IDC_RADIO1));
}

void CPPagePlayback::OnUpdateAutoZoomCombo(CCmdUI* pCmdUI)
{
    pCmdUI->Enable(!!IsDlgButtonChecked(IDC_CHECK5));
}

void CPPagePlayback::OnUpdateAfterPlayback(CCmdUI* pCmdUI)
{
    pCmdUI->Enable(!IsDlgButtonChecked(IDC_RADIO2));
}

void CPPagePlayback::OnUpdateSpeedStep(CCmdUI* pCmdUI)
{
    // if there is an error retrieving the position, assume the speedstep is set to auto
    BOOL bError = FALSE;
    int iPos = m_SpeedStepCtrl.GetPos32(&bError);
    m_nSpeedStep = bError ? 0 : iPos;

    // set the edit box text to auto if the position is zero
    if (!bError && iPos == 0) {
        SetDlgItemText(IDC_SPEEDSTEP, ResStr(IDS_SPEEDSTEP_AUTO));
    }
}

void CPPagePlayback::OnBalanceTextDblClk()
{
    // double click on text "Balance" resets the balance to zero
    m_nBalance = 0;
    m_balancectrl.SetPos(m_nBalance);
    ((CMainFrame*)GetParentFrame())->SetBalance(m_nBalance);
    SetModified();
}

BOOL CPPagePlayback::OnToolTipNotify(UINT id, NMHDR* pNMHDR, LRESULT* pResult)
{
    LPTOOLTIPTEXT pTTT = reinterpret_cast<LPTOOLTIPTEXT>(pNMHDR);

    UINT_PTR nID = pNMHDR->idFrom;
    if (pTTT->uFlags & TTF_IDISHWND) {
        nID = ::GetDlgCtrlID((HWND)nID);
    }

    BOOL bRet = FALSE;

    if (nID == IDC_SLIDER1 || nID == IDC_SLIDER2) {
        bRet = TRUE;

        CString strTipText;
        if (nID == IDC_SLIDER1) {
            strTipText.Format(IDS_VOLUME, m_nVolume);
        } else if (nID == IDC_SLIDER2) {
            if (m_nBalance > 0) {
                strTipText.Format(IDS_BALANCE_R, m_nBalance);
            } else if (m_nBalance < 0) {
                strTipText.Format(IDS_BALANCE_L, -m_nBalance);
            } else { //if (m_nBalance == 0)
                strTipText.LoadString(IDS_BALANCE);
            }
        }

        _tcscpy_s(pTTT->szText, strTipText.Left(_countof(pTTT->szText) - 1));
    } else if (nID == IDC_COMBO1) {
        bRet = FillComboToolTip(m_zoomlevelctrl, pTTT);
    } else if (nID == IDC_COMBO2) {
        bRet = FillComboToolTip(m_afterPlayback, pTTT);
    } else if (nID == IDC_COMBO4) {
        bRet = FillComboToolTip(verticalAlignVideoCombo, pTTT);
    }

    if (bRet) {
        PlaceThemedDialogTooltip(nID);
    }

    return bRet;
}

void CPPagePlayback::OnCancel()
{
    const CAppSettings& s = AfxGetAppSettings();

    if (m_nVolume != m_oldVolume) {
        ((CMainFrame*)GetParentFrame())->m_wndToolBar.Volume = m_oldVolume;    //not very nice solution
    }
    if (m_nBalance != s.nBalance) {
        ((CMainFrame*)GetParentFrame())->SetBalance(s.nBalance);
    }

    __super::OnCancel();
}

void CPPagePlayback::OnChangeFitFactorMin() {
    if (m_bInitDialogComplete) { //DO NOT REMOVE THIS! EN_CHANGE fired before all controls created
        UpdateData(true);
        if (m_AutoFitFactorMaxCtrl.m_hWnd) {
            m_AutoFitFactorMaxCtrl.SetRange32(m_nAutoFitFactorMin, MAX_AUTOFIT_SCALE_FACTOR);
        }
        SetModified();
    }
}

void CPPagePlayback::OnChangeFitFactorMax() {
    if (m_bInitDialogComplete) { //DO NOT REMOVE THIS! EN_CHANGE fired before all controls created
        UpdateData(true);
        if (m_AutoFitFactorMinCtrl.m_hWnd) {
            m_AutoFitFactorMinCtrl.SetRange32(MIN_AUTOFIT_SCALE_FACTOR, m_nAutoFitFactorMax);
        }
        SetModified();
    }
}

void CPPagePlayback::AdjustDynamicWidgets() {
    AdjustDynamicWidgetPair(this, IDC_STATIC5, IDC_VOLUMESTEP);
    AdjustDynamicWidgetPair(this, IDC_STATIC6, IDC_SPEEDSTEP);
    AdjustDynamicWidgetPair(this, IDC_STATIC7, IDC_COMBO3);
    AdjustDynamicWidgetPair(this, IDC_CHECK5, IDC_COMBO1);
    AdjustDynamicWidgetPair(this, IDC_STATIC4, IDC_COMBO4);
    AdjustDynamicWidgetPair(this, IDC_STATIC8, IDC_EDIT2);
    AdjustDynamicWidgetPair(this, IDC_STATIC9, IDC_EDIT3);
}
