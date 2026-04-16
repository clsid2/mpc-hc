/*
 * (C) 2010-2017 see Authors.txt
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
#include "../SyncClock/Interfaces.h"
#include <algorithm>
#include <atlbase.h>
#include <atlcoll.h>
#include "../../../mpc-hc/resource.h"
#include "../../../DSUtil/DSUtil.h"
#include "../../../DSUtil/WinAPIUtils.h"
#include <strsafe.h> // Required in CGenlock
#include <videoacc.h>
#include <d3d9.h>
#include "d3dx9/d3dx9.h"
#include <vmr9.h>
#include <evr.h>
#include <Mferror.h>
#include <vector>
#include "../../../SubPic/DX9SubPic.h"
#include "../../../SubPic/SubPicQueueImpl.h"
#include "moreuuids.h"
#include "MacrovisionKicker.h"
#include "IPinHook.h"
#include "PixelShaderCompiler.h"
#include "FocusThread.h"
#include "../../../DSUtil/vd.h"
#include <mpc-hc_config.h>

#include <initguid.h>
#include <mfapi.h>
#include "SyncRenderer.h"
#include "Utils.h"
#include "Variables.h"

#if (0)     // Set to 1 to activate SyncRenderer traces
#define TRACE_SR   TRACE
#else
#define TRACE_SR   __noop
#endif


#define REFERENCE_WIDTH 1920
#define FONT_HEIGHT     21
#define BOLD_THRESHOLD  11
#define TEXT_PADDING    2
#define GRAPH_HEIGHT    360
#define GRAPH_WIDTH     1000

using namespace GothSync;

extern bool LoadResource(UINT resid, CStringA& str, LPCTSTR restype);

CBaseAP::CBaseAP(HWND hWnd, bool bFullscreen, HRESULT& hr, CString& _Error)
    : CSubPicAllocatorPresenterImpl(hWnd, hr, &_Error)
    , m_hDWMAPI(nullptr)
    , m_pDwmIsCompositionEnabled(nullptr)
    , m_pDwmEnableComposition(nullptr)
    , m_pOuterEVR(nullptr)
    , m_SurfaceType(D3DFMT_UNKNOWN)
    , m_BackbufferType(D3DFMT_UNKNOWN)
    , m_DisplayType(D3DFMT_UNKNOWN)
    , m_filter(D3DTEXF_NONE)
    , m_LastAdapterCheck(0)
    , m_CurrentAdapter(UINT_ERROR)
    , m_bicubicA(0)
    , m_nTearingPos(0)
    , m_VMR9AlphaBitmapWidthBytes()
    , m_pD3DXLoadSurfaceFromMemory(nullptr)
    , m_pD3DXLoadSurfaceFromSurface(nullptr)
    , m_pD3DXCreateLine(nullptr)
    , m_pD3DXCreateFont(nullptr)
    , m_pD3DXCreateSprite(nullptr)
    , m_nDXSurface(1)
    , m_nVMR9Surfaces(0)
    , m_iVMR9Surface(0)
    , m_nCurSurface(0)
    , m_nUsedBuffer(0)
    , m_lNextSampleWait(1)
    , m_bSnapToVSync(false)
    , m_uScanLineEnteringPaint(0)
    , m_llEstVBlankTime(0)
    , m_fAvrFps(0.0)
    , m_fJitterStdDev(0.0)
    , m_fJitterMean(0)
    , m_fSyncOffsetAvr(0.0)
    , m_fSyncOffsetStdDev(0.0)
    , m_bHighColorResolution(false)
    , m_bCompositionEnabled(false)
    , m_bDesktopCompositionDisabled(false)
    , m_bIsFullscreen(bFullscreen)
    , fullScreenChanged(false)
    , m_bNeedCheckSample(true)
    , m_dMainThreadId(0)
    , m_ScreenSize(0, 0)
    , m_dDetectedScanlineTime(0.0)
    , m_dD3DRefreshCycle(0)
    , m_dEstRefreshCycle(0.0)
    , m_dFrameCycle(0.0)
    , m_dOptimumDisplayCycle(0.0)
    , m_dCycleDifference(1.0)
    , m_pcFramesDropped(0)
    , m_pcFramesDuplicated(0)
    , m_pcFramesDrawn(0)
    , m_nNextJitter(0)
    , m_nNextSyncOffset(0)
    , m_JitterStdDev(0)
    , m_llLastSyncTime(LONGLONG_ERROR)
    , m_MaxJitter(MINLONG64)
    , m_MinJitter(MAXLONG64)
    , m_MaxSyncOffset(MINLONG64)
    , m_MinSyncOffset(MAXLONG64)
    , m_uSyncGlitches(0)
    , m_llSampleTime(0)
    , m_llLastSampleTime(0)
    , m_llHysteresis(0)
    , m_lShiftToNearest(0)
    , m_lShiftToNearestPrev(0)
    , m_bVideoSlowerThanDisplay(0)
    , m_rtTimePerFrame(0)
    , m_bInterlaced(false)
    , m_TextScale(1.0)
    , m_pGenlock(nullptr)
    , m_pAudioStats(nullptr)
    , m_lAudioLag(0)
    , m_lAudioLagMin(10000)
    , m_lAudioLagMax(-10000)
    , m_lAudioSlaveMode(0)
    , m_FocusThread(nullptr)
    , m_hFocusWindow(nullptr)
{
    ZeroMemory(&m_VMR9AlphaBitmap, sizeof(m_VMR9AlphaBitmap));
    ZeroMemory(&m_caps, sizeof(m_caps));
    ZeroMemory(&m_pp, sizeof(m_pp));

    if (FAILED(hr)) {
        _Error += _T("ISubPicAllocatorPresenterImpl failed\n");
        return;
    }

    HINSTANCE hDll = GetRenderersData()->GetD3X9Dll();

    if (hDll) {
        (FARPROC&)m_pD3DXLoadSurfaceFromMemory = GetProcAddress(hDll, "D3DXLoadSurfaceFromMemory");
        (FARPROC&)m_pD3DXLoadSurfaceFromSurface = GetProcAddress(hDll, "D3DXLoadSurfaceFromSurface");
        (FARPROC&)m_pD3DXCreateLine = GetProcAddress(hDll, "D3DXCreateLine");
        (FARPROC&)m_pD3DXCreateFont = GetProcAddress(hDll, "D3DXCreateFontW");
        (FARPROC&)m_pD3DXCreateSprite = GetProcAddress(hDll, "D3DXCreateSprite");
    } else {
        _Error += _T("The installed DirectX End-User Runtime is outdated. Please download and install the ");
        _Error += MPC_DX_SDK_MONTH _T(" ") MAKE_STR(MPC_DX_SDK_YEAR);
        _Error += _T(" release or newer in order for MPC-HC to function properly.\n");
    }

    m_hDWMAPI = LoadLibrary(L"dwmapi.dll");
    if (m_hDWMAPI) {
        (FARPROC&)m_pDwmIsCompositionEnabled = GetProcAddress(m_hDWMAPI, "DwmIsCompositionEnabled");
        (FARPROC&)m_pDwmEnableComposition = GetProcAddress(m_hDWMAPI, "DwmEnableComposition");
    }

    Direct3DCreate9Ex(D3D_SDK_VERSION, &m_pD3DEx);
    if (m_pD3DEx) {
        m_pD3D = m_pD3DEx;
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

    m_pGenlock = DEBUG_NEW CGenlock(r.m_AdvRendSets.fTargetSyncOffset, r.m_AdvRendSets.fControlLimit, r.m_AdvRendSets.iLineDelta, r.m_AdvRendSets.iColumnDelta, r.m_AdvRendSets.fCycleDelta, 0); // Must be done before CreateDXDevice
    hr = CreateDXDevice(_Error);
    ZeroMemory(m_pllJitter, sizeof(m_pllJitter));
    ZeroMemory(m_pllSyncOffset, sizeof(m_pllSyncOffset));
}

CBaseAP::~CBaseAP()
{
    if (m_bDesktopCompositionDisabled) {
        m_bDesktopCompositionDisabled = false;
        if (m_pDwmEnableComposition) {
            m_pDwmEnableComposition(1);
        }
    }

    m_pFont = nullptr;
    m_pLine = nullptr;
    m_pSprite = nullptr;
    m_pOSDTexture = nullptr;
    m_pOSDSurface = nullptr;
    m_pD3DDev = nullptr;
    m_pD3DDevEx = nullptr;
    m_pPSC.Free();
    m_pD3D = nullptr;
    m_pD3DEx = nullptr;
    m_pSubPicQueue = nullptr;
    m_pAllocator = nullptr;

    m_pAudioStats = nullptr;
    SAFE_DELETE(m_pGenlock);

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

template<int texcoords>
void CBaseAP::AdjustQuad(MYD3DVERTEX<texcoords>* v, double dx, double dy)
{
    float offset = 0.5;

    for (int i = 0; i < 4; i++) {
        v[i].x -= offset;
        v[i].y -= offset;

        for (int j = 0; j < std::max(texcoords - 1, 1); j++) {
            v[i].t[j].u -= (float)(offset * dx);
            v[i].t[j].v -= (float)(offset * dy);
        }

        if constexpr(texcoords > 1) {
            v[i].t[texcoords - 1].u -= offset;
            v[i].t[texcoords - 1].v -= offset;
        }
    }
}

template<int texcoords>
HRESULT CBaseAP::TextureBlt(IDirect3DDevice9* pD3DDev, MYD3DVERTEX<texcoords> v[4], D3DTEXTUREFILTERTYPE filter = D3DTEXF_LINEAR)
{
    CheckPointer(pD3DDev, E_POINTER);

    DWORD FVF = 0;
    switch (texcoords) {
        case 1:
            FVF = D3DFVF_TEX1;
            break;
        case 2:
            FVF = D3DFVF_TEX2;
            break;
        case 3:
            FVF = D3DFVF_TEX3;
            break;
        case 4:
            FVF = D3DFVF_TEX4;
            break;
        case 5:
            FVF = D3DFVF_TEX5;
            break;
        case 6:
            FVF = D3DFVF_TEX6;
            break;
        case 7:
            FVF = D3DFVF_TEX7;
            break;
        case 8:
            FVF = D3DFVF_TEX8;
            break;
        default:
            return E_FAIL;
    }

    HRESULT hr;
    hr = pD3DDev->SetRenderState(D3DRS_CULLMODE, D3DCULL_NONE);
    hr = pD3DDev->SetRenderState(D3DRS_LIGHTING, FALSE);
    hr = pD3DDev->SetRenderState(D3DRS_ZENABLE, FALSE);
    hr = pD3DDev->SetRenderState(D3DRS_STENCILENABLE, FALSE);
    hr = pD3DDev->SetRenderState(D3DRS_ALPHABLENDENABLE, FALSE);
    hr = pD3DDev->SetRenderState(D3DRS_ALPHATESTENABLE, FALSE);
    hr = pD3DDev->SetRenderState(D3DRS_SCISSORTESTENABLE, FALSE);
    hr = pD3DDev->SetRenderState(D3DRS_COLORWRITEENABLE, D3DCOLORWRITEENABLE_ALPHA | D3DCOLORWRITEENABLE_BLUE | D3DCOLORWRITEENABLE_GREEN | D3DCOLORWRITEENABLE_RED);

    for (int i = 0; i < texcoords; i++) {
        hr = pD3DDev->SetSamplerState(i, D3DSAMP_MAGFILTER, filter);
        hr = pD3DDev->SetSamplerState(i, D3DSAMP_MINFILTER, filter);
        hr = pD3DDev->SetSamplerState(i, D3DSAMP_MIPFILTER, D3DTEXF_NONE);

        hr = pD3DDev->SetSamplerState(i, D3DSAMP_ADDRESSU, D3DTADDRESS_CLAMP);
        hr = pD3DDev->SetSamplerState(i, D3DSAMP_ADDRESSV, D3DTADDRESS_CLAMP);
    }

    hr = pD3DDev->SetFVF(D3DFVF_XYZRHW | FVF);

    MYD3DVERTEX<texcoords> tmp = v[2];
    v[2] = v[3];
    v[3] = tmp;
    hr = pD3DDev->DrawPrimitiveUP(D3DPT_TRIANGLEFAN, 2, v, sizeof(v[0]));

    for (int i = 0; i < texcoords; i++) {
        pD3DDev->SetTexture(i, nullptr);
    }

    return S_OK;
}

HRESULT CBaseAP::DrawRectBase(IDirect3DDevice9* pD3DDev, MYD3DVERTEX<0> v[4])
{
    CheckPointer(pD3DDev, E_POINTER);

    HRESULT hr = pD3DDev->SetRenderState(D3DRS_CULLMODE, D3DCULL_NONE);
    hr = pD3DDev->SetRenderState(D3DRS_LIGHTING, FALSE);
    hr = pD3DDev->SetRenderState(D3DRS_ZENABLE, FALSE);
    hr = pD3DDev->SetRenderState(D3DRS_STENCILENABLE, FALSE);
    hr = pD3DDev->SetRenderState(D3DRS_ALPHABLENDENABLE, TRUE);
    hr = pD3DDev->SetRenderState(D3DRS_SRCBLEND, D3DBLEND_SRCALPHA);
    hr = pD3DDev->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA);
    hr = pD3DDev->SetRenderState(D3DRS_ALPHATESTENABLE, FALSE);
    hr = pD3DDev->SetRenderState(D3DRS_SCISSORTESTENABLE, FALSE);

    hr = pD3DDev->SetRenderState(D3DRS_COLORWRITEENABLE, D3DCOLORWRITEENABLE_ALPHA | D3DCOLORWRITEENABLE_BLUE | D3DCOLORWRITEENABLE_GREEN | D3DCOLORWRITEENABLE_RED);

    hr = pD3DDev->SetFVF(D3DFVF_XYZRHW | D3DFVF_TEX0 | D3DFVF_DIFFUSE);

    MYD3DVERTEX<0> tmp = v[2];
    v[2] = v[3];
    v[3] = tmp;
    hr = pD3DDev->DrawPrimitiveUP(D3DPT_TRIANGLEFAN, 2, v, sizeof(v[0]));

    return S_OK;
}

MFOffset CBaseAP::GetOffset(float v)
{
    MFOffset offset;
    offset.value = short(v);
    offset.fract = WORD(65536 * (v - offset.value));
    return offset;
}

MFVideoArea CBaseAP::GetArea(float x, float y, DWORD width, DWORD height)
{
    MFVideoArea area;
    area.OffsetX = GetOffset(x);
    area.OffsetY = GetOffset(y);
    area.Area.cx = width;
    area.Area.cy = height;
    return area;
}

void CBaseAP::ResetStats()
{
    m_pGenlock->ResetStats();
    m_lAudioLag = 0;
    m_lAudioLagMin = 10000;
    m_lAudioLagMax = -10000;
    m_MinJitter = MAXLONG64;
    m_MaxJitter = MINLONG64;
    m_MinSyncOffset = MAXLONG64;
    m_MaxSyncOffset = MINLONG64;
    m_uSyncGlitches = 0;
    m_pcFramesDropped = 0;
    m_llLastSyncTime = LONGLONG_ERROR;
}

bool CBaseAP::SettingsNeedResetDevice()
{
    CRenderersSettings& r = GetRenderersSettings();
    CRenderersSettings::CAdvRendererSettings& New = r.m_AdvRendSets;
    CRenderersSettings::CAdvRendererSettings& Current = m_LastRendererSettings;

    bool bRet = false;
    if (!m_bIsFullscreen) {
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
    bRet = bRet || New.bEVRHighColorResolution != Current.bEVRHighColorResolution;
    m_LastRendererSettings = r.m_AdvRendSets;
    return bRet;
}

HRESULT CBaseAP::CreateDXDevice(CString& _Error)
{
    TRACE(_T("--> CBaseAP::CreateDXDevice on thread: %lu\n"), GetCurrentThreadId());
    const CRenderersSettings& r = GetRenderersSettings();
    m_LastRendererSettings = r.m_AdvRendSets;
    HRESULT hr = E_FAIL;

    m_pFont = nullptr;
    m_pSprite = nullptr;
    m_pLine = nullptr;
    m_pOSDTexture = nullptr;
    m_pOSDSurface = nullptr;

    m_pPSC.Free();

    m_pResizerPixelShader[0] = 0;
    m_pResizerPixelShader[1] = 0;
    m_pResizerPixelShader[2] = 0;
    m_pResizerPixelShader[3] = 0;

    POSITION pos = m_pPixelShadersScreenSpace.GetHeadPosition();
    while (pos) {
        CExternalPixelShader& Shader = m_pPixelShadersScreenSpace.GetNext(pos);
        Shader.m_pPixelShader = nullptr;
    }
    pos = m_pPixelShaders.GetHeadPosition();
    while (pos) {
        CExternalPixelShader& Shader = m_pPixelShaders.GetNext(pos);
        Shader.m_pPixelShader = nullptr;
    }

    if (!m_pD3D) {
        _Error += L"Failed to create Direct3D device\n";
        return E_UNEXPECTED;
    }

    if (m_pD3DDevEx) {
        hr = m_pD3DDevEx->CheckDeviceState(NULL);
        if (hr == D3DERR_DEVICELOST || hr == D3DERR_DEVICEREMOVED || hr == D3DERR_DEVICEHUNG || hr == D3DERR_OUTOFVIDEOMEMORY) {
            m_pD3DDevEx.Release();
            m_pD3DDev.Release();
            m_pD3DEx.Release();
            m_pD3D.Release();
            Direct3DCreate9Ex(D3D_SDK_VERSION, &m_pD3DEx);
            if (m_pD3DEx) {
                m_pD3D = m_pD3DEx;
            } else {
                return E_UNEXPECTED;
            }
        } else if (FAILED(hr)) {
            m_pD3DDevEx.Release();
            m_pD3DDev.Release();
        }
    }

    UINT currentAdapter = GetAdapter(m_pD3D, m_hWnd);
    bool bTryToReset = (currentAdapter == m_CurrentAdapter);
    if (!bTryToReset) {
        m_pD3DDev.Release();
        m_pD3DDevEx.Release();
        m_CurrentAdapter = currentAdapter;
    }

    D3DDISPLAYMODE d3ddm;
    ZeroMemory(&d3ddm, sizeof(d3ddm));
    if (FAILED(m_pD3D->GetAdapterDisplayMode(m_CurrentAdapter, &d3ddm))
            || d3ddm.Width <= 0 || d3ddm.Height <= 0) {
        _Error += L"Can not retrieve display mode data\n";
        return E_UNEXPECTED;
    }

    if (FAILED(m_pD3D->GetDeviceCaps(m_CurrentAdapter, D3DDEVTYPE_HAL, &m_caps))) {
        if ((m_caps.Caps & D3DCAPS_READ_SCANLINE) == 0) {
            _Error += L"Video card does not have scanline access. Display synchronization is not possible.\n";
            return E_UNEXPECTED;
        }
    }

    m_refreshRate = d3ddm.RefreshRate;
    m_dD3DRefreshCycle = 1000.0 / m_refreshRate; // In ms
    m_ScreenSize.SetSize(d3ddm.Width, d3ddm.Height);
    m_pGenlock->SetDisplayResolution(d3ddm.Width, d3ddm.Height);

    BOOL bCompositionEnabled = false;
    if (m_pDwmIsCompositionEnabled) {
        m_pDwmIsCompositionEnabled(&bCompositionEnabled);
    }
    m_bCompositionEnabled = bCompositionEnabled != 0;

    CSize largestScreen = GetLargestScreenSize(CSize(2560, 1440));

    ZeroMemory(&m_pp, sizeof(m_pp));
    if (m_bIsFullscreen) { // Exclusive mode fullscreen
        m_pp.Windowed = FALSE;
        m_pp.BackBufferWidth = d3ddm.Width;
        m_pp.BackBufferHeight = d3ddm.Height;
        m_pp.hDeviceWindow = m_hWnd;
        TRACE(_T("Wnd in CreateDXDevice: %p\n"), m_hWnd);
        m_pp.BackBufferCount = 3;
        m_pp.SwapEffect = D3DSWAPEFFECT_DISCARD;
        m_pp.PresentationInterval = D3DPRESENT_INTERVAL_ONE;
        m_pp.Flags = D3DPRESENTFLAG_VIDEO;
        m_bHighColorResolution = r.m_AdvRendSets.bEVRHighColorResolution;
        if (m_bHighColorResolution) {
            if (FAILED(m_pD3D->CheckDeviceType(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, d3ddm.Format, D3DFMT_A2R10G10B10, false))) {
                m_strStatsMsg[MSG_ERROR] = L"10 bit RGB is not supported by this graphics device in this resolution.";
                m_bHighColorResolution = false;
            }
        }

        if (m_bHighColorResolution) {
            m_pp.BackBufferFormat = D3DFMT_A2R10G10B10;
        } else {
            m_pp.BackBufferFormat = d3ddm.Format;
        }

        if (!m_FocusThread) {
            m_FocusThread = (CFocusThread*)AfxBeginThread(RUNTIME_CLASS(CFocusThread), 0, 0, 0);
        }

        HWND hFocusWindow = m_FocusThread->GetFocusWindow();
        bTryToReset &= (m_hFocusWindow == hFocusWindow || fullScreenChanged);
        m_hFocusWindow = hFocusWindow;

        if (m_pD3DEx) {
            D3DDISPLAYMODEEX DisplayMode;
            ZeroMemory(&DisplayMode, sizeof(DisplayMode));
            DisplayMode.Size = sizeof(DisplayMode);
            m_pD3DEx->GetAdapterDisplayModeEx(m_CurrentAdapter, &DisplayMode, nullptr);

            DisplayMode.Format = m_pp.BackBufferFormat;
            m_pp.FullScreen_RefreshRateInHz = DisplayMode.RefreshRate;

            bTryToReset = bTryToReset && m_pD3DDevEx;
            if (bTryToReset) {
                if (FAILED(hr = m_pD3DDevEx->ResetEx(&m_pp, &DisplayMode))) {
                    bTryToReset = false;
                } 
            }

            if (!bTryToReset) {
                m_pD3DDev.Release();
                m_pD3DDevEx.Release();
                hr = m_pD3DEx->CreateDeviceEx(m_CurrentAdapter, D3DDEVTYPE_HAL, m_hFocusWindow,
                                              D3DCREATE_HARDWARE_VERTEXPROCESSING | D3DCREATE_FPU_PRESERVE | D3DCREATE_MULTITHREADED | D3DCREATE_ENABLE_PRESENTSTATS | D3DCREATE_NOWINDOWCHANGES,
                                              &m_pp, &DisplayMode, &m_pD3DDevEx);
                if (m_pD3DDevEx) {
                    m_pD3DDev = m_pD3DDevEx;
                }
            }

            if (m_pD3DDevEx) {
                m_BackbufferType = m_pp.BackBufferFormat;
                m_DisplayType = DisplayMode.Format;
            }
        } else {
            bTryToReset = bTryToReset && m_pD3DDev;
            if (bTryToReset) {
                hr = m_pD3DDev->TestCooperativeLevel();
                if (hr == S_OK || hr == D3DERR_DEVICENOTRESET) {
                    if (FAILED(hr = m_pD3DDev->Reset(&m_pp))) {
                        bTryToReset = false;
                    }
                } else {
                    bTryToReset = false;
                }
            }
            if (!bTryToReset) {
                m_pD3DDev.Release();
                m_pD3DDevEx.Release();
                hr = m_pD3D->CreateDevice(m_CurrentAdapter, D3DDEVTYPE_HAL, m_hFocusWindow,
                                          D3DCREATE_HARDWARE_VERTEXPROCESSING | D3DCREATE_FPU_PRESERVE | D3DCREATE_MULTITHREADED | D3DCREATE_NOWINDOWCHANGES,
                                          &m_pp, &m_pD3DDev);
            }
            TRACE(_T("Created full-screen device\n"));
            if (m_pD3DDev) {
                m_BackbufferType = m_pp.BackBufferFormat;
                m_DisplayType = d3ddm.Format;
            }
        }
    } else { // Windowed
        m_pp.Windowed = TRUE;
        m_pp.hDeviceWindow = m_hWnd;
        m_pp.SwapEffect = D3DSWAPEFFECT_COPY;
        m_pp.Flags = D3DPRESENTFLAG_VIDEO;
        m_pp.BackBufferCount = 1;
        CSize bbsize = GetBackBufferSize(m_ScreenSize, largestScreen, r.m_AdvRendSets.bDesktopSizeBackBuffer);
        m_pp.BackBufferWidth  = bbsize.cx;
        m_pp.BackBufferHeight = bbsize.cy;
        m_BackbufferType = d3ddm.Format;
        m_DisplayType = d3ddm.Format;
        m_bHighColorResolution = r.m_AdvRendSets.bEVRHighColorResolution;
        if (m_bHighColorResolution) {
            if (FAILED(m_pD3D->CheckDeviceType(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, d3ddm.Format, D3DFMT_A2R10G10B10, false))) {
                m_strStatsMsg[MSG_ERROR] = L"10 bit RGB is not supported by this graphics device in this resolution.";
                m_bHighColorResolution = false;
            }
        }

        if (m_bHighColorResolution) {
            m_BackbufferType = D3DFMT_A2R10G10B10;
            m_pp.BackBufferFormat = D3DFMT_A2R10G10B10;
        }
        if (bCompositionEnabled) {
            // Desktop composition presents the whole desktop
            m_pp.PresentationInterval = D3DPRESENT_INTERVAL_IMMEDIATE;
        } else {
            m_pp.PresentationInterval = D3DPRESENT_INTERVAL_ONE;
        }

        bTryToReset &= (m_hFocusWindow == m_hWnd || fullScreenChanged);
        m_hFocusWindow = m_hWnd;

        if (m_pD3DEx) {
            bTryToReset = bTryToReset && m_pD3DDevEx;
            if (bTryToReset) {
                if (FAILED(m_pD3DDevEx->CheckDeviceState(NULL)) || FAILED(hr = m_pD3DDevEx->ResetEx(&m_pp, nullptr))) {
                    bTryToReset = false;
                } 
            }

            if (!bTryToReset) {
                m_pD3DDev.Release();
                m_pD3DDevEx.Release();
                hr = m_pD3DEx->CreateDeviceEx(m_CurrentAdapter, D3DDEVTYPE_HAL, m_hFocusWindow,
                                              D3DCREATE_HARDWARE_VERTEXPROCESSING | D3DCREATE_FPU_PRESERVE | D3DCREATE_MULTITHREADED | D3DCREATE_ENABLE_PRESENTSTATS,
                                              &m_pp, nullptr, &m_pD3DDevEx);
                if (m_pD3DDevEx) {
                    m_pD3DDev = m_pD3DDevEx;
                }
            }
        } else {
            bTryToReset = bTryToReset && m_pD3DDev;
            if (bTryToReset) {
                hr = m_pD3DDev->TestCooperativeLevel();
                if (hr == S_OK || hr == D3DERR_DEVICENOTRESET) {
                    if (FAILED(hr = m_pD3DDev->Reset(&m_pp))) {
                        bTryToReset = false;
                    }
                } else {
                    bTryToReset = false;
                }
            }

            if (!bTryToReset) {
                m_pD3DDev.Release();
                m_pD3DDevEx.Release();
                hr = m_pD3D->CreateDevice(m_CurrentAdapter, D3DDEVTYPE_HAL, m_hFocusWindow,
                                          D3DCREATE_HARDWARE_VERTEXPROCESSING | D3DCREATE_FPU_PRESERVE | D3DCREATE_MULTITHREADED,
                                          &m_pp, &m_pD3DDev);
            }
            TRACE(_T("Created windowed device\n"));
        }
    }

    if (m_pD3DDev) {
        for (int i = 0; i < 40 && hr == D3DERR_DEVICELOST; i++) {
            TRACE(_T("D3DERR_DEVICELOST. Trying to Reset.\n"));
            Sleep(50);
            hr = m_pD3DDev->TestCooperativeLevel();
        }
        if (hr == D3DERR_DEVICENOTRESET) {
            TRACE(_T("D3DERR_DEVICENOTRESET\n"));
            hr = m_pD3DDev->Reset(&m_pp);
        }

        if (m_pD3DDevEx) {
            m_pD3DDevEx->SetGPUThreadPriority(7);
        }
    }

    if (FAILED(hr)) {
        _Error.AppendFormat(_T("CreateDevice failed: %s\n"), GetWindowsErrorMessage(hr, nullptr).GetString());

        return hr;
    }

    m_pPSC.Attach(DEBUG_NEW CPixelShaderCompiler(m_pD3DDev, true));
    m_filter = D3DTEXF_NONE;

    if (m_caps.StretchRectFilterCaps & D3DPTFILTERCAPS_MINFLINEAR && m_caps.StretchRectFilterCaps & D3DPTFILTERCAPS_MAGFLINEAR) {
        m_filter = D3DTEXF_LINEAR;
    }

    m_bicubicA = 0;

    InitMaxSubtitleTextureSize(r.subPicQueueSettings.nMaxResX, r.subPicQueueSettings.nMaxResY, largestScreen);

    if (m_pAllocator) {
        m_pAllocator->ChangeDevice(m_pD3DDev);
    } else {
        m_pAllocator = DEBUG_NEW CDX9SubPicAllocator(m_pD3DDev, m_maxSubtitleTextureSize, false);
    }

    hr = S_OK;

    CComPtr<ISubPicProvider> pSubPicProvider;
    if (m_pSubPicQueue) {
        TRACE(_T("m_pSubPicQueue != nullptr\n"));
        m_pSubPicQueue->GetSubPicProvider(&pSubPicProvider);
    }

    if (!m_pSubPicQueue) {
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

    m_LastAdapterCheck = GetRenderersData()->GetPerfCounter();
    return S_OK;
}

// function is not used?
HRESULT CBaseAP::ResetDXDevice(CString& _Error)
{
    const CRenderersSettings& r = GetRenderersSettings();
    m_LastRendererSettings = r.m_AdvRendSets;
    HRESULT hr = E_FAIL;

    hr = m_pD3DDev->TestCooperativeLevel();
    if ((hr != D3DERR_DEVICENOTRESET) && (hr != D3D_OK)) {
        return hr;
    }

    CComPtr<IEnumPins> rendererInputEnum;
    std::vector<CComPtr<IPin>> decoderOutput;
    std::vector<CComPtr<IPin>> rendererInput;
    CFilterInfo filterInfo;

    bool disconnected = false;

    // Disconnect all pins to release video memory resources
    if (m_pD3DDev) {
        m_pOuterEVR->QueryFilterInfo(&filterInfo);
        if (SUCCEEDED(m_pOuterEVR->EnumPins(&rendererInputEnum))) {
            CComPtr<IPin> input;
            CComPtr<IPin> output;
            while (hr = rendererInputEnum->Next(1, &input.p, 0), hr == S_OK) { // Must have .p here
                TRACE(_T("Pin found\n"));
                input->ConnectedTo(&output.p);
                if (output != nullptr) {
                    rendererInput.push_back(input);
                    decoderOutput.push_back(output);
                }
                input.Release();
                output.Release();
            }
        } else {
            return hr;
        }
        if (filterInfo.pGraph) {
            for (size_t i = 0; i < decoderOutput.size(); i++) {
                TRACE(_T("Disconnecting pin\n"));
                filterInfo.pGraph->Disconnect(decoderOutput[i].p);
                filterInfo.pGraph->Disconnect(rendererInput[i].p);
                TRACE(_T("Pin disconnected\n"));
            }
            disconnected = true;
        }
    }

    // Release more resources
    m_pSubPicQueue = nullptr;
    m_pFont = nullptr;
    m_pSprite = nullptr;
    m_pLine = nullptr;
    m_pPSC.Free();

    m_pResizerPixelShader[0] = 0;
    m_pResizerPixelShader[1] = 0;
    m_pResizerPixelShader[2] = 0;
    m_pResizerPixelShader[3] = 0;

    POSITION pos = m_pPixelShadersScreenSpace.GetHeadPosition();
    while (pos) {
        CExternalPixelShader& Shader = m_pPixelShadersScreenSpace.GetNext(pos);
        Shader.m_pPixelShader = nullptr;
    }
    pos = m_pPixelShaders.GetHeadPosition();
    while (pos) {
        CExternalPixelShader& Shader = m_pPixelShaders.GetNext(pos);
        Shader.m_pPixelShader = nullptr;
    }

    D3DDISPLAYMODE d3ddm;
    ZeroMemory(&d3ddm, sizeof(d3ddm));
    if (FAILED(m_pD3D->GetAdapterDisplayMode(GetAdapter(m_pD3D, m_hWnd), &d3ddm))
            || d3ddm.Width <= 0 || d3ddm.Height <= 0) {
        _Error += L"Can not retrieve display mode data\n";
        return E_UNEXPECTED;
    }

    m_refreshRate = d3ddm.RefreshRate;
    m_dD3DRefreshCycle = 1000.0 / m_refreshRate; // In ms
    m_ScreenSize.SetSize(d3ddm.Width, d3ddm.Height);
    m_pGenlock->SetDisplayResolution(d3ddm.Width, d3ddm.Height);
    CSize szDesktopSize(GetSystemMetrics(SM_CXVIRTUALSCREEN), GetSystemMetrics(SM_CYVIRTUALSCREEN));

    ZeroMemory(&m_pp, sizeof(m_pp));

    BOOL bCompositionEnabled = false;
    if (m_pDwmIsCompositionEnabled) {
        m_pDwmIsCompositionEnabled(&bCompositionEnabled);
    }
    m_bCompositionEnabled = bCompositionEnabled != 0;
    m_bHighColorResolution = r.m_AdvRendSets.bEVRHighColorResolution;

    CSize largestScreen = GetLargestScreenSize(CSize(2560, 1440));

    if (m_bIsFullscreen) { // Exclusive mode fullscreen
        m_pp.BackBufferWidth = d3ddm.Width;
        m_pp.BackBufferHeight = d3ddm.Height;
        if (m_bHighColorResolution) {
            m_pp.BackBufferFormat = D3DFMT_A2R10G10B10;
        } else {
            m_pp.BackBufferFormat = d3ddm.Format;
        }
        if (FAILED(m_pD3DEx->CheckDeviceType(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, m_pp.BackBufferFormat, m_pp.BackBufferFormat, false))) {
            _Error += L"10 bit RGB is not supported by this graphics device in exclusive mode fullscreen.\n";
            return hr;
        }

        D3DDISPLAYMODEEX DisplayMode;
        ZeroMemory(&DisplayMode, sizeof(DisplayMode));
        DisplayMode.Size = sizeof(DisplayMode);
        if (m_pD3DDevEx) {
            m_pD3DEx->GetAdapterDisplayModeEx(GetAdapter(m_pD3DEx, m_hWnd), &DisplayMode, nullptr);
            DisplayMode.Format = m_pp.BackBufferFormat;
            m_pp.FullScreen_RefreshRateInHz = DisplayMode.RefreshRate;
            if (FAILED(m_pD3DDevEx->CheckDeviceState(NULL)) || FAILED(m_pD3DDevEx->ResetEx(&m_pp, &DisplayMode))) {
                _Error += GetWindowsErrorMessage(hr, nullptr);
                return hr;
            }
        } else if (m_pD3DDev) {
            if (FAILED(m_pD3DDev->Reset(&m_pp))) {
                _Error += GetWindowsErrorMessage(hr, nullptr);
                return hr;
            }
        } else {
            _Error += L"No device.\n";
            return hr;
        }
        m_BackbufferType = m_pp.BackBufferFormat;
        m_DisplayType = d3ddm.Format;
    } else { // Windowed
        CSize bbsize = GetBackBufferSize(m_ScreenSize, largestScreen, r.m_AdvRendSets.bDesktopSizeBackBuffer);
        m_pp.BackBufferWidth  = bbsize.cx;
        m_pp.BackBufferHeight = bbsize.cy;
        m_BackbufferType = d3ddm.Format;
        m_DisplayType = d3ddm.Format;
        if (m_bHighColorResolution) {
            m_BackbufferType = D3DFMT_A2R10G10B10;
            m_pp.BackBufferFormat = D3DFMT_A2R10G10B10;
        }
        if (FAILED(m_pD3DEx->CheckDeviceType(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, m_pp.BackBufferFormat, m_pp.BackBufferFormat, false))) {
            _Error += L"10 bit RGB is not supported by this graphics device in windowed mode.\n";
            return hr;
        }
        if (bCompositionEnabled) {
            // Desktop composition presents the whole desktop
            m_pp.PresentationInterval = D3DPRESENT_INTERVAL_IMMEDIATE;
        } else {
            m_pp.PresentationInterval = D3DPRESENT_INTERVAL_ONE;
        }
        if (m_pD3DDevEx)
            if (FAILED(m_pD3DDevEx->CheckDeviceState(NULL)) || FAILED(m_pD3DDevEx->ResetEx(&m_pp, nullptr))) {
                _Error += GetWindowsErrorMessage(hr, nullptr);
                return hr;
            } else if (m_pD3DDev)
                if (FAILED(m_pD3DDevEx->Reset(&m_pp))) {
                    _Error += GetWindowsErrorMessage(hr, nullptr);
                    return hr;
                } else {
                    _Error += L"No device.\n";
                    return hr;
                }
    }

    if (disconnected) {
        for (size_t i = 0; i < decoderOutput.size(); i++) {
            if (FAILED(filterInfo.pGraph->ConnectDirect(decoderOutput[i].p, rendererInput[i].p, nullptr))) {
                return hr;
            }
        }
    }

    m_pPSC.Attach(DEBUG_NEW CPixelShaderCompiler(m_pD3DDev, true));
    m_filter = D3DTEXF_NONE;

    if ((m_caps.StretchRectFilterCaps & D3DPTFILTERCAPS_MINFLINEAR)
            && (m_caps.StretchRectFilterCaps & D3DPTFILTERCAPS_MAGFLINEAR)) {
        m_filter = D3DTEXF_LINEAR;
    }

    m_bicubicA = 0;

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

    m_pFont = nullptr;
    m_pSprite = nullptr;
    m_pLine = nullptr;

    return S_OK;
}

HRESULT CBaseAP::AllocSurfaces(D3DFORMAT Format)
{
    CAutoLock cAutoLock(this);
    CAutoLock cRenderLock(&m_allocatorLock);

    CheckPointer(m_pD3DDev, E_POINTER);

    const CRenderersSettings& r = GetRenderersSettings();

    for (int i = 0; i < m_nDXSurface + 2; i++) {
        m_pVideoTexture[i] = nullptr;
        m_pVideoSurface[i] = nullptr;
    }

    m_pScreenSizeTemporaryTexture[0] = nullptr;
    m_pScreenSizeTemporaryTexture[1] = nullptr;
    m_SurfaceType = Format;

    HRESULT hr;
    if (r.iAPSurfaceUsage == VIDRNDT_AP_TEXTURE2D || r.iAPSurfaceUsage == VIDRNDT_AP_TEXTURE3D) {
        int nTexturesNeeded = r.iAPSurfaceUsage == VIDRNDT_AP_TEXTURE3D ? m_nDXSurface + 2 : 1;

        for (int i = 0; i < nTexturesNeeded; i++) {
            if (FAILED(hr = m_pD3DDev->CreateTexture(
                                m_nativeVideoSize.cx,
                                m_nativeVideoSize.cy,
                                1,
                                D3DUSAGE_RENDERTARGET,
                                Format,
                                D3DPOOL_DEFAULT,
                                &m_pVideoTexture[i],
                                nullptr))) {
                return hr;
            }

            if (FAILED(hr = m_pVideoTexture[i]->GetSurfaceLevel(0, &m_pVideoSurface[i]))) {
                return hr;
            }
        }
        if (r.iAPSurfaceUsage == VIDRNDT_AP_TEXTURE2D) {
            for (int i = 0; i < m_nDXSurface + 2; i++) {
                m_pVideoTexture[i] = nullptr;
            }
        }
    } else {
        if (FAILED(hr = m_pD3DDev->CreateOffscreenPlainSurface(m_nativeVideoSize.cx, m_nativeVideoSize.cy, D3DFMT_X8R8G8B8, D3DPOOL_DEFAULT, &m_pVideoSurface[m_nCurSurface], nullptr))) {
            return hr;
        }
    }

    hr = m_pD3DDev->ColorFill(m_pVideoSurface[m_nCurSurface], nullptr, 0);
    return S_OK;
}

void CBaseAP::DeleteSurfaces()
{
    CAutoLock cAutoLock(this);
    CAutoLock cRenderLock(&m_allocatorLock);

    for (int i = 0; i < m_nDXSurface + 2; i++) {
        m_pVideoTexture[i] = nullptr;
        m_pVideoSurface[i] = nullptr;
    }
}

// ISubPicAllocatorPresenter

STDMETHODIMP CBaseAP::CreateRenderer(IUnknown** ppRenderer)
{
    return E_NOTIMPL;
}

bool CBaseAP::ClipToSurface(IDirect3DSurface9* pSurface, CRect& s, CRect& d)
{
    D3DSURFACE_DESC desc;
    ZeroMemory(&desc, sizeof(desc));
    if (FAILED(pSurface->GetDesc(&desc))) {
        return false;
    }

    int w = desc.Width, h = desc.Height;
    int sw = s.Width(), sh = s.Height();
    int dw = d.Width(), dh = d.Height();

    if (d.left >= w || d.right < 0 || d.top >= h || d.bottom < 0
            || sw <= 0 || sh <= 0 || dw <= 0 || dh <= 0) {
        s.SetRectEmpty();
        d.SetRectEmpty();
        return true;
    }
    if (d.right > w) {
        s.right -= (d.right - w) * sw / dw;
        d.right = w;
    }
    if (d.bottom > h) {
        s.bottom -= (d.bottom - h) * sh / dh;
        d.bottom = h;
    }
    if (d.left < 0) {
        s.left += (0 - d.left) * sw / dw;
        d.left = 0;
    }
    if (d.top < 0) {
        s.top += (0 - d.top) * sh / dh;
        d.top = 0;
    }
    return true;
}

HRESULT CBaseAP::InitResizers(float bicubicA, bool bNeedScreenSizeTexture)
{
    HRESULT hr;
    do {
        if (bicubicA) {
            if (!m_pResizerPixelShader[0]) {
                break;
            }
            if (!m_pResizerPixelShader[1]) {
                break;
            }
            if (!m_pResizerPixelShader[2]) {
                break;
            }
            if (!m_pResizerPixelShader[3]) {
                break;
            }
            if (m_bicubicA != bicubicA) {
                break;
            }
            if (!m_pScreenSizeTemporaryTexture[0]) {
                break;
            }
            if (bNeedScreenSizeTexture) {
                if (!m_pScreenSizeTemporaryTexture[1]) {
                    break;
                }
            }
        } else {
            if (!m_pResizerPixelShader[0]) {
                break;
            }
            if (bNeedScreenSizeTexture) {
                if (!m_pScreenSizeTemporaryTexture[0]) {
                    break;
                }
                if (!m_pScreenSizeTemporaryTexture[1]) {
                    break;
                }
            }
        }
        return S_OK;
    } while (0);

    m_bicubicA = bicubicA;
    m_pScreenSizeTemporaryTexture[0] = nullptr;
    m_pScreenSizeTemporaryTexture[1] = nullptr;

    for (int i = 0; i < _countof(m_pResizerPixelShader); i++) {
        m_pResizerPixelShader[i] = nullptr;
    }

    if (m_caps.PixelShaderVersion < D3DPS_VERSION(2, 0)) {
        return E_FAIL;
    }

    LPCSTR pProfile = m_caps.PixelShaderVersion >= D3DPS_VERSION(3, 0) ? "ps_3_0" : "ps_2_0";

    CStringA str;
    if (!LoadResource(IDF_SHADER_RESIZER, str, _T("SHADER"))) {
        return E_FAIL;
    }

    CStringA A;
    A.Format("(%f)", bicubicA);
    str.Replace("_The_Value_Of_A_Is_Set_Here_", A);

    LPCSTR pEntries[] = {"main_bilinear", "main_bicubic1pass", "main_bicubic2pass_pass1", "main_bicubic2pass_pass2"};

    ASSERT(_countof(pEntries) == _countof(m_pResizerPixelShader));
    for (int i = 0; i < _countof(pEntries); i++) {
        CString ErrorMessage;
        CString DissAssembly;
        hr = m_pPSC->CompileShader(str, pEntries[i], pProfile, 0, &m_pResizerPixelShader[i], &DissAssembly, &ErrorMessage);
        if (FAILED(hr)) {
            TRACE(_T("%ws"), ErrorMessage.GetString());
            ASSERT(0);
            return hr;
        }
    }
    if (m_bicubicA || bNeedScreenSizeTexture) {
        if (FAILED(m_pD3DDev->CreateTexture(
                       std::min((DWORD)m_ScreenSize.cx, m_caps.MaxTextureWidth),
                       std::min((DWORD)std::max(m_ScreenSize.cy, m_nativeVideoSize.cy), m_caps.MaxTextureHeight),
                       1,
                       D3DUSAGE_RENDERTARGET,
                       D3DFMT_A8R8G8B8,
                       D3DPOOL_DEFAULT,
                       &m_pScreenSizeTemporaryTexture[0],
                       nullptr))) {
            ASSERT(0);
            m_pScreenSizeTemporaryTexture[0] = nullptr; // will do 1 pass then
        }

        if (FAILED(m_pD3DDev->CreateTexture(
                       std::min((DWORD)m_ScreenSize.cx, m_caps.MaxTextureWidth),
                       std::min((DWORD)std::max(m_ScreenSize.cy, m_nativeVideoSize.cy), m_caps.MaxTextureHeight),
                       1,
                       D3DUSAGE_RENDERTARGET,
                       D3DFMT_A8R8G8B8,
                       D3DPOOL_DEFAULT,
                       &m_pScreenSizeTemporaryTexture[1],
                       nullptr))) {
            ASSERT(0);
            m_pScreenSizeTemporaryTexture[1] = nullptr; // will do 1 pass then
        }
    }
    return S_OK;
}

HRESULT CBaseAP::TextureCopy(IDirect3DTexture9* pTexture)
{
    HRESULT hr;

    D3DSURFACE_DESC desc;
    if (!pTexture || FAILED(pTexture->GetLevelDesc(0, &desc))) {
        return E_FAIL;
    }

    float w = (float)desc.Width;
    float h = (float)desc.Height;
    MYD3DVERTEX<1> v[] = {
        {0, 0, 0.5f, 2.0f, 0, 0},
        {w, 0, 0.5f, 2.0f, 1, 0},
        {0, h, 0.5f, 2.0f, 0, 1},
        {w, h, 0.5f, 2.0f, 1, 1},
    };
    for (int i = 0; i < _countof(v); i++) {
        v[i].x -= 0.5;
        v[i].y -= 0.5;
    }
    hr = m_pD3DDev->SetTexture(0, pTexture);
    return TextureBlt(m_pD3DDev, v, D3DTEXF_POINT);
}

HRESULT CBaseAP::DrawRect(DWORD _Color, DWORD _Alpha, const CRect& _Rect)
{
    DWORD Color = D3DCOLOR_ARGB(_Alpha, GetRValue(_Color), GetGValue(_Color), GetBValue(_Color));
    MYD3DVERTEX<0> v[] = {
        {float(_Rect.left), float(_Rect.top), 0.5f, 2.0f, Color},
        {float(_Rect.right), float(_Rect.top), 0.5f, 2.0f, Color},
        {float(_Rect.left), float(_Rect.bottom), 0.5f, 2.0f, Color},
        {float(_Rect.right), float(_Rect.bottom), 0.5f, 2.0f, Color},
    };
    for (int i = 0; i < _countof(v); i++) {
        v[i].x -= 0.5;
        v[i].y -= 0.5;
    }
    return DrawRectBase(m_pD3DDev, v);
}

HRESULT CBaseAP::TextureResize(IDirect3DTexture9* pTexture, const Vector dst[4], D3DTEXTUREFILTERTYPE filter, const CRect& SrcRect)
{
    HRESULT hr;

    D3DSURFACE_DESC desc;
    if (!pTexture || FAILED(pTexture->GetLevelDesc(0, &desc))) {
        return E_FAIL;
    }

    float w = (float)desc.Width;
    float h = (float)desc.Height;

    float dx2 = 1.0f / w;
    float dy2 = 1.0f / h;

    MYD3DVERTEX<1> v[] = {
        {dst[0].x, dst[0].y, dst[0].z, 1.0f / dst[0].z, SrcRect.left * dx2, SrcRect.top * dy2},
        {dst[1].x, dst[1].y, dst[1].z, 1.0f / dst[1].z, SrcRect.right * dx2, SrcRect.top * dy2},
        {dst[2].x, dst[2].y, dst[2].z, 1.0f / dst[2].z, SrcRect.left * dx2, SrcRect.bottom * dy2},
        {dst[3].x, dst[3].y, dst[3].z, 1.0f / dst[3].z, SrcRect.right * dx2, SrcRect.bottom * dy2},
    };
    AdjustQuad(v, 0, 0);
    hr = m_pD3DDev->SetTexture(0, pTexture);
    hr = m_pD3DDev->SetPixelShader(nullptr);
    hr = TextureBlt(m_pD3DDev, v, filter);
    return hr;
}

HRESULT CBaseAP::TextureResizeBilinear(IDirect3DTexture9* pTexture, const Vector dst[4], const CRect& SrcRect)
{
    HRESULT hr;

    D3DSURFACE_DESC desc;
    if (!pTexture || FAILED(pTexture->GetLevelDesc(0, &desc))) {
        return E_FAIL;
    }

    float w = (float)desc.Width;
    float h = (float)desc.Height;

    float tx0 = (float)SrcRect.left;
    float tx1 = (float)SrcRect.right;
    float ty0 = (float)SrcRect.top;
    float ty1 = (float)SrcRect.bottom;

    MYD3DVERTEX<1> v[] = {
        {dst[0].x, dst[0].y, dst[0].z, 1.0f / dst[0].z, tx0, ty0},
        {dst[1].x, dst[1].y, dst[1].z, 1.0f / dst[1].z, tx1, ty0},
        {dst[2].x, dst[2].y, dst[2].z, 1.0f / dst[2].z, tx0, ty1},
        {dst[3].x, dst[3].y, dst[3].z, 1.0f / dst[3].z, tx1, ty1},
    };
    AdjustQuad(v, 1.0, 1.0);
    float fConstData[][4] = {{0.5f / w, 0.5f / h, 0, 0}, {1.0f / w, 1.0f / h, 0, 0}, {1.0f / w, 0, 0, 0}, {0, 1.0f / h, 0, 0}, {w, h, 0, 0}};
    hr = m_pD3DDev->SetPixelShaderConstantF(0, (float*)fConstData, _countof(fConstData));
    hr = m_pD3DDev->SetTexture(0, pTexture);
    hr = m_pD3DDev->SetPixelShader(m_pResizerPixelShader[0]);
    hr = TextureBlt(m_pD3DDev, v, D3DTEXF_POINT);
    m_pD3DDev->SetPixelShader(nullptr);
    return hr;
}

HRESULT CBaseAP::TextureResizeBicubic1pass(IDirect3DTexture9* pTexture, const Vector dst[4], const CRect& SrcRect)
{
    HRESULT hr;

    D3DSURFACE_DESC desc;
    if (!pTexture || FAILED(pTexture->GetLevelDesc(0, &desc))) {
        return E_FAIL;
    }

    float w = (float)desc.Width;
    float h = (float)desc.Height;

    float tx0 = (float)SrcRect.left;
    float tx1 = (float)SrcRect.right;
    float ty0 = (float)SrcRect.top;
    float ty1 = (float)SrcRect.bottom;

    MYD3DVERTEX<1> v[] = {
        {dst[0].x, dst[0].y, dst[0].z, 1.0f / dst[0].z, tx0, ty0},
        {dst[1].x, dst[1].y, dst[1].z, 1.0f / dst[1].z, tx1, ty0},
        {dst[2].x, dst[2].y, dst[2].z, 1.0f / dst[2].z, tx0, ty1},
        {dst[3].x, dst[3].y, dst[3].z, 1.0f / dst[3].z, tx1, ty1},
    };
    AdjustQuad(v, 1.0, 1.0);
    hr = m_pD3DDev->SetTexture(0, pTexture);
    float fConstData[][4] = {{0.5f / w, 0.5f / h, 0, 0}, {1.0f / w, 1.0f / h, 0, 0}, {1.0f / w, 0, 0, 0}, {0, 1.0f / h, 0, 0}, {w, h, 0, 0}};
    hr = m_pD3DDev->SetPixelShaderConstantF(0, (float*)fConstData, _countof(fConstData));
    hr = m_pD3DDev->SetPixelShader(m_pResizerPixelShader[1]);
    hr = TextureBlt(m_pD3DDev, v, D3DTEXF_POINT);
    m_pD3DDev->SetPixelShader(nullptr);
    return hr;
}

HRESULT CBaseAP::TextureResizeBicubic2pass(IDirect3DTexture9* pTexture, const Vector dst[4], const CRect& SrcRect)
{
    // The 2 pass sampler is incorrect in that it only does bilinear resampling in the y direction.
    return TextureResizeBicubic1pass(pTexture, dst, SrcRect);

    /*HRESULT hr;

    // rotated?
    if (dst[0].z != dst[1].z || dst[2].z != dst[3].z || dst[0].z != dst[3].z
    || dst[0].y != dst[1].y || dst[0].x != dst[2].x || dst[2].y != dst[3].y || dst[1].x != dst[3].x)
        return TextureResizeBicubic1pass(pTexture, dst, SrcRect);

    D3DSURFACE_DESC desc;
    if (!pTexture || FAILED(pTexture->GetLevelDesc(0, &desc)))
        return E_FAIL;

    float Tex0_Width = desc.Width;
    float Tex0_Height = desc.Height;

    CSize SrcTextSize = CSize(desc.Width, desc.Height);
    double w = (double)SrcRect.Width();
    double h = (double)SrcRect.Height();
    UNREFERENCED_PARAMETER(w);

    CRect dst1(0, 0, (int)(dst[3].x - dst[0].x), (int)h);

    if (!m_pScreenSizeTemporaryTexture[0] || FAILED(m_pScreenSizeTemporaryTexture[0]->GetLevelDesc(0, &desc)))
        return TextureResizeBicubic1pass(pTexture, dst, SrcRect);

    float Tex1_Width = desc.Width;
    float Tex1_Height = desc.Height;

    float tx0 = SrcRect.left;
    float tx1 = SrcRect.right;
    float ty0 = SrcRect.top;
    float ty1 = SrcRect.bottom;

    float tx0_2 = 0;
    float tx1_2 = dst1.Width();
    float ty0_2 = 0;
    float ty1_2 = h;

    if (dst1.Width() > (int)desc.Width || dst1.Height() > (int)desc.Height)
        return TextureResizeBicubic1pass(pTexture, dst, SrcRect);

    MYD3DVERTEX<1> vx[] =
    {
        {(float)dst1.left, (float)dst1.top,     0.5f, 2.0f, tx0, ty0},
        {(float)dst1.right, (float)dst1.top,    0.5f, 2.0f, tx1, ty0},
        {(float)dst1.left, (float)dst1.bottom,  0.5f, 2.0f, tx0, ty1},
        {(float)dst1.right, (float)dst1.bottom, 0.5f, 2.0f, tx1, ty1},
    };
    AdjustQuad(vx, 1.0, 0.0);       // Casimir666 : bug here, create vertical lines ! TODO : why ??????
    MYD3DVERTEX<1> vy[] =
    {
        {dst[0].x, dst[0].y, dst[0].z, 1.0/dst[0].z, tx0_2, ty0_2},
        {dst[1].x, dst[1].y, dst[1].z, 1.0/dst[1].z, tx1_2, ty0_2},
        {dst[2].x, dst[2].y, dst[2].z, 1.0/dst[2].z, tx0_2, ty1_2},
        {dst[3].x, dst[3].y, dst[3].z, 1.0/dst[3].z, tx1_2, ty1_2},
    };
    AdjustQuad(vy, 0.0, 1.0);
    hr = m_pD3DDev->SetPixelShader(m_pResizerPixelShader[2]);
    {
        float fConstData[][4] = {{0.5f / Tex0_Width, 0.5f / Tex0_Height, 0, 0}, {1.0f / Tex0_Width, 1.0f / Tex0_Height, 0, 0}, {1.0f / Tex0_Width, 0, 0, 0}, {0, 1.0f / Tex0_Height, 0, 0}, {Tex0_Width, Tex0_Height, 0, 0}};
        hr = m_pD3DDev->SetPixelShaderConstantF(0, (float*)fConstData, _countof(fConstData));
    }
    hr = m_pD3DDev->SetTexture(0, pTexture);
    CComPtr<IDirect3DSurface9> pRTOld;
    hr = m_pD3DDev->GetRenderTarget(0, &pRTOld);
    CComPtr<IDirect3DSurface9> pRT;
    hr = m_pScreenSizeTemporaryTexture[0]->GetSurfaceLevel(0, &pRT);
    hr = m_pD3DDev->SetRenderTarget(0, pRT);
    hr = TextureBlt(m_pD3DDev, vx, D3DTEXF_POINT);
    hr = m_pD3DDev->SetPixelShader(m_pResizerPixelShader[3]);
    {
        float fConstData[][4] = {{0.5f / Tex1_Width, 0.5f / Tex1_Height, 0, 0}, {1.0f / Tex1_Width, 1.0f / Tex1_Height, 0, 0}, {1.0f / Tex1_Width, 0, 0, 0}, {0, 1.0f / Tex1_Height, 0, 0}, {Tex1_Width, Tex1_Height, 0, 0}};
        hr = m_pD3DDev->SetPixelShaderConstantF(0, (float*)fConstData, _countof(fConstData));
    }
    hr = m_pD3DDev->SetTexture(0, m_pScreenSizeTemporaryTexture[0]);
    hr = m_pD3DDev->SetRenderTarget(0, pRTOld);
    hr = TextureBlt(m_pD3DDev, vy, D3DTEXF_POINT);
    m_pD3DDev->SetPixelShader(nullptr);
    return hr;*/
}

HRESULT CBaseAP::AlphaBlt(RECT* pSrc, const RECT* pDst, IDirect3DTexture9* pTexture)
{
    if (!pSrc || !pDst) {
        return E_POINTER;
    }

    CRect src(*pSrc), dst(*pDst);

    HRESULT hr;

    do {
        D3DSURFACE_DESC desc;
        ZeroMemory(&desc, sizeof(desc));
        if (FAILED(pTexture->GetLevelDesc(0, &desc)) /*|| desc.Type != D3DRTYPE_TEXTURE*/) {
            break;
        }

        float w = (float)desc.Width;
        float h = (float)desc.Height;

        // Be careful with the code that follows. Some compilers (e.g. Visual Studio 2012) used to miscompile
        // it in some cases (namely x64 with optimizations /O2 /Ot). This bug led pVertices not to be correctly
        // initialized and thus the subtitles weren't shown.
        struct {
            float x, y, z, rhw;
            float tu, tv;
        } pVertices[] = {
            {(float)dst.left, (float)dst.top, 0.5f, 2.0f, (float)src.left / w, (float)src.top / h},
            {(float)dst.right, (float)dst.top, 0.5f, 2.0f, (float)src.right / w, (float)src.top / h},
            {(float)dst.left, (float)dst.bottom, 0.5f, 2.0f, (float)src.left / w, (float)src.bottom / h},
            {(float)dst.right, (float)dst.bottom, 0.5f, 2.0f, (float)src.right / w, (float)src.bottom / h},
        };

        for (size_t i = 0; i < _countof(pVertices); i++) {
            pVertices[i].x -= 0.5f;
            pVertices[i].y -= 0.5f;
        }

        hr = m_pD3DDev->SetTexture(0, pTexture);

        // GetRenderState fails for devices created with D3DCREATE_PUREDEVICE
        // so we need to provide default values in case GetRenderState fails
        DWORD abe, sb, db;
        if (FAILED(m_pD3DDev->GetRenderState(D3DRS_ALPHABLENDENABLE, &abe))) {
            abe = FALSE;
        }
        if (FAILED(m_pD3DDev->GetRenderState(D3DRS_SRCBLEND, &sb))) {
            sb = D3DBLEND_ONE;
        }
        if (FAILED(m_pD3DDev->GetRenderState(D3DRS_DESTBLEND, &db))) {
            db = D3DBLEND_ZERO;
        }

        hr = m_pD3DDev->SetRenderState(D3DRS_CULLMODE, D3DCULL_NONE);
        hr = m_pD3DDev->SetRenderState(D3DRS_LIGHTING, FALSE);
        hr = m_pD3DDev->SetRenderState(D3DRS_ZENABLE, FALSE);
        hr = m_pD3DDev->SetRenderState(D3DRS_ALPHABLENDENABLE, TRUE);
        hr = m_pD3DDev->SetRenderState(D3DRS_SRCBLEND, D3DBLEND_ONE); // pre-multiplied src and ...
        hr = m_pD3DDev->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_SRCALPHA); // ... inverse alpha channel for dst

        hr = m_pD3DDev->SetTextureStageState(0, D3DTSS_COLOROP, D3DTOP_SELECTARG1);
        hr = m_pD3DDev->SetTextureStageState(0, D3DTSS_COLORARG1, D3DTA_TEXTURE);
        hr = m_pD3DDev->SetTextureStageState(0, D3DTSS_ALPHAARG1, D3DTA_TEXTURE);

        hr = m_pD3DDev->SetSamplerState(0, D3DSAMP_MAGFILTER, D3DTEXF_POINT);
        hr = m_pD3DDev->SetSamplerState(0, D3DSAMP_MINFILTER, D3DTEXF_POINT);
        hr = m_pD3DDev->SetSamplerState(0, D3DSAMP_MIPFILTER, D3DTEXF_NONE);

        hr = m_pD3DDev->SetSamplerState(0, D3DSAMP_ADDRESSU, D3DTADDRESS_CLAMP);
        hr = m_pD3DDev->SetSamplerState(0, D3DSAMP_ADDRESSV, D3DTADDRESS_CLAMP);

        hr = m_pD3DDev->SetPixelShader(nullptr);

        hr = m_pD3DDev->SetFVF(D3DFVF_XYZRHW | D3DFVF_TEX1);
        hr = m_pD3DDev->DrawPrimitiveUP(D3DPT_TRIANGLESTRIP, 2, pVertices, sizeof(pVertices[0]));

        m_pD3DDev->SetTexture(0, nullptr);

        m_pD3DDev->SetRenderState(D3DRS_ALPHABLENDENABLE, abe);
        m_pD3DDev->SetRenderState(D3DRS_SRCBLEND, sb);
        m_pD3DDev->SetRenderState(D3DRS_DESTBLEND, db);

        return S_OK;
    } while (0);
    return E_FAIL;
}

// Update the array m_pllJitter with a new vsync period. Calculate min, max and stddev.
void CBaseAP::SyncStats(LONGLONG syncTime)
{
    if (m_llLastSyncTime == LONGLONG_ERROR) {
        m_llLastSyncTime = syncTime;
    }

    m_nNextJitter = (m_nNextJitter + 1) % NB_JITTER;
    LONGLONG jitter = syncTime - m_llLastSyncTime;
    m_pllJitter[m_nNextJitter] = jitter;
    double syncDeviation = (m_pllJitter[m_nNextJitter] - m_fJitterMean) / 10000.0;
    if (abs(syncDeviation) > (GetDisplayCycle() / 2)) {
        m_uSyncGlitches++;
    }

    LONGLONG llJitterSum = 0;
    LONGLONG llJitterSumAvg = 0;
    for (int i = 0; i < NB_JITTER; i++) {
        LONGLONG Jitter = m_pllJitter[i];
        llJitterSum += Jitter;
        llJitterSumAvg += Jitter;
    }
    m_fJitterMean = double(llJitterSumAvg) / NB_JITTER;
    double DeviationSum = 0;

    for (int i = 0; i < NB_JITTER; i++) {
        double deviation = m_pllJitter[i] - m_fJitterMean;
        DeviationSum += deviation * deviation;
        LONGLONG deviationInt = std::llround(deviation);
        m_MaxJitter = std::max(m_MaxJitter, deviationInt);
        m_MinJitter = std::min(m_MinJitter, deviationInt);
    }

    m_fJitterStdDev = sqrt(DeviationSum / NB_JITTER);
    m_fAvrFps = 10000000.0 / (double(llJitterSum) / NB_JITTER);
    m_llLastSyncTime = syncTime;
}

// Collect the difference between periodEnd and periodStart in an array, calculate mean and stddev.
void CBaseAP::SyncOffsetStats(LONGLONG syncOffset)
{
    m_nNextSyncOffset = (m_nNextSyncOffset + 1) % NB_JITTER;
    m_pllSyncOffset[m_nNextSyncOffset] = syncOffset;

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
    m_fSyncOffsetStdDev = StdDev;
}

void CBaseAP::UpdateAlphaBitmap()
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

// Present a sample (frame) using DirectX.
STDMETHODIMP_(bool) CBaseAP::Paint(bool bAll)
{
    if (m_bPendingResetDevice) {
        SendResetRequest();
        return false;
    }

    const CRenderersSettings& r = GetRenderersSettings();
    CRenderersData* rd = GetRenderersData();
    D3DRASTER_STATUS rasterStatus;
    REFERENCE_TIME llCurRefTime = 0;
    REFERENCE_TIME llSyncOffset = 0;
    double dSyncOffset = 0.0;

    CAutoLock cRenderLock(&m_allocatorLock);

    // Estimate time for next vblank based on number of remaining lines in this frame. This algorithm seems to be
    // accurate within one ms why there should not be any need for a more accurate one. The wiggly line seen
    // when using sync to nearest and sync display is most likely due to inaccuracies in the audio-card-based
    // reference clock. The wiggles are not seen with the perfcounter-based reference clock of the sync to video option.
    m_pD3DDev->GetRasterStatus(0, &rasterStatus);
    m_uScanLineEnteringPaint = rasterStatus.ScanLine;
    if (m_pRefClock) {
        m_pRefClock->GetTime(&llCurRefTime);
    }
    int dScanLines = std::max(int(m_ScreenSize.cy - m_uScanLineEnteringPaint), 0);
    dSyncOffset = dScanLines * m_dDetectedScanlineTime; // ms
    llSyncOffset = REFERENCE_TIME(10000.0 * dSyncOffset); // Reference time units (100 ns)
    m_llEstVBlankTime = llCurRefTime + llSyncOffset; // Estimated time for the start of next vblank

    if (m_windowRect.right <= m_windowRect.left || m_windowRect.bottom <= m_windowRect.top
            || m_nativeVideoSize.cx <= 0 || m_nativeVideoSize.cy <= 0
            || !m_pVideoSurface[m_nCurSurface]) {
        return false;
    }

    HRESULT hr;
    CRect rSrcVid(CPoint(0, 0), m_nativeVideoSize);
    CRect rDstVid(m_videoRect);
    CRect rSrcPri(CPoint(0, 0), m_windowRect.Size());
    CRect rDstPri(rSrcPri);

    m_pD3DDev->BeginScene();
    CComPtr<IDirect3DSurface9> pBackBuffer;
    m_pD3DDev->GetBackBuffer(0, 0, D3DBACKBUFFER_TYPE_MONO, &pBackBuffer);
    m_pD3DDev->SetRenderTarget(0, pBackBuffer);
    hr = m_pD3DDev->Clear(0, nullptr, D3DCLEAR_TARGET, 0, 1.0f, 0);
    if (!rDstVid.IsRectEmpty()) {
        if (m_pVideoTexture[m_nCurSurface]) {
            CComPtr<IDirect3DTexture9> pVideoTexture = m_pVideoTexture[m_nCurSurface];

            if (m_pVideoTexture[m_nDXSurface] && m_pVideoTexture[m_nDXSurface + 1] && !m_pPixelShaders.IsEmpty()) {
                static __int64 counter = 0;
                static long start = clock();

                long stop = clock();
                long diff = stop - start;

                if (diff >= 10 * 60 * CLOCKS_PER_SEC) {
                    start = stop;    // reset after 10 min (ps float has its limits in both range and accuracy)
                }

                int src = m_nCurSurface, dst = m_nDXSurface;

                D3DSURFACE_DESC desc;
                m_pVideoTexture[src]->GetLevelDesc(0, &desc);

                float fConstData[][4] = {
                    {(float)desc.Width, (float)desc.Height, (float)(counter++), (float)diff / CLOCKS_PER_SEC},
                    {1.0f / desc.Width, 1.0f / desc.Height, 0, 0},
                };

                hr = m_pD3DDev->SetPixelShaderConstantF(0, (float*)fConstData, _countof(fConstData));

                CComPtr<IDirect3DSurface9> pRT;
                hr = m_pD3DDev->GetRenderTarget(0, &pRT);

                POSITION pos = m_pPixelShaders.GetHeadPosition();
                while (pos) {
                    pVideoTexture = m_pVideoTexture[dst];

                    hr = m_pD3DDev->SetRenderTarget(0, m_pVideoSurface[dst]);
                    CExternalPixelShader& Shader = m_pPixelShaders.GetNext(pos);
                    if (!Shader.m_pPixelShader) {
                        Shader.Compile(m_pPSC);
                    }
                    hr = m_pD3DDev->SetPixelShader(Shader.m_pPixelShader);
                    TextureCopy(m_pVideoTexture[src]);

                    src = dst;
                    if (++dst >= m_nDXSurface + 2) {
                        dst = m_nDXSurface;
                    }
                }

                hr = m_pD3DDev->SetRenderTarget(0, pRT);
                hr = m_pD3DDev->SetPixelShader(nullptr);
            }

            Vector dst[4];
            Transform(rDstVid, dst);

            DWORD iDX9Resizer = r.iDX9Resizer;

            float A = 0;

            switch (iDX9Resizer) {
                case 3:
                    A = -0.60f;
                    break;
                case 4:
                    A = -0.751f;
                    break;  // FIXME : 0.75 crash recent D3D, or eat CPU
                case 5:
                    A = -1.00f;
                    break;
            }
            bool bScreenSpacePixelShaders = !m_pPixelShadersScreenSpace.IsEmpty();

            hr = InitResizers(A, bScreenSpacePixelShaders);

            if (!m_pScreenSizeTemporaryTexture[0] || !m_pScreenSizeTemporaryTexture[1]) {
                bScreenSpacePixelShaders = false;
            }

            if (bScreenSpacePixelShaders) {
                CComPtr<IDirect3DSurface9> pRT;
                hr = m_pScreenSizeTemporaryTexture[1]->GetSurfaceLevel(0, &pRT);
                if (hr != S_OK) {
                    bScreenSpacePixelShaders = false;
                }
                if (bScreenSpacePixelShaders) {
                    hr = m_pD3DDev->SetRenderTarget(0, pRT);
                    if (hr != S_OK) {
                        bScreenSpacePixelShaders = false;
                    }
                    hr = m_pD3DDev->Clear(0, nullptr, D3DCLEAR_TARGET, 0, 1.0f, 0);
                }
            }

            if (rSrcVid.Size() != rDstVid.Size()) {
                if (iDX9Resizer == 0 || iDX9Resizer == 1) {
                    D3DTEXTUREFILTERTYPE Filter = iDX9Resizer == 0 ? D3DTEXF_POINT : D3DTEXF_LINEAR;
                    hr = TextureResize(pVideoTexture, dst, Filter, rSrcVid);
                } else if (iDX9Resizer == 2) {
                    hr = TextureResizeBilinear(pVideoTexture, dst, rSrcVid);
                } else if (iDX9Resizer >= 3) {
                    hr = TextureResizeBicubic2pass(pVideoTexture, dst, rSrcVid);
                }
            } else {
                hr = TextureResize(pVideoTexture, dst, D3DTEXF_POINT, rSrcVid);
            }

            if (bScreenSpacePixelShaders) {
                static __int64 counter = 555;
                static long start = clock() + 333;

                long stop = clock() + 333;
                long diff = stop - start;

                if (diff >= 10 * 60 * CLOCKS_PER_SEC) {
                    start = stop;    // reset after 10 min (ps float has its limits in both range and accuracy)
                }

                D3DSURFACE_DESC desc;
                m_pScreenSizeTemporaryTexture[0]->GetLevelDesc(0, &desc);

                float fConstData[][4] = {
                    {(float)desc.Width, (float)desc.Height, (float)(counter++), (float)diff / CLOCKS_PER_SEC},
                    {1.0f / desc.Width, 1.0f / desc.Height, 0, 0},
                };

                hr = m_pD3DDev->SetPixelShaderConstantF(0, (float*)fConstData, _countof(fConstData));

                int srcTexture = 1, dstTexture = 0;

                POSITION pos = m_pPixelShadersScreenSpace.GetHeadPosition();
                while (pos) {
                    if (m_pPixelShadersScreenSpace.GetTailPosition() == pos) {
                        m_pD3DDev->SetRenderTarget(0, pBackBuffer);
                    } else {
                        CComPtr<IDirect3DSurface9> pRT;
                        hr = m_pScreenSizeTemporaryTexture[dstTexture]->GetSurfaceLevel(0, &pRT);
                        m_pD3DDev->SetRenderTarget(0, pRT);
                    }

                    CExternalPixelShader& Shader = m_pPixelShadersScreenSpace.GetNext(pos);
                    if (!Shader.m_pPixelShader) {
                        Shader.Compile(m_pPSC);
                    }
                    hr = m_pD3DDev->SetPixelShader(Shader.m_pPixelShader);
                    TextureCopy(m_pScreenSizeTemporaryTexture[srcTexture]);

                    std::swap(srcTexture, dstTexture);
                }

                hr = m_pD3DDev->SetPixelShader(nullptr);
            }
        } else {
            if (pBackBuffer) {
                ClipToSurface(pBackBuffer, rSrcVid, rDstVid);
                // rSrcVid has to be aligned on mod2 for yuy2->rgb conversion with StretchRect
                rSrcVid.left &= ~1;
                rSrcVid.right &= ~1;
                rSrcVid.top &= ~1;
                rSrcVid.bottom &= ~1;
                hr = m_pD3DDev->StretchRect(m_pVideoSurface[m_nCurSurface], rSrcVid, pBackBuffer, rDstVid, m_filter);
                if (FAILED(hr)) {
                    return false;
                }
            }
        }
    }

    AlphaBltSubPic(rDstPri, rDstVid);

    if (m_VMR9AlphaBitmap.dwFlags & VMRBITMAP_UPDATE) {
        CAutoLock BitMapLock(&m_VMR9AlphaBitmapLock);
        CRect rcSrc(m_VMR9AlphaBitmap.rSrc);
        m_pOSDTexture = nullptr;
        m_pOSDSurface = nullptr;
        if ((m_VMR9AlphaBitmap.dwFlags & VMRBITMAP_DISABLE) == 0 && (BYTE*)m_VMR9AlphaBitmapData) {
            if ((m_pD3DXLoadSurfaceFromMemory != nullptr) &&
                    SUCCEEDED(hr = m_pD3DDev->CreateTexture(
                                       rcSrc.Width(),
                                       rcSrc.Height(),
                                       1,
                                       D3DUSAGE_RENDERTARGET,
                                       D3DFMT_A8R8G8B8,
                                       D3DPOOL_DEFAULT,
                                       &m_pOSDTexture,
                                       nullptr))) {
                if (SUCCEEDED(hr = m_pOSDTexture->GetSurfaceLevel(0, &m_pOSDSurface))) {
                    hr = m_pD3DXLoadSurfaceFromMemory(m_pOSDSurface, nullptr, nullptr, (BYTE*)m_VMR9AlphaBitmapData, D3DFMT_A8R8G8B8, m_VMR9AlphaBitmapWidthBytes,
                                                      nullptr, &m_VMR9AlphaBitmapRect, D3DX_FILTER_NONE, m_VMR9AlphaBitmap.clrSrcKey);
                }
                if (FAILED(hr)) {
                    m_pOSDTexture = nullptr;
                    m_pOSDSurface = nullptr;
                }
            }
        }
        m_VMR9AlphaBitmap.dwFlags ^= VMRBITMAP_UPDATE;
    }
    if (rd->m_iDisplayStats) {
        DrawStats();
    }
    if (m_pOSDTexture) {
        AlphaBlt(rSrcPri, rDstPri, m_pOSDTexture);
    }
    m_pD3DDev->EndScene();

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
    if (FAILED(hr)) {
        TRACE(_T("Device lost or something\n"));
    }
    // Calculate timing statistics
    if (m_pRefClock) {
        m_pRefClock->GetTime(&llCurRefTime);    // To check if we called Present too late to hit the right vsync
    }
    m_llEstVBlankTime = std::max(m_llEstVBlankTime, llCurRefTime); // Sometimes the real value is larger than the estimated value (but never smaller)
    if (rd->m_iDisplayStats < 3) {        // Partial on-screen statistics
        SyncStats(m_llEstVBlankTime);     // Max of estimate and real. Sometimes Present may actually return immediately so we need the estimate as a lower bound
    }
    if (rd->m_iDisplayStats == 1) {       // Full on-screen statistics
        SyncOffsetStats(-llSyncOffset);   // Minus because we want time to flow downward in the graph in DrawStats
    }

    // Adjust sync
    double frameCycle = (m_llSampleTime - m_llLastSampleTime) / 10000.0;
    if (frameCycle < 0) {
        frameCycle = 0.0;    // Happens when searching.
    }

    if (r.m_AdvRendSets.bSynchronizeVideo) {
        m_pGenlock->ControlClock(dSyncOffset, frameCycle);
    } else if (r.m_AdvRendSets.bSynchronizeDisplay) {
        m_pGenlock->ControlDisplay(dSyncOffset, frameCycle);
    } else {
        m_pGenlock->UpdateStats(dSyncOffset, frameCycle);    // No sync or sync to nearest neighbor
    }

    m_dFrameCycle = m_pGenlock->frameCycleAvg;
    m_dCycleDifference = GetCycleDifference();
    if (abs(m_dCycleDifference) < 0.05) { // If less than 5% speed difference
        m_bSnapToVSync = true;
    } else {
        m_bSnapToVSync = false;
    }

    // Check how well audio is matching rate (if at all)
    DWORD tmp;
    if (m_pAudioStats != nullptr) {
        m_pAudioStats->GetStatParam(AM_AUDREND_STAT_PARAM_SLAVE_ACCUMERROR, &m_lAudioLag, &tmp);
        m_lAudioLagMin = std::min((long)m_lAudioLag, m_lAudioLagMin);
        m_lAudioLagMax = std::max((long)m_lAudioLag, m_lAudioLagMax);
        m_pAudioStats->GetStatParam(AM_AUDREND_STAT_PARAM_SLAVE_MODE, &m_lAudioSlaveMode, &tmp);
    }

    if (rd->m_bResetStats) {
        ResetStats();
        rd->m_bResetStats = false;
    }

    bool fResetDevice = m_bPendingResetDevice;
    if (hr == D3DERR_DEVICELOST && m_pD3DDev->TestCooperativeLevel() == D3DERR_DEVICENOTRESET || hr == S_PRESENT_MODE_CHANGED) {
        fResetDevice = true;
    }
    if (SettingsNeedResetDevice()) {
        fResetDevice = true;
    }

    BOOL bCompositionEnabled = false;
    if (m_pDwmIsCompositionEnabled) {
        m_pDwmIsCompositionEnabled(&bCompositionEnabled);
    }
    if ((bCompositionEnabled != 0) != m_bCompositionEnabled) {
        if (m_bIsFullscreen) {
            m_bCompositionEnabled = (bCompositionEnabled != 0);
        } else {
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
            if (m_CurrentAdapter != GetAdapter(m_pD3D, m_hWnd)) {
                fResetDevice = true;
            }
#ifdef _DEBUG
            else {
                ASSERT(m_pD3D->GetAdapterMonitor(m_CurrentAdapter) == m_pD3D->GetAdapterMonitor(GetAdapter(m_pD3D, m_hWnd)));
            }
#endif
        }
    }

    if (fResetDevice) {
        m_bPendingResetDevice = true;
        SendResetRequest();
    }
    return true;
}

void CBaseAP::SendResetRequest()
{
    if (!m_bDeviceResetRequested) {
        m_bDeviceResetRequested = true;
        AfxGetApp()->m_pMainWnd->PostMessage(WM_RESET_DEVICE);
    }
}

STDMETHODIMP_(bool) CBaseAP::ResetDevice()
{
    DeleteSurfaces();
    HRESULT hr;
    CString Error;
    if (FAILED(hr = CreateDXDevice(Error)) || FAILED(hr = AllocSurfaces())) {
        m_bDeviceResetRequested = false;
        return false;
    }
    m_pGenlock->SetMonitor(GetAdapter(m_pD3D, m_hWnd));
    m_pGenlock->GetTiming();
    OnResetDevice();
    m_bDeviceResetRequested = false;
    m_bPendingResetDevice = false;
    return true;
}

STDMETHODIMP_(bool) CBaseAP::DisplayChange()
{
    m_bPendingResetDevice = true;
    SendResetRequest();
    return true;
}

void CBaseAP::InitStats()
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

void CBaseAP::DrawStats()
{
    const CRenderersData* rd = GetRenderersData();

    InitStats();
    const float textScale = float(m_windowRect.Width()) / REFERENCE_WIDTH;
    const int lineHeight = lround((FONT_HEIGHT + TEXT_PADDING) * textScale);

    // pApp->m_iDisplayStats = 1 for full stats, 2 for little less, 3 for basic, 0 for no stats
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
        CRect rc(lineHeight, lineHeight, 0, 0);

        m_pSprite->Begin(D3DXSPRITE_ALPHABLEND);
        CString strText;

        strText.Format(_T("Frames drawn from stream start: %u | Sample time stamp: %ld ms"),
                       m_pcFramesDrawn, (LONG)(m_llSampleTime / 10000));
        drawText(rc, strText);

        if (rd->m_iDisplayStats == 1) {
            strText.Format(_T("Frame cycle  : %.3f ms [%.3f ms, %.3f ms]  Actual  %+5.3f ms [%+.3f ms, %+.3f ms]"),
                           m_dFrameCycle, m_pGenlock->minFrameCycle, m_pGenlock->maxFrameCycle,
                           m_fJitterMean / 10000.0, (double(llMinJitter) / 10000.0),
                           (double(llMaxJitter) / 10000.0));
            drawText(rc, strText);

            strText.Format(_T("Display cycle: Measured closest match %.3f ms   Measured base %.3f ms"),
                           m_dOptimumDisplayCycle, m_dEstRefreshCycle);
            drawText(rc, strText);

            strText.Format(_T("Frame rate   : %.3f fps   Actual frame rate: %.3f fps"),
                           1000.0 / m_dFrameCycle, 10000000.0 / m_fJitterMean);
            drawText(rc, strText);

            strText.Format(_T("Windows      : Display cycle %.3f ms    Display refresh rate %u Hz"),
                           m_dD3DRefreshCycle, m_refreshRate);
            drawText(rc, strText);

            if (m_pGenlock->powerstripTimingExists) {
                strText.Format(_T("Powerstrip   : Display cycle %.3f ms    Display refresh rate %.3f Hz"),
                               1000.0 / m_pGenlock->curDisplayFreq, m_pGenlock->curDisplayFreq);
                drawText(rc, strText);
            }

            if ((m_caps.Caps & D3DCAPS_READ_SCANLINE) == 0) {
                strText = _T("Scan line err: Graphics device does not support scan line access. No sync is possible");
                drawText(rc, strText);
            }

#ifdef _DEBUG
            if (m_pD3DDevEx) {
                CComPtr<IDirect3DSwapChain9> pSC;
                HRESULT hr = m_pD3DDevEx->GetSwapChain(0, &pSC);
                CComQIPtr<IDirect3DSwapChain9Ex> pSCEx = pSC;
                if (pSCEx) {
                    D3DPRESENTSTATS stats;
                    hr = pSCEx->GetPresentStats(&stats);
                    if (SUCCEEDED(hr)) {
                        strText = _T("Graphics device present stats:");
                        drawText(rc, strText);

                        strText.Format(_T("    PresentCount %u PresentRefreshCount %u SyncRefreshCount %u"),
                                       stats.PresentCount, stats.PresentRefreshCount, stats.SyncRefreshCount);
                        drawText(rc, strText);

                        LARGE_INTEGER Freq;
                        QueryPerformanceFrequency(&Freq);
                        Freq.QuadPart /= 1000;
                        strText.Format(_T("    SyncQPCTime %dms SyncGPUTime %dms"),
                                       stats.SyncQPCTime.QuadPart / Freq.QuadPart,
                                       stats.SyncGPUTime.QuadPart / Freq.QuadPart);
                        drawText(rc, strText);
                    } else {
                        strText = L"Graphics device does not support present stats";
                        drawText(rc, strText);
                    }
                }
            }
#endif

            strText.Format(_T("Video size   : %ld x %ld  (AR = %ld : %ld)  Display resolution %ld x %ld "),
                           m_nativeVideoSize.cx, m_nativeVideoSize.cy, m_aspectRatio.cx, m_aspectRatio.cy,
                           m_ScreenSize.cx, m_ScreenSize.cy);
            drawText(rc, strText);

            if (r.m_AdvRendSets.bSynchronizeDisplay || r.m_AdvRendSets.bSynchronizeVideo) {
                if (r.m_AdvRendSets.bSynchronizeDisplay && !m_pGenlock->PowerstripRunning()) {
                    strText = _T("Sync error   : PowerStrip is not running. No display sync is possible.");
                    drawText(rc, strText);
                } else {
                    strText.Format(_T("Sync adjust  : %d | # of adjustments: %u"),
                                   m_pGenlock->adjDelta,
                                   (m_pGenlock->clockAdjustmentsMade + m_pGenlock->displayAdjustmentsMade) / 2);
                    drawText(rc, strText);
                }
            }
        }

        strText.Format(_T("Sync offset  : Average %3.1f ms [%.1f ms, %.1f ms]   Target %3.1f ms"),
                       m_pGenlock->syncOffsetAvg, m_pGenlock->minSyncOffset,
                       m_pGenlock->maxSyncOffset, r.m_AdvRendSets.fTargetSyncOffset);
        drawText(rc, strText);

        strText.Format(_T("Sync status  : glitches %u,  display-frame cycle mismatch: %7.3f %%,  dropped frames %u"),
                       m_uSyncGlitches, 100 * m_dCycleDifference, m_pcFramesDropped);
        drawText(rc, strText);

        if (rd->m_iDisplayStats == 1) {
            if (m_pAudioStats && r.m_AdvRendSets.bSynchronizeVideo) {
                strText.Format(_T("Audio lag   : %3lu ms [%ld ms, %ld ms] | %s"),
                               m_lAudioLag, m_lAudioLagMin, m_lAudioLagMax,
                               (m_lAudioSlaveMode == 4) ?
                               _T("Audio renderer is matching rate (for analog sound output)") :
                               _T("Audio renderer is not matching rate"));
                drawText(rc, strText);
            }

            strText.Format(_T("Sample time  : waiting %3ld ms"), m_lNextSampleWait);
            if (r.m_AdvRendSets.bSynchronizeNearest) {
                CString temp;
                temp.Format(_T("  paint time correction: %3ld ms  Hysteresis: %I64d"),
                            m_lShiftToNearest, m_llHysteresis / 10000);
                strText += temp;
            }
            drawText(rc, strText);

            strText.Format(_T("Buffering    : Buffered %3ld    Free %3ld    Current Surface %3d"),
                           m_nUsedBuffer, m_nDXSurface - m_nUsedBuffer, m_nCurSurface);
            drawText(rc, strText);

            strText = _T("Settings     : ");

            if (m_bIsFullscreen) {
                strText += _T("D3DFS ");
            }
            if (r.m_AdvRendSets.bVMRDisableDesktopComposition) {
                strText += _T("DisDC ");
            }
            if (r.m_AdvRendSets.bSynchronizeVideo) {
                strText += _T("SyncVideo ");
            }
            if (r.m_AdvRendSets.bSynchronizeDisplay) {
                strText += _T("SyncDisplay ");
            }
            if (r.m_AdvRendSets.bSynchronizeNearest) {
                strText += _T("SyncNearest ");
            }
            if (m_bHighColorResolution) {
                strText += _T("10 bit ");
            }
            if (r.m_AdvRendSets.iEVROutputRange == 0) {
                strText += _T("0-255 ");
            } else if (r.m_AdvRendSets.iEVROutputRange == 1) {
                strText += _T("16-235 ");
            }

            drawText(rc, strText);
            drawText(rc, rd->m_strDXVAInfo);

            strText.Format(L"DirectX SDK  : %u", rd->GetDXSdkRelease());
            drawText(rc, strText);

            for (int i = 0; i < 6; i++) {
                if (m_strStatsMsg[i][0]) {
                    drawText(rc, m_strStatsMsg[i]);
                }
            }
        }
        m_pSprite->End();
    }

    if (m_pLine && (rd->m_iDisplayStats < 3)) {
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

        if (rd->m_iDisplayStats == 1) { // Full on-screen statistics
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
        m_pLine->End();
    }
}

double CBaseAP::GetRefreshRate()
{
    if (m_pGenlock->powerstripTimingExists) {
        return m_pGenlock->curDisplayFreq;
    } else {
        return (double)m_refreshRate;
    }
}

double CBaseAP::GetDisplayCycle()
{
    if (m_pGenlock->powerstripTimingExists) {
        return 1000.0 / m_pGenlock->curDisplayFreq;
    } else {
        return (double)m_dD3DRefreshCycle;
    }
}

double CBaseAP::GetCycleDifference()
{
    double dBaseDisplayCycle = GetDisplayCycle();
    double minDiff = 1.0;
    if (dBaseDisplayCycle == 0.0 || m_dFrameCycle == 0.0) {
        return 1.0;
    } else {
        for (UINT i = 1; i <= 8; i++) { // Try a lot of multiples of the display frequency
            double dDisplayCycle = i * dBaseDisplayCycle;
            double diff = (dDisplayCycle - m_dFrameCycle) / m_dFrameCycle;
            if (abs(diff) < abs(minDiff)) {
                minDiff = diff;
                m_dOptimumDisplayCycle = dDisplayCycle;
            }
        }
    }
    return minDiff;
}

void CBaseAP::EstimateRefreshTimings()
{
    if (m_pD3DDev) {
        const CRenderersData* rd = GetRenderersData();
        D3DRASTER_STATUS rasterStatus;
        m_pD3DDev->GetRasterStatus(0, &rasterStatus);
        while (rasterStatus.ScanLine != 0) {
            m_pD3DDev->GetRasterStatus(0, &rasterStatus);
        }
        while (rasterStatus.ScanLine == 0) {
            m_pD3DDev->GetRasterStatus(0, &rasterStatus);
        }
        m_pD3DDev->GetRasterStatus(0, &rasterStatus);
        LONGLONG startTime = rd->GetPerfCounter();
        UINT startLine = rasterStatus.ScanLine;
        LONGLONG endTime = 0;
        UINT endLine = 0;
        bool done = false;
        while (!done) { // Estimate time for one scan line
            m_pD3DDev->GetRasterStatus(0, &rasterStatus);
            UINT line = rasterStatus.ScanLine;
            LONGLONG time = rd->GetPerfCounter();
            if (line > 0) {
                endLine = line;
                endTime = time;
            } else {
                done = true;
            }
        }
        m_dDetectedScanlineTime = (endTime - startTime) / ((endLine - startLine) * 10000.0);

        // Estimate the display refresh rate from the vsyncs
        m_pD3DDev->GetRasterStatus(0, &rasterStatus);
        while (rasterStatus.ScanLine != 0) {
            m_pD3DDev->GetRasterStatus(0, &rasterStatus);
        }
        // Now we're at the start of a vsync
        startTime = rd->GetPerfCounter();
        UINT i;
        for (i = 1; i <= 50; i++) {
            m_pD3DDev->GetRasterStatus(0, &rasterStatus);
            while (rasterStatus.ScanLine == 0) {
                m_pD3DDev->GetRasterStatus(0, &rasterStatus);
            }
            while (rasterStatus.ScanLine != 0) {
                m_pD3DDev->GetRasterStatus(0, &rasterStatus);
            }
            // Now we're at the next vsync
        }
        endTime = rd->GetPerfCounter();
        m_dEstRefreshCycle = (endTime - startTime) / ((i - 1) * 10000.0);
    }
}

bool CBaseAP::ExtractInterlaced(const AM_MEDIA_TYPE* pmt)
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

HRESULT CBaseAP::Resize(IDirect3DTexture9* pTexture, const CRect& srcRect, const CRect& destRect) {
    HRESULT hr = E_FAIL;

    const CRenderersSettings& r = GetRenderersSettings();

    DWORD iDX9Resizer = r.iDX9Resizer;
    Vector dst[4];
    Transform(destRect, dst);

    if (iDX9Resizer == 0 || iDX9Resizer == 1) {
        D3DTEXTUREFILTERTYPE Filter = iDX9Resizer == 0 ? D3DTEXF_POINT : D3DTEXF_LINEAR;
        hr = TextureResize(pTexture, dst, Filter, srcRect);
    } else if (iDX9Resizer == 2) {
        hr = TextureResizeBilinear(pTexture, dst, srcRect);
    } else if (iDX9Resizer >= 3) {
        hr = TextureResizeBicubic1pass(pTexture, dst, srcRect);
    }

    return hr;
}

STDMETHODIMP CBaseAP::GetDIB(BYTE* lpDib, DWORD* size)
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

STDMETHODIMP CBaseAP::SetPixelShader(LPCSTR pSrcData, LPCSTR pTarget)
{
    return SetPixelShader2(pSrcData, pTarget, false);
}

STDMETHODIMP CBaseAP::SetPixelShader2(LPCSTR pSrcData, LPCSTR pTarget, bool bScreenSpace)
{
    CAutoLock cRenderLock(&m_allocatorLock);

    CAtlList<CExternalPixelShader>* pPixelShaders;
    if (bScreenSpace) {
        pPixelShaders = &m_pPixelShadersScreenSpace;
    } else {
        pPixelShaders = &m_pPixelShaders;
    }

    if (!pSrcData && !pTarget) {
        pPixelShaders->RemoveAll();
        m_pD3DDev->SetPixelShader(nullptr);
        return S_OK;
    }

    if (!pSrcData) {
        return E_INVALIDARG;
    }

    CExternalPixelShader Shader;
    Shader.m_SourceData = pSrcData;
    Shader.m_SourceTarget = pTarget;

    CComPtr<IDirect3DPixelShader9> pPixelShader;

    HRESULT hr = Shader.Compile(m_pPSC);
    if (FAILED(hr)) {
        return hr;
    }

    pPixelShaders->AddTail(Shader);
    Paint(true);
    return S_OK;
}

CSyncAP::CSyncAP(HWND hWnd, bool bFullscreen, HRESULT& hr, CString& _Error)
    : CBaseAP(hWnd, bFullscreen, hr, _Error)
    , m_LastClockState(MFCLOCK_STATE_INVALID)
    , m_dwVideoAspectRatioMode(MFVideoARMode_PreservePicture)
    , m_dwVideoRenderPrefs((MFVideoRenderPrefs)0)
    , m_BorderColor(RGB(0, 0, 0))
    , m_hEvtQuit(nullptr)
    , m_bEvtQuit(0)
    , m_hEvtFlush(nullptr)
    , m_bEvtFlush(0)
    , m_hEvtSkip(nullptr)
    , m_bEvtSkip(false)
    , m_bUseInternalTimer(false)
    , m_LastSetOutputRange(-1)
    , m_bPendingRenegotiate(false)
    , m_bPendingMediaFinished(false)
    , m_bPrerolled(false)
    , m_hRenderThread(nullptr)
    , m_hMixerThread(nullptr)
    , m_nRenderState(Shutdown)
    , m_bStepping(false)
    , m_nCurrentGroupId(0)
    , m_nResetToken(0)
    , m_nStepCount(0)
    , m_SampleFreeCallback(this, &CSyncAP::OnSampleFree)
    , fnDXVA2CreateDirect3DDeviceManager9(_T("dxva2.dll"), "DXVA2CreateDirect3DDeviceManager9")
    , fnMFCreateDXSurfaceBuffer(_T("evr.dll"), "MFCreateDXSurfaceBuffer")
    , fnMFCreateVideoSampleFromSurface(_T("evr.dll"), "MFCreateVideoSampleFromSurface")
    , fnMFCreateMediaType(_T("mfplat.dll"), "MFCreateMediaType")
    , fnAvSetMmThreadCharacteristicsW(_T("avrt.dll"), "AvSetMmThreadCharacteristicsW")
    , fnAvSetMmThreadPriority(_T("avrt.dll"), "AvSetMmThreadPriority")
    , fnAvRevertMmThreadCharacteristics(_T("avrt.dll"), "AvRevertMmThreadCharacteristics")
{
    const CRenderersSettings& r = GetRenderersSettings();

    if (FAILED(hr)) {
        _Error += L"SyncAP failed\n";
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

    // Bufferize frame only with 3D texture
    if (r.iAPSurfaceUsage == VIDRNDT_AP_TEXTURE3D) {
        m_nDXSurface = std::max(std::min(r.iEvrBuffers, MAX_PICTURE_SLOTS - 2), 4);
    } else {
        m_nDXSurface = 1;
    }

    m_pOuterEVR = nullptr;
    m_lShiftToNearest = -1; // Illegal value to start with
}

CSyncAP::~CSyncAP()
{
    StopWorkerThreads();
    m_pMediaType = nullptr;
    m_pClock = nullptr;
    m_pD3DManager = nullptr;

    if (m_bHookedNewSegment) {
        UnhookNewSegment();
    }
}

HRESULT CSyncAP::CheckShutdown() const
{
    if (m_nRenderState == Shutdown) {
        return MF_E_SHUTDOWN;
    } else {
        return S_OK;
    }
}

void CSyncAP::StartWorkerThreads()
{
    DWORD dwThreadId;
    if (m_nRenderState == Shutdown) {
        m_hEvtQuit = CreateEvent(nullptr, TRUE, FALSE, nullptr);
        m_hEvtFlush = CreateEvent(nullptr, TRUE, FALSE, nullptr);
        m_hEvtSkip = CreateEvent(nullptr, TRUE, FALSE, nullptr);
        m_hMixerThread = ::CreateThread(nullptr, 0, MixerThreadStatic, (LPVOID)this, 0, &dwThreadId);
        SetThreadPriority(m_hMixerThread, THREAD_PRIORITY_HIGHEST);
        m_hRenderThread = ::CreateThread(nullptr, 0, RenderThreadStatic, (LPVOID)this, 0, &dwThreadId);
        SetThreadPriority(m_hRenderThread, THREAD_PRIORITY_TIME_CRITICAL);
        m_nRenderState = Stopped;
    }
}

void CSyncAP::StopWorkerThreads()
{
    if (m_nRenderState != Shutdown) {
        SetEvent(m_hEvtFlush);
        m_bEvtFlush = true;
        SetEvent(m_hEvtQuit);
        m_bEvtQuit = true;
        SetEvent(m_hEvtSkip);
        m_bEvtSkip = true;

        if (m_hRenderThread && WaitForSingleObject(m_hRenderThread, 10000) == WAIT_TIMEOUT) {
            ASSERT(FALSE);
            TerminateThread(m_hRenderThread, 0xDEAD);
        }

        SAFE_CLOSE_HANDLE(m_hRenderThread);

        if (m_hMixerThread && WaitForSingleObject(m_hMixerThread, 10000) == WAIT_TIMEOUT) {
            ASSERT(FALSE);
            TerminateThread(m_hMixerThread, 0xDEAD);
        }

        SAFE_CLOSE_HANDLE(m_hMixerThread);
        SAFE_CLOSE_HANDLE(m_hEvtFlush);
        SAFE_CLOSE_HANDLE(m_hEvtQuit);
        SAFE_CLOSE_HANDLE(m_hEvtSkip);

        m_bEvtFlush = false;
        m_bEvtQuit = false;
        m_bEvtSkip = false;
    }
    m_nRenderState = Shutdown;
}

STDMETHODIMP CSyncAP::CreateRenderer(IUnknown** ppRenderer)
{
    CheckPointer(ppRenderer, E_POINTER);
    *ppRenderer = nullptr;
    HRESULT hr = E_FAIL;

    do {
        CMacrovisionKicker* pMK = DEBUG_NEW CMacrovisionKicker(NAME("CMacrovisionKicker"), nullptr);
        CComPtr<IUnknown> pUnk = (IUnknown*)(INonDelegatingUnknown*)pMK;

        CSyncRenderer* pOuterEVR = DEBUG_NEW CSyncRenderer(NAME("CSyncRenderer"), pUnk, hr, &m_VMR9AlphaBitmap, this);
        m_pOuterEVR = pOuterEVR;

        pMK->SetInner((IUnknown*)(INonDelegatingUnknown*)pOuterEVR);
        CComQIPtr<IBaseFilter> pBF = pUnk;

        if (FAILED(hr)) {
            break;
        }

        // Set EVR custom presenter
        CComPtr<IMFVideoPresenter> pVP;
        CComPtr<IMFVideoRenderer> pMFVR;
        CComQIPtr<IMFGetService, &__uuidof(IMFGetService)> pMFGS = pBF;
        CComQIPtr<IEVRFilterConfig> pConfig = pBF;
        pConfig->SetNumberOfStreams(3);

        hr = pMFGS->GetService(MR_VIDEO_RENDER_SERVICE, IID_PPV_ARGS(&pMFVR));

        if (SUCCEEDED(hr)) {
            hr = QueryInterface(IID_PPV_ARGS(&pVP));
        }
        if (SUCCEEDED(hr)) {
            hr = pMFVR->InitializeRenderer(nullptr, pVP);
        }

        if (SUCCEEDED(hr)) {
            CComPtr<IPin> pPin = GetFirstPin(pBF);
            if (HookNewSegment((IPinC*)(IPin*)pPin)) {
                m_bUseInternalTimer = true;
                m_bHookedNewSegment = true;
            }
            *ppRenderer = pBF.Detach();
        } else {
            *ppRenderer = nullptr;
        }
    } while (0);

    return hr;
}

STDMETHODIMP_(bool) CSyncAP::Paint(bool bAll)
{
    return __super::Paint(bAll);
}

STDMETHODIMP_(bool) CSyncAP::Paint(IMFSample* pMFSample)
{
    m_pCurrentlyDisplayedSample = pMFSample;
    pMFSample->GetUINT32(GUID_SURFACE_INDEX, (UINT32*)&m_nCurSurface);

    auto sampleHasCurrentGroupId = [this](IMFSample * pSample) {
        UINT32 nGroupId;
        return (SUCCEEDED(pSample->GetUINT32(GUID_GROUP_ID, &nGroupId)) && nGroupId == m_nCurrentGroupId);
    };
    ASSERT(sampleHasCurrentGroupId(pMFSample));

    return Paint(true);
}

STDMETHODIMP CSyncAP::NonDelegatingQueryInterface(REFIID riid, void** ppv)
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
    } else if (riid == __uuidof(IEVRTrustedVideoPlugin)) {
        hr = GetInterface((IEVRTrustedVideoPlugin*)this, ppv);
    } else if (riid == IID_IQualProp) {
        hr = GetInterface((IQualProp*)this, ppv);
    } else if (riid == __uuidof(IMFRateSupport)) {
        hr = GetInterface((IMFRateSupport*)this, ppv);
    } else if (riid == __uuidof(IDirect3DDeviceManager9)) {
        hr = m_pD3DManager->QueryInterface(__uuidof(IDirect3DDeviceManager9), (void**) ppv);
    } else if (riid == __uuidof(ISyncClockAdviser)) {
        hr = GetInterface((ISyncClockAdviser*)this, ppv);
    } else if (riid == __uuidof(ID3DFullscreenControl)) {
        hr = GetInterface((ID3DFullscreenControl*)this, ppv);
    } else {
        hr = __super::NonDelegatingQueryInterface(riid, ppv);
    }

    return hr;
}

// IMFClockStateSink
STDMETHODIMP CSyncAP::OnClockStart(MFTIME hnsSystemTime, LONGLONG llClockStartOffset)
{
    HRESULT hr;
    CHECK_HR(CheckShutdown());
    m_nRenderState = Started;
    return S_OK;
}

STDMETHODIMP CSyncAP::OnClockStop(MFTIME hnsSystemTime)
{
    HRESULT hr;
    CHECK_HR(CheckShutdown());
    m_nRenderState = Stopped;
    return S_OK;
}

STDMETHODIMP CSyncAP::OnClockPause(MFTIME hnsSystemTime)
{
    HRESULT hr;
    CHECK_HR(CheckShutdown());
    m_nRenderState = Paused;
    return S_OK;
}

STDMETHODIMP CSyncAP::OnClockRestart(MFTIME hnsSystemTime)
{
    HRESULT hr;
    CHECK_HR(CheckShutdown());
    m_nRenderState  = Started;
    return S_OK;
}

STDMETHODIMP CSyncAP::OnClockSetRate(MFTIME hnsSystemTime, float flRate)
{
    return E_NOTIMPL;
}

// IBaseFilter delegate
bool CSyncAP::GetState(DWORD dwMilliSecsTimeout, FILTER_STATE* State, HRESULT& _ReturnValue)
{
    CAutoLock lock(&m_SampleQueueLock);
    switch (m_nRenderState) {
        case Started:
            *State = State_Running;
            break;
        case Paused:
            *State = State_Paused;
            break;
        case Stopped:
            *State = State_Stopped;
            break;
        default:
            *State = State_Stopped;
            _ReturnValue = E_FAIL;
    }
    _ReturnValue = S_OK;
    return true;
}

// IQualProp
STDMETHODIMP CSyncAP::get_FramesDroppedInRenderer(int* pcFrames)
{
    *pcFrames = m_pcFramesDropped;
    return S_OK;
}

STDMETHODIMP CSyncAP::get_FramesDrawn(int* pcFramesDrawn)
{
    *pcFramesDrawn = m_pcFramesDrawn;
    return S_OK;
}

STDMETHODIMP CSyncAP::get_AvgFrameRate(int* piAvgFrameRate)
{
    *piAvgFrameRate = (int)(m_fAvrFps * 100);
    return S_OK;
}

STDMETHODIMP CSyncAP::get_Jitter(int* iJitter)
{
    *iJitter = (int)((m_fJitterStdDev / 10000.0) + 0.5);
    return S_OK;
}

STDMETHODIMP CSyncAP::get_AvgSyncOffset(int* piAvg)
{
    *piAvg = (int)((m_fSyncOffsetAvr / 10000.0) + 0.5);
    return S_OK;
}

STDMETHODIMP CSyncAP::get_DevSyncOffset(int* piDev)
{
    *piDev = (int)((m_fSyncOffsetStdDev / 10000.0) + 0.5);
    return S_OK;
}

// IMFRateSupport
STDMETHODIMP CSyncAP::GetSlowestRate(MFRATE_DIRECTION eDirection, BOOL fThin, float* pflRate)
{
    *pflRate = 0;
    return S_OK;
}

STDMETHODIMP CSyncAP::GetFastestRate(MFRATE_DIRECTION eDirection, BOOL fThin, float* pflRate)
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

STDMETHODIMP CSyncAP::IsRateSupported(BOOL fThin, float flRate, float* pflNearestSupportedRate)
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

float CSyncAP::GetMaxRate(BOOL bThin)
{
    float fMaxRate = FLT_MAX;  // Default.
    UINT32 fpsNumerator = 0, fpsDenominator = 0;

    if (!bThin && m_pMediaType) {
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

void CSyncAP::CompleteFrameStep(bool bCancel)
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
STDMETHODIMP CSyncAP::ProcessMessage(MFVP_MESSAGE_TYPE eMessage, ULONG_PTR ulParam)
{
    HRESULT hr = S_OK;
    CHECK_HR(CheckShutdown());

    switch (eMessage) {
        case MFVP_MESSAGE_BEGINSTREAMING:
            hr = BeginStreaming();
            m_llHysteresis = 0;
            m_lShiftToNearest = 0;
            m_bStepping = false;
            break;

        case MFVP_MESSAGE_CANCELSTEP:
            m_bStepping = false;
            CompleteFrameStep(true);
            break;

        case MFVP_MESSAGE_ENDOFSTREAM:
            m_bPendingMediaFinished = true;
            break;

        case MFVP_MESSAGE_ENDSTREAMING:
            m_pGenlock->ResetTiming();
            m_pRefClock = nullptr;
            m_nRenderState = Stopped;
            break;

        case MFVP_MESSAGE_FLUSH:
            SetEvent(m_hEvtFlush);
            m_bEvtFlush = true;
            while (WaitForSingleObject(m_hEvtFlush, 1) == WAIT_OBJECT_0) {
                ;
            }
            break;

        case MFVP_MESSAGE_INVALIDATEMEDIATYPE:
            m_bPendingRenegotiate = true;
            while (m_bPendingRenegotiate) {
                Sleep(1);
            }
            break;

        case MFVP_MESSAGE_PROCESSINPUTNOTIFY:
            break;

        case MFVP_MESSAGE_STEP:
            m_nStepCount = (int)ulParam;
            m_bStepping = true;
            break;

        default:
            ASSERT(FALSE);
            break;
    }
    return hr;
}

HRESULT CSyncAP::IsMediaTypeSupported(IMFMediaType* pMixerType)
{
    HRESULT hr;
    AM_MEDIA_TYPE* pAMMedia;
    UINT nInterlaceMode;

    CHECK_HR(pMixerType->GetRepresentation(FORMAT_VideoInfo2, (void**)&pAMMedia));
    CHECK_HR(pMixerType->GetUINT32(MF_MT_INTERLACE_MODE, &nInterlaceMode));

    if ((pAMMedia->majortype != MEDIATYPE_Video)) {
        hr = MF_E_INVALIDMEDIATYPE;
    }
    pMixerType->FreeRepresentation(FORMAT_VideoInfo2, (void*)pAMMedia);
    return hr;
}

HRESULT CSyncAP::CreateOptimalOutputType(IMFMediaType* pMixerProposedType, IMFMediaType* pMixerInputType, IMFMediaType** ppType)
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
            TRACE(_T("Copying color attribute %s failed: 0x%08x\n"), static_cast<LPCTSTR>(CComBSTR(guidKey)), hr);
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
    MFVideoArea Area = GetArea(0, 0, videoSize.cx, videoSize.cy);
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

HRESULT CSyncAP::SetMediaType(IMFMediaType* pType)
{
    HRESULT hr = S_OK;
    AM_MEDIA_TYPE* pAMMedia = nullptr;
    CString strTemp;

    CHECK_HR(CheckShutdown());

    if (pType == nullptr) {
        // Release
        RemoveAllSamples();
        DeleteSurfaces();
        CAutoLock lock(this);
        m_pMediaType = nullptr;
        return hr;
    }

    DWORD dwFlags = 0;
    if (m_pMediaType && m_pMediaType->IsEqual(pType, &dwFlags) == S_OK) {
        // Nothing to do
        return hr;
    }

    CHECK_HR(pType->GetRepresentation(FORMAT_VideoInfo2, (void**)&pAMMedia));

    hr = InitializeDevice(pAMMedia);
    if (SUCCEEDED(hr)) {
        CAutoLock lock(this);
        m_pMediaType = pType;

        strTemp = GetMediaTypeName(pAMMedia->subtype);
        strTemp.Replace(L"MEDIASUBTYPE_", L"");
        m_strStatsMsg[MSG_MIXEROUT].Format(L"Mixer output : %s", strTemp.GetString());
    }

    pType->FreeRepresentation(FORMAT_VideoInfo2, (void*)pAMMedia);

    return hr;
}

LONGLONG CSyncAP::GetMediaTypeMerit(IMFMediaType* pMediaType)
{
    AM_MEDIA_TYPE* pAMMedia = nullptr;
    MFVIDEOFORMAT* VideoFormat;

    HRESULT hr;
    CHECK_HR(pMediaType->GetRepresentation(FORMAT_MFVideoFormat, (void**)&pAMMedia));
    VideoFormat = (MFVIDEOFORMAT*)pAMMedia->pbFormat;

    LONGLONG Merit = 0;
    switch (VideoFormat->surfaceInfo.Format) {
        case FCC('NV12'):
            Merit = 90000000;
            break;
        case FCC('YV12'):
            Merit = 80000000;
            break;
        case FCC('YUY2'):
            Merit = 70000000;
            break;
        case FCC('UYVY'):
            Merit = 60000000;
            break;

        case D3DFMT_X8R8G8B8: // Never opt for RGB
        case D3DFMT_A8R8G8B8:
        case D3DFMT_R8G8B8:
        case D3DFMT_R5G6B5:
            Merit = 0;
            break;
        default:
            Merit = 1000;
            break;
    }
    pMediaType->FreeRepresentation(FORMAT_MFVideoFormat, (void*)pAMMedia);
    return Merit;
}

HRESULT CSyncAP::RenegotiateMediaType()
{
    HRESULT hr = S_OK;

    CComPtr<IMFMediaType> pMixerType;
    CComPtr<IMFMediaType> pMixerInputType;
    CComPtr<IMFMediaType> pType;

    if (!m_pMixer) {
        return MF_E_INVALIDREQUEST;
    }

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

    CInterfaceArray<IMFMediaType> ValidMixerTypes;
    // Loop through all of the mixer's proposed output types.
    DWORD iTypeIndex = 0;
    while ((hr != MF_E_NO_MORE_TYPES)) {
        pMixerType  = nullptr;
        pType = nullptr;

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
        if (SUCCEEDED(hr)) {
            LONGLONG Merit = GetMediaTypeMerit(pType);

            size_t nTypes = ValidMixerTypes.GetCount();
            size_t iInsertPos = 0;
            for (size_t i = 0; i < nTypes; ++i) {
                LONGLONG ThisMerit = GetMediaTypeMerit(ValidMixerTypes[i]);
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
    for (size_t i = 0; i < nValidTypes; ++i) {
        pType = ValidMixerTypes[i];
    }

    for (size_t i = 0; i < nValidTypes; ++i) {
        pType = ValidMixerTypes[i];
        hr = SetMediaType(pType);
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

bool CSyncAP::GetSampleFromMixer()
{
    MFT_OUTPUT_DATA_BUFFER dataBuffer;
    HRESULT hr = S_OK;
    DWORD dwStatus;
    LONGLONG llClockBefore = 0;
    LONGLONG llClockAfter  = 0;
    LONGLONG llMixerLatency;

    UINT dwSurface;
    bool newSample = false;

    auto sampleHasCurrentGroupId = [this](IMFSample * pSample) {
        UINT32 nGroupId;
        return (SUCCEEDED(pSample->GetUINT32(GUID_GROUP_ID, &nGroupId)) && nGroupId == m_nCurrentGroupId);
    };

    while (SUCCEEDED(hr)) { // Get as many frames as there are and that we have samples for
        CComPtr<IMFSample> pSample;
        if (FAILED(GetFreeSample(&pSample))) { // All samples are taken for the moment. Better luck next time
            break;
        }

        ZeroMemory(&dataBuffer, sizeof(dataBuffer));
        dataBuffer.pSample = pSample;
        pSample->GetUINT32(GUID_SURFACE_INDEX, &dwSurface);
        ASSERT(sampleHasCurrentGroupId(pSample));

        {
            llClockBefore = GetRenderersData()->GetPerfCounter();
            hr = m_pMixer->ProcessOutput(0, 1, &dataBuffer, &dwStatus);
            llClockAfter = GetRenderersData()->GetPerfCounter();
        }

        if (hr == MF_E_TRANSFORM_NEED_MORE_INPUT) { // There are no samples left in the mixer
            AddToFreeList(pSample, false);
            pSample = nullptr; // The sample should not be used after being queued
            // Important: Release any events returned from the ProcessOutput method.
            SAFE_RELEASE(dataBuffer.pEvents);
            break;
        }

        if (m_pSink) {
            llMixerLatency = llClockAfter - llClockBefore;
            m_pSink->Notify(EC_PROCESSING_LATENCY, (LONG_PTR)&llMixerLatency, 0);
        }

        newSample = true;

        if (GetRenderersData()->m_bTearingTest) {
            RECT rcTearing;

            rcTearing.left = m_nTearingPos;
            rcTearing.top = 0;
            rcTearing.right = rcTearing.left + 4;
            rcTearing.bottom = m_nativeVideoSize.cy;
            m_pD3DDev->ColorFill(m_pVideoSurface[dwSurface], &rcTearing, D3DCOLOR_ARGB(255, 255, 0, 0));

            rcTearing.left = (rcTearing.right + 15) % m_nativeVideoSize.cx;
            rcTearing.right = rcTearing.left + 4;
            m_pD3DDev->ColorFill(m_pVideoSurface[dwSurface], &rcTearing, D3DCOLOR_ARGB(255, 255, 0, 0));
            m_nTearingPos = (m_nTearingPos + 7) % m_nativeVideoSize.cx;
        }

        if (SUCCEEDED(TrackSample(pSample))) {
            AddToScheduledList(pSample, false); // Schedule, then go back to see if there is more where that came from
            pSample = nullptr; // The sample should not be used after being queued
        } else {
            ASSERT(FALSE);
        }

        // Important: Release any events returned from the ProcessOutput method.
        SAFE_RELEASE(dataBuffer.pEvents);
    }
    return newSample;
}

STDMETHODIMP CSyncAP::GetCurrentMediaType(__deref_out  IMFVideoMediaType** ppMediaType)
{
    HRESULT hr = S_OK;
    CAutoLock lock(this);
    CheckPointer(ppMediaType, E_POINTER);
    CHECK_HR(CheckShutdown());

    if (!m_pMediaType) {
        return MF_E_NOT_INITIALIZED;
    }

    CHECK_HR(m_pMediaType->QueryInterface(IID_PPV_ARGS(ppMediaType)));
    return hr;
}

// IMFTopologyServiceLookupClient
STDMETHODIMP CSyncAP::InitServicePointers(__in IMFTopologyServiceLookup* pLookup)
{
    DWORD dwObjects = 1;
    pLookup->LookupService(MF_SERVICE_LOOKUP_GLOBAL, 0, MR_VIDEO_MIXER_SERVICE, IID_PPV_ARGS(&m_pMixer), &dwObjects);
    pLookup->LookupService(MF_SERVICE_LOOKUP_GLOBAL, 0, MR_VIDEO_RENDER_SERVICE, IID_PPV_ARGS(&m_pSink), &dwObjects);
    pLookup->LookupService(MF_SERVICE_LOOKUP_GLOBAL, 0, MR_VIDEO_RENDER_SERVICE, IID_PPV_ARGS(&m_pClock), &dwObjects);
    StartWorkerThreads();
    return S_OK;
}

STDMETHODIMP CSyncAP::ReleaseServicePointers()
{
    StopWorkerThreads();
    m_pMixer = nullptr;
    m_pSink = nullptr;
    m_pClock = nullptr;
    return S_OK;
}

// IMFVideoDeviceID
STDMETHODIMP CSyncAP::GetDeviceID(__out  IID* pDeviceID)
{
    CheckPointer(pDeviceID, E_POINTER);
    *pDeviceID = IID_IDirect3DDevice9;
    return S_OK;
}

// IMFGetService
STDMETHODIMP CSyncAP::GetService(__RPC__in REFGUID guidService, __RPC__in REFIID riid, __RPC__deref_out_opt LPVOID* ppvObject)
{
    if (guidService == MR_VIDEO_RENDER_SERVICE) {
        return NonDelegatingQueryInterface(riid, ppvObject);
    } else if (guidService == MR_VIDEO_ACCELERATION_SERVICE) {
        return m_pD3DManager->QueryInterface(__uuidof(IDirect3DDeviceManager9), (void**) ppvObject);
    }

    return E_NOINTERFACE;
}

// IMFAsyncCallback
STDMETHODIMP CSyncAP::GetParameters(__RPC__out DWORD* pdwFlags, __RPC__out DWORD* pdwQueue)
{
    return E_NOTIMPL;
}

STDMETHODIMP CSyncAP::Invoke(__RPC__in_opt IMFAsyncResult* pAsyncResult)
{
    return E_NOTIMPL;
}

// IMFVideoDisplayControl
STDMETHODIMP CSyncAP::GetNativeVideoSize(SIZE* pszVideo, SIZE* pszARVideo)
{
    if (pszVideo) {
        pszVideo->cx    = m_nativeVideoSize.cx;
        pszVideo->cy    = m_nativeVideoSize.cy;
    }
    if (pszARVideo) {
        pszARVideo->cx  = m_aspectRatio.cx;
        pszARVideo->cy  = m_aspectRatio.cy;
    }
    return S_OK;
}

STDMETHODIMP CSyncAP::GetIdealVideoSize(SIZE* pszMin, SIZE* pszMax)
{
    if (pszMin) {
        pszMin->cx = 1;
        pszMin->cy = 1;
    }

    if (pszMax) {
        D3DDISPLAYMODE d3ddm;
        ZeroMemory(&d3ddm, sizeof(d3ddm));

        if (SUCCEEDED(m_pD3D->GetAdapterDisplayMode(GetAdapter(m_pD3D, m_hWnd), &d3ddm))) {
            pszMax->cx = d3ddm.Width;
            pszMax->cy = d3ddm.Height;
        }
    }
    return S_OK;
}

STDMETHODIMP CSyncAP::SetVideoPosition(const MFVideoNormalizedRect* pnrcSource, const LPRECT prcDest)
{
    return S_OK;
}

STDMETHODIMP CSyncAP::GetVideoPosition(MFVideoNormalizedRect* pnrcSource, LPRECT prcDest)
{
    if (pnrcSource) {
        pnrcSource->left    = 0.0;
        pnrcSource->top     = 0.0;
        pnrcSource->right   = 1.0;
        pnrcSource->bottom  = 1.0;
    }
    if (prcDest) {
        memcpy(prcDest, &m_videoRect, sizeof(m_videoRect));     //GetClientRect (m_hWnd, prcDest);
    }
    return S_OK;
}

STDMETHODIMP CSyncAP::SetAspectRatioMode(DWORD dwAspectRatioMode)
{
    m_dwVideoAspectRatioMode = (MFVideoAspectRatioMode)dwAspectRatioMode;
    return S_OK;
}

STDMETHODIMP CSyncAP::GetAspectRatioMode(DWORD* pdwAspectRatioMode)
{
    CheckPointer(pdwAspectRatioMode, E_POINTER);
    *pdwAspectRatioMode = m_dwVideoAspectRatioMode;
    return S_OK;
}

STDMETHODIMP CSyncAP::SetVideoWindow(HWND hwndVideo)
{
    if (m_hWnd != hwndVideo) {
        CAutoLock lock(this);
        CAutoLock lock2(&m_ImageProcessingLock);
        CAutoLock cRenderLock(&m_allocatorLock);

        m_hWnd = hwndVideo;
        m_bPendingResetDevice = true;
        if (m_pSink) {
            m_pSink->Notify(EC_DISPLAY_CHANGED, 0, 0);
        }

        SendResetRequest();
    }
    return S_OK;
}

STDMETHODIMP CSyncAP::GetVideoWindow(HWND* phwndVideo)
{
    CheckPointer(phwndVideo, E_POINTER);
    *phwndVideo = m_hWnd;
    return S_OK;
}

STDMETHODIMP CSyncAP::RepaintVideo()
{
    Paint(true);
    return S_OK;
}

STDMETHODIMP CSyncAP::GetCurrentImage(BITMAPINFOHEADER* pBih, BYTE** pDib, DWORD* pcbDib, LONGLONG* pTimeStamp) {
    if (!pBih || !pDib || !pcbDib) {
        return E_POINTER;
    }
    CheckPointer(m_pD3DDevEx, E_ABORT);

    HRESULT hr = S_OK;
    const unsigned width = m_windowRect.Width();
    const unsigned height = m_windowRect.Height();
    const unsigned len = width * height * 4;

    memset(pBih, 0, sizeof(BITMAPINFOHEADER));
    pBih->biSize = sizeof(BITMAPINFOHEADER);
    pBih->biWidth = width;
    pBih->biHeight = height;
    pBih->biBitCount = 32;
    pBih->biPlanes = 1;
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
        TRACE_SR(L"CSyncAP::GetCurrentImage failed : %s", S_OK == hr ? L"S_OK" : Error.GetBuffer());
        CoTaskMemFree(p);
        return hr;
    }

    RetrieveBitmapData(width, height, 32, p ? (BYTE*)p : (BYTE*)(pBih + 1), (BYTE*)r.pBits, r.Pitch);

    pDestSurface->UnlockRect();

    *pDib = p;
    *pcbDib = len;

    return S_OK;
}

STDMETHODIMP CSyncAP::SetBorderColor(COLORREF Clr)
{
    m_BorderColor = Clr;
    return S_OK;
}

STDMETHODIMP CSyncAP::GetBorderColor(COLORREF* pClr)
{
    CheckPointer(pClr, E_POINTER);
    *pClr = m_BorderColor;
    return S_OK;
}

STDMETHODIMP CSyncAP::SetRenderingPrefs(DWORD dwRenderFlags)
{
    m_dwVideoRenderPrefs = (MFVideoRenderPrefs)dwRenderFlags;
    return S_OK;
}

STDMETHODIMP CSyncAP::GetRenderingPrefs(DWORD* pdwRenderFlags)
{
    CheckPointer(pdwRenderFlags, E_POINTER);
    *pdwRenderFlags = m_dwVideoRenderPrefs;
    return S_OK;
}

STDMETHODIMP CSyncAP::SetFullscreen(BOOL fFullscreen)
{
    m_bIsFullscreen = !!fFullscreen;
    return S_OK;
}

STDMETHODIMP CSyncAP::GetFullscreen(BOOL* pfFullscreen)
{
    CheckPointer(pfFullscreen, E_POINTER);
    *pfFullscreen = m_bIsFullscreen;
    return S_OK;
}

// IEVRTrustedVideoPlugin
STDMETHODIMP CSyncAP::IsInTrustedVideoMode(BOOL* pYes)
{
    CheckPointer(pYes, E_POINTER);
    *pYes = TRUE;
    return S_OK;
}

STDMETHODIMP CSyncAP::CanConstrict(BOOL* pYes)
{
    CheckPointer(pYes, E_POINTER);
    *pYes = TRUE;
    return S_OK;
}

STDMETHODIMP CSyncAP::SetConstriction(DWORD dwKPix)
{
    return S_OK;
}

STDMETHODIMP CSyncAP::DisableImageExport(BOOL bDisable)
{
    return S_OK;
}

// IDirect3DDeviceManager9
STDMETHODIMP CSyncAP::ResetDevice(IDirect3DDevice9* pDevice, UINT resetToken)
{
    HRESULT hr = m_pD3DManager->ResetDevice(pDevice, resetToken);
    return hr;
}

STDMETHODIMP CSyncAP::OpenDeviceHandle(HANDLE* phDevice)
{
    HRESULT hr = m_pD3DManager->OpenDeviceHandle(phDevice);
    return hr;
}

STDMETHODIMP CSyncAP::CloseDeviceHandle(HANDLE hDevice)
{
    HRESULT hr = m_pD3DManager->CloseDeviceHandle(hDevice);
    return hr;
}

STDMETHODIMP CSyncAP::TestDevice(HANDLE hDevice)
{
    HRESULT hr = m_pD3DManager->TestDevice(hDevice);
    return hr;
}

STDMETHODIMP CSyncAP::LockDevice(HANDLE hDevice, IDirect3DDevice9** ppDevice, BOOL fBlock)
{
    HRESULT hr = m_pD3DManager->LockDevice(hDevice, ppDevice, fBlock);
    return hr;
}

STDMETHODIMP CSyncAP::UnlockDevice(HANDLE hDevice, BOOL fSaveState)
{
    HRESULT hr = m_pD3DManager->UnlockDevice(hDevice, fSaveState);
    return hr;
}

STDMETHODIMP CSyncAP::GetVideoService(HANDLE hDevice, REFIID riid, void** ppService)
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

STDMETHODIMP CSyncAP::GetNativeVideoSize(LONG* lpWidth, LONG* lpHeight, LONG* lpARWidth, LONG* lpARHeight)
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
        *lpARWidth  = m_aspectRatio.cx;
    }
    if (lpARHeight) {
        *lpARHeight = m_aspectRatio.cy;
    }
    return S_OK;
}

STDMETHODIMP CSyncAP::InitializeDevice(AM_MEDIA_TYPE* pMediaType)
{
    HRESULT hr;
    CAutoLock lock(this);
    CAutoLock lock2(&m_ImageProcessingLock);
    CAutoLock cRenderLock(&m_allocatorLock);

    RemoveAllSamples();
    DeleteSurfaces();

    VIDEOINFOHEADER2* vih2 = (VIDEOINFOHEADER2*) pMediaType->pbFormat;
    int w = vih2->bmiHeader.biWidth;
    int h = abs(vih2->bmiHeader.biHeight);

    SetVideoSize(CSize(w, h), m_aspectRatio);
    if (m_bHighColorResolution) {
        hr = AllocSurfaces(D3DFMT_A2R10G10B10);
    } else {
        hr = AllocSurfaces(D3DFMT_X8R8G8B8);
    }

    for (int i = 0; i < m_nDXSurface; i++) {
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
    return hr;
}

DWORD WINAPI CSyncAP::MixerThreadStatic(LPVOID lpParam)
{
    CSyncAP* pThis = (CSyncAP*) lpParam;
    pThis->MixerThread();
    return 0;
}

void CSyncAP::MixerThread()
{
    HANDLE hEvts[] = {m_hEvtQuit};
    bool bQuit = false;
    TIMECAPS tc;
    DWORD dwResolution;

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
                bool bNewSample;
                {
                    CAutoLock AutoLock(&m_ImageProcessingLock);
                    bNewSample = GetSampleFromMixer();
                }

                if (m_rtTimePerFrame == 0 && bNewSample) {
                    // Use the code from VMR9 to get the movie fps, as this method is reliable.
                    CComPtr<IPin> pPin;
                    CMediaType    mt;
                    if (SUCCEEDED(m_pOuterEVR->FindPin(L"EVR Input0", &pPin)) &&
                            SUCCEEDED(pPin->ConnectionMediaType(&mt))) {
                        ExtractAvgTimePerFrame(&mt, m_rtTimePerFrame);

                        m_bInterlaced = ExtractInterlaced(&mt);

                        if (m_rtTimePerFrame > 0) {
                            m_fps = 10000000.0 / m_rtTimePerFrame;
                        }
                    }

                    // Update internal subtitle clock
                    if (m_bUseInternalTimer && m_pSubPicQueue) {
                        m_pSubPicQueue->SetFPS(m_fps);
                    }
                }
            }
            break;
        }
    }
    timeEndPeriod(dwResolution);
}

DWORD WINAPI CSyncAP::RenderThreadStatic(LPVOID lpParam)
{
    CSyncAP* pThis = (CSyncAP*)lpParam;
    pThis->RenderThread();
    return 0;
}

// Get samples that have been received and queued up by MixerThread() and present them at the correct time by calling Paint().
void CSyncAP::RenderThread()
{
    HANDLE hEvts[] = {m_hEvtQuit, m_hEvtFlush, m_hEvtSkip};
    bool bQuit = false;
    TIMECAPS tc;
    CComPtr<IMFSample> pNewSample; // The sample next in line to be presented

    // Tell Multimedia Class Scheduler we are doing threaded playback (increase priority)
    HANDLE hAvrt = 0;
    if (fnAvSetMmThreadCharacteristicsW) {
        DWORD dwTaskIndex = 0;
        hAvrt = fnAvSetMmThreadCharacteristicsW(L"Playback", &dwTaskIndex);
        if (fnAvSetMmThreadPriority) {
            fnAvSetMmThreadPriority(hAvrt, AVRT_PRIORITY_HIGH);
        }
    }

    // Set timer resolution
    timeGetDevCaps(&tc, sizeof(TIMECAPS));
    DWORD dwResolution = std::min(std::max(tc.wPeriodMin, 0u), tc.wPeriodMax);
    VERIFY(timeBeginPeriod(dwResolution) == 0);

    auto SubPicSetTime = [&] {
        if (!g_bExternalSubtitleTime) {
            CSubPicAllocatorPresenterImpl::SetTime(g_tSegmentStart + m_llSampleTime * (g_bExternalSubtitle ? g_dRate : 1));
        }
    };

    auto checkPendingMediaFinished = [this]() {
        if (m_bPendingMediaFinished) {
            CAutoLock lock(&m_SampleQueueLock);
            if (m_ScheduledSamples.IsEmpty()) {
                m_bPendingMediaFinished = false;
                m_pSink->Notify(EC_COMPLETE, 0, 0);
            }
        }
    };

    while (!bQuit) {
        m_lNextSampleWait = 1; // Default value for running this loop
        int nSamplesLeft = 0;
        bool stepForward = false;
        LONG lDisplayCycle  = (LONG)(GetDisplayCycle());
        LONG lDisplayCycle2 = (LONG)(GetDisplayCycle() / 2.0); // These are a couple of empirically determined constants used the control the "snap" function
        LONG lDisplayCycle4 = (LONG)(GetDisplayCycle() / 4.0);

        const CRenderersSettings& r = GetRenderersSettings();
        double dTargetSyncOffset = (&r == nullptr) ? 12.0 : r.m_AdvRendSets.fTargetSyncOffset;

        if ((m_nRenderState == Started || !m_bPrerolled) && !pNewSample) {  // If either streaming or the pre-roll sample and no sample yet fetched
            if (SUCCEEDED(GetScheduledSample(&pNewSample, nSamplesLeft))) { // Get the next sample
                m_llLastSampleTime = m_llSampleTime;
                if (!m_bPrerolled) {
                    m_bPrerolled = true;    // m_bPrerolled is a ticket to show one (1) frame immediately
                    m_lNextSampleWait = 0;  // Present immediately
                } else if (SUCCEEDED(pNewSample->GetSampleTime(&m_llSampleTime))) { // Get zero-based sample due time
                    if (m_llLastSampleTime == m_llSampleTime) { // In the rare case there are duplicate frames in the movie. There really shouldn't be but it happens.
                        checkPendingMediaFinished();
                        pNewSample = nullptr;
                        m_lNextSampleWait = 0;
                    } else {
                        MFTIME llSystemTime;
                        LONGLONG llRefClockTime;
                        m_pClock->GetCorrelatedTime(0, &llRefClockTime, &llSystemTime); // Get zero-based reference clock time. llSystemTime is not used for anything here
                        m_lNextSampleWait = (LONG)((m_llSampleTime - llRefClockTime) / 10000); // Time left until sample is due, in ms
                        if (m_bStepping) {
                            m_lNextSampleWait = 0;
                        } else if (r.m_AdvRendSets.bSynchronizeNearest) { // Present at the closest "safe" occasion at dTargetSyncOffset ms before vsync to avoid tearing
                            if (m_lNextSampleWait < -lDisplayCycle) { // We have to allow slightly negative numbers at this stage. Otherwise we get "choking" when frame rate > refresh rate
                                SetEvent(m_hEvtSkip);
                                m_bEvtSkip = true;
                            }
                            REFERENCE_TIME rtRefClockTimeNow = 0;
                            if (m_pRefClock) {
                                m_pRefClock->GetTime(&rtRefClockTimeNow);    // Reference clock time now
                            }
                            LONG lLastVsyncTime = (LONG)((m_llEstVBlankTime - rtRefClockTimeNow) / 10000); // Last vsync time relative to now
                            if (abs(lLastVsyncTime) > lDisplayCycle) {
                                lLastVsyncTime = - lDisplayCycle;    // To even out glitches in the beginning
                            }

                            LONGLONG llNextSampleWait = (LONGLONG)((lLastVsyncTime + GetDisplayCycle() - dTargetSyncOffset) * 10000); // Time from now util next safe time to Paint()
                            while ((llRefClockTime + llNextSampleWait) < (m_llSampleTime + m_llHysteresis)) { // While the proposed time is in the past of sample presentation time
                                llNextSampleWait = llNextSampleWait + (LONGLONG)(GetDisplayCycle() * 10000); // Try the next possible time, one display cycle ahead
                            }
                            m_lNextSampleWait = (LONG)(llNextSampleWait / 10000);
                            m_lShiftToNearestPrev = m_lShiftToNearest;
                            m_lShiftToNearest = (LONG)((llRefClockTime + llNextSampleWait - m_llSampleTime) / 10000); // The adjustment made to get to the sweet point in time, in ms

                            // If m_lShiftToNearest is pushed a whole cycle into the future, then we are getting more frames
                            // than we can chew and we need to throw one away. We don't want to wait many cycles and skip many
                            // frames.
                            if (m_lShiftToNearest > (lDisplayCycle + 1)) {
                                SetEvent(m_hEvtSkip);
                                m_bEvtSkip = true;
                            }

                            // We need to add a hysteresis to the control of the timing adjustment to avoid judder when
                            // presentation time is close to vsync and the renderer couldn't otherwise make up its mind
                            // whether to present before the vsync or after. That kind of indecisiveness leads to judder.
                            if (m_bSnapToVSync) {

                                if ((m_lShiftToNearestPrev - m_lShiftToNearest) > lDisplayCycle2) { // If a step down in the m_lShiftToNearest function. Display slower than video.
                                    m_bVideoSlowerThanDisplay = false;
                                    m_llHysteresis = -(LONGLONG)lDisplayCycle4 * 10000;
                                } else if ((m_lShiftToNearest - m_lShiftToNearestPrev) > lDisplayCycle2) { // If a step up
                                    m_bVideoSlowerThanDisplay = true;
                                    m_llHysteresis = (LONGLONG)lDisplayCycle4 * 10000;
                                } else if ((m_lShiftToNearest < (3 * lDisplayCycle4)) && (m_lShiftToNearest > lDisplayCycle4)) {
                                    m_llHysteresis = 0;    // Reset when between 1/4 and 3/4 of the way either way
                                }

                                if ((m_lShiftToNearest < lDisplayCycle2) && (m_llHysteresis > 0)) {
                                    m_llHysteresis = 0;    // Should never really be in this territory.
                                }
                                if (m_lShiftToNearest < 0) {
                                    m_llHysteresis = 0;    // A glitch might get us to a sticky state where both these numbers are negative.
                                }
                                if ((m_lShiftToNearest > lDisplayCycle2) && (m_llHysteresis < 0)) {
                                    m_llHysteresis = 0;
                                }
                            }
                        }

                        if (m_lNextSampleWait < 0) { // Skip late or duplicate sample.
                            SetEvent(m_hEvtSkip);
                            m_bEvtSkip = true;
                        }

                        if (m_lNextSampleWait > 1000) {
                            m_lNextSampleWait = 1000; // So as to avoid full a full stop when sample is far in the future (shouldn't really happen).
                        }
                    }
                } // if got new sample
            } else {
                checkPendingMediaFinished();
            }
        }
        // Wait for the next presentation time (m_lNextSampleWait) or some other event.
        DWORD dwObject = WaitForMultipleObjects(_countof(hEvts), hEvts, FALSE, (DWORD)m_lNextSampleWait);
        switch (dwObject) {
            case WAIT_OBJECT_0: // Quit
                bQuit = true;
                break;

            case WAIT_OBJECT_0 + 1: // Flush
                checkPendingMediaFinished();
                pNewSample = nullptr;
                FlushSamples();
                m_bEvtFlush = false;
                ResetEvent(m_hEvtFlush);
                m_bPrerolled = false;
                m_lShiftToNearest = 0;
                stepForward = true;
                break;

            case WAIT_OBJECT_0 + 2: // Skip sample
                m_pcFramesDropped++;
                m_llSampleTime = m_llLastSampleTime; // This sample will never be shown
                m_bEvtSkip = false;
                ResetEvent(m_hEvtSkip);
                stepForward = true;
                break;

            case WAIT_TIMEOUT: // Time to show the sample or something
                if (m_LastSetOutputRange != -1 && m_LastSetOutputRange != r.m_AdvRendSets.iEVROutputRange || m_bPendingRenegotiate) {
                    checkPendingMediaFinished();
                    pNewSample = nullptr;
                    FlushSamples();
                    RenegotiateMediaType();
                    m_bPendingRenegotiate = false;
                }

                if (m_bPendingResetDevice) {
                    checkPendingMediaFinished();
                    pNewSample = nullptr;
                    SendResetRequest();
                } else if (m_nStepCount < 0) {
                    m_nStepCount = 0;
                    m_pcFramesDropped++;
                    stepForward = true;
                } else if (pNewSample && (m_nStepCount > 0)) {
                    SubPicSetTime();
                    Paint(pNewSample);
                    CompleteFrameStep(false);
                    m_pcFramesDrawn++;
                    stepForward = true;
                } else if (pNewSample && !m_bStepping) { // When a stepped frame is shown, a new one is fetched that we don't want to show here while stepping
                    SubPicSetTime();
                    Paint(pNewSample);
                    m_pcFramesDrawn++;
                    stepForward = true;
                }
                break;
        } // switch
        if (stepForward) {
            checkPendingMediaFinished();
            pNewSample = nullptr;
        }
    } // while
    pNewSample = nullptr;
    timeEndPeriod(dwResolution);
    if (fnAvRevertMmThreadCharacteristics) {
        fnAvRevertMmThreadCharacteristics(hAvrt);
    }
}

STDMETHODIMP_(bool) CSyncAP::ResetDevice()
{
    CAutoLock lock(this);
    CAutoLock lock2(&m_ImageProcessingLock);
    CAutoLock cRenderLock(&m_allocatorLock);

    RemoveAllSamples();

    bool bResult = __super::ResetDevice();

    for (int i = 0; i < m_nDXSurface; i++) {
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
    return bResult;
}

void CSyncAP::OnResetDevice()
{
    TRACE(_T("--> CSyncAP::OnResetDevice on thread: %lu\n"), GetCurrentThreadId());
    m_pD3DManager->ResetDevice(m_pD3DDev, m_nResetToken);
    if (m_pSink) {
        m_pSink->Notify(EC_DISPLAY_CHANGED, 0, 0);
    }
    CSize videoSize = GetVisibleVideoSize();
    if (m_pSink) {
        m_pSink->Notify(EC_VIDEO_SIZE_CHANGED, MAKELPARAM(videoSize.cx, videoSize.cy), 0);
    }
}

void CSyncAP::RemoveAllSamples()
{
    CAutoLock imageProcesssingLock(&m_ImageProcessingLock);
    CAutoLock sampleQueueLock(&m_SampleQueueLock);

    FlushSamples();
    m_ScheduledSamples.RemoveAll();
    m_FreeSamples.RemoveAll();
    m_nUsedBuffer = 0;
    // Increment the group id to make sure old samples will really be deleted
    m_nCurrentGroupId++;
}

HRESULT CSyncAP::GetFreeSample(IMFSample** ppSample)
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

HRESULT CSyncAP::GetScheduledSample(IMFSample** ppSample, int& count)
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

void CSyncAP::AddToFreeList(IMFSample* pSample, bool bTail)
{
    CAutoLock lock(&m_SampleQueueLock);

    m_nUsedBuffer--;
    if (bTail) {
        m_FreeSamples.AddTail(pSample);
    } else {
        m_FreeSamples.AddHead(pSample);
    }
}

void CSyncAP::AddToScheduledList(IMFSample* pSample, bool bSorted)
{
    CAutoLock lock(&m_SampleQueueLock);

    if (bSorted) {
        m_ScheduledSamples.AddHead(pSample);
    } else {
        m_ScheduledSamples.AddTail(pSample);
    }
}

void CSyncAP::FlushSamples()
{
    CAutoLock lock(this);
    CAutoLock lock2(&m_SampleQueueLock);

    m_bPrerolled = false;
    m_pCurrentlyDisplayedSample = nullptr;
    m_ScheduledSamples.RemoveAll();
}

HRESULT CSyncAP::TrackSample(IMFSample* pSample)
{
    HRESULT hr = E_FAIL;
    if (CComQIPtr<IMFTrackedSample> pTracked = pSample) {
        hr = pTracked->SetAllocator(&m_SampleFreeCallback, nullptr);
    }
    return hr;
}

HRESULT CSyncAP::OnSampleFree(IMFAsyncResult* pResult)
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

HRESULT CSyncAP::AdviseSyncClock(ISyncClock* sC)
{
    return m_pGenlock->AdviseSyncClock(sC);
}

HRESULT CSyncAP::BeginStreaming()
{
    m_pcFramesDropped = 0;
    m_pcFramesDrawn = 0;

    CComPtr<IBaseFilter> pEVR;
    CFilterInfo filterInfo;
    m_pOuterEVR->QueryInterface(IID_PPV_ARGS(&pEVR));
    pEVR->QueryFilterInfo(&filterInfo);

    BeginEnumFilters(filterInfo.pGraph, pEF, pBF);
    if (CComQIPtr<IAMAudioRendererStats> pAS = pBF) {
        m_pAudioStats = pAS;
    };
    EndEnumFilters;

    pEVR->GetSyncSource(&m_pRefClock);
    m_pGenlock->SetMonitor(GetAdapter(m_pD3D, m_hWnd));
    m_pGenlock->GetTiming();

    ResetStats();
    EstimateRefreshTimings();
    if (m_dFrameCycle > 0.0) {
        m_dCycleDifference = GetCycleDifference();    // Might have moved to another display
    }

    m_nRenderState = Paused;

    return S_OK;
}

HRESULT CreateSyncRenderer(const CLSID& clsid, HWND hWnd, bool bFullscreen, ISubPicAllocatorPresenter** ppAP)
{
    HRESULT hr = E_FAIL;
    if (clsid == CLSID_SyncAllocatorPresenter) {
        CString Error;
        *ppAP = DEBUG_NEW CSyncAP(hWnd, bFullscreen, hr, Error);
        (*ppAP)->AddRef();

        if (FAILED(hr)) {
            Error += L"\n";
            Error += GetWindowsErrorMessage(hr, nullptr);
            MessageBox(hWnd, Error, L"Error creating EVR Sync", MB_OK | MB_ICONERROR);
            (*ppAP)->Release();
            *ppAP = nullptr;
        } else if (!Error.IsEmpty()) {
            MessageBox(hWnd, Error, L"Warning creating EVR Sync", MB_OK | MB_ICONWARNING);
        }
    }
    return hr;
}

CSyncRenderer::CSyncRenderer(const TCHAR* pName, LPUNKNOWN pUnk, HRESULT& hr, VMR9AlphaBitmap* pVMR9AlphaBitmap, CSyncAP* pAllocatorPresenter): CUnknown(pName, pUnk)
{
    hr = m_pEVR.CoCreateInstance(CLSID_EnhancedVideoRenderer, GetOwner());
    CComQIPtr<IBaseFilter> pEVRBase = m_pEVR;
    m_pEVRBase = pEVRBase; // Don't keep a second reference on the EVR filter
    m_pVMR9AlphaBitmap = pVMR9AlphaBitmap;
    m_pAllocatorPresenter = pAllocatorPresenter;
}

CSyncRenderer::~CSyncRenderer()
{
}

HRESULT STDMETHODCALLTYPE CSyncRenderer::GetState(DWORD dwMilliSecsTimeout, __out  FILTER_STATE* State)
{
    if (m_pEVRBase) {
        return m_pEVRBase->GetState(dwMilliSecsTimeout, State);
    }
    return E_NOTIMPL;
}

STDMETHODIMP CSyncRenderer::EnumPins(__out IEnumPins** ppEnum)
{
    if (m_pEVRBase) {
        return m_pEVRBase->EnumPins(ppEnum);
    }
    return E_NOTIMPL;
}

STDMETHODIMP CSyncRenderer::FindPin(LPCWSTR Id, __out  IPin** ppPin)
{
    if (m_pEVRBase) {
        return m_pEVRBase->FindPin(Id, ppPin);
    }
    return E_NOTIMPL;
}

STDMETHODIMP CSyncRenderer::QueryFilterInfo(__out  FILTER_INFO* pInfo)
{
    if (m_pEVRBase) {
        return m_pEVRBase->QueryFilterInfo(pInfo);
    }
    return E_NOTIMPL;
}

STDMETHODIMP CSyncRenderer::JoinFilterGraph(__in_opt  IFilterGraph* pGraph, __in_opt  LPCWSTR pName)
{
    if (m_pEVRBase) {
        return m_pEVRBase->JoinFilterGraph(pGraph, pName);
    }
    return E_NOTIMPL;
}

STDMETHODIMP CSyncRenderer::QueryVendorInfo(__out  LPWSTR* pVendorInfo)
{
    if (m_pEVRBase) {
        return m_pEVRBase->QueryVendorInfo(pVendorInfo);
    }
    return E_NOTIMPL;
}

STDMETHODIMP CSyncRenderer::Stop()
{
    if (m_pEVRBase) {
        return m_pEVRBase->Stop();
    }
    return E_NOTIMPL;
}

STDMETHODIMP CSyncRenderer::Pause()
{
    if (m_pEVRBase) {
        return m_pEVRBase->Pause();
    }
    return E_NOTIMPL;
}

STDMETHODIMP CSyncRenderer::Run(REFERENCE_TIME tStart)
{
    if (m_pEVRBase) {
        return m_pEVRBase->Run(tStart);
    }
    return E_NOTIMPL;
}

STDMETHODIMP CSyncRenderer::SetSyncSource(__in_opt IReferenceClock* pClock)
{
    if (m_pEVRBase) {
        return m_pEVRBase->SetSyncSource(pClock);
    }
    return E_NOTIMPL;
}

STDMETHODIMP CSyncRenderer::GetSyncSource(__deref_out_opt IReferenceClock** pClock)
{
    if (m_pEVRBase) {
        return m_pEVRBase->GetSyncSource(pClock);
    }
    return E_NOTIMPL;
}

STDMETHODIMP CSyncRenderer::GetClassID(__RPC__out CLSID* pClassID)
{
    if (m_pEVRBase) {
        return m_pEVRBase->GetClassID(pClassID);
    }
    return E_NOTIMPL;
}

STDMETHODIMP CSyncRenderer::GetAlphaBitmapParameters(VMR9AlphaBitmap* pBmpParms)
{
    CheckPointer(pBmpParms, E_POINTER);
    CAutoLock BitMapLock(&m_pAllocatorPresenter->m_VMR9AlphaBitmapLock);
    memcpy(pBmpParms, m_pVMR9AlphaBitmap, sizeof(VMR9AlphaBitmap));
    return S_OK;
}

STDMETHODIMP CSyncRenderer::SetAlphaBitmap(const VMR9AlphaBitmap*  pBmpParms)
{
    CheckPointer(pBmpParms, E_POINTER);
    CAutoLock BitMapLock(&m_pAllocatorPresenter->m_VMR9AlphaBitmapLock);
    memcpy(m_pVMR9AlphaBitmap, pBmpParms, sizeof(VMR9AlphaBitmap));
    m_pVMR9AlphaBitmap->dwFlags |= VMRBITMAP_UPDATE;
    m_pAllocatorPresenter->UpdateAlphaBitmap();
    return S_OK;
}

STDMETHODIMP CSyncRenderer::UpdateAlphaBitmapParameters(const VMR9AlphaBitmap* pBmpParms)
{
    CheckPointer(pBmpParms, E_POINTER);
    CAutoLock BitMapLock(&m_pAllocatorPresenter->m_VMR9AlphaBitmapLock);
    memcpy(m_pVMR9AlphaBitmap, pBmpParms, sizeof(VMR9AlphaBitmap));
    m_pVMR9AlphaBitmap->dwFlags |= VMRBITMAP_UPDATE;
    m_pAllocatorPresenter->UpdateAlphaBitmap();
    return S_OK;
}

STDMETHODIMP CSyncRenderer::NonDelegatingQueryInterface(REFIID riid, void** ppv)
{
    HRESULT hr;

    if (riid == __uuidof(IVMRMixerBitmap9)) {
        return GetInterface((IVMRMixerBitmap9*)this, ppv);
    }

    if (riid == __uuidof(IBaseFilter)) {
        return GetInterface((IBaseFilter*)this, ppv);
    }

    if (riid == __uuidof(IMediaFilter)) {
        return GetInterface((IMediaFilter*)this, ppv);
    }
    if (riid == __uuidof(IPersist)) {
        return GetInterface((IPersist*)this, ppv);
    }
    if (riid == __uuidof(IBaseFilter)) {
        return GetInterface((IBaseFilter*)this, ppv);
    }

    hr = m_pEVR ? m_pEVR->QueryInterface(riid, ppv) : E_NOINTERFACE;
    if (m_pEVR && FAILED(hr)) {
        hr = m_pAllocatorPresenter ? m_pAllocatorPresenter->QueryInterface(riid, ppv) : E_NOINTERFACE;
    }
    return SUCCEEDED(hr) ? hr : __super::NonDelegatingQueryInterface(riid, ppv);
}

CGenlock::CGenlock(double target, double limit, int lineD, int colD, double clockD, UINT mon)
    : powerstripTimingExists(false)
    , liveSource(false)
    , adjDelta(0)               // Number of rows used in display frequency adjustment, typically 1 (one)
    , lineDelta(lineD)          // Number of columns used in display frequency adjustment, typically 1 - 2
    , columnDelta(colD)         // Delta used in clock speed adjustment. In fractions of 1.0. Typically around 0.001
    , cycleDelta(clockD)        // The monitor to be adjusted if the display refresh rate is the controlled parameter
    , displayAdjustmentsMade(0)
    , clockAdjustmentsMade(0)
    , totalLines(0)
    , totalColumns(0)
    , visibleLines(0)
    , visibleColumns(0)
    , syncOffsetFifo(64)
    , frameCycleFifo(4)
    , minSyncOffset(DBL_MAX)
    , maxSyncOffset(DBL_MIN)
    , syncOffsetAvg(0.0)
    , minFrameCycle(DBL_MAX)
    , maxFrameCycle(DBL_MIN)
    , frameCycleAvg(0.0)
    , pixelClock(0)
    , displayFreqCruise(0.0)
    , displayFreqSlower(0.0)
    , displayFreqFaster(0.0)
    , curDisplayFreq(0.0)
    , controlLimit(limit)       // How much sync offset is allowed to drift from target sync offset before control kicks in
    , monitor(mon)
    , psWnd(nullptr)
    , displayTiming()
    , displayTimingSave()
    , lowSyncOffset(target - limit)
    , targetSyncOffset(target)  // Target sync offset, typically around 10 ms
    , highSyncOffset(target + limit)
{
    ZeroMemory(faster, MAX_LOADSTRING);
    ZeroMemory(cruise, MAX_LOADSTRING);
    ZeroMemory(slower, MAX_LOADSTRING);
    ZeroMemory(savedTiming, MAX_LOADSTRING);
}

CGenlock::~CGenlock()
{
    ResetTiming();
    syncClock = nullptr;
};

BOOL CGenlock::PowerstripRunning()
{
    psWnd = FindWindow(_T("TPShidden"), nullptr);
    if (!psWnd) {
        return FALSE;    // Powerstrip is not running
    } else {
        return TRUE;
    }
}

// Get the display timing parameters through PowerStrip (if running).
HRESULT CGenlock::GetTiming()
{
    ATOM getTiming;
    LPARAM lParam = 0;
    WPARAM wParam = monitor;
    int i = 0;
    int j = 0;
    int params = 0;
    TCHAR tmpStr[MAX_LOADSTRING] = _T("");

    CAutoLock lock(&csGenlockLock);
    if (!PowerstripRunning()) {
        return E_FAIL;
    }

    getTiming = static_cast<ATOM>(SendMessage(psWnd, UM_GETTIMING, wParam, lParam));
    GlobalGetAtomName(getTiming, savedTiming, MAX_LOADSTRING);

    while (params < TIMING_PARAM_CNT) {
        while (savedTiming[i] != _T(',') && savedTiming[i] != _T('\0')) {
            tmpStr[j++] = savedTiming[i];
            tmpStr[j] = _T('\0');
            i++;
        }
        i++; // Skip trailing comma
        j = 0;
        displayTiming[params] = _ttoi(tmpStr);
        displayTimingSave[params] = displayTiming[params];
        params++;
    }

    // The display update frequency is controlled by adding and subtracting pixels form the
    // image. This is done by either subtracting columns or rows or both. Some displays like
    // row adjustments and some column adjustments. One should probably not do both.
    StringCchPrintf(faster, MAX_LOADSTRING, _T("%u,%u,%u,%u,%u,%u,%u,%u,%u,%u\0"),
                    displayTiming[0],
                    displayTiming[HFRONTPORCH] - columnDelta,
                    displayTiming[2],
                    displayTiming[3],
                    displayTiming[4],
                    displayTiming[VFRONTPORCH] - lineDelta,
                    displayTiming[6],
                    displayTiming[7],
                    displayTiming[PIXELCLOCK],
                    displayTiming[9]
                   );

    // Nominal update frequency
    StringCchPrintf(cruise, MAX_LOADSTRING, _T("%u,%u,%u,%u,%u,%u,%u,%u,%u,%u\0"),
                    displayTiming[0],
                    displayTiming[HFRONTPORCH],
                    displayTiming[2],
                    displayTiming[3],
                    displayTiming[4],
                    displayTiming[VFRONTPORCH],
                    displayTiming[6],
                    displayTiming[7],
                    displayTiming[PIXELCLOCK],
                    displayTiming[9]
                   );

    // Lower than nominal update frequency
    StringCchPrintf(slower, MAX_LOADSTRING, _T("%u,%u,%u,%u,%u,%u,%u,%u,%u,%u\0"),
                    displayTiming[0],
                    displayTiming[HFRONTPORCH] + columnDelta,
                    displayTiming[2],
                    displayTiming[3],
                    displayTiming[4],
                    displayTiming[VFRONTPORCH] + lineDelta,
                    displayTiming[6],
                    displayTiming[7],
                    displayTiming[PIXELCLOCK],
                    displayTiming[9]
                   );

    totalColumns = displayTiming[HACTIVE] + displayTiming[HFRONTPORCH] + displayTiming[HSYNCWIDTH] + displayTiming[HBACKPORCH];
    totalLines = displayTiming[VACTIVE] + displayTiming[VFRONTPORCH] + displayTiming[VSYNCWIDTH] + displayTiming[VBACKPORCH];
    pixelClock = 1000 * displayTiming[PIXELCLOCK]; // Pixels/s
    displayFreqCruise = (double)pixelClock / (totalLines * totalColumns); // Frames/s
    displayFreqSlower = (double)pixelClock / ((totalLines + lineDelta) * (totalColumns + columnDelta));
    displayFreqFaster = (double)pixelClock / ((totalLines - lineDelta) * (totalColumns - columnDelta));
    curDisplayFreq = displayFreqCruise;
    GlobalDeleteAtom(getTiming);
    adjDelta = 0;
    powerstripTimingExists = true;
    return S_OK;
}

// Reset display timing parameters to nominal.
HRESULT CGenlock::ResetTiming()
{
    CAutoLock lock(&csGenlockLock);

    if (!PowerstripRunning()) {
        return E_FAIL;
    }

    if (displayAdjustmentsMade > 0) {
        ATOM setTiming = GlobalAddAtom(cruise);
        SendMessage(psWnd, UM_SETCUSTOMTIMINGFAST, monitor, (LPARAM)setTiming);
        GlobalDeleteAtom(setTiming);
        curDisplayFreq = displayFreqCruise;
    }
    adjDelta = 0;
    return S_OK;
}

// Reset reference clock speed to nominal.
HRESULT CGenlock::ResetClock()
{
    adjDelta = 0;
    if (syncClock == nullptr) {
        return E_FAIL;
    } else {
        return syncClock->AdjustClock(1.0);
    }
}

HRESULT CGenlock::SetTargetSyncOffset(double targetD)
{
    targetSyncOffset = targetD;
    lowSyncOffset = targetD - controlLimit;
    highSyncOffset = targetD + controlLimit;
    return S_OK;
}

HRESULT CGenlock::GetTargetSyncOffset(double* targetD)
{
    *targetD = targetSyncOffset;
    return S_OK;
}

HRESULT CGenlock::SetControlLimit(double cL)
{
    controlLimit = cL;
    return S_OK;
}

HRESULT CGenlock::GetControlLimit(double* cL)
{
    *cL = controlLimit;
    return S_OK;
}

HRESULT CGenlock::SetDisplayResolution(UINT columns, UINT lines)
{
    visibleColumns = columns;
    visibleLines = lines;
    return S_OK;
}

HRESULT CGenlock::AdviseSyncClock(ISyncClock* sC)
{
    if (!sC) {
        return E_FAIL;
    }
    if (syncClock) {
        syncClock = nullptr;    // Release any outstanding references if this is called repeatedly
    }
    syncClock = sC;
    return S_OK;
}

// Set the monitor to control. This is best done manually as not all monitors can be controlled
// so automatic detection of monitor to control might have unintended effects.
// The PowerStrip API uses zero-based monitor numbers, i.e. the default monitor is 0.
HRESULT CGenlock::SetMonitor(UINT mon)
{
    monitor = mon;
    return S_OK;
}

HRESULT CGenlock::ResetStats()
{
    CAutoLock lock(&csGenlockLock);
    minSyncOffset = DBL_MAX;
    maxSyncOffset = DBL_MIN;
    minFrameCycle = DBL_MAX;
    maxFrameCycle = DBL_MIN;
    displayAdjustmentsMade = 0;
    clockAdjustmentsMade = 0;
    return S_OK;
}

// Synchronize by adjusting display refresh rate
HRESULT CGenlock::ControlDisplay(double syncOffset, double frameCycle)
{
    LPARAM lParam = 0;
    WPARAM wParam = monitor;
    ATOM setTiming;

    const CRenderersSettings& r = GetRenderersSettings();
    targetSyncOffset = r.m_AdvRendSets.fTargetSyncOffset;
    lowSyncOffset = targetSyncOffset - r.m_AdvRendSets.fControlLimit;
    highSyncOffset = targetSyncOffset + r.m_AdvRendSets.fControlLimit;

    syncOffsetAvg = syncOffsetFifo.Average(syncOffset);
    minSyncOffset = std::min(minSyncOffset, syncOffset);
    maxSyncOffset = std::max(maxSyncOffset, syncOffset);
    frameCycleAvg = frameCycleFifo.Average(frameCycle);
    minFrameCycle = std::min(minFrameCycle, frameCycle);
    maxFrameCycle = std::max(maxFrameCycle, frameCycle);

    if (!PowerstripRunning() || !powerstripTimingExists) {
        return E_FAIL;
    }
    // Adjust as seldom as possible by checking the current controlState before changing it.
    if ((syncOffsetAvg > highSyncOffset) && (adjDelta != 1))
        // Speed up display refresh rate by subtracting pixels from the image.
    {
        adjDelta = 1; // Increase refresh rate
        curDisplayFreq = displayFreqFaster;
        setTiming = GlobalAddAtom(faster);
        lParam = setTiming;
        SendMessage(psWnd, UM_SETCUSTOMTIMINGFAST, wParam, lParam);
        GlobalDeleteAtom(setTiming);
        displayAdjustmentsMade++;
    } else
        // Slow down display refresh rate by adding pixels to the image.
        if ((syncOffsetAvg < lowSyncOffset) && (adjDelta != -1)) {
            adjDelta = -1;
            curDisplayFreq = displayFreqSlower;
            setTiming = GlobalAddAtom(slower);
            lParam = setTiming;
            SendMessage(psWnd, UM_SETCUSTOMTIMINGFAST, wParam, lParam);
            GlobalDeleteAtom(setTiming);
            displayAdjustmentsMade++;
        } else
            // Cruise.
            if ((syncOffsetAvg < targetSyncOffset) && (adjDelta == 1)) {
                adjDelta = 0;
                curDisplayFreq = displayFreqCruise;
                setTiming = GlobalAddAtom(cruise);
                lParam = setTiming;
                SendMessage(psWnd, UM_SETCUSTOMTIMINGFAST, wParam, lParam);
                GlobalDeleteAtom(setTiming);
                displayAdjustmentsMade++;
            } else if ((syncOffsetAvg > targetSyncOffset) && (adjDelta == -1)) {
                adjDelta = 0;
                curDisplayFreq = displayFreqCruise;
                setTiming = GlobalAddAtom(cruise);
                lParam = setTiming;
                SendMessage(psWnd, UM_SETCUSTOMTIMINGFAST, wParam, lParam);
                GlobalDeleteAtom(setTiming);
                displayAdjustmentsMade++;
            }
    return S_OK;
}

// Synchronize by adjusting reference clock rate (and therefore video FPS).
// Todo: check so that we don't have a live source
HRESULT CGenlock::ControlClock(double syncOffset, double frameCycle)
{
    const CRenderersSettings& r = GetRenderersSettings();
    targetSyncOffset = r.m_AdvRendSets.fTargetSyncOffset;
    lowSyncOffset = targetSyncOffset - r.m_AdvRendSets.fControlLimit;
    highSyncOffset = targetSyncOffset + r.m_AdvRendSets.fControlLimit;

    syncOffsetAvg = syncOffsetFifo.Average(syncOffset);
    minSyncOffset = std::min(minSyncOffset, syncOffset);
    maxSyncOffset = std::max(maxSyncOffset, syncOffset);
    frameCycleAvg = frameCycleFifo.Average(frameCycle);
    minFrameCycle = std::min(minFrameCycle, frameCycle);
    maxFrameCycle = std::max(maxFrameCycle, frameCycle);

    if (!syncClock) {
        return E_FAIL;
    }
    // Adjust as seldom as possible by checking the current controlState before changing it.
    if ((syncOffsetAvg > highSyncOffset) && (adjDelta != 1))
        // Slow down video stream.
    {
        adjDelta = 1;
        syncClock->AdjustClock(1.0 - cycleDelta); // Makes the clock move slower by providing smaller increments
        clockAdjustmentsMade++;
    } else
        // Speed up video stream.
        if ((syncOffsetAvg < lowSyncOffset) && (adjDelta != -1)) {
            adjDelta = -1;
            syncClock->AdjustClock(1.0 + cycleDelta);
            clockAdjustmentsMade++;
        } else
            // Cruise.
            if ((syncOffsetAvg < targetSyncOffset) && (adjDelta == 1)) {
                adjDelta = 0;
                syncClock->AdjustClock(1.0);
                clockAdjustmentsMade++;
            } else if ((syncOffsetAvg > targetSyncOffset) && (adjDelta == -1)) {
                adjDelta = 0;
                syncClock->AdjustClock(1.0);
                clockAdjustmentsMade++;
            }
    return S_OK;
}

// Don't adjust anything, just update the syncOffset stats
HRESULT CGenlock::UpdateStats(double syncOffset, double frameCycle)
{
    syncOffsetAvg = syncOffsetFifo.Average(syncOffset);
    minSyncOffset = std::min(minSyncOffset, syncOffset);
    maxSyncOffset = std::max(maxSyncOffset, syncOffset);
    frameCycleAvg = frameCycleFifo.Average(frameCycle);
    minFrameCycle = std::min(minFrameCycle, frameCycle);
    maxFrameCycle = std::max(maxFrameCycle, frameCycle);
    return S_OK;
}

STDMETHODIMP CSyncAP::SetD3DFullscreen(bool fEnabled)
{
    CAutoLock cAutoLock(this);
    CAutoLock cRenderLock(&m_allocatorLock);

    fullScreenChanged = (fEnabled != m_bIsFullscreen);
    m_bIsFullscreen = fEnabled;
    return S_OK;
}

STDMETHODIMP CSyncAP::GetD3DFullscreen(bool* pfEnabled)
{
    CheckPointer(pfEnabled, E_POINTER);
    *pfEnabled = m_bIsFullscreen;
    return S_OK;
}
