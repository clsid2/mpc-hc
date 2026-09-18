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

#include "stdafx.h"
#include "AudioParser.h"
#include <mpc_defines.h>

// Channel masks

DWORD GetDefChannelMask(WORD nChannels)
{
	switch (nChannels) {
		case 1: // 1.0 Mono (KSAUDIO_SPEAKER_MONO)
			return SPEAKER_FRONT_CENTER;
		case 2: // 2.0 Stereo (KSAUDIO_SPEAKER_STEREO)
			return SPEAKER_FRONT_LEFT | SPEAKER_FRONT_RIGHT;
		case 3: // 2.1 Stereo
			return SPEAKER_FRONT_LEFT | SPEAKER_FRONT_RIGHT | SPEAKER_LOW_FREQUENCY;
		case 4: // 4.0 Quad (KSAUDIO_SPEAKER_QUAD)
			return SPEAKER_FRONT_LEFT | SPEAKER_FRONT_RIGHT
				   | SPEAKER_BACK_LEFT | SPEAKER_BACK_RIGHT;
		case 5: // 5.0
			return SPEAKER_FRONT_LEFT | SPEAKER_FRONT_RIGHT | SPEAKER_FRONT_CENTER
				   | SPEAKER_BACK_LEFT | SPEAKER_BACK_RIGHT;
		case 6: // 5.1 Side (KSAUDIO_SPEAKER_5POINT1_SURROUND)
			return SPEAKER_FRONT_LEFT | SPEAKER_FRONT_RIGHT | SPEAKER_FRONT_CENTER
				   | SPEAKER_LOW_FREQUENCY | SPEAKER_SIDE_LEFT | SPEAKER_SIDE_RIGHT;
		case 7: // 6.1 Side
			return SPEAKER_FRONT_LEFT | SPEAKER_FRONT_RIGHT | SPEAKER_FRONT_CENTER
				   | SPEAKER_LOW_FREQUENCY | SPEAKER_BACK_CENTER
				   | SPEAKER_SIDE_LEFT | SPEAKER_SIDE_RIGHT;
		case 8: // 7.1 Surround (KSAUDIO_SPEAKER_7POINT1_SURROUND)
			return SPEAKER_FRONT_LEFT | SPEAKER_FRONT_RIGHT | SPEAKER_FRONT_CENTER
				   | SPEAKER_LOW_FREQUENCY | SPEAKER_BACK_LEFT | SPEAKER_BACK_RIGHT
				   | SPEAKER_SIDE_LEFT | SPEAKER_SIDE_RIGHT;
		case 10: // 9.1 Surround
			return SPEAKER_FRONT_LEFT | SPEAKER_FRONT_RIGHT | SPEAKER_FRONT_CENTER
				   | SPEAKER_LOW_FREQUENCY | SPEAKER_BACK_LEFT | SPEAKER_BACK_RIGHT
				   | SPEAKER_SIDE_LEFT | SPEAKER_SIDE_RIGHT
				   | SPEAKER_TOP_FRONT_LEFT | SPEAKER_TOP_FRONT_RIGHT;
		case 12: // 11.1
			return SPEAKER_FRONT_LEFT | SPEAKER_FRONT_RIGHT | SPEAKER_FRONT_CENTER
				   | SPEAKER_LOW_FREQUENCY | SPEAKER_BACK_LEFT | SPEAKER_BACK_RIGHT
				   | SPEAKER_FRONT_LEFT_OF_CENTER | SPEAKER_FRONT_RIGHT_OF_CENTER
				   | SPEAKER_TOP_FRONT_LEFT | SPEAKER_TOP_FRONT_RIGHT
				   | SPEAKER_TOP_BACK_LEFT | SPEAKER_TOP_BACK_RIGHT;
		default:
			return 0;
	}
}

// MPEG Audio

// http://mpgedit.org/mpgedit/mpeg_format/mpeghdr.htm
// http://www.codeproject.com/Articles/8295/MPEG-Audio-Frame-Header

static const short mpeg1_rates[3][16] = {
	{ 0, 32, 64, 96, 128, 160, 192, 224, 256, 288, 320, 352, 384, 416, 448, 0 }, // MPEG 1 Layer 1
	{ 0, 32, 48, 56,  64,  80,  96, 112, 128, 160, 192, 224, 256, 320, 384, 0 }, // MPEG 1 Layer 2
	{ 0, 32, 40, 48,  56,  64,  80,  96, 112, 128, 160, 192, 224, 256, 320, 0 }, // MPEG 1 Layer 3
};
static const int mpeg1_samplerates[] = { 44100, 48000, 32000, 0 };

int ParseMP3Header(const BYTE* buf, MPEGLAYER3WAVEFORMAT* mp3wf) // experimental
{
	// http://msdn.microsoft.com/en-us/library/windows/desktop/dd390710%28v=vs.85%29.aspx

	if ((GETU16(buf) & 0xfeff) != 0xfaff) { // sync + (mpaver_id = MPEG Version 1 = 11b) + (layer_desc = Layer 3 = 01b)
		return 0;
	}

	BYTE bitrate_index    = buf[2] >> 4;
	BYTE samplerate_index = (buf[2] & 0x0c) >> 2;
	if (bitrate_index == 0 || bitrate_index == 15 || samplerate_index == 3) {
		return 0;
	}
	BYTE pading_bit       = (buf[2] & 0x02) >> 1;
	BYTE channel_mode     = (buf[3] & 0xc0) >> 6;

	DWORD bitrate = (DWORD)mpeg1_rates[2][bitrate_index] * 1000;

	mp3wf->wfx.wFormatTag      = WAVE_FORMAT_MPEGLAYER3;
	mp3wf->wfx.nChannels       = channel_mode == 0x3 ? 1 : 2;
	mp3wf->wfx.nSamplesPerSec  = mpeg1_samplerates[samplerate_index];
	mp3wf->wfx.nAvgBytesPerSec = bitrate / 8;
	mp3wf->wfx.nBlockAlign     = 144 * bitrate / mp3wf->wfx.nSamplesPerSec; // without pading_bit ?
	mp3wf->wfx.wBitsPerSample  = 0;
	mp3wf->wfx.cbSize          = MPEGLAYER3_WFX_EXTRA_BYTES;

	mp3wf->wID             = MPEGLAYER3_ID_MPEG;
	mp3wf->fdwFlags        = MPEGLAYER3_FLAG_PADDING_ISO; // clarify on average bitrate.
	mp3wf->nBlockSize      = 144 * bitrate / mp3wf->wfx.nSamplesPerSec + pading_bit;
	mp3wf->nFramesPerBlock = 1152;
	mp3wf->nCodecDelay     = 0;

	return mp3wf->nBlockSize;
}

