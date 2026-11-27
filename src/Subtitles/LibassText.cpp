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
#include "LibassText.h"
#include <cstring>

std::string StripAssOverrideTags(const char* text)
{
    std::string out;
    if (!text) {
        return out;
    }
    out.reserve(strlen(text));

    // '{', '}' and '\\' are ASCII, so walking UTF-8 byte by byte never splits
    // a character
    for (const char* p = text; *p; p++) {
        if (*p == '{') {
            if (const char* close = strchr(p + 1, '}')) {
                p = close;
                continue;
            }
            // unclosed: no later '{' can close either, so the rest is text,
            // line breaks included
            out += '{';
        } else if (*p == '\\' && (p[1] == 'N' || p[1] == 'n')) {
            out += '\n';
            p++;
        } else if (*p == '\\' && p[1] == 'h') {
            out += ' ';
            p++;
        } else {
            out += *p;
        }
    }
    return out;
}
