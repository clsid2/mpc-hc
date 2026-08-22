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

#include "stdafx.h"
#include "ColorControlsDlg.h"
#include "mplayerc.h"
#include "MainFrm.h"
#include "SettingsDefines.h"
#include "AppSettings.h"

// CColorControlsDlg dialog

CColorControlsDlg::CColorControlsDlg()
    : CModelessDialog(IDD, AfxGetMainWnd())
{
    Create(IDD, AfxGetMainWnd());
}

BOOL CColorControlsDlg::OnInitDialog()
{
    EnableSaveRestoreKey(IDS_R_DLG_COLOR_CONTROLS);

    __super::OnInitDialog();

    SetupAnchors();

    m_SliBrightness.EnableWindow(TRUE);
    m_SliBrightness.SetRange(-100, 100, true);
    m_SliBrightness.SetTic(0);
    m_SliBrightness.SetPageSize(5);
    m_SliBrightness.SetLockToZero();

    m_SliContrast.EnableWindow(TRUE);
    m_SliContrast.SetRange(-100, 100, true);
    m_SliContrast.SetTic(0);
    m_SliContrast.SetPageSize(5);
    m_SliContrast.SetLockToZero();

    m_SliHue.EnableWindow(TRUE);
    m_SliHue.SetRange(-180, 180, true);
    m_SliHue.SetTic(0);
    m_SliHue.SetPageSize(10);
    m_SliHue.SetLockToZero();

    m_SliSaturation.EnableWindow(TRUE);
    m_SliSaturation.SetRange(-100, 100, true);
    m_SliSaturation.SetTic(0);
    m_SliSaturation.SetPageSize(5);
    m_SliSaturation.SetLockToZero();

    UpdateSliders();

    return TRUE;
}

void CColorControlsDlg::SetupAnchors()
{
    AddAnchor(IDC_STATIC1, TOP_LEFT);
    AddAnchor(IDC_STATIC2, TOP_LEFT);
    AddAnchor(IDC_STATIC3, TOP_LEFT);
    AddAnchor(IDC_STATIC4, TOP_LEFT);
    AddAnchor(IDC_STATIC5, TOP_LEFT);
    AddAnchor(IDC_STATIC6, TOP_LEFT);
    AddAnchor(IDC_STATIC7, TOP_LEFT);
    AddAnchor(IDC_STATIC8, TOP_LEFT);
    AddAnchor(IDC_SLI_BRIGHTNESS, TOP_LEFT, TOP_RIGHT);
    AddAnchor(IDC_SLI_CONTRAST,   TOP_LEFT, TOP_RIGHT);
    AddAnchor(IDC_SLI_HUE,        TOP_LEFT, TOP_RIGHT);
    AddAnchor(IDC_SLI_SATURATION, TOP_LEFT, TOP_RIGHT);
    AddAnchor(IDC_RESET, TOP_RIGHT);
    AddAnchor(IDCANCEL,  TOP_RIGHT);
}

TrackSizeConstraints CColorControlsDlg::GetTrackSizeConstraints() const
{
    // The dialog's height is fixed; only its width can be resized, and only up to 2x the template width.
    TrackSizeConstraints constraints;
    constraints.max.enabled = true;
    constraints.max.xMultiplier = 2.0;
    return constraints;
}

void CColorControlsDlg::UpdateSliders()
{
    const CAppSettings& s = AfxGetAppSettings();

    m_SliBrightness.SetPos(s.iBrightness);
    m_SliContrast.SetPos(s.iContrast);
    m_SliHue.SetPos(s.iHue);
    m_SliSaturation.SetPos(s.iSaturation);

    m_sBrightness.Format(s.iBrightness ? _T("%+d") : _T("%d"), s.iBrightness);
    m_sContrast.Format(s.iContrast ? _T("%+d") : _T("%d"), s.iContrast);
    m_sHue.Format(s.iHue ? _T("%+d") : _T("%d"), s.iHue);
    m_sSaturation.Format(s.iSaturation ? _T("%+d") : _T("%d"), s.iSaturation);

    UpdateData(FALSE);
}

void CColorControlsDlg::DoDataExchange(CDataExchange* pDX)
{
    __super::DoDataExchange(pDX);
    DDX_Control(pDX, IDC_SLI_BRIGHTNESS, m_SliBrightness);
    DDX_Control(pDX, IDC_SLI_CONTRAST, m_SliContrast);
    DDX_Control(pDX, IDC_SLI_HUE, m_SliHue);
    DDX_Control(pDX, IDC_SLI_SATURATION, m_SliSaturation);
    DDX_Text(pDX, IDC_STATIC1, m_sBrightness);
    DDX_Text(pDX, IDC_STATIC2, m_sContrast);
    DDX_Text(pDX, IDC_STATIC3, m_sHue);
    DDX_Text(pDX, IDC_STATIC4, m_sSaturation);
    fulfillThemeReqs();
}

void CColorControlsDlg::OnHScroll(UINT nSBCode, UINT nPos, CScrollBar* pScrollBar)
{
    CAppSettings& s = AfxGetAppSettings();

    if (*pScrollBar == m_SliBrightness) {
        s.iBrightness = m_SliBrightness.GetPos();
        ((CMainFrame*)AfxGetMyApp()->GetMainWnd())->SetColorControl(ProcAmp_Brightness, s.iBrightness, s.iContrast, s.iHue, s.iSaturation);
        m_sBrightness.Format(s.iBrightness ? _T("%+d") : _T("%d"), s.iBrightness);
    } else if (*pScrollBar == m_SliContrast) {
        s.iContrast = m_SliContrast.GetPos();
        ((CMainFrame*)AfxGetMyApp()->GetMainWnd())->SetColorControl(ProcAmp_Contrast, s.iBrightness, s.iContrast, s.iHue, s.iSaturation);
        m_sContrast.Format(s.iContrast ? _T("%+d") : _T("%d"), s.iContrast);
    } else if (*pScrollBar == m_SliHue) {
        s.iHue = m_SliHue.GetPos();
        ((CMainFrame*)AfxGetMyApp()->GetMainWnd())->SetColorControl(ProcAmp_Hue, s.iBrightness, s.iContrast, s.iHue, s.iSaturation);
        m_sHue.Format(s.iHue ? _T("%+d") : _T("%d"), s.iHue);
    } else if (*pScrollBar == m_SliSaturation) {
        s.iSaturation = m_SliSaturation.GetPos();
        ((CMainFrame*)AfxGetMyApp()->GetMainWnd())->SetColorControl(ProcAmp_Saturation, s.iBrightness, s.iContrast, s.iHue, s.iSaturation);
        m_sSaturation.Format(s.iSaturation ? _T("%+d") : _T("%d"), s.iSaturation);
    }

    UpdateData(FALSE);

    __super::OnHScroll(nSBCode, nPos, pScrollBar);
}

void CColorControlsDlg::OnBnClickedReset()
{
    CAppSettings& s = AfxGetAppSettings();

    s.iBrightness = AfxGetMyApp()->GetColorControl(ProcAmp_Brightness)->DefaultValue;
    s.iContrast   = AfxGetMyApp()->GetColorControl(ProcAmp_Contrast)->DefaultValue;
    s.iHue        = AfxGetMyApp()->GetColorControl(ProcAmp_Hue)->DefaultValue;
    s.iSaturation = AfxGetMyApp()->GetColorControl(ProcAmp_Saturation)->DefaultValue;

    ((CMainFrame*)AfxGetMyApp()->GetMainWnd())->SetColorControl(ProcAmp_All, s.iBrightness, s.iContrast, s.iHue, s.iSaturation);

    UpdateSliders();
}

void CColorControlsDlg::OnShowWindow(BOOL bShow, UINT nStatus)
{
    if (bShow) {
        UpdateSliders();
    }

    __super::OnShowWindow(bShow, nStatus);
}

BEGIN_MESSAGE_MAP(CColorControlsDlg, CModelessDialog)
    ON_WM_HSCROLL()
    ON_WM_SHOWWINDOW()
    ON_BN_CLICKED(IDC_RESET, OnBnClickedReset)
END_MESSAGE_MAP()
