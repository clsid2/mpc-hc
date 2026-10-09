/*
 * (C) 2014, 2017 see Authors.txt
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

class CSaveImageDialog : public CFileDialog
    , public CMPCThemeUtil
{
    DECLARE_DYNAMIC(CSaveImageDialog)

public:
    CSaveImageDialog(
        int nJpegQuality,
        LPCTSTR lpszDefExt = nullptr, LPCTSTR lpszFileName = nullptr,
        LPCTSTR lpszFilter = nullptr, CWnd* pParentWnd = nullptr, bool subtitleOptionSupported = false);
    virtual ~CSaveImageDialog();

    int m_nJpegQuality;

protected:
    CSpinButtonCtrl m_jpegQualitySpin;
    CEdit m_jpegQualityEdit;
    bool bSubtitleOptionSupported;

    DECLARE_MESSAGE_MAP()
    virtual BOOL OnInitDialog();
    virtual BOOL OnFileNameOK();
    virtual void OnFolderChange();
    virtual void OnTypeChange();
};
