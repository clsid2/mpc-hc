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
#include "SubtitleInputPin.h"
#include "VobSubFile.h"
#include "RTS.h"
#include "DVBSub.h"
#include "PGSSub.h"

#include <uuids.h>
#include "moreuuids.h"
#include "../DSUtil/ISOLang.h"

#if !TRACE_SUBTITLES
#undef TRACE
#define TRACE(...)
#endif

// our first format id
#define __GAB1__ "GAB1"

// our tags for __GAB1__ (ushort) + size (ushort)

// "lang" + '0'
#define __GAB1_LANGUAGE__ 0
// (int)start+(int)stop+(char*)line+'0'
#define __GAB1_ENTRY__ 1
// L"lang" + '0'
#define __GAB1_LANGUAGE_UNICODE__ 2
// (int)start+(int)stop+(WCHAR*)line+'0'
#define __GAB1_ENTRY_UNICODE__ 3

// same as __GAB1__, but the size is (uint) and only __GAB1_LANGUAGE_UNICODE__ is valid
#define __GAB2__ "GAB2"

// (BYTE*)
#define __GAB1_RAWTEXTSUBTITLE__ 4

CSubtitleInputPin::CSubtitleInputPin(CBaseFilter* pFilter, CCritSec* pLock, CCritSec* pSubLock, HRESULT* phr)
    : CBaseInputPin(NAME("CSubtitleInputPin"), pFilter, pLock, phr, L"Input")
    , m_pSubLock(pSubLock)
    , m_bExitDecodingThread(false)
    , m_bStopDecoding(false)
{
    m_bCanReconnectWhenActive = true;
    m_decodeThread = std::thread([this]() {
        DecodeSamples();
    });
}

CSubtitleInputPin::~CSubtitleInputPin()
{
    m_bExitDecodingThread = m_bStopDecoding = true;
    m_condQueueReady.notify_one();
    if (m_decodeThread.joinable()) {
        m_decodeThread.join();
    }
}

HRESULT CSubtitleInputPin::CheckMediaType(const CMediaType* pmt)
{
    return pmt->majortype == MEDIATYPE_Text && (pmt->subtype == MEDIASUBTYPE_NULL || pmt->subtype == FOURCCMap((DWORD)0))
           || pmt->majortype == MEDIATYPE_Subtitle && (pmt->subtype == MEDIASUBTYPE_UTF8 || pmt->subtype == MEDIASUBTYPE_SSA || pmt->subtype == MEDIASUBTYPE_ASS || pmt->subtype == MEDIASUBTYPE_ASS2 || pmt->subtype == MEDIASUBTYPE_VOBSUB || pmt->subtype == MEDIASUBTYPE_WEBVTT)
           || IsRLECodedSub(pmt)
           ? S_OK
           : E_FAIL;
}

HRESULT CSubtitleInputPin::CompleteConnect(IPin* pReceivePin)
{
    InvalidateSamples();
    m_lastHeader.clear();

    if (m_mt.majortype == MEDIATYPE_Text) {
        if (!(m_pSubStream = DEBUG_NEW CRenderedTextSubtitle(m_pSubLock))) {
            return E_FAIL;
        }
        CRenderedTextSubtitle* pRTS = (CRenderedTextSubtitle*)(ISubStream*)m_pSubStream;
        pRTS->m_name = CString(GetPinName(pReceivePin)) + _T(" (embedded)");
        pRTS->m_storageRes = pRTS->m_playRes = CSize(384, 288);
        pRTS->CreateDefaultStyle(DEFAULT_CHARSET);
    } else if (m_mt.majortype == MEDIATYPE_Subtitle) {
        SUBTITLEINFO* psi = (SUBTITLEINFO*)m_mt.pbFormat;
        DWORD   dwOffset = 0;
        CString name;
        LCID    lcid = 0;

        if (psi != nullptr) {
            dwOffset = psi->dwOffset;

            lcid = ISOLang::ISO6392ToLcid(psi->IsoLang);
            if (0 == lcid) { //try 639-1 in case it comes from BCP-47 (contains mix of 639-1 and 639-2)
                lcid = ISOLang::ISO6391ToLcid(psi->IsoLang);
            }
            name = ISOLang::ISO639XToLanguage(psi->IsoLang);

            CString trackName(psi->TrackName);
            trackName.Trim();
            if (!trackName.IsEmpty()) {
                if (!name.IsEmpty()) {
                    if (trackName[0] != _T('(') && trackName[0] != _T('[')) {
                        name += _T(",");
                    }
                    name += _T(" ");
                }
                name += trackName;
            }
            if (name.IsEmpty()) {
                name = _T("Unknown");
            }
        }

        name.Replace(_T(""), _T(""));
        name.Replace(_T(""), _T(""));

        bool subtype_utf8 = m_mt.subtype == MEDIASUBTYPE_UTF8;
        bool subtype_vtt  = m_mt.subtype == MEDIASUBTYPE_WEBVTT;
        bool subtype_ass  = m_mt.subtype == MEDIASUBTYPE_SSA || m_mt.subtype == MEDIASUBTYPE_ASS || m_mt.subtype == MEDIASUBTYPE_ASS2;

        if (subtype_utf8 || subtype_ass || subtype_vtt) {
            if (!(m_pSubStream = DEBUG_NEW CRenderedTextSubtitle(m_pSubLock))) {
                return E_FAIL;
            }

            CRenderedTextSubtitle* pRTS = (CRenderedTextSubtitle*)(ISubStream*)m_pSubStream;
            pRTS->SetSubtitleTypeFromGUID(m_mt.subtype);
#if USE_LIBASS
            if (pRTS->m_LibassContext.CheckSubType()) {
                pRTS->m_LibassContext.SetFilterGraphFromFilter(m_pFilter);
            }
#endif
            pRTS->m_name = name;
            pRTS->m_lcid = lcid;
            if (lcid > 0) {
                pRTS->m_langname = ISOLang::LCIDToLanguage(lcid);
            }
            pRTS->m_storageRes = pRTS->m_playRes = CSize(384, 288);
            pRTS->CreateDefaultStyle(DEFAULT_CHARSET);

            if (subtype_ass && dwOffset > 0 && m_mt.cbFormat > dwOffset) {
                m_lastHeader.assign(m_mt.pbFormat + dwOffset, m_mt.pbFormat + m_mt.cbFormat);
            }

            if (dwOffset > 0 && m_mt.cbFormat - dwOffset > 0) {
                CMediaType mt = m_mt;
                if (mt.pbFormat[dwOffset + 0] != 0xef
                        && mt.pbFormat[dwOffset + 1] != 0xbb
                        && mt.pbFormat[dwOffset + 2] != 0xfb) {
                    dwOffset -= 3;
                    mt.pbFormat[dwOffset + 0] = 0xef;
                    mt.pbFormat[dwOffset + 1] = 0xbb;
                    mt.pbFormat[dwOffset + 2] = 0xbf;
                }

                // process with own parser first, even when using libass, since we need certain info like PlayRes
                pRTS->Open(mt.pbFormat + dwOffset, mt.cbFormat - dwOffset, DEFAULT_CHARSET, pRTS->m_name);
#if USE_LIBASS
                if (pRTS->m_LibassContext.m_renderUsingLibass) {
                    bool success = pRTS->m_LibassContext.LoadASSTrack((char*)m_mt.Format() + psi->dwOffset, m_mt.FormatLength() - psi->dwOffset, subtype_ass ? Subtitle::ASS : Subtitle::SRT);
                    pRTS->m_LibassContext.m_renderUsingLibass = success && pRTS->m_LibassContext.IsLibassActive();
                }
#endif
            }
        } else if (m_mt.subtype == MEDIASUBTYPE_VOBSUB) {
            if (!(m_pSubStream = DEBUG_NEW CVobSubStream(m_pSubLock))) {
                return E_FAIL;
            }
            CVobSubStream* pVSS = (CVobSubStream*)(ISubStream*)m_pSubStream;
            pVSS->Open(name, m_mt.pbFormat + dwOffset, m_mt.cbFormat - dwOffset);
        } else if (IsRLECodedSub(&m_mt)) {
            if (m_mt.subtype == MEDIASUBTYPE_DVB_SUBTITLES) {
                m_pSubStream = DEBUG_NEW CDVBSub(m_pSubLock, name, lcid);
            } else {
                m_pSubStream = DEBUG_NEW CPGSSub(m_pSubLock, name, lcid);
            }
            if (!m_pSubStream) {
                return E_FAIL;
            }
        }
    }

    AddSubStream(m_pSubStream);

    return __super::CompleteConnect(pReceivePin);
}

HRESULT CSubtitleInputPin::BreakConnect()
{
    InvalidateSamples();

    RemoveSubStream(m_pSubStream);
    m_pSubStream = nullptr;

    ASSERT(IsStopped());

    return __super::BreakConnect();
}

STDMETHODIMP CSubtitleInputPin::ReceiveConnection(IPin* pConnector, const AM_MEDIA_TYPE* pmt)
{
    if (m_Connected) {
        InvalidateSamples();

        RemoveSubStream(m_pSubStream);
        m_pSubStream = nullptr;

        m_Connected->Release();
        m_Connected = nullptr;
    }

    return __super::ReceiveConnection(pConnector, pmt);
}

STDMETHODIMP CSubtitleInputPin::NewSegment(REFERENCE_TIME tStart, REFERENCE_TIME tStop, double dRate)
{
    CAutoLock cAutoLock(&m_csReceive);

    InvalidateSamples();

    if (m_mt.majortype == MEDIATYPE_Text || m_mt.majortype == MEDIATYPE_Subtitle && (m_mt.subtype == MEDIASUBTYPE_UTF8
        || m_mt.subtype == MEDIASUBTYPE_WEBVTT || m_mt.subtype == MEDIASUBTYPE_SSA || m_mt.subtype == MEDIASUBTYPE_ASS
        || m_mt.subtype == MEDIASUBTYPE_ASS2)) {
        CAutoLock cAutoLock2(m_pSubLock);
        CRenderedTextSubtitle* pRTS = (CRenderedTextSubtitle*)(ISubStream*)m_pSubStream;
        if (pRTS->m_webvtt_allow_clear || pRTS->m_subtitleType != Subtitle::VTT) {
            pRTS->RemoveAll();
            pRTS->CreateSegments();
            pRTS->FlushEventsLibass();
        }
        // WebVTT can be read as one big blob of data during pin connection, instead of as samples during playback.
        // This depends on how it is being demuxed. So clear only if we previously got data through samples.
    } else if (m_mt.majortype == MEDIATYPE_Subtitle && (m_mt.subtype == MEDIASUBTYPE_VOBSUB)) {
        CAutoLock cAutoLock2(m_pSubLock);
        CVobSubStream* pVSS = (CVobSubStream*)(ISubStream*)m_pSubStream;
        pVSS->RemoveAll();
    } else if (IsRLECodedSub(&m_mt)) {
        CAutoLock cAutoLock2(m_pSubLock);
        CRLECodedSubtitle* pRLECodedSubtitle = (CRLECodedSubtitle*)(ISubStream*)m_pSubStream;
        pRLECodedSubtitle->NewSegment(tStart, tStop, dRate);
    }

    TRACE(_T("NewSegment: InvalidateSubtitle %.3f\n"), RT2SEC(tStart));
    // IMPORTANT: m_pSubLock must not be locked when calling this
    InvalidateSubtitle(tStart, m_pSubStream);

    return __super::NewSegment(tStart, tStop, dRate);
}

STDMETHODIMP CSubtitleInputPin::Receive(IMediaSample* pSample)
{
    HRESULT hr = __super::Receive(pSample);
    if (FAILED(hr)) {
        return hr;
    }

    CAutoLock cAutoLock(&m_csReceive);

    if (m_mt.majortype == MEDIATYPE_Subtitle
            && (m_mt.subtype == MEDIASUBTYPE_SSA || m_mt.subtype == MEDIASUBTYPE_ASS || m_mt.subtype == MEDIASUBTYPE_ASS2)) {
        // The splitter can send an updated subtitle header as a dynamic media type change,
        // e.g. when MKV ordered chapters transition into a linked segment whose track has
        // different styles. Queue it so it is applied in decode order, ahead of the samples
        // that reference the new styles.
        AM_MEDIA_TYPE* pmt = nullptr;
        if (pSample->GetMediaType(&pmt) == S_OK && pmt) {
            if (pmt->majortype == MEDIATYPE_Subtitle && pmt->formattype == FORMAT_SubtitleInfo
                    && pmt->cbFormat > sizeof(SUBTITLEINFO)) {
                const SUBTITLEINFO* psi = (const SUBTITLEINFO*)pmt->pbFormat;
                if (psi->dwOffset >= sizeof(SUBTITLEINFO) && psi->dwOffset < pmt->cbFormat) {
                    const BYTE* pHeader = pmt->pbFormat + psi->dwOffset;
                    size_t headerLen = pmt->cbFormat - psi->dwOffset;
                    if (m_lastHeader.size() != headerLen || memcmp(m_lastHeader.data(), pHeader, headerLen) != 0) {
                        m_lastHeader.assign(pHeader, pHeader + headerLen);
                        std::unique_lock<std::mutex> lock(m_mutexQueue);
                        m_sampleQueue.emplace_back(DEBUG_NEW SubtitleSample(pHeader, headerLen));
                        lock.unlock();
                        m_condQueueReady.notify_one();
                    }
                }
            }
            DeleteMediaType(pmt);
        }
    }

    REFERENCE_TIME tStart, tStop;
    hr = pSample->GetTime(&tStart, &tStop);

    switch (hr) {
        case S_OK:
            tStart += m_tStart;
            tStop += m_tStart;
            break;
        case VFW_S_NO_STOP_TIME:
            tStart += m_tStart;
            tStop = INVALID_TIME;
            break;
        case VFW_E_SAMPLE_TIME_NOT_SET:
            tStart = tStop = INVALID_TIME;
            break;
        default:
            ASSERT(FALSE);
            return hr;
    }

    if ((tStart == INVALID_TIME || tStop == INVALID_TIME) && !IsRLECodedSub(&m_mt)) {
        ASSERT(FALSE);
    } else {
        BYTE* pData = nullptr;
        hr = pSample->GetPointer(&pData);
        long len = pSample->GetActualDataLength();
        if (FAILED(hr) || pData == nullptr || len <= 0) {
            return hr;
        }

        {
            std::unique_lock<std::mutex> lock(m_mutexQueue);
            m_sampleQueue.emplace_back(DEBUG_NEW SubtitleSample(tStart, tStop, pData, size_t(len)));
            lock.unlock();
            m_condQueueReady.notify_one();
        }
    }

    return S_OK;
}

STDMETHODIMP CSubtitleInputPin::EndOfStream(void)
{
    HRESULT hr = __super::EndOfStream();

    if (SUCCEEDED(hr)) {
        std::unique_lock<std::mutex> lock(m_mutexQueue);
        m_sampleQueue.emplace_back(nullptr); // nullptr means end of stream
        lock.unlock();
        m_condQueueReady.notify_one();
    }

    return hr;
}

bool CSubtitleInputPin::IsRLECodedSub(const CMediaType* pmt) const
{
    return !!(pmt->majortype == MEDIATYPE_Subtitle
              && (pmt->subtype == MEDIASUBTYPE_HDMVSUB                                                // Blu-Ray presentation graphics
                  || pmt->subtype == MEDIASUBTYPE_DVB_SUBTITLES                                       // DVB subtitles
                  || (pmt->subtype == MEDIASUBTYPE_NULL && pmt->formattype == FORMAT_SubtitleInfo))); // Workaround : support for Haali PGS
}

void  CSubtitleInputPin::DecodeSamples()
{
    SetThreadName(DWORD(-1), "Subtitle Input Pin Thread");

    for (; !m_bExitDecodingThread;) {
        std::unique_lock<std::mutex> lock(m_mutexQueue);

        auto needStopProcessing = [this]() {
            return m_bStopDecoding || m_bExitDecodingThread;
        };

        auto isQueueReady = [&]() {
            return !m_sampleQueue.empty() || needStopProcessing();
        };

        m_condQueueReady.wait(lock, isQueueReady);
        lock.unlock(); // Release this lock until we can acquire the other one

        REFERENCE_TIME rtInvalidate = -1;

        if (!needStopProcessing()) {
            CAutoLock cAutoLock(m_pSubLock);
            lock.lock(); // Reacquire the lock

            while (!m_sampleQueue.empty() && !needStopProcessing()) {
                const auto& pSample = m_sampleQueue.front();

                if (pSample) {
                    REFERENCE_TIME rtSampleInvalidate = DecodeSample(pSample);
                    if (rtSampleInvalidate >= 0 && (rtSampleInvalidate < rtInvalidate || rtInvalidate < 0)) {
                        rtInvalidate = rtSampleInvalidate;
                    }
                } else { // marker for end of stream
                    if (IsRLECodedSub(&m_mt)) {
                        CRLECodedSubtitle* pRLECodedSubtitle = (CRLECodedSubtitle*)(ISubStream*)m_pSubStream;
                        pRLECodedSubtitle->EndOfStream();
                    }
                }

                m_sampleQueue.pop_front();
            }
        }

        if (rtInvalidate >= 0) {
            //TRACE(_T("NewSegment: InvalidateSubtitle %.3f\n"), double(rtInvalidate) / 10000000.0);
            // IMPORTANT: m_pSubLock must not be locked when calling this
            InvalidateSubtitle(rtInvalidate, m_pSubStream);
        }
    }
}

REFERENCE_TIME CSubtitleInputPin::DecodeSample(const std::unique_ptr<SubtitleSample>& pSample)
{
    bool bInvalidate = false;

    if (pSample->data.size() <= 0) {
        return -1;
    }

    if (pSample->bHeaderChange) {
        ApplyHeaderChange(pSample->data.data(), pSample->data.size());
        return -1;
    }

    if (m_mt.majortype == MEDIATYPE_Text) {
        CRenderedTextSubtitle* pRTS = (CRenderedTextSubtitle*)(ISubStream*)m_pSubStream;

        char* pData = (char*)pSample->data.data();

        if (!strncmp(pData, __GAB1__, strlen(__GAB1__))) {
            char* ptr = &pData[strlen(__GAB1__) + 1];
            char* end = &pData[pSample->data.size()];

            while (ptr < end) {
                WORD tag = *((WORD*)(ptr));
                ptr += 2;
                WORD size = *((WORD*)(ptr));
                ptr += 2;

                if (tag == __GAB1_LANGUAGE__) {
                    pRTS->m_name = ptr;
                } else if (tag == __GAB1_ENTRY__) {
                    pRTS->Add(AToW(&ptr[8]), false, MS2RT(*(int*)ptr), MS2RT(*(int*)(ptr + 4)));
                    bInvalidate = true;
                } else if (tag == __GAB1_LANGUAGE_UNICODE__) {
                    pRTS->m_name = (WCHAR*)ptr;
                } else if (tag == __GAB1_ENTRY_UNICODE__) {
                    pRTS->Add((WCHAR*)(ptr + 8), true, MS2RT(*(int*)ptr), MS2RT(*(int*)(ptr + 4)));
                    bInvalidate = true;
                }

                ptr += size;
            }
        } else if (!strncmp(pData, __GAB2__, strlen(__GAB2__))) {
            char* ptr = &pData[strlen(__GAB2__) + 1];
            char* end = &pData[pSample->data.size()];

            while (ptr < end) {
                WORD tag = *((WORD*)(ptr));
                ptr += 2;
                DWORD size = *((DWORD*)(ptr));
                ptr += 4;

                if (tag == __GAB1_LANGUAGE_UNICODE__) {
                    pRTS->m_name = (WCHAR*)ptr;
                } else if (tag == __GAB1_RAWTEXTSUBTITLE__) {
                    pRTS->Open((BYTE*)ptr, size, DEFAULT_CHARSET, pRTS->m_name);
                    bInvalidate = true;
                }

                ptr += size;
            }
        } else if (pSample->data.size() > 1 && *pData != '\0') {
            CStringA str(pData, (int)pSample->data.size());

            str.Replace("\r\n", "\n");
            FastTrim(str);

            if (!str.IsEmpty()) {
                pRTS->Add(AToW(str), false, pSample->rtStart, pSample->rtStop);
                bInvalidate = true;
            }
        }
    } else if (m_mt.majortype == MEDIATYPE_Subtitle) {
        if (m_mt.subtype == MEDIASUBTYPE_UTF8 || m_mt.subtype == MEDIASUBTYPE_WEBVTT) {
            CRenderedTextSubtitle* pRTS = (CRenderedTextSubtitle*)(ISubStream*)m_pSubStream;
#if USE_LIBASS
            if (pRTS->m_LibassContext.IsLibassActive()) {
                LPCSTR data = (LPCSTR)pSample->data.data();
                int dataSize = (int)pSample->data.size();
                pRTS->m_LibassContext.SetFilterGraphFromFilter(m_pFilter);
                pRTS->m_LibassContext.LoadASSSample((char*)data, dataSize, pSample->rtStart, pSample->rtStop);
            } else
#endif
            {
                CStringW str = UTF8To16(CStringA((LPCSTR)pSample->data.data(), (int)pSample->data.size()));
                FastTrim(str);
                if (!str.IsEmpty()) {
                    pRTS->Add(str, true, pSample->rtStart, pSample->rtStop);
                    bInvalidate = true;
                    if (pRTS->m_subtitleType == Subtitle::VTT) {
                        pRTS->m_webvtt_allow_clear = true;
                    }
                }
            }
        } else if (m_mt.subtype == MEDIASUBTYPE_SSA || m_mt.subtype == MEDIASUBTYPE_ASS || m_mt.subtype == MEDIASUBTYPE_ASS2) {
            CRenderedTextSubtitle* pRTS = (CRenderedTextSubtitle*)(ISubStream*)m_pSubStream;
#if USE_LIBASS
            if (pRTS->m_LibassContext.IsLibassActive()) {
                ass_process_chunk(pRTS->m_LibassContext.m_track.get(), (char*)pSample->data.data(), (int)pSample->data.size(), pSample->rtStart / 10000, (pSample->rtStop - pSample->rtStart) / 10000);
            } else
#endif
            {
                CStringW str = UTF8To16(CStringA((LPCSTR)pSample->data.data(), (int)pSample->data.size()));
                FastTrim(str);
                if (!str.IsEmpty()) {
                    STSEntry stse;
                    int fields = m_mt.subtype == MEDIASUBTYPE_ASS2 ? 10 : 9;
                    if (pRTS->event_param == 0b011111000001) { // non-standard variant
                        fields = 4;
                    }
                    CAtlList<CStringW> sl;
                    ExplodeNoTrim(str, sl, ',', fields);
                    if (sl.GetCount() == (size_t)fields) {
                        stse.readorder = wcstol(sl.RemoveHead(), nullptr, 10);
                        stse.layer = wcstol(sl.RemoveHead(), nullptr, 10);
                        stse.style = sl.RemoveHead(); // no trim, its value is a lookup key
                        if (fields >= 9) {
                            stse.actor = sl.RemoveHead().Trim();
                            stse.marginRect.left = wcstol(sl.RemoveHead(), nullptr, 10);
                            stse.marginRect.right = wcstol(sl.RemoveHead(), nullptr, 10);
                            stse.marginRect.top = stse.marginRect.bottom = wcstol(sl.RemoveHead(), nullptr, 10);
                        }
                        if (fields == 10) {
                            stse.marginRect.bottom = wcstol(sl.RemoveHead(), nullptr, 10);
                        }
                        if (fields >= 9) {
                            stse.effect = sl.RemoveHead().Trim();
                        }
                        stse.str = sl.RemoveHead().Trim();
                    } else {
                        ASSERT(false);
                    }

                    if (!stse.str.IsEmpty()) {
                        pRTS->Add(stse.str, true, pSample->rtStart, pSample->rtStop,
                            stse.style, stse.actor, stse.effect, stse.marginRect, stse.layer, stse.readorder);
                        bInvalidate = true;
                    }
                }
            }
        } else if (m_mt.subtype == MEDIASUBTYPE_VOBSUB) {
            CVobSubStream* pVSS = (CVobSubStream*)(ISubStream*)m_pSubStream;
            pVSS->Add(pSample->rtStart, pSample->rtStop, pSample->data.data(), (int)pSample->data.size());
        } else if (IsRLECodedSub(&m_mt)) {
            CRLECodedSubtitle* pRLECodedSubtitle = (CRLECodedSubtitle*)(ISubStream*)m_pSubStream;
            pRLECodedSubtitle->ParseSample(pSample->rtStart, pSample->rtStop, pSample->data.data(), (int)pSample->data.size());
        }
    }

    return bInvalidate ? pSample->rtStart : -1;
}

void CSubtitleInputPin::ApplyHeaderChange(const BYTE* pData, size_t len)
{
    // Runs on the decoding thread with m_pSubLock held (see DecodeSamples). The samples
    // that follow reference the style definitions of this header by name.
    CRenderedTextSubtitle* pRTS = (CRenderedTextSubtitle*)(ISubStream*)m_pSubStream;
    if (!pRTS) {
        return;
    }

#if USE_LIBASS
    if (pRTS->m_LibassContext.IsLibassActive()) {
        // libass resolves the style of new events against the most recently added
        // style with a matching name, so earlier events keep their definitions
        ass_process_codec_private(pRTS->m_LibassContext.m_track.get(), (char*)pData, (int)len);
        return;
    }
#endif

    std::vector<BYTE> buf;
    buf.reserve(len + 3);
    if (len < 3 || pData[0] != 0xef || pData[1] != 0xbb || pData[2] != 0xbf) {
        static const BYTE bom[] = { 0xef, 0xbb, 0xbf };
        buf.insert(buf.end(), bom, bom + 3);
    }
    buf.insert(buf.end(), pData, pData + len);

    CSimpleTextSubtitle sts;
    if (!sts.Open(buf.data(), (int)buf.size(), DEFAULT_CHARSET, pRTS->m_name)) {
        return;
    }

    POSITION pos = sts.m_styles.GetStartPosition();
    while (pos) {
        CString name;
        STSStyle* style = nullptr;
        sts.m_styles.GetNextAssoc(pos, name, style);
        if (style && !(sts.m_bUsingPlayerDefaultStyle && name == _T("Default"))) {
            // on a name collision, AddStyle renames the existing style and remaps the
            // events already referencing it, so they keep their original definitions
            pRTS->AddStyle(name, DEBUG_NEW STSStyle(*style));
        }
    }
}

void CSubtitleInputPin::InvalidateSamples()
{
    m_bStopDecoding = true;
    {
        std::lock_guard<std::mutex> lock(m_mutexQueue);
        m_sampleQueue.clear();
        m_bStopDecoding = false;
    }
}
