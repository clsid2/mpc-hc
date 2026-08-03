/*
 * (C) 2003-2006 Gabest
 * (C) 2006-2012, 2014-2017 see Authors.txt
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

#include "PPageBase.h"
#include "resource.h"
#include "CMPCThemePPageBase.h"
#include "CMPCThemeComboBox.h"


// CPPageOutput dialog

class CPPageOutput : public CMPCThemePPageBase
{
    DECLARE_DYNAMIC(CPPageOutput)

private:
    CStringArray m_AudioRendererDisplayNames;
    HICON m_tick, m_cross, m_warn;

    CMPCThemeComboBox m_iDSVideoRendererTypeCtrl;
    CMPCThemeComboBox m_iAudioRendererTypeCtrl;
    CMPCThemeComboBox m_SubtitleRendererCtrl;

    CStatic m_iDSSupportIcon;

    void UpdateSubtitleRendererList();

    UINT GetRendererTooltipID() const;
    void UpdateStatusIcon();

public:
    CPPageOutput();
    virtual ~CPPageOutput();

    // Dialog Data
    enum { IDD = IDD_PPAGEOUTPUT };
    int m_iDSVideoRendererType;
    int m_iAudioRendererType;
    int m_iMPCAudioRendererType;
    int m_iSaneAudioRendererType;
    CAppSettings::SubtitleRenderer m_lastSubrenderer;
    const CString& GetAudioRendererDisplayName();

protected:
    virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
    virtual BOOL OnInitDialog();
    virtual BOOL OnApply();

    DECLARE_MESSAGE_MAP()

public:
    afx_msg void OnUpdateVideoRendererSettings(CCmdUI* pCmdUI);
    afx_msg void OpenVideoRendererSettings();
    afx_msg void OnUpdateAudioRendererSettings(CCmdUI* pCmdUI);
    afx_msg void OpenAudioRendererSettings();
    afx_msg void OnDSRendererChange();
    afx_msg void OnAudioRendererChange();
    afx_msg void OnSubtitleRendererChange();
};
