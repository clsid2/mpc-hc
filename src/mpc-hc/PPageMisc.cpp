/*
 * (C) 2006-2014, 2016-2017 see Authors.txt
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
#include "PPageOutput.h"
#include "moreuuids.h"
#include "PPageMisc.h"
#include <psapi.h>
#include "PPageSheet.h"
#include "CMPCThemeMsgBox.h"

// CPPageMisc dialog

IMPLEMENT_DYNAMIC(CPPageMisc, CMPCThemePPageBase)
CPPageMisc::CPPageMisc()
    : CMPCThemePPageBase(CPPageMisc::IDD, CPPageMisc::IDD)
    , m_nUpdaterAutoCheck(0)
    , m_nUpdaterDelay(7)
{
}

CPPageMisc::~CPPageMisc()
{
}

void CPPageMisc::DoDataExchange(CDataExchange* pDX)
{
    __super::DoDataExchange(pDX);
    DDX_Control(pDX, IDC_EXPORT_KEYS, m_ExportKeys);
    DDX_Check(pDX, IDC_CHECK1, m_nUpdaterAutoCheck);
    DDX_Text(pDX, IDC_EDIT1, m_nUpdaterDelay);
    DDX_Control(pDX, IDC_CHECK1, m_updaterAutoCheckCtrl);
    DDX_Control(pDX, IDC_EDIT1, m_updaterDelayCtrl);
    DDX_Control(pDX, IDC_SPIN1, m_updaterDelaySpin);

    // Validate the delay between each check
    if (pDX->m_bSaveAndValidate && (m_nUpdaterDelay < 1 || m_nUpdaterDelay > 365)) {
        m_updaterDelayCtrl.ShowBalloonTip(ResStr(IDS_UPDATE_DELAY_ERROR_TITLE), ResStr(IDS_UPDATE_DELAY_ERROR_MSG), TTI_ERROR);
        pDX->PrepareEditCtrl(IDC_EDIT1);
        pDX->Fail();
    }
}


BEGIN_MESSAGE_MAP(CPPageMisc, CMPCThemePPageBase)
    ON_BN_CLICKED(IDC_RESET_SETTINGS, OnResetSettings)
    ON_BN_CLICKED(IDC_EXPORT_SETTINGS, OnExportSettings)
    ON_BN_CLICKED(IDC_EXPORT_KEYS, OnExportKeys)
    ON_UPDATE_COMMAND_UI(IDC_EDIT1, OnUpdateDelayEditBox)
    ON_UPDATE_COMMAND_UI(IDC_SPIN1, OnUpdateDelayEditBox)
    ON_UPDATE_COMMAND_UI(IDC_STATIC5, OnUpdateDelayEditBox)
    ON_UPDATE_COMMAND_UI(IDC_STATIC6, OnUpdateDelayEditBox)
END_MESSAGE_MAP()


// CPPageMisc message handlers

BOOL CPPageMisc::OnInitDialog()
{
    __super::OnInitDialog();

    const CAppSettings& s = AfxGetAppSettings();

    CreateToolTip();

    if (AfxGetMyApp()->IsIniValid()) {
        m_ExportKeys.EnableWindow(FALSE);
    }

    m_nUpdaterAutoCheck = s.nUpdaterAutoCheck;
    m_nUpdaterDelay = s.nUpdaterDelay;
    m_updaterDelaySpin.SetRange32(1, 365);

    AdjustDynamicWidgets();
    UpdateData(FALSE);

    return TRUE;
}

BOOL CPPageMisc::OnApply()
{
    UpdateData();

    CAppSettings& s = AfxGetAppSettings();

    s.nUpdaterAutoCheck = m_nUpdaterAutoCheck;
    s.nUpdaterDelay = m_nUpdaterDelay;

    return __super::OnApply();
}

void CPPageMisc::OnUpdateDelayEditBox(CCmdUI* pCmdUI)
{
    pCmdUI->Enable(m_updaterAutoCheckCtrl.GetCheck() == BST_CHECKED);
}

void CPPageMisc::OnResetSettings()
{
    if (CMPCThemeMsgBox::MessageBox(this, ResStr(IDS_RESET_SETTINGS_WARNING), ResStr(IDS_RESET_SETTINGS), MB_ICONEXCLAMATION | MB_YESNO | MB_DEFBUTTON2) == IDYES) {
        AfxGetAppSettings().SetAsUninitialized(); // Consider the settings as initialized

        // Exit the Options dialog and inform the caller that we want to reset the settings
        EndDialog(CPPageSheet::RESET_SETTINGS);
    }
}

void CPPageMisc::OnExportSettings()
{
    if (GetParent()->GetDlgItem(ID_APPLY_NOW)->IsWindowEnabled()) {
        int ret = CMPCThemeMsgBox::MessageBox(this, ResStr(IDS_EXPORT_SETTINGS_WARNING), ResStr(IDS_EXPORT_SETTINGS), MB_ICONEXCLAMATION | MB_YESNOCANCEL);

        if (ret == IDCANCEL) {
            return;
        } else if (ret == IDYES) {
            GetParent()->PostMessage(PSM_APPLY);
        }
    }

    // In INI mode the settings live in two files (settings + MediaHistory), so
    // they are exported together as a .zip; in registry mode a single .reg.
    CString ext = AfxGetMyApp()->IsIniValid() ? _T("zip") : _T("reg");
    CFileDialog fileSaveDialog(FALSE, ext, _T("mpc-hc-settings.") + ext);

    if (fileSaveDialog.DoModal() == IDOK) {
        if (AfxGetMyApp()->ExportSettings(fileSaveDialog.GetPathName())) {
            CMPCThemeMsgBox::MessageBox(this, ResStr(IDS_EXPORT_SETTINGS_SUCCESS), ResStr(IDS_EXPORT_SETTINGS), MB_ICONINFORMATION | MB_OK);
        } else {
            CMPCThemeMsgBox::MessageBox(this, ResStr(IDS_EXPORT_SETTINGS_FAILED), ResStr(IDS_EXPORT_SETTINGS), MB_ICONERROR | MB_OK);
        }
    }
}

void CPPageMisc::OnExportKeys()
{
    if (GetParent()->GetDlgItem(ID_APPLY_NOW)->IsWindowEnabled()) {
        int ret = CMPCThemeMsgBox::MessageBox(this, ResStr(IDS_EXPORT_SETTINGS_WARNING), ResStr(IDS_EXPORT_SETTINGS), MB_ICONEXCLAMATION | MB_YESNOCANCEL);

        if (ret == IDCANCEL) {
            return;
        } else if (ret == IDYES) {
            GetParent()->PostMessage(PSM_APPLY);
        }
    }

    CFileDialog fileDialogKeys(FALSE, _T("reg"), _T("mpc-hc-keys.reg"));
    if (fileDialogKeys.DoModal() == IDOK) {
        if (AfxGetMyApp()->ExportSettings(fileDialogKeys.GetPathName(), _T("Commands2"))) {
            // also export mouse settings from registry
            if (!AfxGetMyApp()->IsIniValid()) {
                CFileDialog fileDialogMouse(FALSE, _T("reg"), _T("mpc-hc-mouse.reg"));
                if (fileDialogMouse.DoModal() == IDOK) {
                    AfxGetMyApp()->ExportSettings(fileDialogMouse.GetPathName(), _T("Mouse"));
                }
            }
            CMPCThemeMsgBox::MessageBox(this, ResStr(IDS_EXPORT_SETTINGS_SUCCESS), ResStr(IDS_EXPORT_SETTINGS), MB_ICONINFORMATION | MB_OK);
        } else {
            if (GetLastError() == ERROR_FILE_NOT_FOUND) {
                CMPCThemeMsgBox::MessageBox(this, ResStr(IDS_EXPORT_SETTINGS_NO_KEYS), ResStr(IDS_EXPORT_SETTINGS), MB_ICONINFORMATION | MB_OK);
            } else {
                CMPCThemeMsgBox::MessageBox(this, ResStr(IDS_EXPORT_SETTINGS_FAILED), ResStr(IDS_EXPORT_SETTINGS), MB_ICONERROR | MB_OK);
            }
        }
    }
}

void CPPageMisc::AdjustDynamicWidgets() {
    AdjustDynamicWidgetPair(this, IDC_STATIC5, IDC_EDIT1);
}
