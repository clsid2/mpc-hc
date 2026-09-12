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



 Filename: pitch_fr.h

------------------------------------------------------------------------------
 INCLUDE DESCRIPTION

      File             : pitch_fr.h
      Purpose          : Find the pitch period with 1/3 or 1/6 subsample
                       : resolution (closed loop).

------------------------------------------------------------------------------
*/

#ifndef _PITCH_FR_H_
#define _PITCH_FR_H_
#define pitch_fr_h "$Id $"

/*----------------------------------------------------------------------------
; INCLUDES
----------------------------------------------------------------------------*/
#include "typedef.h"
#include "mode.h"

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
        Word16 T0_prev_subframe;   /* integer pitch lag of previous sub-frame */
    } Pitch_frState;

    /*----------------------------------------------------------------------------
    ; GLOBAL FUNCTION DEFINITIONS
    ; [List function prototypes here]
    ----------------------------------------------------------------------------*/

    Word16 Pitch_fr_init(Pitch_frState **st);
    /* initialize one instance of the pre processing state.
       Stores pointer to filter status struct in *st. This pointer has to
       be passed to Pitch_fr in each call.
       returns 0 on success
     */

    Word16 Pitch_fr_reset(Pitch_frState *st);
    /* reset of pre processing state (i.e. set state memory to zero)
       returns 0 on success
     */

    void Pitch_fr_exit(Pitch_frState **st);
    /* de-initialize pre processing state (i.e. free status struct)
       stores NULL in *st
     */

    Word16 Pitch_fr(         /* o   : pitch period (integer)                    */
        Pitch_frState *st,   /* i/o : State struct                              */
        enum Mode mode,      /* i   : codec mode                                */
        Word16 T_op[],       /* i   : open loop pitch lags                      */
        Word16 exc[],        /* i   : excitation buffer                         */
        Word16 xn[],         /* i   : target vector                             */
        Word16 h[],          /* i   : impulse response of synthesis and
                                  weighting filters                         */
        Word16 L_subfr,      /* i   : Length of subframe                        */
        Word16 i_subfr,      /* i   : subframe offset                           */
        Word16 *pit_frac,    /* o   : pitch period (fractional)                 */
        Word16 *resu3,       /* o   : subsample resolution 1/3 (=1) or 1/6 (=0) */
        Word16 *ana_index,   /* o   : index of encoding                         */
        Flag   *pOverflow
    );

#ifdef __cplusplus
}
#endif

#endif  /* _PITCH_FR_H_ */


