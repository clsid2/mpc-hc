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

#include "stdafx.h"
#include "mplayerc.h"
#include "RedirectWaitDlg.h"

// CRedirectWaitDlg dialog

IMPLEMENT_DYNAMIC(CRedirectWaitDlg, CMPCThemeDialog)

CRedirectWaitDlg::CRedirectWaitDlg(RetryFn fnRetry, ULONGLONG tWaitForMs, CWnd* pParent /*= nullptr*/)
    : CMPCThemeDialog(CRedirectWaitDlg::IDD, pParent)
    , m_fnRetry(fnRetry)
    , m_tWaitFor(tWaitForMs)
    , m_tStart(0)
    , m_nTimerId(0)
{
}

CRedirectWaitDlg::~CRedirectWaitDlg()
{
}

void CRedirectWaitDlg::DoDataExchange(CDataExchange* pDX)
{
    CMPCThemeDialog::DoDataExchange(pDX);
    fulfillThemeReqs();
}

BEGIN_MESSAGE_MAP(CRedirectWaitDlg, CMPCThemeDialog)
    ON_WM_TIMER()
END_MESSAGE_MAP()

// CRedirectWaitDlg message handlers

BOOL CRedirectWaitDlg::OnInitDialog()
{
    CMPCThemeDialog::OnInitDialog();

    m_tStart = GetTickCount64();
    m_nTimerId = SetTimer(1, 1000, nullptr);

    return TRUE;
}

void CRedirectWaitDlg::Finish(int nResult)
{
    if (m_nTimerId) {
        KillTimer(m_nTimerId);
        m_nTimerId = 0;
    }
    EndDialog(nResult);
}

void CRedirectWaitDlg::OnTimer(UINT_PTR nIDEvent)
{
    if (m_nTimerId && nIDEvent == m_nTimerId) {
        if (m_fnRetry) {
            const int nResult = m_fnRetry();
            if (nResult != 0) {
                Finish(nResult);
                return;
            }
        }

        if (GetTickCount64() - m_tStart >= m_tWaitFor) {
            Finish(IDNO); // gave it long enough, let the caller ask the user what to do
            return;
        }
    }

    CMPCThemeDialog::OnTimer(nIDEvent);
}
