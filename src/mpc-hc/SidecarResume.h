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
#include "stdafx.h"

// Content-based identity hash of a media file, hex-encoded.
// Returns an empty string if the file cannot be read.
CStringW ComputeContentHash(const CString& filePath);

// Read <filePath>.mpcresume. Returns true and sets outPos only if the
// sidecar exists, parses, and its hash matches ComputeContentHash(filePath).
bool LoadSidecarPosition(const CString& filePath, REFERENCE_TIME& outPos);

// Writes <filePath>.mpcresume. Silently does nothing on any failure
// (read-only media folder, sharing violation, ...). force skips the 30s
// rate limit, mirroring UpdateCurrentFilePosition's forcePersist.
void SaveSidecarPosition(const CString& filePath, REFERENCE_TIME position, bool force = false);
