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
#include "CaptureController.h"
#include "MainFrm.h"
#include "mplayerc.h"
#include "GraphThread.h"
#include "TunerScanDlg.h"
#include <DSUtil.h>
#include <moreuuids.h>
#include <IMPCVRSubclassReplacement.h>

BEGIN_MESSAGE_MAP(CCaptureController, CCmdTarget)
    ON_COMMAND(ID_VIEW_CAPTURE, OnViewCapture)
    ON_UPDATE_COMMAND_UI(ID_VIEW_CAPTURE, OnUpdateViewCapture)
    ON_COMMAND(ID_NAVIGATE_TUNERSCAN, OnTunerScan)
    ON_UPDATE_COMMAND_UI(ID_NAVIGATE_TUNERSCAN, OnUpdateTunerScan)
END_MESSAGE_MAP()

CCaptureController::CCaptureController(CMainFrame& frame)
    : m_frame(frame)
    , m_graph(frame.Graph())
    , m_bStopTunerScan(false)
    , m_fCapturing(false)
    , m_bScanDlgOpened(false)
{
}

void CCaptureController::OnViewCapture()
{
    const bool bHiding = m_frame.m_controls.ControlChecked(CMainFrameControls::Panel::CAPTURE);
    m_frame.m_controls.ToggleControl(CMainFrameControls::Panel::CAPTURE);
    AfxGetAppSettings().bHideCaptureSettings = bHiding;
}

void CCaptureController::OnUpdateViewCapture(CCmdUI* pCmdUI)
{
    pCmdUI->SetCheck(m_frame.m_controls.ControlChecked(CMainFrameControls::Panel::CAPTURE));
    pCmdUI->Enable(m_frame.GetLoadState() == MLS::LOADED && m_frame.GetPlaybackMode() == PM_ANALOG_CAPTURE);
}

void CCaptureController::OnTunerScan()
{
    m_bScanDlgOpened = true;
    CTunerScanDlg dlg(&m_frame);
    dlg.DoModal();
    m_bScanDlgOpened = false;
}

void CCaptureController::OnUpdateTunerScan(CCmdUI* pCmdUI)
{
    pCmdUI->Enable(m_frame.GetLoadState() == MLS::LOADED && m_frame.GetPlaybackMode() == PM_DIGITAL_CAPTURE);
}

// Called from GraphThread
HRESULT CCaptureController::OpenBDAGraph()
{
    m_frame.lastOpenFile.Empty();

    HRESULT hr = m_graph.m_pGB->RenderFile(L"", L"");
    if (SUCCEEDED(hr)) {
        m_frame.SetPlaybackMode(PM_DIGITAL_CAPTURE);
        m_pDVBState = std::make_unique<DVBState>();

        // Check for supported interfaces
        BeginEnumFilters(m_graph.m_pGB, pEF, pBF);
        bool fsf = false;
        CLSID clsid = GetCLSID(pBF);
        // IFileSourceFilter
        if (!m_graph.m_pFSF) {
            m_graph.m_pFSF = pBF;
            if (m_graph.m_pFSF) {
                fsf = true;
                if (!m_graph.m_pAMNS) {
                    m_graph.m_pAMNS = pBF;
                }
            }
        }
        // IAMStreamSelect / IDirectVobSub
        if (!fsf) {
            if (clsid == __uuidof(CAudioSwitcherFilter)) {
                m_graph.m_pAudioSwitcherSS = pBF;
            } else {
                if (clsid == CLSID_VSFilter || clsid == CLSID_XySubFilter) {
                    if (!AfxGetAppSettings().IsISRAutoLoadEnabled()) {
                        m_frame.m_subs.m_pDVS = pBF;
                        m_frame.m_subs.m_pDVS2 = pBF;
                    }
                }
            }
        }
        // Others
        if (!m_graph.m_pLN21) {
            m_graph.m_pLN21 = pBF;
        }
        if (!m_graph.m_pAMMC[0]) {
            m_graph.m_pAMMC[0] = pBF;
        } else if (!m_graph.m_pAMMC[1]) {
            m_graph.m_pAMMC[1] = pBF;
        }
        EndEnumFilters;

        ASSERT(m_graph.m_pFSF);

        // BDA graph builder implements IAMStreamSelect
        m_graph.m_pSplitterSS = m_graph.m_pGB;
    }
    return hr;
}

// Called from CloseMediaPrivate: waits for a pending EIT update, then drops the state OpenBDAGraph created
void CCaptureController::ReleaseDVBState()
{
    if (m_pDVBState) {
        m_pDVBState->Join();
        m_pDVBState = nullptr;
    }
}

// Called from GraphThread
void CCaptureController::OpenCapture(OpenDeviceData* pODD)
{
    m_frame.lastOpenFile.Empty();

    m_frame.m_wndCaptureBar.InitControls();

    CStringW vidfrname, audfrname;
    CComPtr<IBaseFilter> pVidCapTmp, pAudCapTmp;

    m_VidDispName = pODD->DisplayName[0];

    if (!m_VidDispName.IsEmpty()) {
        if (!CreateFilter(m_VidDispName, &pVidCapTmp, vidfrname)) {
            throw (UINT)IDS_MAINFRM_96;
        }
    }

    m_AudDispName = pODD->DisplayName[1];

    if (!m_AudDispName.IsEmpty()) {
        if (!CreateFilter(m_AudDispName, &pAudCapTmp, audfrname)) {
            throw (UINT)IDS_MAINFRM_96;
        }
    }

    if (!pVidCapTmp && !pAudCapTmp) {
        throw (UINT)IDS_MAINFRM_98;
    }

    m_graph.m_pCGB = nullptr;
    m_graph.m_pVidCap = nullptr;
    m_graph.m_pAudCap = nullptr;

    if (FAILED(m_graph.m_pCGB.CoCreateInstance(CLSID_CaptureGraphBuilder2))) {
        throw (UINT)IDS_MAINFRM_99;
    }

    HRESULT hr;

    m_graph.m_pCGB->SetFiltergraph(m_graph.m_pGB);

    if (pVidCapTmp) {
        if (FAILED(hr = m_graph.m_pGB->AddFilter(pVidCapTmp, vidfrname))) {
            throw (UINT)IDS_CAPTURE_ERROR_VID_FILTER;
        }

        m_graph.m_pVidCap = pVidCapTmp;

        if (!pAudCapTmp) {
            if (FAILED(m_graph.m_pCGB->FindInterface(&PIN_CATEGORY_CAPTURE, &MEDIATYPE_Interleaved, m_graph.m_pVidCap, IID_PPV_ARGS(&m_graph.m_pAMVSCCap)))
                    && FAILED(m_graph.m_pCGB->FindInterface(&PIN_CATEGORY_CAPTURE, &MEDIATYPE_Video, m_graph.m_pVidCap, IID_PPV_ARGS(&m_graph.m_pAMVSCCap)))) {
                TRACE(_T("Warning: No IAMStreamConfig interface for vidcap capture"));
            }

            if (FAILED(m_graph.m_pCGB->FindInterface(&PIN_CATEGORY_PREVIEW, &MEDIATYPE_Interleaved, m_graph.m_pVidCap, IID_PPV_ARGS(&m_graph.m_pAMVSCPrev)))
                    && FAILED(m_graph.m_pCGB->FindInterface(&PIN_CATEGORY_PREVIEW, &MEDIATYPE_Video, m_graph.m_pVidCap, IID_PPV_ARGS(&m_graph.m_pAMVSCPrev)))) {
                TRACE(_T("Warning: No IAMStreamConfig interface for vidcap capture"));
            }

            if (FAILED(m_graph.m_pCGB->FindInterface(&PIN_CATEGORY_CAPTURE, &MEDIATYPE_Audio, m_graph.m_pVidCap, IID_PPV_ARGS(&m_graph.m_pAMASC)))
                    && FAILED(m_graph.m_pCGB->FindInterface(&PIN_CATEGORY_PREVIEW, &MEDIATYPE_Audio, m_graph.m_pVidCap, IID_PPV_ARGS(&m_graph.m_pAMASC)))) {
                TRACE(_T("Warning: No IAMStreamConfig interface for vidcap"));
            } else {
                m_graph.m_pAudCap = m_graph.m_pVidCap;
            }
        } else {
            if (FAILED(m_graph.m_pCGB->FindInterface(&PIN_CATEGORY_CAPTURE, &MEDIATYPE_Video, m_graph.m_pVidCap, IID_PPV_ARGS(&m_graph.m_pAMVSCCap)))) {
                TRACE(_T("Warning: No IAMStreamConfig interface for vidcap capture"));
            }

            if (FAILED(m_graph.m_pCGB->FindInterface(&PIN_CATEGORY_CAPTURE, &MEDIATYPE_Video, m_graph.m_pVidCap, IID_PPV_ARGS(&m_graph.m_pAMVSCPrev)))) {
                TRACE(_T("Warning: No IAMStreamConfig interface for vidcap capture"));
            }
        }

        if (FAILED(m_graph.m_pCGB->FindInterface(&LOOK_UPSTREAM_ONLY, nullptr, m_graph.m_pVidCap, IID_PPV_ARGS(&m_graph.m_pAMXBar)))) {
            TRACE(_T("Warning: No IAMCrossbar interface was found\n"));
        }

        if (FAILED(m_graph.m_pCGB->FindInterface(&LOOK_UPSTREAM_ONLY, nullptr, m_graph.m_pVidCap, IID_PPV_ARGS(&m_graph.m_pAMTuner)))) {
            TRACE(_T("Warning: No IAMTVTuner interface was found\n"));
        }
        // TODO: init m_graph.m_pAMXBar

        if (m_graph.m_pAMTuner) { // load saved channel
            m_graph.m_pAMTuner->put_CountryCode(AfxGetApp()->GetProfileInt(_T("Capture"), _T("Country"), 1));

            int vchannel = pODD->vchannel;
            if (vchannel < 0) {
                vchannel = AfxGetApp()->GetProfileInt(_T("Capture\\") + CString(m_VidDispName), _T("Channel"), -1);
            }
            if (vchannel >= 0) {
                OAFilterState fs = State_Stopped;
                m_graph.m_pMC->GetState(0, &fs);
                if (fs == State_Running) {
                    m_frame.MediaControlPause(true);
                }
                m_graph.m_pAMTuner->put_Channel(vchannel, AMTUNER_SUBCHAN_DEFAULT, AMTUNER_SUBCHAN_DEFAULT);
                if (fs == State_Running) {
                    m_frame.MediaControlRun();
                }
            }
        }
    }

    if (pAudCapTmp) {
        if (FAILED(hr = m_graph.m_pGB->AddFilter(pAudCapTmp, CStringW(audfrname)))) {
            throw (UINT)IDS_CAPTURE_ERROR_AUD_FILTER;
        }

        m_graph.m_pAudCap = pAudCapTmp;

        if (FAILED(m_graph.m_pCGB->FindInterface(&PIN_CATEGORY_CAPTURE, &MEDIATYPE_Audio, m_graph.m_pAudCap, IID_PPV_ARGS(&m_graph.m_pAMASC)))
                && FAILED(m_graph.m_pCGB->FindInterface(&PIN_CATEGORY_PREVIEW, &MEDIATYPE_Audio, m_graph.m_pAudCap, IID_PPV_ARGS(&m_graph.m_pAMASC)))) {
            TRACE(_T("Warning: No IAMStreamConfig interface for vidcap"));
        }
    }

    if (!(m_graph.m_pVidCap || m_graph.m_pAudCap)) {
        throw (UINT)IDS_MAINFRM_108;
    }

    pODD->title.LoadString(IDS_CAPTURE_LIVE);

    m_frame.SetPlaybackMode(PM_ANALOG_CAPTURE);
}

void CCaptureController::OpenSetupCaptureBar()
{
    if (m_frame.GetPlaybackMode() == PM_ANALOG_CAPTURE) {
        if (m_graph.m_pVidCap && m_graph.m_pAMVSCCap) {
            CComQIPtr<IAMVfwCaptureDialogs> pVfwCD = m_graph.m_pVidCap;

            if (!m_graph.m_pAMXBar && pVfwCD) {
                m_frame.m_wndCaptureBar.m_capdlg.SetupVideoControls(m_VidDispName, m_graph.m_pAMVSCCap, pVfwCD);
            } else {
                m_frame.m_wndCaptureBar.m_capdlg.SetupVideoControls(m_VidDispName, m_graph.m_pAMVSCCap, m_graph.m_pAMXBar, m_graph.m_pAMTuner);
            }
        }

        if (m_graph.m_pAudCap && m_graph.m_pAMASC) {
            CInterfaceArray<IAMAudioInputMixer> pAMAIM;

            BeginEnumPins(m_graph.m_pAudCap, pEP, pPin) {
                if (CComQIPtr<IAMAudioInputMixer> pAIM = pPin) {
                    pAMAIM.Add(pAIM);
                }
            }
            EndEnumPins;

            m_frame.m_wndCaptureBar.m_capdlg.SetupAudioControls(m_AudDispName, m_graph.m_pAMASC, pAMAIM);
        }

        BuildGraphVideoAudio(
            m_frame.m_wndCaptureBar.m_capdlg.m_fVidPreview, false,
            m_frame.m_wndCaptureBar.m_capdlg.m_fAudPreview, false);
    }
}

static const int ATSC_FIRST_CHANNEL            = 2;
static const int ATSC_FIRST_UHF_CHANNEL        = 14;
static const int ATSC_LAST_UHF_CHANNEL         = 51;
static const int ATSC_RADIO_ASTRONOMY_CHANNEL  = 37;

// Centre frequency in kHz of a US ATSC terrestrial RF channel, or false if the
// channel number is not one that carries a broadcast.
//
// The plan is deliberately a lookup rather than arithmetic, because it is not a
// uniform raster. Stepping by bandwidth - which is what a DVB scan does, and
// what this scan used to do for every standard - is correct only within a band:
// there is a 10 MHz step between channels 4 and 5, the FM broadcast band sits
// between 6 and 7, and 260 MHz separate 13 from 14. A 6 MHz sweep therefore
// lands off-channel from channel 5 onwards and wastes some forty tuning
// attempts crossing the gap below UHF.
static bool GetATSCChannelFrequency(int nChannel, ULONG& ulFrequency)
{
    // VHF low (2-6) and VHF high (7-13) are irregular and are listed out.
    static const struct { int nChannel; ULONG ulFrequency; } VHFChannels[] = {
        {  2,  57000 }, {  3,  63000 }, {  4,  69000 },
        {  5,  79000 }, {  6,  85000 },
        {  7, 177000 }, {  8, 183000 }, {  9, 189000 }, { 10, 195000 },
        { 11, 201000 }, { 12, 207000 }, { 13, 213000 },
    };

    for (const auto& channel : VHFChannels) {
        if (channel.nChannel == nChannel) {
            ulFrequency = channel.ulFrequency;
            return true;
        }
    }

    // UHF is a regular 6 MHz raster starting at channel 14 on 473 MHz.
    // Channel 37 is reserved worldwide for radio astronomy and never carries a
    // broadcast, so receivers skip it. Above channel 36 the band was reassigned
    // to mobile use by the 2017 repack, but older recordings may still sit
    // there, so the plan runs to channel 51.
    if (nChannel >= ATSC_FIRST_UHF_CHANNEL && nChannel <= ATSC_LAST_UHF_CHANNEL && nChannel != ATSC_RADIO_ASTRONOMY_CHANNEL) {
        ulFrequency = 473000 + (nChannel - ATSC_FIRST_UHF_CHANNEL) * 6000;
        return true;
    }

    return false;
}

void CCaptureController::DoTunerScan(TunerScanData* pTSD)
{
    if (m_frame.GetPlaybackMode() == PM_DIGITAL_CAPTURE) {
        CComQIPtr<IBDATuner> pTun = m_graph.m_pGB;
        if (pTun) {
            bool wasStopped = false;
            if (m_frame.GetMediaState() == State_Stopped) {
                SetChannel(-1);
                m_frame.MediaControlRun();
                wasStopped = true;
            }

            BOOLEAN bPresent;
            BOOLEAN bLocked;
            LONG lDbStrength = 0;
            LONG lPercentQuality = 0;
            int nOffset = pTSD->Offset ? 3 : 1;
            LONG lOffsets[3] = {0, pTSD->Offset, -pTSD->Offset};
            m_bStopTunerScan = false;
            pTun->Scan(0, 0, 0, NULL);  // Clear maps

            // Work out which frequencies to visit before tuning any of them.
            // For ATSC these come from the RF channel plan, because its
            // channels are not evenly spaced; for DVB the historical fixed step
            // by bandwidth is correct. The start and stop frequencies bound the
            // scan either way, so the dialog keeps working unchanged.
            bool bIsATSC = false;
            pTun->IsATSC(bIsATSC);

            std::vector<ULONG> frequencies;
            if (bIsATSC) {
                for (int nChannel = ATSC_FIRST_CHANNEL; nChannel <= ATSC_LAST_UHF_CHANNEL; nChannel++) {
                    ULONG ulChannelFrequency;
                    if (GetATSCChannelFrequency(nChannel, ulChannelFrequency)
                            && ulChannelFrequency >= pTSD->FrequencyStart
                            && ulChannelFrequency <= pTSD->FrequencyStop) {
                        frequencies.push_back(ulChannelFrequency);
                    }
                }
            } else {
                for (ULONG ulFrequency = pTSD->FrequencyStart; ulFrequency <= pTSD->FrequencyStop; ulFrequency += pTSD->Bandwidth) {
                    frequencies.push_back(ulFrequency);
                }
            }

            for (size_t nIndex = 0; nIndex < frequencies.size(); nIndex++) {
                const ULONG ulFrequency = frequencies[nIndex];
                bool bSucceeded = false;
                for (int nOffsetPos = 0; nOffsetPos < nOffset && !bSucceeded; nOffsetPos++) {
                    if (SUCCEEDED(pTun->SetFrequency(ulFrequency + lOffsets[nOffsetPos], pTSD->Bandwidth, pTSD->SymbolRate))) {
                        Sleep(200); // Let the tuner some time to detect the signal
                        if (SUCCEEDED(pTun->GetStats(bPresent, bLocked, lDbStrength, lPercentQuality)) && bPresent) {
                            ::SendMessage(pTSD->Hwnd, WM_TUNER_STATS, lDbStrength, lPercentQuality);
                            pTun->Scan(ulFrequency + lOffsets[nOffsetPos], pTSD->Bandwidth, pTSD->SymbolRate, pTSD->Hwnd);
                            bSucceeded = true;
                        }
                    }
                }

                // Steps are uneven under a channel plan, so progress counts
                // frequencies visited rather than distance travelled up the band.
                int nProgress = MulDiv((int)nIndex + 1, 100, (int)frequencies.size());
                ::SendMessage(pTSD->Hwnd, WM_TUNER_SCAN_PROGRESS, nProgress, 0);
                ::SendMessage(pTSD->Hwnd, WM_TUNER_STATS, lDbStrength, lPercentQuality);

                if (m_bStopTunerScan) {
                    break;
                }
            }

            ::SendMessage(pTSD->Hwnd, WM_TUNER_SCAN_END, 0, 0);
            if (wasStopped) {
                SetChannel(AfxGetAppSettings().nDVBLastChannel);
                m_frame.MediaControlStop();
            }
        }
    }
}

#define AUDIOBUFFERLEN 500

static void SetLatency(IBaseFilter* pBF, int cbBuffer)
{
    BeginEnumPins(pBF, pEP, pPin) {
        if (CComQIPtr<IAMBufferNegotiation> pAMBN = pPin) {
            ALLOCATOR_PROPERTIES ap;
            ap.cbAlign = -1;  // -1 means no preference.
            ap.cbBuffer = cbBuffer;
            ap.cbPrefix = -1;
            ap.cBuffers = -1;
            pAMBN->SuggestAllocatorProperties(&ap);
        }
    }
    EndEnumPins;
}

HRESULT CCaptureController::BuildCapture(IPin* pPin, IBaseFilter* pBF[3], const GUID& majortype, AM_MEDIA_TYPE* pmt)
{
    IBaseFilter* pBuff = pBF[0];
    IBaseFilter* pEnc = pBF[1];
    IBaseFilter* pMux = pBF[2];

    if (!pPin || !pMux) {
        return E_FAIL;
    }

    CString err;
    HRESULT hr = S_OK;
    CFilterInfo fi;

    if (FAILED(pMux->QueryFilterInfo(&fi)) || !fi.pGraph) {
        m_graph.m_pGB->AddFilter(pMux, L"Multiplexer");
    }

    CStringW prefix;
    CString type;
    if (majortype == MEDIATYPE_Video) {
        prefix = L"Video ";
        type.LoadString(IDS_CAPTURE_ERROR_VIDEO);
    } else if (majortype == MEDIATYPE_Audio) {
        prefix = L"Audio ";
        type.LoadString(IDS_CAPTURE_ERROR_AUDIO);
    }

    if (pBuff) {
        hr = m_graph.m_pGB->AddFilter(pBuff, prefix + L"Buffer");
        if (FAILED(hr)) {
            err.Format(IDS_CAPTURE_ERROR_ADD_BUFFER, type.GetString());
            m_frame.MessageBox(err, ResStr(IDS_CAPTURE_ERROR), MB_ICONERROR | MB_OK);
            return hr;
        }

        hr = m_graph.m_pGB->ConnectFilter(pPin, pBuff);
        if (FAILED(hr)) {
            err.Format(IDS_CAPTURE_ERROR_CONNECT_BUFF, type.GetString());
            m_frame.MessageBox(err, ResStr(IDS_CAPTURE_ERROR), MB_ICONERROR | MB_OK);
            return hr;
        }

        pPin = GetFirstPin(pBuff, PINDIR_OUTPUT);
    }

    if (pEnc) {
        hr = m_graph.m_pGB->AddFilter(pEnc, prefix + L"Encoder");
        if (FAILED(hr)) {
            err.Format(IDS_CAPTURE_ERROR_ADD_ENCODER, type.GetString());
            m_frame.MessageBox(err, ResStr(IDS_CAPTURE_ERROR), MB_ICONERROR | MB_OK);
            return hr;
        }

        hr = m_graph.m_pGB->ConnectFilter(pPin, pEnc);
        if (FAILED(hr)) {
            err.Format(IDS_CAPTURE_ERROR_CONNECT_ENC, type.GetString());
            m_frame.MessageBox(err, ResStr(IDS_CAPTURE_ERROR), MB_ICONERROR | MB_OK);
            return hr;
        }

        pPin = GetFirstPin(pEnc, PINDIR_OUTPUT);

        if (CComQIPtr<IAMStreamConfig> pAMSC = pPin) {
            if (pmt->majortype == majortype) {
                hr = pAMSC->SetFormat(pmt);
                if (FAILED(hr)) {
                    err.Format(IDS_CAPTURE_ERROR_COMPRESSION, type.GetString());
                    m_frame.MessageBox(err, ResStr(IDS_CAPTURE_ERROR), MB_ICONERROR | MB_OK);
                    return hr;
                }
            }
        }

    }

    //if (pMux)
    {
        hr = m_graph.m_pGB->ConnectFilter(pPin, pMux);
        if (FAILED(hr)) {
            err.Format(IDS_CAPTURE_ERROR_MULTIPLEXER, type.GetString());
            m_frame.MessageBox(err, ResStr(IDS_CAPTURE_ERROR), MB_ICONERROR | MB_OK);
            return hr;
        }
    }

    m_frame.CleanGraph();

    return S_OK;
}

bool CCaptureController::BuildToCapturePreviewPin(
    IBaseFilter* pVidCap, IPin** ppVidCapPin, IPin** ppVidPrevPin,
    IBaseFilter* pAudCap, IPin** ppAudCapPin, IPin** ppAudPrevPin)
{
    HRESULT hr;
    *ppVidCapPin = *ppVidPrevPin = nullptr;
    *ppAudCapPin = *ppAudPrevPin = nullptr;
    CComPtr<IPin> pDVAudPin;

    if (pVidCap) {
        CComPtr<IPin> pPin;
        if (!pAudCap // only look for interleaved stream when we don't use any other audio capture source
                && SUCCEEDED(m_graph.m_pCGB->FindPin(pVidCap, PINDIR_OUTPUT, &PIN_CATEGORY_CAPTURE, &MEDIATYPE_Interleaved, TRUE, 0, &pPin))) {
            CComPtr<IBaseFilter> pDVSplitter;
            hr = pDVSplitter.CoCreateInstance(CLSID_DVSplitter);
            hr = m_graph.m_pGB->AddFilter(pDVSplitter, L"DV Splitter");

            hr = m_graph.m_pCGB->RenderStream(nullptr, &MEDIATYPE_Interleaved, pPin, nullptr, pDVSplitter);

            pPin = nullptr;
            hr = m_graph.m_pCGB->FindPin(pDVSplitter, PINDIR_OUTPUT, nullptr, &MEDIATYPE_Video, TRUE, 0, &pPin);
            hr = m_graph.m_pCGB->FindPin(pDVSplitter, PINDIR_OUTPUT, nullptr, &MEDIATYPE_Audio, TRUE, 0, &pDVAudPin);

            CComPtr<IBaseFilter> pDVDec;
            hr = pDVDec.CoCreateInstance(CLSID_DVVideoCodec);
            hr = m_graph.m_pGB->AddFilter(pDVDec, L"DV Video Decoder");

            hr = m_graph.m_pGB->ConnectFilter(pPin, pDVDec);

            pPin = nullptr;
            hr = m_graph.m_pCGB->FindPin(pDVDec, PINDIR_OUTPUT, nullptr, &MEDIATYPE_Video, TRUE, 0, &pPin);
        } else if (FAILED(m_graph.m_pCGB->FindPin(pVidCap, PINDIR_OUTPUT, &PIN_CATEGORY_CAPTURE, &MEDIATYPE_Video, TRUE, 0, &pPin))) {
            m_frame.MessageBox(ResStr(IDS_CAPTURE_ERROR_VID_CAPT_PIN), ResStr(IDS_CAPTURE_ERROR), MB_ICONERROR | MB_OK);
            return false;
        }

        CComPtr<IBaseFilter> pSmartTee;
        hr = pSmartTee.CoCreateInstance(CLSID_SmartTee);
        hr = m_graph.m_pGB->AddFilter(pSmartTee, L"Smart Tee (video)");

        hr = m_graph.m_pGB->ConnectFilter(pPin, pSmartTee);

        hr = pSmartTee->FindPin(L"Preview", ppVidPrevPin);
        hr = pSmartTee->FindPin(L"Capture", ppVidCapPin);
    }

    if (pAudCap || pDVAudPin) {
        CComPtr<IPin> pPin;
        if (pDVAudPin) {
            pPin = pDVAudPin;
        } else if (FAILED(m_graph.m_pCGB->FindPin(pAudCap, PINDIR_OUTPUT, &PIN_CATEGORY_CAPTURE, &MEDIATYPE_Audio, TRUE, 0, &pPin))) {
            m_frame.MessageBox(ResStr(IDS_CAPTURE_ERROR_AUD_CAPT_PIN), ResStr(IDS_CAPTURE_ERROR), MB_ICONERROR | MB_OK);
            return false;
        }

        CComPtr<IBaseFilter> pSmartTee;
        hr = pSmartTee.CoCreateInstance(CLSID_SmartTee);
        hr = m_graph.m_pGB->AddFilter(pSmartTee, L"Smart Tee (audio)");

        hr = m_graph.m_pGB->ConnectFilter(pPin, pSmartTee);

        hr = pSmartTee->FindPin(L"Preview", ppAudPrevPin);
        hr = pSmartTee->FindPin(L"Capture", ppAudCapPin);
    }

    return true;
}

bool CCaptureController::BuildGraphVideoAudio(int fVPreview, bool fVCapture, int fAPreview, bool fACapture)
{
    OAFilterState fs = m_frame.GetMediaState();

    if (fs != State_Stopped) {
        m_frame.SendMessage(WM_COMMAND, ID_PLAY_STOP);
    }

    HRESULT hr;

    if (fVPreview) {
        m_frame.m_OSD.Stop();

        m_graph.m_pMVRS.Release();
        m_graph.m_pMVRFG.Release();
        m_graph.m_pMVRSR.Release();
        m_graph.m_pMVRC.Release();
        m_graph.m_pMVRI.Release();
        m_graph.m_pMVTO.Release();
        m_graph.m_pMPCVRSR.Release();
        m_graph.m_pD3DFSC.Release();
        m_graph.m_pCAP3.Release();
        m_graph.m_pCAP2.Release();
        m_graph.m_pCAP.Release();
        m_graph.m_pVMRWC.Release();
        m_graph.m_pVMRMC.Release();
        m_graph.m_pVMB.Release();
        m_graph.m_pMFVMB.Release();
        m_graph.m_pMFVP.Release();
        m_graph.m_pMFVDC.Release();
        m_graph.m_pQP.Release();
    }

    m_graph.m_pGB->NukeDownstream(m_graph.m_pVidCap);
    m_graph.m_pGB->NukeDownstream(m_graph.m_pAudCap);

    m_frame.CleanGraph();

    if (m_graph.m_pAMVSCCap) {
        hr = m_graph.m_pAMVSCCap->SetFormat(&m_frame.m_wndCaptureBar.m_capdlg.m_mtv);
    }
    if (m_graph.m_pAMVSCPrev) {
        hr = m_graph.m_pAMVSCPrev->SetFormat(&m_frame.m_wndCaptureBar.m_capdlg.m_mtv);
    }
    if (m_graph.m_pAMASC) {
        hr = m_graph.m_pAMASC->SetFormat(&m_frame.m_wndCaptureBar.m_capdlg.m_mta);
    }

    CComPtr<IBaseFilter> pVidBuffer = m_frame.m_wndCaptureBar.m_capdlg.m_pVidBuffer;
    CComPtr<IBaseFilter> pAudBuffer = m_frame.m_wndCaptureBar.m_capdlg.m_pAudBuffer;
    CComPtr<IBaseFilter> pVidEnc = m_frame.m_wndCaptureBar.m_capdlg.m_pVidEnc;
    CComPtr<IBaseFilter> pAudEnc = m_frame.m_wndCaptureBar.m_capdlg.m_pAudEnc;
    CComPtr<IBaseFilter> pMux = m_frame.m_wndCaptureBar.m_capdlg.m_pMux;
    CComPtr<IBaseFilter> pDst = m_frame.m_wndCaptureBar.m_capdlg.m_pDst;
    CComPtr<IBaseFilter> pAudMux = m_frame.m_wndCaptureBar.m_capdlg.m_pAudMux;
    CComPtr<IBaseFilter> pAudDst = m_frame.m_wndCaptureBar.m_capdlg.m_pAudDst;

    bool fFileOutput = (pMux && pDst) || (pAudMux && pAudDst);
    bool fCapture = (fVCapture || fACapture);

    if (m_graph.m_pAudCap) {
        AM_MEDIA_TYPE* pmt = &m_frame.m_wndCaptureBar.m_capdlg.m_mta;
        int ms = (fACapture && fFileOutput && m_frame.m_wndCaptureBar.m_capdlg.m_fAudOutput) ? AUDIOBUFFERLEN : 60;
        if (pMux != pAudMux && fACapture) {
            SetLatency(m_graph.m_pAudCap, -1);
        } else if (pmt->pbFormat) {
            SetLatency(m_graph.m_pAudCap, ((WAVEFORMATEX*)pmt->pbFormat)->nAvgBytesPerSec * ms / 1000);
        }
    }

    CComPtr<IPin> pVidCapPin, pVidPrevPin, pAudCapPin, pAudPrevPin;
    BuildToCapturePreviewPin(m_graph.m_pVidCap, &pVidCapPin, &pVidPrevPin, m_graph.m_pAudCap, &pAudCapPin, &pAudPrevPin);

    bool fVidPrev = pVidPrevPin && fVPreview;
    bool fVidCap = pVidCapPin && fVCapture && fFileOutput && m_frame.m_wndCaptureBar.m_capdlg.m_fVidOutput;

    if (fVPreview == 2 && !fVidCap && pVidCapPin) {
        pVidPrevPin = pVidCapPin;
        pVidCapPin = nullptr;
    }

    bool fAudPrev = pAudPrevPin && fAPreview;
    bool fAudCap = pAudCapPin && fACapture && fFileOutput && m_frame.m_wndCaptureBar.m_capdlg.m_fAudOutput;

    if (fAPreview == 2 && !fAudCap && pAudCapPin) {
        pAudPrevPin = pAudCapPin;
        pAudCapPin = nullptr;
    }

    // Render both previews before BuildCapture adds the mux to the graph.
    // CFGManager::Connect prefers filters already in the graph and would
    // otherwise connect a preview pin to a spare input of the mux.
    if (fVidPrev) {
        m_graph.m_pGB->Render(pVidPrevPin);

        m_graph.m_pGB->FindInterface(IID_PPV_ARGS(&m_graph.m_pCAP), TRUE);
        m_graph.m_pGB->FindInterface(IID_PPV_ARGS(&m_graph.m_pCAP2), TRUE);
        m_graph.m_pGB->FindInterface(IID_PPV_ARGS(&m_graph.m_pCAP3), TRUE);
        m_graph.m_pGB->FindInterface(IID_PPV_ARGS(&m_graph.m_pVMRWC), FALSE);
        m_graph.m_pGB->FindInterface(IID_PPV_ARGS(&m_graph.m_pVMRMC), TRUE);
        m_graph.m_pGB->FindInterface(IID_PPV_ARGS(&m_graph.m_pVMB), TRUE);
        m_graph.m_pGB->FindInterface(IID_PPV_ARGS(&m_graph.m_pMFVMB), TRUE);
        m_graph.m_pGB->FindInterface(IID_PPV_ARGS(&m_graph.m_pMFVDC), TRUE);
        m_graph.m_pGB->FindInterface(IID_PPV_ARGS(&m_graph.m_pMFVP), TRUE);
        m_graph.m_pMVTO = m_graph.m_pCAP;
        m_graph.m_pMVRSR = m_graph.m_pCAP;
        m_graph.m_pMVRS = m_graph.m_pCAP;
        m_graph.m_pMVRFG = m_graph.m_pCAP;
        m_graph.m_pMPCVRSR = m_graph.m_pCAP;
        m_graph.m_pMVRC = m_graph.m_pCAP;
        m_graph.m_pMVRI = m_graph.m_pCAP;
        m_graph.m_pD3DFSC = m_graph.m_pCAP;

        const CAppSettings& s = AfxGetAppSettings();
        m_frame.m_pVideoWnd = &m_frame.m_wndView;

        if (m_graph.m_pMFVDC) {
            m_graph.m_pMFVDC->SetVideoWindow(m_frame.m_pVideoWnd->m_hWnd);
        } else if (m_graph.m_pVMRWC) {
            m_graph.m_pVMRWC->SetVideoClippingWindow(m_frame.m_pVideoWnd->m_hWnd);
        }

        if (s.fShowOSD || s.fShowDebugInfo) { // Force OSD on when the debug switch is used
            if (m_graph.m_pMVTO) {
                m_frame.m_OSD.Start(m_frame.m_pVideoWnd, m_graph.m_pMVTO);
            } else if (m_frame.m_fFullScreen && !m_frame.m_fAudioOnly && m_graph.m_pCAP3) { // MPCVR
                m_frame.m_OSD.Start(m_frame.m_pVideoWnd, m_graph.m_pVMB, m_graph.m_pMFVMB, false);
            } else if (!m_frame.m_fAudioOnly && m_frame.IsD3DFullScreenMode() && (m_graph.m_pVMB || m_graph.m_pMFVMB)) {
                m_frame.m_OSD.Start(m_frame.m_pVideoWnd, m_graph.m_pVMB, m_graph.m_pMFVMB, true);
            } else {
                m_frame.m_OSD.Start(m_frame.m_pOSDWnd);
            }
        }
    }

    if (fAudPrev) {
        m_graph.m_pGB->Render(pAudPrevPin);
    }

    if (fVidCap) {
        IBaseFilter* pBF[3] = {pVidBuffer, pVidEnc, pMux};
        HRESULT hr2 = BuildCapture(pVidCapPin, pBF, MEDIATYPE_Video, &m_frame.m_wndCaptureBar.m_capdlg.m_mtcv);
        UNREFERENCED_PARAMETER(hr2);
    }

    m_graph.m_pAMDF.Release();
    if (m_graph.m_pCGB && FAILED(m_graph.m_pCGB->FindInterface(&PIN_CATEGORY_CAPTURE, &MEDIATYPE_Video, m_graph.m_pVidCap, IID_PPV_ARGS(&m_graph.m_pAMDF)))) {
        TRACE(_T("Warning: No IAMDroppedFrames interface for vidcap capture"));
    }

    if (fAudCap) {
        IBaseFilter* pBF[3] = {pAudBuffer, pAudEnc, pAudMux ? pAudMux : pMux};
        HRESULT hr2 = BuildCapture(pAudCapPin, pBF, MEDIATYPE_Audio, &m_frame.m_wndCaptureBar.m_capdlg.m_mtca);
        UNREFERENCED_PARAMETER(hr2);
    }

    if ((m_graph.m_pVidCap || m_graph.m_pAudCap) && fCapture && fFileOutput) {
        if (pMux != pDst) {
            hr = m_graph.m_pGB->AddFilter(pDst, L"File Writer V/A");
            hr = m_graph.m_pGB->ConnectFilter(GetFirstPin(pMux, PINDIR_OUTPUT), pDst);
        }

        if (CComQIPtr<IConfigAviMux> pCAM = pMux) {
            int nIn, nOut, nInC, nOutC;
            CountPins(pMux, nIn, nOut, nInC, nOutC);
            pCAM->SetMasterStream(nInC - 1);
            //pCAM->SetMasterStream(-1);
            pCAM->SetOutputCompatibilityIndex(FALSE);
        }

        if (CComQIPtr<IConfigInterleaving> pCI = pMux) {
            //if (FAILED(pCI->put_Mode(INTERLEAVE_CAPTURE)))
            if (FAILED(pCI->put_Mode(INTERLEAVE_NONE_BUFFERED))) {
                pCI->put_Mode(INTERLEAVE_NONE);
            }

            REFERENCE_TIME rtInterleave = 10000i64 * AUDIOBUFFERLEN, rtPreroll = 0; //10000i64*500
            pCI->put_Interleaving(&rtInterleave, &rtPreroll);
        }

        if (pMux != pAudMux && pAudMux != pAudDst) {
            hr = m_graph.m_pGB->AddFilter(pAudDst, L"File Writer A");
            hr = m_graph.m_pGB->ConnectFilter(GetFirstPin(pAudMux, PINDIR_OUTPUT), pAudDst);
        }
    }

    REFERENCE_TIME stop = MAX_TIME;
    if (m_graph.m_pCGB) {
        hr = m_graph.m_pCGB->ControlStream(&PIN_CATEGORY_CAPTURE, nullptr, nullptr, nullptr, &stop, 0, 0); // stop in the infinite
    }

    m_frame.CleanGraph();

    m_frame.OpenSetupVideo();
    m_frame.OpenSetupAudio();
    m_frame.OpenSetupStatsBar();
    m_frame.OpenSetupStatusBar();
    m_frame.RecalcLayout();

    m_frame.SetupVMR9ColorControl();

    if (m_frame.GetLoadState() == MLS::LOADED) {
        if (fs == State_Running) {
            m_frame.SendMessage(WM_COMMAND, ID_PLAY_PLAY);
        } else if (fs == State_Paused) {
            m_frame.SendMessage(WM_COMMAND, ID_PLAY_PAUSE);
        }
    }

    return true;
}

bool CCaptureController::StartCapture()
{
    if (!m_graph.m_pCGB || m_fCapturing) {
        return false;
    }

    if (!m_frame.m_wndCaptureBar.m_capdlg.m_pMux && !m_frame.m_wndCaptureBar.m_capdlg.m_pDst) {
        return false;
    }

    HRESULT hr;

    ::SetPriorityClass(::GetCurrentProcess(), HIGH_PRIORITY_CLASS);

    // rare to see two capture filters to support IAMPushSource at the same time...
    //hr = CComQIPtr<IAMGraphStreams>(m_graph.m_pGB)->SyncUsingStreamOffset(TRUE); // TODO:

    BuildGraphVideoAudio(
        m_frame.m_wndCaptureBar.m_capdlg.m_fVidPreview, true,
        m_frame.m_wndCaptureBar.m_capdlg.m_fAudPreview, true);

    hr = m_graph.m_pME->CancelDefaultHandling(EC_REPAINT);
    m_frame.SendMessage(WM_COMMAND, ID_PLAY_PLAY);
    m_fCapturing = true;

    return true;
}

bool CCaptureController::StopCapture()
{
    if (!m_graph.m_pCGB || !m_fCapturing) {
        return false;
    }

    if (!m_frame.m_wndCaptureBar.m_capdlg.m_pMux && !m_frame.m_wndCaptureBar.m_capdlg.m_pDst) {
        return false;
    }

    m_frame.m_wndStatusBar.SetStatusMessage(StrRes(IDS_CONTROLS_COMPLETING));
    m_fCapturing = false;

    BuildGraphVideoAudio(
        m_frame.m_wndCaptureBar.m_capdlg.m_fVidPreview, false,
        m_frame.m_wndCaptureBar.m_capdlg.m_fAudPreview, false);

    m_graph.m_pME->RestoreDefaultHandling(EC_REPAINT);

    ::SetPriorityClass(::GetCurrentProcess(), AfxGetAppSettings().dwPriority);

    m_frame.m_rtDurationOverride = -1;

    return true;
}

void CCaptureController::AddCurDevToPlaylist()
{
    if (m_frame.GetPlaybackMode() == PM_ANALOG_CAPTURE) {
        m_frame.m_wndPlaylistBar.Append(
            m_VidDispName,
            m_AudDispName,
            m_frame.m_wndCaptureBar.m_capdlg.GetVideoInput(),
            m_frame.m_wndCaptureBar.m_capdlg.GetVideoChannel(),
            m_frame.m_wndCaptureBar.m_capdlg.GetAudioInput()
        );
    }
}

void CCaptureController::StartTunerScan(CAutoPtr<TunerScanData> pTSD)
{
    // Remove the old info during the scan
    if (m_pDVBState) {
        m_pDVBState->Reset();
    }
    m_frame.m_wndInfoBar.RemoveAllLines();
    m_frame.m_wndNavigationBar.m_navdlg.SetChannelInfoAvailable(false);
    m_frame.RecalcLayout();
    m_frame.OpenSetupWindowTitle();

    if (m_frame.m_pGraphThread && m_frame.m_pGraphThread->m_hThread) {
        m_frame.m_pGraphThread->PostThreadMessage(CGraphThread::TM_TUNER_SCAN, (WPARAM)0, (LPARAM)pTSD.Detach());
    } else {
        DoTunerScan(pTSD);
    }
}

void CCaptureController::StopTunerScan()
{
    m_bStopTunerScan = true;
}

void CCaptureController::StartHeadlessDVBScan()
{
    const CAppSettings& s = AfxGetAppSettings();

    m_headlessDVBScanChannels.clear();

    CAutoPtr<TunerScanData> pTSD(DEBUG_NEW TunerScanData);
    pTSD->Hwnd = m_frame.m_hWnd;
    pTSD->FrequencyStart = s.cmdlnDVBScan.ulFrequencyStart;
    pTSD->FrequencyStop = s.cmdlnDVBScan.ulFrequencyStop;
    // Bandwidth is kHz in TunerScanData and MHz in the profile. This is the
    // same conversion CTunerScanDlg makes when it loads its fields, so a
    // headless run and a dialog run scan identically for identical settings.
    pTSD->Bandwidth = s.cmdlnDVBScan.ulBandwidth ? s.cmdlnDVBScan.ulBandwidth
                      : (ULONG)s.iBDABandwidth * 1000;
    pTSD->SymbolRate = s.cmdlnDVBScan.ulSymbolRate ? s.cmdlnDVBScan.ulSymbolRate
                       : (ULONG)s.iBDASymbolRate;
    pTSD->Offset = s.fBDAUseOffset ? s.iBDAOffset : 0;

    StartTunerScan(pTSD);
}

LRESULT CCaptureController::OnHeadlessScanNewChannel(WPARAM wParam, LPARAM lParam)
{
    if (!m_bHeadlessDVBScan) {
        return FALSE;
    }

    const CAppSettings& s = AfxGetAppSettings();
    const size_t maxChannelsNum = ID_NAVIGATE_JUMPTO_SUBITEM_END - ID_NAVIGATE_JUMPTO_SUBITEM_START + 1;

    try {
        CBDAChannel channel((LPCTSTR)lParam);
        // The dialog applies this filter as it fills its list rather than at
        // save time, so apply it here too.
        if (!s.fBDAIgnoreEncryptedChannels || !channel.IsEncrypted()) {
            if (m_headlessDVBScanChannels.size() < maxChannelsNum) {
                channel.SetPrefNumber((int)m_headlessDVBScanChannels.size());
                m_headlessDVBScanChannels.push_back(channel);
            }
        }
    } catch (CException* e) {
        // A record that will not tokenise is dropped and the scan continues,
        // which is what CTunerScanDlg::OnNewChannel does with the same failure.
        TRACE(_T("/dvbscan: failed to parse a scanned channel record\n"));
        e->Delete();
    }

    return TRUE;
}

LRESULT CCaptureController::OnHeadlessScanEnd(WPARAM wParam, LPARAM lParam)
{
    if (!m_bHeadlessDVBScan) {
        return FALSE;
    }
    FinishHeadlessDVBScan();
    return TRUE;
}

void CCaptureController::FinishHeadlessDVBScan()
{
    CAppSettings& s = AfxGetAppSettings();

    // The serializer the web interface uses, not a second copy of it: whatever
    // /dvb/channels.json would say about these channels, this file says too.
    const CStringA json = DVBChannelsToJSON(m_headlessDVBScanChannels);

    bool bWritten = false;
    if (!s.cmdlnDVBScan.strOutputPath.IsEmpty()) {
        CFile file;
        CFileException fe;
        if (file.Open(s.cmdlnDVBScan.strOutputPath,
                      CFile::modeCreate | CFile::modeWrite | CFile::typeBinary, &fe)) {
            try {
                file.Write((LPCSTR)json, json.GetLength());
                bWritten = true;
            } catch (CFileException* pfe) {
                pfe->Delete();
            }
            file.Close();
        }
    }

    if (!bWritten) {
        // Losing the result silently would leave a caller unable to tell an
        // empty scan from an unwritable path.
        TRACE(_T("/dvbscan: could not write the result to '%s'\n"),
              s.cmdlnDVBScan.strOutputPath.GetString());
    }

    // /dvbscansave: the scan result becomes the saved channel list, so that a
    // following /device run has something to tune. It replaces the list rather
    // than merging into it the way CTunerScanDlg::OnBnClickedSave does, because
    // a headless run should give the same result whatever was saved before. The
    // pref numbers are already 0..n-1 in scan order, and the first channel is
    // made the last watched one, which is the one /device tunes. An empty scan
    // leaves the saved list alone. Nothing points into the old list here:
    // StartTunerScan reset the DVB state. The profile itself is written by
    // SaveSettings on the way out, like every other setting.
    if (s.cmdlnDVBScan.bSaveChannels && !m_headlessDVBScanChannels.empty()) {
        s.m_DVBChannels = m_headlessDVBScanChannels;
        s.nDVBLastChannel = s.m_DVBChannels.front().GetPrefNumber();
    }

    m_bHeadlessDVBScan = false;
    m_frame.PostMessage(WM_CLOSE);
}

HRESULT CCaptureController::SetChannel(int nChannel)
{
    CAppSettings& s = AfxGetAppSettings();
    HRESULT hr = S_OK;
    CComQIPtr<IBDATuner> pTun = m_graph.m_pGB;
    CBDAChannel* pChannel = s.FindChannelByPref(nChannel);

    if (s.m_DVBChannels.empty() && nChannel == INT_ERROR) {
        hr = S_FALSE; // All channels have been cleared or it is the first start
    } else if (pTun && pChannel && !m_pDVBState->bSetChannelActive) {
        m_pDVBState->Reset();
        m_frame.m_wndInfoBar.RemoveAllLines();
        m_frame.m_wndNavigationBar.m_navdlg.SetChannelInfoAvailable(false);
        m_frame.RecalcLayout();
        m_pDVBState->bSetChannelActive = true;

        // Skip n intermediate ZoomVideoWindow() calls while the new size is stabilized:
        switch (s.iDSVideoRendererType) {
            case VIDRNDT_DS_MADVR:
                if (s.nDVBStopFilterGraph == DVB_STOP_FG_ALWAYS) {
                    m_frame.m_nLockedZoomVideoWindow = 3;
                } else {
                    m_frame.m_nLockedZoomVideoWindow = 0;
                }
                break;
            case VIDRNDT_DS_EVR_CUSTOM:
                m_frame.m_nLockedZoomVideoWindow = 0;
                break;
            default:
                m_frame.m_nLockedZoomVideoWindow = 0;
        }
        if (SUCCEEDED(hr = pTun->SetChannel(nChannel))) {
            if (hr == S_FALSE) {
                // Re-create all
                m_frame.m_nLockedZoomVideoWindow = 0;
                m_frame.PostMessage(WM_COMMAND, ID_FILE_OPENDEVICE);
                return hr;
            }

            m_pDVBState->bActive = true;
            m_pDVBState->pChannel = pChannel;
            m_pDVBState->sChannelName = pChannel->GetName();

            m_frame.m_wndInfoBar.SetLine(ResStr(IDS_INFOBAR_CHANNEL), m_pDVBState->sChannelName);
            m_frame.RecalcLayout();

            if (s.fRememberZoomLevel && !(m_frame.m_fFullScreen || m_frame.IsZoomed() || m_frame.IsIconic())) {
                m_frame.ZoomVideoWindow();
            }
            m_frame.MoveVideoWindow();

            // Add temporary flag to allow EC_VIDEO_SIZE_CHANGED event to stabilize window size
            // for 5 seconds since playback starts
            m_frame.m_bAllowWindowZoom = true;
            m_frame.m_timerOneTime.Subscribe(CMainFrame::TimerOneTimeSubscriber::AUTOFIT_TIMEOUT, [this]
            { m_frame.m_bAllowWindowZoom = false; }, 5000);

            UpdateCurrentChannelInfo();
        }
        m_pDVBState->bSetChannelActive = false;
    } else {
        hr = E_FAIL;
        ASSERT(FALSE);
    }

    return hr;
}

void CCaptureController::UpdateCurrentChannelInfo(bool bShowOSD /*= true*/, bool bShowInfoBar /*= false*/)
{
    const CBDAChannel* pChannel = m_pDVBState->pChannel;
    CComQIPtr<IBDATuner> pTun = m_graph.m_pGB;

    if (!m_pDVBState->bInfoActive && pChannel && pTun) {
        if (m_pDVBState->infoData.valid()) {
            m_pDVBState->bAbortInfo = true;
            m_pDVBState->infoData.get();
        }
        m_pDVBState->bAbortInfo = false;
        m_pDVBState->bInfoActive = true;
        m_pDVBState->infoData = std::async(std::launch::async, [this, pChannel, pTun, bShowOSD, bShowInfoBar] {
            DVBState::EITData infoData;
            infoData.hr = pTun->UpdatePSI(pChannel, infoData.NowNext);
            infoData.bShowOSD = bShowOSD;
            infoData.bShowInfoBar = bShowInfoBar;
            if (m_pDVBState && !m_pDVBState->bAbortInfo)
            {
                m_frame.PostMessage(WM_DVB_EIT_DATA_READY);
            }
            return infoData;
        });
    }
}

LRESULT CCaptureController::OnCurrentChannelInfoUpdated(WPARAM wParam, LPARAM lParam)
{
    if (m_frame.GetLoadState() != MLS::LOADED) {
        return 0;
    }

    if (!m_pDVBState->bAbortInfo && m_pDVBState->infoData.valid()) {
        EventDescriptor& NowNext = m_pDVBState->NowNext;
        const auto infoData = m_pDVBState->infoData.get();
        NowNext = infoData.NowNext;

        if (infoData.hr != S_FALSE) {
            // Set a timer to update the infos only if channel has now/next flag
            time_t tNow;
            time(&tNow);
            time_t tElapse = NowNext.duration - (tNow - NowNext.startTime);
            if (tElapse < 0) {
                tElapse = 0;
            }
            // We set a 15s delay to let some room for the program infos to change
            tElapse += 15;
            m_frame.m_timerOneTime.Subscribe(CMainFrame::TimerOneTimeSubscriber::DVBINFO_UPDATE,
                                     [this] { UpdateCurrentChannelInfo(false, false); },
                                     1000 * (UINT)tElapse);
            m_frame.m_wndNavigationBar.m_navdlg.SetChannelInfoAvailable(true);
        } else {
            m_frame.m_wndNavigationBar.m_navdlg.SetChannelInfoAvailable(false);
        }

        CString sChannelInfo = m_pDVBState->sChannelName;
        m_frame.m_wndInfoBar.RemoveAllLines();
        m_frame.m_wndInfoBar.SetLine(StrRes(IDS_INFOBAR_CHANNEL), sChannelInfo);

        if (infoData.hr == S_OK) {
            // EIT information parsed correctly
            if (infoData.bShowOSD) {
                sChannelInfo.AppendFormat(_T(" | %s (%s - %s)"), NowNext.eventName.GetString(), NowNext.strStartTime.GetString(), NowNext.strEndTime.GetString());
            }

            m_frame.m_wndInfoBar.SetLine(StrRes(IDS_INFOBAR_TITLE), NowNext.eventName);
            m_frame.m_wndInfoBar.SetLine(StrRes(IDS_INFOBAR_TIME), NowNext.strStartTime + _T(" - ") + NowNext.strEndTime);

            if (NowNext.parentalRating >= 0) {
                CString parentRating;
                if (!NowNext.parentalRating) {
                    parentRating.LoadString(IDS_NO_PARENTAL_RATING);
                } else {
                    parentRating.Format(IDS_PARENTAL_RATING, NowNext.parentalRating);
                }
                m_frame.m_wndInfoBar.SetLine(StrRes(IDS_INFOBAR_PARENTAL_RATING), parentRating);
            }

            if (!NowNext.content.IsEmpty()) {
                m_frame.m_wndInfoBar.SetLine(StrRes(IDS_INFOBAR_CONTENT), NowNext.content);
            }

            CString description = NowNext.eventDesc;
            if (!NowNext.extendedDescriptorsText.IsEmpty()) {
                if (!description.IsEmpty()) {
                    description += _T("; ");
                }
                description += NowNext.extendedDescriptorsText;
            }
            m_frame.m_wndInfoBar.SetLine(StrRes(IDS_INFOBAR_DESCRIPTION), description);

            for (const auto& item : NowNext.extendedDescriptorsItems) {
                m_frame.m_wndInfoBar.SetLine(item.first, item.second);
            }

            if (infoData.bShowInfoBar && !m_frame.m_controls.ControlChecked(CMainFrameControls::Toolbar::INFO)) {
                m_frame.m_controls.ToggleControl(CMainFrameControls::Toolbar::INFO);
            }
        }

        m_frame.RecalcLayout();
        if (infoData.bShowOSD) {
            m_frame.m_OSD.DisplayMessage(OSD_TOPLEFT, sChannelInfo, 3500);
        }

        m_frame.OpenSetupWindowTitle();
    } else {
        ASSERT(FALSE);
    }

    m_pDVBState->bInfoActive = false;

    return 0;
}

CString CCaptureController::GetCaptureTitle()
{
    CString title;

    title.LoadString(IDS_CAPTURE_LIVE);
    if (m_frame.GetPlaybackMode() == PM_ANALOG_CAPTURE) {
        CString devName = GetFriendlyName(m_VidDispName);
        if (!devName.IsEmpty()) {
            title.AppendFormat(_T(" | %s"), devName.GetString());
        }
    } else {
        CString& eventName = m_pDVBState->NowNext.eventName;
        if (m_pDVBState->bActive) {
            title.AppendFormat(_T(" | %s"), m_pDVBState->sChannelName.GetString());
            if (!eventName.IsEmpty()) {
                title.AppendFormat(_T(" - %s"), eventName.GetString());
            }
        } else {
            title += _T(" | DVB");
        }
    }
    return title;
}
