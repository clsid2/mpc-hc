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
#include <cmath>
#include <intrin.h>
#include <vector>
#include "Rasterizer.h"
#include "SeparableFilter.h"
#include "../SubPic/ISubPic.h"
#include <ft2build.h>
#include <freetype/ftoutln.h>
#include <freetype/internal/ftobjs.h>
#include FT_FREETYPE_H
#include FT_SYNTHESIS_H

#define DEBUG_PERFORMANCE 0
#if DEBUG_PERFORMANCE
#include <sstream>
#include <chrono>
#define BEGIN_PERF_TIMER(a) \
    std::chrono::steady_clock::time_point begin##a = std::chrono::steady_clock::now(); \
    std::wostringstream ss##a; \

#define TRACE_PERF_TIMER(a, idx, ref1, ref2) \
    std::chrono::steady_clock::time_point end##a##idx = std::chrono::steady_clock::now(); \
    ss##a << "\n"; \
    ss##a << std::chrono::time_point_cast<std::chrono::microseconds>(end##a##idx).time_since_epoch().count(); \
    ss##a << ": "##ref1 << "(" << ref2 << ") = "; \
    ss##a << std::chrono::duration_cast<std::chrono::microseconds>(end##a##idx - begin##a).count() << "[µs]" << std::endl; \

#define END_PERF_TIMER(a, ref1, ref2) \
    TRACE_PERF_TIMER(a, 0, ref1, ref2) \
    TRACE("%s\n", ss##a.str().c_str()); \

#else
#define BEGIN_PERF_TIMER(a)
#define TRACE_PERF_TIMER(a, idx, ref1, ref2)
#define END_PERF_TIMER(a, ref1, ref2)
#endif

int Rasterizer::getOverlayWidth() const
{
    return m_pOverlayData ? m_pOverlayData->mOverlayWidth * 8 : 0;
}

Rasterizer::Rasterizer()
    : fFirstSet(false)
    , mpPathTypes(nullptr)
    , mpPathPoints(nullptr)
    , mPathPoints(0)
    , m_bUseAVX2(false)
    , mpEdgeBuffer(nullptr)
    , mEdgeHeapSize(0)
    , mEdgeNext(0)
    , mpScanBuffer(nullptr)
{
    int cpuinfo[4];
    __cpuid(cpuinfo, 1);
    //bool sse2    = cpuinfo[3] & (1 << 26);
    //bool sse42   = cpuinfo[2] & (1 << 20);
    bool avxflag = cpuinfo[2] & (1 << 28);
    bool osxsave = cpuinfo[2] & (1 << 27);
    bool xsave   = cpuinfo[2] & (1 << 26);
    if (avxflag && osxsave && xsave) {
        __cpuidex(cpuinfo, 7, 0);
        if (cpuinfo[1] & (1 << 5)) {
            unsigned long long xcrFeatureMask = _xgetbv(_XCR_XFEATURE_ENABLED_MASK);
            m_bUseAVX2 = (xcrFeatureMask & 0x6) == 0x6;
        }
    }
}

Rasterizer::~Rasterizer()
{
    _TrashPath();
}

void Rasterizer::_TrashPath()
{
    delete [] mpPathTypes;
    delete [] mpPathPoints;
    mpPathTypes = nullptr;
    mpPathPoints = nullptr;
    mPathPoints = 0;
}

void Rasterizer::_ReallocEdgeBuffer(unsigned int edges)
{
    Edge* pNewEdgeBuffer = (Edge*)realloc(mpEdgeBuffer, sizeof(Edge) * edges);
    if (pNewEdgeBuffer) {
        mpEdgeBuffer = pNewEdgeBuffer;
        mEdgeHeapSize = edges;
    } else {
        AfxThrowMemoryException();
    }
}

void Rasterizer::AnalyzeBezierMinMax(int ptbase, bool fBSpline, int& minx, int& maxx, int& miny, int& maxy)
{
    const POINT* pt0 = mpPathPoints + ptbase;
    const POINT* pt1 = mpPathPoints + ptbase + 1;
    const POINT* pt2 = mpPathPoints + ptbase + 2;
    const POINT* pt3 = mpPathPoints + ptbase + 3;

    double x0 = pt0->x;
    double x1 = pt1->x;
    double x2 = pt2->x;
    double x3 = pt3->x;
    double y0 = pt0->y;
    double y1 = pt1->y;
    double y2 = pt2->y;
    double y3 = pt3->y;

    double cx3, cx2, cx1, cx0, cy3, cy2, cy1, cy0;

    if (fBSpline) {
        double _1div6 = 1.0 / 6.0;

        cx3 = _1div6 * (-  x0 + 3 * x1 - 3 * x2 + x3);
        cx2 = _1div6 * (3 * x0 - 6 * x1 + 3 * x2);
        cx1 = _1div6 * (-3 * x0    + 3 * x2);
        cx0 = _1div6 * (x0 + 4 * x1 + 1 * x2);

        cy3 = _1div6 * (-  y0 + 3 * y1 - 3 * y2 + y3);
        cy2 = _1div6 * (3 * y0 - 6 * y1 + 3 * y2);
        cy1 = _1div6 * (-3 * y0     + 3 * y2);
        cy0 = _1div6 * (y0 + 4 * y1 + 1 * y2);
    } else { // bezier
        cx3 = -  x0 + 3 * x1 - 3 * x2 + x3;
        cx2 =  3 * x0 - 6 * x1 + 3 * x2;
        cx1 = -3 * x0 + 3 * x1;
        cx0 = x0;

        cy3 = -  y0 + 3 * y1 - 3 * y2 + y3;
        cy2 =  3 * y0 - 6 * y1 + 3 * y2;
        cy1 = -3 * y0 + 3 * y1;
        cy0 = y0;
    }

    double maxaccel1 = fabs(2 * cy2) + fabs(6 * cy3);
    double maxaccel2 = fabs(2 * cx2) + fabs(6 * cx3);

    double maxaccel = maxaccel1 > maxaccel2 ? maxaccel1 : maxaccel2;
    double h = 1.0;

    if (maxaccel > 8.0) {
        h = sqrt(8.0 / maxaccel);
    }

    for (double t = 0; t < 1.0; t += h) {
        double x = cx0 + t * (cx1 + t * (cx2 + t * cx3));
        double y = cy0 + t * (cy1 + t * (cy2 + t * cy3));
        int ix = (int)x;
        int iy = (int)y;
        if (ix < minx) {
            minx = ix;
        }
        if (ix > maxx) {
            maxx = ix;
        }
        if (iy < miny) {
            miny = iy;
        }
        if (iy > maxy) {
            maxy = iy;
        }
    }

    double x = cx0 + cx1 + cx2 + cx3;
    double y = cy0 + cy1 + cy2 + cy3;
    int ix = (int)x;
    int iy = (int)y;
    if (ix < minx) {
        minx = ix;
    }
    if (ix > maxx) {
        maxx = ix;
    }
    if (iy < miny) {
        miny = iy;
    }
    if (iy > maxy) {
        maxy = iy;
    }
}

void Rasterizer::_EvaluateBezier(int ptbase, bool fBSpline)
{
    const POINT* pt0 = mpPathPoints + ptbase;
    const POINT* pt1 = mpPathPoints + ptbase + 1;
    const POINT* pt2 = mpPathPoints + ptbase + 2;
    const POINT* pt3 = mpPathPoints + ptbase + 3;

    double x0 = pt0->x;
    double x1 = pt1->x;
    double x2 = pt2->x;
    double x3 = pt3->x;
    double y0 = pt0->y;
    double y1 = pt1->y;
    double y2 = pt2->y;
    double y3 = pt3->y;

    double cx3, cx2, cx1, cx0, cy3, cy2, cy1, cy0;

    if (fBSpline) {
        // 1   [-1 +3 -3 +1]
        // - * [+3 -6 +3  0]
        // 6   [-3  0 +3  0]
        //     [+1 +4 +1  0]

        double _1div6 = 1.0 / 6.0;

        cx3 = _1div6 * (-  x0 + 3 * x1 - 3 * x2 + x3);
        cx2 = _1div6 * (3 * x0 - 6 * x1 + 3 * x2);
        cx1 = _1div6 * (-3 * x0    + 3 * x2);
        cx0 = _1div6 * (x0 + 4 * x1 + 1 * x2);

        cy3 = _1div6 * (-  y0 + 3 * y1 - 3 * y2 + y3);
        cy2 = _1div6 * (3 * y0 - 6 * y1 + 3 * y2);
        cy1 = _1div6 * (-3 * y0     + 3 * y2);
        cy0 = _1div6 * (y0 + 4 * y1 + 1 * y2);
    } else { // bezier
        // [-1 +3 -3 +1]
        // [+3 -6 +3  0]
        // [-3 +3  0  0]
        // [+1  0  0  0]

        cx3 = -  x0 + 3 * x1 - 3 * x2 + x3;
        cx2 =  3 * x0 - 6 * x1 + 3 * x2;
        cx1 = -3 * x0 + 3 * x1;
        cx0 = x0;

        cy3 = -  y0 + 3 * y1 - 3 * y2 + y3;
        cy2 =  3 * y0 - 6 * y1 + 3 * y2;
        cy1 = -3 * y0 + 3 * y1;
        cy0 = y0;
    }

    //
    // This equation is from Graphics Gems I.
    //
    // The idea is that since we're approximating a cubic curve with lines,
    // any error we incur is due to the curvature of the line, which we can
    // estimate by calculating the maximum acceleration of the curve.  For
    // a cubic, the acceleration (second derivative) is a line, meaning that
    // the absolute maximum acceleration must occur at either the beginning
    // (|c2|) or the end (|c2+c3|).  Our bounds here are a little more
    // conservative than that, but that's okay.
    //
    // If the acceleration of the parametric formula is zero (c2 = c3 = 0),
    // that component of the curve is linear and does not incur any error.
    // If a=0 for both X and Y, the curve is a line segment and we can
    // use a step size of 1.

    double maxaccel1 = fabs(2 * cy2) + fabs(6 * cy3);
    double maxaccel2 = fabs(2 * cx2) + fabs(6 * cx3);

    double maxaccel = maxaccel1 > maxaccel2 ? maxaccel1 : maxaccel2;
    double h = 1.0;

    if (maxaccel > 8.0) {
        h = sqrt(8.0 / maxaccel);
    }

    if (!fFirstSet) {
        firstp.x = (LONG)cx0;
        firstp.y = (LONG)cy0;
        lastp = firstp;
        fFirstSet = true;
    }

    for (double t = 0; t < 1.0; t += h) {
        double x = cx0 + t * (cx1 + t * (cx2 + t * cx3));
        double y = cy0 + t * (cy1 + t * (cy2 + t * cy3));
        _EvaluateLine(lastp.x, lastp.y, (int)x, (int)y);
    }

    double x = cx0 + cx1 + cx2 + cx3;
    double y = cy0 + cy1 + cy2 + cy3;
    _EvaluateLine(lastp.x, lastp.y, (int)x, (int)y);
}

void Rasterizer::_EvaluateLine(int pt1idx, int pt2idx)
{
    const POINT* pt1 = mpPathPoints + pt1idx;
    const POINT* pt2 = mpPathPoints + pt2idx;

    _EvaluateLine(pt1->x, pt1->y, pt2->x, pt2->y);
}

void Rasterizer::_EvaluateLine(int x0, int y0, int x1, int y1)
{
    if (lastp.x != x0 || lastp.y != y0) {
        _EvaluateLine(lastp.x, lastp.y, x0, y0);
    }

    if (!fFirstSet) {
        firstp.x = x0;
        firstp.y = y0;
        fFirstSet = true;
    }
    lastp.x = x1;
    lastp.y = y1;

    if (y1 > y0) {
        _EvaluateLine<LINE_UP>(x0, y0, x1, y1);
    } else if (y1 < y0) {
        _EvaluateLine<LINE_DOWN>(x1, y1, x0, y0);
    }
}

template<int flag>
void Rasterizer::_EvaluateLine(int x0, int y0, int x1, int y1)
{
    __int64 xacc = (__int64)x0 << 13;

    // prestep

    int dy = y1 - y0;
    int y = ((y0 + 3) & ~7) + 4;
    int iy = y >> 3;

    y1 = (y1 - 5) >> 3;

    ASSERT(y1 < m_pOutlineData->mHeight);

    if (iy <= y1) {
        __int64 invslope = (__int64(x1 - x0) << 16) / dy;

        if (mEdgeNext + y1 + 1 - iy > mEdgeHeapSize) {
            unsigned int nSize = mEdgeHeapSize * 2;
            while (mEdgeNext + y1 + 1 - iy > nSize) {
                nSize *= 2;
            }
            _ReallocEdgeBuffer(nSize);
        }

        xacc += (invslope * (y - y0)) >> 3;

        while (iy <= y1) {
            int ix = (int)((xacc + 32768) >> 16);

            mpEdgeBuffer[mEdgeNext].next = mpScanBuffer[iy];
            mpEdgeBuffer[mEdgeNext].posandflag = (ix << 1) | flag;

            mpScanBuffer[iy] = mEdgeNext++;

            ++iy;
            xacc += invslope;
        }
    }
}

bool Rasterizer::BeginPath(HDC hdc)
{
    _TrashPath();

    return !!::BeginPath(hdc);
}

bool Rasterizer::EndPath(HDC hdc)
{
    ::CloseFigure(hdc);

    if (::EndPath(hdc)) {
        mPathPoints = GetPath(hdc, nullptr, nullptr, 0);

        if (!mPathPoints) {
            return true;
        }

        mpPathTypes = (BYTE*)malloc(sizeof(BYTE) * mPathPoints);
        mpPathPoints = (POINT*)malloc(sizeof(POINT) * mPathPoints);

        if (mPathPoints == GetPath(hdc, mpPathPoints, mpPathTypes, mPathPoints)) {
            return true;
        }
    }

    ::AbortPath(hdc);

    return false;
}

bool Rasterizer::PartialBeginPath(HDC hdc, bool bClearPath)
{
    if (bClearPath) {
        _TrashPath();
    }

    return !!::BeginPath(hdc);
}

bool Rasterizer::ResizePath(int nPoints) {
    BYTE* pNewTypes = (BYTE*)realloc(mpPathTypes, (mPathPoints + nPoints) * sizeof(BYTE));
    if (pNewTypes) {
        mpPathTypes = pNewTypes;
    } else {
        return false;
    }

    POINT* pNewPoints = (POINT*)realloc(mpPathPoints, (mPathPoints + nPoints) * sizeof(POINT));
    if (pNewPoints) {
        mpPathPoints = pNewPoints;
    } else {
        return false;
    }

    return true;
}

bool Rasterizer::PartialEndPath(HDC hdc, long dx, long dy)
{
    ::CloseFigure(hdc);

    if (::EndPath(hdc)) {
        int nPoints;

        nPoints = GetPath(hdc, nullptr, nullptr, 0);

        if (nPoints < 1) {
            return true;
        }

        bool resizeSuccess = ResizePath(nPoints);

        BYTE* pTypes = DEBUG_NEW BYTE[nPoints];
        POINT* pPoints = DEBUG_NEW POINT[nPoints];

        if (resizeSuccess && nPoints == GetPath(hdc, pPoints, pTypes, nPoints)) {
            for (ptrdiff_t i = 0; i < nPoints; ++i) {
                mpPathPoints[mPathPoints + i].x = pPoints[i].x + dx;
                mpPathPoints[mPathPoints + i].y = pPoints[i].y + dy;
                mpPathTypes[mPathPoints + i] = pTypes[i];
            }

            mPathPoints += nPoints;

            delete [] pTypes;
            delete [] pPoints;
            return true;
        } else {
            ASSERT(FALSE);
        }

        delete [] pTypes;
        delete [] pPoints;
    }

    ::AbortPath(hdc);

    return false;
}

bool Rasterizer::ScanConvert()
{
    try {
        int lastmoveto = INT_MAX;
        int i;

        // Drop any outlines we may have.

        m_pOutlineData = std::make_shared<COutlineData>();

        // Determine bounding box

        if (!mPathPoints) {
            return false;
        }

        int minx = INT_MAX;
        int miny = INT_MAX;
        int maxx = INT_MIN;
        int maxy = INT_MIN;

        int bezier_idx = 0;
        for (i = 0; i < mPathPoints; ++i) {
            int ix = mpPathPoints[i].x;
            int iy = mpPathPoints[i].y;

            // in case of Bezier/BSpline calculate the actual min/max
            BYTE pt = mpPathTypes[i] & ~PT_CLOSEFIGURE;
            if (pt == PT_BEZIERTO || pt == PT_BSPLINETO) {
                bezier_idx++;
                if (bezier_idx == 1 && i > 0 && (mPathPoints >= i + 3)) {
                    AnalyzeBezierMinMax(i - 1, pt == PT_BSPLINETO, minx, maxx, miny, maxy);
                }
                if (bezier_idx == 3) {
                    bezier_idx = 0;
                }
                continue;
            } else {
                bezier_idx = 0;
            }

            if (ix < minx) {
                minx = ix;
            }
            if (ix > maxx) {
                maxx = ix;
            }
            if (iy < miny) {
                miny = iy;
            }
            if (iy > maxy) {
                maxy = iy;
            }
        }

        minx = (minx >> 3) & ~7;
        miny = (miny >> 3) & ~7;
        maxx = (maxx + 7) >> 3;
        maxy = (maxy + 7) >> 3;

        for (i = 0; i < mPathPoints; ++i) {
            mpPathPoints[i].x -= minx * 8;
            mpPathPoints[i].y -= miny * 8;
        }

        if (minx > maxx || miny > maxy) {
            _TrashPath();
            return true;
        }

        m_pOutlineData->mWidth = maxx + 1 - minx;
        m_pOutlineData->mHeight = maxy + 1 - miny;
        m_pOutlineData->mPathOffsetX = minx;
        m_pOutlineData->mPathOffsetY = miny;

        // Initialize edge buffer.  We use edge 0 as a sentinel.

        mEdgeNext = 1;
        mEdgeHeapSize = 2048;
        mpEdgeBuffer = (Edge*)malloc(sizeof(Edge) * mEdgeHeapSize);
        if (!mpEdgeBuffer) {
            TRACE(_T("Rasterizer::ScanConvert: Failed to allocate mpEdgeBuffer\n"));
            return false;
        }

        // Initialize scanline list.
        mpScanBuffer = DEBUG_NEW unsigned int[m_pOutlineData->mHeight];
        ZeroMemory(mpScanBuffer, m_pOutlineData->mHeight * sizeof(unsigned int));

        // Scan convert the outline.  Yuck, Bezier curves....

        // Unfortunately, Windows 95/98 GDI has a bad habit of giving us text
        // paths with all but the first figure left open, so we can't rely
        // on the PT_CLOSEFIGURE flag being used appropriately.

        fFirstSet = false;
        firstp.x = firstp.y = 0;
        lastp.x = lastp.y = 0;

        for (i = 0; i < mPathPoints; ++i) {
            BYTE t = mpPathTypes[i] & ~PT_CLOSEFIGURE;

            switch (t) {
                case PT_MOVETO:
                    if (lastmoveto >= 0 && firstp != lastp) {
                        _EvaluateLine(lastp.x, lastp.y, firstp.x, firstp.y);
                    }
                    lastmoveto = i;
                    fFirstSet = false;
                    lastp = mpPathPoints[i];
                    break;
                case PT_MOVETONC:
                    break;
                case PT_LINETO:
                    if (mPathPoints - (i - 1) >= 2) {
                        _EvaluateLine(i - 1, i);
                    }
                    break;
                case PT_BEZIERTO:
                    if (mPathPoints - (i - 1) >= 4) {
                        _EvaluateBezier(i - 1, false);
                    }
                    i += 2;
                    break;
                case PT_BSPLINETO:
                    if (mPathPoints - (i - 1) >= 4) {
                        _EvaluateBezier(i - 1, true);
                    }
                    i += 2;
                    break;
                case PT_BSPLINEPATCHTO:
                    if (mPathPoints - (i - 3) >= 4) {
                        _EvaluateBezier(i - 3, true);
                    }
                    break;
            }

        }

        if (lastmoveto >= 0 && firstp != lastp) {
            _EvaluateLine(lastp.x, lastp.y, firstp.x, firstp.y);
        }

        // Free the path since we don't need it anymore.

        _TrashPath();

        // Convert the edges to spans.  We couldn't do this before because some of
        // the regions may have winding numbers >+1 and it would have been a pain
        // to try to adjust the spans on the fly.  We use one heap to detangle
        // a scanline's worth of edges from the singly-linked lists, and another
        // to collect the actual scans.

        std::vector<int> heap;

        m_pOutlineData->mOutline.reserve(mEdgeNext / 2);

        for (__int64 y = 0; y < m_pOutlineData->mHeight; ++y) {
            int count = 0;

            // Detangle scanline into edge heap.

            for (size_t ptr = (mpScanBuffer[y] & (unsigned int)(-1)); ptr; ptr = mpEdgeBuffer[ptr].next) {
                heap.emplace_back(mpEdgeBuffer[ptr].posandflag);
            }

            // Sort edge heap.  Note that we conveniently made the opening edges
            // one more than closing edges at the same spot, so we won't have any
            // problems with abutting spans.

            std::sort(heap.begin(), heap.end()/*begin() + heap.size()*/);

            // Process edges and add spans.  Since we only check for a non-zero
            // winding number, it doesn't matter which way the outlines go!

            auto itX1 = heap.cbegin();
            auto itX2 = heap.cend(); // begin() + heap.size();

            size_t x1 = 0;
            size_t x2;

            for (; itX1 != itX2; ++itX1) {
                size_t x = *itX1;

                if (!count) {
                    x1 = (x >> 1);
                }

                if (x & LINE_UP) {
                    ++count;
                } else {
                    --count;
                }

                if (!count) {
                    x2 = (x >> 1);

                    if (x2 > x1) {
                        m_pOutlineData->mOutline.emplace_back((y << 32) + x1 + 0x4000000040000000i64, (y << 32) + x2 + 0x4000000040000000i64); // G: damn Avery, this is evil! :)
                    }
                }
            }

            heap.clear();
        }

        // Dump the edge and scan buffers, since we no longer need them.
        free(mpEdgeBuffer);
        delete[] mpScanBuffer;

        // All done!
        return true;
    } catch (CMemoryException* e) {
        TRACE(_T("Rasterizer::ScanConvert: Memory allocation failed\n"));
        free(mpEdgeBuffer);
        delete[] mpScanBuffer;
        e->Delete();
        return false;
    }
}

void Rasterizer::_OverlapRegion(tSpanBuffer& dst, const tSpanBuffer& src, int dx, int dy)
{
    tSpanBuffer temp;

    temp.reserve(dst.size() + src.size());

    dst.swap(temp);

    auto itA = temp.cbegin();
    auto itAE = temp.cend();
    auto itB = src.cbegin();
    auto itBE = src.cend();

    // Don't worry -- even if dy<0 this will still work! // G: hehe, the evil twin :)

    unsigned __int64 offset1 = (((__int64)dy) << 32) - dx;
    unsigned __int64 offset2 = (((__int64)dy) << 32) + dx;

    while (itA != itAE && itB != itBE) {
        if (itB->first + offset1 < itA->first) {
            // B span is earlier.  Use it.

            unsigned __int64 x1 = itB->first + offset1;
            unsigned __int64 x2 = itB->second + offset2;

            ++itB;

            // B spans don't overlap, so begin merge loop with A first.

            for (;;) {
                while (itA != itAE && itA->first <= x2) {
                    x2 = std::max(x2, itA->second);
                    ++itA;
                }

                // If we run out of B spans or the B span doesn't overlap,
                // then the next A span can't either (because A spans don't
                // overlap) and we exit.

                if (itB == itBE || itB->first + offset1 > x2) {
                    break;
                }

                do {
                    x2 = std::max(x2, itB->second + offset2);
                    ++itB;
                } while (itB != itBE && itB->first + offset1 <= x2);

                // If we run out of A spans or the A span doesn't overlap,
                // then the next B span can't either (because B spans don't
                // overlap) and we exit.

                if (itA == itAE || itA->first > x2) {
                    break;
                }
            }

            // Flush span.

            dst.emplace_back(x1, x2);
        } else {
            // A span is earlier.  Use it.

            unsigned __int64 x1 = itA->first;
            unsigned __int64 x2 = itA->second;

            ++itA;

            // A spans don't overlap, so begin merge loop with B first.

            for (;;) {
                while (itB != itBE && itB->first + offset1 <= x2) {
                    x2 = std::max(x2, itB->second + offset2);
                    ++itB;
                }

                // If we run out of A spans or the A span doesn't overlap,
                // then the next B span can't either (because B spans don't
                // overlap) and we exit.

                if (itA == itAE || itA->first > x2) {
                    break;
                }

                do {
                    x2 = std::max(x2, itA->second);
                    ++itA;
                } while (itA != itAE && itA->first <= x2);

                // If we run out of B spans or the B span doesn't overlap,
                // then the next A span can't either (because A spans don't
                // overlap) and we exit.

                if (itB == itBE || itB->first + offset1 > x2) {
                    break;
                }
            }

            // Flush span.

            dst.emplace_back(x1, x2);
        }
    }

    // Copy over leftover spans.

    while (itA != itAE) {
        dst.emplace_back(*itA);
        ++itA;
    }

    while (itB != itBE) {
        unsigned __int64 x1 = itB->first + offset1;
        unsigned __int64 x2 = itB->second + offset2;
        ++itB;
        while (itB != itBE && itB->first + offset1 <= x2) {
            x2 = std::max(x2, itB->second + offset2);
            ++itB;
        }
        dst.emplace_back(x1, x2);
    }
}

bool Rasterizer::CreateWidenedRegion(int rx, int ry)
{
    if (m_pOutlineData->mOutline.empty()) {
        return true;
    }

    if (rx < 0) {
        rx = 0;
    }
    if (ry < 0) {
        ry = 0;
    }

    m_pOutlineData->mWideBorder = std::max(rx, ry);

    if (m_pEllipse) {
        CreateWidenedRegionFast(rx, ry);
    } else if (ry > 0) {
        // Do a half circle.
        // _OverlapRegion mirrors this so both halves are done.
        for (int dy = -ry; dy <= ry; ++dy) {
            int dx = std::lround(sqrt(float(ry * ry - dy * dy)) * float(rx) / float(ry));

            _OverlapRegion(m_pOutlineData->mWideOutline, m_pOutlineData->mOutline, dx, dy);
        }
    } else {
        _OverlapRegion(m_pOutlineData->mWideOutline, m_pOutlineData->mOutline, rx, 0);
    }

    return true;
}

void Rasterizer::CreateWidenedRegionFast(int rx, int ry)
{
    CAtlList<CEllipseCenterGroup> centerGroups;
    std::vector<SpanEndPoint> wideSpanEndPoints;

    try {
        wideSpanEndPoints.reserve(10);
        m_pOutlineData->mWideOutline.reserve(m_pOutlineData->mOutline.size() + m_pOutlineData->mOutline.size() / 2);
    } catch (...) {
        return;
    }

    auto flushLines = [&](int yStart, int yStop, tSpanBuffer & dst) {
        for (int y = yStart; y < yStop; y++) {
            POSITION pos = centerGroups.GetHeadPosition();
            while (pos) {
                POSITION curPos = pos;
                auto& group = centerGroups.GetNext(pos);
                group.FlushLine(y, wideSpanEndPoints);
                if (group.IsEmpty()) {
                    centerGroups.RemoveAt(curPos);
                }
            }

            if (!wideSpanEndPoints.empty()) {
                ASSERT(wideSpanEndPoints.size() % 2 == 0);
                std::sort(wideSpanEndPoints.begin(), wideSpanEndPoints.end());

                for (auto it = wideSpanEndPoints.cbegin(); it != wideSpanEndPoints.cend(); ++it) {
                    int xLeft = it->x;

                    int count = 1;
                    do {
                        ++it;
                        if (it->bEnd) {
                            count--;
                        } else {
                            count++;
                        }
                    } while (count > 0);

                    int xRight = it->x;

                    if (xLeft < xRight) {
                        dst.emplace_back(static_cast<unsigned __int64>(y) << 32 | xLeft, static_cast<unsigned __int64>(y) << 32 | xRight);
                    }
                }

                wideSpanEndPoints.clear();
            }
        }
    };

    int yPrec = unsigned int(m_pOutlineData->mOutline.front().first >> 32);
    POSITION pos = centerGroups.GetHeadPosition();
    for (const auto& span : m_pOutlineData->mOutline) {
        int y = int(span.first >> 32);
        int xLeft = int(span.first);
        int xRight = int(span.second);

        if (y != yPrec) {
            flushLines(yPrec - ry, y - ry, m_pOutlineData->mWideOutline);
            yPrec = y;
            pos = centerGroups.GetHeadPosition();
        }

        while (pos) {
            int position = centerGroups.GetAt(pos).GetRelativePosition(xLeft, y);
            if (position == CEllipseCenterGroup::INSIDE) {
                break;
            } else if (position == CEllipseCenterGroup::BEFORE) {
                pos = centerGroups.InsertBefore(pos, CEllipseCenterGroup(m_pEllipse));
                break;
            } else {
                centerGroups.GetNext(pos);
            }
        }
        if (!pos) {
            pos = centerGroups.AddTail(CEllipseCenterGroup(m_pEllipse));
        }
        centerGroups.GetNext(pos).AddSpan(y, xLeft, xRight);
    }
    // Flush the remaining of the lines
    flushLines(yPrec - ry, yPrec + ry + 1, m_pOutlineData->mWideOutline);
}

bool Rasterizer::Rasterize(int xsub, int ysub, int fBlur, double fGaussianBlur)
{
    m_pOverlayData = std::make_shared<COverlayData>();

    if (!m_pOutlineData || !m_pOutlineData->mWidth || !m_pOutlineData->mHeight) {
        return true;
    }

    xsub &= 7;
    ysub &= 7;

    int width =  m_pOutlineData->mWidth + xsub;
    int height = m_pOutlineData->mHeight;// + ysub

    m_pOverlayData->mOffsetX = m_pOutlineData->mPathOffsetX - xsub;
    m_pOverlayData->mOffsetY = m_pOutlineData->mPathOffsetY - ysub;

    m_pOutlineData->mWideBorder = (m_pOutlineData->mWideBorder + 7) & ~7;

    if (!m_pOutlineData->mWideOutline.empty() || fBlur || fGaussianBlur > 0) {
        int bluradjust = 0;
        if (fGaussianBlur > 0) {
            bluradjust += (int)(fGaussianBlur * 3 * 8 + 0.5) | 1;
        }
        if (fBlur) {
            bluradjust += 8;
        }

        // Expand the buffer a bit when we're blurring, since that can also widen the borders a bit
        bluradjust = (bluradjust + 7) & ~7;

        width  += 2 * m_pOutlineData->mWideBorder + bluradjust * 2;
        height += 2 * m_pOutlineData->mWideBorder + bluradjust * 2;

        xsub += m_pOutlineData->mWideBorder + bluradjust;
        ysub += m_pOutlineData->mWideBorder + bluradjust;

        m_pOverlayData->mOffsetX -= m_pOutlineData->mWideBorder + bluradjust;
        m_pOverlayData->mOffsetY -= m_pOutlineData->mWideBorder + bluradjust;
    }

    m_pOverlayData->mOverlayWidth = ((width + 7) >> 3) + 1;
    // fixed image height
    m_pOverlayData->mOverlayHeight = ((height + 14) >> 3) + 1;
    m_pOverlayData->mOverlayPitch = (m_pOverlayData->mOverlayWidth + 15) & ~15; // Round the next multiple of 16

    uint64_t buffersize = m_pOverlayData->mOverlayPitch * m_pOverlayData->mOverlayHeight;
    if (buffersize > 134217728ULL) {
        TRACE(L"Skipping subtitle rasterize due to excessive overlay size\n");
        m_pOverlayData = nullptr;
        return false;
    }
    m_pOverlayData->mpOverlayBufferBody = (byte*)_aligned_malloc(buffersize, 16);
    if (!m_pOverlayData->mpOverlayBufferBody) {
        m_pOverlayData = nullptr;
        return false;
    }
    m_pOverlayData->mpOverlayBufferBorder = (byte*)_aligned_malloc(buffersize, 16);
    if (!m_pOverlayData->mpOverlayBufferBorder) {
        _aligned_free(m_pOverlayData->mpOverlayBufferBody);
        m_pOverlayData->mpOverlayBufferBody = nullptr;
        m_pOverlayData = nullptr;
        return false;
    }

    ZeroMemory(m_pOverlayData->mpOverlayBufferBody, buffersize);
    ZeroMemory(m_pOverlayData->mpOverlayBufferBorder, buffersize);

    // Are we doing a border?

    const tSpanBuffer* pOutline[2] = { &m_pOutlineData->mOutline, &m_pOutlineData->mWideOutline };

    for (ptrdiff_t i = _countof(pOutline) - 1; i >= 0; i--) {
        auto it = pOutline[i]->cbegin();
        auto itEnd = pOutline[i]->cend();
        byte* buffer = (i == 0) ? m_pOverlayData->mpOverlayBufferBody : m_pOverlayData->mpOverlayBufferBorder;

        for (; it != itEnd; ++it) {
            unsigned __int64 f = (*it).first;
            unsigned int y = (f >> 32) - 0x40000000 + ysub;
            unsigned int x1 = (f & 0xffffffff) - 0x40000000 + xsub;

            unsigned __int64 s = (*it).second;
            unsigned int x2 = (s & 0xffffffff) - 0x40000000 + xsub;

            if (x2 > x1) {
                unsigned int first = x1 >> 3;
                unsigned int last = (x2 - 1) >> 3;
                byte* dst = buffer + m_pOverlayData->mOverlayPitch * (y >> 3) + first;

                if (first == last) {
                    *dst += byte(x2 - x1);
                } else {
                    *dst += byte(((first + 1) << 3) - x1);
                    ++dst;

                    while (++first < last) {
                        *dst += 0x08;
                        ++dst;
                    }

                    *dst += byte(x2 - (last << 3));
                }
            }
        }
    }

    // Do some gaussian blur magic
    if (fGaussianBlur > 0) {
        GaussianKernel filter(fGaussianBlur);
        if (m_pOverlayData->mOverlayWidth >= filter.width && m_pOverlayData->mOverlayHeight >= filter.width) {
            size_t pitch = m_pOverlayData->mOverlayPitch;

            byte* tmp = (byte*)_aligned_malloc(pitch * m_pOverlayData->mOverlayHeight * sizeof(byte), 16);
            if (!tmp) {
                return false;
            }

            byte* src = m_pOutlineData->mWideOutline.empty() ? m_pOverlayData->mpOverlayBufferBody : m_pOverlayData->mpOverlayBufferBorder;

#if defined(_M_IX86_FP) && _M_IX86_FP < 2
            if (!m_bUseSSE2) {
                SeparableFilterX<1>(src, tmp, m_pOverlayData->mOverlayWidth, m_pOverlayData->mOverlayHeight, pitch,
                                    filter.kernel, filter.width, filter.divisor);
                SeparableFilterY<1>(tmp, src, m_pOverlayData->mOverlayWidth, m_pOverlayData->mOverlayHeight, pitch,
                                    filter.kernel, filter.width, filter.divisor);
            } else
#endif
            {
                SeparableFilterX_SSE2(src, tmp, m_pOverlayData->mOverlayWidth, m_pOverlayData->mOverlayHeight, pitch,
                                      filter.kernel, filter.width, filter.divisor);
                SeparableFilterY_SSE2(tmp, src, m_pOverlayData->mOverlayWidth, m_pOverlayData->mOverlayHeight, pitch,
                                      filter.kernel, filter.width, filter.divisor);
            }

            _aligned_free(tmp);
        }
    }

    // If we're blurring, do a 3x3 box blur
    // Can't do it on subpictures smaller than 3x3 pixels
    for (int pass = 0; pass < fBlur; pass++) {
        if (m_pOverlayData->mOverlayWidth >= 3 && m_pOverlayData->mOverlayHeight >= 3) {
            int pitch = m_pOverlayData->mOverlayPitch;

            byte* tmp = DEBUG_NEW byte[pitch * m_pOverlayData->mOverlayHeight];
            if (!tmp) {
                return false;
            }

            byte* buffer = m_pOutlineData->mWideOutline.empty() ? m_pOverlayData->mpOverlayBufferBody : m_pOverlayData->mpOverlayBufferBorder;
            memcpy(tmp, buffer, pitch * m_pOverlayData->mOverlayHeight);

            // This could be done in a separated way and win some speed
            for (ptrdiff_t j = 1; j < m_pOverlayData->mOverlayHeight - 1; j++) {
                byte* src = tmp + pitch * j + 1;
                byte* dst = buffer + pitch * j + 1;

                for (ptrdiff_t i = 1; i < m_pOverlayData->mOverlayWidth - 1; i++, src++, dst++) {
                    *dst = (src[-1 - pitch] + (src[-pitch] << 1) + src[+1 - pitch]
                            + (src[-1] << 1) + (src[0] << 2) + (src[+1] << 1)
                            + src[-1 + pitch] + (src[+pitch] << 1) + src[+1 + pitch]) >> 4;
                }
            }

            delete [] tmp;
        }
    }

    return true;
}

namespace
{
    struct C {

        static __forceinline DWORD safe_subtract(DWORD a, DWORD b) {
            return a > b ? a - b : 0;
        }

        static __forceinline void pix_mix(DWORD* dst, DWORD color, DWORD alpha) {
            const int ROUNDING_ERR = 1 << (6 - 1);
            DWORD a = (alpha * (color >> 24) + ROUNDING_ERR) >> 6;
            DWORD ia = 256 - a;
            a += 1;

            *dst = ((((*dst & 0x00ff00ff) * ia + (color & 0x00ff00ff) * a) & 0xff00ff00) >> 8) |
                   ((((*dst & 0x0000ff00) * ia + (color & 0x0000ff00) * a) & 0x00ff0000) >> 8) |
                   ((((*dst >> 8) & 0x00ff0000) * ia) & 0xff000000);
        }

        static __forceinline void pix_mix(DWORD* dst, DWORD color, DWORD shapealpha, DWORD clipalpha) {
            const int ROUNDING_ERR = 1 << (12 - 1);
            DWORD a = (shapealpha * clipalpha * (color >> 24) + ROUNDING_ERR) >> 12;
            DWORD ia = 256 - a;
            a += 1;

            *dst = ((((*dst & 0x00ff00ff) * ia + (color & 0x00ff00ff) * a) & 0xff00ff00) >> 8) |
                   ((((*dst & 0x0000ff00) * ia + (color & 0x0000ff00) * a) & 0x00ff0000) >> 8) |
                   ((((*dst >> 8) & 0x00ff0000) * ia) & 0xff000000);
        }

        template <typename... Args>
        static __forceinline void pix_mix_row(BYTE* __restrict dst, const BYTE* __restrict alpha, int w, DWORD color,
                                              Args... args) {
            DWORD* __restrict dst_w = reinterpret_cast<DWORD* __restrict>(dst);
            for (int wt = 0; wt < w; ++wt) {
                pix_mix(&dst_w[wt], color, alpha[wt], args[wt]...);
            }
        }

        template <typename... Args>
        static __forceinline void pix_mix_row(BYTE* __restrict dst, const BYTE alpha, int w, DWORD color, Args... args) {
            DWORD* __restrict dst_w = reinterpret_cast<DWORD* __restrict>(dst);
            for (int wt = 0; wt < w; ++wt) {
                pix_mix(&dst_w[wt], color, alpha, args[wt]...);
            }
        }

        static __forceinline void pix_mix(DWORD* __restrict dst, DWORD color, DWORD border, DWORD, DWORD body) {
            pix_mix(dst, color, safe_subtract(border, body));
        }

        static __forceinline void pix_mix(DWORD* __restrict dst, DWORD color, DWORD border, DWORD, DWORD body,
                                          DWORD clipalpha) {
            pix_mix(dst, color, safe_subtract(border, body), clipalpha);
        }
    };

    struct SSE2 {

        static __forceinline DWORD safe_subtract(DWORD a, DWORD b) {
            __m128i ap = _mm_cvtsi32_si128(a);
            __m128i bp = _mm_cvtsi32_si128(b);
            __m128i rp = _mm_subs_epu16(ap, bp);

            return (DWORD)_mm_cvtsi128_si32(rp);
        }

        // Calculate alpha value SSE2
        static __forceinline __m128i calc_alpha_value(__m128i alpha, DWORD color, size_t) {
            const int ROUNDING_ERR = 1 << (6 - 1);

            const __m128i zero = _mm_setzero_si128();
            const __m128i color_alpha_128 = _mm_set1_epi16(color >> 24);
            const __m128i round_err_128 = _mm_set1_epi16(ROUNDING_ERR);

            __m128i srchi = alpha;
            alpha = _mm_unpacklo_epi8(alpha, zero);
            srchi = _mm_unpackhi_epi8(srchi, zero);
            alpha = _mm_mullo_epi16(alpha, color_alpha_128);
            srchi = _mm_mullo_epi16(srchi, color_alpha_128);
            alpha = _mm_adds_epu16(alpha, round_err_128);
            alpha = _mm_srli_epi16(alpha, 6);
            srchi = _mm_adds_epu16(srchi, round_err_128);
            srchi = _mm_srli_epi16(srchi, 6);
            alpha = _mm_packus_epi16(alpha, srchi);

            return alpha;
        }

        static __forceinline __m128i calc_alpha_value(__m128i border, DWORD color, const BYTE* __restrict,
                                                      const BYTE* __restrict body, size_t i) {
            return calc_alpha_value(_mm_subs_epu8(border, _mm_loadu_si128(reinterpret_cast<const __m128i*>(body + i))),
                                    color, 0);
        }

        static __forceinline __m128i calc_alpha_value(__m128i alpha, DWORD color, const BYTE* __restrict am, size_t i) {
            const int ROUNDING_ERR = 1 << (12 - 1);

            const __m128i color_alpha_128 = _mm_set1_epi16(color >> 24);
            const __m128i round_err_128 = _mm_set1_epi16(ROUNDING_ERR >> 8);

            const __m128i zero = _mm_setzero_si128();

            __m128i mask = _mm_loadu_si128(reinterpret_cast<const __m128i*>(am + i));
            __m128i src1hi = alpha;
            __m128i maskhi = mask;

            alpha = _mm_unpacklo_epi8(alpha, zero);
            src1hi = _mm_unpackhi_epi8(src1hi, zero);
            mask = _mm_unpacklo_epi8(zero, mask);
            maskhi = _mm_unpackhi_epi8(zero, maskhi);
            alpha = _mm_mullo_epi16(alpha, color_alpha_128);
            src1hi = _mm_mullo_epi16(src1hi, color_alpha_128);
            alpha = _mm_mulhi_epu16(alpha, mask);
            src1hi = _mm_mulhi_epu16(src1hi, maskhi);
            alpha = _mm_adds_epu16(alpha, round_err_128);
            src1hi = _mm_adds_epu16(src1hi, round_err_128);
            alpha = _mm_srli_epi16(alpha, 12 + 8 - 16);
            src1hi = _mm_srli_epi16(src1hi, 12 + 8 - 16);
            alpha = _mm_packus_epi16(alpha, src1hi);

            return alpha;
        }

        static __forceinline __m128i calc_alpha_value(__m128i border, DWORD color, const BYTE* __restrict,
                                                      const BYTE* __restrict body, const BYTE* __restrict am, size_t i) {
            return calc_alpha_value(_mm_subs_epu8(border, _mm_loadu_si128(reinterpret_cast<const __m128i*>(body + i))),
                                    color, am, i);
        }

        static __forceinline void pix_mix(DWORD* __restrict dst, DWORD color, DWORD alpha) {
            const int ROUNDING_ERR = 1 << (6 - 1);
            alpha = ((alpha * (color >> 24) + ROUNDING_ERR) >> 6) & 0xff;
            color &= 0xffffff;

            __m128i zero = _mm_setzero_si128();
            __m128i a = _mm_set1_epi32(((alpha + 1) << 16) | (0x100 - alpha));
            __m128i d = _mm_unpacklo_epi8(_mm_cvtsi32_si128(*dst), zero);
            __m128i s = _mm_unpacklo_epi8(_mm_cvtsi32_si128(color), zero);
            __m128i r = _mm_unpacklo_epi16(d, s);

            r = _mm_madd_epi16(r, a);
            r = _mm_srli_epi32(r, 8);
            r = _mm_packs_epi32(r, r);
            r = _mm_packus_epi16(r, r);

            *dst = (DWORD)_mm_cvtsi128_si32(r);
        }

        static __forceinline void pix_mix(DWORD* __restrict dst, DWORD color, DWORD shapealpha, DWORD clipalpha) {
            const int ROUNDING_ERR = 1 << (12 - 1);
            DWORD alpha = ((shapealpha * clipalpha * (color >> 24) + ROUNDING_ERR) >> 12) & 0xff;
            color &= 0xffffff;

            __m128i zero = _mm_setzero_si128();
            __m128i a = _mm_set1_epi32(((alpha + 1) << 16) | (0x100 - alpha));
            __m128i d = _mm_unpacklo_epi8(_mm_cvtsi32_si128(*dst), zero);
            __m128i s = _mm_unpacklo_epi8(_mm_cvtsi32_si128(color), zero);
            __m128i r = _mm_unpacklo_epi16(d, s);

            r = _mm_madd_epi16(r, a);
            r = _mm_srli_epi32(r, 8);
            r = _mm_packs_epi32(r, r);
            r = _mm_packus_epi16(r, r);

            *dst = (DWORD)_mm_cvtsi128_si32(r);
        }

        static __forceinline void pix_mix(DWORD* __restrict dst, DWORD color, DWORD border, DWORD, DWORD body) {
            pix_mix(dst, color, safe_subtract(border, body));
        }

        static __forceinline void pix_mix(DWORD* __restrict dst, DWORD color, DWORD border, DWORD, DWORD body,
                                          DWORD clipalpha) {
            pix_mix(dst, color, safe_subtract(border, body), clipalpha);
        }

        static __forceinline __m128i pix_mix_row(const __m128i& dst, const __m128i& c_r, const __m128i& c_g,
                                                 const __m128i& c_b, const __m128i& a) {
            __m128i d_a, d_r, d_g, d_b;

            d_a = _mm_srli_epi32(dst, 24);

            d_r = _mm_slli_epi32(dst, 8);
            d_r = _mm_srli_epi32(d_r, 24);

            d_g = _mm_slli_epi32(dst, 16);
            d_g = _mm_srli_epi32(d_g, 24);

            d_b = _mm_slli_epi32(dst, 24);
            d_b = _mm_srli_epi32(d_b, 24);

            d_r = _mm_or_si128(d_r, c_r);
            d_g = _mm_or_si128(d_g, c_g);
            d_b = _mm_or_si128(d_b, c_b);

            d_a = _mm_mullo_epi16(d_a, a);
            d_r = _mm_madd_epi16(d_r, a);
            d_g = _mm_madd_epi16(d_g, a);
            d_b = _mm_madd_epi16(d_b, a);

            d_a = _mm_srli_epi32(d_a, 8);
            d_r = _mm_srli_epi32(d_r, 8);
            d_g = _mm_srli_epi32(d_g, 8);
            d_b = _mm_srli_epi32(d_b, 8);

            d_a = _mm_slli_epi32(d_a, 24);
            d_r = _mm_slli_epi32(d_r, 16);
            d_g = _mm_slli_epi32(d_g, 8);

            d_b = _mm_or_si128(d_b, d_g);
            d_b = _mm_or_si128(d_b, d_r);

            return _mm_or_si128(d_b, d_a);
        }

        template <typename... Args>
        static __forceinline void pix_mix_row(BYTE* __restrict dst, const BYTE* __restrict alpha, int w, DWORD color,
                                              Args... args) {
            const __m128i c_r = _mm_set1_epi32((color & 0xFF0000));
            const __m128i c_g = _mm_set1_epi32((color & 0xFF00) << 8);
            const __m128i c_b = _mm_set1_epi32((color & 0xFF) << 16);

            const __m128i zero = _mm_setzero_si128();
            const __m128i ones = _mm_set1_epi16(0x1);

            const BYTE* alpha_end0 = alpha + (w & ~15);
            const BYTE* alpha_end = alpha + w;

            int i = 0;
            for (; alpha < alpha_end0; alpha += 16, dst += 16 * 4, i += 16) {
                __m128i a = _mm_loadu_si128(reinterpret_cast<const __m128i*>(alpha));
                __m128i d1 = _mm_loadu_si128(reinterpret_cast<const __m128i*>(dst));
                __m128i d2 = _mm_loadu_si128(reinterpret_cast<const __m128i*>(dst + 16));
                __m128i d3 = _mm_loadu_si128(reinterpret_cast<const __m128i*>(dst + 32));
                __m128i d4 = _mm_loadu_si128(reinterpret_cast<const __m128i*>(dst + 48));

                a = calc_alpha_value(a, color, args..., i);
                __m128i ra = _mm_cmpeq_epi32(zero, zero);
                ra = _mm_xor_si128(ra, a);
                __m128i a1 = _mm_unpacklo_epi8(ra, a);
                __m128i a2 = _mm_unpackhi_epi8(a1, zero);
                a1 = _mm_unpacklo_epi8(a1, zero);
                a1 = _mm_add_epi16(a1, ones);
                a2 = _mm_add_epi16(a2, ones);

                __m128i a3 = _mm_unpackhi_epi8(ra, a);
                __m128i a4 = _mm_unpackhi_epi8(a3, zero);
                a3 = _mm_unpacklo_epi8(a3, zero);
                a3 = _mm_add_epi16(a3, ones);
                a4 = _mm_add_epi16(a4, ones);

                d1 = pix_mix_row(d1, c_r, c_g, c_b, a1);
                d2 = pix_mix_row(d2, c_r, c_g, c_b, a2);
                d3 = pix_mix_row(d3, c_r, c_g, c_b, a3);
                d4 = pix_mix_row(d4, c_r, c_g, c_b, a4);

                _mm_storeu_si128(reinterpret_cast<__m128i*>(dst), d1);
                _mm_storeu_si128(reinterpret_cast<__m128i*>(dst + 16), d2);
                _mm_storeu_si128(reinterpret_cast<__m128i*>(dst + 32), d3);
                _mm_storeu_si128(reinterpret_cast<__m128i*>(dst + 48), d4);
            }
            DWORD* dst_w = reinterpret_cast<DWORD*>(dst);
            for (; alpha < alpha_end; alpha++, dst_w++, i++) {
                pix_mix(dst_w, color, *alpha, args[i]...);
            }
        }

        template <typename... Args>
        static __forceinline void pix_mix_row(BYTE* dst, BYTE alpha, int w, DWORD color, Args... args) {
            const __m128i c_r = _mm_set1_epi32((color & 0xFF0000));
            const __m128i c_g = _mm_set1_epi32((color & 0xFF00) << 8);
            const __m128i c_b = _mm_set1_epi32((color & 0xFF) << 16);

            const int ROUNDING_ERR = 1 << (6 - 1);
            const DWORD a_ = (alpha * (color >> 24) + ROUNDING_ERR) >> 6;
            const __m128i a = _mm_set1_epi32(((a_ + 1) << 16) | (0x100 - a_));

            const BYTE* dst_end0 = dst + ((4 * w) & ~63);
            const BYTE* dst_end = dst + 4 * w;
            for (; dst < dst_end0; dst += 16 * 4) {
                __m128i d1 = _mm_loadu_si128(reinterpret_cast<const __m128i*>(dst));
                __m128i d2 = _mm_loadu_si128(reinterpret_cast<const __m128i*>(dst + 16));
                __m128i d3 = _mm_loadu_si128(reinterpret_cast<const __m128i*>(dst + 32));
                __m128i d4 = _mm_loadu_si128(reinterpret_cast<const __m128i*>(dst + 48));

                d1 = pix_mix_row(d1, c_r, c_g, c_b, a);
                d2 = pix_mix_row(d2, c_r, c_g, c_b, a);
                d3 = pix_mix_row(d3, c_r, c_g, c_b, a);
                d4 = pix_mix_row(d4, c_r, c_g, c_b, a);

                _mm_storeu_si128(reinterpret_cast<__m128i*>(dst), d1);
                _mm_storeu_si128(reinterpret_cast<__m128i*>(dst + 16), d2);
                _mm_storeu_si128(reinterpret_cast<__m128i*>(dst + 32), d3);
                _mm_storeu_si128(reinterpret_cast<__m128i*>(dst + 48), d4);
            }
            for (; dst < dst_end; dst += 4) {
                pix_mix(reinterpret_cast<DWORD*>(dst), color, alpha, args...);
            }
        }
    };

    struct AVX2 {

        // Calculate alpha value AVX2
        static __forceinline __m256i __vectorcall calc_alpha_value(__m256i alpha, DWORD color, size_t) {
            const int ROUNDING_ERR = 1 << (6 - 1);

            const __m256i zero = _mm256_setzero_si256();
            const __m256i color_alpha_128 = _mm256_set1_epi16(color >> 24);
            const __m256i round_err_128 = _mm256_set1_epi16(ROUNDING_ERR);

            __m256i srchi = alpha;
            alpha = _mm256_unpacklo_epi8(alpha, zero);
            srchi = _mm256_unpackhi_epi8(srchi, zero);
            alpha = _mm256_mullo_epi16(alpha, color_alpha_128);
            srchi = _mm256_mullo_epi16(srchi, color_alpha_128);
            alpha = _mm256_adds_epu16(alpha, round_err_128);
            alpha = _mm256_srli_epi16(alpha, 6);
            srchi = _mm256_adds_epu16(srchi, round_err_128);
            srchi = _mm256_srli_epi16(srchi, 6);
            alpha = _mm256_packus_epi16(alpha, srchi);

            return alpha;
        }

        static __forceinline __m256i calc_alpha_value(__m256i border, DWORD color, const BYTE* __restrict,
                                                      const BYTE* __restrict body, size_t i) {
            return calc_alpha_value(_mm256_subs_epu8(border, _mm256_lddqu_si256(reinterpret_cast<const __m256i*>(body + i))),
                                    color, 0);
        }

        static __forceinline __m256i calc_alpha_value(__m256i alpha, DWORD color, const BYTE* __restrict am, size_t i) {
            const int ROUNDING_ERR = 1 << (12 - 1);

            const __m256i color_alpha_128 = _mm256_set1_epi16(color >> 24);
            const __m256i round_err_128 = _mm256_set1_epi16(ROUNDING_ERR >> 8);

            const __m256i zero = _mm256_setzero_si256();

            __m256i mask = _mm256_lddqu_si256(reinterpret_cast<const __m256i*>(am + i));
            __m256i src1hi = alpha;
            __m256i maskhi = mask;

            alpha = _mm256_unpacklo_epi8(alpha, zero);
            src1hi = _mm256_unpackhi_epi8(src1hi, zero);
            mask = _mm256_unpacklo_epi8(zero, mask);
            maskhi = _mm256_unpackhi_epi8(zero, maskhi);
            alpha = _mm256_mullo_epi16(alpha, color_alpha_128);
            src1hi = _mm256_mullo_epi16(src1hi, color_alpha_128);
            alpha = _mm256_mulhi_epu16(alpha, mask);
            src1hi = _mm256_mulhi_epu16(src1hi, maskhi);
            alpha = _mm256_adds_epu16(alpha, round_err_128);
            src1hi = _mm256_adds_epu16(src1hi, round_err_128);
            alpha = _mm256_srli_epi16(alpha, 12 + 8 - 16);
            src1hi = _mm256_srli_epi16(src1hi, 12 + 8 - 16);
            alpha = _mm256_packus_epi16(alpha, src1hi);

            return alpha;
        }

        static __forceinline __m256i calc_alpha_value(__m256i border, DWORD color, const BYTE* __restrict,
                                                      const BYTE* __restrict body, const BYTE* __restrict am,
                                                      size_t i) {
            return calc_alpha_value(_mm256_subs_epu8(border, _mm256_lddqu_si256(reinterpret_cast<const __m256i*>(body + i))),
                                    color, am, i);
        }

        static __forceinline __m256i pix_mix_row(const __m256i& dst, const __m256i& c_r, const __m256i& c_g,
                                                 const __m256i& c_b, const __m256i& a) {
            __m256i d_a, d_r, d_g, d_b;

            d_a = _mm256_srli_epi32(dst, 24);

            d_r = _mm256_slli_epi32(dst, 8);
            d_r = _mm256_srli_epi32(d_r, 24);

            d_g = _mm256_slli_epi32(dst, 16);
            d_g = _mm256_srli_epi32(d_g, 24);

            d_b = _mm256_slli_epi32(dst, 24);
            d_b = _mm256_srli_epi32(d_b, 24);

            d_r = _mm256_or_si256(d_r, c_r);
            d_g = _mm256_or_si256(d_g, c_g);
            d_b = _mm256_or_si256(d_b, c_b);

            d_a = _mm256_mullo_epi16(d_a, a);
            d_r = _mm256_madd_epi16(d_r, a);
            d_g = _mm256_madd_epi16(d_g, a);
            d_b = _mm256_madd_epi16(d_b, a);

            d_a = _mm256_srli_epi32(d_a, 8);
            d_r = _mm256_srli_epi32(d_r, 8);
            d_g = _mm256_srli_epi32(d_g, 8);
            d_b = _mm256_srli_epi32(d_b, 8);

            d_a = _mm256_slli_epi32(d_a, 24);
            d_r = _mm256_slli_epi32(d_r, 16);
            d_g = _mm256_slli_epi32(d_g, 8);

            d_b = _mm256_or_si256(d_b, d_g);
            d_b = _mm256_or_si256(d_b, d_r);

            return _mm256_or_si256(d_b, d_a);
        }

        template <typename... Args>
        static __forceinline void pix_mix_row(BYTE* __restrict dst, const BYTE* __restrict alpha, int w, DWORD color,
                                              Args... args) {
            const __m256i c_r = _mm256_set1_epi32((color & 0xFF0000));
            const __m256i c_g = _mm256_set1_epi32((color & 0xFF00) << 8);
            const __m256i c_b = _mm256_set1_epi32((color & 0xFF) << 16);

            const __m256i zero = _mm256_setzero_si256();
            const __m256i ones = _mm256_set1_epi16(1);

            const BYTE* alpha_end0 = alpha + (w & ~31);
            const BYTE* alpha_end1 = alpha + (w & ~15);
            const BYTE* alpha_end = alpha + w;

            const __m256i perm_mask = _mm256_set_epi32(7, 3, 5, 1, 6, 2, 4, 0);

            int i = 0;
            for (; alpha < alpha_end0; alpha += 32, dst += 32 * 4, i += 32) {
                // TODO: Refactor memory allocation and use aligned loads
                __m256i a = _mm256_lddqu_si256(reinterpret_cast<const __m256i*>(alpha));

                __m256i d1 = _mm256_lddqu_si256(reinterpret_cast<const __m256i*>(dst));
                __m256i d2 = _mm256_lddqu_si256(reinterpret_cast<const __m256i*>(dst + 32));
                __m256i d3 = _mm256_lddqu_si256(reinterpret_cast<const __m256i*>(dst + 64));
                __m256i d4 = _mm256_lddqu_si256(reinterpret_cast<const __m256i*>(dst + 96));

                a = calc_alpha_value(a, color, args..., i);
                a = _mm256_permutevar8x32_epi32(a, perm_mask);

                __m256i ra = _mm256_cmpeq_epi32(zero, zero);
                ra = _mm256_xor_si256(ra, a);

                __m256i a1 = _mm256_unpacklo_epi8(ra, a);
                __m256i a2 = _mm256_unpackhi_epi8(ra, a);

                __m256i a3 = _mm256_unpackhi_epi8(a1, zero);
                a1 = _mm256_unpacklo_epi8(a1, zero);

                __m256i a4 = _mm256_unpackhi_epi8(a2, zero);
                a2 = _mm256_unpacklo_epi8(a2, zero);

                a1 = _mm256_add_epi16(a1, ones);
                a3 = _mm256_add_epi16(a3, ones);

                a2 = _mm256_add_epi16(a2, ones);
                a4 = _mm256_add_epi16(a4, ones);

                d1 = pix_mix_row(d1, c_r, c_g, c_b, a1);
                d2 = pix_mix_row(d2, c_r, c_g, c_b, a2);
                d3 = pix_mix_row(d3, c_r, c_g, c_b, a3);
                d4 = pix_mix_row(d4, c_r, c_g, c_b, a4);

                _mm256_storeu_si256(reinterpret_cast<__m256i*>(dst), d1);
                _mm256_storeu_si256(reinterpret_cast<__m256i*>(dst + 32), d2);
                _mm256_storeu_si256(reinterpret_cast<__m256i*>(dst + 64), d3);
                _mm256_storeu_si256(reinterpret_cast<__m256i*>(dst + 96), d4);
            }

            // Zero upper halves of YMM registers to avoid AVX/SSE translation penalties
            _mm256_zeroupper();

            // We could compute tail with masked loads/stores, but they are expensive, so it is better to do 128-bit vectors
            // instead
            // for (; alpha < alpha_end1 - 16; alpha += 16, dst += 16 * 4, i += 16) {
            if (alpha_end0 != alpha_end1) {
                const auto zero_low = _mm256_castsi256_si128(zero);
                const auto ones_low = _mm256_castsi256_si128(ones);

                __m128i a = _mm_lddqu_si128(reinterpret_cast<const __m128i*>(alpha));
                __m128i d1 = _mm_lddqu_si128(reinterpret_cast<const __m128i*>(dst));
                __m128i d2 = _mm_lddqu_si128(reinterpret_cast<const __m128i*>(dst + 16));
                __m128i d3 = _mm_lddqu_si128(reinterpret_cast<const __m128i*>(dst + 32));
                __m128i d4 = _mm_lddqu_si128(reinterpret_cast<const __m128i*>(dst + 48));

                a = SSE2::calc_alpha_value(a, color, args..., i);
                __m128i ra = _mm_cmpeq_epi32(zero_low, zero_low);
                ra = _mm_xor_si128(ra, a);
                __m128i a1 = _mm_unpacklo_epi8(ra, a);
                __m128i a2 = _mm_unpackhi_epi8(a1, zero_low);
                a1 = _mm_unpacklo_epi8(a1, zero_low);
                a1 = _mm_add_epi16(a1, ones_low);
                a2 = _mm_add_epi16(a2, ones_low);

                __m128i a3 = _mm_unpackhi_epi8(ra, a);
                __m128i a4 = _mm_unpackhi_epi8(a3, zero_low);
                a3 = _mm_unpacklo_epi8(a3, zero_low);
                a3 = _mm_add_epi16(a3, ones_low);
                a4 = _mm_add_epi16(a4, ones_low);

                const auto c_r_low = _mm256_castsi256_si128(c_r);
                const auto c_g_low = _mm256_castsi256_si128(c_g);
                const auto c_b_low = _mm256_castsi256_si128(c_b);

                d1 = SSE2::pix_mix_row(d1, c_r_low, c_g_low, c_b_low, a1);
                d2 = SSE2::pix_mix_row(d2, c_r_low, c_g_low, c_b_low, a2);
                d3 = SSE2::pix_mix_row(d3, c_r_low, c_g_low, c_b_low, a3);
                d4 = SSE2::pix_mix_row(d4, c_r_low, c_g_low, c_b_low, a4);

                _mm_storeu_si128(reinterpret_cast<__m128i*>(dst), d1);
                _mm_storeu_si128(reinterpret_cast<__m128i*>(dst + 16), d2);
                _mm_storeu_si128(reinterpret_cast<__m128i*>(dst + 32), d3);
                _mm_storeu_si128(reinterpret_cast<__m128i*>(dst + 48), d4);

                alpha += 16, dst += 16 * 4, i += 16;
            }

            DWORD* dst_w = reinterpret_cast<DWORD*>(dst);
            for (; alpha < alpha_end; alpha++, dst_w++, i++) {
                SSE2::pix_mix(dst_w, color, *alpha, args[i]...);
            }
        }

        template <typename... Args>
        static __forceinline void pix_mix_row(BYTE* dst, BYTE alpha, int w, DWORD color, Args... args) {
            const __m256i c_r = _mm256_set1_epi32((color & 0xFF0000));
            const __m256i c_g = _mm256_set1_epi32((color & 0xFF00) << 8);
            const __m256i c_b = _mm256_set1_epi32((color & 0xFF) << 16);

            const int ROUNDING_ERR = 1 << (6 - 1);
            const DWORD a_ = (alpha * (color >> 24) + ROUNDING_ERR) >> 6;
            const __m256i a = _mm256_set1_epi32(((a_ + 1) << 16) | (0x100 - a_));

            const BYTE* dst_end0 = dst + ((4 * w) & ~127);
            const BYTE* dst_end = dst + 4 * w;
            for (; dst < dst_end0; dst += 32 * 4) {
                __m256i d1 = _mm256_lddqu_si256(reinterpret_cast<const __m256i*>(dst));
                __m256i d2 = _mm256_lddqu_si256(reinterpret_cast<const __m256i*>(dst + 32));
                __m256i d3 = _mm256_lddqu_si256(reinterpret_cast<const __m256i*>(dst + 64));
                __m256i d4 = _mm256_lddqu_si256(reinterpret_cast<const __m256i*>(dst + 96));

                d1 = pix_mix_row(d1, c_r, c_g, c_b, a);
                d2 = pix_mix_row(d2, c_r, c_g, c_b, a);
                d3 = pix_mix_row(d3, c_r, c_g, c_b, a);
                d4 = pix_mix_row(d4, c_r, c_g, c_b, a);

                _mm256_storeu_si256(reinterpret_cast<__m256i*>(dst), d1);
                _mm256_storeu_si256(reinterpret_cast<__m256i*>(dst + 32), d2);
                _mm256_storeu_si256(reinterpret_cast<__m256i*>(dst + 64), d3);
                _mm256_storeu_si256(reinterpret_cast<__m256i*>(dst + 96), d4);
            }

            // Zero upper halves of YMM registers to avoid AVX/SSE translation penalties
            _mm256_zeroupper();

            if (dst_end - dst_end0 >= 16 * 4) {
                __m128i d1 = _mm_loadu_si128(reinterpret_cast<const __m128i*>(dst));
                __m128i d2 = _mm_loadu_si128(reinterpret_cast<const __m128i*>(dst + 16));
                __m128i d3 = _mm_loadu_si128(reinterpret_cast<const __m128i*>(dst + 32));
                __m128i d4 = _mm_loadu_si128(reinterpret_cast<const __m128i*>(dst + 48));

                auto c_r_low = _mm256_castsi256_si128(c_r);
                auto c_g_low = _mm256_castsi256_si128(c_g);
                auto c_b_low = _mm256_castsi256_si128(c_b);
                auto a_low = _mm256_castsi256_si128(a);

                d1 = SSE2::pix_mix_row(d1, c_r_low, c_g_low, c_b_low, a_low);
                d2 = SSE2::pix_mix_row(d2, c_r_low, c_g_low, c_b_low, a_low);
                d3 = SSE2::pix_mix_row(d3, c_r_low, c_g_low, c_b_low, a_low);
                d4 = SSE2::pix_mix_row(d4, c_r_low, c_g_low, c_b_low, a_low);

                _mm_storeu_si128(reinterpret_cast<__m128i*>(dst), d1);
                _mm_storeu_si128(reinterpret_cast<__m128i*>(dst + 16), d2);
                _mm_storeu_si128(reinterpret_cast<__m128i*>(dst + 32), d3);
                _mm_storeu_si128(reinterpret_cast<__m128i*>(dst + 48), d4);

                dst += 4 * 16;
            }

            for (; dst < dst_end; dst += 4) {
                SSE2::pix_mix(reinterpret_cast<DWORD*>(dst), color, alpha, args...);
            }
        }
    };

    ///////////////////////////////////////////////////////////////////////////

    // Draw single color fill or shadow
    template <typename VER>
    void DrawInternal(BYTE* __restrict dst, int pitch, const BYTE* __restrict alpha, int overlay_pitch, int width,
                      int height, const DWORD* __restrict switchpts)
    {
        // The <<6 is due to pixmix expecting the alpha parameter to be
        // the multiplication of two 6-bit unsigned numbers but we
        // only have one here. (No alpha mask.)
        while (height--) {
            VER::pix_mix_row(dst, alpha, width, switchpts[0]);
            alpha += overlay_pitch;
            dst += pitch;
        }
    }

    // Draw single color border
    template <typename VER>
    void DrawInternal(BYTE* __restrict dst, int pitch, const BYTE* __restrict alpha, int overlay_pitch, int width,
                      int height, const DWORD* __restrict switchpts, const BYTE* __restrict srcBorder,
                      const BYTE* __restrict srcBody)
    {
        // src contains two different bitmaps, interlaced per pixel.
        // The first stored is the fill, the second is the widened
        // fill region created by CreateWidenedRegion().
        // Since we're drawing only the border, we must obtain that
        // by subtracting the fill from the widened region. The
        // subtraction must be saturating since the widened region
        // pixel value can be smaller than the fill value.
        // This happens when blur edges is used.

        while (height--) {
            VER::pix_mix_row(dst, alpha, width, switchpts[0], srcBorder, srcBody);
            srcBody += overlay_pitch;
            alpha += overlay_pitch;
            dst += pitch;
        }
    }

    // Draw multi color fill or shadow
    template <typename VER>
    void DrawInternal(BYTE* __restrict dst, int pitch, const BYTE* __restrict alpha, int overlay_pitch, int width,
                      int height, const DWORD* __restrict switchpts, int xo)
    {
        // xo is the offset (usually negative) we have moved into the image
        // So if we have passed the switchpoint (?) switch to another color
        // (So switchpts stores both colours *and* coordinates?)
        const int len = std::max(0, std::min(int(switchpts[3]) - xo, width));
        const int len1 = width - len;
        const int len_bytes = len * sizeof(DWORD);

        while (height--) {
            VER::pix_mix_row(dst, alpha, len, switchpts[0]);
            dst += len_bytes;
            alpha += len;
            VER::pix_mix_row(dst, alpha, len1, switchpts[2]);
            dst += pitch - len_bytes;
            alpha += overlay_pitch - len;
        }
    }

    // Draw multi color border
    template <typename VER>
    void DrawInternal(BYTE* __restrict dst, int pitch, const BYTE* __restrict alpha, int overlay_pitch, int width,
                      int height, const DWORD* __restrict switchpts, const BYTE* __restrict srcBorder,
                      const BYTE* __restrict srcBody, int xo)
    {
        const int len = std::max(0, std::min(int(switchpts[3]) - xo, width));
        const int len1 = width - len;
        const int len_bytes = len * sizeof(DWORD);

        while (height--) {
            VER::pix_mix_row(dst, alpha, len, switchpts[0], srcBorder, srcBody);
            dst += len_bytes;
            alpha += len;
            VER::pix_mix_row(dst, alpha, len1, switchpts[2], srcBorder, srcBody);
            dst += pitch - len_bytes;
            alpha += overlay_pitch - len;
        }
    }

    // Draw single color border with alpha mask
    template <typename VER>
    void DrawInternal(BYTE* __restrict dst, int pitch, const BYTE* __restrict alpha, int overlay_pitch, int width,
                      int height, const DWORD* __restrict switchpts, const BYTE* __restrict srcBorder,
                      const BYTE* __restrict srcBody, const BYTE* __restrict alpha_mask, int alpha_pitch)
    {
        while (height--) {
            VER::pix_mix_row(dst, alpha, width, switchpts[0], srcBorder, srcBody, alpha_mask);
            alpha_mask += alpha_pitch;
            srcBody += overlay_pitch;
            alpha += overlay_pitch;
            dst += pitch;
        }
    }

    // Draw single color fill or shadow with alpha mask
    template <typename VER>
    void DrawInternal(BYTE* __restrict dst, int pitch, const BYTE* __restrict alpha, int overlay_pitch, int width,
                      int height, const DWORD* __restrict switchpts, const BYTE* __restrict alpha_mask,
                      int alpha_pitch)
    {
        // Both s and am contain 6-bit bitmaps of two different
        // alpha masks; s is the subtitle shape and am is the
        // clipping mask.
        // Multiplying them together yields a 12-bit number.
        // I think some imprecision is introduced here??
        while (height--) {
            VER::pix_mix_row(dst, alpha, width, switchpts[0], alpha_mask);
            alpha_mask += alpha_pitch;
            alpha += overlay_pitch;
            dst += pitch;
        }
    }

    // Draw multi color border with alpha mask
    template <typename VER>
    void DrawInternal(BYTE* __restrict dst, int pitch, const BYTE* __restrict alpha, int overlay_pitch, int width,
                      int height, const DWORD* __restrict switchpts, const BYTE* __restrict srcBorder,
                      const BYTE* __restrict srcBody, const BYTE* __restrict alpha_mask, int alpha_pitch, int xo)
    {
        const int len = std::max(0, std::min(int(switchpts[3]) - xo, width));
        const int len1 = width - len;
        const int len_bytes = len * sizeof(DWORD);

        while (height--) {
            VER::pix_mix_row(dst, alpha, len, switchpts[0], srcBorder, srcBody);
            dst += len_bytes;
            alpha += len;
            alpha_mask += len;
            VER::pix_mix_row(dst, alpha, len1, switchpts[2], srcBorder, srcBody);
            dst += pitch - len_bytes;
            alpha += overlay_pitch - len;
            alpha_mask += alpha_pitch - len;
        }
    }

    // Draw multi color fill or shadow with alpha mask
    template <typename VER>
    void DrawInternal(BYTE* __restrict dst, int pitch, const BYTE* __restrict alpha, int overlay_pitch, int width,
                      int height, const DWORD* __restrict switchpts, const BYTE* __restrict alpha_mask, int alpha_pitch,
                      int xo)
    {
        const int len = std::max(0, std::min(int(switchpts[3]) - xo, width));
        const int len1 = width - len;
        const int len_bytes = len * sizeof(DWORD);

        while (height--) {
            VER::pix_mix_row(dst, alpha, len, switchpts[0], alpha_mask);
            dst += len_bytes;
            alpha += len;
            alpha_mask += len;
            VER::pix_mix_row(dst, alpha, len1, switchpts[2], alpha_mask);
            dst += pitch - len_bytes;
            alpha += overlay_pitch - len;
            alpha_mask += alpha_pitch - len;
        }
    }

    template <typename VER>
    void DrawInternal(BYTE* __restrict dst, int pitch, BYTE alpha, int width, int height, DWORD color)
    {
        while (height--) {
            VER::pix_mix_row(dst, alpha, width, color);
            dst += pitch;
        }
    }

    template <class... Args>
    __forceinline void DrawInternal(bool bUseAVX2, Args&& ... args)
    {
#ifndef __AVX2__
        if (bUseAVX2) {
#endif
            DrawInternal<AVX2>(std::forward<Args>(args)...);
#ifndef __AVX2__
        } else {
            DrawInternal<SSE2>(std::forward<Args>(args)...);
        }
#endif
        // C version is not used, provided only for reference
        // DrawInternal<C>(std::forward<Args>(args)...);
    }
}

// Render a subpicture onto a surface.
// spd is the surface to render on.
// clipRect is a rectangular clip region to render inside.
// pAlphaMask is an alpha clipping mask.
// xsub and ysub ???
// switchpts seems to be an array of fill colours interlaced with coordinates.
//    switchpts[i*2] contains a colour and switchpts[i*2+1] contains the coordinate to use that colour from
// fBody tells whether to render the body of the subs.
// fBorder tells whether to render the border of the subs.
CRect Rasterizer::Draw(SubPicDesc& spd, CRect& clipRect, byte* pAlphaMask, int xsub, int ysub,
                       const DWORD* switchpts, bool fBody, bool fBorder) const
{
    CRect bbox(0, 0, 0, 0);

    if (!m_pOverlayData || !switchpts || (!fBody && !fBorder)) {
        return bbox;
    }

    // Limit drawn area to intersection of rendering surface and rectangular clip area
    CRect r(0, 0, spd.w, spd.h);
    r &= clipRect;

    // Remember that all subtitle coordinates are specified in 1/8 pixels
    // (x+4)>>3 rounds to nearest whole pixel.
    // ??? What is xsub, ysub, mOffsetX and mOffsetY ?
    int x = (xsub + m_pOverlayData->mOffsetX + 4) >> 3;
    int y = (ysub + m_pOverlayData->mOffsetY + 4) >> 3;
    int w = m_pOverlayData->mOverlayWidth;
    int h = m_pOverlayData->mOverlayHeight;
    int xo = 0, yo = 0;

    // Again, limiting?
    if (x < r.left) {
        xo = r.left - x;
        w -= r.left - x;
        x = r.left;
    }
    if (y < r.top) {
        yo = r.top - y;
        h -= r.top - y;
        y = r.top;
    }
    if (x + w > r.right) {
        w = r.right - x;
    }
    if (y + h > r.bottom) {
        h = r.bottom - y;
    }

    // Check if there's actually anything to render
    if (w <= 0 || h <= 0) {
        return bbox;
    }

    bbox.SetRect(x, y, x + w, y + h);
    bbox &= CRect(0, 0, spd.w, spd.h);

    BYTE* srcBody = m_pOverlayData->mpOverlayBufferBody + m_pOverlayData->mOverlayPitch * yo + xo;
    BYTE* srcBorder = m_pOverlayData->mpOverlayBufferBorder + m_pOverlayData->mOverlayPitch * yo + xo;
    BYTE* alphaMask = pAlphaMask + spd.w * y + x;
    BYTE* dst = (BYTE*)((DWORD*)(spd.bits + spd.pitch * y) + x);
    BYTE* s = fBorder ? srcBorder : srcBody;

    enum {
        NONE = 0,
        ALPHA = 1,
        BODY = 1 << 1,
        SWITCHPOINT = 1 << 2,
    };

    int draw_op = 0;
    draw_op |= pAlphaMask ? ALPHA : 0;
    draw_op |= fBody ? BODY : 0;
    draw_op |= switchpts[1] != DWORD_MAX ? SWITCHPOINT : 0;

    switch (draw_op) {
        case BODY:
            // Draw single color fill or shadow
            DrawInternal(m_bUseAVX2, dst, spd.pitch, s, m_pOverlayData->mOverlayPitch, w, h, switchpts);
            break;
        case NONE:
            // Draw single color border
            ASSERT(s == srcBorder);
            __assume(s == srcBorder);
            DrawInternal(m_bUseAVX2, dst, spd.pitch, s, m_pOverlayData->mOverlayPitch, w, h, switchpts, srcBorder,
                         srcBody);
            break;
        case BODY | SWITCHPOINT:
            // Draw multi color fill or shadow
            DrawInternal(m_bUseAVX2, dst, spd.pitch, s, m_pOverlayData->mOverlayPitch, w, h, switchpts, xo);
            break;
        case SWITCHPOINT:
            // Draw multi color border
            ASSERT(s == srcBorder);
            __assume(s == srcBorder);
            DrawInternal(m_bUseAVX2, dst, spd.pitch, s, m_pOverlayData->mOverlayPitch, w, h, switchpts, srcBorder,
                         srcBody, xo);
            break;
        case ALPHA:
            // Draw single color border with alpha mask
            ASSERT(s == srcBorder);
            __assume(s == srcBorder);
            DrawInternal(m_bUseAVX2, dst, spd.pitch, s, m_pOverlayData->mOverlayPitch, w, h, switchpts, srcBorder,
                         srcBody, alphaMask, spd.w);
            break;
        case ALPHA | BODY:
            // Draw single color fill or shadow with alpha mask
            DrawInternal(m_bUseAVX2, dst, spd.pitch, s, m_pOverlayData->mOverlayPitch, w, h, switchpts, alphaMask,
                         spd.w);
            break;
        case ALPHA | SWITCHPOINT:
            // Draw multi color border with alpha mask
            ASSERT(s == srcBorder);
            __assume(s == srcBorder);
            DrawInternal(m_bUseAVX2, dst, spd.pitch, s, m_pOverlayData->mOverlayPitch, w, h, switchpts, srcBorder,
                         srcBody, alphaMask, spd.w, xo);
            break;
        case ALPHA | BODY | SWITCHPOINT:
            // Draw multi color fill or shadow with alpha mask
            DrawInternal(m_bUseAVX2, dst, spd.pitch, s, m_pOverlayData->mOverlayPitch, w, h, switchpts, alphaMask,
                         spd.w, xo);
            break;
        default:
            UNREACHABLE_CODE();
    }

    return bbox;
}

void Rasterizer::FillSolidRect(SubPicDesc& spd, int x, int y, int nWidth, int nHeight, DWORD lColor) const
{
	if (x < 0 || y < 0) {
        ASSERT(FALSE);
        return;
	}
    ASSERT(spd.w >= x + nWidth && spd.h >= y + nHeight);
    BYTE* dst = (BYTE*)((DWORD*)(spd.bits + spd.pitch * y) + x);
    DrawInternal(m_bUseAVX2, dst, spd.pitch, BYTE(0x40), nWidth, nHeight, lColor);
}

inline void Rasterizer::AddFTPath(BYTE type, FT_Pos x, FT_Pos y, FTPathData *data) {
    y = data->tmAscent - y + data->dy;
    x += data->dx;
    data->ftTypes.push_back(type);
    data->ftPoints.push_back({ x, y });
}

static FT_Error ft_move_to(const FT_Vector* to, void* user) {
    FTPathData* data = (FTPathData*)user;
    data->r->AddFTPath(PT_MOVETO, to->x / 64, to->y / 64, data);
    return 0;
}
static FT_Error ft_line_to(const FT_Vector* to, void* user) {
    FTPathData* data = (FTPathData*)user;
    data->r->AddFTPath(PT_LINETO, to->x / 64, to->y / 64, data);
    return 0;
}
static FT_Error ft_conic_to(const FT_Vector* control_1, const FT_Vector* to, void* user) {
    FTPathData* data = (FTPathData*)user;
    data->r->AddFTPath(PT_BEZIERTO, control_1->x / 64, control_1->y / 64, data);
    data->r->AddFTPath(PT_BEZIERTO, (control_1->x+to->x) / 128, (control_1->y+to->y) / 128, data);
    data->r->AddFTPath(PT_BEZIERTO, to->x / 64, to->y / 64, data);
    return 0;
}
static FT_Error ft_cubic_to(const FT_Vector* control_1, const FT_Vector* control_2, const FT_Vector* to, void* user) {
    FTPathData* data = (FTPathData*)user;
    data->r->AddFTPath(PT_BEZIERTO, control_1->x / 64, control_1->y / 64, data);
    data->r->AddFTPath(PT_BEZIERTO, control_2->x / 64, control_2->y / 64, data);
    data->r->AddFTPath(PT_BEZIERTO, to->x / 64, to->y / 64, data);
    return 0;
}

FT_DEFINE_OUTLINE_FUNCS(
    ft_decompose_funcs,
    (FT_Outline_MoveTo_Func)ft_move_to,   /* move_to  */
    (FT_Outline_LineTo_Func)ft_line_to,   /* line_to  */
    (FT_Outline_ConicTo_Func)ft_conic_to,  /* conic_to */
    (FT_Outline_CubicTo_Func)ft_cubic_to,  /* cubic_to */
    0,                                      /* shift    */
    0                                       /* delta    */
)

FT_UInt Rasterizer::GetLangCodePoint(wchar_t ch, faceData& fd) {
    FT_UInt cp;

    if (fd.codePoints.count(ch)) {
        cp = fd.codePoints[ch];
    } else {
        cp = FT_Get_Char_Index(fd.face, ch);
    }
    return cp;
}

bool Rasterizer::GetPathFreeType(HDC hdc, bool bClearPath, std::wstring fontNameK, wchar_t ch, int dx, int dy, CStringA langHint, FTLibraryData* ftLibraryData) {
    BEGIN_PERF_TIMER(GetPathFreeType);
    if (bClearPath) {
        _TrashPath();
    }

    auto& fc = ftLibraryData->GetFaceCache();
    if (fc.count(fontNameK) == 0) {
        return false;
    }

    auto& fd = fc[fontNameK];
    FT_Face& face = fd.face;
    FT_Error error;

    if (ftLibraryData && ftLibraryData->IsInitialized()) {
        //error = FT_Load_Char(face, ch, FT_LOAD_NO_HINTING|FT_LOAD_NO_BITMAP);
        FT_UInt cp =  GetLangCodePoint(ch, fd);
        error = FT_Load_Glyph(face, cp, FT_LOAD_NO_HINTING | FT_LOAD_NO_BITMAP);
        if (!error) {
            FT_Glyph_Metrics* metrics = &face->glyph->metrics;
            FTPathData pd;
            pd.dx = dx;
            pd.dy = dy;
            pd.r = this;
            pd.tmAscent = fc[fontNameK].ascent; //this is the y baseline.  match windows and not Freetype
            error = FT_Outline_Decompose(&face->glyph->outline, &ft_decompose_funcs, (void*)&pd);
#if 0
            for (int a = 0; a < mPathPoints; a++) {
                TRACE("winxxx\t%d\t%d\t%d\n", mpPathPoints[a].x, mpPathPoints[a].y, mpPathTypes[a]);
            }
#endif
            int nPoints = pd.ftPoints.size();
            if (nPoints > 0 && ResizePath(nPoints)) {
                for (int a = 0; a < pd.ftPoints.size(); a++) {
                    mpPathTypes[mPathPoints + a] = pd.ftTypes[a];
                    mpPathPoints[mPathPoints + a] = pd.ftPoints[a];
                }
                mPathPoints += nPoints;
            } else {
                error = nPoints > 0 || !CStringW::StrTraits::IsSpace(ch);
            }
#if 0
            for (int a = mPathPoints - nPoints; a < mPathPoints; a++) {
                TRACE("ft\t%d\t%d\t%d\n", mpPathPoints[a].x, mpPathPoints[a].y, mpPathTypes[a]);
            }
#endif
        }
        if (!error) {
            END_PERF_TIMER(GetPathFreeType, "function", fontNameK);
            return true;
        }
    }
    return false;
}
