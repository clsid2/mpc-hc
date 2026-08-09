// RGB surface conversion helpers. Historically these wrapped VirtualDub's
// Kasumi pixmap library (hence the file name); they are now self-contained.
// The per-format row converters below are transcribed verbatim from Kasumi's
// blt_reference_rgb.cpp so results stay bit-identical to the old code paths.
//
// Pixel layouts follow the Windows DIB conventions the callers use:
// 16 bpp = RGB565, 24 bpp = BGR, 32 bpp = BGRX (the fourth byte is not
// interpreted as alpha and reads back as 0 after conversion from 24 bpp).

#include "stdafx.h"
#include "vd.h"

namespace
{
    void Row565To888(BYTE* dst, const BYTE* src, int w)
    {
        const WORD* s = (const WORD*)src;
        do {
            const UINT32 px = *s++;
            UINT32 rb = px & 0xf81f;
            UINT32 g = px & 0x07e0;
            rb += rb << 5;
            g += g << 6;
            dst[0] = (BYTE)(rb >> 2);
            dst[1] = (BYTE)(g >> 9);
            dst[2] = (BYTE)(rb >> 13);
            dst += 3;
        } while (--w);
    }

    void Row565To8888(BYTE* dst, const BYTE* src, int w)
    {
        const WORD* s = (const WORD*)src;
        UINT32* d = (UINT32*)dst;
        do {
            const UINT32 px = *s++;
            const UINT32 rb = ((px & 0xf800) << 8) + ((px & 0x001f) << 3);
            const UINT32 g = ((px & 0x07e0) << 5) + (px & 0x0300);
            *d++ = rb + ((rb & 0xe000e0) >> 5) + g;
        } while (--w);
    }

    void Row888To565(BYTE* dst, const BYTE* src, int w)
    {
        WORD* d = (WORD*)dst;
        do {
            const UINT32 r = ((UINT32)src[2] & 0xf8) << 8;
            const UINT32 g = ((UINT32)src[1] & 0xfc) << 3;
            const UINT32 b = (UINT32)src[0] >> 3;
            src += 3;
            *d++ = (WORD)(r + g + b);
        } while (--w);
    }

    void Row8888To565(BYTE* dst, const BYTE* src, int w)
    {
        WORD* d = (WORD*)dst;
        do {
            const UINT32 r = ((UINT32)src[2] & 0xf8) << 8;
            const UINT32 g = ((UINT32)src[1] & 0xfc) << 3;
            const UINT32 b = (UINT32)src[0] >> 3;
            src += 4;
            *d++ = (WORD)(r + g + b);
        } while (--w);
    }

    void Row888To8888(BYTE* dst, const BYTE* src, int w)
    {
        UINT32* d = (UINT32*)dst;
        do {
            *d++ = (UINT32)src[0] + ((UINT32)src[1] << 8) + ((UINT32)src[2] << 16);
            src += 3;
        } while (--w);
    }

    void Row8888To888(BYTE* dst, const BYTE* src, int w)
    {
        do {
            dst[0] = src[0];
            dst[1] = src[1];
            dst[2] = src[2];
            dst += 3;
            src += 4;
        } while (--w);
    }
}

bool BitBltFromRGBToRGB(int w, int h, BYTE* dst, int dstpitch, int dbpp, BYTE* src, int srcpitch, int sbpp)
{
    if (w <= 0 || h <= 0 || !dst || !src) {
        return false;
    }

    void (*rowconv)(BYTE*, const BYTE*, int) = nullptr;

    if (sbpp == dbpp) {
        rowconv = nullptr; // straight row copy
    } else if (sbpp == 16 && dbpp == 24) {
        rowconv = Row565To888;
    } else if (sbpp == 16 && dbpp == 32) {
        rowconv = Row565To8888;
    } else if (sbpp == 24 && dbpp == 16) {
        rowconv = Row888To565;
    } else if (sbpp == 24 && dbpp == 32) {
        rowconv = Row888To8888;
    } else if (sbpp == 32 && dbpp == 16) {
        rowconv = Row8888To565;
    } else if (sbpp == 32 && dbpp == 24) {
        rowconv = Row8888To888;
    } else {
        ASSERT(FALSE);
        return false;
    }

    if (sbpp != 16 && sbpp != 24 && sbpp != 32) {
        ASSERT(FALSE);
        return false;
    }

    // Pitches may be negative for bottom-up rows; the same row indexing works
    // for either orientation since both sides advance by their own pitch.
    const size_t rowbytes = (size_t)w * (sbpp >> 3);
    for (int y = 0; y < h; y++, src += srcpitch, dst += dstpitch) {
        if (rowconv) {
            rowconv(dst, src, w);
        } else {
            memcpy(dst, src, rowbytes);
        }
    }

    return true;
}
