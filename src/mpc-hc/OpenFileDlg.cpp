/*
 * (C) 2003-2006 Gabest
 * (C) 2006-2015, 2017 see Authors.txt
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
#include <shlobj.h>
#include <dlgs.h>
#include "OpenFileDlg.h"

#define __DUMMY__ _T("*.*")

namespace FileDialogUtils
{
    static void AppendPath(IShellItem* pItem, CAtlList<CString>& paths)
    {
        CComHeapPtr<WCHAR> path;
        if (SUCCEEDED(pItem->GetDisplayName(SIGDN_FILESYSPATH, &path))) {
            paths.AddTail(CString(path));
        }
    }

    // Fallback for the old style dialog. The buffer holds a sequence of null terminated
    // strings: either a single full path, or the folder followed by one bare file name
    // per selected file. Note that the MSDN example for CFileDialog gets the first case
    // wrong, it reports the path as the folder and then finds no files at all.
    static void ParseOFNBuffer(const OPENFILENAME& ofn, CAtlList<CString>& paths)
    {
        const TCHAR* p = ofn.lpstrFile;
        if (!p) {
            return;
        }

        size_t remaining = ofn.nMaxFile;
        size_t len = _tcsnlen(p, remaining);
        if (len == 0 || len >= remaining) {
            return;
        }

        CString folder(p, (int)len);
        p += len + 1;
        remaining -= len + 1;

        len = _tcsnlen(p, remaining);
        if (len == 0) {
            // only one string, so it is a full path rather than a folder
            paths.AddTail(folder);
            return;
        }

        if (folder[folder.GetLength() - 1] != _T('\\')) {
            folder += _T('\\');
        }
        while (len > 0 && len < remaining) {
            paths.AddTail(folder + CString(p, (int)len));
            p += len + 1;
            remaining -= len + 1;
            len = _tcsnlen(p, remaining);
        }
    }

    bool GetSelectedPaths(CFileDialog& fd, CAtlList<CString>& paths)
    {
        // Read the result from the shell interface the dialog is built on. Unlike the
        // OPENFILENAME buffer and CFileDialog::GetNextPathName(), IShellItem has no
        // path length limit of its own.
        CComPtr<IFileOpenDialog> pOpenDlg;
        pOpenDlg.Attach(fd.GetIFileOpenDialog());
        if (pOpenDlg) {
            CComPtr<IShellItemArray> pItems;
            if (SUCCEEDED(pOpenDlg->GetResults(&pItems)) && pItems) {
                DWORD count = 0;
                if (SUCCEEDED(pItems->GetCount(&count))) {
                    for (DWORD i = 0; i < count; i++) {
                        CComPtr<IShellItem> pItem;
                        if (SUCCEEDED(pItems->GetItemAt(i, &pItem))) {
                            AppendPath(pItem, paths);
                        }
                    }
                }
            }
        }

        if (paths.IsEmpty()) {
            // old style dialog, or a selection with no file system path (COpenFileDlg
            // substitutes "*.*" when a directory was picked)
            ParseOFNBuffer(fd.GetOFN(), paths);
        }

        return !paths.IsEmpty();
    }

    CString GetSelectedPath(CFileDialog& fd)
    {
        CAtlList<CString> paths;
        return GetSelectedPaths(fd, paths) ? paths.GetHead() : CString();
    }
}

bool COpenFileDlg::m_fAllowDirSelection = false;
WNDPROC COpenFileDlg::m_wndProc = nullptr;


// COpenFileDlg

IMPLEMENT_DYNAMIC(COpenFileDlg, CFileDialog)
COpenFileDlg::COpenFileDlg(CAtlArray<CString>& mask, bool fAllowDirSelection, LPCTSTR lpszDefExt, LPCTSTR lpszFileName,
                           DWORD dwFlags, LPCTSTR lpszFilter, CWnd* pParentWnd)
    : CFileDialog(TRUE, lpszDefExt, lpszFileName, dwFlags | OFN_NOVALIDATE, lpszFilter, pParentWnd, 0)
    , m_mask(mask)
{
    m_defaultDir = lpszFileName;
    m_defaultDir.RemoveFileSpec();

    m_fAllowDirSelection = fAllowDirSelection;
    m_pOFN->lpstrInitialDir = m_defaultDir.FileExists() ? (LPCTSTR)m_defaultDir : nullptr;

    m_buff = DEBUG_NEW TCHAR[10000];
    m_buff[0] = 0;
    m_pOFN->lpstrFile = m_buff;
    m_pOFN->nMaxFile = 10000;
}

COpenFileDlg::~COpenFileDlg()
{
    delete [] m_buff;
}

BEGIN_MESSAGE_MAP(COpenFileDlg, CFileDialog)
    ON_WM_DESTROY()
END_MESSAGE_MAP()


// COpenFileDlg message handlers

LRESULT CALLBACK COpenFileDlg::WindowProcNew(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    if (message == WM_COMMAND && HIWORD(wParam) == BN_CLICKED && LOWORD(wParam) == IDOK
            && m_fAllowDirSelection) {
        CAutoVectorPtr<TCHAR> path;
        // MAX_PATH should be bigger for multiple selection, but we are only interested if it's zero length
        // note: allocating MAX_PATH only will cause a buffer overrun for too long strings, and will result in a silent app disappearing crash, 100% reproducible
        if (path.Allocate(MAX_PATH + 1) && ::GetDlgItemText(hwnd, cmb13, (TCHAR*)path, MAX_PATH) == 0) {
            ::SendMessage(hwnd, CDM_SETCONTROLTEXT, edt1, (LPARAM)__DUMMY__);
        }
    }

    return CallWindowProc(COpenFileDlg::m_wndProc, hwnd, message, wParam, lParam);
}

BOOL COpenFileDlg::OnInitDialog()
{
    CFileDialog::OnInitDialog();

    m_wndProc = (WNDPROC)SetWindowLongPtr(GetParent()->m_hWnd, GWLP_WNDPROC, (LONG_PTR)WindowProcNew);

    return TRUE;  // return TRUE unless you set the focus to a control
    // EXCEPTION: OCX Property Pages should return FALSE
}

void COpenFileDlg::OnDestroy()
{
    int i = GetPathName().Find(__DUMMY__);
    if (i >= 0) {
        m_pOFN->lpstrFile[i] = m_pOFN->lpstrFile[i + 1] = 0;
    }

    CFileDialog::OnDestroy();
}

BOOL COpenFileDlg::OnNotify(WPARAM wParam, LPARAM lParam, LRESULT* pResult)
{
    ASSERT(pResult != nullptr);

    OFNOTIFY* pNotify = (OFNOTIFY*)lParam;
    // allow message map to override
    if (__super::OnNotify(wParam, lParam, pResult)) {
        ASSERT(pNotify->hdr.code != CDN_INCLUDEITEM);
        return TRUE;
    }

    switch (pNotify->hdr.code) {
        case CDN_INCLUDEITEM:
            if (OnIncludeItem((OFNOTIFYEX*)lParam, pResult)) {
                return TRUE;
            }
            break;
    }

    return FALSE;   // not handled
}

BOOL COpenFileDlg::OnIncludeItem(OFNOTIFYEX* pOFNEx, LRESULT* pResult)
{
    CString fn;
    if (!SHGetPathFromIDList((PIDLIST_ABSOLUTE)pOFNEx->pidl, fn.GetBuffer(MAX_PATH))) {
        fn.ReleaseBuffer(0);
        IShellFolder* psf = (IShellFolder*)pOFNEx->psf;
        PCUITEMID_CHILD pidl = (PCUITEMID_CHILD)pOFNEx->pidl;
        STRRET s;
        CComHeapPtr<TCHAR> fnTmp;
        if (SUCCEEDED(psf->GetDisplayNameOf(pidl, SHGDN_NORMAL | SHGDN_FORPARSING, &s))
                && SUCCEEDED(StrRetToStr(&s, pidl, &fnTmp))) {
            fn = fnTmp;
        }
    } else {
        fn.ReleaseBuffer();
    }

    /*
        WIN32_FILE_ATTRIBUTE_DATA fad;
        if (GetFileAttributesEx(fn, GetFileExInfoStandard, &fad)
        && (fad.dwFileAttributes&FILE_ATTRIBUTE_DIRECTORY))
            return FALSE;
    */
    int i = fn.ReverseFind('.'), j = fn.ReverseFind('\\');
    if (i < 0 || i < j) {
        return FALSE;
    }

    CString mask = m_mask[pOFNEx->lpOFN->nFilterIndex - 1] + _T(";");
    CString ext = fn.Mid(i).MakeLower() + _T(";");

    *pResult = mask.Find(ext) >= 0 || mask.Find(_T("*.*")) >= 0;

    return TRUE;
}
