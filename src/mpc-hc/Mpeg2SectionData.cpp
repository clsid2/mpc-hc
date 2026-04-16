/*
 * (C) 2009-2017 see Authors.txt
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
#include "BaseClasses/streams.h"
#include <mpeg2data.h>

#include "GolombBuffer.h"
#include "Mpeg2SectionData.h"
#include "FreeviewEPGDecode.h"
#include "resource.h"
#include "Logger.h"

#define BeginEnumDescriptors(gb, nType, nLength)                    \
{                                                                   \
    BYTE DescBuffer[256];                                           \
    size_t nLimit = (size_t)gb.BitRead(12) + gb.GetPos();           \
    while (gb.GetPos() < nLimit) {                                  \
        MPEG2_DESCRIPTOR nType = (MPEG2_DESCRIPTOR)gb.BitRead(8);   \
        WORD nLength = (WORD)gb.BitRead(8);

#define SkipDescriptor(gb, nType, nLength)                          \
    gb.ReadBuffer(DescBuffer, nLength);                             \
    BDA_LOG(_T("Skipped descriptor : 0x%02x"), nType);              \
    UNREFERENCED_PARAMETER(nType);

#define EndEnumDescriptors }}

#define BeginEnumDescriptorsATSC(gb, nType, nLength, bits)          \
{                                                                   \
    BYTE DescBuffer[256];                                           \
    size_t nLimit = (size_t)gb.BitRead(bits) + gb.GetPos();         \
    while (gb.GetPos() < nLimit) {                                  \
        MPEG2_DESCRIPTOR nType = (MPEG2_DESCRIPTOR)gb.BitRead(8);   \
        WORD nLength = (WORD)gb.BitRead(8);


void UTF16BE2LE(BYTE* in, int len) {
    for (int i = 0; i < len; i+=2) {
        BYTE in0 = in[i];
        in[i] = in[i+1];
        in[i+1] = in0;
    }
    in[len] = 0;
    in[len + 1] = 0;
}

CMpeg2DataParser::CMpeg2DataParser(IBaseFilter* pFilter)
{
    m_pData = pFilter;

    ZeroMemory(&m_Filter, sizeof(m_Filter));
    m_Filter.bVersionNumber = 1;
    m_Filter.wFilterSize = MPEG2_FILTER_VERSION_1_SIZE;
    m_Filter.fSpecifySectionNumber = TRUE;
}

CStringW CMpeg2DataParser::ConvertString(BYTE* pBuffer, size_t uLength)
{
    static const UINT16 codepages[0x20] = {
        20269,  // 00 - Default ISO/IEC 6937
        28595,  // 01 - ISO 8859-5 Cyrillic
        28596,  // 02 - ISO 8859-6 Arabic
        28597,  // 03 - ISO 8859-7 Greek
        28598,  // 04 - ISO 8859-8 Hebrew
        28599,  // 05 - ISO 8859-9 Latin 5
        28591,  // 06 - ??? - ISO/IEC 8859-10 - Latin alphabet No. 6
        28591,  // 07 - ??? - ISO/IEC 8859-11 - Latin/Thai (draft only)
        28591,  // 08 - reserved
        28603,  // 09 - ISO 8859-13 - Estonian
        28591,  // 0a - ??? - ISO/IEC 8859-14 - Latin alphabet No. 8 (Celtic)
        28605,  // 0b - ISO 8859-15 Latin 9
        28591,  // 0c - reserved
        28591,  // 0d - reserved
        28591,  // 0e - reserved
        28591,  // 0f - reserved
        0,      // 10 - See codepages10 array
        28591,  // 11 - ??? - ISO/IEC 10646 - Basic Multilingual Plane (BMP)
        28591,  // 12 - ??? - KSX1001-2004 - Korean Character Set
        20936,  // 13 - Chinese Simplified (GB2312-80)
        950,    // 14 - Chinese Traditional (Big5)
        65001,  // 15 - UTF-8 encoding of ISO/IEC 10646 - Basic Multilingual Plane (BMP)
        28591,  // 16 - reserved
        28591,  // 17 - reserved
        28591,  // 18 - reserved
        28591,  // 19 - reserved
        28591,  // 1a - reserved
        28591,  // 1b - reserved
        28591,  // 1c - reserved
        28591,  // 1d - reserved
        28591,  // 1e - reserved
        28591   // 1f - TODO!
    };

    static const UINT16 codepages10[0x10] = {
        28591,  // 00 - reserved
        28591,  // 01 - ISO 8859-1 Western European
        28592,  // 02 - ISO 8859-2 Central European
        28593,  // 03 - ISO 8859-3 Latin 3
        28594,  // 04 - ISO 8859-4 Baltic
        28595,  // 05 - ISO 8859-5 Cyrillic
        28596,  // 06 - ISO 8859-6 Arabic
        28597,  // 07 - ISO 8859-7 Greek
        28598,  // 08 - ISO 8859-8 Hebrew
        28599,  // 09 - ISO 8859-9 Turkish
        28591,  // 0a - ??? - ISO/IEC 8859-10
        28591,  // 0b - ??? - ISO/IEC 8859-11
        28591,  // 0c - ??? - ISO/IEC 8859-12
        28603,  // 0d - ISO 8859-13 Estonian
        28591,  // 0e - ??? - ISO/IEC 8859-14
        28605,  // 0f - ISO 8859-15 Latin 9

        // 0x10 to 0xFF - reserved for future use
    };

    CStringW strResult;
    if (uLength > 0) {
        UINT cp = CP_ACP;
        int nDestSize;

        if (pBuffer[0] == 0x10) {
            pBuffer++;
            uLength--;
            if (pBuffer[0] == 0x00) {
                cp = codepages10[pBuffer[1]];
            } else { // if (pBuffer[0] > 0x00)
                // reserved for future use, use default codepage
                cp = codepages[0];
            }
            pBuffer += 2;
            uLength -= 2;
        } else if (pBuffer[0] < 0x20) {
            cp = codepages[pBuffer[0]];
            pBuffer++;
            uLength--;
        } else { // No code page indication, use the default
            cp = codepages[0];
        }

        // Work around a bug in MS MultiByteToWideChar with ISO/IEC 6937 and take care of the Euro symbol special case (step 1/2)...
        CArray<size_t> euroSymbolPos;
        if (cp == 20269) {
            BYTE tmp;
            for (size_t i = 0; i < uLength - 1; i++) {
                if (pBuffer[i] >= 0xC1 && pBuffer[i] <= 0xCF && pBuffer[i] != 0xC9 && pBuffer[i] != 0xCC) {
                    // Swap the current char with the next one
                    tmp = pBuffer[i];
                    pBuffer[i] = pBuffer[i + 1];
                    pBuffer[++i] = tmp;
                } else if (pBuffer[i] == 0xA4) { // € was added as 0xA4 in the DVB spec
                    euroSymbolPos.Add(i);
                }
            }
            // Handle last symbol if it's a €
            if (pBuffer[uLength - 1] == 0xA4) {
                euroSymbolPos.Add(uLength - 1);
            }
        }

        nDestSize = MultiByteToWideChar(cp, MB_PRECOMPOSED, (LPCSTR)pBuffer, (int)uLength, nullptr, 0);
        if (nDestSize > 0) {
            LPWSTR strResultBuff = strResult.GetBuffer(nDestSize);
            MultiByteToWideChar(cp, MB_PRECOMPOSED, (LPCSTR)pBuffer, (int)uLength, strResultBuff, nDestSize);

            // Work around a bug in MS MultiByteToWideChar with ISO/IEC 6937 and take care of the Euro symbol special case (step 2/2)...
            if (cp == 20269) {
                for (size_t i = 0, len = (size_t)nDestSize; i < len; i++) {
                    switch (strResultBuff[i]) {
                        case 0x60: // grave accent
                            strResultBuff[i] = 0x0300;
                            break;
                        case 0xb4: // acute accent
                            strResultBuff[i] = 0x0301;
                            break;
                        case 0x5e: // circumflex accent
                            strResultBuff[i] = 0x0302;
                            break;
                        case 0x7e: // tilde
                            strResultBuff[i] = 0x0303;
                            break;
                        case 0xaf: // macron
                            strResultBuff[i] = 0x0304;
                            break;
                        case 0xf8f8: // dot
                            strResultBuff[i] = 0x0307;
                            break;
                    }
                }

                for (INT_PTR i = 0, len = euroSymbolPos.GetCount(); i < len; i++) {
                    strResultBuff[euroSymbolPos[i]] = _T('€');
                }
            }

            // Some strings seems to be null-terminated, we need to take that into account.
            while (nDestSize > 0 && strResultBuff[nDestSize - 1] == L'\0') {
                nDestSize--;
            }

            strResult.ReleaseBuffer(nDestSize);
        }
    }

    return strResult;
}

BDA_STREAM_TYPE CMpeg2DataParser::ConvertToDVBType(PES_STREAM_TYPE nType)
{
    switch (nType) {
        case VIDEO_STREAM_MPEG1:
        case VIDEO_STREAM_MPEG2:
            return BDA_MPV;
        case AUDIO_STREAM_MPEG1:
        case AUDIO_STREAM_MPEG2:
            return BDA_MPA;
        case VIDEO_STREAM_H264:
            return BDA_H264;
        case VIDEO_STREAM_HEVC:
            return BDA_HEVC;
        case AUDIO_STREAM_AC3:
            return BDA_AC3;
        case AUDIO_STREAM_AC3_PLUS:
            return BDA_EAC3;
        case AUDIO_STREAM_AAC:
            return BDA_ADTS;
        case AUDIO_STREAM_AAC_LATM:
            return BDA_LATM;
        case SUBTITLE_STREAM:
            return BDA_SUBTITLE;
    }

    return BDA_UNKNOWN;
}

HRESULT CMpeg2DataParser::ParseSIHeader(CGolombBuffer& gb, DVB_SI SIType, WORD& wSectionLength, WORD& wTSID)
{
    if (gb.BitRead(8) != SIType) {
        return ERROR_INVALID_DATA;              // table_id
    }
    gb.BitRead(1);                              // section_syntax_indicator
    gb.BitRead(1);                              // reserved_future_use
    gb.BitRead(2);                              // reserved
    wSectionLength = (WORD)gb.BitRead(12);      // section_length
    wTSID = (WORD)gb.BitRead(16);               // transport_stream_id
    gb.BitRead(2);                              // reserved
    gb.BitRead(5);                              // version_number
    gb.BitRead(1);                              // current_next_indicator
    gb.BitRead(8);                              // section_number
    gb.BitRead(8);                              // last_section_number

    return S_OK;
}

HRESULT CMpeg2DataParser::ParseSDT(ULONG ulFrequency, ULONG ulBandwidth, ULONG ulSymbolRate)
{
    HRESULT hr;
    CComPtr<ISectionList> pSectionList;
    DWORD dwLength;
    PSECTION data;
    WORD wTSID;
    WORD wONID;
    WORD wSectionLength;
    WORD serviceType = 0;

    if (!m_pData) {
        return E_FAIL;
    }

    CheckNoLog(m_pData->GetSection(PID_SDT, SI_SDT, &m_Filter, 15000, &pSectionList));
    CheckNoLog(pSectionList->GetSectionData(0, &dwLength, &data));

    CGolombBuffer gb((BYTE*)data, dwLength);

    // service_description_section()
    CheckNoLog(ParseSIHeader(gb, SI_SDT, wSectionLength, wTSID));

    wONID = (WORD)gb.BitRead(16);                               // original_network_id
    gb.BitRead(8);                                              // reserved_future_use

    while (gb.GetSize() - gb.GetPos() > 4) {
        CBDAChannel Channel;
        Channel.SetFrequency(ulFrequency);
        Channel.SetBandwidth(ulBandwidth);
        Channel.SetSymbolRate(ulSymbolRate);
        Channel.SetTSID(wTSID);
        Channel.SetONID(wONID);
        Channel.SetSID((ULONG)gb.BitRead(16));                  // service_id   uimsbf
        gb.BitRead(6);                                          // reserved_future_use   bslbf
        gb.BitRead(1);                                          // EIT_schedule_flag   bslbf
        Channel.SetNowNextFlag(!!gb.BitRead(1));                // EIT_present_following_flag   bslbf
        gb.BitRead(3);                                          // running_status   uimsbf
        Channel.SetEncrypted(!!gb.BitRead(1));                  // free_CA_mode   bslbf

        // Descriptors:
        BeginEnumDescriptors(gb, nType, nLength) {
            switch (nType) {
                case DT_SERVICE:
                    serviceType = (WORD)gb.BitRead(8);          // service_type
                    nLength = (WORD)gb.BitRead(8);              // service_provider_name_length
                    gb.ReadBuffer(DescBuffer, nLength);         // service_provider_name

                    nLength = (WORD)gb.BitRead(8);              // service_name_length
                    gb.ReadBuffer(DescBuffer, nLength);         // service_name
                    DescBuffer[nLength] = 0;
                    Channel.SetName(ConvertString(DescBuffer, nLength));
                    BDA_LOG(_T("%-20s %lu"), Channel.GetName(), Channel.GetSID());
                    break;
                default:
                    SkipDescriptor(gb, nType, nLength);         // descriptor()
                    break;
            }
        }
        EndEnumDescriptors;


        if (!Channels.Lookup(Channel.GetSID())) {
            switch (serviceType) {
                case DIGITAL_TV:
                case DIGITAL_RADIO:
                case AVC_DIGITAL_RADIO:
                case MPEG2_HD_DIGITAL_TV:
                case AVC_SD_TV:
                case AVC_HD_TV:
                case HEVC_TV:
                    Channels[Channel.GetSID()] = Channel;
                    break;
                default:
                    BDA_LOG(_T("DVB: Skipping not supported service: %-20s %lu"), Channel.GetName(), Channel.GetSID());
                    break;
            }
        }
    }

    return S_OK;
}

//supports Master Guide Table; see ATSC A/65:2013
HRESULT CMpeg2DataParser::ParseMGT(enum DVB_SI &vctType)
{
    vctType = SI_undef;
    HRESULT hr;
    CComPtr<ISectionList> pSectionList;
    DWORD dwLength;
    PSECTION data;
    WORD wTSID;
    WORD wSectionLength;

    hr = m_pData->GetSection(PID_PSIP, TID_MGT, &m_Filter, 15000, &pSectionList);
    CheckNoLog(hr);
    CheckNoLog(pSectionList->GetSectionData(0, &dwLength, &data));

    CGolombBuffer gb((BYTE*)data, dwLength);

    CheckNoLog(ParseSIHeader(gb, TID_MGT, wSectionLength, wTSID));
    gb.BitRead(8);
    uint16_t num_tables = gb.BitRead(16);

    for (uint8_t i = 0; i < num_tables; i++) {
        uint16_t table_type = gb.BitRead(16); //table_type
        
        gb.BitRead(3);  //reserved
        uint16_t table_type_PID = gb.BitRead(13); //table_type_PID
        if (table_type_PID == PID_PSIP) { //expect to find TVCT table with PID_PSIP id
            if (table_type == TT_TVCT_C0 || table_type == TT_TVCT_C1) {
                vctType = (DVB_SI)TID_TVCT;
            } else if (table_type == TT_CVCT_C0 || table_type == TT_CVCT_C1) {
                vctType = (DVB_SI)TID_CVCT;
            }
        }
        gb.BitRead(3);  //reserved
        gb.BitRead(5);  //table_type_version_number
        gb.BitRead(32); //number_bytes
        gb.BitRead(4);  //reserved

        BeginEnumDescriptorsATSC(gb, nType, nLength, 12) {          // for (i=0;i<N;i++) {
            SkipDescriptor(gb, nType, nLength);                     // descriptor()
        }
        EndEnumDescriptors;
    }

    return S_OK;
}

//supports Terrestrial Virtual Channel Table and Cable Virtual Channel Table; see ATSC A/65:2013
HRESULT CMpeg2DataParser::ParseVCT(ULONG ulFrequency, ULONG ulBandwidth, ULONG ulSymbolRate, enum DVB_SI vctType)
{
    HRESULT hr;
    CComPtr<ISectionList> pSectionList;
    DWORD dwLength;
    PSECTION data;
    WORD wTSID;
    WORD wSectionLength;
    WORD serviceType = 0;

    hr = m_pData->GetSection(PID_PSIP, vctType, &m_Filter, 15000, &pSectionList);
    CheckNoLog(hr);
    CheckNoLog(pSectionList->GetSectionData(0, &dwLength, &data));

    CGolombBuffer gb((BYTE*)data, dwLength);

    CheckNoLog(ParseSIHeader(gb, vctType, wSectionLength, wTSID));
    gb.BitRead(8); //protocol_version
    uint8_t num_channels = gb.BitRead(8);

    const int SHORT_NAME_LEN = 7*2;
    BYTE short_name[SHORT_NAME_LEN+2];

    for (uint8_t i = 0; i < num_channels; i++) {
        gb.ReadBuffer(short_name, SHORT_NAME_LEN); //short_name
        UTF16BE2LE(short_name, SHORT_NAME_LEN);
        CStringW shortName((wchar_t*)short_name);
        gb.BitRead(4); //reserved
        uint16_t major_channel_number = gb.BitRead(10); //major_channel_number
        uint16_t minor_channel_number = gb.BitRead(10); //minor_channel_number
        gb.BitRead(8); //modulation_mode
        gb.BitRead(32); //carrier_frequency
        uint16_t channel_TSID=gb.BitRead(16); //channel_TSID
        uint16_t program_number=gb.BitRead(16); //program_number
        gb.BitRead(2); //ETM_location
        gb.BitRead(1); //access_controlled
        gb.BitRead(1); //hidden
        gb.BitRead(2); //TVCT: reserved(1), CVCT: path_select(1) / out_of_band(1)
        gb.BitRead(1); //hide_guide
        gb.BitRead(3); //reserved
        serviceType = gb.BitRead(6); //service_type
        uint16_t source_id=gb.BitRead(16); //source_id
        gb.BitRead(6); //reserved
        BeginEnumDescriptorsATSC(gb, nType, nLength, 10) {          // for (i=0;i<N;i++) {
            SkipDescriptor(gb, nType, nLength);                     // descriptor()
        }
        EndEnumDescriptors;

        CBDAChannel Channel;
        CStringW name;
        name.Format(L"%d.%d - %s", major_channel_number, minor_channel_number, shortName);
        Channel.SetName(name);
        Channel.SetFrequency(ulFrequency);
        Channel.SetBandwidth(ulBandwidth);
        Channel.SetSymbolRate(ulSymbolRate);
        Channel.SetTSID(channel_TSID);
        Channel.SetONID(0); //ATSC doesn't apply
        Channel.SetSID(program_number);

        if (!Channels.Lookup(Channel.GetSID())) {
            switch (serviceType) {
            case ATSC_DIGITAL_TV:
                Channels[Channel.GetSID()] = Channel;
                break;
            default:
                BDA_LOG(_T("ATSC: Skipping not supported service: %-20s %lu"), Channel.GetName(), Channel.GetSID());
                break;
            }
        }
    }

    return S_OK;
}

HRESULT CMpeg2DataParser::ParsePAT()
{
    HRESULT hr;
    CComPtr<ISectionList> pSectionList;
    DWORD dwLength;
    PSECTION data;
    WORD wTSID;
    WORD wSectionLength;

    CheckNoLog(m_pData->GetSection(PID_PAT, SI_PAT, &m_Filter, 15000, &pSectionList));
    CheckNoLog(pSectionList->GetSectionData(0, &dwLength, &data));

    CGolombBuffer gb((BYTE*)data, dwLength);

    // program_association_section()
    CheckNoLog(ParseSIHeader(gb, SI_PAT, wSectionLength, wTSID));
    while (gb.GetSize() - gb.GetPos() > 4) {
        WORD program_number = (WORD)gb.BitRead(16);         // program_number
        gb.BitRead(3);                                      // reserved
        if (program_number == 0) {
            gb.BitRead(13);                                 // network_PID
        } else {
            WORD program_map_PID = (WORD)gb.BitRead(13);    // program_map_PID
            if (Channels.Lookup(program_number)) {
                Channels [program_number].SetPMT(program_map_PID);
                ParsePMT(Channels [program_number]);
            }
        }
    }

    return S_OK;
}

HRESULT CMpeg2DataParser::ParsePMT(CBDAChannel& Channel)
{
    HRESULT hr;
    CComPtr<ISectionList> pSectionList;
    DWORD dwLength;
    PSECTION data;
    WORD wTSID;
    WORD wSectionLength;

    Channel.SetVideoFps(BDA_FPS_NONE);
    Channel.SetVideoChroma(BDA_Chroma_NONE);

    CheckNoLog(m_pData->GetSection((PID)Channel.GetPMT(), SI_PMT, &m_Filter, 15000, &pSectionList));
    CheckNoLog(pSectionList->GetSectionData(0, &dwLength, &data));

    CGolombBuffer gb((BYTE*)data, dwLength);

    // TS_program_map_section()
    CheckNoLog(ParseSIHeader(gb, SI_PMT, wSectionLength, wTSID));

    gb.BitRead(3);                                              // reserved
    Channel.SetPCR((ULONG)gb.BitRead(13));                      // PCR_PID
    gb.BitRead(4);                                              // reserved
    BeginEnumDescriptors(gb, nType, nLength) {                  // for (i=0;i<N;i++) {
        SkipDescriptor(gb, nType, nLength);                     // descriptor()
    }
    EndEnumDescriptors;


    while (gb.GetSize() - gb.GetPos() > 4) {
        PES_STREAM_TYPE pes_stream_type;
        BDA_STREAM_TYPE dvb_stream_type;
        WORD wPID;
        CString strLanguage;

        pes_stream_type = (PES_STREAM_TYPE)gb.BitRead(8);       // stream_type
        gb.BitRead(3);                                          // reserved
        wPID = (WORD)gb.BitRead(13);                            // elementary_PID
        gb.BitRead(4);                                          // reserved

        BeginEnumDescriptors(gb, nType, nLength) {              // ES_info_length
            switch (nType) {
                case DT_ISO_639_LANGUAGE:
                    gb.ReadBuffer(DescBuffer, nLength);
                    strLanguage = ConvertString(DescBuffer, 3);
                    break;
                case DT_AC3_AUDIO:
                    pes_stream_type = AUDIO_STREAM_AC3;
                    SkipDescriptor(gb, nType, nLength);
                    break;
                case DT_EXTENDED_AC3_AUDIO:
                    pes_stream_type = AUDIO_STREAM_AC3_PLUS;
                    SkipDescriptor(gb, nType, nLength);
                    break;
                case DT_AAC_AUDIO:
                    pes_stream_type = AUDIO_STREAM_AAC_LATM;
                    SkipDescriptor(gb, nType, nLength);
                    break;
                case DT_SUBTITLING:
                    gb.ReadBuffer(DescBuffer, nLength);
                    strLanguage = ConvertString(DescBuffer, 3);
                    pes_stream_type = SUBTITLE_STREAM;
                    break;
                case DT_VIDEO_STREAM: {
                    gb.BitRead(1);                      // multiple_frame_rate_flag
                    Channel.SetVideoFps((BDA_FPS_TYPE) gb.BitRead(4));
                    UINT MPEG_1_only_flag  = (UINT) gb.BitRead(1);
                    gb.BitRead(1);                      // constrained_parameter_flag
                    gb.BitRead(1);                      // still_picture_flag
                    if (!MPEG_1_only_flag) {
                        gb.BitRead(8);                  // profile_and_level_indicator
                        Channel.SetVideoChroma((BDA_CHROMA_TYPE) gb.BitRead(2));
                        gb.BitRead(1);                  // frame_rate_extension_flag
                        gb.BitRead(5);                  // Reserved
                    }
                }
                break;
                case DT_TARGET_BACKGROUND_GRID:
                    Channel.SetVideoWidth((ULONG) gb.BitRead(14));
                    Channel.SetVideoHeight((ULONG) gb.BitRead(14));
                    Channel.SetVideoAR((BDA_AspectRatio_TYPE) gb.BitRead(4));
                    break;
                default:
                    SkipDescriptor(gb, nType, nLength);
                    break;
            }
        }
        EndEnumDescriptors;
        if ((dvb_stream_type = ConvertToDVBType(pes_stream_type)) != BDA_UNKNOWN) {
            Channel.AddStreamInfo(wPID, dvb_stream_type, pes_stream_type, strLanguage);
        }
    }
    if ((Channel.GetVideoType() == BDA_MPV) && (Channel.GetVideoPID())) {
        if (Channel.GetVideoFps() == BDA_FPS_NONE) {
            Channel.SetVideoFps(BDA_FPS_25_0);
        }
        if ((Channel.GetVideoWidth() == 0) && (Channel.GetVideoHeight() == 0)) {
            Channel.SetVideoWidth(720);
            Channel.SetVideoHeight(576);
        }
    } else if ((Channel.GetVideoType() == BDA_H264 || Channel.GetVideoType() == BDA_HEVC) && (Channel.GetVideoPID())) {
        if (Channel.GetVideoFps() == BDA_FPS_NONE) {
            Channel.SetVideoFps(BDA_FPS_25_0);
        }
    }


    return S_OK;
}

HRESULT CMpeg2DataParser::SetTime(CGolombBuffer& gb, EventDescriptor& NowNext)
{
    wchar_t descBuffer[6];
    time_t  tNow, tTime;
    tm      tmTime;
    long    timezone;

    // init tm structures
    time(&tNow);
    gmtime_s(&tmTime, &tNow);
    _tzset();
    _get_timezone(&timezone); // The difference in seconds between UTC and local time.

    // Start time:
    tmTime.tm_hour  = (int)(gb.BitRead(4) * 10);
    tmTime.tm_hour += (int)gb.BitRead(4);
    tmTime.tm_min   = (int)(gb.BitRead(4) * 10);
    tmTime.tm_min  += (int)gb.BitRead(4);
    tmTime.tm_sec   = (int)(gb.BitRead(4) * 10);
    tmTime.tm_sec  += (int)gb.BitRead(4);
    // Convert to time_t
    // mktime() expect tm struct to be local time and since we are feeding it with GMT
    // we need to compensate for timezone offset. Note that we don't compensate for DST.
    // That is because tm_isdst is set to 0 (GMT) and in this case mktime() won't add any offset.
    NowNext.startTime = tTime = mktime(&tmTime) - timezone;
    while (tNow < tTime) {
        // If current time is less than even start time it is likely that event started the day before.
        // We go one day back
        NowNext.startTime = tTime -= 86400;
    }

    localtime_s(&tmTime, &tTime);
    wcsftime(descBuffer, 6, L"%H:%M", &tmTime);
    descBuffer[5] = '\0';
    NowNext.strStartTime = descBuffer;

    // Duration:
    NowNext.duration  = (time_t)(36000 * gb.BitRead(4));
    NowNext.duration += (time_t)(3600 * gb.BitRead(4));
    NowNext.duration += (time_t)(600 * gb.BitRead(4));
    NowNext.duration += (time_t)(60 * gb.BitRead(4));
    NowNext.duration += (time_t)(10 * gb.BitRead(4));
    NowNext.duration += (time_t)gb.BitRead(4);

    tTime += NowNext.duration;
    localtime_s(&tmTime, &tTime);
    wcsftime(descBuffer, 6, L"%H:%M", &tmTime);
    descBuffer[5] = '\0';
    NowNext.strEndTime = descBuffer;

    return S_OK;
}

HRESULT CMpeg2DataParser::ParseEIT(ULONG ulSID, EventDescriptor& NowNext)
{
    HRESULT hr = S_OK;
    DWORD dwLength;
    PSECTION data;
    ULONG ulGetSID;
    EventInformationSection InfoEvent;
    NowNext = EventDescriptor();

    do {
        CComPtr<ISectionList> pSectionList;
        CheckNoLog(m_pData->GetSection(PID_EIT, SI_EIT_act, nullptr, 5000, &pSectionList));
        CheckNoLog(pSectionList->GetSectionData(0, &dwLength, &data));

        CGolombBuffer gb((BYTE*)data, dwLength);

        InfoEvent.TableID = (UINT8)gb.BitRead(8);
        InfoEvent.SectionSyntaxIndicator = (WORD)gb.BitRead(1);
        gb.BitRead(3);
        InfoEvent.SectionLength = (WORD)gb.BitRead(12);
        ulGetSID  = (ULONG)gb.BitRead(8);
        ulGetSID += 0x100 * (ULONG)gb.BitRead(8);
        InfoEvent.ServiceId = ulGetSID; // This is really strange, ServiceID should be uimsbf ???
        if (InfoEvent.ServiceId == ulSID) {
            gb.BitRead(2);
            InfoEvent.VersionNumber = (UINT8)gb.BitRead(5);
            InfoEvent.CurrentNextIndicator = (UINT8)gb.BitRead(1);
            if (InfoEvent.CurrentNextIndicator == 1) {
                InfoEvent.SectionNumber = (UINT8)gb.BitRead(8);
                InfoEvent.LastSectionNumber = (UINT8)gb.BitRead(8);
                InfoEvent.TransportStreamID = (WORD)gb.BitRead(16);
                InfoEvent.OriginalNetworkID = (WORD)gb.BitRead(16);
                InfoEvent.SegmentLastSectionNumber = (UINT8)gb.BitRead(8);
                InfoEvent.LastTableID = (UINT8)gb.BitRead(8);

                // Info event
                InfoEvent.EventID = (WORD)gb.BitRead(16);
                InfoEvent.StartDate = (WORD)gb.BitRead(16);
                SetTime(gb, NowNext);

                InfoEvent.RunninStatus = (WORD)gb.BitRead(3);
                InfoEvent.FreeCAMode = (WORD)gb.BitRead(1);
                if (InfoEvent.RunninStatus == 4) {
                    UINT8 nLen;
                    UINT8 itemsLength;

                    //  Descriptors:
                    BeginEnumDescriptors(gb, nType, nLength) {
                        switch (nType) {
                            case DT_SHORT_EVENT:
                                gb.BitRead(24);                         // ISO_639_language_code

                                nLen = (UINT8)gb.BitRead(8);            // event_name_length
                                gb.ReadBuffer(DescBuffer, nLen);
                                if (IsFreeviewEPG(InfoEvent.OriginalNetworkID, DescBuffer, nLen)) {
                                    NowNext.eventName = DecodeFreeviewEPG(DescBuffer, nLen);
                                } else {
                                    NowNext.eventName = ConvertString(DescBuffer, nLen);
                                }

                                nLen = (UINT8)gb.BitRead(8);            // text_length
                                gb.ReadBuffer(DescBuffer, nLen);
                                if (IsFreeviewEPG(InfoEvent.OriginalNetworkID, DescBuffer, nLen)) {
                                    NowNext.eventDesc = DecodeFreeviewEPG(DescBuffer, nLen);
                                } else {
                                    NowNext.eventDesc = ConvertString(DescBuffer, nLen);
                                }
                                break;
                            case DT_EXTENDED_EVENT:
                                gb.BitRead(4);                          // descriptor_number
                                gb.BitRead(4);                          // last_descriptor_number
                                gb.BitRead(24);                         // ISO_639_language_code

                                itemsLength = (UINT8)gb.BitRead(8);     // length_of_items
                                while (itemsLength > 0) {
                                    nLen = (UINT8)gb.BitRead(8);        // item_description_length
                                    gb.ReadBuffer(DescBuffer, nLen);
                                    CString itemDesc = ConvertString(DescBuffer, nLen);
                                    itemsLength -= nLen + 1;

                                    nLen = (UINT8)gb.BitRead(8);        // item_length
                                    gb.ReadBuffer(DescBuffer, nLen);
                                    CString itemText = ConvertString(DescBuffer, nLen);
                                    itemsLength -= nLen + 1;
                                    NowNext.extendedDescriptorsItems.emplace_back(itemDesc, itemText);
                                }

                                nLen = (UINT8)gb.BitRead(8);            // text_length
                                if (nLen > 0) {
                                    gb.ReadBuffer(DescBuffer, nLen);
                                    NowNext.extendedDescriptorsText += ConvertString(DescBuffer, nLen);
                                }
                                break;
                            case DT_PARENTAL_RATING: {
                                ASSERT(nLength % 4 == 0);
                                int rating = -1;
                                while (nLength >= 4) {
                                    gb.BitRead(24);                         // ISO 3166 country_code
                                    rating = (int)gb.BitRead(8);            // rating
                                    nLength -= 4;
                                }
                                if (rating >= 0 && rating <= 0x0f) {
                                    if (rating > 0) {                       // 0x00 undefined
                                        rating += 3;                        // 0x01 to 0x0F minimum age = rating + 3 years
                                    }
                                    NowNext.parentalRating = rating;
                                }
                            }
                            break;
                            case DT_CONTENT:
                                ASSERT(nLength % 2 == 0);
                                while (nLength >= 2) {
                                    BYTE content = (BYTE)gb.BitRead(4);     // content_nibble_level_1
                                    gb.BitRead(4);                          // content_nibble_level_2
                                    gb.BitRead(8);                          // user_byte
                                    if (1 <= content && content <= 10) {
                                        if (!NowNext.content.IsEmpty()) {
                                            NowNext.content.Append(_T(", "));
                                        }

                                        static UINT contents[] = {
                                            IDS_CONTENT_MOVIE_DRAMA,
                                            IDS_CONTENT_NEWS_CURRENTAFFAIRS,
                                            IDS_CONTENT_SHOW_GAMESHOW,
                                            IDS_CONTENT_SPORTS,
                                            IDS_CONTENT_CHILDREN_YOUTH_PROG,
                                            IDS_CONTENT_MUSIC_BALLET_DANCE,
                                            IDS_CONTENT_MUSIC_ART_CULTURE,
                                            IDS_CONTENT_SOCIAL_POLITICAL_ECO,
                                            IDS_CONTENT_EDUCATION_SCIENCE,
                                            IDS_CONTENT_LEISURE
                                        };

                                        NowNext.content.AppendFormat(contents[content - 1]);
                                    }
                                    nLength -= 2;
                                }
                                break;
                            default:
                                SkipDescriptor(gb, nType, nLength);
                                break;
                        }
                    }
                    EndEnumDescriptors;
                }
            }
        }
        m_Filter.SectionNumber++;
    } while ((InfoEvent.ServiceId != ulSID || InfoEvent.RunninStatus != 4) && m_Filter.SectionNumber <= 22);

    if (m_Filter.SectionNumber > 22 || InfoEvent.CurrentNextIndicator != 1) {
        NowNext = EventDescriptor();
        hr = E_FAIL;
    }

    return hr;
}

HRESULT CMpeg2DataParser::ParseNIT()
{
    HRESULT hr;
    CComPtr<ISectionList> pSectionList;
    DWORD dwLength;
    PSECTION data;
    WORD wTSID;
    WORD wSectionLength;

    CheckNoLog(m_pData->GetSection(PID_NIT, SI_NIT, &m_Filter, 15000, &pSectionList));
    CheckNoLog(pSectionList->GetSectionData(0, &dwLength, &data));

    CGolombBuffer gb((BYTE*)data, dwLength);

    // network_information_section()
    CheckNoLog(ParseSIHeader(gb, SI_NIT, wSectionLength, wTSID));

    gb.BitRead(4);                                              // reserved_future_use
    BeginEnumDescriptors(gb, nType, nLength) {                  // for (i=0;i<N;i++) {
        SkipDescriptor(gb, nType, nLength);                     // descriptor()
    }
    EndEnumDescriptors;

    gb.BitRead(4);                                              // reserved_future_use
    gb.BitRead(12);                                             // network_descriptors_length
    while (gb.GetSize() - gb.GetPos() > 4) {
        WORD transport_stream_id = (WORD)gb.BitRead(16);        // transport_stream_id
        UNREFERENCED_PARAMETER(transport_stream_id);
        WORD original_network_id = (WORD)gb.BitRead(16);        // original_network_id
        UNREFERENCED_PARAMETER(original_network_id);
        gb.BitRead(4);                                          // reserved_future_use
        BeginEnumDescriptors(gb, nType, nLength) {
            switch (nType) {
                case DT_LOGICAL_CHANNEL:
                    for (int i = 0; i < nLength / 4; i++) {
                        WORD service_id  = (WORD)gb.BitRead(16);
                        gb.BitRead(6);
                        WORD logical_channel_number  = (WORD)gb.BitRead(10);
                        if (Channels.Lookup(service_id)) {
                            Channels[service_id].SetOriginNumber(logical_channel_number);
                            BDA_LOG(_T("NIT association : %d -> %s"), logical_channel_number, Channels[service_id].ToString().GetString());
                        }
                    }
                    break;
                default:
                    SkipDescriptor(gb, nType, nLength);
                    break;
            }
        }
        EndEnumDescriptors;
    }

    return S_OK;
}
