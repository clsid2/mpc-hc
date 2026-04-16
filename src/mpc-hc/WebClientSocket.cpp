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
#include "../Subtitles/TextFile.h"
#include "WebServer.h"
#include "WebClientSocket.h"
#include "text.h"
#include "PathUtils.h"

#define MAX_HEADER_SIZE 512 * 1024
#define MAX_DATA_SIZE 2 * 1024 * 1024

CWebClientSocket::CWebClientSocket(CWebServer* pWebServer, CMainFrame* pMainFrame)
    : m_pWebServer(pWebServer)
    , m_pMainFrame(pMainFrame)
    , m_buffLen(0)
    , m_buffMaxLen(2048)
    , m_buffLenProcessed(0)
    , m_parsingState(PARSING_HEADER)
    , m_dataLen(0)
{
    m_buff = DEBUG_NEW char[m_buffMaxLen];
}

CWebClientSocket::~CWebClientSocket()
{
    delete [] m_buff;
}

bool CWebClientSocket::SetCookie(CStringA name, CString value, __time64_t expire, CString path, CString domain)
{
    if (name.IsEmpty()) {
        return false;
    }
    if (value.IsEmpty()) {
        m_cookie.RemoveKey(name);
        return true;
    }

    m_cookie[name] = value;

    m_cookieattribs[name].path = path;
    m_cookieattribs[name].domain = domain;

    if (expire >= 0) {
        CTime t(expire);
        SYSTEMTIME st;
        t.GetAsSystemTime(st);
        CStringA str;
        SystemTimeToHttpDate(st, str);
        m_cookieattribs[name].expire = str;
    }

    return true;
}

void CWebClientSocket::Clear()
{
    m_buffLen = 0;
    m_buffLenProcessed = 0;

    m_parsingState = PARSING_HEADER;

    m_dataLen = 0;

    m_hdrlines.RemoveAll();
    m_data.Empty();

    m_cmd.Empty();
    m_path.Empty();
    m_ver.Empty();
    m_get.RemoveAll();
    m_post.RemoveAll();
    m_cookie.RemoveAll();
    m_request.RemoveAll();
}

void CWebClientSocket::HandleRequest()
{
    // remember new cookies

    CStringA cookie;
    if (m_hdrlines.Lookup("cookie", cookie)) {
        CAtlList<CStringA> sl;
        Explode(cookie, sl, ';');
        POSITION pos = sl.GetHeadPosition();
        while (pos) {
            CAtlList<CStringA> sl2;
            Explode(sl.GetNext(pos), sl2, '=', 2);
            if (sl2.GetCount() == 2) {
                m_cookie[sl2.GetHead()] = UTF8To16(sl2.GetTail());
            } else {
                m_cookie[sl2.GetHead()].Empty();
            }
        }
    }

    // start new session

    if (!m_cookie.Lookup("MPCSESSIONID", m_sessid)) {
        srand((unsigned int)time(nullptr));
        m_sessid.Format(_T("%08x"), rand() * 0x12345678);
        SetCookie("MPCSESSIONID", m_sessid);
    } else {
        // TODO: load session
    }

    CStringA reshdr, resbody;

    if (m_cmd == "GET" || m_cmd == "HEAD" || m_cmd == "POST") {
        int i = m_path.Find('?');
        if (i >= 0) {
            m_query = m_path.Mid(i + 1);
            m_path.Truncate(i);
        }
        m_path = UrlDecode(m_path);

        if (!m_query.IsEmpty()) {
            int j = m_query.Find('#');
            if (j >= 0) {
                m_query.Truncate(j);
            }

            CAtlList<CStringA> sl;
            Explode(m_query, sl, '&');
            POSITION pos = sl.GetHeadPosition();
            while (pos) {
                CAtlList<CStringA> sl2;
                Explode(sl.GetNext(pos), sl2, '=', 2);
                if (sl2.GetCount() == 2) {
                    m_get[sl2.GetHead()] = UTF8To16(UrlDecode(sl2.GetTail()));
                } else {
                    m_get[sl2.GetHead()] = _T("");
                }
            }
        }

        // m_request <-- m_get+m_post+m_cookie
        {
            CStringA key;
            CString value;
            POSITION pos;
            pos = m_get.GetStartPosition();
            while (pos) {
                m_get.GetNextAssoc(pos, key, value);
                m_request[key] = value;
            }
            pos = m_post.GetStartPosition();
            while (pos) {
                m_post.GetNextAssoc(pos, key, value);
                m_request[key] = value;
            }
            pos = m_cookie.GetStartPosition();
            while (pos) {
                m_cookie.GetNextAssoc(pos, key, value);
                m_request[key] = value;
            }
        }

        m_pWebServer->OnRequest(this, reshdr, resbody);
    } else {
        reshdr = "HTTP/1.0 400 Bad Request\r\n";
    }

    if (!reshdr.IsEmpty()) {
        // cookies
        {
            POSITION pos = m_cookie.GetStartPosition();
            while (pos) {
                CStringA key;
                CString value;
                m_cookie.GetNextAssoc(pos, key, value);
                reshdr += "Set-Cookie: " + key + "=" + TToA(value);
                POSITION pos2 = m_cookieattribs.GetStartPosition();
                while (pos2) {
                    cookie_attribs attribs;
                    m_cookieattribs.GetNextAssoc(pos2, key, attribs);
                    if (!attribs.path.IsEmpty()) {
                        reshdr += "; path=" + attribs.path;
                    }
                    if (!attribs.expire.IsEmpty()) {
                        reshdr += "; expire=" + attribs.expire;
                    }
                    if (!attribs.domain.IsEmpty()) {
                        reshdr += "; domain=" + attribs.domain;
                    }
                }
                reshdr += "\r\n";
            }
        }

        reshdr +=
            "Access-Control-Allow-Origin: *\r\n"
            "Server: MPC-HC WebServer\r\n"
            "Connection: close\r\n"
            "\r\n";

        Send(reshdr, reshdr.GetLength());

        if (m_cmd != "HEAD" && reshdr.Find("HTTP/1.0 200 OK") == 0 && !resbody.IsEmpty()) {
            Send(resbody, resbody.GetLength());
        }

        CStringA connection = "close";
        m_hdrlines.Lookup("connection", connection);

        Clear();

        // TODO
        // if (connection == _T("close"))
        OnClose(0);
    }
}

void CWebClientSocket::ParseHeader(const char* headerEnd)
{
    char* start = m_buff, *end;

    // Parse the request type
    end = strchr(start, ' ');
    if (!end) {
        ASSERT(false);
        return;
    }
    m_cmd.SetString(start, int(end - start));
    m_cmd.MakeUpper();
    start = end + 1;
    end = strchr(start, ' ');
    if (!end) {
        ASSERT(false);
        return;
    }
    m_path.SetString(start, int(end - start));
    start = end + 1;
    end = strstr(start, "\r\n");
    m_ver.SetString(start, int(end - start));
    m_ver.MakeUpper();

    CStringA key, val;
    start = end + 2;
    while (start < headerEnd) {
        // Parse the header fields
        end = strchr(start, ':');
        key.SetString(start, int(end - start));
        start = end + 1;
        end = strstr(start, "\r\n");
        val.SetString(start, int(end - start));
        start = end + 2;

        m_hdrlines[key.MakeLower()] = val;
    }

    if (m_cmd == "POST") {
        CStringA str;
        if (m_hdrlines.Lookup("content-length", str)) {
            m_dataLen = strtol(str, nullptr, 10);
        }
    }
    m_parsingState = (m_dataLen > 0) ? PARSING_POST_DATA : PARSING_DONE;
}

void CWebClientSocket::ParsePostData()
{
    char* start = m_buff, *end;
    char* endData = m_buff + m_buffLen;
    CStringA key, val;

    while (start < endData) {
        end = strchr(start, '=');
        if (!end) {
            break;
        }
        key.SetString(start, int(end - start));
        start = end + 1;
        end = strchr(start, '&');
        if (!end) {
            end = endData;
        }
        val.SetString(start, int(end - start));
        start = end + 1;

        m_post[key.MakeLower()] = UTF8To16(UrlDecode(val));
    }

    m_parsingState = PARSING_DONE;
}
//

void CWebClientSocket::OnReceive(int nErrorCode)
{
    if (nErrorCode == 0 && m_parsingState != PARSING_DONE) {
        if (m_buffMaxLen - m_buffLen <= 1) {
            char* buff = (char*)realloc(m_buff, 2 * m_buffMaxLen * sizeof(char));
            if (buff) {
                m_buff = buff;
                m_buffMaxLen *= 2;
            } else {
                ASSERT(0);
            }
        }

        int nRead = Receive(m_buff + m_buffLen, m_buffMaxLen - m_buffLen - 1);
        if (nRead > 0) {
            m_buffLen += nRead;
            m_buff[m_buffLen] = '\0';

            switch (m_parsingState) {
                case PARSING_HEADER: {
                    // Search the header end
                    char* headerEnd = strstr(m_buff + m_buffLenProcessed, "\r\n\r\n");

                    if (headerEnd) {
                        ParseHeader(headerEnd);
                        if (m_dataLen > MAX_DATA_SIZE) {
                            // Refuse the connection if someone tries to send
                            // more than MAX_DATA_SIZE of size.
                            OnClose(0);
                            return;
                        }

                        headerEnd += 4;
                        m_buffLen = std::max(int(m_buff + m_buffLen - headerEnd), 0);
                        if (m_buffLen > 0) {
                            memcpy(m_buff, headerEnd, m_buffLen + 1);
                            if (m_buffLen >= m_dataLen) {
                                ParsePostData();
                            }
                        }
                    } else if (m_buffLen > MAX_HEADER_SIZE) {
                        // If we got more than MAX_HEADER_SIZE of data without finding
                        // the end of the header we close the connection.
                        OnClose(0);
                        return;
                    } else {
                        // Start next search from current end of the file
                        m_buffLenProcessed += nRead;
                    }
                }
                break;
                case PARSING_POST_DATA: {
                    if (m_buffLen >= m_dataLen) {
                        ParsePostData();
                    }
                }
                break;
            }

            if (m_parsingState == PARSING_DONE) {
                HandleRequest();
            }
        }
    }
}

void CWebClientSocket::OnClose(int nErrorCode)
{
    // TODO: save session
    m_pWebServer->OnClose(this);
    __super::OnClose(nErrorCode);
}

////////////////////

bool CWebClientSocket::OnCommand(CStringA& hdr, CStringA& body, CStringA& mime)
{
    CString arg;
    if (m_request.Lookup("wm_command", arg)) {
        int id = _ttol(arg);

        if (id > 0) {
            if (id == ID_FILE_EXIT) {
                m_pMainFrame->PostMessage(WM_COMMAND, id);
            } else {
                m_pMainFrame->SendMessage(WM_COMMAND, id);
            }
        } else {
            if (arg == _T(CMD_SETPOS) && m_request.Lookup("position", arg)) {
                REFERENCE_TIME rtDur = m_pMainFrame->GetDur();
                if (rtDur == 0) {
                    return false;
                }
                int h, m, s, ms = 0;
                TCHAR c;
                if (_stscanf_s(arg, _T("%d%c%d%c%d%c%d"), &h, &c, 1, &m, &c, 1, &s, &c, 1, &ms) >= 5) {
                    REFERENCE_TIME rtPos = 10000i64 * (((h * 60 + m) * 60 + s) * 1000 + ms);
                    m_pMainFrame->SeekTo(rtPos);
                    for (int retries = 20; retries-- > 0; Sleep(50)) {
                        if (abs((int)((rtPos - m_pMainFrame->GetPos()) / 10000)) < 100) {
                            break;
                        }
                    }
                }
            } else if (arg == _T(CMD_SETPOS) && m_request.Lookup("percent", arg)) {
                REFERENCE_TIME rtDur = m_pMainFrame->GetDur();
                if (rtDur == 0) {
                    return false;
                }
                float percent = 0;
                if (_stscanf_s(arg, _T("%f"), &percent) == 1) {
                    m_pMainFrame->SeekTo((REFERENCE_TIME)(percent / 100 * rtDur));
                }
            } else if (arg == _T(CMD_SETVOLUME) && m_request.Lookup("volume", arg)) {
                int volume = _tcstol(arg, nullptr, 10);
                m_pMainFrame->m_wndToolBar.Volume = std::min(std::max(volume, 0), 100);
                m_pMainFrame->OnPlayVolume(0);
            }
        }
    }

    CStringA ref;
    if (!m_hdrlines.Lookup("referer", ref)) {
        return true;
    }

    hdr =
        "HTTP/1.0 302 Found\r\n"
        "Location: " + ref + "\r\n";

    return true;
}

bool CWebClientSocket::OnIndex(CStringA& hdr, CStringA& body, CStringA& mime)
{
    CStringA wmcoptions;

    // generate page

    const CAppSettings& s = AfxGetAppSettings();
    POSITION pos = s.wmcmds.GetHeadPosition();
    while (pos) {
        const wmcmd& wc = s.wmcmds.GetNext(pos);
        CStringA str;
        str.Format("%u", wc.cmd);
        CStringA valueName(UTF8(wc.GetName()));
        valueName.Replace("&", "&amp;");
        wmcoptions += "<option value=\"" + str + "\">" + valueName + "</option>\r\n";
    }

    m_pWebServer->LoadPage(IDR_HTML_INDEX, body, AToT(m_path));
    body.Replace("[wmcoptions]", wmcoptions);

    return true;
}

bool CWebClientSocket::OnInfo(CStringA& hdr, CStringA& body, CStringA& mime)
{
    m_pWebServer->LoadPage(IDR_HTML_INFO, body, AToT(m_path));
    body.Replace("[version]", UTF8(AfxGetMyApp()->m_strVersion));
    body.Replace("[file]", UTF8(m_pMainFrame->GetFileName()));
    body.Replace("[position]", UTF8(ReftimeToString2(m_pMainFrame->GetPos())));
    body.Replace("[duration]", UTF8(ReftimeToString2(m_pMainFrame->GetDur())));
    body.Replace("[size]", UTF8(GetSize()));
    return true;
}

bool CWebClientSocket::OnBrowser(CStringA& hdr, CStringA& body, CStringA& mime)
{
    CAtlList<CStringA> rootdrives;
    for (TCHAR drive[] = _T("A:"); drive[0] <= _T('Z'); drive[0]++) {
        if (GetDriveType(drive) != DRIVE_NO_ROOT_DIR) {
            rootdrives.AddTail(CStringA(drive) + '\\');
        }
    }
    // process GET

    CString path;
    CFileStatus fs;
    if (m_get.Lookup("path", path)) {
        if (!PathUtils::IsURL(path) && CFileGetStatus(path, fs) && !(fs.m_attribute & CFile::directory)) {
            // TODO: make a new message for just opening files, this is a bit overkill now...

            CAtlList<CString> cmdln;

            cmdln.AddTail(path);

            CString focus;
            if (m_get.Lookup("focus", focus) && !focus.CompareNoCase(_T("no"))) {
                cmdln.AddTail(_T("/nofocus"));
            }

            int len = 0;

            POSITION pos = cmdln.GetHeadPosition();
            while (pos) {
                CString& str = cmdln.GetNext(pos);
                len += (str.GetLength() + 1) * sizeof(TCHAR);
            }

            CAutoVectorPtr<BYTE> buff;
            if (buff.Allocate(4 + len)) {
                BYTE* p = buff;
                *(DWORD*)p = (DWORD)cmdln.GetCount();
                p += sizeof(DWORD);

                pos = cmdln.GetHeadPosition();
                while (pos) {
                    CString& str = cmdln.GetNext(pos);
                    len = (str.GetLength() + 1) * sizeof(TCHAR);
                    memcpy(p, (LPCTSTR)str, len);
                    p += len;
                }

                COPYDATASTRUCT cds;
                cds.dwData = 0x6ABE51;
                cds.cbData = DWORD(p - buff);
                cds.lpData = (void*)(BYTE*)buff;
                m_pMainFrame->SendMessage(WM_COPYDATA, (WPARAM)nullptr, (LPARAM)&cds);
            }

            CPath p(path);
            p.RemoveFileSpec();
            path = (LPCTSTR)p;
        }
    } else {
        path = m_pMainFrame->m_wndPlaylistBar.GetCurFileName();

        if (!PathUtils::IsURL(path) && CFileGetStatus(path, fs) && !(fs.m_attribute & CFile::directory)) {
            CPath p(path);
            p.RemoveFileSpec();
            path = (LPCTSTR)p;
        }
    }

    if (PathUtils::IsURL(path)) {
        path.Empty();
    }

    if (!path.IsEmpty() && CFileGetStatus(path, fs) && (fs.m_attribute & CFile::directory)
            || path.Find(_T("\\")) == 0) { // FIXME
        CPath p(path);
        p.Canonicalize();
        p.MakePretty();
        p.AddBackslash();
        path = (LPCTSTR)p;
    }

    CStringA files;

    if (path.IsEmpty()) {
        POSITION pos = rootdrives.GetHeadPosition();
        while (pos) {
            CStringA& drive = rootdrives.GetNext(pos);

            files += "<tr>\r\n";
            files +=
                "<td class=\"dirname\"><a href=\"[path]?path=" + UrlEncode(drive) + "\">" + drive + "</a></td>\r\n"
                "<td class=\"dirtype\">Directory</td>\r\n"
                "<td class=\"dirsize\">&nbsp;</td>\r\n"
                "<td class=\"dirdate\">&nbsp;</td>\r\n";
            files += "</tr>\r\n";
        }

        path = _T("Root");
    } else {
        CString parent;

        if (path.GetLength() > 3) {
            CPath p(path + "..");
            p.Canonicalize();
            p.AddBackslash();
            parent = (LPCTSTR)p;
        }

        files += "<tr>\r\n";
        files +=
            "<td class=\"dirname\"><a href=\"[path]?path=" + UTF8Arg(parent) + "\">..</a></td>\r\n"
            "<td class=\"dirtype\">Directory</td>\r\n"
            "<td class=\"dirsize\">&nbsp;</td>\r\n"
            "<td class=\"dirdate\">&nbsp;</td>\r\n";
        files += "</tr>\r\n";

        WIN32_FIND_DATA fd;
        ZeroMemory(&fd, sizeof(WIN32_FIND_DATA));

        HANDLE hFind = FindFirstFile(path + "*.*", &fd);
        if (hFind != INVALID_HANDLE_VALUE) {
            do {
                if (!(fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) || fd.cFileName[0] == '.') {
                    continue;
                }

                CString fullpath = path + fd.cFileName;

                files += "<tr>\r\n";
                files +=
                    "<td class=\"dirname\"><a href=\"[path]?path=" + UTF8Arg(fullpath) + "\">" + UTF8(fd.cFileName) + "</a></td>\r\n"
                    "<td class=\"dirtype\">Directory</td>\r\n"
                    "<td class=\"dirsize\">&nbsp;</td>\r\n"
                    "<td class=\"dirdate\"><span class=\"nobr\">" + CStringA(CTime(fd.ftLastWriteTime).Format(_T("%Y.%m.%d %H:%M"))) + "</span></td>\r\n";
                files += "</tr>\r\n";
            } while (FindNextFile(hFind, &fd));

            FindClose(hFind);
        }

        hFind = FindFirstFile(path + "*.*", &fd);
        if (hFind != INVALID_HANDLE_VALUE) {
            do {
                if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
                    continue;
                }

                CString fullpath = path + fd.cFileName;
                TCHAR* ext = _tcsrchr(fd.cFileName, '.');
                if (ext != nullptr) {
                    ext++;
                }

                CStringA size;
                size.Format("%I64uK", ((UINT64)fd.nFileSizeHigh << 22) | (fd.nFileSizeLow >> 10));

                CString type(_T("&nbsp;"));
                LoadType(fullpath, type);

                if (ext != nullptr) {
                    files += "<tr class=\"" + UTF8(ext) + "\">\r\n";
                } else {
                    files += "<tr class=\"noext\">\r\n";
                }
                files +=
                    "<td><a href=\"[path]?path=" + UTF8Arg(fullpath) + "\">" + UTF8(fd.cFileName) + "</a></td>\r\n"
                    "<td><span class=\"nobr\">" + UTF8(type) + "</span></td>\r\n"
                    "<td><span class=\"nobr\">" + size + "</span></td>\r\n"
                    "<td><span class=\"nobr\">" + CStringA(CTime(fd.ftLastWriteTime).Format(_T("%Y.%m.%d %H:%M"))) + "</span></td>\r\n";
                files += "</tr>\r\n";
            } while (FindNextFile(hFind, &fd));

            FindClose(hFind);
        }
    }

    m_pWebServer->LoadPage(IDR_HTML_BROWSER, body, AToT(m_path));
    body.Replace("[currentdir]", HtmlSpecialChars(UTF8(path)));
    body.Replace("[currentfiles]", files);

    return true;
}

bool CWebClientSocket::OnControls(CStringA& hdr, CStringA& body, CStringA& mime)
{
    CString path = m_pMainFrame->m_wndPlaylistBar.GetCurFileName();
    CString dir;

    if (!path.IsEmpty() && !PathUtils::IsURL(path)) {
        CPath p(path);
        p.RemoveFileSpec();
        dir = (LPCTSTR)p;
    }

    OAFilterState fs = m_pMainFrame->GetMediaState();
    CString state;
    state.Format(_T("%d"), fs);
    CString statestring;
    switch (fs) {
        case State_Stopped:
            statestring.LoadString(IDS_CONTROLS_STOPPED);
            break;
        case State_Paused:
            statestring.LoadString(IDS_CONTROLS_PAUSED);
            break;
        case State_Running:
            statestring.LoadString(IDS_CONTROLS_PLAYING);
            break;
        default:
            statestring = _T("N/A");
            break;
    }

    CString volumelevel, muted;
    volumelevel.Format(_T("%d"), m_pMainFrame->m_wndToolBar.m_volctrl.GetPos());
    muted.Format(_T("%d"), m_pMainFrame->m_wndToolBar.Volume == -10000 ? 1 : 0);

    CString reloadtime(_T("0")); // TODO

    m_pWebServer->LoadPage(IDR_HTML_CONTROLS, body, AToT(m_path));
    body.Replace("[filepatharg]", UTF8Arg(path));
    body.Replace("[filepath]", UTF8(path));
    body.Replace("[filedirarg]", UTF8Arg(dir));
    body.Replace("[filedir]", UTF8(dir));
    body.Replace("[state]", UTF8(state));
    body.Replace("[statestring]", UTF8(statestring));
    body.Replace("[position]", UTF8(NumToCString(std::lround(m_pMainFrame->GetPos() / 10000i64))));
    body.Replace("[positionstring]", UTF8(ReftimeToString2(m_pMainFrame->GetPos())));
    body.Replace("[duration]", UTF8(NumToCString(std::lround(m_pMainFrame->GetDur() / 10000i64))));
    body.Replace("[durationstring]", UTF8(ReftimeToString2(m_pMainFrame->GetDur())));
    body.Replace("[volumelevel]", UTF8(volumelevel));
    body.Replace("[muted]", UTF8(muted));
    body.Replace("[playbackrate]", UTF8(NumToCString(m_pMainFrame->GetPlayingRate())));
    body.Replace("[reloadtime]", UTF8(reloadtime));

    return true;
}

bool CWebClientSocket::OnVariables(CStringA& hdr, CStringA& body, CStringA& mime)
{
    CString path = m_pMainFrame->m_wndPlaylistBar.GetCurFileName();
    CString dir;
    CString strName;

    if (!path.IsEmpty() && !PathUtils::IsURL(path)) {
        CPath p(path);
        p.RemoveFileSpec();
        dir = (LPCTSTR)p;
    }

    OAFilterState fs = m_pMainFrame->GetMediaState();
    CString state;
    state.Format(_T("%d"), fs);
    CString statestring;
    switch (fs) {
        case State_Stopped:
            statestring.LoadString(IDS_CONTROLS_STOPPED);
            break;
        case State_Paused:
            statestring.LoadString(IDS_CONTROLS_PAUSED);
            break;
        case State_Running:
            statestring.LoadString(IDS_CONTROLS_PLAYING);
            break;
        default:
            statestring = _T("N/A");
            break;
    }

    CString volumelevel, muted;
    volumelevel.Format(_T("%d"), m_pMainFrame->m_wndToolBar.m_volctrl.GetPos());
    muted.Format(_T("%d"), m_pMainFrame->m_wndToolBar.Volume == -10000 ? 1 : 0);

    CString reloadtime(_T("0")); // TODO

    m_pWebServer->LoadPage(IDR_HTML_VARIABLES, body, AToT(m_path));
    body.Replace("[file]", UTF8(m_pMainFrame->GetFileName()));
    body.Replace("[filepatharg]", UTF8Arg(path));
    body.Replace("[filepath]", UTF8(path));
    body.Replace("[filedirarg]", UTF8Arg(dir));
    body.Replace("[filedir]", UTF8(dir));
    body.Replace("[state]", UTF8(state));
    body.Replace("[statestring]", UTF8(statestring));
    body.Replace("[position]", UTF8(NumToCString(std::lround(m_pMainFrame->GetPos() / 10000i64))));
    body.Replace("[positionstring]", UTF8(ReftimeToString2(m_pMainFrame->GetPos())));
    body.Replace("[duration]", UTF8(NumToCString(std::lround(m_pMainFrame->GetDur() / 10000i64))));
    body.Replace("[durationstring]", UTF8(ReftimeToString2(m_pMainFrame->GetDur())));
    body.Replace("[volumelevel]", UTF8(volumelevel));
    body.Replace("[muted]", UTF8(muted));
    body.Replace("[playbackrate]", UTF8(NumToCString(m_pMainFrame->GetPlayingRate())));
    body.Replace("[size]", UTF8(GetSize()));
    body.Replace("[reloadtime]", UTF8(reloadtime));
    body.Replace("[version]", UTF8(AfxGetMyApp()->m_strVersion));
    m_pMainFrame->GetCurrentAudioTrackIdx(&strName);
    body.Replace("[audiotrack]", UTF8(strName));
    m_pMainFrame->GetCurrentSubtitleTrackIdx(&strName);
    body.Replace("[subtitletrack]", UTF8(strName));

    return true;
}

bool CWebClientSocket::OnStatus(CStringA& hdr, CStringA& body, CStringA& mime)
{
    CString title;
    m_pMainFrame->GetWindowText(title);

    CPath file(m_pMainFrame->m_wndPlaylistBar.GetCurFileName());

    CString status;
    OAFilterState fs = m_pMainFrame->GetMediaState();
    switch (fs) {
        case State_Stopped:
            status.LoadString(IDS_CONTROLS_STOPPED);
            break;
        case State_Paused:
            status.LoadString(IDS_CONTROLS_PAUSED);
            break;
        case State_Running:
            status.LoadString(IDS_CONTROLS_PLAYING);
            break;
        default:
            status = _T("N/A");
            break;
    }

    REFERENCE_TIME pos = m_pMainFrame->GetPos();
    REFERENCE_TIME dur = m_pMainFrame->GetDur();

    title.Replace(_T("'"), _T("\\'"));
    status.Replace(_T("'"), _T("\\'"));

    body.Format("OnStatus(\"%s\", \"%s\", %ld, \"%s\", %ld, \"%s\", %d, %d, \"%s\")", // , \"%s\"
                UTF8(title).GetString(), UTF8(status).GetString(),
                std::lround(pos / 10000i64), UTF8(ReftimeToString2(pos)).GetString(),
                std::lround(dur / 10000i64), UTF8(ReftimeToString2(dur)).GetString(),
                m_pMainFrame->IsMuted(), m_pMainFrame->GetVolume(),
                UTF8(file).GetString()/*, UTF8(dir)*/);

    return true;
}

bool CWebClientSocket::OnError404(CStringA& hdr, CStringA& body, CStringA& mime)
{
    m_pWebServer->LoadPage(IDR_HTML_404, body, AToT(m_path));
    return true;
}

bool CWebClientSocket::OnPlayer(CStringA& hdr, CStringA& body, CStringA& mime)
{
    m_pWebServer->LoadPage(IDR_HTML_PLAYER, body, AToT(m_path));
    if (AfxGetAppSettings().bWebUIEnablePreview) {
        body.Replace("[preview]",
                     "<img src=\"snapshot.jpg\" id=\"snapshot\" alt=\"snapshot\" onload=\"onLoadSnapshot()\" onabort=\"onAbortErrorSnapshot()\" onerror=\"onAbortErrorSnapshot()\">");
    } else {
        body.Replace("[preview]", UTF8(StrRes(IDS_WEBUI_DISABLED_PREVIEW_MSG)));
    }
    return true;
}

bool CWebClientSocket::OnSnapshotJpeg(CStringA& hdr, CStringA& body, CStringA& mime)
{
    // TODO: add quality control and return logo when nothing is loaded

    bool bRet = false;

    BYTE* pData = nullptr;
    long size = 0;
    if (!AfxGetAppSettings().bWebUIEnablePreview) {
        hdr = "HTTP/1.0 403 Forbidden\r\n";
        bRet = true;
    } else if (m_pMainFrame->GetDIB(&pData, size, true)) {
        PBITMAPINFO bi = reinterpret_cast<PBITMAPINFO>(pData);
        PBITMAPINFOHEADER bih = &bi->bmiHeader;

        int bpp = bih->biBitCount;
        if (bpp != 16 && bpp != 24 && bpp != 32) {
            return false;
        }
        int w = bih->biWidth;
        int h = abs(bih->biHeight);
        BYTE* p = DEBUG_NEW BYTE[w * h * 4];

        const BYTE* src = pData + sizeof(*bih);

        int srcpitch = w * (bpp >> 3);
        int dstpitch = w * 4;

        BitBltFromRGBToRGB(w, h, p, dstpitch, 32, (BYTE*)src + srcpitch * (h - 1), -srcpitch, bpp);

        {
            CBitmap bmp;
            bmp.CreateBitmap(w, h, bih->biPlanes, bpp, p);
            delete [] p;

            CImage img;
            img.Attach(bmp);
            IStream* pStream = nullptr;
            CByteArray ba;
            if (SUCCEEDED(CreateStreamOnHGlobal(nullptr, TRUE, &pStream))) {
                if (SUCCEEDED(img.Save(pStream, Gdiplus::ImageFormatJPEG))) {
                    ULARGE_INTEGER ulnSize;
                    LARGE_INTEGER lnOffset;
                    lnOffset.QuadPart = 0;
                    if (SUCCEEDED(pStream->Seek(lnOffset, STREAM_SEEK_END, &ulnSize))) {
                        if (SUCCEEDED(pStream->Seek(lnOffset, STREAM_SEEK_SET, 0))) {
                            ULONG ulBytesRead;
                            ba.SetSize((INT_PTR)ulnSize.QuadPart);
                            pStream->Read(ba.GetData(), (ULONG)ulnSize.QuadPart, &ulBytesRead);
                        }
                    }
                }
            }

            pStream->Release();
            delete [] pData;

            hdr +=
                "Expires: Thu, 19 Nov 1981 08:52:00 GMT\r\n"
                "Cache-Control: no-store, no-cache, must-revalidate, post-check=0, pre-check=0\r\n"
                "Pragma: no-cache\r\n";
            body = CStringA((char*)ba.GetData(), (int)ba.GetCount());
            mime = "image/jpeg";
            bRet = true;
        }
    }

    return bRet;
}

bool CWebClientSocket::OnViewRes(CStringA& hdr, CStringA& body, CStringA& mime)
{
    CString id;
    if (!m_get.Lookup("id", id)) {
        return false;
    }

    uintptr_t key = 0;
    if (1 != _stscanf_s(id, _T("%Ix"), &key) || key == 0) {
        return false;
    }

    CAutoLock cAutoLock(&CDSMResource::m_csResources);

    CDSMResource* res = nullptr;
    if (!CDSMResource::m_resources.Lookup(key, res) || !res) {
        return false;
    }

    body = CStringA((const char*)res->data.GetData(), (int)res->data.GetCount());
    mime = res->mime;

    return true;
}

static CStringA GetChannelsJSON(const std::vector<CBDAChannel>& channels)
{
    // begin the JSON object with the "channels" array inside
    CStringA jsonChannels = "{ \"channels\" : [";

    for (auto it = channels.begin(); it != channels.end();) {
        // fill the array with individual channel objects
        jsonChannels += it->ToJSON();
        if (++it == channels.end()) {
            break;
        }
        jsonChannels += ",";
    }

    // terminate the array and the object, and return.
    jsonChannels += "] }";
    return jsonChannels;
}

bool CWebClientSocket::OnDVBChannels(CStringA& hdr, CStringA& body, CStringA& mime)
{
    if (m_pMainFrame->GetPlaybackMode() == PM_DIGITAL_CAPTURE) {
        mime = "application/json";
        body = GetChannelsJSON(AfxGetAppSettings().m_DVBChannels);
    } else {
        // if we're not currently running the capture, report the service as
        // unavailable and return.
        hdr = "HTTP/1.0 503 Service Unavailable\r\n";
    }

    // the request is always "handled", successfully or not.
    return true;
}

bool CWebClientSocket::OnDVBSetChannel(CStringA& hdr, CStringA& body, CStringA& mime)
{
    CString requestParam;
    int channelIdx;
    if (m_pMainFrame->GetPlaybackMode() == PM_DIGITAL_CAPTURE) {
        // the 'idx' GET parameter should contain a valid integer of the
        // channel to switch to.
        if (m_get.Lookup("idx", requestParam)
                && _stscanf_s(requestParam, _T("%d"), &channelIdx) == 1
                && channelIdx >= 0) {
            if (AfxGetAppSettings().FindChannelByPref(channelIdx)) {
                m_pMainFrame->SendMessage(WM_COMMAND, channelIdx + ID_NAVIGATE_JUMPTO_SUBITEM_START);
            } else {
                // Return a 404 if requested channel number was not found.
                hdr = "HTTP/1.0 404 Not Found\r\n";
            }
        } else {
            // invalid parameters.
            hdr = "HTTP/1.0 400 Bad Request\r\n";
        }
    } else {
        // not running the capture - don't report channels.
        hdr = "HTTP/1.0 503 Service Unavailable\r\n";
    }
    return true;
}

CString CWebClientSocket::GetSize() const
{
    CString sizeString;
    LONGLONG size = 0;

    if (CComQIPtr<IBaseFilter> pBF = m_pMainFrame->m_pFSF) {
        BeginEnumPins(pBF, pEP, pPin) {
            if (CComQIPtr<IAsyncReader> pAR = pPin) {
                LONGLONG total, available;
                if (SUCCEEDED(pAR->Length(&total, &available))) {
                    size = total;
                    break;
                }
            }
        }
        EndEnumPins;
    }

    if (size == 0) {
        WIN32_FIND_DATA wfd;
        HANDLE hFind = FindFirstFile(m_pMainFrame->m_wndPlaylistBar.GetCurFileName(), &wfd);
        if (hFind != INVALID_HANDLE_VALUE) {
            FindClose(hFind);
            size = (LONGLONG(wfd.nFileSizeHigh) << 32) | wfd.nFileSizeLow;
        }
    }

    const int MAX_FILE_SIZE_BUFFER = 65;
    TCHAR szFileSize[MAX_FILE_SIZE_BUFFER];
    StrFormatByteSizeW(size, szFileSize, MAX_FILE_SIZE_BUFFER);
    sizeString.Format(L"%s", szFileSize);

    return sizeString;
}
