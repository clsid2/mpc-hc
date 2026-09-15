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
#include "PlayerBarDockContext.h"
#include "DpiHelper.h"

// The tracking loop below is adapted from MFC's dockcont.cpp. MFC draws the
// drag outline by XORing onto a DC of the desktop window. Under DWM that
// drawing can land on the wrong monitor and is never erased (issue #4203).
// Track/Move/Stretch/... are non-virtual in CDockContext, so they are copied
// here and the feedback is routed to a layered window instead.

#define HORZF(dw) (dw & CBRS_ORIENT_HORZ)
#define VERTF(dw) (dw & CBRS_ORIENT_VERT)

// from afxglobals.h, which is not included here
#ifndef AFX_CX_BORDER
#define AFX_CX_BORDER 1
#define AFX_CY_BORDER 1
#endif

static void AdjustRectangle(CRect& rect, CPoint pt)
{
    int nXOffset = (pt.x < rect.left) ? (pt.x - rect.left) :
                   (pt.x > rect.right) ? (pt.x - rect.right) : 0;
    int nYOffset = (pt.y < rect.top) ? (pt.y - rect.top) :
                   (pt.y > rect.bottom) ? (pt.y - rect.bottom) : 0;
    rect.OffsetRect(nXOffset, nYOffset);
}

CPlayerBarDockContext::CPlayerBarDockContext(CControlBar* pBar)
    : CDockContext(pBar)
{
}

/////////////////////////////////////////////////////////////////////////////
// Drag Operations

void CPlayerBarDockContext::StartDrag(CPoint pt)
{
    ASSERT_VALID(m_pBar);
    m_bDragging = TRUE;

    InitLoop();

    ASSERT((m_pBar->m_dwStyle & CBRS_SIZE_DYNAMIC) != 0);

    // get true bar size (including borders)
    CRect rect;
    m_pBar->GetWindowRect(rect);
    m_ptLast = pt;
    CSize sizeHorz = m_pBar->CalcDynamicLayout(0, LM_HORZ | LM_HORZDOCK);
    CSize sizeVert = m_pBar->CalcDynamicLayout(0, LM_VERTDOCK);
    CSize sizeFloat = m_pBar->CalcDynamicLayout(0, LM_HORZ | LM_MRUWIDTH);

    m_rectDragHorz = CRect(rect.TopLeft(), sizeHorz);
    m_rectDragVert = CRect(rect.TopLeft(), sizeVert);

    // calculate frame dragging rectangle
    m_rectFrameDragHorz = CRect(rect.TopLeft(), sizeFloat);
    m_rectFrameDragVert = CRect(rect.TopLeft(), sizeFloat);

    CMiniFrameWnd::CalcBorders(&m_rectFrameDragHorz);
    CMiniFrameWnd::CalcBorders(&m_rectFrameDragVert);

    m_rectFrameDragHorz.InflateRect(-2, -2);
    m_rectFrameDragVert.InflateRect(-2, -2);

    // adjust rectangles so that point is inside
    AdjustRectangle(m_rectDragHorz, pt);
    AdjustRectangle(m_rectDragVert, pt);
    AdjustRectangle(m_rectFrameDragHorz, pt);
    AdjustRectangle(m_rectFrameDragVert, pt);

    // initialize tracking state and enter tracking loop
    m_dwOverDockStyle = CanDock();
    Move(pt);   // call it here to handle special keys
    Track();
}

void CPlayerBarDockContext::Move(CPoint pt)
{
    CPoint ptOffset = pt - m_ptLast;

    // offset all drag rects to new position
    m_rectDragHorz.OffsetRect(ptOffset);
    m_rectFrameDragHorz.OffsetRect(ptOffset);
    m_rectDragVert.OffsetRect(ptOffset);
    m_rectFrameDragVert.OffsetRect(ptOffset);
    m_ptLast = pt;

    // if control key is down don't dock
    m_dwOverDockStyle = m_bForceFrame ? 0 : CanDock();

    // update feedback
    DrawFocusRect();
}

void CPlayerBarDockContext::OnKey(int nChar, BOOL bDown)
{
    if (nChar == VK_CONTROL) {
        UpdateState(&m_bForceFrame, bDown);
    }
    if (nChar == VK_SHIFT) {
        UpdateState(&m_bFlip, bDown);
    }
}

void CPlayerBarDockContext::EndDrag()
{
    CancelLoop();

    if (m_dwOverDockStyle != 0) {
        CDockBar* pDockBar = GetDockBar(m_dwOverDockStyle);
        ASSERT(pDockBar != nullptr);

        CRect rect = (m_dwOverDockStyle & CBRS_ORIENT_VERT) ? m_rectDragVert : m_rectDragHorz;

        UINT uID = ::GetDlgCtrlID(pDockBar->m_hWnd);
        if (uID >= AFX_IDW_DOCKBAR_TOP && uID <= AFX_IDW_DOCKBAR_BOTTOM) {
            m_uMRUDockID = uID;
            m_rectMRUDockPos = rect;
            pDockBar->ScreenToClient(&m_rectMRUDockPos);
        }

        // dock it at the specified position, RecalcLayout will snap
        m_pDockSite->DockControlBar(m_pBar, pDockBar, &rect);
        m_pDockSite->RecalcLayout();
    } else if ((m_dwStyle & CBRS_SIZE_DYNAMIC) || (HORZF(m_dwStyle) && !m_bFlip) || (VERTF(m_dwStyle) && m_bFlip)) {
        m_dwMRUFloatStyle = CBRS_ALIGN_TOP | (m_dwDockStyle & CBRS_FLOAT_MULTI);
        m_ptMRUFloatPos = m_rectFrameDragHorz.TopLeft();
        m_pDockSite->FloatControlBar(m_pBar, m_ptMRUFloatPos, m_dwMRUFloatStyle);
    } else { // vertical float
        m_dwMRUFloatStyle = CBRS_ALIGN_LEFT | (m_dwDockStyle & CBRS_FLOAT_MULTI);
        m_ptMRUFloatPos = m_rectFrameDragVert.TopLeft();
        m_pDockSite->FloatControlBar(m_pBar, m_ptMRUFloatPos, m_dwMRUFloatStyle);
    }
}

/////////////////////////////////////////////////////////////////////////////
// Resize Operations

#define m_rectRequestedSize     m_rectDragHorz
#define m_rectActualSize        m_rectDragVert
#define m_rectActualFrameSize   m_rectFrameDragHorz
#define m_rectFrameBorders      m_rectFrameDragVert

void CPlayerBarDockContext::StartResize(int nHitTest, CPoint pt)
{
    ASSERT_VALID(m_pBar);
    ASSERT(m_pBar->m_dwStyle & CBRS_SIZE_DYNAMIC);
    m_bDragging = FALSE;

    InitLoop();

    // get true bar size (including borders)
    CRect rect;
    m_pBar->GetWindowRect(rect);
    m_ptLast = pt;
    m_nHitTest = nHitTest;

    CSize size = m_pBar->CalcDynamicLayout(0, LM_HORZ | LM_MRUWIDTH);
    m_rectRequestedSize = CRect(rect.TopLeft(), size);
    m_rectActualSize = CRect(rect.TopLeft(), size);
    m_rectActualFrameSize = CRect(rect.TopLeft(), size);

    // calculate frame rectangle
    CMiniFrameWnd::CalcBorders(&m_rectActualFrameSize);
    m_rectActualFrameSize.InflateRect(-2, -2);

    m_rectFrameBorders = CRect(CPoint(0, 0), m_rectActualFrameSize.Size() - m_rectActualSize.Size());

    // initialize tracking state and enter tracking loop
    m_dwOverDockStyle = 0;
    Stretch(pt);   // call it here to handle special keys
    Track();
}

void CPlayerBarDockContext::Stretch(CPoint pt)
{
    CPoint ptOffset = pt - m_ptLast;

    // offset all drag rects to new position
    int nLength = 0;
    DWORD dwMode = LM_HORZ;
    if (m_nHitTest == HTLEFT || m_nHitTest == HTRIGHT) {
        if (m_nHitTest == HTLEFT) {
            m_rectRequestedSize.left += ptOffset.x;
        } else {
            m_rectRequestedSize.right += ptOffset.x;
        }
        nLength = m_rectRequestedSize.Width();
    } else {
        dwMode |= LM_LENGTHY;
        if (m_nHitTest == HTTOP) {
            m_rectRequestedSize.top += ptOffset.y;
        } else {
            m_rectRequestedSize.bottom += ptOffset.y;
        }
        nLength = m_rectRequestedSize.Height();
    }
    nLength = (nLength >= 0) ? nLength : 0;

    CSize size = m_pBar->CalcDynamicLayout(nLength, dwMode);

    CRect rectDesk;
    rectDesk.left = GetSystemMetrics(SM_XVIRTUALSCREEN);
    rectDesk.top = GetSystemMetrics(SM_YVIRTUALSCREEN);
    rectDesk.right = rectDesk.left + GetSystemMetrics(SM_CXVIRTUALSCREEN);
    rectDesk.bottom = rectDesk.top + GetSystemMetrics(SM_CYVIRTUALSCREEN);
    CRect rectTemp = m_rectActualFrameSize;

    if (m_nHitTest == HTLEFT || m_nHitTest == HTTOP) {
        rectTemp.left = rectTemp.right - (size.cx + m_rectFrameBorders.Width());
        rectTemp.top = rectTemp.bottom - (size.cy + m_rectFrameBorders.Height());
        CRect rect;
        if (rect.IntersectRect(rectDesk, rectTemp)) {
            m_rectActualSize.left = m_rectActualSize.right - size.cx;
            m_rectActualSize.top = m_rectActualSize.bottom - size.cy;
            m_rectActualFrameSize.left = rectTemp.left;
            m_rectActualFrameSize.top = rectTemp.top;
        }
    } else {
        rectTemp.right = rectTemp.left + (size.cx + m_rectFrameBorders.Width());
        rectTemp.bottom = rectTemp.top + (size.cy + m_rectFrameBorders.Height());
        CRect rect;
        if (rect.IntersectRect(rectDesk, rectTemp)) {
            m_rectActualSize.right = m_rectActualSize.left + size.cx;
            m_rectActualSize.bottom = m_rectActualSize.top + size.cy;
            m_rectActualFrameSize.right = rectTemp.right;
            m_rectActualFrameSize.bottom = rectTemp.bottom;
        }
    }
    m_ptLast = pt;

    // update feedback
    DrawFocusRect();
}

void CPlayerBarDockContext::EndResize()
{
    CancelLoop();

    m_pBar->CalcDynamicLayout(m_rectActualSize.Width(), LM_HORZ | LM_COMMIT);
    m_pDockSite->FloatControlBar(m_pBar, m_rectActualFrameSize.TopLeft(),
                                 CBRS_ALIGN_TOP | (m_dwDockStyle & CBRS_FLOAT_MULTI) | CBRS_SIZE_DYNAMIC);
}

/////////////////////////////////////////////////////////////////////////////
// Operations

void CPlayerBarDockContext::InitLoop()
{
    m_dragRect.Create();

    // get styles from bar
    m_dwDockStyle = m_pBar->m_dwDockStyle;
    m_dwStyle = m_pBar->m_dwStyle & CBRS_ALIGN_ANY;
    ASSERT(m_dwStyle != 0);

    // initialize state
    m_bForceFrame = m_bFlip = FALSE;
}

void CPlayerBarDockContext::CancelLoop()
{
    DrawFocusRect(TRUE);    // gets rid of focus rect
    m_dragRect.Destroy();
    ReleaseCapture();
}

/////////////////////////////////////////////////////////////////////////////
// Implementation

void CPlayerBarDockContext::DrawFocusRect(BOOL bRemoveRect)
{
    if (bRemoveRect) {
        m_dragRect.Hide();
        return;
    }

    // default to thin frame
    CSize size(AFX_CX_BORDER, AFX_CY_BORDER);
    bool bDocked = true;

    // determine new rect and size
    CRect rect;
    if (HORZF(m_dwOverDockStyle)) {
        rect = m_rectDragHorz;
    } else if (VERTF(m_dwOverDockStyle)) {
        rect = m_rectDragVert;
    } else {
        if ((HORZF(m_dwStyle) && !m_bFlip) || (VERTF(m_dwStyle) && m_bFlip)) {
            rect = m_rectFrameDragHorz;
        } else {
            rect = m_rectFrameDragVert;
        }
        // use thick frame instead, sized for the monitor the rect is on
        DpiHelper dpi;
        UINT rectDpi = DpiHelper::GetDPIForRect(&rect);
        dpi.Override(rectDpi, rectDpi);
        size.cx = dpi.GetSystemMetricsDPI(SM_CXFRAME) - AFX_CX_BORDER;
        size.cy = dpi.GetSystemMetricsDPI(SM_CYFRAME) - AFX_CY_BORDER;
        bDocked = false;
    }

    if (bDocked) {
        // looks better one pixel in (makes the bar look pushed down)
        rect.InflateRect(-AFX_CX_BORDER, -AFX_CY_BORDER);
    }

    m_dragRect.Show(rect, size, bDocked);
}

void CPlayerBarDockContext::UpdateState(BOOL* pFlag, BOOL bNewValue)
{
    if (*pFlag != bNewValue) {
        *pFlag = bNewValue;
        m_bFlip = (HORZF(m_dwDockStyle) && VERTF(m_dwDockStyle) && m_bFlip); // shift key
        m_dwOverDockStyle = (m_bForceFrame) ? 0 : CanDock();
        DrawFocusRect();
    }
}

BOOL CPlayerBarDockContext::Track()
{
    // don't handle if capture already set
    if (::GetCapture() != nullptr) {
        // Move/Stretch already showed the outline; the capture is not ours to release
        m_dragRect.Destroy();
        return FALSE;
    }

    // set capture to the window which received this message
    m_pBar->SetCapture();
    ASSERT(m_pBar == CWnd::GetCapture());

    // get messages until capture lost or cancelled/accepted
    while (CWnd::GetCapture() == m_pBar) {
        MSG msg;
        if (!::GetMessage(&msg, nullptr, 0, 0)) {
            AfxPostQuitMessage((int)msg.wParam);
            break;
        }

        switch (msg.message) {
            case WM_LBUTTONUP:
                if (m_bDragging) {
                    EndDrag();
                } else {
                    EndResize();
                }
                return TRUE;
            case WM_MOUSEMOVE:
                if (m_bDragging) {
                    Move(msg.pt);
                } else {
                    Stretch(msg.pt);
                }
                break;
            case WM_KEYUP:
                if (m_bDragging) {
                    OnKey((int)msg.wParam, FALSE);
                }
                break;
            case WM_KEYDOWN:
                if (m_bDragging) {
                    OnKey((int)msg.wParam, TRUE);
                }
                if (msg.wParam == VK_ESCAPE) {
                    CancelLoop();
                    return FALSE;
                }
                break;
            case WM_RBUTTONDOWN:
                CancelLoop();
                return FALSE;

            // just dispatch rest of the messages
            default:
                DispatchMessage(&msg);
                break;
        }
    }

    CancelLoop();

    return FALSE;
}
