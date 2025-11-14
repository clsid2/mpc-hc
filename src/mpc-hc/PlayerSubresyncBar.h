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

#pragma once

#include "CMPCThemePlayerBar.h"
#include "PlayerListCtrl.h"
#include "../Subtitles/STS.h"
#include "../Subtitles/VobSubFile.h"
#include <map>
#include <vector>


class CMainFrame;
interface ISubStream;

// CPlayerSubresyncBar


class CPlayerSubresyncBar : public CMPCThemePlayerBar
    , public CMPCThemeListCtrlCustomInterface
{
    DECLARE_DYNAMIC(CPlayerSubresyncBar)

private:
    CString m_strYes, m_strNo;
    CString m_strYesMenu, m_strNoMenu;
    bool bHadFocusBeforeClick = false;

    CPlayerListCtrl m_list;

    CMainFrame* m_pMainFrame;

    CFont m_font;
    void ScaleFont();

	int m_itemHeight = 0;
	bool createdWindow;

    EventClient m_eventc;
    void EventCallback(MpcEvent ev);

    CCritSec* m_pSubLock;
    CComPtr<ISubStream> m_pSubStream;
    double m_fps;
    bool m_external;

    int m_lastSegment;
    REFERENCE_TIME m_rt;

    enum {
        // TEXTSUB
        COL_START = 0,
        COL_END,
        COL_PREVSTART,
        COL_PREVEND,
        COL_TEXT,
        COL_STYLE,
        COL_FONT,
        COL_CHARSET,
        COL_UNICODE,
        COL_LAYER,
        COL_ACTOR,
        COL_EFFECT,
        COL_COUNT_TEXTSUB,
        // VOBSUB same as TEXTSUB
        COL_VOBID = COL_TEXT,
        COL_CELLID,
        COL_FORCED,
        COL_COUNT_VOBSUB
    };

    enum MODE {
        NONE = 0,
        VOBSUB,
        TEXTSUB
    };
    MODE m_mode;

    std::multimap<REFERENCE_TIME, size_t> m_newStartsIndex;
    struct SubTime {
        REFERENCE_TIME orgStart, newStart, orgEnd, newEnd;
        std::multimap<REFERENCE_TIME, size_t>::iterator itIndex;
    };
    std::vector<SubTime> m_subtimes;

    CSimpleTextSubtitle m_sts;
    CAtlArray<CVobSubFile::SubPos> m_vobSub;

    struct DisplayData {
        REFERENCE_TIME tStart, tPrevStart, tEnd, tPrevEnd;
        int flags;
    };
    std::vector<DisplayData> m_displayData;
    CString m_displayBuffer;

    void UpdatePreview();

    enum {
        TSMOD = 1,
        TEMOD = 2,
        TSADJ = 4,
        TEADJ = 8,
        TSEP  = 0x80000000
    };

    void SetSTS0(int& start, int end, REFERENCE_TIME ti0);
    void SetSTS1(int& start, int end, REFERENCE_TIME ti0, double m, int i0);

    void GetCheck(int iItem, bool& fStartMod, bool& fEndMod, bool& fStartAdj, bool& fEndAdj) const;
    void SetCheck(int iItem, bool fStart, bool fEnd);

    bool ModStart(int iItem, REFERENCE_TIME t, bool fReset = false);
    bool ModEnd(int iItem, REFERENCE_TIME t, bool fReset = false);

    void OnGetDisplayInfoTextSub(LV_ITEM* pItem);
    void OnGetDisplayInfoVobSub(LV_ITEM* pItem);

public:
    CPlayerSubresyncBar(CMainFrame* pMainFrame);
    virtual ~CPlayerSubresyncBar();

    BOOL Create(CWnd* pParentWnd, UINT defDockBarID, CCritSec* pSubLock);

    virtual void ReloadTranslatableResources();

    void SetTime(REFERENCE_TIME rt);
    void SetFPS(double fps);

    void SetSubtitle(ISubStream* pSubStream, double fps, bool external);
    bool RefreshEmbeddedTextSubtitleData();
    void ReloadSubtitle();
    void ResetSubtitle();
    void SaveSubtitle();

    int FindNearestSub(REFERENCE_TIME& rtPos, bool bForward);
    bool ShiftSubtitle(int nItem, long lValue, REFERENCE_TIME& rtPos);
    bool SaveToDisk();

protected:
    virtual BOOL PreCreateWindow(CREATESTRUCT& cs);
    virtual BOOL PreTranslateMessage(MSG* pMsg);
    virtual void GetCustomTextColors(INT_PTR nItem, int iSubItem, COLORREF& clrText, COLORREF& clrTextBk, bool& overrideSelectedBG);
    virtual void DoCustomPrePaint();
    virtual void GetCustomGridColors(int nItem, COLORREF& horzGridColor, COLORREF& vertGridColor);

    bool HandleShortCuts(const MSG* pMsg);

    DECLARE_MESSAGE_MAP()

    void OnMeasureItem(int nIDCtl, LPMEASUREITEMSTRUCT lpMeasureItemStruct);
    afx_msg void OnSize(UINT nType, int cx, int cy);
    afx_msg void OnGetDisplayInfo(NMHDR* pNMHDR, LRESULT* pResult);
    afx_msg void OnBeginlabeleditList(NMHDR* pNMHDR, LRESULT* pResult);
    afx_msg void OnDolabeleditList(NMHDR* pNMHDR, LRESULT* pResult);
    afx_msg void OnEndlabeleditList(NMHDR* pNMHDR, LRESULT* pResult);
    afx_msg void OnRclickList(NMHDR* pNMHDR, LRESULT* pResult);
    afx_msg void OnNMDblclkList(NMHDR* pNMHDR, LRESULT* pResult);
    afx_msg void OnLvnKeydownList(NMHDR* pNMHDR, LRESULT* pResult);
    void OnCustomdrawList(NMHDR* pNMHDR, LRESULT* pResult);
};
