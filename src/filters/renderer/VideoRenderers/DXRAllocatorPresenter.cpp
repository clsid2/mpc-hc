/*
 * (C) 2006-2016 see Authors.txt
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
#include "RenderersSettings.h"
#include "DXRAllocatorPresenter.h"
#include "../../../SubPic/DX9SubPic.h"
#include "../../../SubPic/SubPicQueueImpl.h"
#include "moreuuids.h"

using namespace DSObjects;

//
// CDXRAllocatorPresenter
//

CDXRAllocatorPresenter::CDXRAllocatorPresenter(HWND hWnd, HRESULT& hr, CString& _Error)
    : CSubPicAllocatorPresenterImpl(hWnd, hr, &_Error)
    , m_ScreenSize(0, 0)
{
    if (FAILED(hr)) {
        _Error += L"ISubPicAllocatorPresenterImpl failed\n";
        return;
    }

    hr = S_OK;
}

CDXRAllocatorPresenter::~CDXRAllocatorPresenter()
{
    if (m_pSRCB) {
        // nasty, but we have to let it know about our death somehow
        ((CSubRenderCallback*)(ISubRenderCallback*)m_pSRCB)->SetDXRAP(nullptr);
    }

    // the order is important here
    m_pSubPicQueue = nullptr;
    m_pAllocator = nullptr;
    m_pDXR = nullptr;
}

STDMETHODIMP CDXRAllocatorPresenter::NonDelegatingQueryInterface(REFIID riid, void** ppv)
{
    /*
        if (riid == __uuidof(IVideoWindow))
            return GetInterface((IVideoWindow*)this, ppv);
        if (riid == __uuidof(IBasicVideo))
            return GetInterface((IBasicVideo*)this, ppv);
        if (riid == __uuidof(IBasicVideo2))
            return GetInterface((IBasicVideo2*)this, ppv);
    */
    /*
        if (riid == __uuidof(IVMRWindowlessControl))
            return GetInterface((IVMRWindowlessControl*)this, ppv);
    */

    if (riid != IID_IUnknown && m_pDXR) {
        if (SUCCEEDED(m_pDXR->QueryInterface(riid, ppv))) {
            return S_OK;
        }
    }

    return __super::NonDelegatingQueryInterface(riid, ppv);
}

HRESULT CDXRAllocatorPresenter::SetDevice(IDirect3DDevice9* pD3DDev)
{
    HRESULT hr = S_OK;

    if (!pD3DDev) {
        // release all resources
        m_pSubPicQueue = nullptr;
        m_pAllocator = nullptr;
        DestroySecondaryPipeline(); // release the secondary pipeline's device ref too (mirror the primary)
        m_pD3DDev = nullptr;
        __super::SetPosition(CRect(), CRect());
        return hr;
    }

    m_pD3DDev = pD3DDev;

    const CRenderersSettings& r = GetRenderersSettings();
    CSize largestScreen = GetLargestScreenSize(CSize(2560, 1440));
    InitMaxSubtitleTextureSize(r.subPicQueueSettings.nMaxResX, r.subPicQueueSettings.nMaxResY, largestScreen);

    if (m_pAllocator) {
        m_pAllocator->ChangeDevice(pD3DDev);
    } else {
        m_pAllocator = DEBUG_NEW CDX9SubPicAllocator(pD3DDev, m_maxSubtitleTextureSize, true);
        if (!m_pAllocator) {
            return E_FAIL;
        }
    }

    {
        // Lock before check because m_pSubPicQueue might be initialized in CSubPicAllocatorPresenterImpl::Connect
        CAutoLock cAutoLock(this);
        if (!m_pSubPicQueue) {
            m_pSubPicQueue = r.subPicQueueSettings.nSize > 0
                             ? (ISubPicQueue*)DEBUG_NEW CSubPicQueue(r.subPicQueueSettings, m_pAllocator, &hr)
                             : (ISubPicQueue*)DEBUG_NEW CSubPicQueueNoThread(r.subPicQueueSettings, m_pAllocator, &hr);
        } else {
            this->Unlock();
            m_pSubPicQueue->Invalidate();
        }
    }

    if (SUCCEEDED(hr) && m_pSubPicProvider) {
        m_pSubPicQueue->SetSubPicProvider(m_pSubPicProvider);
    }

    // Keep the secondary pipeline (if any) on the same device as the primary above.
    ChangeSecondaryDevice(pD3DDev);

    return hr;
}

CComPtr<ISubPicAllocator> CDXRAllocatorPresenter::CreateSecondaryAllocator()
{
    // Twin of the primary CDX9SubPicAllocator created in SetDevice; null if no device yet.
    if (!m_pD3DDev) {
        return nullptr;
    }
    return (ISubPicAllocator*)DEBUG_NEW CDX9SubPicAllocator(m_pD3DDev, m_maxSubtitleTextureSize, true);
}

HRESULT CDXRAllocatorPresenter::Render(
    REFERENCE_TIME rtStart, REFERENCE_TIME rtStop, REFERENCE_TIME atpf,
    int left, int top, int right, int bottom, int width, int height)
{
    CRect wndRect(0, 0, width, height);
    CRect videoRect(left, top, right, bottom);
    __super::SetPosition(wndRect, videoRect); // needed? should be already set by the player
    SetTime(rtStart);
    if (atpf > 0 && m_pSubPicQueue) {
        m_pSubPicQueue->SetFPS(10000000.0 / atpf);
    }
    AlphaBltSubPic(wndRect, videoRect);
    // paint the secondary subtitle on top of the primary one
    AlphaBltSubPic2(wndRect, videoRect);
    return S_OK;
}

// ISubPicAllocatorPresenter

STDMETHODIMP CDXRAllocatorPresenter::CreateRenderer(IUnknown** ppRenderer)
{
    CheckPointer(ppRenderer, E_POINTER);

    if (m_pDXR) {
        return E_UNEXPECTED;
    }
    m_pDXR.CoCreateInstance(CLSID_DXR, GetOwner());
    if (!m_pDXR) {
        return E_FAIL;
    }

    CComQIPtr<ISubRender> pSR = m_pDXR;
    if (!pSR) {
        m_pDXR = nullptr;
        return E_FAIL;
    }

    m_pSRCB = DEBUG_NEW CSubRenderCallback(this);
    if (FAILED(pSR->SetCallback(m_pSRCB))) {
        m_pDXR = nullptr;
        return E_FAIL;
    }

    (*ppRenderer = (IUnknown*)(INonDelegatingUnknown*)(this))->AddRef();

    MONITORINFO mi;
    mi.cbSize = sizeof(MONITORINFO);
    if (GetMonitorInfo(MonitorFromWindow(m_hWnd, MONITOR_DEFAULTTONEAREST), &mi)) {
        m_ScreenSize.SetSize(mi.rcMonitor.right - mi.rcMonitor.left, mi.rcMonitor.bottom - mi.rcMonitor.top);
    }

    return S_OK;
}

STDMETHODIMP_(void) CDXRAllocatorPresenter::SetPosition(RECT w, RECT v)
{
    if (CComQIPtr<IBasicVideo> pBV = m_pDXR) {
        pBV->SetDefaultSourcePosition();
        pBV->SetDestinationPosition(v.left, v.top, v.right - v.left, v.bottom - v.top);
    }

    if (CComQIPtr<IVideoWindow> pVW = m_pDXR) {
        pVW->SetWindowPosition(w.left, w.top, w.right - w.left, w.bottom - w.top);
    }

    SetVideoSize(GetVideoSize(false), GetVideoSize(true));
}

STDMETHODIMP_(SIZE) CDXRAllocatorPresenter::GetVideoSize(bool bCorrectAR) const
{
    SIZE size = {0, 0};

    if (!bCorrectAR) {
        if (CComQIPtr<IBasicVideo> pBV = m_pDXR) {
            pBV->GetVideoSize(&size.cx, &size.cy);
        }
    } else {
        if (CComQIPtr<IBasicVideo2> pBV2 = m_pDXR) {
            pBV2->GetPreferredAspectRatio(&size.cx, &size.cy);
        }
    }

    return size;
}

STDMETHODIMP_(bool) CDXRAllocatorPresenter::Paint(bool bAll)
{
    return false; // TODO
}

STDMETHODIMP CDXRAllocatorPresenter::GetDIB(BYTE* lpDib, DWORD* size)
{
    HRESULT hr = E_NOTIMPL;
    if (CComQIPtr<IBasicVideo> pBV = m_pDXR) {
        hr = pBV->GetCurrentImage((long*)size, (long*)lpDib);
    }
    return hr;
}

STDMETHODIMP CDXRAllocatorPresenter::SetPixelShader(LPCSTR pSrcData, LPCSTR pTarget)
{
    return E_NOTIMPL; // TODO
}
