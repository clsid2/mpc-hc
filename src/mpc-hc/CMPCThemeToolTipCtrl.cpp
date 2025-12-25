#include "stdafx.h"
#include "CMPCThemeToolTipCtrl.h"
#include "CMPCTheme.h"
#include "CMPCThemeUtil.h"
#include <afxglobals.h>
#include "mplayerc.h"


CMPCThemeToolTipCtrl::CMPCThemeToolTipCtrl()
{
}


CMPCThemeToolTipCtrl::~CMPCThemeToolTipCtrl()
{
}

IMPLEMENT_DYNAMIC(CMPCThemeToolTipCtrl, CToolTipCtrl)
BEGIN_MESSAGE_MAP(CMPCThemeToolTipCtrl, CToolTipCtrl)
    ON_WM_PAINT()
    ON_WM_ERASEBKGND()
    ON_WM_CREATE()
    ON_WM_WINDOWPOSCHANGING()
END_MESSAGE_MAP()

void CMPCThemeToolTipCtrl::drawText(CDC& dc, CMPCThemeToolTipCtrl* tt, CRect& rect, bool calcRect)
{
    CFont* font = tt->GetFont();
    CFont* pOldFont = dc.SelectObject(font);

    CString text;
    tt->GetWindowText(text);
    int maxWidth = tt->GetMaxTipWidth();
    int calcStyle = 0;
    if (calcRect) {
        calcStyle = DT_CALCRECT;
    }
    rect.DeflateRect(6, 2);
    if (maxWidth == -1) {
        if (calcRect) {
            dc.DrawTextW(text, rect, DT_LEFT | DT_SINGLELINE | DT_NOPREFIX | calcStyle);
        } else {
            dc.DrawTextW(text, rect, DT_VCENTER | DT_CENTER | DT_SINGLELINE | DT_NOPREFIX);
        }
    } else {
        dc.DrawTextW(text, rect, DT_LEFT | DT_WORDBREAK | DT_NOPREFIX | calcStyle);
    }
    rect.InflateRect(6, 2); //when calculating, put it back
    if (!calcStyle) {
        tt->lastDrawRect = rect;
    }

    dc.SelectObject(pOldFont);
}

void CMPCThemeToolTipCtrl::paintTT(CDC& dc, CMPCThemeToolTipCtrl* tt)
{
    CRect r;
    tt->GetClientRect(r);

    dc.FillSolidRect(r, CMPCTheme::MenuBGColor);

    CBrush fb;
    fb.CreateSolidBrush(CMPCTheme::TooltipBorderColor);
    dc.FrameRect(r, &fb);
    fb.DeleteObject();
    COLORREF oldClr = dc.SetTextColor(CMPCTheme::TextFGColor);
    drawText(dc, tt, r, false);
    dc.SetTextColor(oldClr);
}

void CMPCThemeToolTipCtrl::OnPaint()
{
    if (AppIsThemeLoaded()) {
        CPaintDC dc(this);
        paintTT(dc, this);
    } else {
        __super::OnPaint();
    }
}


BOOL CMPCThemeToolTipCtrl::OnEraseBkgnd(CDC* pDC)
{
    if (AppIsThemeLoaded()) {
        return TRUE;
    } else {
        return __super::OnEraseBkgnd(pDC);
    }
}


int CMPCThemeToolTipCtrl::OnCreate(LPCREATESTRUCT lpCreateStruct)
{
    if (CToolTipCtrl::OnCreate(lpCreateStruct) == -1) {
        return -1;
    }

    return 0;
}

void CMPCThemeToolTipCtrl::RedrawIfVisible() {
    if (::IsWindow(m_hWnd) && IsWindowVisible()) {
        CWindowDC dc(this);
        CRect wr;
        drawText(dc, this, wr, true);
        if (wr != lastDrawRect) {
            Update();
        } else {
            RedrawWindow();
        }
    }
}

void CMPCThemeToolTipCtrl::OnWindowPosChanging(WINDOWPOS* lpwndpos)
{
    CToolTipCtrl::OnWindowPosChanging(lpwndpos);
    if (AppIsThemeLoaded()) {
        //hack to make it fit if fonts differ from parent. can be manually avoided
        //if the parent widget is set to same font (see CMPCThemePlayerListCtrl using MessageFont now)
        CString text;
        GetWindowText(text);
        if (text.GetLength() > 0 && GetMaxTipWidth() == -1) {
            CWindowDC dc(this);

            CRect cr;
            drawText(dc, this, cr, true);//calculate crect required to fit the text

            lpwndpos->cx = cr.Width();
            lpwndpos->cy = cr.Height();
        }
    }
}

//tooltip rules for how to hover on parent window
//by default tooltipctrl will simply hover at the mouse position,
//unlike the tooltips created with EnableTooltips which center below the window
//(in all cases tested-slider, combobox, edit)
void CMPCThemeToolTipCtrl::SetHoverPosition(CWnd* parent) {
    if (IsWindow(parent->GetSafeHwnd())) {
        CRect parentRect, ttRect;
        parent->GetWindowRect(parentRect);
        GetWindowRect(ttRect);
        int centerOffset = (parentRect.left + parentRect.right - ttRect.left - ttRect.right) / 2;
        ttRect.right += centerOffset;
        ttRect.left += centerOffset;
        ttRect.bottom += (parentRect.bottom - ttRect.top);
        ttRect.top = parentRect.bottom;
        MoveWindow(ttRect);
    }
}
