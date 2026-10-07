/*
 * (C) 2003-2006 Gabest
 * (C) 2006-2014, 2017 see Authors.txt
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
#include "SaveThumbnailsDialog.h"
#include "CMPCThemeStatic.h"
#undef SubclassWindow

// CSaveThumbnailsDialog

IMPLEMENT_DYNAMIC(CSaveThumbnailsDialog, CSaveImageDialog)
CSaveThumbnailsDialog::CSaveThumbnailsDialog(
    int nJpegQuality, int rows, int cols, int width,
    LPCTSTR lpszDefExt, LPCTSTR lpszFileName,
    LPCTSTR lpszFilter, CWnd* pParentWnd) :
    CSaveImageDialog(nJpegQuality, lpszDefExt, lpszFileName, lpszFilter, pParentWnd),
    m_rows(rows),
    m_cols(cols),
    m_width(width)
{
    // customization has to be done before OnInitDialog
    if (m_bVistaStyle) {
        IFileDialogCustomize* pfdc = GetIFileDialogCustomize();
        CStringW str;

        pfdc->StartVisualGroup(IDS_THUMB_IMAGE_WIDTH, ResStr(IDS_THUMB_IMAGE_WIDTH));
        pfdc->AddText(IDS_THUMB_PIXELS, ResStr(IDS_THUMB_PIXELS));
        str.Format(L"%d", std::clamp(m_width, 256, 3840));
        pfdc->AddEditBox(IDC_EDIT4, str);
        pfdc->EndVisualGroup();

        pfdc->StartVisualGroup(IDS_THUMB_THUMBNAILS, ResStr(IDS_THUMB_THUMBNAILS));
        pfdc->AddText(IDS_THUMB_ROWNUMBER, ResStr(IDS_THUMB_ROWNUMBER));
        str.Format(L"%d", std::clamp(m_rows, 1, 40));
        pfdc->AddEditBox(IDC_EDIT2, str);

        pfdc->AddText(IDS_THUMB_COLNUMBER, ResStr(IDS_THUMB_COLNUMBER));
        str.Format(L"%d", std::clamp(m_cols, 1, 16));
        pfdc->AddEditBox(IDC_EDIT3, str);
        pfdc->EndVisualGroup();

        pfdc->Release();
    }
}

CSaveThumbnailsDialog::~CSaveThumbnailsDialog()
{
}

BOOL CSaveThumbnailsDialog::OnInitDialog()
{
    __super::OnInitDialog();
    return TRUE;  // return TRUE unless you set the focus to a control
    // EXCEPTION: OCX Property Pages should return FALSE
}

BEGIN_MESSAGE_MAP(CSaveThumbnailsDialog, CSaveImageDialog)
END_MESSAGE_MAP()

// CSaveThumbnailsDialog message handlers

BOOL CSaveThumbnailsDialog::OnFileNameOK()
{
    if (m_bVistaStyle) {
        CComPtr<IFileDialogCustomize> pfdc = GetIFileDialogCustomize();
        CComHeapPtr<WCHAR> result;

        if (SUCCEEDED(pfdc->GetEditBoxText(IDC_EDIT2, &result))) {
            m_rows = _wtoi(result);
        }

        result.Free();
        if (SUCCEEDED(pfdc->GetEditBoxText(IDC_EDIT3, &result))) {
            m_cols = _wtoi(result);
        }

        result.Free();
        if (SUCCEEDED(pfdc->GetEditBoxText(IDC_EDIT4, &result))) {
            m_width = _wtoi(result);
        }
    }
    return __super::OnFileNameOK();
}
