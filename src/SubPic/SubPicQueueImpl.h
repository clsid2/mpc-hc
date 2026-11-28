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

#include <memory>
#include <mutex>
#include <condition_variable>

#include "ISubPic.h"
#include "SubPicQueueSettings.h"

class CSubPicQueueImpl : public CUnknown, public ISubPicQueue
{
    static const double DEFAULT_FPS;

protected:
    struct SubPicProviderWithSharedLock {
    private:
        unsigned int m_nRef = 0;
        std::mutex m_mutex;

    public:
        const CComPtr<ISubPicProvider> pSubPicProvider;

        SubPicProviderWithSharedLock(const CComPtr<ISubPicProvider>& pSubPicProvider)
            : pSubPicProvider(pSubPicProvider) {
        }

        HRESULT Lock() {
            HRESULT hr;

            if (pSubPicProvider) {
                std::lock_guard<std::mutex> lock(m_mutex);

                hr = S_OK;
                if ((m_nRef == 0 && SUCCEEDED(hr = pSubPicProvider->Lock()))
                        || m_nRef > 0) {
                    m_nRef++;
                }
            } else {
                hr = E_POINTER;
            }

            return hr;
        }

        HRESULT Unlock() {
            HRESULT hr;

            if (pSubPicProvider) {
                std::lock_guard<std::mutex> lock(m_mutex);

                hr = S_OK;
                if ((m_nRef == 1 && SUCCEEDED(hr = pSubPicProvider->Unlock()))
                        || m_nRef > 1) {
                    m_nRef--;
                }
            } else {
                hr = E_POINTER;
            }

            return hr;
        }
    };

private:
    CCritSec m_csSubPicProvider;
    std::shared_ptr<SubPicProviderWithSharedLock> m_pSubPicProviderWithSharedLock;

protected:
    double m_fps;
    REFERENCE_TIME m_rtTimePerFrame;
    REFERENCE_TIME m_rtTimePerSubFrame;
    REFERENCE_TIME m_rtNow;

    SubPicQueueSettings m_settings;

    CComPtr<ISubPicAllocator> m_pAllocator;

    std::shared_ptr<SubPicProviderWithSharedLock> GetSubPicProviderWithSharedLock() {
        CAutoLock cAutoLock(&m_csSubPicProvider);
        return m_pSubPicProviderWithSharedLock;
    }

    // For an animated subpic, rtStart is the time of the frame it is wanted for. Given the segment
    // it belongs to, it is rendered at that time, or at the segment's edge when that is close.
    HRESULT RenderTo(ISubPic* pSubPic, REFERENCE_TIME rtStart, REFERENCE_TIME rtStop, double fps, BOOL bIsAnimated,
                     REFERENCE_TIME rtSegmentStart = ISubPic::INVALID_SUBPIC_TIME, REFERENCE_TIME rtSegmentStop = ISubPic::INVALID_SUBPIC_TIME);

public:
    CSubPicQueueImpl(SubPicQueueSettings settings, ISubPicAllocator* pAllocator, HRESULT* phr);
    virtual ~CSubPicQueueImpl();

    DECLARE_IUNKNOWN;
    STDMETHODIMP NonDelegatingQueryInterface(REFIID riid, void** ppv);

    // ISubPicQueue

    STDMETHODIMP SetSubPicProvider(ISubPicProvider* pSubPicProvider);
    STDMETHODIMP GetSubPicProvider(ISubPicProvider** pSubPicProvider);

    STDMETHODIMP SetFPS(double fps);
    STDMETHODIMP SetTime(REFERENCE_TIME rtNow);
    /*
    STDMETHODIMP Invalidate(REFERENCE_TIME rtInvalidate = -1) PURE;
    STDMETHODIMP_(bool) LookupSubPic(REFERENCE_TIME rtNow, ISubPic** ppSubPic) PURE;
    STDMETHODIMP_(bool) LookupSubPic(REFERENCE_TIME rtNow, bool bAdviseBlocking, CComPtr<ISubPic>& pSubPic);

    STDMETHODIMP GetStats(int& nSubPics, REFERENCE_TIME& rtNow, REFERENCE_TIME& rtStart, REFERENCE_TIME& rtStop) PURE;
    STDMETHODIMP GetStats(int nSubPics, REFERENCE_TIME& rtStart, REFERENCE_TIME& rtStop) PURE;
    */
};

class CSubPicQueue : public CSubPicQueueImpl, protected CAMThread
{
protected:
    bool m_bExitThread;

    CComPtr<ISubPic> m_pSubPic;
    CInterfaceList<ISubPic> m_queue;

    std::mutex m_mutexSubpic; // to protect m_pSubPic
    std::mutex m_mutexQueue; // to protect m_queue
    std::condition_variable m_condQueueFull;
    std::condition_variable m_condQueueReady;

    CAMEvent m_runQueueEvent;

    REFERENCE_TIME m_rtNowLast;
    bool m_bFpsSet;

    bool m_bInvalidate;
    REFERENCE_TIME m_rtInvalidate;

    bool EnqueueSubPic(CComPtr<ISubPic>& pSubPic, bool bBlocking);
    REFERENCE_TIME GetCurrentRenderingTime();

    // CAMThread
    virtual DWORD ThreadProc();

public:
    CSubPicQueue(SubPicQueueSettings settings, ISubPicAllocator* pAllocator, HRESULT* phr);
    virtual ~CSubPicQueue();

    // ISubPicQueue

    STDMETHODIMP SetFPS(double fps);
    STDMETHODIMP SetTime(REFERENCE_TIME rtNow);

    STDMETHODIMP Invalidate(REFERENCE_TIME rtInvalidate = -1);
    STDMETHODIMP_(bool) LookupSubPic(REFERENCE_TIME rtNow, CComPtr<ISubPic>& pSubPic);
    STDMETHODIMP_(bool) LookupSubPic(REFERENCE_TIME rtNow, bool bAdviseBlocking, CComPtr<ISubPic>& pSubPic);

    STDMETHODIMP GetStats(int& nSubPics, REFERENCE_TIME& rtNow, REFERENCE_TIME& rtStart, REFERENCE_TIME& rtStop);
    STDMETHODIMP GetStats(int nSubPic, REFERENCE_TIME& rtStart, REFERENCE_TIME& rtStop);
};

class CSubPicQueueNoThread : public CSubPicQueueImpl
{
protected:
    CCritSec m_csLock;
    CComPtr<ISubPic> m_pSubPic;

public:
    CSubPicQueueNoThread(SubPicQueueSettings settings, ISubPicAllocator* pAllocator, HRESULT* phr);
    virtual ~CSubPicQueueNoThread();

    // ISubPicQueue

    STDMETHODIMP SetFPS(double fps);

    STDMETHODIMP Invalidate(REFERENCE_TIME rtInvalidate = -1);
    STDMETHODIMP_(bool) LookupSubPic(REFERENCE_TIME rtNow, CComPtr<ISubPic>& pSubPic);
    STDMETHODIMP_(bool) LookupSubPic(REFERENCE_TIME rtNow, bool bAdviseBlocking, CComPtr<ISubPic>& pSubPic);

    STDMETHODIMP GetStats(int& nSubPics, REFERENCE_TIME& rtNow, REFERENCE_TIME& rtStart, REFERENCE_TIME& rtStop);
    STDMETHODIMP GetStats(int nSubPic, REFERENCE_TIME& rtStart, REFERENCE_TIME& rtStop);
};
