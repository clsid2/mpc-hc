/*
 * (C) 2003-2006 Gabest
 * (C) 2006-2013 see Authors.txt
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

#include <afxwin.h>
#include <atlpath.h>


namespace FileDialogUtils
{
    // Retrieves the path(s) the user selected, to be called after DoModal() returned IDOK.
    // Use this instead of CFileDialog::GetStartPosition()/GetNextPathName(), which cannot
    // return a path longer than _MAX_PATH. Returns false if nothing could be retrieved.
    bool GetSelectedPaths(CFileDialog& fd, CAtlList<CString>& paths);

    // Single selection variant, returns an empty string on failure.
    CString GetSelectedPath(CFileDialog& fd);
}


// COpenFileDlg

class COpenFileDlg : public CFileDialog
{
    DECLARE_DYNAMIC(COpenFileDlg)

private:
    TCHAR* m_buff;
    CAtlArray<CString>& m_mask;
    CPath m_defaultDir;

public:
    COpenFileDlg(CAtlArray<CString>& mask, bool fAllowDirSelection,
                 LPCTSTR lpszDefExt = nullptr,
                 LPCTSTR lpszFileName = nullptr,
                 DWORD dwFlags = OFN_HIDEREADONLY | OFN_OVERWRITEPROMPT,
                 LPCTSTR lpszFilter = nullptr,
                 CWnd* pParentWnd = nullptr);
    virtual ~COpenFileDlg();

    static bool m_fAllowDirSelection;
    static WNDPROC m_wndProc;
    static LRESULT CALLBACK WindowProcNew(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam);

    virtual BOOL OnInitDialog();

protected:
    DECLARE_MESSAGE_MAP()
    virtual BOOL OnNotify(WPARAM wParam, LPARAM lParam, LRESULT* pResult);
    virtual BOOL OnIncludeItem(OFNOTIFYEX* pOFNEx, LRESULT* pResult);

public:
    afx_msg void OnDestroy();
};
