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

#include <atlstr.h>
#include "CastTranscoder.h"

// Decodes the first audio track of srcPath with the player's own LAV filters --
// which read what Media Foundation cannot, DTS and 7.1 among them -- downmixes it
// to stereo, and writes it as FLAC to outFlacPath (compressed as it flows, so no
// gigabyte PCM WAV ever lands on disk). A short DirectShow graph run to end of
// stream, whose audio renderer hands the PCM to a Media Foundation FLAC writer.
// The caller then muxes that FLAC against the original file's copied video.
// targetChannels is accepted for the interface's sake; the LAV path is stereo
// unless preserveLayout is set, when the source's own channel layout is kept
// (a surround-capable renderer that takes FLAC is handed its surround, since
// FLAC -- unlike Media Foundation's AAC encoder -- encodes more than two
// channels). hCancel, when set, is checked while the graph runs and abandons
// the decode with a "cancelled" failure. Returns false (with a reason in
// pError) on any failure, leaving no output.
bool CastLavDecodeToFlac(const CString& srcPath, int targetChannels, const CString& outFlacPath,
                         CString* pError = nullptr, HANDLE hCancel = nullptr,
                         bool preserveLayout = false);

// Remuxes a Matroska or WebM file -- a container Media Foundation cannot demux
// at all -- into an MP4 in one DirectShow graph: the LAV splitter reads the
// file, its compressed H.264/HEVC video is taken off the video pin and
// rewritten from length-prefixed to Annex-B NALUs -- the form the MP4 sink
// consumes -- with a decode timestamp synthesized beside its presentation
// times (the splitter delivers video in decode order, PTS only), so nothing
// is decoded, and the audio, decoded and downmixed to stereo by the LAV audio
// decoder, is encoded to AAC beside it. Video and audio flow to the two
// streams of one shared sink writer, one from each of the splitter's
// streaming threads, which is why every sample is written under a common
// mutex and the writer is created with its throttling disabled. fragmented
// writes the segmented output the HLS path tails as it grows. info is logged,
// not trusted: the graph reads what the file actually holds. Fails like the
// other engines, cancellation included, leaving no output.
bool CastLavRemuxToMp4(const CString& srcPath, const CastMediaInfo& info, int targetChannels,
                       const CString& outPath, bool fragmented, const CastTranscodeProgress& prog,
                       CString* pError = nullptr);

// The surround variant of the remux above, for a device that plays E-AC-3:
// the same LAV graph -- splitter, compressed video off the pin, audio through
// the LAV decoder -- but the output side is the player's FFmpeg instead of the
// Media Foundation sink writer. One "mp4" muxer takes the H.264/HEVC samples
// length-prefixed exactly as the pin delivers them, no Annex-B conversion,
// with the format block's sequence header as the track's extradata; beside
// them the decoded PCM -- the source's own multichannel layout, mixing off,
// never downmixed -- is resampled to planar float and encoded to E-AC-3 as it
// flows (FLTP at 384 kbit/s up to 5.1, 640 above). The E-AC-3 encoder takes a
// fixed set of channel layouts and refuses the rest -- 7.1 among them -- and
// that refusal is a clean failure, logged, so the caller can fall back to the
// stereo AAC engine. targetChannels is what routed the call here; it is not
// applied. Non-fragmented output only. Fails like the other engines,
// cancellation included, leaving no output.
bool CastLavRemuxToEac3Mp4(const CString& srcPath, const CastMediaInfo& info, int targetChannels,
                           const CString& outPath, const CastTranscodeProgress& prog,
                           CString* pError = nullptr);
