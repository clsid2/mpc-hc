/*
 * (C) 2010-2014 see Authors.txt
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
#include "SyncClock.h"

CSyncClockFilter::CSyncClockFilter(LPUNKNOWN pUnk, HRESULT* phr)
    : CBaseFilter(NAME("SyncClock"), nullptr, &m_Lock, CLSID_NULL)
    , m_Clock(static_cast<IBaseFilter*>(this), phr)
{
}

CSyncClockFilter::~CSyncClockFilter()
{
}

STDMETHODIMP CSyncClockFilter::AdjustClock(double adjustment)
{
    m_Clock.adjustment = adjustment;
    return S_OK;
}

STDMETHODIMP CSyncClockFilter::SetBias(double bias)
{
    m_Clock.bias = bias;
    return S_OK;
}

STDMETHODIMP CSyncClockFilter::GetBias(double* bias)
{
    *bias = m_Clock.bias;
    return S_OK;
}

STDMETHODIMP CSyncClockFilter::GetStartTime(REFERENCE_TIME* startTime)
{
    *startTime = m_tStart;
    return S_OK;
}

STDMETHODIMP CSyncClockFilter::NonDelegatingQueryInterface(REFIID riid, void** ppv)
{
    CheckPointer(ppv, E_POINTER);

    if (riid == IID_IReferenceClock) {
        return GetInterface(static_cast<IReferenceClock*>(&m_Clock), ppv);
    } else if (riid == IID_ISyncClock) {
        return GetInterface(static_cast<ISyncClock*>(this), ppv);
    } else {
        return CBaseFilter::NonDelegatingQueryInterface(riid, ppv);
    }
}

int CSyncClockFilter::GetPinCount()
{
    return 0;
}

CBasePin* CSyncClockFilter::GetPin(int i)
{
    UNREFERENCED_PARAMETER(i);
    return nullptr;
}

// CSyncClock methods
CSyncClock::CSyncClock(LPUNKNOWN pUnk, HRESULT* phr)
    : CBaseReferenceClock(NAME("SyncClock"), pUnk, phr)
    , adjustment(1.0)
    , bias(1.0)
    , m_rtPrivateTime(0)
    , m_llPerfFrequency(0)
    , m_rtPrevTime(0)
    , m_pCurrentRefClock(0)
    , m_pPrevRefClock(0)
{
    QueryPerformanceFrequency((LARGE_INTEGER*)&m_llPerfFrequency);
    m_rtPrivateTime = GetTicks100ns();
    m_rtPrevTime = m_rtPrivateTime;
}

REFERENCE_TIME CSyncClock::GetPrivateTime()
{
    CAutoLock cObjectLock(this);

    REFERENCE_TIME rtTime = GetTicks100ns();

    REFERENCE_TIME delta = rtTime - m_rtPrevTime;
    // We ignore that rtTime may wrap around. Not gonna happen too often
    m_rtPrevTime = rtTime;

    delta = (REFERENCE_TIME)((double)delta * adjustment * bias);
    m_rtPrivateTime = m_rtPrivateTime + delta;
    return m_rtPrivateTime;
}

REFERENCE_TIME CSyncClock::GetTicks100ns()
{
    LONGLONG i64Ticks100ns;
    if (m_llPerfFrequency != 0) {
        QueryPerformanceCounter((LARGE_INTEGER*)&i64Ticks100ns);
        i64Ticks100ns = LONGLONG((double(i64Ticks100ns) * 10000000) / double(m_llPerfFrequency) + 0.5);
        return (REFERENCE_TIME)i64Ticks100ns;
    }
    return 0;
}
