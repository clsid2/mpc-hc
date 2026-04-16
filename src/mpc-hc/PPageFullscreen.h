/*
 * (C) 2003-2006 Gabest
 * (C) 2006-2014, 2016 see Authors.txt
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

#pragma once

#include "CMPCThemePPageBase.h"
#include "PlayerListCtrl.h"
#include "CMPCThemeComboBox.h"
#include "CMPCThemeSpinButtonCtrl.h"

// CPPageFullscreen dialog

class CPPageFullscreen : public CMPCThemePPageBase
{
    DECLARE_DYNAMIC(CPPageFullscreen)

private:
    std::vector<CString> m_monitorDisplayNames;
    std::vector<CString> m_monitorDeviceNames;
    CStringW m_fullScreenMonitorID;
    CStringW m_fullScreenMonitorDeviceName;
    int m_iFullScreenMonitor;
    CMPCThemeComboBox m_fullScreenMonitorCtrl;

    BOOL m_bLaunchFullscreen;
    BOOL m_fExitFullScreenAtTheEnd;

    BOOL m_bHideFullscreenControls;
    CMPCThemeComboBox m_hidePolicy;
    unsigned m_uHideFullscreenControlsDelay;
    BOOL m_bHideFullscreenDockedPanels;

    std::vector<DisplayMode> m_displayModes;
    CAtlList<CString> m_displayModesString;
    size_t m_nCurrentDisplayModeIndex;
    CString m_CurrentDisplayModeString;
    CStringW m_FullScreenSeparateControlsText;

    std::vector<AutoChangeMode> m_autoChangeFSModes;
    BOOL m_bAutoChangeFSModeEnabled;
    BOOL m_bAutoChangeFSModeApplyDefModeAtFSExist;
    BOOL m_bAutoChangeFSModeRestoreResAfterProgExit;
    BOOL m_bFullScreenSeparateControls;
    unsigned m_uAutoChangeFullscrResDelay;

    CPlayerListCtrl m_list;
    enum {
        COL_N,
        COL_FRAMERATE_START,
        COL_FRAMERATE_STOP,
        COL_DISPLAY_MODE,
        COL_AUDIO_DELAY
    };

    CMPCThemeSpinButtonCtrl m_delaySpinner;

    void ModesUpdate();

public:
    CPPageFullscreen();
    virtual ~CPPageFullscreen();

    // Dialog Data
    enum { IDD = IDD_PPAGEFULLSCREEN };

protected:
    virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
    virtual BOOL OnInitDialog();
    virtual BOOL OnApply();
    inline void RenumberListItem(int nItem);
    void RenumberListItems(int nStartItem);

    DECLARE_MESSAGE_MAP()

    afx_msg void OnUpdateFullScreenMonitor();

    afx_msg void OnUpdateHideControls(CCmdUI* pCmdUI);
    afx_msg void OnHideControlsPolicyChange();
    afx_msg void OnUpdateHideDelay(CCmdUI* pCmdUI);

    afx_msg void OnUpdateAutoChangeFullscreenMode(CCmdUI* pCmdUI);
    afx_msg void OnListCheckChange();
    afx_msg void OnAdd();
    afx_msg void OnRemove();
    afx_msg void OnUpdateRemove(CCmdUI* pCmdUI);
    afx_msg void OnMoveUp();
    afx_msg void OnUpdateUp(CCmdUI* pCmdUI);
    afx_msg void OnMoveDown();
    afx_msg void OnUpdateDown(CCmdUI* pCmdUI);
    afx_msg void OnListBeginEdit(NMHDR* pNMHDR, LRESULT* pResult);
    afx_msg void OnListDoEdit(NMHDR* pNMHDR, LRESULT* pResult);
    afx_msg void OnListEndEdit(NMHDR* pNMHDR, LRESULT* pResult);
    afx_msg void OnListCustomDraw(NMHDR* pNMHDR, LRESULT* pResult);
    afx_msg BOOL OnToolTipNotify(UINT id, NMHDR* pNMH, LRESULT* pResult);
};
