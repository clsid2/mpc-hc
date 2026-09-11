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

#include <atomic>
#include <vector>

struct MkvMetadataEntry {
    CString name;
    CString value;
};

struct MkvCastMember {
    CString actor;
    CString role;
};

struct MkvMetadata {
    CString title;
    CString releaseDate;
    std::vector<MkvMetadataEntry> creators;
    std::vector<MkvCastMember> cast;

    bool HasDisplayableData() const;
};

class CMkvMetadataReader
{
public:
    // This deadline is shared with the UI so a stalled network open cannot
    // leave the Properties sheet in its loading state indefinitely.
    static constexpr ULONGLONG READ_TIMEOUT_MS = 5000;

    static bool Read(LPCTSTR path, MkvMetadata& metadata, const std::atomic_bool* cancelled = nullptr,
                     ULONGLONG startTime = 0);
};
