/*
 * (C) 2013-2015 see Authors.txt
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

#include <atlpath.h>

// CPath builds the result of these methods in a MAX_PATH buffer around a Shlwapi call, so past
// MAX_PATH they fail, and the void ones do it silently: Combine and Canonicalize leave an empty
// path, AddBackslash does nothing. While the result fits, the CPath method runs unchanged; only
// a result that would not fit is built here instead. The methods are not virtual, so this only
// applies to calls made through a CLongPath, never through a CPath reference, pointer or copy.
//
// RelativePathTo is left to CPath. A long path fails it visibly, since it returns FALSE, and a
// faithful reimplementation of PathRelativePathTo is more than its two callers justify.
class CLongPath : public CPath
{
public:
    CLongPath() = default;
    CLongPath(LPCTSTR pszPath) : CPath(pszPath) {}
    CLongPath(const CPath& path) : CPath(path) {}

    // CPath::operator+= would reach CPath::Append
    CLongPath& operator+=(LPCTSTR pszMore) {
        Append(pszMore);
        return *this;
    }

    void AddBackslash();
    BOOL AddExtension(LPCTSTR pszExtension);
    BOOL Append(LPCTSTR pszMore);
    void Canonicalize();
    void Combine(LPCTSTR pszDir, LPCTSTR pszFile);
    BOOL RenameExtension(LPCTSTR pszExtension);
};

namespace PathUtils
{
    CString BaseName(LPCTSTR path);
    CString DirName(LPCTSTR path);
    CString FileName(LPCTSTR path);
    CString FileExt(LPCTSTR path);
    CString StripExtensionAndRarVolumeSuffix(LPCTSTR path);
    CString GetModulePath(HMODULE hModule);
    CString GetAfxModulePath(bool bWithModuleName = false);
    CString GetProgramPath(bool bWithExeName = false);
    CString CombinePaths(LPCTSTR dir, LPCTSTR path);
    CString FilterInvalidCharsFromFileName(LPCTSTR fn, TCHAR replacementChar = _T('_'));
    CString Unquote(LPCTSTR path);
    CString StripPathOrUrl(LPCTSTR path);
    bool IsInDir(LPCTSTR path, LPCTSTR dir);
    bool IsStrictlyInDir(LPCTSTR path, LPCTSTR dir);
    CString ToRelative(LPCTSTR dir, const LPCTSTR path, bool* pbRelative = nullptr);
    bool IsRelative(LPCTSTR path);
    bool Exists(LPCTSTR path);
    bool IsFile(LPCTSTR path);
    bool IsDir(LPCTSTR path);
    bool IsLinkFile(LPCTSTR path);
    bool CreateDirRecursive(LPCTSTR path);
    CString ResolveLinkFile(LPCTSTR path);
    void RecurseAddDir(LPCTSTR path, CAtlList<CString>& sl);
    void ParseDirs(CAtlList<CString>& pathsList);

    bool IsURL(CString& fn);
    bool IsFullFilePath(CString& fn);
}
