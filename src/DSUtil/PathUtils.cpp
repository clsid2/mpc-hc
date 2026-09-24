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
#include <regex>
#include <vector>
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

    CString StripExtensionAndRarVolumeSuffix(LPCTSTR path)
    {
        // base.mkv        -> base
        // base.part01.rar -> base (multi-volume rar)
        static const std::wregex re(_T("(\\.part\\d+\\.rar|\\.[^.\\\\/]+)$"),
                                    std::wregex::icase | std::wregex::optimize);
        std::wcmatch mc;
        if (std::regex_search(path, mc, re)) {
            return CString(path, (int)mc.position());
        }
        return path;
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
        CLongPath cp;
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

    // True only for a path below dir, never dir itself. Both must be canonical and in the same
    // form; they are compared as text, case-insensitively, with no length limit.
    bool IsStrictlyInDir(LPCTSTR path, LPCTSTR dir)
    {
        CString d(dir);
        d.TrimRight(_T('\\'));
        const CString p(path);
        const int len = d.GetLength();
        return len > 0 && p.GetLength() > len + 1 && p[len] == _T('\\') && p[len + 1] != _T('\\')
               && CompareStringOrdinal(p, len, d, len, TRUE) == CSTR_EQUAL;
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

namespace
{
    bool IsDriveLetter(TCHAR c)
    {
        return (c >= _T('A') && c <= _T('Z')) || (c >= _T('a') && c <= _T('z'));
    }

    // A name Windows may read as something else. Trailing dots and spaces get trimmed, so a name
    // made of nothing else could end up as "." or ".." or nothing, depending on which API sees
    // it. "?" and "*" can't be in a file name, but do mark device and NT namespace paths.
    bool IsAmbiguousName(const CString& name)
    {
        return name.SpanIncluding(_T(". ")) == name || name.FindOneOf(_T("?*")) >= 0;
    }

    // Finds the root of a path, which ".." must never climb above: a drive ("C:\"), a share
    // ("\\server\share"), either of those behind the long path prefix, or a lone backslash.
    // A relative path has an empty root. Returns where the first name after the root starts,
    // or -1 for anything that can't be resolved safely: a drive relative path, whose meaning
    // depends on a current directory, a device or volume GUID path, or a malformed share.
    int SplitRoot(const CString& path, CString& root)
    {
        root.Empty();
        const int len = path.GetLength();
        int server;

        if (path.Left(8).CompareNoCase(_T("\\\\?\\UNC\\")) == 0) {
            server = 8;
        } else if (path.Left(4) == _T("\\\\?\\")) {
            if (len >= 6 && IsDriveLetter(path[4]) && path[5] == _T(':') && (len == 6 || path[6] == _T('\\'))) {
                root = path.Left(6) + _T('\\');
                return 7;
            }
            return -1;
        } else if (path.Left(2) == _T("\\\\")) {
            server = 2;
        } else if (len >= 2 && path[1] == _T(':')) {
            if (IsDriveLetter(path[0]) && (len == 2 || path[2] == _T('\\'))) {
                root = path.Left(2) + _T('\\');
                return 3;
            }
            return -1;
        } else if (len >= 1 && path[0] == _T('\\')) {
            root = _T('\\');
            return 1;
        } else {
            return 0;
        }

        int share = path.Find(_T('\\'), server) + 1;
        if (share <= 0) {
            return -1;
        }
        int shareEnd = path.Find(_T('\\'), share);
        if (shareEnd < 0) {
            shareEnd = len;
        }
        CString serverName = path.Mid(server, share - 1 - server);
        CString shareName = path.Mid(share, shareEnd - share);
        if (IsAmbiguousName(serverName) || IsAmbiguousName(shareName)) {
            return -1;
        }
        root = path.Left(shareEnd);
        return shareEnd + 1;
    }

    // Resolves "." and ".." the way PathCanonicalize does, without its MAX_PATH limit, but
    // failing rather than guessing: the web server relies on this to keep a request inside its
    // root, so a result that is not fully resolved must never come out of here. "/" is taken as
    // a separator too, because the file APIs take it as one. Unlike PathCanonicalize this keeps
    // the long path prefix, which a path this long needs, and never climbs out of a share.
    bool CanonicalizeLongPath(CString path, CString& result)
    {
        path.Replace(_T('/'), _T('\\'));

        CString root;
        const int start = SplitRoot(path, root);
        if (start < 0) {
            return false;
        }

        std::vector<CString> names;
        const int len = path.GetLength();
        for (int pos = start; pos < len;) {
            int end = path.Find(_T('\\'), pos);
            if (end < 0) {
                end = len;
            }
            CString name = path.Mid(pos, end - pos);
            pos = end + 1;

            if (name.IsEmpty() || name == _T(".")) {
                continue;
            }
            if (name == _T("..")) {
                if (!names.empty()) {
                    names.pop_back();
                } else if (root.IsEmpty()) {
                    // a relative path climbing above where it starts has nothing to resolve against
                    return false;
                }
                // at the root, ".." stays there
                continue;
            }
            if (IsAmbiguousName(name)) {
                return false;
            }
            names.push_back(name);
        }

        CString ret = root;
        for (const auto& name : names) {
            if (!ret.IsEmpty() && ret[ret.GetLength() - 1] != _T('\\')) {
                ret += _T('\\');
            }
            ret += name;
        }
        if (ret.IsEmpty()) {
            return false;
        }
        if (len > 0 && path[len - 1] == _T('\\') && ret[ret.GetLength() - 1] != _T('\\')) {
            ret += _T('\\');
        }
        result = ret;
        return true;
    }

    // PathCombine without its MAX_PATH limit, canonicalizing the result as it does
    bool CombineLongPath(LPCTSTR pszDir, LPCTSTR pszFile, CString& result)
    {
        CString dir(pszDir), file(pszFile), path;

        if (file.IsEmpty()) {
            path = dir;
        } else if (dir.IsEmpty() || (file.GetLength() >= 2 && file[1] == _T(':')) || file.Left(2) == _T("\\\\")) {
            path = file;
        } else if (file[0] == _T('\\')) {
            // rooted without a drive, so it belongs on the root of the directory
            dir.Replace(_T('/'), _T('\\'));
            CString root;
            if (SplitRoot(dir, root) < 0 || root.IsEmpty()) {
                return false;
            }
            if (root[root.GetLength() - 1] == _T('\\')) {
                root.Truncate(root.GetLength() - 1);
            }
            path = root + file;
        } else {
            path = dir;
            if (path[path.GetLength() - 1] != _T('\\')) {
                path += _T('\\');
            }
            path += file;
        }

        return CanonicalizeLongPath(path, result);
    }
}

// Each method below first lets CPath try whenever its inputs fit in MAX_PATH, and keeps that
// result unless it failed and the result could have been too long, so any path that works with
// CPath gets exactly what CPath gives it.

void CLongPath::AddBackslash()
{
    // PathAddBackslash needs room for the backslash and the terminator
    if (m_strPath.GetLength() < MAX_PATH - 1) {
        __super::AddBackslash();
    } else if (m_strPath[m_strPath.GetLength() - 1] != _T('\\')) {
        m_strPath += _T('\\');
    }
}

BOOL CLongPath::AddExtension(LPCTSTR pszExtension)
{
    const CString path(m_strPath);
    const CString ext(pszExtension ? pszExtension : _T(".exe"));
    if (path.GetLength() < MAX_PATH) {
        BOOL ret = __super::AddExtension(pszExtension);
        if (ret || path.GetLength() + ext.GetLength() + 1 < MAX_PATH) {
            return ret;
        }
        m_strPath = path;
    }

    if (FindExtension() >= 0) {
        return FALSE;
    }
    m_strPath += ext;
    return TRUE;
}

BOOL CLongPath::Append(LPCTSTR pszMore)
{
    const CString path(m_strPath), more(pszMore);
    if (path.GetLength() < MAX_PATH && more.GetLength() < MAX_PATH) {
        BOOL ret = __super::Append(more);
        if (ret || path.GetLength() + more.GetLength() + 2 < MAX_PATH) {
            return ret;
        }
    }

    // PathAppend drops the leading backslashes of anything but a UNC path, then combines
    CString tail(more);
    if (tail.Left(2) != _T("\\\\")) {
        tail.TrimLeft(_T('\\'));
    }
    if (!CombineLongPath(path, tail, m_strPath)) {
        m_strPath.Empty();
        return FALSE;
    }
    return TRUE;
}

void CLongPath::Canonicalize()
{
    const CString path(m_strPath);
    if (path.GetLength() < MAX_PATH) {
        __super::Canonicalize();
        if (!m_strPath.IsEmpty() || path.GetLength() + 1 < MAX_PATH) {
            return;
        }
    }

    if (!CanonicalizeLongPath(path, m_strPath)) {
        m_strPath.Empty();
    }
}

void CLongPath::Combine(LPCTSTR pszDir, LPCTSTR pszFile)
{
    // copied first, as either may point into m_strPath, which CPath::Combine writes over
    const CString dir(pszDir), file(pszFile);
    if (dir.GetLength() < MAX_PATH && file.GetLength() < MAX_PATH) {
        __super::Combine(dir, file);
        if (!m_strPath.IsEmpty() || dir.GetLength() + file.GetLength() + 2 < MAX_PATH) {
            return;
        }
    }

    if (!CombineLongPath(dir, file, m_strPath)) {
        m_strPath.Empty();
    }
}

BOOL CLongPath::RenameExtension(LPCTSTR pszExtension)
{
    const CString path(m_strPath), ext(pszExtension);
    if (path.GetLength() < MAX_PATH) {
        BOOL ret = __super::RenameExtension(pszExtension);
        if (ret || path.GetLength() + ext.GetLength() + 1 < MAX_PATH) {
            return ret;
        }
        m_strPath = path;
    }

    int pos = FindExtension();
    if (pos >= 0) {
        m_strPath.Truncate(pos);
    }
    m_strPath += ext;
    return TRUE;
}
