/*
 * (C) 2003-2006 Gabest
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

#pragma once

#include <atlbase.h>
#include <atlcoll.h>
#include <condition_variable>
#include "ISubPic.h"
#include "CoordGeom.h"
#include "SubRenderIntf.h"
#include "ScreenUtil.h"

class CSubPicAllocatorPresenterImpl
    : public CUnknown
    , public CCritSec
	, public ISubPicAllocatorPresenter4
    , public ISubRenderConsumer2
{
private:
    CCritSec m_csSubPicProvider;

protected:
    HWND m_hWnd;
    REFERENCE_TIME m_rtSubtitleDelay;

    CSize m_maxSubtitleTextureSize;
    CSize m_curSubtitleTextureSize;
    CSize m_nativeVideoSize, m_aspectRatio;
    CRect m_videoRect, m_windowRect;
	bool  m_bOtherTransform = false;

	REFERENCE_TIME m_rtNow = 0;
	double m_fps           = 25.0;
	UINT m_refreshRate     = 0;

    CMediaType m_inputMediaType;

    CComPtr<ISubPicProvider> m_pSubPicProvider;
    CComPtr<ISubPicAllocator> m_pAllocator;
    CComPtr<ISubPicQueue> m_pSubPicQueue;

    // Secondary (second-subtitle) pipeline. Lazily built on the first non-null SetSubPicProvider2()
    // and torn down when cleared, so it costs nothing (no extra worker thread / static cache) when
    // unused. m_csSubPicQueue2 guards ONLY the load/store of the m_pSubPicQueue2 / m_pAllocator2
    // member pointers against the paint thread; heavy work (queue construction, worker join,
    // FreeStatic) must happen outside the lock (see CreateSecondaryPipeline/DestroySecondaryPipeline).
    CComPtr<ISubPicProvider> m_pSubPicProvider2;
    CComPtr<ISubPicAllocator> m_pAllocator2;
    CComPtr<ISubPicQueue> m_pSubPicQueue2;
    REFERENCE_TIME m_rtSecondaryDelay = 0;
    REFERENCE_TIME m_rtNow2 = 0;
    CCritSec m_csSubPicQueue2;

    bool m_bDeviceResetRequested;
    bool m_bPendingResetDevice;

    void InitMaxSubtitleTextureSize(int maxSizeX, int maxSizeY, CSize largestScreen);

    HRESULT AlphaBltSubPic(const CRect& windowRect,
                           const CRect& videoRect,
                           SubPicDesc* pTarget = nullptr,
                           const double videoStretchFactor = 1.0,
                           int xOffsetInPixels = 0, int yOffsetInPixels = 0);

    // Blends the secondary subtitle (from the second pipeline, using the m_rtNow2 shifted clock).
    // Does NOT apply the primary subPicVerticalShift hotkey offset; returns S_FALSE (not E_FAIL)
    // when no secondary pipeline / subpic is present so the frame is not failed.
    HRESULT AlphaBltSubPic2(const CRect& windowRect,
                            const CRect& videoRect,
                            SubPicDesc* pTarget = nullptr,
                            const double videoStretchFactor = 1.0,
                            int xOffsetInPixels = 0, int yOffsetInPixels = 0);

    // Secondary pipeline hook: supporting renderers override this to return a freshly created
    // device-specific allocator; the base returns nullptr, which disables the secondary pipeline.
    virtual CComPtr<ISubPicAllocator> CreateSecondaryAllocator() { return nullptr; }

    void CreateSecondaryPipeline();
    void DestroySecondaryPipeline();
    void ChangeSecondaryDevice(IUnknown* pDev);

    void UpdateXForm();
    HRESULT CreateDIBFromSurfaceData(D3DSURFACE_DESC desc, D3DLOCKED_RECT r, BYTE* lpDib) const;

    Vector m_defaultVideoAngle, m_videoAngle;
    bool m_bDefaultVideoAngleSwitchAR;
    XForm m_xform;
    void Transform(CRect r, Vector v[4]);

    bool m_bHookedNewSegment;
    bool m_bHookedReceive;

public:
    CSubPicAllocatorPresenterImpl(HWND hWnd, HRESULT& hr, CString* _pError);
    virtual ~CSubPicAllocatorPresenterImpl();

    DECLARE_IUNKNOWN;
    STDMETHODIMP NonDelegatingQueryInterface(REFIID riid, void** ppv);
    STDMETHODIMP_(void) SetVideoSize(CSize szVideo, CSize szAspectRatio = CSize(0, 0));

    // ISubPicAllocatorPresenter

    STDMETHODIMP CreateRenderer(IUnknown** ppRenderer) PURE;
    STDMETHODIMP_(SIZE) GetVideoSize(bool bCorrectAR) const;
    STDMETHODIMP_(void) SetPosition(RECT w, RECT v);
    STDMETHODIMP_(bool) Paint(bool bAll) PURE;
    STDMETHODIMP_(void) SetTime(REFERENCE_TIME rtNow);
    STDMETHODIMP_(void) SetSubtitleDelay(int delayMs);
    STDMETHODIMP_(int) GetSubtitleDelay() const;
    STDMETHODIMP_(double) GetFPS() const;
    STDMETHODIMP_(void) SetSubPicProvider(ISubPicProvider* pSubPicProvider);
    STDMETHODIMP_(void) Invalidate(REFERENCE_TIME rtInvalidate = -1);
    STDMETHODIMP GetDIB(BYTE* lpDib, DWORD* size) { return E_NOTIMPL; }
    STDMETHODIMP GetDisplayedImage(LPVOID* dibImage) { return E_NOTIMPL; }
    STDMETHODIMP SetVideoAngle(Vector v);
    STDMETHODIMP SetPixelShader(LPCSTR pSrcData, LPCSTR pTarget) { return E_NOTIMPL; }
    STDMETHODIMP_(bool) ResetDevice() { return false; }
    STDMETHODIMP_(bool) DisplayChange() { return false; }
    STDMETHODIMP_(void) GetPosition(RECT* windowRect, RECT* videoRect) { *windowRect = m_windowRect; *videoRect = m_videoRect; }
    STDMETHODIMP_(void) SetVideoMediaType(CMediaType input) { m_inputMediaType = input; }

    // ISubPicAllocatorPresenter2

    STDMETHODIMP SetPixelShader2(LPCSTR pSrcData, LPCSTR pTarget, bool bScreenSpace) {
        if (!bScreenSpace) {
            return SetPixelShader(pSrcData, pTarget);
        }
        return E_NOTIMPL;
    }

    STDMETHODIMP_(SIZE) GetVisibleVideoSize() const {
        return m_nativeVideoSize;
    }

    STDMETHODIMP SetIsRendering(bool bIsRendering) { return E_NOTIMPL; }
    STDMETHODIMP_(bool) IsRendering() { return true; }
    STDMETHODIMP SetDefaultVideoAngle(Vector v);

    // ISubPicAllocatorPresenter3

	STDMETHODIMP SetRotation(int rotation) { return E_NOTIMPL; }
	STDMETHODIMP_(int) GetRotation() { return 0; }
	STDMETHODIMP SetFlip(bool flip) { return E_NOTIMPL; }
	STDMETHODIMP_(bool) GetFlip() { return false; }
    STDMETHODIMP GetVideoFrame(BYTE* lpDib, DWORD* size) { return E_NOTIMPL; }
    STDMETHODIMP_(int) GetPixelShaderMode() { return 0; }
    STDMETHODIMP ClearPixelShaders(int target) { return E_NOTIMPL; }
    STDMETHODIMP AddPixelShader(int target, LPCWSTR name, LPCSTR profile, LPCSTR sourceCode) { return E_NOTIMPL; }
    STDMETHODIMP_(bool) ResizeDevice() { return false; }
    STDMETHODIMP_(bool) ToggleStats() { return false; }

    // ISubPicAllocatorPresenter4

    STDMETHODIMP_(void) SetSubPicProvider2(ISubPicProvider* pSubPicProvider2);
    STDMETHODIMP_(void) SetSecondaryDelay(int delayMs);
    STDMETHODIMP_(int) GetSecondaryDelay() const;
    STDMETHODIMP_(void) InvalidateSubtitle2(REFERENCE_TIME rtInvalidate = -1);
    STDMETHODIMP_(bool) SupportsDualSubtitles() { return false; }

    // ISubRenderOptions

    STDMETHODIMP GetBool(LPCSTR field, bool* value);
    STDMETHODIMP GetInt(LPCSTR field, int* value);
    STDMETHODIMP GetSize(LPCSTR field, SIZE* value);
    STDMETHODIMP GetRect(LPCSTR field, RECT* value);
    STDMETHODIMP GetUlonglong(LPCSTR field, ULONGLONG* value);
    STDMETHODIMP GetDouble(LPCSTR field, double* value);
    STDMETHODIMP GetString(LPCSTR field, LPWSTR* value, int* chars);
    STDMETHODIMP GetBin(LPCSTR field, LPVOID* value, int* size);
    STDMETHODIMP SetBool(LPCSTR field, bool value);
    STDMETHODIMP SetInt(LPCSTR field, int value);
    STDMETHODIMP SetSize(LPCSTR field, SIZE value);
    STDMETHODIMP SetRect(LPCSTR field, RECT value);
    STDMETHODIMP SetUlonglong(LPCSTR field, ULONGLONG value);
    STDMETHODIMP SetDouble(LPCSTR field, double value);
    STDMETHODIMP SetString(LPCSTR field, LPWSTR value, int chars);
    STDMETHODIMP SetBin(LPCSTR field, LPVOID value, int size);

    // ISubRenderConsumer

    STDMETHODIMP GetMerit(ULONG* plMerit) {
        CheckPointer(plMerit, E_POINTER);
        *plMerit = 4 << 16;
        return S_OK;
    }
    STDMETHODIMP Connect(ISubRenderProvider* subtitleRenderer);
    STDMETHODIMP Disconnect();
    STDMETHODIMP DeliverFrame(REFERENCE_TIME start, REFERENCE_TIME stop, LPVOID context, ISubRenderFrame* subtitleFrame);

    // ISubRenderConsumer2

    STDMETHODIMP Clear(REFERENCE_TIME clearNewerThan = 0);
};
