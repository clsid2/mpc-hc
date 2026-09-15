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



 Filename: gsmamr_dec.h

------------------------------------------------------------------------------
 INCLUDE DESCRIPTION

 This header contains all the necessary information needed to allow the gsm amr
 decoder library to be used properly upon release.

------------------------------------------------------------------------------
*/
#ifndef _GSMAMR_DEC_H_
#define _GSMAMR_DEC_H_

/*----------------------------------------------------------------------------
; INCLUDES
----------------------------------------------------------------------------*/

#include "gsm_amr_typedefs.h"
#include "pvamrnbdecoder_api.h"
/*--------------------------------------------------------------------------*/
#ifdef __cplusplus
extern "C"
{
#endif

    /*----------------------------------------------------------------------------
    ; MACROS
    ----------------------------------------------------------------------------*/


    /*----------------------------------------------------------------------------
    ; DEFINES
    ----------------------------------------------------------------------------*/
    /* Number of 13-bit linear PCM samples per 20 ms frame */
    /* L_FRAME = (8 kHz) * (20 msec) = 160 samples         */
#define L_FRAME     160

    /*----------------------------------------------------------------------------
    ; EXTERNAL VARIABLES REFERENCES
    ----------------------------------------------------------------------------*/


    /*----------------------------------------------------------------------------
    ; SIMPLE TYPEDEF'S
    ----------------------------------------------------------------------------*/

    /*----------------------------------------------------------------------------
    ; ENUMERATED TYPEDEF'S
    ----------------------------------------------------------------------------*/
    enum Frame_Type_3GPP
    {
        AMR_475 = 0,        /* 4.75 kbps    */
        AMR_515,            /* 5.15 kbps    */
        AMR_59,             /* 5.9 kbps     */
        AMR_67,             /* 6.7 kbps     */
        AMR_74,             /* 7.4 kbps     */
        AMR_795,            /* 7.95 kbps    */
        AMR_102,            /* 10.2 kbps    */
        AMR_122,            /* 12.2 kbps    */
        AMR_SID,            /* GSM AMR DTX  */
        GSM_EFR_SID,        /* GSM EFR DTX  */
        TDMA_EFR_SID,       /* TDMA EFR DTX */
        PDC_EFR_SID,        /* PDC EFR DTX  */
        FOR_FUTURE_USE1,    /* Unused 1     */
        FOR_FUTURE_USE2,    /* Unused 2     */
        FOR_FUTURE_USE3,    /* Unused 3     */
        AMR_NO_DATA
    };      /* No data      */

    /*----------------------------------------------------------------------------
    ; STRUCTURES TYPEDEF'S
    ----------------------------------------------------------------------------*/


    /*----------------------------------------------------------------------------
    ; GLOBAL FUNCTION DEFINITIONS
    ----------------------------------------------------------------------------*/
    /*
     * This function allocates memory for filter structure and initializes state
     * memory used by the GSM AMR decoder. This function returns zero. It will
     * return negative one if there is an error.
     */
    Word16 GSMInitDecode(void **state_data,
                         Word8 *id);

    /*
     * AMRDecode steps into the part of the library that decodes the raw data
     * speech bits for the decoding process. It returns the address offset of
     * the next frame to be decoded.
     */
    Word16 AMRDecode(
        void                      *state_data,
        enum Frame_Type_3GPP      frame_type,
        UWord8                    *speech_bits_ptr,
        Word16                    *raw_pcm_buffer,
        Word16                    input_format
    );

    /*
     * This function resets the state memory used by the GSM AMR decoder. This
     * function returns zero. It will return negative one if there is an error.
     */
    Word16 Speech_Decode_Frame_reset(void *state_data);

    /*
     * This function frees up the memory used for the state memory of the
     * GSM AMR decoder.
     */
    void GSMDecodeFrameExit(void **state_data);


#ifdef __cplusplus
}
#endif

#endif  /* _GSMAMR_DEC_H_ */

