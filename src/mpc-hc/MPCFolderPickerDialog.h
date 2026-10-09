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

#pragma once
#include "CMPCThemeUtil.h"

class MPCFolderPickerDialog : public CFolderPickerDialog
    , public CMPCThemeUtil
{
    DECLARE_DYNAMIC(MPCFolderPickerDialog)

public:
    MPCFolderPickerDialog(LPCTSTR lpszFolder = NULL, DWORD dwFlags = 0, CWnd* pParentWnd = NULL, int prominentID = 0, DWORD dwSize = 0, BOOL fNonFileSystemFolders = FALSE);
protected:

    DECLARE_MESSAGE_MAP()
    virtual BOOL OnInitDialog();
    virtual void OnFolderChange();

};
