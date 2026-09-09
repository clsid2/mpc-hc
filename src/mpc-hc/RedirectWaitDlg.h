/*
 * (C) 2026 see Authors.txt
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

#include <functional>
#include "CMPCThemeDialog.h"

// Shown while this process retries handing its command line to a first instance that has
// stopped responding. The retry runs on the dialog's own timer, so the dialog closes itself
// as soon as the other instance answers instead of waiting for the user to dismiss it.
//
// It renders unthemed, and deliberately so: this runs from InitInstance before LoadSettings(),
// so m_bThemeLoaded is still false and CMPCThemeDialog draws as a plain dialog. Loading the
// settings earlier just to theme this one dialog would slow down every redirect, which is the
// path this whole change exists to keep quick. Deriving from CMPCThemeDialog anyway costs
// nothing and means it picks up the theme if the startup order ever changes.
class CRedirectWaitDlg : public CMPCThemeDialog
{
    DECLARE_DYNAMIC(CRedirectWaitDlg)

public:
    // Returns IDOK once the command line has been handed over, IDABORT if the other instance
    // has gone away, or 0 to keep waiting.
    typedef std::function<int()> RetryFn;

    CRedirectWaitDlg(RetryFn fnRetry, ULONGLONG tWaitForMs, CWnd* pParent = nullptr);
    virtual ~CRedirectWaitDlg();

    // Dialog Data
    enum { IDD = IDD_REDIRECT_WAIT_DLG };

protected:
    virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
    virtual BOOL OnInitDialog();
    afx_msg void OnTimer(UINT_PTR nIDEvent);

    DECLARE_MESSAGE_MAP()

private:
    void Finish(int nResult);

    RetryFn m_fnRetry;
    ULONGLONG m_tWaitFor;
    ULONGLONG m_tStart;
    UINT_PTR m_nTimerId;
};
