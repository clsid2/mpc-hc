/*
 * (C) 2003-2006 Gabest
 * (C) 2006-2013, 2015-2016 see Authors.txt
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

#include "CMPCThemePPageBase.h"
#include "FloatEdit.h"
#include "DropTarget.h"
#include "CMPCThemePlayerListCtrl.h"
#include "CMPCThemeTreeCtrl.h"

bool IsExternalVideoRenderer(CLSID clsid);
bool IgnoreExternalFilter(CLSID clsid);

class CPPageExternalFiltersListBox : public CMPCThemePlayerListCtrl
{
    DECLARE_DYNAMIC(CPPageExternalFiltersListBox)

public:
    CPPageExternalFiltersListBox();

protected:
    virtual void PreSubclassWindow();
    virtual INT_PTR OnToolHitTest(CPoint point, TOOLINFO* pTI) const;

    DECLARE_MESSAGE_MAP()

    afx_msg BOOL OnToolTipNotify(UINT id, NMHDR* pNMHDR, LRESULT* pResult);
};


// CPPageExternalFilters dialog

class CPPageExternalFilters : public CMPCThemePPageBase, public CDropClient
{
    DECLARE_DYNAMIC(CPPageExternalFilters)

public:
    CPPageExternalFilters();
    virtual ~CPPageExternalFilters();

    // Dialog Data
    enum { IDD = IDD_PPAGEEXTERNALFILTERS };

private:
    CAutoPtrList<FilterOverride> m_pFilters;
    FilterOverride* m_pLastSelFilter;

    CPPageExternalFiltersListBox m_filters;
    int m_iLoadType;
    CMPCThemeHexEdit m_dwMerit;
    CMPCThemeTreeCtrl m_tree;

    CDropTarget m_dropTarget;
    void OnDropFiles(std::list<CString>& slFiles, DROPEFFECT) override;
    DROPEFFECT OnDropAccept(COleDataObject*, DWORD, CPoint) override;

    void Exchange(CListCtrl& list, int i, int j);
    void StepUp(CListCtrl& list);
    void StepDown(CListCtrl& list);

    FilterOverride* GetCurFilter();

    void SetupMajorTypes(CAtlArray<GUID>& guids);
    void SetupSubTypes(CAtlArray<GUID>& guids);

protected:
    virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
    virtual BOOL OnInitDialog();
    virtual BOOL OnApply();

    DECLARE_MESSAGE_MAP()

    afx_msg void OnUpdateFilter(CCmdUI* pCmdUI);
    afx_msg void OnUpdateFilterUp(CCmdUI* pCmdUI);
    afx_msg void OnUpdateFilterDown(CCmdUI* pCmdUI);
    afx_msg void OnUpdateFilterMerit(CCmdUI* pCmdUI);
    afx_msg void OnUpdateSubType(CCmdUI* pCmdUI);
    afx_msg void OnUpdateDeleteType(CCmdUI* pCmdUI);
    afx_msg void OnAddRegistered();
    afx_msg void OnRemoveFilter();
    afx_msg void OnMoveFilterUp();
    afx_msg void OnMoveFilterDown();
    void OnDoubleClickFilter(NMHDR* pNMHDR, LRESULT* pResult);
    afx_msg int OnVKeyToItem(UINT nKey, CListBox* pListBox, UINT nIndex);
    afx_msg void OnAddMajorType();
    afx_msg void OnAddSubType();
    afx_msg void OnDeleteType();
    afx_msg void OnResetTypes();
    void OnFilterChanged(NMHDR* pNMHDR, LRESULT* pResult);
    afx_msg void OnFilterSelectionChange();
    afx_msg void OnFilterCheckChange();
    afx_msg void OnClickedMeritRadioButton();
    afx_msg void OnChangeMerit();
    afx_msg void OnDoubleClickType(NMHDR* pNMHDR, LRESULT* pResult);
    afx_msg void OnKeyDownType(NMHDR* pNMHDR, LRESULT* pResult);
    afx_msg void OnDestroy();
    afx_msg BOOL OnToolTipNotify(UINT id, NMHDR* pNMHDR, LRESULT* pResult);
};
