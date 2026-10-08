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

// MFVideoTransFunc_HLG, which the Windows 8.1 SDK doesn't declare.
constexpr UINT32 TRANSFER_FUNCTION_HLG = 16;

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
