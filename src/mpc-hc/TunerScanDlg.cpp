/*
 * (C) 2009-2017 see Authors.txt
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

// TunerScanDlg.cpp : implementation file
//

#include "stdafx.h"
#include "mplayerc.h"
#include "MainFrm.h"
#include "TunerScanDlg.h"
#include "DVBChannel.h"
#include "CMPCTheme.h"
#include "CMPCThemeUtil.h"


enum TSC_COLUMN {
    TSCC_NUMBER,
    TSCC_NAME,
    TSCC_FREQUENCY,
    TSCC_ENCRYPTED,
    TSCC_VIDEO_FORMAT,
    TSCC_VIDEO_FPS,
    TSCC_VIDEO_RES,
    TSCC_VIDEO_AR,
    TSCC_CHANNEL
};

// CTunerScanDlg dialog

IMPLEMENT_DYNAMIC(CTunerScanDlg, CMPCThemeDialog)

CTunerScanDlg::CTunerScanDlg(CMainFrame* pMainFrame)
    : CMPCThemeDialog(CTunerScanDlg::IDD, pMainFrame)
    , m_pMainFrame(pMainFrame)
    , m_bInProgress(false)
{
    const CAppSettings& s = AfxGetAppSettings();

    m_ulFrequencyStart = s.iBDAScanFreqStart;
    m_ulFrequencyEnd = s.iBDAScanFreqEnd;
    m_ulBandwidth = s.iBDABandwidth * 1000;
    m_ulSymbolRate = s.iBDASymbolRate;
    m_bUseOffset = s.fBDAUseOffset;
    m_lOffset = s.iBDAOffset;
    m_bIgnoreEncryptedChannels = s.fBDAIgnoreEncryptedChannels;
}

CTunerScanDlg::~CTunerScanDlg()
{
}

BOOL CTunerScanDlg::OnInitDialog()
{
    CMPCThemeDialog::OnInitDialog();

    m_OffsetEditBox.EnableWindow(m_bUseOffset);

    m_ChannelList.InsertColumn(TSCC_NUMBER, ResStr(IDS_DVB_CHANNEL_NUMBER), LVCFMT_LEFT, 35);
    m_ChannelList.InsertColumn(TSCC_NAME, ResStr(IDS_DVB_CHANNEL_NAME), LVCFMT_LEFT, 190);
    m_ChannelList.InsertColumn(TSCC_FREQUENCY, ResStr(IDS_DVB_CHANNEL_FREQUENCY), LVCFMT_LEFT, 65);
    m_ChannelList.InsertColumn(TSCC_ENCRYPTED, ResStr(IDS_DVB_CHANNEL_ENCRYPTION), LVCFMT_CENTER, 55);
    m_ChannelList.InsertColumn(TSCC_VIDEO_FORMAT, ResStr(IDS_DVB_CHANNEL_FORMAT), LVCFMT_CENTER, 55);
    m_ChannelList.InsertColumn(TSCC_VIDEO_FPS, ResStr(IDS_DVB_CHANNEL_FPS), LVCFMT_CENTER, 50);
    m_ChannelList.InsertColumn(TSCC_VIDEO_RES, ResStr(IDS_DVB_CHANNEL_RESOLUTION), LVCFMT_CENTER, 70);
    m_ChannelList.InsertColumn(TSCC_VIDEO_AR, ResStr(IDS_DVB_CHANNEL_ASPECT_RATIO), LVCFMT_CENTER, 50);
    m_ChannelList.InsertColumn(TSCC_CHANNEL, _T("Channel"), LVCFMT_LEFT, 0);

    m_Progress.SetRange(0, 100);
    CMPCThemeUtil::fulfillThemeReqs(&m_Progress);
    m_Strength.SetRange(0, 100);
    CMPCThemeUtil::fulfillThemeReqs(&m_Strength);
    m_Quality.SetRange(0, 100);
    CMPCThemeUtil::fulfillThemeReqs(&m_Quality);

    m_btnSave.EnableWindow(FALSE);

    return TRUE;
}

void CTunerScanDlg::DoDataExchange(CDataExchange* pDX)
{
    CMPCThemeDialog::DoDataExchange(pDX);
    DDX_Text(pDX, IDC_FREQ_START, m_ulFrequencyStart);
    DDX_Text(pDX, IDC_FREQ_END, m_ulFrequencyEnd);
    DDX_Text(pDX, IDC_BANDWIDTH, m_ulBandwidth);
    DDX_Text(pDX, IDC_SYMBOLRATE, m_ulSymbolRate);
    DDX_Text(pDX, IDC_OFFSET, m_lOffset);
    DDX_Check(pDX, IDC_CHECK_OFFSET, m_bUseOffset);
    DDX_Check(pDX, IDC_CHECK_IGNORE_ENCRYPTED, m_bIgnoreEncryptedChannels);
    DDX_Control(pDX, IDC_PROGRESS, m_Progress);
    DDX_Control(pDX, IDC_STRENGTH, m_Strength);
    DDX_Control(pDX, IDC_QUALITY, m_Quality);
    DDX_Control(pDX, IDC_CHANNEL_LIST, m_ChannelList);
    DDX_Control(pDX, ID_START, m_btnStart);
    DDX_Control(pDX, ID_SAVE, m_btnSave);
    DDX_Control(pDX, IDCANCEL, m_btnCancel);
    DDX_Control(pDX, IDC_OFFSET, m_OffsetEditBox);
    fulfillThemeReqs();
}

BEGIN_MESSAGE_MAP(CTunerScanDlg, CMPCThemeDialog)
    ON_MESSAGE(WM_TUNER_SCAN_PROGRESS, OnScanProgress)
    ON_MESSAGE(WM_TUNER_SCAN_END, OnScanEnd)
    ON_MESSAGE(WM_TUNER_STATS, OnStats)
    ON_MESSAGE(WM_TUNER_NEW_CHANNEL, OnNewChannel)
    ON_BN_CLICKED(ID_SAVE, &CTunerScanDlg::OnBnClickedSave)
    ON_BN_CLICKED(ID_START, &CTunerScanDlg::OnBnClickedStart)
    ON_BN_CLICKED(IDCANCEL, &CTunerScanDlg::OnBnClickedCancel)
    ON_BN_CLICKED(IDC_CHECK_OFFSET, &CTunerScanDlg::OnBnClickedCheckOffset)
END_MESSAGE_MAP()

// CTunerScanDlg message handlers

void CTunerScanDlg::OnBnClickedSave()
{
    auto& DVBChannels = AfxGetAppSettings().m_DVBChannels;
    const size_t maxChannelsNum = ID_NAVIGATE_JUMPTO_SUBITEM_END - ID_NAVIGATE_JUMPTO_SUBITEM_START + 1;

    for (int i = 0; i < m_ChannelList.GetItemCount(); i++) {
        try {
            CBDAChannel channel(m_ChannelList.GetItemText(i, TSCC_CHANNEL));
            auto it = std::find(std::begin(DVBChannels), std::end(DVBChannels), channel);
            if (it != DVBChannels.end()) {
                // replace existing channel
                channel.SetPrefNumber(it->GetPrefNumber());
                *it = channel;
            } else {
                // add new channel to the end
                const size_t size = DVBChannels.size();
                if (size < maxChannelsNum) {
                    channel.SetPrefNumber((int)size);
                    DVBChannels.push_back(channel);
                } else {
                    // Just to be safe. We have 600 channels limit, but we never know what user might load there.
                    CString msg;
                    msg.Format(_T("Unable to add new channel \"%s\" to the list. Channels list is full. Please notify developers about the problem."), channel.GetName());
                    AfxMessageBox(msg, MB_OK | MB_ICONERROR);
                }
            }
        } catch (CException* e) {
            // The tokenisation can fail if the input string was invalid
            TRACE(_T("Failed to parse a DVB channel from string \"%s\""), m_ChannelList.GetItemText(i, TSCC_CHANNEL).GetString());
            ASSERT(FALSE);
            e->Delete();
        }
    }
    m_pMainFrame->SetChannel(0);

    OnOK();
}

void CTunerScanDlg::OnBnClickedStart()
{
    if (!m_bInProgress) {
        UpdateData(true);
        CAutoPtr<TunerScanData> pTSD(DEBUG_NEW TunerScanData);
        pTSD->Hwnd           = m_hWnd;
        pTSD->FrequencyStart = m_ulFrequencyStart;
        pTSD->FrequencyStop  = m_ulFrequencyEnd;
        pTSD->Bandwidth      = m_ulBandwidth;
        pTSD->SymbolRate     = m_ulSymbolRate;
        pTSD->Offset         = m_bUseOffset ? m_lOffset : 0;
        SaveScanSettings();

        m_ChannelList.DeleteAllItems();
        m_pMainFrame->StartTunerScan(pTSD);

        SetProgress(true);
    } else {
        m_pMainFrame->StopTunerScan();
    }
}

void CTunerScanDlg::OnBnClickedCancel()
{
    if (m_bInProgress) {
        m_pMainFrame->StopTunerScan();
    }
    m_pMainFrame->SetChannel(AfxGetAppSettings().nDVBLastChannel);

    OnCancel();
}

void CTunerScanDlg::OnBnClickedCheckOffset()
{
    UpdateData(true);
    m_OffsetEditBox.EnableWindow(m_bUseOffset);
}

LRESULT CTunerScanDlg::OnScanProgress(WPARAM wParam, LPARAM lParam)
{
    m_Progress.SetPos(static_cast<int>(wParam));
    return TRUE;
}

LRESULT CTunerScanDlg::OnScanEnd(WPARAM wParam, LPARAM lParam)
{
    SetProgress(false);
    return TRUE;
}

LRESULT CTunerScanDlg::OnStats(WPARAM wParam, LPARAM lParam)
{
    m_Strength.SetPos((int)wParam);
    m_Quality.SetPos((int)lParam);
    return TRUE;
}

LRESULT CTunerScanDlg::OnNewChannel(WPARAM wParam, LPARAM lParam)
{
    try {
        CBDAChannel channel((LPCTSTR)lParam);
        if (!m_bIgnoreEncryptedChannels || !channel.IsEncrypted()) {
            CString strTemp;
            int nItem, nChannelNumber;

            if (channel.GetOriginNumber() != 0) { // LCN is available
                nChannelNumber = channel.GetOriginNumber();
                // Insert new channel so that channels are sorted by their logical number
                for (nItem = 0; nItem < m_ChannelList.GetItemCount(); nItem++) {
                    if ((int)m_ChannelList.GetItemData(nItem) > nChannelNumber || (int)m_ChannelList.GetItemData(nItem) == 0) {
                        break;
                    }
                }
            } else {
                nChannelNumber = 0;
                nItem = m_ChannelList.GetItemCount();
            }

            if (channel.HasATSCNumber()) {
                // ATSC numbers are two-part and are what a viewer recognises,
                // so show 9.1 rather than the 9001 used internally to order them.
                strTemp.Format(_T("%d.%d"), channel.GetATSCMajor(), channel.GetATSCMinor());
            } else {
                strTemp.Format(_T("%d"), nChannelNumber);
            }
            nItem = m_ChannelList.InsertItem(nItem, strTemp);

            m_ChannelList.SetItemData(nItem, channel.GetOriginNumber());

            m_ChannelList.SetItemText(nItem, TSCC_NAME, channel.GetName());

            strTemp.Format(_T("%lu"), channel.GetFrequency());
            m_ChannelList.SetItemText(nItem, TSCC_FREQUENCY, strTemp);

            m_ChannelList.SetItemText(nItem, TSCC_ENCRYPTED, ResStr(channel.IsEncrypted() ? IDS_YES : IDS_NO));
            if (channel.GetVideoType() == BDA_H264) {
                strTemp = _T("H.264");
            } else if (channel.GetVideoType() == BDA_HEVC) {
                strTemp = _T("HEVC");
            } else if (channel.GetVideoPID()) {
                strTemp = _T("MPEG-2");
            } else {
                strTemp = _T("-");
            }
            m_ChannelList.SetItemText(nItem, TSCC_VIDEO_FORMAT, strTemp);
            strTemp = channel.GetVideoFpsDesc();
            m_ChannelList.SetItemText(nItem, TSCC_VIDEO_FPS, strTemp);
            if (channel.GetVideoWidth() || channel.GetVideoHeight()) {
                strTemp.Format(_T("%lux%lu"), channel.GetVideoWidth(), channel.GetVideoHeight());
            } else {
                strTemp = _T("-");
            }
            m_ChannelList.SetItemText(nItem, TSCC_VIDEO_RES, strTemp);
            strTemp.Format(_T("%lu/%lu"), channel.GetVideoARy(), channel.GetVideoARx());
            m_ChannelList.SetItemText(nItem, TSCC_VIDEO_AR, strTemp);
            m_ChannelList.SetItemText(nItem, TSCC_CHANNEL, (LPCTSTR) lParam);
        }
    } catch (CException* e) {
        // The tokenisation can fail if the input string was invalid
        TRACE(_T("Failed to parse a DVB channel from string \"%s\""), (LPCTSTR)lParam);
        ASSERT(FALSE);
        e->Delete();
        return FALSE;
    }

    return TRUE;
}

void CTunerScanDlg::SetProgress(bool bState)
{
    if (bState) {
        m_btnStart.SetWindowTextW(ResStr(IDS_DVB_CHANNEL_STOP_SCAN));
        m_btnSave.EnableWindow(FALSE);
    } else {
        m_btnStart.SetWindowTextW(ResStr(IDS_DVB_CHANNEL_START_SCAN));
        m_Progress.SetPos(0);
        m_btnSave.EnableWindow(TRUE);
    }

    m_bInProgress = bState;
}

void CTunerScanDlg::SaveScanSettings()
{
    CAppSettings& s = AfxGetAppSettings();

    s.iBDAScanFreqStart = m_ulFrequencyStart;
    s.iBDAScanFreqEnd = m_ulFrequencyEnd;
    div_t bdw = div(m_ulBandwidth, 1000);
    s.iBDABandwidth = bdw.quot;
    s.iBDASymbolRate = m_ulSymbolRate;
    s.fBDAUseOffset = !!m_bUseOffset;
    s.iBDAOffset = m_lOffset;
    s.fBDAIgnoreEncryptedChannels = !!m_bIgnoreEncryptedChannels;
}
