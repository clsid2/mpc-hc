/*
 * (C) 2010-2014 see Authors.txt
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
#include "PlayerNavigationBar.h"
#include "DSUtil.h"
#include "mplayerc.h"

// CPlayerNavigationBar

IMPLEMENT_DYNAMIC(CPlayerNavigationBar, CMPCThemePlayerBar)
CPlayerNavigationBar::CPlayerNavigationBar(CMainFrame* pMainFrame)
    : CMPCThemePlayerBar(pMainFrame)
    , m_pParent(nullptr)
    , m_navdlg(pMainFrame)
{
    GetEventd().Connect(m_eventc, {
        MpcEvent::DPI_CHANGED,
    }, std::bind(&CPlayerNavigationBar::EventCallback, this, std::placeholders::_1));
}

BOOL CPlayerNavigationBar::Create(CWnd* pParentWnd, UINT defDockBarID)
{
    if (!CMPCThemePlayerBar::Create(ResStr(IDS_NAVIGATION_BAR), pParentWnd, ID_VIEW_NAVIGATION, defDockBarID, _T("Navigation Bar"))) {
        return FALSE;
    }

    m_pParent = pParentWnd;
    m_navdlg.Create(this);
    m_navdlg.ShowWindow(SW_SHOWNORMAL);

    CRect r;
    m_navdlg.GetWindowRect(r);
    m_szMinVert = m_szVert = r.Size();
    m_szMinHorz = m_szHorz = r.Size();
    m_szMinFloat = m_szFloat = r.Size();
    m_bFixedFloat = true;
    m_szFixedFloat = r.Size();

    return TRUE;
}

void CPlayerNavigationBar::ReloadTranslatableResources()
{
    SetWindowText(ResStr(IDS_NAVIGATION_BAR));

    m_navdlg.DestroyWindow();
    m_navdlg.Create(this);
    CRect r;
    GetClientRect(r);
    m_navdlg.MoveWindow(r);
    m_navdlg.UpdateElementList();
    m_navdlg.ShowWindow(SW_SHOWNORMAL);
}

static HWND g_parentFrame = nullptr;
static WNDPROC g_parentFrameOrigWndProc = nullptr;
LRESULT CALLBACK ParentFrameSubclassWndProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    ASSERT(g_parentFrameOrigWndProc);
    WNDPROC origWndProc = g_parentFrameOrigWndProc;
    if (message == WM_SYSCOMMAND && wParam == SC_CLOSE) {
        AfxGetAppSettings().fHideNavigation = true;
    } else if (message == WM_NCDESTROY) {
        if ((WNDPROC)GetWindowLongPtr(hwnd, GWLP_WNDPROC) == ParentFrameSubclassWndProc) {
            SetWindowLongPtr(hwnd, GWLP_WNDPROC, (LONG_PTR)origWndProc);
        }
        if (hwnd == g_parentFrame) {
            g_parentFrame = nullptr;
            g_parentFrameOrigWndProc = nullptr;
        }
    }
    return CallWindowProc(origWndProc, hwnd, message, wParam, lParam);
}

static void SubclassParentFrame(HWND hwnd)
{
    ASSERT(!g_parentFrame);
    if ((WNDPROC)GetWindowLongPtr(hwnd, GWLP_WNDPROC) != ParentFrameSubclassWndProc) {
        g_parentFrameOrigWndProc = (WNDPROC)SetWindowLongPtr(hwnd, GWLP_WNDPROC, (LONG_PTR)ParentFrameSubclassWndProc);
        g_parentFrame = hwnd;
    }
}

static void UnsubclassParentFrame()
{
    if (g_parentFrame && IsWindow(g_parentFrame)) {
        // if something subclassed the frame after us, it still calls into us, so keep the original procedure
        if ((WNDPROC)GetWindowLongPtr(g_parentFrame, GWLP_WNDPROC) != ParentFrameSubclassWndProc) {
            return;
        }
        SetWindowLongPtr(g_parentFrame, GWLP_WNDPROC, (LONG_PTR)g_parentFrameOrigWndProc);
    }
    g_parentFrame = nullptr;
    g_parentFrameOrigWndProc = nullptr;
}

void CPlayerNavigationBar::OnUpdateCmdUI(CFrameWnd* pTarget, BOOL bDisableIfNoHndler)
{
    __super::OnUpdateCmdUI(pTarget, bDisableIfNoHndler);

    m_navdlg.UpdateDialogControls(&m_navdlg, bDisableIfNoHndler);
}

BOOL CPlayerNavigationBar::PreTranslateMessage(MSG* pMsg)
{
    if (CWnd* pParent1 = GetParent()) {
        CWnd* pParent2 = pParent1->GetParent();
        if (pParent2 != m_pParent) {
            if (pParent2->m_hWnd != g_parentFrame) {
                UnsubclassParentFrame();
                if (!g_parentFrame) {
                    SubclassParentFrame(pParent2->m_hWnd);
                }
            }
        } else {
            UnsubclassParentFrame();
        }
    }

    if (IsWindow(pMsg->hwnd) && IsVisible() && pMsg->message >= WM_KEYFIRST && pMsg->message <= WM_KEYLAST) {
        if (IsDialogMessage(pMsg)) {
            return TRUE;
        }
    }

    return __super::PreTranslateMessage(pMsg);
}

BEGIN_MESSAGE_MAP(CPlayerNavigationBar, CMPCThemePlayerBar)
    ON_WM_SIZE()
    ON_WM_NCLBUTTONUP()
END_MESSAGE_MAP()

// CPlayerNavigationBar message handlers

void CPlayerNavigationBar::OnSize(UINT nType, int cx, int cy)
{
    __super::OnSize(nType, cx, cy);
    if (::IsWindow(m_navdlg.m_hWnd)) {
        CRect r;
        GetClientRect(r);
        m_navdlg.MoveWindow(r);
    }
}

void CPlayerNavigationBar::OnNcLButtonUp(UINT nHitTest, CPoint point)
{
    __super::OnNcLButtonUp(nHitTest, point);

    if (nHitTest == HTCLOSE) {
        AfxGetAppSettings().fHideNavigation = true;
    }
}

void CPlayerNavigationBar::EventCallback(MpcEvent ev) {
    switch (ev) {
    case MpcEvent::DPI_CHANGED:
        InitializeSize();
        break;

    default:
        ASSERT(FALSE);
    }
}

