/*
 * (C) 2015 see Authors.txt
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

// Player events recorded for crash reports
enum class CrashEvent : unsigned short {
    Start,
    LoadState,      // arg: MLS
    GraphBuilt,
    Play,
    Pause,
    Stop,
    Fullscreen,     // arg: 1 entering
    D3DFullscreen,  // arg: 1 entering
    ResetDevice,    // arg: 0 begin, 1 ok, -1 failed
    CloseStep,      // arg: step inside CloseMediaPrivate
    Close,
    Destroy,
};

namespace CrashReporter
{
    void Enable();
    void Disable();
    bool IsEnabled();

    // Key/value sent with the report. An empty value removes the key.
    void SetInfo(LPCWSTR key, LPCWSTR value);
    void LogEvent(CrashEvent e, int arg = 0);

    // mpcvrRenderType and lavHWAccel are -1 when unknown
    void UpdatePlaybackInfo(int videoRenderer, int mpcvrRenderType, int lavHWAccel, LPCWSTR activeDecoder);
};
