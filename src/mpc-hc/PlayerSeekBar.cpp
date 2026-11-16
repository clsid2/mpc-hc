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

#include "stdafx.h"
#include "PlayerSeekBar.h"
#include "MainFrm.h"
#include "mplayerc.h"
#include "CMPCTheme.h"
#include "WinAPIUtils.h"


#define TOOLTIP_SHOW_DELAY 100
#define TOOLTIP_HIDE_TIMEOUT 3000
#define HOVER_CAPTURED_TIMEOUT 100
#define ADD_TO_BOTTOM_WITHOUT_CONTROLBAR 2
#define SEEK_DRAGGER_OVERLAP 5
#define TIME_SECTION_GAP 8          // gap (dialog px) between the channel and the time section
#define TIME_SECTION_PADDING 3      // right padding (dialog px) inside the time section

IMPLEMENT_DYNAMIC(CPlayerSeekBar, CDialogBar)

CPlayerSeekBar::CPlayerSeekBar(CMainFrame* pMainFrame)
    : m_pMainFrame(pMainFrame)
    , m_rtStart(0)
    , m_rtStop(0)
    , m_rtPos(0)
    , m_rtPosDraw(0)
    , m_bEnabled(false)
    , m_bHasDuration(false)
    , m_rtHoverPos(0)
    , m_cursor(AfxGetApp()->LoadStandardCursor(IDC_HAND))
    , m_bDraggingThumb(false)
    , m_bHoverThumb(false)
    , m_tooltipState(TOOLTIP_HIDDEN)
    , m_bIgnoreLastTooltipPoint(true)
    , m_lastDragSeekTickCount(0)
{
    ZeroMemory(&m_ti, sizeof(m_ti));
    m_ti.cbSize = sizeof(m_ti);

    GetEventd().Connect(m_eventc, {
        MpcEvent::DPI_CHANGED,
        MpcEvent::STREAM_POS_UPDATE_REQUEST,
    }, std::bind(&CPlayerSeekBar::EventCallback, this, std::placeholders::_1));
}

CPlayerSeekBar::~CPlayerSeekBar()
{
}

BOOL CPlayerSeekBar::Create(CWnd* pParentWnd)
{
    if (!__super::Create(pParentWnd,
                         IDD_PLAYERSEEKBAR, WS_CHILD | WS_VISIBLE | CBRS_ALIGN_BOTTOM, IDD_PLAYERSEEKBAR)) {
        return FALSE;
    }

    if (!AppIsThemeLoaded()) {
        CDC* pDC = GetWindowDC();
        if (CMPCThemeUtil::getFontByType(mpcThemeFont, GetParent(), CMPCThemeUtil::MessageFont)) {
            SetFont(&mpcThemeFont);
        }
        ReleaseDC(pDC);
    }

    // Should never be RTLed
    ModifyStyleEx(WS_EX_LAYOUTRTL, WS_EX_NOINHERITLAYOUT);


    m_tooltip.Create(this, TTS_NOPREFIX | TTS_ALWAYSTIP);
    m_tooltip.SetMaxTipWidth(-1);

    m_ti.uFlags = TTF_IDISHWND | TTF_TRACK | TTF_ABSOLUTE;
    m_ti.hwnd = m_hWnd;
    m_ti.uId = (UINT_PTR)m_hWnd;
    m_ti.hinst = AfxGetInstanceHandle();
    m_ti.lpszText = nullptr;

    m_tooltip.SendMessage(TTM_ADDTOOL, 0, (LPARAM)&m_ti);

    return TRUE;
}

void CPlayerSeekBar::EventCallback(MpcEvent ev)
{
    switch (ev) {
        case MpcEvent::DPI_CHANGED:
            m_pEnabledThumb = nullptr;
            m_pDisabledThumb = nullptr;
            m_timeFont.DeleteObject(); // rebuilt on next use
            m_timeText.Empty();
            break;

        case MpcEvent::STREAM_POS_UPDATE_REQUEST:
            // The time options (remaining / high precision / percentage) just changed, which can
            // resize the time section; fully repaint so the channel is redrawn at its new width.
            Invalidate();
            break;

        default:
            ASSERT(FALSE);
    }
}

BOOL CPlayerSeekBar::PreCreateWindow(CREATESTRUCT& cs)
{
    if (!__super::PreCreateWindow(cs)) {
        return FALSE;
    }

    m_dwStyle &= ~CBRS_BORDER_TOP;
    m_dwStyle &= ~CBRS_BORDER_BOTTOM;
    m_dwStyle |= CBRS_SIZE_FIXED;

    return TRUE;
}

CSize CPlayerSeekBar::CalcFixedLayout(BOOL bStretch, BOOL bHorz)
{
    CSize ret = __super::CalcFixedLayout(bStretch, bHorz);
    const CAppSettings& s = AfxGetAppSettings();
    if (AppIsThemeLoaded()) {
        ret.cy = m_pMainFrame->m_dpi.ScaleY(5 + s.iModernSeekbarHeight); //expand the toolbar if using "fill" mode
    } else {
        ret.cy = m_pMainFrame->m_dpi.ScaleY(20);
    }
    if (!m_pMainFrame->m_controls.ControlChecked(CMainFrameControls::Toolbar::CONTROLS)) {
        ret.cy += ADD_TO_BOTTOM_WITHOUT_CONTROLBAR;
    }
    return ret;
}

void CPlayerSeekBar::MoveThumb(const CPoint& point)
{
    if (m_bHasDuration) {
        REFERENCE_TIME rtPosDraw = PositionFromClientPoint(point);
        REFERENCE_TIME rtPos = rtPosDraw;
        const CAppSettings& s = AfxGetAppSettings();
        REFERENCE_TIME duration = m_rtStop - m_rtStart;
        if (duration >= 600000000LL && s.bFastSeek && (GetKeyState(VK_SHIFT) >= 0)) {
            REFERENCE_TIME rtMaxDiff = s.bAllowInaccurateFastseek ? 200000000LL : std::min(100000000LL, duration / 30);
            rtPos = m_pMainFrame->GetClosestKeyFrame(rtPos, rtMaxDiff, rtMaxDiff);
        }
        const CTrimController& trim = m_pMainFrame->m_trim;
        SyncThumbToVideo(trim.ConfineSeek(rtPos), trim.ConfineSeek(rtPosDraw));
    }
}

void CPlayerSeekBar::SyncVideoToThumb()
{
    GetParent()->PostMessage(WM_HSCROLL, (WPARAM)0, reinterpret_cast<LPARAM>(m_hWnd));
}

void CPlayerSeekBar::CheckScrollDistance(CPoint point, REFERENCE_TIME minimum_duration_change, ULONGLONG minimum_elapsed_tickcount)
{
    ULONGLONG tickcount = GetTickCount64();
    ULONGLONG ticks_since_last_seek = tickcount - m_lastDragSeekTickCount;
    REFERENCE_TIME posdiff = m_rtHoverPos > m_rtPosDraw ? m_rtHoverPos - m_rtPosDraw : m_rtPosDraw - m_rtHoverPos;

    if (posdiff >= minimum_duration_change && ticks_since_last_seek >= minimum_elapsed_tickcount) {
        m_lastDragSeekTickCount = tickcount;
        m_rtHoverPos = m_rtPosDraw;
        m_hoverPoint = point;

        if (pauseAfterFirstScroll && minimum_elapsed_tickcount > 0LL) {
            pauseAfterFirstScroll = false;
            pausedForScrolling = true;
            m_pMainFrame->MediaControlPause();
        }

        SyncVideoToThumb();
    }
}

long CPlayerSeekBar::ChannelPointFromPosition(REFERENCE_TIME rtPos) const
{
    rtPos = std::min(m_rtStop, std::max(m_rtStart, rtPos));
    long ret = 0;
    auto w = GetChannelRect().Width();
    if (m_bHasDuration) {
        ret = (long)(w * (rtPos - m_rtStart) / (m_rtStop - m_rtStart));
    }
    if (ret >= w) {
        ret = w - 1;
    }
    return ret;
}

REFERENCE_TIME CPlayerSeekBar::PositionFromClientPoint(const CPoint& point) const
{
    REFERENCE_TIME rtRet = -1;
    if (m_bHasDuration) {
        ASSERT(m_rtStart < m_rtStop);
        const CRect channelRect(GetChannelRect());
        auto channelPointX = (point.x < channelRect.left) ? channelRect.left :
                             (point.x > channelRect.right) ? channelRect.right : point.x;
        ASSERT(channelPointX >= channelRect.left && channelPointX <= channelRect.right);
        rtRet = m_rtStart + (channelPointX - channelRect.left) * (m_rtStop - m_rtStart) / channelRect.Width();
    }
    return rtRet;
}

void CPlayerSeekBar::SyncThumbToVideo(REFERENCE_TIME rtPos, REFERENCE_TIME rtPosDraw, bool bUpdateTaskbar)
{
    m_rtPos = rtPos;
    m_rtPosDraw = rtPosDraw;
    if (m_bHasDuration) {
        CRect newThumbRect(GetThumbRect());
        bool bSetTaskbar = (rtPos <= 0);
        if (newThumbRect != m_lastThumbRect) {
            bSetTaskbar = true;
            InvalidateRect(newThumbRect | m_lastThumbRect);
        }
        if (bUpdateTaskbar && bSetTaskbar && AfxGetAppSettings().bUseEnhancedTaskBar && m_pMainFrame->m_api.m_pTaskbarList) {
            VERIFY(S_OK == m_pMainFrame->m_api.m_pTaskbarList->SetProgressValue(m_pMainFrame->m_hWnd, std::max(m_rtPos, 1ll), m_rtStop));
        }
    }
}

void CPlayerSeekBar::CreateThumb(bool bEnabled, CDC& parentDC)
{
    auto& pThumb = bEnabled ? m_pEnabledThumb : m_pDisabledThumb;
    pThumb = std::unique_ptr<CDC>(new CDC());

    if (pThumb->CreateCompatibleDC(&parentDC)) {
        COLORREF
        white  = GetSysColor(COLOR_WINDOW),
        shadow = GetSysColor(COLOR_3DSHADOW),
        light  = GetSysColor(COLOR_3DHILIGHT),
        bkg    = GetSysColor(COLOR_BTNFACE);

        CRect r(GetThumbRect());
        r.MoveToXY(0, 0);
        CRect ri(GetInnerThumbRect(bEnabled, r));

        CBitmap bmp;
        VERIFY(bmp.CreateCompatibleBitmap(&parentDC, r.Width(), r.Height()));
        VERIFY(pThumb->SelectObject(bmp));

        if (AppIsThemeLoaded()) {
            //just a rectangle, we will draw from scratch
        } else {
            pThumb->Draw3dRect(&r, light, 0);
            r.DeflateRect(0, 0, 1, 1);
            pThumb->Draw3dRect(&r, light, shadow);
            r.DeflateRect(1, 1, 1, 1);

            if (bEnabled) {
                pThumb->ExcludeClipRect(ri);
                ri.InflateRect(0, 1, 0, 1);
                pThumb->FillSolidRect(ri, white);
                pThumb->SetPixel(ri.CenterPoint().x, ri.top, 0);
                pThumb->SetPixel(ri.CenterPoint().x, ri.bottom - 1, 0);
            }
            pThumb->ExcludeClipRect(ri);

            ri.InflateRect(1, 1, 1, 1);
            pThumb->Draw3dRect(&ri, shadow, bkg);
            pThumb->ExcludeClipRect(ri);

            CBrush b(bkg);
            pThumb->FillRect(&r, &b);
        }
    } else {
        ASSERT(FALSE);
    }
}

CRect CPlayerSeekBar::GetBaseRect() const
{
    CRect r;
    GetClientRect(&r);
    r.top += 1;
    if (m_pMainFrame->m_controls.ControlChecked(CMainFrameControls::Toolbar::CONTROLS)) {
        r.bottom += ADD_TO_BOTTOM_WITHOUT_CONTROLBAR;
    }

    if (AppIsThemeLoaded()) { //no thumb so we can use all the space
        r.DeflateRect(m_pMainFrame->m_dpi.ScaleFloorX(2), m_pMainFrame->m_dpi.ScaleFloorX(2));
    } else {
        CSize sz(m_pMainFrame->m_dpi.ScaleFloorX(8), m_pMainFrame->m_dpi.ScaleFloorY(7) + 1);
        r.DeflateRect(sz.cx, sz.cy, sz.cx, sz.cy);
    }

    return r;
}

CRect CPlayerSeekBar::GetChannelRect() const
{
    CRect r(GetBaseRect());
    // Reserve a dedicated section on the left or right for the time (modern theme only); the channel
    // takes the remaining width so the time is never drawn over it (#3256).
    if (TimeSectionVisible()) {
        EnsureTimeFont();
        const int w = std::max(m_timeReservedWidth, m_timeActualWidth) + m_pMainFrame->m_dpi.ScaleX(TIME_SECTION_GAP);
        if (TimeSectionOnLeft()) {
            r.left += w;
        } else {
            r.right -= w;
        }
    }
    if (TrimActive()) {
        // Keep the grips at both ends inside the bar.
        CRect clientRect;
        GetClientRect(clientRect);
        const int grip = TrimGripWidth() + 1;
        r.left = std::max<LONG>(r.left, clientRect.left + grip);
        r.right = std::min<LONG>(r.right, clientRect.right - grip);
    }
    return r;
}

CRect CPlayerSeekBar::GetSeekableRect() const
{
    // The interactive (seek/hover/preview) area spans the bar but stops at the channel edge nearest
    // the time section, so the dedicated time section does not behave like the seekbar (#3256).
    CRect r;
    GetClientRect(&r);
    if (TimeSectionVisible()) {
        if (TimeSectionOnLeft()) {
            r.left = GetChannelRect().left;
        } else {
            r.right = GetChannelRect().right;
        }
    }
    return r;
}

CRect CPlayerSeekBar::GetTimeRect() const
{
    CRect r(GetBaseRect());
    if (TimeSectionVisible()) {
        EnsureTimeFont();
        const int w = std::max(m_timeReservedWidth, m_timeActualWidth);
        if (TimeSectionOnLeft()) {
            r.right = r.left + w;
        } else {
            r.left = r.right - w;
        }
    } else {
        // Empty section, collapsed to the edge it would occupy.
        if (TimeSectionOnLeft()) {
            r.right = r.left;
        } else {
            r.left = r.right;
        }
    }
    return r;
}

bool CPlayerSeekBar::ShowTimeOnSeekBar() const
{
    if (!AppIsThemeLoaded()) {
        return false; // time on the seekbar is only supported with the modern theme
    }
    const CAppSettings& s = AfxGetAppSettings();
    switch (s.nTimeOnSeekBar) {
        case TIME_ON_SEEKBAR_ALWAYS:
            return true;
        case TIME_ON_SEEKBAR_WHEN_STATUSBAR_HIDDEN:
            return !m_pMainFrame->m_controls.ControlChecked(CMainFrameControls::Toolbar::STATUS);
        default:
            return false;
    }
}

bool CPlayerSeekBar::TimeSectionOnLeft() const
{
    return AfxGetAppSettings().bTimeOnSeekBarLeft;
}

bool CPlayerSeekBar::TimeSectionVisible() const
{
    // Show the time section whenever there is a status-timer string to display. That covers media
    // with a known duration (position / duration) as well as durationless media - live streams and
    // capture still report a running position or "Live" - while leaving the channel full-width when
    // nothing is loaded. The reserved width adapts to the shorter format via BuildTimeTemplate.
    return ShowTimeOnSeekBar() && !m_timeText.IsEmpty();
}

int CPlayerSeekBar::TimeSectionEffectiveWidth() const
{
    return TimeSectionVisible() ? std::max(m_timeReservedWidth, m_timeActualWidth) : 0;
}

CString CPlayerSeekBar::BuildTimeTemplate() const
{
    // Build the widest string the time section can currently produce, matching the format options
    // used by CPlayerStatusBar::SetStatusTimer, so the reserved width fits exactly what's enabled.
    const CAppSettings& s = AfxGetAppSettings();
    const bool highPrec = s.bHighPrecisionTimer || m_pMainFrame->IsSubresyncBarVisible();

    // Size to the largest time this media will show. With a known duration that's the duration (the
    // position and remaining time never exceed it); without one (live stream/capture) the status bar
    // shows only the running position, so size to that instead. Minutes/seconds are always 2 digits,
    // so only the hour digit-count varies (the status timer uses %02u, a minimum of 2 hour digits).
    const LONGLONG oneHour = 3600LL * 10000000LL;
    const LONGLONG rtWidest = m_bHasDuration ? m_rtStop : m_rtPos;
    CString time;
    if (rtWidest >= oneHour) {
        int hourDigits = 0;
        for (LONGLONG h = rtWidest / oneHour; h > 0; h /= 10) {
            hourDigits++;
        }
        if (hourDigits < 2) {
            hourDigits = 2;
        }
        time = CString(_T('0'), hourDigits) + _T(":00:00");
    } else {
        time = _T("00:00");
    }
    // '0' placeholders are reliably full-width, so the measured width never under-reserves.
    if (highPrec) {
        time += _T(".000");
    }

    if (!m_bHasDuration) {
        // Durationless media: the status bar shows a single running value, with no "/ duration",
        // remaining-time prefix or percentage, so reserve for just that. Non-numeric strings such
        // as "Live" are handled by UpdateTime's actual-width safety net.
        return time;
    }

    CString t = s.fRemainingTime ? (_T("- ") + time + _T(" / ") + time) : (time + _T(" / ") + time);
    if (s.bTimerShowPercentage) {
        t += _T(" (100.0%)");
    }
    return t;
}

void CPlayerSeekBar::EnsureTimeFont() const
{
    LOGFONT lf;
    GetStatusFont(&lf);
    LONG baseHeight = m_pMainFrame->m_dpi.ScaleSystemToOverrideY(lf.lfHeight);
    LONG baseMag = baseHeight < 0 ? -baseHeight : baseHeight;

    // Scale the time font to the seekbar height (~2/3 of the channel height), but never below
    // the normal status-bar font, so a short seekbar keeps the default size while a tall one
    // (modern "fill" mode) gets proportionally larger text. Use the base rect height (not
    // GetChannelRect, which reserves width via this method) to avoid recursion.
    LONG channelH = GetBaseRect().Height();
    LONG mag = (channelH > 0) ? std::max(baseMag, (LONG)MulDiv(channelH, 2, 3)) : baseMag;

    const bool fontChanged = (!(HFONT)m_timeFont || m_timeFontHeight != mag);
    if (fontChanged) {
        m_timeFont.DeleteObject();
        lf.lfHeight = -mag; // negative => character height
        VERIFY(m_timeFont.CreateFontIndirect(&lf));
        m_timeFontHeight = mag;
    }

    // Recompute the reserved section width when the font or the set of enabled time options changes.
    const CString tmpl = BuildTimeTemplate();
    if (fontChanged || tmpl != m_timeTemplate) {
        m_timeTemplate = tmpl;
        CClientDC dc(const_cast<CPlayerSeekBar*>(this));
        CFont* pOldFont = dc.SelectObject(&m_timeFont);
        m_timeReservedWidth = dc.GetTextExtent(tmpl).cx + m_pMainFrame->m_dpi.ScaleX(TIME_SECTION_PADDING);
        dc.SelectObject(pOldFont);
    }
}

void CPlayerSeekBar::UpdateTime()
{
    // Mirror the status-bar timer text: "position / duration" for media with a duration, or a single
    // running position / "Live" for durationless media (live streams, capture). The section is shown
    // whenever that text is non-empty (TimeSectionVisible), so no space is reserved when idle.
    const int oldEffWidth = TimeSectionEffectiveWidth();

    CString newText;
    if (ShowTimeOnSeekBar()) {
        newText = m_pMainFrame->m_wndStatusBar.GetStatusTimer();
    }
    const bool textChanged = (newText != m_timeText);

    int newActualWidth = 0;
    if (!newText.IsEmpty()) {
        EnsureTimeFont(); // refresh the modeled reserved width for the current options/duration
        // Safety net: if the actual string is wider than the model (e.g. the frame-count format, a
        // "Live" label, or the position overrunning a short/inaccurate duration), widen so it never
        // clips. The model stays the stable floor, so the common case doesn't jitter.
        CClientDC dc(this);
        CFont* pOldFont = dc.SelectObject(&m_timeFont);
        newActualWidth = dc.GetTextExtent(newText).cx + m_pMainFrame->m_dpi.ScaleX(TIME_SECTION_PADDING);
        dc.SelectObject(pOldFont);
    }

    m_timeText = newText;
    m_timeActualWidth = newActualWidth;

    const int newEffWidth = TimeSectionEffectiveWidth();
    if (newEffWidth != oldEffWidth) {
        Invalidate(); // channel geometry changed (section appeared/disappeared or resized)
    } else if (textChanged) {
        InvalidateRect(GetTimeRect()); // same footprint, only the text changed
    }
}

CRect CPlayerSeekBar::GetThumbRect() const
{
    const CRect channelRect(GetChannelRect());
    const long x = channelRect.left + ChannelPointFromPosition(m_rtPosDraw);
    CSize s;
    s.cy = m_pMainFrame->m_dpi.ScaleFloorY(SEEK_DRAGGER_OVERLAP);
    s.cx = m_pMainFrame->m_dpi.TransposeScaledY(channelRect.Height()) / 2 + s.cy;
    CRect r(x + 1 - s.cx, channelRect.top - s.cy, x + s.cx, channelRect.bottom + s.cy);
    return r;
}

CRect CPlayerSeekBar::GetInnerThumbRect(bool bEnabled, const CRect& thumbRect) const
{
    CSize s(m_pMainFrame->m_dpi.ScaleFloorX(4) - 1, m_pMainFrame->m_dpi.ScaleFloorY(5));
    if (!bEnabled) {
        s.cy -= 1;
    }
    CRect r(thumbRect);
    r.DeflateRect(s.cx, s.cy, s.cx, s.cy);
    return r;
}

bool CPlayerSeekBar::TrimActive() const
{
    return m_bHasDuration && m_pMainFrame->m_trim.IsActive();
}

int CPlayerSeekBar::TrimGripWidth() const
{
    return std::max(2, m_pMainFrame->m_dpi.ScaleX(3));
}

int CPlayerSeekBar::TrimEdge(CTrimSelection::Part part) const
{
    const CRect channelRect(GetChannelRect());
    const REFERENCE_TIME time = m_pMainFrame->m_trim.Selection().Boundary(part);
    return CTrimSelection::TimeToPixel(time - m_rtStart, m_rtStop - m_rtStart, channelRect.left, channelRect.Width());
}

// A bracket whose upright stands outside the selection and whose feet point into it, so
// that the two grips read as "[" and "]" and never cover each other's upright.
CRect CPlayerSeekBar::GetTrimGripRect(CTrimSelection::Part part) const
{
    CRect clientRect;
    GetClientRect(clientRect);
    const CRect channelRect(GetChannelRect());
    const int overhang = m_pMainFrame->m_dpi.ScaleY(AppIsThemeLoaded() ? 3 : SEEK_DRAGGER_OVERLAP);
    const int top = std::max<int>(clientRect.top + 1, channelRect.top - overhang);
    const int bottom = std::min<int>(clientRect.bottom - 1, channelRect.bottom + overhang);
    const int x = TrimEdge(part);
    const int w = TrimGripWidth();
    const int feet = m_pMainFrame->m_dpi.ScaleX(4);
    return part == CTrimSelection::Part::Start ? CRect(x - w, top, x + feet, bottom) : CRect(x - feet, top, x + w, bottom);
}

CTrimSelection::Part CPlayerSeekBar::HitTestTrim(const CPoint& point) const
{
    if (!TrimActive() || !GetSeekableRect().PtInRect(point)) {
        return CTrimSelection::Part::None;
    }
    const CRect channelRect(GetChannelRect());
    // The hit zone is wider than the visible grip.
    const int radius = m_pMainFrame->m_dpi.ScaleX(6);
    return CTrimSelection::HitTest(point.x, TrimEdge(CTrimSelection::Part::Start), TrimEdge(CTrimSelection::Part::End), radius,
                                   channelRect.left, channelRect.right, m_pMainFrame->m_trim.Selection().Focus());
}

void CPlayerSeekBar::MoveTrimGrip(const CPoint& point)
{
    const CRect channelRect(GetChannelRect());
    const REFERENCE_TIME time = m_rtStart + CTrimSelection::PixelToTime(point.x - m_trimGrabOffset, m_rtStop - m_rtStart, channelRect.left, channelRect.Width());
    m_pMainFrame->m_trim.MoveBoundary(m_trimDrag, time);
    OnTrimChanged();
}

void CPlayerSeekBar::ShowTrimTip(CTrimSelection::Part part)
{
    const REFERENCE_TIME time = m_pMainFrame->m_trim.Selection().Boundary(part);
    m_tooltipText.Format(part == CTrimSelection::Part::Start ? IDS_TRIM_GRIP_START : IDS_TRIM_GRIP_END,
                         CTrimSelection::FormatTimeSeconds(time).c_str());
    m_ti.lpszText = (LPTSTR)(LPCTSTR)m_tooltipText;
    m_tooltip.SetToolInfo(&m_ti);
    if (m_tooltipState != TOOLTIP_VISIBLE || m_trimTip == CTrimSelection::Part::None) {
        KillTimer(TIMER_SHOWHIDE_TOOLTIP);
        m_tooltip.SendMessage(TTM_TRACKACTIVATE, TRUE, (LPARAM)&m_ti);
        m_tooltipState = TOOLTIP_VISIBLE;
        TRACKMOUSEEVENT tme = { sizeof(tme), TME_LEAVE, m_hWnd };
        TrackMouseEvent(&tme);
    }
    m_trimTip = part;

    const CSize bubbleSize(m_tooltip.GetBubbleSize(&m_ti));
    CRect clientRect;
    GetClientRect(clientRect);
    CPoint pt(TrimEdge(part) - bubbleSize.cx / 2, GetChannelRect().top - bubbleSize.cy - m_pMainFrame->m_dpi.ScaleY(13) - TrimTipLift());
    pt.x = std::clamp<LONG>(pt.x, clientRect.left, std::max<LONG>(clientRect.left, clientRect.right - bubbleSize.cx));
    ClientToScreen(&pt);
    m_tooltip.SendMessage(TTM_TRACKPOSITION, 0, MAKELPARAM(pt.x, pt.y));
}

BEGIN_MESSAGE_MAP(CTrimBadge, CWnd)
    ON_WM_PAINT()
    ON_WM_NCHITTEST()
    ON_WM_MOUSEACTIVATE()
END_MESSAGE_MAP()

void CTrimBadge::Show(CWnd* pOwner, const CString& text, const CRect& below, int gap)
{
    if (!m_hWnd) {
        const CString cls = AfxRegisterWndClass(0, ::LoadCursor(nullptr, IDC_ARROW));
        if (!CreateEx(WS_EX_NOACTIVATE | WS_EX_TOOLWINDOW, cls, _T(""), WS_POPUP, CRect(), pOwner, 0)) {
            return;
        }
        CMPCThemeUtil::getFontByType(m_font, pOwner, CMPCThemeUtil::MessageFont);
    }
    CSize size;
    {
        CClientDC dc(this);
        CFont* pOld = dc.SelectObject(&m_font);
        size = dc.GetTextExtent(text);
        dc.SelectObject(pOld);
    }
    size.cx += size.cy;
    size.cy += size.cy / 2;
    const CRect rect(CPoint(below.CenterPoint().x - size.cx / 2, below.top - gap - size.cy), size);
    const bool textChanged = text != m_text;
    const bool rectChanged = rect != m_rect || !IsWindowVisible();
    m_text = text;
    if (rectChanged) {
        m_rect = rect;
        SetWindowPos(nullptr, rect.left, rect.top, rect.Width(), rect.Height(), SWP_NOZORDER | SWP_NOACTIVATE | SWP_SHOWWINDOW | SWP_NOCOPYBITS);
    }
    if (textChanged || rectChanged) {
        // Paint at the final size before another mouse move can expose stale text.
        Invalidate(FALSE);
        UpdateWindow();
    }
}

void CTrimBadge::Hide()
{
    if (m_hWnd && IsWindowVisible()) {
        ShowWindow(SW_HIDE);
    }
}

void CTrimBadge::OnPaint()
{
    CPaintDC paintDC(this);
    CRect r;
    GetClientRect(r);
    CDC dc;
    CBitmap bitmap;
    if (r.IsRectEmpty() || !dc.CreateCompatibleDC(&paintDC) || !bitmap.CreateCompatibleBitmap(&paintDC, r.Width(), r.Height())) {
        return;
    }
    CBitmap* oldBitmap = dc.SelectObject(&bitmap);
    const bool themed = AppIsThemeLoaded();
    dc.FillSolidRect(r, themed ? CMPCTheme::MenuBGColor : GetSysColor(COLOR_INFOBK));
    CBrush border;
    border.CreateSolidBrush(themed ? CMPCTheme::TooltipBorderColor : GetSysColor(COLOR_WINDOWFRAME));
    dc.FrameRect(r, &border);
    CFont* pOld = dc.SelectObject(&m_font);
    dc.SetBkMode(TRANSPARENT);
    dc.SetTextColor(themed ? CMPCTheme::TextFGColor : GetSysColor(COLOR_INFOTEXT));
    dc.DrawText(m_text, r, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
    dc.SelectObject(pOld);
    paintDC.BitBlt(0, 0, r.Width(), r.Height(), &dc, 0, 0, SRCCOPY);
    dc.SelectObject(oldBitmap);
}

LRESULT CTrimBadge::OnNcHitTest(CPoint)
{
    return HTTRANSPARENT;
}

int CTrimBadge::OnMouseActivate(CWnd*, UINT, UINT)
{
    return MA_NOACTIVATE;
}

void CPlayerSeekBar::UpdateTrimBadge()
{
    if (!TrimActive() || m_trimDrag == CTrimSelection::Part::None || !IsWindowVisible() || m_pMainFrame->IsIconic()) {
        KillTimer(TIMER_TRIM_BADGE);
        m_trimBadge.Hide();
        return;
    }
    CString text;
    text.Format(IDS_TRIM_CLIP_LENGTH, CTrimSelection::FormatLength(m_pMainFrame->m_trim.Selection().Length()).c_str());
    CRect rect;
    GetWindowRect(rect);
    m_trimBadge.Show(m_pMainFrame, text, rect, m_pMainFrame->m_dpi.ScaleY(6));
    // Polled so that the badge follows the player when it moves, resizes or hides its controls.
    SetTimer(TIMER_TRIM_BADGE, 100, nullptr);
}

// While the length badge is up, the seekbar tooltips sit above it.
int CPlayerSeekBar::TrimTipLift() const
{
    const int height = m_trimBadge.Height();
    return height ? height + m_pMainFrame->m_dpi.ScaleY(4) : 0;
}

void CPlayerSeekBar::OnTrimChanged()
{
    CString name;
    if (TrimActive()) {
        const CTrimSelection& selection = m_pMainFrame->m_trim.Selection();
        name.Format(IDS_TRIM_ACCESSIBLE_NAME, CTrimSelection::FormatTime(selection.Start()).c_str(),
                    CTrimSelection::FormatTime(selection.End()).c_str());
        if (m_trimDrag != CTrimSelection::Part::None) {
            ShowTrimTip(m_trimDrag);
        }
    }
    UpdateTrimBadge();
    SetWindowText(name);
    Invalidate();
}

bool CPlayerSeekBar::CancelTrimDrag()
{
    if (m_trimDrag == CTrimSelection::Part::None) {
        return false;
    }
    m_pMainFrame->m_trim.MoveBoundary(m_trimDrag, m_trimDragOrigin);
    ReleaseCapture();
    return true;
}

void CPlayerSeekBar::DrawTrim(CDC& dc, const CRect& channelRect)
{
    using Part = CTrimSelection::Part;
    const CTrimSelection& selection = m_pMainFrame->m_trim.Selection();
    const bool modern = AppIsThemeLoaded();
    const int startX = TrimEdge(Part::Start);
    const int endX = TrimEdge(Part::End);

    dc.SelectClipRgn(nullptr);
    if (!modern) {
        dc.ExcludeClipRect(m_lastThumbRect);
    }
    const CRect before(channelRect.left, channelRect.top, startX, channelRect.bottom);
    const CRect after(endX, channelRect.top, channelRect.right, channelRect.bottom);
    if (modern) {
        // Lift the selected span and mute the excluded parts; the playhead is repainted so it
        // stays fully visible. Dark and light channels need opposite tints to keep the contrast.
        const COLORREF bg = CMPCTheme::ScrollBGColor;
        const bool dark = GetRValue(bg) + GetGValue(bg) + GetBValue(bg) < 3 * 128;
        auto tint = [&](const CRect& r, COLORREF color, BYTE alpha) {
            CDC mem;
            CBitmap pixel;
            if (r.IsRectEmpty() || !mem.CreateCompatibleDC(&dc) || !pixel.CreateCompatibleBitmap(&dc, 1, 1)) {
                return;
            }
            CBitmap* pOld = mem.SelectObject(&pixel);
            mem.SetPixelV(0, 0, color);
            const BLENDFUNCTION blend = { AC_SRC_OVER, 0, alpha, 0 };
            dc.AlphaBlend(r.left, r.top, r.Width(), r.Height(), &mem, 0, 0, 1, 1, blend);
            mem.SelectObject(pOld);
        };
        const CRect selected(startX, channelRect.top, endX, channelRect.bottom);
        tint(selected, dark ? RGB(255, 255, 255) : CMPCTheme::SeekbarCurrentPositionColor, dark ? 40 : 60);
        // The playhead never precedes Start, so the part before it would otherwise keep the
        // "played" fill and look lighter than the part after End. Both are excluded alike.
        dc.FillSolidRect(before, CMPCTheme::ScrollBGColor);
        dc.FillSolidRect(after, CMPCTheme::ScrollBGColor);
        tint(before, dark ? RGB(0, 0, 0) : RGB(128, 128, 128), dark ? 130 : 110);
        tint(after, dark ? RGB(0, 0, 0) : RGB(128, 128, 128), dark ? 130 : 110);
        const long playhead = channelRect.left + ChannelPointFromPosition(m_rtPosDraw);
        CRect playheadRect(playhead - m_pMainFrame->m_dpi.ScaleX(2) + 1, channelRect.top, playhead + m_pMainFrame->m_dpi.ScaleX(2), channelRect.bottom);
        playheadRect &= channelRect;
        dc.FillSolidRect(playheadRect, CMPCTheme::SeekbarCurrentPositionColor);
    } else {
        // The native trackbar selection look: the selected span is highlighted.
        dc.FillSolidRect(before, GetSysColor(COLOR_BTNFACE));
        dc.FillSolidRect(after, GetSysColor(COLOR_BTNFACE));
        if (endX > startX) {
            dc.FillSolidRect(startX, channelRect.top, endX - startX, channelRect.Height(), GetSysColor(COLOR_HIGHLIGHT));
        }
    }
    dc.SelectClipRgn(nullptr);

    const COLORREF gripColor = modern ? CMPCTheme::TextFGColor : GetSysColor(COLOR_BTNTEXT);
    const int w = TrimGripWidth();
    const int foot = std::max(1, m_pMainFrame->m_dpi.ScaleY(2));
    {
        // A thin frame joins the brackets so the selection reads as one box.
        const CRect first(GetTrimGripRect(Part::Start));
        const CRect last(GetTrimGripRect(Part::End));
        const int edge = std::max(1, m_pMainFrame->m_dpi.ScaleY(1));
        dc.FillSolidRect(first.left, first.top, last.right - first.left, edge, gripColor);
        dc.FillSolidRect(first.left, first.bottom - edge, last.right - first.left, edge, gripColor);
    }
    for (const Part part : { Part::Start, Part::End }) {
        const CRect grip(GetTrimGripRect(part));
        const CRect upright = part == Part::Start ? CRect(grip.left, grip.top, grip.left + w, grip.bottom)
                                                  : CRect(grip.right - w, grip.top, grip.right, grip.bottom);
        dc.FillSolidRect(upright, gripColor);
        dc.FillSolidRect(grip.left, grip.top, grip.Width(), foot, gripColor);
        dc.FillSolidRect(grip.left, grip.bottom - foot, grip.Width(), foot, gripColor);
        if (selection.Focus() == part && m_trimDrag == Part::None) {
            CRect focus(grip);
            focus.InflateRect(2, 1);
            dc.SetTextColor(gripColor);
            dc.SetBkColor(modern ? CMPCTheme::ContentBGColor : GetSysColor(COLOR_BTNFACE));
            dc.DrawFocusRect(focus);
        }
    }
}

void CPlayerSeekBar::UpdateTooltip(const CPoint& point)
{
    const CTrimSelection::Part grip = HitTestTrim(point);
    if (grip != CTrimSelection::Part::None && !DraggingThumb()) {
        m_pMainFrame->PreviewWindowHide();
        ShowTrimTip(grip);
        return;
    }
    if (m_trimTip != CTrimSelection::Part::None) {
        HideToolTip();
    }
    if (!m_bHasDuration || !GetSeekableRect().PtInRect(point) || DraggingThumb()) {
        HideToolTip();
        m_pMainFrame->PreviewWindowHide();
        return;
    }

    switch (m_tooltipState) {
        case TOOLTIP_HIDDEN: {
            // If mouse moved or the tooltip wasn't hidden by timeout
            if (point != m_tooltipPoint || m_bIgnoreLastTooltipPoint) {
                // Start show tooltip countdown
                m_tooltipState = TOOLTIP_TRIGGERED;
                VERIFY(SetTimer(TIMER_SHOWHIDE_TOOLTIP, TOOLTIP_SHOW_DELAY, nullptr));
                // Track mouse leave
                TRACKMOUSEEVENT tme = { sizeof(tme), TME_LEAVE, m_hWnd };
                VERIFY(TrackMouseEvent(&tme));
            }
        }
        break;
        case TOOLTIP_TRIGGERED:
            // Do nothing until tooltip is shown
            break;
        case TOOLTIP_VISIBLE:
            // Update the tooltip if needed
            if (point != m_tooltipPoint) {
                m_tooltipPoint = point;
                if (!m_pMainFrame->CanPreviewUse()) {
                    UpdateToolTipText();
                    UpdateToolTipPosition(point);
                    VERIFY(SetTimer(TIMER_SHOWHIDE_TOOLTIP, TOOLTIP_HIDE_TIMEOUT, nullptr));
                }
            }
            break;
        default:
            ASSERT(FALSE);
    }
}

void CPlayerSeekBar::UpdateToolTipPosition(CPoint point)
{
    if (m_pMainFrame->CanPreviewUse()) {
        if (!m_pMainFrame->m_wndPreView.IsWindowVisible()) {
            m_pMainFrame->m_wndPreView.SetWindowSize();
        }

        CRect rc;
        m_pMainFrame->m_wndPreView.GetWindowRect(&rc);
        const int r_width = rc.Width();
        const int r_height = rc.Height();

        MONITORINFO mi = { sizeof(mi) };
        GetMonitorInfoW(MonitorFromWindow(m_hWnd, MONITOR_DEFAULTTONEAREST), &mi);

        point.x -= r_width / 2 - 2;
        CRect cRect = GetChannelRect();
        CPoint bottomRight = cRect.BottomRight();
        ClientToScreen(&bottomRight);
        if (AfxGetAppSettings().nHoverPosition == TIME_TOOLTIP_BELOW_SEEKBAR && mi.rcWork.bottom > bottomRight.y + 8 + rc.Height()) {
            point.y = cRect.BottomRight().y + 8;
        } else {
            point.y = cRect.TopLeft().y - (r_height + 13);
        }
        ClientToScreen(&point);
        point.x = std::max(mi.rcWork.left + 5, std::min(point.x, mi.rcWork.right - r_width - 5));

        if (point.y <= 5) {
            const CRect r = mi.rcWork;
            if (!r.PtInRect(point)) {
                CPoint p(point);
                p.y = GetChannelRect().TopLeft().y + 30;
                ClientToScreen(&p);

                point.y = p.y;
            }
        }

        m_pMainFrame->m_wndPreView.SetWindowPos(&wndTopMost, point.x, point.y, 0, 0, SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
    } else {
        CSize bubbleSize(m_tooltip.GetBubbleSize(&m_ti));
        CRect windowRect;
        GetWindowRect(windowRect);

        if (AfxGetAppSettings().nHoverPosition == TIME_TOOLTIP_ABOVE_SEEKBAR) {
            point.x -= bubbleSize.cx / 2 - 2;
            point.y = GetChannelRect().TopLeft().y - (bubbleSize.cy + m_pMainFrame->m_dpi.ScaleY(13)) - TrimTipLift();
        } else {
            point.x += m_pMainFrame->m_dpi.ScaleX(10);
            point.y += m_pMainFrame->m_dpi.ScaleY(20);
        }
        point.x = std::max(0l, std::min(point.x, windowRect.Width() - bubbleSize.cx));
        ClientToScreen(&point);

        m_tooltip.SendMessage(TTM_TRACKPOSITION, 0, MAKELPARAM(point.x, point.y));
    }
}

void CPlayerSeekBar::GenerateToolTipText(REFERENCE_TIME rtPos)
{
    ASSERT(m_bHasDuration);

    CString time;
    GUID timeFormat = m_pMainFrame->GetTimeFormat();
    if (timeFormat == TIME_FORMAT_MEDIA_TIME) {
        DVD_HMSF_TIMECODE tcNow = RT2HMS_r(rtPos);
        if (tcNow.bHours > 0) {
            time.Format(_T("%02u:%02u:%02u"), tcNow.bHours, tcNow.bMinutes, tcNow.bSeconds);
        } else {
            time.Format(_T("%02u:%02u"), tcNow.bMinutes, tcNow.bSeconds);
        }
    } else if (timeFormat == TIME_FORMAT_FRAME) {
        time.Format(_T("%I64d"), rtPos);
    } else {
        m_tooltipText.Empty();
        return;
    }

    CComBSTR chapterName;
    {
        CAutoLock lock(&m_csChapterBag);
        if (m_pChapterBag) {
            REFERENCE_TIME rt = rtPos;
            m_pChapterBag->ChapLookup(&rt, &chapterName);
        }
    }

    if (chapterName.Length() == 0) {
        m_tooltipText = time;
    } else {
        m_tooltipText.Format(_T("%s - %s"), time.GetString(), static_cast<LPCTSTR>(chapterName));
    }
}

void CPlayerSeekBar::UpdateToolTipText()
{
    ASSERT(m_bHasDuration);
    REFERENCE_TIME rtPos = PositionFromClientPoint(m_tooltipPoint);
    GenerateToolTipText(rtPos);
    
    m_ti.lpszText = (LPTSTR)(LPCTSTR)m_tooltipText;
    m_tooltip.SetToolInfo(&m_ti);
}

void CPlayerSeekBar::UpdateToolTipTextPreview(REFERENCE_TIME rtPos)
{
    ASSERT(m_bHasDuration);
    GenerateToolTipText(rtPos);
    m_pMainFrame->m_wndPreView.SetWindowTextW(m_tooltipText);
}

void CPlayerSeekBar::Enable(bool bEnable)
{
    if (bEnable != m_bEnabled) {
        m_bEnabled = bEnable;
        Invalidate();
    }
}

void CPlayerSeekBar::HideToolTip()
{
    if (m_tooltipState != TOOLTIP_HIDDEN) {
        KillTimer(TIMER_SHOWHIDE_TOOLTIP);
        m_tooltip.SendMessage(TTM_TRACKACTIVATE, FALSE, (LPARAM)&m_ti);
        m_tooltipState = TOOLTIP_HIDDEN;
    }
    m_trimTip = CTrimSelection::Part::None;
}

void CPlayerSeekBar::GetRange(REFERENCE_TIME& rtStart, REFERENCE_TIME& rtStop) const
{
    rtStart = m_rtStart;
    rtStop = m_rtStop;
}

void CPlayerSeekBar::SetRange(REFERENCE_TIME rtStart, REFERENCE_TIME rtStop)
{
    if (rtStart < rtStop) {
        if (m_rtStart != rtStart || m_rtStop != rtStop) {
            m_rtStart = rtStart;
            m_rtStop = rtStop;
            auto hasChapters = [&]() {
                CAutoLock lock(&m_csChapterBag);
                return m_pChapterBag && m_pChapterBag->ChapGetCount();
            };
            if (!m_bHasDuration || hasChapters()) {
                Invalidate();
            }
            m_bHasDuration = true;
        }
    } else {
        m_rtStart = 0;
        m_rtStop = 0;
        if (m_bHasDuration) {
            m_bHasDuration = false;
            m_timeText.Empty();   // media closed: clear the time text (the section keeps its standard spacing)
            m_timeActualWidth = 0; // drop any actual-string widening so the closed state uses the model width
            HideToolTip();
            if (DraggingThumb() || m_trimDrag != CTrimSelection::Part::None) {
                ReleaseCapture();
            }
            Invalidate();
        }
    }
}

REFERENCE_TIME CPlayerSeekBar::GetPos() const
{
    return m_rtPos;
}

void CPlayerSeekBar::SetPos(REFERENCE_TIME rtPos)
{
    // Graph position polls must not overwrite the boundary preview during a drag.
    if (DraggingThumb() || DraggingTrim()) {
        return;
    }

    SyncThumbToVideo(rtPos, rtPos);
}

void CPlayerSeekBar::SetTrimPreviewPos(REFERENCE_TIME rtPos)
{
    SyncThumbToVideo(rtPos, rtPos, false);
}

bool CPlayerSeekBar::HasDuration() const
{
    return m_bHasDuration;
}

REFERENCE_TIME CPlayerSeekBar::GetDuration()
{
    return m_bHasDuration ? m_rtStop - m_rtStart : 0LL;
}

void CPlayerSeekBar::SetChapterBag(IDSMChapterBag* pCB)
{
    CAutoLock lock(&m_csChapterBag);
    m_pChapterBag = pCB;
    Invalidate();
}

void CPlayerSeekBar::RemoveChapters()
{
    SetChapterBag(nullptr);
}

bool CPlayerSeekBar::DraggingThumb()
{
    return m_bDraggingThumb;
}

BEGIN_MESSAGE_MAP(CPlayerSeekBar, CDialogBar)
    ON_WM_PAINT()
    ON_WM_LBUTTONDOWN()
    ON_WM_LBUTTONDBLCLK()
    ON_WM_LBUTTONUP()
    ON_WM_XBUTTONDOWN()
    ON_WM_XBUTTONUP()
    ON_WM_XBUTTONDBLCLK()
    ON_WM_MBUTTONDOWN()
    ON_WM_MOUSEMOVE()
    ON_WM_CONTEXTMENU()
    ON_WM_ERASEBKGND()
    ON_WM_SETCURSOR()
    ON_WM_TIMER()
    ON_WM_MOUSELEAVE()
    ON_WM_THEMECHANGED()
    ON_WM_CAPTURECHANGED()
END_MESSAGE_MAP()

void CPlayerSeekBar::OnPaint()
{
    CPaintDC dc(this);

    COLORREF
    dark   = GetSysColor(COLOR_GRAYTEXT),
    white  = GetSysColor(COLOR_WINDOW),
    shadow = GetSysColor(COLOR_3DSHADOW),
    light  = GetSysColor(COLOR_3DHILIGHT),
    bkg    = GetSysColor(COLOR_BTNFACE);

    const CRect channelRect(GetChannelRect());
    auto funcMarkChannel = [&](REFERENCE_TIME pos, long verticalPadding, long thickness, COLORREF markColor) {
        long markPos = channelRect.left + ChannelPointFromPosition(pos);
        CRect r(markPos, channelRect.top + verticalPadding, markPos + thickness - 1, channelRect.bottom - verticalPadding);
        if (r.right < channelRect.right) {
            r.right++;
        }
        ASSERT(r.right <= channelRect.right);
        dc.FillSolidRect(&r, markColor);
        dc.ExcludeClipRect(&r);
    };

    const CAppSettings& s = AfxGetAppSettings();
    if (AppIsThemeLoaded()) {
        // Thumb
        CRect r(GetThumbRect());
        m_lastThumbRect = r;

        if (m_bHasDuration) {
            // A-B Repeat
            REFERENCE_TIME aPos, bPos;
            if (m_pMainFrame->CheckABRepeat(aPos, bPos)) {
                long thickness = m_pMainFrame->m_dpi.ScaleX(2);
                if (aPos) {
                    funcMarkChannel(aPos, 1, thickness, CMPCTheme::SeekbarABColor);
                }
                if (bPos) {
                    funcMarkChannel(bPos, 1, thickness, CMPCTheme::SeekbarABColor);
                }
            }

            // Chapters
            CAutoLock lock(&m_csChapterBag);
            if (m_pChapterBag) {
                long thickness = m_pMainFrame->m_dpi.ScaleX(1);
                for (DWORD i = 0; i < m_pChapterBag->ChapGetCount(); i++) {
                    REFERENCE_TIME rtChap;
                    if (SUCCEEDED(m_pChapterBag->ChapGet(i, &rtChap, nullptr))) {
                        funcMarkChannel(rtChap, 1, thickness, CMPCTheme::SeekbarChapterColor);
                    } else {
                        ASSERT(FALSE);
                    }
                }
            }
        }

        // Channel
        {
            {
                long seekPos = ChannelPointFromPosition(m_rtPosDraw);
                CRect r, playedRect, unplayedRect, curPosRect;
                playedRect = channelRect;
                playedRect.right = playedRect.left + seekPos - m_pMainFrame->m_dpi.ScaleX(2) + 1;
                dc.FillSolidRect(&playedRect, CMPCTheme::ScrollProgressColor);
                unplayedRect = channelRect;
                unplayedRect.left = playedRect.left + seekPos + m_pMainFrame->m_dpi.ScaleX(2);
                dc.FillSolidRect(&unplayedRect, m_bEnabled ? CMPCTheme::ScrollBGColor : CMPCTheme::ScrollBGColor);
                curPosRect = channelRect;
                curPosRect.left = playedRect.right;
                curPosRect.right = unplayedRect.left;
                dc.FillSolidRect(&curPosRect, CMPCTheme::SeekbarCurrentPositionColor);
            }
            CRect r(channelRect);
            CBrush fb;
            fb.CreateSolidBrush(CMPCTheme::NoBorderColor);
            dc.FrameRect(&r, &fb);
            fb.DeleteObject();
            dc.ExcludeClipRect(&r);
        }

        // Background
        {
            CRect r;
            GetClientRect(&r);
            dc.FillSolidRect(&r, CMPCTheme::ContentBGColor);
        }
    } else {
        // Thumb
        {
            auto& pThumb = m_bEnabled ? m_pEnabledThumb : m_pDisabledThumb;
            if (!pThumb) {
                CreateThumb(m_bEnabled, dc);
                ASSERT(pThumb);
            }
            CRect r(GetThumbRect());
            CRect ri(GetInnerThumbRect(m_bEnabled, r));

            CRgn rg, rgi;
            VERIFY(rg.CreateRectRgnIndirect(&r));
            VERIFY(rgi.CreateRectRgnIndirect(&ri));

            ExtSelectClipRgn(dc, rgi, RGN_DIFF);
            VERIFY(dc.BitBlt(r.TopLeft().x, r.TopLeft().y, r.Width(), r.Height(), pThumb.get(), 0, 0, SRCCOPY));
            ExtSelectClipRgn(dc, rg, RGN_XOR);

            m_lastThumbRect = r;
        }

        if (m_bHasDuration) {
            // A-B Repeat
            REFERENCE_TIME aPos, bPos;
            if (m_pMainFrame->CheckABRepeat(aPos, bPos)) {
                long thickness = m_pMainFrame->m_dpi.ScaleX(2);
                if (aPos) {
                    funcMarkChannel(aPos, 0, thickness, CMPCTheme::SeekbarABColor);
                }
                if (bPos) {
                    funcMarkChannel(bPos, 0, thickness, CMPCTheme::SeekbarABColor);
                }
            }

            // Chapters
            CAutoLock lock(&m_csChapterBag);
            if (m_pChapterBag) {
                long thickness = m_pMainFrame->m_dpi.ScaleX(1);
                for (DWORD i = 0; i < m_pChapterBag->ChapGetCount(); i++) {
                    REFERENCE_TIME rtChap;
                    if (SUCCEEDED(m_pChapterBag->ChapGet(i, &rtChap, nullptr))) {
                        funcMarkChannel(rtChap, 0, thickness, dark);
                    } else {
                        ASSERT(FALSE);
                    }
                }
            }
        }

        // Channel
        {
            dc.FillSolidRect(&channelRect, m_bEnabled ? white : bkg);
            CRect r(channelRect);
            r.InflateRect(1, 1);
            dc.Draw3dRect(&r, shadow, light);
            dc.ExcludeClipRect(&r);
        }

        // Background
        {
            CRect r;
            GetClientRect(&r);
            dc.FillSolidRect(&r, bkg);
        }
    }

    if (TrimActive()) {
        DrawTrim(dc, channelRect);
    }

    // Modern theme only (ShowTimeOnSeekBar() is false under the classic theme): draw the time in its
    // dedicated section beside the channel (the channel was shrunk to make room in GetChannelRect, so
    // the time is never drawn over it). Aligned toward the outer edge of the bar - right by default,
    // left when the advanced option places the section on the left.
    if (TimeSectionVisible()) {
        dc.SelectClipRgn(nullptr); // channel/thumb drawing restricted the clip region
        EnsureTimeFont();
        CFont* pOldFont = dc.SelectObject(&m_timeFont);
        int oldBkMode = dc.SetBkMode(TRANSPARENT);
        COLORREF oldColor = dc.SetTextColor(CMPCTheme::TextFGColor);
        CRect rt(GetTimeRect());
        const int pad = m_pMainFrame->m_dpi.ScaleX(TIME_SECTION_PADDING);
        UINT align;
        if (TimeSectionOnLeft()) {
            rt.left += pad;
            align = DT_LEFT;
        } else {
            rt.right -= pad;
            align = DT_RIGHT;
        }
        dc.DrawText(m_timeText, rt, align | DT_SINGLELINE | DT_VCENTER | DT_NOPREFIX);
        dc.SetTextColor(oldColor);
        dc.SetBkMode(oldBkMode);
        dc.SelectObject(pOldFont);
    }
}

void CPlayerSeekBar::OnContextMenu(CWnd* pWnd, CPoint point)
{
    // Right-clicking the time section shows the same Remaining time / High precision / Show percentage
    // options as the status-bar time control; elsewhere on the seekbar keep the default behavior (#3256).
    if (TimeSectionVisible() && point.x != -1) {
        CPoint clientPoint = point;
        ScreenToClient(&clientPoint);
        if (GetTimeRect().PtInRect(clientPoint)) {
            m_pMainFrame->m_wndStatusBar.ShowTimerOptionsMenu(this, point);
            return;
        }
    }
    if (TrimActive()) {
        if (point.x == -1) {
            point = GetChannelRect().CenterPoint();
            ClientToScreen(&point);
        }
        HideToolTip();
        m_pMainFrame->PreviewWindowHide();
        m_pMainFrame->m_trim.ShowContextMenu(this, point);
        return;
    }
    __super::OnContextMenu(pWnd, point);
}

void CPlayerSeekBar::OnLButtonDown(UINT nFlags, CPoint point)
{
    if (m_bEnabled && m_bHasDuration && GetSeekableRect().PtInRect(point)) {
        const CTrimSelection::Part grip = HitTestTrim(point);
        if (grip != CTrimSelection::Part::None) {
            SetCapture();
            m_trimDrag = grip;
            m_trimDragOrigin = m_pMainFrame->m_trim.Selection().Boundary(grip);
            m_trimGrabOffset = point.x - TrimEdge(grip);
            m_pMainFrame->PreviewWindowHide();
            m_pMainFrame->m_trim.BeginBoundaryDrag(grip);
            OnTrimChanged();
            return;
        }
        if (TrimActive()) {
            m_pMainFrame->m_trim.SetFocus(CTrimSelection::Part::Playhead);
            Invalidate();
        }
        pauseAfterFirstScroll = AfxGetAppSettings().bPauseWhileDraggingSeekbar && m_pMainFrame->GetMediaState() == State_Running;
        pausedForScrolling = false;
        SetCapture();
        m_bDraggingThumb = true;
        MoveThumb(point);
        CheckScrollDistance(point, 0ULL, 0LL);
        invalidateThumb();
    } else {
        if (!m_pMainFrame->m_fFullScreen) {
            ClientToScreen(&point);
            m_pMainFrame->PostMessage(WM_NCLBUTTONDOWN, HTCAPTION, MAKELPARAM(point.x, point.y));
        }
    }
}

void CPlayerSeekBar::OnLButtonDblClk(UINT nFlags, CPoint point)
{
    OnLButtonDown(nFlags, point);
}

void CPlayerSeekBar::OnLButtonUp(UINT nFlags, CPoint point)
{
    if (m_trimDrag != CTrimSelection::Part::None) {
        MoveTrimGrip(point);
        ReleaseCapture();   // OnCaptureChanged commits the boundary
        return;
    }
    if (DraggingThumb()) {
        ReleaseCapture();
        // update video position if seekbar moved at least 250 ms or 1/100th of duration
        CheckScrollDistance(point, std::min(2500000LL, m_rtStop / 100), 0LL);
        invalidateThumb();
        pauseAfterFirstScroll = false;
        if (pausedForScrolling) {
            pausedForScrolling = false;
            m_pMainFrame->MediaControlRun();
        }
    }
    checkHover(point);
}

void CPlayerSeekBar::OnXButtonDown(UINT nFlags, UINT nButton, CPoint point)
{
    UNREFERENCED_PARAMETER(nFlags);
    UNREFERENCED_PARAMETER(point);
    // do medium jumps when clicking mouse navigation buttons on the seekbar
    // if not dragging the seekbar thumb
    if (!DraggingThumb()) {
        switch (nButton) {
            case XBUTTON1:
                SendMessage(WM_COMMAND, ID_PLAY_SEEKBACKWARDMED);
                break;
            case XBUTTON2:
                SendMessage(WM_COMMAND, ID_PLAY_SEEKFORWARDMED);
                break;
        }
    }
}

void CPlayerSeekBar::OnXButtonUp(UINT nFlags, UINT nButton, CPoint point)
{
    UNREFERENCED_PARAMETER(nFlags);
    UNREFERENCED_PARAMETER(nButton);
    UNREFERENCED_PARAMETER(point);
    // do nothing
}

void CPlayerSeekBar::OnXButtonDblClk(UINT nFlags, UINT nButton, CPoint point)
{
    OnXButtonDown(nFlags, nButton, point);
}

void CPlayerSeekBar::checkHover(CPoint point)
{
    CRect tRect(GetThumbRect());
    bool oldHover = m_bHoverThumb;
    m_bHoverThumb = false;
    if (m_bEnabled && m_bHasDuration && tRect.PtInRect(point)) {
        m_bHoverThumb = true;
    }

    if (m_bHoverThumb != oldHover) {
        invalidateThumb();
    }
}

void CPlayerSeekBar::invalidateThumb()
{
    CRect tRect(GetThumbRect());
    InvalidateRect(tRect);
}

void CPlayerSeekBar::OnMouseMove(UINT nFlags, CPoint point)
{
    if (m_trimDrag != CTrimSelection::Part::None) {
        if (nFlags & MK_LBUTTON) {
            MoveTrimGrip(point);
        }
        return;
    }
    if (DraggingThumb() && (nFlags & MK_LBUTTON)) {
        MoveThumb(point);
        // update video position if seekbar moved at least 500ms or 1/30th of duration
        CheckScrollDistance(point, std::min(5000000LL, m_rtStop / 30), 150LL);
    }

    if (AfxGetAppSettings().fUseSeekbarHover) {
        UpdateTooltip(point);

        if (m_bHasDuration && m_pMainFrame->CanPreviewUse()) {
            const OAFilterState fs = m_pMainFrame->m_CachedFilterState;
            if (fs != -1 && GetSeekableRect().PtInRect(point)) {
                UpdateToolTipPosition(point);
                PreviewWindowShow(point);
            } else {
                m_pMainFrame->PreviewWindowHide();
            }
        }
    }
}

BOOL CPlayerSeekBar::OnEraseBkgnd(CDC* pDC)
{
    return TRUE;
}

BOOL CPlayerSeekBar::OnSetCursor(CWnd* pWnd, UINT nHitTest, UINT message)
{
    BOOL ret = TRUE;
    CPoint point;
    GetCursorPos(&point);
    ScreenToClient(&point);
    if (m_bEnabled && m_bHasDuration && GetSeekableRect().PtInRect(point)) {
        const bool grip = m_trimDrag != CTrimSelection::Part::None || HitTestTrim(point) != CTrimSelection::Part::None;
        ::SetCursor(grip ? AfxGetApp()->LoadStandardCursor(IDC_SIZEWE) : m_cursor);
    } else {
        ret = __super::OnSetCursor(pWnd, nHitTest, message);
    }
    return ret;
}

void CPlayerSeekBar::OnTimer(UINT_PTR nIDEvent)
{
    CPoint point;
    VERIFY(GetCursorPos(&point));
    ScreenToClient(&point);
    switch (nIDEvent) {
        case TIMER_SHOWHIDE_TOOLTIP:
            if (m_tooltipState == TOOLTIP_TRIGGERED && m_bHasDuration) {
                m_tooltipPoint = point;
                UpdateToolTipText();
                if (!m_pMainFrame->CanPreviewUse()) {
                    m_tooltip.SendMessage(TTM_TRACKACTIVATE, TRUE, (LPARAM)&m_ti);
                }
                UpdateToolTipPosition(point);
                m_tooltipState = TOOLTIP_VISIBLE;
                VERIFY(SetTimer(TIMER_SHOWHIDE_TOOLTIP, m_pMainFrame->CanPreviewUse() ? 10 : TOOLTIP_HIDE_TIMEOUT, nullptr));
            } else if (m_tooltipState == TOOLTIP_VISIBLE) {
                HideToolTip();
                PreviewWindowShow(point);
                ASSERT(!m_bIgnoreLastTooltipPoint);
                KillTimer(TIMER_SHOWHIDE_TOOLTIP);
            } else {
                KillTimer(TIMER_SHOWHIDE_TOOLTIP);
            }
            break;
        case TIMER_TRIM_BADGE:
            UpdateTrimBadge();
            break;
        default:
            ASSERT(FALSE);
    }
}

void CPlayerSeekBar::OnMouseLeave()
{
    HideToolTip();
    m_pMainFrame->PreviewWindowHide();
    m_bIgnoreLastTooltipPoint = true;
    checkHover(CPoint(0, 0));
}

LRESULT CPlayerSeekBar::OnThemeChanged()
{
    m_pEnabledThumb = nullptr;
    m_pDisabledThumb = nullptr;
    m_timeFont.DeleteObject();
    return __super::OnThemeChanged();
}

void CPlayerSeekBar::OnCaptureChanged(CWnd* pWnd)
{
    if (m_trimDrag != CTrimSelection::Part::None) {
        // Also reached when another window takes the capture: the boundary stays where it was dragged.
        const CTrimSelection::Part part = m_trimDrag;
        m_trimDrag = CTrimSelection::Part::None;
        HideToolTip();
        m_bIgnoreLastTooltipPoint = true;
        if (m_pMainFrame->m_trim.IsActive()) {
            m_pMainFrame->m_trim.EndBoundaryDrag(part);
        }
        OnTrimChanged();
        return;
    }
    // Capture can also change without a drag in progress (e.g. when the time section's right-click
    // popup menu takes capture), so only run the drag-end cleanup when we were actually dragging.
    if (m_bDraggingThumb) {
        m_bDraggingThumb = false;
        pauseAfterFirstScroll = false;
        if (pausedForScrolling) {
            pausedForScrolling = false;
            m_pMainFrame->MediaControlRun();
        }
        if (!pWnd) {
            // HACK: windowed (not renderless) video renderers may not produce WM_MOUSEMOVE message here
            m_pMainFrame->UpdateControlState(CMainFrame::UPDATE_CHILDVIEW_CURSOR_HACK);
        }
    }
}

void CPlayerSeekBar::PreviewWindowShow(CPoint point) {
    if ((point.x != m_last_pointx_preview || m_bIgnoreLastTooltipPoint) && !DraggingThumb()) {
        m_bIgnoreLastTooltipPoint = false;
        m_last_pointx_preview = point.x;
        if (m_pMainFrame->CanPreviewUse()) {
            REFERENCE_TIME newpos = std::clamp(PositionFromClientPoint(point), 0LL, m_rtStop);
            if (newpos != m_pos_preview) {
                if (GetKeyState(VK_SHIFT) >= 0) {
                    newpos = m_pMainFrame->GetClosestKeyFramePreview(newpos);
                }
            }
            if (newpos != m_pos_preview || !m_pMainFrame->m_wndPreView.IsWindowVisible()) {
                m_pos_preview = newpos;
                UpdateToolTipTextPreview(m_pos_preview);
                m_pMainFrame->PreviewWindowShow(m_pos_preview);
            }
        }
    }
}

void CPlayerSeekBar::OnMButtonDown(UINT nFlags, CPoint point) {
    if (m_pMainFrame->m_wndPreView && ::IsWindow(m_pMainFrame->m_wndPreView)) {
        if (m_pMainFrame->m_bUseSeekPreview) {
            if (m_pMainFrame->m_wndPreView.IsWindowVisible()) {
                m_pMainFrame->PreviewWindowHide();
                OnMouseMove(nFlags, point);
            }
            m_pMainFrame->m_bUseSeekPreview = false;
        } else {
            m_pMainFrame->m_bUseSeekPreview = (m_pMainFrame->m_previewGraph.m_pGB != nullptr);
        }
    }
}
