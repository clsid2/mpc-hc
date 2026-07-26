#include "stdafx.h"
#include "DpiHelper.h"
#include "CMPCThemeSpinButtonCtrl.h"
#include "CMPCTheme.h"
#include "CMPCThemeEdit.h"
#include "mplayerc.h"

CMPCThemeSpinButtonCtrl::CMPCThemeSpinButtonCtrl()
{
}


CMPCThemeSpinButtonCtrl::~CMPCThemeSpinButtonCtrl()
{
}

IMPLEMENT_DYNAMIC(CMPCThemeSpinButtonCtrl, CSpinButtonCtrl)
BEGIN_MESSAGE_MAP(CMPCThemeSpinButtonCtrl, CSpinButtonCtrl)
    ON_WM_PAINT()
    ON_WM_MOUSEMOVE()
    ON_WM_LBUTTONDOWN()
    ON_WM_LBUTTONUP()
    ON_WM_ERASEBKGND()
END_MESSAGE_MAP()


void CMPCThemeSpinButtonCtrl::drawSpinArrow(CDC& dc, COLORREF arrowClr, CRect arrowRect, arrowOrientation orientation)
{
    DpiHelper dpiWindow;
    dpiWindow.Override(GetSafeHwnd());

    Gdiplus::Graphics gfx(dc.m_hDC);
    Gdiplus::Color clr;
    clr.SetFromCOLORREF(arrowClr);

    int dpi = dpiWindow.DPIX();
    float steps;

    if (dpi < 120) {
        steps = 2;
    } else if (dpi < 144) {
        steps = 3;
    } else if (dpi < 168) {
        steps = 4;
    } else if (dpi < 192) {
        steps = 4;
    } else {
        steps = 4.5;
    }

    int xPos;
    int yPos;
    int xsign, ysign;
    switch (orientation) {
        case arrowLeft:
            xPos = arrowRect.right - (arrowRect.Width() - (steps)) / 2;
            yPos = arrowRect.top + (arrowRect.Height() - (steps * 2 + 1)) / 2;
            xsign = -1;
            ysign = 1;
            break;
        case arrowRight:
            xPos = arrowRect.left + (arrowRect.Width() - (steps + 1)) / 2;
            yPos = arrowRect.top + (arrowRect.Height() - (steps * 2 + 1)) / 2;
            xsign = 1;
            ysign = 1;
            break;
        case arrowTop:
            xPos = arrowRect.left + (arrowRect.Width() - (steps * 2 + 1)) / 2;
            yPos = arrowRect.bottom - (arrowRect.Height() - (steps)) / 2;
            xsign = 1;
            ysign = -1;
            break;
        case arrowBottom:
        default:
            xPos = arrowRect.left + (arrowRect.Width() - (steps * 2 + 1)) / 2;
            yPos = arrowRect.top + (arrowRect.Height() - (steps + 1)) / 2;
            xsign = 1;
            ysign = 1;
            break;
    }

    Gdiplus::PointF vertices[3];

    if (orientation == arrowLeft || orientation == arrowRight) {
        vertices[0] = Gdiplus::PointF(xPos, yPos);
        vertices[1] = Gdiplus::PointF(xPos + steps * xsign, yPos + steps * ysign);
        vertices[2] = Gdiplus::PointF(xPos, yPos + steps * 2 * ysign);
    } else {
        vertices[0] = Gdiplus::PointF(xPos, yPos);
        vertices[1] = Gdiplus::PointF(xPos + steps * xsign, yPos + steps * ysign);
        vertices[2] = Gdiplus::PointF(xPos + steps * 2 * xsign, yPos);
    }

    Gdiplus::Pen pen(clr, 1);

    if (floor(steps) != steps) {
        gfx.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
    } else {
        gfx.SetSmoothingMode(Gdiplus::SmoothingModeNone);
    }

    gfx.DrawPolygon(&pen, vertices, 3);

    Gdiplus::SolidBrush brush(clr);
    gfx.FillPolygon(&brush, vertices, 3);
}

void CMPCThemeSpinButtonCtrl::OnPaint()
{
    if (AppNeedsThemedControls()) {
        CWnd* buddy = GetBuddy();
        bool hasBuddy = false;
        CMPCThemeEdit* buddyEdit;
        if (nullptr != buddy && nullptr != (buddyEdit = DYNAMIC_DOWNCAST(CMPCThemeEdit, buddy))) {
            buddyEdit->setBuddy(this); //we need to know about the buddy spin ctrl to clip it in ncpaint :-/
            hasBuddy = true;
        }

        CPaintDC dc(this);
        CRect   rectItem;
        GetClientRect(rectItem);

        COLORREF bgClr = CMPCTheme::ContentBGColor;


        CBrush borderBrush(CMPCTheme::EditBorderColor);
        CBrush butBorderBrush(CMPCTheme::ButtonBorderInnerColor);

        dc.FillSolidRect(rectItem, bgClr);

        bool horz = 0 != (GetStyle() & UDS_HORZ);
        if (horz) {
            if (hasBuddy) {
                dc.ExcludeClipRect(1, 0, rectItem.Width() - 1, 1); //don't get top edge of rect
                dc.FrameRect(rectItem, &borderBrush);
            }
        } else {
            if (hasBuddy) {
                dc.ExcludeClipRect(0, 1, 1, rectItem.Height() - 1); //don't get left edge of rect
                dc.FrameRect(rectItem, &borderBrush);
            }
        }

        int buddySpacing = hasBuddy ? 1 : 0;
        for (int firstOrSecond = 0; firstOrSecond < 2; firstOrSecond++) {
            CRect butRect = rectItem;
            if (horz) {
                butRect.DeflateRect(1, 1, 1, 1 + buddySpacing);
                if (0 == firstOrSecond) {//left or top
                    butRect.right -= butRect.Width() / 2;
                } else {
                    butRect.left += butRect.Width() / 2;
                }
                butRect.DeflateRect(1, 0);
            } else {
                butRect.DeflateRect(1, 1, 1 + buddySpacing, 1);
                if (0 == firstOrSecond) {//left or top
                    butRect.bottom -= butRect.Height() / 2;
                } else {
                    butRect.top += butRect.Height() / 2;
                }
                butRect.DeflateRect(0, 1);
            }


            if (butRect.PtInRect(downPos)) {
                bgClr = CMPCTheme::ButtonFillSelectedColor;
            } else {
                bgClr = CMPCTheme::ButtonFillColor;
            }

            dc.FillSolidRect(butRect, bgClr);
            dc.FrameRect(butRect, &butBorderBrush);

            COLORREF arrowColor;
            if (IsWindowEnabled()) {
                arrowColor = CMPCTheme::TextFGColor;
            } else {
                arrowColor = CMPCTheme::ButtonDisabledFGColor;
            }

            if (horz) {
                if (0 == firstOrSecond) { //left
                    drawSpinArrow(dc, arrowColor, butRect, arrowOrientation::arrowLeft);
                } else {
                    drawSpinArrow(dc, arrowColor, butRect, arrowOrientation::arrowRight);
                }
            } else {
                if (0 == firstOrSecond) { //top
                    drawSpinArrow(dc, arrowColor, butRect, arrowOrientation::arrowTop);
                } else {
                    drawSpinArrow(dc, arrowColor, butRect, arrowOrientation::arrowBottom);
                }
            }
        }

    } else {
        __super::OnPaint();
    }

}


void CMPCThemeSpinButtonCtrl::OnMouseMove(UINT nFlags, CPoint point)
{
    CSpinButtonCtrl::OnMouseMove(nFlags, point);
    if (MK_LBUTTON & nFlags) {
        downPos = point;
    } else {
        downPos = CPoint(-1, -1);
    }
}


void CMPCThemeSpinButtonCtrl::OnLButtonDown(UINT nFlags, CPoint point)
{
    CSpinButtonCtrl::OnLButtonDown(nFlags, point);
    downPos = point;
}


void CMPCThemeSpinButtonCtrl::OnLButtonUp(UINT nFlags, CPoint point)
{
    CSpinButtonCtrl::OnLButtonUp(nFlags, point);
    downPos = CPoint(-1, -1);
}


BOOL CMPCThemeSpinButtonCtrl::OnEraseBkgnd(CDC* pDC)
{
    if (AppNeedsThemedControls()) {
        return TRUE;
    } else {
        return __super::OnEraseBkgnd(pDC);
    }
}
