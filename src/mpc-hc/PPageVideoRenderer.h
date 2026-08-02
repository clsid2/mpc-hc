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

#include "CMPCThemePPageBase.h"
#include "CMPCThemeComboBox.h"
#include "resource.h"


// CPPageVideoRenderer dialog
// Popup settings page for the VMR-9 (renderless), EVR (CP) and Sync renderers,
// launched from the Settings button on the Output page.

class CPPageVideoRenderer : public CMPCThemePPageBase
{
    DECLARE_DYNAMIC(CPPageVideoRenderer)

public:
    CPPageVideoRenderer();
    virtual ~CPPageVideoRenderer();

    enum { IDD = IDD_PPAGEVIDEORENDERER };

private:
    CStringArray m_D3D9GUIDNames;

    CMPCThemeComboBox m_iD3D9RenderDeviceCtrl;
    CMPCThemeComboBox m_APSurfaceUsageCtrl;
    CMPCThemeComboBox m_DX9ResizerCtrl;
    CMPCThemeComboBox m_EVRBuffersCtrl;

    int m_iAPSurfaceUsage;
    int m_iDX9Resizer;
    BOOL m_fVMR9MixerMode;
    BOOL m_fD3DFullscreen;
    BOOL m_fVMR9AlterativeVSync;
    BOOL m_fResetDevice;
    BOOL m_fCacheShaders;
    CString m_iEvrBuffers;
    BOOL m_fD3D9RenderDevice;
    int m_iD3D9RenderDevice;

    BOOL m_bSynchronizeVideo;
    BOOL m_bSynchronizeDisplay;
    BOOL m_bSynchronizeNearest;
    int m_iLineDelta;
    int m_iColumnDelta;
    double m_fCycleDelta;
    double m_fTargetSyncOffset;
    double m_fControlLimit;

protected:
    virtual void DoDataExchange(CDataExchange* pDX);
    virtual BOOL OnInitDialog();
    virtual BOOL OnApply();

    DECLARE_MESSAGE_MAP()

public:
    afx_msg void OnSurfaceChange();
    afx_msg void OnD3D9DeviceCheck();
    afx_msg void OnFullscreenCheck();
    afx_msg void OnBnClickedSyncVideo();
    afx_msg void OnBnClickedSyncDisplay();
    afx_msg void OnBnClickedSyncNearest();
    afx_msg void OnUpdateSyncDisplay(CCmdUI* pCmdUI);
    afx_msg void OnUpdateSyncVideo(CCmdUI* pCmdUI);
};
