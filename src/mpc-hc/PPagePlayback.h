/*
 * (C) 2003-2006 Gabest
 * (C) 2006-2016 see Authors.txt
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

#include "CDarkPPageBase.h"
#include "CDarkComboBox.h"
#include "CDarkSliderCtrl.h"
#include "CDarkSpinButtonCtrl.h"
#include "CDarkEdit.h"

// CPPagePlayback dialog

class CPPagePlayback : public CDarkPPageBase
{
    DECLARE_DYNAMIC(CPPagePlayback)

    //  private:
    int m_oldVolume; //not very nice solution

public:
    CPPagePlayback();
    virtual ~CPPagePlayback();

    CDarkSliderCtrl m_volumectrl;
    CDarkSliderCtrl m_balancectrl;
    int m_nVolume;
    int m_nBalance;
    int m_nVolumeStep;
    CDarkSpinButtonCtrl m_VolumeStepCtrl;
    int m_nSpeedStep;
    CDarkSpinButtonCtrl m_SpeedStepCtrl;
    int m_iLoopForever;
    int m_iLoopMode;
    CDarkComboBox m_LoopMode;
    CDarkEdit m_loopnumctrl;
    int m_nLoops;
    int m_iAfterPlayback;
    int m_iZoomLevel;
    BOOL m_iRememberZoomLevel;
    int m_nAutoFitFactor;
    CDarkSpinButtonCtrl m_AutoFitFactorCtrl;
    BOOL m_fAutoloadAudio;
    BOOL m_fEnableWorkerThreadForOpening;
    BOOL m_fReportFailedPins;
    CString m_subtitlesLanguageOrder;
    CString m_audiosLanguageOrder;
    BOOL m_fAllowOverridingExternalSplitterChoice;

    CDarkComboBox m_zoomlevelctrl;
    CDarkComboBox m_afterPlayback;

    // Dialog Data
    enum { IDD = IDD_PPAGEPLAYBACK };

protected:
    virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
    virtual BOOL OnInitDialog();
    virtual BOOL OnApply();

    DECLARE_MESSAGE_MAP()
    CDarkToolTipCtrl darkTT;
public:
    afx_msg void OnHScroll(UINT nSBCode, UINT nPos, CScrollBar* pScrollBar);
    afx_msg void OnBnClickedRadio12(UINT nID);
    afx_msg void OnUpdateLoopNum(CCmdUI* pCmdUI);
    afx_msg void OnUpdateAutoZoomCombo(CCmdUI* pCmdUI);
    afx_msg void OnUpdateAfterPlayback(CCmdUI* pCmdUI);
    afx_msg void OnUpdateSpeedStep(CCmdUI* pCmdUI);

    afx_msg void OnBalanceTextDblClk();
    afx_msg BOOL OnToolTipNotify(UINT id, NMHDR* pNMHDR, LRESULT* pResult);
    virtual void OnCancel();
    BOOL PreTranslateMessage(MSG * pMsg);
};
