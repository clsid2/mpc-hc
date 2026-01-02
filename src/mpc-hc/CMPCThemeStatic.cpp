#include "stdafx.h"
#include "CMPCThemeStatic.h"
#include "CMPCTheme.h"
#include "CMPCThemeUtil.h"
#include "mplayerc.h"

CMPCThemeStatic::CMPCThemeStatic()
{
    isFileDialogChild = false;
}


CMPCThemeStatic::~CMPCThemeStatic()
{
}
IMPLEMENT_DYNAMIC(CMPCThemeStatic, CStatic)
BEGIN_MESSAGE_MAP(CMPCThemeStatic, CStatic)
    ON_WM_PAINT()
    ON_WM_NCPAINT()
    ON_WM_ENABLE()
    ON_WM_ERASEBKGND()
    ON_REGISTERED_MESSAGE(WMU_RESIZESUPPORT, ResizeSupport)
END_MESSAGE_MAP()

//this message is sent by resizablelib
//we prevent clipping for statics as they don't get redrawn correctly after erasing
LRESULT CMPCThemeStatic::ResizeSupport(WPARAM wParam, LPARAM lParam) {
    if (AppNeedsThemedControls()) {
        if (wParam == RSZSUP_QUERYPROPERTIES) {
            LPRESIZEPROPERTIES props = (LPRESIZEPROPERTIES)lParam;
            props->bAskClipping = false;
            props->bCachedLikesClipping = false;
            return TRUE;
        }
    }
    return FALSE;
}

void CMPCThemeStatic::OnPaint()
{
    if (AppNeedsThemedControls()) {
        CPaintDC dc(this);

        CString sTitle;
        GetWindowText(sTitle);
        CRect rectItem;
        GetClientRect(rectItem);
        dc.SetBkMode(TRANSPARENT);

        COLORREF oldBkColor = dc.GetBkColor();
        COLORREF oldTextColor = dc.GetTextColor();

        bool isDisabled = !IsWindowEnabled();
        UINT style = GetStyle();

        if (!sTitle.IsEmpty()) {
            bool canWrap = sTitle.Find(_T("\n")) != -1;

            CFont* font = GetFont();
            CFont* pOldFont = dc.SelectObject(font);

            UINT uFormat = 0;
            if (style & SS_LEFTNOWORDWRAP) {
                if (!canWrap) {
                    uFormat |= DT_SINGLELINE;
                }
            } else {
                uFormat |= DT_WORDBREAK;
            }

            if (0 != (style & SS_CENTERIMAGE) && !canWrap) {
                //If the static control contains a single line of text, the text is centered vertically in the client area of the control. msdn
                uFormat |= DT_SINGLELINE;
                uFormat |= DT_VCENTER;
            } else {
                uFormat |= DT_TOP;
            }

            if ((style & SS_CENTER) == SS_CENTER) {
                uFormat |= DT_CENTER;
            } else if ((style & SS_RIGHT) == SS_RIGHT) {
                uFormat |= DT_RIGHT;
            } else { // if ((style & SS_LEFT) == SS_LEFT || (style & SS_LEFTNOWORDWRAP) == SS_LEFTNOWORDWRAP) {
                uFormat |= DT_LEFT;
            }

            UINT ellipsisStyle = (style & SS_ELLIPSISMASK);
            if (ellipsisStyle == SS_PATHELLIPSIS) {
                uFormat |= DT_PATH_ELLIPSIS;
            } else if (ellipsisStyle == SS_ENDELLIPSIS) {
                uFormat |= DT_END_ELLIPSIS;
            } else if (ellipsisStyle == SS_WORDELLIPSIS) {
                uFormat |= DT_WORD_ELLIPSIS;
            }

            if ((SendMessage(WM_QUERYUISTATE, 0, 0) & UISF_HIDEACCEL) != 0) {
                uFormat |= DT_HIDEPREFIX;
            }

            dc.SetBkColor(CMPCTheme::WindowBGColor);
            if (isDisabled) {
                dc.SetTextColor(CMPCTheme::ButtonDisabledFGColor);
                dc.DrawTextW(sTitle, -1, &rectItem, uFormat);
            } else {
                dc.SetTextColor(CMPCTheme::TextFGColor);
                dc.DrawTextW(sTitle, -1, &rectItem, uFormat);
            }
            dc.SelectObject(pOldFont);
            dc.SetBkColor(oldBkColor);
            dc.SetTextColor(oldTextColor);
        }
    } else {
        __super::OnPaint();
    }
}


void CMPCThemeStatic::OnNcPaint()
{
    if (AppNeedsThemedControls()) {
        CDC* pDC = GetWindowDC();

        CRect rect;
        GetWindowRect(&rect);
        rect.OffsetRect(-rect.left, -rect.top);
        DWORD type = GetStyle() & SS_TYPEMASK;

        if (SS_ETCHEDHORZ == type || SS_ETCHEDVERT == type) { //etched lines assumed
            rect.DeflateRect(0, 0, 1, 1); //make it thinner
            CBrush brush(CMPCTheme::StaticEtchedColor);
            pDC->FillSolidRect(rect, CMPCTheme::StaticEtchedColor);
        } else if (SS_ETCHEDFRAME == type) { //etched border
            CBrush brush(CMPCTheme::StaticEtchedColor);
            pDC->FrameRect(rect, &brush);
        } else { //not supported yet
        }

        ReleaseDC(pDC);
    } else {
        CStatic::OnNcPaint();
    }
}

void CMPCThemeStatic::OnEnable(BOOL bEnable)
{
    if (AppNeedsThemedControls()) {
        SetRedraw(FALSE);
        __super::OnEnable(bEnable);
        SetRedraw(TRUE);
        CWnd* parent = GetParent();
        if (nullptr != parent) {
            CRect wr;
            GetWindowRect(wr);
            parent->ScreenToClient(wr);
            parent->InvalidateRect(wr, TRUE);
        } else {
            Invalidate();
        }
    } else {
        __super::OnEnable(bEnable);
    }
}

BOOL CMPCThemeStatic::OnEraseBkgnd(CDC* pDC)
{
    if (AppNeedsThemedControls()) {
        CRect r;
        GetClientRect(r);
        if (isFileDialogChild) {
            HBRUSH hBrush = CMPCThemeUtil::getCtlColorFileDialog(pDC->GetSafeHdc(), CTLCOLOR_STATIC);
            ::FillRect(pDC->GetSafeHdc(), r, hBrush);
        } else {
            CMPCThemeUtil::drawParentDialogBGClr(this, pDC, r);
        }
        return TRUE;
    } else {
        return CStatic::OnEraseBkgnd(pDC);
    }
}
