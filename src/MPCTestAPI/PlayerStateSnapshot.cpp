/*
 * (C) 2008-2026 see Authors.txt
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
#include "PlayerStateSnapshot.h"


struct PlayerStateField
{
    LPCTSTR label;
    MPCAPI_COMMAND request;
    MPCAPI_COMMAND response;
};

// Add one row to extend the client-side snapshot with another independent
// request/reply pair. The API has no atomic aggregate-state command.
static const PlayerStateField playerStateFields[] = {
    { _T("version"), CMD_GETVERSION, CMD_VERSION },
    { _T("now-playing"), CMD_GETNOWPLAYING, CMD_NOWPLAYING },
    { _T("position"), CMD_GETCURRENTPOSITION, CMD_CURRENTPOSITION },
    { _T("audio-track"), CMD_GETCURRENTAUDIOTRACK, CMD_CURRENTAUDIOTRACK },
    { _T("subtitle-track"), CMD_GETCURRENTSUBTITLETRACK, CMD_CURRENTSUBTITLETRACK },
    { _T("volume"), CMD_GETVOLUME, CMD_CURRENTVOLUME },
    { _T("mute"), CMD_GETMUTE, CMD_CURRENTMUTE },
};

void CPlayerStateSnapshot::Begin()
{
    m_responses.clear();
    m_active = true;
}

bool CPlayerStateSnapshot::IsActive() const
{
    return m_active;
}

bool CPlayerStateSnapshot::Capture(MPCAPI_COMMAND response, LPCTSTR value)
{
    if (!m_active) {
        return false;
    }

    for (const PlayerStateField& field : playerStateFields) {
        if (field.response == response) {
            return m_responses.emplace(response, value).second;
        }
    }

    return false;
}

CString CPlayerStateSnapshot::Complete()
{
    m_active = false;

    CString summary;
    summary.Format(_T("CLIENT_QUERYPLAYERSTATE (non-atomic; replies collected for up to %u ms): "),
                   COLLECTION_WINDOW_MS);

    for (size_t i = 0; i < _countof(playerStateFields); i++) {
        const PlayerStateField& field = playerStateFields[i];
        if (i != 0) {
            summary.Append(_T(" | "));
        }

        const auto response = m_responses.find(field.response);
        if (response == m_responses.cend()) {
            summary.AppendFormat(_T("%s=<no reply>"), field.label);
        } else {
            CString value = response->second;
            value.Replace(_T("\r"), _T(" "));
            value.Replace(_T("\n"), _T(" "));
            summary.AppendFormat(_T("%s=\"%s\""), field.label, value.GetString());
        }
    }

    m_responses.clear();
    return summary;
}

size_t CPlayerStateSnapshot::GetRequestCount()
{
    return _countof(playerStateFields);
}

MPCAPI_COMMAND CPlayerStateSnapshot::GetRequest(size_t index)
{
    ASSERT(index < _countof(playerStateFields));
    return playerStateFields[index].request;
}
