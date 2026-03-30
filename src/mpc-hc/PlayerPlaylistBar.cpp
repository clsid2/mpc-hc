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
#include <cmath>
#include <afxinet.h>
#include "mplayerc.h"
#include "MainFrm.h"
#include "DSUtil.h"
#include "SaveTextFileDialog.h"
#include "PlayerPlaylistBar.h"
#include "SettingsDefines.h"
#include "InternalFiltersConfig.h"
#include "PathUtils.h"
#include "WinAPIUtils.h"
#include "CMPCTheme.h"
#include "CoverArt.h"
#include "FileHandle.h"
#include "MediaInfo/MediaInfoDLL.h"

#undef SubclassWindow



IMPLEMENT_DYNAMIC(CPlayerPlaylistBar, CMPCThemePlayerBar)
CPlayerPlaylistBar::CPlayerPlaylistBar(CMainFrame* pMainFrame)
    : CMPCThemePlayerBar(pMainFrame)
    , m_pMainFrame(pMainFrame)
    , m_list()
    , m_nTimeColWidth(0)
    , m_pDragImage(nullptr)
    , m_bDragging(FALSE)
    , m_nDragIndex(0)
    , m_nDropIndex(0)
    , m_bHiddenDueToFullscreen(false)
    , m_pl(AfxGetAppSettings().bShufflePlaylistItems)
    , createdWindow(false)
    , inlineEditXpos(0)
    , m_tcLastSave(0)
    , m_SaveDelayed(false)
    , m_insertingPos(nullptr)
{
    GetEventd().Connect(m_eventc, {
        MpcEvent::DPI_CHANGED,
    }, std::bind(&CPlayerPlaylistBar::EventCallback, this, std::placeholders::_1));
}

CPlayerPlaylistBar::~CPlayerPlaylistBar()
{
}

BOOL CPlayerPlaylistBar::Create(CWnd* pParentWnd, UINT defDockBarID)
{
    if (!__super::Create(ResStr(IDS_PLAYLIST_CAPTION), pParentWnd, ID_VIEW_PLAYLIST, defDockBarID, _T("Playlist"))) {
        return FALSE;
    }

    m_list.CreateEx(
        WS_EX_DLGMODALFRAME | WS_EX_CLIENTEDGE,
        WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_CLIPCHILDREN | WS_TABSTOP
        | LVS_OWNERDRAWFIXED
        | LVS_OWNERDATA
        | LVS_NOCOLUMNHEADER
        | LVS_REPORT | LVS_SINGLESEL | LVS_AUTOARRANGE | LVS_NOSORTHEADER, // TODO: remove LVS_SINGLESEL and implement multiple item repositioning (dragging is ready)
        CRect(0, 0, 100, 100), this, IDC_PLAYLIST);

    m_list.SetExtendedStyle(m_list.GetExtendedStyle() | LVS_EX_FULLROWSELECT | LVS_EX_DOUBLEBUFFER);

    // The column titles don't have to be translated since they aren't displayed anyway
    m_list.InsertColumn(COL_NAME, _T("Name"), LVCFMT_LEFT);

    m_list.InsertColumn(COL_TIME, _T("Time"), LVCFMT_RIGHT);

    ScaleFont();

    m_fakeImageList.Create(1, 16, ILC_COLOR4, 10, 10);
    m_list.SetImageList(&m_fakeImageList, LVSIL_SMALL);

    m_dropTarget.Register(this);

	createdWindow = true;

    return TRUE;
}

BOOL CPlayerPlaylistBar::PreCreateWindow(CREATESTRUCT& cs)
{
    if (!__super::PreCreateWindow(cs)) {
        return FALSE;
    }

    cs.dwExStyle |= WS_EX_ACCEPTFILES;

    return TRUE;
}

BOOL CPlayerPlaylistBar::PreTranslateMessage(MSG* pMsg)
{
    if (IsWindow(pMsg->hwnd) && IsVisible() && pMsg->message >= WM_KEYFIRST && pMsg->message <= WM_KEYLAST) {
        if (GetKeyState(VK_MENU) & 0x8000) {
            if (TranslateAccelerator(m_pMainFrame->GetSafeHwnd(), m_pMainFrame->GetDefaultAccelerator(), pMsg)) {
                return TRUE;
            }
        }
        if (IsDialogMessage(pMsg)) {
            return TRUE;
        }
    }

    return __super::PreTranslateMessage(pMsg);
}

void CPlayerPlaylistBar::ReloadTranslatableResources()
{
    SetWindowText(ResStr(IDS_PLAYLIST_CAPTION));
}

void CPlayerPlaylistBar::LoadState(CFrameWnd* pParent)
{
    CString section = _T("ToolBars\\") + m_strSettingName;

    if (AfxGetApp()->GetProfileInt(section, _T("Visible"), FALSE)) {
        SetAutohidden(true);
    }

    __super::LoadState(pParent);
}

void CPlayerPlaylistBar::SaveState()
{
    __super::SaveState();

    CString section = _T("ToolBars\\") + m_strSettingName;

    AfxGetApp()->WriteProfileInt(section, _T("Visible"),
                                 IsWindowVisible() || (AfxGetAppSettings().bHidePlaylistFullScreen && m_bHiddenDueToFullscreen) || IsAutohidden());
}

bool CPlayerPlaylistBar::IsHiddenDueToFullscreen() const
{
    return m_bHiddenDueToFullscreen;
}

void CPlayerPlaylistBar::SetHiddenDueToFullscreen(bool bHiddenDueToFullscreen, bool returningFromFullScreen /* = false */)
{
    if (bHiddenDueToFullscreen) {
        SetAutohidden(false);
    } else if (returningFromFullScreen) { //it was already hidden, but now we will flag it as autohidden vs. hiddenDueToFullscreen
        SetAutohidden(true);
    }
    m_bHiddenDueToFullscreen = bHiddenDueToFullscreen;
}

void CPlayerPlaylistBar::LoadDuration(POSITION pos) {
    if (AfxGetAppSettings().bUseMediainfoLoadFileDuration) {

        CPlaylistItem& pli = m_pl.GetAt(pos);

        MediaInfoDLL::MediaInfo mi;
        if (mi.IsReady()) {
            auto fn = pli.m_fns.GetHead();
            if (!PathUtils::IsURL(fn)) {
                auto fnString = fn.GetBuffer();
                size_t fp = mi.Open(fnString);
                fn.ReleaseBuffer();
                if (fp > 0) {
                    MediaInfoDLL::String info = mi.Get(MediaInfoDLL::Stream_General, 0, L"Duration");
                    if (!info.empty()) {
                        try {
                            int duration = std::stoi(info);
                            if (duration > 0) {
                                pli.m_duration = duration * 10000LL;
                                RefreshItem(pos);
                            }
                        } catch (...) {
                        }
                    }
                }
            }
            mi.Close();
        }
    }
}

void CPlayerPlaylistBar::AddItem(CString fn, bool insertAtCurrent /*= false*/)
{
    if (fn.IsEmpty()) {
        return;
    }

    CPlaylistItem pli;
    pli.m_fns.AddTail(fn);
    pli.AutoLoadFiles();

    POSITION pos;
    if (insertAtCurrent && m_insertingPos != nullptr) {
        pos = m_insertingPos = m_pl.InsertAfter(m_insertingPos, pli);
    } else {
        pos = m_pl.AddTail(pli);
    }
    LoadDuration(pos);
}

void CPlayerPlaylistBar::AddItem(CString fn, CAtlList<CString>* subs)
{
    CAtlList<CString> sl;
    sl.AddTail(fn);
    AddItem(sl, subs);
}

void CPlayerPlaylistBar::AddItem(CAtlList<CString>& fns, CAtlList<CString>* subs, CString label, CString ydl_src, CString ydl_ua, CString cue, CAtlList<CYoutubeDLInstance::YDLSubInfo>* ydl_subs)
{
    CPlaylistItem pli;

    POSITION pos = fns.GetHeadPosition();
    while (pos) {
        CString fn = fns.GetNext(pos);
        if (!fn.Trim().IsEmpty()) {
            pli.m_fns.AddTail(fn);
        }
    }

    if (pli.m_fns.IsEmpty()) {
        return;
    }

    if (subs) {
        POSITION posSub = subs->GetHeadPosition();
        while (posSub) {
            CString fn = subs->GetNext(posSub);
            if (!fn.Trim().IsEmpty()) {
                pli.m_subs.AddTail(fn);
            }
        }
    }

    pli.AutoLoadFiles();
    if (label) {
        pli.m_label = label;
    } else {
        ASSERT(false);
    }
    if (!ydl_src.IsEmpty()) {
        pli.m_ydlSourceURL = ydl_src;
        pli.m_useragent = ydl_ua;
        pli.m_bYoutubeDL = true;
    }

    if (!cue.IsEmpty()) {
        pli.m_cue = true;
        pli.m_cue_filename = cue;
    }

    if (ydl_subs) {
        pli.m_ydl_subs.AddTailList(ydl_subs);
    }

    POSITION ipos = m_pl.AddTail(pli);

    if (m_pl.GetCount() > 1) {
        LoadDuration(ipos);
    }
}

void CPlayerPlaylistBar::ReplaceCurrentItem(CAtlList<CString>& fns, CAtlList<CString>* subs, CString label, CString ydl_src, CString ydl_ua, CString cue, CAtlList<CYoutubeDLInstance::YDLSubInfo>* ydl_subs)
{
    CAutoLock pledit(&m_plEditLock);

    CPlaylistItem* pli = GetCur();
    if (pli == nullptr) {
        AddItem(fns, subs, label, ydl_src, ydl_ua, cue);
    } else {
        pli->m_fns.RemoveAll();
        pli->m_fns.AddTailList(&fns);
        pli->m_subs.RemoveAll();
        if (subs) {
            pli->m_subs.AddTailList(subs);
        }
        pli->m_label = label;
        pli->m_ydlSourceURL = ydl_src;
        pli->m_useragent = ydl_ua;
        pli->m_bYoutubeDL = !ydl_src.IsEmpty();
        pli->m_cue = !cue.IsEmpty();
        pli->m_cue_filename = cue;
        pli->m_ydl_subs.RemoveAll();
        if (ydl_subs) {
            pli->m_ydl_subs.AddTailList(ydl_subs);
        }

        Refresh();
        SavePlaylist();
    }
}

void CPlayerPlaylistBar::AddSubtitleToCurrent(CString fn)
{
    CPlaylistItem* pli = GetCur();
    if (pli != nullptr) {
        if (!pli->m_subs.Find(fn)) {
            pli->m_subs.AddHead(fn);
        }
    }
}

void CPlayerPlaylistBar::ParsePlayList(CString fn, CAtlList<CString>* subs, int redir_count)
{
    TRACE(fn + _T("\n"));
    CAtlList<CString> sl;
    sl.AddTail(fn);
    ParsePlayList(sl, subs, redir_count);
}

void CPlayerPlaylistBar::ResolveLinkFiles(CAtlList<CString>& fns)
{
    // resolve .lnk files

    POSITION pos = fns.GetHeadPosition();
    while (pos) {
        CString& fn = fns.GetNext(pos);

        if (PathUtils::IsLinkFile(fn)) {
            CString fnResolved = PathUtils::ResolveLinkFile(fn);
            if (!fnResolved.IsEmpty()) {
                fn = fnResolved;
            }
        }
    }
}

bool CPlayerPlaylistBar::AddItemNoDuplicate(CString fn, bool insertAtCurrent /*= false*/)
{
    POSITION pos = m_pl.GetHeadPosition();
    CString fnLower = CString(fn).MakeLower();
    while (pos) {
        const CPlaylistItem& pli = m_pl.GetNext(pos);
        POSITION subpos = pli.m_fns.GetHeadPosition();
        while (subpos) {
            CString cur = pli.m_fns.GetNext(subpos);
            if (cur.MakeLower() == fnLower) {
                // duplicate
                return false;
            }
        }
    }

    AddItem(fn, insertAtCurrent);
    return true;
}

bool CPlayerPlaylistBar::AddFromFilemask(CString mask, bool recurse_dirs, bool insertAtCurrent /*= false*/)
{
    ASSERT(ContainsWildcard(mask));

    bool added = false;

    std::set<CString, CStringUtils::LogicalLess> filelist;
    if (m_pMainFrame->WildcardFileSearch(mask, filelist, recurse_dirs)) {
        auto it = filelist.begin();
        while (it != filelist.end()) {
            if (AddItemNoDuplicate(*it, insertAtCurrent)) {
                added = true;
            }
            it++;
        }
    }

    return added;
}

bool CPlayerPlaylistBar::AddItemsInFolder(CString pathname, bool insertAtCurrent /*= false*/)
{
    CString mask = CString(pathname).TrimRight(_T("\\/")) + _T("\\*.*");
    return AddFromFilemask(mask, false, insertAtCurrent);
}

void CPlayerPlaylistBar::ExternalPlayListLoaded(CStringW fn) {
    auto& s = AfxGetAppSettings();
    if (!PathUtils::IsURL(fn)) {
        s.externalPlayListPath = fn;
        m_ExternalPlayListFNCopy = m_pl.GetIDs();
    } else {
        s.externalPlayListPath = L"";
        m_ExternalPlayListFNCopy.clear();
    }
}

//convenience function to clear the saved playlist if it has been invalidated
void CPlayerPlaylistBar::ClearExternalPlaylistIfInvalid() {
    CStringW discard;
    IsExternalPlayListActive(discard);
}

bool CPlayerPlaylistBar::IsExternalPlayListActive(CStringW& playlistPath) {
    auto& s = AfxGetAppSettings();
    if (!s.externalPlayListPath.IsEmpty() && m_pl.GetIDs() == m_ExternalPlayListFNCopy) {
        playlistPath = s.externalPlayListPath;
        return true;
    } else {
        s.externalPlayListPath = L"";
        m_ExternalPlayListFNCopy.clear();
        return false;
    }
}

void CPlayerPlaylistBar::ParsePlayList(CAtlList<CString>& fns, CAtlList<CString>* subs, int redir_count, CString label, CString ydl_src, CString ydl_ua, CString cue, CAtlList<CYoutubeDLInstance::YDLSubInfo>* ydl_subs)
{
    if (fns.IsEmpty()) {
        return;
    }
    if (redir_count >= 5) {
        ASSERT(FALSE);
        return;
    }

    if (redir_count == 0) {
        ResolveLinkFiles(fns);

        // single entry can be a directory or file mask -> search for files
        // multiple filenames means video file plus audio dub
        if (fns.GetCount() == 1) {
            CString fname = fns.GetHead();
            if (!PathUtils::IsURL(fname)) {
                if (ContainsWildcard(fname)) {
                    AddFromFilemask(fname, true);
                    return;
                } else if (PathUtils::IsDir(fname)) {
                    AddItemsInFolder(fname);
                    return;
                }
            }
        }
    }

    CAtlList<CString> redir;
    CString ct = GetContentType(fns.GetHead(), &redir);
    if (!redir.IsEmpty()) {
        POSITION pos = redir.GetHeadPosition();
        while (pos) {
            ParsePlayList(redir.GetNext(pos), subs, redir_count + 1);
        }
        return;
    }

    if (ct == _T("application/x-mpc-playlist")) {
        auto fn = fns.GetHead();
        if (ParseMPCPlayList(fn)) {
            ExternalPlayListLoaded(fn);
        }

        return;
    } else if (ct == _T("application/x-cue-sheet")) {
        ParseCUESheet(fns.GetHead());
        return;
    } else if (ydl_src.IsEmpty() && (ct == _T("audio/x-mpegurl") || ct == _T("audio/mpegurl"))) {
        auto fn = fns.GetHead();
        if (!PathUtils::IsURL(fn) || fn.Find(L"/hls/") == -1) {
            bool lav_fallback = false;
            if (ParseM3UPlayList(fn, &lav_fallback)) {
                ExternalPlayListLoaded(fn);
                return;
            } else if (!lav_fallback) {
                /* invalid file or Internet connection failed */
                return;
            }
        }
        /* fall through to AddItem below, we do this for HLS playlist, since those should be handled directly by LAV Splitter */
    } else {
#if INTERNAL_SOURCEFILTER_MPEG
        const CAppSettings& s = AfxGetAppSettings();
        if (ct == _T("application/x-bdmv-playlist") && (s.SrcFilters[SRC_MPEG] || s.SrcFilters[SRC_MPEGTS])) {
            ParseBDMVPlayList(fns.GetHead());
            return;
        }
#endif
    }
    if (ydl_src.IsEmpty() && ct == _T("ytdl")) {
        ydl_src = fns.GetHead();
    }

    AddItem(fns, subs, label, ydl_src, ydl_ua, cue, ydl_subs);
}

static CString CombinePath(CString base, CString fn, bool base_is_url)
{
    if (base_is_url) {
        if (PathUtils::IsURL(fn)) {
            return fn;
        }
    }
    else {
        if (PathUtils::IsFullFilePath(fn)) {
            return fn;
        } else if (PathUtils::IsURL(fn)) {
            return fn;
        }
    }
    return base + fn;
}

static CString CombinePath(CPath p, CString fn)
{
    if (PathUtils::IsFullFilePath(fn)) {
        return fn;
    }
    p.Append(CPath(fn));
    return (LPCTSTR)p;
}

bool CPlayerPlaylistBar::ParseBDMVPlayList(CString fn)
{
    CHdmvClipInfo ClipInfo;
    CString strPlaylistFile;
    CHdmvClipInfo::HdmvPlaylist MainPlaylist;

    CPath Path(fn);
    Path.RemoveFileSpec();
    Path.RemoveFileSpec();


    if (SUCCEEDED(ClipInfo.FindMainMovie(Path + L"\\", strPlaylistFile, MainPlaylist, m_pMainFrame->m_MPLSPlaylist))) {
        CAtlList<CString> strFiles;
        strFiles.AddHead(strPlaylistFile);
        Append(strFiles, MainPlaylist.size() > 1, nullptr);
    }

    return !m_pl.IsEmpty();
}

bool CPlayerPlaylistBar::ParseCUESheet(CString cuefn) {
    CString str;
    std::vector<int> idx;
    int cue_index(0);

    CWebTextFile f(CTextFile::UTF8);
    f.SetFallbackEncoding(CTextFile::ANSI);
    if (!f.Open(cuefn) || !f.ReadString(str)) {
        return false;
    }
    f.Seek(0, CFile::SeekPosition::begin);

    CString base;
    bool isurl = PathUtils::IsURL(cuefn);
    if (isurl) {
        int p = cuefn.Find(_T('?'));
        if (p > 0) {
            cuefn = cuefn.Left(p);
        }
        p = cuefn.ReverseFind(_T('/'));
        if (p > 0) {
            base = cuefn.Left(p + 1);
        }
    }
    else {
        CPath basefilepath(cuefn);
        basefilepath.RemoveFileSpec();
        basefilepath.AddBackslash();
        base = basefilepath.m_strPath;
    }

    bool success = false;
    CString title(_T(""));
    CString performer(_T(""));
    CAtlList<CueTrackMeta> trackl;
    CAtlList<CPlaylistItem> pl;
    CueTrackMeta track;
    int trackID(0);
    CString lastfile;

    while(f.ReadString(str)) {
        str.Trim();
        if (cue_index == 0 && str.Left(5) == _T("TITLE")) {
            title = str.Mid(6).Trim(_T("\""));
        }
        else if (cue_index == 0 && str.Left(9) == _T("PERFORMER")) {
            performer = str.Mid(10).Trim(_T("\""));
        }
        else if (str.Left(4) == _T("FILE")) {
            if (str.Right(4) == _T("WAVE") || str.Right(3) == _T("MP3") || str.Right(4) == _T("FLAC") || str.Right(4) == _T("AIFF")) {
                CString file_entry;
                if (str.Right(3) == _T("MP3")) {
                    file_entry = str.Mid(5, str.GetLength() - 9).Trim(_T("\""));
                } else {
                    file_entry = str.Mid(5, str.GetLength() - 10).Trim(_T("\""));
                }
                if (file_entry != lastfile) {
                    CPlaylistItem pli;
                    lastfile = file_entry;
                    file_entry = CombinePath(base, file_entry, isurl);
                    pli.m_cue = true;
                    pli.m_cue_index = cue_index;
                    pli.m_cue_filename = cuefn;
                    pli.m_fns.AddTail(file_entry);
                    pl.AddTail(pli);
                    cue_index++;
                }
            }
        }
        else if (cue_index > 0) {
            if (str.Left(5) == _T("TRACK") && str.Right(5) == _T("AUDIO")) {
                CT2CA tmp(str.Mid(6, str.GetLength() - 12));
                const char* tmp2(tmp);
                sscanf_s(tmp2, "%d", &trackID);
                if (track.trackID != 0) {
                    trackl.AddTail(track);
                    track = CueTrackMeta();
                }
                track.trackID = trackID;
                track.fileID = cue_index - 1;
            }
            else if (str.Left(5) == _T("TITLE")) {
                track.title = str.Mid(6).Trim(_T("\""));
            }
            else if (str.Left(9) == _T("PERFORMER")) {
                track.performer = str.Mid(10).Trim(_T("\""));
            }
        }
    }

    if (track.trackID != 0) {
        trackl.AddTail(track);
    }

    CPath cp(cuefn);
    CString fn_no_ext;
    CString fdir;
    if (cp.FileExists()) {
        cp.RemoveExtension();
        fn_no_ext = cp.m_strPath;
        cp.RemoveFileSpec();
        fdir = cp.m_strPath;
    }
    bool currentCoverIsFileArt(false);
    CString cover(CoverArt::FindExternal(fn_no_ext, fdir, _T(""), currentCoverIsFileArt));

    int fileid = 0;
    int filecount = pl.GetCount();
    POSITION p = pl.GetHeadPosition();
    while (p) {
        CPlaylistItem pli = pl.GetNext(p);
        if (performer.IsEmpty()) {
            if (!title.IsEmpty()) {
                pli.m_label = title;
                if (filecount > 1) {
                    pli.m_label.AppendFormat(L" [%d/%d]", ++fileid, filecount);
                }
            }
        } else {
            if (title.IsEmpty()) {
                pli.m_label = performer;
            } else {
                pli.m_label = title + _T(" - ") + performer;
            }
            if (filecount > 1) {
                pli.m_label.AppendFormat(L" [%d/%d]", ++fileid, filecount);
            }
        }
        if (!cover.IsEmpty()) pli.m_cover = cover;
        m_pl.AddTail(pli);
        success = true;
    }
    
    return success;
}

bool CPlayerPlaylistBar::ParseM3UPlayList(CString fn, bool* lav_fallback) {
    CString str;
    CPlaylistItem pli;
    std::vector<int> idx;

    bool isurl = PathUtils::IsURL(fn);

    CWebTextFile f(CTextFile::UTF8);
    f.SetFallbackEncoding(CTextFile::ANSI);

    DWORD dwError = 0;
    if (!f.Open(fn, dwError)) {
        if (isurl && (dwError == 12029)) {
            // connection failed, can sometimes happen (due to user-agent?), so try fallback
            *lav_fallback = true;
        }
        return false;
    }
    if (!f.ReadString(str)) {
        return false;
    }

    bool isExt = false;
    if (str.Left(7) == _T("#EXTM3U")) {
        isExt = true;
    } else {
        f.Seek(0, CFile::SeekPosition::begin);
    }

    CString base;
    if (isurl) {
        int p = fn.Find(_T('?'));
        if (p > 0) {
            fn = fn.Left(p);
        }
        p = fn.ReverseFind(_T('/'));
        if (p > 0) {
            base = fn.Left(p + 1);
        }
    }
    else {
        CPath basefilepath(fn);
        basefilepath.RemoveFileSpec();
        basefilepath.AddBackslash();
        base = basefilepath.m_strPath;
    }

    bool success = false;
    CString extinf_title = L"";

    while (f.ReadString(str)) {
        str = str.Trim();
        if (str.IsEmpty()) { continue; }

        if (str.Find(_T("#")) == 0) {
            if (isExt && str.Find(_T("#EXTINF")) == 0) {
                CAtlList<CString> sl;
                Explode(str, sl, ':', 2);
                if (sl.GetCount() == 2) {
                    CString key = sl.RemoveHead();
                    CString value = sl.RemoveHead();
                    if (key == _T("#EXTINF")) {
                        int findDelim;
                        if (-1 != (findDelim = value.Find(_T(",")))) {
                            extinf_title = value.Mid(findDelim + 1);
                        }
                    }
                }
            } else if (str.Find(_T("#EXT-X-STREAM-INF")) == 0 || str.Find(_T("#EXT-X-TARGETDURATION")) == 0 || str.Find(_T("#EXT-X-MEDIA-SEQUENCE")) == 0) {
                // HTTP Live Stream, let LAV Splitter handle this
                *lav_fallback = true;
                return false;
            }
            continue;
        }

        // file or url
        str = CombinePath(base, str, isurl);
        if (!isurl && !PathUtils::IsURL(str) && ContainsWildcard(str)) {
            extinf_title = L"";
            // wildcard entry
            std::set<CString, CStringUtils::LogicalLess> filelist;
            if (m_pMainFrame->WildcardFileSearch(str, filelist, true)) {
                auto it = filelist.begin();
                while (it != filelist.end()) {
                    pli = CPlaylistItem();
                    pli.m_fns.AddTail(*it);
                    m_pl.AddTail(pli);
                    success = true;
                    it++;
                }
            }
        } else {
            // check for nested playlist
            CString ext = GetFileExt(str);
            if (ext == L".m3u" || ext == L".m3u8") {
                bool dummy = false;
                if (!PathUtils::IsURL(str) && ParseM3UPlayList(str, &dummy)) {
                    success = true;
                    extinf_title = L"";
                    continue;
                }
            } else if (ext == L".pls") {
                int count = m_pl.GetCount();
                ParsePlayList(str, nullptr, 1);
                if (m_pl.GetCount() > count) {
                    success = true;
                    extinf_title = L"";
                    continue;
                }
            }

            pli = CPlaylistItem();
            pli.m_fns.AddTail(str);
            if (PathUtils::IsURL(str) && CMainFrame::IsOnYDLWhitelist(str)) {
                pli.m_ydlSourceURL = str;
                pli.m_bYoutubeDL = true;
            }
            if (!extinf_title.IsEmpty()) {
                pli.m_label = extinf_title;
            }
            m_pl.AddTail(pli);
            success = true;
            extinf_title = L"";
        }
    }

    return success;
}

bool CPlayerPlaylistBar::ParseMPCPlayList(CString fn)
{
    CString str;
    CAtlMap<int, CPlaylistItem> pli;
    std::vector<int> idx;

    CWebTextFile f(CTextFile::UTF8);
    f.SetFallbackEncoding(CTextFile::ANSI);
    if (!f.Open(fn) || !f.ReadString(str) || str != _T("MPCPLAYLIST")) {
        return false;
    }

    CPath base(fn);
    base.RemoveFileSpec();

    while (f.ReadString(str)) {
        CAtlList<CString> sl;
        Explode(str, sl, ',', 3);
        if (sl.GetCount() != 3) {
            continue;
        }

        if (int i = _ttoi(sl.RemoveHead())) {
            CString key = sl.RemoveHead();
            CString value = sl.RemoveHead();

            if (key == _T("type")) {
                pli[i].m_type = (CPlaylistItem::type_t)_ttol(value);
                idx.push_back(i);
            } else if (key == _T("label")) {
                pli[i].m_label = value;
            } else if (key == _T("filename")) {
                if (!PathUtils::IsURL(value)) value = CombinePath(base, value);
                pli[i].m_fns.AddTail(value);
            } else if (key == _T("subtitle")) {
                if (!PathUtils::IsURL(value)) value = CombinePath(base, value);
                pli[i].m_subs.AddTail(value);
            } else if (key == _T("ydlSourceURL")) {
                pli[i].m_ydlSourceURL = value;
                pli[i].m_bYoutubeDL = true;
            } else if (key == _T("duration")) {
                long long dur = _ttoll(value) * 10000LL;
                if (dur > 0) {
                    pli[i].m_duration = dur;
                }
            } else if (key == _T("video")) {
                while (pli[i].m_fns.GetCount() < 2) {
                    pli[i].m_fns.AddTail(_T(""));
                }
                pli[i].m_fns.GetHead() = value;
            } else if (key == _T("audio")) {
                while (pli[i].m_fns.GetCount() < 2) {
                    pli[i].m_fns.AddTail(_T(""));
                }
                pli[i].m_fns.GetTail() = value;
            } else if (key == _T("vinput")) {
                pli[i].m_vinput = _ttol(value);
            } else if (key == _T("vchannel")) {
                pli[i].m_vchannel = _ttol(value);
            } else if (key == _T("ainput")) {
                pli[i].m_ainput = _ttol(value);
            } else if (key == _T("country")) {
                pli[i].m_country = _ttol(value);
            } else if (key == _T("cue_filename")) {
                pli[i].m_cue = true;
                pli[i].m_cue_filename = value;
            } else if (key == _T("cue_index")) {
                pli[i].m_cue_index = _ttoi(value);
            } else if (key == _T("cover")) {
                pli[i].m_cover = value;
            } else if (key == _T("ydl_sub") || key == _T("ydl_auto_sub")) {
                CAtlList<CString> li;
                Explode(value, li, ',', 3);
                if (li.GetCount() != 3) continue;
                CYoutubeDLInstance::YDLSubInfo s;
                if (key == _T("ydl_auto_sub")) s.isAutomaticCaptions = true;
                s.lang = li.RemoveHead();
                s.ext = li.RemoveHead();
                s.url = li.RemoveHead();
                if (!s.lang.IsEmpty() && !s.url.IsEmpty()) pli[i].m_ydl_subs.AddTail(s);
            }
        }
    }

    std::sort(idx.begin(), idx.end());
    for (int i : idx) {
        if (pli[i].m_fns.GetCount() > 0) m_pl.AddTail(pli[i]);
    }
    return !pli.IsEmpty();
}

bool CPlayerPlaylistBar::PlaylistCanStripPath(CString path)
{
    CPath p(path);
    p.RemoveFileSpec();
    CString base = p.m_strPath + L"\\";
    int baselen = base.GetLength();

    POSITION pos = m_pl.GetHeadPosition();
    while (pos) {
        CPlaylistItem& pli = m_pl.GetNext(pos);

        if (pli.m_type != CPlaylistItem::file) {
            return false;
        } else {
            POSITION pos2 = pli.m_fns.GetHeadPosition();
            while (pos2) {
                CString fn = pli.m_fns.GetNext(pos2);
                CString filebase = fn.Left(baselen);
                if (base != filebase) {
                    return false;
                }
            }

            pos2 = pli.m_subs.GetHeadPosition();
            while (pos2) {
                CString fn = pli.m_subs.GetNext(pos2);
                CString filebase = fn.Left(baselen);
                if (base != filebase) {
                    return false;
                }
            }

            if (pli.m_cue) {
                CString filebase = pli.m_cue_filename.Left(baselen);
                if (base != filebase) {
                    return false;
                }
            }

            if (!pli.m_cover.IsEmpty()) {
                CString filebase = pli.m_cover.Left(baselen);
                if (base != filebase) {
                    return false;
                }
            }
        }
    }

    return true;
}

bool CPlayerPlaylistBar::SaveMPCPlayList(CString fn, CTextFile::enc e)
{
    CAppSettings& s = AfxGetAppSettings();
    CTextFile f;
    if (!f.Save(fn, e)) {
        return false;
    }

    bool bRemovePath = PlaylistCanStripPath(fn);

    CPath pl_path(fn);
    pl_path.RemoveFileSpec();
    CString pl_path_str = pl_path.m_strPath + L"\\";
    int pl_path_len = pl_path_str.GetLength();

    f.WriteString(_T("MPCPLAYLIST\n"));

    POSITION pos = m_pl.GetHeadPosition(), pos2;
    for (int i = 1; pos; i++) {
        CPlaylistItem& pli = m_pl.GetNext(pos);

        CString idx;
        idx.Format(_T("%d"), i);

        CString str;
        str.Format(_T("%d,type,%d"), i, pli.m_type);
        f.WriteString(str + _T("\n"));

        if (pli.m_label && !pli.m_label.IsEmpty()) {
            f.WriteString(idx + _T(",label,") + pli.m_label + _T("\n"));
        }

        if (pli.m_type == CPlaylistItem::file) {
            pos2 = pli.m_fns.GetHeadPosition();
            while (pos2) {
                CString fn2 = pli.m_fns.GetNext(pos2);
                if (bRemovePath && (pl_path_str == fn2.Left(pl_path_len))) {
                    fn2 = fn2.Mid(pl_path_len);
                }
                f.WriteString(idx + _T(",filename,") + fn2 + _T("\n"));
            }

            pos2 = pli.m_subs.GetHeadPosition();
            while (pos2) {
                CString fn2 = pli.m_subs.GetNext(pos2);
                if (bRemovePath && (pl_path_str == fn2.Left(pl_path_len))) {
                    fn2 = fn2.Mid(pl_path_len);
                }
                f.WriteString(idx + _T(",subtitle,") + fn2 + _T("\n"));
            }
            if (pli.m_bYoutubeDL && !pli.m_ydlSourceURL.IsEmpty()) {
                f.WriteString(idx + _T(",ydlSourceURL,") + pli.m_ydlSourceURL + _T("\n"));
            }
            if (pli.m_cue) {
                CString cue = pli.m_cue_filename;
                if (bRemovePath && (pl_path_str == cue.Left(pl_path_len))) {
                    cue = cue.Mid(pl_path_len);
                }
                f.WriteString(idx + _T(",cue_filename,") + cue + _T("\n"));
                if (pli.m_cue_index > 0) {
                    str.Format(_T("%d,cue_index,%d"), i, pli.m_cue_index);
                    f.WriteString(str + _T("\n"));
                }
            }
            if (!pli.m_cover.IsEmpty()) {
                CString cover = pli.m_cover;
                if (bRemovePath && (pl_path_str == cover.Left(pl_path_len))) {
                    cover = cover.Mid(pl_path_len);
                }
                f.WriteString(idx + _T(",cover,") + cover + _T("\n"));
            }
            if (pli.m_ydl_subs.GetCount() > 0) {
                POSITION pos3 = pli.m_ydl_subs.GetHeadPosition();
                while (pos3) {
                    CYoutubeDLInstance::YDLSubInfo subinfo = pli.m_ydl_subs.GetNext(pos3);
                    if (!subinfo.data.IsEmpty()) continue;
                    CString t;
                    CString t2 = subinfo.isAutomaticCaptions ? _T("ydl_auto_sub") : _T("ydl_sub");
                    t.Format(_T("%d,%s,%s,%s,%s\n"), i, static_cast<LPCWSTR>(t2), static_cast<LPCWSTR>(subinfo.lang), static_cast<LPCWSTR>(subinfo.ext), static_cast<LPCWSTR>(subinfo.url));
                    f.WriteString(t);
                }
            }
            if (pli.m_duration > 0) {
                CString dur;
                dur.Format(_T("%lld"), pli.m_duration / 10000LL);
                f.WriteString(idx + _T(",duration,") + dur + _T("\n"));
            }
        } else if (pli.m_type == CPlaylistItem::device && pli.m_fns.GetCount() == 2) {
            f.WriteString(idx + _T(",video,") + pli.m_fns.GetHead() + _T("\n"));
            f.WriteString(idx + _T(",audio,") + pli.m_fns.GetTail() + _T("\n"));
            str.Format(_T("%d,vinput,%d"), i, pli.m_vinput);
            f.WriteString(str + _T("\n"));
            str.Format(_T("%d,vchannel,%d"), i, pli.m_vchannel);
            f.WriteString(str + _T("\n"));
            str.Format(_T("%d,ainput,%d"), i, pli.m_ainput);
            f.WriteString(str + _T("\n"));
            str.Format(_T("%d,country,%ld"), i, pli.m_country);
            f.WriteString(str + _T("\n"));
        }
    }

    if (s.fKeepHistory && s.bRememberExternalPlaylistPos) {
        s.SavePlayListPosition(fn, GetSelIdx());
    }
    return true;
}

void CPlayerPlaylistBar::Refresh()
{
    SetupList();
    ResizeListColumn();
}

void CPlayerPlaylistBar::RefreshItem(POSITION pos)
{
    int idx = FindItem(pos);
    if (idx >= 0) {
        m_list.RedrawItems(idx, idx);
    }
}

void CPlayerPlaylistBar::PlayListChanged() {
    AfxGetAppSettings().externalPlayListPath = L"";
}

bool CPlayerPlaylistBar::Empty()
{
    CAutoLock pledit(&m_plEditLock);

    bool bWasPlaying = m_pl.RemoveAll();
    m_list.SetItemCountEx(0);
    m_posToIndex.clear();
    m_indexToPos.clear();
    m_SaveDelayed = true;
    AfxGetAppSettings().externalPlayListPath = L"";

    return bWasPlaying;
}

void CPlayerPlaylistBar::Open(CAtlList<CString>& fns, bool fMulti, CAtlList<CString>* subs, CString label, CString ydl_src, CString ydl_ua, CString cue)
{
    Empty();
    Append(fns, fMulti, subs, label, ydl_src, ydl_ua, cue);

    CString ext = CPath(fns.GetHead()).GetExtension().MakeLower();
    if (!fMulti && (ext == _T(".mpcpl"))) {
        m_playListPath = fns.GetHead();
    }
    else {
        m_playListPath.Empty();
    }
}

void CPlayerPlaylistBar::Append(CAtlList<CString>& fns, bool fMulti, CAtlList<CString>* subs, CString label, CString ydl_src, CString ydl_ua, CString cue, CAtlList<CYoutubeDLInstance::YDLSubInfo>* ydl_subs)
{
    POSITION posFirstAdded = m_pl.GetTailPosition();
    int activateListItemIndex = (int)m_pl.GetCount();

    if (fMulti) {
        ASSERT(subs == nullptr || subs->IsEmpty());
        POSITION pos = fns.GetHeadPosition();
        while (pos) {
            ParsePlayList(fns.GetNext(pos), nullptr);
        }
    } else {
        ParsePlayList(fns, subs, 0, label, ydl_src, ydl_ua, cue, ydl_subs);
    }

    Refresh();
    SavePlaylist(activateListItemIndex != 0);

    // Get the POSITION of the first item we just added
    if (posFirstAdded) {
        m_pl.GetNext(posFirstAdded);
    } else { // if the playlist was originally empty
        CAppSettings& s = AfxGetAppSettings();
        posFirstAdded = m_pl.GetHeadPosition();
        if (s.fKeepHistory && s.bRememberExternalPlaylistPos && fns.GetCount() == 1 && fns.GetHead() == s.externalPlayListPath) {
            UINT idx = s.GetSavedPlayListPosition(s.externalPlayListPath);
            if (idx != 0) {
                posFirstAdded = FindPos(idx);
                m_pl.SetPos(posFirstAdded);
            }
        }
    }
    if (posFirstAdded) {
        EnsureVisible(m_pl.GetTailPosition()); // This ensures that we maximize the number of newly added items shown
        EnsureVisible(posFirstAdded);
        if (activateListItemIndex) { // Select the first added item only if some were already present
            m_list.SetItemState(activateListItemIndex, LVIS_SELECTED | LVIS_FOCUSED, LVIS_SELECTED | LVIS_FOCUSED);
            m_list.SetSelectionMark(activateListItemIndex);
        }
    }
}

void CPlayerPlaylistBar::Open(CStringW vdn, CStringW adn, int vinput, int vchannel, int ainput)
{
    Empty();
    Append(vdn, adn, vinput, vchannel, ainput);
}

void CPlayerPlaylistBar::OpenDVD(CString fn)
{
    Empty();
    m_playListPath.Empty();

    CString fnifo;
    if (fn.Find(L".ifo") == -1) {
        if (CPath(fn).IsDirectory()) {
            fn = ForceTrailingSlash(fn);
            fnifo = fn + L"VIDEO_TS.IFO";
            if (!CPath(fnifo).FileExists()) {
                fnifo = fn + L"VIDEO_TS\\VIDEO_TS.IFO";
                if (!CPath(fnifo).FileExists()) {
                    fnifo = fn + L"AUDIO_TS.IFO";
                    if (!CPath(fnifo).FileExists()) {
                        fnifo = fn + L"AUDIO_TS\\AUDIO_TS.IFO";
                        if (!CPath(fnifo).FileExists()) {
                            return;
                        }
                    }
                }
            }
        } else {
            return;
        }
    } else {
        if (CPath(fn).FileExists()) {
            fnifo = fn;
        } else {
            return;
        }
    }

    CAtlList<CString> fns;
    fns.AddHead(fnifo);
    Append(fns, false);    
}

void CPlayerPlaylistBar::Append(CStringW vdn, CStringW adn, int vinput, int vchannel, int ainput)
{
    CPlaylistItem pli;
    pli.m_type = CPlaylistItem::device;
    pli.m_fns.AddTail(CString(vdn));
    pli.m_fns.AddTail(CString(adn));
    pli.m_vinput = vinput;
    pli.m_vchannel = vchannel;
    pli.m_ainput = ainput;
    CAtlList<CStringW> sl;
    CStringW vfn = GetFriendlyName(vdn);
    CStringW afn = GetFriendlyName(adn);
    if (!vfn.IsEmpty()) {
        sl.AddTail(vfn);
    }
    if (!afn.IsEmpty()) {
        sl.AddTail(afn);
    }
    CStringW label = Implode(sl, '|');
    label.Replace(L"|", L" - ");
    pli.m_label = CString(label);
    m_pl.AddTail(pli);

    Refresh();
    EnsureVisible(m_pl.GetTailPosition());
    m_list.SetItemState((int)m_pl.GetCount() - 1, LVIS_SELECTED | LVIS_FOCUSED, LVIS_SELECTED | LVIS_FOCUSED);
    SavePlaylist(true);
}

void CPlayerPlaylistBar::SetupList()
{
    RebuildPosMap();
    m_list.SetItemCountEx((int)m_pl.GetCount(), 0);
    m_list.Invalidate();
}

void CPlayerPlaylistBar::SyncSelectionToPos(POSITION pos)
{
    if (!pos) {
        return;
    }
    int idx = FindItem(pos);
    if (idx < 0) {
        return;
    }
    m_list.SetItemState(-1, 0, LVIS_SELECTED | LVIS_FOCUSED);
    m_list.SetItemState(idx, LVIS_SELECTED | LVIS_FOCUSED, LVIS_SELECTED | LVIS_FOCUSED);
    m_list.SetSelectionMark(idx);
    m_list.EnsureVisible(idx, TRUE);
}

void CPlayerPlaylistBar::UpdateList()
{
    // Virtual list mode: data is always fetched from m_pl on demand
    m_list.Invalidate();
}

void CPlayerPlaylistBar::EnsureVisible(POSITION pos)
{
    int i = FindItem(pos);
    if (i < 0) {
        return;
    }
    m_list.EnsureVisible(i, TRUE);
    m_list.Invalidate();
}

int CPlayerPlaylistBar::FindItem(const POSITION pos) const
{
    auto it = m_posToIndex.find(pos);
    if (it != m_posToIndex.end()) {
        return it->second;
    }
    return -1;
}

POSITION CPlayerPlaylistBar::FindPos(int i)
{
    if (i < 0 || i >= (int)m_indexToPos.size()) {
        return nullptr;
    }
    return m_indexToPos[i];
}

void CPlayerPlaylistBar::RebuildPosMap()
{
    m_posToIndex.clear();
    m_indexToPos.clear();
    m_indexToPos.reserve(m_pl.GetCount());
    POSITION pos = m_pl.GetHeadPosition();
    for (int i = 0; pos; i++) {
        m_posToIndex[pos] = i;
        m_indexToPos.push_back(pos);
        m_pl.GetNext(pos);
    }
}

void CPlayerPlaylistBar::InvalidatePlayingItem(POSITION oldPos, POSITION newPos)
{
    // Only repaint the old and new "now playing" items instead of updating all items
    if (oldPos) {
        RefreshItem(oldPos);
    }
    if (newPos && newPos != oldPos) {
        RefreshItem(newPos);
    }
    m_list.UpdateWindow();
}

INT_PTR CPlayerPlaylistBar::GetCount() const
{
    return m_pl.GetCount(); // TODO: n - .fInvalid
}

int CPlayerPlaylistBar::GetValidCount() const
{
    int validcount = 0;
    POSITION pos = m_pl.GetHeadPosition();
    while (pos) {
        if (!m_pl.GetAt(pos).m_fInvalid) {
            validcount++;
        }
        m_pl.GetNext(pos);
    }
    return validcount;
}

int CPlayerPlaylistBar::GetSelIdx() const
{
    return FindItem(m_pl.GetPos());
}

void CPlayerPlaylistBar::SetSelIdx(int i)
{
    m_pl.SetPos(FindPos(i));
}

bool CPlayerPlaylistBar::IsAtEnd()
{
    POSITION pos = m_pl.GetPos(), tail = m_pl.GetShuffleAwareTailPosition();
    bool isAtEnd = (pos && pos == tail);

    if (!isAtEnd && pos) {
        isAtEnd = m_pl.GetNextWrap(pos).m_fInvalid;
        while (isAtEnd && pos && pos != tail) {
            isAtEnd = m_pl.GetNextWrap(pos).m_fInvalid;
        }
    }

    return isAtEnd;
}

bool CPlayerPlaylistBar::GetCur(CPlaylistItem& pli, bool check_fns) const
{
    if (!m_pl.IsEmpty()) {
        POSITION p = m_pl.GetPos();
        if (p) {
            pli = m_pl.GetAt(p);
            return !check_fns || !pli.m_fns.IsEmpty();
        }
    }
    return false;
}

CPlaylistItem* CPlayerPlaylistBar::GetCur()
{
    if (!m_pl.IsEmpty()) {
        POSITION p = m_pl.GetPos();
        if (p) {
            CPlaylistItem* result = &m_pl.GetAt(p);
            return result;
        }
    }
    return nullptr;
}

CString CPlayerPlaylistBar::GetCurFileName(bool use_ydl_source)
{
    CString fn;
    CPlaylistItem* pli = GetCur();

    if (pli && pli->m_bYoutubeDL && use_ydl_source) {
        fn = pli->m_ydlSourceURL;
    } else if (pli && !pli->m_fns.IsEmpty()) {
        fn = pli->m_fns.GetHead();
    }
    return fn;
}

CString CPlayerPlaylistBar::GetCurFileNameTitle()
{
    CString fn;
    CPlaylistItem* pli = GetCur();

    if (pli && pli->m_bYoutubeDL) {
        fn = pli->m_label;
    } else if (pli && !pli->m_fns.IsEmpty()) {
        fn = pli->m_fns.GetHead();
    }
    return fn;
}

bool CPlayerPlaylistBar::SetNext()
{
    POSITION pos = m_pl.GetPos(), org = pos;
    while (m_pl.GetNextWrap(pos).m_fInvalid && pos != org) {
        ;
    }
    InvalidatePlayingItem(org, pos);
    m_pl.SetPos(pos);
    EnsureVisible(pos);

    return (pos != org);
}

bool CPlayerPlaylistBar::SetPrev()
{
    POSITION pos = m_pl.GetPos(), org = pos;
    while (m_pl.GetPrevWrap(pos).m_fInvalid && pos != org) {
        ;
    }
    InvalidatePlayingItem(org, pos);
    m_pl.SetPos(pos);
    EnsureVisible(pos);

    return (pos != org);
}

void CPlayerPlaylistBar::SetFirstSelected()
{
    POSITION oldPos = m_pl.GetPos();
    POSITION pos = m_list.GetFirstSelectedItemPosition();
    if (pos) {
        pos = FindPos(m_list.GetNextSelectedItem(pos));
    } else {
        pos = m_pl.GetShuffleAwareTailPosition();
        POSITION org = pos;
        while (m_pl.GetNextWrap(pos).m_fInvalid && pos != org) {
            ;
        }
        // Select the first item to be played when no item was previously selected
        m_list.SetItemState(FindItem(pos), LVIS_SELECTED | LVIS_FOCUSED, LVIS_SELECTED | LVIS_FOCUSED);
    }
    InvalidatePlayingItem(oldPos, pos);
    m_pl.SetPos(pos);
    EnsureVisible(pos);
}

void CPlayerPlaylistBar::SetFirst()
{
    POSITION pos = m_pl.GetTailPosition(), org = pos;
    if (!pos) {
        return;
    }
    while (m_pl.GetNextWrap(pos).m_fInvalid && pos != org) {
        ;
    }
    InvalidatePlayingItem(org, pos);
    m_pl.SetPos(pos);
    EnsureVisible(pos);
}

void CPlayerPlaylistBar::SetLast()
{
    POSITION pos = m_pl.GetHeadPosition(), org = pos;
    while (m_pl.GetPrevWrap(pos).m_fInvalid && pos != org) {
        ;
    }
    InvalidatePlayingItem(org, pos);
    m_pl.SetPos(pos);
    EnsureVisible(pos);
}

void CPlayerPlaylistBar::SetCurValid(bool fValid)
{
    POSITION pos = m_pl.GetPos();
    if (pos) {
        m_pl.GetAt(pos).m_fInvalid = !fValid;
        if (!fValid) {
            RefreshItem(pos);
        }
    }
}

void CPlayerPlaylistBar::SetCurLabel(CString label)
{
    POSITION pos = m_pl.GetPos();
    if (pos) {
        auto pi = m_pl.GetAt(pos);
        pi.m_label = label;
        m_pl.SetAt(pos, pi);
        RefreshItem(pos);
    }
}

void CPlayerPlaylistBar::SetCurTime(REFERENCE_TIME rt)
{
    POSITION pos = m_pl.GetPos();
    if (pos) {
        CPlaylistItem& pli = m_pl.GetAt(pos);
        pli.m_duration = rt;
        RefreshItem(pos);
    }
}

void CPlayerPlaylistBar::Randomize()
{
    POSITION selPos = FindPos(m_list.GetSelectionMark());
    m_pl.Randomize();
    SetupList();
    SyncSelectionToPos(selPos ? selPos : m_pl.GetPos());
    SavePlaylist();
}

void CPlayerPlaylistBar::UpdateLabel(CString in) {
    POSITION pos = m_pl.GetPos();
    if (pos) {
        CPlaylistItem& m = m_pl.GetAt(pos);
        m.m_label = in;
        m_pl.SetAt(m_pl.GetPos(), m);
        RefreshItem(pos);
    }
}

OpenMediaData* CPlayerPlaylistBar::GetCurOMD(REFERENCE_TIME rtStart, ABRepeat abRepeat /* = ABRepeat() */)
{
    CPlaylistItem* pli = GetCur();
    if (pli == nullptr) {
        return nullptr;
    }

    OpenMediaData* omd = nullptr;

    if (pli->m_type == CPlaylistItem::device) {
        if (OpenDeviceData* p = DEBUG_NEW OpenDeviceData()) {
            POSITION pos = pli->m_fns.GetHeadPosition();
            for (int i = 0; i < _countof(p->DisplayName) && pos; i++) {
                p->DisplayName[i] = pli->m_fns.GetNext(pos);
            }
            p->vinput = pli->m_vinput;
            p->vchannel = pli->m_vchannel;
            p->ainput = pli->m_ainput;
            omd = p;
        }
    } else {
        pli->AutoLoadFiles();
        CString fn = CString(pli->m_fns.GetHead()).MakeLower();

        if (fn.Find(_T("video_ts.ifo")) >= 0) {
            if (OpenDVDData* p = DEBUG_NEW OpenDVDData()) {
                p->path = pli->m_fns.GetHead();
                p->subs.AddTailList(&pli->m_subs);
                omd = p;
            }
        } else if (OpenFileData* p = DEBUG_NEW OpenFileData()) {
            p->fns.AddTailList(&pli->m_fns);
            p->subs.AddTailList(&pli->m_subs);
            p->rtStart = rtStart;
            p->bAddToRecent = true;
            p->abRepeat = abRepeat;
            p->useragent = pli->m_useragent;
            p->referrer = pli->m_ydlSourceURL;
            omd = p;
        }
    }

    if (m_SaveDelayed) {
        SavePlaylist();
    }

    return omd;
}

bool CPlayerPlaylistBar::SelectFileInPlaylist(LPCTSTR filename)
{
    if (!filename) {
        return false;
    }
    POSITION pos = m_pl.GetHeadPosition();
    while (pos) {
        CPlaylistItem& pli = m_pl.GetAt(pos);
        if (pli.m_bYoutubeDL) {
            if (pli.m_ydlSourceURL.CompareNoCase(filename) == 0) {
                m_pl.SetPos(pos);
                EnsureVisible(pos);
                return true;
            }
        } else {
            if (pli.m_fns.GetHead().CompareNoCase(filename) == 0) {
                m_pl.SetPos(pos);
                EnsureVisible(pos);
                return true;
            }
        }
        m_pl.GetNext(pos);
    }
    return false;
}

bool CPlayerPlaylistBar::DeleteFileInPlaylist(POSITION pos, bool recycle)
{
    CAutoLock pledit(&m_plEditLock);

    auto& s = AfxGetAppSettings();
    CString filename = m_pl.GetAt(pos).m_fns.GetHead();
    bool candeletefile = false;
    bool folderPlayNext = false;
    bool noconfirm = !s.bConfirmFileDelete;
    if (!PathUtils::IsURL(filename)) {
        if (!noconfirm && IsWindows10OrGreater()) {
            // show prompt, because Windows might not ask for confirmation
            CString msg;
            msg.Format(L"Move file to recycle bin?\n\n%s", filename.GetString());
            if (AfxMessageBox(msg, MB_ICONQUESTION | MB_YESNO, 0) == IDYES) {
                candeletefile = true;
                noconfirm = true;
            } else {
                return false;
            }
        } else {
            candeletefile = true;
        }
        folderPlayNext = (m_pl.GetCount() == 1 && (s.nCLSwitches & CLSW_PLAYNEXT || s.eAfterPlayback == CAppSettings::AfterPlayback::PLAY_NEXT));
    }
    
    bool isplaying = false;
    if (pos == m_pl.GetPos()) {
        isplaying = true;
    }

    // Get position of next file
    POSITION nextpos = pos;
    if (isplaying) {
        m_pl.GetNext(nextpos);
        if (nextpos == nullptr && m_pl.GetCount() > 1) {
            nextpos = m_pl.GetHeadPosition();
        }
    }

    // remove selected from playlist
    int listPos = FindItem(pos);
    if (listPos >= 0) {
        m_pl.RemoveAt(pos);
        RebuildPosMap();
        m_list.SetItemCountEx((int)m_pl.GetCount(), LVSICF_NOINVALIDATEALL);
        m_list.Invalidate();
        SavePlaylist();
    } else {
        ASSERT(false);
    }

    if (isplaying && !folderPlayNext && nextpos) {
        m_pl.SetPos(nextpos);
    }

    if (isplaying) {
        if (m_pMainFrame->IsStateLoaded()) {
            // close file to release the file handle
            m_pMainFrame->CloseMedia(nextpos != nullptr, candeletefile);
        }
        // if state is loading/closing/aborting, then file may remain in use, but try delete anyway
    }

    if (candeletefile) {
        FileDelete(filename, m_pMainFrame->m_hWnd, true, noconfirm);
    }

    // Continue with next file
    if (isplaying) {
        if (folderPlayNext) {
            m_pMainFrame->DoAfterPlaybackEvent();
        } else if (nextpos) {
            m_pMainFrame->PostMessage(WM_MPC_OPENCURPLAYLIST, 0, 0);
        }
    }

    return true;
}

void CPlayerPlaylistBar::LoadPlaylist(LPCTSTR filename)
{
    CString base;
    auto& s = AfxGetAppSettings();

    m_list.SetRedraw(FALSE);

    if (AfxGetMyApp()->GetAppSavePath(base)) {
        CPath p;
        p.Combine(base, _T("default.mpcpl"));

        if (p.FileExists()) {
            if (s.bRememberPlaylistItems) {
                ParseMPCPlayList(p);
                Refresh();
                //this code checks for a saved position in a playlist that was active when the player was last closed
                if (s.fKeepHistory && s.bRememberExternalPlaylistPos && !s.externalPlayListPath.IsEmpty()) { 
                    UINT idx = s.GetSavedPlayListPosition(s.externalPlayListPath);
                    if (idx >= 0 && idx < m_pl.GetCount()) {
                        ExternalPlayListLoaded(s.externalPlayListPath);
                        POSITION pos = FindPos(idx);
                        m_pl.SetPos(pos);
                        EnsureVisible(pos);
                    }
                } else {
                    SelectFileInPlaylist(filename);
                }
            } else {
                ::DeleteFile(p);
            }
        }
    }
    m_list.SetRedraw(TRUE);
    m_list.RedrawWindow(NULL, NULL, RDW_ERASE | RDW_INVALIDATE);
}

void CPlayerPlaylistBar::SavePlaylist(bool can_delay /* = false*/)
{
    CString base;

    if (AfxGetMyApp()->GetAppSavePath(base)) {
        CPath p;
        p.Combine(base, _T("default.mpcpl"));

        if (AfxGetAppSettings().bRememberPlaylistItems) {
            bool write = true;
            ULONGLONG tcnow = GetTickCount64();
            if (can_delay) {
                // avoid constantly writing playlist file when appending multiple files in short timeframe
                // which can happen when opening/appending selection from Explorer
                // playlist will also be saved when closing player
                if (m_tcLastSave && (tcnow - m_tcLastSave) < 2000) {
                    write = false;
                    m_SaveDelayed = true;
                }
            }
            if (write) {
                m_tcLastSave = tcnow;
                m_SaveDelayed = false;
                // Only create this folder when needed
                if (!PathUtils::Exists(base)) {
                    ::CreateDirectory(base, nullptr);
                }

                SaveMPCPlayList(p, CTextFile::UTF8);
            }
        } else if (p.FileExists()) {
            ::DeleteFile(p);
        }
    }
}

BEGIN_MESSAGE_MAP(CPlayerPlaylistBar, CMPCThemePlayerBar)
    ON_WM_DESTROY()
    ON_WM_SIZE()
    ON_NOTIFY(LVN_KEYDOWN, IDC_PLAYLIST, OnLvnKeyDown)
    ON_NOTIFY(NM_DBLCLK, IDC_PLAYLIST, OnNMDblclkList)
    //ON_NOTIFY(NM_CUSTOMDRAW, IDC_PLAYLIST, OnCustomdrawList)
    ON_WM_MEASUREITEM()
    ON_WM_DRAWITEM()
    ON_COMMAND_EX(ID_PLAY_PLAY, OnPlayPlay)
    ON_NOTIFY(LVN_BEGINDRAG, IDC_PLAYLIST, OnBeginDrag)
    ON_WM_MOUSEMOVE()
    ON_WM_LBUTTONUP()
    ON_NOTIFY_EX(TTN_NEEDTEXT, 0, OnToolTipNotify)
    ON_WM_TIMER()
    ON_WM_CONTEXTMENU()
    ON_NOTIFY(LVN_GETDISPINFO, IDC_PLAYLIST, OnLvnGetDispInfoList)
    ON_NOTIFY(LVN_BEGINLABELEDIT, IDC_PLAYLIST, OnLvnBeginlabeleditList)
    ON_NOTIFY(LVN_ENDLABELEDIT, IDC_PLAYLIST, OnLvnEndlabeleditList)
    ON_WM_XBUTTONDOWN()
    ON_WM_XBUTTONUP()
    ON_WM_XBUTTONDBLCLK()
    ON_WM_ERASEBKGND()
    ON_WM_VSCROLL()
END_MESSAGE_MAP()


void CPlayerPlaylistBar::ScaleFont()
{
    LOGFONT lf;
    m_pMainFrame->m_dpi.GetMessageFont(&lf);
    //lf.lfHeight = m_pMainFrame->m_dpi.ScaleSystemToOverrideY(lf.lfHeight);

    m_font.DeleteObject();
    if (m_font.CreateFontIndirect(&lf)) {
        m_list.SetFont(&m_font);
    }

    CDC* pDC = m_list.GetDC();
    CFont* old = pDC->SelectObject(m_list.GetFont());
    m_nTimeColWidth = pDC->GetTextExtent(_T("000:00:00")).cx + m_pMainFrame->m_dpi.ScaleX(5);
    pDC->SelectObject(old);
    m_list.ReleaseDC(pDC);
    m_list.SetColumnWidth(COL_TIME, m_nTimeColWidth);
}

void CPlayerPlaylistBar::EventCallback(MpcEvent ev)
{
    switch (ev) {
        case MpcEvent::DPI_CHANGED:
            InitializeSize();
            ScaleFont();
            ResizeListColumn();
            break;

        default:
            ASSERT(FALSE);
    }
}

// CPlayerPlaylistBar message handlers

void CPlayerPlaylistBar::ResizeListColumn()
{
    if (::IsWindow(m_list.m_hWnd)) {
        // Commit any active inline edit before resizing
        if (m_list.GetVirtualEditCtrl()) {
            m_list.SetFocus();
        }

        CRect r;
        GetClientRect(r);
        r.DeflateRect(2, 2);

        m_list.SetRedraw(FALSE);
        m_list.SetColumnWidth(COL_NAME, 0);
        m_list.SetRedraw(TRUE);

        m_list.MoveWindow(r, FALSE);
        m_list.GetClientRect(r);

        m_list.SetRedraw(FALSE);
        m_list.SetColumnWidth(COL_NAME, r.Width() - m_nTimeColWidth);
        m_list.SetRedraw(TRUE);

        Invalidate();
        m_list.RedrawWindow(nullptr, nullptr, RDW_FRAME | RDW_INVALIDATE);
    }
}

void CPlayerPlaylistBar::OnDestroy()
{
    m_dropTarget.Revoke();
    __super::OnDestroy();
}

void CPlayerPlaylistBar::OnSize(UINT nType, int cx, int cy)
{
    __super::OnSize(nType, cx, cy);

    ResizeListColumn();
}

void CPlayerPlaylistBar::OnLvnKeyDown(NMHDR* pNMHDR, LRESULT* pResult)
{
    LPNMLVKEYDOWN pLVKeyDown = reinterpret_cast<LPNMLVKEYDOWN>(pNMHDR);

    *pResult = FALSE;

    CAutoLock pledit(&m_plEditLock);

    int selected = (int)m_list.GetFirstSelectedItemPosition();
    if (!selected) {
        return;
    }
    selected--; // actual list index

    if (pLVKeyDown->wVKey == VK_DELETE) {
        if (m_pl.GetCount() > 1) {
            POSITION remplpos = FindPos(selected);
            POSITION curplpos = m_pl.GetPos();
            if (!remplpos) {
                ASSERT(FALSE);
                return;
            }
            if (remplpos == curplpos) {
                m_pMainFrame->SendMessage(WM_COMMAND, ID_FILE_CLOSEMEDIA);
            }
            m_pl.RemoveAt(remplpos);
            RebuildPosMap();
            m_list.SetItemCountEx((int)m_pl.GetCount(), LVSICF_NOINVALIDATEALL);
            m_list.Invalidate();

            if (m_list.GetItemCount() > 0) {
                int sel = (selected < m_list.GetItemCount()) ? selected : m_list.GetItemCount() - 1;
                m_list.SetItemState(sel, LVIS_SELECTED | LVIS_FOCUSED, LVIS_SELECTED | LVIS_FOCUSED);
                m_list.SetSelectionMark(sel);
            }
            ResizeListColumn();
        } else {
           if (Empty() && !m_pMainFrame->IsStateClosed()) {
                m_pMainFrame->PostMessage(WM_COMMAND, ID_FILE_CLOSEMEDIA);
            }
        }

        *pResult = TRUE;
    } else if (pLVKeyDown->wVKey == VK_SPACE) {
        m_pl.SetPos(FindPos(selected));
        m_list.Invalidate();
        m_pMainFrame->SetFocus();
        m_pMainFrame->PostMessage(WM_MPC_OPENCURPLAYLIST, 0, 0);

        *pResult = TRUE;
    }
}

void CPlayerPlaylistBar::OnNMDblclkList(NMHDR* pNMHDR, LRESULT* pResult)
{
    LPNMLISTVIEW lpnmlv = (LPNMLISTVIEW)pNMHDR;

    if (lpnmlv->iItem >= 0 && lpnmlv->iSubItem >= 0) {
        POSITION pos = FindPos(lpnmlv->iItem);
        if (m_pMainFrame->GetPlaybackMode() == PM_FILE && pos == m_pl.GetPos()) {
            // If the file is already playing, reset position
            CAppSettings& s = AfxGetAppSettings();
            s.MRU.UpdateCurrentFilePosition(0LL, true);
        } else {
            m_pl.SetPos(pos);
        }
        m_list.Invalidate();
        m_pMainFrame->PostMessage(WM_MPC_OPENCURPLAYLIST, 0, 0);
    }

    m_pMainFrame->SetFocus();

    *pResult = 0;
}

void CPlayerPlaylistBar::OnMeasureItem(int nIDCtl, LPMEASUREITEMSTRUCT lpMeasureItemStruct)
{
    __super::OnMeasureItem(nIDCtl, lpMeasureItemStruct);
    lpMeasureItemStruct->itemHeight = m_pMainFrame->m_dpi.CalculateListCtrlItemHeight((CListCtrl*)&m_list);
#if 0
    if (createdWindow) {
        //after creation, measureitem is called once for every window resize.  we will cache the default height before DPI scaling
        if (m_itemHeight == 0) {
            m_itemHeight = lpMeasureItemStruct->itemHeight;
        }
        lpMeasureItemStruct->itemHeight = m_pMainFrame->m_dpi.ScaleArbitraryToOverrideY(m_itemHeight, m_initialWindowDPI); //we must scale by initial DPI, NOT current DPI
    } else {
        //before creation, we must return a valid DPI scaled value, to prevent visual glitches when icon height has been tweaked.
        //we cannot cache this value as it may be different from that calculated after font has been set
        lpMeasureItemStruct->itemHeight = m_pMainFrame->m_dpi.ScaleSystemToOverrideY(lpMeasureItemStruct->itemHeight);
        m_initialWindowDPI = m_pMainFrame->m_dpi.DPIY(); //the initial DPI is always cached by ListCtrl and is never updated for future calculations of OnMeasureItem
    }
#endif
}

/*
void CPlayerPlaylistBar::OnCustomdrawList(NMHDR* pNMHDR, LRESULT* pResult)
{
    NMLVCUSTOMDRAW* pLVCD = reinterpret_cast<NMLVCUSTOMDRAW*>(pNMHDR);

    *pResult = CDRF_DODEFAULT;

    if (CDDS_PREPAINT == pLVCD->nmcd.dwDrawStage)
    {
        *pResult = CDRF_NOTIFYPOSTPAINT|CDRF_NOTIFYITEMDRAW;
    }
    else if (CDDS_ITEMPREPAINT == pLVCD->nmcd.dwDrawStage)
    {
        pLVCD->nmcd.uItemState &= ~CDIS_SELECTED;
        pLVCD->nmcd.uItemState &= ~CDIS_FOCUS;

        pLVCD->clrText = (pLVCD->nmcd.dwItemSpec == m_playList.m_idx) ? 0x0000ff : CLR_DEFAULT;
        pLVCD->clrTextBk = m_list.GetItemState(pLVCD->nmcd.dwItemSpec, LVIS_SELECTED) ? 0xf1dacc : CLR_DEFAULT;

        *pResult = CDRF_NOTIFYPOSTPAINT;
    }
    else if (CDDS_ITEMPOSTPAINT == pLVCD->nmcd.dwDrawStage)
    {
        int nItem = static_cast<int>(pLVCD->nmcd.dwItemSpec);

        if (m_list.GetItemState(pLVCD->nmcd.dwItemSpec, LVIS_SELECTED))
        {
            CRect r, r2;
            m_list.GetItemRect(nItem, &r, LVIR_BOUNDS);
            m_list.GetItemRect(nItem, &r2, LVIR_LABEL);
            r.left = r2.left;
            FrameRect(pLVCD->nmcd.hdc, &r, CBrush(0xc56a31));
        }

        *pResult = CDRF_SKIPDEFAULT;
    }
    else if (CDDS_POSTPAINT == pLVCD->nmcd.dwDrawStage)
    {
    }
}
*/

void CPlayerPlaylistBar::OnDrawItem(int nIDCtl, LPDRAWITEMSTRUCT lpDrawItemStruct)
{
    if (nIDCtl != IDC_PLAYLIST) {
        return;
    }

    int dpi3 = m_pMainFrame->m_dpi.ScaleX(3);
    int dpi6 = m_pMainFrame->m_dpi.ScaleX(6);

    int nItem = lpDrawItemStruct->itemID;
    CRect rcItem = lpDrawItemStruct->rcItem;
    POSITION pos = FindPos(nItem);
    if (pos == NULL) {
        return;
    }
    if (m_pl.IsEmpty()) {
        ASSERT(false);
        return;
    }
    bool itemPlaying = pos == m_pl.GetPos();
    bool itemSelected = !!m_list.GetItemState(nItem, LVIS_SELECTED);
    CPlaylistItem& pli = m_pl.GetAt(pos);

    CDC* pDC = CDC::FromHandle(lpDrawItemStruct->hDC);
    int oldDC = pDC->SaveDC();

    CString fmt, file, num;

    fmt.Format(_T("[%%0%dd]"), (int)log10(0.1 + m_pl.GetCount()) + 1);
    num.Format(fmt, nItem + 1);
    CSize numWidth = pDC->GetTextExtent(num) + pDC->GetTextExtent(L"w");

    COLORREF bgColor, contentBGColor;

    if (AppNeedsThemedControls()) {
        contentBGColor = CMPCTheme::ContentBGColor;
    } else {
        contentBGColor = GetSysColor(COLOR_WINDOW);
    }

    CRect fileRect = rcItem, seqRect = rcItem;
    fileRect.left += numWidth.cx;
    inlineEditXpos = numWidth.cx - 2; //magic number 2 for accounting for border/padding of inline edit.  works at all dpi except 168, where it's off by 1px (shrug)
    seqRect.right = fileRect.left;

    if (itemSelected) {
        if (AppNeedsThemedControls()) {
            bgColor = CMPCTheme::ContentSelectedColor;
        } else {
            bgColor = 0xf1dacc;
            FrameRect(pDC->m_hDC, rcItem, CBrush(0xc56a31));
        }
        FillRect(pDC->m_hDC, fileRect, CBrush(bgColor));
        FillRect(pDC->m_hDC, seqRect, CBrush(contentBGColor));
    } else {
        bgColor = contentBGColor;
        FillRect(pDC->m_hDC, rcItem, CBrush(bgColor));
    }

    COLORREF textColor, sequenceColor;

    if (AppNeedsThemedControls()) {
        if (pli.m_fInvalid) {
            textColor = CMPCTheme::ContentTextDisabledFGColorFade2;
            sequenceColor = textColor;
        } else if (itemPlaying) {
            textColor = itemSelected ? CMPCTheme::ActivePlayListItemHLColor : CMPCTheme::ActivePlayListItemColor;
            sequenceColor = CMPCTheme::ActivePlayListItemColor;
        } else {
            textColor = CMPCTheme::TextFGColor;
            sequenceColor = textColor;
        }
    } else {
        textColor = itemPlaying ? 0xff : 0;
        if (pli.m_fInvalid) {
            textColor |= 0xA0A0A0;
        }
        sequenceColor = textColor;
    }

    CString time = !pli.m_fInvalid ? pli.GetLabel(1) : CString(_T("Invalid"));
    CPoint timept(rcItem.right, 0);
    if (!time.IsEmpty()) {
        CSize timesize = pDC->GetTextExtent(time);
        if ((dpi3 + timesize.cx + dpi3) < rcItem.Width() / 2) {
            timept = CPoint(rcItem.right - (dpi3 + timesize.cx + dpi3), (rcItem.top + rcItem.bottom - timesize.cy) / 2);

            pDC->SetTextColor(textColor);
            pDC->SetBkColor(bgColor);
            pDC->TextOut(timept.x, timept.y, time);
        }
    }
    pli.inlineEditMaxWidth = timept.x - inlineEditXpos;

    file = pli.GetLabel(0);
    CSize filesize = pDC->GetTextExtent(file);
    while (dpi3 + numWidth.cx + filesize.cx + dpi6 > timept.x && file.GetLength() > 3) {
        file = file.Left(file.GetLength() - 4) + _T("...");
        filesize = pDC->GetTextExtent(file);
    }

    CWnd* pFocused = GetFocus();
    bool inlineEditActive = pFocused && pFocused != &m_list && m_list.IsChild(pFocused);
    if (!inlineEditActive || !itemSelected) { //if inline edit is active, and this is the selected item, don't draw filename (visually distracting while editing)
        pDC->SetTextColor(textColor);
        pDC->SetBkColor(bgColor);
        pDC->TextOut(rcItem.left + dpi3 + numWidth.cx, (rcItem.top + rcItem.bottom - filesize.cy) / 2, file);
    }

    if (!itemPlaying) {
        pDC->SetTextColor(CMPCTheme::ContentTextDisabledFGColorFade);
    } else {
        pDC->SetTextColor(sequenceColor);
    }
    pDC->SetBkColor(contentBGColor);
    pDC->TextOut(rcItem.left + dpi3, (rcItem.top + rcItem.bottom - filesize.cy) / 2, num);


    pDC->RestoreDC(oldDC);
}

BOOL CPlayerPlaylistBar::OnPlayPlay(UINT nID)
{
    m_list.Invalidate();
    return FALSE;
}

DROPEFFECT CPlayerPlaylistBar::OnDropAccept(COleDataObject*, DWORD, CPoint)
{
    return DROPEFFECT_COPY;
}

void CPlayerPlaylistBar::OnDropFiles(CAtlList<CString>& slFiles, DROPEFFECT)
{
    SetForegroundWindow();
    m_list.SetFocus();

    if ((slFiles.GetCount() == 1) && m_pMainFrame->CanSendToYoutubeDL(slFiles.GetHead())) {
        if (m_pMainFrame->ProcessYoutubeDLURL(slFiles.GetHead(), true)) {
            return;
        } else if (m_pMainFrame->IsOnYDLWhitelist(slFiles.GetHead())) {
            // don't bother trying to open this website URL directly
            return;
        }
    }

    PathUtils::ParseDirs(slFiles);

    Append(slFiles, true);
}

void CPlayerPlaylistBar::OnBeginDrag(NMHDR* pNMHDR, LRESULT* pResult)
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

void CPlayerPlaylistBar::OnMouseMove(UINT nFlags, CPoint point)
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

void CPlayerPlaylistBar::OnTimer(UINT_PTR nIDEvent)
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

void CPlayerPlaylistBar::OnLButtonUp(UINT nFlags, CPoint point)
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

void CPlayerPlaylistBar::DropItemOnList()
{
    m_ptDropPoint.y += 10;
    m_nDropIndex = m_list.HitTest(CPoint(10, m_ptDropPoint.y));

    if (m_nDropIndex < 0) {
        m_nDropIndex = (int)m_pl.GetCount();
    }

    ASSERT(m_indexToPos.size() == (size_t)m_pl.GetCount());

    POSITION dragPos = FindPos(m_nDragIndex);
    if (!dragPos || m_nDragIndex == m_nDropIndex) {
        return;
    }

    // Build the desired order by moving dragPos within m_indexToPos
    int dropIdx = m_nDropIndex;
    if (m_nDropIndex > m_nDragIndex) {
        dropIdx--;
    }
    m_indexToPos.erase(m_indexToPos.begin() + m_nDragIndex);
    if (dropIdx > (int)m_indexToPos.size()) {
        dropIdx = (int)m_indexToPos.size();
    }
    m_indexToPos.insert(m_indexToPos.begin() + dropIdx, dragPos);

    // Reorder m_pl to match the new visual order (preserves POSITIONs)
    for (size_t i = 0; i < m_indexToPos.size(); i++) {
        m_pl.MoveToTail(m_indexToPos[i]);
    }

    RebuildPosMap();
    m_list.Invalidate();
    ResizeListColumn();

    int newIndex = FindItem(dragPos);
    if (newIndex >= 0) {
        // In virtual list mode, explicitly clear old states and set new ones
        m_list.SetItemState(-1, 0, LVIS_SELECTED | LVIS_FOCUSED);
        m_list.SetItemState(newIndex, LVIS_SELECTED | LVIS_FOCUSED, LVIS_SELECTED | LVIS_FOCUSED);
        m_list.SetSelectionMark(newIndex);
    }
}

BOOL CPlayerPlaylistBar::OnToolTipNotify(UINT id, NMHDR* pNMHDR, LRESULT* pResult)
{
    TOOLTIPTEXT* pTTT = (TOOLTIPTEXT*)pNMHDR;

    if ((HWND)pTTT->lParam != m_list.m_hWnd) {
        return FALSE;
    }

    int row = ((pNMHDR->idFrom - 1) >> 10) & 0x3fffff;
    int col = (pNMHDR->idFrom - 1) & 0x3ff;

    if (row < 0 || size_t(row) >= m_pl.GetCount()) {
        return FALSE;
    }

    CPlaylistItem& pli = m_pl.GetAt(FindPos(row));

    static CString strTipText; // static string
    strTipText.Empty();

    if (col == COL_NAME) {
        POSITION pos = pli.m_fns.GetHeadPosition();
        while (pos) {
            strTipText += _T("\n") + pli.m_fns.GetNext(pos);
        }
        strTipText.Trim();

        if (pli.m_type == CPlaylistItem::device) {
            if (pli.m_vinput >= 0) {
                strTipText.AppendFormat(_T("\nVideo Input %d"), pli.m_vinput);
            }
            if (pli.m_vchannel >= 0) {
                strTipText.AppendFormat(_T("\nVideo Channel %d"), pli.m_vchannel);
            }
            if (pli.m_ainput >= 0) {
                strTipText.AppendFormat(_T("\nAudio Input %d"), pli.m_ainput);
            }
        }

        ::SendMessage(pNMHDR->hwndFrom, TTM_SETMAXTIPWIDTH, 0, 1000);
    } else if (col == COL_TIME) {
        return FALSE;
    }

    pTTT->lpszText = (LPWSTR)(LPCWSTR)strTipText;

    *pResult = 0;

    return TRUE;    // message was handled
}

void CPlayerPlaylistBar::OnContextMenu(CWnd* /*pWnd*/, CPoint point)
{
    CAutoLock pledit(&m_plEditLock);

    LVHITTESTINFO lvhti;

    bool bOnItem;
    if (point.x == -1 && point.y == -1) {
        lvhti.iItem = m_list.GetSelectionMark();

        if (lvhti.iItem == -1 && m_pl.GetCount() == 1) {
            lvhti.iItem = 0;
        }

        CRect r;
        if (!!m_list.GetItemRect(lvhti.iItem, r, LVIR_BOUNDS)) {
            point.SetPoint(r.left, r.bottom);
        } else {
            point.SetPoint(0, 0);
        }
        m_list.ClientToScreen(&point);
        bOnItem = lvhti.iItem != -1;
    } else {
        lvhti.pt = point;
        m_list.ScreenToClient(&lvhti.pt);
        m_list.SubItemHitTest(&lvhti);
        bOnItem = lvhti.iItem >= 0 && !!(lvhti.flags & LVHT_ONITEM);
        if (!bOnItem && m_pl.GetCount() == 1) {
            bOnItem = true;
            lvhti.iItem = 0;
        }
    }

    POSITION pos = FindPos(lvhti.iItem);
    bool bIsLocalFile = bOnItem ? PathUtils::Exists(m_pl.GetAt(pos).m_fns.GetHead()) : false;

    CMPCThemeMenu m;
    m.CreatePopupMenu();

    enum {
        M_OPEN = 1,
        M_ADD,
        M_REMOVE,
        M_CLEAR,
        M_CLIPBOARD,
        M_SHOWFOLDER,
        M_ADDFOLDER,
        M_RECYCLE,
        M_SAVE,
        M_SAVEAS,
        M_SORTBYNAME,
        M_SORTBYPATH,
        M_RANDOMIZE,
        M_SORTBYID,
        M_SHUFFLE,
        M_HIDEFULLSCREEN
    };

    CAppSettings& s = AfxGetAppSettings();

    m.AppendMenu(MF_STRING | (!bOnItem ? (MF_DISABLED | MF_GRAYED) : MF_ENABLED), M_OPEN, ResStr(IDS_PLAYLIST_OPEN));
    if (m_pMainFrame->GetPlaybackMode() == PM_ANALOG_CAPTURE) {
        m.AppendMenu(MF_STRING | MF_ENABLED, M_ADD, ResStr(IDS_PLAYLIST_ADD));
    }
    m.AppendMenu(MF_STRING | (!bOnItem ? (MF_DISABLED | MF_GRAYED) : MF_ENABLED), M_REMOVE, ResStr(IDS_PLAYLIST_REMOVE));
    m.AppendMenu(MF_SEPARATOR);
    m.AppendMenu(MF_STRING | (!m_pl.GetCount() ? (MF_DISABLED | MF_GRAYED) : MF_ENABLED), M_CLEAR, ResStr(IDS_PLAYLIST_CLEAR));
    m.AppendMenu(MF_SEPARATOR);
    m.AppendMenu(MF_STRING | (!bOnItem ? (MF_DISABLED | MF_GRAYED) : MF_ENABLED), M_CLIPBOARD, ResStr(IDS_PLAYLIST_COPYTOCLIPBOARD));
    m.AppendMenu(MF_STRING | ((!bOnItem || !bIsLocalFile) ? (MF_DISABLED | MF_GRAYED) : MF_ENABLED), M_SHOWFOLDER, ResStr(IDS_PLAYLIST_SHOWFOLDER));
    m.AppendMenu(MF_STRING | ((!bOnItem || !bIsLocalFile) ? (MF_DISABLED | MF_GRAYED) : MF_ENABLED), M_RECYCLE, ResStr(IDS_FILE_RECYCLE));
    m.AppendMenu(MF_STRING | ((!bOnItem || !bIsLocalFile) ? (MF_DISABLED | MF_GRAYED) : MF_ENABLED), M_ADDFOLDER, ResStr(IDS_PLAYLIST_ADDFOLDER));
    m.AppendMenu(MF_SEPARATOR);
    if (!m_playListPath.IsEmpty()) {
        m.AppendMenu(MF_STRING | (!m_pl.GetCount() ? (MF_DISABLED | MF_GRAYED) : MF_ENABLED), M_SAVE, ResStr(IDS_PLAYLIST_SAVE));
    }
    m.AppendMenu(MF_STRING | (!m_pl.GetCount() ? (MF_DISABLED | MF_GRAYED) : MF_ENABLED), M_SAVEAS, ResStr(IDS_PLAYLIST_SAVEAS));
    m.AppendMenu(MF_SEPARATOR);
    {
        CMPCThemeMenu sortMenu;
        sortMenu.CreatePopupMenu();
        UINT sortGray = !m_pl.GetCount() ? (MF_DISABLED | MF_GRAYED) : MF_ENABLED;
        sortMenu.AppendMenu(MF_STRING | sortGray, M_SORTBYNAME, ResStr(IDS_PLAYLIST_SORTBYLABEL));
        sortMenu.AppendMenu(MF_STRING | sortGray, M_SORTBYPATH, ResStr(IDS_PLAYLIST_SORTBYPATH));
        sortMenu.AppendMenu(MF_STRING | sortGray, M_RANDOMIZE, ResStr(IDS_PLAYLIST_RANDOMIZE));
        sortMenu.AppendMenu(MF_SEPARATOR);
        sortMenu.AppendMenu(MF_STRING | sortGray, M_SORTBYID, ResStr(IDS_PLAYLIST_RESTORE));
        m.AppendMenu(MF_POPUP | sortGray, (UINT_PTR)sortMenu.GetSafeHmenu(), ResStr(IDS_PLAYLIST_SORT));
        sortMenu.Detach();
    }
    m.AppendMenu(MF_SEPARATOR);
    m.AppendMenu(MF_STRING | MF_ENABLED | (s.bShufflePlaylistItems ? MF_CHECKED : MF_UNCHECKED), M_SHUFFLE, ResStr(IDS_PLAYLIST_SHUFFLE));
    m.AppendMenu(MF_SEPARATOR);
    m.AppendMenu(MF_STRING | MF_ENABLED | (s.bHidePlaylistFullScreen ? MF_CHECKED : MF_UNCHECKED), M_HIDEFULLSCREEN, ResStr(IDS_PLAYLIST_HIDEFS));
    if (AppNeedsThemedControls()) {
        m.fulfillThemeReqs();
    }

    m_bHasActivePopup = true;
    //use mainframe as parent to take advantage of measure redirect (was 'this' but text was not printed)
    //adipose: note this will bypass CPlayerBar::OnEnterMenuLoop, so we set m_bHasActivePopup directly here
    int nID = (int)m.TrackPopupMenu(TPM_LEFTBUTTON | TPM_RETURNCMD, point.x, point.y, m_pMainFrame);
    m_bHasActivePopup = false;
    m_list.SetFocus();
    switch (nID) {
        case M_OPEN:
            m_pl.SetPos(pos);
            m_list.Invalidate();
            m_pMainFrame->PostMessage(WM_MPC_OPENCURPLAYLIST, 0, 0);
            break;
        case M_ADD:
            m_pMainFrame->AddCurDevToPlaylist();
            m_pl.SetPos(m_pl.GetTailPosition());
            break;
        case M_REMOVE:
            if (m_pl.GetCount() > 1) {
                if (m_pl.RemoveAt(pos)) {
                    m_pMainFrame->PostMessage(WM_COMMAND, ID_FILE_CLOSEMEDIA);
                }
                RebuildPosMap();
                m_list.SetItemCountEx((int)m_pl.GetCount(), LVSICF_NOINVALIDATEALL);
                m_list.Invalidate();
            } else {
                if (Empty()) {
                    m_pMainFrame->PostMessage(WM_COMMAND, ID_FILE_CLOSEMEDIA);
                }
            }
            SavePlaylist(true);
            break;
        case M_RECYCLE:
            DeleteFileInPlaylist(pos, true);
            break;
        case M_CLEAR:
            if (Empty()) {
                m_pMainFrame->PostMessage(WM_COMMAND, ID_FILE_CLOSEMEDIA);
            }
            SavePlaylist();
            break;
        case M_SORTBYID: {
            POSITION selPos = FindPos(m_list.GetSelectionMark());
            m_pl.SortById();
            SetupList();
            SyncSelectionToPos(selPos ? selPos : m_pl.GetPos());
            SavePlaylist();
            break;
        }
        case M_SORTBYNAME: {
            POSITION selPos = FindPos(m_list.GetSelectionMark());
            m_pl.SortByName();
            SetupList();
            SyncSelectionToPos(selPos ? selPos : m_pl.GetPos());
            SavePlaylist();
            break;
        }
        case M_SORTBYPATH: {
            POSITION selPos = FindPos(m_list.GetSelectionMark());
            m_pl.SortByPath();
            SetupList();
            SyncSelectionToPos(selPos ? selPos : m_pl.GetPos());
            SavePlaylist();
            break;
        }
        case M_RANDOMIZE:
            Randomize();
            break;
        case M_CLIPBOARD: {
                CString str;

                CPlaylistItem& pli = m_pl.GetAt(pos);
                POSITION pos2 = pli.m_fns.GetHeadPosition();
                while (pos2) {
                    str += _T("\r\n") + pli.m_fns.GetNext(pos2);
                }
                str.Trim();

                CClipboard clipboard(this);
                VERIFY(clipboard.SetText(str));
            }
            break;
        case M_SHOWFOLDER:
            ExploreToFile(m_pl.GetAt(pos).m_fns.GetHead());
            break;
        case M_ADDFOLDER: {
            // add all media files in current playlist item folder that are not yet in the playlist
            const CString dirName = PathUtils::DirName(m_pl.GetAt(pos).m_fns.GetHead());
            if (PathUtils::IsDir(dirName)) {
                m_insertingPos = pos;
                if (AddItemsInFolder(dirName, true)) {
                    Refresh();
                    SavePlaylist();
                }
            }
            break;
        }
        case M_SAVE: {
            SaveMPCPlayList(m_playListPath, CTextFile::UTF8);
            break;
        }
        case M_SAVEAS: {
            CSaveTextFileDialog fd(
                CTextFile::DEFAULT_ENCODING, nullptr, nullptr,
                _T("MPC-HC playlist (*.mpcpl)|*.mpcpl|Playlist (*.pls)|*.pls|Winamp playlist (*.m3u)|*.m3u|Windows Media playlist (*.asx)|*.asx||"),
                this);

            if (fd.DoModal() != IDOK) {
                break;
            }

            CTextFile::enc encoding = (CTextFile::enc)fd.GetEncoding();

            int idx = fd.m_pOFN->nFilterIndex;

            if (encoding == CTextFile::DEFAULT_ENCODING) {
                if (idx == 1 || idx == 3) {
                    encoding = CTextFile::UTF8;
                } else {
                    encoding = CTextFile::ANSI;
                }
            }

            CPath path(fd.GetPathName());

            switch (idx) {
                case 1:
                    path.AddExtension(_T(".mpcpl"));
                    break;
                case 2:
                    path.AddExtension(_T(".pls"));
                    break;
                case 3:
                    path.AddExtension(_T(".m3u"));
                    break;
                case 4:
                    path.AddExtension(_T(".asx"));
                    break;
                default:
                    break;
            }

            if (idx == 1) {
                SaveMPCPlayList(path, encoding);
                m_playListPath = (LPCTSTR)path;
                break;
            }

            CTextFile f;
            if (!f.Save(path, encoding)) {
                break;
            }

            if (idx == 2) {
                f.WriteString(_T("[playlist]\n"));
            } else if (idx == 4) {
                f.WriteString(_T("<ASX version = \"3.0\">\n"));
            }

            pos = m_pl.GetHeadPosition();
            CString str;
            int i;
            for (i = 0; pos; i++) {
                CPlaylistItem& pli = m_pl.GetNext(pos);

                if (pli.m_type != CPlaylistItem::file) {
                    continue;
                }

                CString fn = pli.m_fns.GetHead();

                /*
                if (idx != 4 && PlaylistCanStripPath(path))
                {
                    CPath p(path);
                    p.StripPath();
                    fn = (LPCTSTR)p;
                }
                */

                switch (idx) {
                    case 2:
                        str.Format(_T("File%d=%s\n"), i + 1, fn.GetString());
                        break;
                    case 3:
                        str.Format(_T("%s\n"), fn.GetString());
                        break;
                    case 4:
                        str.Format(_T("<Entry><Ref href = \"%s\"/></Entry>\n"), fn.GetString());
                        break;
                    default:
                        break;
                }
                f.WriteString(str);
            }

            if (idx == 2) {
                str.Format(_T("NumberOfEntries=%d\n"), i);
                f.WriteString(str);
                f.WriteString(_T("Version=2\n"));
            } else if (idx == 4) {
                f.WriteString(_T("</ASX>\n"));
            }
        }
        break;
        case M_SHUFFLE:
            s.bShufflePlaylistItems = !s.bShufflePlaylistItems;
            m_pl.SetShuffle(s.bShufflePlaylistItems);
            break;
        case M_HIDEFULLSCREEN:
            s.bHidePlaylistFullScreen = !s.bHidePlaylistFullScreen;
            m_pMainFrame->HidePlaylistFullScreen();
            break;
        default:
            break;
    }
}

void CPlayerPlaylistBar::OnLvnGetDispInfoList(NMHDR* pNMHDR, LRESULT* pResult)
{
    NMLVDISPINFO* pDispInfo = reinterpret_cast<NMLVDISPINFO*>(pNMHDR);
    LVITEM& item = pDispInfo->item;

    POSITION pos = FindPos(item.iItem);
    if (!pos) {
        return;
    }
    CPlaylistItem& pli = m_pl.GetAt(pos);

    if (item.mask & LVIF_TEXT) {
        CString text = pli.GetLabel(item.iSubItem);
        _tcsncpy_s(item.pszText, item.cchTextMax, text, _TRUNCATE);
    }

    *pResult = 0;
}

void CPlayerPlaylistBar::OnLvnBeginlabeleditList(NMHDR* pNMHDR, LRESULT* pResult)
{
    NMLVDISPINFO* pDispInfo = reinterpret_cast<NMLVDISPINFO*>(pNMHDR);
    POSITION pos = FindPos(pDispInfo->item.iItem);
    int maxW = pos ? m_pl.GetAt(pos).inlineEditMaxWidth : -1;
    m_list.AdjustVirtualEditPos(inlineEditXpos, maxW);
    *pResult = 1; // allow editing
}

void CPlayerPlaylistBar::OnLvnEndlabeleditList(NMHDR* pNMHDR, LRESULT* pResult)
{
    NMLVDISPINFO* pDispInfo = reinterpret_cast<NMLVDISPINFO*>(pNMHDR);
    // pszText is nullptr when the edit was cancelled (ESC)
    if (pDispInfo->item.pszText != nullptr) {
        POSITION pos = FindPos(pDispInfo->item.iItem);
        if (pos) {
            CString text(pDispInfo->item.pszText);
            text.Trim();
            m_pl.GetAt(pos).m_label = text;
            RefreshItem(pos);
        }
    }
    *pResult = 0;
}

void CPlayerPlaylistBar::OnXButtonDown(UINT nFlags, UINT nButton, CPoint point)
{
    UNREFERENCED_PARAMETER(nFlags);
    UNREFERENCED_PARAMETER(point);
    if (m_pMainFrame->GetPlaybackMode() == PM_FILE && GetCount() > 1) {
        switch (nButton) {
            case XBUTTON1:
                if (SetPrev()) {
                    m_pMainFrame->PostMessage(WM_MPC_OPENCURPLAYLIST, 0, 0);
                }
                break;
            case XBUTTON2:
                if (SetNext()) {
                    m_pMainFrame->PostMessage(WM_MPC_OPENCURPLAYLIST, 0, 0);
                }
                break;
        }
    }
}

void CPlayerPlaylistBar::OnXButtonUp(UINT nFlags, UINT nButton, CPoint point)
{
    UNREFERENCED_PARAMETER(nFlags);
    UNREFERENCED_PARAMETER(nButton);
    UNREFERENCED_PARAMETER(point);
    // do nothing
}

void CPlayerPlaylistBar::OnXButtonDblClk(UINT nFlags, UINT nButton, CPoint point)
{
    OnXButtonDown(nFlags, nButton, point);
}
