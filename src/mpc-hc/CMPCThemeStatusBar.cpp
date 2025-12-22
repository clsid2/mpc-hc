#include "stdafx.h"
#include "CMPCThemeStatusBar.h"
#include "CMPCTheme.h"
#include "CMPCThemeUtil.h"
#include "mplayerc.h"
#include "DpiHelper.h"

CMPCThemeStatusBar::CMPCThemeStatusBar()
{
}


CMPCThemeStatusBar::~CMPCThemeStatusBar()
{
}

void CMPCThemeStatusBar::PreSubclassWindow()
{
    if (AppIsThemeLoaded()) {
        ModifyStyleEx(WS_BORDER, WS_EX_STATICEDGE, 0);
    } else {
        __super::PreSubclassWindow();
    }
}


BEGIN_MESSAGE_MAP(CMPCThemeStatusBar, CStatusBar)
    ON_WM_NCPAINT()
    ON_WM_ERASEBKGND()
END_MESSAGE_MAP()

void CMPCThemeStatusBar::SetText(LPCTSTR lpszText, int nPane, int nType)
{
    CStatusBarCtrl& ctrl = GetStatusBarCtrl();

    if (AppIsThemeLoaded()) {
        ctrl.SetText(_T(""), nPane, SBT_OWNERDRAW);
        texts[nPane] = lpszText;
        Invalidate();
    } else {
        ctrl.SetText(lpszText, nPane, nType);
    }
}

BOOL CMPCThemeStatusBar::SetParts(int nParts, int* pWidths)
{
    CStatusBarCtrl& ctrl = GetStatusBarCtrl();
    numParts = nParts;
    return ctrl.SetParts(nParts, pWidths);
}

int CMPCThemeStatusBar::GetParts(int nParts, int* pParts)
{
    CStatusBarCtrl& ctrl = GetStatusBarCtrl();
    return ctrl.GetParts(nParts, pParts);
}

BOOL CMPCThemeStatusBar::GetRect(int nPane, LPRECT lpRect)
{
    CStatusBarCtrl& ctrl = GetStatusBarCtrl();
    return ctrl.GetRect(nPane, lpRect);
}


void CMPCThemeStatusBar::DrawItem(LPDRAWITEMSTRUCT lpDrawItemStruct)
{
    CDC dc;
    dc.Attach(lpDrawItemStruct->hDC);
    CRect rect(&lpDrawItemStruct->rcItem);
    int item = lpDrawItemStruct->itemID;
    dc.SetBkColor(CMPCTheme::StatusBarBGColor);
    dc.SetTextColor(CMPCTheme::TextFGColor);
    CFont font;
    if (CMPCThemeUtil::getFontByType(font, this, CMPCThemeUtil::MessageFont)) {
        dc.SelectObject(&font);
    }
    dc.FillSolidRect(rect, CMPCTheme::StatusBarBGColor);
    rect.left += 4;
    dc.DrawTextW(texts[item], rect, DT_NOPREFIX);
    if (item < numParts - 1) { //draw a separator
        CRect separator(rect.right, rect.top, rect.right + 1, rect.bottom);
        dc.OffsetClipRgn(1, 0); //separator is 1 pixel beyond our rect
        dc.FillSolidRect(separator, CMPCTheme::StatusBarSeparatorColor);
    }
    dc.Detach();
}


void CMPCThemeStatusBar::OnNcPaint()
{
    if (!AppIsThemeLoaded()) {
        return __super::OnNcPaint();
    } else {
        CWindowDC dc(this);

        CRect rcWindow;
        GetWindowRect(rcWindow);
        ScreenToClient(rcWindow);
        rcWindow.OffsetRect(-rcWindow.TopLeft());
        CStatusBarCtrl& ctrl = GetStatusBarCtrl();

        int nHorz, nVert, nSpacing;
        GetStatusBarCtrl().GetBorders(nHorz, nVert, nSpacing);
        for (int item = 0; item < numParts; item++) { //don't touch the status bar elements; they are painted in DrawItem
            CRect rc;
            if (GetRect(item, rc)) {
                rc.DeflateRect(1, 1, item < numParts - 1 ? 0 : 1, 1); //the rects provided to DrawItem exclude the border
                dc.ExcludeClipRect(rc);
            }
        }

        // Also exclude the gripper area (background is painted in OnEraseBkgnd)
        if (GetStyle() & SBARS_SIZEGRIP) {
            DpiHelper dpiHelper;
            dpiHelper.Override(GetSafeHwnd());
            CRect rcGripper = rcWindow;
            rcGripper.left = rcGripper.right - dpiHelper.GetSystemMetricsDPI(SM_CXVSCROLL);
            dc.ExcludeClipRect(rcGripper);
        }

        // Fill the borders only (parts and gripper are already excluded)
        dc.FillSolidRect(rcWindow, CMPCTheme::StatusBarBGColor);
        dc.SelectClipRgn(nullptr);
    }
}


BOOL CMPCThemeStatusBar::OnEraseBkgnd(CDC* pDC)
{
    if (!AppIsThemeLoaded()) {
        return __super::OnEraseBkgnd(pDC);
    } else {
        // Paint the gripper background so it has the correct color
        if (GetStyle() & SBARS_SIZEGRIP) {
            DpiHelper dpiHelper;
            dpiHelper.Override(GetSafeHwnd());
            CRect rcClient;
            GetClientRect(&rcClient);
            CRect rcGripper = rcClient;
            rcGripper.left = rcGripper.right - dpiHelper.GetSystemMetricsDPI(SM_CXVSCROLL);
            pDC->FillSolidRect(rcGripper, CMPCTheme::StatusBarBGColor);
        }
        return TRUE;
    }
}
