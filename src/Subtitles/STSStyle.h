/*
 * (C) 2003-2006 Gabest
 * (C) 2006-2017 see Authors.txt
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

#include <atlcoll.h>
#include <array>
#include "TextFile.h"

class STSStyle {
public:
    enum RelativeTo {
        WINDOW,
        VIDEO,
        AUTO // ~video for SSA/ASS, ~window for the rest
    };

    CRect      marginRect;             // measured from the sides
    int        scrAlignment;           // 1 - 9: as on the numpad, 0: default
    int        borderStyle;            // 0: outline, 1: opaque box
    double     outlineWidthX, outlineWidthY;
    double     shadowDepthX, shadowDepthY;
    std::array<COLORREF, 4> colors;    // usually: {primary, secondary, outline/background, shadow}
    std::array<BYTE, 4> alpha;
    int        charSet;
    CString    fontName;
    double     fontSize;               // height
    double     fontScaleX, fontScaleY; // percent
    double     fontSpacing;            // +/- pixels
    LONG       fontWeight;
    int        fItalic;
    int        fUnderline;
    int        fStrikeOut;
    int        fBlur;
    double     fGaussianBlur;
    double     fontAngleZ, fontAngleX, fontAngleY;
    double     fontShiftX, fontShiftY;

    RelativeTo relativeTo;
    bool hasAnsiStyleName;

#if USE_LIBASS
    // libass stuff
    bool       Kerning = false;
    bool       ScaledBorderAndShadow = false;
    CString    customTags;
#endif

    STSStyle();

    void SetDefault();

    bool operator == (const STSStyle& s) const;
    bool operator != (const STSStyle& s) const {
        return !(*this == s);
    };
    bool IsFontStyleEqual(const STSStyle& s) const;

    STSStyle& operator = (LOGFONT& lf);

    friend LOGFONTA& operator <<= (LOGFONTA& lfa, const STSStyle& s);
    friend LOGFONTW& operator <<= (LOGFONTW& lfw, const STSStyle& s);

    friend CString& operator <<= (CString& style, const STSStyle& s);
    friend STSStyle& operator <<= (STSStyle& s, const CString& style);
};

class CSTSStyleMap : public CAtlMap<CString, STSStyle*, CStringElementTraits<CString>>
{
public:
    CSTSStyleMap() {}
    virtual ~CSTSStyleMap() { Free(); }
    void Free();
};
