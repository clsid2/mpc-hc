/*
 * (C) 2003-2006 Gabest
 * (C) 2006-2017 see Authors.txt
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
#include "SubPicAllocatorPresenterImpl.h"
#include "../DSUtil/DSUtil.h"
#include "../filters/renderer/VideoRenderers/RenderersSettings.h"
#include "../mpc-hc/VersionInfo.h"
#include <cmath>
#include "XySubPicQueueImpl.h"
#include "XySubPicProvider.h"
#include <d3d9.h>
#include <evr.h>
#include <dxva2api.h>
#include <functional>
#include "ScreenUtil.h"

#include "mpc-hc_config.h"

#if !TRACE_SUBTITLES
#define TRACE(...)
#endif

CSubPicAllocatorPresenterImpl::CSubPicAllocatorPresenterImpl(HWND hWnd, HRESULT& hr, CString* _pError)
    : CUnknown(NAME("CSubPicAllocatorPresenterImpl"), nullptr)
    , m_hWnd(hWnd)
    , m_rtSubtitleDelay(0)
    , m_maxSubtitleTextureSize(0, 0)
    , m_nativeVideoSize(0, 0)
    , m_aspectRatio(0, 0)
    , m_videoRect(0, 0, 0, 0)
    , m_windowRect(0, 0, 0, 0)
    , m_rtNow(0)
    , m_fps(25.0)
    , m_refreshRate(0)
    , m_bDeviceResetRequested(false)
    , m_bPendingResetDevice(false)
    , m_bDefaultVideoAngleSwitchAR(false)
    , m_bHookedNewSegment(false)
    , m_bHookedReceive(false)
{
    if (!IsWindow(m_hWnd)) {
        hr = E_INVALIDARG;
        if (_pError) {
            *_pError += _T("Invalid window handle in ISubPicAllocatorPresenterImpl\n");
        }
        return;
    }
    GetWindowRect(m_hWnd, &m_windowRect);
    hr = S_OK;
}

CSubPicAllocatorPresenterImpl::~CSubPicAllocatorPresenterImpl()
{
}

STDMETHODIMP CSubPicAllocatorPresenterImpl::NonDelegatingQueryInterface(REFIID riid, void** ppv)
{

    return
        QI(ISubPicAllocatorPresenter)
        QI(ISubPicAllocatorPresenter2)
        QI(ISubRenderOptions)
        QI(ISubRenderConsumer)
        QI(ISubRenderConsumer2)
        __super::NonDelegatingQueryInterface(riid, ppv);
}

void CSubPicAllocatorPresenterImpl::InitMaxSubtitleTextureSize(int maxSizeX, int maxSizeY, CSize largestScreen)
{
    if (maxSizeX < 384 || maxSizeY < 288) {
        m_maxSubtitleTextureSize = largestScreen;
    } else {
        if (maxSizeX * maxSizeY > largestScreen.cx * largestScreen.cy) {
            m_maxSubtitleTextureSize = largestScreen;
        } else {
            m_maxSubtitleTextureSize.cx = maxSizeX;
            m_maxSubtitleTextureSize.cy = maxSizeY;
        }
    }
#if DEBUG_OVERRIDE_TEXTURE_SIZE
    m_maxSubtitleTextureSize = CSize(DEBUG_OVERRIDE_TEXTURE_SIZE_WIDTH, DEBUG_OVERRIDE_TEXTURE_SIZE_HEIGHT);
#endif
    m_curSubtitleTextureSize = m_maxSubtitleTextureSize;
    TRACE(_T("CSubPicAllocatorPresenterImpl::InitMaxSubtitleTextureSize %dx%d\n"), m_maxSubtitleTextureSize.cx, m_maxSubtitleTextureSize.cy);
}

HRESULT CSubPicAllocatorPresenterImpl::AlphaBltSubPic(const CRect& windowRect,
                                                   const CRect& videoRect,
                                                   SubPicDesc* pTarget /*= nullptr*/,
                                                   const double videoStretchFactor /*= 1.0*/,
                                                   int xOffsetInPixels /*= 0*/, int yOffsetInPixels /*= 0*/)
{
    CComPtr<ISubPic> pSubPic;
    if (m_pSubPicQueue->LookupSubPic(m_rtNow, !IsRendering(), pSubPic)) {
        CRect rcSource, rcDest;

        const CRenderersSettings& r = GetRenderersSettings();
        int yOffset = yOffsetInPixels + r.subPicVerticalShift;
        if (SUCCEEDED(pSubPic->GetSourceAndDest(windowRect, videoRect, rcSource, rcDest,
                                                videoStretchFactor, xOffsetInPixels, yOffset))) {
            return pSubPic->AlphaBlt(rcSource, rcDest, pTarget);
        }
    }

    return E_FAIL;
}

// ISubPicAllocatorPresenter

STDMETHODIMP_(void) CSubPicAllocatorPresenterImpl::SetVideoSize(CSize szVideo, CSize szAspectRatio /* = CSize(0, 0) */)
{
    if (szVideo.cx == 0 || szVideo.cy == 0) {
        return;
    }

    m_nativeVideoSize = szVideo;
    m_aspectRatio = szAspectRatio;
}

STDMETHODIMP_(SIZE) CSubPicAllocatorPresenterImpl::GetVideoSize(bool bCorrectAR) const
{
    CSize videoSize(GetVisibleVideoSize());

    if (bCorrectAR && m_aspectRatio.cx > 0 && m_aspectRatio.cy > 0) {
        videoSize.cx = (LONGLONG(videoSize.cy) * LONGLONG(m_aspectRatio.cx)) / LONGLONG(m_aspectRatio.cy);
    }

    if (m_bDefaultVideoAngleSwitchAR) {
        std::swap(videoSize.cx, videoSize.cy);
    }

    return videoSize;
}

STDMETHODIMP_(void) CSubPicAllocatorPresenterImpl::SetPosition(RECT w, RECT v)
{
    bool bWindowPosChanged = !!(m_windowRect != w);
    bool bWindowSizeChanged = !!(m_windowRect.Size() != CRect(w).Size());
    bool bVideoRectChanged = !!(m_videoRect != v);

    m_windowRect = w;
    m_videoRect = v;

    if (m_pAllocator && (bWindowPosChanged || bWindowSizeChanged || bVideoRectChanged)) {
        if (m_windowRect.Width() != m_curSubtitleTextureSize.cx || m_windowRect.Height() != m_curSubtitleTextureSize.cy) {
            int maxpixels = m_maxSubtitleTextureSize.cx * m_maxSubtitleTextureSize.cy;
            if (m_windowRect.Width() * m_windowRect.Height() <= maxpixels) {
                // use window size
                m_curSubtitleTextureSize = CSize(m_windowRect.Width(), m_windowRect.Height());
            } else {
                bool correct_ar = false;
                if (m_maxSubtitleTextureSize.cx == 2560 && m_windowRect.Width() >= 3800 && m_windowRect.Width() <= 4096) { // not 3840, to handle a maximized window as well
                    m_curSubtitleTextureSize = CSize(m_windowRect.Width() / 2, m_windowRect.Height() / 2);
                } else {
                    m_curSubtitleTextureSize = CSize(m_maxSubtitleTextureSize.cx, m_maxSubtitleTextureSize.cy);
                    correct_ar = true;
                }

                if (correct_ar) {
                    double new_w = sqrt((uint64_t)m_curSubtitleTextureSize.cx * m_curSubtitleTextureSize.cy * m_windowRect.Width()  / m_windowRect.Height());
                    double new_h = sqrt((uint64_t)m_curSubtitleTextureSize.cx * m_curSubtitleTextureSize.cy * m_windowRect.Height() / m_windowRect.Width());
                    m_curSubtitleTextureSize.cx = lround(new_w);
                    m_curSubtitleTextureSize.cy = lround(new_h);
                }
            }
        }

        m_pAllocator->SetMaxTextureSize(m_curSubtitleTextureSize);
        m_pAllocator->SetCurSize(m_windowRect.Size());
        m_pAllocator->SetCurVidRect(m_videoRect);

        if (m_pSubPicQueue) {
            m_pSubPicQueue->Invalidate();
        }
    }

	if (bWindowPosChanged || bVideoRectChanged || m_bOtherTransform) {
        Paint(false);
		m_bOtherTransform = false;
    }
}

STDMETHODIMP_(void) CSubPicAllocatorPresenterImpl::SetTime(REFERENCE_TIME rtNow)
{
    m_rtNow = rtNow - m_rtSubtitleDelay;

    if (m_pSubPicQueue) {
        m_pSubPicQueue->SetTime(m_rtNow);
    }
}

STDMETHODIMP_(void) CSubPicAllocatorPresenterImpl::SetSubtitleDelay(int delayMs)
{
    REFERENCE_TIME delay = MILLISECONDS_TO_100NS_UNITS(delayMs);
    if (m_rtSubtitleDelay != delay) {
        REFERENCE_TIME oldDelay = m_rtSubtitleDelay;
        m_rtSubtitleDelay = delay;
        SetTime(m_rtNow + oldDelay);
        Paint(false);
    }
}

STDMETHODIMP_(int) CSubPicAllocatorPresenterImpl::GetSubtitleDelay() const
{
    return (int)(m_rtSubtitleDelay / 10000);
}

STDMETHODIMP_(double) CSubPicAllocatorPresenterImpl::GetFPS() const
{
    return m_fps;
}

STDMETHODIMP_(void) CSubPicAllocatorPresenterImpl::SetSubPicProvider(ISubPicProvider* pSubPicProvider)
{
    CAutoLock cAutoLock(&m_csSubPicProvider);

    m_pSubPicProvider = pSubPicProvider;

    if (m_pSubPicQueue) {
        m_pSubPicQueue->SetSubPicProvider(pSubPicProvider);
    }

    if (m_pAllocator) {
        m_pAllocator->SetMaxTextureSize(m_curSubtitleTextureSize);
        m_pAllocator->SetCurSize(m_windowRect.Size());
        m_pAllocator->SetCurVidRect(m_videoRect);
    }

    Paint(false);
}

STDMETHODIMP_(void) CSubPicAllocatorPresenterImpl::Invalidate(REFERENCE_TIME rtInvalidate)
{
    if (m_pSubPicQueue) {
        m_pSubPicQueue->Invalidate(rtInvalidate);
    }
}

void CSubPicAllocatorPresenterImpl::Transform(CRect r, Vector v[4])
{
    v[0] = Vector((float)r.left, (float)r.top, 0);
    v[1] = Vector((float)r.right, (float)r.top, 0);
    v[2] = Vector((float)r.left, (float)r.bottom, 0);
    v[3] = Vector((float)r.right, (float)r.bottom, 0);

    Vector center((float)r.CenterPoint().x, (float)r.CenterPoint().y, 0);
    int l = (int)(Vector((float)r.Size().cx, (float)r.Size().cy, 0).Length() * 1.5f) + 1;

    for (size_t i = 0; i < 4; i++) {
        v[i] = m_xform << (v[i] - center);
        v[i].z = v[i].z / l + 0.5f;
        v[i].x /= v[i].z * 2;
        v[i].y /= v[i].z * 2;
        v[i] += center;
    }
}

STDMETHODIMP CSubPicAllocatorPresenterImpl::SetDefaultVideoAngle(Vector v)
{
    if (m_defaultVideoAngle != v) {
        constexpr float pi_2 = float(M_PI_2);

        // In theory it should be a multiple of 90°
        int zAnglePi2 = std::lround(v.z / pi_2);
        //ASSERT(zAnglePi2 * pi_2 == v.z);

        // Normalize the Z angle
        zAnglePi2 %= 4;
        if (zAnglePi2 < 0) {
            zAnglePi2 += 4;
        }
        v.z = zAnglePi2 * pi_2;

        // Check if the default rotation change the AR
        m_bDefaultVideoAngleSwitchAR = (v.z == pi_2 || v.z == 3.0f * pi_2);

        m_defaultVideoAngle = v;
        UpdateXForm();
        return S_OK;
    }
    return S_FALSE;
}

STDMETHODIMP CSubPicAllocatorPresenterImpl::SetVideoAngle(Vector v)
{
    if (m_videoAngle != v) {
        m_videoAngle = v;
        UpdateXForm();
        return S_OK;
    }
    return S_FALSE;
}

void CSubPicAllocatorPresenterImpl::UpdateXForm()
{
    Vector v = m_defaultVideoAngle + m_videoAngle;

    auto normalizeAngle = [](float & rad) {
        constexpr float twoPi = float(2.0 * M_PI);

        while (rad < 0.0f) {
            rad += twoPi;
        }
        while (rad > twoPi) {
            rad -= twoPi;
        }
    };

    normalizeAngle(v.x);
    normalizeAngle(v.y);
    normalizeAngle(v.z);

    CSize AR = GetVideoSize(true);
    float fARCorrection = m_bDefaultVideoAngleSwitchAR ? float(AR.cx) / AR.cy : 1.0f;

    m_xform = XForm(Ray(Vector(), v), Vector(1.0f / fARCorrection, fARCorrection, 1.0f), false);

    Paint(false);
}

HRESULT CSubPicAllocatorPresenterImpl::CreateDIBFromSurfaceData(D3DSURFACE_DESC desc, D3DLOCKED_RECT r, BYTE* lpDib) const
{
    bool bNeedRotation = !IsEqual(m_defaultVideoAngle.z, 0.0f);

    BITMAPINFOHEADER* bih = (BITMAPINFOHEADER*)lpDib;
    ZeroMemory(bih, sizeof(BITMAPINFOHEADER));
    bih->biSize = sizeof(BITMAPINFOHEADER);
    bih->biWidth = desc.Width;
    bih->biHeight = desc.Height;
    bih->biBitCount = 32;
    bih->biPlanes = 1;
    bih->biSizeImage = bih->biWidth * bih->biHeight * bih->biBitCount >> 3;

    UINT* pBits;
    if (bNeedRotation) {
        pBits = (UINT*)malloc(bih->biSizeImage);
        if (!pBits) {
            return E_OUTOFMEMORY;
        }
    } else {
        pBits = (UINT*)(bih + 1);
    }

    BitBltFromRGBToRGB(bih->biWidth, bih->biHeight,
                       (BYTE*)pBits, bih->biWidth * bih->biBitCount >> 3, bih->biBitCount,
                       (BYTE*)r.pBits + r.Pitch * (desc.Height - 1), -r.Pitch, 32);

    if (bNeedRotation) {
        constexpr float pi_2 = float(M_PI_2);

        if (m_bDefaultVideoAngleSwitchAR) {
            std::swap(bih->biWidth, bih->biHeight);
        }

        std::function<size_t(UINT, UINT)> convCoordinates;
        if (IsEqual(m_defaultVideoAngle.z, pi_2)) {
            convCoordinates = [desc](UINT x, UINT y) {
                return x * desc.Height + desc.Height - 1 - y;
            };
        } else if (IsEqual(m_defaultVideoAngle.z, 2 * pi_2)) {
            convCoordinates = [desc](UINT x, UINT y) {
                return desc.Width - 1 - x + (desc.Height - 1 - y) * desc.Width;
            };
        } else {
            ASSERT(IsEqual(m_defaultVideoAngle.z, 3 * pi_2));
            convCoordinates = [desc](UINT x, UINT y) {
                return (desc.Width - 1 - x) * desc.Height + y;
            };
        }

        UINT* pBitsDest = (UINT*)(bih + 1);
        for (UINT x = 0; x < desc.Width; x++) {
            for (UINT y = 0; y < desc.Height; y++) {
                pBitsDest[convCoordinates(x, y)] = pBits[x + y * desc.Width];
            }
        }

        free(pBits);
    }

    return S_OK;
}

// ISubRenderOptions

STDMETHODIMP CSubPicAllocatorPresenterImpl::GetBool(LPCSTR field, bool* value)
{
    CheckPointer(value, E_POINTER);
    return E_INVALIDARG;
}

STDMETHODIMP CSubPicAllocatorPresenterImpl::GetInt(LPCSTR field, int* value)
{
    CheckPointer(value, E_POINTER);
    if (!strcmp(field, "supportedLevels")) {
        if (CComQIPtr<IMFVideoPresenter> pEVR = (ISubPicAllocatorPresenter*)this) {
            const CRenderersSettings& r = GetRenderersSettings();
            if (r.m_AdvRendSets.iEVROutputRange == 1) {
                *value = 3; // TV preferred
            } else {
                *value = 2; // PC preferred
            }
        } else {
            *value = 0; // PC only
        }
        return S_OK;
    }

    return E_INVALIDARG;
}

STDMETHODIMP CSubPicAllocatorPresenterImpl::GetSize(LPCSTR field, SIZE* value)
{
    CheckPointer(value, E_POINTER);
    if (!strcmp(field, "originalVideoSize")) {
        *value = GetVideoSize(false);
        return S_OK;
    } else if (!strcmp(field, "arAdjustedVideoSize")) {
        *value = GetVideoSize(true);
        return S_OK;
    }

    return E_INVALIDARG;
}

STDMETHODIMP CSubPicAllocatorPresenterImpl::GetRect(LPCSTR field, RECT* value)
{
    CheckPointer(value, E_POINTER);
    if (!strcmp(field, "videoOutputRect") || !strcmp(field, "subtitleTargetRect")) {
        if (m_videoRect.IsRectEmpty()) {
            *value = m_windowRect;
        } else {
            value->left = 0;
            value->top = 0;
            value->right = m_videoRect.Width();
            value->bottom = m_videoRect.Height();
        }
        return S_OK;
    }

    return E_INVALIDARG;
}

STDMETHODIMP CSubPicAllocatorPresenterImpl::GetUlonglong(LPCSTR field, ULONGLONG* value)
{
    CheckPointer(value, E_POINTER);
    if (!strcmp(field, "frameRate")) {
        *value = (REFERENCE_TIME)(10000000.0 / m_fps);
        return S_OK;
    }

    return E_INVALIDARG;
}

STDMETHODIMP CSubPicAllocatorPresenterImpl::GetDouble(LPCSTR field, double* value)
{
    CheckPointer(value, E_POINTER);
    if (!strcmp(field, "refreshRate")) {
        *value = 1000.0 / m_refreshRate;
        return S_OK;
    }

    return E_INVALIDARG;
}

STDMETHODIMP CSubPicAllocatorPresenterImpl::GetString(LPCSTR field, LPWSTR* value, int* chars)
{
    CheckPointer(value, E_POINTER);
    CheckPointer(chars, E_POINTER);
    CStringW ret = nullptr;

    if (!strcmp(field, "name")) {
        ret = L"MPC-HC";
    } else if (!strcmp(field, "version")) {
        ret = VersionInfo::GetVersionString();
    } else if (!strcmp(field, "yuvMatrix")) {
        ret = L"None";

        if (m_inputMediaType.IsValid() && m_inputMediaType.formattype == FORMAT_VideoInfo2) {
            VIDEOINFOHEADER2* pVIH2 = (VIDEOINFOHEADER2*)m_inputMediaType.pbFormat;

            if (pVIH2->dwControlFlags & AMCONTROL_COLORINFO_PRESENT) {
                DXVA2_ExtendedFormat& flags = (DXVA2_ExtendedFormat&)pVIH2->dwControlFlags;

                ret = (flags.NominalRange == DXVA2_NominalRange_Normal) ? L"PC." : L"TV.";

                switch (flags.VideoTransferMatrix) {
                    case DXVA2_VideoTransferMatrix_BT601:
                        ret.Append(L"601");
                        break;
                    case DXVA2_VideoTransferMatrix_BT709:
                        ret.Append(L"709");
                        break;
                    case DXVA2_VideoTransferMatrix_SMPTE240M:
                        ret.Append(L"240M");
                        break;
                    case 4:
                        ret.Append(L"2020");
                        break;
                    default:
                        ret = L"None";
                        break;
                }
            }
        }
    }

    if (!ret.IsEmpty()) {
        int len = ret.GetLength();
        size_t sz = (len + 1) * sizeof(WCHAR);
        LPWSTR buf = (LPWSTR)LocalAlloc(LPTR, sz);

        if (!buf) {
            return E_OUTOFMEMORY;
        }

        wcscpy_s(buf, len + 1, ret);
        *chars = len;
        *value = buf;

        return S_OK;
    }

    return E_INVALIDARG;
}

STDMETHODIMP CSubPicAllocatorPresenterImpl::GetBin(LPCSTR field, LPVOID* value, int* size)
{
    CheckPointer(value, E_POINTER);
    CheckPointer(size, E_POINTER);
    return E_INVALIDARG;
}

STDMETHODIMP CSubPicAllocatorPresenterImpl::SetBool(LPCSTR field, bool value)
{
    return E_INVALIDARG;
}

STDMETHODIMP CSubPicAllocatorPresenterImpl::SetInt(LPCSTR field, int value)
{
    return E_INVALIDARG;
}

STDMETHODIMP CSubPicAllocatorPresenterImpl::SetSize(LPCSTR field, SIZE value)
{
    return E_INVALIDARG;
}

STDMETHODIMP CSubPicAllocatorPresenterImpl::SetRect(LPCSTR field, RECT value)
{
    return E_INVALIDARG;
}

STDMETHODIMP CSubPicAllocatorPresenterImpl::SetUlonglong(LPCSTR field, ULONGLONG value)
{
    return E_INVALIDARG;
}

STDMETHODIMP CSubPicAllocatorPresenterImpl::SetDouble(LPCSTR field, double value)
{
    return E_INVALIDARG;
}

STDMETHODIMP CSubPicAllocatorPresenterImpl::SetString(LPCSTR field, LPWSTR value, int chars)
{
    return E_INVALIDARG;
}

STDMETHODIMP CSubPicAllocatorPresenterImpl::SetBin(LPCSTR field, LPVOID value, int size)
{
    return E_INVALIDARG;
}

// ISubRenderConsumer

STDMETHODIMP CSubPicAllocatorPresenterImpl::Connect(ISubRenderProvider* subtitleRenderer)
{
    HRESULT hr = E_FAIL;

    if (m_pSubPicProvider) {
        return hr;
    }

    hr = subtitleRenderer->SetBool("combineBitmaps", true);
    if (FAILED(hr)) {
        return hr;
    }

    if (CComQIPtr<ISubRenderConsumer> pSubConsumer = m_pSubPicQueue) {
        hr = pSubConsumer->Connect(subtitleRenderer);
    } else {
        CComPtr<ISubPicProvider> pSubPicProvider = (ISubPicProvider*)DEBUG_NEW CXySubPicProvider(subtitleRenderer);
        CComPtr<ISubPicQueue> pSubPicQueue = (ISubPicQueue*)DEBUG_NEW CXySubPicQueueNoThread(m_pAllocator, &hr);

        if (SUCCEEDED(hr)) {
            pSubPicQueue->SetSubPicProvider(pSubPicProvider);
            m_pSubPicProvider = pSubPicProvider;
            m_pSubPicQueue = pSubPicQueue;
            m_pAllocator->SetInverseAlpha(true);
        }
    }

    return hr;
}

STDMETHODIMP CSubPicAllocatorPresenterImpl::Disconnect()
{
    m_pSubPicProvider = nullptr;
    return m_pSubPicQueue->SetSubPicProvider(m_pSubPicProvider);
}

STDMETHODIMP CSubPicAllocatorPresenterImpl::DeliverFrame(REFERENCE_TIME start, REFERENCE_TIME stop, LPVOID context, ISubRenderFrame* subtitleFrame)
{
    HRESULT hr = E_FAIL;

    if (CComQIPtr<IXyCompatProvider> pXyProvider = m_pSubPicProvider) {
        hr = pXyProvider->DeliverFrame(start, stop, context, subtitleFrame);
    }

    return hr;
}

// ISubRenderConsumer2

STDMETHODIMP CSubPicAllocatorPresenterImpl::Clear(REFERENCE_TIME clearNewerThan /* = 0 */)
{
    return m_pSubPicQueue->Invalidate(clearNewerThan);
}
