/*
 * (C) 2008-2013, 2016 see Authors.txt
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

#include "Mpeg2Def.h"
#include <atlcoll.h>

enum BDVM_VideoFormat {
    BDVM_VideoFormat_Unknown = 0,
    BDVM_VideoFormat_480i    = 1,
    BDVM_VideoFormat_576i    = 2,
    BDVM_VideoFormat_480p    = 3,
    BDVM_VideoFormat_1080i   = 4,
    BDVM_VideoFormat_720p    = 5,
    BDVM_VideoFormat_1080p   = 6,
    BDVM_VideoFormat_576p    = 7
};

enum BDVM_FrameRate {
    BDVM_FrameRate_Unknown = 0,
    BDVM_FrameRate_23_976  = 1,
    BDVM_FrameRate_24      = 2,
    BDVM_FrameRate_25      = 3,
    BDVM_FrameRate_29_97   = 4,
    BDVM_FrameRate_50      = 6,
    BDVM_FrameRate_59_94   = 7
};

enum BDVM_AspectRatio {
    BDVM_AspectRatio_Unknown = 0,
    BDVM_AspectRatio_4_3     = 2,
    BDVM_AspectRatio_16_9    = 3,
    BDVM_AspectRatio_2_21    = 4
};

enum BDVM_ChannelLayout {
    BDVM_ChannelLayout_Unknown = 0,
    BDVM_ChannelLayout_MONO    = 1,
    BDVM_ChannelLayout_STEREO  = 3,
    BDVM_ChannelLayout_MULTI   = 6,
    BDVM_ChannelLayout_COMBO   = 12
};

enum BDVM_SampleRate {
    BDVM_SampleRate_Unknown = 0,
    BDVM_SampleRate_48      = 1,
    BDVM_SampleRate_96      = 4,
    BDVM_SampleRate_192     = 5,
    BDVM_SampleRate_48_192  = 12,
    BDVM_SampleRate_48_96   = 14
};

class CHdmvClipInfo
{
public:

    struct Stream {
        Stream() {
            ZeroMemory(this, sizeof(*this));
        }
        short m_PID;
        PES_STREAM_TYPE m_Type;
        char m_LanguageCode[4];
        LCID m_LCID;

        // Valid for video types
        BDVM_VideoFormat m_VideoFormat;
        BDVM_FrameRate m_FrameRate;
        BDVM_AspectRatio m_AspectRatio;
        // Valid for audio types
        BDVM_ChannelLayout m_ChannelLayout;
        BDVM_SampleRate m_SampleRate;

        LPCTSTR Format();
    };

    struct PlaylistItem {
        CString m_strFileName;
        REFERENCE_TIME m_rtIn;
        REFERENCE_TIME m_rtOut;

        REFERENCE_TIME Duration() const { return m_rtOut - m_rtIn; }

        bool operator == (const PlaylistItem& pi) const {
            return pi.m_strFileName == m_strFileName;
        }
    };

    enum PlaylistMarkType {
        Reserved  = 0x00,
        EntryMark = 0x01,
        LinkPoint = 0x02
    };

    struct PlaylistChapter {
        short            m_nPlayItemId;
        PlaylistMarkType m_nMarkType;
        REFERENCE_TIME   m_rtTimestamp;
        short            m_nEntryPID;
        REFERENCE_TIME   m_rtDuration;
    };

    struct BDMVMeta {
        CString         langcode;
        CString         title;
    };

    CHdmvClipInfo();
    ~CHdmvClipInfo();

    HRESULT ReadInfo(LPCTSTR strFile);
    Stream* FindStream(short wPID);
    bool IsHdmv() const { return m_bIsHdmv; };
    size_t GetStreamNumber() { return m_Streams.GetCount(); };
    Stream* GetStreamByIndex(size_t nIndex) { return (nIndex < m_Streams.GetCount()) ? &m_Streams[nIndex] : nullptr; };

    HRESULT FindMainMovie(LPCTSTR strFolder, CString& strPlaylistFile, CAtlList<PlaylistItem>& MainPlaylist, CAtlList<PlaylistItem>& MPLSPlaylists);
    HRESULT ReadPlaylist(CString strPlaylistFile, REFERENCE_TIME& rtDuration, CAtlList<PlaylistItem>& Playlist);
    HRESULT ReadChapters(CString strPlaylistFile, CAtlList<CHdmvClipInfo::PlaylistItem>& PlaylistItems, CAtlList<PlaylistChapter>& Chapters);
    bool ReadMeta(LPCTSTR strFolder, CAtlList<BDMVMeta>& meta);

private:
    DWORD SequenceInfo_start_address;
    DWORD ProgramInfo_start_address;

    HANDLE m_hFile;

    CAtlArray<Stream> m_Streams;
    bool m_bIsHdmv;

    DWORD ReadDword();
    short ReadShort();
    BYTE ReadByte();
    void ReadBuffer(BYTE* pBuff, DWORD nLen);

    HRESULT ReadProgramInfo();
    HRESULT CloseFile(HRESULT hr);
};
