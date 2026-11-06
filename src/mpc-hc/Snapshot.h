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

#pragma once

#include <vector>

class CMainFrame;

// Save image, auto save, save thumbnails and the frame grabbing they share.
// Composed into CMainFrame, which routes its commands here from OnCmdMsg.
class CSnapshot : public CCmdTarget
{
    CMainFrame& m_frame;

    HRESULT RenderCurrentSubtitles(BYTE* pData);
    CString MakeSnapshotFileName(BOOL thumbnails);
    BOOL IsRendererCompatibleWithSaveImage();
    CString GetVidPos() const;

public:
    explicit CSnapshot(CMainFrame& frame);

    HRESULT GetDisplayedImage(std::vector<BYTE>& dib, CString& errmsg);
    HRESULT GetCurrentFrame(std::vector<BYTE>& dib, CString& errmsg);
    HRESULT GetOriginalFrame(std::vector<BYTE>& dib, CString& errmsg);
    bool GetDIB(BYTE** ppData, long& size, bool fSilent = false);
    BYTE* ConvertDIBTo24bppRGB(BYTE* pData, long size, int& outWidth, int& outHeight, int& outPitch);
    void SaveDIB(LPCTSTR fn, BYTE* pData, long size);
#if MPC_SMTC_VIDEO_THUMBNAIL
    bool CaptureVideoThumbnail(std::vector<BYTE>& thumbnail);
#endif
    void SaveImage(LPCTSTR fn, bool displayed, bool includeSubtitles);
    // False when no sheet was written, including the failures that report
    // nothing at all. A headless /thumbnails run exits non-zero on it.
    bool SaveThumbnails(LPCTSTR fn);

    afx_msg void OnFileSaveImage();
    afx_msg void OnFileSaveImageAuto();
    afx_msg void OnUpdateFileSaveImage(CCmdUI* pCmdUI);
    afx_msg void OnCmdLineSaveThumbnails();
    afx_msg void OnFileSaveThumbnails();
    afx_msg void OnUpdateFileSaveThumbnails(CCmdUI* pCmdUI);

    DECLARE_MESSAGE_MAP()
};
