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



 Filename: sp_enc.h

------------------------------------------------------------------------------
 INCLUDE DESCRIPTION

       File             : sp_enc.h
       Purpose          : Encoding of one speech frame

------------------------------------------------------------------------------
*/

#ifndef sp_enc_h
#define sp_enc_h "$Id $"

/*----------------------------------------------------------------------------
; INCLUDES
----------------------------------------------------------------------------*/
#include "typedef.h"
#include "cnst.h"
#include "pre_proc.h"
#include "mode.h"
#include "cod_amr.h"

/*--------------------------------------------------------------------------*/
#ifdef __cplusplus
extern "C"
{
#endif

    /*----------------------------------------------------------------------------
    ; MACROS
    ; [Define module specific macros here]
    ----------------------------------------------------------------------------*/

    /*----------------------------------------------------------------------------
    ; DEFINES
    ; [Include all pre-processor statements here.]
    ----------------------------------------------------------------------------*/


    /*----------------------------------------------------------------------------
    ; EXTERNAL VARIABLES REFERENCES
    ; [Declare variables used in this module but defined elsewhere]
    ----------------------------------------------------------------------------*/

    /*----------------------------------------------------------------------------
    ; SIMPLE TYPEDEF'S
    ----------------------------------------------------------------------------*/

    /*----------------------------------------------------------------------------
    ; ENUMERATED TYPEDEF'S
    ----------------------------------------------------------------------------*/

    /*----------------------------------------------------------------------------
    ; STRUCTURES TYPEDEF'S
    ----------------------------------------------------------------------------*/
    typedef struct
    {
        Pre_ProcessState *pre_state;
        cod_amrState   *cod_amr_state;
        Flag dtx;
    } Speech_Encode_FrameState;

    /*----------------------------------------------------------------------------
    ; GLOBAL FUNCTION DEFINITIONS
    ; [List function prototypes here]
    ----------------------------------------------------------------------------*/
    /* initialize one instance of the speech encoder
       Stores pointer to filter status struct in *st. This pointer has to
       be passed to Speech_Encode_Frame in each call.
       returns 0 on success */
    Word16 GSMInitEncode(void **state_data,
                         Flag   dtx,
                         Word8  *id);


    /* reset speech encoder (i.e. set state memory to zero)
       returns 0 on success */
    Word16 Speech_Encode_Frame_reset(void *state_data);

    /* de-initialize speech encoder (i.e. free status struct)
       stores NULL in *s */
    void GSMEncodeFrameExit(void **state_data);

    void Speech_Encode_Frame_First(
        Speech_Encode_FrameState *st, /* i/o : post filter states     */
        Word16 *new_speech);          /* i   : speech input           */

    void GSMEncodeFrame(
        void *state_data,             /* i/o : encoder states         */
        enum Mode mode,               /* i   : speech coder mode      */
        Word16 *new_speech,           /* i   : input speech           */
        Word16 *serial,               /* o   : serial bit stream      */
        enum Mode *usedMode           /* o   : used speech coder mode */
    );

#ifdef __cplusplus
}
#endif

#endif  /* _sp_enc_h_ */

