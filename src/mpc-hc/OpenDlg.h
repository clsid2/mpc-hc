/*
 * (C) 2003-2006 Gabest
 * (C) 2006-2014 see Authors.txt
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

#include "resource.h"
#include "CMPCThemeResizableDialog.h"
#include "CMPCThemeStatic.h"
#include "CMPCThemeComboBox.h"

// COpenDlg dialog

class COpenDlg : public CMPCThemeResizableDialog
{
    //  DECLARE_DYNAMIC(COpenDlg)
private:
    CStatic m_icon;
    CMPCThemeComboBox m_cbMRU;
    CString m_path;
    CMPCThemeComboBox m_cbMRUDub;
    CString m_pathDub;
    CMPCThemeStatic m_labelDub;
    BOOL m_bAppendToPlaylist;

    bool m_bMultipleFiles;
    CAtlList<CString> m_fns;

public:
    COpenDlg(CWnd* pParent = nullptr);
    virtual ~COpenDlg();

    // Dialog Data
    enum { IDD = IDD_OPEN_DLG };

    const CAtlList<CString>& GetFileNames() const { return m_fns; }
    bool HasMultipleFiles() const { return m_bMultipleFiles; }
    bool GetAppendToPlaylist() const { return !!m_bAppendToPlaylist; }

    UINT GetDialogTemplateID() const override { return IDD; }
    void SetupAnchors() override;
    TrackSizeConstraints GetTrackSizeConstraints() const override;

protected:
    virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
    virtual BOOL OnInitDialog();

    DECLARE_MESSAGE_MAP()

    afx_msg void OnBrowseFile();
    afx_msg void OnBrowseDubFile();
    afx_msg void OnOk();
    afx_msg void OnUpdateDub(CCmdUI* pCmdUI);
    afx_msg void OnUpdateOk(CCmdUI* pCmdUI);
};
