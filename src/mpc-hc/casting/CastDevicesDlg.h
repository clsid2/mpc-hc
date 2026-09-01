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

#include "CMPCThemeResizableDialog.h"
#include "CMPCThemePlayerListCtrl.h"
#include "CMPCThemeComboBox.h"
#include "CMPCThemeEdit.h"
#include "CastTarget.h"
#include "resource.h"
#include <vector>

// The one place device discovery ever runs. It is started when this dialog
// opens and stopped when it closes, so the player holds no socket and no
// worker thread for casting while nobody is looking for a device. What the
// user keeps here is what the cast submenu is built from afterwards, which is
// why that menu is instant and silent.
class CCastDevicesDlg : public CMPCThemeResizableDialog
{
public:
    CCastDevicesDlg(CCastTarget* pTarget, CWnd* pParent = nullptr);
    virtual ~CCastDevicesDlg();

    enum { IDD = IDD_CASTDEVICES_DLG };

    // so that a move to a monitor of another DPI re-lays the dialog out from
    // its template and re-anchors it
    UINT GetDialogTemplateID() const override { return IDD; }
    void SetupAnchors() override;

    // in: the saved list as it stands; out, on IDOK: the list to save
    std::vector<CastSavedDevice> m_devices;

protected:
    // One line of the list: a device that is saved, one the discovery has just
    // found, or -- most of the time -- both at once.
    struct Row {
        CastSavedDevice device;
        bool saved = false;
        bool online = false;
    };

    virtual void DoDataExchange(CDataExchange* pDX);
    virtual BOOL OnInitDialog();
    virtual BOOL PreTranslateMessage(MSG* pMsg);
    virtual void OnOK();

    void RebuildRows();
    CString RowsSignature() const;
    void FillList();
    void UpdateColumnWidths();
    void UpdateStatusText();
    int SelectedRow() const;
    void SelectById(const CString& id);
    Row* FindRow(const CString& id);
    void StartDiscovery();

    CMPCThemePlayerListCtrl m_list;
    CMPCThemeEdit m_name;
    CMPCThemeEdit m_host;
    CMPCThemeEdit m_port;
    CMPCThemeComboBox m_protocol;

    CCastTarget* m_pTarget;
    std::vector<Row> m_rows;
    CString m_signature;  // what the list was last filled from
    // Whether any row has an advertised name worth a column of its own; see
    // FillList(). Most lists have none, and then that column is not shown.
    bool m_bShowAdvertised = false;
    ULONGLONG m_scanStarted = 0;
    bool m_bDiscovering = false;
    bool m_bFilling = false; // the list is being rebuilt, its notifications mean nothing

    DECLARE_MESSAGE_MAP()

public:
    afx_msg void OnDestroy();
    afx_msg void OnTimer(UINT_PTR nIDEvent);
    afx_msg void OnSize(UINT nType, int cx, int cy);
    afx_msg void OnLvnItemChanged(NMHDR* pNMHDR, LRESULT* pResult);
    afx_msg void OnAdd();
    afx_msg void OnUpdateAdd(CCmdUI* pCmdUI);
    afx_msg void OnRemove();
    afx_msg void OnUpdateRemove(CCmdUI* pCmdUI);
    afx_msg void OnRename();
    afx_msg void OnUpdateRename(CCmdUI* pCmdUI);
    afx_msg void OnRescan();
    afx_msg void OnFind();
    afx_msg void OnUpdateFind(CCmdUI* pCmdUI);
};
