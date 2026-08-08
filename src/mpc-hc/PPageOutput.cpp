/*
 * (C) 2003-2006 Gabest
 * (C) 2006-2017 see Authors.txt
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
#include "PPageOutput.h"
#include "moreuuids.h"
#include "Monitors.h"
#include <WinapiFunc.h>
#include "PPageAudioRenderer.h"
#include "PPageVideoRenderer.h"
#include "CMPCThemePropertySheet.h"
#include "FGFilter.h"
#include "MainFrm.h"
#include <mvrInterfaces.h>
#include "FGManager.h"
#include "CMPCThemeMsgBox.h"
#include "../VideoRenderers/MPCVRAllocatorPresenter.h"
#include "GPUInfo.h"
#include "FakeFilterMapper2.h"

// CPPageOutput dialog

IMPLEMENT_DYNAMIC(CPPageOutput, CMPCThemePPageBase)
CPPageOutput::CPPageOutput()
    : CMPCThemePPageBase(CPPageOutput::IDD, CPPageOutput::IDD)
    , m_tick(nullptr)
    , m_cross(nullptr)
    , m_warn(nullptr)
    , m_iDSVideoRendererType(VIDRNDT_DS_VMR7)
    , m_iAudioRendererType(0)
    , m_iMPCAudioRendererType(-1)
    , m_iSaneAudioRendererType(-1)
    , m_lastSubrenderer(CAppSettings::SubtitleRenderer::INTERNAL)
{
}

CPPageOutput::~CPPageOutput()
{
    for (HICON icon : { m_tick, m_cross, m_warn }) {
        if (icon) {
            DestroyIcon(icon);
        }
    }
}

void CPPageOutput::DoDataExchange(CDataExchange* pDX)
{
    __super::DoDataExchange(pDX);
    DDX_Control(pDX, IDC_VIDRND_COMBO, m_iDSVideoRendererTypeCtrl);
    DDX_Control(pDX, IDC_AUDRND_COMBO, m_iAudioRendererTypeCtrl);
    DDX_Control(pDX, IDC_COMBO1, m_SubtitleRendererCtrl);
    DDX_Control(pDX, IDC_VIDRND_SUPPORT_ICON, m_iDSSupportIcon);
    DDX_CBIndex(pDX, IDC_AUDRND_COMBO, m_iAudioRendererType);
}

BEGIN_MESSAGE_MAP(CPPageOutput, CMPCThemePPageBase)
    ON_UPDATE_COMMAND_UI(IDC_BUTTON1, OnUpdateVideoRendererSettings)
    ON_BN_CLICKED(IDC_BUTTON1, OpenVideoRendererSettings)
    ON_UPDATE_COMMAND_UI(IDC_BUTTON2, OnUpdateAudioRendererSettings)
    ON_BN_CLICKED(IDC_BUTTON2, OpenAudioRendererSettings)
    ON_CBN_SELCHANGE(IDC_VIDRND_COMBO, OnDSRendererChange)
    ON_CBN_SELCHANGE(IDC_AUDRND_COMBO, OnAudioRendererChange)
    ON_CBN_SELCHANGE(IDC_COMBO1, OnSubtitleRendererChange)
END_MESSAGE_MAP()

// CPPageOutput message handlers

BOOL CPPageOutput::OnInitDialog()
{
    __super::OnInitDialog();

    SetHandCursor(m_hWnd, IDC_AUDRND_COMBO);

    CAppSettings& s = AfxGetAppSettings();

    m_iDSVideoRendererType  = s.iDSVideoRendererType;
    m_lastSubrenderer = s.GetSubtitleRenderer();

    m_iAudioRendererTypeCtrl.SetRedraw(FALSE);

    // System default
    m_AudioRendererDisplayNames.Add(_T(""));
    m_iAudioRendererTypeCtrl.AddString(ResStr(IDS_PPAGE_OUTPUT_SYS_DEF));
    m_iAudioRendererType = 0;
    // SaneAR
    m_AudioRendererDisplayNames.Add(AUDRNDT_INTERNAL);
    m_iAudioRendererTypeCtrl.AddString(ResStr(IDS_PPAGE_OUTPUT_AUD_INTERNAL_REND).GetString());
    m_iSaneAudioRendererType = m_iAudioRendererTypeCtrl.GetCount() - 1;
    if (s.strAudioRendererDisplayName == AUDRNDT_INTERNAL && m_iAudioRendererType == 0) {
        m_iAudioRendererType = m_iSaneAudioRendererType;
    }
    // MPC AR
    m_AudioRendererDisplayNames.Add(AUDRNDT_MPC);
    m_iAudioRendererTypeCtrl.AddString(ResStr(IDS_PPAGE_OUTPUT_AUD_MPC_REND).GetString());
    m_iMPCAudioRendererType = m_iAudioRendererTypeCtrl.GetCount() - 1;
    if (s.strAudioRendererDisplayName == AUDRNDT_MPC && m_iAudioRendererType == 0) {
        m_iAudioRendererType = m_iMPCAudioRendererType;
    }

    // List of available renderers
    std::map<CStringW,CStringW> devicelist = GetAudioDeviceList();

    for (auto it = devicelist.cbegin(); it != devicelist.cend(); it++) {
        CString description = (*it).first;
        CString deviceid = (*it).second;
        m_AudioRendererDisplayNames.Add(deviceid);
        m_iAudioRendererTypeCtrl.AddString(description);
        if (s.strAudioRendererDisplayName == deviceid && m_iAudioRendererType == 0) {
            m_iAudioRendererType = m_iAudioRendererTypeCtrl.GetCount() - 1;
        }
    }

    // NULL renderers
    m_AudioRendererDisplayNames.Add(AUDRNDT_NULL_COMP);
    m_iAudioRendererTypeCtrl.AddString(ResStr(IDS_PPAGE_OUTPUT_AUD_NULL_COMP).GetString());
    if (s.strAudioRendererDisplayName == AUDRNDT_NULL_COMP && m_iAudioRendererType == 0) {
        m_iAudioRendererType = m_iAudioRendererTypeCtrl.GetCount() - 1;
    }
    m_AudioRendererDisplayNames.Add(AUDRNDT_NULL_UNCOMP);
    m_iAudioRendererTypeCtrl.AddString(ResStr(IDS_PPAGE_OUTPUT_AUD_NULL_UNCOMP).GetString());
    if (s.strAudioRendererDisplayName == AUDRNDT_NULL_UNCOMP && m_iAudioRendererType == 0) {
        m_iAudioRendererType = m_iAudioRendererTypeCtrl.GetCount() - 1;
    }

    // check if previously used renderer is not in the list of available ones, and reset to default
    if (m_iAudioRendererType == 0 && !s.strAudioRendererDisplayName.IsEmpty()) {
        s.strAudioRendererDisplayName = _T("");
    }

    CorrectComboListWidth(m_iAudioRendererTypeCtrl);
    m_iAudioRendererTypeCtrl.SetRedraw(TRUE);
    m_iAudioRendererTypeCtrl.Invalidate();
    m_iAudioRendererTypeCtrl.UpdateWindow();

    UpdateSubtitleRendererList();

    auto addRenderer = [&](int nID) {
        WORD resName;

        switch (nID) {
            case VIDRNDT_DS_VMR7:
                resName = IDS_PPAGE_OUTPUT_VMR7;
                break;
            case VIDRNDT_DS_OVERLAYMIXER:
                resName = IDS_PPAGE_OUTPUT_OVERLAYMIXER;
                break;
            case VIDRNDT_DS_VMR9WINDOWED:
                resName = IDS_PPAGE_OUTPUT_VMR9WINDOWED;
                break;
            case VIDRNDT_DS_VMR9RENDERLESS:
                resName = IDS_PPAGE_OUTPUT_VMR9RENDERLESS;
                break;
            case VIDRNDT_DS_DXR:
                resName = IDS_PPAGE_OUTPUT_DXR;
                break;
            case VIDRNDT_DS_NULL_COMP:
                resName = IDS_PPAGE_OUTPUT_NULL_COMP;
                break;
            case VIDRNDT_DS_NULL_UNCOMP:
                resName = IDS_PPAGE_OUTPUT_NULL_UNCOMP;
                break;
            case VIDRNDT_DS_EVR:
                resName = IDS_PPAGE_OUTPUT_EVR;
                break;
            case VIDRNDT_DS_EVR_CUSTOM:
                resName = IDS_PPAGE_OUTPUT_EVR_CUSTOM;
                break;
            case VIDRNDT_DS_MADVR:
                resName = IDS_PPAGE_OUTPUT_MADVR;
                break;
            case VIDRNDT_DS_SYNC:
                resName = IDS_PPAGE_OUTPUT_SYNC;
                break;
            case VIDRNDT_DS_MPCVR:
                resName = IDS_PPAGE_OUTPUT_MPCVR;
                break;
            default:
                ASSERT(FALSE);
                return;
        }

        CString sName(StrRes(resName));
        bool available = s.IsVideoRendererAvailable(nID);
        if (m_iDSVideoRendererType == nID || available) {
            if (!available) {
                sName.AppendFormat(_T("   %s"), ResStr(IDS_PPAGE_OUTPUT_UNAVAILABLE).GetString());
            }
            m_iDSVideoRendererTypeCtrl.SetItemData(m_iDSVideoRendererTypeCtrl.AddString(sName), nID);
        }
    };

    CComboBox& m_iDSVRTC = m_iDSVideoRendererTypeCtrl;
    m_iDSVRTC.SetRedraw(FALSE); // Do not draw the control while we are filling it with items
    addRenderer(VIDRNDT_DS_MPCVR);
    addRenderer(VIDRNDT_DS_MADVR);
    addRenderer(VIDRNDT_DS_EVR_CUSTOM);
    addRenderer(VIDRNDT_DS_EVR);
    addRenderer(VIDRNDT_DS_SYNC);
    addRenderer(VIDRNDT_DS_VMR9RENDERLESS);
    addRenderer(VIDRNDT_DS_VMR9WINDOWED);
    addRenderer(VIDRNDT_DS_VMR7);
    addRenderer(VIDRNDT_DS_DXR);
    addRenderer(VIDRNDT_DS_OVERLAYMIXER);
    addRenderer(VIDRNDT_DS_NULL_COMP);
    addRenderer(VIDRNDT_DS_NULL_UNCOMP);

    m_iDSVRTC.SetCurSel(0);
    for (int j = 1; j < m_iDSVRTC.GetCount(); ++j) {
        if ((UINT)m_iDSVideoRendererType == m_iDSVRTC.GetItemData(j)) {
            m_iDSVRTC.SetCurSel(j);
            break;
        }
    }

    m_iDSVRTC.SetRedraw(TRUE);
    m_iDSVRTC.Invalidate();
    m_iDSVRTC.UpdateWindow();

    UpdateData(FALSE);

    // All three indicators are rendered at the small-icon size for this
    // window's DPI, so they stay the same size as each other when scaling.
    DpiHelper dpiWindow;
    dpiWindow.Override(GetSafeHwnd());
    const int iconSize = dpiWindow.GetSystemMetricsDPI(SM_CXSMICON);

    auto svgIcon = [iconSize](UINT nIDIcon) -> HICON {
        CImage img;
        if (FAILED(SVGImage::LoadIconDef({ nIDIcon, iconSize }, img)) || img.IsNull()) {
            return nullptr;
        }
        CImageList imageList;
        imageList.Create(img.GetWidth(), img.GetHeight(), ILC_COLOR32, 1, 0);
        imageList.Add(CBitmap::FromHandle(img), (CBitmap*)nullptr);
        return imageList.ExtractIcon(0);
    };

    m_tick = svgIcon(IDF_SVG_TICK);
    m_cross = svgIcon(IDF_SVG_CROSS);
    LoadIconWithScaleDown(nullptr, (PCWSTR)IDI_WARNING, iconSize, iconSize, &m_warn);

    CRect brc;
    GetDlgItem(IDC_BUTTON1)->GetWindowRect(brc);
    SetMPCThemeButtonIcon(IDC_BUTTON1, { IDF_SVG_GEAR, int(0.7f * brc.Width()) });
    SetMPCThemeButtonIcon(IDC_BUTTON2, { IDF_SVG_GEAR, int(0.7f * brc.Width()) });

    CreateToolTip();

    m_wndToolTip.AddTool(GetDlgItem(IDC_VIDRND_COMBO), L"");
    m_wndToolTip.AddTool(GetDlgItem(IDC_VIDRND_SUPPORT_ICON), L"");
    m_wndToolTip.AddTool(GetDlgItem(IDC_VIDRND_SUPPORT_NOTE), L"");

    OnDSRendererChange();

    return TRUE;  // return TRUE unless you set the focus to a control
    // EXCEPTION: OCX Property Pages should return FALSE
}

BOOL CPPageOutput::OnApply()
{
    UpdateData();

    CAppSettings& s = AfxGetAppSettings();

    if (!s.IsVideoRendererAvailable(m_iDSVideoRendererType)) {
        ((CPropertySheet*)GetParent())->SetActivePage(this);
        AfxMessageBox(IDS_PPAGE_OUTPUT_UNAVAILABLEMSG, MB_ICONEXCLAMATION | MB_OK, 0);

        // revert to the renderer in the settings
        m_iDSVideoRendererTypeCtrl.SetCurSel(0);
        for (int i = 0; i < m_iDSVideoRendererTypeCtrl.GetCount(); ++i) {
            if ((UINT)s.iDSVideoRendererType == m_iDSVideoRendererTypeCtrl.GetItemData(i)) {
                m_iDSVideoRendererTypeCtrl.SetCurSel(i);
                break;
            }
        }
        OnDSRendererChange();

        return FALSE;
    }

    if (s.iDSVideoRendererType != m_iDSVideoRendererType) {
        // video renderer changed, optimize HW decoding settings
        CMPlayerCApp* pApp = AfxGetMyApp();
        int curhwa = pApp->GetProfileInt(IDS_R_INTERNAL_LAVVIDEO_HWACCEL, _T("HWAccel"), -1);
        bool needcopyback = false; // external filters that require copyback
        if (s.GetSubtitleRenderer() == CAppSettings::SubtitleRenderer::VS_FILTER) {
            needcopyback = true;
        }
        if (s.m_filters.GetCount() > 0) {
            POSITION pos = s.m_filters.GetHeadPosition();
            while (pos) {
               FilterOverride fo = s.m_filters.GetNext(pos);
               if (!fo.fDisabled && fo.dwMerit > MERIT_DO_NOT_USE) {
                   if (fo.clsid == CLSID_VSFilter || fo.clsid == CLSID_VSFilter2 || fo.clsid == CLSID_FFDShowRawVideo || fo.clsid ==  CLSID_AviSynthFilter || fo.clsid ==  CLSID_VapourSynthFilter) {
                       needcopyback = true;
                       break;
                   }
               }
            }
        }

        if (m_iDSVideoRendererType == VIDRNDT_DS_MPCVR) {
            GPUDetect gpuinfo = GPUDetect();
            if (gpuinfo.SupportD3D11VA()) {
                WriteRegistryDWORD(HKEY_CURRENT_USER, L"Software\\MPC-BE Filters\\MPC Video Renderer", L"UseD3D11", 1);
                if (curhwa > HWAccel_None) {
                    pApp->WriteProfileInt(IDS_R_INTERNAL_LAVVIDEO_HWACCEL, _T("HWAccel"), HWAccel_D3D11);
                    if (!needcopyback && (-1 != pApp->GetProfileInt(IDS_R_INTERNAL_LAVVIDEO_HWACCEL, _T("HWAccelDeviceD3D11"), -1))) {
                        if (AfxMessageBox(L"Change hardware decoding setting to \"D3D11 Native\"?\n\nThat gives better performance and is optimal choice for the selected video renderer.", MB_YESNO, 0) == IDYES) {
                            pApp->WriteProfileInt(IDS_R_INTERNAL_LAVVIDEO_HWACCEL, _T("HWAccelDeviceD3D11"), -1);
                        }
                    }
                }
            } else {
                WriteRegistryDWORD(HKEY_CURRENT_USER, L"Software\\MPC-BE Filters\\MPC Video Renderer", L"UseD3D11", 0);
                if (curhwa == HWAccel_D3D11) {
                    if (needcopyback) {
                        pApp->WriteProfileInt(IDS_R_INTERNAL_LAVVIDEO_HWACCEL, _T("HWAccel"), HWAccel_DXVA2CopyBack);
                    } else {
                        pApp->WriteProfileInt(IDS_R_INTERNAL_LAVVIDEO_HWACCEL, _T("HWAccel"), HWAccel_DXVA2Native);
                    }
                }
            }
        } else if (m_iDSVideoRendererType == VIDRNDT_DS_MADVR) {
            if (curhwa == HWAccel_D3D11) {
                if (-1 == pApp->GetProfileInt(IDS_R_INTERNAL_LAVVIDEO_HWACCEL, _T("HWAccelDeviceD3D11"), -1)) {
                    if (needcopyback || (AfxMessageBox(L"Change hardware decoding setting from \"D3D11 Native\" to \"D3D11 Copyback\"?\n\nThat gives better stability with MadVR. Native mode may causes freezes. It also does not support deinterlacing and black border detection.", MB_YESNO, 0) == IDYES)) {
                        pApp->WriteProfileInt(IDS_R_INTERNAL_LAVVIDEO_HWACCEL, _T("HWAccelDeviceD3D11"), 0);
                    }
                }
            } else if (curhwa == HWAccel_DXVA2Native || curhwa == HWAccel_DXVA2CopyBack) {
                GPUDetect gpuinfo = GPUDetect();
                if (gpuinfo.SupportD3D11VA() && gpuinfo.GetHwaCaps() >= GPU_HWA_CAP_AV1_P0) {
                    if (AfxMessageBox(L"Change hardware decoding setting to \"D3D11 Copyback\"?\n\nThat supports more video formats than DXVA2.", MB_YESNO, 0) == IDYES) {
                        pApp->WriteProfileInt(IDS_R_INTERNAL_LAVVIDEO_HWACCEL, _T("HWAccel"), HWAccel_D3D11);
                        if (-1 == pApp->GetProfileInt(IDS_R_INTERNAL_LAVVIDEO_HWACCEL, _T("HWAccelDeviceD3D11"), -1)) {
                            pApp->WriteProfileInt(IDS_R_INTERNAL_LAVVIDEO_HWACCEL, _T("HWAccelDeviceD3D11"), 0);
                        }
                    }
                } else if (curhwa == HWAccel_DXVA2Native) {
                    if (needcopyback || (AfxMessageBox(L"Change hardware decoding setting from \"DXVA2 Native\" to \"DXVA2 Copyback\"?\n\nIn most situations copyback mode is recommended when using MadVR.", MB_YESNO, 0) == IDYES)) {
                        pApp->WriteProfileInt(IDS_R_INTERNAL_LAVVIDEO_HWACCEL, _T("HWAccel"), HWAccel_DXVA2CopyBack);
                    }
                }
            }
        } else if (m_iDSVideoRendererType == VIDRNDT_DS_EVR_CUSTOM || m_iDSVideoRendererType == VIDRNDT_DS_SYNC || m_iDSVideoRendererType == VIDRNDT_DS_EVR) {
            if (curhwa == HWAccel_D3D11) {
                GPUDetect gpuinfo = GPUDetect();
                if (!gpuinfo.SupportD3D11VA() || gpuinfo.GetHwaCaps() < GPU_HWA_CAP_AV1_P0) {
                    if (needcopyback) {
                        pApp->WriteProfileInt(IDS_R_INTERNAL_LAVVIDEO_HWACCEL, _T("HWAccel"), HWAccel_DXVA2CopyBack);
                    } else {
                        pApp->WriteProfileInt(IDS_R_INTERNAL_LAVVIDEO_HWACCEL, _T("HWAccel"), HWAccel_DXVA2Native);
                    }
                }
            } else if (curhwa == HWAccel_DXVA2CopyBack && !needcopyback) {
                if (AfxMessageBox(L"Change hardware decoding setting from \"DXVA2 Copyback\" to \"DXVA2 Native\"?\n\nThat should give better performance with the selected video renderer.", MB_YESNO, 0) == IDYES) {
                    pApp->WriteProfileInt(IDS_R_INTERNAL_LAVVIDEO_HWACCEL, _T("HWAccel"), HWAccel_DXVA2Native);
                }
            }
        } else if (m_iDSVideoRendererType == VIDRNDT_DS_VMR9RENDERLESS || m_iDSVideoRendererType == VIDRNDT_DS_VMR9WINDOWED || m_iDSVideoRendererType == VIDRNDT_DS_VMR7 || m_iDSVideoRendererType == VIDRNDT_DS_OVERLAYMIXER) {
            if (curhwa == HWAccel_DXVA2Native || curhwa == HWAccel_D3D11) {
                pApp->WriteProfileInt(IDS_R_INTERNAL_LAVVIDEO_HWACCEL, _T("HWAccel"), HWAccel_DXVA2CopyBack);
            }
        }
    }

    s.iDSVideoRendererType                  = m_iDSVideoRendererType;
    s.strAudioRendererDisplayName           = GetAudioRendererDisplayName();

    if (m_SubtitleRendererCtrl.IsWindowEnabled()) {
        auto subrenderer = static_cast<CAppSettings::SubtitleRenderer>(m_SubtitleRendererCtrl.GetItemData(m_SubtitleRendererCtrl.GetCurSel()));
        m_lastSubrenderer = subrenderer;
        s.SetSubtitleRenderer(subrenderer);
    }

    return __super::OnApply();
}

void CPPageOutput::OnUpdateVideoRendererSettings(CCmdUI* pCmdUI) {
    pCmdUI->Enable(m_iDSVideoRendererType == VIDRNDT_DS_MPCVR || m_iDSVideoRendererType == VIDRNDT_DS_MADVR
                   || m_iDSVideoRendererType == VIDRNDT_DS_EVR_CUSTOM || m_iDSVideoRendererType == VIDRNDT_DS_VMR9RENDERLESS
                   || m_iDSVideoRendererType == VIDRNDT_DS_SYNC);
}

void CPPageOutput::OpenVideoRendererSettings() {
    if (m_iDSVideoRendererType == VIDRNDT_DS_EVR_CUSTOM || m_iDSVideoRendererType == VIDRNDT_DS_VMR9RENDERLESS
            || m_iDSVideoRendererType == VIDRNDT_DS_SYNC) {
        CPPageVideoRenderer page;
        page.SetRendererType(m_iDSVideoRendererType);
        CMPCThemePropertySheet dlg(IDS_PPAGE_OUTPUT_VIDEO_RENDERER_SETTINGS, this);
        dlg.m_psh.dwFlags |= PSH_NOAPPLYNOW;
        dlg.AddPage(&page);
        if (dlg.DoModal() == IDOK) {
            OnDSRendererChange(); // refresh status icon; the surface setting may have changed
        }
        return;
    }

    GUID clsid;
    if (m_iDSVideoRendererType == VIDRNDT_DS_MPCVR) {
        clsid = CLSID_MPCVR;
    } else if (m_iDSVideoRendererType == VIDRNDT_DS_MADVR) {
        clsid = CLSID_madVR;
    } else {
        return;
    }

    auto m = AfxGetMainFrame();
    if (!m->FilterSettingsByClassID(clsid, this)) { //if it is currently in use, get the running instance
        CComPtr<IUnknown> pIU;
        if (CLSID_MPCVR == clsid && SUCCEEDED(DSObjects::CMPCVRAllocatorPresenter::InstantiateInternalMPCVR(pIU, nullptr))) {
            m->FilterSettings(pIU, this);
        } else {
            CFGFilterRegistry fvr(clsid);
            CComPtr<IBaseFilter> pBF;
            CInterfaceList<IUnknown, &IID_IUnknown> pUnks; //unused
            if (SUCCEEDED(fvr.Create(&pBF, pUnks))) { //otherwise, create our own
                m->FilterSettings(CComPtr<IUnknown>(pBF), this);
            }
        }
    }
}

void CPPageOutput::OnUpdateAudioRendererSettings(CCmdUI* pCmdUI) {
    pCmdUI->Enable(m_iAudioRendererType == m_iMPCAudioRendererType || m_iAudioRendererType == m_iSaneAudioRendererType);
}

void CPPageOutput::OpenAudioRendererSettings() {
    if (m_iAudioRendererType == m_iMPCAudioRendererType) {
        ShowPPage(CFGManager::GetMpcAudioRendererInstance);
    } else if (m_iAudioRendererType == m_iSaneAudioRendererType) {
        CPPageAudioRenderer page;
        CMPCThemePropertySheet dlg(IDS_PPAGE_OUTPUT_AUDIO_RENDERER_SETTINGS, this);
        dlg.m_psh.dwFlags |= PSH_NOAPPLYNOW;
        dlg.AddPage(&page);
        dlg.DoModal();
    }
}

void CPPageOutput::OnDSRendererChange()
{
    UpdateData();
    m_iDSVideoRendererType = (int)m_iDSVideoRendererTypeCtrl.GetItemData(m_iDSVideoRendererTypeCtrl.GetCurSel());

    if (AfxGetAppSettings().iDSVideoRendererType != m_iDSVideoRendererType) {
        if (CAppSettings::IsSubtitleRendererSupported(CAppSettings::SubtitleRenderer::INTERNAL, m_iDSVideoRendererType)) {
            if (m_lastSubrenderer == CAppSettings::SubtitleRenderer::VS_FILTER || !CAppSettings::IsSubtitleRendererRegistered(m_lastSubrenderer)) {
                m_lastSubrenderer = CAppSettings::SubtitleRenderer::INTERNAL;
            }
        }
    }

    UINT tipID = GetRendererTooltipID();
    m_wndToolTip.UpdateTipText(tipID ? ResStr(tipID) : CString(), GetDlgItem(IDC_VIDRND_COMBO));

    UpdateStatusIcon();

    UpdateSubtitleRendererList();
    SetModified();
}

UINT CPPageOutput::GetRendererTooltipID() const
{
    switch (m_iDSVideoRendererType) {
        case VIDRNDT_DS_VMR7:           return IDC_DSVMR7;
        case VIDRNDT_DS_OVERLAYMIXER:   return IDC_DSOVERLAYMIXER;
        case VIDRNDT_DS_VMR9WINDOWED:   return IDC_DSVMR9WIN;
        case VIDRNDT_DS_EVR:            return IDC_DSEVR;
        case VIDRNDT_DS_NULL_COMP:      return IDC_DSNULL_COMP;
        case VIDRNDT_DS_NULL_UNCOMP:    return IDC_DSNULL_UNCOMP;
        case VIDRNDT_DS_VMR9RENDERLESS: return IDC_DSVMR9REN;
        case VIDRNDT_DS_EVR_CUSTOM:     return IDC_DSEVR_CUSTOM;
        case VIDRNDT_DS_SYNC:           return IDC_DSSYNC;
        case VIDRNDT_DS_MADVR:          return IDC_DSMADVR;
        case VIDRNDT_DS_DXR:            return IDC_DSDXR;
        case VIDRNDT_DS_MPCVR:          return IDC_DSMPCVR;
        default:                        return 0;
    }
}

void CPPageOutput::UpdateStatusIcon()
{
    // Feature-support indicator below the renderer dropdown:
    // green tick     - supports all player functionality including HDR (MPCVR/madVR)
    // yellow warning - supports all player functionality except HDR (EVR CP/Sync)
    // red cross      - old renderer with limited functionality
    // The tooltip carries the detailed capability description of the renderer.
    HICON icon;
    UINT noteID;
    switch (m_iDSVideoRendererType) {
        case VIDRNDT_DS_MPCVR:
        case VIDRNDT_DS_MADVR:
            icon = m_tick;
            noteID = IDS_PPAGE_OUTPUT_SUPPORT_ALL_HDR;
            break;
        case VIDRNDT_DS_EVR_CUSTOM:
        case VIDRNDT_DS_SYNC:
            icon = m_warn;
            noteID = IDS_PPAGE_OUTPUT_SUPPORT_NO_HDR;
            break;
        case VIDRNDT_DS_NULL_COMP:
        case VIDRNDT_DS_NULL_UNCOMP:
            icon = nullptr; // no video rendered at all, a capability warning makes no sense
            noteID = 0;
            break;
        default:
            icon = m_cross;
            noteID = IDS_PPAGE_OUTPUT_SUPPORT_LIMITED;
            break;
    }

    m_iDSSupportIcon.SetIcon(icon);
    m_iDSSupportIcon.ShowWindow(icon ? SW_SHOW : SW_HIDE);

    CWnd* pNote = GetDlgItem(IDC_VIDRND_SUPPORT_NOTE);
    pNote->SetWindowText(noteID ? ResStr(noteID) : CString());
    pNote->ShowWindow(noteID ? SW_SHOW : SW_HIDE);

    UINT tipID = GetRendererTooltipID();
    CString tip = tipID ? ResStr(tipID) : CString();
    m_wndToolTip.UpdateTipText(tip, GetDlgItem(IDC_VIDRND_SUPPORT_ICON));
    m_wndToolTip.UpdateTipText(tip, pNote);
}

void CPPageOutput::OnAudioRendererChange() {
    UpdateData();
    SetModified();
}

const CString& CPPageOutput::GetAudioRendererDisplayName() {
    return m_AudioRendererDisplayNames[m_iAudioRendererType];
}

void CPPageOutput::OnSubtitleRendererChange()
{
    UpdateData();
    SetModified();

    m_lastSubrenderer = static_cast<CAppSettings::SubtitleRenderer>(m_SubtitleRendererCtrl.GetItemData(m_SubtitleRendererCtrl.GetCurSel()));
}

void CPPageOutput::UpdateSubtitleRendererList()
{
    auto addSubtitleRenderer = [&](CAppSettings::SubtitleRenderer nID) {
        if (!CAppSettings::IsSubtitleRendererSupported(nID, m_iDSVideoRendererType)) {
            return;
        }

        CString sName;
        switch (nID) {
            case CAppSettings::SubtitleRenderer::INTERNAL:
                sName = ResStr(IDS_SUBTITLE_RENDERER_INTERNAL);
                break;
            case CAppSettings::SubtitleRenderer::VS_FILTER:
                sName = ResStr(IDS_SUBTITLE_RENDERER_VS_FILTER);
                break;
            case CAppSettings::SubtitleRenderer::XY_SUB_FILTER:
                sName = ResStr(IDS_SUBTITLE_RENDERER_XY_SUB_FILTER);
                break;
            case CAppSettings::SubtitleRenderer::NONE:
                sName = ResStr(IDS_SUBTITLE_RENDERER_NONE);
                break;
            default:
                ASSERT(FALSE);
                break;
        }

        if (!CAppSettings::IsSubtitleRendererRegistered(nID)) {
            sName.AppendFormat(_T("   %s"), ResStr(IDS_PPAGE_OUTPUT_UNAVAILABLE).GetString());
        }

        m_SubtitleRendererCtrl.SetItemData(m_SubtitleRendererCtrl.AddString(sName), static_cast<int>(nID));
    };

    m_SubtitleRendererCtrl.SetRedraw(FALSE);
    while (m_SubtitleRendererCtrl.DeleteString(0) != CB_ERR);
    addSubtitleRenderer(CAppSettings::SubtitleRenderer::INTERNAL);
    addSubtitleRenderer(CAppSettings::SubtitleRenderer::VS_FILTER);
    addSubtitleRenderer(CAppSettings::SubtitleRenderer::XY_SUB_FILTER);
    addSubtitleRenderer(CAppSettings::SubtitleRenderer::NONE);
    m_SubtitleRendererCtrl.SetCurSel(0);
    if (m_SubtitleRendererCtrl.IsWindowEnabled()) {
        for (int j = 0; j < m_SubtitleRendererCtrl.GetCount(); ++j) {
            if ((UINT)m_lastSubrenderer == m_SubtitleRendererCtrl.GetItemData(j)) {
                m_SubtitleRendererCtrl.SetCurSel(j);
                break;
            }
        }
    }
    m_SubtitleRendererCtrl.EnableWindow(m_SubtitleRendererCtrl.GetCount() > 1);
    CorrectComboListWidth(m_SubtitleRendererCtrl);
    m_SubtitleRendererCtrl.SetRedraw(TRUE);
    m_SubtitleRendererCtrl.Invalidate();
    m_SubtitleRendererCtrl.UpdateWindow();
}
