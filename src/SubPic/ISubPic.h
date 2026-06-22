/*
 * (C) 2003-2006 Gabest
 * (C) 2006-2016 see Authors.txt
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

#include <atlbase.h>
#include <atlcoll.h>
#include "CoordGeom.h"
#include "strmif.h"

// to force rendering subs at a specific size
#define DEBUG_OVERRIDE_TEXTURE_SIZE 0
#define DEBUG_OVERRIDE_TEXTURE_SIZE_WIDTH 2560
#define DEBUG_OVERRIDE_TEXTURE_SIZE_HEIGHT 1440

#pragma pack(push, 1)
struct SubPicDesc {
    int type;
    int w, h, bpp, pitch, pitchUV;
    BYTE* bits;
    BYTE* bitsU;
    BYTE* bitsV;
    RECT vidrect; // video rectangle

    SubPicDesc()
        : type(0)
        , w(0)
        , h(0)
        , bpp(0)
        , pitch(0)
        , pitchUV(0)
        , bits(nullptr)
        , bitsU(nullptr)
        , bitsV(nullptr) {
        ZeroMemory(&vidrect, sizeof(vidrect));
    }
};
#pragma pack(pop)

enum RelativeTo {
    WINDOW,
    VIDEO,
    BEST_FIT
};

//
// ISubPic
//

interface __declspec(uuid("DA3A5B51-958C-4C28-BF66-68D7947577A2"))
ISubPic :
public IUnknown {
    static const REFERENCE_TIME INVALID_SUBPIC_TIME = -1;

    STDMETHOD_(void*, GetObject)() PURE;

    STDMETHOD_(REFERENCE_TIME, GetStart)() const PURE;
    STDMETHOD_(REFERENCE_TIME, GetStop)() const PURE;
    STDMETHOD_(void, SetStart)(REFERENCE_TIME rtStart) PURE;
    STDMETHOD_(void, SetStop)(REFERENCE_TIME rtStop) PURE;

    STDMETHOD(GetDesc)(SubPicDesc& spd /*[out]*/) PURE;
    STDMETHOD(CopyTo)(ISubPic* pSubPic /*[in]*/) PURE;

    STDMETHOD(ClearDirtyRect)() PURE;
    STDMETHOD(GetDirtyRect)(RECT* pDirtyRect /*[out]*/) const PURE;
    STDMETHOD(SetDirtyRect)(const RECT* pDirtyRect /*[in]*/) PURE;

    STDMETHOD(GetMaxSize)(SIZE* pMaxSize /*[out]*/) const PURE;
    STDMETHOD(SetSize)(SIZE pSize /*[in]*/, RECT vidrect /*[in]*/) PURE;

    STDMETHOD(Lock)(SubPicDesc& spd /*[out]*/) PURE;
    STDMETHOD(Unlock)(RECT* pDirtyRect /*[in]*/) PURE;

    STDMETHOD(AlphaBlt)(RECT * pSrc, RECT * pDst, SubPicDesc* pTarget = nullptr /*[in]*/) PURE;
    STDMETHOD(GetSourceAndDest)(RECT rcWindow /*[in]*/, RECT rcVideo /*[in]*/,
                                RECT* pRcSource /*[out]*/,  RECT* pRcDest /*[out]*/,
                                const double videoStretchFactor = 1.0 /*[in]*/,
                                int xOffsetInPixels = 0 /*[in]*/, int yOffsetInPixels = 0 /*[in]*/) const PURE;
    STDMETHOD(SetVirtualTextureSize)(const SIZE pSize, const POINT pTopLeft) PURE;
    STDMETHOD(GetRelativeTo)(RelativeTo* pRelativeTo /*[out]*/) const PURE;
    STDMETHOD(SetRelativeTo)(RelativeTo relativeTo /*[in]*/) PURE;

    STDMETHOD_(REFERENCE_TIME, GetSegmentStart)() const PURE;
    STDMETHOD_(REFERENCE_TIME, GetSegmentStop)() const PURE;
    STDMETHOD_(void, SetSegmentStart)(REFERENCE_TIME rtStart) PURE;
    STDMETHOD_(void, SetSegmentStop)(REFERENCE_TIME rtStop) PURE;

    STDMETHOD_(void, SetInverseAlpha)(bool bInverted) PURE;
};

//
// ISubPicAllocator
//

interface __declspec(uuid("CF7C3C23-6392-4a42-9E72-0736CFF793CB"))
ISubPicAllocator :
public IUnknown {
    STDMETHOD(SetCurSize)(SIZE size /*[in]*/) PURE;
    STDMETHOD(SetCurVidRect)(RECT curvidrect) PURE;

    STDMETHOD(GetStatic)(ISubPic** ppSubPic /*[out]*/) PURE;
    STDMETHOD(AllocDynamic)(ISubPic** ppSubPic /*[out]*/) PURE;

    STDMETHOD_(bool, IsDynamicWriteOnly)() const PURE;

    STDMETHOD(ChangeDevice)(IUnknown * pDev) PURE;
    STDMETHOD(SetMaxTextureSize)(SIZE maxTextureSize) PURE;

    STDMETHOD(FreeStatic)() PURE;
    
    STDMETHOD_(void, SetInverseAlpha)(bool bInverted) PURE;
};

//
// ISubPicProvider
//

interface __declspec(uuid("D62B9A1A-879A-42db-AB04-88AA8F243CFD"))
ISubPicProvider :
public IUnknown {
    static const REFERENCE_TIME UNKNOWN_TIME = _I64_MAX;

    STDMETHOD(Lock)() PURE;
    STDMETHOD(Unlock)() PURE;

    STDMETHOD_(POSITION, GetStartPosition)(REFERENCE_TIME rt, double fps) PURE;
    STDMETHOD_(POSITION, GetNext)(POSITION pos) PURE;

    STDMETHOD_(REFERENCE_TIME, GetStart)(POSITION pos, double fps) PURE;
    STDMETHOD_(REFERENCE_TIME, GetStop)(POSITION pos, double fps) PURE;

    STDMETHOD_(bool, IsAnimated)(POSITION pos) PURE;

    STDMETHOD(Render)(SubPicDesc & spd, REFERENCE_TIME rt, double fps, RECT & bbox) PURE;
    STDMETHOD(GetTextureSize)(POSITION pos, SIZE & MaxTextureSize, SIZE & VirtualSize, POINT & VirtualTopLeft) PURE;
    STDMETHOD(GetRelativeTo)(POSITION pos, RelativeTo & relativeTo) PURE;
};

//
// ISubPicQueue
//

interface __declspec(uuid("C8334466-CD1E-4ad1-9D2D-8EE8519BD180"))
ISubPicQueue :
public IUnknown {
    STDMETHOD(SetSubPicProvider)(ISubPicProvider* pSubPicProvider /*[in]*/) PURE;
    STDMETHOD(GetSubPicProvider)(ISubPicProvider** pSubPicProvider /*[out]*/) PURE;

    STDMETHOD(SetFPS)(double fps /*[in]*/) PURE;
    STDMETHOD(SetTime)(REFERENCE_TIME rtNow /*[in]*/) PURE;

    STDMETHOD(Invalidate)(REFERENCE_TIME rtInvalidate = -1) PURE;
    STDMETHOD_(bool, LookupSubPic)(REFERENCE_TIME rtNow /*[in]*/, CComPtr<ISubPic>& pSubPic /*[out]*/) PURE;

    STDMETHOD(GetStats)(int& nSubPics, REFERENCE_TIME & rtNow, REFERENCE_TIME & rtStart, REFERENCE_TIME& rtStop /*[out]*/) PURE;
    STDMETHOD(GetStats)(int nSubPic /*[in]*/, REFERENCE_TIME & rtStart, REFERENCE_TIME& rtStop /*[out]*/) PURE;

    STDMETHOD_(bool, LookupSubPic)(REFERENCE_TIME rtNow /*[in]*/, bool bAdviseBlocking, CComPtr<ISubPic>& pSubPic /*[out]*/) PURE;
};

//
// ISubPicAllocatorPresenter
//
#define TARGET_FRAME 0
#define TARGET_SCREEN 1

interface __declspec(uuid("CF75B1F0-535C-4074-8869-B15F177F944E"))
ISubPicAllocatorPresenter :
public IUnknown {
    STDMETHOD(CreateRenderer)(IUnknown** ppRenderer) PURE;

    STDMETHOD_(SIZE, GetVideoSize)(bool bCorrectAR) const PURE;
    STDMETHOD_(void, SetPosition)(RECT w, RECT v) PURE;
    STDMETHOD_(bool, Paint)(bool bAll) PURE;

    STDMETHOD_(void, SetTime)(REFERENCE_TIME rtNow) PURE;
    STDMETHOD_(void, SetSubtitleDelay)(int delayMs) PURE;
    STDMETHOD_(int, GetSubtitleDelay)() const PURE;
    STDMETHOD_(double, GetFPS)() const PURE;

    STDMETHOD_(void, SetSubPicProvider)(ISubPicProvider * pSubPicProvider) PURE;
    STDMETHOD_(void, Invalidate)(REFERENCE_TIME rtInvalidate = -1) PURE;

    STDMETHOD(GetDIB)(BYTE * lpDib, DWORD * size) PURE;
	STDMETHOD (GetDisplayedImage) (LPVOID* dibImage) PURE;

    STDMETHOD(SetVideoAngle)(Vector v) PURE;
    STDMETHOD(SetPixelShader)(LPCSTR pSrcData, LPCSTR pTarget) PURE;

    STDMETHOD_(bool, ResetDevice)() PURE;
    STDMETHOD_(bool, DisplayChange)() PURE;

    STDMETHOD_(void, GetPosition)(RECT* windowRect, RECT* videoRect) PURE;

    STDMETHOD_(void, SetVideoMediaType)(CMediaType input) PURE;
};

interface __declspec(uuid("767AEBA8-A084-488a-89C8-F6B74E53A90F"))
ISubPicAllocatorPresenter2 :
public ISubPicAllocatorPresenter {
    STDMETHOD(SetPixelShader2)(LPCSTR pSrcData, LPCSTR pTarget, bool bScreenSpace) PURE;
    STDMETHOD_(SIZE, GetVisibleVideoSize)() const PURE;

    STDMETHOD_(bool, IsRendering)() PURE;
    STDMETHOD(SetIsRendering)(bool bIsRendering) PURE;

    STDMETHOD(SetDefaultVideoAngle)(Vector v) PURE;
};


interface __declspec(uuid("AD863F43-83F9-4B8E-962C-426F2BDBEAEF"))
ISubPicAllocatorPresenter3 :
public ISubPicAllocatorPresenter2 {
	STDMETHOD (SetRotation) (int rotation) PURE;
	STDMETHOD_(int, GetRotation) () PURE;
	STDMETHOD (SetFlip) (bool flip) PURE;
	STDMETHOD_(bool, GetFlip) () PURE;
	STDMETHOD (GetVideoFrame) (BYTE* lpDib, DWORD* size) PURE;
	STDMETHOD_(int, GetPixelShaderMode) () PURE;
	STDMETHOD (ClearPixelShaders) (int target) PURE;
	STDMETHOD (AddPixelShader) (int target, LPCWSTR name, LPCSTR profile, LPCSTR sourceCode) PURE;
	STDMETHOD_(bool, ResizeDevice) () PURE;
    STDMETHOD_(bool, ToggleStats) () PURE;
};

// ISubPicAllocatorPresenter4 — additive interface for the optional second (secondary) subtitle pipeline.
// Renderers supporting dual subtitles override SupportsDualSubtitles() to return true and implement the
// secondary pipeline (queue2/allocator2); the base presenter disables it by default (returns false).
interface __declspec(uuid("7E4C2A91-3B5D-4F86-9A1C-D80E5F62B374"))
ISubPicAllocatorPresenter4 :
public ISubPicAllocatorPresenter3 {
    STDMETHOD_(void, SetSubPicProvider2)(ISubPicProvider * pSubPicProvider2) PURE;
    STDMETHOD_(void, SetSecondaryDelay)(int delayMs) PURE;
    STDMETHOD_(int,  GetSecondaryDelay)() const PURE;
    STDMETHOD_(void, InvalidateSubtitle2)(REFERENCE_TIME rtInvalidate = -1) PURE;
    STDMETHOD_(bool, SupportsDualSubtitles)() PURE;
};

//
// ISubStream
//

interface __declspec(uuid("DE11E2FB-02D3-45e4-A174-6B7CE2783BDB"))
ISubStream :
public IPersist {
    STDMETHOD_(int, GetStreamCount)() PURE;
    STDMETHOD(GetStreamInfo)(int i, WCHAR** ppName, LCID * pLCID) PURE;
    STDMETHOD_(int, GetStream)() PURE;
    STDMETHOD(SetStream)(int iStream) PURE;
    STDMETHOD(Reload)() PURE;
    STDMETHOD(SetSourceTargetInfo)(CString yuvMatrix, int targetBlackLevel, int targetWhiteLevel) PURE;
    virtual CString GetPath() { return _T(""); }

    // TODO: get rid of IPersist to identify type and use only
    // interface functions to modify the settings of the substream
};
