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

#include <qnetwork.h>
#include <mvrInterfaces.h>
#include "IBufferInfo.h"
#include "IKeyFrameInfo.h"
#include "AllocatorCommon.h"

interface IGraphBuilder2;
interface IMFVideoDisplayControl;
interface IMFVideoProcessor;
interface IMadVRCommand;
interface IMadVRInfo;
interface IMadVRFrameGrabber;
interface IMadVRSettings;
interface IMadVRSubclassReplacement;
interface IMPCVRSubclassReplacement;
interface ISubPicAllocatorPresenter2;
interface ISubPicAllocatorPresenter;
DECLARE_INTERFACE_IID(IAMLine21Decoder_2, "6E8D4A21-310C-11d0-B79A-00AA003767A7");

// The interfaces of a playback graph: the graph itself, its video renderer,
// the DVD navigator and the capture devices. CMainFrame holds one for playback
// and one for the seekbar preview.
struct CPlayerGraph {
    CComPtr<IGraphBuilder2> m_pGB;
    CComQIPtr<IMediaControl> m_pMC;
    CComQIPtr<IMediaEventEx> m_pME;
    CComQIPtr<IVideoWindow> m_pVW;
    CComQIPtr<IBasicVideo> m_pBV;
    CComQIPtr<IBasicAudio> m_pBA;
    CComQIPtr<IMediaSeeking> m_pMS;
    CComQIPtr<IVideoFrameStep> m_pFS;
    CComQIPtr<IFileSourceFilter> m_pFSF;
    CComQIPtr<IKeyFrameInfo> m_pKFI;
    CComQIPtr<IQualProp, &IID_IQualProp> m_pQP;
    CComQIPtr<IBufferInfo> m_pBI;
    CComQIPtr<IAMOpenProgress> m_pAMOP;
    CComQIPtr<IAMMediaContent, &IID_IAMMediaContent> m_pAMMC[2];
    CComQIPtr<IAMNetworkStatus, &IID_IAMNetworkStatus> m_pAMNS;
    CComQIPtr<IAMStreamSelect> m_pAudioSwitcherSS;
    CComQIPtr<IAMStreamSelect> m_pSplitterSS;
    CComQIPtr<IAMStreamSelect> m_pSplitterDubSS;
    CComQIPtr<IAMStreamSelect> m_pOtherSS[2];

    CComPtr<IVMRMixerControl9> m_pVMRMC;
    CComPtr<IMFVideoDisplayControl> m_pMFVDC;
    CComPtr<IMFVideoProcessor> m_pMFVP;
    CComPtr<IVMRMixerBitmap9>    m_pVMB;
    CComPtr<IMFVideoMixerBitmap>    m_pMFVMB;
    CComPtr<IVMRWindowlessControl9> m_pVMRWC;

    CComPtr<ISubPicAllocatorPresenter> m_pCAP;
    CComPtr<ISubPicAllocatorPresenter2> m_pCAP2;
    CComPtr<ISubPicAllocatorPresenter3> m_pCAP3;

    CComPtr<IMadVRSettings> m_pMVRS;
    CComPtr<IMadVRSubclassReplacement> m_pMVRSR;
    CComPtr<IMPCVRSubclassReplacement> m_pMPCVRSR;
    CComPtr<IMadVRCommand> m_pMVRC;
    CComPtr<IMadVRInfo> m_pMVRI;
    CComPtr<IMadVRFrameGrabber> m_pMVRFG;
    CComPtr<IMadVRTextOsd> m_pMVTO;

    CComPtr<ID3DFullscreenControl> m_pD3DFSC;

    CComQIPtr<IDvdControl2> m_pDVDC;
    CComQIPtr<IDvdInfo2> m_pDVDI;
    CComPtr<IAMLine21Decoder_2> m_pLN21;

    CComPtr<ICaptureGraphBuilder2> m_pCGB;
    CComPtr<IBaseFilter> m_pVidCap, m_pAudCap;
    CComPtr<IAMVideoCompression> m_pAMVCCap, m_pAMVCPrev;
    CComPtr<IAMStreamConfig> m_pAMVSCCap, m_pAMVSCPrev, m_pAMASC;
    CComPtr<IAMCrossbar> m_pAMXBar;
    CComPtr<IAMTVTuner> m_pAMTuner;
    CComPtr<IAMDroppedFrames> m_pAMDF;
};
