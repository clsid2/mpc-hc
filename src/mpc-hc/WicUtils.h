/*
 * (C) 2020-2023 see Authors.txt
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

#include <wincodec.h>

DEFINE_GUID(GUID_ContainerFormatHeif, 0xE1E62521, 0x6787, 0x405B, 0xA3, 0x39, 0x50, 0x07, 0x15, 0xB5, 0x76, 0x3F);
DEFINE_GUID(GUID_ContainerFormatWebp, 0xE094B0E2, 0x67F2, 0x45B3, 0xB0, 0xEA, 0x11, 0x53, 0x37, 0xCA, 0x7C, 0xF3);

DEFINE_GUID(CLSID_WICHeifDecoder,     0xE9A4A80A, 0x44FE, 0x4DE4, 0x89, 0x71, 0x71, 0x50, 0xB1, 0x0A, 0x51, 0x99);
DEFINE_GUID(CLSID_WICWebpDecoder,     0x7693E886, 0x51C9, 0x4070, 0x84, 0x19, 0x9F, 0x70, 0x73, 0x8E, 0xC8, 0xFA);

// CWICImagingFactory

class CWICImagingFactory
{
// http://www.nuonsoft.com/blog/2011/10/17/introduction-to-wic-how-to-use-wic-to-load-an-image-and-draw-it-with-gdi/
public:
    inline static CWICImagingFactory& GetInstance()
    {
        if (nullptr == m_pInstance.get())
            m_pInstance.reset(new CWICImagingFactory());
        return *m_pInstance;
    }

    IWICImagingFactory* GetFactory() const;

protected:
    CComPtr<IWICImagingFactory> m_pWICImagingFactory;

private:
    CWICImagingFactory();   // Private because singleton
    inline static std::unique_ptr<CWICImagingFactory> m_pInstance;
};

/////////////////////

#if 0 // unused, kept for future use
struct WICCodecInfo_t {
    GUID containerFormat;
    std::wstring name;
    std::wstring fileExts;
    std::vector<WICPixelFormatGUID> pixelFmts;
};

HRESULT WicGetCodecs(std::vector<WICCodecInfo_t>& codecs, bool bEncoder);

HRESULT WicCheckComponent(const GUID guid);
#endif

HRESULT WicLoadImage(IWICBitmap** ppBitmap, const bool pma, const std::wstring_view filename);
HRESULT WicLoadImage(IWICBitmap** ppBitmap, const bool pma, BYTE* input, const size_t size);
HRESULT WicLoadImage(IWICBitmap** ppBitmap, const bool pma, IStream* pIStream);

HRESULT WicCreateDibSecton(HBITMAP& hBitmap, BYTE** ppData, BITMAPINFO& bminfo, IWICBitmapSource* pBitmapSource);
HRESULT WicCreateDibSecton(HBITMAP& hBitmap, IWICBitmapSource* pBitmapSource);

#if 0 // unused, kept for future use
HRESULT WicCreateHBitmap(HBITMAP& hBitmap, IWICBitmapSource* pBitmapSource);

HRESULT WicCreateBitmap(IWICBitmap** ppBitmap, IWICBitmapSource* pBitmapSource);
HRESULT WicCreateBitmapScaled(IWICBitmap** ppBitmap, UINT width, UINT height, IWICBitmapSource* pBitmapSource);

HRESULT WicSaveImage(
    BYTE* src, const UINT pitch,
    const UINT width, const UINT height,
    const WICPixelFormatGUID pixelFormat,
    const int quality, // for JPEG
    const std::wstring_view filename,
    BYTE* output, size_t& outLen
);
#endif
