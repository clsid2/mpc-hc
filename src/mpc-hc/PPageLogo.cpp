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
#include "PPageLogo.h"
#include "CMPCTheme.h"
#include "CMPCThemeUtil.h"
#include "ColorProfileUtil.h"
#include "OpenFileDlg.h"

// CPPageLogo dialog

IMPLEMENT_DYNAMIC(CPPageLogo, CMPCThemePPageBase)
CPPageLogo::CPPageLogo()
    : CMPCThemePPageBase(CPPageLogo::IDD, CPPageLogo::IDD)
    , m_intext(0)
    , colorProfileEnabled(FALSE)
{
    m_logoids.AddTail(IDF_LOGO0);
    m_logoids.AddTail(IDF_LOGO1);
    m_logoids.AddTail(IDF_LOGO2);
    m_logoids.AddTail(IDF_LOGO3);
    m_logoids.AddTail(IDF_LOGO4);
    m_logoidpos = m_logoids.GetHeadPosition();
}

CPPageLogo::~CPPageLogo()
{
}

void CPPageLogo::DoDataExchange(CDataExchange* pDX)
{
    __super::DoDataExchange(pDX);
    DDX_Radio(pDX, IDC_RADIO1, m_intext);
    DDX_Text(pDX, IDC_LOGOFILENAME, m_logofn);
    DDX_Control(pDX, IDC_LOGOPREVIEW, m_logopreview);
    DDX_Text(pDX, IDC_AUTHOR, m_author);
    DDX_Check(pDX, IDC_CHECK1, colorProfileEnabled);
}


BEGIN_MESSAGE_MAP(CPPageLogo, CMPCThemePPageBase)
    ON_BN_CLICKED(IDC_RADIO1, OnBnClickedInternalRadio)
    ON_BN_CLICKED(IDC_RADIO2, OnBnClickedExternalRadio)
    ON_BN_CLICKED(IDC_CHECK1, OnBnClickedColorProfile)
    ON_NOTIFY(UDN_DELTAPOS, IDC_SPIN1, OnDeltaposSpin1)
    ON_BN_CLICKED(IDC_BUTTON2, OnBnClickedButton2)
END_MESSAGE_MAP()


// CPPageLogo message handlers

BOOL CPPageLogo::OnInitDialog()
{
    __super::OnInitDialog();

    const CAppSettings& s = AfxGetAppSettings();

    m_intext = s.fLogoExternal ? 1 : 0;
    m_logofn = s.strLogoFileName;
    colorProfileEnabled = s.fLogoColorProfileEnabled;

    UpdateData(FALSE);

    m_logoidpos = m_logoids.GetHeadPosition();
    int useLogoId = s.nLogoId;
    if (-1 == useLogoId) { // if the user has never chosen a logo, we can try loading a theme default logo
        if (AppIsThemeLoaded()) {
            useLogoId = CMPCThemeUtil::defaultLogo();
        } else {
            useLogoId = DEF_LOGO;
        }
    }

    for (POSITION pos = m_logoids.GetHeadPosition(); pos; m_logoids.GetNext(pos)) {
        if (m_logoids.GetAt(pos) == useLogoId) {
            m_logoidpos = pos;
            break;
        }
    }

    if (!m_intext) {
        OnBnClickedInternalRadio();
    } else {
        OnBnClickedExternalRadio();
    }

    return TRUE;  // return TRUE unless you set the focus to a control
    // EXCEPTION: OCX Property Pages should return FALSE
}

BOOL CPPageLogo::OnApply()
{
    UpdateData();

    CAppSettings& s = AfxGetAppSettings();


    if (s.fLogoExternal != !!m_intext || s.strLogoFileName != m_logofn
            || s.nLogoId != m_logoids.GetAt(m_logoidpos)
            || s.fLogoColorProfileEnabled != colorProfileEnabled) {
        s.fLogoExternal = !!m_intext;
        s.strLogoFileName = m_logofn;
        s.nLogoId = m_logoids.GetAt(m_logoidpos);
        s.fLogoColorProfileEnabled = colorProfileEnabled;

        if (CMainFrame* pMainFrame = AfxGetMainFrame()) {
            pMainFrame->UpdateControlState(CMainFrame::UPDATE_LOGO);
        }
    }

    return __super::OnApply();
}

void CPPageLogo::OnBnClickedInternalRadio()
{
    ASSERT(m_logoidpos);

    GetDataFromRes();
    Invalidate();

    m_intext = 0;
    UpdateData(FALSE);

    SetModified();
}

void CPPageLogo::OnBnClickedExternalRadio() {
    UpdateData();

    m_author.Empty();

    m_logo.DeleteObject();
    m_logo.LoadFromFile(m_logofn);
    SetLogoPreview();
    Invalidate();

    m_intext = 1;
    UpdateData(FALSE);

    SetModified();
}

void CPPageLogo::OnBnClickedColorProfile() {
    UpdateData();
    SetLogoPreview(true);
    SetModified();
}

void CPPageLogo::SetLogoPreview(bool reload) {
    if (reload) {
        if (m_intext) {
            OnBnClickedExternalRadio();
        } else {
            OnBnClickedInternalRadio();
        }
    }
    if (colorProfileEnabled && m_logo.m_hObject) {
        CImage t;
        t.Attach(m_logo);
        ColorProfileUtil::applyColorProfile(m_hWnd, t);
        t.Detach();
    }
    m_logopreview.SetBitmap(m_logo);
}

void CPPageLogo::OnDeltaposSpin1(NMHDR* pNMHDR, LRESULT* pResult)
{
    LPNMUPDOWN pNMUpDown = reinterpret_cast<LPNMUPDOWN>(pNMHDR);

    if (pNMUpDown->iDelta < 0) {
        m_logoids.GetNext(m_logoidpos);
        if (!m_logoidpos) {
            m_logoidpos = m_logoids.GetHeadPosition();
        }
    } else {
        m_logoids.GetPrev(m_logoidpos);
        if (!m_logoidpos) {
            m_logoidpos = m_logoids.GetTailPosition();
        }
    }

    GetDataFromRes();

    UpdateData(FALSE);
    SetModified();
    *pResult = 0;
}

void CPPageLogo::OnBnClickedButton2()
{
    CFileDialog dlg(TRUE, nullptr, m_logofn,
                    OFN_EXPLORER | OFN_ENABLESIZING | OFN_HIDEREADONLY | OFN_NOCHANGEDIR,
                    _T("Images (*.bmp;*.gif;*.jpg;*.png)|*.bmp;*.gif;*.jpg;*.png|All files (*.*)|*.*||"),
                    this, 0);

    if (dlg.DoModal() == IDOK) {
        m_logofn = FileDialogUtils::GetSelectedPath(dlg);
        UpdateData(FALSE);
        OnBnClickedExternalRadio();
    }
}

void CPPageLogo::GetDataFromRes()
{
    m_author.Empty();
    m_logo.DeleteObject();

    int id = m_logoids.GetAt(m_logoidpos);
    if (IDF_LOGO0 != id) {
        m_logo.Load(id);
        if (!m_author.LoadString(id)) {
            m_author.LoadString(IDS_LOGO_AUTHOR);
        }
    }
    SetLogoPreview();
}

