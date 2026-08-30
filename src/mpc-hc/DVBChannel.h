/*
 * (C) 2009-2016 see Authors.txt
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

#include <vector>
#include <array>

#define FORMAT_VERSION_0       0
#define FORMAT_VERSION_1       1
#define FORMAT_VERSION_2       2
#define FORMAT_VERSION_3       3
#define FORMAT_VERSION_4       4
#define FORMAT_VERSION_5       5
#define FORMAT_VERSION_6       6
#define FORMAT_VERSION_CURRENT 7

#define BDA_MAX_AUDIO    10
#define BDA_MAX_SUBTITLE 10

struct EventDescriptor {
    CString eventName;
    CString eventDesc;
    time_t startTime = 0;
    time_t duration  = 0;
    CString strStartTime;
    CString strEndTime;
    std::vector<std::pair<CString, CString>> extendedDescriptorsItems;
    CString extendedDescriptorsText;
    int parentalRating = -1;
    CString content;
};

enum BDA_STREAM_TYPE {
    BDA_MPV      = 0x00,
    BDA_H264     = 0x01,
    BDA_MPA      = 0x02,
    BDA_AC3      = 0x03,
    BDA_EAC3     = 0x04,
    BDA_HEVC     = 0x05,
    BDA_ADTS     = 0x10,
    BDA_LATM     = 0x11,
    BDA_PSI      = 0x80,
    BDA_TIF      = 0x81,
    BDA_EPG      = 0x82,
    BDA_SUB      = 0x83,
    BDA_SUBTITLE = 0xFE,
    BDA_UNKNOWN  = 0xFF
};

enum BDA_CHROMA_TYPE {
    BDA_Chroma_NONE  = 0x00,
    BDA_Chroma_4_2_0 = 0x01,
    BDA_Chroma_4_2_2 = 0x02,
    BDA_Chroma_4_4_4 = 0x03
};

enum BDA_FPS_TYPE {
    BDA_FPS_NONE   = 0x00,
    BDA_FPS_23_976 = 0x01,
    BDA_FPS_24_0   = 0x02,
    BDA_FPS_25_0   = 0x03,
    BDA_FPS_29_97  = 0x04,
    BDA_FPS_30_0   = 0x05,
    BDA_FPS_50_0   = 0x06,
    BDA_FPS_59_94  = 0x07,
    BDA_FPS_60_0   = 0x08
};

enum BDA_AspectRatio_TYPE {
    BDA_AR_NULL   = 0x00,
    BDA_AR_1      = 0x01,
    BDA_AR_3_4    = 0x02,
    BDA_AR_9_16   = 0x03,
    BDA_AR_1_2_21 = 0x04
};

struct BDAStreamInfo {
    ULONG           ulPID    = 0;
    BDA_STREAM_TYPE nType    = BDA_UNKNOWN;
    PES_STREAM_TYPE nPesType = INVALID;
    CString         sLanguage;

    LCID GetLCID() const;
};

class CBDAChannel
{
public:
    CBDAChannel() = default;
    CBDAChannel(CString strChannel);
    ~CBDAChannel() = default;

    CString ToString() const;
    /**
     * @brief Output a JSON representation of a BDA channel.
     * @note The object contains two elements : "index", which corresponds to
     * @c m_nPrefNumber, and "name", which contains @c m_strName.
     * @returns A string representing a JSON object containing the
     * aforementioned elements.
     */
    CStringA ToJSON() const;

    LPCTSTR GetName() const { return m_strName; };
    ULONG GetFrequency() const { return m_ulFrequency; };
    ULONG GetBandwidth() const { return m_ulBandwidth; }
    ULONG GetSymbolRate() const { return m_ulSymbolRate; }
    int GetPrefNumber() const { return m_nPrefNumber; };
    int GetOriginNumber() const { return m_nOriginNumber; };
    // ATSC identifies a service by a two-part virtual channel number carried in
    // the VCT, which is independent of the RF channel it arrives on. Both parts
    // are zero for DVB, where the logical channel number in m_nOriginNumber is
    // the equivalent concept.
    int GetATSCMajor() const { return m_nATSCMajor; };
    int GetATSCMinor() const { return m_nATSCMinor; };
    bool HasATSCNumber() const { return m_nATSCMajor != 0; };
    ULONG GetONID() const { return m_ulONID; };
    ULONG GetTSID() const { return m_ulTSID; };
    ULONG GetSID() const { return m_ulSID; };
    ULONG GetPMT() const { return m_ulPMT; };
    ULONG GetPCR() const { return m_ulPCR; };
    ULONG GetVideoPID() const { return m_ulVideoPID; };
    BDA_FPS_TYPE GetVideoFps() const { return m_nVideoFps; }
    CString GetVideoFpsDesc();
    BDA_CHROMA_TYPE GetVideoChroma() const { return m_nVideoChroma; }
    ULONG GetVideoWidth() const {return m_nVideoWidth; }
    ULONG GetVideoHeight() const {return m_nVideoHeight; }
    BDA_AspectRatio_TYPE GetVideoAR() {return m_nVideoAR; }
    DWORD GetVideoARx();
    DWORD GetVideoARy();
    BDA_STREAM_TYPE GetVideoType() const { return m_nVideoType; }
    ULONG GetDefaultAudioPID() const { return m_Audios[GetDefaultAudio()].ulPID; };
    BDA_STREAM_TYPE GetDefaultAudioType() const { return m_Audios[GetDefaultAudio()].nType; }
    ULONG GetDefaultSubtitlePID() const { return m_Subtitles[GetDefaultSubtitle()].ulPID; };
    int GetAudioCount() const { return m_nAudioCount; };
    int GetDefaultAudio() const { return m_nDefaultAudio; };
    int GetSubtitleCount() const { return m_nSubtitleCount; };
    int GetDefaultSubtitle() const { return m_nDefaultSubtitle; };
    BDAStreamInfo* GetAudio(int nIndex) { return &m_Audios[nIndex]; };
    const BDAStreamInfo* GetAudio(int nIndex) const { return &m_Audios[nIndex]; };
    BDAStreamInfo* GetSubtitle(int nIndex) { return &m_Subtitles[nIndex]; };
    const BDAStreamInfo* GetSubtitle(int nIndex) const { return &m_Subtitles[nIndex]; };
    bool HasName() const { return !m_strName.IsEmpty(); };
    bool IsEncrypted() const { return m_bEncrypted; };
    bool GetNowNextFlag() const { return m_bNowNextFlag; };
    REFERENCE_TIME GetAvgTimePerFrame();

    void SetName(LPCTSTR Value) { m_strName = Value; };
    void SetFrequency(ULONG Value) { m_ulFrequency = Value; };
    void SetBandwidth(ULONG ulBandwidth) { m_ulBandwidth = ulBandwidth; }
    void SetSymbolRate(ULONG ulSymbolRate) { m_ulSymbolRate = ulSymbolRate; }
    void SetPrefNumber(int Value) { m_nPrefNumber = Value; };
    void SetOriginNumber(int Value) { m_nOriginNumber = Value; };
    // Also derives m_nOriginNumber, so that everything already keying off the
    // logical channel number - sorting in the scan dialog, the channel list -
    // orders ATSC services correctly without needing to know about ATSC.
    // 9.1 < 9.2 < 9.91 < 10.1 < 50.1 holds under this encoding.
    void SetATSCNumber(int nMajor, int nMinor) {
        m_nATSCMajor = nMajor;
        m_nATSCMinor = nMinor;
        m_nOriginNumber = nMajor * 1000 + nMinor;
    };
    void SetEncrypted(bool Value) { m_bEncrypted = Value; };
    void SetNowNextFlag(bool Value) { m_bNowNextFlag = Value; };
    void SetONID(ULONG Value) { m_ulONID = Value; };
    void SetTSID(ULONG Value) { m_ulTSID = Value; };
    void SetSID(ULONG Value) { m_ulSID = Value; };
    void SetPMT(ULONG Value) { m_ulPMT = Value; };
    void SetPCR(ULONG Value) { m_ulPCR = Value; };
    void SetVideoPID(ULONG Value) { m_ulVideoPID = Value; };
    void SetVideoFps(BDA_FPS_TYPE Value) { m_nVideoFps = Value; };
    void SetVideoChroma(BDA_CHROMA_TYPE Value) { m_nVideoChroma = Value; };
    void SetVideoWidth(ULONG Value) { m_nVideoWidth = Value; };
    void SetVideoHeight(ULONG Value) { m_nVideoHeight = Value; };
    void SetVideoAR(BDA_AspectRatio_TYPE Value) { m_nVideoAR = Value; };
    void SetDefaultAudio(int Value) { m_nDefaultAudio = Value; }
    void SetDefaultSubtitle(int Value) { m_nDefaultSubtitle = Value; }

    void AddStreamInfo(ULONG ulPID, BDA_STREAM_TYPE nType, PES_STREAM_TYPE nPesType, LPCTSTR strLanguage);

    bool operator < (CBDAChannel const& channel) const {
        int aOriginNumber = GetOriginNumber();
        int bOriginNumber = channel.GetOriginNumber();
        return (aOriginNumber == 0 && bOriginNumber == 0) ? GetPrefNumber() < channel.GetPrefNumber() : (aOriginNumber == 0 || bOriginNumber == 0) ? bOriginNumber == 0 : aOriginNumber < bOriginNumber;
    }

    // Returns true for channels with the same place, doesn't necessarily need to be equal (i.e if internal streams were updated)
    bool operator==(CBDAChannel const& channel) const {
        return GetPMT() == channel.GetPMT() && GetFrequency() == channel.GetFrequency();
    }

private:
    CString m_strName;
    ULONG m_ulFrequency             = 0;
    ULONG m_ulBandwidth             = 0;
    ULONG m_ulSymbolRate            = 0;
    int m_nPrefNumber               = 0;
    int m_nOriginNumber             = 0;
    int m_nATSCMajor                = 0;
    int m_nATSCMinor                = 0;
    bool m_bEncrypted               = false;
    bool m_bNowNextFlag             = false;
    ULONG m_ulONID                  = 0;
    ULONG m_ulTSID                  = 0;
    ULONG m_ulSID                   = 0;
    ULONG m_ulPMT                   = 0;
    ULONG m_ulPCR                   = 0;
    ULONG m_ulVideoPID              = 0;
    BDA_STREAM_TYPE m_nVideoType    = BDA_MPV;
    BDA_FPS_TYPE m_nVideoFps        = BDA_FPS_25_0;
    BDA_CHROMA_TYPE m_nVideoChroma  = BDA_Chroma_4_2_0;
    ULONG m_nVideoWidth             = 0;
    ULONG m_nVideoHeight            = 0;
    BDA_AspectRatio_TYPE m_nVideoAR = BDA_AR_NULL;
    int m_nAudioCount               = 0;
    int m_nDefaultAudio             = 0;
    int m_nSubtitleCount            = 0;
    int m_nDefaultSubtitle          = -1;
    std::array<BDAStreamInfo, BDA_MAX_AUDIO> m_Audios;
    std::array<BDAStreamInfo, BDA_MAX_SUBTITLE> m_Subtitles;

    void FromString(CString strValue);
};
