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
    } else {
        return NULL;
    }
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
        CPaintDC dc(this);

        CRect rect;
        GetClientRect(&rect);

        CBrush brush;
        brush.CreateSolidBrush(CMPCTheme::InlineEditBorderColor);
        dc.FrameRect(&rect, &brush);
        brush.DeleteObject();

        rect.DeflateRect(1, 1);
        CBrush bgBrush(CMPCTheme::ContentBGColor);
        dc.FrameRect(rect, &bgBrush);
    } else {
        __super::OnPaint();
    }
}
