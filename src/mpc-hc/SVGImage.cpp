/*
* (C) 2016-2017 see Authors.txt
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
#include "SVGImage.h"
#include "mplayerc.h"
#include <atlimage.h>

#pragma warning(disable: 4244)
#define NANOSVG_IMPLEMENTATION
#include <nanosvg/src/nanosvg.h>
#define NANOSVGRAST_IMPLEMENTATION
#include <nanosvg/src/nanosvgrast.h>

namespace
{
    HRESULT NSVGimageToCImage(NSVGimage* svgImage, CImage& image, float scale)
    {
        image.Destroy();

        if (!svgImage) {
            return E_FAIL;
        }

        NSVGrasterizer* rasterizer = nsvgCreateRasterizer();
        if (!rasterizer) {
            nsvgDelete(svgImage);
            return E_FAIL;
        }

        if (!image.Create(int(svgImage->width * scale), int(svgImage->height * scale), 32)) {
            nsvgDeleteRasterizer(rasterizer);
            nsvgDelete(svgImage);
            return E_FAIL;
        }

        nsvgRasterize(rasterizer, svgImage, 0.0f, 0.0f, scale,
                      static_cast<unsigned char*>(image.GetBits()),
                      image.GetWidth(), image.GetHeight(), image.GetPitch());

        // NanoSVG outputs RGBA but we need BGRA so we swap red and blue
        BYTE* bits = static_cast<BYTE*>(image.GetBits());
        for (int y = 0; y < image.GetHeight(); y++, bits += image.GetPitch()) {
            RGBQUAD* p = reinterpret_cast<RGBQUAD*>(bits);
            for (int x = 0; x < image.GetWidth(); x++) {
                std::swap(p[x].rgbRed, p[x].rgbBlue);
            }
        }

        nsvgDeleteRasterizer(rasterizer);
        nsvgDelete(svgImage);

        return S_OK;
    }
}

HRESULT SVGImage::Load(LPCTSTR filename, CImage& image, float scale /*= 1.0f*/)
{
    return NSVGimageToCImage(nsvgParseFromFile(CStringA(filename), "px", 96.0f), image, scale);
}

HRESULT SVGImage::Load(UINT uResId, CImage& image, float scale /*= 1.0f*/)
{
    CStringA svg;
    if (!LoadResource(uResId, svg, _T("SVG"))) {
        return E_FAIL;
    }

    return NSVGimageToCImage(nsvgParse(svg.GetBuffer(), "px", 96.0f), image, scale);
}

void GdiTransform(CImage& image, Gdiplus::RotateFlipType transform) {
    int bpp = image.GetBPP();

    HBITMAP hbmp = image.Detach();
    Gdiplus::Bitmap* bmpTemp = Gdiplus::Bitmap::FromHBITMAP(hbmp, 0);
    Gdiplus::PixelFormat pixel_format = bmpTemp->GetPixelFormat();
    if (bpp == 32) {
        pixel_format = PixelFormat32bppARGB;
    }
    image.Attach(hbmp);

    Gdiplus::Bitmap bmp(image.GetWidth(), image.GetHeight(), image.GetPitch(), pixel_format, static_cast<BYTE*>(image.GetBits()));
    bmp.RotateFlip(transform);

    image.Destroy();
    if (image.Create(bmp.GetWidth(), bmp.GetHeight(), 32, CImage::createAlphaChannel)) {
        Gdiplus::Bitmap dst(image.GetWidth(), image.GetHeight(), image.GetPitch(), PixelFormat32bppARGB, static_cast<BYTE*>(image.GetBits()));
        Gdiplus::Graphics graphics(&dst);
        graphics.DrawImage(&bmp, 0, 0);
    }
}

HRESULT SVGImage::LoadIconDef(IconDef iconDef, CImage& image) {
    CStringA svg;
    if (!LoadResource(iconDef.nIDIcon, svg, _T("SVG"))) {
        return E_FAIL;
    }

    NSVGimage* svgImage = nsvgParse(svg.GetBuffer(), "px", 96.0f);
    HRESULT ret = NSVGimageToCImage(svgImage, image, (float)iconDef.svgTargetWidth / svgImage->width);
    if (iconDef.svgTransform == TRANSFORM_FLIP_HORZ) {
        GdiTransform(image, Gdiplus::RotateNoneFlipX);
    } else if (iconDef.svgTransform == TRANSFORM_ROT90CCW) {
        GdiTransform(image, Gdiplus::Rotate270FlipNone);
    } else if (iconDef.svgTransform == TRANSFORM_ROT90CW) {
        GdiTransform(image, Gdiplus::Rotate90FlipNone);
    }

    return ret;
}
