/*
 * (C) 2012-2013 see Authors.txt
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

#include <afxwin.h>
#include "resource.h"
#include "UpdateChecker.h"
#include "CMPCThemeDialog.h"
#include "CMPCThemeButton.h"

class UpdateCheckerDlg : public CMPCThemeDialog
{
    DECLARE_DYNAMIC(UpdateCheckerDlg)

public:
    UpdateCheckerDlg(Update_Status updateStatus, const Version& latestVersion, CWnd* pParent = nullptr);
    virtual ~UpdateCheckerDlg();

    enum { IDD = IDD_UPDATE_DIALOG };

protected:
    virtual void DoDataExchange(CDataExchange* pDX);
    afx_msg virtual BOOL OnInitDialog();
    virtual void OnOK();
    virtual void OnCancel();
    afx_msg void OnOpenDownloadPage();
    afx_msg void OnUpdateLater();
    afx_msg void OnIgnoreUpdate();

    DECLARE_MESSAGE_MAP()
private:
    Update_Status m_updateStatus;
    Version m_latestVersion;
    CString m_text;
    CStatic m_icon;
    CMPCThemeButton m_dlButton;
    CMPCThemeButton m_laterButton;
    CMPCThemeButton m_ignoreButton;
};
