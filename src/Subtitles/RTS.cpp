/*
 * (C) 2003-2006 Gabest
 * (C) 2006-2022 see Authors.txt
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
#include <cmath>
#include <intrin.h>
#include "ColorConvTable.h"
#include "RTS.h"
#include "../DSUtil/PathUtils.h"
#include "../filters/renderer/VideoRenderers/RenderersSettings.h"
#include "moreuuids.h"

#if !TRACE_SUBTITLES
#undef TRACE
#define TRACE(...)
#endif

#define MAXGDIFONTSIZE 36000

// WARNING: this isn't very thread safe, use only one RTS a time. We should use TLS in future.
static HDC g_hDC;
static int g_hDC_refcnt = 0;

static long revcolor(long c)
{
    return ((c & 0xff0000) >> 16) + (c & 0xff00) + ((c & 0xff) << 16);
}

//////////////////////////////////////////////////////////////////////////////////////////////

void alpha_mask_deleter::operator()(CAlphaMask* ptr) const noexcept
{
    m_alphaMaskPool.emplace_front(std::move(*ptr));
    std::default_delete<CAlphaMask>()(ptr);
    if (m_alphaMaskPool.size() > 10) {
        m_alphaMaskPool.pop_back();
    }
}

// CMyFont

CMyFont::CMyFont(const STSStyle& style)
{
    LOGFONT lf;
    ZeroMemory(&lf, sizeof(lf));
    lf <<= style;
    lf.lfHeight = (LONG)(style.fontSize + 0.5);
    lf.lfOutPrecision = OUT_TT_PRECIS;
    lf.lfClipPrecision = CLIP_DEFAULT_PRECIS;
    lf.lfQuality = ANTIALIASED_QUALITY;
    lf.lfPitchAndFamily = DEFAULT_PITCH | FF_DONTCARE;
    if (lf.lfCharSet == 0)
        lf.lfCharSet = DEFAULT_CHARSET;

    if (!CreateFontIndirect(&lf)) {
        _tcscpy_s(lf.lfFaceName, _T("Calibri"));
        VERIFY(CreateFontIndirect(&lf));
    }

    HFONT hOldFont = SelectFont(g_hDC, *this);

#if 0
    WCHAR selectedFontName[LF_FACESIZE];
    GetTextFaceW(g_hDC, LF_FACESIZE, selectedFontName);
    if (wcsncmp(selectedFontName, lf.lfFaceName, LF_FACESIZE)) { //GDI chose a different font -- let's use default instead
        SelectFont(g_hDC, hOldFont);
        DeleteObject();
        _tcscpy_s(lf.lfFaceName, _T("Calibri"));
        VERIFY(CreateFontIndirect(&lf));
        HFONT hOldFont = SelectFont(g_hDC, *this);
    }
#endif

    TEXTMETRIC tm;
    GetTextMetrics(g_hDC, &tm);
    m_ascent = ((tm.tmAscent + 4) >> 3);
    m_descent = ((tm.tmDescent + 4) >> 3);
    SelectFont(g_hDC, hOldFont);
}

// CWord

CWord::CWord(const STSStyle& style, CStringW str, int ktype, int kstart, int kend, double scalex, double scaley,
             RenderingCaches& renderingCaches)
    : m_fDrawn(false)
    , m_p(INT_MAX, INT_MAX)
    , m_renderingCaches(renderingCaches)
    , m_scalex(scalex)
    , m_scaley(scaley)
    , m_str(str)
    , m_fWhiteSpaceChar(false)
    , m_fLineBreak(false)
    , m_style(style)
    , m_pOpaqueBox(nullptr)
    , m_ktype(ktype)
    , m_kstart(kstart)
    , m_kend(kend)
    , m_width(0)
    , m_ascent(0)
    , m_descent(0)
{
    if (str.IsEmpty()) {
        m_fWhiteSpaceChar = m_fLineBreak = true;
    }
    if (m_style.fontSize > MAXGDIFONTSIZE) {
        double fact = m_style.fontSize / MAXGDIFONTSIZE;
        m_style.fontSize = MAXGDIFONTSIZE;
        m_style.fontScaleX *= fact;
        m_style.fontScaleY *= fact;
    }

}


CWord::CWord(RenderingCaches& renderingCaches)
    : m_fDrawn(false)
    , m_p(INT_MAX, INT_MAX)
    , m_renderingCaches(renderingCaches)
    , m_scalex(0)
    , m_scaley(0)
    , m_str(L"")
    , m_fWhiteSpaceChar(false)
    , m_fLineBreak(false)
    , m_style()
    , m_pOpaqueBox(nullptr)
    , m_ktype(0)
    , m_kstart(0)
    , m_kend(0)
    , m_width(0)
    , m_ascent(0)
    , m_descent(0)
{
}


CWord::~CWord()
{
    delete m_pOpaqueBox;
}

bool CWord::Append(CWord* w)
{
    if (m_style != w->m_style
            || m_fLineBreak || w->m_fLineBreak
            || w->m_kstart != w->m_kend || m_ktype != w->m_ktype) {
        return false;
    }

    m_fWhiteSpaceChar = m_fWhiteSpaceChar && w->m_fWhiteSpaceChar;
    m_str += w->m_str;
    m_width += w->m_width;

    m_fDrawn = false;
    m_p = CPoint(INT_MAX, INT_MAX);

    return true;
}

void CWord::Paint(const CPoint& p, const CPoint& org)
{
    if (!m_str) {
        return;
    }

    COverlayKey overlayKey(this, p, org);

    if (m_renderingCaches.overlayCache.Lookup(overlayKey, m_pOverlayData)) {
        m_fDrawn = m_renderingCaches.outlineCache.Lookup(overlayKey, m_pOutlineData);
        if (m_style.borderStyle == 1) {
            if (m_style.outlineWidthX > 0.0 || m_style.shadowDepthX > 0.0 || m_style.outlineWidthY > 0.0 || m_style.shadowDepthY > 0.0) {
                VERIFY(CreateOpaqueBox());
            }
        }
    } else {
        if (!m_fDrawn) {
            if (m_renderingCaches.outlineCache.Lookup(overlayKey, m_pOutlineData)) {
                if (m_style.borderStyle == 1) {
                    if (m_style.outlineWidthX > 0.0 || m_style.shadowDepthX > 0.0 || m_style.outlineWidthY > 0.0 || m_style.shadowDepthY > 0.0) {
                        VERIFY(CreateOpaqueBox());
                    }
                }
            } else {
                if (!CreatePath()) {
                    return;
                }

                Transform(CPoint((org.x - p.x) * 8, (org.y - p.y) * 8));

                if (!ScanConvert()) {
                    return;
                }

                if (m_style.borderStyle == 0 && (m_style.outlineWidthX + m_style.outlineWidthY > 0)) {
                    int rx = std::max<int>(0, std::lround(m_style.outlineWidthX));
                    int ry = std::max<int>(0, std::lround(m_style.outlineWidthY));

                    if (!m_pEllipse || m_pEllipse->GetXRadius() != rx || m_pEllipse->GetYRadius() != ry) {
                        CEllipseKey ellipseKey(rx, ry);
                        if (!m_renderingCaches.ellipseCache.Lookup(ellipseKey, m_pEllipse)) {
                            m_pEllipse = std::make_shared<CEllipse>(rx, ry);

                            m_renderingCaches.ellipseCache.SetAt(ellipseKey, m_pEllipse);
                        }
                    }

                    if (!CreateWidenedRegion(rx, ry)) {
                        return;
                    }
                } else if (m_style.borderStyle == 1) {
                    if (m_style.outlineWidthX > 0.0 || m_style.shadowDepthX > 0.0 || m_style.outlineWidthY > 0.0 || m_style.shadowDepthY > 0.0) {
                        VERIFY(CreateOpaqueBox());
                    }
                }

                m_renderingCaches.outlineCache.SetAt(overlayKey, m_pOutlineData);
            }

            m_fDrawn = true;

            if (!Rasterize(p.x & 7, p.y & 7, m_style.fBlur, m_style.fGaussianBlur)) {
                return;
            }
            m_renderingCaches.overlayCache.SetAt(overlayKey, m_pOverlayData);
        } else if ((m_p.x & 7) != (p.x & 7) || (m_p.y & 7) != (p.y & 7)) {
            Rasterize(p.x & 7, p.y & 7, m_style.fBlur, m_style.fGaussianBlur);
            m_renderingCaches.overlayCache.SetAt(overlayKey, m_pOverlayData);
        }
    }

    m_p = p;

    if (m_pOpaqueBox) {
        m_pOpaqueBox->Paint(p, org);
    }
}

void CWord::Transform(CPoint org)
{
    if ((fabs(m_style.fontAngleX) > 0.000001) || (fabs(m_style.fontAngleY) > 0.000001) || (fabs(m_style.fontAngleZ) > 0.000001) ||
        (fabs(m_style.fontShiftX) > 0.000001) || (fabs(m_style.fontShiftY) > 0.000001)) {
        Transform_SSE2(org);
    } else if ((fabs(m_style.fontScaleX - 100) > 0.000001) || (fabs(m_style.fontScaleY - 100) > 0.000001)) {
        Transform_quick_SSE2(org);
    }
}

bool CWord::CreateOpaqueBox()
{
    if (m_pOpaqueBox) {
        return true;
    }

    STSStyle style = m_style;
    style.borderStyle = 0;

    // We don't want to apply the outline and the scaling twice
    style.outlineWidthX = style.outlineWidthY = 0.0;
    if (m_str.GetLength() > 2) {
        // some SSA subs use an opaque box to draw text backgrounds for translated signs
        // these use single character with a large fontscale
        // don't adjust scale in that case
        style.fontScaleX = style.fontScaleY = 100.0;
    }

    style.colors[0] = m_style.colors[2];
    style.alpha[0] = m_style.alpha[2];

    int w = std::lround(m_style.outlineWidthX);
    int h = std::lround(m_style.outlineWidthY);

    // Convert to pixels rounding to nearest
    CStringW str;
    str.Format(L"m %d %d l %d %d %d %d %d %d",
               -(w + 4) / 8, -(h + 4) / 8,
               (m_width + w + 4) / 8, -(h + 4) / 8,
               (m_width + w + 4) / 8, (m_ascent + m_descent + h + 4) / 8,
               -(w + 4) / 8, (m_ascent + m_descent + h + 4) / 8);

    try {
        m_pOpaqueBox = DEBUG_NEW CPolygon(style, str, 0, 0, 0, 1.0, 1.0, 0, m_renderingCaches);
    } catch (CMemoryException* e) {
        e->Delete();
        m_pOpaqueBox = nullptr;
    }

    return !!m_pOpaqueBox;
}

#if 0
void CWord::Transform_C(const CPoint& org)
{
    const double scalex = m_style.fontScaleX / 100.0;
    const double scaley = m_style.fontScaleY / 100.0;
    const double xzoomf = m_scalex * 20000.0;
    const double yzoomf = m_scaley * 20000.0;

    const double caz = cos((M_PI / 180.0) * m_style.fontAngleZ);
    const double saz = sin((M_PI / 180.0) * m_style.fontAngleZ);
    const double cax = cos((M_PI / 180.0) * m_style.fontAngleX);
    const double sax = sin((M_PI / 180.0) * m_style.fontAngleX);
    const double cay = cos((M_PI / 180.0) * m_style.fontAngleY);
    const double say = sin((M_PI / 180.0) * m_style.fontAngleY);

    double dOrgX = static_cast<double>(org.x), dOrgY = static_cast<double>(org.y);
    for (ptrdiff_t i = 0; i < mPathPoints; i++) {
        double x, y, z, xx, yy, zz;

        x = mpPathPoints[i].x;
        y = mpPathPoints[i].y;
        z = 0;

        const double dPPx = m_style.fontShiftX * y + x;
        y = scaley * (m_style.fontShiftY * x + y) - dOrgY;
        x = scalex * dPPx - dOrgX;

        xx = x * caz + y * saz;
        yy = -(x * saz - y * caz);
        zz = z;

        x = xx;
        y = yy * cax + zz * sax;
        z = yy * sax - zz * cax;

        xx = x * cay + z * say;
        yy = y;
        zz = x * say - z * cay;

        x = xx * xzoomf / std::max((zz + xzoomf), 1000.0);
        y = yy * yzoomf / std::max((zz + yzoomf), 1000.0);

        // round to integer
        mpPathPoints[i].x = std::lround(x) + org.x;
        mpPathPoints[i].y = std::lround(y) + org.y;
    }
}
#endif

#if 0
void CWord::Transform_quick_C(const CPoint& org)
{
    const double scalex = m_style.fontScaleX / 100.0;
    const double scaley = m_style.fontScaleY / 100.0;

    for (ptrdiff_t i = 0; i < mPathPoints; i++) {
        double x = scalex * mpPathPoints[i].x;
        double y = scaley * mpPathPoints[i].y;

        // round to integer
        mpPathPoints[i].x = std::lround(x);
        mpPathPoints[i].y = std::lround(y);
    }
}
#endif

void CWord::Transform_quick_SSE2(const CPoint& org)
{
    const __m128 __xscale = _mm_set_ps1((float)(m_style.fontScaleX / 100.0));
    const __m128 __yscale = _mm_set_ps1((float)(m_style.fontScaleY / 100.0));

    int mPathPointsD4 = mPathPoints / 4;
    int mPathPointsM4 = mPathPoints % 4;

    for (ptrdiff_t i = 0; i < mPathPointsD4 + 1; i++) {
        __m128 __pointx, __pointy;
        // we can't use load .-.
        if (i != mPathPointsD4) {
            __pointx = _mm_set_ps((float)mpPathPoints[4 * i + 0].x, (float)mpPathPoints[4 * i + 1].x, (float)mpPathPoints[4 * i + 2].x, (float)mpPathPoints[4 * i + 3].x);
            __pointy = _mm_set_ps((float)mpPathPoints[4 * i + 0].y, (float)mpPathPoints[4 * i + 1].y, (float)mpPathPoints[4 * i + 2].y, (float)mpPathPoints[4 * i + 3].y);
        } else { // last cycle
            switch (mPathPointsM4) {
            default:
            case 0:
                continue;
            case 1:
                __pointx = _mm_set_ps((float)mpPathPoints[4 * i + 0].x, 0, 0, 0);
                __pointy = _mm_set_ps((float)mpPathPoints[4 * i + 0].y, 0, 0, 0);
                break;
            case 2:
                __pointx = _mm_set_ps((float)mpPathPoints[4 * i + 0].x, (float)mpPathPoints[4 * i + 1].x, 0, 0);
                __pointy = _mm_set_ps((float)mpPathPoints[4 * i + 0].y, (float)mpPathPoints[4 * i + 1].y, 0, 0);
                break;
            case 3:
                __pointx = _mm_set_ps((float)mpPathPoints[4 * i + 0].x, (float)mpPathPoints[4 * i + 1].x, (float)mpPathPoints[4 * i + 2].x, 0);
                __pointy = _mm_set_ps((float)mpPathPoints[4 * i + 0].y, (float)mpPathPoints[4 * i + 1].y, (float)mpPathPoints[4 * i + 2].y, 0);
                break;
            }
        }

        // scale
        __pointx = _mm_mul_ps(__pointx, __xscale);
        __pointy = _mm_mul_ps(__pointy, __yscale);

        // round to integer
        __m128i __pointxRounded = _mm_cvtps_epi32(__pointx);
        __m128i __pointyRounded = _mm_cvtps_epi32(__pointy);

        if (i == mPathPointsD4) { // last cycle
            for (int k = 0; k < mPathPointsM4; k++) {
                mpPathPoints[i * 4 + k].x = __pointxRounded.m128i_i32[3 - k];
                mpPathPoints[i * 4 + k].y = __pointyRounded.m128i_i32[3 - k];
            }
        } else {
            for (int k = 0; k < 4; k++) {
                mpPathPoints[i * 4 + k].x = __pointxRounded.m128i_i32[3 - k];
                mpPathPoints[i * 4 + k].y = __pointyRounded.m128i_i32[3 - k];
            }
        }
    }
}

void CWord::Transform_SSE2(const CPoint& org)
{
    // SSE code
    // speed up ~1.5-1.7x
    const __m128 __xshift = _mm_set_ps1((float)m_style.fontShiftX);
    const __m128 __yshift = _mm_set_ps1((float)m_style.fontShiftY);

    const __m128 __xorg = _mm_set_ps1((float)org.x);
    const __m128 __yorg = _mm_set_ps1((float)org.y);

    const __m128 __xscale = _mm_set_ps1((float)(m_style.fontScaleX / 100.0));
    const __m128 __yscale = _mm_set_ps1((float)(m_style.fontScaleY / 100.0));
    const __m128 __xzoomf = _mm_set_ps1((float)(m_scalex * 20000.0));
    const __m128 __yzoomf = _mm_set_ps1((float)(m_scaley * 20000.0));

    const __m128 __caz = _mm_set_ps1((float)cos((M_PI / 180.0) * m_style.fontAngleZ));
    const __m128 __saz = _mm_set_ps1((float)sin((M_PI / 180.0) * m_style.fontAngleZ));
    const __m128 __cax = _mm_set_ps1((float)cos((M_PI / 180.0) * m_style.fontAngleX));
    const __m128 __sax = _mm_set_ps1((float)sin((M_PI / 180.0) * m_style.fontAngleX));
    const __m128 __cay = _mm_set_ps1((float)cos((M_PI / 180.0) * m_style.fontAngleY));
    const __m128 __say = _mm_set_ps1((float)sin((M_PI / 180.0) * m_style.fontAngleY));

    const __m128 __1000 = _mm_set_ps1(1000.0f);

    int mPathPointsD4 = mPathPoints / 4;
    int mPathPointsM4 = mPathPoints % 4;

    for (ptrdiff_t i = 0; i < mPathPointsD4 + 1; i++) {
        __m128 __pointx, __pointy, __tmpx, __tmpy;
        // we can't use load .-.
        if (i != mPathPointsD4) {
            __pointx = _mm_set_ps((float)mpPathPoints[4 * i + 0].x, (float)mpPathPoints[4 * i + 1].x, (float)mpPathPoints[4 * i + 2].x, (float)mpPathPoints[4 * i + 3].x);
            __pointy = _mm_set_ps((float)mpPathPoints[4 * i + 0].y, (float)mpPathPoints[4 * i + 1].y, (float)mpPathPoints[4 * i + 2].y, (float)mpPathPoints[4 * i + 3].y);
        } else { // last cycle
            switch (mPathPointsM4) {
                default:
                case 0:
                    continue;
                case 1:
                    __pointx = _mm_set_ps((float)mpPathPoints[4 * i + 0].x, 0, 0, 0);
                    __pointy = _mm_set_ps((float)mpPathPoints[4 * i + 0].y, 0, 0, 0);
                    break;
                case 2:
                    __pointx = _mm_set_ps((float)mpPathPoints[4 * i + 0].x, (float)mpPathPoints[4 * i + 1].x, 0, 0);
                    __pointy = _mm_set_ps((float)mpPathPoints[4 * i + 0].y, (float)mpPathPoints[4 * i + 1].y, 0, 0);
                    break;
                case 3:
                    __pointx = _mm_set_ps((float)mpPathPoints[4 * i + 0].x, (float)mpPathPoints[4 * i + 1].x, (float)mpPathPoints[4 * i + 2].x, 0);
                    __pointy = _mm_set_ps((float)mpPathPoints[4 * i + 0].y, (float)mpPathPoints[4 * i + 1].y, (float)mpPathPoints[4 * i + 2].y, 0);
                    break;
            }
        }

        // scale and shift
        __tmpy = __pointx; // save a copy for calculating __pointy later
        if (m_style.fontShiftX != 0) {
            __tmpx = _mm_mul_ps(__xshift, __pointy);
            __pointx = _mm_add_ps(__pointx, __tmpx);
        }
        __pointx = _mm_mul_ps(__pointx, __xscale);
        __pointx = _mm_sub_ps(__pointx, __xorg);

        if (m_style.fontShiftY != 0) {
            __tmpy = _mm_mul_ps(__yshift, __tmpy);   // __tmpy is a copy of __pointx here, because it may otherwise be modified
            __pointy = _mm_add_ps(__pointy, __tmpy);
        }
        __pointy = _mm_mul_ps(__pointy, __yscale);
        __pointy = _mm_sub_ps(__pointy, __yorg);

        // rotate

        __m128 __xx, __yy;
        __m128 __zz = _mm_setzero_ps();

        // xx = x * caz + y * saz
        __tmpx   = _mm_mul_ps(__pointx, __caz);      // x * caz
        __tmpy   = _mm_mul_ps(__pointy, __saz);      // y * saz
        __xx     = _mm_add_ps(__tmpx, __tmpy);       // xx = x * caz + y * saz;

        // yy = -(x * saz - y * caz)
        __tmpx   = _mm_mul_ps(__pointx, __saz);      // x * saz
        __tmpy   = _mm_mul_ps(__pointy, __caz);      // y * caz
        __yy     = _mm_sub_ps(__tmpy, __tmpx);       // yy = -(x * saz - y * caz) = y * caz - x * saz

        __pointx = __xx;                             // x = xx

        // y = yy * cax + zz * sax
        __tmpx   = _mm_mul_ps(__zz, __sax);          // zz * sax
        __tmpy   = _mm_mul_ps(__yy, __cax);          // yy * cax
        __pointy = _mm_add_ps(__tmpy, __tmpx);       // y = yy * cax + zz * sax

        // z = yy * sax - zz * cax
        __tmpx   = _mm_mul_ps(__zz, __cax);          // zz * cax
        __tmpy   = _mm_mul_ps(__yy, __sax);          // yy * sax
        __zz     = _mm_sub_ps(__tmpy, __tmpx);       // z = yy * sax - zz * cax

        // xx = x * cay + z * say
        __tmpx   = _mm_mul_ps(__pointx, __cay);      // x * cay
        __tmpy   = _mm_mul_ps(__zz, __say);          // z * say
        __xx     = _mm_add_ps(__tmpx, __tmpy);       // xx = x * cay + z * say

        __yy     = __pointy;                         // yy = y

        // zz = x * say - z * cay
        __tmpx   = _mm_mul_ps(__pointx, __say);      // x * say
        __zz     = _mm_mul_ps(__zz, __cay);          // z * cay
        __zz     = _mm_sub_ps(__tmpx, __zz);         // zz = x * say - z * cay

        // x = xx * xzoomf / std::max((zz + xzoomf), 1000.0);
        // y = yy * yzoomf / std::max((zz + yzoomf), 1000.0);
        __m128 __tmpzz = _mm_add_ps(__zz, __xzoomf); // zz + xzoomf

        __xx     = _mm_mul_ps(__xx, __xzoomf);       // xx * xzoomf
        __pointx = _mm_div_ps(__xx, _mm_max_ps(__tmpzz, __1000)); // x = (xx * xzoomf) / std::max((zz + xzoomf), 1000.0)

        __tmpzz  = _mm_add_ps(__zz, __yzoomf);       // zz + yzoomf

        __yy     = _mm_mul_ps(__yy, __yzoomf);       // yy * yzoomf
        __pointy = _mm_div_ps(__yy, _mm_max_ps(__tmpzz, __1000)); // y = yy * yzoomf / std::max((zz + yzoomf), 1000.0);

        __pointx = _mm_add_ps(__pointx, __xorg);     // x = x + org.x
        __pointy = _mm_add_ps(__pointy, __yorg);     // y = y + org.y

        // round to integer
        __m128i __pointxRounded = _mm_cvtps_epi32(__pointx);
        __m128i __pointyRounded = _mm_cvtps_epi32(__pointy);

        if (i == mPathPointsD4) { // last cycle
            for (int k = 0; k < mPathPointsM4; k++) {
                mpPathPoints[i * 4 + k].x = __pointxRounded.m128i_i32[3 - k];
                mpPathPoints[i * 4 + k].y = __pointyRounded.m128i_i32[3 - k];
            }
        } else {
            for (int k = 0; k < 4; k++) {
                mpPathPoints[i * 4 + k].x = __pointxRounded.m128i_i32[3 - k];
                mpPathPoints[i * 4 + k].y = __pointyRounded.m128i_i32[3 - k];
            }
        }
    }
}

// CText

CText::CText(const STSStyle& style, CStringW str, int ktype, int kstart, int kend, double scalex, double scaley,
             RenderingCaches& renderingCaches)
    : CWord(style, str, ktype, kstart, kend, scalex, scaley, renderingCaches)
{
    if (m_str == L" ") {
        m_fWhiteSpaceChar = true;
    }

    CTextDimsKey textDimsKey(m_str, m_style);
    CTextDims textDims;
    if (!renderingCaches.textDimsCache.Lookup(textDimsKey, textDims)) {
        CMyFont font(m_style);
        m_ascent  = font.m_ascent;
        m_descent = font.m_descent;

        HFONT hOldFont = SelectFont(g_hDC, font);

        if (m_style.fontSpacing) {
            for (LPCWSTR s = m_str; *s; s++) {
                CSize extent;
                if (!GetTextExtentPoint32W(g_hDC, s, 1, &extent)) {
                    SelectFont(g_hDC, hOldFont);
                    ASSERT(0);
                    return;
                }
                m_width += extent.cx + (int)m_style.fontSpacing;
            }
            // m_width -= (int)m_style.fontSpacing; // TODO: subtract only at the end of the line
        } else {
            CSize extent;
            if (!GetTextExtentPoint32W(g_hDC, m_str, str.GetLength(), &extent)) {
                SelectFont(g_hDC, hOldFont);
                ASSERT(0);
                return;
            }
            m_width += extent.cx;
        }

        SelectFont(g_hDC, hOldFont);

        textDims.ascent  = m_ascent;
        textDims.descent = m_descent;
        textDims.width   = m_width;

        renderingCaches.textDimsCache.SetAt(textDimsKey, textDims);
    } else {
        m_ascent  = textDims.ascent;
        m_descent = textDims.descent;
        m_width   = textDims.width;
    }

    m_ascent  = (int)(m_style.fontScaleY / 100 * m_ascent);
    m_descent = (int)(m_style.fontScaleY / 100 * m_descent);
    m_width   = (int)(m_style.fontScaleX / 100 * m_width + 4) >> 3;
}

//null constructor for use by CLineBG
CText::CText(RenderingCaches& renderingCaches)
    :CWord(renderingCaches)
    , m_RTS(nullptr) {
}

CWord* CText::Copy()
{
    return DEBUG_NEW CText(*this);
}

bool CText::Append(CWord* w)
{
    return (dynamic_cast<CText*>(w) && CWord::Append(w));
}

bool CText::CreatePath()
{
    CMyFont font(m_style);

    HFONT hOldFont = SelectFont(g_hDC, font);

    LONG cx = 0;
    auto getExtent = [&](LPCWSTR s, int len) {
        CSize extent;
        if (!GetTextExtentPoint32W(g_hDC, s, len, &extent)) {
            SelectFont(g_hDC, hOldFont);
            ASSERT(0);
            return false;
        }
        cx = extent.cx;
        return true;
    };

    bool useFreetypePath = false;
    std::wstring fontNameFT;
    CStringA langHint = "";
    if (m_RTS) {
        langHint = m_RTS->openTypeLangHint;
        useFreetypePath = m_RTS->GetUseFreeType();
        if (useFreetypePath) {
            fontNameFT = CW2W(m_style.fontName);
            fontNameFT += std::to_wstring(m_style.fontSize);
        }
    }

    if (m_style.fontSpacing) {
        int width = 0;
        bool bFirstPath = true;

        if (!useFreetypePath) {
            for (LPCWSTR s = m_str; *s; s++) {
                if (!getExtent(s, 1)) {
                    return false;
                }
                if (cx == 0) {
                    // possible unhandled unprintable character
                    ASSERT(*s == L'\x202a' || *s == L'\x202b');
                    continue;
                }
                PartialBeginPath(g_hDC, bFirstPath);
                bFirstPath = false;
                TextOutW(g_hDC, 0, 0, s, 1);
                int mp = mPathPoints;
                PartialEndPath(g_hDC, width, 0);
                if (mp == mPathPoints && !CStringW::StrTraits::IsSpace(s[0]) && m_RTS) { //failed to add points, we will try again with FreeType as emulator
                    useFreetypePath = true;
                    fontNameFT = CW2W(m_style.fontName);
                    fontNameFT += std::to_wstring(m_style.fontSize);
                    break;
                }
#if 0
                GetPathFreeType(g_hDC, false, s[0], m_style.fontSize, width+ cx + (int)m_style.fontSpacing, 0);
                GetPathFreeType(g_hDC, false, s[0], m_style.fontSize, width, m_style.fontSize*2);
#endif

                width += cx + (int)m_style.fontSpacing;
            }
        }
        if (useFreetypePath) { //try freetype
            int ftWidth = 0;
            bFirstPath = true;
            m_RTS->m_ftLibrary.LoadCodeFaceData(g_hDC, fontNameFT);
            if (!langHint.IsEmpty()) {
                m_RTS->m_ftLibrary.LoadCodePoints(m_str, fontNameFT, langHint);
            }
            for (LPCWSTR s = m_str; *s; s++) {
                if (!getExtent(s, 1)) {
                    return false;
                }
                if (cx == 0) {
                    // possible unhandled unprintable character
                    ASSERT(false);
                    continue;
                }

                if (!GetPathFreeType(g_hDC, bFirstPath, fontNameFT, s[0], ftWidth, 0, langHint, &m_RTS->m_ftLibrary)) {
                    break;
                }
                bFirstPath = false;
                ftWidth += cx + (int)m_style.fontSpacing;
            }
        }
    } else {
        int strlen = m_str.GetLength();

        if (!getExtent(m_str, strlen) || cx == 0) {
            // possible unhandled unprintable character
            ASSERT(false);
            return false;
        }

        if (!useFreetypePath) {
            BeginPath(g_hDC);
            TextOutW(g_hDC, 0, 0, m_str, strlen);
            EndPath(g_hDC);


            if (mPathPoints == 0 && m_RTS && strlen > 0) { // try freetype as fallback
                useFreetypePath = true;
                fontNameFT = CW2W(m_style.fontName);
                fontNameFT += std::to_wstring(m_style.fontSize);
            }
        }

        if (useFreetypePath && strlen > 0) {
            int ftWidth = 0;
            bool bFirstPath = true;
            m_RTS->m_ftLibrary.LoadCodeFaceData(g_hDC, fontNameFT);
            if (!langHint.IsEmpty()) {
                m_RTS->m_ftLibrary.LoadCodePoints(m_str, fontNameFT, langHint);
            }
            for (LPCWSTR s = m_str; *s; s++) {
                if (!getExtent(s, 1)) {
                    return false;
                }
                if (!GetPathFreeType(g_hDC, bFirstPath, fontNameFT, s[0], ftWidth, 0, langHint, &m_RTS->m_ftLibrary)) {
                    break;
                }
                bFirstPath = false;
                ftWidth += cx;
            }
        }
    }

    SelectFont(g_hDC, hOldFont);

    return true;
}


CLineBG::CLineBG(RenderingCaches& renderingCaches)
    : CText(renderingCaches) {
}

std::shared_ptr<CLineBG> CLineBG::CLineBGFactory(CLine const* line, RenderingCaches& renderingCaches) {
    if (!line || line->GetCount() < 2) { //single word does not have need of a combining algorithm
        return nullptr;
    }


    std::shared_ptr<CLineBG> lineBG = std::make_shared<CLineBG>(renderingCaches);

    bool first = true;
    POSITION pos = line->GetHeadPosition();
    while (pos) {
        CWord* w = line->GetNext(pos);
        if (first) {
            if (w->m_style.borderStyle == 0) {
                return nullptr;
            }

            lineBG->m_scalex = w->m_scalex;
            lineBG->m_scaley = w->m_scaley;
            lineBG->m_str = w->m_str;
            lineBG->m_style = w->m_style;
            lineBG->m_ktype = w->m_ktype;
            lineBG->m_kstart = w->m_kstart;
            lineBG->m_kend = w->m_kend;
            lineBG->m_width = w->m_width;
            lineBG->m_ascent = w->m_ascent;
            lineBG->m_descent = w->m_descent;
            first = false;
        } else if (lineBG->m_scalex != w->m_scalex ||
            lineBG->m_scaley != w->m_scaley ||
            lineBG->m_ktype != w->m_ktype ||
            lineBG->m_kstart != w->m_kstart ||
            lineBG->m_kend != w->m_kend ||
            lineBG->m_ascent != w->m_ascent ||
            lineBG->m_descent != w->m_descent) {
            return nullptr; //to combine words into one line, we currently restrict it to words who have identical properties
        } else {
            //check style
            STSStyle s1(lineBG->m_style);
            STSStyle s2(w->m_style);
            //everything in style must match except these properties
            s2.fontWeight = s1.fontWeight;
            s2.fItalic = s1.fItalic;
            s2.fUnderline = s1.fUnderline;
            s2.fStrikeOut = s1.fStrikeOut;

            if (s2 != s1) {
                return nullptr;
            }
            lineBG->m_width += w->m_width;
        }
    }
    return lineBG;
}

//see CLine::PaintShadow
CRect CLineBG::PaintLineShadow(SubPicDesc& spd, CRect& clipRect, BYTE* pAlphaMask, CPoint p, CPoint org, int time, int alpha) {
    CRect bbox(0, 0, 0, 0);

    if (m_style.shadowDepthX != 0 || m_style.shadowDepthY != 0) {
        int x = p.x + (int)(m_style.shadowDepthX + 0.5);
        int y = p.y + m_ascent - m_ascent + (int)(m_style.shadowDepthY + 0.5);

        DWORD a = 0xff - m_style.alpha[3];
        if (alpha > 0) {
            a = a * (0xff - static_cast<DWORD>(alpha)) / 0xff;
        }
        COLORREF shadow = revcolor(m_style.colors[3]) | (a << 24);
        DWORD sw[6] = { shadow, DWORD_MAX };
        sw[0] = ColorConvTable::ColorCorrection(sw[0]);

        Paint(CPoint(x, y), org);

        if (m_style.borderStyle == 1 && m_pOpaqueBox) {
            bbox |= m_pOpaqueBox->Draw(spd, clipRect, pAlphaMask, x, y, sw, true, false);
        }
    }

    return bbox;
}


//see CLine::PaintOutline
CRect CLineBG::PaintLineOutline(SubPicDesc& spd, CRect& clipRect, BYTE* pAlphaMask, CPoint p, CPoint org, int time, int alpha) {
    CRect bbox(0, 0, 0, 0);

    bool has_outline = m_style.outlineWidthX + m_style.outlineWidthY > 0.0;
    if ((has_outline || m_style.borderStyle == 1) && !(m_ktype == 2 && time < m_kstart)) {
        int x = p.x;
        int y = p.y + m_ascent - m_ascent;
        DWORD aoutline = m_style.alpha[2];
        if (alpha > 0) {
            aoutline += alpha * (0xff - m_style.alpha[2]) / 0xff;
        }
        COLORREF outline = revcolor(has_outline ? m_style.colors[2] : m_style.colors[3]) | ((0xff - aoutline) << 24);
        DWORD sw[6] = { outline, DWORD_MAX };
        sw[0] = ColorConvTable::ColorCorrection(sw[0]);

        Paint(CPoint(x, y), org);
        if (m_style.borderStyle == 1 && m_pOpaqueBox) {
            bbox |= m_pOpaqueBox->Draw(spd, clipRect, pAlphaMask, x, y, sw, true, false);
        }
    }

    return bbox;
}

// CPolygon

CPolygon::CPolygon(const STSStyle& style, CStringW str, int ktype, int kstart, int kend, double scalex, double scaley, int baseline,
                   RenderingCaches& renderingCaches)
    : CWord(style, str, ktype, kstart, kend, scalex, scaley, renderingCaches)
    , m_baseline(baseline)
{
    ParseStr();
}

CPolygon::CPolygon(CPolygon& src)
    : CWord(src.m_style, src.m_str, src.m_ktype, src.m_kstart, src.m_kend, src.m_scalex, src.m_scaley, src.m_renderingCaches)
    , m_baseline(src.m_baseline)
    , m_pPolygonPath(src.m_pPolygonPath)
{
    m_width = src.m_width;
    m_ascent = src.m_ascent;
    m_descent = src.m_descent;
}

CPolygon::~CPolygon()
{
}

CWord* CPolygon::Copy()
{
    return (DEBUG_NEW CPolygon(*this));
}

bool CPolygon::Append(CWord* w)
{
    CPolygon* p = dynamic_cast<CPolygon*>(w);
    if (!p) {
        return false;
    }

    // TODO
    return false;

    //return true;
}

bool CPolygon::GetPOINT(LPCWSTR& str, POINT& point) const
{
    LPWSTR xEnd = nullptr;
    LPWSTR yEnd = nullptr;

    point.x = std::lround(wcstod(str, &xEnd) * m_scalex) * 64;
    if (xEnd <= str) {
        return false;
    }
    point.y = std::lround(wcstod(xEnd, &yEnd) * m_scaley) * 64;

    bool ret = yEnd > xEnd;
    str = yEnd;

    return ret;
}

bool CPolygon::ParseStr()
{
    if (m_pPolygonPath && !m_pPolygonPath->typesOrg.IsEmpty()) {
        return true;
    }

    CPolygonPathKey polygonPathKey(m_str, m_scalex, m_scaley);
    if (!m_renderingCaches.polygonCache.Lookup(polygonPathKey, m_pPolygonPath)) {
        m_pPolygonPath = std::make_shared<CPolygonPath>();
        CPoint p;
        bool bFoundMove = false;
        size_t i, j, lastSplineStart = SIZE_T_ERROR;

        auto isValidAction = [](const WCHAR c) {
            return c == L'm' || c == L'n' || c == L'l' || c == L'b'
                   || c == L's' || c == L'p' || c == L'c';
        };

        for (LPCWSTR str = m_str; *str;) {
            // Trim any leading invalid characters and whitespace
            while (*str && !isValidAction(*str)) {
                str++;
            }
            const WCHAR c = *str;
            if (*str) {
                do {
                    str++;
                } while (isValidAction(*str));
            }
            switch (c) {
                case L'm':
                    if (!bFoundMove) {
                        if (m_pPolygonPath->typesOrg.GetCount() > 0) {
                            // move command not first so we abort
                            m_pPolygonPath = nullptr;
                            return false;
                        }
                        bFoundMove = true;
                    }
                    while (GetPOINT(str, p)) {
                        m_pPolygonPath->typesOrg.Add(PT_MOVETO);
                        m_pPolygonPath->pointsOrg.Add(p);
                    }
                    break;
                case L'n':
                    while (GetPOINT(str, p)) {
                        m_pPolygonPath->typesOrg.Add(PT_MOVETONC);
                        m_pPolygonPath->pointsOrg.Add(p);
                    }
                    break;
                case L'l':
                    if (m_pPolygonPath->pointsOrg.GetCount() < 1) {
                        break;
                    }
                    while (GetPOINT(str, p)) {
                        m_pPolygonPath->typesOrg.Add(PT_LINETO);
                        m_pPolygonPath->pointsOrg.Add(p);
                    }
                    break;
                case L'b':
                    j = m_pPolygonPath->typesOrg.GetCount();
                    if (j < 1) {
                        break;
                    }
                    while (GetPOINT(str, p)) {
                        m_pPolygonPath->typesOrg.Add(PT_BEZIERTO);
                        m_pPolygonPath->pointsOrg.Add(p);
                        ++j;
                    }
                    j = m_pPolygonPath->typesOrg.GetCount() - ((m_pPolygonPath->typesOrg.GetCount() - j) % 3);
                    m_pPolygonPath->typesOrg.SetCount(j);
                    m_pPolygonPath->pointsOrg.SetCount(j);
                    break;
                case L's':
                    if (m_pPolygonPath->pointsOrg.GetCount() < 1) {
                        break;
                    }
                    j = lastSplineStart = m_pPolygonPath->typesOrg.GetCount();
                    i = 3;
                    while (i-- && GetPOINT(str, p)) {
                        m_pPolygonPath->typesOrg.Add(PT_BSPLINETO);
                        m_pPolygonPath->pointsOrg.Add(p);
                        ++j;
                    }
                    if (m_pPolygonPath->typesOrg.GetCount() - lastSplineStart < 3) {
                        m_pPolygonPath->typesOrg.SetCount(lastSplineStart);
                        m_pPolygonPath->pointsOrg.SetCount(lastSplineStart);
                        lastSplineStart = SIZE_T_ERROR;
                    }
                // no break
                case L'p':
                    if (m_pPolygonPath->pointsOrg.GetCount() < 3) {
                        break;
                    }
                    while (GetPOINT(str, p)) {
                        m_pPolygonPath->typesOrg.Add(PT_BSPLINEPATCHTO);
                        m_pPolygonPath->pointsOrg.Add(p);
                    }
                    break;
                case L'c':
                    if (lastSplineStart != SIZE_T_ERROR && lastSplineStart > 0) {
                        m_pPolygonPath->typesOrg.Add(PT_BSPLINEPATCHTO);
                        m_pPolygonPath->typesOrg.Add(PT_BSPLINEPATCHTO);
                        m_pPolygonPath->typesOrg.Add(PT_BSPLINEPATCHTO);
                        p = m_pPolygonPath->pointsOrg[lastSplineStart - 1]; // we need p for temp storage, because operator [] will return a reference to CPoint and Add() may reallocate its internal buffer (this is true for MFC 7.0 but not for 6.0, hehe)
                        m_pPolygonPath->pointsOrg.Add(p);
                        p = m_pPolygonPath->pointsOrg[lastSplineStart];
                        m_pPolygonPath->pointsOrg.Add(p);
                        p = m_pPolygonPath->pointsOrg[lastSplineStart + 1];
                        m_pPolygonPath->pointsOrg.Add(p);
                        lastSplineStart = SIZE_T_ERROR;
                    }
                    break;
                default:
                    break;
            }
        }

        if (!bFoundMove) {
            // move command not found so we abort
            m_pPolygonPath = nullptr;
            return false;
        }

        int minx = INT_MAX, miny = INT_MAX, maxx = INT_MIN, maxy = INT_MIN;

        for (size_t m = 0; m < m_pPolygonPath->typesOrg.GetCount(); m++) {
            if (minx > m_pPolygonPath->pointsOrg[m].x) {
                minx = m_pPolygonPath->pointsOrg[m].x;
            }
            if (miny > m_pPolygonPath->pointsOrg[m].y) {
                miny = m_pPolygonPath->pointsOrg[m].y;
            }
            if (maxx < m_pPolygonPath->pointsOrg[m].x) {
                maxx = m_pPolygonPath->pointsOrg[m].x;
            }
            if (maxy < m_pPolygonPath->pointsOrg[m].y) {
                maxy = m_pPolygonPath->pointsOrg[m].y;
            }
        }

        m_pPolygonPath->size.SetSize(std::max(maxx - minx, 0), std::max(maxy - miny, 0));

        m_renderingCaches.polygonCache.SetAt(polygonPathKey, m_pPolygonPath);
    }

    m_width = m_pPolygonPath->size.cx;
    m_ascent = m_pPolygonPath->size.cy;

    int baseline = std::lround(m_scaley * m_baseline) * 64;
    m_descent = baseline;
    m_ascent -= baseline;

    m_width = ((int)(m_style.fontScaleX / 100.0 * m_width) + 4) >> 3;
    m_ascent = ((int)(m_style.fontScaleY / 100.0 * m_ascent) + 4) >> 3;
    m_descent = ((int)(m_style.fontScaleY / 100.0 * m_descent) + 4) >> 3;

    return true;
}

bool CPolygon::CreatePath()
{
    int len = m_pPolygonPath ? (int)m_pPolygonPath->typesOrg.GetCount() : 0;
    if (len == 0) {
        return false;
    }

    if (mPathPoints != len) {
        BYTE* pNewPathTypes = (BYTE*)realloc(mpPathTypes, len * sizeof(BYTE));
        if (!pNewPathTypes) {
            return false;
        }
        mpPathTypes = pNewPathTypes;
        POINT* pNewPathPoints = (POINT*)realloc(mpPathPoints, len * sizeof(POINT));
        if (!pNewPathPoints) {
            return false;
        }
        mpPathPoints = pNewPathPoints;
        mPathPoints = len;
    }

    memcpy(mpPathTypes, m_pPolygonPath->typesOrg.GetData(), len * sizeof(BYTE));
    memcpy(mpPathPoints, m_pPolygonPath->pointsOrg.GetData(), len * sizeof(POINT));

    return true;
}

// CClipper

CClipper::CClipper(CStringW str, const CSize& size, double scalex, double scaley, bool inverse, const CPoint& cpOffset,
                   RenderingCaches& renderingCaches)
    : CPolygon(STSStyle(), str, 0, 0, 0, scalex, scaley, 0, renderingCaches)
    , m_size(size)
    , m_inverse(inverse)
    , m_cpOffset(cpOffset)
    , m_pAlphaMask(nullptr)
    , m_effectType(-1)
{
}

CAlphaMaskSharedPtr CClipper::GetAlphaMask(const std::shared_ptr<CClipper>& clipper)
{
    if (m_pAlphaMask) {
        return m_pAlphaMask;
    }

    ASSERT(this == clipper.get());
    if (m_size.cx <= 0 || m_size.cy <= 0) {
        return nullptr;
    }

    CClipperKey key(clipper);
    if (m_renderingCaches.alphaMaskCache.Lookup(key, m_pAlphaMask)) {
        return m_pAlphaMask;
    }

    Paint(CPoint(0, 0), CPoint(0, 0));

    if (!m_pOverlayData) {
        return nullptr;
    }

    int w = m_pOverlayData->mOverlayWidth, h = m_pOverlayData->mOverlayHeight;

    int x = (m_pOverlayData->mOffsetX + m_cpOffset.x + 4) >> 3, y = (m_pOverlayData->mOffsetY + m_cpOffset.y + 4) >> 3;
    int xo = 0, yo = 0;

    if (x < 0) {
        xo = -x;
        w -= -x;
        x = 0;
    }
    if (y < 0) {
        yo = -y;
        h -= -y;
        y = 0;
    }
    if (x + w > m_size.cx) {
        w = m_size.cx - x;
    }
    if (y + h > m_size.cy) {
        h = m_size.cy - y;
    }

    if (w <= 0 || h <= 0) {
        return nullptr;
    }

    const size_t alphaMaskSize = size_t(m_size.cx) * m_size.cy;

    try {
        m_pAlphaMask = CAlphaMask::Alloc(m_renderingCaches.alphaMaskPool, alphaMaskSize);
    } catch (CMemoryException* e) {
        e->Delete();
        m_pAlphaMask = nullptr;
        return nullptr;
    }

    BYTE* pAlphaMask = m_pAlphaMask->get();
    memset(pAlphaMask, (m_inverse ? 0x40 : 0), alphaMaskSize);

    const BYTE* src = m_pOverlayData->mpOverlayBufferBody + m_pOverlayData->mOverlayPitch * yo + xo;
    BYTE* dst = pAlphaMask + m_size.cx * y + x;

    if (m_inverse) {
        for (ptrdiff_t i = 0; i < h; ++i) {
            for (ptrdiff_t wt = 0; wt < w; ++wt) {
                dst[wt] = 0x40 - src[wt];
            }
            src += m_pOverlayData->mOverlayPitch;
            dst += m_size.cx;
        }
    } else {
        for (ptrdiff_t i = 0; i < h; ++i) {
            memcpy(dst, src, w * sizeof(BYTE));
            src += m_pOverlayData->mOverlayPitch;
            dst += m_size.cx;
        }
    }

    if (m_effectType == EF_SCROLL) {
        int height = m_effect.param[4];
        int spd_w = m_size.cx, spd_h = m_size.cy;
        int da = (64 << 8) / height;
        int a = 0;
        int k = m_effect.param[0] >> 3;
        int l = k + height;
        if (k < 0) {
            a += -k * da;
            k = 0;
        }
        if (l > spd_h) {
            l = spd_h;
        }

        if (k < spd_h) {
            BYTE* am = &pAlphaMask[k * spd_w];

            ZeroMemory(pAlphaMask, am - pAlphaMask);

            for (ptrdiff_t j = k; j < l; j++, a += da) {
                for (ptrdiff_t i = 0; i < spd_w; i++, am++) {
                    *am = BYTE(((*am) * a) >> 14);
                }
            }
        }

        da = -(64 << 8) / height;
        a = 0x40 << 8;
        l = m_effect.param[1] >> 3;
        k = l - height;
        if (k < 0) {
            a += -k * da;
            k = 0;
        }
        if (l > spd_h) {
            l = spd_h;
        }

        if (k < spd_h) {
            BYTE* am = &pAlphaMask[k * spd_w];

            int j = k;
            for (; j < l; j++, a += da) {
                for (ptrdiff_t i = 0; i < spd_w; i++, am++) {
                    *am = BYTE(((*am) * a) >> 14);
                }
            }

            ZeroMemory(am, (spd_h - j)*spd_w);
        }
    } else if (m_effectType == EF_BANNER)  {
        int width = m_effect.param[2];
        int spd_w = m_size.cx, spd_h = m_size.cy;
        int da = (64 << 8) / width;
        BYTE* am = pAlphaMask;

        for (ptrdiff_t j = 0; j < spd_h; j++, am += spd_w) {
            int a = 0;
            int k = std::min(width, spd_w);

            for (ptrdiff_t i = 0; i < k; i++, a += da) {
                am[i] = BYTE((am[i] * a) >> 14);
            }

            a = 0x40 << 8;
            k = spd_w - width;

            if (k < 0) {
                a -= -k * da;
                k = 0;
            }

            for (ptrdiff_t i = k; i < spd_w; i++, a -= da) {
                am[i] = BYTE((am[i] * a) >> 14);
            }
        }

    }
    m_renderingCaches.alphaMaskCache.SetAt(key, m_pAlphaMask);
    return m_pAlphaMask;
}

CWord* CClipper::Copy()
{
    return DEBUG_NEW CClipper(m_str, m_size, m_scalex, m_scaley, m_inverse, m_cpOffset, m_renderingCaches);
}

bool CClipper::Append(CWord* w)
{
    return false;
}

// CLine

CLine::~CLine()
{
    POSITION pos = GetHeadPosition();
    while (pos) {
        delete GetNext(pos);
    }
}

void CLine::Compact()
{
    POSITION pos = GetHeadPosition();
    while (pos) {
        CWord* w = GetNext(pos);
        if (!w->m_fWhiteSpaceChar) {
            break;
        }

        m_width -= w->m_width;
        delete w;
        RemoveHead();
    }

    pos = GetTailPosition();
    while (pos) {
        CWord* w = GetPrev(pos);
        if (!w->m_fWhiteSpaceChar) {
            break;
        }

        m_width -= w->m_width;
        delete w;
        RemoveTail();
    }

    if (IsEmpty()) {
        return;
    }

    CLine l;
    l.AddTailList(this);
    RemoveAll();

    CWord* last = nullptr;

    pos = l.GetHeadPosition();
    while (pos) {
        CWord* w = l.GetNext(pos);

        if (!last || !last->Append(w)) {
            AddTail(last = w->Copy());
        }
    }

    m_ascent = m_descent = m_borderX = m_borderY = 0;

    pos = GetHeadPosition();
    while (pos) {
        CWord* w = GetNext(pos);

        if (m_ascent < w->m_ascent) {
            m_ascent = w->m_ascent;
        }
        if (m_descent < w->m_descent) {
            m_descent = w->m_descent;
        }
        if (m_borderX < w->m_style.outlineWidthX) {
            m_borderX = (int)(w->m_style.outlineWidthX + 0.5);
        }
        if (m_borderY < w->m_style.outlineWidthY) {
            m_borderY = (int)(w->m_style.outlineWidthY + 0.5);
        }
    }
}

//note that CLineBG::PaintLineShadow is derived from this code and should be updated if this is changed
CRect CLine::PaintShadow(SubPicDesc& spd, CRect& clipRect, BYTE* pAlphaMask, CPoint p, CPoint org, int time, int alpha)
{
    CRect bbox(0, 0, 0, 0);

    POSITION pos = GetHeadPosition();
    while (pos) {
        CWord* w = GetNext(pos);

        if (w->m_fLineBreak) {
            return bbox;    // should not happen since this class is just a line of text without any breaks
        }

        if (w->m_style.shadowDepthX != 0 || w->m_style.shadowDepthY != 0) {
            int x = p.x + (int)(w->m_style.shadowDepthX + 0.5);
            int y = p.y + m_ascent - w->m_ascent + (int)(w->m_style.shadowDepthY + 0.5);

            DWORD a = 0xff - w->m_style.alpha[3];
            if (alpha > 0) {
                a = a * (0xff - static_cast<DWORD>(alpha)) / 0xff;
            }
            COLORREF shadow = revcolor(w->m_style.colors[3]) | (a << 24);
            DWORD sw[6] = {shadow, DWORD_MAX};
            sw[0] = ColorConvTable::ColorCorrection(sw[0]);

            w->Paint(CPoint(x, y), org);

            if (w->m_style.borderStyle == 0) {
                bbox |= w->Draw(spd, clipRect, pAlphaMask, x, y, sw,
                                w->m_ktype > 0 || w->m_style.alpha[0] < 0xff,
                                (w->m_style.outlineWidthX + w->m_style.outlineWidthY > 0) && !(w->m_ktype == 2 && time < w->m_kstart));
            } else if (w->m_style.borderStyle == 1 && w->m_pOpaqueBox) {
                bbox |= w->m_pOpaqueBox->Draw(spd, clipRect, pAlphaMask, x, y, sw, true, false);
            }
        }

        p.x += w->m_width;
    }

    return bbox;
}

//note that CLineBG::PaintLineOutline is derived from this code and should be updated if this is changed
CRect CLine::PaintOutline(SubPicDesc& spd, CRect& clipRect, BYTE* pAlphaMask, CPoint p, CPoint org, int time, int alpha)
{
    CRect bbox(0, 0, 0, 0);

    POSITION pos = GetHeadPosition();
    while (pos) {
        CWord* w = GetNext(pos);

        if (w->m_fLineBreak) {
            return bbox;    // should not happen since this class is just a line of text without any breaks
        }

        bool has_outline = w->m_style.outlineWidthX + w->m_style.outlineWidthY > 0.0;
        if ((has_outline || w->m_style.borderStyle == 1) && !(w->m_ktype == 2 && time < w->m_kstart)) {
            int x = p.x;
            int y = p.y + m_ascent - w->m_ascent;
            DWORD aoutline = w->m_style.alpha[2];
            if (alpha > 0) {
                aoutline += alpha * (0xff - w->m_style.alpha[2]) / 0xff;
            }
            COLORREF outline = revcolor(has_outline ? w->m_style.colors[2] : w->m_style.colors[3]) | ((0xff - aoutline) << 24);
            DWORD sw[6] = {outline, DWORD_MAX};
            sw[0] = ColorConvTable::ColorCorrection(sw[0]);

            w->Paint(CPoint(x, y), org);

            if (w->m_style.borderStyle == 0) {
                bbox |= w->Draw(spd, clipRect, pAlphaMask, x, y, sw, !w->m_style.alpha[0] && !w->m_style.alpha[1] && !alpha, true);
            } else if (w->m_style.borderStyle == 1 && w->m_pOpaqueBox) {
                bbox |= w->m_pOpaqueBox->Draw(spd, clipRect, pAlphaMask, x, y, sw, true, false);
            }
        }

        p.x += w->m_width;
    }

    return bbox;
}

CRect CLine::PaintBody(SubPicDesc& spd, CRect& clipRect, BYTE* pAlphaMask, CPoint p, CPoint org, int time, int alpha)
{
    CRect bbox(0, 0, 0, 0);

    POSITION pos = GetHeadPosition();
    while (pos) {
        CWord* w = GetNext(pos);

        if (w->m_fLineBreak) {
            return bbox;    // should not happen since this class is just a line of text without any breaks
        }

        int x = p.x;
        int y = p.y + m_ascent - w->m_ascent;
        // colors

        DWORD aprimary = w->m_style.alpha[0];
        DWORD asecondary = w->m_style.alpha[1];
        if (alpha > 0) {
            aprimary += alpha * (0xff - w->m_style.alpha[0]) / 0xff;
            asecondary += alpha * (0xff - w->m_style.alpha[1]) / 0xff;
        }
        COLORREF primary = revcolor(w->m_style.colors[0]) | ((0xff - aprimary) << 24);
        COLORREF secondary = revcolor(w->m_style.colors[1]) | ((0xff - asecondary) << 24);

        DWORD sw[6] = {primary, 0, secondary};

        // karaoke

        double t = 0.0;

        if (w->m_ktype == 0 || w->m_ktype == 2) {
            t = time < w->m_kstart ? 0.0 : 1.0;
        } else if (w->m_ktype == 1) {
            if (time < w->m_kstart) {
                t = 0.0;
            } else if (time < w->m_kend) {
                t = 1.0 * (time - w->m_kstart) / (w->m_kend - w->m_kstart);

                double angle = fmod(w->m_style.fontAngleZ, 360.0);
                if (angle > 90 && angle < 270) {
                    t = 1.0 - t;
                    COLORREF tmp = sw[0];
                    sw[0] = sw[2];
                    sw[2] = tmp;
                }
            } else {
                t = 1.0;
            }
        }

        if (t >= 1.0) {
            sw[1] = DWORD_MAX;
        }

        // move dividerpoint
        int bluradjust = 0;
        if (w->m_style.fGaussianBlur > 0) {
            bluradjust += (int)(w->m_style.fGaussianBlur * 3 * 8 + 0.5) | 1;
        }
        if (w->m_style.fBlur) {
            bluradjust += 8;
        }

        w->Paint(CPoint(x, y), org);

        sw[0] = ColorConvTable::ColorCorrection(sw[0]);
        sw[2] = ColorConvTable::ColorCorrection(sw[2]);
        sw[3] = (int)(w->m_style.outlineWidthX + t * w->getOverlayWidth() + t * bluradjust) >> 3;
        sw[4] = sw[2];
        sw[5] = 0x00ffffff;

        bbox |= w->Draw(spd, clipRect, pAlphaMask, x, y, sw, true, false);
        p.x += w->m_width;
    }

    return bbox;
}


// CSubtitle

CSubtitle::CSubtitle(RenderingCaches& renderingCaches)
    : m_renderingCaches(renderingCaches)
    , m_scrAlignment(0)
    , m_wrapStyle(0)
    , m_fAnimated(false)
    , m_bIsAnimated(false)
    , m_relativeTo(STSStyle::AUTO)
    , m_pClipper(nullptr)
    , m_topborder(0)
    , m_bottomborder(0)
    , m_clipInverse(false)
    , m_target_scale_x(1.0)
    , m_target_scale_y(1.0)
    , m_script_scale_x(1.0)
    , m_script_scale_y(1.0)
    , m_total_scale_x(1.0)
    , m_total_scale_y(1.0)
    , m_allowLinePadding(false)
{
    ZeroMemory(m_effects, sizeof(Effect*)*EF_NUMBEROFEFFECTS);
}

CSubtitle::~CSubtitle()
{
    Empty();
}

void CSubtitle::Empty()
{
    POSITION pos = GetHeadPosition();
    while (pos) {
        delete GetNext(pos);
    }

    pos = m_words.GetHeadPosition();
    while (pos) {
        delete m_words.GetNext(pos);
    }

    EmptyEffects();

    m_pClipper.reset();
}

void CSubtitle::EmptyEffects()
{
    for (ptrdiff_t i = 0; i < EF_NUMBEROFEFFECTS; i++) {
        SAFE_DELETE(m_effects[i]);
    }
}

int CSubtitle::GetFullWidth()
{
    int width = 0;

    POSITION pos = m_words.GetHeadPosition();
    while (pos) {
        width += m_words.GetNext(pos)->m_width;
    }

    return width;
}

int CSubtitle::GetFullLineWidth(POSITION pos)
{
    int width = 0;

    while (pos) {
        CWord* w = m_words.GetNext(pos);
        if (w->m_fLineBreak) {
            break;
        }
        width += w->m_width;
    }

    return width;
}

int CSubtitle::GetWrapWidth(POSITION pos, int maxwidth)
{
    if (m_wrapStyle == 0 || m_wrapStyle == 3) {
        if (maxwidth > 0) {
            int fullwidth = GetFullLineWidth(pos);

            int minwidth = fullwidth / ((abs(fullwidth) / maxwidth) + 1);

            int width = 0, wordwidth = 0;

            while (pos && width < minwidth) {
                CWord* w = m_words.GetNext(pos);
                wordwidth = w->m_width;
                if (abs(width + wordwidth) < abs(maxwidth)) {
                    width += wordwidth;
                }
            }

            if (m_wrapStyle == 3 && width < fullwidth && fullwidth - width + wordwidth < maxwidth) {
                width -= wordwidth;
            }
            maxwidth = width;
        }
    } else if (m_wrapStyle == 1) {
        //      maxwidth = maxwidth;
    } else if (m_wrapStyle == 2) {
        maxwidth = INT_MAX;
    }

    return maxwidth;
}

CLine* CSubtitle::GetNextLine(POSITION& pos, int maxwidth)
{
    if (pos == nullptr) {
        return nullptr;
    }

    CLine* ret;
    try {
        ret = DEBUG_NEW CLine();
    } catch (CMemoryException* e) {
        e->Delete();
        return nullptr;
    }

    ret->m_width = ret->m_ascent = ret->m_descent = ret->m_borderX = ret->m_borderY = ret->m_linePadding = 0;

    maxwidth = GetWrapWidth(pos, maxwidth);

    bool fEmptyLine = true;

    while (pos) {
        CWord* w = m_words.GetNext(pos);

        if (ret->m_ascent < w->m_ascent) {
            ret->m_ascent = w->m_ascent;
        }
        if (ret->m_descent < w->m_descent) {
            ret->m_descent = w->m_descent;
        }
        if (ret->m_borderX < w->m_style.outlineWidthX) {
            ret->m_borderX = (int)(w->m_style.outlineWidthX + 0.5);
        }
        if (ret->m_borderY < w->m_style.outlineWidthY) {
            ret->m_borderY = (int)(w->m_style.outlineWidthY + 0.5);
        }
        if (w->m_style.borderStyle == 1 && m_allowLinePadding && (ret->m_linePadding < ret->m_borderY * 2)) {
            ret->m_linePadding = ret->m_borderY * 2;
        }

        if (w->m_fLineBreak) {
            if (fEmptyLine) {
                ret->m_ascent /= 2;
                ret->m_descent /= 2;
                ret->m_borderX = ret->m_borderY = 0;
            }

            ret->Compact();

            return ret;
        }

        fEmptyLine = false;

        bool fWSC = w->m_fWhiteSpaceChar;

        int width = w->m_width;
        POSITION pos2 = pos;
        while (pos2) {
            if (m_words.GetAt(pos2)->m_fWhiteSpaceChar != fWSC
                    || m_words.GetAt(pos2)->m_fLineBreak) {
                break;
            }

            CWord* w2 = m_words.GetNext(pos2);
            width += w2->m_width;
        }

        if ((ret->m_width += width) <= maxwidth || ret->IsEmpty()) {
            ret->AddTail(w->Copy());

            while (pos != pos2) {
                ret->AddTail(m_words.GetNext(pos)->Copy());
            }

            pos = pos2;
        } else {
            if (pos) {
                m_words.GetPrev(pos);
            } else {
                pos = m_words.GetTailPosition();
            }

            ret->m_width -= width;

            break;
        }
    }

    ret->Compact();

    return ret;
}

void CSubtitle::CreateClippers(CSize size)
{
    size.cx >>= 3;
    size.cy >>= 3;

    auto createClipper = [this](const CSize & size) {
        ASSERT(!m_pClipper);
        CStringW str;
        str.Format(L"m %d %d l %d %d %d %d %d %d", 0, 0, size.cx, 0, size.cx, size.cy, 0, size.cy);

        try {
            m_pClipper = std::make_shared<CClipper>(str, size, 1.0, 1.0, false, CPoint(0, 0), m_renderingCaches);
        } catch (CMemoryException* e) {
            e->Delete();
        }

        return !!m_pClipper;
    };

    if (m_effects[EF_BANNER] && m_effects[EF_BANNER]->param[2]) {
        if (!m_pClipper && !createClipper(size)) {
            return;
        }
        m_pClipper->SetEffect(*m_effects[EF_BANNER], EF_BANNER);
    } else if (m_effects[EF_SCROLL] && m_effects[EF_SCROLL]->param[4]) {
        if (!m_pClipper && !createClipper(size)) {
            return;
        }
        m_pClipper->SetEffect(*m_effects[EF_SCROLL], EF_SCROLL);
    }
}

void CSubtitle::MakeLines(CSize size, const CRect& marginRect)
{
    CSize spaceNeeded(0, 0);

    bool fFirstLine = true;

    m_topborder = m_bottomborder = 0;

    CLine* l = nullptr;

    POSITION pos = m_words.GetHeadPosition();
    while (pos) {
        l = GetNextLine(pos, size.cx - marginRect.left - marginRect.right);
        if (!l) {
            break;
        }

        if (fFirstLine) {
            m_topborder = l->m_borderY;
            l->m_linePadding = 0;
            fFirstLine = false;
        }

        spaceNeeded.cx = std::max<long>(l->m_width + l->m_borderX, spaceNeeded.cx);
        spaceNeeded.cy += l->m_ascent + l->m_descent + l->m_linePadding;

        AddTail(l);
    }

    if (l) {
        m_bottomborder = l->m_borderY;
    }

    m_rect = CRect(
                 CPoint((m_scrAlignment % 3) == 1 ? marginRect.left
                        : (m_scrAlignment % 3) == 2 ? (marginRect.left + (size.cx - marginRect.right) - spaceNeeded.cx + 1) / 2
                        : (size.cx - marginRect.right - spaceNeeded.cx),
                        m_scrAlignment <= 3 ? (size.cy - marginRect.bottom - spaceNeeded.cy)
                        : m_scrAlignment <= 6 ? (marginRect.top + (size.cy - marginRect.bottom) - spaceNeeded.cy + 1) / 2
                        : marginRect.top),
                 spaceNeeded);
}

// CScreenLayoutAllocator

void CScreenLayoutAllocator::Empty()
{
    m_subrects.RemoveAll();
}

void CScreenLayoutAllocator::AdvanceToSegment(int segment, const CAtlArray<int>& sa)
{
    POSITION pos = m_subrects.GetHeadPosition();
    while (pos) {
        POSITION prev = pos;

        SubRect& sr = m_subrects.GetNext(pos);

        bool fFound = false;

        if (abs(sr.segment - segment) <= 1) { // using abs() makes it possible to play the subs backwards, too :)
            for (size_t i = 0; i < sa.GetCount() && !fFound; i++) {
                if (sa[i] == sr.entry) {
                    sr.segment = segment;
                    fFound = true;
                }
            }
        }

        if (!fFound) {
            m_subrects.RemoveAt(prev);
        }
    }
}

CRect CScreenLayoutAllocator::AllocRect(const CSubtitle* s, int segment, int entry, int layer, int collisions)
{
    // TODO: handle collisions == 1 (reversed collisions)

    POSITION pos = m_subrects.GetHeadPosition();
    while (pos) {
        SubRect& sr = m_subrects.GetNext(pos);
        if (sr.segment == segment && sr.entry == entry) {
            return (sr.r + CRect(0, -s->m_topborder, 0, -s->m_bottomborder));
        }
    }

    CRect r = s->m_rect + CRect(0, s->m_topborder, 0, s->m_bottomborder);

    bool fSearchDown = s->m_scrAlignment > 3;

    bool fOK;

    do {
        fOK = true;

        pos = m_subrects.GetHeadPosition();
        while (pos) {
            SubRect& sr = m_subrects.GetNext(pos);

            if (layer == sr.layer && !(r & sr.r).IsRectEmpty()) {
                if (fSearchDown) {
                    r.bottom = sr.r.bottom + r.Height();
                    r.top = sr.r.bottom;
                } else {
                    r.top = sr.r.top - r.Height();
                    r.bottom = sr.r.top;
                }

                fOK = false;
            }
        }
    } while (!fOK);

    SubRect sr;
    sr.r = r;
    sr.segment = segment;
    sr.entry = entry;
    sr.layer = layer;
    m_subrects.AddTail(sr);

    return (sr.r + CRect(0, -s->m_topborder, 0, -s->m_bottomborder));
}

// CRenderedTextSubtitle

CAtlMap<CStringW, SSATagCmd, CStringElementTraits<CStringW>> CRenderedTextSubtitle::s_SSATagCmds;

CRenderedTextSubtitle::CRenderedTextSubtitle(CCritSec* pLock)
    : CSubPicProviderImpl(pLock)
    , m_time(0)
    , m_delay(0)
    , m_animStart(0)
    , m_animEnd(0)
    , m_animAccel(0.0)
    , m_ktype(0)
    , m_kstart(0)
    , m_kend(0)
    , m_nPolygon(0)
    , m_polygonBaselineOffset(0)
    , m_bOverridePlacement(false)
    , m_overridePlacement(50, 90)
    , m_webvtt_allow_clear(false)
    , m_bUseFreeType(false)
{
    m_size = CSize(0, 0);

    if (g_hDC_refcnt == 0) {
        g_hDC = CreateCompatibleDC(nullptr);
        SetBkMode(g_hDC, TRANSPARENT);
        SetTextColor(g_hDC, 0xffffff);
        SetMapMode(g_hDC, MM_TEXT);
    }

    g_hDC_refcnt++;

    if (s_SSATagCmds.IsEmpty()) {
        s_SSATagCmds[L"1c"] = SSA_1c;
        s_SSATagCmds[L"2c"] = SSA_2c;
        s_SSATagCmds[L"3c"] = SSA_3c;
        s_SSATagCmds[L"4c"] = SSA_4c;
        s_SSATagCmds[L"1a"] = SSA_1a;
        s_SSATagCmds[L"2a"] = SSA_2a;
        s_SSATagCmds[L"3a"] = SSA_3a;
        s_SSATagCmds[L"4a"] = SSA_4a;
        s_SSATagCmds[L"alpha"] = SSA_alpha;
        s_SSATagCmds[L"an"] = SSA_an;
        s_SSATagCmds[L"a"] = SSA_a;
        s_SSATagCmds[L"blur"] = SSA_blur;
        s_SSATagCmds[L"bord"] = SSA_bord;
        s_SSATagCmds[L"be"] = SSA_be;
        s_SSATagCmds[L"b"] = SSA_b;
        s_SSATagCmds[L"clip"] = SSA_clip;
        s_SSATagCmds[L"iclip"] = SSA_iclip;
        s_SSATagCmds[L"c"] = SSA_c;
        s_SSATagCmds[L"fade"] = SSA_fade;
        s_SSATagCmds[L"fad"] = SSA_fade;
        s_SSATagCmds[L"fax"] = SSA_fax;
        s_SSATagCmds[L"fay"] = SSA_fay;
        s_SSATagCmds[L"fe"] = SSA_fe;
        s_SSATagCmds[L"fn"] = SSA_fn;
        s_SSATagCmds[L"frx"] = SSA_frx;
        s_SSATagCmds[L"fry"] = SSA_fry;
        s_SSATagCmds[L"frz"] = SSA_frz;
        s_SSATagCmds[L"fr"] = SSA_fr;
        s_SSATagCmds[L"fscx"] = SSA_fscx;
        s_SSATagCmds[L"fscy"] = SSA_fscy;
        s_SSATagCmds[L"fsc"] = SSA_fsc;
        s_SSATagCmds[L"fsp"] = SSA_fsp;
        s_SSATagCmds[L"fs"] = SSA_fs;
        s_SSATagCmds[L"i"] = SSA_i;
        s_SSATagCmds[L"kt"] = SSA_kt;
        s_SSATagCmds[L"kf"] = SSA_kf;
        s_SSATagCmds[L"K"] = SSA_K;
        s_SSATagCmds[L"ko"] = SSA_ko;
        s_SSATagCmds[L"k"] = SSA_k;
        s_SSATagCmds[L"move"] = SSA_move;
        s_SSATagCmds[L"org"] = SSA_org;
        s_SSATagCmds[L"pbo"] = SSA_pbo;
        s_SSATagCmds[L"pos"] = SSA_pos;
        s_SSATagCmds[L"p"] = SSA_p;
        s_SSATagCmds[L"q"] = SSA_q;
        s_SSATagCmds[L"r"] = SSA_r;
        s_SSATagCmds[L"shad"] = SSA_shad;
        s_SSATagCmds[L"s"] = SSA_s;
        s_SSATagCmds[L"t"] = SSA_t;
        s_SSATagCmds[L"u"] = SSA_u;
        s_SSATagCmds[L"xbord"] = SSA_xbord;
        s_SSATagCmds[L"xshad"] = SSA_xshad;
        s_SSATagCmds[L"ybord"] = SSA_ybord;
        s_SSATagCmds[L"yshad"] = SSA_yshad;
    }
}

CRenderedTextSubtitle::~CRenderedTextSubtitle()
{
    Deinit();

    g_hDC_refcnt--;
    if (g_hDC_refcnt == 0) {
        DeleteDC(g_hDC);
    }
}

void CRenderedTextSubtitle::Copy(CSimpleTextSubtitle& sts)
{
    __super::Copy(sts);

    m_size = CSize(0, 0);

    if (CRenderedTextSubtitle* pRTS = dynamic_cast<CRenderedTextSubtitle*>(&sts)) {
        m_size = pRTS->m_size;
    }
}

void CRenderedTextSubtitle::Empty()
{
    Deinit();

    __super::Empty();
}

void CRenderedTextSubtitle::SetOverride(bool bOverrideDefault, bool bOverrideAll, const STSStyle& styleOverride) {
    overrideANSICharset = styleOverride.charSet;
    bool bOverride = bOverrideDefault || bOverrideAll;
    bool changed = (bOverride && !m_bStyleOverrideActive) || (m_SubRendererSettings.overrideDefaultStyle != bOverrideDefault) || (m_SubRendererSettings.overrideAllStyles != bOverrideAll) || bOverride && (m_SubRendererSettings.defaultStyle != styleOverride);
    
    if (changed) {
        m_bStyleOverrideActive = bOverride;
        m_SubRendererSettings.defaultStyle = bOverride ? styleOverride : GetOriginalDefaultStyle();
        m_SubRendererSettings.overrideDefaultStyle = bOverride;
        m_SubRendererSettings.overrideAllStyles = bOverrideAll;

        UpdateSubRelativeTo(m_subtitleType, m_SubRendererSettings.defaultStyle.relativeTo);

        if (bOverride) {
            m_scaledBAS = 0;
        } else {
            m_scaledBAS = m_scaledBAS2; // restore original
        }

        if (bOverride && m_playRes.cy != 288 && m_playRes.cx > 0 && m_playRes.cy > 0) {
            /* Default style is defined relative to a 384x288 PlayRes resolution. Scale to get consistent font size. */
            double scaleX = m_playRes.cx / 384.0;
            double scaleY = m_playRes.cy / 288.0;
            m_SubRendererSettings.defaultStyle.fontSize    *= scaleY;
            m_SubRendererSettings.defaultStyle.fontSpacing *= scaleX;
            m_SubRendererSettings.defaultStyle.marginRect.left   = std::lround(scaleX * m_SubRendererSettings.defaultStyle.marginRect.left);
            m_SubRendererSettings.defaultStyle.marginRect.top    = std::lround(scaleY * m_SubRendererSettings.defaultStyle.marginRect.top);
            m_SubRendererSettings.defaultStyle.marginRect.right  = std::lround(scaleX * m_SubRendererSettings.defaultStyle.marginRect.right);
            m_SubRendererSettings.defaultStyle.marginRect.bottom = std::lround(scaleY * m_SubRendererSettings.defaultStyle.marginRect.bottom);
        }

        SetDefaultStyle(m_SubRendererSettings.defaultStyle);

#if USE_LIBASS
        if (m_LibassContext.IsLibassActive()) {
            m_LibassContext.StylesChanged();
        }
#endif
    }
}

void CRenderedTextSubtitle::OnChanged()
{
    __super::OnChanged();
    CAutoLock cAutoLock(&renderLock);

    POSITION pos = m_subtitleCache.GetStartPosition();
    while (pos) {
        int i;
        CSubtitle* s;
        m_subtitleCache.GetNextAssoc(pos, i, s);
        delete s;
    }

    m_subtitleCache.RemoveAll();

    m_sla.Empty();
}

bool CRenderedTextSubtitle::Init(CSize size, const CRect& vidrect)
{
    if (!vidrect.Width()) {
        return false;
    }

    CAutoLock cAutoLock(&renderLock);
    CRect newVidRect = CRect(vidrect.left * 8, vidrect.top * 8, vidrect.right * 8, vidrect.bottom * 8);
    CSize newSize = CSize(size.cx * 8, size.cy * 8);
    if (m_size != newSize || m_vidrect != newVidRect) {
        TRACE(_T("RTS Init | texture %dx%d | vidrect %dx%d\n"), size.cx, size.cy, vidrect.Width(), vidrect.Height());
        Deinit();
        m_size = newSize;
        m_vidrect = newVidRect;
    }

    return true;
}

void CRenderedTextSubtitle::Deinit()
{
    CAutoLock cAutoLock(&renderLock);
    POSITION pos = m_subtitleCache.GetStartPosition();
    while (pos) {
        int i;
        CSubtitle* s;
        m_subtitleCache.GetNextAssoc(pos, i, s);
        delete s;
    }
    m_subtitleCache.RemoveAll();

    m_sla.Empty();

    m_size = CSize(0, 0);
    m_vidrect.SetRectEmpty();
}

void CRenderedTextSubtitle::ParseEffect(CSubtitle* sub, CString str)
{
    str.Trim();
    if (!sub || str.IsEmpty()) {
        return;
    }

    const TCHAR* s = _tcschr(str, ';');
    if (!s) {
        s = (LPTSTR)(LPCTSTR)str;
        s += str.GetLength() - 1;
    }
    s++;
    CString effect = str.Left(int(s - str));

    if (!effect.CompareNoCase(_T("Banner;"))) {
        sub->m_bIsAnimated = true;

        int delay, lefttoright = 0, fadeawaywidth = 0;
        if (_stscanf_s(s, _T("%d;%d;%d"), &delay, &lefttoright, &fadeawaywidth) < 1) {
            return;
        }

        Effect* e;
        try {
            e = DEBUG_NEW Effect;
        } catch (CMemoryException* e) {
            e->Delete();
            return;
        }

        sub->m_effects[e->type = EF_BANNER] = e;
        e->param[0] = (int)(std::max(1.0 * delay / sub->m_total_scale_x, 1.0));
        e->param[1] = lefttoright;
        e->param[2] = std::lround(sub->m_total_scale_x * fadeawaywidth);

        sub->m_wrapStyle = 2;
    } else if (!effect.CompareNoCase(_T("Scroll up;")) || !effect.CompareNoCase(_T("Scroll down;"))) {
        sub->m_bIsAnimated = true;

        int top, bottom, delay, fadeawayheight = 0;
        if (_stscanf_s(s, _T("%d;%d;%d;%d"), &top, &bottom, &delay, &fadeawayheight) < 3) {
            return;
        }

        if (top > bottom) {
            int tmp = top;
            top = bottom;
            bottom = tmp;
        }

        Effect* e;
        try {
            e = DEBUG_NEW Effect;
        } catch (CMemoryException* e) {
            e->Delete();
            return;
        }

        sub->m_effects[e->type = EF_SCROLL] = e;
        e->param[0] = std::lround(sub->m_total_scale_y * top * 8.0);
        e->param[1] = std::lround(sub->m_total_scale_y * bottom * 8.0);
        e->param[2] = (int)(std::max(double(delay) / sub->m_total_scale_y, 1.0));
        e->param[3] = (effect.GetLength() == 12);
        e->param[4] = std::lround(sub->m_total_scale_y * fadeawayheight);
    }
}

void CRenderedTextSubtitle::ParseString(CSubtitle* sub, CStringW str, STSStyle& style)
{
    if (!sub) {
        return;
    }

    if (str.GetLength() == 1) {
        if (str[0] == L'\x202a' || str[0] == L'\x202b') {
            return; // ignore Unicode control character
        }
    }

    str.Replace(L"<br>", L"\n");
    str.Replace(L"\\N", L"\n");
    str.Replace(L"\\n", (sub->m_wrapStyle < 2 || sub->m_wrapStyle == 3) ? L" " : L"\n");
    str.Replace(L"\\h", L"\x00A0"); // no-break space

    for (int i = 0, j = 0, len = str.GetLength(); j <= len; j++) {
        WCHAR c = str[j];

        if (c != L'\n' && c != L' ' && c != 0) {
            continue;
        }

        if (i < j) {
            if (CText* w = DEBUG_NEW CText(style, str.Mid(i, j - i), m_ktype, m_kstart, m_kend, sub->m_target_scale_x, sub->m_target_scale_y, m_renderingCaches)) {
                w->SetRts(this);
                sub->m_words.AddTail(w);
                m_kstart = m_kend;
            }
        }

        if (c == L'\n') {
            if (CText* w = DEBUG_NEW CText(style, CStringW(), m_ktype, m_kstart, m_kend, sub->m_target_scale_x, sub->m_target_scale_y, m_renderingCaches)) {
                w->SetRts(this);
                sub->m_words.AddTail(w);
                m_kstart = m_kend;
            }
        } else if (c == L' ') {
            if (CText* w = DEBUG_NEW CText(style, CStringW(c), m_ktype, m_kstart, m_kend, sub->m_target_scale_x, sub->m_target_scale_y, m_renderingCaches)) {
                w->SetRts(this);
                sub->m_words.AddTail(w);
                m_kstart = m_kend;
            }
        }

        i = j + 1;
    }

    return;
}

void CRenderedTextSubtitle::ParsePolygon(CSubtitle* sub, CStringW str, STSStyle& style)
{
    if (!sub || !str.GetLength() || !m_nPolygon) {
        return;
    }

    int s = 1 << (m_nPolygon - 1);
    if (CWord* w = DEBUG_NEW CPolygon(style, str, m_ktype, m_kstart, m_kend,
                                      sub->m_total_scale_x / s, sub->m_total_scale_y / s,
                                      m_polygonBaselineOffset,
                                      m_renderingCaches)) {
        sub->m_words.AddTail(w);
        m_kstart = m_kend;
    }
}

bool CRenderedTextSubtitle::ParseSSATag(SSATagsList& tagsList, const CStringW& str)
{
    if (m_renderingCaches.SSATagsCache.Lookup(str, tagsList)) {
        //TRACE(_T("ParseSSATag (cached): %s\n"), str.GetString());
        return true;
    }
    //TRACE(_T("ParseSSATag: %s\n"), str.GetString());

    int nTags = 0, nUnrecognizedTags = 0;
    tagsList.reset(DEBUG_NEW CAtlList<SSATag>());

    for (int i = 0, j; (j = str.Find(L'\\', i)) >= 0; i = j) {
        int jOld;
        // find the end of the current tag or the start of its parameters
        for (jOld = ++j; str[j] && str[j] != L'(' && str[j] != L'\\'; ++j) {
            ;
        }
        CStringW cmd = str.Mid(jOld, j - jOld);
        FastTrim(cmd);
        if (cmd.IsEmpty()) {
            continue;
        }

        nTags++;

        SSATag tag;
        tag.cmd = SSA_unknown;
        for (int cmdLength = std::min(SSA_CMD_MAX_LENGTH, cmd.GetLength()), cmdLengthMin = SSA_CMD_MIN_LENGTH; cmdLength >= cmdLengthMin; cmdLength--) {
            if (s_SSATagCmds.Lookup(cmd.Left(cmdLength), tag.cmd)) {
                break;
            }
        }
        if (tag.cmd == SSA_unknown) {
            nUnrecognizedTags++;
            continue;
        }

        if (str[j] == L'(') {
            // complex tags search
            int br = 1; // 1 bracket
            // find the end of the parameters
            for (jOld = ++j; str[j] && br > 0; ++j) {
                if (str[j] == L'(') {
                    br++;
                } else if (str[j] == L')') {
                    br--;
                }
                if (br == 0) {
                    break;
                }
            }
            CStringW param = str.Mid(jOld, j - jOld);
            FastTrim(param);

            while (!param.IsEmpty()) {
                int k = param.Find(L','), l = param.Find(L'\\');

                if (k >= 0 && (l < 0 || k < l)) {
                    CStringW s = param.Left(k).Trim();
                    if (!s.IsEmpty()) {
                        tag.params.Add(s);
                    }
                    param = k + 1 < param.GetLength() ? param.Mid(k + 1).GetString() : L"";
                } else {
                    FastTrim(param);
                    if (!param.IsEmpty()) {
                        tag.params.Add(param);
                    }
                    param.Empty();
                }
            }
        }

        switch (tag.cmd) {
            case SSA_1c:
            case SSA_2c:
            case SSA_3c:
            case SSA_4c:
            case SSA_1a:
            case SSA_2a:
            case SSA_3a:
            case SSA_4a:
                if (cmd.GetLength() > 2) {
                    tag.paramsInt.Add(wcstol(cmd.Mid(2).Trim(L"&H"), nullptr, 16));
                }
                break;
            case SSA_alpha:
                if (cmd.GetLength() > 5) {
                    tag.paramsInt.Add(wcstol(cmd.Mid(5).Trim(L"&H"), nullptr, 16));
                }
                break;
            case SSA_an:
            case SSA_be:
            case SSA_fe:
            case SSA_kt:
            case SSA_kf:
            case SSA_ko:
                if (cmd.GetLength() > 2) {
                    tag.paramsInt.Add(wcstol(cmd.Mid(2), nullptr, 10));
                }
                break;
            case SSA_fn:
                tag.params.Add(cmd.Mid(2));
                break;
            case SSA_fr:
                if (cmd.GetLength() > 2) {
                    tag.paramsReal.Add(wcstod(cmd.Mid(2), nullptr));
                }
                break;
            case SSA_fs:
                if (cmd.GetLength() > 2) {
                    int s = 2;
                    if (cmd[s] == L'+' || cmd[s] == L'-') {
                        tag.params.Add(cmd.Mid(s, 1));
                    }
                    tag.paramsInt.Add(wcstol(cmd.Mid(s), nullptr, 10));
                }
                break;
            case SSA_a:
            case SSA_b:
            case SSA_i:
            case SSA_k:
            case SSA_K:
            case SSA_p:
            case SSA_q:
            case SSA_s:
            case SSA_u:
                if (cmd.GetLength() > 1) {
                    tag.paramsInt.Add(wcstol(cmd.Mid(1), nullptr, 10));
                }
                break;
            case SSA_r:
                tag.params.Add(cmd.Mid(1));
                break;
            case SSA_blur:
            case SSA_bord:
            case SSA_fscx:
            case SSA_fscy:
            case SSA_shad:
                if (cmd.GetLength() > 4) {
                    tag.paramsReal.Add(wcstod(cmd.Mid(4), nullptr));
                }
                break;
            case SSA_clip:
            case SSA_iclip: {
                size_t nParams = tag.params.GetCount();
                if (nParams == 2) {
                    tag.paramsInt.Add(wcstol(tag.params[0], nullptr, 10));
                    tag.params.RemoveAt(0);
                } else if (nParams == 4) {
                    for (size_t n = 0; n < nParams; n++) {
                        tag.paramsInt.Add(wcstol(tag.params[n], nullptr, 10));
                    }
                    tag.params.RemoveAll();
                }
            }
            break;
            case SSA_fade: {
                size_t nParams = tag.params.GetCount();
                if (nParams == 7 || nParams == 2) {
                    for (size_t n = 0; n < nParams; n++) {
                        tag.paramsInt.Add(wcstol(tag.params[n], nullptr, 10));
                    }
                    tag.params.RemoveAll();
                }
            }
            break;
            case SSA_move: {
                size_t nParams = tag.params.GetCount();
                if (nParams == 4 || nParams == 6) {
                    for (size_t n = 0; n < 4; n++) {
                        tag.paramsReal.Add(wcstod(tag.params[n], nullptr));
                    }
                    for (size_t n = 4; n < nParams; n++) {
                        tag.paramsInt.Add(wcstol(tag.params[n], nullptr, 10));
                    }
                    tag.params.RemoveAll();
                }
            }
            break;
            case SSA_org:
            case SSA_pos: {
                size_t nParams = tag.params.GetCount();
                if (nParams == 2) {
                    for (size_t n = 0; n < nParams; n++) {
                        tag.paramsReal.Add(wcstod(tag.params[n], nullptr));
                    }
                    tag.params.RemoveAll();
                }
            }
            break;
            case SSA_c:
                if (cmd.GetLength() > 1) {
                    tag.paramsInt.Add(wcstol(cmd.Mid(1).Trim(L"&H"), nullptr, 16));
                }
                break;
            case SSA_frx:
            case SSA_fry:
            case SSA_frz:
            case SSA_fax:
            case SSA_fay:
            case SSA_fsc:
            case SSA_fsp:
                if (cmd.GetLength() > 3) {
                    tag.paramsReal.Add(wcstod(cmd.Mid(3), nullptr));
                }
                break;
            case SSA_pbo:
                if (cmd.GetLength() > 3) {
                    tag.paramsInt.Add(wcstol(cmd.Mid(3), nullptr, 10));
                }
                break;
            case SSA_t: {
                size_t nParams = tag.params.GetCount();
                if (nParams >= 1 && nParams <= 4) {
                    if (nParams == 2) {
                        tag.paramsReal.Add(wcstod(tag.params[0], nullptr));
                    } else if (nParams == 3) {
                        tag.paramsReal.Add(wcstod(tag.params[0], nullptr));
                        tag.paramsReal.Add(wcstod(tag.params[1], nullptr));
                    } else if (nParams == 4) {
                        tag.paramsInt.Add(wcstol(tag.params[0], nullptr, 10));
                        tag.paramsInt.Add(wcstol(tag.params[1], nullptr, 10));
                        tag.paramsReal.Add(wcstod(tag.params[2], nullptr));
                    }

                    ParseSSATag(tag.subTagsList, tag.params[nParams - 1]);
                }
                tag.params.RemoveAll();
            }
            break;
            case SSA_xbord:
            case SSA_xshad:
            case SSA_ybord:
            case SSA_yshad:
                if (cmd.GetLength() > 5) {
                    tag.paramsReal.Add(wcstod(cmd.Mid(5), nullptr));
                }
                break;
        }

        tagsList->AddTail(tag);
    }

    m_renderingCaches.SSATagsCache.SetAt(str, tagsList);

    //return (nUnrecognizedTags < nTags);
    return true; // there are people keeping comments inside {}, lets make them happy now
}

bool CRenderedTextSubtitle::CreateSubFromSSATag(CSubtitle* sub, const SSATagsList& tagsList,
                                                STSStyle& style, STSStyle& org, bool fAnimate /*= false*/)
{
    if (!sub || !tagsList) {
        return false;
    }

    POSITION pos = tagsList->GetHeadPosition();
    while (pos) {
        const SSATag& tag = tagsList->GetNext(pos);

        // TODO: call ParseStyleModifier(cmd, params, ..) and move the rest there

        switch (tag.cmd) {
            case SSA_1c:
            case SSA_2c:
            case SSA_3c:
            case SSA_4c: {
                int k = tag.cmd - SSA_1c;

                if (!tag.paramsInt.IsEmpty()) {
                    DWORD c = tag.paramsInt[0];
                    style.colors[k] = (((int)CalcAnimation(c & 0xff, style.colors[k] & 0xff, fAnimate)) & 0xff
                                       | ((int)CalcAnimation(c & 0xff00, style.colors[k] & 0xff00, fAnimate)) & 0xff00
                                       | ((int)CalcAnimation(c & 0xff0000, style.colors[k] & 0xff0000, fAnimate)) & 0xff0000);
                } else {
                    style.colors[k] = org.colors[k];
                }
            }
            break;
            case SSA_1a:
            case SSA_2a:
            case SSA_3a:
            case SSA_4a: {
                int k = tag.cmd - SSA_1a;

                style.alpha[k] = !tag.paramsInt.IsEmpty()
                                 ? (BYTE)CalcAnimation(tag.paramsInt[0] & 0xff, style.alpha[k], fAnimate)
                                 : org.alpha[k];
            }
            break;
            case SSA_alpha:
                for (ptrdiff_t k = 0; k < 4; k++) {
                    style.alpha[k] = !tag.paramsInt.IsEmpty()
                                     ? (BYTE)CalcAnimation(tag.paramsInt[0] & 0xff, style.alpha[k], fAnimate)
                                     : org.alpha[k];
                }
                break;
            case SSA_an: {
                int n = !tag.paramsInt.IsEmpty() ? tag.paramsInt[0] : 0;
                if (sub->m_scrAlignment < 0) {
                    sub->m_scrAlignment = (n > 0 && n < 10) ? n : org.scrAlignment;
                }
            }
            break;
            case SSA_a: {
                int n = !tag.paramsInt.IsEmpty() ? tag.paramsInt[0] : 0;
                if (sub->m_scrAlignment < 0) {
                    sub->m_scrAlignment = (n > 0 && n < 12) ? ((((n - 1) & 3) + 1) + ((n & 4) ? 6 : 0) + ((n & 8) ? 3 : 0)) : org.scrAlignment;
                }
            }
            break;
            case SSA_blur:
                if (!tag.paramsReal.IsEmpty()) {
                    double n = CalcAnimation(tag.paramsReal[0], style.fGaussianBlur, fAnimate);
                    style.fGaussianBlur = (n < 0 ? 0 : n);
                } else {
                    style.fGaussianBlur = org.fGaussianBlur;
                }
                if (style.fGaussianBlur > 25.0) {
                    style.fGaussianBlur = 25.0;
                    TRACE(L"INSANE blur value !!!\n");
                }
                break;
            case SSA_bord:
                if (!tag.paramsReal.IsEmpty()) {
                    double nx = CalcAnimation(tag.paramsReal[0], style.outlineWidthX, fAnimate);
                    style.outlineWidthX = (nx < 0 ? 0 : nx);
                    double ny = CalcAnimation(tag.paramsReal[0], style.outlineWidthY, fAnimate);
                    style.outlineWidthY = (ny < 0 ? 0 : ny);
                } else {
                    style.outlineWidthX = org.outlineWidthX;
                    style.outlineWidthY = org.outlineWidthY;
                }
                break;
            case SSA_be:
                style.fBlur = !tag.paramsInt.IsEmpty()
                              ? (int)(CalcAnimation(tag.paramsInt[0], style.fBlur, fAnimate) + 0.5)
                              : org.fBlur;
                break;
            case SSA_b: {
                int n = !tag.paramsInt.IsEmpty() ? tag.paramsInt[0] : -1;
                style.fontWeight = (n == 0 ? FW_NORMAL : n == 1 ? FW_BOLD : n >= 100 ? n : org.fontWeight);
            }
            break;
            case SSA_clip:
            case SSA_iclip: {
                bool invert = (tag.cmd == SSA_iclip);
                size_t nParams = tag.params.GetCount();
                size_t nParamsInt = tag.paramsInt.GetCount();

                if (nParams == 1 && nParamsInt == 0 && !sub->m_pClipper) {
                    sub->m_pClipper = std::make_shared<CClipper>(tag.params[0], CSize(m_size.cx >> 3, m_size.cy >> 3), sub->m_total_scale_x, sub->m_total_scale_y,
                                                                 invert, (sub->m_relativeTo == STSStyle::VIDEO) ? CPoint(m_vidrect.left, m_vidrect.top) : CPoint(0, 0),
                                                                 m_renderingCaches);
                } else if (nParams == 1 && nParamsInt == 1 && !sub->m_pClipper) {
                    long scale = tag.paramsInt[0];
                    if (scale < 1) {
                        scale = 1;
                    }
                    long scalediv = (1 << (scale - 1));
                    sub->m_pClipper = std::make_shared<CClipper>(tag.params[0], CSize(m_size.cx >> 3, m_size.cy >> 3),
                                                                 sub->m_total_scale_x / scalediv, sub->m_total_scale_y / scalediv, invert,
                                                                 (sub->m_relativeTo == STSStyle::VIDEO) ? CPoint(m_vidrect.left, m_vidrect.top) : CPoint(0, 0),
                                                                 m_renderingCaches);
                } else if (nParamsInt == 4) {
                    sub->m_clipInverse = invert;

                    double dLeft   = sub->m_total_scale_x * tag.paramsInt[0];
                    double dTop    = sub->m_total_scale_y * tag.paramsInt[1];
                    double dRight  = sub->m_total_scale_x * tag.paramsInt[2];
                    double dBottom = sub->m_total_scale_y * tag.paramsInt[3];

                    if (sub->m_relativeTo == STSStyle::VIDEO) {
                        double dOffsetX = m_vidrect.left / 8.0;
                        double dOffsetY = m_vidrect.top / 8.0;
                        dLeft += dOffsetX;
                        dTop += dOffsetY;
                        dRight += dOffsetX;
                        dBottom += dOffsetY;
                    }

                    sub->m_clip.SetRect(
                        static_cast<int>(CalcAnimation(dLeft, sub->m_clip.left, fAnimate)),
                        static_cast<int>(CalcAnimation(dTop, sub->m_clip.top, fAnimate)),
                        static_cast<int>(CalcAnimation(dRight, sub->m_clip.right, fAnimate)),
                        static_cast<int>(CalcAnimation(dBottom, sub->m_clip.bottom, fAnimate)));
                }
            }
            break;
            case SSA_c:
                if (!tag.paramsInt.IsEmpty()) {
                    DWORD c = tag.paramsInt[0];
                    style.colors[0] = (((int)CalcAnimation(c & 0xff, style.colors[0] & 0xff, fAnimate)) & 0xff
                                       | ((int)CalcAnimation(c & 0xff00, style.colors[0] & 0xff00, fAnimate)) & 0xff00
                                       | ((int)CalcAnimation(c & 0xff0000, style.colors[0] & 0xff0000, fAnimate)) & 0xff0000);
                } else {
                    style.colors[0] = org.colors[0];
                }
                break;
            case SSA_fade: {
                sub->m_bIsAnimated = true;

                if (tag.paramsInt.GetCount() == 7 && !sub->m_effects[EF_FADE]) { // {\fade(a1=param[0], a2=param[1], a3=param[2], t1=t[0], t2=t[1], t3=t[2], t4=t[3])
                    if (Effect* e = DEBUG_NEW Effect) {
                        for (size_t k = 0; k < 3; k++) {
                            e->param[k] = tag.paramsInt[k];
                        }
                        for (size_t k = 0; k < 4; k++) {
                            e->t[k] = tag.paramsInt[3 + k];
                        }

                        sub->m_effects[EF_FADE] = e;
                    }
                } else if (tag.paramsInt.GetCount() == 2 && !sub->m_effects[EF_FADE]) { // {\fad(t1=t[1], t2=t[2])
                    if (Effect* e = DEBUG_NEW Effect) {
                        e->param[0] = e->param[2] = 0xff;
                        e->param[1] = 0x00;
                        for (size_t k = 1; k < 3; k++) {
                            e->t[k] = tag.paramsInt[k - 1];
                        }
                        e->t[0] = e->t[3] = -1; // will be substituted with "start" and "end"

                        sub->m_effects[EF_FADE] = e;
                    }
                }
            }
            break;
            case SSA_fax:
                style.fontShiftX = !tag.paramsReal.IsEmpty()
                                   ? CalcAnimation(tag.paramsReal[0], style.fontShiftX, fAnimate)
                                   : org.fontShiftX;
                break;
            case SSA_fay:
                style.fontShiftY = !tag.paramsReal.IsEmpty()
                                   ? CalcAnimation(tag.paramsReal[0], style.fontShiftY, fAnimate)
                                   : org.fontShiftY;
                break;
            case SSA_fe:
                style.charSet = !tag.paramsInt.IsEmpty()
                                ? tag.paramsInt[0]
                                : org.charSet;
                if (style.charSet < 0) style.charSet = DEFAULT_CHARSET;
                break;
            case SSA_fn:
                style.fontName = (!tag.params.IsEmpty() && !tag.params[0].IsEmpty() && tag.params[0] != L"0")
                                 ? CString(tag.params[0]).Trim()
                                 : org.fontName;
                if (style.fontName == _T("splatter")) {
                    /* workaround for slow rendering with this font
                       slowness occurs in Windows GDI CloseFigure() function
                    */
                    style.fontName = _T("Arial");
                }
                break;
            case SSA_frx:
                style.fontAngleX = !tag.paramsReal.IsEmpty()
                                   ? CalcAnimation(tag.paramsReal[0], style.fontAngleX, fAnimate)
                                   : org.fontAngleX;
                break;
            case SSA_fry:
                style.fontAngleY = !tag.paramsReal.IsEmpty()
                                   ? CalcAnimation(tag.paramsReal[0], style.fontAngleY, fAnimate)
                                   : org.fontAngleY;
                break;
            case SSA_frz:
            case SSA_fr:
                style.fontAngleZ = !tag.paramsReal.IsEmpty()
                                   ? CalcAnimation(tag.paramsReal[0], style.fontAngleZ, fAnimate)
                                   : org.fontAngleZ;
                break;
            case SSA_fscx:
                if (!tag.paramsReal.IsEmpty()) {
                    double n = CalcAnimation(tag.paramsReal[0], style.fontScaleX, fAnimate);
                    style.fontScaleX = (n < 0 ? 0 : n);
                } else {
                    style.fontScaleX = org.fontScaleX;
                }
                break;
            case SSA_fscy:
                if (!tag.paramsReal.IsEmpty()) {
                    double n = CalcAnimation(tag.paramsReal[0], style.fontScaleY, fAnimate);
                    style.fontScaleY = (n < 0 ? 0 : n);
                } else {
                    style.fontScaleY = org.fontScaleY;
                }
                break;
            case SSA_fsc:
                style.fontScaleX = org.fontScaleX;
                style.fontScaleY = org.fontScaleY;
                break;
            case SSA_fsp:
                style.fontSpacing = !tag.paramsReal.IsEmpty()
                                    ? CalcAnimation(tag.paramsReal[0], style.fontSpacing, fAnimate)
                                    : org.fontSpacing;
                break;
            case SSA_fs:
                if (!tag.paramsInt.IsEmpty()) {
                    if (!tag.params.IsEmpty() && (tag.params[0][0] == L'-' || tag.params[0][0] == L'+')) {
                        double n = CalcAnimation(style.fontSize + style.fontSize * tag.paramsInt[0] / 10, style.fontSize, fAnimate);
                        style.fontSize = (n > 0) ? n : org.fontSize;
                    } else {
                        double n = CalcAnimation(tag.paramsInt[0], style.fontSize, fAnimate);
                        style.fontSize = (n > 0) ? n : org.fontSize;
                    }
                } else {
                    style.fontSize = org.fontSize;
                }
                break;
            case SSA_i: {
                int n = !tag.paramsInt.IsEmpty() ? tag.paramsInt[0] : -1;
                style.fItalic = (n == 0 ? false : n == 1 ? true : org.fItalic);
            }
            break;
            case SSA_kt:
                sub->m_bIsAnimated = true;

                m_kstart = !tag.paramsInt.IsEmpty()
                           ? tag.paramsInt[0] * 10
                           : 0;
                m_kend = m_kstart;
                break;
            case SSA_kf:
            case SSA_K:
                sub->m_bIsAnimated = true;

                m_ktype = 1;
                m_kstart = m_kend;
                m_kend += !tag.paramsInt.IsEmpty()
                          ? tag.paramsInt[0] * 10
                          : 1000;
                break;
            case SSA_ko:
                sub->m_bIsAnimated = true;

                m_ktype = 2;
                m_kstart = m_kend;
                m_kend += !tag.paramsInt.IsEmpty()
                          ? tag.paramsInt[0] * 10
                          : 1000;
                break;
            case SSA_k:
                sub->m_bIsAnimated = true;

                m_ktype = 0;
                m_kstart = m_kend;
                m_kend += !tag.paramsInt.IsEmpty()
                          ? tag.paramsInt[0] * 10
                          : 1000;
                break;
            case SSA_move: // {\move(x1=param[0], y1=param[1], x2=param[2], y2=param[3][, t1=t[0], t2=t[1]])}
                sub->m_bIsAnimated = true;

                if (tag.paramsReal.GetCount() == 4 && !sub->m_effects[EF_MOVE]) {
                    if (Effect* e = DEBUG_NEW Effect) {
                        e->param[0] = std::lround(sub->m_total_scale_x * tag.paramsReal[0] * 8.0);
                        e->param[1] = std::lround(sub->m_total_scale_y * tag.paramsReal[1] * 8.0);
                        e->param[2] = std::lround(sub->m_total_scale_x * tag.paramsReal[2] * 8.0);
                        e->param[3] = std::lround(sub->m_total_scale_y * tag.paramsReal[3] * 8.0);
                        e->t[0] = e->t[1] = -1;

                        if (tag.paramsInt.GetCount() == 2) {
                            for (size_t k = 0; k < 2; k++) {
                                e->t[k] = tag.paramsInt[k];
                            }
                        }

                        sub->m_effects[EF_MOVE] = e;
                    }
                }
                break;
            case SSA_org: // {\org(x=param[0], y=param[1])}
                if (tag.paramsReal.GetCount() == 2 && !sub->m_effects[EF_ORG]) {
                    if (Effect* e = DEBUG_NEW Effect) {
                        e->param[0] = std::lround(sub->m_total_scale_x * tag.paramsReal[0] * 8.0);
                        e->param[1] = std::lround(sub->m_total_scale_y * tag.paramsReal[1] * 8.0);

                        if (sub->m_relativeTo == STSStyle::VIDEO) {
                            e->param[0] += m_vidrect.left;
                            e->param[1] += m_vidrect.top;
                        }

                        sub->m_effects[EF_ORG] = e;
                    }
                }
                break;
            case SSA_pbo:
                m_polygonBaselineOffset = !tag.paramsInt.IsEmpty() ? tag.paramsInt[0] : 0;
                break;
            case SSA_pos:
                if (tag.paramsReal.GetCount() == 2 && !sub->m_effects[EF_MOVE]) {
                    if (Effect* e = DEBUG_NEW Effect) {
                        e->param[0] = e->param[2] = std::lround(sub->m_total_scale_x * tag.paramsReal[0] * 8.0);
                        e->param[1] = e->param[3] = std::lround(sub->m_total_scale_y * tag.paramsReal[1] * 8.0);
                        e->t[0] = e->t[1] = 0;

                        sub->m_effects[EF_MOVE] = e;
                    }
                }
                break;
            case SSA_p: {
                int n = !tag.paramsInt.IsEmpty() ? tag.paramsInt[0] : 0;
                m_nPolygon = (n <= 0 ? 0 : n);
            }
            break;
            case SSA_q: {
                int n = !tag.paramsInt.IsEmpty() ? tag.paramsInt[0] : -1;
                sub->m_wrapStyle = (0 <= n && n <= 3)
                                   ? n
                                   : m_defaultWrapStyle;
            }
            break;
            case SSA_r:
                if (tag.params[0].IsEmpty() || !GetStyle(tag.params[0], style)) {
                    style = org;
                }
                break;
            case SSA_shad:
                if (!tag.paramsReal.IsEmpty()) {
                    double nx = CalcAnimation(tag.paramsReal[0], style.shadowDepthX, fAnimate);
                    style.shadowDepthX = (nx < 0 ? 0 : nx);
                    double ny = CalcAnimation(tag.paramsReal[0], style.shadowDepthY, fAnimate);
                    style.shadowDepthY = (ny < 0 ? 0 : ny);
                } else {
                    style.shadowDepthX = org.shadowDepthX;
                    style.shadowDepthY = org.shadowDepthY;
                }
                break;
            case SSA_s: {
                int n = !tag.paramsInt.IsEmpty() ? tag.paramsInt[0] : -1;
                style.fStrikeOut = (n == 0 ? false : n == 1 ? true : org.fStrikeOut);
            }
            break;
            case SSA_t: // \t([<t1>,<t2>,][<accel>,]<style modifiers>)
                if (tag.subTagsList) {
                    sub->m_bIsAnimated = true;

                    m_animStart = m_animEnd = 0;
                    m_animAccel = 1;

                    size_t nParams = tag.paramsInt.GetCount() + tag.paramsReal.GetCount();
                    if (nParams == 1) {
                        m_animAccel = tag.paramsReal[0];
                    } else if (nParams == 2) {
                        m_animStart = (int)tag.paramsReal[0];
                        m_animEnd = (int)tag.paramsReal[1];
                    } else if (nParams == 3) {
                        m_animStart = tag.paramsInt[0];
                        m_animEnd = tag.paramsInt[1];
                        m_animAccel = tag.paramsReal[0];
                    }

                    CreateSubFromSSATag(sub, tag.subTagsList, style, org, true);

                    sub->m_fAnimated = true;
                }
                break;
            case SSA_u: {
                int n = !tag.paramsInt.IsEmpty() ? tag.paramsInt[0] : -1;
                style.fUnderline = (n == 0 ? false : n == 1 ? true : org.fUnderline);
            }
            break;
            case SSA_xbord:
                if (!tag.paramsReal.IsEmpty()) {
                    double nx = CalcAnimation(tag.paramsReal[0], style.outlineWidthX, fAnimate);
                    style.outlineWidthX = (nx < 0 ? 0 : nx);
                } else {
                    style.outlineWidthX = org.outlineWidthX;
                }
                break;
            case SSA_xshad:
                style.shadowDepthX = !tag.paramsReal.IsEmpty()
                                     ? CalcAnimation(tag.paramsReal[0], style.shadowDepthX, fAnimate)
                                     : org.shadowDepthX;
                break;
            case SSA_ybord:
                if (!tag.paramsReal.IsEmpty()) {
                    double ny = CalcAnimation(tag.paramsReal[0], style.outlineWidthY, fAnimate);
                    style.outlineWidthY = (ny < 0 ? 0 : ny);
                } else {
                    style.outlineWidthY = org.outlineWidthY;
                }
                break;
            case SSA_yshad:
                style.shadowDepthY = !tag.paramsReal.IsEmpty()
                                     ? CalcAnimation(tag.paramsReal[0], style.shadowDepthY, fAnimate)
                                     : org.shadowDepthY;
                break;
        }
    }

    return true;
}

bool CRenderedTextSubtitle::ParseHtmlTag(CSubtitle* sub, CStringW str, STSStyle& style, const STSStyle& org)
{
    if (str.Find(L"!--") == 0) {
        return true;
    }

    bool fClosing = str[0] == '/';
    str.Trim(L" /");

    int i = str.Find(' ');
    if (i < 0) {
        i = str.GetLength();
    }

    CStringW tag = str.Left(i).MakeLower();
    str = str.Mid(i).Trim();

    CAtlArray<CStringW> attribs, params;
    while ((i = str.Find('=')) > 0) {
        attribs.Add(str.Left(i).Trim().MakeLower());
        str = str.Mid(i + 1);
        for (i = 0; _istspace(str[i]); i++) {
            ;
        }
        str = str.Mid(i);
        if (str[0] == '\"') {
            str = str.Mid(1);
            i = str.Find('\"');
        } else {
            i = str.Find(' ');
        }
        if (i < 0) {
            i = str.GetLength();
        }
        params.Add(str.Left(i).Trim().MakeLower());
        str = str.Mid(i + 1);
    }

    if (tag == L"text" || tag == L"span") {
        ;
    } else if (tag == L"b" || tag == L"strong") {
        style.fontWeight = !fClosing ? FW_BOLD : org.fontWeight;
    } else if (tag == L"i" || tag == L"em") {
        style.fItalic = !fClosing ? true : org.fItalic;
    } else if (tag == L"u") {
        style.fUnderline = !fClosing ? true : org.fUnderline;
    } else if (tag == L"s" || tag == L"strike" || tag == L"del") {
        style.fStrikeOut = !fClosing ? true : org.fStrikeOut;
    } else if (tag == L"font") {
        if (!fClosing) {
            for (size_t j = 0; j < attribs.GetCount(); j++) {
                if (params[j].IsEmpty()) {
                    continue;
                }

                int nColor = -1;

                if (attribs[j] == L"face") {
                    style.fontName = params[j];
                } else if (attribs[j] == L"size") {
                    if (params[j][0] == '+') {
                        style.fontSize += wcstol(params[j], nullptr, 10);
                    } else if (params[j][0] == '-') {
                        style.fontSize -= wcstol(params[j], nullptr, 10);
                    } else {
                        style.fontSize = wcstol(params[j], nullptr, 10);
                    }
                } else if (attribs[j] == L"color") {
                    nColor = 0;
                } else if (attribs[j] == L"outline-color") {
                    nColor = 2;
                } else if (attribs[j] == L"outline-level") {
                    style.outlineWidthX = style.outlineWidthY = wcstol(params[j], nullptr, 10);
                } else if (attribs[j] == L"shadow-color") {
                    nColor = 3;
                } else if (attribs[j] == L"shadow-level") {
                    style.shadowDepthX = style.shadowDepthY = wcstol(params[j], nullptr, 10);
                }

                if (nColor >= 0 && nColor < 4) {
                    CString key = WToT(params[j]).TrimLeft('#');
                    DWORD val;
                    if (g_colors.Lookup(key, val)) {
                        style.colors[nColor] = val;
                    } else if ((style.colors[nColor] = _tcstol(key, nullptr, 16)) == 0) {
                        style.colors[nColor] = 0x00ffffff;    // default is white
                    }
                    style.colors[nColor] = ((style.colors[nColor] >> 16) & 0xff) | ((style.colors[nColor] & 0xff) << 16) | (style.colors[nColor] & 0x00ff00);
                }
            }
        } else {
            style.fontName = org.fontName;
            style.fontSize = org.fontSize;
            style.colors = org.colors;
        }
    } else if (tag == L"k" && attribs.GetCount() == 1 && attribs[0] == L"t") {
        m_ktype = 1;
        m_kstart = m_kend;
        m_kend += wcstol(params[0], nullptr, 10);
    } else {
        return false;
    }

    return true;
}

double CRenderedTextSubtitle::CalcAnimation(double dst, double src, bool fAnimate)
{
    int s = m_animStart ? m_animStart : 0;
    int e = m_animEnd ? m_animEnd : m_delay;

    if (fabs(dst - src) >= 0.0001 && fAnimate) {
        if (m_time < s) {
            dst = src;
        } else if (s <= m_time && m_time < e) {
            double t = pow(1.0 * (m_time - s) / (e - s), m_animAccel);
            dst = (1 - t) * src + t * dst;
        }
        //      else dst = dst;
    }

    return dst;
}

CSubtitle* CRenderedTextSubtitle::GetSubtitle(int entry)
{
    CAutoLock cAutoLock(&renderLock);
    CSubtitle* sub;
    if (m_subtitleCache.Lookup(entry, sub)) {
        if (sub->m_fAnimated) {
            delete sub;
            sub = nullptr;
        } else {
            return sub;
        }
    }

    try {
        sub = DEBUG_NEW CSubtitle(m_renderingCaches);
    } catch (CMemoryException* e) {
        e->Delete();
        return nullptr;
    }

    CStringW str = GetStrW(entry, true);

    if (m_playRes.cx <= 0 || m_playRes.cy <= 0) {
        ASSERT(false);
        m_playRes = CSize(384, 288);
    }
    if (m_storageRes.cx <= 0 || m_storageRes.cy <= 0) {
        ASSERT(false);
        m_storageRes = m_playRes;
    }

    sub->m_allowLinePadding = (m_subtitleType != Subtitle::ASS && m_subtitleType != Subtitle::SSA);

    STSStyle stss;
    if (m_SubRendererSettings.overrideAllStyles) {
        stss = m_SubRendererSettings.defaultStyle;
        UpdateSubRelativeTo(m_subtitleType, stss.relativeTo);
    } else {
        // find the appropriate embedded style
        GetStyle(entry, stss);
        if (m_bOverridePlacement) {
            // Apply override placement to embedded style
            stss.scrAlignment = 2;
            LONG mw = m_storageRes.cx - stss.marginRect.left - stss.marginRect.right;
            stss.marginRect.bottom = std::lround(m_storageRes.cy - m_storageRes.cy * m_overridePlacement.cy / 100.0);
            // We need to set top margin, otherwise subtitles outside video frame will be clipped. Support up to 3 lines of subtitles. Should be enough.
            stss.marginRect.top    = m_storageRes.cy - (stss.marginRect.bottom + std::lround(stss.fontSize * 3.0));
            stss.marginRect.left   = std::lround(m_storageRes.cx * m_overridePlacement.cx / 100.0 - mw / 2.0);
            stss.marginRect.right  = m_storageRes.cx - (stss.marginRect.left + mw);
        }
    }

    double dFontScaleXCompensation = 1.0;
    double dFontScaleYCompensation = 1.0;

    if (m_ePARCompensationType == EPCTAccurateSize_ISR || m_ePARCompensationType == EPCTAccurateSize) {
        dFontScaleXCompensation = m_dPARCompensation;
    } else if (m_ePARCompensationType == EPCTUpscale) {
        if (m_dPARCompensation < 1.0) {
            dFontScaleYCompensation = 1.0 / m_dPARCompensation;
        } else {
            dFontScaleXCompensation = m_dPARCompensation;
        }
    } else if (m_ePARCompensationType == EPCTDownscale) {
        if (m_dPARCompensation < 1.0) {
            dFontScaleXCompensation = m_dPARCompensation;
        } else {
            dFontScaleYCompensation = 1.0 / m_dPARCompensation;
        }
    }

    const CRenderersSettings& r = GetRenderersSettings();
    if (r.fontScaleOverride != 1.0) {
        stss.fontSize *= r.fontScaleOverride;
    }

    STSStyle orgstss = stss;

    sub->m_scrAlignment = -stss.scrAlignment;
    sub->m_wrapStyle = m_defaultWrapStyle;
    sub->m_fAnimated = false;
    sub->m_relativeTo = stss.relativeTo;
    sub->m_target_scale_x = m_vidrect.Width()  > 0 && m_storageRes.cx > 0 ? double((sub->m_relativeTo == STSStyle::VIDEO) ? m_vidrect.Width()  : m_size.cx) / (m_storageRes.cx * 8.0) : 1.0;
    sub->m_target_scale_y = m_vidrect.Height() > 0 && m_storageRes.cy > 0 ? double((sub->m_relativeTo == STSStyle::VIDEO) ? m_vidrect.Height() : m_size.cy) / (m_storageRes.cy * 8.0) : 1.0;
    if (m_playRes.cx == 0 || m_playRes.cy == 0 || m_playRes == m_storageRes) {
        sub->m_script_scale_x = 1.0;
        sub->m_script_scale_y = 1.0;
        sub->m_total_scale_x = sub->m_target_scale_x;
        sub->m_total_scale_y = sub->m_target_scale_y;
    } else {
        sub->m_script_scale_x = static_cast<double>(m_storageRes.cx) / m_playRes.cx;
        sub->m_script_scale_y = static_cast<double>(m_storageRes.cy) / m_playRes.cy;
        sub->m_total_scale_x = sub->m_target_scale_x * sub->m_script_scale_x;
        sub->m_total_scale_y = sub->m_target_scale_y * sub->m_script_scale_y;
    }

    const STSEntry& stse = GetAt(entry);
    CRect marginRect = stse.marginRect;
    if (marginRect.left == 0) {
        marginRect.left = orgstss.marginRect.left;
    }
    if (marginRect.top == 0) {
        marginRect.top = orgstss.marginRect.top;
    }
    if (marginRect.right == 0) {
        marginRect.right = orgstss.marginRect.right;
    }
    if (marginRect.bottom == 0) {
        marginRect.bottom = orgstss.marginRect.bottom;
    }

    marginRect.left   = std::lround(sub->m_total_scale_x * marginRect.left * 8.0);
    marginRect.top    = std::lround(sub->m_total_scale_y * marginRect.top * 8.0);
    marginRect.right  = std::lround(sub->m_total_scale_x * marginRect.right * 8.0);
    marginRect.bottom = std::lround(sub->m_total_scale_y * marginRect.bottom * 8.0);

    if (sub->m_relativeTo == STSStyle::VIDEO) {
        // Account for the user trying to fool the renderer by setting negative margins
        CRect clipRect = m_vidrect;
        if (marginRect.left < 0) {
            clipRect.left = std::max(0l, clipRect.left + marginRect.left);
        }
        if (marginRect.top < 0) {
            clipRect.top = std::max(0l, clipRect.top + marginRect.top);
        }
        if (marginRect.right < 0) {
            clipRect.right = std::min(m_size.cx, clipRect.right - marginRect.right);
        }
        if (marginRect.bottom < 0) {
            clipRect.bottom = std::min(m_size.cy, clipRect.bottom - marginRect.bottom);
        }

        sub->m_clip.SetRect(clipRect.left >> 3, clipRect.top >> 3, clipRect.right >> 3, clipRect.bottom >> 3);
    } else {
        sub->m_clip.SetRect(0, 0, m_size.cx >> 3, m_size.cy >> 3);
    }

    if (sub->m_relativeTo == STSStyle::VIDEO) {
        marginRect.left   += m_vidrect.left;
        marginRect.top    += m_vidrect.top;
        marginRect.right  += m_size.cx - m_vidrect.right;
        marginRect.bottom += m_size.cy - m_vidrect.bottom;
    }

    m_animStart = m_animEnd = 0;
    m_animAccel = 1;
    m_ktype = m_kstart = m_kend = 0;
    m_nPolygon = 0;
    m_polygonBaselineOffset = 0;
    ParseEffect(sub, stse.effect);

    for (int iStart = 0, iEnd; iStart < str.GetLength(); iStart = iEnd) {
        bool bParsed = false;

        if (str[iStart] == L'{' && (iEnd = str.Find(L'}', iStart)) > 0) {
            SSATagsList tagsList;
            bParsed = ParseSSATag(tagsList, str.Mid(iStart + 1, iEnd - iStart - 1));
            if (bParsed) {
                CreateSubFromSSATag(sub, tagsList, stss, orgstss);
                iStart = iEnd + 1;
            }
        } else if (str[iStart] == L'<' && (iEnd = str.Find(L'>', iStart)) > 0) {
            bParsed = ParseHtmlTag(sub, str.Mid(iStart + 1, iEnd - iStart - 1), stss, orgstss);
            if (bParsed) {
                iStart = iEnd + 1;
            }
        }

        if (bParsed) {
            iEnd = FindOneOf(str, L"{<", iStart);
            if (iEnd < 0) {
                iEnd = str.GetLength();
            }
            if (iEnd == iStart) {
                continue;
            }
        } else {
            iEnd = FindOneOf(str, L"{<", iStart + 1);
            if (iEnd < 0) {
                iEnd = str.GetLength();
            }
        }

        STSStyle tmp = stss;

        tmp.fontSize      *= sub->m_total_scale_y * 64.0;
        tmp.fontSpacing   *= sub->m_total_scale_x * 64.0;
        if (m_scaledBAS == 1) {
            tmp.outlineWidthX *= sub->m_total_scale_x * 8.0;
            tmp.outlineWidthY *= sub->m_total_scale_y * 8.0;
            tmp.shadowDepthX  *= sub->m_total_scale_x * 8.0;
            tmp.shadowDepthY  *= sub->m_total_scale_y * 8.0;
        } else if (m_scaledBAS == -1 && m_layoutRes.cx == 0 && sub->m_script_scale_y <= 0.9 && (m_subtitleType == Subtitle::ASS || m_subtitleType == Subtitle::SSA)) {
            // If PlayRes is bigger than video, it usually is a buggy script where ScaledBorderAndShadow was intended
            tmp.outlineWidthX *= sub->m_total_scale_x * 8.0;
            tmp.outlineWidthY *= sub->m_total_scale_y * 8.0;
            tmp.shadowDepthX  *= sub->m_total_scale_x * 8.0;
            tmp.shadowDepthY  *= sub->m_total_scale_y * 8.0;
        } else {
            tmp.outlineWidthX *= 8.0;
            tmp.outlineWidthY *= 8.0;
            tmp.shadowDepthX  *= 8.0;
            tmp.shadowDepthY  *= 8.0;
        }
        tmp.fGaussianBlur *= sub->m_target_scale_y;

        if (m_nPolygon) {
            ParsePolygon(sub, str.Mid(iStart, iEnd - iStart), tmp);
        } else {
            if (m_ePARCompensationType != EPCTDisabled) {
                tmp.fontScaleX *= dFontScaleXCompensation;
                tmp.fontScaleY *= dFontScaleYCompensation;
            }

            ParseString(sub, str.Mid(iStart, iEnd - iStart), tmp);
        }
    }

    // just a "work-around" solution... in most cases nobody will want to use \org together with moving but without rotating the subs
    if (sub->m_effects[EF_ORG] && (sub->m_effects[EF_MOVE] || sub->m_effects[EF_BANNER] || sub->m_effects[EF_SCROLL])) {
        sub->m_fAnimated = true;
    }

    sub->m_scrAlignment = abs(sub->m_scrAlignment);

    sub->CreateClippers(m_size);

    sub->MakeLines(m_size, marginRect);

    m_subtitleCache[entry] = sub;

    return sub;
}

//

STDMETHODIMP CRenderedTextSubtitle::NonDelegatingQueryInterface(REFIID riid, void** ppv)
{
    CheckPointer(ppv, E_POINTER);
    *ppv = nullptr;

    return
        QI(IPersist)
        QI(ISubStream)
        QI(ISubPicProvider)
        __super::NonDelegatingQueryInterface(riid, ppv);
}

// ISubPicProvider

STDMETHODIMP_(POSITION) CRenderedTextSubtitle::GetStartPosition(REFERENCE_TIME rt, double fps)
{
#if USE_LIBASS
    if (m_LibassContext.IsLibassActive()) {
        return m_LibassContext.GetStartPosition(rt, fps);
    }
#endif

    int iSegment = -1;
    SearchSubs(rt, fps, &iSegment, nullptr);

    if (iSegment < 0) {
        iSegment = 0;
    }

    __assume((INT_PTR)iSegment >= INT_MIN && (INT_PTR)iSegment <= INT_MAX);
    return GetNext((POSITION)(INT_PTR)iSegment);
}

CString CRenderedTextSubtitle::GetPath() {
    if (m_provider == _T("Local") && !m_path.IsEmpty()) return m_path;
    else return _T("");
}

STDMETHODIMP_(POSITION) CRenderedTextSubtitle::GetNext(POSITION pos)
{
#if USE_LIBASS
    if (m_LibassContext.IsLibassActive()) {
        return m_LibassContext.GetNext(pos);
    }
#endif

    __assume((INT_PTR)pos >= INT_MIN && (INT_PTR)pos <= INT_MAX);
    int iSegment = (int)(INT_PTR)pos;

    const STSSegment* stss = GetSegment(iSegment);
    while (stss && stss->subs.IsEmpty()) {
        iSegment++;
        stss = GetSegment(iSegment);
    }

    return (stss ? (POSITION)(INT_PTR)(iSegment + 1) : nullptr);
}

STDMETHODIMP_(REFERENCE_TIME) CRenderedTextSubtitle::GetStart(POSITION pos, double fps)
{
#if USE_LIBASS
    if (m_LibassContext.IsLibassActive()) {
        return m_LibassContext.GetCurrent(pos);
    }
#endif
    __assume((INT_PTR)pos - 1 >= INT_MIN && (INT_PTR)pos <= INT_MAX);
    return TranslateSegmentStart((int)(INT_PTR)pos - 1, fps);
}

STDMETHODIMP_(REFERENCE_TIME) CRenderedTextSubtitle::GetStop(POSITION pos, double fps)
{
#if USE_LIBASS
    if (m_LibassContext.IsLibassActive()) {
        return m_LibassContext.GetCurrent(pos) + 1;
    }
#endif
    __assume((INT_PTR)pos - 1 >= INT_MIN && (INT_PTR)pos <= INT_MAX);
    return TranslateSegmentEnd((int)(INT_PTR)pos - 1, fps);
}

STDMETHODIMP_(bool) CRenderedTextSubtitle::IsAnimated(POSITION pos)
{
#if USE_LIBASS
    if (m_LibassContext.IsLibassActive()) {
        return false;
    }
#endif
    __assume((INT_PTR)pos - 1 >= INT_MIN && (INT_PTR)pos <= INT_MAX);
    int iSegment = (int)(INT_PTR)pos - 1;

    const STSSegment* stss = GetSegment(iSegment);
    if (stss) {
        for (size_t i = 0, count = stss->subs.GetCount(); i < count; i++) {
            CSubtitle* sub = GetSubtitle(stss->subs[i]);
            if (sub && sub->m_bIsAnimated) {
                return true;
            }
        }
    }

    return false;
}

struct LSub {
    int idx, layer, readorder;

    bool operator <(const LSub& rhs) const {
        return (layer == rhs.layer) ? readorder < rhs.readorder : layer < rhs.layer;
    }
};

STDMETHODIMP CRenderedTextSubtitle::Render(SubPicDesc& spd, REFERENCE_TIME rt, double fps, RECT& bbox)
{
    CAutoLock cAutoLock(&renderLock);
    //TRACE(_T("render sub start: %lld\n"), rt);

    if (!spd.vidrect.right) {
        // video size is not known yet
        return S_FALSE;
    }

#if USE_LIBASS
    HRESULT libassResult = m_LibassContext.Render(rt, spd, bbox, m_size, m_vidrect);
    if (libassResult != E_POINTER) { //libass not initialized
        return libassResult;
    }
#endif

    Init(CSize(spd.w, spd.h), spd.vidrect);

    int segment;
    const STSSegment* stss = SearchSubs(rt, fps, &segment);
    if (!stss) {
        //TRACE(_T("render sub skipped: %lld\n"), rt);
        return S_FALSE;
    }

    CRect bbox2(0, 0, 0, 0);

    // clear any cached subs that is behind current time
    {
        POSITION pos = m_subtitleCache.GetStartPosition();
        while (pos) {
            int entry;
            CSubtitle* pSub;
            m_subtitleCache.GetNextAssoc(pos, entry, pSub);

            STSEntry& stse = GetAt(entry);
            if (stse.end < rt) {
                delete pSub;
                m_subtitleCache.RemoveKey(entry);
            }
        }
    }

    m_sla.AdvanceToSegment(segment, stss->subs);

    CAtlArray<LSub> subs;

    for (ptrdiff_t i = 0, j = stss->subs.GetCount(); i < j; i++) {
        const auto idx = stss->subs[i];
        const auto& sts_entry = GetAt(idx);
        subs.Add({ idx, sts_entry.layer, sts_entry.readorder });
    }

    std::sort(subs.GetData(), subs.GetData() + subs.GetCount());

    for (ptrdiff_t i = 0, j = subs.GetCount(); i < j; i++) {
        int entry = subs[i].idx;

        STSEntry stse = GetAt(entry);

        {
            REFERENCE_TIME start = TranslateStart(entry, fps);
            m_time = (int)RT2MS(rt - start);
            m_delay = (int)RT2MS(TranslateEnd(entry, fps) - start);
        }

        CSubtitle* s = GetSubtitle(entry);
        if (!s) {
            continue;
        }

        CRect clipRect = s->m_clip;
        CRect r = s->m_rect;
        CSize spaceNeeded = r.Size();

        // apply the effects

        bool fPosOverride = false, fOrgOverride = false;

        int alpha = 0x00;

        CPoint org2;

        BYTE* pAlphaMask = nullptr;

        if (s->m_pClipper) {
            const auto& ptr = s->m_pClipper->GetAlphaMask(s->m_pClipper);
            if (ptr) {
                pAlphaMask = ptr->get();
            }
        }

        for (int k = 0; k < EF_NUMBEROFEFFECTS; k++) {
            if (!s->m_effects[k]) {
                continue;
            }

            switch (k) {
                case EF_MOVE: { // {\move(x1=param[0], y1=param[1], x2=param[2], y2=param[3], t1=t[0], t2=t[1])}
                    CPoint p;
                    CPoint p1(s->m_effects[k]->param[0], s->m_effects[k]->param[1]);
                    CPoint p2(s->m_effects[k]->param[2], s->m_effects[k]->param[3]);
                    int t1 = s->m_effects[k]->t[0];
                    int t2 = s->m_effects[k]->t[1];

                    if (t2 < t1) {
                        std::swap(t1, t2);
                    }

                    if (t1 <= 0 && t2 <= 0) {
                        t1 = 0;
                        t2 = m_delay;
                    }

                    if (m_time <= t1) {
                        p = p1;
                    } else if (p1 == p2) {
                        p = p1;
                    } else if (t1 < m_time && m_time < t2) {
                        double t = 1.0 * (m_time - t1) / (t2 - t1);
                        p.x = (int)((1 - t) * p1.x + t * p2.x);
                        p.y = (int)((1 - t) * p1.y + t * p2.y);
                    } else {
                        p = p2;
                    }
                    r = CRect(
                            CPoint((s->m_scrAlignment % 3) == 1 ? p.x : (s->m_scrAlignment % 3) == 0 ? p.x - spaceNeeded.cx : p.x - (spaceNeeded.cx + 1) / 2,
                                   s->m_scrAlignment <= 3 ? p.y - spaceNeeded.cy : s->m_scrAlignment <= 6 ? p.y - (spaceNeeded.cy + 1) / 2 : p.y),
                            spaceNeeded);

                    if (s->m_relativeTo == STSStyle::VIDEO) {
                        r.OffsetRect(m_vidrect.TopLeft());
                    }
                    fPosOverride = true;
                }
                break;
                case EF_ORG: { // {\org(x=param[0], y=param[1])}
                    org2 = CPoint(s->m_effects[k]->param[0], s->m_effects[k]->param[1]);
                    fOrgOverride = true;
                }
                break;
                case EF_FADE: { // {\fade(a1=param[0], a2=param[1], a3=param[2], t1=t[0], t2=t[1], t3=t[2], t4=t[3]) or {\fad(t1=t[1], t2=t[2])
                    int t1 = s->m_effects[k]->t[0];
                    int t2 = s->m_effects[k]->t[1];
                    int t3 = s->m_effects[k]->t[2];
                    int t4 = s->m_effects[k]->t[3];

                    if (t1 == -1 && t4 == -1) {
                        t1 = 0;
                        t3 = m_delay - t3;
                        t4 = m_delay;
                    }

                    if (m_time < t1) {
                        alpha = s->m_effects[k]->param[0];
                    } else if (m_time >= t1 && m_time < t2) {
                        double t = 1.0 * (m_time - t1) / (t2 - t1);
                        alpha = (int)(s->m_effects[k]->param[0] * (1 - t) + s->m_effects[k]->param[1] * t);
                    } else if (m_time >= t2 && m_time < t3) {
                        alpha = s->m_effects[k]->param[1];
                    } else if (m_time >= t3 && m_time < t4) {
                        double t = 1.0 * (m_time - t3) / (t4 - t3);
                        alpha = (int)(s->m_effects[k]->param[1] * (1 - t) + s->m_effects[k]->param[2] * t);
                    } else if (m_time >= t4) {
                        alpha = s->m_effects[k]->param[2];
                    }
                }
                break;
                case EF_BANNER: { // Banner;delay=param[0][;leftoright=param[1];fadeawaywidth=param[2]]
                    int left = (s->m_relativeTo == STSStyle::VIDEO) ? m_vidrect.left : 0,
                        right = (s->m_relativeTo == STSStyle::VIDEO) ? m_vidrect.right : m_size.cx;

                    r.left = !!s->m_effects[k]->param[1]
                             ? (left/*marginRect.left*/ - spaceNeeded.cx) + (int)(m_time * 8.0 / s->m_effects[k]->param[0])
                             : (right /*- marginRect.right*/) - (int)(m_time * 8.0 / s->m_effects[k]->param[0]);

                    r.right = r.left + spaceNeeded.cx;

                    clipRect &= CRect(left >> 3, clipRect.top, right >> 3, clipRect.bottom);

                    fPosOverride = true;
                }
                break;
                case EF_SCROLL: { // Scroll up/down(toptobottom=param[3]);top=param[0];bottom=param[1];delay=param[2][;fadeawayheight=param[4]]
                    r.top = !!s->m_effects[k]->param[3]
                            ? s->m_effects[k]->param[0] + (int)(m_time * 8.0 / s->m_effects[k]->param[2]) - spaceNeeded.cy
                            : s->m_effects[k]->param[1] - (int)(m_time * 8.0 / s->m_effects[k]->param[2]);

                    r.bottom = r.top + spaceNeeded.cy;

                    CRect cr(0, (s->m_effects[k]->param[0] + 4) >> 3, spd.w, (s->m_effects[k]->param[1] + 4) >> 3);

                    if (s->m_relativeTo == STSStyle::VIDEO) {
                        r.top += m_vidrect.top;
                        r.bottom += m_vidrect.top;
                        cr.top += m_vidrect.top >> 3;
                        cr.bottom += m_vidrect.top >> 3;
                    }

                    clipRect &= cr;

                    fPosOverride = true;
                }
                break;
                default:
                    ASSERT(FALSE); // Shouldn't happen
                    break;
            }
        }

        if (!fPosOverride && !fOrgOverride && !s->m_fAnimated) {
            r = m_sla.AllocRect(s, segment, entry, stse.layer, m_collisions);
        }

        CPoint org;
        org.x = (s->m_scrAlignment % 3) == 1 ? r.left : (s->m_scrAlignment % 3) == 2 ? r.CenterPoint().x : r.right;
        org.y = s->m_scrAlignment <= 3 ? r.bottom : s->m_scrAlignment <= 6 ? r.CenterPoint().y : r.top;

        if (!fOrgOverride) {
            org2 = org;
        }

        // Rectangles for inverse clip
        CRect iclipRect[4];
        iclipRect[0] = CRect(0, 0, spd.w, clipRect.top);
        iclipRect[1] = CRect(0, clipRect.top, clipRect.left, clipRect.bottom);
        iclipRect[2] = CRect(clipRect.right, clipRect.top, spd.w, clipRect.bottom);
        iclipRect[3] = CRect(0, clipRect.bottom, spd.w, spd.h);

        bool multiLine = s->GetCount() > 1;
        int numPass = multiLine ? 2 : 1;
        for (int pass = 0; pass < numPass; pass++) {
            bool paintBG = (pass == 0);
            bool paintBody = (!multiLine || pass == 1);
            CPoint p(0, r.top);
            POSITION pos;

            pos = s->GetHeadPosition();
            while (pos) {
                CLine* l = s->GetNext(pos);

                p.y += l->m_linePadding; // always zero for first line
                p.x = (s->m_scrAlignment % 3) == 1 ? org.x
                    : (s->m_scrAlignment % 3) == 0 ? org.x - l->m_width
                    : org.x - (l->m_width / 2);

                if (s->m_clipInverse) {
                    if (paintBG) {
                        bbox2 |= l->PaintShadow(spd, iclipRect[0], pAlphaMask, p, org2, m_time, alpha);
                        bbox2 |= l->PaintShadow(spd, iclipRect[1], pAlphaMask, p, org2, m_time, alpha);
                        bbox2 |= l->PaintShadow(spd, iclipRect[2], pAlphaMask, p, org2, m_time, alpha);
                        bbox2 |= l->PaintShadow(spd, iclipRect[3], pAlphaMask, p, org2, m_time, alpha);
                        bbox2 |= l->PaintOutline(spd, iclipRect[0], pAlphaMask, p, org2, m_time, alpha);
                        bbox2 |= l->PaintOutline(spd, iclipRect[1], pAlphaMask, p, org2, m_time, alpha);
                        bbox2 |= l->PaintOutline(spd, iclipRect[2], pAlphaMask, p, org2, m_time, alpha);
                        bbox2 |= l->PaintOutline(spd, iclipRect[3], pAlphaMask, p, org2, m_time, alpha);
                    }
                    if (paintBody) {
                        bbox2 |= l->PaintBody(spd, iclipRect[0], pAlphaMask, p, org2, m_time, alpha);
                        bbox2 |= l->PaintBody(spd, iclipRect[1], pAlphaMask, p, org2, m_time, alpha);
                        bbox2 |= l->PaintBody(spd, iclipRect[2], pAlphaMask, p, org2, m_time, alpha);
                        bbox2 |= l->PaintBody(spd, iclipRect[3], pAlphaMask, p, org2, m_time, alpha);
                    }
                } else {
                    if (paintBG) {
                        auto lineBG = CLineBG::CLineBGFactory(l, m_renderingCaches);
                        if (lineBG) {
                            bbox2 |= lineBG->PaintLineShadow(spd, clipRect, pAlphaMask, p, org2, m_time, alpha);
                            bbox2 |= lineBG->PaintLineOutline(spd, clipRect, pAlphaMask, p, org2, m_time, alpha);
                        } else {
                            bbox2 |= l->PaintShadow(spd, clipRect, pAlphaMask, p, org2, m_time, alpha);
                            bbox2 |= l->PaintOutline(spd, clipRect, pAlphaMask, p, org2, m_time, alpha);
                        }
                    }
                    if (paintBody) {
                        bbox2 |= l->PaintBody(spd, clipRect, pAlphaMask, p, org2, m_time, alpha);
                    }
                }
                p.y += l->m_ascent + l->m_descent;
            }
        }
    }

    bbox = bbox2;

    //TRACE(_T("render sub done: %lld\n"), rt);
    return (subs.GetCount() && !bbox2.IsRectEmpty()) ? S_OK : S_FALSE;
}

// IPersist

STDMETHODIMP CRenderedTextSubtitle::GetClassID(CLSID* pClassID)
{
    return pClassID ? *pClassID = __uuidof(this), S_OK : E_POINTER;
}

// ISubStream

STDMETHODIMP_(int) CRenderedTextSubtitle::GetStreamCount()
{
    return 1;
}

STDMETHODIMP CRenderedTextSubtitle::GetStreamInfo(int iStream, WCHAR** ppName, LCID* pLCID)
{
    CheckPointer(ppName, E_POINTER);
    if (iStream != 0) {
        return E_INVALIDARG;
    }

    if (pLCID) {
        *pLCID = m_lcid;
    }

    CStringW strName;
    if (m_langname.IsEmpty()) {
        if (!m_provider.IsEmpty()) {
            strName.Format(L"[%s] %s", m_provider.GetString(), m_name.GetString());
        } else {
            strName.Format(L"%s", m_name.GetString());
        }
    } else {
        CString strLanguage;
        if (m_lcid && m_lcid != LCID(-1)) {
            WCHAR dispName[1024];
            memset(dispName, 0, 1024 * sizeof(WCHAR));
            if (0 == GetLocaleInfoEx(m_langname, LOCALE_SLOCALIZEDLANGUAGENAME, (LPWSTR)&dispName, 1024)) {
                int len = GetLocaleInfo(m_lcid, LOCALE_SENGLANGUAGE, strLanguage.GetBuffer(64), 64);
                strLanguage.ReleaseBufferSetLength(std::max(len - 1, 0));
            } else {
                strLanguage = dispName;
            }
        }

        if (strLanguage.IsEmpty()) {
            strLanguage = m_langname;
        }

        if (!strLanguage.IsEmpty() && m_eHearingImpaired == Subtitle::HI_YES) {
            strLanguage = strLanguage + L" [HI]";
        }

        if (!m_provider.IsEmpty()) {
            strName.Format(L"[%s] %s\t%s", m_provider.GetString(), m_name.GetString(), strLanguage.GetString());
        } else {
            strName.Format(L"%s\t%s", m_name.GetString(), strLanguage.GetString());
        }
    }

    *ppName = (WCHAR*)CoTaskMemAlloc((strName.GetLength() + 1) * sizeof(WCHAR));
    CheckPointer(*ppName, E_OUTOFMEMORY);
    wcscpy_s(*ppName, strName.GetLength() + 1, strName);

    return S_OK;
}

STDMETHODIMP_(int) CRenderedTextSubtitle::GetStream()
{
    return 0;
}

STDMETHODIMP CRenderedTextSubtitle::SetStream(int iStream)
{
    return iStream == 0 ? S_OK : E_FAIL;
}

STDMETHODIMP CRenderedTextSubtitle::Reload()
{
    if (m_path.IsEmpty() || !PathUtils::Exists(m_path)) {
        return E_FAIL;
    }
    return Open(m_path, DEFAULT_CHARSET, m_name) ? S_OK : E_FAIL;
}

STDMETHODIMP CRenderedTextSubtitle::SetSourceTargetInfo(CString yuvVideoMatrix, int targetBlackLevel, int targetWhiteLevel)
{
    auto parseMatrixString = [&](const CString& input, ColorConvTable::YuvRangeType& yuvRange, ColorConvTable::YuvMatrixType& yuvMatrix) {
        int nPos = 0;
        CString range = input.Tokenize(_T("."), nPos);
        CString matrix = input.Mid(nPos);

        if (range == _T("PC")) {
            yuvRange = ColorConvTable::RANGE_PC;
        } else {
            yuvRange = ColorConvTable::RANGE_TV;
        }

        if (matrix == _T("709")) {
            yuvMatrix = ColorConvTable::BT709;
        } else if (matrix == _T("240M")) {
            yuvMatrix = ColorConvTable::BT709;
        } else if (matrix == _T("601")) {
            yuvMatrix = ColorConvTable::BT601;
        } else if (matrix == _T("2020")) {
            yuvMatrix = ColorConvTable::BT2020;
        } else {
            yuvMatrix = ColorConvTable::AUTO;
        }
    };

    ColorConvTable::YuvMatrixType video_matrix = ColorConvTable::AUTO;
    ColorConvTable::YuvRangeType video_range = ColorConvTable::RANGE_TV;

    yuvVideoMatrix.MakeUpper();
    if (!yuvVideoMatrix.IsEmpty() && yuvVideoMatrix != _T("NONE")) {
        parseMatrixString(yuvVideoMatrix, video_range, video_matrix);
    }

    bool bCorrect601to709 = false;
    if (m_subtitleType == Subtitle::ASS || m_subtitleType == Subtitle::SSA) {
        ColorConvTable::YuvMatrixType script_matrix = ColorConvTable::BT601;
        ColorConvTable::YuvRangeType script_range = ColorConvTable::RANGE_TV;

        if (!m_sYCbCrMatrix.IsEmpty()) {
            if (m_sYCbCrMatrix == _T("NONE")) {
                script_matrix = ColorConvTable::NONE_RGB;
                script_range = ColorConvTable::RANGE_PC;
            } else {
                parseMatrixString(m_sYCbCrMatrix, script_range, script_matrix);
            }
        }

        bCorrect601to709 = (script_matrix == ColorConvTable::BT601) && (video_matrix == ColorConvTable::BT709);
    }
    ColorConvTable::SetDefaultConvType(video_matrix, video_range, (targetWhiteLevel < 245), bCorrect601to709);

    return S_OK;
}

void CRenderedTextSubtitle::SetSubtitleTypeFromGUID(GUID subtype) {
    if (subtype== MEDIASUBTYPE_UTF8) {
        m_subtitleType = Subtitle::SRT;
    } else if (subtype == MEDIASUBTYPE_SSA) {
        m_subtitleType = Subtitle::SSA;
    } else if (subtype == MEDIASUBTYPE_ASS || subtype == MEDIASUBTYPE_ASS2) {
        m_subtitleType = Subtitle::ASS;
    } else if (subtype == MEDIASUBTYPE_WEBVTT) {
        m_subtitleType = Subtitle::VTT;
    }
}
