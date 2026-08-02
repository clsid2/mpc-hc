/*
 * (C) 2015-2017 see Authors.txt
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
#include "PPageAudioRenderer.h"

#include "mplayerc.h"
#include "VersionInfo.h"

#include <FunctionDiscoveryKeys_devpkey.h>
#include <Mmdeviceapi.h>
#include <sanear/src/Settings.h>
#include "AppSettings.h"

namespace
{
    std::vector<std::pair<CString, CString>> GetDevices()
    {
        std::vector<std::pair<CString, CString>> ret;

        CComPtr<IMMDeviceEnumerator> enumerator;
        CComPtr<IMMDeviceCollection> collection;
        UINT count = 0;

        if (SUCCEEDED(enumerator.CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_INPROC_SERVER)) &&
                SUCCEEDED(enumerator->EnumAudioEndpoints(eRender, DEVICE_STATE_ACTIVE | DEVICE_STATE_UNPLUGGED, &collection)) &&
                SUCCEEDED(collection->GetCount(&count))) {

            for (UINT i = 0; i < count; i++) {
                CComPtr<IMMDevice> device;
                CComHeapPtr<WCHAR> id;
                CComPtr<IPropertyStore> devicePropertyStore;
                PROPVARIANT friendlyName;
                PropVariantInit(&friendlyName);

                if (SUCCEEDED(collection->Item(i, &device)) &&
                        SUCCEEDED(device->GetId(&id)) &&
                        SUCCEEDED(device->OpenPropertyStore(STGM_READ, &devicePropertyStore)) &&
                        SUCCEEDED(devicePropertyStore->GetValue(PKEY_Device_FriendlyName, &friendlyName))) {

                    ret.emplace_back(friendlyName.pwszVal, static_cast<LPWSTR>(id));
                    PropVariantClear(&friendlyName);
                }
            }
        }

        return ret;
    }
}

IMPLEMENT_DYNAMIC(CPPageAudioRenderer, CMPCThemePPageBase)
CPPageAudioRenderer::CPPageAudioRenderer()
    : CMPCThemePPageBase(IDD, IDS_PPAGE_AUDIORENDERER_TITLE)
    , m_bExclusiveMode(FALSE)
    , m_bCrossfeedEnabled(FALSE)
    , m_bIgnoreSystemChannelMixer(TRUE)
{
    m_bPopupHosted = true;
}

void CPPageAudioRenderer::DoDataExchange(CDataExchange* pDX)
{
    __super::DoDataExchange(pDX);
    DDX_Check(pDX, IDC_CHECK1, m_bExclusiveMode);
    DDX_Check(pDX, IDC_CHECK3, m_bCrossfeedEnabled);
    DDX_Check(pDX, IDC_CHECK4, m_bIgnoreSystemChannelMixer);

    DDX_Control(pDX, IDC_COMBO1, m_combo1);
    DDX_Control(pDX, IDC_SLIDER1, m_slider1);
    DDX_Control(pDX, IDC_SLIDER2, m_slider2);
}

BEGIN_MESSAGE_MAP(CPPageAudioRenderer, CMPCThemePPageBase)
    ON_WM_HSCROLL()
    ON_UPDATE_COMMAND_UI(IDC_BUTTON1, OnUpdateCrossfeedGroup)
    ON_BN_CLICKED(IDC_BUTTON1, OnCMoyButton)
    ON_UPDATE_COMMAND_UI(IDC_BUTTON2, OnUpdateCrossfeedGroup)
    ON_BN_CLICKED(IDC_BUTTON2, OnJMeierButton)
    ON_UPDATE_COMMAND_UI(IDC_STATIC1, OnUpdateCrossfeedGroup)
    ON_UPDATE_COMMAND_UI(IDC_SLIDER1, OnUpdateCrossfeedGroup)
    ON_UPDATE_COMMAND_UI(IDC_STATIC2, OnUpdateCrossfeedCutoffLabel)
    ON_UPDATE_COMMAND_UI(IDC_STATIC3, OnUpdateCrossfeedGroup)
    ON_UPDATE_COMMAND_UI(IDC_SLIDER2, OnUpdateCrossfeedGroup)
    ON_UPDATE_COMMAND_UI(IDC_STATIC4, OnUpdateCrossfeedLevelLabel)
END_MESSAGE_MAP()

BOOL CPPageAudioRenderer::OnInitDialog()
{
    __super::OnInitDialog();

    const auto& s = AfxGetAppSettings();

    CString defaultString;
    defaultString.LoadString(IDS_PPAGE_OUTPUT_SYS_DEF);
    m_combo1.AddString(defaultString);
    m_deviceIds.emplace_back();
    m_combo1.SetItemData(0, 0);
    m_combo1.SetCurSel(0);

    for (const auto& device : GetDevices()) {
        int idx = m_combo1.AddString(device.first);
        if (idx >= 0) {
            m_deviceIds.emplace_back(device.second);
            m_combo1.SetItemData(idx, m_deviceIds.size() - 1);
        }
    }

    CComHeapPtr<WCHAR> pDeviceId;
    if (SUCCEEDED(s.sanear->GetOutputDevice(&pDeviceId, &m_bExclusiveMode, nullptr))) {
        if (pDeviceId && pDeviceId[0] != '\0') {
            bool found = false;
            for (size_t i = 0; i < m_deviceIds.size(); i++) {
                if (m_deviceIds[i] == pDeviceId) {
                    for (int j = 0; j < m_combo1.GetCount(); j++) {
                        if (m_combo1.GetItemData(j) == i) {
                            m_combo1.SetCurSel(j);
                        }
                    }
                    found = true;
                    break;
                }
            }
            if (!found) {
                // device does not exist anymore or is invalid, reset to default
                s.sanear->SetOutputDevice(nullptr, m_bExclusiveMode, 200);
            }
        }
    }

    m_bCrossfeedEnabled = s.sanear->GetCrossfeedEnabled();
    m_bIgnoreSystemChannelMixer = s.sanear->GetIgnoreSystemChannelMixer();

    m_slider1.SetRangeMin(SaneAudioRenderer::ISettings::CROSSFEED_CUTOFF_FREQ_MIN);
    m_slider1.SetRangeMax(SaneAudioRenderer::ISettings::CROSSFEED_CUTOFF_FREQ_MAX);
    m_slider2.SetRangeMin(SaneAudioRenderer::ISettings::CROSSFEED_LEVEL_MIN);
    m_slider2.SetRangeMax(SaneAudioRenderer::ISettings::CROSSFEED_LEVEL_MAX);

    UINT32 crossfeedCuttoffFrequency;
    UINT32 crossfeedLevel;
    s.sanear->GetCrossfeedSettings(&crossfeedCuttoffFrequency, &crossfeedLevel);
    m_slider1.SetPos(crossfeedCuttoffFrequency);
    m_slider2.SetPos(crossfeedLevel);

    UpdateData(FALSE);

    return TRUE;
}

BOOL CPPageAudioRenderer::OnApply()
{
    if (!UpdateData(TRUE)) {
        return FALSE;
    }

    const auto& s = AfxGetAppSettings();

    CString deviceId;
    DWORD_PTR idx = m_combo1.GetItemData(m_combo1.GetCurSel());
    if (idx != CB_ERR) {
        deviceId = m_deviceIds[idx];
    }

    UINT32 buffer;
    s.sanear->GetOutputDevice(nullptr, nullptr, &buffer);
    s.sanear->SetOutputDevice(deviceId, m_bExclusiveMode, buffer);

    s.sanear->SetCrossfeedSettings(m_slider1.GetPos(), m_slider2.GetPos());
    s.sanear->SetCrossfeedEnabled(m_bCrossfeedEnabled);
    s.sanear->SetIgnoreSystemChannelMixer(m_bIgnoreSystemChannelMixer);

    return __super::OnApply();
}

void CPPageAudioRenderer::OnHScroll(UINT nSBCode, UINT nPos, CScrollBar* pScrollBar)
{
    if (pScrollBar && (*pScrollBar == m_slider1 || *pScrollBar == m_slider2)) {
        SetModified(TRUE);
    }
}

void CPPageAudioRenderer::OnCMoyButton()
{
    m_slider1.SetPos(SaneAudioRenderer::ISettings::CROSSFEED_CUTOFF_FREQ_CMOY);
    m_slider2.SetPos(SaneAudioRenderer::ISettings::CROSSFEED_LEVEL_CMOY);
    SetModified(TRUE);
}

void CPPageAudioRenderer::OnJMeierButton()
{
    m_slider1.SetPos(SaneAudioRenderer::ISettings::CROSSFEED_CUTOFF_FREQ_JMEIER);
    m_slider2.SetPos(SaneAudioRenderer::ISettings::CROSSFEED_LEVEL_JMEIER);
    SetModified(TRUE);
}

void CPPageAudioRenderer::OnUpdateCrossfeedGroup(CCmdUI* pCmdUI)
{
    pCmdUI->Enable(IsDlgButtonChecked(IDC_CHECK3));
}

void CPPageAudioRenderer::OnUpdateCrossfeedCutoffLabel(CCmdUI* pCmdUI)
{
    OnUpdateCrossfeedGroup(pCmdUI);
    CString label;
    label.Format(_T("%d Hz"), m_slider1.GetPos());
    pCmdUI->SetText(label);
}

void CPPageAudioRenderer::OnUpdateCrossfeedLevelLabel(CCmdUI* pCmdUI)
{
    OnUpdateCrossfeedGroup(pCmdUI);
    int pos = m_slider2.GetPos();
    CString label;
    label.Format(_T("%d.%d dB"), pos / 10, pos % 10);
    pCmdUI->SetText(label);
}
