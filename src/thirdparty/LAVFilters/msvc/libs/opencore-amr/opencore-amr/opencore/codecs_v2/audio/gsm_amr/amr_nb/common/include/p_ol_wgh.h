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

 Filename: p_ol_wgh.h

------------------------------------------------------------------------------
 INCLUDE DESCRIPTION

       File             : p_ol_wgh.h
       Purpose          : Compute the open loop pitch lag with weighting.

------------------------------------------------------------------------------
*/

#ifndef P_OL_WGH_H
#define P_OL_WGH_H

/*----------------------------------------------------------------------------
; INCLUDES
----------------------------------------------------------------------------*/
#include "typedef.h"
#include "mode.h"
#include "vad.h"

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
    extern const Word16 corrweight[];

    /*----------------------------------------------------------------------------
    ; SIMPLE TYPEDEF'S
    ----------------------------------------------------------------------------*/

    /*----------------------------------------------------------------------------
    ; ENUMERATED TYPEDEF'S
    ----------------------------------------------------------------------------*/

    /*----------------------------------------------------------------------------
    ; STRUCTURES TYPEDEF'S
    ----------------------------------------------------------------------------*/
    /* state variable */

    typedef struct
    {
        Word16 old_T0_med;
        Word16 ada_w;
        Word16 wght_flg;
    } pitchOLWghtState;

    /*----------------------------------------------------------------------------
    ; GLOBAL FUNCTION DEFINITIONS
    ; [List function prototypes here]
    ----------------------------------------------------------------------------*/
    Word16 p_ol_wgh_init(pitchOLWghtState **st);
    /* initialize one instance of the pre processing state.
       Stores pointer to filter status struct in *st. This pointer has to
       be passed to p_ol_wgh in each call.
       returns 0 on success
     */

    Word16 p_ol_wgh_reset(pitchOLWghtState *st);
    /* reset of pre processing state (i.e. set state memory to zero)
       returns 0 on success
     */

    void p_ol_wgh_exit(pitchOLWghtState **st);
    /* de-initialize pre processing state (i.e. free status struct)
       stores NULL in *st
     */

    Word16 Pitch_ol_wgh(      /* o   : open loop pitch lag                            */
        pitchOLWghtState *st, /* i/o : State struct                                   */
        vadState *vadSt,      /* i/o : VAD state struct                               */
        Word16 signal[],      /* i   : signal used to compute the open loop pitch     */
        /*       signal[-pit_max] to signal[-1] should be known */
        Word16 pit_min,       /* i   : minimum pitch lag                              */
        Word16 pit_max,       /* i   : maximum pitch lag                              */
        Word16 L_frame,       /* i   : length of frame to compute pitch               */
        Word16 old_lags[],    /* i   : history with old stored Cl lags                */
        Word16 ol_gain_flg[], /* i   : OL gain flag                                   */
        Word16 idx,           /* i   : index                                          */
        Flag dtx,             /* i   : dtx flag; use dtx=1, do not use dtx=0          */
        Flag   *pOverflow     /* o   : overflow flag                                  */
    );

#ifdef __cplusplus
}
#endif

#endif  /* _P_OL_WGH_H_ */
