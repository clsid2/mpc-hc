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
#include "PlayerSubresyncBar.h"
#include "mplayerc.h"
#include "MainFrm.h"
#include "WinAPIUtils.h"
#include "PPageSubStyle.h"
#include "../Subtitles/RTS.h"
#include "CMPCTheme.h"
#include "CMPCThemePropertySheet.h"

// CPlayerSubresyncBar

IMPLEMENT_DYNAMIC(CPlayerSubresyncBar, CMPCThemePlayerBar)
CPlayerSubresyncBar::CPlayerSubresyncBar(CMainFrame* pMainFrame)
    : CMPCThemePlayerBar(pMainFrame)
    , m_pSubLock(nullptr)
    , m_pMainFrame(pMainFrame)
    , m_fps(0.0)
    , m_lastSegment(-1)
    , m_rt(0)
    , m_mode(NONE)
    , m_external(false)
	, createdWindow(false)
{
    GetEventd().Connect(m_eventc, {
        MpcEvent::DPI_CHANGED,
    }, std::bind(&CPlayerSubresyncBar::EventCallback, this, std::placeholders::_1));
}

CPlayerSubresyncBar::~CPlayerSubresyncBar()
{
}

BOOL CPlayerSubresyncBar::Create(CWnd* pParentWnd, UINT defDockBarID, CCritSec* pSubLock)
{
    if (!__super::Create(ResStr(IDS_SUBRESYNC_CAPTION), pParentWnd, ID_VIEW_SUBRESYNC, defDockBarID, _T("Subresync"))) {
        return FALSE;
    }

    m_pSubLock = pSubLock;

    m_list.CreateEx(
        WS_EX_DLGMODALFRAME | WS_EX_CLIENTEDGE,
        WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_TABSTOP | LVS_REPORT | LVS_OWNERDATA /*|LVS_SHOWSELALWAYS*/ | LVS_AUTOARRANGE | LVS_NOSORTHEADER,
        CRect(0, 0, 100, 100), this, IDC_SUBRESYNCLIST);

    ScaleFont();

    //m_list.SetExtendedStyle(m_list.GetExtendedStyle() | LVS_EX_FULLROWSELECT | LVS_EX_DOUBLEBUFFER);
    m_list.setAdditionalStyles(LVS_EX_FULLROWSELECT | LVS_EX_DOUBLEBUFFER);
    m_list.setAdditionalStyles(WS_CLIPCHILDREN, false);
    m_list.setColorInterface(this);
    m_strYes = m_strYesMenu = ResStr(IDS_SUBRESYNC_YES);
    m_strNo = m_strNoMenu = ResStr(IDS_SUBRESYNC_NO);
    m_strYes.Remove(_T('&'));
    m_strNo.Remove(_T('&'));

	createdWindow = true;

    return TRUE;
}

BOOL CPlayerSubresyncBar::PreCreateWindow(CREATESTRUCT& cs)
{
    if (!__super::PreCreateWindow(cs)) {
        return FALSE;
    }

    return TRUE;
}

BOOL CPlayerSubresyncBar::PreTranslateMessage(MSG* pMsg)
{
    if (IsWindow(pMsg->hwnd) && IsVisible() && pMsg->message >= WM_KEYFIRST && pMsg->message <= WM_KEYLAST) {
        if (HandleShortCuts(pMsg) || IsDialogMessage(pMsg)) {
            return TRUE;
        }
    }

    return __super::PreTranslateMessage(pMsg);
}

void CPlayerSubresyncBar::ReloadTranslatableResources()
{
    SetWindowText(ResStr(IDS_SUBRESYNC_CAPTION));

    m_strYes = m_strYesMenu = ResStr(IDS_SUBRESYNC_YES);
    m_strNo = m_strNoMenu = ResStr(IDS_SUBRESYNC_NO);
    m_strYes.Remove(_T('&'));
    m_strNo.Remove(_T('&'));

    CHeaderCtrl* pHeaderCtrl = m_list.GetHeaderCtrl();
    if (pHeaderCtrl && pHeaderCtrl->GetItemCount() > 4) {
        auto setColumnHeaderText = [pHeaderCtrl](int nPos, CString str) {
            HDITEM item;
            item.mask = HDI_TEXT;
            item.pszText = (LPTSTR)(LPCTSTR)str;
            item.cchTextMax = str.GetLength() + 1;
            VERIFY(pHeaderCtrl->SetItem(nPos, &item));
        };

        setColumnHeaderText(COL_START, StrRes(IDS_SUBRESYNC_CLN_TIME));
        setColumnHeaderText(COL_END, StrRes(IDS_SUBRESYNC_CLN_END));
        setColumnHeaderText(COL_PREVSTART, StrRes(IDS_SUBRESYNC_CLN_PREVIEW));
        setColumnHeaderText(COL_PREVEND, StrRes(IDS_SUBRESYNC_CLN_END));
        if (m_mode == VOBSUB) {
            ASSERT(pHeaderCtrl->GetItemCount() == COL_COUNT_VOBSUB);
            setColumnHeaderText(COL_VOBID, StrRes(IDS_SUBRESYNC_CLN_VOB_ID));
            setColumnHeaderText(COL_CELLID, StrRes(IDS_SUBRESYNC_CLN_CELL_ID));
            setColumnHeaderText(COL_FORCED, StrRes(IDS_SUBRESYNC_CLN_FORCED));
        } else if (m_mode == TEXTSUB) {
            ASSERT(pHeaderCtrl->GetItemCount() == COL_COUNT_TEXTSUB);
            setColumnHeaderText(COL_TEXT, StrRes(IDS_SUBRESYNC_CLN_TEXT));
            setColumnHeaderText(COL_STYLE, StrRes(IDS_SUBRESYNC_CLN_STYLE));
            setColumnHeaderText(COL_FONT, StrRes(IDS_SUBRESYNC_CLN_FONT));
            setColumnHeaderText(COL_CHARSET, StrRes(IDS_SUBRESYNC_CLN_CHARSET));
            setColumnHeaderText(COL_UNICODE, StrRes(IDS_SUBRESYNC_CLN_UNICODE));
            setColumnHeaderText(COL_LAYER, StrRes(IDS_SUBRESYNC_CLN_LAYER));
            setColumnHeaderText(COL_ACTOR, StrRes(IDS_SUBRESYNC_CLN_ACTOR));
            setColumnHeaderText(COL_EFFECT, StrRes(IDS_SUBRESYNC_CLN_EFFECT));
        }
    }
}

void CPlayerSubresyncBar::SetTime(REFERENCE_TIME rt)
{
    if (m_rt != rt) {
        m_rt = rt;
        int curSegment;

        if (!m_sts.SearchSubs(rt, 25, &curSegment)) {
            curSegment = -1;
        }

        if (m_lastSegment != curSegment) {
            m_list.Invalidate();
        }
        m_lastSegment = curSegment;
    }
}

void CPlayerSubresyncBar::SetFPS(double fps)
{
    if (m_fps != fps) {
        m_fps = fps;

        ReloadSubtitle();
    }
}

void CPlayerSubresyncBar::SetSubtitle(ISubStream* pSubStream, double fps, bool external)
{
    // Avoid reloading the same subtitles again
    if (m_pSubStream != pSubStream || m_fps != fps) {
        m_pSubStream = pSubStream;
        m_fps = fps;
        m_external = external;

        // FIXME: if subresync bar is not visible, then delay this until we enable it
        ReloadSubtitle();
    }
}

bool CPlayerSubresyncBar::RefreshEmbeddedTextSubtitleData()
{
    if (!m_pSubStream) {
        return false;
    }
    if (m_external || m_mode != TEXTSUB) {
        return false;
    }

    CLSID clsid;
    m_pSubStream->GetClassID(&clsid);

    if (clsid == __uuidof(CRenderedTextSubtitle)) {
        CRenderedTextSubtitle* pRTS = (CRenderedTextSubtitle*)(ISubStream*)m_pSubStream;

        m_lastSegment = -1;

        pRTS->Lock();
        m_sts.Copy(*pRTS);
        pRTS->Unlock();
        m_sts.ConvertToTimeBased(m_fps);
        m_sts.Sort(true);

        m_subtimes.resize(m_sts.GetCount());

        for (size_t i = 0, j = m_sts.GetCount(); i < j; i++) {
            m_subtimes[i].orgStart = m_sts[i].start;
            m_subtimes[i].orgEnd = m_sts[i].end;
        }

        ResetSubtitle();

        return true;
    } else {
        return false;
    }
}

void CPlayerSubresyncBar::ReloadSubtitle()
{
    m_mode = NONE;
    m_lastSegment = -1;
    m_sts.Empty();
    m_vobSub.RemoveAll();

    ResetSubtitle();

    CHeaderCtrl* hctrl = m_list.GetHeaderCtrl();
    if (hctrl) {
        for (int i = 0, count = hctrl->GetItemCount(); i < count; i++) {
            m_list.DeleteColumn(0);
        }
    }

    if (!m_pSubStream) {
        return;
    }

    CLSID clsid;
    m_pSubStream->GetClassID(&clsid);

    if (clsid == __uuidof(CVobSubFile)) {
        CVobSubFile* pVSF = (CVobSubFile*)(ISubStream*)m_pSubStream;

        m_mode = VOBSUB;

        pVSF->Lock();
        ASSERT(pVSF->m_nLang < pVSF->m_langs.size());
        m_vobSub.Copy(pVSF->m_langs[pVSF->m_nLang].subpos);

        for (size_t i = 0, j = m_vobSub.GetCount(); i < j; i++) {
            CString str;
            str.Format(_T("%d,%d,%d,%Iu"), m_vobSub[i].vobid, m_vobSub[i].cellid, m_vobSub[i].bForced, i);
            m_sts.Add(TToW(str), false, (int)m_vobSub[i].start, (int)m_vobSub[i].stop);
        }
        pVSF->Unlock();

        m_sts.CreateDefaultStyle(DEFAULT_CHARSET);

        pVSF->m_bOnlyShowForcedSubs = false;

        m_list.InsertColumn(COL_START, ResStr(IDS_SUBRESYNC_CLN_TIME), LVCFMT_LEFT, 80);
        m_list.InsertColumn(COL_END, ResStr(IDS_SUBRESYNC_CLN_END), LVCFMT_LEFT, 80);
        m_list.InsertColumn(COL_PREVSTART, ResStr(IDS_SUBRESYNC_CLN_PREVIEW), LVCFMT_LEFT, 80);
        m_list.InsertColumn(COL_PREVEND, ResStr(IDS_SUBRESYNC_CLN_END), LVCFMT_LEFT, 80);
        m_list.InsertColumn(COL_VOBID, ResStr(IDS_SUBRESYNC_CLN_VOB_ID), LVCFMT_CENTER, 60);
        m_list.InsertColumn(COL_CELLID, ResStr(IDS_SUBRESYNC_CLN_CELL_ID), LVCFMT_CENTER, 60);
        m_list.InsertColumn(COL_FORCED, ResStr(IDS_SUBRESYNC_CLN_FORCED), LVCFMT_CENTER, 60);
    } else if (clsid == __uuidof(CRenderedTextSubtitle)) {
        CRenderedTextSubtitle* pRTS = (CRenderedTextSubtitle*)(ISubStream*)m_pSubStream;

        m_mode = TEXTSUB;

        pRTS->Lock();
        m_sts.Copy(*pRTS);
        pRTS->Unlock();
        m_sts.ConvertToTimeBased(m_fps);
        m_sts.Sort(true);

        m_list.InsertColumn(COL_START, ResStr(IDS_SUBRESYNC_CLN_TIME), LVCFMT_LEFT, 90);
        m_list.InsertColumn(COL_END, ResStr(IDS_SUBRESYNC_CLN_END), LVCFMT_LEFT, 4);
        m_list.InsertColumn(COL_PREVSTART, ResStr(IDS_SUBRESYNC_CLN_PREVIEW), LVCFMT_LEFT, 80);
        m_list.InsertColumn(COL_PREVEND, ResStr(IDS_SUBRESYNC_CLN_END), LVCFMT_LEFT, 4);
        m_list.InsertColumn(COL_TEXT, ResStr(IDS_SUBRESYNC_CLN_TEXT), LVCFMT_LEFT, 275);
        m_list.InsertColumn(COL_STYLE, ResStr(IDS_SUBRESYNC_CLN_STYLE), LVCFMT_LEFT, 80);
        m_list.InsertColumn(COL_FONT, ResStr(IDS_SUBRESYNC_CLN_FONT), LVCFMT_LEFT, 60);
        m_list.InsertColumn(COL_CHARSET, ResStr(IDS_SUBRESYNC_CLN_CHARSET), LVCFMT_CENTER, 20);
        m_list.InsertColumn(COL_UNICODE, ResStr(IDS_SUBRESYNC_CLN_UNICODE), LVCFMT_CENTER, 40);
        m_list.InsertColumn(COL_LAYER, ResStr(IDS_SUBRESYNC_CLN_LAYER), LVCFMT_CENTER, 50);
        m_list.InsertColumn(COL_ACTOR, ResStr(IDS_SUBRESYNC_CLN_ACTOR), LVCFMT_LEFT, 80);
        m_list.InsertColumn(COL_EFFECT, ResStr(IDS_SUBRESYNC_CLN_EFFECT), LVCFMT_LEFT, 80);
    }

    m_subtimes.resize(m_sts.GetCount());

    for (size_t i = 0, j = m_sts.GetCount(); i < j; i++) {
        m_subtimes[i].orgStart = m_sts[i].start;
        m_subtimes[i].orgEnd = m_sts[i].end;
    }

    ResetSubtitle();
}

void CPlayerSubresyncBar::ResetSubtitle()
{
    m_list.SetRedraw(FALSE);
    m_list.DeleteAllItems();

    if (m_mode == VOBSUB || m_mode == TEXTSUB) {
        REFERENCE_TIME prevstart = INT64_MIN;

        size_t nCount = m_sts.GetCount();
        m_displayData.resize(nCount);
        m_newStartsIndex.clear();

        auto itHint = m_newStartsIndex.end();
        for (size_t i = 0; i < nCount; i++) {
            m_subtimes[i].newStart = m_subtimes[i].orgStart;
            m_subtimes[i].newEnd = m_subtimes[i].orgEnd;
            // In general subtitle files are more or less sorted so try to use that to insert more efficiently
            m_subtimes[i].itIndex = itHint = m_newStartsIndex.emplace_hint(itHint, m_subtimes[i].newStart, i);
            ++itHint;

            m_displayData[i].tStart = m_displayData[i].tPrevStart = m_subtimes[i].orgStart;
            m_displayData[i].tEnd = m_displayData[i].tPrevEnd = m_subtimes[i].orgEnd;

            m_displayData[i].flags = (prevstart > m_subtimes[i].orgStart) ? TSEP : 0;
            prevstart = m_subtimes[i].orgStart;
        }

        m_list.SetItemCount((int)nCount);

        // Since all items in COL_START and COL_PREVSTART have the same text size,
        // we can compute it for one sample string. We can't use auto-resize because
        // the list is in virtual mode so we have to guess the right extra margin.
        int width = m_list.GetStringWidth(_T("00:00:00.000"));
        m_list.SetColumnWidth(COL_START, width + 12);
        m_list.SetColumnWidth(COL_PREVSTART, width + 14);
    }

    m_list.SetRedraw(TRUE);
    m_list.Invalidate();
    m_list.UpdateWindow();
}

void CPlayerSubresyncBar::SaveSubtitle()
{
    if (CMainFrame* pMainFrame = AfxGetMainFrame()) {
        CLSID clsid;
        m_pSubStream->GetClassID(&clsid);

        if (clsid == __uuidof(CVobSubFile) && m_mode == VOBSUB) {
            CVobSubFile* pVSF = (CVobSubFile*)(ISubStream*)m_pSubStream;

            CAutoLock cAutoLock(m_pSubLock);

            ASSERT(pVSF->m_nLang < pVSF->m_langs.size());
            CAtlArray<CVobSubFile::SubPos>& sp = pVSF->m_langs[pVSF->m_nLang].subpos;

            for (size_t i = 0, j = sp.GetCount(); i < j; i++) {
                sp[i].bValid = false;
            }

            for (size_t i = 0, j = m_sts.GetCount(); i < j; i++) {
                int spnum = m_sts[i].readorder;

                sp[spnum].start  = m_sts[i].start;
                sp[spnum].stop   = m_sts[i].end;
                sp[spnum].bValid = true;
            }
        } else if (clsid == __uuidof(CRenderedTextSubtitle) && m_mode == TEXTSUB) {
            CRenderedTextSubtitle* pRTS = (CRenderedTextSubtitle*)(ISubStream*)m_pSubStream;

            CAutoLock cAutoLock(m_pSubLock);

            pRTS->Copy(m_sts);
        } else {
            return;
        }

        pMainFrame->InvalidateSubtitle();
    }
}

void CPlayerSubresyncBar::ScaleFont()
{
    LOGFONT lf;
    GetMessageFont(&lf);
    lf.lfHeight = m_pMainFrame->m_dpi.ScaleSystemToOverrideY(lf.lfHeight);

    m_font.DeleteObject();
    if (m_font.CreateFontIndirect(&lf)) {
        m_list.SetFont(&m_font);
    }
}

void CPlayerSubresyncBar::EventCallback(MpcEvent ev)
{
    switch (ev) {
        case MpcEvent::DPI_CHANGED:
            InitializeSize();
            ScaleFont();
            m_list.SetWindowPos(nullptr, 0, 0, 0, 0, SWP_FRAMECHANGED | SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
            break;

        default:
            ASSERT(FALSE);
    }
}

void CPlayerSubresyncBar::UpdatePreview()
{
    if (m_mode == VOBSUB || m_mode == TEXTSUB) {
        CAtlArray<int> schk;

        for (int i = 0, j = (int)m_sts.GetCount(); i < j;) {
            schk.RemoveAll();

            int start = i, end;

            for (end = i; end < j; end++) {
                int data = m_displayData[end].flags;
                if ((data & TSEP) && end > i) {
                    break;
                }
                if (data & (TSMOD | TSADJ)) {
                    schk.Add(end);
                }
            }

            if (schk.IsEmpty()) {
                for (; start < end; start++) {
                    m_sts[start].start = m_subtimes[start].orgStart;
                    m_sts[start].end = m_subtimes[start].orgEnd;
                }
            } else if (schk.GetCount() == 1) {
                int k = schk[0];
                REFERENCE_TIME dt = m_subtimes[k].newStart - m_subtimes[k].orgStart;
                for (; start < end; start++) {
                    m_sts[start].start = m_subtimes[start].orgStart + dt;
                    m_sts[start].end = (m_displayData[start].flags & TEMOD)
                                       ? m_subtimes[start].newEnd
                                       : (m_subtimes[start].orgEnd + dt);
                }
            } else if (schk.GetCount() >= 2) {
                int i0 = 0;
                int i1 = 0;
                REFERENCE_TIME ti0 = 0;
                REFERENCE_TIME ds = 0;
                double m = 0;

                int k, l;
                for (k = 0, l = (int)schk.GetCount() - 1; k < l; k++) {
                    i0 = schk[k];
                    i1 = schk[k + 1];

                    ti0 = m_subtimes[i0].orgStart;
                    ds = m_subtimes[i1].orgStart - ti0;

                    if (ds == 0) {
                        SetSTS0(start, i1, ti0);
                    } else {
                        m = double(m_subtimes[i1].newStart - m_subtimes[i0].newStart) / ds;
                        SetSTS1(start, i1, ti0, m, i0);
                    }

                }

                ASSERT(k > 0);
                if (ds == 0) {
                    SetSTS0(start, end, ti0);
                } else {
                    SetSTS1(start, end, ti0, m, i0);
                }
            }

            i = end;
        }

        m_sts.CreateSegments();

        for (size_t i = 0, j = m_sts.GetCount(); i < j; i++) {
            m_displayData[i].tPrevStart = m_sts[i].start;
            m_displayData[i].tPrevEnd = m_sts[i].end;
        }

        m_list.Invalidate();

        if (IsWindowVisible()) {
            SaveSubtitle();
        }
    }
}

void CPlayerSubresyncBar::SetSTS0(int& start, int end, REFERENCE_TIME ti0)
{
    for (; start < end; start++) {
        m_sts[start].start = ti0;
        REFERENCE_TIME endpos;
        if (m_displayData[start].flags & TEMOD) {
            endpos = m_subtimes[start].newEnd;
        } else {
            endpos = ti0 + m_subtimes[start].orgEnd - m_subtimes[start].orgStart;
        }
        m_sts[start].end = endpos;
    }
}

void CPlayerSubresyncBar::SetSTS1(int& start, int end, REFERENCE_TIME ti0, double m, int i0)
{
    for (; start < end; start++) {
        m_sts[start].start = REFERENCE_TIME((m_subtimes[start].orgStart - ti0) * m + m_subtimes[i0].newStart);
        REFERENCE_TIME endpos;
        if (m_displayData[start].flags & TEMOD) {
            endpos = m_subtimes[start].newEnd;
        } else {
            REFERENCE_TIME diff = m_subtimes[start].orgEnd - m_subtimes[start].orgStart;
            if (m_mode == VOBSUB) {
                endpos = m_sts[start].start + diff;
            } else {
                endpos = m_sts[start].start + REFERENCE_TIME(diff * m);
            }
        }
        m_sts[start].end = endpos;
    }
}

void CPlayerSubresyncBar::GetCheck(int iItem, bool& bStartMod, bool& bEndMod, bool& bStartAdj, bool& bEndAdj) const
{
    if (0 <= iItem && (size_t)iItem < m_sts.GetCount()) {
        int nCheck = m_displayData[iItem].flags;
        bStartMod = !!(nCheck & TSMOD);
        bEndMod = !!(nCheck & TEMOD);
        bStartAdj = !!(nCheck & TSADJ);
        bEndAdj = !!(nCheck & TEADJ);
    }
}

void CPlayerSubresyncBar::SetCheck(int iItem, bool bStart, bool bEnd)
{
    if (0 <= iItem && (size_t)iItem < m_sts.GetCount()) {
        const SubTime& st = m_subtimes[iItem];

        int nCheck = m_displayData[iItem].flags & TSEP;

        if (bStart) {
            nCheck |= TSMOD;
        } else if (abs(st.orgStart - st.newStart)) {
            nCheck |= TSADJ;
        }
        if (bEnd) {
            nCheck |= TEMOD;
        } else if (abs(st.orgEnd - st.newEnd)) {
            nCheck |= TEADJ;
        }

        m_displayData[iItem].flags = nCheck;
        m_displayData[iItem].tStart = bStart ? m_subtimes[iItem].newStart : m_subtimes[iItem].orgStart;
        m_displayData[iItem].tEnd = bEnd ? m_subtimes[iItem].newEnd : m_subtimes[iItem].orgEnd;
    }
}

bool CPlayerSubresyncBar::ModStart(int iItem, REFERENCE_TIME t, bool bReset)
{
    bool bRet = false;
    bool bStartMod, bEndMod, bStartAdj, bEndAdj;

    GetCheck(iItem, bStartMod, bEndMod, bStartAdj, bEndAdj);

    SubTime& st = m_subtimes[iItem];

    //if (bStartMod || bStartAdj || st.orgStart != t || bReset)
    {
        bRet = (st.newStart != t);

        st.newStart = t;
        if (!bEndMod) {
            st.newEnd = st.newStart + (st.orgEnd - st.orgStart);
        } else if (bReset) {
            st.newStart = st.newEnd - (st.orgEnd - st.orgStart);
        }
        m_newStartsIndex.erase(st.itIndex);
        st.itIndex = m_newStartsIndex.emplace(st.newStart, size_t(iItem));

        SetCheck(iItem, !bReset, bEndMod);
    }

    return bRet;
}

bool CPlayerSubresyncBar::ModEnd(int iItem, REFERENCE_TIME t, bool bReset)
{
    bool bRet = false;
    bool bStartMod, bEndMod, bStartAdj, bEndAdj;

    GetCheck(iItem, bStartMod, bEndMod, bStartAdj, bEndAdj);

    SubTime& st = m_subtimes[iItem];

    //if (bEndMod || bEndAdj || st.orgEnd != t || bReset)
    {
        bRet = (st.newEnd != t);

        st.newEnd = t;
        if (!bStartMod) {
            st.newStart = st.newEnd - (st.orgEnd - st.orgStart);
            m_newStartsIndex.erase(st.itIndex);
            st.itIndex = m_newStartsIndex.emplace(st.newStart, size_t(iItem));
        } else if (bReset) {
            st.newEnd = st.newStart + (st.orgEnd - st.orgStart);
        }

        SetCheck(iItem, bStartMod, !bReset);
    }

    return bRet;
}

BEGIN_MESSAGE_MAP(CPlayerSubresyncBar, CMPCThemePlayerBar)
    ON_WM_MEASUREITEM()
    ON_WM_SIZE()
    ON_NOTIFY(LVN_GETDISPINFO, IDC_SUBRESYNCLIST, OnGetDisplayInfo)
    ON_NOTIFY(LVN_BEGINLABELEDIT, IDC_SUBRESYNCLIST, OnBeginlabeleditList)
    ON_NOTIFY(LVN_DOLABELEDIT, IDC_SUBRESYNCLIST, OnDolabeleditList)
    ON_NOTIFY(LVN_ENDLABELEDIT, IDC_SUBRESYNCLIST, OnEndlabeleditList)
    ON_NOTIFY(NM_RCLICK, IDC_SUBRESYNCLIST, OnRclickList)
    ON_NOTIFY(NM_DBLCLK, IDC_SUBRESYNCLIST, OnNMDblclkList)
    ON_NOTIFY(LVN_KEYDOWN, IDC_SUBRESYNCLIST, OnLvnKeydownList)
    ON_NOTIFY(NM_CUSTOMDRAW, IDC_SUBRESYNCLIST, OnCustomdrawList)
END_MESSAGE_MAP()


// CPlayerSubresyncBar message handlers

void CPlayerSubresyncBar::OnSize(UINT nType, int cx, int cy)
{
    __super::OnSize(nType, cx, cy);

    if (::IsWindow(m_list.m_hWnd)) {
        CRect r;
        GetClientRect(r);
        r.DeflateRect(2, 2);
        m_list.MoveWindow(r);
    }
}

void CPlayerSubresyncBar::OnGetDisplayInfo(NMHDR* pNMHDR, LRESULT* pResult)
{
    NMLVDISPINFO* pDispInfo = (NMLVDISPINFO*)pNMHDR;
    LV_ITEM* pItem = &pDispInfo->item;

    if (pItem->iItem < 0 || size_t(pItem->iItem) >= m_displayData.size()) {
        return;
    }

    if (pItem->mask & LVIF_TEXT) {
        auto formatTime = [](REFERENCE_TIME t, TCHAR * buff, size_t buffLen) {
            int ms = int(RT2MS(abs(t)));
            int s = ms / 1000;
            int m = s / 60;
            _stprintf_s(buff, buffLen, t >= 0
                        ? _T("%02d:%02d:%02d.%03d")
                        : _T("-%02d:%02d:%02d.%03d"),
                        m / 60,
                        m % 60,
                        s % 60,
                        ms % 1000);
        };

        switch (pItem->iSubItem) {
            case COL_START:
                formatTime(m_displayData[pItem->iItem].tStart, pItem->pszText, pItem->cchTextMax);
                break;
            case COL_PREVSTART:
                formatTime(m_displayData[pItem->iItem].tPrevStart, pItem->pszText, pItem->cchTextMax);
                break;
            case COL_END:
                formatTime(m_displayData[pItem->iItem].tEnd, pItem->pszText, pItem->cchTextMax);
                break;
            case COL_PREVEND:
                formatTime(m_displayData[pItem->iItem].tPrevEnd, pItem->pszText, pItem->cchTextMax);
                break;
            default:
                if (m_mode == TEXTSUB) {
                    OnGetDisplayInfoTextSub(pItem);
                } else if (m_mode == VOBSUB) {
                    OnGetDisplayInfoVobSub(pItem);
                }
                break;
        }
    }
}

void CPlayerSubresyncBar::OnGetDisplayInfoTextSub(LV_ITEM* pItem)
{
    if (pItem->mask & LVIF_TEXT) {
        const STSEntry& entry = m_sts[pItem->iItem];

        switch (pItem->iSubItem) {
            case COL_TEXT:
                m_displayBuffer = m_sts.GetStrW(pItem->iItem, true);
                pItem->pszText = (LPWSTR)(LPCWSTR)m_displayBuffer;
                break;
            case COL_STYLE:
                pItem->pszText = (LPWSTR)(LPCWSTR)entry.style;
                break;
            case COL_FONT:
                if (const STSStyle* style = m_sts.GetStyle(pItem->iItem)) {
                    pItem->pszText = (LPWSTR)(LPCWSTR)style->fontName;
                }
                break;
            case COL_CHARSET:
                if (const STSStyle* style = m_sts.GetStyle(pItem->iItem)) {
                    _stprintf_s(pItem->pszText, pItem->cchTextMax, _T("%d"), style->charSet);
                }
                break;
            case COL_UNICODE:
                pItem->pszText = (LPWSTR)(LPCWSTR)(entry.fUnicode ? m_strYes : m_strNo);
                break;
            case COL_LAYER:
                _stprintf_s(pItem->pszText, pItem->cchTextMax, _T("%d"), entry.layer);
                break;
            case COL_ACTOR:
                pItem->pszText = (LPWSTR)(LPCWSTR)entry.actor;
                break;
            case COL_EFFECT:
                pItem->pszText = (LPWSTR)(LPCWSTR)entry.effect;
                break;
            default:
                ASSERT(FALSE);
                break;
        }
    }
}

void CPlayerSubresyncBar::OnGetDisplayInfoVobSub(LV_ITEM* pItem)
{
    if (pItem->mask & LVIF_TEXT) {
        const CVobSubFile::SubPos& entry = m_vobSub[m_sts[pItem->iItem].readorder];

        switch (pItem->iSubItem) {
            case COL_VOBID:
                _stprintf_s(pItem->pszText, pItem->cchTextMax, _T("%d"), entry.vobid);
                break;
            case COL_CELLID:
                _stprintf_s(pItem->pszText, pItem->cchTextMax, _T("%d"), entry.cellid);
                break;
            case COL_FORCED:
                pItem->pszText = (LPWSTR)(LPCWSTR)(entry.bForced ? m_strYes : m_strNo);
                break;
            default:
                ASSERT(FALSE);
                break;
        }
    }
}

static bool ParseTime(CString str, REFERENCE_TIME& ret, bool bWarn = true)
{
    int sign = 1, h, m, s, ms;
    TCHAR c;

    str.Trim();
    if (!str.IsEmpty() && str[0] == '-') {
        sign = -1;
    }

    int n = _stscanf_s(str, _T("%d%c%d%c%d%c%d"), &h, &c, 1, &m, &c, 1, &s, &c, 1, &ms);

    h = abs(h);

    if (n == 7
            && 0 <= h && h < 24
            && 0 <= m && m < 60
            && 0 <= s && s < 60
            && 0 <= ms && ms < 1000) {
        ret = MS2RT(sign * (h * 60 * 60 * 1000 + m * 60 * 1000 + s * 1000 + ms));
        return true;
    }

    if (bWarn) {
        AfxMessageBox(IDS_SUBRESYNC_TIME_FORMAT, MB_ICONEXCLAMATION | MB_OK, 0);
    }
    return false;
}

void CPlayerSubresyncBar::OnBeginlabeleditList(NMHDR* pNMHDR, LRESULT* pResult)
{
    LV_DISPINFO* pDispInfo = (LV_DISPINFO*)pNMHDR;
    LV_ITEM* pItem = &pDispInfo->item;

    *pResult = FALSE;

    if (pItem->iItem >= 0) {
        if ((pItem->iSubItem == COL_START || pItem->iSubItem == COL_END || pItem->iSubItem == COL_TEXT
                || pItem->iSubItem == COL_STYLE || pItem->iSubItem == COL_LAYER
                || pItem->iSubItem == COL_ACTOR || pItem->iSubItem == COL_EFFECT)
                && m_mode == TEXTSUB) {
            *pResult = TRUE;
        } else if ((pItem->iSubItem == COL_START)
                   && m_mode == VOBSUB) {
            *pResult = TRUE;
        }
    }
}

void CPlayerSubresyncBar::OnDolabeleditList(NMHDR* pNMHDR, LRESULT* pResult)
{
    LV_DISPINFO* pDispInfo = (LV_DISPINFO*)pNMHDR;
    LV_ITEM* pItem = &pDispInfo->item;

    *pResult = FALSE;

    if (pItem->iItem >= 0) {
        if ((pItem->iSubItem == COL_START || pItem->iSubItem == COL_END || pItem->iSubItem == COL_TEXT
                || pItem->iSubItem == COL_STYLE || pItem->iSubItem == COL_LAYER
                || pItem->iSubItem == COL_ACTOR || pItem->iSubItem == COL_EFFECT)
                && m_mode == TEXTSUB) {
            m_list.ShowInPlaceEdit(pItem->iItem, pItem->iSubItem);
            *pResult = TRUE;
        } else if ((pItem->iSubItem == COL_START)
                   && m_mode == VOBSUB) {
            m_list.ShowInPlaceEdit(pItem->iItem, pItem->iSubItem);
            *pResult = TRUE;
        }
    }
}

void CPlayerSubresyncBar::OnEndlabeleditList(NMHDR* pNMHDR, LRESULT* pResult)
{
    LV_DISPINFO* pDispInfo = (LV_DISPINFO*)pNMHDR;
    LV_ITEM* pItem = &pDispInfo->item;

    *pResult = FALSE;

    if (!m_list.m_fInPlaceDirty) {
        return;
    }

    if (pItem->iItem >= 0 && pItem->pszText && (m_mode == VOBSUB || m_mode == TEXTSUB)) {
        bool bNeedsUpdate = false;

        switch (pItem->iSubItem) {
            case COL_START: {
                REFERENCE_TIME t;
                if (ParseTime(pItem->pszText, t)) {
                    bNeedsUpdate = ModStart(pItem->iItem, t);

                    *pResult = TRUE;
                }
            }
            break;
            case COL_END:
                if (m_mode == TEXTSUB) {
                    REFERENCE_TIME t;
                    if (ParseTime(pItem->pszText, t)) {
                        bNeedsUpdate = ModEnd(pItem->iItem, t);

                        *pResult = TRUE;
                    }
                }
                break;
            case COL_TEXT:
                if (m_mode == TEXTSUB) {
                    CString str = m_sts.GetStrW(pItem->iItem, true);

                    if (str != pItem->pszText) {
                        bNeedsUpdate = true;
                        m_sts.SetStr(pItem->iItem, CString(pItem->pszText), true);
                    }
                }
                break;
            case COL_STYLE:
                if (m_mode == TEXTSUB) {
                    CString str(pItem->pszText);
                    str.Trim();

                    if (!str.IsEmpty() && m_sts[pItem->iItem].style != str) {
                        bNeedsUpdate = true;

                        if (!m_sts.m_styles.Lookup(str)) {
                            m_sts.AddStyle(str, DEBUG_NEW STSStyle());
                        }

                        m_sts[pItem->iItem].style = str;
                    }
                }
                break;
            case COL_LAYER:
                if (m_mode == TEXTSUB) {
                    int l;
                    if (_stscanf_s(pItem->pszText, _T("%d"), &l) == 1) {
                        bNeedsUpdate = true;
                        m_sts[pItem->iItem].layer = l;
                    }
                }
                break;
            case COL_ACTOR:
                if (m_mode == TEXTSUB) {
                    CString str(pItem->pszText);
                    str.Trim();
                    if (!str.IsEmpty()) {
                        bNeedsUpdate = true;
                        m_sts[pItem->iItem].actor = str;
                    }
                }
                break;
            case COL_EFFECT:
                if (m_mode == TEXTSUB) {
                    CString str(pItem->pszText);
                    str.Trim();
                    if (!str.IsEmpty()) {
                        bNeedsUpdate = true;
                        m_sts[pItem->iItem].effect = str;
                    }
                }
                break;
        }

        if (bNeedsUpdate) {
            UpdatePreview();
            m_list.Update(pItem->iItem);
        }
    }
}

void CPlayerSubresyncBar::OnRclickList(NMHDR* pNMHDR, LRESULT* pResult)
{
    LPNMLISTVIEW lpnmlv = (LPNMLISTVIEW)pNMHDR;

    if (lpnmlv->iItem >= 0 && lpnmlv->iSubItem >= 0) {
        enum {
            TOGSEP = 1,
            DUPITEM, DELITEM,
            RESETS, SETOS, SETCS, RESETE, SETOE, SETCE,
            STYLEFIRST, STYLELAST = STYLEFIRST + 1000, STYLEEDIT,
            UNICODEYES, UNICODENO,
            LAYERDEC, LAYERINC,
            ACTORFIRST, ACTORLAST = ACTORFIRST + 1000,
            EFFECTFIRST, EFFECTLAST = EFFECTFIRST + 1000
        };

        CStringArray stylesNames;
        CStringArray actors;
        CStringArray effects;

        CMPCThemeMenu m;
        m.CreatePopupMenu();

        if (m_mode == VOBSUB || m_mode == TEXTSUB) {
            m.AppendMenu(MF_STRING | MF_ENABLED, TOGSEP, ResStr(IDS_SUBRESYNC_SEPARATOR));
            m.AppendMenu(MF_SEPARATOR);
            if (m_mode == TEXTSUB) {
                m.AppendMenu(MF_STRING | MF_ENABLED, DUPITEM, ResStr(IDS_SUBRESYNC_DUPLICATE));
            }
            m.AppendMenu(MF_STRING | MF_ENABLED, DELITEM, ResStr(IDS_SUBRESYNC_DELETE));
        }

        switch (lpnmlv->iSubItem) {
            case COL_START:
                if (m_mode == VOBSUB || m_mode == TEXTSUB) {
                    m.AppendMenu(MF_SEPARATOR);
                    m.AppendMenu(MF_STRING | MF_ENABLED, RESETS, ResStr(IDS_SUBRESYNC_RESET) + _T("\tF1"));
                    m.AppendMenu(MF_STRING | MF_ENABLED, SETOS, ResStr(IDS_SUBRESYNC_ORIGINAL) + _T("\tF3"));
                    m.AppendMenu(MF_STRING | MF_ENABLED, SETCS, ResStr(IDS_SUBRESYNC_CURRENT) + _T("\tF5"));
                }
                break;
            case COL_END:
                if (m_mode == TEXTSUB) {
                    m.AppendMenu(MF_SEPARATOR);
                    m.AppendMenu(MF_STRING | MF_ENABLED, RESETE, ResStr(IDS_SUBRESYNC_RESET) + _T("\tF2"));
                    m.AppendMenu(MF_STRING | MF_ENABLED, SETOE, ResStr(IDS_SUBRESYNC_ORIGINAL) + _T("\tF4"));
                    m.AppendMenu(MF_STRING | MF_ENABLED, SETCE, ResStr(IDS_SUBRESYNC_CURRENT) + _T("\tF6"));
                }
                break;
            case COL_STYLE:
                if (m_mode == TEXTSUB) {
                    m.AppendMenu(MF_SEPARATOR);

                    int id = STYLEFIRST;

                    POSITION pos = m_sts.m_styles.GetStartPosition();
                    while (pos && id <= STYLELAST) {
                        CString key;
                        STSStyle* val;
                        m_sts.m_styles.GetNextAssoc(pos, key, val);
                        stylesNames.Add(key);
                        m.AppendMenu(MF_STRING | MF_ENABLED, id++, key);
                    }

                    if (id > STYLEFIRST && m_list.GetSelectedCount() == 1) {
                        m.AppendMenu(MF_SEPARATOR);
                        m.AppendMenu(MF_STRING | MF_ENABLED, STYLEEDIT, ResStr(IDS_SUBRESYNC_EDIT));
                    }
                }
                break;
            case COL_UNICODE:
                if (m_mode == TEXTSUB) {
                    m.AppendMenu(MF_SEPARATOR);
                    m.AppendMenu(MF_STRING | MF_ENABLED, UNICODEYES, m_strYesMenu);
                    m.AppendMenu(MF_STRING | MF_ENABLED, UNICODENO, m_strNoMenu);
                }
                break;
            case COL_LAYER:
                if (m_mode == TEXTSUB) {
                    m.AppendMenu(MF_SEPARATOR);
                    m.AppendMenu(MF_STRING | MF_ENABLED, LAYERDEC, ResStr(IDS_SUBRESYNC_DECREASE));
                    m.AppendMenu(MF_STRING | MF_ENABLED, LAYERINC, ResStr(IDS_SUBRESYNC_INCREASE));
                }
                break;
            case COL_ACTOR:
                if (m_mode == TEXTSUB) {
                    CMapStringToPtr actormap;

                    for (size_t i = 0, j = m_sts.GetCount(); i < j; i++) {
                        actormap[m_sts[i].actor] = nullptr;
                    }

                    actormap.RemoveKey(_T(""));

                    if (!actormap.IsEmpty()) {
                        m.AppendMenu(MF_SEPARATOR);

                        int id = ACTORFIRST;

                        POSITION pos = actormap.GetStartPosition();
                        while (pos && id <= ACTORLAST) {
                            CString key;
                            void* val;
                            actormap.GetNextAssoc(pos, key, val);

                            actors.Add(key);

                            m.AppendMenu(MF_STRING | MF_ENABLED, id++, key);
                        }
                    }
                }
                break;
            case COL_EFFECT:
                if (m_mode == TEXTSUB) {
                    CMapStringToPtr effectmap;

                    for (size_t i = 0, j = m_sts.GetCount(); i < j; i++) {
                        effectmap[m_sts[i].effect] = nullptr;
                    }

                    effectmap.RemoveKey(_T(""));

                    if (!effectmap.IsEmpty()) {
                        m.AppendMenu(MF_SEPARATOR);

                        int id = EFFECTFIRST;

                        POSITION pos = effectmap.GetStartPosition();
                        while (pos && id <= EFFECTLAST) {
                            CString key;
                            void* val;
                            effectmap.GetNextAssoc(pos, key, val);

                            effects.Add(key);

                            m.AppendMenu(MF_STRING | MF_ENABLED, id++, key);
                        }
                    }
                }
                break;
        }

        CPoint p = lpnmlv->ptAction;
        ::MapWindowPoints(pNMHDR->hwndFrom, HWND_DESKTOP, &p, 1);

        if (AppNeedsThemedControls()) {
            m.fulfillThemeReqs();
        }
        UINT id = m.TrackPopupMenu(TPM_LEFTBUTTON | TPM_RETURNCMD, p.x, p.y, m_pMainFrame);

        bool bNeedsUpdate = false;

        POSITION pos = m_list.GetFirstSelectedItemPosition();
        while (pos) {
            int iItem = m_list.GetNextSelectedItem(pos);

            SubTime& st = m_subtimes[iItem];

            switch (id) {
                case TOGSEP:
                    m_displayData[iItem].flags ^= TSEP;
                    m_list.Invalidate();
                    bNeedsUpdate = true;
                    break;
                case DUPITEM: {
                    CUIntArray items;
                    pos = m_list.GetFirstSelectedItemPosition();
                    while (pos) {
                        items.Add(m_list.GetNextSelectedItem(pos));
                    }

                    std::sort(items.GetData(), items.GetData() + items.GetCount(), std::greater<>());

                    for (INT_PTR i = 0, l = items.GetCount(); i < l; i++) {
                        iItem = items[i];

                        STSEntry entry = m_sts[iItem];
                        m_sts.InsertAt(iItem + 1, entry);
                        SubTime subtime = m_subtimes[iItem];
                        m_subtimes.insert(m_subtimes.begin() + iItem + 1, subtime);
                        m_subtimes[iItem + 1].itIndex = m_newStartsIndex.emplace(m_subtimes[iItem + 1].newStart, size_t(iItem + 1));
                        for (size_t j = size_t(iItem + 2); j < m_subtimes.size(); j++) {
                            m_subtimes[j].itIndex->second++;
                        }
                        DisplayData displayData = m_displayData[iItem];
                        m_displayData.insert(m_displayData.begin() + iItem + 1, displayData);
                    }

                    m_list.SetItemCount((int)m_sts.GetCount());

                    bNeedsUpdate = true;
                    break;
                }
                case DELITEM: {
                    CUIntArray items;
                    pos = m_list.GetFirstSelectedItemPosition();
                    while (pos) {
                        items.Add(m_list.GetNextSelectedItem(pos));
                    }

                    std::sort(items.GetData(), items.GetData() + items.GetCount(), std::greater<>());

                    for (INT_PTR i = 0, l = items.GetCount(); i < l; i++) {
                        iItem = items[i];
                        m_sts.RemoveAt(iItem);
                        m_newStartsIndex.erase(m_subtimes[iItem].itIndex);
                        m_subtimes.erase(m_subtimes.begin() + iItem);
                        for (size_t j = size_t(iItem); j < m_subtimes.size(); j++) {
                            m_subtimes[j].itIndex->second--;
                        }
                        m_displayData.erase(m_displayData.begin() + iItem);
                    }

                    m_list.SetItemCount((int)m_sts.GetCount());

                    iItem = items[items.GetCount() - 1];
                    if (iItem >= m_list.GetItemCount()) {
                        iItem = m_list.GetItemCount() - 1;
                    }
                    m_list.SetSelectionMark(iItem);

                    bNeedsUpdate = true;
                    break;
                }
                case RESETS:
                    ModStart(iItem, st.orgStart, true);
                    bNeedsUpdate = true;
                    break;
                case SETOS:
                    ModStart(iItem, st.orgStart);
                    bNeedsUpdate = true;
                    break;
                case SETCS:
                    ModStart(iItem, m_rt);
                    bNeedsUpdate = true;
                    break;
                case RESETE:
                    ModEnd(iItem, st.orgEnd, true);
                    bNeedsUpdate = true;
                    break;
                case SETOE:
                    ModEnd(iItem, st.orgEnd);
                    bNeedsUpdate = true;
                    break;
                case SETCE:
                    ModEnd(iItem, m_rt);
                    bNeedsUpdate = true;
                    break;
                default:
                    if (STYLEFIRST <= id && id <= STYLELAST) {
                        CString s = stylesNames[id - STYLEFIRST];
                        if (m_sts[iItem].style != s) {
                            m_sts[iItem].style = s;
                            bNeedsUpdate = true;
                        }
                    } else if (id == STYLEEDIT) {
                        CAutoPtrArray<CPPageSubStyle> pages;
                        CAtlArray<STSStyle*> styles;

                        STSStyle* stss = m_sts.GetStyle(iItem);
                        int iSelPage = 0;

                        POSITION posStyles = m_sts.m_styles.GetStartPosition();
                        for (int i = 0; posStyles; i++) {
                            CString key;
                            STSStyle* val;
                            m_sts.m_styles.GetNextAssoc(posStyles, key, val);

                            CAutoPtr<CPPageSubStyle> page(DEBUG_NEW CPPageSubStyle());
                            page->InitStyle(key, *val);
                            pages.Add(page);
                            styles.Add(val);

                            if (stss == val) {
                                iSelPage = i;
                            }
                        }

                        CMPCThemePropertySheet dlg(ResStr(IDS_SUBTITLES_STYLES_CAPTION), this, iSelPage);

                        for (size_t i = 0, l = pages.GetCount(); i < l; i++) {
                            dlg.AddPage(pages[i]);
                        }

                        if (dlg.DoModal() == IDOK) {
                            for (size_t j = 0, l = pages.GetCount(); j < l; j++) {
                                pages[j]->GetStyle(*styles[j]);
                            }
                            bNeedsUpdate = true;
                        }
                    } else if (id == UNICODEYES || id == UNICODENO) {
                        m_sts.ConvertUnicode(iItem, id == UNICODEYES);
                        bNeedsUpdate = true;
                    } else if (id == LAYERDEC || id == LAYERINC) {
                        int d = (id == LAYERDEC) ? -1 : 1;
                        m_sts[iItem].layer += d;
                        bNeedsUpdate = true;
                    } else if (ACTORFIRST <= id && id <= ACTORLAST) {
                        CString s = actors[id - ACTORFIRST];
                        if (m_sts[iItem].actor != s) {
                            m_sts[iItem].actor = s;
                            bNeedsUpdate = true;
                        }
                    } else if (EFFECTFIRST <= id && id <= EFFECTLAST) {
                        CString s = effects[id - EFFECTFIRST];
                        if (m_sts[iItem].effect != s) {
                            m_sts[iItem].effect = s;
                            bNeedsUpdate = true;
                        }
                    }
                    break;
            }

            if (bNeedsUpdate) {
                m_list.Update(iItem);
            }
        }

        if (bNeedsUpdate) {
            UpdatePreview();
        }
    }

    *pResult = 0;
}

void CPlayerSubresyncBar::OnNMDblclkList(NMHDR* pNMHDR, LRESULT* pResult)
{
    LPNMLISTVIEW lpnmlv = (LPNMLISTVIEW)pNMHDR;

    if (lpnmlv->iItem >= 0 && lpnmlv->iSubItem >= 0 && (m_mode == VOBSUB || m_mode == TEXTSUB)) {
        if (CMainFrame* pMainFrame = AfxGetMainFrame()) {
            REFERENCE_TIME rt;
            if (lpnmlv->iSubItem > COL_PREVEND || !ParseTime(m_list.GetItemText(lpnmlv->iItem, lpnmlv->iSubItem), rt, false)) {
                rt = m_sts[lpnmlv->iItem].start;
            }
            pMainFrame->SeekTo(rt);
        }
    }

    *pResult = 0;
}

void CPlayerSubresyncBar::OnLvnKeydownList(NMHDR* pNMHDR, LRESULT* pResult)
{
    LPNMLVKEYDOWN pLVKeyDown = reinterpret_cast<LPNMLVKEYDOWN>(pNMHDR);
    UNREFERENCED_PARAMETER(pLVKeyDown);

    *pResult = 0;
}

static CUIntArray m_itemGroups;
static int m_totalGroups;

void CPlayerSubresyncBar::DoCustomPrePaint()
{
    m_itemGroups.SetSize(m_list.GetItemCount());
    m_totalGroups = 0;
    for (int i = 0, j = m_list.GetItemCount(); i < j; i++) {
        if (m_displayData[i].flags & TSEP) {
            m_totalGroups++;
        }
        m_itemGroups[i] = m_totalGroups;
    }
}

void CPlayerSubresyncBar::GetCustomTextColors(INT_PTR nItem, int iSubItem, COLORREF& clrText, COLORREF& clrTextBk, bool& overrideSelectedBG)
{
    COLORREF fadeText, normalText, activeNormalText, activeFadeText;
    COLORREF bgNormalOdd, bgNormalEven, bgMod, bgAdjust;
    bool useFadeText;

    if (AppNeedsThemedControls()) {
        normalText = CMPCTheme::SubresyncFadeText1;
        fadeText = CMPCTheme::SubresyncFadeText2;
        activeNormalText = CMPCTheme::TextFGColor;
        activeFadeText = CMPCTheme::SubresyncActiveFadeText;
        bgNormalOdd = CMPCTheme::WindowBGColor;
        bgNormalEven = CMPCTheme::ContentBGColor;
        bgMod = CMPCTheme::SubresyncHLColor1;
        bgAdjust = CMPCTheme::SubresyncHLColor2;
    } else {
        normalText = 0;
        fadeText = 0x606060;
        activeNormalText = normalText & 0xFF;
        activeFadeText = fadeText & 0xFF;
        bgNormalOdd = 0xffffff - 0x100010;
        bgNormalEven = 0xffffff - 0x200020;
        bgMod = 0xffddbb;
        bgAdjust = 0xffeedd;
    }

    if ((iSubItem == COL_START || iSubItem == COL_END || iSubItem == COL_TEXT || iSubItem == COL_STYLE
            || iSubItem == COL_LAYER || iSubItem == COL_ACTOR || iSubItem == COL_EFFECT)
            && m_mode == TEXTSUB) {
        useFadeText = false;
    } else if ((iSubItem == COL_START)
               && m_mode == VOBSUB) {
        useFadeText = false;
    } else {
        useFadeText = true;
    }

    clrTextBk = (m_itemGroups[nItem] & 1) ? bgNormalOdd : bgNormalEven;

    if (m_sts[nItem].start <= m_rt && m_rt < m_sts[nItem].end) {
        clrText = useFadeText ? activeFadeText : activeNormalText;
    } else {
        clrText = useFadeText ? fadeText : normalText;
    }

    int nCheck = m_displayData[nItem].flags;

    if ((nCheck & TSMOD) && (iSubItem == COL_START || iSubItem == COL_PREVSTART)) {
        clrTextBk = bgMod;
    } else if ((nCheck & TSADJ) && (/*iSubItem == COL_START ||*/ iSubItem == COL_PREVSTART)) {
        clrTextBk = bgAdjust;
    }

    if ((nCheck & TEMOD) && (iSubItem == COL_END || iSubItem == COL_PREVEND)) {
        clrTextBk = bgMod;
    } else if ((nCheck & TEADJ) && (/*iSubItem == COL_END ||*/ iSubItem == COL_PREVEND)) {
        clrTextBk = bgAdjust;
    }
}

void CPlayerSubresyncBar::GetCustomGridColors(int nItem, COLORREF& horzGridColor, COLORREF& vertGridColor)
{
    bool bSeparator = nItem < m_list.GetItemCount() - 1 && (m_displayData[nItem + 1].flags & TSEP);
    if (AppNeedsThemedControls()) {
        horzGridColor = bSeparator ? CMPCTheme::SubresyncGridSepColor : CMPCTheme::ListCtrlGridColor;
        vertGridColor = CMPCTheme::ListCtrlGridColor;
    } else {
        horzGridColor = bSeparator ? 0x404040 : 0xe0e0e0;
        vertGridColor = 0xe0e0e0;
    }
}

void CPlayerSubresyncBar::OnCustomdrawList(NMHDR* pNMHDR, LRESULT* pResult)
{
    //this custom draw is used only in classic mode
    *pResult = CDRF_DODEFAULT;
    if (!AppNeedsThemedControls()) {
        NMLVCUSTOMDRAW* pLVCD = reinterpret_cast<NMLVCUSTOMDRAW*>(pNMHDR);

        if (CDDS_PREPAINT == pLVCD->nmcd.dwDrawStage) {
            DoCustomPrePaint();
            *pResult = CDRF_NOTIFYPOSTPAINT | CDRF_NOTIFYITEMDRAW;
        } else if (CDDS_ITEMPREPAINT == pLVCD->nmcd.dwDrawStage) {
            pLVCD->nmcd.uItemState &= ~CDIS_FOCUS;

            *pResult = CDRF_NOTIFYPOSTPAINT | CDRF_NOTIFYSUBITEMDRAW;
        } else if ((CDDS_ITEMPREPAINT | CDDS_SUBITEM) == pLVCD->nmcd.dwDrawStage) {
            bool ignore;
            GetCustomTextColors(pLVCD->nmcd.dwItemSpec, pLVCD->iSubItem, pLVCD->clrText, pLVCD->clrTextBk, ignore);
            *pResult = CDRF_NOTIFYPOSTPAINT;
        } else if ((CDDS_ITEMPOSTPAINT | CDDS_SUBITEM) == pLVCD->nmcd.dwDrawStage) {
            //      *pResult = CDRF_DODEFAULT;
        } else if (CDDS_ITEMPOSTPAINT == pLVCD->nmcd.dwDrawStage) {
            int nItem = static_cast<int>(pLVCD->nmcd.dwItemSpec);

            LVITEM rItem;
            ZeroMemory(&rItem, sizeof(LVITEM));
            rItem.mask = LVIF_IMAGE | LVIF_STATE;
            rItem.iItem = nItem;
            rItem.stateMask = LVIS_SELECTED;
            m_list.GetItem(&rItem);

            {
                CDC* pDC = CDC::FromHandle(pLVCD->nmcd.hdc);
                COLORREF horzGridColor, vertGridColor;
                GetCustomGridColors(nItem, horzGridColor, vertGridColor);

                CRect rcItem;
                m_list.GetItemRect(nItem, &rcItem, LVIR_BOUNDS);

                {
                    CPen p(PS_INSIDEFRAME, 1, horzGridColor);
                    CPen* old = pDC->SelectObject(&p);
                    pDC->MoveTo(CPoint(rcItem.left, rcItem.bottom - 1));
                    pDC->LineTo(CPoint(rcItem.right, rcItem.bottom - 1));
                    pDC->SelectObject(old);
                }

                {
                    CPen p(PS_INSIDEFRAME, 1, vertGridColor);
                    CPen* old = pDC->SelectObject(&p);

                    CHeaderCtrl* pHeader = (CHeaderCtrl*)m_list.GetDlgItem(0);
                    int nColumnCount = pHeader->GetItemCount();

                    // Get the column offset
                    int offset = rcItem.left;
                    for (int i = 0; i < nColumnCount; i++) {
                        offset += m_list.GetColumnWidth(i);
                        pDC->MoveTo(CPoint(offset, rcItem.top));
                        pDC->LineTo(CPoint(offset, rcItem.bottom));
                    }

                    pDC->SelectObject(old);
                }

                *pResult = CDRF_SKIPDEFAULT;
            }
        } else if (CDDS_POSTPAINT == pLVCD->nmcd.dwDrawStage) {
        }
    }
}

bool CPlayerSubresyncBar::HandleShortCuts(const MSG* pMsg)
{
    bool bHandled = false;

    if (pMsg->message == WM_KEYDOWN && VK_F1 <= pMsg->wParam && pMsg->wParam <= VK_F6) {
        int iItem = -1;
        bool bStep = false;

        POSITION pos = m_list.GetFirstSelectedItemPosition();
        while (pos) {
            iItem = m_list.GetNextSelectedItem(pos);

            const SubTime& st = m_subtimes[iItem];

            switch (pMsg->wParam) {
                case VK_F1:
                    ModStart(iItem, st.orgStart, true);
                    bHandled = true;
                    break;
                case VK_F3:
                    ModStart(iItem, st.orgStart);
                    bHandled = true;
                    break;
                case VK_F5:
                    ModStart(iItem, m_rt);
                    bHandled = true;
                    break;
                case VK_F2:
                    ModEnd(iItem, st.orgEnd, true);
                    bHandled = true;
                    break;
                case VK_F4:
                    ModEnd(iItem, st.orgEnd);
                    bHandled = true;
                    break;
                case VK_F6:
                    ModEnd(iItem, m_rt);
                    bHandled = bStep = true;
                    break;
            }

            if (bHandled) {
                m_list.Update(iItem);
            }
        }

        if (bHandled) {
            if (bStep && m_list.GetSelectedCount() == 1 && iItem < m_list.GetItemCount() - 1) {
                m_list.SetItemState(iItem, 0, LVIS_SELECTED);
                m_list.SetItemState(iItem + 1, LVIS_SELECTED, LVIS_SELECTED);
                m_list.SetSelectionMark(iItem + 1);
                m_list.EnsureVisible(iItem + 1, false);
            }

            UpdatePreview();
        }
    }

    return bHandled;
}

void CPlayerSubresyncBar::OnMeasureItem(int nIDCtl, LPMEASUREITEMSTRUCT lpMeasureItemStruct)
{
	__super::OnMeasureItem(nIDCtl, lpMeasureItemStruct);

	if (createdWindow) {
		//after creation, measureitem is called once for every window resize.  we will cache the default before DPI scaling
		if (m_itemHeight == 0) {
			m_itemHeight = lpMeasureItemStruct->itemHeight;
		}
		lpMeasureItemStruct->itemHeight = m_pMainFrame->m_dpi.ScaleSystemToOverrideY(m_itemHeight);
	} else {
		//before creation, we must return a valid DPI scaled value, to prevent visual glitches when icon height has been tweaked.
		//we cannot cache this value as it may be different from that calculated after font has been set
		lpMeasureItemStruct->itemHeight = m_pMainFrame->m_dpi.ScaleSystemToOverrideY(lpMeasureItemStruct->itemHeight);
	}
}

int CPlayerSubresyncBar::FindNearestSub(REFERENCE_TIME& rtPos, bool bForward)
{
    if (m_subtimes.empty()) {
        return -2;
    }

    REFERENCE_TIME lCurTime = rtPos + (bForward ? 1 : -1);

    if (lCurTime < m_newStartsIndex.begin()->first) {
        size_t i = m_newStartsIndex.begin()->second;
        rtPos = m_subtimes[i].newStart;
        return bForward ? int(i) : -1;
    } else if (lCurTime > m_newStartsIndex.rbegin()->first) {
        size_t i = m_newStartsIndex.rbegin()->second;
        rtPos = m_subtimes[i].newStart;
        return bForward ? -1 : int(i);
    }

    for (auto it = m_newStartsIndex.begin(), itPrec = it++; it != m_newStartsIndex.end(); it++, itPrec++) {
        if (lCurTime >= itPrec->first && lCurTime < it->first) {
            rtPos = bForward ? it->first : itPrec->first;
            return int(bForward ? it->second : itPrec->second);
        }
    }

    return -1;
}

bool CPlayerSubresyncBar::ShiftSubtitle(int nItem, long lValue, REFERENCE_TIME& rtPos)
{
    bool bRet = false;

    if ((nItem == 0) || (m_subtimes[nItem - 1].newEnd < m_subtimes[nItem].newStart + lValue)) {
        for (size_t i = nItem; i < m_sts.GetCount(); i++) {
            m_subtimes[i].newStart += lValue;
            m_subtimes[i].newEnd   += lValue;
            m_subtimes[i].orgStart += lValue;
            m_subtimes[i].orgEnd   += lValue;
        }
        UpdatePreview();
        SaveSubtitle();
        bRet = true;
        rtPos = m_subtimes[nItem].newStart;
    }
    return bRet;
}

bool CPlayerSubresyncBar::SaveToDisk()
{
    return m_sts.SaveAs(m_sts.m_path, m_sts.m_subtitleType);
}
