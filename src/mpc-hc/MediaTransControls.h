/*
* (C) 2002-2021 see Authors.txt
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
#include <sdkddkver.h>
#include <atlbase.h>
#include <windows.media.h>
#include "SMTC2Compat.h"
#include <atomic>

// Set to 0 to disable capturing video frames for the SMTC thumbnail
#define MPC_SMTC_VIDEO_THUMBNAIL 0

// Messages posted to CMainFrame from SMTC event callbacks, which arrive on WinRT threadpool threads
#define WM_SMTC_SEEK        (WM_APP + 900)
#define WM_SMTC_AUTOREPEAT  (WM_APP + 901)
#define WM_SMTC_SHUFFLE     (WM_APP + 902)
#define WM_SMTC_RATE        (WM_APP + 903)

class CMainFrame;

class MediaTransControls {
public:
    MediaTransControls(void) {
        m_pMainFrame = nullptr;
        smtc_controls = nullptr;
        smtc_controls2 = nullptr;
        smtc_updater = nullptr;
        m_EventRegistrationToken.value = 0;
        m_EventRegistrationTokenPositionChange.value = 0;
        m_EventRegistrationTokenRateChange.value = 0;
        m_EventRegistrationTokenShuffleChange.value = 0;
        m_EventRegistrationTokenAutoRepeatChange.value = 0;
        requested_seek_position = 0;
        requested_playback_rate = 0.0;
    }
    ~MediaTransControls(void) {
        if (smtc_controls && m_EventRegistrationToken.value) {
            smtc_controls->remove_ButtonPressed(m_EventRegistrationToken);
        }
        if (smtc_controls2) {
            if (m_EventRegistrationTokenPositionChange.value) {
                smtc_controls2->remove_PlaybackPositionChangeRequested(m_EventRegistrationTokenPositionChange);
            }
            if (m_EventRegistrationTokenRateChange.value) {
                smtc_controls2->remove_PlaybackRateChangeRequested(m_EventRegistrationTokenRateChange);
            }
            if (m_EventRegistrationTokenShuffleChange.value) {
                smtc_controls2->remove_ShuffleEnabledChangeRequested(m_EventRegistrationTokenShuffleChange);
            }
            if (m_EventRegistrationTokenAutoRepeatChange.value) {
                smtc_controls2->remove_AutoRepeatModeChangeRequested(m_EventRegistrationTokenAutoRepeatChange);
            }
        }
    }

    bool Init(CMainFrame* main);

    void close();

    CComPtr<ABI::Windows::Media::ISystemMediaTransportControls> smtc_controls;
    CComPtr<ABI::Windows::Media::ISystemMediaTransportControls2> smtc_controls2;
    CComPtr<ABI::Windows::Media::ISystemMediaTransportControlsDisplayUpdater> smtc_updater;

    void loadThumbnail(CString fn);
    void loadThumbnail(BYTE* content, size_t size);
    void loadThumbnailFromUrl(CString url);
    bool IsActive();

    // ISystemMediaTransportControls2 features (Windows 10 1607+)
    void SetAutoRepeatMode(ABI::Windows::Media::MediaPlaybackAutoRepeatMode mode);
    void SetShuffleEnabled(bool enabled);
    void SetPlaybackRate(double rate);
    void UpdateTimelineProperties(REFERENCE_TIME startTime, REFERENCE_TIME endTime, REFERENCE_TIME position);

    // Payloads of the WM_SMTC_SEEK/WM_SMTC_RATE messages, written by the SMTC
    // callback threads and read by the main frame handlers on the UI thread
    std::atomic<REFERENCE_TIME> requested_seek_position;
    std::atomic<double> requested_playback_rate;
protected:
    CMainFrame* m_pMainFrame;
    EventRegistrationToken m_EventRegistrationToken;
    EventRegistrationToken m_EventRegistrationTokenPositionChange;
    EventRegistrationToken m_EventRegistrationTokenRateChange;
    EventRegistrationToken m_EventRegistrationTokenShuffleChange;
    EventRegistrationToken m_EventRegistrationTokenAutoRepeatChange;
    CComPtr<ABI::Windows::Media::ISystemMediaTransportControlsTimelineProperties> smtc_timeline;
    void OnButtonPressed(ABI::Windows::Media::SystemMediaTransportControlsButton button);
};
