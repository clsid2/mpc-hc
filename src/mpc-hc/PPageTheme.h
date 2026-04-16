/*
 * (C) 2003-2006 Gabest
 * (C) 2006-2012, 2016 see Authors.txt
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

#pragma once

// CPPageTheme dialog

#include "CMPCThemePPageBase.h"
#include "CMPCThemeSpinButtonCtrl.h"
#include "CMPCThemeRadioOrCheck.h"
#include "CMPCThemeEdit.h"

class CPPageTheme : public CMPCThemePPageBase
{
    DECLARE_DYNAMIC(CPPageTheme)

private:
    BOOL m_bUseModernTheme;
    int m_iModernSeekbarHeight;
    CMPCThemeSpinButtonCtrl m_ModernSeekbarHeightCtrl;
    CMPCThemeEdit m_ModernSeekbarHeightEdit;
    CMPCThemeComboBox m_ThemeMode;
    int m_iThemeMode;
    CMPCThemeComboBox m_langsComboBox;
    CMPCThemeComboBox m_HoverPosition;
    int m_nPosLangEnglish;
    CMPCThemeComboBox m_FontSize;
    CMPCThemeComboBox m_FontType;
    int m_nOSDSize;

    CString m_strOSDFont;

    BOOL m_fShowChapters;
    int m_iSeekPreviewSize;
    CMPCThemeSpinButtonCtrl m_SeekPreviewSizeCtrl;
    CMPCThemeEdit m_SeekPreviewSizeEdit;
    BOOL m_fShowOSD;
    BOOL m_fShowCurrentTimeInOSD;
    BOOL m_bShowVideoInfoInStatusbar;
    BOOL m_bShowAudioFormatInStatusbar;
    BOOL m_bShowLangInStatusbar;
    BOOL m_bShowFPSInStatusbar;
    BOOL m_bShowABMarksInStatusbar;
    BOOL m_fSnapToDesktopEdges;
    BOOL m_fLimitWindowProportions;
    BOOL m_bHideWindowedControls;
    BOOL m_bUseEnhancedTaskBar;
    BOOL m_bUseSMTC;
    BOOL m_bShowPreview;
    BOOL m_bShowTime;
    CMPCThemeRadioOrCheck m_ShowTimeCtrl;
public:
    CPPageTheme();
    virtual ~CPPageTheme();

    // Dialog Data
    EventClient m_eventc;
    enum { IDD = IDD_PPAGETHEME };

protected:
    virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
    virtual BOOL OnInitDialog();
    virtual BOOL OnApply();

    void PreviewEnableSubControls(bool hoverEnabled);
    void ThemeEnableSubControls(bool themeEnabled);

    DECLARE_MESSAGE_MAP()
public:
    afx_msg BOOL OnToolTipNotify(UINT id, NMHDR* pNMH, LRESULT* pResult);
    afx_msg void OnPreviewClicked();
    afx_msg void OnTimeClicked();
    afx_msg void OnThemeClicked();
    afx_msg void OnChngOSDCombo();
    void AdjustDynamicWidgets();
};
