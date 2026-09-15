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

#pragma once

// Top-most layered popup showing a hollow frame; used as drag feedback
// instead of XOR drawing on the desktop DC, which DWM does not composite
// reliably on multi-monitor setups.
// It looks the same as the XOR drawing did: the frame shows the screen
// content under it, inverted through the patterns CDockContext used, the
// halftone brush when floating and the white brush when over a dock target.
class CDragRectWnd : public CWnd
{
public:
    CDragRectWnd() = default;
    virtual ~CDragRectWnd();

    bool Create();
    void Destroy();
    void Show(const CRect& rcScreen, const CSize& border, bool bDocked);
    void Hide();

private:
    bool CaptureScreen();
    void ReleaseScreen();
    bool EnsureSurface(const CSize& size, const CSize& border);
    void ReleaseSurface();

    // the desktop as it was when the drag started, the frame is painted from it
    HBITMAP m_bmScreen = nullptr;
    DWORD* m_pScreenBits = nullptr;
    CRect m_rcSnapshot = CRect(0, 0, 0, 0);

    // layered surface, rebuilt only when the frame changes size
    HDC m_dcSurface = nullptr;
    HBITMAP m_bmSurface = nullptr;
    HGDIOBJ m_hOldSurface = nullptr;
    DWORD* m_pSurfaceBits = nullptr;
    CSize m_sizeSurface = CSize(0, 0);
    CSize m_sizeBorder = CSize(0, 0);
};
