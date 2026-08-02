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
#include "mplayerc.h"
#include "PPageVideoRenderer.h"
#include "CMPCThemeMsgBox.h"
#include <d3d9.h>


// CPPageVideoRenderer dialog

IMPLEMENT_DYNAMIC(CPPageVideoRenderer, CMPCThemePPageBase)
CPPageVideoRenderer::CPPageVideoRenderer()
    : CMPCThemePPageBase(CPPageVideoRenderer::IDD, IDS_PPAGE_VIDEORENDERER_TITLE)
    , m_iAPSurfaceUsage(0)
    , m_iDX9Resizer(0)
    , m_fVMR9MixerMode(FALSE)
    , m_fD3DFullscreen(FALSE)
    , m_fVMR9AlterativeVSync(FALSE)
    , m_fResetDevice(FALSE)
    , m_fCacheShaders(FALSE)
    , m_iEvrBuffers(_T("5"))
    , m_fD3D9RenderDevice(FALSE)
    , m_iD3D9RenderDevice(-1)
    , m_bSynchronizeVideo(FALSE)
    , m_bSynchronizeDisplay(FALSE)
    , m_bSynchronizeNearest(FALSE)
    , m_iLineDelta(0)
    , m_iColumnDelta(0)
    , m_fCycleDelta(0.0012)
    , m_fTargetSyncOffset(10.0)
    , m_fControlLimit(2.0)
{
    m_bPopupHosted = true;
}

CPPageVideoRenderer::~CPPageVideoRenderer()
{
}

void CPPageVideoRenderer::DoDataExchange(CDataExchange* pDX)
{
    __super::DoDataExchange(pDX);
    DDX_Control(pDX, IDC_D3D9DEVICE_COMBO, m_iD3D9RenderDeviceCtrl);
    DDX_Control(pDX, IDC_DX_SURFACE, m_APSurfaceUsageCtrl);
    DDX_Control(pDX, IDC_DX9RESIZER_COMBO, m_DX9ResizerCtrl);
    DDX_Control(pDX, IDC_EVR_BUFFERS, m_EVRBuffersCtrl);
    DDX_CBIndex(pDX, IDC_DX_SURFACE, m_iAPSurfaceUsage);
    DDX_CBIndex(pDX, IDC_DX9RESIZER_COMBO, m_iDX9Resizer);
    DDX_CBIndex(pDX, IDC_D3D9DEVICE_COMBO, m_iD3D9RenderDevice);
    DDX_Check(pDX, IDC_D3D9DEVICE, m_fD3D9RenderDevice);
    DDX_Check(pDX, IDC_RESETDEVICE, m_fResetDevice);
    DDX_Check(pDX, IDC_CACHESHADERS, m_fCacheShaders);
    DDX_Check(pDX, IDC_FULLSCREEN_MONITOR_CHECK, m_fD3DFullscreen);
    DDX_Check(pDX, IDC_DSVMR9ALTERNATIVEVSYNC, m_fVMR9AlterativeVSync);
    DDX_Check(pDX, IDC_DSVMR9LOADMIXER, m_fVMR9MixerMode);
    DDX_CBString(pDX, IDC_EVR_BUFFERS, m_iEvrBuffers);
    DDX_Check(pDX, IDC_SYNCVIDEO, m_bSynchronizeVideo);
    DDX_Check(pDX, IDC_SYNCDISPLAY, m_bSynchronizeDisplay);
    DDX_Check(pDX, IDC_SYNCNEAREST, m_bSynchronizeNearest);
    DDX_Text(pDX, IDC_CYCLEDELTA, m_fCycleDelta);
    DDX_Text(pDX, IDC_LINEDELTA, m_iLineDelta);
    DDX_Text(pDX, IDC_COLUMNDELTA, m_iColumnDelta);
    DDX_Text(pDX, IDC_TARGETSYNCOFFSET, m_fTargetSyncOffset);
    DDX_Text(pDX, IDC_CONTROLLIMIT, m_fControlLimit);
}

BEGIN_MESSAGE_MAP(CPPageVideoRenderer, CMPCThemePPageBase)
    ON_CBN_SELCHANGE(IDC_DX_SURFACE, OnSurfaceChange)
    ON_BN_CLICKED(IDC_D3D9DEVICE, OnD3D9DeviceCheck)
    ON_BN_CLICKED(IDC_FULLSCREEN_MONITOR_CHECK, OnFullscreenCheck)
    ON_BN_CLICKED(IDC_SYNCVIDEO, OnBnClickedSyncVideo)
    ON_BN_CLICKED(IDC_SYNCDISPLAY, OnBnClickedSyncDisplay)
    ON_BN_CLICKED(IDC_SYNCNEAREST, OnBnClickedSyncNearest)
    ON_UPDATE_COMMAND_UI(IDC_STATIC1, OnUpdateSyncVideo)
    ON_UPDATE_COMMAND_UI(IDC_CYCLEDELTA, OnUpdateSyncVideo)
    ON_UPDATE_COMMAND_UI(IDC_STATIC2, OnUpdateSyncDisplay)
    ON_UPDATE_COMMAND_UI(IDC_LINEDELTA, OnUpdateSyncDisplay)
    ON_UPDATE_COMMAND_UI(IDC_STATIC3, OnUpdateSyncDisplay)
    ON_UPDATE_COMMAND_UI(IDC_COLUMNDELTA, OnUpdateSyncDisplay)
    ON_UPDATE_COMMAND_UI(IDC_STATIC4, OnUpdateSyncDisplay)
END_MESSAGE_MAP()

// CPPageVideoRenderer message handlers

BOOL CPPageVideoRenderer::OnInitDialog()
{
    __super::OnInitDialog();

    const CAppSettings& s = AfxGetAppSettings();
    const CRenderersSettings& r = s.m_RenderersSettings;

    m_APSurfaceUsageCtrl.AddString(ResStr(IDS_PPAGE_OUTPUT_SURF_OFFSCREEN));
    m_APSurfaceUsageCtrl.AddString(ResStr(IDS_PPAGE_OUTPUT_SURF_2D));
    m_APSurfaceUsageCtrl.AddString(ResStr(IDS_PPAGE_OUTPUT_SURF_3D));
    CorrectComboListWidth(m_APSurfaceUsageCtrl);
    m_iAPSurfaceUsage = r.iAPSurfaceUsage;

    m_DX9ResizerCtrl.AddString(ResStr(IDS_PPAGE_OUTPUT_RESIZE_NN));
    m_DX9ResizerCtrl.AddString(ResStr(IDS_PPAGE_OUTPUT_RESIZER_BILIN));
    m_DX9ResizerCtrl.AddString(ResStr(IDS_PPAGE_OUTPUT_RESIZER_BIL_PS));
    m_DX9ResizerCtrl.AddString(ResStr(IDS_PPAGE_OUTPUT_RESIZER_BICUB1));
    m_DX9ResizerCtrl.AddString(ResStr(IDS_PPAGE_OUTPUT_RESIZER_BICUB2));
    m_DX9ResizerCtrl.AddString(ResStr(IDS_PPAGE_OUTPUT_RESIZER_BICUB3));
    m_iDX9Resizer = r.iDX9Resizer;

    m_fVMR9MixerMode = r.fVMR9MixerMode;
    m_fVMR9AlterativeVSync = r.m_AdvRendSets.bVMR9AlterativeVSync;
    m_fD3DFullscreen = s.fD3DFullscreen;
    m_fResetDevice = r.fResetDevice;
    m_fCacheShaders = r.m_AdvRendSets.bCacheShaders;

    int EVRBuffers[] = { 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 25, 30, 35, 40, 45, 50, 55, 60 };
    CString EVRBuffer;
    for (size_t i = 0; i < _countof(EVRBuffers); i++) {
        EVRBuffer.Format(_T("%d"), EVRBuffers[i]);
        m_EVRBuffersCtrl.AddString(EVRBuffer);
    }
    m_iEvrBuffers.Format(_T("%d"), r.iEvrBuffers);

    IDirect3D9* pD3D9 = Direct3DCreate9(D3D_SDK_VERSION);
    if (pD3D9) {
        TCHAR strGUID[50];
        CString cstrGUID;
        CString d3ddevice_str = _T("");

        D3DADAPTER_IDENTIFIER9 adapterIdentifier;

        for (UINT adp = 0; adp < pD3D9->GetAdapterCount(); ++adp) {
            if (SUCCEEDED(pD3D9->GetAdapterIdentifier(adp, 0, &adapterIdentifier))) {
                d3ddevice_str = adapterIdentifier.Description;
                d3ddevice_str += _T(" - ");
                d3ddevice_str += adapterIdentifier.DeviceName;
                cstrGUID = _T("");
                if (::StringFromGUID2(adapterIdentifier.DeviceIdentifier, strGUID, 50) > 0) {
                    cstrGUID = strGUID;
                }
                if (!cstrGUID.IsEmpty()) {
                    boolean m_find = false;
                    for (INT_PTR j = 0; !m_find && (j < m_D3D9GUIDNames.GetCount()); j++) {
                        if (m_D3D9GUIDNames.GetAt(j) == cstrGUID) {
                            m_find = true;
                        }
                    }
                    if (!m_find) {
                        m_iD3D9RenderDeviceCtrl.AddString(d3ddevice_str);
                        m_D3D9GUIDNames.Add(cstrGUID);
                        if (r.D3D9RenderDevice == cstrGUID) {
                            m_iD3D9RenderDevice = m_iD3D9RenderDeviceCtrl.GetCount() - 1;
                        }
                    }
                }
            }
        }
        pD3D9->Release();
    }
    CorrectComboListWidth(m_iD3D9RenderDeviceCtrl);

    m_fD3D9RenderDevice = (m_iD3D9RenderDevice != -1);

    const CRenderersSettings::CAdvRendererSettings& ars = r.m_AdvRendSets;
    m_bSynchronizeVideo = ars.bSynchronizeVideo;
    m_bSynchronizeDisplay = ars.bSynchronizeDisplay;
    m_bSynchronizeNearest = ars.bSynchronizeNearest;
    m_iLineDelta = ars.iLineDelta;
    m_iColumnDelta = ars.iColumnDelta;
    m_fCycleDelta = ars.fCycleDelta;
    m_fTargetSyncOffset = ars.fTargetSyncOffset;
    m_fControlLimit = ars.fControlLimit;

    UpdateData(FALSE);

    // The D3D9 render device selection is only meaningful with more than one adapter
    GetDlgItem(IDC_D3D9DEVICE)->EnableWindow(m_iD3D9RenderDeviceCtrl.GetCount() > 1);
    GetDlgItem(IDC_D3D9DEVICE_COMBO)->EnableWindow(m_iD3D9RenderDeviceCtrl.GetCount() > 1 && m_fD3D9RenderDevice);

    CreateToolTip();

    static const UINT surfaceTips[] = { IDC_REGULARSURF, IDC_TEXTURESURF2D, IDC_TEXTURESURF3D };
    if (m_iAPSurfaceUsage >= 0 && m_iAPSurfaceUsage < _countof(surfaceTips)) {
        m_wndToolTip.AddTool(GetDlgItem(IDC_DX_SURFACE), ResStr(surfaceTips[m_iAPSurfaceUsage]));
    }

    return TRUE;  // return TRUE unless you set the focus to a control
    // EXCEPTION: OCX Property Pages should return FALSE
}

BOOL CPPageVideoRenderer::OnApply()
{
    UpdateData();

    CAppSettings& s = AfxGetAppSettings();
    CRenderersSettings& r = s.m_RenderersSettings;

    s.fD3DFullscreen = m_fD3DFullscreen ? true : false;

    r.iAPSurfaceUsage = m_iAPSurfaceUsage;
    r.iDX9Resizer = m_iDX9Resizer;
    r.fVMR9MixerMode = !!m_fVMR9MixerMode;
    r.m_AdvRendSets.bVMR9AlterativeVSync = m_fVMR9AlterativeVSync != FALSE;
    r.fResetDevice = !!m_fResetDevice;
    r.m_AdvRendSets.bCacheShaders = !!m_fCacheShaders;
    if (m_iEvrBuffers.IsEmpty() || _stscanf_s(m_iEvrBuffers, _T("%d"), &r.iEvrBuffers) != 1) {
        r.iEvrBuffers = 5;
    }
    if (m_fD3D9RenderDevice && m_iD3D9RenderDevice != -1) {
        r.D3D9RenderDevice = m_D3D9GUIDNames[m_iD3D9RenderDevice];
    } else {
        r.D3D9RenderDevice.Empty();
    }

    CRenderersSettings::CAdvRendererSettings& ars = r.m_AdvRendSets;
    ars.bSynchronizeVideo = !!m_bSynchronizeVideo;
    ars.bSynchronizeDisplay = !!m_bSynchronizeDisplay;
    ars.bSynchronizeNearest = !!m_bSynchronizeNearest;
    ars.iLineDelta = m_iLineDelta;
    ars.iColumnDelta = m_iColumnDelta;
    ars.fCycleDelta = m_fCycleDelta;
    ars.fTargetSyncOffset = m_fTargetSyncOffset;
    ars.fControlLimit = m_fControlLimit;

    return __super::OnApply();
}

void CPPageVideoRenderer::OnSurfaceChange()
{
    UpdateData();

    switch (m_iAPSurfaceUsage) {
        case VIDRNDT_AP_SURFACE:
            m_wndToolTip.UpdateTipText(ResStr(IDC_REGULARSURF), GetDlgItem(IDC_DX_SURFACE));
            break;
        case VIDRNDT_AP_TEXTURE2D:
            m_wndToolTip.UpdateTipText(ResStr(IDC_TEXTURESURF2D), GetDlgItem(IDC_DX_SURFACE));
            break;
        case VIDRNDT_AP_TEXTURE3D:
            m_wndToolTip.UpdateTipText(ResStr(IDC_TEXTURESURF3D), GetDlgItem(IDC_DX_SURFACE));
            break;
    }

    SetModified();
}

void CPPageVideoRenderer::OnD3D9DeviceCheck()
{
    UpdateData();
    GetDlgItem(IDC_D3D9DEVICE_COMBO)->EnableWindow(m_fD3D9RenderDevice);
    SetModified();
}

void CPPageVideoRenderer::OnFullscreenCheck()
{
    UpdateData();
    if (m_fD3DFullscreen && CMPCThemeMsgBox::MessageBoxW(this, ResStr(IDS_D3DFS_WARNING), nullptr, MB_ICONQUESTION | MB_YESNO | MB_DEFBUTTON2) == IDNO) {
        m_fD3DFullscreen = false;
        UpdateData(FALSE);
    } else {
        SetModified();
    }
}

void CPPageVideoRenderer::OnBnClickedSyncVideo()
{
    m_bSynchronizeVideo = !m_bSynchronizeVideo;
    if (m_bSynchronizeVideo) {
        m_bSynchronizeDisplay = FALSE;
        m_bSynchronizeNearest = FALSE;
    }
    UpdateData(FALSE);
    SetModified();
}

void CPPageVideoRenderer::OnBnClickedSyncDisplay()
{
    m_bSynchronizeDisplay = !m_bSynchronizeDisplay;
    if (m_bSynchronizeDisplay) {
        m_bSynchronizeVideo = FALSE;
        m_bSynchronizeNearest = FALSE;
    }
    UpdateData(FALSE);
    SetModified();
}

void CPPageVideoRenderer::OnBnClickedSyncNearest()
{
    m_bSynchronizeNearest = !m_bSynchronizeNearest;
    if (m_bSynchronizeNearest) {
        m_bSynchronizeVideo = FALSE;
        m_bSynchronizeDisplay = FALSE;
    }
    UpdateData(FALSE);
    SetModified();
}

void CPPageVideoRenderer::OnUpdateSyncDisplay(CCmdUI* pCmdUI)
{
    pCmdUI->Enable(m_bSynchronizeDisplay);
}

void CPPageVideoRenderer::OnUpdateSyncVideo(CCmdUI* pCmdUI)
{
    pCmdUI->Enable(m_bSynchronizeVideo);
}
