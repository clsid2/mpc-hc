/*
 * (C) 2003-2006 Gabest
 * (C) 2006-2015 see Authors.txt
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
#include "mplayerc.h"
#include "PlayerStatusBar.h"
#include "MainFrm.h"
#include "DSUtil.h"
#include "CMPCTheme.h"
#include "DpiHelper.h"
#include "ImageGrayer.h"
#include "SVGImage.h"

// CPlayerStatusBar

// tooltip tool for the audio channel icon, which is painted rather than a control
static constexpr UINT_PTR AUDIO_ICON_TOOL = 1;

IMPLEMENT_DYNAMIC(CPlayerStatusBar, CDialogBar)

CPlayerStatusBar::CPlayerStatusBar(CMainFrame* pMainFrame)
    : m_pMainFrame(pMainFrame)
    , m_status(pMainFrame->m_dpi, false, true)
    , m_time(pMainFrame->m_dpi, true, false)
    , m_nAudioChannels(-1)
    , m_audioRect(0, 0, 0, 0)
    , m_hIcon(0)
    , m_rtNow(0LL)
    , m_rtDur(0LL)
    , m_time_rect(-1, -1, -1, -1)
{
    EventRouter::EventSelection fires;
    fires.insert(MpcEvent::STREAM_POS_UPDATE_REQUEST);
    EventRouter::EventSelection receives;
    receives.insert(MpcEvent::DPI_CHANGED);
    GetEventd().Connect(m_eventc, receives, std::bind(&CPlayerStatusBar::EventCallback, this, std::placeholders::_1), fires);
}

CPlayerStatusBar::~CPlayerStatusBar()
{
    if (m_hIcon) {
        DestroyIcon(m_hIcon);
    }
}

BOOL CPlayerStatusBar::Create(CWnd* pParentWnd)
{
    BOOL ret = CDialogBar::Create(pParentWnd, IDD_PLAYERSTATUSBAR, WS_CHILD | WS_VISIBLE | CBRS_ALIGN_BOTTOM, IDD_PLAYERSTATUSBAR);

    // Should never be RTLed
    ModifyStyleEx(WS_EX_LAYOUTRTL, WS_EX_NOINHERITLAYOUT);
    CreateToolTip();

    return ret;
}

void CPlayerStatusBar::CreateToolTip()
{
    CToolTipCtrl& tip = AppIsThemeLoaded() ? themedToolTip : m_tooltip;
    tip.Create(this, TTS_NOPREFIX | TTS_ALWAYSTIP);
    tip.SetDelayTime(TTDT_INITIAL, 0);
    tip.SetDelayTime(TTDT_AUTOPOP, 2500);
    tip.SetDelayTime(TTDT_RESHOW, 0);
    tip.AddTool(&m_time, IDS_TOOLTIP_REMAINING_TIME);
    tip.AddTool(&m_status);
    tip.AddTool(this, LPSTR_TEXTCALLBACK, m_audioRect, AUDIO_ICON_TOOL);
}

//the tooltip and the audio channel icon are the ones made for the theme, so swap them
LRESULT CPlayerStatusBar::OnMPCThemeChanged(WPARAM wParam, LPARAM lParam)
{
    themedToolTip.DestroyWindow();
    m_tooltip.DestroyWindow();
    CreateToolTip();
    LoadStatusBitmap();
    Invalidate();
    Relayout();
    return 0;
}

BOOL CPlayerStatusBar::PreCreateWindow(CREATESTRUCT& cs)
{
    if (!CDialogBar::PreCreateWindow(cs)) {
        return FALSE;
    }

    m_dwStyle &= ~CBRS_BORDER_TOP;
    m_dwStyle &= ~CBRS_BORDER_BOTTOM;

    return TRUE;
}

CSize CPlayerStatusBar::CalcFixedLayout(BOOL bStretch, BOOL bHorz)
{
    CSize ret = __super::CalcFixedLayout(bStretch, bHorz);
    if (!m_initialWindowDPI) {
        m_initialWindowDPI = m_pMainFrame->m_dpi.DPIY(); //the initial DPI is always cached by CDialogBar and is never updated for future calculations of CalcFixedLayout
    }
    CSize r2 = ret;
    ret.cy = m_pMainFrame->m_dpi.ScaleArbitraryToOverrideY(ret.cy, m_initialWindowDPI); //we must scale by initial DPI, NOT current DPI
    ret.cy = std::max<long>(ret.cy, m_pMainFrame->m_dpi.ScaleY(24)); //at least 24px scaled to current dpi
    return ret;
}

int CPlayerStatusBar::OnCreate(LPCREATESTRUCT lpCreateStruct)
{
    if (CDialogBar::OnCreate(lpCreateStruct) == -1) {
        return -1;
    }

    CRect r;
    r.SetRectEmpty();

    m_type.Create(_T(""), WS_CHILD | WS_VISIBLE | SS_ICON | SS_CENTERIMAGE,
                  r, this, IDC_STATIC1);

    m_status.Create(_T(""), WS_CHILD | WS_VISIBLE | SS_OWNERDRAW | SS_NOTIFY,
                    r, this, IDC_PLAYERSTATUS);

    m_time.Create(_T(""), WS_CHILD | WS_VISIBLE | SS_OWNERDRAW | SS_NOTIFY,
                  r, this, IDC_PLAYERTIME);
    // Should never be RTLed
    m_time.ModifyStyleEx(WS_EX_LAYOUTRTL, WS_EX_NOINHERITLAYOUT);

    m_status.SetWindowPos(&m_time, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);

    ScaleFont();

    Relayout();

    return 0;
}

void CPlayerStatusBar::ScaleFont() {
    m_status.ScaleFont(m_pMainFrame->m_dpi);
    m_time.ScaleFont(m_pMainFrame->m_dpi);
}

void CPlayerStatusBar::EventCallback(MpcEvent ev)
{
    switch (ev) {
        case MpcEvent::DPI_CHANGED:
            ScaleFont();
            LoadStatusBitmap();
            SetMediaTypeIcon();
            Invalidate();
            Relayout();
            break;

        default:
            ASSERT(FALSE);
    }
}

void CPlayerStatusBar::Relayout()
{
    const CAppSettings& s = AfxGetAppSettings();
    CRect rfull;
    GetClientRect(rfull);

    if (s.bShowAudioFormatInStatusbar) {
        rfull.DeflateRect(8, 4, 8, 4);
        m_audioRect.SetRectEmpty();
    } else {
        int iconWidth = GetAudioIconWidth();
#if 0
        if (m_type.GetIcon()) {
            CRect rtype;
            rtype.SetRect(6, rfull.top + 4, 6 + m_pMainFrame->m_dpi.ScaleX(16), rfull.bottom - 4);
            m_type.MoveWindow(rtype);
        }

        rfull.DeflateRect(11 + m_pMainFrame->m_dpi.ScaleX(16), 5, iconWidth + 8, 4);
#else
        rfull.DeflateRect(8, 4, iconWidth + 8, 4);
#endif
        GetClientRect(m_audioRect);
        m_audioRect.left = m_audioRect.right - iconWidth - 1;
    }
    CToolTipCtrl& tip = AppIsThemeLoaded() ? themedToolTip : m_tooltip;
    if (tip.m_hWnd) {
        tip.SetToolRect(this, AUDIO_ICON_TOOL, m_audioRect);
    }

    if (CDC* pDC = m_time.GetDC()) {
        CFont* pOld = pDC->SelectObject(&m_time.GetFont());
        CRect rtime = rfull;
        CString str;
        m_time.GetWindowText(str);
        // When the time is shown on the seekbar instead (modern theme only), collapse the
        // status-bar time control (but keep its text so GetStatusTimer() still feeds the seekbar).
        if (str.IsEmpty() || (s.nTimeOnSeekBar == TIME_ON_SEEKBAR_ALWAYS && AppIsThemeLoaded())) {
            rtime.left = rtime.right;
        } else {
            rtime.left = rtime.right - pDC->GetTextExtent(str).cx;
        }
        m_time.MoveWindow(&rtime, FALSE);
        m_time_rect = rtime;
        pDC->SelectObject(pOld);
        m_time.ReleaseDC(pDC);
    }

    CRect rstatus = rfull;
    if (m_time_rect.left > 0) {
        rstatus.right = m_time_rect.left - 8;
    }
    m_status.MoveWindow(&rstatus, FALSE);

    // rfull excludes the strip the audio channel icon sits in, so invalidate everything
    Invalidate();
    UpdateWindow();
}

void CPlayerStatusBar::Clear()
{
    m_status.SetWindowText(_T(""));
    m_time.SetWindowText(_T(""));
    m_typeExt.Empty();
    SetMediaTypeIcon();
    SetAudioChannels(-1);
}

void CPlayerStatusBar::SetAudioChannels(int nChannels)
{
    if (m_nAudioChannels == nChannels) {
        return;
    }

    m_nAudioChannels = nChannels;
    LoadStatusBitmap();

    Invalidate();
    Relayout();
}

// The modern theme draws a speaker from an SVG scaled to the current DPI and tinted like the
// status text, flattened onto the bar colour so OnPaint can BitBlt it like the classic bitmap,
// with the channel count written beside it. Rebuilt whenever the channels, the theme colours
// or the DPI change.
void CPlayerStatusBar::LoadStatusBitmap()
{
    if (m_bm.m_hObject) {
        m_bm.DeleteObject();
    }
    if (m_nAudioChannels < 0) {
        return;
    }

    if (AppIsThemeLoaded()) {
        UINT svgid = m_nAudioChannels > 0 ? IDF_SVG_AUDIOTYPE_SPEAKER : IDF_SVG_AUDIOTYPE_NOAUDIO;
        CImage svg, tinted;
        // the SVGs are 16 high with 2px clear on either side, like the bitmaps, and differ in width
        if (SUCCEEDED(SVGImage::Load(svgid, svg, m_pMainFrame->m_dpi.ScaleY(16) / 16.0f))
                && ImageGrayer::Colorize(svg, tinted, CMPCTheme::InfoBarTextColor, CMPCTheme::InfoBarBGColor, false)) {
            m_bm.Attach(tinted.Detach());
        }
    } else {
        UINT id = m_nAudioChannels == 0 ? IDB_AUDIOTYPE_NOAUDIO
                  : m_nAudioChannels == 1 ? IDB_AUDIOTYPE_MONO
                  : IDB_AUDIOTYPE_STEREO;
        // We can't use m_bm.LoadBitmap(id) directly since we want to load the bitmap from the main executable
        CImage img;
        img.LoadFromResource(AfxGetInstanceHandle(), id);
        m_bm.Attach(img.Detach());
    }
}

CString CPlayerStatusBar::GetAudioChannelsLabel() const
{
    CString label;
    if (AppIsThemeLoaded() && m_nAudioChannels > 0) {
        label.Format(_T("%dch"), m_nAudioChannels);
    }
    return label;
}

// The speaker and the channel count after it
int CPlayerStatusBar::GetAudioIconWidth()
{
    int width = 0;
    if (m_bm.m_hObject) {
        BITMAP bm;
        m_bm.GetBitmap(&bm);
        width = bm.bmWidth;
    }
    CString label = GetAudioChannelsLabel();
    if (!label.IsEmpty()) {
        if (CDC* pDC = GetDC()) {
            CFont* pOld = pDC->SelectObject(&m_status.GetFont());
            width += pDC->GetTextExtent(label).cx + m_pMainFrame->m_dpi.ScaleX(2);
            pDC->SelectObject(pOld);
            ReleaseDC(pDC);
        }
    }
    return width;
}

void CPlayerStatusBar::SetMediaType(CString ext)
{
    if (ext != m_typeExt) {
        m_typeExt = ext;
        SetMediaTypeIcon();
    }
}

CString CPlayerStatusBar::GetStatusMessage() const
{
    CString strResult;

    m_status.GetWindowText(strResult);

    return strResult;
}

void CPlayerStatusBar::SetStatusMessage(CString str)
{
    str.Trim();
    if (GetStatusMessage() != str) {
        m_status.SetRedraw(FALSE);
        m_status.SetWindowText(str);
        m_status.SetRedraw(TRUE);
        Relayout();
    }
}

CString CPlayerStatusBar::PreparePathStatusMessage(CLongPath path)
{
    if (CDC* pDC = m_status.GetDC()) {
        CRect r;
        m_status.GetClientRect(r);
        path.CompactPath(pDC->m_hDC, m_time_rect.left - r.left - 1);
        m_status.ReleaseDC(pDC);
    } else {
        ASSERT(FALSE);
    }

    return path;
}

REFERENCE_TIME CPlayerStatusBar::GetTimerCurPos()
{
    return m_rtNow;
}

REFERENCE_TIME CPlayerStatusBar::GetTimerDuration()
{
    return m_rtDur;
}

CString CPlayerStatusBar::GetStatusTimer() const
{
    CString strResult;

    m_time.GetWindowText(strResult);

    return strResult;
}

void CPlayerStatusBar::SetStatusTimer(CString str)
{
    str.Trim();
    if (GetStatusTimer() != str) {
        m_time.SetRedraw(FALSE);
        m_time.SetWindowText(str);
        m_time.SetRedraw(TRUE);
        Relayout();
    }
}

void CPlayerStatusBar::SetStatusTimer(REFERENCE_TIME rtNow, REFERENCE_TIME rtDur, bool fHighPrecision, const GUID& timeFormat/* = TIME_FORMAT_MEDIA_TIME*/)
{
    CString str;
    CString posstr;
    const CAppSettings& s = AfxGetAppSettings();

    m_rtNow = rtNow;
    m_rtDur = rtDur;

    if (rtDur > 0) {
        REFERENCE_TIME rtRem = rtDur - rtNow;
        CString durstr, remstr;

        if (timeFormat == TIME_FORMAT_MEDIA_TIME) {
            DVD_HMSF_TIMECODE tcNow, tcDur, tcRem;

            if (fHighPrecision || s.bHighPrecisionTimer) {
                tcNow = RT2HMSF(rtNow);
                tcDur = RT2HMSF(rtDur);
                tcRem = RT2HMSF(rtRem);
            } else {
                tcNow = RT2HMS(rtNow);
                tcDur = RT2HMS(rtDur);
                tcRem = RT2HMS(rtRem);
            }

            if (tcDur.bHours > 0 || (rtNow > rtDur && tcNow.bHours > 0)) {
                posstr.Format(_T("%02u:%02u:%02u"), tcNow.bHours, tcNow.bMinutes, tcNow.bSeconds);
                durstr.Format(_T("%02u:%02u:%02u"), tcDur.bHours, tcDur.bMinutes, tcDur.bSeconds);
                remstr.Format(_T("%02u:%02u:%02u"), tcRem.bHours, tcRem.bMinutes, tcRem.bSeconds);
            } else {
                posstr.Format(_T("%02u:%02u"), tcNow.bMinutes, tcNow.bSeconds);
                durstr.Format(_T("%02u:%02u"), tcDur.bMinutes, tcDur.bSeconds);
                remstr.Format(_T("%02u:%02u"), tcRem.bMinutes, tcRem.bSeconds);
            }

            if (fHighPrecision || s.bHighPrecisionTimer) {
                posstr.AppendFormat(_T(".%03d"), int((rtNow / 10000) % 1000));
                durstr.AppendFormat(_T(".%03d"), int((rtDur / 10000) % 1000));
                remstr.AppendFormat(_T(".%03d"), int((rtRem / 10000) % 1000));
            }
        } else if (timeFormat == TIME_FORMAT_FRAME) {
            posstr.Format(_T("%I64d"), rtNow);
            durstr.Format(_T("%I64d"), rtDur);
            remstr.Format(_T("%I64d"), rtRem);
        }

        if (s.fRemainingTime) {
            str = _T("- ") + remstr + _T(" / ") + durstr;
        } else {
            str = posstr + _T(" / ") + durstr;
        }
        if (s.bTimerShowPercentage) {
            str.AppendFormat(_T(" (%.01f%%)"), s.fRemainingTime ? (100.0 * rtRem / rtDur) : (100.0 * rtNow / rtDur));
        }
    } else {
        if (timeFormat == TIME_FORMAT_MEDIA_TIME) {
            DVD_HMSF_TIMECODE tcNow;
            if (fHighPrecision || s.bHighPrecisionTimer) {
                tcNow = RT2HMSF(rtNow);
            } else {
                tcNow = RT2HMS(rtNow);
            }

            if (tcNow.bHours > 0) {
                str.Format(_T("%02u:%02u:%02u"), tcNow.bHours, tcNow.bMinutes, tcNow.bSeconds);
            } else {
                str.Format(_T("%02u:%02u"), tcNow.bMinutes, tcNow.bSeconds);
            }

            if (fHighPrecision || s.bHighPrecisionTimer) {
                str.AppendFormat(_T(".%03d"), int((rtNow / 10000) % 1000));
            }
        } else if (timeFormat == TIME_FORMAT_FRAME) {
            str.Format(_T("%I64d"), rtNow);
        }
    }

    SetStatusTimer(str);
}

void CPlayerStatusBar::ShowTimer(bool fShow)
{
    m_time.ShowWindow(fShow ? SW_SHOW : SW_HIDE);

    Relayout();
}

BEGIN_MESSAGE_MAP(CPlayerStatusBar, CDialogBar)
    ON_WM_ERASEBKGND()
    ON_WM_PAINT()
    ON_WM_SIZE()
    ON_WM_CREATE()
    ON_WM_LBUTTONDOWN()
    ON_WM_SETCURSOR()
    ON_WM_CTLCOLOR()
    ON_WM_CONTEXTMENU()
    ON_NOTIFY_EX(TTN_NEEDTEXT, 0, OnToolTipNotify)
    ON_MPCTHEMECHANGED()
END_MESSAGE_MAP()


// CPlayerStatusBar message handlers

void CPlayerStatusBar::SetMediaTypeIcon()
{
#if 0
    if (m_hIcon) {
        DestroyIcon(m_hIcon);
    }

    m_hIcon = m_typeExt.IsEmpty() ? NULL : LoadIcon(m_typeExt, true, &m_pMainFrame->m_dpi);

    m_type.SetIcon(m_hIcon);

    Relayout();
#endif
}

BOOL CPlayerStatusBar::OnEraseBkgnd(CDC* pDC)
{
    return TRUE;
}

void CPlayerStatusBar::OnPaint()
{
    CPaintDC dc(this); // device context for painting

    for (CWnd* pChild = GetWindow(GW_CHILD); pChild; pChild = pChild->GetNextWindow()) {
        if (!pChild->IsWindowVisible()) {
            continue;
        }

        CRect r;
        pChild->GetClientRect(&r);
        pChild->MapWindowPoints(this, &r);
        dc.ExcludeClipRect(&r);
    }

    CRect r;
    GetClientRect(&r);

    if (m_pMainFrame->m_pLastBar != this || m_pMainFrame->m_fFullScreen) {
        r.InflateRect(0, 0, 0, 1);
    }

    if (m_pMainFrame->m_fFullScreen) {
        r.InflateRect(1, 0, 1, 0);
    }

    if (AppIsThemeLoaded()) {
        dc.FillSolidRect(&r, CMPCTheme::InfoBarBorderColor);
        CRect top(r.left, r.top, r.right, r.top + 1);
        dc.FillSolidRect(&top, CMPCTheme::WindowBGColor);
    } else {
        dc.Draw3dRect(&r, GetSysColor(COLOR_3DSHADOW), GetSysColor(COLOR_3DHILIGHT));
    }

    r.DeflateRect(1, 1);

    dc.FillSolidRect(&r, CMPCTheme::InfoBarBGColor);

    // Only draw the audio-channel bitmap when Relayout actually reserves room for it (Audio Info off).
    // When Audio Info is on, no space is reserved and the time control overlaps this area; drawing the
    // bitmap here would leave it exposed once the time collapses for "time on seekbar" (#3256).
    if (m_bm.m_hObject && !AfxGetAppSettings().bShowAudioFormatInStatusbar) {
        BITMAP bm;
        m_bm.GetBitmap(&bm);
        CDC memdc;
        memdc.CreateCompatibleDC(&dc);
        memdc.SelectObject(&m_bm);
        CRect clientRect;
        GetClientRect(&clientRect);
        CRect statusRect;
        m_status.GetWindowRect(statusRect);
        ScreenToClient(statusRect);
        int x = clientRect.right - GetAudioIconWidth() - 1;
        dc.BitBlt(x, statusRect.CenterPoint().y - bm.bmHeight / 2,
                  bm.bmWidth, bm.bmHeight, &memdc, 0, 0, SRCCOPY);

        CString label = GetAudioChannelsLabel();
        if (!label.IsEmpty()) {
            CFont* pOld = dc.SelectObject(&m_status.GetFont());
            dc.SetTextColor(CMPCTheme::InfoBarTextColor);
            dc.SetBkMode(TRANSPARENT);
            CRect textRect(x + bm.bmWidth, statusRect.top, clientRect.right - 1, statusRect.bottom);
            dc.DrawText(label, textRect, DT_SINGLELINE | DT_NOPREFIX | DT_VCENTER | DT_LEFT);
            dc.SelectObject(pOld);
        }
    }
}

void CPlayerStatusBar::OnSize(UINT nType, int cx, int cy)
{
    CDialogBar::OnSize(nType, cx, cy);

    Invalidate();
    Relayout();
}

void CPlayerStatusBar::OnLButtonDown(UINT nFlags, CPoint point)
{
    CMainFrame* pFrame = ((CMainFrame*)GetParentFrame());
    pFrame->RestoreFocus();

    WINDOWPLACEMENT wp;
    wp.length = sizeof(wp);
    pFrame->GetWindowPlacement(&wp);

    if (m_time_rect.PtInRect(point)) {
        OnTimeDisplayClicked();
    } else if (!pFrame->m_fFullScreen && wp.showCmd != SW_SHOWMAXIMIZED) {
        CRect r;
        GetClientRect(r);
        CPoint p = point;
        ClientToScreen(&point);
        pFrame->PostMessage(WM_NCLBUTTONDOWN,
                            (p.x >= r.Width() - r.Height() && !pFrame->IsCaptionHidden()) ? HTBOTTOMRIGHT :
                            HTCAPTION,
                            MAKELPARAM(point.x, point.y));
    }
}

BOOL CPlayerStatusBar::OnSetCursor(CWnd* pWnd, UINT nHitTest, UINT message)
{
    CMainFrame* pFrame = ((CMainFrame*)GetParentFrame());

    WINDOWPLACEMENT wp;
    wp.length = sizeof(wp);
    pFrame->GetWindowPlacement(&wp);

    CPoint p;
    GetCursorPos(&p);
    ScreenToClient(&p);

    if (m_time_rect.PtInRect(p) && !IsMenu(m_timerMenu)) {
        SetCursor(LoadCursor(nullptr, IDC_HAND));
        return TRUE;
    }

    return CDialogBar::OnSetCursor(pWnd, nHitTest, message);
}

HBRUSH CPlayerStatusBar::OnCtlColor(CDC* pDC, CWnd* pWnd, UINT nCtlColor)
{
    HBRUSH hbr = CDialogBar::OnCtlColor(pDC, pWnd, nCtlColor);

    if (*pWnd == m_type) {
        pDC->SetDCBrushColor(CMPCTheme::InfoBarBGColor);
        hbr = (HBRUSH)GetStockObject(DC_BRUSH);
    }

    // TODO:  Return a different brush if the default is not desired
    return hbr;
}

BOOL CPlayerStatusBar::PreTranslateMessage(MSG* pMsg)
{
    if (AppIsThemeLoaded()) {
        themedToolTip.RelayEvent(pMsg);
    } else {
        m_tooltip.RelayEvent(pMsg);
    }

    return __super::PreTranslateMessage(pMsg);
}

void CPlayerStatusBar::OnTimeDisplayClicked()
{
    CAppSettings& s = AfxGetAppSettings();

    s.fRemainingTime = !s.fRemainingTime;
    m_eventc.FireEvent(MpcEvent::STREAM_POS_UPDATE_REQUEST);
}

void CPlayerStatusBar::OnContextMenu(CWnd* pWnd, CPoint point)
{
    CPoint clientPoint = point;
    ScreenToClient(&clientPoint);
    if (!m_time_rect.PtInRect(clientPoint)) {
        return __super::OnContextMenu(pWnd, point);
    }

    ShowTimerOptionsMenu(this, point);
}

void CPlayerStatusBar::ShowTimerOptionsMenu(CWnd* pOwner, CPoint screenPt)
{
    // Shared by the status-bar time control and the seekbar time section (#3256).
    CAppSettings& s = AfxGetAppSettings();

    enum {
        REMAINING_TIME = 1,
        HIGH_PRECISION,
        SHOW_PERCENTAGE
    };

    m_timerMenu.DestroyMenu();
    m_timerMenu.CreatePopupMenu();
    m_timerMenu.AppendMenu(MF_STRING | MF_ENABLED | (s.fRemainingTime ? MF_CHECKED : MF_UNCHECKED), REMAINING_TIME, ResStr(IDS_TIMER_REMAINING_TIME));
    UINT nFlags = MF_STRING;
    if (m_pMainFrame->IsSubresyncBarVisible()) {
        nFlags |= MF_DISABLED | MF_CHECKED;
    } else {
        nFlags |= MF_ENABLED | (s.bHighPrecisionTimer ? MF_CHECKED : MF_UNCHECKED);
    }
    m_timerMenu.AppendMenu(nFlags, HIGH_PRECISION, ResStr(IDS_TIMER_HIGH_PRECISION));
    m_timerMenu.AppendMenu(MF_STRING | MF_ENABLED | (s.bTimerShowPercentage ? MF_CHECKED : MF_UNCHECKED), SHOW_PERCENTAGE, ResStr(IDS_TIMER_SHOW_PERCENTAGE));

    m_timerMenu.fulfillThemeReqs();
    switch (m_timerMenu.TrackPopupMenu(TPM_LEFTBUTTON | TPM_RETURNCMD, screenPt.x, screenPt.y, pOwner)) {
        case REMAINING_TIME:
            s.fRemainingTime = !s.fRemainingTime;
            m_eventc.FireEvent(MpcEvent::STREAM_POS_UPDATE_REQUEST);
            break;
        case HIGH_PRECISION:
            s.bHighPrecisionTimer = !s.bHighPrecisionTimer;
            m_eventc.FireEvent(MpcEvent::STREAM_POS_UPDATE_REQUEST);
            break;
        case SHOW_PERCENTAGE:
            s.bTimerShowPercentage = !s.bTimerShowPercentage;
            m_eventc.FireEvent(MpcEvent::STREAM_POS_UPDATE_REQUEST);
            break;
    }
    m_timerMenu.DestroyMenu();
}

BOOL CPlayerStatusBar::OnToolTipNotify(UINT id, NMHDR* pNMHDR, LRESULT* pResult)
{
    TOOLTIPTEXT* pTTT = reinterpret_cast<LPTOOLTIPTEXT>(pNMHDR);
    if (!(pTTT->uFlags & TTF_IDISHWND) && pNMHDR->idFrom == AUDIO_ICON_TOOL) {
        // the icon stands in for Audio Info, so its tooltip shows what that would have added
        m_audioTip = m_pMainFrame->GetAudioInfo();
        if (!m_audioTip.IsEmpty()) {
            pTTT->lpszText = const_cast<LPTSTR>(m_audioTip.GetString());
            *pResult = 0;
            return TRUE;
        }
        return FALSE;
    }
    if (pTTT->uFlags & TTF_IDISHWND) {
        UINT_PTR nID = pNMHDR->idFrom;
        if (::GetDlgCtrlID((HWND)nID) == IDC_PLAYERSTATUS) {
            CString type;
            const CString msg = GetStatusMessage();
            if (m_pMainFrame->GetDecoderType(type)
                    && (msg.Find(ResStr(IDS_CONTROLS_PAUSED)) == 0 || msg.Find(ResStr(IDS_CONTROLS_PLAYING)) == 0)) {
                _tcscpy_s(pTTT->szText, type);
                *pResult = 0;
                return TRUE;
            }
        }
    }
    return FALSE;
}
