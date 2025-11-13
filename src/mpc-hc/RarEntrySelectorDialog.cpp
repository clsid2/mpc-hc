/*
 * (C) 2014, 2016-2017 see Authors.txt
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
#include "RarEntrySelectorDialog.h"
#include "SettingsDefines.h"

RarEntrySelectorDialog::RarEntrySelectorDialog(CRFSList<CRFSFile>* file_list, CWnd* parent)
    : CMPCThemeResizableDialog(RarEntrySelectorDialog::IDD, parent)
    ,currentEntry(L"")
    ,currentIndex(-1)
{
    this->file_list = file_list;
}

RarEntrySelectorDialog::~RarEntrySelectorDialog() {
}

CStringW RarEntrySelectorDialog::GetCurrentEntry() {
    return currentEntry;
}

int RarEntrySelectorDialog::GetCurrentIndex() {
    return currentIndex;
}

void RarEntrySelectorDialog::OnOK() {
    int index = m_list.GetCurSel();
    if (LB_ERR != index) {
        CRFSFile* file = (CRFSFile*)m_list.GetItemData(index);
        currentEntry = file->filename;
        currentIndex = index;
    }

    CMPCThemeResizableDialog::OnOK();
}

void RarEntrySelectorDialog::DoDataExchange(CDataExchange* pDX) {
    __super::DoDataExchange(pDX);
    DDX_Control(pDX, IDC_LIST1, m_list);
    fulfillThemeReqs();
}


BEGIN_MESSAGE_MAP(RarEntrySelectorDialog, CMPCThemeResizableDialog)
    ON_LBN_DBLCLK(IDC_LIST1, OnLbnDblclkList1)
END_MESSAGE_MAP()

BOOL RarEntrySelectorDialog::OnInitDialog() {
    __super::OnInitDialog();

    CRFSFile* file = file_list->First();
    int index = 0;
    while (file) {
        int item = m_list.AddString(file->filename);
        m_list.SetItemData(item, (DWORD_PTR)file);
        file = file_list->Next(file);
        index++;
    }

    if (index > 0) {
        m_list.SetCurSel(0);
    }

    EnableSaveRestoreKey(IDS_R_DLG_RAR_ENTRY_SELECTOR);
    AddAnchor(IDC_LIST1, TOP_LEFT, BOTTOM_RIGHT);
    AddAnchor(IDOK, BOTTOM_RIGHT);
    AddAnchor(IDCANCEL, BOTTOM_RIGHT);

    fulfillThemeReqs();

    return FALSE;
}

void RarEntrySelectorDialog::OnLbnDblclkList1() {
    CPoint clk(GetCurrentMessage()->pt);
    m_list.ScreenToClient(&clk);
    BOOL bOutside = FALSE;
    UINT index = m_list.ItemFromPoint(clk, bOutside);
    if (!bOutside) { //LBN_DBLCLK doesn't actually check if you clicked in a blank space
        SendMessage(WM_COMMAND, IDOK); //has the result of clicking "Select" with current selection
    }
}
