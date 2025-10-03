/*
 * (C) 2003-2006 Gabest
 * (C) 2006-2016 see Authors.txt
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
#include "USFSubtitles.h"
#include <MsXml.h>
#include <algorithm>

#define DeclareNameAndValue(pNode, name, val) \
    CComBSTR name;                            \
    pNode->get_nodeName(&name);               \
    name.ToLower();                           \
    CComVariant val;                          \
    pNode->get_nodeValue(&val);

#define BeginEnumAttribs(pNode, pChild)                                                               \
    {                                                                                                 \
        CComPtr<IXMLDOMNamedNodeMap> pAttribs;                                                        \
        if (SUCCEEDED(pNode->get_attributes(&pAttribs)) && pAttribs != nullptr) {                     \
            CComPtr<IXMLDOMNode> pChild;                                                              \
            for (pAttribs->nextNode(&pChild); pChild; pChild = nullptr, pAttribs->nextNode(&pChild)) {

#define EndEnumAttribs }}}

#define BeginEnumChildren(pNode, pChild)                                                                                \
    {                                                                                                                   \
        CComPtr<IXMLDOMNode> pChild, pChild##Next;                                                                      \
        for (pNode->get_firstChild(&pChild); pChild; pChild##Next = nullptr, pChild->get_nextSibling(&pChild##Next), pChild = pChild##Next) {

#define EndEnumChildren }}

static CStringW GetText(CComPtr<IXMLDOMNode> pNode)
{
    CComBSTR bstr;
    pNode->get_text(&bstr);

    return CStringW(bstr);
}

static CStringW GetXML(CComPtr<IXMLDOMNode> pNode)
{
    CComBSTR bstr;
    pNode->get_xml(&bstr);
    CStringW str(bstr);
    str.Remove('\r');
    str.Replace('\n', ' ');

    for (int i = 0; (i = str.Find(L" ", i)) >= 0;) {
        for (++i; i < str.GetLength() && (str[i] == ' ');) {
            str.Delete(i);
        }
    }
    return str;
}

static CStringW GetAttrib(CStringW attrib, CComPtr<IXMLDOMNode> pNode)
{
    CStringW ret;

    BeginEnumAttribs(pNode, pChild) {
        DeclareNameAndValue(pChild, name, val);

        if (CStringW(name) == attrib && val.vt == VT_BSTR) { // TODO: prepare for other types
            ret = val.bstrVal;
            break;
        }
    }
    EndEnumAttribs;

    return ret;
}

static int TimeToInt(CStringW str)
{
    CAtlList<CStringW> sl;
    int i = 0;
    for (CStringW token = str.Tokenize(L":.,", i); !token.IsEmpty(); token = str.Tokenize(L":.,", i)) {
        sl.AddHead(token);
    }

    if (sl.GetCount() > 4) {
        return -1;
    }

    int time = 0;
    int mul[4] = {1, 1000, 60 * 1000, 60 * 60 * 1000};
    POSITION pos = sl.GetHeadPosition();

    for (i = 0; pos; i++) {
        const WCHAR* s = sl.GetNext(pos);
        WCHAR* tmp = nullptr;
        int t = wcstol(s, &tmp, 10);
        if (s >= tmp) {
            return -1;
        }
        time += t * mul[i];
    }

    return time;
}

static DWORD StringToDWORD(CStringW str)
{
    if (str.IsEmpty()) {
        return 0;
    }
    if (str[0] == '#') {
        return (DWORD)wcstol(str, nullptr, 16);
    } else {
        return (DWORD)wcstol(str, nullptr, 10);
    }
}

static DWORD ColorToDWORD(CStringW str)
{
    if (str.IsEmpty()) {
        return 0;
    }

    DWORD ret = 0;

    if (str[0] == '#') {
        ret = (DWORD)wcstol(str.TrimLeft('#'), nullptr, 16);
    } else {
        g_colors.Lookup(CString(str), ret);
    }

    ret = ((ret & 0xff) << 16) | (ret & 0xff00ff00) | ((ret >> 16) & 0xff);

    return ret;
}

static int TranslateAlignment(CStringW alignment)
{
    return
        !alignment.CompareNoCase(L"BottomLeft") ? 1 :
        !alignment.CompareNoCase(L"BottomCenter") ? 2 :
        !alignment.CompareNoCase(L"BottomRight") ? 3 :
        !alignment.CompareNoCase(L"MiddleLeft") ? 4 :
        !alignment.CompareNoCase(L"MiddleCenter") ? 5 :
        !alignment.CompareNoCase(L"MiddleRight") ? 6 :
        !alignment.CompareNoCase(L"TopLeft") ? 7 :
        !alignment.CompareNoCase(L"TopCenter") ? 8 :
        !alignment.CompareNoCase(L"TopRight") ? 9 :
        2;
}

static int TranslateMargin(CStringW margin, int wndsize)
{
    int ret = 0;

    if (!margin.IsEmpty()) {
        ret = wcstol(margin, nullptr, 10);
        if (margin.Find('%') >= 0) {
            ret = wndsize * ret / 100;
        }
    }

    return ret;
}

////////////////

CUSFSubtitles::CUSFSubtitles()
{
}

CUSFSubtitles::~CUSFSubtitles()
{
}

bool CUSFSubtitles::Read(LPCTSTR fn)
{
    VARIANT_BOOL vb;
    CComPtr<IXMLDOMDocument> pDoc;
    if (FAILED(pDoc.CoCreateInstance(CLSID_DOMDocument))
            || FAILED(pDoc->put_async(VARIANT_FALSE))
            || FAILED(pDoc->load(CComVariant(fn), &vb)) || vb != VARIANT_TRUE) {
        return false;
    }

    styles.RemoveAll();
    effects.RemoveAll();
    texts.RemoveAll();

    if (!ParseUSFSubtitles(CComQIPtr<IXMLDOMNode>(pDoc))) {
        return false;
    }

    POSITION pos = styles.GetHeadPosition();
    while (pos) {
        style_t* def = styles.GetNext(pos);

        if (def->name.CompareNoCase(L"Default")) {
            continue;
        }

        POSITION pos2 = styles.GetHeadPosition();
        while (pos2) {
            style_t* s = styles.GetNext(pos2);

            if (!s->name.CompareNoCase(L"Default")) {
                continue;
            }

            if (s->fontstyle.face.IsEmpty()) {
                s->fontstyle.face = def->fontstyle.face;
            }
            if (s->fontstyle.size.IsEmpty()) {
                s->fontstyle.size = def->fontstyle.size;
            }
            if (s->fontstyle.color[0].IsEmpty()) {
                s->fontstyle.color[0] = def->fontstyle.color[0];
            }
            if (s->fontstyle.color[1].IsEmpty()) {
                s->fontstyle.color[1] = def->fontstyle.color[1];
            }
            if (s->fontstyle.color[2].IsEmpty()) {
                s->fontstyle.color[2] = def->fontstyle.color[2];
            }
            if (s->fontstyle.color[3].IsEmpty()) {
                s->fontstyle.color[3] = def->fontstyle.color[3];
            }
            if (s->fontstyle.italic.IsEmpty()) {
                s->fontstyle.italic = def->fontstyle.italic;
            }
            if (s->fontstyle.weight.IsEmpty()) {
                s->fontstyle.weight = def->fontstyle.weight;
            }
            if (s->fontstyle.underline.IsEmpty()) {
                s->fontstyle.underline = def->fontstyle.underline;
            }
            if (s->fontstyle.alpha.IsEmpty()) {
                s->fontstyle.alpha = def->fontstyle.alpha;
            }
            if (s->fontstyle.outline.IsEmpty()) {
                s->fontstyle.outline = def->fontstyle.outline;
            }
            if (s->fontstyle.shadow.IsEmpty()) {
                s->fontstyle.shadow = def->fontstyle.shadow;
            }
            if (s->fontstyle.wrap.IsEmpty()) {
                s->fontstyle.wrap = def->fontstyle.wrap;
            }
            if (s->pal.alignment.IsEmpty()) {
                s->pal.alignment = def->pal.alignment;
            }
            if (s->pal.relativeto.IsEmpty()) {
                s->pal.relativeto = def->pal.relativeto;
            }
            if (s->pal.horizontal_margin.IsEmpty()) {
                s->pal.horizontal_margin = def->pal.horizontal_margin;
            }
            if (s->pal.vertical_margin.IsEmpty()) {
                s->pal.vertical_margin = def->pal.vertical_margin;
            }
            if (s->pal.rotate[0].IsEmpty()) {
                s->pal.rotate[0] = def->pal.rotate[0];
            }
            if (s->pal.rotate[1].IsEmpty()) {
                s->pal.rotate[1] = def->pal.rotate[1];
            }
            if (s->pal.rotate[2].IsEmpty()) {
                s->pal.rotate[2] = def->pal.rotate[2];
            }
        }

        break;
    }

    pos = texts.GetHeadPosition();
    while (pos) {
        text_t* t = texts.GetNext(pos);
        if (t->style.IsEmpty()) {
            t->style = L"Default";
        }
    }

    return true;
}

bool CUSFSubtitles::ConvertToSTS(CSimpleTextSubtitle& sts)
{
    sts.m_name = metadata.language.text;
    sts.m_encoding = CTextFile::UTF8;
    sts.m_playRes = CSize(640, 480);
    sts.m_scaledBAS = sts.m_scaledBAS2 = 0;
    sts.m_defaultWrapStyle = 1;

    // TODO: map metadata.language.code to charset num (windows doesn't have such a function...)
    int charSet = DEFAULT_CHARSET;

    POSITION pos = styles.GetHeadPosition();
    while (pos) {
        style_t* s = styles.GetNext(pos);

        if (!s->name.CompareNoCase(L"Default") && !s->fontstyle.wrap.IsEmpty()) {
            sts.m_defaultWrapStyle =
                !s->fontstyle.wrap.CompareNoCase(L"no") ? 2 :
                /*!s->fontstyle.wrap.CompareNoCase(L"auto") ? 1 :*/
                1;
        }

        STSStyle* stss = DEBUG_NEW STSStyle;
        if (!stss) {
            continue;
        }

        if (!s->pal.horizontal_margin.IsEmpty()) {
            stss->marginRect.left = stss->marginRect.right = TranslateMargin(s->pal.horizontal_margin, sts.m_playRes.cx);
        }
        if (!s->pal.vertical_margin.IsEmpty()) {
            stss->marginRect.top = stss->marginRect.bottom = TranslateMargin(s->pal.vertical_margin, sts.m_playRes.cy);
        }

        stss->scrAlignment = TranslateAlignment(s->pal.alignment);

        if (!s->pal.relativeto.IsEmpty()) stss->relativeTo =
                !s->pal.relativeto.CompareNoCase(L"window") ? STSStyle::WINDOW :
                !s->pal.relativeto.CompareNoCase(L"video") ? STSStyle::VIDEO :
                STSStyle::WINDOW;

        stss->borderStyle = 0;
        if (!s->fontstyle.outline.IsEmpty()) {
            stss->outlineWidthX = stss->outlineWidthY = wcstol(s->fontstyle.outline, nullptr, 10);
        }
        if (!s->fontstyle.shadow.IsEmpty()) {
            stss->shadowDepthX = stss->shadowDepthY = wcstol(s->fontstyle.shadow, nullptr, 10);
        }

        for (size_t i = 0; i < 4; i++) {
            DWORD color = ColorToDWORD(s->fontstyle.color[i]);
            auto alpha = (BYTE)wcstol(s->fontstyle.alpha, nullptr, 10);

            stss->colors[i] = color & 0xffffff;
            stss->alpha[i] = (BYTE)(color >> 24);

            stss->alpha[i] = BYTE(stss->alpha[i] + (255 - stss->alpha[i]) * std::min(std::max(alpha, 0ui8), 100ui8) / 100);
        }

        if (!s->fontstyle.face.IsEmpty()) {
            stss->fontName = s->fontstyle.face;
        }
        if (!s->fontstyle.size.IsEmpty()) {
            stss->fontSize = wcstol(s->fontstyle.size, nullptr, 10);
        }
        if (!s->fontstyle.weight.IsEmpty()) stss->fontWeight =
                !s->fontstyle.weight.CompareNoCase(L"normal") ? FW_NORMAL :
                !s->fontstyle.weight.CompareNoCase(L"bold") ? FW_BOLD :
                !s->fontstyle.weight.CompareNoCase(L"lighter") ? FW_LIGHT :
                !s->fontstyle.weight.CompareNoCase(L"bolder") ? FW_SEMIBOLD :
                wcstol(s->fontstyle.weight, nullptr, 10);
        if (stss->fontWeight == 0) {
            stss->fontWeight = FW_NORMAL;
        }
        if (!s->fontstyle.italic.IsEmpty()) {
            stss->fItalic = s->fontstyle.italic.CompareNoCase(L"yes") == 0;
        }
        if (!s->fontstyle.underline.IsEmpty()) {
            stss->fUnderline = s->fontstyle.underline.CompareNoCase(L"yes") == 0;
        }
        if (!s->pal.rotate[0].IsEmpty()) {
            stss->fontAngleZ = wcstol(s->pal.rotate[0], nullptr, 10);
        }
        if (!s->pal.rotate[1].IsEmpty()) {
            stss->fontAngleX = wcstol(s->pal.rotate[1], nullptr, 10);
        }
        if (!s->pal.rotate[2].IsEmpty()) {
            stss->fontAngleY = wcstol(s->pal.rotate[2], nullptr, 10);
        }

        stss->charSet = charSet;

        sts.AddStyle(WToT(s->name), stss);
    }

    pos = texts.GetHeadPosition();
    while (pos) {
        text_t* t = texts.GetNext(pos);

        if (!t->pal.alignment.IsEmpty()) {
            CStringW s;
            s.Format(L"{\\an%d}", TranslateAlignment(t->pal.alignment));
            t->str = s + t->str;
        }

        CRect marginRect;
        marginRect.SetRectEmpty();

        if (!t->pal.horizontal_margin.IsEmpty()) {
            marginRect.left = marginRect.right = TranslateMargin(t->pal.horizontal_margin, sts.m_storageRes.cx);
        }
        if (!t->pal.vertical_margin.IsEmpty()) {
            marginRect.top = marginRect.bottom = TranslateMargin(t->pal.vertical_margin, sts.m_storageRes.cy);
        }

        WCHAR rtags[3][8] = {L"{\\rz%d}", L"{\\rx%d}", L"{\\ry%d}"};
        for (size_t i = 0; i < 3; i++) {
            if (int angle = wcstol(t->pal.rotate[i], nullptr, 10)) {
                CStringW str;
                str.Format(rtags[i], angle);
                t->str = str + t->str;
            }
        }

        if (t->style.CompareNoCase(L"Default") != 0) {
            POSITION pos2 = styles.GetHeadPosition();
            while (pos2) {
                style_t* s = styles.GetNext(pos2);
                if (s->name == t->style && !s->fontstyle.wrap.IsEmpty()) {
                    int WrapStyle =
                        !s->fontstyle.wrap.CompareNoCase(L"no") ? 2 :
                        /*!s->fontstyle.wrap.CompareNoCase(L"auto") ? 1 :*/
                        1;

                    if (WrapStyle != sts.m_defaultWrapStyle) {
                        CStringW str;
                        str.Format(L"{\\q%d}", WrapStyle);
                        t->str = str + t->str;
                    }

                    break;
                }
            }
        }

        // TODO: apply effects as {\t(..)} after usf's standard clearly defines them

        sts.Add(t->str, true, t->start, t->stop, WToT(t->style), _T(""), _T(""), marginRect);
    }

    return true;
}

bool CUSFSubtitles::ParseUSFSubtitles(CComPtr<IXMLDOMNode> pNode)
{
    DeclareNameAndValue(pNode, name, val);

    if (name == L"usfsubtitles") {
        // metadata

        BeginEnumChildren(pNode, pChild) {
            DeclareNameAndValue(pChild, childName, childVal);

            if (childName == L"metadata") {
                ParseMetadata(pChild, metadata);
            }
        }
        EndEnumChildren;

        // styles

        BeginEnumChildren(pNode, pChild) {
            DeclareNameAndValue(pChild, childName, childVal);

            if (childName == L"styles") {
                BeginEnumChildren(pChild, pGrandChild) { // :)
                    DeclareNameAndValue(pGrandChild, grandChildName, grandChildVal);

                    if (grandChildName == L"style") {
                        CAutoPtr<style_t> s(DEBUG_NEW style_t);
                        if (s) {
                            ParseStyle(pGrandChild, s);
                            styles.AddTail(s);
                        }
                    }
                }
                EndEnumChildren;
            }
        }
        EndEnumChildren;

        // effects

        BeginEnumChildren(pNode, pChild) {
            DeclareNameAndValue(pChild, childName, childVal);

            if (childName == L"effects") {
                BeginEnumChildren(pChild, pGrandChild) { // :)
                    DeclareNameAndValue(pGrandChild, grandChildName, grandChildVal);

                    if (grandChildName == L"effect") {
                        CAutoPtr<effect_t> e(DEBUG_NEW effect_t);
                        if (e) {
                            ParseEffect(pGrandChild, e);
                            effects.AddTail(e);
                        }
                    }
                }
                EndEnumChildren;
            }
        }
        EndEnumChildren;

        // subtitles

        BeginEnumChildren(pNode, pChild) {
            DeclareNameAndValue(pChild, childName, childVal);

            if (childName == L"subtitles") {
                BeginEnumChildren(pChild, pGrandChild) { // :)
                    DeclareNameAndValue(pGrandChild, grandChildName, grandChildVal);

                    if (grandChildName == L"subtitle") {
                        CStringW sstart = GetAttrib(L"start", pGrandChild);
                        CStringW sstop = GetAttrib(L"stop", pGrandChild);
                        CStringW sduration = GetAttrib(L"duration", pGrandChild);
                        if (sstart.IsEmpty() || (sstop.IsEmpty() && sduration.IsEmpty())) {
                            continue;
                        }

                        int start = TimeToInt(sstart);
                        int stop = !sstop.IsEmpty() ? TimeToInt(sstop) : (start + TimeToInt(sduration));

                        ParseSubtitle(pGrandChild, start, stop);
                    }
                }
                EndEnumChildren;
            }
        }
        EndEnumChildren;

        return true;
    }

    BeginEnumChildren(pNode, pChild) {
        if (ParseUSFSubtitles(pChild)) {
            return true;
        }
    }
    EndEnumChildren;

    return false;
}

void CUSFSubtitles::ParseMetadata(CComPtr<IXMLDOMNode> pNode, metadata_t& m)
{
    DeclareNameAndValue(pNode, name, val);

    if (name == L"title") {
        m.title = GetText(pNode);
    } else if (name == L"date") {
        m.date = GetText(pNode);
    } else if (name == L"comment") {
        m.comment = GetText(pNode);
    } else if (name == L"author") {
        BeginEnumChildren(pNode, pChild) {
            DeclareNameAndValue(pChild, childName, childVal);

            if (childName == L"name") {
                m.author.name = GetText(pChild);
            } else if (childName == L"email") {
                m.author.email = GetText(pChild);
            } else if (childName == L"url") {
                m.author.url = GetText(pChild);
            }
        }
        EndEnumChildren;

        return;
    } else if (name == L"language") {
        m.language.text = GetText(pNode);
        m.language.code = GetAttrib(L"code", pNode);
    } else if (name == L"languageext") {
        m.languageext.text = GetText(pNode);
        m.languageext.code = GetAttrib(L"code", pNode);
    }

    BeginEnumChildren(pNode, pChild) {
        ParseMetadata(pChild, metadata);
    }
    EndEnumChildren;
}

void CUSFSubtitles::ParseStyle(CComPtr<IXMLDOMNode> pNode, style_t* s)
{
    DeclareNameAndValue(pNode, name, val);

    if (name == L"style") {
        s->name = GetAttrib(L"name", pNode);
    } else if (name == L"fontstyle") {
        ParseFontstyle(pNode, s->fontstyle);
        return;
    } else if (name == L"position") {
        ParsePal(pNode, s->pal);
        return;
    }

    BeginEnumChildren(pNode, pChild) {
        ParseStyle(pChild, s);
    }
    EndEnumChildren;
}

void CUSFSubtitles::ParseFontstyle(CComPtr<IXMLDOMNode> pNode, fontstyle_t& fs)
{
    fs.face = GetAttrib(L"face", pNode);
    fs.size = GetAttrib(L"size", pNode);
    fs.color[0] = GetAttrib(L"color", pNode);
    fs.color[1] = GetAttrib(L"back-color", pNode);
    fs.color[2] = GetAttrib(L"outline-color", pNode);
    fs.color[3] = GetAttrib(L"shadow-color", pNode);
    fs.italic = GetAttrib(L"italic", pNode);
    fs.weight = GetAttrib(L"weight", pNode);
    fs.underline = GetAttrib(L"underline", pNode);
    fs.alpha = GetAttrib(L"alpha", pNode);
    fs.outline = GetAttrib(L"outline-level", pNode);
    fs.shadow = GetAttrib(L"shadow-level", pNode);
    fs.wrap = GetAttrib(L"wrap", pNode);
}

void CUSFSubtitles::ParsePal(CComPtr<IXMLDOMNode> pNode, posattriblist_t& pal)
{
    pal.alignment = GetAttrib(L"alignment", pNode);
    pal.relativeto = GetAttrib(L"relative-to", pNode);
    pal.horizontal_margin = GetAttrib(L"horizontal-margin", pNode);
    pal.vertical_margin = GetAttrib(L"vertical-margin", pNode);
    pal.rotate[0] = GetAttrib(L"rotate-z", pNode);
    pal.rotate[1] = GetAttrib(L"rotate-x", pNode);
    pal.rotate[2] = GetAttrib(L"rotate-y", pNode);
}

void CUSFSubtitles::ParseEffect(CComPtr<IXMLDOMNode> pNode, effect_t* e)
{
    DeclareNameAndValue(pNode, name, val);

    if (name == L"effect") {
        e->name = GetAttrib(L"name", pNode);
    } else if (name == L"keyframes") {
        BeginEnumChildren(pNode, pChild) {
            DeclareNameAndValue(pChild, childName, childVal);

            if (childName == L"keyframe") {
                CAutoPtr<keyframe_t> k(DEBUG_NEW keyframe_t);
                if (k) {
                    ParseKeyframe(pChild, k);
                    e->keyframes.AddTail(k);
                }
            }
        }
        EndEnumChildren;

        return;
    }

    BeginEnumChildren(pNode, pChild) {
        ParseEffect(pChild, e);
    }
    EndEnumChildren;
}

void CUSFSubtitles::ParseKeyframe(CComPtr<IXMLDOMNode> pNode, keyframe_t* k)
{
    DeclareNameAndValue(pNode, name, val);

    if (name == L"keyframe") {
        k->position = GetAttrib(L"position", pNode);
    } else if (name == L"fontstyle") {
        ParseFontstyle(pNode, k->fontstyle);
        return;
    } else if (name == L"position") {
        ParsePal(pNode, k->pal);
        return;
    }
}

void CUSFSubtitles::ParseSubtitle(CComPtr<IXMLDOMNode> pNode, int start, int stop)
{
    DeclareNameAndValue(pNode, name, val);

    if (name == L"text" || name == L"karaoke") {
        CAutoPtr<text_t> t(DEBUG_NEW text_t);
        if (t) {
            t->start = start;
            t->stop = stop;
            t->style = GetAttrib(L"style", pNode);
            t->effect = GetAttrib(L"effect", pNode);
            ParsePal(pNode, t->pal);
            ParseText(pNode, t->str);
            texts.AddTail(t);
        }

        return;
    }
    //  else if

    BeginEnumChildren(pNode, pChild) {
        ParseSubtitle(pChild, start, stop);
    }
    EndEnumChildren;
}

void CUSFSubtitles::ParseText(CComPtr<IXMLDOMNode> pNode, CStringW& str)
{
    DeclareNameAndValue(pNode, name, val);

    CStringW prefix, postfix;

    if (name == L"b") {
        prefix = L"{\\b1}";
        postfix = L"{\\b}";
    } else if (name == L"i") {
        prefix = L"{\\i1}";
        postfix = L"{\\i}";
    } else if (name == L"u") {
        prefix = L"{\\u1}";
        postfix = L"{\\u}";
    } else if (name == L"font") {
        fontstyle_t fs;
        ParseFontstyle(pNode, fs);

        if (!fs.face.IsEmpty()) {
            prefix += L"{\\fn" + fs.face + L"}";
            postfix += L"{\\fn}";
        }
        if (!fs.size.IsEmpty()) {
            prefix += L"{\\fs" + fs.size + L"}";
            postfix += L"{\\fs}";
        }
        if (!fs.outline.IsEmpty()) {
            prefix += L"{\\bord" + fs.outline + L"}";
            postfix += L"{\\bord}";
        }
        if (!fs.shadow.IsEmpty()) {
            prefix += L"{\\shad" + fs.shadow + L"}";
            postfix += L"{\\shad}";
        }

        for (size_t i = 0; i < 4; i++) {
            if (!fs.color[i].IsEmpty()) {
                CStringW s;
                s.Format(L"{\\%uc&H%06x&}", i + 1, ColorToDWORD(fs.color[i]));
                prefix += s;
                s.Format(L"{\\%uc}", i + 1);
                postfix += s;
            }
        }
    } else if (name == L"k") {
        int t = wcstol(GetAttrib(L"t", pNode), nullptr, 10);
        CStringW s;
        s.Format(L"{\\kf%d}", t / 10);
        str += s;
        return;
    } else if (name == L"br") {
        str += L"\\N";
        return;
    } else if (name == L"#text") {
        str += GetXML(pNode);
        return;
    }

    BeginEnumChildren(pNode, pChild) {
        CStringW s;
        ParseText(pChild, s);
        str += s;
    }
    EndEnumChildren;

    str = prefix + str + postfix;
}

void CUSFSubtitles::ParseShape(CComPtr<IXMLDOMNode> pNode)
{
    // no specs on this yet
}
