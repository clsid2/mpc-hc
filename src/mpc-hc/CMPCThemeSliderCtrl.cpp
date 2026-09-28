#include "stdafx.h"
#include "CMPCThemeSliderCtrl.h"
#include "CMPCTheme.h"
#include "mplayerc.h"
#undef SubclassWindow

CMPCThemeSliderCtrl::CMPCThemeSliderCtrl()
    : m_bDrag(false), m_bHover(false), lockToZero(false)
{

}


CMPCThemeSliderCtrl::~CMPCThemeSliderCtrl()
{
}

void CMPCThemeSliderCtrl::PreSubclassWindow()
{
    if (AppNeedsThemedControls()) {
        CToolTipCtrl* pTip = GetToolTips();
        if (nullptr != pTip) {
            themedToolTip.SubclassWindow(pTip->m_hWnd);
        }
    }
}

IMPLEMENT_DYNAMIC(CMPCThemeSliderCtrl, CSliderCtrl)

BEGIN_MESSAGE_MAP(CMPCThemeSliderCtrl, CSliderCtrl)
    ON_NOTIFY_REFLECT(NM_CUSTOMDRAW, &CMPCThemeSliderCtrl::OnNMCustomdraw)
    ON_WM_MOUSEMOVE()
    ON_WM_LBUTTONUP()
    ON_WM_MOUSELEAVE()
    ON_WM_MOUSEWHEEL()
END_MESSAGE_MAP()


void CMPCThemeSliderCtrl::OnNMCustomdraw(NMHDR* pNMHDR, LRESULT* pResult)
{
    LPNMCUSTOMDRAW pNMCD = reinterpret_cast<LPNMCUSTOMDRAW>(pNMHDR);
    LRESULT lr = CDRF_DODEFAULT;

    if (AppNeedsThemedControls()) {
        switch (pNMCD->dwDrawStage) {
            case CDDS_PREPAINT:
                lr = CDRF_NOTIFYITEMDRAW;
                break;

            case CDDS_ITEMPREPAINT:

                if (CMPCTheme::isWindows11Style && (pNMCD->dwItemSpec == TBCD_CHANNEL || pNMCD->dwItemSpec == TBCD_THUMB)) {
                    drawFluentPart(pNMCD);
                    lr = CDRF_SKIPDEFAULT;
                } else if (pNMCD->dwItemSpec == TBCD_CHANNEL) {
                    CDC dc;
                    dc.Attach(pNMCD->hdc);

                    CRect rect;
                    GetClientRect(rect);
                    dc.FillSolidRect(&rect, CMPCTheme::WindowBGColor);

                    CRect channelRect;
                    GetChannelRect(channelRect);
                    CRect thumbRect;
                    GetThumbRect(thumbRect);

                    CRect r;
                    if (TBS_VERT == (GetStyle() & TBS_VERT)) {
                        channelRect = CRect(channelRect.top, channelRect.left, channelRect.bottom, channelRect.right); //for vertical, channelrect returns 90deg rotated dimensions
                        channelRect.NormalizeRect();
                        CopyRect(&pNMCD->rc, CRect(thumbRect.left + 2, channelRect.top, thumbRect.right - 3, channelRect.bottom - 2));
                        CopyRect(r, &pNMCD->rc);
                        r.DeflateRect(6, 0, 6, 0);
                    }
                    else {
                        CopyRect(&pNMCD->rc, CRect(channelRect.left, thumbRect.top + 2, channelRect.right - 2, thumbRect.bottom - 3));
                        CopyRect(r, &pNMCD->rc);
                        r.DeflateRect(0, 6, 0, 6);
                    }


                    dc.FillSolidRect(r, CMPCTheme::SliderChannelColor);
                    CBrush fb;
                    fb.CreateSolidBrush(CMPCTheme::NoBorderColor);
                    dc.FrameRect(r, &fb);
                    fb.DeleteObject();

                    dc.Detach();
                    lr = CDRF_SKIPDEFAULT;
                } else if (pNMCD->dwItemSpec == TBCD_THUMB) {
                    CDC dc;
                    dc.Attach(pNMCD->hdc);
                    pNMCD->rc.bottom--;
                    CRect r(pNMCD->rc);
                    r.DeflateRect(0, 0, 1, 0);

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

                    dc.Detach();
                    lr = CDRF_SKIPDEFAULT;
                }

                break;
        };
    }

    *pResult = lr;
}

//windows 11 style: a thin rounded rail with the value side in the accent, and a round thumb carrying an accent dot.
//the thumb stays inside the thumb rect, since that is all the control repaints when the thumb moves
void CMPCThemeSliderCtrl::drawFluentPart(LPNMCUSTOMDRAW pNMCD)
{
    CDC dc;
    dc.Attach(pNMCD->hdc);
    Gdiplus::Graphics gfx(dc.m_hDC);
    gfx.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias8x8);
    auto gdip = [](COLORREF c) { return Gdiplus::Color(GetRValue(c), GetGValue(c), GetBValue(c)); };
    const bool vert = TBS_VERT == (GetStyle() & TBS_VERT);

    CRect thumbRect;
    GetThumbRect(thumbRect);

    if (pNMCD->dwItemSpec == TBCD_CHANNEL) {
        CRect rect;
        GetClientRect(rect);
        dc.FillSolidRect(&rect, CMPCTheme::WindowBGColor);

        CRect channelRect;
        GetChannelRect(channelRect);
        if (vert) {
            channelRect = CRect(channelRect.top, channelRect.left, channelRect.bottom, channelRect.right); //for vertical, channelrect returns 90deg rotated dimensions
            channelRect.NormalizeRect();
        }
        DpiHelper dpi;
        dpi.Override(GetSafeHwnd());
        const Gdiplus::REAL rail = (Gdiplus::REAL)(std::max)(2, dpi.ScaleY(4));
        //the rail runs along the channel, centred on the thumb across it
        const Gdiplus::REAL cross = vert ? (thumbRect.left + thumbRect.right) / 2.0f : (thumbRect.top + thumbRect.bottom) / 2.0f;
        const Gdiplus::REAL from = (Gdiplus::REAL)(vert ? channelRect.top : channelRect.left);
        const Gdiplus::REAL to = (Gdiplus::REAL)(vert ? channelRect.bottom : channelRect.right);
        const Gdiplus::REAL value = vert ? (thumbRect.top + thumbRect.bottom) / 2.0f : (thumbRect.left + thumbRect.right) / 2.0f;
        auto capsule = [&](Gdiplus::REAL a, Gdiplus::REAL b, COLORREF clr) {
            if (b - a < rail) {
                return;
            }
            Gdiplus::GraphicsPath path;
            if (vert) {
                path.AddArc(cross - rail / 2, a, rail, rail, 180, 180);
                path.AddArc(cross - rail / 2, b - rail, rail, rail, 0, 180);
            } else {
                path.AddArc(a, cross - rail / 2, rail, rail, 90, 180);
                path.AddArc(b - rail, cross - rail / 2, rail, rail, 270, 180);
            }
            path.CloseFigure();
            Gdiplus::SolidBrush brush(gdip(clr));
            gfx.FillPath(&brush, &path);
        };
        capsule(from, to, CMPCTheme::SliderChannelColor);
        if (!vert) { //the value side, as windows 11 fills it; vertical sliders here run either way, so they keep a plain rail
            capsule(from, value, CMPCTheme::CheckboxCheckedColor);
        }
    } else {
        CRect r(thumbRect);
        const Gdiplus::REAL d = (Gdiplus::REAL)(std::min)(r.Width(), r.Height()) - 1.0f;
        const Gdiplus::REAL cx = (r.left + r.right) / 2.0f, cy = (r.top + r.bottom) / 2.0f;
        Gdiplus::SolidBrush disc(gdip(CMPCTheme::SliderThumbColor));
        Gdiplus::Pen outline(gdip(CMPCTheme::SliderThumbBorderColor), 1.0f);
        gfx.FillEllipse(&disc, cx - d / 2, cy - d / 2, d, d);
        gfx.DrawEllipse(&outline, cx - d / 2, cy - d / 2, d, d);
        //fluent grows the dot on hover and shrinks it while dragging
        const Gdiplus::REAL dot = d * (m_bDrag ? 0.42f : m_bHover ? 0.62f : 0.52f);
        Gdiplus::SolidBrush accent(gdip(CMPCTheme::CheckboxCheckedColor));
        gfx.FillEllipse(&accent, cx - dot / 2, cy - dot / 2, dot, dot);
    }
    dc.Detach();
}

void CMPCThemeSliderCtrl::invalidateThumb()
{
    int max = GetRangeMax();
    SetRangeMax(max, TRUE);
}


void CMPCThemeSliderCtrl::checkHover(CPoint point)
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

void CMPCThemeSliderCtrl::OnMouseMove(UINT nFlags, CPoint point)
{
    checkHover(point);
    CSliderCtrl::OnMouseMove(nFlags, point);
}


void CMPCThemeSliderCtrl::OnLButtonUp(UINT nFlags, CPoint point)
{
    m_bDrag = false;
    invalidateThumb();
    checkHover(point);
    CSliderCtrl::OnLButtonUp(nFlags, point);
}


void CMPCThemeSliderCtrl::OnMouseLeave()
{
    checkHover(CPoint(-1, -1));
    CSliderCtrl::OnMouseLeave();
}


void CMPCThemeSliderCtrl::SendScrollMsg(WORD wSBcode, WORD wHiWPARAM /*= 0*/) {
    ASSERT(::IsWindow(m_hWnd));
    CWnd* m_pParent = GetParent();
    if (m_pParent && ::IsWindow(m_pParent->m_hWnd)) {
        bool isVert = GetStyle() & TBS_VERT;
        m_pParent->SendMessage(isVert ? WM_VSCROLL : WM_HSCROLL, MAKELONG(wSBcode, wHiWPARAM), (LPARAM)m_hWnd);
    }
}

BOOL CMPCThemeSliderCtrl::OnMouseWheel(UINT nFlags, short zDelta, CPoint pt) {
    if (lockToZero) {
        WORD wSBcode = 0xFFFF;
        int dir = 1;
        if (zDelta >= WHEEL_DELTA) {
            wSBcode = SB_LINEUP;
        } else if (zDelta <= -WHEEL_DELTA) {
            wSBcode = SB_LINEDOWN;
            dir = -1;
            zDelta = -zDelta;
        }
        if (wSBcode != 0xFFFF) {
            int scrollIncrememt = (GetRangeMax() - GetRangeMin()) / 50;
            do {
                SendScrollMsg(wSBcode);
                int curPos = GetPos();
                int newPos = curPos + dir * scrollIncrememt;
                if (abs(newPos) < abs(scrollIncrememt) && SGN(newPos) != SGN(curPos)) { //we crossed zero and are in between +/- scrollIncrement
                    newPos = 0;
                }
                SetPos(newPos);
            } while ((zDelta -= WHEEL_DELTA) >= WHEEL_DELTA);
            SendScrollMsg(SB_ENDSCROLL);
        }

        return 1;	// Message was processed. (was 0, but per https://msdn.microsoft.com/en-us/data/eff58fe7(v=vs.85) should be 1 if scrolling enabled
    } else {
        return CSliderCtrl::OnMouseWheel(nFlags, zDelta, pt);
    }
}
