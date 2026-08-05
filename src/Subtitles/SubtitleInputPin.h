/*
 * (C) 2003-2006 Gabest
 * (C) 2006-2015 see Authors.txt
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

#include <vector>
#include <list>
#include <memory>
#include <thread>
#include <condition_variable>

#include "../SubPic/ISubPic.h"

//
// CSubtitleInputPin
//

class CSubtitleInputPin : public CBaseInputPin
{
    static const REFERENCE_TIME INVALID_TIME = _I64_MIN;

    CCritSec m_csReceive;

    CCritSec* m_pSubLock;
    CComPtr<ISubStream> m_pSubStream;

    struct SubtitleSample {
        REFERENCE_TIME rtStart, rtStop;
        std::vector<BYTE> data;
        bool bHeaderChange; // data holds a new subtitle header from a dynamic media type change

        SubtitleSample(REFERENCE_TIME rtStart, REFERENCE_TIME rtStop, BYTE* pData, size_t len)
            : rtStart(rtStart)
            , rtStop(rtStop)
            , data(pData, pData + len)
            , bHeaderChange(false) {}

        SubtitleSample(const BYTE* pData, size_t len)
            : rtStart(0)
            , rtStop(0)
            , data(pData, pData + len)
            , bHeaderChange(true) {}
    };

    std::list<std::unique_ptr<SubtitleSample>> m_sampleQueue;
    std::vector<BYTE> m_lastHeader;

    bool m_bExitDecodingThread, m_bStopDecoding;
    std::thread m_decodeThread;
    std::mutex m_mutexQueue; // to protect m_sampleQueue
    std::condition_variable m_condQueueReady;

    void DecodeSamples();
    REFERENCE_TIME DecodeSample(const std::unique_ptr<SubtitleSample>& pSample);
    void ApplyHeaderChange(const BYTE* pData, size_t len);
    void InvalidateSamples();

protected:
    virtual void AddSubStream(ISubStream* pSubStream) = 0;
    virtual void RemoveSubStream(ISubStream* pSubStream) = 0;
    virtual void InvalidateSubtitle(REFERENCE_TIME rtStart, ISubStream* pSubStream) = 0;

    bool IsRLECodedSub(const CMediaType* pmt) const;

public:
    CSubtitleInputPin(CBaseFilter* pFilter, CCritSec* pLock, CCritSec* pSubLock, HRESULT* phr);
    ~CSubtitleInputPin();

    HRESULT CheckMediaType(const CMediaType* pmt);
    HRESULT CompleteConnect(IPin* pReceivePin);
    HRESULT BreakConnect();
    STDMETHODIMP ReceiveConnection(IPin* pConnector, const AM_MEDIA_TYPE* pmt);
    STDMETHODIMP NewSegment(REFERENCE_TIME tStart, REFERENCE_TIME tStop, double dRate);
    STDMETHODIMP Receive(IMediaSample* pSample);
    STDMETHODIMP EndOfStream(void);

    ISubStream* GetSubStream() { return m_pSubStream; }
};
