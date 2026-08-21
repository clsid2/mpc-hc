/*
 * (C) 2026 see Authors.txt
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

#include "DebugShadersDlg.h"
#include "CMPCThemeSliderCtrl.h"
#include "resource.h"

// CColorControlsDlg dialog

class CColorControlsDlg : public CModelessDialog
{
public:
    CColorControlsDlg();

    enum { IDD = IDD_COLORCONTROLS_DLG };

    UINT GetDialogTemplateID() const override { return IDD; }
    void SetupAnchors() override;
    TrackSizeConstraints GetTrackSizeConstraints() const override;

protected:
    CMPCThemeSliderCtrl m_SliBrightness;
    CMPCThemeSliderCtrl m_SliContrast;
    CMPCThemeSliderCtrl m_SliHue;
    CMPCThemeSliderCtrl m_SliSaturation;
    CString m_sBrightness;
    CString m_sContrast;
    CString m_sHue;
    CString m_sSaturation;

    void UpdateSliders();

    virtual void DoDataExchange(CDataExchange* pDX) override;
    virtual BOOL OnInitDialog() override;

    DECLARE_MESSAGE_MAP()
    afx_msg void OnHScroll(UINT nSBCode, UINT nPos, CScrollBar* pScrollBar);
    afx_msg void OnBnClickedReset();
    afx_msg void OnShowWindow(BOOL bShow, UINT nStatus);
};
