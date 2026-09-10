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

// Normally MPCVR subclasses the top-level ancestor of its parent window to
// receive the messages it needs (hotkeys, exclusive mode handling, ...).
// The player can disable that subclassing by calling "DisableSubclassing"
// right after creating the filter and before the input pin is connected.
// It must then forward the messages from its own WindowProc by calling
// "WindowProcFromParent", from the same window MPCVR would otherwise have
// subclassed. If "WindowProcFromParent" returns "TRUE", consider the message
// handled by MPCVR: do not process it further, just return the value written
// to "result". If it returns "FALSE", process the message as usual.

interface __declspec(uuid("79C92864-E293-445B-90C7-2816E0508790"))
    IMPCVRSubclassReplacement :
    public IUnknown
{
    STDMETHOD_(void, DisableSubclassing)() PURE; // must be called before the input pin is connected
    STDMETHOD_(bool, WindowProcFromParent)(HWND hwnd, UINT uMsg, WPARAM* wParam, LPARAM* lParam, LRESULT* result) PURE;
};
