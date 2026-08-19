/*
 * (C) 2003-2006 Gabest
 * (C) 2006-2014 see Authors.txt
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

#include <afxsock.h>
#include <atlcoll.h>
#include <atlpath.h>

#define UTF8(str)     UTF16To8(TToW(str))
#define UTF8Arg(str)  UrlEncode(UTF8(str))
#define CMD_SETPOS            "-1"
#define WEB_CMD_SETVOLUME     "-2"
#define CMD_SETPLAYLISTINDEX  "-3"


class CWebServerSocket;
class CWebClientSocket;
class CMainFrame;

class CWebServer
{
    CMainFrame* m_pMainFrame;
    int m_nPort;

    DWORD ThreadProc();
    static DWORD WINAPI StaticThreadProc(LPVOID lpParam);
    DWORD m_ThreadId;
    HANDLE m_hThread;

    CAutoPtrList<CWebClientSocket> m_clients;

    typedef bool (CWebClientSocket::*RequestHandler)(CStringA& hdr, CStringA& body, CStringA& mime);
    static CAtlStringMap<RequestHandler, CStringA> m_internalpages;
    static CAtlStringMap<UINT, CStringA> m_downloads;
    static CAtlStringMap<CStringA, CStringA> m_mimes;
    CPath m_webroot;

    CAtlStringMap<> m_cgi;
    bool CallCGI(CWebClientSocket* pClient, CStringA& hdr, CStringA& body, CStringA& mime);

public:
    CWebServer(CMainFrame* pMainFrame, int nPort = 13579);
    virtual ~CWebServer();

    static void Init();

    static void Deploy(CString dir);

    bool ToLocalPath(CString& path, CString& redir);
    bool LoadPage(UINT resid, CStringA& str, CString path = _T(""));

    void OnAccept(CWebServerSocket* pServer);
    void OnClose(const CWebClientSocket* pClient);
    void OnRequest(CWebClientSocket* pClient, CStringA& reshdr, CStringA& resbody);
};
