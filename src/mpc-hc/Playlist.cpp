/*
 * (C) 2003-2006 Gabest
 * (C) 2006-2015, 2017 see Authors.txt
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
#include "mplayerc.h"
#include "Playlist.h"
#include "PathUtils.h"
#include "SettingsDefines.h"

//
// CPlaylistItem
//

UINT CPlaylistItem::m_globalid = 0;

CPlaylistItem::CPlaylistItem()
    : m_posNextShuffle(nullptr)
    , m_posPrevShuffle(nullptr)
    , m_type(file)
    , m_duration(0)
    , m_vinput(-1)
    , m_vchannel(-1)
    , m_ainput(-1)
    , m_country(0)
    , m_fns()
    , m_subs()
    , m_fInvalid(false)
    , m_label(_T(""))
    , m_cue(false)
    , m_cue_filename(_T(""))
    , m_cue_index(0)
    , m_cover(_T(""))
    , m_bYoutubeDL(false)
    , m_ydlSourceURL(_T(""))
    , m_useragent(_T(""))
    , m_ydl_subs()
    , inlineEditMaxWidth(0)
{
    m_id = m_globalid++;
}

CPlaylistItem::~CPlaylistItem()
{
}

CPlaylistItem::CPlaylistItem(const CPlaylistItem& pli)
{
    *this = pli;
}

CPlaylistItem& CPlaylistItem::operator=(const CPlaylistItem& pli)
{
    if (this != &pli) {
        m_id = pli.m_id;
        m_label = pli.m_label;
        m_fns.RemoveAll();
        m_fns.AddTailList(&pli.m_fns);
        m_subs.RemoveAll();
        m_subs.AddTailList(&pli.m_subs);
        m_type = pli.m_type;
        m_fInvalid = pli.m_fInvalid;
        m_duration = pli.m_duration;
        m_vinput = pli.m_vinput;
        m_vchannel = pli.m_vchannel;
        m_ainput = pli.m_ainput;
        m_country = pli.m_country;
        m_posNextShuffle = pli.m_posNextShuffle;
        m_posPrevShuffle = pli.m_posPrevShuffle;
        m_cue = pli.m_cue;
        m_cue_filename = pli.m_cue_filename;
        m_cue_index = pli.m_cue_index;
        m_cover = pli.m_cover;
        m_bYoutubeDL = pli.m_bYoutubeDL;
        m_ydlSourceURL = pli.m_ydlSourceURL;
        m_useragent = pli.m_useragent;
        m_ydl_subs.RemoveAll();
        m_ydl_subs.AddHeadList(&pli.m_ydl_subs);
    }
    return *this;
}

POSITION CPlaylistItem::FindFile(LPCTSTR path)
{
    POSITION pos = m_fns.GetHeadPosition();
    while (pos) {
        if (m_fns.GetAt(pos).CompareNoCase(path) == 0) {
            return pos;
        }
        m_fns.GetNext(pos);
    }
    return nullptr;
}

CString CPlaylistItem::GetLabel(int i)
{
    CString str;

    if (i == 0) {
        ASSERT(m_label);
        if (m_label && !m_label.IsEmpty()) {
            str = m_label;
        } else if (!m_fns.IsEmpty()) {
            str = PathUtils::StripPathOrUrl(m_fns.GetHead());
        }
    } else if (i == 1) {
        if (m_fInvalid) {
            return _T("Invalid");
        }

        if (m_type == file) {
            REFERENCE_TIME rt = m_duration;

            if (rt > 0) {
                rt = (rt + 5000000) / 10000000;
                int ss = int(rt % 60);
                rt /= 60;
                int mm = int(rt % 60);
                rt /= 60;
                int hh = int(rt);

                str.Format(_T("%02d:%02d:%02d"), hh, mm, ss);
            }
        } else if (m_type == device) {
            // TODO
        }

    }

    return str;
}

void CPlaylistItem::AutoLoadFiles()
{
    if (m_fns.IsEmpty()) {
        return;
    }

    const CAppSettings& s = AfxGetAppSettings();

    CString fn = m_fns.GetHead();

    if (s.fAutoloadAudio && !PathUtils::IsURL(fn) && (s.fEnableAudioSwitcher || s.SelectedAudioRenderer() != AUDRNDT_INTERNAL && s.SelectedAudioRenderer() != AUDRNDT_MPC)) {
        int i = fn.ReverseFind('.');
        if (i > 0) {
            const CMediaFormats& mf = s.m_Formats;

            CString ext = fn.Mid(i + 1).MakeLower();

            if (!mf.FindExt(ext, true)) {
                CString path = fn;
                path.Replace('/', '\\');
                path = path.Left(path.ReverseFind('\\') + 1);

                WIN32_FIND_DATA fd;
                ZeroMemory(&fd, sizeof(WIN32_FIND_DATA));
                HANDLE hFind = FindFirstFile(fn.Left(i) + _T("*.*"), &fd);
                if (hFind != INVALID_HANDLE_VALUE) {
                    do {
                        if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
                            continue;
                        }

                        CString fullpath = path + fd.cFileName;
                        CString ext2 = fullpath.Mid(fullpath.ReverseFind('.') + 1).MakeLower();
                        if (!FindStringInList(m_fns, fullpath) && ext != ext2
                                && mf.FindExt(ext2, true) && mf.IsUsingEngine(fullpath, DirectShow)) {
                            m_fns.AddTail(fullpath);
                        }
                    } while (FindNextFile(hFind, &fd));

                    FindClose(hFind);
                }
            }
        }
    }

    if (s.IsISRAutoLoadEnabled()) {
        const CString& pathList = s.strSubtitlePaths;

        CAtlArray<CString> paths;

        int pos = 0;
        do {
            CString path = pathList.Tokenize(_T(";"), pos);
            if (!path.IsEmpty()) {
                paths.Add(path);
            }
        } while (pos != -1);

        CString dir = fn;
        dir.Replace('\\', '/');
        int l  = dir.ReverseFind('/') + 1;
        int l2 = dir.ReverseFind('.');
        if (l2 < l) { // no extension, read to the end
            l2 = fn.GetLength();
        }
        CString title = dir.Mid(l, l2 - l);
        paths.Add(title);

        CAtlArray<Subtitle::SubFile> ret;
        Subtitle::GetSubFileNames(fn, paths, ret);

        for (size_t i = 0; i < ret.GetCount(); i++) {
            if (!FindStringInList(m_subs, ret[i].fn)) {
                m_subs.AddTail(ret[i].fn);
            }
        }
    }
}

//
// CPlaylist
//

CPlaylist::CPlaylist(bool bShuffle /*= false*/)
    : m_pos(nullptr)
    , m_bShuffle(bShuffle)
    , m_posHeadShuffle(nullptr)
    , m_posTailShuffle(nullptr)
    , m_nShuffledListSize(0)
{
}

CPlaylist::~CPlaylist()
{
}

bool CPlaylist::RemoveAll()
{
    __super::RemoveAll();
    bool bWasPlaying = (m_pos != nullptr);
    m_pos = nullptr;
    m_posHeadShuffle = m_posTailShuffle = nullptr;
    m_nShuffledListSize = 0;
    return bWasPlaying;
}

bool CPlaylist::RemoveAt(POSITION pos)
{
    if (pos) {
        // Update the shuffled list only if there is no pending reshuffle
        if (m_bShuffle && m_nShuffledListSize == GetCount()) {
            const CPlaylistItem& pli = GetAt(pos);
            if (pos == m_posHeadShuffle) {
                m_posHeadShuffle = pli.m_posNextShuffle;
            } else {
                GetAt(pli.m_posPrevShuffle).m_posNextShuffle = pli.m_posNextShuffle;
            }
            if (pos == m_posTailShuffle) {
                m_posTailShuffle = pli.m_posPrevShuffle;
            } else {
                GetAt(pli.m_posNextShuffle).m_posPrevShuffle = pli.m_posPrevShuffle;
            }
            m_nShuffledListSize--;
        }
        // Actually remove the item
        __super::RemoveAt(pos);
        // Check if it was the currently playing item
        if (m_pos == pos) {
            m_pos = nullptr;
            return true;
        }
    }

    return false;
}

struct plsort_t {
    UINT n;
    POSITION pos;

    bool operator <(const plsort_t& rhs) const {
        return n < rhs.n;
    }
};

struct plsort2_t {
    LPCTSTR  str;
    POSITION pos;

    bool operator <(const plsort2_t& rhs) const {
        return StrCmpLogicalW(str, rhs.str) < 0;
    }
};

void CPlaylist::SortById()
{
    CAtlArray<plsort_t> a;
    a.SetCount(GetCount());
    POSITION pos = GetHeadPosition();
    for (int i = 0; pos; i++, GetNext(pos)) {
        a[i].n = GetAt(pos).m_id, a[i].pos = pos;
    }
    std::sort(a.GetData(), a.GetData() + a.GetCount());
    for (size_t i = 0; i < a.GetCount(); i++) {
        MoveToTail(a[i].pos);
    }
}

void CPlaylist::SortByName()
{
    CAtlArray<plsort2_t> a;
    a.SetCount(GetCount());
    POSITION pos = GetHeadPosition();
    for (int i = 0; pos; i++, GetNext(pos)) {
        CString& fn = GetAt(pos).m_fns.GetHead();
        a[i].str = (LPCTSTR)fn + std::max(fn.ReverseFind('/'), fn.ReverseFind('\\')) + 1;
        a[i].pos = pos;
    }
    std::sort(a.GetData(), a.GetData() + a.GetCount());
    for (size_t i = 0; i < a.GetCount(); i++) {
        MoveToTail(a[i].pos);
    }
}

void CPlaylist::SortByPath()
{
    CAtlArray<plsort2_t> a;
    a.SetCount(GetCount());
    POSITION pos = GetHeadPosition();
    for (int i = 0; pos; i++, GetNext(pos)) {
        a[i].str = GetAt(pos).m_fns.GetHead(), a[i].pos = pos;
    }
    std::sort(a.GetData(), a.GetData() + a.GetCount());
    for (size_t i = 0; i < a.GetCount(); i++) {
        MoveToTail(a[i].pos);
    }
}

void CPlaylist::Randomize()
{
    CAtlArray<plsort_t> a;
    a.SetCount(GetCount());
    srand((unsigned int)time(nullptr));
    POSITION pos = GetHeadPosition();
    for (int i = 0; pos; i++, GetNext(pos)) {
        a[i].n = rand(), a[i].pos = pos;
    }
    std::sort(a.GetData(), a.GetData() + a.GetCount());
    for (size_t i = 0; i < a.GetCount(); i++) {
        MoveToTail(a[i].pos);
    }
}

POSITION CPlaylist::GetPos() const
{
    return m_pos;
}

void CPlaylist::SetPos(POSITION pos)
{
    m_pos = pos;
}

POSITION CPlaylist::GetShuffleAwareHeadPosition()
{
    POSITION posHead;
    if (m_bShuffle) {
        ReshuffleIfNeeded();
        posHead = m_posHeadShuffle;
    } else {
        posHead = GetHeadPosition();
    }
    return posHead;
}

POSITION CPlaylist::GetShuffleAwareTailPosition()
{
    POSITION posTail;
    if (m_bShuffle) {
        ReshuffleIfNeeded();
        posTail = m_posTailShuffle;
    } else {
        posTail = GetTailPosition();
    }
    return posTail;
}

CPlaylistItem& CPlaylist::GetNextWrap(POSITION& pos)
{
    if (m_bShuffle) {
        ReshuffleIfNeeded();
        pos = GetAt(pos).m_posNextShuffle;
    } else {
        GetNext(pos);
    }
    if (!pos) {
        pos = GetShuffleAwareHeadPosition();
    }
    return GetAt(pos);
}

CPlaylistItem& CPlaylist::GetPrevWrap(POSITION& pos)
{
    if (m_bShuffle) {
        ReshuffleIfNeeded();
        pos = GetAt(pos).m_posPrevShuffle;
    } else {
        GetPrev(pos);
    }
    if (!pos) {
        pos = GetShuffleAwareTailPosition();
    }
    return GetAt(pos);
}

// Calling this function with bEnable equals to true when
// shuffle is already enabled will re-shuffle the tracks.
void CPlaylist::SetShuffle(bool bEnable)
{
    m_bShuffle = bEnable;

    if (bEnable && !IsEmpty()) {
        m_nShuffledListSize = GetCount();
        CAtlArray<plsort_t> positions;
        positions.SetCount(m_nShuffledListSize + 1);
        srand((unsigned int)time(nullptr));
        POSITION pos = GetHeadPosition();
        for (size_t i = 0; pos; i++, GetNext(pos)) {
            positions[i].n = rand();
            positions[i].pos = pos;
        }
        std::sort(positions.GetData(), positions.GetData() + m_nShuffledListSize);
        positions[m_nShuffledListSize].pos = nullptr; // Termination

        m_posHeadShuffle = positions[0].pos;
        m_posTailShuffle = nullptr;
        for (size_t i = 0; i < m_nShuffledListSize; i++) {
            pos = positions[i].pos;
            CPlaylistItem& pli = GetAt(pos);
            pli.m_posPrevShuffle = m_posTailShuffle;
            pli.m_posNextShuffle = positions[i + 1].pos;
            m_posTailShuffle = pos;
        }
    } else {
        m_posHeadShuffle = m_posTailShuffle = nullptr;
        m_nShuffledListSize = 0;
    }
}

CPlaylistIDs CPlaylist::GetIDs() {
    CPlaylistIDs ids;
    POSITION pos = GetHeadPosition();
    for (size_t i = 0; pos; i++, GetNext(pos)) {
        CPlaylistItem& pli = GetAt(pos);
        ids.push_back(pli.m_id);
    }
    return ids;
}

// This will reshuffle if the shuffled list size
// does not match the playlist size.
bool CPlaylist::ReshuffleIfNeeded()
{
    if (m_bShuffle && m_nShuffledListSize != GetCount()) {
        SetShuffle(true);
        return true;
    }
    return false;
}
