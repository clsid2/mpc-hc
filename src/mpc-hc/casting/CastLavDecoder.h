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

// Decodes the first audio track of srcPath with the player's own LAV filters --
// which read what Media Foundation cannot, DTS and 7.1 among them -- downmixes it
// to stereo, and writes it as FLAC to outFlacPath (compressed as it flows, so no
// gigabyte PCM WAV ever lands on disk). A short DirectShow graph run to end of
// stream, whose audio renderer hands the PCM to a Media Foundation FLAC writer.
// The caller then muxes that FLAC against the original file's copied video.
// targetChannels is accepted for the interface's sake; the LAV path is stereo.
// hCancel, when set, is checked while the graph runs and abandons the decode
// with a "cancelled" failure. Returns false (with a reason in pError) on any
// failure, leaving no output.
bool CastLavDecodeToFlac(const CString& srcPath, int targetChannels, const CString& outFlacPath,
                         CString* pError = nullptr, HANDLE hCancel = nullptr);
