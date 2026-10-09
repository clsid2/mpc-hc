/*
 * (C) 2024 see Authors.txt
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
#include "MPCFolderPickerDialog.h"


IMPLEMENT_DYNAMIC(MPCFolderPickerDialog, CFolderPickerDialog)
MPCFolderPickerDialog::MPCFolderPickerDialog(LPCTSTR lpszFolder, DWORD dwFlags, CWnd* pParentWnd, int prominentID, DWORD dwSize, BOOL fNonFileSystemFolders):
    CFolderPickerDialog(lpszFolder, dwFlags, pParentWnd, dwSize, fNonFileSystemFolders)
{
    if (m_bVistaStyle) {
        dialogProminentControlStringID = prominentID;
        IFileDialogCustomize* pfdc = GetIFileDialogCustomize();
        pfdc->AddCheckButton(prominentID, ResStr(prominentID), TRUE);
        pfdc->Release();
    }

}

BOOL MPCFolderPickerDialog::OnInitDialog()
{
    __super::OnInitDialog();
    return TRUE;  // return TRUE unless you set the focus to a control
    // EXCEPTION: OCX Property Pages should return FALSE
}

void MPCFolderPickerDialog::OnFolderChange()
{
    __super::OnFolderChange();
    if (m_bVistaStyle) {
        subClassFileDialog(static_cast<IFileDialog*>(m_pIFileDialog));
    }
}

BEGIN_MESSAGE_MAP(MPCFolderPickerDialog, CFolderPickerDialog)
END_MESSAGE_MAP()

