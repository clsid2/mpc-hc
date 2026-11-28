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

#include <mmdeviceapi.h>
#include <vector>

namespace AudioDevices
{
    struct Endpoint {
        CString name;
        CString id;
    };

    // The audio output endpoints in the given states, a mask of DEVICE_STATE_ flags.
    std::vector<Endpoint> GetRenderEndpoints(DWORD stateMask);

    // Windows can give a device a new endpoint ID, for example when its cable moves to another port.
    // Returns the new ID when savedId is not among activeEndpoints and exactly one of them has the saved
    // name. Returns an empty string otherwise, including when two active endpoints have that name.
    CString FindMovedEndpoint(const std::vector<Endpoint>& activeEndpoints, const CString& savedId, const CString& savedName);
}
