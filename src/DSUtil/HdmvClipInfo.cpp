/*
 * (C) 2008-2014, 2017 see Authors.txt
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
#include "HdmvClipInfo.h"
#include "DSUtil.h"
#include "ISOLang.h"
#include "tinyxml2/library/tinyxml2.h"
#include <regex>

using namespace tinyxml2;

CHdmvClipInfo::CHdmvClipInfo()
    : SequenceInfo_start_address(0)
    , ProgramInfo_start_address(0)
    , m_hFile(INVALID_HANDLE_VALUE)
    , m_bIsHdmv(false)
{
}

CHdmvClipInfo::~CHdmvClipInfo()
{
    CloseFile(S_OK);
}

HRESULT CHdmvClipInfo::CloseFile(HRESULT hr)
{
    if (m_hFile != INVALID_HANDLE_VALUE) {
        CloseHandle(m_hFile);
        m_hFile = INVALID_HANDLE_VALUE;
    }
    return hr;
}

DWORD CHdmvClipInfo::ReadDword()
{
    return ReadByte() << 24 | ReadByte() << 16 | ReadByte() << 8 | ReadByte();
}

short CHdmvClipInfo::ReadShort()
{
    return ReadByte() << 8 | ReadByte();
}

BYTE CHdmvClipInfo::ReadByte()
{
    BYTE  bVal;
    DWORD dwRead;
    VERIFY(ReadFile(m_hFile, &bVal, sizeof(bVal), &dwRead, nullptr));

    return bVal;
}

void CHdmvClipInfo::ReadBuffer(BYTE* pBuff, DWORD nLen)
{
    DWORD dwRead;
    VERIFY(ReadFile(m_hFile, pBuff, nLen, &dwRead, nullptr));
}

HRESULT CHdmvClipInfo::ReadProgramInfo()
{
    BYTE number_of_program_sequences;
    LARGE_INTEGER Pos;

    m_Streams.RemoveAll();
    Pos.QuadPart = ProgramInfo_start_address;
    SetFilePointerEx(m_hFile, Pos, nullptr, FILE_BEGIN);

    ReadDword();    //length
    ReadByte();     //reserved_for_word_align
    number_of_program_sequences = (BYTE)ReadByte();
    int iStream = 0;
    for (size_t i = 0; i < number_of_program_sequences; i++) {
        ReadDword();    //SPN_program_sequence_start
        ReadShort();    //program_map_PID
        BYTE number_of_streams_in_ps = (BYTE)ReadByte(); //number_of_streams_in_ps
        ReadByte();     //reserved_for_future_use

        for (size_t stream_index = 0; stream_index < number_of_streams_in_ps; stream_index++) {
            m_Streams.SetCount(iStream + 1);
            m_Streams[iStream].m_PID = ReadShort();  // stream_PID

            // == StreamCodingInfo
            Pos.QuadPart = 0;
            SetFilePointerEx(m_hFile, Pos, &Pos, FILE_CURRENT);
            Pos.QuadPart += ReadByte() + 1; // length
            m_Streams[iStream].m_Type = (PES_STREAM_TYPE)ReadByte();

            switch (m_Streams[iStream].m_Type) {
                case VIDEO_STREAM_MPEG1:
                case VIDEO_STREAM_MPEG2:
                case VIDEO_STREAM_H264:
                case VIDEO_STREAM_VC1:
                case VIDEO_STREAM_HEVC: {
                    UINT8 Temp = ReadByte();
                    BDVM_VideoFormat VideoFormat = (BDVM_VideoFormat)(Temp >> 4);
                    BDVM_FrameRate FrameRate = (BDVM_FrameRate)(Temp & 0xf);
                    Temp = ReadByte();
                    BDVM_AspectRatio AspectRatio = (BDVM_AspectRatio)(Temp >> 4);

                    m_Streams[iStream].m_VideoFormat = VideoFormat;
                    m_Streams[iStream].m_FrameRate = FrameRate;
                    m_Streams[iStream].m_AspectRatio = AspectRatio;
                }
                break;
                case AUDIO_STREAM_MPEG1:
                case AUDIO_STREAM_MPEG2:
                case AUDIO_STREAM_LPCM:
                case AUDIO_STREAM_AC3:
                case AUDIO_STREAM_DTS:
                case AUDIO_STREAM_AC3_TRUE_HD:
                case AUDIO_STREAM_AC3_PLUS:
                case AUDIO_STREAM_DTS_HD:
                case AUDIO_STREAM_DTS_HD_MASTER_AUDIO:
                case SECONDARY_AUDIO_AC3_PLUS:
                case SECONDARY_AUDIO_DTS_HD: {
                    UINT8 Temp = ReadByte();
                    BDVM_ChannelLayout ChannelLayout = (BDVM_ChannelLayout)(Temp >> 4);
                    BDVM_SampleRate SampleRate = (BDVM_SampleRate)(Temp & 0xF);

                    ReadBuffer((BYTE*)m_Streams[iStream].m_LanguageCode, 3);
                    m_Streams[iStream].m_LanguageCode[3] = '\0';
                    m_Streams[iStream].m_LCID = ISOLang::ISO6392ToLcid(m_Streams[iStream].m_LanguageCode);
                    m_Streams[iStream].m_ChannelLayout = ChannelLayout;
                    m_Streams[iStream].m_SampleRate = SampleRate;
                }
                break;
                case PRESENTATION_GRAPHICS_STREAM:
                case INTERACTIVE_GRAPHICS_STREAM: {
                    ReadBuffer((BYTE*)m_Streams[iStream].m_LanguageCode, 3);
                    m_Streams[iStream].m_LanguageCode[3] = '\0';
                    m_Streams[iStream].m_LCID = ISOLang::ISO6392ToLcid(m_Streams[iStream].m_LanguageCode);
                }
                break;
                case SUBTITLE_STREAM: {
                    ReadByte(); // Should this really be here?
                    ReadBuffer((BYTE*)m_Streams[iStream].m_LanguageCode, 3);
                    m_Streams[iStream].m_LanguageCode[3] = '\0';
                    m_Streams[iStream].m_LCID = ISOLang::ISO6392ToLcid(m_Streams[iStream].m_LanguageCode);
                }
                break;
                default:
                    break;
            }

            iStream++;
            SetFilePointerEx(m_hFile, Pos, nullptr, FILE_BEGIN);
        }
    }
    return S_OK;
}

HRESULT CHdmvClipInfo::ReadInfo(LPCTSTR strFile)
{
    m_bIsHdmv = false;
    m_hFile = CreateFile(strFile, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr,
                         OPEN_EXISTING, FILE_ATTRIBUTE_READONLY | FILE_FLAG_SEQUENTIAL_SCAN, nullptr);

    if (m_hFile != INVALID_HANDLE_VALUE) {
        BYTE Buff[100];
        ReadBuffer(Buff, 4);
        if (memcmp(Buff, "HDMV", 4)) {
            return CloseFile(VFW_E_INVALID_FILE_FORMAT);
        }

        ReadBuffer(Buff, 4);
        if ((memcmp(Buff, "0300", 4) != 0) && (memcmp(Buff, "0200", 4) != 0) && (memcmp(Buff, "0100", 4) != 0)) {
            return CloseFile(VFW_E_INVALID_FILE_FORMAT);
        }

        SequenceInfo_start_address = ReadDword();
        ProgramInfo_start_address  = ReadDword();

        ReadProgramInfo();

        m_bIsHdmv = true;

        return CloseFile(S_OK);
    }

    return AmHresultFromWin32(GetLastError());
}

CHdmvClipInfo::Stream* CHdmvClipInfo::FindStream(short wPID)
{
    size_t nStreams = m_Streams.GetCount();
    for (size_t i = 0; i < nStreams; i++) {
        if (m_Streams[i].m_PID == wPID) {
            return &m_Streams[i];
        }
    }

    return nullptr;
}

LPCTSTR CHdmvClipInfo::Stream::Format()
{
    switch (m_Type) {
        case VIDEO_STREAM_MPEG1:
            return _T("Mpeg1");
        case VIDEO_STREAM_MPEG2:
            return _T("Mpeg2");
        case VIDEO_STREAM_H264:
            return _T("H264");
        case VIDEO_STREAM_VC1:
            return _T("VC1");
        case VIDEO_STREAM_HEVC:
            return _T("HEVC");
        case AUDIO_STREAM_MPEG1:
            return _T("MPEG1");
        case AUDIO_STREAM_MPEG2:
            return _T("MPEG2");
        case AUDIO_STREAM_LPCM:
            return _T("LPCM");
        case AUDIO_STREAM_AC3:
            return _T("AC3");
        case AUDIO_STREAM_DTS:
            return _T("DTS");
        case AUDIO_STREAM_AC3_TRUE_HD:
            return _T("MLP");
        case AUDIO_STREAM_AC3_PLUS:
            return _T("DD+");
        case AUDIO_STREAM_DTS_HD:
            return _T("DTS-HD");
        case AUDIO_STREAM_DTS_HD_MASTER_AUDIO:
            return _T("DTS-HD XLL");
        case SECONDARY_AUDIO_AC3_PLUS:
            return _T("Sec DD+");
        case SECONDARY_AUDIO_DTS_HD:
            return _T("Sec DTS-HD");
        case PRESENTATION_GRAPHICS_STREAM:
            return _T("PG");
        case INTERACTIVE_GRAPHICS_STREAM:
            return _T("IG");
        case SUBTITLE_STREAM:
            return _T("Text");
        default:
            return _T("Unknown");
    }
}

HRESULT CHdmvClipInfo::ReadPlaylist(CString strPlaylistFile, REFERENCE_TIME& rtDuration, CAtlList<PlaylistItem>& Playlist)
{
    CPath Path(strPlaylistFile);
    rtDuration = 0;

    // Get BDMV folder
    Path.RemoveFileSpec();
    Path.RemoveFileSpec();

    m_hFile = CreateFile(strPlaylistFile, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr,
                         OPEN_EXISTING, FILE_ATTRIBUTE_READONLY | FILE_FLAG_SEQUENTIAL_SCAN, nullptr);

    if (m_hFile != INVALID_HANDLE_VALUE) {
        BYTE Buff[100];
        bool bDuplicate = false;
        ReadBuffer(Buff, 4);
        if (memcmp(Buff, "MPLS", 4)) {
            return CloseFile(VFW_E_INVALID_FILE_FORMAT);
        }

        ReadBuffer(Buff, 4);
        if ((memcmp(Buff, "0300", 4) != 0) && (memcmp(Buff, "0200", 4) != 0) && (memcmp(Buff, "0100", 4) != 0)) {
            return CloseFile(VFW_E_INVALID_FILE_FORMAT);
        }

        LARGE_INTEGER Pos;
        unsigned short nPlaylistItems;

        Pos.QuadPart = ReadDword(); // PlayList_start_address
        ReadDword();                // PlayListMark_start_address

        // PlayList()
        SetFilePointerEx(m_hFile, Pos, nullptr, FILE_BEGIN);
        ReadDword();                   // length
        ReadShort();                   // reserved_for_future_use
        nPlaylistItems = ReadShort();  // number_of_PlayItems
        ReadShort();                   // number_of_SubPaths

        Pos.QuadPart += 10;
        for (size_t i = 0; i < nPlaylistItems; i++) {
            DWORD dwTemp;
            PlaylistItem Item;
            SetFilePointerEx(m_hFile, Pos, nullptr, FILE_BEGIN);
            Pos.QuadPart += ReadShort() + 2;
            ReadBuffer(Buff, 5);
            Item.m_strFileName.Format(_T("%s\\STREAM\\%c%c%c%c%c.M2TS"), static_cast<LPCTSTR>(Path), Buff[0], Buff[1], Buff[2], Buff[3], Buff[4]);

            ReadBuffer(Buff, 4);
            if (memcmp(Buff, "M2TS", 4)) {
                return CloseFile(VFW_E_INVALID_FILE_FORMAT);
            }
            ReadBuffer(Buff, 3);

            dwTemp = ReadDword();
            Item.m_rtIn = 20000i64 * dwTemp / 90; // Carefull : 32->33 bits!

            dwTemp = ReadDword();
            Item.m_rtOut = 20000i64 * dwTemp / 90; // Carefull : 32->33 bits!

            rtDuration += (Item.m_rtOut - Item.m_rtIn);

            if (Playlist.Find(Item) != nullptr) {
                bDuplicate = true;
            }
            Playlist.AddTail(Item);

            //TRACE(_T("File : %s, Duration : %s, Total duration  : %s\n"), strTemp, ReftimeToString (rtOut - rtIn), ReftimeToString (rtDuration));
        }

        CloseFile(S_OK);
        return bDuplicate ? S_FALSE : S_OK;
    }

    return AmHresultFromWin32(GetLastError());
}

HRESULT CHdmvClipInfo::ReadChapters(CString strPlaylistFile, CAtlList<CHdmvClipInfo::PlaylistItem>& PlaylistItems, CAtlList<PlaylistChapter>& Chapters)
{
    CPath Path(strPlaylistFile);

    // Get BDMV folder
    Path.RemoveFileSpec();
    Path.RemoveFileSpec();

    m_hFile = CreateFile(strPlaylistFile, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr,
                         OPEN_EXISTING, FILE_ATTRIBUTE_READONLY | FILE_FLAG_SEQUENTIAL_SCAN, nullptr);

    if (m_hFile != INVALID_HANDLE_VALUE) {
        REFERENCE_TIME* rtOffset = DEBUG_NEW REFERENCE_TIME[PlaylistItems.GetCount()];
        REFERENCE_TIME rtSum = 0;
        int nIndex = 0;
        BYTE Buff[100];

        POSITION pos = PlaylistItems.GetHeadPosition();
        while (pos) {
            CHdmvClipInfo::PlaylistItem& PI = PlaylistItems.GetNext(pos);

            rtOffset[nIndex] = rtSum - PI.m_rtIn;
            rtSum = rtSum + PI.Duration();
            nIndex++;
        }

        ReadBuffer(Buff, 4);
        if (memcmp(Buff, "MPLS", 4)) {
            SAFE_DELETE_ARRAY(rtOffset);
            return CloseFile(VFW_E_INVALID_FILE_FORMAT);
        }

        ReadBuffer(Buff, 4);
        if ((memcmp(Buff, "0300", 4) != 0) && (memcmp(Buff, "0200", 4) != 0) && (memcmp(Buff, "0100", 4) != 0)) {
            SAFE_DELETE_ARRAY(rtOffset);
            return CloseFile(VFW_E_INVALID_FILE_FORMAT);
        }

        LARGE_INTEGER  Pos;
        unsigned short nMarkCount;

        ReadDword();                // PlayList_start_address
        Pos.QuadPart = ReadDword(); // PlayListMark_start_address

        // PlayListMark()
        SetFilePointerEx(m_hFile, Pos, nullptr, FILE_BEGIN);
        ReadDword();                // length
        nMarkCount = ReadShort();   // number_of_PlayList_marks
        for (size_t i = 0; i < nMarkCount; i++) {
            PlaylistChapter Chapter;

            ReadByte();                                           // reserved_for_future_use
            Chapter.m_nMarkType   = (PlaylistMarkType)ReadByte(); // mark_type
            Chapter.m_nPlayItemId = ReadShort();                  // ref_to_PlayItem_id
            Chapter.m_rtTimestamp = 20000i64 * ReadDword() / 90 + rtOffset[Chapter.m_nPlayItemId];    // mark_time_stamp
            Chapter.m_nEntryPID   = ReadShort();                  // entry_ES_PID
            Chapter.m_rtDuration  = 20000i64 * ReadDword() / 90;  // duration

            Chapters.AddTail(Chapter);

            //TRACE(_T("Chapter %d : %s\n"), i, ReftimeToString(Chapter.m_rtTimestamp));
        }

        CloseFile(S_OK);
        SAFE_DELETE_ARRAY(rtOffset);
        return S_OK;
    }

    return AmHresultFromWin32(GetLastError());
}

#define MIN_LIMIT 3

HRESULT CHdmvClipInfo::FindMainMovie(LPCTSTR strFolder, CString& strPlaylistFile, CAtlList<PlaylistItem>& MainPlaylist, CAtlList<PlaylistItem>& MPLSPlaylists)
{
    HRESULT hr = E_FAIL;

    CString strPath(strFolder);
    CString strFilter;

    MPLSPlaylists.RemoveAll();

    CAtlList<PlaylistItem> Playlist;
    WIN32_FIND_DATA fd;
    ZeroMemory(&fd, sizeof(WIN32_FIND_DATA));

    strPath.Replace(_T("\\PLAYLIST\\"), _T("\\"));
    strPath.Replace(_T("\\STREAM\\"), _T("\\"));
    strPath += _T("\\BDMV\\");
    strFilter.Format(_T("%sPLAYLIST\\*.mpls"), strPath.GetString());

    HANDLE hFind = FindFirstFile(strFilter, &fd);
    if (hFind != INVALID_HANDLE_VALUE) {
        REFERENCE_TIME rtMax = 0;
        REFERENCE_TIME rtCurrent;
        CString strCurrentPlaylist;
        do {
            strCurrentPlaylist.Format(_T("%sPLAYLIST\\%s"), strPath.GetString(), fd.cFileName);
            Playlist.RemoveAll();

            // Main movie shouldn't have duplicate M2TS filename...
            if (ReadPlaylist(strCurrentPlaylist, rtCurrent, Playlist) == S_OK) {
                if (rtCurrent > rtMax) {
                    rtMax = rtCurrent;
                    strPlaylistFile = strCurrentPlaylist;
                    MainPlaylist.RemoveAll();
                    POSITION pos = Playlist.GetHeadPosition();
                    while (pos) {
                        MainPlaylist.AddTail(Playlist.GetNext(pos));
                    }
                    hr = S_OK;
                }
                if (rtCurrent >= (REFERENCE_TIME)MIN_LIMIT * 600000000) {
                    PlaylistItem Item;
                    Item.m_strFileName = strCurrentPlaylist;
                    Item.m_rtIn = 0;
                    Item.m_rtOut = rtCurrent;
                    MPLSPlaylists.AddTail(Item);
                }

            }
        } while (FindNextFile(hFind, &fd));

        FindClose(hFind);
    }

    if (MPLSPlaylists.GetCount() > 1) {
        // bubble sort
        for (size_t j = 0; j < MPLSPlaylists.GetCount(); j++) {
            for (size_t i = 0; i < MPLSPlaylists.GetCount() - 1; i++) {
                if (MPLSPlaylists.GetAt(MPLSPlaylists.FindIndex(i)).Duration() < MPLSPlaylists.GetAt(MPLSPlaylists.FindIndex(i + 1)).Duration()) {
                    MPLSPlaylists.SwapElements(MPLSPlaylists.FindIndex(i), MPLSPlaylists.FindIndex(i + 1));
                }
            }
        }
    }

    return hr;
}

bool CHdmvClipInfo::ReadMeta(LPCTSTR strFolder, CAtlList<BDMVMeta>& meta)
{
    bool re = false;
    CString strPath(strFolder);
    CString strFilter;

    meta.RemoveAll();

    strPath.Replace(_T("\\PLAYLIST\\"), _T("\\"));
    strPath.Replace(_T("\\STREAM\\"), _T("\\"));
    strPath += _T("\\BDMV\\");
    strPath.Replace(_T("\\\\"), _T("\\"));
    strPath.Replace(_T("\\\\"), _T("\\"));
    strFilter.Format(_T("%sMETA\\DL\\bdmt_*.xml"), strPath.GetString());

    WIN32_FIND_DATA fd;
    ZeroMemory(&fd, sizeof(WIN32_FIND_DATA));

    HANDLE hFind = FindFirstFile(strFilter, &fd);
    if (hFind != INVALID_HANDLE_VALUE) {
        do {
            std::wstring TempFileName(fd.cFileName);
            std::match_results<std::wstring::const_iterator> mr;
            CHdmvClipInfo::BDMVMeta Item;
            tinyxml2::XMLDocument doc;
            CStringW FileName(fd.cFileName);
            CStringW FilePath;

            bool matched = std::regex_search(TempFileName, mr, std::wregex(L"^bdmt_(\\w+).xml$"));

            if (matched) {
                Item.langcode = mr[1].str().c_str();
            }
            else {
                Item.langcode = L"";
            }

            FilePath.Format(L"%sMETA\\DL\\", strPath.GetString());
            FilePath += FileName;
            FILE* f;
            if (!_wfopen_s(&f, FilePath.GetString(), L"rb")) {
                doc.LoadFile(f);
                fclose(f);

                XMLElement* rootNote(doc.RootElement());
                if (!strcmp(rootNote->Name(), "disclib")) {
                    XMLElement* discinfo = rootNote->FirstChildElement("di:discinfo");
                    if (discinfo != NULL) {
                        XMLElement* title = discinfo->FirstChildElement("di:title");
                        if (title != NULL) {
                            XMLElement* name = title->FirstChildElement("di:name");
                            if (name != NULL) {
                                Item.title = UTF8ToStringW(name->GetText());
                            }
                        }
                    }
                }
            }

            if (!Item.title.IsEmpty()) {
                meta.AddTail(Item);
                re = true;
            }
        } while (FindNextFile(hFind, &fd));

        FindClose(hFind);
    }

    return re;
}
