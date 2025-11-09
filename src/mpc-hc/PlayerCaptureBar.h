/*
 * (C) 2003-2006 Gabest
 * (C) 2006-2014 see Authors.txt
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

#include "CMPCThemePlayerBar.h"
#include "PlayerCaptureDialog.h"

// CPlayerCaptureBar

class CPlayerCaptureBar : public CMPCThemePlayerBar
{
    DECLARE_DYNAMIC(CPlayerCaptureBar)

private:
    CWnd* m_pParent;

public:
    CPlayerCaptureDialog m_capdlg;

    CPlayerCaptureBar(CMainFrame* pMainFrame);
    virtual ~CPlayerCaptureBar();

    BOOL Create(CWnd* pParentWnd, UINT defDockBarID);

    virtual void ReloadTranslatableResources();

    void InitControls();
    EventClient m_eventc;
    void EventCallback(MpcEvent ev);

protected:
    virtual BOOL PreTranslateMessage(MSG* pMsg);

    DECLARE_MESSAGE_MAP()

    afx_msg void OnNcLButtonUp(UINT nHitTest, CPoint point);
};
