/*
 * Copyright (C) 2008-2012, OctaneSnail <os@v12pwr.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <windows.h>
#include <streams.h>

#include "File.h"
#include "Utils.h"
#include "unrar/rar.hpp"

#include <vector>

// Builds the volume-extent index far enough to cover byte offset upTo (or to
// the last volume, whichever comes first). Only stored (Method==0) files are
// playable, so the mapping from unpacked offset to a position inside a volume
// file is exact: each volume holds one contiguous chunk of the file's bytes.
// Returns true if at least the first extent is known.
bool CRFSFile::EnsureExtents(LONGLONG upTo)
{
    CAutoLock lock(&m_extentLock);

    if (m_extents.empty()) {
        Archive arc;
        arc.SetExceptions(false);
        if (!arc.Open(rarFilename) || !arc.IsArchive(false)) {
            ErrorMsg(GetLastError(), L"CRFSFile::EnsureExtents - Archive Open");
            return false;
        }
        arc.Seek(startingBlockPos, SEEK_SET);
        if (0 == arc.SearchBlock(HEAD_FILE)) {
            ErrorMsg(GetLastError(), L"CRFSFile::EnsureExtents - SearchBlock");
            return false;
        }
        if (arc.FileHead.Method != 0 || arc.FileHead.Encrypted || arc.FileHead.SplitBefore) {
            return false;
        }
        VolumeExtent e;
        e.start = 0;
        e.size = arc.FileHead.PackSize;
        e.dataPos = arc.NextBlockPos - arc.FileHead.PackSize;
        e.volume = arc.FileName;
        m_extents.push_back(e);
        m_extentsComplete = !arc.FileHead.SplitAfter;
    }

    bool newNumbering = true;
    bool oldSchemeTested = false;

    while (!m_extentsComplete) {
        const VolumeExtent& prev = m_extents.back();
        if (prev.start + prev.size > upTo) {
            break; // already cover the requested range
        }

        // Same volume-name sequencing MergeArchive uses, including its
        // fallback for new-style sets renamed to the old naming scheme.
        std::wstring nextName = prev.volume;
        NextVolumeName(nextName, !newNumbering);

        Archive arc;
        arc.SetExceptions(false);
        bool opened = arc.Open(nextName) && arc.IsArchive(false);
        if (!opened && newNumbering && !oldSchemeTested) {
            oldSchemeTested = true;
            std::wstring altName = prev.volume;
            NextVolumeName(altName, true);
            opened = arc.Open(altName) && arc.IsArchive(false);
            if (opened) {
                nextName = altName;
                newNumbering = false;
            }
        }
        if (!opened) {
            break; // missing/broken next volume: serve what we have
        }
        newNumbering = arc.NewNumbering;

        // The continuation chunk is the first file block in the volume.
        if (0 == arc.SearchBlock(HEAD_FILE)) {
            break;
        }
        if (arc.FileHead.Method != 0 || arc.FileHead.Encrypted || !arc.FileHead.SplitBefore) {
            break;
        }
        VolumeExtent e;
        e.start = prev.start + prev.size;
        e.size = arc.FileHead.PackSize;
        e.dataPos = arc.NextBlockPos - arc.FileHead.PackSize;
        e.volume = nextName;
        m_extents.push_back(e);
        m_extentsComplete = !arc.FileHead.SplitAfter;
    }

    return !m_extents.empty();
}

CRFSFile::ReadThread::ReadThread(CRFSFile* file, LONGLONG llPosition, DWORD lLength, BYTE* pBuffer) {
    this->file = file;
    this->llPosition = llPosition;
    this->lLength = lLength;
    this->pBuffer = pBuffer;
    this->read = 0;
}

DWORD CRFSFile::ReadThread::ThreadStart() {
    if (file) {
        return file->SyncRead(llPosition, lLength, pBuffer, &read);
    } else {
        return S_FALSE;
    }
}

DWORD WINAPI CRFSFile::ReadThread::ThreadStartStatic(void *param) {
    ReadThread* t = (ReadThread*)param;
    if (t) {
        return t->ThreadStart();
    } else {
        return S_FALSE;
    }
}

HRESULT CRFSFile::SyncRead(LONGLONG llPosition, DWORD lLength, BYTE* pBuffer, LONG* cbActual) {
    if (llPosition < 0) {
        return E_FAIL;
    }
    if (lLength == 0) {
        if (cbActual)
            *cbActual = 0;
        return S_OK;
    }

    const LONGLONG last = llPosition + lLength - 1;
    EnsureExtents(last);

    // Snapshot the index so file I/O runs outside the lock. One entry per
    // volume visited so far; trivial to copy.
    std::vector<VolumeExtent> extents;
    {
        CAutoLock lock(&m_extentLock);
        if (m_extents.empty()) {
            return E_FAIL;
        }
        extents = m_extents;
    }

    // First extent whose end lies beyond the start position.
    size_t i = 0;
    {
        size_t lo = 0, hi = extents.size();
        while (lo < hi) {
            size_t mid = (lo + hi) / 2;
            if (extents[mid].start + extents[mid].size <= llPosition) {
                lo = mid + 1;
            } else {
                hi = mid;
            }
        }
        i = lo;
    }

    size_t totalRead = 0;
    LONGLONG pos = llPosition;

    while (pos <= last && i < extents.size()) {
        const VolumeExtent& e = extents[i];
        if (pos < e.start) {
            break; // gap: should not happen with sequentially built extents
        }

        const LONGLONG avail = e.start + e.size - pos;
        const LONGLONG remaining = last - pos + 1;
        const size_t want = (size_t)(avail < remaining ? avail : remaining);

        File vol;
        vol.SetExceptions(false);
        if (!vol.Open(e.volume)) {
            ErrorMsg(GetLastError(), L"CRFSFile::SyncRead - volume open");
            break;
        }
        vol.Seek(e.dataPos + (pos - e.start), SEEK_SET);
        int r = vol.Read(pBuffer + (size_t)(pos - llPosition), want);
        if (r <= 0) {
            break;
        }

        totalRead += r;
        pos += r;
        if ((size_t)r < want) {
            break; // truncated volume: serve what we have
        }
        i++;
    }

    if (cbActual)
        *cbActual = (LONG)totalRead;
    return totalRead > 0 ? S_OK : E_FAIL;
}


