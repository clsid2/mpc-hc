/*
 * (C) 2015-2016 see Authors.txt
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

#pragma once
#include <afxcmn.h>

class DpiHelper final
{
public:

    DpiHelper();

    void Override(HWND hWindow);
    void Override(int dpix, int dpiy);
    int GetSystemMetricsDPI(int nIndex);
    void GetMessageFont(LOGFONT* lf);
    bool GetNonClientMetrics(PNONCLIENTMETRICSW, bool& dpiCorrected);
    int GetSystemMetrics(int type);
    static UINT GetDPIForWindow(HWND wnd);
    static UINT GetDPIForMonitor(HMONITOR hMonitor);
    static UINT GetDPIForRect(const RECT* pRect);
    static double GetTextScaleFactor();
    int CalculateListCtrlItemHeight(CListCtrl* wnd);

    // Get dialog font metrics for DLU conversion at specified DPI (cached)
    // fontFace: font name (e.g., "Segoe UI")
    // fontSize: point size (e.g., 9)
    static bool GetDialogFontMetricsForDPI(UINT dpi, LPCTSTR fontFace, int fontSize, int& avgWidth, int& avgHeight);
    static void ClearDialogFontMetricsCache();

    // DPI-aware window rect adjustment (adds window decorations like title bar, borders)
    static BOOL AdjustWindowRectExForDpi(LPRECT lpRect, DWORD dwStyle, BOOL bMenu, DWORD dwExStyle, UINT dpi);

    inline double ScaleFactorX() const { return m_dpix / 96.0; }
    inline double ScaleFactorY() const { return m_dpiy / 96.0; }

    // Convert a point size (1/72 of an inch) to raw pixels.
    inline int PointsToPixels(int pt) const { return MulDiv(pt, m_dpiy, 72); }

    inline int ScaleFloorX(int x) const { return x * m_dpix / 96; }
    inline int ScaleFloorY(int y) const { return y * m_dpiy / 96; }
    inline int ScaleX(int x) const { return MulDiv(x, m_dpix, 96); }
    inline int ScaleY(int y) const { return MulDiv(y, m_dpiy, 96); }
    inline int TransposeScaledX(int x) const { return MulDiv(x, m_dpiy, m_dpix); }
    inline int TransposeScaledY(int y) const { return MulDiv(y, m_dpix, m_dpiy); }

    inline int ScaleSystemToOverrideX(int x) const { return MulDiv(x, m_dpix, m_sdpix); }
    inline int ScaleSystemToOverrideY(int y) const { return MulDiv(y, m_dpiy, m_sdpiy); }
    inline int ScaleArbitraryToOverrideY (int y, int dpi_y) const { return MulDiv(y, m_dpix, dpi_y); }

    inline int DPIX() { return m_dpix; }
    inline int DPIY() { return m_dpiy; }
    inline int SDPIX() const { return m_sdpix; }
    inline int SDPIY() const { return m_sdpiy; }
    static bool CanUsePerMonitorV2();
    inline void ScaleRect(__inout RECT* pRect) {
        pRect->left = ScaleX(pRect->left);
        pRect->right = ScaleX(pRect->right);
        pRect->top = ScaleY(pRect->top);
        pRect->bottom = ScaleY(pRect->bottom);
    }

private:

    int m_dpix, m_dpiy;
    int m_sdpix, m_sdpiy;
};
