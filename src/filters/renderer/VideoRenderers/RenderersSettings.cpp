/*
 * (C) 2003-2006 Gabest
 * (C) 2006-2018 see Authors.txt
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
#include <VersionHelpers.h>
#include <mpc-hc_config.h>
#include <d3d9.h>
#include "d3dx9/d3dx9.h"

void CRenderersSettings::CAdvRendererSettings::SetDefault()
{
    bVMR9AlterativeVSync              = false;
    iVMR9VSyncOffset                  = 0;
    bVMR9VSyncAccurate                = false;
    bVMR9FullscreenGUISupport         = false;
    bVMR9VSync                        = false;
    bVMR9FullFloatingPointProcessing  = false;
    bVMR9HalfFloatingPointProcessing  = false;
    bVMR9ColorManagementEnable        = false;
    iVMR9ColorManagementInput         = VIDEO_SYSTEM_UNKNOWN;
    iVMR9ColorManagementAmbientLight  = AMBIENT_LIGHT_BRIGHT;
    iVMR9ColorManagementIntent        = COLOR_RENDERING_INTENT_PERCEPTUAL;
    bVMRDisableDesktopComposition     = false;
    bVMRFlushGPUBeforeVSync           = true;
    bVMRFlushGPUAfterPresent          = true;
    bVMRFlushGPUWait                  = false;
    bEVRHighColorResolution           = false;
    bEVRForceInputHighColorResolution = false;
    bEVREnableFrameTimeCorrection     = false;
    iEVROutputRange                   = 0;
    bSynchronizeVideo                 = false;
    bSynchronizeDisplay               = false;
    bSynchronizeNearest               = true;
    iLineDelta                        = 0;
    iColumnDelta                      = 0;
    fCycleDelta                       = 0.0012;
    fTargetSyncOffset                 = 12.0;
    fControlLimit                     = 2.0;
    bCacheShaders                     = false;
    bDesktopSizeBackBuffer            = false;
}

/////////////////////////////////////////////////////////////////////////////
// CRenderersData construction

CRenderersData::CRenderersData()
    : m_hD3DX9Dll(nullptr)
    , m_nDXSdkRelease(D3DX_SDK_VERSION)
    , m_bTearingTest(false)
    , m_iDisplayStats(0)
    , m_bResetStats(false)
      // Don't disable hardware features before initializing a renderer
    , m_bFP16Support(true)
    , m_bFP32Support(true)
    , m_b10bitSupport(true)
{
    QueryPerformanceFrequency(&llPerfFrequency);
}

LONGLONG CRenderersData::GetPerfCounter() const
{
    LARGE_INTEGER i64Ticks100ns;

    QueryPerformanceCounter(&i64Ticks100ns);
    return llMulDiv(i64Ticks100ns.QuadPart, 10000000, llPerfFrequency.QuadPart, 0);
}

HINSTANCE CRenderersData::GetD3X9Dll()
{
    // D3DX9 v43 is the latest available and will not get any update. We support only this specific version.
    static_assert(D3DX_SDK_VERSION == MPC_DX_SDK_NUMBER, "Different D3DX9 version?");
    if (m_hD3DX9Dll == nullptr) {
        m_strD3DX9Version.Format(_T("d3dx9_%u.dll"), D3DX_SDK_VERSION);
        m_hD3DX9Dll = LoadLibrary(m_strD3DX9Version);
    }

    return m_hD3DX9Dll;
}
