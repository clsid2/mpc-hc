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

#include <oleidl.h>

class CMainFrame;

// A file type verb that carries a DropTarget CLSID makes Explorer hand the whole selection
// to one IDropTarget::Drop call, instead of running the verb's command line once per file
// and leaving the player to guess which of the resulting processes belong together. This
// executable is the server for those CLSIDs (LocalServer32), so COM connects Explorer to
// the player that is already running, or starts one with -Embedding when none is.
//
// One CLSID per verb, because a drop target is not told which verb invoked it. The values
// differ between the 32-bit and 64-bit builds, like the ProgIDs do, so both can be
// installed side by side.
extern const CLSID CLSID_MPCHCDropTargetPlay;
extern const CLSID CLSID_MPCHCDropTargetEnqueue;

class CShellDropTarget : public IDropTarget
{
public:
    CShellDropTarget(CMainFrame* pMainFrame, bool bAppend);

    // IUnknown
    STDMETHODIMP QueryInterface(REFIID riid, void** ppv) override;
    STDMETHODIMP_(ULONG) AddRef() override;
    STDMETHODIMP_(ULONG) Release() override;

    // IDropTarget
    STDMETHODIMP DragEnter(IDataObject* pDataObj, DWORD grfKeyState, POINTL pt, DWORD* pdwEffect) override;
    STDMETHODIMP DragOver(DWORD grfKeyState, POINTL pt, DWORD* pdwEffect) override;
    STDMETHODIMP DragLeave() override;
    STDMETHODIMP Drop(IDataObject* pDataObj, DWORD grfKeyState, POINTL pt, DWORD* pdwEffect) override;

private:
    ~CShellDropTarget() = default;

    LONG m_cRef;
    CMainFrame* const m_pMainFrame;
    const bool m_bAppend;
};

class CShellDropTargetFactory : public IClassFactory
{
public:
    CShellDropTargetFactory(CMainFrame* pMainFrame, bool bAppend);

    // IUnknown
    STDMETHODIMP QueryInterface(REFIID riid, void** ppv) override;
    STDMETHODIMP_(ULONG) AddRef() override;
    STDMETHODIMP_(ULONG) Release() override;

    // IClassFactory
    STDMETHODIMP CreateInstance(IUnknown* pUnkOuter, REFIID riid, void** ppv) override;
    STDMETHODIMP LockServer(BOOL fLock) override;

private:
    ~CShellDropTargetFactory() = default;

    LONG m_cRef;
    CMainFrame* const m_pMainFrame;
    const bool m_bAppend;
};

// Registers the class objects with COM for the life of the process. Every instance
// registers, whether or not COM started it: REGCLS_MULTIPLEUSE lets one registration
// serve any number of Explorer invocations, and COM routes them to whichever registered
// instance is still alive.
class CShellDropTargetServer
{
public:
    CShellDropTargetServer();
    ~CShellDropTargetServer();

    void Register(CMainFrame* pMainFrame);
    void Revoke();

private:
    DWORD m_dwPlayCookie;
    DWORD m_dwEnqueueCookie;
};
