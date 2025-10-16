#include "stdafx.h"
#include "CMPCThemeScrollBarRenderer.h"
#include "CMPCTheme.h"
#include "CMPCThemeUtil.h"
#include "DpiHelper.h"
#include <gdiplus.h>

CMPCThemeScrollBarRenderer* CMPCThemeScrollBarRenderer::s_pActiveRenderer = nullptr;

CMPCThemeScrollBarRenderer::CMPCThemeScrollBarRenderer()
    : m_bDrawingScrollbar(false)
    , m_hMouseHook(NULL)
    , m_hWndTracking(NULL)
    , m_dwThreadId(0) {
}

CMPCThemeScrollBarRenderer::~CMPCThemeScrollBarRenderer() {
    UninstallMouseHook();
}

void CMPCThemeScrollBarRenderer::ProcessMessage(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam) {
    switch (message) {
    case WM_NCMOUSEMOVE:
        OnNcMouseMove(hWnd, wParam, lParam);
        break;
    case WM_NCLBUTTONDOWN:
        OnNcLButtonDown(hWnd, wParam, lParam);
        break;
    case WM_NCLBUTTONUP:
        OnNcLButtonUp(hWnd, wParam, lParam);
        break;
    case WM_NCMOUSELEAVE:
        OnNcMouseLeave(hWnd);
        break;
    }
}

void CMPCThemeScrollBarRenderer::InstallMouseHook(HWND hWnd) {
    if (m_hMouseHook != NULL) {
        return;
    }

    m_hWndTracking = hWnd;
    m_dwThreadId = GetWindowThreadProcessId(hWnd, NULL);
    s_pActiveRenderer = this;

    m_hMouseHook = SetWindowsHookExW(WH_MOUSE, MouseProc, NULL, m_dwThreadId);

    if (m_hMouseHook == NULL) {
        TRACE(L"Failed to install mouse hook: %d\n", GetLastError());
    }
}

void CMPCThemeScrollBarRenderer::UninstallMouseHook() {
    if (m_hMouseHook != NULL) {
        UnhookWindowsHookEx(m_hMouseHook);
        m_hMouseHook = NULL;
    }

    if (s_pActiveRenderer == this) {
        s_pActiveRenderer = nullptr;
    }

    m_hWndTracking = NULL;
    m_dwThreadId = 0;
}

LRESULT CALLBACK CMPCThemeScrollBarRenderer::MouseProc(int nCode, WPARAM wParam, LPARAM lParam) {
    if (nCode >= 0 && s_pActiveRenderer != nullptr) {
        MOUSEHOOKSTRUCT* pMouseStruct = (MOUSEHOOKSTRUCT*)lParam;
        s_pActiveRenderer->HandleMouseEvent(wParam, pMouseStruct->pt);
    }

    return CallNextHookEx(NULL, nCode, wParam, lParam);
}

void CMPCThemeScrollBarRenderer::HandleMouseEvent(WPARAM wParam, const POINT& pt) {
    if (m_hWndTracking == NULL || !::IsWindow(m_hWndTracking)) {
        return;
    }

    LPARAM lParam = MAKELPARAM(pt.x, pt.y);

    switch (wParam) {
    case WM_MOUSEMOVE: {
        ProcessMessage(m_hWndTracking, WM_NCMOUSEMOVE, 0, lParam);
        break;
    }

    case WM_LBUTTONUP: {
        ProcessMessage(m_hWndTracking, WM_NCLBUTTONUP, 0, lParam);
        break;
    }
    }
}

bool CMPCThemeScrollBarRenderer::GetScrollBarState(HWND hWnd, int nBar, SCROLLBARINFO& sbi) {
    sbi.cbSize = sizeof(SCROLLBARINFO);
    int objId = (nBar == SB_VERT) ? OBJID_VSCROLL : OBJID_HSCROLL;
    return GetScrollBarInfo(hWnd, objId, &sbi) != FALSE;
}

BOOL CMPCThemeScrollBarRenderer::GetScrollBarRect(HWND hWnd, BOOL bVertical, CRect& rect) {
    if (!hWnd) {
        return FALSE;
    }

    SCROLLBARINFO sbi = { 0 };
    int nBar = bVertical ? SB_VERT : SB_HORZ;
    if (!GetScrollBarState(hWnd, nBar, sbi)) {
        return FALSE;
    }

    if (sbi.rgstate[0] & STATE_SYSTEM_INVISIBLE) {
        return FALSE;
    }

    rect = sbi.rcScrollBar;
    MapWindowPoints(HWND_DESKTOP, hWnd, (LPPOINT)&rect, 2);
    CPoint offset = CMPCThemeUtil::GetClientRectOffset(CWnd::FromHandlePermanent(hWnd));
    rect += offset;
    return TRUE;
}

bool CMPCThemeScrollBarRenderer::GetScrollBarRects(HWND hWnd, CRect& vScrollRect, CRect& hScrollRect, bool& bHasVScroll, bool& bHasHScroll) {
    bHasVScroll = GetScrollBarRect(hWnd, TRUE, vScrollRect);
    bHasHScroll = GetScrollBarRect(hWnd, FALSE, hScrollRect);
    return bHasVScroll || bHasHScroll;
}

bool CMPCThemeScrollBarRenderer::GetScrollBarCornerRect(HWND hWnd, CRect& cornerRect) {
    cornerRect.SetRectEmpty();

    if (!hWnd || !::IsWindow(hWnd)) {
        return false;
    }

    CWnd* pWnd = CWnd::FromHandlePermanent(hWnd);
    if (!pWnd) {
        return false;
    }

    CRect vScrollRect, hScrollRect;
    bool bHasVScroll, bHasHScroll;
    if (!GetScrollBarRects(hWnd, vScrollRect, hScrollRect, bHasVScroll, bHasHScroll)) {
        return false;
    }

    if (!bHasVScroll || !bHasHScroll) {
        return false;
    }

    CRect wr;
    pWnd->GetWindowRect(wr);
    wr.OffsetRect(-wr.left, -wr.top);

    int sbThickness = GetSystemMetrics(SM_CXVSCROLL);
    CPoint clientOffset = CMPCThemeUtil::GetClientRectOffset(pWnd);
    int borderThickness = clientOffset.x;

    if (sbThickness >= wr.Width() - borderThickness * 2 || sbThickness >= wr.Height() - borderThickness * 2) {
        return false;
    }

    cornerRect = CRect(wr.right - sbThickness - borderThickness, wr.bottom - sbThickness - borderThickness, wr.right - borderThickness, wr.bottom - borderThickness);

    return true;
}

void CMPCThemeScrollBarRenderer::DrawScrollBarCorner(CDC* pDC, HWND hWnd, const CRect& cornerRect) {
    if (cornerRect.IsRectEmpty()) {
        return;
    }

    CBrush brushCorner(CMPCTheme::ContentBGColor);
    pDC->FillRect(cornerRect, &brushCorner);
}

BOOL CMPCThemeScrollBarRenderer::ApplyScrollbarClipping(HDC hdc, HWND hWnd, const CRect& drawRect, HRGN& hOldClipRgn, bool bDrawThemedScrollbars) {
    hOldClipRgn = NULL;

    if (m_bDrawingScrollbar) {
        return TRUE;
    }

    CRect vScrollRect, hScrollRect;
    bool bHasVScroll, bHasHScroll;
    if (!GetScrollBarRects(hWnd, vScrollRect, hScrollRect, bHasVScroll, bHasHScroll)) {
        return TRUE;
    }

    CRect vIntersect, hIntersect;
    BOOL bIntersectsV = bHasVScroll && vIntersect.IntersectRect(&drawRect, &vScrollRect);
    BOOL bIntersectsH = bHasHScroll && hIntersect.IntersectRect(&drawRect, &hScrollRect);
    if (!bIntersectsV && !bIntersectsH) {
        return TRUE;
    }

    if (bDrawThemedScrollbars) {
        m_bDrawingScrollbar = true;

        CDC* pDC = CDC::FromHandle(hdc);
        if (pDC) {
            DrawThemedScrollBars(pDC, hWnd, vScrollRect, hScrollRect, bHasVScroll, bHasHScroll);
        }

        m_bDrawingScrollbar = false;
    }

    CRect excludeRect = bIntersectsV ? vScrollRect : hScrollRect;
    if (bIntersectsV && bIntersectsH) {
        excludeRect.UnionRect(&vScrollRect, &hScrollRect);
    }

    hOldClipRgn = CreateRectRgn(0, 0, 0, 0);
    if (GetClipRgn(hdc, hOldClipRgn) <= 0) {
        DeleteObject(hOldClipRgn);
        hOldClipRgn = NULL;
    }

    HRGN hDrawRgn = CreateRectRgn(drawRect.left, drawRect.top, drawRect.right, drawRect.bottom);
    HRGN hExcludeRgn = CreateRectRgn(excludeRect.left, excludeRect.top, excludeRect.right, excludeRect.bottom);
    HRGN hFinalRgn = CreateRectRgn(0, 0, 0, 0);

    auto cleanup = [&](BOOL deleteOldRgn) {
        DeleteObject(hFinalRgn);
        DeleteObject(hExcludeRgn);
        DeleteObject(hDrawRgn);
        if (deleteOldRgn && hOldClipRgn) {
            DeleteObject(hOldClipRgn);
            hOldClipRgn = NULL;
        }
    };

    if (!hDrawRgn || !hExcludeRgn || !hFinalRgn) {
        cleanup(TRUE);
        return TRUE;
    }

    int result = CombineRgn(hFinalRgn, hDrawRgn, hExcludeRgn, RGN_DIFF);

    if (result == NULLREGION || result == ERROR) {
        cleanup(TRUE);
        return FALSE;
    }

    SelectClipRgn(hdc, hFinalRgn);
    cleanup(FALSE);
    return TRUE;
}

void CMPCThemeScrollBarRenderer::RestoreClipping(HDC hdc, HRGN hOldClipRgn) {
    SelectClipRgn(hdc, hOldClipRgn);
    if (hOldClipRgn) {
        DeleteObject(hOldClipRgn);
    }
}

void CMPCThemeScrollBarRenderer::drawSBArrow(CDC& dc, COLORREF arrowClr, CRect arrowRect, arrowOrientation orientation, int dpi) {
    Gdiplus::Graphics gfx(dc.m_hDC);
    Gdiplus::Color clr;
    clr.SetFromCOLORREF(arrowClr);

    int xPos;
    int yPos;
    int xsign, ysign;
    int rows, steps;

    if (dpi < 120) {
        rows = 3;
        steps = 3;
    } else if (dpi < 144) {
        rows = 3;
        steps = 4;
    } else if (dpi < 168) {
        rows = 4;
        steps = 5;
    } else if (dpi < 192) {
        rows = 4;
        steps = 5;
    } else {
        rows = 4;
        steps = 5;
    }

    float shortDim = steps + rows;
    int indent;
    switch (orientation) {
    case arrowLeft:
        indent = ceil((arrowRect.Width() - shortDim) / 2);
        xPos = arrowRect.right - indent - 1;
        yPos = arrowRect.top + (arrowRect.Height() - (steps * 2 + 1)) / 2;
        xsign = -1;
        ysign = 1;
        break;
    case arrowRight:
        indent = ceil((arrowRect.Width() - shortDim) / 2);
        yPos = arrowRect.top + (arrowRect.Height() - (steps * 2 + 1)) / 2;
        xPos = arrowRect.left + indent;
        xsign = 1;
        ysign = 1;
        break;
    case arrowTop:
        xPos = arrowRect.left + (arrowRect.Width() - (steps * 2 + 1)) / 2;
        indent = ceil((arrowRect.Height() - shortDim) / 2);
        yPos = arrowRect.top + indent + shortDim - 1;
        xsign = 1;
        ysign = -1;
        break;
    case arrowBottom:
    default:
        xPos = arrowRect.left + (arrowRect.Width() - (steps * 2 + 1)) / 2;
        indent = ceil((arrowRect.Height() - shortDim) / 2);
        yPos = arrowRect.top + indent;
        xsign = 1;
        ysign = 1;
        break;
    }

    gfx.SetSmoothingMode(Gdiplus::SmoothingModeNone);
    Gdiplus::Pen pen(clr, 1);
    for (int i = 0; i < rows; i++) {
        if (orientation == arrowLeft || orientation == arrowRight) {
            gfx.DrawLine(&pen, xPos + i * xsign, yPos, xPos + (steps + i) * xsign, steps * ysign + yPos);
            gfx.DrawLine(&pen, xPos + (steps + i) * xsign, steps * ysign + yPos, xPos + i * xsign, (steps * 2) * ysign + yPos);
        } else {
            gfx.DrawLine(&pen, xPos, yPos + i * ysign, steps * xsign + xPos, yPos + (steps + i) * ysign);
            gfx.DrawLine(&pen, steps * xsign + xPos, yPos + (steps + i) * ysign, (steps * 2) * xsign + xPos, yPos + i * ysign);
        }
    }
}

void CMPCThemeScrollBarRenderer::DrawScrollBar(CDC* pDC, HWND hWnd, int nBar, const CRect& targetRect) {
    DpiHelper dpiWindow;
    dpiWindow.Override(hWnd);
    int nDPI = dpiWindow.DPIX();

    bool bHorizontal = (nBar == SB_HORZ);
    ScrollBarState& state = bHorizontal ? m_hScrollState : m_vScrollState;

    CRect rectTLArrow, rectBRArrow, rectThumb, rectTLChannel, rectBRChannel;
    bool bScrollBarEnabled = true;
    CalculateScrollBarRects(hWnd, nBar, targetRect, rectTLArrow, rectBRArrow, rectThumb, rectTLChannel, rectBRChannel, &bScrollBarEnabled);

    int xOffset = targetRect.left;
    int yOffset = targetRect.top;

    CDC dcMem;
    dcMem.CreateCompatibleDC(pDC);
    DWORD dwLayout = GetLayout(dcMem.m_hDC);
    if (dwLayout != GDI_ERROR && (dwLayout & LAYOUT_RTL)) {
        SetLayout(dcMem.m_hDC, dwLayout & ~LAYOUT_RTL);
    }

    CBitmap bmMem;
    bmMem.CreateCompatibleBitmap(pDC, targetRect.Width(), targetRect.Height());
    CBitmap* pOldBm = dcMem.SelectObject(&bmMem);

    CBrush brushBG(CMPCTheme::ScrollBGColor);
    CRect memRect(0, 0, targetRect.Width(), targetRect.Height());
    dcMem.FillRect(memRect, &brushBG);

    CBrush brushChannel(CMPCTheme::ScrollBGColor);

    XSB_EDRAWELEM eState;
    stXSB_AREA stArea;

    for (int nElem = eTLbutton; nElem <= eThumb; nElem++) {
        stArea.eArea = (eXSB_AREA)nElem;

        const CRect* prcElem = nullptr;
        eState = eNotDrawn;

        switch (stArea.eArea) {
        case eTLbutton:
            prcElem = &rectTLArrow;
            break;
        case eBRbutton:
            prcElem = &rectBRArrow;
            break;
        case eTLchannel:
            prcElem = &rectTLChannel;
            break;
        case eBRchannel:
            prcElem = &rectBRChannel;
            break;
        case eThumb:
            prcElem = &rectThumb;
            break;
        }

        if (!prcElem || prcElem->IsRectEmpty()) {
            continue;
        }

        XSB_EDRAWELEM eState;
        if (!bScrollBarEnabled) {
            eState = eDisabled;
        } else if (state.bDragging && stArea.IsThumb()) {
            eState = eDown;
        } else if (state.eMouseDownArea.eArea == stArea.eArea && state.eMouseOverArea.eArea == stArea.eArea) {
            eState = eDown;
        } else if (state.eMouseOverArea.eArea == stArea.eArea && !state.bDragging) {
            eState = eHot;
        } else {
            eState = eNormal;
        }

        CRect butRect = *prcElem;
        butRect.OffsetRect(-xOffset, -yOffset);

        if (bHorizontal) {
            butRect.top += 1;
            butRect.bottom -= 1;
        } else {
            butRect.left += 1;
            butRect.right -= 1;
        }

        if (stArea.IsButton()) {
            CBrush brushButton;
            COLORREF buttonClr = RGB(0, 0, 0);
            COLORREF buttonFGClr = CMPCTheme::ScrollButtonArrowColor;
            switch (eState) {
            case eDisabled:
                buttonClr = CMPCTheme::ScrollBGColor;
                break;
            case eNormal:
                buttonClr = CMPCTheme::ScrollBGColor;
                break;
            case eDown:
                buttonClr = CMPCTheme::ScrollButtonClickColor;
                buttonFGClr = CMPCTheme::ScrollButtonArrowClickColor;
                break;
            case eHot:
                buttonClr = CMPCTheme::ScrollButtonHoverColor;
                break;
            default:
                ASSERT(FALSE);
            }
            brushButton.CreateSolidBrush(buttonClr);
            dcMem.FillRect(butRect, &brushButton);
            brushButton.DeleteObject();

            if (bHorizontal) {
                if (nElem == eTLbutton) {
                    drawSBArrow(dcMem, buttonFGClr, butRect, arrowOrientation::arrowLeft, nDPI);
                } else {
                    drawSBArrow(dcMem, buttonFGClr, butRect, arrowOrientation::arrowRight, nDPI);
                }
            } else {
                if (nElem == eTLbutton) {
                    drawSBArrow(dcMem, buttonFGClr, butRect, arrowOrientation::arrowTop, nDPI);
                } else {
                    drawSBArrow(dcMem, buttonFGClr, butRect, arrowOrientation::arrowBottom, nDPI);
                }
            }
        } else if (stArea.IsChannel()) {
            dcMem.FillRect(butRect, &brushChannel);
        } else {
            CBrush brushThumb;
            switch (eState) {
            case eDisabled:
                brushThumb.CreateSolidBrush(CMPCTheme::ScrollBGColor);
                break;
            case eNormal:
                brushThumb.CreateSolidBrush(CMPCTheme::ScrollThumbColor);
                break;
            case eDown:
                brushThumb.CreateSolidBrush(CMPCTheme::ScrollThumbDragColor);
                break;
            case eHot:
                brushThumb.CreateSolidBrush(CMPCTheme::ScrollThumbHoverColor);
                break;
            default:
                ASSERT(FALSE);
            }
            dcMem.FillRect(butRect, &brushThumb);
            brushThumb.DeleteObject();
        }
    }

    pDC->BitBlt(targetRect.left, targetRect.top, targetRect.Width(), targetRect.Height(), &dcMem, 0, 0, SRCCOPY);

    dcMem.SelectObject(pOldBm);
    bmMem.DeleteObject();
}

eXSB_AREA CMPCThemeScrollBarRenderer::GetScrollBarArea(HWND hWnd, CPoint clientPoint, bool bVertical, const CRect& scrollRect) {
    if (!scrollRect.PtInRect(clientPoint)) {
        return eNone;
    }

    CRect rectTLArrow, rectBRArrow, rectThumb, rectTLChannel, rectBRChannel;
    int nBar = bVertical ? SB_VERT : SB_HORZ;
    CalculateScrollBarRects(hWnd, nBar, scrollRect, rectTLArrow, rectBRArrow, rectThumb, rectTLChannel, rectBRChannel);

    if (rectThumb.PtInRect(clientPoint)) {
        return eThumb;
    } else if (rectTLArrow.PtInRect(clientPoint)) {
        return eTLbutton;
    } else if (rectBRArrow.PtInRect(clientPoint)) {
        return eBRbutton;
    } else if (rectTLChannel.PtInRect(clientPoint)) {
        return eTLchannel;
    } else if (rectBRChannel.PtInRect(clientPoint)) {
        return eBRchannel;
    }

    return eNone;
}

void CMPCThemeScrollBarRenderer::CalculateScrollBarRects(HWND hWnd, int nBar, const CRect& scrollRect, CRect& rectTLArrow, CRect& rectBRArrow, CRect& rectThumb, CRect& rectTLChannel, CRect& rectBRChannel, bool* pbEnabled) {
    // Get scroll info
    SCROLLINFO si = { 0 };
    si.cbSize = sizeof(SCROLLINFO);
    si.fMask = SIF_ALL;
    if (!::GetScrollInfo(hWnd, nBar, &si)) {
        rectTLArrow.SetRectEmpty();
        rectBRArrow.SetRectEmpty();
        rectThumb.SetRectEmpty();
        rectTLChannel.SetRectEmpty();
        rectBRChannel.SetRectEmpty();
        if (pbEnabled) *pbEnabled = false;
        return;
    }

    bool bEnabled = true;
    if (pbEnabled) {
        SCROLLBARINFO sbi = { 0 };
        if (GetScrollBarState(hWnd, nBar, sbi)) {
            bEnabled = !(sbi.rgstate[0] & STATE_SYSTEM_UNAVAILABLE);
        }
        *pbEnabled = bEnabled;
    }
    bool bHorizontal = (nBar == SB_HORZ);
    UINT uArrowWH = bHorizontal ? scrollRect.Height() : scrollRect.Width();
    UINT uThumbMinHW = 8;

    if (bHorizontal) {
        int cxClient = scrollRect.Width();
        int cxArrow = std::min((int)uArrowWH, cxClient / 2);

        rectTLArrow.SetRect(scrollRect.left, scrollRect.top, scrollRect.left + cxArrow, scrollRect.bottom);
        rectBRArrow.SetRect(scrollRect.right - cxArrow, scrollRect.top, scrollRect.right, scrollRect.bottom);

        int cxChannel = cxClient - (2 * cxArrow);
        int nRange = si.nMax - si.nMin + 1;
        if (nRange > 0 && cxChannel > (int)uThumbMinHW) {
            double dblPx_SU = (double)cxChannel / (double)nRange;
            int xThumb = (int)((si.nPos - si.nMin) * dblPx_SU);
            int cxThumb = std::max((int)uThumbMinHW, (int)(si.nPage * dblPx_SU));

            if (cxThumb > cxChannel) {
                cxThumb = cxChannel;
            }
            if (xThumb + cxThumb > cxChannel) {
                xThumb = cxChannel - cxThumb;
            }

            xThumb += rectTLArrow.right;
            rectThumb.SetRect(xThumb, scrollRect.top, xThumb + cxThumb, scrollRect.bottom);

            rectTLChannel.SetRect(rectTLArrow.right, scrollRect.top, rectThumb.left, scrollRect.bottom);
            rectBRChannel.SetRect(rectThumb.right, scrollRect.top, rectBRArrow.left, scrollRect.bottom);
        } else {
            rectThumb.SetRectEmpty();
            rectTLChannel.SetRect(rectTLArrow.right, scrollRect.top, rectBRArrow.left, scrollRect.bottom);
            rectBRChannel.SetRectEmpty();
        }
    } else {
        int cyClient = scrollRect.Height();
        int cyArrow = std::min((int)uArrowWH, cyClient / 2);

        rectTLArrow.SetRect(scrollRect.left, scrollRect.top, scrollRect.right, scrollRect.top + cyArrow);
        rectBRArrow.SetRect(scrollRect.left, scrollRect.bottom - cyArrow, scrollRect.right, scrollRect.bottom);

        int cyChannel = cyClient - (2 * cyArrow);
        int nRange = si.nMax - si.nMin + 1;
        if (nRange > 0 && cyChannel > (int)uThumbMinHW) {
            double dblPx_SU = (double)cyChannel / (double)nRange;
            int yThumb = (int)((si.nPos - si.nMin) * dblPx_SU);
            int cyThumb = std::max((int)uThumbMinHW, (int)(si.nPage * dblPx_SU));

            if (cyThumb > cyChannel) {
                cyThumb = cyChannel;
            }
            if (yThumb + cyThumb > cyChannel) {
                yThumb = cyChannel - cyThumb;
            }

            yThumb += rectTLArrow.bottom;
            rectThumb.SetRect(scrollRect.left, yThumb, scrollRect.right, yThumb + cyThumb);

            rectTLChannel.SetRect(scrollRect.left, rectTLArrow.bottom, scrollRect.right, rectThumb.top);
            rectBRChannel.SetRect(scrollRect.left, rectThumb.bottom, scrollRect.right, rectBRArrow.top);
        } else {
            rectThumb.SetRectEmpty();
            rectTLChannel.SetRect(scrollRect.left, rectTLArrow.bottom, scrollRect.right, rectBRArrow.top);
            rectBRChannel.SetRectEmpty();
        }
    }
}

void CMPCThemeScrollBarRenderer::OnNcMouseMove(HWND hWnd, WPARAM wParam, LPARAM lParam) {
    CPoint point(GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
    ::ScreenToClient(hWnd, &point);

    CRect vScrollRect, hScrollRect;
    bool bHasVScroll, bHasHScroll;
    if (!GetScrollBarRects(hWnd, vScrollRect, hScrollRect, bHasVScroll, bHasHScroll)) {
        return;
    }

    if (bHasVScroll && vScrollRect.PtInRect(point)) {
        eXSB_AREA area = GetScrollBarArea(hWnd, point, true, vScrollRect);
        if (m_vScrollState.bDragging) {
            m_vScrollState.eMouseOverArea.eArea = area;
        } else if (m_vScrollState.eMouseDownArea.eArea != eNone) {
            m_vScrollState.eMouseOverArea.eArea = (area == m_vScrollState.eMouseDownArea.eArea) ? area : eNone;
        } else {
            m_vScrollState.eMouseOverArea.eArea = area;
        }
    } else {
        m_vScrollState.eMouseOverArea.eArea = eNone;
    }

    if (bHasHScroll && hScrollRect.PtInRect(point)) {
        eXSB_AREA area = GetScrollBarArea(hWnd, point, false, hScrollRect);
        if (m_hScrollState.bDragging) {
            m_hScrollState.eMouseOverArea.eArea = area;
        } else if (m_hScrollState.eMouseDownArea.eArea != eNone) {
            m_hScrollState.eMouseOverArea.eArea = (area == m_hScrollState.eMouseDownArea.eArea) ? area : eNone;
        } else {
            m_hScrollState.eMouseOverArea.eArea = area;
        }
    } else {
        m_hScrollState.eMouseOverArea.eArea = eNone;
    }
}

void CMPCThemeScrollBarRenderer::OnNcLButtonDown(HWND hWnd, WPARAM wParam, LPARAM lParam) {
    CPoint point(GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
    ::ScreenToClient(hWnd, &point);

    CRect vScrollRect, hScrollRect;
    bool bHasVScroll, bHasHScroll;
    if (!GetScrollBarRects(hWnd, vScrollRect, hScrollRect, bHasVScroll, bHasHScroll)) {
        return;
    }

    if (bHasVScroll && vScrollRect.PtInRect(point)) {
        m_vScrollState.eMouseDownArea = m_vScrollState.eMouseOverArea;
        if (m_vScrollState.eMouseDownArea.IsThumb()) {
            m_vScrollState.bDragging = true;
        }
        InstallMouseHook(hWnd);
    }

    if (bHasHScroll && hScrollRect.PtInRect(point)) {
        m_hScrollState.eMouseDownArea = m_hScrollState.eMouseOverArea;
        if (m_hScrollState.eMouseDownArea.IsThumb()) {
            m_hScrollState.bDragging = true;
        }
        InstallMouseHook(hWnd);
    }
}

void CMPCThemeScrollBarRenderer::OnNcLButtonUp(HWND hWnd, WPARAM wParam, LPARAM lParam) {
    m_vScrollState.eMouseDownArea.eArea = eNone;
    m_vScrollState.bDragging = false;
    m_hScrollState.eMouseDownArea.eArea = eNone;
    m_hScrollState.bDragging = false;

    UninstallMouseHook();
}

void CMPCThemeScrollBarRenderer::OnNcMouseLeave(HWND hWnd) {
    m_vScrollState.eMouseOverArea.eArea = eNone;
    m_hScrollState.eMouseOverArea.eArea = eNone;
}

void CMPCThemeScrollBarRenderer::DrawThemedScrollBars(CDC* pDC, HWND hWnd, const CRect& vScrollRect, const CRect& hScrollRect, bool bHasVScroll, bool bHasHScroll) {

    if (bHasVScroll && !vScrollRect.IsRectEmpty()) {
        DrawScrollBar(pDC, hWnd, SB_VERT, vScrollRect);
    }

    if (bHasHScroll && !hScrollRect.IsRectEmpty()) {
        DrawScrollBar(pDC, hWnd, SB_HORZ, hScrollRect);
    }

    // Draw corner if both scrollbars present
    if (bHasVScroll && bHasHScroll) {
        CRect cornerRect;
        if (GetScrollBarCornerRect(hWnd, cornerRect)) {
            DrawScrollBarCorner(pDC, hWnd, cornerRect);
        }
    }
}

void CMPCThemeScrollBarRenderer::HandleNcPaint(HWND hWnd) {
    CWindowDC dc(CWnd::FromHandle(hWnd));
    int oldDC = dc.SaveDC();

    CRect wr;
    ::GetWindowRect(hWnd, wr);
    wr.OffsetRect(-wr.left, -wr.top);

    CRect clip = wr;
    CPoint clientOffset = CMPCThemeUtil::GetClientRectOffset(CWnd::FromHandle(hWnd));
    clip.DeflateRect(clientOffset.x, clientOffset.x);
    dc.ExcludeClipRect(clip);

    CRect vScrollRect, hScrollRect;
    bool bHasVScroll, bHasHScroll;
    if (GetScrollBarRects(hWnd, vScrollRect, hScrollRect, bHasVScroll, bHasHScroll)) {
        if (bHasVScroll) {
            dc.ExcludeClipRect(vScrollRect);
        }
        if (bHasHScroll) {
            dc.ExcludeClipRect(hScrollRect);
        }
    }

    CBrush brush(CMPCTheme::WindowBorderColorLight);
    dc.FillSolidRect(wr, CMPCTheme::ContentBGColor);
    dc.FrameRect(wr, &brush);

    dc.RestoreDC(oldDC);

    //we force the draw to go through the native path, so that the hover states are property updated
    //note, the fMask is zero -- it doesn't actually change any scroll info
    if (bHasVScroll) {
        SCROLLINFO si = { sizeof(SCROLLINFO), 0 };
        ::SetScrollInfo(hWnd, SB_VERT, &si, false);
    }
    if (bHasHScroll) {
        SCROLLINFO si = { sizeof(SCROLLINFO), 0 };
        ::SetScrollInfo(hWnd, SB_HORZ, &si, false);
    }
}
