/* ------------------------------------------------------------------
 * Copyright (C) 1998-2009 PacketVideo
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either
 * express or implied.
 * See the License for the specific language governing permissions
 * and limitations under the License.
 * -------------------------------------------------------------------
 */
//////////////////////////////////////////////////////////////////////////////////
//                                                                              //
//  File: pvgsmamrdecoderinterface.h                                            //
//                                                                              //
//////////////////////////////////////////////////////////////////////////////////

#ifndef _PVGSMAMR_DECODER_INTERFACE_H
#define _PVGSMAMR_DECODER_INTERFACE_H

#include "oscl_base.h"

/*----------------------------------------------------------------------------
; ENUMERATED TYPEDEF'S
----------------------------------------------------------------------------*/

typedef enum
{
    /*
     *    One word (2-byte) to indicate type of frame type.
     *    One word (2-byte) to indicate frame type.
     *    One word (2-byte) to indicate mode.
     *    N words (2-byte) containing N bits (bit 0 = 0xff81, bit 1 = 0x007f).
     */
    ETS = 0, /* Both AMR-Narrowband and AMR-Wideband */

    /*
     *    One word (2-byte) for sync word (good frames: 0x6b21, bad frames: 0x6b20)
     *    One word (2-byte) for frame length N.
     *    N words (2-byte) containing N bits (bit 0 = 0x007f, bit 1 = 0x0081).
     */
    ITU, /* AMR-Wideband */

    /*
     *   AMR-WB MIME/storage format, see RFC 3267 (sections 5.1 and 5.3) for details
     */
    MIME_IETF,

    WMF, /* AMR-Narrowband */

    IF2  /* AMR-Narrowband */

} bitstream_format;



/*----------------------------------------------------------------------------
; STRUCTURES TYPEDEF'S
----------------------------------------------------------------------------*/
typedef struct
{
    int16 prev_ft;
    int16 prev_mode;
} RX_State;


typedef struct tPVAmrDecoderExternal
{
    /*
     * INPUT:
     * Pointer to the input buffer that contains the encoded bistream data.
     * The data is filled in such that the first bit transmitted is
     * the most-significant bit (MSB) of the first array element.
     * The buffer is accessed in a linear fashion for speed, and the number of
     * bytes consumed varies frame to frame. This is use for mime/ietf data
     */
    uint8  *pInputBuffer;

    /*
     * INPUT:
     * Pointer to the input buffer that contains the encoded stream data.
     * The data is filled such that the first bit transmitted is
     * in the  first int16 element.
     * The buffer is accessed in a linear fashion for speed, and the number of
     * bytes consumed varies frame to frame.
     */
    int16  *pInputSampleBuffer;

    /*
     * INPUT: (but what is pointed to is an output)
     * Pointer to the output buffer to hold the 16-bit PCM audio samples.
     */
    int16  *pOutputBuffer;

    /*
     * INPUT:
     * Number of requested output audio channels. This relieves the calling
     * environment from having to perform stereo-to-mono or mono-to-stereo
     * conversions.
     */
    int32     desiredChannels;

    /*
         * INPUT:
         * Format type of the encoded bitstream.
         */
    bitstream_format     input_format;

    /*
     * OUTPUT:
     * The sampling rate decoded from the bitstream, in units of
     * samples/second. For this release of the library this value does
     * not change from frame to frame, but future versions will.
     */
    int32   samplingRate;

    /*
     * OUTPUT:
     * This value is the bitrate in units of bits/second. IT
     * is calculated using the number of bits consumed for the current frame,
     * and then multiplying by the sampling_rate, divided by points in a frame.
     * This value can changes frame to frame.
     */
    int32   bitRate;

    /*
     * OUTPUT:
     * The number of channels decoded from the bitstream. The output data
     * will have be the amount specified in the variable desiredChannels,
     * this output is informative only, and can be ignored.
     */
    int32     encodedChannels;

    /*
     * OUTPUT:
     * This value is the number of output PCM samples per channel.
     * It is  320.
     */
    int16     frameLength;

    /*
     * OUTPUT:
     * This value is the quality indicator. 1 (good)  0 (bad)
    */
    uint8     quality;


    /*
     * OUTPUT:
     *  GSM AMR NB and WB mode (i.e. bit-rate )
     */
    int16     mode;
    int16     mode_old;

    /*
     * OUTPUT:
     *  GSM AMR NB and WB frame type ( speech_good, speech_bad, sid, etc.)
     */
    int16     frame_type;

    int16 reset_flag;
    int16 reset_flag_old;

    /*
     * OUTPUT:
     *  Decoder  status
     */
    int32     status;

    /*
     * OUTPUT:
     *  Rx status state
     */
    RX_State  rx_state;

} tPVAmrDecoderExternal;

// CDecoder_AMRInterface

#ifdef __cplusplus

class CDecoder_AMRInterface
{
    public:
        virtual ~CDecoder_AMRInterface() {};
        OSCL_IMPORT_REF virtual int32 StartL(tPVAmrDecoderExternal * pExt,
                                             bool aAllocateInputBuffer  = false,
                                             bool aAllocateOutputBuffer = false) = 0;

        OSCL_IMPORT_REF virtual int32 ExecuteL(tPVAmrDecoderExternal * pExt) = 0;

        OSCL_IMPORT_REF virtual int32 ResetDecoderL() = 0;
        OSCL_IMPORT_REF virtual void StopL() = 0;
        OSCL_IMPORT_REF virtual void TerminateDecoderL() = 0;
};
#endif


#endif

