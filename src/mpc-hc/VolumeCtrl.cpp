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

#include "stdafx.h"
#include "mplayerc.h"
#include "VolumeCtrl.h"
#include "AppSettings.h"
#include "CMPCTheme.h"
#include "CMPCThemeUtil.h"
#include "MainFrm.h"
#undef SubclassWindow


// CVolumeCtrl

IMPLEMENT_DYNAMIC(CVolumeCtrl, CSliderCtrl)
CVolumeCtrl::CVolumeCtrl(bool fSelfDrawn)
    : m_fSelfDrawn(fSelfDrawn)
    , m_bDrag(false)
    , m_bHover(false)
    , modernStyle(AfxGetAppSettings().bMPCTheme)
    , showPercentage(AfxGetAppSettings().bShowVolumePercentage)
{
}

CVolumeCtrl::~CVolumeCtrl()
{
}

bool CVolumeCtrl::Create(CWnd* pParentWnd)
{
    DWORD tooltipStyle = showPercentage && AppIsThemeLoaded() ? 0 : TBS_TOOLTIPS;
    if (!CSliderCtrl::Create(WS_CHILD | WS_VISIBLE | TBS_NOTICKS | TBS_HORZ | tooltipStyle, CRect(0, 0, 0, 0), pParentWnd, IDC_SLIDER1)) {
        return false;
    }

    const CAppSettings& s = AfxGetAppSettings();
    EnableToolTips(TRUE);
    SetRange(0, 100);
    SetPos(s.nVolume);
    SetPageSize(s.nVolumeStep);
    SetLineSize(0);

    if (AppIsThemeLoaded()) {
        CToolTipCtrl* pTip = GetToolTips();
        if (NULL != pTip) {
            themedToolTip.SubclassWindow(pTip->m_hWnd);
        }
    }

    return true;
}

void CVolumeCtrl::SetPosInternal(int pos)
{
    SetPos(pos);
    const int currentPos = GetPos();
    // Post even when the clamped position is unchanged (e.g. Volume Up at 100):
    // the frame handler owes the user OSD feedback on every press and dedups the
    // renderer/API work itself.
    GetParent()->PostMessage(WM_HSCROLL, MAKEWPARAM(static_cast<WORD>(currentPos), SB_THUMBPOSITION), reinterpret_cast<LPARAM>(m_hWnd)); // this will be reflected back on us
    POINT p;
    ::GetCursorPos(&p);
    ScreenToClient(&p);
    checkHover(p);
}

void CVolumeCtrl::IncreaseVolume()
{
    // align volume up to step. recommend using steps 1, 2, 5 and 10
    SetPosInternal(GetPos() + GetPageSize() - GetPos() % GetPageSize());

}

void CVolumeCtrl::DecreaseVolume()
{
    // align volume down to step. recommend using steps 1, 2, 5 and 10
    int m = GetPos() % GetPageSize();
    SetPosInternal(GetPos() - (m ? m : GetPageSize()));
}

BEGIN_MESSAGE_MAP(CVolumeCtrl, CSliderCtrl)
    ON_NOTIFY_REFLECT(NM_CUSTOMDRAW, OnNMCustomdraw)
    ON_WM_LBUTTONDOWN()
    ON_WM_SETFOCUS()
    ON_WM_HSCROLL_REFLECT()
    ON_WM_SETCURSOR()
    ON_NOTIFY_EX(TTN_NEEDTEXT, 0, OnToolTipNotify)
    ON_WM_MOUSEWHEEL()
    ON_WM_MOUSEMOVE()
    ON_WM_LBUTTONUP()
    ON_WM_MOUSELEAVE()
    ON_WM_PAINT()
    ON_WM_ERASEBKGND()
END_MESSAGE_MAP()

// CVolumeCtrl message handlers

void CVolumeCtrl::getCustomChannelRect(LPRECT rc)
{
    if (AppIsThemeLoaded()) {
        CRect channelRect;
        GetClientRect(channelRect);
        CopyRect(rc, CRect(channelRect.left, channelRect.top, channelRect.right - AfxGetMainFrame()->m_dpi.ScaleFloorX(2), channelRect.bottom));
    } else {
        CRect channelRect;
        GetChannelRect(channelRect);
        CRect thumbRect;
        GetThumbRect(thumbRect);

        CopyRect(rc, CRect(channelRect.left, thumbRect.top + 2, channelRect.right - 2, thumbRect.bottom - 2));
    }
}

void CVolumeCtrl::OnNMCustomdraw(NMHDR* pNMHDR, LRESULT* pResult)
{
    LPNMCUSTOMDRAW pNMCD = reinterpret_cast<LPNMCUSTOMDRAW>(pNMHDR);

    LRESULT lr = CDRF_DODEFAULT;

    bool usetheme = AppIsThemeLoaded();

    if (m_fSelfDrawn)
        switch (pNMCD->dwDrawStage) {
            case CDDS_PREPAINT:
                lr = CDRF_NOTIFYITEMDRAW;
                break;

            case CDDS_ITEMPREPAINT:

                if (pNMCD->dwItemSpec == TBCD_CHANNEL) {
                    CDC dc;
                    dc.Attach(pNMCD->hdc);

                    if (usetheme) {
                        CRect rect;
                        GetClientRect(rect);
                        dc.FillSolidRect(&rect, CMPCTheme::PlayerBGColor);
                    }

                    getCustomChannelRect(&pNMCD->rc);

                    if (usetheme) {
                        DpiHelper dpiWindow;
                        dpiWindow.Override(GetSafeHwnd());

                        CRect r(pNMCD->rc);
                        r.DeflateRect(0, dpiWindow.ScaleFloorY(6), 0, dpiWindow.ScaleFloorY(6));
                        dc.FillSolidRect(r, CMPCTheme::ScrollBGColor);
                        CBrush fb;
                        fb.CreateSolidBrush(CMPCTheme::NoBorderColor);
                        dc.FrameRect(r, &fb);
                        fb.DeleteObject();
                    } else {
                        CPen shadow;
                        CPen light;
                        shadow.CreatePen(PS_SOLID, 1, GetSysColor(COLOR_3DSHADOW));
                        light.CreatePen(PS_SOLID, 1, GetSysColor(COLOR_3DHILIGHT));
                        CPen* old = dc.SelectObject(&light);
                        dc.MoveTo(pNMCD->rc.right, pNMCD->rc.top);
                        dc.LineTo(pNMCD->rc.right, pNMCD->rc.bottom);
                        dc.LineTo(pNMCD->rc.left, pNMCD->rc.bottom);
                        dc.SelectObject(&shadow);
                        dc.LineTo(pNMCD->rc.right, pNMCD->rc.top);
                        dc.SelectObject(old);
                        shadow.DeleteObject();
                        light.DeleteObject();
                    }

                    dc.Detach();
                    lr = CDRF_SKIPDEFAULT;
                } else if (pNMCD->dwItemSpec == TBCD_THUMB) {
                    CDC dc;
                    dc.Attach(pNMCD->hdc);
                    pNMCD->rc.bottom--;
                    CRect r(pNMCD->rc);
                    r.DeflateRect(0, 0, 1, 0);

                    COLORREF shadow = GetSysColor(COLOR_3DSHADOW);
                    COLORREF light = GetSysColor(COLOR_3DHILIGHT);
                    if (usetheme) {
                        CBrush fb;
                        if (m_bDrag) {
                            dc.FillSolidRect(r, CMPCTheme::ScrollThumbDragColor);
                        } else if (m_bHover) {
                            dc.FillSolidRect(r, CMPCTheme::ScrollThumbHoverColor);
                        } else {
                            dc.FillSolidRect(r, CMPCTheme::ScrollThumbColor);
                        }
                        fb.CreateSolidBrush(CMPCTheme::NoBorderColor);
                        dc.FrameRect(r, &fb);
                        fb.DeleteObject();
                    } else {
                        dc.Draw3dRect(&r, light, 0);
                        r.DeflateRect(0, 0, 1, 1);
                        dc.Draw3dRect(&r, light, shadow);
                        r.DeflateRect(1, 1, 1, 1);
                        dc.FillSolidRect(&r, GetSysColor(COLOR_BTNFACE));
                        dc.SetPixel(r.left + 7, r.top - 1, GetSysColor(COLOR_BTNFACE));
                    }

                    dc.Detach();
                    lr = CDRF_SKIPDEFAULT;
                }

                break;
        };

    pNMCD->uItemState &= ~CDIS_FOCUS;

    *pResult = lr;
}

void CVolumeCtrl::OnLButtonDown(UINT nFlags, CPoint point)
{
    CRect r;
    GetChannelRect(&r);

    if (r.left >= r.right) {
        return;
    }

    int start, stop;
    GetRange(start, stop);

    if (!(AppIsThemeLoaded() && modernStyle)) {
        r.left += 3;
        r.right -= 4;
    }

    if (point.x < r.left) {
        SetPosInternal(start);
    } else if (point.x >= r.right) {
        SetPosInternal(stop);
    } else {
        int w = r.right - r.left;
        if (start < stop) {
            if (!(AppIsThemeLoaded() && modernStyle)) {
                SetPosInternal(start + ((stop - start) * (point.x - r.left) + (w / 2)) / w);
            } else {
                SetPosInternal(start + lround((stop - start) * float(point.x - r.left) / w));
            }
        }
    }
    m_bDrag = true;
    if (AppIsThemeLoaded() && modernStyle) {
        if (themedToolTip.m_hWnd) {
            TOOLINFO ti = { sizeof(TOOLINFO) };
            ti.uFlags = TTF_TRACK | TTF_IDISHWND | TTF_ABSOLUTE;
            ti.hwnd = m_hWnd;
            ti.uId = (UINT_PTR)m_hWnd;
            ti.hinst = AfxGetInstanceHandle();
            ti.lpszText = LPSTR_TEXTCALLBACK;

            themedToolTip.SendMessage(TTM_TRACKACTIVATE, TRUE, (LPARAM)&ti);
        }

        updateModernVolCtrl(point);
        SetCapture();
    } else {
        invalidateThumb();
        CSliderCtrl::OnLButtonDown(nFlags, point);
    }
}

void CVolumeCtrl::OnSetFocus(CWnd* pOldWnd)
{
    CSliderCtrl::OnSetFocus(pOldWnd);

    AfxGetMainWnd()->SetFocus(); // don't focus on us, parents will take care of our positioning
}

void CVolumeCtrl::HScroll(UINT nSBCode, UINT nPos)
{
    auto &s = AfxGetAppSettings();
    auto oldVolume = s.nVolume;
    s.nVolume = GetPos();

    CFrameWnd* pFrame = GetParentFrame();
    if (pFrame && pFrame != GetParent()) {
        // Always forward so the frame shows the volume OSD on every press, including when
        // the value is clamped and unchanged (e.g. Volume Up at 100); the frame dedups the
        // renderer/LCD/API work itself. Only the text redraw is conditioned on a change.
        pFrame->PostMessage(WM_HSCROLL, MAKEWPARAM(static_cast<WORD>(nPos), static_cast<WORD>(nSBCode)), reinterpret_cast<LPARAM>(m_hWnd));
        if (s.nVolume != oldVolume) {
            CRect r;
            getCustomChannelRect(r);
            InvalidateRect(r); //needed to redraw the volume text
        }
    }
}

BOOL CVolumeCtrl::OnSetCursor(CWnd* pWnd, UINT nHitTest, UINT message)
{
    ::SetCursor(AfxGetApp()->LoadStandardCursor(IDC_HAND));
    return TRUE;
}

BOOL CVolumeCtrl::OnToolTipNotify(UINT id, NMHDR* pNMHDR, LRESULT* pResult)
{
    TOOLTIPTEXT* pTTT = reinterpret_cast<LPTOOLTIPTEXT>(pNMHDR);
    CString str;
    str.Format(IDS_VOLUME, GetPos());
    _tcscpy_s(pTTT->szText, str);
    pTTT->hinst = nullptr;

    *pResult = 0;

    return TRUE;
}

BOOL CVolumeCtrl::OnMouseWheel(UINT nFlags, short zDelta, CPoint point)
{
    if (zDelta > 0) {
        IncreaseVolume();
    } else if (zDelta < 0) {
        DecreaseVolume();
    } else {
        return FALSE;
    }
    return TRUE;
}

void CVolumeCtrl::invalidateThumb()
{
    if (!(AppIsThemeLoaded() && modernStyle)) {
        SetRangeMax(100, TRUE);
    }
}


void CVolumeCtrl::checkHover(CPoint point)
{
    CRect thumbRect;
    GetThumbRect(thumbRect);
    bool oldHover = m_bHover;
    m_bHover = false;
    if (thumbRect.PtInRect(point)) {
        m_bHover = true;
    }

    if (m_bHover != oldHover) {
        invalidateThumb();
    }
}

void CVolumeCtrl::updateModernVolCtrl(CPoint point)
{
    //CSliderCtrl::OnMouseMove yields bad results due to assumption of thumb width
    //we must do all position calculation ourselves, and send correct position to tooltip

    CRect r;
    GetChannelRect(&r);

    int start, stop;
    GetRange(start, stop);
    int useX;
    if (point.x < r.left) {
        SetPosInternal(start);
        useX = r.left;
    } else if (point.x >= r.right) {
        SetPosInternal(stop);
        useX = r.right;
    } else {
        int w = r.right - r.left;
        if (start < stop) {
            SetPosInternal(start + lround((stop - start) * float(point.x - r.left) / w));
        }
        useX = point.x;
    }
    POINT p = { useX, point.y };
    ClientToScreen(&p);
    CRect ttRect;
    if (themedToolTip.m_hWnd) {
        CRect cr = r;
        ClientToScreen(cr);
        themedToolTip.GetWindowRect(ttRect);
        p.y = cr.top - ttRect.Height();
        themedToolTip.SendMessage(TTM_TRACKPOSITION, 0, MAKELPARAM(p.x, p.y));
    }

    RECT ur;
    getCustomChannelRect(&ur);
    RedrawWindow(&ur, nullptr, RDW_INVALIDATE); //we must redraw the whole channel with the modern volume ctrl. by default only areas where thumb has been are invalidated
}


void CVolumeCtrl::OnMouseMove(UINT nFlags, CPoint point)
{
    checkHover(point);

    if (AppIsThemeLoaded() && modernStyle && m_bDrag) {
        updateModernVolCtrl(point);
    } else {
        CSliderCtrl::OnMouseMove(nFlags, point);
    }
}


void CVolumeCtrl::OnLButtonUp(UINT nFlags, CPoint point)
{
    if (AppIsThemeLoaded() && modernStyle) {
        if (m_bDrag) {
            ReleaseCapture();
        }
        m_bDrag = false;
        if (themedToolTip.m_hWnd) {
            themedToolTip.SendMessage(TTM_TRACKACTIVATE, FALSE, 0);
        }
    } else {
        m_bDrag = false;
        invalidateThumb();
        checkHover(point);
        CSliderCtrl::OnLButtonUp(nFlags, point);
    }
}


void CVolumeCtrl::OnMouseLeave()
{
    checkHover(CPoint(-1 - 1));
    CSliderCtrl::OnMouseLeave();
}


void CVolumeCtrl::OnPaint() {
    if (m_fSelfDrawn && AppIsThemeLoaded() && modernStyle) {
        DpiHelper dpiWindow;
        dpiWindow.Override(GetSafeHwnd());

        CPaintDC dc(this);
        CRect r, cr;
        GetClientRect(cr);

        CDC dcMem;
        CBitmap bmMem;
        CRect memRect = { 0, 0, cr.right, cr.bottom };
        CMPCThemeUtil::initMemDC(&dc, dcMem, bmMem, memRect);

        dcMem.FillSolidRect(&memRect, CMPCTheme::PlayerBGColor);
        getCustomChannelRect(r);
        //r.DeflateRect(0, dpiWindow.ScaleFloorY(3), 0, dpiWindow.ScaleFloorY(2));
        r.OffsetRect(-cr.TopLeft());

        CRect filledRect, unfilledRect;
        filledRect = r;
        filledRect.right = r.left + lround(r.Width() * float(GetPos()) / 100);
        dcMem.FillSolidRect(&filledRect, CMPCTheme::ScrollProgressColor);

        if (filledRect.right < r.right) { //do not fill bg if already full
            unfilledRect = r;
            unfilledRect.left = filledRect.right;
            dcMem.FillSolidRect(&unfilledRect, CMPCTheme::ScrollBGColor);
        }

        CBrush fb;
        fb.CreateSolidBrush(CMPCTheme::NoBorderColor);
        dcMem.FrameRect(r, &fb);
        fb.DeleteObject();

        if (showPercentage) {
            dcMem.SetTextColor(CMPCTheme::TextFGColor);
            CFont f;
            LOGFONT lf = { 0 };
            lf.lfHeight = r.Height();
            lf.lfQuality = CLEARTYPE_QUALITY;
            wcscpy_s(lf.lfFaceName, L"Calibri");
            f.CreateFontIndirectW(&lf);
            CFont* oldFont = (CFont*)dcMem.SelectObject(&f);
            int oldMode = dcMem.SetBkMode(TRANSPARENT);
            CStringW str;
            str.Format(IDS_VOLUME, GetPos());
            dcMem.DrawTextW(str, r, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS | DT_NOPREFIX);
            dcMem.SelectObject(oldFont);
            dcMem.SetBkMode(oldMode);
        }

        CMPCThemeUtil::flushMemDC(&dc, dcMem, memRect);
    } else {
        __super::OnPaint(); //can trigger OnNMCustomdraw
    }
}

BOOL CVolumeCtrl::OnEraseBkgnd(CDC* pDC) {
    if (m_fSelfDrawn && AppIsThemeLoaded() && modernStyle) {
        return TRUE;
    } else {
        return CSliderCtrl::OnEraseBkgnd(pDC);
    }
}
