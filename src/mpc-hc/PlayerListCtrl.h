/*
 * (C) 2003-2006 Gabest
 * (C) 2006-2015 see Authors.txt
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

#include "WinHotkeyCtrl.h"
#include "CMPCThemeComboBox.h"
#include "CMPCThemePlayerListCtrl.h"
#include "CMPCThemeInlineEdit.h"

#define LVN_DOLABELEDIT (LVN_FIRST+1)


class CInPlaceWinHotkey : public CWinHotkeyCtrl
{
private:
    int m_iItem;
    int m_iSubItem;
    CString m_sInitText;
    BOOL m_bESC; // To indicate whether ESC key was pressed

public:
    CInPlaceWinHotkey(int iItem, int iSubItem, CString sInitText);
    virtual ~CInPlaceWinHotkey();

protected:
    virtual BOOL PreTranslateMessage(MSG* pMsg);

    DECLARE_MESSAGE_MAP()

public:
    afx_msg void OnKillFocus(CWnd* pNewWnd);
    afx_msg void OnNcDestroy();
    afx_msg void OnChar(UINT nChar, UINT nRepCnt, UINT nFlags);
    afx_msg int OnCreate(LPCREATESTRUCT lpCreateStruct);
};

class CInPlaceEdit : public CEdit
{
protected:
    int m_iItem;
    int m_iSubItem;
    CString m_sInitText;
    BOOL m_bESC; // To indicate whether ESC key was pressed

public:
    CInPlaceEdit(int iItem, int iSubItem, CString sInitText);
    virtual ~CInPlaceEdit();

protected:
    virtual BOOL PreTranslateMessage(MSG* pMsg);

    DECLARE_MESSAGE_MAP()

public:
    afx_msg void OnKillFocus(CWnd* pNewWnd);
    afx_msg void OnNcDestroy();
    afx_msg void OnChar(UINT nChar, UINT nRepCnt, UINT nFlags);
    afx_msg int OnCreate(LPCREATESTRUCT lpCreateStruct);
};

class CInPlaceFloatEdit : public CInPlaceEdit
{
public:
    CInPlaceFloatEdit(int iItem, int iSubItem, CString sInitText);
    virtual ~CInPlaceFloatEdit();

protected:
    DECLARE_MESSAGE_MAP()

public:
    afx_msg void OnChar(UINT nChar, UINT nRepCnt, UINT nFlags);
};

class CInPlaceComboBox : public CMPCThemeComboBox
{
private:
    int m_iItem;
    int m_iSubItem;
    CAtlList<CString> m_lstItems;
    int m_nSel;
    BOOL m_bESC; // To indicate whether ESC key was pressed

public:
    CInPlaceComboBox(int iItem, int iSubItem, CAtlList<CString>& plstItems, int nSel);
    virtual ~CInPlaceComboBox();

protected:
    virtual BOOL PreTranslateMessage(MSG* pMsg);

    DECLARE_MESSAGE_MAP()

public:
    afx_msg void OnKillFocus(CWnd* pNewWnd);
    afx_msg void OnNcDestroy();
    afx_msg void OnChar(UINT nChar, UINT nRepCnt, UINT nFlags);
    afx_msg int OnCreate(LPCREATESTRUCT lpCreateStruct);
    afx_msg void OnCloseup();
};

class CInPlaceListBox : public CListBox
{
private:
    int m_iItem;
    int m_iSubItem;
    CAtlList<CString> m_lstItems;
    int m_nSel;
    BOOL m_bESC; // To indicate whether ESC key was pressed

public:
    CInPlaceListBox(int iItem, int iSubItem, CAtlList<CString>& plstItems, int nSel);
    virtual ~CInPlaceListBox();

protected:
    virtual BOOL PreTranslateMessage(MSG* pMsg);

    DECLARE_MESSAGE_MAP()

public:
    afx_msg void OnKillFocus(CWnd* pNewWnd);
    afx_msg void OnNcDestroy();
    afx_msg void OnChar(UINT nChar, UINT nRepCnt, UINT nFlags);
    afx_msg int OnCreate(LPCREATESTRUCT lpCreateStruct);
};

// CMPCThemeInPlaceEdit — themed inline edit for virtual (LVS_OWNERDATA) list controls.
// CPlayerListCtrl creates and owns instances. The edit notifies the list ctrl via a
// back-reference pointer and self-destructs in OnNcDestroy (same as CInPlaceEdit).

class CMPCThemeInPlaceEdit : public CMPCThemeInlineEdit
{
protected:
    int m_iItem;
    int m_iSubItem;
    CString m_sInitText;
    CMPCThemeInPlaceEdit** m_ppSelf; // nulled on destruction so owner's pointer clears
    BOOL m_bESC;

public:
    CMPCThemeInPlaceEdit(int iItem, int iSubItem, CString sInitText, CMPCThemeInPlaceEdit** ppSelf);
    virtual ~CMPCThemeInPlaceEdit();

protected:
    virtual BOOL PreTranslateMessage(MSG* pMsg);

    DECLARE_MESSAGE_MAP()

public:
    afx_msg void OnKillFocus(CWnd* pNewWnd);
    afx_msg void OnNcDestroy();
    afx_msg void OnChar(UINT nChar, UINT nRepCnt, UINT nFlags);
    afx_msg int OnCreate(LPCREATESTRUCT lpCreateStruct);
};

// CPlayerListCtrl

class CPlayerListCtrl : public CMPCThemePlayerListCtrl
{
    DECLARE_DYNAMIC(CPlayerListCtrl)

private:
    int m_nItemClicked, m_nSubItemClicked;
    int m_tStartEditingDelay;
    bool inPlaceControl;
    CRect inPlaceControlRect;
    UINT_PTR m_nTimerID;

    CMPCThemeInPlaceEdit* m_pVirtualEdit; // active virtual-list inline edit, or nullptr

    bool PrepareInPlaceControl(int nRow, int nCol, CRect& rect);
    void StartVirtualEditLabel(int nItem, int nSubItem);

public:
    CPlayerListCtrl(int tStartEditingDelay = 500);
    virtual ~CPlayerListCtrl();

    int HitTestEx(const CPoint& point, int* col) const;
    CImageList* CreateDragImageEx(LPPOINT lpPoint);

    int GetBottomIndex() const;

    // Returns the active virtual-list inline edit, or nullptr if none is open.
    // Valid during and after LVN_BEGINLABELEDIT; caller may adjust rect/font.
    CMPCThemeInPlaceEdit* GetVirtualEditCtrl() const { return m_pVirtualEdit; }

    // Positions the active virtual-list inline edit. xOffset shifts the left edge
    // (e.g. to clear a sequence number drawn before the label); maxWidth caps the
    // right edge. Call from LVN_BEGINLABELEDIT before returning *pResult = 1.
    void AdjustVirtualEditPos(int xOffset, int maxWidth = -1);

    static LRESULT SendLabelEditNotify(CWnd* pList, UINT code, int nItem, int nSubItem, LPCTSTR pszText = nullptr);

    CWinHotkeyCtrl* ShowInPlaceWinHotkey(int nItem, int nCol);
    CEdit* ShowInPlaceEdit(int nItem, int nCol);
    CEdit* ShowInPlaceFloatEdit(int nItem, int nCol);
    CComboBox* ShowInPlaceComboBox(int nItem, int nCol, CAtlList<CString>& lstItems, int nSel, bool bShowDropDown = false);
    CListBox* ShowInPlaceListBox(int nItem, int nCol, CAtlList<CString>& lstItems, int nSel);
    int InsertColumn(_In_ int nCol, _In_z_ LPCWSTR lpszColumnHeading, _In_ int nFormat = LVCFMT_LEFT, _In_ int nWidth = -1, _In_ int nSubItem = -1, _In_ int nMinWidth = 20);

    bool m_fInPlaceDirty;

protected:
    virtual INT_PTR OnToolHitTest(CPoint point, TOOLINFO* pTI) const;
    virtual ULONG GetGestureStatus(CPoint) override { return 0; };

    DECLARE_MESSAGE_MAP()

public:
    afx_msg void OnSize(UINT nType, int cx, int cy);
    afx_msg void OnVScroll(UINT nSBCode, UINT nPos, CScrollBar* pScrollBar);
    afx_msg void OnHScroll(UINT nSBCode, UINT nPos, CScrollBar* pScrollBar);
    afx_msg void OnLButtonDown(UINT nFlags, CPoint point);
    afx_msg BOOL OnMouseWheel(UINT nFlags, short zDelta, CPoint pt);
    afx_msg void OnTimer(UINT_PTR nIDEvent);
    afx_msg void OnLButtonDblClk(UINT nFlags, CPoint point);
    afx_msg void OnLvnMarqueeBegin(NMHDR* pNMHDR, LRESULT* pResult);
    afx_msg void OnLvnInsertitem(NMHDR* pNMHDR, LRESULT* pResult);
    afx_msg void OnLvnDeleteitem(NMHDR* pNMHDR, LRESULT* pResult);
    afx_msg void OnEnChangeEdit1();
    afx_msg void OnEnChangeWinHotkey1();
    afx_msg void OnCbnDropdownCombo1();
    afx_msg void OnCbnSelendokCombo1();
    afx_msg void OnLbnSelChangeList1();
    afx_msg BOOL OnHdnItemchanging(UINT id, NMHDR* pNMHDR, LRESULT* pResult);
    afx_msg BOOL OnToolTipNotify(UINT id, NMHDR* pNMHDR, LRESULT* pResult);
    afx_msg void OnXButtonDown(UINT nFlags, UINT nButton, CPoint point);
    afx_msg void OnXButtonUp(UINT nFlags, UINT nButton, CPoint point);
    afx_msg void OnXButtonDblClk(UINT nFlags, UINT nButton, CPoint point);
    afx_msg BOOL OnSetCursor(CWnd* pWnd, UINT nHitTest, UINT message);
    afx_msg BOOL OnLvnEndlabeledit(NMHDR* pNMHDR, LRESULT* pResult);
    afx_msg LRESULT OnLvmEditLabel(WPARAM wParam, LPARAM lParam);
};
