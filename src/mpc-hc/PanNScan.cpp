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
#include "PanNScan.h"
#include <algorithm>

namespace
{
    bool ParseNumber(const CString& token, double& value)
    {
        LPCTSTR begin = token.GetString();
        LPTSTR end = nullptr;
        double d = _tcstod(begin, &end);
        if (end == begin) {
            return false;
        }
        while (_istspace(*end)) {
            end++;
        }
        if (*end) {
            return false;
        }
        value = d;
        return true;
    }
}

namespace PanNScan
{
    bool ParseValues(const CString& str, Values& values)
    {
        double* const fields[] = { &values.posX, &values.posY, &values.zoomX, &values.zoomY };
        double parsed[_countof(fields)];

        int pos = 0, n = 0;
        for (CString token = str.Tokenize(_T(","), pos); pos >= 0; token = str.Tokenize(_T(","), pos), n++) {
            if (n == _countof(fields) || !ParseNumber(token, parsed[n])) {
                return false;
            }
        }
        if (n != _countof(fields)) {
            return false;
        }

        for (int i = 0; i < n; i++) {
            *fields[i] = parsed[i];
        }
        return true;
    }

    bool ParsePreset(const CString& str, CString& label, Values& values)
    {
        int comma = str.Find(_T(','));
        if (comma <= 0) {
            return false;
        }

        Values parsed;
        if (!ParseValues(str.Mid(comma + 1), parsed)) {
            return false;
        }

        label = str.Left(comma);
        values = parsed;
        return true;
    }

    CString FormatPreset(const CString& label, const Values& values)
    {
        CString str;
        str.Format(_T("%s,%.3f,%.3f,%.3f,%.3f"), label.GetString(), values.posX, values.posY, values.zoomX, values.zoomY);
        return str;
    }

    void Clamp(Values& values)
    {
        values.posX = std::clamp(values.posX, -0.5, 1.5);
        values.posY = std::clamp(values.posY, -0.5, 1.5);
        values.zoomX = std::clamp(values.zoomX, 0.2, 5.0);
        values.zoomY = std::clamp(values.zoomY, 0.2, 5.0);
    }
}
