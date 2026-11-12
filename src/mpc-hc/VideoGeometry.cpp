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
#include "VideoGeometry.h"
#include "MainFrm.h"
#include "mplayerc.h"
#include "PnSPresetsDlg.h"

BEGIN_MESSAGE_MAP(CVideoGeometry, CCmdTarget)
    ON_COMMAND_RANGE(ID_VIEW_ZOOM_50, ID_VIEW_ZOOM_200, OnViewZoom)
    ON_UPDATE_COMMAND_UI_RANGE(ID_VIEW_ZOOM_50, ID_VIEW_ZOOM_200, OnUpdateViewZoom)
    ON_COMMAND_RANGE(ID_VIEW_ZOOM_25, ID_VIEW_ZOOM_25, OnViewZoom)
    ON_UPDATE_COMMAND_UI_RANGE(ID_VIEW_ZOOM_25, ID_VIEW_ZOOM_25, OnUpdateViewZoom)
    ON_COMMAND(ID_VIEW_ZOOM_AUTOFIT, OnViewZoomAutoFit)
    ON_UPDATE_COMMAND_UI(ID_VIEW_ZOOM_AUTOFIT, OnUpdateViewZoom)
    ON_COMMAND(ID_VIEW_ZOOM_AUTOFIT_LARGER, OnViewZoomAutoFitLarger)
    ON_UPDATE_COMMAND_UI(ID_VIEW_ZOOM_AUTOFIT_LARGER, OnUpdateViewZoom)
    ON_COMMAND_RANGE(ID_VIEW_ZOOM_SUB, ID_VIEW_ZOOM_ADD, OnViewModifySize)
    ON_COMMAND_RANGE(ID_VIEW_VF_HALF, ID_VIEW_VF_ZOOM2, OnViewDefaultVideoFrame)
    ON_UPDATE_COMMAND_UI_RANGE(ID_VIEW_VF_HALF, ID_VIEW_VF_ZOOM2, OnUpdateViewDefaultVideoFrame)
    ON_COMMAND(ID_VIEW_VF_SWITCHZOOM, OnViewSwitchVideoFrame)
    ON_COMMAND(ID_VIEW_VF_COMPMONDESKARDIFF, OnViewCompMonDeskARDiff)
    ON_UPDATE_COMMAND_UI(ID_VIEW_VF_COMPMONDESKARDIFF, OnUpdateViewCompMonDeskARDiff)
    ON_COMMAND_RANGE(ID_VIEW_RESET, ID_PANSCAN_CENTER, OnViewPanNScan)
    ON_UPDATE_COMMAND_UI_RANGE(ID_VIEW_RESET, ID_PANSCAN_CENTER, OnUpdateViewPanNScan)
    ON_COMMAND_RANGE(ID_PANNSCAN_PRESETS_START, ID_PANNSCAN_PRESETS_END, OnViewPanNScanPresets)
    ON_UPDATE_COMMAND_UI_RANGE(ID_PANNSCAN_PRESETS_START, ID_PANNSCAN_PRESETS_END, OnUpdateViewPanNScanPresets)
    ON_COMMAND_RANGE(ID_PANSCAN_ROTATEXP, ID_PANSCAN_ROTATEZM, OnViewRotate)
    ON_UPDATE_COMMAND_UI_RANGE(ID_PANSCAN_ROTATEXP, ID_PANSCAN_ROTATEZM, OnUpdateViewRotate)
    ON_COMMAND_RANGE(ID_PANSCAN_ROTATEZ270_OLD, ID_PANSCAN_ROTATEZ270_OLD, OnViewRotate)
    ON_COMMAND_RANGE(ID_PANSCAN_ROTATEZP2, ID_PANSCAN_ROTATEZP2, OnViewRotate)
    ON_COMMAND_RANGE(ID_ASPECTRATIO_START, ID_ASPECTRATIO_END, OnViewAspectRatio)
    ON_UPDATE_COMMAND_UI_RANGE(ID_ASPECTRATIO_START, ID_ASPECTRATIO_END, OnUpdateViewAspectRatio)
    ON_COMMAND(ID_ASPECTRATIO_NEXT, OnViewAspectRatioNext)
END_MESSAGE_MAP()

CVideoGeometry::CVideoGeometry(CMainFrame& frame)
    : m_frame(frame)
    , m_graph(frame.Graph())
    , m_ZoomX(1.0)
    , m_ZoomY(1.0)
    , m_PosX(0.5)
    , m_PosY(0.5)
    , m_AngleX(0)
    , m_AngleY(0)
    , m_AngleZ(0)
    , m_iDefRotation(0)
{
}

void CVideoGeometry::OnViewZoom(UINT nID)
{
    double scale = (nID == ID_VIEW_ZOOM_25) ? 0.25 : (nID == ID_VIEW_ZOOM_50) ? 0.5 : (nID == ID_VIEW_ZOOM_200) ? 2.0 : 1.0;

    m_frame.ZoomVideoWindow(scale);

    CString strODSMessage;
    strODSMessage.Format(IDS_OSD_ZOOM, scale * 100);
    m_frame.m_OSD.DisplayMessage(OSD_TOPLEFT, strODSMessage, 3000);
}

void CVideoGeometry::OnUpdateViewZoom(CCmdUI* pCmdUI)
{
    pCmdUI->Enable(m_frame.GetLoadState() == MLS::LOADED && !m_frame.m_fAudioOnly);
}

void CVideoGeometry::OnViewZoomAutoFit()
{
    m_frame.ZoomVideoWindow(CMainFrame::ZOOM_AUTOFIT);
    m_frame.m_OSD.DisplayMessage(OSD_TOPLEFT, ResStr(IDS_OSD_ZOOM_AUTO), 3000);
}

void CVideoGeometry::OnViewZoomAutoFitLarger()
{
    m_frame.ZoomVideoWindow(CMainFrame::ZOOM_AUTOFIT_LARGER);
    m_frame.m_OSD.DisplayMessage(OSD_TOPLEFT, ResStr(IDS_OSD_ZOOM_AUTO_LARGER), 3000);
}

void CVideoGeometry::OnViewModifySize(UINT nID) {
    if (m_frame.m_fFullScreen || !m_frame.m_pVideoWnd || m_frame.IsZoomed() || m_frame.IsIconic() || m_frame.GetLoadState() != MLS::LOADED) {
        return;
    }

    // For audio-only without custom cover art (logo or nothing), zoom is a no-op.
    if (m_frame.m_fAudioOnly && !m_frame.m_wndView.IsCustomImgLoaded()) {
        return;
    }

    enum resizeMethod {
        autoChoose
        , byHeight
        , byWidth
    } usedMethod;

    const CAppSettings& s = AfxGetAppSettings();

    MINMAXINFO mmi;
    CSize videoSize = m_frame.GetVideoOrArtSize(mmi);
    int minWidth = (int)mmi.ptMinTrackSize.x;

    int mult = (nID == ID_VIEW_ZOOM_ADD ? 1 : ID_VIEW_ZOOM_SUB ? -1 : 0);
    double videoRatio = double(videoSize.cy) / double(videoSize.cx);

    CRect rect;
    m_frame.GetWindowRect(&rect);

    CRect videoRect;
    videoRect = m_frame.m_pVideoWnd->GetVideoRect();
    if (videoRect.Width() == 0) {
        if (m_frame.m_fAudioOnly) {
            m_frame.m_wndView.GetWindowRect(&videoRect); // art display area in screen coords
        } else {
            videoRect = rect;
        }
    }
    double videoRectRatio = double(videoRect.Height()) / double(videoRect.Width());
    bool previouslyProportional = IsNearlyEqual(videoRectRatio, videoRatio, 0.01);
    // When shrinking a non-proportional window, treat the current window shape as the
    // content ratio so both dimensions shrink together rather than only bars reducing.
    if (mult < 0 && !previouslyProportional) {
        videoRatio = videoRectRatio;
        previouslyProportional = true;
    }

    CRect workRect, maxRect;
    m_frame.GetWorkAreaRect(workRect);
    maxRect = m_frame.GetZoomWindowRect(CSize(INT_MAX, INT_MAX), true);

    CSize targetSize;
    CRect zoomRect;

    auto calculateZoomWindowRect = [&](resizeMethod useMethod = autoChoose, CSize forceDimension = {0,0}) {
        int newWidth = videoRect.Width();
        int newHeight = videoRect.Height();

        if (useMethod == autoChoose) {
            bool bPickByHeight;
            if (previouslyProportional) {
                // No bars (or shrinking, handled above): step by the largest dimension.
                bPickByHeight = videoRect.Height() > videoRect.Width();
            } else {
                // Bars present, growing: step toward filling them.
                // Pillarboxed (view wider than content AR) → byHeight fills side bars
                // Letterboxed (view taller than content AR) → byWidth fills top/bottom bars
                bPickByHeight = videoRectRatio + 0.01 < videoRatio;
            }
            if (bPickByHeight) {
                useMethod = byHeight;
                newHeight = videoRect.Height() + m_frame.m_dpi.ScaleY(16) * mult;
            } else {
                useMethod = byWidth;
                newWidth = std::max(videoRect.Width() + m_frame.m_dpi.ScaleX(16) * mult, minWidth);
            }
        } else if (useMethod == byHeight) {
            newHeight = forceDimension.cy + videoRect.Height() - rect.Height();
        } else {
            newWidth = forceDimension.cx + videoRect.Width() - rect.Width();
        }

        if (useMethod == byHeight) {
            double newRatio = double(newHeight) / double(videoRect.Width());
            if (s.fLimitWindowProportions || previouslyProportional || SGN(newRatio - videoRatio) != SGN(videoRectRatio - videoRatio)) {
                newWidth = std::max(int(ceil(newHeight / videoRatio)), minWidth);
                if (mult == 1) {
                    newWidth = std::max(newWidth, videoRect.Width());
                }
            }
        } else {
            double newRatio = double(videoRect.Height()) / double(newWidth);
            if (s.fLimitWindowProportions || previouslyProportional || SGN(newRatio - videoRatio) != SGN(videoRectRatio - videoRatio)) {
                newHeight = int(ceil(newWidth * videoRatio));
                if (mult == 1) {
                    newHeight = std::max(newHeight, videoRect.Height());
                }
            }
        }
        targetSize = rect.Size() + CSize(newWidth - videoRect.Width(), newHeight - videoRect.Height());
        usedMethod = useMethod;
        return m_frame.GetZoomWindowRect(targetSize, true);
    };

    zoomRect = calculateZoomWindowRect();

    CRect newRect, work;
    newRect = zoomRect; //this will be our default (always constrained to work area)

    //if old rect was constrained to a single monitor, we zoom incrementally
    if (m_frame.GetWorkAreaRect(work) && work.PtInRect(rect.TopLeft()) && work.PtInRect(rect.BottomRight()-CSize(1,1))
        && ((zoomRect.Height() != rect.Height() && usedMethod == byHeight) || (zoomRect.Width() != rect.Width() && usedMethod == byWidth))) {

        if (zoomRect.Width() != targetSize.cx && zoomRect.Width() == maxRect.Width()) { //we appear to have been constrained by Screen Width
            if (maxRect.Width() != rect.Width() && maxRect.Width() - rect.Width() <= m_frame.m_dpi.ScaleX(32)) { //snap to fill only if we were already close to the edge
                newRect = calculateZoomWindowRect(byWidth, maxRect.Size());
            }
        } else if (zoomRect.Height() != targetSize.cy && zoomRect.Height() == maxRect.Height()) { //we appear to have been constrained by Screen Height
            if (maxRect.Height() != rect.Height() && maxRect.Height() - rect.Height() <= m_frame.m_dpi.ScaleY(32)) { //snap to fill only if we were already close to the edge
                newRect = calculateZoomWindowRect(byHeight, maxRect.Size());
            }
        } else {
            newRect = zoomRect;
        }
    }

    // Step 5: if the result fills the entire work area, switch to maximized state
    // instead of a floating window that merely covers the same pixels.
    if (newRect == maxRect) {
        m_frame.ShowWindow(SW_MAXIMIZE);
        return;
    }

    m_frame.MoveWindow(newRect);
}

void CVideoGeometry::OnViewDefaultVideoFrame(UINT nID)
{
    AfxGetAppSettings().iDefaultVideoSize = nID - ID_VIEW_VF_HALF;
    m_ZoomX = m_ZoomY = 1;
    m_PosX = m_PosY = 0.5;
    m_frame.MoveVideoWindow();
}

void CVideoGeometry::OnUpdateViewDefaultVideoFrame(CCmdUI* pCmdUI)
{
    const CAppSettings& s = AfxGetAppSettings();

    pCmdUI->Enable(m_frame.GetLoadState() == MLS::LOADED && !m_frame.m_fAudioOnly && s.iDSVideoRendererType != VIDRNDT_DS_EVR);

    int dvs = pCmdUI->m_nID - ID_VIEW_VF_HALF;
    if (s.iDefaultVideoSize == dvs && pCmdUI->m_pMenu) {
        pCmdUI->m_pMenu->CheckMenuRadioItem(ID_VIEW_VF_HALF, ID_VIEW_VF_ZOOM2, pCmdUI->m_nID, MF_BYCOMMAND);
    }
}

void CVideoGeometry::OnViewSwitchVideoFrame()
{
    CAppSettings& s = AfxGetAppSettings();

    int vs = s.iDefaultVideoSize;
    if (vs <= DVS_DOUBLE || vs == DVS_FROMOUTSIDE) {
        vs = DVS_STRETCH;
    } else if (vs == DVS_FROMINSIDE) {
        vs = DVS_ZOOM1;
    } else if (vs == DVS_ZOOM2) {
        vs = DVS_FROMOUTSIDE;
    } else {
        vs++;
    }
    switch (vs) { // TODO: Read messages from resource file
        case DVS_STRETCH:
            m_frame.m_OSD.DisplayMessage(OSD_TOPLEFT, ResStr(IDS_STRETCH_TO_WINDOW));
            break;
        case DVS_FROMINSIDE:
            m_frame.m_OSD.DisplayMessage(OSD_TOPLEFT, ResStr(IDS_TOUCH_WINDOW_FROM_INSIDE));
            break;
        case DVS_ZOOM1:
            m_frame.m_OSD.DisplayMessage(OSD_TOPLEFT, ResStr(IDS_ZOOM1));
            break;
        case DVS_ZOOM2:
            m_frame.m_OSD.DisplayMessage(OSD_TOPLEFT, ResStr(IDS_ZOOM2));
            break;
        case DVS_FROMOUTSIDE:
            m_frame.m_OSD.DisplayMessage(OSD_TOPLEFT, ResStr(IDS_TOUCH_WINDOW_FROM_OUTSIDE));
            break;
    }
    s.iDefaultVideoSize = vs;
    m_ZoomX = m_ZoomY = 1;
    m_PosX = m_PosY = 0.5;
    m_frame.MoveVideoWindow();
}

void CVideoGeometry::OnViewCompMonDeskARDiff()
{
    CAppSettings& s = AfxGetAppSettings();

    s.fCompMonDeskARDiff = !s.fCompMonDeskARDiff;
    m_frame.OnVideoSizeChanged();
}

void CVideoGeometry::OnUpdateViewCompMonDeskARDiff(CCmdUI* pCmdUI)
{
    const CAppSettings& s = AfxGetAppSettings();

    pCmdUI->Enable(m_frame.GetLoadState() == MLS::LOADED
                   && !m_frame.m_fAudioOnly
                   && s.iDSVideoRendererType != VIDRNDT_DS_EVR
                   // This doesn't work with madVR due to the fact that it positions video itself.
                   // And it has exactly the same option built in.
                   && s.iDSVideoRendererType != VIDRNDT_DS_MADVR);
    pCmdUI->SetCheck(s.fCompMonDeskARDiff);
}

void CVideoGeometry::OnViewPanNScan(UINT nID)
{
    if (m_frame.GetLoadState() != MLS::LOADED) {
        return;
    }

    int x = 0, y = 0;
    int dx = 0, dy = 0;

    switch (nID) {
        case ID_VIEW_RESET:
            // Subtitle overrides
            m_frame.m_subs.ResetSubtitlePosAndSize(true);
            // Pan&Scan
            m_ZoomX = m_ZoomY = 1.0;
            m_PosX = m_PosY = 0.5;
            m_AngleX = m_AngleY = m_AngleZ = 0;
            PerformFlipRotate();
            break;
        case ID_VIEW_INCSIZE:
            x = y = 1;
            break;
        case ID_VIEW_DECSIZE:
            x = y = -1;
            break;
        case ID_VIEW_INCWIDTH:
            x = 1;
            break;
        case ID_VIEW_DECWIDTH:
            x = -1;
            break;
        case ID_VIEW_INCHEIGHT:
            y = 1;
            break;
        case ID_VIEW_DECHEIGHT:
            y = -1;
            break;
        case ID_PANSCAN_CENTER:
            m_PosX = m_PosY = 0.5;
            break;
        case ID_PANSCAN_MOVELEFT:
            dx = -1;
            break;
        case ID_PANSCAN_MOVERIGHT:
            dx = 1;
            break;
        case ID_PANSCAN_MOVEUP:
            dy = -1;
            break;
        case ID_PANSCAN_MOVEDOWN:
            dy = 1;
            break;
        case ID_PANSCAN_MOVEUPLEFT:
            dx = dy = -1;
            break;
        case ID_PANSCAN_MOVEUPRIGHT:
            dx = 1;
            dy = -1;
            break;
        case ID_PANSCAN_MOVEDOWNLEFT:
            dx = -1;
            dy = 1;
            break;
        case ID_PANSCAN_MOVEDOWNRIGHT:
            dx = dy = 1;
            break;
        default:
            break;
    }

    if (x > 0 && m_ZoomX < 5.0) {
        m_ZoomX = std::min(m_ZoomX * 1.02, 5.0);
    } else if (x < 0 && m_ZoomX > 0.2) {
        m_ZoomX = std::max(m_ZoomX / 1.02, 0.2);
    }

    if (y > 0 && m_ZoomY < 5.0) {
        m_ZoomY = std::min(m_ZoomY * 1.02, 5.0);
    } else if (y < 0 && m_ZoomY > 0.2) {
        m_ZoomY = std::max(m_ZoomY / 1.02, 0.2);
    }

    if (dx < 0 && m_PosX > -0.5) {
        m_PosX = std::max(m_PosX - 0.005 * m_ZoomX, -0.5);
    } else if (dx > 0 && m_PosX < 1.5) {
        m_PosX = std::min(m_PosX + 0.005 * m_ZoomX, 1.5);
    }

    if (dy < 0 && m_PosY > -0.5) {
        m_PosY = std::max(m_PosY - 0.005 * m_ZoomY, -0.5);
    } else if (dy > 0 && m_PosY < 1.5) {
        m_PosY = std::min(m_PosY + 0.005 * m_ZoomY, 1.5);
    }

    m_frame.MoveVideoWindow(true);
}

void CVideoGeometry::OnUpdateViewPanNScan(CCmdUI* pCmdUI)
{
    pCmdUI->Enable(m_frame.GetLoadState() == MLS::LOADED && !m_frame.m_fAudioOnly && AfxGetAppSettings().iDSVideoRendererType != VIDRNDT_DS_EVR);
}

void CVideoGeometry::ApplyPanNScanPresetString()
{
    auto& s = AfxGetAppSettings();

    if (s.strPnSPreset.IsEmpty())
        return;

    if (s.strPnSPreset.Find(',') != -1) { // try to set raw values
        if (_stscanf_s(s.strPnSPreset, _T("%lf,%lf,%lf,%lf"), &m_PosX, &m_PosY, &m_ZoomX, &m_ZoomY) == 4) {
            ValidatePanNScanParameters();
            m_frame.MoveVideoWindow();
        }
    } else { // try to set named preset
        for (int i = 0; i < s.m_pnspresets.GetCount(); i++) {
            int j = 0;
            CString str = s.m_pnspresets[i];
            CString label = str.Tokenize(_T(","), j);
            if (s.strPnSPreset == label) {
                OnViewPanNScanPresets(i + ID_PANNSCAN_PRESETS_START);
            }
        }
    }

    s.strPnSPreset.Empty();
}

void CVideoGeometry::OnViewPanNScanPresets(UINT nID)
{
    if (m_frame.GetLoadState() != MLS::LOADED) {
        return;
    }

    CAppSettings& s = AfxGetAppSettings();

    nID -= ID_PANNSCAN_PRESETS_START;

    if ((INT_PTR)nID == s.m_pnspresets.GetCount()) {
        CPnSPresetsDlg dlg;
        dlg.m_pnspresets.Copy(s.m_pnspresets);
        if (dlg.DoModal() == IDOK) {
            s.m_pnspresets.Copy(dlg.m_pnspresets);
            s.SaveSettings();
        }
        return;
    }

    m_PosX = 0.5;
    m_PosY = 0.5;
    m_ZoomX = 1.0;
    m_ZoomY = 1.0;

    CString str = s.m_pnspresets[nID];

    int i = 0, j = 0;
    for (CString token = str.Tokenize(_T(","), i); !token.IsEmpty(); token = str.Tokenize(_T(","), i), j++) {
        float f = 0;
        if (_stscanf_s(token, _T("%f"), &f) != 1) {
            continue;
        }

        switch (j) {
            case 0:
                break;
            case 1:
                m_PosX = f;
                break;
            case 2:
                m_PosY = f;
                break;
            case 3:
                m_ZoomX = f;
                break;
            case 4:
                m_ZoomY = f;
                break;
            default:
                break;
        }
    }

    if (j != 5) {
        return;
    }

    ValidatePanNScanParameters();

    m_frame.MoveVideoWindow(true);
}

void CVideoGeometry::ValidatePanNScanParameters()
{
    m_PosX = std::min(std::max(m_PosX, -0.5), 1.5);
    m_PosY = std::min(std::max(m_PosY, -0.5), 1.5);
    m_ZoomX = std::min(std::max(m_ZoomX, 0.2), 5.0);
    m_ZoomY = std::min(std::max(m_ZoomY, 0.2), 5.0);
}

void CVideoGeometry::OnUpdateViewPanNScanPresets(CCmdUI* pCmdUI)
{
    int nID = pCmdUI->m_nID - ID_PANNSCAN_PRESETS_START;
    const CAppSettings& s = AfxGetAppSettings();
    pCmdUI->Enable(m_frame.GetLoadState() == MLS::LOADED && !m_frame.m_fAudioOnly && nID >= 0 && nID <= s.m_pnspresets.GetCount() && s.iDSVideoRendererType != VIDRNDT_DS_EVR);
}

int nearest90(int angle) {
    return int(float(angle) / 90 + 0.5) * 90;
}

bool CVideoGeometry::PerformFlipRotate()
{
    HRESULT hr = E_NOTIMPL;
    // Note: m_AngleZ is counterclockwise, so value 270 means rotated 90 degrees clockwise
    if (m_graph.m_pCAP3) {
        int rotation = (360 - m_AngleZ + m_iDefRotation) % 360;
        if (m_graph.m_pMVRS) { // MadVR
            bool isFlip = m_AngleX == 180;
            // MadVR does not support mirror, instead of flip we rotate 180 degrees
            hr = m_graph.m_pCAP3->SetRotation(isFlip ? (rotation + 180) % 360 : rotation);
        } else { // MPCVR
            bool isFlip, isMirror;
            if (m_iDefRotation == 90 || m_iDefRotation == 270) {
                isFlip   = m_AngleY == 180;
                isMirror = m_AngleX == 180;
            } else {
                isFlip   = m_AngleX == 180;
                isMirror = m_AngleY == 180;
            }
            // MPCVR: instead of flip, we mirror plus rotate 180 degrees
            hr = m_graph.m_pCAP3->SetRotation(isFlip ? (rotation + 180) % 360 : rotation);
            if (SUCCEEDED(hr)) {
                // SetFlip actually mirrors instead of doing vertical flip
                hr = m_graph.m_pCAP3->SetFlip(isFlip || isMirror);
            }
        }
    } else if (m_graph.m_pCAP) {
        // EVR-CP behavior for custom angles is ignored when choosing video size and zoom
        // We get better results if we treat the closest 90 as the standard rotation, and custom rotate the remainder (<45deg)
        int z = m_AngleZ;
        if (m_graph.m_pCAP2) {
            int nZ = nearest90(z);
            z = (z - nZ + 360) % 360;
            Vector defAngle = Vector(0, 0, Vector::DegToRad((nZ - m_iDefRotation + 360) % 360));
            m_graph.m_pCAP2->SetDefaultVideoAngle(defAngle);
        }
        
        hr = m_graph.m_pCAP->SetVideoAngle(Vector(Vector::DegToRad(m_AngleX), Vector::DegToRad(m_AngleY), Vector::DegToRad(z)));
    }

    if (FAILED(hr)) {
        m_AngleX = m_AngleY = m_AngleZ = 0;
        return false;
    }
    if (m_frame.m_previewGraph.m_pCAP2) { //we support rotating preview
        m_frame.PreviewWindowHide();
        m_frame.m_wndPreView.SetWindowSize();
        m_frame.SetPreviewVideoPosition();
        //adipose: using defaultvideoangle instead of videoangle, as some oddity with AR shows up when using normal rotate with EVRCP.
        //Since we only need to support 4 angles, this will work, but it *should* work with SetVideoAngle...

        hr = m_frame.m_previewGraph.m_pCAP2->SetDefaultVideoAngle(Vector(Vector::DegToRad(nearest90(m_AngleX)), Vector::DegToRad(nearest90(m_AngleY)), Vector::DegToRad(m_frame.defaultVideoAngle + nearest90(m_AngleZ))));
    }

    return true;
}

void CVideoGeometry::OnViewRotate(UINT nID)
{
    switch (nID) {
    case ID_PANSCAN_ROTATEXP:
        if (!m_graph.m_pCAP3) {
            m_AngleX += 2;
            break;
        }
        [[fallthrough]]; // fall through for m_graph.m_pCAP3
    case ID_PANSCAN_ROTATEXM:
        if (m_AngleX >= 180) {
            m_AngleX = 0;
        } else {
            m_AngleX = 180;
        }
        break;
    case ID_PANSCAN_ROTATEYP:
        if (!m_graph.m_pCAP3) {
            m_AngleY += 2;
            break;
        }
        [[fallthrough]];
    case ID_PANSCAN_ROTATEYM:
        if (m_AngleY >= 180) {
            m_AngleY = 0;
        } else {
            m_AngleY = 180;
        }
        break;
    case ID_PANSCAN_ROTATEZM:
        if (m_AngleZ == 0 || m_AngleZ > 270) {
            m_AngleZ = 270;
        } else if (m_AngleZ > 180) {
            m_AngleZ = 180;
        } else if (m_AngleZ > 90) {
            m_AngleZ = 90;
        } else if (m_AngleZ > 0) {
            m_AngleZ = 0;
        }
        break;
    case ID_PANSCAN_ROTATEZP2:
        if (!m_graph.m_pCAP3) {
            m_AngleZ += 2;
            break;
        }
        [[fallthrough]];
    case ID_PANSCAN_ROTATEZ270:
    case ID_PANSCAN_ROTATEZ270_OLD:
        if (m_AngleZ < 90) {
            m_AngleZ = 90;
        } else if (m_AngleZ >= 270) {
            m_AngleZ = 0;
        } else if (m_AngleZ >= 180) {
            m_AngleZ = 270;
        } else if (m_AngleZ >= 90) {
            m_AngleZ = 180;
        }
        break;
    default:
        return;
    }

    m_AngleX %= 360;
    m_AngleY %= 360;
    if (m_AngleX == 180 && m_AngleY == 180) {
        m_AngleX = m_AngleY = 0;
        m_AngleZ += 180;
    }
    m_AngleZ %= 360;

    ASSERT(m_AngleX >= 0);
    ASSERT(m_AngleY >= 0);
    ASSERT(m_AngleZ >= 0);

    if (PerformFlipRotate()) {
        // FIXME: do proper resizing of the window after rotate
        if (!m_graph.m_pMVRC) {
            m_frame.MoveVideoWindow();
        }

        CString info;
        info.Format(_T("x: %d, y: %d, z: %d"), m_AngleX, m_AngleY, m_AngleZ);
        m_frame.SendStatusMessage(info, 3000);
    }
}

void CVideoGeometry::OnUpdateViewRotate(CCmdUI* pCmdUI)
{
    pCmdUI->Enable(m_frame.GetLoadState() == MLS::LOADED && !m_frame.m_fAudioOnly && (m_graph.m_pCAP || m_graph.m_pCAP3));
}

// FIXME
const static SIZE s_ar[] = {{0, 0}, {4, 3}, {5, 4}, {16, 9}, {235, 100}, {185, 100}};

void CVideoGeometry::OnViewAspectRatio(UINT nID)
{
    auto& s = AfxGetAppSettings();

    CString info;
    if (nID == ID_ASPECTRATIO_SAR) {
        s.fKeepAspectRatio = false;
        info.LoadString(IDS_ASPECT_RATIO_SAR);
    } else {
        s.fKeepAspectRatio = true;
        CSize ar = s_ar[nID - ID_ASPECTRATIO_START];
        s.SetAspectRatioOverride(ar);
        if (ar.cx && ar.cy) {
            info.Format(IDS_MAINFRM_68, ar.cx, ar.cy);
        } else {
            info.LoadString(IDS_MAINFRM_69);
        }
    }

    m_frame.SendStatusMessage(info, 3000);
    m_frame.m_OSD.DisplayMessage(OSD_TOPLEFT, info, 3000);

    m_frame.OnVideoSizeChanged();
}

void CVideoGeometry::OnUpdateViewAspectRatio(CCmdUI* pCmdUI)
{
    const CAppSettings& s = AfxGetAppSettings();

    bool bSelected;
    if (pCmdUI->m_nID == ID_ASPECTRATIO_SAR) {
        bSelected = s.fKeepAspectRatio == false;
    } else {
        bSelected = s.fKeepAspectRatio == true && s.GetAspectRatioOverride() == s_ar[pCmdUI->m_nID - ID_ASPECTRATIO_START];
    }
    if (bSelected && pCmdUI->m_pMenu) {
        pCmdUI->m_pMenu->CheckMenuRadioItem(ID_ASPECTRATIO_START, ID_ASPECTRATIO_END, pCmdUI->m_nID, MF_BYCOMMAND);
    }

    pCmdUI->Enable(m_frame.GetLoadState() == MLS::LOADED && !m_frame.m_fAudioOnly && s.iDSVideoRendererType != VIDRNDT_DS_EVR);
}

void CVideoGeometry::OnViewAspectRatioNext()
{
    static_assert(ID_ASPECTRATIO_SAR - ID_ASPECTRATIO_START == _countof(s_ar) && ID_ASPECTRATIO_SAR == ID_ASPECTRATIO_END,
                  "ID_ASPECTRATIO_SAR needs to be last item in the menu.");

    const auto& s = AfxGetAppSettings();
    UINT nID = ID_ASPECTRATIO_START;
    if (s.fKeepAspectRatio) {
        const CSize ar = s.GetAspectRatioOverride();
        for (int i = 0; i < _countof(s_ar); i++) {
            if (ar == s_ar[i]) {
                nID += (i + 1) % ((ID_ASPECTRATIO_END - ID_ASPECTRATIO_START) + 1);
                break;
            }
        }
    }

    OnViewAspectRatio(nID);
}
