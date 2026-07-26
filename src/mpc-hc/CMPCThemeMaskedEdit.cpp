#include "stdafx.h"
#include "CMPCThemeMaskedEdit.h"
#include "mplayerc.h"
#include "CMPCTheme.h"

CMPCThemeMaskedEdit::CMPCThemeMaskedEdit()
{
}


CMPCThemeMaskedEdit::~CMPCThemeMaskedEdit()
{
}

IMPLEMENT_DYNAMIC(CMPCThemeMaskedEdit, CMFCMaskedEdit)
BEGIN_MESSAGE_MAP(CMPCThemeMaskedEdit, CMFCMaskedEdit)
    ON_WM_NCPAINT()
END_MESSAGE_MAP()


void CMPCThemeMaskedEdit::PreSubclassWindow()
{
    if (AppNeedsThemedControls()) {
        ModifyStyleEx(WS_EX_CLIENTEDGE, WS_EX_STATICEDGE, SWP_FRAMECHANGED);
        CRect r;
        GetClientRect(r);
        r.DeflateRect(2, 2); //some default padding for those spaceless fonts
        SetRect(r);
    } else {
        __super::PreSubclassWindow();
    }
}

void CMPCThemeMaskedEdit::OnNcPaint()
{
    if (AppNeedsThemedControls()) {
        CWindowDC dc(this);

        CRect rect;
        GetWindowRect(&rect);
        rect.OffsetRect(-rect.left, -rect.top);

        CBrush brush;
        brush.CreateSolidBrush(CMPCTheme::EditBorderColor);

        dc.FrameRect(&rect, &brush);
        brush.DeleteObject();
    } else {
        __super::OnNcPaint();
    }
}
