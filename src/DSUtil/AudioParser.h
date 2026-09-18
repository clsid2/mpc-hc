/*
 * (C) 2011-2020 see Authors.txt
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

#include <MMReg.h>

#define AC3_SYNCWORD                 0x770B
#define AC3_SYNCWORD_LE              0x0B77
#define AAC_ADTS_SYNCWORD            0xF0FF
#define AAC_LATM_SYNCWORD             0x2B7

#define MPA_SYNCWORD                 0xE0FF

#define TRUEHD_SYNCWORD          0xBA6F72F8
#define MLP_SYNCWORD             0xBB6F72F8

#define IEC61937_SYNCWORD        0x4E1FF872

#define DTS_SYNCWORD_CORE_BE     0x0180FE7F
#define DTS_SYNCWORD_CORE_LE     0x80017FFE
#define DTS_SYNCWORD_CORE_14B_BE 0x00E8FF1F
#define DTS_SYNCWORD_CORE_14B_LE 0xE8001FFF
#define DTS_SYNCWORD_SUBSTREAM   0x25205864

#define EAC3_FRAME_TYPE_INDEPENDENT 0
#define EAC3_FRAME_TYPE_DEPENDENT   1
#define EAC3_FRAME_TYPE_AC3_CONVERT 2
#define EAC3_FRAME_TYPE_RESERVED    3

#define DCA_PROFILE_HD_HRA  0x01 // High Resolution Audio
#define DCA_PROFILE_HD_MA   0x02 // Master Audio
#define DCA_PROFILE_EXPRESS 0x03 // Express

enum {
	IEC61937_AC3                = 0x01,          ///< AC-3 data
	IEC61937_MPEG1_LAYER1       = 0x04,          ///< MPEG-1 layer 1
	IEC61937_MPEG1_LAYER23      = 0x05,          ///< MPEG-1 layer 2 or 3 data or MPEG-2 without extension
	IEC61937_MPEG2_EXT          = 0x06,          ///< MPEG-2 data with extension
	IEC61937_MPEG2_AAC          = 0x07,          ///< MPEG-2 AAC ADTS
	IEC61937_MPEG2_LAYER1_LSF   = 0x08,          ///< MPEG-2, layer-1 low sampling frequency
	IEC61937_MPEG2_LAYER2_LSF   = 0x09,          ///< MPEG-2, layer-2 low sampling frequency
	IEC61937_MPEG2_LAYER3_LSF   = 0x0A,          ///< MPEG-2, layer-3 low sampling frequency
	IEC61937_DTS1               = 0x0B,          ///< DTS type I   (512 samples)
	IEC61937_DTS2               = 0x0C,          ///< DTS type II  (1024 samples)
	IEC61937_DTS3               = 0x0D,          ///< DTS type III (2048 samples)
	IEC61937_ATRAC              = 0x0E,          ///< Atrac data
	IEC61937_ATRAC3             = 0x0F,          ///< Atrac 3 data
	IEC61937_ATRACX             = 0x10,          ///< Atrac 3 plus data
	IEC61937_DTSHD              = 0x11,          ///< DTS HD data
	IEC61937_WMAPRO             = 0x12,          ///< WMA 9 Professional data
	IEC61937_MPEG2_AAC_LSF_2048 = 0x13,          ///< MPEG-2 AAC ADTS half-rate low sampling frequency
	IEC61937_MPEG2_AAC_LSF_4096 = 0x13 | 0x20,   ///< MPEG-2 AAC ADTS quarter-rate low sampling frequency
	IEC61937_EAC3               = 0x15,          ///< E-AC-3 data
	IEC61937_TRUEHD             = 0x16,          ///< TrueHD data
};


DWORD GetDefChannelMask(WORD nChannels);

// need >= 4 bytes (experimental)
int ParseMP3Header         (const BYTE* buf, MPEGLAYER3WAVEFORMAT* mp3wf);
