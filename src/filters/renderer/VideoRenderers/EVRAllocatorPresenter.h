/*
 * (C) 2006-2015 see Authors.txt
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

#include "DX9AllocatorPresenter.h"
#include <mfapi.h>  // API Media Foundation
#include <evr9.h>
#include "../../../DSUtil/WinapiFunc.h"
#include "AsyncCallback.h"

namespace DSObjects
{
    class COuterEVR;

    class CEVRAllocatorPresenter :
        public CDX9AllocatorPresenter,
        public IMFGetService,
        public IMFTopologyServiceLookupClient,
        public IMFVideoDeviceID,
        public IMFVideoPresenter,
        public IDirect3DDeviceManager9,

        public IMFAsyncCallback,
        public IQualProp,
        public IMFRateSupport,
        public IMFVideoDisplayControl,
        public IMFVideoMixerBitmap,
        public IEVRTrustedVideoPlugin
    /*  public IMFVideoPositionMapper,      // Non mandatory EVR Presenter Interfaces (see later...) */
    {
    public:
        CEVRAllocatorPresenter(HWND hWnd, bool bFullscreen, HRESULT& hr, CString& _Error, bool isPreview=false);
        ~CEVRAllocatorPresenter();

        DECLARE_IUNKNOWN;
        STDMETHODIMP NonDelegatingQueryInterface(REFIID riid, void** ppv);

        STDMETHODIMP CreateRenderer(IUnknown** ppRenderer);
        STDMETHODIMP_(bool) Paint(bool bAll);
        STDMETHODIMP GetNativeVideoSize(LONG* lpWidth, LONG* lpHeight, LONG* lpARWidth, LONG* lpARHeight);
        STDMETHODIMP InitializeDevice(IMFMediaType* pMediaType);
        STDMETHODIMP_(bool) ResetDevice();
        STDMETHODIMP_(bool) DisplayChange();

        // ISubPicAllocatorPresenter2
        STDMETHODIMP_(bool) IsRendering() {
            return (m_nRenderState == Started);
        }

        // IMFClockStateSink
        STDMETHODIMP OnClockStart(/* [in] */ MFTIME hnsSystemTime, /* [in] */ LONGLONG llClockStartOffset);
        STDMETHODIMP OnClockStop(/* [in] */ MFTIME hnsSystemTime);
        STDMETHODIMP OnClockPause(/* [in] */ MFTIME hnsSystemTime);
        STDMETHODIMP OnClockRestart(/* [in] */ MFTIME hnsSystemTime);
        STDMETHODIMP OnClockSetRate(/* [in] */ MFTIME hnsSystemTime, /* [in] */ float flRate);

        // IBaseFilter delegate
        bool GetState(DWORD dwMilliSecsTimeout, FILTER_STATE* State, HRESULT& _ReturnValue);

        // IQualProp (EVR statistics window)
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
        STDMETHODIMP InitServicePointers(/* [in] */ __in  IMFTopologyServiceLookup* pLookup);
        STDMETHODIMP ReleaseServicePointers();

        // IMFVideoDeviceID
        STDMETHODIMP GetDeviceID(/* [out] */ __out  IID* pDeviceID);

        // IMFGetService
        STDMETHODIMP GetService(/* [in] */ __RPC__in REFGUID guidService,
                                           /* [in] */ __RPC__in REFIID riid,
                                           /* [iid_is][out] */ __RPC__deref_out_opt LPVOID* ppvObject);

        // IMFAsyncCallback
        STDMETHODIMP GetParameters(/* [out] */ __RPC__out DWORD* pdwFlags, /* [out] */ __RPC__out DWORD* pdwQueue);
        STDMETHODIMP Invoke(/* [in] */ __RPC__in_opt IMFAsyncResult* pAsyncResult);

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

        // IMFVideoMixerBitmap
        STDMETHODIMP ClearAlphaBitmap();
        STDMETHODIMP GetAlphaBitmapParameters(MFVideoAlphaBitmapParams *pBmpParms);
        STDMETHODIMP SetAlphaBitmap(const MFVideoAlphaBitmap *pBmpParms);
        STDMETHODIMP UpdateAlphaBitmapParameters(const MFVideoAlphaBitmapParams *pBmpParms);

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

    protected:
        STDMETHODIMP_(bool) Paint(IMFSample* pMFSample);
        void OnResetDevice();
        virtual void OnVBlankFinished(bool bAll, LONGLONG PerformanceCounter);

        double m_ModeratedTime;
        LONGLONG m_ModeratedTimeLast;
        LONGLONG m_ModeratedClockLast;
        LONGLONG m_ModeratedTimer;
        MFCLOCK_STATE m_LastClockState;
        LONGLONG GetClockTime(LONGLONG PerformanceCounter);

    private:

        enum RENDER_STATE {
            Stopped  = State_Stopped,
            Paused   = State_Paused,
            Started  = State_Running,
            Shutdown = State_Running + 1
        };

        COuterEVR*                       m_pOuterEVR;
        bool                             InputPinIsHLGNow() override;
        bool                             HasCurrentFrame() override { return m_nRenderState != Stopped && m_pCurrentlyDisplayedSample; }
        CComPtr<IMFClock>                m_pClock;
        CComPtr<IDirect3DDeviceManager9> m_pD3DManager;
        CComPtr<IMFTransform>            m_pMixer;
        CComPtr<IMediaEventSink>         m_pSink;
        CComPtr<IMFMediaType>            m_pMediaType;
        MFVideoAspectRatioMode           m_dwVideoAspectRatioMode;
        MFVideoRenderPrefs               m_dwVideoRenderPrefs;
        COLORREF                         m_BorderColor;


        HANDLE                           m_hEvtQuit;         // Stop rendering thread event
        bool                             m_bEvtQuit;
        HANDLE                           m_hEvtFlush;        // Discard all buffers
        bool                             m_bEvtFlush;

        bool                             m_fUseInternalTimer;
        INT32                            m_LastSetOutputRange;
        std::atomic_bool                 m_bPendingRenegotiate;
        bool                             m_bPendingMediaFinished;

        HANDLE                           m_hThread;
        HANDLE                           m_hGetMixerThread;
        HANDLE                           m_hVSyncThread;
        RENDER_STATE                     m_nRenderState;

        CCritSec                         m_SampleQueueLock;
        CCritSec                         m_ImageProcessingLock;
        CCritSec                         m_MediaTypeLock;
        CCritSec                         m_ThreadsLock;

        UINT32                           m_nCurrentGroupId;
        CInterfaceList<IMFSample>        m_FreeSamples;
        CInterfaceList<IMFSample>        m_ScheduledSamples;
        CComPtr<IMFSample>               m_pCurrentlyDisplayedSample;
        bool                             m_bLastSampleOffsetValid;
        LONGLONG                         m_LastScheduledSampleTime;
        double                           m_LastScheduledSampleTimeFP;
        LONGLONG                         m_LastScheduledUncorrectedSampleTime;
        LONGLONG                         m_MaxSampleDuration;
        LONGLONG                         m_LastSampleOffset;
        LONGLONG                         m_VSyncOffsetHistory[5];
        LONGLONG                         m_LastPredictedSync;
        int                              m_VSyncOffsetHistoryPos;

        UINT                             m_nResetToken;
        int                              m_nStepCount;

        bool                             m_bSignaledStarvation;
        LONGLONG                         m_StarvationClock;

        // Stats variable for IQualProp
        UINT                             m_pcFrames;
        UINT                             m_nDroppedUpdate;
        UINT                             m_pcFramesDrawn;    // Retrieves the number of frames drawn since streaming started
        UINT                             m_piAvg;
        UINT                             m_piDev;


        void                             GetMixerThread();
        static DWORD WINAPI              GetMixerThreadStatic(LPVOID lpParam);

        bool                             GetImageFromMixer();
        void                             RenderThread();
        static DWORD WINAPI              PresentThread(LPVOID lpParam);
        void                             ResetStats();
        void                             StartWorkerThreads();
        void                             StopWorkerThreads();
        HRESULT                          CheckShutdown() const;
        void                             CompleteFrameStep(bool bCancel);
        static DWORD WINAPI              VSyncThreadStatic(LPVOID lpParam);
        void                             VSyncThread();

        void                             RemoveAllSamples();
        HRESULT                          GetFreeSample(IMFSample** ppSample);
        HRESULT                          GetScheduledSample(IMFSample** ppSample, int& count);
        void                             AddToFreeList(IMFSample* pSample, bool bTail);
        void                             AddToScheduledList(IMFSample* pSample, bool bSorted);
        void                             FlushSamples();

        HRESULT TrackSample(IMFSample* pSample);

        // Callback when a video sample is released.
        HRESULT OnSampleFree(IMFAsyncResult* pResult);
        AsyncCallback<CEVRAllocatorPresenter> m_SampleFreeCallback;

        // === Media type negotiation functions
        HRESULT                          RenegotiateMediaType();
        HRESULT                          IsMediaTypeSupported(IMFMediaType* pMixerType);
        HRESULT                          CreateOptimalOutputType(IMFMediaType* pMixerProposedType, IMFMediaType* pMixerInputType, IMFMediaType** pType);
        HRESULT                          SetMediaType(IMFMediaType* pType);
        HRESULT                          GetMediaTypeFourCC(IMFMediaType* pType, DWORD* pFourCC);
        HRESULT                          GetMediaTypeMerit(IMFMediaType* pType, int* pMerit);
        LPCTSTR                          GetMediaTypeFormatDesc(IMFMediaType* pMediaType);

        const WinapiFunc<decltype(DXVA2CreateDirect3DDeviceManager9)> fnDXVA2CreateDirect3DDeviceManager9;
        const WinapiFunc<decltype(MFCreateDXSurfaceBuffer)> fnMFCreateDXSurfaceBuffer;
        const WinapiFunc<decltype(MFCreateVideoSampleFromSurface)> fnMFCreateVideoSampleFromSurface;
        const WinapiFunc<decltype(MFCreateMediaType)> fnMFCreateMediaType;

        const WinapiFunc<decltype(AvSetMmThreadCharacteristicsW)> fnAvSetMmThreadCharacteristicsW;
        const WinapiFunc<decltype(AvSetMmThreadPriority)> fnAvSetMmThreadPriority;
        const WinapiFunc<decltype(AvRevertMmThreadCharacteristics)> fnAvRevertMmThreadCharacteristics;
    };

}
