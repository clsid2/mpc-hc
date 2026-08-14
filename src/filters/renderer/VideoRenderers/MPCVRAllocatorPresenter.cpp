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
#include "MPCVRAllocatorPresenter.h"
#include "../../../SubPic/DX9SubPic.h"
#include "../../../SubPic/DX11SubPic.h"
#include "../../../SubPic/SubPicQueueImpl.h"
#include "moreuuids.h"
#include "FilterInterfaces.h"
#include "Variables.h"
#include "IPinHook.h"
#include "Utils.h"
#include <mfapi.h>
#include "../DSUtil/PathUtils.h"
#include "../DSUtil/FileVersionInfo.h"

using namespace DSObjects;
#define MPCVR_LIB_DIR L"MPCVR\\"
#ifndef _WIN64
#define MPCVR_LIB_FILENAME L"MpcVideoRenderer.ax"
#else
#define MPCVR_LIB_FILENAME L"MpcVideoRenderer64.ax"
#endif

#define MPCVR_FILTERS_VERSION(major, minor, rev, commit) ((QWORD)(major) << 48 | (QWORD)(minor) << 32 | (QWORD)(rev) << 16 | (QWORD)(commit))


//
// CMPCVRAllocatorPresenter
//

CMPCVRAllocatorPresenter::CMPCVRAllocatorPresenter(HWND hWnd, HRESULT& hr, CString& _Error)
    : CSubPicAllocatorPresenterImpl(hWnd, hr, &_Error)
{
    if (FAILED(hr)) {
        _Error += L"ISubPicAllocatorPresenterImpl failed\n";
        return;
    }

    hr = S_OK;
}

CMPCVRAllocatorPresenter::~CMPCVRAllocatorPresenter()
{
    // the order is important here
    m_pSubPicQueue.Release();
    m_pAllocator.Release();
    m_pMPCVR.Release();

    if (m_bHookedNewSegment) {
        UnhookNewSegment();
    }
}

STDMETHODIMP CMPCVRAllocatorPresenter::NonDelegatingQueryInterface(REFIID riid, void** ppv)
{
    if (riid != IID_IUnknown && m_pMPCVR) {
        if (SUCCEEDED(m_pMPCVR->QueryInterface(riid, ppv))) {
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

HRESULT CMPCVRAllocatorPresenter::SetDevice(IDirect3DDevice9* pD3DDev)
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

    if (SUCCEEDED(hr) && m_pSubPicQueue && m_pSubPicProvider) {
        m_pSubPicQueue->SetSubPicProvider(m_pSubPicProvider);
    }

    return hr;
}


// ISubRenderCallback4 (called through CSubRenderCallback)

HRESULT CMPCVRAllocatorPresenter::RenderEx3(REFERENCE_TIME rtStart,
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

HRESULT CMPCVRAllocatorPresenter::SetDevice11(ID3D11Device* pD3DDev)
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

HRESULT CMPCVRAllocatorPresenter::Render11(
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

CStringW GetInternalLibraryPath() {
    CString filterPath = PathUtils::CombinePaths(PathUtils::GetProgramPath(), MPCVR_LIB_DIR);
    filterPath = PathUtils::CombinePaths(filterPath, MPCVR_LIB_FILENAME);
    return filterPath;
}

bool CMPCVRAllocatorPresenter::CheckVersion(CString filterPath) {
    QWORD fversion = FileVersionInfo::GetFileVersionNum(filterPath);
    return fversion >= MPCVR_FILTERS_VERSION(0, 9, 0, 0);
}

bool CMPCVRAllocatorPresenter::HasInternalMPCVRFilter() {
    CStringW lPath = GetInternalLibraryPath();
    return CheckVersion(lPath);
}

HRESULT CMPCVRAllocatorPresenter::InstantiateInternalMPCVR(CComPtr<IUnknown>& m_pMPCVR, LPUNKNOWN pUnkOuter)
{
    if (HasInternalMPCVRFilter()) {
        CComPtr<IClassFactory> pCF;
        HRESULT hr = LoadExternalObject(GetInternalLibraryPath(), CLSID_MPCVR, IID_PPV_ARGS(&pCF));
        if (SUCCEEDED(hr)) {
            hr = pCF->CreateInstance(pUnkOuter, IID_PPV_ARGS(&m_pMPCVR.p));
        }
        return hr;
    }
    return E_FAIL;
}

// ISubPicAllocatorPresenter

STDMETHODIMP CMPCVRAllocatorPresenter::CreateRenderer(IUnknown** ppRenderer)
{
    CheckPointer(ppRenderer, E_POINTER);

    if (m_pMPCVR) {
        return E_UNEXPECTED;
    }

    HRESULT hr = InstantiateInternalMPCVR(m_pMPCVR, GetOwner());

    if (FAILED(hr) && IsCLSIDRegistered(CLSID_MPCVR)) {
        hr = m_pMPCVR.CoCreateInstance(CLSID_MPCVR, GetOwner());
    }

    if (FAILED(hr)) {
        return hr;
    }

    if (!m_pMPCVR) {
        return E_FAIL;
    }

    CComQIPtr<ISubRender> pSR = m_pMPCVR;
    CComQIPtr<ISubRender11> pSR11 = m_pMPCVR;
    if (!pSR && !pSR11) {
        m_pMPCVR = nullptr;
        return E_FAIL;
    }

    if (pSR && FAILED(pSR->SetCallback(this))) {
        m_pMPCVR = nullptr;
        return E_FAIL;
    }
    if (pSR11 && FAILED(pSR11->SetCallback11(this))) {
        m_pMPCVR = nullptr;
        return E_FAIL;
    }

    (*ppRenderer = (IUnknown*)(INonDelegatingUnknown*)(this))->AddRef();

	if (CComQIPtr<IExFilterConfig> pIExFilterConfig = m_pMPCVR) {
        pIExFilterConfig->Flt_SetBool("lessRedraws", true);
        pIExFilterConfig->Flt_SetBool("d3dFullscreenControl", true);
	}

    CComQIPtr<IBaseFilter> pBF = m_pMPCVR;
    CComPtr<IPin> pPin = GetFirstPin(pBF);
    m_bHookedNewSegment = HookNewSegment((IPinC*)(IPin*)pPin);

    return S_OK;
}

STDMETHODIMP_(void) CMPCVRAllocatorPresenter::SetPosition(RECT w, RECT v)
{
    if (CComQIPtr<IBasicVideo> pBV = m_pMPCVR) {
        pBV->SetDefaultSourcePosition();
        pBV->SetDestinationPosition(v.left, v.top, v.right - v.left, v.bottom - v.top);
    }

    if (CComQIPtr<IVideoWindow> pVW = m_pMPCVR) {
        pVW->SetWindowPosition(w.left, w.top, w.right - w.left, w.bottom - w.top);
    }

	__super::SetPosition(w, v);
}

// ISubPicAllocatorPresenter

STDMETHODIMP CMPCVRAllocatorPresenter::SetRotation(int rotation)
{
	if (AngleStep90(rotation)) {
		HRESULT hr = E_NOTIMPL;
		if (CComQIPtr<IExFilterConfig> pIExFilterConfig = m_pMPCVR) {
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

STDMETHODIMP_(int) CMPCVRAllocatorPresenter::GetRotation()
{
    if (CComQIPtr<IExFilterConfig> pIExFilterConfig = m_pMPCVR) {
        int rotation = 0;
        if (SUCCEEDED(pIExFilterConfig->Flt_GetInt("rotation", &rotation))) {
            return rotation;
        }
    }
    return 0;
}

STDMETHODIMP CMPCVRAllocatorPresenter::SetFlip(bool flip) {
    HRESULT hr = E_NOTIMPL;
    if (CComQIPtr<IExFilterConfig> pIExFilterConfig = m_pMPCVR) {
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

STDMETHODIMP_(bool) CMPCVRAllocatorPresenter::GetFlip() {
    if (CComQIPtr<IExFilterConfig> pIExFilterConfig = m_pMPCVR) {
        bool flip = false;
        if (SUCCEEDED(pIExFilterConfig->Flt_GetBool("flip", &flip))) {
            return flip;
        }
    }
    return false;
}

STDMETHODIMP_(SIZE) CMPCVRAllocatorPresenter::GetVideoSize(bool bCorrectAR) const
{
    SIZE size = {0, 0};

    if (!bCorrectAR) {
        __int64 ovs = 0;
        CComQIPtr<IExFilterConfig> pIExFilterConfig = m_pMPCVR;
        // MPCVR 0.10.8.2564 and newer
        static_assert(sizeof(__int64) == sizeof(size));
        if (pIExFilterConfig && SUCCEEDED(pIExFilterConfig->Flt_GetInt64("originalVideoSize", (__int64*)&size))) {
            return size;
        }
        // MPCVR 0.10.2.2540 and newer return an aspect ratio corrected size through
        // IBasicVideo::GetVideoSize (matching VMR-9/madVR behavior, needed by the
        // DVD Navigator), so read the coded frame size from the input connection.
        if (CComQIPtr<IBaseFilter> pBF = m_pMPCVR) {
            if (CComPtr<IPin> pPin = GetFirstPin(pBF, PINDIR_INPUT)) {
                CMediaType mt;
                if (SUCCEEDED(pPin->ConnectionMediaType(&mt)) && mt.pbFormat) {
                    RECT rcSource = {};
                    if (mt.formattype == FORMAT_VideoInfo2) {
                        const VIDEOINFOHEADER2* vih2 = (VIDEOINFOHEADER2*)mt.pbFormat;
                        rcSource = vih2->rcSource;
                    } else if (mt.formattype == FORMAT_VideoInfo) {
                        const VIDEOINFOHEADER* vih = (VIDEOINFOHEADER*)mt.pbFormat;
                        rcSource = vih->rcSource;
                    }
                    size.cx = rcSource.right - rcSource.left;
                    size.cy = rcSource.bottom - rcSource.top;
                }
            }
        }
        if (size.cx > 0 && size.cy > 0) {
            if (pIExFilterConfig) {
#if _DEBUG
                SIZE bv_size = {0, 0};
                if (CComQIPtr<IBasicVideo> pBV = m_pMPCVR) {
                    pBV->GetVideoSize(&bv_size.cx, &bv_size.cy);
                }
#endif
                int rotation = 0;
                if (SUCCEEDED(pIExFilterConfig->Flt_GetInt("rotation", &rotation)) && (rotation == 90 || rotation == 270)) {
                    std::swap(size.cx, size.cy);
                    ASSERT(size.cx >= bv_size.cx && size.cy == bv_size.cy);
                } else {
                    ASSERT(size.cx <= bv_size.cx && size.cy == bv_size.cy);
                }
            }
        } else {
            if (CComQIPtr<IBasicVideo> pBV = m_pMPCVR) {
                pBV->GetVideoSize(&size.cx, &size.cy);
            }
        }
    } else {
        if (CComQIPtr<IBasicVideo2> pBV2 = m_pMPCVR) {
            pBV2->GetPreferredAspectRatio(&size.cx, &size.cy);
        }
    }

    return size;
}

STDMETHODIMP_(bool) CMPCVRAllocatorPresenter::Paint(bool /*bAll*/)
{
    if (CComQIPtr<IExFilterConfig> pIExFilterConfig = m_pMPCVR) {
        return SUCCEEDED(pIExFilterConfig->Flt_SetBool("cmd_redraw", true));
    }
    return false;
}

STDMETHODIMP CMPCVRAllocatorPresenter::GetDIB(BYTE* lpDib, DWORD* size)
{
    HRESULT hr = E_NOTIMPL;
    if (CComQIPtr<IBasicVideo> pBV = m_pMPCVR) {
        hr = pBV->GetCurrentImage((long*)size, (long*)lpDib);
    }
    return hr;
}

STDMETHODIMP CMPCVRAllocatorPresenter::GetDisplayedImage(LPVOID* dibImage)
{
	if (CComQIPtr<IExFilterConfig> pIExFilterConfig = m_pMPCVR) {
		unsigned size = 0;
		HRESULT hr = pIExFilterConfig->Flt_GetBin("displayedImage", dibImage, &size);

		return hr;
	}

	return E_FAIL;
}

STDMETHODIMP_(int) CMPCVRAllocatorPresenter::GetPixelShaderMode()
{
	if (CComQIPtr<IExFilterConfig> pIExFilterConfig = m_pMPCVR) {
		int rtype = 0;
		if (S_OK == pIExFilterConfig->Flt_GetInt("renderType", &rtype)) {
			return rtype;
		}
	}
	return -1;
}

STDMETHODIMP CMPCVRAllocatorPresenter::ClearPixelShaders(int target)
{
	HRESULT hr = E_FAIL;

	if (CComQIPtr<IExFilterConfig> pIExFilterConfig = m_pMPCVR) {
        if (TARGET_SCREEN == target) {
            hr = pIExFilterConfig->Flt_SetBool("cmd_clearPostScaleShaders", true);
        } else {
            hr = pIExFilterConfig->Flt_SetBool("cmd_clearPreScaleShaders", true);
        }
	}
	return hr;
}

BYTE* WriteChunk(BYTE* dst, const uint32_t code, const int32_t size, BYTE* data)
{
	memcpy(dst, &code, 4);
	dst += 4;
	memcpy(dst, &size, 4);
	dst += 4;
	memcpy(dst, data, size);
	dst += size;

	return dst;
}

STDMETHODIMP CMPCVRAllocatorPresenter::AddPixelShader(int target, LPCWSTR name, LPCSTR profile, LPCSTR sourceCode)
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
		if (CComQIPtr<IExFilterConfig> pIExFilterConfig = m_pMPCVR) {
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

STDMETHODIMP_(bool) CMPCVRAllocatorPresenter::DisplayChange()
{
	if (CComQIPtr<IExFilterConfig> pIExFilterConfig = m_pMPCVR) {
		return SUCCEEDED(pIExFilterConfig->Flt_SetBool("displayChange", true));
	}

	return false;
}

STDMETHODIMP_(bool) CMPCVRAllocatorPresenter::IsRendering()
{
    if (CComQIPtr<IExFilterConfig> pIExFilterConfig = m_pMPCVR) {
        int playbackState;
        if (SUCCEEDED(pIExFilterConfig->Flt_GetInt("playbackState", &playbackState))) {
            return playbackState == State_Running;
        }
    }

    return false;
}

STDMETHODIMP CMPCVRAllocatorPresenter::SetPixelShader(LPCSTR pSrcData, LPCSTR pTarget)
{
    return E_NOTIMPL; // TODO
}

STDMETHODIMP_(bool) CMPCVRAllocatorPresenter::ToggleStats() {
    if (CComQIPtr<IExFilterConfig> pIExFilterConfig = m_pMPCVR) {
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
