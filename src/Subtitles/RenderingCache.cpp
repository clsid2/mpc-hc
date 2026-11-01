/*
 * (C) 2013-2014, 2016-2017 see Authors.txt
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
#include "RenderingCache.h"
#include "RTS.h"
#include <cmath>

// Hash doubles with fractional resolution, or animated values like \fr12.3 and \fr12.4 collide and look like re-misses
static inline ULONG HashDouble(double x)
{
    return ULONG(std::llround(x * 1e3));
}

CTextDimsKey::CTextDimsKey(const CStringW& str, const STSStyle& style)
    : m_str(str)
    , m_style(DEBUG_NEW STSStyle(style))
{
    UpdateHash();
}

CTextDimsKey::CTextDimsKey(const CTextDimsKey& textDimsKey)
    : m_hash(textDimsKey.m_hash)
    , m_str(textDimsKey.m_str)
    , m_style(DEBUG_NEW STSStyle(*textDimsKey.m_style))
{
}

void CTextDimsKey::UpdateHash()
{
    m_hash  = CStringElementTraits<CString>::Hash(m_str);
    m_hash += m_hash << 5;
    m_hash += m_style->charSet;
    m_hash += m_hash << 5;
    m_hash += CStringElementTraits<CString>::Hash(m_style->fontName);
    m_hash += m_hash << 5;
    m_hash += HashDouble(m_style->fontSize);
    m_hash += m_hash << 5;
    m_hash += HashDouble(m_style->fontSpacing);
    m_hash += m_hash << 5;
    m_hash += m_style->fontWeight;
    m_hash += m_hash << 5;
    m_hash += m_style->fItalic;
    m_hash += m_hash << 5;
    m_hash += m_style->fUnderline;
    m_hash += m_hash << 5;
    m_hash += m_style->fStrikeOut;
}

bool CTextDimsKey::operator==(const CTextDimsKey& textDimsKey) const
{
    return m_str == textDimsKey.m_str
           && m_style->charSet == textDimsKey.m_style->charSet
           && m_style->fontName == textDimsKey.m_style->fontName
           && IsNearlyEqual(m_style->fontSize, textDimsKey.m_style->fontSize, 1e-6)
           && IsNearlyEqual(m_style->fontSpacing, textDimsKey.m_style->fontSpacing, 1e-6)
           && m_style->fontWeight == textDimsKey.m_style->fontWeight
           && m_style->fItalic == textDimsKey.m_style->fItalic
           && m_style->fUnderline == textDimsKey.m_style->fUnderline
           && m_style->fStrikeOut == textDimsKey.m_style->fStrikeOut;
}

CPolygonPathKey::CPolygonPathKey(const CStringW& str, double scalex, double scaley)
    : m_str(str)
    , m_scalex(scalex)
    , m_scaley(scaley)
{
    UpdateHash();
}

CPolygonPathKey::CPolygonPathKey(const CPolygonPathKey& polygonPathKey)
    : m_hash(polygonPathKey.m_hash)
    , m_str(polygonPathKey.m_str)
    , m_scalex(polygonPathKey.m_scalex)
    , m_scaley(polygonPathKey.m_scaley)
{
}

void CPolygonPathKey::UpdateHash()
{
    m_hash = CStringElementTraits<CStringW>::Hash(m_str);
    m_hash += m_hash << 5;
    m_hash += int(m_scalex * 1e6);
    m_hash += m_hash << 5;
    m_hash += int(m_scaley * 1e6);
}

bool CPolygonPathKey::operator==(const CPolygonPathKey& polygonPathKey) const
{
    return m_str == polygonPathKey.m_str
           && IsNearlyEqual(m_scalex, polygonPathKey.m_scalex, 1e-6)
           && IsNearlyEqual(m_scaley, polygonPathKey.m_scaley, 1e-6);
}

CFontKey::CFontKey(const LOGFONT& lf)
    : m_lf(lf)
{
    UpdateHash();
}

void CFontKey::UpdateHash()
{
    m_hash = 0;
    const BYTE* p = reinterpret_cast<const BYTE*>(&m_lf);
    for (size_t i = 0; i < sizeof(m_lf); i++) {
        m_hash += m_hash << 5;
        m_hash += p[i];
    }
}

COutlineKey::COutlineKey(const CWord* word, CPoint org)
    : CTextDimsKey(word->m_str, word->m_style)
    , m_scalex(word->m_scalex)
    , m_scaley(word->m_scaley)
    , m_org(org)
{
    UpdateHash();
}

COutlineKey::COutlineKey(const COutlineKey& outLineKey)
    : CTextDimsKey(outLineKey.m_str, *outLineKey.m_style)
    , m_hash(outLineKey.m_hash)
    , m_scalex(outLineKey.m_scalex)
    , m_scaley(outLineKey.m_scaley)
    , m_org(outLineKey.m_org)
{
}

void COutlineKey::UpdateHash()
{
    // CreatePath
    m_hash = __super::GetHash();
    m_hash += m_hash << 5;
    m_hash += int(m_scalex * 1e6);
    m_hash += m_hash << 5;
    m_hash += int(m_scaley * 1e6);
    m_hash += m_hash << 5;
    // Transform
    m_hash += HashDouble(m_style->fontScaleX);
    m_hash += m_hash << 5;
    m_hash += HashDouble(m_style->fontScaleY);
    m_hash += m_hash << 5;
    m_hash += HashDouble(m_style->fontAngleX);
    m_hash += m_hash << 5;
    m_hash += HashDouble(m_style->fontAngleY);
    m_hash += m_hash << 5;
    m_hash += HashDouble(m_style->fontAngleZ);
    m_hash += m_hash << 5;
    m_hash += HashDouble(m_style->fontShiftX);
    m_hash += m_hash << 5;
    m_hash += HashDouble(m_style->fontShiftY);
    m_hash += m_hash << 5;
    m_hash += m_org.x + (m_org.y << 16);
    m_hash += m_hash << 5;
    // CreateWidenedRegion
    m_hash += m_style->borderStyle;
    m_hash += m_hash << 5;
    m_hash += HashDouble(m_style->outlineWidthX);
    m_hash += m_hash << 5;
    m_hash += HashDouble(m_style->outlineWidthY);
    m_hash += m_hash << 5;
    m_hash += int(m_style->shadowDepthX);
    m_hash += m_hash << 5;
    m_hash += int(m_style->shadowDepthY);
}

bool COutlineKey::operator==(const COutlineKey& outLineKey) const
{
    return __super::operator==(outLineKey) // CreatePath
           && IsNearlyEqual(m_scalex, outLineKey.m_scalex, 1e-6)
           && IsNearlyEqual(m_scaley, outLineKey.m_scaley, 1e-6)
           // Transform
           && IsNearlyEqual(m_style->fontScaleX, outLineKey.m_style->fontScaleX, 1e-6)
           && IsNearlyEqual(m_style->fontScaleY, outLineKey.m_style->fontScaleY, 1e-6)
           && IsNearlyEqual(m_style->fontAngleX, outLineKey.m_style->fontAngleX, 1e-6)
           && IsNearlyEqual(m_style->fontAngleY, outLineKey.m_style->fontAngleY, 1e-6)
           && IsNearlyEqual(m_style->fontAngleZ, outLineKey.m_style->fontAngleZ, 1e-6)
           && IsNearlyEqual(m_style->fontShiftX, outLineKey.m_style->fontShiftX, 1e-6)
           && IsNearlyEqual(m_style->fontShiftY, outLineKey.m_style->fontShiftY, 1e-6)
           && m_org == outLineKey.m_org
           // CreateWidenedRegion
           && m_style->borderStyle == outLineKey.m_style->borderStyle
           && IsNearlyEqual(m_style->outlineWidthX, outLineKey.m_style->outlineWidthX, 1e-6)
           && IsNearlyEqual(m_style->outlineWidthY, outLineKey.m_style->outlineWidthY, 1e-6);
}

COverlayKey::COverlayKey(const CWord* word, CPoint p, CPoint org)
    : COutlineKey(word, CPoint(org.x - p.x, org.y - p.y))
    , m_subp(p.x & 7, p.y & 7)
{
    UpdateHash();
}

COverlayKey::COverlayKey(const COverlayKey& overlayKey)
    : COutlineKey(overlayKey)
    , m_subp(overlayKey.m_subp)
    , m_hash(overlayKey.m_hash)
{
}

void COverlayKey::UpdateHash()
{
    m_hash = __super::GetHash();
    m_hash += m_hash << 5;
    m_hash += m_subp.x + (m_subp.y << 16);
    m_hash += m_hash << 5;
    m_hash += m_style->fBlur;
    m_hash += m_hash << 5;
    m_hash += HashDouble(m_style->fGaussianBlur);
}

bool COverlayKey::operator==(const COverlayKey& overlayKey) const
{
    return __super::operator==(overlayKey)
           && m_subp == overlayKey.m_subp
           && m_style->fBlur == overlayKey.m_style->fBlur
           && IsNearlyEqual(m_style->fGaussianBlur, overlayKey.m_style->fGaussianBlur, 1e-6);
}

CClipperKey::CClipperKey(const std::shared_ptr<CClipper>& clipper)
    : m_clipper(clipper)
{
    UpdateHash();
}

void CClipperKey::UpdateHash()
{
    m_hash = m_clipper->Hash();
}

bool CClipperKey::operator==(const CClipperKey& clipperKey) const
{
    if (m_clipper == clipperKey.m_clipper) {
        return true;
    }
    if (m_clipper && clipperKey.m_clipper) {
        return *m_clipper == *clipperKey.m_clipper;
    }
    return false;
}
