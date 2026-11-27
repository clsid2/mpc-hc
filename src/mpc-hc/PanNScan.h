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

// Pan and scan preset strings and the limits on pan and zoom, kept free of
// the frame so the menu, the presets dialog and the unit tests share them.
// A preset is "label,posX,posY,zoomX,zoomY".
namespace PanNScan
{
    struct Values {
        double posX = 0.5;
        double posY = 0.5;
        double zoomX = 1.0;
        double zoomY = 1.0;
    };

    // Parses "posX,posY,zoomX,zoomY". Fails unless there are exactly four
    // fields and each is a number with nothing after it; on failure the
    // output is left untouched.
    bool ParseValues(const CString& str, Values& values);

    // Parses "label,posX,posY,zoomX,zoomY". Fails on an empty label and on
    // anything ParseValues rejects; on failure the outputs are left untouched.
    bool ParsePreset(const CString& str, CString& label, Values& values);

    CString FormatPreset(const CString& label, const Values& values);

    // Clamps position to [-0.5, 1.5] and zoom to [0.2, 5.0].
    void Clamp(Values& values);
}
