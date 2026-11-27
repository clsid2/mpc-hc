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

#include <string>

// The plain text of an ASS event's Text field, as libass stores it: override
// blocks {...} removed, \N and \n turned into a line break and \h into a
// space. A '{' with no closing '}' is kept as text. This is what the internal
// renderer's RemoveSSATags does with its own parse (STS.cpp), applied to the
// UTF-8 text of an ASS_Event, so a libass track gives the same text. Any other
// backslash is left as it is. Null gives an empty string.
std::string StripAssOverrideTags(const char* text);
