/*
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
#include <algorithm>
#include "EVRAllocatorPresenter.h"
#include "OuterEVR.h"
#include <Mferror.h>
#include "IPinHook.h"
#include "MacrovisionKicker.h"
#include "IMPCVideoDecFilter.h"
#include "Utils.h"
#include "Variables.h"

#if (0)     // Set to 1 to activate EVR traces
#define TRACE_EVR   TRACE
#else
#define TRACE_EVR   __noop
#endif

#define MIN_FRAME_TIME 15000

enum EVR_STATS_MSG {
    MSG_MIXERIN,
    MSG_MIXEROUT
};

// Guid to tag IMFSample with a group id
static const GUID GUID_GROUP_ID = { 0x309e32cc, 0x9b23, 0x4c6c, { 0x86, 0x63, 0xcd, 0xd9, 0xad, 0x49, 0x7f, 0x8a } };
// Guid to tag IMFSample with DirectX surface index
static const GUID GUID_SURFACE_INDEX = { 0x30c8e9f6, 0x415, 0x4b81, { 0xa3, 0x15, 0x1, 0xa, 0xc6, 0xa9, 0xda, 0x19 } };


// === Helper functions
MFOffset MakeOffset(float v)
{
    MFOffset offset;
    offset.value = short(v);
    offset.fract = WORD(65536 * (v - offset.value));
    return offset;
}

MFVideoArea MakeArea(float x, float y, DWORD width, DWORD height)
{
    MFVideoArea area;
    area.OffsetX = MakeOffset(x);
    area.OffsetY = MakeOffset(y);
    area.Area.cx = width;
    area.Area.cy = height;
    return area;
}

using namespace DSObjects;

CEVRAllocatorPresenter::CEVRAllocatorPresenter(HWND hWnd, bool bFullscreen, HRESULT& hr, CString& _Error, bool isPreview)
    : CDX9AllocatorPresenter(hWnd, bFullscreen, hr, true, _Error, isPreview)
    , m_ModeratedTime(0)
    , m_ModeratedTimeLast(-1)
    , m_ModeratedClockLast(-1)
    , m_ModeratedTimer(0)
    , m_LastClockState(MFCLOCK_STATE_INVALID)
    , m_pOuterEVR(nullptr)
    , m_dwVideoAspectRatioMode(MFVideoARMode_PreservePicture)
    , m_dwVideoRenderPrefs((MFVideoRenderPrefs)0)
    , m_BorderColor(RGB(0, 0, 0))
    , m_hEvtQuit(nullptr)
    , m_bEvtQuit(0)
    , m_hEvtFlush(nullptr)
    , m_bEvtFlush(0)
    , m_fUseInternalTimer(false)
    , m_LastSetOutputRange(-1)
    , m_bPendingRenegotiate(false)
    , m_bPendingMediaFinished(false)
    , m_hThread(nullptr)
    , m_hGetMixerThread(nullptr)
    , m_hVSyncThread(nullptr)
    , m_nRenderState(Shutdown)
    , m_nCurrentGroupId(0)
    , m_bLastSampleOffsetValid(false)
    , m_LastScheduledSampleTime(-1)
    , m_LastScheduledSampleTimeFP(-1)
    , m_LastScheduledUncorrectedSampleTime(-1)
    , m_MaxSampleDuration(0)
    , m_LastSampleOffset(0)
    , m_LastPredictedSync(0)
    , m_VSyncOffsetHistoryPos(0)
    , m_nResetToken(0)
    , m_nStepCount(0)
    , m_bSignaledStarvation(false)
    , m_StarvationClock(0)
    , m_SampleFreeCallback(this, &CEVRAllocatorPresenter::OnSampleFree)
    , fnDXVA2CreateDirect3DDeviceManager9(_T("dxva2.dll"), "DXVA2CreateDirect3DDeviceManager9")
    , fnMFCreateDXSurfaceBuffer(_T("evr.dll"), "MFCreateDXSurfaceBuffer")
    , fnMFCreateVideoSampleFromSurface(_T("evr.dll"), "MFCreateVideoSampleFromSurface")
    , fnMFCreateMediaType(_T("mfplat.dll"), "MFCreateMediaType")
    , fnAvSetMmThreadCharacteristicsW(_T("avrt.dll"), "AvSetMmThreadCharacteristicsW")
    , fnAvSetMmThreadPriority(_T("avrt.dll"), "AvSetMmThreadPriority")
    , fnAvRevertMmThreadCharacteristics(_T("avrt.dll"), "AvRevertMmThreadCharacteristics")
{
    const CRenderersSettings& r = GetRenderersSettings();

    ZeroMemory(m_VSyncOffsetHistory, sizeof(m_VSyncOffsetHistory));
    ResetStats();

    if (FAILED(hr)) {
        _Error += L"DX9AllocatorPresenter failed\n";
        return;
    }

    if (!fnDXVA2CreateDirect3DDeviceManager9 || !fnMFCreateDXSurfaceBuffer || !fnMFCreateVideoSampleFromSurface || !fnMFCreateMediaType) {
        if (!fnDXVA2CreateDirect3DDeviceManager9) {
            _Error += L"Could not find DXVA2CreateDirect3DDeviceManager9 (dxva2.dll)\n";
        }
        if (!fnMFCreateDXSurfaceBuffer) {
            _Error += L"Could not find MFCreateDXSurfaceBuffer (evr.dll)\n";
        }
        if (!fnMFCreateVideoSampleFromSurface) {
            _Error += L"Could not find MFCreateVideoSampleFromSurface (evr.dll)\n";
        }
        if (!fnMFCreateMediaType) {
            _Error += L"Could not find MFCreateMediaType (mfplat.dll)\n";
        }
        hr = E_FAIL;
        return;
    }

    // Init DXVA manager
    hr = fnDXVA2CreateDirect3DDeviceManager9(&m_nResetToken, &m_pD3DManager);
    if (SUCCEEDED(hr) && m_pD3DManager) {
        hr = m_pD3DManager->ResetDevice(m_pD3DDev, m_nResetToken);
        if (FAILED(hr)) {
            _Error += L"m_pD3DManager->ResetDevice failed\n";
        }
    } else {
        _Error += L"DXVA2CreateDirect3DDeviceManager9 failed\n";
    }


    // Bufferize frame only with 3D texture!
    if (!m_bIsPreview && r.iAPSurfaceUsage == VIDRNDT_AP_TEXTURE3D) {
        m_nNbDXSurface  = std::max(std::min(r.iEvrBuffers, MAX_VIDEO_SURFACES), 4);
    } else {
        m_nNbDXSurface = 1;
    }
}

CEVRAllocatorPresenter::~CEVRAllocatorPresenter()
{
    StopWorkerThreads();    // If not already done...
    m_pMediaType  = nullptr;
    m_pClock      = nullptr;
    m_pD3DManager = nullptr;

    if (m_bHookedNewSegment) {
        UnhookNewSegment();
    }
}

void CEVRAllocatorPresenter::ResetStats()
{
    m_pcFrames       = 0;
    m_nDroppedUpdate = 0;
    m_pcFramesDrawn  = 0;
    m_piAvg          = 0;
    m_piDev          = 0;
}

HRESULT CEVRAllocatorPresenter::CheckShutdown() const
{
    if (m_nRenderState == Shutdown) {
        return MF_E_SHUTDOWN;
    } else {
        return S_OK;
    }
}

void CEVRAllocatorPresenter::StartWorkerThreads()
{
    DWORD dwThreadId;

    if (m_nRenderState == Shutdown) {
        m_hEvtQuit  = CreateEvent(nullptr, TRUE, FALSE, nullptr);
        m_hEvtFlush = CreateEvent(nullptr, TRUE, FALSE, nullptr);

        m_hThread = ::CreateThread(nullptr, 0, PresentThread, (LPVOID)this, 0, &dwThreadId);
        SetThreadPriority(m_hThread, THREAD_PRIORITY_TIME_CRITICAL);
        m_hGetMixerThread = ::CreateThread(nullptr, 0, GetMixerThreadStatic, (LPVOID)this, 0, &dwThreadId);
        SetThreadPriority(m_hGetMixerThread, THREAD_PRIORITY_HIGHEST);
        m_hVSyncThread = ::CreateThread(nullptr, 0, VSyncThreadStatic, (LPVOID)this, 0, &dwThreadId);
        SetThreadPriority(m_hVSyncThread, THREAD_PRIORITY_HIGHEST);

        m_nRenderState = Stopped;
        TRACE_EVR("EVR: Worker threads started...\n");
    }
}

void CEVRAllocatorPresenter::StopWorkerThreads()
{
    if (m_nRenderState != Shutdown) {
        SetEvent(m_hEvtFlush);
        m_bEvtFlush = true;
        SetEvent(m_hEvtQuit);
        m_bEvtQuit = true;
        if (m_hThread && WaitForSingleObject(m_hThread, 10000) == WAIT_TIMEOUT) {
            ASSERT(FALSE);
            TerminateThread(m_hThread, 0xDEAD);
        }
        if (m_hGetMixerThread && WaitForSingleObject(m_hGetMixerThread, 10000) == WAIT_TIMEOUT) {
            ASSERT(FALSE);
            TerminateThread(m_hGetMixerThread, 0xDEAD);
        }
        if (m_hVSyncThread && WaitForSingleObject(m_hVSyncThread, 10000) == WAIT_TIMEOUT) {
            ASSERT(FALSE);
            TerminateThread(m_hVSyncThread, 0xDEAD);
        }

        SAFE_CLOSE_HANDLE(m_hThread);
        SAFE_CLOSE_HANDLE(m_hGetMixerThread);
        SAFE_CLOSE_HANDLE(m_hVSyncThread);
        SAFE_CLOSE_HANDLE(m_hEvtFlush);
        SAFE_CLOSE_HANDLE(m_hEvtQuit);

        m_bEvtFlush = false;
        m_bEvtQuit  = false;

        TRACE_EVR("EVR: Worker threads stopped...\n");
    }
    m_nRenderState = Shutdown;
}

STDMETHODIMP CEVRAllocatorPresenter::CreateRenderer(IUnknown** ppRenderer)
{
    CheckPointer(ppRenderer, E_POINTER);

    *ppRenderer = nullptr;
    HRESULT hr = S_OK;

    CMacrovisionKicker* pMK  = DEBUG_NEW CMacrovisionKicker(NAME("CMacrovisionKicker"), nullptr);
    CComPtr<IUnknown>   pUnk = (IUnknown*)(INonDelegatingUnknown*)pMK;

    COuterEVR* pOuterEVR = DEBUG_NEW COuterEVR(NAME("COuterEVR"), pUnk, hr, &m_VMR9AlphaBitmap, this);
    m_pOuterEVR = pOuterEVR;

    pMK->SetInner((IUnknown*)(INonDelegatingUnknown*)pOuterEVR);
    CComQIPtr<IBaseFilter> pBF = pUnk;

    if (FAILED(hr)) {
        return E_FAIL;
    }

    // Set EVR custom presenter
    CComPtr<IMFVideoPresenter> pVP;
    CComPtr<IMFVideoRenderer>  pMFVR;
    CComQIPtr<IMFGetService, &__uuidof(IMFGetService)> pMFGS = pBF;
    CComQIPtr<IEVRFilterConfig> pConfig = pBF;
    if (!m_bIsPreview) {
        pConfig->SetNumberOfStreams(3);
    }

    hr = pMFGS->GetService(MR_VIDEO_RENDER_SERVICE, IID_PPV_ARGS(&pMFVR));

    if (SUCCEEDED(hr)) {
        hr = QueryInterface(IID_PPV_ARGS(&pVP));
    }
    if (SUCCEEDED(hr)) {
        hr = pMFVR->InitializeRenderer(nullptr, pVP);
    }

    if (SUCCEEDED(hr)) {
        if (!m_bIsPreview) {
            CComPtr<IPin> pPin = GetFirstPin(pBF);
            if (HookNewSegment((IPinC*)(IPin*)pPin)) {
                m_fUseInternalTimer = true;
                m_bHookedNewSegment = true;
            };
        }
        *ppRenderer = pBF.Detach();
    } else {
        *ppRenderer = nullptr;
    }

    return hr;
}

STDMETHODIMP_(bool) CEVRAllocatorPresenter::Paint(bool bAll)
{
    return __super::Paint(bAll);
}

STDMETHODIMP_(bool) CEVRAllocatorPresenter::Paint(IMFSample* pMFSample)
{
    CAutoLock lock(&m_RenderLock);

    m_pCurrentlyDisplayedSample = pMFSample;
    pMFSample->GetUINT32(GUID_SURFACE_INDEX, (UINT32*)&m_nCurSurface);

    auto sampleHasCurrentGroupId = [this](IMFSample * pSample) {
        UINT32 nGroupId;
        return (SUCCEEDED(pSample->GetUINT32(GUID_GROUP_ID, &nGroupId)) && nGroupId == m_nCurrentGroupId);
    };
    ASSERT(sampleHasCurrentGroupId(pMFSample));

    return Paint(true);
}

STDMETHODIMP CEVRAllocatorPresenter::NonDelegatingQueryInterface(REFIID riid, void** ppv)
{
    HRESULT hr;
    if (riid == __uuidof(IMFClockStateSink)) {
        hr = GetInterface((IMFClockStateSink*)this, ppv);
    } else if (riid == __uuidof(IMFVideoPresenter)) {
        hr = GetInterface((IMFVideoPresenter*)this, ppv);
    } else if (riid == __uuidof(IMFTopologyServiceLookupClient)) {
        hr = GetInterface((IMFTopologyServiceLookupClient*)this, ppv);
    } else if (riid == __uuidof(IMFVideoDeviceID)) {
        hr = GetInterface((IMFVideoDeviceID*)this, ppv);
    } else if (riid == __uuidof(IMFGetService)) {
        hr = GetInterface((IMFGetService*)this, ppv);
    } else if (riid == __uuidof(IMFAsyncCallback)) {
        hr = GetInterface((IMFAsyncCallback*)this, ppv);
    } else if (riid == __uuidof(IMFVideoDisplayControl)) {
        hr = GetInterface((IMFVideoDisplayControl*)this, ppv);
    } else if (riid == __uuidof(IMFVideoMixerBitmap)) {
        hr = GetInterface((IMFVideoMixerBitmap*)this, ppv);
    } else if (riid == __uuidof(IEVRTrustedVideoPlugin)) {
        hr = GetInterface((IEVRTrustedVideoPlugin*)this, ppv);
    } else if (riid == IID_IQualProp) {
        hr = GetInterface((IQualProp*)this, ppv);
    } else if (riid == __uuidof(IMFRateSupport)) {
        hr = GetInterface((IMFRateSupport*)this, ppv);
    } else if (riid == __uuidof(IDirect3DDeviceManager9))
        //      hr = GetInterface((IDirect3DDeviceManager9*)this, ppv);
    {
        hr = m_pD3DManager->QueryInterface(__uuidof(IDirect3DDeviceManager9), (void**) ppv);
    } else if (riid == __uuidof(ID3DFullscreenControl)) {
        hr = GetInterface((ID3DFullscreenControl*)this, ppv);
    } else {
        hr = __super::NonDelegatingQueryInterface(riid, ppv);
    }

    return hr;
}

// IMFClockStateSink
STDMETHODIMP CEVRAllocatorPresenter::OnClockStart(MFTIME hnsSystemTime, LONGLONG llClockStartOffset)
{
    HRESULT hr;
    CHECK_HR(CheckShutdown());

    m_nRenderState = Started;

    TRACE_EVR("EVR: OnClockStart  hnsSystemTime = %I64d,   llClockStartOffset = %I64d\n", hnsSystemTime, llClockStartOffset);
    m_ModeratedTimeLast  = -1;
    m_ModeratedClockLast = -1;

    return S_OK;
}

STDMETHODIMP CEVRAllocatorPresenter::OnClockStop(MFTIME hnsSystemTime)
{
    HRESULT hr;
    CHECK_HR(CheckShutdown());

    TRACE_EVR("EVR: OnClockStop  hnsSystemTime = %I64d\n", hnsSystemTime);
    m_nRenderState = Stopped;

    m_ModeratedClockLast = -1;
    m_ModeratedTimeLast  = -1;
    return S_OK;
}

STDMETHODIMP CEVRAllocatorPresenter::OnClockPause(MFTIME hnsSystemTime)
{
    HRESULT hr;
    CHECK_HR(CheckShutdown());

    TRACE_EVR("EVR: OnClockPause  hnsSystemTime = %I64d\n", hnsSystemTime);
    if (!m_bSignaledStarvation) {
        m_nRenderState = Paused;
    }
    m_ModeratedTimeLast  = -1;
    m_ModeratedClockLast = -1;
    return S_OK;
}

STDMETHODIMP CEVRAllocatorPresenter::OnClockRestart(MFTIME hnsSystemTime)
{
    HRESULT hr;
    CHECK_HR(CheckShutdown());

    m_nRenderState = Started;

    m_ModeratedTimeLast  = -1;
    m_ModeratedClockLast = -1;
    TRACE_EVR("EVR: OnClockRestart  hnsSystemTime = %I64d\n", hnsSystemTime);

    return S_OK;
}

STDMETHODIMP CEVRAllocatorPresenter::OnClockSetRate(MFTIME hnsSystemTime, float flRate)
{
    ASSERT(FALSE);
    return E_NOTIMPL;
}

// IBaseFilter delegate
bool CEVRAllocatorPresenter::GetState(DWORD dwMilliSecsTimeout, FILTER_STATE* State, HRESULT& _ReturnValue)
{
    CAutoLock lock(&m_SampleQueueLock);

    if (m_bSignaledStarvation) {
        size_t nSamples = std::max(m_nNbDXSurface / 2, 1);
        if ((m_ScheduledSamples.GetCount() < nSamples || m_LastSampleOffset < -m_rtTimePerFrame * 2) && !g_bNoDuration) {
            *State = (FILTER_STATE)Paused;
            _ReturnValue = VFW_S_STATE_INTERMEDIATE;
            return true;
        }
        m_bSignaledStarvation = false;
    }
    return false;
}

// IQualProp
STDMETHODIMP CEVRAllocatorPresenter::get_FramesDroppedInRenderer(int* pcFrames)
{
    *pcFrames = m_pcFrames;
    return S_OK;
}

STDMETHODIMP CEVRAllocatorPresenter::get_FramesDrawn(int* pcFramesDrawn)
{
    *pcFramesDrawn = m_pcFramesDrawn;
    return S_OK;
}

STDMETHODIMP CEVRAllocatorPresenter::get_AvgFrameRate(int* piAvgFrameRate)
{
    *piAvgFrameRate = (int)(m_fAvrFps * 100);
    return S_OK;
}

STDMETHODIMP CEVRAllocatorPresenter::get_Jitter(int* iJitter)
{
    *iJitter = (int)((m_fJitterStdDev / 10000.0) + 0.5);
    return S_OK;
}

STDMETHODIMP CEVRAllocatorPresenter::get_AvgSyncOffset(int* piAvg)
{
    *piAvg = (int)((m_fSyncOffsetAvr / 10000.0) + 0.5);
    return S_OK;
}

STDMETHODIMP CEVRAllocatorPresenter::get_DevSyncOffset(int* piDev)
{
    *piDev = (int)((m_fSyncOffsetStdDev / 10000.0) + 0.5);
    return S_OK;
}

// IMFRateSupport

STDMETHODIMP CEVRAllocatorPresenter::GetSlowestRate(MFRATE_DIRECTION eDirection, BOOL fThin, float* pflRate)
{
    // TODO : not finished...
    *pflRate = 0;
    return S_OK;
}

STDMETHODIMP CEVRAllocatorPresenter::GetFastestRate(MFRATE_DIRECTION eDirection, BOOL fThin, float* pflRate)
{
    HRESULT hr = S_OK;
    float fMaxRate = 0.0f;

    CAutoLock lock(this);

    CheckPointer(pflRate, E_POINTER);
    CHECK_HR(CheckShutdown());

    // Get the maximum forward rate.
    fMaxRate = GetMaxRate(fThin);

    // For reverse playback, swap the sign.
    if (eDirection == MFRATE_REVERSE) {
        fMaxRate = -fMaxRate;
    }

    *pflRate = fMaxRate;

    return hr;
}

STDMETHODIMP CEVRAllocatorPresenter::IsRateSupported(BOOL fThin, float flRate, float* pflNearestSupportedRate)
{
    // fRate can be negative for reverse playback.
    // pfNearestSupportedRate can be NULL.

    CAutoLock lock(this);

    HRESULT hr = S_OK;
    float fMaxRate = 0.0f;
    float fNearestRate = flRate;   // Default.

    CheckPointer(pflNearestSupportedRate, E_POINTER);
    CHECK_HR(CheckShutdown());

    // Find the maximum forward rate.
    fMaxRate = GetMaxRate(fThin);

    if (fabsf(flRate) > fMaxRate) {
        // The (absolute) requested rate exceeds the maximum rate.
        hr = MF_E_UNSUPPORTED_RATE;

        // The nearest supported rate is fMaxRate.
        fNearestRate = fMaxRate;
        if (flRate < 0) {
            // For reverse playback, swap the sign.
            fNearestRate = -fNearestRate;
        }
    }

    // Return the nearest supported rate if the caller requested it.
    if (pflNearestSupportedRate != nullptr) {
        *pflNearestSupportedRate = fNearestRate;
    }

    return hr;
}

float CEVRAllocatorPresenter::GetMaxRate(BOOL bThin)
{
    float fMaxRate = FLT_MAX;  // Default.
    UINT32 fpsNumerator = 0, fpsDenominator = 0;

    if (!bThin && (m_pMediaType != nullptr)) {
        // Non-thinned: Use the frame rate and monitor refresh rate.

        // Frame rate:
        MFGetAttributeRatio(m_pMediaType, MF_MT_FRAME_RATE,
                            &fpsNumerator, &fpsDenominator);

        // Monitor refresh rate:
        UINT MonitorRateHz = m_refreshRate; // D3DDISPLAYMODE

        if (fpsDenominator && fpsNumerator && MonitorRateHz) {
            // Max Rate = Refresh Rate / Frame Rate
            fMaxRate = (float)MulDiv(MonitorRateHz, fpsDenominator, fpsNumerator);
        }
    }
    return fMaxRate;
}

void CEVRAllocatorPresenter::CompleteFrameStep(bool bCancel)
{
    if (m_nStepCount > 0) {
        if (bCancel || (m_nStepCount == 1)) {
            m_pSink->Notify(EC_STEP_COMPLETE, bCancel ? TRUE : FALSE, 0);
            m_nStepCount = 0;
        } else {
            m_nStepCount--;
        }
    }
}

// IMFVideoPresenter
STDMETHODIMP CEVRAllocatorPresenter::ProcessMessage(MFVP_MESSAGE_TYPE eMessage, ULONG_PTR ulParam)
{
    HRESULT hr = S_OK;
    CHECK_HR(CheckShutdown());

    switch (eMessage) {
        case MFVP_MESSAGE_BEGINSTREAMING:           // The EVR switched from stopped to paused. The presenter should allocate resources
            m_nRenderState = Paused;
            ResetStats();
            TRACE_EVR("EVR: MFVP_MESSAGE_BEGINSTREAMING\n");
            break;

        case MFVP_MESSAGE_CANCELSTEP:               // Cancels a frame step
            TRACE_EVR("EVR: MFVP_MESSAGE_CANCELSTEP\n");
            CompleteFrameStep(true);
            break;

        case MFVP_MESSAGE_ENDOFSTREAM:              // All input streams have ended.
            TRACE_EVR("EVR: MFVP_MESSAGE_ENDOFSTREAM\n");
            m_bPendingMediaFinished = true;
            break;

        case MFVP_MESSAGE_ENDSTREAMING:             // The EVR switched from running or paused to stopped. The presenter should free resources
            m_nRenderState = Stopped;
            TRACE_EVR("EVR: MFVP_MESSAGE_ENDSTREAMING\n");
            break;

        case MFVP_MESSAGE_FLUSH:                    // The presenter should discard any pending samples
            SetEvent(m_hEvtFlush);
            m_bEvtFlush = true;
            TRACE_EVR("EVR: MFVP_MESSAGE_FLUSH\n");
            while (WaitForSingleObject(m_hEvtFlush, 1) == WAIT_OBJECT_0) {
                ;
            }
            break;

        case MFVP_MESSAGE_INVALIDATEMEDIATYPE:      // The mixer's output format has changed. The EVR will initiate format negotiation, as described previously
            /*
                1) The EVR sets the media type on the reference stream.
                2) The EVR calls IMFVideoPresenter::ProcessMessage on the presenter with the MFVP_MESSAGE_INVALIDATEMEDIATYPE message.
                3) The presenter sets the media type on the mixer's output stream.
                4) The EVR sets the media type on the substreams.
            */
            m_bPendingRenegotiate = true;
            while (m_bPendingRenegotiate) {
                Sleep(1);
            }
            break;

        case MFVP_MESSAGE_PROCESSINPUTNOTIFY:       // One input stream on the mixer has received a new sample
            //      GetImageFromMixer();
            break;

        case MFVP_MESSAGE_STEP:                     // Requests a frame step.
            TRACE_EVR("EVR: MFVP_MESSAGE_STEP\n");
            m_nStepCount = (int)ulParam;
            hr = S_OK;
            break;

        default:
            ASSERT(FALSE);
            break;
    }
    return hr;
}

HRESULT CEVRAllocatorPresenter::IsMediaTypeSupported(IMFMediaType* pMixerType)
{
    HRESULT hr;

    // We support only video types
    GUID MajorType;
    hr = pMixerType->GetMajorType(&MajorType);

    if (SUCCEEDED(hr)) {
        if (MajorType != MFMediaType_Video) {
            hr = MF_E_INVALIDMEDIATYPE;
        }
    }

    // We support only progressive formats
    MFVideoInterlaceMode InterlaceMode = MFVideoInterlace_Unknown;

    if (SUCCEEDED(hr)) {
        hr = pMixerType->GetUINT32(MF_MT_INTERLACE_MODE, (UINT32*)&InterlaceMode);
    }

    if (SUCCEEDED(hr)) {
        if (InterlaceMode != MFVideoInterlace_Progressive) {
            hr = MF_E_INVALIDMEDIATYPE;
        }
    }

    // Check whether we support the surface format
    int Merit = 0;

    if (SUCCEEDED(hr)) {
        hr = GetMediaTypeMerit(pMixerType, &Merit);
    }

    if (SUCCEEDED(hr)) {
        if (Merit == 0) {
            hr = MF_E_INVALIDMEDIATYPE;
        }
    }

    return hr;
}

HRESULT CEVRAllocatorPresenter::CreateOptimalOutputType(IMFMediaType* pMixerProposedType, IMFMediaType* pMixerInputType, IMFMediaType** ppType)
{
    HRESULT hr;
    IMFMediaType* pOptimalMediaType;

    CHECK_HR(fnMFCreateMediaType(&pOptimalMediaType));
    CHECK_HR(pMixerProposedType->CopyAllItems(pOptimalMediaType));

    const GUID colorAttributes[] = {
        MF_MT_VIDEO_LIGHTING,
        MF_MT_VIDEO_PRIMARIES,
        MF_MT_TRANSFER_FUNCTION,
        MF_MT_YUV_MATRIX,
        MF_MT_VIDEO_CHROMA_SITING
    };

    auto copyAttribute = [](IMFAttributes * pFrom, IMFAttributes * pTo, REFGUID guidKey) {
        PROPVARIANT val;
        HRESULT hr = pFrom->GetItem(guidKey, &val);

        if (SUCCEEDED(hr)) {
            hr = pTo->SetItem(guidKey, val);
            PropVariantClear(&val);
        } else if (hr == MF_E_ATTRIBUTENOTFOUND) {
            hr = pTo->DeleteItem(guidKey);
        }
        return hr;
    };

    for (REFGUID guidKey : colorAttributes) {
        if (FAILED(hr = copyAttribute(pMixerInputType, pOptimalMediaType, guidKey))) {
            TRACE_EVR(_T("Copying color attribute %s failed: 0x%08x\n"), CComBSTR(guidKey), hr);
        }
    }

    pOptimalMediaType->SetUINT32(MF_MT_PAN_SCAN_ENABLED, 0);

    const CRenderersSettings& r = GetRenderersSettings();

    UINT32 nominalRange;
    if (SUCCEEDED(pMixerInputType->GetUINT32(MF_MT_VIDEO_NOMINAL_RANGE, &nominalRange))
            && nominalRange == MFNominalRange_0_255) {
        // EVR mixer always assumes 16-235 input. Bug?
        // To keep untouched 0-255 range and avoid unwanted expansion we need to request 16-235 as output.
        // To get 16-235 output we need to request 48-208 as output.
        nominalRange = (r.m_AdvRendSets.iEVROutputRange == 1) ? MFNominalRange_48_208 : MFNominalRange_16_235;
    } else {
        nominalRange = (r.m_AdvRendSets.iEVROutputRange == 1) ? MFNominalRange_16_235 : MFNominalRange_0_255;
    }
    pOptimalMediaType->SetUINT32(MF_MT_VIDEO_NOMINAL_RANGE, nominalRange);
    m_LastSetOutputRange = r.m_AdvRendSets.iEVROutputRange;

    ULARGE_INTEGER ui64Size;
    pOptimalMediaType->GetUINT64(MF_MT_FRAME_SIZE, &ui64Size.QuadPart);

    CSize videoSize((LONG)ui64Size.HighPart, (LONG)ui64Size.LowPart);
    MFVideoArea Area = MakeArea(0, 0, videoSize.cx, videoSize.cy);
    pOptimalMediaType->SetBlob(MF_MT_GEOMETRIC_APERTURE, (UINT8*)&Area, sizeof(MFVideoArea));

    ULARGE_INTEGER ui64AspectRatio;
    pOptimalMediaType->GetUINT64(MF_MT_PIXEL_ASPECT_RATIO, &ui64AspectRatio.QuadPart);

    UINT64 ui64ARx = UINT64(ui64AspectRatio.HighPart) * ui64Size.HighPart;
    UINT64 ui64ARy = UINT64(ui64AspectRatio.LowPart)  * ui64Size.LowPart;
    UINT64 gcd = GCD(ui64ARx, ui64ARy);
    if (gcd > 1) {
        ui64ARx /= gcd;
        ui64ARy /= gcd;
    }

    CSize aspectRatio((LONG)ui64ARx, (LONG)ui64ARy);
    if (videoSize != m_nativeVideoSize || aspectRatio != m_aspectRatio) {
        SetVideoSize(videoSize, aspectRatio);

        // Notify the graph about the change
        if (m_pSink) {
            m_pSink->Notify(EC_VIDEO_SIZE_CHANGED, MAKELPARAM(m_nativeVideoSize.cx, m_nativeVideoSize.cy), 0);
        }
    }

    *ppType = pOptimalMediaType;
    (*ppType)->AddRef();

    return hr;
}

HRESULT CEVRAllocatorPresenter::SetMediaType(IMFMediaType* pType)
{
    HRESULT hr = S_OK;
    AM_MEDIA_TYPE* pAMMedia = nullptr;

    CHECK_HR(CheckShutdown());

    if (!pType) {
        // Release
        RemoveAllSamples();
        DeleteSurfaces();
        CAutoLock lock(&m_MediaTypeLock);
        m_pMediaType = nullptr;
        return hr;
    }

    DWORD dwFlags = 0;
    if (m_pMediaType && m_pMediaType->IsEqual(pType, &dwFlags) == S_OK) {
        // Nothing to do
        return hr;
    }

    CHECK_HR(pType->GetRepresentation(FORMAT_VideoInfo2, (void**)&pAMMedia));

    hr = InitializeDevice(pType);
    if (SUCCEEDED(hr)) {
        CAutoLock lock(&m_MediaTypeLock);
        m_pMediaType = pType;

        CString strTemp = GetMediaTypeName(pAMMedia->subtype);
        strTemp.Replace(L"MEDIASUBTYPE_", L"");
        CString strTemp1 = GetMediaTypeFormatDesc(pType);
        strTemp1.Replace(L"D3DFMT_", L"");
        m_strStatsMsg[MSG_MIXEROUT].Format(L"Mixer output : %-10s    Type %-10s", strTemp.GetString(), strTemp1.GetString());
    }

    pType->FreeRepresentation(FORMAT_VideoInfo2, (void*)pAMMedia);

    return hr;
}

HRESULT CEVRAllocatorPresenter::GetMediaTypeFourCC(IMFMediaType* pType, DWORD* pFourCC)
{
    CheckPointer(pFourCC, E_POINTER);

    HRESULT hr = S_OK;
    GUID guidSubType = GUID_NULL;

    if (SUCCEEDED(hr)) {
        hr = pType->GetGUID(MF_MT_SUBTYPE, &guidSubType);
    }

    if (SUCCEEDED(hr)) {
        *pFourCC = guidSubType.Data1;
    }

    return hr;
}

HRESULT CEVRAllocatorPresenter::GetMediaTypeMerit(IMFMediaType* pType, int* pMerit)
{
    DWORD Format;
    HRESULT hr = GetMediaTypeFourCC(pType, &Format);

    if (SUCCEEDED(hr)) {
        switch (Format) {
            case FCC('AI44'):   // Palettized, 4:4:4
                *pMerit = 31;
                break;
            case FCC('YVU9'):   // 8-bit, 16:1:1
                *pMerit = 30;
                break;
            case FCC('NV11'):   // 8-bit, 4:1:1
                *pMerit = 29;
                break;
            case FCC('Y41P'):
                *pMerit = 28;
                break;
            case FCC('Y41T'):
                *pMerit = 27;
                break;
            case FCC('P016'):   // 4:2:0
                *pMerit = 26;
                break;
            case FCC('P010'):
                *pMerit = 25;
                break;
            case FCC('IMC1'):
                *pMerit = 24;
                break;
            case FCC('IMC3'):
                *pMerit = 23;
                break;
            case FCC('IMC2'):
                *pMerit = 22;
                break;
            case FCC('IMC4'):
                *pMerit = 21;
                break;
            case FCC('YV12'):
                *pMerit = 20;
                break;
            case FCC('NV12'):
                *pMerit = 19;
                break;
            case FCC('I420'):
                *pMerit = 18;
                break;
            case FCC('IYUV'):
                *pMerit = 17;
                break;
            case FCC('Y216'):   // 4:2:2
                *pMerit = 16;
                break;
            case FCC('v216'):
                *pMerit = 15;
                break;
            case FCC('P216'):
                *pMerit = 14;
                break;
            case FCC('Y210'):
                *pMerit = 13;
                break;
            case FCC('v210'):
                *pMerit = 12;
                break;
            case FCC('P210'):
                *pMerit = 11;
                break;
            case FCC('YUY2'):
                *pMerit = 10;
                break;
            case FCC('UYVY'):
                *pMerit = 9;
                break;
            case FCC('Y42T'):
                *pMerit = 8;
                break;
            case FCC('YVYU'):
                *pMerit = 7;
                break;
            case FCC('Y416'):   // 4:4:4
                *pMerit = 6;
                break;
            case FCC('Y410'):
                *pMerit = 5;
                break;
            case FCC('v410'):
                *pMerit = 4;
                break;
            case FCC('AYUV'):
                *pMerit = 3;
                break;
            case D3DFMT_X8R8G8B8:
                if (m_bForceInputHighColorResolution) {
                    *pMerit = 63;
                } else {
                    *pMerit = 1;
                }
                break;
            case D3DFMT_A8R8G8B8:   // an accepted format, but fails on most surface types
            case D3DFMT_A8B8G8R8:
            case D3DFMT_X8B8G8R8:
            case D3DFMT_R8G8B8:
            case D3DFMT_R5G6B5:
            case D3DFMT_X1R5G5B5:
            case D3DFMT_A1R5G5B5:
            case D3DFMT_A4R4G4B4:
            case D3DFMT_R3G3B2:
            case D3DFMT_A8R3G3B2:
            case D3DFMT_X4R4G4B4:
            case D3DFMT_A8P8:
            case D3DFMT_P8:
                *pMerit = 0;
                break;
            default:
                *pMerit = 2;
                break;
        }
    }

    return hr;
}

LPCTSTR FindD3DFormat(const D3DFORMAT Format);

LPCTSTR CEVRAllocatorPresenter::GetMediaTypeFormatDesc(IMFMediaType* pMediaType)
{
    D3DFORMAT Format = D3DFMT_UNKNOWN;
    GetMediaTypeFourCC(pMediaType, (DWORD*)&Format);
    return FindD3DFormat(Format);
}

HRESULT CEVRAllocatorPresenter::RenegotiateMediaType()
{
    HRESULT hr = S_OK;

    CComPtr<IMFMediaType> pMixerType;
    CComPtr<IMFMediaType> pMixerInputType;
    CComPtr<IMFMediaType> pType;

    if (!m_pMixer) {
        return MF_E_INVALIDREQUEST;
    }

    CInterfaceArray<IMFMediaType> ValidMixerTypes;

    // Get the mixer's input type
    hr = m_pMixer->GetInputCurrentType(0, &pMixerInputType);
    if (SUCCEEDED(hr)) {
        AM_MEDIA_TYPE* pMT;
        hr = pMixerInputType->GetRepresentation(FORMAT_VideoInfo2, (void**)&pMT);
        if (SUCCEEDED(hr)) {
            m_inputMediaType = *pMT;
            pMixerInputType->FreeRepresentation(FORMAT_VideoInfo2, pMT);
        }
    }

    // Loop through all of the mixer's proposed output types.
    DWORD iTypeIndex = 0;
    while ((hr != MF_E_NO_MORE_TYPES)) {
        pMixerType   = nullptr;
        pType        = nullptr;

        // Step 1. Get the next media type supported by mixer.
        hr = m_pMixer->GetOutputAvailableType(0, iTypeIndex++, &pMixerType);
        if (FAILED(hr)) {
            break;
        }

        // Step 2. Check if we support this media type.
        if (SUCCEEDED(hr)) {
            hr = IsMediaTypeSupported(pMixerType);
        }

        if (SUCCEEDED(hr)) {
            hr = CreateOptimalOutputType(pMixerType, pMixerInputType, &pType);
        }

        // Step 4. Check if the mixer will accept this media type.
        if (SUCCEEDED(hr)) {
            hr = m_pMixer->SetOutputType(0, pType, MFT_SET_TYPE_TEST_ONLY);
        }

        int Merit = 0;
        if (SUCCEEDED(hr)) {
            hr = GetMediaTypeMerit(pType, &Merit);
        }

        if (SUCCEEDED(hr)) {
            size_t nTypes = ValidMixerTypes.GetCount();
            size_t iInsertPos = 0;
            for (size_t i = 0; i < nTypes; ++i) {
                int ThisMerit;
                GetMediaTypeMerit(ValidMixerTypes[i], &ThisMerit);

                if (Merit > ThisMerit) {
                    iInsertPos = i;
                    break;
                } else {
                    iInsertPos = i + 1;
                }
            }

            ValidMixerTypes.InsertAt(iInsertPos, pType);
        }
    }


    size_t nValidTypes = ValidMixerTypes.GetCount();
#ifdef _DEBUG
    for (size_t i = 0; i < nValidTypes; ++i) {
        // Step 3. Adjust the mixer's type to match our requirements.
        pType = ValidMixerTypes[i];
        TRACE_EVR("EVR: Valid mixer output type: %ws\n", GetMediaTypeFormatDesc(pType));
    }
#endif
    for (size_t i = 0; i < nValidTypes; ++i) {
        // Step 4. Adjust the mixer's type to match our requirements.
        pType = ValidMixerTypes[i];

        TRACE_EVR("EVR: Trying mixer output type: %ws\n", GetMediaTypeFormatDesc(pType));

        // Step 5. Try to set the media type on ourselves.
        hr = SetMediaType(pType);

        // Step 6. Set output media type on mixer.
        if (SUCCEEDED(hr)) {
            hr = m_pMixer->SetOutputType(0, pType, 0);

            // If something went wrong, clear the media type.
            if (FAILED(hr)) {
                SetMediaType(nullptr);
            } else {
                break;
            }
        }
    }

    pMixerType = nullptr;
    pType = nullptr;
    return hr;
}

bool CEVRAllocatorPresenter::GetImageFromMixer()
{
    MFT_OUTPUT_DATA_BUFFER dataBuffer;
    HRESULT hr = S_OK;
    DWORD dwStatus;
    REFERENCE_TIME nsSampleTime;
    LONGLONG llClockBefore = 0;
    LONGLONG llClockAfter = 0;
    LONGLONG llMixerLatency;
    UINT dwSurface;

    bool bDoneSomething = false;

    auto sampleHasCurrentGroupId = [this](IMFSample * pSample) {
        UINT32 nGroupId;
        return (SUCCEEDED(pSample->GetUINT32(GUID_GROUP_ID, &nGroupId)) && nGroupId == m_nCurrentGroupId);
    };

    while (SUCCEEDED(hr)) {
        CComPtr<IMFSample> pSample;

        if (FAILED(GetFreeSample(&pSample))) {
            break;
        }

        ZeroMemory(&dataBuffer, sizeof(dataBuffer));
        dataBuffer.pSample = pSample;
        pSample->GetUINT32(GUID_SURFACE_INDEX, &dwSurface);
        ASSERT(sampleHasCurrentGroupId(pSample));

        {
            llClockBefore = GetRenderersData()->GetPerfCounter();
            if (!m_pMixer) {
                ASSERT(FALSE);
                break;
            }
            hr = m_pMixer->ProcessOutput(0, 1, &dataBuffer, &dwStatus);
            llClockAfter = GetRenderersData()->GetPerfCounter();
        }

        if (hr == MF_E_TRANSFORM_NEED_MORE_INPUT) {
            AddToFreeList(pSample, false);
            pSample = nullptr; // The sample should not be used after being queued
            // Important: Release any events returned from the ProcessOutput method.
            SAFE_RELEASE(dataBuffer.pEvents);
            break;
        }

        if (m_pSink) {
            //CAutoLock autolock(this); We shouldn't need to lock here, m_pSink is thread safe
            llMixerLatency = llClockAfter - llClockBefore;
            m_pSink->Notify(EC_PROCESSING_LATENCY, (LONG_PTR)&llMixerLatency, 0);
        }

        pSample->GetSampleTime(&nsSampleTime);
        REFERENCE_TIME nsDuration;
        pSample->GetSampleDuration(&nsDuration);

        if (GetRenderersData()->m_bTearingTest) {
            RECT rcTearing;

            rcTearing.left   = m_nTearingPos;
            rcTearing.top    = 0;
            rcTearing.right  = rcTearing.left + 4;
            rcTearing.bottom = m_nativeVideoSize.cy;
            m_pD3DDev->ColorFill(m_pVideoSurface[dwSurface], &rcTearing, D3DCOLOR_ARGB(255, 255, 0, 0));

            rcTearing.left  = (rcTearing.right + 15) % m_nativeVideoSize.cx;
            rcTearing.right = rcTearing.left + 4;
            m_pD3DDev->ColorFill(m_pVideoSurface[dwSurface], &rcTearing, D3DCOLOR_ARGB(255, 255, 0, 0));
            m_nTearingPos = (m_nTearingPos + 7) % m_nativeVideoSize.cx;
        }

        TRACE_EVR("EVR: Get from Mixer : %u  (%I64d) (%I64d)\n", dwSurface, nsSampleTime, m_rtTimePerFrame ? nsSampleTime / m_rtTimePerFrame : 0);

        if (SUCCEEDED(TrackSample(pSample))) {
            AddToScheduledList(pSample, false);
            pSample = nullptr; // The sample should not be used after being queued
            bDoneSomething = true;
        } else {
            ASSERT(FALSE);
        }

        // Important: Release any events returned from the ProcessOutput method.
        SAFE_RELEASE(dataBuffer.pEvents);

        if (m_rtTimePerFrame == 0) {
            break;
        }
    }

    return bDoneSomething;
}

STDMETHODIMP CEVRAllocatorPresenter::GetCurrentMediaType(__deref_out  IMFVideoMediaType** ppMediaType)
{
    HRESULT hr = S_OK;
    CAutoLock lock(&m_MediaTypeLock);

    CheckPointer(ppMediaType, E_POINTER);
    CHECK_HR(CheckShutdown());

    if (!m_pMediaType) {
        return MF_E_NOT_INITIALIZED;
    }

    CHECK_HR(m_pMediaType->QueryInterface(IID_PPV_ARGS(ppMediaType)));

    return hr;
}

// IMFTopologyServiceLookupClient
STDMETHODIMP CEVRAllocatorPresenter::InitServicePointers(/* [in] */ __in  IMFTopologyServiceLookup* pLookup)
{
    HRESULT hr = S_OK;
    DWORD dwObjects = 1;

    CAutoLock cThreadsLock(&m_ThreadsLock);

    TRACE_EVR("EVR: CEVRAllocatorPresenter::InitServicePointers\n");
    CHECK_HR(pLookup->LookupService(MF_SERVICE_LOOKUP_GLOBAL, 0, MR_VIDEO_MIXER_SERVICE,
                                    IID_PPV_ARGS(&m_pMixer), &dwObjects));

    CHECK_HR(pLookup->LookupService(MF_SERVICE_LOOKUP_GLOBAL, 0, MR_VIDEO_RENDER_SERVICE,
                                    IID_PPV_ARGS(&m_pSink), &dwObjects));

    pLookup->LookupService(MF_SERVICE_LOOKUP_GLOBAL, 0, MR_VIDEO_RENDER_SERVICE,
                           IID_PPV_ARGS(&m_pClock), &dwObjects);


    StartWorkerThreads();
    return hr;
}

STDMETHODIMP CEVRAllocatorPresenter::ReleaseServicePointers()
{
    TRACE_EVR("EVR: CEVRAllocatorPresenter::ReleaseServicePointers\n");
    CAutoLock cThreadsLock(&m_ThreadsLock);

    StopWorkerThreads();

    m_pMixer.Release();
    m_pSink.Release();
    m_pClock.Release();
    return S_OK;
}

// IMFVideoDeviceID
STDMETHODIMP CEVRAllocatorPresenter::GetDeviceID(/* [out] */ __out  IID* pDeviceID)
{
    CheckPointer(pDeviceID, E_POINTER);
    *pDeviceID = IID_IDirect3DDevice9;
    return S_OK;
}

// IMFGetService
STDMETHODIMP CEVRAllocatorPresenter::GetService(/* [in] */ __RPC__in REFGUID guidService,
                                                           /* [in] */ __RPC__in REFIID riid,
                                                           /* [iid_is][out] */ __RPC__deref_out_opt LPVOID* ppvObject)
{
    if (guidService == MR_VIDEO_RENDER_SERVICE) {
        return NonDelegatingQueryInterface(riid, ppvObject);
    } else if (guidService == MR_VIDEO_ACCELERATION_SERVICE) {
        return m_pD3DManager->QueryInterface(__uuidof(IDirect3DDeviceManager9), (void**) ppvObject);
    }

    return E_NOINTERFACE;
}

// IMFAsyncCallback
STDMETHODIMP CEVRAllocatorPresenter::GetParameters(/* [out] */ __RPC__out DWORD* pdwFlags, /* [out] */ __RPC__out DWORD* pdwQueue)
{
    return E_NOTIMPL;
}

STDMETHODIMP CEVRAllocatorPresenter::Invoke(/* [in] */ __RPC__in_opt IMFAsyncResult* pAsyncResult)
{
    return E_NOTIMPL;
}

// IMFVideoDisplayControl
STDMETHODIMP CEVRAllocatorPresenter::GetNativeVideoSize(SIZE* pszVideo, SIZE* pszARVideo)
{
    if (pszVideo) {
        pszVideo->cx = m_nativeVideoSize.cx;
        pszVideo->cy = m_nativeVideoSize.cy;
    }
    if (pszARVideo) {
        pszARVideo->cx  = m_aspectRatio.cx;
        pszARVideo->cy  = m_aspectRatio.cy;
    }
    return S_OK;
}

STDMETHODIMP CEVRAllocatorPresenter::GetIdealVideoSize(SIZE* pszMin, SIZE* pszMax)
{
    if (pszMin) {
        pszMin->cx = 1;
        pszMin->cy = 1;
    }

    if (pszMax) {
        D3DDISPLAYMODE d3ddm;
        ZeroMemory(&d3ddm, sizeof(d3ddm));

        if (SUCCEEDED(m_pD3D->GetAdapterDisplayMode(GetAdapter(m_pD3D), &d3ddm))) {
            pszMax->cx = d3ddm.Width;
            pszMax->cy = d3ddm.Height;
        }
    }

    return S_OK;
}

STDMETHODIMP CEVRAllocatorPresenter::SetVideoPosition(const MFVideoNormalizedRect* pnrcSource, const LPRECT prcDest)
{
    return S_OK;
}

STDMETHODIMP CEVRAllocatorPresenter::GetVideoPosition(MFVideoNormalizedRect* pnrcSource, LPRECT prcDest)
{
    // Always all source rectangle ?
    if (pnrcSource) {
        pnrcSource->left   = 0.0;
        pnrcSource->top    = 0.0;
        pnrcSource->right  = 1.0;
        pnrcSource->bottom = 1.0;
    }

    if (prcDest) {
        memcpy(prcDest, &m_videoRect, sizeof(m_videoRect));     //GetClientRect (m_hWnd, prcDest);
    }

    return S_OK;
}

STDMETHODIMP CEVRAllocatorPresenter::SetAspectRatioMode(DWORD dwAspectRatioMode)
{
    m_dwVideoAspectRatioMode = (MFVideoAspectRatioMode)dwAspectRatioMode;
    return S_OK;
}

STDMETHODIMP CEVRAllocatorPresenter::GetAspectRatioMode(DWORD* pdwAspectRatioMode)
{
    CheckPointer(pdwAspectRatioMode, E_POINTER);
    *pdwAspectRatioMode = m_dwVideoAspectRatioMode;
    return S_OK;
}

STDMETHODIMP CEVRAllocatorPresenter::SetVideoWindow(HWND hwndVideo)
{
    if (m_hWnd != hwndVideo) {
        CAutoLock lock(this);
        CAutoLock lock2(&m_ImageProcessingLock);
        CAutoLock cRenderLock(&m_RenderLock);

        m_hWnd = hwndVideo;
        m_bPendingResetDevice = true;
        if (m_pSink) {
            m_pSink->Notify(EC_DISPLAY_CHANGED, 0, 0);
        }
        SendResetRequest();
    }
    return S_OK;
}

STDMETHODIMP CEVRAllocatorPresenter::GetVideoWindow(HWND* phwndVideo)
{
    CheckPointer(phwndVideo, E_POINTER);
    *phwndVideo = m_hWnd;
    return S_OK;
}

STDMETHODIMP CEVRAllocatorPresenter::RepaintVideo()
{
    Paint(true);
    return S_OK;
}

STDMETHODIMP CEVRAllocatorPresenter::GetCurrentImage(BITMAPINFOHEADER *pBih, BYTE **pDib, DWORD *pcbDib, LONGLONG *pTimeStamp)
{
    if (!pBih || !pDib || !pcbDib) {
        return E_POINTER;
    }
    CheckPointer(m_pD3DDevEx, E_ABORT);

    HRESULT hr = S_OK;
    const unsigned width  = m_windowRect.Width();
    const unsigned height = m_windowRect.Height();
    const unsigned len = width * height * 4;

    memset(pBih, 0, sizeof(BITMAPINFOHEADER));
    pBih->biSize      = sizeof(BITMAPINFOHEADER);
    pBih->biWidth     = width;
    pBih->biHeight    = height;
    pBih->biBitCount  = 32;
    pBih->biPlanes    = 1;
    pBih->biSizeImage = DIBSIZE(*pBih);

    BYTE* p = (BYTE*)CoTaskMemAlloc(len); // only this allocator can be used
    if (!p) {
        return E_OUTOFMEMORY;
    }

    CComPtr<IDirect3DSurface9> pBackBuffer;
    CComPtr<IDirect3DSurface9> pDestSurface;
    D3DLOCKED_RECT r;
    if (FAILED(hr = m_pD3DDevEx->GetBackBuffer(0, 0, D3DBACKBUFFER_TYPE_MONO, &pBackBuffer))
            || FAILED(hr = m_pD3DDevEx->CreateRenderTarget(width, height, D3DFMT_X8R8G8B8, D3DMULTISAMPLE_NONE, 0, TRUE, &pDestSurface, nullptr))
            || (FAILED(hr = m_pD3DDevEx->StretchRect(pBackBuffer, m_windowRect, pDestSurface, nullptr, D3DTEXF_NONE)))
            || (FAILED(hr = pDestSurface->LockRect(&r, nullptr, D3DLOCK_READONLY)))) {
        CString Error = GetWindowsErrorMessage(hr, nullptr);
        TRACE_EVR(L"CEVRAllocatorPresenter::GetCurrentImage failed : %s", S_OK == hr ? L"S_OK" : Error.GetBuffer());
        CoTaskMemFree(p);
        return hr;
    }

    RetrieveBitmapData(width, height, 32, p, (BYTE*)r.pBits, r.Pitch);

    pDestSurface->UnlockRect();

    *pDib = p;
    *pcbDib = len;

    return S_OK;
}

STDMETHODIMP CEVRAllocatorPresenter::SetBorderColor(COLORREF Clr)
{
    m_BorderColor = Clr;
    return S_OK;
}

STDMETHODIMP CEVRAllocatorPresenter::GetBorderColor(COLORREF* pClr)
{
    CheckPointer(pClr, E_POINTER);
    *pClr = m_BorderColor;
    return S_OK;
}

STDMETHODIMP CEVRAllocatorPresenter::SetRenderingPrefs(DWORD dwRenderFlags)
{
    m_dwVideoRenderPrefs = (MFVideoRenderPrefs)dwRenderFlags;
    return S_OK;
}

STDMETHODIMP CEVRAllocatorPresenter::GetRenderingPrefs(DWORD* pdwRenderFlags)
{
    CheckPointer(pdwRenderFlags, E_POINTER);
    *pdwRenderFlags = m_dwVideoRenderPrefs;
    return S_OK;
}

STDMETHODIMP CEVRAllocatorPresenter::SetFullscreen(BOOL fFullscreen)
{
    m_bIsFullscreen = !!fFullscreen;
    return S_OK;
}

STDMETHODIMP CEVRAllocatorPresenter::GetFullscreen(BOOL* pfFullscreen)
{
    CheckPointer(pfFullscreen, E_POINTER);
    *pfFullscreen = m_bIsFullscreen;
    return S_OK;
}

// IMFVideoMixerBitmap
STDMETHODIMP CEVRAllocatorPresenter::ClearAlphaBitmap()
{
    CAutoLock cRenderLock(&m_RenderLock);
    m_bAlphaBitmapEnable = false;

    return S_OK;
}

STDMETHODIMP CEVRAllocatorPresenter::GetAlphaBitmapParameters(MFVideoAlphaBitmapParams *pBmpParms)
{
    CheckPointer(pBmpParms, E_POINTER);
    CAutoLock cRenderLock(&m_RenderLock);

    if (m_bAlphaBitmapEnable && m_pAlphaBitmapTexture) {
        *pBmpParms = m_AlphaBitmapParams; // formal implementation, don't believe it
        return S_OK;
    } else {
        return MF_E_NOT_INITIALIZED;
    }
}

STDMETHODIMP CEVRAllocatorPresenter::SetAlphaBitmap(const MFVideoAlphaBitmap *pBmpParms)
{
    CheckPointer(pBmpParms, E_POINTER);
    CAutoLock cRenderLock(&m_RenderLock);

    CheckPointer(m_pD3DDevEx, E_ABORT);
    HRESULT hr = S_OK;

    if (pBmpParms->GetBitmapFromDC && pBmpParms->bitmap.hdc) {
        HBITMAP hBitmap = (HBITMAP)GetCurrentObject(pBmpParms->bitmap.hdc, OBJ_BITMAP);
        if (!hBitmap) {
            return E_INVALIDARG;
        }
        DIBSECTION info = { 0 };
        if (!::GetObjectW(hBitmap, sizeof(DIBSECTION), &info)) {
            return E_INVALIDARG;
        }
        BITMAP& bm = info.dsBm;
        if (!bm.bmWidth || !bm.bmHeight || bm.bmBitsPixel != 32 || !bm.bmBits) {
            return E_INVALIDARG;
        }

        if (m_pAlphaBitmapTexture) {
            D3DSURFACE_DESC desc = {};
            m_pAlphaBitmapTexture->GetLevelDesc(0, &desc);
            if (bm.bmWidth != desc.Width || bm.bmHeight != desc.Height) {
                m_pAlphaBitmapTexture.Release();
            }
        }

        if (!m_pAlphaBitmapTexture) {
            hr = m_pD3DDevEx->CreateTexture(bm.bmWidth, bm.bmHeight, 1, D3DUSAGE_DYNAMIC, D3DFMT_A8R8G8B8, D3DPOOL_DEFAULT, &m_pAlphaBitmapTexture, nullptr);
        }

        if (SUCCEEDED(hr)) {
            CComPtr<IDirect3DSurface9> pSurface;
            hr = m_pAlphaBitmapTexture->GetSurfaceLevel(0, &pSurface);
            if (SUCCEEDED(hr)) {
                D3DLOCKED_RECT lr;
                hr = pSurface->LockRect(&lr, nullptr, D3DLOCK_DISCARD);
                if (S_OK == hr) {
                    if (bm.bmWidthBytes == lr.Pitch) {
                        memcpy(lr.pBits, bm.bmBits, bm.bmWidthBytes * bm.bmHeight);
                    }
                    else {
                        LONG linesize = std::min(bm.bmWidthBytes, (LONG)lr.Pitch);
                        BYTE* src = (BYTE*)bm.bmBits;
                        BYTE* dst = (BYTE*)lr.pBits;
                        for (LONG y = 0; y < bm.bmHeight; ++y) {
                            memcpy(dst, src, linesize);
                            src += bm.bmWidthBytes;
                            dst += lr.Pitch;
                        }
                    }
                    hr = pSurface->UnlockRect();
                }
            }
        }
    } else {
        return E_INVALIDARG;
    }

    m_bAlphaBitmapEnable = SUCCEEDED(hr) && m_pAlphaBitmapTexture;

    if (m_bAlphaBitmapEnable) {
        hr = UpdateAlphaBitmapParameters(&pBmpParms->params);
    }

    return hr;
}

STDMETHODIMP CEVRAllocatorPresenter::UpdateAlphaBitmapParameters(const MFVideoAlphaBitmapParams *pBmpParms)
{
    CheckPointer(pBmpParms, E_POINTER);
    CAutoLock cRenderLock(&m_RenderLock);

    m_AlphaBitmapParams = *pBmpParms; // formal implementation, don't believe it

    return S_OK;
}

// IEVRTrustedVideoPlugin
STDMETHODIMP CEVRAllocatorPresenter::IsInTrustedVideoMode(BOOL* pYes)
{
    CheckPointer(pYes, E_POINTER);
    *pYes = TRUE;
    return S_OK;
}

STDMETHODIMP CEVRAllocatorPresenter::CanConstrict(BOOL* pYes)
{
    CheckPointer(pYes, E_POINTER);
    *pYes = TRUE;
    return S_OK;
}

STDMETHODIMP CEVRAllocatorPresenter::SetConstriction(DWORD dwKPix)
{
    return S_OK;
}

STDMETHODIMP CEVRAllocatorPresenter::DisableImageExport(BOOL bDisable)
{
    return S_OK;
}


// IDirect3DDeviceManager9
STDMETHODIMP CEVRAllocatorPresenter::ResetDevice(IDirect3DDevice9* pDevice, UINT resetToken)
{
    HRESULT hr = m_pD3DManager->ResetDevice(pDevice, resetToken);
    return hr;
}

STDMETHODIMP CEVRAllocatorPresenter::OpenDeviceHandle(HANDLE* phDevice)
{
    HRESULT hr = m_pD3DManager->OpenDeviceHandle(phDevice);
    return hr;
}

STDMETHODIMP CEVRAllocatorPresenter::CloseDeviceHandle(HANDLE hDevice)
{
    HRESULT hr = m_pD3DManager->CloseDeviceHandle(hDevice);
    return hr;
}

STDMETHODIMP CEVRAllocatorPresenter::TestDevice(HANDLE hDevice)
{
    HRESULT hr = m_pD3DManager->TestDevice(hDevice);
    return hr;
}

STDMETHODIMP CEVRAllocatorPresenter::LockDevice(HANDLE hDevice, IDirect3DDevice9** ppDevice, BOOL fBlock)
{
    HRESULT hr = m_pD3DManager->LockDevice(hDevice, ppDevice, fBlock);
    return hr;
}

STDMETHODIMP CEVRAllocatorPresenter::UnlockDevice(HANDLE hDevice, BOOL fSaveState)
{
    HRESULT hr = m_pD3DManager->UnlockDevice(hDevice, fSaveState);
    return hr;
}

STDMETHODIMP CEVRAllocatorPresenter::GetVideoService(HANDLE hDevice, REFIID riid, void** ppService)
{
    HRESULT hr = m_pD3DManager->GetVideoService(hDevice, riid, ppService);

    if (riid == __uuidof(IDirectXVideoDecoderService)) {
        UINT  nNbDecoder = 5;
        GUID* pDecoderGuid;
        IDirectXVideoDecoderService* pDXVAVideoDecoder = (IDirectXVideoDecoderService*) *ppService;
        pDXVAVideoDecoder->GetDecoderDeviceGuids(&nNbDecoder, &pDecoderGuid);
    } else if (riid == __uuidof(IDirectXVideoProcessorService)) {
        IDirectXVideoProcessorService* pDXVAProcessor = (IDirectXVideoProcessorService*) *ppService;
        UNREFERENCED_PARAMETER(pDXVAProcessor);
    }

    return hr;
}

STDMETHODIMP CEVRAllocatorPresenter::GetNativeVideoSize(LONG* lpWidth, LONG* lpHeight, LONG* lpARWidth, LONG* lpARHeight)
{
    // This function should be called...
    ASSERT(FALSE);

    if (lpWidth) {
        *lpWidth = m_nativeVideoSize.cx;
    }
    if (lpHeight) {
        *lpHeight = m_nativeVideoSize.cy;
    }
    if (lpARWidth) {
        *lpARWidth = m_aspectRatio.cx;
    }
    if (lpARHeight) {
        *lpARHeight = m_aspectRatio.cy;
    }
    return S_OK;
}

STDMETHODIMP CEVRAllocatorPresenter::InitializeDevice(IMFMediaType* pMediaType)
{
    HRESULT hr;
    CAutoLock lock(this);
    CAutoLock lock2(&m_ImageProcessingLock);
    CAutoLock cRenderLock(&m_RenderLock);

    RemoveAllSamples();
    DeleteSurfaces();

    // Retrieve the surface size and format
    UINT32 Width;
    UINT32 Height;
    hr = MFGetAttributeSize(pMediaType, MF_MT_FRAME_SIZE, &Width, &Height);

    D3DFORMAT Format;
    if (SUCCEEDED(hr)) {
        SetVideoSize(CSize(Width, Height), m_aspectRatio);
        hr = GetMediaTypeFourCC(pMediaType, (DWORD*)&Format);
    }

    if (SUCCEEDED(hr)) {
        hr = AllocSurfaces();
    }

    if (SUCCEEDED(hr)) {
        for (int i = 0; i < m_nNbDXSurface; i++) {
            CComPtr<IMFSample> pMFSample;
            hr = fnMFCreateVideoSampleFromSurface(m_pVideoSurface[i], &pMFSample);

            if (SUCCEEDED(hr)) {
                pMFSample->SetUINT32(GUID_GROUP_ID, m_nCurrentGroupId);
                pMFSample->SetUINT32(GUID_SURFACE_INDEX, i);
                CAutoLock sampleQueueLock(&m_SampleQueueLock);
                m_FreeSamples.AddTail(pMFSample);
                pMFSample = nullptr; // The sample should not be used after being queued
            }
            ASSERT(SUCCEEDED(hr));
        }
    }

    return hr;
}

DWORD WINAPI CEVRAllocatorPresenter::GetMixerThreadStatic(LPVOID lpParam)
{
    SetThreadName(DWORD(-1), "CEVRPresenter::MixerThread");
    CEVRAllocatorPresenter* pThis = (CEVRAllocatorPresenter*) lpParam;
    pThis->GetMixerThread();
    return 0;
}

DWORD WINAPI CEVRAllocatorPresenter::PresentThread(LPVOID lpParam)
{
    SetThreadName(DWORD(-1), "CEVRPresenter::PresentThread");
    CEVRAllocatorPresenter* pThis = (CEVRAllocatorPresenter*) lpParam;
    pThis->RenderThread();
    return 0;
}

bool ExtractInterlaced(const AM_MEDIA_TYPE* pmt)
{
    if (pmt->formattype == FORMAT_VideoInfo) {
        return false;
    } else if (pmt->formattype == FORMAT_VideoInfo2) {
        return (((VIDEOINFOHEADER2*)pmt->pbFormat)->dwInterlaceFlags & AMINTERLACE_IsInterlaced) != 0;
    } else if (pmt->formattype == FORMAT_MPEGVideo) {
        return false;
    } else if (pmt->formattype == FORMAT_MPEG2Video) {
        return (((MPEG2VIDEOINFO*)pmt->pbFormat)->hdr.dwInterlaceFlags & AMINTERLACE_IsInterlaced) != 0;
    } else {
        return false;
    }
}

void CEVRAllocatorPresenter::GetMixerThread()
{
    HANDLE   hEvts[] = { m_hEvtQuit};
    bool     bQuit = false;
    TIMECAPS tc;
    DWORD    dwResolution;

    // Tell Multimedia Class Scheduler we are a playback thread (increase priority)
    //HANDLE hAvrt = 0;
    //if (pfAvSetMmThreadCharacteristicsW) {
    //    DWORD dwTaskIndex = 0;
    //    hAvrt = pfAvSetMmThreadCharacteristicsW(L"Playback", &dwTaskIndex);
    //    if (pfAvSetMmThreadPriority) {
    //        pfAvSetMmThreadPriority(hAvrt, AVRT_PRIORITY_HIGH /*AVRT_PRIORITY_CRITICAL*/);
    //    }
    //}

    timeGetDevCaps(&tc, sizeof(TIMECAPS));
    dwResolution = std::min(std::max(tc.wPeriodMin, 0u), tc.wPeriodMax);
    timeBeginPeriod(dwResolution);

    while (!bQuit) {
        DWORD dwObject = WaitForMultipleObjects(_countof(hEvts), hEvts, FALSE, 1);
        switch (dwObject) {
            case WAIT_OBJECT_0:
                bQuit = true;
                break;
            case WAIT_TIMEOUT: {
                bool bDoneSomething = false;
                {
                    CAutoLock AutoLock(&m_ImageProcessingLock);
                    bDoneSomething = GetImageFromMixer();
                }
                if (m_rtTimePerFrame == 0 && bDoneSomething) {
                    //CAutoLock lock(this);
                    //CAutoLock lock2(&m_ImageProcessingLock);
                    //CAutoLock cRenderLock(&m_RenderLock);

                    // Use the code from VMR9 to get the movie fps, as this method is reliable.
                    CComPtr<IPin> pPin;
                    CMediaType    mt;
                    if (
                        SUCCEEDED(m_pOuterEVR->FindPin(L"EVR Input0", &pPin)) &&
                        SUCCEEDED(pPin->ConnectionMediaType(&mt))) {
                        ExtractAvgTimePerFrame(&mt, m_rtTimePerFrame);

                        m_bInterlaced = ExtractInterlaced(&mt);

                        CComPtr<IPin> pPinTo;
                        if (SUCCEEDED(pPin->ConnectedTo(&pPinTo)) && pPinTo) {
                            m_Decoder = GetFilterName(GetFilterFromPin(pPinTo));
                        }
                    }
                    // If framerate not set by Video Decoder choose 23.97...
                    if (m_rtTimePerFrame == 0) {
                        m_rtTimePerFrame = 417166;
                    }

                    // Update internal subtitle clock
                    if (m_fUseInternalTimer && m_pSubPicQueue) {
                        m_fps = 10000000.0 / m_rtTimePerFrame;
                        m_pSubPicQueue->SetFPS(m_fps);
                    }
                }
            }
            break;
        }
    }

    timeEndPeriod(dwResolution);
    //  if (pfAvRevertMmThreadCharacteristics) pfAvRevertMmThreadCharacteristics (hAvrt);
}

void ModerateFloat(double& Value, double Target, double& ValuePrim, double ChangeSpeed)
{
    double xbiss = (-(ChangeSpeed) * (ValuePrim) - (Value - Target) * (ChangeSpeed * ChangeSpeed) * 0.25f);
    ValuePrim += xbiss;
    Value += ValuePrim;
}

LONGLONG CEVRAllocatorPresenter::GetClockTime(LONGLONG PerformanceCounter)
{
    LONGLONG llClockTime;
    MFTIME   nsCurrentTime;
    m_pClock->GetCorrelatedTime(0, &llClockTime, &nsCurrentTime);
    DWORD Characteristics = 0;
    m_pClock->GetClockCharacteristics(&Characteristics);
    MFCLOCK_STATE State;
    m_pClock->GetState(0, &State);

    if (!(Characteristics & MFCLOCK_CHARACTERISTICS_FLAG_FREQUENCY_10MHZ)) {
        MFCLOCK_PROPERTIES Props;
        if (m_pClock->GetProperties(&Props) == S_OK) {
            llClockTime = (llClockTime * 10000000) / Props.qwClockFrequency;    // Make 10 MHz
        }

    }
    LONGLONG llPerf = PerformanceCounter;
    //return llClockTime + (llPerf - nsCurrentTime);
    double Target = llClockTime + (llPerf - nsCurrentTime) * m_ModeratedTimeSpeed;

    bool bReset = false;
    if (m_ModeratedTimeLast < 0 || State != m_LastClockState || m_ModeratedClockLast < 0) {
        bReset = true;
        m_ModeratedTimeLast = llPerf;
        m_ModeratedClockLast = llClockTime;
    }

    m_LastClockState = State;

    LONGLONG TimeChangeM  = llPerf - m_ModeratedTimeLast;
    LONGLONG ClockChangeM = llClockTime - m_ModeratedClockLast;
    UNREFERENCED_PARAMETER(ClockChangeM);

    m_ModeratedTimeLast  = llPerf;
    m_ModeratedClockLast = llClockTime;

#if 1

    if (bReset) {
        m_ModeratedTimeSpeed = 1.0;
        m_ModeratedTimeSpeedPrim = 0.0;
        ZeroMemory(m_TimeChangeHistory, sizeof(m_TimeChangeHistory));
        ZeroMemory(m_ClockChangeHistory, sizeof(m_ClockChangeHistory));
        m_ClockTimeChangeHistoryPos = 0;
    }
    if (TimeChangeM) {
        int Pos = m_ClockTimeChangeHistoryPos % 100;
        int nHistory = std::min(m_ClockTimeChangeHistoryPos, 100);
        ++m_ClockTimeChangeHistoryPos;
        if (nHistory > 50) {
            int iLastPos = (Pos - (nHistory)) % 100;
            if (iLastPos < 0) {
                iLastPos += 100;
            }

            double TimeChange = llPerf - m_TimeChangeHistory[iLastPos];
            double ClockChange = llClockTime - m_ClockChangeHistory[iLastPos];

            double ClockSpeedTarget = ClockChange / TimeChange;
            double ChangeSpeed = 0.1;
            if (ClockSpeedTarget > m_ModeratedTimeSpeed) {
                if (ClockSpeedTarget / m_ModeratedTimeSpeed > 0.1) {
                    ChangeSpeed = 0.1;
                } else {
                    ChangeSpeed = 0.01;
                }
            } else {
                if (m_ModeratedTimeSpeed / ClockSpeedTarget > 0.1) {
                    ChangeSpeed = 0.1;
                } else {
                    ChangeSpeed = 0.01;
                }
            }
            ModerateFloat(m_ModeratedTimeSpeed, ClockSpeedTarget, m_ModeratedTimeSpeedPrim, ChangeSpeed);
            //m_ModeratedTimeSpeed = TimeChange / ClockChange;
        }
        m_TimeChangeHistory[Pos]  = (double)llPerf;
        m_ClockChangeHistory[Pos] = (double)llClockTime;
    }

    return (LONGLONG)Target;
#else
    double EstimateTime = m_ModeratedTime + TimeChange * m_ModeratedTimeSpeed + m_ClockDiffCalc;
    double Diff = Target - EstimateTime;

    // > 5 ms just set it
    if ((fabs(Diff) > 50000.0 || bReset)) {

        //      TRACE_EVR("EVR: Reset clock at diff: %f ms\n", (m_ModeratedTime - Target) /10000.0);
        if (State == MFCLOCK_STATE_RUNNING) {
            if (bReset) {
                m_ModeratedTimeSpeed = 1.0;
                m_ModeratedTimeSpeedPrim = 0.0;
                m_ClockDiffCalc  = 0;
                m_ClockDiffPrim  = 0;
                m_ModeratedTime  = Target;
                m_ModeratedTimer = llPerf;
            } else {
                EstimateTime = m_ModeratedTime + TimeChange * m_ModeratedTimeSpeed;
                Diff = Target - EstimateTime;
                m_ClockDiffCalc = Diff;
                m_ClockDiffPrim = 0;
            }
        } else {
            m_ModeratedTimeSpeed = 0.0;
            m_ModeratedTimeSpeedPrim = 0.0;
            m_ClockDiffCalc  = 0;
            m_ClockDiffPrim  = 0;
            m_ModeratedTime  = Target;
            m_ModeratedTimer = llPerf;
        }
    }

    {
        LONGLONG ModerateTime = 10000;
        double ChangeSpeed = 1.00;
        /*if (m_ModeratedTimeSpeedPrim != 0.0)
          {
              if (m_ModeratedTimeSpeedPrim < 0.1)
                  ChangeSpeed = 0.1;
          }*/

        int nModerate = 0;
        double Change = 0;
        while (m_ModeratedTimer < llPerf - ModerateTime) {
            m_ModeratedTimer += ModerateTime;
            m_ModeratedTime += double(ModerateTime) * m_ModeratedTimeSpeed;

            double TimerDiff = llPerf - m_ModeratedTimer;

            double Diff = (double)(m_ModeratedTime - (Target - TimerDiff));

            double TimeSpeedTarget;
            double AbsDiff = fabs(Diff);
            TimeSpeedTarget = 1.0 - (Diff / 1000000.0);
            //          TimeSpeedTarget = m_ModeratedTimeSpeed - (Diff / 100000000000.0);
            //if (AbsDiff > 20000.0)
            //              TimeSpeedTarget = 1.0 - (Diff / 1000000.0);
            /*else if (AbsDiff > 5000.0)
                TimeSpeedTarget = 1.0 - (Diff / 100000000.0);
            else
                TimeSpeedTarget = 1.0 - (Diff / 500000000.0);*/
            double StartMod = m_ModeratedTimeSpeed;
            ModerateFloat(m_ModeratedTimeSpeed, TimeSpeedTarget, m_ModeratedTimeSpeedPrim, ChangeSpeed);
            m_ModeratedTimeSpeed = TimeSpeedTarget;
            ++nModerate;
            Change += m_ModeratedTimeSpeed - StartMod;
        }
        if (nModerate) {
            m_ModeratedTimeSpeedDiff = Change / nModerate;
        }

        double Ret = m_ModeratedTime + double(llPerf - m_ModeratedTimer) * m_ModeratedTimeSpeed;
        double Diff = Target - Ret;
        ModerateFloat(m_ClockDiffCalc, Diff, m_ClockDiffPrim, ChangeSpeed * 0.1);

        Ret += m_ClockDiffCalc;
        Diff = Target - Ret;
        m_ClockDiff = Diff;
        return LONGLONG(Ret + 0.5);
    }

    return Target;
    return LONGLONG(m_ModeratedTime + 0.5);
#endif
}

void CEVRAllocatorPresenter::OnVBlankFinished(bool bAll, LONGLONG PerformanceCounter)
{
    // This function is meant to be called only from the rendering function
    // so with the ownership on m_RenderLock.
    if (!m_pCurrentlyDisplayedSample || !m_OrderedPaint || !bAll) {
        return;
    }

    LONGLONG llClockTime;
    LONGLONG nsSampleTime;
    LONGLONG SampleDuration = 0;
    LONGLONG TimePerFrame = m_rtTimePerFrame;
    if (!TimePerFrame) {
        return;
    }

    if (!m_bSignaledStarvation) {
        llClockTime = GetClockTime(PerformanceCounter);
        m_StarvationClock = llClockTime;
    } else {
        llClockTime = m_StarvationClock;
    }

    if (SUCCEEDED(m_pCurrentlyDisplayedSample->GetSampleDuration(&SampleDuration))) {
        // Some filters return invalid values, ignore them
        if (SampleDuration > MIN_FRAME_TIME) {
            TimePerFrame = SampleDuration;
        }
    }

    if (FAILED(m_pCurrentlyDisplayedSample->GetSampleTime(&nsSampleTime))) {
        nsSampleTime = llClockTime;
    }

    {
        m_nNextSyncOffset = (m_nNextSyncOffset + 1) % NB_JITTER;
        LONGLONG SyncOffset = nsSampleTime - llClockTime;

        m_pllSyncOffset[m_nNextSyncOffset] = SyncOffset;
        //TRACE_EVR("EVR: SyncOffset(%d, %d): %8I64d     %8I64d     %8I64d \n", m_nCurSurface, m_VSyncMode, m_LastPredictedSync, -SyncOffset, m_LastPredictedSync - (-SyncOffset));

        m_MaxSyncOffset = MINLONG64;
        m_MinSyncOffset = MAXLONG64;

        LONGLONG AvrageSum = 0;
        for (int i = 0; i < NB_JITTER; i++) {
            LONGLONG Offset = m_pllSyncOffset[i];
            AvrageSum += Offset;
            m_MaxSyncOffset = std::max(m_MaxSyncOffset, Offset);
            m_MinSyncOffset = std::min(m_MinSyncOffset, Offset);
        }
        double MeanOffset = double(AvrageSum) / NB_JITTER;
        double DeviationSum = 0;
        for (int i = 0; i < NB_JITTER; i++) {
            double Deviation = double(m_pllSyncOffset[i]) - MeanOffset;
            DeviationSum += Deviation * Deviation;
        }
        double StdDev = sqrt(DeviationSum / NB_JITTER);

        m_fSyncOffsetAvr = MeanOffset;
        m_bSyncStatsAvailable = true;
        m_fSyncOffsetStdDev = StdDev;
    }
}

STDMETHODIMP_(bool) CEVRAllocatorPresenter::ResetDevice()
{
    CAutoLock cThreadsLock(&m_ThreadsLock);

    StopWorkerThreads();

    CAutoLock lock(this);
    CAutoLock lock2(&m_ImageProcessingLock);
    CAutoLock cRenderLock(&m_RenderLock);

    RemoveAllSamples();

    bool bResult = __super::ResetDevice();

    for (int i = 0; i < m_nNbDXSurface; i++) {
        CComPtr<IMFSample> pMFSample;
        HRESULT hr = fnMFCreateVideoSampleFromSurface(m_pVideoSurface[i], &pMFSample);

        if (SUCCEEDED(hr)) {
            pMFSample->SetUINT32(GUID_GROUP_ID, m_nCurrentGroupId);
            pMFSample->SetUINT32(GUID_SURFACE_INDEX, i);
            CAutoLock sampleQueueLock(&m_SampleQueueLock);
            m_FreeSamples.AddTail(pMFSample);
            pMFSample = nullptr; // The sample should not be used after being queued
        }
        ASSERT(SUCCEEDED(hr));
    }

    if (bResult) {
        StartWorkerThreads();
    }

    return bResult;
}

STDMETHODIMP_(bool) CEVRAllocatorPresenter::DisplayChange()
{
    CAutoLock lock(this);
    CAutoLock lock2(&m_ImageProcessingLock);
    CAutoLock cRenderLock(&m_RenderLock);

    bool bResult = __super::DisplayChange();

    m_DetectedFrameRate = 0.0;
    m_DetectedFrameTime = 0.0;
    m_DetectedFrameTimeStdDev = 0.0;
    m_DetectedLock = false;
    ZeroMemory(m_DetectedFrameTimeHistory, sizeof(m_DetectedFrameTimeHistory));
    ZeroMemory(m_DetectedFrameTimeHistoryHistory, sizeof(m_DetectedFrameTimeHistoryHistory));
    m_DetectedFrameTimePos = 0;
    ZeroMemory(&m_VMR9AlphaBitmap, sizeof(m_VMR9AlphaBitmap));
    ZeroMemory(m_ldDetectedRefreshRateList, sizeof(m_ldDetectedRefreshRateList));
    ZeroMemory(m_ldDetectedScanlineRateList, sizeof(m_ldDetectedScanlineRateList));
    m_DetectedRefreshRatePos = 0;
    m_DetectedRefreshTimePrim = 0;
    m_DetectedScanlineTime = 0;
    m_DetectedScanlineTimePrim = 0;
    m_DetectedRefreshRate = 0;

    ZeroMemory(m_pllJitter, sizeof(m_pllJitter));
    ZeroMemory(m_pllSyncOffset, sizeof(m_pllSyncOffset));
    m_nNextJitter       = 0;
    m_nNextSyncOffset = 0;
    m_llLastPerf        = 0;
    m_fAvrFps           = 0.0;
    m_fJitterStdDev     = 0.0;
    m_fSyncOffsetStdDev = 0.0;
    m_fSyncOffsetAvr    = 0.0;
    m_bSyncStatsAvailable = false;

    return bResult;
}

void CEVRAllocatorPresenter::RenderThread()
{
    HANDLE   hEvts[] = { m_hEvtQuit, m_hEvtFlush};
    bool     bQuit = false, bForcePaint = false;
    TIMECAPS tc;
    MFTIME   nsSampleTime;
    LONGLONG llClockTime;

    // Tell Multimedia Class Scheduler we are a playback thread (increase priority)
    HANDLE hAvrt = 0;
    if (fnAvSetMmThreadCharacteristicsW) {
        DWORD dwTaskIndex = 0;
        hAvrt = fnAvSetMmThreadCharacteristicsW(L"Playback", &dwTaskIndex);
        if (fnAvSetMmThreadPriority) {
            fnAvSetMmThreadPriority(hAvrt, AVRT_PRIORITY_HIGH /*AVRT_PRIORITY_CRITICAL*/);
        }
    }

    timeGetDevCaps(&tc, sizeof(TIMECAPS));
    DWORD dwResolution = std::min(std::max(tc.wPeriodMin, 0u), tc.wPeriodMax);
    VERIFY(timeBeginPeriod(dwResolution) == 0);

    auto checkPendingMediaFinished = [this]() {
        if (m_bPendingMediaFinished && m_nRenderState != Stopped) {
            CAutoLock lock(&m_SampleQueueLock);
            if (m_ScheduledSamples.IsEmpty()) {
                m_bPendingMediaFinished = false;
                m_pSink->Notify(EC_COMPLETE, 0, 0);
                TRACE_EVR("EVR: send EC_COMPLETE\n");
            }
        }
    };

    const CRenderersSettings& r = GetRenderersSettings();

    auto SubPicSetTime = [&] {
        if (!g_bExternalSubtitleTime && !m_bIsPreview) {
            CSubPicAllocatorPresenterImpl::SetTime(g_tSegmentStart + nsSampleTime * (g_bExternalSubtitle ? g_dRate : 1));
        }
    };

    int NextSleepTime = 1;
    while (!bQuit) {
        LONGLONG llPerf = GetRenderersData()->GetPerfCounter();
        UNREFERENCED_PARAMETER(llPerf);
        if (!r.m_AdvRendSets.bVMR9VSyncAccurate && NextSleepTime == 0) {
            NextSleepTime = 1;
        }
        DWORD dwObject = WaitForMultipleObjects(_countof(hEvts), hEvts, FALSE, std::max(NextSleepTime < 0 ? 1 : NextSleepTime, 0));
        /*      dwObject = WAIT_TIMEOUT;
                if (m_bEvtFlush)
                    dwObject = WAIT_OBJECT_0 + 1;
                else if (m_bEvtQuit)
                    dwObject = WAIT_OBJECT_0;*/
        //      if (NextSleepTime)
        //          TRACE_EVR("EVR: Sleep: %7.3f\n", double(GetRenderersData()->GetPerfCounter()-llPerf) / 10000.0);
        if (NextSleepTime > 1) {
            NextSleepTime = 0;
        } else if (NextSleepTime == 0) {
            NextSleepTime = -1;
        }
        switch (dwObject) {
            case WAIT_OBJECT_0:
                bQuit = true;
                break;
            case WAIT_OBJECT_0 + 1:
                // Flush pending samples!
                FlushSamples();
                m_bEvtFlush = false;
                ResetEvent(m_hEvtFlush);
                bForcePaint = true;
                TRACE_EVR("EVR: Flush done!\n");
                break;

            case WAIT_TIMEOUT:

                if (m_LastSetOutputRange != -1 && m_LastSetOutputRange != r.m_AdvRendSets.iEVROutputRange || m_bPendingRenegotiate) {
                    FlushSamples();
                    RenegotiateMediaType();
                    m_bPendingRenegotiate = false;
                }
                if (m_bPendingResetDevice) {
                    SendResetRequest();
                }

                // Discard timer events if playback stop
                //if ((dwObject == WAIT_OBJECT_0 + 3) && (m_nRenderState != Started)) continue;

                //TRACE_EVR ("EVR: RenderThread ==>> Waiting buffer\n");

                //if (WaitForMultipleObjects (_countof(hEvtsBuff), hEvtsBuff, FALSE, INFINITE) == WAIT_OBJECT_0+2)
                {
                    CComPtr<IMFSample> pMFSample;
                    //LONGLONG llPerf2 = GetRenderersData()->GetPerfCounter();
                    //UNREFERENCED_PARAMETER(llPerf2);
                    int nSamplesLeft = 0;
                    if (SUCCEEDED(GetScheduledSample(&pMFSample, nSamplesLeft))) {
                        //UINT32 nSurface;
                        //pMFSample->GetUINT32(GUID_SURFACE_INDEX, (UINT32*)&nSurface);

                        bool bValidSampleTime = true;
                        HRESULT hrGetSampleTime = pMFSample->GetSampleTime(&nsSampleTime);
                        if (hrGetSampleTime != S_OK || nsSampleTime == 0) {
                            bValidSampleTime = false;
                        }
                        // We assume that all samples have the same duration
                        LONGLONG SampleDuration = 0;
                        bool bValidSampleDuration = true;
                        HRESULT hrGetSampleDuration = pMFSample->GetSampleDuration(&SampleDuration);
                        // Some filters return invalid values, ignore them
                        if (hrGetSampleDuration != S_OK || SampleDuration <= MIN_FRAME_TIME) {
                            bValidSampleDuration = false;
                        }

                        //TRACE_EVR("EVR: RenderThread ==>> Presenting surface %d  (%I64d)\n", nSurface, nsSampleTime);

                        bool bStepForward = false;

                        if (m_nStepCount < 0) {
                            // Drop frame
                            TRACE_EVR("EVR: Dropped frame\n");
                            m_pcFrames++;
                            bStepForward = true;
                            m_nStepCount = 0;
                            /*
                            } else if (m_nStepCount > 0) {
                                ++m_OrderedPaint;
                                SubPicSetTime();
                                Paint(pMFSample);
                                m_nDroppedUpdate = 0;
                                CompleteFrameStep(false);
                                bStepForward = true;
                            */
                        } else if (m_nRenderState == Started) {
                            LONGLONG CurrentCounter = GetRenderersData()->GetPerfCounter();
                            // Calculate wake up timer
                            if (!m_bSignaledStarvation) {
                                llClockTime = GetClockTime(CurrentCounter);
                                m_StarvationClock = llClockTime;
                            } else {
                                llClockTime = m_StarvationClock;
                            }

                            if (!bValidSampleTime) {
                                // Just play as fast as possible
                                bStepForward = true;
                                ++m_OrderedPaint;
                                SubPicSetTime();
                                Paint(pMFSample);
                            } else {
                                LONGLONG TimePerFrame = (LONGLONG)(GetFrameTime() * 10000000.0);
                                LONGLONG SyncOffset = 0;
                                LONGLONG VSyncTime = 0;
                                LONGLONG TimeToNextVSync = -1;
                                bool bVSyncCorrection = false;
                                double DetectedRefreshTime;
                                double DetectedScanlinesPerFrame;
                                double DetectedScanlineTime;
                                int DetectedRefreshRatePos;
                                {
                                    CAutoLock Lock(&m_refreshRateLock);
                                    DetectedRefreshTime = m_DetectedRefreshTime;
                                    DetectedRefreshRatePos = m_DetectedRefreshRatePos;
                                    DetectedScanlinesPerFrame = m_DetectedScanlinesPerFrame;
                                    DetectedScanlineTime = m_DetectedScanlineTime;
                                }

                                if (DetectedRefreshRatePos < 20 || !DetectedRefreshTime || !DetectedScanlinesPerFrame) {
                                    DetectedRefreshTime = 1.0 / m_refreshRate;
                                    DetectedScanlinesPerFrame = m_ScreenSize.cy;
                                    DetectedScanlineTime = DetectedRefreshTime / double(m_ScreenSize.cy);
                                }

                                if (r.m_AdvRendSets.bVMR9VSync) {
                                    bVSyncCorrection = true;
                                    double TargetVSyncPos = GetVBlackPos();
                                    double RefreshLines = DetectedScanlinesPerFrame;
                                    double ScanlinesPerSecond = 1.0 / DetectedScanlineTime;
                                    double CurrentVSyncPos = fmod(double(m_VBlankStartMeasure) + ScanlinesPerSecond * ((CurrentCounter - m_VBlankStartMeasureTime) / 10000000.0), RefreshLines);
                                    double LinesUntilVSync = 0;
                                    //TargetVSyncPos -= ScanlinesPerSecond * (DrawTime/10000000.0);
                                    //TargetVSyncPos -= 10;
                                    TargetVSyncPos = fmod(TargetVSyncPos, RefreshLines);
                                    if (TargetVSyncPos < 0) {
                                        TargetVSyncPos += RefreshLines;
                                    }
                                    if (TargetVSyncPos > CurrentVSyncPos) {
                                        LinesUntilVSync = TargetVSyncPos - CurrentVSyncPos;
                                    } else {
                                        LinesUntilVSync = (RefreshLines - CurrentVSyncPos) + TargetVSyncPos;
                                    }
                                    double TimeUntilVSync = LinesUntilVSync * DetectedScanlineTime;
                                    TimeToNextVSync = (LONGLONG)(TimeUntilVSync * 10000000.0);
                                    VSyncTime = (LONGLONG)(DetectedRefreshTime * 10000000.0);

                                    LONGLONG ClockTimeAtNextVSync = llClockTime + (LONGLONG)(TimeUntilVSync * 10000000.0 * m_ModeratedTimeSpeed);

                                    SyncOffset = (nsSampleTime - ClockTimeAtNextVSync);

                                    //if (SyncOffset < 0)
                                    //    TRACE_EVR("EVR: SyncOffset(%d): %I64d     %I64d     %I64d\n", m_nCurSurface, SyncOffset, TimePerFrame, VSyncTime);
                                } else {
                                    SyncOffset = (nsSampleTime - llClockTime);
                                }

                                //LONGLONG SyncOffset = nsSampleTime - llClockTime;
                                TRACE_EVR("EVR: SyncOffset: %I64d SampleFrame: %I64d ClockFrame: %I64d\n", SyncOffset, TimePerFrame != 0 ? nsSampleTime / TimePerFrame : 0, TimePerFrame != 0 ? llClockTime / TimePerFrame : 0);
                                if (bValidSampleDuration && !m_DetectedLock) {
                                    TimePerFrame = SampleDuration;
                                }

                                LONGLONG MinMargin = MIN_FRAME_TIME + std::min(LONGLONG(m_DetectedFrameTimeStdDev), 20000ll);
                                //if (m_FrameTimeCorrection) {
                                //    MinMargin = MIN_FRAME_TIME;
                                //} else {
                                //    MinMargin = MIN_FRAME_TIME + std::min(LONGLONG(m_DetectedFrameTimeStdDev), 20000ll);
                                //}
                                LONGLONG TimePerFrameMargin = std::min(std::max(TimePerFrame * 2l / 100l, MinMargin), TimePerFrame * 11l / 100l); // (0.02..0.11)TimePerFrame
                                LONGLONG TimePerFrameMargin0 = TimePerFrameMargin / 2;
                                LONGLONG TimePerFrameMargin1 = 0;

                                if (m_DetectedLock && TimePerFrame < VSyncTime) {
                                    VSyncTime = TimePerFrame;
                                }

                                if (m_VSyncMode == 1) {
                                    TimePerFrameMargin1 = -TimePerFrameMargin;
                                } else if (m_VSyncMode == 2) {
                                    TimePerFrameMargin1 = TimePerFrameMargin;
                                }

                                m_LastSampleOffset = SyncOffset;
                                m_bLastSampleOffsetValid = true;

                                LONGLONG VSyncOffset0 = 0;
                                bool bDoVSyncCorrection = false;
                                if ((SyncOffset < -(TimePerFrame + TimePerFrameMargin0 - TimePerFrameMargin1)) && nSamplesLeft > 0) { // Only drop if we have something else to display at once
                                    // Drop frame
                                    TRACE_EVR("EVR: Dropped frame\n");
                                    m_pcFrames++;
                                    bStepForward = true;
                                    ++m_nDroppedUpdate;
                                    NextSleepTime = 0;
                                    //VSyncOffset0 = (-SyncOffset) - VSyncTime;
                                    //VSyncOffset0 = (-SyncOffset) - VSyncTime + TimePerFrameMargin1;
                                    //m_LastPredictedSync = VSyncOffset0;
                                    bDoVSyncCorrection = false;
                                } else if (SyncOffset < TimePerFrameMargin1) {

                                    if (bVSyncCorrection) {
                                        VSyncOffset0 = -SyncOffset;
                                        bDoVSyncCorrection = true;
                                    }

                                    // Paint and prepare for next frame
                                    TRACE_EVR("EVR: Normalframe\n");
                                    m_nDroppedUpdate = 0;
                                    bStepForward = true;
                                    m_LastFrameDuration = nsSampleTime - m_LastSampleTime;
                                    m_LastSampleTime = nsSampleTime;
                                    m_LastPredictedSync = VSyncOffset0;

                                    if (m_nStepCount > 0) {
                                        CompleteFrameStep(false);
                                    }

                                    ++m_OrderedPaint;

                                    SubPicSetTime();
                                    Paint(pMFSample);

                                    NextSleepTime = 0;
                                    m_pcFramesDrawn++;
                                } else {
                                    if (TimeToNextVSync >= 0 && SyncOffset > 0) {
                                        NextSleepTime = (int)(TimeToNextVSync / 10000 - 2);
                                    } else {
                                        NextSleepTime = (int)(SyncOffset / 10000 - 2);
                                    }

                                    if (NextSleepTime > TimePerFrame) {
                                        NextSleepTime = 1;
                                    }

                                    if (NextSleepTime < 0) {
                                        NextSleepTime = 0;
                                    }
                                    NextSleepTime = 1;
                                    //TRACE_EVR ("EVR: Delay\n");
                                }

                                if (bDoVSyncCorrection) {
                                    //LONGLONG VSyncOffset0 = (((SyncOffset) % VSyncTime) + VSyncTime) % VSyncTime;
                                    LONGLONG Margin = TimePerFrameMargin;

                                    LONGLONG VSyncOffsetMin = 30000000000000;
                                    LONGLONG VSyncOffsetMax = -30000000000000;
                                    for (int i = 0; i < 5; ++i) {
                                        VSyncOffsetMin = std::min(m_VSyncOffsetHistory[i], VSyncOffsetMin);
                                        VSyncOffsetMax = std::max(m_VSyncOffsetHistory[i], VSyncOffsetMax);
                                    }

                                    m_VSyncOffsetHistory[m_VSyncOffsetHistoryPos] = VSyncOffset0;
                                    m_VSyncOffsetHistoryPos = (m_VSyncOffsetHistoryPos + 1) % 5;

                                    //LONGLONG VSyncTime2 = VSyncTime2 + (VSyncOffsetMax - VSyncOffsetMin);
                                    //VSyncOffsetMin; = (((VSyncOffsetMin) % VSyncTime) + VSyncTime) % VSyncTime;
                                    //VSyncOffsetMax = (((VSyncOffsetMax) % VSyncTime) + VSyncTime) % VSyncTime;

                                    //TRACE_EVR("EVR: SyncOffset(%d, %d): %8I64d     %8I64d     %8I64d     %8I64d\n", m_nCurSurface, m_VSyncMode,VSyncOffset0, VSyncOffsetMin, VSyncOffsetMax, VSyncOffsetMax - VSyncOffsetMin);

                                    if (m_VSyncMode == 0) {
                                        // 23.976 in 60 Hz
                                        if (VSyncOffset0 < Margin && VSyncOffsetMax > (VSyncTime - Margin)) {
                                            m_VSyncMode = 2;
                                        } else if (VSyncOffset0 > (VSyncTime - Margin) && VSyncOffsetMin < Margin) {
                                            m_VSyncMode = 1;
                                        }
                                    } else if (m_VSyncMode == 2) {
                                        if (VSyncOffsetMin > (Margin)) {
                                            m_VSyncMode = 0;
                                        }
                                    } else if (m_VSyncMode == 1) {
                                        if (VSyncOffsetMax < (VSyncTime - Margin)) {
                                            m_VSyncMode = 0;
                                        }
                                    }
                                }
                            }
                        } else if (m_nRenderState == Paused) {
                            if (bForcePaint) {
                                bStepForward = true;
                                // Ensure that the renderer is properly updated after seeking when paused
                                SubPicSetTime();
                                Paint(pMFSample);
                            }
                            NextSleepTime = int(SampleDuration / 10000 - 2);
                        }

                        if (bStepForward) {
                            m_MaxSampleDuration = std::max(SampleDuration, m_MaxSampleDuration);
                            checkPendingMediaFinished();
                        } else {
                            AddToScheduledList(pMFSample, true);
                            pMFSample = nullptr; // The sample should not be used after being queued
                        }

                        bForcePaint = false;
                    } else if (m_bLastSampleOffsetValid && m_LastSampleOffset < -10000000) { // Only starve if we are 1 seconds behind
                        if (m_nRenderState == Started && !g_bNoDuration) {
                            m_pSink->Notify(EC_STARVATION, 0, 0);
                            m_bSignaledStarvation = true;
                        }
                    } else {
                        checkPendingMediaFinished();
                    }
                }
                //else
                //{
                //    TRACE_EVR ("EVR: RenderThread ==>> Flush before rendering frame!\n");
                //}

                break;
        }
    }

    timeEndPeriod(dwResolution);
    if (fnAvRevertMmThreadCharacteristics) {
        fnAvRevertMmThreadCharacteristics(hAvrt);
    }
}

void CEVRAllocatorPresenter::VSyncThread()
{
    HANDLE   hEvts[] = { m_hEvtQuit};
    bool     bQuit = false;
    TIMECAPS tc;
    DWORD    dwResolution;

    // Tell Multimedia Class Scheduler we are a playback thread (increase priority)
    //HANDLE hAvrt = 0;
    //if (pfAvSetMmThreadCharacteristicsW) {
    //    DWORD dwTaskIndex = 0;
    //    hAvrt = pfAvSetMmThreadCharacteristicsW(L"Playback", &dwTaskIndex);
    //    if (pfAvSetMmThreadPriority) {
    //        pfAvSetMmThreadPriority(hAvrt, AVRT_PRIORITY_HIGH /*AVRT_PRIORITY_CRITICAL*/);
    //    }
    //}

    timeGetDevCaps(&tc, sizeof(TIMECAPS));
    dwResolution = std::min(std::max(tc.wPeriodMin, 0u), tc.wPeriodMax);
    timeBeginPeriod(dwResolution);
    const CRenderersData* rd = GetRenderersData();
    const CRenderersSettings& r = GetRenderersSettings();

    while (!bQuit) {

        DWORD dwObject = WaitForMultipleObjects(_countof(hEvts), hEvts, FALSE, 1);
        switch (dwObject) {
            case WAIT_OBJECT_0:
                bQuit = true;
                break;
            case WAIT_TIMEOUT: {
                // Do our stuff
                if (m_pD3DDev && r.m_AdvRendSets.bVMR9VSync) {
                    if (m_nRenderState == Started) {
                        int VSyncPos  = GetVBlackPos();
                        int WaitRange = std::max(m_ScreenSize.cy / 40l, 5l);
                        int MinRange  = std::max(std::min(long(0.003 * m_ScreenSize.cy * m_refreshRate + 0.5), m_ScreenSize.cy / 3l), 5l); // 1.8  ms or max 33 % of Time

                        VSyncPos += MinRange + WaitRange;

                        VSyncPos = VSyncPos % m_ScreenSize.cy;
                        if (VSyncPos < 0) {
                            VSyncPos += m_ScreenSize.cy;
                        }

                        int ScanLine = 0;
                        int StartScanLine = ScanLine;
                        UNREFERENCED_PARAMETER(StartScanLine);
                        int LastPos = ScanLine;
                        UNREFERENCED_PARAMETER(LastPos);
                        ScanLine = (VSyncPos + 1) % m_ScreenSize.cy;
                        if (ScanLine < 0) {
                            ScanLine += m_ScreenSize.cy;
                        }
                        int ScanLineMiddle = ScanLine + m_ScreenSize.cy / 2;
                        ScanLineMiddle = ScanLineMiddle % m_ScreenSize.cy;
                        if (ScanLineMiddle < 0) {
                            ScanLineMiddle += m_ScreenSize.cy;
                        }

                        int ScanlineStart = ScanLine;
                        HANDLE lockOwner = nullptr;
                        WaitForVBlankRange(ScanlineStart, 5, true, true, false, lockOwner);
                        LONGLONG TimeStart = rd->GetPerfCounter();

                        WaitForVBlankRange(ScanLineMiddle, 5, true, true, false, lockOwner);
                        LONGLONG TimeMiddle = rd->GetPerfCounter();

                        int ScanlineEnd = ScanLine;
                        WaitForVBlankRange(ScanlineEnd, 5, true, true, false, lockOwner);
                        LONGLONG TimeEnd = rd->GetPerfCounter();

                        double nSeconds = (TimeEnd - TimeStart) / 10000000.0;
                        LONGLONG llDiffMiddle = TimeMiddle - TimeStart;
                        ASSERT(llDiffMiddle > 0);

                        if (nSeconds > 0.003 && llDiffMiddle > 0) {
                            double dDiffMiddle = double(llDiffMiddle);
                            double dDiffEnd = double(TimeEnd - TimeMiddle);

                            double dDiffDiff = dDiffEnd / dDiffMiddle;
                            if (dDiffDiff < 1.3 && dDiffDiff > (1 / 1.3)) {
                                double ScanLineSeconds;
                                double nScanLines;
                                if (ScanLineMiddle > ScanlineEnd) {
                                    ScanLineSeconds = dDiffMiddle / 10000000.0;
                                    nScanLines = ScanLineMiddle - ScanlineStart;
                                } else {
                                    ScanLineSeconds = dDiffEnd / 10000000.0;
                                    nScanLines = ScanlineEnd - ScanLineMiddle;
                                }

                                double ScanLineTime = ScanLineSeconds / nScanLines;

                                int iPos = m_DetectedRefreshRatePos % 100;
                                m_ldDetectedScanlineRateList[iPos] = ScanLineTime;
                                if (m_DetectedScanlineTime && ScanlineStart != ScanlineEnd) {
                                    int Diff = ScanlineEnd - ScanlineStart;
                                    nSeconds -= double(Diff) * m_DetectedScanlineTime;
                                }
                                m_ldDetectedRefreshRateList[iPos] = nSeconds;
                                double Average = 0;
                                double AverageScanline = 0;
                                int nPos = std::min(iPos + 1, 100);
                                for (int i = 0; i < nPos; ++i) {
                                    Average += m_ldDetectedRefreshRateList[i];
                                    AverageScanline += m_ldDetectedScanlineRateList[i];
                                }

                                if (nPos) {
                                    Average /= double(nPos);
                                    AverageScanline /= double(nPos);
                                } else {
                                    Average = 0;
                                    AverageScanline = 0;
                                }

                                double ThisValue = Average;

                                if (Average > 0.0 && AverageScanline > 0.0) {
                                    CAutoLock Lock(&m_refreshRateLock);
                                    ++m_DetectedRefreshRatePos;
                                    if (m_DetectedRefreshTime == 0 || m_DetectedRefreshTime / ThisValue > 1.01 || m_DetectedRefreshTime / ThisValue < 0.99) {
                                        m_DetectedRefreshTime = ThisValue;
                                        m_DetectedRefreshTimePrim = 0;
                                    }
                                    if (_isnan(m_DetectedRefreshTime)) {
                                        m_DetectedRefreshTime = 0.0;
                                    }
                                    if (_isnan(m_DetectedRefreshTimePrim)) {
                                        m_DetectedRefreshTimePrim = 0.0;
                                    }

                                    ModerateFloat(m_DetectedRefreshTime, ThisValue, m_DetectedRefreshTimePrim, 1.5);
                                    if (m_DetectedRefreshTime > 0.0) {
                                        m_DetectedRefreshRate = 1.0 / m_DetectedRefreshTime;
                                    } else {
                                        m_DetectedRefreshRate = 0.0;
                                    }

                                    if (m_DetectedScanlineTime == 0 || m_DetectedScanlineTime / AverageScanline > 1.01 || m_DetectedScanlineTime / AverageScanline < 0.99) {
                                        m_DetectedScanlineTime = AverageScanline;
                                        m_DetectedScanlineTimePrim = 0;
                                    }
                                    ModerateFloat(m_DetectedScanlineTime, AverageScanline, m_DetectedScanlineTimePrim, 1.5);
                                    if (m_DetectedScanlineTime > 0.0) {
                                        m_DetectedScanlinesPerFrame = m_DetectedRefreshTime / m_DetectedScanlineTime;
                                    } else {
                                        m_DetectedScanlinesPerFrame = 0;
                                    }
                                }
                                //TRACE(_T("Refresh: %f\n"), RefreshRate);
                            }
                        }
                    }
                } else {
                    m_DetectedRefreshRate = 0.0;
                    m_DetectedScanlinesPerFrame = 0.0;
                }
            }
            break;
        }
    }

    timeEndPeriod(dwResolution);
    //if (pfAvRevertMmThreadCharacteristics) pfAvRevertMmThreadCharacteristics (hAvrt);
}

DWORD WINAPI CEVRAllocatorPresenter::VSyncThreadStatic(LPVOID lpParam)
{
    SetThreadName(DWORD(-1), "CEVRAllocatorPresenter::VSyncThread");
    CEVRAllocatorPresenter* pThis = (CEVRAllocatorPresenter*) lpParam;
    pThis->VSyncThread();
    return 0;
}

void CEVRAllocatorPresenter::OnResetDevice()
{
    // Reset DXVA Manager, and get new buffers
    m_pD3DManager->ResetDevice(m_pD3DDev, m_nResetToken);

    // Not necessary, but Microsoft documentation say Presenter should send this message...
    if (m_pSink) {
        m_pSink->Notify(EC_DISPLAY_CHANGED, 0, 0);
    }
}

void CEVRAllocatorPresenter::RemoveAllSamples()
{
    CAutoLock imageProcesssingLock(&m_ImageProcessingLock);
    CAutoLock sampleQueueLock(&m_SampleQueueLock);

    FlushSamples();
    m_FreeSamples.RemoveAll();
    m_LastScheduledUncorrectedSampleTime = -1;
    m_nUsedBuffer = 0;
    // Increment the group id to make sure old samples will really be deleted
    m_nCurrentGroupId++;
}

HRESULT CEVRAllocatorPresenter::GetFreeSample(IMFSample** ppSample)
{
    CAutoLock lock(&m_SampleQueueLock);
    HRESULT hr = S_OK;

    if (!m_FreeSamples.IsEmpty()) {
        m_nUsedBuffer++;
        *ppSample = m_FreeSamples.RemoveHead().Detach();
    } else {
        hr = MF_E_SAMPLEALLOCATOR_EMPTY;
    }

    return hr;
}

HRESULT CEVRAllocatorPresenter::GetScheduledSample(IMFSample** ppSample, int& count)
{
    CAutoLock lock(&m_SampleQueueLock);
    HRESULT hr = S_OK;

    count = (int)m_ScheduledSamples.GetCount();
    if (count > 0) {
        *ppSample = m_ScheduledSamples.RemoveHead().Detach();
        --count;
    } else {
        hr = MF_E_SAMPLEALLOCATOR_EMPTY;
    }

    return hr;
}

void CEVRAllocatorPresenter::AddToFreeList(IMFSample* pSample, bool bTail)
{
    CAutoLock lock(&m_SampleQueueLock);

    m_nUsedBuffer--;
    if (bTail) {
        m_FreeSamples.AddTail(pSample);
    } else {
        m_FreeSamples.AddHead(pSample);
    }
}

void CEVRAllocatorPresenter::AddToScheduledList(IMFSample* pSample, bool bSorted)
{
    CAutoLock lock(&m_SampleQueueLock);

    if (bSorted) {
        // Insert sorted
        /*POSITION Iterator = m_ScheduledSamples.GetHeadPosition();

          LONGLONG NewSampleTime;
          pSample->GetSampleTime(&NewSampleTime);

          while (Iterator != nullptr)
          {
              POSITION CurrentPos = Iterator;
              IMFSample *pIter = m_ScheduledSamples.GetNext(Iterator);
              LONGLONG SampleTime;
              pIter->GetSampleTime(&SampleTime);
              if (NewSampleTime < SampleTime)
              {
                  m_ScheduledSamples.InsertBefore(CurrentPos, pSample);
                  return;
              }
          }*/

        m_ScheduledSamples.AddHead(pSample);
    } else {
        const CRenderersSettings& r = GetRenderersSettings();
        double ForceFPS = 0.0;
        //double ForceFPS = 59.94;
        //double ForceFPS = 23.976;
        if (ForceFPS != 0.0) {
            m_rtTimePerFrame = (REFERENCE_TIME)(10000000.0 / ForceFPS);
        }
        LONGLONG Duration = m_rtTimePerFrame;
        UNREFERENCED_PARAMETER(Duration);
        LONGLONG PrevTime = m_LastScheduledUncorrectedSampleTime;
        LONGLONG Time;
        LONGLONG SetDuration;
        pSample->GetSampleDuration(&SetDuration);
        pSample->GetSampleTime(&Time);
        m_LastScheduledUncorrectedSampleTime = Time;

        m_bCorrectedFrameTime = false;

        LONGLONG Diff2 = PrevTime - (LONGLONG)(m_LastScheduledSampleTimeFP * 10000000.0);
        LONGLONG Diff = Time - PrevTime;
        if (PrevTime == -1) {
            Diff = 0;
        }
        if (Diff < 0) {
            Diff = -Diff;
        }
        if (Diff2 < 0) {
            Diff2 = -Diff2;
        }
        if (Diff < m_rtTimePerFrame * 8 && m_rtTimePerFrame && Diff2 < m_rtTimePerFrame * 8) { // Detect seeking
            int iPos = (m_DetectedFrameTimePos++) % 60;
            Diff = Time - PrevTime;
            if (PrevTime == -1) {
                Diff = 0;
            }
            m_DetectedFrameTimeHistory[iPos] = Diff;

            if (m_DetectedFrameTimePos >= 10) {
                int nFrames = std::min(m_DetectedFrameTimePos, 60);
                LONGLONG DectedSum = 0;
                for (int i = 0; i < nFrames; ++i) {
                    DectedSum += m_DetectedFrameTimeHistory[i];
                }

                double Average = double(DectedSum) / double(nFrames);
                double DeviationSum = 0.0;
                for (int i = 0; i < nFrames; ++i) {
                    double Deviation = m_DetectedFrameTimeHistory[i] - Average;
                    DeviationSum += Deviation * Deviation;
                }

                double StdDev = sqrt(DeviationSum / double(nFrames));

                m_DetectedFrameTimeStdDev = StdDev;

                double DetectedRate = 1.0 / (double(DectedSum) / (nFrames * 10000000.0));
                double AllowedError = 0.0003;

                static double AllowedValues[] = {60.0, 59.94, 50.0, 48.0, 47.952, 30.0, 29.97, 25.0, 24.0, 23.976};

                int nAllowed = _countof(AllowedValues);
                for (int i = 0; i < nAllowed; ++i) {
                    if (fabs(1.0 - DetectedRate / AllowedValues[i]) < AllowedError) {
                        DetectedRate = AllowedValues[i];
                        break;
                    }
                }

                m_DetectedFrameTimeHistoryHistory[m_DetectedFrameTimePos % 500] = DetectedRate;

                class CAutoInt
                {
                public:

                    int m_Int;

                    CAutoInt() {
                        m_Int = 0;
                    }
                    CAutoInt(int _Other) {
                        m_Int = _Other;
                    }

                    operator int () const {
                        return m_Int;
                    }

                    CAutoInt& operator ++ () {
                        ++m_Int;
                        return *this;
                    }
                };


                CMap<double, double, CAutoInt, CAutoInt> Map;

                for (int i = 0; i < 500; ++i) {
                    ++Map[m_DetectedFrameTimeHistoryHistory[i]];
                }

                POSITION Pos = Map.GetStartPosition();
                double BestVal = 0.0;
                int BestNum = 5;
                while (Pos) {
                    double Key;
                    CAutoInt Value;
                    Map.GetNextAssoc(Pos, Key, Value);
                    if (Value.m_Int > BestNum && Key != 0.0) {
                        BestNum = Value.m_Int;
                        BestVal = Key;
                    }
                }

                m_DetectedLock = false;
                for (int i = 0; i < nAllowed; ++i) {
                    if (BestVal == AllowedValues[i]) {
                        m_DetectedLock = true;
                        break;
                    }
                }
                if (BestVal != 0.0) {
                    m_DetectedFrameRate = BestVal;
                    m_DetectedFrameTime = 1.0 / BestVal;
                }
            }

            LONGLONG PredictedNext = PrevTime + m_rtTimePerFrame;
            LONGLONG PredictedDiff = PredictedNext - Time;
            if (PredictedDiff < 0) {
                PredictedDiff = -PredictedDiff;
            }

            if (m_DetectedFrameTime != 0.0
                    //&& PredictedDiff > MIN_FRAME_TIME
                    && m_DetectedLock && r.m_AdvRendSets.bEVREnableFrameTimeCorrection) {
                double CurrentTime = Time / 10000000.0;
                double LastTime = m_LastScheduledSampleTimeFP;
                double PredictedTime = LastTime + m_DetectedFrameTime;
                if (fabs(PredictedTime - CurrentTime) > 0.0015) { // 1.5 ms wrong, lets correct
                    CurrentTime = PredictedTime;
                    Time = (LONGLONG)(CurrentTime * 10000000.0);
                    pSample->SetSampleTime(Time);
                    pSample->SetSampleDuration(LONGLONG(m_DetectedFrameTime * 10000000.0));
                    m_bCorrectedFrameTime = true;
                    m_FrameTimeCorrection = 30;
                }
                m_LastScheduledSampleTimeFP = CurrentTime;
            } else {
                m_LastScheduledSampleTimeFP = Time / 10000000.0;
            }
        } else {
            m_LastScheduledSampleTimeFP = Time / 10000000.0;
            if (Diff > m_rtTimePerFrame * 8) {
                // Seek
                m_bSignaledStarvation = false;
                m_DetectedFrameTimePos = 0;
                m_DetectedLock = false;
            }
        }

        //      TRACE_EVR("EVR: Time: %f %f %f\n", Time / 10000000.0, SetDuration / 10000000.0, m_DetectedFrameRate);
        if (!m_bCorrectedFrameTime && m_FrameTimeCorrection) {
            --m_FrameTimeCorrection;
        }

#if 0
        if (Time <= m_LastScheduledUncorrectedSampleTime && m_LastScheduledSampleTime >= 0) {
            PrevTime = m_LastScheduledSampleTime;
        }

        m_bCorrectedFrameTime = false;
        if (PrevTime != -1 && (Time >= PrevTime - ((Duration * 20) / 9) || Time == 0) || ForceFPS != 0.0) {
            if (Time - PrevTime > ((Duration * 20) / 9) && Time - PrevTime < Duration * 8 || Time == 0 || ((Time - PrevTime) < (Duration / 11)) || ForceFPS != 0.0) {
                // Error!!!!
                Time = PrevTime + Duration;
                pSample->SetSampleTime(Time);
                pSample->SetSampleDuration(Duration);
                m_bCorrectedFrameTime = true;
                TRACE_EVR("EVR: Corrected invalid sample time\n");
            }
        }
        if (Time + Duration * 10 < m_LastScheduledSampleTime) {
            // Flush when repeating movie
            FlushSamplesInternal();
        }
#endif

#if 0
        static LONGLONG LastDuration = 0;
        LONGLONG SetDuration = m_rtTimePerFrame;
        pSample->GetSampleDuration(&SetDuration);
        if (SetDuration != LastDuration) {
            TRACE_EVR("EVR: Old duration: %I64d New duration: %I64d\n", LastDuration, SetDuration);
        }
        LastDuration = SetDuration;
#endif
        m_LastScheduledSampleTime = Time;

        m_ScheduledSamples.AddTail(pSample);
    }
}

void CEVRAllocatorPresenter::FlushSamples()
{
    CAutoLock lock(this);
    CAutoLock lock2(&m_SampleQueueLock);
    CAutoLock lock3(&m_RenderLock);

    m_pCurrentlyDisplayedSample = nullptr;
    m_ScheduledSamples.RemoveAll();

    m_LastSampleOffset = 0;
    m_bLastSampleOffsetValid = false;
    m_bSignaledStarvation = false;
    m_LastScheduledSampleTime = -1;
}

HRESULT CEVRAllocatorPresenter::TrackSample(IMFSample* pSample)
{
    HRESULT hr = E_FAIL;
    if (CComQIPtr<IMFTrackedSample> pTracked = pSample) {
        hr = pTracked->SetAllocator(&m_SampleFreeCallback, nullptr);
    }
    return hr;
}

HRESULT CEVRAllocatorPresenter::OnSampleFree(IMFAsyncResult* pResult)
{
    CComPtr<IUnknown> pObject;
    HRESULT hr = pResult->GetObject(&pObject);
    if (SUCCEEDED(hr)) {
        if (CComQIPtr<IMFSample> pSample = pObject) {
            // Ignore the sample if it is from an old group
            UINT32 nGroupId;
            CAutoLock sampleQueueLock(&m_SampleQueueLock);
            if (SUCCEEDED(pSample->GetUINT32(GUID_GROUP_ID, &nGroupId)) && nGroupId == m_nCurrentGroupId) {
                AddToFreeList(pSample, true);
                pSample = nullptr; // The sample should not be used after being queued
            }
        }
    }
    return hr;
}
