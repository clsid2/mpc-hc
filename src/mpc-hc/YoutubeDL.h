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
#pragma once
#include "stdafx.h"
#include "rapidjson/include/rapidjson/document.h"
typedef rapidjson::GenericValue<rapidjson::UTF16<>> Value;

struct  CUtf16JSON;

CString GetYDLExePath(bool* is_ytdlp);

class CYoutubeDLInstance
{
public:
    CYoutubeDLInstance();
    ~CYoutubeDLInstance();

    struct YDLSubInfo {
        CString lang;
        CString ext;
        CString url;
        CString data;
        bool isAutomaticCaptions = false;
    };

    class YDLStreamURL {
    public:
        CString video_url;
        CString audio_url;
        CString title;
        CString season;
        CString series;
        int season_number = -1;
        CString season_id;
        CString episode;
        int episode_number = -1;
        CString episode_id;
        CString webpage_url;
        std::list<YDLSubInfo> subtitles;
        YDLStreamURL() :
            video_url(),
            audio_url(),
            title(),
            season(),
            series(),
            season_number(-1),
            season_id(),
            episode(),
            episode_number(-1),
            episode_id(),
            webpage_url(),
            subtitles() {}
        YDLStreamURL(const YDLStreamURL& r) :
            video_url(r.video_url),
            audio_url(r.audio_url),
            title(r.title),
            season(r.season),
            series(r.series),
            season_number(r.season_number),
            season_id(r.season_id),
            episode(r.episode),
            episode_number(r.episode_number),
            episode_id(r.episode_id),
            webpage_url(r.webpage_url),
            subtitles(r.subtitles) {
        }
    };

    struct YDLPlaylistInfo {
        CString id;
        CString title;
        CString uploader;
        CString uploader_id;
        CString uploader_url;
    };

    bool Run(CString url);
    bool GetHttpStreams(std::list<YDLStreamURL>& streams, YDLPlaylistInfo& info, CString& useragent);
    static bool isPrefer(std::list<CString>& list, CString& lang);

private:
    CUtf16JSON* pJSON;
    bool bIsPlaylist;
    HANDLE hStdout_r, hStdout_w;
    HANDLE hStderr_r, hStderr_w;
    char* buf_out;
    char* buf_err;
    DWORD idx_out;
    DWORD idx_err;
    DWORD capacity_out, capacity_err;

    bool loadJSON();
    void loadSub(const Value& obj, std::list<YDLSubInfo>& subs, bool isAutomaticCaptions = false);
    static DWORD WINAPI BuffOutThread(void* ydl_inst);
    static DWORD WINAPI BuffErrThread(void* ydl_inst);
};
