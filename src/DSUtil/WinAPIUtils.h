/*
 * (C) 2011-2014, 2016 see Authors.txt
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

struct IDirect3D9;


bool SetPrivilege(LPCTSTR privilege, bool bEnable = true);

bool ExportRegistryKey(CStdioFile& file, HKEY hKeyRoot, CString keyName = _T(""));

UINT GetAdapter(IDirect3D9* pD3D, HWND hWnd);

void GetMessageFont(LOGFONT* lf);
void GetStatusFont(LOGFONT* lf);

bool IsFontInstalled(LPCTSTR lpszFont);

bool ExploreToFile(LPCTSTR path);

HRESULT FileDelete(CString file, HWND hWnd, bool recycle = true, bool noconfirm = false);

bool IsLeftMouseButtonDown();

class CoInitializeHelper
{
public:
    CoInitializeHelper();
    ~CoInitializeHelper();
};

class CClipboard
{
public:
    CClipboard(CWnd* pWnd = nullptr) {
        m_bOpened = ::OpenClipboard(pWnd->GetSafeHwnd());
    }
    ~CClipboard() {
        if(m_bOpened) {
            VERIFY(::CloseClipboard());
        }
    }

    BOOL SetText(const CString& text) const;

protected:
    BOOL m_bOpened;
};
