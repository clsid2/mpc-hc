/*
 * (C) 2015, 2017 see Authors.txt
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
#include "CMPCThemeSliderCtrl.h"
#include "CMPCThemeComboBox.h"


class CPPageAudioRenderer : public CMPCThemePPageBase
{
    DECLARE_DYNAMIC(CPPageAudioRenderer)
public:

    CPPageAudioRenderer();

    enum { IDD = IDD_PPAGEAUDIORENDERER };

protected:

    void DoDataExchange(CDataExchange* pDX) override;
    BOOL OnInitDialog() override;
    BOOL OnApply() override;

    void OnHScroll(UINT nSBCode, UINT nPos, CScrollBar* pScrollBar);

    void OnCMoyButton();
    void OnJMeierButton();

    void OnUpdateCrossfeedGroup(CCmdUI* pCmdUI);
    void OnUpdateCrossfeedCutoffLabel(CCmdUI* pCmdUI);
    void OnUpdateCrossfeedLevelLabel(CCmdUI* pCmdUI);

    DECLARE_MESSAGE_MAP()

    std::vector<CString> m_deviceIds;

    BOOL m_bExclusiveMode;
    BOOL m_bCrossfeedEnabled;
    BOOL m_bIgnoreSystemChannelMixer;

    CMPCThemeComboBox m_combo1;
    CMPCThemeSliderCtrl m_slider1;
    CMPCThemeSliderCtrl m_slider2;
};
