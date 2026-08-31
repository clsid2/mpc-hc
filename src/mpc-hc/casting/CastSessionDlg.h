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

#include "DebugShadersDlg.h" // CModelessDialog
#include "CMPCThemeSliderCtrl.h"
#include "CastTarget.h"
#include "resource.h"

// What the player hands over when a cast starts. Everything the session needs
// is in here, so that from the moment it starts nothing about it depends on the
// player's graph, its playlist or even the file still being open: the player
// goes back to being an ordinary stopped player.
struct CastSessionMedia {
    CString path;              // the file the media server serves
    CString title;             // what to call it in the window and on the device
    CastMediaInfo info;        // what MediaInfo said about it, for DLNA profiles
    double durationSec = 0.0;
    double startSec = 0.0;     // where the device picks the file up
    bool rememberPosition = false; // history keeps a position for this file
};

// The window a cast session lives in. It owns the session: it drives the
// device, it is the only thing that shows where the device is, and closing it
// ends the cast. The player keeps no part of this, which is why none of its
// own transport, rate, frame step or A-B repeat has to be taken away while a
// cast is running.
class CCastSessionDlg : public CModelessDialog
{
public:
    CCastSessionDlg(CCastTarget* pTarget, CWnd* pParent);
    virtual ~CCastSessionDlg();

    enum { IDD = IDD_CASTSESSION_DLG };

    UINT GetDialogTemplateID() const override { return IDD; }
    void SetupAnchors() override;
    TrackSizeConstraints GetTrackSizeConstraints() const override;

    // The device is already connected when this is called; the window loads the
    // media into it and takes the session over from here.
    void StartSession(const CastSessionMedia& media);

    bool IsCasting() const;
    CString GetActiveDeviceId() const;
    const CString& GetPath() const { return m_media.path; }
    double GetPosition() const;

    // Stops the device and writes down where it got to. The window stays up
    // saying what happened, because a session that ended by itself is worth
    // reading; nStatus is the string that says so, 0 for nothing to report.
    // bSyncLocalGraph is false when another session follows straight away and
    // the player has no epilogue coming to it.
    void EndSession(UINT nStatus = IDS_CAST_STOPPED, bool bSyncLocalGraph = true);

protected:
    virtual void DoDataExchange(CDataExchange* pDX);
    virtual BOOL OnInitDialog();

    void UpdateTransport();  // buttons and status line, from the device state
    // Seekbar and time, from the device clock. bPlayedOut is set only when the
    // device reports the media finished, and shows the end of the file rather
    // than the last position it managed to report before it got there.
    void UpdatePosition(bool bPlayedOut = false);
    void SetStatusText(UINT nID, const CString& detail = CString());
    void RememberPosition(double seconds);
    void HoldSystemAwake(bool hold);

    CMPCThemeSliderCtrl m_seek;
    CMPCThemeSliderCtrl m_volume;

    CCastTarget* m_pTarget;
    CastSessionMedia m_media;
    bool m_bSessionLive = false;
    int m_nLoops = 0;              // times the device has played the media out
    bool m_bSeekDrag = false;      // the user has hold of the seekbar thumb
    ULONGLONG m_seekSettleUntil = 0; // until then the seekbar shows what was asked for
    double m_seekRequested = 0.0;
    double m_lastRemembered = 0.0; // position the history was last written at
    HANDLE m_hPowerRequest = nullptr;

    DECLARE_MESSAGE_MAP()

public:
    afx_msg void OnDestroy();
    afx_msg void OnTimer(UINT_PTR nIDEvent);
    afx_msg void OnHScroll(UINT nSBCode, UINT nPos, CScrollBar* pScrollBar);
    afx_msg void OnPlayPause();
    afx_msg void OnStop();
    afx_msg LRESULT OnCastStateChanged(WPARAM wParam, LPARAM lParam);
};
