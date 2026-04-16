/*
* (C) 2014-2017 see Authors.txt
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
#include "PPageToolBar.h"
#include "mplayerc.h"
#include "MainFrm.h"
#include "AddCommandDlg.h"
#include "PathUtils.h"
#include "PPageToolBarLayout.h"

IMPLEMENT_DYNAMIC(CPPageToolBar, CMPCThemePPageBase)

CPPageToolBar::CPPageToolBar()
    : CMPCThemePPageBase(IDD, IDD)
, m_iDefaultToolbarSize(DEF_TOOLBAR_HEIGHT)
{
    EventRouter::EventSelection fires;
    fires.insert(MpcEvent::DEFAULT_TOOLBAR_SIZE_CHANGED);
    fires.insert(MpcEvent::TOOLBAR_THEME_CHANGED);
    GetEventd().Connect(m_eventc, fires);
}

void CPPageToolBar::DoDataExchange(CDataExchange* pDX)
{
    __super::DoDataExchange(pDX);
    DDX_Text(pDX, IDC_EDIT1, m_iDefaultToolbarSize);
    DDV_MinMaxInt(pDX, m_iDefaultToolbarSize, MIN_TOOLBAR_HEIGHT, MAX_TOOLBAR_HEIGHT);
    DDX_Control(pDX, IDC_SPIN1, m_DefaultToolbarSizeCtrl);

    DDX_Control(pDX, IDC_COMBO1, m_cmbAction1);
    DDX_Control(pDX, IDC_COMBO2, m_cmbAction2);
    DDX_Control(pDX, IDC_COMBO3, m_cmbAction3);
    DDX_Control(pDX, IDC_COMBO4, m_cmbAction4);
    DDX_Control(pDX, IDC_COMBO5, m_cmbRightAction1);
    DDX_Control(pDX, IDC_COMBO6, m_cmbRightAction2);
    DDX_Control(pDX, IDC_COMBO7, m_cmbRightAction3);
    DDX_Control(pDX, IDC_COMBO8, m_cmbRightAction4);
    DDX_Control(pDX, IDC_COMBO9, m_cmbActiveTheme);
    DDX_Control(pDX, IDC_COMBO10, m_cmbToolbarAlignment);

}

void CPPageToolBar::AddCmdToAction(WORD id, CMPCThemeComboBox &actCombo) {
    auto& s = AfxGetAppSettings();

    AddStringData(actCombo, CString(StrRes(IDS_AG_NONE)), ID_MENU_NONE);
    AddStringData(actCombo, L"<...>", ID_COMBO_ADD_CMD);
    if (id && s.CommandIDToWMCMD.count(id) > 0) {
        size_t idx = actCombo.GetCount();
        auto wc = s.CommandIDToWMCMD[id];
        idx = actCombo.InsertString(idx - 1, ResStr(wc->dwname));
        actCombo.SetItemData(idx, id);
        actCombo.SelectByItemData(id);
    } else {
        actCombo.SelectByItemData(ID_MENU_NONE);
    }
}

void CPPageToolBar::OnActionChange1() {
    OnActionChange(m_cmbAction1);
}
void CPPageToolBar::OnActionChange2() {
    OnActionChange(m_cmbAction2);
}
void CPPageToolBar::OnActionChange3() {
    OnActionChange(m_cmbAction3);
}
void CPPageToolBar::OnActionChange4() {
    OnActionChange(m_cmbAction4);
}
void CPPageToolBar::OnRightActionChange1() {
    OnActionChange(m_cmbRightAction1);
}
void CPPageToolBar::OnRightActionChange2() {
    OnActionChange(m_cmbRightAction2);
}
void CPPageToolBar::OnRightActionChange3() {
    OnActionChange(m_cmbRightAction3);
}
void CPPageToolBar::OnRightActionChange4() {
    OnActionChange(m_cmbRightAction4);
}

void CPPageToolBar::OnChangeTheme() {
}

void CPPageToolBar::OnActionChange(CMPCThemeComboBox& actCombo) {
    int curSel = actCombo.GetCurSel();
    if (curSel != CB_ERR && actCombo.GetItemData(curSel) == ID_COMBO_ADD_CMD) {
        CAddCommandDlg dlg(this);
        if (dlg.DoModal() == IDOK) {
            const WORD id = dlg.GetSelectedCommandID();
            size_t idx;
            for (idx = 0; idx < actCombo.GetCount(); idx++) {
                if (id == actCombo.GetItemData(idx)) {
                    break;
                }
            }

            if (idx == actCombo.GetCount()) {
                AddCmdToAction(id, actCombo);
            }
            actCombo.SelectByItemData(id);
        } else {
            actCombo.SelectByItemData(ID_MENU_NONE);
        }
        SetModified();
    }
}

BOOL CPPageToolBar::OnInitDialog()
{
    __super::OnInitDialog();
    auto& s = AfxGetAppSettings();

    m_DefaultToolbarSizeCtrl.SetRange32(MIN_TOOLBAR_HEIGHT, MAX_TOOLBAR_HEIGHT);
    m_iDefaultToolbarSize = s.nDefaultToolbarSize;

    AdjustDynamicWidgetPair(this, IDC_STATIC3, IDC_EDIT1);

    AddCmdToAction(s.nToolbarAction1, m_cmbAction1);
    AddCmdToAction(s.nToolbarAction2, m_cmbAction2);
    AddCmdToAction(s.nToolbarAction3, m_cmbAction3);
    AddCmdToAction(s.nToolbarAction4, m_cmbAction4);
    AddCmdToAction(s.nToolbarRightAction1, m_cmbRightAction1);
    AddCmdToAction(s.nToolbarRightAction2, m_cmbRightAction2);
    AddCmdToAction(s.nToolbarRightAction3, m_cmbRightAction3);
    AddCmdToAction(s.nToolbarRightAction4, m_cmbRightAction4);
    PopulateThemes();

    m_cmbToolbarAlignment.AddString(ResStr(IDS_TOOLBAR_ALIGNMENT_LEFT));
    m_cmbToolbarAlignment.AddString(ResStr(IDS_TOOLBAR_ALIGNMENT_CENTER));
    m_cmbToolbarAlignment.AddString(ResStr(IDS_TOOLBAR_ALIGNMENT_RIGHT));
    m_cmbToolbarAlignment.SetCurSel(s.nToolbarAlignment);

    UpdateData(FALSE);

    return TRUE;
}

CAppSettings::TOOLBAR_TYPE CPPageToolBar::ExternalTBType(CStringW tbPath) {
    static std::vector<CStringW> postFix = {
        L"buttons16",
        L"buttons24",
        L"buttons32",
        L"buttons48",
        L"buttons64",
    };

    bool svgExists = true, svg16 = true;
    for (const auto& pf : postFix) {
        if (!PathUtils::Exists(PathUtils::CombinePaths(tbPath, pf + L".svg"))) {
            if (pf == L"buttons16") {
                svg16 = false; //it's ok, we will keep going
            } else {
                svgExists = false;
                break;
            }
        }
    }

    if (svgExists) {
        if (svg16) {
            return CAppSettings::EXTERNAL_TOOLBAR_WITH_16;
        } else {
            return CAppSettings::EXTERNAL_TOOLBAR_NO_16;
        }
    }

    bool pngExists = true, png16 = true;
    for (const auto& pf : postFix) {
        if (!PathUtils::Exists(PathUtils::CombinePaths(tbPath, pf + L".png"))) {
            if (pf == L"buttons16") {
                png16 = false; //it's ok, we will keep going
            } else {
                pngExists = false;
                break;
            }
        }
    }
    if (pngExists) {
        if (png16) {
            return CAppSettings::EXTERNAL_TOOLBAR_WITH_16;
        } else {
            return CAppSettings::EXTERNAL_TOOLBAR_NO_16;
        }
    }
    return CAppSettings::INVALID_TOOLBAR;
}

void CPPageToolBar::PopulateThemes() {
    auto& s = AfxGetAppSettings();

    AddStringData(m_cmbActiveTheme, CString(StrRes(IDS_AG_BUILTIN_TOOLBAR)), 0);
    m_cmbActiveTheme.SelectByItemData(0);

    CStringW dirPostfix = L"toolbars/*";
    std::vector<CString> paths({ PathUtils::CombinePaths(PathUtils::GetProgramPath(), dirPostfix) });
    CString appDataPath;
    if (AfxGetMyApp()->GetAppDataPath(appDataPath)) {
        paths.emplace_back(PathUtils::CombinePaths(appDataPath, dirPostfix));
    }

    CFileFind finder;
    for (const auto& path : paths) {
        BOOL next = finder.FindFile(path);
        while (next) {
            next = finder.FindNextFile();
            CString tbPath = finder.GetFilePath();
            if (!finder.IsDots() && finder.IsDirectory()) {

                CAppSettings::TOOLBAR_TYPE tbType = ExternalTBType(tbPath);
                if (tbType > 0) {
                    AddStringData(m_cmbActiveTheme, finder.GetFileName(), tbType);
                    if (finder.GetFileName() == s.strToolbarName && s.nToolbarType > 0) {
                        m_cmbActiveTheme.SetCurSel(m_cmbActiveTheme.GetCount() - 1);
                    }
                }
            }
        }
    }
}

BOOL CPPageToolBar::OnApply()
{
    UpdateData();
    auto& s = AfxGetAppSettings();

    int idx = m_cmbActiveTheme.GetCurSel();
    CStringW oldTBName = s.strToolbarName;
    if (idx == 0 || idx == CB_ERR) {
        s.nToolbarType = CAppSettings::INTERNAL_TOOLBAR;
        s.strToolbarName = L"";
    } else {
        s.nToolbarType = (CAppSettings::TOOLBAR_TYPE)m_cmbActiveTheme.GetItemData(idx);
        m_cmbActiveTheme.GetLBText(idx, s.strToolbarName);
    }

    int nOldDefaultToolbarSize = s.nDefaultToolbarSize;
    s.nDefaultToolbarSize = m_iDefaultToolbarSize;
    if (nOldDefaultToolbarSize != s.nDefaultToolbarSize || s.strToolbarName != oldTBName) {
        if (s.strToolbarName != oldTBName) {
            m_eventc.FireEvent(MpcEvent::TOOLBAR_THEME_CHANGED); //updates toolbar and customize images
            CPPageToolBarLayout* tbl = static_cast<CPPageToolBarLayout*>(FindSiblingPage(RUNTIME_CLASS(CPPageToolBarLayout)));
            if (tbl) {
                tbl->ReloadImageLists();
            }
        } else {
            m_eventc.FireEvent(MpcEvent::DEFAULT_TOOLBAR_SIZE_CHANGED); //updates toolbar only
        }
        if (CMainFrame* pMainFrame = AfxGetMainFrame()) {
            pMainFrame->RecalcLayout();
        }
    }

    auto persistAction = [](UINT& nAction, CMPCThemeComboBox& cmbAction) {
        UINT itemData = (UINT)GetCurItemData(cmbAction);
        if (CB_ERR == itemData || ID_MENU_NONE == itemData) {
            itemData = 0;
        }
        nAction = itemData;
    };

    persistAction(s.nToolbarAction1, m_cmbAction1);
    persistAction(s.nToolbarAction2, m_cmbAction2);
    persistAction(s.nToolbarAction3, m_cmbAction3);
    persistAction(s.nToolbarAction4, m_cmbAction4);
    persistAction(s.nToolbarRightAction1, m_cmbRightAction1);
    persistAction(s.nToolbarRightAction2, m_cmbRightAction2);
    persistAction(s.nToolbarRightAction3, m_cmbRightAction3);
    persistAction(s.nToolbarRightAction4, m_cmbRightAction4);

    int nOldToolbarAlignment = s.nToolbarAlignment;
    s.nToolbarAlignment = m_cmbToolbarAlignment.GetCurSel();

    // Trigger layout recalculation if alignment changed
    if (nOldToolbarAlignment != s.nToolbarAlignment) {
        if (CMainFrame* pMainFrame = AfxGetMainFrame()) {
            // Force the toolbar to recalculate its control positions
            pMainFrame->m_wndToolBar.ArrangeControls();
            pMainFrame->RecalcLayout();
        }
    }

    return __super::OnApply();
}

BEGIN_MESSAGE_MAP(CPPageToolBar, CMPCThemePPageBase)
    ON_CBN_SELENDOK(IDC_COMBO1, OnActionChange1)
    ON_CBN_SELENDOK(IDC_COMBO2, OnActionChange2)
    ON_CBN_SELENDOK(IDC_COMBO3, OnActionChange3)
    ON_CBN_SELENDOK(IDC_COMBO4, OnActionChange4)
    ON_CBN_SELENDOK(IDC_COMBO5, OnRightActionChange1)
    ON_CBN_SELENDOK(IDC_COMBO6, OnRightActionChange2)
    ON_CBN_SELENDOK(IDC_COMBO7, OnRightActionChange3)
    ON_CBN_SELENDOK(IDC_COMBO8, OnRightActionChange4)
    ON_CBN_SELENDOK(IDC_COMBO9, OnChangeTheme)
END_MESSAGE_MAP()
