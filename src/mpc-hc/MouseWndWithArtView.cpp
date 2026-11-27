/*
 * (C) 2003-2006 Gabest
 * (C) 2006-2015 see Authors.txt
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
#include "mplayerc.h"
#include "MouseWndWithArtView.h"
#include "MainFrm.h"
#include "CMPCTheme.h"
#include "CMPCThemeUtil.h"
#include "ColorProfileUtil.h"

CMouseWndWithArtView::CMouseWndWithArtView(CMainFrame* pMainFrame, bool bD3DFS/* = false*/)
    : m_vrect(0, 0, 0, 0)
    , CMouseWnd(pMainFrame, bD3DFS)
    , m_pMainFrame(pMainFrame)
{
    LoadImg();
}

CMouseWndWithArtView::~CMouseWndWithArtView()
{
}

void CMouseWndWithArtView::LoadImg(const CString& imagePath)
{
    CMPCPngImage img;

    if (!imagePath.IsEmpty()) {
        img.LoadFromFile(imagePath);
    }

    LoadImgInternal(img.Detach());
}

void CMouseWndWithArtView::LoadImg(std::vector<BYTE> buffer)
{
    CMPCPngImage img;

    if (!buffer.empty()) {
        img.LoadFromBuffer(buffer.data(), (UINT)buffer.size());
    }

    LoadImgInternal(img.Detach());
}

void CMouseWndWithArtView::LoadImgInternal(HGDIOBJ hImg)
{
    CAppSettings& s = AfxGetAppSettings();
    bool bHaveLogo = false;

    m_img.DeleteObject();
    m_resizedImg.Destroy();
    m_bCustomImgLoaded = !!m_img.Attach(hImg);

    if (!m_bCustomImgLoaded && s.fLogoExternal) {
        bHaveLogo = !!m_img.LoadFromFile(s.strLogoFileName);
    }

    if (!bHaveLogo && !m_bCustomImgLoaded) {
        s.fLogoExternal = false;               // use the built-in logo instead
        s.strLogoFileName.Empty();             // clear logo file name
        int useLogoId = s.nLogoId;
        if (useLogoId == -1) { // if the user has never chosen a logo, we can try loading a theme default logo
            if (AppIsThemeLoaded()) {
                useLogoId = CMPCThemeUtil::defaultLogo();
            } else {
                useLogoId = DEF_LOGO;
            }
        }
        if (!m_img.Load(useLogoId)) { // try the latest selected build-in logo
            s.nLogoId = -1;           // upon failure, use default
            m_img.Load(DEF_LOGO);
        }
    }

    if (m_hWnd) {
        Invalidate();
    }
}

CSize CMouseWndWithArtView::GetLogoSize()
{
    return m_img.GetSize();
}

IMPLEMENT_DYNAMIC(CMouseWndWithArtView, CMouseWnd)

BEGIN_MESSAGE_MAP(CMouseWndWithArtView, CMouseWnd)
    ON_WM_ERASEBKGND()
END_MESSAGE_MAP()

BOOL CMouseWndWithArtView::OnEraseBkgnd(CDC* pDC)
{
    if (!pDC) {
        ASSERT(FALSE);
        return FALSE;
    }

    CRect r;

    if ((m_pMainFrame->GetLoadState() != MLS::CLOSED || (!m_bFirstMedia && m_pMainFrame->Controls().DelayShowNotLoaded())) &&
            !m_pMainFrame->HasDedicatedFSVideoWindow() && !m_pMainFrame->IsAudioOnly()) {
        pDC->ExcludeClipRect(m_vrect);
    } else {
        CImage img;
        img.Attach(m_img);
        if (!img.IsNull()) {
            const double dImageAR = double(img.GetWidth()) / img.GetHeight();

            GetClientRect(r);
            int width = r.Width();
            int height = r.Height();
            if (!m_bCustomImgLoaded) {
                // Limit logo size
                // TODO: Use vector logo to preserve quality and remove limit.
                width = std::min(img.GetWidth(), width);
                height = std::min(img.GetHeight(), height);
            }

            double dImgWidth = height * dImageAR;
            double dImgHeight;
            if (width < dImgWidth) {
                dImgWidth = width;
                dImgHeight = dImgWidth / dImageAR;
            } else {
                dImgHeight = height;
            }

            int x = std::lround((r.Width() - dImgWidth) / 2.0);
            int y = std::lround((r.Height() - dImgHeight) / 2.0);

            r = CRect(CPoint(x, y), CSize(std::lround(dImgWidth), std::lround(dImgHeight)));

            if (!r.IsRectEmpty()) {
                if (m_resizedImg.IsNull() || r.Width() != m_resizedImg.GetWidth() || r.Height() != m_resizedImg.GetHeight() || img.GetBPP() != m_resizedImg.GetBPP()) {
                    m_resizedImg.Destroy();
                    m_resizedImg.Create(r.Width(), r.Height(), std::max(img.GetBPP(), 24));

                    HDC hDC = m_resizedImg.GetDC();
                    SetStretchBltMode(hDC, STRETCH_HALFTONE);
                    img.StretchBlt(hDC, 0, 0, r.Width(), r.Height(), SRCCOPY);
                    m_resizedImg.ReleaseDC();
                    if (AfxGetAppSettings().fLogoColorProfileEnabled) {
                        ColorProfileUtil::applyColorProfile(m_hWnd, m_resizedImg);
                    }
                }

                m_resizedImg.BitBlt(*pDC, r.TopLeft());
                pDC->ExcludeClipRect(r);
            }
        }
        img.Detach();
    }

    GetClientRect(r);
    pDC->FillSolidRect(r, 0);

    return TRUE;
}

void CMouseWndWithArtView::SetVideoRect(const CRect& r)
{
    m_vrect = r;

    Invalidate();
}
