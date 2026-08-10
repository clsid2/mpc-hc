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
#include "SubtitleHelpers.h"
#include "TextFile.h"
#include "../DSUtil/PathUtils.h"
#include "../DSUtil/ISOLang.h"

namespace
{
    constexpr std::array<LPCTSTR, 13> subTypesExt = {
        _T("srt"), _T("sub"), _T("smi"), _T("psb"),
        _T("ssa"), _T("ass"), _T("idx"), _T("usf"),
        _T("xss"), _T("txt"), _T("rt"), _T("sup"),
        _T("vtt")
    };

    LPCTSTR separators = _T(".\\-_");
    LPCTSTR extListVid = _T("(avi)|(mkv)|(mp4)|(mov)|(webm)|(wmv)|(flv)|(ts)|(m2ts)");
}

LPCTSTR Subtitle::GetSubtitleFileExt(SubType type)
{
    return (type >= 0 && size_t(type) < subTypesExt.size()) ? subTypesExt[type] : nullptr;
}

bool Subtitle::IsTextSubtitleFileName(CString fileName)
{
    auto fileExt = PathUtils::FileExt(fileName).TrimLeft('.');
    return std::any_of(subTypesExt.cbegin(), subTypesExt.cend(), [&](LPCTSTR ext) {
        return fileExt == ext;
    });
}

void Subtitle::GetSubFileNames(CString fn, const CAtlArray<CString>& paths, CAtlArray<SubFile>& ret)
{
    ret.RemoveAll();

    if (fn.Find(_T("://")) > 1) {
        return;
    }

    fn.Replace('/', '\\');

    ExtendMaxPathLengthIfNeeded(fn, MAX_PATH);

    int l = fn.ReverseFind('\\') + 1;

    CString orgpath = fn.Left(l);
    CString title = PathUtils::StripExtensionAndRarVolumeSuffix(fn.Mid(l));
    int titleLength = title.GetLength();

    WIN32_FIND_DATA wfd;

    CString extListSub, regExpSub, regExpVid;
    for (size_t i = 0; i < subTypesExt.size(); i++) {
        extListSub.AppendFormat(_T("(%s)"), subTypesExt[i]);
        if (i < subTypesExt.size() - 1) {
            extListSub.AppendChar(_T('|'));
        }
    }
    regExpSub.Format(_T("([%s]+.+)?\\.(%s)$"), separators, extListSub.GetString());
    regExpVid.Format(_T(".+\\.(%s)$"), extListVid);

    const std::wregex::flag_type reFlags = std::wregex::icase | std::wregex::optimize;
    std::wregex reSub(regExpSub, reFlags), reVid(regExpVid, reFlags);

    for (size_t k = 0; k < paths.GetCount(); k++) {
        CString path = paths[k];
        path.Replace('/', '\\');

        l = path.GetLength();
        if (l > 0 && path[l - 1] != '\\') {
            path += _T('\\');
        }

        if (path.Find(':') == -1 && path.Find(_T("\\\\")) != 0) {
            path = orgpath + path;
            path.Replace(_T("\\.\\"), _T("\\"));
            ExtendMaxPathLengthIfNeeded(path, MAX_PATH);
        }

        CAtlList<CString> subs, vids;

        HANDLE hFile = FindFirstFile(path + title + _T("*"), &wfd);
        if (hFile != INVALID_HANDLE_VALUE) {
            do {
                CString fn2 = path + wfd.cFileName;
                if (std::regex_match(&wfd.cFileName[titleLength], reSub)) {
                    subs.AddTail(fn2);
                } else if (std::regex_match(&wfd.cFileName[titleLength], reVid)) {
                    // Convert to lower-case and cut the extension for easier matching
                    vids.AddTail(fn2.Left(fn2.ReverseFind(_T('.'))).MakeLower());
                }
            } while (FindNextFile(hFile, &wfd));

            FindClose(hFile);
        }

        POSITION posSub = subs.GetHeadPosition();
        while (posSub) {
            CString& fn2 = subs.GetNext(posSub);

            bool bMatchAnotherVid = false;
            if (!vids.IsEmpty()) {
                // Check if there is an exact match for another video file
                CString fnlower = fn2;
                fnlower.MakeLower();
                POSITION posVid = vids.GetHeadPosition();
                while (posVid) {
                    if (fnlower.Find(vids.GetNext(posVid)) == 0) {
                        bMatchAnotherVid = true;
                        break;
                    }
                }
            }

            if (!bMatchAnotherVid) {
                ExtendMaxPathLengthIfNeeded(fn2, MAX_PATH);
                SubFile f;
                f.fn = fn2;
                ret.Add(f);
            }
        }
    }

    if (ret.IsEmpty()) {
        // Load all subs from folder .\Subs\FILENAME_WITHOUT_EXT
        CString path = orgpath + L"Subs\\" + title + L"\\";
        ExtendMaxPathLengthIfNeeded(path, MAX_PATH);

        HANDLE hFile = FindFirstFile(path + L"*", &wfd);
        if (hFile != INVALID_HANDLE_VALUE) {
            do {
                CString fn2 = path + wfd.cFileName;
                CString ext = CPath(fn2).GetExtension();
                if (ext == L".srt" || ext == L".ass" || ext == L".ssa" || ext == L".webvtt" || ext == L".vtt") {
                    ExtendMaxPathLengthIfNeeded(fn2, MAX_PATH);
                    SubFile f;
                    f.fn = fn2;
                    ret.Add(f);
                }
            } while (FindNextFile(hFile, &wfd));
            FindClose(hFile);
        }
    }

    // sort files, this way the user can define the order (movie.00.English.srt, movie.01.Hungarian.srt, etc)
    std::sort(ret.GetData(), ret.GetData() + ret.GetCount());
}

void Subtitle::GetLCIDAndLangName(CStringW subName, LCID& lcid, CString& langname, HearingImpairedType& hi, std::wregex re) {
    std::wcmatch mc;
    if (std::regex_search((LPCTSTR)subName, mc, re)) {
        ASSERT(mc.size() == 4);
        ASSERT(mc[2].matched);

        CStringW langFull(mc[1].str().c_str());
        CStringA langSimple(mc[2].str().c_str());

        LCID lcidFull = 0;
        if (langFull.Find(L"-") != -1) { //only use LocaleNameToLCID when we have the longer ISO codes
            lcidFull = LocaleNameToLCID(langFull, 0);
        }
        if (lcidFull) {
            lcid = lcidFull;
            langname = langFull;
        } else {
            size_t len = langSimple.GetLength();
            if (len == 2 || len == 3) {
                langname = ISOLang::ISO639XToLanguage(langSimple);
                if (!langname.IsEmpty()) {
                    if (len == 3) {
                        lcid = ISOLang::ISO6392ToLcid(langSimple);
                    } else  {
                        lcid = ISOLang::ISO6391ToLcid(langSimple);
                    }
                }
            } else if (len > 3) {
                if (ISOLang::IsISO639Language(langSimple, &lcid)) {
                    langname = langSimple;
                }
            }
        }
        if (!langname.IsEmpty() && langSimple.CompareNoCase("hi") == 0) {
            hi = HI_YES;
        }
    }
}

CString Subtitle::GuessSubtitleName(const CString& fn, CString videoName, LCID& lcid, CString& langname, HearingImpairedType& hi)
{
    CString name;

    // The filename of the subtitle file
    int iExtStart = fn.ReverseFind('.');
    if (iExtStart < 0) {
        iExtStart = fn.GetLength();
    }
    CString subName = fn.Left(iExtStart).Mid(fn.ReverseFind('\\') + 1);

    if (!videoName.IsEmpty()) {
        // The filename of the video file
        iExtStart = videoName.ReverseFind('.');
        if (iExtStart < 0) {
            iExtStart = videoName.GetLength();
        }
        CString videoExt = videoName.Mid(iExtStart + 1).MakeLower();
        videoName = videoName.Left(iExtStart).Mid(videoName.ReverseFind('\\') + 1);

        CString subNameNoCase = CString(subName).MakeLower();
        CString videoNameNoCase = CString(videoName).MakeLower();

        // Check if the subtitle filename starts with the video filename
        // so that we can try to find a language info right after it
        if (subNameNoCase.Find(videoNameNoCase) == 0) {
            int iVideoNameEnd = videoName.GetLength();
            // Get ride of the video extension if it's in the subtitle filename
            if (subNameNoCase.Find(videoExt, iVideoNameEnd) == iVideoNameEnd + 1) {
                iVideoNameEnd += 1 + videoExt.GetLength();
            }
            subName = subName.Mid(iVideoNameEnd);

            std::wregex re(_T("^[.\\-_ ]+(([^.\\-_ ]+)(?:[.\\-_ ]+([^.\\-_ ]+))?)"), std::wregex::icase);
            GetLCIDAndLangName(subName, lcid, langname, hi, re);
        }
    }

    // If we couldn't find any info yet, we try to find the language at the end of the filename
    if (langname.IsEmpty()) {
        std::wregex re(_T(".*?[.\\-_ ]+(([^.\\-_ ]+)(?:[.\\-_ ]+([^.\\-_ ]+))?)$"), std::wregex::icase);
        GetLCIDAndLangName(subName, lcid, langname, hi, re);
    }

    name = fn.Mid(fn.ReverseFind('\\') + 1);
    if (name.GetLength() > 100) { // Cut some part of the filename if it's too long
        name.Format(_T("%s...%s"), name.Left(50).TrimRight(_T(".-_ ")).GetString(), name.Right(50).TrimLeft(_T(".-_ ")).GetString());
    }

    return name;
}
