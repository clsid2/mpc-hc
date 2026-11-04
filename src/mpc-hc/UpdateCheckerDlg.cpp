/*
 * (C) 2012-2013, 2017 see Authors.txt
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


#include <afxdialogex.h>
#include "stdafx.h"
#include "UpdateCheckerDlg.h"
#include "mpc-hc_config.h"

IMPLEMENT_DYNAMIC(UpdateCheckerDlg, CMPCThemeDialog)

UpdateCheckerDlg::UpdateCheckerDlg(Update_Status updateStatus, const Version& latestVersion, CWnd* pParent /*=nullptr*/)
    : CMPCThemeDialog(UpdateCheckerDlg::IDD, pParent)
    , m_updateStatus(updateStatus)
    , m_latestVersion(latestVersion)
{
    switch (updateStatus) {
        case UPDATER_UPDATE_AVAILABLE:
        case UPDATER_UPDATE_AVAILABLE_IGNORED:
            m_text.Format(IDS_NEW_UPDATE_AVAILABLE,
                          latestVersion.ToString().GetString(),
                          UpdateChecker::MPC_HC_VERSION.ToString().GetString());
            break;
        case UPDATER_LATEST_STABLE:
            m_text.LoadString(IDS_USING_LATEST_STABLE);
            break;
        case UPDATER_NEWER_VERSION:
            m_text.Format(IDS_USING_NEWER_VERSION,
                          UpdateChecker::MPC_HC_VERSION.ToString().GetString(),
                          latestVersion.ToString().GetString());
            break;
        case UPDATER_ERROR:
            m_text.LoadString(IDS_UPDATE_ERROR);
            break;
        default:
            ASSERT(0); // should never happen
    }
}

UpdateCheckerDlg::~UpdateCheckerDlg()
{
}

void UpdateCheckerDlg::DoDataExchange(CDataExchange* pDX)
{
    CMPCThemeDialog::DoDataExchange(pDX);
    DDX_Text(pDX, IDC_UPDATE_DLG_TEXT, m_text);
    DDX_Control(pDX, IDC_UPDATE_ICON, m_icon);
    DDX_Control(pDX, IDC_UPDATE_DL_BUTTON, m_dlButton);
    DDX_Control(pDX, IDC_UPDATE_LATER_BUTTON, m_laterButton);
    DDX_Control(pDX, IDC_UPDATE_IGNORE_BUTTON, m_ignoreButton);
    fulfillThemeReqs();
}


BEGIN_MESSAGE_MAP(UpdateCheckerDlg, CMPCThemeDialog)
    ON_BN_CLICKED(IDC_UPDATE_DL_BUTTON, OnOpenDownloadPage)
    ON_BN_CLICKED(IDC_UPDATE_LATER_BUTTON, OnUpdateLater)
    ON_BN_CLICKED(IDC_UPDATE_IGNORE_BUTTON, OnIgnoreUpdate)
END_MESSAGE_MAP()

BOOL UpdateCheckerDlg::OnInitDialog()
{
    BOOL ret = __super::OnInitDialog();

    switch (m_updateStatus) {
        case UPDATER_UPDATE_AVAILABLE:
        case UPDATER_UPDATE_AVAILABLE_IGNORED:
            m_icon.SetIcon(LoadIcon(nullptr, IDI_QUESTION));
            break;
        case UPDATER_LATEST_STABLE:
        case UPDATER_NEWER_VERSION:
        case UPDATER_ERROR: {
            m_icon.SetIcon(LoadIcon(nullptr, (m_updateStatus == UPDATER_ERROR) ? IDI_WARNING : IDI_INFORMATION));
            m_dlButton.ShowWindow(SW_HIDE);
            m_laterButton.ShowWindow(SW_HIDE);
            m_ignoreButton.SetWindowText(ResStr(IDS_UPDATE_CLOSE));

            CRect buttonRect, windowRect;
            m_ignoreButton.GetWindowRect(&buttonRect);
            ScreenToClient(&buttonRect);
            // Reduce the button width
            buttonRect.left += 30;
            // Center the button
            GetWindowRect(&windowRect);
            buttonRect.MoveToX((windowRect.Width() - buttonRect.Width()) / 2);
            m_ignoreButton.MoveWindow(&buttonRect);

            // Change the default button
            SetDefID(IDC_UPDATE_IGNORE_BUTTON);
            ret = FALSE; // Focus has been set explicitly
        }
        break;
        default:
            ASSERT(0); // should never happen
    }

    return ret;
}

void UpdateCheckerDlg::OnOpenDownloadPage()
{
    ShellExecute(nullptr, _T("open"), DOWNLOAD_URL, nullptr, nullptr, SW_SHOWNORMAL);

    DestroyWindow();
}

void UpdateCheckerDlg::OnUpdateLater()
{
    DestroyWindow();
}

void UpdateCheckerDlg::OnIgnoreUpdate()
{
    if (m_updateStatus == UPDATER_UPDATE_AVAILABLE) {
        UpdateChecker::IgnoreLatestVersion(m_latestVersion);
    }

    DestroyWindow();
}

// modeless and owned by CMainFrame, so Enter, Escape and the close box destroy
// the window instead of ending a modal loop
void UpdateCheckerDlg::OnOK()
{
    DestroyWindow();
}

void UpdateCheckerDlg::OnCancel()
{
    DestroyWindow();
}
