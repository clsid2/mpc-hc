#include "stdafx.h"
#include "CMPCThemePlayerListCtrl.h"
#include "CMPCTheme.h"
#include "CMPCThemeUtil.h"
#include "mplayerc.h"
#undef SubclassWindow

CMPCThemePlayerListCtrl::ColumnCache CMPCThemePlayerListCtrl::s_columnCache;

CMPCThemePlayerListCtrl::CMPCThemePlayerListCtrl() : CListCtrl(), CMPCThemeScrollBarRenderer()
{
    themeGridLines = false;
    fullRowSelect = false;
    clipChildWindows = false;
    hasCheckedColors = false;
    hasCBImages = false;
    customThemeInterface = nullptr;

}


CMPCThemePlayerListCtrl::~CMPCThemePlayerListCtrl()
{
}


void CMPCThemePlayerListCtrl::PreSubclassWindow()
{
    if (!AppNeedsThemedControls()) {
        EnableToolTips(TRUE);
    } else {
        if (CMPCThemeUtil::canUseWin10DarkTheme()) {
            //SetWindowTheme(GetSafeHwnd(), L"DarkMode_Explorer", NULL);
        } else {
            SetWindowTheme(GetSafeHwnd(), L"", NULL);
        }
        CToolTipCtrl* t = GetToolTips();
        if (nullptr != t) {
            lvsToolTip.SubclassWindow(t->m_hWnd);
        }
        subclassHeader();
    }
    CListCtrl::PreSubclassWindow();
}

IMPLEMENT_DYNAMIC(CMPCThemePlayerListCtrl, CListCtrl)

BEGIN_MESSAGE_MAP(CMPCThemePlayerListCtrl, CListCtrl)
    ON_WM_PAINT()
    ON_WM_NCPAINT()
    ON_WM_CREATE()
    ON_WM_MOUSEMOVE()
    ON_WM_MOUSEWHEEL()
    ON_WM_ERASEBKGND()
    ON_WM_CTLCOLOR()
END_MESSAGE_MAP()

void CMPCThemePlayerListCtrl::subclassHeader()
{
    CHeaderCtrl* t = GetHeaderCtrl();
    if (nullptr != t && IsWindow(t->m_hWnd) && themedHdrCtrl.m_hWnd == NULL) {
        themedHdrCtrl.SetParent(this);
        themedHdrCtrl.SubclassWindow(t->GetSafeHwnd());
    }
}

void CMPCThemePlayerListCtrl::ExcludeChildWindows(CDC* pDC, CRgn* pClipRgn) {
    if (clipChildWindows) {
        // Enumerate all child windows and exclude them from clipping (except the header)
        CWnd* pChild = GetWindow(GW_CHILD);
        while (pChild != NULL) {
            // Check if child window is visible
            if (pChild->IsWindowVisible() && pChild != &themedHdrCtrl) {
                CRect childRect;
                pChild->GetWindowRect(&childRect);
                ScreenToClient(&childRect);

                // Create region for this child window
                CRgn childRgn;
                childRgn.CreateRectRgnIndirect(&childRect);

                // Exclude this child from our clipping region
                pClipRgn->CombineRgn(pClipRgn, &childRgn, RGN_DIFF);
            }

            pChild = pChild->GetWindow(GW_HWNDNEXT);
        }
    }
}

CPoint DetectHeaderOffset(CListCtrl* pList, CHeaderCtrl* pHeader) {
    CRect columnRect, headerItemRect, headerWindowRect;

    if (!pList->GetSubItemRect(0, 0, LVIR_BOUNDS, columnRect) ||
        !pHeader->GetItemRect(0, headerItemRect)) {
        return CPoint(0, 0);
    }

    pHeader->GetWindowRect(&headerWindowRect);
    pList->ScreenToClient(&headerWindowRect);

    int offset = columnRect.left - (headerWindowRect.left + headerItemRect.left);
    return CPoint(offset, 0);
}

bool CMPCThemePlayerListCtrl::IsCustomDrawActive() {
    if (!::IsWindow(m_hWnd)) {
        return false;
    }

    CWnd* pParent = GetParent();
    if (!pParent) {
        return false;
    }

    NMLVCUSTOMDRAW nmcd = { 0 };
    nmcd.nmcd.hdr.hwndFrom = m_hWnd;
    nmcd.nmcd.hdr.idFrom = GetDlgCtrlID();
    nmcd.nmcd.hdr.code = NM_CUSTOMDRAW;
    nmcd.nmcd.dwDrawStage = CDDS_PREPAINT;

    LRESULT lResult = pParent->SendMessage(WM_NOTIFY,  nmcd.nmcd.hdr.idFrom,  (LPARAM)&nmcd);

    return (lResult != CDRF_DODEFAULT);
}

bool CMPCThemePlayerListCtrl::PaintHooksActive() {
    return (GetStyle() & (LVS_OWNERDRAWFIXED)) != 0 || IsCustomDrawActive();
}

void CMPCThemePlayerListCtrl::RedrawHeader(CRect headerRect) {
    if (themedHdrCtrl) {
        ScreenToClient(&headerRect);
        RedrawWindow(headerRect, 0, RDW_INVALIDATE);
    }
}

inline CHeaderCtrl* CMPCThemePlayerListCtrl::GetHeaderFast() {
    if (themedHdrCtrl) {
        return &themedHdrCtrl;
    } else {
        return GetHeaderCtrl();
    }
}

void CMPCThemePlayerListCtrl::OnPaint() {
    if (AppNeedsThemedControls() && !PaintHooksActive()) {
        CPaintDC dc(this);
        int dcCfg = dc.SaveDC();

        CRect updateRect;
        dc.GetClipBox(&updateRect);
        if (updateRect.IsRectEmpty()) {
            return;
        }

        // Setup
        CRect clientRect;
        GetClientRect(&clientRect);
        CPoint clientTopLeft(0, 0);
        ClientToScreen(&clientTopLeft);
        if (nullptr != customThemeInterface) {
            customThemeInterface->DoCustomPrePaint();
        }

        // Calculate areas
        CRect listArea = clientRect;
        bool hasHeader = themedHdrCtrl && themedHdrCtrl.IsWindowVisible();
        CRect headerRect(0, 0, 0, 0);
        if (hasHeader) {
            themedHdrCtrl.GetWindowRect(&headerRect);
            ScreenToClient(&headerRect);
            listArea.top = headerRect.bottom;
        }

        // Create single buffer for entire client area
        m_listBuffer.EnsureInitialized(&dc, clientRect.Size(), GetFont());


        // Draw list items
        CRect listDrawRect;
        listDrawRect.IntersectRect(&updateRect, &listArea);
        if (!listDrawRect.IsRectEmpty()) {
            EraseBkgnd(&m_listBuffer.memDC, listDrawRect);
            DrawAllItems(&m_listBuffer.memDC, listDrawRect);
        }

        // Draw header if needed
        if (hasHeader) {
            CRect headerDrawRect;
            headerDrawRect.IntersectRect(&updateRect, &headerRect);
            if (!headerDrawRect.IsRectEmpty()) {
                CPoint headerOffset = DetectHeaderOffset(this, &themedHdrCtrl);
                headerOffset += headerRect.TopLeft(); // Add position offset
                themedHdrCtrl.DrawAllItems(&m_listBuffer.memDC, headerOffset, headerDrawRect);
                MapWindowPoints(&themedHdrCtrl, headerDrawRect);
                themedHdrCtrl.ValidateRect(headerDrawRect);
            }
        }

        CRgn clipRgn;
        clipRgn.CreateRectRgnIndirect(&updateRect);
        ExcludeChildWindows(&dc, &clipRgn);
        dc.SelectClipRgn(&clipRgn);

        dc.BitBlt(updateRect.left, updateRect.top, updateRect.Width(), updateRect.Height(),  &m_listBuffer.memDC, updateRect.left, updateRect.top, SRCCOPY);


        dc.RestoreDC(dcCfg);
    } else {
        __super::OnPaint();
    }
}
void CMPCThemePlayerListCtrl::setAdditionalStyles(DWORD styles, bool exStyle /* = true*/)
{
    if (AppNeedsThemedControls()) {
        DWORD stylesToAdd = styles, stylesToRemove = 0;
        bool dummy;

        auto handleStyle = [&](DWORD style, bool& flag) {
          if (styles & style) {
            stylesToAdd &= ~style;
            stylesToRemove |= style;
            flag = true;
          }
        };


        if (styles & LVS_EX_FULLROWSELECT) {
            //we need these to remain, or else other columns may not get refreshed on a selection change.
            //no regressions observed yet, but unclear why we removed this style for custom draw previously
            //error was observed with playersubresyncbar
            //handleStyle(LVS_EX_FULLROWSELECT, fullRowSelect);
          fullRowSelect = true;
        }

        handleStyle(LVS_EX_GRIDLINES, themeGridLines);
        handleStyle(LVS_EX_DOUBLEBUFFER, dummy);
        handleStyle(WS_CLIPCHILDREN, clipChildWindows);

        if (exStyle) {
            SetExtendedStyle((GetExtendedStyle() | stylesToAdd) & ~stylesToRemove);
        } else {
            SetWindowLongPtrW(m_hWnd, GWL_STYLE, (GetStyle() | stylesToAdd) & ~stylesToRemove);
        }
    } else {
        if (exStyle) {
            SetExtendedStyle(GetExtendedStyle() | styles);
        } else {
            SetWindowLongPtrW(m_hWnd, GWL_STYLE, GetStyle() | styles);
        }
    }
}

void CMPCThemePlayerListCtrl::setHasCBImages(bool on)
{
    hasCBImages = on;
}

void CMPCThemePlayerListCtrl::setItemTextWithDefaultFlag(int nItem, int nSubItem, LPCTSTR lpszText, bool flagged)
{
    SetItemText(nItem, nSubItem, lpszText);
    setFlaggedItem(nItem, flagged);
}

void CMPCThemePlayerListCtrl::setFlaggedItem(int iItem, bool flagged)
{
    flaggedItems[iItem] = flagged;
}

bool CMPCThemePlayerListCtrl::getFlaggedItem(int iItem)
{
    auto it = flaggedItems.find(iItem);
    if (it != flaggedItems.end()) {
        return it->second;
    } else {
        return false;
    }
}

void CMPCThemePlayerListCtrl::DoDPIChanged()
{
    if (listMPCThemeFontBold.m_hObject) {
        listMPCThemeFontBold.DeleteObject();
    }

}


BOOL CMPCThemePlayerListCtrl::PreTranslateMessage(MSG* pMsg)
{
    if (AppNeedsThemedControls()) {
        if (!IsWindow(themedToolTip.m_hWnd)) {
            themedToolTip.Create(this, TTS_ALWAYSTIP);
            themedToolTip.enableFlickerHelper();
        }
        if (IsWindow(themedToolTip.m_hWnd)) {
            themedToolTip.RelayEvent(pMsg);
        }
    }
    return __super::PreTranslateMessage(pMsg);
}

void CMPCThemePlayerListCtrl::setCheckedColors(COLORREF checkedBG, COLORREF checkedText, COLORREF uncheckedText)
{
    checkedBGClr = checkedBG;
    checkedTextClr = checkedText;
    uncheckedTextClr = uncheckedText;
    hasCheckedColors = true;
}

void CMPCThemePlayerListCtrl::OnNcPaint() {
    if (AppNeedsThemedControls()) {
        HandleNcPaint(m_hWnd);
    } else {
        __super::OnNcPaint();
    }
}

int CMPCThemePlayerListCtrl::OnCreate(LPCREATESTRUCT lpCreateStruct)
{
    if (__super::OnCreate(lpCreateStruct) == -1) {
        return -1;
    }

    if (AppNeedsThemedControls()) {
        SetBkColor(CMPCTheme::ContentBGColor);
        subclassHeader();
    }

    return 0;
}

LRESULT CMPCThemePlayerListCtrl::WindowProc(UINT message, WPARAM wParam, LPARAM lParam)
{
    if (AppNeedsThemedControls()) {
        CMPCThemeScrollBarRenderer::ProcessMessage(m_hWnd, message, wParam, lParam);
    }
    LRESULT result = __super::WindowProc(message, wParam, lParam);
    return result;
}

void CMPCThemePlayerListCtrl::updateToolTip(CPoint point)
{
    if (AppNeedsThemedControls() && nullptr != themedToolTip) {
        TOOLINFO ti = { 0 };
        UINT_PTR tid = OnToolHitTest(point, &ti);
        //OnToolHitTest returns -1 on failure but doesn't update uId to match

        if (tid == -1 || themedToolTipCid != ti.uId) { //if no tooltip, or id has changed, remove old tool
            if (themedToolTip.GetToolCount() > 0) {
                themedToolTip.DelTool(this);
                themedToolTip.Activate(FALSE);
            }
            themedToolTipCid = (UINT_PTR) - 1;
        }

        if (tid != -1 && themedToolTipCid != ti.uId && 0 != ti.uId) {

            themedToolTipCid = ti.uId;

            CRect cr;
            GetClientRect(&cr); //we reset the tooltip every time we move anyway, so this rect is adequate

            themedToolTip.AddTool(this, LPSTR_TEXTCALLBACK, &cr, ti.uId);
            themedToolTip.Activate(TRUE);
        }
    }
}

void CMPCThemePlayerListCtrl::OnMouseMove(UINT nFlags, CPoint point)
{
    __super::OnMouseMove(nFlags, point);
    updateToolTip(point);
}


BOOL CMPCThemePlayerListCtrl::OnMouseWheel(UINT nFlags, short zDelta, CPoint pt)
{
    BOOL ret = __super::OnMouseWheel(nFlags, zDelta, pt);
    ScreenToClient(&pt);
    updateToolTip(pt);
    return ret;
}

void CMPCThemePlayerListCtrl::drawItem(CDC* pDC, int nItem, int nSubItem, CRect rRow, DWORD dwStyle, DWORD extendedStyle, UINT itemState, bool isChecked, CImageList* smallImageList, UINT cbResID)
{
    CRect rect, rIcon, rText, rTextBG, rectDC, rClient;
    GetClientRect(rClient);
    //GetItemRect(nItem, rRow, LVIR_BOUNDS);
    GetSubItemRectFast(nItem, nSubItem, LVIR_LABEL, rText, rRow);
    GetSubItemRectFast(nItem, nSubItem, LVIR_ICON, rIcon, rRow);
    GetSubItemRectFast(nItem, nSubItem, LVIR_BOUNDS, rect, rRow);

    if (0 == nSubItem) { //getsubitemrect gives whole row for 0/LVIR_BOUNDS.  but LVIR_LABEL is limited to text bounds.  MSDN undocumented behavior
        rect.right = rText.right;
    }

    CFont* curFont = nullptr;

    //issubitemvisible
    if (rClient.left <= rect.right && rClient.right >= rect.left && rClient.top <= rect.bottom && rClient.bottom >= rect.top) {
        COLORREF textColor = CMPCTheme::TextFGColor;
        COLORREF bgColor = CMPCTheme::ContentBGColor;
        COLORREF selectedBGColor = CMPCTheme::ContentSelectedColor;

        COLORREF oldTextColor = pDC->GetTextColor();
        COLORREF oldBkColor = pDC->GetBkColor();

        CString text = GetCachedText(nItem, nSubItem);
        if (nullptr != customThemeInterface) { //subclasses can override colors here
            bool overrideSelectedBG = false;
            customThemeInterface->GetCustomTextColors(nItem, nSubItem, textColor, bgColor, overrideSelectedBG);
            if (overrideSelectedBG) {
                selectedBGColor = bgColor;
            }
        }

        pDC->SetTextColor(textColor);
        pDC->SetBkColor(bgColor);

        rectDC = rRow;

        if (!IsWindowEnabled() && 0 == nSubItem) { //no gridlines, bg for full row
            pDC->FillSolidRect(rRow, CMPCTheme::ListCtrlDisabledBGColor);
        } else {
            pDC->FillSolidRect(rect, CMPCTheme::ContentBGColor); //no flicker because we have a memory dc
        }

        rTextBG = rText;
        int align = DT_LEFT;
        if (nSubItem < s_columnCache.columns.size()) {
            align = s_columnCache.columns[nSubItem].align;
        }

        UINT textFormat = DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS | DT_NOPREFIX;
        if (align == HDF_CENTER) {
            textFormat |= DT_CENTER;
        } else if (align == HDF_LEFT) {
            textFormat |= DT_LEFT;
            if (nSubItem == 0) {//less indent for first column
                rText.left += 2;
            } else {
                rText.left += 6;
            }
        } else {
            textFormat |= DT_RIGHT;
            rText.right -= 6;
        }

        int contentLeft = rText.left;
        if (rIcon.Width() > 0) {
            if (nSubItem == 0) {
                int imageIndex = GetCachedImageIndex(nItem);

                contentLeft = rIcon.left;
                if (hasCBImages) { //draw manually to match theme
                    rIcon.DeflateRect(0, 0, 1, 0);
                    if (rIcon.Height() > rIcon.Width()) {
                        rIcon.DeflateRect(0, (rIcon.Height() - rIcon.Width()) / 2); //as tall as wide
                    }

                    CMPCThemeUtil::drawCheckBox(GetParent(), imageIndex, false, false, rIcon, pDC);
                } else {
                    if (dwStyle == LVS_ICON) {
                    } else if (smallImageList) {
                        int cx, cy;
                        ImageList_GetIconSize(smallImageList->m_hImageList, &cx, &cy);
                        rIcon.top += (rIcon.Height() - cy) / 2;
                        smallImageList->Draw(pDC, imageIndex, rIcon.TopLeft(), ILD_TRANSPARENT);
                    }
                }
                if (align == HDF_LEFT) {
                    rText.left += 2;    //more ident after image
                }
            }
        }

        if ((extendedStyle & LVS_EX_CHECKBOXES) && INDEXTOSTATEIMAGEMASK(0) != (itemState & LVIS_STATEIMAGEMASK)) {
            if (nSubItem == 0) {
                int cbSize = GetSystemMetrics(SM_CXMENUCHECK);
                int cbYMargin = (rect.Height() - cbSize - 1) / 2;
                int cbXMargin = (contentLeft - rect.left - cbSize) / 2;
                CRect rcb = { rect.left + cbXMargin, rect.top + cbYMargin, rect.left + cbXMargin + cbSize, rect.top + cbYMargin + cbSize };
                CMPCThemeUtil::drawCheckBox(GetParent(), isChecked, false, true, rcb, pDC, false, cbResID);
            }
        }

        if (IsWindowEnabled()) {
            bool selected = false;
            if ((itemState & LVIS_SELECTED) == LVIS_SELECTED && (nSubItem == 0 || fullRowSelect) && (GetStyle() & LVS_SHOWSELALWAYS || GetFocus() == this)) {
                bgColor = selectedBGColor;
                if (LVS_REPORT != dwStyle) { //in list mode we don't fill the "whole" column
                    CRect tmp = rText;
                    pDC->DrawTextW(text, tmp, textFormat | DT_CALCRECT); //end of string
                    rTextBG.right = tmp.right + (rText.left - rTextBG.left); //end of string plus same indent from the left side
                }
                selected = true;
            } else if (hasCheckedColors) {
                if (isChecked && checkedBGClr != -1) {
                    bgColor = checkedBGClr;
                }
                if (isChecked && checkedTextClr != -1) {
                    pDC->SetTextColor(checkedTextClr);
                }
                if (!isChecked && uncheckedTextClr != -1) {
                    pDC->SetTextColor(uncheckedTextClr);
                }
            }
            pDC->FillSolidRect(rTextBG, bgColor);

            if (themeGridLines || (nullptr != customThemeInterface && customThemeInterface->UseCustomGrid())) {
                CRect rGrid = rect;
                rGrid.bottom -= 1;
                CPen gridPenV, gridPenH, *oldPen;
                if (nullptr != customThemeInterface && customThemeInterface->UseCustomGrid()) {
                    COLORREF horzGridColor, vertGridColor;
                    customThemeInterface->GetCustomGridColors(nItem, horzGridColor, vertGridColor);
                    gridPenV.CreatePen(PS_SOLID, 1, vertGridColor);
                    gridPenH.CreatePen(PS_SOLID, 1, horzGridColor);
                } else {
                    gridPenV.CreatePen(PS_SOLID, 1, CMPCTheme::ListCtrlGridColor);
                    gridPenH.CreatePen(PS_SOLID, 1, CMPCTheme::ListCtrlGridColor);
                }

                oldPen = pDC->SelectObject(&gridPenV);
                if (nSubItem != 0) {
                    pDC->MoveTo(rGrid.TopLeft());
                    pDC->LineTo(rGrid.left, rGrid.bottom);
                } else {
                    pDC->MoveTo(rGrid.left, rGrid.bottom);
                }

                pDC->SelectObject(&gridPenH);
                pDC->LineTo(rGrid.BottomRight());

                pDC->SelectObject(&gridPenV);
                pDC->LineTo(rGrid.right, rGrid.top);

                pDC->SelectObject(oldPen);
                gridPenV.DeleteObject();
                gridPenH.DeleteObject();
            } else if (selected) {
                CBrush borderBG;
                borderBG.CreateSolidBrush(CMPCTheme::ListCtrlDisabledBGColor);
                pDC->FrameRect(rTextBG, &borderBG);
                borderBG.DeleteObject();
            }
        }

        if (getFlaggedItem(nItem)) { //could be a setting, but flagged items are bold for now
            if (!listMPCThemeFontBold.m_hObject) {
                listMPCThemeFont = GetFont();
                LOGFONT lf;
                listMPCThemeFont->GetLogFont(&lf);
                lf.lfWeight = FW_BOLD;
                listMPCThemeFontBold.CreateFontIndirect(&lf);
            }

            curFont = pDC->GetCurrentFont();
            pDC->SelectObject(listMPCThemeFontBold);
        }
        pDC->DrawTextW(text, rText, textFormat);
        pDC->SetTextColor(oldTextColor);
        pDC->SetBkColor(oldBkColor);
        if (curFont) {
            pDC->SelectObject(curFont);
        }
    }
}

BOOL CMPCThemePlayerListCtrl::OnCustomDraw(NMHDR* pNMHDR, LRESULT* pResult)
{
#if 0
    if (AppNeedsThemedControls()) {
        NMLVCUSTOMDRAW* pLVCD = reinterpret_cast<NMLVCUSTOMDRAW*>(pNMHDR);

        *pResult = CDRF_DODEFAULT;
        if (pLVCD->nmcd.dwDrawStage == CDDS_PREPAINT) {
            if (nullptr != customThemeInterface) {
                customThemeInterface->DoCustomPrePaint();
            }
            *pResult = CDRF_NOTIFYITEMDRAW;
        } else if (pLVCD->nmcd.dwDrawStage == CDDS_ITEMPREPAINT) {
            DWORD dwStyle = GetStyle() & LVS_TYPEMASK;
            if (LVS_REPORT == dwStyle) {
                *pResult = CDRF_NOTIFYSUBITEMDRAW;
            } else {
                int nItem = static_cast<int>(pLVCD->nmcd.dwItemSpec);
                CDC* pDC = CDC::FromHandle(pLVCD->nmcd.hdc);
                drawItem(pDC, nItem, 0);
                *pResult = CDRF_SKIPDEFAULT;
            }
        } else if (pLVCD->nmcd.dwDrawStage == (CDDS_ITEMPREPAINT | CDDS_SUBITEM)) {
            if (GetStyle() & LVS_OWNERDRAWFIXED) {
                //found that for ownerdraw routines, we can end up here and draw both ways on hover/tooltip. this should prevent it
                *pResult = CDRF_DODEFAULT;
            } else {
                int nItem = static_cast<int>(pLVCD->nmcd.dwItemSpec);
                if (IsItemVisible(nItem)) {
                    int nSubItem = pLVCD->iSubItem;
                    CDC* pDC = CDC::FromHandle(pLVCD->nmcd.hdc);
                    drawItem(pDC, nItem, nSubItem);
                }
                *pResult = CDRF_SKIPDEFAULT;
            }
        }
        return TRUE;
    }
#endif
    return FALSE;
}


BOOL CMPCThemePlayerListCtrl::OnEraseBkgnd(CDC* pDC) {
    if (AppNeedsThemedControls() && !PaintHooksActive()) {
        return TRUE;
    } else {
        CRect updateRect;
        pDC->GetClipBox(&updateRect);
        return EraseBkgnd(pDC, updateRect);
    }
}


BOOL CMPCThemePlayerListCtrl::EraseBkgnd(CDC* pDC, CRect updateRect) {
    if (AppNeedsThemedControls()) {
        CRect r;
        GetClientRect(r);

        int dcState = pDC->SaveDC();
        int topIndex = GetTopIndex();
        int visibleCount = GetCountPerPage();
        int startItem = std::max(0, topIndex - 1);
        int endItem = std::min(GetItemCount() - 1, topIndex + visibleCount + 1);

        std::vector<CRect> exclusionRegions;

        if (endItem >= startItem) {
            CRect combinedRect;
            bool hasCombinedRect = false;

            for (int y = startItem; y <= endItem; y++) {
                CRect itemRect;
                GetItemRect(y, itemRect, LVIR_BOUNDS);

                // Only process if item intersects update region
                if (itemRect.bottom >= updateRect.top && itemRect.top <= updateRect.bottom) {
                    if (!hasCombinedRect) {
                        // Start a new combined rectangle
                        combinedRect = itemRect;
                        hasCombinedRect = true;
                    } else {
                        // Check if this item is adjacent to the previous combined rectangle
                        if (itemRect.top <= combinedRect.bottom + 1 &&
                            itemRect.left == combinedRect.left &&
                            itemRect.right == combinedRect.right) {
                            // Extend the combined rectangle downward
                            combinedRect.bottom = itemRect.bottom;
                        } else {
                            // Gap found - save the current combined rectangle and start a new one
                            exclusionRegions.push_back(combinedRect);
                            combinedRect = itemRect;
                        }
                    }
                } else if (hasCombinedRect) {
                    // Item doesn't intersect - break the combination
                    exclusionRegions.push_back(combinedRect);
                    hasCombinedRect = false;
                }
            }

            // Add the final combined rectangle if we have one
            if (hasCombinedRect) {
                exclusionRegions.push_back(combinedRect);
            }
        }

        // Apply the combined exclusion regions
        for (const auto& region : exclusionRegions) {
            pDC->ExcludeClipRect(region);
        }

        pDC->FillSolidRect(r, CMPCTheme::ContentBGColor);

        // Grid line drawing remains the same...
        if (themeGridLines || (nullptr != customThemeInterface && customThemeInterface->UseCustomGrid())) {
            CPen gridPen, * oldPen;
            gridPen.CreatePen(PS_SOLID, 1, CMPCTheme::ListCtrlGridColor);
            oldPen = pDC->SelectObject(&gridPen);

            if (GetItemCount() > 0) {
                // Vertical grid lines - clip to update region
                for (int x = 0; x < themedHdrCtrl.GetItemCount(); x++) {
                    CRect gr;
                    themedHdrCtrl.GetItemRect(x, gr);

                    if (gr.right >= updateRect.left && gr.right <= updateRect.right) {
                        pDC->MoveTo(gr.right, std::max(r.top, updateRect.top));
                        pDC->LineTo(gr.right, std::min(r.bottom, updateRect.bottom));
                    }
                }

                // Horizontal grid lines for visible rows
                CRect gr;
                for (int y = startItem; y <= endItem || gr.bottom < std::min(r.bottom, updateRect.bottom); y++) {
                    if (y >= GetItemCount()) {
                        if (gr.IsRectEmpty()) break;
                        gr.OffsetRect(0, gr.Height());
                    } else {
                        GetItemRect(y, gr, LVIR_BOUNDS);
                    }

                    if (gr.bottom >= updateRect.top && gr.bottom <= updateRect.bottom) {
                        CPen horzPen;
                        int lineY = gr.bottom - 1;
                        int lineLeft = std::max(r.left, updateRect.left);
                        int lineRight = std::min(r.right, updateRect.right);

                        pDC->MoveTo(lineLeft, lineY);

                        if (nullptr != customThemeInterface && customThemeInterface->UseCustomGrid()) {
                            COLORREF horzGridColor, tmp;
                            customThemeInterface->GetCustomGridColors(y, horzGridColor, tmp);
                            horzPen.CreatePen(PS_SOLID, 1, horzGridColor);
                            pDC->SelectObject(&horzPen);
                            pDC->LineTo(lineRight, lineY);
                            pDC->SelectObject(&gridPen);
                            horzPen.DeleteObject();
                        } else {
                            pDC->LineTo(lineRight, lineY);
                        }
                    }

                    if (y >= endItem && gr.bottom >= std::min(r.bottom, updateRect.bottom)) {
                        break;
                    }
                }
            }
            pDC->SelectObject(oldPen);
            gridPen.DeleteObject();
        }
        pDC->RestoreDC(dcState);
    } else {
        return __super::OnEraseBkgnd(pDC);
    }
    return TRUE;
}


HBRUSH CMPCThemePlayerListCtrl::OnCtlColor(CDC* pDC, CWnd* pWnd, UINT nCtlColor)
{
    HBRUSH ret;
    ret = getCtlColor(pDC, pWnd, nCtlColor);
    if (nullptr != ret) {
        return ret;
    } else {
        return __super::OnCtlColor(pDC, pWnd, nCtlColor);
    }
}

void CMPCThemePlayerListCtrl::DrawAllItems(CDC* pDC, const CRect& drawRect) {
    int itemCount = GetItemCount();
    if (itemCount == 0) return;

    DWORD style = GetStyle() & LVS_TYPEMASK;
    DWORD extendedStyle = GetExtendedStyle();
    UINT cbResID = getResourceByDPI(GetParent(), pDC, CMPCTheme::ThemeCheckBoxes);

    CImageList* smallImageList = nullptr;
    if (style == LVS_SMALLICON || style == LVS_LIST || style == LVS_REPORT && !smallImageList) {
        smallImageList = GetImageList(LVSIL_SMALL);
    }

    if (style == LVS_REPORT) {
        UpdateColumnCache(style);

        int topIndex = GetTopIndex();
        int visibleCount = GetCountPerPage();

        int startItem = std::max(0, topIndex - 1);
        int endItem = std::min(itemCount - 1, topIndex + visibleCount + 1);

        CHeaderCtrl* pHeader = GetHeaderFast();
        int colCount = pHeader ? pHeader->GetItemCount() : 1;

        std::vector<int> visibleColumns;

        if (pHeader && colCount > 0 && itemCount > 0) {
            visibleColumns.reserve(colCount);

            for (int i = 0; i < colCount; i++) {
                CRect subItemRect;
                GetSubItemRect(startItem, i, LVIR_BOUNDS, subItemRect);

                if (subItemRect.right > drawRect.left && subItemRect.left < drawRect.right) {
                    visibleColumns.push_back(i);
                }
            }
        } else {
            visibleColumns.push_back(0);
        }

        for (int nItem = startItem; nItem <= endItem; nItem++) {
            CRect itemRect;
            GetItemRect(nItem, &itemRect, LVIR_BOUNDS);

            CRect intersectRect;
            if (!intersectRect.IntersectRect(&itemRect, &drawRect))
                continue;

            UINT itemState = GetItemState(nItem, LVIS_SELECTED | LVIS_STATEIMAGEMASK);
            bool isChecked = FALSE;
            if ((extendedStyle & LVS_EX_CHECKBOXES) && INDEXTOSTATEIMAGEMASK(0) != (itemState & LVIS_STATEIMAGEMASK)) {
                isChecked = (TRUE == GetCheck(nItem));
            }

            for (int nSubItem : visibleColumns) {
                drawItem(pDC, nItem, nSubItem, itemRect, style, extendedStyle, itemState, isChecked, smallImageList, cbResID);
            }
        }
    } else {
        for (int nItem = 0; nItem < itemCount; nItem++) {
            CRect itemRect;
            GetItemRect(nItem, &itemRect, LVIR_BOUNDS);

            CRect intersectRect;
            if (intersectRect.IntersectRect(&itemRect, &drawRect)) {
                UINT itemState = GetItemState(nItem, LVIS_SELECTED | LVIS_STATEIMAGEMASK);
                bool isChecked = FALSE;
                if ((extendedStyle & LVS_EX_CHECKBOXES) && INDEXTOSTATEIMAGEMASK(0) != (itemState & LVIS_STATEIMAGEMASK)) {
                    isChecked = (TRUE == GetCheck(nItem));
                }

                drawItem(pDC, nItem, 0, itemRect, style, extendedStyle, itemState, isChecked, smallImageList, cbResID);
            }
        }
    }
}

void CMPCThemePlayerListCtrl::UpdateColumnCache(DWORD style)
{
    s_columnCache.columns.clear();
    s_columnCache.hasIcons = false;
    
    ASSERT(style == LVS_REPORT);
    
    CHeaderCtrl* pHeader = GetHeaderFast();
    if (!pHeader) return;
    
    int colCount = pHeader->GetItemCount();
    if (colCount == 0) return;
    
    int itemCount = GetItemCount();
    if (itemCount == 0) return;
    
    CImageList* pImageList = GetImageList(LVSIL_SMALL);
    s_columnCache.hasIcons = (pImageList != nullptr && pImageList->GetImageCount() > 0);
    
    s_columnCache.columns.reserve(colCount);
    
    int topIndex = GetTopIndex();
    for (int i = 0; i < colCount; i++) {
        CRect boundsRect, labelRect, iconRect;
        GetSubItemRect(topIndex, i, LVIR_BOUNDS, boundsRect);
        GetSubItemRect(topIndex, i, LVIR_LABEL, labelRect);
        GetSubItemRect(topIndex, i, LVIR_ICON, iconRect);
        
        HDITEM hditem = {0};
        hditem.mask = HDI_FORMAT;
        pHeader->GetItem(i, &hditem);
        int align = hditem.fmt & HDF_JUSTIFYMASK;
        
        ColumnCache::ColumnInfo info;
        info.left = boundsRect.left;
        info.width = boundsRect.Width();
        info.labelLeft = labelRect.left;
        info.labelRight = labelRect.right;
        info.iconLeft = iconRect.left;
        info.iconRight = iconRect.right;
        info.align = align;
        s_columnCache.columns.push_back(info);
    }
}

bool CMPCThemePlayerListCtrl::GetSubItemRectFast(int nItem, int nSubItem, int nArea, CRect& rect, const CRect& rRow) {
    DWORD dwStyle = GetStyle() & LVS_TYPEMASK;

    if (dwStyle == LVS_REPORT && !s_columnCache.columns.empty() && nSubItem < (int)s_columnCache.columns.size()) {

        const auto& col = s_columnCache.columns[nSubItem];

        if (nSubItem == 0 && nArea == LVIR_BOUNDS) {
            rect = rRow;
        } else if (nArea == LVIR_LABEL) {
            rect.left = col.labelLeft;
            rect.right = col.labelRight;
            rect.top = rRow.top;
            rect.bottom = rRow.bottom;
        } else if (nArea == LVIR_ICON) {
            rect.left = col.iconLeft;
            rect.right = col.iconRight;
            rect.top = rRow.top;
            rect.bottom = rRow.bottom;
        } else {
            rect.left = col.left;
            rect.right = col.left + col.width;
            rect.top = rRow.top;
            rect.bottom = rRow.bottom;
        }

        return TRUE;
    }

    return GetSubItemRect(nItem, nSubItem, nArea, rect);
}

const CString& CMPCThemePlayerListCtrl::GetCachedText(int nItem, int nSubItem) {
    DWORD currentTime = GetTickCount();

    if (currentTime - m_cacheTimestamp >= CACHE_TIMEOUT_MS) {
        m_textCache.clear();
        m_cacheTimestamp = currentTime;
    }

    auto it = m_textCache.find(nItem);
    if (it != m_textCache.end() && nSubItem < (int)it->second.columns.size()) {
        return it->second.columns[nSubItem];
    }

    int colCount = GetHeaderCtrl() ? GetHeaderCtrl()->GetItemCount() : 1;
    TextCacheEntry& entry = m_textCache[nItem];
    entry.columns.resize(colCount);

    LVITEM lvi = { 0 };
    lvi.iItem = nItem;
    lvi.iSubItem = 0;
    lvi.mask = LVIF_IMAGE;
    GetItem(&lvi);
    entry.imageIndex = lvi.iImage;

    for (int i = 0; i < colCount; i++) {
        entry.columns[i] = GetItemText(nItem, i);
    }

    return entry.columns[nSubItem];
}

int CMPCThemePlayerListCtrl::GetCachedImageIndex(int nItem) {
    auto it = m_textCache.find(nItem);
    if (it != m_textCache.end()) {
        return it->second.imageIndex;
    }

    // Force cache population by getting any text column
    GetCachedText(nItem, 0);
    return m_textCache[nItem].imageIndex;
}
