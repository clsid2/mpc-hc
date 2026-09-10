/*
 * (C) 2013-2015, 2017 see Authors.txt
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
#include "MouseTouch.h"
#include "MainFrm.h"
#include "mplayerc.h"
#include "FullscreenWnd.h"
#include <mvrInterfaces.h>

#define TRACE_LEFTCLICKS 0

#define CURSOR_HIDE_TIMEOUT 2000

CMouse::CMouse(CMainFrame* pMainFrm, bool bD3DFS/* = false*/)
    : m_bD3DFS(bD3DFS)
    , m_pMainFrame(pMainFrm)
    , m_dwMouseHiderStartTick(0)
    , m_bLeftDown(false)
    , m_bLeftUpDelayed(false)
    , m_bLeftDoubleStarted(false)
    , m_leftDoubleStartTime(0)
    , m_popupMenuUninitTime(0)
    , m_doubleclicktime((int)GetDoubleClickTime())
{
    m_cursors[Cursor::NONE] = nullptr;
    m_cursors[Cursor::ARROW] = LoadCursor(nullptr, IDC_ARROW);
    m_cursors[Cursor::HAND] = LoadCursor(nullptr, IDC_HAND);
    ResetToBlankState();

    EventRouter::EventSelection evs;
    evs.insert(MpcEvent::SWITCHING_TO_FULLSCREEN);
    evs.insert(MpcEvent::SWITCHED_TO_FULLSCREEN);
    evs.insert(MpcEvent::SWITCHING_TO_FULLSCREEN_D3D);
    evs.insert(MpcEvent::SWITCHED_TO_FULLSCREEN_D3D);
    evs.insert(MpcEvent::MEDIA_LOADED);
    evs.insert(MpcEvent::CONTEXT_MENU_POPUP_UNINITIALIZED);
    evs.insert(MpcEvent::SYSTEM_MENU_POPUP_INITIALIZED);
    GetEventd().Connect(m_eventc, evs, std::bind(&CMouse::EventCallback, this, std::placeholders::_1));
}

CMouse::~CMouse()
{
    StopMouseHider();
}

UINT CMouse::GetMouseFlags()
{
    UINT nFlags = 0;
    nFlags |= GetKeyState(VK_CONTROL)  < 0 ? MK_CONTROL  : 0;
    nFlags |= GetKeyState(VK_LBUTTON)  < 0 ? MK_LBUTTON  : 0;
    nFlags |= GetKeyState(VK_MBUTTON)  < 0 ? MK_MBUTTON  : 0;
    nFlags |= GetKeyState(VK_RBUTTON)  < 0 ? MK_RBUTTON  : 0;
    nFlags |= GetKeyState(VK_SHIFT)    < 0 ? MK_SHIFT    : 0;
    nFlags |= GetKeyState(VK_XBUTTON1) < 0 ? MK_XBUTTON1 : 0;
    nFlags |= GetKeyState(VK_XBUTTON2) < 0 ? MK_XBUTTON2 : 0;
    return nFlags;
}

bool CMouse::CursorOnRootWindow(const CPoint& screenPoint, const CFrameWnd& frameWnd)
{
    bool ret = false;

    CWnd* pWnd = CWnd::WindowFromPoint(screenPoint);
    CWnd* pRoot = pWnd ? pWnd->GetAncestor(GA_ROOT) : nullptr;

    // tooltips are special case
    if (pRoot && pRoot == pWnd) {
        CString strClass;
        VERIFY(GetClassName(pRoot->m_hWnd, strClass.GetBuffer(256), 256));
        strClass.ReleaseBuffer();
        if (strClass == _T("tooltips_class32")) {
            CWnd* pTooltipOwner = pWnd->GetParent();
            pRoot = pTooltipOwner ? pTooltipOwner->GetAncestor(GA_ROOT) : nullptr;
        }
    }

    if (pRoot) {
        ret = (pRoot->m_hWnd == frameWnd.m_hWnd);
    } else {
        ASSERT(FALSE);
    }

    return ret;
}
bool CMouse::CursorOnWindow(const CPoint& screenPoint, const CWnd& wnd)
{
    bool ret = false;
    CWnd* pWnd = CWnd::WindowFromPoint(screenPoint);
    if (pWnd) {
        ret = (pWnd->m_hWnd == wnd.m_hWnd);
    } else {
        ASSERT(FALSE);
    }
    return ret;
}

bool CMouse::Dragging()
{
    return m_drag == Drag::DRAGGED;
}

void CMouse::ResetToBlankState()
{
    StopMouseHider();
    m_bLeftDown = false;
    m_bTrackingMouseLeave = false;
    m_drag = Drag::NO_DRAG;
    m_cursor = Cursor::ARROW;
    m_switchingToFullscreen.first = false;
    if (m_bLeftUpDelayed) {
        m_bLeftUpDelayed = false;
        KillTimer(GetWnd(), (UINT_PTR)this);
    }
}

void CMouse::StartMouseHider(const CPoint& screenPoint)
{
    ASSERT(!m_pMainFrame->IsInteractiveVideo());
    m_mouseHiderStartScreenPoint = screenPoint;
    if (!m_bMouseHiderStarted) {
        // periodic timer is used here intentionally, recreating timer after each mouse move is more expensive
        auto t = m_bD3DFS ? CMainFrame::TimerHiderSubscriber::CURSOR_HIDER_D3DFS : CMainFrame::TimerHiderSubscriber::CURSOR_HIDER;
        m_pMainFrame->m_timerHider.Subscribe(t, std::bind(&CMouse::MouseHiderCallback, this));
        m_bMouseHiderStarted = true;
    }
    m_dwMouseHiderStartTick = GetTickCount64();
}
void CMouse::StopMouseHider()
{
    auto t = m_bD3DFS ? CMainFrame::TimerHiderSubscriber::CURSOR_HIDER_D3DFS : CMainFrame::TimerHiderSubscriber::CURSOR_HIDER;
    m_pMainFrame->m_timerHider.Unsubscribe(t);
    m_bMouseHiderStarted = false;
}
void CMouse::MouseHiderCallback()
{
    ASSERT(!m_pMainFrame->IsInteractiveVideo());
    if (GetTickCount64() > m_dwMouseHiderStartTick + CURSOR_HIDE_TIMEOUT) {
        StopMouseHider();
        ASSERT(m_cursor != Cursor::NONE);
        m_cursor = Cursor::NONE;
        CPoint screenPoint;
        VERIFY(GetCursorPos(&screenPoint));
        m_hideCursorPoint = screenPoint;
        ::SetCursor(m_cursors[m_cursor]);
    }
}

void CMouse::StartMouseLeaveTracker()
{
    ASSERT(!m_pMainFrame->IsInteractiveVideo());
    TRACKMOUSEEVENT tme = { sizeof(tme), TME_LEAVE, GetWnd() };
    if (TrackMouseEvent(&tme)) {
        m_bTrackingMouseLeave = true;
    } else {
        ASSERT(FALSE);
    }
}
void CMouse::StopMouseLeaveTracker()
{
    TRACKMOUSEEVENT tme = { sizeof(tme), TME_LEAVE | TME_CANCEL, GetWnd() };
    TrackMouseEvent(&tme);
    m_bTrackingMouseLeave = false;
}

CPoint CMouse::GetVideoPoint(const CPoint& point) const
{
    return m_bD3DFS ? point : CPoint(point - m_pMainFrame->m_wndView.GetVideoRect().TopLeft());
}

bool CMouse::IsOnFullscreenWindow() const
{
    if (m_pMainFrame->HasDedicatedFSVideoWindow()) {
        return m_pMainFrame->m_pDedicatedFSVideoWnd == this; //we are the fullscreen window
    } else {
        return &m_pMainFrame->m_wndView == this && m_pMainFrame->IsFullScreenMainFrame(); //we are the view and it is fullscreened
    }
}

WORD CMouse::AssignedMouseToCmd(UINT mouseValue, UINT nFlags) {
    CAppSettings& s = AfxGetAppSettings();

    CAppSettings::MOUSE_ASSIGNMENT mcmds = {};

    switch (mouseValue) {
    case wmcmd::MUP:       mcmds = s.MouseMiddleClick;  break;
    case wmcmd::X1UP:      mcmds = s.MouseX1Click;      break;
    case wmcmd::X2UP:      mcmds = s.MouseX2Click;      break;
    case wmcmd::WUP:       mcmds = s.MouseWheelUp;      break;
    case wmcmd::WDOWN:     mcmds = s.MouseWheelDown;    break;
    case wmcmd::WLEFT:     mcmds = s.MouseWheelLeft;    break;
    case wmcmd::WRIGHT:    mcmds = s.MouseWheelRight;   break;
    case wmcmd::LUP:       return (WORD)s.nMouseLeftClick;
    case wmcmd::LDBLCLK:   return (WORD)s.nMouseLeftDblClick;
    case wmcmd::RUP:       return (WORD)s.nMouseRightClick;
    }

    if (mcmds.ctrl && (nFlags & MK_CONTROL)) {
        return (WORD)mcmds.ctrl;
    }
    if (mcmds.shift && (nFlags & MK_SHIFT)) {
        return (WORD)mcmds.shift;
    }
    if (mcmds.rbtn && (nFlags & MK_RBUTTON)) {
        return (WORD)mcmds.rbtn;
    }

    return (WORD)mcmds.normal;
}

bool CMouse::OnButton(UINT id, const CPoint& point, int nFlags)
{
    bool ret = false;
    WORD cmd = AssignedMouseToCmd(id, nFlags);
    if (cmd) {
        m_pMainFrame->PostMessage(WM_COMMAND, cmd);
        ret = true;
    }
    return ret;
}

void CMouse::EventCallback(MpcEvent ev)
{
    CPoint screenPoint;
    VERIFY(GetCursorPos(&screenPoint));
    switch (ev) {
        case MpcEvent::SWITCHED_TO_FULLSCREEN:
        case MpcEvent::SWITCHED_TO_FULLSCREEN_D3D:
            m_switchingToFullscreen.first = false;
            break;
        case MpcEvent::SWITCHING_TO_FULLSCREEN:
        case MpcEvent::SWITCHING_TO_FULLSCREEN_D3D:
            m_switchingToFullscreen = std::make_pair(true, screenPoint);
        // no break
        case MpcEvent::MEDIA_LOADED:
            if (CursorOnWindow(screenPoint, GetWnd())) {
                SetCursor(screenPoint);
            }
            break;
        case MpcEvent::CONTEXT_MENU_POPUP_UNINITIALIZED:
            m_popupMenuUninitTime = GetMessageTime();
            break;
        case MpcEvent::SYSTEM_MENU_POPUP_INITIALIZED:
            if (!GetCapture() && CursorOnWindow(screenPoint, GetWnd())) {
                ::SetCursor(m_cursors[Cursor::ARROW]);
            }
            break;
        default:
            ASSERT(FALSE);
    }
}

// madVR compatibility layer for exclusive mode seekbar
bool CMouse::UsingMVR() const
{
    return !!m_pMainFrame->m_pMVRSR;
}
void CMouse::MVRMove(UINT nFlags, const CPoint& point)
{
    if (UsingMVR()) {
        WPARAM wp = nFlags;
        LPARAM lp = MAKELPARAM(point.x, point.y);
        LRESULT lr = 0;
        m_pMainFrame->m_pMVRSR->ParentWindowProc(GetWnd(), WM_MOUSEMOVE, &wp, &lp, &lr);
    }
}
bool CMouse::MVRDown(UINT nFlags, const CPoint& point)
{
    bool ret = false;
    if (UsingMVR()) {
        WPARAM wp = nFlags;
        LPARAM lp = MAKELPARAM(point.x, point.y);
        LRESULT lr = 0;
        ret = !!m_pMainFrame->m_pMVRSR->ParentWindowProc(GetWnd(), WM_LBUTTONDOWN, &wp, &lp, &lr);
    }
    return ret;
}
bool CMouse::MVRUp(UINT nFlags, const CPoint& point)
{
    bool ret = false;
    if (UsingMVR()) {
        WPARAM wp = nFlags;
        LPARAM lp = MAKELPARAM(point.x, point.y);
        LRESULT lr = 0;
        ret = !!m_pMainFrame->m_pMVRSR->ParentWindowProc(GetWnd(), WM_LBUTTONUP, &wp, &lp, &lr);
    }
    return ret;
}

// Left button
void CMouse::InternalOnLButtonDown(UINT nFlags, const CPoint& point)
{
    m_bLeftDown = false;
    GetWnd().SetFocus();
    SetCursor(nFlags, point);

    if (MVRDown(nFlags, point)) {
        return;
    }
    if (!UsingMVR() && m_pMainFrame->isSafeZone(point)) {
        return;
    }
    bool bIsOnFS = IsOnFullscreenWindow();
    if ((!m_bD3DFS || !bIsOnFS) && (abs(GetMessageTime() - m_popupMenuUninitTime) < 2)) {
        return;
    }
    if (m_pMainFrame->GetLoadState() == MLS::LOADED && m_pMainFrame->GetPlaybackMode() == PM_DVD && (m_pMainFrame->IsD3DFullScreenMode() ^ m_bD3DFS) == 0) {
        ULONG ulButtonTotal = 0;
        ULONG ulButtonCurrent = 0;     
        if (SUCCEEDED(m_pMainFrame->m_pDVDI->GetCurrentButton(&ulButtonTotal, &ulButtonCurrent)) && ulButtonTotal > 0) {
            ULONG ulButtonMousePos = 0;
            CPoint vp = GetVideoPoint(point);
            if (SUCCEEDED(m_pMainFrame->m_pDVDI->GetButtonAtPosition(vp, &ulButtonMousePos))) {
                if (SUCCEEDED(m_pMainFrame->m_pDVDC->SelectAndActivateButton(ulButtonMousePos))) {
                    return;
                }
            }
        } else {
            ASSERT(false);
        }
    }
    if (bIsOnFS && (m_bD3DFS || m_pMainFrame->IsFullScreenMainFrameExclusiveMPCVR()) && m_pMainFrame->m_OSD.OnLButtonDown(nFlags, point)) {
        return;
    }

#if TRACE_LEFTCLICKS
    TRACE(L"InternalOnLButtonDown\n");
#endif

    int msgtime = GetMessageTime();

    m_bLeftDown = true;
    bool bDouble = false;
    if (m_bLeftDoubleStarted && (msgtime - m_leftDoubleStartTime < m_doubleclicktime) &&
            CMouse::PointEqualsImprecise(m_leftDoubleStartPoint, point, GetSystemMetrics(SM_CXDOUBLECLK) / 2, GetSystemMetrics(SM_CYDOUBLECLK) / 2)) {
        m_bLeftDoubleStarted = false;
        bDouble = true;
    } else {
        m_bLeftDoubleStarted = true;
        m_leftDoubleStartTime = msgtime;
        m_leftDoubleStartPoint = point;
    }

    if (m_bLeftUpDelayed) {
        KillTimer(GetWnd(), (UINT_PTR)this);
        if (!bDouble) {
            PerformDelayedLeftUp();
        }
    }

    auto onButton = [&]() {
        GetWnd().SetCapture();
        bool ret = false;
        if (bIsOnFS || !m_pMainFrame->IsCaptionHidden()) {
            ret = OnButton(wmcmd::LDOWN, point);
        }
        if (bDouble) {
            // perform the LeftUp command before double-click
            // reason is that toggling fullscreen can move the video window, which can cause the LeftUp action to not be processed properly
            // ToDo: rewrite code to trigger doubleclick at second LeftUp instead of LeftDown
            if (m_bLeftUpDelayed) {
                // the first LeftUp was delayed and then skipped, so skip second one as well
                m_bLeftUpDelayed = false;
            } else {
#if TRACE_LEFTCLICKS
                TRACE(L"doing early LEFT UP\n");
#endif
                OnButton(wmcmd::LUP, point);
            }
#if TRACE_LEFTCLICKS
            TRACE(L"doing DOUBLE\n");
#endif
            ret = OnButton(wmcmd::LDBLCLK, point) || ret;
            m_bLeftDown = false; // skip next LEFT UP
        }
        if (!ret || bDouble) {
            ReleaseCapture();
        }
        return ret;
    };

    m_drag = (!onButton() && !bIsOnFS) ? Drag::BEGIN_DRAG : Drag::NO_DRAG;
    if (m_drag == Drag::BEGIN_DRAG) {
        GetWnd().SetCapture();
        m_beginDragPoint = point;
        GetWnd().ClientToScreen(&m_beginDragPoint);
    }
}

void CMouse::PerformDelayedLeftUp()
{
    m_bLeftUpDelayed = false;
    OnButton(wmcmd::LUP, m_LeftUpPoint);
    m_LeftUpPoint = CPoint();
}

void CMouse::OnTimerLeftUp(HWND hWnd, UINT nMsg, UINT_PTR nIDEvent, DWORD dwTime)
{
    CMouse* pCMouse = (CMouse*)nIDEvent;
    if (pCMouse && pCMouse->m_bLeftUpDelayed) {
        KillTimer(hWnd, nIDEvent);
        pCMouse->PerformDelayedLeftUp();
    }
}

void CMouse::InternalOnLButtonUp(UINT nFlags, const CPoint& point)
{
#if TRACE_LEFTCLICKS
    TRACE(L"InternalOnLButtonUp\n");
#endif
    ReleaseCapture();
    if (!MVRUp(nFlags, point)) {
        bool bIsOnFS = IsOnFullscreenWindow();
        if (!(bIsOnFS && (m_bD3DFS || m_pMainFrame->IsFullScreenMainFrameExclusiveMPCVR()) && m_pMainFrame->m_OSD.OnLButtonUp(nFlags, point))) {
            if (m_bLeftDown) {
                UINT delay = (UINT)AfxGetAppSettings().iMouseLeftUpDelay;
                if (delay > 0 && m_pMainFrame->GetLoadState() == MLS::LOADED) {
                    ASSERT(!m_bLeftUpDelayed);
                    m_bLeftUpDelayed = true;
                    m_LeftUpPoint = point;
                    SetTimer(GetWnd(), (UINT_PTR)this, std::min(delay, GetDoubleClickTime()), OnTimerLeftUp);
                } else {
                    OnButton(wmcmd::LUP, point);
                }
            } else {
                #if TRACE_LEFTCLICKS
                TRACE(L"skipped LEFT UP\n");
                #endif
            }
        }
    }

    m_drag = Drag::NO_DRAG;
    m_bLeftDown = false;
    SetCursor(nFlags, point);
}

// Middle button
void CMouse::InternalOnMButtonDown(UINT nFlags, const CPoint& point)
{
    SetCursor(nFlags, point);
    //all mouse commands operate on UP
    //OnButton(wmcmd::MDOWN, point);
}
void CMouse::InternalOnMButtonUp(UINT nFlags, const CPoint& point)
{
    m_bWaitingRButtonUp = false;
    OnButton(wmcmd::MUP, point, nFlags);
    SetCursor(nFlags, point);
}
void CMouse::InternalOnMButtonDblClk(UINT nFlags, const CPoint& point)
{
    SetCursor(nFlags, point);
    OnButton(wmcmd::MDOWN, point);
    OnButton(wmcmd::MDBLCLK, point);
}

// Right button
void CMouse::InternalOnRButtonDown(UINT nFlags, const CPoint& point)
{
    m_bWaitingRButtonUp = true;
    SetCursor(nFlags, point);
    OnButton(wmcmd::RDOWN, point);
}
void CMouse::InternalOnRButtonUp(UINT nFlags, const CPoint& point)
{
    if (m_bWaitingRButtonUp) {
        m_bWaitingRButtonUp = false;
        if (!m_pMainFrame->m_bIsMPCVRExclusiveMode) {
            OnButton(wmcmd::RUP, point);
        }
        SetCursor(nFlags, point);
    }
}
void CMouse::InternalOnRButtonDblClk(UINT nFlags, const CPoint& point)
{
    SetCursor(nFlags, point);
    OnButton(wmcmd::RDOWN, point);
    OnButton(wmcmd::RDBLCLK, point);
}

// Navigation buttons
bool CMouse::InternalOnXButtonDown(UINT nFlags, UINT nButton, const CPoint& point)
{
    SetCursor(nFlags, point);
    //all mouse commands operate on UP
    //return OnButton(nButton == XBUTTON1 ? wmcmd::X1DOWN : nButton == XBUTTON2 ? wmcmd::X2DOWN : wmcmd::NONE, point);
    return false;
}
bool CMouse::InternalOnXButtonUp(UINT nFlags, UINT nButton, const CPoint& point)
{
    m_bWaitingRButtonUp = false;
    bool ret = OnButton(nButton == XBUTTON1 ? wmcmd::X1UP : nButton == XBUTTON2 ? wmcmd::X2UP : wmcmd::NONE, point, nFlags);
    SetCursor(nFlags, point);
    return ret;
}
bool CMouse::InternalOnXButtonDblClk(UINT nFlags, UINT nButton, const CPoint& point)
{
    InternalOnXButtonDown(nFlags, nButton, point);
    return OnButton(nButton == XBUTTON1 ? wmcmd::X1DBLCLK : nButton == XBUTTON2 ? wmcmd::X2DBLCLK : wmcmd::NONE, point);
}

BOOL CMouse::InternalOnMouseWheel(UINT nFlags, short zDelta, const CPoint& point)
{
    m_bWaitingRButtonUp = false;
    return zDelta > 0 ? OnButton(wmcmd::WUP, point, nFlags) :
           zDelta < 0 ? OnButton(wmcmd::WDOWN, point, nFlags) :
           FALSE;
}

BOOL CMouse::OnMouseHWheelImpl(UINT nFlags, short zDelta, const CPoint& point) {
    m_bWaitingRButtonUp = false;
    return zDelta > 0 ? OnButton(wmcmd::WRIGHT, point, nFlags) :
        zDelta < 0 ? OnButton(wmcmd::WLEFT, point, nFlags) :
        FALSE;
}

bool CMouse::SelectCursor(const CPoint& screenPoint, const CPoint& clientPoint, UINT nFlags)
{
    const auto& s = AfxGetAppSettings();

    if ((m_bD3DFS || m_pMainFrame->IsFullScreenMainFrameExclusiveMPCVR()) && m_pMainFrame->m_OSD.OnMouseMove(nFlags, clientPoint)) {
        StopMouseHider();
        m_cursor = Cursor::HAND;
        return true;
    }

    if (m_pMainFrame->GetLoadState() == MLS::LOADED && m_pMainFrame->GetPlaybackMode() == PM_DVD && (m_pMainFrame->IsD3DFullScreenMode() ^ m_bD3DFS) == 0) {
        ULONG ulButtonTotal = 0;
        ULONG ulButtonCurrent = 0;     
        if (SUCCEEDED(m_pMainFrame->m_pDVDI->GetCurrentButton(&ulButtonTotal, &ulButtonCurrent)) && ulButtonTotal > 0) {
            ULONG ulButtonMousePos = 0;
            CPoint vp = GetVideoPoint(clientPoint);
            if (SUCCEEDED(m_pMainFrame->m_pDVDI->GetButtonAtPosition(vp, &ulButtonMousePos))) {
                StopMouseHider();
                m_cursor = Cursor::HAND;
                if (ulButtonMousePos != ulButtonCurrent) {
                    m_pMainFrame->m_pDVDC->SelectButton(ulButtonMousePos);
                }
                return true;
            }
        }
    }

    bool bMouseButtonDown = !!(nFlags & ~(MK_CONTROL | MK_SHIFT));
    bool bHidden = (m_cursor == Cursor::NONE);
    bool bHiddenAndMoved = bHidden && !PointEqualsImprecise(screenPoint, m_hideCursorPoint);
    bool bCanHide = !bMouseButtonDown &&
                    (m_pMainFrame->GetLoadState() == MLS::LOADED || m_pMainFrame->m_controls.DelayShowNotLoaded()) &&
                    !m_pMainFrame->IsInteractiveVideo() &&
                    (m_switchingToFullscreen.first || IsOnFullscreenWindow() ||
                     (s.bHideWindowedMousePointer && !(m_pMainFrame->IsD3DFullScreenMode() ^ m_bD3DFS)));

    if (m_switchingToFullscreen.first) {
        if (bCanHide && PointEqualsImprecise(screenPoint, m_switchingToFullscreen.second)) {
            StopMouseHider();
            m_cursor = Cursor::NONE;
            m_hideCursorPoint = screenPoint;
            return true;
        }
    }

    if (!bHidden || bHiddenAndMoved || !bCanHide) {
        m_cursor = Cursor::ARROW;
        if (bCanHide) {
            if (!m_bMouseHiderStarted || screenPoint != m_mouseHiderStartScreenPoint) {
                StartMouseHider(screenPoint);
            }
        } else if (m_bMouseHiderStarted) {
            StopMouseHider();
        }
    }

    return true;
}

void CMouse::SetCursor(UINT nFlags, const CPoint& screenPoint, const CPoint& clientPoint)
{
    if (SelectCursor(screenPoint, clientPoint, nFlags) && !m_pMainFrame->IsInteractiveVideo()) {
        ::SetCursor(m_cursors[m_cursor]);
    }
}
void CMouse::SetCursor(UINT nFlags, const CPoint& clientPoint)
{
    CPoint screenPoint(clientPoint);
    GetWnd().ClientToScreen(&screenPoint);
    SetCursor(nFlags, screenPoint, clientPoint);
}

void CMouse::SetCursor(const CPoint& screenPoint)
{
    ASSERT(CursorOnWindow(screenPoint, GetWnd()));
    CPoint clientPoint(screenPoint);
    GetWnd().ScreenToClient(&clientPoint);
    SetCursor(GetMouseFlags(), screenPoint, clientPoint);
}

bool CMouse::TestDrag(const CPoint& screenPoint)
{
    bool ret = false;
    if (m_drag == Drag::BEGIN_DRAG) {
        ASSERT(!IsOnFullscreenWindow());
        CRect r;
        GetWnd().GetWindowRect(r);
        int maxDiffX = r.Width() / (m_pMainFrame->IsZoomed() ? 16 : 40);
        int maxDiffY = r.Height() / (m_pMainFrame->IsZoomed() ? 16 : 40);
        CPoint diff = screenPoint - m_beginDragPoint;
        bool checkDrag = abs(diff.x) > maxDiffX || abs(diff.y) > maxDiffY;

        if (checkDrag) {
            bool bUpAssigned = !!AssignedMouseToCmd(wmcmd::LUP,0);
            if ((!bUpAssigned && screenPoint != m_beginDragPoint) ||
                (bUpAssigned && !PointEqualsImprecise(screenPoint, m_beginDragPoint,
                    GetSystemMetrics(SM_CXDRAG), GetSystemMetrics(SM_CYDRAG)))) {
                VERIFY(ReleaseCapture());
                m_pMainFrame->PostMessage(WM_NCLBUTTONDOWN, HTCAPTION, MAKELPARAM(m_beginDragPoint.x, m_beginDragPoint.y));
                m_drag = Drag::DRAGGED;
                m_bLeftDown = false;
                ret = true;
            }
        }
    } else {
        m_drag = Drag::NO_DRAG;
    }
    return ret;
}

BOOL CMouse::InternalOnSetCursor(CWnd* pWnd, UINT nHitTest, UINT message)
{
    return nHitTest == HTCLIENT;
}

void CMouse::InternalOnMouseMove(UINT nFlags, const CPoint& point)
{
    CPoint screenPoint(point);
    GetWnd().ClientToScreen(&screenPoint);

    if (!TestDrag(screenPoint) && !m_pMainFrame->IsInteractiveVideo()) {
        if (!m_bTrackingMouseLeave) {
            StartMouseLeaveTracker();
        }
        SetCursor(nFlags, screenPoint, point);
        MVRMove(nFlags, point);
    }

    m_pMainFrame->UpdateControlState(CMainFrame::UPDATE_CONTROLS_VISIBILITY);
}

void CMouse::InternalOnMouseLeave()
{
    // A renderer child window created on our thread (MPCVR in exclusive mode)
    // makes TrackMouseEvent report a leave while the cursor is still on the video.
    // Ignore it; the next mouse move re-arms the tracker.
    CPoint screenPoint;
    if (GetCursorPos(&screenPoint)) {
        HWND hUnder = ::WindowFromPoint(screenPoint);
        if (hUnder && (hUnder == GetWnd().m_hWnd || ::IsChild(GetWnd().m_hWnd, hUnder))) {
            m_bTrackingMouseLeave = false;
            return;
        }
    }
    StopMouseHider();
    m_bTrackingMouseLeave = false;
    m_cursor = Cursor::ARROW;
    if (m_bD3DFS || m_pMainFrame->IsFullScreenMainFrameExclusiveMPCVR()) {
        m_pMainFrame->m_OSD.OnMouseLeave();
    }
}

void CMouse::InternalOnDestroy()
{
    ResetToBlankState();
}

CMouseWnd::CMouseWnd(CMainFrame* pMainFrm, bool bD3DFS/* = false*/)
    : CMouse(pMainFrm, bD3DFS)
{
}

IMPLEMENT_DYNAMIC(CMouseWnd, CWnd)
BEGIN_MESSAGE_MAP(CMouseWnd, CWnd)
    ON_WM_LBUTTONDOWN()
    ON_WM_LBUTTONUP()
    ON_WM_LBUTTONDBLCLK()
    ON_WM_MBUTTONDOWN()
    ON_WM_MBUTTONUP()
    ON_WM_MBUTTONDBLCLK()
    ON_WM_RBUTTONDOWN()
    ON_WM_RBUTTONUP()
    ON_WM_RBUTTONDBLCLK()
    ON_WM_XBUTTONDOWN()
    ON_WM_XBUTTONUP()
    ON_WM_XBUTTONDBLCLK()
    ON_WM_MOUSEWHEEL()
    ON_WM_MOUSEHWHEEL()
    ON_WM_SETCURSOR()
    ON_WM_MOUSEMOVE()
    ON_WM_MOUSELEAVE()
    ON_WM_DESTROY()
END_MESSAGE_MAP()

void CMouseWnd::OnLButtonDown(UINT nFlags, CPoint point)
{
    CMouse::InternalOnLButtonDown(nFlags, point);
}
void CMouseWnd::OnLButtonUp(UINT nFlags, CPoint point)
{
    CMouse::InternalOnLButtonUp(nFlags, point);
}
void CMouseWnd::OnLButtonDblClk(UINT nFlags, CPoint point)
{
    CMouse::InternalOnLButtonDown(nFlags, point);
}

void CMouseWnd::OnMButtonDown(UINT nFlags, CPoint point)
{
    CMouse::InternalOnMButtonDown(nFlags, point);
}
void CMouseWnd::OnMButtonUp(UINT nFlags, CPoint point)
{
    CMouse::InternalOnMButtonUp(nFlags, point);
}
void CMouseWnd::OnMButtonDblClk(UINT nFlags, CPoint point)
{
    CMouse::InternalOnMButtonDblClk(nFlags, point);
}

void CMouseWnd::OnRButtonDown(UINT nFlags, CPoint point)
{
    CMouse::InternalOnRButtonDown(nFlags, point);
}
void CMouseWnd::OnRButtonUp(UINT nFlags, CPoint point)
{
    CMouse::InternalOnRButtonUp(nFlags, point);
}
void CMouseWnd::OnRButtonDblClk(UINT nFlags, CPoint point)
{
    CMouse::InternalOnRButtonDblClk(nFlags, point);
}

void CMouseWnd::OnXButtonDown(UINT nFlags, UINT nButton, CPoint point)
{
    if (!CMouse::InternalOnXButtonDown(nFlags, nButton, point)) {
        CWnd::OnXButtonDown(nFlags, nButton, point);
    }
}
void CMouseWnd::OnXButtonUp(UINT nFlags, UINT nButton, CPoint point)
{
    if (!CMouse::InternalOnXButtonUp(nFlags, nButton, point)) {
        CWnd::OnXButtonUp(nFlags, nButton, point);
    }
}
void CMouseWnd::OnXButtonDblClk(UINT nFlags, UINT nButton, CPoint point)
{
    if (!CMouse::InternalOnXButtonDblClk(nFlags, nButton, point)) {
        CWnd::OnXButtonDblClk(nFlags, nButton, point);
    }
}

BOOL CMouseWnd::OnMouseWheel(UINT nFlags, short zDelta, CPoint point)
{
    return CMouse::InternalOnMouseWheel(nFlags, zDelta, point);
}

void CMouseWnd::OnMouseHWheel(UINT nFlags, short zDelta, CPoint point) {
    if (!CMouse::OnMouseHWheelImpl(nFlags, zDelta, point)) {
        Default();
    }
}

BOOL CMouseWnd::OnSetCursor(CWnd* pWnd, UINT nHitTest, UINT message)
{
    return CMouse::InternalOnSetCursor(pWnd, nHitTest, message) ||
           CWnd::OnSetCursor(pWnd, nHitTest, message);
}

void CMouseWnd::OnMouseMove(UINT nFlags, CPoint point)
{
    CMouse::InternalOnMouseMove(nFlags, point);
    CWnd::OnMouseMove(nFlags, point);
}

void CMouseWnd::OnMouseLeave()
{
    CMouse::InternalOnMouseLeave();
    CWnd::OnMouseLeave();
}

void CMouseWnd::OnDestroy()
{
    CMouse::InternalOnDestroy();
    CWnd::OnDestroy();
}

std::unordered_set<const CWnd*> CMainFrameMouseHook::GetRoots()
{
    std::unordered_set<const CWnd*> ret;
    const CMainFrame* pMainFrame = AfxGetMainFrame();
    ASSERT(pMainFrame);
    if (pMainFrame) {
        ret.emplace(pMainFrame);
        if (pMainFrame->IsD3DFullScreenMode()) {
            ret.emplace(pMainFrame->m_pDedicatedFSVideoWnd);
        }
    }
    return ret;
}
