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

#include "stdafx.h"
#include "mplayerc.h"
#include "PlayerBar.h"
#include "PlayerListCtrl.h"
#include "WinHotkeyCtrl.h"

// CInPlaceHotKey

CInPlaceWinHotkey::CInPlaceWinHotkey(int iItem, int iSubItem, CString sInitText)
    : m_iItem(iItem)
    , m_iSubItem(iSubItem)
    , m_sInitText(sInitText)
    , m_bESC(FALSE)
{
}

CInPlaceWinHotkey::~CInPlaceWinHotkey()
{
}

BEGIN_MESSAGE_MAP(CInPlaceWinHotkey, CWinHotkeyCtrl)
    ON_WM_KILLFOCUS()
    ON_WM_NCDESTROY()
    ON_WM_CHAR()
    ON_WM_CREATE()
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CInPlaceHotKey message handlers
BOOL CInPlaceWinHotkey::PreTranslateMessage(MSG* pMsg)
{
    if (pMsg->message == WM_KEYDOWN) {
        if (pMsg->wParam == VK_RETURN
                || pMsg->wParam == VK_DELETE
                || pMsg->wParam == VK_ESCAPE
                || GetKeyState(VK_CONTROL)) {
            ::TranslateMessage(pMsg);
            ::DispatchMessage(pMsg);
            return TRUE;                // DO NOT process further
        }
    }

    return CWinHotkeyCtrl::PreTranslateMessage(pMsg);
}

void CInPlaceWinHotkey::OnKillFocus(CWnd* pNewWnd)
{
    CWinHotkeyCtrl::OnKillFocus(pNewWnd);

    CString str;
    GetWindowText(str);

    CPlayerListCtrl::SendLabelEditNotify(GetParent(), LVN_ENDLABELEDIT, m_iItem, m_iSubItem,
                                         m_bESC ? nullptr : (LPCTSTR)str);

    DestroyWindow();
}

void CInPlaceWinHotkey::OnNcDestroy()
{
    CWinHotkeyCtrl::OnNcDestroy();

    delete this;
}

void CInPlaceWinHotkey::OnChar(UINT nChar, UINT nRepCnt, UINT nFlags)
{
    if (nChar == VK_ESCAPE || nChar == VK_RETURN) {
        if (nChar == VK_ESCAPE) {
            m_bESC = TRUE;
        }
        GetParent()->SetFocus();
        return;
    }

    CWinHotkeyCtrl::OnChar(nChar, nRepCnt, nFlags);
}

int CInPlaceWinHotkey::OnCreate(LPCREATESTRUCT lpCreateStruct)
{
    if (CWinHotkeyCtrl::OnCreate(lpCreateStruct) == -1) {
        return -1;
    }

    // Set the proper font
    CFont* font = GetParent()->GetFont();
    SetFont(font);

    SetWindowText(m_sInitText);
    SetFocus();
    SetSel(0, -1);
    return 0;
}

// CInPlaceEdit

CInPlaceEdit::CInPlaceEdit(int iItem, int iSubItem, CString sInitText)
    : m_iItem(iItem)
    , m_iSubItem(iSubItem)
    , m_sInitText(sInitText)
    , m_bESC(FALSE)
{
}

CInPlaceEdit::~CInPlaceEdit()
{
}

BEGIN_MESSAGE_MAP(CInPlaceEdit, CEdit)
    //{{AFX_MSG_MAP(CInPlaceEdit)
    ON_WM_KILLFOCUS()
    ON_WM_NCDESTROY()
    ON_WM_CHAR()
    ON_WM_CREATE()
    //}}AFX_MSG_MAP
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CInPlaceEdit message handlers

BOOL CInPlaceEdit::PreTranslateMessage(MSG* pMsg)
{
    if (pMsg->message == WM_KEYDOWN) {
        if (pMsg->wParam == VK_RETURN
                || pMsg->wParam == VK_DELETE
                || pMsg->wParam == VK_ESCAPE
                || GetKeyState(VK_CONTROL)) {
            ::TranslateMessage(pMsg);
            ::DispatchMessage(pMsg);
            return TRUE;                // DO NOT process further
        }
    }

    return CEdit::PreTranslateMessage(pMsg);
}

void CInPlaceEdit::OnKillFocus(CWnd* pNewWnd)
{
    CEdit::OnKillFocus(pNewWnd);

    CString str;
    GetWindowText(str);

    CPlayerListCtrl::SendLabelEditNotify(GetParent(), LVN_ENDLABELEDIT, m_iItem, m_iSubItem,
                                         m_bESC ? nullptr : (LPCTSTR)str);

    DestroyWindow();
}

void CInPlaceEdit::OnNcDestroy()
{
    CEdit::OnNcDestroy();

    delete this;
}

void CInPlaceEdit::OnChar(UINT nChar, UINT nRepCnt, UINT nFlags)
{
    if (nChar == VK_ESCAPE || nChar == VK_RETURN) {
        if (nChar == VK_ESCAPE) {
            m_bESC = TRUE;
        }
        GetParent()->SetFocus();
        return;
    }

    CEdit::OnChar(nChar, nRepCnt, nFlags);
}

int CInPlaceEdit::OnCreate(LPCREATESTRUCT lpCreateStruct)
{
    if (CEdit::OnCreate(lpCreateStruct) == -1) {
        return -1;
    }

    // Set the proper font
    CFont* font = GetParent()->GetFont();
    SetFont(font);

    SetWindowText(m_sInitText);
    SetFocus();
    SetSel(0, -1);
    return 0;
}

// CInPlaceFloatEdit

CInPlaceFloatEdit::CInPlaceFloatEdit(int iItem, int iSubItem, CString sInitText)
    : CInPlaceEdit(iItem, iSubItem, sInitText)
{
}

CInPlaceFloatEdit::~CInPlaceFloatEdit()
{
}

BEGIN_MESSAGE_MAP(CInPlaceFloatEdit, CInPlaceEdit)
    ON_WM_CHAR()
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CInPlaceFloatEdit message handlers

void CInPlaceFloatEdit::OnChar(UINT nChar, UINT nRepCnt, UINT nFlags)
{
    if (nChar == VK_ESCAPE || nChar == VK_RETURN) {
        if (nChar == VK_ESCAPE) {
            m_bESC = TRUE;
        }
        GetParent()->SetFocus();
        return;
    }

    if (nChar == ',') {
        nChar = '.';
    }

    if (!(nChar >= '0' && nChar <= '9' || nChar == '.' || nChar == '\b')) {
        return;
    }

    CString str;
    GetWindowText(str);

    if ((nChar == '.')  && str.Find('.') >= 0) {
        int nStartChar, nEndChar;
        GetSel(nStartChar, nEndChar);
        if (!(nStartChar < nEndChar && str.Mid(nStartChar, nEndChar - nStartChar).Find('.') >= 0)) {
            return;
        }
    }

    //CEdit::OnChar(nChar, nRepCnt, nFlags);
    DefWindowProc(WM_CHAR, nChar, MAKELONG(nRepCnt, nFlags));
}

// CInPlaceComboBox

CInPlaceComboBox::CInPlaceComboBox(int iItem, int iSubItem, CAtlList<CString>& lstItems, int nSel) : CMPCThemeComboBox()
    , m_iItem(iItem)
    , m_iSubItem(iSubItem)
{
    m_lstItems.AddTailList(&lstItems);
    m_nSel = nSel;
    m_bESC = FALSE;
}

CInPlaceComboBox::~CInPlaceComboBox()
{
}

BEGIN_MESSAGE_MAP(CInPlaceComboBox, CMPCThemeComboBox)
    //{{AFX_MSG_MAP(CInPlaceComboBox)
    ON_WM_CREATE()
    ON_WM_KILLFOCUS()
    ON_WM_CHAR()
    ON_WM_NCDESTROY()
    ON_CONTROL_REFLECT(CBN_CLOSEUP, OnCloseup)
    //}}AFX_MSG_MAP
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CInPlaceComboBox message handlers

int CInPlaceComboBox::OnCreate(LPCREATESTRUCT lpCreateStruct)
{
    if (__super::OnCreate(lpCreateStruct) == -1) {
        return -1;
    }

    // Set the proper font
    CFont* font = GetParent()->GetFont();
    SetFont(font);

    for (POSITION pos = m_lstItems.GetHeadPosition(); pos != nullptr;) {
        AddString((LPCTSTR)(m_lstItems.GetNext(pos)));
    }

    SetFocus();
    SetCurSel(m_nSel);
    return 0;
}

BOOL CInPlaceComboBox::PreTranslateMessage(MSG* pMsg)
{
    if (pMsg->message == WM_KEYDOWN) {
        if (pMsg->wParam == VK_RETURN
                || pMsg->wParam == VK_ESCAPE) {
            ::TranslateMessage(pMsg);
            ::DispatchMessage(pMsg);
            return TRUE;                // DO NOT process further
        }
    }

    return __super::PreTranslateMessage(pMsg);
}

void CInPlaceComboBox::OnKillFocus(CWnd* pNewWnd)
{
    __super::OnKillFocus(pNewWnd);

    CString str;
    GetWindowText(str);

    LV_DISPINFO dispinfo;
    dispinfo.hdr.hwndFrom = GetParent()->m_hWnd;
    dispinfo.hdr.idFrom = GetDlgCtrlID();
    dispinfo.hdr.code = LVN_ENDLABELEDIT;
    dispinfo.item.mask = LVIF_TEXT | LVIF_PARAM;
    dispinfo.item.iItem = m_iItem;
    dispinfo.item.iSubItem = m_iSubItem;
    dispinfo.item.pszText = m_bESC ? nullptr : LPTSTR((LPCTSTR)str);
    dispinfo.item.cchTextMax = str.GetLength();
    dispinfo.item.lParam = GetCurSel();
    GetParent()->GetParent()->SendMessage(WM_NOTIFY, GetParent()->GetDlgCtrlID(), (LPARAM)&dispinfo);

    PostMessage(WM_CLOSE);
}

void CInPlaceComboBox::OnChar(UINT nChar, UINT nRepCnt, UINT nFlags)
{
    if (nChar == VK_ESCAPE || nChar == VK_RETURN) {
        if (nChar == VK_ESCAPE) {
            m_bESC = TRUE;
        }
        GetParent()->SetFocus();
        return;
    }

    __super::OnChar(nChar, nRepCnt, nFlags);
}

void CInPlaceComboBox::OnNcDestroy()
{
    __super::OnNcDestroy();

    delete this;
}

void CInPlaceComboBox::OnCloseup()
{
    GetParent()->SetFocus();
}

// CInPlaceListBox

CInPlaceListBox::CInPlaceListBox(int iItem, int iSubItem, CAtlList<CString>& lstItems, int nSel)
    : m_iItem(iItem)
    , m_iSubItem(iSubItem)
{
    m_lstItems.AddTailList(&lstItems);
    m_nSel = nSel;
    m_bESC = FALSE;
}

CInPlaceListBox::~CInPlaceListBox()
{
}

BEGIN_MESSAGE_MAP(CInPlaceListBox, CListBox)
    //{{AFX_MSG_MAP(CInPlaceListBox)
    ON_WM_CREATE()
    ON_WM_KILLFOCUS()
    ON_WM_CHAR()
    ON_WM_NCDESTROY()
    //}}AFX_MSG_MAP
    ON_WM_CTLCOLOR()
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CInPlaceListBox message handlers

int CInPlaceListBox::OnCreate(LPCREATESTRUCT lpCreateStruct)
{
    if (CListBox::OnCreate(lpCreateStruct) == -1) {
        return -1;
    }

    // Set the proper font
    CFont* font = GetParent()->GetFont();
    SetFont(font);

    for (POSITION pos = m_lstItems.GetHeadPosition(); pos != nullptr;) {
        AddString((LPCTSTR)(m_lstItems.GetNext(pos)));
    }
    SetCurSel(m_nSel);
    SetFocus();
    return 0;
}

BOOL CInPlaceListBox::PreTranslateMessage(MSG* pMsg)
{
    if (pMsg->message == WM_KEYDOWN) {
        if (pMsg->wParam == VK_RETURN
                || pMsg->wParam == VK_ESCAPE) {
            ::TranslateMessage(pMsg);
            ::DispatchMessage(pMsg);
            return TRUE;                // DO NOT process further
        }
    }

    return CListBox::PreTranslateMessage(pMsg);
}

void CInPlaceListBox::OnKillFocus(CWnd* pNewWnd)
{
    CListBox::OnKillFocus(pNewWnd);

    CString str;
    GetWindowText(str);

    LV_DISPINFO dispinfo;
    dispinfo.hdr.hwndFrom = GetParent()->m_hWnd;
    dispinfo.hdr.idFrom = GetDlgCtrlID();
    dispinfo.hdr.code = LVN_ENDLABELEDIT;
    dispinfo.item.mask = LVIF_TEXT | LVIF_PARAM;
    dispinfo.item.iItem = m_iItem;
    dispinfo.item.iSubItem = m_iSubItem;
    dispinfo.item.pszText = m_bESC ? nullptr : LPTSTR((LPCTSTR)str);
    dispinfo.item.cchTextMax = str.GetLength();
    dispinfo.item.lParam = GetCurSel();
    GetParent()->GetParent()->SendMessage(WM_NOTIFY, GetParent()->GetDlgCtrlID(), (LPARAM)&dispinfo);

    PostMessage(WM_CLOSE);
}

void CInPlaceListBox::OnChar(UINT nChar, UINT nRepCnt, UINT nFlags)
{
    if (nChar == VK_ESCAPE || nChar == VK_RETURN) {
        if (nChar == VK_ESCAPE) {
            m_bESC = TRUE;
        }
        GetParent()->SetFocus();
        return;
    }

    CListBox::OnChar(nChar, nRepCnt, nFlags);
}

void CInPlaceListBox::OnNcDestroy()
{
    CListBox::OnNcDestroy();

    delete this;
}


// CMPCThemeInPlaceEdit

CMPCThemeInPlaceEdit::CMPCThemeInPlaceEdit(int iItem, int iSubItem, CString sInitText, CMPCThemeInPlaceEdit** ppSelf)
    : m_iItem(iItem)
    , m_iSubItem(iSubItem)
    , m_sInitText(sInitText)
    , m_ppSelf(ppSelf)
    , m_bESC(FALSE)
{
    if (m_ppSelf) *m_ppSelf = this;
}

CMPCThemeInPlaceEdit::~CMPCThemeInPlaceEdit()
{
}

BEGIN_MESSAGE_MAP(CMPCThemeInPlaceEdit, CMPCThemeInlineEdit)
    ON_WM_KILLFOCUS()
    ON_WM_NCDESTROY()
    ON_WM_CHAR()
    ON_WM_CREATE()
    ON_WM_MOUSEWHEEL()
END_MESSAGE_MAP()

BOOL CMPCThemeInPlaceEdit::PreTranslateMessage(MSG* pMsg)
{
    if (pMsg->message == WM_KEYDOWN) {
        if (pMsg->wParam == VK_RETURN
                || pMsg->wParam == VK_DELETE
                || pMsg->wParam == VK_ESCAPE
                || GetKeyState(VK_CONTROL)) {
            ::TranslateMessage(pMsg);
            ::DispatchMessage(pMsg);
            return TRUE;
        }
    }
    return CEdit::PreTranslateMessage(pMsg);
}

void CMPCThemeInPlaceEdit::OnKillFocus(CWnd* pNewWnd)
{
    CEdit::OnKillFocus(pNewWnd);

    CString str;
    GetWindowText(str);

    CPlayerListCtrl::SendLabelEditNotify(GetParent(), LVN_ENDLABELEDIT, m_iItem, m_iSubItem,
                                         m_bESC ? nullptr : (LPCTSTR)str);

    DestroyWindow();
}

void CMPCThemeInPlaceEdit::OnNcDestroy()
{
    if (m_ppSelf) *m_ppSelf = nullptr;
    CEdit::OnNcDestroy();
    delete this;
}

void CMPCThemeInPlaceEdit::OnChar(UINT nChar, UINT nRepCnt, UINT nFlags)
{
    if (nChar == VK_ESCAPE || nChar == VK_RETURN) {
        if (nChar == VK_ESCAPE) {
            m_bESC = TRUE;
        }
        GetParent()->SetFocus();
        return;
    }
    CEdit::OnChar(nChar, nRepCnt, nFlags);
}

int CMPCThemeInPlaceEdit::OnCreate(LPCREATESTRUCT lpCreateStruct)
{
    if (CEdit::OnCreate(lpCreateStruct) == -1) {
        return -1;
    }
    SetFont(GetParent()->GetFont());
    SetWindowText(m_sInitText);
    return 0;
}

BOOL CMPCThemeInPlaceEdit::OnMouseWheel(UINT /*nFlags*/, short /*zDelta*/, CPoint /*pt*/)
{
    return TRUE; // swallow; prevent propagation to main frame volume handler
}

// CPlayerListCtrl

IMPLEMENT_DYNAMIC(CPlayerListCtrl, CMPCThemePlayerListCtrl)
CPlayerListCtrl::CPlayerListCtrl(int tStartEditingDelay)
    : m_nItemClicked(-1)
    , m_nSubItemClicked(-1)
    , m_tStartEditingDelay(tStartEditingDelay)
    , m_nTimerID(0)
    , m_fInPlaceDirty(false)
    , inPlaceControl(false)
    , m_pVirtualEdit(nullptr)
{
}


CPlayerListCtrl::~CPlayerListCtrl()
{
}

int CPlayerListCtrl::HitTestEx(const CPoint& point, int* col) const
{
    if (col) {
        *col = 0;
    }

    int row;
    if ((GetStyle() & LVS_TYPEMASK) != LVS_REPORT) {
        row = HitTest(point, nullptr);//adipose to keep from breaking list view, use point.x
        return row;
    } else {
        row = HitTest(CPoint(0, point.y), nullptr); //in report mode x=0 is ok?
    }

    int nColumnCount = ((CHeaderCtrl*)GetDlgItem(0))->GetItemCount();

    for (int top = GetTopIndex(), bottom = GetBottomIndex(); top <= bottom; top++) {
        CRect r;
        GetItemRect(top, &r, LVIR_BOUNDS);

        if (r.top <= point.y && point.y < r.bottom) {
            for (int colnum = 0; colnum < nColumnCount; colnum++) {
                int colwidth = GetColumnWidth(colnum);

                if (point.x >= r.left && point.x <= (r.left + colwidth)) {
                    if (col) {
                        *col = colnum;
                    }
                    return top;
                }

                r.left += colwidth;
            }
        }
    }

    return -1;
}

int CPlayerListCtrl::GetBottomIndex() const
{
    CRect r;
    GetClientRect(r);

    int nBottomIndex = GetTopIndex() + GetCountPerPage() - 1;

    if (nBottomIndex >= GetItemCount()) {
        nBottomIndex = GetItemCount() - 1;
    } else if (nBottomIndex < GetItemCount()) {
        CRect br;
        GetItemRect(nBottomIndex, br, LVIR_BOUNDS);

        if (br.bottom < r.bottom) {
            nBottomIndex++;
        }
    }

    return nBottomIndex;
}

CImageList* CPlayerListCtrl::CreateDragImageEx(LPPOINT lpPoint)
{
    if (GetSelectedCount() <= 0) {
        return nullptr;
    }

    CRect cSingleRect, cCompleteRect(0, 0, 0, 0);
    GetClientRect(cSingleRect);
    int nWidth = cSingleRect.Width();

    // Start and Stop index in view area
    int nIndex = GetTopIndex() - 1;
    int nBottomIndex = GetBottomIndex();

    // Determine the size of the drag image (limited for
    // rows visible and Client width)
    while ((nIndex = GetNextItem(nIndex, LVNI_SELECTED)) != -1 && nIndex <= nBottomIndex) {
        GetItemRect(nIndex, cSingleRect, LVIR_BOUNDS);
        /*
        CRect r;
        GetItemRect(nIndex, r, LVIR_LABEL);
        cSingleRect.left = r.left;
        */
        if (cSingleRect.left < 0) {
            cSingleRect.left = 0;
        }
        if (cSingleRect.right > nWidth) {
            cSingleRect.right = nWidth;
        }

        cCompleteRect |= cSingleRect;
    }

    //
    // Create bitmap in memory DC
    //
    CClientDC cDc(this);
    CDC cMemDC;
    CBitmap cBitmap;

    if (!cMemDC.CreateCompatibleDC(&cDc)) {
        return nullptr;
    }

    if (!cBitmap.CreateCompatibleBitmap(&cDc, cCompleteRect.Width(), cCompleteRect.Height())) {
        return nullptr;
    }

    CBitmap* pOldMemDCBitmap = cMemDC.SelectObject(&cBitmap);
    // Here green is used as mask color
    cMemDC.FillSolidRect(0, 0, cCompleteRect.Width(), cCompleteRect.Height(), RGB(0, 255, 0));

    // Paint each DragImage in the DC
    nIndex = GetTopIndex() - 1;
    while ((nIndex = GetNextItem(nIndex, LVNI_SELECTED)) != -1 && nIndex <= nBottomIndex) {
        CPoint pt;
        CImageList* pSingleImageList = CreateDragImage(nIndex, &pt);

        if (pSingleImageList) {
            GetItemRect(nIndex, cSingleRect, LVIR_BOUNDS);

            pSingleImageList->Draw(&cMemDC,
                                   0,
                                   CPoint(cSingleRect.left - cCompleteRect.left, cSingleRect.top - cCompleteRect.top),
                                   ILD_MASK);

            pSingleImageList->DeleteImageList();
            delete pSingleImageList;
        }
    }

    cMemDC.SelectObject(pOldMemDCBitmap);

    //
    // Create the image list with the merged drag images
    //
    CImageList* pCompleteImageList = DEBUG_NEW CImageList;

    pCompleteImageList->Create(cCompleteRect.Width(),
                               cCompleteRect.Height(),
                               ILC_COLOR | ILC_MASK, 0, 1);

    // Here green is used as mask color
    pCompleteImageList->Add(&cBitmap, RGB(0, 255, 0));

    //
    // as an optional service:
    // Find the offset of the current mouse cursor to the image list
    // this we can use in BeginDrag()
    //
    if (lpPoint) {
        lpPoint->x = cCompleteRect.left;
        lpPoint->y = cCompleteRect.top;
    }

    return pCompleteImageList;
}

bool CPlayerListCtrl::PrepareInPlaceControl(int nRow, int nCol, CRect& rect)
{
    CHeaderCtrl* pHeaderCtrl = GetHeaderCtrl();
    if (!pHeaderCtrl || nCol >= pHeaderCtrl->GetItemCount() || GetColumnWidth(nCol) < 5) {
        return false;
    }

    // Compute the position of the cell
    CRect rcColHeader;
    pHeaderCtrl->GetItemRect(nCol, &rcColHeader);

    GetItemRect(nRow, &rect, LVIR_BOUNDS);

    rect.left += rcColHeader.left;
    rect.right = rect.left + rcColHeader.Width();

    // Ensure the cell is visible by scrolling if necessary
    CRect rcClient;
    GetClientRect(&rcClient);
    rcClient.TopLeft().Offset(0, rcColHeader.Height());

    CSize scroll;
    if (rect.left >= rcClient.right) {
        scroll.cx = rcClient.right - rect.right;
    } else if (rect.right <= rect.left) {
        scroll.cx = rcClient.left - rect.left;
    }
    if (rect.top >= rcClient.bottom) {
        scroll.cy = rcClient.bottom - rect.bottom;
    } else if (rect.bottom <= rcClient.top) {
        scroll.cy = rcClient.top - rect.top;
    }

    if (scroll.cx || scroll.cy) {
        rect.OffsetRect(scroll);
        Scroll(-scroll);
    }

    // The cell can be partially shown so we crop what is outside of the visible part
    rect &= rcClient;

    rect.DeflateRect(1, 0, 0, 1);
    inPlaceControl = true;
    inPlaceControlRect = rect;
    ClientToScreen(inPlaceControlRect);
    return true;
}

CWinHotkeyCtrl* CPlayerListCtrl::ShowInPlaceWinHotkey(int nItem, int nCol)
{
    CRect rect;
    if (!PrepareInPlaceControl(nItem, nCol, rect)) {
        return nullptr;
    }

    DWORD dwStyle = /*WS_BORDER|*/WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL | ES_LEFT;

    CWinHotkeyCtrl* pWinHotkey = DEBUG_NEW CInPlaceWinHotkey(nItem, nCol, GetItemText(nItem, nCol));
    pWinHotkey->Create(dwStyle, rect, this, IDC_WINHOTKEY1);

    m_fInPlaceDirty = false;

    return pWinHotkey;
}

void CPlayerListCtrl::OnEnChangeWinHotkey1()
{
    m_fInPlaceDirty = true;
}

CEdit* CPlayerListCtrl::ShowInPlaceEdit(int nItem, int nCol)
{
    CRect rect;
    if (!PrepareInPlaceControl(nItem, nCol, rect)) {
        return nullptr;
    }

    DWORD dwStyle = /*WS_BORDER|*/WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL;

    LV_COLUMN lvcol;
    lvcol.mask = LVCF_FMT;
    GetColumn(nCol, &lvcol);
    dwStyle |= (lvcol.fmt & LVCFMT_JUSTIFYMASK) == LVCFMT_LEFT ? ES_LEFT
               : (lvcol.fmt & LVCFMT_JUSTIFYMASK) == LVCFMT_RIGHT ? ES_RIGHT
               : ES_CENTER;

    CEdit* pEdit = DEBUG_NEW CInPlaceEdit(nItem, nCol, GetItemText(nItem, nCol));
    pEdit->Create(dwStyle, rect, this, IDC_EDIT1);

    m_fInPlaceDirty = false;

    return pEdit;
}

void CPlayerListCtrl::AdjustVirtualEditPos(int xOffset, int maxWidth)
{
    if (!m_pVirtualEdit) return;
    CRect r;
    m_pVirtualEdit->GetWindowRect(r);
    ScreenToClient(r);
    r.left += xOffset;
    if (maxWidth > 0 && r.left + maxWidth < r.right) {
        r.right = r.left + maxWidth;
    }
    m_pVirtualEdit->MoveWindow(r);
    m_pVirtualEdit->setOverridePos(xOffset, maxWidth > 0 ? maxWidth : -1);
}

void CPlayerListCtrl::StartVirtualEditLabel(int nItem, int nSubItem)
{
    // Cancel any lingering edit (should already be gone via SetFocus at top of OnLButtonDown)
    if (m_pVirtualEdit) {
        SetFocus();
    }

    CString text = GetItemText(nItem, nSubItem); // triggers LVN_GETDISPINFO for virtual lists

    CRect rect;
    GetItemRect(nItem, &rect, LVIR_LABEL);

    // Create hidden — parent adjusts rect/font in response to LVN_BEGINLABELEDIT
    m_pVirtualEdit = DEBUG_NEW CMPCThemeInPlaceEdit(nItem, nSubItem, text, &m_pVirtualEdit);
    m_pVirtualEdit->Create(WS_CHILD | WS_BORDER | ES_AUTOHSCROLL, rect, this, 0);

    // Notify parent; edit is already created so parent can call GetVirtualEditCtrl()
    if (!SendLabelEditNotify(this, LVN_BEGINLABELEDIT, nItem, nSubItem, text)) {
        // Parent denied editing
        if (m_pVirtualEdit) {
            m_pVirtualEdit->DestroyWindow(); // → OnNcDestroy → m_pVirtualEdit = nullptr
        }
        return;
    }

    // Parent allowed; show and focus
    if (m_pVirtualEdit) {
        m_pVirtualEdit->ShowWindow(SW_SHOW);
        m_pVirtualEdit->SetFocus();
        m_pVirtualEdit->SetSel(0, -1);
    }
}

CEdit* CPlayerListCtrl::ShowInPlaceFloatEdit(int nItem, int nCol)
{
    CRect rect;
    if (!PrepareInPlaceControl(nItem, nCol, rect)) {
        return nullptr;
    }

    DWORD dwStyle = /*WS_BORDER|*/WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL | ES_RIGHT;

    CEdit* pFloatEdit = DEBUG_NEW CInPlaceFloatEdit(nItem, nCol, GetItemText(nItem, nCol));
    pFloatEdit->Create(dwStyle, rect, this, IDC_EDIT1);

    m_fInPlaceDirty = false;

    return pFloatEdit;
}

CComboBox* CPlayerListCtrl::ShowInPlaceComboBox(int nItem, int nCol, CAtlList<CString>& lstItems, int nSel, bool bShowDropDown)
{
    CRect rect;
    if (!PrepareInPlaceControl(nItem, nCol, rect)) {
        return nullptr;
    }

    DWORD dwStyle = /*WS_BORDER|*/WS_CHILD | WS_VISIBLE | WS_VSCROLL /*|WS_HSCROLL*/
                                  | CBS_DROPDOWNLIST | CBS_DISABLENOSCROLL/*|CBS_NOINTEGRALHEIGHT*/;
    CComboBox* pComboBox = DEBUG_NEW CInPlaceComboBox(nItem, nCol, lstItems, nSel);
    pComboBox->Create(dwStyle, rect, this, IDC_COMBO1);

    CorrectComboListWidth(*pComboBox);

    int width = GetColumnWidth(nCol);
    if (pComboBox->GetDroppedWidth() < width) {
        pComboBox->SetDroppedWidth(width);
    }

    if (bShowDropDown) {
        pComboBox->ShowDropDown();
    }

    m_fInPlaceDirty = false;

    return pComboBox;
}

CListBox* CPlayerListCtrl::ShowInPlaceListBox(int nItem, int nCol, CAtlList<CString>& lstItems, int nSel)
{
    CRect rect;
    if (!PrepareInPlaceControl(nItem, nCol, rect)) {
        return nullptr;
    }

    DWORD dwStyle = WS_BORDER | WS_CHILD | WS_VISIBLE | WS_VSCROLL/*|WS_HSCROLL*/ | LBS_NOTIFY;
    CListBox* pListBox = DEBUG_NEW CInPlaceListBox(nItem, nCol, lstItems, nSel);
    pListBox->Create(dwStyle, rect, this, IDC_LIST1);

    CRect ir;
    GetItemRect(m_nItemClicked, &ir, LVIR_BOUNDS);

    pListBox->SetItemHeight(-1, ir.Height());

    CDC* pDC = pListBox->GetDC();
    CFont* pWndFont = GetFont();
    pDC->SelectObject(pWndFont);
    int width = GetColumnWidth(nCol);
    POSITION pos = lstItems.GetHeadPosition();
    while (pos) {
        int w = pDC->GetTextExtent(lstItems.GetNext(pos)).cx + 16;
        if (width < w) {
            width = w;
        }
    }
    ReleaseDC(pDC);

    CRect r;
    pListBox->GetWindowRect(r);
    ScreenToClient(r);
    r.top = ir.bottom;
    r.bottom = r.top + pListBox->GetItemHeight(0) * (pListBox->GetCount() + 1);
    r.right = r.left + width;
    pListBox->MoveWindow(r);

    m_fInPlaceDirty = false;

    return pListBox;
}

BEGIN_MESSAGE_MAP(CPlayerListCtrl, CMPCThemePlayerListCtrl)
    ON_WM_SIZE()
    ON_WM_VSCROLL()
    ON_WM_HSCROLL()
    ON_WM_MOUSEWHEEL()
    ON_WM_LBUTTONDOWN()
    ON_WM_TIMER()
    ON_WM_LBUTTONDBLCLK()
    ON_NOTIFY_REFLECT(LVN_MARQUEEBEGIN, OnLvnMarqueeBegin)
    ON_NOTIFY_REFLECT_EX(LVN_BEGINDRAG, OnLvnBegindrag)
    ON_NOTIFY_REFLECT(LVN_INSERTITEM, OnLvnInsertitem)
    ON_NOTIFY_REFLECT(LVN_DELETEITEM, OnLvnDeleteitem)
    ON_EN_CHANGE(IDC_EDIT1, OnEnChangeEdit1)
    ON_EN_CHANGE(IDC_WINHOTKEY1, OnEnChangeWinHotkey1)
    ON_CBN_DROPDOWN(IDC_COMBO1, OnCbnDropdownCombo1)
    ON_CBN_SELENDOK(IDC_COMBO1, OnCbnSelendokCombo1)
    ON_LBN_SELCHANGE(IDC_LIST1, OnLbnSelChangeList1)
    ON_NOTIFY_EX(HDN_ITEMCHANGINGA, 0, OnHdnItemchanging)
    ON_NOTIFY_EX(HDN_ITEMCHANGINGW, 0, OnHdnItemchanging)
    ON_NOTIFY_EX(TTN_NEEDTEXT, 0, OnToolTipNotify)
    ON_WM_XBUTTONDOWN()
    ON_WM_XBUTTONUP()
    ON_WM_XBUTTONDBLCLK()
    ON_WM_SETCURSOR()
    ON_NOTIFY_REFLECT_EX(LVN_ENDLABELEDIT, &CPlayerListCtrl::OnLvnEndlabeledit)
    ON_MESSAGE(LVM_EDITLABEL, OnLvmEditLabel)
END_MESSAGE_MAP()

// CPlayerListCtrl message handlers

void CPlayerListCtrl::OnSize(UINT nType, int cx, int cy)
{
    if (m_pVirtualEdit) {
        SetFocus();
    }
    __super::OnSize(nType, cx, cy);
}

void CPlayerListCtrl::OnVScroll(UINT nSBCode, UINT nPos, CScrollBar* pScrollBar)
{
    if (GetFocus() != this) {
        SetFocus();
    }
    CListCtrl::OnVScroll(nSBCode, nPos, pScrollBar);
}

void CPlayerListCtrl::OnHScroll(UINT nSBCode, UINT nPos, CScrollBar* pScrollBar)
{
    if (GetFocus() != this) {
        SetFocus();
    }
    CListCtrl::OnHScroll(nSBCode, nPos, pScrollBar);
}

BOOL CPlayerListCtrl::OnMouseWheel(UINT nFlags, short zDelta, CPoint pt)
{
    if (m_pVirtualEdit && ::IsWindow(m_pVirtualEdit->GetSafeHwnd())) {
        return TRUE;
    }
    if (GetFocus() != this) {
        SetFocus();
    }
    return CListCtrl::OnMouseWheel(nFlags, zDelta, pt);
}

LRESULT CPlayerListCtrl::SendLabelEditNotify(CWnd* pList, UINT code, int nItem, int nSubItem, LPCTSTR pszText)
{
    LV_DISPINFO dispinfo = {};
    dispinfo.hdr.hwndFrom = pList->m_hWnd;
    dispinfo.hdr.idFrom = pList->GetDlgCtrlID();
    dispinfo.hdr.code = code;
    dispinfo.item.iItem = nItem;
    dispinfo.item.iSubItem = nSubItem;
    if (pszText) {
        dispinfo.item.mask = LVIF_TEXT;
        dispinfo.item.pszText = const_cast<LPTSTR>(pszText);
        dispinfo.item.cchTextMax = (int)_tcslen(pszText);
    }
    return pList->GetParent()->SendMessage(WM_NOTIFY, pList->GetDlgCtrlID(), (LPARAM)&dispinfo);
}

LRESULT CPlayerListCtrl::OnLvmEditLabel(WPARAM wParam, LPARAM)
{
    int nItem = (int)wParam;
    if (nItem < 0) {
        return 0;
    }
    if (GetStyle() & LVS_OWNERDATA) {
        StartVirtualEditLabel(nItem, m_nSubItemClicked);
    } else {
        if (SendLabelEditNotify(this, LVN_BEGINLABELEDIT, nItem, m_nSubItemClicked)) {
            SendLabelEditNotify(this, LVN_DOLABELEDIT, nItem, m_nSubItemClicked);
        }
    }
    return 0;
}

void CPlayerListCtrl::OnLButtonDown(UINT nFlags, CPoint point)
{
    CListCtrl::OnLButtonDown(nFlags, point);

    if (GetFocus() != this) {
        SetFocus();
    }

    if (m_nTimerID) {
        KillTimer(m_nTimerID);
        m_nTimerID = 0;
    }

    int m_nItemClickedNow, m_nSubItemClickedNow;

    if ((m_nItemClickedNow = HitTestEx(point, &m_nSubItemClickedNow)) < 0) {
        m_nItemClicked = -1;
    } else if (m_nItemClicked == m_nItemClickedNow /*&& m_nSubItemClicked == m_nSubItemClickedNow*/) {
        m_nSubItemClicked = m_nSubItemClickedNow;

        m_nTimerID = SetTimer(1, std::max((UINT)m_tStartEditingDelay, GetDoubleClickTime()), nullptr);
    } else {
        m_nItemClicked = m_nItemClickedNow;
        m_nSubItemClicked = m_nSubItemClickedNow;

        SetItemState(m_nItemClicked, LVIS_SELECTED | LVIS_FOCUSED, LVIS_SELECTED | LVIS_FOCUSED);
    }
}

void CPlayerListCtrl::OnTimer(UINT_PTR nIDEvent)
{
    if (nIDEvent == m_nTimerID) {
        KillTimer(m_nTimerID);
        m_nTimerID = 0;

        UINT flag = LVIS_FOCUSED;
        if ((GetItemState(m_nItemClicked, flag) & flag) == flag && m_nSubItemClicked >= 0) {
            SendMessage(LVM_EDITLABEL, m_nItemClicked, 0);
        }
    } else if (nIDEvent == 43) {
        // CListCtrl does really strange things on this timer.
        // For example, when using mouse scroll right after mouse left button was clicked,
        // this timer scrolls the control back to the clicked item after a while.
        // There is no known problems with simply killing this timer.
        VERIFY(KillTimer(nIDEvent));
    } else {
        __super::OnTimer(nIDEvent);
    }
}

void CPlayerListCtrl::OnLButtonDblClk(UINT nFlags, CPoint point)
{
    if (m_nTimerID) {
        KillTimer(m_nTimerID);
        m_nTimerID = 0;
    }
    m_nItemClicked = -1;

    CListCtrl::OnLButtonDblClk(nFlags, point);
}

void CPlayerListCtrl::OnLvnMarqueeBegin(NMHDR* pNMHDR, LRESULT* pResult)
{
    LPNMLISTVIEW pNMLV = reinterpret_cast<LPNMLISTVIEW>(pNMHDR);
    UNREFERENCED_PARAMETER(pNMLV);
    *pResult = 1;
}

BOOL CPlayerListCtrl::OnLvnBegindrag(NMHDR* pNMHDR, LRESULT* pResult)
{
    if (m_nTimerID) {
        KillTimer(m_nTimerID);
        m_nTimerID = 0;
    }
    m_nItemClicked = -1;
    *pResult = 0;
    return FALSE; // let parent handle the drag
}

void CPlayerListCtrl::OnLvnInsertitem(NMHDR* pNMHDR, LRESULT* pResult)
{
    LPNMLISTVIEW pNMLV = reinterpret_cast<LPNMLISTVIEW>(pNMHDR);
    UNREFERENCED_PARAMETER(pNMLV);
    m_nItemClicked = -1;

    *pResult = 0;
}

void CPlayerListCtrl::OnLvnDeleteitem(NMHDR* pNMHDR, LRESULT* pResult)
{
    LPNMLISTVIEW pNMLV = reinterpret_cast<LPNMLISTVIEW>(pNMHDR);
    UNREFERENCED_PARAMETER(pNMLV);
    m_nItemClicked = -1;
    *pResult = 0;
}

void CPlayerListCtrl::OnEnChangeEdit1()
{
    m_fInPlaceDirty = true;
}

void CPlayerListCtrl::OnCbnDropdownCombo1()
{
    CMPCThemeComboBox* pCombo = (CMPCThemeComboBox*)GetDlgItem(IDC_COMBO1);

    CRect ir;
    GetItemRect(m_nItemClicked, &ir, LVIR_BOUNDS);

    CRect r;
    pCombo->GetWindowRect(r);
    ScreenToClient(r);
    r.bottom = r.top + ir.Height() + pCombo->GetItemHeight(0) * 10;
    pCombo->MoveWindow(r);
}

void CPlayerListCtrl::OnCbnSelendokCombo1()
{
    m_fInPlaceDirty = true;
}

void CPlayerListCtrl::OnLbnSelChangeList1()
{
    m_fInPlaceDirty = true;
    SetFocus();
}

BOOL CPlayerListCtrl::OnHdnItemchanging(UINT id, NMHDR* pNMHDR, LRESULT* pResult)
{
    LPNMHEADER phdr = reinterpret_cast<LPNMHEADER>(pNMHDR);
    UNREFERENCED_PARAMETER(phdr);
    //  SetFocus();
    *pResult = 0;
    return FALSE;
}

INT_PTR CPlayerListCtrl::OnToolHitTest(CPoint point, TOOLINFO* pTI) const
{
    int col;
    int row = HitTestEx(point, &col);
    if (row == -1) {
        return -1;
    }

    CHeaderCtrl* pHeader = (CHeaderCtrl*)GetDlgItem(0);
    if (nullptr == pHeader) {
        return -1;    //no header ctrl
    }
    int nColumnCount = pHeader->GetItemCount();

    CRect rect;
    GetItemRect(row, &rect, LVIR_BOUNDS);

    for (int colnum = 0; colnum < nColumnCount; colnum++) {
        int colwidth = GetColumnWidth(colnum);

        if (colnum == col) {
            rect.right = rect.left + colwidth;
            break;
        }

        rect.left += colwidth;
    }

    pTI->hwnd = m_hWnd;
    pTI->uId = (UINT_PTR(row) << 10) + (col & 0x3ff) + 1;
    pTI->lpszText = LPSTR_TEXTCALLBACK;
    pTI->rect = rect;

    return pTI->uId;
}

BOOL CPlayerListCtrl::OnToolTipNotify(UINT id, NMHDR* pNMHDR, LRESULT* pResult)
{
    TOOLTIPTEXT* pTTT = (TOOLTIPTEXT*)pNMHDR;

    UINT_PTR nID = pNMHDR->idFrom;

    if (pTTT->uFlags & TTF_IDISHWND) {
        // idFrom is actually the HWND of the tool
        nID = ::GetDlgCtrlID((HWND)nID);
    }

    if (nID == 0) {   // Notification in NT from automatically
        return FALSE; // created tooltip
    }

    pTTT->lParam = (LPARAM)m_hWnd;

    *pResult = 0;

    return !!GetParent()->SendMessage(WM_NOTIFY, id, (LPARAM)pNMHDR);
}

void CPlayerListCtrl::OnXButtonDown(UINT nFlags, UINT nButton, CPoint point)
{
    if (CWnd* pParent = GetParent()) {
        MapWindowPoints(pParent, &point, 1);
        pParent->SendMessage(WM_XBUTTONDOWN, MAKEWPARAM(nFlags, nButton), MAKELPARAM(point.x, point.y));
    }
}

void CPlayerListCtrl::OnXButtonUp(UINT nFlags, UINT nButton, CPoint point)
{
    if (CWnd* pParent = GetParent()) {
        MapWindowPoints(pParent, &point, 1);
        pParent->SendMessage(WM_XBUTTONUP, MAKEWPARAM(nFlags, nButton), MAKELPARAM(point.x, point.y));
    }
}

void CPlayerListCtrl::OnXButtonDblClk(UINT nFlags, UINT nButton, CPoint point)
{
    if (CWnd* pParent = GetParent()) {
        MapWindowPoints(pParent, &point, 1);
        pParent->SendMessage(WM_XBUTTONDBLCLK, MAKEWPARAM(nFlags, nButton), MAKELPARAM(point.x, point.y));
    }
}

BOOL CPlayerListCtrl::OnSetCursor(CWnd* pWnd, UINT nHitTest, UINT message) {
    if (inPlaceControl) {
        POINT p;
        GetCursorPos(&p);
        if (inPlaceControlRect.PtInRect(p)) {
            return FALSE;
        }
    }
    return CListCtrl::OnSetCursor(pWnd, nHitTest, message);
}


BOOL CPlayerListCtrl::OnLvnEndlabeledit(NMHDR* pNMHDR, LRESULT* pResult) {
    inPlaceControl = false;
    return FALSE;
}

int CPlayerListCtrl::InsertColumn(_In_ int nCol, _In_z_ LPCWSTR lpszColumnHeading,
    _In_ int nFormat, _In_ int nWidth, _In_ int nSubItem, _In_ int nMinWidth) {
    nCol = __super::InsertColumn(nCol, lpszColumnHeading, nFormat, nWidth, nSubItem);
    if (nCol != -1 && nMinWidth > 0) {
        LVCOLUMNW col;
        col.mask = LVCF_MINWIDTH;
        col.cxMin = nMinWidth;
        SetColumn(nCol, &col);
        SetExtendedStyle(GetExtendedStyle() | LVS_EX_COLUMNSNAPPOINTS);
    }

    return nCol;
}
