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

#include <cstdint>
#include <cstdio>
#include <string>

// Last player events, for crash reports. An aggregate, so a global instance
// sits in the data segment and is in the minidump. The caller locks.
struct CrashEventLog {
    static constexpr size_t Capacity = 64;

    struct Entry {
        uint32_t ms;
        uint16_t code;
        int32_t arg;
    };

    char tag[16];   // to find the buffer in a dump
    uint32_t total;
    Entry entries[Capacity];

    void Push(uint32_t ms, uint16_t code, int32_t arg)
    {
        entries[total++ % Capacity] = { ms, code, arg };
    }

    // Oldest first: "12.345 Name(arg); ...", arg only when nonzero.
    template <class NameFn>
    std::wstring Format(NameFn name) const
    {
        std::wstring out;
        wchar_t buf[64];
        size_t n = total < Capacity ? total : Capacity;
        for (size_t i = total - n; i < total; i++) {
            const Entry& e = entries[i % Capacity];
            int len = swprintf_s(buf, L"%u.%03u %s", e.ms / 1000, e.ms % 1000, name(e.code));
            if (len > 0 && e.arg) {
                swprintf_s(buf + len, 64 - len, L"(%d)", e.arg);
            }
            if (!out.empty()) {
                out += L"; ";
            }
            out += buf;
        }
        return out;
    }
};
