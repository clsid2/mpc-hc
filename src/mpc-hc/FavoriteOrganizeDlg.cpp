/*
 * (C) 2003-2006 Gabest
 * (C) 2006-2016 see Authors.txt
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
#include <strsafe.h>
#include <WinAPIUtils.h>
#include "FavoriteOrganizeDlg.h"
#include "mplayerc.h"
#include "MainFrm.h"
#include "CMPCTheme.h"
#undef SubclassWindow

// CFavoriteOrganizeDlg dialog

//IMPLEMENT_DYNAMIC(CFavoriteOrganizeDlg, CMPCThemeResizableDialog)
CFavoriteOrganizeDlg::CFavoriteOrganizeDlg(CWnd* pParent /*=nullptr*/)
    : CMPCThemeModelessResizableDialog(CFavoriteOrganizeDlg::IDD, pParent)
{
}

CFavoriteOrganizeDlg::~CFavoriteOrganizeDlg()
{
}

void CFavoriteOrganizeDlg::SetupList(bool fSave)
{

    int i = m_tab.GetCurSel();

    if (fSave) {
        // Update the internal list to match the visual list order and names
        CAtlList<CString> sl;

        for (int j = 0; j < m_list.GetItemCount(); j++) {
            CAtlList<CString> args;
            ExplodeEsc(m_sl[i].GetAt((POSITION)m_list.GetItemData(j)), args, _T(';'));
            args.RemoveHead();
            args.AddHead(m_list.GetItemText(j, 0));
            sl.AddTail(ImplodeEsc(args, _T(';')));
        }
        m_sl[i].RemoveAll();
        m_sl[i].AddTailList(&sl);

        // Update itemdata pointers to point to new positions in the rebuilt list
        POSITION pos = m_sl[i].GetHeadPosition();
        for (int j = 0; j < m_list.GetItemCount() && pos; j++) {
            POSITION tmp = pos;
            m_sl[i].GetNext(pos);
            m_list.SetItemData(j, (DWORD_PTR)tmp);
        }
    } else {
        m_list.SetRedraw(FALSE);
        m_list.DeleteAllItems();

        for(POSITION pos = m_sl[i].GetHeadPosition(), tmp; pos; ) {
            tmp = pos;

            FileFavorite ff;
            VERIFY(FileFavorite::TryParse(m_sl[i].GetNext(pos), ff));

            int n = m_list.InsertItem(m_list.GetItemCount(), ff.Name);
            m_list.SetItemData(n, (DWORD_PTR)tmp);

            CString str = ff.ToString();
            if (!str.IsEmpty()) {
                m_list.SetItemText(n, 1, str);
            }
        }

        UpdateColumnsSizes();
        m_list.SetRedraw(TRUE);
        m_list.RedrawWindow(0, 0, RDW_INVALIDATE);
    }
}

void CFavoriteOrganizeDlg::UpdateColumnsSizes()
{
    CRect r;
    m_list.GetClientRect(r);
    m_list.SetColumnWidth(0, LVSCW_AUTOSIZE_USEHEADER);
    if (firstSize) {
        m_list.SetColumnWidth(1, LVSCW_AUTOSIZE); //this is needed to calculate the min width, but don't keep resetting it or you get flicker
        firstSize = false;
        minSizeTime = m_list.GetColumnWidth(1);
    }
    m_list.SetColumnWidth(1, std::max(minSizeTime, r.Width() - m_list.GetColumnWidth(0)));
}

void CFavoriteOrganizeDlg::DoDataExchange(CDataExchange* pDX)
{
    __super::DoDataExchange(pDX);
    DDX_Control(pDX, IDC_TAB1, m_tab);
    DDX_Control(pDX, IDC_LIST2, m_list);
}


BEGIN_MESSAGE_MAP(CFavoriteOrganizeDlg, CMPCThemeModelessResizableDialog)
    ON_NOTIFY(TCN_SELCHANGE, IDC_TAB1, OnTcnSelchangeTab1)
    ON_WM_DRAWITEM()
    ON_NOTIFY(LVN_ITEMCHANGED, IDC_LIST2, OnLvnItemchangedList2)
    ON_BN_CLICKED(IDC_BUTTON1, OnRenameBnClicked)
    ON_UPDATE_COMMAND_UI(IDC_BUTTON1, OnUpdateRenameBn)
    ON_BN_CLICKED(IDC_BUTTON2, OnDeleteBnClicked)
    ON_UPDATE_COMMAND_UI(IDC_BUTTON2, OnUpdateDeleteBn)
    ON_BN_CLICKED(IDC_BUTTON3, OnUpBnClicked)
    ON_UPDATE_COMMAND_UI(IDC_BUTTON3, OnUpdateUpBn)
    ON_BN_CLICKED(IDC_BUTTON4, OnDownBnClicked)
    ON_UPDATE_COMMAND_UI(IDC_BUTTON4, OnUpdateDownBn)
    ON_NOTIFY(TCN_SELCHANGING, IDC_TAB1, OnTcnSelchangingTab1)
    ON_BN_CLICKED(IDOK, OnBnClickedOk)
    ON_BN_CLICKED(ID_APPLY_NOW, OnBnClickedApply)
    ON_UPDATE_COMMAND_UI(ID_APPLY_NOW, OnUpdateApplyBn)
    ON_WM_SHOWWINDOW()
    ON_WM_ACTIVATE()
    ON_NOTIFY(LVN_ENDLABELEDIT, IDC_LIST2, OnLvnEndlabeleditList2)
    ON_NOTIFY(NM_DBLCLK, IDC_LIST2, OnPlayFavorite)
    ON_NOTIFY(LVN_KEYDOWN, IDC_LIST2, OnKeyPressed)
    ON_NOTIFY(LVN_GETINFOTIP, IDC_LIST2, OnLvnGetInfoTipList)
    ON_WM_SIZE()
    ON_MESSAGE(WM_DPICHANGED, OnDpiChanged)
END_MESSAGE_MAP()

void  CFavoriteOrganizeDlg::OnLvnItemchangedList2(NMHDR* pNMHDR, LRESULT* pResult) {
    CWnd::UpdateDialogControls(this, TRUE); //needed for modeless dialog due to no idle pump
}
// CFavoriteOrganizeDlg message handlers

BOOL CFavoriteOrganizeDlg::OnInitDialog()
{
    EnableSaveRestoreKey(IDS_R_DLG_ORGANIZE_FAV);

    __super::OnInitDialog();
    if (GetExStyle() & WS_EX_TOPMOST) {
        if (auto tt = m_list.GetToolTips()) { //when dialog is topmost, tooltips appear behind the dialog?
            tt->SetWindowPos(&wndTopMost, 0, 0, 0, 0, SWP_NOSIZE | SWP_NOACTIVATE | SWP_NOMOVE);
        }
    }
    firstSize = true;
    minSizeTime = 0;
    m_tab.InsertItem(0, ResStr(IDS_FAVFILES));
    m_tab.InsertItem(1, ResStr(IDS_FAVDVDS));
    //  m_tab.InsertItem(2, ResStr(IDS_FAVDEVICES));
    m_tab.SetCurSel(0);

    m_list.InsertColumn(0, _T(""));
    m_list.InsertColumn(1, _T(""));
    m_list.SetExtendedStyle(m_list.GetExtendedStyle() | LVS_EX_INFOTIP);
    m_list.setAdditionalStyles(LVS_EX_FULLROWSELECT);

    LoadList();

    SetupAnchors();
    fulfillThemeReqs();

    return TRUE;  // return TRUE unless you set the focus to a control
    // EXCEPTION: OCX Property Pages should return FALSE
}

void CFavoriteOrganizeDlg::LoadList() {
    const CAppSettings& s = AfxGetAppSettings();
    s.GetFav(FAV_FILE, m_sl[0]);
    s.GetFav(FAV_DVD, m_sl[1]);
    s.GetFav(FAV_DEVICE, m_sl[2]);

    SetupList(false);
    m_bModified = false;
}

void CFavoriteOrganizeDlg::AddItemToVisualList(int tabIndex, const CString& favoriteString) {
    // Add to the in-memory list
    m_sl[tabIndex].AddTail(favoriteString);

    // If we're currently on this tab, refresh the list
    if (m_tab.GetCurSel() == tabIndex) {
        SetupList(false);

        // Select and scroll to the newly added item (it's at the end)
        int newItemIndex = m_list.GetItemCount() - 1;
        if (newItemIndex >= 0) {
            m_list.SetItemState(newItemIndex, LVIS_SELECTED | LVIS_FOCUSED, LVIS_SELECTED | LVIS_FOCUSED);
            m_list.EnsureVisible(newItemIndex, FALSE);
        }
    }

    // Mark as modified so changes can be saved
    m_bModified = true;

    // Update dialog controls to enable Apply button
    UpdateDialogControls(this, TRUE);
}

BOOL CFavoriteOrganizeDlg::PreTranslateMessage(MSG* pMsg)
{
    // Inhibit default handling for the Enter key when the list has the focus and an item is selected.
    if (pMsg->message == WM_KEYDOWN && pMsg->wParam == VK_RETURN
            && pMsg->hwnd == m_list.GetSafeHwnd() && m_list.GetSelectedCount() > 0) {
        return FALSE;
    }

    return __super::PreTranslateMessage(pMsg);
}

void CFavoriteOrganizeDlg::OnTcnSelchangeTab1(NMHDR* pNMHDR, LRESULT* pResult)
{
    SetupList(false);

    m_list.SetWindowPos(&wndTop, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);

    *pResult = 0;
}

void CFavoriteOrganizeDlg::OnDrawItem(int nIDCtl, LPDRAWITEMSTRUCT lpDrawItemStruct)
{
    if (nIDCtl != IDC_LIST2) {
        return;
    }

    int nItem = lpDrawItemStruct->itemID;
    if (m_list.IsItemVisible(nItem)) {
        CEdit* pEdit = m_list.GetEditControl();

        CRect rcItem = lpDrawItemStruct->rcItem;
        CRect rText, rTime, rectDC, rHighlight;

        m_list.GetSubItemRect(nItem, 0, LVIR_LABEL, rText);
        rText.left += 2; //magic number for column 0
        m_list.GetSubItemRect(nItem, 1, LVIR_LABEL, rTime);
        rTime.right -= 6; //magic number for column >0, from right
        rectDC = rcItem;
        rHighlight = rcItem;
        rHighlight.left = rText.left;
        rHighlight.right = rTime.right;

        CDC* pDC = CDC::FromHandle(lpDrawItemStruct->hDC);
        int savedDC = pDC->SaveDC();
        bool isSelected = !!m_list.GetItemState(nItem, LVIS_SELECTED);
        bool isEdited = pEdit && ::IsWindow(pEdit->m_hWnd) && isSelected;

        CDC dcMem;
        CBitmap bmMem;
        CMPCThemeUtil::initMemDC(pDC, dcMem, bmMem, rectDC);
        rcItem.OffsetRect(-rectDC.TopLeft());
        rText.OffsetRect(-rectDC.TopLeft());
        rTime.OffsetRect(-rectDC.TopLeft());
        rHighlight.OffsetRect(-rectDC.TopLeft());

        if (AppIsThemeLoaded()) {
            dcMem.FillSolidRect(rcItem, CMPCTheme::ContentBGColor);
        } else {
            dcMem.FillSolidRect(rcItem, GetSysColor(COLOR_WINDOW));
        }

        if (isSelected) {
            if (AppIsThemeLoaded()) {
                dcMem.FillSolidRect(rHighlight, CMPCTheme::ContentSelectedColor);
            } else {
                CBrush b2;
                dcMem.FillSolidRect(rHighlight, 0xf1dacc);
                b2.CreateSolidBrush(0xc56a31);
                dcMem.FrameRect(rHighlight, &b2);
                b2.DeleteObject();
            }
        }

        COLORREF textcolor;
        if (AppIsThemeLoaded()) {
            textcolor = CMPCTheme::TextFGColor;
        } else {
            textcolor = 0;
        }
        dcMem.SetTextColor(textcolor);

        CString str;

        if (!isEdited) {
            str = m_list.GetItemText(nItem, 0);
            dcMem.DrawTextW(str, rText, DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS | DT_LEFT | DT_NOPREFIX);
        }
        str = m_list.GetItemText(nItem, 1);
        if (!str.IsEmpty()) {
            dcMem.DrawTextW(str, rTime, DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS | DT_RIGHT | DT_NOPREFIX);
        }
        if (isEdited) { //added to reduce flicker while editing.  
            CRect r;
            pEdit->GetWindowRect(r);
            m_list.ScreenToClient(r);
            pDC->ExcludeClipRect(r);
        }
        CMPCThemeUtil::flushMemDC(pDC, dcMem, rectDC);
        pDC->RestoreDC(savedDC);
    }
}

void CFavoriteOrganizeDlg::OnRenameBnClicked()
{
    if (POSITION pos = m_list.GetFirstSelectedItemPosition()) {
        m_list.SetFocus();
        m_list.EditLabel(m_list.GetNextSelectedItem(pos));
    }
}

void CFavoriteOrganizeDlg::OnUpdateRenameBn(CCmdUI* pCmdUI)
{
    pCmdUI->Enable(m_list.GetSelectedCount() == 1);
}

void CFavoriteOrganizeDlg::OnLvnEndlabeleditList2(NMHDR* pNMHDR, LRESULT* pResult)
{
    NMLVDISPINFO* pDispInfo = reinterpret_cast<NMLVDISPINFO*>(pNMHDR);
    if (pDispInfo->item.iItem >= 0 && pDispInfo->item.pszText) {
        m_list.SetItemText(pDispInfo->item.iItem, 0, pDispInfo->item.pszText);
        m_bModified = true;
        UpdateDialogControls(this, TRUE);
    }
    UpdateColumnsSizes();

    *pResult = 0;
}

void CFavoriteOrganizeDlg::PlayFavorite(int nItem)
{
    switch (m_tab.GetCurSel()) {
        case 0: // Files
            ((CMainFrame*)GetParentFrame())->PlayFavoriteFile(m_sl[0].GetAt((POSITION)m_list.GetItemData(nItem)));
            break;
        case 1: // DVDs
            ((CMainFrame*)GetParentFrame())->PlayFavoriteDVD(m_sl[1].GetAt((POSITION)m_list.GetItemData(nItem)));
            break;
        case 2: // Devices
            break;
    }
}

void CFavoriteOrganizeDlg::OnPlayFavorite(NMHDR* pNMHDR, LRESULT* pResult)
{
    LPNMITEMACTIVATE pItemActivate = reinterpret_cast<LPNMITEMACTIVATE>(pNMHDR);

    if (pItemActivate->iItem >= 0) {
        PlayFavorite(pItemActivate->iItem);
    }
}

void CFavoriteOrganizeDlg::OnKeyPressed(NMHDR* pNMHDR, LRESULT* pResult)
{
    LV_KEYDOWN* pLVKeyDow = (LV_KEYDOWN*)pNMHDR;

    switch (pLVKeyDow->wVKey) {
        case VK_DELETE:
        case VK_BACK:
            OnDeleteBnClicked();
            *pResult = 1;
            break;
        case VK_RETURN:
            if (POSITION pos = m_list.GetFirstSelectedItemPosition()) {
                int nItem = m_list.GetNextSelectedItem(pos);
                if (nItem >= 0 && nItem < m_list.GetItemCount()) {
                    PlayFavorite(nItem);
                }
            }
            *pResult = 1;
            break;
        case 'A':
            if (GetKeyState(VK_CONTROL) < 0) { // If the high-order bit is 1, the key is down; otherwise, it is up.
                m_list.SetItemState(-1, LVIS_SELECTED, LVIS_SELECTED);
            }
            *pResult = 1;
            break;
        case 'I':
            if (GetKeyState(VK_CONTROL) < 0) { // If the high-order bit is 1, the key is down; otherwise, it is up.
                for (int nItem = 0; nItem < m_list.GetItemCount(); nItem++) {
                    m_list.SetItemState(nItem, ~m_list.GetItemState(nItem, LVIS_SELECTED), LVIS_SELECTED);
                }
            }
            *pResult = 1;
            break;

        case 'C':
            if (GetKeyState(VK_CONTROL) < 0) {
                if(m_tab.GetCurSel() == 0) {   // Files
                    CopyToClipboard();
                }
            }
            *pResult = 1;
            break;
        default:
            *pResult = 0;
    }
}

void CFavoriteOrganizeDlg::OnDeleteBnClicked()
{
    POSITION pos;
    int nItem = -1;

    while ((pos = m_list.GetFirstSelectedItemPosition()) != nullptr) {
        nItem = m_list.GetNextSelectedItem(pos);
        if (nItem < 0 || nItem >= m_list.GetItemCount()) {
            return;
        }

        m_list.DeleteItem(nItem);
        m_bModified = true;
    }

    nItem = std::min(nItem, m_list.GetItemCount() - 1);
    m_list.SetItemState(nItem, LVIS_SELECTED, LVIS_SELECTED);
    UpdateDialogControls(this, TRUE);
}

void CFavoriteOrganizeDlg::OnUpdateDeleteBn(CCmdUI* pCmdUI)
{
    pCmdUI->Enable(m_list.GetSelectedCount() > 0);
}

void CFavoriteOrganizeDlg::MoveItem(int nItem, int offset)
{
    DWORD_PTR data = m_list.GetItemData(nItem);
    CString strName = m_list.GetItemText(nItem, 0);
    CString strPos = m_list.GetItemText(nItem, 1);

    m_list.DeleteItem(nItem);

    nItem += offset;

    m_list.InsertItem(nItem, strName);
    m_list.SetItemData(nItem, data);
    m_list.SetItemText(nItem, 1, strPos);
    m_list.SetItemState(nItem, LVIS_SELECTED, LVIS_SELECTED);
    m_list.EnsureVisible(nItem, FALSE);
    m_bModified = true;
}

void CFavoriteOrganizeDlg::OnUpBnClicked()
{
    POSITION pos = m_list.GetFirstSelectedItemPosition();

    while (pos) {
        int nItem = m_list.GetNextSelectedItem(pos);
        if (nItem <= 0 || nItem >= m_list.GetItemCount()) {
            return;
        }

        MoveItem(nItem, -1);
    }
    UpdateDialogControls(this, TRUE);
}

void CFavoriteOrganizeDlg::OnUpdateUpBn(CCmdUI* pCmdUI)
{
    pCmdUI->Enable(m_list.GetSelectedCount() > 0 && !m_list.GetItemState(0, LVIS_SELECTED));
}

void CFavoriteOrganizeDlg::OnDownBnClicked()
{
    CArray<int> selectedItems;
    POSITION pos = m_list.GetFirstSelectedItemPosition();

    while (pos) {
        int nItem = m_list.GetNextSelectedItem(pos);
        if (nItem < 0 || nItem >= m_list.GetItemCount() - 1) {
            return;
        }

        selectedItems.Add(nItem);
    }

    for (INT_PTR i = selectedItems.GetSize() - 1; i >= 0; i--) {
        MoveItem(selectedItems[i], +1);
    }
    UpdateDialogControls(this, TRUE);
}

void CFavoriteOrganizeDlg::OnUpdateDownBn(CCmdUI* pCmdUI)
{
    pCmdUI->Enable(m_list.GetSelectedCount() > 0 && !m_list.GetItemState(m_list.GetItemCount() - 1, LVIS_SELECTED));
}

void CFavoriteOrganizeDlg::OnTcnSelchangingTab1(NMHDR* pNMHDR, LRESULT* pResult)
{
    SetupList(true);

    *pResult = 0;
}

void CFavoriteOrganizeDlg::SaveChanges()
{
    SetupList(true);

    CAppSettings& s = AfxGetAppSettings();
    s.SetFav(FAV_FILE, m_sl[0]);
    s.SetFav(FAV_DVD, m_sl[1]);
    s.SetFav(FAV_DEVICE, m_sl[2]);

    m_bModified = false;
}

void CFavoriteOrganizeDlg::OnBnClickedOk()
{
    SaveChanges();
    OnOK();
}

void CFavoriteOrganizeDlg::OnBnClickedApply()
{
    SaveChanges();
    UpdateDialogControls(this, TRUE);
}

void CFavoriteOrganizeDlg::OnUpdateApplyBn(CCmdUI* pCmdUI)
{
    pCmdUI->Enable(m_bModified);
}

void CFavoriteOrganizeDlg::OnCancel()
{
    if (m_bModified) {
        int nRet = AfxMessageBox(IDS_FAVORITES_UNSAVED_CHANGES, MB_YESNOCANCEL | MB_ICONQUESTION);
        if (nRet == IDYES) {
            SaveChanges();
        } else if (nRet == IDCANCEL) {
            return;
        } else {
            // IDNO - discard changes by reloading from settings
            LoadList();
        }
    }

    __super::OnCancel();
}

void CFavoriteOrganizeDlg::OnShowWindow(BOOL bShow, UINT nStatus)
{
    __super::OnShowWindow(bShow, nStatus);

    if (bShow) {
        // Always reload data from settings when showing the dialog
        LoadList();
    }
}

void CFavoriteOrganizeDlg::OnActivate(UINT nState, CWnd* pWndOther, BOOL bMinimized)
{
    __super::OnActivate(nState, pWndOther, bMinimized);

    if (nState == WA_ACTIVE) {
        m_list.SetWindowPos(&wndTop, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);
    }
}

void CFavoriteOrganizeDlg::SetupAnchors()
{
    AddAnchor(IDC_TAB1, TOP_LEFT, BOTTOM_RIGHT);
    AddAnchor(IDC_LIST2, TOP_LEFT, BOTTOM_RIGHT);
    AddAnchor(IDC_BUTTON1, TOP_RIGHT);
    AddAnchor(IDC_BUTTON2, TOP_RIGHT);
    AddAnchor(IDC_BUTTON3, TOP_RIGHT);
    AddAnchor(IDC_BUTTON4, TOP_RIGHT);
    AddAnchor(IDOK, BOTTOM_RIGHT);
    AddAnchor(IDCANCEL, BOTTOM_RIGHT);
    AddAnchor(ID_APPLY_NOW, BOTTOM_RIGHT);
}

void CFavoriteOrganizeDlg::OnSize(UINT nType, int cx, int cy)
{
    __super::OnSize(nType, cx, cy);

    if (IsWindow(m_list)) {
        UpdateColumnsSizes(); //on first size, we need to call this, or it doesn't use the full window until a rename/resize
    }
}

LRESULT CFavoriteOrganizeDlg::OnDpiChanged(WPARAM wParam, LPARAM lParam)
{
    LRESULT result = __super::OnDpiChanged(wParam, lParam);

    // CDpiAwareResizableDialog blocks OnSize during dpi changes.  Update columns here
    if (IsWindow(m_list)) {
        UpdateColumnsSizes();
    }

    return result;
}

void CFavoriteOrganizeDlg::OnLvnGetInfoTipList(NMHDR* pNMHDR, LRESULT* pResult)
{
    LPNMLVGETINFOTIP pGetInfoTip = reinterpret_cast<LPNMLVGETINFOTIP>(pNMHDR);

    CAtlList<CString> args;
    ExplodeEsc(m_sl[m_tab.GetCurSel()].GetAt((POSITION)m_list.GetItemData(pGetInfoTip->iItem)), args, _T(';'));
    CString path = args.RemoveTail();
    // Relative to drive value is always third. If less args are available that means it is not included.
    int rootLength = (args.GetCount() == 3 && args.RemoveTail() != _T("0")) ? CPath(path).SkipRoot() : 0;

    StringCchCopyW(pGetInfoTip->pszText, pGetInfoTip->cchTextMax, path.Mid(rootLength));

    *pResult = 0;
}


void CFavoriteOrganizeDlg::CopyToClipboard()
{
    CAtlList<CString>* pSL = &m_sl[m_tab.GetCurSel()];

    // Iterate through selected items
    CString favorites;
    for(POSITION pos = m_list.GetFirstSelectedItemPosition(); pos; ) {
        int iItem = m_list.GetNextSelectedItem(pos);
        const CString& fav = pSL->GetAt((POSITION)m_list.GetItemData(iItem));
        CAtlList<CString> args;
        ((CMainFrame*)GetParentFrame())->ParseFavoriteFile(fav, args);

        CString path = args.GetHead().Trim();
        if (!path.IsEmpty()) {
            favorites.Append(path);
            favorites.Append(_T("\r\n"));
        }
    }

    if (!favorites.IsEmpty()) {
        CClipboard clipboard(this);
        VERIFY(clipboard.SetText(favorites));
    }
}
