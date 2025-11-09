#include "stdafx.h"
#include "CMPCThemeScrollBarHelper.h"
#include "CMPCTheme.h"
#include "CMPCThemeUtil.h"

CMPCThemeScrollBarHelper::CMPCThemeScrollBarHelper(CWnd* scrollWindow)
    :helperInfo(nullptr)
    ,setWindowRegionActive(false)
    ,regionCreated(false)
{
    window = scrollWindow;
    pParent = nullptr;
}


CMPCThemeScrollBarHelper::~CMPCThemeScrollBarHelper()
{
}

void CMPCThemeScrollBarHelper::createThemedScrollBars()
{
    pParent = window->GetParent();
    ScrollBarHelperInfo i(window);
    if (nullptr != pParent && IsWindow(pParent->m_hWnd)) {
        if (i.canVSB && !IsWindow(vertSB.m_hWnd)) {
            VERIFY(vertSB.Create(SBS_VERT | WS_CHILD |
                                 WS_VISIBLE, CRect(0, 0, 0, 0), pParent, 0));
            vertSB.setScrollWindow(window); //we want messages from this SB
        }

        if (i.canHSB && !IsWindow(horzSB.m_hWnd)) {
            VERIFY(horzSB.Create(SBS_HORZ | WS_CHILD |
                                 WS_VISIBLE, CRect(0, 0, 0, 0), pParent, 0));
            horzSB.ModifyStyleEx(WS_EX_LAYOUTRTL, 0); //don't inherit RTL--GetScrollInfo already gives us the correct positions from an RTL window
            horzSB.setScrollWindow(window); //we want messages from this SB
        }
    }
    hideNativeScrollBars();
}

void CMPCThemeScrollBarHelper::InvalidateScrollbarArea() {
    if (window) {
        CRect r;
        if (vertSB) {
            vertSB.GetWindowRect(r);
            window->ScreenToClient(r);
            window->InvalidateRect(r);
        }
        if (horzSB) {
            horzSB.GetWindowRect(r);
            window->ScreenToClient(r);
            window->InvalidateRect(r);
        }
    }
}

void CMPCThemeScrollBarHelper::OnWindowPosChanged() {
    {
        std::lock_guard<std::recursive_mutex> lck(helperMutex);
        //this is to prevent recursive calls to OnWindowPos due to SetWindowRgn
        if (!setWindowRegionActive) {
            hideNativeScrollBars();
        }
    }
}

void CMPCThemeScrollBarHelper::setWindowRegionExclusive(HRGN h) {
    {
        std::lock_guard<std::recursive_mutex> lck(helperMutex);
        setWindowRegionActive = true;
    }
    window->SetWindowRgn(h, false);
    regionCreated = (NULL != h);
    {
        std::lock_guard<std::recursive_mutex> lck(helperMutex);
        setWindowRegionActive = false;
    }
}

void CMPCThemeScrollBarHelper::hideNativeScrollBars()
{

    if (!CMPCThemeUtil::IsWindowVisibleAndRendered(window)) {
        if (IsWindow(vertSB.m_hWnd)) {
            vertSB.ShowWindow(SW_HIDE);
        }
        if (IsWindow(horzSB.m_hWnd)) {
            horzSB.ShowWindow(SW_HIDE);
        }
        return;
    }

    bool windowChanged = helperInfo.UpdateHelperInfo(window);

    ScrollBarHelperInfo& i = helperInfo;
    CRect wr = i.wr; 
    CRect horzRect, vertRect;
    bool needsRegion = false;
    CRect sbWRVert;
    CRect sbWRHorz;


    if (IsWindow(vertSB.m_hWnd)) {
        if (i.canVSB) {
            int width = i.sbThickness, height = wr.bottom - wr.top - 2 * i.borderThickness - (i.canHSB ? i.sbThickness : 0);
            needsRegion = true;

            vertRect = CRect(CPoint(i.wrOnParent.right - width - i.borderThickness, i.wrOnParent.top + i.borderThickness), CSize(width, height));
            vertSB.MoveWindow(vertRect);
            vertSB.ShowWindow(SW_SHOW);
            updateScrollInfo();
        } else {
            if (vertSB.IsWindowVisible()) {
                vertSB.GetWindowRect(sbWRVert);
                vertSB.ShowWindow(SW_HIDE);
                window->ScreenToClient(sbWRVert);
            }
        }
    }

    if (IsWindow(horzSB.m_hWnd)) {
        if (i.canHSB) {
            int height = i.sbThickness, width = wr.right - wr.left - 2 * i.borderThickness - (i.canVSB ? i.sbThickness : 0);
            needsRegion = true;

            horzRect = CRect(CPoint(i.wrOnParent.left + i.borderThickness, i.wrOnParent.bottom - height - i.borderThickness), CSize(width, height));
            horzSB.MoveWindow(horzRect);
            horzSB.ShowWindow(SW_SHOW);
            updateScrollInfo();
        } else {
            if (horzSB.IsWindowVisible()) {
                horzSB.GetWindowRect(sbWRHorz);
                horzSB.ShowWindow(SW_HIDE);
                window->ScreenToClient(sbWRHorz);
            }
        }
    }
    if (needsRegion) {
        if (windowChanged || !regionCreated) {
            HRGN contentRgn = CreateRectRgn(wr.left, wr.top, wr.right, wr.bottom);
            if (!vertRect.IsRectEmpty()) {
                ::MapWindowPoints(pParent->GetSafeHwnd(), window->GetSafeHwnd(), (LPPOINT)&vertRect, 2);
                vertRect += helperInfo.clientOffset;
                HRGN vertRgn = CreateRectRgnIndirect(vertRect);
                CombineRgn(contentRgn, contentRgn, vertRgn, RGN_DIFF);
            }
            if (!horzRect.IsRectEmpty()) {
                ::MapWindowPoints(pParent->GetSafeHwnd(), window->GetSafeHwnd(), (LPPOINT)&horzRect, 2);
                horzRect += helperInfo.clientOffset;
                HRGN horzRgn = CreateRectRgnIndirect(horzRect);
                CombineRgn(contentRgn, contentRgn, horzRgn, RGN_DIFF);
            }
            setWindowRegionExclusive(contentRgn);
        }
    } else {
        setWindowRegionExclusive(NULL);
    }
    if (!sbWRVert.IsRectEmpty()) window->RedrawWindow(sbWRVert, NULL, RDW_INVALIDATE | RDW_ALLCHILDREN);
    if (!sbWRHorz.IsRectEmpty()) window->RedrawWindow(sbWRHorz, NULL, RDW_INVALIDATE | RDW_ALLCHILDREN);
}

void CMPCThemeScrollBarHelper::updateScrollInfo(UpdateScrollInfoAction action /*= UpdateScrollInfoAction::SCROLL_NONE*/)
{
    if (IsWindow(vertSB.m_hWnd)) {
        vertSB.updateScrollInfo();
        if (action == SCROLL_INVALIDATE) {
            vertSB.Invalidate();
        } else if (action == SCROLL_REDRAW) {
            vertSB.RedrawWindow();
        }
    }
    if (IsWindow(horzSB.m_hWnd)) {
        horzSB.updateScrollInfo();
        if (action == SCROLL_INVALIDATE) {
            horzSB.Invalidate();
        } else if (action == SCROLL_REDRAW) {
            horzSB.RedrawWindow();
        }
    }
}


//clistctrl does not seem to scroll when receiving thumb messages, so we handle them here
//this will allow the scrollbar to update as well
//inspired by flyhigh https://www.codeproject.com/Articles/14724/Replace-a-Window-s-Internal-Scrollbar-with-a-custo
//changed to avoid glitchy redraws and only update the scrollbar that has been changed
bool CMPCThemeScrollBarHelper::WindowProc(CListCtrl* list, UINT message, WPARAM wParam, LPARAM lParam)
{
    if (message == WM_VSCROLL || message == WM_HSCROLL) {
        WORD sbCode = LOWORD(wParam);
        if (sbCode == SB_THUMBTRACK || sbCode == SB_THUMBPOSITION) {
            SCROLLINFO siv = { 0 };
            siv.cbSize = sizeof(SCROLLINFO);
            siv.fMask = SIF_ALL;
            SCROLLINFO sih = siv;
            int nPos = HIWORD(wParam);
            CRect rcClient;
            list->GetClientRect(&rcClient);

            SIZE sizeAll;
            SIZE size = { 0, 0 };
            if (WM_VSCROLL == message) {
                list->GetScrollInfo(SB_VERT, &siv);
                if (siv.nPage == 0) {
                    sizeAll.cy = rcClient.bottom;
                } else {
                    sizeAll.cy = rcClient.bottom * (siv.nMax + 1) / siv.nPage;
                }
                size.cy = sizeAll.cy * (nPos - siv.nPos) / (siv.nMax + 1);
            } else {
                list->GetScrollInfo(SB_HORZ, &sih);
                if (sih.nPage == 0) {
                    sizeAll.cx = rcClient.right;
                } else {
                    sizeAll.cx = rcClient.right * (sih.nMax + 1) / sih.nPage;
                }
                size.cx = sizeAll.cx * (nPos - sih.nPos) / (sih.nMax + 1);
            }
            //adipose: this code is needed to prevent listctrl glitchy drawing.
            //scroll sends a cascade of redraws which are untenable during a thumb drag
            //only one redraw per scroll call this way
            if (nullptr != pParent && IsWindow(pParent->m_hWnd)) {
                pParent->SetRedraw(FALSE);
                list->Scroll(size);
                pParent->SetRedraw();
                list->Invalidate();
                CHeaderCtrl* hdrCtrl = list->GetHeaderCtrl();
                if (nullptr != hdrCtrl) {
                    hdrCtrl->Invalidate();
                }
            }
            return true; //processed
        }
    }
    return false;
}

bool CMPCThemeScrollBarHelper::WindowProc(CTreeCtrl* tree, UINT message, WPARAM wParam, LPARAM lParam)
{
    if (message == WM_VSCROLL || message == WM_HSCROLL) {
        WORD sbCode = LOWORD(wParam);
        if (sbCode == SB_THUMBTRACK || sbCode == SB_THUMBPOSITION) {
            SCROLLINFO siv = { 0 };
            siv.cbSize = sizeof(SCROLLINFO);
            siv.fMask = SIF_ALL;
            SCROLLINFO sih = siv;
            int nPos = HIWORD(wParam);
            CRect rcClient;
            tree->GetClientRect(&rcClient);
            tree->GetScrollInfo(SB_VERT, &siv);
            tree->GetScrollInfo(SB_HORZ, &sih);

            WPARAM wp = (WPARAM) - 1;
            int lines = 0;
            if (WM_VSCROLL == message) {
                wp = nPos < siv.nPos ? SB_LINEUP : SB_LINEDOWN;
                lines = abs(nPos - siv.nPos);
            } else {
                wp = nPos < sih.nPos ? SB_LINELEFT : SB_LINERIGHT;
                lines = abs(nPos - sih.nPos);
            }

            if (-1 != wp && nullptr != pParent && IsWindow(pParent->m_hWnd)) {
                pParent->SetRedraw(FALSE);
                while (lines-- > 0) {
                    tree->SendMessage(message, wp, 0);
                }
                pParent->SetRedraw();
                tree->Invalidate();
            }
            return true; //processed
        }
    }
    return false;
}

void CMPCThemeScrollBarHelper::themedNcPaintWithSB()
{
    createThemedScrollBars();
    doNcPaint(window);
}

void CMPCThemeScrollBarHelper::themedNcPaint(CWnd* window, CMPCThemeScrollable* swindow)
{
    if (window->GetStyle() & WS_HSCROLL) {
        SCROLLBARINFO sbHorz = { sizeof(SCROLLBARINFO) };
        if (window->GetScrollBarInfo(OBJID_HSCROLL, &sbHorz)) {
            if (0 == (sbHorz.rgstate[0] & (STATE_SYSTEM_INVISIBLE | STATE_SYSTEM_UNAVAILABLE))) {
                swindow->doDefault(); //required to get the horz sb buttons to draw in listctrl
            }
        }
    }

    doNcPaint(window);
    if (window->GetStyle() & WS_VSCROLL) {
        window->SetScrollPos(SB_VERT, window->GetScrollPos(SB_VERT), TRUE);
    }
}

ScrollBarHelperInfo::ScrollBarHelperInfo(CWnd* w):
    wr(), corner(), wrOnParent()
    ,sbThickness(0), borderThickness(0)
    ,canVSB(false), canHSB(false), needsSBCorner(false)
{
    if (w && w->m_hWnd && IsWindow(w->m_hWnd)) {
        w->GetWindowRect(wr);

        wrOnParent = wr;
        CWnd* pParent = w->GetParent();
        if (nullptr != pParent) {
            pParent->ScreenToClient(wrOnParent);
        }

        wr.OffsetRect(-wr.left, -wr.top);

        sbThickness = GetSystemMetrics(SM_CXVSCROLL);
        clientOffset = CMPCThemeUtil::GetClientRectOffset(w);
        borderThickness = clientOffset.x;

        auto style = w->GetStyle();
        //we have to draw vertical scrollbar because ncpaint is overridden to handle horizontal scrollbar
        //windows dark theme horizontal scrollbar is broken
        //exceptions: SB simply disappears if window is less than border thickness
        canVSB = 0 != (style & WS_VSCROLL) && sbThickness < wr.Width() - borderThickness * 2;
        canHSB = 0 != (style & WS_HSCROLL) && sbThickness < wr.Height() - borderThickness * 2;
        needsSBCorner = (style & (WS_VSCROLL | WS_HSCROLL)) == (WS_VSCROLL | WS_HSCROLL) && canVSB && canHSB;
        if (CMPCThemeUtil::IsRTL(w)) {
            corner = { wr.left + borderThickness, wr.bottom - sbThickness - borderThickness, wr.left + sbThickness + borderThickness,  wr.bottom - borderThickness };
        } else {
            corner = { wr.right - sbThickness - borderThickness, wr.bottom - sbThickness - borderThickness,  wr.right - borderThickness, wr.bottom - borderThickness };
        }
    }
}

bool ScrollBarHelperInfo::UpdateHelperInfo(CWnd* w) {
    ScrollBarHelperInfo tmp(w);
    if (tmp == *this) {
        return false;
    }
    *this = tmp;
    return true;
}

bool ScrollBarHelperInfo::operator==(ScrollBarHelperInfo& other) {
    return other.borderThickness == borderThickness && other.sbThickness == sbThickness //dimensions
        && other.wr == wr && other.wrOnParent == wrOnParent && other.corner == corner //rects
        && other.clientOffset == clientOffset
        && other.canHSB == canHSB && other.canVSB == canVSB && other.needsSBCorner == needsSBCorner //bools
        ;
}

void CMPCThemeScrollBarHelper::doNcPaint(CWnd* window)
{
    HRGN currentRgn = CreateRectRgn(0, 0, 0, 0);
    int rType = window->GetWindowRgn(currentRgn);
    window->SetWindowRgn(NULL, false);

    CWindowDC dc(window);
    int oldDC = dc.SaveDC();

    CRect clip;
    ScrollBarHelperInfo i(window);

    CRect &wr = i.wr;

    clip = wr; //client rect is insufficient to clip scrollbars
    clip.DeflateRect(i.borderThickness, i.borderThickness);
    dc.ExcludeClipRect(clip);
    CBrush brush(CMPCTheme::WindowBorderColorLight); //color used for column sep in explorer
    dc.FillSolidRect(wr, CMPCTheme::ContentBGColor);
    dc.FrameRect(wr, &brush);

    dc.RestoreDC(oldDC);
    if (i.needsSBCorner) {
        dc.FillSolidRect(i.corner, CMPCTheme::ContentBGColor);
    }

    if (rType == COMPLEXREGION || rType == SIMPLEREGION) {
        window->SetWindowRgn(currentRgn, false);
    }
}

