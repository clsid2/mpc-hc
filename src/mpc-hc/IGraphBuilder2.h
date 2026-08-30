/*
 * (C) 2003-2006 Gabest
 * (C) 2006-2014, 2016-2017 see Authors.txt
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

class CMediaType;

interface __declspec(uuid("165BE9D6-0929-4363-9BA3-580D735AA0F6"))
    IGraphBuilder2 :
    public IFilterGraph2
{
    STDMETHOD(IsPinDirection)(IPin* pPin, PIN_DIRECTION dir) PURE;
    STDMETHOD(IsPinConnected)(IPin* pPin) PURE;
    STDMETHOD(ConnectFilter)(IBaseFilter* pBF, IPin* pPinIn) PURE;
    STDMETHOD(ConnectFilter)(IPin* pPinOut, IBaseFilter* pBF) PURE;
    STDMETHOD(ConnectFilterDirect)(IPin* pPinOut, IBaseFilter* pBF, const AM_MEDIA_TYPE* pmt) PURE;
    STDMETHOD(NukeDownstream)(IUnknown* pUnk) PURE;
    STDMETHOD(FindInterface)(REFIID iid, void** ppv, BOOL bRemove) PURE;
    STDMETHOD(AddToROT)() PURE;
    STDMETHOD(RemoveFromROT)() PURE;
};

// private use only
interface __declspec(uuid("43CDA93D-6A4E-4A07-BD3E-49D161073EE7"))
    IGraphBuilderDeadEnd :
    public IUnknown
{
    STDMETHOD_(size_t, GetCount)() PURE;
    STDMETHOD(GetDeadEnd)(int iIndex, CAtlList<CStringW>& path, CAtlList<CMediaType>& mts) PURE;
};

// private use only
interface __declspec(uuid("546E72B3-66A1-4A58-A99B-56530B3E2FFF"))
    IBDATuner :
    public IUnknown
{
    STDMETHOD(SetChannel)(int nChannelPrefNumber) PURE;
    STDMETHOD(SetAudio)(int nAudioIndex) PURE;
    STDMETHOD(SetFrequency)(ULONG ulFrequency, ULONG ulBandwidth, ULONG ulSymbolRate) PURE;
    STDMETHOD(Scan)(ULONG ulFrequency, ULONG ulBandwidth, ULONG ulSymbolRate, HWND hWnd) PURE;
    STDMETHOD(GetStats)(BOOLEAN& bPresent, BOOLEAN& bLocked, LONG& lDbStrength, LONG& lPercentQuality) PURE;
    STDMETHOD(UpdatePSI)(const class CBDAChannel* pChannel, struct EventDescriptor& NowNext) PURE;
    // ATSC and DVB differ in how their RF channels are laid out, so a scan has
    // to know which it is dealing with rather than assuming a uniform raster.
    STDMETHOD(IsATSC)(bool& bIsATSC) PURE;
};
