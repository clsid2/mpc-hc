/*
 * (C) 2026 see Authors.txt
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
#include "Snapshot.h"
#include "MainFrm.h"
#include "mplayerc.h"
#include "SaveImageDialog.h"
#include "SaveThumbnailsDialog.h"
#include "CMPCThemeUtil.h"
#include "../Subtitles/PGSSub.h"
#include "../Subtitles/RTS.h"
#include <PathUtils.h>
#include <DSUtil.h>
#include <mvrInterfaces.h>
#include "stb/deprecated/stb_image_resize2.h"
#include "stb/stb_image_write.h"

BEGIN_MESSAGE_MAP(CSnapshot, CCmdTarget)
    ON_COMMAND(ID_FILE_SAVE_IMAGE, OnFileSaveImage)
    ON_UPDATE_COMMAND_UI(ID_FILE_SAVE_IMAGE, OnUpdateFileSaveImage)
    ON_COMMAND(ID_FILE_SAVE_IMAGE_AUTO, OnFileSaveImageAuto)
    ON_UPDATE_COMMAND_UI(ID_FILE_SAVE_IMAGE_AUTO, OnUpdateFileSaveImage)
    ON_COMMAND(ID_CMDLINE_SAVE_THUMBNAILS, OnCmdLineSaveThumbnails)
    ON_COMMAND(ID_FILE_SAVE_THUMBNAILS, OnFileSaveThumbnails)
    ON_UPDATE_COMMAND_UI(ID_FILE_SAVE_THUMBNAILS, OnUpdateFileSaveThumbnails)
END_MESSAGE_MAP()

CSnapshot::CSnapshot(CMainFrame& frame)
    : m_frame(frame)
{
}

bool CSnapshot::GetDIB(BYTE** ppData, long& size, bool fSilent)
{
    if (!ppData) {
        return false;
    }
    if (m_frame.GetLoadState() != MLS::LOADED || m_frame.m_fAudioOnly) {
        return false;
    }
    OAFilterState fs = m_frame.GetMediaState();
    if (fs != State_Paused && fs != State_Running) {
        return false;
    }

    *ppData = nullptr;
    size = 0;

    if (fs == State_Running && !m_frame.Graph().m_pCAP) {
        m_frame.MediaControlPause(true); // wait for completion
    }

    HRESULT hr = S_OK;
    CString errmsg;

    do {
        if (m_frame.Graph().m_pCAP) {
            hr = m_frame.Graph().m_pCAP->GetDIB(nullptr, (DWORD*)&size);
            if (FAILED(hr)) {
                errmsg.Format(IDS_GETDIB_FAILED, hr);
                break;
            }

            *ppData = DEBUG_NEW BYTE[size];
            if (!(*ppData)) {
                return false;
            }

            hr = m_frame.Graph().m_pCAP->GetDIB(*ppData, (DWORD*)&size);
            if (FAILED(hr)) {
                errmsg.Format(IDS_GETDIB_FAILED, hr);
                break;
            }
        } else if (m_frame.Graph().m_pMFVDC) {
            // Capture with EVR
            BITMAPINFOHEADER bih = {sizeof(BITMAPINFOHEADER)};
            BYTE* pDib;
            DWORD dwSize;
            REFERENCE_TIME rtImage = 0;
            hr = m_frame.Graph().m_pMFVDC->GetCurrentImage(&bih, &pDib, &dwSize, &rtImage);
            if (FAILED(hr) || dwSize == 0) {
                errmsg.Format(IDS_GETCURRENTIMAGE_FAILED, hr);
                break;
            }

            size = (long)dwSize + sizeof(BITMAPINFOHEADER);
            *ppData = DEBUG_NEW BYTE[size];
            if (!(*ppData)) {
                return false;
            }
            memcpy_s(*ppData, size, &bih, sizeof(BITMAPINFOHEADER));
            memcpy_s(*ppData + sizeof(BITMAPINFOHEADER), size - sizeof(BITMAPINFOHEADER), pDib, dwSize);
            CoTaskMemFree(pDib);
        } else {
            hr = m_frame.Graph().m_pBV->GetCurrentImage(&size, nullptr);
            if (FAILED(hr) || size == 0) {
                errmsg.Format(IDS_GETCURRENTIMAGE_FAILED, hr);
                break;
            }

            *ppData = DEBUG_NEW BYTE[size];
            if (!(*ppData)) {
                return false;
            }

            hr = m_frame.Graph().m_pBV->GetCurrentImage(&size, (long*)*ppData);
            if (FAILED(hr)) {
                errmsg.Format(IDS_GETCURRENTIMAGE_FAILED, hr);
                break;
            }
        }
    } while (0);

    if (!fSilent) {
        if (!errmsg.IsEmpty()) {
            AfxMessageBox(errmsg, MB_OK);
        }
    }

    if (fs == State_Running && m_frame.GetMediaState() != State_Running) {
        m_frame.MediaControlRun();
    }

    if (FAILED(hr)) {
        SAFE_DELETE_ARRAY(*ppData);
        return false;
    }

    return true;
}

#if MPC_SMTC_VIDEO_THUMBNAIL
// Callback for stb_image_write to append to vector
static void stbi_write_to_vector(void* context, void* data, int size) {
    std::vector<BYTE>* vec = (std::vector<BYTE>*)context;
    size_t oldSize = vec->size();
    vec->resize(oldSize + size);
    memcpy(vec->data() + oldSize, data, size);
}
#endif

BYTE* CSnapshot::ConvertDIBTo24bppRGB(BYTE* pData, long size, int& outWidth, int& outHeight, int& outPitch)
{
    if (!pData || size < (long)sizeof(BITMAPINFOHEADER)) {
        return nullptr;
    }

    PBITMAPINFO bi = reinterpret_cast<PBITMAPINFO>(pData);
    PBITMAPINFOHEADER bih = &bi->bmiHeader;
    int bpp = bih->biBitCount;

    if (bpp != 16 && bpp != 24 && bpp != 32) {
        return nullptr;
    }

    bool topdown = (bih->biHeight < 0);
    int w = bih->biWidth;
    int h = abs(bih->biHeight);
    int srcpitch = w * (bpp >> 3);

    // biSizeImage may be 0 or too small for BI_RGB, so the pixels the copy below reads must fit in what we were given
    if (w <= 0 || h <= 0 || (LONGLONG)sizeof(BITMAPINFOHEADER) + (LONGLONG)w * (bpp >> 3) * h > size) {
        return nullptr;
    }
    int dstpitch = (w * 3 + 3) / 4 * 4; // round w * 3 to next multiple of 4

    BYTE* p = DEBUG_NEW BYTE[dstpitch * h];
    const BYTE* src = pData + sizeof(*bih);

    if (topdown) {
        BitBltFromRGBToRGB(w, h, p, dstpitch, 24, (BYTE*)src, srcpitch, bpp);
    } else {
        BitBltFromRGBToRGB(w, h, p, dstpitch, 24, (BYTE*)src + srcpitch * (h - 1), -srcpitch, bpp);
    }

    outWidth = w;
    outHeight = h;
    outPitch = dstpitch;
    return p;
}

void CSnapshot::SaveDIB(LPCTSTR fn, BYTE* pData, long size)
{
    CLongPath path(fn);

    // GDI+ refuses to write to a path at or beyond MAX_PATH unless it carries the long path prefix
    CString target(fn);
    ExtendMaxPathLengthIfNeeded(target, true);

    int w, h, dstpitch;
    BYTE* p = ConvertDIBTo24bppRGB(pData, size, w, h, dstpitch);
    if (!p) {
        AfxMessageBox(IDS_SCREENSHOT_ERROR, MB_ICONWARNING | MB_OK, 0);
        return;
    }

    {
        Gdiplus::GdiplusStartupInput gdiplusStartupInput;
        ULONG_PTR gdiplusToken;
        Gdiplus::GdiplusStartup(&gdiplusToken, &gdiplusStartupInput, nullptr);

        Gdiplus::Bitmap* bm = new Gdiplus::Bitmap(w, h, dstpitch, PixelFormat24bppRGB, p);

        UINT num;       // number of image encoders
        UINT arraySize; // size, in bytes, of the image encoder array

        // How many encoders are there?
        // How big (in bytes) is the array of all ImageCodecInfo objects?
        Gdiplus::GetImageEncodersSize(&num, &arraySize);

        // Create a buffer large enough to hold the array of ImageCodecInfo
        // objects that will be returned by GetImageEncoders.
        Gdiplus::ImageCodecInfo* pImageCodecInfo = (Gdiplus::ImageCodecInfo*)DEBUG_NEW BYTE[arraySize];

        // GetImageEncoders creates an array of ImageCodecInfo objects
        // and copies that array into a previously allocated buffer.
        // The third argument, imageCodecInfos, is a pointer to that buffer.
        Gdiplus::GetImageEncoders(num, arraySize, pImageCodecInfo);

        Gdiplus::EncoderParameters* pEncoderParameters = nullptr;

        // Find the mime type based on the extension
        CString ext(path.GetExtension());
        CStringW mime;
        if (ext == _T(".jpg")) {
            mime = L"image/jpeg";

            // Set the encoder parameter for jpeg quality
            pEncoderParameters = DEBUG_NEW Gdiplus::EncoderParameters;
            ULONG quality = AfxGetAppSettings().nJpegQuality;

            pEncoderParameters->Count = 1;
            pEncoderParameters->Parameter[0].Guid = Gdiplus::EncoderQuality;
            pEncoderParameters->Parameter[0].Type = Gdiplus::EncoderParameterValueTypeLong;
            pEncoderParameters->Parameter[0].NumberOfValues = 1;
            pEncoderParameters->Parameter[0].Value = &quality;
        } else if (ext == _T(".bmp")) {
            mime = L"image/bmp";
        } else {
            mime = L"image/png";
        }

        // Get the encoder clsid
        CLSID encoderClsid = CLSID_NULL;
        for (UINT i = 0; i < num && encoderClsid == CLSID_NULL; i++) {
            if (wcscmp(pImageCodecInfo[i].MimeType, mime) == 0) {
                encoderClsid = pImageCodecInfo[i].Clsid;
            }
        }

        Gdiplus::Status s = bm->Save(target, &encoderClsid, pEncoderParameters);

        // All GDI+ objects must be destroyed before GdiplusShutdown is called
        delete bm;
        delete [] pImageCodecInfo;
        delete pEncoderParameters;
        Gdiplus::GdiplusShutdown(gdiplusToken);
        delete [] p;

        if (s != Gdiplus::Ok) {
            AfxMessageBox(IDS_SCREENSHOT_ERROR, MB_ICONWARNING | MB_OK, 0);
            return;
        }
    }

    path.m_strPath.Replace(_T("\\\\"), _T("\\"));

    m_frame.SendStatusMessage(m_frame.m_wndStatusBar.PreparePathStatusMessage(path), 3000);
}

#if MPC_SMTC_VIDEO_THUMBNAIL
bool CSnapshot::CaptureVideoThumbnail(std::vector<BYTE>& thumbnail)
{
    // Get the current video frame as DIB
    std::vector<BYTE> dib;
    CString errmsg;
    HRESULT hr = GetCurrentFrame(dib, errmsg);
    if (FAILED(hr) || dib.empty()) {
        return false;
    }

    // Convert DIB to 24bpp BGR
    int w, h, dstpitch;
    BYTE* bgr = ConvertDIBTo24bppRGB(dib.data(), (long)dib.size(), w, h, dstpitch);
    if (!bgr) {
        return false;
    }

    // Downscale to at most 320 pixels wide, preserving aspect ratio
    int tw = w;
    int th = h;
    if (tw > 320) {
        th = std::max(1, MulDiv(h, 320, w));
        tw = 320;
    }

    // Allocate buffer for RGB output (tightly packed, no padding)
    int rgbPitch = tw * 3;
    BYTE* rgb = DEBUG_NEW BYTE[rgbPitch * th];

    // Downscale and convert BGR to RGB using stb_image_resize2
    STBIR_RESIZE resize;
    stbir_resize_init(&resize, bgr, w, h, dstpitch, rgb, tw, th, rgbPitch, STBIR_BGR, STBIR_TYPE_UINT8);
    stbir_set_pixel_layouts(&resize, STBIR_BGR, STBIR_RGB);
    stbir_resize_extended(&resize);

    delete[] bgr;

    // Encode to JPEG using stb_image_write
    int quality = AfxGetAppSettings().nJpegQuality;
    int result = stbi_write_jpg_to_func(stbi_write_to_vector, &thumbnail, tw, th, 3, rgb, quality);

    delete[] rgb;
    return result != 0;
}
#endif

HRESULT GetBasicVideoFrame(IBasicVideo* pBasicVideo, std::vector<BYTE>& dib) {
    // IBasicVideo::GetCurrentImage() gives the original frame

    long size;

    HRESULT hr = pBasicVideo->GetCurrentImage(&size, nullptr);
    if (FAILED(hr)) {
        return hr;
    }
    if (size <= 0) {
        return E_ABORT;
    }

    dib.resize(size);

    hr = pBasicVideo->GetCurrentImage(&size, (long*)dib.data());
    if (FAILED(hr)) {
        dib.clear();
    }

    return hr;
}

HRESULT GetVideoDisplayControlFrame(IMFVideoDisplayControl* pVideoDisplayControl, std::vector<BYTE>& dib) {
    // IMFVideoDisplayControl::GetCurrentImage() gives the displayed frame

    BITMAPINFOHEADER	bih = { sizeof(BITMAPINFOHEADER) };
    BYTE* pDib;
    DWORD				size;
    REFERENCE_TIME		rtImage = 0;

    HRESULT hr = pVideoDisplayControl->GetCurrentImage(&bih, &pDib, &size, &rtImage);
    if (S_OK != hr) {
        return hr;
    }
    if (size == 0) {
        return E_ABORT;
    }

    dib.resize(sizeof(BITMAPINFOHEADER) + size);

    memcpy(dib.data(), &bih, sizeof(BITMAPINFOHEADER));
    memcpy(dib.data() + sizeof(BITMAPINFOHEADER), pDib, size);
    CoTaskMemFree(pDib);

    return hr;
}

HRESULT GetMadVRFrameGrabberFrame(IMadVRFrameGrabber* pMadVRFrameGrabber, std::vector<BYTE>& dib, bool displayed) {
    LPVOID dibImage = nullptr;
    HRESULT hr;

    if (displayed) {
        hr = pMadVRFrameGrabber->GrabFrame(ZOOM_PLAYBACK_SIZE, 0, 0, 0, 0, 0, &dibImage, 0);
    } else {
        hr = pMadVRFrameGrabber->GrabFrame(ZOOM_ENCODED_SIZE, 0, 0, 0, 0, 0, &dibImage, 0);
    }

    if (S_OK != hr) {
        return hr;
    }
    if (!dibImage) {
        return E_ABORT;
    }

    const BITMAPINFOHEADER* bih = (BITMAPINFOHEADER*)dibImage;

    dib.resize(sizeof(BITMAPINFOHEADER) + bih->biSizeImage);
    memcpy(dib.data(), dibImage, sizeof(BITMAPINFOHEADER) + bih->biSizeImage);
    LocalFree(dibImage);

    return hr;
}

HRESULT CSnapshot::GetDisplayedImage(std::vector<BYTE>& dib, CString& errmsg) {
    errmsg.Empty();
    HRESULT hr;

	if (m_frame.Graph().m_pCAP) {
		LPVOID dibImage = nullptr;
		hr = m_frame.Graph().m_pCAP->GetDisplayedImage(&dibImage);

		if (S_OK == hr && dibImage) {
			const BITMAPINFOHEADER* bih = (BITMAPINFOHEADER*)dibImage;
			dib.resize(sizeof(BITMAPINFOHEADER) + bih->biSizeImage);
			memcpy(dib.data(), dibImage, sizeof(BITMAPINFOHEADER) + bih->biSizeImage);
			LocalFree(dibImage);
		} else if (S_OK == hr) {
			hr = E_FAIL;
		}
	}
	else if (m_frame.Graph().m_pMFVDC) {
        hr = GetVideoDisplayControlFrame(m_frame.Graph().m_pMFVDC, dib);
    } else if (m_frame.Graph().m_pMVRFG) {
        hr = GetMadVRFrameGrabberFrame(m_frame.Graph().m_pMVRFG, dib, true);
    } else {
        hr = E_NOINTERFACE;
    }

    if (FAILED(hr)) {
		errmsg.Format(L"CMainFrame::GetCurrentImage() failed, 0x%08x", hr);
    }

    return hr;
}

HRESULT CSnapshot::GetCurrentFrame(std::vector<BYTE>& dib, CString& errmsg) {
    HRESULT hr = S_OK;
    errmsg.Empty();

    OAFilterState fs = m_frame.GetMediaState();
    if (m_frame.m_eMediaLoadState != MLS::LOADED || m_frame.m_fAudioOnly || (fs != State_Paused && fs != State_Running)) {
        return E_ABORT;
    }

    if (fs == State_Running && !m_frame.Graph().m_pCAP) {
        m_frame.MediaControlPause(true); //wait for completion
    }

    if (m_frame.Graph().m_pCAP) {
        DWORD size;
        hr = m_frame.Graph().m_pCAP->GetDIB(nullptr, &size);

        if (S_OK == hr) {
            dib.resize(size);
            hr = m_frame.Graph().m_pCAP->GetDIB(dib.data(), &size);
        }

        if (FAILED(hr)) {
            errmsg.Format(L"ISubPicAllocatorPresenter3::GetDIB() failed, 0x%08x", hr);
        }
    } else if (m_frame.Graph().m_pBV) {
        hr = GetBasicVideoFrame(m_frame.Graph().m_pBV, dib);

        if (hr == E_NOINTERFACE && m_frame.Graph().m_pMFVDC) {
            // hmm, EVR is not able to give the original frame, giving the displayed image
            hr = GetDisplayedImage(dib, errmsg);
        } else if (FAILED(hr)) {
            errmsg.Format(L"IBasicVideo::GetCurrentImage() failed, 0x%08x", hr);
        }
    } else {
        hr = E_POINTER;
        errmsg.Format(L"Interface not found!");
    }

    if (fs == State_Running && m_frame.GetMediaState() != State_Running) {
        m_frame.MediaControlRun();
    }

    return hr;
}

HRESULT CSnapshot::GetOriginalFrame(std::vector<BYTE>& dib, CString& errmsg) {
    HRESULT hr = S_OK;
    errmsg.Empty();

    if (m_frame.Graph().m_pMVRFG) {
        hr = GetMadVRFrameGrabberFrame(m_frame.Graph().m_pMVRFG, dib, false);
        if (FAILED(hr)) {
            errmsg.Format(L"IMadVRFrameGrabber::GrabFrame() failed, 0x%08x", hr);
        }
    } else {
        hr = GetCurrentFrame(dib, errmsg);
    }

    return hr;
}

HRESULT CSnapshot::RenderCurrentSubtitles(BYTE* pData) {
    ASSERT(m_frame.Graph().m_pCAP && AfxGetAppSettings().bSnapShotSubtitles && !m_frame.Graph().m_pMVRFG && AfxGetAppSettings().fEnableSubtitles && AfxGetAppSettings().IsISRAutoLoadEnabled());
    CheckPointer(pData, E_FAIL);
    HRESULT hr = S_FALSE;

    if (CComQIPtr<ISubPicProvider> pSubPicProvider = m_frame.m_pCurrentSubInput.pSubStream) {
        const PBITMAPINFOHEADER bih = (PBITMAPINFOHEADER)pData;
        const int width = bih->biWidth;
        const int height = abs(bih->biHeight);
        const bool topdown = bih->biHeight < 0;

        REFERENCE_TIME rtNow = 0;
        m_frame.Graph().m_pMS->GetCurrentPosition(&rtNow);

        int delay = m_frame.Graph().m_pCAP->GetSubtitleDelay();
        if (delay != 0) {
            if (delay > 0 && delay * 10000LL > rtNow) {
                return S_FALSE;
            } else {
                rtNow -= delay * 10000LL;
            }
        }

        int subWidth = width;
        int subHeight = height;
        bool needsResize = false;
        if (CPGSSub* pgsSub = dynamic_cast<CPGSSub*>(pSubPicProvider.p)) {
            CSize sz;
            if (SUCCEEDED(pgsSub->GetPresentationSegmentTextureSize(rtNow, sz))) {
                subWidth = sz.cx;
                subHeight = sz.cy;
                needsResize = true;
            }
        }

        SubPicDesc spdRender;
        
        spdRender.type = MSP_RGB32;
        spdRender.w = subWidth;
        spdRender.h = subHeight;
        spdRender.bpp = 32;
        spdRender.pitch = subWidth * 4;
        spdRender.vidrect = { 0, 0, width, height };
        spdRender.bits = DEBUG_NEW BYTE[spdRender.pitch * spdRender.h];
        
        CComPtr<CMemSubPicAllocator> pSubPicAllocator = DEBUG_NEW CMemSubPicAllocator(spdRender.type, CSize(spdRender.w, spdRender.h));

        CMemSubPic memSubPic(spdRender, pSubPicAllocator);
        memSubPic.SetInverseAlpha(false);
        memSubPic.ClearDirtyRect();

        RECT bbox = {};
        hr = pSubPicProvider->Render(spdRender, rtNow, m_frame.Graph().m_pCAP->GetFPS(), bbox);
        if (needsResize) {
            memSubPic.UnlockARGB();
        }

        if (S_OK == hr) {
            SubPicDesc spdTarget;
            spdTarget.type = MSP_RGB32;
            spdTarget.w = width;
            spdTarget.h = height;
            spdTarget.bpp = 32;
            spdTarget.pitch = topdown ? width * 4 : -width * 4;
            spdTarget.vidrect = { 0, 0, width, height };
            spdTarget.bits = (BYTE*)(bih + 1) + (topdown ? 0 : (width * 4) * (height - 1));

            hr = memSubPic.AlphaBlt(&spdRender.vidrect, &spdTarget.vidrect, &spdTarget);
        }
    }

    return hr;
}

void CSnapshot::SaveImage(LPCWSTR fn, bool displayed, bool includeSubtitles) {
    std::vector<BYTE> dib;
    CString errmsg;
    HRESULT hr;
    if (displayed) {
        hr = GetDisplayedImage(dib, errmsg);
    } else {
        hr = GetCurrentFrame(dib, errmsg);
        if (includeSubtitles && m_frame.Graph().m_pCAP && hr == S_OK) {
            RenderCurrentSubtitles(dib.data());
        }
    }

    if (hr == S_OK && !dib.empty()) {
        SaveDIB(fn, dib.data(), (long)dib.size());
        m_frame.m_OSD.DisplayMessage(OSD_TOPLEFT, ResStr(IDS_OSD_IMAGE_SAVED), 3000);
    } else {
        m_frame.m_OSD.DisplayMessage(OSD_TOPLEFT, errmsg, 3000);
    }
}

bool CSnapshot::SaveThumbnails(LPCTSTR fn)
{
    if (!m_frame.Graph().m_pMC || !m_frame.Graph().m_pMS || m_frame.GetPlaybackMode() != PM_FILE /*&& m_frame.GetPlaybackMode() != PM_DVD*/) {
        return false;
    }

    REFERENCE_TIME rtPos = m_frame.GetPos();
    REFERENCE_TIME rtDur = m_frame.GetDur();

    if (rtDur <= 0) {
        AfxMessageBox(IDS_THUMBNAILS_NO_DURATION, MB_ICONWARNING | MB_OK, 0);
        return false;
    }

    OAFilterState filterState = m_frame.UpdateCachedMediaState();
    bool bWasStopped = (filterState == State_Stopped);
    if (filterState != State_Paused) {
        m_frame.OnPlayPause();
    }

    CSize szVideoARCorrected, szVideo, szAR;

    if (m_frame.Graph().m_pCAP) {
        szVideo = m_frame.Graph().m_pCAP->GetVideoSize(false);
        szAR = m_frame.Graph().m_pCAP->GetVideoSize(true);
    } else if (m_frame.Graph().m_pMFVDC) {
        VERIFY(SUCCEEDED(m_frame.Graph().m_pMFVDC->GetNativeVideoSize(&szVideo, &szAR)));
    } else {
        VERIFY(SUCCEEDED(m_frame.Graph().m_pBV->GetVideoSize(&szVideo.cx, &szVideo.cy)));

        CComQIPtr<IBasicVideo2> pBV2 = m_frame.Graph().m_pBV;
        long lARx = 0, lARy = 0;
        if (pBV2 && SUCCEEDED(pBV2->GetPreferredAspectRatio(&lARx, &lARy)) && lARx > 0 && lARy > 0) {
            szAR.SetSize(lARx, lARy);
        }
    }

    if (szVideo.cx <= 0 || szVideo.cy <= 0) {
        AfxMessageBox(IDS_THUMBNAILS_NO_FRAME_SIZE, MB_ICONWARNING | MB_OK, 0);
        return false;
    }

    // with the overlay mixer IBasicVideo2 won't tell the new AR when changed dynamically
    DVD_VideoAttributes VATR;
    if (m_frame.GetPlaybackMode() == PM_DVD && SUCCEEDED(m_frame.Graph().m_pDVDI->GetCurrentVideoAttributes(&VATR))) {
        szAR.SetSize(VATR.ulAspectX, VATR.ulAspectY);
    }

    szVideoARCorrected = (szAR.cx <= 0 || szAR.cy <= 0) ? szVideo : CSize(MulDiv(szVideo.cy, szAR.cx, szAR.cy), szVideo.cy);

    const CAppSettings& s = AfxGetAppSettings();

    int cols = std::clamp(s.iThumbCols, 1, 16);
    int rows = std::clamp(s.iThumbRows, 1, 40);

    const int margin = 5;
    int width = std::clamp(s.iThumbWidth, 256, 3840);
    double fontscale = width / 1280.0;
    int fontsize = (int)(fontscale * 16);
    const int infoheight = 4 * fontsize + 6 + 2 * margin;
    // Values the dialog accepts (width 3840, 40 rows, 1 column) on a portrait video
    // make width * height * 4 wrap in int, so size the sheet in 64 bit and refuse
    // anything a bitmap cannot hold.
    const LONGLONG llHeight = (LONGLONG)width * szVideoARCorrected.cy / szVideoARCorrected.cx * rows / cols + infoheight;
    const LONGLONG llImageSize = (LONGLONG)width * llHeight * 4;
    if (llHeight <= 0 || llHeight > 32767 || llImageSize > INT_MAX - (LONGLONG)sizeof(BITMAPINFOHEADER)) {
        AfxMessageBox(IDS_OUT_OF_MEMORY, MB_ICONWARNING | MB_OK, 0);
        return false;
    }
    int height = (int)llHeight;

    int dibsize = sizeof(BITMAPINFOHEADER) + (int)llImageSize;

    CAutoVectorPtr<BYTE> dib;
    if (!dib.Allocate(dibsize)) {
        AfxMessageBox(IDS_OUT_OF_MEMORY, MB_ICONWARNING | MB_OK, 0);
        return false;
    }

    BITMAPINFOHEADER* bih = (BITMAPINFOHEADER*)(BYTE*)dib;
    ZeroMemory(bih, sizeof(BITMAPINFOHEADER));
    bih->biSize = sizeof(BITMAPINFOHEADER);
    bih->biWidth = width;
    bih->biHeight = height;
    bih->biPlanes = 1;
    bih->biBitCount = 32;
    bih->biCompression = BI_RGB;
    bih->biSizeImage = width * height * 4;
    memsetd(bih + 1, 0xffffff, bih->biSizeImage);

    SubPicDesc spd;
    spd.w = width;
    spd.h = height;
    spd.bpp = 32;
    spd.pitch = -width * 4;
    spd.vidrect = CRect(0, 0, width, height);
    spd.bits = (BYTE*)(bih + 1) + (width * 4) * (height - 1);

    bool darktheme = AppIsThemeLoaded() && s.eModernThemeMode == CMPCTheme::ModernThemeMode::DARK;

    int gradientBase = 0xe0;
    if (darktheme) {
        gradientBase = 0x00;
    }
    // Paint the background
    {
        BYTE* p = (BYTE*)spd.bits;
        for (int y = 0; y < spd.h; y++, p += spd.pitch) {
            for (int x = 0; x < spd.w; x++) {
                ((DWORD*)p)[x] = 0x010101 * (gradientBase + 0x08 * y / spd.h + 0x18 * (spd.w - x) / spd.w);
            }
        }
    }

    CCritSec csSubLock;
    RECT bbox;
    CSize szThumbnail((width - margin * 2) / cols - margin * 2, (height - margin * 2 - infoheight) / rows - margin * 2);
    // Ensure the thumbnails aren't ridiculously small so that the time indication can at least fit
    if (szThumbnail.cx < 60 || szThumbnail.cy < 20) {
        AfxMessageBox(IDS_THUMBNAIL_TOO_SMALL, MB_ICONWARNING | MB_OK, 0);
        return false;
    }

    // Allocate before muting, so a failure here does not leave the player silent
    std::unique_ptr<BYTE[]> thumb(new(std::nothrow) BYTE[szThumbnail.cx * szThumbnail.cy * 4]);
    if (!thumb) {
        AfxMessageBox(IDS_OUT_OF_MEMORY, MB_ICONWARNING | MB_OK, 0);
        return false;
    }

    m_frame.m_nVolumeBeforeFrameStepping = m_frame.m_wndToolBar.Volume;
    if (m_frame.Graph().m_pBA) {
        m_frame.Graph().m_pBA->put_Volume(-10000);
    }

    // Draw the thumbnails

    int pics = cols * rows;
    REFERENCE_TIME rtInterval = rtDur / (pics + 1LL);
    for (int i = 1; i <= pics; i++) {
        REFERENCE_TIME rt = rtInterval * i;
        // use a keyframe if close to target time
        if (rtInterval >= 100000000LL) {
            REFERENCE_TIME rtMaxDiff = std::min(100000000LL, rtInterval / 10); // no more than 10 sec
            rt = m_frame.GetClosestKeyFrame(rt, rtMaxDiff, rtMaxDiff);
        }

        m_frame.DoSeekTo(rt, false);
        m_frame.UpdateWindow();

        HRESULT hr = m_frame.Graph().m_pFS ? m_frame.Graph().m_pFS->Step(1, nullptr) : E_FAIL;
        if (FAILED(hr)) {
            if (m_frame.Graph().m_pBA) {
                m_frame.Graph().m_pBA->put_Volume(m_frame.m_nVolumeBeforeFrameStepping);
            }
            AfxMessageBox(IDS_FRAME_STEP_ERROR_RENDERER, MB_ICONEXCLAMATION | MB_OK, 0);
            return false;
        }

        bool abortloop = false;
        HANDLE hGraphEvent = nullptr;
        m_frame.Graph().m_pME->GetEventHandle((OAEVENT*)&hGraphEvent);
        while (hGraphEvent) {
            DWORD res = WaitForSingleObject(hGraphEvent, 5000);
            if (res == WAIT_OBJECT_0) {
                LONG evCode = 0;
                LONG_PTR evParam1, evParam2;
                while (m_frame.Graph().m_pME && SUCCEEDED(m_frame.Graph().m_pME->GetEvent(&evCode, &evParam1, &evParam2, 0))) {
                    m_frame.Graph().m_pME->FreeEventParams(evCode, evParam1, evParam2);
                    if (EC_STEP_COMPLETE == evCode) {
                        hGraphEvent = nullptr;
                    }
                }
            } else {
                hGraphEvent = nullptr;
                if (res == WAIT_TIMEOUT) {
                    // Likely a seek failure has occurred. For example due to an incomplete file.
                    REFERENCE_TIME rtCur = 0;
                    m_frame.Graph().m_pMS->GetCurrentPosition(&rtCur);
                    if (rtCur >= rtDur) {
                        abortloop = true;
                    }
                }
            }
        }

        if (abortloop) {
            break;
        }

        int col = (i - 1) % cols;
        int row = (i - 1) / cols;

        CPoint p(2 * margin + col * (szThumbnail.cx + 2 * margin), infoheight + 2 * margin + row * (szThumbnail.cy + 2 * margin));
        CRect r(p, szThumbnail);

        CRenderedTextSubtitle rts(&csSubLock);
        rts.m_SubRendererSettings.renderSSAUsingLibass = false;
        rts.m_SubRendererSettings.overrideDefaultStyle = false;
        rts.m_SubRendererSettings.overrideAllStyles = false;
        rts.CreateDefaultStyle(0);
        rts.m_storageRes = rts.m_playRes = CSize(width, height);
        STSStyle* style = DEBUG_NEW STSStyle();
        style->fontName = L"Calibri";
        style->marginRect.SetRectEmpty();
        rts.AddStyle(_T("thumbs"), style);

        DVD_HMSF_TIMECODE hmsf = RT2HMS_r(rt);
        CStringW str;
        if (!darktheme) {
            str.Format(L"{\\an7\\1c&Hffffff&\\4a&Hb0&\\bord1\\shad4\\be1}{\\p1}m %d %d l %d %d %d %d %d %d{\\p}",
                r.left, r.top, r.right, r.top, r.right, r.bottom, r.left, r.bottom);
            rts.Add(str, true, MS2RT(0), MS2RT(1), _T("thumbs")); // Thumbnail background
        }
        str.Format(L"{\\an3\\1c&Hffffff&\\3c&H000000&\\alpha&H80&\\fs%d\\b1\\bord2\\shad0\\pos(%d,%d)}%02u:%02u:%02u",
                   fontsize, r.right - 5, r.bottom - 3, hmsf.bHours, hmsf.bMinutes, hmsf.bSeconds);
        rts.Add(str, true, MS2RT(1), MS2RT(2), _T("thumbs")); // Thumbnail time

        rts.Render(spd, 0, 25, bbox); // Draw the thumbnail background/time

        BYTE* pData = nullptr;
        long size = 0;
        if (!GetDIB(&pData, size)) {
            if (m_frame.Graph().m_pBA) {
                m_frame.Graph().m_pBA->put_Volume(m_frame.m_nVolumeBeforeFrameStepping);
            }
            return false;
        }

        BITMAPINFO* bi = (BITMAPINFO*)pData;

        if (bi->bmiHeader.biBitCount != 32) {
            CString strTemp;
            strTemp.Format(IDS_THUMBNAILS_INVALID_FORMAT, bi->bmiHeader.biBitCount);
            AfxMessageBox(strTemp);
            delete [] pData;
            if (m_frame.Graph().m_pBA) {
                m_frame.Graph().m_pBA->put_Volume(m_frame.m_nVolumeBeforeFrameStepping);
            }
            return false;
        }

        int sw = bi->bmiHeader.biWidth;
        int sh = abs(bi->bmiHeader.biHeight);
        int sp = sw * 4;
        const BYTE* src = pData + sizeof(bi->bmiHeader);

        stbir_resize(src, sw, sh, sp, thumb.get(), szThumbnail.cx, szThumbnail.cy, szThumbnail.cx * 4, STBIR_RGBA_PM, STBIR_TYPE_UINT8, STBIR_EDGE_CLAMP, STBIR_FILTER_DEFAULT);

        BYTE* dst = spd.bits + spd.pitch * r.top + r.left * 4;

        const BYTE* tsrc = thumb.get();
        int tsrcPitch = szThumbnail.cx * 4;
        if (bi->bmiHeader.biHeight >= 0) {
            tsrc += tsrcPitch * (szThumbnail.cy - 1);
            tsrcPitch = -tsrcPitch;
        }
        for (int y = 0; y < szThumbnail.cy; y++, dst += spd.pitch, tsrc += tsrcPitch) {
            memcpy(dst, tsrc, abs(tsrcPitch));
        }
        
        rts.Render(spd, 10000, 25, bbox); // Draw the thumbnail time

        delete [] pData;
    }

    // Draw the file information
    {
        CRenderedTextSubtitle rts(&csSubLock);
        rts.m_SubRendererSettings.renderSSAUsingLibass = false;
        rts.m_SubRendererSettings.overrideDefaultStyle = false;
        rts.m_SubRendererSettings.overrideAllStyles = false;
        rts.CreateDefaultStyle(0);
        rts.m_storageRes = rts.m_playRes = CSize(width, height);
        STSStyle* style = DEBUG_NEW STSStyle();
        // Use System UI font.
        CFont tempFont;
        CMPCThemeUtil::getFontByType(tempFont, nullptr, CMPCThemeUtil::MessageFont);
        LOGFONT lf;
        if (tempFont.GetLogFont(&lf)) {
            CString fontName(lf.lfFaceName);
            style->fontName = fontName;
        }
        style->marginRect.SetRect(margin * 2, margin * 2, margin * 2, height - infoheight - margin);
        rts.AddStyle(_T("thumbs"), style);

        CStringW str;
        str.Format(L"{\\an9\\fs%d\\b1\\bord0\\shad0\\1c&Hffffff&}%s", infoheight - 2 * margin, L"MPC-HC");
        if (darktheme) {
            str.Replace(L"\\1c&Hffffff", L"\\1c&Hc8c8c8");
        }
        rts.Add(str, true, 0, 1, _T("thumbs"), _T(""), _T(""), CRect(0, 0, 0, 0), -1);

        DVD_HMSF_TIMECODE hmsf = RT2HMS_r(rtDur);

        CString title;
        CPlaylistItem pli = CPlaylistItem(true);
        if (m_frame.m_wndPlaylistBar.GetCur(pli, true) && pli.m_bYoutubeDL && pli.m_label && !pli.m_label.IsEmpty()) {
            title = pli.m_label;
        } else {
            title = m_frame.GetFileName();
        }

        CStringW fs;
        CString curfile = m_frame.m_wndPlaylistBar.GetCurFileName();
        if (!PathUtils::IsURL(curfile)) {
            ExtendMaxPathLengthIfNeeded(curfile, true);
            WIN32_FIND_DATA wfd;
            HANDLE hFind = FindFirstFile(curfile, &wfd);
            if (hFind != INVALID_HANDLE_VALUE) {
                FindClose(hFind);

                __int64 size = (__int64(wfd.nFileSizeHigh) << 32) | wfd.nFileSizeLow;
                const int MAX_FILE_SIZE_BUFFER = 65;
                WCHAR szFileSize[MAX_FILE_SIZE_BUFFER];
                StrFormatByteSizeW(size, szFileSize, MAX_FILE_SIZE_BUFFER);
                CString szByteSize;
                szByteSize.Format(_T("%I64d"), size);
                fs.Format(IDS_THUMBNAILS_INFO_FILESIZE, szFileSize, FormatNumber(szByteSize).GetString());
            }
        }        

        CStringW ar;
        if (szAR.cx > 0 && szAR.cy > 0 && szAR.cx != szVideo.cx && szAR.cy != szVideo.cy) {
            ar.Format(L"(%ld:%ld)", szAR.cx, szAR.cy);
        }
        CStringW fmt = ResStr(IDS_THUMBNAILS_INFO_HEADER);
        if (darktheme) {
            fmt.Replace(L"\\1c&H000000", L"\\1c&Hc8c8c8");
        }
        str.Format(fmt, fontsize,
                   title.GetString(), fs.GetString(), szVideo.cx, szVideo.cy, ar.GetString(), hmsf.bHours, hmsf.bMinutes, hmsf.bSeconds);
        rts.Add(str, true, 0, 1, _T("thumbs"));

        rts.Render(spd, 0, 25, bbox);
    }

    SaveDIB(fn, (BYTE*)dib, dibsize);

    if (m_frame.Graph().m_pBA) {
        m_frame.Graph().m_pBA->put_Volume(m_frame.m_nVolumeBeforeFrameStepping);
    }

    if (bWasStopped) {
        m_frame.OnPlayStop();
    } else {
        m_frame.DoSeekTo(rtPos, false);
    }

    m_frame.m_OSD.DisplayMessage(OSD_TOPLEFT, ResStr(IDS_OSD_THUMBS_SAVED), 3000);

    return true;
}

CString CSnapshot::MakeSnapshotFileName(BOOL thumbnails)
{
    CAppSettings& s = AfxGetAppSettings();
    CString prefix;
    CString fn;

    ASSERT(!thumbnails || m_frame.GetPlaybackMode() == PM_FILE);

    auto videoFn = m_frame.GetFileName();
    auto fullName = m_frame.m_wndPlaylistBar.GetCurFileName(true);
    bool needsExtensionRemoval = !s.bSnapShotKeepVideoExtension;
    if (m_frame.IsPlaylistFile(videoFn)) {
        CPlaylistItem pli = CPlaylistItem(true);
        if (m_frame.m_wndPlaylistBar.GetCur(pli, true)) {
            videoFn = pli.m_label;
            needsExtensionRemoval = false;
        }
    } else if (needsExtensionRemoval && PathUtils::IsURL(fullName)){
        auto title = m_frame.getBestTitle();
        if (!title.IsEmpty()) {
            videoFn = title;
            needsExtensionRemoval = false;
        }
    }


    if (needsExtensionRemoval) {
        int nPos = videoFn.ReverseFind('.');
        if (nPos != -1) {
            videoFn = videoFn.Left(nPos);
        }
    }

    bool saveImagePosition, saveImageCurrentTime;

    if (m_frame.m_wndSeekBar.HasDuration()) {
        saveImagePosition = s.bSaveImagePosition;
        saveImageCurrentTime = s.bSaveImageCurrentTime;
    } else {
        saveImagePosition = false;
        saveImageCurrentTime = true;
    }

    if (m_frame.GetPlaybackMode() == PM_FILE) {
        if (thumbnails) {
            prefix.Format(_T("%s_thumbs"), videoFn.GetString());
        } else {
            if (saveImagePosition) {
                prefix.Format(_T("%s_snapshot_%s"), videoFn.GetString(), GetVidPos().GetString());
            } else {
                prefix.Format(_T("%s_snapshot"), videoFn.GetString());
            }
        }
    } else if (m_frame.GetPlaybackMode() == PM_DVD) {
        if (saveImagePosition) {
            prefix.Format(_T("dvd_snapshot_%s"), GetVidPos().GetString());
        } else {
            prefix = _T("dvd_snapshot");
        }
    } else if (m_frame.GetPlaybackMode() == PM_DIGITAL_CAPTURE) {
        prefix.Format(_T("%s_snapshot"), m_frame.m_pDVBState->sChannelName.GetString());
    } else {
        prefix = _T("snapshot");
    }

    if (!thumbnails && saveImageCurrentTime) {
        CTime t = CTime::GetCurrentTime();
        fn.Format(_T("%s_[%s]%s"), PathUtils::FilterInvalidCharsFromFileName(prefix).GetString(), t.Format(_T("%Y.%m.%d_%H.%M.%S")).GetString(), s.strSnapshotExt.GetString());
    } else {
        fn.Format(_T("%s%s"), PathUtils::FilterInvalidCharsFromFileName(prefix).GetString(), s.strSnapshotExt.GetString());
    }
    return fn;
}

BOOL CSnapshot::IsRendererCompatibleWithSaveImage()
{
    BOOL result = TRUE;
    const CAppSettings& s = AfxGetAppSettings();

    if (m_frame.m_fShockwaveGraph) {
        AfxMessageBox(IDS_SCREENSHOT_ERROR_SHOCKWAVE, MB_ICONEXCLAMATION | MB_OK, 0);
        result = FALSE;
    } else if (s.iDSVideoRendererType == VIDRNDT_DS_OVERLAYMIXER) {
        AfxMessageBox(IDS_SCREENSHOT_ERROR_OVERLAY, MB_ICONEXCLAMATION | MB_OK, 0);
        result = FALSE;
    }

    return result;
}

CString CSnapshot::GetVidPos() const
{
    CString posstr = _T("");
    if ((m_frame.GetPlaybackMode() == PM_FILE) || (m_frame.GetPlaybackMode() == PM_DVD)) {
        __int64 start, stop, pos;
        m_frame.m_wndSeekBar.GetRange(start, stop);
        pos = m_frame.m_wndSeekBar.GetPos();

        DVD_HMSF_TIMECODE tcNow = RT2HMSF(pos);
        DVD_HMSF_TIMECODE tcDur = RT2HMSF(stop);

        if (tcDur.bHours > 0 || tcNow.bHours > 0) {
            posstr.Format(_T("%02u.%02u.%02u.%03u"), tcNow.bHours, tcNow.bMinutes, tcNow.bSeconds, (pos / 10000) % 1000);
        } else {
            posstr.Format(_T("%02u.%02u.%03u"), tcNow.bMinutes, tcNow.bSeconds, (pos / 10000) % 1000);
        }
    }

    return posstr;
}

void CSnapshot::OnFileSaveImage()
{
    CAppSettings& s = AfxGetAppSettings();

    /* Check if a compatible renderer is being used */
    if (!IsRendererCompatibleWithSaveImage()) {
        return;
    }

    CLongPath psrc;
    if (!s.strSnapshotPath.IsEmpty() && PathUtils::IsDir(s.strSnapshotPath)) {
        psrc.Combine(s.strSnapshotPath.GetString(), MakeSnapshotFileName(FALSE));
    } else {
        psrc = CLongPath(MakeSnapshotFileName(FALSE));        
    }

    bool subtitleOptionSupported = !m_frame.Graph().m_pMVRFG && s.fEnableSubtitles && s.IsISRAutoLoadEnabled();

    CSaveImageDialog fd(s.nJpegQuality, s.strSnapshotExt, (LPCTSTR)psrc,
                        _T("BMP - Windows Bitmap (*.bmp)|*.bmp|JPG - JPEG Image (*.jpg)|*.jpg|PNG - Portable Network Graphics (*.png)|*.png||"), m_frame.GetModalParent(), subtitleOptionSupported);

    if (s.strSnapshotExt == _T(".bmp")) {
        fd.m_pOFN->nFilterIndex = 1;
    } else if (s.strSnapshotExt == _T(".jpg")) {
        fd.m_pOFN->nFilterIndex = 2;
    } else if (s.strSnapshotExt == _T(".png")) {
        fd.m_pOFN->nFilterIndex = 3;
    }

    if (fd.DoModal() != IDOK) {
        return;
    }

    if (fd.m_pOFN->nFilterIndex == 1) {
        s.strSnapshotExt = _T(".bmp");
    } else if (fd.m_pOFN->nFilterIndex == 2) {
        s.strSnapshotExt = _T(".jpg");
        s.nJpegQuality = fd.m_nJpegQuality;
    } else {
        fd.m_pOFN->nFilterIndex = 3;
        s.strSnapshotExt = _T(".png");
    }

    CLongPath pdst(fd.GetPathName());
    CString ext(pdst.GetExtension().MakeLower());
    if (ext != s.strSnapshotExt) {
        if (ext == _T(".bmp") || ext == _T(".jpg") || ext == _T(".png")) {
            ext = s.strSnapshotExt;
        } else {
            ext += s.strSnapshotExt;
        }
        if (!pdst.RenameExtension(ext)) {
            ASSERT(false);
            return;
        }
    }
    CString path = (LPCTSTR)pdst;
    pdst.RemoveFileSpec();
    s.strSnapshotPath = (LPCTSTR)pdst;

    bool includeSubtitles = subtitleOptionSupported && s.bSnapShotSubtitles;

    SaveImage(path, false, includeSubtitles);
}

void CSnapshot::OnFileSaveImageAuto()
{
    const CAppSettings& s = AfxGetAppSettings();

    // If path doesn't exist, Save Image instead
    if (!PathUtils::IsDir(s.strSnapshotPath)) {
        OnFileSaveImage();
        return;
    }

    /* Check if a compatible renderer is being used */
    if (!IsRendererCompatibleWithSaveImage()) {
        return;
    }

    bool includeSubtitles = s.bSnapShotSubtitles && !m_frame.Graph().m_pMVRFG && s.fEnableSubtitles && s.IsISRAutoLoadEnabled();

    CString fn;
    fn.Format(_T("%s\\%s"), s.strSnapshotPath.GetString(), MakeSnapshotFileName(FALSE).GetString());
    SaveImage(fn.GetString(), false, includeSubtitles);
}

void CSnapshot::OnUpdateFileSaveImage(CCmdUI* pCmdUI)
{
    OAFilterState fs = m_frame.GetMediaState();
    pCmdUI->Enable(m_frame.GetLoadState() == MLS::LOADED && !m_frame.m_fAudioOnly && (fs == State_Paused || fs == State_Running));
}

void CSnapshot::OnCmdLineSaveThumbnails()
{
    CAppSettings& s = AfxGetAppSettings();

    /* Check if a compatible renderer is being used */
    if (!IsRendererCompatibleWithSaveImage()) {
        return;
    }

    CPlaylistItem pli = CPlaylistItem(true);
    if (!m_frame.m_wndPlaylistBar.GetCur(pli, true)) {
        AfxGetMyApp()->ReportCmdLineError(_T("no file to take thumbnails of"));
        return;
    }

    CLongPath psrc(m_frame.m_wndPlaylistBar.GetCurFileName(true));
    psrc.RemoveFileSpec();
    psrc.Combine(psrc, MakeSnapshotFileName(TRUE));

    s.iThumbRows = std::clamp(s.iThumbRows, 1, 40);
    s.iThumbCols = std::clamp(s.iThumbCols, 1, 16);
    s.iThumbWidth = std::clamp(s.iThumbWidth, 256, 3840);

    CString path = (LPCTSTR)psrc;
    if (path.IsEmpty() || psrc.IsRelative()) {
        // Combine hands back an unusable destination two ways, neither of them an
        // error it reports. The result is empty when it refuses a path it cannot
        // resolve safely; and when the file name arrived without a directory the
        // result is the bare file name, which SaveDIB would write into the program
        // folder rather than beside the video, with nothing to say it went astray.
        AfxGetMyApp()->ReportCmdLineError(_T("thumbnail output path could not be resolved"));
        return;
    }

    if (!SaveThumbnails(path)) {
        AfxGetMyApp()->m_nExitCode = 1;
    }
}

void CSnapshot::OnFileSaveThumbnails()
{
    CAppSettings& s = AfxGetAppSettings();

    /* Check if a compatible renderer is being used */
    if (!IsRendererCompatibleWithSaveImage()) {
        return;
    }

    CLongPath psrc(s.strSnapshotPath);
    psrc.Combine(s.strSnapshotPath, MakeSnapshotFileName(TRUE));

    CSaveThumbnailsDialog fd(s.nJpegQuality, s.iThumbRows, s.iThumbCols, s.iThumbWidth, s.strSnapshotExt, (LPCTSTR)psrc,
                             _T("BMP - Windows Bitmap (*.bmp)|*.bmp|JPG - JPEG Image (*.jpg)|*.jpg|PNG - Portable Network Graphics (*.png)|*.png||"), m_frame.GetModalParent());

    if (s.strSnapshotExt == _T(".bmp")) {
        fd.m_pOFN->nFilterIndex = 1;
    } else if (s.strSnapshotExt == _T(".jpg")) {
        fd.m_pOFN->nFilterIndex = 2;
    } else if (s.strSnapshotExt == _T(".png")) {
        fd.m_pOFN->nFilterIndex = 3;
    }

    if (fd.DoModal() != IDOK) {
        return;
    }

    if (fd.m_pOFN->nFilterIndex == 1) {
        s.strSnapshotExt = _T(".bmp");
    } else if (fd.m_pOFN->nFilterIndex == 2) {
        s.strSnapshotExt = _T(".jpg");
        s.nJpegQuality = fd.m_nJpegQuality;
    } else {
        fd.m_pOFN->nFilterIndex = 3;
        s.strSnapshotExt = _T(".png");
    }

    s.iThumbRows = std::clamp(fd.m_rows, 1, 40);
    s.iThumbCols = std::clamp(fd.m_cols, 1, 16);
    s.iThumbWidth = std::clamp(fd.m_width, 256, 3840);

    CLongPath pdst(fd.GetPathName());
    CString ext(pdst.GetExtension().MakeLower());
    if (ext != s.strSnapshotExt) {
        if (ext == _T(".bmp") || ext == _T(".jpg") || ext == _T(".png")) {
            ext = s.strSnapshotExt;
        } else {
            ext += s.strSnapshotExt;
        }
        if (!pdst.RenameExtension(ext)) {
            ASSERT(false);
            return;
        }
    }
    CString path = (LPCTSTR)pdst;
    pdst.RemoveFileSpec();
    s.strSnapshotPath = (LPCTSTR)pdst;

    SaveThumbnails(path);
}

void CSnapshot::OnUpdateFileSaveThumbnails(CCmdUI* pCmdUI)
{
    OAFilterState fs = m_frame.GetMediaState();
    UNREFERENCED_PARAMETER(fs);
    pCmdUI->Enable(m_frame.GetLoadState() == MLS::LOADED && !m_frame.m_fAudioOnly && (m_frame.GetPlaybackMode() == PM_FILE /*|| m_frame.GetPlaybackMode() == PM_DVD*/));
}
