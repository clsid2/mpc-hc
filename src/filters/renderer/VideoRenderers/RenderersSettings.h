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

#pragma once

#include "../../../SubPic/SubPicQueueSettings.h"

enum {
    WM_REARRANGERENDERLESS = WM_APP + 1,
    WM_RESET_DEVICE
};

#define WM_MYMOUSELAST WM_XBUTTONDBLCLK

enum {
    VIDRNDT_AP_SURFACE,
    VIDRNDT_AP_TEXTURE2D,
    VIDRNDT_AP_TEXTURE3D
};

enum VideoSystem {
    VIDEO_SYSTEM_UNKNOWN,
    VIDEO_SYSTEM_HDTV,
    VIDEO_SYSTEM_SDTV_NTSC,
    VIDEO_SYSTEM_SDTV_PAL
};

enum AmbientLight {
    AMBIENT_LIGHT_BRIGHT,
    AMBIENT_LIGHT_DIM,
    AMBIENT_LIGHT_DARK
};

enum ColorRenderingIntent {
    COLOR_RENDERING_INTENT_PERCEPTUAL,
    COLOR_RENDERING_INTENT_RELATIVE_COLORIMETRIC,
    COLOR_RENDERING_INTENT_SATURATION,
    COLOR_RENDERING_INTENT_ABSOLUTE_COLORIMETRIC
};

class CRenderersSettings
{

public:
    bool fResetDevice;

    class CAdvRendererSettings
    {
    public:
        CAdvRendererSettings() { SetDefault(); }

        bool    bVMR9AlterativeVSync;
        int     iVMR9VSyncOffset;
        bool    bVMR9VSyncAccurate;
        bool    bVMR9FullscreenGUISupport;
        bool    bVMR9VSync;
        bool    bVMR9FullFloatingPointProcessing;
        bool    bVMR9HalfFloatingPointProcessing;
        bool    bVMR9ColorManagementEnable;
        int     iVMR9ColorManagementInput;
        int     iVMR9ColorManagementAmbientLight;
        int     iVMR9ColorManagementIntent;
        bool    bVMRDisableDesktopComposition;
        bool    bVMRFlushGPUBeforeVSync;
        bool    bVMRFlushGPUAfterPresent;
        bool    bVMRFlushGPUWait;
        bool    bDesktopSizeBackBuffer;

        // EVR
        bool    bEVRHighColorResolution;
        bool    bEVRForceInputHighColorResolution;
        bool    bEVREnableFrameTimeCorrection;
        int     iEVROutputRange;

        // SyncRenderer settings
        bool    bSynchronizeVideo;
        bool    bSynchronizeDisplay;
        bool    bSynchronizeNearest;
        int     iLineDelta;
        int     iColumnDelta;
        double  fCycleDelta;
        double  fTargetSyncOffset;
        double  fControlLimit;

        // Other settings
        bool    bCacheShaders;
        CString sShaderCachePath;

        void    SetDefault();
    };

    CAdvRendererSettings m_AdvRendSets;

    int         iAPSurfaceUsage;
    int         iDX9Resizer;
    bool        fVMR9MixerMode;
    int         iEvrBuffers;

    SubPicQueueSettings subPicQueueSettings;

    int         subPicVerticalShift;
    double      fontScaleOverride;

    CString     D3D9RenderDevice;
    void        UpdateData(bool fSave);
};

class CRenderersData
{
    HINSTANCE     m_hD3DX9Dll;
    const UINT    m_nDXSdkRelease;
    LARGE_INTEGER llPerfFrequency;

public:
    CRenderersData();

    bool        m_bTearingTest;
    int         m_iDisplayStats;
    bool        m_bResetStats;  // Set to reset the presentation statistics
    CString     m_strD3DX9Version;

    // Hardware feature support
    bool        m_bFP16Support;
    bool        m_bFP32Support;
    bool        m_b10bitSupport;
    CString     m_strDXVAInfo;

    LONGLONG    GetPerfCounter() const;
    HINSTANCE   GetD3X9Dll();
    UINT        GetDXSdkRelease() const { return m_nDXSdkRelease; };
};

extern CRenderersData*      GetRenderersData();
extern CRenderersSettings&  GetRenderersSettings();

extern bool LoadResource(UINT resid, CStringA& str, LPCTSTR restype);
