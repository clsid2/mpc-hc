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

#include <future>
#include <memory>
#include <vector>
#include "DVBChannel.h"

class CMainFrame;
struct CPlayerGraph;
class OpenDeviceData;

class TunerScanData
{
public:
    ULONG FrequencyStart;
    ULONG FrequencyStop;
    ULONG Bandwidth;
    ULONG SymbolRate;
    LONG  Offset;
    HWND  Hwnd;
};

// Analog capture, the BDA graph, the DVB channel state and its EIT info, and
// the tuner scan, from the dialog or headless. Composed into CMainFrame, which
// routes its commands here from OnCmdMsg and forwards the scan and EIT window
// messages.
class CCaptureController : public CCmdTarget
{
    CMainFrame& m_frame;
    CPlayerGraph& m_graph;

    HRESULT BuildCapture(IPin* pPin, IBaseFilter* pBF[3], const GUID& majortype, AM_MEDIA_TYPE* pmt); // pBF: 0 buff, 1 enc, 2 mux, pmt is for 1 enc
    bool BuildToCapturePreviewPin(IBaseFilter* pVidCap, IPin** pVidCapPin, IPin** pVidPrevPin,
                                  IBaseFilter* pAudCap, IPin** pAudCapPin, IPin** pAudPrevPin);

    bool        m_bStopTunerScan;

public:
    explicit CCaptureController(CMainFrame& frame);

    CStringW m_VidDispName, m_AudDispName;

    void DoTunerScan(TunerScanData* pTSD);
    void OpenCapture(OpenDeviceData* pODD);
    HRESULT OpenBDAGraph();
    void ReleaseDVBState();
    void OpenSetupCaptureBar();

    void StartTunerScan(CAutoPtr<TunerScanData> pTSD);
    void StopTunerScan();
    HRESULT SetChannel(int nChannel);

    // Headless tuner scan, driven by /dvbscan rather than CTunerScanDlg.
    // DoTunerScan reports through window messages, so this listens for them
    // instead of the dialog and writes the result out. The scan engine itself
    // is unchanged and unaware of which listener it is talking to.
    bool m_bHeadlessDVBScan = false;
    std::vector<CBDAChannel> m_headlessDVBScanChannels;
    void StartHeadlessDVBScan();
    void FinishHeadlessDVBScan();

    void AddCurDevToPlaylist();

    CString GetCaptureTitle();

    // capturing
    bool m_fCapturing;
    bool BuildGraphVideoAudio(int fVPreview, bool fVCapture, int fAPreview, bool fACapture);
    bool StartCapture(), StopCapture();

    // DVB capture
    void UpdateCurrentChannelInfo(bool bShowOSD = true, bool bShowInfoBar = false);
    LRESULT OnCurrentChannelInfoUpdated(WPARAM wParam, LPARAM lParam);

    struct DVBState {
        struct EITData {
            HRESULT hr        = E_FAIL;
            EventDescriptor NowNext;
            bool bShowOSD     = true;
            bool bShowInfoBar = false;
        };

        CString         sChannelName;                // Current channel name
        CBDAChannel*    pChannel          = nullptr; // Pointer to current channel object
        EventDescriptor NowNext;                     // Current channel EIT
        bool            bActive           = false;   // True when channel is active
        bool            bSetChannelActive = false;   // True when channel change is in progress
        bool            bInfoActive       = false;   // True when EIT data update is in progress
        bool            bAbortInfo        = true;    // True when aborting current EIT update
        std::future<DVBState::EITData> infoData;

        void Reset() {
            sChannelName.Empty();
            pChannel          = nullptr;
            NowNext           = EventDescriptor();
            bActive           = false;
            bSetChannelActive = false;
            bInfoActive       = false;
            bAbortInfo        = true;
        }

        void Join() {
            if (infoData.valid()) {
                bAbortInfo = true;
                infoData.wait();
            }
        }

        ~DVBState() {
            bAbortInfo = true;
        }
    };

    std::unique_ptr<DVBState> m_pDVBState = nullptr;

    // Reached through the frame's forwarders, and only in headless scan mode:
    // with the dialog running these go to it instead, because DoTunerScan sends
    // to the HWND it was handed.
    LRESULT OnHeadlessScanNewChannel(WPARAM wParam, LPARAM lParam);
    LRESULT OnHeadlessScanEnd(WPARAM wParam, LPARAM lParam);

    afx_msg void OnViewCapture();
    afx_msg void OnUpdateViewCapture(CCmdUI* pCmdUI);
    afx_msg void OnTunerScan();
    afx_msg void OnUpdateTunerScan(CCmdUI* pCmdUI);

    bool        m_bScanDlgOpened;

    DECLARE_MESSAGE_MAP()
};
