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
#include "CommandLine.h"
#include "MainFrm.h"
#include "mplayerc.h"
#include "FakeFilterMapper2.h"
#include "YoutubeDL.h"
#include <PathUtils.h>
#include <DSUtil.h>

CCommandLine::CCommandLine(CMainFrame& frame)
    : m_frame(frame)
    , m_dwLastRun(0)
    , m_nLastAppendSelectionIndex(0)
    , m_sydlLastProcessURL()
{
}

// A command line redirected from another instance. CMainFrame::OnCopyData has already
// handed anything else that arrives by WM_COPYDATA to the API.
BOOL CCommandLine::ReceiveCommandLine(const COPYDATASTRUCT* pCDS)
{
    if (m_frame.m_capture.m_bScanDlgOpened) {
        return FALSE;
    }

    CAtlList<CString> cmdln;
    UnpackCommandLine(pCDS, cmdln);

    QueueCommandLine(cmdln);

    return TRUE;
}

void CCommandLine::UnpackCommandLine(const COPYDATASTRUCT* pCDS, CAtlList<CString>& cmdln)
{
    DWORD len = *((DWORD*)pCDS->lpData);
    TCHAR* pBuff = (TCHAR*)((DWORD*)pCDS->lpData + 1);
    TCHAR* pBuffEnd = pBuff + (pCDS->cbData - sizeof(DWORD)) / sizeof(TCHAR);

    while (len-- > 0 && pBuff < pBuffEnd) {
        // the sender is another process, so do not trust it to terminate the last string
        size_t n = wcsnlen(pBuff, pBuffEnd - pBuff);
        CString str(pBuff, (int)n);
        pBuff += n + 1;

        cmdln.AddTail(str);
    }
}

// Queue a command line and return at once. Everything that acts on it probes the filesystem,
// and a path on an unreachable share blocks that for half a minute. While it ran inside the
// WM_COPYDATA handler the window was not pumping messages, so Windows marked it as not
// responding and the other instances redirecting to us gave up and each opened a window of
// their own (#4149). The arrival time travels with the command line, so a slow open cannot
// make the next file of the same Explorer selection look like a new one. The Explorer drop
// target (ShellDropTarget.cpp) comes in here too, with Explorer itself waiting on the return.
void CCommandLine::QueueCommandLine(const CAtlList<CString>& cmdln)
{
    m_pendingCommandLines.emplace_back();
    PendingCommandLine& pending = m_pendingCommandLines.back();
    pending.tArrived = GetTickCount64();
    pending.cmdln.AddTailList(&cmdln);
    VERIFY(m_frame.PostMessage(WM_MPC_CMDLINE));
}

LRESULT CCommandLine::OnCommandLineReceived(WPARAM wParam, LPARAM lParam)
{
    if (m_bProcessingCommandLine) {
        // Re-entered through a nested message pump: opening and closing the graph both pump
        // posted messages. The call already running drains the queue before it returns.
        return 0;
    }

    m_bProcessingCommandLine = true;
    while (!m_pendingCommandLines.empty()) {
        PendingCommandLine& pending = m_pendingCommandLines.front();
        ProcessCommandLine(pending.cmdln, pending.tArrived);
        m_pendingCommandLines.pop_front();
    }
    m_bProcessingCommandLine = false;

    return 0;
}

void CCommandLine::ProcessCommandLine(CAtlList<CString>& cmdln, ULONGLONG tArrived)
{
    // Re-checked because this now runs later than the message that carried the command line.
    if (AfxGetMyApp()->m_fClosingState || m_frame.m_capture.m_bScanDlgOpened) {
        return;
    }
    {
        auto pCmdln = std::make_shared<CAtlList<CString>>();
        pCmdln->AddTailList(&cmdln);
        // may add to the playlist rather than open, so it is never discarded
        if (m_frame.DeferIfNested(CMainFrame::DeferredActionType::Other, [this, pCmdln, tArrived] { ProcessCommandLine(*pCmdln, tArrived); })) {
            return;
        }
    }

    CAppSettings& s = AfxGetAppSettings();

    s.ParseCommandLine(cmdln);

    if (s.nCLSwitches & CLSW_SLAVE) {
        m_frame.m_api.m_lastApiVolume = m_frame.GetVolume();
        m_frame.m_api.m_lastApiMute = m_frame.IsMuted() ? 1 : 0;
        m_frame.m_api.m_hostIntApiVersion = 0; // new host: integer channel unconfirmed until it says HELLO
        m_frame.m_api.SendAPICommand(CMD_CONNECT, L"%d", PtrToInt(m_frame.GetSafeHwnd()));
        s.nCLSwitches &= ~CLSW_SLAVE;
    }

    POSITION pos = s.slFilters.GetHeadPosition();
    while (pos) {
        CString fullpath = MakeFullPath(s.slFilters.GetNext(pos));

        CLongPath tmp(fullpath);
        tmp.RemoveFileSpec();
        tmp.AddBackslash();
        CString path = tmp;

        WIN32_FIND_DATA fd;
        ZeroMemory(&fd, sizeof(WIN32_FIND_DATA));
        HANDLE hFind = FindFirstFile(fullpath, &fd);
        if (hFind != INVALID_HANDLE_VALUE) {
            do {
                if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
                    continue;
                }

                CFilterMapper2 fm2(false);
                fm2.Register(path + fd.cFileName);
                while (!fm2.m_filters.IsEmpty()) {
                    if (FilterOverride* f = fm2.m_filters.RemoveTail()) {
                        f->fTemporary = true;

                        bool fFound = false;

                        POSITION pos2 = s.m_filters.GetHeadPosition();
                        while (pos2) {
                            FilterOverride* f2 = s.m_filters.GetNext(pos2);
                            if (f2->type == FilterOverride::EXTERNAL && !f2->path.CompareNoCase(f->path)) {
                                fFound = true;
                                break;
                            }
                        }

                        if (!fFound) {
                            CAutoPtr<FilterOverride> p(f);
                            s.m_filters.AddHead(p);
                        }
                    }
                }
            } while (FindNextFile(hFind, &fd));

            FindClose(hFind);
        }
    }

    bool fSetForegroundWindow = false;

    auto applyRandomizeSwitch = [&]() {
        if (s.nCLSwitches & CLSW_RANDOMIZE) {
            m_frame.m_wndPlaylistBar.Randomize();
            s.nCLSwitches &= ~CLSW_RANDOMIZE;
        }
    };

    if ((s.nCLSwitches & CLSW_DVD) && !s.slFiles.IsEmpty()) {
        if (!m_frame.CloseMediaBeforeOpen()) {
            return;
        }
        fSetForegroundWindow = true;

        CAutoPtr<OpenDVDData> p(DEBUG_NEW OpenDVDData());
        if (p) {
            p->path = s.slFiles.GetHead();
            p->subs.AddTailList(&s.slSubs);
            m_frame.m_wndPlaylistBar.OpenDVD(p->path);
        }
        // ToDo: open indirectly
        m_frame.OpenMedia(p);
        s.nCLSwitches &= ~CLSW_DVD;
    } else if (s.nCLSwitches & CLSW_CD) {
        if (m_frame.GetMediaState() == State_Running) {
            m_frame.MediaControlPause(true);
        }
        fSetForegroundWindow = true;

        CAtlList<CString> sl;
        if (!s.slFiles.IsEmpty()) {
            GetOpticalDiskType(s.slFiles.GetHead()[0], sl);
        } else {
            CString dir;
            dir.ReleaseBufferSetLength(GetCurrentDirectory(2048, dir.GetBuffer(2048)));

            GetOpticalDiskType(dir[0], sl);

            for (TCHAR drive = _T('A'); sl.IsEmpty() && drive <= _T('Z'); drive++) {
                GetOpticalDiskType(drive, sl);
            }
        }

        m_frame.m_wndPlaylistBar.Open(sl, true);
        applyRandomizeSwitch();
        s.nCLSwitches &= ~CLSW_CD;
        m_frame.PostMessage(WM_MPC_OPENCURPLAYLIST, 0, 0);
    } else if (s.nCLSwitches & (CLSW_DEVICE | CLSW_DVBSCAN)) {
        // OnFileOpendevice shows the capture options page and returns when no
        // device is configured. Interactively that is a prompt; for a headless
        // run it is a modal nobody can answer, and the process would sit there
        // with no device, no scan and nothing to time out. Check the same
        // condition first and fail the run instead.
        if ((s.nCLSwitches & CLSW_DVBSCAN) && s.iDefaultCaptureDevice == 0 &&
                s.strAnalogVideo == L"dummy" && s.strAnalogAudio == L"dummy") {
            TRACE(_T("/dvbscan: no capture device configured, nothing to scan\n"));
            s.nCLSwitches &= ~CLSW_DVBSCAN;
            AfxGetMyApp()->m_nExitCode = 1;
            m_frame.PostMessage(WM_CLOSE);
        } else {
            // /dvbscan implies opening the capture device, because DoTunerScan
            // only runs in PM_DIGITAL_CAPTURE. CLSW_DVBSCAN is deliberately
            // left set: OnFilePostOpenmedia consumes it once the device is up.
            m_frame.PostMessage(WM_COMMAND, ID_FILE_OPENDEVICE);
            s.nCLSwitches &= ~CLSW_DEVICE;
        }
    } else if (!s.slFiles.IsEmpty()) {
        CAtlList<CString> sl;
        sl.AddTailList(&s.slFiles);

        PathUtils::ParseDirs(sl);

        bool fMulti = sl.GetCount() > 1;
        if (!fMulti) {
            sl.AddTailList(&s.slDubs);
        }

        if (m_frame.m_disc.OpenBD(s.slFiles.GetHead())) {
            // Nothing more to do
        } else if (!fMulti && CLongPath(s.slFiles.GetHead() + _T("\\VIDEO_TS")).IsDirectory()) {
            fSetForegroundWindow = true;

            if (m_frame.GetMediaState() == State_Running) {
                m_frame.MediaControlPause(true);
            }

            CAutoPtr<OpenDVDData> p(DEBUG_NEW OpenDVDData());
            if (p) {
                p->path = s.slFiles.GetHead();
                p->subs.AddTailList(&s.slSubs);
                m_frame.m_wndPlaylistBar.OpenDVD(p->path);
            }
            // ToDo: open indirectly
            m_frame.OpenMedia(p);
        } else {
            // When the command line reached us, not when we got round to acting on it: an open
            // that took a long time must not make the next file of the same selection, which
            // arrived while we were busy, look like the start of a new one.
            const ULONGLONG tcnow = tArrived;
            // Opening a multi-file selection in Explorer spawns one process per file. Those arrive here in
            // arbitrary order, so the entries added by the second and later ones are sorted back into place.
            bool bSameSelection = m_dwLastRun && ((tcnow - m_dwLastRun) < s.iRedirectOpenToAppendThreshold);
            if (bSameSelection) {
                s.nCLSwitches |= CLSW_ADD;
            }
            m_dwLastRun = tcnow;
            bool bRandomize = !!(s.nCLSwitches & CLSW_RANDOMIZE);

            if ((s.nCLSwitches & CLSW_ADD) && !m_frame.IsPlaylistEmpty()) {
                if (!bSameSelection) { // a new selection starts at the current end of the playlist
                    m_nLastAppendSelectionIndex = (int)m_frame.m_wndPlaylistBar.GetCount();
                }

                if (!fMulti && sl.GetCount() > 1) {
                    // video + dub, appended as one entry like the open path below (#4224)
                    m_frame.m_wndPlaylistBar.Append(sl, false, &s.slSubs);
                } else {
                    POSITION pos2 = sl.GetHeadPosition();
                    while (pos2) {
                        CString fn = sl.GetNext(pos2);
                        if (!CanSendToYoutubeDL(fn) || !ProcessYoutubeDLURL(fn, true)) {
                            CAtlList<CString> sl2;
                            sl2.AddHead(fn);
                            m_frame.m_wndPlaylistBar.Append(sl2, false, &s.slSubs);
                        }
                    }
                }

                applyRandomizeSwitch();

                if (s.nCLSwitches & (CLSW_OPEN | CLSW_PLAY)) {
                    m_frame.m_wndPlaylistBar.SetLast();
                    if ((s.nCLSwitches & CLSW_STARTVALID) && s.rtStart > 0 || s.abRepeat) {
                        m_frame.m_reloadFilename = m_frame.m_wndPlaylistBar.GetCurFileName();
                        m_frame.m_rtReloadPos = s.rtStart;
                        m_frame.reloadABRepeat = s.abRepeat;
                        m_frame.m_iReloadAudioIdx = -1;
                        m_frame.m_iReloadSubIdx = -1;
                    }
                    m_frame.PostMessage(WM_MPC_OPENCURPLAYLIST, 0, 0);
                }

                // Done last so that it does not interfere with the item selected above. Sorting only moves
                // list nodes around, so the playlist position stays on the same item.
                if (bSameSelection && !bRandomize) {
                    m_frame.m_wndPlaylistBar.SortByPathFrom(m_nLastAppendSelectionIndex);
                }
            } else {
                fSetForegroundWindow = true;
                // The playlist gets replaced below and its first item starts playing. The rest of
                // the selection is sorted in under it, so the playing item stays on top: sorting
                // from 0 moved it into the middle and everything sorted before it never played.
                m_nLastAppendSelectionIndex = 1;

                if (m_frame.GetMediaState() == State_Running) {
                    m_frame.MediaControlPause(true);
                }

                if (fMulti || sl.GetCount() == 1) {
                    bool first = true;
                    POSITION pos2 = sl.GetHeadPosition();
                    while (pos2) {
                        CString fn = sl.GetNext(pos2);
                        if (!CanSendToYoutubeDL(fn) || !ProcessYoutubeDLURL(fn, !first, false)) {
                            CAtlList<CString> sl2;
                            sl2.AddHead(fn);
                            if (first) {
                                m_frame.m_wndPlaylistBar.Open(sl2, false, &s.slSubs);
                            } else {
                                m_frame.m_wndPlaylistBar.Append(sl2, false, &s.slSubs);
                            }
                        }
                        first = false;
                    }
                } else {
                    // video + dub
                    m_frame.m_wndPlaylistBar.Open(sl, false, &s.slSubs);
                }

                applyRandomizeSwitch();
                if (sl.GetCount() != 1 || !m_frame.IsPlaylistFile(sl.GetHead())) { //playlists already set first pos (or saved pos)
                    m_frame.m_wndPlaylistBar.SetFirst();
                }

                if ((s.nCLSwitches & CLSW_STARTVALID) && s.rtStart > 0 || s.abRepeat) {
                    m_frame.m_reloadFilename = m_frame.m_wndPlaylistBar.GetCurFileName();
                    m_frame.m_rtReloadPos = s.rtStart;
                    m_frame.reloadABRepeat = s.abRepeat;
                    m_frame.m_iReloadAudioIdx = -1;
                    m_frame.m_iReloadSubIdx = -1;
                }
                m_frame.PostMessage(WM_MPC_OPENCURPLAYLIST, 0, 0);

                s.nCLSwitches &= ~CLSW_STARTVALID;
                s.rtStart = 0;
            }
            s.nCLSwitches &= ~CLSW_ADD;
        }
    } else if ((s.nCLSwitches & CLSW_PLAY) && !m_frame.IsPlaylistEmpty()) {
        if ((s.nCLSwitches & CLSW_STARTVALID) && s.rtStart > 0 || s.abRepeat) {
            m_frame.m_reloadFilename = m_frame.m_wndPlaylistBar.GetCurFileName();
            m_frame.m_rtReloadPos = s.rtStart;
            m_frame.reloadABRepeat = s.abRepeat;
            m_frame.m_iReloadAudioIdx = -1;
            m_frame.m_iReloadSubIdx = -1;
        }
        m_frame.PostMessage(WM_MPC_OPENCURPLAYLIST, 0, 0);
    } else {
        applyRandomizeSwitch();
    }

    if (s.nCLSwitches & CLSW_PRESET1) {
        m_frame.SendMessage(WM_COMMAND, ID_VIEW_PRESETS_MINIMAL);
        s.nCLSwitches &= ~CLSW_PRESET1;
    } else if (s.nCLSwitches & CLSW_PRESET2) {
        m_frame.SendMessage(WM_COMMAND, ID_VIEW_PRESETS_COMPACT);
        s.nCLSwitches &= ~CLSW_PRESET2;
    } else if (s.nCLSwitches & CLSW_PRESET3) {
        m_frame.SendMessage(WM_COMMAND, ID_VIEW_PRESETS_NORMAL);
        s.nCLSwitches &= ~CLSW_PRESET3;
    } else if (s.nCLSwitches & CLSW_PRESET4) {
        m_frame.SendMessage(WM_COMMAND, ID_VIEW_PRESETS_CUSTOM);
        s.nCLSwitches &= ~CLSW_PRESET4;
    }
    if (s.nCLSwitches & CLSW_VOLUME) {
        if (m_frame.IsMuted()) {
            m_frame.SendMessage(WM_COMMAND, ID_VOLUME_MUTE);
        }
        m_frame.m_wndToolBar.SetVolume(s.nCmdVolume);
        s.nCLSwitches &= ~CLSW_VOLUME;
    }
    if (s.nCLSwitches & CLSW_MUTE) {
        if (!m_frame.IsMuted()) {
            m_frame.SendMessage(WM_COMMAND, ID_VOLUME_MUTE);
        }
        s.nCLSwitches &= ~CLSW_MUTE;
    }

    if (fSetForegroundWindow && !(s.nCLSwitches & CLSW_NOFOCUS)) {
        m_frame.SetForegroundWindow();
    }
}

static const CString ydl_whitelist[] = {
    _T("youtube.com/"),
    _T("youtu.be/"),
    _T("twitch.tv/"),
    _T("twitch.com/"),
    _T("instagram.com/"),
    _T("facebook.com/"),
    _T("tiktok.com/"),
    _T("vimeo.com/"),
    _T("dailymotion.com/"),
    _T("crunchyroll.com/"),
    _T("bbc.co.uk/"),
    _T("pornhub.com/"),
    _T("xvideos.com/"),
    _T("xhamster.com/"),
    _T("youporn.com/"),
    _T("tnaflix.com/"),
};

static const CString ydl_blacklist[] = {
    _T("googlevideo.com/videoplayback"), // already processed URL
    _T("googlevideo.com/api/manifest"),
    _T("@127.0.0.1:"), // local URL
    _T("saunalahti.fi/")
};

bool CCommandLine::IsOnYDLWhitelist(CString url) {
    for (int i = 0; i < _countof(ydl_whitelist); i++) {
        if (url.Find(ydl_whitelist[i]) >= 0) {
            return true;
        }
    }
    return false;
}

bool CCommandLine::CanSendToYoutubeDL(const CString url)
{
    if (url.Left(4).MakeLower() == _T("http")) {
        auto& s = AfxGetAppSettings();
        if (!s.bUseYDL) {
            return false;
        }

        // Blacklist: don't use for IP addresses
        std::wcmatch regmatch;
        std::wregex regexp(LR"(https?:\/\/(\d{1,3}\.){3}\d{1,3}.*)");
        if (std::regex_match(url.GetString(), regmatch, regexp)) {
            return false;
        }

        // Whitelist: popular supported sites
        if (IsOnYDLWhitelist(url)) {
            return true;
        }

        // Blacklist: unsupported sites where YDL causes an error or long delay
        for (int i = 0; i < _countof(ydl_blacklist); i++) {
            if (url.Find(ydl_blacklist[i], 7) > 0) {
                return false;
            }
        }

        // Blacklist: URL points to a file
        CString baseurl;
        int q = url.Find(_T('?'));
        if (q > 0) {
            baseurl = url.Left(q);
        } else {
            baseurl = url;
            q = url.GetLength();
        }
        int p = baseurl.ReverseFind(_T('.'));
        if (p > 0 && (q - p <= 6)) {
            CString ext = baseurl.Mid(p);
            if (ext == L".m3u8" || ext == L".mpd") {
                return false;
            }
            if (s.m_Formats.FindExt(ext)) {
                return false;
            }
        }

        return true;
    }
    return false;
}

bool CCommandLine::ProcessYoutubeDLURL(CString url, bool append, bool replace)
{
    auto& s = AfxGetAppSettings();
    CAtlList<CYoutubeDLInstance::YDLStreamURL> streams;
    CAtlList<CString> filenames;
    CYoutubeDLInstance ydl;
    CYoutubeDLInstance::YDLPlaylistInfo listinfo;
    CString useragent;

    m_sydlLastProcessURL = url;

    m_frame.m_wndStatusBar.SetStatusMessage(ResStr(IDS_CONTROLS_YOUTUBEDL));

    {
        CMainFrame::CDeferredActionScope actionScope(m_frame);
        if (!ydl.Run(url)) {
            return false;
        }
    }
    if (!ydl.GetHttpStreams(streams, listinfo, useragent)) {
        return false;
    }

    if (!append && !replace) {
        m_frame.m_wndPlaylistBar.Empty();
    }

    CString f_title;

    for (unsigned int i = 0; i < streams.GetCount(); i++) {
        auto stream = streams.GetAt(streams.FindIndex(i));
        CString v_url = stream.video_url;
        CString a_url = stream.audio_url;
        filenames.RemoveAll();
        if (!v_url.IsEmpty() && (!s.bYDLAudioOnly || a_url.IsEmpty())) {
            filenames.AddTail(v_url);

        }
        if (!a_url.IsEmpty()) {
            filenames.AddTail(a_url);
        }
        CString title = stream.title;
        CString seasonid;
        if (stream.season_number != -1) {
            seasonid.Format(_T("S%02d"), stream.season_number);
        }
        CString episodeid;
        if (stream.episode_number != -1) {
            episodeid.Format(_T("E%02d"), stream.episode_number);
        }
        CString epiid;
        if (!seasonid.IsEmpty() || !episodeid.IsEmpty()) {
            epiid.Format(_T("%s%s. "), static_cast<LPCWSTR>(seasonid), static_cast<LPCWSTR>(episodeid));
        }
        CString season;
        if (!stream.series.IsEmpty()) {
            season = stream.series;
            CString t(stream.season.Left(6));
            if (!stream.season.IsEmpty() && (t.MakeLower() != _T("season") || stream.season_number == -1)) {
                season += _T(" ") + stream.season;
            }
            season += _T(" - ");
        }
        else if (!stream.season.IsEmpty()) {
            CString t(stream.season.Left(6));
            if (t.MakeLower() != _T("season") || stream.season_number == -1) {
                season = stream.season + _T(" - ");
            }
        }
        title.Format(_T("%s%s%s"), static_cast<LPCWSTR>(epiid), static_cast<LPCWSTR>(season), static_cast<LPCWSTR>(title));
        if (i == 0) f_title = title;

        CString ydl_src = stream.webpage_url.IsEmpty() ? url : stream.webpage_url;
        if (i == 0) m_sydlLastProcessURL = ydl_src;

        int targetlen = title.GetLength() > 100 ? 50 : 150 - title.GetLength();
        CString short_url = ShortenURL(ydl_src, targetlen, true);
        
        if (ydl_src == filenames.GetHead()) {
            // Processed URL is same as input, can happen for DASH manifest files. Clear source URL to avoid reprocessing.
            ydl_src = _T("");
        }

        if (replace) {
            m_frame.m_wndPlaylistBar.ReplaceCurrentItem(filenames, nullptr, title + " (" + short_url + ")", ydl_src, useragent, _T(""), &stream.subtitles);
            break;
        } else {
            m_frame.m_wndPlaylistBar.Append(filenames, false, nullptr, title + " (" + short_url + ")", ydl_src, useragent, _T(""), &stream.subtitles);
        }
    }

    if (s.fKeepHistory) {
        auto* mru = &s.MRU;
        RecentFileEntry r;
        mru->LoadMediaHistoryEntryFN(m_sydlLastProcessURL, r);
        if (streams.GetCount() > 1) {
            auto h = streams.GetHead();
            if (!h.series.IsEmpty()) {
                r.title = h.series;
                if (!h.season.IsEmpty()) {
                    r.title += _T(" - ") + h.season;
                }
            }
            else if (!h.season.IsEmpty()) {
                r.title = h.season;
            }
            else if (!listinfo.title.IsEmpty()) {
                if (!listinfo.uploader.IsEmpty()) r.title.Format(_T("%s - %s"), static_cast<LPCWSTR>(listinfo.uploader), static_cast<LPCWSTR>(listinfo.title));
                else r.title = listinfo.title;
            }
            else r.title = f_title;
        }
        else if (streams.GetCount() == 1) {
            r.title = f_title;
        }
        mru->Add(r, false);
    }

    if (!append && (!replace || m_frame.m_wndPlaylistBar.GetCount() == 0)) {
        m_frame.m_wndPlaylistBar.SetFirst();
    }
    return true;
}

bool CCommandLine::DownloadWithYoutubeDL(CString url, CString filename)
{
    PROCESS_INFORMATION proc_info;
    STARTUPINFO startup_info;
    const auto& s = AfxGetAppSettings();

    bool ytdlp = true;
    CString args = _T("\"") + GetYDLExePath(&ytdlp) + _T("\" --console-title \"") + url + _T("\"");
    if (!s.sYDLCommandLine.IsEmpty()) {
        args.Append(_T(" "));
        args.Append(s.sYDLCommandLine);
    }
    if (s.bYDLAudioOnly && (s.sYDLCommandLine.Find(_T("-f ")) < 0)) {
        args.Append(_T(" -f bestaudio"));
    }
    if (s.sYDLCommandLine.Find(_T("-o ")) < 0) {
        args.Append(_T(" -o \"" + filename + "\""));
    }

    ZeroMemory(&proc_info, sizeof(PROCESS_INFORMATION));
    ZeroMemory(&startup_info, sizeof(STARTUPINFO));
    startup_info.cb = sizeof(STARTUPINFO);

    if (!CreateProcess(NULL, args.GetBuffer(), NULL, NULL, false, 0,
                       NULL, NULL, &startup_info, &proc_info)) {
        AfxMessageBox(_T("An error occurred while attempting to run yt-dlp/youtube-dl"), MB_ICONERROR, 0);
        return false;
    }

    CloseHandle(proc_info.hProcess);
    CloseHandle(proc_info.hThread);

    return true;
}
