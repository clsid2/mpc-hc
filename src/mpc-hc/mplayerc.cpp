/*
 * (C) 2003-2006 Gabest
 * (C) 2006-2018 see Authors.txt
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
#include "AboutDlg.h"
#include "CmdLineHelpDlg.h"
#include "CrashReporter.h"
#include "DSUtil.h"
#include "FakeFilterMapper2.h"
#include "FileAssoc.h"
#include "FileVersionInfo.h"
#include "Ifo.h"
#include "MainFrm.h"
#include "MhookHelper.h"
#include "PPageFormats.h"
#include "PPageSheet.h"
#include "PathUtils.h"
#include "Struct.h"
#include "UpdateChecker.h"
#include "WebServer.h"
#include "WinAPIUtils.h"
#include "mpc-hc_config.h"
#include "winddk/ntddcdvd.h"
#include <afxsock.h>
#include <atlsync.h>
#include <winternl.h>
#include <regex>
#include "ExceptionHandler.h"
#include "FGFilterLAV.h"
#include "CMPCThemeMsgBox.h"
#include "version.h"
#include "psapi.h"

HICON LoadIcon(CString fn, bool bSmallIcon, DpiHelper* pDpiHelper/* = nullptr*/)
{
    if (fn.IsEmpty()) {
        return nullptr;
    }

    CString ext = fn.Left(fn.Find(_T("://")) + 1).TrimRight(':');
    if (ext.IsEmpty() || !ext.CompareNoCase(_T("file"))) {
        ext = _T(".") + fn.Mid(fn.ReverseFind('.') + 1);
    }

    CSize size(bSmallIcon ? GetSystemMetrics(SM_CXSMICON) : GetSystemMetrics(SM_CXICON),
               bSmallIcon ? GetSystemMetrics(SM_CYSMICON) : GetSystemMetrics(SM_CYICON));

    if (pDpiHelper) {
        size.cx = pDpiHelper->ScaleSystemToOverrideX(size.cx);
        size.cy = pDpiHelper->ScaleSystemToOverrideY(size.cy);
    }

    typedef HRESULT(WINAPI * LIWSD)(HINSTANCE, PCWSTR, int, int, HICON*);
    auto loadIcon = [&size](PCWSTR pszName) {
        LIWSD pLIWSD = (LIWSD)GetProcAddress(GetModuleHandle(_T("comctl32.dll")), "LoadIconWithScaleDown");
        HICON ret = nullptr;
        if (pLIWSD) {
            pLIWSD(AfxGetInstanceHandle(), pszName, size.cx, size.cy, &ret);
        } else {
            ret = (HICON)LoadImage(AfxGetInstanceHandle(), pszName, IMAGE_ICON, size.cx, size.cy, 0);
        }
        return ret;
    };

    if (!ext.CompareNoCase(_T(".ifo"))) {
        if (HICON hIcon = loadIcon(MAKEINTRESOURCE(IDI_DVD))) {
            return hIcon;
        }
    }

    if (!ext.CompareNoCase(_T(".cda"))) {
        if (HICON hIcon = loadIcon(MAKEINTRESOURCE(IDI_AUDIOCD))) {
            return hIcon;
        }
    }

    if (!ext.CompareNoCase(_T(".unknown"))) {
        if (HICON hIcon = loadIcon(MAKEINTRESOURCE(IDI_UNKNOWN))) {
            return hIcon;
        }
    }

    do {
        CRegKey key;
        TCHAR buff[256];
        ULONG len;

        auto openRegKey = [&](HKEY hKeyParent, LPCTSTR lpszKeyName, LPCTSTR lpszValueName) {
            if (ERROR_SUCCESS == key.Open(hKeyParent, lpszKeyName, KEY_READ)) {
                len = _countof(buff);
                ZeroMemory(buff, sizeof(buff));
                CString progId;

                if (ERROR_SUCCESS == key.QueryStringValue(lpszValueName, buff, &len) && !(progId = buff).Trim().IsEmpty()) {
                    return (ERROR_SUCCESS == key.Open(HKEY_CLASSES_ROOT, progId + _T("\\DefaultIcon"), KEY_READ));
                }
            }
            return false;
        };

        if (!openRegKey(HKEY_CURRENT_USER, _T("Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\FileExts\\") + ext + _T("\\UserChoice"), _T("Progid"))
                && !openRegKey(HKEY_CLASSES_ROOT, ext, nullptr)
                && ERROR_SUCCESS != key.Open(HKEY_CLASSES_ROOT, ext + _T("\\DefaultIcon"), KEY_READ)) {
            break;
        }

        CString icon;
        len = _countof(buff);
        ZeroMemory(buff, sizeof(buff));
        if (ERROR_SUCCESS != key.QueryStringValue(nullptr, buff, &len) || (icon = buff).Trim().IsEmpty()) {
            break;
        }

        int i = icon.ReverseFind(',');
        if (i < 0) {
            break;
        }

        int id = 0;
        if (_stscanf_s(icon.Mid(i + 1), _T("%d"), &id) != 1) {
            break;
        }

        icon = icon.Left(i);
        icon.Trim(_T(" \\\""));

        HICON hIcon = nullptr;
        UINT cnt = bSmallIcon
                   ? ExtractIconEx(icon, id, nullptr, &hIcon, 1)
                   : ExtractIconEx(icon, id, &hIcon, nullptr, 1);
        if (hIcon && cnt == 1) {
            return hIcon;
        }
    } while (0);

    return loadIcon(MAKEINTRESOURCE(IDI_UNKNOWN));
}

bool LoadType(CString fn, CString& type)
{
    bool found = false;

    if (!fn.IsEmpty()) {
        CString ext = fn.Left(fn.Find(_T("://")) + 1).TrimRight(':');
        if (ext.IsEmpty() || !ext.CompareNoCase(_T("file"))) {
            ext = _T(".") + fn.Mid(fn.ReverseFind('.') + 1);
        }

        // Try MPC-HC's internal formats list
        const CMediaFormatCategory* mfc = AfxGetAppSettings().m_Formats.FindMediaByExt(ext);

        if (mfc != nullptr) {
            found = true;
            type = mfc->GetDescription();
        } else { // Fallback to registry
            CRegKey key;
            TCHAR buff[256];
            ULONG len;

            CString tmp = _T("");
            CString mplayerc_ext = _T("mplayerc") + ext;

            if (ERROR_SUCCESS == key.Open(HKEY_CLASSES_ROOT, mplayerc_ext)) {
                tmp = mplayerc_ext;
            }

            if (!tmp.IsEmpty() || ERROR_SUCCESS == key.Open(HKEY_CLASSES_ROOT, ext)) {
                found = true;

                if (tmp.IsEmpty()) {
                    tmp = ext;
                }

                while (ERROR_SUCCESS == key.Open(HKEY_CLASSES_ROOT, tmp)) {
                    len = _countof(buff);
                    ZeroMemory(buff, sizeof(buff));

                    if (ERROR_SUCCESS != key.QueryStringValue(nullptr, buff, &len)) {
                        break;
                    }

                    CString str(buff);
                    str.Trim();

                    if (str.IsEmpty() || str == tmp) {
                        break;
                    }

                    tmp = str;
                }

                type = tmp;
            }
        }
    }

    return found;
}

bool LoadResource(UINT resid, CStringA& str, LPCTSTR restype)
{
    str.Empty();
    HRSRC hrsrc = FindResource(AfxGetApp()->m_hInstance, MAKEINTRESOURCE(resid), restype);
    if (!hrsrc) {
        return false;
    }
    HGLOBAL hGlobal = LoadResource(AfxGetApp()->m_hInstance, hrsrc);
    if (!hGlobal) {
        return false;
    }
    DWORD size = SizeofResource(AfxGetApp()->m_hInstance, hrsrc);
    if (!size) {
        return false;
    }
    memcpy(str.GetBufferSetLength(size), LockResource(hGlobal), size);
    return true;
}

static bool FindRedir(const CUrl& src,const CString& body, CAtlList<CString>& urls, const std::vector<std::wregex>& res)
{
    bool bDetectHLS = false;
    for (const auto re : res) {
        std::wcmatch mc;

        for (LPCTSTR s = body; std::regex_search(s, mc, re); s += mc.position() + mc.length()) {
            CString url = mc[mc.size() - 1].str().c_str();
            url.Trim();

            if (url.CompareNoCase(_T("asf path")) == 0) {
                continue;
            }
            if (url.Find(_T("EXTM3U")) == 0 || url.Find(_T("#EXTINF")) == 0) {
                bDetectHLS = true;
                continue;
            }
            // Detect HTTP Live Streaming and let the source filter handle that
            if (bDetectHLS
                    && (url.Find(_T("EXT-X-STREAM-INF:")) != -1
                        || url.Find(_T("EXT-X-TARGETDURATION:")) != -1
                        || url.Find(_T("EXT-X-MEDIA-SEQUENCE:")) != -1)) {
                urls.RemoveAll();
                break;
            }

            CUrl dst;
            dst.CrackUrl(CString(url));
            if (_tcsicmp(src.GetSchemeName(), dst.GetSchemeName())
                    || _tcsicmp(src.GetHostName(), dst.GetHostName())
                    || _tcsicmp(src.GetUrlPath(), dst.GetUrlPath())) {
                urls.AddTail(url);
            } else {
                // recursive
                urls.RemoveAll();
                break;
            }
        }
    }

    return !urls.IsEmpty();
}

static bool FindRedir(const CString& fn, CAtlList<CString>& fns, const std::vector<std::wregex>& res)
{
    CString body;

    CTextFile f(CTextFile::UTF8);
    if (f.Open(fn)) {
        int i = 0;
        for (CString tmp; i < 10000 && f.ReadString(tmp); body += tmp + '\n', ++i) {
            ;
        }
    }

    CString dir = fn.Left(std::max(fn.ReverseFind('/'), fn.ReverseFind('\\')) + 1); // "ReverseFindOneOf"

    for (const auto re : res) {
        std::wcmatch mc;

        for (LPCTSTR s = body; std::regex_search(s, mc, re); s += mc.position() + mc.length()) {
            CString fn2 = mc[mc.size() - 1].str().c_str();
            fn2.Trim();

            if (!fn2.CompareNoCase(_T("asf path"))) {
                continue;
            }
            if (fn2.Find(_T("EXTM3U")) == 0 || fn2.Find(_T("#EXTINF")) == 0) {
                continue;
            }

            if (fn2.Find(_T(":")) < 0 && fn2.Find(_T("\\\\")) != 0 && fn2.Find(_T("//")) != 0) {
                CPath p;
                p.Combine(dir, fn2);
                fn2 = (LPCTSTR)p;
            }

            if (!fn2.CompareNoCase(fn)) {
                continue;
            }

            fns.AddTail(fn2);
        }
    }

    return !fns.IsEmpty();
}


CString GetContentType(CString fn, CAtlList<CString>* redir)
{
    fn.Trim();
    if (fn.IsEmpty()) {
        return "";
    }

    CUrl url;
    CString content, body;
    BOOL url_fail = false;
    BOOL ishttp = false;
    BOOL parsefile = false;
    BOOL isurl = PathUtils::IsURL(fn);

    // Get content type based on the URI scheme
    if (isurl) {
        url.CrackUrl(fn);

        if (_tcsicmp(url.GetSchemeName(), _T("pnm")) == 0) {
            return "audio/x-pn-realaudio";
        }
        if (_tcsicmp(url.GetSchemeName(), _T("mms")) == 0) {
            return "video/x-ms-asf";
        }
        if (_tcsicmp(url.GetSchemeName(), _T("http")) == 0 || _tcsicmp(url.GetSchemeName(), _T("https")) == 0) {
            ishttp = true;
            if (AfxGetMainFrame()->CanSendToYoutubeDL(fn)) {
                return "ytdl";
            }
        } else {
            return "";
        }
    }

    CString ext = CPath(fn).GetExtension().MakeLower();
    int p = ext.FindOneOf(_T("?#"));
    if (p > 0) {
        ext = ext.Left(p);
    }

    // no further analysis needed if known audio/video extension and points directly to a file
    if (!ext.IsEmpty()) {
        if (ext == _T(".mp4") || ext == _T(".m4v") || ext == _T(".mov") || ext == _T(".mkv") || ext == _T(".webm") || ext == _T(".avi") || ext == _T(".wmv") || ext == _T(".mpg") || ext == _T(".mpeg") || ext == _T(".flv") || ext == _T(".ogm") || ext == _T(".m2ts") || ext == _T(".ts")) {
            content = _T("video");
        } else if (ext == _T(".mp3") || ext == _T(".m4a") || ext == _T(".aac") || ext == _T(".flac") || ext == _T(".mka") || ext == _T(".ogg") || ext == _T(".opus")) {
            content = _T("audio");
        } else if (ext == _T(".mpcpl")) {
            content = _T("application/x-mpc-playlist");
        } else if (ext == _T(".m3u") || ext == _T(".m3u8")) {
            content = _T("audio/x-mpegurl");
        } else if (ext == _T(".bdmv")) {
            content = _T("application/x-bdmv-playlist");
        } else if (ext == _T(".cue")) {
            content = _T("application/x-cue-sheet");
        } else if (ext == _T(".swf")) {
            content = _T("application/x-shockwave-flash");
        }

        if (!content.IsEmpty()) {
            return content;
        }
    }

    // Get content type by getting the header response from server
    if (ishttp) {
        CInternetSession internet;
        internet.SetOption(INTERNET_OPTION_CONNECT_TIMEOUT,  5000);
        internet.SetOption(INTERNET_OPTION_RECEIVE_TIMEOUT, 10000);
        internet.SetOption(INTERNET_OPTION_SEND_TIMEOUT,    10000);
        CString headers = _T("User-Agent: MPC-HC");
        CHttpFile* httpFile = NULL;
        try {
            httpFile = (CHttpFile*)internet.OpenURL(fn,
                1,
                INTERNET_FLAG_TRANSFER_ASCII | INTERNET_FLAG_DONT_CACHE | INTERNET_FLAG_RELOAD,
                headers,
                DWORD(-1));
        }
        catch (CInternetException* pEx)
        {
            pEx->Delete();
            url_fail = true; // Timeout has most likely occured, server unreachable
            return content;
        }

        if (httpFile) {
            //CString	strContentType;
            //httpFile->QueryInfo(HTTP_QUERY_RAW_HEADERS, strContentType); // Check also HTTP_QUERY_RAW_HEADERS_CRLF
            //DWORD dw = 8192;  // Arbitrary 8192 char length for Url (should handle most cases)
            //CString urlredirect; // Retrieve the new Url in case we encountered an HTTP redirection (HTTP 302 code)
            //httpFile->QueryOption(INTERNET_OPTION_URL, urlredirect.GetBuffer(8192), &dw);
            DWORD	dwStatus;
            httpFile->QueryInfoStatusCode(dwStatus);
            switch (dwStatus) {
                case HTTP_STATUS_OK:                  // 200  request completed
                case HTTP_STATUS_CREATED:             // 201  object created, reason = new URI
                case HTTP_STATUS_ACCEPTED:            // 202  async completion (TBS)
                case HTTP_STATUS_PARTIAL:             // 203  partial completion
                case HTTP_STATUS_NO_CONTENT:          // 204  no info to return
                case HTTP_STATUS_RESET_CONTENT:       // 205  request completed, but clear form
                case HTTP_STATUS_PARTIAL_CONTENT:     // 206  partial GET furfilled
                case HTTP_STATUS_AMBIGUOUS:           // 300  server couldn't decide what to return
                case HTTP_STATUS_MOVED:               // 301  object permanently moved
                case HTTP_STATUS_REDIRECT:            // 302  object temporarily moved
                case HTTP_STATUS_REDIRECT_METHOD:     // 303  redirection w/ new access method
                case HTTP_STATUS_NOT_MODIFIED:        // 304  if-modified-since was not modified
                case HTTP_STATUS_USE_PROXY:           // 305  redirection to proxy, location header specifies proxy to use
                case HTTP_STATUS_REDIRECT_KEEP_VERB:  // 307  HTTP/1.1: keep same verb
                case 308/*HTTP_STATUS_PERMANENT_REDIRECT*/:  // 308  Object permanently moved keep verb
                    break;
                default:
                    //CString	strStatus;
                    //httpFile->QueryInfo(HTTP_QUERY_STATUS_TEXT, strStatus);	// Status String - eg OK, Not Found
                    url_fail = true;
            }

            if (url_fail) {
                httpFile->Close(); // Close() isn't called by the destructor
                delete httpFile;
                return content;
            }

            if (content.IsEmpty()) {
                httpFile->QueryInfo(HTTP_QUERY_CONTENT_TYPE, content);	// Content-Type - eg text/html
            }

            long contentsize = 0;
            CString contentlength = _T("");
            if (httpFile->QueryInfo(HTTP_QUERY_CONTENT_LENGTH, contentlength)) {
                contentsize = _ttol(contentlength);
            }           

            // Partial download of response body to further identify content types
            if (content.IsEmpty() && contentsize < 256*1024) {
                UINT br = 0;
                char buffer[513] = "";
                while (body.GetLength() < 256) {
                    br = httpFile->Read(buffer, 256);
                    if (br == 0) {
                        break;
                    }
                    buffer[br] = '\0';
                    body += buffer;
                }
                if (body.GetLength() >= 8) {
                    BOOL exit = false;
                    if (!wcsncmp((LPCWSTR)body, _T(".ra"), 3)) {
                        content = _T("audio/x-pn-realaudio");
                        exit = true;
                    } else if (!wcsncmp((LPCWSTR)body, _T(".RMF"), 4)) {
                        content = _T("audio/x-pn-realaudio");
                        exit = true;
                    }

                    if (exit) {
                        httpFile->Close();
                        delete httpFile;
                        return content;
                    }
                }
            }
            // Download larger piece of response body in case it's a playlist
            if (redir && contentsize < 256*1024 && (content == _T("audio/x-scpls") || content == _T("audio/scpls")
                || content == _T("video/x-ms-asf") || content == _T("text/plain")
                || content == _T("application/octet-stream") || content == _T("application/pls+xml"))) {
                UINT br = 0;
                char buffer[513] = "";
                while (body.GetLength() < 64 * 1024) { // should be enough for a playlist...
                    br = httpFile->Read(buffer, 256);
                    if (br == 0) {
                        break;
                    }
                    buffer[br] = '\0';
                    body += buffer;
                }
            }

            httpFile->Close();
            delete httpFile;
        }
    }

    // If content type is empty, plain text or octet-stream (weird server!) GUESS by extension if it exists.....
    if (content.IsEmpty() || content == _T("text/plain") || content == _T("application/octet-stream")) {
        if (ext == _T(".pls")) {
            content = _T("audio/x-scpls");
            parsefile = true;
        } else if (ext == _T(".asx")) {
            content = _T("video/x-ms-asf");
            parsefile = true;
        } else if (ext == _T(".ram")) {
            content = _T("audio/x-pn-realaudio");
            parsefile = true;
        }
    }

    if (redir && !content.IsEmpty() && (isurl && !body.IsEmpty() || !isurl && parsefile)) {
        std::vector<std::wregex> res;
        const std::wregex::flag_type reFlags = std::wregex::icase | std::wregex::optimize;

        if (content == _T("video/x-ms-asf")) {
            // ...://..."/>
            res.emplace_back(_T("[a-zA-Z]+://[^\n\">]*"), reFlags);
            // Ref#n= ...://...\n
            res.emplace_back(_T("Ref\\d+\\s*=\\s*[\"]*([a-zA-Z]+://[^\n\"]+)"), reFlags);
        }
        else if (content == _T("audio/x-scpls") || content == _T("audio/scpls") || content == _T("application/pls+xml")) {
            // File1=...\n
            res.emplace_back(_T("file\\d+\\s*=\\s*[\"]*([^\n\"]+)"), reFlags);
        }
        else if (content == _T("audio/x-pn-realaudio")) {
            // rtsp://...
            res.emplace_back(_T("rtsp://[^\n]+"), reFlags);
            // http://...
            res.emplace_back(_T("http://[^\n]+"), reFlags);
        }

        if (res.size()) {
            if (isurl) {
                FindRedir(url, body, *redir, res);
            } else {
                FindRedir(fn, *redir, res);
            }
        }
    }

    return content;
}

WORD AssignedToCmd(UINT keyValue)
{
    if (keyValue == 0) {
        ASSERT(false);
        return 0;
    }

    WORD assignTo = 0;
    const CAppSettings& s = AfxGetAppSettings();

    POSITION pos = s.wmcmds.GetHeadPosition();
    while (pos && !assignTo) {
        const wmcmd& wc = s.wmcmds.GetNext(pos);

        if (wc.key == keyValue) {
            assignTo = wc.cmd;
        }
    }

    return assignTo;
}

std::map<CStringW, CStringW> GetAudioDeviceList() {
    std::map<CStringW, CStringW> devicelist;
    BeginEnumSysDev(CLSID_AudioRendererCategory, pMoniker) {
        CComHeapPtr<OLECHAR> olestr;
        if (FAILED(pMoniker->GetDisplayName(0, 0, &olestr))) {
            continue;
        }
        CStringW dispname(olestr);
        CStringW friendlyname;
        CComPtr<IPropertyBag> pPB;
        if (SUCCEEDED(pMoniker->BindToStorage(0, 0, IID_PPV_ARGS(&pPB)))) {
            CComVariant var;
            if (SUCCEEDED(pPB->Read(_T("FriendlyName"), &var, nullptr))) {
                CStringW frname(var.bstrVal);
                var.Clear();
                friendlyname = frname;
                if (SUCCEEDED(pPB->Read(_T("WaveOutId"), &var, nullptr))) {
                    DWORD dw = var.intVal;
                    var.Clear();
                    if (dw != -1) { // skip default waveout
                        friendlyname = L"WaveOut: " + friendlyname;
                    }
                    friendlyname.Append(L"  [Deprecated]");
                }
            }
        } else {
            friendlyname = dispname;
        }
        devicelist.emplace(friendlyname, dispname);
    }
    EndEnumSysDev;

    return devicelist;
}

void SetAudioRenderer(int AudioDevNo)
{
    CStringArray dispnames;
    AfxGetMyApp()->m_AudioRendererDisplayName_CL = _T("");
    dispnames.Add(_T(""));
    dispnames.Add(AUDRNDT_INTERNAL);
    dispnames.Add(AUDRNDT_MPC);
    int devcount = 3;

    std::map<CStringW, CStringW> devicelist = GetAudioDeviceList();

    for (auto it = devicelist.cbegin(); it != devicelist.cend(); it++) {
        dispnames.Add((*it).second);
        devcount++;
    }

    dispnames.Add(AUDRNDT_NULL_COMP);
    dispnames.Add(AUDRNDT_NULL_UNCOMP);
    devcount += 2;

    if (AudioDevNo >= 1 && AudioDevNo <= devcount) {
        AfxGetMyApp()->m_AudioRendererDisplayName_CL = dispnames[AudioDevNo - 1];
    }
}

void SetHandCursor(HWND m_hWnd, UINT nID)
{
    SetClassLongPtr(GetDlgItem(m_hWnd, nID), GCLP_HCURSOR, (LONG_PTR)AfxGetApp()->LoadStandardCursor(IDC_HAND));
}

// CMPlayerCApp

CMPlayerCApp::CMPlayerCApp()
    : m_hNTDLL(nullptr)
    , m_bDelayingIdle(false)
    , m_bProfileInitialized(false)
    , m_bQueuedProfileFlush(false)
    , m_dwProfileLastAccessTick(0)
    , m_fClosingState(false)
    , m_bThemeLoaded(false)
{
    m_strVersion = FileVersionInfo::GetFileVersionStr(PathUtils::GetProgramPath(true));

    ZeroMemory(&m_ColorControl, sizeof(m_ColorControl));
    ResetColorControlRange();

    ZeroMemory(&m_VMR9ColorControl, sizeof(m_VMR9ColorControl));
    m_VMR9ColorControl[0].dwSize     = sizeof(VMR9ProcAmpControlRange);
    m_VMR9ColorControl[0].dwProperty = ProcAmpControl9_Brightness;
    m_VMR9ColorControl[1].dwSize     = sizeof(VMR9ProcAmpControlRange);
    m_VMR9ColorControl[1].dwProperty = ProcAmpControl9_Contrast;
    m_VMR9ColorControl[2].dwSize     = sizeof(VMR9ProcAmpControlRange);
    m_VMR9ColorControl[2].dwProperty = ProcAmpControl9_Hue;
    m_VMR9ColorControl[3].dwSize     = sizeof(VMR9ProcAmpControlRange);
    m_VMR9ColorControl[3].dwProperty = ProcAmpControl9_Saturation;

    ZeroMemory(&m_EVRColorControl, sizeof(m_EVRColorControl));

    GetRemoteControlCode = GetRemoteControlCodeMicrosoft;
}

CMPlayerCApp::~CMPlayerCApp()
{
    if (m_hNTDLL) {
        FreeLibrary(m_hNTDLL);
    }
    // Wait for any pending I/O operations to be canceled
    while (WAIT_IO_COMPLETION == SleepEx(0, TRUE));
}

int CMPlayerCApp::DoMessageBox(LPCTSTR lpszPrompt, UINT nType,
                               UINT nIDPrompt)
{
    if (AppIsThemeLoaded()) {
        CWnd* pParentWnd = CWnd::GetActiveWindow();
        if (pParentWnd == NULL) {
            pParentWnd = GetMainWnd();
            if (pParentWnd == NULL) {
                return CWinAppEx::DoMessageBox(lpszPrompt, nType, nIDPrompt);
            } else {
                pParentWnd = pParentWnd->GetLastActivePopup();
            }
        }

        CMPCThemeMsgBox dlgMessage(pParentWnd, lpszPrompt, _T(""), nType,
                                   nIDPrompt);

        return (int)dlgMessage.DoModal();
    } else {
        return CWinAppEx::DoMessageBox(lpszPrompt, nType, nIDPrompt);
    }
}

void CMPlayerCApp::DelayedIdle()
{
    m_bDelayingIdle = false;
}

BOOL CMPlayerCApp::IsIdleMessage(MSG* pMsg)
{
    BOOL ret = __super::IsIdleMessage(pMsg);
    if (ret && pMsg->message == WM_MOUSEMOVE) {
        if (m_bDelayingIdle) {
            ret = FALSE;
        } else {
            auto pMainFrm = AfxGetMainFrame();
            if (pMainFrm && m_pMainWnd) {
                const unsigned uTimeout = 100;
                // delay next WM_MOUSEMOVE initiated idle for uTimeout ms
                // if there will be no WM_MOUSEMOVE messages, WM_TIMER will initiate the idle
                pMainFrm->m_timerOneTime.Subscribe(
                    CMainFrame::TimerOneTimeSubscriber::DELAY_IDLE, std::bind(&CMPlayerCApp::DelayedIdle, this), uTimeout);
                m_bDelayingIdle = true;
            }
        }
    }
    return ret;
}

BOOL CMPlayerCApp::OnIdle(LONG lCount)
{
    BOOL ret = __super::OnIdle(lCount);

    if (!ret) {
        FlushProfile(false);
    }

    return ret;
}

BOOL CMPlayerCApp::PumpMessage()
{
    return MsgWaitForMultipleObjectsEx(0, nullptr, INFINITE, QS_ALLINPUT,
                                       MWMO_INPUTAVAILABLE | MWMO_ALERTABLE) == WAIT_IO_COMPLETION ? TRUE : __super::PumpMessage();
}

void CMPlayerCApp::ShowCmdlnSwitches() const
{
    CString cmdLine;

    if ((m_s->nCLSwitches & CLSW_UNRECOGNIZEDSWITCH) && __argc > 0) {
        cmdLine = __targv[0];
        for (int i = 1; i < __argc; i++) {
            cmdLine.AppendFormat(_T(" %s"), __targv[i]);
        }
    }

    CmdLineHelpDlg dlg(cmdLine);
    dlg.DoModal();
}

CMPlayerCApp theApp; // The one and only CMPlayerCApp object

HWND g_hWnd = nullptr;

bool CMPlayerCApp::StoreSettingsToIni()
{
    std::lock_guard<std::recursive_mutex> lock(m_profileMutex);

    CString ini = GetIniPath();
    free((void*)m_pszRegistryKey);
    m_pszRegistryKey = nullptr;
    free((void*)m_pszProfileName);
    m_pszProfileName = _tcsdup(ini);

    return true;
}

bool CMPlayerCApp::StoreSettingsToRegistry()
{
    std::lock_guard<std::recursive_mutex> lock(m_profileMutex);

    free((void*)m_pszRegistryKey);
    m_pszRegistryKey = nullptr;

    SetRegistryKey(_T("MPC-HC"));

    return true;
}

CString CMPlayerCApp::GetIniPath() const
{
    CString path = PathUtils::GetProgramPath(true);
    path = path.Left(path.ReverseFind('.') + 1) + _T("ini");
    return path;
}

bool CMPlayerCApp::IsIniValid() const
{
    return PathUtils::Exists(GetIniPath());
}

bool CMPlayerCApp::GetAppSavePath(CString& path)
{
    if (IsIniValid()) { // If settings ini file found, store stuff in the same folder as the exe file
        path = PathUtils::GetProgramPath();
    } else {
        return GetAppDataPath(path);
    }

    return true;
}

bool CMPlayerCApp::GetAppDataPath(CString& path)
{
    path.Empty();

    HRESULT hr = SHGetFolderPath(nullptr, CSIDL_APPDATA, nullptr, 0, path.GetBuffer(MAX_PATH));
    path.ReleaseBuffer();
    if (FAILED(hr)) {
        return false;
    }
    CPath p;
    p.Combine(path, _T("MPC-HC"));
    path = (LPCTSTR)p;

    return true;
}

bool CMPlayerCApp::ChangeSettingsLocation(bool useIni)
{
    std::lock_guard<std::recursive_mutex> lock(m_profileMutex);

    bool success;

    // Load favorites so that they can be correctly saved to the new location
    CAtlList<CString> filesFav, DVDsFav, devicesFav;
    m_s->GetFav(FAV_FILE, filesFav);
    m_s->GetFav(FAV_DVD, DVDsFav);
    m_s->GetFav(FAV_DEVICE, devicesFav);

    if (useIni) {
        success = StoreSettingsToIni();
        // No need to delete old mpc-hc.ini,
        // as it will be overwritten during CAppSettings::SaveSettings()
    } else {
        success = StoreSettingsToRegistry();
        _tremove(GetIniPath());
    }

    // Save favorites to the new location
    m_s->SetFav(FAV_FILE, filesFav);
    m_s->SetFav(FAV_DVD, DVDsFav);
    m_s->SetFav(FAV_DEVICE, devicesFav);

    // Save external filters to the new location
    m_s->SaveExternalFilters();

    // Write settings immediately
    m_s->SaveSettings(true);

    return success;
}

bool CMPlayerCApp::ExportSettings(CString savePath, CString subKey)
{
    bool success = false;
    m_s->SaveSettings();

    if (IsIniValid()) {
        success = !!CopyFile(GetIniPath(), savePath, FALSE);
    } else {
        CString regKey;
        if (subKey.IsEmpty()) {
            regKey.Format(_T("Software\\%s\\%s"), m_pszRegistryKey, m_pszProfileName);
        } else {
            regKey.Format(_T("Software\\%s\\%s\\%s"), m_pszRegistryKey, m_pszProfileName, subKey.GetString());
        }

        FILE* fStream;
        errno_t error = _tfopen_s(&fStream, savePath, _T("wt,ccs=UNICODE"));
        CStdioFile file(fStream);
        file.WriteString(_T("Windows Registry Editor Version 5.00\n\n"));

        success = !error && ExportRegistryKey(file, HKEY_CURRENT_USER, regKey);

        file.Close();
        if (!success && !error) {
            DeleteFile(savePath);
        }
    }

    return success;
}

void CMPlayerCApp::InitProfile()
{
    std::lock_guard<std::recursive_mutex> lock(m_profileMutex);

    if (!m_pszRegistryKey) {
        // Don't reread mpc-hc.ini if the cache needs to be flushed or it was accessed recently
        if (m_bProfileInitialized && (m_bQueuedProfileFlush || GetTickCount64() - m_dwProfileLastAccessTick < 100ULL)) {
            m_dwProfileLastAccessTick = GetTickCount64();
            return;
        }

        m_bProfileInitialized = true;
        m_dwProfileLastAccessTick = GetTickCount64();

        ASSERT(m_pszProfileName);
        if (!PathUtils::Exists(m_pszProfileName)) {
            return;
        }

        FILE* fp;
        int fpStatus;
        do { // Open mpc-hc.ini in UNICODE mode, retry if it is already being used by another process
            fp = _tfsopen(m_pszProfileName, _T("r, ccs=UNICODE"), _SH_SECURE);
            if (fp || (GetLastError() != ERROR_SHARING_VIOLATION)) {
                break;
            }
            Sleep(100);
        } while (true);
        if (!fp) {
            ASSERT(FALSE);
            return;
        }
        if (_ftell_nolock(fp) == 0L) {
            // No BOM was consumed, assume mpc-hc.ini is ANSI encoded
            fpStatus = fclose(fp);
            ASSERT(fpStatus == 0);
            do { // Reopen mpc-hc.ini in ANSI mode, retry if it is already being used by another process
                fp = _tfsopen(m_pszProfileName, _T("r"), _SH_SECURE);
                if (fp || (GetLastError() != ERROR_SHARING_VIOLATION)) {
                    break;
                }
                Sleep(100);
            } while (true);
            if (!fp) {
                ASSERT(FALSE);
                return;
            }
        }

        CStdioFile file(fp);

        ASSERT(!m_bQueuedProfileFlush);
        m_ProfileMap.clear();

        CString line, section, var, val;
        while (file.ReadString(line)) {
            // Parse mpc-hc.ini file, this parser:
            //  - doesn't trim whitespaces
            //  - doesn't remove quotation marks
            //  - omits keys with empty names
            //  - omits unnamed sections
            int pos = 0;
            if (line[0] == _T('[')) {
                pos = line.Find(_T(']'));
                if (pos == -1) {
                    continue;
                }
                section = line.Mid(1, pos - 1);
            } else if (line[0] != _T(';')) {
                pos = line.Find(_T('='));
                if (pos == -1) {
                    continue;
                }
                var = line.Mid(0, pos);
                val = line.Mid(pos + 1);
                if (!section.IsEmpty() && !var.IsEmpty()) {
                    m_ProfileMap[section][var] = val;
                }
            }
        }
        fpStatus = fclose(fp);
        ASSERT(fpStatus == 0);

        m_dwProfileLastAccessTick = GetTickCount64();
    }
}

void CMPlayerCApp::FlushProfile(bool bForce/* = true*/)
{
    std::lock_guard<std::recursive_mutex> lock(m_profileMutex);

    if (!m_pszRegistryKey) {
        if (!bForce && !m_bQueuedProfileFlush) {
            return;
        }

        m_bQueuedProfileFlush = false;

        ASSERT(m_bProfileInitialized);
        ASSERT(m_pszProfileName);

        FILE* fp;
        int fpStatus;
        do { // Open mpc-hc.ini, retry if it is already being used by another process
            fp = _tfsopen(m_pszProfileName, _T("w, ccs=UTF-8"), _SH_SECURE);
            if (fp || (GetLastError() != ERROR_SHARING_VIOLATION)) {
                break;
            }
            Sleep(100);
        } while (true);
        if (!fp) {
            ASSERT(FALSE);
            return;
        }
        CStdioFile file(fp);
        CString line;
        try {
            file.WriteString(_T("; MPC-HC\n"));
            for (auto it1 = m_ProfileMap.begin(); it1 != m_ProfileMap.end(); ++it1) {
                line.Format(_T("[%s]\n"), it1->first.GetString());
                file.WriteString(line);
                for (auto it2 = it1->second.begin(); it2 != it1->second.end(); ++it2) {
                    line.Format(_T("%s=%s\n"), it2->first.GetString(), it2->second.GetString());
                    file.WriteString(line);
                }
            }
        } catch (CFileException& e) {
            // Fail silently if disk is full
            UNREFERENCED_PARAMETER(e);
            ASSERT(FALSE);
        }
        fpStatus = fclose(fp);
        ASSERT(fpStatus == 0);
    }
}

BOOL CMPlayerCApp::GetProfileBinary(LPCTSTR lpszSection, LPCTSTR lpszEntry, LPBYTE* ppData, UINT* pBytes)
{
    std::lock_guard<std::recursive_mutex> lock(m_profileMutex);

    if (m_pszRegistryKey) {
        return CWinAppEx::GetProfileBinary(lpszSection, lpszEntry, ppData, pBytes);
    } else {
        if (!lpszSection || !lpszEntry || !ppData || !pBytes) {
            ASSERT(FALSE);
            return FALSE;
        }
        CString sectionStr(lpszSection);
        CString keyStr(lpszEntry);
        if (sectionStr.IsEmpty() || keyStr.IsEmpty()) {
            ASSERT(FALSE);
            return FALSE;
        }
        CString valueStr;

        InitProfile();
        auto it1 = m_ProfileMap.find(sectionStr);
        if (it1 != m_ProfileMap.end()) {
            auto it2 = it1->second.find(keyStr);
            if (it2 != it1->second.end()) {
                valueStr = it2->second;
            }
        }
        if (valueStr.IsEmpty()) {
            return FALSE;
        }
        int length = valueStr.GetLength();
        // Encoding: each 4-bit sequence is coded in one character, from 'A' for 0x0 to 'P' for 0xf
        if (length % 2) {
            ASSERT(FALSE);
            return FALSE;
        }
        for (int i = 0; i < length; i++) {
            if (valueStr[i] < 'A' || valueStr[i] > 'P') {
                ASSERT(FALSE);
                return FALSE;
            }
        }
        *pBytes = length / 2;
        *ppData = new (std::nothrow) BYTE[*pBytes];
        if (!(*ppData)) {
            ASSERT(FALSE);
            return FALSE;
        }
        for (UINT i = 0; i < *pBytes; i++) {
            (*ppData)[i] = BYTE((valueStr[i * 2] - 'A') | ((valueStr[i * 2 + 1] - 'A') << 4));
        }
        return TRUE;
    }
}

UINT CMPlayerCApp::GetProfileInt(LPCTSTR lpszSection, LPCTSTR lpszEntry, int nDefault)
{
    std::lock_guard<std::recursive_mutex> lock(m_profileMutex);

    int res = nDefault;
    if (m_pszRegistryKey) {
        res = CWinAppEx::GetProfileInt(lpszSection, lpszEntry, nDefault);
    } else {
        if (!lpszSection || !lpszEntry) {
            ASSERT(FALSE);
            return res;
        }
        CString sectionStr(lpszSection);
        CString keyStr(lpszEntry);
        if (sectionStr.IsEmpty() || keyStr.IsEmpty()) {
            ASSERT(FALSE);
            return res;
        }

        InitProfile();
        auto it1 = m_ProfileMap.find(sectionStr);
        if (it1 != m_ProfileMap.end()) {
            auto it2 = it1->second.find(keyStr);
            if (it2 != it1->second.end()) {
                res = _ttoi(it2->second);
            }
        }
    }
    return res;
}

std::list<CStringW> CMPlayerCApp::GetSectionSubKeys(LPCWSTR lpszSection) {
    std::lock_guard<std::recursive_mutex> lock(m_profileMutex);

    std::list<CStringW> keys;

    if (m_pszRegistryKey) {
        WCHAR    achKey[MAX_REGKEY_LEN];   // buffer for subkey name
        DWORD    cbName;                   // size of name string 
        DWORD    cSubKeys = 0;             // number of subkeys 

        if (HKEY hAppKey = GetAppRegistryKey()) {
            HKEY hSectionKey;
            if (ERROR_SUCCESS == RegOpenKeyExW(hAppKey, lpszSection, 0, KEY_READ, &hSectionKey)) {
                RegQueryInfoKeyW(hSectionKey, NULL, NULL, NULL, &cSubKeys, NULL, NULL, NULL, NULL, NULL, NULL, NULL);

                if (cSubKeys) {
                    for (DWORD i = 0; i < cSubKeys; i++) {
                        cbName = MAX_REGKEY_LEN;
                        if (ERROR_SUCCESS == RegEnumKeyExW(hSectionKey, i, achKey, &cbName, NULL, NULL, NULL, NULL)){
                            keys.push_back(achKey);
                        }
                    }
                }
            }
        }
    } else {
        if (!lpszSection) {
            ASSERT(FALSE);
            return keys;
        }
        CStringW sectionStr(lpszSection);
        if (sectionStr.IsEmpty()) {
            ASSERT(FALSE);
            return keys;
        }
        InitProfile();
        auto it1 = m_ProfileMap.begin();
        while (it1 != m_ProfileMap.end()) {
            if (it1->first.Find(sectionStr + L"\\") == 0) {
                CStringW subKey = it1->first.Mid(sectionStr.GetLength() + 1);
                if (subKey.Find(L"\\") == -1) {
                    keys.push_back(subKey);
                }
            }
            it1++;
        }

    }
    return keys;
}


CString CMPlayerCApp::GetProfileString(LPCTSTR lpszSection, LPCTSTR lpszEntry, LPCTSTR lpszDefault)
{
    std::lock_guard<std::recursive_mutex> lock(m_profileMutex);

    CString res;
    if (m_pszRegistryKey) {
        res = CWinAppEx::GetProfileString(lpszSection, lpszEntry, lpszDefault);
    } else {
        if (!lpszSection || !lpszEntry) {
            ASSERT(FALSE);
            return res;
        }
        CString sectionStr(lpszSection);
        CString keyStr(lpszEntry);
        if (sectionStr.IsEmpty() || keyStr.IsEmpty()) {
            ASSERT(FALSE);
            return res;
        }
        if (lpszDefault) {
            res = lpszDefault;
        }

        InitProfile();
        auto it1 = m_ProfileMap.find(sectionStr);
        if (it1 != m_ProfileMap.end()) {
            auto it2 = it1->second.find(keyStr);
            if (it2 != it1->second.end()) {
                res = it2->second;
            }
        }
    }
    return res;
}

BOOL CMPlayerCApp::WriteProfileBinary(LPCTSTR lpszSection, LPCTSTR lpszEntry, LPBYTE pData, UINT nBytes)
{
    std::lock_guard<std::recursive_mutex> lock(m_profileMutex);

    if (m_pszRegistryKey) {
        return CWinAppEx::WriteProfileBinary(lpszSection, lpszEntry, pData, nBytes);
    } else {
        if (!lpszSection || !lpszEntry || !pData || !nBytes) {
            ASSERT(FALSE);
            return FALSE;
        }
        CString sectionStr(lpszSection);
        CString keyStr(lpszEntry);
        if (sectionStr.IsEmpty() || keyStr.IsEmpty()) {
            ASSERT(FALSE);
            return FALSE;
        }
        CString valueStr;

        TCHAR* buffer = valueStr.GetBufferSetLength(nBytes * 2);
        // Encoding: each 4-bit sequence is coded in one character, from 'A' for 0x0 to 'P' for 0xf
        for (UINT i = 0; i < nBytes; i++) {
            buffer[i * 2] = 'A' + (pData[i] & 0xf);
            buffer[i * 2 + 1] = 'A' + (pData[i] >> 4 & 0xf);
        }
        valueStr.ReleaseBufferSetLength(nBytes * 2);

        InitProfile();
        CString& old = m_ProfileMap[sectionStr][keyStr];
        if (old != valueStr) {
            old = valueStr;
            m_bQueuedProfileFlush = true;
        }
        return TRUE;
    }
}

LONG CMPlayerCApp::RemoveProfileKey(LPCWSTR lpszSection, LPCWSTR lpszEntry) {
    std::lock_guard<std::recursive_mutex> lock(m_profileMutex);

    if (m_pszRegistryKey) {
        if (HKEY hAppKey = GetAppRegistryKey()) {
            HKEY hSectionKey;
            if (ERROR_SUCCESS == RegOpenKeyEx(hAppKey, lpszSection, 0, KEY_READ, &hSectionKey)) {
                return CWinAppEx::DelRegTree(hSectionKey, lpszEntry);
            }
        }
    } else {
        if (!lpszSection || !lpszEntry) {
            ASSERT(FALSE);
            return 1;
        }
        CString sectionStr(lpszSection);
        CString keyStr(lpszEntry);
        if (sectionStr.IsEmpty() || keyStr.IsEmpty()) {
            ASSERT(FALSE);
            return 1;
        }

        InitProfile();
        auto it1 = m_ProfileMap.find(sectionStr + L"\\" + keyStr);
        if (it1 != m_ProfileMap.end()) {
            m_ProfileMap.erase(it1);
            m_bQueuedProfileFlush = true;
        }
    }
    return 0;
}

BOOL CMPlayerCApp::WriteProfileInt(LPCTSTR lpszSection, LPCTSTR lpszEntry, int nValue)
{
    std::lock_guard<std::recursive_mutex> lock(m_profileMutex);

    if (m_pszRegistryKey) {
        return CWinAppEx::WriteProfileInt(lpszSection, lpszEntry, nValue);
    } else {
        if (!lpszSection || !lpszEntry) {
            ASSERT(FALSE);
            return FALSE;
        }
        CString sectionStr(lpszSection);
        CString keyStr(lpszEntry);
        if (sectionStr.IsEmpty() || keyStr.IsEmpty()) {
            ASSERT(FALSE);
            return FALSE;
        }
        CString valueStr;
        valueStr.Format(_T("%d"), nValue);

        InitProfile();
        CString& old = m_ProfileMap[sectionStr][keyStr];
        if (old != valueStr) {
            old = valueStr;
            m_bQueuedProfileFlush = true;
        }
        return TRUE;
    }
}

BOOL CMPlayerCApp::WriteProfileString(LPCTSTR lpszSection, LPCTSTR lpszEntry, LPCTSTR lpszValue)
{
    std::lock_guard<std::recursive_mutex> lock(m_profileMutex);

    if (m_pszRegistryKey) {
        return CWinAppEx::WriteProfileString(lpszSection, lpszEntry, lpszValue);
    } else {
        if (!lpszSection) {
            ASSERT(FALSE);
            return FALSE;
        }
        CString sectionStr(lpszSection);
        if (sectionStr.IsEmpty()) {
            ASSERT(FALSE);
            return FALSE;
        }
        CString keyStr(lpszEntry);
        if (lpszEntry && keyStr.IsEmpty()) {
            ASSERT(FALSE);
            return FALSE;
        }

        InitProfile();

        // Mimic CWinAppEx::WriteProfileString() behavior
        if (lpszEntry) {
            if (lpszValue) {
                CString& old = m_ProfileMap[sectionStr][keyStr];
                if (old != lpszValue) {
                    old = lpszValue;
                    m_bQueuedProfileFlush = true;
                }
            } else { // Delete key
                auto it = m_ProfileMap.find(sectionStr);
                if (it != m_ProfileMap.end()) {
                    if (it->second.erase(keyStr)) {
                        m_bQueuedProfileFlush = true;
                    }
                }
            }
        } else { // Delete section
            if (m_ProfileMap.erase(sectionStr)) {
                m_bQueuedProfileFlush = true;
            }
        }
        return TRUE;
    }
}

bool CMPlayerCApp::HasProfileEntry(LPCTSTR lpszSection, LPCTSTR lpszEntry)
{
    std::lock_guard<std::recursive_mutex> lock(m_profileMutex);

    bool ret = false;
    if (m_pszRegistryKey) {
        if (HKEY hAppKey = GetAppRegistryKey()) {
            HKEY hSectionKey;
            if (RegOpenKeyEx(hAppKey, lpszSection, 0, KEY_READ, &hSectionKey) == ERROR_SUCCESS) {
                LONG lResult = RegQueryValueEx(hSectionKey, lpszEntry, nullptr, nullptr, nullptr, nullptr);
                ret = (lResult == ERROR_SUCCESS);
                VERIFY(RegCloseKey(hSectionKey) == ERROR_SUCCESS);
            }
            VERIFY(RegCloseKey(hAppKey) == ERROR_SUCCESS);
        } else {
            ASSERT(FALSE);
        }
    } else {
        InitProfile();
        auto it1 = m_ProfileMap.find(lpszSection);
        if (it1 != m_ProfileMap.end()) {
            auto& sectionMap = it1->second;
            auto it2 = sectionMap.find(lpszEntry);
            ret = (it2 != sectionMap.end());
        }
    }
    return ret;
}

std::vector<int> CMPlayerCApp::GetProfileVectorInt(CString strSection, CString strKey) {
    std::vector<int> vData;
    UINT uSize = theApp.GetProfileInt(strSection, strKey + _T("Size"), 0);
    UINT uSizeRead = 0;
    BYTE* temp = nullptr;
    theApp.GetProfileBinary(strSection, strKey, &temp, &uSizeRead);
    if (uSizeRead == uSize) {
        vData.resize(uSizeRead / sizeof(int), 0);
        memcpy(vData.data(), temp, uSizeRead);
    }
    delete[] temp;
    temp = nullptr;
    return vData;
}


void CMPlayerCApp::WriteProfileVectorInt(CString strSection, CString strKey, std::vector<int> vData) {
    UINT uSize = static_cast<UINT>(sizeof(int) * vData.size());
    theApp.WriteProfileBinary(
        strSection,
        strKey,
        (LPBYTE)vData.data(),
        uSize
    );
    theApp.WriteProfileInt(strSection, strKey + _T("Size"), uSize);
}

void CMPlayerCApp::PreProcessCommandLine()
{
    m_cmdln.RemoveAll();

    for (int i = 1; i < __argc; i++) {
        m_cmdln.AddTail(CString(__targv[i]).Trim(_T(" \"")));
    }
}

bool CMPlayerCApp::SendCommandLine(HWND hWnd)
{
    if (m_cmdln.IsEmpty()) {
        return false;
    }

    int bufflen = sizeof(DWORD);

    POSITION pos = m_cmdln.GetHeadPosition();
    while (pos) {
        bufflen += (m_cmdln.GetNext(pos).GetLength() + 1) * sizeof(TCHAR);
    }

    CAutoVectorPtr<BYTE> buff;
    if (!buff.Allocate(bufflen)) {
        return FALSE;
    }

    BYTE* p = buff;

    *(DWORD*)p = (DWORD)m_cmdln.GetCount();
    p += sizeof(DWORD);

    pos = m_cmdln.GetHeadPosition();
    while (pos) {
        const CString& s = m_cmdln.GetNext(pos);
        int len = (s.GetLength() + 1) * sizeof(TCHAR);
        memcpy(p, s, len);
        p += len;
    }

    COPYDATASTRUCT cds;
    cds.dwData = 0x6ABE51;
    cds.cbData = bufflen;
    cds.lpData = (void*)(BYTE*)buff;

    return !!SendMessageTimeoutW(hWnd, WM_COPYDATA, (WPARAM)nullptr, (LPARAM)&cds, SMTO_ABORTIFHUNG | SMTO_NOTIMEOUTIFNOTHUNG, 5000, nullptr);
}

// CMPlayerCApp initialization

// This hook prevents the program from reporting that a debugger is attached
BOOL(WINAPI* Real_IsDebuggerPresent)() = IsDebuggerPresent;
BOOL WINAPI Mine_IsDebuggerPresent()
{
    TRACE(_T("Oops, somebody was trying to be naughty! (called IsDebuggerPresent)\n"));
    return FALSE;
}

// This hook prevents the program from reporting that a debugger is attached
NTSTATUS(WINAPI* Real_NtQueryInformationProcess)(HANDLE, PROCESSINFOCLASS, PVOID, ULONG, PULONG) = nullptr;
NTSTATUS WINAPI Mine_NtQueryInformationProcess(HANDLE ProcessHandle, PROCESSINFOCLASS ProcessInformationClass, PVOID ProcessInformation, ULONG ProcessInformationLength, PULONG ReturnLength)
{
    NTSTATUS nRet;

    nRet = Real_NtQueryInformationProcess(ProcessHandle, ProcessInformationClass, ProcessInformation, ProcessInformationLength, ReturnLength);

    if (ProcessInformationClass == ProcessBasicInformation) {
        PROCESS_BASIC_INFORMATION* pbi = (PROCESS_BASIC_INFORMATION*)ProcessInformation;
        PEB_NT* pPEB = (PEB_NT*)pbi->PebBaseAddress;
        PEB_NT PEB;

        ReadProcessMemory(ProcessHandle, pPEB, &PEB, sizeof(PEB), nullptr);
        PEB.BeingDebugged = FALSE;
        WriteProcessMemory(ProcessHandle, pPEB, &PEB, sizeof(PEB), nullptr);
    } else if (ProcessInformationClass == 7) { // ProcessDebugPort
        BOOL* pDebugPort = (BOOL*)ProcessInformation;
        *pDebugPort = FALSE;
    }

    return nRet;
}

#define USE_DLL_BLOCKLIST 0

#if USE_DLL_BLOCKLIST
#define STATUS_UNSUCCESSFUL ((NTSTATUS)0xC0000001L)

typedef enum _SECTION_INHERIT { ViewShare = 1, ViewUnmap = 2 } SECTION_INHERIT;

typedef enum _SECTION_INFORMATION_CLASS {
    SectionBasicInformation = 0,
    SectionImageInformation
} SECTION_INFORMATION_CLASS;

typedef struct _SECTION_BASIC_INFORMATION {
    PVOID BaseAddress;
    ULONG Attributes;
    LARGE_INTEGER Size;
} SECTION_BASIC_INFORMATION;

typedef NTSTATUS(STDMETHODCALLTYPE* pfn_NtMapViewOfSection)(HANDLE, HANDLE, PVOID, ULONG_PTR, SIZE_T, PLARGE_INTEGER, PSIZE_T, SECTION_INHERIT, ULONG, ULONG);
typedef NTSTATUS(STDMETHODCALLTYPE* pfn_NtUnmapViewOfSection)(HANDLE, PVOID);
typedef NTSTATUS(STDMETHODCALLTYPE* pfn_NtQuerySection)(HANDLE, SECTION_INFORMATION_CLASS, PVOID, SIZE_T, PSIZE_T);
typedef DWORD(STDMETHODCALLTYPE* pfn_GetMappedFileNameW)(HANDLE, LPVOID, LPWSTR, DWORD);

static pfn_NtMapViewOfSection Real_NtMapViewOfSection = nullptr;
static pfn_NtUnmapViewOfSection Real_NtUnmapViewOfSection = nullptr;
static pfn_NtQuerySection Real_NtQuerySection = nullptr;
static pfn_GetMappedFileNameW Real_GetMappedFileNameW = nullptr;

typedef struct {
    // DLL name, lower case, with backslash as prefix
    const wchar_t* name;
    size_t name_len;
} blocked_module_t;

// list of modules that can cause crashes or other unwanted behavior
static blocked_module_t moduleblocklist[] = {
#if WIN64
    // Logitech codec
    {_T("\\lvcod64.dll"), 12},
    // ProxyCodec64
    {_T("\\pxc0.dll"), 9},
#else
    {_T("\\mlc.dll"), 8},
#endif
    // Lame
    {_T("\\lameacm.acm"), 12},
    // ffdshow vfw
    {_T("\\ff_vfw.dll"), 11},
#if WIN64
    // Trusteer Rapport
    {_T("\\rooksbas_x64.dll"), 17},
    {_T("\\rooksdol_x64.dll"), 17},
    {_T("\\rapportgh_x64.dll"), 18},
#endif
    // ASUS GamerOSD
    {_T("\\atkdx11disp.dll"), 16},
    // ASUS GPU TWEAK II OSD
    {_T("\\gtii-osd64.dll"), 15},
    {_T("\\gtii-osd64-vk.dll"), 18},
    {_T("\\gtiii-osd64.dll"), 16},
    // Nahimic Audio
    {L"\\nahimicmsidevprops.dll", 23},
    {L"\\nahimicmsiosd.dll", 18},
    // LoiLoGameRecorder
    {_T("\\loilocap.dll"), 13},
    // Other
    {_T("\\tortoiseoverlays.dll"), 21},
};

bool IsBlockedModule(wchar_t* modulename)
{
    size_t mod_name_len = wcslen(modulename);

    //TRACE(L"Checking module blocklist: %s\n", modulename);

    for (size_t i = 0; i < _countof(moduleblocklist); i++) {
        blocked_module_t* b = &moduleblocklist[i];
        if (mod_name_len > b->name_len) {
            wchar_t* dll_ptr = modulename + mod_name_len - b->name_len;
            if (_wcsicmp(dll_ptr, b->name) == 0) {
                //AfxMessageBox(modulename);
                TRACE(L"Blocked module load: %s\n", modulename);
                return true;
            }
        }
    }

    return false;
}

NTSTATUS STDMETHODCALLTYPE Mine_NtMapViewOfSection(HANDLE SectionHandle, HANDLE ProcessHandle, PVOID* BaseAddress, ULONG_PTR ZeroBits,  SIZE_T CommitSize,
    PLARGE_INTEGER SectionOffset, PSIZE_T ViewSize, SECTION_INHERIT InheritDisposition, ULONG AllocationType, ULONG Win32Protect)
{
    NTSTATUS ret = Real_NtMapViewOfSection(SectionHandle, ProcessHandle, BaseAddress, ZeroBits, CommitSize, SectionOffset, ViewSize, InheritDisposition, AllocationType, Win32Protect);

    // Verify map and process
    if (ret < 0 || ProcessHandle != GetCurrentProcess())
        return ret;

    // Fetch section information
    SIZE_T wrote = 0;
    SECTION_BASIC_INFORMATION section_information;
    if (Real_NtQuerySection(SectionHandle, SectionBasicInformation, &section_information, sizeof(section_information), &wrote) < 0)
        return ret;

    // Verify fetch was successful
    if (wrote != sizeof(section_information))
        return ret;

    // We're not interested in non-image maps
    if (!(section_information.Attributes & SEC_IMAGE))
        return ret;

    // Get the actual filename if possible
    wchar_t fileName[MAX_PATH];
    // ToDo: switch to PSAPI_VERSION=2 and directly use K32GetMappedFileNameW ?
    if (Real_GetMappedFileNameW(ProcessHandle, *BaseAddress, fileName, _countof(fileName)) == 0)
        return ret;

    if (IsBlockedModule(fileName)) {
        Real_NtUnmapViewOfSection(ProcessHandle, BaseAddress);
        ret = STATUS_UNSUCCESSFUL;
    }

    return ret;
}
#endif

static LONG Mine_ChangeDisplaySettingsEx(LONG ret, DWORD dwFlags, LPVOID lParam)
{
    if (dwFlags & CDS_VIDEOPARAMETERS) {
        VIDEOPARAMETERS* vp = (VIDEOPARAMETERS*)lParam;

        if (vp->Guid == GUIDFromCString(_T("{02C62061-1097-11d1-920F-00A024DF156E}"))
                && (vp->dwFlags & VP_FLAGS_COPYPROTECT)) {
            if (vp->dwCommand == VP_COMMAND_GET) {
                if ((vp->dwTVStandard & VP_TV_STANDARD_WIN_VGA)
                        && vp->dwTVStandard != VP_TV_STANDARD_WIN_VGA) {
                    TRACE(_T("Ooops, tv-out enabled? macrovision checks suck..."));
                    vp->dwTVStandard = VP_TV_STANDARD_WIN_VGA;
                }
            } else if (vp->dwCommand == VP_COMMAND_SET) {
                TRACE(_T("Ooops, as I already told ya, no need for any macrovision bs here"));
                return 0;
            }
        }
    }

    return ret;
}

// These two hooks prevent the program from requesting Macrovision checks
LONG(WINAPI* Real_ChangeDisplaySettingsExA)(LPCSTR, LPDEVMODEA, HWND, DWORD, LPVOID) = ChangeDisplaySettingsExA;
LONG(WINAPI* Real_ChangeDisplaySettingsExW)(LPCWSTR, LPDEVMODEW, HWND, DWORD, LPVOID) = ChangeDisplaySettingsExW;
LONG WINAPI Mine_ChangeDisplaySettingsExA(LPCSTR lpszDeviceName, LPDEVMODEA lpDevMode, HWND hwnd, DWORD dwFlags, LPVOID lParam)
{
    return Mine_ChangeDisplaySettingsEx(Real_ChangeDisplaySettingsExA(lpszDeviceName, lpDevMode, hwnd, dwFlags, lParam), dwFlags, lParam);
}
LONG WINAPI Mine_ChangeDisplaySettingsExW(LPCWSTR lpszDeviceName, LPDEVMODEW lpDevMode, HWND hwnd, DWORD dwFlags, LPVOID lParam)
{
    return Mine_ChangeDisplaySettingsEx(Real_ChangeDisplaySettingsExW(lpszDeviceName, lpDevMode, hwnd, dwFlags, lParam), dwFlags, lParam);
}

// This hook forces files to open even if they are currently being written
HANDLE(WINAPI* Real_CreateFileA)(LPCSTR, DWORD, DWORD, LPSECURITY_ATTRIBUTES, DWORD, DWORD, HANDLE) = CreateFileA;
HANDLE WINAPI Mine_CreateFileA(LPCSTR p1, DWORD p2, DWORD p3, LPSECURITY_ATTRIBUTES p4, DWORD p5, DWORD p6, HANDLE p7)
{
    p3 |= FILE_SHARE_WRITE;

    return Real_CreateFileA(p1, p2, p3, p4, p5, p6, p7);
}

static BOOL CreateFakeVideoTS(LPCWSTR strIFOPath, LPWSTR strFakeFile, size_t nFakeFileSize)
{
    BOOL  bRet = FALSE;
    WCHAR szTempPath[MAX_PATH];
    WCHAR strFileName[MAX_PATH];
    WCHAR strExt[_MAX_EXT];
    CIfo  Ifo;

    if (!GetTempPathW(MAX_PATH, szTempPath)) {
        return FALSE;
    }

    _wsplitpath_s(strIFOPath, nullptr, 0, nullptr, 0, strFileName, _countof(strFileName), strExt, _countof(strExt));
    _snwprintf_s(strFakeFile, nFakeFileSize, _TRUNCATE, L"%sMPC%s%s", szTempPath, strFileName, strExt);

    if (Ifo.OpenFile(strIFOPath) && Ifo.RemoveUOPs() && Ifo.SaveFile(strFakeFile)) {
        bRet = TRUE;
    }

    return bRet;
}

static CMPCThemeScrollBarRenderer* GetScrollBarRenderer(HWND hWnd) {
    CWnd* pWnd = CWnd::FromHandlePermanent(hWnd);

    if (pWnd && AppNeedsThemedControls()) {
        CMPCThemePlayerListCtrl* pListCtrl = DYNAMIC_DOWNCAST(CMPCThemePlayerListCtrl, pWnd);
        if (pListCtrl) {
            return pListCtrl; // Implicit upcast to CMPCThemeScrollBarRenderer*
        }
    }
    return nullptr;
}

static BOOL(WINAPI* Real_BitBlt)(HDC, int, int, int, int, HDC, int, int, DWORD) = BitBlt;
BOOL WINAPI Mine_BitBlt(HDC hdc, int x, int y, int cx, int cy, HDC hdcSrc, int x1, int y1, DWORD rop) {
    HWND hWnd = WindowFromDC(hdc);
    CMPCThemeScrollBarRenderer* pRenderer = GetScrollBarRenderer(hWnd);
    if (pRenderer) {
        CRect drawRect(x, y, x + cx, y + cy);
        auto clipState = pRenderer->ApplyScrollbarClipping(hdc, hWnd, drawRect, true);
        
        BOOL result = TRUE;
        if (!clipState.IsFullyClipped()) {
            result = Real_BitBlt(hdc, x, y, cx, cy, hdcSrc, x1, y1, rop);
        }
        
        CMPCThemeScrollBarRenderer::RestoreClipping(hdc, clipState);
        return result;
    }
    return Real_BitBlt(hdc, x, y, cx, cy, hdcSrc, x1, y1, rop);
}

static BOOL(WINAPI* Real_GdiAlphaBlend)(HDC, int, int, int, int, HDC, int, int, int, int, BLENDFUNCTION) = GdiAlphaBlend;
BOOL WINAPI Mine_GdiAlphaBlend(HDC hdcDest, int xoriginDest, int yoriginDest, int wDest, int hDest, HDC hdcSrc, int xoriginSrc, int yoriginSrc, int wSrc, int hSrc, BLENDFUNCTION ftn) {
    HWND hWnd = WindowFromDC(hdcDest);
    CMPCThemeScrollBarRenderer* pRenderer = GetScrollBarRenderer(hWnd);

    if (pRenderer) {
        CRect drawRect(xoriginDest, yoriginDest, xoriginDest + wDest, yoriginDest + hDest);
        
        // We draw to the src of the blend function -- note, this relies on an assumption
        // that win32 uses a src hdc with the same origin as the window (double buffer hdc?)
        // Real_GdiAlphaBlend will then function normally with our src data
        auto clipState = pRenderer->ApplyScrollbarClipping(hdcSrc, hWnd, drawRect, true);
        CMPCThemeScrollBarRenderer::RestoreClipping(hdcSrc, clipState);
    }
    
    return Real_GdiAlphaBlend(hdcDest, xoriginDest, yoriginDest, wDest, hDest, hdcSrc, xoriginSrc, yoriginSrc, wSrc, hSrc, ftn);
}

static HRESULT(WINAPI* Real_DrawThemeBackground)(HTHEME, HDC, int, int, LPCRECT, LPCRECT) = DrawThemeBackground;
HRESULT WINAPI Mine_DrawThemeBackground(HTHEME hTheme, HDC hdc, int iPartId, int iStateId, LPCRECT pRect, LPCRECT pClipRect) {
    HWND hWnd = WindowFromDC(hdc);
    CMPCThemeScrollBarRenderer* pRenderer = GetScrollBarRenderer(hWnd);
    if (pRenderer) {
        CRect drawRect(pRect);
        auto clipState = pRenderer->ApplyScrollbarClipping(hdc, hWnd, drawRect, false);
        
        HRESULT result = S_OK;
        if (!clipState.IsFullyClipped()) {
            result = Real_DrawThemeBackground(hTheme, hdc, iPartId, iStateId, pRect, pClipRect);
        }
        
        CMPCThemeScrollBarRenderer::RestoreClipping(hdc, clipState);
        return result;
    }
    return Real_DrawThemeBackground(hTheme, hdc, iPartId, iStateId, pRect, pClipRect);
}

int(WINAPI* Real_ScrollWindowEx)(HWND, int, int, CONST RECT*, CONST RECT*, HRGN, LPRECT, UINT) = ScrollWindowEx;
int WINAPI Mine_ScrollWindowEx(HWND hWnd, int dx, int dy, CONST RECT* prcScroll, CONST RECT* prcClip, HRGN hrgnUpdate, LPRECT prcUpdate, UINT flags)
{
    RECT expandedClip = { 0 };
    CWnd* pWnd = CWnd::FromHandlePermanent(hWnd);
    if (pWnd && prcClip && dx && AppNeedsThemedControls()) {
        CMPCThemePlayerListCtrl* pList = dynamic_cast<CMPCThemePlayerListCtrl*>(pWnd);
        if (pList && !pList->PaintHooksActive()) {
            expandedClip = *prcClip;
            expandedClip.top = 0; //horizontal scroll will need to include header
            prcClip = &expandedClip;
        } else {
            CMPCThemeHeaderCtrl* pHeader = dynamic_cast<CMPCThemeHeaderCtrl*>(pWnd);
            if (pHeader) {
                pList = dynamic_cast<CMPCThemePlayerListCtrl*>(pWnd->GetParent());
                if (pList && !pList->PaintHooksActive()) {
                    return NULLREGION;
                }
            }
        }
    }
    return Real_ScrollWindowEx(hWnd, dx, dy, prcScroll, prcClip, hrgnUpdate, prcUpdate, flags);
}


// This hook forces files to open even if they are currently being written and hijacks
// IFO file opening so that a modified IFO with no forbidden operations is opened instead.
HANDLE(WINAPI* Real_CreateFileW)(LPCWSTR, DWORD, DWORD, LPSECURITY_ATTRIBUTES, DWORD, DWORD, HANDLE) = CreateFileW;
HANDLE WINAPI Mine_CreateFileW(LPCWSTR p1, DWORD p2, DWORD p3, LPSECURITY_ATTRIBUTES p4, DWORD p5, DWORD p6, HANDLE p7)
{
    HANDLE hFile = INVALID_HANDLE_VALUE;
    size_t nLen  = wcslen(p1);

    p3 |= FILE_SHARE_WRITE;

    if (nLen >= 4 && _wcsicmp(p1 + nLen - 4, L".ifo") == 0) {
        WCHAR strFakeFile[MAX_PATH];
        if (CreateFakeVideoTS(p1, strFakeFile, _countof(strFakeFile))) {
            hFile = Real_CreateFileW(strFakeFile, p2, p3, p4, p5, p6, p7);
        }
    }

    if (hFile == INVALID_HANDLE_VALUE) {
        hFile = Real_CreateFileW(p1, p2, p3, p4, p5, p6, p7);
    }

    return hFile;
}

// This hooks disables the DVD version check
BOOL(WINAPI* Real_DeviceIoControl)(HANDLE, DWORD, LPVOID, DWORD, LPVOID, DWORD, LPDWORD, LPOVERLAPPED) = DeviceIoControl;
BOOL WINAPI Mine_DeviceIoControl(HANDLE hDevice, DWORD dwIoControlCode, LPVOID lpInBuffer, DWORD nInBufferSize, LPVOID lpOutBuffer, DWORD nOutBufferSize, LPDWORD lpBytesReturned, LPOVERLAPPED lpOverlapped)
{
    BOOL ret = Real_DeviceIoControl(hDevice, dwIoControlCode, lpInBuffer, nInBufferSize, lpOutBuffer, nOutBufferSize, lpBytesReturned, lpOverlapped);
    if (IOCTL_DVD_GET_REGION == dwIoControlCode && lpOutBuffer && nOutBufferSize == sizeof(DVD_REGION)) {
        DVD_REGION* pDVDRegion = (DVD_REGION*)lpOutBuffer;

        if (pDVDRegion->RegionData > 0) {
            UCHAR disc_regions = ~pDVDRegion->RegionData;
            if ((disc_regions & pDVDRegion->SystemRegion) == 0) {
                if      (disc_regions & 1)   pDVDRegion->SystemRegion = 1;
                else if (disc_regions & 2)   pDVDRegion->SystemRegion = 2;
                else if (disc_regions & 4)   pDVDRegion->SystemRegion = 4;
                else if (disc_regions & 8)   pDVDRegion->SystemRegion = 8;
                else if (disc_regions & 16)  pDVDRegion->SystemRegion = 16;
                else if (disc_regions & 32)  pDVDRegion->SystemRegion = 32;
                else if (disc_regions & 128) pDVDRegion->SystemRegion = 128;
                ret = true;
            }
        } else if (pDVDRegion->SystemRegion == 0) {
            pDVDRegion->SystemRegion = 1;
            ret = true;
        }
    }
    return ret;
}

MMRESULT(WINAPI* Real_mixerSetControlDetails)(HMIXEROBJ, LPMIXERCONTROLDETAILS, DWORD) = mixerSetControlDetails;
MMRESULT WINAPI Mine_mixerSetControlDetails(HMIXEROBJ hmxobj, LPMIXERCONTROLDETAILS pmxcd, DWORD fdwDetails)
{
    if (fdwDetails == (MIXER_OBJECTF_HMIXER | MIXER_SETCONTROLDETAILSF_VALUE)) {
        return MMSYSERR_NOERROR;    // don't touch the mixer, kthx
    }
    return Real_mixerSetControlDetails(hmxobj, pmxcd, fdwDetails);
}

BOOL (WINAPI* Real_LockWindowUpdate)(HWND) = LockWindowUpdate;
BOOL WINAPI Mine_LockWindowUpdate(HWND hWndLock)
{
    // TODO: Check if needed on Windows 8+
    if (hWndLock == ::GetDesktopWindow()) {
        // locking the desktop window with aero active locks the entire compositor,
        // unfortunately MFC does that (when dragging CControlBar) and we want to prevent it
        return FALSE;
    } else {
        return Real_LockWindowUpdate(hWndLock);
    }
}

BOOL RegQueryBoolValue(HKEY hKeyRoot, LPCWSTR lpSubKey, LPCWSTR lpValuename, BOOL defaultvalue) {
    BOOL result = defaultvalue;
    HKEY hKeyOpen;
    DWORD rv = RegOpenKeyEx(hKeyRoot, lpSubKey, 0, KEY_READ, &hKeyOpen);
    if (rv == ERROR_SUCCESS) {
        DWORD data;
        DWORD dwBufferSize = sizeof(DWORD);
        rv = RegQueryValueEx(hKeyOpen, lpValuename, NULL, NULL, reinterpret_cast<LPBYTE>(&data), &dwBufferSize);
        if (rv == ERROR_SUCCESS) {
            result = (data > 0);
        }
        RegCloseKey(hKeyOpen);
    }
    return result;
}

#if USE_DRDUMP_CRASH_REPORTER
void DisableCrashReporter()
{
    if (CrashReporter::IsEnabled()) {
        CrashReporter::Disable();
        MPCExceptionHandler::Enable();
    }
}
#endif

BOOL CMPlayerCApp::InitInstance()
{
    // Remove the working directory from the search path to work around the DLL preloading vulnerability
    SetDllDirectory(_T(""));

    // At this point we have not hooked this function yet so we get the real result
    if (!IsDebuggerPresent()) {
#if !defined(_DEBUG) && USE_DRDUMP_CRASH_REPORTER
        if (RegQueryBoolValue(HKEY_CURRENT_USER, _T("Software\\MPC-HC\\MPC-HC\\Settings"), _T("EnableCrashReporter"), true)) {
            CrashReporter::Enable();
            if (!CrashReporter::IsEnabled()) {
                MPCExceptionHandler::Enable();
            }
        } else {
            MPCExceptionHandler::Enable();
        }
#else
        MPCExceptionHandler::Enable();
#endif
    }

    if (!HeapSetInformation(nullptr, HeapEnableTerminationOnCorruption, nullptr, 0)) {
        TRACE(_T("Failed to enable \"terminate on corruption\" heap option, error %u\n"), GetLastError());
        ASSERT(FALSE);
    }

    bool bHookingSuccessful = MH_Initialize() == MH_OK;

#ifndef _DEBUG
    bHookingSuccessful &= !!Mhook_SetHookEx(&Real_IsDebuggerPresent, Mine_IsDebuggerPresent);
#endif

    m_hNTDLL = LoadLibrary(_T("ntdll.dll"));
#if 0
#ifndef _DEBUG  // Disable NtQueryInformationProcess in debug (prevent VS debugger to stop on crash address)
    if (m_hNTDLL) {
        Real_NtQueryInformationProcess = (decltype(Real_NtQueryInformationProcess))GetProcAddress(m_hNTDLL, "NtQueryInformationProcess");

        if (Real_NtQueryInformationProcess) {
            bHookingSuccessful &= !!Mhook_SetHookEx(&Real_NtQueryInformationProcess, Mine_NtQueryInformationProcess);
        }
    }
#endif
#endif

    bHookingSuccessful &= !!Mhook_SetHookEx(&Real_CreateFileW, Mine_CreateFileW);
    bHookingSuccessful &= !!Mhook_SetHookEx(&Real_DeviceIoControl, Mine_DeviceIoControl);
    bHookingSuccessful &= !!Mhook_SetHookEx(&Real_ScrollWindowEx, Mine_ScrollWindowEx);
    bHookingSuccessful &= !!Mhook_SetHookEx(&Real_BitBlt, Mine_BitBlt);
    bHookingSuccessful &= !!Mhook_SetHookEx(&Real_GdiAlphaBlend, Mine_GdiAlphaBlend);
    bHookingSuccessful &= !!Mhook_SetHookEx(&Real_DrawThemeBackground, Mine_DrawThemeBackground);

    bHookingSuccessful &= MH_EnableHook(MH_ALL_HOOKS) == MH_OK;

    if (!bHookingSuccessful) {
        AfxMessageBox(IDS_HOOKS_FAILED);
    }

#if USE_DLL_BLOCKLIST
    if (m_hNTDLL) {
        Real_NtMapViewOfSection = (pfn_NtMapViewOfSection)GetProcAddress(m_hNTDLL, "NtMapViewOfSection");
        Real_NtUnmapViewOfSection = (pfn_NtUnmapViewOfSection)GetProcAddress(m_hNTDLL, "NtUnmapViewOfSection");
        Real_NtQuerySection = (pfn_NtQuerySection)GetProcAddress(m_hNTDLL, "NtQuerySection");
        Real_GetMappedFileNameW = (pfn_GetMappedFileNameW)GetProcAddress(GetModuleHandle(L"kernel32.dll"), "K32GetMappedFileNameW");
        if (Real_NtMapViewOfSection && Real_NtUnmapViewOfSection && Real_NtQuerySection && Real_GetMappedFileNameW) {
            VERIFY(Mhook_SetHookEx(&Real_NtMapViewOfSection, Mine_NtMapViewOfSection));
        }
    }
#endif

    // If those hooks fail it's annoying but try to run anyway without reporting any error in release mode
    VERIFY(Mhook_SetHookEx(&Real_ChangeDisplaySettingsExA, Mine_ChangeDisplaySettingsExA));
    VERIFY(Mhook_SetHookEx(&Real_ChangeDisplaySettingsExW, Mine_ChangeDisplaySettingsExW));
    VERIFY(Mhook_SetHookEx(&Real_CreateFileA, Mine_CreateFileA)); // The internal splitter uses the right share mode anyway so this is no big deal
    VERIFY(Mhook_SetHookEx(&Real_LockWindowUpdate, Mine_LockWindowUpdate));
    VERIFY(Mhook_SetHookEx(&Real_mixerSetControlDetails, Mine_mixerSetControlDetails));
    MH_EnableHook(MH_ALL_HOOKS);

    CFilterMapper2::Init();

    if (FAILED(OleInitialize(nullptr))) {
        AfxMessageBox(_T("OleInitialize failed!"));
        return FALSE;
    }

    m_s = std::make_unique<CAppSettings>();

    // Be careful if you move that code: IDR_MAINFRAME icon can only be loaded from the executable,
    // LoadIcon can't be used after the language DLL has been set as the main resource.
    HICON icon = LoadIcon(IDR_MAINFRAME);

    WNDCLASS wndcls;
    ZeroMemory(&wndcls, sizeof(WNDCLASS));
    wndcls.style = CS_DBLCLKS | CS_HREDRAW | CS_VREDRAW;
    wndcls.lpfnWndProc = ::DefWindowProc;
    wndcls.hInstance = AfxGetInstanceHandle();
    wndcls.hIcon = icon;
    wndcls.hCursor = LoadCursor(IDC_ARROW);
    wndcls.hbrBackground = 0;//(HBRUSH)(COLOR_WINDOW + 1); // no bkg brush, the view and the bars should always fill the whole client area
    wndcls.lpszMenuName = nullptr;
    wndcls.lpszClassName = MPC_WND_CLASS_NAME;

    if (!AfxRegisterClass(&wndcls)) {
        AfxMessageBox(_T("MainFrm class registration failed!"));
        return FALSE;
    }

    if (!AfxSocketInit(nullptr)) {
        AfxMessageBox(_T("AfxSocketInit failed!"));
        return FALSE;
    }

    PreProcessCommandLine();

    if (IsIniValid()) {
        StoreSettingsToIni();
    } else {
        StoreSettingsToRegistry();
    }

    m_s->ParseCommandLine(m_cmdln);

    VERIFY(SetCurrentDirectory(PathUtils::GetProgramPath()));

    if (m_s->nCLSwitches & (CLSW_HELP | CLSW_UNRECOGNIZEDSWITCH)) { // show commandline help window
        m_s->LoadSettings();
        ShowCmdlnSwitches();
        return FALSE;
    }

    if (m_s->nCLSwitches & CLSW_RESET) { // reset settings
        // We want the other instances to be closed before resetting the settings.
        HWND hWnd = FindWindow(MPC_WND_CLASS_NAME, nullptr);

        while (hWnd) {
            Sleep(500);

            hWnd = FindWindow(MPC_WND_CLASS_NAME, nullptr);

            if (hWnd && MessageBox(nullptr, ResStr(IDS_RESET_SETTINGS_MUTEX), ResStr(IDS_RESET_SETTINGS), MB_ICONEXCLAMATION | MB_RETRYCANCEL) == IDCANCEL) {
                return FALSE;
            }
        }

        // If the profile was already cached, it should be cleared here
        ASSERT(!m_bProfileInitialized);

        // Remove the settings
        if (IsIniValid()) {
            FILE* fp;
            do { // Open mpc-hc.ini, retry if it is already being used by another process
                fp = _tfsopen(m_pszProfileName, _T("w"), _SH_SECURE);
                if (fp || (GetLastError() != ERROR_SHARING_VIOLATION)) {
                    break;
                }
                Sleep(100);
            } while (true);
            if (fp) {
                // Close without writing anything, it should produce empty file
                VERIFY(fclose(fp) == 0);
            } else {
                ASSERT(FALSE);
            }
        } else {
            CRegKey key;
            // Clear settings
            key.Attach(GetAppRegistryKey());
            VERIFY(key.RecurseDeleteKey(_T("")) == ERROR_SUCCESS);
            VERIFY(key.Close() == ERROR_SUCCESS);
            // Set ExePath value to prevent settings migration
            key.Attach(GetAppRegistryKey());
            VERIFY(key.SetStringValue(_T("ExePath"), PathUtils::GetProgramPath(true)) == ERROR_SUCCESS);
            VERIFY(key.Close() == ERROR_SUCCESS);
        }

        // Remove the current playlist if it exists
        CString strSavePath;
        if (GetAppSavePath(strSavePath)) {
            CPath playlistPath;
            playlistPath.Combine(strSavePath, _T("default.mpcpl"));

            if (playlistPath.FileExists()) {
                try {
                    CFile::Remove(playlistPath);
                } catch (...) {}
            }
        }
    }

    if ((m_s->nCLSwitches & CLSW_CLOSE) && m_s->slFiles.IsEmpty()) { // "/close" switch and empty file list
        return FALSE;
    }

    if (m_s->nCLSwitches & (CLSW_REGEXTVID | CLSW_REGEXTAUD | CLSW_REGEXTPL)) { // register file types
        m_s->fileAssoc.RegisterApp();

        CMediaFormats& mf = m_s->m_Formats;
        mf.UpdateData(false);

        bool bAudioOnly;

        auto iconLib = m_s->fileAssoc.GetIconLib();
        if (iconLib) {
            iconLib->SaveVersion();
        }

        for (size_t i = 0, cnt = mf.GetCount(); i < cnt; i++) {
            bool bPlaylist = !mf[i].GetLabel().CompareNoCase(_T("pls"));

            if (bPlaylist && !(m_s->nCLSwitches & CLSW_REGEXTPL)) {
                continue;
            }

            bAudioOnly = mf[i].IsAudioOnly();

            if (((m_s->nCLSwitches & CLSW_REGEXTVID) && !bAudioOnly) ||
                    ((m_s->nCLSwitches & CLSW_REGEXTAUD) && bAudioOnly) ||
                    ((m_s->nCLSwitches & CLSW_REGEXTPL) && bPlaylist)) {
                m_s->fileAssoc.Register(mf[i], true, false, true);
            }
        }

        SHChangeNotify(SHCNE_ASSOCCHANGED, SHCNF_IDLIST, nullptr, nullptr);

        return FALSE;
    }

    if (m_s->nCLSwitches & CLSW_UNREGEXT) { // unregistered file types
        CMediaFormats& mf = m_s->m_Formats;
        mf.UpdateData(false);

        for (size_t i = 0, cnt = mf.GetCount(); i < cnt; i++) {
            m_s->fileAssoc.Register(mf[i], false, false, false);
        }

        SHChangeNotify(SHCNE_ASSOCCHANGED, SHCNF_IDLIST, nullptr, nullptr);

        return FALSE;
    }

    if (m_s->nCLSwitches & CLSW_ICONSASSOC) {
        CMediaFormats& mf = m_s->m_Formats;
        mf.UpdateData(false);

        CAtlList<CString> registeredExts;
        m_s->fileAssoc.GetAssociatedExtensionsFromRegistry(registeredExts);

        m_s->fileAssoc.ReAssocIcons(registeredExts);

        SHChangeNotify(SHCNE_ASSOCCHANGED, SHCNF_IDLIST, nullptr, nullptr);

        return FALSE;
    }

    // Enable to open options with administrator privilege (for Vista UAC)
    if (m_s->nCLSwitches & CLSW_ADMINOPTION) {
        m_s->LoadSettings(); // read all settings. long time but not critical at this point

        switch (m_s->iAdminOption) {
            case CPPageFormats::IDD: {
                CPPageSheet options(ResStr(IDS_OPTIONS_CAPTION), nullptr, nullptr, m_s->iAdminOption);
                options.LockPage();
                options.DoModal();
            }
            break;

            default:
                ASSERT(FALSE);
        }
        return FALSE;
    }

    if (m_s->nCLSwitches & (CLSW_CONFIGLAVSPLITTER | CLSW_CONFIGLAVAUDIO | CLSW_CONFIGLAVVIDEO)) {
        m_s->LoadSettings();
        if (m_s->nCLSwitches & CLSW_CONFIGLAVSPLITTER) {
            CFGFilterLAVSplitter::ShowPropertyPages(NULL);
        }
        if (m_s->nCLSwitches & CLSW_CONFIGLAVAUDIO) {
            CFGFilterLAVAudio::ShowPropertyPages(NULL);
        }
        if (m_s->nCLSwitches & CLSW_CONFIGLAVVIDEO) {
            CFGFilterLAVVideo::ShowPropertyPages(NULL);
        }
        return FALSE;
    }

    m_mutexOneInstance.Create(nullptr, TRUE, MPC_WND_CLASS_NAME);

    if (GetLastError() == ERROR_ALREADY_EXISTS) {
        if ((m_s->nCLSwitches & CLSW_ADD) || !(m_s->GetAllowMultiInst() || m_s->nCLSwitches & CLSW_NEW || m_cmdln.IsEmpty())) {
            DWORD res = WaitForSingleObject(m_mutexOneInstance.m_h, 5000);
            if (res == WAIT_OBJECT_0 || res == WAIT_ABANDONED) {
                HWND hWnd = ::FindWindow(MPC_WND_CLASS_NAME, nullptr);
                if (hWnd) {
                    DWORD dwProcessId = 0;
                    if (GetWindowThreadProcessId(hWnd, &dwProcessId) && dwProcessId) {
                        VERIFY(AllowSetForegroundWindow(dwProcessId));
                    } else {
                        ASSERT(FALSE);
                    }
                    if (!(m_s->nCLSwitches & CLSW_MINIMIZED) && IsIconic(hWnd) &&
                        (!(m_s->nCLSwitches & CLSW_ADD) || m_s->nCLSwitches & CLSW_PLAY) //do not restore when adding to playlist of minimized player, unless also playing
                        ) {
                        ShowWindow(hWnd, SW_RESTORE);
                    }
                    if (SendCommandLine(hWnd)) {
                        m_mutexOneInstance.Close();
                        return FALSE;
                    }
                }
            }
            if ((m_s->nCLSwitches & CLSW_ADD)) {
                ASSERT(FALSE);
                return FALSE; // don't open new instance if SendCommandLine() failed
            }
        }
    }

    if (!IsIniValid()) {
        CRegKey key;
        if (ERROR_SUCCESS == key.Create(HKEY_CURRENT_USER, _T("Software\\MPC-HC\\MPC-HC"))) {
            if (RegQueryValueEx(key, _T("ExePath"), 0, nullptr, nullptr, nullptr) != ERROR_SUCCESS) { // First launch
                // Move registry settings from the old to the new location
                CRegKey oldKey;
                if (ERROR_SUCCESS == oldKey.Open(HKEY_CURRENT_USER, _T("Software\\Gabest\\Media Player Classic"), KEY_READ)) {
                    SHCopyKey(oldKey, _T(""), key, 0);
                }
            }

            key.SetStringValue(_T("ExePath"), PathUtils::GetProgramPath(true));
        }
    }

    m_s->UpdateSettings(); // update settings
    m_s->LoadSettings(); // read settings

    #if !defined(_DEBUG) && USE_DRDUMP_CRASH_REPORTER
    if (!m_s->bEnableCrashReporter) {
        DisableCrashReporter();
    }
    #endif

    m_AudioRendererDisplayName_CL = _T("");

    if (!__super::InitInstance()) {
        MessageBoxW(nullptr, L"MPC-HC encountered a problem during initialization", L"MPC-HC", MB_ICONERROR | MB_OK);
        return FALSE;
    }

    AfxEnableControlContainer();

    CMainFrame* pFrame;
    try {
        pFrame = DEBUG_NEW CMainFrame;
        if (!pFrame || !pFrame->LoadFrame(IDR_MAINFRAME, WS_OVERLAPPEDWINDOW | FWS_ADDTOTITLE, nullptr, nullptr)) {
            MessageBox(nullptr, L"MPC-HC encountered a problem during initialization", L"MPC-HC", MB_ICONERROR | MB_OK);
            return FALSE;
        }
    } catch (...) {
        MessageBoxW(nullptr, L"MPC-HC encountered a problem during initialization", L"MPC-HC", MB_ICONERROR | MB_OK);
        return FALSE;
    }

    m_pMainWnd = pFrame;
    if (!m_pMainWnd) {
        MessageBoxW(nullptr, L"MPC-HC encountered a problem during initialization", L"MPC-HC", MB_ICONERROR | MB_OK);
        return FALSE;
    }

    pFrame->m_controls.LoadState();
    CPoint borderAdjustDirection;
    pFrame->SetDefaultWindowRect((m_s->nCLSwitches & CLSW_MONITOR) ? m_s->iMonitor : 0);
    if (!m_s->slFiles.IsEmpty()) {
        pFrame->m_controls.DelayShowNotLoaded(true);
    }
    pFrame->SetDefaultFullscreenState();
    pFrame->UpdateControlState(CMainFrame::UPDATE_CONTROLS_VISIBILITY);
    pFrame->SetIcon(icon, TRUE);

    bool bRestoreLastWindowType = (m_s->fRememberWindowSize || m_s->fRememberWindowPos) && !m_s->fLastFullScreen && !m_s->fLaunchfullscreen;
    bool bMinimized = (m_s->nCLSwitches & CLSW_MINIMIZED) || (bRestoreLastWindowType && m_s->nLastWindowType == SIZE_MINIMIZED);
    bool bMaximized = bRestoreLastWindowType && m_s->nLastWindowType == SIZE_MAXIMIZED;

    if (bMinimized) {
        m_nCmdShow = (m_s->nCLSwitches & CLSW_NOFOCUS) ? SW_SHOWMINNOACTIVE : SW_SHOWMINIMIZED;
    } else if (bMaximized) {
        // Show maximized without focus is not supported nor make sense.
        m_nCmdShow = (m_s->nCLSwitches & CLSW_NOFOCUS) ? SW_SHOWNOACTIVATE : SW_SHOWMAXIMIZED;
    } else {
        m_nCmdShow = (m_s->nCLSwitches & CLSW_NOFOCUS) ? SW_SHOWNOACTIVATE : SW_SHOWNORMAL;
    }

    pFrame->ActivateFrame(m_nCmdShow);

    if (AfxGetAppSettings().HasFixedWindowSize() && IsWindows8OrGreater()) {//make adjustments for drop shadow frame
        CRect rect, frame;
        pFrame->GetWindowRect(&rect);
        CRect diff = pFrame->GetInvisibleBorderSize();
        if (!diff.IsRectNull()) {
            rect.InflateRect(diff);
            pFrame->SetWindowPos(nullptr, rect.left, rect.top, rect.Width(), rect.Height(), SWP_NOZORDER | SWP_NOACTIVATE);
        }
    }

    /* adipose 2019-11-12:
        LoadPlayList this used to be performed inside OnCreate,
        but due to all toolbars being hidden, EnsureVisible does not correctly
        scroll to the current file in the playlist.  We call after activating
        the frame to fix this issue.
    */
    pFrame->m_wndPlaylistBar.LoadPlaylist(pFrame->GetRecentFile());

    pFrame->UpdateWindow();


    if (bMinimized && bMaximized) {
        WINDOWPLACEMENT wp;
        GetWindowPlacement(*pFrame, &wp);
        wp.flags = WPF_RESTORETOMAXIMIZED;
        SetWindowPlacement(*pFrame, &wp);
    }

    pFrame->m_hAccelTable = m_s->hAccel;
    m_s->WinLircClient.SetHWND(m_pMainWnd->m_hWnd);
    if (m_s->fWinLirc) {
        m_s->WinLircClient.Connect(m_s->strWinLircAddr);
    }
    m_s->UIceClient.SetHWND(m_pMainWnd->m_hWnd);
    if (m_s->fUIce) {
        m_s->UIceClient.Connect(m_s->strUIceAddr);
    }

    if (UpdateChecker::IsAutoUpdateEnabled()) {
        UpdateChecker::CheckForUpdate(true);
    }

    SendCommandLine(m_pMainWnd->m_hWnd);
    RegisterHotkeys();

    // set HIGH I/O Priority for better playback performance
    if (m_hNTDLL) {
        typedef NTSTATUS(WINAPI * FUNC_NTSETINFORMATIONPROCESS)(HANDLE, ULONG, PVOID, ULONG);
        FUNC_NTSETINFORMATIONPROCESS NtSetInformationProcess = (FUNC_NTSETINFORMATIONPROCESS)GetProcAddress(m_hNTDLL, "NtSetInformationProcess");

        if (NtSetInformationProcess && SetPrivilege(SE_INC_BASE_PRIORITY_NAME)) {
            ULONG IoPriority = 3;
            ULONG ProcessIoPriority = 0x21;
            NTSTATUS NtStatus = NtSetInformationProcess(GetCurrentProcess(), ProcessIoPriority, &IoPriority, sizeof(ULONG));
            TRACE(_T("Set I/O Priority - %d\n"), NtStatus);
            UNREFERENCED_PARAMETER(NtStatus);
        }
    }

    m_mutexOneInstance.Release();

    CWebServer::Init();

    if (m_s->fAssociatedWithIcons) {
        m_s->fileAssoc.CheckIconsAssoc();
    }

    return TRUE;
}

UINT CMPlayerCApp::GetRemoteControlCodeMicrosoft(UINT nInputcode, HRAWINPUT hRawInput)
{
    UINT dwSize = 0;
    UINT nMceCmd = 0;

    // Support for MCE remote control
    UINT ret = GetRawInputData(hRawInput, RID_INPUT, nullptr, &dwSize, sizeof(RAWINPUTHEADER));
    if (ret == 0 && dwSize > 0) {
        BYTE* pRawBuffer = DEBUG_NEW BYTE[dwSize];
        if (GetRawInputData(hRawInput, RID_INPUT, pRawBuffer, &dwSize, sizeof(RAWINPUTHEADER)) != -1) {
            RAWINPUT* raw = (RAWINPUT*)pRawBuffer;
            if (raw->header.dwType == RIM_TYPEHID && raw->data.hid.dwSizeHid >= 3) {
                nMceCmd = 0x10000 + (raw->data.hid.bRawData[1] | raw->data.hid.bRawData[2] << 8);
            }
        }
        delete [] pRawBuffer;
    }

    return nMceCmd;
}

UINT CMPlayerCApp::GetRemoteControlCodeSRM7500(UINT nInputcode, HRAWINPUT hRawInput)
{
    UINT dwSize = 0;
    UINT nMceCmd = 0;

    UINT ret = GetRawInputData(hRawInput, RID_INPUT, nullptr, &dwSize, sizeof(RAWINPUTHEADER));
    if (ret == 0 && dwSize > 21) {
        BYTE* pRawBuffer = DEBUG_NEW BYTE[dwSize];
        if (GetRawInputData(hRawInput, RID_INPUT, pRawBuffer, &dwSize, sizeof(RAWINPUTHEADER)) != -1) {
            RAWINPUT* raw = (RAWINPUT*)pRawBuffer;

            // data.hid.bRawData[21] set to one when key is pressed
            if (raw->header.dwType == RIM_TYPEHID && raw->data.hid.dwSizeHid >= 22 && raw->data.hid.bRawData[21] == 1) {
                // data.hid.bRawData[21] has keycode
                switch (raw->data.hid.bRawData[20]) {
                    case 0x0033:
                        nMceCmd = MCE_DETAILS;
                        break;
                    case 0x0022:
                        nMceCmd = MCE_GUIDE;
                        break;
                    case 0x0036:
                        nMceCmd = MCE_MYTV;
                        break;
                    case 0x0026:
                        nMceCmd = MCE_RECORDEDTV;
                        break;
                    case 0x0005:
                        nMceCmd = MCE_RED;
                        break;
                    case 0x0002:
                        nMceCmd = MCE_GREEN;
                        break;
                    case 0x0045:
                        nMceCmd = MCE_YELLOW;
                        break;
                    case 0x0046:
                        nMceCmd = MCE_BLUE;
                        break;
                    case 0x000A:
                        nMceCmd = MCE_MEDIA_PREVIOUSTRACK;
                        break;
                    case 0x004A:
                        nMceCmd = MCE_MEDIA_NEXTTRACK;
                        break;
                }
            }
        }
        delete [] pRawBuffer;
    }

    return nMceCmd;
}

void CMPlayerCApp::RegisterHotkeys()
{
    CAutoVectorPtr<RAWINPUTDEVICELIST> inputDeviceList;
    UINT nInputDeviceCount = 0, nErrCode;
    RID_DEVICE_INFO deviceInfo;
    RAWINPUTDEVICE MCEInputDevice[] = {
        // usUsagePage     usUsage         dwFlags     hwndTarget
        {  0xFFBC,         0x88,           0,          nullptr},
        {  0x000C,         0x01,           0,          nullptr},
        {  0x000C,         0x80,           0,          nullptr}
    };

    // Register MCE Remote Control raw input
    for (unsigned int i = 0; i < _countof(MCEInputDevice); i++) {
        MCEInputDevice[i].hwndTarget = m_pMainWnd->m_hWnd;
    }

    // Get the size of the device list
    nErrCode = GetRawInputDeviceList(nullptr, &nInputDeviceCount, sizeof(RAWINPUTDEVICELIST));
    inputDeviceList.Attach(DEBUG_NEW RAWINPUTDEVICELIST[nInputDeviceCount]);
    if (nErrCode == UINT(-1) || !nInputDeviceCount || !inputDeviceList) {
        ASSERT(nErrCode != UINT(-1));
        return;
    }

    nErrCode = GetRawInputDeviceList(inputDeviceList, &nInputDeviceCount, sizeof(RAWINPUTDEVICELIST));
    if (nErrCode == UINT(-1)) {
        ASSERT(FALSE);
        return;
    }

    for (UINT i = 0; i < nInputDeviceCount; i++) {
        UINT nTemp = deviceInfo.cbSize = sizeof(deviceInfo);

        if (GetRawInputDeviceInfo(inputDeviceList[i].hDevice, RIDI_DEVICEINFO, &deviceInfo, &nTemp) > 0) {
            if (deviceInfo.hid.dwVendorId == 0x00000471 &&         // Philips HID vendor id
                    deviceInfo.hid.dwProductId == 0x00000617) {    // IEEE802.15.4 RF Dongle (SRM 7500)
                MCEInputDevice[0].usUsagePage = deviceInfo.hid.usUsagePage;
                MCEInputDevice[0].usUsage = deviceInfo.hid.usUsage;
                GetRemoteControlCode = GetRemoteControlCodeSRM7500;
            }
        }
    }

    RegisterRawInputDevices(MCEInputDevice, _countof(MCEInputDevice), sizeof(RAWINPUTDEVICE));

    if (m_s->fGlobalMedia) {
        POSITION pos = m_s->wmcmds.GetHeadPosition();
        while (pos) {
            const wmcmd& wc = m_s->wmcmds.GetNext(pos);
            if (wc.appcmd != 0) {
                UINT vkappcmd = GetVKFromAppCommand(wc.appcmd);
                if (vkappcmd > 0) {
                    RegisterHotKey(m_pMainWnd->m_hWnd, wc.appcmd, 0, vkappcmd);
                }
            }
        }
    }
}

void CMPlayerCApp::UnregisterHotkeys()
{
    if (m_s->fGlobalMedia) {
        POSITION pos = m_s->wmcmds.GetHeadPosition();
        while (pos) {
            const wmcmd& wc = m_s->wmcmds.GetNext(pos);
            if (wc.appcmd != 0) {
                UnregisterHotKey(m_pMainWnd->m_hWnd, wc.appcmd);
            }
        }
    }
}

UINT CMPlayerCApp::GetVKFromAppCommand(UINT nAppCommand)
{
    // Note: Only a subset of AppCommands have a VirtualKey
    switch (nAppCommand) {
        case APPCOMMAND_MEDIA_PLAY_PAUSE:
            return VK_MEDIA_PLAY_PAUSE;
        case APPCOMMAND_MEDIA_STOP:
            return VK_MEDIA_STOP;
        case APPCOMMAND_MEDIA_NEXTTRACK:
            return VK_MEDIA_NEXT_TRACK;
        case APPCOMMAND_MEDIA_PREVIOUSTRACK:
            return VK_MEDIA_PREV_TRACK;
        case APPCOMMAND_VOLUME_DOWN:
            return VK_VOLUME_DOWN;
        case APPCOMMAND_VOLUME_UP:
            return VK_VOLUME_UP;
        case APPCOMMAND_VOLUME_MUTE:
            return VK_VOLUME_MUTE;
        case APPCOMMAND_LAUNCH_MEDIA_SELECT:
            return VK_LAUNCH_MEDIA_SELECT;
        case APPCOMMAND_BROWSER_BACKWARD:
            return VK_BROWSER_BACK;
        case APPCOMMAND_BROWSER_FORWARD:
            return VK_BROWSER_FORWARD;
        case APPCOMMAND_BROWSER_REFRESH:
            return VK_BROWSER_REFRESH;
        case APPCOMMAND_BROWSER_STOP:
            return VK_BROWSER_STOP;
        case APPCOMMAND_BROWSER_SEARCH:
            return VK_BROWSER_SEARCH;
        case APPCOMMAND_BROWSER_FAVORITES:
            return VK_BROWSER_FAVORITES;
        case APPCOMMAND_BROWSER_HOME:
            return VK_BROWSER_HOME;
        case APPCOMMAND_LAUNCH_APP1:
            return VK_LAUNCH_APP1;
        case APPCOMMAND_LAUNCH_APP2:
            return VK_LAUNCH_APP2;
    }

    return 0;
}

int CMPlayerCApp::ExitInstance()
{
    // We might be exiting before m_s is initialized.
    if (m_s) {
        m_s->SaveSettings();
        m_s = nullptr;
    }

    CMPCPngImage::CleanUp();

    MH_Uninitialize();

    OleUninitialize();

    return CWinAppEx::ExitInstance();
}

BOOL CMPlayerCApp::SaveAllModified()
{
    // CWinApp::SaveAllModified
    // Called by the framework to save all documents
    // when the application's main frame window is to be closed,
    // or through a WM_QUERYENDSESSION message.
    if (m_s && !m_fClosingState) {
        if (auto pMainFrame = AfxFindMainFrame()) {
            if (pMainFrame->GetLoadState() != MLS::CLOSED) {
                pMainFrame->CloseMedia();
            }
        }
    }

    return TRUE;
}

// CMPlayerCApp message handlers

BEGIN_MESSAGE_MAP(CMPlayerCApp, CWinAppEx)
    ON_COMMAND(ID_HELP_ABOUT, OnAppAbout)
    ON_COMMAND(ID_FILE_EXIT, OnFileExit)
    ON_COMMAND(ID_HELP_SHOWCOMMANDLINESWITCHES, OnHelpShowcommandlineswitches)
END_MESSAGE_MAP()

void CMPlayerCApp::OnAppAbout()
{
    CAboutDlg aboutDlg;
    aboutDlg.DoModal();
}

void CMPlayerCApp::SetClosingState()
{
    m_fClosingState = true;
#if USE_DRDUMP_CRASH_REPORTER & ((MPC_VERSION_REV < 10) || MPC_VERSION_PATCH == 1)
    DisableCrashReporter();
#endif
}

void CMPlayerCApp::OnFileExit()
{
    OnAppExit();
}

void CMPlayerCApp::OnHelpShowcommandlineswitches()
{
    ShowCmdlnSwitches();
}

// CRemoteCtrlClient

CRemoteCtrlClient::CRemoteCtrlClient()
    : m_pWnd(nullptr)
    , m_nStatus(DISCONNECTED)
{
}

void CRemoteCtrlClient::SetHWND(HWND hWnd)
{
    CAutoLock cAutoLock(&m_csLock);

    m_pWnd = CWnd::FromHandle(hWnd);
}

void CRemoteCtrlClient::Connect(CString addr)
{
    CAutoLock cAutoLock(&m_csLock);

    if (m_nStatus == CONNECTING && m_addr == addr) {
        TRACE(_T("CRemoteCtrlClient (Connect): already connecting to %s\n"), addr.GetString());
        return;
    }

    if (m_nStatus == CONNECTED && m_addr == addr) {
        TRACE(_T("CRemoteCtrlClient (Connect): already connected to %s\n"), addr.GetString());
        return;
    }

    m_nStatus = CONNECTING;

    TRACE(_T("CRemoteCtrlClient (Connect): connecting to %s\n"), addr.GetString());

    Close();

    Create();

    CString ip = addr.Left(addr.Find(':') + 1).TrimRight(':');
    int port = _tcstol(addr.Mid(addr.Find(':') + 1), nullptr, 10);

    __super::Connect(ip, port);

    m_addr = addr;
}

void CRemoteCtrlClient::DisConnect()
{
    CAutoLock cAutoLock(&m_csLock);

    ShutDown(2);
    Close();
}

void CRemoteCtrlClient::OnConnect(int nErrorCode)
{
    CAutoLock cAutoLock(&m_csLock);

    m_nStatus = (nErrorCode == 0 ? CONNECTED : DISCONNECTED);

    TRACE(_T("CRemoteCtrlClient (OnConnect): %d\n"), nErrorCode);
}

void CRemoteCtrlClient::OnClose(int nErrorCode)
{
    CAutoLock cAutoLock(&m_csLock);

    if (m_hSocket != INVALID_SOCKET && m_nStatus == CONNECTED) {
        TRACE(_T("CRemoteCtrlClient (OnClose): connection lost\n"));
    }

    m_nStatus = DISCONNECTED;

    TRACE(_T("CRemoteCtrlClient (OnClose): %d\n"), nErrorCode);
}

void CRemoteCtrlClient::OnReceive(int nErrorCode)
{
    if (nErrorCode != 0 || !m_pWnd) {
        return;
    }

    CStringA str;
    int ret = Receive(str.GetBuffer(256), 255, 0);
    if (ret <= 0) {
        return;
    }
    str.ReleaseBuffer(ret);

    TRACE(_T("CRemoteCtrlClient (OnReceive): %S\n"), str.GetString());

    OnCommand(str);

    __super::OnReceive(nErrorCode);
}

void CRemoteCtrlClient::ExecuteCommand(CStringA cmd, int repcnt)
{
    cmd.Trim();
    if (cmd.IsEmpty()) {
        return;
    }
    cmd.Replace(' ', '_');

    const CAppSettings& s = AfxGetAppSettings();

    POSITION pos = s.wmcmds.GetHeadPosition();
    while (pos) {
        const wmcmd& wc = s.wmcmds.GetNext(pos);
        if ((repcnt == 0 && wc.rmrepcnt == 0 || wc.rmrepcnt > 0 && (repcnt % wc.rmrepcnt) == 0)
                && (wc.rmcmd.CompareNoCase(cmd) == 0 || wc.cmd == (WORD)strtol(cmd, nullptr, 10))) {
            CAutoLock cAutoLock(&m_csLock);
            TRACE(_T("CRemoteCtrlClient (calling command): %s\n"), wc.GetName().GetString());
            m_pWnd->SendMessage(WM_COMMAND, wc.cmd);
            break;
        }
    }
}

// CWinLircClient

CWinLircClient::CWinLircClient()
{
}

void CWinLircClient::OnCommand(CStringA str)
{
    TRACE(_T("CWinLircClient (OnCommand): %S\n"), str.GetString());

    int i = 0, j = 0, repcnt = 0;
    for (CStringA token = str.Tokenize(" ", i);
            !token.IsEmpty();
            token = str.Tokenize(" ", i), j++) {
        if (j == 1) {
            repcnt = strtol(token, nullptr, 16);
        } else if (j == 2) {
            ExecuteCommand(token, repcnt);
        }
    }
}

// CUIceClient

CUIceClient::CUIceClient()
{
}

void CUIceClient::OnCommand(CStringA str)
{
    TRACE(_T("CUIceClient (OnCommand): %S\n"), str.GetString());

    CStringA cmd;
    int i = 0, j = 0;
    for (CStringA token = str.Tokenize("|", i);
            !token.IsEmpty();
            token = str.Tokenize("|", i), j++) {
        if (j == 0) {
            cmd = token;
        } else if (j == 1) {
            ExecuteCommand(cmd, strtol(token, nullptr, 16));
        }
    }
}

// CMPlayerCApp continuation

COLORPROPERTY_RANGE* CMPlayerCApp::GetColorControl(ControlType nFlag)
{
    switch (nFlag) {
        case ProcAmp_Brightness:
            return &m_ColorControl[0];
        case ProcAmp_Contrast:
            return &m_ColorControl[1];
        case ProcAmp_Hue:
            return &m_ColorControl[2];
        case ProcAmp_Saturation:
            return &m_ColorControl[3];
    }
    return nullptr;
}

void CMPlayerCApp::ResetColorControlRange()
{
    m_ColorControl[0].dwProperty   = ProcAmp_Brightness;
    m_ColorControl[0].MinValue     = -100;
    m_ColorControl[0].MaxValue     = 100;
    m_ColorControl[0].DefaultValue = 0;
    m_ColorControl[0].StepSize     = 1;
    m_ColorControl[1].dwProperty   = ProcAmp_Contrast;
    m_ColorControl[1].MinValue     = -100;
    m_ColorControl[1].MaxValue     = 100;
    m_ColorControl[1].DefaultValue = 0;
    m_ColorControl[1].StepSize     = 1;
    m_ColorControl[2].dwProperty   = ProcAmp_Hue;
    m_ColorControl[2].MinValue     = -180;
    m_ColorControl[2].MaxValue     = 180;
    m_ColorControl[2].DefaultValue = 0;
    m_ColorControl[2].StepSize     = 1;
    m_ColorControl[3].dwProperty   = ProcAmp_Saturation;
    m_ColorControl[3].MinValue     = -100;
    m_ColorControl[3].MaxValue     = 100;
    m_ColorControl[3].DefaultValue = 0;
    m_ColorControl[3].StepSize     = 1;
}

void CMPlayerCApp::UpdateColorControlRange(bool isEVR)
{
    if (isEVR) {
        // Brightness
        m_ColorControl[0].MinValue      = FixedToInt(m_EVRColorControl[0].MinValue);
        m_ColorControl[0].MaxValue      = FixedToInt(m_EVRColorControl[0].MaxValue);
        m_ColorControl[0].DefaultValue  = FixedToInt(m_EVRColorControl[0].DefaultValue);
        m_ColorControl[0].StepSize      = std::max(1, FixedToInt(m_EVRColorControl[0].StepSize));
        // Contrast
        m_ColorControl[1].MinValue      = FixedToInt(m_EVRColorControl[1].MinValue, 100) - 100;
        m_ColorControl[1].MaxValue      = FixedToInt(m_EVRColorControl[1].MaxValue, 100) - 100;
        m_ColorControl[1].DefaultValue  = FixedToInt(m_EVRColorControl[1].DefaultValue, 100) - 100;
        m_ColorControl[1].StepSize      = std::max(1, FixedToInt(m_EVRColorControl[1].StepSize, 100));
        // Hue
        m_ColorControl[2].MinValue      = FixedToInt(m_EVRColorControl[2].MinValue);
        m_ColorControl[2].MaxValue      = FixedToInt(m_EVRColorControl[2].MaxValue);
        m_ColorControl[2].DefaultValue  = FixedToInt(m_EVRColorControl[2].DefaultValue);
        m_ColorControl[2].StepSize      = std::max(1, FixedToInt(m_EVRColorControl[2].StepSize));
        // Saturation
        m_ColorControl[3].MinValue      = FixedToInt(m_EVRColorControl[3].MinValue, 100) - 100;
        m_ColorControl[3].MaxValue      = FixedToInt(m_EVRColorControl[3].MaxValue, 100) - 100;
        m_ColorControl[3].DefaultValue  = FixedToInt(m_EVRColorControl[3].DefaultValue, 100) - 100;
        m_ColorControl[3].StepSize      = std::max(1, FixedToInt(m_EVRColorControl[3].StepSize, 100));
    } else {
        // Brightness
        m_ColorControl[0].MinValue      = (int)floor(m_VMR9ColorControl[0].MinValue + 0.5);
        m_ColorControl[0].MaxValue      = (int)floor(m_VMR9ColorControl[0].MaxValue + 0.5);
        m_ColorControl[0].DefaultValue  = (int)floor(m_VMR9ColorControl[0].DefaultValue + 0.5);
        m_ColorControl[0].StepSize      = std::max(1, (int)(m_VMR9ColorControl[0].StepSize + 0.5));
        // Contrast
        if (*(int*)&m_VMR9ColorControl[1].MinValue == 1036830720) {
            m_VMR9ColorControl[1].MinValue = 0.11f;    //fix NVIDIA bug
        }
        m_ColorControl[1].MinValue      = (int)floor(m_VMR9ColorControl[1].MinValue * 100 + 0.5) - 100;
        m_ColorControl[1].MaxValue      = (int)floor(m_VMR9ColorControl[1].MaxValue * 100 + 0.5) - 100;
        m_ColorControl[1].DefaultValue  = (int)floor(m_VMR9ColorControl[1].DefaultValue * 100 + 0.5) - 100;
        m_ColorControl[1].StepSize      = std::max(1, (int)(m_VMR9ColorControl[1].StepSize * 100 + 0.5));
        // Hue
        m_ColorControl[2].MinValue      = (int)floor(m_VMR9ColorControl[2].MinValue + 0.5);
        m_ColorControl[2].MaxValue      = (int)floor(m_VMR9ColorControl[2].MaxValue + 0.5);
        m_ColorControl[2].DefaultValue  = (int)floor(m_VMR9ColorControl[2].DefaultValue + 0.5);
        m_ColorControl[2].StepSize      = std::max(1, (int)(m_VMR9ColorControl[2].StepSize + 0.5));
        // Saturation
        m_ColorControl[3].MinValue      = (int)floor(m_VMR9ColorControl[3].MinValue * 100 + 0.5) - 100;
        m_ColorControl[3].MaxValue      = (int)floor(m_VMR9ColorControl[3].MaxValue * 100 + 0.5) - 100;
        m_ColorControl[3].DefaultValue  = (int)floor(m_VMR9ColorControl[3].DefaultValue * 100 + 0.5) - 100;
        m_ColorControl[3].StepSize      = std::max(1, (int)(m_VMR9ColorControl[3].StepSize * 100 + 0.5));
    }

    // Brightness
    if (m_ColorControl[0].MinValue < -100) {
        m_ColorControl[0].MinValue = -100;
    }
    if (m_ColorControl[0].MaxValue > 100) {
        m_ColorControl[0].MaxValue = 100;
    }
    // Contrast
    if (m_ColorControl[1].MinValue == m_ColorControl[1].MaxValue) { // when ProcAmp is unsupported
        m_ColorControl[1].MinValue = m_ColorControl[1].MaxValue = m_ColorControl[1].DefaultValue = 0;
    }
    if (m_ColorControl[1].MinValue < -100) {
        m_ColorControl[1].MinValue = -100;
    }
    if (m_ColorControl[1].MaxValue > 100) {
        m_ColorControl[1].MaxValue = 100;
    }
    // Hue
    if (m_ColorControl[2].MinValue < -180) {
        m_ColorControl[2].MinValue = -180;
    }
    if (m_ColorControl[2].MaxValue > 180) {
        m_ColorControl[2].MaxValue = 180;
    }
    // Saturation
    if (m_ColorControl[3].MinValue == m_ColorControl[3].MaxValue) { // when ProcAmp is unsupported
        m_ColorControl[3].MinValue = m_ColorControl[3].MaxValue = m_ColorControl[3].DefaultValue = 0;
    }
    if (m_ColorControl[3].MinValue < -100) {
        m_ColorControl[3].MinValue = -100;
    }
    if (m_ColorControl[3].MaxValue > 100) {
        m_ColorControl[3].MaxValue = 100;
    }
}

VMR9ProcAmpControlRange* CMPlayerCApp::GetVMR9ColorControl(ControlType nFlag)
{
    switch (nFlag) {
        case ProcAmp_Brightness:
            return &m_VMR9ColorControl[0];
        case ProcAmp_Contrast:
            return &m_VMR9ColorControl[1];
        case ProcAmp_Hue:
            return &m_VMR9ColorControl[2];
        case ProcAmp_Saturation:
            return &m_VMR9ColorControl[3];
    }
    return nullptr;
}

DXVA2_ValueRange* CMPlayerCApp::GetEVRColorControl(ControlType nFlag)
{
    switch (nFlag) {
        case ProcAmp_Brightness:
            return &m_EVRColorControl[0];
        case ProcAmp_Contrast:
            return &m_EVRColorControl[1];
        case ProcAmp_Hue:
            return &m_EVRColorControl[2];
        case ProcAmp_Saturation:
            return &m_EVRColorControl[3];
    }
    return nullptr;
}

void CMPlayerCApp::RunAsAdministrator(LPCTSTR strCommand, LPCTSTR strArgs, bool bWaitProcess)
{
    SHELLEXECUTEINFO execinfo;
    ZeroMemory(&execinfo, sizeof(execinfo));
    execinfo.lpFile = strCommand;
    execinfo.cbSize = sizeof(execinfo);
    execinfo.lpVerb = _T("runas");
    execinfo.fMask  = SEE_MASK_NOCLOSEPROCESS;
    execinfo.nShow  = SW_SHOWDEFAULT;
    execinfo.lpParameters = strArgs;

    ShellExecuteEx(&execinfo);

    if (bWaitProcess) {
        WaitForSingleObject(execinfo.hProcess, INFINITE);
    }
}

