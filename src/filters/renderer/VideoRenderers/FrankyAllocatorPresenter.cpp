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
#include "FrankyAllocatorPresenter.h"
#include "../../../SubPic/DX9SubPic.h"
#include "../../../SubPic/DX11SubPic.h"
#include "../../../SubPic/SubPicQueueImpl.h"
#include "moreuuids.h"
#include "FilterInterfaces.h"
#include "Variables.h"
#include "IPinHook.h"
#include "Utils.h"
#include <mfapi.h>

using namespace DSObjects;

//
// CFrankyAllocatorPresenter
//

CFrankyAllocatorPresenter::CFrankyAllocatorPresenter(HWND hWnd, HRESULT& hr, CString& _Error)
    : CSubPicAllocatorPresenterImpl(hWnd, hr, &_Error)
{
    if (FAILED(hr)) {
        _Error += L"ISubPicAllocatorPresenterImpl failed\n";
        return;
    }

    hr = S_OK;
}

CFrankyAllocatorPresenter::~CFrankyAllocatorPresenter()
{
    // the order is important here
    m_pSubPicQueue.Release();
    m_pAllocator.Release();
    m_pRenderer.Release();

    if (m_bHookedNewSegment) {
        UnhookNewSegment();
    }
}

STDMETHODIMP CFrankyAllocatorPresenter::NonDelegatingQueryInterface(REFIID riid, void** ppv)
{
    if (riid != IID_IUnknown && m_pRenderer) {
        if (SUCCEEDED(m_pRenderer->QueryInterface(riid, ppv))) {
            return S_OK;
        }
    }

	return QI(ISubRenderCallback)
		   QI(ISubRenderCallback2)
		   QI(ISubRenderCallback3)
		   QI(ISubRenderCallback4)
		   QI(ISubPicAllocatorPresenter3)
		   QI(ISubRender11Callback)
		   __super::NonDelegatingQueryInterface(riid, ppv);
}

HRESULT CFrankyAllocatorPresenter::SetDevice(IDirect3DDevice9* pD3DDev)
{
    if (!pD3DDev) {
        // release all resources
        m_pSubPicQueue.Release();
        m_pAllocator.Release();
        return S_OK;
    }

    const CRenderersSettings& r = GetRenderersSettings();

    CSize screenSize;
    MONITORINFO mi = { sizeof(MONITORINFO) };
    if (GetMonitorInfoW(MonitorFromWindow(m_hWnd, MONITOR_DEFAULTTONEAREST), &mi)) {
        screenSize.SetSize(mi.rcMonitor.right - mi.rcMonitor.left, mi.rcMonitor.bottom - mi.rcMonitor.top);
    }

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

    HRESULT hr = S_OK;
    bool bInvalidate = false;
    {
        // Lock before check because m_pSubPicQueue might be initialized in CSubPicAllocatorPresenterImpl::Connect
        CAutoLock cAutoLock(this);
        if (!m_pSubPicQueue) {
            m_pSubPicQueue = r.subPicQueueSettings.nSize > 0
                             ? (ISubPicQueue*)DEBUG_NEW CSubPicQueue(r.subPicQueueSettings, m_pAllocator, &hr)
                             : (ISubPicQueue*)DEBUG_NEW CSubPicQueueNoThread(r.subPicQueueSettings, m_pAllocator, &hr);
        } else {
            bInvalidate = true;
        }
    }
    if (bInvalidate) {
        // Invalidate outside the lock, the queue thread may need it
        m_pSubPicQueue->Invalidate();
    }

    if (SUCCEEDED(hr) && m_pSubPicQueue && m_pSubPicProvider) {
        m_pSubPicQueue->SetSubPicProvider(m_pSubPicProvider);
    }

    return hr;
}


// ISubRenderCallback4 (called through CSubRenderCallback)

HRESULT CFrankyAllocatorPresenter::RenderEx3(REFERENCE_TIME rtStart,
											REFERENCE_TIME rtStop,
											REFERENCE_TIME atpf,
											RECT croppedVideoRect,
											RECT originalVideoRect,
											RECT viewportRect,
											const double videoStretchFactor,
											int xOffsetInPixels, DWORD flags)
{
	CheckPointer(m_pSubPicQueue, E_UNEXPECTED);

	if (!g_bExternalSubtitleTime) {
		if (g_bExternalSubtitle && g_dRate != 0.0) {
			const REFERENCE_TIME sampleTime = rtStart - g_tSegmentStart;
			SetTime(REFERENCE_TIME(g_tSegmentStart + sampleTime * g_dRate));
		} else {
			SetTime(rtStart);
		}
	}
	if (atpf > 0) {
		m_fps = 10000000.0 / atpf;
		m_pSubPicQueue->SetFPS(m_fps);
	}

	return AlphaBltSubPic(viewportRect, croppedVideoRect, nullptr, videoStretchFactor, xOffsetInPixels);
}

// ISubRender11Callback

HRESULT CFrankyAllocatorPresenter::SetDevice11(ID3D11Device* pD3DDev)
{
	if (!pD3DDev) {
		// release all resources
		m_pSubPicQueue.Release();
		m_pAllocator.Release();
		return S_OK;
	}

	CRenderersSettings& r = GetRenderersSettings();

    CSize screenSize;
    MONITORINFO mi = { sizeof(MONITORINFO) };
    if (GetMonitorInfoW(MonitorFromWindow(m_hWnd, MONITOR_DEFAULTTONEAREST), &mi)) {
        screenSize.SetSize(mi.rcMonitor.right - mi.rcMonitor.left, mi.rcMonitor.bottom - mi.rcMonitor.top);
    }

    CSize largestScreen = GetLargestScreenSize(CSize(2560, 1440));
    InitMaxSubtitleTextureSize(r.subPicQueueSettings.nMaxResX, r.subPicQueueSettings.nMaxResY, largestScreen);

	if (m_pAllocator) {
		m_pAllocator->ChangeDevice(pD3DDev);
	}
	else {
		m_pAllocator = DEBUG_NEW CDX11SubPicAllocator(pD3DDev, m_maxSubtitleTextureSize);
		if (!m_pAllocator) {
			return E_FAIL;
		}
	}

	HRESULT hr = S_OK;
	if (!m_pSubPicQueue) {
		CAutoLock cAutoLock(this);
		m_pSubPicQueue = r.subPicQueueSettings.nSize
			? (ISubPicQueue*)DEBUG_NEW CSubPicQueue(r.subPicQueueSettings, m_pAllocator, &hr)
			: (ISubPicQueue*)DEBUG_NEW CSubPicQueueNoThread(r.subPicQueueSettings, m_pAllocator, &hr);
	}
	else {
		m_pSubPicQueue->Invalidate();
	}

	if (SUCCEEDED(hr) && m_pSubPicQueue && m_pSubPicProvider) {
		m_pSubPicQueue->SetSubPicProvider(m_pSubPicProvider);
	}

	return hr;
}

HRESULT CFrankyAllocatorPresenter::Render11(
											REFERENCE_TIME rtStart,
											REFERENCE_TIME rtStop,
											REFERENCE_TIME atpf,
											RECT croppedVideoRect,
											RECT originalVideoRect,
											RECT viewportRect,
											const double videoStretchFactor,
											int xOffsetInPixels, DWORD flags)
{
	return RenderEx3(rtStart, rtStop, atpf, croppedVideoRect, originalVideoRect, viewportRect, videoStretchFactor, xOffsetInPixels, flags);
}

// ISubPicAllocatorPresenter

STDMETHODIMP CFrankyAllocatorPresenter::CreateRenderer(IUnknown** ppRenderer)
{
    CheckPointer(ppRenderer, E_POINTER);

    if (m_pRenderer) {
        return E_UNEXPECTED;
    }

    HRESULT hr = m_pRenderer.CoCreateInstance(CLSID_FrankyRenderer, GetOwner());

    if (FAILED(hr)) {
        return hr;
    }

    if (!m_pRenderer) {
        return E_FAIL;
    }

    CComQIPtr<ISubRender> pSR = m_pRenderer;
    CComQIPtr<ISubRender11> pSR11 = m_pRenderer;
    if (!pSR && !pSR11) {
        m_pRenderer = nullptr;
        return E_FAIL;
    }

    if (pSR && FAILED(pSR->SetCallback(this))) {
        m_pRenderer = nullptr;
        return E_FAIL;
    }
    if (pSR11 && FAILED(pSR11->SetCallback11(this))) {
        m_pRenderer = nullptr;
        return E_FAIL;
    }

    (*ppRenderer = (IUnknown*)(INonDelegatingUnknown*)(this))->AddRef();

	if (CComQIPtr<IExFilterConfig> pIExFilterConfig = m_pRenderer) {
        pIExFilterConfig->Flt_SetBool("lessRedraws", true);
        pIExFilterConfig->Flt_SetBool("d3dFullscreenControl", true);
	}

    CComQIPtr<IBaseFilter> pBF = m_pRenderer;
    CComPtr<IPin> pPin = GetFirstPin(pBF);
    m_bHookedNewSegment = HookNewSegment((IPinC*)(IPin*)pPin);

    return S_OK;
}

STDMETHODIMP_(void) CFrankyAllocatorPresenter::SetPosition(RECT w, RECT v)
{
    if (CComQIPtr<IBasicVideo> pBV = m_pRenderer) {
        pBV->SetDefaultSourcePosition();
        pBV->SetDestinationPosition(v.left, v.top, v.right - v.left, v.bottom - v.top);
    }

    if (CComQIPtr<IVideoWindow> pVW = m_pRenderer) {
        pVW->SetWindowPosition(w.left, w.top, w.right - w.left, w.bottom - w.top);
    }

	__super::SetPosition(w, v);
}

// ISubPicAllocatorPresenter

STDMETHODIMP CFrankyAllocatorPresenter::SetRotation(int rotation)
{
	if (AngleStep90(rotation)) {
		HRESULT hr = E_NOTIMPL;
		if (CComQIPtr<IExFilterConfig> pIExFilterConfig = m_pRenderer) {
			int curRotation = rotation;
			hr = pIExFilterConfig->Flt_GetInt("rotation", &curRotation);
			if (SUCCEEDED(hr) && rotation != curRotation) {
				hr = pIExFilterConfig->Flt_SetInt("rotation", rotation);
				if (SUCCEEDED(hr)) {
					m_bOtherTransform = true;
				}
			}
		}
		return hr;
	}
	return E_INVALIDARG;
}

STDMETHODIMP_(int) CFrankyAllocatorPresenter::GetRotation()
{
    if (CComQIPtr<IExFilterConfig> pIExFilterConfig = m_pRenderer) {
        int rotation = 0;
        if (SUCCEEDED(pIExFilterConfig->Flt_GetInt("rotation", &rotation))) {
            return rotation;
        }
    }
    return 0;
}

STDMETHODIMP CFrankyAllocatorPresenter::SetFlip(bool flip) {
    HRESULT hr = E_NOTIMPL;
    if (CComQIPtr<IExFilterConfig> pIExFilterConfig = m_pRenderer) {
        bool curFlip = flip;
        hr = pIExFilterConfig->Flt_GetBool("flip", &curFlip);
        if (SUCCEEDED(hr) && flip != curFlip) {
            hr = pIExFilterConfig->Flt_SetBool("flip", flip);
            if (SUCCEEDED(hr)) {
                m_bOtherTransform = true;
            }
        }
    }
    return hr;
}

STDMETHODIMP_(bool) CFrankyAllocatorPresenter::GetFlip() {
    if (CComQIPtr<IExFilterConfig> pIExFilterConfig = m_pRenderer) {
        bool flip = false;
        if (SUCCEEDED(pIExFilterConfig->Flt_GetBool("flip", &flip))) {
            return flip;
        }
    }
    return false;
}

STDMETHODIMP_(SIZE) CFrankyAllocatorPresenter::GetVideoSize(bool bCorrectAR) const
{
    SIZE size = {0, 0};
    if (bCorrectAR) {
        if (CComQIPtr<IBasicVideo2> pBV = m_pRenderer) {
            pBV->GetPreferredAspectRatio(&size.cx, &size.cy);
        }
    } else if (CComQIPtr<IBasicVideo> pBV = m_pRenderer) {
        pBV->GetVideoSize(&size.cx, &size.cy);
    }
    return size;
}

STDMETHODIMP_(bool) CFrankyAllocatorPresenter::Paint(bool /*bAll*/)
{
    if (CComQIPtr<IExFilterConfig> pIExFilterConfig = m_pRenderer) {
        return SUCCEEDED(pIExFilterConfig->Flt_SetBool("cmd_redraw", true));
    }
    return false;
}

STDMETHODIMP CFrankyAllocatorPresenter::GetDIB(BYTE* lpDib, DWORD* size)
{
    HRESULT hr = E_NOTIMPL;
    if (CComQIPtr<IBasicVideo> pBV = m_pRenderer) {
        hr = pBV->GetCurrentImage((long*)size, (long*)lpDib);
    }
    return hr;
}

STDMETHODIMP CFrankyAllocatorPresenter::GetDisplayedImage(LPVOID* dibImage)
{
	if (CComQIPtr<IExFilterConfig> pIExFilterConfig = m_pRenderer) {
		unsigned size = 0;
		HRESULT hr = pIExFilterConfig->Flt_GetBin("displayedImage", dibImage, &size);

		return hr;
	}

	return E_FAIL;
}

STDMETHODIMP_(int) CFrankyAllocatorPresenter::GetPixelShaderMode()
{
	if (CComQIPtr<IExFilterConfig> pIExFilterConfig = m_pRenderer) {
		int rtype = 0;
		if (S_OK == pIExFilterConfig->Flt_GetInt("renderType", &rtype)) {
			return rtype;
		}
	}
	return -1;
}

STDMETHODIMP CFrankyAllocatorPresenter::ClearPixelShaders(int target)
{
	HRESULT hr = E_FAIL;

	if (CComQIPtr<IExFilterConfig> pIExFilterConfig = m_pRenderer) {
        if (TARGET_SCREEN == target) {
            hr = pIExFilterConfig->Flt_SetBool("cmd_clearPostScaleShaders", true);
        } else {
            hr = pIExFilterConfig->Flt_SetBool("cmd_clearPreScaleShaders", true);
        }
	}
	return hr;
}

static BYTE* WriteChunk(BYTE* dst, const uint32_t code, const int32_t size, BYTE* data)
{
	memcpy(dst, &code, 4);
	dst += 4;
	memcpy(dst, &size, 4);
	dst += 4;
	memcpy(dst, data, size);
	dst += size;

	return dst;
}

STDMETHODIMP CFrankyAllocatorPresenter::AddPixelShader(int target, LPCWSTR name, LPCSTR profile, LPCSTR sourceCode)
{
	HRESULT hr = E_FAIL;

	const int namesize = (int) wcslen(name) * sizeof(wchar_t);
	const int codesize = (int) strlen(sourceCode);

	int iProfile = 0;
	if (!strcmp(profile, "ps_2_0") || !strcmp(profile, "ps_2_a") || !strcmp(profile, "ps_2_b") || !strcmp(profile, "ps_3_0")) {
		iProfile = 3;
	}
	else if (!strcmp(profile, "ps_4_0")) {
		iProfile = 4;
	}

	if (codesize) {
		if (CComQIPtr<IExFilterConfig> pIExFilterConfig = m_pRenderer) {
			int rtype = 0;
			hr = pIExFilterConfig->Flt_GetInt("renderType", &rtype);
			if (S_OK == hr && (rtype == 9 && iProfile == 3 || rtype == 11 && iProfile == 4)) {
				int size = 8 + codesize;
				if (namesize) {
					size += 8 + namesize;
				}

				BYTE* pBuf = (BYTE*)LocalAlloc(LMEM_FIXED, size);
				if (pBuf) {
					BYTE* p = pBuf;
					if (namesize) {
						p = WriteChunk(p, FCC('NAME'), namesize, (BYTE*)name);
					}
					p = WriteChunk(p, FCC('CODE'), codesize, (BYTE*)sourceCode);

                    if (target == TARGET_FRAME) {
                        hr = pIExFilterConfig->Flt_SetBin("cmd_addPreScaleShader", (LPVOID)pBuf, size);
                    }
                    if (target == TARGET_SCREEN || hr != S_OK) {
                        hr = pIExFilterConfig->Flt_SetBin("cmd_addPostScaleShader", (LPVOID)pBuf, size);
                    }
					LocalFree(pBuf);
				}
			}
		}
	}

	return hr;
}

STDMETHODIMP_(bool) CFrankyAllocatorPresenter::DisplayChange()
{
	if (CComQIPtr<IExFilterConfig> pIExFilterConfig = m_pRenderer) {
		return SUCCEEDED(pIExFilterConfig->Flt_SetBool("displayChange", true));
	}

	return false;
}

STDMETHODIMP_(bool) CFrankyAllocatorPresenter::IsRendering()
{
    if (CComQIPtr<IExFilterConfig> pIExFilterConfig = m_pRenderer) {
        int playbackState;
        if (SUCCEEDED(pIExFilterConfig->Flt_GetInt("playbackState", &playbackState))) {
            return playbackState == State_Running;
        }
    }

    return false;
}

STDMETHODIMP CFrankyAllocatorPresenter::SetPixelShader(LPCSTR pSrcData, LPCSTR pTarget)
{
    return E_NOTIMPL; // TODO
}

STDMETHODIMP_(bool) CFrankyAllocatorPresenter::ToggleStats() {
    if (CComQIPtr<IExFilterConfig> pIExFilterConfig = m_pRenderer) {
        if (pIExFilterConfig) {
            bool statsEnable = 0;
            if (S_OK == pIExFilterConfig->Flt_GetBool("statsEnable", &statsEnable)) {
                statsEnable = !statsEnable;
                pIExFilterConfig->Flt_SetBool("statsEnable", statsEnable);
                return statsEnable;
            }
        }
    }

    return false;
}
