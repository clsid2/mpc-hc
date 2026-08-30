/*
 * (C) 2009-2015 see Authors.txt
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

enum PES_STREAM_TYPE {
    INVALID                             = 0,
    VIDEO_STREAM_MPEG1                  = 0x01,
    VIDEO_STREAM_MPEG2                  = 0x02, // ITU-T Rec. H.262 | ISO/IEC 13818-2 Video or ISO/IEC 11172-2 constrained parameter video stream
    AUDIO_STREAM_MPEG1                  = 0x03, // all layers including mp3 (ISO/IEC 11172-3 Audio)
    AUDIO_STREAM_MPEG2                  = 0x04, // ISO/IEC 13818-3 Audio
    PRIVATE                             = 0x05, // ITU-T Rec. H.222.0 | ISO/IEC 13818-1 private_sections
    PES_PRIVATE                         = 0x06, // ITU-T Rec. H.222.0 | ISO/IEC 13818-1 PES packets containing private data
    PES_07                              = 0x07, // ISO/IEC 13522 MHEG
    PES_08                              = 0x08, // ITU-T Rec. H.222.0 | ISO/IEC 13818-1 Annex A DSM-CC
    PES_09                              = 0x09, // ITU-T Rec. H.222.1
    PES_0a                              = 0x0a, // ISO/IEC 13818-6 type A
    PES_0b                              = 0x0b, // ISO/IEC 13818-6 type B
    PES_0c                              = 0x0c, // ISO/IEC 13818-6 type C
    PES_0d                              = 0x0d, // ISO/IEC 13818-6 type D
    PES_0e                              = 0x0e, // ITU-T Rec. H.222.0 | ISO/IEC 13818-1 auxiliary
    AUDIO_STREAM_AAC                    = 0x0f, // ISO/IEC 13818-7 Audio with ADTS transport syntax
    PES_10                              = 0x10, // ISO/IEC 14496-2 Visual
    AUDIO_STREAM_AAC_LATM               = 0x11, // ISO/IEC 14496-3 Audio with the LATM transport syntax as defined in ISO/IEC 14496-3 / AMD 1
    PES_12                              = 0x12, // ISO/IEC 14496-1 SL-packetized stream or FlexMux stream carried in PES packets
    PES_13                              = 0x13, // ISO/IEC 14496-1 SL-packetized stream or FlexMux stream carried in ISO/IEC14496_sections.
    PES_14                              = 0x14, // ISO/IEC 13818-6 Synchronized Download Protocol
    METADATA_PES_PACKETS                = 0x15, // Metadata carried in PES packets
    METADATA_SECTIONS                   = 0X16, // Metadata carried in metadata_sections
    DATA_CAROUSEL                       = 0x17, // Metadata carried in ISO/IEC 13818-6 Data Carousel
    OBJECT_CAROUSEL                     = 0x18, // Metadata carried in ISO/IEC 13818-6 Object Carousel
    SYNCHRONIZED_DOWNLOAD               = 0x19, // Metadata carried in ISO/IEC 13818-6 Synchronized Download Protocol
    IPMP                                = 0x1A, // IPMP stream (defined in ISO/IEC 13818-11, MPEG-2 IPMP)
    VIDEO_STREAM_H264                   = 0x1B, // AVC video stream conforming to one or more profiles defined in Annex A of ITU-T Rec. H.264
    PES_1C                              = 0x1C, // ISO/IEC 14496-3 Audio, without using any additional transport syntax, such as DST, ALS and SLS
    TEXT                                = 0x1D, // ISO/IEC 14496-17 Text
    AUXILIARY_VIDEO_STREAM              = 0x1E, // Auxiliary video stream as defined in ISO/IEC 23002-3
    SVC_H264                            = 0x1F, // SVC video sub-bitstream of an AVC video stream conforming to one or more profiles defined in Annex G of ITU-T Rec. H.264 | ISO/IEC 14496-10
    MVC_H264                            = 0x20, // MVC video sub-bitstream of an AVC video stream conforming to one or more profiles defined in Annex H of ITU-T Rec. H.264 | ISO/IEC 14496-10
    VIDEO_STREAM_JPEG2000               = 0x21, // ITU - T Rec.T.800 | ISO / IEC 15444 - 1
    ADDITIONAL_VIEW_MPEG2               = 0x22, // ITU - T Rec.H.262 | ISO / IEC 13818 - 2 Additional view for compatible 3D
    ADDITIONAL_VIEW_H264                = 0x23, // ITU - T Rec.H.264 | ISO / IEC 14496 - 10 Additional view for compatible 3D
    VIDEO_STREAM_HEVC                   = 0x24, // ITU - T Rec.H.265 | ISO / IEC 23008 - 2 video stream
    VIDEO_SUBSET_HEVC                   = 0x25, // ITU - T Rec.H.265 | ISO / IEC 23008 - 2 Annex A temporal video subset
    MVCD_H264                           = 0x26, // ITU - T Rec.H.264 | ISO / IEC 14496 - 10 Annex I MVCD video sub - bitstream
    AUDIO_STREAM_LPCM                   = 0x80,
    AUDIO_STREAM_AC3                    = 0x81,
    AUDIO_STREAM_DTS                    = 0x82,
    AUDIO_STREAM_AC3_TRUE_HD            = 0x83,
    AUDIO_STREAM_AC3_PLUS               = 0x84,
    AUDIO_STREAM_DTS_HD                 = 0x85,
    AUDIO_STREAM_DTS_HD_MASTER_AUDIO    = 0x86,
    AUDIO_STREAM_EAC3_ATSC              = 0x87, // ATSC A/53 E-AC-3. The values above are Blu-ray registrations; ATSC signals E-AC-3 with its own stream type, not with 0x84
    PRESENTATION_GRAPHICS_STREAM        = 0x90,
    INTERACTIVE_GRAPHICS_STREAM         = 0x91,
    SUBTITLE_STREAM                     = 0x92,
    SECONDARY_AUDIO_AC3_PLUS            = 0xa1,
    SECONDARY_AUDIO_DTS_HD              = 0xa2,
    VIDEO_STREAM_VC1                    = 0xea
};

enum MPEG2_PID {
    PID_PAT     = 0x000,    // Program Association Table
    PID_CAT     = 0x001,    // Conditional Access Table
    PID_TSDT    = 0x002,    // Transport Stream Description Table
    PID_NIT     = 0x010,    // Network Identification Table
    PID_BAT     = 0x011,    // Bouquet Association Table or ...
    PID_SDT     = 0x011,    // ... Service Description Table
    PID_EIT     = 0x012,    // Event Information Table
    PID_RST     = 0x013,    // Running Status Table
    PID_TDT     = 0x014,    // Time and Date Table or ...
    PID_TOT     = 0x014,    // ... Time Offset Table
    PID_SFN     = 0x015,    // SFN/MIP synchronization
    PID_DIT     = 0x01e,
    PID_SIT     = 0x01f,
    PID_ATSC_PAT_E = 0x1FF7,
    PID_PSIP_TS_E = 0x1FF9,
    PID_PSIP    = 0x1FFB,
    PID_NULL    = 0x1fff    // Null packet
};

enum DVB_SI {
    SI_undef    = -1,
    SI_PAT      = 0x00,
    SI_CAT      = 0x01,
    SI_PMT      = 0x02,
    SI_DSMCC_a  = 0x3a,
    SI_DSMCC_b  = 0x3b,
    SI_DSMCC_c  = 0x3c,
    SI_DSMCC_d  = 0x3d,
    SI_DSMCC_e  = 0x3e,
    SI_DSMCC_f  = 0x3f,
    SI_NIT      = 0x40,
    SI_SDT      = 0x42,
    SI_EIT_act  = 0x4e,
    SI_EIT_oth  = 0x4f,
    SI_EIT_as0, SI_EIT_as1, SI_EIT_as2, SI_EIT_as3, SI_EIT_as4, SI_EIT_as5, SI_EIT_as6, SI_EIT_as7,
    SI_EIT_as8, SI_EIT_as9, SI_EIT_asa, SI_EIT_asb, SI_EIT_asc, SI_EIT_asd, SI_EIT_ase, SI_EIT_asf,
    SI_EIT_os0, SI_EIT_os1, SI_EIT_os2, SI_EIT_os3, SI_EIT_os4, SI_EIT_os5, SI_EIT_os6, SI_EIT_os7,
    SI_EIT_os8, SI_EIT_os9, SI_EIT_osa, SI_EIT_osb, SI_EIT_osc, SI_EIT_osd, SI_EIT_ose, SI_EIT_osf,
    TID_MGT     = 0xC7,
    TID_TVCT    = 0xC8,
    TID_CVCT    = 0xC9
};

enum ATSC_TABLE_TYPE {
    TT_TVCT_C1 = 0x0000,
    TT_TVCT_C0 = 0x0001,
    TT_CVCT_C1 = 0x0002,
    TT_CVCT_C0 = 0x0003,
    TT_ETT     = 0x0004,
    TT_DCCSCT  = 0x0005,
};

enum MPEG2_DESCRIPTOR {
    // http://www.coolstf.com/tsreader/descriptors.html
    DT_VIDEO_STREAM             = 0x02,
    DT_AUDIO_STREAM             = 0x03,
    DT_HIERARCHY                = 0x04,
    DT_REGISTRATION             = 0x05,
    DT_DATA_STREAM_ALIGNMENT    = 0x06,
    DT_TARGET_BACKGROUND_GRID   = 0x07,
    DT_VIDEO_WINDOW             = 0x08,
    DT_CONDITIONAL_ACCESS       = 0x09,
    DT_ISO_639_LANGUAGE         = 0x0a,
    DT_SYSTEM_CLOCK             = 0x0b,
    DT_MULTIPLEX_BUFFER_UTIL    = 0x0c,
    DT_COPYRIGHT_DESCRIPTOR     = 0x0d,
    DT_MAXIMUM_BITRATE          = 0x0e,
    DT_PRIVATE_DATA_INDICATOR   = 0x0f,
    DT_SMOOTHING_BUFFER         = 0x10,
    DT_STD                      = 0x11,     // System Target Decoder ?
    DT_IBP                      = 0x12,
    DT_NETWORK_NAME             = 0x40,
    DT_SERVICE_LIST             = 0x41,
    DT_VBI_DATA                 = 0x45,
    DT_SERVICE                  = 0x48,
    DT_LINKAGE                  = 0x4a,
    DT_SHORT_EVENT              = 0x4d,
    DT_EXTENDED_EVENT           = 0x4e,
    DT_COMPONENT                = 0x50,
    DT_STREAM_IDENTIFIER        = 0x52,
    DT_CONTENT                  = 0x54,
    DT_PARENTAL_RATING          = 0x55,
    DT_TELETEXT                 = 0x56,
    DT_SUBTITLING               = 0x59,
    DT_TERRESTRIAL_DELIV_SYS    = 0x5a,
    DT_PRIVATE_DATA             = 0x5f,
    DID_DATA_BROADCAST          = 0x64,
    DT_DATA_BROADCAST_ID        = 0x66,
    DT_AC3_AUDIO                = 0x6a,     // DVB
    DT_EXTENDED_AC3_AUDIO       = 0x7a,
    DT_AAC_AUDIO                = 0x7c,

    DT_AC3_AUDIO__2             = 0x81,     // DCII or ATSC
    DT_LOGICAL_CHANNEL          = 0x83,
    DT_HD_SIMCAST_LOG_CHANNEL   = 0x88
};

enum MPEG_TYPES {
    mpeg_us,
    mpeg_ps,
    mpeg_ts,
    mpeg_es,
    mpeg_pva
};

enum SERVICE_TYPE {
    DIGITAL_TV          = 0x01,
    DIGITAL_RADIO       = 0x02,
    AVC_DIGITAL_RADIO   = 0x0A,
    MPEG2_HD_DIGITAL_TV = 0x11,
    AVC_SD_TV           = 0x16,
    AVC_HD_TV           = 0x19,
    HEVC_TV             = 0x1F
};

enum ATSC_SERVICE_TYPE {
    ATSC_ANALOG_TV  = 0x01,
    ATSC_DIGITAL_TV = 0x02,
    ATSC_AUDIO      = 0x03
};

extern const wchar_t* StreamTypeToName(PES_STREAM_TYPE _Type);
