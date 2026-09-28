/*
 * (C) 2003-2006 Gabest
 * (C) 2006-2020 see Authors.txt
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
#include "MainFrm.h"
#include "PlayerPreView.h"
#include "CMPCTheme.h"
#include "mplayerc.h"
#include "resource.h"

#define PREVIEW_TOOLTIP_BOTTOM 1

// CPrevView

CPreView::CPreView(CMainFrame* pMainFrame)
    : m_pMainFrame(pMainFrame) {
}

BOOL CPreView::SetWindowTextW(LPCWSTR lpString) {
    m_tooltipstr = lpString;

    CRect rect;
    GetClientRect(&rect);

    if (PREVIEW_TOOLTIP_BOTTOM) {
        rect.top    = rect.bottom - m_border - m_caption + 2;
        rect.bottom = rect.bottom - m_border - 1;
    } else {
        rect.top    = m_border + 1;
        rect.bottom = m_border + m_caption - 2;
    }
    rect.left  += m_border + 2;
    rect.right -= m_border + 2;

    InvalidateRect(rect);

    return ::SetWindowTextW(m_hWnd, lpString);
}

void CPreView::GetVideoRect(LPRECT lpRect) {
    m_view.GetClientRect(lpRect);
}

HWND CPreView::GetVideoHWND() {
    return m_view.GetSafeHwnd();
}

IMPLEMENT_DYNAMIC(CPreView, CWnd)

BEGIN_MESSAGE_MAP(CPreView, CWnd)
    ON_WM_CREATE()
    ON_WM_PAINT()
    ON_WM_SHOWWINDOW()
END_MESSAGE_MAP()

IMPLEMENT_DYNAMIC(previewView, CWnd)

BEGIN_MESSAGE_MAP(previewView, CWnd)
    ON_WM_PAINT()
    ON_WM_ERASEBKGND()
END_MESSAGE_MAP()

// CPreView message handlers

BOOL CPreView::PreCreateWindow(CREATESTRUCT& cs) {
    if (!CWnd::PreCreateWindow(cs)) {
        return FALSE;
    }

    cs.style &= ~WS_BORDER;

    return TRUE;
}

int CPreView::OnCreate(LPCREATESTRUCT lpCreateStruct) {
    if (CWnd::OnCreate(lpCreateStruct) == -1) {
        return -1;
    }

    m_caption = m_pMainFrame->m_dpi.ScaleY(20) + 3; // 3 pixels of padding

    CRect rc;
    GetClientRect(&rc);

    m_videorect.left = m_border;
    m_videorect.right = rc.right - m_border;
    if (PREVIEW_TOOLTIP_BOTTOM) { //bottom tooltip
        m_videorect.top = m_border;
        m_videorect.bottom = rc.bottom - m_caption;
    } else {
        m_videorect.top = m_caption;
        m_videorect.bottom = rc.bottom - m_border;
    }

    if (!m_view.Create(nullptr, nullptr, WS_CHILD | WS_VISIBLE, m_videorect, this, 0)) {
        return -1;
    }

    SetColor();

    return 0;
}

void previewView::OnPaint() {
    CPaintDC dc(this);
    if (onePaint) {
        CRect rc;
        GetClientRect(&rc);
        if (!noImage.m_hObject) {
            noImage.Load(IDB_NOIMAGE);
        }
        CImage i;
        i.Attach(noImage);
        CPoint p = { (rc.Width() - i.GetWidth()) / 2, (rc.Height() - i.GetHeight()) / 2 };
        CRect exRect = { p.x, p.y, p.x + i.GetWidth(), p.y + i.GetHeight() };
        i.BitBlt(dc, p);
        i.Detach();
        dc.ExcludeClipRect(exRect);
        dc.FillSolidRect(rc, RGB(0, 0, 0)); //fill
        onePaint = false;
    }
}

BOOL previewView::OnEraseBkgnd(CDC* pDC) {
    return TRUE;
}

void CPreView::OnPaint() {
    CPaintDC dc(this);

    CRect rcBar;
    GetClientRect(&rcBar);

    CDC mdc;
    mdc.CreateCompatibleDC(&dc);

    CBitmap bm;
    bm.CreateCompatibleBitmap(&dc, rcBar.Width(), rcBar.Height());
    CBitmap* pOldBm = mdc.SelectObject(&bm);
    mdc.SetBkMode(TRANSPARENT);

    COLORREF bg;
    if (AppIsThemeLoaded()) {
        bg = CMPCTheme::CMPCTheme::MenuBGColor;
        m_crText = CMPCTheme::TextFGColor;
    } else {
        bg = GetSysColor(COLOR_BTNFACE);
        m_crText = GetSysColor(COLOR_BTNTEXT);
    }

    mdc.FillSolidRect(0, 0, rcBar.Width(), rcBar.Height(), bg); //fill

    CRect rtime(rcBar);
    if (PREVIEW_TOOLTIP_BOTTOM) {
        rtime.top    = rtime.bottom - m_border - m_caption + 2;
        rtime.bottom = rtime.bottom - m_border - 1;
    } else {
        rtime.top    = m_border + 1;
        rtime.bottom = m_border + m_caption - 2;
    }
    rtime.left  += m_border + 2;
    rtime.right -= m_border + 2;

    // text
    if (!m_font.m_hObject) {
        ScaleFont();
    }
    mdc.SelectObject(&m_font);
    mdc.SetTextColor(m_crText);
    mdc.DrawTextW(m_tooltipstr, m_tooltipstr.GetLength(), &rtime, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS | DT_NOPREFIX);

    dc.ExcludeClipRect(m_videorect);
    dc.BitBlt(0, 0, rcBar.Width(), rcBar.Height(), &mdc, 0, 0, SRCCOPY);

    mdc.SelectObject(pOldBm);
    bm.DeleteObject();
    mdc.DeleteDC();
}

void CPreView::OnShowWindow(BOOL bShow, UINT nStatus) {
    if (bShow) {
        m_view.onePaint = true;
        UpdateWindow();
        m_pMainFrame->SetPreviewVideoPosition();
    }

    __super::OnShowWindow(bShow, nStatus);
}

void CPreView::SetWindowSize() {
    MONITORINFO mi = { sizeof(mi) };
    GetMonitorInfoW(MonitorFromWindow(GetParent()->GetSafeHwnd(), MONITOR_DEFAULTTONEAREST), &mi);

    CRect wr;
    GetParent()->GetClientRect(&wr);

    CSize vs = m_pMainFrame->GetVideoSizeWithRotation(true);
    if (vs.cx == 0) {
        vs.cx = 160;
        vs.cy = 90;
    }

    int h = 0;
    int w = 0;
    if (vs.cy > vs.cx) { // vertical video
        h = (mi.rcWork.bottom - mi.rcWork.top) * m_relativeSize / 100;
        // the preview size should not be larger than half size of the main window, but not less than 100
        h = std::max(100, std::min(h, wr.Height() / 2));
        w = h * vs.cx / vs.cy;
    } else { // normal horizontal video
        w = (mi.rcWork.right - mi.rcWork.left) * m_relativeSize / 100;
        // the preview size should not be larger than half size of the main window, but not less than 160
        w = std::max(160, std::min(w, wr.Width() / 2));
        h = w * vs.cy / vs.cx;
    }
    w += m_border * 2;
    h += m_caption + m_border;

    CRect rc;
    GetClientRect(&rc);
    if (rc.Width() != w || rc.Height() != h) {
        SetWindowPos(nullptr, 0, 0, w, h, SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);

        GetClientRect(&rc);
        m_videorect.right = rc.right - m_border;
        if (PREVIEW_TOOLTIP_BOTTOM) { //bottom tooltip
            m_videorect.bottom = rc.bottom - m_caption;
        } else {
            m_videorect.bottom = rc.bottom - m_border;
        }

        m_view.SetWindowPos(nullptr, 0, 0, m_videorect.Width(), m_videorect.Height(), SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
    }
}

void CPreView::ScaleFont() {
    m_font.DeleteObject();
    CMPCThemeUtil::getFontByType(m_font, nullptr, CMPCThemeUtil::MessageFont);
}

void CPreView::SetColor() {

}
