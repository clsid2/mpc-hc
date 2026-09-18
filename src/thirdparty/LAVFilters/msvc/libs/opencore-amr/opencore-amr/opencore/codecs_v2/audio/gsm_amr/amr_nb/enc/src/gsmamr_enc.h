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



 Filename: gsmamr_enc.h

------------------------------------------------------------------------------
 INCLUDE DESCRIPTION

 This header contains all the necessary information needed to use the
 GSM AMR encoder library.

------------------------------------------------------------------------------
*/
#ifndef _GSMAMR_ENC_H_
#define _GSMAMR_ENC_H_

/*----------------------------------------------------------------------------
; INCLUDES
----------------------------------------------------------------------------*/

#include "gsm_amr_typedefs.h"

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

    /* Output format types */
#define AMR_TX_WMF  0
#define AMR_TX_IF2  1
#define AMR_TX_ETS  2
#define AMR_TX_IETF 3

    /*----------------------------------------------------------------------------
    ; EXTERNAL VARIABLES REFERENCES
    ----------------------------------------------------------------------------*/


    /*----------------------------------------------------------------------------
    ; SIMPLE TYPEDEF'S
    ----------------------------------------------------------------------------*/

    /*----------------------------------------------------------------------------
    ; ENUMERATED TYPEDEF'S
    ----------------------------------------------------------------------------*/

    enum Mode
    {
        MR475 = 0,/* 4.75 kbps */
        MR515,    /* 5.15 kbps */
        MR59,     /* 5.90 kbps */
        MR67,     /* 6.70 kbps */
        MR74,     /* 7.40 kbps */
        MR795,    /* 7.95 kbps */
        MR102,    /* 10.2 kbps */
        MR122,    /* 12.2 kbps */
        MRDTX,    /* DTX       */
        N_MODES   /* Not Used  */
    };

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
        AMR_NO_DATA         /* No data      */
    };

    /*----------------------------------------------------------------------------
    ; STRUCTURES TYPEDEF'S
    ----------------------------------------------------------------------------*/
    /*----------------------------------------------------------------------------
    ; GLOBAL FUNCTION DEFINITIONS
    ----------------------------------------------------------------------------*/
    /* AMREncodeInit initializes the GSM AMR Encoder library by calling
     * GSMInitEncode and sid_sync_init. If initialization was successful,
     * init_status is set to zero, otherwise, it is set to -1.
    */
    int AMREncodeInit(
        void **pEncStructure,
        void **pSidSyncStructure,
        Flag dtx_enable);

    /* AMREncodeReset resets the state memory used by the Encoder and SID sync
     * function. If reset was successful, reset_status is set to zero, otherwise,
     * it is set to -1.
    */
    int AMREncodeReset(
        void *pEncStructure,
        void *pSidSyncStructure);

    /* AMREncodeExit frees up the state memory used by the Encoder and SID
     * synchronization function.
    */
    void AMREncodeExit(
        void **pEncStructure,
        void **pSidSyncStructure);

    /*
     * AMREncode is the entry point to the ETS Encoder library that encodes the raw
     * data speech bits and converts the encoded bitstream into either an IF2-
     * formatted bitstream, WMF-formatted bitstream, or ETS-formatted bitstream,
     * depending on the the value of output_format. A zero is returned on success.
     */
    int AMREncode(
        void *pEncState,
        void *pSidSyncState,
        enum Mode mode,
        Word16 *pEncInput,
        unsigned char *pEncOutput,
        enum Frame_Type_3GPP *p3gpp_frame_type,
        Word16 output_format
    );

#ifdef __cplusplus
}
#endif


#endif  /* _GSMAMR_DEC_H_ */

