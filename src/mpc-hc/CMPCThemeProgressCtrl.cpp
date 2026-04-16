#include "stdafx.h"
#include "CMPCThemeProgressCtrl.h"
#include "CMPCTheme.h"
#include "mplayerc.h"

CMPCThemeProgressCtrl::CMPCThemeProgressCtrl()
{
}

CMPCThemeProgressCtrl::~CMPCThemeProgressCtrl()
{
}

BEGIN_MESSAGE_MAP(CMPCThemeProgressCtrl, CProgressCtrl)
    ON_WM_PAINT()
    ON_MESSAGE(PBM_SETPOS, OnSetPos)
    ON_MESSAGE(PBM_STEPIT, OnStepIt)
    ON_MESSAGE(WM_DPICHANGED, OnDpiChanged)
END_MESSAGE_MAP()

LRESULT CMPCThemeProgressCtrl::OnSetPos(WPARAM wParam, LPARAM lParam)
{
    LRESULT result = DefWindowProc(PBM_SETPOS, wParam, lParam);
    if (AppIsThemeLoaded()) {
        Invalidate(FALSE);
    }
    return result;
}

LRESULT CMPCThemeProgressCtrl::OnStepIt(WPARAM wParam, LPARAM lParam)
{
    LRESULT result = DefWindowProc(PBM_STEPIT, wParam, lParam);
    if (AppIsThemeLoaded()) {
        Invalidate(FALSE);
    }
    return result;
}

LRESULT CMPCThemeProgressCtrl::OnDpiChanged(WPARAM wParam, LPARAM lParam)
{
    LRESULT result = DefWindowProc(WM_DPICHANGED, wParam, lParam);
    if (AppIsThemeLoaded()) {
        RedrawWindow(NULL, NULL, RDW_INVALIDATE | RDW_ERASE | RDW_FRAME | RDW_ALLCHILDREN);
    }
    return result;
}

void CMPCThemeProgressCtrl::OnPaint()
{
    if (!AppIsThemeLoaded()) {
        return __super::OnPaint();
    }

    CPaintDC dc(this);
    CRect rcClient;
    GetClientRect(&rcClient);

    int nLower, nUpper;
    GetRange(nLower, nUpper);
    int nPos = GetPos();

    // Draw border first
    CBrush brush(CMPCTheme::NoBorderColor);
    dc.FrameRect(&rcClient, &brush);

    // Deflate rect for interior painting
    CRect rcInterior(rcClient);
    rcInterior.DeflateRect(1, 1, 1, 1);

    // Draw background
    dc.FillSolidRect(rcInterior, CMPCTheme::ProgressBarBGColor);

    // Draw progress bar
    if (nUpper > nLower && nPos > nLower) {
        int width = MulDiv(rcInterior.Width(), nPos - nLower, nUpper - nLower);
        CRect rcProgress(rcInterior);
        rcProgress.right = rcProgress.left + width;
        dc.FillSolidRect(rcProgress, CMPCTheme::ProgressBarColor);
    }
}
