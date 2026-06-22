/*
 * (C) 2003-2006 Gabest
 * (C) 2006-2015, 2017 see Authors.txt
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

#include "DX9RenderingEngine.h"
#include <evr9.h>

#define VMRBITMAP_UPDATE    0x80000000
#define NB_JITTER           126

class CFocusThread;

namespace DSObjects
{

    class CDX9AllocatorPresenter
        : public CDX9RenderingEngine
        , public ID3DFullscreenControl
    {
    public:
        CCritSec m_VMR9AlphaBitmapLock;
        void     UpdateAlphaBitmap();
    protected:
        bool    m_bAlternativeVSync;
        bool    m_bCompositionEnabled;
        bool    m_bIsEVR;
        int     m_OrderedPaint;
        int     m_VSyncMode;
        bool    m_bDesktopCompositionDisabled;
        bool    m_bIsFullscreen, fullScreenChanged;
        bool    m_bNeedCheckSample;
        DWORD   m_MainThreadId;
        bool    m_bIsPreview;
        bool    m_bIsRendering;

        CRenderersSettings::CAdvRendererSettings m_LastRendererSettings;

        HMODULE m_hDWMAPI;

        HRESULT(__stdcall* m_pDwmIsCompositionEnabled)(__out BOOL* pfEnabled);
        HRESULT(__stdcall* m_pDwmEnableComposition)(UINT uCompositionAction);

        CCritSec m_RenderLock;
        CComPtr<IDirectDraw> m_pDirectDraw;

        HANDLE LockD3DDevice() {
            if (m_pD3DDev) {
                _RTL_CRITICAL_SECTION* pCritSec = (_RTL_CRITICAL_SECTION*)((size_t)m_pD3DDev.p + sizeof(size_t));

                if (!IsBadReadPtr(pCritSec, sizeof(*pCritSec)) && !IsBadWritePtr(pCritSec, sizeof(*pCritSec))
                        && !IsBadReadPtr(pCritSec->DebugInfo, sizeof(*(pCritSec->DebugInfo))) && !IsBadWritePtr(pCritSec->DebugInfo, sizeof(*(pCritSec->DebugInfo)))) {
                    if (pCritSec->DebugInfo->CriticalSection == pCritSec) {
                        EnterCriticalSection(pCritSec);
                        return pCritSec->OwningThread;
                    }
                }
            }
            return 0;
        }

        void UnlockD3DDevice(HANDLE& lockOwner) {
            if (m_pD3DDev) {
                _RTL_CRITICAL_SECTION* pCritSec = (_RTL_CRITICAL_SECTION*)((size_t)m_pD3DDev.p + sizeof(size_t));

                if (!IsBadReadPtr(pCritSec, sizeof(*pCritSec)) && !IsBadWritePtr(pCritSec, sizeof(*pCritSec))
                        && !IsBadReadPtr(pCritSec->DebugInfo, sizeof(*(pCritSec->DebugInfo))) && !IsBadWritePtr(pCritSec->DebugInfo, sizeof(*(pCritSec->DebugInfo)))) {
                    if (pCritSec->DebugInfo->CriticalSection == pCritSec && pCritSec->OwningThread == lockOwner) {
                        LeaveCriticalSection(pCritSec);
                    }
                }
            }
        }

        CString m_D3DDevExError;
        CComPtr<IDirect3DTexture9>      m_pOSDTexture;
        CComPtr<IDirect3DSurface9>      m_pOSDSurface;
        CComPtr<ID3DXLine>              m_pLine;
        CComPtr<ID3DXFont>              m_pFont;
        bool                            m_bAlphaBitmapEnable = false;
        CComPtr<IDirect3DTexture9>      m_pAlphaBitmapTexture;
        MFVideoAlphaBitmapParams        m_AlphaBitmapParams = {};

        CComPtr<ID3DXSprite>            m_pSprite;

        bool SettingsNeedResetDevice(CRenderersSettings& r);

        virtual HRESULT CreateDevice(CString& _Error);
        virtual HRESULT AllocSurfaces();
        virtual void DeleteSurfaces();

        LONGLONG m_LastAdapterCheck;
        UINT GetAdapter(IDirect3D9* pD3D, bool bGetAdapter = true);
        DWORD GetVertexProcessing();

        bool GetVBlank(int& _ScanLine, int& _bInVBlank, bool _bMeasureTime);
        bool WaitForVBlankRange(int& _RasterStart, int _RasterEnd, bool _bWaitIfInside, bool _bNeedAccurate, bool _bMeasure, HANDLE& lockOwner);
        bool WaitForVBlank(bool& _Waited, HANDLE& lockOwner);
        int GetVBlackPos();
        void CalculateJitter(LONGLONG PerformanceCounter);
        virtual void OnVBlankFinished(bool bAll, LONGLONG PerformanceCounter) {}

        // Casimir666
        typedef HRESULT(WINAPI* D3DXLoadSurfaceFromMemoryPtr)(
            LPDIRECT3DSURFACE9  pDestSurface,
            CONST PALETTEENTRY* pDestPalette,
            CONST RECT*     pDestRect,
            LPCVOID         pSrcMemory,
            D3DFORMAT       SrcFormat,
            UINT            SrcPitch,
            CONST PALETTEENTRY* pSrcPalette,
            CONST RECT*     pSrcRect,
            DWORD           Filter,
            D3DCOLOR        ColorKey);

        typedef HRESULT(WINAPI* D3DXLoadSurfaceFromSurfacePtr)(
            LPDIRECT3DSURFACE9        pDestSurface,
            CONST PALETTEENTRY*       pDestPalette,
            CONST RECT*               pDestRect,
            LPDIRECT3DSURFACE9        pSrcSurface,
            CONST PALETTEENTRY*       pSrcPalette,
            CONST RECT*               pSrcRect,
            DWORD                     Filter,
            D3DCOLOR                  ColorKey);

        typedef HRESULT(WINAPI* D3DXCreateLinePtr)(LPDIRECT3DDEVICE9   pDevice, LPD3DXLINE* ppLine);

        typedef HRESULT(WINAPI* D3DXCreateFontPtr)(
            LPDIRECT3DDEVICE9   pDevice,
            int             Height,
            UINT            Width,
            UINT            Weight,
            UINT            MipLevels,
            bool            Italic,
            DWORD           CharSet,
            DWORD           OutputPrecision,
            DWORD           Quality,
            DWORD           PitchAndFamily,
            LPCWSTR         pFaceName,
            LPD3DXFONT*     ppFont);

        void                InitStats();
        void                ResetStats();
        void                DrawStats();
        virtual void        OnResetDevice() {};
        void                SendResetRequest();

        double GetFrameTime() const;
        double GetFrameRate() const;


        int                     m_nTearingPos;
        VMR9AlphaBitmap         m_VMR9AlphaBitmap;
        CAutoVectorPtr<BYTE>    m_VMR9AlphaBitmapData;
        CRect                   m_VMR9AlphaBitmapRect;
        int                     m_VMR9AlphaBitmapWidthBytes;

        D3DXLoadSurfaceFromMemoryPtr    m_pD3DXLoadSurfaceFromMemory;
        D3DXLoadSurfaceFromSurfacePtr   m_pD3DXLoadSurfaceFromSurface;
        D3DXCreateLinePtr               m_pD3DXCreateLine;
        D3DXCreateFontPtr               m_pD3DXCreateFont;
        HRESULT(__stdcall* m_pD3DXCreateSprite)(LPDIRECT3DDEVICE9 pDevice, LPD3DXSPRITE* ppSprite);



        int                     m_nVMR9Surfaces;                // Total number of DX Surfaces
        int                     m_iVMR9Surface;
        long                    m_nUsedBuffer;

        double                  m_fAvrFps;                      // Estimate the real FPS
        double                  m_fJitterStdDev;                // Estimate the Jitter std dev
        double                  m_fJitterMean;
        double                  m_fSyncOffsetStdDev;
        double                  m_fSyncOffsetAvr;
        double                  m_DetectedRefreshRate;

        CCritSec                m_refreshRateLock;
        double                  m_DetectedRefreshTime;
        double                  m_DetectedRefreshTimePrim;
        double                  m_DetectedScanlineTime;
        double                  m_DetectedScanlineTimePrim;
        double                  m_DetectedScanlinesPerFrame;

        double GetRefreshRate() const {
            if (m_DetectedRefreshRate) {
                return m_DetectedRefreshRate;
            }
            return m_refreshRate;
        }

        LONG GetScanLines() const {
            if (m_DetectedRefreshRate) {
                return (LONG)m_DetectedScanlinesPerFrame;
            }
            return m_ScreenSize.cy;
        }

        double                  m_ldDetectedRefreshRateList[100];
        double                  m_ldDetectedScanlineRateList[100];
        int                     m_DetectedRefreshRatePos;
        bool                    m_bSyncStatsAvailable;
        LONGLONG                m_pllJitter [NB_JITTER];            // Jitter buffer for stats
        LONGLONG                m_pllSyncOffset [NB_JITTER];        // Jitter buffer for stats
        LONGLONG                m_llLastPerf;
        LONGLONG                m_JitterStdDev;
        LONGLONG                m_MaxJitter;
        LONGLONG                m_MinJitter;
        LONGLONG                m_MaxSyncOffset;
        LONGLONG                m_MinSyncOffset;
        int                     m_nNextJitter;
        int                     m_nNextSyncOffset;
        REFERENCE_TIME          m_rtTimePerFrame;
        double                  m_DetectedFrameRate;
        double                  m_DetectedFrameTime;
        double                  m_DetectedFrameTimeStdDev;
        bool                    m_DetectedLock;
        LONGLONG                m_DetectedFrameTimeHistory[60];
        double                  m_DetectedFrameTimeHistoryHistory[500];
        int                     m_DetectedFrameTimePos;
        int                     m_bInterlaced;

        int                     m_VBlankEndWait;
        int                     m_VBlankStartWait;
        LONGLONG                m_VBlankWaitTime;
        LONGLONG                m_VBlankLockTime;
        int                     m_VBlankMin;
        int                     m_VBlankMinCalc;
        int                     m_VBlankMax;
        int                     m_VBlankEndPresent;
        LONGLONG                m_VBlankStartMeasureTime;
        int                     m_VBlankStartMeasure;

        LONGLONG                m_PresentWaitTime;
        LONGLONG                m_PresentWaitTimeMin;
        LONGLONG                m_PresentWaitTimeMax;

        LONGLONG                m_PaintTime;
        LONGLONG                m_PaintTimeMin;
        LONGLONG                m_PaintTimeMax;

        LONGLONG                m_WaitForGPUTime;

        LONGLONG                m_RasterStatusWaitTime;
        LONGLONG                m_RasterStatusWaitTimeMin;
        LONGLONG                m_RasterStatusWaitTimeMax;
        LONGLONG                m_RasterStatusWaitTimeMaxCalc;

        double                  m_ClockDiffCalc;
        double                  m_ClockDiffPrim;
        double                  m_ClockDiff;

        double                  m_TimeChangeHistory[100];
        double                  m_ClockChangeHistory[100];
        int                     m_ClockTimeChangeHistoryPos;
        double                  m_ModeratedTimeSpeed;
        double                  m_ModeratedTimeSpeedPrim;
        double                  m_ModeratedTimeSpeedDiff;

        bool                    m_bCorrectedFrameTime;
        int                     m_FrameTimeCorrection;
        LONGLONG                m_LastFrameDuration;
        LONGLONG                m_LastSampleTime;

        CString                 m_strStatsMsg[10];

        CString                 m_D3D9Device;

        CString                 m_Decoder;

        CFocusThread*           m_FocusThread;
        HWND                    m_hFocusWindow;

    public:
        CDX9AllocatorPresenter(HWND hWnd, bool bFullscreen, HRESULT& hr, bool bIsEVR, CString& _Error, bool isPreview = false);
        ~CDX9AllocatorPresenter();

        // ISubPicAllocatorPresenter
        STDMETHODIMP CreateRenderer(IUnknown** ppRenderer);
        STDMETHODIMP_(bool) Paint(bool bAll);
        STDMETHODIMP GetDIB(BYTE* lpDib, DWORD* size);
        STDMETHODIMP SetPixelShader(LPCSTR pSrcData, LPCSTR pTarget);
        STDMETHODIMP SetPixelShader2(LPCSTR pSrcData, LPCSTR pTarget, bool bScreenSpace);
        STDMETHODIMP_(bool) ResetDevice();
        STDMETHODIMP_(bool) DisplayChange();

        // ISubPicAllocatorPresenter4 (second subtitle): VMR9-renderless and EVR-custom support dual subtitles
        STDMETHODIMP_(bool) SupportsDualSubtitles() { return true; }
        CComPtr<ISubPicAllocator> CreateSecondaryAllocator() override;

        // ISubPicAllocatorPresenter2
        STDMETHODIMP_(bool) IsRendering() {
            return m_bIsRendering;
        }

        // ID3DFullscreenControl
        STDMETHODIMP SetD3DFullscreen(bool fEnabled);
        STDMETHODIMP GetD3DFullscreen(bool* pfEnabled);
    };
}
