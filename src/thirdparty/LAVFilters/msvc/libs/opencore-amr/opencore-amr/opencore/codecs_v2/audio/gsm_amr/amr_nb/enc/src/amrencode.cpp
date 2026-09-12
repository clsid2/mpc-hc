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



 Filename: amrencode.cpp
 Functions: AMREncode
            AMREncodeInit
            AMREncodeReset
            AMREncodeExit

------------------------------------------------------------------------------
 MODULE DESCRIPTION

 This file contains the functions required to initialize, reset, exit, and
 invoke the ETS 3GPP GSM AMR encoder.

------------------------------------------------------------------------------
*/


/*----------------------------------------------------------------------------
; INCLUDES
----------------------------------------------------------------------------*/
#include "cnst.h"
#include "mode.h"
#include "frame_type_3gpp.h"
#include "typedef.h"

#include "amrencode.h"
#include "ets_to_if2.h"
#include "ets_to_wmf.h"
#include "sid_sync.h"
#include "sp_enc.h"

/*----------------------------------------------------------------------------
; MACROS [optional]
; [Define module specific macros here]
----------------------------------------------------------------------------*/

/*----------------------------------------------------------------------------
; DEFINES [optional]
; [Include all pre-processor statements here. Include conditional
; compile variables also.]
----------------------------------------------------------------------------*/

/*----------------------------------------------------------------------------
; LOCAL FUNCTION DEFINITIONS
; [List function prototypes here]
----------------------------------------------------------------------------*/

/*----------------------------------------------------------------------------
; LOCAL VARIABLE DEFINITIONS
; [Variable declaration - defined here and used outside this module]
----------------------------------------------------------------------------*/


/*
------------------------------------------------------------------------------
 FUNCTION NAME: AMREncodeInit
------------------------------------------------------------------------------
 INPUT AND OUTPUT DEFINITIONS

 Inputs:
    pEncStructure = pointer containing the pointer to a structure used by
                    the encoder (void)
    pSidSyncStructure = pointer containing the pointer to a structure used for
                        SID synchronization (void)
    dtx_enable = flag to turn off or turn on DTX (Flag)

 Outputs:
    None

 Returns:
    init_status = 0, if initialization was successful; -1, otherwise (int)

 Global Variables Used:
    None

 Local Variables Needed:
    speech_encoder_state = pointer to encoder frame structure
                           (Speech_Encode_FrameState)
    sid_state = pointer to SID sync structure (sid_syncState)

------------------------------------------------------------------------------
 FUNCTION DESCRIPTION

 This function initializes the GSM AMR Encoder library by calling
 GSMInitEncode and sid_sync_init. If initialization was successful,
 init_status is set to zero, otherwise, it is set to -1.

------------------------------------------------------------------------------
 REQUIREMENTS

 None

------------------------------------------------------------------------------
 REFERENCES

 None

------------------------------------------------------------------------------
 PSEUDO-CODE

 // Initialize GSM AMR Encoder
 CALL GSMInitEncode(state_data = &pEncStructure,
                    dtx = dtx_enable,
                    id = char_id            )
   MODIFYING(nothing)
   RETURNING(return_value = enc_init_status)

 // Initialize SID synchronization
 CALL sid_sync_init(state = &pSidSyncStructure)
   MODIFYING(nothing)
   RETURNING(return_value = sid_sync_init_status)

 IF ((enc_init_status != 0) || (sid_sync_init != 0))
 THEN
     init_status = -1

 ENDIF

 MODIFY(nothing)
 RETURN(init_status)

------------------------------------------------------------------------------
 CAUTION [optional]
 [State any special notes, constraints or cautions for users of this function]

------------------------------------------------------------------------------
*/
Word16 AMREncodeInit(
    void **pEncStructure,
    void **pSidSyncStructure,
    Flag dtx_enable)
{
    Word16 enc_init_status = 0;
    Word16 sid_sync_init_status = 0;
    Word16 init_status = 0;

    /* Initialize GSM AMR Encoder */
#ifdef CONSOLE_ENCODER_REF
    /* Change to original ETS input types */
    Speech_Encode_FrameState **speech_encode_frame =
        (Speech_Encode_FrameState **)(pEncStructure);

    sid_syncState **sid_sync_state = (sid_syncState **)(pSidSyncStructure);

    /* Use ETS version of sp_enc.c */
    enc_init_status = Speech_Encode_Frame_init(speech_encode_frame,
                      dtx_enable,
                      (Word8*)"encoder");

    /* Initialize SID synchronization */
    sid_sync_init_status = sid_sync_init(sid_sync_state);

#else
    /* Use PV version of sp_enc.c */
    enc_init_status = GSMInitEncode(pEncStructure,
                                    dtx_enable,
                                    (Word8*)"encoder");

    /* Initialize SID synchronization */
    sid_sync_init_status = sid_sync_init(pSidSyncStructure);


#endif

    if ((enc_init_status != 0) || (sid_sync_init_status != 0))
    {
        init_status = -1;
    }

    return(init_status);
}


/****************************************************************************/

/*
------------------------------------------------------------------------------
 FUNCTION NAME: AMREncodeReset
------------------------------------------------------------------------------
 INPUT AND OUTPUT DEFINITIONS

 Inputs:
    pEncStructure = pointer to a structure used by the encoder (void)
    pSidSyncStructure = pointer to a structure used for SID synchronization
                        (void)

 Outputs:
    None

 Returns:
    reset_status = 0, if reset was successful; -1, otherwise (int)

 Global Variables Used:
    None

 Local Variables Needed:
    speech_encoder_state = pointer to encoder frame structure
                           (Speech_Encode_FrameState)
    sid_state = pointer to SID sync structure (sid_syncState)

------------------------------------------------------------------------------
 FUNCTION DESCRIPTION

 This function resets the state memory used by the Encoder and SID sync
 function. If reset was successful, reset_status is set to zero, otherwise,
 it is set to -1.

------------------------------------------------------------------------------
 REQUIREMENTS

 None

------------------------------------------------------------------------------
 REFERENCES

 None

------------------------------------------------------------------------------
 PSEUDO-CODE

 // Reset GSM AMR Encoder
 CALL Speech_Encode_Frame_reset(state_data = pEncStructure)
   MODIFYING(nothing)
   RETURNING(return_value = enc_reset_status)

 // Reset SID synchronization
 CALL sid_sync_reset(state = pSidSyncStructure)
   MODIFYING(nothing)
   RETURNING(return_value = sid_sync_reset_status)

 IF ((enc_reset_status != 0) || (sid_sync_reset_status != 0))
 THEN
     reset_status = -1

 ENDIF

 MODIFY(nothing)
 RETURN(reset_status)

------------------------------------------------------------------------------
 CAUTION [optional]
 [State any special notes, constraints or cautions for users of this function]

------------------------------------------------------------------------------
*/
Word16 AMREncodeReset(
    void *pEncStructure,
    void *pSidSyncStructure)
{
    Word16 enc_reset_status = 0;
    Word16 sid_sync_reset_status = 0;
    Word16 reset_status = 0;

    /* Reset GSM AMR Encoder */
    enc_reset_status = Speech_Encode_Frame_reset(pEncStructure);


    /* Reset SID synchronization */
    sid_sync_reset_status = sid_sync_reset(pSidSyncStructure);

    if ((enc_reset_status != 0) || (sid_sync_reset_status != 0))
    {
        reset_status = -1;
    }

    return(reset_status);
}


/****************************************************************************/

/*
------------------------------------------------------------------------------
 FUNCTION NAME: AMREncodeExit
------------------------------------------------------------------------------
 INPUT AND OUTPUT DEFINITIONS

 Inputs:
    pEncStructure = pointer containing the pointer to a structure used by
                    the encoder (void)
    pSidSyncStructure = pointer containing the pointer to a structure used for
                        SID synchronization (void)

 Outputs:
    None

 Returns:
    None

 Global Variables Used:
    None

 Local Variables Needed:
    speech_encoder_state = pointer to encoder frame structure
                           (Speech_Encode_FrameState)
    sid_state = pointer to SID sync structure (sid_syncState)

------------------------------------------------------------------------------
 FUNCTION DESCRIPTION

 This function frees up the state memory used by the Encoder and SID
 synchronization function.

------------------------------------------------------------------------------
 REQUIREMENTS

 None

------------------------------------------------------------------------------
 REFERENCES

 None

------------------------------------------------------------------------------
 PSEUDO-CODE

 // Exit GSM AMR Encoder
 CALL GSMEncodeFrameExit(state_data = &pEncStructure)
   MODIFYING(nothing)
   RETURNING(nothing)

 // Exit SID synchronization
 CALL sid_sync_exit(state = &pSidSyncStructure)
   MODIFYING(nothing)
   RETURNING(nothing)

 MODIFY(nothing)
 RETURN(nothing)

------------------------------------------------------------------------------
 CAUTION [optional]
 [State any special notes, constraints or cautions for users of this function]

------------------------------------------------------------------------------
*/
void AMREncodeExit(
    void **pEncStructure,
    void **pSidSyncStructure)
{
    /* Exit GSM AMR Encoder */

#ifdef CONSOLE_ENCODER_REF
    /* Change to original ETS input types */
    Speech_Encode_FrameState ** speech_encode_frame =
        (Speech_Encode_FrameState **)(pEncStructure);

    sid_syncState ** sid_sync_state = (sid_syncState **)(pSidSyncStructure);

    /* Use ETS version of sp_enc.c */
    Speech_Encode_Frame_exit(speech_encode_frame);


    /* Exit SID synchronization */
    sid_sync_exit(sid_sync_state);

#else

    /* Use PV version of sp_enc.c */
    GSMEncodeFrameExit(pEncStructure);

    /* Exit SID synchronization */
    sid_sync_exit(pSidSyncStructure);

#endif

    return;
}


/****************************************************************************/

/*
------------------------------------------------------------------------------
 FUNCTION NAME: AMREncode
------------------------------------------------------------------------------
 INPUT AND OUTPUT DEFINITIONS

 Inputs:
    pEncState = pointer to encoder state structure (void)
    pSidSyncState = pointer to SID sync state structure (void)
    mode = codec mode (enum Mode)
    pEncInput = pointer to the input speech samples (Word16)
    pEncOutput = pointer to the encoded bit stream (unsigned char)
    p3gpp_frame_type = pointer to the 3GPP frame type (enum Frame_Type_3GPP)
    output_format = output format type (Word16); valid values are AMR_WMF,
                    AMR_IF2, and AMR_ETS

 Outputs:
    pEncOutput buffer contains to the newly encoded bit stream
    p3gpp_frame_type store contains the new 3GPP frame type

 Returns:
    num_enc_bytes = number of encoded bytes for a particular
                    mode or -1, if an error occurred (int)

 Global Variables Used:
    WmfEncBytesPerFrame = table containing the number of encoder frame
                          data bytes per codec mode for WMF output
                          format (const int)
    If2EncBytesPerFrame = table containing the number of encoder frame
                          data bytes per codec mode for IF2 output
                          format (const int)

 Local Variables Needed:
    None

------------------------------------------------------------------------------
 FUNCTION DESCRIPTION

 This function is the top-level entry point to the GSM AMR Encoder library.

 The following describes the encoding process for WMF or IF2 formatted output
 data. This functions calls GSMEncodeFrame to encode one frame's worth of
 input speech samples, and returns the newly encoded bit stream in the buffer
 pointed to by pEncOutput.Then the function sid_sync is called to determine
 the transmit frame type. If the transmit frame type is TX_SPEECH_GOOD or
 TX_SID_FIRST or TX_SID_UPDATE, p3gpp_frame_type will be set to the encoder
 used mode. For SID frames, the SID type information and mode information are
 added to the encoded parameter bitstream according to the SID frame format
 described in [1]. If the transmit frame type is TX_NO_DATA, the store
 pointed to by p3gpp_frame_type will be set to NO_DATA. Then the output
 format type (output_format) will be checked to determine the format of the
 encoded data.

 If output_format is AMR_TX_WMF, the function ets_to_wmf will be called to
 convert from ETS format (1 bit/word, where 1 word = 16 bits, information in
 least significant bit) to WMF (aka, non-IF2). The WMF format stores the data
 in octets. The least significant 4 bits of the first octet contains the 3GPP
 frame type information and the most significant 4 bits are zeroed out. The
 succeeding octets contain the packed encoded speech bits. The total number of
 WMF bytes encoded is obtained from WmfEncBytesPerFrame table and returned via
 num_enc_bytes.

 If output_format is AMR_TX_IF2, the function if2_to_ets will be called to
 convert from ETS format to IF2 [1]. The IF2 format stores the data in octets.
 The least significant nibble of the first octet contains the 3GPP frame type
 and the most significant nibble contains the first 4 encoded speech bits. The
 suceeding octets contain the packed encoded speech bits. The total number of
 IF2 bytes encoded is obtained from If2EncBytesPerFrame table and returned via
 num_enc_bytes.

 If output_format is AMR_TX_ETS, GSMFrameEncode is called to generate the
 encoded speech parameters, then, sid_sync is called to determine the transmit
 frame type. If the transmit frame type is not TX_NO_DATA, then the transmit
 frame type information is saved in the first location of the ets_output_bfr,
 followed by the encoded speech parameters. The codec mode information is
 stored immediately after the MAX_SERIAL_SIZE encoded speech parameters. If
 the transmit frame type is TX_NO_DATA, the transmit frame type, encoded
 speech parameters, and codec mode are stored in the same order as before
 in ets_output_bfr. However, for the no data case, the codec mode is set to
 -1.

 After all the required information is generated, the 16-bit data generated
 by the Encoder (in ets_output_bfr) is copied to the buffer pointed to by
 pEncOutput in the little endian configuration, i.e., least significant byte,
 followed by most significant byte. The num_enc_bytes is set to
 2*(MAX_SERIAL_SIZE+2).

 If output_format is invalid, this function flags the error and sets
 num_enc_bytes to -1.

------------------------------------------------------------------------------
 REQUIREMENTS

 None

------------------------------------------------------------------------------
 REFERENCES

 [1] "AMR Speech Codec Frame Structure", 3GPP TS 26.101 version 4.1.0
     Release 4, June 2001

------------------------------------------------------------------------------
 PSEUDO-CODE

 IF ((output_format == AMR_TX_WMF) | (output_format == AMR_TX_IF2))
 THEN
     // Encode one speech frame (20 ms)
     CALL GSMEncodeFrame( state_data = pEncState,
                          mode = mode,
                          new_speech = pEncInput,
                          serial = &ets_output_bfr[0],
                          usedMode = &usedMode )
       MODIFYING(nothing)
       RETURNING(return_value = 0)

     // Determine transmit frame type
     CALL sid_sync(st = pSidSyncState,
                   mode = usedMode
                   tx_frame_type = &tx_frame_type)
       MODIFYING(nothing)
       RETURNING(nothing)

     IF (tx_frame_type != TX_NO_DATA)
     THEN
         // There is data to transmit
         *p3gpp_frame_type = (enum Frame_Type_3GPP) usedMode

         // Add SID type and mode info for SID frames
         IF (*p3gpp_frame_type == AMR_SID)
         THEN
             // Add SID type to encoder output buffer
             IF (tx_frame_type == TX_SID_FIRST)
             THEN
                 ets_output_bfr[AMRSID_TXTYPE_BIT_OFFSET] &= 0x7f

             ELSEIF (tx_frame_type == TX_SID_UPDATE )
             THEN
                 ets_output_bfr[AMRSID_TXTYPE_BIT_OFFSET] |= 0x80

             ENDIF

             // Add mode information bits
             FOR i = 0 TO NUM_AMRSID_TXMODE_BITS-1

                 ets_output_bfr[AMRSID_TXMODE_BIT_OFFSET+i] = (mode>>i)&&0x0001

             ENDFOR

         ENDIF

     ELSE
         // There is no data to transmit
         *p3gpp_frame_type = NO_DATA

     ENDIF

     // Determine the output format to use
     IF (output_format == AMR_TX_WMF)
     THEN
         // Change output data format to WMF
         CALL ets_to_wmf( frame_type_3gpp = *p3gpp_frame_type,
                          ets_input_ptr = &ets_output_bfr[0],
                          wmf_output_ptr = pEncOutput         )
           MODIFYING(nothing)
           RETURNING(nothing)

         // Set up the number of encoded WMF bytes
         num_enc_bytes = WmfEncBytesPerFrame[(int) *p3gpp_frame_type]

     ELSEIF (output_format == AMR_TX_IF2)
     THEN
         // Change output data format to IF2
         CALL ets_to_if2( frame_type_3gpp = *p3gpp_frame_type,
                          ets_input_ptr = &ets_output_bfr[0],
                          if2_output_ptr = pEncOutput         )
           MODIFYING(nothing)
           RETURNING(nothing)

         // Set up the number of encoded IF2 bytes
         num_enc_bytes = If2EncBytesPerFrame[(int) *p3gpp_frame_type]

     ENDIF

 ELSEIF (output_format = AMR_TX_ETS)
 THEN
     // Encode one speech frame (20 ms)
     CALL GSMEncodeFrame( state_data = pEncState,
                          mode = mode,
                          new_speech = pEncInput,
                          serial = &ets_output_bfr[1],
                          usedMode = &usedMode )
       MODIFYING(nothing)
       RETURNING(return_value = 0)

     // Save used mode
     *p3gpp_frame_type = (enum Frame_Type_3GPP) usedMode

     // Determine transmit frame type
     CALL sid_sync(st = pSidSyncState,
                   mode = usedMode
                   tx_frame_type = &tx_frame_type)
       MODIFYING(nothing)
       RETURNING(nothing)

     // Put TX frame type in output buffer
     ets_output_bfr[0] = tx_frame_type

     // Put mode information after the encoded speech parameters
     IF (tx_frame_type != TX_NO_DATA)
     THEN
         ets_output_bfr[MAX_SERIAL_SIZE+1] = mode

     ELSE
         ets_output_bfr[MAX_SERIAL_SIZE+1] = -1

     ENDIF

     // Copy output of encoder to pEncOutput buffer
     ets_output_ptr = (unsigned char *) &ets_output_bfr[0]

     // Copy 16-bit data in 8-bit chunks using Little Endian configuration
     FOR i = 0 TO (2*(MAX_SERIAL_SIZE+6))-1

         *(pEncOutput+i) = *ets_output_ptr
         ets_output_ptr = ets_output_ptr + 1

     ENDFOR

     // Set up number of encoded bytes
     num_enc_bytes = 2*(MAX_SERIAL_SIZE+6)

 ELSE
     // Invalid output_format, set up error code
     num_enc_bytes = -1

 ENDIF

 MODIFY (nothing)
 RETURN (num_enc_bytes)

------------------------------------------------------------------------------
 CAUTION [optional]
 [State any special notes, constraints or cautions for users of this function]

------------------------------------------------------------------------------
*/
Word16 AMREncode(
    void *pEncState,
    void *pSidSyncState,
    enum Mode mode,
    Word16 *pEncInput,
    UWord8 *pEncOutput,
    enum Frame_Type_3GPP *p3gpp_frame_type,
    Word16 output_format
)
{
    Word16 ets_output_bfr[MAX_SERIAL_SIZE+2];
    UWord8 *ets_output_ptr;
    Word16 num_enc_bytes = -1;
    Word16 i;
    enum TXFrameType tx_frame_type;
    enum Mode usedMode = MR475;

    /* Encode WMF or IF2 frames */
    if ((output_format == AMR_TX_WMF) | (output_format == AMR_TX_IF2)
            | (output_format == AMR_TX_IETF))
    {
        /* Encode one speech frame (20 ms) */

#ifndef CONSOLE_ENCODER_REF

        /* Use PV version of sp_enc.c */
        GSMEncodeFrame(pEncState, mode, pEncInput, ets_output_bfr, &usedMode);

#else
        /* Use ETS version of sp_enc.c */
        Speech_Encode_Frame(pEncState, mode, pEncInput, ets_output_bfr, &usedMode);

#endif

        /* Determine transmit frame type */
        sid_sync(pSidSyncState, usedMode, &tx_frame_type);

        if (tx_frame_type != TX_NO_DATA)
        {
            /* There is data to transmit */
            *p3gpp_frame_type = (enum Frame_Type_3GPP) usedMode;

            /* Add SID type and mode info for SID frames */
            if (*p3gpp_frame_type == AMR_SID)
            {
                /* Add SID type to encoder output buffer */
                if (tx_frame_type == TX_SID_FIRST)
                {
                    ets_output_bfr[AMRSID_TXTYPE_BIT_OFFSET] &= 0x0000;
                }
                else if (tx_frame_type == TX_SID_UPDATE)
                {
                    ets_output_bfr[AMRSID_TXTYPE_BIT_OFFSET] |= 0x0001;
                }

                /* Add mode information bits */
                for (i = 0; i < NUM_AMRSID_TXMODE_BITS; i++)
                {
                    ets_output_bfr[AMRSID_TXMODE_BIT_OFFSET+i] =
                        (mode >> i) & 0x0001;
                }
            }
        }
        else
        {
            /* This is no data to transmit */
            *p3gpp_frame_type = (enum Frame_Type_3GPP)AMR_NO_DATA;
        }

        /* At this point, output format is ETS */
        /* Determine the output format to use */
        if (output_format == AMR_TX_IETF)
        {
            /* Change output data format to WMF */
            ets_to_ietf(*p3gpp_frame_type, ets_output_bfr, pEncOutput, &(((Speech_Encode_FrameState*)pEncState)->cod_amr_state->common_amr_tbls));

            /* Set up the number of encoded WMF bytes */
            num_enc_bytes = WmfEncBytesPerFrame[(Word16) *p3gpp_frame_type];

        }
        else if (output_format == AMR_TX_WMF)
        {
            /* Change output data format to WMF */
            ets_to_wmf(*p3gpp_frame_type, ets_output_bfr, pEncOutput, &(((Speech_Encode_FrameState*)pEncState)->cod_amr_state->common_amr_tbls));

            /* Set up the number of encoded WMF bytes */
            num_enc_bytes = WmfEncBytesPerFrame[(Word16) *p3gpp_frame_type];

        }
        else if (output_format == AMR_TX_IF2)
        {
            /* Change output data format to IF2 */
            ets_to_if2(*p3gpp_frame_type, ets_output_bfr, pEncOutput, &(((Speech_Encode_FrameState*)pEncState)->cod_amr_state->common_amr_tbls));

            /* Set up the number of encoded IF2 bytes */
            num_enc_bytes = If2EncBytesPerFrame[(Word16) *p3gpp_frame_type];

        }
    }

    /* Encode ETS frames */
    else if (output_format == AMR_TX_ETS)
    {
        /* Encode one speech frame (20 ms) */

#ifndef CONSOLE_ENCODER_REF

        /* Use PV version of sp_enc.c */
        GSMEncodeFrame(pEncState, mode, pEncInput, &ets_output_bfr[1], &usedMode);

#else
        /* Use ETS version of sp_enc.c */
        Speech_Encode_Frame(pEncState, mode, pEncInput, &ets_output_bfr[1], &usedMode);

#endif

        /* Save used mode */
        *p3gpp_frame_type = (enum Frame_Type_3GPP) usedMode;

        /* Determine transmit frame type */
        sid_sync(pSidSyncState, usedMode, &tx_frame_type);

        /* Put TX frame type in output buffer */
        ets_output_bfr[0] = tx_frame_type;

        /* Put mode information after the encoded speech parameters */
        if (tx_frame_type != TX_NO_DATA)
        {
            ets_output_bfr[1+MAX_SERIAL_SIZE] = (Word16) mode;
        }
        else
        {
            ets_output_bfr[1+MAX_SERIAL_SIZE] = -1;
        }

        /* Copy output of encoder to pEncOutput buffer */
        ets_output_ptr = (UWord8 *) & ets_output_bfr[0];

        /* Copy 16-bit data in 8-bit chunks  */
        /* using Little Endian configuration */
        for (i = 0; i < 2*(MAX_SERIAL_SIZE + 2); i++)
        {
            *(pEncOutput + i) = *ets_output_ptr;
            ets_output_ptr += 1;
        }

        /* Set up the number of encoded bytes */
        num_enc_bytes = 2 * (MAX_SERIAL_SIZE + 2);

    }

    /* Invalid frame format */
    else
    {
        /* Invalid output format, set up error code */
        num_enc_bytes = -1;
    }

    return(num_enc_bytes);
}


