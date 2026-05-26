#include "stdafx.h"
#include "CMPCThemeInlineEdit.h"
#include "CMPCTheme.h"
#include "mplayerc.h"

CMPCThemeInlineEdit::CMPCThemeInlineEdit():
    overrideX(0)
    ,overrideMaxWidth(-1)
    ,offsetEnabled(false)
{
    m_brBkgnd.CreateSolidBrush(CMPCTheme::ContentBGColor);
}

CMPCThemeInlineEdit::~CMPCThemeInlineEdit()
{
    m_brBkgnd.DeleteObject();
}

void CMPCThemeInlineEdit::setOverridePos(int x, int maxWidth) {
    overrideX = x;
    overrideMaxWidth = maxWidth;
    offsetEnabled = true;
}

BEGIN_MESSAGE_MAP(CMPCThemeInlineEdit, CEdit)
    ON_WM_CTLCOLOR_REFLECT()
    ON_WM_WINDOWPOSCHANGED()
    ON_WM_PAINT()
END_MESSAGE_MAP()

HBRUSH CMPCThemeInlineEdit::CtlColor(CDC* pDC, UINT nCtlColor)
{
    if (AppIsThemeLoaded()) {
        pDC->SetTextColor(CMPCTheme::TextFGColor);
        pDC->SetBkColor(CMPCTheme::ContentBGColor);
        return m_brBkgnd;
    }
    return nullptr;
}

void CMPCThemeInlineEdit::OnWindowPosChanged(WINDOWPOS* lpwndpos) {
    if (offsetEnabled && overrideX != lpwndpos->x) {
        lpwndpos->cx = overrideMaxWidth == -1 ? lpwndpos->cx : std::min(lpwndpos->cx, overrideMaxWidth);
        SetWindowPos(nullptr, overrideX, lpwndpos->y, lpwndpos->cx, lpwndpos->cy, SWP_NOZORDER);
    }
    CEdit::OnWindowPosChanged(lpwndpos);
}

void CMPCThemeInlineEdit::OnPaint() {
    if (AppIsThemeLoaded()) {
        Default();
        CDC* pDC = GetDC();
        if (pDC) {
            CRect rect;
            GetClientRect(&rect);
            CBrush outerBrush(CMPCTheme::InlineEditBorderColor);
            pDC->FrameRect(&rect, &outerBrush);
            rect.DeflateRect(1, 1);
            CBrush innerBrush(CMPCTheme::ContentBGColor);
            pDC->FrameRect(rect, &innerBrush);
            ReleaseDC(pDC);
        }
    } else {
        __super::OnPaint();
    }
}
