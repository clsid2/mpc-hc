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
#include <algorithm>
#include "RenderersSettings.h"
#include "DX9AllocatorPresenter.h"
#include <utility>
#include "../../../SubPic/DX9SubPic.h"
#include "../../../SubPic/SubPicQueueImpl.h"
#include "IPinHook.h"
#include "FocusThread.h"
#include "../../../DSUtil/vd.h"
#include <mpc-hc_config.h>

#define FRAMERATE_MAX_DELTA 3000

#define REFERENCE_WIDTH 1920
#define FONT_HEIGHT     21
#define BOLD_THRESHOLD  11
#define TEXT_PADDING    2
#define GRAPH_HEIGHT    360
#define GRAPH_WIDTH     1000

using namespace DSObjects;

#pragma warning(push)
#pragma warning(disable: 4351) // new behavior: elements of array 'array' will be default initialized
// CDX9AllocatorPresenter
CDX9AllocatorPresenter::CDX9AllocatorPresenter(HWND hWnd, bool bFullscreen, HRESULT& hr, bool bIsEVR, CString& _Error, bool isPreview)
    : CDX9RenderingEngine(hWnd, hr, &_Error)
    , m_bAlternativeVSync(false)
    , m_bCompositionEnabled(false)
    , m_bIsEVR(bIsEVR)
    , m_OrderedPaint(0)
    , m_VSyncMode(0)
    , m_bDesktopCompositionDisabled(false)
    , m_bIsFullscreen(bFullscreen)
    , fullScreenChanged(false)
    , m_bNeedCheckSample(true)
    , m_MainThreadId(0)
    , m_bIsRendering(false)
    , m_hDWMAPI(nullptr)
    , m_pDwmIsCompositionEnabled(nullptr)
    , m_pDwmEnableComposition(nullptr)
    , m_pDirectDraw(nullptr)
    , m_LastAdapterCheck(0)
    , m_nTearingPos(0)
    , m_VMR9AlphaBitmapWidthBytes(0)
    , m_pD3DXLoadSurfaceFromMemory(nullptr)
    , m_pD3DXLoadSurfaceFromSurface(nullptr)
    , m_pD3DXCreateLine(nullptr)
    , m_pD3DXCreateFont(nullptr)
    , m_pD3DXCreateSprite(nullptr)
    , m_nVMR9Surfaces(0)
    , m_iVMR9Surface(0)
    , m_nUsedBuffer(0)
    , m_fAvrFps(0.0)
    , m_fJitterStdDev(0.0)
    , m_fJitterMean(0.0)
    , m_fSyncOffsetStdDev(0.0)
    , m_fSyncOffsetAvr(0.0)
    , m_DetectedRefreshRate(0)
    , m_DetectedRefreshTime(0.0)
    , m_DetectedRefreshTimePrim(0)
    , m_DetectedScanlineTime(0)
    , m_DetectedScanlineTimePrim(0)
    , m_DetectedScanlinesPerFrame(0.0)
    , m_ldDetectedRefreshRateList()
    , m_ldDetectedScanlineRateList()
    , m_DetectedRefreshRatePos(0)
    , m_bSyncStatsAvailable(false)
    , m_pllJitter()
    , m_pllSyncOffset()
    , m_llLastPerf(0)
    , m_JitterStdDev(0)
    , m_MaxJitter(MINLONG64)
    , m_MinJitter(MAXLONG64)
    , m_MaxSyncOffset(0)
    , m_MinSyncOffset(0)
    , m_nNextJitter(0)
    , m_nNextSyncOffset(0)
    , m_rtTimePerFrame(0)
    , m_DetectedFrameRate(0.0)
    , m_DetectedFrameTime(0.0)
    , m_DetectedFrameTimeStdDev(0.0)
    , m_DetectedLock(false)
    , m_DetectedFrameTimeHistory()
    , m_DetectedFrameTimeHistoryHistory()
    , m_DetectedFrameTimePos(0)
    , m_bInterlaced(false)
    , m_VBlankEndWait(0)
    , m_VBlankStartWait(0)
    , m_VBlankWaitTime(0)
    , m_VBlankLockTime(0)
    , m_VBlankMin(300000)
    , m_VBlankMinCalc(300000)
    , m_VBlankMax(0)
    , m_VBlankEndPresent(-100000)
    , m_VBlankStartMeasureTime(0)
    , m_VBlankStartMeasure(0)
    , m_PresentWaitTime(0)
    , m_PresentWaitTimeMin(3000000000)
    , m_PresentWaitTimeMax(0)
    , m_PaintTime(0)
    , m_PaintTimeMin(3000000000)
    , m_PaintTimeMax(0)
    , m_WaitForGPUTime(0)
    , m_RasterStatusWaitTime(0)
    , m_RasterStatusWaitTimeMin(3000000000)
    , m_RasterStatusWaitTimeMax(0)
    , m_RasterStatusWaitTimeMaxCalc(0)
    , m_ClockDiffCalc(0.0)
    , m_ClockDiffPrim(0.0)
    , m_ClockDiff(0.0)
    , m_TimeChangeHistory()
    , m_ClockChangeHistory()
    , m_ClockTimeChangeHistoryPos(0)
    , m_ModeratedTimeSpeed(1.0)
    , m_ModeratedTimeSpeedPrim(0.0)
    , m_ModeratedTimeSpeedDiff(0.0)
    , m_bCorrectedFrameTime(0)
    , m_FrameTimeCorrection(0)
    , m_LastFrameDuration(0)
    , m_LastSampleTime(0)
    , m_FocusThread(nullptr)
    , m_hFocusWindow(nullptr)
    , m_bIsPreview(isPreview)
{
    ZeroMemory(&m_VMR9AlphaBitmap, sizeof(m_VMR9AlphaBitmap));

    if (FAILED(hr)) {
        _Error += _T("ISubPicAllocatorPresenterImpl failed\n");
        return;
    }

    HINSTANCE hDll                = GetRenderersData()->GetD3X9Dll();

    if (hDll) {
        (FARPROC&)m_pD3DXLoadSurfaceFromMemory  = GetProcAddress(hDll, "D3DXLoadSurfaceFromMemory");
        (FARPROC&)m_pD3DXLoadSurfaceFromSurface = GetProcAddress(hDll, "D3DXLoadSurfaceFromSurface");
        (FARPROC&)m_pD3DXCreateLine             = GetProcAddress(hDll, "D3DXCreateLine");
        (FARPROC&)m_pD3DXCreateFont             = GetProcAddress(hDll, "D3DXCreateFontW");
        (FARPROC&)m_pD3DXCreateSprite           = GetProcAddress(hDll, "D3DXCreateSprite");
    } else {
        _Error += _T("Your system is missing the file d3dx9_43.dll\n\nTo acquire this file you must install \"DirectX End-User Runtime\" or update your MPC-HC installation.");
    }

    m_hDWMAPI = LoadLibrary(L"dwmapi.dll");
    if (m_hDWMAPI) {
        (FARPROC&)m_pDwmIsCompositionEnabled = GetProcAddress(m_hDWMAPI, "DwmIsCompositionEnabled");
        (FARPROC&)m_pDwmEnableComposition = GetProcAddress(m_hDWMAPI, "DwmEnableComposition");
    }

    const CRenderersSettings& r = GetRenderersSettings();

    if (r.m_AdvRendSets.bVMRDisableDesktopComposition) {
        m_bDesktopCompositionDisabled = true;
        if (m_pDwmEnableComposition) {
            m_pDwmEnableComposition(0);
        }
    } else {
        m_bDesktopCompositionDisabled = false;
    }

    Direct3DCreate9Ex(D3D_SDK_VERSION, &m_pD3DEx);
    if (m_pD3DEx) {
        m_pD3D = m_pD3DEx;
        hr = CreateDevice(_Error);
    } else {
        _Error += L"Failed to create D3D9\n";
        hr = E_UNEXPECTED;
    }
}
#pragma warning(pop)

CDX9AllocatorPresenter::~CDX9AllocatorPresenter()
{
    if (m_bDesktopCompositionDisabled) {
        m_bDesktopCompositionDisabled = false;
        if (m_pDwmEnableComposition) {
            m_pDwmEnableComposition(1);
        }
    }

    m_pFont       = nullptr;
    m_pLine       = nullptr;
    m_pSprite     = nullptr;
    m_pOSDTexture = nullptr;
    m_pOSDSurface = nullptr;
    m_pD3DDev     = nullptr;
    m_pD3DDevEx   = nullptr;

    m_pAlphaBitmapTexture.Release();
    CleanupRenderingEngine();

    m_pD3D   = nullptr;
    m_pD3DEx = nullptr;
    m_pSubPicQueue = nullptr;
    m_pAllocator = nullptr;

    if (m_FocusThread) {
        m_FocusThread->PostThreadMessage(WM_QUIT, 0, 0);
        if (WaitForSingleObject(m_FocusThread->m_hThread, 10000) == WAIT_TIMEOUT) {
            ASSERT(FALSE);
            TerminateThread(m_FocusThread->m_hThread, 0xDEAD);
        }
    }

    if (m_hDWMAPI) {
        FreeLibrary(m_hDWMAPI);
        m_hDWMAPI = nullptr;
    }
}

void ModerateFloat(double& Value, double Target, double& ValuePrim, double ChangeSpeed);

#if 0
class CRandom31
{
public:

    CRandom31() {
        m_Seed = 12164;
    }

    void f_SetSeed(int32 _Seed) {
        m_Seed = _Seed;
    }
    int32 f_GetSeed() {
        return m_Seed;
    }
    /*
    Park and Miller's psuedo-random number generator.
    */
    int32 m_Seed;
    int32 f_Get() {
        static const int32 A = 16807;
        static const int32 M = 2147483647;      // 2^31 - 1
        static const int32 q = M / A;           // M / A
        static const int32 r = M % A;           // M % A
        m_Seed = A * (m_Seed % q) - r * (m_Seed / q);
        if (m_Seed < 0) {
            m_Seed += M;
        }
        return m_Seed;
    }

    static int32 fs_Max() {
        return 2147483646;
    }

    double f_GetFloat() {
        return double(f_Get()) * (1.0 / double(fs_Max()));
    }
};

class CVSyncEstimation
{
private:
    class CHistoryEntry
    {
    public:
        CHistoryEntry() {
            m_Time = 0;
            m_ScanLine = -1;
        }
        LONGLONG m_Time;
        int m_ScanLine;
    };

    class CSolution
    {
    public:
        CSolution() {
            m_ScanLines = 1000;
            m_ScanLinesPerSecond = m_ScanLines * 100;
        }
        int m_ScanLines;
        double m_ScanLinesPerSecond;
        double m_SqrSum;

        void f_Mutate(double _Amount, CRandom31& _Random, int _MinScans) {
            int ToDo = _Random.f_Get() % 10;
            if (ToDo == 0) {
                m_ScanLines = m_ScanLines / 2;
            } else if (ToDo == 1) {
                m_ScanLines = m_ScanLines * 2;
            }

            m_ScanLines = m_ScanLines * (1.0 + (_Random.f_GetFloat() * _Amount) - _Amount * 0.5);
            m_ScanLines = max(m_ScanLines, _MinScans);

            if (ToDo == 2) {
                m_ScanLinesPerSecond /= (_Random.f_Get() % 4) + 1;
            } else if (ToDo == 3) {
                m_ScanLinesPerSecond *= (_Random.f_Get() % 4) + 1;
            }

            m_ScanLinesPerSecond *= 1.0 + (_Random.f_GetFloat() * _Amount) - _Amount * 0.5;
        }

        void f_SpawnInto(CSolution& _Other, CRandom31& _Random, int _MinScans) {
            _Other = *this;
            _Other.f_Mutate(_Random.f_GetFloat() * 0.1, _Random, _MinScans);
        }

        static int fs_Compare(const void* _pFirst, const void* _pSecond) {
            const CSolution* pFirst = (const CSolution*)_pFirst;
            const CSolution* pSecond = (const CSolution*)_pSecond;
            if (pFirst->m_SqrSum < pSecond->m_SqrSum) {
                return -1;
            } else if (pFirst->m_SqrSum > pSecond->m_SqrSum) {
                return 1;
            }
            return 0;
        }


    };

    enum {
        ENumHistory = 128
    };

    CHistoryEntry m_History[ENumHistory];
    int m_iHistory;
    CSolution m_OldSolutions[2];

    CRandom31 m_Random;


    double fp_GetSquareSum(double _ScansPerSecond, double _ScanLines) {
        double SquareSum = 0;
        int nHistory = min(m_nHistory, ENumHistory);
        int iHistory = m_iHistory - nHistory;
        if (iHistory < 0) {
            iHistory += ENumHistory;
        }
        for (int i = 1; i < nHistory; ++i) {
            int iHistory0 = iHistory + i - 1;
            int iHistory1 = iHistory + i;
            if (iHistory0 < 0) {
                iHistory0 += ENumHistory;
            }
            iHistory0 = iHistory0 % ENumHistory;
            iHistory1 = iHistory1 % ENumHistory;
            ASSERT(m_History[iHistory0].m_Time != 0);
            ASSERT(m_History[iHistory1].m_Time != 0);

            double DeltaTime = (m_History[iHistory1].m_Time - m_History[iHistory0].m_Time) / 10000000.0;
            double PredictedScanLine = m_History[iHistory0].m_ScanLine + DeltaTime * _ScansPerSecond;
            PredictedScanLine = fmod(PredictedScanLine, _ScanLines);
            double Delta = (m_History[iHistory1].m_ScanLine - PredictedScanLine);
            double DeltaSqr = Delta * Delta;
            SquareSum += DeltaSqr;
        }
        return SquareSum;
    }

    int m_nHistory;
public:

    CVSyncEstimation() {
        m_iHistory = 0;
        m_nHistory = 0;
    }

    void f_AddSample(int _ScanLine, LONGLONG _Time) {
        m_History[m_iHistory].m_ScanLine = _ScanLine;
        m_History[m_iHistory].m_Time = _Time;
        ++m_nHistory;
        m_iHistory = (m_iHistory + 1) % ENumHistory;
    }

    void f_GetEstimation(double& _RefreshRate, int& _ScanLines, int _ScreenSizeY, int _WindowsRefreshRate) {
        _RefreshRate = 0;
        _ScanLines = 0;

        int iHistory = m_iHistory;
        // We have a full history
        if (m_nHistory > 10) {
            for (int l = 0; l < 5; ++l) {
                const int nSol = 3 + 5 + 5 + 3;
                CSolution Solutions[nSol];

                Solutions[0] = m_OldSolutions[0];
                Solutions[1] = m_OldSolutions[1];
                Solutions[2].m_ScanLines = _ScreenSizeY;
                Solutions[2].m_ScanLinesPerSecond = _ScreenSizeY * _WindowsRefreshRate;

                int iStart = 3;
                for (int i = iStart; i < iStart + 5; ++i) {
                    Solutions[0].f_SpawnInto(Solutions[i], m_Random, _ScreenSizeY);
                }
                iStart += 5;
                for (int i = iStart; i < iStart + 5; ++i) {
                    Solutions[1].f_SpawnInto(Solutions[i], m_Random, _ScreenSizeY);
                }
                iStart += 5;
                for (int i = iStart; i < iStart + 3; ++i) {
                    Solutions[2].f_SpawnInto(Solutions[i], m_Random, _ScreenSizeY);
                }

                int Start = 2;
                if (l == 0) {
                    Start = 0;
                }
                for (int i = Start; i < nSol; ++i) {
                    Solutions[i].m_SqrSum = fp_GetSquareSum(Solutions[i].m_ScanLinesPerSecond, Solutions[i].m_ScanLines);
                }

                qsort(Solutions, nSol, sizeof(Solutions[0]), &CSolution::fs_Compare);
                for (int i = 0; i < 2; ++i) {
                    m_OldSolutions[i] = Solutions[i];
                }
            }

            _ScanLines = m_OldSolutions[0].m_ScanLines + 0.5;
            _RefreshRate = 1.0 / (m_OldSolutions[0].m_ScanLines / m_OldSolutions[0].m_ScanLinesPerSecond);
        } else {
            m_OldSolutions[0].m_ScanLines = _ScreenSizeY;
            m_OldSolutions[1].m_ScanLines = _ScreenSizeY;
        }
    }
};
#endif

bool CDX9AllocatorPresenter::SettingsNeedResetDevice(CRenderersSettings& r)
{
    CRenderersSettings::CAdvRendererSettings& New = r.m_AdvRendSets;
    CRenderersSettings::CAdvRendererSettings& Current = m_LastRendererSettings;

    bool bRet = false;

    bRet = bRet || New.bVMR9AlterativeVSync != Current.bVMR9AlterativeVSync;
    bRet = bRet || New.bVMR9VSyncAccurate != Current.bVMR9VSyncAccurate;

    if (m_bIsFullscreen) {
        bRet = bRet || New.bVMR9FullscreenGUISupport != Current.bVMR9FullscreenGUISupport;
    } else {
        if (Current.bVMRDisableDesktopComposition) {
            if (!m_bDesktopCompositionDisabled) {
                m_bDesktopCompositionDisabled = true;
                if (m_pDwmEnableComposition) {
                    m_pDwmEnableComposition(0);
                }
            }
        } else {
            if (m_bDesktopCompositionDisabled) {
                m_bDesktopCompositionDisabled = false;
                if (m_pDwmEnableComposition) {
                    m_pDwmEnableComposition(1);
                }
            }
        }
    }

    if (m_bIsEVR) {
        bRet = bRet || New.bEVRHighColorResolution != Current.bEVRHighColorResolution;
        bRet = bRet || New.bEVRForceInputHighColorResolution != Current.bEVRForceInputHighColorResolution;
    }

    m_LastRendererSettings = r.m_AdvRendSets;

    return bRet;
}

HRESULT CDX9AllocatorPresenter::CreateDevice(CString& _Error)
{
    const CRenderersSettings& r = GetRenderersSettings();
    CRenderersData* rd = GetRenderersData();

    m_VBlankEndWait = 0;
    m_VBlankMin = 300000;
    m_VBlankMinCalc = 300000;
    m_VBlankMax = 0;
    m_VBlankStartWait = 0;
    m_VBlankWaitTime = 0;
    m_VBlankLockTime = 0;
    m_PresentWaitTime = 0;
    m_PresentWaitTimeMin = 3000000000;
    m_PresentWaitTimeMax = 0;

    m_LastRendererSettings = r.m_AdvRendSets;

    m_VBlankEndPresent = -100000;
    m_VBlankStartMeasureTime = 0;
    m_VBlankStartMeasure = 0;

    m_PaintTime = 0;
    m_PaintTimeMin = 3000000000;
    m_PaintTimeMax = 0;

    m_RasterStatusWaitTime = 0;
    m_RasterStatusWaitTimeMin = 3000000000;
    m_RasterStatusWaitTimeMax = 0;
    m_RasterStatusWaitTimeMaxCalc = 0;

    m_ClockDiff = 0.0;
    m_ClockDiffPrim = 0.0;
    m_ClockDiffCalc = 0.0;

    m_ModeratedTimeSpeed = 1.0;
    m_ModeratedTimeSpeedDiff = 0.0;
    m_ModeratedTimeSpeedPrim = 0;
    ZeroMemory(m_TimeChangeHistory, sizeof(m_TimeChangeHistory));
    ZeroMemory(m_ClockChangeHistory, sizeof(m_ClockChangeHistory));
    m_ClockTimeChangeHistoryPos = 0;

    m_pSprite = nullptr;
    m_pLine = nullptr;
    m_pFont = nullptr;
    m_pOSDTexture = nullptr;
    m_pOSDSurface = nullptr;
    m_pDirectDraw = nullptr;

    m_bAlphaBitmapEnable = false;
    m_pAlphaBitmapTexture.Release();
    ZeroMemory(&m_AlphaBitmapParams, sizeof(m_AlphaBitmapParams));

    CleanupRenderingEngine();

    if (!m_pD3D) {
        _Error += L"Failed to create D3D9\n";
        return E_UNEXPECTED;
    }

    UINT currentAdapter = GetAdapter(m_pD3D);
    bool bTryToReset = (currentAdapter == m_CurrentAdapter);

    if (!bTryToReset) {
        m_pD3DDev = nullptr;
        m_pD3DDevEx = nullptr;
        m_CurrentAdapter = currentAdapter;
    }

    HRESULT hr = S_OK;

    //#define ENABLE_DDRAWSYNC
#ifdef ENABLE_DDRAWSYNC
    hr = DirectDrawCreate(nullptr, &m_pDirectDraw, nullptr);
    if (hr == S_OK) {
        hr = m_pDirectDraw->SetCooperativeLevel(m_hWnd, DDSCL_NORMAL);
    }
#endif

    D3DPRESENT_PARAMETERS pp;
    ZeroMemory(&pp, sizeof(pp));

    BOOL bCompositionEnabled = false;
    if (m_pDwmIsCompositionEnabled) {
        m_pDwmIsCompositionEnabled(&bCompositionEnabled);
    }

    m_bCompositionEnabled = !!bCompositionEnabled;
    m_bAlternativeVSync = r.m_AdvRendSets.bVMR9AlterativeVSync;

    // detect FP16 textures support
    rd->m_bFP16Support = SUCCEEDED(m_pD3D->CheckDeviceFormat(m_CurrentAdapter, D3DDEVTYPE_HAL, D3DFMT_X8R8G8B8, D3DUSAGE_QUERY_FILTER, D3DRTYPE_VOLUMETEXTURE, D3DFMT_A16B16G16R16F));

    // detect FP32 textures support
    rd->m_bFP32Support = SUCCEEDED(m_pD3D->CheckDeviceFormat(m_CurrentAdapter, D3DDEVTYPE_HAL, D3DFMT_X8R8G8B8, D3DUSAGE_QUERY_FILTER, D3DRTYPE_VOLUMETEXTURE, D3DFMT_A32B32G32R32F));

    // detect 10-bit textures support
    rd->m_b10bitSupport = SUCCEEDED(m_pD3D->CheckDeviceFormat(m_CurrentAdapter, D3DDEVTYPE_HAL, D3DFMT_X8R8G8B8, D3DUSAGE_QUERY_FILTER, D3DRTYPE_TEXTURE, D3DFMT_A2R10G10B10));

    // detect 10-bit device support
    bool bHighColorSupport = SUCCEEDED(m_pD3D->CheckDeviceType(m_CurrentAdapter, D3DDEVTYPE_HAL, D3DFMT_A2R10G10B10, D3DFMT_A2R10G10B10, FALSE));

    // set settings that depend on hardware feature support
    m_bForceInputHighColorResolution = r.m_AdvRendSets.bEVRForceInputHighColorResolution && m_bIsEVR && rd->m_b10bitSupport;
    m_bHighColorResolution = r.m_AdvRendSets.bEVRHighColorResolution && m_bIsEVR && rd->m_b10bitSupport && bHighColorSupport;
    m_bFullFloatingPointProcessing = r.m_AdvRendSets.bVMR9FullFloatingPointProcessing && rd->m_bFP32Support;
    m_bHalfFloatingPointProcessing = r.m_AdvRendSets.bVMR9HalfFloatingPointProcessing && rd->m_bFP16Support && !m_bFullFloatingPointProcessing;

    // set color formats
    if (m_bFullFloatingPointProcessing) {
        m_SurfaceType = D3DFMT_A32B32G32R32F;
    } else if (m_bHalfFloatingPointProcessing) {
        m_SurfaceType = D3DFMT_A16B16G16R16F;
    } else if (m_bForceInputHighColorResolution || m_bHighColorResolution) {
        m_SurfaceType = D3DFMT_A2R10G10B10;
    } else {
        m_SurfaceType = D3DFMT_X8R8G8B8;
    }

    CSize largestScreen = GetLargestScreenSize(CSize(2560, 1440));

    D3DDISPLAYMODEEX DisplayMode;
    ZeroMemory(&DisplayMode, sizeof(DisplayMode));
    DisplayMode.Size = sizeof(DisplayMode);
    D3DDISPLAYMODE d3ddm;
    ZeroMemory(&d3ddm, sizeof(d3ddm));

    if (m_bIsFullscreen) {
        if (m_bHighColorResolution) {
            pp.BackBufferFormat = D3DFMT_A2R10G10B10;
        } else {
            pp.BackBufferFormat = D3DFMT_X8R8G8B8;
        }
        pp.Windowed = false;
        pp.BackBufferCount = 3;
        pp.SwapEffect = D3DSWAPEFFECT_FLIP;
        // there's no Desktop composition to take care of alternative vSync in exclusive mode, alternative vSync is therefore unused
        pp.hDeviceWindow = m_hWnd;
        pp.Flags = D3DPRESENTFLAG_VIDEO;
        if (r.m_AdvRendSets.bVMR9FullscreenGUISupport && !m_bHighColorResolution) {
            pp.Flags |= D3DPRESENTFLAG_LOCKABLE_BACKBUFFER;
        }

        if (!m_FocusThread) {
            m_FocusThread = (CFocusThread*)AfxBeginThread(RUNTIME_CLASS(CFocusThread), 0, 0, 0);
        }

        HWND hFocusWindow = m_FocusThread->GetFocusWindow();
        bTryToReset &= (m_hFocusWindow == hFocusWindow || fullScreenChanged);
        m_hFocusWindow = hFocusWindow;

        if (m_pD3DEx) {
            CHECK_HR(m_pD3DEx->GetAdapterDisplayModeEx(m_CurrentAdapter, &DisplayMode, nullptr));

            m_ScreenSize.SetSize(DisplayMode.Width, DisplayMode.Height);
            m_BackBufferSize = m_ScreenSize;

            DisplayMode.Format = pp.BackBufferFormat;
            pp.FullScreen_RefreshRateInHz = m_refreshRate = DisplayMode.RefreshRate;
            pp.BackBufferWidth = m_BackBufferSize.cx;
            pp.BackBufferHeight = m_BackBufferSize.cy;

            bTryToReset = bTryToReset && m_pD3DDevEx && SUCCEEDED(hr = m_pD3DDevEx->ResetEx(&pp, &DisplayMode));

            if (!bTryToReset) {
                m_pD3DDev = nullptr;
                m_pD3DDevEx = nullptr;
                hr = m_pD3DEx->CreateDeviceEx(
                         m_CurrentAdapter, D3DDEVTYPE_HAL, m_hFocusWindow,
                         GetVertexProcessing() | D3DCREATE_FPU_PRESERVE | D3DCREATE_MULTITHREADED | D3DCREATE_ENABLE_PRESENTSTATS | D3DCREATE_NOWINDOWCHANGES, //D3DCREATE_MANAGED
                         &pp, &DisplayMode, &m_pD3DDevEx);
            }

            if (FAILED(hr)) {
                m_D3DDevExError = GetWindowsErrorMessage(hr, nullptr);
            } else {
                m_D3DDevExError.Empty();
            }

            if (m_pD3DDevEx) {
                m_pD3DDev = m_pD3DDevEx;
                m_BackbufferType = pp.BackBufferFormat;
                m_DisplayType = DisplayMode.Format;
            }
        } else {
            m_D3DDevExError = _T("No m_pD3DEx");
        }
        if (bTryToReset && m_pD3DDev && !m_pD3DDevEx) {
            if (FAILED(hr = m_pD3DDev->Reset(&pp))) {
                m_pD3DDev = nullptr;
            }
        }
        if (!m_pD3DDev) {
            CHECK_HR(m_pD3D->GetAdapterDisplayMode(m_CurrentAdapter, &d3ddm));
            d3ddm.Format = pp.BackBufferFormat;

            m_ScreenSize.SetSize(d3ddm.Width, d3ddm.Height);
            m_BackBufferSize = m_ScreenSize;

            pp.FullScreen_RefreshRateInHz = m_refreshRate = d3ddm.RefreshRate;
            pp.BackBufferWidth = m_BackBufferSize.cx;
            pp.BackBufferHeight = m_BackBufferSize.cy;

            hr = m_pD3D->CreateDevice(
                     m_CurrentAdapter, D3DDEVTYPE_HAL, m_hFocusWindow,
                     GetVertexProcessing() | D3DCREATE_FPU_PRESERVE | D3DCREATE_MULTITHREADED | D3DCREATE_NOWINDOWCHANGES, //D3DCREATE_MANAGED
                     &pp, &m_pD3DDev);
            m_DisplayType = d3ddm.Format;
            m_BackbufferType = pp.BackBufferFormat;
        }
        if (m_pD3DDev && r.m_AdvRendSets.bVMR9FullscreenGUISupport && !m_bHighColorResolution) {
            m_pD3DDev->SetDialogBoxMode(true);
            //if (m_pD3DDev->SetDialogBoxMode(true) != S_OK)
            //  ExitProcess(0);
        }
    } else {
        pp.Windowed = TRUE;
        pp.hDeviceWindow = m_hWnd;
        pp.SwapEffect = D3DSWAPEFFECT_COPY;
        pp.Flags = D3DPRESENTFLAG_VIDEO;
        pp.BackBufferCount = 1;
        if (bCompositionEnabled || m_bAlternativeVSync) {
            // Desktop composition takes care of the VSYNC
            pp.PresentationInterval = D3DPRESENT_INTERVAL_IMMEDIATE;
        }

        bTryToReset &= (m_hFocusWindow == m_hWnd || fullScreenChanged);
        m_hFocusWindow = m_hWnd;

        if (m_pD3DEx) {
            HRESULT hr_adm = m_pD3DEx->GetAdapterDisplayModeEx(m_CurrentAdapter, &DisplayMode, nullptr);
            if (FAILED(hr_adm)) {
                ASSERT(false);
                return E_FAIL;
            }

            m_ScreenSize.SetSize(DisplayMode.Width, DisplayMode.Height);
            m_refreshRate = DisplayMode.RefreshRate;
            m_BackBufferSize = GetBackBufferSize(m_ScreenSize, largestScreen, r.m_AdvRendSets.bDesktopSizeBackBuffer);
            pp.BackBufferWidth  = m_BackBufferSize.cx;
            pp.BackBufferHeight = m_BackBufferSize.cy;

            bTryToReset = bTryToReset && m_pD3DDevEx && SUCCEEDED(hr = m_pD3DDevEx->ResetEx(&pp, nullptr));

            if (!bTryToReset) {
                m_pD3DDev = nullptr;
                m_pD3DDevEx = nullptr;
                // We can get 0x8876086a here when switching from two displays to one display using Win + P (Windows 7)
                // Cause: We might not reinitialize dx correctly during the switch
                hr = m_pD3DEx->CreateDeviceEx(
                         m_CurrentAdapter, D3DDEVTYPE_HAL, m_hFocusWindow,
                         GetVertexProcessing() | D3DCREATE_FPU_PRESERVE | D3DCREATE_MULTITHREADED | D3DCREATE_ENABLE_PRESENTSTATS, //D3DCREATE_MANAGED
                         &pp, nullptr, &m_pD3DDevEx);
            }
            if (m_pD3DDevEx) {
                m_pD3DDev = m_pD3DDevEx;
                m_DisplayType = DisplayMode.Format;
            }
        }
        if (bTryToReset && m_pD3DDev && !m_pD3DDevEx) {
            if (FAILED(hr = m_pD3DDev->Reset(&pp))) {
                m_pD3DDev = nullptr;
            }
        }
        if (!m_pD3DDev) {
            CHECK_HR(m_pD3D->GetAdapterDisplayMode(m_CurrentAdapter, &d3ddm));
            m_ScreenSize.SetSize(d3ddm.Width, d3ddm.Height);
            m_refreshRate = d3ddm.RefreshRate;
            m_BackBufferSize = GetBackBufferSize(m_ScreenSize, largestScreen, r.m_AdvRendSets.bDesktopSizeBackBuffer);
            pp.BackBufferWidth  = m_BackBufferSize.cx;
            pp.BackBufferHeight = m_BackBufferSize.cy;

            hr = m_pD3D->CreateDevice(
                     m_CurrentAdapter, D3DDEVTYPE_HAL, m_hFocusWindow,
                     GetVertexProcessing() | D3DCREATE_FPU_PRESERVE | D3DCREATE_MULTITHREADED, //D3DCREATE_MANAGED
                     &pp, &m_pD3DDev);
            m_DisplayType = d3ddm.Format;
        }
        m_BackbufferType = pp.BackBufferFormat;
    }

    if (m_pD3DDev) {
        while (hr == D3DERR_DEVICELOST) {
            TRACE(_T("D3DERR_DEVICELOST. Trying to Reset.\n"));
            hr = m_pD3DDev->TestCooperativeLevel();
        }
        if (hr == D3DERR_DEVICENOTRESET) {
            TRACE(_T("D3DERR_DEVICENOTRESET\n"));
            hr = m_pD3DDev->Reset(&pp);
        }

        if (m_pD3DDevEx) {
            m_pD3DDevEx->SetGPUThreadPriority(7);
        }
    }

    if (FAILED(hr)) {
        _Error += L"CreateDevice failed\n";
        CStringW str;
        str.Format(L"Error code: 0x%X\n", hr);
        _Error += str;

        return hr;
    }

    ASSERT(m_pD3DDev);

    if (m_ScreenSize.cx <= 0 || m_ScreenSize.cy <= 0) {
        _Error += L"Invalid screen size\n";
        return E_FAIL;
    }

    m_MainThreadId = GetCurrentThreadId();

    // Get the device caps
    ZeroMemory(&m_Caps, sizeof(m_Caps));
    m_pD3DDev->GetDeviceCaps(&m_Caps);

    // Initialize the rendering engine
    InitRenderingEngine();

    if (!m_bIsPreview) {
        CComPtr<ISubPicProvider> pSubPicProvider;
        if (m_pSubPicQueue) {
            m_pSubPicQueue->GetSubPicProvider(&pSubPicProvider);
        }

        InitMaxSubtitleTextureSize(r.subPicQueueSettings.nMaxResX, r.subPicQueueSettings.nMaxResY, largestScreen);

        if (m_pAllocator) {
            m_pAllocator->ChangeDevice(m_pD3DDev);
        } else {
            m_pAllocator = DEBUG_NEW CDX9SubPicAllocator(m_pD3DDev, m_maxSubtitleTextureSize, false);
        }

        hr = S_OK;
        if (!m_pSubPicQueue) {
            CAutoLock cAutoLock(this);
            m_pSubPicQueue = r.subPicQueueSettings.nSize > 0
                ? (ISubPicQueue*)DEBUG_NEW CSubPicQueue(r.subPicQueueSettings, m_pAllocator, &hr)
                : (ISubPicQueue*)DEBUG_NEW CSubPicQueueNoThread(r.subPicQueueSettings, m_pAllocator, &hr);
        } else {
            m_pSubPicQueue->Invalidate();
        }

        if (FAILED(hr)) {
            _Error += L"m_pSubPicQueue failed\n";

            return hr;
        }

        if (pSubPicProvider) {
            m_pSubPicQueue->SetSubPicProvider(pSubPicProvider);
        }
    }

    m_LastAdapterCheck = rd->GetPerfCounter();

    return S_OK;
}

HRESULT CDX9AllocatorPresenter::AllocSurfaces()
{
    CAutoLock cAutoLock(this);
    CAutoLock cRenderLock(&m_RenderLock);

    return CreateVideoSurfaces();
}

void CDX9AllocatorPresenter::DeleteSurfaces()
{
    CAutoLock cAutoLock(this);
    CAutoLock cRenderLock(&m_RenderLock);

    FreeVideoSurfaces();
}

UINT CDX9AllocatorPresenter::GetAdapter(IDirect3D9* pD3D, bool bGetAdapter)
{
    if (m_hWnd == nullptr || pD3D == nullptr) {
        return D3DADAPTER_DEFAULT;
    }

    m_D3D9Device = _T("");

    const CRenderersSettings& r = GetRenderersSettings();
    if (bGetAdapter && (pD3D->GetAdapterCount() > 1) && !r.D3D9RenderDevice.IsEmpty()) {
        TCHAR strGUID[50];
        D3DADAPTER_IDENTIFIER9 adapterIdentifier;

        for (UINT adp = 0, num_adp = pD3D->GetAdapterCount(); adp < num_adp; ++adp) {
            if (pD3D->GetAdapterIdentifier(adp, 0, &adapterIdentifier) == S_OK) {
                if ((::StringFromGUID2(adapterIdentifier.DeviceIdentifier, strGUID, 50) > 0) && (r.D3D9RenderDevice == strGUID)) {
                    m_D3D9Device = adapterIdentifier.Description;
                    return  adp;
                }
            }
        }
    }

    HMONITOR hMonitor = MonitorFromWindow(m_hWnd, MONITOR_DEFAULTTONEAREST);
    if (hMonitor == nullptr) {
        return D3DADAPTER_DEFAULT;
    }

    for (UINT adp = 0, num_adp = pD3D->GetAdapterCount(); adp < num_adp; ++adp) {
        HMONITOR hAdpMon = pD3D->GetAdapterMonitor(adp);
        if (hAdpMon == hMonitor) {
            if (bGetAdapter) {
                D3DADAPTER_IDENTIFIER9 adapterIdentifier;
                if (pD3D->GetAdapterIdentifier(adp, 0, &adapterIdentifier) == S_OK) {
                    m_D3D9Device = adapterIdentifier.Description;
                }
            }
            return adp;
        }
    }

    return D3DADAPTER_DEFAULT;
}

DWORD CDX9AllocatorPresenter::GetVertexProcessing()
{
    HRESULT hr;
    D3DCAPS9 caps;

    hr = m_pD3D->GetDeviceCaps(m_CurrentAdapter, D3DDEVTYPE_HAL, &caps);
    if (FAILED(hr)) {
        return D3DCREATE_SOFTWARE_VERTEXPROCESSING;
    }

    if ((caps.DevCaps & D3DDEVCAPS_HWTRANSFORMANDLIGHT) == 0 ||
            caps.VertexShaderVersion < D3DVS_VERSION(2, 0)) {
        return D3DCREATE_SOFTWARE_VERTEXPROCESSING;
    }

    return D3DCREATE_HARDWARE_VERTEXPROCESSING;
}

// ISubPicAllocatorPresenter

STDMETHODIMP CDX9AllocatorPresenter::CreateRenderer(IUnknown** ppRenderer)
{
    return E_NOTIMPL;
}

void CDX9AllocatorPresenter::CalculateJitter(LONGLONG PerfCounter)
{
    // Calculate the jitter!
    LONGLONG llPerf = PerfCounter;
    if ((m_rtTimePerFrame != 0) && (labs((long)(llPerf - m_llLastPerf)) < m_rtTimePerFrame * 3)) {
        m_nNextJitter = (m_nNextJitter + 1) % NB_JITTER;
        m_pllJitter[m_nNextJitter] = llPerf - m_llLastPerf;

        m_MaxJitter = MINLONG64;
        m_MinJitter = MAXLONG64;

        // Calculate the real FPS
        LONGLONG llJitterSum = 0;
        LONGLONG llJitterSumAvg = 0;
        for (int i = 0; i < NB_JITTER; i++) {
            LONGLONG Jitter = m_pllJitter[i];
            llJitterSum += Jitter;
            llJitterSumAvg += Jitter;
        }
        double FrameTimeMean = double(llJitterSumAvg) / NB_JITTER;
        m_fJitterMean = FrameTimeMean;
        double DeviationSum = 0;
        for (int i = 0; i < NB_JITTER; i++) {
            LONGLONG DevInt = m_pllJitter[i] - (LONGLONG)FrameTimeMean;
            double Deviation = (double)DevInt;
            DeviationSum += Deviation * Deviation;
            m_MaxJitter = std::max(m_MaxJitter, DevInt);
            m_MinJitter = std::min(m_MinJitter, DevInt);
        }
        double StdDev = sqrt(DeviationSum / NB_JITTER);

        m_fJitterStdDev = StdDev;
        m_fAvrFps = 10000000.0 / (double(llJitterSum) / NB_JITTER);
    }

    m_llLastPerf = llPerf;
}

bool CDX9AllocatorPresenter::GetVBlank(int& _ScanLine, int& _bInVBlank, bool _bMeasureTime)
{
    LONGLONG llPerf = 0;
    if (_bMeasureTime) {
        llPerf = GetRenderersData()->GetPerfCounter();
    }

    int ScanLine = 0;
    _ScanLine = 0;
    _bInVBlank = 0;

    if (m_pDirectDraw) {
        DWORD ScanLineGet = 0;
        m_pDirectDraw->GetScanLine(&ScanLineGet);
        BOOL InVBlank;
        if (m_pDirectDraw->GetVerticalBlankStatus(&InVBlank) != S_OK) {
            return false;
        }
        ScanLine = ScanLineGet;
        _bInVBlank = InVBlank;
        if (InVBlank) {
            ScanLine = 0;
        }
    } else {
        D3DRASTER_STATUS RasterStatus;
        if (m_pD3DDev->GetRasterStatus(0, &RasterStatus) != S_OK) {
            return false;
        }
        ScanLine = RasterStatus.ScanLine;
        _bInVBlank = RasterStatus.InVBlank;
    }
    if (_bMeasureTime) {
        m_VBlankMax = std::max(m_VBlankMax, ScanLine);
        if (ScanLine != 0 && !_bInVBlank) {
            m_VBlankMinCalc = std::min(m_VBlankMinCalc, ScanLine);
        }
        m_VBlankMin = m_VBlankMax - m_ScreenSize.cy;
    }
    if (_bInVBlank) {
        _ScanLine = 0;
    } else if (m_VBlankMin != 300000) {
        _ScanLine = ScanLine - m_VBlankMin;
    } else {
        _ScanLine = ScanLine;
    }

    if (_bMeasureTime) {
        LONGLONG Time = GetRenderersData()->GetPerfCounter() - llPerf;
        if (Time > 5000000) { // 0.5 sec
            TRACE(_T("GetVBlank too long (%f sec)\n"), Time / 10000000.0);
        }
        m_RasterStatusWaitTimeMaxCalc = std::max(m_RasterStatusWaitTimeMaxCalc, Time);
    }

    return true;
}

bool CDX9AllocatorPresenter::WaitForVBlankRange(int& _RasterStart, int _RasterSize, bool _bWaitIfInside, bool _bNeedAccurate, bool _bMeasure, HANDLE& lockOwner)
{
    if (_bMeasure) {
        m_RasterStatusWaitTimeMaxCalc = 0;
    }
    bool bWaited = false;
    int ScanLine = 0;
    int InVBlank = 0;
    LONGLONG llPerf = 0;
    if (_bMeasure) {
        llPerf = GetRenderersData()->GetPerfCounter();
    }
    GetVBlank(ScanLine, InVBlank, _bMeasure);
    if (_bMeasure) {
        m_VBlankStartWait = ScanLine;
    }

    static bool bOneWait = true;
    if (bOneWait && _bMeasure) {
        bOneWait = false;
        // If we are already in the wanted interval we need to wait until we aren't, this improves sync when for example you are playing 23.976 Hz material on a 24 Hz refresh rate
        int nInVBlank = 0;
        for (;;) {
            if (!GetVBlank(ScanLine, InVBlank, _bMeasure)) {
                break;
            }

            if (InVBlank && nInVBlank == 0) {
                nInVBlank = 1;
            } else if (!InVBlank && nInVBlank == 1) {
                nInVBlank = 2;
            } else if (InVBlank && nInVBlank == 2) {
                nInVBlank = 3;
            } else if (!InVBlank && nInVBlank == 3) {
                break;
            }
        }
    }
    if (_bWaitIfInside) {
        int ScanLineDiff = long(ScanLine) - _RasterStart;
        if (ScanLineDiff > m_ScreenSize.cy / 2) {
            ScanLineDiff -= m_ScreenSize.cy;
        } else if (ScanLineDiff < -m_ScreenSize.cy / 2) {
            ScanLineDiff += m_ScreenSize.cy;
        }

        if (ScanLineDiff >= 0 && ScanLineDiff <= _RasterSize) {
            bWaited = true;
            // If we are already in the wanted interval we need to wait until we aren't, this improves sync when for example you are playing 23.976 Hz material on a 24 Hz refresh rate
            int LastLineDiff = ScanLineDiff;
            for (;;) {
                if (!GetVBlank(ScanLine, InVBlank, _bMeasure)) {
                    break;
                }
                ScanLineDiff = long(ScanLine) - _RasterStart;
                if (ScanLineDiff > m_ScreenSize.cy / 2) {
                    ScanLineDiff -= m_ScreenSize.cy;
                } else if (ScanLineDiff < -m_ScreenSize.cy / 2) {
                    ScanLineDiff += m_ScreenSize.cy;
                }
                if (!(ScanLineDiff >= 0 && ScanLineDiff <= _RasterSize) || (LastLineDiff < 0 && ScanLineDiff > 0)) {
                    break;
                }
                LastLineDiff = ScanLineDiff;
                Sleep(1); // Just sleep
            }
        }
    }
    double RefreshRate = GetRefreshRate();
    LONG ScanLines = GetScanLines();
    int MinRange = std::max(std::min(long(0.0015 * ScanLines * RefreshRate + 0.5), ScanLines / 3l), 5l); // 1.5 ms or max 33 % of Time
    int NoSleepStart = _RasterStart - MinRange;
    int NoSleepRange = MinRange;
    if (NoSleepStart < 0) {
        NoSleepStart += m_ScreenSize.cy;
    }

    int MinRange2 = std::max(std::min(long(0.0050 * ScanLines * RefreshRate + 0.5), ScanLines / 3l), 5l); // 5 ms or max 33 % of Time
    int D3DDevLockStart = _RasterStart - MinRange2;
    int D3DDevLockRange = MinRange2;
    if (D3DDevLockStart < 0) {
        D3DDevLockStart += m_ScreenSize.cy;
    }

    int ScanLineDiff = ScanLine - _RasterStart;
    if (ScanLineDiff > m_ScreenSize.cy / 2) {
        ScanLineDiff -= m_ScreenSize.cy;
    } else if (ScanLineDiff < -m_ScreenSize.cy / 2) {
        ScanLineDiff += m_ScreenSize.cy;
    }
    int LastLineDiff = ScanLineDiff;


    int ScanLineDiffSleep = long(ScanLine) - NoSleepStart;
    if (ScanLineDiffSleep > m_ScreenSize.cy / 2) {
        ScanLineDiffSleep -= m_ScreenSize.cy;
    } else if (ScanLineDiffSleep < -m_ScreenSize.cy / 2) {
        ScanLineDiffSleep += m_ScreenSize.cy;
    }
    int LastLineDiffSleep = ScanLineDiffSleep;


    int ScanLineDiffLock = long(ScanLine) - D3DDevLockStart;
    if (ScanLineDiffLock > m_ScreenSize.cy / 2) {
        ScanLineDiffLock -= m_ScreenSize.cy;
    } else if (ScanLineDiffLock < -m_ScreenSize.cy / 2) {
        ScanLineDiffLock += m_ScreenSize.cy;
    }
    int LastLineDiffLock = ScanLineDiffLock;

    LONGLONG llPerfLock = 0;

    for (;;) {
        if (!GetVBlank(ScanLine, InVBlank, _bMeasure)) {
            break;
        }
        ScanLineDiff = long(ScanLine) - _RasterStart;
        if (ScanLineDiff > m_ScreenSize.cy / 2) {
            ScanLineDiff -= m_ScreenSize.cy;
        } else if (ScanLineDiff < -m_ScreenSize.cy / 2) {
            ScanLineDiff += m_ScreenSize.cy;
        }
        if ((ScanLineDiff >= 0 && ScanLineDiff <= _RasterSize) || (LastLineDiff < 0 && ScanLineDiff > 0)) {
            break;
        }

        LastLineDiff = ScanLineDiff;

        bWaited = true;

        ScanLineDiffLock = long(ScanLine) - D3DDevLockStart;
        if (ScanLineDiffLock > m_ScreenSize.cy / 2) {
            ScanLineDiffLock -= m_ScreenSize.cy;
        } else if (ScanLineDiffLock < -m_ScreenSize.cy / 2) {
            ScanLineDiffLock += m_ScreenSize.cy;
        }

        if (((ScanLineDiffLock >= 0 && ScanLineDiffLock <= D3DDevLockRange) || (LastLineDiffLock < 0 && ScanLineDiffLock > 0))) {
            if (!lockOwner && _bMeasure) {
                llPerfLock = GetRenderersData()->GetPerfCounter();
                lockOwner = LockD3DDevice();
            }
        }
        LastLineDiffLock = ScanLineDiffLock;


        ScanLineDiffSleep = long(ScanLine) - NoSleepStart;
        if (ScanLineDiffSleep > m_ScreenSize.cy / 2) {
            ScanLineDiffSleep -= m_ScreenSize.cy;
        } else if (ScanLineDiffSleep < -m_ScreenSize.cy / 2) {
            ScanLineDiffSleep += m_ScreenSize.cy;
        }

        if (!((ScanLineDiffSleep >= 0 && ScanLineDiffSleep <= NoSleepRange) || (LastLineDiffSleep < 0 && ScanLineDiffSleep > 0)) || !_bNeedAccurate) {
            //TRACE(_T("%d\n"), RasterStatus.ScanLine);
            Sleep(1); // Don't sleep for the last 1.5 ms scan lines, so we get maximum precision
        }
        LastLineDiffSleep = ScanLineDiffSleep;
    }
    _RasterStart = ScanLine;
    if (_bMeasure) {
        m_VBlankEndWait = ScanLine;
        m_VBlankWaitTime = GetRenderersData()->GetPerfCounter() - llPerf;

        if (lockOwner) {
            m_VBlankLockTime = GetRenderersData()->GetPerfCounter() - llPerfLock;
        } else {
            m_VBlankLockTime = 0;
        }

        m_RasterStatusWaitTime = m_RasterStatusWaitTimeMaxCalc;
        m_RasterStatusWaitTimeMin = std::min(m_RasterStatusWaitTimeMin, m_RasterStatusWaitTime);
        m_RasterStatusWaitTimeMax = std::max(m_RasterStatusWaitTimeMax, m_RasterStatusWaitTime);
    }

    return bWaited;
}

int CDX9AllocatorPresenter::GetVBlackPos()
{
    BOOL bCompositionEnabled = m_bCompositionEnabled;

    int WaitRange = std::max(m_ScreenSize.cy / 40l, 5l);
    if (!bCompositionEnabled) {
        if (m_bAlternativeVSync) {
            const CRenderersSettings& r = GetRenderersSettings();
            return r.m_AdvRendSets.iVMR9VSyncOffset;
        } else {
            int MinRange = std::max(std::min(long(0.005 * m_ScreenSize.cy * GetRefreshRate() + 0.5), m_ScreenSize.cy / 3l), 5l); // 5  ms or max 33 % of Time
            int WaitFor = m_ScreenSize.cy - (MinRange + WaitRange);
            return WaitFor;
        }
    } else {
        int WaitFor = m_ScreenSize.cy / 2;
        return WaitFor;
    }
}

bool CDX9AllocatorPresenter::WaitForVBlank(bool& _Waited, HANDLE& lockOwner)
{
    const CRenderersSettings& r = GetRenderersSettings();
    if (!r.m_AdvRendSets.bVMR9VSync) {
        _Waited = true;
        m_VBlankWaitTime = 0;
        m_VBlankLockTime = 0;
        m_VBlankEndWait = 0;
        m_VBlankStartWait = 0;
        return true;
    }
    //_Waited = true;
    //return false;

    BOOL bCompositionEnabled = m_bCompositionEnabled;
    int WaitFor = GetVBlackPos();

    if (!bCompositionEnabled) {
        if (m_bAlternativeVSync) {
            _Waited = WaitForVBlankRange(WaitFor, 0, false, true, true, lockOwner);
            return false;
        } else {
            _Waited = WaitForVBlankRange(WaitFor, 0, false, r.m_AdvRendSets.bVMR9VSyncAccurate, true, lockOwner);
            return true;
        }
    } else {
        // Instead we wait for VBlack after the present, this seems to fix the stuttering problem. It'r also possible to fix by removing the Sleep above, but that isn't an option.
        WaitForVBlankRange(WaitFor, 0, false, r.m_AdvRendSets.bVMR9VSyncAccurate, true, lockOwner);

        return false;
    }
}

void CDX9AllocatorPresenter::UpdateAlphaBitmap()
{
    m_VMR9AlphaBitmapData.Free();

    if ((m_VMR9AlphaBitmap.dwFlags & VMRBITMAP_DISABLE) == 0) {
        HBITMAP hBitmap = (HBITMAP)GetCurrentObject(m_VMR9AlphaBitmap.hdc, OBJ_BITMAP);
        if (!hBitmap) {
            return;
        }
        DIBSECTION info;
        ZeroMemory(&info, sizeof(DIBSECTION));
        if (!::GetObject(hBitmap, sizeof(DIBSECTION), &info)) {
            return;
        }

        m_VMR9AlphaBitmapRect = CRect(0, 0, info.dsBm.bmWidth, info.dsBm.bmHeight);
        m_VMR9AlphaBitmapWidthBytes = info.dsBm.bmWidthBytes;

        if (m_VMR9AlphaBitmapData.Allocate(info.dsBm.bmWidthBytes * info.dsBm.bmHeight)) {
            memcpy((BYTE*)m_VMR9AlphaBitmapData, info.dsBm.bmBits, info.dsBm.bmWidthBytes * info.dsBm.bmHeight);
        }
    }
}

STDMETHODIMP_(bool) CDX9AllocatorPresenter::Paint(bool bAll)
{
    if (m_bPendingResetDevice) {
        SendResetRequest();
        return false;
    }

    CRenderersSettings& r = GetRenderersSettings();
    if (&r == nullptr) {
        return false;
    }

    //TRACE(_T("Thread: %d\n"), (LONG)((CRITICAL_SECTION &)m_RenderLock).OwningThread);

#if 0
    if (TryEnterCriticalSection(&(CRITICAL_SECTION&)(*((CCritSec*)this)))) {
        LeaveCriticalSection((&(CRITICAL_SECTION&)(*((CCritSec*)this))));
    } else {
        __debugbreak();
    }
#endif

    CRenderersData* rd = GetRenderersData();

    LONGLONG StartPaint = rd->GetPerfCounter();
    CAutoLock cRenderLock(&m_RenderLock);

    if (m_windowRect.right <= m_windowRect.left || m_windowRect.bottom <= m_windowRect.top
            || m_nativeVideoSize.cx <= 0 || m_nativeVideoSize.cy <= 0
            || !m_pVideoSurface[m_nCurSurface]) {
        if (m_OrderedPaint) {
            --m_OrderedPaint;
        } else {
            //TRACE(_T("UNORDERED PAINT!!!!!!\n"));
        }


        return false;
    }

    HRESULT hr;

    m_pD3DDev->BeginScene();

    CComPtr<IDirect3DSurface9> pBackBuffer;
    m_pD3DDev->GetBackBuffer(0, 0, D3DBACKBUFFER_TYPE_MONO, &pBackBuffer);
    // FIXME: GetBackBuffer can fail, check return value and reset device

    // Clear the backbuffer
    m_pD3DDev->SetRenderTarget(0, pBackBuffer);
    hr = m_pD3DDev->Clear(0, nullptr, D3DCLEAR_TARGET, 0, 1.0f, 0);

    CRect rSrcVid(CPoint(0, 0), GetVisibleVideoSize());
    CRect rDstVid(m_videoRect);

    CRect rSrcPri(CPoint(0, 0), m_windowRect.Size());
    CRect rDstPri(rSrcPri);

    // Render the current video frame
    hr = RenderVideo(pBackBuffer, rSrcVid, rDstVid);

    if (FAILED(hr)) {
        if (m_RenderingPath == RENDERING_PATH_STRETCHRECT) {
            // Support ffdshow queueing
            // m_pD3DDev->StretchRect may fail if ffdshow is using queue output samples.
            // Here we don't want to show the black buffer.
            if (m_OrderedPaint) {
                --m_OrderedPaint;
            } else {
                //TRACE(_T("UNORDERED PAINT!!!!!!\n"));
            }

            return false;
        }
    }

    if (!m_bIsPreview) {
        // paint the text on the backbuffer
        AlphaBltSubPic(rDstPri, rDstVid);

        // Casimir666 : show OSD
        if (m_VMR9AlphaBitmap.dwFlags & VMRBITMAP_UPDATE) {
            CAutoLock BitMapLock(&m_VMR9AlphaBitmapLock);
            CRect rcSrc(m_VMR9AlphaBitmap.rSrc);
            m_pOSDTexture = nullptr;
            m_pOSDSurface = nullptr;
            if ((m_VMR9AlphaBitmap.dwFlags & VMRBITMAP_DISABLE) == 0 && (BYTE*)m_VMR9AlphaBitmapData) {
                if ((m_pD3DXLoadSurfaceFromMemory != nullptr) &&
                    SUCCEEDED(hr = m_pD3DDev->CreateTexture(rcSrc.Width(), rcSrc.Height(), 1,
                        D3DUSAGE_RENDERTARGET, D3DFMT_A8R8G8B8,
                        D3DPOOL_DEFAULT, &m_pOSDTexture, nullptr))) {
                    if (SUCCEEDED(hr = m_pOSDTexture->GetSurfaceLevel(0, &m_pOSDSurface))) {
                        hr = m_pD3DXLoadSurfaceFromMemory(m_pOSDSurface,
                            nullptr,
                            nullptr,
                            (BYTE*)m_VMR9AlphaBitmapData,
                            D3DFMT_A8R8G8B8,
                            m_VMR9AlphaBitmapWidthBytes,
                            nullptr,
                            &m_VMR9AlphaBitmapRect,
                            D3DX_FILTER_NONE,
                            m_VMR9AlphaBitmap.clrSrcKey);
                    }
                    if (FAILED(hr)) {
                        m_pOSDTexture = nullptr;
                        m_pOSDSurface = nullptr;
                    }
                }
            }
            m_VMR9AlphaBitmap.dwFlags ^= VMRBITMAP_UPDATE;

        }

        if (m_bAlphaBitmapEnable && m_pAlphaBitmapTexture) {
            CRect rcDst(rSrcPri);
            AlphaBlt(rSrcPri, rcDst, m_pAlphaBitmapTexture);
        }

        if (rd->m_bResetStats) {
            ResetStats();
            rd->m_bResetStats = false;
        }

        if (rd->m_iDisplayStats) {
            DrawStats();
        }

        if (m_pOSDTexture) {
            AlphaBlt(rSrcPri, rDstPri, m_pOSDTexture);
        }
    }

    m_pD3DDev->EndScene();

    BOOL bCompositionEnabled = m_bCompositionEnabled;

    bool bDoVSyncInPresent = (!bCompositionEnabled && !m_bAlternativeVSync) || !r.m_AdvRendSets.bVMR9VSync;

    CComPtr<IDirect3DQuery9> pEventQueryFlushBeforeVSync;
    m_pD3DDev->CreateQuery(D3DQUERYTYPE_EVENT, &pEventQueryFlushBeforeVSync);
    if (pEventQueryFlushBeforeVSync) {
        pEventQueryFlushBeforeVSync->Issue(D3DISSUE_END);
    }

    if (r.m_AdvRendSets.bVMRFlushGPUBeforeVSync && pEventQueryFlushBeforeVSync) {
        LONGLONG llPerf = rd->GetPerfCounter();
        BOOL Data;
        //Sleep(5);
        LONGLONG FlushStartTime = rd->GetPerfCounter();
        while (S_FALSE == pEventQueryFlushBeforeVSync->GetData(&Data, sizeof(Data), D3DGETDATA_FLUSH)) {
            if (!r.m_AdvRendSets.bVMRFlushGPUWait) {
                break;
            }
            Sleep(1);
            if (rd->GetPerfCounter() - FlushStartTime > 500000) {
                break;    // timeout after 50 ms
            }
        }
        if (r.m_AdvRendSets.bVMRFlushGPUWait) {
            m_WaitForGPUTime = rd->GetPerfCounter() - llPerf;
        } else {
            m_WaitForGPUTime = 0;
        }
    } else {
        m_WaitForGPUTime = 0;
    }

    if (bAll) {
        m_PaintTime = (rd->GetPerfCounter() - StartPaint);
        m_PaintTimeMin = std::min(m_PaintTimeMin, m_PaintTime);
        m_PaintTimeMax = std::max(m_PaintTimeMax, m_PaintTime);
    }

    bool bWaited = false;
    HANDLE lockOwner = nullptr;
    if (bAll) {
        // Only sync to refresh when redrawing all
        bool bTest = WaitForVBlank(bWaited, lockOwner);
        ASSERT(bTest == bDoVSyncInPresent);
        if (!bDoVSyncInPresent) {
            LONGLONG Time = rd->GetPerfCounter();
            OnVBlankFinished(bAll, Time);
            if (!m_bIsEVR || m_OrderedPaint) {
                CalculateJitter(Time);
            }
        }
    }

    // Create a device pointer m_pd3dDevice

    // Create a query object
    {
        CComPtr<IDirect3DQuery9> pEventQueryFlushAfterVSync;
        m_pD3DDev->CreateQuery(D3DQUERYTYPE_EVENT, &pEventQueryFlushAfterVSync);

        LONGLONG llPerf = rd->GetPerfCounter();
        CRect presentationSrcRect(rDstPri), presentationDestRect(m_windowRect);
        // PresentEx() / Present() performs the clipping
        // TODO: fix the race and uncomment the assert
        //ASSERT(presentationSrcRect.Size() == presentationDestRect.Size());
        if (m_pD3DDevEx) {
            if (m_bIsFullscreen) {
                hr = m_pD3DDevEx->PresentEx(nullptr, nullptr, nullptr, nullptr, 0);
            } else {
                hr = m_pD3DDevEx->PresentEx(presentationSrcRect, presentationDestRect, nullptr, nullptr, 0);
            }
        } else {
            if (m_bIsFullscreen) {
                hr = m_pD3DDev->Present(nullptr, nullptr, nullptr, nullptr);
            } else {
                hr = m_pD3DDev->Present(presentationSrcRect, presentationDestRect, nullptr, nullptr);
            }
        }
        // Issue an End event
        if (pEventQueryFlushAfterVSync) {
            pEventQueryFlushAfterVSync->Issue(D3DISSUE_END);
        }

        BOOL Data;

        if (r.m_AdvRendSets.bVMRFlushGPUAfterPresent && pEventQueryFlushAfterVSync) {
            LONGLONG FlushStartTime = rd->GetPerfCounter();
            while (S_FALSE == pEventQueryFlushAfterVSync->GetData(&Data, sizeof(Data), D3DGETDATA_FLUSH)) {
                if (!r.m_AdvRendSets.bVMRFlushGPUWait) {
                    break;
                }
                if (rd->GetPerfCounter() - FlushStartTime > 500000) {
                    break;    // timeout after 50 ms
                }
            }
        }

        int ScanLine;
        int bInVBlank;
        GetVBlank(ScanLine, bInVBlank, false);

        if (bAll && (!m_bIsEVR || m_OrderedPaint)) {
            m_VBlankEndPresent = ScanLine;
        }

        while (ScanLine == 0 || bInVBlank) {
            GetVBlank(ScanLine, bInVBlank, false);

        }
        m_VBlankStartMeasureTime = rd->GetPerfCounter();
        m_VBlankStartMeasure = ScanLine;

        if (bAll && bDoVSyncInPresent) {
            m_PresentWaitTime = rd->GetPerfCounter() - llPerf;
            m_PresentWaitTimeMin = std::min(m_PresentWaitTimeMin, m_PresentWaitTime);
            m_PresentWaitTimeMax = std::max(m_PresentWaitTimeMax, m_PresentWaitTime);
        } else {
            m_PresentWaitTime = 0;
            m_PresentWaitTimeMin = std::min(m_PresentWaitTimeMin, m_PresentWaitTime);
            m_PresentWaitTimeMax = std::max(m_PresentWaitTimeMax, m_PresentWaitTime);
        }
    }

    if (bDoVSyncInPresent) {
        LONGLONG Time = rd->GetPerfCounter();
        if (!m_bIsEVR || m_OrderedPaint) {
            CalculateJitter(Time);
        }
        OnVBlankFinished(bAll, Time);
    }

    if (lockOwner) {
        UnlockD3DDevice(lockOwner);
    }

    /*if (!bWaited)
    {
        bWaited = true;
        WaitForVBlank(bWaited);
        TRACE(_T("Double VBlank\n"));
        ASSERT(bWaited);
        if (!bDoVSyncInPresent)
        {
            CalculateJitter();
            OnVBlankFinished(bAll);
        }
    }*/

    if (!m_bPendingResetDevice) {
        bool fResetDevice = false;

        if (hr == D3DERR_DEVICELOST && m_pD3DDev->TestCooperativeLevel() == D3DERR_DEVICENOTRESET) {
            TRACE(_T("Reset Device: D3D Device Lost\n"));
            fResetDevice = true;
        }

        if (hr == S_PRESENT_MODE_CHANGED) {
            TRACE(_T("Reset Device: D3D Device mode changed\n"));
            fResetDevice = true;
        }

        if (SettingsNeedResetDevice(r)) {
            TRACE(_T("Reset Device: settings changed\n"));
            fResetDevice = true;
        }

        bCompositionEnabled = false;
        if (m_pDwmIsCompositionEnabled) {
            m_pDwmIsCompositionEnabled(&bCompositionEnabled);
        }
        if ((bCompositionEnabled != 0) != m_bCompositionEnabled) {
            if (m_bIsFullscreen) {
                m_bCompositionEnabled = (bCompositionEnabled != 0);
            } else {
                TRACE(_T("Reset Device: DWM composition changed\n"));
                fResetDevice = true;
            }
        }

        if (r.fResetDevice) {
            LONGLONG time = rd->GetPerfCounter();
            if (time > m_LastAdapterCheck + 20000000) { // check every 2 sec.
                m_LastAdapterCheck = time;
#ifdef _DEBUG
                D3DDEVICE_CREATION_PARAMETERS Parameters;
                if (SUCCEEDED(m_pD3DDev->GetCreationParameters(&Parameters))) {
                    ASSERT(Parameters.AdapterOrdinal == m_CurrentAdapter);
                }
#endif
                if (m_CurrentAdapter != GetAdapter(m_pD3D)) {
                    TRACE(_T("Reset Device: D3D adapter changed\n"));
                    fResetDevice = true;
                }
#ifdef _DEBUG
                else {
                    ASSERT(m_pD3D->GetAdapterMonitor(m_CurrentAdapter) == m_pD3D->GetAdapterMonitor(GetAdapter(m_pD3D)));
                }
#endif
            }
        }

        if (fResetDevice) {
            m_bPendingResetDevice = true;
            SendResetRequest();
        }
    }

    if (m_OrderedPaint) {
        --m_OrderedPaint;
    } else {
        //if (m_bIsEVR)
        //  TRACE(_T("UNORDERED PAINT!!!!!!\n"));
    }
    return true;
}

double CDX9AllocatorPresenter::GetFrameTime() const
{
    if (m_DetectedLock) {
        return m_DetectedFrameTime;
    }

    return m_rtTimePerFrame / 10000000.0;
}

double CDX9AllocatorPresenter::GetFrameRate() const
{
    if (m_DetectedLock) {
        return m_DetectedFrameRate;
    }

    return 10000000.0 / m_rtTimePerFrame;
}

void CDX9AllocatorPresenter::SendResetRequest()
{
    if (!m_bDeviceResetRequested) {
        m_bDeviceResetRequested = true;
        AfxGetApp()->m_pMainWnd->PostMessage(WM_RESET_DEVICE);
    }
}

STDMETHODIMP_(bool) CDX9AllocatorPresenter::ResetDevice()
{
    TRACE(_T("ResetDevice\n"));
    ASSERT(m_MainThreadId == GetCurrentThreadId());

    // In VMR-9 deleting the surfaces before we are told to is bad !
    // Can't comment out this because CDX9AllocatorPresenter is used by EVR Custom
    // Why is EVR using a presenter for DX9 anyway ?!
    DeleteSurfaces();

    HRESULT hr;
    CString Error;
    // TODO: Report error messages here

    // In VMR-9 'AllocSurfaces' call is redundant afaik because
    // 'CreateDevice' calls 'm_pIVMRSurfAllocNotify->ChangeD3DDevice' which in turn calls
    // 'CVMR9AllocatorPresenter::InitializeDevice' which calls 'AllocSurfaces'
    if (FAILED(hr = CreateDevice(Error)) || FAILED(hr = AllocSurfaces())) {
        // TODO: We should probably pause player
#ifdef _DEBUG
        Error += GetWindowsErrorMessage(hr, nullptr);
        TRACE(_T("D3D Reset Error\n%ws\n\n"), Error.GetBuffer());
#endif
        m_bDeviceResetRequested = false;
        return false;
    }
    OnResetDevice();
    m_bDeviceResetRequested = false;
    m_bPendingResetDevice = false;

    return true;
}

STDMETHODIMP_(bool) CDX9AllocatorPresenter::DisplayChange()
{
    m_bPendingResetDevice = true;
    SendResetRequest();
    return true;
}

void CDX9AllocatorPresenter::ResetStats()
{
    CRenderersData* rd = GetRenderersData();
    LONGLONG Time = rd->GetPerfCounter();

    m_PaintTime = 0;
    m_PaintTimeMin = 3000000000;
    m_PaintTimeMax = 0;

    m_RasterStatusWaitTime = 0;
    m_RasterStatusWaitTimeMin = 3000000000;
    m_RasterStatusWaitTimeMax = 0;

    m_MinSyncOffset = 0;
    m_MaxSyncOffset = 0;
    m_fSyncOffsetAvr = 0;
    m_fSyncOffsetStdDev = 0;

    CalculateJitter(Time);
}

void CDX9AllocatorPresenter::InitStats()
{
    ASSERT(m_pD3DDev);
    static LONG currentHeight = 0;
    int newHeight = lround(FONT_HEIGHT * (double(m_windowRect.Width()) / REFERENCE_WIDTH));

    if (m_pD3DXCreateFont && (!m_pFont || currentHeight != newHeight)) {
        m_pFont = nullptr;
        if (newHeight <= 0) {
            ASSERT(FALSE);
        }
        m_pD3DXCreateFont(m_pD3DDev, newHeight, 0, newHeight < BOLD_THRESHOLD ? FW_NORMAL : FW_BOLD,
                          0, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLEARTYPE_NATURAL_QUALITY,
                          FIXED_PITCH | FF_DONTCARE, L"Lucida Console", &m_pFont);
        currentHeight = newHeight;
    }

    if (m_pD3DXCreateSprite && !m_pSprite) {
        m_pD3DXCreateSprite(m_pD3DDev, &m_pSprite);
    }

    if (m_pD3DXCreateLine && !m_pLine) {
        m_pD3DXCreateLine(m_pD3DDev, &m_pLine);
    }
}

void CDX9AllocatorPresenter::DrawStats()
{
    const CRenderersData* rd = GetRenderersData();

    int iDetailedStats;
    switch (rd->m_iDisplayStats) {
        case 1:
            iDetailedStats = 2;
            break;
        case 2:
            iDetailedStats = 1;
            break;
        case 3:
            iDetailedStats = 0;
            break;
        default:
            ASSERT(FALSE);
            return;
    }

    InitStats();
    const float textScale = float(m_windowRect.Width()) / REFERENCE_WIDTH;
    const int lineHeight = lround((FONT_HEIGHT + TEXT_PADDING) * textScale);

    if (m_pFont && m_pSprite) {
        auto drawText = [&](CRect & rc, const CString & strText) {
            D3DXCOLOR Color1(1.0f, 0.2f, 0.2f, 1.0f);
            D3DXCOLOR Color0(0.0f, 0.0f, 0.0f, 1.0f);

            RECT shadowRect = rc;
            OffsetRect(&shadowRect, 2, 2);

            // Draw shadow
            m_pFont->DrawText(m_pSprite, strText, -1, &shadowRect, DT_NOCLIP, Color0);
            // Draw text
            m_pFont->DrawText(m_pSprite, strText, -1, rc, DT_NOCLIP, Color1);
            rc.OffsetRect(0, lineHeight);
        };

        const CRenderersSettings& r = GetRenderersSettings();
        LONGLONG llMaxJitter = m_MaxJitter;
        LONGLONG llMinJitter = m_MinJitter;
        LONGLONG llMaxSyncOffset = m_MaxSyncOffset;
        LONGLONG llMinSyncOffset = m_MinSyncOffset;
        CRect rc(lineHeight, lineHeight, 0, 0);

        m_pSprite->Begin(D3DXSPRITE_ALPHABLEND);
        CString strText;
        if (iDetailedStats > 1) {
            if (m_bIsEVR) {
                strText.Format(L"Frame rate   : %7.03f   (%7.3f ms = %.03f, %s)   (%7.3f ms = %.03f%s, %2.03f StdDev)  Clock: %1.4f %%",
                                m_fAvrFps,
                                double(m_rtTimePerFrame) / 10000.0, 10000000.0 / (double)(m_rtTimePerFrame),
                                m_bInterlaced ? L"I" : L"P", GetFrameTime() * 1000.0, GetFrameRate(),
                                m_DetectedLock ? L" L" : L"", m_DetectedFrameTimeStdDev / 10000.0,
                                m_ModeratedTimeSpeed * 100.0);
            } else {
                strText.Format(L"Frame rate   : %7.03f   (%7.3f ms = %.03f, %s)",
                               m_fAvrFps,
                               double(m_rtTimePerFrame) / 10000.0, 10000000.0 / (double)(m_rtTimePerFrame),
                               m_bInterlaced ? L"I" : L"P");
            }
        } else {
            strText.Format(L"Frame rate   : %7.03f   (%.03f%s)", m_fAvrFps, GetFrameRate(), m_DetectedLock ? L" L" : L"");
        }
        drawText(rc, strText);

        if (iDetailedStats > 1) {
            strText = _T("Settings     : ");

            if (m_bIsEVR) {
                strText += _T("EVR ");
            } else {
                strText += _T("VMR9 ");
            }

            if (m_bIsFullscreen) {
                strText += _T("FS ");
            }
            if (r.m_AdvRendSets.bVMR9FullscreenGUISupport) {
                strText += _T("FSGui ");
            }

            if (r.m_AdvRendSets.bVMRDisableDesktopComposition) {
                strText += _T("DisDC ");
            }

            if (m_bColorManagement) {
                strText += _T("ColorMan ");
            }

            if (r.m_AdvRendSets.bVMRFlushGPUBeforeVSync) {
                strText += _T("GPUFlushBV ");
            }
            if (r.m_AdvRendSets.bVMRFlushGPUAfterPresent) {
                strText += _T("GPUFlushAP ");
            }

            if (r.m_AdvRendSets.bVMRFlushGPUWait) {
                strText += _T("GPUFlushWt ");
            }

            if (r.m_AdvRendSets.bVMR9VSync) {
                strText += _T("VS ");
            }
            if (r.m_AdvRendSets.bVMR9AlterativeVSync) {
                strText += _T("AltVS ");
            }
            if (r.m_AdvRendSets.bVMR9VSyncAccurate) {
                strText += _T("AccVS ");
            }
            if (r.m_AdvRendSets.iVMR9VSyncOffset) {
                strText.AppendFormat(_T("VSOfst(%d)"), r.m_AdvRendSets.iVMR9VSyncOffset);
            }

            if (m_bFullFloatingPointProcessing) {
                strText += _T("FullFP ");
            }

            if (m_bHalfFloatingPointProcessing) {
                strText += _T("HalfFP ");
            }

            if (m_bIsEVR) {
                if (m_bHighColorResolution) {
                    strText += _T("10bitOut ");
                }
                if (m_bForceInputHighColorResolution) {
                    strText += _T("For10bitIn ");
                }
                if (r.m_AdvRendSets.bEVREnableFrameTimeCorrection) {
                    strText += _T("FTC ");
                }
                if (r.m_AdvRendSets.iEVROutputRange == 0) {
                    strText += _T("0-255 ");
                } else if (r.m_AdvRendSets.iEVROutputRange == 1) {
                    strText += _T("16-235 ");
                }
            }


            drawText(rc, strText);

            strText.Format(L"Formats      : Surface %s    Backbuffer %s    Display %s     Device %s      %s",
                           GetD3DFormatStr(m_SurfaceType), GetD3DFormatStr(m_BackbufferType),
                           GetD3DFormatStr(m_DisplayType), m_pD3DDevEx ? L"D3DDevEx" : L"D3DDev",
                           m_D3DDevExError.IsEmpty() ? _T("") : (_T("D3DExError: ") + m_D3DDevExError).GetString());
            drawText(rc, strText);

            if (m_bIsEVR) {
                if (r.m_AdvRendSets.bVMR9VSync) {
                    strText.Format(_T("Refresh rate : %.05f Hz    SL: %4d     (%3u Hz)      "),
                                   m_DetectedRefreshRate, int(m_DetectedScanlinesPerFrame + 0.5), m_refreshRate);
                } else {
                    strText.Format(_T("Refresh rate : %3u Hz      "), m_refreshRate);
                }
                strText.AppendFormat(_T("Last Duration: %10.6f      Corrected Frame Time: %s"),
                                     double(m_LastFrameDuration) / 10000.0, m_bCorrectedFrameTime ? _T("Yes") : _T("No"));
                drawText(rc, strText);
            }
        }

        if (m_bSyncStatsAvailable) {
            if (iDetailedStats > 1) {
                strText.Format(_T("Sync offset  : Min = %+8.3f ms, Max = %+8.3f ms, StdDev = %7.3f ms, Avr = %7.3f ms, Mode = %d"),
                               (double(llMinSyncOffset) / 10000.0), (double(llMaxSyncOffset) / 10000.0),
                               m_fSyncOffsetStdDev / 10000.0, m_fSyncOffsetAvr / 10000.0, m_VSyncMode);
            } else {
                strText.Format(_T("Sync offset  : Mode = %d"), m_VSyncMode);
            }
            drawText(rc, strText);
        }

        if (iDetailedStats > 1) {
            strText.Format(_T("Jitter       : Min = %+8.3f ms, Max = %+8.3f ms, StdDev = %7.3f ms"),
                           (double(llMinJitter) / 10000.0),
                           (double(llMaxJitter) / 10000.0),
                           m_fJitterStdDev / 10000.0);
            drawText(rc, strText);
        }

        if (m_pAllocator && iDetailedStats > 1) {
            CDX9SubPicAllocator* pAlloc = (CDX9SubPicAllocator*)m_pAllocator.p;
            int nFree = 0;
            int nAlloc = 0;
            int nSubPic = 0;
            REFERENCE_TIME rtQueueStart = 0;
            REFERENCE_TIME rtQueueEnd = 0;

            if (m_pSubPicQueue) {
                REFERENCE_TIME rtQueueNow = 0;
                m_pSubPicQueue->GetStats(nSubPic, rtQueueNow, rtQueueStart, rtQueueEnd);
            }

            pAlloc->GetStats(nFree, nAlloc);
            strText.Format(_T("Subtitles    : Free %d     Allocated %d     Buffered %d     QueueStart %7.3f     QueueEnd %7.3f"),
                           nFree, nAlloc, nSubPic, (double(rtQueueStart) / 10000000.0),
                           (double(rtQueueEnd) / 10000000.0));
            drawText(rc, strText);
        }

        if (iDetailedStats > 1) {
            if (m_VBlankEndPresent == -100000) {
                strText.Format(_T("VBlank Wait  : Start %4d   End %4d   Wait %7.3f ms   Lock %7.3f ms   Offset %4d   Max %4d"),
                               m_VBlankStartWait, m_VBlankEndWait,
                               (double(m_VBlankWaitTime) / 10000.0),
                               (double(m_VBlankLockTime) / 10000.0),
                               m_VBlankMin, m_VBlankMax - m_VBlankMin);
            } else {
                strText.Format(_T("VBlank Wait  : Start %4d   End %4d   Wait %7.3f ms   Lock %7.3f ms   Offset %4d   Max %4d   EndPresent %4d"),
                               m_VBlankStartWait, m_VBlankEndWait,
                               (double(m_VBlankWaitTime) / 10000.0),
                               (double(m_VBlankLockTime) / 10000.0),
                               m_VBlankMin,
                               m_VBlankMax - m_VBlankMin,
                               m_VBlankEndPresent);
            }
        } else {
            if (m_VBlankEndPresent == -100000) {
                strText.Format(_T("VBlank Wait  : Start %4d   End %4d"), m_VBlankStartWait, m_VBlankEndWait);
            } else {
                strText.Format(_T("VBlank Wait  : Start %4d   End %4d   EP %4d"), m_VBlankStartWait, m_VBlankEndWait,
                               m_VBlankEndPresent);
            }
        }
        drawText(rc, strText);

        BOOL bCompositionEnabled = m_bCompositionEnabled;

        bool bDoVSyncInPresent = (!bCompositionEnabled && !m_bAlternativeVSync) || !r.m_AdvRendSets.bVMR9VSync;

        if (iDetailedStats > 1 && bDoVSyncInPresent) {
            strText.Format(_T("Present Wait : Wait %7.3f ms   Min %7.3f ms   Max %7.3f ms"),
                           (double(m_PresentWaitTime) / 10000.0),
                           (double(m_PresentWaitTimeMin) / 10000.0),
                           (double(m_PresentWaitTimeMax) / 10000.0));
            drawText(rc, strText);
        }

        if (iDetailedStats > 1) {
            if (m_WaitForGPUTime) {
                strText.Format(_T("Paint Time   : Draw %7.3f ms   Min %7.3f ms   Max %7.3f ms   GPU %7.3f ms"),
                               (double(m_PaintTime - m_WaitForGPUTime) / 10000.0),
                               (double(m_PaintTimeMin) / 10000.0),
                               (double(m_PaintTimeMax) / 10000.0),
                               (double(m_WaitForGPUTime) / 10000.0));
            } else {
                strText.Format(_T("Paint Time   : Draw %7.3f ms   Min %7.3f ms   Max %7.3f ms"),
                               (double(m_PaintTime - m_WaitForGPUTime) / 10000.0),
                               (double(m_PaintTimeMin) / 10000.0),
                               (double(m_PaintTimeMax) / 10000.0));
            }
        } else {
            if (m_WaitForGPUTime) {
                strText.Format(_T("Paint Time   : Draw %7.3f ms   GPU %7.3f ms"),
                               (double(m_PaintTime - m_WaitForGPUTime) / 10000.0),
                               (double(m_WaitForGPUTime) / 10000.0));
            } else {
                strText.Format(_T("Paint Time   : Draw %7.3f ms"),
                               (double(m_PaintTime - m_WaitForGPUTime) / 10000.0));
            }
        }
        drawText(rc, strText);

        if (iDetailedStats > 1 && r.m_AdvRendSets.bVMR9VSync) {
            strText.Format(_T("Raster Status: Wait %7.3f ms   Min %7.3f ms   Max %7.3f ms"),
                           (double(m_RasterStatusWaitTime) / 10000.0),
                           (double(m_RasterStatusWaitTimeMin) / 10000.0),
                           (double(m_RasterStatusWaitTimeMax) / 10000.0));
            drawText(rc, strText);
        }

        if (iDetailedStats > 1) {
            if (m_bIsEVR) {
                strText.Format(_T("Buffering    : Buffered %3ld    Free %3ld    Current Surface %3d"),
                               m_nUsedBuffer, m_nNbDXSurface - m_nUsedBuffer, m_nCurSurface);
            } else {
                strText.Format(_T("Buffering    : VMR9Surfaces %3d   VMR9Surface %3d"),
                               m_nVMR9Surfaces, m_iVMR9Surface);
            }
        } else {
            strText.Format(_T("Buffered     : %3ld"), m_nUsedBuffer);
        }
        drawText(rc, strText);


        if (iDetailedStats > 1) {
            strText.Format(_T("Video size   : %ld x %ld  (AR = %ld : %ld)"),
                           m_nativeVideoSize.cx, m_nativeVideoSize.cy, m_aspectRatio.cx, m_aspectRatio.cy);
            drawText(rc, strText);
            if (m_pVideoTexture[0] || m_pVideoSurface[0]) {
                D3DSURFACE_DESC desc;
                ZeroMemory(&desc, sizeof(desc));
                if (m_pVideoTexture[0]) {
                    m_pVideoTexture[0]->GetLevelDesc(0, &desc);
                } else if (m_pVideoSurface[0]) {
                    m_pVideoSurface[0]->GetDesc(&desc);
                }

                if (desc.Width != (UINT)m_nativeVideoSize.cx || desc.Height != (UINT)m_nativeVideoSize.cy) {
                    strText.Format(_T("Texture size : %u x %u"), desc.Width, desc.Height);
                    drawText(rc, strText);
                }
            }


            drawText(rc, rd->m_strDXVAInfo);

            strText.Format(_T("DirectX SDK  : %u"), rd->GetDXSdkRelease());
            drawText(rc, strText);

            if (!m_D3D9Device.IsEmpty()) {
                strText = _T("Render device: ") + m_D3D9Device;
                drawText(rc, strText);
            }

            if (!m_Decoder.IsEmpty()) {
                strText = _T("Decoder      : ") + m_Decoder;
                drawText(rc, strText);
            }

            for (int i = 0; i < 6; i++) {
                if (m_strStatsMsg[i][0]) {
                    drawText(rc, m_strStatsMsg[i]);
                }
            }
        }
        m_pSprite->End();
    }

    if (m_pLine && iDetailedStats) {
        D3DXVECTOR2 points[NB_JITTER];
        const float graphWidth = GRAPH_WIDTH * textScale;
        const float graphHeight = GRAPH_HEIGHT * textScale;
        const float topLeftX = m_windowRect.Width() - (graphWidth + lineHeight);
        const float topLeftY = m_windowRect.Height() - (graphHeight + lineHeight);
        const float gridStepY = graphHeight / 24.0f;
        const float gridStepX = graphWidth / NB_JITTER;

        // Draw background
        DrawRect(RGB(0, 0, 0), 80, CRect(int(topLeftX),
                                         int(topLeftY),
                                         int(topLeftX + graphWidth),
                                         int(topLeftY + graphHeight)));

        m_pLine->SetWidth(2.5f * textScale);
        m_pLine->SetAntialias(TRUE);
        m_pLine->Begin();

        // Draw grid lines
        for (int i = 1; i < 24; ++i) {
            points[0].x = topLeftX;
            points[0].y = topLeftY + i * gridStepY;
            points[1].y = points[0].y;

            float lineLength;
            D3DCOLOR color;
            if (i % 12 == 0) {
                lineLength = 1.0f;
                color = D3DCOLOR_XRGB(100, 100, 255);
            } else if (i % 4 == 0) {
                lineLength = 0.96f;
                color = D3DCOLOR_XRGB(100, 100, 180);
            } else {
                lineLength = 0.04f;
                color = D3DCOLOR_XRGB(100, 100, 140);
            }
            points[1].x = topLeftX + graphWidth * lineLength;
            m_pLine->Draw(points, 2, color);
        }

        if (m_rtTimePerFrame) {
            // Draw jitter
            for (int i = 1; i <= NB_JITTER; ++i) {
                int nIndex = (m_nNextJitter + i) % NB_JITTER;
                if (nIndex < 0) {
                    nIndex += NB_JITTER;
                    ASSERT(FALSE);
                }
                float jitter = float(m_pllJitter[nIndex] - m_fJitterMean);
                points[i - 1].x = topLeftX + i * gridStepX;
                points[i - 1].y = topLeftY + (jitter * textScale / 2000.0f + graphHeight / 2.0f);
            }
            m_pLine->Draw(points, NB_JITTER, D3DCOLOR_XRGB(255, 100, 100));

            if (m_bSyncStatsAvailable) {
                // Draw sync offset
                for (int i = 1; i <= NB_JITTER; ++i) {
                    int nIndex = (m_nNextSyncOffset + i) % NB_JITTER;
                    if (nIndex < 0) {
                        nIndex += NB_JITTER;
                    }
                    points[i - 1].x = topLeftX + i * gridStepX;
                    points[i - 1].y = topLeftY + (m_pllSyncOffset[nIndex] * textScale / 2000.0f + graphHeight / 2.0f);
                }
                m_pLine->Draw(points, NB_JITTER, D3DCOLOR_XRGB(100, 200, 100));
            }
        }
        m_pLine->End();
    }
}

STDMETHODIMP CDX9AllocatorPresenter::GetDIB(BYTE* lpDib, DWORD* size)
{
    CheckPointer(size, E_POINTER);

    // Keep a reference so that we can safely work on the surface
    // without having to lock everything
    CComPtr<IDirect3DSurface9> pVideoSurface;
    {
        CAutoLock cAutoLock(this);
        CheckPointer(m_pVideoSurface[m_nCurSurface], E_FAIL);
        pVideoSurface = m_pVideoSurface[m_nCurSurface];
    }

    HRESULT hr;

    D3DSURFACE_DESC desc;
    ZeroMemory(&desc, sizeof(desc));
    if (FAILED(hr = pVideoSurface->GetDesc(&desc))) {
        return hr;
    }

    CSize framesize = GetVideoSize(false);
    const CSize dar = GetVideoSize(true);

    bool resize = false;
    if (dar.cx > 0 && dar.cy > 0 && (dar.cx != desc.Width || dar.cy != desc.Height)) {
        framesize.cx = MulDiv(framesize.cy, dar.cx, dar.cy);
        resize = true;
        desc.Width = framesize.cx;
        desc.Height = framesize.cy;
    }

    DWORD required = sizeof(BITMAPINFOHEADER) + (desc.Width * desc.Height * 32 >> 3);
    if (!lpDib) {
        *size = required;
        return S_OK;
    }
    if (*size < required) {
        return E_OUTOFMEMORY;
    }
    *size = required;

    CComPtr<IDirect3DSurface9> pSurface, tSurface;
    // Convert to 8-bit when using 10-bit or full/half processing modes
    if (desc.Format != D3DFMT_X8R8G8B8) {
        if (FAILED(hr = m_pD3DDev->CreateOffscreenPlainSurface(desc.Width, desc.Height, D3DFMT_X8R8G8B8, D3DPOOL_DEFAULT, &tSurface, nullptr))
                || FAILED(hr = m_pD3DXLoadSurfaceFromSurface(tSurface, nullptr, nullptr, pVideoSurface, nullptr, nullptr, D3DX_DEFAULT, 0))) {
            return hr;
        }
    } else {
        tSurface = pVideoSurface;
    }

    if (resize) {
        CComPtr<IDirect3DTexture9> pVideoTexture = m_pVideoTexture[m_nCurSurface];
        if (FAILED(hr = m_pD3DDevEx->CreateRenderTarget(framesize.cx, framesize.cy, D3DFMT_X8R8G8B8, D3DMULTISAMPLE_NONE, 0, TRUE, &pSurface, nullptr))
            || FAILED(hr = m_pD3DDevEx->SetRenderTarget(0, pSurface))
            || FAILED(hr = Resize(pVideoTexture, { CPoint(0, 0), m_nativeVideoSize }, { CPoint(0, 0), framesize }))) {
            return hr;
        }
    } else {
        pSurface = tSurface;
    }

    D3DLOCKED_RECT r;
    if (FAILED(hr = pSurface->LockRect(&r, nullptr, D3DLOCK_READONLY))) {
        // If this fails, we try to use a surface allocated from the system memory
        CComPtr<IDirect3DSurface9> pInputSurface = pSurface;
        pSurface = nullptr;
        if (FAILED(hr = m_pD3DDev->CreateOffscreenPlainSurface(desc.Width, desc.Height, D3DFMT_X8R8G8B8, D3DPOOL_SYSTEMMEM, &pSurface, nullptr))
                || FAILED(hr = m_pD3DDev->GetRenderTargetData(pInputSurface, pSurface))
                || FAILED(hr = pSurface->LockRect(&r, nullptr, D3DLOCK_READONLY))) {
            return hr;
        }
    }

    hr = CreateDIBFromSurfaceData(desc, r, lpDib);

    pSurface->UnlockRect();

    return hr;
}

STDMETHODIMP CDX9AllocatorPresenter::SetPixelShader(LPCSTR pSrcData, LPCSTR pTarget)
{
    return SetPixelShader2(pSrcData, pTarget, false);
}

STDMETHODIMP CDX9AllocatorPresenter::SetPixelShader2(LPCSTR pSrcData, LPCSTR pTarget, bool bScreenSpace)
{
    CAutoLock cRenderLock(&m_RenderLock);

    return SetCustomPixelShader(pSrcData, pTarget, bScreenSpace);
}

STDMETHODIMP CDX9AllocatorPresenter::SetD3DFullscreen(bool fEnabled)
{
    CAutoLock cAutoLock(this);
    CAutoLock cRenderLock(&m_RenderLock);

    fullScreenChanged = (fEnabled != m_bIsFullscreen);
    m_bIsFullscreen = fEnabled;
    return S_OK;
}

STDMETHODIMP CDX9AllocatorPresenter::GetD3DFullscreen(bool* pfEnabled)
{
    CheckPointer(pfEnabled, E_POINTER);
    *pfEnabled = m_bIsFullscreen;
    return S_OK;
}
