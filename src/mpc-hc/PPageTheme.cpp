/*
 * (C) 2003-2006 Gabest
 * (C) 2006-2014 see Authors.txt
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
#include "PPageTheme.h"
#include "CMPCTheme.h"
#include "CMPCThemeUtil.h"
#include "ColorProfileUtil.h"
#include "resource.h"
#include "Translations.h"
#include "WinAPIUtils.h"

// CPPageTheme dialog

IMPLEMENT_DYNAMIC(CPPageTheme, CMPCThemePPageBase)
CPPageTheme::CPPageTheme()
    : CMPCThemePPageBase(CPPageTheme::IDD, CPPageTheme::IDD)
    , m_bUseModernTheme(FALSE)
    , m_iModernSeekbarHeight(DEF_MODERN_SEEKBAR_HEIGHT)
    , m_iThemeMode(0)
    , m_nPosLangEnglish(0)
    , m_nOSDSize(0)
    , m_fShowChapters(TRUE)
    , m_bShowPreview(FALSE)
    , m_bShowTime(TRUE)
    , m_iSeekPreviewSize(15)
    , m_fShowOSD(FALSE)
    , m_fShowCurrentTimeInOSD(FALSE)
    , m_bShowVideoInfoInStatusbar(FALSE)
    , m_bShowAudioFormatInStatusbar(FALSE)
    , m_bShowLangInStatusbar(FALSE)
    , m_bShowFPSInStatusbar(FALSE)
    , m_bShowABMarksInStatusbar(FALSE)
    , m_fSnapToDesktopEdges(FALSE)
    , m_fLimitWindowProportions(TRUE)
    , m_bHideWindowedControls(FALSE)
    , m_bUseEnhancedTaskBar(TRUE)
    , m_bUseSMTC(FALSE)
{
    EventRouter::EventSelection fires;
    fires.insert(MpcEvent::CHANGING_UI_LANGUAGE);
    GetEventd().Connect(m_eventc, fires);
}

CPPageTheme::~CPPageTheme()
{
}

void CPPageTheme::DoDataExchange(CDataExchange* pDX)
{
    __super::DoDataExchange(pDX);
    DDX_Check(pDX, IDC_CHECK1, m_bUseModernTheme);

    DDX_Text(pDX, IDC_MODERNSEEKBARHEIGHT, m_iModernSeekbarHeight);
    DDX_Control(pDX, IDC_MODERNSEEKBARHEIGHT, m_ModernSeekbarHeightEdit);
    DDV_MinMaxInt(pDX, m_iModernSeekbarHeight, MIN_MODERN_SEEKBAR_HEIGHT, MAX_MODERN_SEEKBAR_HEIGHT);
    DDX_Control(pDX, IDC_MODERNSEEKBARHEIGHT_SPIN, m_ModernSeekbarHeightCtrl);

    DDX_Control(pDX, IDC_COMBO1, m_ThemeMode);
    DDX_CBIndex(pDX, IDC_COMBO1, m_iThemeMode);
    DDX_Control(pDX, IDC_COMBO2, m_langsComboBox);

    DDX_Control(pDX, IDC_COMBO5, m_FontType);
    DDX_Control(pDX, IDC_COMBO6, m_FontSize);

    DDX_Text(pDX, IDC_EDIT4, m_iSeekPreviewSize);
    DDX_Control(pDX, IDC_EDIT4, m_SeekPreviewSizeEdit);
    DDX_Control(pDX, IDC_SPIN2, m_SeekPreviewSizeCtrl);
    DDX_Check(pDX, IDC_CHECK2, m_fShowChapters);


    DDX_Control(pDX, IDC_COMBO3, m_HoverPosition);
    DDX_Check(pDX, IDC_SHOW_OSD, m_fShowOSD);
    DDX_Check(pDX, IDC_CHECK13, m_fShowCurrentTimeInOSD);
    DDX_Check(pDX, IDC_CHECK4, m_bShowVideoInfoInStatusbar);
    DDX_Check(pDX, IDC_CHECK12, m_bShowAudioFormatInStatusbar);
    DDX_Check(pDX, IDC_CHECK3, m_bShowLangInStatusbar);
    DDX_Check(pDX, IDC_CHECK5, m_bShowFPSInStatusbar);
    DDX_Check(pDX, IDC_CHECK6, m_bShowABMarksInStatusbar);
    DDX_Check(pDX, IDC_SEEK_PREVIEW, m_bShowPreview);
    DDX_Check(pDX, IDC_CHECK14, m_bShowTime);

    DDX_Control(pDX, IDC_CHECK14, m_ShowTimeCtrl);

    DDX_Check(pDX, IDC_CHECK7, m_fLimitWindowProportions);
    DDX_Check(pDX, IDC_CHECK9, m_fSnapToDesktopEdges);
    DDX_Check(pDX, IDC_CHECK10, m_bHideWindowedControls);
    DDX_Check(pDX, IDC_CHECK_ENHANCED_TASKBAR, m_bUseEnhancedTaskBar);
    DDX_Check(pDX, IDC_CHECK11, m_bUseSMTC);
}


BEGIN_MESSAGE_MAP(CPPageTheme, CMPCThemePPageBase)
    ON_BN_CLICKED(IDC_SEEK_PREVIEW, OnPreviewClicked)
    ON_BN_CLICKED(IDC_CHECK14, OnTimeClicked)
    ON_BN_CLICKED(IDC_CHECK1, OnThemeClicked)
    ON_NOTIFY_EX(TTN_NEEDTEXT, 0, OnToolTipNotify)
    ON_CBN_SELCHANGE(IDC_COMBO5, OnChngOSDCombo)
    ON_CBN_SELCHANGE(IDC_COMBO6, OnChngOSDCombo)
END_MESSAGE_MAP()

int CALLBACK EnumFontProc(ENUMLOGFONT FAR* lf, NEWTEXTMETRIC FAR* tm, int FontType, LPARAM dwData) {
    CAtlArray<CString>* fntl = (CAtlArray<CString>*)dwData;
    if (FontType == TRUETYPE_FONTTYPE) {
        fntl->Add(lf->elfFullName);
    }
    return 1; /* Continue the enumeration */
}
// CPPageTheme message handlers

BOOL CPPageTheme::OnInitDialog()
{
    __super::OnInitDialog();

    const CAppSettings& s = AfxGetAppSettings();
    m_bUseModernTheme = s.bMPCTheme;

    ThemeEnableSubControls(m_bUseModernTheme);

    m_ModernSeekbarHeightCtrl.SetRange32(MIN_MODERN_SEEKBAR_HEIGHT, MAX_MODERN_SEEKBAR_HEIGHT);
    m_iModernSeekbarHeight = s.iModernSeekbarHeight;

    m_iThemeMode = static_cast<int>(s.eModernThemeMode);

    m_ThemeMode.AddString(ResStr(IDS_THEMEMODE_DARK));
    m_ThemeMode.AddString(ResStr(IDS_THEMEMODE_LIGHT));
    m_ThemeMode.AddString(ResStr(IDS_THEMEMODE_WINDOWS));

    for (auto& lr : Translations::GetAvailableLanguageResources()) {
        int pos = m_langsComboBox.AddString(lr.name);
        if (pos != CB_ERR) {
            m_langsComboBox.SetItemData(pos, lr.localeID);
            if (lr.localeID == s.language) {
                m_langsComboBox.SetCurSel(pos);
            }
            if (lr.localeID == 0) {
                m_nPosLangEnglish = pos;
            }
        } else {
            ASSERT(FALSE);
        }
    }

    m_HoverPosition.AddString(ResStr(IDS_TIME_TOOLTIP_ABOVE));
    m_HoverPosition.AddString(ResStr(IDS_TIME_TOOLTIP_BELOW));
    m_HoverPosition.SetCurSel(s.nHoverPosition);

    m_nOSDSize = s.nOSDSize;
    m_strOSDFont = s.strOSDFont;

    m_bShowTime = s.fUseSeekbarHover;
    m_bShowPreview = s.fSeekPreview && m_bShowTime;

    PreviewEnableSubControls(m_bShowPreview);

    m_iSeekPreviewSize = s.iSeekPreviewSize;
    m_SeekPreviewSizeCtrl.SetRange32(5, 40);

    m_fShowChapters = s.fShowChapters;

    m_FontType.Clear();
    m_FontSize.Clear();
    HDC dc = CreateDC(_T("DISPLAY"), nullptr, nullptr, nullptr);
    CAtlArray<CString> fntl;
    EnumFontFamilies(dc, nullptr, (FONTENUMPROC)EnumFontProc, (LPARAM)&fntl);
    DeleteDC(dc);
    for (size_t i = 0; i < fntl.GetCount(); ++i) {
        if (i > 0 && fntl[i - 1] == fntl[i]) {
            continue;
        }
        m_FontType.AddString(fntl[i]);
    }
    int iSel = m_FontType.FindStringExact(0, m_strOSDFont);
    if (iSel == CB_ERR) {
        iSel = m_FontType.FindString(0, m_strOSDFont);
        if (iSel == CB_ERR) {
            LOGFONT lf;
            GetMessageFont(&lf);
            iSel = m_FontType.FindStringExact(0, lf.lfFaceName);
            if (iSel == CB_ERR) {
                iSel = 0;
            }
        }
    }
    m_FontType.SetCurSel(iSel);

    CString str;
    for (int i = 10; i < 51; ++i) {
        str.Format(_T("%d"), i);
        m_FontSize.AddString(str);
        if (m_nOSDSize == i) {
            iSel = i;
        }
    }
    m_FontSize.SetCurSel(iSel - 10);

    m_fShowOSD = s.fShowOSD;
    m_fShowCurrentTimeInOSD = s.fShowCurrentTimeInOSD;
    m_bShowVideoInfoInStatusbar = s.bShowVideoInfoInStatusbar;
    m_bShowAudioFormatInStatusbar = s.bShowAudioFormatInStatusbar;
    m_bShowLangInStatusbar = s.bShowLangInStatusbar;
    m_bShowFPSInStatusbar = s.bShowFPSInStatusbar;
    m_bShowABMarksInStatusbar = s.bShowABMarksInStatusbar;
    m_fSnapToDesktopEdges = s.fSnapToDesktopEdges;
    m_fLimitWindowProportions = s.fLimitWindowProportions;
    m_bHideWindowedControls = s.bHideWindowedControls;
    m_bUseEnhancedTaskBar = s.bUseEnhancedTaskBar;
    m_bUseSMTC = s.bUseSMTC;

    AdjustDynamicWidgets();
    CreateToolTip();
    EnableThemedDialogTooltips(this);

    UpdateData(FALSE);

    return TRUE;  // return TRUE unless you set the focus to a control
    // EXCEPTION: OCX Property Pages should return FALSE
}

BOOL CPPageTheme::OnApply()
{
    UpdateData();

    CAppSettings& s = AfxGetAppSettings();

    s.bMPCTheme = !!m_bUseModernTheme;
    s.iModernSeekbarHeight = m_iModernSeekbarHeight;
    s.eModernThemeMode = static_cast<CMPCTheme::ModernThemeMode>(m_iThemeMode);

    int iLangSel = m_langsComboBox.GetCurSel();
    if (iLangSel != CB_ERR) {
        LANGID language = (LANGID)m_langsComboBox.GetItemData(iLangSel);
        if (s.language != language) {
            // Show a warning when switching to Arabic or Hebrew (must not be translated)
            if (PRIMARYLANGID(language) == LANG_ARABIC || PRIMARYLANGID(language) == LANG_HEBREW) {
                AfxMessageBox(_T("The Arabic and Hebrew translations will be correctly displayed (with a right-to-left layout) after restarting the application.\n"),
                    MB_ICONINFORMATION | MB_OK);
            }

            if (!Translations::SetLanguage(language)) {
                // In case of error, reset the language to English
                language = 0;
                m_langsComboBox.SetCurSel(m_nPosLangEnglish);
            }
            s.language = language;

            // Inform all interested listeners that the UI language changed
            m_eventc.FireEvent(MpcEvent::CHANGING_UI_LANGUAGE);
        }
    } else {
        ASSERT(FALSE);
    }

    s.nHoverPosition = m_HoverPosition.GetCurSel();

    s.nOSDSize = m_nOSDSize;
    m_FontType.GetLBText(m_FontType.GetCurSel(), s.strOSDFont);
    if (s.strOSDFont.GetLength() >= LF_FACESIZE) {
        s.strOSDFont = s.strOSDFont.Left(LF_FACESIZE - 1);
    }
    s.fShowChapters = !!m_fShowChapters;

    s.fSeekPreview = !!m_bShowPreview;
    s.fUseSeekbarHover = !!m_bShowTime;
    if (m_iSeekPreviewSize < 5) m_iSeekPreviewSize = 5;
    if (m_iSeekPreviewSize > 40) m_iSeekPreviewSize = 40;
    CMainFrame* pFrame = ((CMainFrame*)GetParentFrame());
    if (pFrame && s.iSeekPreviewSize != m_iSeekPreviewSize) {
        s.iSeekPreviewSize = m_iSeekPreviewSize;
        pFrame->m_wndPreView.SetRelativeSize(m_iSeekPreviewSize);
    }

    // There is no main frame when the option dialog is displayed stand-alone
    if (CMainFrame* pMainFrame = AfxGetMainFrame()) {
        pMainFrame->UpdateControlState(CMainFrame::UPDATE_SKYPE);
        pMainFrame->UpdateControlState(CMainFrame::UPDATE_SEEKBAR_CHAPTERS);
    }

    s.fShowOSD = m_fShowOSD;
    s.fShowCurrentTimeInOSD = m_fShowCurrentTimeInOSD;
    s.bShowVideoInfoInStatusbar = m_bShowVideoInfoInStatusbar;
    s.bShowAudioFormatInStatusbar = m_bShowAudioFormatInStatusbar;
    s.bShowLangInStatusbar = m_bShowLangInStatusbar;
    s.bShowFPSInStatusbar = m_bShowFPSInStatusbar;
    s.bShowABMarksInStatusbar = m_bShowABMarksInStatusbar;
    s.fSnapToDesktopEdges = m_fSnapToDesktopEdges;
    s.fLimitWindowProportions = m_fLimitWindowProportions;
    s.bHideWindowedControls = m_bHideWindowedControls;

    if (s.bUseEnhancedTaskBar != (bool)m_bUseEnhancedTaskBar) {
        s.bUseEnhancedTaskBar = m_bUseEnhancedTaskBar;
        if (pFrame) {
            if (m_bUseEnhancedTaskBar) {
                pFrame->CreateThumbnailToolbar();
            } else {
                pFrame->UpdateThumbarButton();
            }
        }
    }

    s.bUseSMTC = m_bUseSMTC;

    return __super::OnApply();
}

void CPPageTheme::PreviewEnableSubControls(bool previewEnabled) {
    if (previewEnabled) { //there is no need to disable because time will still be checked if we are disabling
        m_HoverPosition.EnableWindow(TRUE);
    }
    m_ShowTimeCtrl.EnableWindow(!previewEnabled);
    if (previewEnabled) {
        m_ShowTimeCtrl.SetCheck(1);
    }
    m_SeekPreviewSizeEdit.EnableWindow(previewEnabled);
    m_SeekPreviewSizeCtrl.EnableWindow(previewEnabled);
}

void CPPageTheme::OnPreviewClicked() {
    PreviewEnableSubControls(IsDlgButtonChecked(IDC_SEEK_PREVIEW));
    SetModified();
}

void CPPageTheme::OnTimeClicked() {
    m_HoverPosition.EnableWindow(IsDlgButtonChecked(IDC_CHECK14));
    SetModified();
}

void CPPageTheme::ThemeEnableSubControls(bool themeEnabled) {
    m_ThemeMode.EnableWindow(themeEnabled);
    m_ModernSeekbarHeightCtrl.EnableWindow(themeEnabled);
    m_ModernSeekbarHeightEdit.EnableWindow(themeEnabled);
}

void CPPageTheme::OnThemeClicked() {
    ThemeEnableSubControls(IsDlgButtonChecked(IDC_CHECK1));
    SetModified();
}

BOOL CPPageTheme::OnToolTipNotify(UINT id, NMHDR* pNMH, LRESULT* pResult) {
    LPTOOLTIPTEXT pTTT = reinterpret_cast<LPTOOLTIPTEXT>(pNMH);

    UINT_PTR nID = pNMH->idFrom;
    if (pTTT->uFlags & TTF_IDISHWND) {
        nID = ::GetDlgCtrlID((HWND)nID);
    }

    BOOL bRet = FALSE;

    switch (nID) {
    case IDC_COMBO5:
        bRet = FillComboToolTip(m_FontType, pTTT);
        break;
    case IDC_COMBO3:
        bRet = FillComboToolTip(m_HoverPosition, pTTT);
        break;
    }

    if (bRet) {
        PlaceThemedDialogTooltip(nID);
    }

    return bRet;
}

void CPPageTheme::OnChngOSDCombo() {
    CString str;
    m_nOSDSize = m_FontSize.GetCurSel() + 10;
    m_FontType.GetLBText(m_FontType.GetCurSel(), str);
    if (CMainFrame* pMainFrame = AfxGetMainFrame()) {
        pMainFrame->m_OSD.DisplayMessage(OSD_TOPLEFT, _T("Test"), 2000, m_nOSDSize, str);
    }
    SetModified();
}

void CPPageTheme::AdjustDynamicWidgets() {
    AdjustDynamicWidgetPair(this, IDC_STATIC22, IDC_MODERNSEEKBARHEIGHT);
    AdjustDynamicWidgetPair(this, IDC_STATIC2, IDC_COMBO1);
    AdjustDynamicWidgetPair(this, IDC_STATIC5, IDC_COMBO2);
    AdjustDynamicWidgetPair(this, IDC_STATIC11, IDC_COMBO3);
    AdjustDynamicWidgetPair(this, IDC_STATIC7, IDC_EDIT4);
    AdjustDynamicWidgetPair(this, IDC_STATIC6, IDC_COMBO5);
    AdjustDynamicWidgetPair(this, IDC_STATIC23, IDC_COMBO6);
}
