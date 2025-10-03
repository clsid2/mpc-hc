/*
 * (C) 2003-2006 Gabest
 * (C) 2006-2017 see Authors.txt
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
#include <WinAPIUtils.h>
#include "SubtitleDlDlg.h"
#include "SubtitlesProvider.h"
#include "mplayerc.h"
#include "MainFrm.h"
#include "ISOLang.h"
#include "PPageSubMisc.h"
#include "CMPCTheme.h"
#include "CMPCThemeMenu.h"
#include "SysVersion.h"

BEGIN_MESSAGE_MAP(CSubtitleDlDlgListCtrl, CMPCThemePlayerListCtrl)
    ON_NOTIFY_EX(TTN_NEEDTEXT, 0, OnToolNeedText)
END_MESSAGE_MAP()

void CSubtitleDlDlgListCtrl::PreSubclassWindow()
{
    __super::PreSubclassWindow();
}

BOOL CSubtitleDlDlgListCtrl::OnToolNeedText(UINT id, NMHDR* pNMHDR, LRESULT*)
{
    auto pTTT = reinterpret_cast<TOOLTIPTEXT*>(pNMHDR);

    CPoint pt(GetMessagePos());
    ScreenToClient(&pt);

    LVHITTESTINFO lvhti = { pt };
    int nItem = SubItemHitTest(&lvhti);
    int nSubItem = lvhti.iSubItem;

    if (nItem == -1 || !(lvhti.flags & LVHT_ONITEMLABEL) || nSubItem != CSubtitleDlDlg::COL_FILENAME) {
        return FALSE;
    }

    auto subtitleInfo = reinterpret_cast<SubtitlesInfo*>(GetItemData(nItem));
    if (!subtitleInfo || subtitleInfo->releaseNames.empty()) {
        return FALSE;
    }

    static CStringW tooltipText;
    tooltipText = SubtitlesProvidersUtils::JoinContainer(subtitleInfo->releaseNames, L"\n").c_str();
    ASSERT(!tooltipText.IsEmpty());

    pTTT->lpszText = tooltipText.GetBuffer();

    // Needed for multiline tooltips.
    ::SendMessage(pNMHDR->hwndFrom, TTM_SETMAXTIPWIDTH, 0, 1000);

    // Force ListView internal variables related to LABELTIP to invalidate. This is needed to use both custom tooltip and LABELTIP.
    // When LABELTIP is enabled ListView internally changes tooltip to be draw in-place of text. Unfortunately it doesn't
    // clear few variables when someone else handles TTN_NEEDTEXT.
    SetColumnWidth(CSubtitleDlDlg::COL_FILENAME, GetColumnWidth(CSubtitleDlDlg::COL_FILENAME));

    return TRUE;
}

// User Defined Window Messages
enum {
    UWM_SEARCH = WM_USER + 100,
    UWM_SEARCHING,
    UWM_DOWNLOADING,
    UWM_DOWNLOADED,
    UWM_COMPLETED,
    UWM_FINISHED,
    UWM_FAILED_SEARCH,
    UWM_FAILED_DOWNLOAD,
    UWM_CLEAR
};

CSubtitleDlDlg::CSubtitleDlDlg(CMainFrame* pParentWnd)
    : CModelessResizableDialog(IDD, pParentWnd)
    , m_ps(nullptr, 0, 0)
    , m_bIsRefreshed(false)
    , m_pMainFrame(pParentWnd)
{
}

void CSubtitleDlDlg::DoDataExchange(CDataExchange* pDX)
{
    __super::DoDataExchange(pDX);
    DDX_Control(pDX, IDC_LIST1, m_list);
    DDX_Control(pDX, IDC_PROGRESS1, m_progress);
    DDX_Control(pDX, IDC_STATUSBAR, m_status);
    DDX_Text(pDX, IDC_EDIT1, manualSearch);
    fulfillThemeReqs();
}

void CSubtitleDlDlg::SetStatusText(const CString& status, BOOL bPropagate/* = TRUE*/)
{
    m_status.SetText(status, 0, 0);
    if (bPropagate) {
        m_pMainFrame->SendStatusMessage(status, 5000);
    }
}

void CSubtitleDlDlg::SetListViewSortColumn()
{
    CHeaderCtrl* header = m_list.GetHeaderCtrl();
    int columnCount = header->GetItemCount();
    for (int i = 0; i < columnCount; ++i) {
        HDITEM hi = { 0 };
        hi.mask = HDI_FORMAT;
        header->GetItem(i, &hi);

        hi.fmt &= ~(HDF_SORTDOWN | HDF_SORTUP);
        //Set sort image to this column
        if (i == m_ps.m_nSortColumn)  {
            hi.fmt |= m_ps.m_fSortOrder > 0 ? HDF_SORTUP : m_ps.m_fSortOrder < 0 ? HDF_SORTDOWN : NULL;
        }
        header->SetItem(i, &hi);
    }
}

int SortScoreFile(SubtitlesInfo* si_left, SubtitlesInfo* si_right, bool sortup) {
    // high bits are language score
    // low bits are file score
    SHORT left = (SHORT)LOWORD(si_left->Score());
    SHORT right = (SHORT)LOWORD(si_right->Score());
    return left == right ? 0 : sortup ? (left > right ? 1 : -1) : (left < right ? 1 : -1);
}

int SortScore(SubtitlesInfo* si_left, SubtitlesInfo* si_right, bool sortup) {
    DWORD left  = si_left->Score();
    DWORD right = si_right->Score();
    return left == right ? 0 : sortup ? (left > right ? 1 : -1) : (left < right ? 1 : -1);
}

int SortFramerate(SubtitlesInfo* si_left, SubtitlesInfo* si_right, bool sortup) {
    double left = si_left->frameRate;
    double right = si_right->frameRate;
    return (abs(left-right) < 0.001) ? 0 : sortup ? (left > right ? 1 : -1) : (left < right ? 1 : -1);
}

int SortDownloads(SubtitlesInfo* si_left, SubtitlesInfo* si_right, bool sortup) {
    int left = si_left->downloadCount;
    int right = si_right->downloadCount;
    if (left == -1 && right != -1) {
        return 1;
    }
    if (left != -1 && right == -1) {
        return -1;
    }
    return left == right ? 0 : sortup ? (left > right ? 1 : -1) : (left < right ? 1 : -1);
}

int CALLBACK CSubtitleDlDlg::SortCompare(LPARAM lParam1, LPARAM lParam2, LPARAM lParamSort)
{
    PPARAMSORT ps = (PPARAMSORT)(lParamSort);
    CListCtrl* list = (CListCtrl*)CListCtrl::FromHandle(ps->m_hWnd);

    SubtitlesInfo* si_left  = (SubtitlesInfo*)(list->GetItemData((int)lParam1));
    SubtitlesInfo* si_right = (SubtitlesInfo*)(list->GetItemData((int)lParam2));
    bool sortup = (ps->m_fSortOrder == 1);

    if (ps->m_fSortOrder == 0) {
        int x = SortScore(si_left, si_right, false);
        if (x == 0) {
            return SortDownloads(si_left, si_right, false);
        } else {
            return x;
        }
    }

    if (ps->m_nSortColumn == COL_DOWNLOADS) {
        int x = SortDownloads(si_left, si_right, sortup);
        if (x == 0) {
            return SortScore(si_left, si_right, false);
        } else {
            return x;
        }
    }

    if (ps->m_nSortColumn == COL_FRAMERATE) {
        int x = SortFramerate(si_left, si_right, sortup);
        if (x == 0) {
            return SortScore(si_left, si_right, false);
        } else {
            return x;
        }
    }

    if (ps->m_nSortColumn == COL_SCORE) {
        int x = SortScoreFile(si_left, si_right, sortup);
        if (x == 0) {
            x = SortScore(si_left, si_right, sortup);
            if (x == 0) {
                return SortDownloads(si_left, si_right, false);
            } else {
                return x;
            }
        } else {
            return x;
        }
    }

    CString left(list->GetItemText((int)lParam1, ps->m_nSortColumn));
    CString right(list->GetItemText((int)lParam2, ps->m_nSortColumn));
    if (left == _T("-") && right != _T("-")) {
        return 1;
    }
    if (left != _T("-") && right == _T("-")) {
        return -1;
    }
    int x = sortup ? StrCmpLogicalW(left, right) : StrCmpLogicalW(right, left);
    if (x == 0) {
        int x = (ps->m_nSortColumn == COL_LANGUAGE) ? SortScoreFile(si_left, si_right, false) : SortScore(si_left, si_right, false);
        if (x == 0) {
            return SortDownloads(si_left, si_right, false);
        } else {
            return x;
        }
    }
    return x;
}

BOOL CSubtitleDlDlg::OnInitDialog()
{
    __super::OnInitDialog();
    m_progress.SetParent(&m_status);
    if (AppIsThemeLoaded()) {
        SetWindowTheme(m_progress.GetSafeHwnd(), _T(""), _T(""));
        m_progress.SetBarColor(CMPCTheme::ProgressBarColor);
        m_progress.SetBkColor(CMPCTheme::ProgressBarBGColor);
    }
    m_progress.UpdateWindow();

    m_list.SetExtendedStyle(m_list.GetExtendedStyle()
                            /*| LVS_EX_DOUBLEBUFFER | LVS_EX_FULLROWSELECT */
                            | LVS_EX_CHECKBOXES | LVS_EX_LABELTIP);
    m_list.setAdditionalStyles(LVS_EX_DOUBLEBUFFER | LVS_EX_FULLROWSELECT);

    m_list.SetImageList(&m_pMainFrame->m_pSubtitlesProviders->GetImageList(), LVSIL_SMALL);

    m_ps.m_hWnd = m_list.GetSafeHwnd();
    m_ps.m_nSortColumn = AfxGetApp()->GetProfileInt(IDS_R_DLG_SUBTITLEDL, IDS_RS_DLG_SUBTITLEDL_SORTCOLUMN, 0);
    m_ps.m_fSortOrder = AfxGetApp()->GetProfileInt(IDS_R_DLG_SUBTITLEDL, IDS_RS_DLG_SUBTITLEDL_SORTORDER, 0);

    int n = 0, curPos = 0;
    CArray<int> columnWidth;

    CString strColumnWidth(AfxGetApp()->GetProfileString(IDS_R_DLG_SUBTITLEDL, IDS_RS_DLG_SUBTITLEDL_COLWIDTH));
    CString token(strColumnWidth.Tokenize(_T(","), curPos));
    while (!token.IsEmpty()) {
        if (_stscanf_s(token, L"%d", &n) == 1) {
            columnWidth.Add(n);
            token = strColumnWidth.Tokenize(_T(","), curPos);
        } else {
            throw 1;
        }
    }

    if (columnWidth.GetCount() != COL_TOTAL_COLUMNS) {
        // default sizes
        columnWidth.RemoveAll();
        columnWidth.Add(150);
        columnWidth.Add(300);
        columnWidth.Add(100);
        columnWidth.Add(40);
        columnWidth.Add(40);
        columnWidth.Add(70);
        columnWidth.Add(250);
        columnWidth.Add(40);
        columnWidth.Add(40);
    }

    m_list.InsertColumn(COL_PROVIDER, ResStr(IDS_SUBDL_DLG_PROVIDER_COL), LVCFMT_LEFT, columnWidth[COL_PROVIDER]);
    m_list.InsertColumn(COL_FILENAME, ResStr(IDS_SUBDL_DLG_FILENAME_COL), LVCFMT_LEFT, columnWidth[COL_FILENAME]);
    m_list.InsertColumn(COL_LANGUAGE, ResStr(IDS_SUBDL_DLG_LANGUAGE_COL), LVCFMT_LEFT, columnWidth[COL_LANGUAGE]);
    m_list.InsertColumn(COL_FRAMERATE, L"FPS", LVCFMT_RIGHT, columnWidth[COL_FRAMERATE]);
    m_list.InsertColumn(COL_HEARINGIMPAIRED, ResStr(IDS_SUBDL_DLG_HI_COL), LVCFMT_RIGHT, columnWidth[COL_HEARINGIMPAIRED]);
    m_list.InsertColumn(COL_DOWNLOADS, ResStr(IDS_SUBDL_DLG_DOWNLOADS_COL), LVCFMT_RIGHT, columnWidth[COL_DOWNLOADS]);
    m_list.InsertColumn(COL_TITLES, ResStr(IDS_SUBDL_DLG_TITLES_COL), LVCFMT_LEFT, columnWidth[COL_TITLES]);
    m_list.InsertColumn(COL_SCORE, ResStr(IDS_SUBDL_DLG_SCORE_COL), LVCFMT_RIGHT, columnWidth[COL_SCORE]);
    m_list.InsertColumn(COL_DISC, ResStr(IDS_SUBDL_DLG_DISC_COL), LVCFMT_RIGHT, columnWidth[COL_DISC]);
    SetListViewSortColumn();

    AddAnchor(IDC_LIST1, TOP_LEFT, BOTTOM_RIGHT);
    AddAnchor(IDC_CHECK1, BOTTOM_LEFT);
    AddAnchor(IDC_BUTTON1, BOTTOM_RIGHT);
    AddAnchor(IDC_BUTTON2, BOTTOM_RIGHT);
    AddAnchor(IDC_BUTTON3, BOTTOM_RIGHT);
    AddAnchor(IDOK, BOTTOM_RIGHT);
    AddAnchor(IDC_EDIT1, BOTTOM_RIGHT, BOTTOM_RIGHT);
    AddAnchor(IDC_BUTTON4, BOTTOM_RIGHT);
    AddAnchor(IDC_STATUSBAR, BOTTOM_LEFT, BOTTOM_RIGHT);

    CRect cr;
    GetClientRect(cr);
    const CSize s(cr.Width(), 250);
    SetMinTrackSize(s);
    EnableSaveRestoreKey(IDS_R_DLG_SUBTITLEDL, TRUE);

    return TRUE;
}

BOOL CSubtitleDlDlg::PreTranslateMessage(MSG* pMsg)
{
    if (pMsg->message == WM_KEYDOWN && pMsg->wParam == VK_RETURN) {
        // Inhibit default handling for the Enter key when the list has the focus and an item is selected.
        if (pMsg->hwnd == m_list.GetSafeHwnd() && m_list.GetSelectedCount() > 0) {
            return FALSE;
        }
        if (pMsg->hwnd == GetDlgItem(IDC_EDIT1)->GetSafeHwnd()) { //we want <enter> in the search field to initiate search
            SendMessage(WM_COMMAND, (WPARAM)IDC_BUTTON4, LPARAM(0)); //press search button
            return FALSE;
        }
    }
    return __super::PreTranslateMessage(pMsg);
}

void CSubtitleDlDlg::OnOK()
{
    if (IsDlgButtonChecked(IDC_CHECK1) == BST_CHECKED) {
        m_pMainFrame->SetSubtitle(SubtitleInput(nullptr));
        CAutoLock cAutoLock(&m_pMainFrame->m_csSubLock);
        auto& subStreams = m_pMainFrame->m_pSubStreams;
        POSITION pos = subStreams.GetHeadPosition();
        while (pos) {
            POSITION currentPos = pos;
            if (!subStreams.GetNext(pos).pSourceFilter) {
                subStreams.RemoveAt(currentPos);
            }
        }
    }

    bool bActivate = true;
    for (int i = 0; i < m_list.GetItemCount(); ++i) {
        if (m_list.GetCheck(i) == TRUE) {

            SubtitlesInfo& subtitlesInfo = *(SubtitlesInfo*)(m_list.GetItemData(i));
            LVITEMINDEX lvii = { i, -1 };
            m_list.SetItemIndexState(&lvii, INDEXTOSTATEIMAGEMASK(0), LVIS_STATEIMAGEMASK);
            subtitlesInfo.Download(bActivate);
            bActivate = false;
        }
    }

    __super::OnOK();
}

void CSubtitleDlDlg::OnRefresh()
{
    m_list.DeleteAllItems();
    m_pMainFrame->m_pSubtitlesProviders->Search(FALSE);
}

void CSubtitleDlDlg::OnManualSearch()
{
    m_list.DeleteAllItems();
    UpdateData(TRUE);
    m_pMainFrame->m_pSubtitlesProviders->ManualSearch(FALSE, manualSearch);
}

void CSubtitleDlDlg::OnAbort()
{
    m_pMainFrame->m_pSubtitlesProviders->Abort(SubtitlesThreadType(STT_SEARCH | STT_DOWNLOAD));
}

void CSubtitleDlDlg::OnOptions()
{
    m_pMainFrame->ShowOptions(CPPageSubMisc::IDD);
}

void CSubtitleDlDlg::OnUpdateOk(CCmdUI* pCmdUI)
{
    bool fEnable = false;
    for (int i = 0; !fEnable && i < m_list.GetItemCount(); ++i) {
        fEnable = (m_list.GetCheck(i) == TRUE);
    }

    pCmdUI->Enable(fEnable);
}

void CSubtitleDlDlg::OnUpdateRefresh(CCmdUI* pCmdUI)
{
    bool fEnable = true;
    pCmdUI->Enable(fEnable);
}

void CSubtitleDlDlg::OnColumnClick(NMHDR* pNMHDR, LRESULT* pResult)
{
    LPNMHEADER phdr = (LPNMHEADER)(pNMHDR);
    *pResult = 0;

    if (phdr->iItem == m_ps.m_nSortColumn) {
        if (m_ps.m_fSortOrder == 1) {
            m_ps.m_fSortOrder = (m_ps.m_nSortColumn == COL_DOWNLOADS) ? 0 : -1;
        } else if (m_ps.m_fSortOrder == -1) {
            m_ps.m_fSortOrder = (m_ps.m_nSortColumn == COL_DOWNLOADS) ? 1 : 0;
        } else if (m_ps.m_fSortOrder == 0) {
            m_ps.m_fSortOrder = (m_ps.m_nSortColumn == COL_DOWNLOADS) ? -1 : 1;
        }
    } else {
        m_ps.m_nSortColumn = phdr->iItem;
        m_ps.m_fSortOrder = (m_ps.m_nSortColumn == COL_DOWNLOADS) ? -1 : 1;
    }
    SetListViewSortColumn();

    SetRedraw(FALSE);
    m_list.SortItemsEx(SortCompare, (DWORD_PTR)&m_ps);

    SetRedraw(TRUE);
    m_list.Invalidate();
    m_list.UpdateWindow();
}

void CSubtitleDlDlg::OnSize(UINT nType, int cx, int cy)
{
    __super::OnSize(nType, cx, cy);

    if (m_status && m_progress) {
        // Reposition the progress control correctly!
        CRect statusRect, buttonRect;
        m_status.GetClientRect(&statusRect);
        GetDlgItem(IDOK)->GetWindowRect(&buttonRect);
        ScreenToClient(&buttonRect);
        int parts[2] = { buttonRect.left - 2, -1 };
        m_status.SetParts(2, parts);
        m_status.GetRect(1, &statusRect);
        statusRect.DeflateRect(1, 1, 1, 1);
        m_progress.SetWindowPos(nullptr, statusRect.left, statusRect.top, statusRect.Width(), statusRect.Height(),  SWP_NOACTIVATE | SWP_NOZORDER);
    }
    if (m_status && AppIsThemeLoaded()) {
        //m_status currently set up to draw a gripper, but doesn't play nice with theme until it gets invalidated
        //style 0x103 from mpc-hc.rc = SBARS_SIZEGRIP | CCS_TOP | CCS_NOMOVEY ??
        if (m_status.GetStyle() & SBARS_SIZEGRIP) { 
            CRect statusRect;
            m_status.GetClientRect(statusRect);
            statusRect.left = statusRect.right - GetSystemMetrics(SM_CXVSCROLL);
            m_status.InvalidateRect(statusRect);
        }
    }
}

void CSubtitleDlDlg::OnDestroy()
{
    RemoveAllAnchors();

    const CHeaderCtrl& pHC = *m_list.GetHeaderCtrl();
    if (pHC) {
        CString strColumnWidth;
        for (int i = 0; i < pHC.GetItemCount(); ++i) {
            int w = m_list.GetColumnWidth(i);
            strColumnWidth.AppendFormat(L"%d,", w);
        }
        AfxGetApp()->WriteProfileString(IDS_R_DLG_SUBTITLEDL, IDS_RS_DLG_SUBTITLEDL_COLWIDTH, strColumnWidth);
        AfxGetApp()->WriteProfileInt(IDS_R_DLG_SUBTITLEDL, IDS_RS_DLG_SUBTITLEDL_SORTCOLUMN, m_ps.m_nSortColumn);
        AfxGetApp()->WriteProfileInt(IDS_R_DLG_SUBTITLEDL, IDS_RS_DLG_SUBTITLEDL_SORTORDER, m_ps.m_fSortOrder);
    }

    __super::OnDestroy();
}

void CSubtitleDlDlg::DownloadSelectedSubtitles()
{
    POSITION pos = m_list.GetFirstSelectedItemPosition();
    while (pos) {
        int nItem = m_list.GetNextSelectedItem(pos);
        if (nItem >= 0 && nItem < m_list.GetItemCount()) {
            m_list.SetCheck(nItem, TRUE);
        }
    }
    OnOK();
}

// ON_UPDATE_COMMAND_UI does not work for modeless dialogs
BEGIN_MESSAGE_MAP(CSubtitleDlDlg, CModelessResizableDialog)
    ON_WM_ERASEBKGND()
    ON_WM_SIZE()
    ON_COMMAND(IDC_BUTTON1, OnRefresh)
    ON_COMMAND(IDC_BUTTON2, OnAbort)
    ON_COMMAND(IDC_BUTTON3, OnOptions)
    ON_COMMAND(IDC_BUTTON4, OnManualSearch)
    ON_UPDATE_COMMAND_UI(IDOK, OnUpdateOk)
    ON_UPDATE_COMMAND_UI(IDC_BUTTON1, OnUpdateRefresh)
    ON_NOTIFY(HDN_ITEMCLICK, 0, OnColumnClick)
    ON_WM_DESTROY()
    ON_NOTIFY(NM_DBLCLK, IDC_LIST1, OnDoubleClickSubtitle)
    ON_NOTIFY(LVN_KEYDOWN, IDC_LIST1, OnKeyPressedSubtitle)
    ON_NOTIFY(NM_RCLICK, IDC_LIST1, OnRightClick)
    ON_NOTIFY(LVN_ITEMCHANGING, IDC_LIST1, OnItemChanging)
    ON_NOTIFY(LVN_ITEMCHANGED, IDC_LIST1, OnItemChanged)
    ON_WM_SHOWWINDOW()

    ON_MESSAGE(UWM_SEARCH, OnSearch)
    ON_MESSAGE(UWM_SEARCHING, OnSearching)
    ON_MESSAGE(UWM_DOWNLOADING, OnDownloading)
    ON_MESSAGE(UWM_DOWNLOADED, OnDownloaded)
    ON_MESSAGE(UWM_COMPLETED, OnCompleted)
    ON_MESSAGE(UWM_FINISHED, OnFinished)
    ON_MESSAGE(UWM_FAILED_SEARCH, OnFailedSearch)
    ON_MESSAGE(UWM_FAILED_DOWNLOAD, OnFailedDownload)
    ON_MESSAGE(UWM_CLEAR, OnClear)
END_MESSAGE_MAP()

void CSubtitleDlDlg::OnDoubleClickSubtitle(NMHDR* pNMHDR, LRESULT* pResult)
{
    LPNMITEMACTIVATE pItemActivate = (LPNMITEMACTIVATE)(pNMHDR);

    if (pItemActivate->iItem >= 0 &&  m_list.GetCheck(pItemActivate->iItem) != -1) {
        DownloadSelectedSubtitles();
    }
}

void CSubtitleDlDlg::OnKeyPressedSubtitle(NMHDR* pNMHDR, LRESULT* pResult)
{
    LV_KEYDOWN* pLVKeyDow = (LV_KEYDOWN*)pNMHDR;

    if (pLVKeyDow->wVKey == VK_RETURN) {
        DownloadSelectedSubtitles();
        *pResult = TRUE;
    }
}

void CSubtitleDlDlg::OnRightClick(NMHDR* pNMHDR, LRESULT* pResult)
{
    LPNMLISTVIEW lpnmlv = (LPNMLISTVIEW)pNMHDR;

    if (lpnmlv->iItem >= 0 && lpnmlv->iSubItem >= 0) {
        SubtitlesInfo& subtitlesInfo = *(SubtitlesInfo*)(m_list.GetItemData(lpnmlv->iItem));

        enum {
            DOWNLOAD = 0x1000,
            OPEN_URL,
            COPY_URL
        };

        CMPCThemeMenu m;
        m.CreatePopupMenu();
        m.AppendMenu(MF_STRING | (m_list.GetCheck(lpnmlv->iItem) != -1 ? MF_ENABLED : MF_DISABLED), DOWNLOAD, ResStr(IDS_SUBMENU_DOWNLOAD));
        m.AppendMenu(MF_SEPARATOR);
        m.AppendMenu(MF_STRING | (!subtitlesInfo.url.empty() ? MF_ENABLED : MF_DISABLED), OPEN_URL, ResStr(IDS_SUBMENU_OPENURL));
        m.AppendMenu(MF_STRING | (!subtitlesInfo.url.empty() ? MF_ENABLED : MF_DISABLED), COPY_URL, ResStr(IDS_SUBMENU_COPYURL));
        if (AppIsThemeLoaded()) {
            m.fulfillThemeReqs();
        }

        CPoint pt = lpnmlv->ptAction;
        ::MapWindowPoints(lpnmlv->hdr.hwndFrom, HWND_DESKTOP, &pt, 1);

        switch (m.TrackPopupMenu(TPM_LEFTBUTTON | TPM_RETURNCMD, pt.x, pt.y, this)) {
            case DOWNLOAD:
                m_list.SetCheck(lpnmlv->iItem, TRUE);
                OnOK();
                break;
            case OPEN_URL:
                subtitlesInfo.OpenUrl();
                break;
            case COPY_URL: {
                if (!subtitlesInfo.url.empty()) {
                    CClipboard clipboard(this);
                    VERIFY(clipboard.SetText(subtitlesInfo.url.c_str()));
                }
                break;
            }
            default:
                break;
        }
    }
}

void CSubtitleDlDlg::OnItemChanging(NMHDR* pNMHDR, LRESULT* pResult)
{
    LPNMLISTVIEW pNMLV = (LPNMLISTVIEW)(pNMHDR);

    if (pNMLV->uOldState == 0 && pNMLV->uNewState == LVIS_UNCHECKED && pNMLV->lParam) {
        *pResult = TRUE;
    }
}

void CSubtitleDlDlg::OnItemChanged(NMHDR* pNMHDR, LRESULT* pResult)
{
    LPNMLISTVIEW pNMLV = reinterpret_cast<LPNMLISTVIEW>(pNMHDR);
    if (pNMLV->uChanged & LVIF_STATE) { //sync checkboxes and highlighted/selected rows
        if (pNMLV->uNewState & LVIS_CHECKED) {
            m_list.SetItemState(pNMLV->iItem, LVIS_SELECTED, LVIS_SELECTED);
        } else if (pNMLV->uNewState & LVIS_UNCHECKED) {
            m_list.SetItemState(pNMLV->iItem, 0, LVIS_SELECTED);
        } else if (pNMLV->uNewState & LVIS_SELECTED) { //selection was changed
            m_list.SetCheck(pNMLV->iItem, 1);
        } else if (pNMLV->uOldState & LVIS_SELECTED) { //selection was removed
            m_list.SetCheck(pNMLV->iItem, 0);
        }
    }

    UpdateDialogControls(this, FALSE);
}


void CSubtitleDlDlg::OnShowWindow(BOOL bShow, UINT nStatus)
{
    __super::OnShowWindow(bShow, nStatus);

    const auto& s = AfxGetAppSettings();

    if (bShow == TRUE && !m_bIsRefreshed && !m_pMainFrame->m_fAudioOnly && s.fEnableSubtitles) {
        OnRefresh();
        m_bIsRefreshed = true;
    }
}


afx_msg LRESULT CSubtitleDlDlg::OnSearch(WPARAM wParam, LPARAM /*lParam*/)
{
    INT _nCount = (INT)wParam;

    SetStatusText(StrRes(IDS_SUBDL_DLG_SEARCHING));
    GetDlgItem(IDC_BUTTON1)->EnableWindow(FALSE);
    GetDlgItem(IDC_BUTTON1)->ShowWindow(FALSE);
    GetDlgItem(IDC_BUTTON2)->ShowWindow(TRUE);

    m_progress.SetRange32(0, _nCount);
    m_progress.SetStep(1);
    m_progress.SetPos(0);

    return S_OK;
}

afx_msg LRESULT CSubtitleDlDlg::OnSearching(WPARAM /*wParam*/, LPARAM lParam)
{
    SubtitlesInfo& _fileInfo = *(SubtitlesInfo*)lParam;
    CStringW search = UTF8To16(_fileInfo.fileName.c_str());
    if (search.IsEmpty()) {
        search = _fileInfo.manualSearchString;
    }
    CStringW title = ResStr(IDS_SUBDL_DLG_TITLE) + _T(" - ") + search;
    SetWindowText(title);
    return S_OK;
}

afx_msg LRESULT CSubtitleDlDlg::OnDownloading(WPARAM /*wParam*/, LPARAM lParam)
{
    SubtitlesInfo& _fileInfo = *(SubtitlesInfo*)lParam;

    CString statusMessage;
    statusMessage.Format(IDS_SUBDL_DLG_DOWNLOADING, CString(_fileInfo.Provider()->DisplayName().c_str()).GetString(), CString(_fileInfo.fileName.c_str()).GetString());
    SetStatusText(statusMessage);

    return S_OK;
}

afx_msg LRESULT CSubtitleDlDlg::OnDownloaded(WPARAM /*wParam*/, LPARAM lParam)
{
    SubtitlesInfo& _fileInfo = *(SubtitlesInfo*)lParam;

    CString statusMessage;
    statusMessage.Format(IDS_SUBDL_DLG_DOWNLOADED, CString(_fileInfo.Provider()->DisplayName().c_str()).GetString(), CString(_fileInfo.fileName.c_str()).GetString());
    SetStatusText(statusMessage);

    for (int i = 0; i < m_list.GetItemCount(); ++i) {
        SubtitlesInfo& iter = *(SubtitlesInfo*)m_list.GetItemData(i);
        if (iter.UID() == _fileInfo.UID()) {
            LVITEMINDEX lvii = { i, -1 };
            m_list.SetItemIndexState(&lvii, INDEXTOSTATEIMAGEMASK(0), LVIS_STATEIMAGEMASK);
        }
    }

    return S_OK;
}

afx_msg LRESULT CSubtitleDlDlg::OnCompleted(WPARAM wParam, LPARAM lParam)
{
    SRESULT _result = (SRESULT)wParam;
    SubtitlesList& _subtitlesList = *(SubtitlesList*)lParam;

    m_progress.StepIt();

    if (_result == SR_ABORTED) {
        SetStatusText(StrRes(IDS_SUBDL_DLG_ABORTING));
    } else if (!_subtitlesList.empty()) {
        m_list.SetRedraw(FALSE);

        for (const auto& subInfo : _subtitlesList) {
            int iItem = m_list.InsertItem(0, UTF8To16(subInfo.Provider()->DisplayName().c_str()), subInfo.Provider()->GetIconIndex());
            m_list.SetItemText(iItem, COL_FILENAME, UTF8To16(subInfo.fileName.c_str()));
            m_list.SetItemText(iItem, COL_LANGUAGE, ISOLang::ISO639XToLanguage(subInfo.languageCode.c_str()));
            CStringW disc = L"";
            if (subInfo.discNumber > 0) {
                if (subInfo.discCount > 0) {
                    disc.Format(L"%d/%d", subInfo.discNumber, subInfo.discCount);
                } else {
                    disc.Format(L"%d", subInfo.discNumber);
                }
            }
            m_list.SetItemText(iItem, COL_DISC, disc);
            m_list.SetItemText(iItem, COL_HEARINGIMPAIRED, subInfo.hearingImpaired == -1 ? _T("-") : subInfo.hearingImpaired > 0 ? ResStr(IDS_YES).GetString() : ResStr(IDS_NO).GetString());
            CString downloads(_T("-"));
            if (subInfo.downloadCount != -1) {
                downloads.Format(_T("%d"), subInfo.downloadCount);
                downloads = FormatNumber(downloads);
            }
            m_list.SetItemText(iItem, COL_DOWNLOADS, downloads);
            m_list.SetItemText(iItem, COL_TITLES, UTF8To16(subInfo.DisplayTitle().c_str()));

            CString fps;
            if (subInfo.frameRate > 0.1) {
                fps.Format(_T("%.3f"), subInfo.frameRate);
            }
            m_list.SetItemText(iItem, COL_FRAMERATE, fps);

            CString score;
            score.Format(_T("%d"), (SHORT)LOWORD(subInfo.Score()));
            m_list.SetItemText(iItem, COL_SCORE, score);

            m_Subtitles.emplace_back(subInfo);
            m_list.SetItemData(iItem, (DWORD_PTR)&m_Subtitles.back());
        }

        // sort
        m_list.SortItemsEx(SortCompare, (DWORD_PTR)&m_ps);

        m_list.SetRedraw(TRUE);
        m_list.Invalidate();
        m_list.UpdateWindow();
    }
    m_bIsRefreshed = true;
    UpdateWindow();

    return S_OK;
}

afx_msg LRESULT CSubtitleDlDlg::OnFinished(WPARAM wParam, LPARAM lParam)
{
    BOOL _bAborted = (BOOL)wParam;
    BOOL _bShowDialog = (BOOL)lParam;

    if (_bAborted == FALSE) {
        if (!m_Subtitles.empty()) {
            if (_bShowDialog == TRUE && !IsWindowVisible()) {
                ShowWindow(SW_SHOW);
            }

            CString message;
            message.Format(IDS_SUBDL_DLG_FOUND, (int)m_Subtitles.size());
            SetStatusText(message);
        } else {
            SetStatusText(StrRes(IDS_SUBDL_DLG_NOTFOUND));
        }
    } else {
        SetStatusText(StrRes(IDS_SUBDL_DLG_ABORTED));
    }

    int nLower = 0, nUpper = 0;
    m_progress.GetRange(nLower, nUpper);
    m_progress.SetPos(nUpper);

    GetDlgItem(IDC_BUTTON2)->ShowWindow(FALSE);
    GetDlgItem(IDC_BUTTON1)->ShowWindow(TRUE);
    GetDlgItem(IDC_BUTTON1)->EnableWindow(TRUE);
    UpdateWindow();

    return S_OK;
}

afx_msg LRESULT CSubtitleDlDlg::OnFailedSearch(WPARAM /*wParam*/, LPARAM /*lParam*/)
{
    SetStatusText(StrRes(IDS_SUBDL_DLG_FAILED));

    return S_OK;
}

afx_msg LRESULT CSubtitleDlDlg::OnFailedDownload(WPARAM wParam, LPARAM /*lParam*/)
{
    CString status = StrRes(IDS_SUBDL_DLG_FAILED_DL);
    status.AppendFormat(L" (error %lu)", (DWORD)wParam);
    if (wParam == 406) {
        status.Append(L" (global daily download quota exceeded)");
    }
    SetStatusText(status);

    return S_OK;
}


afx_msg LRESULT CSubtitleDlDlg::OnClear(WPARAM /*wParam*/, LPARAM /*lParam*/)
{
    CString title(StrRes(IDS_SUBDL_DLG_TITLE));
    SetWindowText(title);

    m_progress.SetPos(0);
    SetStatusText(_T(""));
    m_list.DeleteAllItems();
    m_Subtitles.clear();

    m_bIsRefreshed = false;

    GetDlgItem(IDC_BUTTON2)->ShowWindow(FALSE);
    GetDlgItem(IDC_BUTTON1)->ShowWindow(TRUE);
    GetDlgItem(IDC_BUTTON1)->EnableWindow(TRUE);
    UpdateWindow();

    return S_OK;
}

void CSubtitleDlDlg::DoSearch(INT _nCount)
{
    SendMessage(UWM_SEARCH, (WPARAM)_nCount, (LPARAM)nullptr);
}
void CSubtitleDlDlg::DoSearching(SubtitlesInfo& _fileInfo)
{
    SendMessage(UWM_SEARCHING, (WPARAM)nullptr, (LPARAM)&_fileInfo);
}
void CSubtitleDlDlg::DoDownloading(SubtitlesInfo& _fileInfo)
{
    SendMessage(UWM_DOWNLOADING, (WPARAM)nullptr, (LPARAM)&_fileInfo);
}
void CSubtitleDlDlg::DoDownloaded(SubtitlesInfo& _fileInfo)
{
    SendMessage(UWM_DOWNLOADED, (WPARAM)nullptr, (LPARAM)&_fileInfo);
}
void CSubtitleDlDlg::DoCompleted(SRESULT _result, SubtitlesList& _subtitlesList)
{
    SendMessage(UWM_COMPLETED, (WPARAM)_result, (LPARAM)&_subtitlesList);
}
void CSubtitleDlDlg::DoFinished(BOOL _bAborted, BOOL _bShowDialog)
{
    SendMessage(UWM_FINISHED, (WPARAM)_bAborted, (LPARAM)_bShowDialog);
}
void CSubtitleDlDlg::DoSearchFailed()
{
    SendMessage(UWM_FAILED_SEARCH, (WPARAM)nullptr, (LPARAM)nullptr);
}
void CSubtitleDlDlg::DoDownloadFailed(DWORD statuscode)
{
    SendMessage(UWM_FAILED_DOWNLOAD, (WPARAM)statuscode, (LPARAM)nullptr);
}
void CSubtitleDlDlg::DoClear()
{
    SendMessage(UWM_CLEAR, (WPARAM)nullptr, (LPARAM)nullptr);
}
