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
#include "ShellDropTarget.h"
#include "mplayerc.h"
#include "MainFrm.h"
#include "PathUtils.h"
#include <shellapi.h>
#include <shlwapi.h>
#include <algorithm>
#include <vector>

#ifdef _WIN64
const CLSID CLSID_MPCHCDropTargetPlay    = { 0x97A8DFA8, 0x4C28, 0x4F94, { 0xB4, 0xB0, 0x5E, 0x2E, 0xE0, 0x26, 0x53, 0xFF } };
const CLSID CLSID_MPCHCDropTargetEnqueue = { 0x7FDF201D, 0xC5E2, 0x4A23, { 0x9A, 0x0C, 0xD7, 0x4E, 0x89, 0x52, 0x86, 0xCB } };
#else
const CLSID CLSID_MPCHCDropTargetPlay    = { 0x350FCB02, 0x242B, 0x4716, { 0x8E, 0x0F, 0xD7, 0xB7, 0xDC, 0xF2, 0x8E, 0xA8 } };
const CLSID CLSID_MPCHCDropTargetEnqueue = { 0xA1A14499, 0x02C8, 0x4A17, { 0x88, 0x95, 0x92, 0x72, 0x51, 0xC8, 0x21, 0xB2 } };
#endif

static bool GetDroppedFiles(IDataObject* pDataObj, CAtlList<CString>& files)
{
    if (!pDataObj) {
        return false;
    }

    FORMATETC fmt = { CF_HDROP, nullptr, DVASPECT_CONTENT, -1, TYMED_HGLOBAL };
    STGMEDIUM stg = {};
    if (FAILED(pDataObj->GetData(&fmt, &stg))) {
        return false;
    }

    if (HDROP hDrop = static_cast<HDROP>(GlobalLock(stg.hGlobal))) {
        UINT nFiles = ::DragQueryFile(hDrop, UINT_MAX, nullptr, 0);
        for (UINT iFile = 0; iFile < nFiles; iFile++) {
            CString fn;
            UINT res = ::DragQueryFile(hDrop, iFile, fn.GetBuffer(2048), 2048);
            if (res) {
                fn.ReleaseBuffer(res);
                files.AddTail(fn);
            }
        }
        GlobalUnlock(stg.hGlobal);
    }
    ReleaseStgMedium(&stg);

    return !files.IsEmpty();
}

// Explorer enumerates a selection with the focused item first and the rest in no order
// worth keeping, so the batch is put in path order here, once, before the player sees it.
// Same comparison as the playlist's own sort by path.
static void SortByPath(CAtlList<CString>& files)
{
    std::vector<CString> v;
    v.reserve(files.GetCount());
    POSITION pos = files.GetHeadPosition();
    while (pos) {
        v.emplace_back(files.GetNext(pos));
    }
    std::sort(v.begin(), v.end(), [](const CString& a, const CString& b) {
        return StrCmpLogicalW(a, b) < 0;
    });
    files.RemoveAll();
    for (const CString& fn : v) {
        files.AddTail(fn);
    }
}

// With multiple instances allowed, a selection opened while this instance is busy gets a
// window of its own, which is what the command line path gave it before the drop target
// existed. It is one process for the whole selection, not one per file.
static bool LaunchNewInstance(const CAtlList<CString>& files)
{
    CString args;
    POSITION pos = files.GetHeadPosition();
    while (pos) {
        if (!args.IsEmpty()) {
            args += _T(' ');
        }
        args += _T('"') + files.GetNext(pos) + _T('"');
    }
    if (args.GetLength() > 30000) { // longer than a command line can carry
        return false;
    }

    SHELLEXECUTEINFO sei = { sizeof(sei) };
    sei.fMask = SEE_MASK_NOASYNC;
    sei.lpFile = PathUtils::GetProgramPath(true);
    sei.lpParameters = args;
    sei.nShow = SW_SHOWNORMAL;
    return !!ShellExecuteEx(&sei);
}

// CShellDropTarget

CShellDropTarget::CShellDropTarget(CMainFrame* pMainFrame, bool bAppend)
    : m_cRef(1)
    , m_pMainFrame(pMainFrame)
    , m_bAppend(bAppend)
{
}

STDMETHODIMP CShellDropTarget::QueryInterface(REFIID riid, void** ppv)
{
    if (!ppv) {
        return E_POINTER;
    }
    if (riid == IID_IUnknown || riid == IID_IDropTarget) {
        *ppv = static_cast<IDropTarget*>(this);
        AddRef();
        return S_OK;
    }
    *ppv = nullptr;
    return E_NOINTERFACE;
}

STDMETHODIMP_(ULONG) CShellDropTarget::AddRef()
{
    return InterlockedIncrement(&m_cRef);
}

STDMETHODIMP_(ULONG) CShellDropTarget::Release()
{
    ULONG cRef = InterlockedDecrement(&m_cRef);
    if (cRef == 0) {
        delete this;
    }
    return cRef;
}

STDMETHODIMP CShellDropTarget::DragEnter(IDataObject* pDataObj, DWORD, POINTL, DWORD* pdwEffect)
{
    if (!pdwEffect) {
        return E_POINTER;
    }
    FORMATETC fmt = { CF_HDROP, nullptr, DVASPECT_CONTENT, -1, TYMED_HGLOBAL };
    *pdwEffect = (pDataObj && pDataObj->QueryGetData(&fmt) == S_OK) ? DROPEFFECT_COPY : DROPEFFECT_NONE;
    return S_OK;
}

STDMETHODIMP CShellDropTarget::DragOver(DWORD, POINTL, DWORD* pdwEffect)
{
    if (!pdwEffect) {
        return E_POINTER;
    }
    *pdwEffect = DROPEFFECT_COPY;
    return S_OK;
}

STDMETHODIMP CShellDropTarget::DragLeave()
{
    return S_OK;
}

// Runs on the main thread, called from Explorer through COM. Explorer waits for it to
// return, so nothing here may touch the filesystem or the graph: the command line is
// queued the same way a redirected one is and acted on after this call has returned.
STDMETHODIMP CShellDropTarget::Drop(IDataObject* pDataObj, DWORD, POINTL, DWORD* pdwEffect)
{
    if (!pdwEffect) {
        return E_POINTER;
    }
    *pdwEffect = DROPEFFECT_NONE;

    CAtlList<CString> files;
    if (!GetDroppedFiles(pDataObj, files)) {
        return E_INVALIDARG;
    }
    SortByPath(files);

    if (!m_pMainFrame || !::IsWindow(m_pMainFrame->m_hWnd)) {
        return E_UNEXPECTED;
    }

    const CAppSettings& s = AfxGetAppSettings();
    if (!m_bAppend && s.GetAllowMultiInst() && m_pMainFrame->GetLoadState() != MLS::CLOSED
            && LaunchNewInstance(files)) {
        *pdwEffect = DROPEFFECT_COPY;
        return S_OK;
    }

    if (!m_bAppend) {
        // The shell hands its foreground rights to a drop target for the duration of this
        // call, so this is the moment to use them. Not when adding to the playlist: same rule
        // as the command line redirect, which leaves a minimized player alone for /add.
        if (m_pMainFrame->IsIconic()) {
            m_pMainFrame->ShowWindow(SW_RESTORE);
        }
        m_pMainFrame->SetForegroundWindow();
    }

    CAtlList<CString> cmdln;
    if (m_bAppend) {
        cmdln.AddTail(_T("/add"));
    }
    cmdln.AddTailList(&files);
    m_pMainFrame->QueueCommandLine(cmdln);

    *pdwEffect = DROPEFFECT_COPY;
    return S_OK;
}

// CShellDropTargetFactory

CShellDropTargetFactory::CShellDropTargetFactory(CMainFrame* pMainFrame, bool bAppend)
    : m_cRef(1)
    , m_pMainFrame(pMainFrame)
    , m_bAppend(bAppend)
{
}

STDMETHODIMP CShellDropTargetFactory::QueryInterface(REFIID riid, void** ppv)
{
    if (!ppv) {
        return E_POINTER;
    }
    if (riid == IID_IUnknown || riid == IID_IClassFactory) {
        *ppv = static_cast<IClassFactory*>(this);
        AddRef();
        return S_OK;
    }
    *ppv = nullptr;
    return E_NOINTERFACE;
}

STDMETHODIMP_(ULONG) CShellDropTargetFactory::AddRef()
{
    return InterlockedIncrement(&m_cRef);
}

STDMETHODIMP_(ULONG) CShellDropTargetFactory::Release()
{
    ULONG cRef = InterlockedDecrement(&m_cRef);
    if (cRef == 0) {
        delete this;
    }
    return cRef;
}

STDMETHODIMP CShellDropTargetFactory::CreateInstance(IUnknown* pUnkOuter, REFIID riid, void** ppv)
{
    if (!ppv) {
        return E_POINTER;
    }
    *ppv = nullptr;
    if (pUnkOuter) {
        return CLASS_E_NOAGGREGATION;
    }
    CShellDropTarget* pTarget = DEBUG_NEW CShellDropTarget(m_pMainFrame, m_bAppend);
    HRESULT hr = pTarget->QueryInterface(riid, ppv);
    pTarget->Release();
    return hr;
}

STDMETHODIMP CShellDropTargetFactory::LockServer(BOOL)
{
    // The player's lifetime is its window, not COM's reference count.
    return S_OK;
}

// CShellDropTargetServer

CShellDropTargetServer::CShellDropTargetServer()
    : m_dwPlayCookie(0)
    , m_dwEnqueueCookie(0)
{
}

CShellDropTargetServer::~CShellDropTargetServer()
{
    Revoke();
}

void CShellDropTargetServer::Register(CMainFrame* pMainFrame)
{
    ASSERT(!m_dwPlayCookie && !m_dwEnqueueCookie);

    struct {
        const CLSID& clsid;
        bool bAppend;
        DWORD& dwCookie;
    } entries[] = {
        { CLSID_MPCHCDropTargetPlay, false, m_dwPlayCookie },
        { CLSID_MPCHCDropTargetEnqueue, true, m_dwEnqueueCookie },
    };

    for (auto& entry : entries) {
        CComPtr<IClassFactory> pFactory;
        pFactory.Attach(DEBUG_NEW CShellDropTargetFactory(pMainFrame, entry.bAppend));
        HRESULT hr = CoRegisterClassObject(entry.clsid, pFactory, CLSCTX_LOCAL_SERVER, REGCLS_MULTIPLEUSE, &entry.dwCookie);
        if (FAILED(hr)) {
            TRACE(_T("CoRegisterClassObject failed: 0x%08x\n"), hr);
            entry.dwCookie = 0;
        }
    }
}

void CShellDropTargetServer::Revoke()
{
    for (DWORD* pCookie : { &m_dwPlayCookie, &m_dwEnqueueCookie }) {
        if (*pCookie) {
            CoRevokeClassObject(*pCookie);
            *pCookie = 0;
        }
    }
}
