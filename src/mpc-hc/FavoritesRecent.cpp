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
#include "FavoritesRecent.h"
#include "MainFrm.h"
#include "mplayerc.h"
#include "FavoriteAddDlg.h"
#include "HistoryDlg.h"
#include "PathUtils.h"
#include <DSUtil.h>

bool FileFavorite::TryParse(const CString& fav, FileFavorite& ff)
{
    CAtlList<CString> parts;
    return TryParse(fav, ff, parts);
}

bool FileFavorite::TryParse(const CString& fav, FileFavorite& ff, CAtlList<CString>& parts)
{
    ExplodeEsc(fav, parts, _T(';'));
    if (parts.IsEmpty()) {
        return false;
    }

    ff.Name = parts.RemoveHead();

    if (!parts.IsEmpty()) {
        // Start position and optional A-B marks "pos[:A:B]"
        auto startPos = parts.RemoveHead();
        _stscanf_s(startPos, _T("%I64d:%I64d:%I64d"), &ff.Start, &ff.MarkA, &ff.MarkB);
        ff.Start = std::max(ff.Start, 0ll); // Sanitize
    }
    if (!parts.IsEmpty()) {
        _stscanf_s(parts.RemoveHead(), _T("%d"), &ff.RelativeDrive);
    }
    return true;
}

CString FileFavorite::ToString() const
{
    CString str;
    if (RelativeDrive) {
        str = _T("[RD]");
    }
    if (Start > 0) {    // Start position
        str.AppendFormat(_T("[%s]"), ReftimeToString2(Start).GetString());
    }
    if (MarkA > 0 || MarkB > 0) {   // A-B marks (only characters to save space)
        CString abMarks;
        if (MarkA > 0) {
            abMarks = _T("A");
        }
        if (MarkB > 0) {
            abMarks.Append(_T("-B"));
        }
        str.AppendFormat(_T("[%s]"), abMarks.GetString());
    }
    return str;
}

class CDVDStateStream : public CUnknown, public IStream
{
    STDMETHODIMP NonDelegatingQueryInterface(REFIID riid, void** ppv) {
        return
            QI(IStream)
            CUnknown::NonDelegatingQueryInterface(riid, ppv);
    }

    size_t m_pos;

public:
    CDVDStateStream() : CUnknown(NAME("CDVDStateStream"), nullptr) {
        m_pos = 0;
    }

    DECLARE_IUNKNOWN;

    CAtlArray<BYTE> m_data;

    // ISequentialStream
    STDMETHODIMP Read(void* pv, ULONG cb, ULONG* pcbRead) {
        size_t cbRead = std::min(m_data.GetCount() - m_pos, size_t(cb));
        cbRead = std::max(cbRead, size_t(0));
        if (cbRead) {
            memcpy(pv, &m_data[m_pos], cbRead);
        }
        if (pcbRead) {
            *pcbRead = (ULONG)cbRead;
        }
        m_pos += cbRead;
        return S_OK;
    }
    STDMETHODIMP Write(const void* pv, ULONG cb, ULONG* pcbWritten) {
        BYTE* p = (BYTE*)pv;
        ULONG cbWritten = (ULONG) - 1;
        while (++cbWritten < cb) {
            m_data.Add(*p++);
        }
        if (pcbWritten) {
            *pcbWritten = cbWritten;
        }
        return S_OK;
    }

    // IStream
    STDMETHODIMP Seek(LARGE_INTEGER dlibMove, DWORD dwOrigin, ULARGE_INTEGER* plibNewPosition) {
        return E_NOTIMPL;
    }

    STDMETHODIMP SetSize(ULARGE_INTEGER libNewSize) {
        return E_NOTIMPL;
    }

    STDMETHODIMP CopyTo(IStream* pstm, ULARGE_INTEGER cb, ULARGE_INTEGER* pcbRead, ULARGE_INTEGER* pcbWritten) {
        return E_NOTIMPL;
    }

    STDMETHODIMP Commit(DWORD grfCommitFlags) {
        return E_NOTIMPL;
    }

    STDMETHODIMP Revert() {
        return E_NOTIMPL;
    }

    STDMETHODIMP LockRegion(ULARGE_INTEGER libOffset, ULARGE_INTEGER cb, DWORD dwLockType) {
        return E_NOTIMPL;
    }

    STDMETHODIMP UnlockRegion(ULARGE_INTEGER libOffset, ULARGE_INTEGER cb, DWORD dwLockType) {
        return E_NOTIMPL;
    }

    STDMETHODIMP Stat(STATSTG* pstatstg, DWORD grfStatFlag) {
        return E_NOTIMPL;
    }

    STDMETHODIMP Clone(IStream** ppstm) {
        return E_NOTIMPL;
    }
};

BEGIN_MESSAGE_MAP(CFavoritesRecent, CCmdTarget)
    ON_COMMAND(ID_FAVORITES_ADD, OnFavoritesAdd)
    ON_UPDATE_COMMAND_UI(ID_FAVORITES_ADD, OnUpdateFavoritesAdd)
    ON_COMMAND(ID_FAVORITES_QUICKADDFAVORITE, OnFavoritesQuickAddFavorite)
    ON_COMMAND(ID_FAVORITES_ORGANIZE, OnFavoritesOrganize)
    ON_UPDATE_COMMAND_UI(ID_FAVORITES_ORGANIZE, OnUpdateFavoritesOrganize)
    ON_COMMAND_RANGE(ID_FAVORITES_FILE_START, ID_FAVORITES_FILE_END, OnFavoritesFile)
    ON_UPDATE_COMMAND_UI_RANGE(ID_FAVORITES_FILE_START, ID_FAVORITES_FILE_END, OnUpdateFavoritesFile)
    ON_COMMAND_RANGE(ID_FAVORITES_DVD_START, ID_FAVORITES_DVD_END, OnFavoritesDVD)
    ON_UPDATE_COMMAND_UI_RANGE(ID_FAVORITES_DVD_START, ID_FAVORITES_DVD_END, OnUpdateFavoritesDVD)
    ON_COMMAND_RANGE(ID_FAVORITES_DEVICE_START, ID_FAVORITES_DEVICE_END, OnFavoritesDevice)
    ON_UPDATE_COMMAND_UI_RANGE(ID_FAVORITES_DEVICE_START, ID_FAVORITES_DEVICE_END, OnUpdateFavoritesDevice)

    ON_COMMAND(ID_RECENT_FILES_CLEAR, OnRecentFileClear)
    ON_UPDATE_COMMAND_UI(ID_RECENT_FILES_CLEAR, OnUpdateRecentFileClear)
    ON_COMMAND_RANGE(ID_RECENT_FILE_START, ID_RECENT_FILE_END, OnRecentFile)
    ON_UPDATE_COMMAND_UI_RANGE(ID_RECENT_FILE_START, ID_RECENT_FILE_END, OnUpdateRecentFile)
    ON_COMMAND(ID_RECENT_FILES_SHOW_HISTORY, OnShowHistory)
END_MESSAGE_MAP()

CFavoritesRecent::CFavoritesRecent(CMainFrame& frame)
    : m_frame(frame)
    , m_graph(frame.Graph())
    , recentFilesMenuFromMRUSequence(-1)
    , m_wndFavoriteOrganizeDialog(&frame)
{
}

LPCTSTR CFavoritesRecent::GetRecentFile() const
{
    auto& MRU = AfxGetAppSettings().MRU;
    MRU.ReadMediaHistory();
    for (int i = 0; i < MRU.GetSize(); i++) {
        if (MRU[i].fns.GetCount() > 0 && !MRU[i].fns.GetHead().IsEmpty()) {
            return MRU[i].fns.GetHead();
        }
    }
    return nullptr;
}

void CFavoritesRecent::AddFavorite(bool fDisplayMessage, bool fShowDialog)
{
    CAppSettings& s = AfxGetAppSettings();
    CAtlList<CString> args;
    WORD osdMsg = 0;
    const TCHAR sep = _T(';');

    // Lambda to conditionally add favorite: to visual list if dialog is open, or persist immediately
    auto addFavorite = [this, &s](favtype favType, const CString& str) {
        if (::IsWindow(m_wndFavoriteOrganizeDialog.m_hWnd) && m_wndFavoriteOrganizeDialog.IsWindowVisible()) {
            m_wndFavoriteOrganizeDialog.AddItemToVisualList(favType, str);
        } else {
            s.AddFav(favType, str);
        }
    };

    if (m_frame.GetPlaybackMode() == PM_FILE) {
        bool is_BD = false;
        CString fn = m_frame.m_wndPlaylistBar.GetCurFileNameTitle();
        if (fn.IsEmpty()) {
            if (m_graph.m_pFSF) {
                CComHeapPtr<OLECHAR> pFN;
                AM_MEDIA_TYPE mt;
                if (SUCCEEDED(m_graph.m_pFSF->GetCurFile(&pFN, &mt)) && pFN && *pFN) {
                    fn = CStringW(pFN);
                }
            }
            if (fn.IsEmpty()) {
                return;
            }
            is_BD = true;
        }

        CString desc = m_frame.GetFileName();

        // capture the rest of the entry here as well, the file can change while the modal pump runs
        CPlaylistItem pli = CPlaylistItem(true);
        bool bHasCur = !is_BD && m_frame.m_wndPlaylistBar.GetCur(pli);
        REFERENCE_TIME rtPos = m_frame.GetPos();
        ABRepeat ab = m_frame.abRepeat;

        // Name
        CString name;
        if (fShowDialog) {
            BOOL bEnableABMarks = static_cast<bool>(m_frame.abRepeat);
            CFavoriteAddDlg dlg(desc, fn, bEnableABMarks);
            if (dlg.DoModal() != IDOK) {
                return;
            }
            name = dlg.m_name;
        } else {
            name = desc;
        }
        args.AddTail(name);

        // RememberPos
        CString posStr = _T("0");
        if (s.bFavRememberPos) {
            posStr.Format(_T("%I64d"), rtPos);
        }
        // RememberABMarks
        if (s.bFavRememberABMarks && ab) {
            posStr.AppendFormat(_T(":%I64d:%I64d"), ab.positionA, ab.positionB);
        }
        args.AddTail(posStr);

        // RelativeDrive
        CString relativeDrive;
        relativeDrive.Format(_T("%d"), s.bFavRelativeDrive);

        args.AddTail(relativeDrive);

        // Paths
        if (is_BD) {
            args.AddTail(fn);
        } else {
            if (bHasCur) {
                if (pli.m_bYoutubeDL) {
                    args.AddTail(pli.m_ydlSourceURL);
                } else {
                    POSITION pos = pli.m_fns.GetHeadPosition();
                    while (pos) {
                        args.AddTail(pli.m_fns.GetNext(pos));
                    }
                }
            }
        }

        CString str = ImplodeEsc(args, sep);
        addFavorite(FAV_FILE, str);
        osdMsg = IDS_FILE_FAV_ADDED;
    } else if (m_frame.GetPlaybackMode() == PM_DVD) {
        WCHAR path[MAX_PATH];
        ULONG len = 0;
        if (SUCCEEDED(m_graph.m_pDVDI->GetDVDDirectory(path, MAX_PATH, &len))) {
            CString fn = path;
            fn.TrimRight(_T("/\\"));

            DVD_PLAYBACK_LOCATION2 Location;
            CString desc;
            if (SUCCEEDED(m_graph.m_pDVDI->GetCurrentLocation(&Location))) {
                desc.Format(_T("%s - T%02u C%02u - %02u:%02u:%02u"), fn.GetString(), Location.TitleNum, Location.ChapterNum,
                            Location.TimeCode.bHours, Location.TimeCode.bMinutes, Location.TimeCode.bSeconds);
            } else {
                desc = fn;
            }

            // capture the state here as well, the disc position can change while the modal pump runs
            CString state;
            {
                CDVDStateStream stream;
                stream.AddRef();

                CComPtr<IDvdState> pStateData;
                CComQIPtr<IPersistStream> pPersistStream;
                if (SUCCEEDED(m_graph.m_pDVDI->GetState(&pStateData))
                        && (pPersistStream = pStateData)
                        && SUCCEEDED(OleSaveToStream(pPersistStream, (IStream*)&stream))) {
                    state = BinToCString(stream.m_data.GetData(), stream.m_data.GetCount());
                }
            }

            // Name
            CString name;
            if (fShowDialog) {
                CFavoriteAddDlg dlg(fn, desc);
                if (dlg.DoModal() != IDOK) {
                    return;
                }
                name = dlg.m_name;
            } else {
                name = s.bFavRememberPos ? desc : fn;
            }
            args.AddTail(name);

            // RememberPos
            CString pos(_T("0"));
            if (s.bFavRememberPos && !state.IsEmpty()) {
                pos = state;
            }

            args.AddTail(pos);

            // Paths
            args.AddTail(fn);

            CString str = ImplodeEsc(args, sep);
            addFavorite(FAV_DVD, str);
            osdMsg = IDS_DVD_FAV_ADDED;
        }
    } // TODO: PM_ANALOG_CAPTURE and PM_DIGITAL_CAPTURE

    if (fDisplayMessage && osdMsg) {
        CString osdMsgStr(StrRes(osdMsg));
        m_frame.SendStatusMessage(osdMsgStr, 3000);
        m_frame.m_OSD.DisplayMessage(OSD_TOPLEFT, osdMsgStr, 3000);
    }
}

void CFavoritesRecent::OnFavoritesAdd()
{
    AddFavorite();
}

void CFavoritesRecent::OnUpdateFavoritesAdd(CCmdUI* pCmdUI)
{
    pCmdUI->Enable(m_frame.GetPlaybackMode() == PM_FILE || m_frame.GetPlaybackMode() == PM_DVD);
}

void CFavoritesRecent::OnFavoritesQuickAddFavorite()
{
    AddFavorite(true, false);
}

void CFavoritesRecent::OnFavoritesOrganize()
{
    m_wndFavoriteOrganizeDialog.ShowAndLoad();
}

void CFavoritesRecent::OnUpdateFavoritesOrganize(CCmdUI* pCmdUI)
{
    const CAppSettings& s = AfxGetAppSettings();
    CAtlList<CString> sl;
    s.GetFav(FAV_FILE, sl);
    bool enable = !sl.IsEmpty();

    if (!enable) {
        s.GetFav(FAV_DVD, sl);
        enable = !sl.IsEmpty();
    }

    pCmdUI->Enable(enable);
}

void CFavoritesRecent::OnRecentFileClear()
{
    if (IDYES != AfxMessageBox(IDS_RECENT_FILES_QUESTION, MB_ICONQUESTION | MB_YESNO, 0)) {
        return;
    }

    CAppSettings& s = AfxGetAppSettings();
    s.ClearRecentFiles();
}

void CFavoritesRecent::OnUpdateRecentFileClear(CCmdUI* pCmdUI)
{
    // TODO: Add your command update UI handler code here
}

void CFavoritesRecent::OnFavoritesFile(UINT nID)
{
    nID -= ID_FAVORITES_FILE_START;
    CAtlList<CString> sl;
    AfxGetAppSettings().GetFav(FAV_FILE, sl);

    if (POSITION pos = sl.FindIndex(nID)) {
        PlayFavoriteFile(sl.GetAt(pos));
    }
}

void CFavoritesRecent::PlayFavoriteFile(const CString& fav)
{
    if (m_frame.DeferIfNested(CMainFrame::DeferredActionType::Open, [this, fav] { PlayFavoriteFile(fav); })) {
        return;
    }

    CAtlList<CString> args;
    REFERENCE_TIME rtStart = 0;
    FileFavorite ff = ParseFavoriteFile(fav, args, &rtStart);
    // closing the current file clears abRepeat, so keep the favorite's marks for the open
    ABRepeat favABRepeat = m_frame.abRepeat;

    auto firstFile = args.GetHead();
    if (!m_frame.m_wndPlaylistBar.SelectFileInPlaylist(firstFile)) {
        if (m_frame.CanSendToYoutubeDL(firstFile)) {
            if (!m_frame.CloseMediaBeforeOpen()) {
                return;
            }
            if (!m_frame.ProcessYoutubeDLURL(firstFile, false)) {
                m_frame.m_wndPlaylistBar.Open(args, false);
            }
        } else {
            m_frame.m_wndPlaylistBar.Open(args, false);
        }
    }

    m_frame.m_wndPlaylistBar.SetCurLabel(ff.Name);

    if (m_frame.IsStateLoaded() && m_frame.GetPlaybackMode() == PM_FILE && m_frame.m_lastOMD && args.GetHead() == m_frame.m_lastOMD->title) {
        m_frame.m_wndSeekBar.Invalidate();
        m_frame.DoSeekTo(rtStart);
    } else {
        if (!m_frame.CloseMediaBeforeOpen()) {
            return;
        }
        m_frame.OpenCurPlaylistItem(rtStart, false, favABRepeat);
    }

}

FileFavorite CFavoritesRecent::ParseFavoriteFile(const CString& fav, CAtlList<CString>& args, REFERENCE_TIME* prtStart)
{
    FileFavorite ff;
    VERIFY(FileFavorite::TryParse(fav, ff, args));

    m_frame.abRepeat.positionA = ff.MarkA;
    m_frame.abRepeat.positionB = ff.MarkB;

    // Start at mark A (if set)
    ff.Start = std::max(ff.Start, m_frame.abRepeat.positionA);

    if (prtStart) {
        *prtStart = ff.Start;
    }

    // NOTE: This is just for the favorites but we could add a global settings that
    // does this always when on. Could be useful when using removable devices.
    // All you have to do then is plug in your 500 gb drive, full with movies and/or music,
    // start MPC-HC (from the 500 gb drive) with a preloaded playlist and press play.
    if (ff.RelativeDrive) {
        // Get the drive MPC-HC is on and apply it to the path list
        CString exePath = PathUtils::GetProgramPath(true);

        CLongPath exeDrive(exePath);

        if (exeDrive.StripToRoot()) {
            POSITION pos = args.GetHeadPosition();

            while (pos != nullptr) {
                CString& stringPath = args.GetNext(pos);    // Note the reference (!)
                CLongPath path(stringPath);

                int rootLength = path.SkipRoot();

                if (path.StripToRoot()) {
                    if (_tcsicmp(exeDrive, path) != 0) {   // Do we need to replace the drive letter ?
                        // Replace drive letter
                        CString newPath(exeDrive);

                        newPath += stringPath.Mid(rootLength);

                        stringPath = newPath;   // Note: Changes args.GetHead()
                    }
                }
            }
        }
    }
    return ff;
}

void CFavoritesRecent::OnUpdateFavoritesFile(CCmdUI* pCmdUI)
{
    //UINT nID = pCmdUI->m_nID - ID_FAVORITES_FILE_START;
}

void CFavoritesRecent::OnRecentFile(UINT nID)
{
    auto& MRU = AfxGetAppSettings().MRU;
    RecentFileEntry r;

    // find corresponding item in MRU list, we can't directly use string from menu because it may have been shortened
    nID -= ID_RECENT_FILE_START;
    if (nID < (UINT)MRU.GetSize()) {
        r = MRU[nID];
    } else {
        ASSERT(false);
        return;
    }

    OpenRecentFileEntry(r);
}

void CFavoritesRecent::OpenRecentFileEntry(RecentFileEntry& r)
{
    if (m_frame.DeferIfNested(CMainFrame::DeferredActionType::Open, [this, r] { RecentFileEntry entry(r); OpenRecentFileEntry(entry); })) {
        return;
    }

    CAtlList<CString> fns;
    fns.AddHeadList(&r.fns);

    if (fns.GetCount() == 1 && m_frame.CanSendToYoutubeDL(r.fns.GetHead())) {
        if (!m_frame.CloseMediaBeforeOpen()) {
            return;
        }
        if (m_frame.ProcessYoutubeDLURL(fns.GetHead(), false)) {
            m_frame.OpenCurPlaylistItem();
            return;
        } else if (m_frame.IsOnYDLWhitelist(fns.GetHead())) {
            m_frame.m_closingmsg = L"Failed to extract stream URL with yt-dlp/youtube-dl";
            m_frame.m_wndStatusBar.SetStatusMessage(m_frame.m_closingmsg);
            // don't bother trying to open this website URL directly
            return;
        }
    }

    CAtlList<CString> subs;
    subs.AddHeadList(&r.subs);

    if (!m_frame.m_wndPlaylistBar.SelectFileInPlaylist(fns.GetHead())) {
        m_frame.m_wndPlaylistBar.Open(fns, false, &subs, r.title, _T(""), r.cue);
    }
    else {
        m_frame.m_wndPlaylistBar.ReplaceCurrentItem(fns, &subs, r.title, _T(""), r.cue);
    }

    m_frame.PostMessage(WM_MPC_OPENCURPLAYLIST, 0, 0);
}

void CFavoritesRecent::OnUpdateRecentFile(CCmdUI* pCmdUI)
{
    //UINT nID = pCmdUI->m_nID - ID_RECENT_FILE_START;
}

void CFavoritesRecent::OnShowHistory()
{
    if (!m_pHistoryDlg) {
        m_pHistoryDlg = std::make_unique<CHistoryDlg>(&m_frame);
        m_pHistoryDlg->Create(IDD_HISTORY, &m_frame);
    }
    if (m_pHistoryDlg->IsWindowVisible()) {
        m_pHistoryDlg->SetActiveWindow();
    } else {
        m_pHistoryDlg->ShowWindow(SW_SHOW);
    }
}

void CFavoritesRecent::OnFavoritesDVD(UINT nID)
{
    nID -= ID_FAVORITES_DVD_START;

    CAtlList<CString> sl;
    AfxGetAppSettings().GetFav(FAV_DVD, sl);

    if (POSITION pos = sl.FindIndex(nID)) {
        PlayFavoriteDVD(sl.GetAt(pos));
    }
}

void CFavoritesRecent::PlayFavoriteDVD(CString fav)
{
    if (m_frame.DeferIfNested(CMainFrame::DeferredActionType::Open, [this, fav] { PlayFavoriteDVD(fav); })) {
        return;
    }
    if (!m_frame.CloseMediaBeforeOpen()) {
        return;
    }

    CAtlList<CString> args;
    CString fn;
    CDVDStateStream stream;

    stream.AddRef();

    ExplodeEsc(fav, args, _T(';'), 3);
    args.RemoveHeadNoReturn(); // desc / name
    CString state = args.RemoveHead(); // state
    if (state != _T("0")) {
        CStringToBin(state, stream.m_data);
    }
    fn = args.RemoveHead(); // path

    CComPtr<IDvdState> pDvdState;
    HRESULT hr = OleLoadFromStream((IStream*)&stream, IID_PPV_ARGS(&pDvdState));
    UNREFERENCED_PARAMETER(hr);

    CAutoPtr<OpenDVDData> p(DEBUG_NEW OpenDVDData());
    if (p) {
        p->path = fn;
        p->pDvdState = pDvdState;
        m_frame.m_wndPlaylistBar.OpenDVD(p->path);
    }
    m_frame.OpenMedia(p);
}

void CFavoritesRecent::OnUpdateFavoritesDVD(CCmdUI* pCmdUI)
{
    //UINT nID = pCmdUI->m_nID - ID_FAVORITES_DVD_START;
}

void CFavoritesRecent::OnFavoritesDevice(UINT nID)
{
    //nID -= ID_FAVORITES_DEVICE_START;
}

void CFavoritesRecent::OnUpdateFavoritesDevice(CCmdUI* pCmdUI)
{
    //UINT nID = pCmdUI->m_nID - ID_FAVORITES_DEVICE_START;
}

void CFavoritesRecent::SetupRecentFilesSubMenu()
{
    auto& s = AfxGetAppSettings();
    auto& MRU = s.MRU;
    MRU.ReadMediaHistory();

    if (MRU.listModifySequence == recentFilesMenuFromMRUSequence) {
        return;
    }
    recentFilesMenuFromMRUSequence = MRU.listModifySequence;

    CMPCThemeMenu& subMenu = m_frame.m_recentFilesMenu;
    // Empty the menu
    while (subMenu.RemoveMenu(0, MF_BYPOSITION));
   
    if (!s.fKeepHistory) {
        return;
    }

    VERIFY(subMenu.AppendMenu(MF_STRING | MF_ENABLED, ID_RECENT_FILES_SHOW_HISTORY, ResStr(IDS_HISTORY_SHOW)));

    if (MRU.GetSize() > 0) {
        VERIFY(subMenu.AppendMenu(MF_STRING | MF_ENABLED, ID_RECENT_FILES_CLEAR, ResStr(IDS_RECENT_FILES_CLEAR)));
        VERIFY(subMenu.AppendMenu(MF_SEPARATOR | MF_ENABLED));
        UINT id = ID_RECENT_FILE_START;
        for (int i = 0; i < MRU.GetSize(); i++) {
            UINT flags = MF_BYCOMMAND | MF_STRING | MF_ENABLED;
            if (!MRU[i].fns.IsEmpty() && !MRU[i].fns.GetHead().IsEmpty()) {
                CString p = MRU[i].cue.IsEmpty() ? MRU[i].fns.GetHead() : MRU[i].cue;
                if (s.bUseTitleInRecentFileList && !MRU[i].title.IsEmpty()) {
                    CString title(MRU[i].title);
                    if (title.GetLength() > 100) {
                        title = title.Left(40) + _T("~~~") + title.Right(57);
                    }
                    int targetlen = 150 - title.GetLength();
                    if (PathUtils::IsURL(p)) {
                        if (title.Right(1) == L')') {
                            // probably already contains shorturl
                            p = title;
                        } else {
                            CString shorturl = ShortenURL(p, targetlen, true);
                            p.Format(_T("%s (%s)"), static_cast<LPCWSTR>(title), static_cast<LPCWSTR>(shorturl));
                        }
                    } else {
                        CString fn = PathUtils::StripPathOrUrl(p);
                        if (fn.GetLength() > targetlen) { // If file name is too long, cut middle part.
                            int l = fn.GetLength();
                            fn.Format(_T("%s~~~%s"), static_cast<LPCWSTR>(fn.Left(l / 2 - 2 + (l % 2))), static_cast<LPCWSTR>(fn.Right(l / 2 - 1)));
                        }
                        p.Format(_T("%s (%s)"), static_cast<LPCWSTR>(title), static_cast<LPCWSTR>(fn));
                    }
                }
                else {
                    if (PathUtils::IsURL(p)) {
                        p = ShortenURL(p, 150);
                    }
                    if (p.GetLength() > 150) {
                        p.Format(_T("%s~~~%s"), static_cast<LPCWSTR>(p.Left(60)), static_cast<LPCWSTR>(p.Right(87)));
                    }
                }
                p = SanitizeMenuLabel(p, 0); //already shortened above, in a way that keeps the extension visible
                VERIFY(subMenu.AppendMenu(flags, id, p));
            } else {
                ASSERT(false);
            }
            id++;
        }
    }
}

void CFavoritesRecent::SetupFavoritesSubMenu()
{
    CMPCThemeMenu& subMenu = m_frame.m_favoritesMenu;
    // Empty the menu
    while (subMenu.RemoveMenu(0, MF_BYPOSITION));

    const CAppSettings& s = AfxGetAppSettings();

    VERIFY(subMenu.AppendMenu(MF_STRING | MF_ENABLED, ID_FAVORITES_ADD, ResStr(IDS_FAVORITES_ADD)));
    VERIFY(subMenu.AppendMenu(MF_STRING | MF_ENABLED, ID_FAVORITES_ORGANIZE, ResStr(IDS_FAVORITES_ORGANIZE)));

    UINT nLastGroupStart = subMenu.GetMenuItemCount();
    UINT id = ID_FAVORITES_FILE_START;
    CAtlList<CString> favs;
    AfxGetAppSettings().GetFav(FAV_FILE, favs);
    POSITION pos = favs.GetHeadPosition();

    while (pos) {
        UINT flags = MF_BYCOMMAND | MF_STRING | MF_ENABLED;

        CString f_str = favs.GetNext(pos);

        FileFavorite ff;
        VERIFY(FileFavorite::TryParse(f_str, ff)); //parse before sanitizing, the escaping is not part of the format

        f_str = ff.Name;
        if (!f_str.IsEmpty()) {
            f_str = SanitizeMenuLabel(f_str);
        }

        CString str = ff.ToString();
        if (!str.IsEmpty()) {
            f_str.AppendFormat(_T("\t%s"), str.GetString());
        }

        if (!f_str.IsEmpty()) {
            VERIFY(subMenu.AppendMenu(flags, id, f_str));
        }

        id++;
        if (id > ID_FAVORITES_FILE_END) {
            break;
        }
    }

    if (id > ID_FAVORITES_FILE_START) {
        VERIFY(subMenu.InsertMenu(nLastGroupStart, MF_SEPARATOR | MF_ENABLED | MF_BYPOSITION));
    }

    nLastGroupStart = subMenu.GetMenuItemCount();

    id = ID_FAVORITES_DVD_START;
    s.GetFav(FAV_DVD, favs);
    pos = favs.GetHeadPosition();

    while (pos) {
        UINT flags = MF_BYCOMMAND | MF_STRING | MF_ENABLED;

        CString str = favs.GetNext(pos);

        CAtlList<CString> sl;
        ExplodeEsc(str, sl, _T(';'), 2);

        str = sl.RemoveHead();

        if (!sl.IsEmpty()) {
            // TODO
        }

        if (!str.IsEmpty()) {
            VERIFY(subMenu.AppendMenu(flags, id, SanitizeMenuLabel(str)));
        }

        id++;
        if (id > ID_FAVORITES_DVD_END) {
            break;
        }
    }

    if (id > ID_FAVORITES_DVD_START) {
        VERIFY(subMenu.InsertMenu(nLastGroupStart, MF_SEPARATOR | MF_ENABLED | MF_BYPOSITION));
    }

    nLastGroupStart = subMenu.GetMenuItemCount();

    id = ID_FAVORITES_DEVICE_START;

    s.GetFav(FAV_DEVICE, favs);

    pos = favs.GetHeadPosition();
    while (pos) {
        UINT flags = MF_BYCOMMAND | MF_STRING | MF_ENABLED;

        CString str = favs.GetNext(pos);

        CAtlList<CString> sl;
        ExplodeEsc(str, sl, _T(';'), 2);

        str = sl.RemoveHead();

        if (!str.IsEmpty()) {
            VERIFY(subMenu.AppendMenu(flags, id, SanitizeMenuLabel(str)));
        }

        id++;
        if (id > ID_FAVORITES_DEVICE_END) {
            break;
        }
    }
}
