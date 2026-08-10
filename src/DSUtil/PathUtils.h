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
