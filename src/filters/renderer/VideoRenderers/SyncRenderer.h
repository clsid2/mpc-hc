/*
 * (C) 2010-2015, 2017 see Authors.txt
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

#include "../../../SubPic/ISubPic.h"
#include "RenderersSettings.h"
#include "SyncAllocatorPresenter.h"
#include "AllocatorCommon.h"
#include "../../../DSUtil/WinapiFunc.h"
#include <d3d9.h>
#include <dxva2api.h>
#include "d3dx9/d3dx9.h"

#define VMRBITMAP_UPDATE 0x80000000
#define MAX_PICTURE_SLOTS (60 + 2) // Last 2 for pixels shader!
#define NB_JITTER 126
#include "AsyncCallback.h"

class CFocusThread;

// Possible messages to the PowerStrip API. PowerStrip is used to control
// the display frequency in one of the video - display synchronization modes.
// Powerstrip can also through a CGenlock object give very accurate timing data
// (given) that the gfx board is supported by PS.
#define UM_SETCUSTOMTIMING     (WM_USER + 200)
#define UM_SETREFRESHRATE      (WM_USER + 201)
#define UM_SETPOLARITY         (WM_USER + 202)
#define UM_REMOTECONTROL       (WM_USER + 210)
#define UM_SETGAMMARAMP        (WM_USER + 203)
#define UM_CREATERESOLUTION    (WM_USER + 204)
#define UM_GETTIMING           (WM_USER + 205)
#define UM_SETCUSTOMTIMINGFAST (WM_USER + 211) // Sets timing without writing to file. Faster

#define PositiveHorizontalPolarity  0x00
#define PositiveVerticalPolarity    0x00
#define NegativeHorizontalPolarity  0x02
#define NegativeVerticalPolarity    0x04
#define HideTrayIcon    0x00
#define ShowTrayIcon    0x01
#define ClosePowerStrip 0x63

#define HACTIVE     0
#define HFRONTPORCH 1
#define HSYNCWIDTH  2
#define HBACKPORCH  3
#define VACTIVE     4
#define VFRONTPORCH 5
#define VSYNCWIDTH  6
#define VBACKPORCH  7
#define PIXELCLOCK  8
#define UNKNOWN     9

// Guid to tag IMFSample with a group id
static const GUID GUID_GROUP_ID = { 0x309e32cc, 0x9b23, 0x4c6c, { 0x86, 0x63, 0xcd, 0xd9, 0xad, 0x49, 0x7f, 0x8a } };
// Guid to tag IMFSample with DirectX surface index
static const GUID GUID_SURFACE_INDEX = { 0x30c8e9f6, 0x415, 0x4b81, { 0xa3, 0x15, 0x1, 0xa, 0xc6, 0xa9, 0xda, 0x19 } };

namespace GothSync
{
    enum EVR_STATS_MSG {
        MSG_MIXERIN,
        MSG_MIXEROUT,
        MSG_ERROR
    };

#pragma pack(push, 1)

    template<int texcoords>
    struct MYD3DVERTEX {
        float x, y, z, rhw;
        struct {
            float u, v;
        } t[texcoords];
    };

    template<>
    struct MYD3DVERTEX<0> {
        float x, y, z, rhw;
        DWORD Diffuse;
    };

#pragma pack(pop)

    class CGenlock;
    class CSyncRenderer;

    // Base allocator-presenter
    class CBaseAP:
        public CSubPicAllocatorPresenterImpl
    {
    protected:
        CRenderersSettings::CAdvRendererSettings m_LastRendererSettings;

        HMODULE m_hDWMAPI;
        HRESULT(__stdcall* m_pDwmIsCompositionEnabled)(__out BOOL* pfEnabled);
        HRESULT(__stdcall* m_pDwmEnableComposition)(UINT uCompositionAction);

        CCritSec m_allocatorLock;
        CComPtr<IDirect3D9Ex> m_pD3DEx;
        CComPtr<IDirect3D9> m_pD3D;
        CComPtr<IDirect3DDevice9Ex> m_pD3DDevEx;
        CComPtr<IDirect3DDevice9> m_pD3DDev;

        CComPtr<IDirect3DTexture9> m_pVideoTexture[MAX_PICTURE_SLOTS];
        CComPtr<IDirect3DSurface9> m_pVideoSurface[MAX_PICTURE_SLOTS];
        CComPtr<IDirect3DTexture9> m_pOSDTexture;
        CComPtr<IDirect3DSurface9> m_pOSDSurface;
        CComPtr<ID3DXLine> m_pLine;
        CComPtr<ID3DXFont> m_pFont;
        CComPtr<ID3DXSprite> m_pSprite;
        CSyncRenderer* m_pOuterEVR;

        class CExternalPixelShader
        {
        public:
            CComPtr<IDirect3DPixelShader9> m_pPixelShader;
            CStringA m_SourceData;
            CStringA m_SourceTarget;
            HRESULT Compile(CPixelShaderCompiler* pCompiler) {
                HRESULT hr = pCompiler->CompileShader(m_SourceData, "main", m_SourceTarget, 0, &m_pPixelShader);
                if (FAILED(hr)) {
                    return hr;
                }
                return S_OK;
            }
        };

        CAutoPtr<CPixelShaderCompiler> m_pPSC;
        CAtlList<CExternalPixelShader> m_pPixelShaders;
        CAtlList<CExternalPixelShader> m_pPixelShadersScreenSpace;
        CComPtr<IDirect3DPixelShader9> m_pResizerPixelShader[4]; // bl, bc1, bc2_1, bc2_2
        CComPtr<IDirect3DTexture9> m_pScreenSizeTemporaryTexture[2];

        D3DFORMAT m_SurfaceType;
        D3DFORMAT m_BackbufferType;
        D3DFORMAT m_DisplayType;
        D3DTEXTUREFILTERTYPE m_filter;
        D3DCAPS9 m_caps;
        D3DPRESENT_PARAMETERS m_pp;

        bool SettingsNeedResetDevice();
        void SendResetRequest();
        virtual HRESULT CreateDXDevice(CString& _Error);
        virtual HRESULT ResetDXDevice(CString& _Error);
        virtual HRESULT AllocSurfaces(D3DFORMAT Format = D3DFMT_A8R8G8B8);
        virtual void DeleteSurfaces();

        LONGLONG m_LastAdapterCheck;
        UINT m_CurrentAdapter;

        float m_bicubicA;
        HRESULT InitResizers(float bicubicA, bool bNeedScreenSizeTexture);

        // Functions to trace timing performance
        void SyncStats(LONGLONG syncTime);
        void SyncOffsetStats(LONGLONG syncOffset);
        void InitStats();
        void DrawStats();

        template<int texcoords>
        void AdjustQuad(MYD3DVERTEX<texcoords>* v, double dx, double dy);
        template<int texcoords>
        HRESULT TextureBlt(IDirect3DDevice9* pD3DDev, MYD3DVERTEX<texcoords> v[4], D3DTEXTUREFILTERTYPE filter);
        MFOffset GetOffset(float v);
        MFVideoArea GetArea(float x, float y, DWORD width, DWORD height);
        bool ClipToSurface(IDirect3DSurface9* pSurface, CRect& s, CRect& d);

        HRESULT DrawRectBase(IDirect3DDevice9* pD3DDev, MYD3DVERTEX<0> v[4]);
        HRESULT DrawRect(DWORD _Color, DWORD _Alpha, const CRect& _Rect);
        HRESULT TextureCopy(IDirect3DTexture9* pTexture);
        HRESULT TextureResize(IDirect3DTexture9* pTexture, const Vector dst[4], D3DTEXTUREFILTERTYPE filter, const CRect& SrcRect);
        HRESULT TextureResizeBilinear(IDirect3DTexture9* pTexture, const Vector dst[4], const CRect& SrcRect);
        HRESULT TextureResizeBicubic1pass(IDirect3DTexture9* pTexture, const Vector dst[4], const CRect& SrcRect);
        HRESULT TextureResizeBicubic2pass(IDirect3DTexture9* pTexture, const Vector dst[4], const CRect& SrcRect);
        HRESULT Resize(IDirect3DTexture9* pTexture, const CRect& srcRect, const CRect& destRect);

        typedef HRESULT(WINAPI* D3DXLoadSurfaceFromMemoryPtr)(
            LPDIRECT3DSURFACE9 pDestSurface,
            CONST PALETTEENTRY* pDestPalette,
            CONST RECT* pDestRect,
            LPCVOID pSrcMemory,
            D3DFORMAT SrcFormat,
            UINT SrcPitch,
            CONST PALETTEENTRY* pSrcPalette,
            CONST RECT* pSrcRect,
            DWORD Filter,
            D3DCOLOR ColorKey);

        typedef HRESULT(WINAPI* D3DXLoadSurfaceFromSurfacePtr)(
            LPDIRECT3DSURFACE9        pDestSurface,
            CONST PALETTEENTRY* pDestPalette,
            CONST RECT* pDestRect,
            LPDIRECT3DSURFACE9        pSrcSurface,
            CONST PALETTEENTRY* pSrcPalette,
            CONST RECT* pSrcRect,
            DWORD                     Filter,
            D3DCOLOR                  ColorKey);

        typedef HRESULT(WINAPI* D3DXCreateLinePtr)
        (LPDIRECT3DDEVICE9 pDevice,
         LPD3DXLINE* ppLine);

        typedef HRESULT(WINAPI* D3DXCreateFontPtr)(
            LPDIRECT3DDEVICE9 pDevice,
            int Height,
            UINT Width,
            UINT Weight,
            UINT MipLevels,
            bool Italic,
            DWORD CharSet,
            DWORD OutputPrecision,
            DWORD Quality,
            DWORD PitchAndFamily,
            LPCWSTR pFaceName,
            LPD3DXFONT* ppFont);

        HRESULT AlphaBlt(RECT* pSrc, const RECT* pDst, IDirect3DTexture9* pTexture);

        virtual void OnResetDevice() {};

        int m_nTearingPos;
        VMR9AlphaBitmap m_VMR9AlphaBitmap;
        CAutoVectorPtr<BYTE> m_VMR9AlphaBitmapData;
        CRect m_VMR9AlphaBitmapRect;
        int m_VMR9AlphaBitmapWidthBytes;

        D3DXLoadSurfaceFromMemoryPtr m_pD3DXLoadSurfaceFromMemory;
        D3DXLoadSurfaceFromSurfacePtr m_pD3DXLoadSurfaceFromSurface;
        D3DXCreateLinePtr m_pD3DXCreateLine;
        D3DXCreateFontPtr m_pD3DXCreateFont;
        HRESULT(__stdcall* m_pD3DXCreateSprite)(LPDIRECT3DDEVICE9 pDevice, LPD3DXSPRITE* ppSprite);

        int m_nDXSurface;   // Total number of DX Surfaces
        int m_nVMR9Surfaces;
        int m_iVMR9Surface;
        int m_nCurSurface; // Surface currently displayed
        long m_nUsedBuffer;

        LONG m_lNextSampleWait; // Waiting time for next sample in EVR
        bool m_bSnapToVSync;    // True if framerate is low enough so that snap to vsync makes sense

        UINT m_uScanLineEnteringPaint;      // The active scan line when entering Paint()
        REFERENCE_TIME m_llEstVBlankTime;   // Next vblank start time in reference clock "coordinates"

        double m_fAvrFps;       // Estimate the true FPS as given by the distance between vsyncs when a frame has been presented
        double m_fJitterStdDev; // VSync estimate std dev
        double m_fJitterMean;   // Means time between two syncpulses when a frame has been presented (i.e. when Paint() has been called

        double m_fSyncOffsetAvr;    // Means time between the call of Paint() and vsync. To avoid tearing this should be several ms at least
        double m_fSyncOffsetStdDev; // The std dev of the above

        bool m_bHighColorResolution;
        bool m_bCompositionEnabled;
        bool m_bDesktopCompositionDisabled;
        bool m_bIsFullscreen, fullScreenChanged;
        bool m_bNeedCheckSample;
        DWORD m_dMainThreadId;

        CSize m_ScreenSize;

        // Display and frame rates and cycles
        double m_dDetectedScanlineTime; // Time for one (horizontal) scan line. Extracted at stream start and used to calculate vsync time
        double m_dD3DRefreshCycle;      // Display refresh cycle ms
        double m_dEstRefreshCycle;      // As estimated from scan lines
        double m_dFrameCycle;           // Average sample time, extracted from the samples themselves
        // double m_fps is defined in ISubPic.h
        double m_dOptimumDisplayCycle;  // The display cycle that is closest to the frame rate. A multiple of the actual display cycle
        double m_dCycleDifference;      // Difference in video and display cycle time relative to the video cycle time

        UINT m_pcFramesDropped;
        UINT m_pcFramesDuplicated;
        UINT m_pcFramesDrawn;

        LONGLONG m_pllJitter [NB_JITTER];     // Vertical sync time stats
        LONGLONG m_pllSyncOffset [NB_JITTER]; // Sync offset time stats
        int m_nNextJitter;
        int m_nNextSyncOffset;
        LONGLONG m_JitterStdDev;

        LONGLONG m_llLastSyncTime;

        LONGLONG m_MaxJitter;
        LONGLONG m_MinJitter;
        LONGLONG m_MaxSyncOffset;
        LONGLONG m_MinSyncOffset;
        UINT m_uSyncGlitches;

        LONGLONG m_llSampleTime, m_llLastSampleTime; // Present time for the current sample
        LONGLONG m_llHysteresis;
        LONG m_lShiftToNearest, m_lShiftToNearestPrev;
        bool m_bVideoSlowerThanDisplay;

        REFERENCE_TIME m_rtTimePerFrame;
        bool m_bInterlaced;
        double m_TextScale;
        CString  m_strStatsMsg[10];

        CGenlock* m_pGenlock;                 // The video - display synchronizer class
        CComPtr<IReferenceClock> m_pRefClock; // The reference clock. Used in Paint()
        CComPtr<IAMAudioRendererStats> m_pAudioStats; // Audio statistics from audio renderer. To check so that audio is in sync
        DWORD m_lAudioLag;                    // Time difference between audio and video when the audio renderer is matching rate to the external reference clock
        long m_lAudioLagMin, m_lAudioLagMax;  // The accumulated difference between the audio renderer and the master clock
        DWORD m_lAudioSlaveMode;              // To check whether the audio renderer matches rate with SyncClock (returns the value 4 if it does)

        double GetRefreshRate();              // Get the best estimate of the display refresh rate in Hz
        double GetDisplayCycle();             // Get the best estimate of the display cycle time in milliseconds
        double GetCycleDifference();          // Get the difference in video and display cycle times.
        void EstimateRefreshTimings();        // Estimate the times for one scan line and one frame respectively from the actual refresh data
        bool ExtractInterlaced(const AM_MEDIA_TYPE* pmt);

        CFocusThread* m_FocusThread;
        HWND          m_hFocusWindow;

    public:
        CBaseAP(HWND hWnd, bool bFullscreen, HRESULT& hr, CString& _Error);
        ~CBaseAP();

        CCritSec m_VMR9AlphaBitmapLock;
        void UpdateAlphaBitmap();
        void ResetStats();

        // ISubPicAllocatorPresenter
        STDMETHODIMP CreateRenderer(IUnknown** ppRenderer);
        STDMETHODIMP_(bool) Paint(bool bAll);
        STDMETHODIMP GetDIB(BYTE* lpDib, DWORD* size);
        STDMETHODIMP SetPixelShader(LPCSTR pSrcData, LPCSTR pTarget);
        STDMETHODIMP SetPixelShader2(LPCSTR pSrcData, LPCSTR pTarget, bool bScreenSpace);
        STDMETHODIMP_(bool) ResetDevice();
        STDMETHODIMP_(bool) DisplayChange();

        // ISubPicAllocatorPresenter4 (second subtitle)
        STDMETHODIMP_(bool) SupportsDualSubtitles() { return true; }
        CComPtr<ISubPicAllocator> CreateSecondaryAllocator() override;
    };

    class CSyncAP:
        public CBaseAP,
        public IMFGetService,
        public IMFTopologyServiceLookupClient,
        public IMFVideoDeviceID,
        public IMFVideoPresenter,
        public IDirect3DDeviceManager9,
        public IMFAsyncCallback,
        public IQualProp,
        public IMFRateSupport,
        public IMFVideoDisplayControl,
        public IEVRTrustedVideoPlugin,
        public ISyncClockAdviser,
        public ID3DFullscreenControl
    {
    public:
        CSyncAP(HWND hWnd, bool bFullscreen, HRESULT& hr, CString& _Error);
        ~CSyncAP();

        DECLARE_IUNKNOWN;
        STDMETHODIMP NonDelegatingQueryInterface(REFIID riid, void** ppv);

        STDMETHODIMP CreateRenderer(IUnknown** ppRenderer);
        STDMETHODIMP_(bool) Paint(bool bAll);
        STDMETHODIMP GetNativeVideoSize(LONG* lpWidth, LONG* lpHeight, LONG* lpARWidth, LONG* lpARHeight);
        STDMETHODIMP InitializeDevice(AM_MEDIA_TYPE* pMediaType);
        STDMETHODIMP_(bool) ResetDevice();

        // ISubPicAllocatorPresenter2
        STDMETHODIMP_(bool) IsRendering() {
            return (m_nRenderState == Started);
        }

        // IMFClockStateSink
        STDMETHODIMP OnClockStart(MFTIME hnsSystemTime, LONGLONG llClockStartOffset);
        STDMETHODIMP OnClockStop(MFTIME hnsSystemTime);
        STDMETHODIMP OnClockPause(MFTIME hnsSystemTime);
        STDMETHODIMP OnClockRestart(MFTIME hnsSystemTime);
        STDMETHODIMP OnClockSetRate(MFTIME hnsSystemTime, float flRate);

        // IBaseFilter delegate
        bool GetState(DWORD dwMilliSecsTimeout, FILTER_STATE* State, HRESULT& _ReturnValue);

        // IQualProp (EVR statistics window). These are incompletely implemented currently
        STDMETHODIMP get_FramesDroppedInRenderer(int* pcFrames);
        STDMETHODIMP get_FramesDrawn(int* pcFramesDrawn);
        STDMETHODIMP get_AvgFrameRate(int* piAvgFrameRate);
        STDMETHODIMP get_Jitter(int* iJitter);
        STDMETHODIMP get_AvgSyncOffset(int* piAvg);
        STDMETHODIMP get_DevSyncOffset(int* piDev);

        // IMFRateSupport
        STDMETHODIMP GetSlowestRate(MFRATE_DIRECTION eDirection, BOOL fThin, float* pflRate);
        STDMETHODIMP GetFastestRate(MFRATE_DIRECTION eDirection, BOOL fThin, float* pflRate);
        STDMETHODIMP IsRateSupported(BOOL fThin, float flRate, float* pflNearestSupportedRate);
        float GetMaxRate(BOOL bThin);

        // IMFVideoPresenter
        STDMETHODIMP ProcessMessage(MFVP_MESSAGE_TYPE eMessage, ULONG_PTR ulParam);
        STDMETHODIMP GetCurrentMediaType(__deref_out  IMFVideoMediaType** ppMediaType);

        // IMFTopologyServiceLookupClient
        STDMETHODIMP InitServicePointers(__in  IMFTopologyServiceLookup* pLookup);
        STDMETHODIMP ReleaseServicePointers();

        // IMFVideoDeviceID
        STDMETHODIMP GetDeviceID(__out  IID* pDeviceID);

        // IMFGetService
        STDMETHODIMP GetService(__RPC__in REFGUID guidService, __RPC__in REFIID riid, __RPC__deref_out_opt LPVOID* ppvObject);

        // IMFAsyncCallback
        STDMETHODIMP GetParameters(__RPC__out DWORD* pdwFlags, /* [out] */ __RPC__out DWORD* pdwQueue);
        STDMETHODIMP Invoke(__RPC__in_opt IMFAsyncResult* pAsyncResult);

        // IMFVideoDisplayControl
        STDMETHODIMP GetNativeVideoSize(SIZE* pszVideo, SIZE* pszARVideo);
        STDMETHODIMP GetIdealVideoSize(SIZE* pszMin, SIZE* pszMax);
        STDMETHODIMP SetVideoPosition(const MFVideoNormalizedRect* pnrcSource, const LPRECT prcDest);
        STDMETHODIMP GetVideoPosition(MFVideoNormalizedRect* pnrcSource, LPRECT prcDest);
        STDMETHODIMP SetAspectRatioMode(DWORD dwAspectRatioMode);
        STDMETHODIMP GetAspectRatioMode(DWORD* pdwAspectRatioMode);
        STDMETHODIMP SetVideoWindow(HWND hwndVideo);
        STDMETHODIMP GetVideoWindow(HWND* phwndVideo);
        STDMETHODIMP RepaintVideo();
        STDMETHODIMP GetCurrentImage(BITMAPINFOHEADER* pBih, BYTE** pDib, DWORD* pcbDib, LONGLONG* pTimeStamp);
        STDMETHODIMP SetBorderColor(COLORREF Clr);
        STDMETHODIMP GetBorderColor(COLORREF* pClr);
        STDMETHODIMP SetRenderingPrefs(DWORD dwRenderFlags);
        STDMETHODIMP GetRenderingPrefs(DWORD* pdwRenderFlags);
        STDMETHODIMP SetFullscreen(BOOL fFullscreen);
        STDMETHODIMP GetFullscreen(BOOL* pfFullscreen);

        // IEVRTrustedVideoPlugin
        STDMETHODIMP IsInTrustedVideoMode(BOOL* pYes);
        STDMETHODIMP CanConstrict(BOOL* pYes);
        STDMETHODIMP SetConstriction(DWORD dwKPix);
        STDMETHODIMP DisableImageExport(BOOL bDisable);

        // IDirect3DDeviceManager9
        STDMETHODIMP ResetDevice(IDirect3DDevice9* pDevice, UINT resetToken);
        STDMETHODIMP OpenDeviceHandle(HANDLE* phDevice);
        STDMETHODIMP CloseDeviceHandle(HANDLE hDevice);
        STDMETHODIMP TestDevice(HANDLE hDevice);
        STDMETHODIMP LockDevice(HANDLE hDevice, IDirect3DDevice9** ppDevice, BOOL fBlock);
        STDMETHODIMP UnlockDevice(HANDLE hDevice, BOOL fSaveState);
        STDMETHODIMP GetVideoService(HANDLE hDevice, REFIID riid, void** ppService);

        // ID3DFullscreenControl
        STDMETHODIMP SetD3DFullscreen(bool fEnabled);
        STDMETHODIMP GetD3DFullscreen(bool* pfEnabled);

    protected:
        STDMETHODIMP_(bool) Paint(IMFSample* pMFSample);
        void OnResetDevice();
        MFCLOCK_STATE m_LastClockState;

    private:
        enum RENDER_STATE {
            Started  = State_Running,
            Stopped  = State_Stopped,
            Paused   = State_Paused,
            Shutdown = State_Running + 1
        };

        CComPtr<IMFClock> m_pClock;
        CComPtr<IDirect3DDeviceManager9> m_pD3DManager;
        CComPtr<IMFTransform> m_pMixer;
        CComPtr<IMediaEventSink> m_pSink;
        CComPtr<IMFMediaType> m_pMediaType;
        MFVideoAspectRatioMode m_dwVideoAspectRatioMode;
        MFVideoRenderPrefs m_dwVideoRenderPrefs;
        COLORREF m_BorderColor;

        HANDLE m_hEvtQuit;  // Stop rendering thread event
        bool m_bEvtQuit;
        HANDLE m_hEvtFlush; // Discard all buffers
        bool m_bEvtFlush;
        HANDLE m_hEvtSkip;  // Skip frame
        bool m_bEvtSkip;

        bool m_bUseInternalTimer;
        INT32 m_LastSetOutputRange;
        std::atomic_bool m_bPendingRenegotiate;
        bool m_bPendingMediaFinished;
        bool m_bPrerolled;  // true if first sample has been displayed.

        HANDLE m_hRenderThread;
        HANDLE m_hMixerThread;
        RENDER_STATE m_nRenderState;
        bool m_bStepping;

        CCritSec m_SampleQueueLock;
        CCritSec m_ImageProcessingLock;

        UINT32                    m_nCurrentGroupId;
        CInterfaceList<IMFSample> m_FreeSamples;
        CInterfaceList<IMFSample> m_ScheduledSamples;
        CComPtr<IMFSample>        m_pCurrentlyDisplayedSample;
        UINT m_nResetToken;
        int m_nStepCount;

        bool GetSampleFromMixer();
        void MixerThread();
        static DWORD WINAPI MixerThreadStatic(LPVOID lpParam);
        void RenderThread();
        static DWORD WINAPI RenderThreadStatic(LPVOID lpParam);

        void StartWorkerThreads();
        void StopWorkerThreads();
        HRESULT CheckShutdown() const;
        void CompleteFrameStep(bool bCancel);

        void RemoveAllSamples();
        STDMETHODIMP AdviseSyncClock(ISyncClock* sC);
        HRESULT BeginStreaming();
        HRESULT GetFreeSample(IMFSample** ppSample);
        HRESULT GetScheduledSample(IMFSample** ppSample, int& count);
        void AddToFreeList(IMFSample* pSample, bool bTail);
        void AddToScheduledList(IMFSample* pSample, bool bSorted);
        void FlushSamples();

        HRESULT TrackSample(IMFSample* pSample);

        // Callback when a video sample is released.
        HRESULT OnSampleFree(IMFAsyncResult* pResult);
        AsyncCallback<CSyncAP> m_SampleFreeCallback;

        LONGLONG GetMediaTypeMerit(IMFMediaType* pMediaType);
        HRESULT RenegotiateMediaType();
        HRESULT IsMediaTypeSupported(IMFMediaType* pMixerType);
        HRESULT CreateOptimalOutputType(IMFMediaType* pMixerProposedType, IMFMediaType* pMixerInputType, IMFMediaType** ppType);
        HRESULT SetMediaType(IMFMediaType* pType);

        const WinapiFunc<decltype(DXVA2CreateDirect3DDeviceManager9)> fnDXVA2CreateDirect3DDeviceManager9;
        const WinapiFunc<decltype(MFCreateDXSurfaceBuffer)> fnMFCreateDXSurfaceBuffer;
        const WinapiFunc<decltype(MFCreateVideoSampleFromSurface)> fnMFCreateVideoSampleFromSurface;
        const WinapiFunc<decltype(MFCreateMediaType)> fnMFCreateMediaType;

        const WinapiFunc<decltype(AvSetMmThreadCharacteristicsW)> fnAvSetMmThreadCharacteristicsW;
        const WinapiFunc<decltype(AvSetMmThreadPriority)> fnAvSetMmThreadPriority;
        const WinapiFunc<decltype(AvRevertMmThreadCharacteristics)> fnAvRevertMmThreadCharacteristics;
    };

    class CSyncRenderer:
        public CUnknown,
        public IVMRMixerBitmap9,
        public IBaseFilter
    {
        CComPtr<IUnknown> m_pEVR;
        IBaseFilter* m_pEVRBase;
        VMR9AlphaBitmap* m_pVMR9AlphaBitmap;
        CSyncAP* m_pAllocatorPresenter;

    public:
        CSyncRenderer(const TCHAR* pName, LPUNKNOWN pUnk, HRESULT& hr, VMR9AlphaBitmap* pVMR9AlphaBitmap, CSyncAP* pAllocatorPresenter);
        ~CSyncRenderer();

        // IBaseFilter
        virtual HRESULT STDMETHODCALLTYPE EnumPins(__out IEnumPins** ppEnum);
        virtual HRESULT STDMETHODCALLTYPE FindPin(LPCWSTR Id, __out IPin** ppPin);
        virtual HRESULT STDMETHODCALLTYPE QueryFilterInfo(__out FILTER_INFO* pInfo);
        virtual HRESULT STDMETHODCALLTYPE JoinFilterGraph(__in_opt IFilterGraph* pGraph, __in_opt LPCWSTR pName);
        virtual HRESULT STDMETHODCALLTYPE QueryVendorInfo(__out LPWSTR* pVendorInfo);
        virtual HRESULT STDMETHODCALLTYPE Stop();
        virtual HRESULT STDMETHODCALLTYPE Pause();
        virtual HRESULT STDMETHODCALLTYPE Run(REFERENCE_TIME tStart);
        virtual HRESULT STDMETHODCALLTYPE GetState(DWORD dwMilliSecsTimeout, __out FILTER_STATE* State);
        virtual HRESULT STDMETHODCALLTYPE SetSyncSource(__in_opt  IReferenceClock* pClock);
        virtual HRESULT STDMETHODCALLTYPE GetSyncSource(__deref_out_opt  IReferenceClock** pClock);
        virtual HRESULT STDMETHODCALLTYPE GetClassID(__RPC__out CLSID* pClassID);

        // IVMRMixerBitmap9
        STDMETHODIMP GetAlphaBitmapParameters(VMR9AlphaBitmap* pBmpParms);
        STDMETHODIMP SetAlphaBitmap(const VMR9AlphaBitmap*  pBmpParms);
        STDMETHODIMP UpdateAlphaBitmapParameters(const VMR9AlphaBitmap* pBmpParms);

        DECLARE_IUNKNOWN;
        virtual HRESULT STDMETHODCALLTYPE NonDelegatingQueryInterface(REFIID riid, void** ppvObject);
    };

    class CGenlock
    {
    public:
        class MovingAverage
        {
        public:
            MovingAverage(size_t size)
                : fifoSize(size)
                , fifo(fifoSize)
                , oldestSample(0)
                , sum(0.0) {
            }

            double Average(double sample) {
                sum = sum + sample - fifo[oldestSample];
                fifo[oldestSample] = sample;
                oldestSample++;
                if (oldestSample == fifoSize) {
                    oldestSample = 0;
                }
                return sum / fifoSize;
            }

        private:
            size_t fifoSize;
            std::vector<double> fifo;
            size_t oldestSample;
            double sum;
        };

        CGenlock(double target, double limit, int rowD, int colD, double clockD, UINT mon);
        CGenlock(const CGenlock&) = delete;
        ~CGenlock();

        CGenlock& operator=(const CGenlock&) = delete;

        BOOL PowerstripRunning();   // TRUE if PowerStrip is running
        HRESULT GetTiming();        // Get the string representing the display's current timing parameters
        HRESULT ResetTiming();      // Reset timing to what was last registered by GetTiming()
        HRESULT ResetClock();       // Reset reference clock speed to nominal
        HRESULT SetTargetSyncOffset(double targetD);
        HRESULT GetTargetSyncOffset(double* targetD);
        HRESULT SetControlLimit(double cL);
        HRESULT GetControlLimit(double* cL);
        HRESULT SetDisplayResolution(UINT columns, UINT lines);
        HRESULT AdviseSyncClock(ISyncClock* sC);
        HRESULT SetMonitor(UINT mon); // Set the number of the monitor to synchronize
        HRESULT ResetStats();         // Reset timing statistics

        HRESULT ControlDisplay(double syncOffset, double frameCycle); // Adjust the frequency of the display if needed
        HRESULT ControlClock(double syncOffset, double frameCycle);   // Adjust the frequency of the clock if needed
        HRESULT UpdateStats(double syncOffset, double frameCycle);    // Don't adjust anything, just update the syncOffset stats

        bool powerstripTimingExists;        // true if display timing has been got through Powerstrip
        bool liveSource;                    // true if live source -> display sync is the only option
        int adjDelta;                       // -1 for display slower in relation to video, 0 for keep, 1 for faster
        int lineDelta;                      // The number of rows added or subtracted when adjusting display fps
        int columnDelta;                    // The number of colums added or subtracted when adjusting display fps
        double cycleDelta;                  // Adjustment factor for cycle time as fraction of nominal value
        UINT displayAdjustmentsMade;        // The number of adjustments made to display refresh rate
        UINT clockAdjustmentsMade;          // The number of adjustments made to clock frequency

        UINT totalLines, totalColumns;      // Including the porches and sync widths
        UINT visibleLines, visibleColumns;  // The nominal resolution
        MovingAverage syncOffsetFifo;
        MovingAverage frameCycleFifo;
        double minSyncOffset, maxSyncOffset;
        double syncOffsetAvg; // Average of the above
        double minFrameCycle, maxFrameCycle;
        double frameCycleAvg;

        UINT pixelClock;                // In pixels/s
        double displayFreqCruise;       // Nominal display frequency in frames/s
        double displayFreqSlower;
        double displayFreqFaster;
        double curDisplayFreq;          // Current (adjusted) display frequency
        double controlLimit;            // How much the sync offset is allowed to drift from target sync offset
        WPARAM monitor;                 // The monitor to be controlled. 0-based.
        CComPtr<ISyncClock> syncClock;  // Interface to an adjustable reference clock

    private:
        HWND psWnd; // PowerStrip window
        const static int TIMING_PARAM_CNT = 10;
        const static int MAX_LOADSTRING = 100;
        std::array<UINT, TIMING_PARAM_CNT> displayTiming;       // Display timing parameters
        std::array<UINT, TIMING_PARAM_CNT> displayTimingSave;   // So that we can reset the display at exit
        TCHAR faster[MAX_LOADSTRING];                           // String corresponding to faster display frequency
        TCHAR cruise[MAX_LOADSTRING];                           // String corresponding to nominal display frequency
        TCHAR slower[MAX_LOADSTRING];                           // String corresponding to slower display frequency
        TCHAR savedTiming[MAX_LOADSTRING];                      // String version of saved timing (to be restored upon exit)
        double lowSyncOffset;       // The closest we want to let the scheduled render time to get to the next vsync. In % of the frame time
        double targetSyncOffset;    // Where we want the scheduled render time to be in relation to the next vsync
        double highSyncOffset;      // The furthers we want to let the scheduled render time to get to the next vsync
        CCritSec csGenlockLock;
    };
}
