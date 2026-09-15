#include "stdafx.h"
#include "CMPCThemeMiniDockFrameWnd.h"
#include "CMPCThemeUtil.h"
#include "DpiHelper.h"

IMPLEMENT_DYNCREATE(CMPCThemeMiniDockFrameWnd, CMiniDockFrameWnd)

BEGIN_MESSAGE_MAP(CMPCThemeMiniDockFrameWnd, CMiniDockFrameWnd)
    ON_WM_CREATE()
    ON_MESSAGE(WM_DPICHANGED, OnDpiChanged)
END_MESSAGE_MAP()


int CMPCThemeMiniDockFrameWnd::OnCreate(LPCREATESTRUCT lpCreateStruct)
{
    if (CMiniDockFrameWnd::OnCreate(lpCreateStruct) == -1) {
        return -1;
    }

    CMPCThemeUtil::enableWindows10DarkFrame(this);

    return 0;
}

LRESULT CMPCThemeMiniDockFrameWnd::OnDpiChanged(WPARAM wParam, LPARAM lParam)
{
    LRESULT result = DefWindowProc(WM_DPICHANGED, wParam, lParam);

    // the system redraws the non-client area at the new DPI, which changes the size of the client
    // area without the bar being told; refit the frame around the bar again
    RecalcLayout(TRUE);

    return result;
}

void CMPCThemeMiniDockFrameWnd::CalcWindowRect(LPRECT lpClientRect, UINT nAdjustType)
{
    // CFrameWnd::RecalcLayout sizes the floating frame to the bar by adding the borders returned
    // here.  The default implementation uses AdjustWindowRectEx, which reports system DPI metrics
    // no matter which monitor the frame is on, so on a monitor with a different scaling the client
    // area does not match the bar and the difference is left unpainted.
    UINT dpi = DpiHelper::GetDPIForWindow(m_hWnd);
    if (dpi != 0) {
        DWORD dwExStyle = GetExStyle();
        if (nAdjustType == 0) {
            dwExStyle &= ~WS_EX_CLIENTEDGE;
        }
        if (DpiHelper::AdjustWindowRectExForDpi(lpClientRect, GetStyle(), FALSE, dwExStyle, dpi)) {
            return;
        }
    }

    __super::CalcWindowRect(lpClientRect, nAdjustType);
}
