/*
 * (C) 2003-2006 Gabest
 * (C) 2006-2017 see Authors.txt
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
#include "mplayerc.h"
#include "resource.h"
#include "MainFrm.h"
#include <atlbase.h>
#include "zlib/zlib.h"
#include "WebServerSocket.h"
#include "WebClientSocket.h"
#include "WebServer.h"
#include "VersionInfo.h"
#include "PathUtils.h"


CAtlStringMap<CWebServer::RequestHandler, CStringA> CWebServer::m_internalpages;
CAtlStringMap<UINT, CStringA> CWebServer::m_downloads;
CAtlStringMap<CStringA, CStringA> CWebServer::m_mimes;

CWebServer::CWebServer(CMainFrame* pMainFrame, int nPort)
    : m_pMainFrame(pMainFrame)
    , m_nPort(nPort)
{
    m_webroot = CPath(PathUtils::GetProgramPath());
    const CAppSettings& s = AfxGetAppSettings();

    CString WebRoot = s.strWebRoot;
    WebRoot.Replace('/', '\\');
    WebRoot.Trim();
    CPath p(WebRoot);
    if (WebRoot.Find(_T(":\\")) < 0 && WebRoot.Find(_T("\\\\")) < 0) {
        m_webroot.Append(WebRoot);
    } else {
        m_webroot = p;
    }
    m_webroot.Canonicalize();
    m_webroot.MakePretty();
    if (!m_webroot.IsDirectory()) {
        m_webroot = CPath();
    }

    CAtlList<CString> sl;
    Explode(s.strWebServerCGI, sl, ';');
    POSITION pos = sl.GetHeadPosition();
    while (pos) {
        CAtlList<CString> sl2;
        CString ext = Explode(sl.GetNext(pos), sl2, '=', 2);
        if (sl2.GetCount() < 2) {
            continue;
        }
        m_cgi[ext] = sl2.GetTail();
    }

    m_ThreadId = 0;
    m_hThread = ::CreateThread(nullptr, 0, StaticThreadProc, (LPVOID)this, 0, &m_ThreadId);
}

CWebServer::~CWebServer()
{
    if (m_hThread != nullptr) {
        PostThreadMessage(m_ThreadId, WM_QUIT, (WPARAM)0, (LPARAM)0);
        if (WaitForSingleObject(m_hThread, 10000) == WAIT_TIMEOUT) {
            TerminateThread(m_hThread, 0xDEAD);
        }
        EXECUTE_ASSERT(CloseHandle(m_hThread));
    }
}

void CWebServer::Init()
{
    m_internalpages["/"] = &CWebClientSocket::OnIndex;
    m_internalpages["/404.html"] = &CWebClientSocket::OnError404;
    m_internalpages["/browse.json"] = &CWebClientSocket::OnBrowseJSON;
    m_internalpages["/browser.html"] = &CWebClientSocket::OnBrowser;
    m_internalpages["/command.html"] = &CWebClientSocket::OnCommand;
    m_internalpages["/commands.json"] = &CWebClientSocket::OnCommandsJSON;
    m_internalpages["/controls.html"] = &CWebClientSocket::OnControls;
    m_internalpages["/index.html"] = &CWebClientSocket::OnIndex;
    m_internalpages["/info.html"] = &CWebClientSocket::OnInfo;
    m_internalpages["/player.html"] = &CWebClientSocket::OnPlayer;
    m_internalpages["/playlist.json"] = &CWebClientSocket::OnPlaylistJSON;
    m_internalpages["/img/buttons.svg"] = &CWebClientSocket::OnToolbarImage;
    m_internalpages["/remote.html"] = &CWebClientSocket::OnRemote;
    m_internalpages["/snapshot.jpg"] = &CWebClientSocket::OnSnapshotJpeg;
    m_internalpages["/status.html"] = &CWebClientSocket::OnStatus;
    m_internalpages["/status.json"] = &CWebClientSocket::OnStatusJSON;
    m_internalpages["/variables.html"] = &CWebClientSocket::OnVariables;
    m_internalpages["/viewres.html"] = &CWebClientSocket::OnViewRes;
    m_internalpages["/dvb/channels.json"] = &CWebClientSocket::OnDVBChannels;
    m_internalpages["/dvb/setchannel"] = &CWebClientSocket::OnDVBSetChannel;

    m_downloads["/default.css"] = IDF_DEFAULT_CSS;
    m_downloads["/favicon.ico"] = IDF_FAVICON;
    m_downloads["/img/1pix.png"] = IDF_1PIX_PNG;
    m_downloads["/img/bottomside.png"] = IDF_BOTTOMSIDE_PNG;
    m_downloads["/img/controlback.png"] = IDF_CONTROLBACK_PNG;
    m_downloads["/img/controlbuttondecrate.png"] = IDF_CONTROLBUTTONDECRATE_PNG;
    m_downloads["/img/controlbuttonincrate.png"] = IDF_CONTROLBUTTONINCRATE_PNG;
    m_downloads["/img/controlbuttonpause.png"] = IDF_CONTROLBUTTONPAUSE_PNG;
    m_downloads["/img/controlbuttonplay.png"] = IDF_CONTROLBUTTONPLAY_PNG;
    m_downloads["/img/controlbuttonskipback.png"] = IDF_CONTROLBUTTONSKIPBACK_PNG;
    m_downloads["/img/controlbuttonskipforward.png"] = IDF_CONTROLBUTTONSKIPFORWARD_PNG;
    m_downloads["/img/controlbuttonstep.png"] = IDF_CONTROLBUTTONSTEP_PNG;
    m_downloads["/img/controlbuttonstop.png"] = IDF_CONTROLBUTTONSTOP_PNG;
    m_downloads["/img/controlvolumebar.png"] = IDF_CONTROLVOLUMEBAR_PNG;
    m_downloads["/img/controlvolumegrip.png"] = IDF_CONTROLVOLUMEGRIP_PNG;
    m_downloads["/img/controlvolumeoff.png"] = IDF_CONTROLVOLUMEOFF_PNG;
    m_downloads["/img/controlvolumeon.png"] = IDF_CONTROLVOLUMEON_PNG;
    m_downloads["/img/headerback.png"] = IDF_HEADERBACK_PNG;
    m_downloads["/img/headerclose.png"] = IDF_HEADERCLOSE_PNG;
    m_downloads["/img/headericon.png"] = IDF_HEADERICON_PNG;
    m_downloads["/img/leftbottomside.png"] = IDF_LEFTBOTTOMSIDE_PNG;
    m_downloads["/img/leftside.png"] = IDF_LEFTSIDE_PNG;
    m_downloads["/img/rightbottomside.png"] = IDF_RIGHTBOTTOMSIDE_PNG;
    m_downloads["/img/rightside.png"] = IDF_RIGHTSIDE_PNG;
    m_downloads["/img/seekbargrip.png"] = IDF_SEEKBARGRIP_PNG;
    m_downloads["/img/seekbarleft.png"] = IDF_SEEKBARLEFT_PNG;
    m_downloads["/img/seekbarmid.png"] = IDF_SEEKBARMID_PNG;
    m_downloads["/img/seekbarright.png"] = IDF_SEEKBARRIGHT_PNG;
    m_downloads["/img/sliderback.png"] = IDF_SLIDERBACK_PNG;
    m_downloads["/img/slidergrip.png"] = IDF_SLIDERGRIP_PNG;
    m_downloads["/img/vbg.png"] = IDF_VBR_PNG;
    m_downloads["/img/vbs.png"] = IDF_VBS_PNG;
    m_downloads["/javascript.js"] = IDF_JAVASCRIPT;

#if 0
    CRegKey key;
    CString str(_T("MIME\\Database\\Content Type"));
    if (ERROR_SUCCESS == key.Open(HKEY_CLASSES_ROOT, str, KEY_READ)) {
        TCHAR buff[256];
        DWORD len = _countof(buff);
        for (int i = 0; ERROR_SUCCESS == key.EnumKey(i, buff, &len); i++, len = _countof(buff)) {
            CRegKey mime;
            TCHAR ext[64];
            ULONG len2 = _countof(ext);
            if (ERROR_SUCCESS == mime.Open(HKEY_CLASSES_ROOT, str + _T("\\") + buff, KEY_READ)
                    && ERROR_SUCCESS == mime.QueryStringValue(_T("Extension"), ext, &len2)) {
                m_mimes[CStringA(ext).MakeLower()] = CStringA(buff).MakeLower();
            }
        }
    }
#endif

    m_mimes[".bmp"] = "image/bmp";
    m_mimes[".css"] = "text/css";
    m_mimes[".gif"] = "image/gif";
    m_mimes[".htm"] = "text/html";
    m_mimes[".html"] = "text/html";
    m_mimes[".jpeg"] = "image/jpeg";
    m_mimes[".jpg"] = "image/jpeg";
    m_mimes[".js"] = "text/javascript";
    m_mimes[".json"] = "text/plain";
    m_mimes[".png"] = "image/png";
    m_mimes[".svg"] = "image/svg+xml";
    m_mimes[".txt"] = "text/plain";
    m_mimes[".ico"] = "image/x-icon";
}

DWORD WINAPI CWebServer::StaticThreadProc(LPVOID lpParam)
{
    SetThreadName(DWORD(-1), "WebServer Thread");
    return ((CWebServer*)lpParam)->ThreadProc();
}

DWORD CWebServer::ThreadProc()
{
    if (!AfxSocketInit(nullptr)) {
        return DWORD_ERROR;
    }

    CWebServerSocket s(this, m_nPort);

    MSG msg;
    while ((int)GetMessage(&msg, nullptr, 0, 0) > 0) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    return 0;
}

static void PutFileContents(LPCTSTR fn, const CStringA& data)
{
    FILE* f = nullptr;
    if (!_tfopen_s(&f, fn, _T("wb"))) {
        fwrite((LPCSTR)data, 1, data.GetLength(), f);
        fclose(f);
    }
}

void CWebServer::Deploy(CString dir)
{
    CStringA data;
    if (LoadResource(IDR_HTML_INDEX, data, RT_HTML)) {
        PutFileContents(dir + _T("index.html"), data);
    }
    if (LoadResource(IDR_HTML_INFO, data, RT_HTML)) {
        PutFileContents(dir + _T("info.html"), data);
    }
    if (LoadResource(IDR_HTML_BROWSER, data, RT_HTML)) {
        PutFileContents(dir + _T("browser.html"), data);
    }
    if (LoadResource(IDR_HTML_CONTROLS, data, RT_HTML)) {
        PutFileContents(dir + _T("controls.html"), data);
    }
    if (LoadResource(IDR_HTML_VARIABLES, data, RT_HTML)) {
        PutFileContents(dir + _T("variables.html"), data);
    }
    if (LoadResource(IDR_HTML_404, data, RT_HTML)) {
        PutFileContents(dir + _T("404.html"), data);
    }
    if (LoadResource(IDR_HTML_PLAYER, data, RT_HTML)) {
        PutFileContents(dir + _T("player.html"), data);
    }
    if (LoadResource(IDR_HTML_REMOTE, data, RT_HTML)) {
        PutFileContents(dir + _T("remote.html"), data);
    }

    // Create the needed folder
    CreateDirectory(dir + _T("img"), nullptr);

    POSITION pos = m_downloads.GetStartPosition();
    while (pos) {
        CStringA fn;
        UINT id;
        m_downloads.GetNextAssoc(pos, fn, id);
        if (LoadResource(id, data, _T("FILE")) || LoadResource(id, data, _T("PNG"))) {
            PutFileContents(dir + AToT(fn), data);
        }
    }
}

bool CWebServer::ToLocalPath(CString& path, CString& redir)
{
    if (!path.IsEmpty() && m_webroot.IsDirectory()) {
        CString str = path;
        str.Replace('/', '\\');
        str.TrimLeft('\\');

        CPath p;
        p.Combine(m_webroot, str);
        p.Canonicalize();

        if (p.IsDirectory()) {
            CAtlList<CString> sl;
            Explode(AfxGetAppSettings().strWebDefIndex, sl, ';');
            POSITION pos = sl.GetHeadPosition();
            while (pos) {
                str = sl.GetNext(pos);
                CPath p2 = p;
                p2.Append(str);
                if (p2.FileExists()) {
                    p = p2;
                    redir = path;
                    if (redir.GetAt(redir.GetLength() - 1) != '/') {
                        redir += _T('/');
                    }
                    redir += str;
                    break;
                }
            }
        }

        if (_tcslen(p) > _tcslen(m_webroot) && p.FileExists()) {
            path = (LPCTSTR)p;
            return true;
        }
    }

    return false;
}

bool CWebServer::LoadPage(UINT resid, CStringA& str, CString path)
{
    CString redir;
    if (ToLocalPath(path, redir)) {
        FILE* f = nullptr;
        if (!_tfopen_s(&f, path, _T("rb"))) {
            fseek(f, 0, 2);
            char* buff = str.GetBufferSetLength(ftell(f));
            fseek(f, 0, 0);
            int len = (int)fread(buff, 1, str.GetLength(), f);
            fclose(f);
            return len == str.GetLength();
        }
    }

    return LoadResource(resid, str, RT_HTML);
}

void CWebServer::OnAccept(CWebServerSocket* pServer)
{
    CAutoPtr<CWebClientSocket> p(DEBUG_NEW CWebClientSocket(this, m_pMainFrame));
    if (pServer->Accept(*p)) {
        CString name;
        UINT port;
        if (AfxGetAppSettings().fWebServerLocalhostOnly && p->GetPeerName(name, port) && name != _T("127.0.0.1")) {
            p->Close();
            return;
        }

        m_clients.AddTail(p);
    }
}

void CWebServer::OnClose(const CWebClientSocket* pClient)
{
    POSITION pos = m_clients.GetHeadPosition();
    while (pos) {
        POSITION cur = pos;
        if (m_clients.GetNext(pos) == pClient) {
            m_clients.RemoveAt(cur);
            break;
        }
    }
}

void CWebServer::OnRequest(CWebClientSocket* pClient, CStringA& hdr, CStringA& body)
{
    const CAppSettings& s = AfxGetAppSettings();

    CPath p(AToT(pClient->m_path));
    CStringA ext = p.GetExtension().MakeLower();
    CStringA mime;
    if (ext.IsEmpty()) {
        mime = "text/html";
    } else {
        if (!m_mimes.Lookup(ext, mime)) {
            mime = "none";
        }
    }

    hdr = "HTTP/1.0 200 OK\r\n";

    bool fHandled = false, fCGI = false;

    if (m_webroot.IsDirectory()) {
        CStringA tmphdr;
        fHandled = fCGI = CallCGI(pClient, tmphdr, body, mime);

        if (fHandled) {
            tmphdr.Replace("\r\n", "\n");
            CAtlList<CStringA> hdrlines;
            ExplodeMin(tmphdr, hdrlines, '\n');
            POSITION pos = hdrlines.GetHeadPosition();
            while (pos) {
                POSITION cur = pos;
                CAtlList<CStringA> sl;
                CStringA key = Explode(hdrlines.GetNext(pos), sl, ':', 2);
                if (sl.GetCount() < 2) {
                    continue;
                }
                key.Trim().MakeLower();
                if (key == "content-type") {
                    mime = sl.GetTail().Trim();
                    hdrlines.RemoveAt(cur);
                } else if (key == "content-length") {
                    hdrlines.RemoveAt(cur);
                }
            }
            tmphdr = Implode(hdrlines, "\r\n");
            hdr += tmphdr + "\r\n";
        }
    }

    RequestHandler rh = nullptr;
    if (!fHandled && m_internalpages.Lookup(pClient->m_path, rh) && (pClient->*rh)(hdr, body, mime)) {
        if (mime.IsEmpty()) {
            mime = "text/html";
        }

        CString redir;
        if (pClient->m_get.Lookup("redir", redir)
                || pClient->m_post.Lookup("redir", redir)) {
            if (redir.IsEmpty()) {
                redir = '/';
            }

            hdr =
                "HTTP/1.0 302 Found\r\n"
                "Location: " + CStringA(redir) + "\r\n";
            return;
        }

        fHandled = true;
    }

    if (!fHandled && m_webroot.IsDirectory()) {
        fHandled = LoadPage(0, body, UTF8To16(pClient->m_path));
    }

    UINT resid;
    if (!fHandled && m_downloads.Lookup(pClient->m_path, resid)
            && (LoadResource(resid, body, _T("FILE")) || LoadResource(resid, body, _T("PNG")))) {
        if (mime.IsEmpty()) {
            mime = "application/octet-stream";
        }
        fHandled = true;
    }

    if (!fHandled) {
        hdr = mime == "text/html"
              ? "HTTP/1.0 301 Moved Permanently\r\n" "Location: /404.html\r\n"
              : "HTTP/1.0 404 Not Found\r\n";
        return;
    }

    /* Don't cache html, js and css files */
    if ((mime == "text/html" || mime == "text/javascript" || mime == "text/css") && !fCGI) {
        if (mime == "text/html") {
            hdr +=
                "Expires: Thu, 19 Nov 1981 08:52:00 GMT\r\n"
                "Cache-Control: no-store, no-cache, must-revalidate, post-check=0, pre-check=0\r\n"
                "Pragma: no-cache\r\n";

            CStringA debug;
            if (s.fWebServerPrintDebugInfo) {
                debug += "<br><hr><pre>\r\n";

                CStringA key;
                POSITION pos;

                {
                    CStringA value;

                    pos = pClient->m_hdrlines.GetStartPosition();
                    while (pos) {
                        pClient->m_hdrlines.GetNextAssoc(pos, key, value);
                        debug += "HEADER[" + key + "] = " + value + "\r\n";
                    }
                }
                debug += "cmd: " + pClient->m_cmd + "\r\n";
                debug += "path: " + pClient->m_path + "\r\n";
                debug += "ver: " + pClient->m_ver + "\r\n";

                {
                    CString value;

                    pos = pClient->m_get.GetStartPosition();
                    while (pos) {
                        pClient->m_get.GetNextAssoc(pos, key, value);
                        debug += "GET[" + HtmlSpecialChars(key) + "] = " + HtmlSpecialChars(UTF8(value)) + "\r\n";
                    }
                    pos = pClient->m_post.GetStartPosition();
                    while (pos) {
                        pClient->m_post.GetNextAssoc(pos, key, value);
                        debug += "POST[" + HtmlSpecialChars(key) + "] = " + HtmlSpecialChars(UTF8(value)) + "\r\n";
                    }
                    pos = pClient->m_cookie.GetStartPosition();
                    while (pos) {
                        pClient->m_cookie.GetNextAssoc(pos, key, value);
                        debug += "COOKIE[" + HtmlSpecialChars(key) + "] = " + HtmlSpecialChars(UTF8(value)) + "\r\n";
                    }
                    pos = pClient->m_request.GetStartPosition();
                    while (pos) {
                        pClient->m_request.GetNextAssoc(pos, key, value);
                        debug += "REQUEST[" + HtmlSpecialChars(key) + "] = " + HtmlSpecialChars(UTF8(value)) + "\r\n";
                    }
                }
                debug += "</pre>";
            }
            body.Replace("[debug]", debug);
        }

        body.Replace("[browserpath]", "/browser.html");
        body.Replace("[commandpath]", "/command.html");
        body.Replace("[controlspath]", "/controls.html");
        body.Replace("[indexpath]", "/index.html");
        body.Replace("[path]", pClient->m_path);
        body.Replace("[setposcommand]", CMD_SETPOS);
        body.Replace("[setvolumecommand]", WEB_CMD_SETVOLUME);
        body.Replace("[wmcname]", "wm_command");
        // TODO: add more general tags to replace
    }

    // gzip
    if (s.fWebServerUseCompression && !body.IsEmpty()
            && hdr.Find("Content-Encoding:") < 0
            && ext != ".png" && ext != ".jpg" && ext != ".jpeg" && ext != ".gif" && ext != ".ico")
        do {
            CStringA accept_encoding;
            pClient->m_hdrlines.Lookup("accept-encoding", accept_encoding);
            accept_encoding.MakeLower();
            CAtlList<CStringA> sl;
            ExplodeMin(accept_encoding, sl, ',');
            if (!sl.Find("gzip")) {
                break;
            }

            // Allocate deflate state
            z_stream strm;

            strm.zalloc = Z_NULL;
            strm.zfree = Z_NULL;
            strm.opaque = Z_NULL;
            int ret = deflateInit2(&strm, Z_DEFAULT_COMPRESSION, Z_DEFLATED, 15 + 16, 8, Z_DEFAULT_STRATEGY);
            if (ret != Z_OK) {
                ASSERT(0);
                break;
            }

            int gzippedBuffLen = body.GetLength();
            BYTE* gzippedBuff = DEBUG_NEW BYTE[gzippedBuffLen];

            // Compress
            strm.avail_in = body.GetLength();
            strm.next_in = (Bytef*)(LPCSTR)body;

            strm.avail_out = gzippedBuffLen;
            strm.next_out = gzippedBuff;

            ret = deflate(&strm, Z_FINISH);
            if (ret != Z_STREAM_END || strm.avail_in != 0) {
                ASSERT(0);
                deflateEnd(&strm);
                delete [] gzippedBuff;
                break;
            }
            gzippedBuffLen -= strm.avail_out;
            memcpy(body.GetBufferSetLength(gzippedBuffLen), gzippedBuff, gzippedBuffLen);

            // Clean up
            deflateEnd(&strm);
            delete [] gzippedBuff;

            hdr += "Content-Encoding: gzip\r\n";
        } while (0);

    CStringA content;
    content.Format(
        "Content-Type: %s\r\n"
        "Content-Length: %d\r\n",
        mime.GetString(), body.GetLength());
    hdr += content;
}

static DWORD WINAPI KillCGI(LPVOID lParam)
{
    HANDLE hProcess = (HANDLE)lParam;
    if (WaitForSingleObject(hProcess, 30000) == WAIT_TIMEOUT) {
        TerminateProcess(hProcess, 0);
    }
    return 0;
}

bool CWebServer::CallCGI(CWebClientSocket* pClient, CStringA& hdr, CStringA& body, CStringA& mime)
{
    CString path = pClient->m_path, redir = path;
    if (!ToLocalPath(path, redir)) {
        return false;
    }
    CString ext = CPath(path).GetExtension().MakeLower();
    CPath dir(path);
    dir.RemoveFileSpec();

    CString cgi;
    if (!m_cgi.Lookup(ext, cgi) || !CPath(cgi).FileExists()) {
        return false;
    }

    HANDLE hProcess = GetCurrentProcess();
    HANDLE hChildStdinRd, hChildStdinWr, hChildStdinWrDup = nullptr;
    HANDLE hChildStdoutRd, hChildStdoutWr, hChildStdoutRdDup = nullptr;

    SECURITY_ATTRIBUTES saAttr;
    ZeroMemory(&saAttr, sizeof(saAttr));
    saAttr.nLength = sizeof(saAttr);
    saAttr.bInheritHandle = TRUE;

    if (CreatePipe(&hChildStdoutRd, &hChildStdoutWr, &saAttr, 0)) {
        BOOL fSuccess = DuplicateHandle(hProcess, hChildStdoutRd, hProcess, &hChildStdoutRdDup, 0, FALSE, DUPLICATE_SAME_ACCESS);
        UNREFERENCED_PARAMETER(fSuccess);
        CloseHandle(hChildStdoutRd);
    }

    if (CreatePipe(&hChildStdinRd, &hChildStdinWr, &saAttr, 0)) {
        BOOL fSuccess = DuplicateHandle(hProcess, hChildStdinWr, hProcess, &hChildStdinWrDup, 0, FALSE, DUPLICATE_SAME_ACCESS);
        UNREFERENCED_PARAMETER(fSuccess);
        CloseHandle(hChildStdinWr);
    }

    STARTUPINFO siStartInfo;
    ZeroMemory(&siStartInfo, sizeof(siStartInfo));
    siStartInfo.cb = sizeof(siStartInfo);
    siStartInfo.hStdError = hChildStdoutWr;
    siStartInfo.hStdOutput = hChildStdoutWr;
    siStartInfo.hStdInput = hChildStdinRd;
    siStartInfo.dwFlags |= STARTF_USESTDHANDLES | STARTF_USESHOWWINDOW;
    siStartInfo.wShowWindow = SW_HIDE;

    PROCESS_INFORMATION piProcInfo;
    ZeroMemory(&piProcInfo, sizeof(piProcInfo));

    CStringA envstr;

    LPVOID lpvEnv = GetEnvironmentStrings();
    if (lpvEnv) {
        CAtlList<CString> env;
        for (LPTSTR lpszVariable = (LPTSTR)lpvEnv; *lpszVariable; lpszVariable += _tcslen(lpszVariable) + 1) {
            if (lpszVariable != (LPTSTR)lpvEnv) {
                env.AddTail(lpszVariable);
            }
        }

        env.AddTail(_T("GATEWAY_INTERFACE=CGI/1.1"));
        env.AddTail(_T("SERVER_SOFTWARE=MPC-HC/") + VersionInfo::GetVersionString());
        env.AddTail(_T("SERVER_PROTOCOL=") + AToT(pClient->m_ver));
        env.AddTail(_T("REQUEST_METHOD=") + AToT(pClient->m_cmd));
        env.AddTail(_T("PATH_INFO=") + redir);
        env.AddTail(_T("PATH_TRANSLATED=") + path);
        env.AddTail(_T("SCRIPT_NAME=") + redir);
        env.AddTail(_T("QUERY_STRING=") + AToT(pClient->m_query));

        {
            CStringA str;
            if (pClient->m_hdrlines.Lookup("content-type", str)) {
                env.AddTail(_T("CONTENT_TYPE=") + AToT(str));
            }
            if (pClient->m_hdrlines.Lookup("content-length", str)) {
                env.AddTail(_T("CONTENT_LENGTH=") + AToT(str));
            }
        }

        POSITION pos = pClient->m_hdrlines.GetStartPosition();
        while (pos) {
            CString key = pClient->m_hdrlines.GetKeyAt(pos);
            CString value = pClient->m_hdrlines.GetNextValue(pos);
            key.Replace(_T("-"), _T("_"));
            key.MakeUpper();
            env.AddTail(_T("HTTP_") + key + _T("=") + value);
        }

        CString str, name;
        UINT port;

        if (pClient->GetPeerName(name, port)) {
            str.Format(_T("%u"), port);
            env.AddTail(_T("REMOTE_ADDR=") + name);
            env.AddTail(_T("REMOTE_HOST=") + name);
            env.AddTail(_T("REMOTE_PORT=") + str);
        }

        if (pClient->GetSockName(name, port)) {
            str.Format(_T("%u"), port);
            env.AddTail(_T("SERVER_NAME=") + name);
            env.AddTail(_T("SERVER_PORT=") + str);
        }

        env.AddTail(_T("\0"));

        str = Implode(env, '\0');
        envstr = CStringA(str, str.GetLength());

        FreeEnvironmentStrings((LPTSTR)lpvEnv);
    }

    TCHAR* cmdln = DEBUG_NEW TCHAR[32768];
    _sntprintf_s(cmdln, 32768, 32768, _T("\"%s\" \"%s\""), cgi.GetString(), path.GetString());

    if (hChildStdinRd && hChildStdoutWr)
        if (CreateProcess(
                    nullptr, cmdln, nullptr, nullptr, TRUE, 0,
                    envstr.GetLength() ? (LPVOID)(LPCSTR)envstr : nullptr,
                    dir, &siStartInfo, &piProcInfo)) {
            DWORD ThreadId;
            VERIFY(CreateThread(nullptr, 0, KillCGI, (LPVOID)piProcInfo.hProcess, 0, &ThreadId));

            static const int BUFFSIZE = 1024;
            DWORD dwRead, dwWritten = 0;

            int i = 0, len = pClient->m_data.GetLength();
            for (; i < len; i += dwWritten) {
                if (!WriteFile(hChildStdinWrDup, (LPCSTR)pClient->m_data + i, std::min(len - i, BUFFSIZE), &dwWritten, nullptr)) {
                    break;
                }
            }

            CloseHandle(hChildStdinWrDup);
            CloseHandle(hChildStdoutWr);

            body.Empty();

            CStringA buff;
            while (i == len && ReadFile(hChildStdoutRdDup, buff.GetBuffer(BUFFSIZE), BUFFSIZE, &dwRead, nullptr) && dwRead) {
                buff.ReleaseBufferSetLength(dwRead);
                body += buff;
            }

            int hdrend = body.Find("\r\n\r\n");
            if (hdrend >= 0) {
                hdr = body.Left(hdrend + 2);
                body = body.Mid(hdrend + 4);
            }

            CloseHandle(hChildStdinRd);
            CloseHandle(hChildStdoutRdDup);

            CloseHandle(piProcInfo.hProcess);
            CloseHandle(piProcInfo.hThread);
        } else {
            body = "CGI Error";
        }

    delete [] cmdln;

    return true;
}
