/*
 * (C) 2012-2016 see Authors.txt
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
#include "PlayerBar.h"
#include "MainFrm.h"
#include "mplayerc.h"

IMPLEMENT_DYNAMIC(CPlayerBar, CSizingControlBarG)
CPlayerBar::CPlayerBar()
    : m_bAutohidden(false)
    , m_bHasActivePopup(false)
    , m_defDockBarID(0)
{
    EventRouter::EventSelection receives;
    receives.insert(MpcEvent::CHANGING_UI_LANGUAGE);
    GetEventd().Connect(m_eventc, receives, std::bind(&CPlayerBar::EventCallback, this, std::placeholders::_1));
}

CPlayerBar::~CPlayerBar()
{
}

void CPlayerBar::EventCallback(MpcEvent ev)
{
    switch (ev) {
        case MpcEvent::CHANGING_UI_LANGUAGE:
            ReloadTranslatableResources();
            break;
        default:
            ASSERT(FALSE);
    }
}

BEGIN_MESSAGE_MAP(CPlayerBar, CSizingControlBarG)
    ON_WM_WINDOWPOSCHANGED()
    ON_WM_ENTERMENULOOP()
    ON_WM_EXITMENULOOP()
END_MESSAGE_MAP()

void CPlayerBar::OnWindowPosChanged(WINDOWPOS* lpwndpos)
{
    if (lpwndpos->flags & SWP_SHOWWINDOW) {
        bool bWasAutohidden = IsAutohidden();
        SetAutohidden(false);
        if (lpwndpos->flags & SWP_FRAMECHANGED && !bWasAutohidden && !IsFloating()) {
            // the panel was re-docked
            if (auto pFrame = AfxGetMainFrame()) {
                // call MoveVideoWindow() manually because we don't receive WM_SIZE message
                // (probably because we disable locking the desktop window on what CControlBar relies)
                pFrame->MoveVideoWindow();
                // let the user see what he did and don't hide the panel for a while
                pFrame->m_controls.LockHideZone(pFrame->m_controls.GetPanelZone(this));
            }
        }
    }
    __super::OnWindowPosChanged(lpwndpos);
}

void CPlayerBar::OnEnterMenuLoop(BOOL bIsTrackPopupMenu)
{
    m_bHasActivePopup = true;
    __super::OnEnterMenuLoop(bIsTrackPopupMenu);
}

void CPlayerBar::OnExitMenuLoop(BOOL bIsTrackPopupMenu)
{
    m_bHasActivePopup = false;
    __super::OnExitMenuLoop(bIsTrackPopupMenu);
}

void CPlayerBar::OnBarStyleChange(DWORD dwOldStyle, DWORD dwNewStyle)
{
    DWORD dwChangedStyle = dwOldStyle ^ dwNewStyle;

    if (dwChangedStyle & CBRS_FLOATING) {
        SaveState();
    }

    __super::OnBarStyleChange(dwOldStyle, dwNewStyle);
}

BOOL CPlayerBar::Create(LPCTSTR lpszWindowName, CWnd* pParentWnd, UINT nID, UINT defDockBarID, CString const& strSettingName)
{
    m_defDockBarID = defDockBarID;
    m_strSettingName = strSettingName;

    return __super::Create(lpszWindowName, pParentWnd, nID);
}

void CPlayerBar::LoadState(CFrameWnd* pParent)
{
    CWinApp* pApp = AfxGetApp();

    CRect rcPrimary, rcExtendedDesktop;
    GetDesktopWindow()->GetWindowRect(&rcPrimary);
    rcExtendedDesktop.left   = GetSystemMetrics(SM_XVIRTUALSCREEN);
    rcExtendedDesktop.top    = GetSystemMetrics(SM_YVIRTUALSCREEN);
    rcExtendedDesktop.right  = rcExtendedDesktop.left + GetSystemMetrics(SM_CXVIRTUALSCREEN);
    rcExtendedDesktop.bottom = rcExtendedDesktop.top  + GetSystemMetrics(SM_CYVIRTUALSCREEN);

    CString section = _T("ToolBars\\") + m_strSettingName;

    __super::LoadState(section + _T("\\State"));

    UINT dockBarID = (UINT)pApp->GetProfileInt(section, _T("DockState"), m_defDockBarID);

    CPoint p;
    p.x = pApp->GetProfileInt(section, _T("DockPosX"), CW_USEDEFAULT);
    p.y = pApp->GetProfileInt(section, _T("DockPosY"), CW_USEDEFAULT);
    if (p.x != CW_USEDEFAULT && p.y != CW_USEDEFAULT) {
        if (p.x < rcExtendedDesktop.left) {
            p.x = rcExtendedDesktop.left;
        }
        if (p.y < rcExtendedDesktop.top) {
            p.y = rcExtendedDesktop.top;
        }
        if (p.x >= rcExtendedDesktop.right) {
            p.x = rcExtendedDesktop.right - 50;
        }
        if (p.y >= rcExtendedDesktop.bottom) {
            p.y = rcExtendedDesktop.bottom - 50;
        }
    } else {
        p = rcPrimary.CenterPoint();
        p.x -= m_szFloat.cx / 2;
        p.y -= m_szFloat.cy / 2;
    }

    ASSERT(m_pDockContext);
    m_pDockContext->m_uMRUDockID = m_defDockBarID;
    m_pDockContext->m_ptMRUFloatPos = p;

    if (dockBarID == AFX_IDW_DOCKBAR_FLOAT) {
        pParent->FloatControlBar(this, p);
    } else {
        pParent->DockControlBar(this, dockBarID);
    }
}

void CPlayerBar::SaveState()
{
    CWnd* parent = GetParent();
    if (!parent) return;
    if (nullptr == DYNAMIC_DOWNCAST(CDockBar, parent)) {
        //the logic below only works if we have a dockbar parent (including floating).
        //see CMiniDockFrameWnd::Create(); initially parent is set to parent frame which makes GetDlgCtrlID() meaningless to us
        return;
    }

    CWinApp* pApp = AfxGetApp();

    CString section = _T("ToolBars\\") + m_strSettingName;

    __super::SaveState(section + _T("\\State"));

    UINT dockBarID = parent->GetDlgCtrlID();

    if (dockBarID == AFX_IDW_DOCKBAR_FLOAT) {
        CRect r;
        parent->GetParent()->GetWindowRect(r);
        pApp->WriteProfileInt(section, _T("DockPosX"), r.left);
        pApp->WriteProfileInt(section, _T("DockPosY"), r.top);
    }

    pApp->WriteProfileInt(section, _T("DockState"), (INT)dockBarID);
}

void CPlayerBar::SetAutohidden(bool bValue)
{
    m_bAutohidden = bValue;
}

bool CPlayerBar::IsAutohidden() const
{
    return m_bAutohidden;
}

bool CPlayerBar::HasActivePopup() const
{
    return m_bHasActivePopup;
}

void CPlayerBar::SetHasActivePopup(bool bValue)
{
    m_bHasActivePopup = bValue;
}
