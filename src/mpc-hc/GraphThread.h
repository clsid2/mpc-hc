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

class CMainFrame;

class CGraphThread : public CWinThread
{
    DECLARE_DYNCREATE(CGraphThread);
public:
    CGraphThread() {}

    BOOL InitInstance();
    int ExitInstance();
    void SetMainFrame(CMainFrame* pMainFrame) { m_pMainFrame = pMainFrame; }

    HRESULT hr_coinit = E_FAIL;

    enum {
        TM_EXIT = WM_APP,
        TM_OPEN,
        TM_CLOSE,
        TM_RESET,
        TM_TUNER_SCAN,
        TM_DISPLAY_CHANGE
    };

protected:
    DECLARE_MESSAGE_MAP()
    afx_msg void OnClose(WPARAM wParam, LPARAM lParam);
    afx_msg void OnDisplayChange(WPARAM wParam, LPARAM lParam);
    afx_msg void OnExit(WPARAM wParam, LPARAM lParam);
    afx_msg void OnOpen(WPARAM wParam, LPARAM lParam);
    afx_msg void OnReset(WPARAM wParam, LPARAM lParam);
    afx_msg void OnTunerScan(WPARAM wParam, LPARAM lParam);

private:
    CMainFrame* m_pMainFrame = nullptr;
};
