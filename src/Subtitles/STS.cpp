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

#include "stdafx.h"
#include "STS.h"
#include <atlbase.h>
#include <algorithm>

#include "RealTextParser.h"
#include <fstream>
#include "USFSubtitles.h"

#include "../DSUtil/PathUtils.h"
#include "../DSUtil/DSMPropertyBag.h"
#include "../DSUtil/DSUtil.h"
#include  <comutil.h>
#include <regex>
#include "LibassContext.h"
#include "../DSUtil/RegexUtil.h"
#include "../mpc-hc/SubtitlesProvidersUtils.h"
#include "../DSUtil/ISOLang.h"

struct htmlcolor {
    LPCTSTR name;
    DWORD  color;
}

static constexpr htmlcolors[] = {
    {_T("white"), 0xffffff},
    {_T("whitesmoke"), 0xf5f5f5},
    {_T("ghostwhite"), 0xf8f8ff},
    {_T("snow"), 0xfffafa},
    {_T("gainsboro"), 0xdcdcdc},
    {_T("lightgrey"), 0xd3d3d3},
    {_T("silver"), 0xc0c0c0},
    {_T("darkgray"), 0xa9a9a9},
    {_T("gray"), 0x808080},
    {_T("dimgray"), 0x696969},
    {_T("lightslategray"), 0x778899},
    {_T("slategray"), 0x708090},
    {_T("darkslategray"), 0x2f4f4f},
    {_T("black"), 0x000000},

    {_T("azure"), 0xf0ffff},
    {_T("aliceblue"), 0xf0f8ff},
    {_T("mintcream"), 0xf5fffa},
    {_T("honeydew"), 0xf0fff0},
    {_T("lightcyan"), 0xe0ffff},
    {_T("paleturqoise"), 0xafeeee},
    {_T("powderblue"), 0xb0e0e6},
    {_T("lightblue"), 0xadd8ed},
    {_T("lightsteelblue"), 0xb0c4de},
    {_T("skyblue"), 0x87ceeb},
    {_T("lightskyblue"), 0x87cefa},
    {_T("cyan"), 0x00ffff},
    {_T("aqua"), 0x00ff80},
    {_T("deepskyblue"), 0x00bfff},
    {_T("aquamarine"), 0x7fffd4},
    {_T("turquoise"), 0x40e0d0},
    {_T("darkturquoise"), 0x00ced1},
    {_T("lightseagreen"), 0x20b2aa},
    {_T("mediumturquoise"), 0x40e0dd},
    {_T("mediumaquamarine"), 0x66cdaa},
    {_T("cadetblue"), 0x5f9ea0},
    {_T("teal"), 0x008080},
    {_T("darkcyan"), 0x008b8b},
    {_T("comflowerblue"), 0x6495ed},
    {_T("dodgerblue"), 0x1e90ff},
    {_T("steelblue"), 0x4682b4},
    {_T("royalblue"), 0x4169e1},
    {_T("blue"), 0x0000ff},
    {_T("mediumblue"), 0x0000cd},
    {_T("mediumslateblue"), 0x7b68ee},
    {_T("slateblue"), 0x6a5acd},
    {_T("darkslateblue"), 0x483d8b},
    {_T("darkblue"), 0x00008b},
    {_T("midnightblue"), 0x191970},
    {_T("navy"), 0x000080},

    {_T("palegreen"), 0x98fb98},
    {_T("lightgreen"), 0x90ee90},
    {_T("mediumspringgreen"), 0x00fa9a},
    {_T("springgreen"), 0x00ff7f},
    {_T("chartreuse"), 0x7fff00},
    {_T("lawngreen"), 0x7cfc00},
    {_T("lime"), 0x00ff00},
    {_T("limegreen"), 0x32cd32},
    {_T("greenyellow"), 0xadff2f},
    {_T("yellowgreen"), 0x9acd32},
    {_T("darkseagreen"), 0x8fbc8f},
    {_T("mediumseagreen"), 0x3cb371},
    {_T("seagreen"), 0x2e8b57},
    {_T("olivedrab"), 0x6b8e23},
    {_T("forestgreen"), 0x228b22},
    {_T("green"), 0x008000},
    {_T("darkkhaki"), 0xbdb76b},
    {_T("olive"), 0x808000},
    {_T("darkolivegreen"), 0x556b2f},
    {_T("darkgreen"), 0x006400},

    {_T("floralwhite"), 0xfffaf0},
    {_T("seashell"), 0xfff5ee},
    {_T("ivory"), 0xfffff0},
    {_T("beige"), 0xf5f5dc},
    {_T("cornsilk"), 0xfff8dc},
    {_T("lemonchiffon"), 0xfffacd},
    {_T("lightyellow"), 0xffffe0},
    {_T("lightgoldenrodyellow"), 0xfafad2},
    {_T("papayawhip"), 0xffefd5},
    {_T("blanchedalmond"), 0xffedcd},
    {_T("palegoldenrod"), 0xeee8aa},
    {_T("khaki"), 0xf0eb8c},
    {_T("bisque"), 0xffe4c4},
    {_T("moccasin"), 0xffe4b5},
    {_T("navajowhite"), 0xffdead},
    {_T("peachpuff"), 0xffdab9},
    {_T("yellow"), 0xffff00},
    {_T("gold"), 0xffd700},
    {_T("wheat"), 0xf5deb3},
    {_T("orange"), 0xffa500},
    {_T("darkorange"), 0xff8c00},

    {_T("oldlace"), 0xfdf5e6},
    {_T("linen"), 0xfaf0e6},
    {_T("antiquewhite"), 0xfaebd7},
    {_T("lightsalmon"), 0xffa07a},
    {_T("darksalmon"), 0xe9967a},
    {_T("salmon"), 0xfa8072},
    {_T("lightcoral"), 0xf08080},
    {_T("indianred"), 0xcd5c5c},
    {_T("coral"), 0xff7f50},
    {_T("tomato"), 0xff6347},
    {_T("orangered"), 0xff4500},
    {_T("red"), 0xff0000},
    {_T("crimson"), 0xdc143c},
    {_T("firebrick"), 0xb22222},
    {_T("maroon"), 0x800000},
    {_T("darkred"), 0x8b0000},

    {_T("lavender"), 0xe6e6fe},
    {_T("lavenderblush"), 0xfff0f5},
    {_T("mistyrose"), 0xffe4e1},
    {_T("thistle"), 0xd8bfd8},
    {_T("pink"), 0xffc0cb},
    {_T("lightpink"), 0xffb6c1},
    {_T("palevioletred"), 0xdb7093},
    {_T("hotpink"), 0xff69b4},
    {_T("fuchsia"), 0xff00ee},
    {_T("magenta"), 0xff00ff},
    {_T("mediumvioletred"), 0xc71585},
    {_T("deeppink"), 0xff1493},
    {_T("plum"), 0xdda0dd},
    {_T("violet"), 0xee82ee},
    {_T("orchid"), 0xda70d6},
    {_T("mediumorchid"), 0xba55d3},
    {_T("mediumpurple"), 0x9370db},
    {_T("purple"), 0x9370db},
    {_T("blueviolet"), 0x8a2be2},
    {_T("darkviolet"), 0x9400d3},
    {_T("darkorchid"), 0x9932cc},

    {_T("tan"), 0xd2b48c},
    {_T("burlywood"), 0xdeb887},
    {_T("sandybrown"), 0xf4a460},
    {_T("peru"), 0xcd853f},
    {_T("goldenrod"), 0xdaa520},
    {_T("darkgoldenrod"), 0xb8860b},
    {_T("chocolate"), 0xd2691e},
    {_T("rosybrown"), 0xbc8f8f},
    {_T("sienna"), 0xa0522d},
    {_T("saddlebrown"), 0x8b4513},
    {_T("brown"), 0xa52a2a},
};

CHtmlColorMap::CHtmlColorMap()
{
    for (size_t i = 0; i < _countof(htmlcolors); i++) {
        SetAt(htmlcolors[i].name, htmlcolors[i].color);
    }
}

const CHtmlColorMap g_colors;

static CStringW SSAColorTag(CStringW arg, CStringW ctag = L"c") {
    DWORD val, color;
    if (g_colors.Lookup(CString(arg), val)) {
        color = (DWORD)val;
    } else if ((color = wcstol(arg, nullptr, 16)) == 0) {
        color = 0x00ffffff;    // default is white
    }
    CStringW tmp;
    tmp.Format(L"%02x%02x%02x", color & 0xff, (color >> 8) & 0xff, (color >> 16) & 0xff);
    return CStringW(L"{\\" + ctag + L"&H") + tmp + L"&}";
}

static std::wstring SSAColorTagCS(std::wstring arg, CStringW ctag = L"c") {
    CStringW _arg(arg.c_str());
    return SSAColorTag(_arg, ctag).GetString();
}

//

const BYTE CharSetList[] = {
    ANSI_CHARSET,
    DEFAULT_CHARSET,
    SHIFTJIS_CHARSET,
    HANGUL_CHARSET,
    GB2312_CHARSET,
    CHINESEBIG5_CHARSET,
    JOHAB_CHARSET,
    HEBREW_CHARSET,
    ARABIC_CHARSET,
    GREEK_CHARSET,
    TURKISH_CHARSET,
    VIETNAMESE_CHARSET,
    THAI_CHARSET,
    EASTEUROPE_CHARSET,
    RUSSIAN_CHARSET,
    BALTIC_CHARSET
};

const TCHAR* CharSetNames[] = {
    _T("ANSI"),
    _T("DEFAULT"),
    _T("SHIFTJIS"),
    _T("HANGUL"),
    _T("GB2312"),
    _T("CHINESEBIG5"),
    _T("JOHAB"),
    _T("HEBREW"),
    _T("ARABIC"),
    _T("GREEK"),
    _T("TURKISH"),
    _T("VIETNAMESE"),
    _T("THAI"),
    _T("EASTEUROPE"),
    _T("RUSSIAN"),
    _T("BALTIC"),
};

const int CharSetLen = _countof(CharSetList);

//

static size_t CountLines(CTextFile* f, ULONGLONG from, ULONGLONG to, CString s = _T("")) {
    size_t n = 0;
    f->Seek(from, CFile::begin);
    while (f->ReadString(s) && f->GetPosition() < to) {
        n++;
    }
    return n;
}

static int FindChar(CStringW str, WCHAR c, int pos, bool fUnicode, int CharSet)
{
    if (fUnicode) {
        return str.Find(c, pos);
    }

    int fStyleMod = 0;

    DWORD cp = CharSetToCodePage(CharSet);
    int OrgCharSet = CharSet;

    for (int i = 0, j = str.GetLength(), k; i < j; i++) {
        WCHAR c2 = str[i];

        if (IsDBCSLeadByteEx(cp, (BYTE)c2)) {
            i++;
        } else if (i >= pos) {
            if (c2 == c) {
                return i;
            }
        }

        if (c2 == '{') {
            fStyleMod++;
        } else if (fStyleMod > 0) {
            if (c2 == '}') {
                fStyleMod--;
            } else if (c2 == 'e' && i >= 3 && i < j - 1 && str.Mid(i - 2, 3) == L"\\fe") {
                CharSet = 0;
                for (k = i + 1; _istdigit(str[k]); k++) {
                    CharSet = CharSet * 10 + (str[k] - '0');
                }
                if (k == i + 1) {
                    CharSet = OrgCharSet;
                }

                cp = CharSetToCodePage(CharSet);
            }
        }
    }

    return -1;
}

static CStringW ToMBCS(CStringW str, DWORD CharSet)
{
    CStringW ret;

    DWORD cp = CharSetToCodePage(CharSet);

    for (int i = 0, j = str.GetLength(); i < j; i++) {
        WCHAR wc = str.GetAt(i);
        char c[8];

        int len;
        if ((len = WideCharToMultiByte(cp, 0, &wc, 1, c, 8, nullptr, nullptr)) > 0) {
            for (ptrdiff_t k = 0; k < len; k++) {
                ret += (WCHAR)(BYTE)c[k];
            }
        } else {
            ret += L'?';
        }
    }

    return ret;
}

static CStringW UnicodeSSAToMBCS(CStringW str, DWORD CharSet)
{
    CStringW ret;
    int OrgCharSet = CharSet;

    for (int j = 0; j < str.GetLength();) {
        j = str.Find('{', j);
        if (j >= 0) {
            ret += ToMBCS(str.Left(j), CharSet);
            str = str.Mid(j);

            j = str.Find('}');
            if (j < 0) {
                ret += ToMBCS(str, CharSet);
                break;
            } else {
                int k = str.Find(L"\\fe");
                if (k >= 0 && k < j) {
                    CharSet = 0;
                    int l = k + 3;
                    for (; _istdigit(str[l]); l++) {
                        CharSet = CharSet * 10 + (str[l] - '0');
                    }
                    if (l == k + 3) {
                        CharSet = OrgCharSet;
                    }
                }

                j++;

                ret += ToMBCS(str.Left(j), OrgCharSet);
                str = str.Mid(j);
                j = 0;
            }
        } else {
            ret += ToMBCS(str, CharSet);
            break;
        }
    }

    return ret;
}

static CStringW MBCSSSAToUnicode(CStringW str, int CharSet)
{
    CStringW ret;
    int OrgCharSet = CharSet;

    for (int j = 0; j < str.GetLength();) {
        j = FindChar(str, '{', 0, false, CharSet);

        if (j >= 0) {
            ret += ToUnicode(str.Left(j), CharSet);
            str = str.Mid(j);

            j = FindChar(str, '}', 0, false, CharSet);

            if (j < 0) {
                ret += ToUnicode(str, CharSet);
                break;
            } else {
                int k = str.Find(L"\\fe");
                if (k >= 0 && k < j) {
                    CharSet = 0;
                    int l = k + 3;
                    for (; _istdigit(str[l]); l++) {
                        CharSet = CharSet * 10 + (str[l] - '0');
                    }
                    if (l == k + 3) {
                        CharSet = OrgCharSet;
                    }
                }

                j++;

                ret += ToUnicode(str.Left(j), OrgCharSet);
                str = str.Mid(j);
                j = 0;
            }
        } else {
            ret += ToUnicode(str, CharSet);
            break;
        }
    }

    return ret;
}

static CStringW RemoveSSATags(CStringW str, bool fUnicode, int CharSet)
{
    str.Replace(L"{\\i1}", L"<i>");
    str.Replace(L"{\\i}", L"</i>");

    for (int i = 0, j; i < str.GetLength();) {
        if ((i = FindChar(str, '{', i, fUnicode, CharSet)) < 0) {
            break;
        }
        if ((j = FindChar(str, '}', i, fUnicode, CharSet)) < 0) {
            break;
        }
        str.Delete(i, j - i + 1);
    }

    str.Replace(L"\\N", L"\n");
    str.Replace(L"\\n", L"\n");
    str.Replace(L"\\h", L" ");

    return str;
}

//

static CStringW SubRipper2SSA(CStringW str)
{
    if (str.Find(L'<') >= 0) {
        str.Replace(L"<i>", L"{\\i1}");
        str.Replace(L"</i>", L"{\\i}");
        str.Replace(L"<b>", L"{\\b1}");
        str.Replace(L"</b>", L"{\\b}");
        str.Replace(L"<u>", L"{\\u1}");
        str.Replace(L"</u>", L"{\\u}");
    }
    return str;
}

CStringW WebVTTCueStrip(CStringW& str)
{
    CStringW cues;
    int p = str.Find(L'\n');
    if (p == 0) p = str.Find(L'\r');
    if (p > 0 && p < 6) { // check for optional cue id: https://w3c.github.io/webvtt/#webvtt-cue-identifier
        int cueid;
        WCHAR cr;
        int c = swscanf_s(str.Left(p), L"%d%c", &cueid, &cr, 1);
        if (c == 1 || c == 2 && cr == L'\r') {
            str.Delete(0, p + 1);
            p = str.Find(L'\n');
            if (p == 0) p = str.Find(L'\r');
        }
    }
    if (p > 0) {
        if (str.Left(6) == _T("align:") || str.Left(9) == _T("position:") || str.Left(9) == _T("vertical:") || str.Left(5) == _T("line:") || str.Left(5) == _T("size:")) {
            if (p > 1 && str[p - 1] == L'\r') {
                cues = str.Left(p - 1);
            } else {
                cues = str.Left(p);
            }
            str.Delete(0, p + 1);
        }
    }
    return cues;
}

using WebVTTcolorData = struct _WebVTTcolorData { std::wstring color; std::wstring bg; bool applied = false; };
using WebVTTcolorMap = std::map<std::wstring, WebVTTcolorData>;

static void WebVTT2SSA(CStringW& str, CStringW& cueTags, WebVTTcolorMap clrMap)
{

    std::vector<WebVTTcolorData> styleStack;
    auto applyStyle = [&styleStack, &str](std::wstring clr, std::wstring bg, int endTag, bool restoring=false) {
        std::wstring tags = L"";
        WebVTTcolorData previous;
        bool applied = false;
        if (styleStack.size() > 0 && !restoring) {
            auto tmp = styleStack.back();
            if (tmp.applied) {
                previous = tmp;
            }
        }
        if (clr != L"" && clr != previous.color) {
            tags += SSAColorTagCS(clr);
        }
        if (bg != L"" && bg != previous.bg) {
            tags += SSAColorTagCS(bg, L"3c");
        }
        if (tags.length() > 0) {
            if (-1 == endTag) {
                str = tags.c_str() + str;
                applied = true;
            } else if (str.Mid(endTag + 1,1) != "<") { //if we are about to open or close a tag, don't set the style yet, as it may change before formattable text arrives
                str = str.Left(endTag + 1) + tags.c_str() + str.Mid(endTag + 1);
                applied = true;
            }
        }
        if (!restoring) {
            styleStack.push_back({ clr, bg, applied }); //push current colors for restoring
        }
    };

    std::wstring clr = L"", bg = L"";
    if (clrMap.count(L"::cue")) { //default cue style
        WebVTTcolorData colorData = clrMap[L"::cue"];
        clr = colorData.color;
        bg = colorData.bg;
        applyStyle(clr, bg, -1);
    }

    int tagPos = str.Find(L"<");
    while (tagPos != std::wstring::npos) {
        int endTag = str.Find(L">", tagPos);
        if (endTag == std::wstring::npos) break;
        CStringW inner = str.Mid(tagPos + 1, endTag - tagPos - 1);
        if (inner.Find(L"/") == 0) { //close tag
            if (styleStack.size()>0) {//should always be true, unless poorly matched close tags in source
                styleStack.pop_back();
            }
            if (styleStack.size() > 0) {
                auto restoreStyle = styleStack[styleStack.size() - 1];
                clr = restoreStyle.color;
                bg = restoreStyle.bg;
                applyStyle(clr, bg, endTag, true);
            } else { //reset default style
                if (endTag + 1 != str.GetLength()) {
                    str = str.Left(endTag + 1) + L"{\\r}" + str.Mid(endTag + 1);
                }
                clr = L"";
                bg = L"";
            }
            tagPos = str.Find(L"<", endTag);
            continue;
        }

        int dotPos = inner.Find(L".");
        if (dotPos == std::wstring::npos) {//it's a simple tag, so we can apply a single style to it, if it exists
            if (clrMap.count(inner.GetString())) {
                WebVTTcolorData colorData = clrMap[inner.GetString()];
                clr = colorData.color;
                bg = colorData.bg;
            }
        } else { //could find multiple classes 
            RegexUtil::wregexResults results;
            std::wregex clsPattern(LR"((\.?[^\.]+))");
            RegexUtil::wstringMatch(clsPattern, (const wchar_t*)inner, results);
            if (results.size() > 1) {
                std::wstring type = results[0][0];

                for (auto iter = results.begin()+1; iter != results.end(); ++iter) { //loop through all classes--whichever is last gets precedence
                    std::wstring cls = (*iter)[0];
                    WebVTTcolorData colorData;
                    if (clrMap.count(type + cls)) {
                        colorData = clrMap[type + cls];
                    } else if (clrMap.count(cls)) {
                        colorData = clrMap[cls];
                    }
                    if (colorData.color != L"") {
                        clr = colorData.color;
                    }
                    if (colorData.bg != L"") {
                        bg = colorData.bg;
                    }
                }
            }
        }

        applyStyle(clr, bg, endTag);
        tagPos = str.Find(L"<",endTag);
    }

    if (str.Find(L'<') >= 0) {
        str.Replace(L"<i>", L"{\\i1}");
        str.Replace(L"</i>", L"{\\i}");
        str.Replace(L"<b>", L"{\\b1}");
        str.Replace(L"</b>", L"{\\b}");
        str.Replace(L"<u>", L"{\\u1}");
        str.Replace(L"</u>", L"{\\u}");
    }

    if (str.Find(L'<') >= 0) {
        std::wstring stdTmp(str);

        // remove tags we don't support
        stdTmp = std::regex_replace(stdTmp, std::wregex(L"<c[.\\w\\d]*>"), L"");
        stdTmp = std::regex_replace(stdTmp, std::wregex(L"</c[.\\w\\d]*>"), L"");
        stdTmp = std::regex_replace(stdTmp, std::wregex(L"<\\d\\d:\\d\\d:\\d\\d.\\d\\d\\d>"), L"");
        stdTmp = std::regex_replace(stdTmp, std::wregex(L"<v[ .][^>]*>"), L"");
        stdTmp = std::regex_replace(stdTmp, std::wregex(L"</v>"), L"");
        stdTmp = std::regex_replace(stdTmp, std::wregex(L"<lang[^>]*>"), L"");
        stdTmp = std::regex_replace(stdTmp, std::wregex(L"</lang>"), L"");
        str = stdTmp.c_str();
    }
    if (str.Find(L'&') >= 0) {
        str.Replace(L"&lt;", L"<");
        str.Replace(L"&gt;", L">");
        str.Replace(L"&nbsp;", L"\\h");
        str.Replace(L"&lrm;", L"");
        str.Replace(L"&rlm;", L"");
        str.Replace(L"&amp;", L"&");
    }

    if (!cueTags.IsEmpty()) {
        std::wstring stdTmp(cueTags);
        std::wregex alignRegex(L"align:(start|left|center|middle|end|right)");
        std::wsmatch match;

        if (std::regex_search(stdTmp, match, alignRegex)) {
            if (match[1] == L"start" || match[1] == L"left") {
                str = L"{\\an1}" + str;
            } else if (match[1] == L"center" || match[1] == L"middle") {
                str = L"{\\an2}" + str;
            } else {
                str = L"{\\an3}" + str;
            }
        }
    }
}

static void WebVTT2SSA(CStringW& str) {
    CStringW discard;
    WebVTTcolorMap discardMap;
    WebVTT2SSA(str, discard, discardMap);
}

static bool OpenVTT(CTextFile* file, CSimpleTextSubtitle& ret, int CharSet) {
    CStringW buff;
    file->ReadString(buff);
    if (buff.Left(6).Compare(L"WEBVTT") != 0) {
        return false;
    }

    auto readTimeCode = [](LPCWSTR str, int& hh, int& mm, int& ss, int& ms) {
        WCHAR sep;
        int c = swscanf_s(str, L"%d%c%d%c%d%c%d",
            &hh, &sep, 1, &mm, &sep, 1, &ss, &sep, 1, &ms);
        if (c == 5) {
            // Hours value is absent, shift read values
            ms = ss;
            ss = mm;
            mm = hh;
            hh = 0;
        }
        return (c == 5 || c == 7);
    };

    //default cue color classes: https://w3c.github.io/webvtt/#default-text-color
    WebVTTcolorMap cueColors = {
        {L".white", WebVTTcolorData({L"ffffff", L""})},
        {L".lime", WebVTTcolorData({L"00ff00", L""})},
        {L".cyan", WebVTTcolorData({L"00ffff", L""})},
        {L".red", WebVTTcolorData({L"ff0000", L""})},
        {L".yellow", WebVTTcolorData({L"ffff00", L""})},
        {L".magenta", WebVTTcolorData({L"ff00ff", L""})},
        {L".blue", WebVTTcolorData({L"0000ff", L""})},
        {L".black", WebVTTcolorData({L"000000", L""})},
        {L".bg_white", WebVTTcolorData({L"", L"ffffff"})},
        {L".bg_lime", WebVTTcolorData({L"", L"00ff00"})},
        {L".bg_cyan", WebVTTcolorData({L"", L"00ffff"})},
        {L".bg_red", WebVTTcolorData({L"", L"ff0000"})},
        {L".bg_yellow", WebVTTcolorData({L"", L"ffff00"})},
        {L".bg_magenta", WebVTTcolorData({L"", L"ff00ff"})},
        {L".bg_blue", WebVTTcolorData({L"", L"0000ff"})},
        {L".bg_black", WebVTTcolorData({L"", L"000000"})},
    };

    CStringW start, end, cueTags;

    auto parseStyle = [&file,&cueColors](CStringW& buff) {
        CStringW styleStr = L"";
        while (file->ReadString(buff)) {
            if (buff.Find(L"-->") != -1) { //not allowed in style block, so we drop out to cue parsing below
                FastTrimRight(buff);
                break;
            }
            if (buff.IsEmpty()) { //empty line not allowed in style block, drop out
                break;
            }
            styleStr += L" "+buff;
        }

        int startComment = styleStr.Find(L"/*");
        while (startComment != -1) { //remove comments
            int endComment = styleStr.Find(L"*/", startComment + 2);
            if (endComment == -1) {
                endComment = styleStr.GetLength()-1;
            }
            styleStr.Delete(startComment, endComment - startComment + 1);
            startComment = styleStr.Find(L"/*");
        }

        if (!styleStr.IsEmpty()) {
            auto parseColor = [](std::wstring styles, std::wstring attr = L"color") {
                //we only support color styles for now
                std::wregex clrPat(LR"(^\s*)" + attr + LR"(\s*:\s*#?([a-zA-Z0-9]*)\s*;)"); //e.g., 0xffffff or white
                std::wregex rgbPat(LR"(^\s*)" + attr + LR"(\s*:\s*rgb\s*\(\s*([0-9]+)\s*,\s*([0-9]+)\s*,\s*([0-9]+)\s*\)\s*;)");
                std::wsmatch match;
                std::wstring clrStr = L"";
                if (std::regex_search(styles, match, clrPat)) {
                    clrStr = match[1];
                } else if (std::regex_search(styles, match, rgbPat)) {
                    int r = stoi(match[1]) & 0xff;
                    int g = stoi(match[2]) & 0xff;
                    int b = stoi(match[3]) & 0xff;
                    DWORD clr = (r << 16) + (g << 8) + b;
                    std::wstringstream hexClr;
                    hexClr << std::hex << clr;
                    clrStr = hexClr.str();
                }
                return clrStr;
            };

            RegexUtil::wregexResults results;
            std::wregex cueDefPattern(LR"(::cue\s*\{([^}]*)\})"); //default cue style
            RegexUtil::wstringMatch(cueDefPattern, (const wchar_t*)styleStr, results);
            if (results.size() > 0) {
                auto iter = results[results.size() - 1];
                std::wstring clr, bgClr;
                clr = parseColor(iter[0]);
                bgClr = parseColor(iter[0], L"background-color");
                if (bgClr == L"") {
                    bgClr = parseColor(iter[0], L"background");
                }
                if (clr != L"" || bgClr != L"") {
                    cueColors[L"::cue"] = WebVTTcolorData({ clr, bgClr });
                }
            }

            std::wregex cuePattern(LR"(::cue\(([^)]+)\)\s*\{([^}]*)\})");
            RegexUtil::wstringMatch(cuePattern, (const wchar_t*)styleStr, results);
            for (const auto& iter : results) {
                std::wstring clr, bgClr;
                clr=parseColor(iter[1]);
                bgClr=parseColor(iter[1], L"background-color");
                if (bgClr == L"") {
                    bgClr = parseColor(iter[1], L"background");
                }
                if (clr != L"" || bgClr != L"") {
                    cueColors[iter[0]] = WebVTTcolorData({ clr, bgClr });
                }
            }
        }
    };

    CStringW lastStr, lastBuff;
    bool foundFirstCue = false;
    while (file->ReadString(buff)) {
        FastTrimRight(buff);
        if (!foundFirstCue && !buff.IsEmpty()) { //STYLE blocks cannot show up after cues begin
            if (buff == L"STYLE" || buff==L"Style:" /*have seen webvtt with incorrect format using 'Style:' instead of 'STYLE'*/ ) {
                parseStyle(buff); //note that buff will contain next line when done, so we can still use it below
            }
        }
        if (buff.IsEmpty()) {
            continue;
        }

        int len = buff.GetLength();
        cueTags = L"";
        int c = swscanf_s(buff, L"%s --> %s %[^\n]s", start.GetBuffer(len), len, end.GetBuffer(len), len, cueTags.GetBuffer(len), len);
        start.ReleaseBuffer();
        end.ReleaseBuffer();
        cueTags.ReleaseBuffer();

        int hh1, mm1, ss1, ms1, hh2, mm2, ss2, ms2;

        if ((c == 2 || c == 3) //either start/end or start/end/cuetags
            && readTimeCode(start, hh1, mm1, ss1, ms1)
            && readTimeCode(end, hh2, mm2, ss2, ms2)) {
            foundFirstCue = true;

            CStringW str, tmp;

            while (file->ReadString(tmp)) {
                FastTrimRight(tmp);
                if (tmp.IsEmpty()) {
                    break;
                }
                WebVTT2SSA(tmp, cueTags, cueColors);
                str += tmp + '\n';
            }

            if (lastStr != str || lastBuff != buff) { //discard repeated subs
                ret.Add(str,
                    file->IsUnicode(),
                    MS2RT((((hh1 * 60i64 + mm1) * 60i64) + ss1) * 1000i64 + ms1),
                    MS2RT((((hh2 * 60i64 + mm2) * 60i64) + ss2) * 1000i64 + ms2));
            }

            lastStr = str;
            lastBuff = buff;
        } else {
            continue;
        }
    }

    // in case of embedded data, we initially might only get the header, so always return true
    return true;
}


bool OpenSubRipper(CTextFile* file, CSimpleTextSubtitle& ret, int CharSet)
{
    CStringW buff, start, end;
    while (file->ReadString(buff)) {
        FastTrimRight(buff);
        if (buff.IsEmpty()) {
            continue;
        }

        int num = 0; // This one isn't really used just assigned a new value

        WCHAR wc;
        int c = swscanf_s(buff, L"%d%c", &num, &wc, 1);

        if (c == 2) { // c == 1 would be numbering, c == 2 might be timecodes
            int len = buff.GetLength();
            c = swscanf_s(buff, L"%s --> %s", start.GetBuffer(len), len, end.GetBuffer(len), len);
            start.ReleaseBuffer();
            end.ReleaseBuffer();

            auto readTimeCode = [](LPCWSTR str, int& hh, int& mm, int& ss, int& ms) {
                WCHAR sep;
                int c = swscanf_s(str, L"%d%c%d%c%d%c%d",
                                  &hh, &sep, 1, &mm, &sep, 1, &ss, &sep, 1, &ms);
                // Check if ms was present
                if (c == 5) {
                    ms = 0;
                }
                return (c == 5 || c == 7);
            };

            int hh1, mm1, ss1, ms1, hh2, mm2, ss2, ms2;

            if (c == 2
                    && readTimeCode(start, hh1, mm1, ss1, ms1)
                    && readTimeCode(end, hh2, mm2, ss2, ms2)) {
                CStringW str, tmp;

                bool bFoundEmpty = false;

                while (file->ReadString(tmp)) {
                    FastTrimRight(tmp);
                    if (tmp.IsEmpty()) {
                        bFoundEmpty = true;
                    }

                    int num2;
                    if (swscanf_s(tmp, L"%d%c", &num2, &wc, 1) == 1 && bFoundEmpty) {
                        num = num2;
                        break;
                    }

                    str += tmp + '\n';
                }

                ret.Add(SubRipper2SSA(str),
                        file->IsUnicode(),
                        MS2RT((((hh1 * 60i64 + mm1) * 60i64) + ss1) * 1000i64 + ms1),
                        MS2RT((((hh2 * 60i64 + mm2) * 60i64) + ss2) * 1000i64 + ms2));
            } else {
                return false;
            }
        } else if (c != 1) { // might be another format
            return false;
        }
    }

    return !ret.IsEmpty();
}

static bool OpenOldSubRipper(CTextFile* file, CSimpleTextSubtitle& ret, int CharSet)
{
    CStringW buff;
    while (file->ReadString(buff)) {
        FastTrim(buff);
        if (buff.IsEmpty()) {
            continue;
        }

        for (int i = 0; i < buff.GetLength(); i++) {
            if ((i = FindChar(buff, '|', i, file->IsUnicode(), CharSet)) < 0) {
                break;
            }
            buff.SetAt(i, '\n');
        }

        int hh1, mm1, ss1, hh2, mm2, ss2;
        int c = swscanf_s(buff, L"{%d:%d:%d}{%d:%d:%d}", &hh1, &mm1, &ss1, &hh2, &mm2, &ss2);

        if (c == 6) {
            ret.Add(
                buff.Mid(buff.Find('}', buff.Find('}') + 1) + 1),
                file->IsUnicode(),
                MS2RT((((hh1 * 60i64 + mm1) * 60i64) + ss1) * 1000i64),
                MS2RT((((hh2 * 60i64 + mm2) * 60i64) + ss2) * 1000i64));
        } else if (c != EOF) { // might be another format
            return false;
        }
    }

    return !ret.IsEmpty();
}

static bool OpenSubViewer(CTextFile* file, CSimpleTextSubtitle& ret, int CharSet)
{
    STSStyle def;
    CStringW font, color, size;
    bool fBold = false;
    bool fItalic = false;
    bool fStriked = false;
    bool fUnderline = false;
    CStringW buff;

    while (file->ReadString(buff)) {
        FastTrim(buff);
        if (buff.IsEmpty()) {
            continue;
        }

        if (buff[0] == '[') {
            for (int i = 0; i < buff.GetLength() && buff[i] == '[';) {
                int j = buff.Find(']', ++i);
                if (j < i) {
                    break;
                }

                CStringW tag = buff.Mid(i, j - i);
                FastTrim(tag);
                tag.MakeLower();

                i += j - i;

                j = buff.Find('[', ++i);
                if (j < 0) {
                    j = buff.GetLength();
                }

                CStringW param = buff.Mid(i, j - i);
                param.Trim(L" \\t,");

                i = j;

                if (tag == L"font") {
                    font = def.fontName.CompareNoCase(WToT(param)) ? param.GetString() : L"";
                } else if (tag == L"colf") {
                    color = def.colors[0] != (DWORD)wcstol(((LPCWSTR)param) + 2, 0, 16) ? param.GetString() : L"";
                } else if (tag == L"size") {
                    size = def.fontSize != (double)wcstol(param, 0, 10) ? param.GetString() : L"";
                } else if (tag == L"style") {
                    if (param.Find(L"no") >= 0) {
                        fBold = fItalic = fStriked = fUnderline = false;
                    } else {
                        fBold = def.fontWeight < FW_BOLD && param.Find(L"bd") >= 0;
                        fItalic = def.fItalic && param.Find(L"it") >= 0;
                        fStriked = def.fStrikeOut && param.Find(L"st") >= 0;
                        fUnderline = def.fUnderline && param.Find(L"ud") >= 0;
                    }
                }
            }

            continue;
        }

        WCHAR sep;
        int hh1, mm1, ss1, hs1, hh2, mm2, ss2, hs2;
        int c = swscanf_s(buff, L"%d:%d:%d%c%d,%d:%d:%d%c%d\n",
                          &hh1, &mm1, &ss1, &sep, 1,
                          &hs1, &hh2, &mm2, &ss2, &sep, 1, &hs2);

        if (c == 10) {
            CStringW str;
            VERIFY(file->ReadString(str));

            str.Replace(L"[br]", L"\\N");

            CStringW prefix;
            if (!font.IsEmpty()) {
                prefix += L"\\fn" + font;
            }
            if (!color.IsEmpty()) {
                prefix += L"\\c" + color;
            }
            if (!size.IsEmpty()) {
                prefix += L"\\fs" + size;
            }
            if (fBold) {
                prefix += L"\\b1";
            }
            if (fItalic) {
                prefix += L"\\i1";
            }
            if (fStriked) {
                prefix += L"\\s1";
            }
            if (fUnderline) {
                prefix += L"\\u1";
            }
            if (!prefix.IsEmpty()) {
                str = L"{" + prefix + L"}" + str;
            }

            ret.Add(str,
                    file->IsUnicode(),
                    MS2RT((((hh1 * 60i64 + mm1) * 60i64) + ss1) * 1000i64 + hs1 * 10i64),
                    MS2RT((((hh2 * 60i64 + mm2) * 60i64) + ss2) * 1000i64 + hs2 * 10i64));
        } else if (c != EOF) { // might be another format
            return false;
        }
    }

    return !ret.IsEmpty();
}

static STSStyle* GetMicroDVDStyle(CString str, int CharSet)
{
    STSStyle* ret = DEBUG_NEW STSStyle();
    if (!ret) {
        return nullptr;
    }

    for (int i = 0, len = str.GetLength(); i < len; i++) {
        int j = str.Find('{', i);
        if (j < 0) {
            j = len;
        }

        if (j >= len) {
            break;
        }

        int k = str.Find('}', j);
        if (k < 0) {
            k = len;
        }

        CString code = str.Mid(j, k - j);
        if (code.GetLength() > 2) {
            code.SetAt(1, (TCHAR)towlower(code[1]));
        }

        if (!_tcsnicmp(code, _T("{c:$"), 4)) {
            _stscanf_s(code, _T("{c:$%lx"), &ret->colors[0]);
        } else if (!_tcsnicmp(code, _T("{f:"), 3)) {
            ret->fontName = code.Mid(3);
        } else if (!_tcsnicmp(code, _T("{s:"), 3)) {
            double f;
            if (1 == _stscanf_s(code, _T("{s:%lf"), &f)) {
                ret->fontSize = f;
            }
        } else if (!_tcsnicmp(code, _T("{h:"), 3)) {
            _stscanf_s(code, _T("{h:%d"), &ret->charSet);
        } else if (!_tcsnicmp(code, _T("{y:"), 3)) {
            code.MakeLower();
            if (code.Find('b') >= 0) {
                ret->fontWeight = FW_BOLD;
            }
            if (code.Find('i') >= 0) {
                ret->fItalic = true;
            }
            if (code.Find('u') >= 0) {
                ret->fUnderline = true;
            }
            if (code.Find('s') >= 0) {
                ret->fStrikeOut = true;
            }
        } else if (!_tcsnicmp(code, _T("{p:"), 3)) {
            int p;
            _stscanf_s(code, _T("{p:%d"), &p);
            ret->scrAlignment = (p == 0) ? 8 : 2;
        }

        i = k;
    }

    return ret;
}

static CStringW MicroDVD2SSA(CStringW str, bool fUnicode, int CharSet)
{
    CStringW ret;

    enum {
        COLOR = 0,
        FONTNAME,
        FONTSIZE,
        FONTCHARSET,
        BOLD,
        ITALIC,
        UNDERLINE,
        STRIKEOUT
    };
    bool fRestore[8];
    int fRestoreLen = 8;

    ZeroMemory(fRestore, sizeof(bool)*fRestoreLen);

    for (int pos = 0, eol; pos < str.GetLength(); pos++) {
        if ((eol = FindChar(str, '|', pos, fUnicode, CharSet)) < 0) {
            eol = str.GetLength();
        }

        CStringW line = str.Mid(pos, eol - pos);

        pos = eol;

        for (int i = 0, j, k, len = line.GetLength(); i < len; i++) {
            if ((j = FindChar(line, '{', i, fUnicode, CharSet)) < 0) {
                j = str.GetLength();
            }

            ret += line.Mid(i, j - i);

            if (j >= len) {
                break;
            }

            if ((k = FindChar(line, '}', j, fUnicode, CharSet)) < 0) {
                k = len;
            }

            {
                CStringW code = line.Mid(j, k - j);

                if (!_wcsnicmp(code, L"{c:$", 4)) {
                    fRestore[COLOR] = (iswupper(code[1]) == 0);
                    code.MakeLower();

                    int color;
                    swscanf_s(code, L"{c:$%x", &color);
                    code.Format(L"{\\c&H%x&}", color);
                    ret += code;
                } else if (!_wcsnicmp(code, L"{f:", 3)) {
                    fRestore[FONTNAME] = (iswupper(code[1]) == 0);

                    code.Format(L"{\\fn%s}", code.Mid(3).GetString());
                    ret += code;
                } else if (!_wcsnicmp(code, L"{s:", 3)) {
                    fRestore[FONTSIZE] = (iswupper(code[1]) == 0);
                    code.MakeLower();

                    double size;
                    swscanf_s(code, L"{s:%lf", &size);
                    code.Format(L"{\\fs%f}", size);
                    ret += code;
                } else if (!_wcsnicmp(code, L"{h:", 3)) {
                    fRestore[COLOR] = (_istupper(code[1]) == 0);
                    code.MakeLower();

                    int iCharSet;
                    swscanf_s(code, L"{h:%d", &iCharSet);
                    code.Format(L"{\\fe%d}", iCharSet);
                    ret += code;
                } else if (!_wcsnicmp(code, L"{y:", 3)) {
                    bool f = (_istupper(code[1]) == 0);

                    code.MakeLower();

                    ret += L'{';
                    if (code.Find('b') >= 0) {
                        ret += L"\\b1";
                        fRestore[BOLD] = f;
                    }
                    if (code.Find('i') >= 0) {
                        ret += L"\\i1";
                        fRestore[ITALIC] = f;
                    }
                    if (code.Find('u') >= 0) {
                        ret += L"\\u1";
                        fRestore[UNDERLINE] = f;
                    }
                    if (code.Find('s') >= 0) {
                        ret += L"\\s1";
                        fRestore[STRIKEOUT] = f;
                    }
                    ret += L'}';
                } else if (!_wcsnicmp(code, L"{o:", 3)) {
                    code.MakeLower();

                    int x, y;
                    TCHAR c;
                    swscanf_s(code, L"{o:%d%c%d", &x, &c, 1, &y);
                    code.Format(L"{\\move(%d,%d,0,0,0,0)}", x, y);
                    ret += code;
                } else {
                    ret += code;
                }
            }

            i = k;
        }

        if (pos >= str.GetLength()) {
            break;
        }

        for (ptrdiff_t i = 0; i < fRestoreLen; i++) {
            if (fRestore[i]) {
                switch (i) {
                    case COLOR:
                        ret += L"{\\c}";
                        break;
                    case FONTNAME:
                        ret += L"{\\fn}";
                        break;
                    case FONTSIZE:
                        ret += L"{\\fs}";
                        break;
                    case FONTCHARSET:
                        ret += L"{\\fe}";
                        break;
                    case BOLD:
                        ret += L"{\\b}";
                        break;
                    case ITALIC:
                        ret += L"{\\i}";
                        break;
                    case UNDERLINE:
                        ret += L"{\\u}";
                        break;
                    case STRIKEOUT:
                        ret += L"{\\s}";
                        break;
                    default:
                        ASSERT(FALSE); // Shouldn't happen
                        break;
                }
            }
        }

        ZeroMemory(fRestore, sizeof(bool)*fRestoreLen);

        ret += L"\\N";
    }

    return ret;
}

static bool OpenMicroDVD(CTextFile* file, CSimpleTextSubtitle& ret, int CharSet)
{
    bool fCheck = false, fCheck2 = false;

    CString style(_T("Default"));

    CStringW buff;
    while (file->ReadString(buff)) {
        FastTrim(buff);
        if (buff.IsEmpty()) {
            continue;
        }

        LONGLONG start, end;
        int c = swscanf_s(buff, L"{%lld}{%lld}", &start, &end);

        if (c != 2) {
            c = swscanf_s(buff, L"{%lld}{}", &start) + 1;
            end = start + 60;
            fCheck = true;
        }

        if (c != 2) {
            int i;
            if (buff.Find('{') == 0 && (i = buff.Find('}')) > 1 && i < buff.GetLength()) {
                if (STSStyle* s = GetMicroDVDStyle(WToT(buff.Mid(i + 1)), CharSet)) {
                    style = buff.Mid(1, i - 1);
                    style.MakeUpper();
                    if (style.GetLength()) {
                        CString str = style.Mid(1);
                        str.MakeLower();
                        style = style.Left(1) + str;
                    }
                    ret.AddStyle(style, s);
                    CharSet = s->charSet;
                    continue;
                }
            }
        }

        if (c == 2) {
            if (fCheck2 && ret.GetCount()) {
                STSEntry& stse = ret[ret.GetCount() - 1];
                stse.end = std::min(stse.end, start);
                fCheck2 = false;
            }

            ret.Add(MicroDVD2SSA(buff.Mid(buff.Find('}', buff.Find('}') + 1) + 1), file->IsUnicode(), CharSet),
                    file->IsUnicode(), start, end, style);

            if (fCheck) {
                fCheck = false;
                fCheck2 = true;
            }
        } else if (c != EOF) { // might be another format
            return false;
        }
    }

    return !ret.IsEmpty();
}

static void ReplaceNoCase(CStringW& str, CStringW from, CStringW to)
{
    CStringW lstr = str;
    lstr.MakeLower();

    for (int i = 0, j = str.GetLength(); i < j;) {
        int k = -1;
        if ((k = lstr.Find(from, i)) >= 0) {
            str.Delete(k, from.GetLength());
            lstr.Delete(k, from.GetLength());
            str.Insert(k, to);
            lstr.Insert(k, to);
            i = k + to.GetLength();
            j = str.GetLength();
        } else {
            break;
        }
    }
}

static CStringW SMI2SSA(CStringW str, int CharSet)
{
    ReplaceNoCase(str, L"&nbsp;", L" ");
    ReplaceNoCase(str, L"&quot;", L"\"");
    ReplaceNoCase(str, L"<br>", L"\\N");
    ReplaceNoCase(str, L"<i>", L"{\\i1}");
    ReplaceNoCase(str, L"</i>", L"{\\i}");
    ReplaceNoCase(str, L"<b>", L"{\\b1}");
    ReplaceNoCase(str, L"</b>", L"{\\b}");

    CStringW lstr = str;
    lstr.MakeLower();

    // maven@maven.de
    // now parse line
    for (int i = 0, j = str.GetLength(); i < j;) {
        int k;
        if ((k = lstr.Find('<', i)) < 0) {
            break;
        }

        int chars_inserted = 0;

        int l = 1;
        for (; k + l < j && lstr[k + l] != '>'; l++) {
            ;
        }
        l++;

        // Modified by Cookie Monster
        if (lstr.Find(L"<font ", k) == k) {
            CStringW args = lstr.Mid(k + 6, l - 6); // delete "<font "
            CStringW arg;

            args.Remove('\"');
            args.Remove('#');   // may include 2 * " + #
            arg.TrimLeft();
            arg.TrimRight(L" >");

            for (;;) {
                args.TrimLeft();
                arg = args.SpanExcluding(L" \t>");
                args = args.Mid(arg.GetLength());

                if (arg.IsEmpty()) {
                    break;
                }
                if (arg.Find(L"color=") == 0) {
                    arg = arg.Mid(6);   // delete "color="
                    if (arg.IsEmpty()) {
                        continue;
                    }

                    CStringW colorTag = SSAColorTag(arg);

                    lstr.Insert(k + l + chars_inserted, colorTag);
                    str.Insert(k + l + chars_inserted, colorTag);
                    chars_inserted += 5 + colorTag.GetLength() + 2;
                }
            }
        }

        else if (lstr.Find(L"</font>", k) == k) {
            lstr.Insert(k + l + chars_inserted, L"{\\c}");
            str.Insert(k + l + chars_inserted, L"{\\c}");
            chars_inserted += 4;
        }

        str.Delete(k, l);
        lstr.Delete(k, l);
        i = k + chars_inserted;
        j = str.GetLength();
    }

    return str;
}

static bool OpenSami(CTextFile* file, CSimpleTextSubtitle& ret, int CharSet)
{
    CStringW buff, caption;
    ULONGLONG pos = file->GetPosition();
    bool fSAMI = false;

    while (file->ReadString(buff) && !fSAMI) {
        if (buff.MakeUpper().Find(L"<SAMI>") >= 0) {
            fSAMI = true;
        }
    }

    if (!fSAMI) {
        return false;
    }

    file->Seek(pos, CFile::begin);

    bool fComment = false;
    int start_time = 0;

    while (file->ReadString(buff)) {
        FastTrim(buff);
        if (buff.IsEmpty()) {
            continue;
        }

        CStringW ubuff = buff;
        ubuff.MakeUpper();

        if (ubuff.Find(L"<!--") >= 0 || ubuff.Find(L"<TITLE>") >= 0) {
            fComment = true;
        }

        if (!fComment) {
            int i;

            if ((i = ubuff.Find(L"<SYNC START=")) >= 0) {
                int time = 0;

                for (i = 12; i < ubuff.GetLength(); i++) {
                    if (ubuff[i] != '>' && ubuff[i] != 'M') {
                        if (iswdigit(ubuff[i])) {
                            time *= 10;
                            time += ubuff[i] - 0x30;
                        }
                    } else {
                        break;
                    }
                }

                ret.Add(
                    SMI2SSA(caption, CharSet),
                    file->IsUnicode(),
                    MS2RT(start_time), MS2RT(time));

                start_time = time;
                caption.Empty();
            }

            caption += buff;
        }

        if (ubuff.Find(L"-->") >= 0 || ubuff.Find(L"</TITLE>") >= 0) {
            fComment = false;
        }
    }

    ret.Add(
        SMI2SSA(caption, CharSet),
        file->IsUnicode(),
        MS2RT(start_time), LONGLONG_MAX);

    return true;
}

static bool OpenVPlayer(CTextFile* file, CSimpleTextSubtitle& ret, int CharSet)
{
    CStringW buff;
    while (file->ReadString(buff)) {
        FastTrim(buff);
        if (buff.IsEmpty()) {
            continue;
        }

        for (int i = 0; i < buff.GetLength(); i++) {
            if ((i = FindChar(buff, '|', i, file->IsUnicode(), CharSet)) < 0) {
                break;
            }
            buff.SetAt(i, '\n');
        }

        int hh, mm, ss;
        int c = swscanf_s(buff, L"%d:%d:%d:", &hh, &mm, &ss);

        if (c == 3) {
            CStringW str = buff.Mid(buff.Find(':', buff.Find(':', buff.Find(':') + 1) + 1) + 1);
            ret.Add(str,
                    file->IsUnicode(),
                    MS2RT((((hh * 60i64 + mm) * 60i64) + ss) * 1000i64),
                    MS2RT((((hh * 60i64 + mm) * 60i64) + ss) * 1000i64 + 1000i64 + 50i64 * str.GetLength()));
        } else if (c != EOF) { // might be another format
            return false;
        }
    }

    return !ret.IsEmpty();
}

static void GetStrW(LPCWSTR& pszBuff, int& nLength, WCHAR sep, LPCWSTR& pszMatch, int& nMatchLength)
{
    // Trim left whitespace
    while (CStringW::StrTraits::IsSpace(*pszBuff)) {
        pszBuff++;
        nLength--;
    }

    LPCWSTR pEnd = CStringW::StrTraits::StringFindChar(pszBuff, sep);
    if (pEnd == nullptr) {
        if (nLength < 1) {
            throw 1;
        }
        nMatchLength = nLength;
    } else {
        nMatchLength = int(pEnd - pszBuff);
    }

    pszMatch = pszBuff;
    if (nMatchLength < nLength) {
        pszBuff = pEnd + 1;
        nLength -= nMatchLength + 1;
    }
}

static CStringW GetStrW(LPCWSTR& pszBuff, int& nLength, WCHAR sep = L',')
{
    LPCWSTR pszMatch;
    int nMatchLength;
    GetStrW(pszBuff, nLength, sep, pszMatch, nMatchLength);

    return CStringW(pszMatch, nMatchLength);
}

static int GetInt(LPCWSTR& pszBuff, int& nLength, WCHAR sep = L',')
{
    LPCWSTR pszMatch;
    int nMatchLength;
    GetStrW(pszBuff, nLength, sep, pszMatch, nMatchLength);

    LPWSTR strEnd;
    int ret;
    if (nMatchLength > 2
            && ((pszMatch[0] == L'&' && towlower(pszMatch[1]) == L'h')
                || (pszMatch[0] == L'0' && towlower(pszMatch[1]) == L'x'))) {
        pszMatch += 2;
        nMatchLength -= 2;
        // Read hexadecimal integer as unsigned
        ret = (int)wcstoul(pszMatch, &strEnd, 16);
    } else {
        ret = wcstol(pszMatch, &strEnd, 10);
    }

    if (pszMatch == strEnd) { // Ensure something was parsed
        throw 1;
    }

    return ret;
}

static double GetFloat(LPCWSTR& pszBuff, int& nLength, WCHAR sep = L',')
{
    if (sep == L'.') { // Parsing a float with '.' as separator doesn't make much sense...
        ASSERT(FALSE);
        return GetInt(pszBuff, nLength, sep);
    }

    LPCWSTR pszMatch;
    int nMatchLength;
    GetStrW(pszBuff, nLength, sep, pszMatch, nMatchLength);

    LPWSTR strEnd;
    double ret = wcstod(pszMatch, &strEnd);
    if (pszMatch == strEnd) { // Ensure something was parsed
        throw 1;
    }

    return ret;
}

static bool LoadFont(const CString& font)
{
    int len = font.GetLength();

    CAutoVectorPtr<BYTE> pData;
    if (len == 0 || (len & 3) == 1 || !pData.Allocate(len)) {
        return false;
    }

    const TCHAR* s = font;
    const TCHAR* e = s + len;
    for (BYTE* p = pData; s < e; s++, p++) {
        *p = BYTE(*s - 33);
    }

    for (ptrdiff_t i = 0, j = 0, k = len & ~3; i < k; i += 4, j += 3) {
        pData[j + 0] = ((pData[i + 0] & 63) << 2) | ((pData[i + 1] >> 4) & 3);
        pData[j + 1] = ((pData[i + 1] & 15) << 4) | ((pData[i + 2] >> 2) & 15);
        pData[j + 2] = ((pData[i + 2] &  3) << 6) | ((pData[i + 3] >> 0) & 63);
    }

    int datalen = (len & ~3) * 3 / 4;

    if ((len & 3) == 2) {
        pData[datalen++] = ((pData[(len & ~3) + 0] & 63) << 2) | ((pData[(len & ~3) + 1] >> 4) & 3);
    } else if ((len & 3) == 3) {
        pData[datalen++] = ((pData[(len & ~3) + 0] & 63) << 2) | ((pData[(len & ~3) + 1] >> 4) & 3);
        pData[datalen++] = ((pData[(len & ~3) + 1] & 15) << 4) | ((pData[(len & ~3) + 2] >> 2) & 15);
    }

    HANDLE hFont = INVALID_HANDLE_VALUE;

    if (HMODULE hModule = LoadLibrary(_T("gdi32.dll"))) {
        typedef HANDLE(WINAPI * PAddFontMemResourceEx)(IN PVOID, IN DWORD, IN PVOID, IN DWORD*);
        if (PAddFontMemResourceEx f = (PAddFontMemResourceEx)GetProcAddress(hModule, "AddFontMemResourceEx")) {
            DWORD cFonts;
            hFont = f(pData, datalen, nullptr, &cFonts);
        }

        FreeLibrary(hModule);
    }

    if (hFont == INVALID_HANDLE_VALUE) {
        TCHAR path[MAX_PATH];
        GetTempPath(MAX_PATH, path);

        DWORD chksum = 0;
        for (ptrdiff_t i = 0, j = datalen >> 2; i < j; i++) {
            chksum += ((DWORD*)(BYTE*)pData)[i];
        }

        CString fn;
        fn.Format(_T("%sfont%08lx.ttf"), path, chksum);

        if (!PathUtils::Exists(fn)) {
            CFile f;
            if (f.Open(fn, CFile::modeCreate | CFile::modeWrite | CFile::typeBinary | CFile::shareDenyNone)) {
                f.Write(pData, datalen);
                f.Close();
            }
        }

        AddFontResource(fn);
    }

    return true;
}

static bool LoadUUEFont(CTextFile* file, CString firstfontname)
{
    CString s, font;
    bool skip_ui_font = firstfontname.Find(_T("segoe")) >= 0;
    while (file->ReadString(s)) {
        FastTrim(s);
        if (s.IsEmpty()) {
            break;
        }
        if (s[0] == '[') { // check for some standard blocks
            if (s.Find(_T("[Script Info]")) == 0) {
                break;
            }
            if (s.Find(_T("[V4+ Styles]")) == 0) {
                break;
            }
            if (s.Find(_T("[V4 Styles]")) == 0) {
                break;
            }
            if (s.Find(_T("[Events]")) == 0) {
                break;
            }
            if (s.Find(_T("[Fonts]")) == 0) {
                break;
            }
            if (s.Find(_T("[Graphics]")) == 0) {
                break;
            }
        }
        if (s.Find(_T("fontname:")) == 0) {
            if (!skip_ui_font && !font.IsEmpty()) {
                LoadFont(font);
            }
            font.Empty();
            skip_ui_font = s.Find(_T("segoe")) > 0;
            continue;
        }

        font += s;
    }

    if (!skip_ui_font && !font.IsEmpty()) {
        LoadFont(font);
    }

    return true;
}

/* Style v3:
 * Format: Name, Fontname, Fontsize, PrimaryColour, SecondaryColour, TertiaryColour, BackColour, Bold, Italic,                                                                    Outline, Shadow, Alignment, MarginL, MarginR, MarginV,          AlphaLevel, Encoding
 * Style v4:
 * Format: Name, Fontname, Fontsize, PrimaryColour, SecondaryColour, TertiaryColour, BackColour, Bold, Italic,                                                       BorderStyle, Outline, Shadow, Alignment, MarginL, MarginR, MarginV,          AlphaLevel, Encoding
 * Style v4+:
 * Format: Name, Fontname, Fontsize, PrimaryColour, SecondaryColour, OutlineColour,  BackColour, Bold, Italic, Underline, StrikeOut, ScaleX, ScaleY, Spacing, Angle, BorderStyle, Outline, Shadow, Alignment, MarginL, MarginR, MarginV,                      Encoding
 * Style v4++:
 * Format: Name, Fontname, Fontsize, PrimaryColour, SecondaryColour, OutlineColour,  BackColour, Bold, Italic, Underline, StrikeOut, ScaleX, ScaleY, Spacing, Angle, BorderStyle, Outline, Shadow, Alignment, MarginL, MarginR, MarginV, MarginB,             Encoding, RelativeTo
 * Non-standard variant 1:
 * Format: Name, Fontname, Fontsize, PrimaryColour, SecondaryColour, OutlineColour,  BackColour, Bold, Italic, Underline,                                            BorderStyle, Outline, Shadow, Alignment, MarginL, MarginR, MarginV,          AlphaLevel, Encoding
 */

bool OpenSubStationAlpha(CTextFile* file, CSimpleTextSubtitle& ret, int CharSet)
{
    bool fRet = false;
    int style_version = 3;
    CStringW buff;
    int ignore_count = 0;
    bool first_line = true;
    bool below_script_info = false;

    // Order of bits: Name, Fontname, Fontsize, PrimaryColour, SecondaryColour, OutlineColour, BackColour, Bold, Italic, Underline, StrikeOut, ScaleX, ScaleY, Spacing, Angle, BorderStyle, Outline, Shadow, Alignment, MarginL, MarginR, MarginV, MarginB, AlphaLevel, Encoding, RelativeTo
    // 26 bits, RelativeTo is least significant
    const uint32_t style_param_v3   = 0b11111111100000001111110110;
    const uint32_t style_param_v4   = 0b11111111100000011111110110;
    const uint32_t style_param_v4p  = 0b11111111111111111111110010;
    const uint32_t style_param_v4pp = 0b11111111111111111111111011;
    const uint32_t style_param_var1 = 0b11111111110000011111110110;
    uint32_t style_param = style_param_v3;

    // Order of bits:  Marked, Layer, Start, End, Style, Name/Actor, MarginL, MarginR, MarginV, MarginB, Effect, Text
    // 12 bits, Text is least significant bit
    const uint32_t event_param_v3   = 0b101111111011;
    const uint32_t event_param_v4   = 0b101111111011;
    const uint32_t event_param_v4p  = 0b011111111011;
    const uint32_t event_param_v4pp = 0b011111111111;
    const uint32_t event_param_var1 = 0b011111000001;
    ret.event_param = event_param_v3;

    while (file->ReadString(buff)) {
        FastTrim(buff);
        if (buff.IsEmpty() || buff.GetAt(0) == L';') {
            continue;
        }

        if (first_line) {
            if (buff == L"1") {
                // SRT file
                return false;
            }
            if (buff == L"WEBVTT") {
                return false;
            }
            first_line = false;
        }

        LPCWSTR pszBuff = buff;
        int nBuffLength = buff.GetLength();
        CStringW entry = GetStrW(pszBuff, nBuffLength, L':');
        entry.MakeLower();

        if (entry == L"dialogue") {
            below_script_info = false;
            try {
                int hh1, mm1, ss1, ms1_div10, hh2, mm2, ss2, ms2_div10, layer = 0;
                CRect marginRect;

                if (ret.event_param & (1 << 11)) { // Marked
                    GetStrW(pszBuff, nBuffLength, L'=');
                    GetInt(pszBuff, nBuffLength);
                }
                if (ret.event_param & (1 << 10)) { // Layer
                    layer = GetInt(pszBuff, nBuffLength);
                }
                hh1 = GetInt(pszBuff, nBuffLength, L':');
                mm1 = GetInt(pszBuff, nBuffLength, L':');
                ss1 = GetInt(pszBuff, nBuffLength, L'.');
                ms1_div10 = GetInt(pszBuff, nBuffLength);
                hh2 = GetInt(pszBuff, nBuffLength, L':');
                mm2 = GetInt(pszBuff, nBuffLength, L':');
                ss2 = GetInt(pszBuff, nBuffLength, L'.');
                ms2_div10 = GetInt(pszBuff, nBuffLength);

                CString style = WToT(GetStrW(pszBuff, nBuffLength));

                CString actor;
                if (ret.event_param & (1 << 6)) { // Actor/Name
                    actor = WToT(GetStrW(pszBuff, nBuffLength));
                }

                if (ret.event_param & (1 << 5)) {
                    marginRect.left = GetInt(pszBuff, nBuffLength);
                }
                if (ret.event_param & (1 << 4)) {
                    marginRect.right = GetInt(pszBuff, nBuffLength);
                }
                if (ret.event_param & (1 << 3)) {
                    marginRect.top = marginRect.bottom = GetInt(pszBuff, nBuffLength);
                }
                if (ret.event_param & (1 << 2)) {
                    marginRect.bottom = GetInt(pszBuff, nBuffLength);
                }

                CString effect;
                if (ret.event_param & (1 << 1)) {
                    effect = WToT(GetStrW(pszBuff, nBuffLength));
                    int len = std::min(effect.GetLength(), nBuffLength);
                    if (effect.Left(len) == WToT(CStringW(pszBuff, len))) {
                        effect.Empty();
                    }
                }

                style.TrimLeft(_T('*'));
                if (!style.CompareNoCase(_T("Default"))) {
                    style = _T("Default");
                }

                ret.Add(pszBuff,
                        file->IsUnicode(),
                        MS2RT((((hh1 * 60i64 + mm1) * 60i64) + ss1) * 1000i64 + ms1_div10 * 10i64),
                        MS2RT((((hh2 * 60i64 + mm2) * 60i64) + ss2) * 1000i64 + ms2_div10 * 10i64),
                        style, actor, effect,
                        marginRect,
                        layer);
            } catch (...) {
                return false;
            }
        } else if (entry == L"style") {
            below_script_info = false;
            STSStyle* style = DEBUG_NEW STSStyle;
            if (!style) {
                return false;
            }

            try {
                CString styleName = WToT(GetStrW(pszBuff, nBuffLength));
                style->fontName = WToT(GetStrW(pszBuff, nBuffLength));
                style->fontSize = GetFloat(pszBuff, nBuffLength);
                for (size_t i = 0; i < 4; i++) {
                    style->colors[i] = (COLORREF)GetInt(pszBuff, nBuffLength);
                }
                style->fontWeight = GetInt(pszBuff, nBuffLength) ? FW_BOLD : FW_NORMAL;
                style->fItalic = GetInt(pszBuff, nBuffLength);
                if (style_param & (1 << 16)) {
                    style->fUnderline = GetInt(pszBuff, nBuffLength);
                }
                if (style_param & (1 << 15)) {
                    style->fStrikeOut = GetInt(pszBuff, nBuffLength);
                }
                if (style_param & (1 << 14)) {
                    style->fontScaleX = GetFloat(pszBuff, nBuffLength);
                }
                if (style_param & (1 << 13)) {
                    style->fontScaleY = GetFloat(pszBuff, nBuffLength);
                }
                if (style_param & (1 << 12)) {
                    style->fontSpacing = GetFloat(pszBuff, nBuffLength);
                }
                if (style_param & (1 << 11)) {
                    style->fontAngleZ = GetFloat(pszBuff, nBuffLength);
                }
                if (style_param & (1 << 10)) {
                    style->borderStyle = GetInt(pszBuff, nBuffLength);
                }
                style->outlineWidthX = style->outlineWidthY = GetFloat(pszBuff, nBuffLength);
                style->shadowDepthX = style->shadowDepthY = GetFloat(pszBuff, nBuffLength);
                style->scrAlignment = GetInt(pszBuff, nBuffLength);
                style->marginRect.left = GetInt(pszBuff, nBuffLength);
                style->marginRect.right = GetInt(pszBuff, nBuffLength);
                style->marginRect.top = style->marginRect.bottom = GetInt(pszBuff, nBuffLength);
                if (style_param & (1 << 3)) {
                    style->marginRect.bottom = GetInt(pszBuff, nBuffLength);
                }

                int alpha = 0;
                if (style_param & (1 << 2)) {
                    alpha = GetInt(pszBuff, nBuffLength);
                }
                style->charSet = GetInt(pszBuff, nBuffLength);
                if (style->charSet < 0) style->charSet = DEFAULT_CHARSET;
                if (style_param & (1)) {
                    style->relativeTo = (STSStyle::RelativeTo)GetInt(pszBuff, nBuffLength);
                }

                if (style_version <= 4)  {
                    style->colors[2] = style->colors[3];    // style->colors[2] is used for drawing the outline
                    alpha = std::max(std::min(alpha, 0xff), 0);
                    for (size_t i = 0; i < 3; i++) {
                        style->alpha[i] = (BYTE)alpha;
                    }
                    style->alpha[3] = 0x80;
                }
                if (style_version >= 5) {
                    for (size_t i = 0; i < 4; i++) {
                        style->alpha[i] = (BYTE)(style->colors[i] >> 24);
                        style->colors[i] &= 0xffffff;
                    }
                    style->fontScaleX = std::max(style->fontScaleX, 0.0);
                    style->fontScaleY = std::max(style->fontScaleY, 0.0);
                }
                style->fontAngleX = style->fontAngleY = 0;
                style->borderStyle = style->borderStyle == 1 ? 0 : style->borderStyle == 3 ? 1 : 0;
                style->outlineWidthX = std::max(style->outlineWidthX, 0.0);
                style->outlineWidthY = std::max(style->outlineWidthY, 0.0);
                style->shadowDepthX = std::max(style->shadowDepthX, 0.0);
                style->shadowDepthY = std::max(style->shadowDepthY, 0.0);
                if (style_version <= 4) {
                    style->scrAlignment = (style->scrAlignment & 4) ? ((style->scrAlignment & 3) + 6) // top
                                          : (style->scrAlignment & 8) ? ((style->scrAlignment & 3) + 3) // mid
                                          : (style->scrAlignment & 3); // bottom
                }

                styleName.TrimLeft(_T('*'));

                style->hasAnsiStyleName = !file->IsUnicode();

                //for srt the default style is created by mpc-hc.  if they have a .style containing "Default" then we can assume the default is not desired
                if (styleName == L"Default" && EndsWithNoCase(file->GetFilePath(), L".srt.style")) {
                    ret.m_styles.RemoveKey(styleName);
                }

                ret.AddStyle(styleName, style);
            } catch (...) {
                delete style;
                TRACE(L"invalid SSA style\n");
                //return false;
            }
        } else if (entry == L"[script info]") {
            fRet = true;
            below_script_info = true;
        } else if (entry == L"playresx") {
            try {
                ret.m_playRes.cx = GetInt(pszBuff, nBuffLength);
            } catch (...) {
                ret.m_playRes = CSize(0, 0);
                return false;
            }

            if (ret.m_playRes.cy <= 0) {
                ret.m_playRes.cy = (ret.m_playRes.cx == 1280)
                                         ? 1024
                                         : ret.m_playRes.cx * 3 / 4;
            }
        } else if (entry == L"playresy") {
            try {
                ret.m_playRes.cy = GetInt(pszBuff, nBuffLength);
            } catch (...) {
                ret.m_playRes = CSize(0, 0);
                return false;
            }

            if (ret.m_playRes.cx <= 0) {
                ret.m_playRes.cx = (ret.m_playRes.cy == 1024)
                                         ? 1280
                                         : ret.m_playRes.cy * 4 / 3;
            }
        } else if (entry == L"layoutresx") {
            try {
                ret.m_layoutRes.cx = GetInt(pszBuff, nBuffLength);
            } catch (...) {
                ret.m_layoutRes = CSize(0, 0);
                return false;
            }
        } else if (entry == L"layoutresy") {
            try {
                ret.m_layoutRes.cy = GetInt(pszBuff, nBuffLength);
            } catch (...) {
                ret.m_layoutRes = CSize(0, 0);
                return false;
            }
        } else if (entry == L"wrapstyle") {
            try {
                ret.m_defaultWrapStyle = GetInt(pszBuff, nBuffLength);
            } catch (...) {
                ret.m_defaultWrapStyle = 1;
                return false;
            }
        } else if (entry == L"scripttype") {
            if (buff.GetLength() >= 4 && !buff.Right(4).CompareNoCase(L"4.00")) {
                style_version = 4;
                style_param = style_param_v4;
                ret.event_param = event_param_v4;
            } else if (buff.GetLength() >= 5 && !buff.Right(5).CompareNoCase(L"4.00+")) {
                style_version = 5;
                style_param = style_param_v4p;
                ret.event_param = event_param_v4p;
            } else if (buff.GetLength() >= 6 && !buff.Right(6).CompareNoCase(L"4.00++")) {
                style_version = 6;
                style_param = style_param_v4pp;
                ret.event_param = event_param_v4pp;
            }
        } else if (entry == L"collisions") {
            if (nBuffLength) {
                buff = GetStrW(pszBuff, nBuffLength);
                buff.MakeLower();
                ret.m_collisions = buff.Find(L"reverse") >= 0 ? 1 : 0;
            }
        } else if (entry == L"scaledborderandshadow") {
            if (nBuffLength) {
                buff = GetStrW(pszBuff, nBuffLength);
                buff.MakeLower();
                ret.m_scaledBAS = ret.m_scaledBAS2 = (buff.Find(L"yes") >= 0) ? 1 : 0;
            }
        } else if (entry == L"[v4 styles]") {
            fRet = true;
            style_param = style_param_v4;
            below_script_info = false;
        } else if (entry == L"[v4+ styles]") {
            fRet = true;
            style_param = style_param_v4p;
            below_script_info = false;
        } else if (entry == L"[v4++ styles]") {
            fRet = true;
            style_param = style_param_v4pp;
            below_script_info = false;
        } else if (entry == L"[events]") {
            fRet = true;
            below_script_info = false;
        } else if (entry == L"language") {
            if (nBuffLength) {
                ret.openTypeLangHint = WToA(GetStrW(pszBuff, nBuffLength));
            }
        } else if (entry == L"fontname") {
            CString fontName = GetStrW(pszBuff, nBuffLength);
            LoadUUEFont(file, fontName);
        } else if (entry == L"ycbcr matrix") {
            if (nBuffLength) {
                ret.m_sYCbCrMatrix = GetStrW(pszBuff, nBuffLength);
                ret.m_sYCbCrMatrix.MakeUpper();
            }
        } else if (entry == L"format") {
            CStringW formatstr = CStringW(pszBuff).TrimLeft().MakeLower();
            formatstr.Remove(L' '); // remove spaces
            if (formatstr.Left(4) == L"name") {
                // Style formats
                if (       formatstr == L"name,fontname,fontsize,primarycolour,secondarycolour,outlinecolour,backcolour,bold,italic,underline,strikeout,scalex,scaley,spacing,angle,borderstyle,outline,shadow,alignment,marginl,marginr,marginv,encoding") {
                    style_param = style_param_v4p;
                } else if (formatstr == L"name,fontname,fontsize,primarycolour,secondarycolour,tertiarycolour,backcolour,bold,italic,borderstyle,outline,shadow,alignment,marginl,marginr,marginv,alphalevel,encoding") {
                    style_param = style_param_v4;
                } else if (formatstr == L"name,fontname,fontsize,primarycolour,secondarycolour,outlinecolour,backcolour,bold,italic,underline,strikeout,scalex,scaley,spacing,angle,borderstyle,outline,shadow,alignment,marginl,marginr,marginv,marginb,encoding,relativeto") {
                    style_param = style_param_v4pp;
                } else if (formatstr == L"name,fontname,fontsize,primarycolour,secondarycolour,tertiarycolour,backcolour,bold,italic,outline,shadow,alignment,marginl,marginr,marginv,alphalevel,encoding") {
                    style_param = style_param_v3;
                } else if (formatstr == L"name,fontname,fontsize,primarycolour,secondarycolour,outlinecolour,backcolour,bold,italic,underline,borderstyle,outline,shadow,alignment,marginl,marginr,marginv,alphalevel,encoding") {
                    style_param = style_param_var1;
                } else {
                    TRACE(_T("Unknown SSA style format: %s\n"), static_cast<LPCWSTR>(formatstr));
                    ASSERT(false);
                }
            } else {
                // Event formats
                if (       formatstr == L"layer,start,end,style,name,marginl,marginr,marginv,effect,text") {
                    ret.event_param = event_param_v4p;
                } else if (formatstr == L"layer,start,end,style,actor,marginl,marginr,marginv,effect,text") {
                    ret.event_param = event_param_v4p;
                } else if (formatstr == L"layer,start,end,style,name,marginl,marginr,marginv,marginb,effect,text") {
                    ret.event_param = event_param_v4pp;
                } else if (formatstr == L"marked,start,end,style,name,marginl,marginr,marginv,effect,text") {
                    ret.event_param = event_param_v4;
                } else if (formatstr == L"layer,start,end,style,text") {
                    ret.event_param = event_param_var1;
                } else {
                    TRACE(_T("Unknown SSA event format: %s\n"), static_cast<LPCWSTR>(formatstr));
                    ASSERT(false);
                }
            }
        } else {
            if (!fRet && !below_script_info) {
                TRACE(_T("Ignoring unknown SSA entry: %s\n"), static_cast<LPCWSTR>(entry));
                if (++ignore_count >= 10) {
                    return false;
                }
            }
        }
    }

    return fRet;
}

static bool OpenXombieSub(CTextFile* file, CSimpleTextSubtitle& ret, int CharSet)
{
    //  CMapStringToPtr stylemap;

    CStringW buff;
    while (file->ReadString(buff)) {
        FastTrim(buff);
        if (buff.IsEmpty() || buff.GetAt(0) == L';') {
            continue;
        }

        LPCWSTR pszBuff = buff;
        int nBuffLength = buff.GetLength();
        CStringW entry = GetStrW(pszBuff, nBuffLength, L'=');
        entry.MakeLower();

        /*if (entry == L"version") {
            double version = GetFloat(buff);
        } else*/
        if (entry == L"screenhorizontal") {
            try {
                ret.m_storageRes.cx = GetInt(pszBuff, nBuffLength);
            } catch (...) {
                ret.m_storageRes = CSize(0, 0);
                return false;
            }

            if (ret.m_storageRes.cy <= 0) {
                ret.m_storageRes.cy = (ret.m_storageRes.cx == 1280)
                                         ? 1024
                                         : ret.m_storageRes.cx * 3 / 4;
            }
        } else if (entry == L"screenvertical") {
            try {
                ret.m_storageRes.cy = GetInt(pszBuff, nBuffLength);
            } catch (...) {
                ret.m_storageRes = CSize(0, 0);
                return false;
            }

            if (ret.m_storageRes.cx <= 0) {
                ret.m_storageRes.cx = (ret.m_storageRes.cy == 1024)
                                         ? 1280
                                         : ret.m_storageRes.cy * 4 / 3;
            }
        } else if (entry == L"style") {
            STSStyle* style = DEBUG_NEW STSStyle;
            if (!style) {
                return false;
            }

            try {
                CString styleName = WToT(GetStrW(pszBuff, nBuffLength)) + _T("_") + WToT(GetStrW(pszBuff, nBuffLength));
                style->fontName = WToT(GetStrW(pszBuff, nBuffLength));
                style->fontSize = GetFloat(pszBuff, nBuffLength);
                for (size_t i = 0; i < 4; i++) {
                    style->colors[i] = (COLORREF)GetInt(pszBuff, nBuffLength);
                }
                for (size_t i = 0; i < 4; i++) {
                    style->alpha[i] = (BYTE)GetInt(pszBuff, nBuffLength);
                }
                style->fontWeight = GetInt(pszBuff, nBuffLength) ? FW_BOLD : FW_NORMAL;
                style->fItalic = GetInt(pszBuff, nBuffLength);
                style->fUnderline = GetInt(pszBuff, nBuffLength);
                style->fStrikeOut = GetInt(pszBuff, nBuffLength);
                style->fBlur = GetInt(pszBuff, nBuffLength);
                style->fontScaleX = GetFloat(pszBuff, nBuffLength);
                style->fontScaleY = GetFloat(pszBuff, nBuffLength);
                style->fontSpacing = GetFloat(pszBuff, nBuffLength);
                style->fontAngleX = GetFloat(pszBuff, nBuffLength);
                style->fontAngleY = GetFloat(pszBuff, nBuffLength);
                style->fontAngleZ = GetFloat(pszBuff, nBuffLength);
                style->borderStyle = GetInt(pszBuff, nBuffLength);
                style->outlineWidthX = style->outlineWidthY = GetFloat(pszBuff, nBuffLength);
                style->shadowDepthX = style->shadowDepthY = GetFloat(pszBuff, nBuffLength);
                style->scrAlignment = GetInt(pszBuff, nBuffLength);
                style->marginRect.left = GetInt(pszBuff, nBuffLength);
                style->marginRect.right = GetInt(pszBuff, nBuffLength);
                style->marginRect.top = style->marginRect.bottom = GetInt(pszBuff, nBuffLength);
                style->charSet = GetInt(pszBuff, nBuffLength);

                style->fontScaleX = std::max(style->fontScaleX, 0.0);
                style->fontScaleY = std::max(style->fontScaleY, 0.0);
                style->fontSpacing = std::max(style->fontSpacing, 0.0);
                style->borderStyle = style->borderStyle == 1 ? 0 : style->borderStyle == 3 ? 1 : 0;
                style->outlineWidthX = std::max(style->outlineWidthX, 0.0);
                style->outlineWidthY = std::max(style->outlineWidthY, 0.0);
                style->shadowDepthX = std::max(style->shadowDepthX, 0.0);
                style->shadowDepthY = std::max(style->shadowDepthY, 0.0);

                ret.AddStyle(styleName, style);
            } catch (...) {
                delete style;
                return false;
            }
        } else if (entry == L"line") {
            try {
                int hh1, mm1, ss1, ms1, hh2, mm2, ss2, ms2, layer = 0;
                CRect marginRect;

                if (GetStrW(pszBuff, nBuffLength) != L"D") {
                    continue;
                }
                CString id = GetStrW(pszBuff, nBuffLength);
                layer = GetInt(pszBuff, nBuffLength);
                hh1 = GetInt(pszBuff, nBuffLength, L':');
                mm1 = GetInt(pszBuff, nBuffLength, L':');
                ss1 = GetInt(pszBuff, nBuffLength, L'.');
                ms1 = GetInt(pszBuff, nBuffLength);
                hh2 = GetInt(pszBuff, nBuffLength, L':');
                mm2 = GetInt(pszBuff, nBuffLength, L':');
                ss2 = GetInt(pszBuff, nBuffLength, L'.');
                ms2 = GetInt(pszBuff, nBuffLength);
                CString style = WToT(GetStrW(pszBuff, nBuffLength)) + _T("_") + WToT(GetStrW(pszBuff, nBuffLength));
                CString actor = WToT(GetStrW(pszBuff, nBuffLength));
                marginRect.left = GetInt(pszBuff, nBuffLength);
                marginRect.right = GetInt(pszBuff, nBuffLength);
                marginRect.top = marginRect.bottom = GetInt(pszBuff, nBuffLength);

                style.TrimLeft('*');
                if (!style.CompareNoCase(_T("Default"))) {
                    style = _T("Default");
                }

                ret.Add(pszBuff,
                        file->IsUnicode(),
                        MS2RT((((hh1 * 60i64 + mm1) * 60i64) + ss1) * 1000i64 + ms1),
                        MS2RT((((hh2 * 60i64 + mm2) * 60i64) + ss2) * 1000i64 + ms2),
                        style, actor, _T(""),
                        marginRect,
                        layer);
            } catch (...) {
                return false;
            }
        } else if (entry == L"fontname") {
            CString fontName = GetStrW(pszBuff, nBuffLength);
            LoadUUEFont(file, fontName);
        }
    }

    return !ret.IsEmpty();
}

static bool OpenUSF(CTextFile* file, CSimpleTextSubtitle& ret, int CharSet)
{
    CString str;
    while (file->ReadString(str)) {
        if (str.Find(_T("USFSubtitles")) >= 0) {
            CUSFSubtitles usf;
            if (usf.Read(file->GetFilePath()) && usf.ConvertToSTS(ret)) {
                return true;
            }

            break;
        }
    }

    return false;
}

static CStringW MPL22SSA(CStringW str, bool fUnicode, int CharSet)
{
    // Convert MPL2 italic tags to MicroDVD italic tags
    if (str[0] == L'/') {
        str = L"{y:i}" + str.Mid(1);
    }
    str.Replace(L"|/", L"|{y:i}");

    return MicroDVD2SSA(str, fUnicode, CharSet);
}

static bool OpenMPL2(CTextFile* file, CSimpleTextSubtitle& ret, int CharSet)
{
    CStringW buff;
    while (file->ReadString(buff)) {
        FastTrim(buff);
        if (buff.IsEmpty()) {
            continue;
        }

        int start, end;
        int c = swscanf_s(buff, L"[%d][%d]", &start, &end);

        if (c == 2) {
            ret.Add(
                MPL22SSA(buff.Mid(buff.Find(']', buff.Find(']') + 1) + 1), file->IsUnicode(), CharSet),
                file->IsUnicode(),
                MS2RT(start * 100i64),
                MS2RT(end * 100i64));
        } else if (c != EOF) { // might be another format
            return false;
        }
    }

    return !ret.IsEmpty();
}

typedef bool (*STSOpenFunct)(CTextFile* file, CSimpleTextSubtitle& ret, int CharSet);

static bool OpenRealText(CTextFile* file, CSimpleTextSubtitle& ret, int CharSet);

struct OpenFunctStruct {
    STSOpenFunct open;
    tmode mode;
    Subtitle::SubType type;
};

static OpenFunctStruct OpenFuncts[] = {
    OpenSubRipper, TIME, Subtitle::SRT,
    OpenOldSubRipper, TIME, Subtitle::SRT,
    OpenSubViewer, TIME, Subtitle::SRT,
    OpenMicroDVD, FRAME, Subtitle::SUB,
    OpenVPlayer, TIME, Subtitle::SRT,
    OpenSubStationAlpha, TIME, Subtitle::SSA,
    OpenVTT, TIME, Subtitle::VTT,
    OpenXombieSub, TIME, Subtitle::XSS,
    OpenMPL2, TIME, Subtitle::SRT,
    OpenRealText, TIME, Subtitle::RT,
    OpenSami, TIME, Subtitle::SMI,
    OpenUSF, TIME, Subtitle::USF,
};

static int nOpenFuncts = _countof(OpenFuncts);

static std::vector<int> PreferredOpenFuncts(CString fn) {
    std::vector<int> functs;
    auto fileExt = PathUtils::FileExt(fn).TrimLeft('.');
    for (int i = 0; i < nOpenFuncts; i++) {
        if (fileExt == _T("vtt")) {
            if (OpenFuncts[i].open == OpenVTT) functs.insert(functs.begin(), i);
            else if (OpenFuncts[i].open == OpenSubRipper) functs.push_back(i);
        } else if (fileExt == _T("srt")) {
            if (OpenFuncts[i].open == OpenSubRipper) functs.insert(functs.begin(), i);
            else functs.push_back(i);
        } else if (fileExt == _T("ssa") || fileExt == _T("ass")) {
            if (OpenFuncts[i].open == OpenSubStationAlpha) functs.insert(functs.begin(), i);
            else if (OpenFuncts[i].open == OpenSubRipper) functs.push_back(i);
        } else if (fileExt == _T("xss")) {
            if (OpenFuncts[i].open == OpenXombieSub) functs.insert(functs.begin(), i);
        } else if (fileExt == _T("sub")) {
            if (OpenFuncts[i].open == OpenSubViewer) functs.insert(functs.begin(), i);
            else functs.push_back(i);
        } else if (fileExt == _T("txt")) {
            if (OpenFuncts[i].open == OpenMicroDVD) functs.insert(functs.begin(), i);
            else functs.push_back(i);
        } else if (fileExt == _T("rt")) {
            if (OpenFuncts[i].open == OpenRealText) functs.insert(functs.begin(), i);
        } else if (fileExt == _T("smi")) {
            if (OpenFuncts[i].open == OpenSami) functs.insert(functs.begin(), i);
        } else if (fileExt == _T("usf")) {
            if (OpenFuncts[i].open == OpenUSF) functs.insert(functs.begin(), i);
        } else if (fileExt == _T("style")) {
            if (OpenFuncts[i].open == OpenSubStationAlpha) functs.push_back(i);
        } else if (fileExt == _T("tmp")) { // used for embedded subs and downloaded subs
            if (OpenFuncts[i].open == OpenSubRipper || OpenFuncts[i].open == OpenSubStationAlpha || OpenFuncts[i].open == OpenVTT) functs.insert(functs.begin(), i);
            else functs.push_back(i);
        } else {
            functs.push_back(i);
        }
    }
    return functs;
}

//

CSimpleTextSubtitle::CSimpleTextSubtitle()
    : m_lcid(0)
    , m_langname()
    , m_subtitleType(Subtitle::SRT)
    , m_mode(TIME)
    , m_encoding(CTextFile::DEFAULT_ENCODING)
    , m_provider(_T("Local"))
    , m_eHearingImpaired(Subtitle::HI_UNKNOWN)
    , m_storageRes(CSize(0, 0))
    , m_playRes(CSize(0, 0))
    , m_layoutRes(CSize(0, 0))
    , m_defaultWrapStyle(0)
    , m_collisions(0)
    , m_scaledBAS(-1)
    , m_scaledBAS2(-1)
    , m_bStyleOverrideActive(false)
    , m_bUsingPlayerDefaultStyle(false)
    , m_ePARCompensationType(EPCTDisabled)
    , m_dPARCompensation(1.0)
    , m_SubRendererSettings(GetSubRendererSettings())
    , overrideANSICharset(0)
#if USE_LIBASS
    , m_LibassContext(this)
#endif
{
}

CSimpleTextSubtitle::~CSimpleTextSubtitle()
{
    Empty();
}
/*
CSimpleTextSubtitle::CSimpleTextSubtitle(CSimpleTextSubtitle& sts)
{
    *this = sts;
}

CSimpleTextSubtitle& CSimpleTextSubtitle::operator = (CSimpleTextSubtitle& sts)
{
    Copy(sts);

    return *this;
}
*/

void CSimpleTextSubtitle::Copy(CSimpleTextSubtitle& sts)
{
    if (this != &sts) {
        Empty();

        m_name = sts.m_name;
        m_mode = sts.m_mode;
        m_path = sts.m_path;
        m_subtitleType = sts.m_subtitleType;
        m_storageRes = sts.m_storageRes;
        m_playRes = sts.m_playRes;
        m_layoutRes = sts.m_layoutRes;
        m_defaultWrapStyle = sts.m_defaultWrapStyle;
        m_collisions = sts.m_collisions;
        m_scaledBAS = sts.m_scaledBAS;
        m_scaledBAS2 = sts.m_scaledBAS2;
        m_encoding = sts.m_encoding;
        m_bUsingPlayerDefaultStyle = sts.m_bUsingPlayerDefaultStyle;
        m_provider = sts.m_provider;
        m_eHearingImpaired = sts.m_eHearingImpaired;
        CopyStyles(sts.m_styles);
        m_segments.Copy(sts.m_segments);
        __super::Copy(sts);
#if USE_LIBASS
        if (m_LibassContext.IsLibassActive()) {
            m_LibassContext.LoadASSFile(m_subtitleType);
        }
#endif
    }
}

void CSimpleTextSubtitle::Append(CSimpleTextSubtitle& sts, REFERENCE_TIME timeoff)
{
    if (timeoff < 0) {
        timeoff = !IsEmpty() ? GetAt(GetCount() - 1).end : 0;
    }

    for (size_t i = 0, j = GetCount(); i < j; i++) {
        if (GetAt(i).start > timeoff) {
            RemoveAt(i, j - i);
            break;
        }
    }

    CopyStyles(sts.m_styles, true);

    for (size_t i = 0, j = sts.GetCount(); i < j; i++) {
        STSEntry stse = sts.GetAt(i);
        stse.start += timeoff;
        stse.end += timeoff;
        stse.readorder += (int)GetCount();
        __super::Add(stse);
    }

    CreateSegments();
}

bool CSimpleTextSubtitle::CopyToStyles(CSTSStyleMap& styles) {
    styles.Free();

    POSITION pos = m_styles.GetStartPosition();
    while (pos) {
        CString key;
        STSStyle* val;
        m_styles.GetNextAssoc(pos, key, val);

        STSStyle* s = DEBUG_NEW STSStyle;
        if (!s) {
            return false;
        }

        *s = *val;

        styles[key] = s;
    }

    return true;
}

bool CSimpleTextSubtitle::CopyStyles(const CSTSStyleMap& styles, bool fAppend)
{
    if (!fAppend) {
        m_styles.Free();
    }

    POSITION pos = styles.GetStartPosition();
    while (pos) {
        CString key;
        STSStyle* val;
        styles.GetNextAssoc(pos, key, val);

        STSStyle* s = DEBUG_NEW STSStyle;
        if (!s) {
            return false;
        }

        *s = *val;

        AddStyle(key, s);
    }

    return true;
}

void CSimpleTextSubtitle::Empty()
{
    m_storageRes = CSize(0, 0);
    m_playRes = CSize(0, 0);
    m_layoutRes = CSize(0, 0);
    m_styles.Free();
    m_segments.RemoveAll();
    RemoveAll();
}

static bool SegmentCompStart(const STSSegment& segment, REFERENCE_TIME start)
{
    return (segment.start < start);
}

void CSimpleTextSubtitle::Add(CStringW str, bool fUnicode, REFERENCE_TIME start, REFERENCE_TIME end, CString style, CString actor, CString effect, const CRect& marginRect, int layer, int readorder)
{
    FastTrim(str);
    if (str.IsEmpty() || start > end) {
        return;
    }
    //TRACE(_T("CSimpleTextSubtitle::Add (%d) = %s\n"), m_segments.GetCount(), str.GetString());
    if (m_subtitleType == Subtitle::VTT) {
        CStringW cueTags = WebVTTCueStrip(str);
        WebVTTcolorMap clrMap;
        WebVTT2SSA(str, cueTags, clrMap);
        if (str.IsEmpty()) return;
    }

    str.Remove('\r');
    str.Replace(L"\n", L"\\N");
    if (style.IsEmpty()) {
        style = _T("Default");
    }
    style.TrimLeft('*');

    STSEntry sub;
    sub.str = str;
    sub.fUnicode = fUnicode;
    sub.style = style;
    sub.actor = actor;
    sub.effect = effect;
    sub.marginRect = marginRect;
    sub.layer = layer;
    sub.start = start;
    sub.end = end;
    sub.readorder = readorder < 0 ? (int)GetCount() : readorder;

    int n = (int)__super::GetCount();

    // Entries with a null duration don't belong to any segments since
    // they are not to be rendered. We choose not to skip them completely
    // so that they are not lost when saving a subtitle file from MPC-HC
    // and so that one can change the timings of such entries using the
    // Subresync bar if necessary.
    if (start == end) {
        return;
    }

    size_t segmentsCount = m_segments.GetCount();

    if (segmentsCount == 0) { // First segment
        n = (int)__super::Add(sub);
        STSSegment stss(start, end);
        stss.subs.Add(n);
        m_segments.Add(stss);
    } else {
        STSSegment* segmentsStart = m_segments.GetData();
        STSSegment* segmentsEnd   = segmentsStart + segmentsCount;
        STSSegment* segment = std::lower_bound(segmentsStart, segmentsEnd, start, SegmentCompStart);

        if (m_subtitleType == Subtitle::VTT && start == segment->start && end == segment->end) {
            // ToDo: compare new sub with existing one to verify if it is really a duplicate
            //TRACE(_T("Dropping duplicate WebVTT sub (n=%d)\n"), n);
            return;
        }

        n = (int)__super::Add(sub);

        size_t i = segment - segmentsStart;
        if (i > 0 && m_segments[i - 1].end > start) {
            // The beginning of i-1th segment isn't modified
            // by the new entry so separate it in two segments
            STSSegment stss(m_segments[i - 1].start, start);
            stss.subs.Copy(m_segments[i - 1].subs);
            m_segments[i - 1].start = start;
            m_segments.InsertAt(i - 1, stss);
        } else if (i < segmentsCount && start < m_segments[i].start) {
            // The new entry doesn't start in an existing segment.
            // It might even not overlap with any segment at all
            STSSegment stss(start, std::min(end, m_segments[i].start));
            stss.subs.Add(n);
            m_segments.InsertAt(i, stss);
            i++;
        }

        REFERENCE_TIME lastEnd = _I64_MAX;
        for (; i < m_segments.GetCount() && m_segments[i].start < end; i++) {
            STSSegment& s = m_segments[i];

            if (lastEnd < s.start) {
                // There is a gap between the segments overlapped by
                // the new entry so we have to create a new one
                STSSegment stss(lastEnd, s.start);
                stss.subs.Add(n);
                lastEnd = s.start; // s might not point on the right segment after inserting so do the modification now
                m_segments.InsertAt(i, stss);
            } else {
                if (end < s.end) {
                    // The end of current segment isn't modified
                    // by the new entry so separate it in two segments
                    STSSegment stss(end, s.end);
                    stss.subs.Copy(s.subs);
                    s.end = end; // s might not point on the right segment after inserting so do the modification now
                    m_segments.InsertAt(i + 1, stss);
                }

                // The array might have been reallocated so create a new reference
                STSSegment& sAdd = m_segments[i];

                // Add the entry to the current segment now that we are you it belongs to it
                size_t entriesCount = sAdd.subs.GetCount();
                // Take a shortcut when possible
                if (!entriesCount || sub.readorder >= GetAt(sAdd.subs[entriesCount - 1]).readorder) {
                    sAdd.subs.Add(n);
                } else {
                    for (size_t j = 0; j < entriesCount; j++) {
                        if (sub.readorder < GetAt(sAdd.subs[j]).readorder) {
                            sAdd.subs.InsertAt(j, n);
                            break;
                        }
                    }
                }

                lastEnd = sAdd.end;
            }
        }

        if (end > m_segments[i - 1].end) {
            // The new entry ends after the last overlapping segment.
            // It might even not overlap with any segment at all
            STSSegment stss(std::max(start, m_segments[i - 1].end), end);
            stss.subs.Add(n);
            m_segments.InsertAt(i, stss);
        }
    }
}

STSStyle* CSimpleTextSubtitle::CreateDefaultStyle(int CharSet)
{
    CString def(_T("Default"));

    STSStyle* ret = nullptr;

    if (!m_styles.Lookup(def, ret)) {
        STSStyle* style = DEBUG_NEW STSStyle();
        *style = m_SubRendererSettings.defaultStyle;
        if (CharSet != DEFAULT_CHARSET) {
            style->charSet = CharSet;
        }
        AddStyle(def, style);
        m_styles.Lookup(def, ret);

        m_bUsingPlayerDefaultStyle = true;
    }

    m_originalDefaultStyle = *ret;

    return ret;
}

STSStyle CSimpleTextSubtitle::GetOriginalDefaultStyle()
{
    return m_originalDefaultStyle;
}

void CSimpleTextSubtitle::ChangeUnknownStylesToDefault()
{
    CAtlMap<CString, STSStyle*, CStringElementTraits<CString>> unknown;
    bool fReport = false; // skip unknown style warnings

    for (size_t i = 0; i < GetCount(); i++) {
        STSEntry& stse = GetAt(i);

        STSStyle* val;
        if (!m_styles.Lookup(stse.style, val)) {
            if (!unknown.Lookup(stse.style, val)) {
                if (fReport) {
                    CString msg;
                    msg.Format(_T("Unknown style found: \"%s\", changed to \"Default\"!\n\nPress Cancel to ignore further warnings."), stse.style.GetString());
                    if (MessageBox(nullptr, msg, _T("Warning"), MB_OKCANCEL | MB_ICONWARNING) != IDOK) {
                        fReport = false;
                    }
                }

                unknown[stse.style] = nullptr;
            }

            stse.style = _T("Default");
        }
    }
}

void CSimpleTextSubtitle::AddStyle(CString name, STSStyle* style)
{
    if (name.IsEmpty()) {
        name = _T("Default");
    }

    if (m_bUsingPlayerDefaultStyle && name == _T("Default")) {
        m_bUsingPlayerDefaultStyle = false;
    }

    STSStyle* val;
    if (m_styles.Lookup(name, val)) {
        if (*val == *style) {
            delete style;
            return;
        }

        int i;
        int len = name.GetLength();

        for (i = len; i > 0 && _istdigit(name[i - 1]); i--) {
            ;
        }

        int idx = 1;

        CString name2 = name;

        if (i < len && _stscanf_s(name.Right(len - i), _T("%d"), &idx) == 1) {
            name2 = name.Left(i);
        }

        idx++;

        CString name3;
        do {
            name3.Format(_T("%s%d"), name2.GetString(), idx);
            idx++;
        } while (m_styles.Lookup(name3));

        m_styles.RemoveKey(name);
        m_styles[name3] = val;

        for (size_t j = 0, count = GetCount(); j < count; j++) {
            STSEntry& stse = GetAt(j);
            if (stse.style == name) {
                stse.style = name3;
            }
        }
    }

    m_styles[name] = style;
}

bool CSimpleTextSubtitle::SetDefaultStyle(const STSStyle& s)
{
    STSStyle* val;
    if (m_styles.Lookup(_T("Default"), val)) {
        *val = s;
    } else {
        val = DEBUG_NEW STSStyle();
        *val = s;
        m_styles[L"Default"] = val;
        m_bUsingPlayerDefaultStyle = true;
    }

    bool changed = (s != m_SubRendererSettings.defaultStyle);
    if (changed) {
        m_SubRendererSettings.defaultStyle = s;
        SetStyleChanged();
    }
    return true;
}

void CSimpleTextSubtitle::SetStyleChanged() {
#if USE_LIBASS
    if (m_LibassContext.IsLibassActive()) {
        m_LibassContext.StylesChanged();
    }
#endif
}

bool CSimpleTextSubtitle::GetDefaultStyle(STSStyle& s) const
{
    STSStyle* val;
    if (!m_styles.Lookup(_T("Default"), val)) {
        return false;
    }
    s = *val;
    return true;
}

void CSimpleTextSubtitle::ConvertToTimeBased(double fps)
{
    if (m_mode == TIME) {
        return;
    }

    for (size_t i = 0, j = GetCount(); i < j; i++) {
        STSEntry& stse = (*this)[i];
        stse.start = std::llround(stse.start * UNITS_FLOAT / fps);
        stse.end   = std::llround(stse.end * UNITS_FLOAT / fps);
    }

    m_mode = TIME;

    CreateSegments();
}

void CSimpleTextSubtitle::ConvertToFrameBased(double fps)
{
    if (m_mode == FRAME) {
        return;
    }

    for (size_t i = 0, j = GetCount(); i < j; i++) {
        STSEntry& stse = (*this)[i];
        stse.start = std::llround(stse.start * fps / UNITS);
        stse.end   = std::llround(stse.end * fps / UNITS);
    }

    m_mode = FRAME;

    CreateSegments();
}

int CSimpleTextSubtitle::SearchSub(REFERENCE_TIME t, double fps)
{
    int i = 0, j = (int)GetCount() - 1, ret = -1;

    if (j >= 0 && t >= TranslateStart(j, fps)) {
        return j;
    }

    while (i < j) {
        int mid = (i + j) >> 1;

        REFERENCE_TIME midt = TranslateStart(mid, fps);

        if (t == midt) {
            while (mid > 0 && t == TranslateStart(mid - 1, fps)) {
                --mid;
            }
            ret = mid;
            break;
        } else if (t < midt) {
            ret = -1;
            if (j == mid) {
                mid--;
            }
            j = mid;
        } else if (t > midt) {
            ret = mid;
            if (i == mid) {
                ++mid;
            }
            i = mid;
        }
    }

    return ret;
}

const STSSegment* CSimpleTextSubtitle::SearchSubs(REFERENCE_TIME t, double fps, /*[out]*/ int* iSegment, int* nSegments)
{
    int i = 0, j = (int)m_segments.GetCount() - 1, ret = -1;

    if (nSegments) {
        *nSegments = j + 1;
    }

    // last segment
    if (j >= 0 && t >= TranslateSegmentStart(j, fps) && t < TranslateSegmentEnd(j, fps)) {
        if (iSegment) {
            *iSegment = j;
        }
        return &m_segments[j];
    }

    // after last segment
    if (j >= 0 && t >= TranslateSegmentEnd(j, fps)) {
        if (iSegment) {
            *iSegment = j + 1;
        }
        return nullptr;
    }

    // before first segment
    if (j > 0 && t < TranslateSegmentStart(i, fps)) {
        if (iSegment) {
            *iSegment = -1;
        }
        return nullptr;
    }

    while (i < j) {
        int mid = (i + j) >> 1;

        REFERENCE_TIME midt = TranslateSegmentStart(mid, fps);

        if (t == midt) {
            ret = mid;
            break;
        } else if (t < midt) {
            ret = -1;
            if (j == mid) {
                mid--;
            }
            j = mid;
        } else if (t > midt) {
            ret = mid;
            if (i == mid) {
                mid++;
            }
            i = mid;
        }
    }

    if (0 <= ret && (size_t)ret < m_segments.GetCount()) {
        if (iSegment) {
            *iSegment = ret;
        }
    }

    if (0 <= ret && (size_t)ret < m_segments.GetCount()
            && !m_segments[ret].subs.IsEmpty()
            && TranslateSegmentStart(ret, fps) <= t && t < TranslateSegmentEnd(ret, fps)) {
        return &m_segments[ret];
    }

    return nullptr;
}

REFERENCE_TIME CSimpleTextSubtitle::TranslateStart(int i, double fps)
{
    return (i < 0 || GetCount() <= (size_t)i ? -1 :
            m_mode == TIME ? GetAt(i).start :
            m_mode == FRAME ? std::llround(GetAt(i).start * UNITS_FLOAT / fps) :
            0);
}

REFERENCE_TIME CSimpleTextSubtitle::TranslateEnd(int i, double fps)
{
    return (i < 0 || GetCount() <= (size_t)i ? -1 :
            m_mode == TIME ? GetAt(i).end :
            m_mode == FRAME ? std::llround(GetAt(i).end * UNITS_FLOAT / fps) :
            0);
}

REFERENCE_TIME CSimpleTextSubtitle::TranslateSegmentStart(int i, double fps)
{
    return (i < 0 || m_segments.GetCount() <= (size_t)i ? -1 :
            m_mode == TIME ? m_segments[i].start :
            m_mode == FRAME ? std::llround(m_segments[i].start * UNITS_FLOAT / fps) :
            0);
}

REFERENCE_TIME CSimpleTextSubtitle::TranslateSegmentEnd(int i, double fps)
{
    return (i < 0 || m_segments.GetCount() <= (size_t)i ? -1 :
            m_mode == TIME ? m_segments[i].end :
            m_mode == FRAME ? std::llround(m_segments[i].end * UNITS_FLOAT / fps) :
            0);
}

STSStyle* CSimpleTextSubtitle::GetStyle(int i)
{
    STSStyle* style = nullptr;
    CString stylename = GetAt(i).style;
    m_styles.Lookup(stylename, style);

    if (!style) {
        m_styles.Lookup(_T("Default"), style);
    }

    return style;
}

void CSimpleTextSubtitle::UpdateSubRelativeTo(Subtitle::SubType type, STSStyle::RelativeTo& relativeTo) {
    if (relativeTo == STSStyle::AUTO) {
        if (type == Subtitle::ASS || type == Subtitle::SSA) {
            relativeTo = STSStyle::VIDEO;
        } else {
            relativeTo = STSStyle::WINDOW;
        }
    }
}

bool CSimpleTextSubtitle::GetStyle(int i, STSStyle& stss)
{
    STSStyle* style = nullptr;
    m_styles.Lookup(GetAt(i).style, style);

    STSStyle* defstyle = nullptr;

    if (!style) {
        m_styles.Lookup(_T("Default"), defstyle);
        if (!defstyle) {
            defstyle = CreateDefaultStyle(DEFAULT_CHARSET);
        }

        style = defstyle;
    }

    if (!style) {
        ASSERT(0);
        return false;
    }

    stss = *style;

    if (stss.relativeTo == STSStyle::AUTO) {
        if (!defstyle) {
            m_styles.Lookup(_T("Default"), defstyle);
        }
        if (defstyle) {
            stss.relativeTo = defstyle->relativeTo;
            // If relative to is set to "auto" even for the default style, decide based on the subtitle type
            UpdateSubRelativeTo(m_subtitleType, stss.relativeTo);
        }
    }

    return true;
}

bool CSimpleTextSubtitle::GetStyle(CString styleName, STSStyle& stss)
{
    STSStyle* style = nullptr;
    m_styles.Lookup(styleName, style);
    if (!style) {
        return false;
    }

    stss = *style;

    if (stss.relativeTo == STSStyle::AUTO) {
        STSStyle* defstyle = nullptr;
        m_styles.Lookup(_T("Default"), defstyle);
        if (defstyle) {
            stss.relativeTo = defstyle->relativeTo;
            // If relative to is set to "auto" even for the default style, decide based on the subtitle type
            UpdateSubRelativeTo(m_subtitleType, stss.relativeTo);
        }
    }

    return true;
}

int CSimpleTextSubtitle::GetCharSet(int charSet)
{
    if (overrideANSICharset >= DEFAULT_CHARSET && (charSet == DEFAULT_CHARSET || charSet == ANSI_CHARSET)) {
        return overrideANSICharset;
    }
    return charSet;
}

int CSimpleTextSubtitle::GetStyleCharSet(int i)
{
    const STSStyle* stss = GetStyle(i);
    return GetCharSet(stss ? stss->charSet : DEFAULT_CHARSET);
}

bool CSimpleTextSubtitle::IsEntryUnicode(int i)
{
    return GetAt(i).fUnicode;
}

void CSimpleTextSubtitle::ConvertUnicode(int i, bool fUnicode)
{
    STSEntry& stse = GetAt(i);

    if (stse.fUnicode ^ fUnicode) {
        int CharSet = GetStyleCharSet(i);

        stse.str = fUnicode
                   ? MBCSSSAToUnicode(stse.str, CharSet)
                   : UnicodeSSAToMBCS(stse.str, CharSet);

        stse.fUnicode = fUnicode;
    }
}

CStringA CSimpleTextSubtitle::GetStrA(int i, bool fSSA)
{
    return WToA(GetStrWA(i, fSSA));
}

CStringW CSimpleTextSubtitle::GetStrW(int i, bool fSSA)
{
    STSEntry const& stse = GetAt(i);
    int CharSet = GetStyleCharSet(i);

    CStringW str = stse.str;

    if (!stse.fUnicode) {
        str = MBCSSSAToUnicode(str, CharSet);
    }

    if (!fSSA) {
        str = RemoveSSATags(str, true, CharSet);
    }

    return str;
}

CStringW CSimpleTextSubtitle::GetStrWA(int i, bool fSSA)
{
    STSEntry const& stse = GetAt(i);
    int CharSet = GetStyleCharSet(i);

    CStringW str = stse.str;

    if (stse.fUnicode) {
        str = UnicodeSSAToMBCS(str, CharSet);
    }

    if (!fSSA) {
        str = RemoveSSATags(str, false, CharSet);
    }

    return str;
}

void CSimpleTextSubtitle::SetStr(int i, CStringA str, bool fUnicode)
{
    SetStr(i, AToW(str), false);
}

void CSimpleTextSubtitle::SetStr(int i, CStringW str, bool fUnicode)
{
    STSEntry& stse = GetAt(i);

    str.Replace(L"\n", L"\\N");

    if (stse.fUnicode && !fUnicode) {
        stse.str = MBCSSSAToUnicode(str, GetStyleCharSet(i));
    } else if (!stse.fUnicode && fUnicode) {
        stse.str = UnicodeSSAToMBCS(str, GetStyleCharSet(i));
    } else {
        stse.str = str;
    }
}

static inline bool comp1(const STSEntry& lhs, const STSEntry& rhs)
{
    if (lhs.start != rhs.start) {
        return lhs.start < rhs.start;
    }
    if (lhs.layer != rhs.layer) {
        return lhs.layer < rhs.layer;
    }
    return lhs.readorder < rhs.readorder;
}

static inline bool comp2(const STSEntry& lhs, const STSEntry& rhs)
{
    return lhs.readorder < rhs.readorder;
}

void CSimpleTextSubtitle::Sort(bool fRestoreReadorder)
{
    std::sort(GetData(), GetData() + GetCount(), !fRestoreReadorder ? comp1 : comp2);
    CreateSegments();
}

struct Breakpoint {
    REFERENCE_TIME t;
    bool isStart;

    Breakpoint(REFERENCE_TIME t, bool isStart) : t(t), isStart(isStart) {};
};

static int BreakpointComp(const void* e1, const void* e2)
{
    const Breakpoint* bp1 = (const Breakpoint*)e1;
    const Breakpoint* bp2 = (const Breakpoint*)e2;

    return SGN(bp1->t - bp2->t);
}

void CSimpleTextSubtitle::CreateSegments()
{
    m_segments.RemoveAll();

    CAtlArray<Breakpoint> breakpoints;

    for (size_t i = 0; i < GetCount(); i++) {
        STSEntry& stse = GetAt(i);
        breakpoints.Add(Breakpoint(stse.start, true));
        breakpoints.Add(Breakpoint(stse.end, false));
    }

    qsort(breakpoints.GetData(), breakpoints.GetCount(), sizeof(Breakpoint), BreakpointComp);

    ptrdiff_t startsCount = 0;
    for (size_t i = 1, end = breakpoints.GetCount(); i < end; i++) {
        startsCount += breakpoints[i - 1].isStart ? +1 : -1;
        if (breakpoints[i - 1].t != breakpoints[i].t && startsCount > 0) {
            m_segments.Add(STSSegment(breakpoints[i - 1].t, breakpoints[i].t));
        }
    }

    STSSegment* segmentsStart = m_segments.GetData();
    STSSegment* segmentsEnd   = segmentsStart + m_segments.GetCount();
    for (size_t i = 0; i < GetCount(); i++) {
        const STSEntry& stse = GetAt(i);
        STSSegment* segment = std::lower_bound(segmentsStart, segmentsEnd, stse.start, SegmentCompStart);
        for (size_t j = segment - segmentsStart; j < m_segments.GetCount() && m_segments[j].end <= stse.end; j++) {
            m_segments[j].subs.Add(int(i));
        }
    }

    OnChanged();
    /*
        for (size_t i = 0, j = m_segments.GetCount(); i < j; i++) {
            STSSegment& stss = m_segments[i];

            TRACE(_T("%d - %d"), stss.start, stss.end);

            for (size_t k = 0, l = stss.subs.GetCount(); k < l; k++) {
                TRACE(_T(", %d"), stss.subs[k]);
            }

            TRACE(_T("\n"));
        }
    */
}

void CSimpleTextSubtitle::FlushEventsLibass()
{
    if (m_LibassContext.IsLibassActive()) {
        ass_flush_events(m_LibassContext.m_track.get());
    }
}

bool CSimpleTextSubtitle::Open(CString fn, int CharSet, CString name, CString videoName)
{
    Empty();

    CWebTextFile f(CTextFile::UTF8);
    if (!f.Open(fn)) {
        return false;
    }

    if (name.IsEmpty()) {
        name = Subtitle::GuessSubtitleName(fn, videoName, m_lcid, m_langname, m_eHearingImpaired);
    }

    return Open(&f, CharSet, name);
}

bool CSimpleTextSubtitle::Open(BYTE* data, int length, int CharSet, CString provider, CString lang, CString ext) {
    Empty();

    m_provider = provider;
    CString name;
    name.Format(_T("%s.%s"), static_cast<LPCWSTR>(lang), static_cast<LPCWSTR>(ext));
    CW2A temp(lang);
    m_lcid = ISOLang::ISO6391ToLcid(temp);
    if (m_lcid > 0) {
        m_langname = ISOLang::LCIDToLanguage(m_lcid);
    }
    if (m_langname.IsEmpty()) {
        m_langname = ISOLang::ISO639XToLanguage(temp);
    }
    return Open(data, length, CharSet, name);
}

bool CSimpleTextSubtitle::Open(CString data, CTextFile::enc SaveCharSet, int ReadCharSet, CString provider, CString lang, CString ext) {
    Empty();

    m_provider = provider;
    CString name;
    name.Format(_T("%s.%s"), static_cast<LPCWSTR>(lang), static_cast<LPCWSTR>(ext));
    CW2A temp(lang);
    m_lcid = ISOLang::ISO6391ToLcid(temp);
    if (m_lcid > 0) {
        m_langname = ISOLang::LCIDToLanguage(m_lcid);
    }
    if (m_langname.IsEmpty()) {
        m_langname = ISOLang::ISO639XToLanguage(temp);
    }
    TCHAR path[MAX_PATH];
    if (!GetTempPath(MAX_PATH, path)) {
        return false;
    }

    TCHAR fn[MAX_PATH];
    if (!GetTempFileName(path, _T("vs"), 0, fn)) {
        return false;
    }

    CTextFile f;
    if (!f.Save(fn, SaveCharSet)) {
        return false;
    }

    f.WriteString(data);
    f.Flush();
    f.Close();

    bool fRet = Open(fn, ReadCharSet, name);

    _tremove(fn);

    m_path = _T("");

    return fRet;
}

bool CSimpleTextSubtitle::Open(CTextFile* f, int CharSet, CString name) {
    Empty();

    if (m_langname.IsEmpty() && m_lcid > 0) {
        m_langname = ISOLang::LCIDToLanguage(m_lcid);
    }

    auto initRes = [&]() {
        if (m_layoutRes.cx > 0 && m_layoutRes.cy > 0) {
            m_storageRes = m_layoutRes;
        } else if (m_storageRes.cx <= 0 || m_storageRes.cy <= 0) {
            if (m_playRes.cx > 0 && m_playRes.cy > 0) {
                m_storageRes = m_playRes;
            } else {
                m_storageRes = CSize(384, 288);
            }
        }
        if (m_playRes.cx <= 0 || m_playRes.cy <= 0) {
            m_playRes = m_storageRes;
        }
    };

    auto loadSSAStyle = [&]() {
        CWebTextFile f2(CTextFile::UTF8);
        if (f2.Open(f->GetFilePath() + _T(".style"))) {
            OpenSubStationAlpha(&f2, *this, CharSet);
        }
    };

    auto setVars = [&](auto name, auto encoding, auto mode) {
        m_name = name;
        m_encoding = encoding;
        m_mode = mode;
    };

    ULONGLONG pos = f->GetPosition();

    auto functs = PreferredOpenFuncts(f->GetFilePath());

    for (int i: functs) {
        if (!OpenFuncts[i].open(f, *this, CharSet)) {
            if (!IsEmpty()) {
                CString lastLine;
                size_t n = CountLines(f, pos, f->GetPosition(), lastLine);
                CString msg;
                msg.Format(_T("Unable to parse the subtitle file. Syntax error at line %Iu:\n\"%s\""), n + 1, lastLine.GetString());
                AfxMessageBox(msg, MB_OK | MB_ICONERROR);
                Empty();
                break;
            }

            f->Seek(pos, CFile::begin);
            Empty();
            continue;
        }

        m_path = f->GetFilePath();
        m_subtitleType = OpenFuncts[i].type;
        setVars(name, f->GetEncoding(), OpenFuncts[i].mode);
        CreateDefaultStyle(CharSet);

#if USE_LIBASS
        if ((m_subtitleType == Subtitle::SubType::SSA || m_subtitleType == Subtitle::SubType::ASS) && m_SubRendererSettings.renderSSAUsingLibass) {
            m_LibassContext.LoadASSFile(Subtitle::SubType::SSA);
        } else if (m_subtitleType == Subtitle::SubType::SRT && m_SubRendererSettings.renderSRTUsingLibass) {
            m_LibassContext.LoadASSFile(Subtitle::SubType::SRT);
        }
        if (m_LibassContext.IsLibassActive()) {
            // we are done now
            // note: the subtitle data loaded by internal parser is kept so that saving (downloaded) subtitle works
            return true;
        }
#endif

        loadSSAStyle();
        ChangeUnknownStylesToDefault();
        initRes();

        return true;
    }

    m_path = _T("");
    return false;
}

bool CSimpleTextSubtitle::Open(CString provider, BYTE* data, int len, int CharSet, CString name, Subtitle::HearingImpairedType eHearingImpaired, LCID lcid)
{
    m_provider = provider;
    m_eHearingImpaired = eHearingImpaired;
    m_lcid = lcid;

    return Open(data, len, CharSet, name);
}

bool CSimpleTextSubtitle::Open(BYTE* data, int len, int CharSet, CString name)
{
    TCHAR path[MAX_PATH];
    if (!GetTempPath(MAX_PATH, path)) {
        return false;
    }

    TCHAR fn[MAX_PATH];
    if (!GetTempFileName(path, _T("vs"), 0, fn)) {
        return false;
    }

    FILE* tmp = nullptr;
    if (_tfopen_s(&tmp, fn, _T("wb"))) {
        return false;
    }

    int i = 0;
    for (; i <= (len - 1024); i += 1024) {
        fwrite(&data[i], 1024, 1, tmp);
    }
    if (len > i) {
        fwrite(&data[i], len - i, 1, tmp);
    }

    fclose(tmp);

    bool fRet = Open(fn, CharSet, name);

    _tremove(fn);

    m_path = _T("");

    return fRet;
}

bool CSimpleTextSubtitle::SaveAs(CString fn, Subtitle::SubType type,
                                 double fps /*= -1*/, LONGLONG delay /*= 0*/,
                                 CTextFile::enc e /*= CTextFile::DEFAULT_ENCODING*/, bool bCreateExternalStyleFile /*= true*/)
{
    LPCTSTR ext = Subtitle::GetSubtitleFileExt(type);
    if (ext && fn.Mid(fn.ReverseFind('.') + 1).CompareNoCase(ext)) {
        if (fn[fn.GetLength() - 1] != '.') {
            fn += _T(".");
        }
        fn += ext;
    }

    CTextFile f;
    if (!f.Save(fn, e)) {
        return false;
    }

    if (type == Subtitle::SMI) {
        CString str;

        str += _T("<SAMI>\n<HEAD>\n");
        str += _T("<STYLE TYPE=\"text/css\">\n");
        str += _T("<!--\n");
        str += _T("P {margin-left: 16pt; margin-right: 16pt; margin-bottom: 16pt; margin-top: 16pt;\n");
        str += _T("   text-align: center; font-size: 18pt; font-family: calibri; font-weight: bold; color: #f0f0f0;}\n");
        str += _T(".UNKNOWNCC {Name:Unknown; lang:en-US; SAMIType:CC;}\n");
        str += _T("-->\n");
        str += _T("</STYLE>\n");
        str += _T("</HEAD>\n");
        str += _T("\n");
        str += _T("<BODY>\n");

        f.WriteString(str);
    } else if (type == Subtitle::SSA || type == Subtitle::ASS) {
        CString str;

        str  = _T("[Script Info]\n");
        str += _T("; Note: This file was saved by MPC-HC.\n");
        str += (type == Subtitle::SSA) ? _T("ScriptType: v4.00\n") : _T("ScriptType: v4.00+\n");
        str += (m_collisions == 0) ? _T("Collisions: Normal\n") : _T("Collisions: Reverse\n");
        if (type == Subtitle::ASS && m_scaledBAS >= 0) {
            if (m_scaledBAS == 1) {
                str += _T("ScaledBorderAndShadow: yes\n");
            } else {
                str += _T("ScaledBorderAndShadow: no\n");
            }
        }
        if (m_sYCbCrMatrix.IsEmpty()) {
            str += _T("YCbCr Matrix: None\n");
        } else {
            str += _T("YCbCr Matrix: ") + m_sYCbCrMatrix + _T("\n");
        }
        str.AppendFormat(_T("PlayResX: %d\n"), m_playRes.cx);
        str.AppendFormat(_T("PlayResY: %d\n"), m_playRes.cy);
        if (m_layoutRes.cx > 0 && m_layoutRes.cy > 0) {
            str.AppendFormat(_T("LayoutResX: %d\n"), m_layoutRes.cx);
            str.AppendFormat(_T("LayoutResY: %d\n"), m_layoutRes.cy);
        }
        str += _T("Timer: 100.0000\n");
        str += _T("\n");
        str += (type == Subtitle::SSA)
               ? _T("[V4 Styles]\nFormat: Name, Fontname, Fontsize, PrimaryColour, SecondaryColour, TertiaryColour, BackColour, Bold, Italic, BorderStyle, Outline, Shadow, Alignment, MarginL, MarginR, MarginV, AlphaLevel, Encoding\n")
               : _T("[V4+ Styles]\nFormat: Name, Fontname, Fontsize, PrimaryColour, SecondaryColour, OutlineColour, BackColour, Bold, Italic, Underline, StrikeOut, ScaleX, ScaleY, Spacing, Angle, BorderStyle, Outline, Shadow, Alignment, MarginL, MarginR, MarginV, Encoding\n");

        f.WriteString(str);

        str  = (type == Subtitle::SSA)
               ? _T("Style: %s,%s,%d,&H%06x,&H%06x,&H%06x,&H%06x,%d,%d,%d,%.2f,%.2f,%d,%d,%d,%d,%d,%d\n")
               : _T("Style: %s,%s,%d,&H%08x,&H%08x,&H%08x,&H%08x,%d,%d,%d,%d,%.2f,%.2f,%.2f,%.2f,%d,%.2f,%.2f,%d,%d,%d,%d,%d\n");

        POSITION pos = m_styles.GetStartPosition();
        while (pos) {
            CString key;
            STSStyle* s;
            m_styles.GetNextAssoc(pos, key, s);

            if (type == Subtitle::SSA) {
                CString str2;
                str2.Format(str, key.GetString(),
                            s->fontName.GetString(), (int)s->fontSize,
                            s->colors[0] & 0xffffff,
                            s->colors[1] & 0xffffff,
                            s->colors[2] & 0xffffff,
                            s->colors[3] & 0xffffff,
                            s->fontWeight > FW_NORMAL ? -1 : 0, s->fItalic ? -1 : 0,
                            s->borderStyle == 0 ? 1 : s->borderStyle == 1 ? 3 : 0,
                            s->outlineWidthY, s->shadowDepthY,
                            s->scrAlignment <= 3 ? s->scrAlignment : s->scrAlignment <= 6 ? ((s->scrAlignment - 3) | 8) : s->scrAlignment <= 9 ? ((s->scrAlignment - 6) | 4) : 2,
                            s->marginRect.left, s->marginRect.right, (s->marginRect.top + s->marginRect.bottom) / 2,
                            s->alpha[0],
                            s->charSet);
                f.WriteString(str2);
            } else {
                CString str2;
                str2.Format(str, key.GetString(),
                            s->fontName.GetString(), (int)s->fontSize,
                            (s->colors[0] & 0xffffff) | (s->alpha[0] << 24),
                            (s->colors[1] & 0xffffff) | (s->alpha[1] << 24),
                            (s->colors[2] & 0xffffff) | (s->alpha[2] << 24),
                            (s->colors[3] & 0xffffff) | (s->alpha[3] << 24),
                            s->fontWeight > FW_NORMAL ? -1 : 0,
                            s->fItalic ? -1 : 0, s->fUnderline ? -1 : 0, s->fStrikeOut ? -1 : 0,
                            s->fontScaleX, s->fontScaleY,
                            s->fontSpacing, s->fontAngleZ,
                            s->borderStyle == 0 ? 1 : s->borderStyle == 1 ? 3 : 0,
                            s->outlineWidthY, s->shadowDepthY,
                            s->scrAlignment,
                            s->marginRect.left, s->marginRect.right, (int)((s->marginRect.top + s->marginRect.bottom) / 2),
                            s->charSet);
                f.WriteString(str2);
            }
        }

        if (!IsEmpty()) {
            str  = _T("\n");
            str += _T("[Events]\n");
            str += (type == Subtitle::SSA)
                   ? _T("Format: Marked, Start, End, Style, Name, MarginL, MarginR, MarginV, Effect, Text\n")
                   : _T("Format: Layer, Start, End, Style, Actor, MarginL, MarginR, MarginV, Effect, Text\n");
            f.WriteString(str);
        }
    }

    CStringW fmt =
        type == Subtitle::SRT ? L"%d\n%02d:%02d:%02d,%03d --> %02d:%02d:%02d,%03d\n%s\n\n" :
        type == Subtitle::SUB ? L"{%d}{%d}%s\n" :
        type == Subtitle::SMI ? L"<SYNC Start=%d><P Class=UNKNOWNCC>\n%s\n<SYNC Start=%d><P Class=UNKNOWNCC>&nbsp;\n" :
        type == Subtitle::PSB ? L"{%d:%02d:%02d}{%d:%02d:%02d}%s\n" :
        type == Subtitle::SSA ? L"Dialogue: Marked=0,%d:%02d:%02d.%02d,%d:%02d:%02d.%02d,%s,%s,%04d,%04d,%04d,%s,%s\n" :
        type == Subtitle::ASS ? L"Dialogue: %d,%d:%02d:%02d.%02d,%d:%02d:%02d.%02d,%s,%s,%04d,%04d,%04d,%s,%s\n" :
        L"";
    //  Sort(true);

    if (m_mode == FRAME) {
        delay = std::lround(delay * fps / 1000.0);
    }

    for (int i = 0, j = (int)GetCount(), k = 0; i < j; i++) {
        STSEntry& stse = GetAt(i);

        int t1 = (int)(RT2MS(TranslateStart(i, fps)) + delay);
        if (t1 < 0) {
            k++;
            continue;
        }

        int t2 = (int)(RT2MS(TranslateEnd(i, fps)) + delay);

        int hh1 = (t1 / 60 / 60 / 1000);
        int mm1 = (t1 / 60 / 1000) % 60;
        int ss1 = (t1 / 1000) % 60;
        int ms1 = (t1) % 1000;
        int hh2 = (t2 / 60 / 60 / 1000);
        int mm2 = (t2 / 60 / 1000) % 60;
        int ss2 = (t2 / 1000) % 60;
        int ms2 = (t2) % 1000;

        CStringW str = f.IsUnicode()
                       ? GetStrW(i, type == Subtitle::SSA || type == Subtitle::ASS)
                       : GetStrWA(i, type == Subtitle::SSA || type == Subtitle::ASS);

        CStringW str2;

        if (type == Subtitle::SRT) {
            str2.Format(fmt, i - k + 1, hh1, mm1, ss1, ms1, hh2, mm2, ss2, ms2, str.GetString());
        } else if (type == Subtitle::SUB) {
            str.Replace('\n', '|');
            str2.Format(fmt, int(t1 * fps / 1000), int(t2 * fps / 1000), str.GetString());
        } else if (type == Subtitle::SMI) {
            str.Replace(L"\n", L"<br>");
            str2.Format(fmt, t1, str.GetString(), t2);
        } else if (type == Subtitle::PSB) {
            str.Replace('\n', '|');
            str2.Format(fmt, hh1, mm1, ss1, hh2, mm2, ss2, str.GetString());
        } else if (type == Subtitle::SSA) {
            str.Replace(L"\n", L"\\N");
            str2.Format(fmt,
                        hh1, mm1, ss1, ms1 / 10,
                        hh2, mm2, ss2, ms2 / 10,
                        TToW(stse.style).GetString(), TToW(stse.actor).GetString(),
                        stse.marginRect.left, stse.marginRect.right, (stse.marginRect.top + stse.marginRect.bottom) / 2,
                        TToW(stse.effect).GetString(), str.GetString());
        } else if (type == Subtitle::ASS) {
            str.Replace(L"\n", L"\\N");
            str2.Format(fmt,
                        stse.layer,
                        hh1, mm1, ss1, ms1 / 10,
                        hh2, mm2, ss2, ms2 / 10,
                        TToW(stse.style).GetString(), TToW(stse.actor).GetString(),
                        stse.marginRect.left, stse.marginRect.right, (stse.marginRect.top + stse.marginRect.bottom) / 2,
                        TToW(stse.effect).GetString(), str.GetString());
        }

        f.WriteString(str2);
    }

    //  Sort();

    if (type == Subtitle::SMI) {
        f.WriteString(_T("</BODY>\n</SAMI>\n"));
    }

    STSStyle* s;
    if (bCreateExternalStyleFile && !m_bUsingPlayerDefaultStyle && m_styles.Lookup(_T("Default"), s) && type != Subtitle::SSA && type != Subtitle::ASS) {
        CTextFile file;
        if (!file.Save(fn + _T(".style"), e)) {
            return false;
        }

        CString str, str2;

        str += _T("ScriptType: v4.00+\n");
        str += _T("PlayResX: %d\n");
        str += _T("PlayResY: %d\n");
        str += _T("\n");
        str += _T("[V4+ Styles]\nFormat: Name, Fontname, Fontsize, PrimaryColour, SecondaryColour, OutlineColour, BackColour, Bold, Italic, Underline, StrikeOut, ScaleX, ScaleY, Spacing, Angle, BorderStyle, Outline, Shadow, Alignment, MarginL, MarginR, MarginV, Encoding\n");
        str2.Format(str, m_storageRes.cx, m_storageRes.cy);
        file.WriteString(str2);

        str  = _T("Style: Default,%s,%d,&H%08x,&H%08x,&H%08x,&H%08x,%d,%d,%d,%d,%.2f,%.2f,%.2f,%.2f,%d,%.2f,%.2f,%d,%d,%d,%d,%d\n");
        str2.Format(str,
                    s->fontName.GetString(), (int)s->fontSize,
                    (s->colors[0] & 0xffffff) | (s->alpha[0] << 24),
                    (s->colors[1] & 0xffffff) | (s->alpha[1] << 24),
                    (s->colors[2] & 0xffffff) | (s->alpha[2] << 24),
                    (s->colors[3] & 0xffffff) | (s->alpha[3] << 24),
                    s->fontWeight > FW_NORMAL ? -1 : 0,
                    s->fItalic ? -1 : 0, s->fUnderline ? -1 : 0, s->fStrikeOut ? -1 : 0,
                    s->fontScaleX, s->fontScaleY,
                    s->fontSpacing, s->fontAngleZ,
                    s->borderStyle == 0 ? 1 : s->borderStyle == 1 ? 3 : 0,
                    s->outlineWidthY, s->shadowDepthY,
                    s->scrAlignment,
                    s->marginRect.left, s->marginRect.right, (int)((s->marginRect.top + s->marginRect.bottom) / 2),
                    s->charSet);
        file.WriteString(str2);
    }

    m_provider = _T("Local");
    m_path = fn;

    return true;
}

////////////////////////////////////////////////////////////////////

STSStyle::STSStyle()
{
    SetDefault();
}

void STSStyle::SetDefault()
{
    marginRect = CRect(20, 20, 20, 20);
    scrAlignment = 2;
    borderStyle = 0;
    outlineWidthX = outlineWidthY = 2;
    shadowDepthX = shadowDepthY = 3;
    colors[0] = 0x00ffffff;
    colors[1] = 0x0000ffff;
    colors[2] = 0x00000000;
    colors[3] = 0x00000000;
    alpha[0] = 0x00;
    alpha[1] = 0x00;
    alpha[2] = 0x00;
    alpha[3] = 0x80;
    charSet = DEFAULT_CHARSET;
    fontName = _T("Calibri");
    fontSize = 18;
    fontScaleX = fontScaleY = 100;
    fontSpacing = 0;
    fontWeight = FW_BOLD;
    fItalic = false;
    fUnderline = false;
    fStrikeOut = false;
    fBlur = 0;
    fGaussianBlur = 0;
    fontShiftX = fontShiftY = fontAngleZ = fontAngleX = fontAngleY = 0;
    relativeTo = STSStyle::AUTO;
    hasAnsiStyleName = false;
#if USE_LIBASS
    Kerning = false;
    ScaledBorderAndShadow = false;
    customTags = L"";
#endif
}

bool STSStyle::operator == (const STSStyle& s) const
{
    return (marginRect == s.marginRect
            && scrAlignment == s.scrAlignment
            && borderStyle == s.borderStyle
            && abs(outlineWidthX - s.outlineWidthX) < 0.00000001
            && abs(outlineWidthY - s.outlineWidthY) < 0.00000001
            && abs(shadowDepthX - s.shadowDepthX) < 0.00000001
            && abs(shadowDepthY - s.shadowDepthY) < 0.00000001
            && colors == s.colors
            && alpha == s.alpha
            && fBlur == s.fBlur
            && abs(fGaussianBlur - s.fGaussianBlur) < 0.00000001
            && relativeTo == s.relativeTo
            && IsFontStyleEqual(s));
}

bool STSStyle::IsFontStyleEqual(const STSStyle& s) const
{
    return (
               charSet == s.charSet
               && fontName == s.fontName
               && fontSize == s.fontSize
               && abs(fontScaleX - s.fontScaleX) < 0.00000001
               && abs(fontScaleY - s.fontScaleY) < 0.00000001
               && abs(fontSpacing - s.fontSpacing) < 0.00000001
               && fontWeight == s.fontWeight
               && fItalic == s.fItalic
               && fUnderline == s.fUnderline
               && fStrikeOut == s.fStrikeOut
               && abs(fontAngleZ - s.fontAngleZ) < 0.00000001
               && abs(fontAngleX - s.fontAngleX) < 0.00000001
               && abs(fontAngleY - s.fontAngleY) < 0.00000001
               && abs(fontShiftX - s.fontShiftX) < 0.00000001
               && abs(fontShiftY - s.fontShiftY) < 0.00000001);
}

STSStyle& STSStyle::operator = (LOGFONT& lf)
{
    charSet = lf.lfCharSet;
    fontName = lf.lfFaceName;
    HDC hDC = GetDC(nullptr);
    fontSize = -MulDiv(lf.lfHeight, 72, GetDeviceCaps(hDC, LOGPIXELSY));
    ReleaseDC(nullptr, hDC);
    //  fontAngleZ = lf.lfEscapement/10.0;
    fontWeight = lf.lfWeight;
    fItalic = lf.lfItalic;
    fUnderline = lf.lfUnderline;
    fStrikeOut = lf.lfStrikeOut;
    return *this;
}

LOGFONTA& operator <<= (LOGFONTA& lfa, const STSStyle& s)
{
    lfa.lfCharSet = (BYTE)s.charSet;
    strncpy_s(lfa.lfFaceName, LF_FACESIZE, CStringA(s.fontName), _TRUNCATE);
    HDC hDC = GetDC(nullptr);
    lfa.lfHeight = -MulDiv((int)(s.fontSize + 0.5), GetDeviceCaps(hDC, LOGPIXELSY), 72);
    ReleaseDC(nullptr, hDC);
    lfa.lfWeight = s.fontWeight;
    lfa.lfItalic = s.fItalic ? -1 : 0;
    lfa.lfUnderline = s.fUnderline ? -1 : 0;
    lfa.lfStrikeOut = s.fStrikeOut ? -1 : 0;
    return lfa;
}

LOGFONTW& operator <<= (LOGFONTW& lfw, const STSStyle& s)
{
    lfw.lfCharSet = (BYTE)s.charSet;
    wcsncpy_s(lfw.lfFaceName, LF_FACESIZE, CStringW(s.fontName), _TRUNCATE);
    HDC hDC = GetDC(nullptr);
    lfw.lfHeight = -MulDiv((int)(s.fontSize + 0.5), GetDeviceCaps(hDC, LOGPIXELSY), 72);
    ReleaseDC(nullptr, hDC);
    lfw.lfWeight = s.fontWeight;
    lfw.lfItalic = s.fItalic ? -1 : 0;
    lfw.lfUnderline = s.fUnderline ? -1 : 0;
    lfw.lfStrikeOut = s.fStrikeOut ? -1 : 0;
    return lfw;
}

CString& operator <<= (CString& style, const STSStyle& s)
{
    style.Format(_T("%d;%d;%d;%d;%d;%d;%f;%f;%f;%f;0x%06lx;0x%06lx;0x%06lx;0x%06lx;0x%02x;0x%02x;0x%02x;0x%02x;%d;%s;%f;%f;%f;%f;%ld;%d;%d;%d;%d;%f;%f;%f;%f;%d"),
                 s.marginRect.left, s.marginRect.right, s.marginRect.top, s.marginRect.bottom,
                 s.scrAlignment, s.borderStyle,
                 s.outlineWidthX, s.outlineWidthY, s.shadowDepthX, s.shadowDepthY,
                 s.colors[0], s.colors[1], s.colors[2], s.colors[3],
                 s.alpha[0], s.alpha[1], s.alpha[2], s.alpha[3],
                 s.charSet,
                 s.fontName.GetString(), s.fontSize,
                 s.fontScaleX, s.fontScaleY,
                 s.fontSpacing, s.fontWeight,
                 s.fItalic, s.fUnderline, s.fStrikeOut, s.fBlur, s.fGaussianBlur,
                 s.fontAngleZ, s.fontAngleX, s.fontAngleY,
                 s.relativeTo);

    return style;
}

STSStyle& operator <<= (STSStyle& s, const CString& style)
{
    s.SetDefault();

    try {
        CStringW str = TToW(style);
        LPCWSTR pszBuff = str;
        int nBuffLength = str.GetLength();
        if (str.Find(';') >= 0) {
            s.marginRect.left = GetInt(pszBuff, nBuffLength, L';');
            s.marginRect.right = GetInt(pszBuff, nBuffLength, L';');
            s.marginRect.top = GetInt(pszBuff, nBuffLength, L';');
            s.marginRect.bottom = GetInt(pszBuff, nBuffLength, L';');
            s.scrAlignment = GetInt(pszBuff, nBuffLength, L';');
            s.borderStyle = GetInt(pszBuff, nBuffLength, L';');
            s.outlineWidthX = GetFloat(pszBuff, nBuffLength, L';');
            s.outlineWidthY = GetFloat(pszBuff, nBuffLength, L';');
            s.shadowDepthX = GetFloat(pszBuff, nBuffLength, L';');
            s.shadowDepthY = GetFloat(pszBuff, nBuffLength, L';');
            for (size_t i = 0; i < 4; i++) {
                s.colors[i] = (COLORREF)GetInt(pszBuff, nBuffLength, L';');
            }
            for (size_t i = 0; i < 4; i++) {
                s.alpha[i] = (BYTE)GetInt(pszBuff, nBuffLength, L';');
            }
            s.charSet = GetInt(pszBuff, nBuffLength, L';');
            s.fontName = WToT(GetStrW(pszBuff, nBuffLength, L';'));
            s.fontSize = GetFloat(pszBuff, nBuffLength, L';');
            s.fontScaleX = GetFloat(pszBuff, nBuffLength, L';');
            s.fontScaleY = GetFloat(pszBuff, nBuffLength, L';');
            s.fontSpacing = GetFloat(pszBuff, nBuffLength, L';');
            s.fontWeight = GetInt(pszBuff, nBuffLength, L';');
            s.fItalic = GetInt(pszBuff, nBuffLength, L';');
            s.fUnderline = GetInt(pszBuff, nBuffLength, L';');
            s.fStrikeOut = GetInt(pszBuff, nBuffLength, L';');
            s.fBlur = GetInt(pszBuff, nBuffLength, L';');
            s.fGaussianBlur = GetFloat(pszBuff, nBuffLength, L';');
            s.fontAngleZ = GetFloat(pszBuff, nBuffLength, L';');
            s.fontAngleX = GetFloat(pszBuff, nBuffLength, L';');
            s.fontAngleY = GetFloat(pszBuff, nBuffLength, L';');
            s.relativeTo = (STSStyle::RelativeTo)GetInt(pszBuff, nBuffLength, L';');
        }
    } catch (...) {
        s.SetDefault();
    }

    return s;
}

static bool OpenRealText(CTextFile* file, CSimpleTextSubtitle& ret, int CharSet)
{
    std::wstring szFile;
    CStringW buff;

    while (file->ReadString(buff)) {
        FastTrim(buff);
        if (buff.IsEmpty()) {
            continue;
        }

        // Make sure that the subtitle file starts with a <window> tag
        if (szFile.empty() && buff.CompareNoCase(_T("<window")) < 0) {
            return false;
        }

        szFile += _T("\n") + buff;
    }

    CRealTextParser RealTextParser;
    if (!RealTextParser.ParseRealText(szFile)) {
        return false;
    }

    CRealTextParser::Subtitles crRealText = RealTextParser.GetParsedSubtitles();

    for (auto i = crRealText.m_mapLines.cbegin(); i != crRealText.m_mapLines.cend(); ++i) {
        ret.Add(
            SubRipper2SSA(i->second.c_str()),
            file->IsUnicode(),
            MS2RT(i->first.first),
            MS2RT(i->first.second));
    }

    return !ret.IsEmpty();
}

void CSTSStyleMap::Free() {
    POSITION pos = GetStartPosition();
    while (pos) {
        CString key;
        STSStyle* val;
        GetNextAssoc(pos, key, val);
        delete val;
    }

    RemoveAll();
}
