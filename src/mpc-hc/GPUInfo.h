/*
* (C) 2026 see Authors.txt
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

#include "FileVersionInfo.h"
#include "VersionHelpersInternal.h"
#include <d3d9.h>
#include <d3d11.h>

# define PCIV_INTEL     0x8086
# define PCIV_AMD       0x1002
# define PCIV_NVIDIA    0x10de
# define PCIV_QUALCOMM  0x4351
# define PCIV_MICROSOFT 0x1414

const int GPU_HWA_CAP_H264_INTEL  = 1 << 0;
const int GPU_HWA_CAP_H264        = 1 << 1;
const int GPU_HWA_CAP_H264_4K     = 1 << 2;
const int GPU_HWA_CAP_VP9_P0      = 1 << 3;
const int GPU_HWA_CAP_VP9_P2      = 1 << 4;
const int GPU_HWA_CAP_HEVC_MAIN   = 1 << 5;
const int GPU_HWA_CAP_HEVC_MAIN10 = 1 << 6;
const int GPU_HWA_CAP_HEVC_MAIN12 = 1 << 7;
const int GPU_HWA_CAP_HEVC_MAIN16 = 1 << 8;
const int GPU_HWA_CAP_AV1_P0      = 1 << 9;
const int GPU_HWA_CAP_AV1_P1      = 1 << 10;
const int GPU_HWA_CAP_AV1_P2      = 1 << 11;

class GPUDetails
{
public:
    GPUDetails();

    UINT vendorid = 0;
    UINT deviceid = 0;
    CStringW description;
    DWORD memorysize = 0;
    LARGE_INTEGER UMDVersion = LARGE_INTEGER();

    CString GetDriverVersionString() { return FileVersionInfo::FormatVersionString(UMDVersion.LowPart, UMDVersion.HighPart); }
};

class GPUDetect
{
public:
    GPUDetect(bool check_hwa_caps = true, bool skip_d3d11 = false);

    int GetCount() { return gpu_count; }
    int GetHwaCaps() { return hwa_caps_dx11 ? hwa_caps_dx11 : hwa_caps_dx9; }
    bool SupportHWA() { return hwa_caps_dx11 > 0 ||  hwa_caps_dx9 > 0; }
    bool SupportD3D11VA() { return hwa_caps_dx11 > 0 && featurelevel >= D3D_FEATURE_LEVEL_11_1; }
    bool IntelHEVCBlacklist();
    // AMD driver from 2015 (version 15.x), the last one for TeraScale cards
    bool HasLegacyAMDDriver() {
        auto legacy = [](const GPUDetails& gpu) { return gpu.vendorid == PCIV_AMD && HIWORD(gpu.UMDVersion.HighPart) == 15; };
        return (gpu_count >= 1 && legacy(gpu1)) || (gpu_count >= 2 && legacy(gpu2));
    }
    bool UseMPCVR() { return GetHwaCaps() >= GPU_HWA_CAP_H264_4K; }
    CString GetGPUID(GPUDetails& gpu) {
        CString res;
        res.Format(L"%04x:%04x", gpu.vendorid, gpu.deviceid);
        return res;
    }
    CString GetGPUID1() { return GetGPUID(gpu1); }
    CString GetGPUID2() { return GetGPUID(gpu2); }

    GPUDetails gpu1;
    GPUDetails gpu2;

private:
    DWORD featurelevel = 0;
    int gpu_count = 0;
    int hwa_caps_dx9 = 0;
    int hwa_caps_dx11 = 0;

    bool GetGPUDetailsDX11(IDXGIFactory1* pIDXGIFactory1, int id, GPUDetails* gpu);
    bool GetGPUDetailsDX9(IDirect3D9Ex* pD3D, int id, GPUDetails* gpu);
};
