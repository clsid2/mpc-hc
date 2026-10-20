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
#include <algorithm>
#include <MMReg.h>
#include "MatroskaMuxer.h"
#include "../../../DSUtil/DSUtil.h"

#ifdef STANDALONE_FILTER
#include <InitGuid.h>
#endif
#include "moreuuids.h"

using namespace MatroskaWriter;

#ifdef STANDALONE_FILTER

const AMOVIESETUP_MEDIATYPE sudPinTypesOut[] = {
    {&MEDIATYPE_Stream, &MEDIASUBTYPE_Matroska}
};

const AMOVIESETUP_PIN sudpPins[] = {
    {const_cast<LPWSTR>(L"Input"), FALSE, FALSE, FALSE, TRUE, &CLSID_NULL, nullptr, 0, nullptr},
    {const_cast<LPWSTR>(L"Output"), FALSE, TRUE, FALSE, FALSE, &CLSID_NULL, nullptr, _countof(sudPinTypesOut), sudPinTypesOut}
};

const AMOVIESETUP_FILTER sudFilter[] = {
    {&__uuidof(CMatroskaMuxerFilter), MatroskaMuxerName, MERIT_DO_NOT_USE, _countof(sudpPins), sudpPins, CLSID_LegacyAmFilterCategory}
};

CFactoryTemplate g_Templates[] = {
    {sudFilter[0].strName, sudFilter[0].clsID, CreateInstance<CMatroskaMuxerFilter>, nullptr, &sudFilter[0]}
};

int g_cTemplates = _countof(g_Templates);

STDAPI DllRegisterServer()
{
    return AMovieDllRegisterServer2(TRUE);
}

STDAPI DllUnregisterServer()
{
    return AMovieDllRegisterServer2(FALSE);
}

#include "../../FilterApp.h"

CFilterApp theApp;

#endif

//
// CMatroskaMuxerFilter
//

CMatroskaMuxerFilter::CMatroskaMuxerFilter(LPUNKNOWN pUnk, HRESULT* phr)
    : CBaseFilter(NAME("CMatroskaMuxerFilter"), pUnk, this, __uuidof(this))
    , m_rtCurrent(0)
    , m_fNegative(true)
    , m_fPositive(false)
{
    if (phr) {
        *phr = S_OK;
    }

    m_pOutput.Attach(DEBUG_NEW CMatroskaMuxerOutputPin(NAME("CMatroskaMuxerOutputPin"), this, this, phr));

    AddInput();

    srand(clock());
}

CMatroskaMuxerFilter::~CMatroskaMuxerFilter()
{
    CAutoLock cAutoLock(this);
}

STDMETHODIMP CMatroskaMuxerFilter::NonDelegatingQueryInterface(REFIID riid, void** ppv)
{
    CheckPointer(ppv, E_POINTER);

    *ppv = nullptr;

    return
        //      QI(IAMFilterMiscFlags)
        QI(IMediaSeeking)
        QI(IMatroskaMuxer)
        __super::NonDelegatingQueryInterface(riid, ppv);
}

UINT CMatroskaMuxerFilter::GetTrackNumber(const CBasePin* pPin)
{
    UINT nTrackNumber = 0;

    POSITION pos = m_pInputs.GetHeadPosition();
    while (pos) {
        nTrackNumber++;
        if (m_pInputs.GetNext(pos) == pPin) {
            return nTrackNumber;
        }
    }

    return 0;
}

void CMatroskaMuxerFilter::AddInput()
{
    POSITION pos = m_pInputs.GetHeadPosition();
    while (pos) {
        CBasePin* pPin = m_pInputs.GetNext(pos);
        if (!pPin->IsConnected()) {
            return;
        }
    }

    CStringW name;
    name.Format(L"Track %u", m_pInputs.GetCount() + 1);

    HRESULT hr;
    CAutoPtr<CMatroskaMuxerInputPin> pPin(DEBUG_NEW CMatroskaMuxerInputPin(name, this, this, &hr));
    m_pInputs.AddTail(pPin);
}

int CMatroskaMuxerFilter::GetPinCount()
{
    return (int)m_pInputs.GetCount() + (m_pOutput ? 1 : 0);
}

CBasePin* CMatroskaMuxerFilter::GetPin(int n)
{
    CAutoLock cAutoLock(this);

    if (n >= 0 && n < (int)m_pInputs.GetCount()) {
        if (POSITION pos = m_pInputs.FindIndex(n)) {
            return m_pInputs.GetAt(pos);
        }
    }

    if (n == (int)m_pInputs.GetCount() && m_pOutput) {
        return m_pOutput;
    }

    return nullptr;
}

STDMETHODIMP CMatroskaMuxerFilter::Stop()
{
    CAutoLock cAutoLock(this);

    HRESULT hr;

    if (FAILED(hr = __super::Stop())) {
        return hr;
    }

    CallWorker(CMD_EXIT);

    return hr;
}

STDMETHODIMP CMatroskaMuxerFilter::Pause()
{
    CAutoLock cAutoLock(this);

    FILTER_STATE fs = m_State;

    HRESULT hr;

    if (FAILED(hr = __super::Pause())) {
        return hr;
    }

    if (fs == State_Stopped && m_pOutput) {
        CAMThread::Create();
        CallWorker(CMD_RUN);
    }

    return hr;
}

STDMETHODIMP CMatroskaMuxerFilter::Run(REFERENCE_TIME tStart)
{
    CAutoLock cAutoLock(this);

    HRESULT hr;

    if (FAILED(hr = __super::Run(tStart))) {
        return hr;
    }

    return hr;
}

// IAMFilterMiscFlags

STDMETHODIMP_(ULONG) CMatroskaMuxerFilter::GetMiscFlags()
{
    return AM_FILTER_MISC_FLAGS_IS_RENDERER;
}

// IMediaSeeking

STDMETHODIMP CMatroskaMuxerFilter::GetCapabilities(DWORD* pCapabilities)
{
    CheckPointer(pCapabilities, E_POINTER);

    *pCapabilities = AM_SEEKING_CanGetDuration | AM_SEEKING_CanGetCurrentPos;

    return S_OK;
}

STDMETHODIMP CMatroskaMuxerFilter::CheckCapabilities(DWORD* pCapabilities)
{
    CheckPointer(pCapabilities, E_POINTER);
    if (*pCapabilities == 0) {
        return S_OK;
    }
    DWORD caps;
    GetCapabilities(&caps);
    caps &= *pCapabilities;
    return caps == 0 ? E_FAIL : caps == *pCapabilities ? S_OK : S_FALSE;
}

STDMETHODIMP CMatroskaMuxerFilter::IsFormatSupported(const GUID* pFormat)
{
    return !pFormat ? E_POINTER : *pFormat == TIME_FORMAT_MEDIA_TIME ? S_OK : S_FALSE;
}

STDMETHODIMP CMatroskaMuxerFilter::QueryPreferredFormat(GUID* pFormat)
{
    return GetTimeFormat(pFormat);
}

STDMETHODIMP CMatroskaMuxerFilter::GetTimeFormat(GUID* pFormat)
{
    CheckPointer(pFormat, E_POINTER);

    *pFormat = TIME_FORMAT_MEDIA_TIME;

    return S_OK;
}

STDMETHODIMP CMatroskaMuxerFilter::IsUsingTimeFormat(const GUID* pFormat)
{
    return IsFormatSupported(pFormat);
}

STDMETHODIMP CMatroskaMuxerFilter::SetTimeFormat(const GUID* pFormat)
{
    return S_OK == IsFormatSupported(pFormat) ? S_OK : E_INVALIDARG;
}

STDMETHODIMP CMatroskaMuxerFilter::GetDuration(LONGLONG* pDuration)
{
    CheckPointer(pDuration, E_POINTER);
    *pDuration = 0;
    POSITION pos = m_pInputs.GetHeadPosition();
    while (pos) {
        REFERENCE_TIME rt = m_pInputs.GetNext(pos)->m_rtDur;
        if (rt > *pDuration) {
            *pDuration = rt;
        }
    }
    return S_OK;
}

STDMETHODIMP CMatroskaMuxerFilter::GetStopPosition(LONGLONG* pStop)
{
    return E_NOTIMPL;
}

STDMETHODIMP CMatroskaMuxerFilter::GetCurrentPosition(LONGLONG* pCurrent)
{
    CheckPointer(pCurrent, E_POINTER);
    *pCurrent = m_rtCurrent;
    return S_OK;
}

STDMETHODIMP CMatroskaMuxerFilter::ConvertTimeFormat(LONGLONG* pTarget, const GUID* pTargetFormat, LONGLONG Source, const GUID* pSourceFormat)
{
    return E_NOTIMPL;
}

STDMETHODIMP CMatroskaMuxerFilter::SetPositions(LONGLONG* pCurrent, DWORD dwCurrentFlags, LONGLONG* pStop, DWORD dwStopFlags)
{
    return E_NOTIMPL;
}

STDMETHODIMP CMatroskaMuxerFilter::GetPositions(LONGLONG* pCurrent, LONGLONG* pStop)
{
    return E_NOTIMPL;
}

STDMETHODIMP CMatroskaMuxerFilter::GetAvailable(LONGLONG* pEarliest, LONGLONG* pLatest)
{
    return E_NOTIMPL;
}

STDMETHODIMP CMatroskaMuxerFilter::SetRate(double dRate)
{
    return E_NOTIMPL;
}

STDMETHODIMP CMatroskaMuxerFilter::GetRate(double* pdRate)
{
    return E_NOTIMPL;
}

STDMETHODIMP CMatroskaMuxerFilter::GetPreroll(LONGLONG* pllPreroll)
{
    return E_NOTIMPL;
}

// IMatroskaMuxer

STDMETHODIMP CMatroskaMuxerFilter::CorrectTimeOffset(bool fNegative, bool fPositive)
{
    m_fNegative = fNegative;
    m_fPositive = fPositive;
    return S_OK;
}

//

ULONGLONG GetStreamPosition(IStream* pStream)
{
    ULARGE_INTEGER pos = {0, 0};
    pStream->Seek(*(LARGE_INTEGER*)&pos, STREAM_SEEK_CUR, &pos);
    return pos.QuadPart;
}

ULONGLONG SetStreamPosition(IStream* pStream, ULONGLONG seekpos)
{
    LARGE_INTEGER pos;
    pos.QuadPart = seekpos;
    ULARGE_INTEGER posnew;
    posnew.QuadPart = GetStreamPosition(pStream);
    pStream->Seek(pos, STREAM_SEEK_SET, &posnew);
    return posnew.QuadPart;
}

#pragma warning(push)
#pragma warning(disable: 4702)
DWORD CMatroskaMuxerFilter::ThreadProc()
{
    CComQIPtr<IStream> pStream;

    if (!m_pOutput || !(pStream = m_pOutput->GetConnected())) {
        for (;;) {
            DWORD cmd = GetRequest();
            if (cmd == CMD_EXIT) {
                CAMThread::m_hThread = nullptr;
            }
            Reply(S_OK);
            if (cmd == CMD_EXIT) {
                return 0;
            }
        }
    }

    REFERENCE_TIME rtDur = 0;
    GetDuration(&rtDur);

    SetStreamPosition(pStream, 0);

    ULARGE_INTEGER uli = {0};
    pStream->SetSize(uli);

    EBML hdr;
    hdr.DocType.Set("matroska");
    hdr.DocTypeVersion.Set(1);
    hdr.DocTypeReadVersion.Set(1);
    hdr.Write(pStream);

    Segment().Write(pStream);
    ULONGLONG segpos = GetStreamPosition(pStream);

    // TODO
    QWORD voidlen = 100;
    if (rtDur > 0) {
        voidlen += QWORD(1.0 * rtDur / MAXCLUSTERTIME / 10000 + 0.5) * 20;
    } else {
        voidlen += QWORD(1.0 * 1000 * 60 * 60 * 24 / MAXCLUSTERTIME + 0.5) * 20; // when no duration is known, allocate for 24 hours (~340k)
    }
    ULONGLONG voidpos = GetStreamPosition(pStream);
    {
        Void v(voidlen);
        voidlen = v.Size();
        v.Write(pStream);
    }

    // Meta Seek

    Seek seek;
    CAutoPtr<SeekHead> sh;

    // Segment Info

    sh.Attach(DEBUG_NEW SeekHead());
    sh->ID.Set(0x1549A966);
    sh->Position.Set(GetStreamPosition(pStream) - segpos);
    seek.SeekHeads.AddTail(sh);

    ULONGLONG infopos = GetStreamPosition(pStream);
    Info info;
    info.MuxingApp.Set(L"DirectShow Matroska Muxer");
    info.TimeCodeScale.Set(1000000);
    info.Duration.Set((float)rtDur / 10000);
    struct tm _2001 = {0, 0, 0, 1, 0, 101, 0, 0, 1};
    info.DateUTC.Set((_time64(nullptr) - _mktime64(&_2001)) * 1000000000);
    info.Write(pStream);

    // Tracks

    sh.Attach(DEBUG_NEW SeekHead());
    sh->ID.Set(0x1654AE6B);
    sh->Position.Set(GetStreamPosition(pStream) - segpos);
    seek.SeekHeads.AddTail(sh);

    UINT64 TrackNumber = 0;
    /*
        CNode<Track> Tracks;
        CAutoPtr<Track> pT(DEBUG_NEW Track());
        POSITION pos = m_pInputs.GetHeadPosition();
        for (int i = 1; pos; i++)
        {
            CMatroskaMuxerInputPin* pPin = m_pInputs.GetNext(pos);
            if (!pPin->IsConnected()) continue;

            CAutoPtr<TrackEntry> pTE(DEBUG_NEW TrackEntry());
            *pTE = *pPin->GetTrackEntry();
            if (TrackNumber == 0 && pTE->TrackType == TrackEntry::TypeVideo)
                TrackNumber = pTE->TrackNumber;
            pT->TrackEntries.AddTail(pTE);
        }
        Tracks.AddTail(pT);
        Tracks.Write(pStream);

        if (TrackNumber == 0) TrackNumber = 1;
    */
    bool fTracksWritten = false;

    //

    Cluster c;
    c.TimeCode.Set(0);

    bool fFirstBlock = true;
    INT64 firstTimeCode = 0;

    CAtlList<CMatroskaMuxerInputPin*> pActivePins;

    POSITION pos = m_pInputs.GetHeadPosition();
    while (pos) {
        CMatroskaMuxerInputPin* pPin = m_pInputs.GetNext(pos);
        if (pPin->IsConnected()) {
            pActivePins.AddTail(pPin);
        }
    }

    for (;;) {
        DWORD cmd = GetRequest();

        switch (cmd) {
            default:
            case CMD_EXIT:
                CAMThread::m_hThread = nullptr;
                Reply(S_OK);
                return 0;

            case CMD_RUN:
                Reply(S_OK);

                Cue cue;
                ULONGLONG lastcueclusterpos = (ULONGLONG) - 1;
                INT64 lastcuetimecode = (INT64) - 1;
                UINT64 nBlocksInCueTrack = 0;

                while (!CheckRequest(nullptr)) {
                    if (m_State == State_Paused) {
                        Sleep(10);
                        continue;
                    }

                    int nPinsGotSomething = 0, nPinsNeeded = 0;
                    CMatroskaMuxerInputPin* pPin = nullptr;
                    REFERENCE_TIME rtMin = _I64_MAX;

                    pos = pActivePins.GetHeadPosition();
                    while (pos) {
                        CMatroskaMuxerInputPin* pTmp = pActivePins.GetNext(pos);

                        CAutoLock cAutoLock(&pTmp->m_csQueue);

                        if (pTmp->m_blocks.IsEmpty() && pTmp->m_fEndOfStreamReceived) {
                            pActivePins.RemoveAt(pActivePins.Find(pTmp));
                            continue;
                        }

                        if (pTmp->GetTrackEntry()->TrackType != TrackEntry::TypeSubtitle) {
                            nPinsNeeded++;
                        }

                        if (!pTmp->m_blocks.IsEmpty()) {
                            if (pTmp->GetTrackEntry()->TrackType != TrackEntry::TypeSubtitle) {
                                nPinsGotSomething++;
                            }

                            if (!pTmp->m_blocks.IsEmpty()) {
                                REFERENCE_TIME rt = pTmp->m_blocks.GetHead()->Block.TimeCode;
                                if (rt < rtMin) {
                                    rtMin = rt;
                                    pPin = pTmp;
                                }
                            }
                        }
                    }

                    if (pActivePins.IsEmpty()) {
                        break;
                    }

                    if (!pPin || nPinsNeeded > nPinsGotSomething || !pPin && nPinsGotSomething == 0) {
                        Sleep(1);
                        continue;
                    }

                    if (!fTracksWritten) {
                        CNode<Track> Tracks;
                        CAutoPtr<Track> pT(DEBUG_NEW Track());
                        pos = pActivePins.GetHeadPosition();
                        for (int i = 1; pos; i++) {
                            CMatroskaMuxerInputPin* pActivePin = pActivePins.GetNext(pos);

                            CAutoPtr<TrackEntry> pTE(DEBUG_NEW TrackEntry());
                            *pTE = *pActivePin->GetTrackEntry();
                            if (TrackNumber == 0 && pTE->TrackType == TrackEntry::TypeVideo) {
                                TrackNumber = pTE->TrackNumber;
                            }
                            pT->TrackEntries.AddTail(pTE);
                        }
                        Tracks.AddTail(pT);
                        Tracks.Write(pStream);

                        if (TrackNumber == 0) {
                            TrackNumber = 1;
                        }

                        fTracksWritten = true;
                    }

                    ASSERT(pPin);

                    CAutoPtr<BlockGroup> b;

                    {
                        CAutoLock cAutoLock(&pPin->m_csQueue);
                        b.Attach(pPin->m_blocks.RemoveHead().Detach());
                    }

                    if (b) {
                        if (fFirstBlock) {
                            if (b->Block.TimeCode < 0 && m_fNegative || b->Block.TimeCode > 0 && m_fPositive) {
                                firstTimeCode = b->Block.TimeCode;
                            }
                            fFirstBlock = false;
                        }

                        b->Block.TimeCode -= firstTimeCode;
                        b->Block.TimeCodeStop -= firstTimeCode;

                        /*
                        TRACE(_T("Muxing (%d): %I64d-%I64d dur=%I64d (c=%d, co=%dms), cnt=%d, ref=%d\n"),
                            GetTrackNumber(pPin),
                            (INT64)b->Block.TimeCode, (INT64)b->Block.TimeCodeStop, (UINT64)b->BlockDuration,
                            (int)((b->Block.TimeCode)/MAXCLUSTERTIME), (int)(b->Block.TimeCode%MAXCLUSTERTIME),
                            b->Block.BlockData.GetCount(), (int)b->ReferenceBlock);
                        */
                        if (b->Block.TimeCode < SHRT_MIN /*0*/) {
                            ASSERT(0);
                            continue;
                        }

                        while ((INT64)(c.TimeCode + MAXCLUSTERTIME) < b->Block.TimeCode) {
                            if (!c.BlockGroups.IsEmpty()) {
                                sh.Attach(DEBUG_NEW SeekHead());
                                sh->ID.Set(c.GetID()/*0x1F43B675*/);
                                sh->Position.Set(GetStreamPosition(pStream) - segpos);
                                seek.SeekHeads.AddTail(sh);

                                c.Write(pStream); // TODO: write blocks
                            }

                            c.TimeCode.Set(c.TimeCode + MAXCLUSTERTIME);
                            c.BlockGroups.RemoveAll();
                            nBlocksInCueTrack = 0;
                        }

                        if (b->Block.TrackNumber == TrackNumber) {
                            nBlocksInCueTrack++;
                        }

                        if (b->ReferenceBlock == 0 && b->Block.TrackNumber == TrackNumber) {
                            ULONGLONG clusterpos = GetStreamPosition(pStream) - segpos;
                            if (lastcueclusterpos != clusterpos || lastcuetimecode + 1000 < b->Block.TimeCode) {
                                CAutoPtr<CueTrackPosition> ctp(DEBUG_NEW CueTrackPosition());
                                ctp->CueTrack.Set(b->Block.TrackNumber);
                                ctp->CueClusterPosition.Set(clusterpos);
                                if (!c.BlockGroups.IsEmpty()) {
                                    ctp->CueBlockNumber.Set(nBlocksInCueTrack);
                                }
                                CAutoPtr<CuePoint> cp(DEBUG_NEW CuePoint());
                                cp->CueTime.Set(b->Block.TimeCode);
                                cp->CueTrackPositions.AddTail(ctp);
                                cue.CuePoints.AddTail(cp);
                                lastcueclusterpos = clusterpos;
                                lastcuetimecode = b->Block.TimeCode;
                            }
                        }

                        info.Duration.Set(std::max<float>(info.Duration, (float)b->Block.TimeCodeStop));

                        m_rtCurrent = b->Block.TimeCode * 10000;

                        b->Block.TimeCode -= c.TimeCode;
                        c.BlockGroups.AddTail(b);
                    }
                }

                if (!c.BlockGroups.IsEmpty()) {
                    sh.Attach(DEBUG_NEW SeekHead());
                    sh->ID.Set(c.GetID()/*0x1F43B675*/);
                    sh->Position.Set(GetStreamPosition(pStream) - segpos);
                    seek.SeekHeads.AddTail(sh);

                    c.Write(pStream);
                }

                if (!cue.CuePoints.IsEmpty()) {
                    sh.Attach(DEBUG_NEW SeekHead());
                    sh->ID.Set(cue.GetID()/*0x1C53BB6B*/);
                    sh->Position.Set(GetStreamPosition(pStream) - segpos);
                    seek.SeekHeads.AddTail(sh);

                    cue.Write(pStream);
                }

                {
                    Tags tags;

                    sh.Attach(DEBUG_NEW SeekHead());
                    sh->ID.Set(tags.GetID());
                    sh->Position.Set(GetStreamPosition(pStream) - segpos);
                    seek.SeekHeads.AddTail(sh);

                    tags.Write(pStream);
                }

                // if the SeekHead cannot fit in the reserved space, leave the Void as it is
                INT64 fit = seek.FitInto(voidlen);
                if (fit >= 0) {
                    SetStreamPosition(pStream, voidpos);
                    seek.Write(pStream);
                    Void::Fill(pStream, (QWORD)fit);
                }

                if (abs(m_rtCurrent - (REFERENCE_TIME)info.Duration * 10000) > 10000000i64) {
                    info.Duration.Set((float)m_rtCurrent / 10000 + 1);
                }

                SetStreamPosition(pStream, infopos);
                info.Write(pStream);

                // TODO: write some tags

                m_pOutput->DeliverEndOfStream();

                break;
        }
    }
    UNREACHABLE_CODE(); // we should only exit via CMD_EXIT
#pragma warning(pop)
}

//
// CMatroskaMuxerInputPin
//

CMatroskaMuxerInputPin::CMatroskaMuxerInputPin(LPCWSTR pName, CBaseFilter* pFilter, CCritSec* pLock, HRESULT* phr)
    : CBaseInputPin(NAME("CMatroskaMuxerInputPin"), pFilter, pLock, phr, pName)
    , m_fActive(false)
    , m_rtLastStart(0)
    , m_rtLastStop(0)
    , m_rtDur(0)
    , m_fEndOfStreamReceived(false)
{
}

CMatroskaMuxerInputPin::~CMatroskaMuxerInputPin()
{
}

STDMETHODIMP CMatroskaMuxerInputPin::NonDelegatingQueryInterface(REFIID riid, void** ppv)
{
    CheckPointer(ppv, E_POINTER);

    return
        __super::NonDelegatingQueryInterface(riid, ppv);
}

HRESULT CMatroskaMuxerInputPin::CheckMediaType(const CMediaType* pmt)
{
    return pmt->majortype == MEDIATYPE_Video && (pmt->formattype == FORMAT_VideoInfo
                                                 || pmt->formattype == FORMAT_VideoInfo2)
           //       || pmt->majortype == MEDIATYPE_Video && pmt->subtype == MEDIASUBTYPE_MPEG1Payload && pmt->formattype == FORMAT_MPEGVideo
           //       || pmt->majortype == MEDIATYPE_Video && pmt->subtype == MEDIASUBTYPE_MPEG2_VIDEO && pmt->formattype == FORMAT_MPEG2_VIDEO
           || pmt->majortype == MEDIATYPE_Video && pmt->subtype == MEDIASUBTYPE_DiracVideo && pmt->formattype == FORMAT_DiracVideoInfo
           || pmt->majortype == MEDIATYPE_Audio && pmt->formattype == FORMAT_WaveFormatEx && pmt->subtype == FOURCCMap(((WAVEFORMATEX*)pmt->pbFormat)->wFormatTag)
           || pmt->majortype == MEDIATYPE_Audio && pmt->subtype == MEDIASUBTYPE_Vorbis && pmt->formattype == FORMAT_VorbisFormat
           || pmt->majortype == MEDIATYPE_Audio && pmt->subtype == MEDIASUBTYPE_Vorbis2 && pmt->formattype == FORMAT_VorbisFormat2
           || pmt->majortype == MEDIATYPE_Audio && (pmt->subtype == MEDIASUBTYPE_14_4
                                                    || pmt->subtype == MEDIASUBTYPE_28_8
                                                    || pmt->subtype == MEDIASUBTYPE_ATRC
                                                    || pmt->subtype == MEDIASUBTYPE_COOK
                                                    || pmt->subtype == MEDIASUBTYPE_DNET
                                                    || pmt->subtype == MEDIASUBTYPE_SIPR) && pmt->formattype == FORMAT_WaveFormatEx
           || pmt->majortype == MEDIATYPE_Text && pmt->subtype == MEDIASUBTYPE_NULL && pmt->formattype == FORMAT_None
           || pmt->majortype == MEDIATYPE_Subtitle && pmt->formattype == FORMAT_SubtitleInfo
           ? S_OK
           : E_INVALIDARG;
}

HRESULT CMatroskaMuxerInputPin::BreakConnect()
{
    HRESULT hr;

    if (FAILED(hr = __super::BreakConnect())) {
        return hr;
    }

    m_pTE.Free();

    return hr;
}

HRESULT CMatroskaMuxerInputPin::CompleteConnect(IPin* pPin)
{
    HRESULT hr;

    if (FAILED(hr = __super::CompleteConnect(pPin))) {
        return hr;
    }

    m_rtDur = 0;
    CComQIPtr<IMediaSeeking> pMS;
    if ((pMS = GetFilterFromPin(pPin)) || (pMS = pPin)) {
        pMS->GetDuration(&m_rtDur);
    }

    m_pTE.Free();
    m_pTE.Attach(DEBUG_NEW TrackEntry());

    m_pTE->TrackUID.Set(rand());
    m_pTE->MinCache.Set(1);
    m_pTE->MaxCache.Set(1);
    m_pTE->TrackNumber.Set((static_cast<CMatroskaMuxerFilter*>(m_pFilter))->GetTrackNumber(this));

    hr = E_FAIL;

    if (m_mt.majortype == MEDIATYPE_Video) {
        m_pTE->TrackType.Set(TrackEntry::TypeVideo);

        if (m_mt.formattype == FORMAT_VideoInfo
                && m_mt.subtype == MEDIASUBTYPE_RV10 || m_mt.subtype == MEDIASUBTYPE_RV20
                || m_mt.subtype == MEDIASUBTYPE_RV30 || m_mt.subtype == MEDIASUBTYPE_RV40) {
            m_pTE->CodecID.Set("V_REAL/RV00");
            m_pTE->CodecID.SetAt(9, (BYTE)(m_mt.subtype.Data1 >> 16));

            if (m_mt.formattype == FORMAT_VideoInfo) {
                VIDEOINFOHEADER* vih = (VIDEOINFOHEADER*)m_mt.pbFormat;
                if (m_mt.cbFormat > sizeof(VIDEOINFOHEADER)) {
                    m_pTE->CodecPrivate.SetCount(m_mt.cbFormat - sizeof(VIDEOINFOHEADER));
                    memcpy(m_pTE->CodecPrivate, m_mt.pbFormat + sizeof(VIDEOINFOHEADER), m_pTE->CodecPrivate.GetCount());
                }
                m_pTE->DefaultDuration.Set(vih->AvgTimePerFrame * 100);
                m_pTE->DescType = TrackEntry::DescVideo;
                m_pTE->v.PixelWidth.Set(vih->bmiHeader.biWidth);
                m_pTE->v.PixelHeight.Set(abs(vih->bmiHeader.biHeight));
                if (vih->AvgTimePerFrame > 0) {
                    m_pTE->v.FramePerSec.Set((float)(10000000.0 / vih->AvgTimePerFrame));
                }
            } else if (m_mt.formattype == FORMAT_VideoInfo2) {
                VIDEOINFOHEADER2* vih = (VIDEOINFOHEADER2*)m_mt.pbFormat;
                if (m_mt.cbFormat > sizeof(VIDEOINFOHEADER2)) {
                    m_pTE->CodecPrivate.SetCount(m_mt.cbFormat - sizeof(VIDEOINFOHEADER2));
                    memcpy(m_pTE->CodecPrivate, m_mt.pbFormat + sizeof(VIDEOINFOHEADER2), m_pTE->CodecPrivate.GetCount());
                }
                m_pTE->DefaultDuration.Set(vih->AvgTimePerFrame * 100);
                m_pTE->DescType = TrackEntry::DescVideo;
                m_pTE->v.PixelWidth.Set(vih->bmiHeader.biWidth);
                m_pTE->v.PixelHeight.Set(abs(vih->bmiHeader.biHeight));
                if (vih->AvgTimePerFrame > 0) {
                    m_pTE->v.FramePerSec.Set((float)(10000000.0 / vih->AvgTimePerFrame));
                }
                m_pTE->v.DisplayWidth.Set(vih->dwPictAspectRatioX);
                m_pTE->v.DisplayHeight.Set(vih->dwPictAspectRatioY);
            } else {
                ASSERT(0);
                return hr;
            }

            hr = S_OK;
        } else if (m_mt.formattype == FORMAT_VideoInfo) {
            m_pTE->CodecID.Set("V_MS/VFW/FOURCC");

            VIDEOINFOHEADER* vih = (VIDEOINFOHEADER*)m_mt.pbFormat;
            m_pTE->CodecPrivate.SetCount(m_mt.cbFormat - FIELD_OFFSET(VIDEOINFOHEADER, bmiHeader));
            memcpy(m_pTE->CodecPrivate, &vih->bmiHeader, m_pTE->CodecPrivate.GetCount());
            m_pTE->DefaultDuration.Set(vih->AvgTimePerFrame * 100);
            m_pTE->DescType = TrackEntry::DescVideo;
            m_pTE->v.PixelWidth.Set(vih->bmiHeader.biWidth);
            m_pTE->v.PixelHeight.Set(abs(vih->bmiHeader.biHeight));
            if (vih->AvgTimePerFrame > 0) {
                m_pTE->v.FramePerSec.Set((float)(10000000.0 / vih->AvgTimePerFrame));
            }

            hr = S_OK;
        } else if (m_mt.formattype == FORMAT_VideoInfo2) {
            m_pTE->CodecID.Set("V_MS/VFW/FOURCC");

            VIDEOINFOHEADER2* vih = (VIDEOINFOHEADER2*)m_mt.pbFormat;
            m_pTE->CodecPrivate.SetCount(m_mt.cbFormat - FIELD_OFFSET(VIDEOINFOHEADER2, bmiHeader));
            memcpy(m_pTE->CodecPrivate, &vih->bmiHeader, m_pTE->CodecPrivate.GetCount());
            m_pTE->DefaultDuration.Set(vih->AvgTimePerFrame * 100);
            m_pTE->DescType = TrackEntry::DescVideo;
            m_pTE->v.PixelWidth.Set(vih->bmiHeader.biWidth);
            m_pTE->v.PixelHeight.Set(abs(vih->bmiHeader.biHeight));
            m_pTE->v.DisplayWidth.Set(vih->dwPictAspectRatioX);
            m_pTE->v.DisplayHeight.Set(vih->dwPictAspectRatioY);
            if (vih->AvgTimePerFrame > 0) {
                m_pTE->v.FramePerSec.Set((float)(10000000.0 / vih->AvgTimePerFrame));
            }

            hr = S_OK;
        } else if (m_mt.formattype == FORMAT_DiracVideoInfo) {
            m_pTE->CodecID.Set("V_DIRAC");

            DIRACINFOHEADER* vih = (DIRACINFOHEADER*)m_mt.pbFormat;
            m_pTE->CodecPrivate.SetCount(vih->cbSequenceHeader);
            memcpy(m_pTE->CodecPrivate, (BYTE*)&vih->dwSequenceHeader[0], m_pTE->CodecPrivate.GetCount());
            m_pTE->DefaultDuration.Set(vih->hdr.AvgTimePerFrame * 100);
            m_pTE->DescType = TrackEntry::DescVideo;
            m_pTE->v.PixelWidth.Set(vih->hdr.bmiHeader.biWidth);
            m_pTE->v.PixelHeight.Set(abs(vih->hdr.bmiHeader.biHeight));
            // m_pTE->v.DisplayWidth.Set(vih->dwPictAspectRatioX);
            // m_pTE->v.DisplayHeight.Set(vih->dwPictAspectRatioY);
            if (vih->hdr.AvgTimePerFrame > 0) {
                m_pTE->v.FramePerSec.Set((float)(10000000.0 / vih->hdr.AvgTimePerFrame));
            }

            hr = S_OK;
        }
        /*
                else if (m_mt.formattype == FORMAT_MPEGVideo)
                {
                    m_pTE->CodecID.Set("V_DSHOW/MPEG1VIDEO"); // V_MPEG1

                    MPEG1VIDEOINFO* pm1vi = (MPEG1VIDEOINFO*)m_mt.pbFormat;
                    m_pTE->CodecPrivate.SetCount(m_mt.FormatLength());
                    memcpy(m_pTE->CodecPrivate, m_mt.pbFormat, m_pTE->CodecPrivate.GetCount());
                    m_pTE->DefaultDuration.Set(pm1vi->hdr.AvgTimePerFrame*100);
                    m_pTE->DescType = TrackEntry::DescVideo;
                    m_pTE->v.PixelWidth.Set(pm1vi->hdr.bmiHeader.biWidth);
                    m_pTE->v.PixelHeight.Set(abs(pm1vi->hdr.bmiHeader.biHeight));
                    if (pm1vi->hdr.AvgTimePerFrame > 0)
                        m_pTE->v.FramePerSec.Set((float)(10000000.0 / pm1vi->hdr.AvgTimePerFrame));

                    hr = S_OK;
                }
                else if (m_mt.formattype == FORMAT_MPEG2_VIDEO)
                {
                    m_pTE->CodecID.Set("V_DSHOW/MPEG2VIDEO"); // V_MPEG2

                    MPEG2VIDEOINFO* pm2vi = (MPEG2VIDEOINFO*)m_mt.pbFormat;
                    m_pTE->CodecPrivate.SetCount(m_mt.FormatLength());
                    memcpy(m_pTE->CodecPrivate, m_mt.pbFormat, m_pTE->CodecPrivate.GetCount());
                    m_pTE->DefaultDuration.Set(pm2vi->hdr.AvgTimePerFrame*100);
                    m_pTE->DescType = TrackEntry::DescVideo;
                    m_pTE->v.PixelWidth.Set(pm2vi->hdr.bmiHeader.biWidth);
                    m_pTE->v.PixelHeight.Set(abs(pm2vi->hdr.bmiHeader.biHeight));
                    if (pm2vi->hdr.AvgTimePerFrame > 0)
                        m_pTE->v.FramePerSec.Set((float)(10000000.0 / pm2vi->hdr.AvgTimePerFrame));

                    hr = S_OK;
                }
        */
    } else if (m_mt.majortype == MEDIATYPE_Audio) {
        m_pTE->TrackType.Set(TrackEntry::TypeAudio);

        if (m_mt.formattype == FORMAT_WaveFormatEx
                && ((WAVEFORMATEX*)m_mt.pbFormat)->wFormatTag == WAVE_FORMAT_AAC
                && m_mt.cbFormat >= sizeof(WAVEFORMATEX) + 2) {
            WAVEFORMATEX* wfe = (WAVEFORMATEX*)m_mt.pbFormat;
            BYTE* p = (BYTE*)(wfe + 1);

            DWORD nSamplesPerSec = wfe->nSamplesPerSec;

            int profile = (p[0] >> 3) - 1;
            int rate1 = ((p[0] & 7) << 1) | (p[1] >> 7);
            int channels = ((p[1] >> 3) & 15);
            int rate2 = rate1;

            if (wfe->cbSize >= 5) {
                profile = 4;

                int exttype = (p[2] << 3) | (p[3] >> 5);
                ASSERT(exttype == 0x2B7);
                ASSERT((p[3] & 31) == 5);
                ASSERT((p[4] >> 7) == 1);
                rate2 = ((p[4] >> 3) & 15);

                if (rate2 < rate1) {
                    m_pTE->a.OutputSamplingFrequency.Set((float)nSamplesPerSec);
                    nSamplesPerSec /= 2;
                }
            }

            switch (profile) {
                default:
                case 0:
                    m_pTE->CodecID.Set("A_AAC/MPEG2/MAIN");
                    break;
                case 1:
                    m_pTE->CodecID.Set("A_AAC/MPEG2/LC");
                    break;
                case 2:
                    m_pTE->CodecID.Set("A_AAC/MPEG2/SSR");
                    break;
                case 3:
                    m_pTE->CodecID.Set("A_AAC/MPEG4/LTP");
                    break;
                case 4:
                    m_pTE->CodecID.Set("A_AAC/MPEG4/LC/SBR");
                    break;
            }

            ASSERT(channels == wfe->nChannels);

            m_pTE->DescType = TrackEntry::DescAudio;
            m_pTE->a.SamplingFrequency.Set((float)nSamplesPerSec);
            m_pTE->a.Channels.Set(channels);
            m_pTE->a.BitDepth.Set(wfe->wBitsPerSample);

            hr = S_OK;
        } else if (m_mt.formattype == FORMAT_WaveFormatEx
                   && ((WAVEFORMATEX*)m_mt.pbFormat)->wFormatTag == WAVE_FORMAT_DOLBY_AC3) {
            m_pTE->CodecID.Set("A_AC3");

            WAVEFORMATEX* wfe = (WAVEFORMATEX*)m_mt.pbFormat;
            m_pTE->DescType = TrackEntry::DescAudio;
            m_pTE->a.SamplingFrequency.Set((float)wfe->nSamplesPerSec);
            m_pTE->a.Channels.Set(wfe->nChannels);
            m_pTE->a.BitDepth.Set(wfe->wBitsPerSample);

            hr = S_OK;
        } else if (m_mt.formattype == FORMAT_WaveFormatEx
                   && ((WAVEFORMATEX*)m_mt.pbFormat)->wFormatTag == WAVE_FORMAT_DVD_DTS) {
            m_pTE->CodecID.Set("A_DTS");

            WAVEFORMATEX* wfe = (WAVEFORMATEX*)m_mt.pbFormat;
            m_pTE->DescType = TrackEntry::DescAudio;
            m_pTE->a.SamplingFrequency.Set((float)wfe->nSamplesPerSec);
            m_pTE->a.Channels.Set(wfe->nChannels);
            m_pTE->a.BitDepth.Set(wfe->wBitsPerSample);

            hr = S_OK;
        } else if (m_mt.formattype == FORMAT_WaveFormatEx
                   && ((WAVEFORMATEX*)m_mt.pbFormat)->wFormatTag == WAVE_FORMAT_FLAC) {
            m_pTE->CodecID.Set("A_FLAC");

            WAVEFORMATEX* wfe = (WAVEFORMATEX*)m_mt.pbFormat;
            m_pTE->DescType = TrackEntry::DescAudio;
            m_pTE->a.SamplingFrequency.Set((float)wfe->nSamplesPerSec);
            m_pTE->a.Channels.Set(wfe->nChannels);
            m_pTE->a.BitDepth.Set(wfe->wBitsPerSample);

            if (wfe->cbSize) {
                m_pTE->CodecPrivate.SetCount(wfe->cbSize);
                memcpy(m_pTE->CodecPrivate, m_mt.pbFormat + sizeof(WAVEFORMATEX), wfe->cbSize);
            }

            hr = S_OK;
        } else if (m_mt.formattype == FORMAT_WaveFormatEx
                   && (m_mt.subtype == MEDIASUBTYPE_14_4
                       || m_mt.subtype == MEDIASUBTYPE_28_8
                       || m_mt.subtype == MEDIASUBTYPE_ATRC
                       || m_mt.subtype == MEDIASUBTYPE_COOK
                       || m_mt.subtype == MEDIASUBTYPE_DNET
                       || m_mt.subtype == MEDIASUBTYPE_SIPR)) {
            CStringA id;
            id.Format("A_REAL/%c%c%c%c",
                      (char)((m_mt.subtype.Data1 >> 0) & 0xff),
                      (char)((m_mt.subtype.Data1 >> 8) & 0xff),
                      (char)((m_mt.subtype.Data1 >> 16) & 0xff),
                      (char)((m_mt.subtype.Data1 >> 24) & 0xff));

            m_pTE->CodecID.Set(id);

            WAVEFORMATEX* wfe = (WAVEFORMATEX*)m_mt.pbFormat;
            DWORD cbSize = sizeof(WAVEFORMATEX) + wfe->cbSize;
            if (m_mt.cbFormat > cbSize) {
                m_pTE->CodecPrivate.SetCount(m_mt.cbFormat - cbSize);
                memcpy(m_pTE->CodecPrivate, m_mt.pbFormat + cbSize, m_pTE->CodecPrivate.GetCount());
            }
            m_pTE->DescType = TrackEntry::DescAudio;
            m_pTE->a.SamplingFrequency.Set((float)wfe->nSamplesPerSec);
            m_pTE->a.Channels.Set(wfe->nChannels);
            m_pTE->a.BitDepth.Set(wfe->wBitsPerSample);

            hr = S_OK;
        } else if (m_mt.formattype == FORMAT_WaveFormatEx
                   && ((WAVEFORMATEX*)m_mt.pbFormat)->wFormatTag == WAVE_FORMAT_PCM) {
            m_pTE->CodecID.Set("A_PCM/INT/LIT");

            WAVEFORMATEX* wfe = (WAVEFORMATEX*)m_mt.pbFormat;
            m_pTE->DescType = TrackEntry::DescAudio;
            m_pTE->a.SamplingFrequency.Set((float)wfe->nSamplesPerSec);
            m_pTE->a.Channels.Set(wfe->nChannels);
            m_pTE->a.BitDepth.Set(wfe->wBitsPerSample);

            hr = S_OK;
        } else if (m_mt.formattype == FORMAT_WaveFormatEx) {
            m_pTE->CodecID.Set("A_MS/ACM");

            WAVEFORMATEX* wfe = (WAVEFORMATEX*)m_mt.pbFormat;
            m_pTE->CodecPrivate.SetCount(m_mt.cbFormat);
            memcpy(m_pTE->CodecPrivate, wfe, m_pTE->CodecPrivate.GetCount());
            m_pTE->DescType = TrackEntry::DescAudio;
            m_pTE->a.SamplingFrequency.Set((float)wfe->nSamplesPerSec);
            m_pTE->a.Channels.Set(wfe->nChannels);
            m_pTE->a.BitDepth.Set(wfe->wBitsPerSample);

            hr = S_OK;
        } else if (m_mt.formattype == FORMAT_VorbisFormat) {
            m_pTE->CodecID.Set("A_VORBIS");

            VORBISFORMAT* pvf = (VORBISFORMAT*)m_mt.pbFormat;
            m_pTE->DescType = TrackEntry::DescAudio;
            m_pTE->a.SamplingFrequency.Set((float)pvf->nSamplesPerSec);
            m_pTE->a.Channels.Set(pvf->nChannels);

            // m_pTE->CodecPrivate will be filled later

            hr = S_OK;
        } else if (m_mt.formattype == FORMAT_VorbisFormat2) {
            m_pTE->CodecID.Set("A_VORBIS");

            VORBISFORMAT2* pvf2 = (VORBISFORMAT2*)m_mt.pbFormat;
            m_pTE->DescType = TrackEntry::DescAudio;
            m_pTE->a.SamplingFrequency.Set((float)pvf2->SamplesPerSec);
            m_pTE->a.Channels.Set(pvf2->Channels);
            m_pTE->a.BitDepth.Set(pvf2->BitsPerSample);

            int len = 1;
            for (int i = 0; i < 2; i++) {
                len += pvf2->HeaderSize[i] / 255 + 1;
            }
            for (int i = 0; i < 3; i++) {
                len += pvf2->HeaderSize[i];
            }
            m_pTE->CodecPrivate.SetCount(len);

            BYTE* src = (BYTE*)m_mt.pbFormat + sizeof(VORBISFORMAT2);
            BYTE* dst = m_pTE->CodecPrivate.GetData();

            *dst++ = 2;
            for (int i = 0; i < 2; i++) {
                for (int len2 = pvf2->HeaderSize[i]; len2 >= 0; len2 -= 255) {
                    *dst++ = (BYTE)std::min(len2, BYTE_MAX);
                }
            }

            memcpy(dst, src, pvf2->HeaderSize[0]);
            dst += pvf2->HeaderSize[0];
            src += pvf2->HeaderSize[0];
            memcpy(dst, src, pvf2->HeaderSize[1]);
            dst += pvf2->HeaderSize[1];
            src += pvf2->HeaderSize[1];
            memcpy(dst, src, pvf2->HeaderSize[2]);
            dst += pvf2->HeaderSize[2];
            src += pvf2->HeaderSize[2];

            ASSERT(src <= m_mt.pbFormat + m_mt.cbFormat);
            ASSERT(dst <= m_pTE->CodecPrivate.GetData() + m_pTE->CodecPrivate.GetCount());

            hr = S_OK;
        }
    } else if (m_mt.majortype == MEDIATYPE_Text) {
        m_pTE->TrackType.Set(TrackEntry::TypeSubtitle);

        if (m_mt.formattype == FORMAT_None) {
            m_pTE->CodecID.Set("S_TEXT/ASCII");
            hr = S_OK;
        }
    } else if (m_mt.majortype == MEDIATYPE_Subtitle) {
        m_pTE->TrackType.Set(TrackEntry::TypeSubtitle);

        m_pTE->CodecID.Set(
            m_mt.subtype == MEDIASUBTYPE_UTF8 ? "S_TEXT/UTF8" :
            m_mt.subtype == MEDIASUBTYPE_SSA ? "S_TEXT/SSA" :
            m_mt.subtype == MEDIASUBTYPE_ASS ? "S_TEXT/ASS" :
            m_mt.subtype == MEDIASUBTYPE_USF ? "S_TEXT/USF" :
            m_mt.subtype == MEDIASUBTYPE_VOBSUB ? "S_VOBSUB" :
            "");

        if (!m_pTE->CodecID.IsEmpty()) {
            hr = S_OK;

            SUBTITLEINFO* psi = (SUBTITLEINFO*)m_mt.pbFormat;
            if (psi->dwOffset) {
                m_pTE->CodecPrivate.SetCount(m_mt.cbFormat - psi->dwOffset);
                memcpy(m_pTE->CodecPrivate, m_mt.pbFormat + psi->dwOffset, m_pTE->CodecPrivate.GetCount());
            }
        }
    }

    if (S_OK == hr) {
        (static_cast<CMatroskaMuxerFilter*>(m_pFilter))->AddInput();
    }

    return hr;
}

HRESULT CMatroskaMuxerInputPin::Active()
{
    m_fActive = true;
    m_rtLastStart = m_rtLastStop = -1;
    m_fEndOfStreamReceived = false;
    return __super::Active();
}

HRESULT CMatroskaMuxerInputPin::Inactive()
{
    m_fActive = false;
    CAutoLock cAutoLock(&m_csQueue);
    m_blocks.RemoveAll();
    m_pVorbisHdrs.RemoveAll();
    return __super::Inactive();
}

STDMETHODIMP CMatroskaMuxerInputPin::NewSegment(REFERENCE_TIME tStart, REFERENCE_TIME tStop, double dRate)
{
    CAutoLock cAutoLock(&m_csReceive);
    return __super::NewSegment(tStart, tStop, dRate);
}

STDMETHODIMP CMatroskaMuxerInputPin::BeginFlush()
{
    return __super::BeginFlush();
}

STDMETHODIMP CMatroskaMuxerInputPin::EndFlush()
{
    return __super::EndFlush();
}

STDMETHODIMP CMatroskaMuxerInputPin::Receive(IMediaSample* pSample)
{
    if (m_fEndOfStreamReceived) {
        /*ASSERT(0);*/
        return S_FALSE;
    }

    CAutoLock cAutoLock(&m_csReceive);

    while (m_fActive) {
        {
            CAutoLock cAutoLock2(&m_csQueue);
            if (m_blocks.GetCount() < MAXBLOCKS) {
                break;
            }
        }

        Sleep(1);
    }

    if (!m_fActive) {
        return S_FALSE;
    }

    HRESULT hr;

    if (FAILED(hr = __super::Receive(pSample))) {
        return hr;
    }

    BYTE* pData = nullptr;
    if (FAILED(hr = pSample->GetPointer(&pData)) || !pData) {
        return hr;
    }

    long inputLen = pSample->GetActualDataLength();

    REFERENCE_TIME rtStart = -1, rtStop = -1;
    hr = pSample->GetTime(&rtStart, &rtStop);

    if (FAILED(hr) || rtStart == -1 || rtStop == -1) {
        TRACE(_T("No timestamp was set on the sample!!!"));
        m_pFilter->NotifyEvent(EC_ERRORABORT, VFW_E_SAMPLE_TIME_NOT_SET, 0);
        return VFW_E_SAMPLE_TIME_NOT_SET;
    }

    //rtStart += m_tStart;
    //rtStop += m_tStart;

    TRACE(_T("Received (%u): %I64d-%I64d (c=%d, co=%dms), len=%ld, d%d p%d s%d\n"),
          (static_cast<CMatroskaMuxerFilter*>(m_pFilter))->GetTrackNumber(this),
          rtStart, rtStop, (int)((rtStart / 10000) / MAXCLUSTERTIME), (int)((rtStart / 10000) % MAXCLUSTERTIME),
          inputLen,
          pSample->IsDiscontinuity() == S_OK ? 1 : 0,
          pSample->IsPreroll() == S_OK ? 1 : 0,
          pSample->IsSyncPoint() == S_OK ? 1 : 0);

    if (m_mt.subtype == MEDIASUBTYPE_Vorbis && m_pVorbisHdrs.GetCount() < 3) {
        CAutoPtr<CBinary> data(DEBUG_NEW CBinary(0));
        data->SetCount(inputLen);
        memcpy(data->GetData(), pData, inputLen);
        m_pVorbisHdrs.Add(data);

        if (m_pVorbisHdrs.GetCount() == 3) {
            int len = 1;
            for (size_t i = 0; i < 2; i++) {
                len += (int)m_pVorbisHdrs[i]->GetCount() / 255 + 1;
            }
            for (size_t i = 0; i < 3; i++) {
                len += (int)m_pVorbisHdrs[i]->GetCount();
            }
            m_pTE->CodecPrivate.SetCount(len);

            BYTE* dst = m_pTE->CodecPrivate.GetData();

            *dst++ = 2;
            for (size_t i = 0; i < 2; i++) {
                for (INT_PTR len2 = m_pVorbisHdrs[i]->GetCount(); len2 >= 0; len2 -= 255) {
                    *dst++ = (BYTE)std::min<INT_PTR>(len2, 255);
                }
            }

            for (size_t i = 0; i < 3; i++) {
                memcpy(dst, m_pVorbisHdrs[i]->GetData(), m_pVorbisHdrs[i]->GetCount());
                dst += m_pVorbisHdrs[i]->GetCount();
            }
        }

        return S_OK;
    }

    if (m_mt.formattype == FORMAT_WaveFormatEx
            && (((WAVEFORMATEX*)m_mt.pbFormat)->wFormatTag == WAVE_FORMAT_PCM
                || ((WAVEFORMATEX*)m_mt.pbFormat)->wFormatTag == WAVE_FORMAT_MPEGLAYER3)) {
        pSample->SetSyncPoint(TRUE);    // HACK: some capture filters don't set this
    }

    CAutoPtr<BlockGroup> b(DEBUG_NEW BlockGroup());
    /*
            // TODO: test this with a longer capture (pcm, mp3)
            if (S_OK == pSample->IsSyncPoint() && rtStart < m_rtLastStart)
            {
                TRACE(_T("!!! timestamp went backwards, dropping this frame !!! rtStart (%I64) < m_rtLastStart (%I64)"), rtStart, m_rtLastStart);
                return S_OK;
            }
    */
    if ((S_OK != pSample->IsSyncPoint() || m_rtLastStart == rtStart) && m_rtLastStart >= 0 /*&& m_rtLastStart < rtStart*/) {
        ASSERT(m_rtLastStart - rtStart <= 0);
        REFERENCE_TIME rtDiff = m_rtLastStart - rtStart;
        b->ReferenceBlock.Set((rtDiff + (rtDiff >= 0 ? 5000 : -5000)) / 10000);
    }

    b->Block.TrackNumber = (static_cast<CMatroskaMuxerFilter*>(m_pFilter))->GetTrackNumber(this);

    b->Block.TimeCode = (rtStart + 5000) / 10000;
    b->Block.TimeCodeStop = (rtStop + 5000) / 10000;

    if (m_pTE->TrackType == TrackEntry::TypeSubtitle) {
        b->BlockDuration.Set((rtStop - rtStart + 5000) / 10000);
    }

    CAutoPtr<CBinary> data(DEBUG_NEW CBinary(0));
    data->SetCount(inputLen);
    memcpy(data->GetData(), pData, inputLen);
    b->Block.BlockData.AddTail(data);

    CAutoLock cAutoLock2(&m_csQueue);
    m_blocks.AddTail(b); // TODO: lacing for audio

    m_rtLastStart = rtStart;
    m_rtLastStop = rtStop;

    return S_OK;
}

STDMETHODIMP CMatroskaMuxerInputPin::EndOfStream()
{
    HRESULT hr;

    if (FAILED(hr = __super::EndOfStream())) {
        return hr;
    }

    CAutoLock cAutoLock(&m_csQueue);

    m_fEndOfStreamReceived = true;

    return hr;
}

//
// CMatroskaMuxerOutputPin
//

CMatroskaMuxerOutputPin::CMatroskaMuxerOutputPin(LPCTSTR pName, CBaseFilter* pFilter, CCritSec* pLock, HRESULT* phr)
    : CBaseOutputPin(pName, pFilter, pLock, phr, L"Output")
{
}

CMatroskaMuxerOutputPin::~CMatroskaMuxerOutputPin()
{
}

STDMETHODIMP CMatroskaMuxerOutputPin::NonDelegatingQueryInterface(REFIID riid, void** ppv)
{
    CheckPointer(ppv, E_POINTER);

    return
        __super::NonDelegatingQueryInterface(riid, ppv);
}

HRESULT CMatroskaMuxerOutputPin::DecideBufferSize(IMemAllocator* pAlloc, ALLOCATOR_PROPERTIES* pProperties)
{
    ASSERT(pAlloc);
    ASSERT(pProperties);

    HRESULT hr = NOERROR;

    pProperties->cBuffers = 1;
    pProperties->cbBuffer = 1;

    ALLOCATOR_PROPERTIES Actual;
    if (FAILED(hr = pAlloc->SetProperties(pProperties, &Actual))) {
        return hr;
    }

    if (Actual.cbBuffer < pProperties->cbBuffer) {
        return E_FAIL;
    }
    ASSERT(Actual.cBuffers == pProperties->cBuffers);

    return NOERROR;
}

HRESULT CMatroskaMuxerOutputPin::CheckMediaType(const CMediaType* pmt)
{
    return pmt->majortype == MEDIATYPE_Stream && pmt->subtype == MEDIASUBTYPE_Matroska
           ? S_OK
           : E_INVALIDARG;
}

HRESULT CMatroskaMuxerOutputPin::GetMediaType(int iPosition, CMediaType* pmt)
{
    CAutoLock cAutoLock(m_pLock);

    if (iPosition < 0) {
        return E_INVALIDARG;
    }
    if (iPosition > 0) {
        return VFW_S_NO_MORE_ITEMS;
    }

    pmt->ResetFormatBuffer();
    pmt->InitMediaType();
    pmt->majortype = MEDIATYPE_Stream;
    pmt->subtype = MEDIASUBTYPE_Matroska;
    pmt->formattype = FORMAT_None;

    return S_OK;
}

STDMETHODIMP CMatroskaMuxerOutputPin::Notify(IBaseFilter* pSender, Quality q)
{
    return E_NOTIMPL;
}
