/*
 * (C) 2003-2006 Gabest
 * (C) 2006-2013, 2015 see Authors.txt
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
#include "mplayerc.h"
#include "FullscreenWnd.h"
#include "MainFrm.h"

IMPLEMENT_DYNAMIC(CFullscreenWnd, CMouseWndWithArtView)
CFullscreenWnd::CFullscreenWnd(CMainFrame* pMainFrame)
    : CMouseWndWithArtView(pMainFrame, true)
    , m_pMainFrame(pMainFrame)
{
}

bool CFullscreenWnd::IsWindow() const
{
    return !!m_hWnd;
}

BOOL CFullscreenWnd::PreTranslateMessage(MSG* pMsg)
{
    switch (pMsg->message) {
        case WM_KEYDOWN:
        case WM_KEYUP:
            m_pMainFrame->PostMessage(pMsg->message, pMsg->wParam, pMsg->lParam);
            return TRUE;
    }

    return __super::PreTranslateMessage(pMsg);
}

BOOL CFullscreenWnd::PreCreateWindow(CREATESTRUCT& cs)
{
    cs.style &= ~WS_BORDER;
    cs.lpszClass = AfxRegisterWndClass(CS_HREDRAW | CS_VREDRAW | CS_DBLCLKS | CS_NOCLOSE,
                                       ::LoadCursor(nullptr, IDC_ARROW), HBRUSH(COLOR_WINDOW + 1), nullptr);

    return __super::PreCreateWindow(cs);
}

LRESULT CFullscreenWnd::WindowProc(UINT message, WPARAM wParam, LPARAM lParam)
{
#if 0
    if (message == WM_NCACTIVATE && LOWORD(wParam) == WA_INACTIVE && m_pMainFrame->IsD3DFullScreenMode()) {
        return m_pMainFrame->m_pFullscreenWnd == this;
    }
#endif

    if (m_pMainFrame->m_pVideoWnd == this) {
        // the renderer is parented to this top-level window, so it gets the messages it would otherwise have hooked from here
        LRESULT ret = 0;
        switch (message) {
            case WM_CLOSE:
                break;
            case WM_MOUSEMOVE:
            case WM_LBUTTONDOWN:
            case WM_LBUTTONUP:
                // CMouseWnd will call madVR window proc
                break;
            default:
                if (m_pMainFrame->ForwardMessageToRenderer(m_hWnd, message, wParam, lParam, ret)) {
                    return ret;
                }
        }
    }

    return __super::WindowProc(message, wParam, lParam);
}

BEGIN_MESSAGE_MAP(CFullscreenWnd, CMouseWnd)
    ON_WM_ERASEBKGND()
    ON_WM_DESTROY()
END_MESSAGE_MAP()

BOOL CFullscreenWnd::OnEraseBkgnd(CDC* pDC)
{
    if (m_pMainFrame->m_fAudioOnly) {
        return __super::OnEraseBkgnd(pDC);
    }
    else {
        CRect r;
        GetClientRect(r);
        pDC->FillSolidRect(r, 0);
        return FALSE;
    }
}

void CFullscreenWnd::OnDestroy()
{
    __super::OnDestroy();

    CWnd* pMainWnd = AfxGetApp()->GetMainWnd();
    if (pMainWnd) {
        pMainWnd->SetActiveWindow();
    }
}

void CFullscreenWnd::SetCursor(LPCWSTR lpCursorName) {
    m_hCursor = ::LoadCursorW(nullptr, lpCursorName);
    m_bCursorVisible = true;
    PostMessageW(WM_SETCURSOR, 0, 0);
}
