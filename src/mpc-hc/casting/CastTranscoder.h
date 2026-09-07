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
#include <functional>
#include <vector>

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
// Media Foundation to demux the container for the video copy, so both take
// MP4-family containers only. Matroska and WebM, which Media Foundation cannot
// demux at all, take a third route: the LAV splitter reads the container, the
// compressed video is copied off its video pin, and the decoded audio joins it
// in one pass (CastLavRemuxToMp4). CastCanDownmix() is the single place that
// says what can be taken, so the decision to downmix or refuse stays honest.

// Whether either engine can downmix this file: a container one of them can
// demux (MP4 family, Matroska, WebM), an audio codec one of them can decode,
// and video that copies into MP4.
bool CastCanDownmix(const CString& srcPath, const CastMediaInfo& info);

// Produces outPath: an MP4 with srcPath's video copied and its audio downmixed
// to targetChannels and re-encoded as AAC. info picks the engine. Blocks until
// finished; the result is a complete, seekable file the media server can serve
// with Range. Returns false (and a reason in pError) on any failure, leaving no
// output behind. hCancel, when given, is polled while the transcode pumps so a
// caller running this on a worker thread can abandon it (the call then fails
// with "cancelled", leaving no output) -- the whole-file transcode is otherwise
// long enough to freeze a UI thread it were called on directly.
bool CastDownmixToMp4(const CString& srcPath, const CastMediaInfo& info, int targetChannels,
                      const CString& outPath, CString* pError = nullptr, HANDLE hCancel = nullptr);

// Optional eyes and a brake for the streaming variant. Both are checked about
// every 32 samples while the transcode pumps: hCancel set abandons the work
// (the call then fails with "cancelled"), onProgress is the HLS segmenter's
// chance to tail the output file as it grows.
struct CastTranscodeProgress {
    HANDLE hCancel = nullptr;
    std::function<void()> onProgress;
};

// The streaming variant: same two engines and the same routing as
// CastDownmixToMp4, but outPath is written as a fragmented MP4 whose moov
// precedes the samples, so a segmenter can read whole fragments out of it
// while it is still being written. Only takes a file with H.264 video; an
// audio-only or HEVC source is refused (the complete-file path serves those).
// Fails like CastDownmixToMp4 does, cancellation included, leaving no output.
bool CastDownmixToFragmentedMp4(const CString& srcPath, const CastMediaInfo& info, int targetChannels,
                                const CString& outPath, const CastTranscodeProgress& prog, CString* pError = nullptr);

// Plumbing the engines share, declared here so both CastTranscoder.cpp and
// CastLavDecoder.cpp reach it. The Media Foundation interfaces are spelled as
// forward-declared raw pointers so this header stays free of MF includes.
struct IMFMediaType;
struct IMFSinkWriter;
struct IMFMediaSink;

// Builds the MP4 output every engine writes into. The non-fragmented variant
// is the plain sink writer over a file URL. The fragmented one goes through
// MFCreateFMPEG4MediaSink on a byte stream -- the moov lands up front and
// samples are cut into moof/mdat fragments as they arrive, which is what lets
// a segmenter read the file while it is still being written; the sink comes
// back separately because it must be ShutDown after Finalize or the file
// stays open. videoNative may be null for the complete-file path (an
// audio-only downmix is a legitimate plain MP4) but not for the fragmented
// one; stream 0 is video, stream 1 audio there. The video type serves as both
// the stream's output and input type, so a compressed track passes through
// untouched.
bool CastCreateMp4Writer(const CString& outPath, bool fragmented, IMFMediaType* videoNative,
                         IMFMediaType* pcmActual, UINT32 sampleRate, UINT32 chans,
                         IMFSinkWriter** ppWriter, IMFMediaSink** ppSink,
                         DWORD& outVideo, DWORD& outAudio, CString* pError);

// Parses an hvcC record -- an HEVC decoder configuration, as carried in an
// MP4's hvcC box or on a Matroska video pin's format block -- into the
// VPS/SPS/PPS NALs it holds, as an Annex-B blob (each NAL preceded by
// 00 00 00 01), the form MF_MT_MPEG_SEQUENCE_HEADER wants. Everything else the
// record carries (type-39 SEI among it) is left out.
bool CastHvcCToAnnexB(const BYTE* hvcc, size_t len, std::vector<BYTE>& seq);
