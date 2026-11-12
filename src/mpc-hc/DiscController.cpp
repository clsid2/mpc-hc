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
#include "DiscController.h"
#include "MainFrm.h"
#include "mplayerc.h"
#include <DSUtil.h>
#include <moreuuids.h>
#include "../DeCSS/VobFile.h"

BEGIN_MESSAGE_MAP(CDiscController, CCmdTarget)
    ON_COMMAND(ID_FILE_OPENDVDBD, OnFileOpendvd)
    ON_COMMAND_RANGE(ID_DVD_ANGLE_NEXT, ID_DVD_ANGLE_PREV, OnDvdAngle)
    ON_COMMAND_RANGE(ID_DVD_AUDIO_NEXT, ID_DVD_AUDIO_PREV, OnDvdAudio)
    ON_COMMAND_RANGE(ID_DVD_SUB_NEXT, ID_DVD_SUB_PREV, OnDvdSub)
    ON_COMMAND(ID_DVD_SUB_ONOFF, OnDvdSubOnOff)
    ON_COMMAND_RANGE(ID_NAVIGATE_TITLEMENU, ID_NAVIGATE_CHAPTERMENU, OnNavigateMenu)
    ON_UPDATE_COMMAND_UI_RANGE(ID_NAVIGATE_TITLEMENU, ID_NAVIGATE_CHAPTERMENU, OnUpdateNavigateMenu)
END_MESSAGE_MAP()

CDiscController::CDiscController(CMainFrame& frame)
    : m_frame(frame)
    , m_graph(frame.Graph())
    , m_iDVDDomain(DVD_DOMAIN_Stop)
    , m_iDVDTitle(0)
    , m_bDVDStillOn(false)
    , m_MPLSPlaylist()
    , m_bIsBDPlay(false)
    , m_bHasBDMeta(false)
{
}

void CDiscController::OnFileOpendvd()
{
    if (!m_frame.IsStateClosedOrLoaded() || m_frame.IsD3DFullScreenMode()) {
        return;
    }

    CAppSettings& s = AfxGetAppSettings();
    CString strTitle(StrRes(IDS_MAINFRM_46));
    CString path;

    if (s.fUseDVDPath && !s.strDVDPath.IsEmpty()) {
        path = s.strDVDPath;
    } else {
        //strDVDPath is actually used as a default to open without the dialog,
        //but since it is always updated to the last path chosen,
        //we can use it as the default for the dialog, too
        CFolderPickerDialog fd(ForceTrailingSlash(s.strDVDPath), FOS_PATHMUSTEXIST, m_frame.GetModalParent());
        fd.m_ofn.lpstrTitle = strTitle;

        if (fd.DoModal() == IDOK) {
            path = fd.GetPathName(); //getfolderpath() does not work correctly for CFolderPickerDialog
        } else {
            return;
        }
    }
    OpenDVDOrBD(path);
}

void CDiscController::OpenDVDOrBD(CStringW path) {
    if (!path.IsEmpty()) {
        AfxGetAppSettings().strDVDPath = path;
        if (!OpenBD(path)) {
            CAutoPtr<OpenDVDData> p(DEBUG_NEW OpenDVDData());
            if (p) {
                p->path = path;
                p->path.Replace(_T('/'), _T('\\'));
                p->path = ForceTrailingSlash(p->path);
                m_frame.m_wndPlaylistBar.OpenDVD(p->path);
            }
            m_frame.OpenMedia(p);
        }
    }
}

bool CDiscController::OpenBD(CString Path)
{
    CHdmvClipInfo ClipInfo;
    CString strPlaylistFile;
    CHdmvClipInfo::HdmvPlaylist MainPlaylist;

#if INTERNAL_SOURCEFILTER_MPEG
    const CAppSettings& s = AfxGetAppSettings();
    bool InternalMpegSplitter = s.SrcFilters[SRC_MPEG] || s.SrcFilters[SRC_MPEGTS];
#else
    bool InternalMpegSplitter = false;
#endif

    m_LastOpenBDPath = Path;

    CString ext = CLongPath(Path).GetExtension();
    ext.MakeLower();

    if ((CLongPath(Path).IsDirectory() && Path.Find(_T("\\BDMV"))) || CLongPath(Path + _T("\\BDMV")).IsDirectory() || (!ext.IsEmpty() && ext == _T(".bdmv"))) {
        if (!ext.IsEmpty() && ext == _T(".bdmv")) {
            Path.Replace(_T("\\BDMV\\"), _T("\\"));
            CLongPath _Path(Path);
            _Path.RemoveFileSpec();
            Path = CString(_Path);
        } else if (Path.Find(_T("\\BDMV"))) {
            Path.Replace(_T("\\BDMV"), _T("\\"));
        }
        if (SUCCEEDED(ClipInfo.FindMainMovie(Path, strPlaylistFile, MainPlaylist, m_MPLSPlaylist))) {
            m_bIsBDPlay = true;

            m_bHasBDMeta = ClipInfo.ReadMeta(Path, m_BDMeta);

            if (!InternalMpegSplitter && !ext.IsEmpty() && ext == _T(".bdmv")) {
                return false;
            } else {
                m_frame.m_wndPlaylistBar.Empty();
                CAtlList<CString> sl;

                if (InternalMpegSplitter) {
                    sl.AddTail(CString(strPlaylistFile));
                } else {
                    sl.AddTail(CString(Path + _T("\\BDMV\\index.bdmv")));
                }

                m_frame.m_wndPlaylistBar.Append(sl, false);
                m_frame.PostMessage(WM_MPC_OPENCURPLAYLIST, 0, 0);
                return true;
            }
        }
    }

    m_LastOpenBDPath = _T("");
    return false;
}

CHdmvClipInfo::BDMVMeta CDiscController::GetBDMVMeta()
{
    return m_BDMeta.GetHead();
}

void CDiscController::OnDvdAngle(UINT nID)
{
    if (m_frame.GetLoadState() != MLS::LOADED) {
        return;
    }

    if (m_graph.m_pDVDI && m_graph.m_pDVDC) {
        ULONG ulAnglesAvailable, ulCurrentAngle;
        if (SUCCEEDED(m_graph.m_pDVDI->GetCurrentAngle(&ulAnglesAvailable, &ulCurrentAngle)) && ulAnglesAvailable > 1) {
            ulCurrentAngle += (nID == ID_DVD_ANGLE_NEXT) ? 1 : -1;
            if (ulCurrentAngle > ulAnglesAvailable) {
                ulCurrentAngle = 1;
            } else if (ulCurrentAngle < 1) {
                ulCurrentAngle = ulAnglesAvailable;
            }
            m_graph.m_pDVDC->SelectAngle(ulCurrentAngle, DVD_CMD_FLAG_Block, nullptr);

            CString osdMessage;
            osdMessage.Format(IDS_AG_ANGLE, ulCurrentAngle);
            m_frame.m_OSD.DisplayMessage(OSD_TOPLEFT, osdMessage);
        }
    }
}

void CDiscController::OnDvdAudio(UINT nID)
{
    nID -= ID_DVD_AUDIO_NEXT;

    if (m_frame.GetLoadState() != MLS::LOADED) {
        return;
    }

    if (m_graph.m_pDVDI && m_graph.m_pDVDC) {
        ULONG nStreamsAvailable, nCurrentStream;
        if (SUCCEEDED(m_graph.m_pDVDI->GetCurrentAudio(&nStreamsAvailable, &nCurrentStream)) && nStreamsAvailable > 1) {
            DVD_AudioAttributes AATR;
            UINT nNextStream = (nCurrentStream + (nID == 0 ? 1 : nStreamsAvailable - 1)) % nStreamsAvailable;

            HRESULT hr = m_graph.m_pDVDC->SelectAudioStream(nNextStream, DVD_CMD_FLAG_Block, nullptr);
            if (SUCCEEDED(m_graph.m_pDVDI->GetAudioAttributes(nNextStream, &AATR))) {
                CString lang;
                CString strMessage;
                if (AATR.Language) {
                    GetLocaleString(AATR.Language, LOCALE_SENGLANGUAGE, lang);
                    m_frame.currentAudioLang = lang;
                } else {
                    lang.Format(IDS_AG_UNKNOWN, nNextStream + 1);
                    m_frame.currentAudioLang.Empty();
                }

                CString format = GetDVDAudioFormatName(AATR);
                CString str;

                if (!format.IsEmpty()) {
                    str.Format(IDS_MAINFRM_11,
                               lang.GetString(),
                               format.GetString(),
                               AATR.dwFrequency,
                               AATR.bQuantization,
                               AATR.bNumberOfChannels,
                               ResStr(AATR.bNumberOfChannels > 1 ? IDS_MAINFRM_13 : IDS_MAINFRM_12).GetString());
                    if (FAILED(hr)) {
                        str += _T(" [") + ResStr(IDS_AG_ERROR) + _T("] ");
                    }
                    strMessage.Format(IDS_AUDIO_STREAM, str.GetString());
                    m_frame.m_OSD.DisplayMessage(OSD_TOPLEFT, strMessage);
                }
            }
        }
    }
}

void CDiscController::OnDvdSub(UINT nID)
{
    nID -= ID_DVD_SUB_NEXT;

    if (m_frame.GetLoadState() != MLS::LOADED) {
        return;
    }

    if (m_graph.m_pDVDI && m_graph.m_pDVDC) {
        ULONG ulStreamsAvailable, ulCurrentStream;
        BOOL bIsDisabled;
        if (SUCCEEDED(m_graph.m_pDVDI->GetCurrentSubpicture(&ulStreamsAvailable, &ulCurrentStream, &bIsDisabled))
                && ulStreamsAvailable > 1) {
            //UINT nNextStream = (ulCurrentStream+(nID==0?1:ulStreamsAvailable-1))%ulStreamsAvailable;
            int nNextStream;

            if (!bIsDisabled) {
                nNextStream = ulCurrentStream + (nID == 0 ? 1 : -1);
            } else {
                nNextStream = (nID == 0 ? 0 : ulStreamsAvailable - 1);
            }

            if (!bIsDisabled && ((nNextStream < 0) || ((ULONG)nNextStream >= ulStreamsAvailable))) {
                m_graph.m_pDVDC->SetSubpictureState(FALSE, DVD_CMD_FLAG_Block, nullptr);
                m_frame.m_OSD.DisplayMessage(OSD_TOPLEFT, ResStr(IDS_SUBTITLE_STREAM_OFF));
            } else {
                HRESULT hr = m_graph.m_pDVDC->SelectSubpictureStream(nNextStream, DVD_CMD_FLAG_Block, nullptr);

                DVD_SubpictureAttributes SATR;
                m_graph.m_pDVDC->SetSubpictureState(TRUE, DVD_CMD_FLAG_Block, nullptr);
                if (SUCCEEDED(m_graph.m_pDVDI->GetSubpictureAttributes(nNextStream, &SATR))) {
                    CString lang;
                    CString strMessage;
                    GetLocaleString(SATR.Language, LOCALE_SENGLANGUAGE, lang);

                    if (FAILED(hr)) {
                        lang += _T(" [") + ResStr(IDS_AG_ERROR) + _T("] ");
                    }
                    strMessage.Format(IDS_SUBTITLE_STREAM, lang.GetString());
                    m_frame.m_OSD.DisplayMessage(OSD_TOPLEFT, strMessage);
                }
            }
        }
    }
}

void CDiscController::OnDvdSubOnOff()
{
    if (m_frame.GetLoadState() != MLS::LOADED) {
        return;
    }

    if (m_graph.m_pDVDI && m_graph.m_pDVDC) {
        ULONG ulStreamsAvailable, ulCurrentStream;
        BOOL bIsDisabled;
        if (SUCCEEDED(m_graph.m_pDVDI->GetCurrentSubpicture(&ulStreamsAvailable, &ulCurrentStream, &bIsDisabled))) {
            m_graph.m_pDVDC->SetSubpictureState(bIsDisabled, DVD_CMD_FLAG_Block, nullptr);
        }
    }
}

void CDiscController::OnNavigateMenu(UINT nID)
{
    nID -= ID_NAVIGATE_TITLEMENU;

    if (m_frame.GetLoadState() != MLS::LOADED || m_frame.GetPlaybackMode() != PM_DVD) {
        return;
    }

    m_frame.m_dSpeedRate = 1.0;

    if (m_frame.GetMediaState() != State_Running) {
        m_frame.SendMessage(WM_COMMAND, ID_PLAY_PLAY);
    }

    m_graph.m_pDVDC->ShowMenu((DVD_MENU_ID)(nID + 2), DVD_CMD_FLAG_Block | DVD_CMD_FLAG_Flush, nullptr);
}

void CDiscController::OnUpdateNavigateMenu(CCmdUI* pCmdUI)
{
    UINT nID = pCmdUI->m_nID - ID_NAVIGATE_TITLEMENU;
    ULONG ulUOPs;

    if (m_frame.GetLoadState() != MLS::LOADED || m_frame.GetPlaybackMode() != PM_DVD
            || FAILED(m_graph.m_pDVDI->GetCurrentUOPS(&ulUOPs))) {
        pCmdUI->Enable(FALSE);
        return;
    }

    pCmdUI->Enable(!(ulUOPs & (UOP_FLAG_ShowMenu_Title << nID)));
}

void CDiscController::SetupDVDChapters()
{
    // Release the old chapter bag and create a new one.
    // Due to smart pointers the old chapter bag won't
    // be deleted until all classes release it.
    m_frame.m_pCB.Release();
    m_frame.m_pCB = DEBUG_NEW CDSMChapterBag(nullptr, nullptr);

    WCHAR buff[MAX_PATH];
    ULONG len, ulNumOfChapters;
    DVD_PLAYBACK_LOCATION2 loc;

    if (m_graph.m_pDVDI && SUCCEEDED(m_graph.m_pDVDI->GetDVDDirectory(buff, _countof(buff), &len))
            && SUCCEEDED(m_graph.m_pDVDI->GetCurrentLocation(&loc))
            && SUCCEEDED(m_graph.m_pDVDI->GetNumberOfChapters(loc.TitleNum, &ulNumOfChapters))) {
        CString path;
        path.Format(L"%s\\video_ts.IFO", buff);
        ULONG VTSN, TTN;

        if (CVobFile::GetTitleInfo(path, loc.TitleNum, VTSN, TTN)) {
            path.Format(L"%s\\VTS_%02lu_0.IFO", buff, VTSN);
            CAtlList<CString> files;

            CVobFile vob;
            if (vob.Open(path, files, TTN, false)) {
                int iChaptersCount = vob.GetChaptersCount();
                if (ulNumOfChapters == (ULONG)iChaptersCount) {
                    for (int i = 0; i < iChaptersCount; i++) {
                        REFERENCE_TIME rt = vob.GetChapterOffset(i);

                        CStringW str;
                        str.Format(IDS_AG_CHAPTER, i + 1);

                        m_frame.m_pCB->ChapAppend(rt, str);
                    }
                } else {
                    // Parser failed!
                    ASSERT(FALSE);
                }
                vob.Close();
            }
        }
    }

    m_frame.m_pCB->ChapSort();

    m_frame.UpdateSeekbarChapterBag();
}

// Called from GraphThread
void CDiscController::OpenDVD(OpenDVDData* pODD)
{
    m_frame.lastOpenFile.Empty();

    CStringW fn = CStringW(pODD->path);
    HRESULT hr = m_graph.m_pGB->RenderFile(fn, nullptr);

    if (SUCCEEDED(hr)) {
        m_frame.lastOpenFile = fn;
    }

    CAppSettings& s = AfxGetAppSettings();

    if (s.fReportFailedPins) {
        m_frame.ShowMediaTypesDialog();
    }

    if (SUCCEEDED(hr) && m_frame.m_bUseSeekPreview) {
        if (FAILED(hr = m_frame.m_previewGraph.m_pGB->RenderFile(pODD->path, nullptr))) {
            m_frame.m_bUseSeekPreview = false;
        }
    }

    // Check for supported interfaces
    BeginEnumFilters(m_graph.m_pGB, pEF, pBF)
        CLSID clsid = GetCLSID(pBF);
        // DVD stuff
        if (!m_graph.m_pDVDC) {
            m_graph.m_pDVDC = pBF;
        }
        if (!m_graph.m_pDVDI) {
            m_graph.m_pDVDI = pBF;
        }
        // IAMStreamSelect filters / IDirectVobSub
        if (clsid == __uuidof(CAudioSwitcherFilter)) {
            m_graph.m_pAudioSwitcherSS = pBF;
        } else {
            if (clsid == CLSID_VSFilter || clsid == CLSID_XySubFilter) {
                if (!s.IsISRAutoLoadEnabled()) {
                    m_frame.m_subs.m_pDVS = pBF;
                    m_frame.m_subs.m_pDVS2 = pBF;
                }
            } else {
                if (clsid != CLSID_MPCBEAudioRenderer) {
                    if (CComQIPtr<IAMStreamSelect> pTest = pBF) {
                        if (!m_graph.m_pOtherSS[0]) {
                            m_graph.m_pOtherSS[0] = pBF;
                        } else if (!m_graph.m_pOtherSS[1]) {
                            m_graph.m_pOtherSS[1] = pBF;
                        } else {
                            ASSERT(false);
                        }
                    }
                }
            }
        }
        // Others
        if (!m_graph.m_pLN21) {
            m_graph.m_pLN21 = pBF;
        }
    EndEnumFilters;

    ASSERT(m_graph.m_pDVDC);
    ASSERT(m_graph.m_pDVDI);

    if (m_frame.m_bUseSeekPreview) {
        BeginEnumFilters(m_frame.m_previewGraph.m_pGB, pEF, pBF) {
            if ((m_frame.m_previewGraph.m_pDVDC = pBF) && (m_frame.m_previewGraph.m_pDVDI = pBF)) {
                break;
            }
        }
        EndEnumFilters;
    }

    if (hr == E_INVALIDARG) {
        throw (UINT)IDS_MAINFRM_93;
    } else if (hr == VFW_E_CANNOT_RENDER) {
        throw (UINT)IDS_DVD_NAV_ALL_PINS_ERROR;
    } else if (hr == VFW_S_PARTIAL_RENDER) {
        throw (UINT)IDS_DVD_NAV_SOME_PINS_ERROR;
    } else if (hr == E_NOINTERFACE || !m_graph.m_pDVDC || !m_graph.m_pDVDI) {
        throw (UINT)IDS_DVD_INTERFACES_ERROR;
    } else if (hr == VFW_E_CANNOT_LOAD_SOURCE_FILTER) {
        throw (UINT)IDS_MAINFRM_94;
    } else if (FAILED(hr)) {
        throw (UINT)IDS_AG_FAILED;
    }

    WCHAR buff[MAX_PATH];
    ULONG len = 0;
    if (SUCCEEDED(hr = m_graph.m_pDVDI->GetDVDDirectory(buff, _countof(buff), &len))) {
        pODD->title = CString(CStringW(buff)).TrimRight(_T("\\"));
    }

    if (s.fKeepHistory) {
        ULONGLONG llDVDGuid;
        if (m_graph.m_pDVDI && SUCCEEDED(m_graph.m_pDVDI->GetDiscID(nullptr, &llDVDGuid))) {
            auto* pMRU = &s.MRU;
            pMRU->Add(pODD->title, llDVDGuid);
        }
        if (!s.IsExcludedFromHistory(pODD->title)) {
            SHAddToRecentDocs(SHARD_PATH, pODD->title);
        }
    }

    // TODO: resetdvd
    m_graph.m_pDVDC->SetOption(DVD_ResetOnStop, FALSE);
    m_graph.m_pDVDC->SetOption(DVD_HMSF_TimeCodeEvents, TRUE);

    if (m_frame.m_bUseSeekPreview && m_frame.m_previewGraph.m_pDVDC) {
        m_frame.m_previewGraph.m_pDVDC->SetOption(DVD_ResetOnStop, FALSE);
        m_frame.m_previewGraph.m_pDVDC->SetOption(DVD_HMSF_TimeCodeEvents, TRUE);
    }

    if (s.idMenuLang) {
        m_graph.m_pDVDC->SelectDefaultMenuLanguage(s.idMenuLang);
    }
    if (s.idAudioLang) {
        m_graph.m_pDVDC->SelectDefaultAudioLanguage(s.idAudioLang, DVD_AUD_EXT_NotSpecified);
    }
    if (s.idSubtitlesLang) {
        m_graph.m_pDVDC->SelectDefaultSubpictureLanguage(s.idSubtitlesLang, DVD_SP_EXT_NotSpecified);
    }

    m_iDVDDomain = DVD_DOMAIN_Stop;

    m_frame.SetPlaybackMode(PM_DVD);
}

bool CDiscController::SeekToDVDChapter(int iChapter, bool bRelative /*= false*/)
{
    if (m_frame.GetPlaybackMode() != PM_DVD) {
        return false;
    }

    ULONG ulNumOfVolumes, ulVolume;
    DVD_DISC_SIDE Side;
    ULONG ulNumOfTitles = 0;
    CheckNoLogBool(m_graph.m_pDVDI->GetDVDVolumeInfo(&ulNumOfVolumes, &ulVolume, &Side, &ulNumOfTitles));

    DVD_PLAYBACK_LOCATION2 Location;
    ULONG ulNumOfChapters = 0;
    ULONG uTitle = 0, uChapter = 0;
    if (bRelative) {
        CheckNoLogBool(m_graph.m_pDVDI->GetCurrentLocation(&Location));

        CheckNoLogBool(m_graph.m_pDVDI->GetNumberOfChapters(Location.TitleNum, &ulNumOfChapters));

        uTitle = Location.TitleNum;
        uChapter = Location.ChapterNum;

        if (iChapter < 0) {
            ULONG tsec = (Location.TimeCode.bHours * 3600)
                         + (Location.TimeCode.bMinutes * 60)
                         + (Location.TimeCode.bSeconds);
            ULONG diff = 0;
            if (m_frame.m_lChapterStartTime != 0xFFFFFFFF && tsec > m_frame.m_lChapterStartTime) {
                diff = tsec - m_frame.m_lChapterStartTime;
            }
            // Go the previous chapter only if more than PREV_CHAP_THRESHOLD seconds
            // have passed since the beginning of the current chapter else restart it
            if (diff <= PREV_CHAP_THRESHOLD) {
                // If we are at the first chapter of a volume that isn't the first
                // one, we skip to the last chapter of the previous volume.
                if (uChapter == 1 && uTitle > 1) {
                    uTitle--;
                    CheckNoLogBool(m_graph.m_pDVDI->GetNumberOfChapters(uTitle, &uChapter));
                } else if (uChapter > 1) {
                    uChapter--;
                }
            }
        } else {
            // If we are at the last chapter of a volume that isn't the last
            // one, we skip to the first chapter of the next volume.
            if (uChapter == ulNumOfChapters && uTitle < ulNumOfTitles) {
                uTitle++;
                uChapter = 1;
            } else if (uChapter < ulNumOfChapters) {
                uChapter++;
            }
        }
    } else if (iChapter > 0) {
        uChapter = ULONG(iChapter);
        if (uChapter <= ulNumOfTitles) {
            uTitle = uChapter;
            uChapter = 1;
        } else {
            CheckNoLogBool(m_graph.m_pDVDI->GetCurrentLocation(&Location));
            uTitle = Location.TitleNum;
            uChapter -= ulNumOfTitles;
        }
    }

    if (uTitle && uChapter
            && SUCCEEDED(m_graph.m_pDVDC->PlayChapterInTitle(uTitle, uChapter, DVD_CMD_FLAG_Block | DVD_CMD_FLAG_Flush, nullptr))) {
        if (SUCCEEDED(m_graph.m_pDVDI->GetCurrentLocation(&Location))
                && SUCCEEDED(m_graph.m_pDVDI->GetNumberOfChapters(Location.TitleNum, &ulNumOfChapters))) {
            CString strTitle;
            strTitle.Format(IDS_AG_TITLE2, Location.TitleNum, ulNumOfTitles);
            __int64 start, stop;
            m_frame.m_wndSeekBar.GetRange(start, stop);

            CString strOSD;
            if (stop > 0) {
                const CAppSettings& s = AfxGetAppSettings();

                DVD_HMSF_TIMECODE currentHMSF = s.fRemainingTime ? RT2HMS_r(stop - HMSF2RT(Location.TimeCode)) : Location.TimeCode;
                DVD_HMSF_TIMECODE stopHMSF = RT2HMS_r(stop);
                strOSD.Format(_T("%s%s/%s %s, %s%02u/%02lu"),
                              s.fRemainingTime ? _T("- ") : _T(""), DVDtimeToString(currentHMSF, stopHMSF.bHours > 0).GetString(), DVDtimeToString(stopHMSF).GetString(),
                              strTitle.GetString(), ResStr(IDS_AG_CHAPTER2).GetString(), Location.ChapterNum, ulNumOfChapters);
            } else {
                strOSD.Format(_T("%s, %s%02u/%02lu"), strTitle.GetString(), ResStr(IDS_AG_CHAPTER2).GetString(), Location.ChapterNum, ulNumOfChapters);
            }

            m_frame.m_OSD.DisplayMessage(OSD_TOPLEFT, strOSD, 3000);
        }

        return true;
    }

    return false;
}

LPCTSTR CDiscController::GetDVDAudioFormatName(const DVD_AudioAttributes& ATR) const
{
    switch (ATR.AudioFormat) {
        case DVD_AudioFormat_AC3:
            return _T("AC3");
        case DVD_AudioFormat_MPEG1:
        case DVD_AudioFormat_MPEG1_DRC:
            return _T("MPEG1");
        case DVD_AudioFormat_MPEG2:
        case DVD_AudioFormat_MPEG2_DRC:
            return _T("MPEG2");
        case DVD_AudioFormat_LPCM:
            return _T("LPCM");
        case DVD_AudioFormat_DTS:
            return _T("DTS");
        case DVD_AudioFormat_SDDS:
            return _T("SDDS");
        case DVD_AudioFormat_Other:
        default:
            return MAKEINTRESOURCE(IDS_MAINFRM_137);
    }
}
