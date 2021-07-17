/*
 * (C) 2003-2006 Gabest
 * (C) 2006-2014, 2016-2017 see Authors.txt
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
#include <atlutil.h>
#include "text.h"
#include <memory>
#include <regex>
#include <array>

DWORD CharSetToCodePage(DWORD dwCharSet)
{
    if (dwCharSet == CP_UTF8) {
        return CP_UTF8;
    }
    if (dwCharSet == CP_UTF7) {
        return CP_UTF7;
    }
    CHARSETINFO cs;
    ZeroMemory(&cs, sizeof(CHARSETINFO));
    ::TranslateCharsetInfo((DWORD*)(DWORD_PTR)dwCharSet, &cs, TCI_SRCCHARSET);
    return cs.ciACP;
}

CStringA ConvertMBCS(CStringA str, DWORD SrcCharSet, DWORD DstCharSet)
{
    WCHAR* utf16 = DEBUG_NEW WCHAR[str.GetLength() + 1];
    ZeroMemory(utf16, (str.GetLength() + 1)*sizeof(WCHAR));

    CHAR* mbcs = DEBUG_NEW CHAR[str.GetLength() * 6 + 1];
    ZeroMemory(mbcs, str.GetLength() * 6 + 1);

    int len = MultiByteToWideChar(
                  CharSetToCodePage(SrcCharSet),
                  0,
                  str,
                  -1, // null terminated string
                  utf16,
                  str.GetLength() + 1);

    len = WideCharToMultiByte(
              CharSetToCodePage(DstCharSet),
              0,
              utf16,
              len,
              mbcs,
              str.GetLength() * 6,
              nullptr,
              nullptr);

    str = mbcs;

    delete [] utf16;
    delete [] mbcs;

    return str;
}

CStringA UrlEncode(const CStringA& strIn)
{
    CStringA strOut;
    DWORD dwStrLen = 0, dwMaxLength = 0;
    // Request the buffer size needed to encode the URL
    AtlEscapeUrl(strIn, strOut.GetBuffer(), &dwStrLen, dwMaxLength, ATL_URL_ENCODE_PERCENT);
    dwMaxLength = dwStrLen;
    // Encode the URL
    if (dwMaxLength > 0) {
        if (AtlEscapeUrl(strIn, strOut.GetBuffer(int(dwMaxLength)), &dwStrLen, dwMaxLength, ATL_URL_ENCODE_PERCENT)) {
            dwStrLen--;
        } else {
            dwStrLen = 0;
        }
        strOut.ReleaseBuffer(dwStrLen);
    }

    return strOut;
}

CStringA EscapeJSONString(const CStringA& str)
{
    CStringA escapedString = str;
    // replace all of JSON's reserved characters with their escaped
    // equivalents.
    escapedString.Replace("\"", "\\\"");
    escapedString.Replace("\\", "\\\\");
    escapedString.Replace("/", "\\/");
    escapedString.Replace("\b", "\\b");
    escapedString.Replace("\f", "\\f");
    escapedString.Replace("\n", "\\n");
    escapedString.Replace("\r", "\\r");
    escapedString.Replace("\t", "\\t");
    return escapedString;
}

CStringA UrlDecode(const CStringA& strIn)
{
    CStringA strOut;
    DWORD dwStrLen = 0, dwMaxLength = strIn.GetLength() + 1;

    if (AtlUnescapeUrl(strIn, strOut.GetBuffer(int(dwMaxLength)), &dwStrLen, dwMaxLength)) {
        dwStrLen--;
    } else {
        dwStrLen = 0;
    }
    strOut.ReleaseBuffer(dwStrLen);

    return strOut;
}

CStringW UrlDecodeWithUTF8(CStringW in, bool keepEncodedSpecialChar) {
    TCHAR t[100];
    DWORD bufSize = _countof(t);
    CString tem(in);
    tem.Replace(_T("+"), _T(" ")); //UrlUnescape does not deal with '+' properly
    if (keepEncodedSpecialChar) {
        tem.Replace(_T("%25"), _T("%2525"));  // %
        tem.Replace(_T("%3A"), _T("%253A"));
        tem.Replace(_T("%3a"), _T("%253A"));
        tem.Replace(_T("%2F"), _T("%252F"));
        tem.Replace(_T("%2f"), _T("%252F"));
        tem.Replace(_T("%3F"), _T("%253F"));
        tem.Replace(_T("%3f"), _T("%253F"));
        tem.Replace(_T("%23"), _T("%2523"));
        tem.Replace(_T("%5B"), _T("%255B"));
        tem.Replace(_T("%5b"), _T("%255B"));
        tem.Replace(_T("%5D"), _T("%255D"));
        tem.Replace(_T("%5d"), _T("%255D"));
        tem.Replace(_T("%40"), _T("%2540"));
        tem.Replace(_T("%21"), _T("%2521"));
        tem.Replace(_T("%24"), _T("%2524"));
        tem.Replace(_T("%26"), _T("%2526"));
        tem.Replace(_T("%27"), _T("%2527"));
        tem.Replace(_T("%28"), _T("%2528"));
        tem.Replace(_T("%29"), _T("%2529"));
        tem.Replace(_T("%2A"), _T("%252A"));
        tem.Replace(_T("%2a"), _T("%252A"));
        tem.Replace(_T("%2B"), _T("%252B"));
        tem.Replace(_T("%2b"), _T("%252B"));
        tem.Replace(_T("%2C"), _T("%252C"));
        tem.Replace(_T("%2c"), _T("%252C"));
        tem.Replace(_T("%3B"), _T("%253B"));
        tem.Replace(_T("%3b"), _T("%253B"));
        tem.Replace(_T("%3D"), _T("%253D"));
        tem.Replace(_T("%3d"), _T("%253D"));
        tem.Replace(_T("%20"), _T("%2520"));
    }
    HRESULT result = UrlUnescape(tem.GetBuffer(), t, &bufSize, URL_ESCAPE_AS_UTF8); //URL_ESCAPE_AS_UTF8 will work as URL_UNESCAPE_AS_UTF8 on windows 8+, otherwise it will just ignore utf-8

    if (result == E_POINTER) {
        std::shared_ptr<TCHAR[]> buffer(new TCHAR[bufSize]);
        if (S_OK == UrlUnescape(tem.GetBuffer(), buffer.get(), &bufSize, URL_ESCAPE_AS_UTF8)) {
            CString urlDecoded(buffer.get());
            return urlDecoded;
        }
    }
    else {
        CString urlDecoded(t);
        return urlDecoded;
    }
    return in;
}

CStringW URLGetHostName(const CStringW in) {
    CStringW t(in);
    if (t.Find(_T("://")) > 1) {
        t = t.Mid(t.Find(_T("://")) + 3);
    }
    if (t.Left(4) == _T("www.")) {
        t = t.Mid(4);
    }
    if (t.Find(_T("/")) > 0) {
        t = t.Left(t.Find(_T("/")));
    }
    return UrlDecodeWithUTF8(t);
}

CStringW ShortenURL(const CStringW url, int targetLength, bool returnHostnameIfTooLong) {
    CStringW t(url);
    if (t.Find(_T("://")) > 1) {
        t = t.Mid(t.Find(_T("://")) + 3);
    }
    if (t.Left(4) == _T("www.")) {
        t = t.Mid(4);
    }
    while (t.GetLength() > targetLength) {
        int position = t.ReverseFind('#');
        if (position > 0) {
            t = t.Left(position);
            continue;
        }
        position = t.ReverseFind('&');
        if (position > 0) {
            t = t.Left(position);
            continue;
        }
        position = t.ReverseFind('?');
        if (position > 0) {
            t = t.Left(position);
            break;
        }
        break;
    }
    t = UrlDecodeWithUTF8(t);
    if (t.GetLength() > targetLength && returnHostnameIfTooLong) return URLGetHostName(url);
    return t;
}

CString ExtractTag(CString tag, CMapStringToString& attribs, bool& fClosing)
{
    tag.Trim();
    attribs.RemoveAll();

    fClosing = !tag.IsEmpty() ? tag[0] == '/' : false;
    tag.TrimLeft('/');

    int i = tag.Find(' ');
    if (i < 0) {
        i = tag.GetLength();
    }
    CString type = tag.Left(i).MakeLower();
    tag = tag.Mid(i).Trim();

    while ((i = tag.Find('=')) > 0) {
        CString attrib = tag.Left(i).Trim().MakeLower();
        tag = tag.Mid(i + 1);
        for (i = 0; i < tag.GetLength() && _istspace(tag[i]); i++) {
            ;
        }
        if (i < tag.GetLength()) {
            tag = tag.Mid(i);
        } else {
            tag.Empty();
        }
        if (!tag.IsEmpty() && tag[0] == '\"') {
            tag = tag.Mid(1);
            i = tag.Find('\"');
        } else {
            i = tag.Find(' ');
        }
        if (i < 0) {
            i = tag.GetLength();
        }
        CString param = tag.Left(i).Trim();
        if (!param.IsEmpty()) {
            attribs[attrib] = param;
        }
        if (i + 1 < tag.GetLength()) {
            tag = tag.Mid(i + 1);
        } else {
            tag.Empty();
        }
    }

    return type;
}

CStringA HtmlSpecialChars(CStringA str, bool bQuotes /*= false*/)
{
    str.Replace("&", "&amp;");
    str.Replace("\"", "&quot;");
    if (bQuotes) {
        str.Replace("\'", "&#039;");
    }
    str.Replace("<", "&lt;");
    str.Replace(">", "&gt;");

    return str;
}

CStringA HtmlSpecialCharsDecode(CStringA str)
{
    str.Replace("&amp;", "&");
    str.Replace("&quot;", "\"");
    str.Replace("&#039;", "\'");
    str.Replace("&lt;", "<");
    str.Replace("&gt;", ">");
    str.Replace("&rsquo;", "'");

    return str;
}

CAtlList<CString>& MakeLower(CAtlList<CString>& sl)
{
    POSITION pos = sl.GetHeadPosition();
    while (pos) {
        sl.GetNext(pos).MakeLower();
    }
    return sl;
}

CAtlList<CString>& MakeUpper(CAtlList<CString>& sl)
{
    POSITION pos = sl.GetHeadPosition();
    while (pos) {
        sl.GetNext(pos).MakeUpper();
    }
    return sl;
}

CString FormatNumber(CString szNumber, bool bNoFractionalDigits /*= true*/)
{
    CString ret;

    int nChars = GetNumberFormat(LOCALE_USER_DEFAULT, 0, szNumber, nullptr, nullptr, 0);
    GetNumberFormat(LOCALE_USER_DEFAULT, 0, szNumber, nullptr, ret.GetBuffer(nChars), nChars);
    ret.ReleaseBuffer();

    if (bNoFractionalDigits) {
        TCHAR szNumberFractionalDigits[2] = {0};
        GetLocaleInfo(LOCALE_USER_DEFAULT, LOCALE_IDIGITS, szNumberFractionalDigits, _countof(szNumberFractionalDigits));
        int nNumberFractionalDigits = _tcstol(szNumberFractionalDigits, nullptr, 10);
        if (nNumberFractionalDigits) {
            ret.Truncate(ret.GetLength() - nNumberFractionalDigits - 1);
        }
    }

    return ret;
}

void GetLocaleString(LCID lcid, LCTYPE type, CString& output) {
    int len = GetLocaleInfo(lcid, type, output.GetBuffer(256), 256);
    output.ReleaseBufferSetLength(std::max(len - 1, 0));
}

int LastIndexOfCString(const CString& text, const CString& pattern) {
    int found = -1;
    int next_pos = 0;
    while (true) {
        next_pos = text.Find(pattern, next_pos);
        if (next_pos > found) {
            found = next_pos;
            next_pos = next_pos + pattern.GetLength();
        } else {
            return found;
        }        
    }
}

bool IsNameSimilar(const CString& title, const CString& fileName) {
    if (fileName.Find(title.Left(25)) > -1) return true;
    CT2CW t(title);
    std::wstring tit(t);
    const std::wregex reg(LR"([^[:print:]]|[/\\:\*\?"<>\|])", std::regex_constants::ECMAScript);
    if (!std::regex_search(tit, reg)) return false;
    const std::array<std::wstring, 3> arr = { L"_", L"-", L"." };
    for (auto& r : arr) {
        std::wstring temp = std::regex_replace(tit, reg, r);
        CStringW newtitle(temp.c_str());
        if (fileName.Find(newtitle.Left(25)) > -1) return true;
    }
    return false;
}
