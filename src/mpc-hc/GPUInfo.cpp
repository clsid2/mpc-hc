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

#include "stdafx.h"
#include "GPUInfo.h"
#include <dxva2api.h>

#pragma comment(lib,"d3d11.lib")
#pragma comment(lib, "dxva2.lib")

GPUDetails::GPUDetails()
{

}

const GUID H264_VLD_NOFGT         = { 0x1b81be68, 0xa0c7, 0x11d3, 0xb9, 0x84, 0x00, 0xc0, 0x4f, 0x2e, 0x73, 0xc5};
const GUID H264_VLD_FGT           = { 0x1b81be69, 0xa0c7, 0x11d3, 0xb9, 0x84, 0x00, 0xc0, 0x4f, 0x2e, 0x73, 0xc5};
const GUID H264_VLD_INTEL_OLD     = { 0x604F8E68, 0x4951, 0x4C54, 0x88, 0xFE, 0xAB, 0xD2, 0x5C, 0x15, 0xB3, 0xD6};
const GUID MPEG2                  = { 0xee27417f, 0x5e28, 0x4e65, 0xbe, 0xea, 0x1d, 0x26, 0xb5, 0x08, 0xad, 0xc9};
const GUID VC1_VLD                = { 0x1b81beA3, 0xa0c7, 0x11d3, 0xb9, 0x84, 0x00, 0xc0, 0x4f, 0x2e, 0x73, 0xc5};
const GUID VC1_D2010              = { 0x1b81beA4, 0xa0c7, 0x11d3, 0xb9, 0x84, 0x00, 0xc0, 0x4f, 0x2e, 0x73, 0xc5};
const GUID VP9_VLD_PROFILE0       = { 0x463707f8, 0xa1d0, 0x4585, 0x87, 0x6d, 0x83, 0xaa, 0x6d, 0x60, 0xb8, 0x9e};
const GUID VP9_VLD_10BIT_PROFILE2 = { 0xa4c749ef, 0x6ecf, 0x48aa, 0x84, 0x48, 0x50, 0xa7, 0xa1, 0x16, 0x5f, 0xf7};
const GUID HEVC_VLD_MAIN          = { 0x5b11d51b, 0x2f4c, 0x4452, 0xbc, 0xc3, 0x09, 0xf2, 0xa1, 0x16, 0x0c, 0xc0};
const GUID HEVC_VLD_MAIN10        = { 0x107af0e0, 0xef1a, 0x4d19, 0xab, 0xa8, 0x67, 0xa1, 0x63, 0x07, 0x3d, 0x13};
const GUID HEVC_VLD_MAIN12        = { 0x1a72925f, 0x0c2c, 0x4f15, 0x96, 0xfb, 0xb1, 0x7d, 0x14, 0x73, 0x60, 0x3f};
const GUID HEVC_VLD_MAIN16        = { 0xa4fbdbb0, 0xa113, 0x482b, 0xa2, 0x32, 0x63, 0x5c, 0xc0, 0x69, 0x7f, 0x6d};
const GUID AV1_PROFILE0           = { 0xb8be4ccb, 0xcf53, 0x46ba, 0x8d, 0x59, 0xd6, 0xb8, 0xa6, 0xda, 0x5d, 0x2a};
const GUID AV1_PROFILE1           = { 0x6936ff0f, 0x45b1, 0x4163, 0x9c, 0xc1, 0x64, 0x6e, 0xf6, 0x94, 0x61, 0x08};
const GUID AV1_PROFILE2           = { 0x0c5f2aa1, 0xe541, 0x4089, 0xbb, 0x7b, 0x98, 0x11, 0x0a, 0x19, 0xd7, 0xc8};

GPUDetect::GPUDetect(bool check_hwa_caps /*= true*/, bool skip_d3d11 /*= false*/)
{
    HRESULT hr = E_INVALIDARG;
    ID3D11Device* pD3D11Device = nullptr;
	ID3D11DeviceContext* pContext = nullptr;
	D3D_FEATURE_LEVEL fl_available = D3D_FEATURE_LEVEL_9_1;
	D3D_FEATURE_LEVEL fl_list[] = { D3D_FEATURE_LEVEL_11_1 };

    bool win8plus = IsWindowsVersionOrGreaterBuild(8, 0, 0);
    if (!skip_d3d11 && win8plus) {
		hr = D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, 0, fl_list, _countof(fl_list), D3D11_SDK_VERSION, &pD3D11Device, &fl_available, &pContext);
	}
	if (!skip_d3d11 && hr != S_OK) {
		hr = D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, 0, nullptr, 0, D3D11_SDK_VERSION, &pD3D11Device, &fl_available, &pContext);
	}
    if (!skip_d3d11 && hr == S_OK && pD3D11Device) {
        IDXGIDevice1* pDXGIDevice1 = nullptr;
        hr = pD3D11Device->QueryInterface(&pDXGIDevice1);
        if (hr == S_OK && pDXGIDevice1) {
            IDXGIAdapter* pDXGIAdapter = nullptr;
            hr = pDXGIDevice1->GetAdapter(&pDXGIAdapter);
            if (hr == S_OK && pDXGIAdapter) {
                IDXGIFactory1* pIDXGIFactory1 = nullptr;
                pDXGIAdapter->GetParent(__uuidof(IDXGIFactory1), (void**)&pIDXGIFactory1);
                if (hr == S_OK && pIDXGIFactory1) {
                    if (GetGPUDetailsDX11(pIDXGIFactory1, 0, &gpu1)) {
                        gpu_count = 1;
                        featurelevel = (DWORD)fl_available;
                        if (GetGPUDetailsDX11(pIDXGIFactory1, 1, &gpu2)) {
                            gpu_count = 2;
                        }

                        if (check_hwa_caps && featurelevel >= D3D_FEATURE_LEVEL_11_1 && gpu1.vendorid != 0 && gpu1.deviceid != 0 && gpu1.vendorid != PCIV_MICROSOFT) {
                            // get video decoding abilities
                            ID3D11VideoDevice* pVideoDevice = nullptr;
                            if (S_OK == pD3D11Device->QueryInterface(&pVideoDevice)) {
                                const UINT decoderProfileCount = pVideoDevice->GetVideoDecoderProfileCount();
                                if (decoderProfileCount > 0) {
                                    DWORD hwacaps = 0;
                                    for (UINT i = 0; i < decoderProfileCount; i++) {
                                        GUID decoderProfile = {};
                                        if (S_OK == pVideoDevice->GetVideoDecoderProfile(i, &decoderProfile)) {
                                            if (decoderProfile == H264_VLD_NOFGT || decoderProfile == H264_VLD_FGT) {
                                                hwacaps |= GPU_HWA_CAP_H264;
                                                hwacaps |= GPU_HWA_CAP_H264_4K;
                                            } else if (decoderProfile == VP9_VLD_PROFILE0) {
                                                hwacaps |= GPU_HWA_CAP_VP9_P0;
                                            } else if (decoderProfile == VP9_VLD_10BIT_PROFILE2) {
                                                hwacaps |= GPU_HWA_CAP_VP9_P2;
                                            } else if (decoderProfile == HEVC_VLD_MAIN) {
                                                hwacaps |= GPU_HWA_CAP_HEVC_MAIN;
                                            } else if (decoderProfile == HEVC_VLD_MAIN10) {
                                                hwacaps |= GPU_HWA_CAP_HEVC_MAIN10;
                                            } else if (decoderProfile == HEVC_VLD_MAIN12) {
                                                hwacaps |= GPU_HWA_CAP_HEVC_MAIN12;
                                            } else if (decoderProfile == HEVC_VLD_MAIN16) {
                                                hwacaps |= GPU_HWA_CAP_HEVC_MAIN16;
                                            } else if (decoderProfile == AV1_PROFILE0) {
                                                hwacaps |= GPU_HWA_CAP_AV1_P0;
                                            } else if (decoderProfile == AV1_PROFILE1) {
                                                hwacaps |= GPU_HWA_CAP_AV1_P1;
                                            } else if (decoderProfile == AV1_PROFILE2) {
                                                hwacaps |= GPU_HWA_CAP_AV1_P2;
                                            }
                                        }
                                    }
                                    hwa_caps_dx11 = hwacaps;
                                }
                                pVideoDevice->Release();
                            }
                        }
                    }
                    pIDXGIFactory1->Release();
                }
                pDXGIAdapter->Release();
            }
            pDXGIDevice1->Release();
        }
        pD3D11Device->Release();
    }

    // DirectX 9
    if (skip_d3d11 || !gpu_count || check_hwa_caps && !hwa_caps_dx11) {
        IDirect3D9Ex* pD3D = nullptr;
        hr = Direct3DCreate9Ex(D3D_SDK_VERSION, &pD3D);
        if (hr == D3D_OK && pD3D) {
            UINT count = pD3D->GetAdapterCount();
            if (count > 0) {
                GPUDetails tmpgpu;
                if (GetGPUDetailsDX9(pD3D, 0, &tmpgpu)) {
                    if (!gpu_count) {
                        gpu1 = tmpgpu;
                        gpu_count = 1;
                        if (count > 1) {
                            GPUDetails tmpgpu2;
                            if (GetGPUDetailsDX9(pD3D, 1, &tmpgpu2)) {
                                if (tmpgpu2.deviceid != gpu1.deviceid) {
                                    gpu2 = tmpgpu2;
                                    gpu_count = 2;
                                } else if (count > 2) {
                                    GPUDetails tmpgpu3;
                                    if (GetGPUDetailsDX9(pD3D, 2, &tmpgpu3)) {
                                        if (tmpgpu3.deviceid != gpu1.deviceid) {
                                            gpu2 = tmpgpu3;
                                            gpu_count = 2;
                                        }
                                    }
                                }
                            }
                        }
                    }

                    if (check_hwa_caps && gpu1.vendorid != 0 && gpu1.deviceid != 0 && gpu1.vendorid != PCIV_MICROSOFT) {
                        // detect video decoding capabilities
                        IDirect3DDevice9Ex* pD3DDev = nullptr;
                        D3DPRESENT_PARAMETERS pp = {};
                        pp.Windowed = TRUE;
                        pp.SwapEffect = D3DSWAPEFFECT_DISCARD;
                        pp.BackBufferFormat = D3DFMT_UNKNOWN;
                        HWND hwnd = GetDesktopWindow();
                        hr = pD3D->CreateDeviceEx(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, hwnd, D3DCREATE_HARDWARE_VERTEXPROCESSING | D3DCREATE_MULTITHREADED | D3DCREATE_NOWINDOWCHANGES, &pp, nullptr, &pD3DDev);
                        if (SUCCEEDED(hr)) {
                            IDirectXVideoDecoderService* decoderService = nullptr;
                            hr = DXVA2CreateVideoService(pD3DDev, IID_PPV_ARGS(&decoderService));
                            if (SUCCEEDED(hr)) {
                                UINT count = 0;
                                GUID* guids = nullptr;
                                hr = decoderService->GetDecoderDeviceGuids(&count, &guids);
                                if (SUCCEEDED(hr)) {
                                    DWORD hwacaps = 0;
                                    for (UINT i = 0; i < count; ++i)
                                    {
                                        GUID decoderProfile = guids[i];
                                        if (decoderProfile == H264_VLD_NOFGT || decoderProfile == H264_VLD_FGT) {
                                            hwacaps |= GPU_HWA_CAP_H264;

                                            // test 4k support
                                            if (!(hwacaps & GPU_HWA_CAP_H264_4K)) {
                                                DXVA2_VideoDesc desc = {};
                                                desc.SampleWidth = 3840;
                                                desc.SampleHeight = 2160;
                                                desc.Format = (D3DFORMAT)MAKEFOURCC('N', 'V', '1', '2');
                                                UINT count = 0;
                                                DXVA2_ConfigPictureDecode* cfg = nullptr;
                                                hr = decoderService->GetDecoderConfigurations(decoderProfile, &desc, nullptr, &count, &cfg);
                                                if (SUCCEEDED(hr) && count > 0)
                                                {
                                                    hwacaps |= GPU_HWA_CAP_H264_4K;
                                                }
                                                CoTaskMemFree(cfg);
                                            }
                                        } else if (decoderProfile == H264_VLD_INTEL_OLD) {
                                            hwacaps |= GPU_HWA_CAP_H264_INTEL;
                                        } else if (decoderProfile == VP9_VLD_PROFILE0) {
                                            hwacaps |= GPU_HWA_CAP_VP9_P0;
                                        } else if (decoderProfile == VP9_VLD_10BIT_PROFILE2) {
                                            hwacaps |= GPU_HWA_CAP_VP9_P2;
                                        } else if (decoderProfile == HEVC_VLD_MAIN) {
                                            hwacaps |= GPU_HWA_CAP_HEVC_MAIN;
                                        } else if (decoderProfile == HEVC_VLD_MAIN10) {
                                            hwacaps |= GPU_HWA_CAP_HEVC_MAIN10;
                                        } else if (decoderProfile == HEVC_VLD_MAIN12) {
                                            hwacaps |= GPU_HWA_CAP_HEVC_MAIN12;
                                        } else if (decoderProfile == HEVC_VLD_MAIN16) {
                                            hwacaps |= GPU_HWA_CAP_HEVC_MAIN16;
                                        } else if (decoderProfile == AV1_PROFILE0) {
                                            hwacaps |= GPU_HWA_CAP_AV1_P0;
                                        } else if (decoderProfile == AV1_PROFILE1) {
                                            hwacaps |= GPU_HWA_CAP_AV1_P1;
                                        } else if (decoderProfile == AV1_PROFILE2) {
                                            hwacaps |= GPU_HWA_CAP_AV1_P2;
                                        }
                                    }
                                    CoTaskMemFree(guids);
                                    hwa_caps_dx9 = hwacaps;
                                }
                                decoderService->Release();
                            }
                            pD3DDev->Release();
                        }
                    }
                }
            }
            pD3D->Release();
        }
    }
}

bool GPUDetect::GetGPUDetailsDX11(IDXGIFactory1* pIDXGIFactory1, int id, GPUDetails* gpu)
{
    bool success = false;
    IDXGIAdapter1* pEnumAdapter;
	HRESULT hr = pIDXGIFactory1->EnumAdapters1(id, &pEnumAdapter);
	if (hr == S_OK) {
		DXGI_ADAPTER_DESC1 desc;
		hr = pEnumAdapter->GetDesc1(&desc);
		if (hr == S_OK) {
			gpu->vendorid = desc.VendorId & 0xFFFF;
			gpu->deviceid = desc.DeviceId & 0xFFFF;
            gpu->description = CString(desc.Description);
			if (desc.DedicatedVideoMemory) {
				SIZE_T temp = desc.DedicatedVideoMemory / 1024 / 1024;
				gpu->memorysize = (DWORD) temp;
			} else {
				SIZE_T temp = desc.DedicatedSystemMemory / 1024 / 1024;
				gpu->memorysize = (DWORD)temp;
			}
            pEnumAdapter->CheckInterfaceSupport(__uuidof(IDXGIDevice), &gpu->UMDVersion);
            success = true;
		}
		pEnumAdapter->Release();
	}

    return success;
}

bool GPUDetect::GetGPUDetailsDX9(IDirect3D9Ex* pD3D, int id, GPUDetails* gpu)
{
    bool success = false;
    D3DADAPTER_IDENTIFIER9 aid9;
	HRESULT hr = pD3D->GetAdapterIdentifier(id, 0, &aid9);
	if (hr == D3D_OK) {
		gpu->vendorid = aid9.VendorId;
		gpu->deviceid = aid9.DeviceId;
        gpu->description = CStringW(aid9.Description);
        gpu->UMDVersion = aid9.DriverVersion;

        success = true;
	}

    return success;
}

LONGLONG MakeVersion(uint16_t a, uint16_t b, uint16_t c, uint16_t d)
{
    return (static_cast<ULONGLONG>(a) << 48) | (static_cast<ULONGLONG>(b) << 32) | (static_cast<ULONGLONG>(c) << 16) | static_cast<ULONGLONG>(d);
}

bool GPUDetect::AMD_DXVA2_Blacklist()
{
    if (gpu_count > 0 && gpu1.vendorid == PCIV_AMD && IsWindowsVersionOrGreaterBuild(8, 1, 0)) {
        // Radeon HD 2xxx-7xxx, buggy driver 15.7 till 15.11
        if (gpu1.UMDVersion.QuadPart == MakeVersion(8,17,10,1404) || gpu1.UMDVersion.QuadPart >= MakeVersion(15,200,1046,0) && gpu1.UMDVersion.QuadPart < MakeVersion(15,201,1151,1009)) {
            return true;
        }
    }

    return false;
}

// GPUs which have (slow) partial acceleration of HEVC
bool GPUDetect::IntelHEVCBlacklist()
{
    bool result = false;

    if (gpu_count > 0 && gpu1.vendorid == PCIV_INTEL) {
        switch (gpu1.deviceid) {
            case 0x0412: // Haswell
            case 0x0416:
            case 0x041a:
            case 0x041e:
            case 0x0a16:
            case 0x0a1e:
            case 0x0a26:
            case 0x0a2e:
            case 0x0c02:
            case 0x0c06:
            case 0x0c12:
            case 0x0c16:
            case 0x0c22:
            case 0x0c26:
            case 0x0d06:
            case 0x0d16:
            case 0x0d22:
            case 0x0d26:
            case 0x1612: // Broadwell
            case 0x1616:
            case 0x161a:
            case 0x161b:
            case 0x161d:
            case 0x161e:
            case 0x1622:
            case 0x1626:
            case 0x162a:
            case 0x162b:
            case 0x162d:
            case 0x162e:
                result = true;
        }
    }

    return result;
}
