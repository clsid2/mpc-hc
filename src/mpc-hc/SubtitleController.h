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

#pragma once

#include "SubtitleDlDlg.h"
#include "YoutubeDL.h"
#include "../SubPic/ISubPic.h"
#include "../filters/transform/VSFilter/IDirectVobSub.h"
#include <list>
#include <memory>

class CMainFrame;
struct CPlayerGraph;
class SubtitlesProviders;
interface ISubClock;

struct SubtitleInput {
    CComQIPtr<ISubStream> pSubStream;
    CComPtr<IBaseFilter> pSourceFilter;

    SubtitleInput() {};
    SubtitleInput(CComQIPtr<ISubStream> pSubStream) : pSubStream(pSubStream) {};
    SubtitleInput(CComQIPtr<ISubStream> pSubStream, CComPtr<IBaseFilter> pSourceFilter)
        : pSubStream(pSubStream), pSourceFilter(pSourceFilter) {};
};

// The subtitle streams of the open file and everything done with them:
// loading and saving, track selection, the secondary track, delay, position
// and size, the resync commands, the download dialog and its providers, and
// the two subtitle submenus. Composed into CMainFrame, which routes its
// commands here from OnCmdMsg and forwards WM_LOADSUBTITLES and
// WM_GETSUBTITLES. The menu objects stay in CMainFrame with the other dynamic
// menus, and the resync bar stays with the other docked bars.
//
// The state is public because the subtitle renderer side reaches it from
// outside: the text pass-through filter, the download dialog, the subtitle
// providers and their threads, the web server and the open path.
class CSubtitleController : public CCmdTarget
{
    CMainFrame& m_frame;
    CPlayerGraph& m_graph;

public:
    explicit CSubtitleController(CMainFrame& frame);
    ~CSubtitleController();

    CComQIPtr<IDirectVobSub> m_pDVS;
    CComQIPtr<IDirectVobSub2> m_pDVS2;

    CCritSec m_csSubLock;
    CCritSec m_csSubtitleManagementLock;

    CList<SubtitleInput> m_pSubStreams;
    std::list<ISubStream*> m_ExternalSubstreams;
    POSITION m_posFirstExtSub;
    SubtitleInput m_pCurrentSubInput;
    SubtitleInput m_pSecondarySubInput; // secondary subtitle track, limited to externally loaded text subtitles

    // last subtitle segment copied to the clipboard by auto-copy, and the time
    // before which there is nothing new to copy
    int m_nLastCopiedSubSegment;
    REFERENCE_TIME m_rtNextAutoCopySubtitle;
    void ResetAutoCopySubtitle() {
        m_nLastCopiedSubSegment = -1;
        m_rtNextAutoCopySubtitle = 0;
    }

    CComPtr<ISubClock> m_pSubClock;

    int         m_loadedSubtitleTrackIndex = -1;

    int         m_nCurSubtitle;
    long        m_lSubtitleShift;
    REFERENCE_TIME m_rtCurSubPos;

    CSubtitleDlDlg m_wndSubtitlesDownloadDialog;
    std::unique_ptr<SubtitlesProviders> m_pSubtitlesProviders;

    SubtitleInput* GetSubtitleInput(int& i, bool bIsOffset = false);
    // Secondary subtitle track helpers. The same eligibility predicate drives the
    // menu builder and the command handler, so the handler can never select an
    // entry the menu did not offer.
    bool IsEligibleSecondarySubtitle(const SubtitleInput& subInput) const;
    SubtitleInput* GetSecondarySubtitleInput(int idx);
    bool IsValidSubtitleStream(int i);
    int GetSelectedSubtitleTrackIndex();

    void SetupSubtitlesSubMenu();
    void SetupSecondarySubtitleSubMenu();

    void AddTextPassThruFilter();

    int SetupSubtitleStreams();

    bool LoadSubtitle(CString fn, SubtitleInput* pSubInput = nullptr, bool bAutoLoad = false);
    bool LoadSubtitle(CYoutubeDLInstance::YDLSubInfo& sub);
    bool SetSubtitle(int i, bool bIsOffset = false, bool bDisplayMessage = false);
    void SetSubtitle(const SubtitleInput& subInput, bool skip_lcid = false);
    void SetSecondarySubtitle(const SubtitleInput& subInput);
    CComPtr<ISubPicProvider> GetSubtitleSubPicProvider();
    void UpdateSubtitleColorInfo();
    void ToggleSubtitleOnOff(bool bDisplayMessage = false);
    void ReplaceSubtitle(const ISubStream* pSubStreamOld, ISubStream* pSubStreamNew);
    void InvalidateSubtitle(DWORD_PTR nSubtitleId = DWORD_PTR_MAX, REFERENCE_TIME rtInvalidate = -1);
    void ReloadSubtitle();
    bool ApplySubtitleRenderingParameters(ISubStream* pSubStream, bool bSecondary);
    void UpdateSubtitleRenderingParameters();
    HRESULT InsertTextPassThruFilter(IBaseFilter* pBF, IPin* pPin, IPin* pPinto);

    void SetSubtitleTrackIdx(int index);
    int GetCurrentSubtitleTrackIdx(CString *pstrName = nullptr);

    void        SetSubtitleDelay(int delay_ms, bool relative = false);
    void        ResetSubtitlePosAndSize(bool repaint = false);

    void        SendSubtitleTracksToApi();

    void SubtitlesSave(const TCHAR* directory = nullptr, bool silent = false);
    REFERENCE_TIME CopyCurrentSubtitleToClipboard(REFERENCE_TIME rtNow);

    LRESULT OnLoadSubtitles(WPARAM wParam, LPARAM lParam);
    LRESULT OnGetSubtitles(WPARAM, LPARAM lParam);

    afx_msg void OnUpdateSubtitlesButton(CCmdUI* pCmdUI);

    afx_msg void OnStreamSub(UINT nID);
    afx_msg void OnStreamSubOnOff();
    afx_msg void OnSubtitlesAutoCopy();

    afx_msg void OnFileSubtitlesLoad();
    afx_msg void OnUpdateFileSubtitlesLoad(CCmdUI* pCmdUI);
    afx_msg void OnFileSubtitlesSave() { SubtitlesSave(); }
    afx_msg void OnUpdateFileSubtitlesSave(CCmdUI* pCmdUI);
    //afx_msg void OnFileSubtitlesUpload();
    //afx_msg void OnUpdateFileSubtitlesUpload(CCmdUI* pCmdUI);
    afx_msg void OnFileSubtitlesDownload();
    afx_msg void OnUpdateFileSubtitlesDownload(CCmdUI* pCmdUI);

    afx_msg void OnViewSubresync();
    afx_msg void OnUpdateViewSubresync(CCmdUI* pCmdUI);

    afx_msg void OnGotoSubtitle(UINT nID);
    afx_msg void OnSubresyncShiftSub(UINT nID);
    afx_msg void OnSubtitleDelay(UINT nID);
    afx_msg void OnSubtitlePos(UINT nID);
    afx_msg void OnSubtitleFontSize(UINT nID);

    afx_msg void OnSubtitlesDefaultStyle();
    afx_msg void OnSubtitlesOverrideStyles();
    afx_msg void OnPlaySubtitles(UINT nID);
    afx_msg void OnPlaySecondarySubtitle(UINT nID);
    afx_msg void OnSecondarySubtitleLoad();

    DECLARE_MESSAGE_MAP()
};
