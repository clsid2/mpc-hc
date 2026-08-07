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

#include "stdafx.h"
#include <WinAPIUtils.h>
#include "HistoryDlg.h"
#include "mplayerc.h"
#include "MainFrm.h"
#include "PathUtils.h"
#include "SettingsDefines.h"
#include "CMPCThemeMenu.h"
#include "date/date.h"

// CHistoryDlg dialog

static void RemoveFromJumpList(const std::list<CString>& paths)
{
    if (paths.empty()) {
        return;
    }

    CComPtr<IApplicationDestinations> pDests;
    HRESULT hr = pDests.CoCreateInstance(CLSID_ApplicationDestinations, nullptr, CLSCTX_INPROC_SERVER);
    if (FAILED(hr)) {
        return;
    }
    CComPtr<IApplicationDocumentLists> pDocLists;
    hr = pDocLists.CoCreateInstance(CLSID_ApplicationDocumentLists, nullptr, CLSCTX_INPROC_SERVER);
    if (FAILED(hr)) {
        return;
    }
    CComPtr<IObjectArray> pItems;
    hr = pDocLists->GetList(ADLT_RECENT, 0, IID_PPV_ARGS(&pItems));
    if (FAILED(hr)) {
        return;
    }
    UINT cObjects = 0;
    hr = pItems->GetCount(&cObjects);
    if (FAILED(hr)) {
        return;
    }
    for (UINT i = 0; i < cObjects; i++) {
        CComPtr<IShellItem> pShellItem;
        if (SUCCEEDED(pItems->GetAt(i, IID_PPV_ARGS(&pShellItem)))) {
            LPWSTR pszName = nullptr;
            if (SUCCEEDED(pShellItem->GetDisplayName(SIGDN_FILESYSPATH, &pszName))) {
                for (const auto& path : paths) {
                    if (path.CompareNoCase(pszName) == 0) {
                        pDests->RemoveDestination(pShellItem);
                        break;
                    }
                }
                CoTaskMemFree(pszName);
            }
        }
    }
}

static bool HasSavedPosition(const RecentFileEntry& r)
{
    if (r.DVDPosition.llDVDGuid) {
        return r.DVDPosition.lTitle != 0
               || r.DVDPosition.timecode.bHours || r.DVDPosition.timecode.bMinutes
               || r.DVDPosition.timecode.bSeconds || r.DVDPosition.timecode.bFrames;
    }
    return r.filePosition > 0;
}

CHistoryDlg::CHistoryDlg(CWnd* pParent)
    : CMPCThemeModelessResizableDialog(CHistoryDlg::IDD, pParent)
{
}

CHistoryDlg::~CHistoryDlg()
{
}

void CHistoryDlg::RefreshList()
{
    auto& MRU = AfxGetAppSettings().MRU;
    MRU.ReadMediaHistory();

    m_entries.clear();
    m_entries.reserve(MRU.GetSize());
    for (int i = 0; i < MRU.GetSize(); i++) {
        m_entries.emplace_back(MRU[i]);
    }

    SetupList();
}

void CHistoryDlg::SetupList()
{
    m_list.SetRedraw(FALSE);
    m_list.DeleteAllItems();

    CString filter;
    m_filterEdit.GetWindowText(filter);

    auto LowerCase = [](CString& str) {
        if (!str.IsEmpty()) {
            ::CharLowerBuffW(str.GetBuffer(), str.GetLength());
        }
    };
    LowerCase(filter);

    for (size_t i = 0; i < m_entries.size(); i++) {
        const auto& entry = m_entries[i];
        if (entry.fns.IsEmpty()) {
            continue;
        }
        const CString& entryPath = entry.fns.GetHead();

        if (!filter.IsEmpty()) {
            CString path(entryPath);
            CString title(entry.title);
            LowerCase(path);
            LowerCase(title);
            if (path.Find(filter) < 0 && (title.IsEmpty() || title.Find(filter) < 0)) {
                continue;
            }
        }

        int n = m_list.InsertItem(m_list.GetItemCount(), entryPath);
        m_list.SetItemText(n, COL_TITLE, entry.title);

        CString str;
        if (entry.DVDPosition.llDVDGuid) {
            if (entry.DVDPosition.lTitle) {
                str.Format(_T("%02lu,%02u:%02u:%02u"),
                           entry.DVDPosition.lTitle,
                           (unsigned)entry.DVDPosition.timecode.bHours,
                           (unsigned)entry.DVDPosition.timecode.bMinutes,
                           (unsigned)entry.DVDPosition.timecode.bSeconds);
            }
        } else if (entry.filePosition > UNITS) {
            LONGLONG seconds = entry.filePosition / UNITS;
            str.Format(_T("%02d:%02d:%02d"), (int)(seconds / 3600), (int)(seconds / 60 % 60), (int)(seconds % 60));
        }
        m_list.SetItemText(n, COL_POS, str);

        // lastOpened is stored as "YYYY-MM-DDTHH:MM:SS.mmmZ"
        CString lastOpened(entry.lastOpened);
        if (lastOpened.GetLength() >= 16) {
            lastOpened = lastOpened.Left(10) + _T(" ") + lastOpened.Mid(11, 5);
        }
        m_list.SetItemText(n, COL_LAST_OPENED, lastOpened);

        m_list.SetItemData(n, i);
    }

    m_list.SetRedraw(TRUE);
    m_list.RedrawWindow();
}

void CHistoryDlg::ApplyColumnWidths()
{
    CArray<int> columnWidth;

    int n = 0, curPos = 0;
    CString strColumnWidth(AfxGetApp()->GetProfileString(IDS_R_DLG_HISTORY, IDS_RS_DLG_HISTORY_COLWIDTH));
    CString token(strColumnWidth.Tokenize(_T(","), curPos));
    while (!token.IsEmpty() && _stscanf_s(token, _T("%d"), &n) == 1) {
        columnWidth.Add(n);
        token = strColumnWidth.Tokenize(_T(","), curPos);
    }

    for (int i = 0; i < COL_COUNT; i++) {
        int width = (i < columnWidth.GetCount()) ? columnWidth[i] : 0;
        if (width <= 0 || width > 2000) {
            // auto-size to the wider of header and content
            m_list.SetColumnWidth(i, LVSCW_AUTOSIZE_USEHEADER);
            const int headerWidth = m_list.GetColumnWidth(i);
            m_list.SetColumnWidth(i, LVSCW_AUTOSIZE);
            width = m_list.GetColumnWidth(i);
            if (headerWidth > width) {
                width = headerWidth;
            }
        } else if (width < 25) {
            width = 25;
        }
        m_list.SetColumnWidth(i, width);
    }
}

void CHistoryDlg::SaveColumnWidths()
{
    CString strColumnWidth;
    for (int i = 0; i < COL_COUNT; i++) {
        strColumnWidth.AppendFormat(_T("%d,"), m_list.GetColumnWidth(i));
    }
    AfxGetApp()->WriteProfileString(IDS_R_DLG_HISTORY, IDS_RS_DLG_HISTORY_COLWIDTH, strColumnWidth);
}

void CHistoryDlg::CopySelectedPaths()
{
    CString paths;

    POSITION pos = m_list.GetFirstSelectedItemPosition();
    while (pos) {
        int nItem = m_list.GetNextSelectedItem(pos);
        size_t index = m_list.GetItemData(nItem);
        if (index < m_entries.size() && !m_entries[index].fns.IsEmpty()) {
            paths.Append(m_entries[index].fns.GetHead());
            paths.Append(_T("\r\n"));
        }
    }

    if (!paths.IsEmpty()) {
        CClipboard clipboard(this);
        VERIFY(clipboard.SetText(paths));
    }
}

void CHistoryDlg::OpenItem(int nItem)
{
    size_t index = m_list.GetItemData(nItem);
    if (index < m_entries.size()) {
        RecentFileEntry r(m_entries[index]);
        AfxGetMainFrame()->OpenRecentFileEntry(r);
    }
}

void CHistoryDlg::RemoveEntries(const std::list<CStringW>& hashes, const std::list<CString>& paths)
{
    if (hashes.empty()) {
        return;
    }

    AfxGetAppSettings().MRU.RemoveEntries(hashes);
    RemoveFromJumpList(paths);
    RefreshList();
}

void CHistoryDlg::ConfirmRemoveSelected()
{
    UINT count = m_list.GetSelectedCount();
    if (count > 0) {
        CString str;
        str.Format(ResStr(IDS_HISTORY_REMOVE_QUESTION), count);
        if (IDYES == AfxMessageBox(str, MB_ICONQUESTION | MB_YESNO)) {
            RemoveSelected();
        }
    }
}

void CHistoryDlg::RemoveSelected()
{
    std::list<CStringW> hashes;
    std::list<CString> paths;

    POSITION pos = m_list.GetFirstSelectedItemPosition();
    while (pos) {
        int nItem = m_list.GetNextSelectedItem(pos);
        size_t index = m_list.GetItemData(nItem);
        if (index < m_entries.size()) {
            hashes.emplace_back(m_entries[index].hash);
            if (!m_entries[index].fns.IsEmpty()) {
                paths.emplace_back(m_entries[index].fns.GetHead());
            }
        }
    }

    RemoveEntries(hashes, paths);
}

void CHistoryDlg::ResetPosition()
{
    auto& MRU = AfxGetAppSettings().MRU;

    POSITION pos = m_list.GetFirstSelectedItemPosition();
    while (pos) {
        int nItem = m_list.GetNextSelectedItem(pos);
        size_t index = m_list.GetItemData(nItem);
        if (index < m_entries.size() && HasSavedPosition(m_entries[index])) {
            auto& entry = m_entries[index];
            if (entry.DVDPosition.llDVDGuid) {
                entry.DVDPosition.lTitle = 0;
                entry.DVDPosition.timecode = {};
            } else {
                entry.filePosition = 0;
            }
            for (int i = 0; i < MRU.GetSize(); i++) {
                if (MRU[i].hash == entry.hash) {
                    if (MRU[i].DVDPosition.llDVDGuid) {
                        MRU[i].DVDPosition.lTitle = 0;
                        MRU[i].DVDPosition.timecode = {};
                    } else {
                        MRU[i].filePosition = 0;
                    }
                    MRU.WriteMediaHistoryEntry(MRU[i]);
                    break;
                }
            }
        }
    }

    RefreshList();
}

void CHistoryDlg::ResetTrackSelection()
{
    auto& MRU = AfxGetAppSettings().MRU;

    POSITION pos = m_list.GetFirstSelectedItemPosition();
    while (pos) {
        int nItem = m_list.GetNextSelectedItem(pos);
        size_t index = m_list.GetItemData(nItem);
        if (index < m_entries.size() && (m_entries[index].AudioTrackIndex != -1 || m_entries[index].SubtitleTrackIndex != -1)) {
            auto& entry = m_entries[index];
            entry.AudioTrackIndex = -1;
            entry.SubtitleTrackIndex = -1;
            for (int i = 0; i < MRU.GetSize(); i++) {
                if (MRU[i].hash == entry.hash) {
                    MRU[i].AudioTrackIndex = -1;
                    MRU[i].SubtitleTrackIndex = -1;
                    MRU.WriteMediaHistoryAudioIndex(MRU[i]);
                    MRU.WriteMediaHistorySubtitleIndex(MRU[i]);
                    break;
                }
            }
        }
    }

    RefreshList();
}

void CHistoryDlg::RemoveMissingFiles()
{
    std::list<CStringW> hashes;
    std::list<CString> paths;

    for (const auto& entry : m_entries) {
        if (entry.fns.IsEmpty()) {
            continue;
        }
        CString path(entry.fns.GetHead());
        if (entry.DVDPosition.llDVDGuid && path.GetLength() == 24 && path.Mid(1).CompareNoCase(_T(":\\VIDEO_TS\\VIDEO_TS.IFO")) == 0) {
            // skip DVD-Video at the root of the disc
            continue;
        }
        if (PathUtils::IsURL(path)) {
            continue;
        }
        if (!::PathFileExistsW(path)) {
            hashes.emplace_back(entry.hash);
            paths.emplace_back(path);
        }
    }

    RemoveEntries(hashes, paths);
}

void CHistoryDlg::RemoveURLs()
{
    std::list<CStringW> hashes;
    std::list<CString> paths;

    for (const auto& entry : m_entries) {
        if (entry.fns.IsEmpty()) {
            continue;
        }
        CString path(entry.fns.GetHead());
        if (PathUtils::IsURL(path)) {
            hashes.emplace_back(entry.hash);
            paths.emplace_back(path);
        }
    }

    RemoveEntries(hashes, paths);
}

void CHistoryDlg::RemoveOlderThan(int days)
{
    // lastOpened timestamps are ISO 8601, so they can be compared as strings
    auto cutoffTime = std::chrono::system_clock::now() - std::chrono::hours(24 * days);
    auto cutoffISO = date::format<wchar_t>(L"%FT%TZ", date::floor<std::chrono::milliseconds>(cutoffTime));
    CString cutoff(cutoffISO.c_str());

    std::list<CStringW> hashes;
    std::list<CString> paths;

    for (const auto& entry : m_entries) {
        if (!entry.lastOpened.IsEmpty() && entry.lastOpened < cutoff) {
            hashes.emplace_back(entry.hash);
            if (!entry.fns.IsEmpty()) {
                paths.emplace_back(entry.fns.GetHead());
            }
        }
    }

    RemoveEntries(hashes, paths);
}

void CHistoryDlg::ClearHistory()
{
    if (IDYES == AfxMessageBox(IDS_RECENT_FILES_QUESTION, MB_ICONQUESTION | MB_YESNO, 0)) {
        AfxGetAppSettings().ClearRecentFiles();
        RefreshList();
    }
}

void CHistoryDlg::DoDataExchange(CDataExchange* pDX)
{
    __super::DoDataExchange(pDX);

    DDX_Control(pDX, IDC_BUTTON1, m_menuButton);
    DDX_Control(pDX, IDC_EDIT1, m_filterEdit);
    DDX_Control(pDX, IDC_BUTTON2, m_removeSelectedButton);
    DDX_Control(pDX, IDC_LIST1, m_list);
}

BEGIN_MESSAGE_MAP(CHistoryDlg, CMPCThemeModelessResizableDialog)
    ON_WM_SHOWWINDOW()
    ON_WM_TIMER()
    ON_WM_DESTROY()
    ON_EN_CHANGE(IDC_EDIT1, OnChangeFilterEdit)
    ON_BN_CLICKED(IDC_BUTTON1, OnBnClickedMenu)
    ON_BN_CLICKED(IDC_BUTTON2, OnBnClickedRemoveSelected)
    ON_NOTIFY(NM_DBLCLK, IDC_LIST1, OnDblclkList)
    ON_NOTIFY(LVN_KEYDOWN, IDC_LIST1, OnKeydownList)
END_MESSAGE_MAP()

// CHistoryDlg message handlers

BOOL CHistoryDlg::OnInitDialog()
{
    EnableSaveRestoreKey(IDS_R_DLG_HISTORY);

    __super::OnInitDialog();

    m_list.setAdditionalStyles(LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES | LVS_EX_DOUBLEBUFFER | LVS_EX_LABELTIP);

    m_list.InsertColumn(COL_PATH, ResStr(IDS_HISTORY_PATH));
    m_list.InsertColumn(COL_TITLE, ResStr(IDS_HISTORY_TITLE));
    m_list.InsertColumn(COL_POS, ResStr(IDS_HISTORY_POSITION));
    m_list.InsertColumn(COL_LAST_OPENED, ResStr(IDS_HISTORY_LAST_OPENED));

    SetupAnchors();
    fulfillThemeReqs();

    return TRUE;
}

void CHistoryDlg::SetupAnchors()
{
    AddAnchor(IDC_BUTTON1, TOP_LEFT);
    AddAnchor(IDC_EDIT1, TOP_LEFT);
    AddAnchor(IDC_BUTTON2, TOP_LEFT);
    AddAnchor(IDC_LIST1, TOP_LEFT, BOTTOM_RIGHT);
}

BOOL CHistoryDlg::PreTranslateMessage(MSG* pMsg)
{
    if (pMsg->message == WM_KEYDOWN) {
        if (pMsg->hwnd == m_list.GetSafeHwnd()) {
            if (pMsg->wParam == VK_RETURN) {
                if (POSITION pos = m_list.GetFirstSelectedItemPosition()) {
                    OpenItem(m_list.GetNextSelectedItem(pos));
                }
                return TRUE;
            }
            if (pMsg->wParam == 'A' && GetKeyState(VK_CONTROL) < 0) {
                for (int i = 0, count = m_list.GetItemCount(); i < count; i++) {
                    m_list.SetItemState(i, LVIS_SELECTED, LVIS_SELECTED);
                }
                return TRUE;
            }
        } else if (pMsg->wParam == VK_RETURN && pMsg->hwnd == m_filterEdit.GetSafeHwnd()) {
            // apply the filter immediately instead of triggering the default button
            KillTimer(m_nFilterTimerID);
            SetupList();
            return TRUE;
        }
    }

    return __super::PreTranslateMessage(pMsg);
}

void CHistoryDlg::OnShowWindow(BOOL bShow, UINT nStatus)
{
    if (bShow) {
        RefreshList();
        ApplyColumnWidths();
    } else if (IsWindow(m_list.GetSafeHwnd())) {
        SaveColumnWidths();
    }

    __super::OnShowWindow(bShow, nStatus);
}

void CHistoryDlg::OnTimer(UINT_PTR nIDEvent)
{
    if (nIDEvent == m_nFilterTimerID) {
        KillTimer(m_nFilterTimerID);
        SetupList();
    } else {
        __super::OnTimer(nIDEvent);
    }
}

void CHistoryDlg::OnChangeFilterEdit()
{
    KillTimer(m_nFilterTimerID);
    m_nFilterTimerID = SetTimer(2, 100, nullptr);
}

void CHistoryDlg::OnBnClickedMenu()
{
    enum {
        M_COPY_SELECTED = 1,
        M_REMOVE_SELECTED,
        M_RESET_POSITION,
        M_RESET_TRACKS,
        M_REMOVE_MISSING,
        M_REMOVE_URLS,
        M_REMOVE_OLDER_WEEK,
        M_REMOVE_OLDER_MONTH,
        M_CLEAR
    };

    bool canResetPosition = false;
    bool canResetTracks = false;
    POSITION pos = m_list.GetFirstSelectedItemPosition();
    while (pos && !(canResetPosition && canResetTracks)) {
        int nItem = m_list.GetNextSelectedItem(pos);
        size_t index = m_list.GetItemData(nItem);
        if (index < m_entries.size()) {
            const auto& entry = m_entries[index];
            if (HasSavedPosition(entry)) {
                canResetPosition = true;
            }
            if (entry.AudioTrackIndex != -1 || entry.SubtitleTrackIndex != -1) {
                canResetTracks = true;
            }
        }
    }

    CMPCThemeMenu menu;
    menu.CreatePopupMenu();
    menu.AppendMenu(MF_STRING | MF_ENABLED, M_COPY_SELECTED, ResStr(IDS_HISTORY_COPY_PATHS) + _T("\tCtrl+C"));
    menu.AppendMenu(MF_STRING | MF_ENABLED, M_REMOVE_SELECTED, ResStr(IDS_HISTORY_REMOVE_SELECTED) + _T("\tDelete"));
    menu.AppendMenu(MF_SEPARATOR);
    menu.AppendMenu(MF_STRING | (canResetPosition ? MF_ENABLED : MF_GRAYED), M_RESET_POSITION, ResStr(IDS_HISTORY_RESET_POSITION));
    menu.AppendMenu(MF_STRING | (canResetTracks ? MF_ENABLED : MF_GRAYED), M_RESET_TRACKS, ResStr(IDS_HISTORY_RESET_TRACKS));
    menu.AppendMenu(MF_SEPARATOR);
    menu.AppendMenu(MF_STRING | MF_ENABLED, M_REMOVE_MISSING, ResStr(IDS_HISTORY_REMOVE_MISSING));
    menu.AppendMenu(MF_STRING | MF_ENABLED, M_REMOVE_URLS, ResStr(IDS_HISTORY_REMOVE_URLS));
    menu.AppendMenu(MF_STRING | MF_ENABLED, M_REMOVE_OLDER_WEEK, ResStr(IDS_HISTORY_REMOVE_OLDER_WEEK));
    menu.AppendMenu(MF_STRING | MF_ENABLED, M_REMOVE_OLDER_MONTH, ResStr(IDS_HISTORY_REMOVE_OLDER_MONTH));
    menu.AppendMenu(MF_SEPARATOR);
    menu.AppendMenu(MF_STRING | MF_ENABLED, M_CLEAR, ResStr(IDS_HISTORY_CLEAR));
    if (AppIsThemeLoaded()) {
        menu.fulfillThemeReqs();
    }

    CRect r;
    m_menuButton.GetWindowRect(r);

    switch (menu.TrackPopupMenu(TPM_LEFTBUTTON | TPM_RETURNCMD, r.left, r.bottom, this)) {
        case M_COPY_SELECTED:
            CopySelectedPaths();
            break;
        case M_REMOVE_SELECTED:
            ConfirmRemoveSelected();
            break;
        case M_RESET_POSITION:
            ResetPosition();
            break;
        case M_RESET_TRACKS:
            ResetTrackSelection();
            break;
        case M_REMOVE_MISSING:
            RemoveMissingFiles();
            break;
        case M_REMOVE_URLS:
            RemoveURLs();
            break;
        case M_REMOVE_OLDER_WEEK:
            RemoveOlderThan(7);
            break;
        case M_REMOVE_OLDER_MONTH:
            RemoveOlderThan(30);
            break;
        case M_CLEAR:
            ClearHistory();
            break;
    }
}

void CHistoryDlg::OnBnClickedRemoveSelected()
{
    ConfirmRemoveSelected();
}

void CHistoryDlg::OnDblclkList(NMHDR* pNMHDR, LRESULT* pResult)
{
    int nItem = ((NM_LISTVIEW*)pNMHDR)->iItem;
    if (nItem >= 0) {
        OpenItem(nItem);
    }

    *pResult = 0;
}

void CHistoryDlg::OnKeydownList(NMHDR* pNMHDR, LRESULT* pResult)
{
    NMLVKEYDOWN* pLVKeyDown = (NMLVKEYDOWN*)pNMHDR;

    if (pLVKeyDown->wVKey == VK_DELETE) {
        ConfirmRemoveSelected();
    } else if (pLVKeyDown->wVKey == 'C' && GetKeyState(VK_CONTROL) < 0) {
        CopySelectedPaths();
    }

    *pResult = 0;
}

void CHistoryDlg::OnDestroy()
{
    SaveColumnWidths();

    __super::OnDestroy();
}
