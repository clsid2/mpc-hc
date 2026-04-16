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

#include "stdafx.h"
#include "VobSubImage.h"
#include "RTS.h"
#include <cmath>
#include <algorithm>

CVobSubImage::CVobSubImage()
    : org(CSize(0, 0))
    , lpTemp1(nullptr)
    , lpTemp2(nullptr)
    , nPlane(0)
    , bCustomPal(false)
    , bAligned(true)
    , tridx(0)
    , orgpal(nullptr)
    , cuspal(nullptr)
    , nLang(SIZE_T_ERROR)
    , nIdx(SIZE_T_ERROR)
    , bForced(false)
    , bAnimated(false)
    , tCurrent(-1)
    , start(0)
    , delay(0)
    , rect(CRect(0, 0, 0, 0))
    , lpPixels(nullptr)
{
    ZeroMemory(&pal, sizeof(pal));
}

CVobSubImage::~CVobSubImage()
{
    Free();
}

bool CVobSubImage::Alloc(int w, int h)
{
    // if there is nothing to crop TrimSubImage might even add a 1 pixel
    // wide border around the text, that's why we need a bit more memory
    // to be allocated.

    ASSERT(w > 0 && h > 0);

    if (lpTemp1 == nullptr || w * h > org.cx * org.cy || (w + 2) * (h + 2) > (org.cx + 2) * (org.cy + 2)) {
        Free();

        try {
            lpTemp1 = DEBUG_NEW RGBQUAD[w * h];
        } catch (CMemoryException* e) {
            ASSERT(FALSE);
            e->Delete();
            return false;
        }

        try {
            lpTemp2 = DEBUG_NEW RGBQUAD[(w + 2) * (h + 2)];
        } catch (CMemoryException* e) {
            ASSERT(FALSE);
            e->Delete();
            delete [] lpTemp1;
            lpTemp1 = nullptr;
            return false;
        }

        org.cx = w;
        org.cy = h;
    }

    lpPixels = lpTemp1;

    return true;
}

void CVobSubImage::Free()
{
    SAFE_DELETE_ARRAY(lpTemp1);
    SAFE_DELETE_ARRAY(lpTemp2);

    lpPixels = nullptr;
}

bool CVobSubImage::Decode(BYTE* _lpData, size_t _packetSize, size_t _dataSize, int _t,
                          bool _bCustomPal,
                          int _tridx,
                          RGBQUAD* _orgpal /*[16]*/, RGBQUAD* _cuspal /*[4]*/,
                          bool _bTrim)
{
    if (!GetPacketInfo(_lpData, _packetSize, _dataSize, _t)) {
        return false;
    }

    int w = rect.Width();
    int h = rect.Height();
    if (w <= 0 || h <= 0 || !Alloc(w, h)) {
        ASSERT(false);
        return false;
    }

    lpPixels = lpTemp1;

    nPlane = 0;
    bAligned = true;

    bCustomPal = _bCustomPal;
    orgpal = _orgpal;
    tridx = _tridx;
    cuspal = _cuspal;

    CPoint p = rect.TopLeft();

    size_t end[] = { nOffset[1], _dataSize };

    while (nOffset[nPlane] < end[nPlane]) {
        DWORD code;

        if ((code = GetNibble(_lpData)) >= 0x4
                || (code = (code << 4) | GetNibble(_lpData)) >= 0x10
                || (code = (code << 4) | GetNibble(_lpData)) >= 0x40
                || (code = (code << 4) | GetNibble(_lpData)) >= 0x100) {
            DrawPixels(p, code >> 2, code & 3);
            if ((p.x += code >> 2) < rect.right) {
                continue;
            }
        }

        DrawPixels(p, rect.right - p.x, code & 3);

        if (!bAligned) {
            GetNibble(_lpData);    // align to byte
        }

        p.x = rect.left;
        p.y++;
        nPlane = 1 - nPlane;
    }

    rect.bottom = std::min(p.y, rect.bottom);

    if (_bTrim) {
        TrimSubImage();
    }

    return true;
}

bool CVobSubImage::GetPacketInfo(const BYTE* lpData, size_t packetSize, size_t dataSize, int t /*= INT_MAX*/)
{
    bool valid = false;

    //  delay = 0;
    size_t i, nextctrlblk = dataSize;
    WORD _pal = 0, tr = 0;
    WORD nPal = 0, nTr = 0;

    do {
        i = nextctrlblk;

        tCurrent = 1024 * ((lpData[i] << 8) | lpData[i + 1]) / 90;
        i += 2;
        nextctrlblk = (lpData[i] << 8) | lpData[i + 1];
        i += 2;

        if (nextctrlblk > packetSize || nextctrlblk < dataSize) {
            TRACE(L"Invalid VobSub packet\n");
            rect.SetRectEmpty();
            return false;
        }

        if (tCurrent > t) {
            break;
        }

        bool bBreak = false;

        while (!bBreak) {
            size_t len = 0;

            switch (lpData[i]) {
                case 0x00:
                    len = 0;
                    break;
                case 0x01:
                    len = 0;
                    break;
                case 0x02:
                    len = 0;
                    break;
                case 0x03:
                    len = 2;
                    break;
                case 0x04:
                    len = 2;
                    break;
                case 0x05:
                    len = 6;
                    break;
                case 0x06:
                    len = 4;
                    break;
                default:
                    len = 0;
                    break;
            }

            if (i + len >= packetSize) {
                TRACE(_T("Warning: Wrong subpicture parameter block ending\n"));
                break;
            }

            switch (lpData[i++]) {
                case 0x00: // forced start displaying
                    bForced = true;
                    valid = true;
                    break;
                case 0x01: // start displaying
                    bForced = false;
                    valid = true;
                    break;
                case 0x02: // stop displaying
                    delay = tCurrent;
                    valid = true;
                    break;
                case 0x03:
                    _pal = (lpData[i] << 8) | lpData[i + 1];
                    i += 2;
                    nPal++;
                    break;
                case 0x04:
                    tr = (lpData[i] << 8) | lpData[i + 1];
                    i += 2;
                    nTr++;
                    //tr &= 0x00f0;
                    break;
                case 0x05:
                    valid = true;
                    rect = CRect((lpData[i] << 4) + (lpData[i + 1] >> 4),
                                 (lpData[i + 3] << 4) + (lpData[i + 4] >> 4),
                                 ((lpData[i + 1] & 0x0f) << 8) + lpData[i + 2] + 1,
                                 ((lpData[i + 4] & 0x0f) << 8) + lpData[i + 5] + 1);
                    i += 6;
                    break;
                case 0x06:
                    nOffset[0] = (lpData[i] << 8) + lpData[i + 1];
                    i += 2;
                    nOffset[1] = (lpData[i] << 8) + lpData[i + 1];
                    i += 2;
                    break;
                case 0xff: // end of ctrlblk
                    bBreak = true;
                    continue;
                default: // skip this ctrlblk
                    bBreak = true;
                    break;
            }
        }
    } while (i <= nextctrlblk && i < packetSize);

    for (i = 0; i < 4; i++) {
        this->pal[i].pal = (_pal >> (i << 2)) & 0xf;
        this->pal[i].tr = (tr >> (i << 2)) & 0xf;
    }

    bAnimated = (nPal > 1 || nTr > 1);

    if (!valid) {
        TRACE(L"Invalid VobSub packet\n");
        rect.SetRectEmpty();
    }
    return valid;
}

BYTE CVobSubImage::GetNibble(const BYTE* lpData)
{
    size_t& off = nOffset[nPlane];
    BYTE ret = lpData[off];
    if (bAligned) {
        ret >>= 4;
    }
    ret &= 0x0f;
    bAligned = !bAligned;
    if (bAligned) {
        off++;
    }
    return ret;
}

void CVobSubImage::DrawPixels(CPoint p, int length, size_t colorId)
{
    if (length <= 0
            || p.x + length < rect.left
            || p.x >= rect.right
            || p.y < rect.top
            || p.y >= rect.bottom) {
        return;
    }

    if (p.x < rect.left) {
        p.x = rect.left;
    }
    if (p.x + length >= rect.right) {
        length = rect.right - p.x;
    }

    RGBQUAD* ptr = &lpPixels[rect.Width() * (p.y - rect.top) + (p.x - rect.left)];

    RGBQUAD c;

    if (!bCustomPal) {
        c = orgpal[pal[colorId].pal];
        c.rgbReserved = (pal[colorId].tr << 4) | pal[colorId].tr;
    } else {
        c = cuspal[colorId];
    }

    while (length-- > 0) {
        *ptr++ = c;
    }
}

void CVobSubImage::TrimSubImage()
{
    CRect r;
    r.left = rect.Width();
    r.top = rect.Height();
    r.right = 0;
    r.bottom = 0;

    RGBQUAD* ptr = lpTemp1;

    for (int j = 0, y = rect.Height(); j < y; j++) {
        for (int i = 0, x = rect.Width(); i < x; i++, ptr++) {
            if (ptr->rgbReserved) {
                if (r.top > j) {
                    r.top = j;
                }
                if (r.bottom < j) {
                    r.bottom = j;
                }
                if (r.left > i) {
                    r.left = i;
                }
                if (r.right < i) {
                    r.right = i;
                }
            }
        }
    }

    if (r.left > r.right || r.top > r.bottom) {
        return;
    }

    r += CRect(0, 0, 1, 1);

    r &= CRect(CPoint(0, 0), rect.Size());

    int w = r.Width(), h = r.Height();

    DWORD offset = r.top * rect.Width() + r.left;

    r += CRect(1, 1, 1, 1);

    DWORD* src = (DWORD*)&lpTemp1[offset];
    DWORD* dst = (DWORD*)&lpTemp2[1 + w + 1];

    ZeroMemory(lpTemp2, (1 + w + 1)*sizeof(RGBQUAD));

    for (int height = h; height; height--, src += rect.Width()) {
        *dst++ = 0;
        memcpy(dst, src, w * sizeof(RGBQUAD));
        dst += w;
        *dst++ = 0;
    }

    ZeroMemory(dst, (1 + w + 1)*sizeof(RGBQUAD));

    lpPixels = lpTemp2;

    rect = r + rect.TopLeft();
}

////////////////////////////////

#define GP(xx, yy) (((xx) < 0 || (yy) < 0 || (xx) >= w || (yy) >= h) ? 0 : p[(yy) * w + (xx)])

CAutoPtrList<COutline>* CVobSubImage::GetOutlineList(CPoint& topleft)
{
    int w = rect.Width(), h = rect.Height(), len = w * h;
    if (len <= 0) {
        return nullptr;
    }

    CAutoVectorPtr<BYTE> p;
    if (!p.Allocate(len)) {
        return nullptr;
    }

    CAutoPtrList<COutline>* ol;
    try {
        ol = DEBUG_NEW CAutoPtrList<COutline>();
    } catch (CMemoryException* e) {
        ASSERT(FALSE);
        e->Delete();
        return nullptr;
    }

    BYTE* cp = p;
    RGBQUAD* rgbp = (RGBQUAD*)lpPixels;

    for (int i = 0; i < len; i++, cp++, rgbp++) {
        *cp = !!rgbp->rgbReserved;
    }

    enum { UP, RIGHT, DOWN, LEFT };

    topleft.x = topleft.y = INT_MAX;

    for (;;) {
        cp = p;

        int x = 0;
        int y = 0;

        for (y = 0; y < h; y++) {
            for (x = 0; x < w - 1; x++, cp++) {
                if (cp[0] == 0 && cp[1] == 1) {
                    break;
                }
            }

            if (x < w - 1) {
                break;
            }

            cp++;
        }

        if (y == h) {
            break;
        }

        int dir = UP;

        int ox = x, oy = y, odir = dir;

        CAutoPtr<COutline> o;
        try {
            o.Attach(DEBUG_NEW COutline);
        } catch (CMemoryException* e) {
            ASSERT(FALSE);
            e->Delete();
            break;
        }

        do {
            CPoint pp;
            BYTE fl = 0;
            BYTE fr = 0;
            BYTE br = 0;

            int prevdir = dir;

            switch (prevdir) {
                case UP:
                    pp = CPoint(x + 1, y);
                    fl = GP(x, y - 1);
                    fr = GP(x + 1, y - 1);
                    br = GP(x + 1, y);
                    break;
                case RIGHT:
                    pp = CPoint(x + 1, y + 1);
                    fl = GP(x + 1, y);
                    fr = GP(x + 1, y + 1);
                    br = GP(x, y + 1);
                    break;
                case DOWN:
                    pp = CPoint(x, y + 1);
                    fl = GP(x, y + 1);
                    fr = GP(x - 1, y + 1);
                    br = GP(x - 1, y);
                    break;
                case LEFT:
                    pp = CPoint(x, y);
                    fl = GP(x - 1, y);
                    fr = GP(x - 1, y - 1);
                    br = GP(x, y - 1);
                    break;
            }

            // turning left if:
            // o . | o .
            // ^ o | < o
            // turning right if:
            // x x | x >
            // ^ o | x o
            //
            // o set, x empty, . can be anything

            if (fl == 1) {
                dir = (dir - 1 + 4) & 3;
            } else if (fl != 1 && fr != 1 && br == 1) {
                dir = (dir + 1) & 3;
            } else if (p[y * w + x] & 16) {
                ASSERT(0);    // we are going around in one place (this must not happen if the starting conditions were correct)
                break;
            }

            p[y * w + x] = (p[y * w + x] << 1) | 2; // increase turn count (== log2(highordbit(*p)))

            switch (dir) {
                case UP:
                    if (prevdir == LEFT) {
                        x--;
                        y--;
                    }
                    if (prevdir == UP) {
                        y--;
                    }
                    break;
                case RIGHT:
                    if (prevdir == UP) {
                        x++;
                        y--;
                    }
                    if (prevdir == RIGHT) {
                        x++;
                    }
                    break;
                case DOWN:
                    if (prevdir == RIGHT) {
                        x++;
                        y++;
                    }
                    if (prevdir == DOWN) {
                        y++;
                    }
                    break;
                case LEFT:
                    if (prevdir == DOWN) {
                        x--;
                        y++;
                    }
                    if (prevdir == LEFT) {
                        x--;
                    }
                    break;
            }

            int d = dir - prevdir;
            o->Add(pp, d == 3 ? -1 : d == -3 ? 1 : d);

            if (topleft.x > pp.x) {
                topleft.x = pp.x;
            }
            if (topleft.y > pp.y) {
                topleft.y = pp.y;
            }
        } while (!(x == ox && y == oy && dir == odir));

        if (!o->pa.IsEmpty() && (x == ox && y == oy && dir == odir)) {
            ol->AddTail(o);
        } else {
            ASSERT(0);
        }
    }

    return ol;
}

static bool FitLine(const COutline& o, int& start, int& end)
{
    int len = (int)o.pa.GetCount();
    if (len < 7) {
        return false;    // small segments should be handled with beziers...
    }

    for (start = 0; start < len && !o.da[start]; start++) {
        ;
    }
    for (end = len - 1; end > start && !o.da[end]; end--) {
        ;
    }

    if (end - start < 8 || end - start < (len - end) + (start - 0)) {
        return false;
    }

    CUIntArray la, ra;

    UINT i, j, k;

    for (i = start + 1, j = end, k = start; i <= j; i++) {
        if (!o.da[i]) {
            continue;
        }
        if (o.da[i] == o.da[k]) {
            return false;
        }
        if (o.da[i] == -1) {
            la.Add(i - k);
        } else {
            ra.Add(i - k);
        }
        k = i;
    }

    bool fl = true, fr = true;

    // these tests are completly heuristic and might be redundant a bit...

    for (i = 0, j = (UINT)la.GetSize(); i < j && fl; i++) {
        if (la[i] != 1) {
            fl = false;
        }
    }
    for (i = 0, j = (UINT)ra.GetSize(); i < j && fr; i++) {
        if (ra[i] != 1) {
            fr = false;
        }
    }

    if (!fl && !fr) {
        return false;    // can't be a line if there are bigger steps than one in both directions (lines are usually drawn by stepping one either horizontally or vertically)
    }
    if (fl && fr && 1.0 * (end - start) / ((len - end) * 2 + (start - 0) * 2) > 0.4) {
        return false;    // if this section is relatively too small it may only be a rounded corner
    }
    if (!fl && !la.IsEmpty() && la.GetSize() <= 4 && (la[0] == 1 && la[la.GetSize() - 1] == 1)) {
        return false;    // one step at both ends, doesn't sound good for a line (may be it was skewed, so only eliminate smaller sections where beziers going to look just as good)
    }
    if (!fr && !ra.IsEmpty() && ra.GetSize() <= 4 && (ra[0] == 1 && ra[ra.GetSize() - 1] == 1)) {
        return false;    // -''-
    }

    CUIntArray& a = !fl ? la : ra;

    len = (int)a.GetSize();

    int sum = 0;

    for (i = 0, j = INT_MAX, k = 0; i < (UINT)len; i++) {
        if (j > a[i]) {
            j = a[i];
        }
        if (k < a[i]) {
            k = a[i];
        }
        sum += a[i];
    }

    if (k - j > 2 && 1.0 * sum / len < 2) {
        return false;
    }
    if (k - j > 2 && 1.0 * sum / len >= 2 && len < 4) {
        return false;
    }

    if ((la.GetSize() / 2 + ra.GetSize() / 2) / 2 <= 2) {
        if ((k + j) / 2 < 2 && k * j != 1) {
            return false;
        }
    }

    double err = 0;

    CPoint sp = o.pa[start], ep = o.pa[end];

    double minerr = 0, maxerr = 0;

    double vx = ep.x - sp.x, vy = ep.y - sp.y, l = sqrt(vx * vx + vy * vy);
    vx /= l;
    vy /= l;

    for (i = start + 1, j = end - 1; i <= j; i++) {
        CPoint p = o.pa[i], dp = p - sp;
        double t = vx * dp.x + vy * dp.y, dx = vx * t + sp.x - p.x, dy = vy * t + sp.y - p.y;
        t = dx * dx + dy * dy;
        err += t;
        t = sqrt(t);
        if (vy * dx - dy * vx < 0) {
            if (minerr > -t) {
                minerr = -t;
            }
        } else {
            if (maxerr < t) {
                maxerr = t;
            }
        }
    }

    return ((maxerr - minerr) / l < 0.1  || err / l < 1.5 || (fabs(maxerr) < 8 && fabs(minerr) < 8));
}

static int CalcPossibleCurveDegree(const COutline& o)
{
    size_t len2 = o.da.GetCount();

    CUIntArray la;

    for (size_t i = 0, j = 0; j < len2; j++) {
        if (j + 1 == len2 || o.da[j]) {
            la.Add(UINT(j - i));
            i = j;
        }
    }

    ptrdiff_t len = la.GetCount();

    int ret = 0;

    // check if we can find a reason to add a penalty degree, or two :P
    // it is mainly about looking for distant corners
    {
        int penalty = 0;

        int ma[2] = {0, 0};
        for (ptrdiff_t i = 0; i < len; i++) {
            ma[i & 1] += la[i];
        }

        int ca[2] = {ma[0], ma[1]};
        for (ptrdiff_t i = 0; i < len; i++) {
            ca[i & 1] -= la[i];

            double c1 = 1.0 * ca[0] / ma[0], c2 = 1.0 * ca[1] / ma[1], c3 = 1.0 * la[i] / ma[i & 1];

            if (len2 > 16 && (fabs(c1 - c2) > 0.7 || (c3 > 0.6 && la[i] > 5))) {
                penalty = 2;
                break;
            }

            if (fabs(c1 - c2) > 0.6 || (c3 > 0.4 && la[i] > 5)) {
                penalty = 1;
            }
        }

        ret += penalty;
    }

    la[0] <<= 1;
    la[len - 1] <<= 1;

    for (ptrdiff_t i = 0; i < len; i += 2) {
        if (la[i] > 1) {
            ret++;    // prependicular to the last chosen section and bigger then 1 -> add a degree and continue with the other dir
            i--;
        }
    }

    return ret;
}

inline double vectlen(CPoint p)
{
    return sqrt((double)(p.x * p.x + p.y * p.y));
}

inline double vectlen(CPoint p1, CPoint p2)
{
    return vectlen(p2 - p1);
}

static bool MinMaxCosfi(COutline& o, double& mincf, double& maxcf) // not really cosfi, it is weighted by the distance from the segment endpoints, and since it would be always between -1 and 0, the applied sign marks side
{
    CAtlArray<CPoint>& pa = o.pa;

    int len = (int)pa.GetCount();
    if (len < 6) {
        return false;
    }

    mincf = 1;
    maxcf = -1;

    CPoint p = pa[len - 1] - pa[0];
    double l = vectlen(p);
    UNREFERENCED_PARAMETER(l);

    for (ptrdiff_t i = 2; i < len - 2; i++) { // skip the endpoints, they aren't accurate
        CPoint p1 = pa[0] - pa[i], p2 = pa[len - 1] - pa[i];
        double l1 = vectlen(p1), l2 = vectlen(p2);
        int sign = p1.x * p.y - p1.y * p.x >= 0 ? 1 : -1;

        double c = (1.0 * len / 2 - fabs(i - 1.0 * len / 2)) / len * 2; // c: 0 -> 1 -> 0

        double cosfi = (1 + (p1.x * p2.x + p1.y * p2.y) / (l1 * l2)) * sign * c;
        if (mincf > cosfi) {
            mincf = cosfi;
        }
        if (maxcf < cosfi) {
            maxcf = cosfi;
        }
    }

    return true;
}

static bool FitBezierVH(COutline& o, CPoint& p1, CPoint& p2)
{
    int i;

    CAtlArray<CPoint>& pa = o.pa;

    int len = (int)pa.GetCount();

    if (len <= 1) {
        return false;
    } else if (len == 2) {
        CPoint mid = pa[0] + pa[1];
        mid.x >>= 1;
        mid.y >>= 1;
        p1 = p2 = mid;
        return true;
    }

    CPoint dir1 = pa[1] - pa[0], dir2 = pa[len - 2] - pa[len - 1];
    if ((dir1.x && dir1.y) || (dir2.x && dir2.y)) {
        return false;    // we are only fitting beziers with hor./ver. endings
    }

    if (CalcPossibleCurveDegree(o) > 3) {
        return false;
    }

    double mincf, maxcf;
    if (MinMaxCosfi(o, mincf, maxcf)) {
        if (maxcf - mincf > 0.8
                || maxcf - mincf > 0.6 && (maxcf >= 0.4 || mincf <= -0.4)) {
            return false;
        }
    }

    CPoint p0 = p1 = pa[0];
    CPoint p3 = p2 = pa[len - 1];

    CAtlArray<double> pl;
    pl.SetCount(len);

    double c10 = 0, c11 = 0, c12 = 0, c13 = 0, c1x = 0, c1y = 0;
    double c20 = 0, c21 = 0, c22 = 0, c23 = 0, c2x = 0, c2y = 0;
    double length = 0;

    for (pl[0] = 0, i = 1; i < len; i++) {
        CPoint diff = (pa[i] - pa[i - 1]);
        pl[i] = (length += sqrt((double)(diff.x * diff.x + diff.y * diff.y)));
    }

    for (i = 0; i < len; i++) {
        double t1 = pl[i] / length;
        double t2 = t1 * t1;
        double t3 = t2 * t1;
        double it1 = 1 - t1;
        double it2 = it1 * it1;
        double it3 = it2 * it1;

        double dc1 = 3.0 * it2 * t1;
        double dc2 = 3.0 * it1 * t2;

        c10 += it3 * dc1;
        c11 += dc1 * dc1;
        c12 += dc2 * dc1;
        c13 += t3 * dc1;
        c1x += pa[i].x * dc1;
        c1y += pa[i].y * dc1;

        c20 += it3 * dc2;
        c21 += dc1 * dc2;
        c22 += dc2 * dc2;
        c23 += t3 * dc2;
        c2x += pa[i].x * dc2;
        c2y += pa[i].y * dc2;
    }

    if (dir1.y == 0 && dir2.x == 0) {
        p1.x = (int)((c1x - c10 * p0.x - c12 * p3.x - c13 * p3.x) / c11 + 0.5);
        p2.y = (int)((c2y - c20 * p0.y - c21 * p0.y - c23 * p3.y) / c22 + 0.5);
    } else if (dir1.x == 0 && dir2.y == 0) {
        p2.x = (int)((c2x - c20 * p0.x - c21 * p0.x - c23 * p3.x) / c22 + 0.5);
        p1.y = (int)((c1y - c10 * p0.y - c12 * p3.y - c13 * p3.y) / c11 + 0.5);
    } else if (dir1.y == 0 && dir2.y == 0) {
        // cramer's rule
        double D = c11 * c22 - c12 * c21;
        p1.x = (int)(((c1x - c10 * p0.x - c13 * p3.x) * c22 - c12 * (c2x - c20 * p0.x - c23 * p3.x)) / D + 0.5);
        p2.x = (int)((c11 * (c2x - c20 * p0.x - c23 * p3.x) - (c1x - c10 * p0.x - c13 * p3.x) * c21) / D + 0.5);
    } else if (dir1.x == 0 && dir2.x == 0) {
        // cramer's rule
        double D = c11 * c22 - c12 * c21;
        p1.y = (int)(((c1y - c10 * p0.y - c13 * p3.y) * c22 - c12 * (c2y - c20 * p0.y - c23 * p3.y)) / D + 0.5);
        p2.y = (int)((c11 * (c2y - c20 * p0.y - c23 * p3.y) - (c1y - c10 * p0.y - c13 * p3.y) * c21) / D + 0.5);
    } else { // must not happen
        ASSERT(0);
        return false;
    }

    // check for "inside-out" beziers
    CPoint dir3 = p1 - p0, dir4 = p2 - p3;
    if ((dir1.x * dir3.x + dir1.y * dir3.y) <= 0 || (dir2.x * dir4.x + dir2.y * dir4.y) <= 0) {
        return false;
    }

    return true;
}

int CVobSubImage::GrabSegment(int _start, const COutline& o, COutline& ret)
{
    ret.RemoveAll();

    int len = int(o.pa.GetCount());

    int cur = (_start) % len, first = -1, last = -1;
    int lastDir = 0;

    for (ptrdiff_t i = 0; i < len; i++) {
        cur = (cur + 1) % len;

        if (o.da[cur] == 0) {
            continue;
        }

        if (first == -1) {
            first = cur;
        }

        if (lastDir == o.da[cur]) {
            CPoint startp = o.pa[first] + o.pa[_start];
            startp.x >>= 1;
            startp.y >>= 1;
            CPoint endp = o.pa[last] + o.pa[cur];
            endp.x >>= 1;
            endp.y >>= 1;

            if (first < _start) {
                first += len;
            }
            _start = ((_start + first) >> 1) + 1;
            if (_start >= len) {
                _start -= len;
            }
            if (cur < last) {
                cur += len;
            }
            cur = ((last + cur + 1) >> 1);
            if (cur >= len) {
                cur -= len;
            }

            ret.Add(startp, 0);

            while (_start != cur) {
                ret.Add(o.pa[_start], o.da[_start]);

                _start++;
                if (_start >= len) {
                    _start -= len;
                }
            }

            ret.Add(endp, 0);

            return last;
        }

        lastDir = o.da[cur];
        last = cur;
    }

    ASSERT(0);

    return _start;
}

void CVobSubImage::SplitOutline(const COutline& o, COutline& o1, COutline& o2)
{
    size_t len = o.pa.GetCount();
    if (len < 4) {
        return;
    }

    CAtlArray<UINT> la, sa, ea;

    size_t i, j, k;

    for (i = 0, j = 0; j < len; j++) {
        if (j + 1 == len || o.da[j]) {
            la.Add(unsigned int(j - i));
            sa.Add(unsigned int(i));
            ea.Add(unsigned int(j));
            i = j;
        }
    }

    size_t maxlen = 0, maxidx = SIZE_T_ERROR;
    size_t maxlen2 = 0, maxidx2 = SIZE_T_ERROR;

    for (i = 0; i < la.GetCount(); i++) {
        if (maxlen < la[i]) {
            maxlen = la[i];
            maxidx = i;
        }

        if (maxlen2 < la[i] && i > 0 && i < la.GetCount() - 1) {
            maxlen2 = la[i];
            maxidx2 = i;
        }
    }

    ASSERT(maxidx != SIZE_T_ERROR && maxidx2 != SIZE_T_ERROR);

    if (maxlen == maxlen2) {
        maxidx = maxidx2;    // if equal choose the inner section
    }

    j = (sa[maxidx] + ea[maxidx]) >> 1, k = (sa[maxidx] + ea[maxidx] + 1) >> 1;

    o1.RemoveAll();
    o2.RemoveAll();

    for (i = 0; i <= j; i++) {
        o1.Add(o.pa[i], o.da[i]);
    }

    if (j != k) {
        CPoint mid = o.pa[j] + o.pa[k];
        mid.x >>= 1;
        mid.y >>= 1;
        o1.Add(mid, 0);
        o2.Add(mid, 0);
    }

    for (i = k; i < len; i++) {
        o2.Add(o.pa[i], o.da[i]);
    }
}

void CVobSubImage::AddSegment(COutline& o, CAtlArray<BYTE>& pathTypes, CAtlArray<CPoint>& pathPoints)
{
    int i, len = int(o.pa.GetCount());
    if (len < 3) {
        return;
    }

    int nLeftTurns = 0, nRightTurns = 0;

    for (i = 0; i < len; i++) {
        if (o.da[i] == -1) {
            nLeftTurns++;
        } else if (o.da[i] == 1) {
            nRightTurns++;
        }
    }

    if (nLeftTurns == 0 && nRightTurns == 0) { // line
        pathTypes.Add(PT_LINETO);
        pathPoints.Add(o.pa[len - 1]);

        return;
    }

    if (nLeftTurns == 0 || nRightTurns == 0) { // b-spline
        pathTypes.Add(PT_MOVETONC);
        pathPoints.Add(o.pa[0] + (o.pa[0] - o.pa[1]));

        for (i = 0; i < 3; i++) {
            pathTypes.Add(PT_BSPLINETO);
            pathPoints.Add(o.pa[i]);
        }

        for (; i < len; i++) {
            pathTypes.Add(PT_BSPLINEPATCHTO);
            pathPoints.Add(o.pa[i]);
        }

        pathTypes.Add(PT_BSPLINEPATCHTO);
        pathPoints.Add(o.pa[len - 1] + (o.pa[len - 1] - o.pa[len - 2]));

        pathTypes.Add(PT_MOVETONC);
        pathPoints.Add(o.pa[len - 1]);

        return;
    }

    int _start, _end;
    if (FitLine(o, _start, _end)) { // b-spline, line, b-spline
        pathTypes.Add(PT_MOVETONC);
        pathPoints.Add(o.pa[0] + (o.pa[0] - o.pa[1]));

        pathTypes.Add(PT_BSPLINETO);
        pathPoints.Add(o.pa[0]);

        pathTypes.Add(PT_BSPLINETO);
        pathPoints.Add(o.pa[1]);

        CPoint p[4], pp, d = o.pa[_end] - o.pa[_start];
        double l = sqrt((double)(d.x * d.x + d.y * d.y)), dx = 1.0 * d.x / l, dy = 1.0 * d.y / l;

        pp = o.pa[_start] - o.pa[_start - 1];
        double l1 = abs(pp.x) + abs(pp.y);
        pp = o.pa[_end] - o.pa[_end + 1];
        double l2 = abs(pp.x) + abs(pp.y);
        p[0] = CPoint((int)(1.0 * o.pa[_start].x + dx * l1 + 0.5), (int)(1.0 * o.pa[_start].y + dy * l1 + 0.5));
        p[1] = CPoint((int)(1.0 * o.pa[_start].x + dx * l1 * 2 + 0.5), (int)(1.0 * o.pa[_start].y + dy * l1 * 2 + 0.5));
        p[2] = CPoint((int)(1.0 * o.pa[_end].x - dx * l2 * 2 + 0.5), (int)(1.0 * o.pa[_end].y - dy * l2 * 2 + 0.5));
        p[3] = CPoint((int)(1.0 * o.pa[_end].x - dx * l2 + 0.5), (int)(1.0 * o.pa[_end].y - dy * l2 + 0.5));

        if (_start == 1) {
            pathTypes.Add(PT_BSPLINETO);
            pathPoints.Add(p[0]);
        } else {
            pathTypes.Add(PT_BSPLINETO);
            pathPoints.Add(o.pa[2]);

            for (ptrdiff_t k = 3; k <= _start; k++) {
                pathTypes.Add(PT_BSPLINEPATCHTO);
                pathPoints.Add(o.pa[k]);
            }

            pathTypes.Add(PT_BSPLINEPATCHTO);
            pathPoints.Add(p[0]);
        }

        pathTypes.Add(PT_BSPLINEPATCHTO);
        pathPoints.Add(p[1]);

        pathTypes.Add(PT_MOVETONC);
        pathPoints.Add(p[0]);

        pathTypes.Add(PT_LINETO);
        pathPoints.Add(p[3]);

        pathTypes.Add(PT_MOVETONC);
        pathPoints.Add(p[2]);

        pathTypes.Add(PT_BSPLINEPATCHTO);
        pathPoints.Add(p[3]);

        for (i = _end; i < len; i++) {
            pathTypes.Add(PT_BSPLINEPATCHTO);
            pathPoints.Add(o.pa[i]);
        }

        pathTypes.Add(PT_BSPLINEPATCHTO);
        pathPoints.Add(o.pa[len - 1] + (o.pa[len - 1] - o.pa[len - 2]));

        pathTypes.Add(PT_MOVETONC);
        pathPoints.Add(o.pa[len - 1]);

        return;
    }

    CPoint p1, p2;
    if (FitBezierVH(o, p1, p2)) { // bezier
        pathTypes.Add(PT_BEZIERTO);
        pathPoints.Add(p1);
        pathTypes.Add(PT_BEZIERTO);
        pathPoints.Add(p2);
        pathTypes.Add(PT_BEZIERTO);
        pathPoints.Add(o.pa[o.pa.GetCount() - 1]);

        return;
    }

    COutline o1, o2;
    SplitOutline(o, o1, o2);
    AddSegment(o1, pathTypes, pathPoints);
    AddSegment(o2, pathTypes, pathPoints);
}

bool CVobSubImage::Polygonize(CAtlArray<BYTE>& pathTypes, CAtlArray<CPoint>& pathPoints, bool bSmooth, int scale)
{
    CPoint topleft;
    CAutoPtr<CAutoPtrList<COutline>> ol(GetOutlineList(topleft));
    if (!ol) {
        return false;
    }

    POSITION pos;

    pos = ol->GetHeadPosition();
    while (pos) {
        CAtlArray<CPoint>& pa = ol->GetNext(pos)->pa;
        for (size_t i = 0; i < pa.GetCount(); i++) {
            pa[i].x = (pa[i].x - topleft.x) << scale;
            pa[i].y = (pa[i].y - topleft.y) << scale;
        }
    }

    pos = ol->GetHeadPosition();
    while (pos) {
        COutline& o = *ol->GetNext(pos), o2;

        if (bSmooth) {
            int i = 0, iFirst = -1;

            for (;;) {
                i = GrabSegment(i, o, o2);

                if (i == iFirst) {
                    break;
                }

                if (iFirst < 0) {
                    iFirst = i;
                    pathTypes.Add(PT_MOVETO);
                    pathPoints.Add(o2.pa[0]);
                }

                AddSegment(o2, pathTypes, pathPoints);
            }
        } else {
            /*
                        for (ptrdiff_t i = 1, len = o.pa.GetSize(); i < len; i++)
                        {
                            if (int dir = o.da[i-1])
                            {
                                CPoint dir2 = o.pa[i] - o.pa[i-1];
                                dir2.x /= 2; dir2.y /= 2;
                                CPoint dir1 = dir > 0 ? CPoint(dir2.y, -dir2.x) : CPoint(-dir2.y, dir2.x);
                                i = i;
                                o.pa[i-1] -= dir1;
                                o.pa.InsertAt(i, o.pa[i-1] + dir2);
                                o.da.InsertAt(i, -dir);
                                o.pa.InsertAt(i+1, o.pa[i] + dir1);
                                o.da.InsertAt(i+1, dir);
                                i += 2;
                                len += 2;
                            }
                        }
            */
            pathTypes.Add(PT_MOVETO);
            pathPoints.Add(o.pa[0]);

            for (size_t i = 1, len = o.pa.GetCount(); i < len; i++) {
                pathTypes.Add(PT_LINETO);
                pathPoints.Add(o.pa[i]);
            }
        }
    }

    return !pathTypes.IsEmpty();
}

bool CVobSubImage::Polygonize(CStringW& assstr, bool bSmooth, int scale)
{
    CAtlArray<BYTE> pathTypes;
    CAtlArray<CPoint> pathPoints;

    if (!Polygonize(pathTypes, pathPoints, bSmooth, scale)) {
        return false;
    }

    assstr.Format(L"{\\an7\\pos(%d,%d)\\p%d}", rect.left, rect.top, 1 + scale);
    //  assstr.Format(L"{\\p%d}", 1+scale);

    BYTE lastType = 0;

    size_t nPoints = pathTypes.GetCount();

    for (size_t i = 0; i < nPoints; i++) {
        CStringW s;

        switch (pathTypes[i]) {
            case PT_MOVETO:
                if (lastType != PT_MOVETO) {
                    assstr += L"m ";
                }
                s.Format(L"%ld %ld ", pathPoints[i].x, pathPoints[i].y);
                break;
            case PT_MOVETONC:
                if (lastType != PT_MOVETONC) {
                    assstr += L"n ";
                }
                s.Format(L"%ld %ld ", pathPoints[i].x, pathPoints[i].y);
                break;
            case PT_LINETO:
                if (lastType != PT_LINETO) {
                    assstr += L"l ";
                }
                s.Format(L"%ld %ld ", pathPoints[i].x, pathPoints[i].y);
                break;
            case PT_BEZIERTO:
                if (i + 2 < nPoints) {
                    if (lastType != PT_BEZIERTO) {
                        assstr += L"b ";
                    }
                    s.Format(L"%ld %ld %ld %ld %ld %ld ", pathPoints[i].x, pathPoints[i].y, pathPoints[i + 1].x, pathPoints[i + 1].y, pathPoints[i + 2].x, pathPoints[i + 2].y);
                    i += 2;
                }
                break;
            case PT_BSPLINETO:
                if (i + 2 < nPoints) {
                    if (lastType != PT_BSPLINETO) {
                        assstr += L"s ";
                    }
                    s.Format(L"%ld %ld %ld %ld %ld %ld ", pathPoints[i].x, pathPoints[i].y, pathPoints[i + 1].x, pathPoints[i + 1].y, pathPoints[i + 2].x, pathPoints[i + 2].y);
                    i += 2;
                }
                break;
            case PT_BSPLINEPATCHTO:
                if (lastType != PT_BSPLINEPATCHTO) {
                    assstr += L"p ";
                }
                s.Format(L"%ld %ld ", pathPoints[i].x, pathPoints[i].y);
                break;
        }

        lastType = pathTypes[i];

        assstr += s;
    }

    assstr += L"{\\p0}";

    return nPoints > 0;
}

void CVobSubImage::Scale2x()
{
    int w = rect.Width(), h = rect.Height();

    if (w > 0 && h > 0) {
        DWORD* src = (DWORD*)lpPixels;
        DWORD* dst = DEBUG_NEW DWORD[w * h];

        for (int y = 0; y < h; y++) {
            for (int x = 0; x < w; x++, src++, dst++) {
                DWORD E = *src;

                DWORD A = x > 0 && y > 0 ? src[-w - 1] : E;
                DWORD B = y > 0 ? src[-w] : E;
                DWORD C = x < w - 1 && y > 0 ? src[-w + 1] : E;
                UNREFERENCED_PARAMETER(A);
                UNREFERENCED_PARAMETER(C);

                DWORD D = x > 0 ? src[-1] : E;
                DWORD F = x < w - 1 ? src[+1] : E;

                DWORD G = x > 0 && y < h - 1 ? src[+w - 1] : E;
                DWORD H = y < h - 1 ? src[+w] : E;
                DWORD I = x < w - 1 && y < h - 1 ? src[+w + 1] : E;
                UNREFERENCED_PARAMETER(G);
                UNREFERENCED_PARAMETER(I);

                DWORD E0 = D == B && B != F && D != H ? D : E;
                DWORD E1 = B == F && B != D && F != H ? F : E;
                DWORD E2 = D == H && D != B && H != F ? D : E;
                DWORD E3 = H == F && D != H && B != F ? F : E;

                *dst = ((((E0 & 0x00ff00ff) + (E1 & 0x00ff00ff) + (E2 & 0x00ff00ff) + (E3 & 0x00ff00ff) + 2) >> 2) & 0x00ff00ff)
                       | (((((E0 >> 8) & 0x00ff00ff) + ((E1 >> 8) & 0x00ff00ff) + ((E2 >> 8) & 0x00ff00ff) + ((E3 >> 8) & 0x00ff00ff) + 2) << 6) & 0xff00ff00);
            }
        }

        src -= w * h;
        dst -= w * h;

        memcpy(src, dst, w * h * 4);

        delete [] dst;
    }
}
