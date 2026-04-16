/*
* (C) 2015-2017 see Authors.txt
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
#include "resource.h"
#include "CMPCThemeSpinButtonCtrl.h"
#include "CMPCThemePlayerListCtrl.h"
#include "EventDispatcher.h"
#include "CMPCThemeComboBox.h"
#include "AppSettings.h"

class CPPageToolBar : public CMPCThemePPageBase
{
    DECLARE_DYNAMIC(CPPageToolBar)
public:
    CPPageToolBar();
    virtual ~CPPageToolBar() = default;
    EventClient m_eventc;
    static CAppSettings::TOOLBAR_TYPE ExternalTBType(CStringW tbPath);

private:
    enum { IDD = IDD_PPAGETOOLBAR };

protected:
    CMPCThemeSpinButtonCtrl m_DefaultToolbarSizeCtrl;
    CMPCThemeComboBox m_cmbActiveTheme;
    CMPCThemeComboBox m_cmbToolbarAlignment;
    CMPCThemeComboBox m_cmbAction1, m_cmbAction2, m_cmbAction3, m_cmbAction4;
    CMPCThemeComboBox m_cmbRightAction1, m_cmbRightAction2, m_cmbRightAction3, m_cmbRightAction4;

    int m_iDefaultToolbarSize;

    virtual void DoDataExchange(CDataExchange* pDX) override;
    void AddCmdToAction(WORD id, CMPCThemeComboBox& actCombo);
    afx_msg void OnActionChange1();
    afx_msg void OnActionChange2();
    afx_msg void OnActionChange3();
    afx_msg void OnActionChange4();
    afx_msg void OnRightActionChange1();
    afx_msg void OnRightActionChange2();
    afx_msg void OnRightActionChange3();
    afx_msg void OnRightActionChange4();
    afx_msg void OnChangeTheme();
    void OnActionChange(CMPCThemeComboBox& actCombo);
    virtual BOOL OnInitDialog() override;
    void PopulateThemes();
    virtual BOOL OnApply() override;

    DECLARE_MESSAGE_MAP()
public:
};

