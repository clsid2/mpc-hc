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
#include <Vfw.h>
#include "winddk/devioctl.h"
#include "winddk/ntddcdrm.h"
#include "DSUtil.h"
#include "Mpeg2Def.h"
#include <emmintrin.h>
#include <d3d9.h>
#include "NullRenderers.h"
#include "mvrInterfaces.h"

#include "moreuuids.h"
#include <dxva.h>
#include <dxva2api.h>
#include <locale.h>

int CountPins(IBaseFilter* pBF, int& nIn, int& nOut, int& nInC, int& nOutC)
{
    nIn = nOut = 0;
    nInC = nOutC = 0;

    BeginEnumPins(pBF, pEP, pPin) {
        PIN_DIRECTION dir;
        if (SUCCEEDED(pPin->QueryDirection(&dir))) {
            CComPtr<IPin> pPinConnectedTo;
            pPin->ConnectedTo(&pPinConnectedTo);

            if (dir == PINDIR_INPUT) {
                nIn++;
                if (pPinConnectedTo) {
                    nInC++;
                }
            } else if (dir == PINDIR_OUTPUT) {
                nOut++;
                if (pPinConnectedTo) {
                    nOutC++;
                }
            }
        }
    }
    EndEnumPins;

    return (nIn + nOut);
}

bool IsSplitter(IBaseFilter* pBF, bool fCountConnectedOnly)
{
    int nIn, nOut, nInC, nOutC;
    CountPins(pBF, nIn, nOut, nInC, nOutC);
    return (fCountConnectedOnly ? nOutC > 1 : nOut > 1);
}

bool IsMultiplexer(IBaseFilter* pBF, bool fCountConnectedOnly)
{
    int nIn, nOut, nInC, nOutC;
    CountPins(pBF, nIn, nOut, nInC, nOutC);
    return (fCountConnectedOnly ? nInC > 1 : nIn > 1);
}

bool IsStreamStart(IBaseFilter* pBF)
{
    CComQIPtr<IAMFilterMiscFlags> pAMMF(pBF);
    if (pAMMF && pAMMF->GetMiscFlags()&AM_FILTER_MISC_FLAGS_IS_SOURCE) {
        return true;
    }

    int nIn, nOut, nInC, nOutC;
    CountPins(pBF, nIn, nOut, nInC, nOutC);
    AM_MEDIA_TYPE mt;
    CComPtr<IPin> pIn = GetFirstPin(pBF);
    return ((nOut > 1)
            || (nOut > 0 && nIn == 1 && pIn && SUCCEEDED(pIn->ConnectionMediaType(&mt)) && mt.majortype == MEDIATYPE_Stream));
}

bool IsStreamEnd(IBaseFilter* pBF)
{
    int nIn, nOut, nInC, nOutC;
    CountPins(pBF, nIn, nOut, nInC, nOutC);
    return (nOut == 0);
}

bool IsVideoRenderer(IBaseFilter* pBF)
{
    CLSID clsid = GetCLSID(pBF);
    if (clsid == CLSID_VideoRenderer || clsid == CLSID_VideoRendererDefault || clsid == CLSID_VideoMixingRenderer9 || clsid == CLSID_EnhancedVideoRenderer || clsid == CLSID_madVR || clsid == CLSID_DXR || clsid == CLSID_MPCVR) {
        return true;
    }

    int nIn, nOut, nInC, nOutC;
    CountPins(pBF, nIn, nOut, nInC, nOutC);

    if (nInC > 0 && nOut == 0) {
        BeginEnumPins(pBF, pEP, pPin) {
            AM_MEDIA_TYPE mt;
            if (S_OK != pPin->ConnectionMediaType(&mt)) {
                continue;
            }

            FreeMediaType(mt);

            return !!(mt.majortype == MEDIATYPE_Video);
            /*&& (mt.formattype == FORMAT_VideoInfo || mt.formattype == FORMAT_VideoInfo2));*/
        }
        EndEnumPins;
    }

    return false;
}

bool IsAudioWaveRenderer(IBaseFilter* pBF)
{
    int nIn, nOut, nInC, nOutC;
    CountPins(pBF, nIn, nOut, nInC, nOutC);

    if (nInC > 0 && nOut == 0 && CComQIPtr<IBasicAudio>(pBF)) {
        BeginEnumPins(pBF, pEP, pPin) {
            AM_MEDIA_TYPE mt;
            if (S_OK != pPin->ConnectionMediaType(&mt)) {
                continue;
            }

            FreeMediaType(mt);

            return !!(mt.majortype == MEDIATYPE_Audio);
            /*&& mt.formattype == FORMAT_WaveFormatEx);*/
        }
        EndEnumPins;
    }

    CLSID clsid;
    memcpy(&clsid, &GUID_NULL, sizeof(clsid));
    pBF->GetClassID(&clsid);

    return clsid == CLSID_DSoundRender ||
           clsid == CLSID_AudioRender ||
           clsid == CLSID_SANEAR_INTERNAL ||
           clsid == CLSID_SANEAR ||
           clsid == CLSID_ReClock ||
           clsid == CLSID_MPCBEAudioRenderer ||
           clsid == GUIDFromCString(L"{EC9ED6FC-7B03-4cb6-8C01-4EABE109F26B}") || // MediaPortal Audio Renderer
           clsid == GUIDFromCString(L"{50063380-2B2F-4855-9A1E-40FCA344C7AC}") || // Surodev ASIO Renderer
           clsid == GUIDFromCString(L"{8DE31E85-10FC-4088-8861-E0EC8E70744A}") || // MultiChannel ASIO Renderer
           clsid == GUIDFromCString(L"{205F9417-8EEF-40B4-91CF-C7C6A96936EF}") || // MBSE MultiChannel ASIO Renderer
           clsid == __uuidof(CNullAudioRenderer) ||
           clsid == __uuidof(CNullUAudioRenderer);
}

IBaseFilter* GetUpStreamFilter(IBaseFilter* pBF, IPin* pInputPin)
{
    return GetFilterFromPin(GetUpStreamPin(pBF, pInputPin));
}

IPin* GetUpStreamPin(IBaseFilter* pBF, IPin* pInputPin)
{
    BeginEnumPins(pBF, pEP, pPin) {
        if (pInputPin && pInputPin != pPin) {
            continue;
        }

        PIN_DIRECTION dir;
        CComPtr<IPin> pPinConnectedTo;
        if (SUCCEEDED(pPin->QueryDirection(&dir)) && dir == PINDIR_INPUT
                && SUCCEEDED(pPin->ConnectedTo(&pPinConnectedTo))) {
            IPin* pRet = pPinConnectedTo.Detach();
            pRet->Release();
            return pRet;
        }
    }
    EndEnumPins;

    return nullptr;
}

IPin* GetFirstPin(IBaseFilter* pBF, PIN_DIRECTION dir)
{
    if (pBF) {
        BeginEnumPins(pBF, pEP, pPin) {
            PIN_DIRECTION dir2;
            if (SUCCEEDED(pPin->QueryDirection(&dir2)) && dir == dir2) {
                IPin* pRet = pPin.Detach();
                pRet->Release();
                return pRet;
            }
        }
        EndEnumPins;
    }

    return nullptr;
}

IPin* GetFirstDisconnectedPin(IBaseFilter* pBF, PIN_DIRECTION dir)
{
    if (pBF) {
        BeginEnumPins(pBF, pEP, pPin) {
            PIN_DIRECTION dir2;
            CComPtr<IPin> pPinTo;
            if (SUCCEEDED(pPin->QueryDirection(&dir2)) && dir == dir2 && (S_OK != pPin->ConnectedTo(&pPinTo))) {
                IPin* pRet = pPin.Detach();
                pRet->Release();
                return pRet;
            }
        }
        EndEnumPins;
    }

    return nullptr;
}

IBaseFilter* FindFilter(LPCWSTR clsid, IFilterGraph* pFG)
{
    CLSID clsid2;
    CLSIDFromString(CComBSTR(clsid), &clsid2);
    return FindFilter(clsid2, pFG);
}

IBaseFilter* FindFilter(const CLSID& clsid, IFilterGraph* pFG)
{
    BeginEnumFilters(pFG, pEF, pBF) {
        CLSID clsid2;
        if (SUCCEEDED(pBF->GetClassID(&clsid2)) && clsid == clsid2) {
            return pBF;
        }
    }
    EndEnumFilters;

    return nullptr;
}

IBaseFilter* FindFirstFilter(IFilterGraph* pFG) {
    BeginEnumFilters(pFG, pEF, pBF) {
        return pBF;
    }
    EndEnumFilters;

    return nullptr;
}

IPin* FindPin(IBaseFilter* pBF, PIN_DIRECTION direction, const AM_MEDIA_TYPE* pRequestedMT)
{
    PIN_DIRECTION pindir;
    BeginEnumPins(pBF, pEP, pPin) {
        CComPtr<IPin> pFellow;

        if (SUCCEEDED(pPin->QueryDirection(&pindir)) &&
                pindir == direction &&
                pPin->ConnectedTo(&pFellow) == VFW_E_NOT_CONNECTED) {
            BeginEnumMediaTypes(pPin, pEM, pmt) {
                if (pmt->majortype == pRequestedMT->majortype && pmt->subtype == pRequestedMT->subtype) {
                    return (pPin);
                }
            }
            EndEnumMediaTypes(pmt);
        }
    }
    EndEnumPins;
    return nullptr;
}

CStringW GetFilterName(IBaseFilter* pBF)
{
    CStringW name = _T("");

    if (pBF) {
        CFilterInfo fi;
        if (SUCCEEDED(pBF->QueryFilterInfo(&fi))) {
            name = fi.achName;
        }
    }

    return name;
}

CStringW GetPinName(IPin* pPin)
{
    CStringW name;
    CPinInfo pi;
    if (pPin && SUCCEEDED(pPin->QueryPinInfo(&pi))) {
        name = pi.achName;
    }

    return name;
}

IFilterGraph* GetGraphFromFilter(IBaseFilter* pBF)
{
    if (!pBF) {
        return nullptr;
    }
    IFilterGraph* pGraph = nullptr;
    CFilterInfo fi;
    if (pBF && SUCCEEDED(pBF->QueryFilterInfo(&fi))) {
        pGraph = fi.pGraph;
    }
    return pGraph;
}

IBaseFilter* GetFilterFromPin(IPin* pPin)
{
    if (!pPin) {
        return nullptr;
    }
    IBaseFilter* pBF = nullptr;
    CPinInfo pi;
    if (pPin && SUCCEEDED(pPin->QueryPinInfo(&pi))) {
        pBF = pi.pFilter;
    }
    return pBF;
}

IPin* AppendFilter(IPin* pPin, CString DisplayName, IGraphBuilder* pGB)
{
    IPin* pRet = pPin;

    CInterfaceList<IBaseFilter> pFilters;

    do {
        if (!pPin || DisplayName.IsEmpty() || !pGB) {
            break;
        }

        CComPtr<IPin> pPinTo;
        PIN_DIRECTION dir;
        if (FAILED(pPin->QueryDirection(&dir)) || dir != PINDIR_OUTPUT || SUCCEEDED(pPin->ConnectedTo(&pPinTo))) {
            break;
        }

        CComPtr<IBindCtx> pBindCtx;
        CreateBindCtx(0, &pBindCtx);

        CComPtr<IMoniker> pMoniker;
        ULONG chEaten;
        if (S_OK != MkParseDisplayName(pBindCtx, CComBSTR(DisplayName), &chEaten, &pMoniker)) {
            break;
        }

        CComPtr<IBaseFilter> pBF;
        if (FAILED(pMoniker->BindToObject(pBindCtx, 0, IID_PPV_ARGS(&pBF))) || !pBF) {
            break;
        }

        CComPtr<IPropertyBag> pPB;
        if (FAILED(pMoniker->BindToStorage(pBindCtx, 0, IID_PPV_ARGS(&pPB)))) {
            break;
        }

        CComVariant var;
        if (FAILED(pPB->Read(_T("FriendlyName"), &var, nullptr))) {
            break;
        }

        pFilters.AddTail(pBF);
        BeginEnumFilters(pGB, pEnum, pBF2);
        pFilters.AddTail(pBF2);
        EndEnumFilters;

        if (FAILED(pGB->AddFilter(pBF, CStringW(var.bstrVal)))) {
            break;
        }

        BeginEnumFilters(pGB, pEnum, pBF2);
        if (!pFilters.Find(pBF2) && SUCCEEDED(pGB->RemoveFilter(pBF2))) {
            pEnum->Reset();
        }
        EndEnumFilters;

        pPinTo = GetFirstPin(pBF, PINDIR_INPUT);
        if (!pPinTo) {
            pGB->RemoveFilter(pBF);
            break;
        }

        if (FAILED(pGB->ConnectDirect(pPin, pPinTo, nullptr))) {
            pGB->Connect(pPin, pPinTo);
            pGB->RemoveFilter(pBF);
            break;
        }

        BeginEnumFilters(pGB, pEnum, pBF2);
        if (!pFilters.Find(pBF2) && SUCCEEDED(pGB->RemoveFilter(pBF2))) {
            pEnum->Reset();
        }
        EndEnumFilters;

        pRet = GetFirstPin(pBF, PINDIR_OUTPUT);
        if (!pRet) {
            pRet = pPin;
            pGB->RemoveFilter(pBF);
            break;
        }
    } while (false);

    return pRet;
}

IPin* InsertFilter(IPin* pPin, CString DisplayName, IGraphBuilder* pGB)
{
    do {
        if (!pPin || DisplayName.IsEmpty() || !pGB) {
            break;
        }

        PIN_DIRECTION dir;
        if (FAILED(pPin->QueryDirection(&dir))) {
            break;
        }

        CComPtr<IPin> pFrom, pTo;

        if (dir == PINDIR_INPUT) {
            pPin->ConnectedTo(&pFrom);
            pTo = pPin;
        } else if (dir == PINDIR_OUTPUT) {
            pFrom = pPin;
            pPin->ConnectedTo(&pTo);
        }

        if (!pFrom || !pTo) {
            break;
        }

        CComPtr<IBindCtx> pBindCtx;
        CreateBindCtx(0, &pBindCtx);

        CComPtr<IMoniker> pMoniker;
        ULONG chEaten;
        if (S_OK != MkParseDisplayName(pBindCtx, CComBSTR(DisplayName), &chEaten, &pMoniker)) {
            break;
        }

        CComPtr<IBaseFilter> pBF;
        if (FAILED(pMoniker->BindToObject(pBindCtx, 0, IID_PPV_ARGS(&pBF))) || !pBF) {
            break;
        }

        CComPtr<IPropertyBag> pPB;
        if (FAILED(pMoniker->BindToStorage(pBindCtx, 0, IID_PPV_ARGS(&pPB)))) {
            break;
        }

        CComVariant var;
        if (FAILED(pPB->Read(_T("FriendlyName"), &var, nullptr))) {
            break;
        }

        if (FAILED(pGB->AddFilter(pBF, CStringW(var.bstrVal)))) {
            break;
        }

        CComPtr<IPin> pFromTo = GetFirstPin(pBF, PINDIR_INPUT);
        if (!pFromTo) {
            pGB->RemoveFilter(pBF);
            break;
        }

        if (FAILED(pGB->Disconnect(pFrom)) || FAILED(pGB->Disconnect(pTo))) {
            pGB->RemoveFilter(pBF);
            pGB->ConnectDirect(pFrom, pTo, nullptr);
            break;
        }

        if (FAILED(pGB->ConnectDirect(pFrom, pFromTo, nullptr))) {
            pGB->RemoveFilter(pBF);
            pGB->ConnectDirect(pFrom, pTo, nullptr);
            break;
        }

        CComPtr<IPin> pToFrom = GetFirstPin(pBF, PINDIR_OUTPUT);
        if (!pToFrom) {
            pGB->RemoveFilter(pBF);
            pGB->ConnectDirect(pFrom, pTo, nullptr);
            break;
        }

        if (FAILED(pGB->ConnectDirect(pToFrom, pTo, nullptr))) {
            pGB->RemoveFilter(pBF);
            pGB->ConnectDirect(pFrom, pTo, nullptr);
            break;
        }

        pPin = pToFrom;
    } while (false);

    return pPin;
}

void ExtractMediaTypes(IPin* pPin, CAtlArray<GUID>& types)
{
    types.RemoveAll();

    BeginEnumMediaTypes(pPin, pEM, pmt) {
        bool fFound = false;

        for (ptrdiff_t i = 0; !fFound && i < (int)types.GetCount(); i += 2) {
            if (types[i] == pmt->majortype && types[i + 1] == pmt->subtype) {
                fFound = true;
            }
        }

        if (!fFound) {
            types.Add(pmt->majortype);
            types.Add(pmt->subtype);
        }
    }
    EndEnumMediaTypes(pmt);
}

void ExtractMediaTypes(IPin* pPin, CAtlList<CMediaType>& mts)
{
    mts.RemoveAll();

    BeginEnumMediaTypes(pPin, pEM, pmt) {
        bool fFound = false;

        POSITION pos = mts.GetHeadPosition();
        while (!fFound && pos) {
            CMediaType& mt = mts.GetNext(pos);
            if (mt.majortype == pmt->majortype && mt.subtype == pmt->subtype) {
                fFound = true;
            }
        }

        if (!fFound) {
            mts.AddTail(CMediaType(*pmt));
        }
    }
    EndEnumMediaTypes(pmt);
}

int Eval_Exception(int n_except)
{
    if (n_except == STATUS_ACCESS_VIOLATION) {
        AfxMessageBox(_T("The property page of this filter has just caused a\nmemory access violation. The application will gently die now :)"));
    }

    return EXCEPTION_CONTINUE_SEARCH;
}

void MyOleCreatePropertyFrame(HWND hwndOwner, UINT x, UINT y, LPCOLESTR lpszCaption, ULONG cObjects, LPUNKNOWN FAR* lplpUnk, ULONG cPages, LPCLSID lpPageClsID, LCID lcid, DWORD dwReserved, LPVOID lpvReserved)
{
    __try {
        OleCreatePropertyFrame(hwndOwner, x, y, lpszCaption, cObjects, lplpUnk, cPages, lpPageClsID, lcid, dwReserved, lpvReserved);
    } __except (Eval_Exception(GetExceptionCode())) {
        // No code; this block never executed.
    }
}

void ShowPPage(CString DisplayName, HWND hParentWnd)
{
    CComPtr<IBindCtx> pBindCtx;
    CreateBindCtx(0, &pBindCtx);

    CComPtr<IMoniker> pMoniker;
    ULONG chEaten;
    if (S_OK != MkParseDisplayName(pBindCtx, CStringW(DisplayName), &chEaten, &pMoniker)) {
        return;
    }

    CComPtr<IBaseFilter> pBF;
    if (FAILED(pMoniker->BindToObject(pBindCtx, 0, IID_PPV_ARGS(&pBF))) || !pBF) {
        return;
    }

    ShowPPage(pBF, hParentWnd);
}

void ShowPPage(IUnknown* pUnk, HWND hParentWnd)
{
    CComQIPtr<ISpecifyPropertyPages> pSPP = pUnk;
    if (!pSPP) {
        return;
    }

    CString str;

    CComQIPtr<IBaseFilter> pBF = pSPP;
    CFilterInfo fi;
    CComQIPtr<IPin> pPin = pSPP;
    CPinInfo pi;
    if (pBF && SUCCEEDED(pBF->QueryFilterInfo(&fi))) {
        str = fi.achName;
    } else if (pPin && SUCCEEDED(pPin->QueryPinInfo(&pi))) {
        str = pi.achName;
    }

    CAUUID caGUID;
    caGUID.pElems = nullptr;
    if (SUCCEEDED(pSPP->GetPages(&caGUID))) {
        IUnknown* lpUnk = nullptr;
        pSPP.QueryInterface(&lpUnk);
        MyOleCreatePropertyFrame(
            hParentWnd, 0, 0, CStringW(str),
            1, (IUnknown**)&lpUnk,
            caGUID.cElems, caGUID.pElems,
            0, 0, nullptr);
        lpUnk->Release();

        if (caGUID.pElems) {
            CoTaskMemFree(caGUID.pElems);
        }
    }
}

CLSID GetCLSID(IBaseFilter* pBF)
{
    CLSID clsid = GUID_NULL;
    if (pBF) {
        pBF->GetClassID(&clsid);
    }
    return clsid;
}

CLSID GetCLSID(IPin* pPin)
{
    return GetCLSID(GetFilterFromPin(pPin));
}

CString CLSIDToString(CLSID& clsid)
{
    CComHeapPtr<OLECHAR> pStr;
    if (S_OK == StringFromCLSID(clsid, &pStr) && pStr) {
        return CString(pStr);
    }
    return CString();
}

bool IsCLSIDRegistered(LPCTSTR clsid)
{
    CString rootkey1(_T("CLSID\\"));
    CString rootkey2(_T("CLSID\\{083863F1-70DE-11d0-BD40-00A0C911CE86}\\Instance\\"));

    return ERROR_SUCCESS == CRegKey().Open(HKEY_CLASSES_ROOT, rootkey1 + clsid, KEY_READ)
           || ERROR_SUCCESS == CRegKey().Open(HKEY_CLASSES_ROOT, rootkey2 + clsid, KEY_READ);
}

bool IsCLSIDRegistered(const CLSID& clsid)
{
    bool fRet = false;

    CComHeapPtr<OLECHAR> pStr;
    if (S_OK == StringFromCLSID(clsid, &pStr) && pStr) {
        fRet = IsCLSIDRegistered(CString(pStr));
    }

    return fRet;
}

CString GetFilterPath(LPCTSTR clsid)
{
    CString rootkey1(_T("CLSID\\"));
    CString rootkey2(_T("CLSID\\{083863F1-70DE-11d0-BD40-00A0C911CE86}\\Instance\\"));

    CRegKey key;
    CString path;

    if (ERROR_SUCCESS == key.Open(HKEY_CLASSES_ROOT, rootkey1 + clsid + _T("\\InprocServer32"), KEY_READ)
            || ERROR_SUCCESS == key.Open(HKEY_CLASSES_ROOT, rootkey2 + clsid + _T("\\InprocServer32"), KEY_READ)) {
        ULONG nCount = MAX_PATH;
        key.QueryStringValue(nullptr, path.GetBuffer(nCount), &nCount);
        path.ReleaseBuffer(nCount);
    }

    return path;
}

CString GetFilterPath(const CLSID& clsid)
{
    CString path;

    CComHeapPtr<OLECHAR> pStr;
    if (S_OK == StringFromCLSID(clsid, &pStr) && pStr) {
        path = GetFilterPath(CString(pStr));
    }

    return path;
}

void CStringToBin(CString str, CAtlArray<BYTE>& data)
{
    str.Trim();
    ASSERT((str.GetLength() & 1) == 0);
    data.SetCount(str.GetLength() / 2);

    BYTE b = 0;

    str.MakeUpper();
    for (int i = 0, j = str.GetLength(); i < j; i++) {
        TCHAR c = str[i];
        if (c >= _T('0') && c <= _T('9')) {
            if (!(i & 1)) {
                b = ((char(c - _T('0')) << 4) & 0xf0) | (b & 0x0f);
            } else {
                b = (char(c - _T('0')) & 0x0f) | (b & 0xf0);
            }
        } else if (c >= _T('A') && c <= _T('F')) {
            if (!(i & 1)) {
                b = ((char(c - _T('A') + 10) << 4) & 0xf0) | (b & 0x0f);
            } else {
                b = (char(c - _T('A') + 10) & 0x0f) | (b & 0xf0);
            }
        } else {
            break;
        }

        if (i & 1) {
            data[i >> 1] = b;
            b = 0;
        }
    }
}

CString BinToCString(const BYTE* ptr, size_t len)
{
    CString ret;

    while (len-- > 0) {
        TCHAR high, low;

        high = (*ptr >> 4) >= 10 ? (*ptr >> 4) - 10 + _T('A') : (*ptr >> 4) + _T('0');
        low = (*ptr & 0xf) >= 10 ? (*ptr & 0xf) - 10 + _T('A') : (*ptr & 0xf) + _T('0');

        ret.AppendFormat(_T("%c%c"), high, low);

        ptr++;
    }

    return ret;
}

void FindFiles(CString fn, CAtlList<CString>& files)
{
    ExtendMaxPathLengthIfNeeded(fn);
    CString path = fn;
    path.Replace('/', '\\');
    path = path.Left(path.ReverseFind('\\') + 1);

    WIN32_FIND_DATA findData;
    ZeroMemory(&findData, sizeof(WIN32_FIND_DATA));
    HANDLE h = FindFirstFile(fn, &findData);
    if (h != INVALID_HANDLE_VALUE) {
        do {
            files.AddTail(path + findData.cFileName);
        } while (FindNextFile(h, &findData));

        FindClose(h);
    }
}

OpticalDiskType_t GetOpticalDiskType(TCHAR drive, CAtlList<CString>& files)
{
    files.RemoveAll();

    CString path;
    path.Format(_T("%c:"), drive);

    if (GetDriveType(path + _T("\\")) != DRIVE_CDROM) {
        return OpticalDisk_NotFound;
    }

    // Check if it contains a disc
    HANDLE hDevice = CreateFile(CString(_T("\\\\.\\")) + path, FILE_READ_ATTRIBUTES,
        FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL);
    if (hDevice == INVALID_HANDLE_VALUE) {
        return OpticalDisk_NotFound;
    }
    DWORD cbBytesReturned;
    BOOL bSuccess = DeviceIoControl(hDevice, IOCTL_STORAGE_CHECK_VERIFY2,
        NULL, 0, NULL, 0, &cbBytesReturned, (LPOVERLAPPED)NULL);
    if (!bSuccess) {
        return OpticalDisk_NotFound;
    }

    // CDROM_DVDVideo
    FindFiles(path + _T("\\VIDEO_TS\\video_ts.ifo"), files);
    if (!files.IsEmpty()) {
        return OpticalDisk_DVDVideo;
    }

    // CDROM_BD
    FindFiles(path + _T("\\BDMV\\index.bdmv"), files);
    if (!files.IsEmpty()) {
        return OpticalDisk_BD;
    }

    // CDROM_VideoCD
    FindFiles(path + _T("\\mpegav\\avseq??.dat"), files);
    FindFiles(path + _T("\\mpegav\\avseq??.mpg"), files);
    FindFiles(path + _T("\\mpeg2\\avseq??.dat"), files);
    FindFiles(path + _T("\\mpeg2\\avseq??.mpg"), files);
    FindFiles(path + _T("\\mpegav\\music??.dat"), files);
    FindFiles(path + _T("\\mpegav\\music??.mpg"), files);
    FindFiles(path + _T("\\mpeg2\\music??.dat"), files);
    FindFiles(path + _T("\\mpeg2\\music??.mpg"), files);
    if (!files.IsEmpty()) {
        return OpticalDisk_VideoCD;
    }

    // CDROM_Audio
    HANDLE hDrive = CreateFile(CString(_T("\\\\.\\")) + path, GENERIC_READ, FILE_SHARE_READ, nullptr,
                                OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, (HANDLE)nullptr);
    if (hDrive != INVALID_HANDLE_VALUE) {
        DWORD BytesReturned;
        CDROM_TOC TOC;
        if (DeviceIoControl(hDrive, IOCTL_CDROM_READ_TOC, nullptr, 0, &TOC, sizeof(TOC), &BytesReturned, 0)) {
            ASSERT(TOC.FirstTrack >= 1u && TOC.LastTrack <= _countof(TOC.TrackData));
            TOC.FirstTrack = std::max(TOC.FirstTrack, UCHAR(1));
            TOC.LastTrack = std::min(TOC.LastTrack, UCHAR(_countof(TOC.TrackData)));
            for (ptrdiff_t i = TOC.FirstTrack; i <= TOC.LastTrack; i++) {
                // MMC-3 Draft Revision 10g: Table 222 - Q Sub-channel control field
                auto& trackData = TOC.TrackData[i - 1];
                trackData.Control &= 5;
                if (trackData.Control == 0 || trackData.Control == 1) {
                    CString fn;
                    fn.Format(_T("%s\\track%02Id.cda"), path.GetString(), i);
                    files.AddTail(fn);
                }
            }
        }

        CloseHandle(hDrive);
    }
    if (!files.IsEmpty()) {
        return OpticalDisk_Audio;
    }

    // it is a cdrom but nothing special
    return OpticalDisk_Unknown;
}

CString GetDriveLabel(TCHAR drive)
{
    CString path;
    path.Format(_T("%c:\\"), drive);

    return GetDriveLabel(CPath(path));
}

CString GetDriveLabel(CPath path)
{
    CString label;
    path.StripToRoot();

    TCHAR VolumeNameBuffer[MAX_PATH], FileSystemNameBuffer[MAX_PATH];
    DWORD VolumeSerialNumber, MaximumComponentLength, FileSystemFlags;
    if (GetVolumeInformation(path,
                             VolumeNameBuffer, MAX_PATH, &VolumeSerialNumber, &MaximumComponentLength,
                             &FileSystemFlags, FileSystemNameBuffer, MAX_PATH)) {
        label = VolumeNameBuffer;
    }

    return label;
}

bool IsDriveVirtual(CString drive)
{
    HKEY hkey;
    DWORD type = REG_BINARY;
    TCHAR data[1024] = { 0 };
    DWORD size = sizeof(data) - 2;

    drive=(drive+_T(":")).Left(2);
    CString subkey = _T("\\DosDevices\\") + drive;

    RegOpenKeyEx(HKEY_LOCAL_MACHINE, _T("SYSTEM\\MountedDevices"), 0, KEY_READ, &hkey);
    if (hkey == INVALID_HANDLE_VALUE) return 0;
    RegQueryValueEx(hkey, subkey, 0, &type, (BYTE*)data, &size);

    RegCloseKey(hkey);
    CString sig(data);
    sig.MakeUpper();
    return sig.Find(_T("VEN_MSFT&PROD_VIRTUAL_DVD-ROM")) >= 0
        || sig.Find(_T("VEN_DISCSOFT&")) >= 0
        || sig.Find(_T("VEN_ELBY&PROD_CLONEDRIVE")) >= 0;
}

bool GetKeyFrames(CString fn, CUIntArray& kfs)
{
    kfs.RemoveAll();

    CString fn2 = CString(fn).MakeLower();
    if (fn2.Mid(fn2.ReverseFind('.') + 1) == _T("avi")) {
        AVIFileInit();

        PAVIFILE pfile;
        if (AVIFileOpen(&pfile, fn, OF_SHARE_DENY_WRITE, 0L) == 0) {
            AVIFILEINFO afi;
            ZeroMemory(&afi, sizeof(afi));
            AVIFileInfo(pfile, &afi, sizeof(AVIFILEINFO));

            CComPtr<IAVIStream> pavi;
            if (AVIFileGetStream(pfile, &pavi, streamtypeVIDEO, 0) == AVIERR_OK) {
                AVISTREAMINFO si;
                AVIStreamInfo(pavi, &si, sizeof(si));

                if (afi.dwCaps & AVIFILECAPS_ALLKEYFRAMES) {
                    kfs.SetSize(si.dwLength);
                    for (DWORD kf = 0; kf < si.dwLength; kf++) {
                        kfs[kf] = kf;
                    }
                } else {
                    for (LONG kf = 0; ; kf++) {
                        kf = pavi->FindSample(kf, FIND_KEY | FIND_NEXT);
                        if (kf < 0 || !kfs.IsEmpty() && kfs[kfs.GetCount() - 1] >= (UINT)kf) {
                            break;
                        }
                        kfs.Add(kf);
                    }

                    if (!kfs.IsEmpty() && kfs[kfs.GetCount() - 1] < si.dwLength - 1) {
                        kfs.Add(si.dwLength - 1);
                    }
                }
            }

            AVIFileRelease(pfile);
        }

        AVIFileExit();
    }

    return !kfs.IsEmpty();
}

DVD_HMSF_TIMECODE RT2HMSF(REFERENCE_TIME rt, double fps /*= 0.0*/) // use to remember the current position
{
    DVD_HMSF_TIMECODE hmsf = {
        (BYTE)((rt / 10000000 / 60 / 60)),
        (BYTE)((rt / 10000000 / 60) % 60),
        (BYTE)((rt / 10000000) % 60),
        (BYTE)(1.0 * ((rt / 10000) % 1000) * fps / 1000)
    };

    return hmsf;
}

DVD_HMSF_TIMECODE RT2HMS(REFERENCE_TIME rt) // used only for information (for display on the screen)
{
    rt = rt / 10000000;
    DVD_HMSF_TIMECODE hmsf = {
        (BYTE)(rt / 3600),
        (BYTE)(rt / 60 % 60),
        (BYTE)(rt % 60),
        0
    };

    return hmsf;
}

DVD_HMSF_TIMECODE RT2HMS_r(REFERENCE_TIME rt) // used only for information (for display on the screen)
{
    // round to nearest second
    return RT2HMS(rt + 5000000);
}

REFERENCE_TIME HMSF2RT(DVD_HMSF_TIMECODE hmsf, double fps /*= -1.0*/)
{
    if (fps <= 0.0) {
        hmsf.bFrames = 0;
        fps = 1.0;
    }
    return (REFERENCE_TIME)((((REFERENCE_TIME)hmsf.bHours * 60 + hmsf.bMinutes) * 60 + hmsf.bSeconds) * 1000 + 1.0 * hmsf.bFrames * 1000 / fps) * 10000;
}

void memsetd(void* dst, unsigned int c, size_t nbytes)
{
    size_t n = nbytes / 4;
    __stosd((unsigned long*)dst, c, n);
}

void memsetw(void* dst, unsigned short c, size_t nbytes)
{
    memsetd(dst, c << 16 | c, nbytes);

    size_t n = nbytes / 2;
    size_t o = (n / 2) * 2;
    if ((n - o) == 1) {
        ((WORD*)dst)[o] = c;
    }
}

bool ExtractBIH(const AM_MEDIA_TYPE* pmt, BITMAPINFOHEADER* bih)
{
    if (pmt && bih && pmt->pbFormat) {
        ZeroMemory(bih, sizeof(*bih));

        if (pmt->formattype == FORMAT_VideoInfo) {
            VIDEOINFOHEADER* vih = (VIDEOINFOHEADER*)pmt->pbFormat;
            memcpy(bih, &vih->bmiHeader, sizeof(BITMAPINFOHEADER));
            return true;
        } else if (pmt->formattype == FORMAT_VideoInfo2) {
            VIDEOINFOHEADER2* vih = (VIDEOINFOHEADER2*)pmt->pbFormat;
            memcpy(bih, &vih->bmiHeader, sizeof(BITMAPINFOHEADER));
            return true;
        } else if (pmt->formattype == FORMAT_MPEGVideo) {
            VIDEOINFOHEADER* vih = &((MPEG1VIDEOINFO*)pmt->pbFormat)->hdr;
            memcpy(bih, &vih->bmiHeader, sizeof(BITMAPINFOHEADER));
            return true;
        } else if (pmt->formattype == FORMAT_MPEG2_VIDEO) {
            VIDEOINFOHEADER2* vih = &((MPEG2VIDEOINFO*)pmt->pbFormat)->hdr;
            memcpy(bih, &vih->bmiHeader, sizeof(BITMAPINFOHEADER));
            return true;
        } else if (pmt->formattype == FORMAT_DiracVideoInfo) {
            VIDEOINFOHEADER2* vih = &((DIRACINFOHEADER*)pmt->pbFormat)->hdr;
            memcpy(bih, &vih->bmiHeader, sizeof(BITMAPINFOHEADER));
            return true;
        }
    }

    return false;
}

bool ExtractAvgTimePerFrame(const AM_MEDIA_TYPE* pmt, REFERENCE_TIME& rtAvgTimePerFrame)
{
    if (pmt->formattype == FORMAT_VideoInfo) {
        rtAvgTimePerFrame = ((VIDEOINFOHEADER*)pmt->pbFormat)->AvgTimePerFrame;
    } else if (pmt->formattype == FORMAT_VideoInfo2) {
        rtAvgTimePerFrame = ((VIDEOINFOHEADER2*)pmt->pbFormat)->AvgTimePerFrame;
    } else if (pmt->formattype == FORMAT_MPEGVideo) {
        rtAvgTimePerFrame = ((MPEG1VIDEOINFO*)pmt->pbFormat)->hdr.AvgTimePerFrame;
    } else if (pmt->formattype == FORMAT_MPEG2Video) {
        rtAvgTimePerFrame = ((MPEG2VIDEOINFO*)pmt->pbFormat)->hdr.AvgTimePerFrame;
    } else {
        return false;
    }

    return true;
}

bool ExtractBIH(IMediaSample* pMS, BITMAPINFOHEADER* bih)
{
    AM_MEDIA_TYPE* pmt = nullptr;
    if (SUCCEEDED(pMS->GetMediaType(&pmt)) && pmt) {
        bool fRet = ExtractBIH(pmt, bih);
        DeleteMediaType(pmt);
        return fRet;
    }

    return false;
}

bool ExtractDim(const AM_MEDIA_TYPE* pmt, int& w, int& h, int& arx, int& ary)
{
    w = h = arx = ary = 0;

    if (pmt->formattype == FORMAT_VideoInfo || pmt->formattype == FORMAT_MPEGVideo) {
        VIDEOINFOHEADER* vih = (VIDEOINFOHEADER*)pmt->pbFormat;
        w = vih->bmiHeader.biWidth;
        h = abs(vih->bmiHeader.biHeight);
        arx = w * vih->bmiHeader.biYPelsPerMeter;
        ary = h * vih->bmiHeader.biXPelsPerMeter;
    } else if (pmt->formattype == FORMAT_VideoInfo2 || pmt->formattype == FORMAT_MPEG2_VIDEO || pmt->formattype == FORMAT_DiracVideoInfo) {
        VIDEOINFOHEADER2* vih = (VIDEOINFOHEADER2*)pmt->pbFormat;
        w = vih->bmiHeader.biWidth;
        h = abs(vih->bmiHeader.biHeight);
        arx = vih->dwPictAspectRatioX;
        ary = vih->dwPictAspectRatioY;
    } else {
        return false;
    }

    if (!arx || !ary) {
        BYTE* ptr = nullptr;
        DWORD len = 0;

        if (pmt->formattype == FORMAT_MPEGVideo) {
            ptr = ((MPEG1VIDEOINFO*)pmt->pbFormat)->bSequenceHeader;
            len = ((MPEG1VIDEOINFO*)pmt->pbFormat)->cbSequenceHeader;

            if (ptr && len >= 8 && *(DWORD*)ptr == 0xb3010000) {
                w = (ptr[4] << 4) | (ptr[5] >> 4);
                h = ((ptr[5] & 0xf) << 8) | ptr[6];
                float ar[] = {
                    1.0000f, 1.0000f, 0.6735f, 0.7031f,
                    0.7615f, 0.8055f, 0.8437f, 0.8935f,
                    0.9157f, 0.9815f, 1.0255f, 1.0695f,
                    1.0950f, 1.1575f, 1.2015f, 1.0000f,
                };
                arx = (int)((float)w / ar[ptr[7] >> 4] + 0.5);
                ary = h;
            }
        } else if (pmt->formattype == FORMAT_MPEG2_VIDEO) {
            ptr = (BYTE*)((MPEG2VIDEOINFO*)pmt->pbFormat)->dwSequenceHeader;
            len = ((MPEG2VIDEOINFO*)pmt->pbFormat)->cbSequenceHeader;

            if (ptr && len >= 8 && *(DWORD*)ptr == 0xb3010000) {
                w = (ptr[4] << 4) | (ptr[5] >> 4);
                h = ((ptr[5] & 0xf) << 8) | ptr[6];
                struct {
                    int x, y;
                } ar[] = {{w, h}, {4, 3}, {16, 9}, {221, 100}, {w, h}};
                int i = std::min(std::max(ptr[7] >> 4, 1), 5) - 1;
                arx = ar[i].x;
                ary = ar[i].y;
            }
        }
    }

    if (!arx || !ary) {
        arx = w;
        ary = h;
    }

    DWORD a = arx, b = ary;
    while (a) {
        int tmp = a;
        a = b % tmp;
        b = tmp;
    }
    if (b) {
        arx /= b, ary /= b;
    }

    return true;
}

bool CreateFilter(CStringW DisplayName, IBaseFilter** ppBF, CStringW& FriendlyName)
{
    if (!ppBF) {
        return false;
    }

    *ppBF = nullptr;
    FriendlyName.Empty();

    CComPtr<IBindCtx> pBindCtx;
    CreateBindCtx(0, &pBindCtx);

    CComPtr<IMoniker> pMoniker;
    ULONG chEaten;
    if (S_OK != MkParseDisplayName(pBindCtx, CComBSTR(DisplayName), &chEaten, &pMoniker)) {
        return false;
    }

    if (FAILED(pMoniker->BindToObject(pBindCtx, 0, IID_PPV_ARGS(ppBF))) || !*ppBF) {
        return false;
    }

    CComPtr<IPropertyBag> pPB;
    CComVariant var;
    if (SUCCEEDED(pMoniker->BindToStorage(pBindCtx, 0, IID_PPV_ARGS(&pPB)))
            && SUCCEEDED(pPB->Read(_T("FriendlyName"), &var, nullptr))) {
        FriendlyName = var.bstrVal;
    }

    return true;
}

IBaseFilter* AppendFilter(IPin* pPin, IMoniker* pMoniker, IGraphBuilder* pGB)
{
    do {
        if (!pPin || !pMoniker || !pGB) {
            break;
        }

        CComPtr<IPin> pPinTo;
        PIN_DIRECTION dir;
        if (FAILED(pPin->QueryDirection(&dir)) || dir != PINDIR_OUTPUT || SUCCEEDED(pPin->ConnectedTo(&pPinTo))) {
            break;
        }

        CComPtr<IBindCtx> pBindCtx;
        CreateBindCtx(0, &pBindCtx);

        CComPtr<IPropertyBag> pPB;
        if (FAILED(pMoniker->BindToStorage(pBindCtx, 0, IID_PPV_ARGS(&pPB)))) {
            break;
        }

        CComVariant var;
        if (FAILED(pPB->Read(_T("FriendlyName"), &var, nullptr))) {
            break;
        }

        CComPtr<IBaseFilter> pBF;
        if (FAILED(pMoniker->BindToObject(pBindCtx, 0, IID_PPV_ARGS(&pBF))) || !pBF) {
            break;
        }

        if (FAILED(pGB->AddFilter(pBF, CStringW(var.bstrVal)))) {
            break;
        }

        BeginEnumPins(pBF, pEP, pPinTo2) {
            PIN_DIRECTION dir2;
            if (FAILED(pPinTo2->QueryDirection(&dir2)) || dir2 != PINDIR_INPUT) {
                continue;
            }

            if (SUCCEEDED(pGB->ConnectDirect(pPin, pPinTo2, nullptr))) {
                return pBF;
            }
        }
        EndEnumFilters;

        pGB->RemoveFilter(pBF);
    } while (false);

    return nullptr;
}

CStringW GetFriendlyName(CStringW displayName)
{
    CStringW friendlyName;

    CComPtr<IBindCtx> pBindCtx;
    CreateBindCtx(0, &pBindCtx);

    CComPtr<IMoniker> pMoniker;
    ULONG chEaten;
    if (S_OK == MkParseDisplayName(pBindCtx, CComBSTR(displayName), &chEaten, &pMoniker)) {
        CComPtr<IPropertyBag> pPB;
        CComVariant var;
        if (SUCCEEDED(pMoniker->BindToStorage(pBindCtx, 0, IID_PPV_ARGS(&pPB)))
                && SUCCEEDED(pPB->Read(_T("FriendlyName"), &var, nullptr))) {
            friendlyName = var.bstrVal;
        }
    }

    return friendlyName;
}

typedef HRESULT(__stdcall* fDllCanUnloadNow)(void);

struct ExternalObject {
    CString path;
    HINSTANCE hInst;
    CLSID clsid;
    fDllCanUnloadNow fpDllCanUnloadNow;
    bool bUnloadOnNextCheck;
};

static CAtlList<ExternalObject> s_extObjs;
static CCritSec s_csExtObjs;

HRESULT LoadExternalObject(LPCTSTR path, REFCLSID clsid, REFIID iid, void** ppv, IUnknown* aggregate)
{
    CheckPointer(ppv, E_POINTER);

    CAutoLock lock(&s_csExtObjs);

    CString fullpath = MakeFullPath(path);

    HINSTANCE hInst = nullptr;
    bool fFound = false;

    POSITION pos = s_extObjs.GetHeadPosition();
    while (pos) {
        ExternalObject& eo = s_extObjs.GetNext(pos);
        if (!eo.path.CompareNoCase(fullpath)) {
            hInst = eo.hInst;
            fFound = true;
            eo.bUnloadOnNextCheck = false;
            break;
        }
    }

    HRESULT hr = E_FAIL;

    if (!hInst) {
        hInst = LoadLibraryEx(fullpath, nullptr, LOAD_WITH_ALTERED_SEARCH_PATH);
    }
    if (hInst) {
        typedef HRESULT(__stdcall * PDllGetClassObject)(REFCLSID rclsid, REFIID riid, LPVOID * ppv);
        PDllGetClassObject p = (PDllGetClassObject)GetProcAddress(hInst, "DllGetClassObject");

        if (p && (aggregate || FAILED(hr = p(clsid, iid, ppv)))) {
            CComPtr<IClassFactory> pCF;
            if (SUCCEEDED(hr = p(clsid, IID_PPV_ARGS(&pCF)))) {
                hr = pCF->CreateInstance(aggregate, iid, ppv);
            }
        }
    }

    if (FAILED(hr) && hInst && !fFound) {
        FreeLibrary(hInst);
        return hr;
    }

    if (hInst && !fFound) {
        ExternalObject eo;
        eo.path = fullpath;
        eo.hInst = hInst;
        eo.clsid = clsid;
        eo.fpDllCanUnloadNow = (fDllCanUnloadNow)GetProcAddress(hInst, "DllCanUnloadNow");
        eo.bUnloadOnNextCheck = false;
        s_extObjs.AddTail(eo);
    }

    return hr;
}

HRESULT LoadExternalFilter(LPCTSTR path, REFCLSID clsid, IBaseFilter** ppBF)
{
    return LoadExternalObject(path, clsid, IID_PPV_ARGS(ppBF));
}

HRESULT LoadExternalPropertyPage(IPersist* pP, REFCLSID clsid, IPropertyPage** ppPP)
{
    CAutoLock lock(&s_csExtObjs);

    CLSID clsid2 = GUID_NULL;
    if (FAILED(pP->GetClassID(&clsid2))) {
        return E_FAIL;
    }

    POSITION pos = s_extObjs.GetHeadPosition();
    while (pos) {
        ExternalObject& eo = s_extObjs.GetNext(pos);
        if (eo.clsid == clsid2) {
            return LoadExternalObject(eo.path, clsid, IID_PPV_ARGS(ppPP));
        }
    }

    return E_FAIL;
}

bool UnloadUnusedExternalObjects()
{
    CAutoLock lock(&s_csExtObjs);

    POSITION pos = s_extObjs.GetHeadPosition(), currentPos;
    while (pos) {
        currentPos = pos;
        ExternalObject& eo = s_extObjs.GetNext(pos);

        if (eo.fpDllCanUnloadNow && eo.fpDllCanUnloadNow() == S_OK) {
            // Before actually unloading it, we require that the library reports
            // that it can be unloaded safely twice in a row with a 60s delay
            // between the two checks.
            if (eo.bUnloadOnNextCheck) {
                FreeLibrary(eo.hInst);
                s_extObjs.RemoveAt(currentPos);
            } else {
                eo.bUnloadOnNextCheck = true;
            }
        } else {
            eo.bUnloadOnNextCheck = false;
        }
    }

    return s_extObjs.IsEmpty();
}

void ExtendMaxPathLengthIfNeeded(CString& path, bool no_url /*= false */)
{
    if (!no_url && path.Find(_T("://")) >= 0) { // URL
        return;
    }

    // Get long path if shortened
    if (path.Find(L'~') > 0) {
        wchar_t destbuf[4096];
        DWORD len = GetLongPathName(path, destbuf, 4096);
        if (len > 0 && len < 4096) {
            path = destbuf;
        }
    }
    if (path.GetLength() >= MAX_PATH) {
        if (path.Left(4) != _T("\\\\?\\")) { // not already have long path prefix
            if (path.Left(2) == _T("\\\\")) { // UNC
                path = _T("\\\\?\\UNC") + path.Mid(1);
            } else { // normal
                path = _T("\\\\?\\") + path;
            }
        }
    }
}

bool ContainsWildcard(CString& path)
{
    int p = path.Find('*');
    if (p >= 0) {
        return true;
    }
    p = path.Find('?');
    if (p >= 0) {
        if (p == 2 && path.Left(4) == _T("\\\\?\\")) {
            CString tmp = CString(path);
            tmp.Delete(0, 3);
            return tmp.Find('?') > 0;
        }
        return true;
    }
    return false;
}

void ShortenLongPath(CString& path)
{
    if (path.GetLength() > MAX_PATH && path.Find(_T("\\\\?\\")) < 0) {
        CString longpath = _T("\\\\?\\") + path;
        TCHAR* buffer = DEBUG_NEW TCHAR[MAX_PATH];
        long length = GetShortPathName(longpath, buffer, MAX_PATH);
        if (length > 0 && length < MAX_PATH) {
            path = buffer;
            path.Replace(_T("\\\\?\\"), _T(""));
            delete[] buffer;
        }
    }
}

CString MakeFullPath(LPCTSTR path)
{
    CString full(path);
    full.Replace('/', '\\');

    if (full.GetLength() > MAX_PATH) {
        return full;
    }

    if (full.GetLength() >= 2 && full[0] == '\\') {
        if (full[1] != '\\') {
            CString fn;
            fn.ReleaseBuffer(GetModuleFileName(AfxGetInstanceHandle(), fn.GetBuffer(MAX_PATH), MAX_PATH));
            CPath p(fn);
            p.StripToRoot();
            full = CString(p) + full.Mid(1);
        }
    } else if (full.Find(_T(":\\")) < 0) {
        CString fn;
        fn.ReleaseBuffer(GetModuleFileName(AfxGetInstanceHandle(), fn.GetBuffer(MAX_PATH), MAX_PATH));
        CPath p(fn);
        p.RemoveFileSpec();
        p.AddBackslash();
        full = CString(p) + full;
    }

    CPath c(full);
    c.Canonicalize();
    return CString(c);
}

inline bool _IsFourCC(const GUID& guid)
{
    // XXXXXXXX-0000-0010-8000-00AA00389B71
    return (guid.Data2 == 0x0000) && (guid.Data3 == 0x0010) &&
        (((DWORD*)guid.Data4)[0] == 0xAA000080) &&
        (((DWORD*)guid.Data4)[1] == 0x719b3800);
}

bool GetMediaTypeFourCC(const GUID& guid, CString& fourCC)
{
    if (_IsFourCC(guid) && (guid.Data1 >= 0x10000)) {
        fourCC.Format(_T("%c%c%c%c"),
            (TCHAR)(guid.Data1 >> 0 ) & 0xFF, (TCHAR)(guid.Data1 >> 8 ) & 0xFF,
            (TCHAR)(guid.Data1 >> 16) & 0xFF, (TCHAR)(guid.Data1 >> 24) & 0xFF);
        fourCC.MakeUpper();
        return true;
    }

    fourCC = _T("UNKN");
    return false;
}

CString GetMediaTypeName(const GUID& guid)
{
    CString ret = guid == GUID_NULL
                  ? CString(_T("Any type"))
                  : CString(GuidNames[guid]);

    if (ret == _T("FOURCC GUID")) {
        CString str;
        if (guid.Data1 >= 0x10000) {
            str.Format(_T("Video: %c%c%c%c"), (guid.Data1 >> 0) & 0xff, (guid.Data1 >> 8) & 0xff, (guid.Data1 >> 16) & 0xff, (guid.Data1 >> 24) & 0xff);
        } else {
            str.Format(_T("Audio: 0x%08x"), guid.Data1);
        }
        ret = str;
    } else if (ret == _T("Unknown GUID Name")) {
        WCHAR null[128] = {0}, buff[128];
        StringFromGUID2(GUID_NULL, null, _countof(null) - 1);
        ret = CString(CStringW(StringFromGUID2(guid, buff, _countof(buff) - 1) ? buff : null));
    }

    return ret;
}

GUID GUIDFromCString(CString str)
{
    GUID guid = GUID_NULL;
    HRESULT hr = CLSIDFromString(CComBSTR(str), &guid);
    ASSERT(SUCCEEDED(hr));
    UNREFERENCED_PARAMETER(hr);
    return guid;
}

HRESULT GUIDFromCString(CString str, GUID& guid)
{
    guid = GUID_NULL;
    return CLSIDFromString(CComBSTR(str), &guid);
}

CString CStringFromGUID(const GUID& guid)
{
    WCHAR null[128] = {0}, buff[128];
    StringFromGUID2(GUID_NULL, null, _countof(null) - 1);
    return CString(StringFromGUID2(guid, buff, _countof(buff) - 1) > 0 ? buff : null);
}

CStringW UTF8To16(LPCSTR utf8)
{
    CStringW str;
    int n = MultiByteToWideChar(CP_UTF8, 0, utf8, -1, nullptr, 0) - 1;
    if (n <= 0) {
        return str;
    }
    str.ReleaseBuffer(MultiByteToWideChar(CP_UTF8, 0, utf8, -1, str.GetBuffer(n), n + 1) - 1);
    return str;
}

CStringA UTF16To8(LPCWSTR utf16)
{
    CStringA str;
    int n = WideCharToMultiByte(CP_UTF8, 0, utf16, -1, nullptr, 0, nullptr, nullptr) - 1;
    if (n < 0) {
        return str;
    }
    str.ReleaseBuffer(WideCharToMultiByte(CP_UTF8, 0, utf16, -1, str.GetBuffer(n), n + 1, nullptr, nullptr) - 1);
    return str;
}

CStringW UTF8ToStringW(const char* S)
{
    CStringW str;
    if (S == nullptr) {
        return str;
    }

    // Don't use MultiByteToWideChar(), some characters are not well decoded
    const unsigned char* Z = (const unsigned char*)S;
    while (*Z) { //0 is end
        //1 byte
        if (*Z < 0x80) {
            str += (wchar_t)(*Z);
            Z++;
        }
        //2 bytes
        else if ((*Z & 0xE0) == 0xC0) {
            if ((*(Z + 1) & 0xC0) == 0x80) {
                str += (wchar_t)((((wchar_t)(*Z & 0x1F)) << 6) | (*(Z + 1) & 0x3F));
                Z += 2;
            } else {
                str.Empty();
                return str; //Bad character
            }
        }
        //3 bytes
        else if ((*Z & 0xF0) == 0xE0) {
            if ((*(Z + 1) & 0xC0) == 0x80 && (*(Z + 2) & 0xC0) == 0x80) {
                str += (wchar_t)((((wchar_t)(*Z & 0x0F)) << 12) | ((*(Z + 1) & 0x3F) << 6) | (*(Z + 2) & 0x3F));
                Z += 3;
            } else {
                str.Empty();
                return str; //Bad character
            }
        }
        //4 bytes
        else if ((*Z & 0xF8) == 0xF0) {
            if ((*(Z + 1) & 0xC0) == 0x80 && (*(Z + 2) & 0xC0) == 0x80 && (*(Z + 3) & 0xC0) == 0x80) {
                str += (wchar_t)((((wchar_t)(*Z & 0x0F)) << 18) | ((*(Z + 1) & 0x3F) << 12) || ((*(Z + 2) & 0x3F) << 6) | (*(Z + 3) & 0x3F));
                Z += 4;
            } else {
                str.Empty();
                return str; //Bad character
            }
        } else {
            str.Empty();
            return str; //Bad character
        }
    }
    return str;
}

CStringW LocalToStringW(const char* S)
{
    CStringW str;
    if (S == nullptr) {
        return str;
    }

    int Size = MultiByteToWideChar(CP_ACP, 0, S, -1, nullptr, 0);
    if (Size != 0) {
        str.ReleaseBuffer(MultiByteToWideChar(CP_ACP, 0, S, -1, str.GetBuffer(Size), Size + 1) - 1);
    }
    return str;
}

BOOL CFileGetStatus(LPCTSTR lpszFileName, CFileStatus& status)
{
    try {
        return CFile::GetStatus(lpszFileName, status);
    } catch (CException* e) {
        // MFCBUG: E_INVALIDARG / "Parameter is incorrect" is thrown for certain cds (vs2003)
        // http://groups.google.co.uk/groups?hl=en&lr=&ie=UTF-8&threadm=OZuXYRzWDHA.536%40TK2MSFTNGP10.phx.gbl&rnum=1&prev=/groups%3Fhl%3Den%26lr%3D%26ie%3DISO-8859-1
        TRACE(_T("CFile::GetStatus has thrown an exception\n"));
        e->Delete();
        return false;
    }
}

// filter registration helpers

bool DeleteRegKey(LPCTSTR pszKey, LPCTSTR pszSubkey)
{
    bool bOK = false;

    HKEY hKey;
    LONG ec = ::RegOpenKeyEx(HKEY_CLASSES_ROOT, pszKey, 0, KEY_ALL_ACCESS, &hKey);
    if (ec == ERROR_SUCCESS) {
        if (pszSubkey != 0) {
            ec = ::RegDeleteKey(hKey, pszSubkey);
        }

        bOK = (ec == ERROR_SUCCESS);

        ::RegCloseKey(hKey);
    }

    return bOK;
}

bool SetRegKeyValue(LPCTSTR pszKey, LPCTSTR pszSubkey, LPCTSTR pszValueName, LPCTSTR pszValue)
{
    bool bOK = false;

    CString szKey(pszKey);
    if (pszSubkey != 0) {
        szKey += CString(_T("\\")) + pszSubkey;
    }

    HKEY hKey;
    LONG ec = ::RegCreateKeyEx(HKEY_CLASSES_ROOT, szKey, 0, 0, REG_OPTION_NON_VOLATILE, KEY_ALL_ACCESS, 0, &hKey, 0);
    if (ec == ERROR_SUCCESS) {
        if (pszValue != 0) {
            ec = ::RegSetValueEx(hKey, pszValueName, 0, REG_SZ,
                                 reinterpret_cast<BYTE*>(const_cast<LPTSTR>(pszValue)),
                                 (DWORD)(_tcslen(pszValue) + 1) * sizeof(TCHAR));
        }

        bOK = (ec == ERROR_SUCCESS);

        ::RegCloseKey(hKey);
    }

    return bOK;
}

bool SetRegKeyValue(LPCTSTR pszKey, LPCTSTR pszSubkey, LPCTSTR pszValue)
{
    return SetRegKeyValue(pszKey, pszSubkey, 0, pszValue);
}

void RegisterSourceFilter(const CLSID& clsid, const GUID& subtype2, LPCTSTR chkbytes, LPCTSTR ext, ...)
{
    CString null = CStringFromGUID(GUID_NULL);
    CString majortype = CStringFromGUID(MEDIATYPE_Stream);
    CString subtype = CStringFromGUID(subtype2);

    SetRegKeyValue(_T("Media Type\\") + majortype, subtype, _T("0"), chkbytes);
    SetRegKeyValue(_T("Media Type\\") + majortype, subtype, _T("Source Filter"), CStringFromGUID(clsid));

    DeleteRegKey(_T("Media Type\\") + null, subtype);

    va_list marker;
    va_start(marker, ext);
    for (; ext; ext = va_arg(marker, LPCTSTR)) {
        DeleteRegKey(_T("Media Type\\Extensions"), ext);
    }
    va_end(marker);
}

void RegisterSourceFilter(const CLSID& clsid, const GUID& subtype2, const CAtlList<CString>& chkbytes, LPCTSTR ext, ...)
{
    CString null = CStringFromGUID(GUID_NULL);
    CString majortype = CStringFromGUID(MEDIATYPE_Stream);
    CString subtype = CStringFromGUID(subtype2);

    POSITION pos = chkbytes.GetHeadPosition();
    for (ptrdiff_t i = 0; pos; i++) {
        CString idx;
        idx.Format(_T("%Id"), i);
        SetRegKeyValue(_T("Media Type\\") + majortype, subtype, idx, chkbytes.GetNext(pos));
    }

    SetRegKeyValue(_T("Media Type\\") + majortype, subtype, _T("Source Filter"), CStringFromGUID(clsid));

    DeleteRegKey(_T("Media Type\\") + null, subtype);

    va_list marker;
    va_start(marker, ext);
    for (; ext; ext = va_arg(marker, LPCTSTR)) {
        DeleteRegKey(_T("Media Type\\Extensions"), ext);
    }
    va_end(marker);
}

void UnRegisterSourceFilter(const GUID& subtype)
{
    DeleteRegKey(_T("Media Type\\") + CStringFromGUID(MEDIATYPE_Stream), CStringFromGUID(subtype));
}

struct DXVA2_DECODER {
    const GUID* Guid;
    LPCTSTR Description;
};

static const DXVA2_DECODER DXVA2Decoder[] = {
    {&GUID_NULL,                        _T("Unknown")},
    {&GUID_NULL,                        _T("Not using DXVA")},
    {&DXVA_Intel_H264_ClearVideo,       _T("H.264 bitstream decoder, ClearVideo(tm)")},  // Intel ClearVideo H264 bitstream decoder
    {&DXVA_Intel_VC1_ClearVideo,        _T("VC-1 bitstream decoder, ClearVideo(tm)")},   // Intel ClearVideo VC-1 bitstream decoder
    {&DXVA_Intel_VC1_ClearVideo_2,      _T("VC-1 bitstream decoder 2, ClearVideo(tm)")}, // Intel ClearVideo VC-1 bitstream decoder 2
    {&DXVA_MPEG4_ASP,                   _T("MPEG-4 ASP bitstream decoder")},             // NVIDIA MPEG-4 ASP bitstream decoder
    {&DXVA_ModeNone,                    _T("Mode none")},
    {&DXVA_ModeH261_A,                  _T("H.261 A, post processing")},
    {&DXVA_ModeH261_B,                  _T("H.261 B, deblocking")},
    {&DXVA_ModeH263_A,                  _T("H.263 A, motion compensation, no FGT")},
    {&DXVA_ModeH263_B,                  _T("H.263 B, motion compensation, FGT")},
    {&DXVA_ModeH263_C,                  _T("H.263 C, IDCT, no FGT")},
    {&DXVA_ModeH263_D,                  _T("H.263 D, IDCT, FGT")},
    {&DXVA_ModeH263_E,                  _T("H.263 E, bitstream decoder, no FGT")},
    {&DXVA_ModeH263_F,                  _T("H.263 F, bitstream decoder, FGT")},
    {&DXVA_ModeMPEG1_A,                 _T("MPEG-1 A, post processing")},
    {&DXVA_ModeMPEG2_A,                 _T("MPEG-2 A, motion compensation")},
    {&DXVA_ModeMPEG2_B,                 _T("MPEG-2 B, motion compensation + blending")},
    {&DXVA_ModeMPEG2_C,                 _T("MPEG-2 C, IDCT")},
    {&DXVA_ModeMPEG2_D,                 _T("MPEG-2 D, IDCT + blending")},
    {&DXVA_ModeH264_A,                  _T("H.264 A, motion compensation, no FGT")},
    {&DXVA_ModeH264_B,                  _T("H.264 B, motion compensation, FGT")},
    {&DXVA_ModeH264_C,                  _T("H.264 C, IDCT, no FGT")},
    {&DXVA_ModeH264_D,                  _T("H.264 D, IDCT, FGT")},
    {&DXVA_ModeH264_E,                  _T("H.264 E, bitstream decoder, no FGT")},
    {&DXVA_ModeH264_F,                  _T("H.264 F, bitstream decoder, FGT")},
    {&DXVA_ModeWMV8_A,                  _T("WMV8 A, post processing")},
    {&DXVA_ModeWMV8_B,                  _T("WMV8 B, motion compensation")},
    {&DXVA_ModeWMV9_A,                  _T("WMV9 A, post processing")},
    {&DXVA_ModeWMV9_B,                  _T("WMV9 B, motion compensation")},
    {&DXVA_ModeWMV9_C,                  _T("WMV9 C, IDCT")},
    {&DXVA_ModeVC1_A,                   _T("VC-1 A, post processing")},
    {&DXVA_ModeVC1_B,                   _T("VC-1 B, motion compensation")},
    {&DXVA_ModeVC1_C,                   _T("VC-1 C, IDCT")},
    {&DXVA_ModeVC1_D,                   _T("VC-1 D, bitstream decoder")},
    {&DXVA_NoEncrypt,                   _T("No encryption")},
    {&DXVA2_ModeMPEG2_MoComp,           _T("MPEG-2 motion compensation")},
    {&DXVA2_ModeMPEG2_IDCT,             _T("MPEG-2 IDCT")},
    {&DXVA2_ModeMPEG2_VLD,              _T("MPEG-2 variable-length decoder")},
    {&DXVA2_ModeH264_A,                 _T("H.264 A, motion compensation, no FGT")},
    {&DXVA2_ModeH264_B,                 _T("H.264 B, motion compensation, FGT")},
    {&DXVA2_ModeH264_C,                 _T("H.264 C, IDCT, no FGT")},
    {&DXVA2_ModeH264_D,                 _T("H.264 D, IDCT, FGT")},
    {&DXVA2_ModeH264_E,                 _T("H.264 E, bitstream decoder, no FGT")},
    {&DXVA2_ModeH264_F,                 _T("H.264 F, bitstream decoder, FGT")},
    {&DXVA2_ModeWMV8_A,                 _T("WMV8 A, post processing")},
    {&DXVA2_ModeWMV8_B,                 _T("WMV8 B, motion compensation")},
    {&DXVA2_ModeWMV9_A,                 _T("WMV9 A, post processing")},
    {&DXVA2_ModeWMV9_B,                 _T("WMV9 B, motion compensation")},
    {&DXVA2_ModeWMV9_C,                 _T("WMV9 C, IDCT")},
    {&DXVA2_ModeVC1_A,                  _T("VC-1 A, post processing")},
    {&DXVA2_ModeVC1_B,                  _T("VC-1 B, motion compensation")},
    {&DXVA2_ModeVC1_C,                  _T("VC-1 C, IDCT")},
    {&DXVA2_ModeVC1_D,                  _T("VC-1 D, bitstream decoder")},
    {&DXVA2_NoEncrypt,                  _T("No encryption")},
    {&DXVA2_VideoProcProgressiveDevice, _T("Progressive scan")},
    {&DXVA2_VideoProcBobDevice,         _T("Bob deinterlacing")},
    {&DXVA2_VideoProcSoftwareDevice,    _T("Software processing")}
};

LPCTSTR GetDXVAMode(const GUID* guidDecoder)
{
    int nPos = 0;

    for (int i = 1; i < _countof(DXVA2Decoder); i++) {
        if (*guidDecoder == *DXVA2Decoder[i].Guid) {
            nPos = i;
            break;
        }
    }

    return DXVA2Decoder[nPos].Description;
}

// hour, minute, second, millisec
CString ReftimeToString(const REFERENCE_TIME& rtVal)
{
    if (rtVal == _I64_MIN) {
        return _T("INVALID TIME");
    }

    CString strTemp;
    LONGLONG llTotalMs = ConvertToMilliseconds(rtVal);
    int lHour     = (int)(llTotalMs / (1000 * 60 * 60));
    int lMinute   = (llTotalMs / (1000 * 60)) % 60;
    int lSecond   = (llTotalMs /  1000) % 60;
    int lMillisec = llTotalMs  %  1000;

    strTemp.Format(_T("%02d:%02d:%02d,%03d"), lHour, lMinute, lSecond, lMillisec);
    return strTemp;
}

// hour, minute, second (round)
CString ReftimeToString2(const REFERENCE_TIME& rtVal)
{
    CString strTemp;
    LONGLONG seconds = (rtVal + 5000000) / 10000000;
    int lHour   = (int)(seconds / 3600);
    int lMinute = (int)(seconds / 60 % 60);
    int lSecond = (int)(seconds % 60);

    strTemp.Format(_T("%02d:%02d:%02d"), lHour, lMinute, lSecond);
    return strTemp;
}

// minute, second (round)
CString ReftimeToString3(const REFERENCE_TIME& rtVal)
{
    CString strTemp;
    LONGLONG seconds = (rtVal + 5000000) / 10000000;
    int lMinute = (int)(seconds / 60 % 60);
    int lSecond = (int)(seconds % 60);

    ASSERT((int)(seconds / 3600) == 0);

    strTemp.Format(_T("%02d:%02d"), lMinute, lSecond);
    return strTemp;
}

//for compatibility with mpc-be ReftimeToString2, which has option to exclude hours
CStringW ReftimeToString4(REFERENCE_TIME rt, bool showZeroHours /* = true*/)
{
    if (rt == INT64_MIN) {
        return L"INVALID TIME";
    }

    DVD_HMSF_TIMECODE tc = RT2HMSF(rt);

    return DVDtimeToString(tc, showZeroHours);
}

CString DVDtimeToString(const DVD_HMSF_TIMECODE& rtVal, bool bAlwaysShowHours)
{
    CString strTemp;
    if (rtVal.bHours > 0 || bAlwaysShowHours) {
        strTemp.Format(_T("%02u:%02u:%02u"), rtVal.bHours, rtVal.bMinutes, rtVal.bSeconds);
    } else {
        strTemp.Format(_T("%02u:%02u"), rtVal.bMinutes, rtVal.bSeconds);
    }
    return strTemp;
}

REFERENCE_TIME StringToReftime(LPCTSTR strVal)
{
    REFERENCE_TIME rt = 0;
    int lHour = 0;
    int lMinute = 0;
    int lSecond = 0;
    int lMillisec = 0;

    if (_stscanf_s(strVal, _T("%02d:%02d:%02d,%03d"), &lHour, &lMinute, &lSecond, &lMillisec) == 4) {
        rt = ((((lHour * 60) + lMinute) * 60 + lSecond) * MILLISECONDS + lMillisec) * (UNITS / MILLISECONDS);
    }

    return rt;
}

const wchar_t* StreamTypeToName(PES_STREAM_TYPE _Type)
{
    switch (_Type) {
        case VIDEO_STREAM_MPEG1:
            return L"MPEG-1";
        case VIDEO_STREAM_MPEG2:
            return L"MPEG-2";
        case AUDIO_STREAM_MPEG1:
            return L"MPEG-1";
        case AUDIO_STREAM_MPEG2:
            return L"MPEG-2";
        case VIDEO_STREAM_H264:
            return L"H264";
        case VIDEO_STREAM_HEVC:
            return L"HEVC";
        case AUDIO_STREAM_LPCM:
            return L"LPCM";
        case AUDIO_STREAM_AC3:
            return L"Dolby Digital";
        case AUDIO_STREAM_DTS:
            return L"DTS";
        case AUDIO_STREAM_AC3_TRUE_HD:
            return L"Dolby TrueHD";
        case AUDIO_STREAM_AC3_PLUS:
            return L"Dolby Digital Plus";
        case AUDIO_STREAM_DTS_HD:
            return L"DTS-HD High Resolution Audio";
        case AUDIO_STREAM_DTS_HD_MASTER_AUDIO:
            return L"DTS-HD Master Audio";
        case PRESENTATION_GRAPHICS_STREAM:
            return L"Presentation Graphics Stream";
        case INTERACTIVE_GRAPHICS_STREAM:
            return L"Interactive Graphics Stream";
        case SUBTITLE_STREAM:
            return L"Subtitle";
        case SECONDARY_AUDIO_AC3_PLUS:
            return L"Secondary Dolby Digital Plus";
        case SECONDARY_AUDIO_DTS_HD:
            return L"Secondary DTS-HD High Resolution Audio";
        case VIDEO_STREAM_VC1:
            return L"VC-1";
    }
    return nullptr;
}

//
// Usage: SetThreadName (-1, "MainThread");
// Code from http://msdn.microsoft.com/en-us/library/xcb2z8hs%28v=vs.110%29.aspx
//

#pragma pack(push,8)
typedef struct tagTHREADNAME_INFO {
    DWORD  dwType;      // must be 0x1000
    LPCSTR szName;      // pointer to name (in user addr space)
    DWORD  dwThreadID;  // thread ID (-1 caller thread)
    DWORD  dwFlags;     // reserved for future use, must be zero
} THREADNAME_INFO;
#pragma pack(pop)

void SetThreadName(DWORD dwThreadID, LPCSTR szThreadName)
{
    THREADNAME_INFO info;
    info.dwType     = 0x1000;
    info.szName     = szThreadName;
    info.dwThreadID = dwThreadID;
    info.dwFlags    = 0;

    __try {
        RaiseException(0x406D1388, 0, sizeof(info) / sizeof(ULONG_PTR), (ULONG_PTR*)&info);
    } __except (EXCEPTION_EXECUTE_HANDLER) {
    }
}

void CorrectComboBoxHeaderWidth(CWnd* pComboBox)
{
    if (!pComboBox) {
        return;
    }

    CDC*   pDC = pComboBox->GetDC();
    CFont* pFont = pComboBox->GetFont();
    CFont* pOldFont = pDC->SelectObject(pFont);

    CString str;
    pComboBox->GetWindowText(str);
    CSize szText = pDC->GetTextExtent(str);

    TEXTMETRIC tm;
    pDC->GetTextMetrics(&tm);
    pDC->SelectObject(pOldFont);
    pComboBox->ReleaseDC(pDC);

    CRect r;
    pComboBox->GetWindowRect(r);
    pComboBox->GetOwner()->ScreenToClient(r);

    r.right = r.left + ::GetSystemMetrics(SM_CXMENUCHECK) + ::GetSystemMetrics(SM_CXEDGE) + szText.cx + tm.tmAveCharWidth;
    pComboBox->MoveWindow(r);
}

CString NormalizeUnicodeStrForSearch(CString srcStr, LANGID langid) {
    if (srcStr.IsEmpty()) return srcStr;
    wchar_t* src;

    _locale_t locale;
    LCID lcid = MAKELCID(MAKELANGID(langid, SUBLANG_DEFAULT), SORT_DEFAULT);
    wchar_t localeName[32];
    if (0 == LCIDToLocaleName(lcid, localeName, 32, LOCALE_ALLOW_NEUTRAL_NAMES)) { //try to lowercase by locale, but if not, do a regular MakeLower()
        srcStr.MakeLower();
        src = srcStr.GetBuffer();
    } else {
        src = srcStr.GetBuffer();
        locale = _wcreate_locale(LC_ALL, localeName);
        _wcslwr_s_l(src, wcslen(src) + 1, locale);
    }

    int dstLen = int(wcslen(src) * 4);
    wchar_t* dest = DEBUG_NEW wchar_t[dstLen];

    int cchActual = NormalizeString(NormalizationKD, src, -1, dest, dstLen);
    if (cchActual <= 0) dest[0] = 0;
    WORD* rgType = DEBUG_NEW WORD[dstLen];
    GetStringTypeW(CT_CTYPE3, dest, -1, rgType);
    PWSTR pszWrite = dest;
    for (int i = 0; dest[i]; i++) {
        if (!(rgType[i] & C3_NONSPACING)) {
            *pszWrite++ = dest[i];
        }
    }
    *pszWrite = 0;
    delete[] rgType;

    CString ret = dest;
    delete[] dest;
    return ret;
}

inline const LONGLONG GetPerfCounter() {
    auto GetPerfFrequency = [] {
        LARGE_INTEGER freq;
        QueryPerformanceFrequency(&freq);
        return freq.QuadPart;
    };
    static const LONGLONG llPerfFrequency = GetPerfFrequency();
    if (llPerfFrequency) {
        LARGE_INTEGER llPerfCounter;
        QueryPerformanceCounter(&llPerfCounter);
        return llMulDiv(llPerfCounter.QuadPart, 10000000LL, llPerfFrequency, 0);
    } else {
        // ms to 100ns units
        return timeGetTime() * 10000;
    }
}

bool FindStringInList(const CAtlList<CString>& list, CString& value)
{
    bool found = false;
    POSITION pos = list.GetHeadPosition();
    while (pos && !found) {
        if (list.GetNext(pos).CompareNoCase(value) == 0) {
            found = true;
        }
    }
    return found;
}

CStringW ForceTrailingSlash(CStringW folder) {
    if (folder.Right(1) != L'\\' && folder.GetLength() > 0) {
        folder += L'\\';
    }
    return folder;
}

CStringW GetChannelStrFromMediaType(AM_MEDIA_TYPE* pmt) {
    int discard;
    return GetChannelStrFromMediaType(pmt, discard);
}

CStringW ChannelsToStr(int channels) {
    CStringW ret;
    switch (channels) {
        case 1:
            return L"mono";
        case 2:
            return L"2.0";
        case 6:
            return L"5.1";
        case 7:
            return L"6.1";
        case 8:
            return L"7.1";
        default:
            ret.Format(L"%uch", channels);
            return ret;
    }
}

CStringW GetChannelStrFromMediaType(AM_MEDIA_TYPE* pmt, int& channels) {
    if (pmt) {
        if (pmt->majortype == MEDIATYPE_Audio) {
            if (pmt->formattype == FORMAT_WaveFormatEx) {
                channels = ((WAVEFORMATEX*)pmt->pbFormat)->nChannels;
                return ChannelsToStr(channels);
            } else if (pmt->formattype == FORMAT_VorbisFormat) {
                channels = ((VORBISFORMAT*)pmt->pbFormat)->nChannels;
                return ChannelsToStr(channels);
            } else if (pmt->formattype == FORMAT_VorbisFormat2) {
                channels = ((VORBISFORMAT2*)pmt->pbFormat)->Channels;
                return ChannelsToStr(channels);
            } else if (pmt->formattype == FORMAT_WaveFormatExFFMPEG) {
                WAVEFORMATEXFFMPEG* wfeff = (WAVEFORMATEXFFMPEG*)pmt->pbFormat;
                WAVEFORMATEX wfe = wfeff->wfex;
                channels = wfe.nChannels;
                return ChannelsToStr(channels);
            } else {
                channels = 2;
            }
        } else if (pmt->majortype == MEDIATYPE_Midi) {
            channels = 2;
            return L"";
        }
    }
    ASSERT(false);
    return L"";
}

CStringW GetShortAudioNameFromMediaType(AM_MEDIA_TYPE* pmt) {
    if (!pmt) {
        return L"";
    }
    if (pmt->majortype != MEDIATYPE_Audio) {
        if (pmt->majortype == MEDIATYPE_Midi) {
            return L"MIDI";
        } else {
            return L"";
        }
    }

    if (pmt->subtype == MEDIASUBTYPE_AAC || pmt->subtype == MEDIASUBTYPE_LATM_AAC || pmt->subtype == MEDIASUBTYPE_AAC_ADTS || pmt->subtype == MEDIASUBTYPE_MPEG_ADTS_AAC
        || pmt->subtype == MEDIASUBTYPE_MPEG_HEAAC || pmt->subtype == MEDIASUBTYPE_MP4A || pmt->subtype == MEDIASUBTYPE_mp4a) {
        return L"AAC";
    } else if (pmt->subtype == MEDIASUBTYPE_DOLBY_AC3 || pmt->subtype == MEDIASUBTYPE_WAVE_DOLBY_AC3 || pmt->subtype == MEDIASUBTYPE_DOLBY_AC3_SPDIF
        || pmt->subtype == MEDIASUBTYPE_RAW_SPORT || pmt->subtype == MEDIASUBTYPE_SPDIF_TAG_241h || pmt->subtype == MEDIASUBTYPE_DVM) {
        return L"AC3";
    } else if (pmt->subtype == MEDIASUBTYPE_DOLBY_DDPLUS) {
        return L"E-AC3";
    } else if (pmt->subtype == MEDIASUBTYPE_DTS || pmt->subtype == MEDIASUBTYPE_DTS2 || pmt->subtype == MEDIASUBTYPE_WAVE_DTS) {
        return L"DTS";
    } else if (pmt->subtype == MEDIASUBTYPE_DTS_HD) {
        return L"DTS-HD";
    } else if (pmt->subtype == MEDIASUBTYPE_DVD_LPCM_AUDIO) {
        return L"LPCM";
    } else if (pmt->subtype == MEDIASUBTYPE_DOLBY_TRUEHD) {
        return L"TrueHD";
    } else if (pmt->subtype == MEDIASUBTYPE_MLP) {
        return L"MLP";
    } else if (pmt->subtype == MEDIASUBTYPE_PCM || pmt->subtype == MEDIASUBTYPE_IEEE_FLOAT) {
        return L"PCM";
    } else if (pmt->subtype == MEDIASUBTYPE_MPEG1AudioPayload || pmt->subtype == MEDIASUBTYPE_MPEG1Packet || pmt->subtype == MEDIASUBTYPE_MPEG1Payload) { //are these all actually possible?
        return L"MP1";
    } else if (pmt->subtype == MEDIASUBTYPE_MPEG2_AUDIO) {
        return L"MP2";
    } else if (pmt->subtype == MEDIASUBTYPE_MP3) {
        return L"MP3";
    } else if (pmt->subtype == MEDIASUBTYPE_FLAC || pmt->subtype == MEDIASUBTYPE_FLAC_FRAMED) {
        return L"FLAC";
    } else if (pmt->subtype == MEDIASUBTYPE_Vorbis || pmt->subtype == MEDIASUBTYPE_Vorbis2) {
        return L"Vorbis";
    } else if (pmt->subtype == MEDIASUBTYPE_OPUS || pmt->subtype == MEDIASUBTYPE_OPUS_OLD) {
        return L"Opus";
    } else if (pmt->subtype == MEDIASUBTYPE_MSAUDIO1) {
        return L"WMA1";
    } else if (pmt->subtype == MEDIASUBTYPE_WMAUDIO2) {
        return L"WMA2";
    } else if (pmt->subtype == MEDIASUBTYPE_WMAUDIO3) {
        return L"WMA3";
    } else if (pmt->subtype == MEDIASUBTYPE_WMAUDIO4) {
        return L"WMA4";
    } else if (pmt->subtype == MEDIASUBTYPE_WMAUDIO_LOSSLESS) {
        return L"WMA-LL";
    } else if (pmt->subtype == MEDIASUBTYPE_BD_LPCM_AUDIO || pmt->subtype == MEDIASUBTYPE_HDMV_LPCM_AUDIO) {
        return L"LPCM";
    } else if (pmt->subtype == MEDIASUBTYPE_IMA_AMV || pmt->subtype == MEDIASUBTYPE_ADPCM_MS || pmt->subtype == MEDIASUBTYPE_IMA_WAV || pmt->subtype == MEDIASUBTYPE_ADPCM_SWF) {
        return L"ADPCM";
    } else if (pmt->subtype == MEDIASUBTYPE_TRUESPEECH) {
        return L"TrueSpeech";
    } else if (pmt->subtype == MEDIASUBTYPE_PCM_NONE || pmt->subtype == MEDIASUBTYPE_PCM_RAW || pmt->subtype == MEDIASUBTYPE_PCM_TWOS || pmt->subtype == MEDIASUBTYPE_PCM_SOWT
        || pmt->subtype == MEDIASUBTYPE_PCM_IN24 || pmt->subtype == MEDIASUBTYPE_PCM_IN32 || pmt->subtype == MEDIASUBTYPE_PCM_FL32 || pmt->subtype == MEDIASUBTYPE_PCM_FL64
        || pmt->subtype == MEDIASUBTYPE_PCM_IN24_le || pmt->subtype == MEDIASUBTYPE_PCM_IN32_le || pmt->subtype == MEDIASUBTYPE_PCM_FL32_le || pmt->subtype == MEDIASUBTYPE_PCM_FL64_le) {
        return L"QTPCM";
    } else if (pmt->subtype == MEDIASUBTYPE_TTA1) {
        return L"TTA";
    } else if (pmt->subtype == MEDIASUBTYPE_SAMR) {
        return L"SAMR";
    } else if (pmt->subtype == MEDIASUBTYPE_QDM2) {
        return L"QDM2";
    } else if (pmt->subtype == MEDIASUBTYPE_MSGSM610) {
        return L"GSM610";
    } else if (pmt->subtype == MEDIASUBTYPE_ALAW || pmt->subtype == MEDIASUBTYPE_MULAW) {
        return L"G711";
    } else if (pmt->subtype == MEDIASUBTYPE_G723 || pmt->subtype == MEDIASUBTYPE_VIVO_G723) {
        return L"G723";
    } else if (pmt->subtype == MEDIASUBTYPE_G726) {
        return L"G726";
    } else if (pmt->subtype == MEDIASUBTYPE_G729 || pmt->subtype == MEDIASUBTYPE_729A) {
        return L"G729";
    } else if (pmt->subtype == MEDIASUBTYPE_APE) {
        return L"APE";
    } else if (pmt->subtype == MEDIASUBTYPE_TAK) {
        return L"TAK";
    } else if (pmt->subtype == MEDIASUBTYPE_ALS) {
        return L"ALS";
    } else if (pmt->subtype == MEDIASUBTYPE_NELLYMOSER) {
        return L"NELLY";
    } else if (pmt->subtype == MEDIASUBTYPE_SPEEX) {
        return L"Speex";
    } else if (pmt->subtype == MEDIASUBTYPE_AES3) {
        return L"AES3";
    } else if (pmt->subtype == MEDIASUBTYPE_DSDL || pmt->subtype == MEDIASUBTYPE_DSDM || pmt->subtype == MEDIASUBTYPE_DSD1 || pmt->subtype == MEDIASUBTYPE_DSD8) {
        return L"DSD";
    } else if (pmt->subtype == MEDIASUBTYPE_IMC) {
        return L"IMC";
    } else if (pmt->subtype == MEDIASUBTYPE_VOXWARE_RT29) {
        return L"RT29";
    } else if (pmt->subtype == MEDIASUBTYPE_MPEG_LOAS) {
        return L"LOAS";
    } else if (pmt->subtype == MEDIASUBTYPE_DOLBY_AC4) {
        return L"AC4";
    }

    if (pmt->subtype == MEDIASUBTYPE_FFMPEG_AUDIO) {
        if (pmt->formattype == FORMAT_WaveFormatExFFMPEG) {
            WAVEFORMATEXFFMPEG* wfeff = (WAVEFORMATEXFFMPEG*)pmt->pbFormat;
            int nCodecId = wfeff->nCodecId;
            if (nCodecId == 0x15067) {
                return L"AC4";
            }
        }
    }

    if (_IsFourCC(pmt->subtype)) {
        CString fcc;
        DWORD a = (pmt->subtype.Data1 >> 24) & 0xFF;
        DWORD b = (pmt->subtype.Data1 >> 16) & 0xFF;
        DWORD c = (pmt->subtype.Data1 >> 8) & 0xFF;
        DWORD d = (pmt->subtype.Data1 >> 0) & 0xFF;
        if (a != 0 || b != 0) {
            fcc.Format(_T("%02x%02x%02x%02x"), a, b, c, d);
        } else {
            fcc.Format(_T("%02x%02x"), c, d);
        }
        return fcc;
    }

    return L"UNKN";
}

bool GetVideoFormatNameFromMediaType(const GUID& guid, CString& name) {
    if (GetMediaTypeFourCC(guid, name)) {
        if (name == L"HVC1") {
            name = L"HEVC";
        } else if (name == L"AVC1") {
            name = L"H264";
        } else if (name == L"VP90") {
            name = L"VP9";
        } else if (name == L"AV01") {
            name = L"AV1";
        }
        return true;
    } else if (guid == MEDIASUBTYPE_MPEG1Payload || guid == MEDIASUBTYPE_MPEG1Video) {
        name = L"MPEG1";
        return true;
    } else if (guid == MEDIASUBTYPE_MPEG2_VIDEO) {
        name = L"MPEG2";
        return true;
    } else if (guid == MEDIASUBTYPE_ARGB32) {
        name = L"ARGB";
        return true;
    } else if (guid == MEDIASUBTYPE_RGB32 || guid == MEDIASUBTYPE_RGB24) {
        name = L"RGB";
        return true;
    } else if (guid == MEDIASUBTYPE_LAV_RAWVIDEO) {
        name = L"RAW";
        return true;
    } else {
        name = L"UNKN";
        ASSERT(false);
    }

    return false;
}
