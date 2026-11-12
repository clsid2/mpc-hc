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
#include "SubtitleController.h"
#include "MainFrm.h"
#include "mplayerc.h"
#include "FGManager.h"
#include "TextPassThruFilter.h"
#include "OpenFileDlg.h"
#include "SaveSubtitlesFileDialog.h"
#include "CMPCThemePropertySheet.h"
#include "FileHandle.h"
#include "PPageSubStyle.h"
#include "PPageSubtitles.h"
#include "SubtitlesProviders.h"
#include "YoutubeDL.h"
#include "Logger.h"
#include <ISOLang.h>
#include <PathUtils.h>
#include <DSUtil.h>
#include <WinAPIUtils.h>
#include "../SubPic/DualSubPicProvider.h"
#include "../Subtitles/PGSSub.h"
#include "../Subtitles/RLECodedSubtitle.h"
#include "../Subtitles/RTS.h"
#include "../Subtitles/STS.h"
#include <SubRenderIntf.h>
#include <moreuuids.h>

// both live in MainFrm.cpp, which also uses them
extern bool g_bExternalSubtitle;
bool MatchSubtrackWithISOLang(CString& tname, const ISOLangT<CString>& l);

BEGIN_MESSAGE_MAP(CSubtitleController, CCmdTarget)
    ON_COMMAND_RANGE(ID_STREAM_SUB_NEXT, ID_STREAM_SUB_PREV, OnStreamSub)
    ON_COMMAND(ID_STREAM_SUB_ONOFF, OnStreamSubOnOff)
    ON_COMMAND(ID_SUBTITLES_AUTOCOPY, OnSubtitlesAutoCopy)
    ON_COMMAND(ID_FILE_SUBTITLES_LOAD, OnFileSubtitlesLoad)
    ON_UPDATE_COMMAND_UI(ID_FILE_SUBTITLES_LOAD, OnUpdateFileSubtitlesLoad)
    ON_COMMAND(ID_FILE_SUBTITLES_SAVE, OnFileSubtitlesSave)
    ON_UPDATE_COMMAND_UI(ID_FILE_SUBTITLES_SAVE, OnUpdateFileSubtitlesSave)
    //ON_COMMAND(ID_FILE_SUBTITLES_UPLOAD, OnFileSubtitlesUpload)
    //ON_UPDATE_COMMAND_UI(ID_FILE_SUBTITLES_UPLOAD, OnUpdateFileSubtitlesUpload)
    ON_COMMAND(ID_FILE_SUBTITLES_DOWNLOAD, OnFileSubtitlesDownload)
    ON_UPDATE_COMMAND_UI(ID_FILE_SUBTITLES_DOWNLOAD, OnUpdateFileSubtitlesDownload)
    ON_COMMAND(ID_VIEW_SUBRESYNC, OnViewSubresync)
    ON_UPDATE_COMMAND_UI(ID_VIEW_SUBRESYNC, OnUpdateViewSubresync)
    ON_COMMAND_RANGE(ID_GOTO_PREV_SUB, ID_GOTO_NEXT_SUB, OnGotoSubtitle)
    ON_COMMAND_RANGE(ID_SUBRESYNC_SHIFT_DOWN, ID_SUBRESYNC_SHIFT_UP, OnSubresyncShiftSub)
    ON_COMMAND_RANGE(ID_SUB_DELAY_DOWN, ID_SUB_DELAY_UP, OnSubtitleDelay)
    ON_COMMAND_RANGE(ID_SUB_POS_DOWN, ID_SUB_POS_UP, OnSubtitlePos)
    ON_COMMAND_RANGE(ID_SUB_FONT_SIZE_DEC, ID_SUB_FONT_SIZE_INC, OnSubtitleFontSize)
    ON_COMMAND_RANGE(ID_SUBTITLES_SUBITEM_START, ID_SUBTITLES_SUBITEM_END, OnPlaySubtitles)
    ON_COMMAND_RANGE(ID_SUBTITLES_SECONDARY_SUBITEM_START, ID_SUBTITLES_SECONDARY_SUBITEM_END, OnPlaySecondarySubtitle)
    ON_COMMAND(ID_SUBTITLES_SECONDARY_LOAD, OnSecondarySubtitleLoad)
    ON_COMMAND(ID_SUBTITLES_OVERRIDE_DEFAULT_STYLE, OnSubtitlesDefaultStyle)
    ON_COMMAND(ID_SUBTITLES_OVERRIDE_ALL_STYLES, OnSubtitlesOverrideStyles)
    ON_UPDATE_COMMAND_UI(ID_SUBTITLES, OnUpdateSubtitlesButton)
END_MESSAGE_MAP()

CSubtitleController::CSubtitleController(CMainFrame& frame)
    : m_frame(frame)
    , m_graph(frame.Graph())
    , m_posFirstExtSub(nullptr)
    , m_nLastCopiedSubSegment(-1)
    , m_rtNextAutoCopySubtitle(0)
    , m_nCurSubtitle(-1)
    , m_lSubtitleShift(0)
    , m_rtCurSubPos(0)
    , m_wndSubtitlesDownloadDialog(&frame)
    //, m_wndSubtitlesUploadDialog(&frame)
{
}

CSubtitleController::~CSubtitleController()
{
}

static bool EnsureDirectory(CString directory)
{
    int ret = SHCreateDirectoryEx(nullptr, directory, nullptr);
    bool result = ret == ERROR_SUCCESS || ret == ERROR_ALREADY_EXISTS;
    if (!result) {
        AfxMessageBox(_T("Cannot create directory: ") + directory, MB_ICONEXCLAMATION | MB_OK);
    }
    return result;
}

void CSubtitleController::OnUpdateSubtitlesButton(CCmdUI* pCmdUI) {
    pCmdUI->Enable(m_frame.IsStateLoaded() && m_frame.m_subtitlesMenu.GetMenuItemCount() > 0);
}

void CSubtitleController::OnStreamSub(UINT nID)
{
    nID -= ID_STREAM_SUB_NEXT;
    if (m_frame.GetLoadState() != MLS::LOADED) {
        return;
    }

    if (!m_pSubStreams.IsEmpty()) {
        AfxGetAppSettings().fEnableSubtitles = true;
        SetSubtitle(nID == 0 ? 1 : -1, true, true);
        m_frame.SetFocus();
    } else if (m_frame.GetPlaybackMode() == PM_FILE) {
        m_frame.OnStreamSelect(nID == 0, 2);
    } else if (m_frame.GetPlaybackMode() == PM_DVD) {
        m_frame.SendMessage(WM_COMMAND, ID_DVD_SUB_NEXT + nID);
    }
}

void CSubtitleController::OnStreamSubOnOff()
{
    if (m_frame.GetLoadState() != MLS::LOADED) {
        return;
    }

    if (m_graph.m_pCAP && !m_pSubStreams.IsEmpty() || m_pDVS) {
        ToggleSubtitleOnOff(true);
        m_frame.SetFocus();
    } else if (m_frame.GetPlaybackMode() == PM_DVD) {
        m_frame.SendMessage(WM_COMMAND, ID_DVD_SUB_ONOFF);
    }
}

void CSubtitleController::OnSubtitlesAutoCopy()
{
    CAppSettings& s = AfxGetAppSettings();
    s.bAutoCopySubtitleToClipboard = !s.bAutoCopySubtitleToClipboard;
    ResetAutoCopySubtitle();
    m_frame.m_OSD.DisplayMessage(OSD_TOPLEFT, ResStr(s.bAutoCopySubtitleToClipboard ? IDS_OSD_AUTOCOPY_SUBTITLE_ON : IDS_OSD_AUTOCOPY_SUBTITLE_OFF));
}

void CSubtitleController::OnFileSubtitlesLoad()
{
    if (!m_graph.m_pCAP && !m_pDVS) {
        AfxMessageBox(IDS_CANNOT_LOAD_SUB, MB_ICONINFORMATION | MB_OK, 0);
        return;
    }

    DWORD dwFlags = OFN_EXPLORER | OFN_ALLOWMULTISELECT | OFN_ENABLESIZING | OFN_NOCHANGEDIR;
    if (!AfxGetAppSettings().fKeepHistory) {
        dwFlags |= OFN_DONTADDTORECENT;
    }
    CString filters;
    filters.Format(_T("%s|*.srt;*.sub;*.ssa;*.ass;*.smi;*.psb;*.txt;*.idx;*.usf;*.xss;*.rt;*.sup;*.webvtt;*.vtt|%s"),
                   ResStr(IDS_SUBTITLE_FILES_FILTER).GetString(), ResStr(IDS_ALL_FILES_FILTER).GetString());

    CFileDialog fd(TRUE, nullptr, nullptr, dwFlags, filters, m_frame.GetModalParent());

    OPENFILENAME& ofn = fd.GetOFN();
    // Provide a buffer big enough to hold 16 paths (which should be more than enough).
    // Only the old style dialog falls back to it, GetSelectedPaths() normally reads the
    // selection straight from the shell interface.
    const int nBufferSize = 16 * (MAX_PATH + 1) + 1;
    CString filenames;
    ofn.lpstrFile = filenames.GetBuffer(nBufferSize);
    ofn.nMaxFile = nBufferSize;
    // Set the current file directory as default folder
    CString curfile = m_frame.m_wndPlaylistBar.GetCurFileName();
    CLongPath defaultDir; // must outlive DoModal(), ofn.lpstrInitialDir points into it
    if (!PathUtils::IsURL(curfile)) {
        ExtendMaxPathLengthIfNeeded(curfile, true);
        defaultDir = curfile.GetString();
        defaultDir.RemoveFileSpec();
        if (!defaultDir.m_strPath.IsEmpty() && defaultDir.IsDirectory()) {
            ofn.lpstrInitialDir = defaultDir.m_strPath;
        }
    }

    if (fd.DoModal() == IDOK) {
        bool bFirstFile = true;
        CAtlList<CString> subfiles;
        FileDialogUtils::GetSelectedPaths(fd, subfiles);
        POSITION pos = subfiles.GetHeadPosition();
        while (pos) {
            const CString& subfile = subfiles.GetNext(pos);
            if (m_pDVS) {
                if (SUCCEEDED(m_pDVS->put_FileName((LPWSTR)(LPCWSTR)subfile))) {
                    m_pDVS->put_SelectedLanguage(0);
                    m_pDVS->put_HideSubtitles(true);
                    m_pDVS->put_HideSubtitles(false);
                    break;
                }
            } else {
                SubtitleInput subInput;
                if (LoadSubtitle(subfile, &subInput) && bFirstFile) {
                    bFirstFile = false;
                    // Use the subtitles file that was just added
                    AfxGetAppSettings().fEnableSubtitles = true;
                    SetSubtitle(subInput);
                }
            }
        }
    }
}

void CSubtitleController::OnUpdateFileSubtitlesLoad(CCmdUI* pCmdUI)
{
    pCmdUI->Enable(!m_frame.m_fAudioOnly && (m_graph.m_pCAP || m_pDVS) && m_frame.GetLoadState() == MLS::LOADED && !m_frame.IsPlaybackCaptureMode());
}

void CSubtitleController::SubtitlesSave(const TCHAR* directory, bool silent)
{
    if (m_frame.GetLoadState() != MLS::LOADED) {
        return;
    }
    if (m_frame.GetPlaybackMode() != PM_FILE && m_frame.GetPlaybackMode() != PM_DVD) {
        return;
    }

    int i = 0;
    SubtitleInput* pSubInput = GetSubtitleInput(i, true);
    if (!pSubInput || !pSubInput->pSubStream) {
        return;
    }

    CLSID clsid;
    if (FAILED(pSubInput->pSubStream->GetClassID(&clsid))) {
        return;
    }

    bool format_rts    = (clsid == __uuidof(CRenderedTextSubtitle));
    bool format_vobsub = (clsid == __uuidof(CVobSubFile));
    if (!format_rts && !format_vobsub) {
        AfxMessageBox(_T("This operation is not supported.\r\nThe current subtitle can not be saved."), MB_ICONEXCLAMATION | MB_OK);
    }

    CString suggestedFileName;
    if (m_frame.lastOpenFile.IsEmpty() || PathUtils::IsURL(m_frame.lastOpenFile)) {
        if (silent) {
            return;
        }
        suggestedFileName = _T("subtitle");
    } else {
        CLongPath path(m_frame.lastOpenFile);
        path.RemoveExtension();
        suggestedFileName = CString(path);
    }

    if (directory && *directory) {
        CLongPath suggestedPath(suggestedFileName);
        int pos = suggestedPath.FindFileName();
        CString fileName = suggestedPath.m_strPath.Mid(pos);
        CLongPath dirPath(directory);
        if (dirPath.IsRelative()) {
            dirPath = CLongPath(suggestedPath.m_strPath.Left(pos)) += dirPath;
        }
        if (EnsureDirectory(dirPath)) {
            suggestedFileName = CString(dirPath += fileName);
        }
        else if (silent) {
            return;
        }
    }

    CAppSettings& s = AfxGetAppSettings();
    bool isSaved = false;
    if (format_vobsub) {
        CVobSubFile* pVSF = (CVobSubFile*)(ISubStream*)pSubInput->pSubStream;

        // remember to set lpszDefExt to the first extension in the filter so that the save dialog autocompletes the extension
        // and tracks attempts to overwrite in a graceful manner
        if (silent) {
            isSaved = pVSF->Save(suggestedFileName + _T(".idx"), m_graph.m_pCAP->GetSubtitleDelay());
        } else {
            CSaveSubtitlesFileDialog fd(m_graph.m_pCAP->GetSubtitleDelay(), _T("idx"), suggestedFileName,
                _T("VobSub (*.idx, *.sub)|*.idx;*.sub||"), m_frame.GetModalParent());

            if (fd.DoModal() == IDOK) {
                CAutoLock cAutoLock(&m_csSubLock);
                isSaved = pVSF->Save(fd.GetPathName(), fd.GetDelay());
            }
        }
    }
    else if (format_rts) {
        CRenderedTextSubtitle* pRTS = (CRenderedTextSubtitle*)(ISubStream*)pSubInput->pSubStream;

        if (s.bAddLangCodeWhenSaveSubtitles && pRTS->m_lcid && pRTS->m_lcid != LCID(-1)) {
            CString str;
            GetLocaleString(pRTS->m_lcid, LOCALE_SISO639LANGNAME, str);
            suggestedFileName += _T('.') + str;

            if (pRTS->m_eHearingImpaired == Subtitle::HI_YES) {
                suggestedFileName += _T(".hi");
            }
        }

        // same thing as in the case of CVobSubFile above for lpszDefExt
        if (silent) {
            Subtitle::SubType type;
            switch (pRTS->m_subtitleType)
            {
                case Subtitle::ASS:
                case Subtitle::SSA:
                case Subtitle::VTT:
                    type = Subtitle::ASS;
                    break;
                default:
                    type = Subtitle::SRT;
            }

            isSaved = pRTS->SaveAs(
                suggestedFileName, type, m_graph.m_pCAP->GetFPS(), m_graph.m_pCAP->GetSubtitleDelay(),
                pRTS->m_encoding, s.bSubSaveExternalStyleFile);
        } else {
            const std::vector<Subtitle::SubType> types = {
                Subtitle::SRT,
                Subtitle::SUB,
                Subtitle::SMI,
                Subtitle::PSB,
                Subtitle::SSA,
                Subtitle::ASS
            };

            CString filter;
            filter += _T("SubRip (*.srt)|*.srt|"); //1 = default
            filter += _T("MicroDVD (*.sub)|*.sub|"); //2
            filter += _T("SAMI (*.smi)|*.smi|"); //3
            filter += _T("PowerDivX (*.psb)|*.psb|"); //4
            filter += _T("SubStation Alpha (*.ssa)|*.ssa|"); //5
            filter += _T("Advanced SubStation Alpha (*.ass)|*.ass|"); //6
            filter += _T("|");

            CSaveSubtitlesFileDialog fd(pRTS->m_encoding, m_graph.m_pCAP->GetSubtitleDelay(), s.bSubSaveExternalStyleFile,
                                        _T("srt"), suggestedFileName, filter, types, m_frame.GetModalParent());

            if (pRTS->m_subtitleType == Subtitle::SSA || pRTS->m_subtitleType == Subtitle::ASS) {
                fd.m_ofn.nFilterIndex = 6; //nFilterIndex is 1-based
            }

            if (fd.DoModal() == IDOK) {
                CAutoLock cAutoLock(&m_csSubLock);
                s.bSubSaveExternalStyleFile = fd.GetSaveExternalStyleFile();
                isSaved = pRTS->SaveAs(fd.GetPathName(), types[fd.m_ofn.nFilterIndex - 1], m_graph.m_pCAP->GetFPS(), fd.GetDelay(), fd.GetEncoding(), fd.GetSaveExternalStyleFile());
            }
        }
    }

    if (isSaved && s.fKeepHistory) {
        auto subPath = pSubInput->pSubStream->GetPath();
        if (!subPath.IsEmpty()) {
            s.MRU.AddSubToCurrent(subPath);
        }
    }
}

// Guards against copying typesetting rather than dialogue; generous enough for
// ordinary subtitles, which are a handful of lines at most.
static const int MAX_AUTOCOPY_SUBTITLE_LINES = 8;
static const int MAX_AUTOCOPY_SUBTITLE_LENGTH = 1000;
// No line of dialogue is on screen for less time than this, while animation and
// karaoke effects are built from events that are much shorter.
static const REFERENCE_TIME MIN_AUTOCOPY_SUBTITLE_DURATION = 250 * 10000i64;

static CStringW StripMarkupTags(CStringW str)
{
    int i = 0;
    while ((i = str.Find(L'<', i)) >= 0) {
        // only remove spans that look like markup tags, e.g. <i> or </font>
        WCHAR c = i + 1 < str.GetLength() ? str[i + 1] : L'\0';
        if (c != L'/' && !iswalpha(c)) {
            i++;
            continue;
        }
        int j = str.Find(L'>', i + 1);
        if (j < 0) {
            break;
        }
        str.Delete(i, j - i + 1);
    }
    return str;
}

// Copies the subtitle shown at rtNow to the clipboard and returns the time the
// caller should call again: the start of the next subtitle segment, since
// nothing can change before then.
REFERENCE_TIME CSubtitleController::CopyCurrentSubtitleToClipboard(REFERENCE_TIME rtNow)
{
    const CAppSettings& s = AfxGetAppSettings();
    ASSERT(s.bAutoCopySubtitleToClipboard);
    if (!s.fEnableSubtitles || !m_graph.m_pCAP) {
        return rtNow;
    }

    double fps = m_graph.m_pCAP->GetFPS();
    if (fps <= 0.0) {
        fps = 25.0;
    }
    const REFERENCE_TIME rtDelay = (REFERENCE_TIME)m_graph.m_pCAP->GetSubtitleDelay() * 10000; // ms -> reference time
    const REFERENCE_TIME rtSub = rtNow - rtDelay;

    CStringW strText;
    REFERENCE_TIME rtNext;
    {
        CAutoLock cAutoLock(&m_csSubLock);

        auto pRTS = dynamic_cast<CRenderedTextSubtitle*>((ISubStream*)m_pCurrentSubInput.pSubStream);
        if (!pRTS) {
            return rtNow;
        }

        int iSegment = -1, nSegments = 0;
        const STSSegment* pSeg = pRTS->SearchSubs(rtSub, fps, &iSegment, &nSegments);

        int iNext;
        if (pSeg) {
            iNext = iSegment + 1;
        } else {
            // between or before segments: find the first one starting after now
            int lo = 0, hi = nSegments;
            while (lo < hi) {
                int mid = (lo + hi) / 2;
                if (pRTS->TranslateSegmentStart(mid, fps) > rtSub) {
                    hi = mid;
                } else {
                    lo = mid + 1;
                }
            }
            iNext = lo;
        }
        if (iNext < nSegments) {
            rtNext = pRTS->TranslateSegmentStart(iNext, fps) + rtDelay;
        } else if (m_pCurrentSubInput.pSourceFilter) {
            rtNext = rtNow; // embedded subtitles: more can still be added while demuxing
        } else {
            rtNext = _I64_MAX;
        }

        if (!pSeg) {
            m_nLastCopiedSubSegment = -1;
            return rtNext;
        }
        if (iSegment == m_nLastCopiedSubSegment) {
            return rtNext;
        }
        m_nLastCopiedSubSegment = iSegment;

        // Typeset and karaoke effects can put hundreds of fragments on screen
        // at the same instant, often a single character each. That is artwork
        // rather than dialogue, and copying it only fills the clipboard with
        // noise. Judge the text that results rather than the number of events,
        // because a heavily typeset scene is mostly positioning and drawing
        // commands that carry no text at all.
        int nLines = 0;
        for (size_t i = 0; i < pSeg->subs.GetCount(); i++) {
            int subIndex = pSeg->subs[i];
            if (subIndex < 0 || subIndex >= (int)pRTS->GetCount()) {
                continue;
            }
            if (pRTS->TranslateEnd(subIndex, fps) - pRTS->TranslateStart(subIndex, fps) < MIN_AUTOCOPY_SUBTITLE_DURATION) {
                continue;
            }
            CStringW line = StripMarkupTags(pRTS->GetStrW(subIndex));
            line.Trim();
            if (line.IsEmpty()) {
                continue;
            }
            if (++nLines > MAX_AUTOCOPY_SUBTITLE_LINES
                    || strText.GetLength() + line.GetLength() > MAX_AUTOCOPY_SUBTITLE_LENGTH) {
                strText.Empty();
                break;
            }
            if (!strText.IsEmpty()) {
                strText += L'\n';
            }
            strText += line;
        }
    }

    // Clipboard access can block on other applications, keep it outside of m_csSubLock
    if (!strText.IsEmpty()) {
        strText.Replace(L"\n", L"\r\n");
        CClipboard clipboard(&m_frame);
        clipboard.SetText(CString(strText));
    }

    return rtNext;
}

void CSubtitleController::OnUpdateFileSubtitlesSave(CCmdUI* pCmdUI)
{
    bool bEnable = false;

    if (!m_pCurrentSubInput.pSourceFilter) {
        if (auto pRTS = dynamic_cast<CRenderedTextSubtitle*>((ISubStream*)m_pCurrentSubInput.pSubStream)) {
            bEnable = !pRTS->IsEmpty();
        } else if (dynamic_cast<CVobSubFile*>((ISubStream*)m_pCurrentSubInput.pSubStream)) {
            bEnable = true;
        }
    }

    pCmdUI->Enable(bEnable);
}

#if 0
void CSubtitleController::OnFileSubtitlesUpload()
{
    m_wndSubtitlesUploadDialog.ShowWindow(SW_SHOW);
}

void CSubtitleController::OnUpdateFileSubtitlesUpload(CCmdUI* pCmdUI)
{
    const CAppSettings& s = AfxGetAppSettings();
    pCmdUI->Enable(!m_pSubStreams.IsEmpty() && s.fEnableSubtitles);
}
#endif

void CSubtitleController::OnFileSubtitlesDownload()
{
    if (!m_frame.m_fAudioOnly) {
        if (m_graph.m_pCAP && AfxGetAppSettings().IsISRAutoLoadEnabled()) {
            m_wndSubtitlesDownloadDialog.ShowWindow(SW_SHOW);
        } else {
            AfxMessageBox(_T("Downloading subtitles only works when using the internal subtitle renderer."), MB_ICONINFORMATION | MB_OK, 0);
        }
    }
}

void CSubtitleController::OnUpdateFileSubtitlesDownload(CCmdUI* pCmdUI)
{
    pCmdUI->Enable(m_frame.GetLoadState() == MLS::LOADED && !m_frame.IsPlaybackCaptureMode() && m_graph.m_pCAP && !m_frame.m_fAudioOnly);
}

void CSubtitleController::OnViewSubresync()
{
    m_frame.m_controls.ToggleControl(CMainFrameControls::Panel::SUBRESYNC);
    m_frame.AdjustStreamPosPoller(true);
}

void CSubtitleController::OnUpdateViewSubresync(CCmdUI* pCmdUI)
{
    pCmdUI->SetCheck(m_frame.m_controls.ControlChecked(CMainFrameControls::Panel::SUBRESYNC));
    bool enabled = m_graph.m_pCAP && m_pCurrentSubInput.pSubStream && !m_frame.IsPlaybackCaptureMode();
    if (enabled) {
        CLSID clsid;
        m_pCurrentSubInput.pSubStream->GetClassID(&clsid);
        if (clsid == __uuidof(CRenderedTextSubtitle)) {
#if USE_LIBASS
            CRenderedTextSubtitle* pRTS = (CRenderedTextSubtitle*)(ISubStream*)m_pCurrentSubInput.pSubStream;
            enabled = !pRTS->m_LibassContext.IsLibassActive();
#endif
        } else {
            enabled = false;
        }
    }
    pCmdUI->Enable(enabled);
}

void CSubtitleController::SetSubtitleDelay(int delay_ms, bool relative)
{
    if (!m_graph.m_pCAP && !m_pDVS) {
        if (m_frame.GetLoadState() == MLS::LOADED) {
            m_frame.SendStatusMessage(L"Delay is not supported by current subtitle renderer", 3000, true);
        }
        return;
    }

    ResetAutoCopySubtitle(); // the new delay moves the segment boundaries

    if (m_pDVS) {
        int currentDelay, speedMul, speedDiv;
        if (FAILED(m_pDVS->get_SubtitleTiming(&currentDelay, &speedMul, &speedDiv))) {
            return;
        }
        if (relative) {
            delay_ms += currentDelay;
        }

        VERIFY(SUCCEEDED(m_pDVS->put_SubtitleTiming(delay_ms, speedMul, speedDiv)));
    }
    else {
        ASSERT(m_graph.m_pCAP != nullptr);
        if (m_pSubStreams.IsEmpty()) {
            m_frame.SendStatusMessage(StrRes(IDS_SUBTITLES_ERROR), 3000, true);
            return;
        }
        if (relative) {
            delay_ms += m_graph.m_pCAP->GetSubtitleDelay();
        }

        m_graph.m_pCAP->SetSubtitleDelay(delay_ms);
    }

    CString strSubDelay;
    strSubDelay.Format(IDS_MAINFRM_139, delay_ms);
    m_frame.SendStatusMessage(strSubDelay, 3000);
    m_frame.m_OSD.DisplayMessage(OSD_TOPLEFT, strSubDelay);
}

int CSubtitleController::GetSelectedSubtitleTrackIndex() {
    int subIdx = 0;
    POSITION pos = m_pSubStreams.GetHeadPosition();
    while (pos) {
        SubtitleInput& subInput = m_pSubStreams.GetNext(pos);
        CComQIPtr<IAMStreamSelect> pSSF = subInput.pSourceFilter;
        if (pSSF) {
            DWORD cStreams;
            if (SUCCEEDED(pSSF->Count(&cStreams))) {
                for (long j = 0; j < (long)cStreams; j++) {
                    DWORD dwFlags, dwGroup;
                    if (SUCCEEDED(pSSF->Info(j, nullptr, &dwFlags, nullptr, &dwGroup, nullptr, nullptr, nullptr))) {
                        if (dwGroup == 2) {
                            if (subInput.pSubStream == m_pCurrentSubInput.pSubStream && dwFlags & (AMSTREAMSELECTINFO_ENABLED | AMSTREAMSELECTINFO_EXCLUSIVE)) {
                                return subIdx;
                            }
                            subIdx++;
                        }
                    }
                }
            }
        } else {
            if (subInput.pSubStream == m_pCurrentSubInput.pSubStream) {
                return subIdx;
            } else {
                subIdx += subInput.pSubStream->GetStreamCount();
            }
        }
    }
    return 0;
}

bool CSubtitleController::IsValidSubtitleStream(int i) {
    if (GetSubtitleInput(i) != nullptr) {
        return true;
    }

    return false;
}

void CSubtitleController::OnSubtitlesDefaultStyle()
{
    CAppSettings& s = AfxGetAppSettings();
    if (!m_pSubStreams.IsEmpty()) {
        s.bSubtitleOverrideDefaultStyle = !s.bSubtitleOverrideDefaultStyle;
        UpdateSubtitleRenderingParameters();
        m_frame.RepaintVideo();
    }
}

void CSubtitleController::OnSubtitlesOverrideStyles()
{
    CAppSettings& s = AfxGetAppSettings();
    if (!m_pSubStreams.IsEmpty()) {
        s.bSubtitleOverrideAllStyles = !s.bSubtitleOverrideAllStyles;
        UpdateSubtitleRenderingParameters();
        m_frame.RepaintVideo();
    }
}

void CSubtitleController::OnPlaySubtitles(UINT nID)
{
    CAppSettings& s = AfxGetAppSettings();
    int i = (int)nID - ID_SUBTITLES_SUBITEM_START;

    if (m_frame.GetPlaybackMode() == PM_DVD) {
        ULONG ulStreamsAvailable, ulCurrentStream;
        BOOL bIsDisabled;
        if (SUCCEEDED(m_graph.m_pDVDI->GetCurrentSubpicture(&ulStreamsAvailable, &ulCurrentStream, &bIsDisabled))
                && ulStreamsAvailable) {
            if (i == 0) {
                m_graph.m_pDVDC->SetSubpictureState(bIsDisabled, DVD_CMD_FLAG_Block, nullptr);
            } else if (i <= int(ulStreamsAvailable)) {
                m_graph.m_pDVDC->SelectSubpictureStream(i - 1, DVD_CMD_FLAG_Block, nullptr);
                m_graph.m_pDVDC->SetSubpictureState(TRUE, DVD_CMD_FLAG_Block, nullptr);
            }
            i -= ulStreamsAvailable + 1;
        }
    }

    if (m_frame.GetPlaybackMode() == PM_DIGITAL_CAPTURE) {
        if (CBDAChannel* pChannel = m_frame.m_capture.m_pDVBState->pChannel) {
            m_frame.OnNavStreamSelectSubMenu(i, 2);
            pChannel->SetDefaultSubtitle(i);
            SetSubtitle(i);
        }
    } else if (!m_pSubStreams.IsEmpty()) {
        // Currently the subtitles menu contains 6 items apart from the actual subtitles list when the ISR is used
        i -= 6;

        if (i == -6) {
            // options
            m_frame.ShowOptions(CPPageSubtitles::IDD);
        } else if (i == -5) {
            // styles
            int j = 0;
            SubtitleInput* pSubInput = GetSubtitleInput(j, true);
            CLSID clsid;

            if (pSubInput && SUCCEEDED(pSubInput->pSubStream->GetClassID(&clsid))) {
                if (clsid == __uuidof(CRenderedTextSubtitle)) {
                    CRenderedTextSubtitle* pRTS = (CRenderedTextSubtitle*)(ISubStream*)pSubInput->pSubStream;

                    CAutoPtrArray<CPPageSubStyle> pages;
                    CAtlArray<STSStyle*> styles;

                    POSITION pos = pRTS->m_styles.GetStartPosition();
                    for (int k = 0; pos; k++) {
                        CString styleName;
                        STSStyle* style;
                        pRTS->m_styles.GetNextAssoc(pos, styleName, style);

                        CAutoPtr<CPPageSubStyle> page(DEBUG_NEW CPPageSubStyle(/*isStyleDialog = */ true));
                        if (style->hasAnsiStyleName) {
                            styleName = ToUnicode(styleName, pRTS->GetCharSet(style->charSet));
                        }
                        page->InitStyle(styleName, *style);
                        pages.Add(page);
                        styles.Add(style);
                    }

                    CMPCThemePropertySheet dlg(IDS_SUBTITLES_STYLES_CAPTION, m_frame.GetModalParent());
                    for (size_t l = 0; l < pages.GetCount(); l++) {
                        dlg.AddPage(pages[l]);
                    }

                    if (dlg.DoModal() == IDOK) {
                        {
                            bool defaultStyleChanged = false, otherStyleChanged = false;
                            {
                                CAutoLock cAutoLock(&m_csSubLock);

                                for (size_t l = 0; l < pages.GetCount(); l++) {
                                    STSStyle tmpStyle = *styles[l];
                                    pages[l]->GetStyle(*styles[l]);
                                    if (pages[l]->GetStyleName() == L"Default") {
                                        if (*styles[l] != s.subtitlesDefStyle) {
                                            pRTS->m_bUsingPlayerDefaultStyle = false;
                                            pRTS->SetDefaultStyle(*styles[l]);
                                            defaultStyleChanged = true;
                                        }
                                    } else if (tmpStyle != *styles[l]) {
                                        otherStyleChanged = true;
                                    }
                                }
                                if (otherStyleChanged || defaultStyleChanged) {
                                    if (!defaultStyleChanged) { //it will already have triggered SetStyleChanged() internally
                                        pRTS->SetStyleChanged();
                                    }
                                    pRTS->Deinit();
                                    InvalidateSubtitle();
                                    m_frame.m_wndSubresyncBar.ReloadSubtitle();
                                }
                            }
                            if (otherStyleChanged || defaultStyleChanged) {
                                m_frame.RepaintVideo();
                            }
                        }
                    }
                }
            }
        } else if (i == -4) {
            // reload
            ReloadSubtitle();
        } else if (i == -3) {
            // hide
            ToggleSubtitleOnOff();
        } else if (i == -2) {
            // override default style
            s.bSubtitleOverrideDefaultStyle = !s.bSubtitleOverrideDefaultStyle;
            UpdateSubtitleRenderingParameters();
            m_frame.RepaintVideo();
        } else if (i == -1) {
            // override all styles
            s.bSubtitleOverrideAllStyles = !s.bSubtitleOverrideAllStyles;
            UpdateSubtitleRenderingParameters();
            m_frame.RepaintVideo();
        } else if (i >= 0) {
            // this is an actual item from the subtitles list
            s.fEnableSubtitles = true;
            SetSubtitle(i);
        }
    } else if (m_frame.GetPlaybackMode() == PM_FILE) {
        m_frame.OnNavStreamSelectSubMenu(i, 2);
    }
}

void CSubtitleController::OnPlaySecondarySubtitle(UINT nID)
{
    int i = (int)nID - ID_SUBTITLES_SECONDARY_SUBITEM_START;

    if (i == 0) {
        // the "Off" entry
        SetSecondarySubtitle(SubtitleInput(nullptr));
    } else if (SubtitleInput* pSubInput = GetSecondarySubtitleInput(i - 1)) {
        AfxGetAppSettings().fEnableSubtitles = true;
        SetSecondarySubtitle(*pSubInput);
    }
}

void CSubtitleController::OnSecondarySubtitleLoad()
{
    if (!m_graph.m_pCAP || m_frame.GetLoadState() != MLS::LOADED || m_frame.m_fAudioOnly) {
        return;
    }

    DWORD dwFlags = OFN_EXPLORER | OFN_ENABLESIZING | OFN_NOCHANGEDIR;
    if (!AfxGetAppSettings().fKeepHistory) {
        dwFlags |= OFN_DONTADDTORECENT;
    }
    CString filters;
    filters.Format(_T("%s|*.srt;*.sub;*.smi;*.psb;*.txt;*.rt;*.webvtt;*.vtt|%s"),
                   ResStr(IDS_SUBTITLE_FILES_FILTER).GetString(), ResStr(IDS_ALL_FILES_FILTER).GetString());

    CFileDialog fd(TRUE, nullptr, nullptr, dwFlags, filters, m_frame.GetModalParent());

    OPENFILENAME& ofn = fd.GetOFN();
    // Set the current file directory as default folder
    CString curfile = m_frame.m_wndPlaylistBar.GetCurFileName();
    CLongPath defaultDir; // must outlive DoModal(), ofn.lpstrInitialDir points into it
    if (!PathUtils::IsURL(curfile)) {
        ExtendMaxPathLengthIfNeeded(curfile, true);
        defaultDir = curfile.GetString();
        defaultDir.RemoveFileSpec();
        if (!defaultDir.m_strPath.IsEmpty() && defaultDir.IsDirectory()) {
            ofn.lpstrInitialDir = defaultDir.m_strPath;
        }
    }

    if (fd.DoModal() != IDOK) {
        return;
    }

    SubtitleInput subInput;
    if (!LoadSubtitle(fd.GetPathName(), &subInput) || !IsEligibleSecondarySubtitle(subInput)) {
        AfxMessageBox(IDS_SUBTITLES_SECONDARY_BAD_FORMAT, MB_ICONINFORMATION | MB_OK, 0);
        return;
    }
    AfxGetAppSettings().fEnableSubtitles = true;
    SetSecondarySubtitle(subInput);
}

// Called from GraphThread
int CSubtitleController::SetupSubtitleStreams()
{
    const CAppSettings& s = AfxGetAppSettings();

    int selected = -1;

    if (!m_pSubStreams.IsEmpty()) {
        bool is_external = false;
        bool externalPriority = false;
        bool has_off_lang = false;
        std::list<ISOLangT<CString>> langs;
        int tPos = 0;
        CString lang = s.strSubtitlesLanguageOrder.Tokenize(_T(",; "), tPos);
        while (tPos != -1) {
            lang.MakeLower();
            ISOLangT<CString> l = ISOLangT<CString>(lang, L"", L"", 0);

            if (lang == _T("off")) {
                has_off_lang = true;
            } else if (lang.Find(L'-') == 2) {
                // BCP 47
            } else {
                l = ISOLang::ISO639XToISOLang(CStringA(lang));
                if (l.name.IsEmpty()) { // not an ISO code
                    l.name = lang;
                } else {
                    l.name.MakeLower();
                }
            }

            langs.emplace_back(l);

            lang = s.strSubtitlesLanguageOrder.Tokenize(_T(",; "), tPos);
        }

        int i = 0;
        int maxrating = 0;
        POSITION pos = m_pSubStreams.GetHeadPosition();
        while (pos) {
            if (m_posFirstExtSub == pos) {
                is_external = true;
                externalPriority = s.fPrioritizeExternalSubtitles;
            }
            SubtitleInput& subInput = m_pSubStreams.GetNext(pos);
            CComPtr<ISubStream> pSubStream = subInput.pSubStream;
            CComQIPtr<IAMStreamSelect> pSSF = subInput.pSourceFilter;

            bool bAllowOverridingSplitterChoice;
            // If the internal LAV Splitter has its own language preferences set, we choose not to override its choice
            if (pSSF && CFGFilterLAV::IsInternalInstance(subInput.pSourceFilter)) {
                bAllowOverridingSplitterChoice = true;
                if (CComQIPtr<ILAVFSettings> pLAVFSettings = subInput.pSourceFilter) {
                    CComHeapPtr<WCHAR> pLangPrefs;
                    LAVSubtitleMode subtitleMode = pLAVFSettings->GetSubtitleMode();
                    if ((((subtitleMode == LAVSubtitleMode_Default && SUCCEEDED(pLAVFSettings->GetPreferredSubtitleLanguages(&pLangPrefs)))
                            || (subtitleMode == LAVSubtitleMode_Advanced && SUCCEEDED(pLAVFSettings->GetAdvancedSubtitleConfig(&pLangPrefs))))
                            && pLangPrefs && wcslen(pLangPrefs))
                            || subtitleMode == LAVSubtitleMode_ForcedOnly || subtitleMode == LAVSubtitleMode_NoSubs) {
                        bAllowOverridingSplitterChoice = false;
                    }
                }
            } else {
                bAllowOverridingSplitterChoice = s.bAllowOverridingExternalSplitterChoice;
            }

            int count = 0;
            if (pSSF) {
                DWORD cStreams;
                if (SUCCEEDED(pSSF->Count(&cStreams))) {
                    count = (int)cStreams;
                }
            } else {
                count = pSubStream->GetStreamCount();
            }

            for (int j = 0; j < count; j++) {
                CComHeapPtr<WCHAR> pName;
                LCID lcid = 0;
                int rating = 0;
                if (pSSF) {
                    DWORD dwFlags, dwGroup = 2;
                    pSSF->Info(j, nullptr, &dwFlags, &lcid, &dwGroup, &pName, nullptr, nullptr);
                    if (dwGroup != 2) { // If the track isn't a subtitle track, we skip it
                        continue;
                    } else if (dwFlags & (AMSTREAMSELECTINFO_ENABLED | AMSTREAMSELECTINFO_EXCLUSIVE)) {
                        // Give slightly higher priority to the track selected by splitter so that
                        // we won't override selected track in case all have the same rating.
                        rating += 1;
                    } else if (!bAllowOverridingSplitterChoice) {
                        // If we aren't allowed to modify the splitter choice and the current
                        // track isn't already selected at splitter level we need to skip it.
                        i++;
                        continue;
                    }
                } else {
                    pSubStream->GetStreamInfo(j, &pName, &lcid);
                }
                CString name(pName);
                name.Trim();
                name.MakeLower();

                size_t k = 0;
                for (const auto& l : langs) {
                    int num = _tstoi(l.name) - 1;
                    if (num >= 0) { // this is track number
                        if (i != num) {
                            k++;
                            continue;  // not matched
                        }
                    } else { // this is lang string
                        if (lcid == 0 || lcid == LCID(-1) || lcid != l.lcid) {
                            // no LCID match, analyze track name for language match
                            if (!MatchSubtrackWithISOLang(name, l)) {
                                k++;
                                continue; // not matched
                            }
                        }
                        // LCID match
                    }
                    rating += 16 * int(langs.size() - k);
                    break;
                }

                if (is_external) {
                    if (rating > 0) {
                        if (externalPriority) {
                            rating += 16 * int(langs.size() + 1);
                        }
                    } else {
                        if (langs.size() == 0 || name.Find(_T("\t")) == -1) {
                            // no preferred language or unknown sub language
                            if (externalPriority) {
                                rating += 16 * int(langs.size() + 1);
                            } else {
                                rating = 1;
                            }
                        }
                    }
                } else {
                    if (s.bPreferDefaultForcedSubtitles) {
                        if (name.Find(_T("[default,forced]")) != -1) { // for LAV Splitter
                            rating += 4 + 2;
                        }
                        if (name.Find(_T("[forced]")) != -1) {
                            rating += 2;
                        }
                        if (name.Find(_T("[default]")) != -1) {
                            rating += 4;
                        }
                    }
#if 0
                    if (rating == 0 && bAllowOverridingSplitterChoice && langs.size() == 0) {
                        // use first embedded track as fallback if there is no preferred language
                        rating = 1;
                    }
#endif
                }

                if (rating > maxrating) {
                    maxrating = rating;
                    selected = i;
                }
                i++;
            }
        }
    }

    if (s.IsISRAutoLoadEnabled() && !m_frame.m_fAudioOnly) {
        if (s.bAutoDownloadSubtitles && m_pSubStreams.IsEmpty()) {
            m_pSubtitlesProviders->Search(TRUE);
        } else if (m_wndSubtitlesDownloadDialog.IsWindowVisible()) {
            m_pSubtitlesProviders->Search(FALSE);
        }
    }

    return selected;
}

void CSubtitleController::SetupSubtitlesSubMenu()
{
    CMPCThemeMenu& subMenu = m_frame.m_subtitlesMenu;
    // Empty the menu
    while (subMenu.RemoveMenu(0, MF_BYPOSITION));

    if (m_frame.GetLoadState() != MLS::LOADED || m_frame.m_fAudioOnly) {
        return;
    }

    UINT id = ID_SUBTITLES_SUBITEM_START;

    // DVD subtitles in DVD mode are never handled by the internal subtitles renderer
    // but it is still possible to load external subtitles so we keep that if block
    // separated from the rest
    if (m_frame.GetPlaybackMode() == PM_DVD) {
        ULONG ulStreamsAvailable, ulCurrentStream;
        BOOL bIsDisabled;
        if (SUCCEEDED(m_graph.m_pDVDI->GetCurrentSubpicture(&ulStreamsAvailable, &ulCurrentStream, &bIsDisabled))
                && ulStreamsAvailable > 0) {
            LCID DefLanguage;
            DVD_SUBPICTURE_LANG_EXT ext;
            if (FAILED(m_graph.m_pDVDI->GetDefaultSubpictureLanguage(&DefLanguage, &ext))) {
                return;
            }

            VERIFY(subMenu.AppendMenu(MF_STRING | (bIsDisabled ? 0 : MF_CHECKED), id++, ResStr(IDS_DVD_SUBTITLES_ENABLE)));
            VERIFY(subMenu.AppendMenu(MF_SEPARATOR | MF_ENABLED));

            for (ULONG i = 0; i < ulStreamsAvailable; i++) {
                LCID Language;
                if (FAILED(m_graph.m_pDVDI->GetSubpictureLanguage(i, &Language))) {
                    continue;
                }

                UINT flags = MF_BYCOMMAND | MF_STRING | MF_ENABLED;
                if (Language == DefLanguage) {
                    flags |= MF_DEFAULT;
                }
                if (i == ulCurrentStream) {
                    flags |= MF_CHECKED;
                }

                CString str;
                if (Language) {
                    GetLocaleString(Language, LOCALE_SENGLANGUAGE, str);
                } else {
                    str.Format(IDS_AG_UNKNOWN, i + 1);
                }

                DVD_SubpictureAttributes ATR;
                if (SUCCEEDED(m_graph.m_pDVDI->GetSubpictureAttributes(i, &ATR))) {
                    switch (ATR.LanguageExtension) {
                        case DVD_SP_EXT_NotSpecified:
                        default:
                            break;
                        case DVD_SP_EXT_Caption_Normal:
                            str += _T("");
                            break;
                        case DVD_SP_EXT_Caption_Big:
                            str += _T(" (Big)");
                            break;
                        case DVD_SP_EXT_Caption_Children:
                            str += _T(" (Children)");
                            break;
                        case DVD_SP_EXT_CC_Normal:
                            str += _T(" (CC)");
                            break;
                        case DVD_SP_EXT_CC_Big:
                            str += _T(" (CC Big)");
                            break;
                        case DVD_SP_EXT_CC_Children:
                            str += _T(" (CC Children)");
                            break;
                        case DVD_SP_EXT_Forced:
                            str += _T(" (Forced)");
                            break;
                        case DVD_SP_EXT_DirectorComments_Normal:
                            str += _T(" (Director Comments)");
                            break;
                        case DVD_SP_EXT_DirectorComments_Big:
                            str += _T(" (Director Comments, Big)");
                            break;
                        case DVD_SP_EXT_DirectorComments_Children:
                            str += _T(" (Director Comments, Children)");
                            break;
                    }
                }

                VERIFY(m_frame.AppendMenuEx(subMenu, flags, id++, str));
            }
        }
    }

    POSITION pos = m_pSubStreams.GetHeadPosition();

    if (m_frame.GetPlaybackMode() == PM_DIGITAL_CAPTURE) {
        DWORD selected = m_frame.SetupNavStreamSelectSubMenu(subMenu, id, 2);
        if (selected != -1) {
            SetSubtitle(selected - ID_SUBTITLES_SUBITEM_START);
        }
    } else if (pos) { // Internal subtitles renderer
        int nItemsBeforeStart = id - ID_SUBTITLES_SUBITEM_START;
        if (nItemsBeforeStart > 0) {
            VERIFY(subMenu.AppendMenu(MF_SEPARATOR));
            nItemsBeforeStart += 2; // Separators
        }

        // Build the static menu's items
        bool bTextSubtitles = false;
        VERIFY(subMenu.AppendMenu(MF_STRING | MF_ENABLED, id++, ResStr(IDS_SUBTITLES_OPTIONS)));
        VERIFY(subMenu.AppendMenu(MF_STRING | MF_ENABLED, id++, ResStr(IDS_SUBTITLES_STYLES)));
        VERIFY(subMenu.AppendMenu(MF_STRING | MF_ENABLED, id++, ResStr(IDS_SUBTITLES_RELOAD)));
        VERIFY(subMenu.AppendMenu(MF_SEPARATOR));

        VERIFY(subMenu.AppendMenu(MF_STRING | MF_ENABLED, id++, ResStr(IDS_SUBTITLES_HIDE)));
        VERIFY(subMenu.AppendMenu(MF_STRING | MF_ENABLED, id++, ResStr(IDS_SUB_OVERRIDE_DEFAULT_STYLE)));
        VERIFY(subMenu.AppendMenu(MF_STRING | MF_ENABLED, id++, ResStr(IDS_SUB_OVERRIDE_ALL_STYLES)));
        VERIFY(subMenu.AppendMenu(MF_SEPARATOR));

        // Build the dynamic menu's items
        int i = 0, iSelected = -1;
        while (pos) {
            SubtitleInput& subInput = m_pSubStreams.GetNext(pos);

            if (CComQIPtr<IAMStreamSelect> pSSF = subInput.pSourceFilter) {
                DWORD cStreams;
                if (FAILED(pSSF->Count(&cStreams))) {
                    continue;
                }

                for (int j = 0, cnt = (int)cStreams; j < cnt; j++) {
                    DWORD dwFlags, dwGroup;
                    CComHeapPtr<WCHAR> pszName;
                    LCID lcid = 0;
                    if (FAILED(pSSF->Info(j, nullptr, &dwFlags, &lcid, &dwGroup, &pszName, nullptr, nullptr))
                            || !pszName) {
                        continue;
                    }

                    if (dwGroup != 2) {
                        continue;
                    }

                    if (subInput.pSubStream == m_pCurrentSubInput.pSubStream
                            && dwFlags & (AMSTREAMSELECTINFO_ENABLED | AMSTREAMSELECTINFO_EXCLUSIVE)) {
                        iSelected = i;
                    }

                    //a tab in a name coming from the splitter is part of the name, not a column
                    CString name = SanitizeMenuLabel(CString(pszName));
                    /*
                    CString lcname = CString(name).MakeLower();
                    if (lcname.Find(_T(" off")) >= 0) {
                        name.LoadString(IDS_AG_DISABLED);
                    }
                    */
                    if (lcid != 0) {
                        CString lcidstr;
                        GetLocaleString(lcid, LOCALE_SENGLANGUAGE, lcidstr);
                        name.Append(_T("\t") + lcidstr);
                    }

                    VERIFY(subMenu.AppendMenu(MF_STRING | MF_ENABLED, id++, name));
                    i++;
                }
            } else {
                CComPtr<ISubStream> pSubStream = subInput.pSubStream;
                if (!pSubStream) {
                    continue;
                }

                if (subInput.pSubStream == m_pCurrentSubInput.pSubStream) {
                    iSelected = i + pSubStream->GetStream();
                }

                for (int j = 0, cnt = pSubStream->GetStreamCount(); j < cnt; j++) {
                    CComHeapPtr<WCHAR> pName;
                    LCID lcid = 0;
                    if (SUCCEEDED(pSubStream->GetStreamInfo(j, &pName, &lcid))) {
                        CString name(pName);
                        if (lcid != 0 && name.Find(L'\t') < 0) {
                            CString lcidstr;
                            GetLocaleString(lcid, LOCALE_SENGLANGUAGE, lcidstr);
                            name.Append(_T("\t") + lcidstr);
                        }

                        name = SanitizeMenuLabel(name, MENU_NAME_MAX, true);
                        VERIFY(subMenu.AppendMenu(MF_STRING | MF_ENABLED, id++, name));
                    } else {
                        VERIFY(subMenu.AppendMenu(MF_STRING | MF_ENABLED, id++, ResStr(IDS_AG_UNKNOWN_STREAM)));
                    }
                    i++;
                }
            }

            if (subInput.pSubStream == m_pCurrentSubInput.pSubStream) {
                CLSID clsid;
                if (SUCCEEDED(subInput.pSubStream->GetClassID(&clsid))
                        && clsid == __uuidof(CRenderedTextSubtitle)) {
                    bTextSubtitles = true;
                }
            }

            // TODO: find a better way to group these entries
            /*if (pos && m_pSubStreams.GetAt(pos).subStream) {
                CLSID cur, next;
                pSubStream->GetClassID(&cur);
                m_pSubStreams.GetAt(pos).subStream->GetClassID(&next);

                if (cur != next) {
                    VERIFY(subMenu.AppendMenu(MF_SEPARATOR));
                }
            }*/
        }

        // Set the menu's items' state
        const CAppSettings& s = AfxGetAppSettings();
        // Style
        if (!bTextSubtitles) {
            subMenu.EnableMenuItem(nItemsBeforeStart + 1, MF_BYPOSITION | MF_GRAYED);
        }
        // Hide
        if (!s.fEnableSubtitles) {
            subMenu.CheckMenuItem(nItemsBeforeStart + 4, MF_BYPOSITION | MF_CHECKED);
        }
        // Style overrides
        if (!bTextSubtitles) {
            subMenu.EnableMenuItem(nItemsBeforeStart + 5, MF_BYPOSITION | MF_GRAYED);
            subMenu.EnableMenuItem(nItemsBeforeStart + 6, MF_BYPOSITION | MF_GRAYED);
        }
        if (s.bSubtitleOverrideDefaultStyle) {
            subMenu.CheckMenuItem(nItemsBeforeStart + 5, MF_BYPOSITION | MF_CHECKED);
        }
        if (s.bSubtitleOverrideAllStyles) {
            subMenu.CheckMenuItem(nItemsBeforeStart + 6, MF_BYPOSITION | MF_CHECKED);
        }
        if (iSelected >= 0) {
            VERIFY(subMenu.CheckMenuRadioItem(nItemsBeforeStart + 8, nItemsBeforeStart + 8 + i - 1, nItemsBeforeStart + 8 + iSelected, MF_BYPOSITION));
        }
    } else if (m_frame.GetPlaybackMode() == PM_FILE) {
        m_frame.SetupNavStreamSelectSubMenu(subMenu, id, 2);
    }
}

bool CSubtitleController::IsEligibleSecondarySubtitle(const SubtitleInput& subInput) const
{
    // Only externally loaded text subtitles can be used as the secondary track,
    // and never the stream that is currently selected as the primary one.
    // Embedded/IAMStreamSelect tracks are excluded because selecting one calls
    // IAMStreamSelect::Enable, which would disturb the primary selection.
    if (!subInput.pSubStream || subInput.pSourceFilter
            || subInput.pSubStream == m_pCurrentSubInput.pSubStream
            || std::find(m_ExternalSubstreams.cbegin(), m_ExternalSubstreams.cend(),
                         (ISubStream*)subInput.pSubStream) == m_ExternalSubstreams.cend()) {
        return false;
    }
    auto pRTS = dynamic_cast<CRenderedTextSubtitle*>((ISubStream*)subInput.pSubStream);
    if (!pRTS) {
        return false;
    }
    // Restrict to simple text formats: SSA/ASS and the XML-based formats can
    // carry their own positioning, which would collide with the primary track.
    switch (pRTS->m_subtitleType) {
        case Subtitle::SRT:
        case Subtitle::SUB:
        case Subtitle::SMI:
        case Subtitle::PSB:
        case Subtitle::TXT:
        case Subtitle::RT:
        case Subtitle::VTT:
            return true;
        default:
            return false;
    }
}

SubtitleInput* CSubtitleController::GetSecondarySubtitleInput(int idx)
{
    // Returns the idx-th (0-based) eligible secondary subtitle stream, or nullptr.
    if (idx >= 0) {
        POSITION pos = m_pSubStreams.GetHeadPosition();
        while (pos) {
            SubtitleInput& subInput = m_pSubStreams.GetNext(pos);
            if (IsEligibleSecondarySubtitle(subInput) && idx-- == 0) {
                return &subInput;
            }
        }
    }
    return nullptr;
}

void CSubtitleController::SetupSecondarySubtitleSubMenu()
{
    CMPCThemeMenu& subMenu = m_frame.m_subtitlesSecondaryMenu;
    // Empty the menu
    while (subMenu.RemoveMenu(0, MF_BYPOSITION));

    if (m_frame.GetLoadState() != MLS::LOADED || m_frame.m_fAudioOnly || !m_graph.m_pCAP) {
        return;
    }

    UINT id = ID_SUBTITLES_SECONDARY_SUBITEM_START;
    int i = 0, iSelected = -1;

    POSITION pos = m_pSubStreams.GetHeadPosition();
    while (pos) {
        SubtitleInput& subInput = m_pSubStreams.GetNext(pos);
        if (!IsEligibleSecondarySubtitle(subInput)) {
            continue;
        }
        if (i == 0) {
            VERIFY(subMenu.AppendMenu(MF_STRING | MF_ENABLED, id++, ResStr(IDS_AG_DISABLED)));
            VERIFY(subMenu.AppendMenu(MF_SEPARATOR));
        }
        // Never hand out a command id beyond the dispatched range
        if (id > ID_SUBTITLES_SECONDARY_SUBITEM_END) {
            break;
        }
        if (subInput.pSubStream == m_pSecondarySubInput.pSubStream) {
            iSelected = i;
        }

        CComHeapPtr<WCHAR> pName;
        CString name;
        if (SUCCEEDED(subInput.pSubStream->GetStreamInfo(0, &pName, nullptr)) && pName) {
            name = SanitizeMenuLabel(CString(pName), MENU_NAME_MAX, true);
        } else {
            name.LoadString(IDS_AG_UNKNOWN_STREAM);
        }
        VERIFY(subMenu.AppendMenu(MF_STRING | MF_ENABLED, id++, name)); // entry i is at menu position i + 2
        i++;
    }

    if (i > 0) {
        // Check the active entry, or "Disabled" (position 0) when no secondary is selected
        int checkPos = (iSelected >= 0) ? (iSelected + 2) : 0;
        VERIFY(subMenu.CheckMenuRadioItem(0, subMenu.GetMenuItemCount() - 1, checkPos, MF_BYPOSITION));
        VERIFY(subMenu.AppendMenu(MF_SEPARATOR));
    }

    // Load a subtitle file directly as the secondary track, without going
    // through the regular load path which changes the primary selection.
    VERIFY(subMenu.AppendMenu(MF_STRING | MF_ENABLED, ID_SUBTITLES_SECONDARY_LOAD, ResStr(IDS_AG_LOAD_SUBTITLES)));
}

void CSubtitleController::AddTextPassThruFilter()
{
    BeginEnumFilters(m_graph.m_pGB, pEF, pBF) {
        if (!IsSplitter(pBF)) {
            continue;
        }

        BeginEnumPins(pBF, pEP, pPin) {
            CComPtr<IPin> pPinTo;
            AM_MEDIA_TYPE mt;
            if (SUCCEEDED(pPin->ConnectedTo(&pPinTo)) && pPinTo
                    && SUCCEEDED(pPin->ConnectionMediaType(&mt))
                    && (mt.majortype == MEDIATYPE_Text || mt.majortype == MEDIATYPE_Subtitle)) {
                InsertTextPassThruFilter(pBF, pPin, pPinTo);
            }
        }
        EndEnumPins;
    }
    EndEnumFilters;
}

HRESULT CSubtitleController::InsertTextPassThruFilter(IBaseFilter* pBF, IPin* pPin, IPin* pPinTo)
{
    HRESULT hr;
    CComQIPtr<IBaseFilter> pTPTF = DEBUG_NEW CTextPassThruFilter(&m_frame);
    CStringW name;
    name.Format(L"TextPassThru%p", static_cast<void*>(pTPTF));
    if (FAILED(hr = m_graph.m_pGB->AddFilter(pTPTF, name))) {
        return hr;
    }

    OAFilterState fs = m_frame.GetMediaState();
    if (fs == State_Running || fs == State_Paused) {
        m_frame.MediaControlStop(true);
    }

    hr = pPinTo->Disconnect();
    hr = pPin->Disconnect();

    if (FAILED(hr = m_graph.m_pGB->ConnectDirect(pPin, GetFirstPin(pTPTF, PINDIR_INPUT), nullptr))
            || FAILED(hr = m_graph.m_pGB->ConnectDirect(GetFirstPin(pTPTF, PINDIR_OUTPUT), pPinTo, nullptr))) {
        hr = m_graph.m_pGB->ConnectDirect(pPin, pPinTo, nullptr);
    } else {
        SubtitleInput subInput(CComQIPtr<ISubStream>(pTPTF), pBF);
        m_pSubStreams.AddTail(subInput);
    }

    if (fs == State_Running) {
        m_frame.MediaControlRun();
    } else if (fs == State_Paused) {
        m_frame.MediaControlPause();
    }

    return hr;
}

bool CSubtitleController::LoadSubtitle(CString fn, SubtitleInput* pSubInput /*= nullptr*/, bool bAutoLoad /*= false*/)
{
    CAppSettings& s = AfxGetAppSettings();
    CComQIPtr<ISubStream> pSubStream;

    if (!s.IsISRAutoLoadEnabled() && (FindFilter(CLSID_VSFilter, m_graph.m_pGB) || FindFilter(CLSID_XySubFilter, m_graph.m_pGB))) {
        // Prevent ISR from loading if VSFilter is already in graph.
        // TODO: Support VSFilter natively (see ticket #4122)
        // Note that this doesn't affect ISR auto-loading if any sub renderer force loading itself into the graph.
        // VSFilter like filters can be blocked when building the graph and ISR auto-loading is enabled but some
        // users don't want that.
        return false;
    }

    if (m_frame.GetPlaybackMode() == PM_FILE && !s.fDisableInternalSubtitles && !FindFilter(__uuidof(CTextPassThruFilter), m_graph.m_pGB)) {
        // Add TextPassThru filter if it isn't already in the graph. (i.e ISR hasn't been loaded before)
        // This will load all embedded subtitle tracks when user triggers ISR (load external subtitle file) for the first time.
        AddTextPassThruFilter();
    }

    CString videoName;
    if (m_frame.GetPlaybackMode() == PM_FILE) {
        videoName = m_frame.m_wndPlaylistBar.GetCurFileName();
    }

    CString ext = CLongPath(fn).GetExtension().MakeLower();

    if (!pSubStream && (ext == _T(".idx") || !bAutoLoad && ext == _T(".sub"))) {
        CAutoPtr<CVobSubFile> pVSF(DEBUG_NEW CVobSubFile(&m_csSubLock));
        if (pVSF && pVSF->Open(fn) && pVSF->GetStreamCount() > 0) {
            pSubStream = pVSF.Detach();
        }
    }

    if (!pSubStream && ext != _T(".idx") && ext != _T(".sup")) {
        CAutoPtr<CRenderedTextSubtitle> pRTS(DEBUG_NEW CRenderedTextSubtitle(&m_csSubLock));
        if (pRTS) {
            if (pRTS->Open(fn, DEFAULT_CHARSET, _T(""), videoName) && pRTS->GetStreamCount() > 0) {
#if USE_LIBASS
                if (pRTS->m_LibassContext.IsLibassActive()) {
                    pRTS->m_LibassContext.SetFilterGraph(m_graph.m_pGB);
                }
#endif
                pSubStream = pRTS.Detach();
            }
        }
    }

    if (!pSubStream) {
        CAutoPtr<CPGSSubFile> pPSF(DEBUG_NEW CPGSSubFile(&m_csSubLock));
        if (pPSF && pPSF->Open(fn, _T(""), videoName) && pPSF->GetStreamCount() > 0) {
            pSubStream = pPSF.Detach();
        }
    }

    if (pSubStream) {
        SubtitleInput subInput(pSubStream);
        m_ExternalSubstreams.push_back(pSubStream);
        m_pSubStreams.AddTail(subInput);

        // Temporarily load fonts from 'Fonts' folder - Begin
        CString path = PathUtils::DirName(fn) + L"\\fonts\\";
        ExtendMaxPathLengthIfNeeded(path);

        if (::PathIsDirectory(path)) {
            WIN32_FIND_DATA fd = {0};
            HANDLE hFind;
            
            hFind = FindFirstFile(path + L"*.?t?", &fd);
            if (hFind != INVALID_HANDLE_VALUE) {
                do {
                    CStringW ext = GetFileExt(fd.cFileName);
                    if (ext == ".ttf" || ext == ".otf" || ext == ".ttc") {
                        m_frame.m_FontInstaller.InstallTempFontFile(path + fd.cFileName);
                    }
                } while (FindNextFile(hFind, &fd));
                
                FindClose(hFind);
            }
        }
        // Temporarily load fonts from 'Fonts' folder - End

        if (!m_posFirstExtSub) {
            m_posFirstExtSub = m_pSubStreams.GetTailPosition();
        }

        if (pSubInput) {
            *pSubInput = subInput;
        }

        if (!bAutoLoad) {
            m_frame.m_wndPlaylistBar.AddSubtitleToCurrent(fn);
            if (s.fKeepHistory) {
                s.MRU.AddSubToCurrent(fn);
            }
        }
    }

    return !!pSubStream;
}

bool CSubtitleController::LoadSubtitle(CYoutubeDLInstance::YDLSubInfo& sub) {
    CAppSettings& s = AfxGetAppSettings();
    CComQIPtr<ISubStream> pSubStream;
    CAtlList<CString> preferlist;
    if (!s.sYDLSubsPreference.IsEmpty()) {
        if (s.sYDLSubsPreference.Find(_T(',')) != -1) {
            ExplodeMin(s.sYDLSubsPreference, preferlist, ',');
        } else {
            ExplodeMin(s.sYDLSubsPreference, preferlist, ' ');
        }
    }
    if (!preferlist.IsEmpty() && !CYoutubeDLInstance::isPrefer(preferlist, sub.lang)) {
        return false;
    }

    if (!s.IsISRAutoLoadEnabled() && (FindFilter(CLSID_VSFilter, m_graph.m_pGB) || FindFilter(CLSID_XySubFilter, m_graph.m_pGB))) {
        // Prevent ISR from loading if VSFilter is already in graph.
        // TODO: Support VSFilter natively (see ticket #4122)
        // Note that this doesn't affect ISR auto-loading if any sub renderer force loading itself into the graph.
        // VSFilter like filters can be blocked when building the graph and ISR auto-loading is enabled but some
        // users don't want that.
        return false;
    }

    if (m_frame.GetPlaybackMode() == PM_FILE && !s.fDisableInternalSubtitles && !FindFilter(__uuidof(CTextPassThruFilter), m_graph.m_pGB)) {
        // Add TextPassThru filter if it isn't already in the graph. (i.e ISR hasn't been loaded before)
        // This will load all embedded subtitle tracks when user triggers ISR (load external subtitle file) for the first time.
        AddTextPassThruFilter();
    }

    CAutoPtr<CRenderedTextSubtitle> pRTS(DEBUG_NEW CRenderedTextSubtitle(&m_csSubLock));
    if (pRTS) {
        bool opened = false;
        if (!sub.url.IsEmpty()) {
            SubtitlesProvidersUtils::stringMap strmap{};
            DWORD dwStatusCode;
            CT2CA tem(sub.url);
            std::string tem2(tem);
            std::string data("");
            SubtitlesProvidersUtils::StringDownload(tem2, strmap, data, true, &dwStatusCode);
            if (dwStatusCode != 200) {
                return false;
            }
            if (sub.ext.IsEmpty()) {
                int m2(sub.url.ReverseFind(_T('?')));
                int m3(sub.url.ReverseFind(_T('#')));
                int m = -1;
                if (m2 > -1 && m3 > -1) m = std::min(m2, m3);
                else if (m2 > -1) m = m2;
                else if (m3 > -1) m = m3;
                CString temp(sub.url);
                if (m > 0) temp = sub.url.Left(m);
                m = temp.ReverseFind(_T('.'));
                if (m >= 0) sub.ext = temp.Mid(m + 1);
            }
            CString langt = sub.isAutomaticCaptions ? sub.lang + _T("[Automatic]") : sub.lang;
            opened = pRTS->Open((BYTE*)data.c_str(), (int)data.length(), DEFAULT_CHARSET, _T("YoutubeDL"), langt, sub.ext);
        } else if (!sub.data.IsEmpty()) {
            CString langt = sub.isAutomaticCaptions ? sub.lang + _T("[Automatic]") : sub.lang;
            opened = pRTS->Open(sub.data, CTextFile::enc::UTF8, DEFAULT_CHARSET, _T("YoutubeDL"), langt, sub.ext);  // Do not modify charset, Now it wroks with Unicode char.
        }
        if (opened && pRTS->GetStreamCount() > 0) {
#if USE_LIBASS
            if (pRTS->m_LibassContext.IsLibassActive()) {
                pRTS->m_LibassContext.SetFilterGraph(m_graph.m_pGB);
            }
#endif
            pSubStream = pRTS.Detach();
        }
    }

    if (pSubStream) {
        SubtitleInput subInput(pSubStream);
        m_ExternalSubstreams.push_back(pSubStream);
        m_pSubStreams.AddTail(subInput);

        if (!m_posFirstExtSub) {
            m_posFirstExtSub = m_pSubStreams.GetTailPosition();
        }
    }

    return !!pSubStream;
}

// Called from GraphThread
bool CSubtitleController::SetSubtitle(int i, bool bIsOffset /*= false*/, bool bDisplayMessage /*= false*/)
{
    if (!m_graph.m_pCAP) {
        return false;
    }
    if (m_frame.GetLoadState() == MLS::CLOSING) {
        return false;
    }

    CAppSettings& s = AfxGetAppSettings();

    SubtitleInput* pSubInput = nullptr;
    if (m_frame.m_iReloadSubIdx >= 0) {
        pSubInput = GetSubtitleInput(m_frame.m_iReloadSubIdx);
        if (pSubInput) {
            i = m_frame.m_iReloadSubIdx;
        }
        m_frame.m_iReloadSubIdx = -1;
    }

    if (!pSubInput) {
        pSubInput = GetSubtitleInput(i, bIsOffset);
    }

    bool success = false;

    if (pSubInput) {
        CComHeapPtr<WCHAR> pName;
        if (CComQIPtr<IAMStreamSelect> pSSF = pSubInput->pSourceFilter) {
            DWORD dwFlags;
            LCID lcid = 0;
            if (FAILED(pSSF->Info(i, nullptr, &dwFlags, &lcid, nullptr, &pName, nullptr, nullptr))) {
                dwFlags = 0;
            }
            if (lcid && s.fEnableSubtitles) {
                m_frame.currentSubLang = ISOLang::LCIDToISO6392(lcid);
            } else {
                m_frame.currentSubLang.Empty();
            }

            // Enable the track only if it isn't already the only selected track in the group
            if (!(dwFlags & AMSTREAMSELECTINFO_EXCLUSIVE)) {
                pSSF->Enable(i, AMSTREAMSELECTENABLE_ENABLE);
            }
            i = 0;
        }
        {
            // m_csSubLock shouldn't be locked when using IAMStreamSelect::Enable or SetSubtitle
            CAutoLock cAutoLock(&m_csSubLock);
            pSubInput->pSubStream->SetStream(i);
        }
        SetSubtitle(*pSubInput, true);

        if (!pName) {
            LCID lcid = 0;
            pSubInput->pSubStream->GetStreamInfo(0, &pName, &lcid);
            if (lcid && s.fEnableSubtitles) {
                m_frame.currentSubLang = ISOLang::LCIDToISO6392(lcid);
            } else {
                m_frame.currentSubLang.Empty();
            }
        }

        if (bDisplayMessage && pName) {
            m_frame.m_OSD.DisplayMessage(OSD_TOPLEFT, m_frame.GetStreamOSDString(CString(pName), LCID(-1), 2));
        }
        success = true;
    }

    if (success && s.fKeepHistory && s.bRememberTrackSelection) {
        s.MRU.UpdateCurrentSubtitleTrack(GetSelectedSubtitleTrackIndex());
    }
    return success;
}

void CSubtitleController::UpdateSubtitleColorInfo()
{
    if (!m_frame.IsStateLoaded() || !m_graph.m_pCAP
            || (!m_pCurrentSubInput.pSubStream && !m_pSecondarySubInput.pSubStream)) {
        return;
    }

    // store video mediatype, so colorspace information can be extracted when present
    IBaseFilter* pBF = FindFilter(GUID_LAVVideo, m_graph.m_pGB);
    if (pBF) {
        CComPtr<IPin> pPin = GetFirstPin(pBF, PINDIR_OUTPUT);
        if (pPin) {
            AM_MEDIA_TYPE mt;
            if (SUCCEEDED(pPin->ConnectionMediaType(&mt))) {
                m_graph.m_pCAP->SetVideoMediaType(CMediaType(mt));
            }
        }
    }

    CComQIPtr<ISubRenderOptions> pSRO = m_graph.m_pCAP;

    LPWSTR yuvMatrix = nullptr;
    int nLen;
    if (m_graph.m_pMVRI) {
        m_graph.m_pMVRI->GetString("yuvMatrix", &yuvMatrix, &nLen);
    } else if (pSRO) {
        pSRO->GetString("yuvMatrix", &yuvMatrix, &nLen);
    }

    int targetBlackLevel = 0, targetWhiteLevel = 255;
    if (m_graph.m_pMVRS) {
        m_graph.m_pMVRS->SettingsGetInteger(L"Black", &targetBlackLevel);
        m_graph.m_pMVRS->SettingsGetInteger(L"White", &targetWhiteLevel);
    } else if (pSRO) {
        int range = 0;
        pSRO->GetInt("supportedLevels", &range);
        if (range == 3) {
            targetBlackLevel = 16;
            targetWhiteLevel = 235;
        }
    }

    if (m_pCurrentSubInput.pSubStream) {
        m_pCurrentSubInput.pSubStream->SetSourceTargetInfo(yuvMatrix, targetBlackLevel, targetWhiteLevel);
    }
    if (m_pSecondarySubInput.pSubStream) {
        m_pSecondarySubInput.pSubStream->SetSourceTargetInfo(yuvMatrix, targetBlackLevel, targetWhiteLevel);
    }
    LocalFree(yuvMatrix);
}

void CSubtitleController::SetSubtitle(const SubtitleInput& subInput, bool skip_lcid /* = false */)
{
    TRACE(_T("CMainFrame::SetSubtitle\n"));

    CAppSettings& s = AfxGetAppSettings();
    ResetSubtitlePosAndSize(false);

    ResetAutoCopySubtitle();

    {
        CAutoLock cAutoLock(&m_csSubLock);

        bool firstuse = !m_pCurrentSubInput.pSubStream;

        if (subInput.pSubStream) {
            bool found = false;
            POSITION pos = m_pSubStreams.GetHeadPosition();
            while (pos) {
                if (subInput.pSubStream == m_pSubStreams.GetNext(pos).pSubStream) {
                    found = true;
                    break;
                }
            }
            // We are trying to set a subtitles stream that isn't in the list so we abort here.
            if (!found) {
                return;
            }
        }

        if (m_graph.m_pCAP && m_pCurrentSubInput.pSubStream && m_pCurrentSubInput.pSubStream != subInput.pSubStream) {
            m_graph.m_pCAP->SetSubPicProvider(nullptr);
        }

        m_pCurrentSubInput = subInput;

        if (m_pSecondarySubInput.pSubStream && m_pSecondarySubInput.pSubStream == subInput.pSubStream) {
            // the new primary is the current secondary; drop the secondary to avoid rendering the same track twice
            m_pSecondarySubInput = SubtitleInput(nullptr);
        }

        UpdateSubtitleColorInfo();
        UpdateSubtitleRenderingParameters();

        if (!skip_lcid) {
            LCID lcid = 0;
            if (m_pCurrentSubInput.pSubStream && s.fEnableSubtitles) {
                CComHeapPtr<WCHAR> pName;
                m_pCurrentSubInput.pSubStream->GetStreamInfo(0, &pName, &lcid);
            }
            if (lcid) {
                GetLocaleString(lcid, LOCALE_SISO639LANGNAME2, m_frame.currentSubLang);
            } else {
                m_frame.currentSubLang.Empty();
            }
        }

        if (m_graph.m_pCAP) {
            g_bExternalSubtitle = (std::find(m_ExternalSubstreams.cbegin(), m_ExternalSubstreams.cend(), subInput.pSubStream) != m_ExternalSubstreams.cend());
            bool use_subresync = false;
            if (auto pRTS = dynamic_cast<CRenderedTextSubtitle*>((ISubStream*)m_pCurrentSubInput.pSubStream)) {
#if USE_LIBASS
                if (!pRTS->m_LibassContext.IsLibassActive())
#endif
                    use_subresync = true;
            }
            if (use_subresync) {
                m_frame.m_wndSubresyncBar.SetSubtitle(subInput.pSubStream, m_graph.m_pCAP->GetFPS(), g_bExternalSubtitle);
            } else {
                m_frame.m_wndSubresyncBar.SetSubtitle(nullptr, m_graph.m_pCAP->GetFPS(), g_bExternalSubtitle);
            }
            // an index into the previous track's subresync list means nothing for the new one
            m_nCurSubtitle = -1;
            m_lSubtitleShift = 0;
        }
    }

    if (m_graph.m_pCAP && s.fEnableSubtitles) {
        m_graph.m_pCAP->SetSubPicProvider(GetSubtitleSubPicProvider());
    }

    if (s.fKeepHistory && s.bRememberTrackSelection) {
        s.MRU.UpdateCurrentSubtitleTrack(GetSelectedSubtitleTrackIndex());
    }
}

void CSubtitleController::SetSecondarySubtitle(const SubtitleInput& subInput)
{
    {
        CAutoLock cAutoLock(&m_csSubLock);

        // The menu already filters, but guard the sink as well.
        if (subInput.pSubStream && !IsEligibleSecondarySubtitle(subInput)) {
            return;
        }

        m_pSecondarySubInput = subInput;

        UpdateSubtitleColorInfo();
        UpdateSubtitleRenderingParameters();
    }

    if (m_graph.m_pCAP && AfxGetAppSettings().fEnableSubtitles) {
        m_graph.m_pCAP->SetSubPicProvider(GetSubtitleSubPicProvider());
    }
}

CComPtr<ISubPicProvider> CSubtitleController::GetSubtitleSubPicProvider()
{
    // The provider for the current subtitle selection: normally the primary
    // subtitle stream itself, but when a secondary subtitle track is active both
    // streams are wrapped in a merging provider that composites them into the
    // same subpics, so every renderer works unchanged.
    CComPtr<ISubPicProvider> pSubPicProvider(CComQIPtr<ISubPicProvider>(m_pCurrentSubInput.pSubStream));
    CComQIPtr<ISubPicProvider> pSecondarySubPicProvider(m_pSecondarySubInput.pSubStream);
    if (pSubPicProvider && pSecondarySubPicProvider) {
        pSubPicProvider = DEBUG_NEW CDualSubPicProvider(pSubPicProvider, pSecondarySubPicProvider);
    } else if (pSecondarySubPicProvider) {
        pSubPicProvider = pSecondarySubPicProvider;
    }
    return pSubPicProvider;
}

void CSubtitleController::ToggleSubtitleOnOff(bool bDisplayMessage /*= false*/)
{
    if (m_pDVS) {
        bool bHideSubtitles = false;
        m_pDVS->get_HideSubtitles(&bHideSubtitles);
        bHideSubtitles = !bHideSubtitles;
        m_pDVS->put_HideSubtitles(bHideSubtitles);
    }
    if (m_graph.m_pCAP && (!m_pDVS || !m_pSubStreams.IsEmpty())) {
        CAppSettings& s = AfxGetAppSettings();
        s.fEnableSubtitles = !s.fEnableSubtitles;

        if (s.fEnableSubtitles) {
            SetSubtitle(0, true, bDisplayMessage);
        } else {
            if (m_graph.m_pCAP) {
                m_graph.m_pCAP->SetSubPicProvider(nullptr);
            }
            m_frame.currentSubLang = ResStr(IDS_AG_DISABLED);

            if (bDisplayMessage) {
                m_frame.m_OSD.DisplayMessage(OSD_TOPLEFT, ResStr(IDS_SUBTITLE_STREAM_OFF));
            }
        }
    }
}

void CSubtitleController::ReplaceSubtitle(const ISubStream* pSubStreamOld, ISubStream* pSubStreamNew)
{
    POSITION pos = m_pSubStreams.GetHeadPosition();
    while (pos) {
        POSITION cur = pos;
        if (pSubStreamOld == m_pSubStreams.GetNext(pos).pSubStream) {
            m_pSubStreams.GetAt(cur).pSubStream = pSubStreamNew;
            if (m_pCurrentSubInput.pSubStream == pSubStreamOld) {
                SetSubtitle(m_pSubStreams.GetAt(cur), true);
            } else if (m_pSecondarySubInput.pSubStream == pSubStreamOld) {
                SetSecondarySubtitle(m_pSubStreams.GetAt(cur));
            }
            break;
        }
    }
}

void CSubtitleController::InvalidateSubtitle(DWORD_PTR nSubtitleId /*= DWORD_PTR_MAX*/, REFERENCE_TIME rtInvalidate /*= -1*/)
{
    if (m_graph.m_pCAP) {
        if (nSubtitleId == DWORD_PTR_MAX
                || nSubtitleId == (DWORD_PTR)(ISubStream*)m_pCurrentSubInput.pSubStream
                || (m_pSecondarySubInput.pSubStream && nSubtitleId == (DWORD_PTR)(ISubStream*)m_pSecondarySubInput.pSubStream)) {
            m_graph.m_pCAP->Invalidate(rtInvalidate);
        }
    }
}

void CSubtitleController::ReloadSubtitle()
{
    {
        CAutoLock cAutoLock(&m_csSubLock);
        POSITION pos = m_pSubStreams.GetHeadPosition();
        while (pos) {
            m_pSubStreams.GetNext(pos).pSubStream->Reload();
        }
    }

    ResetSubtitlePosAndSize(false);

    SetSubtitle(0, true);
    m_frame.m_wndSubresyncBar.ReloadSubtitle();
}

void CSubtitleController::SetSubtitleTrackIdx(int index)
{
    const CAppSettings& s = AfxGetAppSettings();

    if (m_frame.GetLoadState() == MLS::LOADED && m_graph.m_pCAP) {
        // Check if we want to change the enable/disable state
        if (s.fEnableSubtitles != (index >= 0)) {
            ToggleSubtitleOnOff();
        }
        // Set the new subtitles track if needed
        if (s.fEnableSubtitles) {
            SetSubtitle(index);
        }
    }
}

int CSubtitleController::GetCurrentSubtitleTrackIdx(CString *pstrName)
{
    if(pstrName)
        pstrName->Empty();

    if (m_frame.GetLoadState() != MLS::LOADED) {
        return -1;
    }

    if (m_graph.m_pCAP && !m_pSubStreams.IsEmpty()) {
        int idx = 0;
        POSITION pos = m_pSubStreams.GetHeadPosition();
        while (pos) {
            SubtitleInput& subInput = m_pSubStreams.GetNext(pos);
            if (CComQIPtr<IAMStreamSelect> pSSF = subInput.pSourceFilter) {
                DWORD cStreams;
                if (FAILED(pSSF->Count(&cStreams))) {
                    continue;
                }
                for (int j = 0, cnt = (int)cStreams; j < cnt; j++) {
                    DWORD dwFlags, dwGroup;
                    CComHeapPtr<WCHAR> pName;
                    if (FAILED(pSSF->Info(j, nullptr, &dwFlags, nullptr, &dwGroup, &pName, nullptr, nullptr))) {
                        continue;
                    }
                    if (dwGroup != 2) {
                        continue;
                    }

                    if (subInput.pSubStream == m_pCurrentSubInput.pSubStream) {
                        if (dwFlags & (AMSTREAMSELECTINFO_ENABLED | AMSTREAMSELECTINFO_EXCLUSIVE)) {
                            if (pstrName)
                                *pstrName = pName;
                            return idx;
                        }
                    }
                    idx++;
                }
            } else {
                CComPtr<ISubStream> pSubStream = subInput.pSubStream;
                if (!pSubStream) {
                    continue;
                }
                if (subInput.pSubStream == m_pCurrentSubInput.pSubStream) {
                    if (pstrName) {
                        CComHeapPtr<WCHAR> pName;
                        pSubStream->GetStreamInfo(pSubStream->GetStream(), &pName, nullptr);
                        *pstrName = pName;
                    }
                    return idx + pSubStream->GetStream();
                }
                idx += pSubStream->GetStreamCount();
            }
        }
    } else if (m_graph.m_pSplitterSS) {
        DWORD cStreams;
        if (SUCCEEDED(m_graph.m_pSplitterSS->Count(&cStreams))) {
            int idx = 0;
            for (int i = 0; i < (int)cStreams; i++) {
                DWORD dwFlags, dwGroup;
                CComHeapPtr<WCHAR> pszName;

                if (FAILED(m_graph.m_pSplitterSS->Info(i, nullptr, &dwFlags, nullptr, &dwGroup, &pszName, nullptr, nullptr)))
                    continue;

                if (dwGroup != 2)
                    continue;

                if (dwFlags & (AMSTREAMSELECTINFO_ENABLED | AMSTREAMSELECTINFO_EXCLUSIVE)) {
                    if (pstrName)
                        *pstrName = pszName;
                    return idx;
                }

                idx++;
            }
        }
    }

    return -1;
}

afx_msg void CSubtitleController::OnGotoSubtitle(UINT nID)
{
    if (!m_pSubStreams.IsEmpty() && !m_frame.IsPlaybackCaptureMode()) {
        m_rtCurSubPos = m_frame.m_wndSeekBar.GetPos();
        m_lSubtitleShift = 0;
        m_frame.m_wndSubresyncBar.RefreshEmbeddedTextSubtitleData();
        m_nCurSubtitle = m_frame.m_wndSubresyncBar.FindNearestSub(m_rtCurSubPos, (nID == ID_GOTO_NEXT_SUB));
        if (m_nCurSubtitle >= 0 && m_graph.m_pMS) {
            if (nID == ID_GOTO_PREV_SUB) {
                m_frame.OnPlayPause();
            }
            m_graph.m_pMS->SetPositions(&m_rtCurSubPos, AM_SEEKING_AbsolutePositioning, nullptr, AM_SEEKING_NoPositioning);
        }
    }
}

afx_msg void CSubtitleController::OnSubresyncShiftSub(UINT nID)
{
    if (m_nCurSubtitle >= 0) {
        long lShift = (nID == ID_SUBRESYNC_SHIFT_DOWN) ? -100 : 100;
        CString strSubShift;

        if (m_frame.m_wndSubresyncBar.ShiftSubtitle(m_nCurSubtitle, lShift, m_rtCurSubPos)) {
            m_lSubtitleShift += lShift;
            if (m_graph.m_pMS) {
                m_graph.m_pMS->SetPositions(&m_rtCurSubPos, AM_SEEKING_AbsolutePositioning, nullptr, AM_SEEKING_NoPositioning);
            }
        }

        strSubShift.Format(IDS_MAINFRM_138, m_lSubtitleShift);
        m_frame.m_OSD.DisplayMessage(OSD_TOPLEFT, strSubShift);
    }
}

afx_msg void CSubtitleController::OnSubtitleDelay(UINT nID)
{
    int nDelayStep = AfxGetAppSettings().nSubDelayStep;

    if (nID == ID_SUB_DELAY_DOWN) {
        nDelayStep = -nDelayStep;
    }

    SetSubtitleDelay(nDelayStep, /*relative=*/ true);
}

afx_msg void CSubtitleController::OnSubtitlePos(UINT nID)
{
    if (m_graph.m_pCAP) {
        CAppSettings& s = AfxGetAppSettings();
        switch (nID) {
        case ID_SUB_POS_DOWN:
            s.m_RenderersSettings.subPicVerticalShift += 2;
            break;
        case ID_SUB_POS_UP:
            s.m_RenderersSettings.subPicVerticalShift -= 2;
            break;
        }

        if (m_frame.GetMediaState() != State_Running) {
            m_graph.m_pCAP->Paint(false);
        }
    }
}

afx_msg void CSubtitleController::OnSubtitleFontSize(UINT nID)
{
    if (m_graph.m_pCAP && m_pCurrentSubInput.pSubStream) {
        CLSID clsid;
        m_pCurrentSubInput.pSubStream->GetClassID(&clsid);
        if (clsid == __uuidof(CRenderedTextSubtitle)) {
            CAppSettings& s = AfxGetAppSettings();
            switch (nID) {
            case ID_SUB_FONT_SIZE_DEC:
                s.m_RenderersSettings.fontScaleOverride -= 0.05;
                break;
            case ID_SUB_FONT_SIZE_INC:
                s.m_RenderersSettings.fontScaleOverride += 0.05;
                break;
            }

            CRenderedTextSubtitle* pRTS = (CRenderedTextSubtitle*)(ISubStream*)m_pCurrentSubInput.pSubStream;

            if (pRTS->m_LibassContext.IsLibassActive()) {
                // not supported by libass (yet)
                if (!m_frame.IsFullScreenMode()) {
                    AfxMessageBox(_T("Adjusting subtitle text size is not possible when using libass."), MB_ICONERROR, 0);
                }
            }

            {
                CAutoLock cAutoLock(&m_csSubLock);
                pRTS->Deinit();
            }
            InvalidateSubtitle();

            if (m_frame.GetMediaState() != State_Running) {
                m_graph.m_pCAP->Paint(false);
            }
        } else {
            if (!m_frame.IsFullScreenMode()) {
                AfxMessageBox(_T("Adjusting subtitle text size is not possible for image based subtitle formats."), MB_ICONERROR, 0);
            }
        }
    }
}

void CSubtitleController::ResetSubtitlePosAndSize(bool repaint /* = false*/)
{
    CAppSettings& s = AfxGetAppSettings();
    bool changed = (s.m_RenderersSettings.fontScaleOverride != 1.0) || (s.m_RenderersSettings.subPicVerticalShift != 0);

    s.m_RenderersSettings.fontScaleOverride = 1.0;
    s.m_RenderersSettings.subPicVerticalShift = 0;

    if (changed && repaint && m_graph.m_pCAP && m_pCurrentSubInput.pSubStream) {
        CLSID clsid;
        m_pCurrentSubInput.pSubStream->GetClassID(&clsid);
        if (clsid == __uuidof(CRenderedTextSubtitle)) {
            CRenderedTextSubtitle* pRTS = (CRenderedTextSubtitle*)(ISubStream*)m_pCurrentSubInput.pSubStream;
            {
                CAutoLock cAutoLock(&m_csSubLock);
                pRTS->Deinit();
            }
            InvalidateSubtitle();

            if (m_frame.GetMediaState() != State_Running) {
                m_graph.m_pCAP->Paint(false);
            }
        }
    }
}

void CSubtitleController::SendSubtitleTracksToApi()
{
    CStringW strSubs;
    if (m_frame.GetLoadState() == MLS::LOADED) {
        if (m_frame.GetPlaybackMode() == PM_DVD) {
            ULONG ulStreamsAvailable, ulCurrentStream;
            BOOL bIsDisabled;
            if (m_graph.m_pDVDI && SUCCEEDED(m_graph.m_pDVDI->GetCurrentSubpicture(&ulStreamsAvailable, &ulCurrentStream, &bIsDisabled))
                && ulStreamsAvailable > 0) {
                LCID DefLanguage;
                int iSelected = -1;

                DVD_SUBPICTURE_LANG_EXT ext;
                if (FAILED(m_graph.m_pDVDI->GetDefaultSubpictureLanguage(&DefLanguage, &ext))) {
                    return;
                }

                for (ULONG i = 0; i < ulStreamsAvailable; i++) {
                    LCID Language;
                    if (FAILED(m_graph.m_pDVDI->GetSubpictureLanguage(i, &Language))) {
                        continue;
                    }

                    if (i == ulCurrentStream) {
                        iSelected = (int)i;
                    }

                    CString str;
                    if (Language) {
                        GetLocaleString(Language, LOCALE_SENGLANGUAGE, str);
                    } else {
                        str.Format(IDS_AG_UNKNOWN, int(i + 1));
                    }

                    DVD_SubpictureAttributes ATR;
                    if (SUCCEEDED(m_graph.m_pDVDI->GetSubpictureAttributes(i, &ATR))) {
                        switch (ATR.LanguageExtension) {
                        case DVD_SP_EXT_NotSpecified:
                        default:
                            break;
                        case DVD_SP_EXT_Caption_Normal:
                            str += _T("");
                            break;
                        case DVD_SP_EXT_Caption_Big:
                            str += _T(" (Big)");
                            break;
                        case DVD_SP_EXT_Caption_Children:
                            str += _T(" (Children)");
                            break;
                        case DVD_SP_EXT_CC_Normal:
                            str += _T(" (CC)");
                            break;
                        case DVD_SP_EXT_CC_Big:
                            str += _T(" (CC Big)");
                            break;
                        case DVD_SP_EXT_CC_Children:
                            str += _T(" (CC Children)");
                            break;
                        case DVD_SP_EXT_Forced:
                            str += _T(" (Forced)");
                            break;
                        case DVD_SP_EXT_DirectorComments_Normal:
                            str += _T(" (Director Comments)");
                            break;
                        case DVD_SP_EXT_DirectorComments_Big:
                            str += _T(" (Director Comments, Big)");
                            break;
                        case DVD_SP_EXT_DirectorComments_Children:
                            str += _T(" (Director Comments, Children)");
                            break;
                        }
                    }
                    if (!strSubs.IsEmpty()) {
                        strSubs.Append(L"|");
                    }
                    str.Replace(L"|", L"\\|");
                    strSubs.Append(str);
                }
                if (AfxGetAppSettings().fEnableSubtitles) {
                    strSubs.AppendFormat(L"|%d", iSelected);
                } else {
                    strSubs.Append(L"|-1");
                }
            }
        } else {

            POSITION pos = m_pSubStreams.GetHeadPosition();
            int i = 0, iSelected = -1;
            if (pos) {
                while (pos) {
                    SubtitleInput& subInput = m_pSubStreams.GetNext(pos);

                    if (CComQIPtr<IAMStreamSelect> pSSF = subInput.pSourceFilter) {
                        DWORD cStreams;
                        if (FAILED(pSSF->Count(&cStreams))) {
                            continue;
                        }

                        for (int j = 0, cnt = (int)cStreams; j < cnt; j++) {
                            DWORD dwFlags, dwGroup;
                            WCHAR* pszName = nullptr;

                            if (FAILED(pSSF->Info(j, nullptr, &dwFlags, nullptr, &dwGroup, &pszName, nullptr, nullptr))
                                || !pszName) {
                                continue;
                            }

                            CString name(pszName);
                            CoTaskMemFree(pszName);

                            if (dwGroup != 2) {
                                continue;
                            }

                            if (subInput.pSubStream == m_pCurrentSubInput.pSubStream
                                && dwFlags & (AMSTREAMSELECTINFO_ENABLED | AMSTREAMSELECTINFO_EXCLUSIVE)) {
                                iSelected = i;
                            }

                            if (!strSubs.IsEmpty()) {
                                strSubs.Append(L"|");
                            }
                            name.Replace(L"|", L"\\|");
                            strSubs.Append(name);

                            i++;
                        }
                    } else {
                        CComPtr<ISubStream> pSubStream = subInput.pSubStream;
                        if (!pSubStream) {
                            continue;
                        }

                        if (subInput.pSubStream == m_pCurrentSubInput.pSubStream) {
                            iSelected = i + pSubStream->GetStream();
                        }

                        for (int j = 0, cnt = pSubStream->GetStreamCount(); j < cnt; j++) {
                            WCHAR* pName = nullptr;
                            if (SUCCEEDED(pSubStream->GetStreamInfo(j, &pName, nullptr))) {
                                CString name(pName);
                                CoTaskMemFree(pName);

                                if (!strSubs.IsEmpty()) {
                                    strSubs.Append(L"|");
                                }
                                name.Replace(L"|", L"\\|");
                                strSubs.Append(name);
                            }
                            i++;
                        }
                    }

                }
                if (AfxGetAppSettings().fEnableSubtitles) {
                    strSubs.AppendFormat(L"|%d", iSelected);
                } else {
                    strSubs.Append(L"|-1");
                }
            } else {
                strSubs.Append(L"-1");
            }
        }
    } else {
        strSubs.Append(L"-2");
    }
    m_frame.m_api.SendAPICommand(CMD_LISTSUBTITLETRACKS, L"%s", static_cast<LPCWSTR>(strSubs));
}

// Returns the the corresponding subInput or nullptr in case of error.
// i is modified to reflect the locale index of track
SubtitleInput* CSubtitleController::GetSubtitleInput(int& i, bool bIsOffset /*= false*/)
{
    // Only 1, 0 and -1 are supported offsets
    if ((bIsOffset && (i < -1 || i > 1)) || (!bIsOffset && i < 0)) {
        return nullptr;
    }

    POSITION pos = m_pSubStreams.GetHeadPosition();
    SubtitleInput* pSubInput = nullptr, *pSubInputPrec = nullptr;
    int iLocalIdx = -1, iLocalIdxPrec = -1;
    bool bNextTrack = false;

    while (pos && !pSubInput) {
        SubtitleInput& subInput = m_pSubStreams.GetNext(pos);

        if (CComQIPtr<IAMStreamSelect> pSSF = subInput.pSourceFilter) {
            DWORD cStreams;
            if (FAILED(pSSF->Count(&cStreams))) {
                continue;
            }

            for (int j = 0, cnt = (int)cStreams; j < cnt; j++) {
                DWORD dwFlags, dwGroup;

                if (FAILED(pSSF->Info(j, nullptr, &dwFlags, nullptr, &dwGroup, nullptr, nullptr, nullptr))) {
                    continue;
                }

                if (dwGroup != 2) {
                    continue;
                }

                if (bIsOffset) {
                    if (bNextTrack) { // We detected previously that the next subtitles track is the one we want to select
                        pSubInput = &subInput;
                        iLocalIdx = j;
                        break;
                    } else if (subInput.pSubStream == m_pCurrentSubInput.pSubStream
                               && dwFlags & (AMSTREAMSELECTINFO_ENABLED | AMSTREAMSELECTINFO_EXCLUSIVE)) {
                        if (i == 0) {
                            pSubInput = &subInput;
                            iLocalIdx = j;
                            break;
                        } else if (i > 0) {
                            bNextTrack = true; // We want to the select the next subtitles track
                        } else {
                            // We want the previous subtitles track and we know which one it is
                            if (pSubInputPrec) {
                                pSubInput = pSubInputPrec;
                                iLocalIdx = iLocalIdxPrec;
                                break;
                            }
                        }
                    }

                    pSubInputPrec = &subInput;
                    iLocalIdxPrec = j;
                } else {
                    if (i == 0) {
                        pSubInput = &subInput;
                        iLocalIdx = j;
                        break;
                    }

                    i--;
                }
            }
        } else {
            if (bIsOffset) {
                if (bNextTrack) { // We detected previously that the next subtitles track is the one we want to select
                    pSubInput = &subInput;
                    iLocalIdx = 0;
                    break;
                } else if (subInput.pSubStream == m_pCurrentSubInput.pSubStream) {
                    iLocalIdx = subInput.pSubStream->GetStream() + i;
                    if (iLocalIdx >= 0 && iLocalIdx < subInput.pSubStream->GetStreamCount()) {
                        // The subtitles track we want to select is part of this substream
                        pSubInput = &subInput;
                    } else if (i > 0) { // We want to the select the next subtitles track
                        bNextTrack = true;
                    } else {
                        // We want the previous subtitles track and we know which one it is
                        if (pSubInputPrec) {
                            pSubInput = pSubInputPrec;
                            iLocalIdx = iLocalIdxPrec;
                        }
                    }
                } else {
                    pSubInputPrec = &subInput;
                    iLocalIdxPrec = subInput.pSubStream->GetStreamCount() - 1;
                }
            } else {
                if (i < subInput.pSubStream->GetStreamCount()) {
                    pSubInput = &subInput;
                    iLocalIdx = i;
                } else {
                    i -= subInput.pSubStream->GetStreamCount();
                }
            }
        }

        // Handle special cases
        if (!pos && !pSubInput && bIsOffset) {
            if (bNextTrack) { // The last subtitles track was selected and we want the next one
                // Let's restart the loop to select the first subtitles track
                pos = m_pSubStreams.GetHeadPosition();
            } else if (i < 0) { // The first subtitles track was selected and we want the previous one
                pSubInput = pSubInputPrec; // We select the last track
                iLocalIdx = iLocalIdxPrec;
            }
        }
    }

    i = iLocalIdx;

    return pSubInput;
}

bool CSubtitleController::ApplySubtitleRenderingParameters(ISubStream* pSubStream, bool bSecondary)
{
    const CAppSettings& s = AfxGetAppSettings();
    if (auto pRTS = dynamic_cast<CRenderedTextSubtitle*>(pSubStream)) {
        bool bChangeStorageRes = false;
        bool bChangePARComp = false;
        double dPARCompensation = 1.0;
        bool bKeepAspectRatio = s.fKeepAspectRatio;

        CSize szAspectRatio = m_graph.m_pCAP->GetVideoSize(true);
        CSize szVideoFrame;
        if (m_graph.m_pMVRI) {
            // Use IMadVRInfo to get size. See http://bugs.madshi.net/view.php?id=180
            m_graph.m_pMVRI->GetSize("originalVideoSize", &szVideoFrame);
            bKeepAspectRatio = true;
        } else {
            szVideoFrame = m_graph.m_pCAP->GetVideoSize(false);
        }

        if (s.bSubtitleARCompensation && szAspectRatio.cx && szAspectRatio.cy && szVideoFrame.cx && szVideoFrame.cy && bKeepAspectRatio) {
            if (pRTS->m_layoutRes.cx > 0) {
                dPARCompensation = (double)szAspectRatio.cx * pRTS->m_layoutRes.cy / (szAspectRatio.cy * pRTS->m_layoutRes.cx);
            } else {
                dPARCompensation = (double)szAspectRatio.cx * szVideoFrame.cy / (szAspectRatio.cy * szVideoFrame.cx);
            }
        }
        if (pRTS->m_dPARCompensation != dPARCompensation) {
            bChangePARComp = true;
        }

        if (pRTS->m_subtitleType == Subtitle::ASS || pRTS->m_subtitleType == Subtitle::SSA) {
            if (szVideoFrame.cx > 0) {
                if (pRTS->m_layoutRes.cx == 0 || pRTS->m_layoutRes.cy == 0) {
                    bChangeStorageRes = (pRTS->m_storageRes != szVideoFrame);
                } else {
                    bChangeStorageRes = (pRTS->m_storageRes != pRTS->m_layoutRes);
                }
            }
        }

        {
            CAutoLock cAutoLock(&m_csSubLock);
            if (bChangeStorageRes) {
                if (pRTS->m_layoutRes.cx == 0 || pRTS->m_layoutRes.cy == 0) {
                    pRTS->m_storageRes = szVideoFrame;
                } else {
                    pRTS->m_storageRes = pRTS->m_layoutRes;
                }
            }
            if (bChangePARComp) {
                pRTS->m_ePARCompensationType = CSimpleTextSubtitle::EPARCompensationType::EPCTAccurateSize_ISR;
                pRTS->m_dPARCompensation = dPARCompensation;
            }

            STSStyle style = s.subtitlesDefStyle;
            if (pRTS->m_bUsingPlayerDefaultStyle) {
                pRTS->SetDefaultStyle(style);
            } else if (pRTS->GetDefaultStyle(style) && style.relativeTo == STSStyle::AUTO && s.subtitlesDefStyle.relativeTo != STSStyle::AUTO) {
                style.relativeTo = s.subtitlesDefStyle.relativeTo;
                pRTS->SetDefaultStyle(style);
            }
            pRTS->SetOverride(s.bSubtitleOverrideDefaultStyle, s.bSubtitleOverrideAllStyles, s.subtitlesDefStyle);
            if (bSecondary) {
                // the secondary subtitle track is always anchored to the top of the frame
                pRTS->SetAlignment(true, s.nHorPos, s.nSecondarySubVerPos, true);
            } else {
                pRTS->SetAlignment(s.fOverridePlacement, s.nHorPos, s.nVerPos);
            }
            pRTS->SetUseFreeType(s.bUseFreeType);
            pRTS->SetRoundedOpaqueBox(s.bSubtitleRoundedOpaqueBox);
            pRTS->SetOpenTypeLangHint(s.strOpenTypeLangHint);
            pRTS->Deinit();
        }
        return true;
    } else if (auto pVSS = dynamic_cast<CVobSubSettings*>(pSubStream)) {
        {
            CAutoLock cAutoLock(&m_csSubLock);
            pVSS->SetAlignment(s.fOverridePlacement, s.nHorPos, s.nVerPos);
        }
        return true;
    }
    return false;
}

void CSubtitleController::UpdateSubtitleRenderingParameters()
{
    if (!m_graph.m_pCAP) {
        return;
    }

    bool bInvalidate = ApplySubtitleRenderingParameters((ISubStream*)m_pCurrentSubInput.pSubStream, false);
    if (m_pSecondarySubInput.pSubStream) {
        bInvalidate |= ApplySubtitleRenderingParameters((ISubStream*)m_pSecondarySubInput.pSubStream, true);
    }
    if (bInvalidate) {
        m_graph.m_pCAP->Invalidate();
    }
}

typedef struct {
    SubtitlesInfo* pSubtitlesInfo;
    BOOL bActivate;
    std::string fileName;
    std::string fileContents;
} SubtitlesData;

LRESULT CSubtitleController::OnLoadSubtitles(WPARAM wParam, LPARAM lParam)
{
    SubtitlesData& data = *(SubtitlesData*)lParam;

    CAutoPtr<CRenderedTextSubtitle> pRTS(DEBUG_NEW CRenderedTextSubtitle(&m_csSubLock));
    if (pRTS) {
        if (pRTS->Open(CString(data.pSubtitlesInfo->Provider()->DisplayName().c_str()),
            (BYTE*)(LPCSTR)data.fileContents.c_str(), (int)data.fileContents.length(), DEFAULT_CHARSET,
            UTF8To16(data.fileName.c_str()), Subtitle::HearingImpairedType(data.pSubtitlesInfo->hearingImpaired),
            ISOLang::ISO6391ToLcid(data.pSubtitlesInfo->languageCode.c_str())) && pRTS->GetStreamCount() > 0) {
            m_wndSubtitlesDownloadDialog.DoDownloaded(*data.pSubtitlesInfo);
#if USE_LIBASS
            if (pRTS->m_LibassContext.IsLibassActive()) {
                pRTS->m_LibassContext.SetFilterGraph(m_graph.m_pGB);
            }
#endif
            SubtitleInput subElement = pRTS.Detach();
            m_pSubStreams.AddTail(subElement);
            if (data.bActivate) {
                m_ExternalSubstreams.push_back(subElement.pSubStream);
                SetSubtitle(subElement.pSubStream);

                auto& s = AfxGetAppSettings();
                if (s.fKeepHistory && s.bRememberTrackSelection && s.bAutoSaveDownloadedSubtitles) {
                    s.MRU.UpdateCurrentSubtitleTrack(GetSelectedSubtitleTrackIndex());
                }
            }
            return TRUE;
        }
    }

    return FALSE;
}

LRESULT CSubtitleController::OnGetSubtitles(WPARAM, LPARAM lParam)
{
    CheckPointer(lParam, FALSE);

    int n = 0;
    SubtitleInput* pSubInput = GetSubtitleInput(n, true);
    CheckPointer(pSubInput, FALSE);

    CLSID clsid;
    if (FAILED(pSubInput->pSubStream->GetClassID(&clsid)) || clsid != __uuidof(CRenderedTextSubtitle)) {
        return FALSE;
    }

    CRenderedTextSubtitle* pRTS = (CRenderedTextSubtitle*)(ISubStream*)pSubInput->pSubStream;
    // Only for external text subtitles
    if (pRTS->m_path.IsEmpty()) {
        return FALSE;
    }

    SubtitlesInfo* pSubtitlesInfo = reinterpret_cast<SubtitlesInfo*>(lParam);

    pSubtitlesInfo->GetFileInfo();
    pSubtitlesInfo->releaseNames.emplace_back(UTF16To8(pRTS->m_name));
    if (pSubtitlesInfo->hearingImpaired == Subtitle::HI_UNKNOWN) {
        pSubtitlesInfo->hearingImpaired = pRTS->m_eHearingImpaired;
    }

    if (!pSubtitlesInfo->languageCode.length() && pRTS->m_lcid && pRTS->m_lcid != LCID(-1)) {
        CString str;
        GetLocaleString(pRTS->m_lcid, LOCALE_SISO639LANGNAME, str);
        pSubtitlesInfo->languageCode = UTF16To8(str);
    }

    pSubtitlesInfo->frameRate = m_graph.m_pCAP->GetFPS();
    int delay = m_graph.m_pCAP->GetSubtitleDelay();
    if (pRTS->m_mode == FRAME) {
        delay = std::lround(delay * pSubtitlesInfo->frameRate / 1000.0);
    }

    const CStringW fmt(L"%d\n%02d:%02d:%02d,%03d --> %02d:%02d:%02d,%03d\n%s\n\n");
    CStringW content;
    CAutoLock cAutoLock(&m_csSubLock);
    for (int i = 0, j = int(pRTS->GetCount()), k = 0; i < j; i++) {
        int t1 = (int)(RT2MS(pRTS->TranslateStart(i, pSubtitlesInfo->frameRate)) + delay);
        if (t1 < 0) {
            k++;
            continue;
        }

        int t2 = (int)(RT2MS(pRTS->TranslateEnd(i, pSubtitlesInfo->frameRate)) + delay);

        int hh1 = (t1 / 60 / 60 / 1000);
        int mm1 = (t1 / 60 / 1000) % 60;
        int ss1 = (t1 / 1000) % 60;
        int ms1 = (t1) % 1000;
        int hh2 = (t2 / 60 / 60 / 1000);
        int mm2 = (t2 / 60 / 1000) % 60;
        int ss2 = (t2 / 1000) % 60;
        int ms2 = (t2) % 1000;

        content.AppendFormat(fmt, i - k + 1, hh1, mm1, ss1, ms1, hh2, mm2, ss2, ms2, pRTS->GetStrW(i, false).GetString());
    }

    pSubtitlesInfo->fileContents = UTF16To8(content);
    return TRUE;
}
