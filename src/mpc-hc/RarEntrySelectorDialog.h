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
#include "CMPCThemeListBox.h"
#include "resource.h"
#include "RARFileSource/RFS.h"

class RarEntrySelectorDialog : public CMPCThemeResizableDialog
{
private:

public:
    RarEntrySelectorDialog(CRFSList<CRFSFile>* file_list, CWnd* parent);
    virtual ~RarEntrySelectorDialog();
    enum { IDD = IDD_RAR_ENTRY_SELECTOR };

    CStringW GetCurrentEntry();
    int GetCurrentIndex();


protected:
    CMPCThemeListBox m_list;
    CRFSList<CRFSFile>* file_list;
    CStringW currentEntry;
    int currentIndex;
    virtual void OnOK();
    virtual void DoDataExchange(CDataExchange* pDX);
    DECLARE_MESSAGE_MAP()
    afx_msg virtual BOOL OnInitDialog();
    afx_msg void OnLbnDblclkList1();

};
