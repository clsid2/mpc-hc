/*
 * (C) 2013-2017 see Authors.txt
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
#include "MainFrmControls.h"
#include "MainFrm.h"
#include "mplayerc.h"
#include <mvrInterfaces.h>

#define DELAY_SHOW_NOT_LOADED_TIME_MS 3000

CMainFrameControls::ControlsVisibilityState::ControlsVisibilityState()
    : nVisibleCS(0)
    , bLastCanAutoHideToolbars(false)
    , bLastCanAutoHidePanels(false)
    , bLastHaveExclusiveSeekbar(false)
{
    nCurrentCS = AfxGetAppSettings().nCS;
}

UINT CMainFrameControls::GetEffectiveToolbarsSelection()
{
    const auto& s = AfxGetAppSettings();
    UINT ret = s.nCS;
    if (m_pMainFrame->GetPlaybackMode() == PM_DIGITAL_CAPTURE
            || m_pMainFrame->GetPlaybackMode() == PM_ANALOG_CAPTURE) {
        ret &= ~CS_SEEKBAR;
    }
    return ret;
}

bool CMainFrameControls::ShowToolbarsSelection()
{
    auto& st = m_controlsVisibilityState;
    const auto newCS = GetEffectiveToolbarsSelection();
    bool bRecalcLayout = false;
    if (!InFullscreenWithPermahiddenToolbars()) {
        const bool bResize = (newCS != st.nCurrentCS && !ToolbarsCoverVideo() && !m_pMainFrame->IsZoomed());
        if (bResize) {
            CRect newRect;
            m_pMainFrame->GetWindowRect(newRect);
            newRect.bottom -= GetToolbarsHeight(st.nCurrentCS);
            bRecalcLayout = ShowToolbars(newCS);
            st.nCurrentCS = newCS; // some toolbars' height may depend on ControlChecked()
            newRect.bottom += GetToolbarsHeight(st.nCurrentCS);
            m_pMainFrame->MoveWindow(newRect);
        } else {
            bRecalcLayout = ShowToolbars(newCS);
        }
    }
    st.nCurrentCS = newCS;
    return bRecalcLayout;
}

bool CMainFrameControls::ShowToolbars(UINT nCS)
{
    bool bRecalcLayout = false;
    auto& st = m_controlsVisibilityState;
    if (nCS != st.nVisibleCS) {
        m_pMainFrame->m_pLastBar = nullptr;
        int i = 1;
        for (const auto& pair : m_toolbars) {
            auto& pCB = pair.second;
            if (pCB->m_hWnd) {
                if (nCS & i) {
                    m_pMainFrame->ShowControlBar(pCB, TRUE, TRUE);
                    m_pMainFrame->m_pLastBar = pCB;
                } else {
                    m_pMainFrame->ShowControlBar(pCB, FALSE, TRUE);
                }
            } else {
                ASSERT(FALSE);
            }        
            i <<= 1;
        }
        st.nVisibleCS = nCS;
        bRecalcLayout = true;
    }
    return bRecalcLayout;
}

unsigned CMainFrameControls::GetToolbarsHeight(UINT nCS) const
{
    unsigned ret = 0;
    int i = 1;
    for (const auto& pair : m_toolbars) {
        auto& pCB = pair.second;
        if ((nCS & i) && IsWindow(pCB->m_hWnd)) {
            ret += pCB->CalcFixedLayout(TRUE, TRUE).cy;
        }
        i <<= 1;
    }
    return ret;
}

void CMainFrameControls::DelayShowNotLoadedCallback()
{
    ASSERT(m_bDelayShowNotLoaded);
    m_bDelayShowNotLoaded = false;
    UpdateToolbarsVisibility();
}

bool CMainFrameControls::InFullscreenWithPermahiddenToolbars()
{
    const auto& s = AfxGetAppSettings();
    return m_pMainFrame->IsFullScreenMainFrame() && s.bHideFullscreenControls &&
           s.eHideFullscreenControlsPolicy == CAppSettings::HideFullscreenControlsPolicy::SHOW_NEVER;
}

bool CMainFrameControls::InFullscreenWithPermahiddenDockedPanels()
{
    const auto& s = AfxGetAppSettings();
    return m_pMainFrame->IsFullScreenMainFrame() && s.bHideFullscreenControls && s.bHideFullscreenDockedPanels &&
           s.eHideFullscreenControlsPolicy == CAppSettings::HideFullscreenControlsPolicy::SHOW_NEVER;
}

CMainFrameControls::CMainFrameControls(CMainFrame* pMainFrame)
    : m_pMainFrame(pMainFrame)
    , m_bDelayShowNotLoaded(false)
{
}

CMainFrameControls::~CMainFrameControls()
{
    DelayShowNotLoaded(false);
    if (!m_zoneHideTicks.empty()) {
        m_pMainFrame->m_timerHider.Unsubscribe(CMainFrame::TimerHiderSubscriber::TOOLBARS_HIDER);
    }
}

bool CMainFrameControls::ControlChecked(Toolbar toolbar)
{
    const auto& st = m_controlsVisibilityState;
    const auto nID = static_cast<UINT>(toolbar);
    return !!(st.nCurrentCS & (1 << nID));
}

bool CMainFrameControls::ControlChecked(Panel panel)
{
    auto pBar = m_panels[panel];
    return pBar->IsAutohidden() || pBar->IsWindowVisible();
}

void CMainFrameControls::ToggleControl(Toolbar toolbar)
{
    const auto& s = AfxGetAppSettings();
    const auto& st = m_controlsVisibilityState;
    const auto nID = static_cast<UINT>(toolbar);
    auto nCS = s.nCS | st.nCurrentCS;
    nCS ^= (1 << nID);
    SetToolbarsSelection(nCS, true);
}

void CMainFrameControls::ToggleControl(Panel panel)
{
    auto pBar = m_panels[panel];
    if (pBar->IsWindowVisible()) {
        m_pMainFrame->ShowControlBar(pBar, FALSE, FALSE);
    } else if (pBar->IsAutohidden()) {
        pBar->SetAutohidden(false);
    } else {
        DockZone zone = GetPanelZone(panel);
        if (!InFullscreenWithPermahiddenDockedPanels() || zone == DOCK_NONE) {
            m_pMainFrame->ShowControlBar(pBar, TRUE, FALSE);
            if (zone != DOCK_NONE) {
                LockHideZone(zone);
            }
        } else {
            pBar->SetAutohidden(true);
        }
    }
}

void CMainFrameControls::SetToolbarsSelection(UINT nCS, bool bDelayHide/* = false*/)
{
    DelayShowNotLoaded(false);
    if (bDelayHide) {
        LockHideZone(DOCK_BOTTOM);
    }
    AfxGetAppSettings().nCS = nCS;
    if (ShowToolbarsSelection()) {
        m_pMainFrame->RecalcLayout();
    }
}

void CMainFrameControls::LoadState()
{
    ASSERT(!m_panels.empty());
    for (const auto& pair : m_panels) {
        auto pBar = pair.second;
        pBar->LoadState(m_pMainFrame);
        if (pBar->IsFloating() && pBar->IsAutohidden()) {
            pBar->SetAutohidden(false);
            bool delay = pair.first != CMainFrameControls::Panel::PLAYLIST;
            m_pMainFrame->ShowControlBar(pBar, TRUE, delay);
        }
    }
}

void CMainFrameControls::SaveState()
{
    ASSERT(!m_panels.empty());
    for (const auto& pair : m_panels) {
        pair.second->SaveState();
    }
}

void CMainFrameControls::GetDockZones(unsigned& uTop, unsigned& uLeft, unsigned& uRight, unsigned& uBottom, bool bDoEnum/* = true*/)
{
    GetDockZonesInternal(uTop, uLeft, uRight, uBottom, bDoEnum, false);
}

void CMainFrameControls::GetVisibleDockZones(unsigned& uTop, unsigned& uLeft, unsigned& uRight, unsigned& uBottom, bool bDoEnum/* = true*/)
{
    GetDockZonesInternal(uTop, uLeft, uRight, uBottom, bDoEnum, true);
}

CSize CMainFrameControls::GetDockZonesMinSize(unsigned uSaneFallback)
{
    EnumPanelZones();

    const long saneX = m_pMainFrame->m_dpi.ScaleX(uSaneFallback);
    const long saneY = m_pMainFrame->m_dpi.ScaleY(uSaneFallback);

    auto calcDock = [&](DockZone zone) {
        bool bHorz = (zone == DOCK_TOP || zone == DOCK_BOTTOM);
        const auto& panels = m_panelZones.find(zone)->second;
        auto pDock = m_panelDocks.find(zone)->second;
        unsigned stackSize = 0;
        unsigned maxRowSize = 0, rowSize = 0;
        bool bNewRow = true;
        for (int i = 0; i < pDock->m_arrBars.GetCount(); i++) {
            auto pBar = static_cast<CPlayerBar*>(pDock->m_arrBars[i]);
            if (!pBar && !bNewRow) {
                bNewRow = true;
                maxRowSize = std::max(maxRowSize, rowSize);
                rowSize = 0;
            }
            if (pBar) {
                for (const auto panel : panels) {
                    if (m_panels[panel] == pBar) {
                        rowSize += (rowSize ? 8 : 6);
                        rowSize += (bHorz ? std::max(saneX, pBar->m_szMinHorz.cx) : std::max(saneY, pBar->m_szMinVert.cy));
                        rowSize += pBar->m_cxEdge;
                        if (bNewRow) {
                            CSize size = pBar->CalcFixedLayout(TRUE, bHorz);
                            stackSize += (bHorz ? size.cy - 2 : (stackSize ? size.cx - 2 : size.cx - 4));
                            bNewRow = false;
                        }
                        break;
                    }
                }
            }
        }
        return bHorz ? CSize(maxRowSize, stackSize) : CSize(stackSize, maxRowSize);
    };

    const CSize sizeTop(m_panelZones.find(DOCK_TOP) != std::end(m_panelZones) ? calcDock(DOCK_TOP) : 0);
    const CSize sizeLeft(m_panelZones.find(DOCK_LEFT) != std::end(m_panelZones) ? calcDock(DOCK_LEFT) : 0);
    const CSize sizeRight(m_panelZones.find(DOCK_RIGHT) != std::end(m_panelZones) ? calcDock(DOCK_RIGHT) : 0);
    const CSize sizeBottom(m_panelZones.find(DOCK_BOTTOM) != std::end(m_panelZones) ? calcDock(DOCK_BOTTOM) : 0);

    CSize ret;
    ret.cx = std::max(sizeLeft.cx + sizeRight.cx, std::max(sizeTop.cx, sizeBottom.cx));
    ret.cy = sizeTop.cy + sizeBottom.cy + std::max(sizeLeft.cy, sizeRight.cy);
    const unsigned uToolbars = GetToolbarsHeight();
    if (uToolbars) {
        ret.cx = std::max(ret.cx, saneX);
    }
    ret.cy += uToolbars;

    return ret;
}

bool CMainFrameControls::PanelsCoverVideo() const
{
    const auto& s = AfxGetAppSettings();
    return m_pMainFrame->IsFullScreenMainFrame() && !m_pMainFrame->m_bIsMPCVRExclusiveMode ||
        (!m_pMainFrame->IsD3DFullScreenMode() && s.bHideWindowedControls && s.bHideFullscreenControls && s.bHideFullscreenDockedPanels &&
                                           s.eHideFullscreenControlsPolicy != CAppSettings::HideFullscreenControlsPolicy::SHOW_NEVER);
}

bool CMainFrameControls::ToolbarsCoverVideo() const
{
    const auto& s = AfxGetAppSettings();
    return m_pMainFrame->IsFullScreenMainFrame() && !m_pMainFrame->m_bIsMPCVRExclusiveMode ||
        (!m_pMainFrame->IsD3DFullScreenMode() && s.bHideWindowedControls && s.bHideFullscreenControls &&
                                           s.eHideFullscreenControlsPolicy != CAppSettings::HideFullscreenControlsPolicy::SHOW_NEVER);
}

bool ToolbarInputActive(CPlayerBar *bar) {
    HWND capture = GetCapture();
    HWND focus = GetFocus();

    if (capture == nullptr && focus == nullptr) {
        return false;
    }

    if (capture == bar->m_hWnd) {
        return true;
    }

    if (CWnd* pChildDialog = bar->GetWindow(GW_CHILD)) {
        CWnd* pChild = pChildDialog->GetWindow(GW_CHILD);
        while (pChild) {
            if (CComboBox* cb = DYNAMIC_DOWNCAST(CComboBox, pChild)) {
                COMBOBOXINFO cbi = { sizeof(COMBOBOXINFO) };
                if (cb->GetComboBoxInfo(&cbi)) {
                    if (cbi.hwndList == capture) {
                        return true;
                    }
                }
            } else if (CEdit* e = DYNAMIC_DOWNCAST(CEdit, pChild)) {
                if (e->m_hWnd == focus) {
                    return true;
                }
            }
            pChild = pChild->GetNextWindow();
        }
    }
    return false;
}

void CMainFrameControls::UpdateToolbarsVisibility()
{
    ASSERT(GetCurrentThreadId() == AfxGetApp()->m_nThreadID);

    const auto& s = AfxGetAppSettings();
    auto& st = m_controlsVisibilityState;
    const auto ePolicy = s.eHideFullscreenControlsPolicy;
    const unsigned uTimeout = s.uHideFullscreenControlsDelay;

    CPoint screenPoint;
    //VERIFY(GetCursorPos(&screenPoint));
    if (!GetCursorPos(&screenPoint)) screenPoint = { 100, 100 };
    CPoint clientPoint(screenPoint);
    m_pMainFrame->ScreenToClient(&clientPoint);

    const MLS mls = m_pMainFrame->GetLoadState();

    const bool bCanAutoHide = s.bHideFullscreenControls && (mls == MLS::LOADED || m_bDelayShowNotLoaded) &&
                              (m_pMainFrame->IsFullScreenMainFrame() || s.bHideWindowedControls && !m_pMainFrame->IsFullScreenSeparate()) &&
                              ePolicy != CAppSettings::HideFullscreenControlsPolicy::SHOW_NEVER;
    const bool bCanHideDockedPanels = s.bHideFullscreenDockedPanels;

    bool bEnumedPanelZones = false;

    struct {
        int maskShow, maskHide;
        void show(int mask) {
            maskShow |= mask;
            maskHide &= ~mask;
        }
        void hide(int mask) {
            maskHide |= mask;
            maskShow &= ~mask;
        }
    } mask = { 0, 0 };
    const int maskAll = DOCK_LEFT | DOCK_RIGHT | DOCK_TOP | DOCK_BOTTOM;

    bool bRecalcLayout = false;

    bool bExclSeekbar = false;
    if (m_pMainFrame->IsFullScreenMainFrame() && (mls == MLS::LOADED) && m_pMainFrame->m_pMVRS) {
        BOOL bOptExcl = FALSE, bOptExclSeekbar = FALSE;
        VERIFY(m_pMainFrame->m_pMVRS->SettingsGetBoolean(L"enableExclusive", &bOptExcl));
        VERIFY(m_pMainFrame->m_pMVRS->SettingsGetBoolean(L"enableSeekbar", &bOptExclSeekbar));
        bExclSeekbar = (bOptExcl && bOptExclSeekbar);
    } else if (m_bDelayShowNotLoaded && st.bLastHaveExclusiveSeekbar) {
        bExclSeekbar = true;
    } else if (m_pMainFrame->IsFullScreenMainFrameExclusiveMPCVR()) {
        bExclSeekbar = true;
    }

    if (m_pMainFrame->IsFullScreenMainFrame() && s.bHideFullscreenControls &&
            ePolicy == CAppSettings::HideFullscreenControlsPolicy::SHOW_NEVER) {
        // hide completely
        mask.hide(maskAll);
    } else if (bCanAutoHide) {
        if (m_pMainFrame->m_wndSeekBar.DraggingThumb() || m_pMainFrame->m_bTBDropdownActive) {
            // show bottom while dragging the seekbar thumb
            mask.show(DOCK_BOTTOM);
        } else {
            if (uTimeout == 0) {
                // hide initially when the timeout is zero; testCapture() and testHovering() may override it
                mask.hide(maskAll);
                // but exclude zone locks
                for (const auto zone : m_zoneHideLocks) {
                    mask.maskHide &= ~zone;
                }
            }
            const bool bOnWindow = CMouse::CursorOnRootWindow(screenPoint, *m_pMainFrame);
            auto testCapture = [&]() {
                bool ret = false;
                if (HWND hWnd = GetCapture()) {
                    if (hWnd != m_pMainFrame->m_hWnd) {
                        if (GetAncestor(hWnd, GA_ROOT) == m_pMainFrame->m_hWnd) {
                            if (!bEnumedPanelZones) {
                                EnumPanelZones();
                                bEnumedPanelZones = true;
                            }
                            std::map<HWND, DockZone> targetParents;
                            for (const auto& pair : m_panelDocks) {
                                targetParents.emplace(std::make_pair(pair.second->m_hWnd, pair.first));
                            }
                            for (const auto& pair : m_toolbars) {
                                targetParents.emplace(std::make_pair(pair.second->m_hWnd, DOCK_BOTTOM));
                            }
                            while (hWnd && hWnd != m_pMainFrame->m_hWnd) {
                                auto it = targetParents.find(hWnd);
                                if (it != targetParents.end()) {
                                    if (ePolicy == CAppSettings::HideFullscreenControlsPolicy::SHOW_WHEN_CURSOR_MOVED) {
                                        mask.show(maskAll);
                                    } else {
                                        ASSERT(ePolicy == CAppSettings::HideFullscreenControlsPolicy::SHOW_WHEN_HOVERED);
                                        mask.show(it->second);
                                    }
                                }
                                hWnd = GetParent(hWnd);
                            }
                        }
                        ret = true;
                    }
                }
                return ret;
            };
            auto testHovering = [&]() {
                bool ret = false;
                if (bOnWindow) {
                    unsigned uTop, uLeft, uRight, uBottom;
                    GetDockZones(uTop, uLeft, uRight, uBottom, bEnumedPanelZones ? false : (bEnumedPanelZones = true));
                    CRect clientRect;
                    m_pMainFrame->GetClientRect(clientRect);
                    CRect exclSeekbarRect;
                    if (bExclSeekbar) {
                        if (!m_pMainFrame->m_pMVRI
                                || FAILED(m_pMainFrame->m_pMVRI->GetRect("seekbarRect", exclSeekbarRect))) {
                            exclSeekbarRect = clientRect;
                            exclSeekbarRect.top = 56;
                        }
                        uBottom = 0;
                    }
                    if (!bCanHideDockedPanels) {
                        uTop = uLeft = uRight = 0;
                    }
                    const bool bHoveringExclSeekbar = (bExclSeekbar && exclSeekbarRect.PtInRect(clientPoint));
                    ret = true;
                    if (clientRect.PtInRect(clientPoint)) {
                        if (ePolicy == CAppSettings::HideFullscreenControlsPolicy::SHOW_WHEN_CURSOR_MOVED) {
                            if (!bHoveringExclSeekbar) {
                                CRect nondockRect(clientRect);
                                nondockRect.DeflateRect(uLeft, uTop, uRight, uBottom);
                                if (!nondockRect.PtInRect(clientPoint)) {
                                    // hovering any - show all
                                    mask.show(maskAll);
                                } else {
                                    ret = false;
                                }
                            }
                        } else {
                            ASSERT(ePolicy == CAppSettings::HideFullscreenControlsPolicy::SHOW_WHEN_HOVERED);
                            // show hovered
                            auto hoveringZone = [&](unsigned uTargetTop, unsigned uTargetLeft, unsigned uTargetRight, unsigned uTargetBottom) {
                                DockZone ret = DOCK_NONE;
                                if (clientPoint.y <= (int)uTargetTop) {
                                    ret = DOCK_TOP;
                                } else if (clientPoint.y + (int)uTargetBottom >= clientRect.Height()) {
                                    ret = DOCK_BOTTOM;
                                } else if (clientPoint.x <= (int)uTargetLeft) {
                                    ret = DOCK_LEFT;
                                } else if (clientPoint.x + (int)uTargetRight >= clientRect.Width()) {
                                    ret = DOCK_RIGHT;
                                }
                                return ret;
                            };
                            unsigned uVisibleTop, uVisibleLeft, uVisibleRight, uVisibleBottom;
                            GetVisibleDockZones(uVisibleTop, uVisibleLeft, uVisibleRight, uVisibleBottom, bEnumedPanelZones ? false : (bEnumedPanelZones = true));
                            DockZone hoverZone = hoveringZone(uVisibleTop, uVisibleLeft, uVisibleRight, uVisibleBottom);
                            if (hoverZone == DOCK_NONE) {
                                if (!bHoveringExclSeekbar) {
                                    hoverZone = hoveringZone(uTop, uLeft, uRight, uBottom);
                                    if (hoverZone == DOCK_NONE) {
                                        ret = false;
                                    } else {
                                        mask.show(hoverZone);
                                    }
                                }
                            } else {
                                mask.show(hoverZone);
                            }
                        }
                    }
                }
                return ret;
            };
            if (!testCapture() && !testHovering() &&
                    ePolicy == CAppSettings::HideFullscreenControlsPolicy::SHOW_WHEN_CURSOR_MOVED &&
                    bOnWindow && !CMouse::PointEqualsImprecise(screenPoint, st.lastShowPoint) &&
                    !m_pMainFrame->m_wndView.Dragging() && uTimeout > 0) {
                // show when corresponding hide policy is used, not hovering, non-zero timeout, the cursor moved, and not dragging
                mask.show(maskAll);
            }
        }
    } else {
        // always show otherwise
        mask.show(maskAll);
    }

    // we don't care for zone locks if we can't autohide at this point of time
    if (!bCanAutoHide) {
        m_zoneHideLocks.clear();
    }

    // always hide bottom when madVR exclusive seekbar is enabled,
    // but respect zone locks
    if (bExclSeekbar && m_zoneHideLocks.find(DOCK_BOTTOM) == m_zoneHideLocks.end()) {
        mask.hide(DOCK_BOTTOM);
    }

    if ((mask.maskHide || mask.maskShow) && !bEnumedPanelZones) {
        EnumPanelZones();
        bEnumedPanelZones = true;
    }

    const bool bNoTimer = m_zoneHideTicks.empty();
    const ULONGLONG dwTick = GetTickCount64();

    for (const auto& pair : m_zoneHideTicks) {
        if (pair.second <= dwTick && !(mask.maskShow & pair.first)) {
            mask.hide(pair.first);
        }
    }

    {
        auto delayedAutohideZone = [&](DockZone zone) {
            ASSERT(zone != DOCK_NONE);
            if (!(zone & mask.maskHide) && !(zone & mask.maskShow)) {
                if ((zone == DOCK_BOTTOM || (bCanHideDockedPanels && m_panelZones.find(zone) != m_panelZones.end())) &&
                        m_zoneHideTicks.find(zone) == m_zoneHideTicks.end() &&
                        m_zoneHideLocks.find(zone) == m_zoneHideLocks.end()) {
                    ASSERT(uTimeout > 0);
                    m_zoneHideTicks[zone] = dwTick + uTimeout;
                }
            }
        };
        if (!st.bLastCanAutoHidePanels && bCanAutoHide && bCanHideDockedPanels) {
            delayedAutohideZone(DOCK_BOTTOM);
            delayedAutohideZone(DOCK_TOP);
            delayedAutohideZone(DOCK_LEFT);
            delayedAutohideZone(DOCK_RIGHT);
        } else if (!st.bLastCanAutoHideToolbars && bCanAutoHide) {
            delayedAutohideZone(DOCK_BOTTOM);
        }
        if (!bCanAutoHide && (st.bLastCanAutoHideToolbars || st.bLastCanAutoHidePanels)) {
            m_zoneHideTicks.clear();
        }
    }

    if (!bCanHideDockedPanels) {
        for (const auto& pair : m_panels) {
            if (pair.second->IsAutohidden()) {
                bRecalcLayout = true;
                m_pMainFrame->ShowControlBar(pair.second, TRUE, TRUE);
                pair.second->SetAutohidden(false);
            }
        }
    }

    {
        auto showZone = [&](DockZone zone) {
            ASSERT(zone != DOCK_NONE);
            if (zone & mask.maskShow) {
                bool bSetTick = false;
                if (zone == DOCK_BOTTOM) {
                    if (ShowToolbarsSelection()) {
                        bRecalcLayout = true;
                    }
                    bSetTick = true;
                }
                auto it = m_panelZones.find(zone);
                if (it != m_panelZones.end()) {
                    const auto& panels = it->second;
                    for (const auto panel : panels) {
                        if (m_panels[panel]->IsAutohidden()) {
                            bRecalcLayout = true;
                            m_pMainFrame->ShowControlBar(m_panels[panel], TRUE, TRUE);
                        }
                        if (bCanHideDockedPanels) {
                            bSetTick = true;
                        }
                    }
                }
                if (bSetTick && bCanAutoHide) {
                    if (m_zoneHideLocks.find(zone) == m_zoneHideLocks.end()) {
                        // hide zone after timeout
                        m_zoneHideTicks[zone] = dwTick + uTimeout;
                    } else {
                        // delay hiding locked zone for at least current timeout
                        ASSERT(m_zoneHideTicks.find(zone) != m_zoneHideTicks.end());
                        ULONGLONG dwNewHideTick = dwTick + uTimeout;
                        ULONGLONG& dwHideTick = m_zoneHideTicks[zone];
                        if (dwNewHideTick > dwHideTick) {
                            m_zoneHideLocks.erase(zone);
                            dwHideTick = dwNewHideTick;
                        }
                    }
                } else {
                    m_zoneHideTicks.erase(zone);
                }
            }
        };
        showZone(DOCK_BOTTOM);
        showZone(DOCK_TOP);
        showZone(DOCK_LEFT);
        showZone(DOCK_RIGHT);
    }

    if (mask.maskShow) {
        st.lastShowPoint = screenPoint;
    }

    {
        auto checkForPopup = [&](DockZone zone) {
            ASSERT(zone != DOCK_NONE);
            if (zone & mask.maskHide) {
                auto it = m_panelZones.find(zone);
                if (it != m_panelZones.end()) {
                    const auto& panels = it->second;
                    for (const auto panel : panels) {
                        if (m_panels[panel]->HasActivePopup()) {
                            mask.maskHide &= ~zone;
                            break;
                        }
                    }
                }
            }
        };
        checkForPopup(DOCK_BOTTOM);
        checkForPopup(DOCK_TOP);
        checkForPopup(DOCK_LEFT);
        checkForPopup(DOCK_RIGHT);
    }

    {
        auto autohideZone = [&](DockZone zone) {
            ASSERT(zone != DOCK_NONE);
            if (zone & mask.maskHide) {
                auto it = m_panelZones.find(zone);
                if (it != m_panelZones.end() && bCanHideDockedPanels) {
                    const auto panels = it->second; // copy
                    for (const auto panel : panels) {
                        auto pBar = m_panels[panel];
                        if (!pBar->IsAutohidden() && !ToolbarInputActive(pBar) && !pBar->HasActivePopup()) {
                            bRecalcLayout = true;
                            m_pMainFrame->ShowControlBar(pBar, FALSE, TRUE);
                            m_pMainFrame->RestoreFocus();
                            pBar->SetAutohidden(true);
                        }
                    }
                }
                if (zone == DOCK_BOTTOM) {
                    if (ShowToolbars(CS_NONE)) {
                        bRecalcLayout = true;
                    }
                }
                m_zoneHideTicks.erase(zone);
                m_zoneHideLocks.erase(zone);
            }
        };
        autohideZone(DOCK_BOTTOM);
        autohideZone(DOCK_TOP);
        autohideZone(DOCK_LEFT);
        autohideZone(DOCK_RIGHT);
    }

    const bool bNeedTimer = !m_zoneHideTicks.empty();
    if (bNoTimer && bNeedTimer) {
        m_pMainFrame->m_timerHider.Subscribe(CMainFrame::TimerHiderSubscriber::TOOLBARS_HIDER,
            std::bind(&CMainFrameControls::UpdateToolbarsVisibility, this));
    } else if (!bNoTimer && !bNeedTimer) {
        m_pMainFrame->m_timerHider.Unsubscribe(CMainFrame::TimerHiderSubscriber::TOOLBARS_HIDER);
    }

    if (bRecalcLayout) {
        m_pMainFrame->RecalcLayout();
    }

    st.bLastCanAutoHideToolbars = bCanAutoHide;
    st.bLastCanAutoHidePanels = bCanAutoHide && bCanHideDockedPanels;
    st.bLastHaveExclusiveSeekbar = bExclSeekbar;
}

unsigned CMainFrameControls::GetVisibleToolbarsHeight() const
{
    return GetToolbarsHeight(m_controlsVisibilityState.nVisibleCS);
}

unsigned CMainFrameControls::GetToolbarsHeight() const
{
    return GetToolbarsHeight(m_controlsVisibilityState.nCurrentCS);
}

void CMainFrameControls::DelayShowNotLoaded(bool bDoDelay)
{
    auto id = CMainFrame::TimerOneTimeSubscriber::TOOLBARS_DELAY_NOTLOADED;
    if (bDoDelay) {
        m_pMainFrame->m_timerOneTime.Subscribe(id, std::bind(&CMainFrameControls::DelayShowNotLoadedCallback, this),
                                               DELAY_SHOW_NOT_LOADED_TIME_MS);
    } else if (m_bDelayShowNotLoaded) {
        m_pMainFrame->m_timerOneTime.Unsubscribe(id);
    }
    m_bDelayShowNotLoaded = bDoDelay;
}

bool CMainFrameControls::DelayShowNotLoaded() const
{
    return m_bDelayShowNotLoaded;
}

CMainFrameControls::DockZone CMainFrameControls::GetPanelZone(CPlayerBar* pBar)
{
    DockZone ret = DOCK_NONE;
    if (IsWindow(pBar->m_hWnd)) {
        switch (pBar->GetParent()->GetDlgCtrlID()) {
            case AFX_IDW_DOCKBAR_TOP:
                ret = DOCK_TOP;
                break;
            case AFX_IDW_DOCKBAR_BOTTOM:
                ret = DOCK_BOTTOM;
                break;
            case AFX_IDW_DOCKBAR_LEFT:
                ret = DOCK_LEFT;
                break;
            case AFX_IDW_DOCKBAR_RIGHT:
                ret = DOCK_RIGHT;
                break;
        }
    } else {
        ASSERT(FALSE);
    }
    return ret;
}

CMainFrameControls::DockZone CMainFrameControls::GetPanelZone(Panel panel)
{
    return GetPanelZone(m_panels[panel]);
}

void CMainFrameControls::EnumPanelZones()
{
    m_panelZones.clear();
    m_panelDocks.clear();

    for (const auto& pair : m_panels) {
        auto& pBar = pair.second;
        if (IsWindow(pBar->m_hWnd) && (pBar->IsAutohidden() || pBar->IsWindowVisible())) {
            DockZone zone = GetPanelZone(pBar);
            if (zone != DOCK_NONE) {
                m_panelZones[zone].push_back(pair.first);
                m_panelDocks[zone] = pBar->m_pDockBar;
            }
        }
    }

    m_controlsVisibilityState.nCurrentCS = GetEffectiveToolbarsSelection();
}

void CMainFrameControls::GetDockZonesInternal(unsigned& uTop, unsigned& uLeft, unsigned& uRight, unsigned& uBottom, bool bDoEnum, bool bOnlyVisible)
{
    if (bDoEnum) {
        EnumPanelZones();
    }

    auto calcDock = [&](DockZone zone) {
        bool bHorz = (zone == DOCK_TOP || zone == DOCK_BOTTOM);
        const auto& panels = m_panelZones.find(zone)->second;
        auto pDock = m_panelDocks.find(zone)->second;
        unsigned stackSize = 0;
        bool bNewRow = true;
        for (int i = 0; i < pDock->m_arrBars.GetCount(); i++) {
            auto pBar = static_cast<CPlayerBar*>(pDock->m_arrBars[i]);
            if (!pBar && !bNewRow) {
                bNewRow = true;
            }
            if (bNewRow && pBar) {
                for (const auto panel : panels) {
                    if (m_panels[panel] == pBar && (!bOnlyVisible || IsWindowVisible(*pBar))) {
                        CSize size = pBar->CalcFixedLayout(TRUE, bHorz);
                        stackSize += (bHorz ? size.cy - 2 : (stackSize ? size.cx - 2 : size.cx - 4));
                        bNewRow = false;
                        break;
                    }
                }
            }
        }
        return stackSize;
    };

    uTop = (m_panelZones.find(DOCK_TOP) != std::end(m_panelZones)) ? calcDock(DOCK_TOP) : 0;
    uLeft = (m_panelZones.find(DOCK_LEFT) != std::end(m_panelZones)) ? calcDock(DOCK_LEFT) : 0;
    uRight = (m_panelZones.find(DOCK_RIGHT) != std::end(m_panelZones)) ? calcDock(DOCK_RIGHT) : 0;
    uBottom = (m_panelZones.find(DOCK_BOTTOM) != std::end(m_panelZones)) ? calcDock(DOCK_BOTTOM) : 0;

    unsigned uToolbars = bOnlyVisible ? GetVisibleToolbarsHeight() : GetToolbarsHeight();
    uBottom += uToolbars;
}

void CMainFrameControls::LockHideZone(DockZone zone)
{
    ASSERT(zone != DOCK_NONE);
    m_zoneHideLocks.insert(zone);
    auto it = m_zoneHideTicks.find(zone);
    const ULONGLONG dwNewHideTick = GetTickCount64() + 1000ULL;
    if (it != m_zoneHideTicks.end()) {
        ULONGLONG& dwHideTick = it->second;
        if (dwHideTick < dwNewHideTick) {
            dwHideTick = dwNewHideTick;
        }
    } else {
        m_zoneHideTicks[zone] = dwNewHideTick;
    }
    m_pMainFrame->m_timerHider.Subscribe(CMainFrame::TimerHiderSubscriber::TOOLBARS_HIDER,
                                        std::bind(&CMainFrameControls::UpdateToolbarsVisibility, this));
}
