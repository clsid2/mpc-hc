/*
 * (C) 2003-2006 Gabest
 * (C) 2006-2014, 2017 see Authors.txt
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
#include "PnSPresetsDlg.h"
#include "PanNScan.h"


// CPnSPresetsDlg dialog

IMPLEMENT_DYNAMIC(CPnSPresetsDlg, CDialog)
CPnSPresetsDlg::CPnSPresetsDlg(CWnd* pParent /*=nullptr*/)
    : CMPCThemeResizableDialog(CPnSPresetsDlg::IDD, pParent)
{
}

CPnSPresetsDlg::~CPnSPresetsDlg()
{
}

void CPnSPresetsDlg::DoDataExchange(CDataExchange* pDX)
{
    __super::DoDataExchange(pDX);
    DDX_Control(pDX, IDC_EDIT2, m_PosX);
    DDX_Control(pDX, IDC_EDIT3, m_PosY);
    DDX_Control(pDX, IDC_EDIT4, m_ZoomX);
    DDX_Control(pDX, IDC_EDIT5, m_ZoomY);
    DDX_Text(pDX, IDC_EDIT1, m_label);
    DDX_Control(pDX, IDC_LIST1, m_list);
    fulfillThemeReqs();
}

BOOL CPnSPresetsDlg::OnInitDialog()
{
    __super::OnInitDialog();

    for (INT_PTR i = 0, j = m_pnspresets.GetCount(); i < j; i++) {
        CString label;
        double PosX, PosY, ZoomX, ZoomY;
        StringToParams(m_pnspresets[i], label, PosX, PosY, ZoomX, ZoomY);

        m_list.AddString(label);

        if (i == 0) {
            m_list.SetCurSel(0);
            OnLbnSelchangeList1();
        }
    }

    AddAnchor(IDC_LIST1, TOP_LEFT, BOTTOM_RIGHT);
    AddAnchor(IDC_EDIT1, BOTTOM_LEFT, BOTTOM_RIGHT);
    AddAnchor(IDC_STATIC1, BOTTOM_LEFT, BOTTOM_RIGHT);

    AddAnchor(IDC_EDIT2, TOP_RIGHT);
    AddAnchor(IDC_EDIT3, TOP_RIGHT);
    AddAnchor(IDC_EDIT4, TOP_RIGHT);
    AddAnchor(IDC_EDIT5, TOP_RIGHT);
    AddAnchor(IDC_STATIC2, TOP_RIGHT);
    AddAnchor(IDC_STATIC3, TOP_RIGHT);

    AddAnchor(IDC_BUTTON1, BOTTOM_LEFT);
    AddAnchor(IDC_BUTTON2, BOTTOM_LEFT);
    AddAnchor(IDC_BUTTON3, BOTTOM_LEFT);
    AddAnchor(IDC_BUTTON4, BOTTOM_LEFT);
    AddAnchor(IDC_BUTTON5, BOTTOM_LEFT);

    AddAnchor(IDOK, BOTTOM_RIGHT);
    AddAnchor(IDCANCEL, BOTTOM_RIGHT);

    return TRUE;  // return TRUE unless you set the focus to a control
    // EXCEPTION: OCX Property Pages should return FALSE
}

void CPnSPresetsDlg::StringToParams(CString str, CString& label, double& PosX, double& PosY, double& ZoomX, double& ZoomY)
{
    // the same parser as the menu, so the two cannot disagree; a preset the
    // menu would refuse shows its label with the default values
    PanNScan::Values values;
    if (!PanNScan::ParsePreset(str, label, values)) {
        label = str.SpanExcluding(_T(","));
    }
    PosX = values.posX;
    PosY = values.posY;
    ZoomX = values.zoomX;
    ZoomY = values.zoomY;
}

CString CPnSPresetsDlg::ParamsToString(CString label, double PosX, double PosY, double ZoomX, double ZoomY)
{
    PanNScan::Values values;
    values.posX = PosX;
    values.posY = PosY;
    values.zoomX = ZoomX;
    values.zoomY = ZoomY;
    return PanNScan::FormatPreset(label, values);
}

BEGIN_MESSAGE_MAP(CPnSPresetsDlg, CMPCThemeResizableDialog)
    ON_LBN_SELCHANGE(IDC_LIST1, OnLbnSelchangeList1)
    ON_BN_CLICKED(IDC_BUTTON2, OnBnClickedButton2)
    ON_UPDATE_COMMAND_UI(IDC_BUTTON2, OnUpdateButton2)
    ON_BN_CLICKED(IDC_BUTTON3, OnBnClickedButton6)
    ON_UPDATE_COMMAND_UI(IDC_BUTTON3, OnUpdateButton6)
    ON_BN_CLICKED(IDC_BUTTON4, OnBnClickedButton9)
    ON_UPDATE_COMMAND_UI(IDC_BUTTON4, OnUpdateButton9)
    ON_BN_CLICKED(IDC_BUTTON5, OnBnClickedButton10)
    ON_UPDATE_COMMAND_UI(IDC_BUTTON5, OnUpdateButton10)
    ON_BN_CLICKED(IDC_BUTTON1, OnBnClickedButton1)
    ON_UPDATE_COMMAND_UI(IDC_BUTTON1, OnUpdateButton1)
END_MESSAGE_MAP()


// CPnSPresetsDlg message handlers
void CPnSPresetsDlg::OnLbnSelchangeList1()
{
    int i = m_list.GetCurSel();
    if (i >= 0 && i < m_pnspresets.GetCount()) {
        double PosX, PosY, ZoomX, ZoomY;
        StringToParams(m_pnspresets[i], m_label, PosX, PosY, ZoomX, ZoomY);
        m_PosX = PosX;
        m_PosY = PosY;
        m_ZoomX = ZoomX;
        m_ZoomY = ZoomY;
    } else {
        m_label.Empty();
        m_PosX.SetWindowText(_T(""));
        m_PosY.SetWindowText(_T(""));
        m_ZoomX.SetWindowText(_T(""));
        m_ZoomY.SetWindowText(_T(""));
    }

    UpdateData(FALSE);
}

void CPnSPresetsDlg::OnBnClickedButton2() // new
{
    m_pnspresets.Add(_T("New,0.5,0.5,1.0,1.0"));
    m_list.SetCurSel(m_list.AddString(_T("New")));
    OnLbnSelchangeList1();
}

void CPnSPresetsDlg::OnUpdateButton2(CCmdUI* pCmdUI)
{
    CString str;
    int len = m_list.GetCount();
    if (len > 0) {
        m_list.GetText(len - 1, str);
    }
    pCmdUI->Enable(str != _T("New"));
}

void CPnSPresetsDlg::OnBnClickedButton6() // del
{
    int i = m_list.GetCurSel();
    m_list.DeleteString(i);
    m_pnspresets.RemoveAt(i);
    if (i == m_list.GetCount()) {
        i--;
    }
    m_list.SetCurSel(i);
    OnLbnSelchangeList1();
}

void CPnSPresetsDlg::OnUpdateButton6(CCmdUI* pCmdUI)
{
    pCmdUI->Enable(m_list.GetCurSel() >= 0);
}

void CPnSPresetsDlg::OnBnClickedButton9() // up
{
    int i = m_list.GetCurSel();
    CString str, str2;
    m_list.GetText(i, str);
    str2 = m_pnspresets.GetAt(i);
    m_list.DeleteString(i);
    m_pnspresets.RemoveAt(i);
    i--;
    m_list.InsertString(i, str);
    m_pnspresets.InsertAt(i, str2);
    m_list.SetCurSel(i);
}

void CPnSPresetsDlg::OnUpdateButton9(CCmdUI* pCmdUI)
{
    pCmdUI->Enable(m_list.GetCurSel() > 0 && m_list.GetCurSel() < m_list.GetCount());
}

void CPnSPresetsDlg::OnBnClickedButton10() // down
{
    int i = m_list.GetCurSel();
    CString str, str2;
    m_list.GetText(i, str);
    str2 = m_pnspresets.GetAt(i);
    m_list.DeleteString(i);
    m_pnspresets.RemoveAt(i);
    i++;
    m_list.InsertString(i, str);
    m_pnspresets.InsertAt(i, str2);
    m_list.SetCurSel(i);
}

void CPnSPresetsDlg::OnUpdateButton10(CCmdUI* pCmdUI)
{
    pCmdUI->Enable(m_list.GetCurSel() >= 0 && m_list.GetCurSel() < m_list.GetCount() - 1);
}

void CPnSPresetsDlg::OnBnClickedButton1() // set
{
    int i = m_list.GetCurSel();
    UpdateData();
    /*if (m_label.Remove(',') > 0)
        UpdateData(FALSE);*/
    m_label.Replace(',', '.');  // Replace any ',' with '.' as ',' is used as tokeniser
    if (m_label.IsEmpty()) {    // OK lands here without the Set button's check
        return;
    }
    m_pnspresets[i] = ParamsToString(m_label, m_PosX, m_PosY, m_ZoomX, m_ZoomY);
    m_list.DeleteString(i);
    m_list.InsertString(i, m_label);
    m_list.SetCurSel(i);
}

void CPnSPresetsDlg::OnUpdateButton1(CCmdUI* pCmdUI)
{
    UpdateData();
    pCmdUI->Enable(m_list.GetCurSel() >= 0
                   && !m_label.IsEmpty() // && m_label.Find(',') < 0
                   && m_PosX >= -0.5 && m_PosX <= 1.5
                   && m_PosY >= -0.5 && m_PosY <= 1.5
                   && m_ZoomX >= 0.2 && m_ZoomX <= 5.0
                   && m_ZoomY >= 0.2 && m_ZoomY <= 5.0);
}

void CPnSPresetsDlg::OnOK()
{
    if (m_list.GetCurSel() >= 0) {
        OnBnClickedButton1();
    }

    __super::OnOK();
}
