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
*****************************************************************************
*
*      GSM AMR speech codec   Version 2.0.0   February 8, 1999
*
*****************************************************************************
*
*      File             : sp_dec.h
*      Purpose          : Decoding and post filtering of one speech frame.
*

 Description:  Replaced "int" and/or "char" with OSCL defined types.

*****************************************************************************
*/
#ifndef sp_dec_h
#define sp_dec_h "$Id $"

/*
*****************************************************************************
*                         INCLUDE FILES
*****************************************************************************
*/
#include "typedef.h"
#include "cnst.h"
#include "dec_amr.h"
#include "pstfilt.h"
#include "post_pro.h"
#include "mode.h"

/*
*****************************************************************************
*                         DEFINITION OF DATA TYPES
*****************************************************************************
*/
typedef struct
{
    Decoder_amrState  decoder_amrState;
    Post_FilterState  post_state;
    Post_ProcessState postHP_state;
    enum Mode prev_mode;
} Speech_Decode_FrameState;

/*
*****************************************************************************
*                         DECLARATION OF PROTOTYPES
*****************************************************************************
*/

#if defined(__cplusplus)
extern "C"
{
#endif
    Word16 GSMInitDecode(void **state_data,
    Word8 *id);
    /* initialize one instance of the speech decoder
       Stores pointer to filter status struct in *st. This pointer has to
       be passed to Speech_Decode_Frame in each call.
       returns 0 on success
     */

    Word16 Speech_Decode_Frame_reset(void *state_data);
    /* reset speech decoder (i.e. set state memory to zero)
       returns 0 on success
     */

    void GSMDecodeFrameExit(void **state_data);
    /* de-initialize speech decoder (i.e. free status struct)
       stores NULL in *s
     */

    void GSMFrameDecode(
        Speech_Decode_FrameState *st, /* io: post filter states                */
        enum Mode mode,               /* i : AMR mode                          */
        Word16 *serial,               /* i : serial bit stream                 */
        enum RXFrameType frame_type,  /* i : Frame type                        */
        Word16 *synth                 /* o : synthesis speech (postfiltered    */
        /*     output)                           */
    );
    /*    return 0 on success
     */
#if defined(__cplusplus)
}
#endif
#endif
