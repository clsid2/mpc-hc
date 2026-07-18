/*
 * (C) 2003-2006 Gabest
 * (C) 2006-2015, 2017 see Authors.txt
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

#include "Ellipse.h"
#include <memory>
#include <vector>
#include <unordered_map>
#include "SubRendererSettings.h"
#include "OpenTypeLangTags.h"
#include "FreeTypePathTools.h"

#define PT_MOVETONC         0xfe
#define PT_BSPLINETO        0xfc
#define PT_BSPLINEPATCHTO   0xfa

struct SubPicDesc;
using tSpanBuffer = std::vector<std::pair<unsigned __int64, unsigned __int64>>;

struct COutlineData {
    int mWidth, mHeight;
    int mPathOffsetX, mPathOffsetY;
    int mWideBorder;
    tSpanBuffer mOutline, mWideOutline;

    COutlineData()
        : mWidth(0)
        , mHeight(0)
        , mPathOffsetX(0)
        , mPathOffsetY(0)
        , mWideBorder(0) {}
};

typedef std::shared_ptr<COutlineData> COutlineDataSharedPtr;

struct COverlayData {
    int mOffsetX, mOffsetY;
    int mOverlayWidth, mOverlayHeight, mOverlayPitch;
    byte* mpOverlayBufferBody, *mpOverlayBufferBorder;

    COverlayData()
        : mOffsetX(0)
        , mOffsetY(0)
        , mOverlayWidth(0)
        , mOverlayHeight(0)
        , mOverlayPitch(0)
        , mpOverlayBufferBody(nullptr)
        , mpOverlayBufferBorder(nullptr) {}

    COverlayData(const COverlayData& overlayData)
        : mOffsetX(overlayData.mOffsetX)
        , mOffsetY(overlayData.mOffsetY)
        , mOverlayWidth(overlayData.mOverlayWidth)
        , mOverlayHeight(overlayData.mOverlayHeight)
        , mOverlayPitch(overlayData.mOverlayPitch) {
        if (mOverlayPitch > 0 && mOverlayHeight > 0) {
            uint64_t buffersize = mOverlayPitch * mOverlayHeight;
            if (buffersize <= 134217728ULL) {
                mpOverlayBufferBody = (byte*)_aligned_malloc(buffersize, 16);
                mpOverlayBufferBorder = (byte*)_aligned_malloc(buffersize, 16);
            } else {
                ASSERT(false);
            }
            if (!mpOverlayBufferBody || !mpOverlayBufferBorder) {
                mOffsetX = mOffsetY = 0;
                mOverlayWidth = mOverlayHeight = 0;
                DeleteOverlay();
                return;
            }
            memcpy(mpOverlayBufferBody, overlayData.mpOverlayBufferBody, buffersize);
            memcpy(mpOverlayBufferBorder, overlayData.mpOverlayBufferBorder, buffersize);
        } else {
            mpOverlayBufferBody = mpOverlayBufferBorder = nullptr;
        }
    }

    ~COverlayData() {
        DeleteOverlay();
    }

    COverlayData& operator=(const COverlayData& overlayData) {
        mOffsetX = overlayData.mOffsetX;
        mOffsetY = overlayData.mOffsetY;
        mOverlayWidth = overlayData.mOverlayWidth;
        mOverlayHeight = overlayData.mOverlayHeight;
        mOverlayPitch = overlayData.mOverlayPitch;

        DeleteOverlay();
        if (mOverlayPitch > 0 && mOverlayHeight > 0) {
            mpOverlayBufferBody = (byte*)_aligned_malloc(mOverlayPitch * mOverlayHeight, 16);
            mpOverlayBufferBorder = (byte*)_aligned_malloc(mOverlayPitch * mOverlayHeight, 16);
            if (!mpOverlayBufferBody || !mpOverlayBufferBorder) {
                mOffsetX = mOffsetY = 0;
                mOverlayWidth = mOverlayHeight = 0;
                DeleteOverlay();
            }
            memcpy(mpOverlayBufferBody, overlayData.mpOverlayBufferBody, mOverlayPitch * mOverlayHeight);
            memcpy(mpOverlayBufferBorder, overlayData.mpOverlayBufferBorder, mOverlayPitch * mOverlayHeight);
        } else {
            mpOverlayBufferBody = mpOverlayBufferBorder = nullptr;
        }

        return *this;
    };

    void DeleteOverlay() {
        if (mpOverlayBufferBody) {
            _aligned_free(mpOverlayBufferBody);
            mpOverlayBufferBody = nullptr;
        }
        if (mpOverlayBufferBorder) {
            _aligned_free(mpOverlayBufferBorder);
            mpOverlayBufferBorder = nullptr;
        }
    }
};

class Rasterizer;

typedef std::shared_ptr<COverlayData> COverlayDataSharedPtr;
typedef signed long  FT_Pos;
struct FTPathData {
    std::vector<BYTE> ftTypes;
    std::vector<POINT> ftPoints;
    int dx;
    int dy;
    Rasterizer* r;
    LONG tmAscent;
};


class Rasterizer
{
    bool fFirstSet;
    CPoint firstp, lastp;

protected:
    BYTE* mpPathTypes;
    POINT* mpPathPoints;
    int mPathPoints;
    bool m_bUseAVX2;

private:
    enum {
        LINE_DOWN,
        LINE_UP
    };


    struct Edge {
        int next;
        int posandflag;
    }* mpEdgeBuffer;
    unsigned int mEdgeHeapSize;
    unsigned int mEdgeNext;

    unsigned int* mpScanBuffer;
protected:
    CEllipseSharedPtr m_pEllipse;
    COutlineDataSharedPtr m_pOutlineData;
    COverlayDataSharedPtr m_pOverlayData;

private:
    void _TrashPath();
    void _ReallocEdgeBuffer(unsigned int edges);
    void AnalyzeBezierMinMax(int ptbase, bool fBSpline, int& minx, int& maxx, int& miny, int& maxy);
    void _EvaluateBezier(int ptbase, bool fBSpline);
    void _EvaluateLine(int pt1idx, int pt2idx);
    void _EvaluateLine(int x0, int y0, int x1, int y1);
    // The following function is templated and forcingly inlined for performance sake
    template<int flag> __forceinline void _EvaluateLine(int x0, int y0, int x1, int y1);
    static void _OverlapRegion(tSpanBuffer& dst, const tSpanBuffer& src, int dx, int dy);
    void CreateWidenedRegionFast(int borderX, int borderY);
    bool ResizePath(int nPoints);

public:
    Rasterizer();
    virtual ~Rasterizer();

    bool BeginPath(HDC hdc);
    bool EndPath(HDC hdc);
    bool PartialBeginPath(HDC hdc, bool bClearPath);
    bool PartialEndPath(HDC hdc, long dx, long dy);
    bool ScanConvert();
    bool CreateWidenedRegion(int borderX, int borderY);
    bool Rasterize(int xsub, int ysub, int fBlur, double fGaussianBlur);
    int getOverlayWidth() const;
    FT_UInt GetLangCodePoint(wchar_t ch, faceData& fd);
    bool GetPathFreeType(HDC hdc, bool bClearPath, std::wstring fontNameK, wchar_t ch, int dx, int dy, CStringA langHint, FTLibraryData* ftLibraryData);
    inline void AddFTPath(BYTE type, FT_Pos x, FT_Pos y, FTPathData* data);

    CRect Draw(SubPicDesc& spd, CRect& clipRect, byte* pAlphaMask, int xsub, int ysub, const DWORD* switchpts, bool fBody, bool fBorder) const;
    void FillSolidRect(SubPicDesc& spd, int x, int y, int nWidth, int nHeight, DWORD lColor) const;
};
