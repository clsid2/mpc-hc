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

#include "CMPCThemeModelessResizableDialog.h"
#include "CMPCThemeTabCtrl.h"
#include "CMPCThemePlayerListCtrl.h"

// CFavoriteOrganizeDlg dialog

class CFavoriteOrganizeDlg : public CMPCThemeModelessResizableDialog
{
    //  DECLARE_DYNAMIC(CFavoriteOrganizeDlg)

private:
    CAtlList<CString> m_sl[3];

public:
    CFavoriteOrganizeDlg(CWnd* pParent = nullptr);   // standard constructor
    virtual ~CFavoriteOrganizeDlg();

    virtual BOOL PreTranslateMessage(MSG* pMsg);

    // Dialog Data
    enum { IDD = IDD_FAVORGANIZE };

    UINT GetDialogTemplateID() const override { return IDD; }
    void SetupAnchors() override;

    CMPCThemeTabCtrl m_tab;
    CMPCThemePlayerListCtrl m_list;
    bool firstSize=false;
    int minSizeTime = 0;
    bool m_bModified = false;
    void LoadList();
    void AddItemToVisualList(int tabIndex, const CString& favoriteString);

protected:
    virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
    virtual BOOL OnInitDialog();

    void SetupList(bool fSave);
    void SaveChanges();

    void UpdateColumnsSizes();
    void MoveItem(int nItem, int offset);
    void PlayFavorite(int nItem);
    void CopyToClipboard();

    DECLARE_MESSAGE_MAP()
public:
    afx_msg void OnTcnSelchangeTab1(NMHDR* pNMHDR, LRESULT* pResult);
    afx_msg void OnDrawItem(int nIDCtl, LPDRAWITEMSTRUCT lpDrawItemStruct);
    afx_msg void OnLvnItemchangedList2(NMHDR* pNMHDR, LRESULT* pResult);
    afx_msg void OnRenameBnClicked();
    afx_msg void OnUpdateRenameBn(CCmdUI* pCmdUI);
    afx_msg void OnDeleteBnClicked();
    afx_msg void OnUpdateDeleteBn(CCmdUI* pCmdUI);
    afx_msg void OnUpBnClicked();
    afx_msg void OnUpdateUpBn(CCmdUI* pCmdUI);
    afx_msg void OnDownBnClicked();
    afx_msg void OnUpdateDownBn(CCmdUI* pCmdUI);
    afx_msg void OnTcnSelchangingTab1(NMHDR* pNMHDR, LRESULT* pResult);
    afx_msg void OnBnClickedOk();
    afx_msg void OnBnClickedApply();
    afx_msg void OnUpdateApplyBn(CCmdUI* pCmdUI);
    virtual void OnCancel();
    afx_msg void OnShowWindow(BOOL bShow, UINT nStatus);
    afx_msg void OnActivate(UINT nState, CWnd* pWndOther, BOOL bMinimized);
    afx_msg void OnLvnEndlabeleditList2(NMHDR* pNMHDR, LRESULT* pResult);
    afx_msg void OnPlayFavorite(NMHDR* pNMHDR, LRESULT* pResult);
    afx_msg void OnKeyPressed(NMHDR* pNMHDR, LRESULT* pResult);
    afx_msg void OnSize(UINT nType, int cx, int cy);
    afx_msg void OnLvnGetInfoTipList(NMHDR* pNMHDR, LRESULT* pResult);
    afx_msg LRESULT OnDpiChanged(WPARAM wParam, LPARAM lParam);
};
