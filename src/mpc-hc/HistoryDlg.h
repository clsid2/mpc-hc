/*
 * (C) 2026 see Authors.txt
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
#include "CMPCThemeModelessResizableDialog.h"
#include "CMPCThemeEdit.h"
#include "CMPCThemeButton.h"
#include "CMPCThemePlayerListCtrl.h"
#include "AppSettings.h"
#include "resource.h"

// CHistoryDlg dialog

class CHistoryDlg : public CMPCThemeModelessResizableDialog
{
private:
    enum {
        COL_PATH = 0,
        COL_TITLE,
        COL_POS,
        COL_LAST_OPENED,
        COL_COUNT
    };

    CMPCThemeButton m_menuButton;
    CMPCThemeEdit m_filterEdit;
    CMPCThemeButton m_removeSelectedButton;
    CMPCThemePlayerListCtrl m_list;

    std::vector<RecentFileEntry> m_entries;
    UINT_PTR m_nFilterTimerID = 0;

    void RefreshList();
    void SetupList();
    void ApplyColumnWidths();
    void SaveColumnWidths();
    void CopySelectedPaths();
    void OpenItem(int nItem);
    void RemoveEntries(const std::list<CStringW>& hashes, const std::list<CString>& paths);
    void ConfirmRemoveSelected();
    void RemoveSelected();
    void ResetPosition();
    void ResetTrackSelection();
    void RemoveMissingFiles();
    void RemoveURLs();
    void RemoveOlderThan(int days);
    void ClearHistory();

public:
    CHistoryDlg(CWnd* pParent = nullptr);
    virtual ~CHistoryDlg();

    enum { IDD = IDD_HISTORY };

    UINT GetDialogTemplateID() const override { return IDD; }
    void SetupAnchors() override;

protected:
    virtual void DoDataExchange(CDataExchange* pDX);
    virtual BOOL OnInitDialog();
    virtual BOOL PreTranslateMessage(MSG* pMsg);

    DECLARE_MESSAGE_MAP()

public:
    afx_msg void OnShowWindow(BOOL bShow, UINT nStatus);
    afx_msg void OnTimer(UINT_PTR nIDEvent);
    afx_msg void OnChangeFilterEdit();
    afx_msg void OnBnClickedMenu();
    afx_msg void OnBnClickedRemoveSelected();
    afx_msg void OnDblclkList(NMHDR* pNMHDR, LRESULT* pResult);
    afx_msg void OnKeydownList(NMHDR* pNMHDR, LRESULT* pResult);
    afx_msg void OnDestroy();
};
