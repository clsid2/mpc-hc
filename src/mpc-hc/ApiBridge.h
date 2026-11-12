/*
 * (C) 2026 see Authors.txt
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

#include "MpcApi.h"
#include "MediaTransControls.h"

class CMainFrame;
struct CPlayerGraph;
class CWebServer;

// The outside world's view of the player: the WM_COPYDATA and integer API to a
// hosting application, the web server, the Windows media controls (SMTC), the
// taskbar thumbnail toolbar and the Logitech LCD. Composed into CMainFrame,
// which forwards the window messages that carry them (WM_COPYDATA, the integer
// API message and the SMTC requests) here.
class CApiBridge
{
    CMainFrame& m_frame;
    CPlayerGraph& m_graph;

    CAutoPtr<CWebServer> m_pWebServer;

    ULONGLONG m_lastSMTCTimelineUpdate = 0;
#if MPC_SMTC_VIDEO_THUMBNAIL
    ULONGLONG m_nextSMTCThumbnailUpdate = 0;
    ULONGLONG m_lastSMTCThumbnailTick = 0;
#endif

public:
    explicit CApiBridge(CMainFrame& frame);
    ~CApiBridge();

    int m_lastApiVolume = -1;
    int m_lastApiMute = -1;
    int m_hostIntApiVersion = 0; // >0 once the connected host says MPCINT_HELLO (integer channel)

    CMPC_Lcd m_Lcd;
    CComPtr<ITaskbarList3> m_pTaskbarList;
    MediaTransControls m_media_trans_control;

    void StartWebServer(int nPort);
    void StopWebServer();

    void OnApiPlay();
    void OnApiPause();

    // MPC API functions
    void        ProcessAPICommand(HWND hSender, COPYDATASTRUCT* pCDS);
    void        SendAPICommand(MPCAPI_COMMAND nCommand, LPCWSTR fmt, ...);
    bool        SendAPIStringTo(HWND hTarget, MPCAPI_COMMAND nCommand, const CStringW& payload);
    void        PostApiInt(HWND hTarget, WORD command, int value);
    void        SendApiNotify(MPCAPI_COMMAND cmd, int value); // integer channel if host supports it, else WM_COPYDATA
    LRESULT     OnApiIntMessage(WPARAM wParam, LPARAM lParam);
    void        SendNowPlayingToApi(bool sendtrackinfo = true);
    void        SendAudioTracksToApi();
    void        SendPlaylistToApi();
    void        SendCurrentPositionToApi(bool fNotifySeek = false);
    void        SendCurrentVolumeToApi(bool force = false);
    void        SendCurrentMuteToApi(bool force = false);
    void        ShowOSDCustomMessageApi(const MPC_OSDDATA* osdData);

    HRESULT CreateThumbnailToolbar();
    HRESULT UpdateThumbarButton();
    HRESULT UpdateThumbarButton(MPC_PLAYSTATE iPlayState);
    HRESULT UpdateThumbnailClip();

    void MediaTransportControlSetMedia();
    void MediaTransportControlUpdateState(OAFilterState state);
    void MediaTransportControlUpdateTimeline(bool force = false);
    void MediaTransportControlUpdateAutoRepeat();
#if MPC_SMTC_VIDEO_THUMBNAIL
    void MediaTransportControlUpdateThumbnail();
#endif
    LRESULT OnSmtcSeek(WPARAM wParam, LPARAM lParam);
    LRESULT OnSmtcAutoRepeat(WPARAM wParam, LPARAM lParam);
    LRESULT OnSmtcShuffle(WPARAM wParam, LPARAM lParam);
    LRESULT OnSmtcRate(WPARAM wParam, LPARAM lParam);
};
