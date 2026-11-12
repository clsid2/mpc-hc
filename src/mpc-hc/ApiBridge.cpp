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
#include "ApiBridge.h"
#include "MainFrm.h"
#include "mplayerc.h"
#include "CoverArt.h"
#include "WebServer.h"
#include "Logger.h"
#include <PathUtils.h>
#include <DSUtil.h>
#include <strsafe.h>

static UINT WM_MPCAPI_INT = RegisterWindowMessage(MPCAPI_INT_MESSAGE_NAME);

CApiBridge::CApiBridge(CMainFrame& frame)
    : m_frame(frame)
    , m_graph(frame.Graph())
{
}

CApiBridge::~CApiBridge()
{
}

void CApiBridge::OnApiPause()
{
    OAFilterState fs = m_frame.GetMediaState();
    if (fs == State_Running) {
        m_frame.PostMessage(WM_COMMAND, ID_PLAY_PAUSE);
    }
}

void CApiBridge::OnApiPlay()
{
    OAFilterState fs = m_frame.GetMediaState();
    if (fs == State_Stopped || fs == State_Paused) {
        m_frame.PostMessage(WM_COMMAND, ID_PLAY_PLAY);
    }
}

void CApiBridge::StartWebServer(int nPort)
{
    if (!m_pWebServer) {
        m_pWebServer.Attach(DEBUG_NEW CWebServer(&m_frame, nPort));
    }
}

void CApiBridge::StopWebServer()
{
    if (m_pWebServer) {
        m_pWebServer.Free();
    }
}

static bool ParseAPIStatusMessage(const COPYDATASTRUCT* pCDS, CStringW& message)
{
    constexpr size_t maxCodeUnits = 512;
    if (!pCDS || !pCDS->lpData || pCDS->cbData < 2 * sizeof(wchar_t)
            || pCDS->cbData > (maxCodeUnits + 1) * sizeof(wchar_t)
            || pCDS->cbData % sizeof(wchar_t) != 0) {
        return false;
    }

    const wchar_t* const value = static_cast<const wchar_t*>(pCDS->lpData);
    const size_t codeUnits = pCDS->cbData / sizeof(wchar_t) - 1;
    if (value[codeUnits] != L'\0' || wmemchr(value, L'\0', codeUnits)) {
        return false;
    }

    for (size_t i = 0; i < codeUnits; i++) {
        const wchar_t codeUnit = value[i];
        if (codeUnit >= 0xD800 && codeUnit <= 0xDBFF) {
            if (++i >= codeUnits || value[i] < 0xDC00 || value[i] > 0xDFFF) {
                return false;
            }
        } else if ((codeUnit >= 0xDC00 && codeUnit <= 0xDFFF)
                   || codeUnit < 0x20
                   || (codeUnit >= 0x7F && codeUnit <= 0x9F)
                   || codeUnit == 0x2028 || codeUnit == 0x2029) {
            return false;
        }
    }

    message.SetString(value, static_cast<int>(codeUnits));
    return true;
}

static bool ParseAPIInteger(const COPYDATASTRUCT* pCDS, int minimum, int maximum, int& result)
{
    if (!pCDS || !pCDS->lpData || pCDS->cbData < sizeof(wchar_t)
            || pCDS->cbData % sizeof(wchar_t) != 0) {
        return false;
    }

    const wchar_t* const value = static_cast<const wchar_t*>(pCDS->lpData);
    const size_t length = pCDS->cbData / sizeof(wchar_t);
    if (length > 4 || value[length - 1] != L'\0'
            || wmemchr(value, L'\0', length - 1)
            || value[0] < L'0' || value[0] > L'9') {
        return false;
    }

    wchar_t* end = nullptr;
    const long parsed = wcstol(value, &end, 10);
    if (end == value || *end != L'\0' || parsed < minimum || parsed > maximum) {
        return false;
    }

    result = static_cast<int>(parsed);
    return true;
}

void CApiBridge::ProcessAPICommand(HWND hSender, COPYDATASTRUCT* pCDS)
{
    CAtlList<CString> fns;
    REFERENCE_TIME rtPos = 0;
    CString fn;

    if (USE_LOGGER(AfxGetAppSettings())) {
        PLAYER_LOG(_T("CMainFrame::ProcessAPICommand - CMD=%lu"), pCDS->dwData);
    }

    switch (pCDS->dwData) {
        case CMD_OPENFILE:
            fn = CString((LPCWSTR)pCDS->lpData);
            if (m_frame.GetMediaState() == State_Running) {
                m_frame.MediaControlPause(true);
            }
            if (m_frame.CanSendToYoutubeDL(fn)) {
                if (m_frame.ProcessYoutubeDLURL(fn, false)) {
                    m_frame.PostMessage(WM_MPC_OPENCURPLAYLIST, 0, 0);
                    return;
                } else if (m_frame.IsOnYDLWhitelist(fn)) {
                    m_frame.m_closingmsg = L"Failed to extract stream URL with yt-dlp/youtube-dl";
                    m_frame.m_wndStatusBar.SetStatusMessage(m_frame.m_closingmsg);
                    return;
                }
            }
            fns.AddHead(fn);
            m_frame.m_wndPlaylistBar.Open(fns, false);
            m_frame.PostMessage(WM_MPC_OPENCURPLAYLIST, 0, 0);
            break;
        case CMD_STOP:
            m_frame.OnPlayStop();
            break;
        case CMD_CLOSEFILE:
            m_frame.PostMessage(WM_COMMAND, ID_FILE_CLOSEMEDIA);
            break;
        case CMD_PLAYPAUSE:
            m_frame.OnPlayPlaypause();
            break;
        case CMD_PLAY:
            OnApiPlay();
            break;
        case CMD_PAUSE:
            OnApiPause();
            break;
        case CMD_ADDTOPLAYLIST:
            fn = CString((LPCWSTR)pCDS->lpData);
            if (m_frame.CanSendToYoutubeDL(fn)) {
                if (m_frame.ProcessYoutubeDLURL(fn, true)) {
                    return;
                } else if (m_frame.IsOnYDLWhitelist(fn)) {
                    m_frame.m_closingmsg = L"Failed to extract stream URL with yt-dlp/youtube-dl";
                    m_frame.m_wndStatusBar.SetStatusMessage(m_frame.m_closingmsg);
                    return;
                }
            }
            fns.AddHead(fn);
            m_frame.m_wndPlaylistBar.Append(fns, true);
            break;
        case CMD_STARTPLAYLIST:
            if (m_frame.GetMediaState() == State_Running) {
                m_frame.MediaControlPause(true);
            }
            m_frame.PostMessage(WM_MPC_OPENCURPLAYLIST, 0, 0);
            break;
        case CMD_CLEARPLAYLIST:
            m_frame.m_wndPlaylistBar.Empty();
            break;
        case CMD_SETPOSITION:
            if (m_frame.GetLoadState() == MLS::LOADED) {
                rtPos = 10000 * REFERENCE_TIME(_wtof((LPCWSTR)pCDS->lpData) * 1000); //with accuracy of 1 ms
                // imianz: quick and dirty trick
                // Pause->SeekTo->Play (in place of SeekTo only) seems to prevents in most cases
                // some strange video effects on avi files (ex. locks a while and than running fast).
                if (!m_frame.m_fAudioOnly && m_frame.GetMediaState() == State_Running) {
                    m_frame.MediaControlPause(true);
                    m_frame.SeekTo(rtPos);
                    m_frame.MediaControlRun(true);
                } else {
                    m_frame.SeekTo(rtPos);
                }
                // show current position overridden by play command
                m_frame.m_OSD.DisplayMessage(OSD_TOPLEFT, m_frame.m_wndStatusBar.GetStatusTimer(), 2000);
            }
            break;
        case CMD_SETVOLUME: {
            int volume;
            if (hSender == AfxGetAppSettings().hMasterWnd
                    && ParseAPIInteger(pCDS, 0, 100, volume) && volume != m_frame.GetVolume()) {
                // SetVolume posts the canonical volume-change notification.
                m_frame.m_wndToolBar.SetVolume(volume);
            }
            break;
        }
        case CMD_SETMUTE: {
            int mute;
            if (hSender == AfxGetAppSettings().hMasterWnd
                    && ParseAPIInteger(pCDS, 0, 1, mute) && (mute != 0) != m_frame.IsMuted()) {
                m_frame.m_wndToolBar.SetMute(mute != 0);
                m_frame.OnPlayVolume(ID_VOLUME_MUTE);
            }
            break;
        }
        case CMD_SETAUDIODELAY:
            rtPos = (REFERENCE_TIME)_wtol((LPCWSTR)pCDS->lpData) * 10000;
            m_frame.SetAudioDelay(rtPos);
            break;
        case CMD_SETSUBTITLEDELAY:
            m_frame.m_subs.SetSubtitleDelay(_wtoi((LPCWSTR)pCDS->lpData));
            break;
        case CMD_SETINDEXPLAYLIST:
            //m_frame.m_wndPlaylistBar.SetSelIdx(_wtoi((LPCWSTR)pCDS->lpData));
            break;
        case CMD_SETAUDIOTRACK:
            m_frame.SetAudioTrackIdx(_wtoi((LPCWSTR)pCDS->lpData));
            break;
        case CMD_SETSUBTITLETRACK:
            m_frame.m_subs.SetSubtitleTrackIdx(_wtoi((LPCWSTR)pCDS->lpData));
            break;
        case CMD_GETVERSION: {
            CStringW buff = AfxGetMyApp()->m_strVersion;
            SendAPICommand(CMD_VERSION, buff);
            break;
        }
        case CMD_GETSUBTITLETRACKS:
            m_frame.m_subs.SendSubtitleTracksToApi();
            break;
        case CMD_GETAUDIOTRACKS:
            SendAudioTracksToApi();
            break;
        case CMD_GETCURRENTAUDIOTRACK:
            SendApiNotify(CMD_CURRENTAUDIOTRACK, m_frame.GetCurrentAudioTrackIdx());
            break;
        case CMD_GETCURRENTSUBTITLETRACK:
            SendApiNotify(CMD_CURRENTSUBTITLETRACK, m_frame.m_subs.GetCurrentSubtitleTrackIdx());
            break;
        case CMD_GETCURRENTPOSITION:
            SendCurrentPositionToApi();
            break;
        case CMD_GETVOLUME:
            SendCurrentVolumeToApi(true);
            break;
        case CMD_GETMUTE:
            SendCurrentMuteToApi(true);
            break;
        case CMD_GETNOWPLAYING:
            SendNowPlayingToApi();
            break;
        case CMD_JUMPOFNSECONDS:
            m_frame.JumpOfNSeconds(_wtoi((LPCWSTR)pCDS->lpData));
            break;
        case CMD_GETPLAYLIST:
            SendPlaylistToApi();
            break;
        case CMD_JUMPFORWARDMED:
            m_frame.OnPlaySeek(ID_PLAY_SEEKFORWARDMED);
            break;
        case CMD_JUMPBACKWARDMED:
            m_frame.OnPlaySeek(ID_PLAY_SEEKBACKWARDMED);
            break;
        case CMD_TOGGLEFULLSCREEN:
            m_frame.OnViewFullscreen();
            break;
        case CMD_INCREASEVOLUME:
            m_frame.m_wndToolBar.m_volctrl.IncreaseVolume();
            break;
        case CMD_DECREASEVOLUME:
            m_frame.m_wndToolBar.m_volctrl.DecreaseVolume();
            break;
		case CMD_SHADER_TOGGLE :
			m_frame.OnShaderToggle1();
			break;
        case CMD_CLOSEAPP:
            m_frame.PostMessage(WM_CLOSE);
            break;
        case CMD_SETSPEED:
            m_frame.SetPlayingRate(_wtof((LPCWSTR)pCDS->lpData));
            break;
        case CMD_SETPANSCAN:
            AfxGetAppSettings().strPnSPreset = (LPCWSTR)pCDS->lpData;
            m_frame.m_geometry.ApplyPanNScanPresetString();
            break;
        case CMD_OSDSHOWMESSAGE:
            if (pCDS->lpData && pCDS->cbData >= sizeof(MPC_OSDDATA)) {
                ShowOSDCustomMessageApi((MPC_OSDDATA*)pCDS->lpData);
            }
            break;
        case CMD_STATUSSHOWMESSAGE: {
            CStringW message;
            const CAppSettings& settings = AfxGetAppSettings();
            if (hSender == settings.hMasterWnd && IsWindow(hSender)
                    && ParseAPIStatusMessage(pCDS, message)) {
                m_frame.SendStatusMessage(message, 3000, true, true);
            }
            break;
        }
    }
}

bool CApiBridge::SendAPIStringTo(HWND hTarget, MPCAPI_COMMAND nCommand, const CStringW& payload)
{
    COPYDATASTRUCT data = {};
    data.cbData = static_cast<DWORD>((payload.GetLength() + 1) * sizeof(wchar_t));
    data.dwData = nCommand;
    data.lpData = const_cast<wchar_t*>(payload.GetString());

    DWORD_PTR result = 0;
    return SendMessageTimeout(hTarget, WM_COPYDATA, reinterpret_cast<WPARAM>(m_frame.GetSafeHwnd()),
                              reinterpret_cast<LPARAM>(&data),
                              // fire-and-forget; the timeout keeps a slow target from stalling our UI
                              // thread, but is generous enough that a merely busy target still gets it
                              SMTO_ABORTIFHUNG | SMTO_ERRORONEXIT, 500, &result) != 0;
}

void CApiBridge::SendAPICommand(MPCAPI_COMMAND nCommand, LPCWSTR fmt, ...)
{
    const CAppSettings& s = AfxGetAppSettings();

    if (s.hMasterWnd) {
        COPYDATASTRUCT CDS;

        va_list args;
        va_start(args, fmt);

        int nBufferLen = _vsctprintf(fmt, args) + 1; // _vsctprintf doesn't count the null terminator
        TCHAR* pBuff = DEBUG_NEW TCHAR[nBufferLen];
        _vstprintf_s(pBuff, nBufferLen, fmt, args);

        CDS.cbData = (DWORD)nBufferLen * sizeof(TCHAR);
        CDS.dwData = nCommand;
        CDS.lpData = (LPVOID)pBuff;

        ::SendMessage(s.hMasterWnd, WM_COPYDATA, (WPARAM)m_frame.GetSafeHwnd(), (LPARAM)&CDS);

        va_end(args);
        delete [] pBuff;
    }
}

void CApiBridge::SendNowPlayingToApi(bool sendtrackinfo)
{
    if (!AfxGetAppSettings().hMasterWnd) {
        return;
    }

    if (m_frame.GetLoadState() == MLS::LOADED) {
        CString title, author, description;
        CString label;
        CString strDur;

        if (m_frame.GetPlaybackMode() == PM_FILE) {
            m_frame.m_wndInfoBar.GetLine(StrRes(IDS_INFOBAR_TITLE), title);
            m_frame.m_wndInfoBar.GetLine(StrRes(IDS_INFOBAR_AUTHOR), author);
            m_frame.m_wndInfoBar.GetLine(StrRes(IDS_INFOBAR_DESCRIPTION), description);

            CPlaylistItem pli = CPlaylistItem(true);
            if (m_frame.m_wndPlaylistBar.GetCur(pli, true)) {
                label = !pli.m_label.IsEmpty() ? pli.m_label : pli.m_fns.GetHead();
                REFERENCE_TIME rtDur;
                m_graph.m_pMS->GetDuration(&rtDur);
                strDur.Format(L"%.3f", rtDur / 10000000.0);
            }
        } else if (m_frame.GetPlaybackMode() == PM_DVD) {
            DVD_DOMAIN DVDDomain;
            ULONG ulNumOfChapters = 0;
            DVD_PLAYBACK_LOCATION2 Location;

            // Get current DVD Domain
            if (SUCCEEDED(m_graph.m_pDVDI->GetCurrentDomain(&DVDDomain))) {
                switch (DVDDomain) {
                    case DVD_DOMAIN_Stop:
                        title = _T("DVD - Stopped");
                        break;
                    case DVD_DOMAIN_FirstPlay:
                        title = _T("DVD - FirstPlay");
                        break;
                    case DVD_DOMAIN_VideoManagerMenu:
                        title = _T("DVD - RootMenu");
                        break;
                    case DVD_DOMAIN_VideoTitleSetMenu:
                        title = _T("DVD - TitleMenu");
                        break;
                    case DVD_DOMAIN_Title:
                        title = _T("DVD - Title");
                        break;
                }

                // get title information
                if (DVDDomain == DVD_DOMAIN_Title) {
                    // get current location (title number & chapter)
                    if (SUCCEEDED(m_graph.m_pDVDI->GetCurrentLocation(&Location))) {
                        // get number of chapters in current title
                        VERIFY(SUCCEEDED(m_graph.m_pDVDI->GetNumberOfChapters(Location.TitleNum, &ulNumOfChapters)));
                    }

                    // get total time of title
                    DVD_HMSF_TIMECODE tcDur;
                    ULONG ulFlags;
                    if (SUCCEEDED(m_graph.m_pDVDI->GetTotalTitleTime(&tcDur, &ulFlags))) {
                        // calculate duration in seconds
                        strDur.Format(L"%d", tcDur.bHours * 60 * 60 + tcDur.bMinutes * 60 + tcDur.bSeconds);
                    }

                    // build string
                    // DVD - xxxxx|currenttitle|numberofchapters|currentchapter|titleduration
                    author.Format(L"%lu", Location.TitleNum);
                    description.Format(L"%lu", ulNumOfChapters);
                    label.Format(L"%lu", Location.ChapterNum);
                }
            }
        }

        title.Replace(L"|", L"\\|");
        author.Replace(L"|", L"\\|");
        description.Replace(L"|", L"\\|");
        label.Replace(L"|", L"\\|");

        CStringW buff;
        buff.Format(L"%s|%s|%s|%s|%s", title.GetString(), author.GetString(), description.GetString(), label.GetString(), strDur.GetString());

        SendAPICommand(CMD_NOWPLAYING, L"%s", static_cast<LPCWSTR>(buff));
        if (sendtrackinfo) {
            m_frame.m_subs.SendSubtitleTracksToApi();
            SendAudioTracksToApi();
        }
    }
}

void CApiBridge::SendAudioTracksToApi()
{
    CStringW strAudios;

    if (m_frame.GetLoadState() == MLS::LOADED) {
        DWORD cStreams = 0;
        if (m_graph.m_pAudioSwitcherSS && SUCCEEDED(m_graph.m_pAudioSwitcherSS->Count(&cStreams))) {
            int currentStream = -1;
            for (int i = 0; i < (int)cStreams; i++) {
                AM_MEDIA_TYPE* pmt = nullptr;
                DWORD dwFlags = 0;
                LCID lcid = 0;
                DWORD dwGroup = 0;
                WCHAR* pszName = nullptr;
                if (FAILED(m_graph.m_pAudioSwitcherSS->Info(i, &pmt, &dwFlags, &lcid, &dwGroup, &pszName, nullptr, nullptr))) {
                    return;
                }
                if (dwFlags & (AMSTREAMSELECTINFO_ENABLED | AMSTREAMSELECTINFO_EXCLUSIVE)) {
                    currentStream = i;
                }
                CString name(pszName);
                if (!strAudios.IsEmpty()) {
                    strAudios.Append(L"|");
                }
                name.Replace(L"|", L"\\|");
                strAudios.AppendFormat(L"%s", name.GetString());
                if (pmt) {
                    DeleteMediaType(pmt);
                }
                if (pszName) {
                    CoTaskMemFree(pszName);
                }
            }
            strAudios.AppendFormat(L"|%d", currentStream);

        } else {
            strAudios.Append(L"-1");
        }
    } else {
        strAudios.Append(L"-2");
    }
    SendAPICommand(CMD_LISTAUDIOTRACKS, L"%s", static_cast<LPCWSTR>(strAudios));

}

void CApiBridge::SendPlaylistToApi()
{
    CStringW strPlaylist;
    POSITION pos = m_frame.m_wndPlaylistBar.m_pl.GetHeadPosition(), pos2;

    while (pos) {
        CPlaylistItem& pli = m_frame.m_wndPlaylistBar.m_pl.GetNext(pos);

        if (pli.m_type == CPlaylistItem::file) {
            pos2 = pli.m_fns.GetHeadPosition();
            while (pos2) {
                CString fn = pli.m_fns.GetNext(pos2);
                if (!strPlaylist.IsEmpty()) {
                    strPlaylist.Append(L"|");
                }
                fn.Replace(L"|", L"\\|");
                strPlaylist.AppendFormat(L"%s", fn.GetString());
            }
        }
    }
    if (strPlaylist.IsEmpty()) {
        strPlaylist.Append(L"-1");
    } else {
        strPlaylist.AppendFormat(L"|%d", m_frame.m_wndPlaylistBar.GetSelIdx());
    }
    SendAPICommand(CMD_PLAYLIST, L"%s", static_cast<LPCWSTR>(strPlaylist));
}

void CApiBridge::SendCurrentPositionToApi(bool fNotifySeek)
{
    if (!AfxGetAppSettings().hMasterWnd) {
        return;
    }

    if (m_frame.GetLoadState() == MLS::LOADED) {
        CStringW strPos;

        if (m_frame.GetPlaybackMode() == PM_FILE) {
            REFERENCE_TIME rtCur;
            m_graph.m_pMS->GetCurrentPosition(&rtCur);
            strPos.Format(L"%.3f", rtCur / 10000000.0);
        } else if (m_frame.GetPlaybackMode() == PM_DVD) {
            DVD_PLAYBACK_LOCATION2 Location;
            // get current location while playing disc, will return 0, if at a menu
            if (m_graph.m_pDVDI->GetCurrentLocation(&Location) == S_OK) {
                strPos.Format(L"%d", Location.TimeCode.bHours * 60 * 60 + Location.TimeCode.bMinutes * 60 + Location.TimeCode.bSeconds);
            }
        }

        SendAPICommand(fNotifySeek ? CMD_NOTIFYSEEK : CMD_CURRENTPOSITION, strPos);
    }
}

void CApiBridge::SendCurrentVolumeToApi(bool force)
{
    if (!AfxGetAppSettings().hMasterWnd) {
        return;
    }

    const int volume = m_frame.GetVolume();
    if (force || volume != m_lastApiVolume) {
        if (m_hostIntApiVersion > 0) {
            // Host speaks the integer channel: deliver non-blocking, no buffer to keep alive.
            PostApiInt(AfxGetAppSettings().hMasterWnd, MPCINT_CURRENTVOLUME, volume);
            m_lastApiVolume = volume;
        } else {
            CStringW payload;
            payload.Format(L"%d", volume);
            // Only latch the value when the host actually received it, so a send that
            // timed out against a busy host is retried on the next change.
            if (SendAPIStringTo(AfxGetAppSettings().hMasterWnd, CMD_CURRENTVOLUME, payload)) {
                m_lastApiVolume = volume;
            }
        }
    }
}

void CApiBridge::SendCurrentMuteToApi(bool force)
{
    if (!AfxGetAppSettings().hMasterWnd) {
        return;
    }

    const int mute = m_frame.IsMuted() ? 1 : 0;
    if (force || mute != m_lastApiMute) {
        if (m_hostIntApiVersion > 0) {
            PostApiInt(AfxGetAppSettings().hMasterWnd, MPCINT_CURRENTMUTE, mute);
            m_lastApiMute = mute;
        } else {
            CStringW payload;
            payload.Format(L"%d", mute);
            if (SendAPIStringTo(AfxGetAppSettings().hMasterWnd, CMD_CURRENTMUTE, payload)) {
                m_lastApiMute = mute;
            }
        }
    }
}

void CApiBridge::PostApiInt(HWND hTarget, WORD command, int value)
{
    if (hTarget && WM_MPCAPI_INT) {
        // Non-blocking: the value travels inside the message, so there is no buffer to
        // keep alive and no need to wait for the target to process it (unlike WM_COPYDATA).
        ::PostMessage(hTarget, WM_MPCAPI_INT, reinterpret_cast<WPARAM>(m_frame.GetSafeHwnd()),
                      MPCAPI_INT_MAKELPARAM(value, command));
    }
}

// Map a host->MPC integer command to its WM_COPYDATA equivalent (0 if not INT-able).
static MPCAPI_COMMAND IntCommandToApi(WORD intCmd)
{
    switch (intCmd) {
        case MPCINT_SETVOLUME:              return CMD_SETVOLUME;
        case MPCINT_SETMUTE:                return CMD_SETMUTE;
        case MPCINT_GETVOLUME:              return CMD_GETVOLUME;
        case MPCINT_GETMUTE:                return CMD_GETMUTE;
        case MPCINT_STOP:                   return CMD_STOP;
        case MPCINT_CLOSEFILE:              return CMD_CLOSEFILE;
        case MPCINT_PLAYPAUSE:              return CMD_PLAYPAUSE;
        case MPCINT_PLAY:                   return CMD_PLAY;
        case MPCINT_PAUSE:                  return CMD_PAUSE;
        case MPCINT_CLEARPLAYLIST:          return CMD_CLEARPLAYLIST;
        case MPCINT_STARTPLAYLIST:          return CMD_STARTPLAYLIST;
        case MPCINT_TOGGLEFULLSCREEN:       return CMD_TOGGLEFULLSCREEN;
        case MPCINT_JUMPFORWARDMED:         return CMD_JUMPFORWARDMED;
        case MPCINT_JUMPBACKWARDMED:        return CMD_JUMPBACKWARDMED;
        case MPCINT_INCREASEVOLUME:         return CMD_INCREASEVOLUME;
        case MPCINT_DECREASEVOLUME:         return CMD_DECREASEVOLUME;
        case MPCINT_SHADER_TOGGLE:          return CMD_SHADER_TOGGLE;
        case MPCINT_CLOSEAPP:               return CMD_CLOSEAPP;
        case MPCINT_SETAUDIOTRACK:          return CMD_SETAUDIOTRACK;
        case MPCINT_SETSUBTITLETRACK:       return CMD_SETSUBTITLETRACK;
        case MPCINT_JUMPOFNSECONDS:         return CMD_JUMPOFNSECONDS;
        case MPCINT_SETAUDIODELAY:          return CMD_SETAUDIODELAY;
        case MPCINT_SETSUBTITLEDELAY:       return CMD_SETSUBTITLEDELAY;
        case MPCINT_GETCURRENTAUDIOTRACK:   return CMD_GETCURRENTAUDIOTRACK;
        case MPCINT_GETCURRENTSUBTITLETRACK:return CMD_GETCURRENTSUBTITLETRACK;
        default:                            return static_cast<MPCAPI_COMMAND>(0);
    }
}

// Map a MPC->host notification to its integer-channel command (0 if not INT-able).
static WORD ApiCommandToInt(MPCAPI_COMMAND cmd)
{
    switch (cmd) {
        case CMD_CURRENTVOLUME:        return MPCINT_CURRENTVOLUME;
        case CMD_CURRENTMUTE:          return MPCINT_CURRENTMUTE;
        case CMD_STATE:                return MPCINT_STATE;
        case CMD_PLAYMODE:             return MPCINT_PLAYMODE;
        case CMD_CURRENTAUDIOTRACK:    return MPCINT_CURRENTAUDIOTRACK;
        case CMD_CURRENTSUBTITLETRACK: return MPCINT_CURRENTSUBTITLETRACK;
        default:                       return 0;
    }
}

LRESULT CApiBridge::OnApiIntMessage(WPARAM wParam, LPARAM lParam)
{
    const HWND hSender = reinterpret_cast<HWND>(wParam);
    const WORD command = MPCAPI_INT_COMMAND_OF(lParam);
    const int value = MPCAPI_INT_VALUE_OF(lParam);
    CAppSettings& s = AfxGetAppSettings();

    // Every integer command is honored only from the connected host (same gate as the
    // WM_COPYDATA setters). HELLO establishes that the host speaks this channel.
    if (hSender != s.hMasterWnd) {
        return 0;
    }

    if (command == MPCINT_HELLO) {
        m_hostIntApiVersion = value;
        PostApiInt(hSender, MPCINT_HELLO, MPCAPI_INT_VERSION);
        return 0;
    }

    // Reuse ProcessAPICommand so an integer command shares exactly the same handling and
    // sender gating as its WM_COPYDATA form; the value is passed as the string parameter.
    const MPCAPI_COMMAND cmd = IntCommandToApi(command);
    if (cmd) {
        CStringW payload;
        payload.Format(L"%d", value); // ignored by parameterless commands
        COPYDATASTRUCT cds = {};
        cds.dwData = cmd;
        cds.cbData = static_cast<DWORD>((payload.GetLength() + 1) * sizeof(wchar_t));
        cds.lpData = const_cast<wchar_t*>(payload.GetString());
        ProcessAPICommand(hSender, &cds);
    }
    return 0;
}

void CApiBridge::SendApiNotify(MPCAPI_COMMAND cmd, int value)
{
    const CAppSettings& s = AfxGetAppSettings();
    if (!s.hMasterWnd) {
        return;
    }
    const WORD intCmd = ApiCommandToInt(cmd);
    if (m_hostIntApiVersion > 0 && intCmd) {
        PostApiInt(s.hMasterWnd, intCmd, value);
    } else {
        SendAPICommand(cmd, L"%d", value);
    }
}

void CApiBridge::ShowOSDCustomMessageApi(const MPC_OSDDATA* osdData)
{
    // strMsg is a fixed-size field in a host-supplied buffer; bound the read so a
    // non-terminated field cannot over-read past the struct.
    const CStringW msg(osdData->strMsg, static_cast<int>(wcsnlen(osdData->strMsg, _countof(osdData->strMsg))));
    m_frame.m_OSD.DisplayMessage((OSD_MESSAGEPOS)osdData->nMsgPos, msg, osdData->nDurationMS);
}

HRESULT CApiBridge::CreateThumbnailToolbar()
{
    if (!this || !AfxGetAppSettings().bUseEnhancedTaskBar) {
        return E_FAIL;
    }

    if (!m_pTaskbarList) {
        if (SUCCEEDED(m_pTaskbarList.CoCreateInstance(CLSID_TaskbarList, nullptr, CLSCTX_INPROC_SERVER))) {
            if (FAILED(m_pTaskbarList->HrInit())) {
                m_pTaskbarList.Release();
                AfxGetAppSettings().bUseEnhancedTaskBar = false;
                return E_FAIL;
            }
        }
    }

    if (m_pTaskbarList) {
        bool bRTLLayout = false; // Assume left-to-right layout by default
        // Try to locate the window used to display the task bar
        if (CWnd* pTaskBarWnd = m_frame.FindWindow(_T("TaskListThumbnailWnd"), nullptr)) {
            bRTLLayout = !!(pTaskBarWnd->GetExStyle() & WS_EX_LAYOUTRTL);
        }

        CMPCPngImage image;
        if (!image.Load(IDF_WIN7_TOOLBAR)) {
            return E_FAIL;
        }

        CSize size = image.GetSize();

        if (bRTLLayout) { // We don't want the buttons to be mirrored so we pre-mirror them
            // Create memory DCs for the source and destination bitmaps
            CDC sourceDC, destDC;
            sourceDC.CreateCompatibleDC(nullptr);
            destDC.CreateCompatibleDC(nullptr);
            // Swap the source bitmap with an empty one
            CBitmap sourceImg;
            sourceImg.Attach(image.Detach());
            // Create a temporary DC
            CClientDC clientDC(nullptr);
            // Create the destination bitmap
            image.CreateCompatibleBitmap(&clientDC, size.cx, size.cy);
            // Select the bitmaps into the DCs
            HGDIOBJ oldSourceDCObj = sourceDC.SelectObject(sourceImg);
            HGDIOBJ oldDestDCObj = destDC.SelectObject(image);
            // Actually flip the bitmap
            destDC.StretchBlt(0, 0, size.cx, size.cy,
                              &sourceDC, size.cx, 0, -size.cx, size.cy,
                              SRCCOPY);
            // Reselect the old objects back into their DCs
            sourceDC.SelectObject(oldSourceDCObj);
            destDC.SelectObject(oldDestDCObj);

            sourceDC.DeleteDC();
            destDC.DeleteDC();
        }

        CImageList imageList;
        imageList.Create(size.cy, size.cy, ILC_COLOR32, size.cx / size.cy, 0);
        imageList.Add(&image, nullptr);

        if (SUCCEEDED(m_pTaskbarList->ThumbBarSetImageList(m_frame.m_hWnd, imageList.GetSafeHandle()))) {
            THUMBBUTTON buttons[5] = {};

            for (size_t i = 0; i < _countof(buttons); i++) {
                buttons[i].dwMask = THB_BITMAP | THB_TOOLTIP | THB_FLAGS;
                buttons[i].dwFlags = THBF_DISABLED;
                buttons[i].iBitmap = 0; // Will be set later
            }

            // PREVIOUS
            buttons[0].iId = IDTB_BUTTON3;
            // STOP
            buttons[1].iId = IDTB_BUTTON1;
            // PLAY/PAUSE
            buttons[2].iId = IDTB_BUTTON2;
            // NEXT
            buttons[3].iId = IDTB_BUTTON4;
            // FULLSCREEN
            buttons[4].iId = IDTB_BUTTON5;

            if (bRTLLayout) { // We don't want the buttons to be mirrored so we pre-mirror them
                std::reverse(buttons, buttons + _countof(buttons));
            }

            if (SUCCEEDED(m_pTaskbarList->ThumbBarAddButtons(m_frame.m_hWnd, _countof(buttons), buttons))) {
                return UpdateThumbarButton();
            }
        }
    }

    return E_FAIL;
}

HRESULT CApiBridge::UpdateThumbarButton()
{
    MPC_PLAYSTATE state = PS_STOP;
    if (m_frame.GetLoadState() == MLS::LOADED) {
        switch (m_frame.GetMediaState()) {
            case State_Running:
                state = PS_PLAY;
                break;
            case State_Paused:
                state = PS_PAUSE;
                break;
        }
    }
    return UpdateThumbarButton(state);
}

HRESULT CApiBridge::UpdateThumbarButton(MPC_PLAYSTATE iPlayState)
{
    if (!m_pTaskbarList) {
        return E_FAIL;
    }

    THUMBBUTTON buttons[5] = {};

    for (size_t i = 0; i < _countof(buttons); i++) {
        buttons[i].dwMask = THB_BITMAP | THB_TOOLTIP | THB_FLAGS;
    }

    buttons[0].iId = IDTB_BUTTON3;
    buttons[1].iId = IDTB_BUTTON1;
    buttons[2].iId = IDTB_BUTTON2;
    buttons[3].iId = IDTB_BUTTON4;
    buttons[4].iId = IDTB_BUTTON5;

    const CAppSettings& s = AfxGetAppSettings();

    if (!s.bUseEnhancedTaskBar) {
        m_pTaskbarList->SetOverlayIcon(m_frame.m_hWnd, nullptr, L"");
        m_pTaskbarList->SetProgressState(m_frame.m_hWnd, TBPF_NOPROGRESS);

        for (size_t i = 0; i < _countof(buttons); i++) {
            buttons[i].dwFlags = THBF_HIDDEN;
        }
    } else {
        buttons[0].iBitmap = 0;
        StringCchCopy(buttons[0].szTip, _countof(buttons[0].szTip), ResStr(IDS_AG_PREVIOUS));

        buttons[1].iBitmap = 1;
        StringCchCopy(buttons[1].szTip, _countof(buttons[1].szTip), ResStr(IDS_AG_STOP));

        buttons[2].iBitmap = 3;
        StringCchCopy(buttons[2].szTip, _countof(buttons[2].szTip), ResStr(IDS_AG_PLAYPAUSE));

        buttons[3].iBitmap = 4;
        StringCchCopy(buttons[3].szTip, _countof(buttons[3].szTip), ResStr(IDS_AG_NEXT));

        buttons[4].iBitmap = 5;
        StringCchCopy(buttons[4].szTip, _countof(buttons[4].szTip), ResStr(IDS_AG_FULLSCREEN));

        if (m_frame.GetLoadState() == MLS::LOADED) {
            HICON hIcon = nullptr;

            buttons[0].dwFlags = (!s.fUseSearchInFolder && m_frame.m_wndPlaylistBar.GetCount() <= 1 && (m_frame.m_pCB && m_frame.m_pCB->ChapGetCount() <= 1)) ? THBF_DISABLED : THBF_ENABLED;
            buttons[3].dwFlags = (!s.fUseSearchInFolder && m_frame.m_wndPlaylistBar.GetCount() <= 1 && (m_frame.m_pCB && m_frame.m_pCB->ChapGetCount() <= 1)) ? THBF_DISABLED : THBF_ENABLED;
            buttons[4].dwFlags = THBF_ENABLED;

            if (iPlayState == PS_PLAY) {
                buttons[1].dwFlags = THBF_ENABLED;
                buttons[2].dwFlags = THBF_ENABLED;
                buttons[2].iBitmap = 2;

                hIcon = (HICON)LoadImage(AfxGetInstanceHandle(), MAKEINTRESOURCE(IDR_TB_PLAY), IMAGE_ICON, 0, 0, LR_DEFAULTSIZE);
                m_pTaskbarList->SetProgressState(m_frame.m_hWnd, m_frame.m_wndSeekBar.HasDuration() ? TBPF_NORMAL : TBPF_NOPROGRESS);
            } else if (iPlayState == PS_STOP) {
                buttons[1].dwFlags = THBF_DISABLED;
                buttons[2].dwFlags = THBF_ENABLED;
                buttons[2].iBitmap = 3;

                hIcon = (HICON)LoadImage(AfxGetInstanceHandle(), MAKEINTRESOURCE(IDR_TB_STOP), IMAGE_ICON, 0, 0, LR_DEFAULTSIZE);
                m_pTaskbarList->SetProgressState(m_frame.m_hWnd, TBPF_NOPROGRESS);
            } else if (iPlayState == PS_PAUSE) {
                buttons[1].dwFlags = THBF_ENABLED;
                buttons[2].dwFlags = THBF_ENABLED;
                buttons[2].iBitmap = 3;

                hIcon = (HICON)LoadImage(AfxGetInstanceHandle(), MAKEINTRESOURCE(IDR_TB_PAUSE), IMAGE_ICON, 0, 0, LR_DEFAULTSIZE);
                m_pTaskbarList->SetProgressState(m_frame.m_hWnd, m_frame.m_wndSeekBar.HasDuration() ? TBPF_PAUSED : TBPF_NOPROGRESS);
            }

            if (m_frame.GetPlaybackMode() == PM_DVD && m_frame.m_disc.m_iDVDDomain != DVD_DOMAIN_Title) {
                buttons[0].dwFlags = THBF_DISABLED;
                buttons[1].dwFlags = THBF_DISABLED;
                buttons[2].dwFlags = THBF_DISABLED;
                buttons[3].dwFlags = THBF_DISABLED;
            }

            m_pTaskbarList->SetOverlayIcon(m_frame.m_hWnd, hIcon, L"");

            if (hIcon != nullptr) {
                DestroyIcon(hIcon);
            }
        } else {
            for (size_t i = 0; i < _countof(buttons); i++) {
                buttons[i].dwFlags = THBF_DISABLED;
            }

            m_pTaskbarList->SetOverlayIcon(m_frame.m_hWnd, nullptr, L"");
            m_pTaskbarList->SetProgressState(m_frame.m_hWnd, TBPF_NOPROGRESS);
        }

        UpdateThumbnailClip();
    }

    // Try to locate the window used to display the task bar to check if it is RTLed
    if (CWnd* pTaskBarWnd = m_frame.FindWindow(_T("TaskListThumbnailWnd"), nullptr)) {
        // We don't want the buttons to be mirrored so we pre-mirror them
        if (pTaskBarWnd->GetExStyle() & WS_EX_LAYOUTRTL) {
            for (UINT i = 0; i < _countof(buttons); i++) {
                buttons[i].iBitmap = _countof(buttons) - buttons[i].iBitmap;
            }
            std::reverse(buttons, buttons + _countof(buttons));
        }
    }

    return m_pTaskbarList->ThumbBarUpdateButtons(m_frame.m_hWnd, _countof(buttons), buttons);
}

HRESULT CApiBridge::UpdateThumbnailClip()
{
    if (!m_pTaskbarList || !m_frame.m_hWnd || !m_frame.m_wndView.m_hWnd) {
        return E_FAIL;
    }

    const CAppSettings& s = AfxGetAppSettings();

    CRect r;
    m_frame.m_wndView.GetClientRect(&r);
    if (s.eCaptionMenuMode == MODE_SHOWCAPTIONMENU) {
        r.OffsetRect(0, GetSystemMetrics(SM_CYMENU));
    }

    if (!s.bUseEnhancedTaskBar || (m_frame.GetLoadState() != MLS::LOADED) || m_frame.IsFullScreenMode() || r.Width() <= 0 || r.Height() <= 0) {
        return m_pTaskbarList->SetThumbnailClip(m_frame.m_hWnd, nullptr);
    }

    return m_pTaskbarList->SetThumbnailClip(m_frame.m_hWnd, &r);
}

void CApiBridge::MediaTransportControlSetMedia() {
    if (m_media_trans_control.smtc_updater && m_media_trans_control.smtc_controls) {
        TRACE(_T("CMainFrame::MediaTransportControlSetMedia()\n"));
        HRESULT ret = S_OK;
        bool have_secondary_title = false;

        CString title = m_frame.getBestTitle();
        if (title.IsEmpty()) {
            title = m_frame.GetFileName();
        }

        CString author;
        if (m_graph.m_pAMMC[0]) {
            for (const auto& pAMMC : m_graph.m_pAMMC) {
                if (pAMMC) {
                    CComBSTR bstr;
                    if (SUCCEEDED(pAMMC->get_AuthorName(&bstr)) && bstr.Length()) {
                        author = bstr.m_str;
                        author.Trim();
                    }
                }
            }
        }

        // Set media details
        if (m_frame.m_fAudioOnly) {
            ret = m_media_trans_control.smtc_updater->put_Type(ABI::Windows::Media::MediaPlaybackType_Music);
            if (ret != S_OK) {
                TRACE(_T("MediaTransControls: put_Type error %ld\n"), ret);
                return;
            }

            CComPtr<ABI::Windows::Media::IMusicDisplayProperties> pMusicDisplayProperties;
            ret = m_media_trans_control.smtc_updater->get_MusicProperties(&pMusicDisplayProperties);
            if (ret != S_OK) {
                TRACE(_T("MediaTransControls: get_MusicProperties error %ld\n"), ret);
                return;
            }

            if (!title.IsEmpty()) {
                HSTRING ttitle;
                if (WindowsCreateString(title.GetString(), title.GetLength(), &ttitle) == S_OK) {
                    ret = pMusicDisplayProperties->put_Title(ttitle);
                    ASSERT(ret == S_OK);
                }
            }
            if (!author.IsEmpty()) {
                HSTRING temp;
                if (WindowsCreateString(author.GetString(), author.GetLength(), &temp) == S_OK) {
                    ret = pMusicDisplayProperties->put_Artist(temp);
                    ASSERT(ret == S_OK);
                }
            }
        } else {
            ret = m_media_trans_control.smtc_updater->put_Type(ABI::Windows::Media::MediaPlaybackType_Video);
            if (ret != S_OK) {
                TRACE(_T("MediaTransControls: put_Type error %ld\n"), ret);
                return;
            }

            CComPtr<ABI::Windows::Media::IVideoDisplayProperties> pVideoDisplayProperties;
            ret = m_media_trans_control.smtc_updater->get_VideoProperties(&pVideoDisplayProperties);
            if (ret != S_OK) {
                TRACE(_T("MediaTransControls: get_VideoProperties error %ld\n"), ret);
                return;
            }

            if (!title.IsEmpty()) {
                HSTRING ttitle;
                if (WindowsCreateString(title.GetString(), title.GetLength(), &ttitle) == S_OK) {
                    ret = pVideoDisplayProperties->put_Title(ttitle);
                    ASSERT(ret == S_OK);
                }
            }
            CString chapter;
            if (m_frame.m_pCB) {
                DWORD dwChapCount = m_frame.m_pCB->ChapGetCount();
                if (dwChapCount) {
                    REFERENCE_TIME rtNow;
                    m_graph.m_pMS->GetCurrentPosition(&rtNow);

                    CComBSTR bstr;
                    long currentChap = m_frame.m_pCB->ChapLookup(&rtNow, &bstr);
                    if (bstr.Length()) {
                        chapter.Format(_T("%s (%ld/%lu)"), bstr.m_str, std::max(0l, currentChap + 1l), dwChapCount);
                    } else {
                        chapter.Format(_T("%ld/%lu"), currentChap + 1, dwChapCount);
                    }
                }
            }
            if (!chapter.IsEmpty() && chapter != title) {
                HSTRING temp;
                if (WindowsCreateString(chapter.GetString(), chapter.GetLength(), &temp) == S_OK) {
                    ret = pVideoDisplayProperties->put_Subtitle(temp);
                    ASSERT(ret == S_OK);
                    have_secondary_title = true;
                }
            }
            if (!have_secondary_title && !author.IsEmpty() && author != title) {
                HSTRING temp;
                if (WindowsCreateString(author.GetString(), author.GetLength(), &temp) == S_OK) {
                    ret = pVideoDisplayProperties->put_Subtitle(temp);
                    ASSERT(ret == S_OK);
                    have_secondary_title = true;
                }
            }                
        }

        // Thumbnail
#if MPC_SMTC_VIDEO_THUMBNAIL
        if (!m_frame.m_fAudioOnly) {
            // For video, schedule capturing a video frame after playback has started,
            // see MediaTransportControlUpdateThumbnail()
            m_lastSMTCThumbnailTick = 0;
            m_nextSMTCThumbnailUpdate = GetTickCount64() + 5000ULL;
        } else
#endif
        {
            CComQIPtr<IFilterGraph> pFilterGraph = m_graph.m_pGB;
            std::vector<BYTE> internalCover;
            if (CoverArt::FindEmbedded(pFilterGraph, internalCover)) {
                m_media_trans_control.loadThumbnail(internalCover.data(), internalCover.size());
            } else {
                CPlaylistItem pli = CPlaylistItem(true);
                if (m_frame.m_wndPlaylistBar.GetCur(pli) && !pli.m_cover.IsEmpty()) {
                    m_media_trans_control.loadThumbnail(pli.m_cover);
                } else {
                    CString filename = m_frame.m_wndPlaylistBar.GetCurFileName();
                    CString filename_no_ext;
                    CString filedir;
                    if (!PathUtils::IsURL(filename)) {
                        CLongPath path = CLongPath(filename);
                        if (path.FileExists()) {
                            path.RemoveExtension();
                            filename_no_ext = path.m_strPath;
                            path.RemoveFileSpec();
                            filedir = path.m_strPath;
                            bool is_file_art = false;
                            CString img = CoverArt::FindExternal(filename_no_ext, filedir, author, is_file_art);
                            if (!img.IsEmpty()) {
                                if (m_frame.m_fAudioOnly || is_file_art) {
                                    m_media_trans_control.loadThumbnail(img);
                                }
                            }
                        }
                    }
                }
            }
        }

        // Update data and status
        ret = m_media_trans_control.smtc_controls->put_PlaybackStatus(ABI::Windows::Media::MediaPlaybackStatus::MediaPlaybackStatus_Playing);
        if (ret != S_OK) {
            TRACE(_T("MediaTransControls: put_PlaybackStatus error %ld\n"), ret);
            return;
        }
        ret = m_media_trans_control.smtc_updater->Update();
        if (ret != S_OK) {
            TRACE(_T("MediaTransControls: Update error %ld\n"), ret);
            return;
        }
        // Enable
        ret = m_media_trans_control.smtc_controls->put_IsEnabled(true);
        if (ret != S_OK) {
            TRACE(_T("MediaTransControls: put_IsEnabled error %ld\n"), ret);
            return;
        }

        // ISystemMediaTransportControls2: playback rate, repeat/shuffle state and timeline
        m_media_trans_control.SetPlaybackRate(m_frame.m_dSpeedRate);
        m_media_trans_control.SetShuffleEnabled(AfxGetAppSettings().bShufflePlaylistItems);
        MediaTransportControlUpdateAutoRepeat();
        MediaTransportControlUpdateTimeline(true);
    }
}

void CApiBridge::MediaTransportControlUpdateState(OAFilterState state) {
    if (m_media_trans_control.IsActive()) {
        if (state == State_Running)      m_media_trans_control.smtc_controls->put_PlaybackStatus(ABI::Windows::Media::MediaPlaybackStatus_Playing);
        else if (state == State_Paused)  m_media_trans_control.smtc_controls->put_PlaybackStatus(ABI::Windows::Media::MediaPlaybackStatus_Paused);
        else if (state == State_Stopped) m_media_trans_control.smtc_controls->put_PlaybackStatus(ABI::Windows::Media::MediaPlaybackStatus_Stopped);
        else                             m_media_trans_control.smtc_controls->put_PlaybackStatus(ABI::Windows::Media::MediaPlaybackStatus_Changing);

        // Keep rate and timeline in sync with the new state, so that consumers
        // that extrapolate the playback position stay accurate
        m_media_trans_control.SetPlaybackRate(m_frame.m_dSpeedRate);
        MediaTransportControlUpdateTimeline(true);
#if MPC_SMTC_VIDEO_THUMBNAIL
        if (state == State_Paused) {
            MediaTransportControlUpdateThumbnail();
        }
#endif
    }
}

void CApiBridge::MediaTransportControlUpdateTimeline(bool force /*= false*/) {
    if (!m_media_trans_control.IsActive() || !m_media_trans_control.smtc_controls2) {
        return;
    }
    // Note: IsActive() is a COM call, so when throttling check the tick count first
    ULONGLONG tick = GetTickCount64();
    if (!force && tick < m_lastSMTCTimelineUpdate + 2000ULL) {
        return;
    }
    if (m_frame.GetLoadState() != MLS::LOADED || m_frame.IsPlaybackCaptureMode() || !m_media_trans_control.IsActive()) {
        return;
    }
    m_lastSMTCTimelineUpdate = tick;

    __int64 start = 0, stop = 0;
    m_frame.m_wndSeekBar.GetRange(start, stop);
    if (stop > 0) {
        m_media_trans_control.UpdateTimelineProperties(0, stop, m_frame.m_wndSeekBar.GetPos());
    }

#if MPC_SMTC_VIDEO_THUMBNAIL
    // Periodic thumbnail refresh, driven from the same throttled path
    if (m_nextSMTCThumbnailUpdate && tick >= m_nextSMTCThumbnailUpdate && m_frame.GetMediaState() == State_Running) {
        MediaTransportControlUpdateThumbnail();
    }
#endif
}

void CApiBridge::MediaTransportControlUpdateAutoRepeat() {
    const CAppSettings& s = AfxGetAppSettings();
    ABI::Windows::Media::MediaPlaybackAutoRepeatMode mode = ABI::Windows::Media::MediaPlaybackAutoRepeatMode_None;
    if (s.fLoopForever) {
        mode = (s.eLoopMode == CAppSettings::LoopMode::FILE) ? ABI::Windows::Media::MediaPlaybackAutoRepeatMode_Track
                                                             : ABI::Windows::Media::MediaPlaybackAutoRepeatMode_List;
    }
    m_media_trans_control.SetAutoRepeatMode(mode);
}

#if MPC_SMTC_VIDEO_THUMBNAIL
void CApiBridge::MediaTransportControlUpdateThumbnail() {
    if (m_frame.m_fAudioOnly || m_frame.GetLoadState() != MLS::LOADED || m_frame.IsPlaybackCaptureMode() || !m_media_trans_control.IsActive()) {
        return;
    }
    // Capture at most once per second, e.g. when seeking repeatedly
    ULONGLONG tick = GetTickCount64();
    if (m_lastSMTCThumbnailTick && tick < m_lastSMTCThumbnailTick + 1000ULL) {
        return;
    }
    m_lastSMTCThumbnailTick = tick;
    m_nextSMTCThumbnailUpdate = tick + 30000ULL;

    std::vector<BYTE> thumbnail;
    if (m_frame.m_snapshot.CaptureVideoThumbnail(thumbnail)) {
        m_media_trans_control.loadThumbnail(thumbnail.data(), thumbnail.size());
        if (m_media_trans_control.smtc_updater) {
            m_media_trans_control.smtc_updater->Update();
        }
    }
}
#endif

LRESULT CApiBridge::OnSmtcSeek(WPARAM wParam, LPARAM lParam) {
    if (m_frame.GetLoadState() == MLS::LOADED && !m_frame.IsPlaybackCaptureMode()) {
        m_frame.SeekTo(m_media_trans_control.requested_seek_position, false);
    }
    return 0;
}

LRESULT CApiBridge::OnSmtcAutoRepeat(WPARAM wParam, LPARAM lParam) {
    CAppSettings& s = AfxGetAppSettings();
    auto mode = static_cast<ABI::Windows::Media::MediaPlaybackAutoRepeatMode>(wParam);
    switch (mode) {
        case ABI::Windows::Media::MediaPlaybackAutoRepeatMode_Track:
            s.fLoopForever = true;
            s.eLoopMode = CAppSettings::LoopMode::FILE;
            break;
        case ABI::Windows::Media::MediaPlaybackAutoRepeatMode_List:
            s.fLoopForever = true;
            s.eLoopMode = CAppSettings::LoopMode::PLAYLIST;
            break;
        default:
            s.fLoopForever = false;
            break;
    }
    m_frame.m_nLoops = 0;
    MediaTransportControlUpdateAutoRepeat();
    return 0;
}

LRESULT CApiBridge::OnSmtcShuffle(WPARAM wParam, LPARAM lParam) {
    if (AfxGetAppSettings().bShufflePlaylistItems != (wParam != 0)) {
        m_frame.OnPlaylistToggleShuffle();
    } else {
        m_media_trans_control.SetShuffleEnabled(wParam != 0);
    }
    return 0;
}

LRESULT CApiBridge::OnSmtcRate(WPARAM wParam, LPARAM lParam) {
    double rate = m_media_trans_control.requested_playback_rate;
    if (m_frame.GetLoadState() == MLS::LOADED && rate > 0.0) {
        m_frame.SetPlayingRate(rate);
    }
    return 0;
}
