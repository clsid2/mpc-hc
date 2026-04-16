/*
 * (C) 2010-2015, 2017 see Authors.txt
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

#include "InternalFiltersConfig.h"


enum {
    SOURCE_FILTER,
    AUDIO_DECODER,
    VIDEO_DECODER,
    FILTER_TYPE_NB
};

enum SOURCE_FILTER {
#if INTERNAL_SOURCEFILTER_AC3
    SRC_AC3,
#endif
#if INTERNAL_SOURCEFILTER_ASF
    SRC_ASF,
#endif
#if INTERNAL_SOURCEFILTER_AVI
    SRC_AVI,
#endif
#if INTERNAL_SOURCEFILTER_AVS
    SRC_AVS,
#endif
#if INTERNAL_SOURCEFILTER_DTS
    SRC_DTS,
#endif
#if INTERNAL_SOURCEFILTER_FLAC
    SRC_FLAC,
#endif
#if INTERNAL_SOURCEFILTER_FLIC
    SRC_FLIC,
#endif
#if INTERNAL_SOURCEFILTER_FLV
    SRC_FLV,
#endif
#if INTERNAL_SOURCEFILTER_GIF
    SRC_GIF,
#endif
#if INTERNAL_SOURCEFILTER_HTTP
    SRC_HTTP,
#endif
#if INTERNAL_SOURCEFILTER_MATROSKA
    SRC_MATROSKA,
#endif
#if INTERNAL_SOURCEFILTER_MISC
    SRC_MISC,
#endif
#if INTERNAL_SOURCEFILTER_MMS
    SRC_MMS,
#endif
#if INTERNAL_SOURCEFILTER_MP4
    SRC_MP4,
#endif
#if INTERNAL_SOURCEFILTER_MPEGAUDIO
    SRC_MPA,
#endif
#if INTERNAL_SOURCEFILTER_MPEG
    SRC_MPEG,
    SRC_MPEGTS,
#endif
#if INTERNAL_SOURCEFILTER_OGG
    SRC_OGG,
#endif
#if INTERNAL_SOURCEFILTER_REALMEDIA
    SRC_REALMEDIA,
#endif
#if INTERNAL_SOURCEFILTER_RTMP
    SRC_RTMP,
#endif
#if INTERNAL_SOURCEFILTER_RTP
    SRC_RTP,
#endif
#if INTERNAL_SOURCEFILTER_RTSP
    SRC_RTSP,
#endif
#if INTERNAL_SOURCEFILTER_UDP
    SRC_UDP,
#endif
#if INTERNAL_SOURCEFILTER_WTV
    SRC_WTV,
#endif
#if INTERNAL_SOURCEFILTER_APE
    SRC_APE,
#endif
#if INTERNAL_SOURCEFILTER_CDDA
    SRC_CDDA,
#endif
#if INTERNAL_SOURCEFILTER_CDXA
    SRC_CDXA,
#endif
#if INTERNAL_SOURCEFILTER_DSM
    SRC_DSM,
#endif
#if INTERNAL_SOURCEFILTER_RFS
    SRC_RFS,
#endif
#if INTERNAL_SOURCEFILTER_VTS
    SRC_VTS,
#endif
    SRC_LAST
};

enum DECODER {
#if INTERNAL_DECODER_MPEG1
    TRA_MPEG1,
#endif
#if INTERNAL_DECODER_MPEG2
    TRA_MPEG2,
#endif
#if INTERNAL_DECODER_REALVIDEO
    TRA_RV,
#endif
#if INTERNAL_DECODER_REALAUDIO
    TRA_RA,
#endif
#if INTERNAL_DECODER_MPEGAUDIO
    TRA_MPA,
#endif
#if INTERNAL_DECODER_DTS
    TRA_DTS,
#endif
#if INTERNAL_DECODER_LPCM
    TRA_LPCM,
#endif
#if INTERNAL_DECODER_AC3
    TRA_AC3,
#endif
#if INTERNAL_DECODER_AAC
    TRA_AAC,
#endif
#if INTERNAL_DECODER_ALAC
    TRA_ALAC,
#endif
#if INTERNAL_DECODER_ALS
    TRA_ALS,
#endif
#if INTERNAL_DECODER_PS2AUDIO
    TRA_PS2AUD,
#endif
#if INTERNAL_DECODER_VORBIS
    TRA_VORBIS,
#endif
#if INTERNAL_DECODER_FLAC
    TRA_FLAC,
#endif
#if INTERNAL_DECODER_NELLYMOSER
    TRA_NELLY,
#endif
#if INTERNAL_DECODER_AMR
    TRA_AMR,
#endif
#if INTERNAL_DECODER_OPUS
    TRA_OPUS,
#endif
#if INTERNAL_DECODER_WMA
    TRA_WMA,
#endif
#if INTERNAL_DECODER_WMAPRO
    TRA_WMAPRO,
#endif
#if INTERNAL_DECODER_WMALL
    TRA_WMALL,
#endif
#if INTERNAL_DECODER_G726
    TRA_G726,
#endif
#if INTERNAL_DECODER_G729
    TRA_G729,
#endif
#if INTERNAL_DECODER_AC4
    TRA_AC4,
#endif
#if INTERNAL_DECODER_OTHERAUDIO
    TRA_OTHERAUDIO,
#endif
#if INTERNAL_DECODER_PCM
    TRA_PCM,
#endif
#if INTERNAL_DECODER_H264
    TRA_H264,
#endif
#if INTERNAL_DECODER_HEVC
    TRA_HEVC,
#endif
#if INTERNAL_DECODER_VVC
    TRA_VVC,
#endif
#if INTERNAL_DECODER_AV1
    TRA_AV1,
#endif
#if INTERNAL_DECODER_VC1
    TRA_VC1,
#endif
#if INTERNAL_DECODER_FLV
    TRA_FLV4,
#endif
#if INTERNAL_DECODER_VP356
    TRA_VP356,
#endif
#if INTERNAL_DECODER_VP8
    TRA_VP8,
#endif
#if INTERNAL_DECODER_VP9
    TRA_VP9,
#endif
#if INTERNAL_DECODER_XVID
    TRA_XVID,
#endif
#if INTERNAL_DECODER_DIVX
    TRA_DIVX,
#endif
#if INTERNAL_DECODER_MSMPEG4
    TRA_MSMPEG4,
#endif
#if INTERNAL_DECODER_WMV
    TRA_WMV,
#endif
#if INTERNAL_DECODER_SVQ
    TRA_SVQ3,
#endif
#if INTERNAL_DECODER_H263
    TRA_H263,
#endif
#if INTERNAL_DECODER_THEORA
    TRA_THEORA,
#endif
#if INTERNAL_DECODER_AMVV
    TRA_AMVV,
#endif
#if INTERNAL_DECODER_MJPEG
    TRA_MJPEG,
#endif
#if INTERNAL_DECODER_INDEO
    TRA_INDEO,
#endif
#if INTERNAL_DECODER_SCREEN
    TRA_SCREEN,
#endif
#if INTERNAL_DECODER_FLIC
    TRA_FLIC,
#endif
#if INTERNAL_DECODER_MSVIDEO
    TRA_MSVIDEO,
#endif
#if INTERNAL_DECODER_V210_V410
    TRA_V210_V410,
#endif
#if INTERNAL_DECODER_PRORES
    TRA_PRORES,
#endif
#if INTERNAL_DECODER_DNXHD
    TRA_DNXHD,
#endif
#if INTERNAL_DECODER_CFHD
    TRA_CFHD,
#endif
#if INTERNAL_DECODER_OTHERVIDEO
    TRA_OTHERVIDEO,
#endif
    TRA_LAST
};
