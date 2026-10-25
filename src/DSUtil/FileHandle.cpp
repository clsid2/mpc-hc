/*
 * (C) 2011-2021 see Authors.txt
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
#include "FileHandle.h"

//
// Returns the file portion from a path
//
CStringW GetFileOnly(LPCWSTR Path)
{
	CStringW cs = Path;
	::PathStripPathW(cs.GetBuffer(0));
	cs.ReleaseBuffer(-1);
	return cs;
}

//
// Returns the folder portion from a path
//
CStringW GetFolderOnly(LPCWSTR Path)
{
	CStringW cs = Path; // Force CStringW to make a copy
	::PathRemoveFileSpecW(cs.GetBuffer(0));
	cs.ReleaseBuffer(-1);
	return cs;
}

//
// Adds a backslash to the end of a path if it is needed
//
CStringW AddSlash(LPCWSTR Path)
{
	CStringW cs = Path;
	::PathAddBackslashW(cs.GetBuffer(MAX_PATH));
	cs.ReleaseBuffer(-1);
	if(cs.IsEmpty()) {
		cs = L"\\";
	}
	return cs;
}

//
// Removes a backslash from the end of a path if it is there
//
CStringW RemoveSlash(LPCWSTR Path)
{
	CString cs = Path;
	::PathRemoveBackslashW(cs.GetBuffer(MAX_PATH));
	cs.ReleaseBuffer(-1);
	return cs;
}

//
// Returns just the .ext part of the file path
//
CStringW GetFileExt(LPCWSTR Path)
{
    if (::PathIsURLW(Path)) {
        auto q = wcschr(Path, '?');
        if (q) {
            CStringW ext = ::PathFindExtensionW(CStringW(Path, q - Path));
            return ext;
        }
    }
    CStringW ext = ::PathFindExtensionW(Path);
    return ext;
}

//
// Exchanges one file extension for another and returns the new file path
//
CStringW RenameFileExt(LPCWSTR Path, LPCWSTR Ext)
{
	CStringW cs = Path;
	::PathRenameExtensionW(cs.GetBuffer(MAX_PATH), Ext);
	cs.ReleaseBuffer(-1);
	return cs;
}

//
// Removes the file name extension from a path, if one is present
//
CStringW RemoveFileExt(LPCWSTR Path)
{
	CStringW cs = Path;
	::PathRemoveExtensionW(cs.GetBuffer(MAX_PATH));
	cs.ReleaseBuffer(-1);
	return cs;
}

//
// Generate temporary files with any extension
//
BOOL GetTemporaryFilePath(CStringW strExtension, CStringW& strFileName)
{
	WCHAR lpszTempPath[MAX_PATH] = { 0 };
	if (!GetTempPathW(MAX_PATH, lpszTempPath)) {
		return FALSE;
	}

	WCHAR lpszFilePath[MAX_PATH] = { 0 };
	do {
		if (!GetTempFileNameW(lpszTempPath, L"mpc", 0, lpszFilePath)) {
			return FALSE;
		}

		DeleteFile(lpszFilePath);

		strFileName = lpszFilePath;
		strFileName.Replace(L".tmp", strExtension);

		DeleteFile(strFileName);
	} while (_waccess(strFileName, 00) != -1);

	return TRUE;
}

//
// Compact Path
//
CStringW CompactPath(LPCWSTR Path, UINT cchMax)
{
	CStringW cs = Path;
	WCHAR pathbuf[MAX_PATH] = { 0 };
	if (::PathCompactPathExW(pathbuf, cs, cchMax, 0)) {
		cs = pathbuf;
	}

	return cs;
}

//
// Get path of specified module
//
CStringW GetModulePath(HMODULE hModule)
{
	CStringW path;
	DWORD bufsize = MAX_PATH;
	DWORD len = 0;
	while (1) {
		len = GetModuleFileNameW(hModule, path.GetBuffer(bufsize), bufsize);
		if (len < bufsize) {
			break;
		}
		bufsize *= 2;
	}
	path.ReleaseBufferSetLength(len);

	ASSERT(path.GetLength());
	return path;
}

//
// Get path of the executable file of the current process.
//
CStringW GetProgramPath()
{
	return GetModulePath(nullptr);
}

//
// Get program directory with backslash
//
CStringW GetProgramDir()
{
	CStringW path = GetModulePath(nullptr);
	path.Truncate(path.ReverseFind('\\') + 1); // do not use this method for random paths

	return path;
}

// wFunc can be FO_MOVE or FO_COPY.
// To move a folder, add "\" to the end of the source path.
// To copy a folder, add "\*" to the end of the source path.
int FileOperation(const CStringW& source, const CStringW& target, const UINT wFunc)
{
	auto from_str = std::make_unique<WCHAR[]>(source.GetLength() + 2);
	wcscpy_s(from_str.get(), source.GetLength()+1, source);

	auto to_str = std::make_unique<WCHAR[]>(target.GetLength() + 2);
	wcscpy_s(to_str.get(), target.GetLength()+1, target);

	// set double null-terminated string
	from_str[wcslen(from_str.get())+1] = 0;
	to_str[wcslen(to_str.get())+1] = 0;

	SHFILEOPSTRUCTW FileOp = { 0 };
	FileOp.wFunc  = wFunc;
	FileOp.pFrom  = from_str.get();
	FileOp.pTo    = to_str.get();
	FileOp.fFlags = FOF_NO_UI;

	return SHFileOperationW(&FileOp);
}
