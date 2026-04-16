/*
 * (C) 2003-2006 Gabest
 * (C) 2006-2014, 2016 see Authors.txt
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
#include "MainFrm.h"
#include "GraphThread.h"
#include <afxsock.h>

IMPLEMENT_DYNCREATE(CGraphThread, CWinThread)

BOOL CGraphThread::InitInstance()
{
    SetThreadName(DWORD(-1), "GraphThread");
    AfxSocketInit();
    hr_coinit = CoInitialize(nullptr);
    return SUCCEEDED(hr_coinit) ? TRUE : FALSE;
}

int CGraphThread::ExitInstance()
{
    CoUninitialize();
    return __super::ExitInstance();
}

BEGIN_MESSAGE_MAP(CGraphThread, CWinThread)
    ON_THREAD_MESSAGE(TM_CLOSE, OnClose)
    ON_THREAD_MESSAGE(TM_DISPLAY_CHANGE, OnDisplayChange)
    ON_THREAD_MESSAGE(TM_EXIT, OnExit)
    ON_THREAD_MESSAGE(TM_OPEN, OnOpen)
    ON_THREAD_MESSAGE(TM_RESET, OnReset)
    ON_THREAD_MESSAGE(TM_TUNER_SCAN, OnTunerScan)
END_MESSAGE_MAP()

void CGraphThread::OnClose(WPARAM wParam, LPARAM lParam)
{
    ASSERT(m_pMainFrame);
    ASSERT(WaitForSingleObject(m_pMainFrame->m_evClosePrivateFinished, 0) == WAIT_TIMEOUT);
    if (m_pMainFrame->GetLoadState() == MLS::CLOSING) {
        m_pMainFrame->CloseMediaPrivate();
    }
    VERIFY(m_pMainFrame->m_evClosePrivateFinished.Set());
}

void CGraphThread::OnDisplayChange(WPARAM wParam, LPARAM lParam)
{
    if (m_pMainFrame) {
        m_pMainFrame->DisplayChange();
    }
    if (CAMEvent* e = (CAMEvent*)lParam) {
        e->Set();
    }
}

void CGraphThread::OnExit(WPARAM wParam, LPARAM lParam)
{
    PostQuitMessage(0);
    if (CAMEvent* e = (CAMEvent*)lParam) {
        e->Set();
    }
}

void CGraphThread::OnOpen(WPARAM wParam, LPARAM lParam)
{
    TRACE(_T("--> CGraphThread::OnOpen on thread: %lu\n"), GetCurrentThreadId());
    ASSERT(m_pMainFrame);
    ASSERT(WaitForSingleObject(m_pMainFrame->m_evOpenPrivateFinished, 0) == WAIT_TIMEOUT);
    if (m_pMainFrame->GetLoadState() == MLS::LOADING) {
        CAutoPtr<OpenMediaData> pOMD((OpenMediaData*)lParam);
        m_pMainFrame->OpenMediaPrivate(pOMD);
    }
    VERIFY(m_pMainFrame->m_evOpenPrivateFinished.Set());
}

void CGraphThread::OnReset(WPARAM wParam, LPARAM lParam)
{
    if (m_pMainFrame) {
        m_pMainFrame->ResetDevice();
    }
    if (CAMEvent* e = (CAMEvent*)lParam) {
        e->Set();
    }
}

void CGraphThread::OnTunerScan(WPARAM wParam, LPARAM lParam)
{
    if (m_pMainFrame) {
        CAutoPtr<TunerScanData> pTSD((TunerScanData*)lParam);
        m_pMainFrame->DoTunerScan(pTSD);
    }
}
