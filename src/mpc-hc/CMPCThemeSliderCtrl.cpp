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

                if (pNMCD->dwItemSpec == TBCD_CHANNEL) {
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
