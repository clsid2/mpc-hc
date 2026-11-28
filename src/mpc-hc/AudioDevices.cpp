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

#include "stdafx.h"
#include "AudioDevices.h"

#include <FunctionDiscoveryKeys_devpkey.h>

namespace AudioDevices
{
    std::vector<Endpoint> GetRenderEndpoints(DWORD stateMask)
    {
        std::vector<Endpoint> ret;

        CComPtr<IMMDeviceEnumerator> enumerator;
        CComPtr<IMMDeviceCollection> collection;
        UINT count = 0;

        if (SUCCEEDED(enumerator.CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_INPROC_SERVER)) &&
                SUCCEEDED(enumerator->EnumAudioEndpoints(eRender, stateMask, &collection)) &&
                SUCCEEDED(collection->GetCount(&count))) {

            for (UINT i = 0; i < count; i++) {
                CComPtr<IMMDevice> device;
                CComHeapPtr<WCHAR> id;
                CComPtr<IPropertyStore> devicePropertyStore;
                PROPVARIANT friendlyName;
                PropVariantInit(&friendlyName);

                if (SUCCEEDED(collection->Item(i, &device)) &&
                        SUCCEEDED(device->GetId(&id)) &&
                        SUCCEEDED(device->OpenPropertyStore(STGM_READ, &devicePropertyStore)) &&
                        SUCCEEDED(devicePropertyStore->GetValue(PKEY_Device_FriendlyName, &friendlyName))) {

                    ret.push_back({ friendlyName.pwszVal, static_cast<LPWSTR>(id) });
                    PropVariantClear(&friendlyName);
                }
            }
        }

        return ret;
    }

    CString FindMovedEndpoint(const std::vector<Endpoint>& activeEndpoints, const CString& savedId, const CString& savedName)
    {
        CString match;

        if (savedId.IsEmpty() || savedName.IsEmpty()) {
            return match;
        }

        int matches = 0;
        for (const auto& endpoint : activeEndpoints) {
            if (endpoint.id == savedId) {
                return CString();
            }
            if (endpoint.name == savedName) {
                match = endpoint.id;
                matches++;
            }
        }

        return matches == 1 ? match : CString();
    }
}
