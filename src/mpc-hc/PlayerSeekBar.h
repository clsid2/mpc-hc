/*
 * (C) 2003-2006 Gabest
 * (C) 2006-2018 see Authors.txt
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

#include "EventDispatcher.h"
#include "DSMPropertyBag.h"
#include <memory>
#include "CMPCThemeToolTipCtrl.h"

class CMainFrame;

class CPlayerSeekBar : public CDialogBar
{
    DECLARE_DYNAMIC(CPlayerSeekBar)

public:
    CPlayerSeekBar(CMainFrame* pMainFrame);
    virtual ~CPlayerSeekBar();
    virtual BOOL Create(CWnd* pParentWnd);

private:
    enum { TIMER_SHOWHIDE_TOOLTIP = 1 };

    CMainFrame* m_pMainFrame;
    REFERENCE_TIME m_rtStart, m_rtStop, m_rtPos, m_rtPosDraw;
	REFERENCE_TIME m_pos_preview = 0;
    LONG m_last_pointx_preview = 0;

    bool m_bEnabled;
    bool m_bHasDuration;
    REFERENCE_TIME m_rtHoverPos;
    CPoint m_hoverPoint;
    HCURSOR m_cursor;
    bool m_bDraggingThumb, m_bHoverThumb;
    ULONGLONG m_lastDragSeekTickCount;
    bool pauseAfterFirstScroll = false;
    bool pausedForScrolling = false;

    EventClient m_eventc;
    void EventCallback(MpcEvent ev);

    CMPCThemeToolTipCtrl m_tooltip;
    enum { TOOLTIP_HIDDEN, TOOLTIP_TRIGGERED, TOOLTIP_VISIBLE } m_tooltipState;
    CFont mpcThemeFont;
    TOOLINFO m_ti;
    CPoint m_tooltipPoint;
    bool m_bIgnoreLastTooltipPoint;
    CString m_tooltipText;

    CComPtr<IDSMChapterBag> m_pChapterBag;
    CCritSec m_csChapterBag; // Graph thread sets the chapter bag

    std::unique_ptr<CDC> m_pEnabledThumb;
    std::unique_ptr<CDC> m_pDisabledThumb;
    CRect m_lastThumbRect;

    CString m_timeText;                 // currently displayed time string (empty when none)
    mutable CFont m_timeFont;           // font for the time text
    mutable int   m_timeFontHeight = 0; // char height the cached font was built for (px)
    mutable int   m_timeReservedWidth = 0; // modeled (worst-case-for-options) time-section width, the stable floor (px)
    mutable int   m_timeActualWidth = 0;   // width of the actual current time string; widens the section if it ever exceeds the model (px)
    mutable CString m_timeTemplate;     // widest string the reserved width was measured for (tracks the active options)

    bool ShowTimeOnSeekBar() const;
    bool TimeSectionOnLeft() const;     // true => the time section sits left of the channel (advanced option)
    bool TimeSectionVisible() const;    // ShowTimeOnSeekBar() and there is a timer string to display
    int  TimeSectionEffectiveWidth() const; // px the section currently occupies (0 when not shown)
    void EnsureTimeFont() const;
    CString BuildTimeTemplate() const;  // widest time string for the currently enabled time options
    CRect GetTimeRect() const;          // the dedicated time section to the right of the channel

    virtual BOOL PreCreateWindow(CREATESTRUCT& cs);
    virtual CSize CalcFixedLayout(BOOL bStretch, BOOL bHorz) override;

    void MoveThumb(const CPoint& point);
    void SyncVideoToThumb();
    void checkHover(CPoint point);
    void invalidateThumb();
    void CheckScrollDistance(CPoint point, REFERENCE_TIME minimum_duration_change, ULONGLONG minimum_elapsed_tickcount);
    long ChannelPointFromPosition(REFERENCE_TIME rtPos) const;
    REFERENCE_TIME PositionFromClientPoint(const CPoint& point) const;
    void SyncThumbToVideo(REFERENCE_TIME rtPos, REFERENCE_TIME rtPosDraw);

    void CreateThumb(bool bEnabled, CDC& parentDC);
    CRect GetBaseRect() const;          // full usable rect (channel + time section), before reserving the time area
    CRect GetChannelRect() const;
    CRect GetSeekableRect() const;      // client area that responds to seek/hover (excludes the time section)
    CRect GetThumbRect() const;
    CRect GetInnerThumbRect(bool bEnabled, const CRect& thumbRect) const;

    void UpdateTooltip(const CPoint& point);
    void UpdateToolTipText();
    void GenerateToolTipText(REFERENCE_TIME rtPos);

public:
    void Enable(bool bEnable);
    void HideToolTip();
    void UpdateToolTipPosition(CPoint point);

    void GetRange(REFERENCE_TIME& rtStart, REFERENCE_TIME& rtStop) const;
    void SetRange(REFERENCE_TIME rtStart, REFERENCE_TIME rtStop);
    REFERENCE_TIME GetPos() const;
    void SetPos(REFERENCE_TIME rtPos);
    bool HasDuration() const;
    REFERENCE_TIME GetDuration();

    void SetChapterBag(IDSMChapterBag* pCB);
    void RemoveChapters();

    bool DraggingThumb();
    void PreviewWindowShow(const CPoint point);
    void UpdateToolTipTextPreview(REFERENCE_TIME rtPos);

    void UpdateTime();

private:
    DECLARE_MESSAGE_MAP()

    afx_msg void OnPaint();
    afx_msg void OnLButtonDown(UINT nFlags, CPoint point);
    afx_msg void OnLButtonDblClk(UINT nFlags, CPoint point);
    afx_msg void OnLButtonUp(UINT nFlags, CPoint point);
    afx_msg void OnXButtonDown(UINT nFlags, UINT nButton, CPoint point);
    afx_msg void OnXButtonUp(UINT nFlags, UINT nButton, CPoint point);
    afx_msg void OnXButtonDblClk(UINT nFlags, UINT nButton, CPoint point);
    afx_msg void OnMButtonDown(UINT nFlags, CPoint point);
    afx_msg void OnMouseMove(UINT nFlags, CPoint point);
    afx_msg void OnContextMenu(CWnd* pWnd, CPoint point);
    afx_msg BOOL OnEraseBkgnd(CDC* pDC);
    afx_msg BOOL OnSetCursor(CWnd* pWnd, UINT nHitTest, UINT message);
    afx_msg void OnTimer(UINT_PTR nIDEvent);
    afx_msg void OnMouseLeave();
    afx_msg LRESULT OnThemeChanged();
    afx_msg void OnCaptureChanged(CWnd* pWnd);
};
