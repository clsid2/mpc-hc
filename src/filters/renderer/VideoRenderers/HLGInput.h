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

#include "HLGToSDR.h"

// Is the video reaching the renderer HLG? The mixer's input type doesn't always carry the
// colour attributes, so when it doesn't say HLG, the media type of the renderer's input pin
// is checked too: the decoder (e.g. LAV Video) sets the transfer function in the DXVA2
// extended format held in VIDEOINFOHEADER2's dwControlFlags.

inline bool MixerTypeIsHLG(IMFMediaType* pMixerInputType)
{
    UINT32 transferFunction;
    return pMixerInputType && SUCCEEDED(pMixerInputType->GetUINT32(MF_MT_TRANSFER_FUNCTION, &transferFunction))
           && transferFunction == TRANSFER_FUNCTION_HLG;
}

// Not from within the mixer's type negotiation: the EVR is connecting its pin then and
// querying it deadlocks. The presenters call it from the render thread instead.
inline bool InputPinIsHLG(IBaseFilter* pRenderer)
{
    CComPtr<IEnumPins> pEnumPins;
    if (!pRenderer || FAILED(pRenderer->EnumPins(&pEnumPins))) {
        return false;
    }
    for (CComPtr<IPin> pPin; pEnumPins->Next(1, &pPin, nullptr) == S_OK; pPin.Release()) {
        PIN_DIRECTION dir;
        CMediaType mt;
        if (FAILED(pPin->QueryDirection(&dir)) || dir != PINDIR_INPUT || FAILED(pPin->ConnectionMediaType(&mt))
                || mt.formattype != FORMAT_VideoInfo2 || mt.cbFormat < sizeof(VIDEOINFOHEADER2)) {
            continue;
        }
        const DWORD flags = ((const VIDEOINFOHEADER2*)mt.pbFormat)->dwControlFlags;
        // DXVA2_ExtendedFormat: VideoTransferFunction is the 5 bits from bit 27
        if ((flags & AMCONTROL_COLORINFO_PRESENT) && ((flags >> 27) & 0x1F) == TRANSFER_FUNCTION_HLG) {
            return true;
        }
    }
    return false;
}
