/*
 * (C) 2003-2006 Gabest
 * (C) 2006-2012, 2017 see Authors.txt
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
#include "SaveTextFileDialog.h"


// CSaveTextFileDialog

IMPLEMENT_DYNAMIC(CSaveTextFileDialog, CFileDialog)
CSaveTextFileDialog::CSaveTextFileDialog(
    CTextFile::enc e,
    LPCTSTR lpszDefExt, LPCTSTR lpszFileName,
    LPCTSTR lpszFilter, CWnd* pParentWnd)
    : CFileDialog(FALSE, lpszDefExt, lpszFileName,
                  OFN_EXPLORER | OFN_ENABLESIZING | OFN_HIDEREADONLY | OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR,
                  lpszFilter, pParentWnd, 0)
    , m_e(e)
{
    // customization has to be done before OnInitDialog
    if (m_bVistaStyle) {
        IFileDialogCustomize* pfdc = GetIFileDialogCustomize();

        pfdc->StartVisualGroup(IDS_TEXTFILE_ENC, ResStr(IDS_TEXTFILE_ENC));
        pfdc->AddComboBox(IDC_COMBO1);
        pfdc->AddControlItem(IDC_COMBO1, CTextFile::DEFAULT_ENCODING, _T("Automatic"));
        pfdc->AddControlItem(IDC_COMBO1, CTextFile::ANSI, _T("ANSI"));
        pfdc->AddControlItem(IDC_COMBO1, CTextFile::LE16, _T("Unicode (UTF-16 LE BOM)"));
        pfdc->AddControlItem(IDC_COMBO1, CTextFile::BE16, _T("Unicode (UTF-16 BE BOM)"));
        pfdc->AddControlItem(IDC_COMBO1, CTextFile::UTF8, _T("UTF-8"));
        pfdc->SetSelectedControlItem(IDC_COMBO1, m_e);
        pfdc->EndVisualGroup();
        pfdc->MakeProminent(IDS_TEXTFILE_ENC);

        pfdc->Release();
    }
}

CSaveTextFileDialog::~CSaveTextFileDialog()
{
}

BOOL CSaveTextFileDialog::OnInitDialog()
{
    __super::OnInitDialog();
    return TRUE;  // return TRUE unless you set the focus to a control
    // EXCEPTION: OCX Property Pages should return FALSE
}

BEGIN_MESSAGE_MAP(CSaveTextFileDialog, CFileDialog)
END_MESSAGE_MAP()

// CSaveTextFileDialog message handlers

BOOL CSaveTextFileDialog::OnFileNameOK()
{
    DWORD result;
    if (m_bVistaStyle) {
        IFileDialogCustomize* pfdc = GetIFileDialogCustomize();
        pfdc->GetSelectedControlItem(IDC_COMBO1, &result);
        pfdc->Release();
        m_e = (CTextFile::enc)result;
    }

    return __super::OnFileNameOK();
}

void CSaveTextFileDialog::OnFolderChange()
{
    __super::OnFolderChange();
    if (m_bVistaStyle) {
        subClassFileDialog(static_cast<IFileDialog*>(m_pIFileDialog));
    }
}
