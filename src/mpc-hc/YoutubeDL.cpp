/*
* (C) 2018 Nicholas Parkanyi
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
#include "YoutubeDL.h"
#include "rapidjson/include/rapidjson/document.h"
#include "mplayerc.h"
#include "logger.h"

struct CUtf16JSON {
    rapidjson::GenericDocument<rapidjson::UTF16<>> d;
};

CString GetYDLExePath(bool* is_ytdlp) {
    auto& s = AfxGetAppSettings();
    CString ydlpath;
    *is_ytdlp = true;
    if (s.sYDLExePath.IsEmpty()) {
        CString appdir = PathUtils::GetProgramPath(false);
        if (CPath(appdir + _T("\\yt-dlp.exe")).FileExists()) {
            ydlpath = _T("yt-dlp.exe");
        } else if (CPath(appdir + _T("\\youtube-dl.exe")).FileExists()) {
            ydlpath = _T("youtube-dl.exe");
            *is_ytdlp = false;
        } else {
            ydlpath = _T("yt-dlp.exe");
        }
    } else {
        ydlpath = s.sYDLExePath;
        // expand environment variables
        if (ydlpath.Find(_T('%')) >= 0) {
            wchar_t expanded_buf[MAX_PATH] = { 0 };
            DWORD req = ExpandEnvironmentStrings(ydlpath, expanded_buf, MAX_PATH);
            if (req > 0 && req < MAX_PATH) {
                ydlpath = CString(expanded_buf);
            }
        }
        if (ydlpath.MakeLower().Find(_T("youtube-dl")) >= 0) {
            *is_ytdlp = false;
        }
    }
    return ydlpath;
}

CYoutubeDLInstance::CYoutubeDLInstance()
    : idx_out(0), idx_err(0),
      buf_out(nullptr), buf_err(nullptr),
      capacity_out(0), capacity_err(0),
      pJSON(new CUtf16JSON)
{
}

CYoutubeDLInstance::~CYoutubeDLInstance()
{
    std::free(buf_out);
    std::free(buf_err);
    delete pJSON;
}

bool CYoutubeDLInstance::Run(CString url)
{
    const size_t bufsize = 2000;  //2KB initial buffer size

    /////////////////////////////
    // Set up youtube-dl process
    /////////////////////////////

    PROCESS_INFORMATION proc_info;
    STARTUPINFO startup_info;
    SECURITY_ATTRIBUTES sec_attrib;
    auto& s = AfxGetAppSettings();

    YDL_LOG(L"%s", url);

    bool ytdlp = true;
    CString args = _T("\"") + GetYDLExePath(&ytdlp) + _T("\" -J --no-warnings");
    if (!s.sYDLSubsPreference.IsEmpty()) {
        args.Append(_T(" --all-subs --write-sub"));
        if (s.bUseAutomaticCaptions) args.Append(_T(" --write-auto-sub"));
    }
    if (url.Find(_T("list=")) > 0) {
        args.Append(_T(" --ignore-errors --no-playlist"));
    }
    args.Append(_T(" \"") + url + _T("\""));
    if (ytdlp) {
        WCHAR lpszTempPath[MAX_PATH] = { 0 };
        if (GetTempPathW(MAX_PATH, lpszTempPath)) {
            args.AppendFormat(_T(" -P temp:\"%s\""), lpszTempPath);
        }
    }

    ZeroMemory(&proc_info, sizeof(PROCESS_INFORMATION));
    ZeroMemory(&startup_info, sizeof(STARTUPINFO));

    //child process must inherit the handles
    sec_attrib.nLength = sizeof(SECURITY_ATTRIBUTES);
    sec_attrib.lpSecurityDescriptor = NULL;
    sec_attrib.bInheritHandle = true;

    if (!CreatePipe(&hStdout_r, &hStdout_w, &sec_attrib, bufsize)) {
        return false;
    }
    if (!CreatePipe(&hStderr_r, &hStderr_w, &sec_attrib, bufsize)) {
        return false;
    }

    startup_info.cb = sizeof(STARTUPINFO);
    startup_info.hStdOutput = hStdout_w;
    startup_info.hStdError = hStderr_w;
    startup_info.wShowWindow = SW_HIDE;
    startup_info.dwFlags |= STARTF_USESTDHANDLES | STARTF_USESHOWWINDOW;

    if (!CreateProcess(NULL, args.GetBuffer(), NULL, NULL, true, 0,
                       NULL, NULL, &startup_info, &proc_info)) {
        DWORD err = GetLastError();
        CString errmsg;
        errmsg.Format(_T("Failed to create process for yt-dlp/youtube-dl, error %lu"), err);
        YDL_LOG(errmsg);
        if (!s.sYDLExePath.IsEmpty()) {
            AfxMessageBox(errmsg + L"\n\nYour YDLExepath value in advanced settings might be incorrect.", MB_ICONERROR, 0);
        } else if (url.Find(L"youtube.com") > 0) {
            AfxMessageBox(errmsg + L"\n\nTo watch Youtube videos with MPC-HC you need to put \"yt-dlp.exe\" in the MPC-HC installation folder.\n\nhttps://github.com/yt-dlp/yt-dlp/releases", MB_ICONERROR, 0);
        }
        return false;
    }

    //we must close the parent process's write handles before calling ReadFile,
    // otherwise it will block forever.
    CloseHandle(hStdout_w);
    CloseHandle(hStderr_w);


    /////////////////////////////////////////////////////
    // Read in stdout and stderr through the pipe buffer
    /////////////////////////////////////////////////////

    buf_out = static_cast<char*>(std::malloc(bufsize));
    buf_err = static_cast<char*>(std::malloc(bufsize));
    capacity_out = bufsize;
    capacity_err = bufsize;

    HANDLE hThreadOut, hThreadErr;
    idx_out = 0;
    idx_err = 0;

    hThreadOut = CreateThread(NULL, 0, BuffOutThread, this, NULL, NULL);
    hThreadErr = CreateThread(NULL, 0, BuffErrThread, this, NULL, NULL);

    WaitForSingleObject(hThreadOut, INFINITE);
    WaitForSingleObject(hThreadErr, INFINITE);

    if (!buf_out || !buf_err) {
        throw std::bad_alloc();
    }

    //NULL-terminate the data
    char* tmp;
    if (idx_out == capacity_out) {
        tmp = static_cast<char*>(std::realloc(buf_out, capacity_out + 1));
        if (tmp) {
            buf_out = tmp;
        }
    }
    buf_out[idx_out] = '\0';

    if (idx_err == capacity_err) {
        tmp = static_cast<char*>(std::realloc(buf_err, capacity_err + 1));
        if (tmp) {
            buf_err = tmp;
        }
    }
    buf_err[idx_err] = '\0';

    DWORD exitcode;
    GetExitCodeProcess(proc_info.hProcess, &exitcode);

    CloseHandle(proc_info.hProcess);
    CloseHandle(proc_info.hThread);
    CloseHandle(hThreadOut);
    CloseHandle(hThreadErr);
    CloseHandle(hStdout_r);
    CloseHandle(hStderr_r);

    // parse output
    if (exitcode == 0 || exitcode == 1) {
        if (loadJSON()) {
            return true;
        }
    }

    if (exitcode) {
        CString err = buf_err;
        if (err.IsEmpty()) {
            if (exitcode == 0xC0000135) {
                err.Format(_T("An error occurred while running yt-dlp/youtube-dl\n\nYou probably forgot to install this required runtime:\nMicrosoft Visual C++ 2010 Service Pack 1 Redistributable Package (x86)"));
            } else {
                err.Format(_T("An error occurred while running yt-dlp/youtube-dl\n\nprocess exitcode = 0x%08x"), exitcode);
            }
        } else {
            if (err.Find(_T("ERROR: Unsupported URL")) >= 0) {
                // abort without showing error message
                return false;
            }
            err = _T("yt-dlp/youtube-dl error message:\n\n") + err;
        }
        AfxMessageBox(err, MB_ICONERROR, 0);
    }
    return false;
}

DWORD WINAPI CYoutubeDLInstance::BuffOutThread(void* ydl_inst)
{
    auto ydl = static_cast<CYoutubeDLInstance*>(ydl_inst);
    DWORD read;

    while (ReadFile(ydl->hStdout_r, ydl->buf_out + ydl->idx_out, ydl->capacity_out - ydl->idx_out, &read, NULL)) {
        ydl->idx_out += read;
        if (ydl->idx_out == ydl->capacity_out) {
            ydl->capacity_out *= 2;
            char* tmp = static_cast<char*>(std::realloc(ydl->buf_out, ydl->capacity_out));
            if (tmp) {
                ydl->buf_out = tmp;
            } else {
                std::free(ydl->buf_out);
                ydl->buf_out = nullptr;
                return 0;
            }
        }
    }

    return GetLastError() == ERROR_BROKEN_PIPE ? 0 : GetLastError();
}

DWORD WINAPI CYoutubeDLInstance::BuffErrThread(void* ydl_inst)
{
    auto ydl = static_cast<CYoutubeDLInstance*>(ydl_inst);
    DWORD read;

    while (ReadFile(ydl->hStderr_r, ydl->buf_err + ydl->idx_err, ydl->capacity_err - ydl->idx_err, &read, NULL)) {
        ydl->idx_err += read;
        if (ydl->idx_err == ydl->capacity_err) {
            ydl->capacity_err *= 2;
            char* tmp = static_cast<char*>(std::realloc(ydl->buf_err, ydl->capacity_err));
            if (tmp) {
                ydl->buf_err = tmp;
            } else {
                std::free(ydl->buf_err);
                ydl->buf_err = nullptr;
                return 0;
            }
        }
    }

    return GetLastError() == ERROR_BROKEN_PIPE ? 0 : GetLastError();
}

struct YDLStreamDetails {
    CString protocol;
    CString url;
    int width  = 0;
    int height = 0;
    CString vcodec;
    CString acodec;
    CString format;
    bool has_video = false;
    bool has_audio = false;
    int vbr = 0;
    int abr = 0;
    int fps = 0;
    CString format_id;
    CString format_note;
    CString language;
    int lang_pref = 0;
    int video_score = 0;
    int audio_score = 0;
    CString useragent;
};

#define YDL_EXTRA_LOGGING 0
#define YDL_LOG_URLS      1
#define YDL_TRACE         0

#define YDL_FORMAT_AUTO      0
#define YDL_FORMAT_H264_30   1
#define YDL_FORMAT_H264_60   2
#define YDL_FORMAT_VP9_30    3
#define YDL_FORMAT_VP9_60    4
#define YDL_FORMAT_VP9P2_30  5
#define YDL_FORMAT_VP9P2_60  6
#define YDL_FORMAT_AV1_30    7
#define YDL_FORMAT_AV1_60    8

#define YDL_FORMAT_AAC       1
#define YDL_FORMAT_OPUS      2

/* Give score based on the following criteria in order or importance:
 * 1: Within required resolution bounds
 * 2: Match preferred format
 * 3: Match preferred fps
 */
void GetVideoScore(YDLStreamDetails& details) {
    int score = 1;
    auto& s = AfxGetAppSettings();

    CString vcodec4 = details.vcodec.Left(4);
    CString vcodec7 = details.vcodec.Left(7);

    if (vcodec7 != _T("unknown")) {
        score += 1;
    }

    if (s.iYDLMaxHeight > 0) {
        if (details.height > details.width) {
            // vertical video
            if (s.iYDLMaxHeight >= details.width) {
                score += 64;
            }
        } else if (s.iYDLMaxHeight >= details.height) {
            score += 64;
        }
    }

    switch (s.iYDLVideoFormat) {
        case YDL_FORMAT_H264_30:
            if (vcodec4 == _T("avc1")) score += 32;
            if (details.fps < 31) score += 8;
            break;
        case YDL_FORMAT_H264_60:
            if (vcodec4 == _T("avc1")) score += 32;
            if (details.fps >= 31) score += 8;
            break;
        case YDL_FORMAT_VP9_30:
            if (vcodec4 == _T("vp09") || vcodec4 == _T("vp9") || vcodec7 == _T("vp09.00")) score += 32;
            else if (vcodec4 == _T("vp9.") || vcodec7 == _T("vp09.02")) score += 16;
            if (details.fps < 31) score += 8;
            break;
        case YDL_FORMAT_VP9_60:
            if (vcodec4 == _T("vp09") || vcodec4 == _T("vp9") || vcodec7 == _T("vp09.00")) score += 32;
            else if (vcodec4 == _T("vp9.") || vcodec7 == _T("vp09.02")) score += 16;
            if (details.fps >= 31) score += 8;
            break;
        case YDL_FORMAT_VP9P2_30:
            if (vcodec4 == _T("vp09") || vcodec4 == _T("vp9") || vcodec7 == _T("vp09.02")) score += 32;
            else if (vcodec4 == _T("vp9.") || vcodec7 == _T("vp09.00")) score += 16;
            if (details.fps < 31) score += 8;
            break;
        case YDL_FORMAT_VP9P2_60:
            if (vcodec4 == _T("vp09") || vcodec4 == _T("vp9") || vcodec7 == _T("vp09.02")) score += 32;
            else if (vcodec4 == _T("vp9.") || vcodec7 == _T("vp09.00")) score += 16;
            if (details.fps >= 31) score += 8;
            break;
        case YDL_FORMAT_AV1_30:
            if (vcodec4 == _T("av01")) score += 32;
            if (details.fps < 31) score += 8;
            break;
        case YDL_FORMAT_AV1_60:
            if (vcodec4 == _T("av01")) score += 32;
            if (details.fps >= 31) score += 8;
            break;
    }

    details.video_score = score;
}

/* Give score based on the following criteria in order or importance:
 * 1: Language
 * 2: Match preferred format
 * 3: Fallback YT format
 */
void GetAudioScore(YDLStreamDetails& details) {
    int score = 1;
    auto& s = AfxGetAppSettings();

    CString acodec = details.acodec.Left(4);

    if (s.iYDLAudioFormat > 0) {
        if (s.iYDLAudioFormat == YDL_FORMAT_AAC) {
            if (acodec == L"mp4a") score += 64;
        } else {
            if (acodec == L"opus") score += 64;
        }
    }

    if (details.lang_pref > 9) {
        score += 256;
    } else if (details.lang_pref > 0) {
        score += 128;
    }

    if (details.protocol == L"https") {
        score += 32;
    }

    // Youtube formats
    if (!details.has_video && !details.format.IsEmpty()) {
        CString fid = details.format;
        bool isdef = fid.Find(L"default") >= 0;
        bool ishigh = fid.Find(L"high") >= 0;
        int p = fid.FindOneOf(L"- ");
        if (p > 0) {
            fid = fid.Left(p);
        }
        if (fid == L"258") {        // AAC (LC) 384 Kbps 5.1
            score += 13;
        } else if (fid == L"141") { // AAC (LC) 256 Kbps 2.0
            score += 12;
        } else if (fid == L"327") { // AAC (LC) 256 Kbps 5.1
            score += 11;
        } else if (fid == L"256") { // AAC (HE v1) 192 Kbps 5.1
            score += 9;
        } else if (fid == L"140") { // AAC (LC) 128 Kbps 2.0
            score += 8;
        } else if (fid == L"139") { // AAC (HE v1) 48 Kbps 2.0
            score += 5;
        } else if (fid == L"338") { // Opus 480 Kbps 4.0
            score += 10;
        } else if (fid == L"774") { // Opus 256 Kbps 2.0
            score += 8;
        } else if (fid == L"251") { // Opus 160 Kbps 2.0
            score += 7;
        } else if (fid == L"250") { // Opus 70 Kbps 2.0
            score += 6;
        } else if (fid == L"249") { // Opus 50 Kbps 2.0
            score += 4;
        } else if (fid == L"599") { // AAC (HE v1) 30 Kbps 2.0
            score += 1;
        } else if (fid == L"600") { // Opus 35 Kbps 2.0
            score += 1;
        } else if (fid == L"380") { // AC3 384 Kbps 5.1
            score += 7;
        } else if (fid == L"328") { // EAC3 384 Kbps 5.1
            score += 7;
        } else if (fid == L"325") { // DTSE 384 Kbps 5.1
            score += 1;
        } else if (fid == L"233") { // same as 139 but native HLS
            score += 5;
        } else if (fid == L"234") { // same as 140 but native HLS
            score += 8;
        } else {
            //ASSERT(false);
        }
    }

    details.audio_score = score;
}

bool GetYDLStreamDetails(const Value& format, YDLStreamDetails& details, bool require_video, bool require_audio_only)
{
    bool canuse = true;
    details = { _T(""), _T(""), 0, 0, _T(""), _T(""), _T(""), false, false, 0, 0, 0, _T(""), _T(""), _T(""), 0, 0, 0, _T("") };

    details.url = format[_T("url")].GetString();
    if (details.url.IsEmpty()) {
        #if YDL_TRACE
        YDL_LOG(_T("empty url\n"));
        #endif
        return false;
    }
    if (format.HasMember(_T("has_drm")) && !format[_T("has_drm")].IsNull()) {
        if (format[_T("has_drm")].GetBool()) {
            return false;
        }
    }
    if (format.HasMember(_T("protocol")) && format[_T("protocol")].IsString()) {
        details.protocol = CString(format[_T("protocol")].GetString()).MakeLower();
        if (details.protocol == _T("mhtml")) {
            return false;
        }
    }
    if (format.HasMember(_T("vcodec")) && format[_T("vcodec")].IsString()) {
        details.vcodec = CString(format[_T("vcodec")].GetString()).MakeLower();
    }
    if (format.HasMember(_T("acodec")) && format[_T("acodec")].IsString()) {
        details.acodec = CString(format[_T("acodec")].GetString()).MakeLower();
    }
    if (format.HasMember(_T("format")) && format[_T("format")].IsString()) {
        details.format = CString(format[_T("format")].GetString()).MakeLower();
    }
    if (format.HasMember(_T("format_id")) && format[_T("format_id")].IsString()) {
        details.format_id = CString(format[_T("format_id")].GetString()).MakeLower();
    }
    if (format.HasMember(_T("format_note")) && format[_T("format_note")].IsString()) {
        details.format_id = CString(format[_T("format_note")].GetString()).MakeLower();
    }
    if (format.HasMember(_T("language")) && format[_T("language")].IsString()) {
        details.language = CString(format[_T("language")].GetString()).MakeLower();
    }
    if (format.HasMember(_T("width")) && format[_T("width")].IsInt()) {
        details.width = format[_T("width")].GetInt();
    }
    if (format.HasMember(_T("height")) && format[_T("height")].IsInt()) {
        details.height = format[_T("height")].GetInt();
    }
    if (format.HasMember(_T("language_preference")) && format[_T("language_preference")].IsInt()) {
        details.lang_pref = format[_T("language_preference")].GetInt();
    }
    if (format.HasMember(_T("http_headers")) && format[_T("http_headers")].HasMember(_T("User-Agent"))) {
        details.useragent = CString(format[_T("http_headers")][_T("User-Agent")].GetString());
    }

    details.has_audio = !details.acodec.IsEmpty() && details.acodec != _T("none");
    details.has_video = !details.vcodec.IsEmpty() && details.vcodec != _T("none") || (details.width > 0) || (details.height > 0);

    if (!details.has_video && details.protocol == _T("http_dash_segments")) {
        details.has_video = details.url.Find(_T("_hd_clear")) > 0; // youtube manifest url that should have video
    }
    if (!details.has_audio && details.protocol == _T("http_dash_segments")) {
        details.has_audio = details.url.Find(_T("_audio_clear")) > 0; // youtube manifest url that should have audio
    }

    // make assumption
    if (!details.has_video && !details.has_audio) {
        if (details.vcodec != _T("none")) {
            details.has_video = true;
            details.vcodec = _T("unknown");
        }
        if (details.acodec != _T("none")) {
            details.has_audio = true;
            details.acodec = _T("unknown");
        }
    }

    if (canuse && require_video && !details.has_video) {
        canuse = false;
    }
    if (canuse && require_audio_only && (details.has_video || !details.has_audio)) {
        canuse = false;
    }

    details.vbr = details.has_video && format.HasMember(_T("vbr")) && format[_T("vbr")].IsFloat() ? (int)format[_T("vbr")].GetFloat() : 0;
    if (details.vbr == 0 && details.has_video) {
        details.vbr = format.HasMember(_T("tbr")) && format[_T("tbr")].IsFloat() ? (int)format[_T("tbr")].GetFloat() : 0;
    }
    details.abr = details.has_audio && format.HasMember(_T("abr")) && format[_T("abr")].IsFloat() ? (int)format[_T("abr")].GetFloat() : 0;
    if (details.abr == 0 && details.has_audio) {
        details.abr = format.HasMember(_T("tbr")) && format[_T("tbr")].IsFloat() ? (int)format[_T("tbr")].GetFloat() : 0;
    }
    details.fps = format.HasMember(_T("fps")) && format[_T("fps")].IsDouble() ? (int)format[_T("fps")].GetDouble() : 0;
    if (details.has_video && details.fps == 0) {
        if (details.format_id.Find(L"p60") > 0) {
            details.fps = 60;
        }
    }

    if (canuse && details.has_video) {
        GetVideoScore(details);
    }
    if (canuse && details.has_audio) {
        GetAudioScore(details);
    }

    #if YDL_TRACE
    if (canuse) {
        TRACE(_T("protocol=%s vcodec=%s width=%d height=%d fps=%d vbr=%d acodec=%s abr=%d formatid=%s lang=%s(p%d) url=%s\n"), static_cast<LPCWSTR>(details.protocol), static_cast<LPCWSTR>(details.vcodec), details.width, details.height, details.fps, details.vbr, static_cast<LPCWSTR>(details.acodec), details.abr, static_cast<LPCWSTR>(details.format_id), static_cast<LPCWSTR>(details.language), details.lang_pref, static_cast<LPCWSTR>(details.url));
    }
    #endif
    #if YDL_LOG_URLS
    if (canuse) {
        YDL_LOG(_T("protocol=%s vcodec=%s width=%d height=%d fps=%d vbr=%d acodec=%s abr=%d formatid=%s lang=%s(p%d) url=%s"), static_cast<LPCWSTR>(details.protocol), static_cast<LPCWSTR>(details.vcodec), details.width, details.height, details.fps, details.vbr, static_cast<LPCWSTR>(details.acodec), details.abr, static_cast<LPCWSTR>(details.format_id), static_cast<LPCWSTR>(details.language), details.lang_pref, static_cast<LPCWSTR>(details.url));
    }
    #endif

    return canuse;
}

// returns true when second is better than first
bool IsBetterYDLStream(YDLStreamDetails& first, YDLStreamDetails& second, int max_height, bool separate)
{
    if (first.has_video) {
        // We want separate audio/video streams
        if (separate && first.has_audio && !second.has_audio) {
            return true;
        }

        if (!second.has_video) {
            return false;
        }

        // Video score
        if (first.video_score > second.video_score) {
            return false;
        } else {
            if (second.video_score > first.video_score) {
                return true;
            }
        }

        // Video resolution
        if (first.height > first.width) {
            // vertical video
            if (second.width > first.width) {
                if (max_height > 0) {
                    // calculate maximum height based on 9:16 AR
                    return (max_height * 16 / 9 + 1) >= second.height;
                }
                return true;
            } else {
                if (second.width == first.width) {
                    if (second.height > first.height) {
                        return true;
                    }
                    if (second.height < first.height) {
                        return false;
                    }
                } else {
                    return false;
                }
            }
        } else {
            if (second.height > first.height) {
                if (max_height > 0) {
                    // calculate maximum width based on 16:9 AR
                    return (max_height * 16 / 9 + 1) >= second.width;
                }
                return true;
            } else {
                if (second.height == first.height) {
                    if (second.width > first.width) {
                        return true;
                    }
                    if (second.width < first.width) {
                        return false;
                    }
                } else {
                    return false;
                }
            }
        }
    } else if (first.has_audio) {
        if (!second.has_audio) {
            return false;
        }

        // Audio score
        if (first.audio_score > second.audio_score) {
            return false;
        } else {
            if (second.audio_score > first.audio_score) {
                return true;
            }
        }
    } else {
        if (second.has_video || second.has_audio) {
            return true;
        } else {
            if (first.format != _T("none")) {
                return false;
            } else {
                if (second.format != _T("none")) {
                    return true;
                }
            }
        }
    }

    // Prefer HTTPS and m3u8_native protocols above http_dash_segments
    if (first.protocol == _T("http_dash_segments")) {
        if (second.protocol != _T("http_dash_segments")) {
            return true;
        }
    } else {
        if (second.protocol == _T("http_dash_segments")) {
            return false;
        }
    }
    // Prefer HTTPS above m3u8_native
    if (second.protocol == _T("https")) {
        if (first.protocol != _T("https")) {
            return true;
        }
    } else {
        if (first.protocol == _T("https")) {
            return false;
        }
    }

    // Prefer single stream
    if (first.has_video && second.has_video) {
        if (first.has_audio) {
            if (!second.has_audio) {
                return false;
            }
        } else {
            if (second.has_audio) {
                return true;
            }
        }
    }

    // Bitrate
    if (first.has_video) {
        if (second.vbr > first.vbr) {
            return true;
        }
    } else {
        if (second.abr > first.abr) {
            return true;
        }
    }

    return false;
}

// find best video track
bool filterVideo(const Value& formats, YDLStreamDetails& ydl_sd, int max_height, bool separate)
{
    YDLStreamDetails current;
    bool found = false;
#if YDL_TRACE
    TRACE(_T("format count: %d\n"), formats.Size());
#endif
    for (rapidjson::SizeType i = 0; i < formats.Size(); i++) {
        if (GetYDLStreamDetails(formats[i], current, true, false)) {
            if (!found || IsBetterYDLStream(ydl_sd, current, max_height, separate)) {
                ydl_sd = current;
                #if YDL_TRACE
                TRACE(_T("This is currently best video stream\n"));
                #endif
                // A single http dash manifest can appear several times in the formats list with different video and audio parameters.
                // So if current entry does not have audio, check if another entry with same url does have audio.
                if (!ydl_sd.has_audio && ydl_sd.protocol == _T("http_dash_segments")) {
                    for (rapidjson::SizeType j = 0; j < formats.Size(); j++) {
                        if (i != j && formats[j].HasMember(_T("url")) && !formats[j][_T("url")].IsNull()) {
                            CString url = formats[j][_T("url")].GetString();
                            if (url == ydl_sd.url && formats[j].HasMember(_T("acodec")) && !formats[j][_T("acodec")].IsNull()) {
                                CString acodec = CString(formats[j][_T("acodec")].GetString()).MakeLower();
                                if (!acodec.IsEmpty() && acodec != _T("none")) {
                                    ydl_sd.has_audio = true;
                                    ydl_sd.acodec = acodec;
                                    #if YDL_TRACE
                                    TRACE(_T("Found matching audio stream for above manifest url\n"));
                                    #endif
                                    break;
                                }
                            }
                        }
                    }
                }
            }
            found = true;
        }
    }   
    return found;
}

// find best audio track (in case we use separate streams)
bool filterAudio(const Value& formats, YDLStreamDetails& ydl_sd)
{
    YDLStreamDetails current;
    bool found = false;

    for (rapidjson::SizeType i = 0; i < formats.Size(); i++) {
        if (GetYDLStreamDetails(formats[i], current, false, true)) {
            if (!found || IsBetterYDLStream(ydl_sd, current, 0, true)) {
                ydl_sd = current;
                #if YDL_TRACE
                TRACE(_T("this is currently best audio stream\n"));
                #endif
                //YDL_LOG(_T("this is currently best audio stream"));
            }
            found = true;
        }
    }
    return found;
}

bool CYoutubeDLInstance::GetHttpStreams(CAtlList<YDLStreamURL>& streams, YDLPlaylistInfo& info, CString& useragent)
{
    CString url;
    CString extractor;
    YDLStreamDetails ydl_sd;
    YDLStreamURL stream;

    if (pJSON->d.IsObject() && pJSON->d.HasMember(_T("extractor"))) {
        extractor = pJSON->d[_T("extractor")].GetString();
    } else {
        return false;
    }

    auto& s = AfxGetAppSettings();

    if (!bIsPlaylist) {
        if (pJSON->d.HasMember(_T("title")) && !pJSON->d[_T("title")].IsNull()) {
            stream.title = pJSON->d[_T("title")].GetString();
        }

        if (!pJSON->d.HasMember(_T("formats")) || pJSON->d[_T("formats")].IsNull()) {
            if (pJSON->d.HasMember(_T("url")) && !pJSON->d[_T("url")].IsNull()) {
                stream.video_url = pJSON->d[_T("url")].GetString();
                stream.audio_url = _T("");
                if (!stream.video_url.IsEmpty()) {
                    streams.AddTail(stream);
                    return true;
                }
            }
            return false;
        }

        if (pJSON->d.HasMember(_T("series")) && !pJSON->d[_T("series")].IsNull()) stream.series = pJSON->d[_T("series")].GetString();
        if (pJSON->d.HasMember(_T("season")) && !pJSON->d[_T("season")].IsNull()) stream.season = pJSON->d[_T("season")].GetString();
        if (pJSON->d.HasMember(_T("season_number")) && !pJSON->d[_T("season_number")].IsNull()) stream.season_number = pJSON->d[_T("season_number")].GetInt();
        if (pJSON->d.HasMember(_T("season_id")) && !pJSON->d[_T("season_id")].IsNull()) stream.season_id = pJSON->d[_T("season_id")].GetString();
        if (pJSON->d.HasMember(_T("episode")) && !pJSON->d[_T("episode")].IsNull()) stream.episode = pJSON->d[_T("episode")].GetString();
        if (pJSON->d.HasMember(_T("episode_number")) && !pJSON->d[_T("episode_number")].IsNull()) stream.episode_number = pJSON->d[_T("episode_number")].GetInt();
        if (pJSON->d.HasMember(_T("episode_id")) && !pJSON->d[_T("episode_id")].IsNull()) stream.episode_id = pJSON->d[_T("episode_id")].GetString();
        if (pJSON->d.HasMember(_T("webpage_url")) && !pJSON->d[_T("webpage_url")].IsNull()) stream.webpage_url = pJSON->d[_T("webpage_url")].GetString();

        if (!s.sYDLSubsPreference.IsEmpty()) {
            if (pJSON->d.HasMember(_T("subtitles")) && !pJSON->d[_T("subtitles")].IsNull() && pJSON->d[_T("subtitles")].IsObject()) {
                loadSub(pJSON->d[_T("subtitles")], stream.subtitles);
            }
            if (s.bUseAutomaticCaptions) {
                if (pJSON->d.HasMember(_T("automatic_captions")) && !pJSON->d[_T("automatic_captions")].IsNull() && pJSON->d[_T("automatic_captions")].IsObject()) {
                    loadSub(pJSON->d[_T("automatic_captions")], stream.subtitles, true);
                }
            }
        }

        if (filterVideo(pJSON->d[_T("formats")], ydl_sd, s.iYDLMaxHeight, s.bYDLAudioOnly)) {
            stream.video_url = ydl_sd.url;
            stream.audio_url = _T("");
            // find separate audio stream
            if (ydl_sd.has_video && !ydl_sd.has_audio) {
                if (filterAudio(pJSON->d[_T("formats")], ydl_sd)) {
                    if (ydl_sd.url != stream.video_url) {
                        stream.audio_url = ydl_sd.url;
                        #if YDL_TRACE
                        TRACE(_T("selected video url = %s\n"), static_cast<LPCWSTR>(stream.video_url));
                        TRACE(_T("selected audio url = %s\n"), static_cast<LPCWSTR>(stream.audio_url));
                        #endif
                    }
                }
            }
            streams.AddTail(stream);
            useragent = ydl_sd.useragent;
        } else if (filterAudio(pJSON->d[_T("formats")], ydl_sd)) {
            stream.audio_url = ydl_sd.url;
            stream.video_url = _T("");
            streams.AddTail(stream);
            useragent = ydl_sd.useragent;
        }
    } else {
        if (pJSON->d.HasMember(_T("id")) && !pJSON->d[_T("id")].IsNull()) info.id = pJSON->d[_T("id")].GetString();
        if (pJSON->d.HasMember(_T("title")) && !pJSON->d[_T("title")].IsNull()) info.title = pJSON->d[_T("title")].GetString();
        if (pJSON->d.HasMember(_T("uploader")) && !pJSON->d[_T("uploader")].IsNull()) info.uploader = pJSON->d[_T("uploader")].GetString();
        if (pJSON->d.HasMember(_T("uploader_id")) && !pJSON->d[_T("uploader_id")].IsNull()) info.uploader_id = pJSON->d[_T("uploader_id")].GetString();
        if (pJSON->d.HasMember(_T("uploader_url")) && !pJSON->d[_T("uploader_url")].IsNull()) info.uploader_url = pJSON->d[_T("uploader_url")].GetString();
        if (pJSON->d.HasMember(_T("entries"))) {
            YDL_LOG(_T("Parsing playlist"));
            const Value& entries = pJSON->d[_T("entries")];

            for (rapidjson::SizeType i = 0; i < entries.Size(); i++) {
                YDL_LOG(_T("Playlist entry %d"), i);
                const Value& entry = entries[i];

                if (!entry.HasMember(_T("formats")) || entry[_T("formats")].IsNull()) {
                    if (entry.HasMember(_T("url")) && !entry[_T("url")].IsNull()) {
                        stream.video_url = entry[_T("url")].GetString();
                        stream.audio_url = _T("");
                        if (!stream.video_url.IsEmpty()) {
                            streams.AddTail(stream);
                        }
                    }
                    continue;
                }

                if (entry.HasMember(_T("title")) && !entry[_T("title")].IsNull()) {
                    stream.title = entry[_T("title")].GetString();
                }
                if (entry.HasMember(_T("webpage_url")) && !entry[_T("webpage_url")].IsNull()) {
                    stream.webpage_url = entry[_T("webpage_url")].GetString();
                }

                if (filterVideo(entry[_T("formats")], ydl_sd, s.iYDLMaxHeight, s.bYDLAudioOnly)) {
                    stream.video_url = ydl_sd.url;
                    stream.audio_url = _T("");
                    if (entry.HasMember(_T("series")) && !entry[_T("series")].IsNull()) stream.series = entry[_T("series")].GetString();
                    if (entry.HasMember(_T("season")) && !entry[_T("season")].IsNull()) stream.season = entry[_T("season")].GetString();
                    if (entry.HasMember(_T("season_number")) && !entry[_T("season_number")].IsNull()) stream.season_number = entry[_T("season_number")].GetInt();
                    if (entry.HasMember(_T("season_id")) && !entry[_T("season_id")].IsNull()) stream.season_id = entry[_T("season_id")].GetString();
                    if (entry.HasMember(_T("episode")) && !entry[_T("episode")].IsNull()) stream.episode = entry[_T("episode")].GetString();
                    if (entry.HasMember(_T("episode_number")) && !entry[_T("episode_number")].IsNull()) stream.episode_number = entry[_T("episode_number")].GetInt();
                    if (entry.HasMember(_T("episode_id")) && !entry[_T("episode_id")].IsNull()) stream.episode_id = entry[_T("episode_id")].GetString();
                    if (entry.HasMember(_T("webpage_url")) && !entry[_T("webpage_url")].IsNull()) stream.webpage_url = entry[_T("webpage_url")].GetString();
                    if (!s.sYDLSubsPreference.IsEmpty()) {
                        if (entry.HasMember(_T("subtitles")) && !entry[_T("subtitles")].IsNull() && entry[_T("subtitles")].IsObject()) {
                            loadSub(entry[_T("subtitles")], stream.subtitles);
                        }
                        if (s.bUseAutomaticCaptions && entry.HasMember(_T("automatic_captions")) && !entry[_T("automatic_captions")].IsNull() && entry[_T("automatic_captions")].IsObject()) {
                            loadSub(entry[_T("automatic_captions")], stream.subtitles);
                        }
                    }
                    if (ydl_sd.has_video && !ydl_sd.has_audio && entry.HasMember(_T("formats")) && !entry[_T("formats")].IsNull()) {
                        if (filterAudio(entry[_T("formats")], ydl_sd)) {
                            stream.audio_url = ydl_sd.url;
                        }
                    }
                    streams.AddTail(stream);
                    if (i == 0) {
                        useragent = ydl_sd.useragent;
                    }
                } else if (filterAudio(entry[_T("formats")], ydl_sd)) {
                    stream.audio_url = ydl_sd.url;
                    stream.video_url = _T("");
                    streams.AddTail(stream);
                    if (i == 0) {
                        useragent = ydl_sd.useragent;
                    }
                }
            }
        }
    }
    return !streams.IsEmpty();
}

bool CYoutubeDLInstance::loadJSON()
{
    if (!buf_out) {
        return false;
    }
    //the JSON buffer is ASCII with Unicode encoded with escape characters
    pJSON->d.Parse<rapidjson::kParseDefaultFlags, rapidjson::ASCII<>>(buf_out);
    if (pJSON->d.HasParseError()) {
        return false;
    }
    if (!pJSON->d.IsObject() || !pJSON->d.HasMember(_T("extractor"))) {
        return false;
    }
    bIsPlaylist = pJSON->d.FindMember(_T("entries")) != pJSON->d.MemberEnd();
    return true;
}

void CYoutubeDLInstance::loadSub(const Value& obj, CAtlList<YDLSubInfo>& subs, bool isAutomaticCaptions /*= false*/) {
    auto& s = AfxGetAppSettings();
    CAtlList<CString> preferlist;
    if (!s.sYDLSubsPreference.IsEmpty()) {
        if (s.sYDLSubsPreference.Find(_T(',')) != -1) {
            ExplodeMin(s.sYDLSubsPreference, preferlist, ',');
        } else {
            ExplodeMin(s.sYDLSubsPreference, preferlist, ' ');
        }
    }
    if (!isAutomaticCaptions) {
        subs.RemoveAll();
    }
    for (Value::ConstMemberIterator iter = obj.MemberBegin(); iter != obj.MemberEnd(); ++iter) {
        CString lang(iter->name.GetString());
        if (!preferlist.IsEmpty() && !isPrefer(preferlist, lang)) {
            continue;
        }
        if (iter->value.IsArray()) {
            const Value& arr = obj[(LPCTSTR)lang];
            for (rapidjson::SizeType i = 0; i < arr.Size(); i++) {
                const Value& dict = arr[i];
                YDLSubInfo sub;
                sub.isAutomaticCaptions = isAutomaticCaptions;
                sub.lang = lang;
                if (dict.HasMember(_T("ext")) && !dict[_T("ext")].IsNull()) {
                    sub.ext = dict[_T("ext")].GetString();
                }
                if (dict.HasMember(_T("url")) && !dict[_T("url")].IsNull()) {
                    sub.url = dict[_T("url")].GetString();
                }
                if (dict.HasMember(_T("data")) && !dict[_T("data")].IsNull() && dict[_T("data")].IsString()) {
                    sub.data = dict[_T("data")].GetString();
                }
                if (!sub.url.IsEmpty() || !sub.data.IsEmpty()) {
                    if (sub.ext.IsEmpty() || sub.ext == _T("vtt") || sub.ext == _T("ass") || sub.ext == _T("srt")) {
                        subs.AddTail(sub);
                    }
                }
            }
        }
    }
}

bool CYoutubeDLInstance::isPrefer(CAtlList<CString>& list, CString& lang) {
    POSITION pos = list.GetHeadPosition();
    while (pos) {
        CString la = list.GetNext(pos);
        if (lang.Left(la.GetLength()) == la) return true;
    }
    return false;
}
