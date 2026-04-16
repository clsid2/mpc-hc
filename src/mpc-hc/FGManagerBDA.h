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

#pragma once

#include "FGManager.h"
#include "DVBChannel.h"
#include <bdaiface.h>

enum DVB_RebuildFilterGraph;
typedef struct tagVIDEOINFOHEADER2 VIDEOINFOHEADER2;

class CDVBStream
{
public:
    CDVBStream()
        : m_pmt(0)
        , m_bFindExisting(false)
        , m_Name(L"")
        , m_nMsc(MEDIA_TRANSPORT_PACKET)
        , m_ulMappedPID(0) {
    }

    CDVBStream(LPCWSTR strName, const AM_MEDIA_TYPE* pmt, bool bFindExisting = false, MEDIA_SAMPLE_CONTENT nMsc = MEDIA_ELEMENTARY_STREAM)
        : m_pmt(pmt)
        , m_bFindExisting(bFindExisting)
        , m_Name(strName)
        , m_nMsc(nMsc)
        , m_ulMappedPID(0) {
    }

    LPCWSTR GetName() const { return m_Name; };
    const AM_MEDIA_TYPE* GetMediaType() { return m_pmt; };
    bool GetFindExisting() const { return m_bFindExisting; };
    IBaseFilter* GetFilter() { return m_pFilter; };

    void SetPin(IPin* pPin) {
        CComPtr<IPin> pPinOut;
        PIN_INFO PinInfo;

        m_pMap = pPin;
        if (m_pMap &&
                SUCCEEDED(pPin->ConnectedTo(&pPinOut)) &&
                SUCCEEDED(pPinOut->QueryPinInfo(&PinInfo))) {
            m_pFilter.Attach(PinInfo.pFilter);
        }
    }

    HRESULT Map(ULONG ulPID) {
        CheckPointer(m_pMap, E_UNEXPECTED);
        ClearMaps();
        m_ulMappedPID = ulPID;
        return m_pMap->MapPID(1, &ulPID, m_nMsc);
    }

    HRESULT Unmap(ULONG ulPID) {
        CheckPointer(m_pMap, E_UNEXPECTED);
        m_ulMappedPID = 0;
        return m_pMap->UnmapPID(1, &ulPID);
    }

    ULONG GetMappedPID() const { return m_ulMappedPID; }

private:
    CComQIPtr<IMPEG2PIDMap> m_pMap;
    CComPtr<IBaseFilter>    m_pFilter;
    const AM_MEDIA_TYPE*    m_pmt;
    bool                    m_bFindExisting;
    LPCWSTR                 m_Name;
    MEDIA_SAMPLE_CONTENT    m_nMsc;
    ULONG                   m_ulMappedPID;

    void ClearMaps() {
        CComPtr<IEnumPIDMap> pEnumMap;

        if (SUCCEEDED(m_pMap->EnumPIDMap(&pEnumMap))) {
            PID_MAP maps[8];
            ULONG   nbPids = 0;
            ZeroMemory(maps, sizeof(maps));

            if (pEnumMap->Next(_countof(maps), maps, &nbPids) == S_OK) {
                for (ULONG i = 0; i < nbPids; i++) {
                    ULONG pid = maps[i].ulPID;

                    m_pMap->UnmapPID(1, &pid);
                }
            }
        }
    }
};

class CFGManagerBDA : public CFGManagerPlayer, IBDATuner, IAMStreamSelect
{
public:
    CFGManagerBDA(HWND hWnd);
    ~CFGManagerBDA();

    // IGraphBuilder
    STDMETHODIMP RenderFile(LPCWSTR lpcwstrFile, LPCWSTR lpcwstrPlayList);

    // IFilterGraph
    STDMETHODIMP ConnectDirect(IPin* pPinOut, IPin* pPinIn, const AM_MEDIA_TYPE* pmt);

    // IBDATuner
    STDMETHODIMP SetChannel(int nChannelPrefNumber);
    STDMETHODIMP SetAudio(int nAudioIndex);
    STDMETHODIMP SetFrequency(ULONG ulFrequency, ULONG ulBandwidth, ULONG ulSymbolRate);
    STDMETHODIMP Scan(ULONG ulFrequency, ULONG ulBandwidth, ULONG ulSymbolRate, HWND hWnd);
    STDMETHODIMP GetStats(BOOLEAN& bPresent, BOOLEAN& bLocked, LONG& lDbStrength, LONG& lPercentQuality);

    // IAMStreamSelect
    STDMETHODIMP Count(DWORD* pcStreams);
    STDMETHODIMP Enable(long lIndex, DWORD dwFlags);
    STDMETHODIMP Info(long lIndex, AM_MEDIA_TYPE** ppmt, DWORD* pdwFlags, LCID* plcid, DWORD* pdwGroup, WCHAR** ppszName, IUnknown** ppObject, IUnknown** ppUnk);

    DECLARE_IUNKNOWN;
    STDMETHODIMP NonDelegatingQueryInterface(REFIID riid, void** ppv);
    STDMETHODIMP UpdatePSI(const CBDAChannel* pChannel, EventDescriptor& NowNext);

private:

    CComQIPtr<IBDA_DeviceControl>        m_pBDAControl;
    CComPtr<IBDA_FrequencyFilter>        m_pBDAFreq;
    CComQIPtr<IBDA_SignalStatistics>     m_pBDATunerStats;
    CComPtr<IBDA_DigitalDemodulator>     m_pBDADemodulator;
    CComQIPtr<IBDA_SignalStatistics>     m_pBDADemodStats;
    CComPtr<IBDA_AutoDemodulate>         m_pBDAAutoDemulate;
    DVB_RebuildFilterGraph m_nDVBRebuildFilterGraph;
    CAtlMap<BDA_STREAM_TYPE, CDVBStream> m_DVBStreams;
    bool tunerIsATSC;

    BDA_STREAM_TYPE m_nCurVideoType;
    BDA_STREAM_TYPE m_nCurAudioType;
    bool            m_fHideWindow;
    CComPtr<IBaseFilter> m_pDemux;

    HRESULT         CreateKSFilter(IBaseFilter** ppBF, CLSID KSCategory, const CStringW& DisplayName);
    HRESULT         CreateKSFilterFN(IBaseFilter** ppBF, CLSID KSCategory, const CStringW& FriendlyName);
    HRESULT         ConnectFilters(IBaseFilter* pOutFiter, IBaseFilter* pInFilter);
    HRESULT         CreateMicrosoftDemux(CComPtr<IBaseFilter>& pMpeg2Demux);
    HRESULT         SetChannelInternal(CBDAChannel* pChannel);
    HRESULT         SwitchStream(BDA_STREAM_TYPE nOldType, BDA_STREAM_TYPE nNewType);
    HRESULT         ChangeState(FILTER_STATE nRequested);
    HRESULT         ClearMaps();
    FILTER_STATE    GetState();
    void UpdateMediaType(VIDEOINFOHEADER2* NewVideoHeader, CBDAChannel* pChannel);
    HRESULT Flush(BDA_STREAM_TYPE nVideoType, BDA_STREAM_TYPE nAudioType);

    template <class ITF>
    HRESULT SearchIBDATopology(const CComPtr<IBaseFilter>& pTuner, CComPtr<ITF>& pItf) {
        CComPtr<IUnknown> pUnk;
        HRESULT hr = SearchIBDATopology(pTuner, __uuidof(ITF), pUnk);

        if (SUCCEEDED(hr)) {
            hr = pUnk.QueryInterface(&pItf);
        }
        return !pItf ? E_NOINTERFACE : hr;
    }

    HRESULT SearchIBDATopology(const CComPtr<IBaseFilter>& pTuner, REFIID iid, CComPtr<IUnknown>& pUnk);
};
