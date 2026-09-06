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

#include "CastTarget.h"

// Reducing a file's audio to fewer channels so a device that cannot output the
// original layout plays it with sound instead of dropping it silently. The
// video is copied untouched; only the audio is decoded, downmixed and
// re-encoded (to AAC), and the two are remuxed into a new MP4.
//
// Two engines share the work behind one door. Media Foundation decodes and
// downmixes in a single pass for the codecs it can read at 5.1 or below (AAC,
// MP3, FLAC, LPCM) -- all in OS code, nothing added to the player's FFmpeg. What
// it cannot read -- DTS, TrueHD, 7.1 -- goes through the player's own LAV
// decoder to a temp WAV that MF then muxes against the copied video. Both need
// Media Foundation to demux the container for the video copy, so both are
// limited to MP4-family containers (not Matroska or WebM, which stay refused).
// CastCanDownmix() is the single place that says what can be taken, so the
// decision to downmix or refuse stays honest.

// Whether either engine can downmix this file: an MP4-family container, an audio
// codec one of them can decode, and video that copies into MP4.
bool CastCanDownmix(const CString& srcPath, const CastMediaInfo& info);

// Produces outPath: an MP4 with srcPath's video copied and its audio downmixed
// to targetChannels and re-encoded as AAC. info picks the engine. Blocks until
// finished; the result is a complete, seekable file the media server can serve
// with Range. Returns false (and a reason in pError) on any failure, leaving no
// output behind.
bool CastDownmixToMp4(const CString& srcPath, const CastMediaInfo& info, int targetChannels,
                      const CString& outPath, CString* pError = nullptr);
