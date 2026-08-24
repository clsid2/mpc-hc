/*
 * (C) 2010-2016 see Authors.txt
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
#include "PlayerNavigationDialog.h"
#include "PlayerBar.h"
#include "DSUtil.h"
#include "mplayerc.h"
#include "MainFrm.h"

// CPlayerNavigationDialog dialog

// IMPLEMENT_DYNAMIC(CPlayerNavigationDialog, CMPCThemeResizableDialog)
CPlayerNavigationDialog::CPlayerNavigationDialog(CMainFrame* pMainFrame)
    : CMPCThemeResizableDialog(CPlayerNavigationDialog::IDD, nullptr)
    , m_pMainFrame(pMainFrame)
    , m_bChannelInfoAvailable(false)
    , m_bTVStations(true)
{
}

BOOL CPlayerNavigationDialog::Create(CWnd* pParent)
{
    if (!__super::Create(IDD, pParent)) {
        return FALSE;
    }

    AddAnchor(IDC_LISTCHANNELS, TOP_LEFT, BOTTOM_RIGHT);
    AddAnchor(IDC_NAVIGATION_INFO, BOTTOM_LEFT);
    AddAnchor(IDC_NAVIGATION_SCAN, BOTTOM_RIGHT);
    AddAnchor(IDC_NAVIGATION_FILTERSTATIONS, BOTTOM_LEFT, BOTTOM_RIGHT);

    return TRUE;
}

void CPlayerNavigationDialog::DoDataExchange(CDataExchange* pDX)
{
    __super::DoDataExchange(pDX);
    DDX_Control(pDX, IDC_LISTCHANNELS, m_channelList);
    DDX_Control(pDX, IDC_NAVIGATION_INFO, m_buttonInfo);
    DDX_Control(pDX, IDC_NAVIGATION_FILTERSTATIONS, m_buttonFilterStations);
    fulfillThemeReqs();
}

BOOL CPlayerNavigationDialog::PreTranslateMessage(MSG* pMsg)
{
    if (pMsg->message == WM_KEYDOWN) {
        if (pMsg->wParam == VK_RETURN) {
            CWnd* pFocused = GetFocus();
            if (pFocused && pFocused->m_hWnd == m_channelList.m_hWnd) {
                return TRUE;
            }
        }
    }
    return __super::PreTranslateMessage(pMsg);
}

BEGIN_MESSAGE_MAP(CPlayerNavigationDialog, CMPCThemeResizableDialog)
    ON_WM_DESTROY()
    ON_WM_CONTEXTMENU()
    ON_LBN_SELCHANGE(IDC_LISTCHANNELS, OnChangeChannel)
    ON_BN_CLICKED(IDC_NAVIGATION_INFO, OnShowChannelInfo)
    ON_UPDATE_COMMAND_UI(IDC_NAVIGATION_INFO, OnUpdateShowChannelInfoButton)
    ON_BN_CLICKED(IDC_NAVIGATION_SCAN, OnTunerScan)
    ON_BN_CLICKED(IDC_NAVIGATION_FILTERSTATIONS, OnTvRadioStations)
    ON_WM_SIZE()
END_MESSAGE_MAP()


// CPlayerNavigationDialog message handlers

BOOL CPlayerNavigationDialog::OnInitDialog()
{
    __super::OnInitDialog();

    if (m_bTVStations) {
        m_buttonFilterStations.SetWindowText(ResStr(IDS_DVB_TVNAV_SEERADIO));
    } else {
        m_buttonFilterStations.SetWindowText(ResStr(IDS_DVB_TVNAV_SEETV));
    }

    ATOM atom = ::GlobalAddAtom(MICROSOFT_TABLETPENSERVICE_PROPERTY);
    ::SetProp(m_channelList.GetSafeHwnd(), MICROSOFT_TABLETPENSERVICE_PROPERTY, nullptr);
    ::GlobalDeleteAtom(atom);

    return FALSE;  // return FALSE so that the dialog does not steal focus
    // EXCEPTION: OCX Property Pages should return FALSE
}

void CPlayerNavigationDialog::OnDestroy()
{
    m_channelList.ResetContent();
    __super::OnDestroy();
}

void CPlayerNavigationDialog::OnChangeChannel()
{
    int nItem = m_channelList.GetCurSel();
    if (nItem != LB_ERR) {
        UINT nChannelID = (UINT)m_channelList.GetItemData(nItem) + ID_NAVIGATE_JUMPTO_SUBITEM_START;
        m_pMainFrame->OnNavigateJumpTo(nChannelID);
    }
}

void CPlayerNavigationDialog::UpdateElementList()
{
    if (m_pMainFrame->GetPlaybackMode() == PM_DIGITAL_CAPTURE) {
        const auto& s = AfxGetAppSettings();
        m_channelList.ResetContent();

        for (const auto& channel : s.m_DVBChannels) {
            if (m_bTVStations && channel.GetVideoPID() || !m_bTVStations && !channel.GetVideoPID()) {
                int nItem = m_channelList.AddString(channel.GetName());
                if (nItem != LB_ERR) {
                    m_channelList.SetItemData(nItem, (DWORD_PTR)channel.GetPrefNumber());
                    if (s.nDVBLastChannel == channel.GetPrefNumber()) {
                        m_channelList.SetCurSel(nItem);
                    }
                }
            }
        }
    }
}

void CPlayerNavigationDialog::UpdatePos(int nID)
{
    for (int i = 0, count = m_channelList.GetCount(); i < count; i++) {
        if ((int)m_channelList.GetItemData(i) == nID) {
            m_channelList.SetCurSel(i);
            break;
        }
    }
}

void CPlayerNavigationDialog::SetChannelInfoAvailable(bool bAvailable)
{
    m_bChannelInfoAvailable = bAvailable;
}

void CPlayerNavigationDialog::OnTunerScan()
{
    m_pMainFrame->OnTunerScan();
    UpdateElementList();
}

void CPlayerNavigationDialog::OnShowChannelInfo()
{
    m_pMainFrame->UpdateCurrentChannelInfo(true, true);
}

void CPlayerNavigationDialog::OnUpdateShowChannelInfoButton(CCmdUI* pCmdUI)
{
    pCmdUI->Enable(m_bChannelInfoAvailable);
}

void CPlayerNavigationDialog::OnTvRadioStations()
{
    m_bTVStations = !m_bTVStations;
    UpdateElementList();

    if (m_bTVStations) {
        m_buttonFilterStations.SetWindowText(ResStr(IDS_DVB_TVNAV_SEERADIO));
    } else {
        m_buttonFilterStations.SetWindowText(ResStr(IDS_DVB_TVNAV_SEETV));
    }
}

void CPlayerNavigationDialog::OnContextMenu(CWnd* pWnd, CPoint point)
{
    auto& s = AfxGetAppSettings();
    BOOL bOutside;
    int nItem;
    const int curSel = m_channelList.GetCurSel();
    const int channelCount = m_channelList.GetCount();

    if (point.x == -1 && point.y == -1) {
        CRect r;
        if (m_channelList.GetItemRect(curSel, r) != LB_ERR) {
            point.SetPoint(r.left, r.bottom);
        } else {
            point.SetPoint(0, 0);
        }
        m_channelList.ClientToScreen(&point);
        nItem = curSel;
        bOutside = nItem == LB_ERR;
    } else {
        CPoint clientPoint = point;
        m_channelList.ScreenToClient(&clientPoint);
        nItem = (int)m_channelList.ItemFromPoint(clientPoint, bOutside);
    }

    CMPCThemeMenu m;
    m.CreatePopupMenu();

    enum {
        M_WATCH = 1,
        M_MOVE_UP,
        M_MOVE_DOWN,
        M_SORT,
        M_REMOVE,
        M_REMOVE_ALL
    };

    auto findChannelByItemNumber = [this](std::vector<CBDAChannel>& c, int nItem) {
        int nPrefNumber = (int)m_channelList.GetItemData(nItem);
        return find_if(c.begin(), c.end(), [&](CBDAChannel const & channel) {
            return channel.GetPrefNumber() == nPrefNumber;
        });
    };

    auto swapChannels = [&](int n1, int n2) {
        auto it1 = findChannelByItemNumber(s.m_DVBChannels, n1);
        auto it2 = findChannelByItemNumber(s.m_DVBChannels, n2);
        int nPrefNumber1 = it1->GetPrefNumber(), nPrefNumber2 = it2->GetPrefNumber();
        // Make sure the current channel number is updated if swapping the channel is going to change it
        if (nPrefNumber1 == s.nDVBLastChannel) {
            s.nDVBLastChannel = nPrefNumber2;
        } else if (nPrefNumber2 == s.nDVBLastChannel) {
            s.nDVBLastChannel = nPrefNumber1;
        }
        // The preferred number shouldn't be swapped so we swap it twice for no-op
        it1->SetPrefNumber(nPrefNumber2);
        it2->SetPrefNumber(nPrefNumber1);
        // Actually swap the channels
        std::iter_swap(it1, it2);
    };

    if (!bOutside) {
        m_channelList.SetCurSel(nItem);

        m.AppendMenu(MF_STRING | (curSel != nItem ? MF_ENABLED : (MF_DISABLED | MF_GRAYED)), M_WATCH, ResStr(IDS_NAVIGATION_WATCH));
        m.AppendMenu(MF_SEPARATOR);
        m.AppendMenu(MF_STRING | (nItem == 0 ? (MF_DISABLED | MF_GRAYED) : MF_ENABLED), M_MOVE_UP, ResStr(IDS_NAVIGATION_MOVE_UP));
        m.AppendMenu(MF_STRING | (nItem == channelCount - 1 ? (MF_DISABLED | MF_GRAYED) : MF_ENABLED), M_MOVE_DOWN, ResStr(IDS_NAVIGATION_MOVE_DOWN));
    }
    m.AppendMenu(MF_STRING | (channelCount > 1 ? MF_ENABLED : (MF_DISABLED | MF_GRAYED)), M_SORT, ResStr(IDS_NAVIGATION_SORT));
    m.AppendMenu(MF_SEPARATOR);
    if (!bOutside) {
        m.AppendMenu(MF_STRING | MF_ENABLED, M_REMOVE, ResStr(IDS_PLAYLIST_REMOVE));
    }
    m.AppendMenu(MF_STRING | (channelCount > 0 ? MF_ENABLED : (MF_DISABLED | MF_GRAYED)), M_REMOVE_ALL, ResStr(IDS_NAVIGATION_REMOVE_ALL));

    if (AppNeedsThemedControls()) {
        m.fulfillThemeReqs();
    }
    //this dialog is the menu owner, which bypasses CPlayerBar::OnEnterMenuLoop,
    //so we set the flag on the owning bar directly to keep it from autohiding
    CPlayerBar* pBar = DYNAMIC_DOWNCAST(CPlayerBar, GetParent());
    if (pBar) {
        pBar->SetHasActivePopup(true);
    }
    int nID = (int)m.TrackPopupMenu(TPM_LEFTBUTTON | TPM_RETURNCMD, point.x, point.y, this);
    if (pBar) {
        pBar->SetHasActivePopup(false);
    }

    try {
        switch (nID) {
            case M_WATCH:
                OnChangeChannel();
                break;
            case M_MOVE_UP:
                swapChannels(nItem, nItem - 1);
                UpdateElementList();
                break;
            case M_MOVE_DOWN:
                swapChannels(nItem, nItem + 1);
                UpdateElementList();
                break;
            case M_SORT: {
                sort(s.m_DVBChannels.begin(), s.m_DVBChannels.end());
                // Update the preferred numbers
                int nPrefNumber = 0;
                int nDVBLastChannel = s.nDVBLastChannel;
                for (auto& channel : s.m_DVBChannels) {
                    // Make sure the current channel number will be updated
                    if (channel.GetPrefNumber() == s.nDVBLastChannel) {
                        nDVBLastChannel = nPrefNumber;
                    }
                    channel.SetPrefNumber(nPrefNumber++);
                }
                s.nDVBLastChannel = nDVBLastChannel;
                UpdateElementList();
                break;
            }
            case M_REMOVE: {
                const auto it = findChannelByItemNumber(s.m_DVBChannels, nItem);
                const int nRemovedPrefNumber = it->GetPrefNumber();
                s.m_DVBChannels.erase(it);
                // Update channels pref number
                for (CBDAChannel& channel : s.m_DVBChannels) {
                    const int nPrefNumber = channel.GetPrefNumber();
                    ASSERT(nPrefNumber != nRemovedPrefNumber);
                    if (nPrefNumber > nRemovedPrefNumber) {
                        channel.SetPrefNumber(nPrefNumber - 1);
                    }
                }
                UpdateElementList();

                int newCurSel = curSel;
                if ((newCurSel >= nItem) && (channelCount - 1 == nItem || newCurSel > nItem)) {
                    --newCurSel;
                }

                const auto newChannelIt = findChannelByItemNumber(s.m_DVBChannels, newCurSel);
                if (newChannelIt != s.m_DVBChannels.end()) {
                    // Update pref number of the current channel
                    s.nDVBLastChannel = newChannelIt->GetPrefNumber();
                    m_channelList.SetCurSel(newCurSel);
                    if (curSel == nItem) {
                        // Set closest channel on list after removing current channel
                        m_pMainFrame->SetChannel(s.nDVBLastChannel);
                    }
                } else { // The last channel was removed
                    s.nDVBLastChannel = INT_ERROR;
                }
            }
            break;
            case M_REMOVE_ALL:
                if (IDYES == AfxMessageBox(IDS_REMOVE_CHANNELS_QUESTION, MB_ICONQUESTION | MB_YESNO, 0)) {
                    s.m_DVBChannels.clear();
                    s.nDVBLastChannel = INT_ERROR;
                    UpdateElementList();
                }
                break;
            default:
                m_channelList.SetCurSel(curSel);
                break;
        }
    } catch (std::exception& e) {
        UNREFERENCED_PARAMETER(e);
        TRACE(_T("Navigation Dialog requested operation failed: \"%s\""), e.what());
        UpdateElementList();
        ASSERT(FALSE);
    }
}

void CPlayerNavigationDialog::OnSize(UINT nType, int cx, int cy) {
    CMPCThemeResizableDialog::OnSize(nType, cx, cy);
    if (m_channelList.m_hWnd) {
        int nItem = m_channelList.GetCurSel();
        m_channelList.EnsureVisible(nItem);
    }
}
