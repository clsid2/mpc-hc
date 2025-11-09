/*
 * (C) 2013-2015, 2017 see Authors.txt
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
#include "PathUtils.h"
#include <memory>
#include "text.h"
#include "DSUtil.h"

namespace PathUtils
{
    CString BaseName(LPCTSTR path)
    {
        CPath cp(path);
        cp.RemoveBackslash();
        cp.StripPath();
        return cp;
    }

    CString DirName(LPCTSTR path)
    {
        CPath cp(path);
        cp.RemoveBackslash();
        cp.RemoveFileSpec();
        return cp;
    }

    CString FileName(LPCTSTR path)
    {
        CPath cp(path);
        cp.StripPath();
        cp.RemoveExtension();
        cp.RemoveBackslash();
        return cp;
    }

    CString FileExt(LPCTSTR path)
    {
        return CPath(path).GetExtension();
    }

    CString GetModulePath(HMODULE hModule)
    {
        CString ret;
        int pos, len = MAX_PATH - 1;
        for (;;) {
            pos = GetModuleFileName(hModule, ret.GetBuffer(len), len);
            if (pos == len) {
                // buffer was too small, enlarge it and try again
                len *= 2;
                ret.ReleaseBuffer(0);
                continue;
            }
            ret.ReleaseBuffer(pos);
            break;
        }
        ASSERT(!ret.IsEmpty());
        return ret;
    }

    CString GetAfxModulePath(bool bWithModuleName/* = false*/)
    {
        CString ret = GetModulePath(AfxGetInstanceHandle());
        if (!bWithModuleName) {
            ret = DirName(ret);
        }
        return ret;
    }

    CString GetProgramPath(bool bWithExeName/* = false*/)
    {
        CString ret = GetModulePath(nullptr);
        if (!bWithExeName) {
            ret = DirName(ret);
        }
        return ret;
    }

    CString CombinePaths(LPCTSTR dir, LPCTSTR path)
    {
        CPath cp;
        cp.Combine(dir, path);
        return cp;
    }

    CString FilterInvalidCharsFromFileName(LPCTSTR fn, TCHAR replacementChar /*= _T('_')*/)
    {
        CString ret = fn;
        int iLength = ret.GetLength();
        LPTSTR buff = ret.GetBuffer();

        for (int i = 0; i < iLength; i++) {
            switch (buff[i]) {
                case _T('<'):
                case _T('>'):
                case _T(':'):
                case _T('"'):
                case _T('/'):
                case _T('\\'):
                case _T('|'):
                case _T('?'):
                case _T('*'):
                case _T('\r'):
                case _T('\n'):
                case _T('\t'):
                    buff[i] = replacementChar;
                    break;
                default:
                    // Do nothing
                    break;
            }
        }

        ret.ReleaseBuffer();

        return ret;
    }

    CString Unquote(LPCTSTR path)
    {
        return CString(path).Trim(_T("\""));
    }

    CString StripPathOrUrl(LPCTSTR path)
    {
        // Replacement for CPath::StripPath which works fine also for URLs
        CString p = path;
        bool isURL = p.Find(_T("://")) > 1;
        p.Replace('\\', '/');
        p.TrimRight('/');
        p = p.Mid(p.ReverseFind('/') + 1);
        if (p.IsEmpty()) {
            return CString(path);
        } else if (isURL) {
            return UrlDecodeWithUTF8(p);
        }
        return p;
    }

    bool IsInDir(LPCTSTR path, LPCTSTR dir)
    {
        return !!CPath(path).IsPrefix(dir);
    }

    CString ToRelative(LPCTSTR dir, const LPCTSTR path, bool* pbRelative/* = nullptr*/)
    {
        CPath cp;
        BOOL rel = cp.RelativePathTo(dir, FILE_ATTRIBUTE_DIRECTORY, path, 0);
        if (pbRelative) {
            *pbRelative = !!rel;
        }
        return cp;
    }

    bool IsRelative(LPCTSTR path)
    {
        return !!CPath(path).IsRelative();
    }

    bool Exists(LPCTSTR path)
    {
        return GetFileAttributes(path) != INVALID_FILE_ATTRIBUTES;
    }

    bool IsFile(LPCTSTR path)
    {
        DWORD attr = GetFileAttributes(path);
        return (attr != INVALID_FILE_ATTRIBUTES) && !(attr & FILE_ATTRIBUTE_DIRECTORY);
    }

    bool IsDir(LPCTSTR path)
    {
        DWORD attr = GetFileAttributes(path);
        return (attr != INVALID_FILE_ATTRIBUTES) && (attr & FILE_ATTRIBUTE_DIRECTORY);
    }

    bool IsLinkFile(LPCTSTR path)
    {
        return !FileExt(path).CompareNoCase(_T(".lnk"));
    }

    bool CreateDirRecursive(LPCTSTR path)
    {
        bool ret = IsDir(path) || CreateDirectory(path, nullptr);
        if (!ret) {
            ret = CreateDirRecursive(DirName(path)) && CreateDirectory(path, nullptr);
        }
        return ret;
    }

    CString ResolveLinkFile(LPCTSTR path)
    {
        TCHAR buff[MAX_PATH];
        CComPtr<IShellLink> pSL;
        pSL.CoCreateInstance(CLSID_ShellLink);
        CComQIPtr<IPersistFile> pPF = pSL;

        if (pSL && pPF
                && SUCCEEDED(pPF->Load(path, STGM_READ))
                && SUCCEEDED(pSL->Resolve(nullptr, SLR_ANY_MATCH | SLR_NO_UI))
                && SUCCEEDED(pSL->GetPath(buff, _countof(buff), nullptr, 0))) {
            return buff;
        }

        return _T("");
    }

    void RecurseAddDir(LPCTSTR path, CAtlList<CString>& paths)
    {
        CFileFind finder;

        BOOL bFound = finder.FindFile(PathUtils::CombinePaths(path, _T("*.*")));
        while (bFound) {
            bFound = finder.FindNextFile();

            if (!finder.IsDots() && finder.IsDirectory()) {
                CString folderPath = finder.GetFilePath();
                ExtendMaxPathLengthIfNeeded(folderPath);
                paths.AddTail(folderPath);
                RecurseAddDir(folderPath, paths);
            }
        }
    }

    void ParseDirs(CAtlList<CString>& paths)
    {
        POSITION pos = paths.GetHeadPosition();
        while (pos) {
            POSITION prevPos = pos;
            CString fn = paths.GetNext(pos);
            // Try to follow link files that point to a directory
            if (IsLinkFile(fn)) {
                fn = ResolveLinkFile(fn);
            }

            if (IsDir(fn)) {
                CAtlList<CString> subDirs;
                RecurseAddDir(fn, subDirs);
                // Add the subdirectories just after their parent
                // so that the tree is not parsed multiple times
                while (!subDirs.IsEmpty()) {
                    paths.InsertAfter(prevPos, subDirs.RemoveTail());
                }
            }
        }
    }

    bool IsURL(CString& fn)
    {
        return (fn.Find(_T("://")) > 1) && (fn.Left(5) != L"file:");
    }

    bool IsFullFilePath(CString& fn)
    {
        return (fn.Find(_T(":")) > 0) && !IsURL(fn) || (fn.Find(_T("\\\\")) == 0);
    }
}
