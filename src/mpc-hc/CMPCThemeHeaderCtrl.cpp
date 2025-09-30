#include "stdafx.h"
#include "CMPCThemeHeaderCtrl.h"
#include "CMPCTheme.h"
#include "CMPCThemeUtil.h"
#include "DpiHelper.h"
#include "mplayerc.h"
#include "CMPCThemePlayerListCtrl.h"

CMPCThemeHeaderCtrl::CMPCThemeHeaderCtrl()
{
    hotItem = -2;
    parent = nullptr;
}


CMPCThemeHeaderCtrl::~CMPCThemeHeaderCtrl()
{
}

IMPLEMENT_DYNAMIC(CMPCThemeHeaderCtrl, CHeaderCtrl)
BEGIN_MESSAGE_MAP(CMPCThemeHeaderCtrl, CHeaderCtrl)
    ON_NOTIFY_REFLECT(NM_CUSTOMDRAW, &CMPCThemeHeaderCtrl::OnNMCustomdraw)
    ON_NOTIFY(HDN_TRACKA, 0, &CMPCThemeHeaderCtrl::OnHdnTrack)
    ON_NOTIFY(HDN_TRACKW, 0, &CMPCThemeHeaderCtrl::OnHdnTrack)
    ON_WM_MOUSEMOVE()
    ON_WM_MOUSELEAVE()
    ON_WM_ERASEBKGND()
    ON_WM_PAINT()
    ON_NOTIFY_REFLECT_EX(HDN_BEGINTRACKA, &CMPCThemeHeaderCtrl::OnHdnBegintrack)
    ON_NOTIFY_REFLECT_EX(HDN_BEGINTRACKW, &CMPCThemeHeaderCtrl::OnHdnBegintrack)
    ON_NOTIFY_REFLECT_EX(HDN_ENDTRACKA, &CMPCThemeHeaderCtrl::OnHdnEndtrack)
    ON_NOTIFY_REFLECT_EX(HDN_ENDTRACKW, &CMPCThemeHeaderCtrl::OnHdnEndtrack)
    ON_WM_WINDOWPOSCHANGING()
END_MESSAGE_MAP()

BOOL CMPCThemeHeaderCtrl::OnEraseBkgnd(CDC* pDC) {
    //header must draw itself 
    if (!parent || parent->PaintHooksActive()) {
        return __super::OnEraseBkgnd(pDC);
    } else {
        return TRUE;
    }
}

void CMPCThemeHeaderCtrl::drawSortArrow(CDC* dc, COLORREF arrowClr, CRect arrowRect, bool ascending)
{
    DpiHelper dpiWindow;
    dpiWindow.Override(GetSafeHwnd());

    Gdiplus::Color clr;
    clr.SetFromCOLORREF(arrowClr);

    int dpi = dpiWindow.DPIX();
    float steps;

    if (dpi < 120) {
        steps = 3.5;
    } else if (dpi < 144) {
        steps = 4;
    } else if (dpi < 168) {
        steps = 5;
    } else if (dpi < 192) {
        steps = 5;
    } else {
        steps = 6;
    }

    int xPos = arrowRect.left + (arrowRect.Width() - (steps * 2 + 1)) / 2;
    int yPos = arrowRect.top;

    Gdiplus::Graphics gfx(dc->m_hDC);
    gfx.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias8x4);
    Gdiplus::Pen pen(clr, 1);
    for (int i = 0; i < 2; i++) {
        Gdiplus::GraphicsPath path;
        Gdiplus::PointF vertices[3];

        if (ascending) {
            vertices[0] = Gdiplus::PointF(xPos, yPos);
            vertices[1] = Gdiplus::PointF(steps + xPos, yPos + steps);
            vertices[2] = Gdiplus::PointF(steps * 2 + xPos, yPos);
        } else {
            vertices[0] = Gdiplus::PointF(xPos, yPos + steps);
            vertices[1] = Gdiplus::PointF(steps + xPos, yPos);
            vertices[2] = Gdiplus::PointF(steps * 2 + xPos, yPos + steps);
        }

        path.AddLines(vertices, 3);
        gfx.DrawPath(&pen, &path);
    }
}

void CMPCThemeHeaderCtrl::drawItem(int nItem, CRect rText, CDC* pDC)
{

    COLORREF textColor = CMPCTheme::TextFGColor;
    COLORREF bgColor = CMPCTheme::ContentBGColor;

    COLORREF oldTextColor = pDC->GetTextColor();
    COLORREF oldBkColor = pDC->GetBkColor();

    CRect rGrid;
    rGrid = rText;


    rGrid.top -= 1;
    rGrid.bottom -= 1;

    if (nItem == hotItem && !colDrag) {
        bgColor = CMPCTheme::ColumnHeaderHotColor;
    }

    pDC->FillSolidRect(rGrid, bgColor);

    CPen gridPen, *oldPen;
    gridPen.CreatePen(PS_SOLID, 1, CMPCTheme::HeaderCtrlGridColor);
    oldPen = pDC->SelectObject(&gridPen);
    if (nItem != 0) {
        //we will draw left border, which lines up with grid.  this differs from native widget
        //which draws the right border which consequently does not line up with the grid (ugly)
        //we only draw the left border starting from the second column
        pDC->MoveTo(rGrid.left, rGrid.top);
        pDC->LineTo(rGrid.left, rGrid.bottom);
    } else {
        pDC->MoveTo(rGrid.left, rGrid.bottom);
    }
    pDC->LineTo(rGrid.BottomRight());
    //pDC->LineTo(rGrid.right, rGrid.top);
    pDC->SelectObject(oldPen);

    if (nItem != -1) {
        HDITEM hditem = { 0 };
        hditem.mask = HDI_FORMAT | HDI_TEXT | HDI_STATE;
        const int c_cchBuffer = 1024;
        TCHAR  lpBuffer[c_cchBuffer];
        hditem.pszText = lpBuffer;
        hditem.cchTextMax = c_cchBuffer;

        GetItem(nItem, &hditem);
        int align = hditem.fmt & HDF_JUSTIFYMASK;
        UINT textFormat = DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS | DT_NOPREFIX;
        if (align == HDF_CENTER) {
            textFormat |= DT_CENTER;
        } else if (align == HDF_LEFT) {
            textFormat |= DT_LEFT;
            rText.left += 6;
        } else {
            textFormat |= DT_RIGHT;
            rText.right -= 6;
        }
        CString text = hditem.pszText;
        pDC->SetTextColor(textColor);
        pDC->SetBkColor(bgColor);

        pDC->DrawTextW(text, rText, textFormat);
        if (hditem.fmt & HDF_SORTUP) {
            drawSortArrow(pDC, CMPCTheme::HeaderCtrlSortArrowColor, rText, true);
        } else if (hditem.fmt & HDF_SORTDOWN) {
            drawSortArrow(pDC, CMPCTheme::HeaderCtrlSortArrowColor, rText, false);
        }
    }

    pDC->SetTextColor(oldTextColor);
    pDC->SetBkColor(oldBkColor);
    gridPen.DeleteObject();
}

/* custom draw doesn't handle empty areas! code is no longer used in favor of OnPaint() */
void CMPCThemeHeaderCtrl::OnNMCustomdraw(NMHDR* pNMHDR, LRESULT* pResult)
{
    NMLVCUSTOMDRAW* pLVCD = reinterpret_cast<NMLVCUSTOMDRAW*>(pNMHDR);

    *pResult = CDRF_DODEFAULT;
    if (AppIsThemeLoaded()) {
        if (pLVCD->nmcd.dwDrawStage == CDDS_PREPAINT) {
            *pResult = CDRF_NOTIFYITEMDRAW;
        } else if (pLVCD->nmcd.dwDrawStage == CDDS_ITEMPREPAINT) {
            int nItem = pLVCD->nmcd.dwItemSpec;
            CDC* pDC = CDC::FromHandle(pLVCD->nmcd.hdc);
            CRect rText;
            GetItemRect(nItem, rText);

            drawItem(nItem, rText, pDC);
            *pResult = CDRF_SKIPDEFAULT;
        }
    }
}


void CMPCThemeHeaderCtrl::OnHdnTrack(NMHDR* pNMHDR, LRESULT* pResult)
{
    //    LPNMHEADER phdr = reinterpret_cast<LPNMHEADER>(pNMHDR);
    *pResult = 0;
}

void CMPCThemeHeaderCtrl::checkHot(CPoint point, bool invalidate)
{
    HDHITTESTINFO hdHitTestInfo;
    hdHitTestInfo.pt = point;

    int prevHotItem = hotItem;
    hotItem = (int)SendMessage(HDM_HITTEST, 0, (LPARAM)&hdHitTestInfo);

    if ((hdHitTestInfo.flags & HHT_ONHEADER) == 0) {
        hotItem = -2;
    }
    if (hotItem != prevHotItem && invalidate) {
        CRect wr;
        GetWindowRect(wr);
        if (!parent || parent->PaintHooksActive()) {
            RedrawWindow(wr);
        } else if (parent) {
            parent->RedrawHeader(wr);
        }
    }
}


void CMPCThemeHeaderCtrl::OnMouseMove(UINT nFlags, CPoint point)
{
    if ((nFlags & MK_LBUTTON) == 0) {
        checkHot(point, true);
    }

    __super::OnMouseMove(nFlags, point);
}

void CMPCThemeHeaderCtrl::OnMouseLeave()
{
    if (hotItem >= 0) {
        hotItem = -1;
        CRect wr;
        GetWindowRect(wr);

        if (!parent || parent->PaintHooksActive()) {
            RedrawWindow(wr);
        } else if (parent) {
            parent->RedrawHeader(wr);
        }
    }
    __super::OnMouseLeave();
}


#define max(a,b)            (((a) > (b)) ? (a) : (b))
void CMPCThemeHeaderCtrl::OnPaint()
{
  if (AppNeedsThemedControls()) {

    if (GetStyle() & HDS_FILTERBAR) {
        Default();
        return;
    }

    CPaintDC dc(this); // device context for painting
    CRect updateRect;
    dc.GetClipBox(&updateRect);

    //header must draw itself 
    if (!parent || parent->PaintHooksActive()) {
        CMemDC memDC(dc, this);
        CDC* pDC = &memDC.GetDC();
        DrawAllItems(pDC, { 0,0 }, updateRect);
    } else {
        if (!updateRect.IsRectEmpty()) {
            ClientToScreen(updateRect);
            parent->RedrawHeader(updateRect);
        }
    }
  } else {
    Default();
  }
}

void CMPCThemeHeaderCtrl::DrawAllItems(CDC* pDC, CPoint offset, const CRect& clipRect) {
    CFont* font = GetFont();
    CFont* pOldFont = pDC->SelectObject(font);

    CRect rect;
    GetClientRect(rect);

    CRect rectItem;
    int nCount = GetItemCount();

    int xMax = 0;

    CPoint ptCursor;
    ::GetCursorPos(&ptCursor);
    ScreenToClient(&ptCursor);
    checkHot(ptCursor, false);

    for (int i = 0; i < nCount; i++) {
        GetItemRect(i, rectItem);
        xMax = max(xMax, rectItem.right);
        
        CRect offsetRectItem = rectItem;
        offsetRectItem.OffsetRect(offset);
        
        CRect intersection;
        if (intersection.IntersectRect(offsetRectItem, clipRect)) {
            drawItem(i, offsetRectItem, pDC);
        }
    }

    // Draw "tail border":
    if (nCount == 0) {
        rectItem = rect;
        rectItem.right++;
    } else {
        rectItem.left = xMax;
        rectItem.right = rect.right + 1;
    }

    rectItem.OffsetRect(offset);
    
    CRect intersection;
    if (intersection.IntersectRect(rectItem, clipRect)) {
        drawItem(-1, rectItem, pDC);
    }
    
    pDC->SelectObject(pOldFont);
}

BOOL CMPCThemeHeaderCtrl::OnHdnBegintrack(NMHDR* pNMHDR, LRESULT* pResult) {
    LPNMHEADER phdr = reinterpret_cast<LPNMHEADER>(pNMHDR);
    colDrag = true;
    *pResult = 0;
    return FALSE;
}


BOOL CMPCThemeHeaderCtrl::OnHdnEndtrack(NMHDR* pNMHDR, LRESULT* pResult) {
    LPNMHEADER phdr = reinterpret_cast<LPNMHEADER>(pNMHDR);
    colDrag = false;
    *pResult = 0;
    return FALSE;
}


void CMPCThemeHeaderCtrl::OnWindowPosChanging(WINDOWPOS* lpwndpos) {
    if (parent && !parent->PaintHooksActive()) {
        lpwndpos->flags |= SWP_NOCOPYBITS; //avoids shifting header pixels during scroll
    }

    __super::OnWindowPosChanging(lpwndpos);
}
