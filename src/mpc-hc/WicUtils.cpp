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

#include "stdafx.h"
#include "WicUtils.h"

 // CWICImagingFactory

CWICImagingFactory::CWICImagingFactory()
{
    HRESULT hr = CoCreateInstance(
        CLSID_WICImagingFactory1, // we use CLSID_WICImagingFactory1 to support Windows 7 without Platform Update
        nullptr,
        CLSCTX_INPROC_SERVER,
        IID_IWICImagingFactory,
        (LPVOID*)&m_pWICImagingFactory
    );
    ASSERT(SUCCEEDED(hr));
}

IWICImagingFactory* CWICImagingFactory::GetFactory() const
{
    ASSERT(m_pWICImagingFactory);
    return m_pWICImagingFactory;
}

/////////////////////

#if 0 // unused, kept for future use

enum ColorSystem_t {
    CS_YUV,
    CS_RGB,
    CS_GRAY,
    CS_IDX,
};

struct PixelFormatDesc {
    WICPixelFormatGUID wicpfguid = GUID_NULL;
    const wchar_t*     str       = nullptr;
    int                cdepth    = 0;
    int                depth     = 0;
    ColorSystem_t      cstype    = CS_RGB;
    int                alpha     = 0;
};

static const PixelFormatDesc s_UnknownPixelFormatDesc = {};

static const PixelFormatDesc s_PixelFormatDescs[] = {
    { GUID_WICPixelFormat1bppIndexed, L"1bppIndexed",  8,  1, CS_IDX,  1 },
    { GUID_WICPixelFormat2bppIndexed, L"2bppIndexed",  8,  2, CS_IDX,  1 },
    { GUID_WICPixelFormat4bppIndexed, L"4bppIndexed",  8,  4, CS_IDX,  1 },
    { GUID_WICPixelFormat8bppIndexed, L"8bppIndexed",  8,  8, CS_IDX,  1 },
    { GUID_WICPixelFormatBlackWhite , L"BlackWhite",   1,  1, CS_GRAY, 0 },
    { GUID_WICPixelFormat2bppGray,    L"2bppGray",     2,  2, CS_GRAY, 0 },
    { GUID_WICPixelFormat4bppGray,    L"4bppGray",     4,  4, CS_GRAY, 0 },
    { GUID_WICPixelFormat8bppGray,    L"8bppGray",     8,  8, CS_GRAY, 0 },
    { GUID_WICPixelFormat16bppBGR555, L"16bppBGR555",  5, 16, CS_RGB,  0 },
    { GUID_WICPixelFormat16bppBGR565, L"16bppBGR565",  6, 16, CS_RGB,  0 },
    { GUID_WICPixelFormat16bppGray,   L"16bppGray",   16, 16, CS_GRAY, 0 },
    { GUID_WICPixelFormat24bppBGR,    L"24bppBGR",     8, 24, CS_RGB,  0 },
    { GUID_WICPixelFormat24bppRGB,    L"24bppRGB",     8, 24, CS_RGB,  0 },
    { GUID_WICPixelFormat32bppBGR,    L"32bppBGR",     8, 32, CS_RGB,  0 },
    { GUID_WICPixelFormat32bppBGRA,   L"32bppBGRA",    8, 32, CS_RGB,  1 },
    { GUID_WICPixelFormat32bppPBGRA,  L"32bppPBGRA",   8, 32, CS_RGB,  2 },
    { GUID_WICPixelFormat32bppRGB,    L"32bppRGB",     8, 32, CS_RGB,  0 },
    { GUID_WICPixelFormat32bppRGBA,   L"32bppRGBA",    8, 32, CS_RGB,  1 },
    { GUID_WICPixelFormat32bppPRGBA,  L"32bppPRGBA",   8, 32, CS_RGB,  2 },
    { GUID_WICPixelFormat48bppRGB,    L"48bppRGB",    16, 48, CS_RGB,  0 },
    { GUID_WICPixelFormat48bppBGR,    L"48bppBGR",    16, 48, CS_RGB,  0 },
    { GUID_WICPixelFormat64bppRGB,    L"64bppRGB",    16, 64, CS_RGB,  0 },
    { GUID_WICPixelFormat64bppRGBA,   L"64bppRGBA",   16, 64, CS_RGB,  1 },
    { GUID_WICPixelFormat64bppBGRA,   L"64bppBGRA",   16, 64, CS_RGB,  1 },
    { GUID_WICPixelFormat64bppPRGBA,  L"64bppPRGBA",  16, 64, CS_RGB,  2 },
    { GUID_WICPixelFormat64bppPBGRA,  L"64bppPBGRA",  16, 64, CS_RGB,  2 },
};

static const PixelFormatDesc* GetPixelFormatDesc(const WICPixelFormatGUID guid)
{
    for (const auto& pfd : s_PixelFormatDescs) {
        if (pfd.wicpfguid == guid) {
            return &pfd;
        }
    }
    return &s_UnknownPixelFormatDesc;
}

HRESULT WicGetCodecs(std::vector<WICCodecInfo_t>& codecs, bool bEncoder)
{
    IWICImagingFactory* pWICFactory = CWICImagingFactory::GetInstance().GetFactory();
    if (!pWICFactory) {
        return E_NOINTERFACE;
    }

    codecs.clear();
    CComPtr<IEnumUnknown> pEnum;

    HRESULT hr = pWICFactory->CreateComponentEnumerator(bEncoder ? WICEncoder : WICDecoder, WICComponentEnumerateDefault, &pEnum);

    if (SUCCEEDED(hr)) {
        GUID containerFormat = {};
        ULONG cbFetched = 0;
        CComPtr<IUnknown> pElement;

        while (S_OK == pEnum->Next(1, &pElement, &cbFetched)) {
            CComQIPtr<IWICBitmapCodecInfo> pCodecInfo(pElement);

            HRESULT hr2 = pCodecInfo->GetContainerFormat(&containerFormat);
            if (SUCCEEDED(hr2)) {
                UINT cbActual = 0;
                WICCodecInfo_t codecInfo = { containerFormat };

                hr2 = pCodecInfo->GetFriendlyName(0, nullptr, &cbActual);
                if (SUCCEEDED(hr2) && cbActual) {
                    codecInfo.name.resize(cbActual);
                    hr2 = pCodecInfo->GetFriendlyName(codecInfo.name.size(), codecInfo.name.data(), &cbActual);
                }

                hr2 = pCodecInfo->GetFileExtensions(0, nullptr, &cbActual);
                if (SUCCEEDED(hr2) && cbActual) {
                    codecInfo.fileExts.resize(cbActual);
                    hr2 = pCodecInfo->GetFileExtensions(codecInfo.fileExts.size(), codecInfo.fileExts.data(), &cbActual);
                }

                hr2 = pCodecInfo->GetPixelFormats(0, nullptr, &cbActual);
                if (SUCCEEDED(hr2) && cbActual) {
                    codecInfo.pixelFmts.resize(cbActual);
                    hr2 = pCodecInfo->GetPixelFormats(codecInfo.pixelFmts.size(), codecInfo.pixelFmts.data(), &cbActual);
                }

                codecs.emplace_back(codecInfo);
            }
            pElement.Release();
        }
    }

    return hr;
}

HRESULT WicCheckComponent(const GUID guid)
{
    IWICImagingFactory* pWICFactory = CWICImagingFactory::GetInstance().GetFactory();
    if (!pWICFactory) {
        return E_NOINTERFACE;
    }

    CComPtr<IWICComponentInfo> pComponentInfo;
    HRESULT hr = pWICFactory->CreateComponentInfo(guid, &pComponentInfo);

#if _DEBUG && 0
    std::wstring name;
    UINT cbActual = 0;
    HRESULT hr2 = pComponentInfo->GetFriendlyName(0, nullptr, &cbActual);
    if (SUCCEEDED(hr2) && cbActual) {
        name.resize(cbActual);
        hr2 = pComponentInfo->GetFriendlyName(name.size(), name.data(), &cbActual);
    }
#endif // DEBUG

    return hr;
}

#endif

#if 0
// Workaround when IWICImagingFactory::CreateDecoderFromStream fails with WINCODEC_ERR_PROPERTYUNEXPECTEDTYPE error for some JPEGs.
HRESULT WicDecodeImageOle(IWICImagingFactory* pWICFactory, IWICBitmap** ppBitmap, const bool pma, IStream* pIStream)
{
    const WICPixelFormatGUID dstPixelFormat = pma ? GUID_WICPixelFormat32bppPBGRA : GUID_WICPixelFormat32bppBGRA;
    CComPtr<IWICBitmap> pTempBitmap;

    LPPICTURE pPicture = nullptr;
    HRESULT hr = ::OleLoadPicture(pIStream, 0, TRUE, IID_IPicture, (LPVOID*)&pPicture);
    if (SUCCEEDED(hr)) {
        HBITMAP hBitmap = nullptr;
        hr = pPicture->get_Handle((OLE_HANDLE*)&hBitmap);
        if (SUCCEEDED(hr)) {
            hr = pWICFactory->CreateBitmapFromHBITMAP(hBitmap, nullptr, WICBitmapIgnoreAlpha, &pTempBitmap);
        }
        pPicture->Release();
    }

    WICPixelFormatGUID pixelFormat = {};
    if (SUCCEEDED(hr)) {
        hr = pTempBitmap->GetPixelFormat(&pixelFormat);
        if (SUCCEEDED(hr)) {
            if (IsEqualGUID(pixelFormat, dstPixelFormat)) {
                *ppBitmap = pTempBitmap.Detach();

                return hr;
            }
        }
    }

    CComPtr<IWICBitmapSource> pBitmapSource;
    if (SUCCEEDED(hr)) {
        hr = WICConvertBitmapSource(dstPixelFormat, pTempBitmap, &pBitmapSource);
    }

    if (SUCCEEDED(hr)) {
        hr = pWICFactory->CreateBitmapFromSource(pBitmapSource, WICBitmapCacheOnLoad, ppBitmap);
    }

    return hr;
}
#endif

HRESULT WicDecodeImage(IWICImagingFactory* pWICFactory, IWICBitmap** ppBitmap, const bool pma, IWICBitmapDecoder* pDecoder)
{
    CComPtr<IWICBitmapFrameDecode> pFrameDecode;

    const WICPixelFormatGUID dstPixelFormat = pma ? GUID_WICPixelFormat32bppPBGRA : GUID_WICPixelFormat32bppBGRA;
    WICPixelFormatGUID pixelFormat = {};
    CComPtr<IWICBitmapSource> pBitmapSource;

    HRESULT hr = pDecoder->GetFrame(0, &pFrameDecode);
    if (SUCCEEDED(hr)) {
        hr = pFrameDecode->GetPixelFormat(&pixelFormat);
    }
    if (SUCCEEDED(hr)) {
        // need premultiplied alpha
        if (IsEqualGUID(pixelFormat, dstPixelFormat)) {
            pBitmapSource = pFrameDecode;
        }
        else {
            hr = WICConvertBitmapSource(dstPixelFormat, pFrameDecode, &pBitmapSource);
        }
    }
    if (SUCCEEDED(hr)) {
        hr = pWICFactory->CreateBitmapFromSource(pBitmapSource, WICBitmapCacheOnLoad, ppBitmap);
    }

    return hr;
}

HRESULT WicLoadImage(IWICBitmap** ppBitmap, const bool pma, const std::wstring_view filename)
{
    IWICImagingFactory* pWICFactory = CWICImagingFactory::GetInstance().GetFactory();
    if (!pWICFactory) {
        return E_NOINTERFACE;
    }

    if (filename.empty()) {
        return E_INVALIDARG;
    }

    CComPtr<IWICBitmapDecoder> pDecoder;

    HRESULT hr = pWICFactory->CreateDecoderFromFilename(
        filename.data(),
        nullptr,
        GENERIC_READ,
        // Specify WICDecodeMetadataCacheOnDemand or some JPEGs will fail to load with a WINCODEC_ERR_PROPERTYUNEXPECTEDTYPE error.
        WICDecodeMetadataCacheOnDemand,
        &pDecoder
    );

    if (SUCCEEDED(hr)) {
        hr = WicDecodeImage(pWICFactory, ppBitmap, pma, pDecoder);
    }

    return hr;
}

HRESULT WicLoadImage(IWICBitmap** ppBitmap, const bool pma, BYTE* input, const size_t size)
{
    IWICImagingFactory* pWICFactory = CWICImagingFactory::GetInstance().GetFactory();
    if (!pWICFactory) {
        return E_NOINTERFACE;
    }

    if (!input || !size) {
        return E_INVALIDARG;
    }

    CComPtr<IWICStream> pStream;
    CComPtr<IWICBitmapDecoder> pDecoder;

    HRESULT hr = pWICFactory->CreateStream(&pStream);
    if (SUCCEEDED(hr)) {
        hr = pStream->InitializeFromMemory(input, size);
    }

    if (SUCCEEDED(hr)) {
        // Specify WICDecodeMetadataCacheOnDemand or some JPEGs will fail to load with a WINCODEC_ERR_PROPERTYUNEXPECTEDTYPE error.
        hr = pWICFactory->CreateDecoderFromStream(pStream, nullptr, WICDecodeMetadataCacheOnDemand, &pDecoder);
        if (SUCCEEDED(hr)) {
            hr = WicDecodeImage(pWICFactory, ppBitmap, pma, pDecoder);
        }
    }

    return hr;
}

HRESULT WicLoadImage(IWICBitmap** ppBitmap, const bool pma, IStream* pIStream)
{
    IWICImagingFactory* pWICFactory = CWICImagingFactory::GetInstance().GetFactory();
    if (!pWICFactory) {
        return E_NOINTERFACE;
    }

    if (!pIStream) {
        return E_POINTER;
    }

    CComPtr <IWICStream> pStream;
    CComPtr<IWICBitmapDecoder> pDecoder;

    HRESULT hr = pWICFactory->CreateStream(&pStream);
    if (SUCCEEDED(hr)) {
        hr = pStream->InitializeFromIStream(pIStream);
    }

    if (SUCCEEDED(hr)) {
        // Specify WICDecodeMetadataCacheOnDemand or some JPEGs will fail to load with a WINCODEC_ERR_PROPERTYUNEXPECTEDTYPE error
        hr = pWICFactory->CreateDecoderFromStream(pStream, nullptr, WICDecodeMetadataCacheOnDemand, &pDecoder);
        if (SUCCEEDED(hr)) {
            hr = WicDecodeImage(pWICFactory, ppBitmap, pma, pDecoder);
        }
    }

    return hr;
}

#if 0 // unused, kept for future use
HRESULT WicCreateHBitmap(HBITMAP& hBitmap, IWICBitmapSource* pBitmapSource)
{
    if (hBitmap != nullptr || !pBitmapSource) {
        return E_INVALIDARG;
    }

    UINT width = 0;
    UINT height = 0;

    HRESULT hr = pBitmapSource->GetSize(&width, &height);
    if (SUCCEEDED(hr)) {
        const UINT bitmapsize = width * height * 4;

        std::unique_ptr<BYTE[]> buffer(new(std::nothrow) BYTE[bitmapsize]);
        if (!buffer) {
            return E_OUTOFMEMORY;
        }

        hr = pBitmapSource->CopyPixels(nullptr, width * 4, bitmapsize, buffer.get());

        if (SUCCEEDED(hr)) {
            hBitmap = CreateBitmap(width, height, 1, 32, buffer.get());
            if (!hBitmap) {
                return E_FAIL;
            }
        }
    }

    return hr;
}
#endif

HRESULT WicCreateDibSecton(HBITMAP& hBitmap, BYTE** ppData, BITMAPINFO& bminfo, IWICBitmapSource* pBitmapSource)
{
    if (hBitmap != nullptr || !pBitmapSource) {
        return E_INVALIDARG;
    }

    UINT width = 0;
    UINT height = 0;

    HRESULT hr = pBitmapSource->GetSize(&width, &height);
    if (SUCCEEDED(hr)) {
        const UINT bitmapsize = width * height * 4;

        bminfo = { sizeof(BITMAPINFOHEADER), (LONG)width, -(LONG)height, 1, 32, BI_RGB };
        // the "hdc" parameter is not needed for DIB_RGB_COLORS
        hBitmap = CreateDIBSection(nullptr, &bminfo, DIB_RGB_COLORS, (void**)ppData, nullptr, 0);
        if (!hBitmap) {
            return E_FAIL;
        }
        hr = pBitmapSource->CopyPixels(nullptr, width * 4, bitmapsize, *ppData);
    }

    return hr;
}

HRESULT WicCreateDibSecton(HBITMAP& hBitmap, IWICBitmapSource* pBitmapSource)
{
    BYTE* pData = nullptr;
    BITMAPINFO bminfo;

    return WicCreateDibSecton(hBitmap, &pData, bminfo, pBitmapSource);
}

#if 0 // unused, kept for future use
HRESULT WicCreateBitmap(IWICBitmap** ppBitmap, IWICBitmapSource* pBitmapSource)
{
    IWICImagingFactory* pWICFactory = CWICImagingFactory::GetInstance().GetFactory();
    if (!pWICFactory) {
        return E_NOINTERFACE;
    }

    HRESULT hr = pWICFactory->CreateBitmapFromSource(pBitmapSource, WICBitmapCacheOnLoad, ppBitmap);

    return hr;
}

HRESULT WicCreateBitmapScaled(IWICBitmap** ppBitmap, UINT width, UINT height, IWICBitmapSource* pBitmapSource)
{
    IWICImagingFactory* pWICFactory = CWICImagingFactory::GetInstance().GetFactory();
    if (!pWICFactory) {
        return E_NOINTERFACE;
    }

    CComPtr<IWICBitmapScaler> pScaler;
    HRESULT hr = pWICFactory->CreateBitmapScaler(&pScaler);
    if (SUCCEEDED(hr)) {
        hr = pScaler->Initialize(pBitmapSource, width, height, WICBitmapInterpolationModeFant);
    }
    if (SUCCEEDED(hr)) {
        hr = pWICFactory->CreateBitmapFromSource(pScaler, WICBitmapCacheOnLoad, ppBitmap);
    }

    return hr;
}

HRESULT WicSaveImage(
    BYTE* src, const UINT pitch,
    const UINT width, const UINT height,
    const WICPixelFormatGUID pixelFormat,
    const int quality,
    const std::wstring_view filename,
    BYTE* output, size_t& outLen)
{
    IWICImagingFactory* pWICFactory = CWICImagingFactory::GetInstance().GetFactory();
    if (!pWICFactory) {
        return E_NOINTERFACE;
    }

    if (!src) {
        return E_POINTER;
    }
    if (!pitch || !width || !height) {
        return E_INVALIDARG;
    }

    CComPtr<IWICStream> pStream;
    CComPtr<IWICBitmapEncoder> pEncoder;
    CComPtr<IWICBitmapFrameEncode> pFrame;
    CComPtr<IPropertyBag2> pPropertyBag2;
    WICPixelFormatGUID convertFormat = {};
    GUID containerFormat = {};


    auto pixFmtDesc = GetPixelFormatDesc(pixelFormat);

    std::wstring ext;
    ext.assign(filename, filename.find_last_of(L'.'));
    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);

    if (ext == L".bmp") {
        containerFormat = GUID_ContainerFormatBmp;
        if (pixFmtDesc->cstype == CS_GRAY || pixFmtDesc->cstype == CS_IDX) {
            convertFormat =
                (pixFmtDesc->depth == 1) ? GUID_WICPixelFormat1bppIndexed :
                (pixFmtDesc->depth <= 4) ? GUID_WICPixelFormat4bppIndexed :
                GUID_WICPixelFormat8bppIndexed;
        }
        else {
            convertFormat = (pixFmtDesc->alpha) ? GUID_WICPixelFormat32bppBGRA : GUID_WICPixelFormat24bppBGR;
        }
    }
    else if (ext == L".png") {
        containerFormat = GUID_ContainerFormatPng;
        if (pixFmtDesc->cstype == CS_GRAY || pixFmtDesc->cstype == CS_IDX) {
            convertFormat = pixelFormat;
        }
        else if (pixFmtDesc->alpha) {
            convertFormat = (pixFmtDesc->depth == 64) ? GUID_WICPixelFormat64bppBGRA : GUID_WICPixelFormat32bppBGRA;
        }
        else {
            convertFormat = (pixFmtDesc->depth == 48) ? GUID_WICPixelFormat48bppBGR : GUID_WICPixelFormat24bppBGR;
        }
    }
    else if (ext == L".jpg" || ext == L".jpeg") {
        containerFormat = GUID_ContainerFormatJpeg;
        if (pixFmtDesc->cstype == CS_GRAY) {
            convertFormat = GUID_WICPixelFormat8bppGray;
        } else {
            convertFormat = GUID_WICPixelFormat24bppBGR;
        }
    }
    else {
        return E_INVALIDARG;
    }

    HRESULT hr = pWICFactory->CreateStream(&pStream);
    if (SUCCEEDED(hr)) {
        if (filename.length() > ext.length()) {
            hr = pStream->InitializeFromFilename(filename.data(), GENERIC_WRITE);
        }
        else if (output && outLen) {
            hr = pStream->InitializeFromMemory(output, outLen);
        }
        else {
            return E_INVALIDARG;
        }
    }
    if (SUCCEEDED(hr)) {
        hr = pWICFactory->CreateEncoder(containerFormat, nullptr, &pEncoder);
    }
    if (SUCCEEDED(hr)) {
        hr = pEncoder->Initialize(pStream, WICBitmapEncoderNoCache);
    }
    if (SUCCEEDED(hr)) {
        hr = pEncoder->CreateNewFrame(&pFrame, &pPropertyBag2);
    }
    if (SUCCEEDED(hr)) {
        if (containerFormat == GUID_ContainerFormatJpeg) {
            PROPBAG2 option = {};
            option.pstrName = (LPOLESTR)L"ImageQuality";
            VARIANT varValue;
            VariantInit(&varValue);
            varValue.vt = VT_R4;
            varValue.fltVal = quality / 100.0f;
            hr = pPropertyBag2->Write(1, &option, &varValue);
#if 0
            if (1/*jpegSubsamplingOff*/) {
                option.pstrName = L"JpegYCrCbSubsampling";
                varValue.vt = VT_UI1;
                varValue.bVal = WICJpegYCrCbSubsampling444;
                hr = pPropertyBag2->Write(1, &option, &varValue);
            }
#endif
        }
        hr = pFrame->Initialize(pPropertyBag2);
    }
    if (SUCCEEDED(hr)) {
        hr = pFrame->SetSize(width, height);
    }
    if (SUCCEEDED(hr)) {
        hr = pFrame->SetPixelFormat(&convertFormat);
    }
    if (SUCCEEDED(hr)) {
        if (IsEqualGUID(pixelFormat, convertFormat)) {
            hr = pFrame->WritePixels(height, pitch, pitch * height, src);
        }
        else {
            CComPtr<IWICBitmap> pBitmap;
            CComPtr<IWICBitmapSource> pConvertBitmapSource;

            hr = pWICFactory->CreateBitmapFromMemory(width, height, pixelFormat, pitch, pitch * height, src, &pBitmap);
            if (SUCCEEDED(hr)) {
                hr = WICConvertBitmapSource(convertFormat, pBitmap, &pConvertBitmapSource);
            }
            if (SUCCEEDED(hr)) {
                hr = pFrame->WriteSource(pConvertBitmapSource, nullptr);
            }
        }
    }
    if (SUCCEEDED(hr)) {
        hr = pFrame->Commit();
    }
    if (SUCCEEDED(hr)) {
        hr = pEncoder->Commit();
    }
    if (SUCCEEDED(hr)) {
        // get the real size of data for IWICStream, which can be placed in memory
        LARGE_INTEGER li = {};
        ULARGE_INTEGER uli = {};
        hr = pStream->Seek(li, STREAM_SEEK_CUR, &uli); // look at the current position, not the end
        outLen = (size_t)uli.QuadPart;
    }

    return hr;
}
#endif
