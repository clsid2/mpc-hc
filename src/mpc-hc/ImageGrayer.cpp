/*
* (C) 2016 see Authors.txt
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
#include "ImageGrayer.h"
#include "CMPCTheme.h"

struct HLS {
    double H, L, S;

    HLS(const RGBQUAD& rgb) {
        double R = rgb.rgbRed / 255.0;
        double G = rgb.rgbGreen / 255.0;
        double B = rgb.rgbBlue / 255.0;
        double max = std::max({ R, G, B });
        double min = std::min({ R, G, B });

        L = (max + min) / 2.0;

        if (max == min) {
            S = H = 0.0;
        } else {
            double d = max - min;

            S = (L < 0.5) ? (d / (max + min)) : (d / (2.0 - max - min));

            if (R == max) {
                H = (G - B) / d;
            } else if (G == max) {
                H = 2.0 + (B - R) / d;
            } else { // if (B == max)
                H = 4.0 + (R - G) / d;
            }
            H /= 6.0;
            if (H < 0.0) {
                H += 1.0;
            }
        }
    }

    RGBQUAD toRGBQUAD() {
        RGBQUAD rgb;
        rgb.rgbReserved = 255;

        if (S == 0.0) {
            rgb.rgbRed = rgb.rgbGreen = rgb.rgbBlue = BYTE(L * 255);
        } else {
            auto hue2rgb = [](double p, double q, double h) {
                if (h < 0.0) {
                    h += 1.0;
                } else if (h > 1.0) {
                    h -= 1.0;
                }

                if (h < 1.0 / 6.0) {
                    return p + (q - p) * 6.0 * h;
                } else if (h < 0.5) {
                    return q;
                } else if (h < 2.0 / 3.0) {
                    return p + (q - p) * (2.0 / 3.0 - h) * 6.0;
                }
                return p;
            };

            double q = (L < 0.5) ? (L * (1 + S)) : (L + S - L * S);
            double p = 2 * L - q;

            rgb.rgbRed = BYTE(hue2rgb(p, q, H + 1.0 / 3.0) * 255);
            rgb.rgbGreen = BYTE(hue2rgb(p, q, H) * 255);
            rgb.rgbBlue = BYTE(hue2rgb(p, q, H - 1.0 / 3.0) * 255);
        }

        return rgb;
    }
};

bool ImageGrayer::Gray(const CImage& imgSource, CImage& imgDest, float brightness)
{
    // Only support 32-bit image for now
    if (imgSource.GetBPP() != 32) {
        return false;
    }

    imgDest.Destroy();
    if (!imgDest.Create(imgSource.GetWidth(), imgSource.GetHeight(), imgSource.GetBPP())) {
        return false;
    }
    BOOL bCopied = imgSource.BitBlt(imgDest.GetDC(), 0, 0);
    imgDest.ReleaseDC();
    if (!bCopied) {
        return false;
    }

    auto adjustBrightness = [](BYTE c, double p) {
        int cAdjusted;
        if (c == 0 && p > 1.0) {
            cAdjusted = std::lround((p - 1.0) * 255);
        } else {
            cAdjusted = std::lround(c * p);
        }

        return BYTE(std::min(cAdjusted, 255));
    };

    BYTE* bits = static_cast<BYTE*>(imgDest.GetBits());
    for (int y = 0; y < imgDest.GetHeight(); y++, bits += imgDest.GetPitch()) {
        RGBQUAD* p = reinterpret_cast<RGBQUAD*>(bits);
        for (int x = 0; x < imgDest.GetWidth(); x++) {
            HLS hls(p[x]);
            hls.S = 0.0; // Make the color gray

            RGBQUAD rgb = hls.toRGBQUAD();

            p[x].rgbRed = BYTE(adjustBrightness(rgb.rgbRed, 1.5 * brightness) * p[x].rgbReserved / 255);
            p[x].rgbGreen = BYTE(adjustBrightness(rgb.rgbGreen, 1.5 * brightness) * p[x].rgbReserved / 255);
            p[x].rgbBlue = BYTE(adjustBrightness(rgb.rgbBlue, 1.5 * brightness) * p[x].rgbReserved / 255);
        }
    }

    return true;
}

bool ImageGrayer::UpdateColor(const CImage& imgSource, CImage& imgDest, bool disabled, mpcColorStyle colorStyle)
{
    // Force to 32-bit
    CImage img32;
    CImage const* imgSrc;
    if (imgSource.GetBPP() != 32) {
        if (!img32.Create(imgSource.GetWidth(), imgSource.GetHeight(), 32, CImage::createAlphaChannel)) {
            return false;
        }

        HDC const iDC = img32.GetDC();
        BOOL const bbResult = imgSource.BitBlt(iDC, 0, 0, SRCCOPY);
        img32.ReleaseDC();

        if (!bbResult) {
            return false;
        }

        BYTE* bits = static_cast<BYTE*>(img32.GetBits());
        for (int y = 0; y < img32.GetHeight(); y++, bits += img32.GetPitch()) {
            RGBQUAD* p = reinterpret_cast<RGBQUAD*>(bits);
            for (int x = 0; x < img32.GetWidth(); x++) {
                HLS hls(p[x]);
                p[x].rgbReserved = 255;
            }
        }
        imgSrc = &img32;
    } else {
        imgSrc = &imgSource;
    }

    if (colorStyle == ImageGrayer::classicGrayscale) {
        return Gray(imgSource, imgDest);
    } else if (colorStyle == ImageGrayer::mpcGrayDisabled) {
        if (disabled) {
            return Gray(imgSource, imgDest, 0.5f);
        } else {
            imgDest = imgSource;
        }
    } else { //mpcMono
        imgDest.Destroy();

        if (!imgDest.Create(imgSrc->GetWidth(), imgSrc->GetHeight(), imgSrc->GetBPP())) {
            return false;
        }
        BOOL bCopied = imgSrc->BitBlt(imgDest.GetDC(), 0, 0);
        imgDest.ReleaseDC();
        if (!bCopied) {
            return false;
        }

        RGBQUAD newColor;
        COLORREF themeColor;

        if (disabled) {
            themeColor = CMPCTheme::ImageDisabledColor;
        } else {
            themeColor = CMPCTheme::TextFGColorFade;
        }
        newColor.rgbRed = GetRValue(themeColor);
        newColor.rgbGreen = GetGValue(themeColor);
        newColor.rgbBlue = GetBValue(themeColor);
        newColor.rgbReserved = 0;



        BYTE* bits = static_cast<BYTE*>(imgDest.GetBits());
        for (int y = 0; y < imgDest.GetHeight(); y++, bits += imgDest.GetPitch()) {
            RGBQUAD* p = reinterpret_cast<RGBQUAD*>(bits);
            for (int x = 0; x < imgDest.GetWidth(); x++) {
                HLS hls(p[x]);

                RGBQUAD rgb = hls.toRGBQUAD();

                if (p[x].rgbReserved != 0) { //ignore the transparent bits
                    p[x].rgbRed = newColor.rgbRed;
                    p[x].rgbBlue = newColor.rgbBlue;
                    p[x].rgbGreen = newColor.rgbGreen;
                }
            }
        }
    }

    return true;
}

bool ImageGrayer::Colorize(const CImage& imgSrc, CImage& imgDest, COLORREF fg, COLORREF bg, bool rot90) {
    // Force to 32-bit
    if (imgSrc.GetBPP() != 32) {
        return false;
    }

    imgDest.Destroy();

    int width = imgSrc.GetWidth();
    int height = imgSrc.GetHeight();

    if (!imgDest.Create(width, height, imgSrc.GetBPP())) {
        return false;
    }
    BOOL bCopied = imgSrc.BitBlt(imgDest.GetDC(), 0, 0);
    imgDest.ReleaseDC();
    if (!bCopied) {
        return false;
    }

    RGBQUAD fgColor, bgColor;
    fgColor.rgbRed = GetRValue(fg);
    fgColor.rgbGreen = GetGValue(fg);
    fgColor.rgbBlue = GetBValue(fg);
    fgColor.rgbReserved = 0;

    bgColor.rgbRed = GetRValue(bg);
    bgColor.rgbGreen = GetGValue(bg);
    bgColor.rgbBlue = GetBValue(bg);
    bgColor.rgbReserved = 0;


    BYTE* bits = static_cast<BYTE*>(imgDest.GetBits());
    for (int y = 0; y < imgDest.GetHeight(); y++, bits += imgDest.GetPitch()) {
        RGBQUAD* p = reinterpret_cast<RGBQUAD*>(bits);
        for (int x = 0; x < imgDest.GetWidth(); x++) {
            HLS hls(p[x]);

            RGBQUAD rgb = hls.toRGBQUAD();

            if (p[x].rgbReserved != 0) { //ignore the transparent bits
                p[x].rgbRed = BYTE(fgColor.rgbRed * p[x].rgbReserved / 255 + (255 - p[x].rgbReserved)  * bgColor.rgbRed / 255);
                p[x].rgbBlue = BYTE(fgColor.rgbBlue * p[x].rgbReserved / 255 + (255 - p[x].rgbReserved) * bgColor.rgbBlue / 255);
                p[x].rgbGreen = BYTE(fgColor.rgbGreen * p[x].rgbReserved / 255 + (255 - p[x].rgbReserved) * bgColor.rgbGreen / 255);
            } else {
                p[x].rgbRed = bgColor.rgbRed;
                p[x].rgbBlue = bgColor.rgbBlue;
                p[x].rgbGreen = bgColor.rgbGreen;
            }
            p[x].rgbReserved = 255;
        }
    }

    if (rot90) {
        Gdiplus::Bitmap* gdiPlusBitmap = Gdiplus::Bitmap::FromHBITMAP(imgDest.Detach(), 0);
        gdiPlusBitmap->RotateFlip(Gdiplus::Rotate90FlipNone);
        HBITMAP hbmp;
        gdiPlusBitmap->GetHBITMAP(Gdiplus::Color::White, &hbmp);
        imgDest.Attach(hbmp);
    }

    return true;
}

void ImageGrayer::PreMultiplyAlpha(CImage& imgSource) {
    BYTE* bits = static_cast<BYTE*>(imgSource.GetBits());
    for (int y = 0; y < imgSource.GetHeight(); y++, bits += imgSource.GetPitch()) {
        RGBQUAD* p = reinterpret_cast<RGBQUAD*>(bits);
        for (int x = 0; x < imgSource.GetWidth(); x++) {
            HLS hls(p[x]);

            RGBQUAD rgb = hls.toRGBQUAD();

            p[x].rgbRed = static_cast<BYTE>(round((float)p[x].rgbRed * p[x].rgbReserved / 255.0f));
            p[x].rgbBlue = static_cast<BYTE>(round((float)p[x].rgbBlue * p[x].rgbReserved / 255.0f));
            p[x].rgbGreen = static_cast<BYTE>(round((float)p[x].rgbGreen * p[x].rgbReserved / 255.0f));
        }
    }
}
