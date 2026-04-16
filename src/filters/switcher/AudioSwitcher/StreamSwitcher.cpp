/*
 * (C) 2003-2006 Gabest
 * (C) 2006-2015, 2017 see Authors.txt
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
#include "StreamSwitcher.h"
#include "../../../DSUtil/DSUtil.h"
#include "../../../DSUtil/PathUtils.h"

#ifdef STANDALONE_FILTER
#include <InitGuid.h>
#endif
#include "moreuuids.h"

#define BLOCKSTREAM

//
// CStreamSwitcherPassThru
//

CStreamSwitcherPassThru::CStreamSwitcherPassThru(LPUNKNOWN pUnk, HRESULT* phr, CStreamSwitcherFilter* pFilter)
    : CMediaPosition(NAME("CStreamSwitcherPassThru"), pUnk)
    , m_pFilter(pFilter)
{
}

STDMETHODIMP CStreamSwitcherPassThru::NonDelegatingQueryInterface(REFIID riid, void** ppv)
{
    CheckPointer(ppv, E_POINTER);
    *ppv = nullptr;

    return
        QI(IMediaSeeking)
        CMediaPosition::NonDelegatingQueryInterface(riid, ppv);
}

template<class T>
HRESULT GetPeer(CStreamSwitcherFilter* pFilter, T** ppT)
{
    *ppT = nullptr;

    CBasePin* pPin = pFilter->GetInputPin();
    if (!pPin) {
        return E_NOTIMPL;
    }

    CComPtr<IPin> pConnected;
    if (FAILED(pPin->ConnectedTo(&pConnected))) {
        return E_NOTIMPL;
    }

    if (CComQIPtr<T> pT = pConnected) {
        *ppT = pT.Detach();
        return S_OK;
    }

    return E_NOTIMPL;
}


#define CallPeerSeeking(call)                               \
    CComPtr<IMediaSeeking> pMS;                             \
    if (FAILED(GetPeer(m_pFilter, &pMS)))                   \
        return E_NOTIMPL;                                   \
    return pMS->##call;

#define CallPeer(call)                                      \
    CComPtr<IMediaPosition> pMP;                            \
    if (FAILED(GetPeer(m_pFilter, &pMP)))                   \
        return E_NOTIMPL;                                   \
    return pMP->##call;

#define CallPeerSeekingAll(call)                            \
    HRESULT hr = E_NOTIMPL;                                 \
    POSITION pos = m_pFilter->m_pInputs.GetHeadPosition();  \
    while (pos) {                                           \
        CBasePin* pPin = m_pFilter->m_pInputs.GetNext(pos); \
        CComPtr<IPin> pConnected;                           \
        if (FAILED(pPin->ConnectedTo(&pConnected)))         \
            continue;                                       \
        if (CComQIPtr<IMediaSeeking> pMS = pConnected) {    \
            HRESULT hr2 = pMS->call;                        \
            if (pPin == m_pFilter->GetInputPin())           \
                hr = hr2;                                   \
        }                                                   \
    }                                                       \
    return hr;

#define CallPeerAll(call)                                   \
    HRESULT hr = E_NOTIMPL;                                 \
    POSITION pos = m_pFilter->m_pInputs.GetHeadPosition();  \
    while (pos) {                                           \
        CBasePin* pPin = m_pFilter->m_pInputs.GetNext(pos); \
        CComPtr<IPin> pConnected;                           \
        if (FAILED(pPin->ConnectedTo(&pConnected)))         \
            continue;                                       \
        if (CComQIPtr<IMediaPosition> pMP = pConnected) {   \
            HRESULT hr2 = pMP->call;                        \
            if (pPin == m_pFilter->GetInputPin())           \
                hr = hr2;                                   \
        }                                                   \
    }                                                       \
    return hr;


// IMediaSeeking

STDMETHODIMP CStreamSwitcherPassThru::GetCapabilities(DWORD* pCaps)
{
    CallPeerSeeking(GetCapabilities(pCaps));
}

STDMETHODIMP CStreamSwitcherPassThru::CheckCapabilities(DWORD* pCaps)
{
    CallPeerSeeking(CheckCapabilities(pCaps));
}

STDMETHODIMP CStreamSwitcherPassThru::IsFormatSupported(const GUID* pFormat)
{
    CallPeerSeeking(IsFormatSupported(pFormat));
}

STDMETHODIMP CStreamSwitcherPassThru::QueryPreferredFormat(GUID* pFormat)
{
    CallPeerSeeking(QueryPreferredFormat(pFormat));
}

STDMETHODIMP CStreamSwitcherPassThru::SetTimeFormat(const GUID* pFormat)
{
    CallPeerSeeking(SetTimeFormat(pFormat));
}

STDMETHODIMP CStreamSwitcherPassThru::GetTimeFormat(GUID* pFormat)
{
    CallPeerSeeking(GetTimeFormat(pFormat));
}

STDMETHODIMP CStreamSwitcherPassThru::IsUsingTimeFormat(const GUID* pFormat)
{
    CallPeerSeeking(IsUsingTimeFormat(pFormat));
}

STDMETHODIMP CStreamSwitcherPassThru::ConvertTimeFormat(LONGLONG* pTarget, const GUID* pTargetFormat, LONGLONG Source, const GUID* pSourceFormat)
{
    CallPeerSeeking(ConvertTimeFormat(pTarget, pTargetFormat, Source, pSourceFormat));
}

STDMETHODIMP CStreamSwitcherPassThru::SetPositions(LONGLONG* pCurrent, DWORD CurrentFlags, LONGLONG* pStop, DWORD StopFlags)
{
    CallPeerSeekingAll(SetPositions(pCurrent, CurrentFlags, pStop, StopFlags));
}

STDMETHODIMP CStreamSwitcherPassThru::GetPositions(LONGLONG* pCurrent, LONGLONG* pStop)
{
    CallPeerSeeking(GetPositions(pCurrent, pStop));
}

STDMETHODIMP CStreamSwitcherPassThru::GetCurrentPosition(LONGLONG* pCurrent)
{
    CallPeerSeeking(GetCurrentPosition(pCurrent));
}

STDMETHODIMP CStreamSwitcherPassThru::GetStopPosition(LONGLONG* pStop)
{
    CallPeerSeeking(GetStopPosition(pStop));
}

STDMETHODIMP CStreamSwitcherPassThru::GetDuration(LONGLONG* pDuration)
{
    CallPeerSeeking(GetDuration(pDuration));
}

STDMETHODIMP CStreamSwitcherPassThru::GetPreroll(LONGLONG* pllPreroll)
{
    CallPeerSeeking(GetPreroll(pllPreroll));
}

STDMETHODIMP CStreamSwitcherPassThru::GetAvailable(LONGLONG* pEarliest, LONGLONG* pLatest)
{
    CallPeerSeeking(GetAvailable(pEarliest, pLatest));
}

STDMETHODIMP CStreamSwitcherPassThru::GetRate(double* pdRate)
{
    CallPeerSeeking(GetRate(pdRate));
}

STDMETHODIMP CStreamSwitcherPassThru::SetRate(double dRate)
{
    if (0.0 == dRate) {
        return E_INVALIDARG;
    }
    CallPeerSeekingAll(SetRate(dRate));
}

// IMediaPosition

STDMETHODIMP CStreamSwitcherPassThru::get_Duration(REFTIME* plength)
{
    CallPeer(get_Duration(plength));
}

STDMETHODIMP CStreamSwitcherPassThru::get_CurrentPosition(REFTIME* pllTime)
{
    CallPeer(get_CurrentPosition(pllTime));
}

STDMETHODIMP CStreamSwitcherPassThru::put_CurrentPosition(REFTIME llTime)
{
    CallPeerAll(put_CurrentPosition(llTime));
}

STDMETHODIMP CStreamSwitcherPassThru::get_StopTime(REFTIME* pllTime)
{
    CallPeer(get_StopTime(pllTime));
}

STDMETHODIMP CStreamSwitcherPassThru::put_StopTime(REFTIME llTime)
{
    CallPeerAll(put_StopTime(llTime));
}

STDMETHODIMP CStreamSwitcherPassThru::get_PrerollTime(REFTIME* pllTime)
{
    CallPeer(get_PrerollTime(pllTime));
}

STDMETHODIMP CStreamSwitcherPassThru::put_PrerollTime(REFTIME llTime)
{
    CallPeerAll(put_PrerollTime(llTime));
}

STDMETHODIMP CStreamSwitcherPassThru::get_Rate(double* pdRate)
{
    CallPeer(get_Rate(pdRate));
}

STDMETHODIMP CStreamSwitcherPassThru::put_Rate(double dRate)
{
    if (0.0 == dRate) {
        return E_INVALIDARG;
    }
    CallPeerAll(put_Rate(dRate));
}

STDMETHODIMP CStreamSwitcherPassThru::CanSeekForward(LONG* pCanSeekForward)
{
    CallPeer(CanSeekForward(pCanSeekForward));
}

STDMETHODIMP CStreamSwitcherPassThru::CanSeekBackward(LONG* pCanSeekBackward)
{
    CallPeer(CanSeekBackward(pCanSeekBackward));
}

//
// CStreamSwitcherAllocator
//

CStreamSwitcherAllocator::CStreamSwitcherAllocator(CStreamSwitcherInputPin* pPin, HRESULT* phr)
    : CMemAllocator(NAME("CStreamSwitcherAllocator"), nullptr, phr)
    , m_pPin(pPin)
    , m_fMediaTypeChanged(false)
{
    ASSERT(phr);
    ASSERT(pPin);
}

#ifdef _DEBUG
CStreamSwitcherAllocator::~CStreamSwitcherAllocator()
{
    ASSERT(m_bCommitted == FALSE);
}
#endif

STDMETHODIMP_(ULONG) CStreamSwitcherAllocator::NonDelegatingAddRef()
{
    return m_pPin->m_pFilter->AddRef();
}

STDMETHODIMP_(ULONG) CStreamSwitcherAllocator::NonDelegatingRelease()
{
    return m_pPin->m_pFilter->Release();
}

STDMETHODIMP CStreamSwitcherAllocator::GetBuffer(
    IMediaSample** ppBuffer,
    REFERENCE_TIME* pStartTime, REFERENCE_TIME* pEndTime,
    DWORD dwFlags)
{
    HRESULT hr = VFW_E_NOT_COMMITTED;

    if (!m_bCommitted) {
        return hr;
    }
    /*
    TRACE(_T("CStreamSwitcherAllocator::GetBuffer m_pPin->m_evBlock.Wait() + %x\n"), this);
        m_pPin->m_evBlock.Wait();
    TRACE(_T("CStreamSwitcherAllocator::GetBuffer m_pPin->m_evBlock.Wait() - %x\n"), this);
    */
    if (m_fMediaTypeChanged) {
        if (!m_pPin || !m_pPin->m_pFilter) {
            return hr;
        }

        CStreamSwitcherOutputPin* pOut = (static_cast<CStreamSwitcherFilter*>(m_pPin->m_pFilter))->GetOutputPin();
        if (!pOut || !pOut->CurrentAllocator()) {
            return hr;
        }

        ALLOCATOR_PROPERTIES Properties, Actual;
        if (FAILED(pOut->CurrentAllocator()->GetProperties(&Actual))) {
            return hr;
        }
        if (FAILED(GetProperties(&Properties))) {
            return hr;
        }

        if (!m_bCommitted || Properties.cbBuffer < Actual.cbBuffer) {
            Properties.cbBuffer = Actual.cbBuffer;
            if (FAILED(Decommit())) {
                return hr;
            }
            if (FAILED(SetProperties(&Properties, &Actual))) {
                return hr;
            }
            if (FAILED(Commit())) {
                return hr;
            }
            ASSERT(Actual.cbBuffer >= Properties.cbBuffer);
            if (Actual.cbBuffer < Properties.cbBuffer) {
                return hr;
            }
        }
    }

    hr = CMemAllocator::GetBuffer(ppBuffer, pStartTime, pEndTime, dwFlags);

    if (m_fMediaTypeChanged && SUCCEEDED(hr)) {
        (*ppBuffer)->SetMediaType(&m_mt);
        m_fMediaTypeChanged = false;
    }

    return hr;
}

void CStreamSwitcherAllocator::NotifyMediaType(const CMediaType& mt)
{
    CopyMediaType(&m_mt, &mt);
    m_fMediaTypeChanged = true;
}


//
// CStreamSwitcherInputPin
//

CStreamSwitcherInputPin::CStreamSwitcherInputPin(CStreamSwitcherFilter* pFilter, HRESULT* phr, LPCWSTR pName)
    : CBaseInputPin(NAME("CStreamSwitcherInputPin"), pFilter, &pFilter->m_csState, phr, pName)
    , m_Allocator(this, phr)
    , m_bSampleSkipped(FALSE)
    , m_bQualityChanged(FALSE)
    , m_bUsingOwnAllocator(FALSE)
    , m_evBlock(TRUE)
    , m_fCanBlock(false)
    , m_hNotifyEvent(nullptr)
    , m_bFirstSampleReceived(false)
{
    m_bCanReconnectWhenActive = true;
}

class __declspec(uuid("138130AF-A79B-45D5-B4AA-87697457BA87"))
    NeroAudioDecoder {};

STDMETHODIMP CStreamSwitcherInputPin::NonDelegatingQueryInterface(REFIID riid, void** ppv)
{
    return
        QI(IStreamSwitcherInputPin)
        IsConnected() && GetCLSID(GetFilterFromPin(GetConnected())) == __uuidof(NeroAudioDecoder) && QI(IPinConnection)
        __super::NonDelegatingQueryInterface(riid, ppv);
}

// IPinConnection

STDMETHODIMP CStreamSwitcherInputPin::DynamicQueryAccept(const AM_MEDIA_TYPE* pmt)
{
    return QueryAccept(pmt);
}

STDMETHODIMP CStreamSwitcherInputPin::NotifyEndOfStream(HANDLE hNotifyEvent)
{
    if (m_hNotifyEvent) {
        SetEvent(m_hNotifyEvent);
    }
    m_hNotifyEvent = hNotifyEvent;
    return S_OK;
}

STDMETHODIMP CStreamSwitcherInputPin::IsEndPin()
{
    return S_OK;
}

STDMETHODIMP CStreamSwitcherInputPin::DynamicDisconnect()
{
    CAutoLock cAutoLock(&m_csReceive);
    Disconnect();
    return S_OK;
}

// IStreamSwitcherInputPin

STDMETHODIMP_(bool) CStreamSwitcherInputPin::IsActive()
{
    // TODO: lock onto something here
    return (this == (static_cast<CStreamSwitcherFilter*>(m_pFilter))->GetInputPin());
}

//

HRESULT CStreamSwitcherInputPin::QueryAcceptDownstream(const AM_MEDIA_TYPE* pmt)
{
    HRESULT hr = S_OK;

    CStreamSwitcherOutputPin* pOut = (static_cast<CStreamSwitcherFilter*>(m_pFilter))->GetOutputPin();

    if (pOut && pOut->IsConnected()) {
        if (CComPtr<IPinConnection> pPC = pOut->CurrentPinConnection()) {
            hr = pPC->DynamicQueryAccept(pmt);
            if (hr == S_OK) {
                return S_OK;
            }
        }

        hr = pOut->GetConnected()->QueryAccept(pmt);
    }

    return hr;
}

void CStreamSwitcherInputPin::Block(bool fBlock)
{
    if (fBlock) {
        m_evBlock.Reset();
    } else {
        m_evBlock.Set();
    }
}

HRESULT CStreamSwitcherInputPin::InitializeOutputSample(IMediaSample* pInSample, IMediaSample** ppOutSample)
{
    if (!pInSample || !ppOutSample) {
        return E_POINTER;
    }

    CStreamSwitcherOutputPin* pOut = (static_cast<CStreamSwitcherFilter*>(m_pFilter))->GetOutputPin();
    ASSERT(pOut->GetConnected());

    CComPtr<IMediaSample> pOutSample;

    DWORD dwFlags = m_bSampleSkipped ? AM_GBF_PREVFRAMESKIPPED : 0;

    if (!(m_SampleProps.dwSampleFlags & AM_SAMPLE_SPLICEPOINT)) {
        dwFlags |= AM_GBF_NOTASYNCPOINT;
    }

    HRESULT hr = pOut->GetDeliveryBuffer(&pOutSample,
                                         (m_SampleProps.dwSampleFlags & AM_SAMPLE_TIMEVALID) ? &m_SampleProps.tStart : nullptr,
                                         (m_SampleProps.dwSampleFlags & AM_SAMPLE_STOPVALID) ? &m_SampleProps.tStop : nullptr,
                                         dwFlags);

    if (FAILED(hr)) {
        return hr;
    }

    if (!pOutSample) {
        return E_FAIL;
    }

    if (CComQIPtr<IMediaSample2> pOutSample2 = pOutSample) {
        AM_SAMPLE2_PROPERTIES OutProps;
        EXECUTE_ASSERT(SUCCEEDED(pOutSample2->GetProperties(FIELD_OFFSET(AM_SAMPLE2_PROPERTIES, tStart), (PBYTE)&OutProps)));
        OutProps.dwTypeSpecificFlags = m_SampleProps.dwTypeSpecificFlags;
        OutProps.dwSampleFlags =
            (OutProps.dwSampleFlags & AM_SAMPLE_TYPECHANGED) |
            (m_SampleProps.dwSampleFlags & ~AM_SAMPLE_TYPECHANGED);

        OutProps.tStart = m_SampleProps.tStart;
        OutProps.tStop  = m_SampleProps.tStop;
        OutProps.cbData = FIELD_OFFSET(AM_SAMPLE2_PROPERTIES, dwStreamId);

        pOutSample2->SetProperties(FIELD_OFFSET(AM_SAMPLE2_PROPERTIES, dwStreamId), (PBYTE)&OutProps);
        if (m_SampleProps.dwSampleFlags & AM_SAMPLE_DATADISCONTINUITY) {
            m_bSampleSkipped = FALSE;
        }
    } else {
        if (m_SampleProps.dwSampleFlags & AM_SAMPLE_TIMEVALID) {
            pOutSample->SetTime(&m_SampleProps.tStart, &m_SampleProps.tStop);
        }

        if (m_SampleProps.dwSampleFlags & AM_SAMPLE_SPLICEPOINT) {
            pOutSample->SetSyncPoint(TRUE);
        }

        if (m_SampleProps.dwSampleFlags & AM_SAMPLE_DATADISCONTINUITY) {
            pOutSample->SetDiscontinuity(TRUE);
            m_bSampleSkipped = FALSE;
        }

        LONGLONG MediaStart, MediaEnd;
        if (pInSample->GetMediaTime(&MediaStart, &MediaEnd) == NOERROR) {
            pOutSample->SetMediaTime(&MediaStart, &MediaEnd);
        }
    }

    *ppOutSample = pOutSample.Detach();

    return S_OK;
}

// pure virtual

HRESULT CStreamSwitcherInputPin::CheckMediaType(const CMediaType* pmt)
{
    return (static_cast<CStreamSwitcherFilter*>(m_pFilter))->CheckMediaType(pmt);
}

// virtual

HRESULT CStreamSwitcherInputPin::CheckConnect(IPin* pPin)
{
    return (IPin*)(static_cast<CStreamSwitcherFilter*>(m_pFilter))->GetOutputPin() == pPin
           ? E_FAIL
           : __super::CheckConnect(pPin);
}

HRESULT CStreamSwitcherInputPin::CompleteConnect(IPin* pReceivePin)
{
    HRESULT hr = __super::CompleteConnect(pReceivePin);
    if (FAILED(hr)) {
        return hr;
    }

    (static_cast<CStreamSwitcherFilter*>(m_pFilter))->CompleteConnect(PINDIR_INPUT, this, pReceivePin);

    m_fCanBlock = false;
    bool fForkedSomewhere = false;

    CStringW streamName;
    CStringW pinName;

    IPin* pPin = (IPin*)this;
    IBaseFilter* pBF = (IBaseFilter*)m_pFilter;

    pPin = GetUpStreamPin(pBF, pPin);
    if (pPin) {
        pBF = GetFilterFromPin(pPin);
    }
    while (pPin && pBF) {
        if (IsSplitter(pBF)) {
            pinName = GetPinName(pPin);
        }

        CLSID clsid = GetCLSID(pBF);
        if (clsid == CLSID_AviSplitter || clsid == CLSID_OggSplitter) {
            m_fCanBlock = true;
        }

        int nIn, nOut, nInC, nOutC;
        CountPins(pBF, nIn, nOut, nInC, nOutC);
        fForkedSomewhere = fForkedSomewhere || nIn > 1 || nOut > 1;

        DWORD cStreams = 0;
        CStringW streamSSName;
        if (CComQIPtr<IAMStreamSelect> pSSF = pBF) {
            hr = pSSF->Count(&cStreams);
            if (SUCCEEDED(hr)) {
                for (int i = 0; i < (int)cStreams; i++) {
                    CComHeapPtr<WCHAR> pszNameSS;
                    AM_MEDIA_TYPE* pmt = nullptr;
                    LCID lcid = 0;

                    hr = pSSF->Info(i, &pmt, nullptr, &lcid, nullptr, &pszNameSS, nullptr, nullptr);
                    if (SUCCEEDED(hr) && pmt && pmt->majortype == MEDIATYPE_Audio) {
                        m_pSSF = pSSF;
                        DeleteMediaType(pmt);
                        streamSSName = pszNameSS;
                        break;
                    }
                    DeleteMediaType(pmt);
                }
            }
        }

        if (CComQIPtr<IFileSourceFilter> pFSF = pBF) {
            CComHeapPtr<WCHAR> fileName;
            AM_MEDIA_TYPE mt;

            if (SUCCEEDED(pFSF->GetCurFile(&fileName, &mt)) && fileName) {
                streamName = fileName;
                if (streamName.Find(L"googlevideo.com") > 0 || (PathUtils::IsURL(streamName) && streamName.GetLength() > 50)) { //we don't like these URLs
                    streamName = streamSSName;
                    if (streamName.GetLength() <= 0) {
                        streamName = L"Online Audio Stream";
                    }
                } else {
                    streamName.Replace('\\', '/');
                    CStringW fn = streamName.Mid(streamName.ReverseFind('/') + 1);
                    if (!fn.IsEmpty()) {
                        streamName = fn;
                    }

                    // Haali & LAVFSplitter return only one "Audio" pin name, cause CMainFrame::OnInitMenuPopup lookup find the wrong popmenu,
                    // add space at the end to prevent this, internal filter never return "Audio" only.
                    if (!pinName.IsEmpty()) {
                        streamName = pinName + L" ";
                    }
                }

                WCHAR* pName = DEBUG_NEW WCHAR[streamName.GetLength() + 1];
                if (pName) {
                    wcscpy_s(pName, streamName.GetLength() + 1, streamName);
                    if (m_pName) {
                        delete[] m_pName;
                    }
                    m_pName = pName;
                    if (cStreams == 1) { // Simple external track, no need to use the info from IAMStreamSelect
                        m_pSSF.Release();
                    }
                }
            }

            break;
        }

        pPin = GetFirstPin(pBF);

        pPin = GetUpStreamPin(pBF, pPin);
        if (pPin) {
            pBF = GetFilterFromPin(pPin);
        }
    }

    if (!fForkedSomewhere) {
        m_fCanBlock = true;
    }

    m_hNotifyEvent = nullptr;

    return S_OK;
}

HRESULT CStreamSwitcherInputPin::Active()
{
    Block(!IsActive());

    return __super::Active();
}

HRESULT CStreamSwitcherInputPin::Inactive()
{
    Block(false);

    return __super::Inactive();
}

// IPin

STDMETHODIMP CStreamSwitcherInputPin::QueryAccept(const AM_MEDIA_TYPE* pmt)
{
    HRESULT hr = __super::QueryAccept(pmt);
    if (S_OK != hr) {
        return hr;
    }

    return QueryAcceptDownstream(pmt);
}

STDMETHODIMP CStreamSwitcherInputPin::ReceiveConnection(IPin* pConnector, const AM_MEDIA_TYPE* pmt)
{
    // FIXME: this locked up once
    //    CAutoLock cAutoLock(&((CStreamSwitcherFilter*)m_pFilter)->m_csReceive);

    HRESULT hr;
    if (S_OK != (hr = QueryAcceptDownstream(pmt))) {
        return VFW_E_TYPE_NOT_ACCEPTED;
    }

    if (m_Connected && m_Connected != pConnector) {
        return VFW_E_ALREADY_CONNECTED;
    }

    SAFE_RELEASE(m_Connected);

    return SUCCEEDED(__super::ReceiveConnection(pConnector, pmt)) ? S_OK : E_FAIL;
}

STDMETHODIMP CStreamSwitcherInputPin::GetAllocator(IMemAllocator** ppAllocator)
{
    CheckPointer(ppAllocator, E_POINTER);

    if (m_pAllocator == nullptr) {
        (m_pAllocator = &m_Allocator)->AddRef();
    }

    (*ppAllocator = m_pAllocator)->AddRef();

    return NOERROR;
}

STDMETHODIMP CStreamSwitcherInputPin::NotifyAllocator(IMemAllocator* pAllocator, BOOL bReadOnly)
{
    HRESULT hr = __super::NotifyAllocator(pAllocator, bReadOnly);
    if (FAILED(hr)) {
        return hr;
    }

    m_bUsingOwnAllocator = (pAllocator == (IMemAllocator*)&m_Allocator);

    return S_OK;
}

STDMETHODIMP CStreamSwitcherInputPin::BeginFlush()
{
    CAutoLock cAutoLock(&(static_cast<CStreamSwitcherFilter*>(m_pFilter))->m_csState);

    HRESULT hr;

    CStreamSwitcherFilter* pSSF = static_cast<CStreamSwitcherFilter*>(m_pFilter);

    CStreamSwitcherOutputPin* pOut = pSSF->GetOutputPin();
    if (!IsConnected() || !pOut || !pOut->IsConnected()) {
        return VFW_E_NOT_CONNECTED;
    }

    if (FAILED(hr = __super::BeginFlush())) {
        return hr;
    }

    return IsActive() ? pSSF->DeliverBeginFlush() : (Block(false), S_OK);
}

STDMETHODIMP CStreamSwitcherInputPin::EndFlush()
{
    CAutoLock cAutoLock(&(static_cast<CStreamSwitcherFilter*>(m_pFilter))->m_csState);

    HRESULT hr;

    CStreamSwitcherFilter* pSSF = static_cast<CStreamSwitcherFilter*>(m_pFilter);

    CStreamSwitcherOutputPin* pOut = pSSF->GetOutputPin();
    if (!IsConnected() || !pOut || !pOut->IsConnected()) {
        return VFW_E_NOT_CONNECTED;
    }

    if (FAILED(hr = __super::EndFlush())) {
        return hr;
    }

    return IsActive() ? pSSF->DeliverEndFlush() : (Block(true), S_OK);
}

STDMETHODIMP CStreamSwitcherInputPin::EndOfStream()
{
    CAutoLock cAutoLock(&m_csReceive);

    CStreamSwitcherFilter* pSSF = static_cast<CStreamSwitcherFilter*>(m_pFilter);

    CStreamSwitcherOutputPin* pOut = pSSF->GetOutputPin();
    if (!IsConnected() || !pOut || !pOut->IsConnected()) {
        return VFW_E_NOT_CONNECTED;
    }

    if (m_hNotifyEvent) {
        SetEvent(m_hNotifyEvent), m_hNotifyEvent = nullptr;
        return S_OK;
    }

    if (!m_bFirstSampleReceived) {
        pSSF->Stop();
    }
    return IsActive() ? pSSF->DeliverEndOfStream() : S_OK;
}

// IMemInputPin

STDMETHODIMP CStreamSwitcherInputPin::Receive(IMediaSample* pSample)
{
    m_bFirstSampleReceived = true;
    AM_MEDIA_TYPE* pmt = nullptr;
    if (SUCCEEDED(pSample->GetMediaType(&pmt)) && pmt) {
        const CMediaType mt(*pmt);
        DeleteMediaType(pmt), pmt = nullptr;
        SetMediaType(&mt);
    }

    // DAMN!!!!!! this doesn't work if the stream we are blocking
    // shares the same thread with another stream, mpeg splitters
    // are usually like that. Our nicely built up multithreaded
    // strategy is useless because of this, ARRRRRRGHHHHHH.

#ifdef BLOCKSTREAM
    if (m_fCanBlock) {
        m_evBlock.Wait();
    }
#endif

    if (!IsActive()) {
#ifdef BLOCKSTREAM
        if (m_fCanBlock) {
            return S_FALSE;
        }
#endif

        TRACE(_T("&^$#@ : a stupid fix for this stupid problem\n"));
        //Sleep(32);
        return E_FAIL; // a stupid fix for this stupid problem
    }

    CAutoLock cAutoLock(&m_csReceive);

    CStreamSwitcherOutputPin* pOut = (static_cast<CStreamSwitcherFilter*>(m_pFilter))->GetOutputPin();
    ASSERT(pOut->GetConnected());

    HRESULT hr = __super::Receive(pSample);
    if (S_OK != hr) {
        return hr;
    }

    if (m_SampleProps.dwStreamId != AM_STREAM_MEDIA) {
        return pOut->Deliver(pSample);
    }

    //

    ALLOCATOR_PROPERTIES props, actual;
    m_pAllocator->GetProperties(&props);
    IMemAllocator* cur_alloc = pOut->CurrentAllocator();
    if (!cur_alloc) {
        ASSERT(false);
        return E_FAIL;
    }
    cur_alloc->GetProperties(&actual);

    REFERENCE_TIME rtStart = 0, rtStop = 0;
    if (S_OK == pSample->GetTime(&rtStart, &rtStop)) {
        //
    }

    long cbBuffer = pSample->GetActualDataLength();

    CMediaType mtOut = m_mt;
    mtOut = (static_cast<CStreamSwitcherFilter*>(m_pFilter))->CreateNewOutputMediaType(mtOut, cbBuffer);

    bool fTypeChanged = false;

    if (mtOut != pOut->CurrentMediaType() || cbBuffer > actual.cbBuffer) {
        fTypeChanged = true;

        m_SampleProps.dwSampleFlags |= AM_SAMPLE_TYPECHANGED/*|AM_SAMPLE_DATADISCONTINUITY|AM_SAMPLE_TIMEDISCONTINUITY*/;

        if (props.cBuffers < 8 && mtOut.majortype == MEDIATYPE_Audio) {
            props.cBuffers = 8;
        }

        props.cbBuffer = cbBuffer;

        if (actual.cbAlign != props.cbAlign
                || actual.cbPrefix != props.cbPrefix
                || actual.cBuffers < props.cBuffers
                || actual.cbBuffer < props.cbBuffer) {
            pOut->DeliverBeginFlush();
            pOut->DeliverEndFlush();
            pOut->CurrentAllocator()->Decommit();
            pOut->CurrentAllocator()->SetProperties(&props, &actual);
            pOut->CurrentAllocator()->Commit();
        }
    }

    CComPtr<IMediaSample> pOutSample;
    if (FAILED(InitializeOutputSample(pSample, &pOutSample))) {
        return E_FAIL;
    }

    pmt = nullptr;
    if (SUCCEEDED(pOutSample->GetMediaType(&pmt)) && pmt) {
        const CMediaType mt(*pmt);
        DeleteMediaType(pmt), pmt = nullptr;
        // TODO
        ASSERT(0);
    }

    if (fTypeChanged) {
        pOut->SetMediaType(&mtOut);
        (static_cast<CStreamSwitcherFilter*>(m_pFilter))->OnNewOutputMediaType(m_mt, mtOut);
        pOutSample->SetMediaType(&mtOut);
    }

    // Transform

    hr = (static_cast<CStreamSwitcherFilter*>(m_pFilter))->Transform(pSample, pOutSample);

    if (S_OK == hr) {
        hr = pOut->Deliver(pOutSample);
        m_bSampleSkipped = FALSE;
    } else if (S_FALSE == hr) {
        hr = S_OK;
        pOutSample = nullptr;
        m_bSampleSkipped = TRUE;

        if (!m_bQualityChanged) {
            m_pFilter->NotifyEvent(EC_QUALITY_CHANGE, 0, 0);
            m_bQualityChanged = TRUE;
        }
    }

    return hr;
}

STDMETHODIMP CStreamSwitcherInputPin::NewSegment(REFERENCE_TIME tStart, REFERENCE_TIME tStop, double dRate)
{
    if (!IsConnected() || !IsActive()) {
        return S_OK;
    }

    CAutoLock cAutoLock(&m_csReceive);

    CStreamSwitcherFilter* pSSF = static_cast<CStreamSwitcherFilter*>(m_pFilter);

    CStreamSwitcherOutputPin* pOut = pSSF->GetOutputPin();
    if (!pOut || !pOut->IsConnected()) {
        return VFW_E_NOT_CONNECTED;
    }

    return pSSF->DeliverNewSegment(tStart, tStop, dRate);
}

//
// CStreamSwitcherOutputPin
//

CStreamSwitcherOutputPin::CStreamSwitcherOutputPin(CStreamSwitcherFilter* pFilter, HRESULT* phr)
    : CBaseOutputPin(NAME("CStreamSwitcherOutputPin"), pFilter, &pFilter->m_csState, phr, L"Out")
{
    //  m_bCanReconnectWhenActive = true;
}

STDMETHODIMP CStreamSwitcherOutputPin::NonDelegatingQueryInterface(REFIID riid, void** ppv)
{
    CheckPointer(ppv, E_POINTER);
    ValidateReadWritePtr(ppv, sizeof(PVOID));
    *ppv = nullptr;

    if (riid == IID_IMediaPosition || riid == IID_IMediaSeeking) {
        if (m_pStreamSwitcherPassThru == nullptr) {
            HRESULT hr = S_OK;
            m_pStreamSwitcherPassThru = (IUnknown*)(INonDelegatingUnknown*)
                                        DEBUG_NEW CStreamSwitcherPassThru(GetOwner(), &hr, static_cast<CStreamSwitcherFilter*>(m_pFilter));

            if (!m_pStreamSwitcherPassThru) {
                return E_OUTOFMEMORY;
            }
            if (FAILED(hr)) {
                return hr;
            }
        }

        return m_pStreamSwitcherPassThru->QueryInterface(riid, ppv);
    }
    /*
        else if (riid == IID_IStreamBuilder)
        {
            return GetInterface((IStreamBuilder*)this, ppv);
        }
    */
    return CBaseOutputPin::NonDelegatingQueryInterface(riid, ppv);
}

HRESULT CStreamSwitcherOutputPin::QueryAcceptUpstream(const AM_MEDIA_TYPE* pmt)
{
    HRESULT hr = S_FALSE;

    CStreamSwitcherInputPin* pIn = (static_cast<CStreamSwitcherFilter*>(m_pFilter))->GetInputPin();

    if (pIn && pIn->IsConnected() && (pIn->IsUsingOwnAllocator() || pIn->CurrentMediaType() == *pmt)) {
        if (CComQIPtr<IPin> pPinTo = pIn->GetConnected()) {
            if (S_OK != (hr = pPinTo->QueryAccept(pmt))) {
                return VFW_E_TYPE_NOT_ACCEPTED;
            }
        } else {
            return E_FAIL;
        }
    }

    return hr;
}

// pure virtual

HRESULT CStreamSwitcherOutputPin::DecideBufferSize(IMemAllocator* pAllocator, ALLOCATOR_PROPERTIES* pProperties)
{
    CStreamSwitcherInputPin* pIn = (static_cast<CStreamSwitcherFilter*>(m_pFilter))->GetInputPin();
    if (!pIn || !pIn->IsConnected()) {
        return E_UNEXPECTED;
    }

    CComPtr<IMemAllocator> pAllocatorIn;
    pIn->GetAllocator(&pAllocatorIn);
    if (!pAllocatorIn) {
        return E_UNEXPECTED;
    }

    HRESULT hr;
    if (FAILED(hr = pAllocatorIn->GetProperties(pProperties))) {
        return hr;
    }

    if (pProperties->cBuffers < 8 && pIn->CurrentMediaType().majortype == MEDIATYPE_Audio) {
        pProperties->cBuffers = 8;
    }

    ALLOCATOR_PROPERTIES Actual;
    if (FAILED(hr = pAllocator->SetProperties(pProperties, &Actual))) {
        return hr;
    }

    return (pProperties->cBuffers > Actual.cBuffers || pProperties->cbBuffer > Actual.cbBuffer
            ? E_FAIL
            : NOERROR);
}

// virtual

class __declspec(uuid("AEFA5024-215A-4FC7-97A4-1043C86FD0B8"))
    MatrixMixer {};

HRESULT CStreamSwitcherOutputPin::CheckConnect(IPin* pPin)
{
    CComPtr<IBaseFilter> pBF = GetFilterFromPin(pPin);

    return
        IsAudioWaveRenderer(pBF) || GetCLSID(pBF) == __uuidof(MatrixMixer)
        ? __super::CheckConnect(pPin)
        : E_FAIL;

    //  return CComQIPtr<IPinConnection>(pPin) ? CBaseOutputPin::CheckConnect(pPin) : E_NOINTERFACE;
    //  return CBaseOutputPin::CheckConnect(pPin);
}

HRESULT CStreamSwitcherOutputPin::BreakConnect()
{
    m_pPinConnection = nullptr;
    return __super::BreakConnect();
}

HRESULT CStreamSwitcherOutputPin::CompleteConnect(IPin* pReceivePin)
{
    m_pPinConnection = CComQIPtr<IPinConnection>(pReceivePin);
    HRESULT hr = __super::CompleteConnect(pReceivePin);

    CStreamSwitcherInputPin* pIn = (static_cast<CStreamSwitcherFilter*>(m_pFilter))->GetInputPin();
    CMediaType mt;
    if (SUCCEEDED(hr) && pIn && pIn->IsConnected()
            && SUCCEEDED(pIn->GetConnected()->ConnectionMediaType(&mt)) && m_mt != mt) {
        if (pIn->GetConnected()->QueryAccept(&m_mt) == S_OK) {
            hr = m_pFilter->ReconnectPin(pIn->GetConnected(), &m_mt);
        } else {
            hr = VFW_E_TYPE_NOT_ACCEPTED;
        }
    }

    return hr;
}

HRESULT CStreamSwitcherOutputPin::CheckMediaType(const CMediaType* pmt)
{
    return (static_cast<CStreamSwitcherFilter*>(m_pFilter))->CheckMediaType(pmt);
}

HRESULT CStreamSwitcherOutputPin::GetMediaType(int iPosition, CMediaType* pmt)
{
    CStreamSwitcherInputPin* pIn = (static_cast<CStreamSwitcherFilter*>(m_pFilter))->GetInputPin();
    if (!pIn || !pIn->IsConnected()) {
        return E_UNEXPECTED;
    }

    CComPtr<IEnumMediaTypes> pEM;
    if (FAILED(pIn->GetConnected()->EnumMediaTypes(&pEM))) {
        return VFW_S_NO_MORE_ITEMS;
    }

    if (iPosition > 0 && FAILED(pEM->Skip(iPosition))) {
        return VFW_S_NO_MORE_ITEMS;
    }

    AM_MEDIA_TYPE* tmp = nullptr;
    if (S_OK != pEM->Next(1, &tmp, nullptr) || !tmp) {
        return VFW_S_NO_MORE_ITEMS;
    }

    CopyMediaType(pmt, tmp);
    DeleteMediaType(tmp);
    /*
        if (iPosition < 0) return E_INVALIDARG;
        if (iPosition > 0) return VFW_S_NO_MORE_ITEMS;

        CopyMediaType(pmt, &pIn->CurrentMediaType());
    */
    return S_OK;
}

// IPin

STDMETHODIMP CStreamSwitcherOutputPin::QueryAccept(const AM_MEDIA_TYPE* pmt)
{
    HRESULT hr = __super::QueryAccept(pmt);
    if (S_OK != hr) {
        return hr;
    }

    return QueryAcceptUpstream(pmt);
}

// IQualityControl

STDMETHODIMP CStreamSwitcherOutputPin::Notify(IBaseFilter* pSender, Quality q)
{
    CStreamSwitcherInputPin* pIn = (static_cast<CStreamSwitcherFilter*>(m_pFilter))->GetInputPin();
    if (!pIn || !pIn->IsConnected()) {
        return VFW_E_NOT_CONNECTED;
    }
    return pIn->PassNotify(q);
}

// IStreamBuilder

STDMETHODIMP CStreamSwitcherOutputPin::Render(IPin* ppinOut, IGraphBuilder* pGraph)
{
    CComPtr<IBaseFilter> pBF;
    pBF.CoCreateInstance(CLSID_DSoundRender);
    if (!pBF || FAILED(pGraph->AddFilter(pBF, L"Default DirectSound Device"))) {
        return E_FAIL;
    }

    if (FAILED(pGraph->ConnectDirect(ppinOut, GetFirstDisconnectedPin(pBF, PINDIR_INPUT), nullptr))) {
        pGraph->RemoveFilter(pBF);
        return E_FAIL;
    }

    return S_OK;
}

STDMETHODIMP CStreamSwitcherOutputPin::Backout(IPin* ppinOut, IGraphBuilder* pGraph)
{
    return S_OK;
}

//
// CStreamSwitcherFilter
//

CStreamSwitcherFilter::CStreamSwitcherFilter(LPUNKNOWN lpunk, HRESULT* phr, const CLSID& clsid)
    : CBaseFilter(NAME("CStreamSwitcherFilter"), lpunk, &m_csState, clsid)
    , m_pInput(nullptr)
    , m_pOutput(nullptr)
{
    if (phr) {
        *phr = S_OK;
    }

    HRESULT hr = S_OK;

    do {
        CAutoPtr<CStreamSwitcherInputPin> pInput;
        CAutoPtr<CStreamSwitcherOutputPin> pOutput;

        hr = S_OK;
        pInput.Attach(DEBUG_NEW CStreamSwitcherInputPin(this, &hr, L"Channel 1"));
        if (!pInput || FAILED(hr)) {
            break;
        }

        hr = S_OK;
        pOutput.Attach(DEBUG_NEW CStreamSwitcherOutputPin(this, &hr));
        if (!pOutput || FAILED(hr)) {
            break;
        }

        CAutoLock cAutoLock(&m_csPins);

        m_pInputs.AddHead(m_pInput = pInput.Detach());
        m_pOutput = pOutput.Detach();

        return;
    } while (false);

    if (phr) {
        *phr = E_FAIL;
    }
}

CStreamSwitcherFilter::~CStreamSwitcherFilter()
{
    CAutoLock cAutoLock(&m_csPins);

    POSITION pos = m_pInputs.GetHeadPosition();
    while (pos) {
        delete m_pInputs.GetNext(pos);
    }
    m_pInputs.RemoveAll();
    m_pInput = nullptr;

    delete m_pOutput;
    m_pOutput = nullptr;
}

STDMETHODIMP CStreamSwitcherFilter::NonDelegatingQueryInterface(REFIID riid, void** ppv)
{
    return
        QI(IAMStreamSelect)
        __super::NonDelegatingQueryInterface(riid, ppv);
}

//

int CStreamSwitcherFilter::GetPinCount()
{
    CAutoLock cAutoLock(&m_csPins);

    return (1 + (int)m_pInputs.GetCount());
}

CBasePin* CStreamSwitcherFilter::GetPin(int n)
{
    CAutoLock cAutoLock(&m_csPins);

    if (n < 0 || n >= GetPinCount()) {
        return nullptr;
    } else if (n == 0) {
        return m_pOutput;
    } else {
        return m_pInputs.GetAt(m_pInputs.FindIndex(n - 1));
    }
}

int CStreamSwitcherFilter::GetConnectedInputPinCount()
{
    CAutoLock cAutoLock(&m_csPins);

    int nConnected = 0;
    POSITION pos = m_pInputs.GetHeadPosition();

    while (pos) {
        if (m_pInputs.GetNext(pos)->IsConnected()) {
            nConnected++;
        }
    }

    return nConnected;
}

CStreamSwitcherInputPin* CStreamSwitcherFilter::GetConnectedInputPin(int n)
{
    if (n >= 0) {
        POSITION pos = m_pInputs.GetHeadPosition();
        while (pos) {
            CStreamSwitcherInputPin* pPin = m_pInputs.GetNext(pos);
            if (pPin->IsConnected()) {
                if (n == 0) {
                    return pPin;
                }
                n--;
            }
        }
    }

    return nullptr;
}

CStreamSwitcherInputPin* CStreamSwitcherFilter::GetInputPin()
{
    return m_pInput;
}

CStreamSwitcherOutputPin* CStreamSwitcherFilter::GetOutputPin()
{
    return m_pOutput;
}

//

HRESULT CStreamSwitcherFilter::CompleteConnect(PIN_DIRECTION dir, CBasePin* pPin, IPin* pReceivePin)
{
    if (dir == PINDIR_INPUT) {
        CAutoLock cAutoLock(&m_csPins);

        int nConnected = GetConnectedInputPinCount();

        if (nConnected == 1) {
            m_pInput = static_cast<CStreamSwitcherInputPin*>(pPin);
        }

        if ((size_t)nConnected == m_pInputs.GetCount()) {
            CStringW name;
            name.Format(L"Channel %ld", ++m_PinVersion);

            HRESULT hr = S_OK;
            CStreamSwitcherInputPin* pInputPin = DEBUG_NEW CStreamSwitcherInputPin(this, &hr, name);
            if (!pInputPin || FAILED(hr)) {
                delete pInputPin;
                return E_FAIL;
            }
            if (pInputPin == (CStreamSwitcherInputPin*)0x3) { // weird x86 bug
                return E_FAIL;
            }
            m_pInputs.AddTail(pInputPin);
        }
    }

    return S_OK;
}

// this should be very thread safe, I hope it is, it must be... :)

void CStreamSwitcherFilter::SelectInput(CStreamSwitcherInputPin* pInput)
{
    // make sure no input thinks it is active
    m_pInput = nullptr;

    // release blocked GetBuffer in our own allocator & block all Receive
    POSITION pos = m_pInputs.GetHeadPosition();
    while (pos) {
        CStreamSwitcherInputPin* pPin = m_pInputs.GetNext(pos);
        pPin->Block(false);
        // a few Receive calls can arrive here, but since m_pInput == nullptr neighter of them gets delivered
        pPin->Block(true);
    }

    // this will let waiting GetBuffer() calls go on inside our Receive()
    if (m_pOutput) {
        m_pOutput->DeliverBeginFlush();
        m_pOutput->DeliverEndFlush();
    }

    if (!pInput) {
        return;
    }

    // set new input
    m_pInput = pInput;

    // let it go
    m_pInput->Block(false);
}

//

HRESULT CStreamSwitcherFilter::Transform(IMediaSample* pIn, IMediaSample* pOut)
{
    BYTE* pDataIn  = nullptr;
    BYTE* pDataOut = nullptr;

    HRESULT hr;
    if (FAILED(hr = pIn->GetPointer(&pDataIn))) {
        return hr;
    }
    if (FAILED(hr = pOut->GetPointer(&pDataOut))) {
        return hr;
    }

    long len  = pIn->GetActualDataLength();
    long size = pOut->GetSize();

    if (!pDataIn || !pDataOut /*|| len > size || len <= 0*/) {
        return S_FALSE;    // FIXME
    }

    memcpy(pDataOut, pDataIn, std::min(len, size));
    pOut->SetActualDataLength(std::min(len, size));

    return S_OK;
}

CMediaType CStreamSwitcherFilter::CreateNewOutputMediaType(CMediaType mt, long& cbBuffer)
{
    return mt;
}

HRESULT CStreamSwitcherFilter::DeliverEndOfStream()
{
    return m_pOutput ? m_pOutput->DeliverEndOfStream() : E_FAIL;
}

HRESULT CStreamSwitcherFilter::DeliverBeginFlush()
{
    return m_pOutput ? m_pOutput->DeliverBeginFlush() : E_FAIL;
}

HRESULT CStreamSwitcherFilter::DeliverEndFlush()
{
    return m_pOutput ? m_pOutput->DeliverEndFlush() : E_FAIL;
}

HRESULT CStreamSwitcherFilter::DeliverNewSegment(REFERENCE_TIME tStart, REFERENCE_TIME tStop, double dRate)
{
    return m_pOutput ? m_pOutput->DeliverNewSegment(tStart, tStop, dRate) : E_FAIL;
}

// IAMStreamSelect

STDMETHODIMP CStreamSwitcherFilter::Count(DWORD* pcStreams)
{
    CheckPointer(pcStreams, E_POINTER);

    CAutoLock cAutoLock(&m_csPins);

    *pcStreams = 0;
    POSITION pos = m_pInputs.GetHeadPosition();
    while (pos) {
        CStreamSwitcherInputPin* pInputPin = m_pInputs.GetNext(pos);
        if (pInputPin == (CStreamSwitcherInputPin*)0x3) { // weird x86 bug
            continue;
        }
        if (pInputPin->IsConnected()) {
            if (CComPtr<IAMStreamSelect> pSSF = pInputPin->GetStreamSelectionFilter()) {
                DWORD cStreams = 0;
                HRESULT hr = pSSF->Count(&cStreams);
                if (SUCCEEDED(hr)) {
                    for (int i = 0; i < (int)cStreams; i++) {
                        AM_MEDIA_TYPE* pmt = nullptr;
                        hr = pSSF->Info(i, &pmt, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr);
                        if (SUCCEEDED(hr) && pmt && pmt->majortype == MEDIATYPE_Audio) {
                            (*pcStreams)++;
                        }
                        DeleteMediaType(pmt);
                    }
                }
            } else {
                (*pcStreams)++;
            }
        }
    }

    return S_OK;
}

STDMETHODIMP CStreamSwitcherFilter::Info(long lIndex, AM_MEDIA_TYPE** ppmt, DWORD* pdwFlags, LCID* plcid, DWORD* pdwGroup, WCHAR** ppszName, IUnknown** ppObject, IUnknown** ppUnk)
{
    CAutoLock cAutoLock(&m_csPins);

    IUnknown* pObject = nullptr;
    bool bFound = false;
    POSITION pos = m_pInputs.GetHeadPosition();
    while (pos && !bFound) {
        CStreamSwitcherInputPin* pInputPin = m_pInputs.GetNext(pos);

        if (pInputPin->IsConnected()) {
            if (CComPtr<IAMStreamSelect> pSSF = pInputPin->GetStreamSelectionFilter()) {
                DWORD cStreams = 0;
                HRESULT hr = pSSF->Count(&cStreams);
                if (SUCCEEDED(hr)) {
                    for (int i = 0; i < (int)cStreams; i++) {
                        AM_MEDIA_TYPE* pmt = nullptr;
                        DWORD dwFlags;
                        LPWSTR pszName = nullptr;
                        hr = pSSF->Info(i, &pmt, &dwFlags, plcid, nullptr, &pszName, nullptr, nullptr);
                        if (SUCCEEDED(hr) && pmt && pmt->majortype == MEDIATYPE_Audio) {
                            if (lIndex == 0) {
                                bFound = true;
                                pObject = pSSF;

                                if (ppmt) {
                                    *ppmt = pmt;
                                } else {
                                    DeleteMediaType(pmt);
                                }

                                if (pdwFlags) {
                                    *pdwFlags = (m_pInput == pInputPin) ? dwFlags : 0;
                                }

                                if (pdwGroup) {
                                    *pdwGroup = 1;
                                }

                                if (ppszName) {
                                    *ppszName = pszName;
                                } else {
                                    CoTaskMemFree(pszName);
                                }

                                break;
                            } else {
                                lIndex--;
                            }
                        }
                        DeleteMediaType(pmt);
                        CoTaskMemFree(pszName);
                    }
                }
            } else if (lIndex == 0) {
                bFound = true;

                if (ppmt) {
                    bool found = false;
                    CComPtr<IPin> pPinUpstream;
                    if (SUCCEEDED(m_pInput->ConnectedTo(&pPinUpstream)) && pPinUpstream) {
                        // find audio input pin of upstream filter
                        if (CComQIPtr<IBaseFilter> pBF = GetFilterFromPin(pPinUpstream)) {
                            BeginEnumPins(pBF, pEP, pPin) {
                                CMediaTypeEx mt;
                                PIN_DIRECTION dir;
                                if (SUCCEEDED(pPin->QueryDirection(&dir)) && dir == PINDIR_INPUT && SUCCEEDED(pPin->ConnectionMediaType(&mt))) {
                                    if (mt.majortype == MEDIATYPE_Audio) {
                                        *ppmt = CreateMediaType(&mt);
                                        found = true;
                                        break;
                                    }
                                }
                            }
                            EndEnumPins;
                        }
                    }
                    if (!found) {
                        *ppmt = CreateMediaType(&m_pInput->CurrentMediaType());
                    }
                }

                if (pdwFlags) {
                    *pdwFlags = (m_pInput == pInputPin) ? AMSTREAMSELECTINFO_EXCLUSIVE : 0;
                }

                if (plcid) {
                    *plcid = 0;
                }

                if (pdwGroup) {
                    *pdwGroup = 1;
                }

                if (ppszName) {
                    *ppszName = (WCHAR*)CoTaskMemAlloc((wcslen(pInputPin->Name()) + 1) * sizeof(WCHAR));
                    if (*ppszName) {
                        wcscpy_s(*ppszName, wcslen(pInputPin->Name()) + 1, pInputPin->Name());
                    }
                }
            } else {
                lIndex--;
            }
        }
    }

    if (!bFound) {
        return E_INVALIDARG;
    }

    if (ppObject) {
        *ppObject = pObject;
        if (pObject) {
            pObject->AddRef();
        }
    }

    if (ppUnk) {
        *ppUnk = nullptr;
    }

    return S_OK;
}

STDMETHODIMP CStreamSwitcherFilter::Enable(long lIndex, DWORD dwFlags)
{
    if (dwFlags != AMSTREAMSELECTENABLE_ENABLE) {
        return E_NOTIMPL;
    }

    PauseGraph;

    bool bFound = false;
    int i = 0;
    CStreamSwitcherInputPin* pNewInputPin = nullptr;
    POSITION pos = m_pInputs.GetHeadPosition();
    while (pos && !bFound) {
        pNewInputPin = m_pInputs.GetNext(pos);

        if (pNewInputPin->IsConnected()) {
            if (CComPtr<IAMStreamSelect> pSSF = pNewInputPin->GetStreamSelectionFilter()) {
                DWORD cStreams = 0;
                HRESULT hr = pSSF->Count(&cStreams);
                if (SUCCEEDED(hr)) {
                    for (i = 0; i < (int)cStreams; i++) {
                        AM_MEDIA_TYPE* pmt = nullptr;
                        hr = pSSF->Info(i, &pmt, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr);
                        if (SUCCEEDED(hr) && pmt && pmt->majortype == MEDIATYPE_Audio) {
                            if (lIndex == 0) {
                                bFound = true;
                                DeleteMediaType(pmt);
                                break;
                            } else {
                                lIndex--;
                            }
                        }
                        DeleteMediaType(pmt);
                    }
                }
            } else if (lIndex == 0) {
                bFound = true;
            } else {
                lIndex--;
            }
        }
    }

    if (!bFound) {
        return E_INVALIDARG;
    }

    SelectInput(pNewInputPin);

    if (CComPtr<IAMStreamSelect> pSSF = pNewInputPin->GetStreamSelectionFilter()) {
        pSSF->Enable(i, dwFlags);
    }

    ResumeGraph;

    return S_OK;
}
