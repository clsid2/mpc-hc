/*
 * (C) 2026 see Authors.txt
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
#include "DragRectWnd.h"

namespace
{
    // top-down 32bpp, so a pixel is one DWORD at (y * width + x)
    HBITMAP CreateArgbBitmap(int cx, int cy, DWORD** ppBits)
    {
        BITMAPINFO bmi = {};
        bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
        bmi.bmiHeader.biWidth = cx;
        bmi.bmiHeader.biHeight = -cy;
        bmi.bmiHeader.biPlanes = 1;
        bmi.bmiHeader.biBitCount = 32;
        bmi.bmiHeader.biCompression = BI_RGB;

        void* pBits = nullptr;
        HBITMAP hbm = ::CreateDIBSection(nullptr, &bmi, DIB_RGB_COLORS, &pBits, nullptr, 0);
        *ppBits = hbm ? (DWORD*)pBits : nullptr;
        return hbm;
    }

    struct ScreenCopy {
        HDC hdcDest;
        CPoint ptOrigin;
    };

    BOOL CALLBACK CopyMonitor(HMONITOR hMonitor, HDC, LPRECT, LPARAM lParam)
    {
        const ScreenCopy* pCopy = reinterpret_cast<const ScreenCopy*>(lParam);

        MONITORINFOEX mi;
        mi.cbSize = sizeof(mi);
        if (::GetMonitorInfo(hMonitor, &mi)) {
            HDC hdcDisplay = ::CreateDC(_T("DISPLAY"), mi.szDevice, nullptr, nullptr);
            if (hdcDisplay) {
                const CRect rc(mi.rcMonitor);
                ::BitBlt(pCopy->hdcDest, rc.left - pCopy->ptOrigin.x, rc.top - pCopy->ptOrigin.y,
                         rc.Width(), rc.Height(), hdcDisplay, 0, 0, SRCCOPY);
                ::DeleteDC(hdcDisplay);
            }
        }
        return TRUE;
    }
}

CDragRectWnd::~CDragRectWnd()
{
    Destroy();
}

bool CDragRectWnd::Create()
{
    if (!m_hWnd) {
        // no owner: HWND_NOTOPMOST on the frame would strip the topmost bit from its owned windows
        if (!CreateEx(WS_EX_LAYERED | WS_EX_TRANSPARENT | WS_EX_TOOLWINDOW | WS_EX_TOPMOST | WS_EX_NOACTIVATE,
                      AfxRegisterWndClass(0), nullptr, WS_POPUP, CRect(0, 0, 0, 0), nullptr, 0)) {
            return false;
        }
    }

    // grab the backdrop once per drag: it is what the frame inverts, and nothing
    // below the frame moves until the drag ends
    CaptureScreen();
    return true;
}

void CDragRectWnd::Destroy()
{
    ReleaseSurface();
    ReleaseScreen();
    if (m_hWnd) {
        DestroyWindow();
    }
}

void CDragRectWnd::Show(const CRect& rcScreen, const CSize& border, bool bDocked)
{
    if (!m_hWnd && !Create()) {
        return;
    }

    const int cx = rcScreen.Width();
    const int cy = rcScreen.Height();
    if (cx <= 0 || cy <= 0 || !EnsureSurface(CSize(cx, cy), border)) {
        Hide();
        return;
    }

    const int shotWidth = m_rcSnapshot.Width();
    const int shotHeight = m_rcSnapshot.Height();

    // MFC inverted the frame with PATINVERT through a pattern brush aligned to
    // the desktop, so the pattern follows the screen position and not the frame.
    // The halftone brush inverts where (x + y) is odd, the white brush everywhere.
    auto invertSpan = [&](int y, int x0, int x1) {
        const int sy = rcScreen.top + y;
        const int shotY = sy - m_rcSnapshot.top;
        const bool bHaveRow = (m_pScreenBits != nullptr && shotY >= 0 && shotY < shotHeight);
        DWORD* pDst = m_pSurfaceBits + (size_t)y * cx + x0;

        for (int x = x0; x < x1; x++, pDst++) {
            const int sx = rcScreen.left + x;
            const bool bInvert = bDocked || (((sx ^ sy) & 1) != 0);
            const int shotX = sx - m_rcSnapshot.left;

            if (bHaveRow && shotX >= 0 && shotX < shotWidth) {
                const DWORD rgb = m_pScreenBits[(size_t)shotY * shotWidth + shotX] & 0x00FFFFFF;
                // opaque, so there is nothing to premultiply
                *pDst = 0xFF000000 | (bInvert ? (rgb ^ 0x00FFFFFF) : rgb);
            } else {
                // no backdrop captured: the bare pattern is close enough on most backgrounds
                *pDst = bInvert ? 0xFFFFFFFF : 0xFF000000;
            }
        }
    };

    // only the frame is painted, the inside of the rect stays transparent
    const int bx = (border.cx > 0) ? border.cx : 0;
    const int by = (border.cy > 0) ? border.cy : 0;
    for (int y = 0; y < cy; y++) {
        if (y < by || y >= cy - by || bx * 2 >= cx) {
            invertSpan(y, 0, cx);
        } else {
            invertSpan(y, 0, bx);
            invertSpan(y, cx - bx, cx);
        }
    }

    POINT ptSrc = { 0, 0 };
    POINT ptDst = { rcScreen.left, rcScreen.top };
    SIZE size = { cx, cy };
    BLENDFUNCTION bf = { AC_SRC_OVER, 0, 255, AC_SRC_ALPHA };
    ::UpdateLayeredWindow(m_hWnd, nullptr, &ptDst, &size, m_dcSurface, &ptSrc, 0, &bf, ULW_ALPHA);

    // re-raise every time: the player may have entered the topmost band since the last drag
    SetWindowPos(&wndTopMost, rcScreen.left, rcScreen.top, cx, cy,
                 SWP_NOACTIVATE | SWP_NOOWNERZORDER | SWP_SHOWWINDOW);
}

void CDragRectWnd::Hide()
{
    if (m_hWnd) {
        ShowWindow(SW_HIDE);
    }
}

bool CDragRectWnd::CaptureScreen()
{
    ReleaseScreen();

    CRect rc;
    rc.left = GetSystemMetrics(SM_XVIRTUALSCREEN);
    rc.top = GetSystemMetrics(SM_YVIRTUALSCREEN);
    rc.right = rc.left + GetSystemMetrics(SM_CXVIRTUALSCREEN);
    rc.bottom = rc.top + GetSystemMetrics(SM_CYVIRTUALSCREEN);
    if (rc.IsRectEmpty()) {
        return false;
    }

    m_bmScreen = CreateArgbBitmap(rc.Width(), rc.Height(), &m_pScreenBits);
    if (!m_bmScreen) {
        return false;
    }

    HDC hdcMem = ::CreateCompatibleDC(nullptr);
    if (!hdcMem) {
        ReleaseScreen();
        return false;
    }

    // one BitBlt of the whole virtual screen from a desktop dc comes back blank
    // for some displays when they do not all share the same dpi or orientation,
    // so each display is copied from its own device dc
    HGDIOBJ hOld = ::SelectObject(hdcMem, m_bmScreen);
    ScreenCopy copy = { hdcMem, rc.TopLeft() };
    ::EnumDisplayMonitors(nullptr, nullptr, CopyMonitor, (LPARAM)&copy);
    ::SelectObject(hdcMem, hOld);
    ::DeleteDC(hdcMem);

    m_rcSnapshot = rc;
    return true;
}

void CDragRectWnd::ReleaseScreen()
{
    if (m_bmScreen) {
        ::DeleteObject(m_bmScreen);
        m_bmScreen = nullptr;
    }
    m_pScreenBits = nullptr;
    m_rcSnapshot.SetRectEmpty();
}

bool CDragRectWnd::EnsureSurface(const CSize& size, const CSize& border)
{
    if (m_bmSurface && m_sizeSurface == size) {
        if (m_sizeBorder != border) {
            // same rect but a different frame width, which happens when the drag
            // crosses to a monitor with another dpi: clear the old frame away
            ZeroMemory(m_pSurfaceBits, (size_t)size.cx * size.cy * sizeof(DWORD));
            m_sizeBorder = border;
        }
        return true;
    }

    ReleaseSurface();

    // a fresh dib section is zeroed, so everything outside the frame is transparent
    m_bmSurface = CreateArgbBitmap(size.cx, size.cy, &m_pSurfaceBits);
    if (!m_bmSurface) {
        return false;
    }

    m_dcSurface = ::CreateCompatibleDC(nullptr);
    if (!m_dcSurface) {
        ReleaseSurface();
        return false;
    }

    m_hOldSurface = ::SelectObject(m_dcSurface, m_bmSurface);
    m_sizeSurface = size;
    m_sizeBorder = border;
    return true;
}

void CDragRectWnd::ReleaseSurface()
{
    if (m_dcSurface) {
        ::SelectObject(m_dcSurface, m_hOldSurface);
        ::DeleteDC(m_dcSurface);
        m_dcSurface = nullptr;
        m_hOldSurface = nullptr;
    }
    if (m_bmSurface) {
        ::DeleteObject(m_bmSurface);
        m_bmSurface = nullptr;
    }
    m_pSurfaceBits = nullptr;
    m_sizeSurface = CSize(0, 0);
    m_sizeBorder = CSize(0, 0);
}
