/*
 * (C) 2003-2006 Gabest
 * (C) 2006-2013 see Authors.txt
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
#include "FloatEdit.h"
#include "../filters/switcher/AudioSwitcher/AudioSwitcher.h"
#include "CMPCThemeButton.h"
#include "CMPCThemeSliderCtrl.h"
#include "CMPCThemeEdit.h"
#include "CMPCThemeSpinButtonCtrl.h"
#include "CMPCThemePlayerListCtrl.h"
#include "CMPCThemeComboBox.h"




// CPPageAudioSwitcher dialog

class CPPageAudioSwitcher : public CMPCThemePPageBase
{
    DECLARE_DYNAMIC(CPPageAudioSwitcher)

private:
    DWORD m_pSpeakerToChannelMap[AS_MAX_CHANNELS][AS_MAX_CHANNELS];
    DWORD m_dwChannelMask;

    BOOL m_fEnableAudioSwitcher;
    BOOL m_fAudioNormalize;
    UINT m_nAudioMaxNormFactor;
    CMPCThemeSpinButtonCtrl m_AudioMaxNormFactorSpin;
    BOOL m_fAudioNormalizeRecover;
    int m_AudioBoostPos;
    CMPCThemeSliderCtrl m_AudioBoostCtrl;
    BOOL m_fCustomChannelMapping;
    CMPCThemeRadioOrCheck m_fCustomChannelMappingCtrl;
    CMPCThemeEdit m_nChannelsCtrl;
    int m_nChannels;
    CMPCThemeSpinButtonCtrl m_nChannelsSpinCtrl;
    CMPCThemePlayerListCtrl m_list;
    int m_tAudioTimeShift;
    CMPCThemeRadioOrCheck m_fAudioTimeShiftCtrl;
    CMPCThemeIntEdit m_tAudioTimeShiftCtrl;
    CMPCThemeSpinButtonCtrl m_tAudioTimeShiftSpin;
    BOOL m_fAudioTimeShift;
    int m_iReplayGainMode;
    CMPCThemeComboBox m_replayGainModeCtrl;
    int m_iReplayGainPreamp;
    CMPCThemeIntEdit m_replayGainPreampCtrl;
    CMPCThemeSpinButtonCtrl m_replayGainPreampSpin;
    BOOL m_bReplayGainPreventClipping;

    // tooltip for slidercontrol
    CToolTipCtrl m_tooltip;
public:
    CPPageAudioSwitcher(IFilterGraph* pFG);
    virtual ~CPPageAudioSwitcher();

    // Dialog Data
    enum { IDD = IDD_PPAGEAUDIOSWITCHER };

protected:
    virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
    virtual BOOL OnInitDialog();
    virtual BOOL OnApply();

    void SetChannelMappingSW(int v);

    DECLARE_MESSAGE_MAP()

    afx_msg void OnClickCheck1();
    afx_msg void OnNMClickList1(NMHDR* pNMHDR, LRESULT* pResult);
    afx_msg void OnDrawItem(int nIDCtl, LPDRAWITEMSTRUCT lpDrawItemStruct);
    afx_msg void OnEnChangeEdit1();
    afx_msg void OnUpdateAudioSwitcher(CCmdUI* pCmdUI);
    afx_msg void OnUpdateNormalize(CCmdUI* pCmdUI);
    afx_msg void OnUpdateTimeShift(CCmdUI* pCmdUI);
    afx_msg void OnUpdateReplayGain(CCmdUI* pCmdUI);
    afx_msg void OnUpdateChannelMapping(CCmdUI* pCmdUI);

    afx_msg void OnHScroll(UINT nSBCode, UINT nPos, CScrollBar* pScrollBar);
    afx_msg BOOL OnToolTipNotify(UINT id, NMHDR* pNMHDR, LRESULT* pResult);
    virtual void OnCancel();
};
