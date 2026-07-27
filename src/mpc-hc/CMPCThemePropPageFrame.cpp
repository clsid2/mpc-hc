#include "stdafx.h"
#include "CMPCThemePropPageFrame.h"
#include "CMPCTheme.h"
#include "CMPCThemeUtil.h"
#include "mplayerc.h"
#include "TreePropSheet/PropPageFrameDefault.h"
#include "../DSUtil/WinAPIUtils.h"

CBrush CMPCThemePropPageFrame::mpcThemeBorderBrush;

CMPCThemePropPageFrame::CMPCThemePropPageFrame() : CPropPageFrameDefault()
{
    if (nullptr == mpcThemeBorderBrush.m_hObject) {
        mpcThemeBorderBrush.CreateSolidBrush(CMPCTheme::WindowBorderColorLight);
    }
}


CMPCThemePropPageFrame::~CMPCThemePropPageFrame()
{
}

BEGIN_MESSAGE_MAP(CMPCThemePropPageFrame, CPropPageFrameDefault)
    ON_WM_PAINT()
    ON_WM_ERASEBKGND()
END_MESSAGE_MAP()


BOOL CMPCThemePropPageFrame::Create(DWORD dwWindowStyle, const RECT& rect, CWnd* pwndParent, UINT nID)
{
    return CWnd::Create(
               AfxRegisterWndClass(CS_HREDRAW | CS_VREDRAW, AfxGetApp()->LoadStandardCursor(IDC_ARROW), 0),
               _T("MPCTheme Page Frame"),
               dwWindowStyle, rect, pwndParent, nID);
}

CWnd* CMPCThemePropPageFrame::GetWnd()
{
    return static_cast<CWnd*>(this);
}

void CMPCThemePropPageFrame::DrawCaption(CDC* pDC, CRect rect, LPCTSTR lpszCaption, HICON hIcon)
{
    COLORREF    clrLeft = CMPCTheme::ContentSelectedColor;
    COLORREF    clrRight = CMPCTheme::ContentBGColor;
    FillGradientRectH(pDC, rect, clrLeft, clrRight);

    rect.left += 2;

    COLORREF clrPrev = pDC->SetTextColor(CMPCTheme::PropPageCaptionFGColor);
    int nBkStyle = pDC->SetBkMode(TRANSPARENT);

    LOGFONT lf;
    GetMessageFont(&lf);
    lf.lfHeight = static_cast<long>(-.8f * rect.Height());
    lf.lfWeight = FW_BOLD;
    CFont f;
    f.CreateFontIndirectW(&lf);
    CFont* oldFont = pDC->SelectObject(&f);

    TEXTMETRIC GDIMetrics;
    GetTextMetricsW(pDC->GetSafeHdc(), &GDIMetrics);
    while (GDIMetrics.tmHeight > rect.Height() && abs(lf.lfHeight) > 10) {
        pDC->SelectObject(oldFont);
        f.DeleteObject();
        lf.lfHeight++;
        f.CreateFontIndirectW(&lf);
        pDC->SelectObject(&f);
        GetTextMetricsW(pDC->GetSafeHdc(), &GDIMetrics);
    }


    rect.top -= GDIMetrics.tmDescent - 1;

    pDC->DrawTextW(lpszCaption, rect, DT_LEFT | DT_SINGLELINE | DT_END_ELLIPSIS); // DT_NOPREFIX not needed

    pDC->SetTextColor(clrPrev);
    pDC->SelectObject(oldFont);
    pDC->SetBkMode(nBkStyle);
}

void CMPCThemePropPageFrame::OnPaint()
{
    CPaintDC dc(this);
    Draw(&dc);
}

BOOL CMPCThemePropPageFrame::OnEraseBkgnd(CDC* pDC)
{
    BOOL ret;
    if (CMPCThemeUtil::MPCThemeEraseBkgnd(pDC, this, CTLCOLOR_DLG)) {
        ret = TRUE;
    } else {
        ret = __super::OnEraseBkgnd(pDC); //light erases to match its native pages
    }

    if (AppIsThemeLoaded()) { //the caption is themed in light too, so keep the matching frame border
        CRect rect;
        GetClientRect(rect);
        pDC->FrameRect(rect, &mpcThemeBorderBrush);
    }
    return ret;
}
