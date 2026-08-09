//  VirtualDub - Video processing and capture application
//  Copyright (C) 1998-2007 Avery Lee
//
//  This program is free software; you can redistribute it and/or modify
//  it under the terms of the GNU General Public License as published by
//  the Free Software Foundation; either version 2 of the License, or
//  (at your option) any later version.
//
//  This program is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//  GNU General Public License for more details.
//
//  You should have received a copy of the GNU General Public License
//  along with this program; if not, write to the Free Software
//  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

#include "stdafx.h"

#include <cstdint>
typedef uint32_t uint32; // formerly from vd2/system/vdstl.h

#pragma warning(disable: 4799)      // warning C4799: function has no EMMS instruction

///////////////////////////////////////////////////////////////////////////

#ifdef _M_IX86
static void __declspec(naked) asm_blend_row_clipped(void *dst, const void *src, uint32 w, ptrdiff_t srcpitch) {
    __asm {
        push    ebp
        push    edi
        push    esi
        push    ebx

        mov     edi,[esp+20]
        mov     esi,[esp+24]
        sub     edi,esi
        mov     ebp,[esp+28]
        mov     edx,[esp+32]

xloop:
        mov     ecx,[esi]
        mov     eax,0fefefefeh

        mov     ebx,[esi+edx]
        and     eax,ecx

        shr     eax,1
        and     ebx,0fefefefeh

        shr     ebx,1
        add     esi,4

        add     eax,ebx
        dec     ebp

        mov     [edi+esi-4],eax
        jnz     xloop

        pop     ebx
        pop     esi
        pop     edi
        pop     ebp
        ret
    };
}

static void __declspec(naked) asm_blend_row(void *dst, const void *src, uint32 w, ptrdiff_t srcpitch) {
    __asm {
        push    ebp
        push    edi
        push    esi
        push    ebx

        mov     edi,[esp+20]
        mov     esi,[esp+24]
        sub     edi,esi
        mov     ebp,[esp+28]
        mov     edx,[esp+32]

xloop:
        mov     ecx,[esi]
        mov     eax,0fcfcfcfch

        mov     ebx,[esi+edx]
        and     eax,ecx

        shr     ebx,1
        mov     ecx,[esi+edx*2]

        shr     ecx,2
        and     ebx,07f7f7f7fh

        shr     eax,2
        and     ecx,03f3f3f3fh

        add     eax,ebx
        add     esi,4

        add     eax,ecx
        dec     ebp

        mov     [edi+esi-4],eax
        jnz     xloop

        pop     ebx
        pop     esi
        pop     edi
        pop     ebp
        ret
    };
}
#else
static void asm_blend_row_clipped(void *dst0, const void *src0, uint32 w, ptrdiff_t srcpitch) {
    uint32 *dst = (uint32 *)dst0;
    const uint32 *src = (const uint32 *)src0;
    const uint32 *src2 = (const uint32 *)((const char *)src + srcpitch);

    do {
        const uint32 x = *src++;
        const uint32 y = *src2++;

        *dst++ = (x|y) - (((x^y)&0xfefefefe)>>1);
    } while(--w);
}

static void asm_blend_row(void *dst0, const void *src0, uint32 w, ptrdiff_t srcpitch) {
    uint32 *dst = (uint32 *)dst0;
    const uint32 *src = (const uint32 *)src0;
    const uint32 *src2 = (const uint32 *)((const char *)src + srcpitch);
    const uint32 *src3 = (const uint32 *)((const char *)src2 + srcpitch);

    do {
        const uint32 a = *src++;
        const uint32 b = *src2++;
        const uint32 c = *src3++;
        const uint32 hi = (a & 0xfcfcfc) + 2*(b & 0xfcfcfc) + (c & 0xfcfcfc);
        const uint32 lo = (a & 0x030303) + 2*(b & 0x030303) + (c & 0x030303) + 0x020202;

        *dst++ = (hi + (lo & 0x0c0c0c))>>2;
    } while(--w);
}
#endif

#if defined(_M_IX86) || defined(_M_X64) // formerly VD_CPU_X86/VD_CPU_AMD64 from vd2/system/vdtypes.h
    static void asm_blend_row_SSE2(void *dst, const void *src, uint32 w, ptrdiff_t srcpitch) {
        __m128i zero = _mm_setzero_si128();
        __m128i inv = _mm_cmpeq_epi8(zero, zero);

        w = (w + 3) >> 2;

        const __m128i *src1 = (const __m128i *)src;
        const __m128i *src2 = (const __m128i *)((const char *)src + srcpitch);
        const __m128i *src3 = (const __m128i *)((const char *)src + srcpitch*2);
        __m128i *dstrow = (__m128i *)dst;
        do {
            __m128i a = *src1++;
            __m128i b = *src2++;
            __m128i c = *src3++;

            *dstrow++ = _mm_avg_epu8(_mm_xor_si128(_mm_avg_epu8(_mm_xor_si128(a, inv), _mm_xor_si128(c, inv)), inv), b);
        } while(--w);
    }
#endif

    void Average_scalar(void *dst, ptrdiff_t dstPitch, const void *src1, const void *src2, ptrdiff_t srcPitch, uint32 w16, uint32 h) {
        uint32 w4 = w16 << 2;
        do {
            uint32 *dstv = (uint32 *)dst;
            uint32 *src1v = (uint32 *)src1;
            uint32 *src2v = (uint32 *)src2;

            for(uint32 i=0; i<w4; ++i) {
                uint32 a = src1v[i];
                uint32 b = src2v[i];

                dstv[i] = (a|b) - (((a^b) & 0xfefefefe) >> 1);
            }

            dst = (char *)dst + dstPitch;
            src1 = (char *)src1 + srcPitch;
            src2 = (char *)src2 + srcPitch;
        } while(--h);
    }

    void Average_SSE2(void *dst, ptrdiff_t dstPitch, const void *src1, const void *src2, ptrdiff_t srcPitch, uint32 w16, uint32 h) {
        do {
            __m128i *dstv = (__m128i *)dst;
            __m128i *src1v = (__m128i *)src1;
            __m128i *src2v = (__m128i *)src2;

            for(uint32 i=0; i<w16; ++i)
                dstv[i] = _mm_avg_epu8(src1v[i], src2v[i]);

            dst = (char *)dst + dstPitch;
            src1 = (char *)src1 + srcPitch;
            src2 = (char *)src2 + srcPitch;
        } while(--h);
    }

    void InterpPlane_Bob(void *dst, ptrdiff_t dstpitch, const void *src, ptrdiff_t srcpitch, uint32 w, uint32 h, bool interpField2) {
        w = (w + 3) >> 2;

        uint32 y0 = interpField2 ? 1 : 2;

        if (!interpField2)
            memcpy(dst, src, w * 4);

        if (h > y0) {
            ASSERT(((UINT_PTR)dst & 0xF) == 0);
            ASSERT((dstpitch & 0xF) == 0);
            ASSERT(((UINT_PTR)src & 0xF) == 0);
            ASSERT((srcpitch*(y0 - 1) & 0xF) == 0);
            Average_SSE2((char *)dst + dstpitch*y0,
                dstpitch*2,
                (const char *)src + srcpitch*(y0 - 1),
                (const char *)src + srcpitch*(y0 + 1),
                srcpitch*2,
                (w + 3) >> 2,
                (h - y0) >> 1);
        }

        if (interpField2)
            memcpy((char *)dst + dstpitch*(h - 1), (const char *)src + srcpitch*(h - 1), w*4);
    }

    void BlendPlane(void *dst, ptrdiff_t dstpitch, const void *src, ptrdiff_t srcpitch, uint32 w, uint32 h) {
        void (*blend_func)(void *, const void *, uint32, ptrdiff_t);
        if (!(srcpitch % 16))
            blend_func = asm_blend_row_SSE2;
        else
            blend_func = asm_blend_row;        

        w = (w + 3) >> 2;

        asm_blend_row_clipped(dst, src, w, srcpitch);
        if (h-=2)
            do {
                dst = ((char *)dst + dstpitch);

                blend_func(dst, src, w, srcpitch);

                src = ((char *)src + srcpitch);
            } while(--h);

        asm_blend_row_clipped((char *)dst + dstpitch, src, w, srcpitch);
    }

void DeinterlaceBob(BYTE* dst, BYTE* src, DWORD w, DWORD h, DWORD dstpitch, DWORD srcpitch, bool topfield)
{
    topfield = !topfield;

    InterpPlane_Bob(dst, dstpitch, src, srcpitch, w, h, topfield);
}

void DeinterlaceBlend(BYTE* dst, BYTE* src, DWORD w, DWORD h, DWORD dstpitch, DWORD srcpitch)
{
    BlendPlane(dst, dstpitch, src, srcpitch, w, h);
}
