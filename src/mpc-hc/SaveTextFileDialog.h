/*
 * (C) 2003-2006 Gabest
 * (C) 2006-2013, 2017 see Authors.txt
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

#include "../Subtitles/TextFile.h"
#include "CMPCThemeUtil.h"

// CSaveTextFileDialog

class CSaveTextFileDialog : public CFileDialog
    , public CMPCThemeUtil
{
protected:
    CTextFile::enc m_e;
    CComboBox m_encoding;

    DECLARE_DYNAMIC(CSaveTextFileDialog)

public:
    CSaveTextFileDialog(
        CTextFile::enc e,
        LPCTSTR lpszDefExt = nullptr, LPCTSTR lpszFileName = nullptr,
        LPCTSTR lpszFilter = nullptr, CWnd* pParentWnd = nullptr);
    virtual ~CSaveTextFileDialog();

    CTextFile::enc GetEncoding() { return m_e; }

protected:
    DECLARE_MESSAGE_MAP()
    virtual BOOL OnInitDialog();
    virtual BOOL OnFileNameOK();
    virtual void OnFolderChange();

    afx_msg void OnEncodingChange();
};
