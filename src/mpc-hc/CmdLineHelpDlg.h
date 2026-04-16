/*
 * (C) 2014 see Authors.txt
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
#include "CMPCThemeResizableDialog.h"
#include "resource.h"

class CmdLineHelpDlg : public CMPCThemeResizableDialog
{
private:
    CStatic m_icon;
    CString m_cmdLine;
    CString m_text;

public:
    CmdLineHelpDlg(const CString& cmdLine = _T(""));
    virtual ~CmdLineHelpDlg();

    enum { IDD = IDD_CMD_LINE_HELP };

    UINT GetDialogTemplateID() const override { return IDD; }
    void SetupAnchors() override;

protected:
    virtual void DoDataExchange(CDataExchange* pDX);
    afx_msg virtual BOOL OnInitDialog();

    DECLARE_MESSAGE_MAP()
};
