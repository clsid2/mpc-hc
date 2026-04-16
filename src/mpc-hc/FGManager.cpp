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
#include "FGManager.h"
#include "../DeCSS/VobFile.h"
#include "../filters/Filters.h"
#include "AllocatorCommon.h"
#include "DeinterlacerFilter.h"
#include "FakeFilterMapper2.h"
#include "FileVersionInfo.h"
#include "IPinHook.h"
#include "NullRenderers.h"
#include "PathUtils.h"
#include "SyncAllocatorPresenter.h"
#include "mplayerc.h"
#include "sanear/src/Factory.h"
#include "../src/thirdparty/MpcAudioRenderer/MpcAudioRenderer.h"
#include "DSUtil.h"
#include <d3d9.h>
#include <evr.h>
#include <evr9.h>
#include <vmr9.h>
#include <ks.h>
#include <ksproxy.h>
#include <mpconfig.h>
#include <mvrInterfaces.h>
#include "../src/thirdparty/LAVFilters/src/include/IURLSourceFilterLAV.h"

#include <initguid.h>
#include "moreuuids.h"
#include <dmodshow.h>

#if !TRACE_GRAPH_BUILD
#undef TRACE
#define TRACE(...)
#endif

//
// CFGManager
//

class CNullAudioRenderer;

CFGManager::CFGManager(LPCWSTR pClassName, LPCWSTR pInputFileURL, HWND hWnd, bool IsPreview)
    : CUnknown(pClassName, nullptr)
    , m_dwRegister(0)
	, m_hWnd(hWnd)
	, m_bIsPreview(IsPreview)
    , m_bPreviewSupportsRotation(false)
    , m_ignoreVideo(false)
    , m_bIsCapture(false)
    , m_source()
    , m_transform()
    , m_override()
    , m_deadends()
    , m_aborted(false)
    , m_useragent()
    , m_referrer()
    , m_input(pInputFileURL)
{
    m_pUnkInner.CoCreateInstance(CLSID_FilterGraph, GetOwner());
    m_pFM.CoCreateInstance(CLSID_FilterMapper2);
}

CFGManager::~CFGManager()
{
    CAutoLock cAutoLock(this);
    while (!m_source.IsEmpty()) {
        delete m_source.RemoveHead();
    }
    while (!m_transform.IsEmpty()) {
        delete m_transform.RemoveHead();
    }
    while (!m_override.IsEmpty()) {
        delete m_override.RemoveHead();
    }
    m_pUnks.RemoveAll();
    m_pUnkInner.Release();
}

STDMETHODIMP CFGManager::NonDelegatingQueryInterface(REFIID riid, void** ppv)
{
    CheckPointer(ppv, E_POINTER);

    return
        QI(IFilterGraph)
        QI(IGraphBuilder)
        QI(IFilterGraph2)
        QI(IGraphBuilder2)
        QI(IGraphBuilderDeadEnd)
        (m_pUnkInner && riid != IID_IUnknown && SUCCEEDED(m_pUnkInner->QueryInterface(riid, ppv))) ? S_OK :
        __super::NonDelegatingQueryInterface(riid, ppv);
}

//

void CFGManager::CStreamPath::Append(IBaseFilter* pBF, IPin* pPin)
{
    path_t p;
    p.clsid = GetCLSID(pBF);
    p.filter = GetFilterName(pBF);
    p.pin = GetPinName(pPin);
    AddTail(p);
}

bool CFGManager::CStreamPath::Compare(const CStreamPath& path)
{
    POSITION pos1 = GetHeadPosition();
    POSITION pos2 = path.GetHeadPosition();

    while (pos1 && pos2) {
        const path_t& p1 = GetNext(pos1);
        const path_t& p2 = path.GetNext(pos2);

        if (p1.filter != p2.filter) {
            return true;
        } else if (p1.pin != p2.pin) {
            return false;
        }
    }

    return true;
}

//

bool CFGManager::CheckBytes(HANDLE hFile, CString chkbytes)
{
    CAtlList<CString> sl;
    Explode(chkbytes, sl, ',');

    if (sl.GetCount() < 4) {
        return false;
    }

    ASSERT(!(sl.GetCount() & 3));

    LARGE_INTEGER size = {0, 0};
    GetFileSizeEx(hFile, &size);

    while (sl.GetCount() >= 4) {
        CString offsetstr = sl.RemoveHead();
        CString cbstr = sl.RemoveHead();
        CString maskstr = sl.RemoveHead();
        CString valstr = sl.RemoveHead();

        long cb = _ttol(cbstr);

        if (offsetstr.IsEmpty() || cbstr.IsEmpty()
                || valstr.IsEmpty() || (valstr.GetLength() & 1)
                || cb * 2 != valstr.GetLength()) {
            return false;
        }

        LARGE_INTEGER offset;
        offset.QuadPart = _ttoi64(offsetstr);
        if (offset.QuadPart < 0) {
            offset.QuadPart = size.QuadPart - offset.QuadPart;
        }
        SetFilePointerEx(hFile, offset, &offset, FILE_BEGIN);

        // LAME
        while (maskstr.GetLength() < valstr.GetLength()) {
            maskstr += _T('F');
        }

        CAtlArray<BYTE> mask, val;
        CStringToBin(maskstr, mask);
        CStringToBin(valstr, val);

        for (size_t i = 0; i < val.GetCount(); i++) {
            BYTE b;
            DWORD r;
            if (!ReadFile(hFile, &b, 1, &r, nullptr) || (b & mask[i]) != val[i]) {
                return false;
            }
        }
    }

    return sl.IsEmpty();
}

bool CFGManager::HasFilterOverride(CLSID clsid)
{
    POSITION pos = m_override.GetHeadPosition();
    while (pos) {
        CFGFilter* pFGF = m_transform.GetNext(pos);
        if (pFGF->GetCLSID() == clsid) {
            return true;
        }
    }
    return false;
}

bool CFGManager::HasFilterOverride(CStringW DisplayName)
{
    POSITION pos = m_override.GetHeadPosition();
    while (pos) {
        CFGFilter* pFGF = m_transform.GetNext(pos);
        if (pFGF->GetCLSID() == CLSID_NULL) {
            CFGFilterRegistry* pFGFR = dynamic_cast<CFGFilterRegistry*>(pFGF);
            if (pFGFR && pFGFR->GetDisplayName().CompareNoCase(DisplayName) == 0) {
                return true;
            }
        }
    }
    return false;
}

CFGFilter* LookupFilterRegistry(const GUID& guid, CAtlList<CFGFilter*>& list, UINT64 fallback_merit = MERIT64_DO_USE)
{
    POSITION pos = list.GetHeadPosition();
    CFGFilter* pFilter = nullptr;
    while (pos) {
        CFGFilter* pFGF = list.GetNext(pos);
        if (pFGF->GetCLSID() == guid) {
            pFilter = pFGF;
            break;
        }
    }
    if (pFilter) {
        return DEBUG_NEW CFGFilterRegistry(guid, pFilter->GetMerit());
    } else {
        return DEBUG_NEW CFGFilterRegistry(guid, fallback_merit);
    }
}

HRESULT CFGManager::EnumSourceFilters(LPCWSTR lpcwstrFileName, CFGFilterList& fl)
{
    // TODO: use overrides

    CheckPointer(lpcwstrFileName, E_POINTER);

    fl.RemoveAll();

    CStringW fn = CStringW(lpcwstrFileName).TrimLeft();
    CStringW cfn = fn;
    if (cfn.Left(4) == "\\\\?\\") {
        cfn = cfn.Mid(4);
    }
    CStringW protocol = cfn.Left(cfn.Find(':') + 1).TrimRight(':').MakeLower();
    CStringW ext = CPathW(fn).GetExtension().MakeLower();

    HANDLE hFile = INVALID_HANDLE_VALUE;

    if (protocol.GetLength() <= 1 || protocol == L"file") {
        hFile = CreateFile(CString(fn), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, (HANDLE)nullptr);

        // In case of audio CDs with extra content, the audio tracks
        // cannot be accessed directly so we have to try opening it
        if (hFile == INVALID_HANDLE_VALUE && ext != L".cda") {
            return VFW_E_NOT_FOUND;
        }
    }

    if (hFile == INVALID_HANDLE_VALUE) {
        // internal / protocol

        POSITION pos = m_source.GetHeadPosition();
        while (pos) {
            CFGFilter* pFGF = m_source.GetNext(pos);
            if (pFGF->m_protocols.Find(CString(protocol))) {
                fl.Insert(pFGF, 0, false, false);
            }
        }
    } else {
        // internal / check bytes

        POSITION pos = m_source.GetHeadPosition();
        while (pos) {
            CFGFilter* pFGF = m_source.GetNext(pos);

            POSITION pos2 = pFGF->m_chkbytes.GetHeadPosition();
            while (pos2) {
                if (CheckBytes(hFile, pFGF->m_chkbytes.GetNext(pos2))) {
                    fl.Insert(pFGF, 1, false, false);
                    break;
                }
            }
        }
    }

    if (!ext.IsEmpty()) {
        // internal / file extension

        POSITION pos = m_source.GetHeadPosition();
        while (pos) {
            CFGFilter* pFGF = m_source.GetNext(pos);
            if (pFGF->m_extensions.Find(CString(ext))) {
                fl.Insert(pFGF, 2, false, false);
            }
        }
    }

    {
        // internal / the rest

        POSITION pos = m_source.GetHeadPosition();
        while (pos) {
            CFGFilter* pFGF = m_source.GetNext(pos);
            if (pFGF->m_protocols.IsEmpty() && pFGF->m_chkbytes.IsEmpty() && pFGF->m_extensions.IsEmpty()) {
                fl.Insert(pFGF, 3, false, false);
            }
        }
    }

    TCHAR buff[256];
    ULONG len;

    if (hFile == INVALID_HANDLE_VALUE) {
        // protocol

        CRegKey key;
        if (ERROR_SUCCESS == key.Open(HKEY_CLASSES_ROOT, CString(protocol), KEY_READ)) {
            CRegKey exts;
            if (ERROR_SUCCESS == exts.Open(key, _T("Extensions"), KEY_READ)) {
                len = _countof(buff);
                if (ERROR_SUCCESS == exts.QueryStringValue(CString(ext), buff, &len)) {
                    fl.Insert(LookupFilterRegistry(GUIDFromCString(buff), m_override), 4);
                }
            }

            len = _countof(buff);
            if (ERROR_SUCCESS == key.QueryStringValue(_T("Source Filter"), buff, &len)) {
                fl.Insert(LookupFilterRegistry(GUIDFromCString(buff), m_override), 5);
            }
        }

        fl.Insert(DEBUG_NEW CFGFilterRegistry(CLSID_URLReader), 6);
    } else {
        // check bytes

        CRegKey key;
        if (ERROR_SUCCESS == key.Open(HKEY_CLASSES_ROOT, _T("Media Type"), KEY_READ)) {
            FILETIME ft;
            len = _countof(buff);
            for (DWORD i = 0; ERROR_SUCCESS == key.EnumKey(i, buff, &len, &ft); i++, len = _countof(buff)) {
                GUID majortype;
                if (FAILED(GUIDFromCString(buff, majortype))) {
                    continue;
                }

                CRegKey majorkey;
                if (ERROR_SUCCESS == majorkey.Open(key, buff, KEY_READ)) {
                    len = _countof(buff);
                    for (DWORD j = 0; ERROR_SUCCESS == majorkey.EnumKey(j, buff, &len, &ft); j++, len = _countof(buff)) {
                        GUID subtype;
                        if (FAILED(GUIDFromCString(buff, subtype))) {
                            continue;
                        }

                        CRegKey subkey;
                        if (ERROR_SUCCESS == subkey.Open(majorkey, buff, KEY_READ)) {
                            len = _countof(buff);
                            if (ERROR_SUCCESS != subkey.QueryStringValue(_T("Source Filter"), buff, &len)) {
                                continue;
                            }

                            GUID clsid = GUIDFromCString(buff);
                            TCHAR buff2[256];
                            ULONG len2;

                            len = _countof(buff);
                            len2 = sizeof(buff2);
                            for (DWORD k = 0, type;
                                    clsid != GUID_NULL && ERROR_SUCCESS == RegEnumValue(subkey, k, buff2, &len2, 0, &type, (BYTE*)buff, &len);
                                    k++, len = _countof(buff), len2 = sizeof(buff2)) {
                                if (CheckBytes(hFile, CString(buff))) {
                                    CFGFilter* pFGF = LookupFilterRegistry(clsid, m_override);
                                    pFGF->AddType(majortype, subtype);
                                    if (pFGF->GetMerit() >= MERIT64_ABOVE_DSHOW) {
                                        fl.Insert(pFGF, 7);
                                    } else {
                                        fl.Insert(pFGF, 9);
                                    }
                                    break;
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    if (!ext.IsEmpty()) {
        // file extension

        CRegKey key;
        if (ERROR_SUCCESS == key.Open(HKEY_CLASSES_ROOT, _T("Media Type\\Extensions\\") + CString(ext), KEY_READ)) {
            len = _countof(buff);
            ZeroMemory(buff, sizeof(buff));
            LONG ret = key.QueryStringValue(_T("Source Filter"), buff, &len); // QueryStringValue can return ERROR_INVALID_DATA on bogus strings (radlight mpc v1003, fixed in v1004)
            if (ERROR_SUCCESS == ret || ERROR_INVALID_DATA == ret && GUIDFromCString(buff) != GUID_NULL) {
                GUID clsid = GUIDFromCString(buff);
                GUID majortype = GUID_NULL;
                GUID subtype = GUID_NULL;

                len = _countof(buff);
                if (ERROR_SUCCESS == key.QueryStringValue(_T("Media Type"), buff, &len)) {
                    majortype = GUIDFromCString(buff);
                }

                len = _countof(buff);
                if (ERROR_SUCCESS == key.QueryStringValue(_T("Subtype"), buff, &len)) {
                    subtype = GUIDFromCString(buff);
                }

                CFGFilter* pFGF = LookupFilterRegistry(clsid, m_override);
                pFGF->AddType(majortype, subtype);
                fl.Insert(pFGF, 7);
            }
        }

        // preferred external filters
        if (ext == L".avs" || ext == L".vpy") {
            POSITION pos = m_override.GetHeadPosition();
            while (pos) {
                CFGFilter* pFGF = m_override.GetNext(pos);
                if (pFGF->GetMerit() >= MERIT64_ABOVE_DSHOW) {
                    if (pFGF->GetCLSID() == GUIDFromCString(L"{7D3BBD5A-880D-4A30-A2D1-7B8C2741AFEF}")) { // MPC Script Source
                        if (ext == L".avs" || ext == L".vpy") {
                            fl.Insert(pFGF, 0, false, false);
                        }
                    }
                }
            }
        }
    }

    if (hFile != INVALID_HANDLE_VALUE) {
        CloseHandle(hFile);

        CFGFilter* pFGF = LookupFilterRegistry(CLSID_AsyncReader, m_override);
        pFGF->AddType(MEDIATYPE_Stream, MEDIASUBTYPE_NULL);
        fl.Insert(pFGF, 9);
    }

    return S_OK;
}

HRESULT CFGManager::AddSourceFilter(CFGFilter* pFGF, LPCWSTR lpcwstrFileName, LPCWSTR lpcwstrFilterName, IBaseFilter** ppBF)
{
    CLSID clsid = pFGF->GetCLSID();
    TRACE(_T("FGM: AddSourceFilter trying '%s'\n"), CStringFromGUID(clsid).GetString());

    CheckPointer(lpcwstrFileName, E_POINTER);
    CheckPointer(ppBF, E_POINTER);

    ASSERT(*ppBF == nullptr);

    HRESULT hr;

    CComPtr<IBaseFilter> pBF;
    CInterfaceList<IUnknown, &IID_IUnknown> pUnks;
    if (FAILED(hr = pFGF->Create(&pBF, pUnks))) {
        return hr;
    }

    CComQIPtr<IFileSourceFilter> pFSF = pBF;
    if (!pFSF) {
        return E_NOINTERFACE;
    }

    if (clsid == __uuidof(CRARFileSource) && m_entryRFS.GetLength() > 0) {
        CComPtr<CRARFileSource> rfs = static_cast<CRARFileSource*>(pBF.p);
        std::wstring preselectedRarFileEntry(m_entryRFS.GetBuffer());
        rfs->SetPreselectedRarFileEntry(preselectedRarFileEntry);
    }

    if (FAILED(hr = AddFilter(pBF, lpcwstrFilterName))) {
        return hr;
    }

    const AM_MEDIA_TYPE* pmt = nullptr;
    if (clsid == GUID_LAVSplitterSource) {
        CComQIPtr<IURLSourceFilterLAV> pSFL = pBF;
        if (pSFL && (!m_useragent.IsEmpty() || !m_referrer.IsEmpty())) {
            // ToDo: set strings
            hr = pSFL->LoadURL(lpcwstrFileName, m_useragent, m_referrer);
            if (FAILED(hr)) {
                RemoveFilter(pBF);
                return hr;
            }
        } else {
            hr = pFSF->Load(lpcwstrFileName, pmt);
            if (FAILED(hr)) {
                RemoveFilter(pBF);
                return hr;
            }
        }
    } else {
        CMediaType mt;
        const CAtlList<GUID>& types = pFGF->GetTypes();
        if (types.GetCount() == 2 && (types.GetHead() != GUID_NULL || types.GetTail() != GUID_NULL)) {
            mt.majortype = types.GetHead();
            mt.subtype = types.GetTail();
            pmt = &mt;
        }
        
        hr = pFSF->Load(lpcwstrFileName, pmt);
        if (FAILED(hr) || m_aborted) { // sometimes looping with AviSynth
            RemoveFilter(pBF);
            return m_aborted ? E_ABORT : hr;
        }

        BeginEnumMediaTypes(GetFirstPin(pBF, PINDIR_OUTPUT), pEMT, pmt2) {
            static const GUID guid1 =
            { 0x640999A0, 0xA946, 0x11D0, { 0xA5, 0x20, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 } }; // ASX file Parser
            static const GUID guid2 =
            { 0x640999A1, 0xA946, 0x11D0, { 0xA5, 0x20, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 } }; // ASX v.2 file Parser
            static const GUID guid3 =
            { 0xD51BD5AE, 0x7548, 0x11CF, { 0xA5, 0x20, 0x00, 0x80, 0xC7, 0x7E, 0xF5, 0x8A } }; // XML Playlist

            if (pmt2->subtype == guid1 || pmt2->subtype == guid2 || pmt2->subtype == guid3) {
                RemoveFilter(pBF);
                pFGF = DEBUG_NEW CFGFilterRegistry(CLSID_NetShowSource);
                hr = AddSourceFilter(pFGF, lpcwstrFileName, lpcwstrFilterName, ppBF);
                delete pFGF;
                return hr;
            }
        }
        EndEnumMediaTypes(pmt2);
    }

    *ppBF = pBF.Detach();

    m_pUnks.AddTailList(&pUnks);

    return S_OK;
}

// IFilterGraph

STDMETHODIMP CFGManager::AddFilter(IBaseFilter* pFilter, LPCWSTR pName)
{
    if (!m_pUnkInner) {
        return E_UNEXPECTED;
    }

    CAutoLock cAutoLock(this);

    HRESULT hr;

    if (FAILED(hr = CComQIPtr<IFilterGraph2>(m_pUnkInner)->AddFilter(pFilter, pName))) {
        return hr;
    }

    // TODO
    hr = pFilter->JoinFilterGraph(nullptr, nullptr);
    hr = pFilter->JoinFilterGraph(this, pName);

    return hr;
}

STDMETHODIMP CFGManager::RemoveFilter(IBaseFilter* pFilter)
{
    if (!m_pUnkInner) {
        return E_UNEXPECTED;
    }

    CAutoLock cAutoLock(this);

    return CComQIPtr<IFilterGraph2>(m_pUnkInner)->RemoveFilter(pFilter);
}

STDMETHODIMP CFGManager::EnumFilters(IEnumFilters** ppEnum)
{
    if (!m_pUnkInner) {
        return E_UNEXPECTED;
    }

    // Not locking here fixes a deadlock involving ReClock
    //CAutoLock cAutoLock(this);

    return CComQIPtr<IFilterGraph2>(m_pUnkInner)->EnumFilters(ppEnum);
}

STDMETHODIMP CFGManager::FindFilterByName(LPCWSTR pName, IBaseFilter** ppFilter)
{
    if (!m_pUnkInner) {
        return E_UNEXPECTED;
    }

    CAutoLock cAutoLock(this);

    return CComQIPtr<IFilterGraph2>(m_pUnkInner)->FindFilterByName(pName, ppFilter);
}

STDMETHODIMP CFGManager::ConnectDirect(IPin* pPinOut, IPin* pPinIn, const AM_MEDIA_TYPE* pmt)
{
    if (!m_pUnkInner) {
        return E_UNEXPECTED;
    }

    CAutoLock cAutoLock(this);

    CComPtr<IBaseFilter> pBF = GetFilterFromPin(pPinIn);
    CLSID clsid = GetCLSID(pBF);

    // TODO: GetUpStreamFilter goes up on the first input pin only
    for (CComPtr<IBaseFilter> pBFUS = GetFilterFromPin(pPinOut); pBFUS; pBFUS = GetUpStreamFilter(pBFUS)) {
        if (pBFUS == pBF) {
            return VFW_E_CIRCULAR_GRAPH;
        }
        if (clsid != CLSID_Proxy && GetCLSID(pBFUS) == clsid) {
            return VFW_E_CANNOT_CONNECT;
        }
    }

    return CComQIPtr<IFilterGraph2>(m_pUnkInner)->ConnectDirect(pPinOut, pPinIn, pmt);
}

STDMETHODIMP CFGManager::Reconnect(IPin* ppin)
{
    if (!m_pUnkInner) {
        return E_UNEXPECTED;
    }

    CAutoLock cAutoLock(this);

    return CComQIPtr<IFilterGraph2>(m_pUnkInner)->Reconnect(ppin);
}

STDMETHODIMP CFGManager::Disconnect(IPin* ppin)
{
    if (!m_pUnkInner) {
        return E_UNEXPECTED;
    }

    CAutoLock cAutoLock(this);

    return CComQIPtr<IFilterGraph2>(m_pUnkInner)->Disconnect(ppin);
}

STDMETHODIMP CFGManager::SetDefaultSyncSource()
{
    if (!m_pUnkInner) {
        return E_UNEXPECTED;
    }

    CAutoLock cAutoLock(this);

    return CComQIPtr<IFilterGraph2>(m_pUnkInner)->SetDefaultSyncSource();
}

// IGraphBuilder

STDMETHODIMP CFGManager::Connect(IPin* pPinOut, IPin* pPinIn)
{
    return Connect(pPinOut, pPinIn, true);
}

HRESULT CFGManager::Connect(IPin* pPinOut, IPin* pPinIn, bool bContinueRender)
{
    CAutoLock cAutoLock(this);

    CheckPointer(pPinOut, E_POINTER);

    if (m_aborted) {
        return E_ABORT;
    }

    HRESULT hr;

    if (S_OK != IsPinDirection(pPinOut, PINDIR_OUTPUT)
            || pPinIn && S_OK != IsPinDirection(pPinIn, PINDIR_INPUT)) {
        return VFW_E_INVALID_DIRECTION;
    }

    if (S_OK == IsPinConnected(pPinOut)
            || pPinIn && S_OK == IsPinConnected(pPinIn)) {
        return VFW_E_ALREADY_CONNECTED;
    }

    bool fDeadEnd = true;

    if (pPinIn) {
        // 1. Try a direct connection between the filters, with no intermediate filters

        if (SUCCEEDED(hr = ConnectDirect(pPinOut, pPinIn, nullptr))) {
            return hr;
        }
    } else {
        // 1. Use IStreamBuilder

        if (CComQIPtr<IStreamBuilder> pSB = pPinOut) {
            if (SUCCEEDED(hr = pSB->Render(pPinOut, this))) {
                return hr;
            }

            pSB->Backout(pPinOut, this);
        }
    }

    // 2. Try cached filters

    CComPtr<IBaseFilter> pFilterPinIn = nullptr;
    if (pPinIn) {
        pFilterPinIn = GetFilterFromPin(pPinIn);
    }

    if (CComQIPtr<IGraphConfig> pGC = (IGraphBuilder2*)this) {
        BeginEnumCachedFilters(pGC, pEF, pBF) {
            if (pFilterPinIn && pFilterPinIn == pBF) {
                continue;
            }

            hr = pGC->RemoveFilterFromCache(pBF);

            // does RemoveFilterFromCache call AddFilter like AddFilterToCache calls RemoveFilter ?

            if (SUCCEEDED(hr = ConnectFilterDirect(pPinOut, pBF, nullptr))) {
                if (!IsStreamEnd(pBF)) {
                    fDeadEnd = false;
                }

                if (SUCCEEDED(hr = ConnectFilter(pBF, pPinIn))) {
                    return hr;
                }
            }

            hr = pGC->AddFilterToCache(pBF);
        }
        EndEnumCachedFilters;
    }

    // 3. Try filters in the graph

    CComPtr<IBaseFilter> pFilterPinOut = GetFilterFromPin(pPinOut);
    CLSID clsid_pinout = GetCLSID(pFilterPinOut);

    {
        CInterfaceList<IBaseFilter> pBFs;

        BeginEnumFilters(this, pEF, pBF) {
            if (pFilterPinIn && pFilterPinIn == pBF || pFilterPinOut == pBF) {
                continue;
            }

            // HACK: ffdshow - audio capture filter
            if (clsid_pinout == GUIDFromCString(_T("{04FE9017-F873-410E-871E-AB91661A4EF7}"))
                    && GetCLSID(pBF) == GUIDFromCString(_T("{E30629D2-27E5-11CE-875D-00608CB78066}"))) {
                continue;
            }

            pBFs.AddTail(pBF);
        }
        EndEnumFilters;

        POSITION pos = pBFs.GetHeadPosition();
        while (pos) {
            IBaseFilter* pBF = pBFs.GetNext(pos);

            if (SUCCEEDED(hr = ConnectFilterDirect(pPinOut, pBF, nullptr))) {
                if (!IsStreamEnd(pBF)) {
                    fDeadEnd = false;
                }

                if (SUCCEEDED(hr = ConnectFilter(pBF, pPinIn))) {
                    return hr;
                }
            }

            EXECUTE_ASSERT(SUCCEEDED(Disconnect(pPinOut)));
        }
    }

    // 4. Look up filters in the registry

    {
        // workaround for Cyberlink video decoder, which can have an unwanted output pin
        if (clsid_pinout == GUIDFromCString(_T("{F8FC6C1F-DE81-41A8-90FF-0316FDD439FD}"))) {
            CPinInfo infoPinOut;
            if (SUCCEEDED(pPinOut->QueryPinInfo(&infoPinOut))) {
                if (CString(infoPinOut.achName) == L"~Encode Out") {
                    // ignore this pin
                    return S_OK;
                }
            }
        }

        CFGFilterList fl;

        CAtlArray<GUID> types;
        ExtractMediaTypes(pPinOut, types);

        POSITION pos = m_transform.GetHeadPosition();
        while (pos) {
            CFGFilter* pFGF = m_transform.GetNext(pos);
#if !WIN64
            if (pFGF != (CFGFilter*)0x3) // invalid pointer value, weird x86 bug
#endif
            if (pFGF->GetMerit() < MERIT64_DO_USE || pFGF->CheckTypes(types, false)) {
                fl.Insert(pFGF, 0, pFGF->CheckTypes(types, true), false);
            }
        }

        pos = m_override.GetHeadPosition();
        while (pos) {
            CFGFilter* pFGF = m_override.GetNext(pos);
            if (pFGF->GetMerit() < MERIT64_DO_USE || pFGF->CheckTypes(types, false)) {
                fl.Insert(pFGF, 0, pFGF->CheckTypes(types, true), false);
            }
        }

        CComPtr<IEnumMoniker> pEM;
        if (!types.IsEmpty()
                && SUCCEEDED(m_pFM->EnumMatchingFilters(
                                 &pEM, 0, FALSE, MERIT_DO_NOT_USE + 1,
                                 TRUE, (DWORD)types.GetCount() / 2, types.GetData(), nullptr, nullptr, FALSE,
                                 !!pPinIn, 0, nullptr, nullptr, nullptr))) {
            for (CComPtr<IMoniker> pMoniker; S_OK == pEM->Next(1, &pMoniker, nullptr); pMoniker = nullptr) {
                CFGFilterRegistry* pFGF = DEBUG_NEW CFGFilterRegistry(pMoniker);
                fl.Insert(pFGF, 0, pFGF->CheckTypes(types, true));
            }
        }

        // let's check whether the madVR allocator presenter is in our list
        // it should be if madVR is selected as the video renderer
        CFGFilter* pMadVRAllocatorPresenter = nullptr;
        pos = fl.GetHeadPosition();
        while (pos) {
            CFGFilter* pFGF = fl.GetNext(pos);
            if (pFGF->GetCLSID() == CLSID_madVRAllocatorPresenter) {
                // found it!
                pMadVRAllocatorPresenter = pFGF;
                break;
            }
        }

        pos = fl.GetHeadPosition();
        while (pos) {
            if (m_aborted) {
                return E_ABORT;
            }

            CFGFilter* pFGF = fl.GetNext(pos);

            // avoid pointless connection attempts
            CLSID candidate = pFGF->GetCLSID();
            if (clsid_pinout == candidate) {
                continue;
            } else if (candidate == CLSID_VSFilter) {
                if (clsid_pinout == GUID_LAVAudio || clsid_pinout == __uuidof(CAudioSwitcherFilter)) {
                    continue;
                }
            } else if (candidate == CLSID_RDPDShowRedirectionFilter) {
                if (clsid_pinout == __uuidof(CAudioSwitcherFilter)) {
                    continue;
                }
            } else if (candidate == GUID_LAVAudio) {
                if (clsid_pinout == __uuidof(CAudioSwitcherFilter)) {
                    continue;
                }
            }

#if 0
            // Checks if madVR is already in the graph to avoid two instances at the same time
            CComPtr<IBaseFilter> pBFmadVR;
            FindFilterByName(_T("madVR"), &pBFmadVR);
            if (pBFmadVR && (pFGF->GetName() == _T("madVR"))) {
                continue;
            }
#endif

            if (pMadVRAllocatorPresenter && (pFGF->GetCLSID() == CLSID_madVR)) {
                // the pure madVR filter was selected (without the allocator presenter)
                // subtitles, OSD etc don't work correctly without the allocator presenter
                // so we prefer the allocator presenter over the pure filter
                pFGF = pMadVRAllocatorPresenter;
            }

            CString filtername = pFGF->GetName().GetString();
            if (filtername.IsEmpty()) {
                filtername = CLSIDToString(candidate);
            }
            TRACE(_T("FGM: Connecting '%s'\n"), filtername);

            CComPtr<IBaseFilter> pBF;
            CInterfaceList<IUnknown, &IID_IUnknown> pUnks;
            hr = pFGF->Create(&pBF, pUnks);
            if (FAILED(hr)) {
                TRACE(_T("FGM: Filter creation failed\n"));
                if (!m_bIsCapture) {
                    // Check if selected video renderer fails to load
                    CLSID filter = pFGF->GetCLSID();
                    if (filter == CLSID_MPCVRAllocatorPresenter || filter == CLSID_madVRAllocatorPresenter || filter == CLSID_DXRAllocatorPresenter) {
                        if (IDYES == AfxMessageBox(_T("The selected video renderer has failed to load.\n\nThe player will now fallback to using a basic video renderer, which has reduced performance and quality. Subtitles may also fail to load.\n\nDo you want to change settings to use the default video renderer (EVR-CP/VMR9)? (player restart required)"), MB_ICONEXCLAMATION | MB_YESNO, 0)) {
                            CAppSettings& s = AfxGetAppSettings();
                            s.iDSVideoRendererType = IsCLSIDRegistered(CLSID_EnhancedVideoRenderer) ? VIDRNDT_DS_EVR_CUSTOM : VIDRNDT_DS_VMR9RENDERLESS;
                        }
                    } else if (filter == CLSID_EVRAllocatorPresenter || filter == CLSID_VMR9AllocatorPresenter) {
                        if (IDYES == AfxMessageBox(_T("The selected video renderer has failed to load.\n\nThis problem is often caused by a bug in the graphics driver. Or you may be using a generic driver which has limited capabilities. It is recommended to update the graphics driver to solve this problem. A proper driver is required for optimal video playback performance and quality.\n\nThe player will now fallback to using a basic video renderer, which has reduced performance and quality. Subtitles may also fail to load.\n\nYou can select a different renderer here:\nOptions > playback > Output\n\nDo you want to use the basic video renderer by default?"), MB_ICONEXCLAMATION | MB_YESNO, 0)) {
                            CAppSettings& s = AfxGetAppSettings();
                            s.iDSVideoRendererType = IsCLSIDRegistered(CLSID_EnhancedVideoRenderer) ? VIDRNDT_DS_EVR : VIDRNDT_DS_VMR9WINDOWED;
                            s.SetSubtitleRenderer(CAppSettings::SubtitleRenderer::VS_FILTER);
                            // Disable DXVA in internal video decoder
                            CMPlayerCApp* pApp = AfxGetMyApp();
                            pApp->WriteProfileInt(IDS_R_INTERNAL_LAVVIDEO_HWACCEL, _T("HWAccel"), 0);
                        }
                    }
                }
                continue;
            }

            if (FAILED(hr = AddFilter(pBF, pFGF->GetName()))) {
                TRACE(_T("FGM: Adding the filter failed\n"));
                pUnks.RemoveAll();
                pBF.Release();
                continue;
            }

            hr = ConnectFilterDirect(pPinOut, pBF, nullptr);
            /*
            if (FAILED(hr))
            {
                if (types.GetCount() >= 2 && types[0] == MEDIATYPE_Stream && types[1] != GUID_NULL)
                {
                    CMediaType mt;

                    mt.majortype = types[0];
                    mt.subtype = types[1];
                    mt.formattype = FORMAT_None;
                    if (FAILED(hr)) hr = ConnectFilterDirect(pPinOut, pBF, &mt);

                    mt.formattype = GUID_NULL;
                    if (FAILED(hr)) hr = ConnectFilterDirect(pPinOut, pBF, &mt);
                }
            }
            */
            if (SUCCEEDED(hr)) {
                TRACE(_T("FGM: Filter connected to %s\n"), CLSIDToString(clsid_pinout));
                if (!IsStreamEnd(pBF)) {
                    fDeadEnd = false;
                }

                if (m_aborted) {
                    return E_ABORT;
                }

                if (bContinueRender) {
                    hr = ConnectFilter(pBF, pPinIn);
                }

                if (SUCCEEDED(hr)) {
                    m_pUnks.AddTailList(&pUnks);

                    // maybe the application should do this...

                    POSITION posInterface = pUnks.GetHeadPosition();
                    while (posInterface) {
                        if (CComQIPtr<IMixerPinConfig, &IID_IMixerPinConfig> pMPC = pUnks.GetNext(posInterface)) {
                            pMPC->SetAspectRatioMode(AM_ARMODE_STRETCHED);
                        }
                    }

                    if (CComQIPtr<IVMRAspectRatioControl> pARC = pBF) {
                        pARC->SetAspectRatioMode(VMR_ARMODE_NONE);
                    }

                    if (CComQIPtr<IVMRAspectRatioControl9> pARC = pBF) {
                        pARC->SetAspectRatioMode(VMR_ARMODE_NONE);
                    }

                    if (CComQIPtr<IVMRMixerControl9> pMC = pBF) {
                        m_pUnks.AddTail(pMC);
                    }

                    if (CComQIPtr<IVMRMixerBitmap9> pMB = pBF) {
                        m_pUnks.AddTail(pMB);
                    }

                    if (CComQIPtr<IMFVideoMixerBitmap> pMFVMB = pBF) {
                        m_pUnks.AddTail(pMFVMB);
                    }

                    if (CComQIPtr<IMFGetService, &__uuidof(IMFGetService)> pMFGS = pBF) {
                        CComPtr<IMFVideoDisplayControl> pMFVDC;
                        CComPtr<IMFVideoMixerBitmap>    pMFMB;
                        CComPtr<IMFVideoProcessor>      pMFVP;

                        if (SUCCEEDED(pMFGS->GetService(MR_VIDEO_RENDER_SERVICE, IID_PPV_ARGS(&pMFVDC)))) {
                            m_pUnks.AddTail(pMFVDC);
                        }

                        if (SUCCEEDED(pMFGS->GetService(MR_VIDEO_MIXER_SERVICE, IID_PPV_ARGS(&pMFMB)))) {
                            m_pUnks.AddTail(pMFMB);
                        }

                        if (SUCCEEDED(pMFGS->GetService(MR_VIDEO_MIXER_SERVICE, IID_PPV_ARGS(&pMFVP)))) {
                            m_pUnks.AddTail(pMFVP);
                        }
                    }

                    return hr;
                }
            }

            TRACE(_T("FGM: Failed to connect to %s\n"), CLSIDToString(clsid_pinout));
            CPinInfo infoPinOut;
            if (SUCCEEDED(pPinOut->QueryPinInfo(&infoPinOut))) {
                TRACE(_T("FGM: Output pin name: %s\n"), infoPinOut.achName);
            }
            EXECUTE_ASSERT(SUCCEEDED(RemoveFilter(pBF)));
            pUnks.RemoveAll();
            pBF.Release();
        }
    }

    if (fDeadEnd) {
        CAutoPtr<CStreamDeadEnd> psde(DEBUG_NEW CStreamDeadEnd());
        psde->AddTailList(&m_streampath);
        int skip = 0;
        BeginEnumMediaTypes(pPinOut, pEM, pmt) {
            if (pmt->majortype == MEDIATYPE_Stream && pmt->subtype == MEDIASUBTYPE_NULL) {
                skip++;
            }
            psde->mts.AddTail(CMediaType(*pmt));
        }
        EndEnumMediaTypes(pmt);
        if (skip < (int)psde->mts.GetCount()) {
            m_deadends.Add(psde);
        }
    }

    return pPinIn ? VFW_E_CANNOT_CONNECT : VFW_E_CANNOT_RENDER;
}

STDMETHODIMP CFGManager::Render(IPin* pPinOut)
{
    CAutoLock cAutoLock(this);

    return RenderEx(pPinOut, 0, nullptr);
}

HRESULT CFGManager::RenderRFSFileEntry(LPCWSTR lpcwstrFileName, LPCWSTR lpcwstrPlayList, CStringW entryRFS){
    this->m_entryRFS = entryRFS;
    return RenderFile(lpcwstrFileName, lpcwstrPlayList);
}

CUnknown* WINAPI CFGManager::GetMpcAudioRendererInstance(LPUNKNOWN lpunk, HRESULT* phr) {
    return CreateInstance<CMpcAudioRenderer>(lpunk, phr);
}

STDMETHODIMP CFGManager::RenderFile(LPCWSTR lpcwstrFileName, LPCWSTR lpcwstrPlayList)
{
    TRACE(_T("CFGManager::RenderFile on thread: %lu\n"), GetCurrentThreadId());
    CAutoLock cAutoLock(this);

    m_streampath.RemoveAll();
    m_deadends.RemoveAll();

    HRESULT hr;
    HRESULT hrRFS = S_OK;

    CFGFilterList fl;
    if (FAILED(hr = EnumSourceFilters(lpcwstrFileName, fl))) {
        return hr;
    }

    CAutoPtrArray<CStreamDeadEnd> deadends;

    hr = VFW_E_CANNOT_RENDER;

    POSITION pos = fl.GetHeadPosition();
    while (pos) {
        CComPtr<IBaseFilter> pBF;
        CFGFilter* pFG = fl.GetNext(pos);

        if (SUCCEEDED(hr = AddSourceFilter(pFG, lpcwstrFileName, pFG->GetName(), &pBF))) {
            m_streampath.RemoveAll();
            m_deadends.RemoveAll();

            if (m_ignoreVideo) {
                CFGFilter* pFGF = DEBUG_NEW CFGFilterInternal<CNullVideoRenderer>(StrRes(IDS_PPAGE_OUTPUT_NULL_COMP), MERIT64_ABOVE_DSHOW + 3);
                pFGF->AddType(MEDIATYPE_Video, MEDIASUBTYPE_NULL);
                m_transform.AddTail(pFGF);
            }

            if (SUCCEEDED(hr = ConnectFilter(pBF, nullptr))) {
                // insert null video renderer on next RenderFile call which is used for audio dubs
                m_ignoreVideo = True;
                TRACE(_T("CFGManager::RenderFile complete\n"));
                return hr;
            }

            NukeDownstream(pBF);
            RemoveFilter(pBF);

            deadends.Append(m_deadends);

            if (hr == E_ABORT) {
                break;
            }
        } else if (pFG->GetCLSID() == __uuidof(CRARFileSource) && HRESULT_FACILITY(hr) == FACILITY_ITF) {
            hrRFS = hr;
        }
    }

    m_deadends.Copy(deadends);

    // If RFS was part of the graph, return its error code instead of the last error code.
    // TODO: Improve filter error reporting to graph manager.
    return hrRFS != S_OK ? hrRFS : hr;
}

STDMETHODIMP CFGManager::AddSourceFilter(LPCWSTR lpcwstrFileName, LPCWSTR lpcwstrFilterName, IBaseFilter** ppFilter)
{
    CAutoLock cAutoLock(this);

    HRESULT hr;
    CFGFilterList fl;

    if (FAILED(hr = EnumSourceFilters(lpcwstrFileName, fl))) {
        return hr;
    }

    POSITION pos = fl.GetHeadPosition();
    while (pos) {
        if (SUCCEEDED(hr = AddSourceFilter(fl.GetNext(pos), lpcwstrFileName, lpcwstrFilterName, ppFilter))) {
            return hr;
        }
    }

    return VFW_E_CANNOT_LOAD_SOURCE_FILTER;
}

STDMETHODIMP CFGManager::SetLogFile(DWORD_PTR hFile)
{
    if (!m_pUnkInner) {
        return E_UNEXPECTED;
    }

    CAutoLock cAutoLock(this);

    return CComQIPtr<IFilterGraph2>(m_pUnkInner)->SetLogFile(hFile);
}

STDMETHODIMP CFGManager::Abort()
{
    if (!m_pUnkInner) {
        ASSERT(false);
        return E_UNEXPECTED;
    }

    // When a filter (renderer) in the child thread (the graph thread) calls CreateWindow()
    // then that call triggers an implicit call of SendMessage to the main window.
    // This is a blocking call, meaning main thread must be able to process that window message.
    // So we can not request a lock here when called from main thread since that would result in a deadlock.
    //CAutoLock cAutoLock(this);

    m_aborted = true;

    return CComQIPtr<IFilterGraph2>(m_pUnkInner)->Abort();
}

STDMETHODIMP CFGManager::ShouldOperationContinue()
{
    if (!m_pUnkInner) {
        return E_UNEXPECTED;
    }

    CAutoLock cAutoLock(this);

    return CComQIPtr<IFilterGraph2>(m_pUnkInner)->ShouldOperationContinue();
}

// IFilterGraph2

STDMETHODIMP CFGManager::AddSourceFilterForMoniker(IMoniker* pMoniker, IBindCtx* pCtx, LPCWSTR lpcwstrFilterName, IBaseFilter** ppFilter)
{
    if (!m_pUnkInner) {
        return E_UNEXPECTED;
    }

    CAutoLock cAutoLock(this);

    return CComQIPtr<IFilterGraph2>(m_pUnkInner)->AddSourceFilterForMoniker(pMoniker, pCtx, lpcwstrFilterName, ppFilter);
}

STDMETHODIMP CFGManager::ReconnectEx(IPin* ppin, const AM_MEDIA_TYPE* pmt)
{
    if (!m_pUnkInner) {
        return E_UNEXPECTED;
    }

    CAutoLock cAutoLock(this);

    return CComQIPtr<IFilterGraph2>(m_pUnkInner)->ReconnectEx(ppin, pmt);
}

STDMETHODIMP CFGManager::RenderEx(IPin* pPinOut, DWORD dwFlags, DWORD* pvContext)
{
    CAutoLock cAutoLock(this);

    m_streampath.RemoveAll();
    m_deadends.RemoveAll();

    if (!pPinOut || dwFlags > AM_RENDEREX_RENDERTOEXISTINGRENDERERS || pvContext) {
        return E_INVALIDARG;
    }

    if (dwFlags & AM_RENDEREX_RENDERTOEXISTINGRENDERERS) {
        CInterfaceList<IBaseFilter> pBFs;

        BeginEnumFilters(this, pEF, pBF) {
            if (CComQIPtr<IAMFilterMiscFlags> pAMMF = pBF) {
                if (pAMMF->GetMiscFlags() & AM_FILTER_MISC_FLAGS_IS_RENDERER) {
                    pBFs.AddTail(pBF);
                }
            } else {
                BeginEnumPins(pBF, pEP, pPin) {
                    CComPtr<IPin> pPinIn;
                    DWORD size = 1;
                    if (SUCCEEDED(pPin->QueryInternalConnections(&pPinIn, &size)) && size == 0) {
                        pBFs.AddTail(pBF);
                        break;
                    }
                }
                EndEnumPins;
            }
        }
        EndEnumFilters;

        while (!pBFs.IsEmpty()) {
            HRESULT hr;
            if (SUCCEEDED(hr = ConnectFilter(pPinOut, pBFs.RemoveHead()))) {
                return hr;
            }
        }

        return VFW_E_CANNOT_RENDER;
    }

    return Connect(pPinOut, (IPin*)nullptr);
}

// IGraphBuilder2

STDMETHODIMP CFGManager::IsPinDirection(IPin* pPin, PIN_DIRECTION dir1)
{
    CAutoLock cAutoLock(this);

    CheckPointer(pPin, E_POINTER);

    PIN_DIRECTION dir2;
    if (FAILED(pPin->QueryDirection(&dir2))) {
        return E_FAIL;
    }

    return dir1 == dir2 ? S_OK : S_FALSE;
}

STDMETHODIMP CFGManager::IsPinConnected(IPin* pPin)
{
    CAutoLock cAutoLock(this);

    CheckPointer(pPin, E_POINTER);

    CComPtr<IPin> pPinTo;
    return SUCCEEDED(pPin->ConnectedTo(&pPinTo)) && pPinTo ? S_OK : S_FALSE;
}

STDMETHODIMP CFGManager::ConnectFilter(IBaseFilter* pBF, IPin* pPinIn)
{
    CAutoLock cAutoLock(this);

    CheckPointer(pBF, E_POINTER);

    if (m_aborted) {
        return E_ABORT;
    }

    if (pPinIn && S_OK != IsPinDirection(pPinIn, PINDIR_INPUT)) {
        return VFW_E_INVALID_DIRECTION;
    }

    int nTotal = 0, nRendered = 0;

    const CAppSettings& s = AfxGetAppSettings();

    BeginEnumPins(pBF, pEP, pPin) {
        if (S_OK == IsPinDirection(pPin, PINDIR_OUTPUT)
                && S_OK != IsPinConnected(pPin)
                && !((s.iDSVideoRendererType != VIDRNDT_DS_EVR_CUSTOM && s.iDSVideoRendererType != VIDRNDT_DS_EVR && s.iDSVideoRendererType != VIDRNDT_DS_SYNC) && GetPinName(pPin)[0] == '~')) {

            CLSID clsid;
            pBF->GetClassID(&clsid);
            // Disable DVD subtitle mixing in EVR (CP) and Sync Renderer for Microsoft DTV-DVD Video Decoder, it corrupts DVD playback.
            if (clsid == CLSID_CMPEG2VidDecoderDS) {
                if (s.iDSVideoRendererType == VIDRNDT_DS_EVR_CUSTOM || s.iDSVideoRendererType == VIDRNDT_DS_SYNC) {
                    if (GetPinName(pPin)[0] == '~') {
                        continue;
                    }
                }
            }
            // No multiple pin for Internal MPEG2 Software Decoder, NVIDIA PureVideo Decoder, Sonic Cinemaster VideoDecoder
            else if (clsid == CLSID_CMpeg2DecFilter
                     || clsid == CLSID_NvidiaVideoDecoder
                     || clsid == CLSID_SonicCinemasterVideoDecoder) {
                if (GetPinName(pPin)[0] == '~') {
                    continue;
                }
                //TODO: enable multiple pins for the renderer, if the video decoder supports DXVA
            }

            m_streampath.Append(pBF, pPin);

            HRESULT hr = Connect(pPin, pPinIn);

            if (SUCCEEDED(hr)) {
                for (int i = (int)m_deadends.GetCount() - 1; i >= 0; i--) {
                    if (m_deadends.GetAt(i)->Compare(m_streampath)) {
                        m_deadends.RemoveAt(i);
                    }
                }
                nRendered++;
            }

            nTotal++;

            m_streampath.RemoveTail();

            if (SUCCEEDED(hr) && pPinIn) {
                return S_OK;
            }
        }
    }
    EndEnumPins;

    return
        nRendered == nTotal ? (nRendered > 0 ? S_OK : S_FALSE) :
        nRendered > 0 ? VFW_S_PARTIAL_RENDER :
        VFW_E_CANNOT_RENDER;
}

STDMETHODIMP CFGManager::ConnectFilter(IPin* pPinOut, IBaseFilter* pBF)
{
    CAutoLock cAutoLock(this);

    CheckPointer(pPinOut, E_POINTER);
    CheckPointer(pBF, E_POINTER);

    if (S_OK != IsPinDirection(pPinOut, PINDIR_OUTPUT)) {
        return VFW_E_INVALID_DIRECTION;
    }

    const CAppSettings& s = AfxGetAppSettings();

    BeginEnumPins(pBF, pEP, pPin) {
        if (S_OK == IsPinDirection(pPin, PINDIR_INPUT)
                && S_OK != IsPinConnected(pPin)
                && !((s.iDSVideoRendererType != VIDRNDT_DS_EVR_CUSTOM && s.iDSVideoRendererType != VIDRNDT_DS_EVR && s.iDSVideoRendererType != VIDRNDT_DS_SYNC) && GetPinName(pPin)[0] == '~')) {
            HRESULT hr = Connect(pPinOut, pPin);
            if (SUCCEEDED(hr)) {
                return hr;
            }
        }
    }
    EndEnumPins;

    return VFW_E_CANNOT_CONNECT;
}

STDMETHODIMP CFGManager::ConnectFilterDirect(IPin* pPinOut, IBaseFilter* pBF, const AM_MEDIA_TYPE* pmt)
{
    CAutoLock cAutoLock(this);

    CheckPointer(pPinOut, E_POINTER);
    CheckPointer(pBF, E_POINTER);

    if (S_OK != IsPinDirection(pPinOut, PINDIR_OUTPUT)) {
        return VFW_E_INVALID_DIRECTION;
    }

    const CAppSettings& s = AfxGetAppSettings();

    BeginEnumPins(pBF, pEP, pPin) {
        if (S_OK == IsPinDirection(pPin, PINDIR_INPUT)
                && S_OK != IsPinConnected(pPin)
                && !((s.iDSVideoRendererType != VIDRNDT_DS_EVR_CUSTOM && s.iDSVideoRendererType != VIDRNDT_DS_EVR && s.iDSVideoRendererType != VIDRNDT_DS_SYNC) && GetPinName(pPin)[0] == '~')) {
            HRESULT hr = ConnectDirect(pPinOut, pPin, pmt);
            if (SUCCEEDED(hr)) {
                return hr;
            }
        }
    }
    EndEnumPins;

    return VFW_E_CANNOT_CONNECT;
}

STDMETHODIMP CFGManager::NukeDownstream(IUnknown* pUnk)
{
    CAutoLock cAutoLock(this);

    if (CComQIPtr<IBaseFilter> pBF = pUnk) {
        BeginEnumPins(pBF, pEP, pPin) {
            NukeDownstream(pPin);
        }
        EndEnumPins;
    } else if (CComQIPtr<IPin> pPin = pUnk) {
        CComPtr<IPin> pPinTo;
        if (S_OK == IsPinDirection(pPin, PINDIR_OUTPUT)
                && SUCCEEDED(pPin->ConnectedTo(&pPinTo)) && pPinTo) {
            if (pBF = GetFilterFromPin(pPinTo)) {
                if (GetCLSID(pBF) == CLSID_EnhancedVideoRenderer) {
                    // GetFilterFromPin() returns pointer to the Base EVR,
                    // but we need to remove Outer EVR from the graph.
                    CComPtr<IBaseFilter> pOuterEVR;
                    if (SUCCEEDED(pBF->QueryInterface(&pOuterEVR))) {
                        pBF = pOuterEVR;
                    }
                }
                NukeDownstream(pBF);
                Disconnect(pPinTo);
                Disconnect(pPin);
                RemoveFilter(pBF);
            }
        }
    } else {
        return E_INVALIDARG;
    }

    return S_OK;
}

STDMETHODIMP CFGManager::FindInterface(REFIID iid, void** ppv, BOOL bRemove)
{
    CAutoLock cAutoLock(this);

    CheckPointer(ppv, E_POINTER);

    for (POSITION pos = m_pUnks.GetHeadPosition(); pos; m_pUnks.GetNext(pos)) {
        if (SUCCEEDED(m_pUnks.GetAt(pos)->QueryInterface(iid, ppv))) {
            if (bRemove) {
                m_pUnks.RemoveAt(pos);
            }
            return S_OK;
        }
    }

    return E_NOINTERFACE;
}

STDMETHODIMP CFGManager::AddToROT()
{
    CAutoLock cAutoLock(this);

    HRESULT hr;

    if (m_dwRegister) {
        return S_FALSE;
    }

    CComPtr<IRunningObjectTable> pROT;
    CComPtr<IMoniker> pMoniker;
    WCHAR wsz[256];
    swprintf_s(wsz, _countof(wsz), L"FilterGraph %08p pid %08x (MPC-HC)", this, GetCurrentProcessId());
    if (SUCCEEDED(hr = GetRunningObjectTable(0, &pROT))
            && SUCCEEDED(hr = CreateItemMoniker(L"!", wsz, &pMoniker))) {
        hr = pROT->Register(ROTFLAGS_REGISTRATIONKEEPSALIVE, (IGraphBuilder2*)this, pMoniker, &m_dwRegister);
    }

    return hr;
}

STDMETHODIMP CFGManager::RemoveFromROT()
{
    if (!m_dwRegister) {
        return S_FALSE;
    }

    //CAutoLock cAutoLock(this);

    HRESULT hr;
    CComPtr<IRunningObjectTable> pROT;
    if (SUCCEEDED(hr = GetRunningObjectTable(0, &pROT))
            && SUCCEEDED(hr = pROT->Revoke(m_dwRegister))) {
        m_dwRegister = 0;
    }

    return hr;
}

// IGraphBuilderDeadEnd

STDMETHODIMP_(size_t) CFGManager::GetCount()
{
    CAutoLock cAutoLock(this);

    return m_deadends.GetCount();
}

STDMETHODIMP CFGManager::GetDeadEnd(int iIndex, CAtlList<CStringW>& path, CAtlList<CMediaType>& mts)
{
    CAutoLock cAutoLock(this);

    if (iIndex < 0 || iIndex >= (int)m_deadends.GetCount()) {
        return E_FAIL;
    }

    path.RemoveAll();
    mts.RemoveAll();

    POSITION pos = m_deadends[iIndex]->GetHeadPosition();
    while (pos) {
        const path_t& p = m_deadends[iIndex]->GetNext(pos);

        CStringW str;
        str.Format(L"%s::%s", p.filter.GetString(), p.pin.GetString());
        path.AddTail(str);
    }

    mts.AddTailList(&m_deadends[iIndex]->mts);

    return S_OK;
}

//
//  Custom Filter Injection
//

void CFGManagerCustom::InsertLAVSplitterSource(bool IsPreview)
{
    const CAppSettings& s = AfxGetAppSettings();
    const bool* src = s.SrcFilters;

    CFGFilterLAV* filter;
    if (IsPreview) {
        filter = CFGFilterLAV::CreateFilterPreview(CFGFilterLAV::SPLITTER_SOURCE);
    } else {
        filter = CFGFilterLAV::CreateFilter(CFGFilterLAV::SPLITTER_SOURCE);
    }
    CAutoPtr<CFGFilterLAVSplitterBase> pFGLAVSplitterSource(static_cast<CFGFilterLAVSplitterBase*> (filter));

#if INTERNAL_SOURCEFILTER_AVI
    if (src[SRC_AVI] || IsPreview) {
        pFGLAVSplitterSource->m_chkbytes.AddTail(_T("0,4,,52494646,8,4,,41564920"));
        pFGLAVSplitterSource->m_chkbytes.AddTail(_T("0,4,,52494646,8,4,,41564958"));
        pFGLAVSplitterSource->AddEnabledFormat("avi");
    }
#endif

#if INTERNAL_SOURCEFILTER_AVS
    if (src[SRC_AVS] || IsPreview) {
        pFGLAVSplitterSource->m_extensions.AddTail(_T(".avs"));
        pFGLAVSplitterSource->AddEnabledFormat("avisynth");
    }
#endif

#if INTERNAL_SOURCEFILTER_MP4
    if (src[SRC_MP4] || IsPreview) {
        pFGLAVSplitterSource->m_chkbytes.AddTail(_T("4,4,,66747970")); // ftyp
        pFGLAVSplitterSource->m_chkbytes.AddTail(_T("4,4,,6d6f6f76")); // moov
        pFGLAVSplitterSource->m_chkbytes.AddTail(_T("4,4,,6d646174")); // mdat
        pFGLAVSplitterSource->m_chkbytes.AddTail(_T("4,4,,736b6970")); // skip
        pFGLAVSplitterSource->m_chkbytes.AddTail(_T("4,12,ffffffff00000000ffffffff,77696465027fe3706d646174")); // wide ? mdat
        pFGLAVSplitterSource->m_chkbytes.AddTail(_T("3,3,,000001")); // raw mpeg4 video
        pFGLAVSplitterSource->m_extensions.AddTail(_T(".mov"));
        pFGLAVSplitterSource->m_extensions.AddTail(_T(".mp4"));
        pFGLAVSplitterSource->AddEnabledFormat("mp4");
    }
#endif

#if INTERNAL_SOURCEFILTER_FLV
    if (src[SRC_FLV] || IsPreview) {
        pFGLAVSplitterSource->m_chkbytes.AddTail(_T("0,4,,464C5601")); // FLV (v1)
        pFGLAVSplitterSource->AddEnabledFormat("flv");
    }
#endif

#if INTERNAL_SOURCEFILTER_GIF
    if (src[SRC_GIF] && !IsPreview) {
        pFGLAVSplitterSource->m_extensions.AddTail(_T(".gif"));
        pFGLAVSplitterSource->AddEnabledFormat("gif");
    }
#endif

#if INTERNAL_SOURCEFILTER_ASF
    if (src[SRC_ASF] || IsPreview) {
        pFGLAVSplitterSource->m_extensions.AddTail(_T(".wmv"));
        pFGLAVSplitterSource->m_extensions.AddTail(_T(".asf"));
        pFGLAVSplitterSource->m_extensions.AddTail(_T(".dvr-ms"));
        pFGLAVSplitterSource->AddEnabledFormat("asf");
    }
#endif

#if INTERNAL_SOURCEFILTER_WTV
    if (src[SRC_WTV] || IsPreview) {
        pFGLAVSplitterSource->m_extensions.AddTail(_T(".wtv"));
        pFGLAVSplitterSource->AddEnabledFormat("wtv");
    }
#endif

#if INTERNAL_SOURCEFILTER_MATROSKA
    if (src[SRC_MATROSKA] || IsPreview) {
        pFGLAVSplitterSource->m_chkbytes.AddTail(_T("0,4,,1A45DFA3"));
        pFGLAVSplitterSource->m_extensions.AddTail(_T(".mkv"));
        pFGLAVSplitterSource->AddEnabledFormat("matroska");
    }
#endif

#if INTERNAL_SOURCEFILTER_REALMEDIA
    if (src[SRC_REALMEDIA] || IsPreview) {
        pFGLAVSplitterSource->m_chkbytes.AddTail(_T("0,4,,2E524D46"));
        pFGLAVSplitterSource->AddEnabledFormat("rm");
    }
#endif

#if INTERNAL_SOURCEFILTER_FLIC
    if (src[SRC_FLIC] || IsPreview) {
        pFGLAVSplitterSource->m_chkbytes.AddTail(_T("4,2,,11AF"));
        pFGLAVSplitterSource->m_chkbytes.AddTail(_T("4,2,,12AF"));
        pFGLAVSplitterSource->m_extensions.AddTail(_T(".fli"));
        pFGLAVSplitterSource->m_extensions.AddTail(_T(".flc"));
        pFGLAVSplitterSource->AddEnabledFormat("flic");
    }
#endif

#if INTERNAL_SOURCEFILTER_FLAC
    if (src[SRC_FLAC] && !IsPreview) {
        pFGLAVSplitterSource->m_chkbytes.AddTail(_T("0,4,,664C6143"));
        pFGLAVSplitterSource->m_extensions.AddTail(_T(".flac"));
        pFGLAVSplitterSource->AddEnabledFormat("flac");
    }
#endif

#if INTERNAL_SOURCEFILTER_OGG
    if (src[SRC_OGG] || IsPreview) {
        pFGLAVSplitterSource->m_chkbytes.AddTail(_T("0,4,,4F676753"));
        pFGLAVSplitterSource->AddEnabledFormat("ogg");
    }
#endif

#if INTERNAL_SOURCEFILTER_MPEG
    if (src[SRC_MPEG] || IsPreview) {
        pFGLAVSplitterSource->m_chkbytes.AddTail(_T("0,16,FFFFFFFFF100010001800001FFFFFFFF,000001BA2100010001800001000001BB"));
        pFGLAVSplitterSource->m_chkbytes.AddTail(_T("0,5,FFFFFFFFC0,000001BA40"));
        pFGLAVSplitterSource->m_chkbytes.AddTail(_T("0,4,,54467263,1660,1,,47"));
        pFGLAVSplitterSource->m_chkbytes.AddTail(_T("0,8,fffffc00ffe00000,4156000055000000"));
        pFGLAVSplitterSource->AddEnabledFormat("mpeg");
        pFGLAVSplitterSource->AddEnabledFormat("mpegraw");
    }
    if (src[SRC_MPEGTS] || IsPreview) {
        pFGLAVSplitterSource->m_chkbytes.AddTail(_T("0,1,,47,188,1,,47,376,1,,47"));
        pFGLAVSplitterSource->m_chkbytes.AddTail(_T("4,1,,47,196,1,,47,388,1,,47"));
        pFGLAVSplitterSource->m_extensions.AddTail(_T(".ts")); // for some broken .ts
        pFGLAVSplitterSource->AddEnabledFormat("mpegts");
    }
    if (src[SRC_MPEG] || src[SRC_MPEGTS] || IsPreview) {
        // for Blu-ray playback
        pFGLAVSplitterSource->m_chkbytes.AddTail(_T("0,4,,494E4458")); // INDX (index.bdmv)
        pFGLAVSplitterSource->m_chkbytes.AddTail(_T("0,4,,4D4F424A")); // MOBJ (MovieObject.bdmv)
        pFGLAVSplitterSource->m_chkbytes.AddTail(_T("0,4,,4D504C53")); // MPLS
    }
#endif

#if INTERNAL_SOURCEFILTER_AC3
    if (src[SRC_AC3] && !IsPreview) {
        pFGLAVSplitterSource->m_chkbytes.AddTail(_T("0,2,,0B77"));                          // AC3, E-AC3
        pFGLAVSplitterSource->m_chkbytes.AddTail(_T("4,4,,F8726FBB"));                      // MLP
        pFGLAVSplitterSource->m_extensions.AddTail(_T(".ac3"));
        pFGLAVSplitterSource->m_extensions.AddTail(_T(".eac3"));
        pFGLAVSplitterSource->m_extensions.AddTail(_T(".mlp"));
        pFGLAVSplitterSource->m_extensions.AddTail(_T(".truehd"));
        pFGLAVSplitterSource->AddEnabledFormat("ac3");
        pFGLAVSplitterSource->AddEnabledFormat("eac3");
        pFGLAVSplitterSource->AddEnabledFormat("mlp");
        pFGLAVSplitterSource->AddEnabledFormat("truehd");
    }
#endif

#if INTERNAL_SOURCEFILTER_DTS
    if (src[SRC_DTS] && !IsPreview) {
        pFGLAVSplitterSource->m_chkbytes.AddTail(_T("0,4,,7FFE8001"));                      // DTS
        pFGLAVSplitterSource->m_chkbytes.AddTail(_T("0,4,,fE7f0180"));                      // DTS LE
        pFGLAVSplitterSource->m_chkbytes.AddTail(_T("0,4,,52494646,8,8,,57415645666D7420"));// RIFFxxxxWAVEfmt_ for DTSWAV
        pFGLAVSplitterSource->m_extensions.AddTail(_T(".dts"));
        pFGLAVSplitterSource->m_extensions.AddTail(_T(".dtshd"));
        pFGLAVSplitterSource->m_extensions.AddTail(_T(".dtsma"));
        pFGLAVSplitterSource->AddEnabledFormat("dts");
        pFGLAVSplitterSource->AddEnabledFormat("dtshd");
    }
#endif

#if INTERNAL_SOURCEFILTER_MPEGAUDIO
    if (src[SRC_MPA] && !IsPreview) {
        pFGLAVSplitterSource->m_chkbytes.AddTail(_T("0,2,FFE0,FFE0"));
        pFGLAVSplitterSource->m_chkbytes.AddTail(_T("0,10,FFFFFF00000080808080,49443300000000000000"));
        pFGLAVSplitterSource->AddEnabledFormat("mp3");
    }
#endif

#if INTERNAL_SOURCEFILTER_HTTP
    if (src[SRC_HTTP] && !IsPreview) {
        pFGLAVSplitterSource->m_protocols.AddTail(_T("http"));
        pFGLAVSplitterSource->m_protocols.AddTail(_T("https"));
        pFGLAVSplitterSource->m_protocols.AddTail(_T("icyx"));
        pFGLAVSplitterSource->AddEnabledFormat("http");
        pFGLAVSplitterSource->AddEnabledFormat("dash");
        pFGLAVSplitterSource->AddEnabledFormat("hls");
    }
#endif

#if INTERNAL_SOURCEFILTER_RTSP
    if (src[SRC_RTSP] && !IsPreview) {
        pFGLAVSplitterSource->m_protocols.AddTail(_T("rtsp"));
        // Add transport protocol specific RTSP URL handlers
        pFGLAVSplitterSource->m_protocols.AddTail(_T("rtspu")); // UDP
        pFGLAVSplitterSource->m_protocols.AddTail(_T("rtspm")); // UDP multicast
        pFGLAVSplitterSource->m_protocols.AddTail(_T("rtspt")); // TCP
        pFGLAVSplitterSource->m_protocols.AddTail(_T("rtsph")); // HTTP
        pFGLAVSplitterSource->AddEnabledFormat("rtsp");
    }
#endif

#if INTERNAL_SOURCEFILTER_UDP
    if (src[SRC_UDP] && !IsPreview) {
        pFGLAVSplitterSource->m_protocols.AddTail(_T("udp"));
        pFGLAVSplitterSource->AddEnabledFormat("udp");
    }
#endif

#if INTERNAL_SOURCEFILTER_RTP
    if (src[SRC_RTP] && !IsPreview) {
        pFGLAVSplitterSource->m_protocols.AddTail(_T("rtp"));
        pFGLAVSplitterSource->AddEnabledFormat("rtp");
    }
#endif

#if INTERNAL_SOURCEFILTER_MMS
    if (src[SRC_MMS] && !IsPreview) {
        pFGLAVSplitterSource->m_protocols.AddTail(_T("mms"));
        pFGLAVSplitterSource->m_protocols.AddTail(_T("mmsh"));
        pFGLAVSplitterSource->m_protocols.AddTail(_T("mmst"));
        pFGLAVSplitterSource->AddEnabledFormat("mms");
    }
#endif

#if INTERNAL_SOURCEFILTER_RTMP
    if (src[SRC_RTMP] && !IsPreview) {
        pFGLAVSplitterSource->m_protocols.AddTail(_T("rtmp"));
        pFGLAVSplitterSource->m_protocols.AddTail(_T("rtmpt"));
        pFGLAVSplitterSource->AddEnabledFormat("live_flv");
    }
#endif

#if INTERNAL_SOURCEFILTER_APE
    if (src[SRC_APE] || IsPreview) {
        pFGLAVSplitterSource->m_extensions.AddTail(_T(".ape"));
        pFGLAVSplitterSource->AddEnabledFormat("ape");
    }
#endif

#if INTERNAL_SOURCEFILTER_MISC
    // ToDo: split into separate options
    if (src[SRC_MISC] || IsPreview) {
        // video
        pFGLAVSplitterSource->m_extensions.AddTail(_T(".dv"));
        pFGLAVSplitterSource->m_extensions.AddTail(_T(".dhav"));
        pFGLAVSplitterSource->m_extensions.AddTail(_T(".m3u8"));
        pFGLAVSplitterSource->m_extensions.AddTail(_T(".y4m"));
        pFGLAVSplitterSource->AddEnabledFormat("dv");
        pFGLAVSplitterSource->AddEnabledFormat("dhav");
        pFGLAVSplitterSource->AddEnabledFormat("y4m");
    }
    if (src[SRC_MISC] && !IsPreview) {
        // raw video
        pFGLAVSplitterSource->m_extensions.AddTail(_T(".264"));
        pFGLAVSplitterSource->m_extensions.AddTail(_T(".265"));
        pFGLAVSplitterSource->m_extensions.AddTail(_T(".h264"));
        pFGLAVSplitterSource->m_extensions.AddTail(_T(".h265"));
        pFGLAVSplitterSource->m_extensions.AddTail(_T(".av1"));
        pFGLAVSplitterSource->m_extensions.AddTail(_T(".m4v"));
        pFGLAVSplitterSource->m_extensions.AddTail(_T(".apv"));
        pFGLAVSplitterSource->AddEnabledFormat("av1");
        pFGLAVSplitterSource->AddEnabledFormat("m4v");
        pFGLAVSplitterSource->AddEnabledFormat("rawvideo");
        pFGLAVSplitterSource->AddEnabledFormat("apv");
        // audio
        pFGLAVSplitterSource->m_extensions.AddTail(_T(".amr"));
        pFGLAVSplitterSource->m_extensions.AddTail(_T(".mpc"));
        pFGLAVSplitterSource->m_extensions.AddTail(_T(".w64"));
        pFGLAVSplitterSource->m_extensions.AddTail(_T(".wav"));
        pFGLAVSplitterSource->m_extensions.AddTail(_T(".wv"));
        pFGLAVSplitterSource->AddEnabledFormat("amr");
        pFGLAVSplitterSource->AddEnabledFormat("mpc");
        pFGLAVSplitterSource->AddEnabledFormat("mpc8");
        pFGLAVSplitterSource->AddEnabledFormat("w64");
        pFGLAVSplitterSource->AddEnabledFormat("wav");
        pFGLAVSplitterSource->AddEnabledFormat("wv");
    }
#endif

    // Always register the pipe protocol to allow handling standard input
    pFGLAVSplitterSource->m_protocols.AddTail(_T("pipe"));

    // Add LAV Source Filter if needed
    if (!pFGLAVSplitterSource->m_extensions.IsEmpty()
        || !pFGLAVSplitterSource->m_chkbytes.IsEmpty()
        || !pFGLAVSplitterSource->m_protocols.IsEmpty()) {
        m_source.AddTail(pFGLAVSplitterSource.Detach());
    }
}

void CFGManagerCustom::InsertLAVSplitter(bool IsPreview)
{
    const CAppSettings& s = AfxGetAppSettings();
    const bool* src = s.SrcFilters;

    CFGFilterLAV* filterHM, * filterLM;
    if (IsPreview) {
        filterHM = CFGFilterLAV::CreateFilterPreview(CFGFilterLAV::SPLITTER, MERIT64_ABOVE_DSHOW);
        filterLM = CFGFilterLAV::CreateFilterPreview(CFGFilterLAV::SPLITTER, MERIT64_DO_USE, true);
    } else {
        filterHM = CFGFilterLAV::CreateFilter(CFGFilterLAV::SPLITTER, MERIT64_ABOVE_DSHOW);
        filterLM = CFGFilterLAV::CreateFilter(CFGFilterLAV::SPLITTER, MERIT64_DO_USE, true);
    }
    CAutoPtr<CFGFilterLAVSplitterBase> pFGLAVSplitter(static_cast<CFGFilterLAVSplitterBase*>(filterHM));
    CAutoPtr<CFGFilterLAVSplitterBase> pFGLAVSplitterLM(static_cast<CFGFilterLAVSplitterBase*>(filterLM));

#if INTERNAL_SOURCEFILTER_MATROSKA
    if (src[SRC_MATROSKA] || IsPreview) {
        pFGLAVSplitter->AddType(MEDIATYPE_Stream, MEDIASUBTYPE_Matroska);
        pFGLAVSplitter->AddEnabledFormat("matroska");
    }
#endif

#if INTERNAL_SOURCEFILTER_REALMEDIA
    if (src[SRC_REALMEDIA] || IsPreview) {
        pFGLAVSplitter->AddType(MEDIATYPE_Stream, MEDIASUBTYPE_RealMedia);
        pFGLAVSplitter->AddEnabledFormat("rm");
    }
#endif

#if INTERNAL_SOURCEFILTER_AVI
    if (src[SRC_AVI] || IsPreview) {
        pFGLAVSplitter->AddType(MEDIATYPE_Stream, MEDIASUBTYPE_Avi);
        pFGLAVSplitter->AddEnabledFormat("avi");
    }
#endif

#if INTERNAL_SOURCEFILTER_AVS
    if (src[SRC_AVS] || IsPreview) {
        pFGLAVSplitter->AddEnabledFormat("avisynth");
    }
#endif

#if INTERNAL_SOURCEFILTER_OGG
    if (src[SRC_OGG] || IsPreview) {
        pFGLAVSplitter->AddType(MEDIATYPE_Stream, MEDIASUBTYPE_Ogg);
        pFGLAVSplitter->AddEnabledFormat("ogg");
    }
#endif

#if INTERNAL_SOURCEFILTER_MPEG
    if (src[SRC_MPEG] || IsPreview) {
        pFGLAVSplitter->AddType(MEDIATYPE_Stream, MEDIASUBTYPE_MPEG1System);
        pFGLAVSplitter->AddType(MEDIATYPE_Stream, MEDIASUBTYPE_MPEG2_PROGRAM);
        pFGLAVSplitter->AddType(MEDIATYPE_Stream, MEDIASUBTYPE_MPEG2_PVA);
        pFGLAVSplitter->AddEnabledFormat("mpeg");
        pFGLAVSplitter->AddEnabledFormat("mpegraw");
    }
    if (src[SRC_MPEGTS] || IsPreview) {
        pFGLAVSplitter->AddType(MEDIATYPE_Stream, MEDIASUBTYPE_MPEG2_TRANSPORT);
        pFGLAVSplitter->AddEnabledFormat("mpegts");
    }
#endif

#if INTERNAL_SOURCEFILTER_AC3
    if (src[SRC_AC3] && !IsPreview) {
        pFGLAVSplitter->AddType(MEDIATYPE_Stream, MEDIASUBTYPE_DOLBY_AC3);
        pFGLAVSplitter->AddType(MEDIATYPE_Stream, MEDIASUBTYPE_DOLBY_TRUEHD);
        pFGLAVSplitter->AddType(MEDIATYPE_Stream, MEDIASUBTYPE_DOLBY_DDPLUS);
        pFGLAVSplitter->AddType(MEDIATYPE_Stream, MEDIASUBTYPE_MLP);
        pFGLAVSplitter->AddEnabledFormat("ac3");
        pFGLAVSplitter->AddEnabledFormat("eac3");
    }
#endif

#if INTERNAL_SOURCEFILTER_DTS
    if (src[SRC_DTS] && !IsPreview) {
        pFGLAVSplitter->AddType(MEDIATYPE_Stream, MEDIASUBTYPE_DTS);
        pFGLAVSplitter->AddType(MEDIATYPE_Stream, MEDIASUBTYPE_DTS_HD);
        pFGLAVSplitter->AddEnabledFormat("dts");
        pFGLAVSplitter->AddEnabledFormat("dtshd");
    }
#endif

#if INTERNAL_SOURCEFILTER_MPEGAUDIO
    if (src[SRC_MPA] && !IsPreview) {
        pFGLAVSplitter->AddType(MEDIATYPE_Stream, MEDIASUBTYPE_MPEG1Audio);
        pFGLAVSplitter->AddEnabledFormat("mp3");
    }
#endif

#if INTERNAL_SOURCEFILTER_MP4
    if (src[SRC_MP4] || IsPreview) {
        pFGLAVSplitter->AddType(MEDIATYPE_Stream, MEDIASUBTYPE_MP4);
        pFGLAVSplitter->AddEnabledFormat("mp4");
    }
#endif

#if INTERNAL_SOURCEFILTER_FLV
    if (src[SRC_FLV] || IsPreview) {
        pFGLAVSplitter->AddType(MEDIATYPE_Stream, MEDIASUBTYPE_FLV);
        pFGLAVSplitter->AddEnabledFormat("flv");
    }
#endif

#if INTERNAL_SOURCEFILTER_ASF
    if (src[SRC_ASF] || IsPreview) {
        pFGLAVSplitter->AddType(MEDIATYPE_Stream, MEDIASUBTYPE_ASF);
        pFGLAVSplitter->AddEnabledFormat("asf");
    }
#endif

    // Add LAV Splitter if needed
    if (!pFGLAVSplitter->GetTypes().IsEmpty()) {
        m_transform.AddTail(pFGLAVSplitter.Detach());
    }

    if (!IsPreview && pFGLAVSplitterLM) {
        // Add low merit LAV Splitter
        pFGLAVSplitterLM->AddType(MEDIATYPE_Stream, MEDIASUBTYPE_NULL);
        pFGLAVSplitterLM->AddEnabledFormat("*");
        // Explicitly disable all common subtitles format
        pFGLAVSplitterLM->AddDisabledFormat("aqtitle");
        pFGLAVSplitterLM->AddDisabledFormat("ass");
        pFGLAVSplitterLM->AddDisabledFormat("dvbsub");
        pFGLAVSplitterLM->AddDisabledFormat("dvbtxt");
        pFGLAVSplitterLM->AddDisabledFormat("jacosub");
        pFGLAVSplitterLM->AddDisabledFormat("lrc");
        pFGLAVSplitterLM->AddDisabledFormat("microdvd");
        pFGLAVSplitterLM->AddDisabledFormat("mpl2");
        pFGLAVSplitterLM->AddDisabledFormat("mpsub");
        pFGLAVSplitterLM->AddDisabledFormat("realtext");
        pFGLAVSplitterLM->AddDisabledFormat("sami");
        pFGLAVSplitterLM->AddDisabledFormat("srt");
        pFGLAVSplitterLM->AddDisabledFormat("stl");
        pFGLAVSplitterLM->AddDisabledFormat("subviewer");
        pFGLAVSplitterLM->AddDisabledFormat("subviewer1");
        pFGLAVSplitterLM->AddDisabledFormat("sup");
        pFGLAVSplitterLM->AddDisabledFormat("vobsub");
        pFGLAVSplitterLM->AddDisabledFormat("vplayer");
        pFGLAVSplitterLM->AddDisabledFormat("webvtt");
        m_transform.AddTail(pFGLAVSplitterLM.Detach());
    }
}

void CFGManagerCustom::InsertOtherInternalSourcefilters(bool IsPreview)
{
    const CAppSettings& s = AfxGetAppSettings();
    const bool* src = s.SrcFilters;
    CFGFilter* pFGF;

    bool isurl = PathUtils::IsURL(m_input);
    bool isfile = !isurl && !m_input.IsEmpty();
    CString ext;
    bool willuselav = false; // some of the most common ext that certainly won't use any of the filters below
    if (isfile) {
        ext = CPath(m_input).GetExtension().MakeLower();
        willuselav = (ext == L".mkv") || (ext == L".mp4") || (ext == L".avi") || (ext == L".mp3") || (ext == L".m4a") || (ext == L".ogg") || (ext == L".flac");
    }

#if INTERNAL_SOURCEFILTER_RFS
    if ((src[SRC_RFS] || IsPreview) && isfile && !willuselav) {
        pFGF = DEBUG_NEW CFGFilterInternal<CRARFileSource>();
        pFGF->m_chkbytes.AddTail(_T("0,7,,526172211A0700"));   // rar4 signature
        pFGF->m_chkbytes.AddTail(_T("0,8,,526172211A070100")); // rar5 signature
        pFGF->m_extensions.AddTail(_T(".rar"));
        m_source.AddTail(pFGF);
    }
#endif

#if INTERNAL_SOURCEFILTER_CDDA
    if (src[SRC_CDDA] && !IsPreview && isfile && (ext == L".cda")) {
        pFGF = DEBUG_NEW CFGFilterInternal<CCDDAReader>();
        pFGF->m_extensions.AddTail(_T(".cda"));
        m_source.AddTail(pFGF);
    }
#endif

#if INTERNAL_SOURCEFILTER_CDXA
    if ((src[SRC_CDXA] || IsPreview) && isfile && !willuselav) {
        pFGF = DEBUG_NEW CFGFilterInternal<CCDXAReader>();
        pFGF->m_chkbytes.AddTail(_T("0,4,,52494646,8,4,,43445841"));
        m_source.AddTail(pFGF);
    }
#endif

#if INTERNAL_SOURCEFILTER_VTS
    if ((src[SRC_VTS] || IsPreview)  && isfile && !willuselav) {
        pFGF = DEBUG_NEW CFGFilterInternal<CVTSReader>();
        pFGF->m_chkbytes.AddTail(_T("0,12,,445644564944454F2D565453"));
        m_source.AddTail(pFGF);
    }
#endif

#if INTERNAL_SOURCEFILTER_DSM
    if (isfile && (ext == L".dsm" || ext == L".dsv" || ext == L".dsa" || ext == L".dss")) {
        if (src[SRC_DSM] || IsPreview) {
            pFGF = DEBUG_NEW CFGFilterInternal<CDSMSourceFilter>();
            pFGF->m_chkbytes.AddTail(_T("0,4,,44534D53"));
            m_source.AddTail(pFGF);
        }

        if (src[SRC_DSM]) {
            pFGF = DEBUG_NEW CFGFilterInternal<CDSMSplitterFilter>(DSMSplitterName, MERIT64_ABOVE_DSHOW);
        } else {
            pFGF = DEBUG_NEW CFGFilterInternal<CDSMSplitterFilter>(LowMerit(DSMSplitterName), MERIT64_DO_USE);
        }
        pFGF->AddType(MEDIATYPE_Stream, MEDIASUBTYPE_DirectShowMedia);
        pFGF->AddType(MEDIATYPE_Stream, GUID_NULL);
        m_transform.AddTail(pFGF);
    }
#endif
}

void CFGManagerCustom::InsertLAVVideo(bool IsPreview)
{
    const CAppSettings& s = AfxGetAppSettings();
    const bool* tra = s.TraFilters;
    CFGFilter* pFGF;

    CAutoPtr<CFGFilterLAV> pFGLAVVideo  (IsPreview ? CFGFilterLAV::CreateFilterPreview(CFGFilterLAV::VIDEO_DECODER, MERIT64_ABOVE_DSHOW)  : CFGFilterLAV::CreateFilter(CFGFilterLAV::VIDEO_DECODER, MERIT64_ABOVE_DSHOW));
    CAutoPtr<CFGFilterLAV> pFGLAVVideoLM(IsPreview ? CFGFilterLAV::CreateFilterPreview(CFGFilterLAV::VIDEO_DECODER, MERIT64_LOWEST+5, true) : CFGFilterLAV::CreateFilter(CFGFilterLAV::VIDEO_DECODER, MERIT64_LOWEST+5, true));
    
#if INTERNAL_DECODER_MPEG1
    pFGF = IsPreview || tra[TRA_MPEG1] ? pFGLAVVideo : pFGLAVVideoLM;
    pFGF->AddType(MEDIATYPE_Video, MEDIASUBTYPE_MPEG1Packet);
    pFGF->AddType(MEDIATYPE_Video, MEDIASUBTYPE_MPEG1Payload);
#endif
#if INTERNAL_DECODER_FLV
    pFGF = IsPreview || tra[TRA_FLV4] ? pFGLAVVideo : pFGLAVVideoLM;
    pFGF->AddType(MEDIATYPE_Video, MEDIASUBTYPE_FLV1);
    pFGF->AddType(MEDIATYPE_Video, MEDIASUBTYPE_flv1);
    pFGF->AddType(MEDIATYPE_Video, MEDIASUBTYPE_FLV4);
    pFGF->AddType(MEDIATYPE_Video, MEDIASUBTYPE_flv4);
#endif
#if INTERNAL_DECODER_VP356
    pFGF = IsPreview || tra[TRA_VP356] ? pFGLAVVideo : pFGLAVVideoLM;
    pFGF->AddType(MEDIATYPE_Video, MEDIASUBTYPE_VP30);
    pFGF->AddType(MEDIATYPE_Video, MEDIASUBTYPE_VP31);
    pFGF->AddType(MEDIATYPE_Video, MEDIASUBTYPE_VP40);
    pFGF->AddType(MEDIATYPE_Video, MEDIASUBTYPE_VP50);
    pFGF->AddType(MEDIATYPE_Video, MEDIASUBTYPE_vp50);
    pFGF->AddType(MEDIATYPE_Video, MEDIASUBTYPE_VP60);
    pFGF->AddType(MEDIATYPE_Video, MEDIASUBTYPE_vp60);
    pFGF->AddType(MEDIATYPE_Video, MEDIASUBTYPE_VP61);
    pFGF->AddType(MEDIATYPE_Video, MEDIASUBTYPE_vp61);
    pFGF->AddType(MEDIATYPE_Video, MEDIASUBTYPE_VP62);
    pFGF->AddType(MEDIATYPE_Video, MEDIASUBTYPE_vp62);
    pFGF->AddType(MEDIATYPE_Video, MEDIASUBTYPE_VP6F);
    pFGF->AddType(MEDIATYPE_Video, MEDIASUBTYPE_vp6f);
    pFGF->AddType(MEDIATYPE_Video, MEDIASUBTYPE_VP6A);
    pFGF->AddType(MEDIATYPE_Video, MEDIASUBTYPE_vp6a);
#endif
#if INTERNAL_DECODER_H264
    pFGF = IsPreview || tra[TRA_H264] ? pFGLAVVideo : pFGLAVVideoLM;
    pFGF->AddType(MEDIATYPE_Video, MEDIASUBTYPE_H264);
    pFGF->AddType(MEDIATYPE_Video, MEDIASUBTYPE_h264);
    pFGF->AddType(MEDIATYPE_Video, MEDIASUBTYPE_X264);
    pFGF->AddType(MEDIATYPE_Video, MEDIASUBTYPE_x264);
    pFGF->AddType(MEDIATYPE_Video, MEDIASUBTYPE_VSSH);
    pFGF->AddType(MEDIATYPE_Video, MEDIASUBTYPE_vssh);
    pFGF->AddType(MEDIATYPE_Video, MEDIASUBTYPE_DAVC);
    pFGF->AddType(MEDIATYPE_Video, MEDIASUBTYPE_davc);
    pFGF->AddType(MEDIATYPE_Video, MEDIASUBTYPE_PAVC);
    pFGF->AddType(MEDIATYPE_Video, MEDIASUBTYPE_pavc);
    pFGF->AddType(MEDIATYPE_Video, MEDIASUBTYPE_AVC1);
    pFGF->AddType(MEDIATYPE_Video, MEDIASUBTYPE_avc1);
    pFGF->AddType(MEDIATYPE_Video, MEDIASUBTYPE_H264_bis);
    pFGF->AddType(MEDIATYPE_Video, MEDIASUBTYPE_CCV1);
#endif
#if INTERNAL_DECODER_HEVC
    pFGF = IsPreview || tra[TRA_HEVC] ? pFGLAVVideo : pFGLAVVideoLM;
    pFGF->AddType(MEDIATYPE_Video, MEDIASUBTYPE_HVC1);
    pFGF->AddType(MEDIATYPE_Video, MEDIASUBTYPE_HEVC);
    pFGF->AddType(MEDIATYPE_Video, MEDIASUBTYPE_HM10);
    pFGF->AddType(MEDIATYPE_Video, MEDIASUBTYPE_H265);
#endif
#if INTERNAL_DECODER_VVC
    pFGF = IsPreview || tra[TRA_VVC] ? pFGLAVVideo : pFGLAVVideoLM;
    pFGF->AddType(MEDIATYPE_Video, MEDIASUBTYPE_VVC1);
#endif
#if INTERNAL_DECODER_AV1
    pFGF = IsPreview || tra[TRA_AV1] ? pFGLAVVideo : pFGLAVVideoLM;
    pFGF->AddType(MEDIATYPE_Video, MEDIASUBTYPE_AV01);
#endif
#if INTERNAL_DECODER_VC1
    pFGF = IsPreview || tra[TRA_VC1] ? pFGLAVVideo : pFGLAVVideoLM;
    pFGF->AddType(MEDIATYPE_Video, MEDIASUBTYPE_WVC1);
    pFGF->AddType(MEDIATYPE_Video, MEDIASUBTYPE_wvc1);
#endif
#if INTERNAL_DECODER_XVID
    pFGF = IsPreview || tra[TRA_XVID] ? pFGLAVVideo : pFGLAVVideoLM;
    pFGF->AddType(MEDIATYPE_Video, MEDIASUBTYPE_XVID);
    pFGF->AddType(MEDIATYPE_Video, MEDIASUBTYPE_xvid);
    pFGF->AddType(MEDIATYPE_Video, MEDIASUBTYPE_XVIX);
    pFGF->AddType(MEDIATYPE_Video, MEDIASUBTYPE_xvix);
    pFGF->AddType(MEDIATYPE_Video, MEDIASUBTYPE_MP4V);
    pFGF->AddType(MEDIATYPE_Video, MEDIASUBTYPE_mp4v);
    pFGF->AddType(MEDIATYPE_Video, MEDIASUBTYPE_M4S2);
    pFGF->AddType(MEDIATYPE_Video, MEDIASUBTYPE_m4s2);
    pFGF->AddType(MEDIATYPE_Video, MEDIASUBTYPE_MP4S);
    pFGF->AddType(MEDIATYPE_Video, MEDIASUBTYPE_mp4s);
    pFGF->AddType(MEDIATYPE_Video, MEDIASUBTYPE_3IV1);
    pFGF->AddType(MEDIATYPE_Video, MEDIASUBTYPE_3iv1);
    pFGF->AddType(MEDIATYPE_Video, MEDIASUBTYPE_3IV2);
    pFGF->AddType(MEDIATYPE_Video, MEDIASUBTYPE_3iv2);
    pFGF->AddType(MEDIATYPE_Video, MEDIASUBTYPE_3IVX);
    pFGF->AddType(MEDIATYPE_Video, MEDIASUBTYPE_3ivx);
    pFGF->AddType(MEDIATYPE_Video, MEDIASUBTYPE_BLZ0);
    pFGF->AddType(MEDIATYPE_Video, MEDIASUBTYPE_blz0);
    pFGF->AddType(MEDIATYPE_Video, MEDIASUBTYPE_DM4V);
    pFGF->AddType(MEDIATYPE_Video, MEDIASUBTYPE_dm4v);
    pFGF->AddType(MEDIATYPE_Video, MEDIASUBTYPE_DXGM);
    pFGF->AddType(MEDIATYPE_Video, MEDIASUBTYPE_dxgm);
    pFGF->AddType(MEDIATYPE_Video, MEDIASUBTYPE_FMP4);
    pFGF->AddType(MEDIATYPE_Video, MEDIASUBTYPE_fmp4);
    pFGF->AddType(MEDIATYPE_Video, MEDIASUBTYPE_HDX4);
    pFGF->AddType(MEDIATYPE_Video, MEDIASUBTYPE_hdx4);
    pFGF->AddType(MEDIATYPE_Video, MEDIASUBTYPE_LMP4);
    pFGF->AddType(MEDIATYPE_Video, MEDIASUBTYPE_lmp4);
    pFGF->AddType(MEDIATYPE_Video, MEDIASUBTYPE_NDIG);
    pFGF->AddType(MEDIATYPE_Video, MEDIASUBTYPE_ndig);
    pFGF->AddType(MEDIATYPE_Video, MEDIASUBTYPE_RMP4);
    pFGF->AddType(MEDIATYPE_Video, MEDIASUBTYPE_rmp4);
    pFGF->AddType(MEDIATYPE_Video, MEDIASUBTYPE_SMP4);
    pFGF->AddType(MEDIATYPE_Video, MEDIASUBTYPE_smp4);
    pFGF->AddType(MEDIATYPE_Video, MEDIASUBTYPE_SEDG);
    pFGF->AddType(MEDIATYPE_Video, MEDIASUBTYPE_sedg);
    pFGF->AddType(MEDIATYPE_Video, MEDIASUBTYPE_UMP4);
    pFGF->AddType(MEDIATYPE_Video, MEDIASUBTYPE_ump4);
    pFGF->AddType(MEDIATYPE_Video, MEDIASUBTYPE_WV1F);
    pFGF->AddType(MEDIATYPE_Video, MEDIASUBTYPE_wv1f);
#endif
#if INTERNAL_DECODER_DIVX
    pFGF = IsPreview || tra[TRA_DIVX] ? pFGLAVVideo : pFGLAVVideoLM;
    pFGF->AddType(MEDIATYPE_Video, MEDIASUBTYPE_DIVX);
    pFGF->AddType(MEDIATYPE_Video, MEDIASUBTYPE_divx);
    pFGF->AddType(MEDIATYPE_Video, MEDIASUBTYPE_DX50);
    pFGF->AddType(MEDIATYPE_Video, MEDIASUBTYPE_dx50);
#endif
#if INTERNAL_DECODER_WMV
    pFGF = IsPreview || tra[TRA_WMV] ? pFGLAVVideo : pFGLAVVideoLM;
    pFGF->AddType(MEDIATYPE_Video, MEDIASUBTYPE_WMV1);
    pFGF->AddType(MEDIATYPE_Video, MEDIASUBTYPE_wmv1);
    pFGF->AddType(MEDIATYPE_Video, MEDIASUBTYPE_WMV2);
    pFGF->AddType(MEDIATYPE_Video, MEDIASUBTYPE_wmv2);
    pFGF->AddType(MEDIATYPE_Video, MEDIASUBTYPE_WMV3);
    pFGF->AddType(MEDIATYPE_Video, MEDIASUBTYPE_wmv3);
    pFGF->AddType(MEDIATYPE_Video, MEDIASUBTYPE_2VMW);
#endif
#if INTERNAL_DECODER_MSMPEG4
    pFGF = IsPreview || tra[TRA_MSMPEG4] ? pFGLAVVideo : pFGLAVVideoLM;
    pFGF->AddType(MEDIATYPE_Video, MEDIASUBTYPE_DIV3);
    pFGF->AddType(MEDIATYPE_Video, MEDIASUBTYPE_div3);
    pFGF->AddType(MEDIATYPE_Video, MEDIASUBTYPE_DVX3);
    pFGF->AddType(MEDIATYPE_Video, MEDIASUBTYPE_dvx3);
    pFGF->AddType(MEDIATYPE_Video, MEDIASUBTYPE_MP43);
    pFGF->AddType(MEDIATYPE_Video, MEDIASUBTYPE_mp43);
    pFGF->AddType(MEDIATYPE_Video, MEDIASUBTYPE_COL1);
    pFGF->AddType(MEDIATYPE_Video, MEDIASUBTYPE_col1);
    pFGF->AddType(MEDIATYPE_Video, MEDIASUBTYPE_DIV4);
    pFGF->AddType(MEDIATYPE_Video, MEDIASUBTYPE_div4);
    pFGF->AddType(MEDIATYPE_Video, MEDIASUBTYPE_DIV5);
    pFGF->AddType(MEDIATYPE_Video, MEDIASUBTYPE_div5);
    pFGF->AddType(MEDIATYPE_Video, MEDIASUBTYPE_DIV6);
    pFGF->AddType(MEDIATYPE_Video, MEDIASUBTYPE_div6);
    pFGF->AddType(MEDIATYPE_Video, MEDIASUBTYPE_AP41);
    pFGF->AddType(MEDIATYPE_Video, MEDIASUBTYPE_ap41);
    pFGF->AddType(MEDIATYPE_Video, MEDIASUBTYPE_MPG3);
    pFGF->AddType(MEDIATYPE_Video, MEDIASUBTYPE_mpg3);
    pFGF->AddType(MEDIATYPE_Video, MEDIASUBTYPE_DIV2);
    pFGF->AddType(MEDIATYPE_Video, MEDIASUBTYPE_div2);
    pFGF->AddType(MEDIATYPE_Video, MEDIASUBTYPE_MP42);
    pFGF->AddType(MEDIATYPE_Video, MEDIASUBTYPE_mp42);
    pFGF->AddType(MEDIATYPE_Video, MEDIASUBTYPE_MPG4);
    pFGF->AddType(MEDIATYPE_Video, MEDIASUBTYPE_mpg4);
    pFGF->AddType(MEDIATYPE_Video, MEDIASUBTYPE_DIV1);
    pFGF->AddType(MEDIATYPE_Video, MEDIASUBTYPE_div1);
    pFGF->AddType(MEDIATYPE_Video, MEDIASUBTYPE_MP41);
    pFGF->AddType(MEDIATYPE_Video, MEDIASUBTYPE_mp41);
#endif
#if INTERNAL_DECODER_SVQ
    pFGF = IsPreview || tra[TRA_SVQ3] ? pFGLAVVideo : pFGLAVVideoLM;
    pFGF->AddType(MEDIATYPE_Video, MEDIASUBTYPE_SVQ3);
    pFGF->AddType(MEDIATYPE_Video, MEDIASUBTYPE_SVQ1);
#endif
#if INTERNAL_DECODER_H263
    pFGF = IsPreview || tra[TRA_H263] ? pFGLAVVideo : pFGLAVVideoLM;
    pFGF->AddType(MEDIATYPE_Video, MEDIASUBTYPE_H263);
    pFGF->AddType(MEDIATYPE_Video, MEDIASUBTYPE_h263);
    pFGF->AddType(MEDIATYPE_Video, MEDIASUBTYPE_S263);
    pFGF->AddType(MEDIATYPE_Video, MEDIASUBTYPE_s263);
#endif
#if INTERNAL_DECODER_THEORA
    pFGF = IsPreview || tra[TRA_THEORA] ? pFGLAVVideo : pFGLAVVideoLM;
    pFGF->AddType(MEDIATYPE_Video, MEDIASUBTYPE_THEORA);
    pFGF->AddType(MEDIATYPE_Video, MEDIASUBTYPE_theora);
#endif
#if INTERNAL_DECODER_AMVV
    pFGF = IsPreview || tra[TRA_AMVV] ? pFGLAVVideo : pFGLAVVideoLM;
    pFGF->AddType(MEDIATYPE_Video, MEDIASUBTYPE_AMVV);
#endif
#if INTERNAL_DECODER_VP8
    pFGF = IsPreview || tra[TRA_VP8] ? pFGLAVVideo : pFGLAVVideoLM;
    pFGF->AddType(MEDIATYPE_Video, MEDIASUBTYPE_VP80);
#endif
#if INTERNAL_DECODER_VP9
    pFGF = IsPreview || tra[TRA_VP9] ? pFGLAVVideo : pFGLAVVideoLM;
    pFGF->AddType(MEDIATYPE_Video, MEDIASUBTYPE_VP90);
#endif
#if INTERNAL_DECODER_MJPEG
    pFGF = IsPreview || tra[TRA_MJPEG] ? pFGLAVVideo : pFGLAVVideoLM;
    pFGF->AddType(MEDIATYPE_Video, MEDIASUBTYPE_MJPG);
    pFGF->AddType(MEDIATYPE_Video, MEDIASUBTYPE_QTJpeg);
    pFGF->AddType(MEDIATYPE_Video, MEDIASUBTYPE_MJPA);
    pFGF->AddType(MEDIATYPE_Video, MEDIASUBTYPE_MJPB);
    pFGF->AddType(MEDIATYPE_Video, MEDIASUBTYPE_AVRn);
    pFGF->AddType(MEDIATYPE_Video, MEDIASUBTYPE_LJPG);
    pFGF->AddType(MEDIATYPE_Video, MEDIASUBTYPE_JPGL);
    pFGF->AddType(MEDIATYPE_Video, MEDIASUBTYPE_MJLS);
    pFGF->AddType(MEDIATYPE_Video, MEDIASUBTYPE_MJPA);
    pFGF->AddType(MEDIATYPE_Video, MEDIASUBTYPE_MJPB);
    pFGF->AddType(MEDIATYPE_Video, MEDIASUBTYPE_SP5X);
    pFGF->AddType(MEDIATYPE_Video, MEDIASUBTYPE_SP54);
#endif
#if INTERNAL_DECODER_INDEO
    pFGF = IsPreview || tra[TRA_INDEO] ? pFGLAVVideo : pFGLAVVideoLM;
    pFGF->AddType(MEDIATYPE_Video, MEDIASUBTYPE_IV31);
    pFGF->AddType(MEDIATYPE_Video, MEDIASUBTYPE_IV32);
    pFGF->AddType(MEDIATYPE_Video, MEDIASUBTYPE_IV41);
    pFGF->AddType(MEDIATYPE_Video, MEDIASUBTYPE_IV50);
#endif
#if INTERNAL_DECODER_SCREEN
    pFGF = IsPreview || tra[TRA_SCREEN] ? pFGLAVVideo : pFGLAVVideoLM;
    pFGF->AddType(MEDIATYPE_Video, MEDIASUBTYPE_TSCC);
    pFGF->AddType(MEDIATYPE_Video, MEDIASUBTYPE_TSC2);
    pFGF->AddType(MEDIATYPE_Video, MEDIASUBTYPE_VMNC);
#endif
#if INTERNAL_DECODER_FLIC
    pFGF = IsPreview || tra[TRA_FLIC] ? pFGLAVVideo : pFGLAVVideoLM;
    pFGF->AddType(MEDIATYPE_Video, MEDIASUBTYPE_FLIC);
#endif
#if INTERNAL_DECODER_MSVIDEO
    pFGF = IsPreview || tra[TRA_MSVIDEO] ? pFGLAVVideo : pFGLAVVideoLM;
    pFGF->AddType(MEDIATYPE_Video, MEDIASUBTYPE_CRAM);
    pFGF->AddType(MEDIATYPE_Video, MEDIASUBTYPE_WHAM);
    pFGF->AddType(MEDIATYPE_Video, MEDIASUBTYPE_MSVC);
#endif
#if INTERNAL_DECODER_V210_V410
    pFGF = IsPreview || tra[TRA_V210_V410] ? pFGLAVVideo : pFGLAVVideoLM;
    pFGF->AddType(MEDIATYPE_Video, MEDIASUBTYPE_v210);
    pFGF->AddType(MEDIATYPE_Video, MEDIASUBTYPE_v410);
#endif
#if INTERNAL_DECODER_MPEG2
    pFGF = IsPreview || tra[TRA_MPEG2] ? pFGLAVVideo : pFGLAVVideoLM;
    pFGF->AddType(MEDIATYPE_DVD_ENCRYPTED_PACK, MEDIASUBTYPE_MPEG2_VIDEO);
    pFGF->AddType(MEDIATYPE_MPEG2_PACK, MEDIASUBTYPE_MPEG2_VIDEO);
    pFGF->AddType(MEDIATYPE_MPEG2_PES, MEDIASUBTYPE_MPEG2_VIDEO);
    pFGF->AddType(MEDIATYPE_Video, MEDIASUBTYPE_MPEG2_VIDEO);
    pFGF->AddType(MEDIATYPE_Video, MEDIASUBTYPE_MPG2);
#endif
#if INTERNAL_DECODER_PRORES
    pFGF = IsPreview || tra[TRA_PRORES] ? pFGLAVVideo : pFGLAVVideoLM;
    pFGF->AddType(MEDIATYPE_Video, MEDIASUBTYPE_apch);
    pFGF->AddType(MEDIATYPE_Video, MEDIASUBTYPE_apcn);
    pFGF->AddType(MEDIATYPE_Video, MEDIASUBTYPE_apcs);
    pFGF->AddType(MEDIATYPE_Video, MEDIASUBTYPE_apco);
    pFGF->AddType(MEDIATYPE_Video, MEDIASUBTYPE_ap4h);
    pFGF->AddType(MEDIATYPE_Video, MEDIASUBTYPE_ap4x);
#endif
#if INTERNAL_DECODER_DNXHD
    pFGF = IsPreview || tra[TRA_DNXHD] ? pFGLAVVideo : pFGLAVVideoLM;
    pFGF->AddType(MEDIATYPE_Video, MEDIASUBTYPE_AVdn);
    pFGF->AddType(MEDIATYPE_Video, MEDIASUBTYPE_AVdh);
#endif
#if INTERNAL_DECODER_CFHD
    pFGF = IsPreview || tra[TRA_CFHD] ? pFGLAVVideo : pFGLAVVideoLM;
    pFGF->AddType(MEDIATYPE_Video, MEDIASUBTYPE_CFHD);
#endif
#if INTERNAL_DECODER_OTHERVIDEO
    pFGF = IsPreview || tra[TRA_OTHERVIDEO] ? pFGLAVVideo : pFGLAVVideoLM;
    pFGF->AddType(MEDIATYPE_Video, MEDIASUBTYPE_APV1);
    pFGF->AddType(MEDIATYPE_Video, MEDIASUBTYPE_CUVC);
    pFGF->AddType(MEDIATYPE_Video, MEDIASUBTYPE_NCLC);
    pFGF->AddType(MEDIATYPE_Video, MEDIASUBTYPE_FSV1);
    pFGF->AddType(MEDIATYPE_Video, MEDIASUBTYPE_FSV2);
    pFGF->AddType(MEDIATYPE_Video, MEDIASUBTYPE_LAV_RAWVIDEO);
#endif

    // Add LAV Video if needed
    if (!pFGLAVVideo->GetTypes().IsEmpty()) {
        m_transform.AddTail(pFGLAVVideo.Detach());
    }

    // Add low merit LAV video
    pFGLAVVideoLM->AddType(MEDIATYPE_Video, MEDIASUBTYPE_NULL);
    m_transform.AddTail(pFGLAVVideoLM.Detach());
}

void CFGManagerCustom::InsertLAVAudio()
{
    const CAppSettings& s = AfxGetAppSettings();
    const bool* tra = s.TraFilters;
    CFGFilter* pFGF;

    CAutoPtr<CFGFilterLAV> pFGLAVAudio(CFGFilterLAV::CreateFilter(CFGFilterLAV::AUDIO_DECODER, MERIT64_ABOVE_DSHOW));
    CAutoPtr<CFGFilterLAV> pFGLAVAudioLM(CFGFilterLAV::CreateFilter(CFGFilterLAV::AUDIO_DECODER, MERIT64_LOWEST+5, true));

#if INTERNAL_DECODER_MPEGAUDIO
    pFGF = tra[TRA_MPA] ? pFGLAVAudio : pFGLAVAudioLM;
    pFGF->AddType(MEDIATYPE_Audio, MEDIASUBTYPE_MP3);
    pFGF->AddType(MEDIATYPE_Audio, MEDIASUBTYPE_MPEG1AudioPayload);
    pFGF->AddType(MEDIATYPE_Audio, MEDIASUBTYPE_MPEG1Payload);
    pFGF->AddType(MEDIATYPE_Audio, MEDIASUBTYPE_MPEG1Packet);

    pFGF->AddType(MEDIATYPE_DVD_ENCRYPTED_PACK, MEDIASUBTYPE_MPEG2_AUDIO);
    pFGF->AddType(MEDIATYPE_MPEG2_PACK, MEDIASUBTYPE_MPEG2_AUDIO);
    pFGF->AddType(MEDIATYPE_MPEG2_PES, MEDIASUBTYPE_MPEG2_AUDIO);
    pFGF->AddType(MEDIATYPE_Audio, MEDIASUBTYPE_MPEG2_AUDIO);
#endif

#if INTERNAL_DECODER_AMR
    pFGF = tra[TRA_AMR] ? pFGLAVAudio : pFGLAVAudioLM;
    pFGF->AddType(MEDIATYPE_Audio, MEDIASUBTYPE_SAMR);
    pFGF->AddType(MEDIATYPE_Audio, MEDIASUBTYPE_AMR);
    pFGF->AddType(MEDIATYPE_Audio, MEDIASUBTYPE_SAWB);
#endif

#if INTERNAL_DECODER_LPCM
    pFGF = tra[TRA_LPCM] ? pFGLAVAudio : pFGLAVAudioLM;
    pFGF->AddType(MEDIATYPE_DVD_ENCRYPTED_PACK, MEDIASUBTYPE_DVD_LPCM_AUDIO);
    pFGF->AddType(MEDIATYPE_MPEG2_PACK, MEDIASUBTYPE_DVD_LPCM_AUDIO);
    pFGF->AddType(MEDIATYPE_MPEG2_PES, MEDIASUBTYPE_DVD_LPCM_AUDIO);
    pFGF->AddType(MEDIATYPE_Audio, MEDIASUBTYPE_DVD_LPCM_AUDIO);
    pFGF->AddType(MEDIATYPE_Audio, MEDIASUBTYPE_HDMV_LPCM_AUDIO);
#endif

#if INTERNAL_DECODER_AC3
    pFGF = tra[TRA_AC3] ? pFGLAVAudio : pFGLAVAudioLM;
    pFGF->AddType(MEDIATYPE_DVD_ENCRYPTED_PACK, MEDIASUBTYPE_DOLBY_AC3);
    pFGF->AddType(MEDIATYPE_MPEG2_PACK, MEDIASUBTYPE_DOLBY_AC3);
    pFGF->AddType(MEDIATYPE_MPEG2_PES, MEDIASUBTYPE_DOLBY_AC3);
    pFGF->AddType(MEDIATYPE_Audio, MEDIASUBTYPE_DOLBY_AC3);
    pFGF->AddType(MEDIATYPE_Audio, MEDIASUBTYPE_WAVE_DOLBY_AC3);
    pFGF->AddType(MEDIATYPE_Audio, MEDIASUBTYPE_DOLBY_TRUEHD);
    pFGF->AddType(MEDIATYPE_Audio, MEDIASUBTYPE_DOLBY_DDPLUS);
    pFGF->AddType(MEDIATYPE_Audio, MEDIASUBTYPE_MLP);
#endif

#if INTERNAL_DECODER_DTS
    pFGF = tra[TRA_DTS] ? pFGLAVAudio : pFGLAVAudioLM;
    pFGF->AddType(MEDIATYPE_DVD_ENCRYPTED_PACK, MEDIASUBTYPE_DTS);
    pFGF->AddType(MEDIATYPE_MPEG2_PACK, MEDIASUBTYPE_DTS);
    pFGF->AddType(MEDIATYPE_MPEG2_PES, MEDIASUBTYPE_DTS);
    pFGF->AddType(MEDIATYPE_Audio, MEDIASUBTYPE_DTS);
    pFGF->AddType(MEDIATYPE_Audio, MEDIASUBTYPE_DTS_HD);
    pFGF->AddType(MEDIATYPE_Audio, MEDIASUBTYPE_WAVE_DTS);
#endif

#if INTERNAL_DECODER_AAC
    pFGF = tra[TRA_AAC] ? pFGLAVAudio : pFGLAVAudioLM;
    pFGF->AddType(MEDIATYPE_MPEG2_PACK, MEDIASUBTYPE_AAC);
    pFGF->AddType(MEDIATYPE_MPEG2_PES, MEDIASUBTYPE_AAC);
    pFGF->AddType(MEDIATYPE_Audio, MEDIASUBTYPE_AAC);
    pFGF->AddType(MEDIATYPE_Audio, MEDIASUBTYPE_LATM_AAC);
    pFGF->AddType(MEDIATYPE_Audio, MEDIASUBTYPE_AAC_ADTS);
    pFGF->AddType(MEDIATYPE_MPEG2_PACK, MEDIASUBTYPE_MP4A);
    pFGF->AddType(MEDIATYPE_MPEG2_PES, MEDIASUBTYPE_MP4A);
    pFGF->AddType(MEDIATYPE_Audio, MEDIASUBTYPE_MP4A);
    pFGF->AddType(MEDIATYPE_MPEG2_PACK, MEDIASUBTYPE_mp4a);
    pFGF->AddType(MEDIATYPE_MPEG2_PES, MEDIASUBTYPE_mp4a);
    pFGF->AddType(MEDIATYPE_Audio, MEDIASUBTYPE_mp4a);
#endif

#if INTERNAL_DECODER_PS2AUDIO
    pFGF = tra[TRA_PS2AUD] ? pFGLAVAudio : pFGLAVAudioLM;
    pFGF->AddType(MEDIATYPE_MPEG2_PACK, MEDIASUBTYPE_PS2_PCM);
    pFGF->AddType(MEDIATYPE_MPEG2_PES, MEDIASUBTYPE_PS2_PCM);
    pFGF->AddType(MEDIATYPE_Audio, MEDIASUBTYPE_PS2_PCM);
    pFGF->AddType(MEDIATYPE_MPEG2_PACK, MEDIASUBTYPE_PS2_ADPCM);
    pFGF->AddType(MEDIATYPE_MPEG2_PES, MEDIASUBTYPE_PS2_ADPCM);
    pFGF->AddType(MEDIATYPE_Audio, MEDIASUBTYPE_PS2_ADPCM);
#endif

#if INTERNAL_DECODER_VORBIS
    pFGF = tra[TRA_VORBIS] ? pFGLAVAudio : pFGLAVAudioLM;
    pFGF->AddType(MEDIATYPE_Audio, MEDIASUBTYPE_Vorbis2);
#endif

#if INTERNAL_DECODER_FLAC
    pFGF = tra[TRA_FLAC] ? pFGLAVAudio : pFGLAVAudioLM;
    pFGF->AddType(MEDIATYPE_Audio, MEDIASUBTYPE_FLAC_FRAMED);
#endif

#if INTERNAL_DECODER_NELLYMOSER
    pFGF = tra[TRA_NELLY] ? pFGLAVAudio : pFGLAVAudioLM;
    pFGF->AddType(MEDIATYPE_Audio, MEDIASUBTYPE_NELLYMOSER);
#endif

#if INTERNAL_DECODER_ALAC
    pFGF = tra[TRA_ALAC] ? pFGLAVAudio : pFGLAVAudioLM;
    pFGF->AddType(MEDIATYPE_Audio, MEDIASUBTYPE_ALAC);
#endif

#if INTERNAL_DECODER_ALS
    pFGF = tra[TRA_ALS] ? pFGLAVAudio : pFGLAVAudioLM;
    pFGF->AddType(MEDIATYPE_Audio, MEDIASUBTYPE_ALS);
#endif

#if INTERNAL_DECODER_OPUS
    pFGF = tra[TRA_OPUS] ? pFGLAVAudio : pFGLAVAudioLM;
    pFGF->AddType(MEDIATYPE_Audio, MEDIASUBTYPE_OPUS);
#endif

#if INTERNAL_DECODER_WMA
    pFGF = tra[TRA_WMA] ? pFGLAVAudio : pFGLAVAudioLM;
    pFGF->AddType(MEDIATYPE_Audio, MEDIASUBTYPE_MSAUDIO1);
    pFGF->AddType(MEDIATYPE_Audio, MEDIASUBTYPE_WMAUDIO2);
#endif

#if INTERNAL_DECODER_WMAPRO
    pFGF = tra[TRA_WMAPRO] ? pFGLAVAudio : pFGLAVAudioLM;
    pFGF->AddType(MEDIATYPE_Audio, MEDIASUBTYPE_WMAUDIO3);
#endif

#if INTERNAL_DECODER_WMALL
    pFGF = tra[TRA_WMALL] ? pFGLAVAudio : pFGLAVAudioLM;
    pFGF->AddType(MEDIATYPE_Audio, MEDIASUBTYPE_WMAUDIO_LOSSLESS);
#endif

#if INTERNAL_DECODER_PCM
    pFGF = tra[TRA_PCM] ? pFGLAVAudio : pFGLAVAudioLM;
    pFGF->AddType(MEDIATYPE_Audio, MEDIASUBTYPE_PCM_NONE);
    pFGF->AddType(MEDIATYPE_Audio, MEDIASUBTYPE_PCM_RAW);
    pFGF->AddType(MEDIATYPE_Audio, MEDIASUBTYPE_PCM_TWOS);
    pFGF->AddType(MEDIATYPE_Audio, MEDIASUBTYPE_PCM_SOWT);
    pFGF->AddType(MEDIATYPE_Audio, MEDIASUBTYPE_PCM_IN24);
    pFGF->AddType(MEDIATYPE_Audio, MEDIASUBTYPE_PCM_IN32);
    pFGF->AddType(MEDIATYPE_Audio, MEDIASUBTYPE_PCM_FL32);
    pFGF->AddType(MEDIATYPE_Audio, MEDIASUBTYPE_PCM_FL64);
    pFGF->AddType(MEDIATYPE_Audio, MEDIASUBTYPE_IEEE_FLOAT); // only for 64-bit float PCM
    /* todo: this should not depend on PCM */
    #if INTERNAL_DECODER_ADPCM
    pFGF->AddType(MEDIATYPE_Audio, MEDIASUBTYPE_IMA4);
    pFGF->AddType(MEDIATYPE_Audio, MEDIASUBTYPE_ADPCM_SWF);
    pFGF->AddType(MEDIATYPE_Audio, MEDIASUBTYPE_ADPCM_AMV);
    pFGF->AddType(MEDIATYPE_Audio, MEDIASUBTYPE_IMA_WAV);
    #endif
#endif

#if INTERNAL_DECODER_G726
    pFGF = tra[TRA_G726] ? pFGLAVAudio : pFGLAVAudioLM;
    pFGF->AddType(MEDIATYPE_Audio, MEDIASUBTYPE_G726);
#endif

#if INTERNAL_DECODER_G729
    pFGF = tra[TRA_G729] ? pFGLAVAudio : pFGLAVAudioLM;
    pFGF->AddType(MEDIATYPE_Audio, MEDIASUBTYPE_G729);
    pFGF->AddType(MEDIATYPE_Audio, MEDIASUBTYPE_729A);
#endif

#if INTERNAL_DECODER_AC4
    pFGF = tra[TRA_AC4] ? pFGLAVAudio : pFGLAVAudioLM;
    pFGF->AddType(MEDIATYPE_Audio, MEDIASUBTYPE_DOLBY_AC4);
#endif

#if INTERNAL_DECODER_OTHERAUDIO
    pFGF = tra[TRA_OTHERAUDIO] ? pFGLAVAudio : pFGLAVAudioLM;
    pFGF->AddType(MEDIATYPE_Audio, MEDIASUBTYPE_IMC);
#endif

    // Add LAV Audio if needed
    if (!pFGLAVAudio->GetTypes().IsEmpty()) {
        m_transform.AddTail(pFGLAVAudio.Detach());
    }

    // Add low merit LAV Audio
    pFGLAVAudioLM->AddType(MEDIATYPE_Audio, MEDIASUBTYPE_NULL);
    m_transform.AddTail(pFGLAVAudioLM.Detach());
}

void CFGManagerCustom::InsertBlockedFilters()
{
    // "Subtitle Mixer" makes an access violation around the
    // 11-12th media type when enumerating them on its output.
    m_transform.AddTail(DEBUG_NEW CFGFilterRegistry(GUIDFromCString(_T("{00A95963-3BE5-48C0-AD9F-3356D67EA09D}")), MERIT64_DO_NOT_USE));

    // DiracSplitter.ax is crashing MPC-HC when opening invalid files...
    m_transform.AddTail(DEBUG_NEW CFGFilterRegistry(GUIDFromCString(_T("{09E7F58E-71A1-419D-B0A0-E524AE1454A9}")), MERIT64_DO_NOT_USE));
    m_transform.AddTail(DEBUG_NEW CFGFilterRegistry(GUIDFromCString(_T("{5899CFB9-948F-4869-A999-5544ECB38BA5}")), MERIT64_DO_NOT_USE));
    m_transform.AddTail(DEBUG_NEW CFGFilterRegistry(GUIDFromCString(_T("{F78CF248-180E-4713-B107-B13F7B5C31E1}")), MERIT64_DO_NOT_USE));

    // ISCR suxx
    m_transform.AddTail(DEBUG_NEW CFGFilterRegistry(GUIDFromCString(_T("{48025243-2D39-11CE-875D-00608CB78066}")), MERIT64_DO_NOT_USE));

    // Samsung's "mpeg-4 demultiplexor" can even open matroska files, amazing...
    m_transform.AddTail(DEBUG_NEW CFGFilterRegistry(GUIDFromCString(_T("{99EC0C72-4D1B-411B-AB1F-D561EE049D94}")), MERIT64_DO_NOT_USE));

    // LG Video Renderer (lgvid.ax) just crashes when trying to connect it
    m_transform.AddTail(DEBUG_NEW CFGFilterRegistry(GUIDFromCString(_T("{9F711C60-0668-11D0-94D4-0000C02BA972}")), MERIT64_DO_NOT_USE));

    // palm demuxer crashes (even crashes graphedit when dropping an .ac3 onto it)
    m_transform.AddTail(DEBUG_NEW CFGFilterRegistry(GUIDFromCString(_T("{BE2CF8A7-08CE-4A2C-9A25-FD726A999196}")), MERIT64_DO_NOT_USE));

    // mainconcept color space converter
    m_transform.AddTail(DEBUG_NEW CFGFilterRegistry(GUIDFromCString(_T("{272D77A0-A852-4851-ADA4-9091FEAD4C86}")), MERIT64_DO_NOT_USE));

    // mainconcept mp4 demuxer
    m_transform.AddTail(DEBUG_NEW CFGFilterRegistry(GUIDFromCString(_T("{2A55FF12-1657-41D7-9D2D-A2CDC6978FF2}")), MERIT64_DO_NOT_USE));

    // Accusoft PICVideo M-JPEG Codec
    m_transform.AddTail(DEBUG_NEW CFGFilterRegistry(GUIDFromCString(_T("{4C4CD9E1-F876-11D2-962F-00500471FDDC}")), MERIT64_DO_NOT_USE));

    #ifndef _WIN64
	// PICVideo Lossless JPEG Decompressor (pvljpg20.dll)
	m_transform.AddTail(DEBUG_NEW CFGFilterRegistry(GUIDFromCString(_T("{BA310CC1-470D-11D3-962F-00500471FDDC}")), MERIT64_DO_NOT_USE));
    #endif

    // SolveigMM MP4 Demultiplexer (smm_mp4demuxer.ax)
    m_transform.AddTail(DEBUG_NEW CFGFilterRegistry(GUIDFromCString(_T("{5F19B8FE-BA79-4183-B3CF-FEE4E8F801E4}")), MERIT64_DO_NOT_USE));

    // Morgan's Stream Switcher (mmswitch.ax)
    m_transform.AddTail(DEBUG_NEW CFGFilterRegistry(CLSID_MorganStreamSwitcher, MERIT64_DO_NOT_USE));

    if (AfxGetAppSettings().bBlockRDP) {
        m_transform.AddTail(DEBUG_NEW CFGFilterRegistry(CLSID_RDPDShowRedirectionFilter, MERIT64_DO_NOT_USE));
    }

    // DCDSPFilter (early versions crash mpc)
    {
        CRegKey key;

        TCHAR buff[256];
        ULONG len = sizeof(buff);
        ZeroMemory(buff, sizeof(buff));

        CString clsid = _T("{B38C58A0-1809-11D6-A458-EDAE78F1DF12}");

        if (ERROR_SUCCESS == key.Open(HKEY_CLASSES_ROOT, _T("CLSID\\") + clsid + _T("\\InprocServer32"), KEY_READ)
            && ERROR_SUCCESS == key.QueryStringValue(nullptr, buff, &len)
            && FileVersionInfo::GetFileVersionNum(buff) < 0x0001000000030000ui64) {
            m_transform.AddTail(DEBUG_NEW CFGFilterRegistry(GUIDFromCString(clsid), MERIT64_DO_NOT_USE));
        }
    }
}

void CFGManagerCustom::InsertSubtitleFilters(bool IsPreview)
{
    const CAppSettings& s = AfxGetAppSettings();
    CFGFilter* pFGF;

    // Null text renderer
    pFGF = DEBUG_NEW CFGFilterInternal<CNullTextRenderer>(L"NullTextRenderer", IsPreview ? MERIT64_ABOVE_DSHOW : MERIT64_DO_USE);
    pFGF->AddType(MEDIATYPE_Text, MEDIASUBTYPE_NULL);
    pFGF->AddType(MEDIATYPE_ScriptCommand, MEDIASUBTYPE_NULL);
    pFGF->AddType(MEDIATYPE_Subtitle, MEDIASUBTYPE_NULL);
    pFGF->AddType(MEDIATYPE_Text, MEDIASUBTYPE_NULL);
    pFGF->AddType(MEDIATYPE_Video, MEDIASUBTYPE_DVD_SUBPICTURE);
    pFGF->AddType(MEDIATYPE_NULL, MEDIASUBTYPE_CVD_SUBPICTURE);
    pFGF->AddType(MEDIATYPE_NULL, MEDIASUBTYPE_SVCD_SUBPICTURE);
    m_transform.AddTail(pFGF);

    if (IsPreview) {
        m_transform.AddTail(DEBUG_NEW CFGFilterRegistry(CLSID_VSFilter, MERIT64_DO_NOT_USE));
        m_transform.AddTail(DEBUG_NEW CFGFilterRegistry(CLSID_VSFilter2, MERIT64_DO_NOT_USE));
        m_transform.AddTail(DEBUG_NEW CFGFilterRegistry(CLSID_XySubFilter, MERIT64_DO_NOT_USE));
        m_transform.AddTail(DEBUG_NEW CFGFilterRegistry(CLSID_XySubFilter_AutoLoader, MERIT64_DO_NOT_USE));
        m_transform.AddTail(DEBUG_NEW CFGFilterRegistry(CLSID_AssFilter, MERIT64_DO_NOT_USE));
        m_transform.AddTail(DEBUG_NEW CFGFilterRegistry(CLSID_AssFilter_AutoLoader, MERIT64_DO_NOT_USE));
    } else {
        // Insert preferred subtitle renderer and block others
        switch (s.GetSubtitleRenderer()) {
        case CAppSettings::SubtitleRenderer::INTERNAL:
            if (s.fBlockVSFilter) {
                m_transform.AddTail(DEBUG_NEW CFGFilterRegistry(CLSID_VSFilter, MERIT64_DO_NOT_USE));
                m_transform.AddTail(DEBUG_NEW CFGFilterRegistry(CLSID_VSFilter2, MERIT64_DO_NOT_USE));
            }
            if (s.fBlockVSFilter || IsCLSIDRegistered(CLSID_VSFilter)) {
                m_transform.AddTail(DEBUG_NEW CFGFilterRegistry(CLSID_XySubFilter, MERIT64_DO_NOT_USE));
                m_transform.AddTail(DEBUG_NEW CFGFilterRegistry(CLSID_XySubFilter_AutoLoader, MERIT64_DO_NOT_USE));
            }
            m_transform.AddTail(DEBUG_NEW CFGFilterRegistry(CLSID_AssFilter, MERIT64_DO_NOT_USE));
            m_transform.AddTail(DEBUG_NEW CFGFilterRegistry(CLSID_AssFilter_AutoLoader, MERIT64_DO_NOT_USE));
            break;
        case CAppSettings::SubtitleRenderer::VS_FILTER:
            pFGF = DEBUG_NEW CFGFilterRegistry(CLSID_VSFilter, MERIT64_ABOVE_DSHOW);
            if (pFGF) {
                pFGF->AddType(MEDIASUBTYPE_NULL, MEDIASUBTYPE_NULL);
                m_override.AddTail(pFGF);
            }
            m_transform.AddTail(DEBUG_NEW CFGFilterRegistry(CLSID_XySubFilter, MERIT64_DO_NOT_USE));
            m_transform.AddTail(DEBUG_NEW CFGFilterRegistry(CLSID_XySubFilter_AutoLoader, MERIT64_DO_NOT_USE));
            m_transform.AddTail(DEBUG_NEW CFGFilterRegistry(CLSID_AssFilter, MERIT64_DO_NOT_USE));
            m_transform.AddTail(DEBUG_NEW CFGFilterRegistry(CLSID_AssFilter_AutoLoader, MERIT64_DO_NOT_USE));
            break;
        case CAppSettings::SubtitleRenderer::XY_SUB_FILTER:
            pFGF = DEBUG_NEW CFGFilterRegistry(CLSID_XySubFilter_AutoLoader, MERIT64_ABOVE_DSHOW);
            if (pFGF) {
                pFGF->AddType(MEDIASUBTYPE_NULL, MEDIASUBTYPE_NULL);
                m_override.AddTail(pFGF);
            }
            pFGF = DEBUG_NEW CFGFilterRegistry(CLSID_XySubFilter, MERIT64_ABOVE_DSHOW);
            if (pFGF) {
                pFGF->AddType(MEDIATYPE_Text, MEDIASUBTYPE_NULL);
                pFGF->AddType(MEDIATYPE_Subtitle, MEDIASUBTYPE_NULL);
                m_override.AddTail(pFGF);
            }
            if (s.fBlockVSFilter) {
                m_transform.AddTail(DEBUG_NEW CFGFilterRegistry(CLSID_VSFilter, MERIT64_DO_NOT_USE));
                m_transform.AddTail(DEBUG_NEW CFGFilterRegistry(CLSID_VSFilter2, MERIT64_DO_NOT_USE));
            }
            m_transform.AddTail(DEBUG_NEW CFGFilterRegistry(CLSID_AssFilter, MERIT64_DO_NOT_USE));
            m_transform.AddTail(DEBUG_NEW CFGFilterRegistry(CLSID_AssFilter_AutoLoader, MERIT64_DO_NOT_USE));
            break;
        case CAppSettings::SubtitleRenderer::NONE:
            m_transform.AddTail(DEBUG_NEW CFGFilterRegistry(CLSID_VSFilter, MERIT64_DO_NOT_USE));
            m_transform.AddTail(DEBUG_NEW CFGFilterRegistry(CLSID_VSFilter2, MERIT64_DO_NOT_USE));
            m_transform.AddTail(DEBUG_NEW CFGFilterRegistry(CLSID_XySubFilter, MERIT64_DO_NOT_USE));
            m_transform.AddTail(DEBUG_NEW CFGFilterRegistry(CLSID_XySubFilter_AutoLoader, MERIT64_DO_NOT_USE));
            m_transform.AddTail(DEBUG_NEW CFGFilterRegistry(CLSID_AssFilter, MERIT64_DO_NOT_USE));
            m_transform.AddTail(DEBUG_NEW CFGFilterRegistry(CLSID_AssFilter_AutoLoader, MERIT64_DO_NOT_USE));
            break;
        }
    }
}

void CFGManagerCustom::InsertBroadcomDecoder()
{
    CFGFilter* pFGF = DEBUG_NEW CFGFilterRegistry(GUIDFromCString(_T("{2DE1D17E-46B1-42A8-9AEC-E20E80D9B1A9}")), MERIT64_ABOVE_DSHOW);
    pFGF->AddType(MEDIATYPE_Video, MEDIASUBTYPE_H264);
    pFGF->AddType(MEDIATYPE_Video, MEDIASUBTYPE_h264);
    pFGF->AddType(MEDIATYPE_Video, MEDIASUBTYPE_X264);
    pFGF->AddType(MEDIATYPE_Video, MEDIASUBTYPE_x264);
    pFGF->AddType(MEDIATYPE_Video, MEDIASUBTYPE_VSSH);
    pFGF->AddType(MEDIATYPE_Video, MEDIASUBTYPE_vssh);
    pFGF->AddType(MEDIATYPE_Video, MEDIASUBTYPE_DAVC);
    pFGF->AddType(MEDIATYPE_Video, MEDIASUBTYPE_davc);
    pFGF->AddType(MEDIATYPE_Video, MEDIASUBTYPE_PAVC);
    pFGF->AddType(MEDIATYPE_Video, MEDIASUBTYPE_pavc);
    pFGF->AddType(MEDIATYPE_Video, MEDIASUBTYPE_AVC1);
    pFGF->AddType(MEDIATYPE_Video, MEDIASUBTYPE_avc1);
    pFGF->AddType(MEDIATYPE_Video, MEDIASUBTYPE_H264_bis);
    pFGF->AddType(MEDIATYPE_Video, MEDIASUBTYPE_CCV1);

    pFGF->AddType(MEDIATYPE_Video, MEDIASUBTYPE_WVC1);
    pFGF->AddType(MEDIATYPE_Video, MEDIASUBTYPE_wvc1);

    pFGF->AddType(MEDIATYPE_DVD_ENCRYPTED_PACK, MEDIASUBTYPE_MPEG2_VIDEO);
    pFGF->AddType(MEDIATYPE_MPEG2_PACK, MEDIASUBTYPE_MPEG2_VIDEO);
    pFGF->AddType(MEDIATYPE_MPEG2_PES, MEDIASUBTYPE_MPEG2_VIDEO);
    pFGF->AddType(MEDIATYPE_Video, MEDIASUBTYPE_MPEG2_VIDEO);
    m_transform.AddHead(pFGF);
}

//
//  CFGManagerCustom
//

CFGManagerCustom::CFGManagerCustom(LPCWSTR pClassName, LPCWSTR pInputFileURL, HWND hWnd, bool IsPreview)
    : CFGManager(pClassName, pInputFileURL, hWnd, IsPreview)
{
    const CAppSettings& s = AfxGetAppSettings();

    bool bOverrideBroadcom = false;
    CFGFilter* pFGF;

    const bool* src = s.SrcFilters;
    const bool* tra = s.TraFilters;

    // Reset LAVFilters internal instances
    CFGFilterLAV::ResetInternalInstances();

    // Add internal filters
    InsertLAVSplitterSource(IsPreview);
    InsertLAVSplitter(IsPreview);
    InsertOtherInternalSourcefilters(IsPreview);
#if HAS_VIDEO_DECODERS
    InsertLAVVideo(IsPreview);
#endif
#if HAS_AUDIO_DECODERS
    if (!IsPreview) {
        InsertLAVAudio();
    }
#endif
    InsertSubtitleFilters(IsPreview);

    // Blocked filters
    InsertBlockedFilters();
    if (m_bIsPreview) {
        m_transform.AddHead(DEBUG_NEW CFGFilterRegistry(CLSID_RDPDShowRedirectionFilter, MERIT64_DO_NOT_USE));
    }

    // Overrides
    WORD merit_low = 1;
    POSITION pos = s.m_filters.GetTailPosition();
    while (pos) {
        FilterOverride* fo = s.m_filters.GetPrev(pos);

        if (!fo->fDisabled && fo->name == _T("Broadcom Video Decoder")) {
            bOverrideBroadcom = true;
        }
        if (fo->fDisabled || fo->type == FilterOverride::EXTERNAL && !PathUtils::Exists(MakeFullPath(fo->path))) {
            continue;
        }

        ULONGLONG merit =
            fo->iLoadType == FilterOverride::BLOCK ? MERIT64_DO_NOT_USE :
            fo->iLoadType == FilterOverride::PREFERRED ? (IsPreview ? MERIT64_DO_USE : MERIT64_ABOVE_DSHOW) :
            MERIT64(fo->dwMerit);

        merit += merit_low++;

        pFGF = nullptr;
        if (fo->type == FilterOverride::REGISTERED) {
            pFGF = DEBUG_NEW CFGFilterRegistry(fo->dispname, merit);
        } else if (fo->type == FilterOverride::EXTERNAL) {
            pFGF = DEBUG_NEW CFGFilterFile(fo->clsid, fo->path, CStringW(fo->name), merit);
        }
        if (pFGF) {
            pFGF->SetTypes(fo->guids);
            m_override.AddTail(pFGF);
        }
    }

    /* Use Broadcom decoder (if installed) for VC-1, H.264 and MPEG-2 */
    if (!IsPreview && !bOverrideBroadcom) {
        // ToDo: maybe remove support for this old filter?
        InsertBroadcomDecoder();
    }
}

STDMETHODIMP CFGManagerCustom::AddFilter(IBaseFilter* pBF, LPCWSTR pName)
{
    CAutoLock cAutoLock(this);

    HRESULT hr;

    CLSID clsid = GetCLSID(pBF);

    if (clsid == CLSID_AVIDec || clsid == CLSID_ACMWrapper) {
        AfxGetMyApp()->HookModuleLoading();
    }

    if (FAILED(hr = __super::AddFilter(pBF, pName))) {
        return hr;
    }

    if (clsid == CLSID_DMOWrapperFilter) {
        if (CComQIPtr<IPropertyBag> pPB = pBF) {
            CComVariant var(true);
            pPB->Write(_T("_HIRESOUTPUT"), &var);
        }
    }

    if (clsid == __uuidof(CAudioSwitcherFilter)) {
        if (CComQIPtr<IAudioSwitcherFilter> pASF = pBF) {
            CAppSettings& s = AfxGetAppSettings();
            pASF->SetSpeakerConfig(s.fCustomChannelMapping, s.pSpeakerToChannelMap);
            pASF->SetAudioTimeShift(s.fAudioTimeShift ? 10000i64 * s.iAudioTimeShift : 0);
            pASF->SetNormalizeBoost2(s.fAudioNormalize, s.nAudioMaxNormFactor, s.fAudioNormalizeRecover, s.nAudioBoost);
        }
    }

    return hr;
}

//
//  CFGManagerPlayer
//

CFGManagerPlayer::CFGManagerPlayer(LPCWSTR pClassName, LPCWSTR pInputFileURL, HWND hWnd, bool IsPreview)
    : CFGManagerCustom(pClassName, pInputFileURL, hWnd, IsPreview)
    , m_hWnd(hWnd)
{
    TRACE(_T("CFGManagerPlayer::CFGManagerPlayer on thread: %lu\n"), GetCurrentThreadId());
    CFGFilter* pFGF;

    const CAppSettings& s = AfxGetAppSettings();

    /* value is chosen so that it is higher than standard renderers, but lower than important intermediate filters like VSFilter */
    UINT64 renderer_merit = MERIT64(0x800001) + 0x100;

    // Switchers

    if (s.fEnableAudioSwitcher && !m_bIsPreview) {
        pFGF = DEBUG_NEW CFGFilterInternal<CAudioSwitcherFilter>(L"Audio Switcher", renderer_merit + 0x100);
        pFGF->AddType(MEDIATYPE_Audio, MEDIASUBTYPE_NULL);
        m_transform.AddTail(pFGF);
    }

    // Renderers
    if (!m_bIsPreview) {
        switch (s.iDSVideoRendererType) {
            case VIDRNDT_DS_DEFAULT:
                m_transform.AddTail(DEBUG_NEW CFGFilterRegistry(CLSID_VideoRendererDefault,  MERIT64(0x800001)));
                m_transform.AddTail(DEBUG_NEW CFGFilterRegistry(CLSID_VideoMixingRenderer9,  MERIT64(0x200003)));
                m_transform.AddTail(DEBUG_NEW CFGFilterRegistry(CLSID_EnhancedVideoRenderer, MERIT64(0x200002)));
                break;
            case VIDRNDT_DS_OVERLAYMIXER:
                m_transform.AddTail(DEBUG_NEW CFGFilterVideoRenderer(m_hWnd, CLSID_OverlayMixer, StrRes(IDS_PPAGE_OUTPUT_OVERLAYMIXER), renderer_merit));
                break;
            case VIDRNDT_DS_VMR9WINDOWED:
                m_transform.AddTail(DEBUG_NEW CFGFilterVideoRenderer(m_hWnd, CLSID_VideoMixingRenderer9, StrRes(IDS_PPAGE_OUTPUT_VMR9WINDOWED), renderer_merit));
                break;
            case VIDRNDT_DS_VMR9RENDERLESS:
                m_transform.AddTail(DEBUG_NEW CFGFilterVideoRenderer(m_hWnd, CLSID_VMR9AllocatorPresenter, StrRes(IDS_PPAGE_OUTPUT_VMR9RENDERLESS), renderer_merit));
                break;
            case VIDRNDT_DS_EVR:
                m_transform.AddTail(DEBUG_NEW CFGFilterVideoRenderer(m_hWnd, CLSID_EnhancedVideoRenderer, StrRes(IDS_PPAGE_OUTPUT_EVR), renderer_merit));
                break;
            case VIDRNDT_DS_EVR_CUSTOM:
                m_transform.AddTail(DEBUG_NEW CFGFilterVideoRenderer(m_hWnd, CLSID_EVRAllocatorPresenter, StrRes(IDS_PPAGE_OUTPUT_EVR_CUSTOM), renderer_merit));
                break;
            case VIDRNDT_DS_DXR:
                m_transform.AddTail(DEBUG_NEW CFGFilterVideoRenderer(m_hWnd, CLSID_DXRAllocatorPresenter, StrRes(IDS_PPAGE_OUTPUT_DXR), renderer_merit));
                break;
            case VIDRNDT_DS_MADVR:
                m_transform.AddTail(DEBUG_NEW CFGFilterVideoRenderer(m_hWnd, CLSID_madVRAllocatorPresenter, StrRes(IDS_PPAGE_OUTPUT_MADVR), renderer_merit));
                break;
            case VIDRNDT_DS_SYNC:
                m_transform.AddTail(DEBUG_NEW CFGFilterVideoRenderer(m_hWnd, CLSID_SyncAllocatorPresenter, StrRes(IDS_PPAGE_OUTPUT_SYNC), renderer_merit));
                break;
            case VIDRNDT_DS_MPCVR:
                if (!m_bIsCapture) {
                    m_transform.AddTail(DEBUG_NEW CFGFilterVideoRenderer(m_hWnd, CLSID_MPCVRAllocatorPresenter, StrRes(IDS_PPAGE_OUTPUT_MPCVR), renderer_merit));
                }
                break;
            case VIDRNDT_DS_NULL_COMP:
                pFGF = DEBUG_NEW CFGFilterInternal<CNullVideoRenderer>(StrRes(IDS_PPAGE_OUTPUT_NULL_COMP), MERIT64_ABOVE_DSHOW + 2);
                pFGF->AddType(MEDIATYPE_Video, MEDIASUBTYPE_NULL);
                m_transform.AddTail(pFGF);
                break;
            case VIDRNDT_DS_NULL_UNCOMP:
                pFGF = DEBUG_NEW CFGFilterInternal<CNullUVideoRenderer>(StrRes(IDS_PPAGE_OUTPUT_NULL_UNCOMP), MERIT64_ABOVE_DSHOW + 2);
                pFGF->AddType(MEDIATYPE_Video, MEDIASUBTYPE_NULL);
                m_transform.AddTail(pFGF);
                break;
        }
        // add fallback
        if (s.iDSVideoRendererType == VIDRNDT_DS_MPCVR || s.iDSVideoRendererType == VIDRNDT_DS_MADVR) {
            if (IsCLSIDRegistered(CLSID_EnhancedVideoRenderer)) {
                m_transform.AddTail(DEBUG_NEW CFGFilterVideoRenderer(m_hWnd, CLSID_EnhancedVideoRenderer, StrRes(IDS_PPAGE_OUTPUT_EVR), renderer_merit - 1));
            } else {
                m_transform.AddTail(DEBUG_NEW CFGFilterVideoRenderer(m_hWnd, CLSID_VideoMixingRenderer9, StrRes(IDS_PPAGE_OUTPUT_VMR9WINDOWED), renderer_merit - 1));
            }
        }
    } else {
        bool preview_evrcp = (s.iDSVideoRendererType == VIDRNDT_DS_EVR_CUSTOM) || (s.iDSVideoRendererType == VIDRNDT_DS_SYNC) || (s.iDSVideoRendererType == VIDRNDT_DS_MADVR) || (s.iDSVideoRendererType == VIDRNDT_DS_MPCVR);
        if (preview_evrcp && CAppSettings::IsVideoRendererAvailable(VIDRNDT_DS_EVR_CUSTOM)) {
            m_transform.AddTail(DEBUG_NEW CFGFilterVideoRenderer(m_hWnd, CLSID_EVRAllocatorPresenter, L"EVRCP - Preview Window", MERIT64_ABOVE_DSHOW + 2, true));
            m_bPreviewSupportsRotation = true;
        } else if (CAppSettings::IsVideoRendererAvailable(VIDRNDT_DS_EVR)) {
            m_transform.AddTail(DEBUG_NEW CFGFilterVideoRenderer(m_hWnd, CLSID_EnhancedVideoRenderer, L"EVR - Preview Window", MERIT64_ABOVE_DSHOW + 2, true));
        } else {
            m_transform.AddTail(DEBUG_NEW CFGFilterVideoRenderer(m_hWnd, CLSID_VideoMixingRenderer9, L"VMR9 - Preview Window", MERIT64_ABOVE_DSHOW + 2, true));
        }
    }

    if (!m_bIsPreview) {
        bool fallback_ds = false;
        bool fallback_mpcar = false;
        bool fallback_null = false;
        CString SelAudioRenderer = s.SelectedAudioRenderer();
        if (SelAudioRenderer == AUDRNDT_NULL_COMP) {
            pFGF = DEBUG_NEW CFGFilterInternal<CNullAudioRenderer>(AUDRNDT_NULL_COMP, MERIT64_ABOVE_DSHOW + 2);
            pFGF->AddType(MEDIATYPE_Audio, MEDIASUBTYPE_NULL);
            // DVD stuff
            pFGF->AddType(MEDIATYPE_DVD_ENCRYPTED_PACK, MEDIASUBTYPE_DOLBY_AC3);
            pFGF->AddType(MEDIATYPE_MPEG2_PACK, MEDIASUBTYPE_DOLBY_AC3);
            pFGF->AddType(MEDIATYPE_MPEG2_PES, MEDIASUBTYPE_DOLBY_AC3);
            pFGF->AddType(MEDIATYPE_DVD_ENCRYPTED_PACK, MEDIASUBTYPE_MPEG2_AUDIO);
            pFGF->AddType(MEDIATYPE_MPEG2_PACK, MEDIASUBTYPE_MPEG2_AUDIO);
            pFGF->AddType(MEDIATYPE_MPEG2_PES, MEDIASUBTYPE_MPEG2_AUDIO);
            pFGF->AddType(MEDIATYPE_DVD_ENCRYPTED_PACK, MEDIASUBTYPE_DVD_LPCM_AUDIO);
            pFGF->AddType(MEDIATYPE_MPEG2_PACK, MEDIASUBTYPE_DVD_LPCM_AUDIO);
            pFGF->AddType(MEDIATYPE_MPEG2_PES, MEDIASUBTYPE_DVD_LPCM_AUDIO);
            pFGF->AddType(MEDIATYPE_DVD_ENCRYPTED_PACK, MEDIASUBTYPE_DTS);
            pFGF->AddType(MEDIATYPE_MPEG2_PACK, MEDIASUBTYPE_DTS);
            pFGF->AddType(MEDIATYPE_MPEG2_PES, MEDIASUBTYPE_DTS);
            m_transform.AddTail(pFGF);
        } else if (SelAudioRenderer == AUDRNDT_NULL_UNCOMP) {
            pFGF = DEBUG_NEW CFGFilterInternal<CNullUAudioRenderer>(AUDRNDT_NULL_UNCOMP, MERIT64_ABOVE_DSHOW + 2);
            pFGF->AddType(MEDIATYPE_Audio, MEDIASUBTYPE_NULL);
            m_transform.AddTail(pFGF);
        } else if (SelAudioRenderer == AUDRNDT_INTERNAL) {
            struct SaneAudioRendererFilter : CFGFilter {
                SaneAudioRendererFilter(CStringW name, UINT64 merit) :
                    CFGFilter(SaneAudioRenderer::Factory::GetFilterGuid(), name, merit) {}

                HRESULT Create(IBaseFilter** ppBF, CInterfaceList<IUnknown, &IID_IUnknown>&) override {
                    return SaneAudioRenderer::Factory::CreateFilter(AfxGetAppSettings().sanear, ppBF);
                }
            };
            pFGF = DEBUG_NEW SaneAudioRendererFilter(AUDRNDT_SANEAR, renderer_merit + 0x50);
            pFGF->AddType(MEDIATYPE_Audio, MEDIASUBTYPE_NULL);
            m_transform.AddTail(pFGF);
            fallback_ds = true;
            fallback_null = true;
        } else if (SelAudioRenderer == AUDRNDT_MPC) {
            pFGF = DEBUG_NEW CFGFilterInternal<CMpcAudioRenderer>(AUDRNDT_MPC, renderer_merit);
            pFGF->AddType(MEDIATYPE_Audio, MEDIASUBTYPE_PCM);
            pFGF->AddType(MEDIATYPE_Audio, MEDIASUBTYPE_IEEE_FLOAT);
            m_transform.AddTail(pFGF);
            fallback_ds = true;
            fallback_null = true;
        } else if (!SelAudioRenderer.IsEmpty()) {
            pFGF = DEBUG_NEW CFGFilterRegistry(SelAudioRenderer, renderer_merit);
            pFGF->AddType(MEDIATYPE_Audio, MEDIASUBTYPE_NULL);
            m_transform.AddTail(pFGF);
            fallback_mpcar = true;
            fallback_null = true;
        } else {
            fallback_mpcar = true;
            fallback_null = true;
        }

        // fallbacks
        if (fallback_mpcar) {
            pFGF = DEBUG_NEW CFGFilterInternal<CMpcAudioRenderer>(AUDRNDT_MPC, MERIT64_UNLIKELY + 1);
            pFGF->AddType(MEDIATYPE_Audio, MEDIASUBTYPE_PCM);
            pFGF->AddType(MEDIATYPE_Audio, MEDIASUBTYPE_IEEE_FLOAT);
            m_transform.AddTail(pFGF);
        }
        if (fallback_ds) {
            pFGF = DEBUG_NEW CFGFilterRegistry(L"@device:cm:{E0F158E1-CB04-11D0-BD4E-00A0C911CE86}\\Default DirectSound Device", MERIT64_LOWEST + 1);
            m_transform.AddTail(pFGF);
        }
        if (fallback_null) {
            pFGF = DEBUG_NEW CFGFilterInternal<CNullUAudioRenderer>(AUDRNDT_NULL_UNCOMP, MERIT64_LOWEST);
            pFGF->AddType(MEDIATYPE_Audio, MEDIASUBTYPE_NULL);
            m_transform.AddTail(pFGF);
        }
    } else {
        pFGF = DEBUG_NEW CFGFilterInternal<CNullAudioRenderer>(AUDRNDT_NULL_COMP, MERIT64_ABOVE_DSHOW + 2);
        pFGF->AddType(MEDIATYPE_Audio, MEDIASUBTYPE_NULL);
        // DVD stuff
        pFGF->AddType(MEDIATYPE_DVD_ENCRYPTED_PACK, MEDIASUBTYPE_DOLBY_AC3);
        pFGF->AddType(MEDIATYPE_MPEG2_PACK, MEDIASUBTYPE_DOLBY_AC3);
        pFGF->AddType(MEDIATYPE_MPEG2_PES, MEDIASUBTYPE_DOLBY_AC3);
        pFGF->AddType(MEDIATYPE_DVD_ENCRYPTED_PACK, MEDIASUBTYPE_MPEG2_AUDIO);
        pFGF->AddType(MEDIATYPE_MPEG2_PACK, MEDIASUBTYPE_MPEG2_AUDIO);
        pFGF->AddType(MEDIATYPE_MPEG2_PES, MEDIASUBTYPE_MPEG2_AUDIO);
        pFGF->AddType(MEDIATYPE_DVD_ENCRYPTED_PACK, MEDIASUBTYPE_DVD_LPCM_AUDIO);
        pFGF->AddType(MEDIATYPE_MPEG2_PACK, MEDIASUBTYPE_DVD_LPCM_AUDIO);
        pFGF->AddType(MEDIATYPE_MPEG2_PES, MEDIASUBTYPE_DVD_LPCM_AUDIO);
        pFGF->AddType(MEDIATYPE_DVD_ENCRYPTED_PACK, MEDIASUBTYPE_DTS);
        pFGF->AddType(MEDIATYPE_MPEG2_PACK, MEDIASUBTYPE_DTS);
        pFGF->AddType(MEDIATYPE_MPEG2_PES, MEDIASUBTYPE_DTS);
        m_transform.AddTail(pFGF);
    }
}

STDMETHODIMP CFGManagerPlayer::ConnectDirect(IPin* pPinOut, IPin* pPinIn, const AM_MEDIA_TYPE* pmt)
{
    CAutoLock cAutoLock(this);

    CLSID pin_clsid = GetCLSID(pPinOut);
    if (pin_clsid == CLSID_MPEG2Demultiplexer) {
        CComQIPtr<IMediaSeeking> pMS = pPinOut;
        REFERENCE_TIME rtDur = 0;
        if (!pMS || FAILED(pMS->GetDuration(&rtDur)) || rtDur <= 0) {
            return E_FAIL;
        }
    } else if (pin_clsid == CLSID_StillVideo || pin_clsid == CLSID_MPCImageSource) {
        CComQIPtr<IMediaSeeking> pMS = pPinOut;
        if (pMS) {
            const CAppSettings& s = AfxGetAppSettings();
            if (s.iStillVideoDuration > 0) {
                REFERENCE_TIME rtCur = 0;
                REFERENCE_TIME rtDur = 0;
                REFERENCE_TIME rtDurOverride = s.iStillVideoDuration * 10000000LL;
                pMS->GetDuration(&rtDur);
                if (rtDur == 0 || rtDur >= 10 * 3600 * 10000000LL) {
                    rtDur = rtDurOverride;
                } else if (rtDur < rtDurOverride) {
                    rtDur = (rtDurOverride / rtDur) * rtDur;
                }
                // always call SetPositions() to prevent infinite repeat by the source filter
                pMS->SetPositions(&rtCur, AM_SEEKING_AbsolutePositioning, &rtDur, AM_SEEKING_AbsolutePositioning);
            }
        }
    }

    return __super::ConnectDirect(pPinOut, pPinIn, pmt);
}

//
// CFGManagerDVD
//

CFGManagerDVD::CFGManagerDVD(LPCTSTR pInputFileURL, HWND hWnd, bool IsPreview)
    : CFGManagerPlayer(L"CFGManagerDVD", pInputFileURL, hWnd, IsPreview)
{
    const CAppSettings& s = AfxGetAppSettings();

    // elecard's decoder isn't suited for dvd playback (atm)
    m_transform.AddTail(DEBUG_NEW CFGFilterRegistry(GUIDFromCString(_T("{F50B3F13-19C4-11CF-AA9A-02608C9BABA2}")), MERIT64_DO_NOT_USE));
}

class CResetDVD : public CDVDSession
{
public:
    CResetDVD(LPCTSTR path) {
        if (Open(path)) {
            if (BeginSession()) {
                Authenticate(); /*GetDiscKey();*/
                EndSession();
            }
            Close();
        }
    }
};

STDMETHODIMP CFGManagerDVD::RenderFile(LPCWSTR lpcwstrFile, LPCWSTR lpcwstrPlayList)
{
    CAutoLock cAutoLock(this);

    HRESULT hr;

    CComPtr<IBaseFilter> pBF;
    if (FAILED(hr = AddSourceFilter(lpcwstrFile, lpcwstrFile, &pBF))) {
        return hr;
    }

    return ConnectFilter(pBF, nullptr);
}

STDMETHODIMP CFGManagerDVD::AddSourceFilter(LPCWSTR lpcwstrFileName, LPCWSTR lpcwstrFilterName, IBaseFilter** ppFilter)
{
    CAutoLock cAutoLock(this);

    CheckPointer(lpcwstrFileName, E_POINTER);
    CheckPointer(ppFilter, E_POINTER);

    CStringW fn = CStringW(lpcwstrFileName).TrimLeft();

    GUID clsid = CLSID_DVDNavigator;

    CComPtr<IBaseFilter> pBF;
    if (FAILED(pBF.CoCreateInstance(clsid))
            || FAILED(AddFilter(pBF, L"DVD Navigator"))) {
        return VFW_E_CANNOT_LOAD_SOURCE_FILTER;
    }

    CComQIPtr<IDvdControl2> pDVDC;
    CComQIPtr<IDvdInfo2> pDVDI;

    if (!((pDVDC = pBF) && (pDVDI = pBF))) {
        return E_NOINTERFACE;
    }

    WCHAR buff[MAX_PATH];
    ULONG len;
    if ((!fn.IsEmpty()
            && FAILED(pDVDC->SetDVDDirectory(fn))
            && FAILED(pDVDC->SetDVDDirectory(fn + L"VIDEO_TS"))
            && FAILED(pDVDC->SetDVDDirectory(fn + L"\\VIDEO_TS")))
            || FAILED(pDVDI->GetDVDDirectory(buff, _countof(buff), &len)) || len == 0) {
        return E_INVALIDARG;
    }

    pDVDC->SetOption(DVD_ResetOnStop, FALSE);
    pDVDC->SetOption(DVD_HMSF_TimeCodeEvents, TRUE);

    if (clsid == CLSID_DVDNavigator) {
        CResetDVD(CString(buff));
    }

    *ppFilter = pBF.Detach();

    return S_OK;
}

//
// CFGManagerCapture
//

CFGManagerCapture::CFGManagerCapture(HWND hWnd)
    : CFGManagerPlayer(_T("CFGManagerCapture"), L"", hWnd)
{
    const CAppSettings& s = AfxGetAppSettings();

    if (s.bCaptureDeinterlace) {
        // set merit higher than our video renderers
        CFGFilter* pFGF = DEBUG_NEW CFGFilterInternal<CDeinterlacerFilter>(L"Deinterlacer", MERIT64(0x800001) + 0x200);
        pFGF->AddType(MEDIATYPE_Video, MEDIASUBTYPE_NULL);
        m_transform.AddTail(pFGF);
    }

    m_bIsCapture = True;
}

//
// CFGAggregator
//

CFGAggregator::CFGAggregator(const CLSID& clsid, LPCTSTR pName, LPUNKNOWN pUnk, HRESULT& hr)
    : CUnknown(pName, pUnk)
{
    hr = m_pUnkInner.CoCreateInstance(clsid, GetOwner());
}

CFGAggregator::~CFGAggregator()
{
    m_pUnkInner.Release();
}

STDMETHODIMP CFGAggregator::NonDelegatingQueryInterface(REFIID riid, void** ppv)
{
    CheckPointer(ppv, E_POINTER);

    return
        m_pUnkInner && (riid != IID_IUnknown && SUCCEEDED(m_pUnkInner->QueryInterface(riid, ppv))) ? S_OK :
        __super::NonDelegatingQueryInterface(riid, ppv);
}
