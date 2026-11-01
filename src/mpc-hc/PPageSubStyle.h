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

#pragma once

#include "ColorButton.h"
#include "CMPCThemePPageBase.h"
#include "../Subtitles/STS.h"
#include "CMPCThemeSliderCtrl.h"

// CPPageSubStyle dialog

class CPPageSubStyle : public CMPCThemePPageBase
{
    DECLARE_DYNAMIC(CPPageSubStyle)

private:
    CString m_title;
    STSStyle m_stss;
    bool m_bDefaultStyle;
    bool isStyleDialog;

    CMPCThemeButton m_font;
    int m_iCharset;
    CMPCThemeComboBox m_cbCharset;
    int iOpenTypeLangHint;
    CMPCThemeComboBox openTypeLangHint;
    CMPCThemeFloatEdit m_spacing;
    int m_angle;
    CMPCThemeSpinButtonCtrl m_angleSpin;
    int m_scalex;
    CMPCThemeSpinButtonCtrl m_scalexSpin;
    int m_scaley;
    CMPCThemeSpinButtonCtrl m_scaleySpin;
    int m_borderStyle;
    BOOL m_bRoundedOpaqueBox;
    CMPCThemeFloatEdit m_borderWidth;
    CMPCThemeFloatEdit m_shadowDepth;
    int m_screenAlignment;
    CRect m_margin;
    CMPCThemeSpinButtonCtrl m_marginLeftSpin;
    CMPCThemeSpinButtonCtrl m_marginRightSpin;
    CMPCThemeSpinButtonCtrl m_marginTopSpin;
    CMPCThemeSpinButtonCtrl m_marginBottomSpin;
    std::array<CColorButton, 4> m_color;
    std::array<int, 4> m_alpha;
    std::array<CMPCThemeSliderCtrl, 4> m_alphaSliders;
    BOOL m_bLinkAlphaSliders;
    int m_iRelativeTo;
#if USE_LIBASS
    int iRenderSSAUsingLibass;
#endif

    void AskColor(int i);

public:
    CPPageSubStyle(bool isStyleDialog = false);
    virtual ~CPPageSubStyle();

    void InitStyle(const CString& title, const STSStyle& stss);
    void GetStyle(STSStyle& stss) const { stss = m_stss; }
    CString GetStyleName() { return m_title; }

    // Dialog Data
    enum { IDD = IDD_PPAGESUBSTYLE };

protected:
    virtual void DoDataExchange(CDataExchange* pDX);
    virtual BOOL OnInitDialog();
    virtual BOOL OnApply();

    DECLARE_MESSAGE_MAP()

    afx_msg void OnChooseFont();
    afx_msg void OnChoosePrimaryColor();
    afx_msg void OnChooseSecondaryColor();
    afx_msg void OnChooseOutlineColor();
    afx_msg void OnChooseShadowColor();
    afx_msg void OnLinkAlphaSlidersChanged();
    afx_msg void OnHScroll(UINT nSBCode, UINT nPos, CScrollBar* pScrollBar);
    afx_msg void OnUpdateRoundedBox(CCmdUI* pCmdUI);
    void AdjustDynamicWidgets();
    afx_msg BOOL OnToolTipNotify(UINT id, NMHDR* pNMHDR, LRESULT* pResult);
};
