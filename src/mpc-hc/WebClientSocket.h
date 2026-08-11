/*
 * (C) 2003-2006 Gabest
 * (C) 2006-2015 see Authors.txt
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

class CWebServer;

class CWebClientSocket : public CAsyncSocket
{
    CWebServer* m_pWebServer;
    CMainFrame* m_pMainFrame;

    char* m_buff;
    int m_buffLen, m_buffMaxLen, m_buffLenProcessed;

    enum PARSING_STATE {
        PARSING_HEADER,
        PARSING_POST_DATA,
        PARSING_DONE
    };
    PARSING_STATE m_parsingState;
    int m_dataLen;

    struct cookie_attribs {
        CString path, expire, domain;
    };
    CAtlStringMap<cookie_attribs, CStringA> m_cookieattribs;

    void Clear();
    void HandleRequest();
    void ParseHeader(const char* headerEnd);
    void ParsePostData();

protected:
    void OnReceive(int nErrorCode);
    void OnClose(int nErrorCode);

public:
    CWebClientSocket(CWebServer* pWebServer, CMainFrame* pMainFrame);
    virtual ~CWebClientSocket();

    bool SetCookie(CStringA name, CString value = _T(""), __time64_t expire = -1, CString path = _T("/"), CString domain = _T(""));

    CString m_sessid;
    CStringA m_cmd, m_path, m_query, m_ver;
    CStringA m_data;
    CAtlStringMap<CStringA, CStringA> m_hdrlines;
    CAtlStringMap<CString, CStringA> m_get, m_post, m_cookie;
    CAtlStringMap<CString, CStringA> m_request;

    bool OnCommand(CStringA& hdr, CStringA& body, CStringA& mime);
    bool OnIndex(CStringA& hdr, CStringA& body, CStringA& mime);
    bool OnInfo(CStringA& hdr, CStringA& body, CStringA& mime);
    bool OnBrowser(CStringA& hdr, CStringA& body, CStringA& mime);
    bool OnControls(CStringA& hdr, CStringA& body, CStringA& mime);
    bool OnVariables(CStringA& hdr, CStringA& body, CStringA& mime);
    bool OnStatus(CStringA& hdr, CStringA& body, CStringA& mime);
    bool OnError404(CStringA& hdr, CStringA& body, CStringA& mime);
    bool OnPlayer(CStringA& hdr, CStringA& body, CStringA& mime);
    bool OnRemote(CStringA& hdr, CStringA& body, CStringA& mime);
    bool OnToolbarImage(CStringA& hdr, CStringA& body, CStringA& mime);
    bool OnSnapshotJpeg(CStringA& hdr, CStringA& body, CStringA& mime);
    bool OnViewRes(CStringA& hdr, CStringA& body, CStringA& mime);
    bool OnDVBChannels(CStringA& hdr, CStringA& body, CStringA& mime);
    bool OnDVBSetChannel(CStringA& hdr, CStringA& body, CStringA& mime);
    bool OnStatusJSON(CStringA& hdr, CStringA& body, CStringA& mime);
    bool OnPlaylistJSON(CStringA& hdr, CStringA& body, CStringA& mime);
    bool OnCommandsJSON(CStringA& hdr, CStringA& body, CStringA& mime);
    bool OnBrowseJSON(CStringA& hdr, CStringA& body, CStringA& mime);

private:
    CString GetSize() const;
    void GetNavStreamTracksJSON(DWORD dwSelGroup, UINT baseID, CStringA& tracks, int& active) const;
    CStringA GetAudioTracksJSON() const;
    CStringA GetSubtitleTracksJSON() const;
};
