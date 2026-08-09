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

#include "stdafx.h"
#include "MemSubPic.h"

#include <emmintrin.h>
#include "stb/stb_image.h"
#include "stb/stb_image_resize2.h"

// color conv

static unsigned char clipBase[256 * 3];
static unsigned char* clip = clipBase + 256;

static const int c2y_cyb = std::lround(0.114 * 219 / 255 * 65536);
static const int c2y_cyg = std::lround(0.587 * 219 / 255 * 65536);
static const int c2y_cyr = std::lround(0.299 * 219 / 255 * 65536);
static const int c2y_cu = std::lround(1.0 / 2.018 * 1024);
static const int c2y_cv = std::lround(1.0 / 1.596 * 1024);

int c2y_yb[256];
int c2y_yg[256];
int c2y_yr[256];

static const int y2c_cbu = std::lround(2.018 * 65536);
static const int y2c_cgu = std::lround(0.391 * 65536);
static const int y2c_cgv = std::lround(0.813 * 65536);
static const int y2c_crv = std::lround(1.596 * 65536);
static int y2c_bu[256];
static int y2c_gu[256];
static int y2c_gv[256];
static int y2c_rv[256];

static const int cy_cy = std::lround(255.0 / 219.0 * 65536);
static const int cy_cy2 = std::lround(255.0 / 219.0 * 32768);

void ColorConvInit()
{
    static bool bColorConvInitOK = false;
    if (bColorConvInitOK) {
        return;
    }

    for (int i = 0; i < 256; i++) {
        clipBase[i] = 0;
        clipBase[i + 256] = BYTE(i);
        clipBase[i + 512] = 255;
    }

    for (int i = 0; i < 256; i++) {
        c2y_yb[i] = c2y_cyb * i;
        c2y_yg[i] = c2y_cyg * i;
        c2y_yr[i] = c2y_cyr * i;

        y2c_bu[i] = y2c_cbu * (i - 128);
        y2c_gu[i] = y2c_cgu * (i - 128);
        y2c_gv[i] = y2c_cgv * (i - 128);
        y2c_rv[i] = y2c_crv * (i - 128);
    }

    bColorConvInitOK = true;
}

//
// CMemSubPic
//

CMemSubPic::CMemSubPic(const SubPicDesc& spd, CMemSubPicAllocator* pAllocator)
    : m_pAllocator(pAllocator)
    , m_spd(spd)
{
    m_maxsize.SetSize(spd.w, spd.h);
    m_rcDirty.SetRect(0, 0, spd.w, spd.h);
}

CMemSubPic::~CMemSubPic()
{
    m_pAllocator->FreeSpdBits(m_spd);
    if (m_resizedSpd) {
        m_pAllocator->FreeSpdBits(*m_resizedSpd);
    }
}

// ISubPic

STDMETHODIMP_(void*) CMemSubPic::GetObject()
{
    return (void*)&m_spd;
}

STDMETHODIMP CMemSubPic::GetDesc(SubPicDesc& spd)
{
    spd.type = m_spd.type;
    spd.w = m_spd.w;
    spd.h = m_spd.h;
    spd.bpp = m_spd.bpp;
    spd.pitch = m_spd.pitch;
    spd.bits = m_spd.bits;
    spd.bitsU = m_spd.bitsU;
    spd.bitsV = m_spd.bitsV;
    spd.vidrect = m_vidrect;

    return S_OK;
}

STDMETHODIMP CMemSubPic::CopyTo(ISubPic* pSubPic)
{
    HRESULT hr;
    if (FAILED(hr = __super::CopyTo(pSubPic))) {
        return hr;
    }

    SubPicDesc src, dst;
    if (FAILED(GetDesc(src)) || FAILED(pSubPic->GetDesc(dst))) {
        return E_FAIL;
    }

    if (auto subPic = dynamic_cast<CMemSubPic*>(pSubPic)) {
        ASSERT(subPic->m_pAllocator == m_pAllocator);
        ASSERT(subPic->m_resizedSpd == nullptr);
        // Move because we are not going to reuse it.
        subPic->m_resizedSpd = std::move(m_resizedSpd);
    }

    int w = m_rcDirty.Width(), h = m_rcDirty.Height();
    BYTE* s = src.bits + src.pitch * m_rcDirty.top + m_rcDirty.left * 4;
    BYTE* d = dst.bits + dst.pitch * m_rcDirty.top + m_rcDirty.left * 4;

    for (ptrdiff_t j = 0; j < h; j++, s += src.pitch, d += dst.pitch) {
        memcpy(d, s, w * 4);
    }

    return S_OK;
}

STDMETHODIMP CMemSubPic::ClearDirtyRect()
{
    if (m_rcDirty.IsRectEmpty()) {
        return S_FALSE;
    }

    DWORD color = m_bInvAlpha ? 0x00000000 : 0xFF000000;

    BYTE* p = m_spd.bits + m_spd.pitch * m_rcDirty.top + m_rcDirty.left * (m_spd.bpp >> 3);
    for (ptrdiff_t j = 0, h = m_rcDirty.Height(); j < h; j++, p += m_spd.pitch) {
        int w = m_rcDirty.Width();
#ifdef _WIN64
        memsetd(p, color, w * 4); // nya
#else
        __asm {
            mov eax, color
            mov ecx, w
            mov edi, p
            cld
            rep stosd
        }
#endif
    }

    m_rcDirty.SetRectEmpty();

    return S_OK;
}

STDMETHODIMP CMemSubPic::Lock(SubPicDesc& spd)
{
    return GetDesc(spd);
}

HRESULT CMemSubPic::UnlockARGB() { //derived from Unlock(), supports ARGB
    m_rcDirty = CRect(0, 0, m_spd.w, m_spd.h);

    if (m_rcDirty.IsRectEmpty()) {
        return S_OK;
    }

    if (m_spd.bpp != 32 || m_spd.type != MSP_RGB32) {
        return E_INVALIDARG;
    }

    CRect r = m_spd.vidrect;
    CRect rcDirty = m_rcDirty;
    if (m_spd.h != r.Height() || m_spd.w != r.Width()) {
        if (!m_resizedSpd) {
            m_resizedSpd = std::unique_ptr<SubPicDesc>(DEBUG_NEW SubPicDesc);
        }

        m_resizedSpd->type = m_spd.type;
        m_resizedSpd->w = r.Width();
        m_resizedSpd->h = r.Height();
        m_resizedSpd->pitch = r.Width() * 4;
        m_resizedSpd->bpp = m_spd.bpp;
        m_resizedSpd->vidrect = { 0,0,r.Width(),r.Height() };

        if (!m_resizedSpd->bits) {
            m_pAllocator->AllocSpdBits(*m_resizedSpd);
        }

        auto& s = m_spd;
        auto& d = *m_resizedSpd;
        stbir_resize(s.bits, s.w, s.h, s.pitch, d.bits, d.w, d.h, d.pitch, STBIR_RGBA_PM, STBIR_TYPE_UINT8_SRGB, STBIR_EDGE_CLAMP, STBIR_FILTER_DEFAULT);
        TRACE("CMemSubPic: Resized SubPic %dx%d -> %dx%d\n", m_spd.w, m_spd.h, r.Width(), r.Height());

        // Set whole resized spd as dirty, we are not going to reuse it.
        rcDirty.SetRect(0, 0, m_resizedSpd->w, m_resizedSpd->h);
    }
    else if (m_resizedSpd) {
        // Resize is not needed so release m_resizedSpd.
        m_pAllocator->FreeSpdBits(*m_resizedSpd);
        m_resizedSpd = nullptr;
    }

    if (!m_resizedSpd) {
        m_rcDirty = rcDirty;
    }

    return S_OK;
}


STDMETHODIMP CMemSubPic::Unlock(RECT* pDirtyRect)
{
    m_rcDirty = pDirtyRect ? *pDirtyRect : CRect(0, 0, m_spd.w, m_spd.h);

    if (m_rcDirty.IsRectEmpty()) {
        return S_OK;
    }

    CRect r = m_spd.vidrect;
    CRect rcDirty = m_rcDirty;
    if (m_spd.h != r.Height() || m_spd.w != r.Width()) {
        if (!m_resizedSpd) {
            m_resizedSpd = std::unique_ptr<SubPicDesc>(DEBUG_NEW SubPicDesc);
        }

        m_resizedSpd->type = m_spd.type;
        m_resizedSpd->w = r.Width();
        m_resizedSpd->h = r.Height();
        m_resizedSpd->pitch = r.Width() * 4;
        m_resizedSpd->bpp = m_spd.bpp;

        if (!m_resizedSpd->bits) {
            m_pAllocator->AllocSpdBits(*m_resizedSpd);
        }

        // Straight (non-premultiplied) data: resize the four channels
        // independently, matching the former VDPixmapResample XRGB behavior.
        stbir_resize(m_spd.bits, m_spd.w, m_spd.h, m_spd.pitch,
                     m_resizedSpd->bits, m_resizedSpd->w, m_resizedSpd->h, m_resizedSpd->pitch,
                     STBIR_4CHANNEL, STBIR_TYPE_UINT8, STBIR_EDGE_CLAMP, STBIR_FILTER_CATMULLROM);
        TRACE("CMemSubPic: Resized SubPic %dx%d -> %dx%d\n", m_spd.w, m_spd.h, r.Width(), r.Height());

        // Set whole resized spd as dirty, we are not going to reuse it.
        rcDirty.SetRect(0, 0, m_resizedSpd->w, m_resizedSpd->h);
    } else if (m_resizedSpd) {
        // Resize is not needed so release m_resizedSpd.
        m_pAllocator->FreeSpdBits(*m_resizedSpd);
        m_resizedSpd = nullptr;
    }

    const SubPicDesc& subPic = m_resizedSpd ? *m_resizedSpd : m_spd;

    if (subPic.type == MSP_YUY2 || subPic.type == MSP_YV12 || subPic.type == MSP_IYUV || subPic.type == MSP_AYUV) {
        ColorConvInit();

        if (subPic.type == MSP_YUY2 || subPic.type == MSP_YV12 || subPic.type == MSP_IYUV) {
            rcDirty.left &= ~1;
            rcDirty.right = (rcDirty.right + 1) & ~1;

            if (subPic.type == MSP_YV12 || subPic.type == MSP_IYUV) {
                rcDirty.top &= ~1;
                rcDirty.bottom = (rcDirty.bottom + 1) & ~1;
            }
        }
    }

    if (!m_resizedSpd) {
        m_rcDirty = rcDirty;
    }

    int w = rcDirty.Width(), h = rcDirty.Height();
    BYTE* top = subPic.bits + subPic.pitch * rcDirty.top + rcDirty.left * 4;
    BYTE* bottom = top + subPic.pitch * h;

    if (subPic.type == MSP_RGB16) {
        for (; top < bottom ; top += subPic.pitch) {
            DWORD* s = (DWORD*)top;
            DWORD* e = s + w;
            for (; s < e; s++) {
                *s = ((*s >> 3) & 0x1f000000) | ((*s >> 8) & 0xf800) | ((*s >> 5) & 0x07e0) | ((*s >> 3) & 0x001f);
                //*s = (*s&0xff000000)|((*s>>8)&0xf800)|((*s>>5)&0x07e0)|((*s>>3)&0x001f);
            }
        }
    } else if (subPic.type == MSP_RGB15) {
        for (; top < bottom; top += subPic.pitch) {
            DWORD* s = (DWORD*)top;
            DWORD* e = s + w;
            for (; s < e; s++) {
                *s = ((*s >> 3) & 0x1f000000) | ((*s >> 9) & 0x7c00) | ((*s >> 6) & 0x03e0) | ((*s >> 3) & 0x001f);
                //*s = (*s&0xff000000)|((*s>>9)&0x7c00)|((*s>>6)&0x03e0)|((*s>>3)&0x001f);
            }
        }
    } else if (subPic.type == MSP_YUY2 || subPic.type == MSP_YV12 || subPic.type == MSP_IYUV) {
        for (; top < bottom ; top += subPic.pitch) {
            BYTE* s = top;
            BYTE* e = s + w * 4;
            for (; s < e; s += 8) { // ARGB ARGB -> AxYU AxYV
                if ((s[3] + s[7]) < 0x1fe) {
                    s[1] = BYTE((c2y_yb[s[0]] + c2y_yg[s[1]] + c2y_yr[s[2]] + 0x108000) >> 16);
                    s[5] = BYTE((c2y_yb[s[4]] + c2y_yg[s[5]] + c2y_yr[s[6]] + 0x108000) >> 16);

                    int scaled_y = (s[1] + s[5] - 32) * cy_cy2;

                    s[0] = clip[(((((s[0] + s[4]) << 15) - scaled_y) >> 10) * c2y_cu + 0x800000 + 0x8000) >> 16];
                    s[4] = clip[(((((s[2] + s[6]) << 15) - scaled_y) >> 10) * c2y_cv + 0x800000 + 0x8000) >> 16];
                } else {
                    s[1] = s[5] = 0x10;
                    s[0] = s[4] = 0x80;
                }
            }
        }
    } else if (subPic.type == MSP_AYUV) {
        for (; top < bottom ; top += subPic.pitch) {
            BYTE* s = top;
            BYTE* e = s + w * 4;

            for (; s < e; s += 4) { // ARGB -> AYUV
                if (s[3] < 0xff) {
                    auto y = BYTE((c2y_yb[s[0]] + c2y_yg[s[1]] + c2y_yr[s[2]] + 0x108000) >> 16);
                    int scaled_y = (y - 32) * cy_cy;
                    s[1] = clip[((((s[0] << 16) - scaled_y) >> 10) * c2y_cu + 0x800000 + 0x8000) >> 16];
                    s[0] = clip[((((s[2] << 16) - scaled_y) >> 10) * c2y_cv + 0x800000 + 0x8000) >> 16];
                    s[2] = y;
                } else {
                    s[0] = s[1] = 0x80;
                    s[2] = 0x10;
                }
            }
        }
    }

    return S_OK;
}

#ifdef _WIN64
void AlphaBlt_YUY2_SSE2(int w, int h, BYTE* d, int dstpitch, BYTE* s, int srcpitch)
{
    unsigned int ia;
    static const __int64 _8181 = 0x0080001000800010i64;

    for (ptrdiff_t j = 0; j < h; j++, s += srcpitch, d += dstpitch) {
        DWORD* d2 = (DWORD*)d;
        BYTE* s2 = s;
        BYTE* s2end = s2 + w * 4;

        for (; s2 < s2end; s2 += 8, d2++) {
            ia = (s2[3] + s2[7]) >> 1;
            if (ia < 0xff) {
                unsigned int c = (s2[4] << 24) | (s2[5] << 16) | (s2[0] << 8) | s2[1]; // (v<<24)|(y2<<16)|(u<<8)|y1;

                ia = (ia << 24) | (s2[7] << 16) | (ia << 8) | s2[3];
                // SSE2
                __m128i mm_zero = _mm_setzero_si128();
                __m128i mm_8181 = _mm_move_epi64(_mm_cvtsi64_si128(_8181));
                __m128i mm_c = _mm_cvtsi32_si128(c);
                mm_c = _mm_unpacklo_epi8(mm_c, mm_zero);
                __m128i mm_d = _mm_cvtsi32_si128(*d2);
                mm_d = _mm_unpacklo_epi8(mm_d, mm_zero);
                __m128i mm_a = _mm_cvtsi32_si128(ia);
                mm_a = _mm_unpacklo_epi8(mm_a, mm_zero);
                mm_a = _mm_srli_epi16(mm_a, 1);
                mm_d = _mm_sub_epi16(mm_d, mm_8181);
                mm_d = _mm_mullo_epi16(mm_d, mm_a);
                mm_d = _mm_srai_epi16(mm_d, 7);
                mm_d = _mm_adds_epi16(mm_d, mm_c);
                mm_d = _mm_packus_epi16(mm_d, mm_d);
                *d2 = (DWORD)_mm_cvtsi128_si32(mm_d);
            }
        }
    }
}

#else

void AlphaBlt_YUY2_MMX(int w, int h, BYTE* d, int dstpitch, BYTE* s, int srcpitch)
{
    unsigned int ia;
    static const __int64 _8181 = 0x0080001000800010i64;

    for (ptrdiff_t j = 0; j < h; j++, s += srcpitch, d += dstpitch) {
        DWORD* d2 = (DWORD*)d;
        BYTE* s2 = s;
        BYTE* s2end = s2 + w * 4;

        for (; s2 < s2end; s2 += 8, d2++) {
            ia = (s2[3] + s2[7]) >> 1;
            if (ia < 0xff) {
                unsigned int c = (s2[4] << 24) | (s2[5] << 16) | (s2[0] << 8) | s2[1]; // (v<<24)|(y2<<16)|(u<<8)|y1;
                ia = (ia << 24) | (s2[7] << 16) | (ia << 8) | s2[3];
                __asm {
                    mov         esi, s2
                    mov         edi, d2
                    pxor        mm0, mm0
                    movq        mm1, _8181
                    movd        mm2, c
                    punpcklbw   mm2, mm0
                    movd        mm3, [edi]
                    punpcklbw   mm3, mm0
                    movd        mm4, ia
                    punpcklbw   mm4, mm0
                    psrlw       mm4, 1
                    psubsw      mm3, mm1
                    pmullw      mm3, mm4
                    psraw       mm3, 7
                    paddsw      mm3, mm2
                    packuswb    mm3, mm3
                    movd        [edi], mm3
                };
            }
        }
    }
    _mm_empty();
}
#endif

void AlphaBlt_YUY2_C(int w, int h, BYTE* d, int dstpitch, BYTE* s, int srcpitch)
{
    unsigned int ia;

    for (ptrdiff_t j = 0; j < h; j++, s += srcpitch, d += dstpitch) {
        DWORD* d2 = (DWORD*)d;
        BYTE* s2 = s;
        BYTE* s2end = s2 + w * 4;

        for (; s2 < s2end; s2 += 8, d2++) {
            ia = (s2[3] + s2[7]) >> 1;
            if (ia < 0xff) {
                //unsigned int c = (s2[4]<<24)|(s2[5]<<16)|(s2[0]<<8)|s2[1]; // (v<<24)|(y2<<16)|(u<<8)|y1;

                // YUY2 colorspace fix. rewritten from sse2 asm
                DWORD y1 = (DWORD)(((((*d2 & 0xff) - 0x10) * (s2[3] >> 1)) >> 7) + s2[1]) & 0xff; // y1
                DWORD uu = (DWORD)((((((*d2 >> 8) & 0xff) - 0x80) * (ia >> 1)) >> 7) + s2[0]) & 0xff; // u
                DWORD y2 = (DWORD)((((((*d2 >> 16) & 0xff) - 0x10) * (s2[7] >> 1)) >> 7) + s2[5]) & 0xff; // y2
                DWORD vv = (DWORD)((((((*d2 >> 24) & 0xff) - 0x80) * (ia >> 1)) >> 7) + s2[4]) & 0xff; // v
                *d2 = (y1) | (uu << 8) | (y2 << 16) | (vv << 24);
            }
        }
    }
}

STDMETHODIMP CMemSubPic::AlphaBlt(RECT* pSrc, RECT* pDst, SubPicDesc* pTarget)
{
    ASSERT(pTarget);

    if (!pSrc || !pDst || !pTarget) {
        return E_POINTER;
    }

    const SubPicDesc& src = m_resizedSpd ? *m_resizedSpd : m_spd;
    SubPicDesc dst = *pTarget; // copy, because we might modify it

    if (src.type != dst.type) {
        return E_INVALIDARG;
    }

    CRect rs(*pSrc), rd(*pDst);

    if (m_resizedSpd) {
        rs = rd = CRect(0, 0, m_resizedSpd->w, m_resizedSpd->h);
    }

    if (dst.h < 0) {
        dst.h = -dst.h;
        rd.bottom = dst.h - rd.bottom;
        rd.top = dst.h - rd.top;
    }

    if (rs.Width() != rd.Width() || rs.Height() != abs(rd.Height())) {
        return E_INVALIDARG;
    }

    int w = rs.Width(), h = rs.Height();
    BYTE* s = src.bits + src.pitch * rs.top + rs.left * 4;
    BYTE* d = dst.bits + dst.pitch * rd.top + ((rd.left * dst.bpp) >> 3);

    if (rd.top > rd.bottom) {
        if (dst.type == MSP_RGB32 || dst.type == MSP_RGB24
                || dst.type == MSP_RGB16 || dst.type == MSP_RGB15
                || dst.type == MSP_YUY2 || dst.type == MSP_AYUV) {
            d = dst.bits + dst.pitch * (rd.top - 1) + (rd.left * dst.bpp >> 3);
        } else if (dst.type == MSP_YV12 || dst.type == MSP_IYUV) {
            d = dst.bits + dst.pitch * (rd.top - 1) + (rd.left * 8 >> 3);
        } else {
            return E_NOTIMPL;
        }

        dst.pitch = -dst.pitch;
    }

    // TODO: m_bInvAlpha support
    switch (dst.type) {
        case MSP_RGBA:
            for (ptrdiff_t j = 0; j < h; j++, s += src.pitch, d += dst.pitch) {
                BYTE* s2 = s;
                BYTE* s2end = s2 + w * 4;
                DWORD* d2 = (DWORD*)d;
                for (; s2 < s2end; s2 += 4, d2++) {
                    if (s2[3] < 0xff) {
                        DWORD bd = 0x00000100 - ((DWORD) s2[3]);
                        DWORD B = ((*((DWORD*)s2) & 0x000000ff) << 8) / bd;
                        DWORD V = ((*((DWORD*)s2) & 0x0000ff00) / bd) << 8;
                        DWORD R = (((*((DWORD*)s2) & 0x00ff0000) >> 8) / bd) << 16;
                        *d2 = B | V | R
                              | (0xff000000 - (*((DWORD*)s2) & 0xff000000)) & 0xff000000;
                    }
                }
            }
            break;
        case MSP_RGB32:
        case MSP_AYUV:
            for (ptrdiff_t j = 0; j < h; j++, s += src.pitch, d += dst.pitch) {
                BYTE* s2 = s;
                BYTE* s2end = s2 + w * 4;
                DWORD* d2 = (DWORD*)d;
                for (; s2 < s2end; s2 += 4, d2++) {
#ifdef _WIN64
                    DWORD ia = 256 - s2[3];
                    if (s2[3] < 0xff) {
                        *d2 = ((((*d2 & 0x00ff00ff) * s2[3]) >> 8) + (((*((DWORD*)s2) & 0x00ff00ff) * ia) >> 8) & 0x00ff00ff)
                              | ((((*d2 & 0x0000ff00) * s2[3]) >> 8) + (((*((DWORD*)s2) & 0x0000ff00) * ia) >> 8) & 0x0000ff00);
                    }
#else
                    if (s2[3] < 0xff) {
                        *d2 = ((((*d2 & 0x00ff00ff) * s2[3]) >> 8) + (*((DWORD*)s2) & 0x00ff00ff) & 0x00ff00ff)
                              | ((((*d2 & 0x0000ff00) * s2[3]) >> 8) + (*((DWORD*)s2) & 0x0000ff00) & 0x0000ff00);
                    }
#endif
                }
            }
            break;
        case MSP_RGB24:
            for (ptrdiff_t j = 0; j < h; j++, s += src.pitch, d += dst.pitch) {
                BYTE* s2 = s;
                BYTE* s2end = s2 + w * 4;
                BYTE* d2 = d;
                for (; s2 < s2end; s2 += 4, d2 += 3) {
                    if (s2[3] < 0xff) {
                        d2[0] = ((d2[0] * s2[3]) >> 8) + s2[0];
                        d2[1] = ((d2[1] * s2[3]) >> 8) + s2[1];
                        d2[2] = ((d2[2] * s2[3]) >> 8) + s2[2];
                    }
                }
            }
            break;
        case MSP_RGB16:
            for (ptrdiff_t j = 0; j < h; j++, s += src.pitch, d += dst.pitch) {
                BYTE* s2 = s;
                BYTE* s2end = s2 + w * 4;
                WORD* d2 = (WORD*)d;
                for (; s2 < s2end; s2 += 4, d2++) {
                    if (s2[3] < 0x1f) {
                        *d2 = (WORD)((((((*d2 & 0xf81f) * s2[3]) >> 5) + (*(DWORD*)s2 & 0xf81f)) & 0xf81f)
                                     | (((((*d2 & 0x07e0) * s2[3]) >> 5) + (*(DWORD*)s2 & 0x07e0)) & 0x07e0));
                    }
                }
            }
            break;
        case MSP_RGB15:
            for (ptrdiff_t j = 0; j < h; j++, s += src.pitch, d += dst.pitch) {
                BYTE* s2 = s;
                BYTE* s2end = s2 + w * 4;
                WORD* d2 = (WORD*)d;
                for (; s2 < s2end; s2 += 4, d2++) {
                    if (s2[3] < 0x1f) {
                        *d2 = (WORD)((((((*d2 & 0x7c1f) * s2[3]) >> 5) + (*(DWORD*)s2 & 0x7c1f)) & 0x7c1f)
                                     | (((((*d2 & 0x03e0) * s2[3]) >> 5) + (*(DWORD*)s2 & 0x03e0)) & 0x03e0));
                    }
                }
            }
            break;
        case MSP_YUY2: {
#ifdef _WIN64
            auto alphablt_func = AlphaBlt_YUY2_SSE2;
#else
            auto alphablt_func = AlphaBlt_YUY2_MMX;
#endif
            //alphablt_func = AlphaBlt_YUY2_C;

            alphablt_func(w, h, d, dst.pitch, s, src.pitch);
        }
        break;
        case MSP_YV12:
        case MSP_IYUV:
            for (ptrdiff_t j = 0; j < h; j++, s += src.pitch, d += dst.pitch) {
                BYTE* s2 = s;
                BYTE* s2end = s2 + w * 4;
                BYTE* d2 = d;
                for (; s2 < s2end; s2 += 4, d2++) {
                    if (s2[3] < 0xff) {
                        d2[0] = (((d2[0] - 0x10) * s2[3]) >> 8) + s2[1];
                    }
                }
            }
            break;
        default:
            return E_NOTIMPL;
    }

    dst.pitch = abs(dst.pitch);

    if (dst.type == MSP_YV12 || dst.type == MSP_IYUV) {
        int h2 = h / 2;

        if (!dst.pitchUV) {
            dst.pitchUV = dst.pitch / 2;
        }

        BYTE* ss[2];
        ss[0] = src.bits + src.pitch * rs.top + rs.left * 4;
        ss[1] = ss[0] + 4;

        if (!dst.bitsU || !dst.bitsV) {
            dst.bitsU = dst.bits + dst.pitch * dst.h;
            dst.bitsV = dst.bitsU + dst.pitchUV * dst.h / 2;

            if (dst.type == MSP_YV12) {
                BYTE* p = dst.bitsU;
                dst.bitsU = dst.bitsV;
                dst.bitsV = p;
            }
        }

        BYTE* dd[2];
        dd[0] = dst.bitsU + dst.pitchUV * rd.top / 2 + rd.left / 2;
        dd[1] = dst.bitsV + dst.pitchUV * rd.top / 2 + rd.left / 2;

        if (rd.top > rd.bottom) {
            dd[0] = dst.bitsU + dst.pitchUV * (rd.top / 2 - 1) + rd.left / 2;
            dd[1] = dst.bitsV + dst.pitchUV * (rd.top / 2 - 1) + rd.left / 2;
            dst.pitchUV = -dst.pitchUV;
        }

        for (ptrdiff_t i = 0; i < 2; i++) {
            s = ss[i];
            d = dd[i];
            BYTE* is = ss[1 - i];
            for (ptrdiff_t j = 0; j < h2; j++, s += src.pitch * 2, d += dst.pitchUV, is += src.pitch * 2) {
                BYTE* s2 = s;
                BYTE* s2end = s2 + w * 4;
                BYTE* d2 = d;
                BYTE* is2 = is;
                for (; s2 < s2end; s2 += 8, d2++, is2 += 8) {
                    unsigned int ia = (s2[3] + s2[3 + src.pitch] + is2[3] + is2[3 + src.pitch]) >> 2;
                    if (ia < 0xff) {
                        *d2 = BYTE((((*d2 - 0x80) * ia) >> 8) + ((s2[0] + s2[src.pitch]) >> 1));
                    }
                }
            }
        }
    }

    return S_OK;
}

//
// CMemSubPicAllocator
//

CMemSubPicAllocator::CMemSubPicAllocator(int type, SIZE maxsize)
    : CSubPicAllocatorImpl(maxsize, false)
    , m_type(type)
    , m_maxsize(maxsize)
{
}

CMemSubPicAllocator::~CMemSubPicAllocator()
{
    CAutoLock cAutoLock(this);

    for (const auto& p : m_freeMemoryChunks) {
        delete[] std::get<1>(p);
    }
}

// ISubPicAllocatorImpl

bool CMemSubPicAllocator::Alloc(bool fStatic, ISubPic** ppSubPic)
{
    if (!ppSubPic || m_maxsize.cx <= 0 || m_maxsize.cy <= 0) {
        return false;
    }

    SubPicDesc spd;
    spd.w = m_maxsize.cx;
    spd.h = m_maxsize.cy;
    spd.bpp = 32;
    spd.pitch = (spd.w * spd.bpp) >> 3;
    spd.type = m_type;
    spd.vidrect = m_curvidrect;

    if (!AllocSpdBits(spd)) {
        return false;
    }

    try {
        *ppSubPic = DEBUG_NEW CMemSubPic(spd, this);
    } catch (CMemoryException* e) {
        e->Delete();
        delete [] spd.bits;
        return false;
    }

    (*ppSubPic)->AddRef();
    (*ppSubPic)->SetInverseAlpha(m_bInvAlpha);

    return true;
}

bool CMemSubPicAllocator::AllocSpdBits(SubPicDesc& spd)
{
    CAutoLock cAutoLock(this);

    ASSERT(!spd.bits);
    ASSERT(spd.pitch * spd.h > 0);

    auto it = std::find_if(m_freeMemoryChunks.cbegin(), m_freeMemoryChunks.cend(), [&](const std::pair<size_t, BYTE*>& p) {
        return std::get<0>(p) == size_t(spd.pitch) * spd.h;
    });

    if (it != m_freeMemoryChunks.cend()) {
        spd.bits = std::get<1>(*it);
        m_freeMemoryChunks.erase(it);
    } else {
        try {
            spd.bits = DEBUG_NEW BYTE[spd.pitch * spd.h];
        } catch (CMemoryException* e) {
            ASSERT(FALSE);
            e->Delete();
            return false;
        }
    }
    return true;
}

void CMemSubPicAllocator::FreeSpdBits(SubPicDesc& spd)
{
    CAutoLock cAutoLock(this);

    ASSERT(spd.bits);
    m_freeMemoryChunks.emplace_back(spd.pitch * spd.h, spd.bits);
    spd.bits = nullptr;
}

STDMETHODIMP CMemSubPicAllocator::SetMaxTextureSize(SIZE maxTextureSize)
{
    if (maxTextureSize.cx > 0 && maxTextureSize.cy > 0 && m_maxsize != maxTextureSize) {
        m_maxsize = maxTextureSize;
        CAutoLock cAutoLock(this);
        for (const auto& p : m_freeMemoryChunks) {
            delete[] std::get<1>(p);
        }
        m_freeMemoryChunks.clear();
    }
    return S_OK;
}
