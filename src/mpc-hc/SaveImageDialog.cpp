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

#include "stdafx.h"
#include "mplayerc.h"
#include "SaveImageDialog.h"


IMPLEMENT_DYNAMIC(CSaveImageDialog, CFileDialog)
CSaveImageDialog::CSaveImageDialog(
    int nJpegQuality,
    LPCTSTR lpszDefExt, LPCTSTR lpszFileName,
    LPCTSTR lpszFilter, CWnd* pParentWnd, bool subtitleOptionSupported /*=false*/) :
    CFileDialog(FALSE, lpszDefExt, lpszFileName,
                OFN_EXPLORER | OFN_ENABLESIZING | OFN_HIDEREADONLY | OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR,
                lpszFilter, pParentWnd, 0),
    m_nJpegQuality(nJpegQuality)
{
    if (m_bVistaStyle) {
        IFileDialogCustomize* pfdc = GetIFileDialogCustomize();
        CString str;

        pfdc->StartVisualGroup(IDS_IMAGE_QUALITY, ResStr(IDS_IMAGE_QUALITY));
        str.Format(L"%d", std::clamp(m_nJpegQuality, 20, 100));
        pfdc->AddEditBox(IDC_EDIT1, str);
        pfdc->EndVisualGroup();

        bSubtitleOptionSupported = subtitleOptionSupported;
        if (bSubtitleOptionSupported) {
            pfdc->StartVisualGroup(IDS_SAVEDIALOG_INCLUDE_SUBS, L"");
            pfdc->AddCheckButton(IDS_SCREENSHOT_SUBTITLES, ResStr(IDS_SCREENSHOT_SUBTITLES), AfxGetAppSettings().bSnapShotSubtitles);
            pfdc->EndVisualGroup();
        }

        pfdc->Release();
    }
}

CSaveImageDialog::~CSaveImageDialog()
{
}

INT_PTR CSaveImageDialog::DoModal()
{
    enableFileDialogHook();
    return __super::DoModal();
}


BOOL CSaveImageDialog::OnInitDialog()
{
    __super::OnInitDialog();
    return TRUE;  // return TRUE unless you set the focus to a control
    // EXCEPTION: OCX Property Pages should return FALSE
}

BEGIN_MESSAGE_MAP(CSaveImageDialog, CFileDialog)
END_MESSAGE_MAP()

BOOL CSaveImageDialog::OnFileNameOK()
{
    if (m_bVistaStyle) {
        CComPtr<IFileDialogCustomize> pfdc = GetIFileDialogCustomize();
        CComHeapPtr<WCHAR> result;

        if (SUCCEEDED(pfdc->GetEditBoxText(IDC_EDIT1, &result))) {
            m_nJpegQuality = _wtoi(result);
        }

        m_nJpegQuality = std::clamp(m_nJpegQuality, 20, 100);

        if (bSubtitleOptionSupported) {
            BOOL bChecked;
            pfdc->GetCheckButtonState(IDS_SCREENSHOT_SUBTITLES, &bChecked);
            AfxGetAppSettings().bSnapShotSubtitles = !!bChecked;
        }
    }

    return __super::OnFileNameOK();
}

void CSaveImageDialog::OnTypeChange()
{
    __super::OnTypeChange();

    if (m_bVistaStyle) {
        IFileDialogCustomize* pfdc = GetIFileDialogCustomize();

        if (m_pOFN->nFilterIndex == 2) { // JPEG encoding is chosen
            pfdc->SetControlState(IDS_IMAGE_QUALITY, CDCS_ENABLEDVISIBLE);
            pfdc->SetControlState(IDC_EDIT1, CDCS_ENABLEDVISIBLE);
        } else {
            pfdc->SetControlState(IDS_IMAGE_QUALITY, CDCS_INACTIVE);
            pfdc->SetControlState(IDC_EDIT1, CDCS_INACTIVE);
        }

        pfdc->Release();
    }
}
