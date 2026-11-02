/*
 * (C) 2009-2018 see Authors.txt
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
#include "resource.h"
#include "EditListEditor.h"
#include "DSUtil.h"


CClip::CClip()
    : m_rtIn(_I64_MIN)
    , m_rtOut(_I64_MIN)
{
}

CClip::~CClip()
{
}

void CClip::SetIn(LPCTSTR strVal)
{
    m_rtIn = StringToReftime(strVal);
}

void CClip::SetOut(LPCTSTR strVal)
{
    m_rtOut = StringToReftime(strVal);
}

void CClip::SetIn(REFERENCE_TIME rtVal)
{
    m_rtIn = rtVal;
    if (m_rtIn > m_rtOut) {
        m_rtOut = _I64_MIN;
    }
};

void CClip::SetOut(REFERENCE_TIME rtVal)
{
    m_rtOut = rtVal;
    if (m_rtIn > m_rtOut) {
        m_rtIn = _I64_MIN;
    }
};

CString CClip::GetIn() const
{
    return m_rtIn == _I64_MIN ? CString() : ReftimeToString(m_rtIn);
}

CString CClip::GetOut() const
{
    return m_rtOut == _I64_MIN ? CString() : ReftimeToString(m_rtOut);
}

IMPLEMENT_DYNAMIC(CEditListEditor, CPlayerBar)
CEditListEditor::CEditListEditor()
    : m_curPos(nullptr)
    , m_bDragging(FALSE)
    , m_nDragIndex(-1)
    , m_nDropIndex(-1)
    , m_pDragImage(nullptr)
    , m_bFileOpen(false)
{
}

CEditListEditor::~CEditListEditor()
{
    SaveEditListToFile();
}

BEGIN_MESSAGE_MAP(CEditListEditor, CPlayerBar)
    ON_WM_SIZE()
    ON_NOTIFY(LVN_ITEMCHANGED, IDC_EDITLIST, OnLvnItemchanged)
    ON_NOTIFY(LVN_KEYDOWN, IDC_EDITLIST, OnLvnKeyDown)
    ON_WM_DRAWITEM()
    ON_NOTIFY(LVN_BEGINDRAG, IDC_EDITLIST, OnBeginDrag)
    ON_WM_MOUSEMOVE()
    ON_WM_LBUTTONUP()
    ON_WM_TIMER()
    ON_NOTIFY(LVN_BEGINLABELEDIT, IDC_EDITLIST, OnBeginlabeleditList)
    ON_NOTIFY(LVN_DOLABELEDIT, IDC_EDITLIST, OnDolabeleditList)
    ON_NOTIFY(LVN_ENDLABELEDIT, IDC_EDITLIST, OnEndlabeleditList)
END_MESSAGE_MAP()

BOOL CEditListEditor::Create(CWnd* pParentWnd, UINT defDockBarID)
{
    if (!__super::Create(ResStr(IDS_EDIT_LIST_EDITOR), pParentWnd, ID_VIEW_EDITLISTEDITOR, defDockBarID, _T("Edit List Editor"))) {
        return FALSE;
    }

    m_stUsers.Create(_T("User :"), WS_VISIBLE | WS_CHILD, CRect(5, 5, 100, 21), this, 0);
    m_cbUsers.Create(WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST, CRect(90, 0, 260, 21), this, 0);
    FillCombo(_T("Users.txt"), m_cbUsers, false);

    m_stHotFolders.Create(_T("Hot folder :"), WS_VISIBLE | WS_CHILD, CRect(5, 35, 100, 51), this, 0);
    m_cbHotFolders.Create(WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST, CRect(90, 30, 260, 21), this, 0);
    FillCombo(_T("HotFolders.txt"), m_cbHotFolders, true);

    m_list.CreateEx(
        WS_EX_DLGMODALFRAME | WS_EX_CLIENTEDGE,
        WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_CLIPCHILDREN | WS_TABSTOP
        | LVS_OWNERDRAWFIXED
        | LVS_REPORT | LVS_SINGLESEL | LVS_AUTOARRANGE | LVS_NOSORTHEADER,
        CRect(0, 0, 100, 100), this, IDC_EDITLIST);

    m_list.SetExtendedStyle(m_list.GetExtendedStyle() | LVS_EX_FULLROWSELECT | LVS_EX_DOUBLEBUFFER);

    m_list.InsertColumn(COL_IN, _T("Nb."), LVCFMT_LEFT, 35);
    m_list.InsertColumn(COL_IN, _T("In"), LVCFMT_LEFT, 100);
    m_list.InsertColumn(COL_OUT, _T("Out"), LVCFMT_LEFT, 100);
    m_list.InsertColumn(COL_NAME, _T("Name"), LVCFMT_LEFT, 150);

    m_fakeImageList.Create(1, 16, ILC_COLOR4, 10, 10);
    m_list.SetImageList(&m_fakeImageList, LVSIL_SMALL);

    return TRUE;
}

void CEditListEditor::ReloadTranslatableResources()
{
    SetWindowText(ResStr(IDS_EDIT_LIST_EDITOR));
}

void CEditListEditor::OnSize(UINT nType, int cx, int cy)
{
    __super::OnSize(nType, cx, cy);

    ResizeListColumn();
}

void CEditListEditor::ResizeListColumn()
{
    if (::IsWindow(m_list.m_hWnd)) {
        CRect r;
        GetClientRect(r);
        r.DeflateRect(2, 2);

        r.top += 60;
        m_list.SetRedraw(FALSE);
        m_list.MoveWindow(r);
        m_list.GetClientRect(r);
        m_list.SetRedraw(TRUE);
    }
}

void CEditListEditor::SaveEditListToFile()
{
    if ((m_bFileOpen || !m_editList.IsEmpty()) && !m_strFileName.IsEmpty()) {
        CStdioFile editListFile;
        if (editListFile.Open(m_strFileName, CFile::modeCreate | CFile::modeWrite)) {
            CString strLine;
            int nIndex;
            CString strUser;
            CString strHotFolders;

            nIndex = m_cbUsers.GetCurSel();
            if (nIndex >= 0) {
                m_cbUsers.GetLBText(nIndex, strUser);
            }

            nIndex = m_cbHotFolders.GetCurSel();
            if (nIndex >= 0) {
                m_cbHotFolders.GetLBText(nIndex, strHotFolders);
            }

            POSITION pos = m_editList.GetHeadPosition();
            for (int i = 0; pos; i++, m_editList.GetNext(pos)) {
                CClip&  CurClip = m_editList.GetAt(pos);

                if (CurClip.HaveIn() && CurClip.HaveOut()) {
                    strLine.Format(_T("%s\t%s\t%s\t%s\t%s\n"), CurClip.GetIn().GetString(), CurClip.GetOut().GetString(), CurClip.GetName().GetString(), strUser.GetString(), strHotFolders.GetString());
                    editListFile.WriteString(strLine);
                }
            }

            editListFile.Close();
        }
    }
}

void CEditListEditor::CloseFile()
{
    SaveEditListToFile();
    m_editList.RemoveAll();
    m_list.DeleteAllItems();
    m_curPos = nullptr;
    m_strFileName.Empty();
    m_bFileOpen = false;
    m_cbHotFolders.SetCurSel(0);
}

void CEditListEditor::OpenFile(LPCTSTR lpFileName)
{
    CString strLine;
    CStdioFile editListFile;
    CString strUser;
    CString strHotFolders;

    CloseFile();
    m_strFileName.Format(_T("%s.edl"), lpFileName);

    if (editListFile.Open(m_strFileName, CFile::modeRead)) {
        m_bFileOpen = true;
        while (editListFile.ReadString(strLine)) {
            //int nPos = 0;
            CString strIn;      //  = strLine.Tokenize(_T(" \t"), nPos);
            CString strOut;     //  = strLine.Tokenize(_T(" \t"), nPos);
            CString strName;    //  = strLine.Tokenize(_T(" \t"), nPos);

            AfxExtractSubString(strIn, strLine, 0, _T('\t'));
            AfxExtractSubString(strOut, strLine, 1, _T('\t'));
            AfxExtractSubString(strName, strLine, 2, _T('\t'));
            if (strUser.IsEmpty()) {
                AfxExtractSubString(strUser, strLine, 3, _T('\t'));
                SelectCombo(strUser, m_cbUsers);
            }
            if (strHotFolders.IsEmpty()) {
                AfxExtractSubString(strHotFolders, strLine, 4, _T('\t'));
                SelectCombo(strHotFolders, m_cbHotFolders);
            }

            if (!strIn.IsEmpty() && !strOut.IsEmpty()) {
                CClip NewClip;
                NewClip.SetIn(strIn);
                NewClip.SetOut(strOut);
                NewClip.SetName(strName);

                InsertClip(nullptr, NewClip);
            }
        }

        editListFile.Close();
    } else {
        m_bFileOpen = false;
    }

    if (m_nameList.IsEmpty()) {
        CStdioFile nameFile;
        CString str;
        if (nameFile.Open(_T("EditListNames.txt"), CFile::modeRead)) {
            while (nameFile.ReadString(str)) {
                m_nameList.Add(str);
            }
            nameFile.Close();
        }
    }
}

void CEditListEditor::SetIn(REFERENCE_TIME rtIn)
{
    if (m_curPos) {
        CClip& curClip = m_editList.GetAt(m_curPos);

        curClip.SetIn(rtIn);
        m_list.Invalidate();
    }
}

void CEditListEditor::SetOut(REFERENCE_TIME rtOut)
{
    if (m_curPos) {
        CClip& curClip = m_editList.GetAt(m_curPos);

        curClip.SetOut(rtOut);
        m_list.Invalidate();
    }
}

void CEditListEditor::NewClip(REFERENCE_TIME rtVal)
{
    CClip newClip;
    newClip.SetIn(rtVal);

    if (m_curPos) {
        CClip& curClip = m_editList.GetAt(m_curPos);

        if (curClip.HaveIn()) {
            if (!curClip.HaveOut()) {
                curClip.SetOut(rtVal);
            }
        }
    }
    m_curPos = InsertClip(m_curPos, newClip);
    m_list.Invalidate();
}

void CEditListEditor::Save()
{
    SaveEditListToFile();
}

POSITION CEditListEditor::InsertClip(POSITION pos, CClip& newClip)
{
    LVITEM lv;
    POSITION newClipPos;

    if (pos == nullptr) {
        newClipPos = m_editList.AddTail(newClip);
    } else {
        newClipPos = m_editList.InsertAfter(pos, newClip);
    }

    lv.mask = LVIF_STATE | LVIF_TEXT;
    lv.iItem = FindIndex(pos);
    lv.iSubItem = 0;
    lv.pszText = const_cast<LPTSTR>(_T(""));
    lv.state = m_list.GetItemCount() == 0 ? LVIS_SELECTED : 0;
    m_list.InsertItem(&lv);

    return newClipPos;
}

int CEditListEditor::FindIndex(const POSITION pos) const
{
    int iItem = 0;
    POSITION curPos = m_editList.GetHeadPosition();
    while (curPos && curPos != pos) {
        m_editList.GetNext(curPos);
        iItem++;
    }
    return iItem;
}

int CEditListEditor::FindNameIndex(LPCTSTR strName) const
{
    int nResult = -1;

    for (int i = 0; i < m_nameList.GetCount(); i++) {
        const CString& curName = m_nameList.GetAt(i);

        if (curName == strName) {
            nResult = i;
            break;
        }
    }

    return nResult;
}

void CEditListEditor::FillCombo(LPCTSTR strFileName, CComboBox& combo, bool bAllowNull)
{
    CStdioFile nameFile;
    CString str;
    if (nameFile.Open(strFileName, CFile::modeRead)) {
        if (bAllowNull) {
            combo.AddString(_T(""));
        }

        while (nameFile.ReadString(str)) {
            combo.AddString(str);
        }
        nameFile.Close();
    }
}

void CEditListEditor::SelectCombo(LPCTSTR strValue, CComboBox& combo)
{
    for (int i = 0; i < combo.GetCount(); i++) {
        CString strTemp;
        combo.GetLBText(i, strTemp);
        if (strTemp == strValue) {
            combo.SetCurSel(i);
            break;
        }
    }
}

void CEditListEditor::OnDrawItem(int nIDCtl, LPDRAWITEMSTRUCT lpDrawItemStruct)
{
    if (nIDCtl != IDC_EDITLIST) {
        return;
    }

    int nItem = lpDrawItemStruct->itemID;
    CRect rcItem = lpDrawItemStruct->rcItem;
    POSITION pos = m_editList.FindIndex(nItem);

    if (pos) {
        bool fSelected = (pos == m_curPos);
        UNREFERENCED_PARAMETER(fSelected);
        CClip& curClip = m_editList.GetAt(pos);
        CString strTemp;

        CDC* pDC = CDC::FromHandle(lpDrawItemStruct->hDC);

        bool fThemed = AppNeedsThemedControls();
        COLORREF rowcolor = fThemed ? CMPCTheme::ContentBGColor : GetSysColor(COLOR_WINDOW);
        if (!!m_list.GetItemState(nItem, LVIS_SELECTED)) {
            rowcolor = fThemed ? CMPCTheme::ContentSelectedColor : 0xf1dacc;
            FillRect(pDC->m_hDC, rcItem, CBrush(rowcolor));
            FrameRect(pDC->m_hDC, rcItem, CBrush(0xc56a31));
        } else {
            FillRect(pDC->m_hDC, rcItem, CBrush(rowcolor));
        }

        COLORREF textcolor = fThemed ? CMPCTheme::TextFGColor : RGB(0, 0, 0);
        if (!curClip.HaveIn() || !curClip.HaveOut()) {
            textcolor = fThemed ? CMPCTheme::EnsureContrast(RGB(255, 0, 0), rowcolor, 4.5) : RGB(255, 0, 0); //plain red is lost on the grey selections
        }
        if (fThemed) {
            pDC->SetBkMode(TRANSPARENT);
        }

        for (int i = 0; i < COL_MAX; i++) {
            m_list.GetSubItemRect(nItem, i, LVIR_LABEL, rcItem);
            pDC->SetTextColor(textcolor);
            switch (i) {
                case COL_INDEX:
                    strTemp.Format(_T("%d"), nItem + 1);
                    pDC->DrawText(strTemp, rcItem, DT_CENTER | DT_VCENTER);
                    break;
                case COL_IN:
                    pDC->DrawText(curClip.GetIn(), rcItem, DT_CENTER | DT_VCENTER);
                    break;
                case COL_OUT:
                    pDC->DrawText(curClip.GetOut(), rcItem, DT_CENTER | DT_VCENTER);
                    break;
                case COL_NAME:
                    pDC->DrawText(curClip.GetName(), rcItem, DT_LEFT | DT_VCENTER);
                    break;
            }
        }
    }
}

void CEditListEditor::OnLvnItemchanged(NMHDR* pNMHDR, LRESULT* pResult)
{
    LPNMLISTVIEW pNMLV = reinterpret_cast<LPNMLISTVIEW>(pNMHDR);

    if (pNMLV->iItem >= 0) {
        m_curPos = m_editList.FindIndex(pNMLV->iItem);
    }
}

void CEditListEditor::OnLvnKeyDown(NMHDR* pNMHDR, LRESULT* pResult)
{
    LPNMLVKEYDOWN pLVKeyDown = reinterpret_cast<LPNMLVKEYDOWN>(pNMHDR);

    *pResult = FALSE;

    if (pLVKeyDown->wVKey == VK_DELETE) {
        POSITION pos = m_list.GetFirstSelectedItemPosition();
        POSITION clipPos;
        int nItem = -1;
        while (pos) {
            nItem = m_list.GetNextSelectedItem(pos);
            clipPos = m_editList.FindIndex(nItem);
            if (clipPos) {
                m_editList.RemoveAt(clipPos);
            }
            m_list.DeleteItem(nItem);
        }
        if (nItem != -1) {
            m_list.SetItemState(std::min(nItem, m_list.GetItemCount() - 1), LVIS_SELECTED, LVIS_SELECTED);
        }
        m_list.Invalidate();
    }
}

void CEditListEditor::OnBeginDrag(NMHDR* pNMHDR, LRESULT* pResult)
{
    ModifyStyle(WS_EX_ACCEPTFILES, 0);

    m_nDragIndex = ((LPNMLISTVIEW)pNMHDR)->iItem;

    CPoint p(0, 0);
    m_pDragImage = m_list.CreateDragImageEx(&p);

    CPoint p2 = ((LPNMLISTVIEW)pNMHDR)->ptAction;

    m_pDragImage->BeginDrag(0, p2 - p);
    m_pDragImage->DragEnter(GetDesktopWindow(), ((LPNMLISTVIEW)pNMHDR)->ptAction);

    m_bDragging = TRUE;
    m_nDropIndex = -1;

    SetCapture();
}

void CEditListEditor::OnMouseMove(UINT nFlags, CPoint point)
{
    if (m_bDragging) {
        m_ptDropPoint = point;
        ClientToScreen(&m_ptDropPoint);

        m_pDragImage->DragMove(m_ptDropPoint);
        m_pDragImage->DragShowNolock(FALSE);

        WindowFromPoint(m_ptDropPoint)->ScreenToClient(&m_ptDropPoint);

        m_pDragImage->DragShowNolock(TRUE);

        {
            int iOverItem = m_list.HitTest(m_ptDropPoint);
            int iTopItem = m_list.GetTopIndex();
            int iBottomItem = m_list.GetBottomIndex();

            if (iOverItem == iTopItem && iTopItem != 0) { // top of list
                SetTimer(1, 100, nullptr);
            } else {
                KillTimer(1);
            }

            if (iOverItem >= iBottomItem && iBottomItem != (m_list.GetItemCount() - 1)) { // bottom of list
                SetTimer(2, 100, nullptr);
            } else {
                KillTimer(2);
            }
        }
    }

    __super::OnMouseMove(nFlags, point);
}

void CEditListEditor::OnTimer(UINT_PTR nIDEvent)
{
    int iTopItem = m_list.GetTopIndex();
    int iBottomItem = iTopItem + m_list.GetCountPerPage() - 1;

    if (m_bDragging) {
        m_pDragImage->DragShowNolock(FALSE);

        if (nIDEvent == 1) {
            m_list.EnsureVisible(iTopItem - 1, false);
            m_list.UpdateWindow();
            if (m_list.GetTopIndex() == 0) {
                KillTimer(1);
            }
        } else if (nIDEvent == 2) {
            m_list.EnsureVisible(iBottomItem + 1, false);
            m_list.UpdateWindow();
            if (m_list.GetBottomIndex() == (m_list.GetItemCount() - 1)) {
                KillTimer(2);
            }
        }

        m_pDragImage->DragShowNolock(TRUE);
    }

    __super::OnTimer(nIDEvent);
}

void CEditListEditor::OnLButtonUp(UINT nFlags, CPoint point)
{
    if (m_bDragging) {
        ::ReleaseCapture();

        m_bDragging = FALSE;
        m_pDragImage->DragLeave(GetDesktopWindow());
        m_pDragImage->EndDrag();

        delete m_pDragImage;
        m_pDragImage = nullptr;

        KillTimer(1);
        KillTimer(2);

        CPoint pt(point);
        ClientToScreen(&pt);

        if (WindowFromPoint(pt) == &m_list) {
            DropItemOnList();
        }
    }

    ModifyStyle(0, WS_EX_ACCEPTFILES);

    __super::OnLButtonUp(nFlags, point);
}

void CEditListEditor::DropItemOnList()
{
    m_ptDropPoint.y -= 10;
    m_nDropIndex = m_list.HitTest(CPoint(10, m_ptDropPoint.y));

    POSITION dragPos = m_editList.FindIndex(m_nDragIndex);
    POSITION dropPos = m_editList.FindIndex(m_nDropIndex);
    if (dragPos && dropPos) {
        CClip& dragClip = m_editList.GetAt(dragPos);
        m_editList.InsertAfter(dropPos, dragClip);
        m_editList.RemoveAt(dragPos);
        m_list.Invalidate();
    }
}

void CEditListEditor::OnBeginlabeleditList(NMHDR* pNMHDR, LRESULT* pResult)
{
    LV_DISPINFO* pDispInfo = (LV_DISPINFO*)pNMHDR;
    LV_ITEM* pItem = &pDispInfo->item;

    *pResult = FALSE;

    if (pItem->iItem < 0) {
        return;
    }

    if (pItem->iSubItem == COL_NAME) {
        *pResult = TRUE;
    }
}

void CEditListEditor::OnDolabeleditList(NMHDR* pNMHDR, LRESULT* pResult)
{
    LV_DISPINFO* pDispInfo = (LV_DISPINFO*)pNMHDR;
    LV_ITEM* pItem = &pDispInfo->item;

    *pResult = FALSE;

    if (pItem->iItem < 0) {
        return;
    }

    if (m_curPos && pItem->iSubItem == COL_NAME) {
        CClip& curClip = m_editList.GetAt(m_curPos);
        int nSel = FindNameIndex(curClip.GetName());

        CAtlList<CString> sl;
        for (int i = 0; i < m_nameList.GetCount(); i++) {
            sl.AddTail(m_nameList.GetAt(i));
        }
        m_list.ShowInPlaceComboBox(pItem->iItem, pItem->iSubItem, sl, nSel, true);

        *pResult = TRUE;
    }
}

void CEditListEditor::OnEndlabeleditList(NMHDR* pNMHDR, LRESULT* pResult)
{
    LV_DISPINFO* pDispInfo = (LV_DISPINFO*)pNMHDR;
    LV_ITEM* pItem = &pDispInfo->item;

    *pResult = FALSE;

    if (!m_list.m_fInPlaceDirty) {
        return;
    }

    if (pItem->iItem < 0) {
        return;
    }

    CString& curName = m_nameList.GetAt(pItem->lParam);

    if (m_curPos && pItem->iSubItem == COL_NAME) {
        CClip& curClip = m_editList.GetAt(m_curPos);
        curClip.SetName(curName);

        *pResult = TRUE;
    }
}
