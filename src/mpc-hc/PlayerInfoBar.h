/*
 * (C) 2003-2006 Gabest
 * (C) 2006-2015 see Authors.txt
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

#include <atlcoll.h>
#include "StatusLabel.h"
#include "CMPCThemeToolTipCtrl.h"

class CMainFrame;

// CPlayerInfoBar

class CPlayerInfoBar : public CDialogBar
{
    DECLARE_DYNAMIC(CPlayerInfoBar)

private:
    CMainFrame* m_pMainFrame;

    CAutoPtrArray<CStatusLabel> m_label;
    CAutoPtrArray<CStatusLabel> m_info;

    CToolTipCtrl m_tooltip;
    CMPCThemeToolTipCtrl themedToolTip;
    void CreateToolTip();

    EventClient m_eventc;
    void EventCallback(MpcEvent ev);

    void Relayout();

public:
    CPlayerInfoBar(CMainFrame* pMainFrame);
    virtual ~CPlayerInfoBar();

    BOOL Create(CWnd* pParentWnd);

    bool SetLine(CString label, CString info);
    void GetLine(CString label, CString& info);
    bool RemoveLine(CString label);
    void RemoveAllLines();

protected:
    virtual BOOL PreCreateWindow(CREATESTRUCT& cs);
    virtual CSize CalcFixedLayout(BOOL bStretch, BOOL bHorz);
    virtual BOOL PreTranslateMessage(MSG* pMsg);

public:
    afx_msg BOOL OnEraseBkgnd(CDC* pDC);
    afx_msg void OnSize(UINT nType, int cx, int cy);
    afx_msg LRESULT OnMPCThemeChanged(WPARAM wParam, LPARAM lParam);

    DECLARE_MESSAGE_MAP()
    afx_msg void OnLButtonDown(UINT nFlags, CPoint point);
};
