#include "stdafx.h"
#include "CMPCThemeStatusBar.h"
#include "CMPCTheme.h"
#include "CMPCThemeUtil.h"
#include "mplayerc.h"
#include "DpiHelper.h"

CMPCThemeStatusBar::CMPCThemeStatusBar()
    : m_pProgressBar(nullptr)
    , m_nProgressPane(-1)
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
    ON_WM_PAINT()
    ON_WM_SIZE()
END_MESSAGE_MAP()

void CMPCThemeStatusBar::SetText(LPCTSTR lpszText, int nPane, int nType)
{
    if (AppIsThemeLoaded()) {
        texts[nPane] = lpszText;
        Invalidate();
    } else {
        CStatusBarCtrl& ctrl = GetStatusBarCtrl();
        ctrl.SetText(lpszText, nPane, nType);
    }
}

BOOL CMPCThemeStatusBar::SetParts(int nParts, int* pWidths)
{
    CStatusBarCtrl& ctrl = GetStatusBarCtrl();
    numParts = nParts;
    BOOL result = ctrl.SetParts(nParts, pWidths);
    UpdateProgressBarLayout();
    Invalidate();
    return result;
}

void CMPCThemeStatusBar::SetProgressBar(CWnd* pProgress, int nPane)
{
    m_pProgressBar = pProgress;
    m_nProgressPane = nPane;
    if (m_pProgressBar) {
        m_pProgressBar->SetParent(this);
    }
}

void CMPCThemeStatusBar::UpdateProgressBarLayout()
{
    if (m_pProgressBar && m_pProgressBar->GetSafeHwnd() && m_nProgressPane >= 0) {
        CRect rect;
        if (GetRect(m_nProgressPane, &rect)) {
            rect.DeflateRect(1, 1, 1, 1);
            m_pProgressBar->SetWindowPos(nullptr, rect.left, rect.top, rect.Width(), rect.Height(),
                SWP_NOACTIVATE | SWP_NOZORDER);
        }
    }
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
                rc.DeflateRect(1, 1, item < numParts - 1 ? 0 : 1, 1);
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

        // Fill the outer window borders with theme color
        dc.FillSolidRect(rcWindow, CMPCTheme::StatusBarBGColor);
        dc.SelectClipRgn(nullptr);
    }
}


BOOL CMPCThemeStatusBar::OnEraseBkgnd(CDC* pDC)
{
    if (!AppIsThemeLoaded()) {
        return __super::OnEraseBkgnd(pDC);
    } else {
        // Paint the entire client area background
        CRect rcClient;
        GetClientRect(&rcClient);
        pDC->FillSolidRect(rcClient, CMPCTheme::StatusBarBGColor);
        return TRUE;
    }
}

void CMPCThemeStatusBar::OnPaint()
{
    if (!AppIsThemeLoaded()) {
        return __super::OnPaint();
    }

    CPaintDC dc(this);
    CRect rcClient;
    GetClientRect(&rcClient);

    // Paint background
    dc.FillSolidRect(rcClient, CMPCTheme::StatusBarBGColor);

    // Set up font
    CFont font;
    if (CMPCThemeUtil::getFontByType(font, this, CMPCThemeUtil::StatusFont)) {
        dc.SelectObject(&font);
    }
    dc.SetBkColor(CMPCTheme::StatusBarBGColor);
    dc.SetTextColor(CMPCTheme::TextFGColor);

    // Paint each part
    for (int item = 0; item < numParts; item++) {
        CRect rc;
        if (GetRect(item, rc)) {
            // Draw text with left padding
            CRect textRect = rc;
            textRect.left += 4;
            dc.DrawTextW(texts[item], textRect, DT_NOPREFIX | DT_SINGLELINE | DT_VCENTER);

            // Draw separator between parts
            if (item < numParts - 1) {
                CRect separator(rc.right - 1, rc.top, rc.right, rc.bottom);
                dc.FillSolidRect(separator, CMPCTheme::StatusBarSeparatorColor);
            }
        }
    }

    // Draw size gripper if present
    if (GetStyle() & SBARS_SIZEGRIP) {
        DpiHelper dpiHelper;
        dpiHelper.Override(GetSafeHwnd());
        CRect rcGripper = rcClient;

        // Try themed gripper first (modern Windows style)
        HTHEME hTheme = OpenThemeData(GetSafeHwnd(), L"Status");
        if (hTheme) {
            SIZE gripperSize;
            if (SUCCEEDED(GetThemePartSize(hTheme, dc.GetSafeHdc(), SP_GRIPPER, 0, &rcGripper, TS_DRAW, &gripperSize))) {
                rcGripper.left = rcGripper.right - gripperSize.cx;
                rcGripper.top = rcGripper.bottom - gripperSize.cy;
                DrawThemeBackground(hTheme, dc.GetSafeHdc(), SP_GRIPPER, 0, &rcGripper, NULL);
            }
            CloseThemeData(hTheme);
        } else {
            // Fallback to classic gripper
            int gripperSize = dpiHelper.GetSystemMetricsDPI(SM_CXVSCROLL);
            rcGripper.left = std::max(rcGripper.left, rcGripper.right - gripperSize - 1);
            rcGripper.top = std::max(rcGripper.top, rcGripper.bottom - dpiHelper.GetSystemMetricsDPI(SM_CYHSCROLL) - 1);
            dc.DrawFrameControl(&rcGripper, DFC_SCROLL, DFCS_SCROLLSIZEGRIP);
        }
    }
}

void CMPCThemeStatusBar::OnSize(UINT nType, int cx, int cy)
{
    __super::OnSize(nType, cx, cy);
    UpdateProgressBarLayout();
    Invalidate();
}
