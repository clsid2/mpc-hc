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

#ifndef FILE_H
#define FILE_H

#include "List.h"
#include "unrar/rartypes.hpp"

#include <string>
#include <vector>

class Archive;

class CRFSFile : public CRFSNode<CRFSFile>
{
public:
	CRFSFile (void) : size (0), filename (NULL), rarFilename(NULL), startingBlockPos(0), m_extentsComplete(false) { }

	~CRFSFile (void)
	{
		delete [] filename;
		delete [] rarFilename;
	}

    class ReadThread {
    public:
        ReadThread(CRFSFile* file, LONGLONG llPosition, DWORD lLength, BYTE* pBuffer);
        DWORD ThreadStart();
        static DWORD WINAPI ThreadStartStatic(void* param);
        CRFSFile* file;
        LONGLONG llPosition;
        DWORD lLength;
        BYTE* pBuffer;
        LONG read;
    };
    static HRESULT SyncRead(void *param);
    HRESULT SyncRead (LONGLONG llPosition, DWORD lLength, BYTE* pBuffer, LONG *cbActual);

	CMediaType media_type;
	LONGLONG size;
    int64 startingBlockPos;

	wchar_t *filename, *rarFilename;

private:
    // Maps a range of the stored (uncompressed) file onto its location inside
    // a specific volume, so reads can address volume files directly instead of
    // walking every volume through the unrar extraction machinery.
    struct VolumeExtent {
        LONGLONG start;      // unpacked offset of this chunk within the file
        LONGLONG size;       // chunk byte count (PackSize of this volume's piece)
        LONGLONG dataPos;    // offset of the chunk data inside the volume file
        std::wstring volume; // volume file path
    };

    bool EnsureExtents(LONGLONG upTo);

    std::vector<VolumeExtent> m_extents; // built lazily, guarded by m_extentLock
    bool m_extentsComplete;              // last volume reached
    CCritSec m_extentLock;
};

#endif // FILE_H
