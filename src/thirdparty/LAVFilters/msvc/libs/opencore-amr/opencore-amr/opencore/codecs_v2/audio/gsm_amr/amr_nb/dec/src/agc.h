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



 Filename: agc.h

------------------------------------------------------------------------------
 INCLUDE DESCRIPTION

      File             : agc.h
      Purpose          : Scales the postfilter output on a subframe basis
                       : by automatic control of the subframe gain.

------------------------------------------------------------------------------
*/

#ifndef _AGC_H_
#define _AGC_H_

/*----------------------------------------------------------------------------
; INCLUDES
----------------------------------------------------------------------------*/
#include "typedef.h"

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
        Word16 past_gain;
    } agcState;

    /*----------------------------------------------------------------------------
    ; GLOBAL FUNCTION DEFINITIONS
    ; [List function prototypes here]
    ----------------------------------------------------------------------------*/
    /*----------------------------------------------------------------------------
    ;
    ;  Function    : agc_reset
    ;  Purpose     : Reset of agc (i.e. set state memory to 1.0)
    ;  Returns     : 0 on success
    ;
    ----------------------------------------------------------------------------*/
    Word16 agc_reset(agcState *st);


    /*----------------------------------------------------------------------------
    ;
    ;  Function    : agc
    ;  Purpose     : Scales the postfilter output on a subframe basis
    ;  Description : sig_out[n] = sig_out[n] * gain[n];
    ;                where gain[n] is the gain at the nth sample given by
    ;                gain[n] = agc_fac * gain[n-1] + (1 - agc_fac) g_in/g_out
    ;                g_in/g_out is the square root of the ratio of energy at
    ;                the input and output of the postfilter.
    ;
    ----------------------------------------------------------------------------*/
    void agc(
        agcState *st,      /* i/o : agc state                         */
        Word16 *sig_in,    /* i   : postfilter input signal, (l_trm)  */
        Word16 *sig_out,   /* i/o : postfilter output signal, (l_trm) */
        Word16 agc_fac,    /* i   : AGC factor                        */
        Word16 l_trm,      /* i   : subframe size                     */
        Flag *pOverflow    /* i   : overflow flag                     */
    );

    /*----------------------------------------------------------------------------
    ;
    ;  Function:  agc2
    ;  Purpose:   Scales the excitation on a subframe basis
    ;
    ----------------------------------------------------------------------------*/
    void agc2(
        Word16 *sig_in,    /* i   : postfilter input signal   */
        Word16 *sig_out,   /* i/o : postfilter output signal  */
        Word16 l_trm,      /* i   : subframe size             */
        Flag *pOverflow    /* i   : overflow flag             */
    );

#ifdef __cplusplus
}
#endif

#endif  /* _AGC_H_ */


