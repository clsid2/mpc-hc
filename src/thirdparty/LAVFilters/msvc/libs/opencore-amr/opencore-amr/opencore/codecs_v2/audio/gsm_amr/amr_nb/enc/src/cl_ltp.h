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



 Filename: cl_ltp.h

------------------------------------------------------------------------------
 INCLUDE DESCRIPTION

 This file contains all the constant definitions and prototype definitions
 needed by the cl_ltp.c

------------------------------------------------------------------------------
*/

/*----------------------------------------------------------------------------
; CONTINUE ONLY IF NOT ALREADY DEFINED
----------------------------------------------------------------------------*/
#ifndef cl_ltp_h
#define cl_ltp_h "$Id $"


/*----------------------------------------------------------------------------
; INCLUDES
----------------------------------------------------------------------------*/
#include "typedef.h"
#include "mode.h"
#include "pitch_fr.h"
#include "ton_stab.h"

/*--------------------------------------------------------------------------*/
#ifdef __cplusplus
extern "C"
{
#endif

    /*----------------------------------------------------------------------------
    ; MACROS
    ; Define module specific macros here
    ----------------------------------------------------------------------------*/

    /*----------------------------------------------------------------------------
    ; DEFINES
    ; Include all pre-processor statements here.
    ----------------------------------------------------------------------------*/

    /*----------------------------------------------------------------------------
    ; EXTERNAL VARIABLES REFERENCES
    ; Declare variables used in this module but defined elsewhere
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

    /* state variable */
    typedef struct
    {
        Pitch_frState *pitchSt;
    } clLtpState;

    /*----------------------------------------------------------------------------
    ; GLOBAL FUNCTION DEFINITIONS
    ; Function Prototype declaration
    ----------------------------------------------------------------------------*/
    Word16 cl_ltp_init(clLtpState **st);
    /* initialize one instance of the pre processing state.
       Stores pointer to filter status struct in *st. This pointer has to
       be passed to cl_ltp in each call.
       returns 0 on success
     */

    Word16 cl_ltp_reset(clLtpState *st);
    /* reset of pre processing state (i.e. set state memory to zero)
       returns 0 on success
     */
    void cl_ltp_exit(clLtpState **st);
    /* de-initialize pre processing state (i.e. free status struct)
       stores NULL in *st
     */

    void cl_ltp(
        clLtpState *clSt,    /* i/o : State struct                              */
        tonStabState *tonSt, /* i/o : State struct                              */
        enum Mode mode,      /* i   : coder mode                                */
        Word16 frameOffset,  /* i   : Offset to subframe                        */
        Word16 T_op[],       /* i   : Open loop pitch lags                      */
        Word16 *h1,          /* i   : Impulse response vector               Q12 */
        Word16 *exc,         /* i/o : Excitation vector                      Q0 */
        Word16 res2[],       /* i/o : Long term prediction residual          Q0 */
        Word16 xn[],         /* i   : Target vector for pitch search         Q0 */
        Word16 lsp_flag,     /* i   : LSP resonance flag                        */
        Word16 xn2[],        /* o   : Target vector for codebook search      Q0 */
        Word16 y1[],         /* o   : Filtered adaptive excitation           Q0 */
        Word16 *T0,          /* o   : Pitch delay (integer part)                */
        Word16 *T0_frac,     /* o   : Pitch delay (fractional part)             */
        Word16 *gain_pit,    /* o   : Pitch gain                            Q14 */
        Word16 g_coeff[],    /* o   : Correlations between xn, y1, & y2         */
        Word16 **anap,       /* o   : Analysis parameters                       */
        Word16 *gp_limit,    /* o   : pitch gain limit                          */
        const Word16* qua_gain_pitch_ptr, /* i : ptr to read-only table         */
        Flag   *pOverflow    /* o   : overflow indicator                        */
    );

    /*----------------------------------------------------------------------------
    ; END
    ----------------------------------------------------------------------------*/
#ifdef __cplusplus
}
#endif

#endif /* _CL_LTP_H_ */

