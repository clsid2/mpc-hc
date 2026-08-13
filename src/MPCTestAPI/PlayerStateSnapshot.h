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

#pragma once

#include "../mpc-hc/MpcApi.h"

#include <map>


class CPlayerStateSnapshot
{
public:
    static const UINT COLLECTION_WINDOW_MS = 750;

    void Begin();
    bool IsActive() const;
    bool Capture(MPCAPI_COMMAND response, LPCTSTR value);
    CString Complete();

    static size_t GetRequestCount();
    static MPCAPI_COMMAND GetRequest(size_t index);

private:
    bool m_active = false;
    std::map<MPCAPI_COMMAND, CString> m_responses;
};
