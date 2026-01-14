/*
 * (C) 2003-2006 Gabest
 * (C) 2006-2013, 2015 see Authors.txt
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
#include "FloatEdit.h"


// CFloatEdit

IMPLEMENT_DYNAMIC(CMPCThemeFloatEdit, CMPCThemeEdit)

bool CMPCThemeFloatEdit::GetFloat(float& f)
{
    CString s;
    GetWindowText(s);
    return (_stscanf_s(s, _T("%f"), &f) == 1);
}

double CMPCThemeFloatEdit::operator = (double d)
{
    CString s;
    s.Format(_T("%.4f"), d);
    SetWindowText(s);
    return d;
}

CMPCThemeFloatEdit::operator double()
{
    CString s;
    GetWindowText(s);
    float flt;
    if (swscanf_s(s, L"%f", &flt) != 1) {
        flt = 0.0f;
    }
    flt = std::clamp(flt, m_lower, m_upper);

    return flt;
}

void CMPCThemeFloatEdit::SetRange(float fLower, float fUpper)
{
    ASSERT(fLower < fUpper);
    m_lower = fLower;
    m_upper = fUpper;
}

BEGIN_MESSAGE_MAP(CMPCThemeFloatEdit, CMPCThemeEdit)
    ON_WM_CHAR()
END_MESSAGE_MAP()

void CMPCThemeFloatEdit::OnChar(UINT nChar, UINT nRepCnt, UINT nFlags)
{
    if (!(nChar >= '0' && nChar <= '9' || nChar == '.' || nChar == '\b' || nChar == '-')) {
        return;
    }

    if (nChar == '-' && m_lower >= 0) {
        return;
    }

    CString str;
    GetWindowText(str);

    if (nChar == '.' && (str.Find('.') >= 0 || str.IsEmpty())) {
        return;
    }

    int nStartChar, nEndChar;
    GetSel(nStartChar, nEndChar);

    if (nChar == '\b' && nStartChar <= 0) {
        return;
    }

    CMPCThemeEdit::OnChar(nChar, nRepCnt, nFlags);
}

// CIntEdit

IMPLEMENT_DYNAMIC(CMPCThemeIntEdit, CMPCThemeEdit)

BEGIN_MESSAGE_MAP(CMPCThemeIntEdit, CMPCThemeEdit)
    ON_WM_CHAR()
END_MESSAGE_MAP()

void CMPCThemeIntEdit::OnChar(UINT nChar, UINT nRepCnt, UINT nFlags)
{
    if (!(nChar >= '0' && nChar <= '9' || nChar == '-' || nChar == '\b')) {
        return;
    }

    int nStartChar, nEndChar;
    GetSel(nStartChar, nEndChar);

    if (nChar == '-' && nEndChar == 0) {
        CString str;
        GetWindowText(str);
        if (!str.IsEmpty() && str[0] == '-') {
            return;
        }
    }

    if (nChar == '-' && nStartChar != 0) {
        return;
    }

    CMPCThemeEdit::OnChar(nChar, nRepCnt, nFlags);
}

// CHexEdit

IMPLEMENT_DYNAMIC(CMPCThemeHexEdit, CMPCThemeEdit)

bool CMPCThemeHexEdit::GetDWORD(DWORD& dw)
{
    CString s;
    GetWindowText(s);
    return (_stscanf_s(s, _T("%lx"), &dw) == 1);
}

DWORD CMPCThemeHexEdit::operator = (DWORD dw)
{
    CString s;
    s.Format(_T("%08lx"), dw);
    SetWindowText(s);
    return dw;
}

CMPCThemeHexEdit::operator DWORD()
{
    CString s;
    GetWindowText(s);
    DWORD dw;
    return (_stscanf_s(s, _T("%lx"), &dw) == 1 ? dw : 0);
}

BEGIN_MESSAGE_MAP(CMPCThemeHexEdit, CMPCThemeEdit)
    ON_WM_CHAR()
END_MESSAGE_MAP()

void CMPCThemeHexEdit::OnChar(UINT nChar, UINT nRepCnt, UINT nFlags)
{
    if (!(nChar >= 'A' && nChar <= 'F' || nChar >= 'a' && nChar <= 'f'
            || nChar >= '0' && nChar <= '9' || nChar == '\b')) {
        return;
    }

    CString str;
    GetWindowText(str);

    int nStartChar, nEndChar;
    GetSel(nStartChar, nEndChar);

    if (nChar == '\b' && nStartChar <= 0) {
        return;
    }

    if (nChar != '\b' && nEndChar - nStartChar == 0 && str.GetLength() >= 8) {
        return;
    }

    CMPCThemeEdit::OnChar(nChar, nRepCnt, nFlags);
}

// CDynamicEdit

IMPLEMENT_DYNAMIC(CMPCThemeDynamicEdit, CMPCThemeEdit)

void CMPCThemeDynamicEdit::SetMode(Mode mode)
{
    m_mode = mode;
}

void CMPCThemeDynamicEdit::SetIntRange(int lower, int upper)
{
    ASSERT(lower <= upper);
    m_intRange = std::make_pair(lower, upper);
}

BEGIN_MESSAGE_MAP(CMPCThemeDynamicEdit, CMPCThemeEdit)
    ON_WM_CHAR()
END_MESSAGE_MAP()

void CMPCThemeDynamicEdit::OnChar(UINT nChar, UINT nRepCnt, UINT nFlags)
{
    if (m_mode == Mode::INT) {
        // Allow only: digits, minus sign, backspace
        if (!(nChar >= '0' && nChar <= '9' || nChar == '-' || nChar == '\b')) {
            return;
        }

        int nStartChar, nEndChar;
        GetSel(nStartChar, nEndChar);

        // Handle minus sign validation
        if (nChar == '-') {
            // Check if range allows negative values
            if (m_intRange.first >= 0) {
                return; // Range doesn't support negative values
            }

            // Minus sign can only be at position 0
            if (nStartChar != 0) {
                return;
            }

            // Check if there's already a minus sign at position 0
            if (nEndChar == 0) {
                CString str;
                GetWindowText(str);
                if (!str.IsEmpty() && str[0] == '-') {
                    return; // Already has minus sign
                }
            }
        }

        if (nChar == '\b' && nStartChar <= 0) {
            return;
        }
    }
    // Mode::STRING allows all characters through

    CMPCThemeEdit::OnChar(nChar, nRepCnt, nFlags);
}
