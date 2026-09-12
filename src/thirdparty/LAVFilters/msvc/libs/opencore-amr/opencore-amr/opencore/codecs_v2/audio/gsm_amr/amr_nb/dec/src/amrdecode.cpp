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
/****************************************************************************************
Portions of this file are derived from the following 3GPP standard:

    3GPP TS 26.073
    ANSI-C code for the Adaptive Multi-Rate (AMR) speech codec
    Available from http://www.3gpp.org

(C) 2004, 3GPP Organizational Partners (ARIB, ATIS, CCSA, ETSI, TTA, TTC)
Permission to distribute, modify and use this file under the standard license
terms listed above has been obtained from the copyright holder.
****************************************************************************************/
/*
------------------------------------------------------------------------------


 Filename: amrdecode.cpp

------------------------------------------------------------------------------
*/

/*----------------------------------------------------------------------------
; INCLUDES
----------------------------------------------------------------------------*/
#include "amrdecode.h"
#include "cnst.h"
#include "typedef.h"
#include "frame.h"
#include "sp_dec.h"
#include "wmf_to_ets.h"
#include "if2_to_ets.h"
#include "frame_type_3gpp.h"

/*----------------------------------------------------------------------------
; MACROS
; Define module specific macros here
----------------------------------------------------------------------------*/


/*----------------------------------------------------------------------------
; DEFINES
; Include all pre-processor statements here. Include conditional
; compile variables also.
----------------------------------------------------------------------------*/


/*----------------------------------------------------------------------------
; LOCAL FUNCTION DEFINITIONS
; Function Prototype declaration
----------------------------------------------------------------------------*/

/*----------------------------------------------------------------------------
; LOCAL STORE/BUFFER/POINTER DEFINITIONS
; Variable declaration - defined here and used outside this module
----------------------------------------------------------------------------*/

/*
------------------------------------------------------------------------------
 FUNCTION NAME: AMRDecode
------------------------------------------------------------------------------
 INPUT AND OUTPUT DEFINITIONS

 Inputs:
    state_data      = pointer to a structure (type void)

    frame_type      = 3GPP frame type (enum Frame_Type_3GPP)

    speech_bits_ptr = pointer to the beginning of the raw encoded speech bits
                      for the current frame to be decoded (unsigned char)

    raw_pcm_buffer  = pointer to the output pcm outputs array (Word16)

    input_format    = input format used; valid values are AMR_WMF, AMR_IF2,
                      and AMR_ETS (Word16)

 Outputs:
    raw_pcm_buffer contains the newly decoded linear PCM speech samples
    state_data->prev_mode contains the new mode

 Returns:
    byte_offset     = address offset of the next frame to be processed or
                      error condition flag (-1) (int)

 Global Variables Used:
    WmfDecBytesPerFrame = table containing the number of core AMR data bytes
                          used by each codec mode for WMF input format (const
                          int)

    If2DecBytesPerFrame = table containing the number of core AMR data bytes
                          used by each codec mode for IF2 input format (const
                          int)

 Local Variables Needed:
    None

------------------------------------------------------------------------------
 FUNCTION DESCRIPTION

 This function is the top level entry function to the GSM AMR Decoder library.

 First, it checks the input format type (input_format) to determine the type
 of de-formattting that needs to be done. If input_format is AMR_WMF, the input
 data is in WMF (aka, non-IF2) format and the function wmf_to_ets will be
 called to convert to the ETS format (1 bit/word, where 1 word = 16 bits),
 and byte_offset will be updated according to the contents of WmfDecBytesPerFrame
 table.

 If input_format is AMR_IF2, the input data is in IF2 format [1] and the
 function if2_to_ets will be called to convert to the ETS format, and
 byte_offset will be updated according to the contents of If2DecBytesPerFrame
 table.

 The codec mode and receive frame type is initialized based on the incoming
 frame_type.

 If input_format is AMR_ETS, the input data is in the ETS format. The receive
 frame type is set to the value in the first location of the buffer pointed to
 by speech_bits_ptr. Then, the encoded speech parameters in the buffer pointed
 to by speech_bits is copied to dec_ets_input_bfr and the type will be changed
 from unsigned char to Word16. Lastly, if the receive frame type is not
 RX_NO_DATA, the mode is obtained from the buffer pointed to by
 speech_bits_ptr, offset by MAX_SERIAL_SIZE+1, otherwise, the mode is set to
 the previous mode (found the in state_data->prev_mode).

 If input_format is an unsupported format, byte_offset will be set to -1, to
 indicate an error condition has occurred, and the function will exit.

 If there are no errors, GSMFrameDecode is called to decode a 20 ms frame. It
 puts the decoded linear PCM samples in the buffer pointed to by
 raw_pcm_buffer. Then, the prev_mode field of the structure pointed to by
 state_data is updated to the current mode.

 This function returns the new byte_offset value to indicate the address
 offset of the next speech frame to be decoded.

------------------------------------------------------------------------------
 REQUIREMENTS

 None

------------------------------------------------------------------------------
 REFERENCES

 [1] "AMR Speech Codec Frame Structure", 3GPP TS 26.101 version 4.1.0
     Release 4, June 2001

------------------------------------------------------------------------------
 PSEUDO-CODE

 Note: AMRSID_RXTYPE_BIT_OFFSET = 35
       AMRSID_RXMODE_BIT_OFFSET = 36
       NUM_AMRSID_RXMODE_BITS = 3


 // Set up Decoder state structure pointer
 Speech_Decode_FrameState *decoder_state
            = (Speech_Decode_FrameState *) state_data

 // Determine type of de-formatting

 // Decode WMF or IF2 frames
 IF ((input_format == AMR_RX_WMF) | (input_format == AMR_RX_IF2))
 THEN
     IF (input_format == AMR_RX_WMF)
     THEN
         // Convert incoming packetized raw WMF data to ETS format
         CALL wmf_to_ets(frame_type = frame_type
                         input_ptr = speech_bits_ptr
                         output_ptr = &dec_ets_input_bfr)
           MODIFYING(nothing)
           RETURNING(nothing)

         // Address offset of the start of next frame
         byte_offset = WmfDecBytesPerFrame[frame_type]

     ELSEIF (input_format == AMR_RX_IF2)
     THEN
         // Convert incoming packetized raw IF2 data to ETS format
         CALL if2_to_ets(frame_type = frame_type
                         input_ptr = speech_bits_ptr
                         output_ptr = &dec_ets_input_bfr)
           MODIFYING(nothing)
           RETURNING(nothing)

         // Address offset of the start of next frame
         byte_offset = If2DecBytesPerFrame[frame_type]

     ENDIF

       // Determine AMR codec mode and AMR RX frame type
     IF (frame_type <= AMR_122)
     THEN
         mode = (enum Mode) frame_type;
         rx_type = RX_SPEECH_GOOD;

     ELSEIF (frame_type == AMR_SID)
     THEN
         // Clear mode store prior to reading mode info from input buffer
         mode = 0

         FOR i = 0 TO NUM_AMRSID_RXMODE_BITS-1

             mode |= (dec_ets_input_bfr[AMRSID_RXMODE_BIT_OFFSET+i] << i)

         ENDFOR

         IF (dec_ets_input_bfr[AMRSID_RXTYPE_BIT_OFFSET] == 0)
         THEN
             rx_type = RX_SID_FIRST

         ELSE
             rx_type = RX_SID_UPDATE

         ENDIF

     ELSEIF ((frame_type > AMR_SID) && (frame_type < NO_DATA))
     THEN
         // Use previous mode
         mode = decoder_state->prev_mode

         // Unsupported SID frames
         rx_type = RX_SPEECH_BAD;

     ELSE
         // Use previous mode
         mode = decoder_state->prev_mode

         // No data received
         rx_type = RX_NO_DATA;

     ENDIF

 // Decode ETS frames
 ELSEIF (input_format == AMR_RX_ETS)
 THEN
     // Change type of pointer to incoming raw ETS data
     ets_word_ptr = (Word16 *) speech_bits_ptr

     // Get RX frame type
     rx_type = (enum RXFrameType) *ets_word_ptr
     ets_word_ptr = ets_word_ptr + 1

     // Copy incoming raw ETS data to dec_ets_input_bfr
     FOR i = 0 TO MAX_SERIAL_SIZE-1

         dec_ets_input_bfr[i] = *ets_word_ptr
         ets_word_ptr = ets_word_ptr + 1

     ENDFOR

     // Get codec mode
     IF (rx_type != RX_NO_DATA)
     THEN
         mode = (enum Mode) *ets_word_ptr

     ELSE
         //Use previous mode if no received data
         mode = decoder_state->prev_mode

     ENDIF

     // Set up byte_offset
     byte_offset = 2*(MAX_SERIAL_SIZE+2)

 ELSE
     // Invalid format, return error code
     byte_offset = -1

 ENDIF

 // Proceed with decoding frame, if there are no errors
 IF (byte_offset != -1)
 THEN
     // Decode a 20 ms frame
     CALL GSMFrameDecode( st = decoder_state
                          mode = mode
                          serial = dec_ets_input_bfr,
                          frame_type = rx_type,
                          synth = (Word16 *)raw_pcm_buffer);
       MODIFYING (nothing)
       RETURNING (Nothing)

     // Save mode for next frame
     decoder_state->prev_mode = mode

 ENDIF

 RETURN (byte_offset)

------------------------------------------------------------------------------
 CAUTION [optional]
 [State any special notes, constraints or cautions for users of this function]

------------------------------------------------------------------------------
*/

Word16 AMRDecode(
    void                      *state_data,
    enum Frame_Type_3GPP      frame_type,
    UWord8                    *speech_bits_ptr,
    Word16                    *raw_pcm_buffer,
    bitstream_format          input_format
)
{
    Word16 *ets_word_ptr;
    enum Mode mode = (enum Mode)MR475;
    int modeStore;
    int tempInt;
    enum RXFrameType rx_type = RX_NO_DATA;
    Word16 dec_ets_input_bfr[MAX_SERIAL_SIZE];
    Word16 i;
    Word16 byte_offset = -1;

    /* Type cast state_data to Speech_Decode_FrameState rather than passing
     * that structure type to this function so the structure make up can't
     * be viewed from higher level functions than this.
     */
    Speech_Decode_FrameState *decoder_state
    = (Speech_Decode_FrameState *) state_data;

    /* Determine type of de-formatting */
    /* WMF or IF2 frames */
    if ((input_format == MIME_IETF) | (input_format == IF2))
    {
        if (input_format == MIME_IETF)
        {
            /* Convert incoming packetized raw WMF data to ETS format */
            wmf_to_ets(frame_type, speech_bits_ptr, dec_ets_input_bfr, &(decoder_state->decoder_amrState.common_amr_tbls));

            /* Address offset of the start of next frame */
            byte_offset = WmfDecBytesPerFrame[frame_type];
        }
        else   /* else has to be input_format  IF2 */
        {
            /* Convert incoming packetized raw IF2 data to ETS format */
            if2_to_ets(frame_type, speech_bits_ptr, dec_ets_input_bfr, &(decoder_state->decoder_amrState.common_amr_tbls));

            /* Address offset of the start of next frame */
            byte_offset = If2DecBytesPerFrame[frame_type];
        }

        /* At this point, input data is in ETS format     */
        /* Determine AMR codec mode and AMR RX frame type */
        if (frame_type <= AMR_122)
        {
            mode = (enum Mode) frame_type;
            rx_type = RX_SPEECH_GOOD;
        }
        else if (frame_type == AMR_SID)
        {
            /* Clear mode store prior to reading mode info from input buffer */
            modeStore = 0;

            for (i = 0; i < NUM_AMRSID_RXMODE_BITS; i++)
            {
                tempInt = dec_ets_input_bfr[AMRSID_RXMODE_BIT_OFFSET+i] << i;
                modeStore |= tempInt;
            }
            mode = (enum Mode) modeStore;

            /* Get RX frame type */
            if (dec_ets_input_bfr[AMRSID_RXTYPE_BIT_OFFSET] == 0)
            {
                rx_type = RX_SID_FIRST;
            }
            else
            {
                rx_type = RX_SID_UPDATE;
            }
        }
        else if (frame_type < AMR_NO_DATA)
        {
            /* Invalid frame_type, return error code */
            byte_offset = -1;   /*  !!! */
        }
        else
        {
            mode = decoder_state->prev_mode;

            /*
             * RX_NO_DATA, generate exponential decay from latest valid frame for the first 6 frames
             * after that, create silent frames
             */
            rx_type = RX_NO_DATA;

        }

    }

    /* ETS frames */
    else if (input_format == ETS)
    {
        /* Change type of pointer to incoming raw ETS data */
        ets_word_ptr = (Word16 *) speech_bits_ptr;

        /* Get RX frame type */
        rx_type = (enum RXFrameType) * ets_word_ptr;
        ets_word_ptr++;

        /* Copy incoming raw ETS data to dec_ets_input_bfr */
        for (i = 0; i < MAX_SERIAL_SIZE; i++)
        {
            dec_ets_input_bfr[i] = *ets_word_ptr;
            ets_word_ptr++;
        }

        /* Get codec mode */
        if (rx_type != RX_NO_DATA)
        {
            /* Get mode from input bitstream */
            mode = (enum Mode) * ets_word_ptr;
        }
        else
        {
            /* Use previous mode if no received data */
            mode = decoder_state->prev_mode;
        }

        /* Set up byte_offset */
        byte_offset = 2 * (MAX_SERIAL_SIZE + 2);
    }
    else
    {
        /* Invalid input format, return error code */
        byte_offset = -1;
    }

    /* Proceed with decoding frame, if there are no errors */
    if (byte_offset != -1)
    {
        /* Decode a 20 ms frame */

#ifndef CONSOLE_DECODER_REF
        /* Use PV version of sp_dec.c */
        GSMFrameDecode(decoder_state, mode, dec_ets_input_bfr, rx_type,
                       raw_pcm_buffer);

#else
        /* Use ETS version of sp_dec.c */
        Speech_Decode_Frame(decoder_state, mode, dec_ets_input_bfr, rx_type,
                            raw_pcm_buffer);

#endif

        /* Save mode for next frame */
        decoder_state->prev_mode = mode;
    }

    return (byte_offset);
}



