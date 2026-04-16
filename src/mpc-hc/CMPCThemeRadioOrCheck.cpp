#include "stdafx.h"
#include "CMPCThemeRadioOrCheck.h"
#include "CMPCTheme.h"
#include "CMPCThemeButton.h"
#include "CMPCThemeUtil.h"
#include "VersionHelpersInternal.h"
#include "DpiHelper.h"
#include "mplayerc.h"

CMPCThemeRadioOrCheck::CMPCThemeRadioOrCheck()
{
    isHover = false;
    buttonType = RadioOrCheck::unknownType;
    isFileDialogChild = false;
    isAuto = false;
}


CMPCThemeRadioOrCheck::~CMPCThemeRadioOrCheck()
{
}

void CMPCThemeRadioOrCheck::PreSubclassWindow()
{
    DWORD winButtonType = (GetButtonStyle() & BS_TYPEMASK);

    if (BS_RADIOBUTTON == winButtonType || BS_AUTORADIOBUTTON == winButtonType) {
        buttonType = radioType;
        isAuto = BS_AUTORADIOBUTTON == winButtonType;
    } else if (BS_3STATE == winButtonType || BS_AUTO3STATE == winButtonType) {
        buttonType = threeStateType;
        isAuto = BS_AUTO3STATE == winButtonType;
    } else if (BS_CHECKBOX == winButtonType || BS_AUTOCHECKBOX == winButtonType) {
        buttonType = checkType;
        isAuto = BS_AUTOCHECKBOX == winButtonType;
    }
    ASSERT(buttonType != unknownType);

    CButton::PreSubclassWindow();
}

IMPLEMENT_DYNAMIC(CMPCThemeRadioOrCheck, CButton)
BEGIN_MESSAGE_MAP(CMPCThemeRadioOrCheck, CButton)
    ON_WM_MOUSEMOVE()
    ON_WM_MOUSELEAVE()
    ON_WM_PAINT()
    ON_WM_ENABLE()
    ON_WM_ERASEBKGND()
    ON_WM_UPDATEUISTATE()
END_MESSAGE_MAP()


void CMPCThemeRadioOrCheck::OnPaint()
{
    if (AppIsThemeLoaded() && CMPCTheme::drawThemedControls) {
        DWORD buttonStyle = GetWindowLongPtr(GetSafeHwnd(), GWL_STYLE);

        CPaintDC dc(this);
        CRect   rectItem;
        GetClientRect(rectItem);

        COLORREF oldBkColor = dc.GetBkColor();
        COLORREF oldTextColor = dc.GetTextColor();

        bool isDisabled = !IsWindowEnabled();
        bool isFocused = (GetFocus() == this);

        LRESULT checkState = SendMessage(BM_GETCHECK);

        CString sTitle;
        GetWindowText(sTitle);


        if (0 != (buttonStyle & BS_PUSHLIKE)) {
            CFont* oFont, *font = GetFont();
            oFont = dc.SelectObject(font);
            CMPCThemeButton::drawButtonBase(&dc, rectItem, sTitle, checkState != BST_UNCHECKED, isHover, isFocused, checkState == BST_INDETERMINATE, false, false, m_hWnd);
            dc.SelectObject(oFont);
        } else {
            CRect rectCheck;
            int cbWidth;
            int cbHeight;
            if (IsWindows8OrGreater()) {
                DpiHelper dpiWindow;
                dpiWindow.Override(this->GetSafeHwnd());
                cbWidth = dpiWindow.GetSystemMetricsDPI(SM_CXMENUCHECK);
                cbHeight = dpiWindow.GetSystemMetricsDPI(SM_CYMENUCHECK);
            } else {
                cbWidth = ::GetSystemMetrics(SM_CXMENUCHECK);
                cbHeight = ::GetSystemMetrics(SM_CYMENUCHECK);
            }

            if (buttonStyle & BS_LEFTTEXT) {
                rectCheck.left = rectItem.right - cbWidth;
                rectCheck.right = rectCheck.left + cbWidth;
                rectItem.right = rectCheck.left - 2;
            } else {
                rectCheck.left = rectItem.left;
                rectCheck.right = rectCheck.left + cbWidth;
                rectItem.left = rectCheck.right + 2;
            }

            rectCheck.top = (rectItem.Height() - cbHeight) / 2;
            rectCheck.bottom = rectCheck.top + cbHeight;

            if (buttonType == checkType) {
                CMPCThemeUtil::drawCheckBox(GetParent(), checkState, isHover, true, rectCheck, &dc);
            } else if (buttonType == threeStateType) {
                CMPCThemeUtil::drawCheckBox(GetParent(), checkState, isHover, true, rectCheck, &dc);
            } else if (buttonType == radioType) {
                CMPCThemeUtil::drawCheckBox(GetParent(), checkState, isHover, true, rectCheck, &dc, true);
            }

            if (!sTitle.IsEmpty()) {
                CRect centerRect = rectItem;
                CFont* pOldFont, *font = GetFont();
                pOldFont = dc.SelectObject(font);

                UINT uFormat = 0;
                if (buttonStyle & BS_MULTILINE) {
                    uFormat |= DT_WORDBREAK;
                } else {
                    uFormat |= DT_SINGLELINE;
                }

                if (buttonStyle & BS_VCENTER) {
                    uFormat |= DT_VCENTER;
                }

                if ((buttonStyle & BS_CENTER) == BS_CENTER) {
                    uFormat |= DT_CENTER;
                    dc.DrawTextW(sTitle, -1, &rectItem, uFormat | DT_CALCRECT);  // DT_NOPREFIX not needed
                    rectItem.OffsetRect((centerRect.Width() - rectItem.Width()) / 2,
                                        (centerRect.Height() - rectItem.Height()) / 2);
                } else if ((buttonStyle & BS_RIGHT) == BS_RIGHT) {
                    uFormat |= DT_RIGHT;
                    dc.DrawTextW(sTitle, -1, &rectItem, uFormat | DT_CALCRECT);  // DT_NOPREFIX not needed
                    rectItem.OffsetRect(centerRect.Width() - rectItem.Width(),
                                        (centerRect.Height() - rectItem.Height()) / 2);
                } else { // if ((buttonStyle & BS_LEFT) == BS_LEFT) {
                    uFormat |= DT_LEFT;
                    dc.DrawTextW(sTitle, -1, &rectItem, uFormat | DT_CALCRECT);  // DT_NOPREFIX not needed
                    rectItem.OffsetRect(0, (centerRect.Height() - rectItem.Height()) / 2);
                }

                if (isFileDialogChild) {
                    CMPCThemeUtil::getCtlColorFileDialog(dc.GetSafeHdc(), CTLCOLOR_BTN);
                } else {
                    dc.SetBkColor(CMPCTheme::WindowBGColor);
                }

                CRect focusRect = rectItem;
                focusRect.InflateRect(0, 0);
                if (buttonStyle & BS_MULTILINE) { //needed to clear old select for multi-line
                    HBRUSH hb = CMPCThemeUtil::getParentDialogBGClr(this, &dc);
                    CBrush cb;
                    cb.Attach(hb);
                    dc.FrameRect(focusRect, &cb);
                    cb.Detach();
                }

                if (isDisabled) {
                    dc.SetTextColor(CMPCTheme::ButtonDisabledFGColor);
                    dc.DrawTextW(sTitle, -1, &rectItem, uFormat); // DT_NOPREFIX not needed
                } else {
                    dc.SetTextColor(CMPCTheme::TextFGColor);
                    dc.DrawTextW(sTitle, -1, &rectItem, uFormat); // DT_NOPREFIX not needed
                }
                dc.SelectObject(pOldFont);

                if (isFocused) {
                    dc.SetTextColor(CMPCTheme::ButtonBorderKBFocusColor); //no example of this in explorer, but white seems too harsh
                    CBrush* dotted = dc.GetHalftoneBrush();
                    dc.FrameRect(focusRect, dotted);
                    DeleteObject(dotted);
                }

            }
        }

        dc.SetBkColor(oldBkColor);
        dc.SetTextColor(oldTextColor);
    } else {
        CButton::OnPaint();
    }
}

void CMPCThemeRadioOrCheck::OnSetFocus(CWnd* pOldWnd)
{
    CButton::OnSetFocus(pOldWnd);
    Invalidate();
}

void CMPCThemeRadioOrCheck::checkHover(UINT nFlags, CPoint point, bool invalidate)
{
    CRect r;
    GetClientRect(r);
    bool oldHover = isHover;
    CPoint ptScreen = point;
    ClientToScreen(&ptScreen);

    if (r.PtInRect(point) && WindowFromPoint(ptScreen)->GetSafeHwnd() == GetSafeHwnd()) {
        isHover = true;
    } else {
        isHover = false;
    }
    if (isHover != oldHover && invalidate) {
        Invalidate();
    }

}

void CMPCThemeRadioOrCheck::OnMouseMove(UINT nFlags, CPoint point)
{
    checkHover(nFlags, point);
    CButton::OnMouseMove(nFlags, point);
}


void CMPCThemeRadioOrCheck::OnMouseLeave()
{
    checkHover(0, CPoint(-1, -1));
    CButton::OnMouseLeave();
}


void CMPCThemeRadioOrCheck::OnLButtonUp(UINT nFlags, CPoint point)
{
    checkHover(nFlags, point, false);
    CButton::OnLButtonUp(nFlags, point);
}


void CMPCThemeRadioOrCheck::OnLButtonDown(UINT nFlags, CPoint point)
{
    checkHover(nFlags, point);
    CButton::OnLButtonDown(nFlags, point);
}



void CMPCThemeRadioOrCheck::OnEnable(BOOL bEnable)
{
    if (AppIsThemeLoaded()) {
        SetRedraw(FALSE);
        __super::OnEnable(bEnable);
        SetRedraw(TRUE);
        Invalidate(); //WM_PAINT not handled when enabling/disabling
    } else {
        __super::OnEnable(bEnable);
    }
}


BOOL CMPCThemeRadioOrCheck::OnEraseBkgnd(CDC* pDC)
{
    CRect r;
    GetClientRect(r);
    if (isFileDialogChild) {
        HBRUSH hBrush = CMPCThemeUtil::getCtlColorFileDialog(pDC->GetSafeHdc(), CTLCOLOR_BTN);
        ::FillRect(pDC->GetSafeHdc(), r, hBrush);
    } else {
        CMPCThemeUtil::drawParentDialogBGClr(this, pDC, r);
    }
    return TRUE;
}


void CMPCThemeRadioOrCheck::OnUpdateUIState(UINT nAction, UINT nUIElement) {
    if (nUIElement & UISF_HIDEACCEL) {
        Invalidate();
    }
    return __super::OnUpdateUIState(nAction, nUIElement);
}
