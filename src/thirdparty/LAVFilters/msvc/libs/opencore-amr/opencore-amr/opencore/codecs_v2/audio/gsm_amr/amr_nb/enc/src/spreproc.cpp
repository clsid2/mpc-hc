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



 Filename: spreproc.cpp
 Functions: subframePreProc

------------------------------------------------------------------------------
 MODULE DESCRIPTION


------------------------------------------------------------------------------
*/

/*----------------------------------------------------------------------------
; INCLUDES
----------------------------------------------------------------------------*/
#include "spreproc.h"
#include "typedef.h"
#include "weight_a.h"
#include "syn_filt.h"
#include "residu.h"
#include "oscl_mem.h"

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
; LOCAL VARIABLE DEFINITIONS
; Variable declaration - defined here and used outside this module
----------------------------------------------------------------------------*/

/*
------------------------------------------------------------------------------
 FUNCTION NAME: subframePreProc
------------------------------------------------------------------------------
 INPUT AND OUTPUT DEFINITIONS

 Inputs:
    mode        -- enum Mode          -- coder mode
    gamma1      -- const Word16 array -- spectral exp. factor 1
    gamma1_12k2 -- const Word16 array -- spectral exp. factor 1 for EFR
    gamma2      -- const Word16 array -- spectral exp. factor 2
    A           -- Pointer to Word16  -- A(z) unquantized for the 4 subframes
    Aq          -- Pointer to Word16  -- A(z)   quantized for the 4 subframes
    speech      -- Pointer to Word16  -- speech segment
    mem_err     -- Pointer to Word16  -- pointer to error signal
    mem_w0      -- Pointer to Word16  -- memory of weighting filter
    zero        -- Pointer to Word16  -- pointer to zero vector

 Outputs:
    ai_zero -- Word16 array -- history of weighted synth. filter
    exc     -- Word16 array -- long term prediction residual
    h1      -- Word16 array -- impulse response
    xn      -- Word16 array -- target vector for pitch search
    res2    -- Word16 array -- long term prediction residual
    error   -- Word16 array -- error of LPC synthesis filter

 Returns:
    Zero

 Global Variables Used:
    None

 Local Variables Needed:
    None

------------------------------------------------------------------------------
 FUNCTION DESCRIPTION


------------------------------------------------------------------------------
 REQUIREMENTS

 None

------------------------------------------------------------------------------
 REFERENCES

 spreproc.c, UMTS GSM AMR speech codec, R99 - Version 3.2.0, March 2, 2001

------------------------------------------------------------------------------
 PSEUDO-CODE


------------------------------------------------------------------------------
 CAUTION [optional]
 [State any special notes, constraints or cautions for users of this function]

------------------------------------------------------------------------------
*/

void subframePreProc(
    enum Mode mode,            /* i  : coder mode                            */
    const Word16 gamma1[],     /* i  : spectral exp. factor 1                */
    const Word16 gamma1_12k2[],/* i  : spectral exp. factor 1 for EFR        */
    const Word16 gamma2[],     /* i  : spectral exp. factor 2                */
    Word16 *A,                 /* i  : A(z) unquantized for the 4 subframes  */
    Word16 *Aq,                /* i  : A(z)   quantized for the 4 subframes  */
    Word16 *speech,            /* i  : speech segment                        */
    Word16 *mem_err,           /* i  : pointer to error signal               */
    Word16 *mem_w0,            /* i  : memory of weighting filter            */
    Word16 *zero,              /* i  : pointer to zero vector                */
    Word16 ai_zero[],          /* o  : history of weighted synth. filter     */
    Word16 exc[],              /* o  : long term prediction residual         */
    Word16 h1[],               /* o  : impulse response                      */
    Word16 xn[],               /* o  : target vector for pitch search        */
    Word16 res2[],             /* o  : long term prediction residual         */
    Word16 error[]             /* o  : error of LPC synthesis filter         */
)
{
    Word16 Ap1[MP1];              /* A(z) with spectral expansion         */
    Word16 Ap2[MP1];              /* A(z) with spectral expansion         */
    const Word16 *g1;             /* Pointer to correct gammma1 vector    */

    /* mode specific pointer to gamma1 values */
    if (mode == MR122 || mode == MR102)
    {
        g1 = gamma1_12k2;
    }
    else
    {
        g1 = gamma1;
    }

    /* Find the weighted LPC coefficients for the weighting filter. */
    Weight_Ai(A, g1, Ap1);
    Weight_Ai(A, gamma2, Ap2);

    oscl_memcpy(ai_zero, Ap1, (M + 1)*sizeof(Word16));


    Syn_filt(Aq, ai_zero, h1, L_SUBFR, zero, 0);
    Syn_filt(Ap2, h1, h1, L_SUBFR, zero, 0);

    /*
     *
     *          Find the target vector for pitch search:
     *
     */

    /* LPC residual */
    Residu(Aq, speech, res2, L_SUBFR);

    oscl_memcpy(exc, res2, L_SUBFR*sizeof(Word16));

    Syn_filt(Aq, exc, error, L_SUBFR, mem_err, 0);

    Residu(Ap1, error, xn, L_SUBFR);

    /* target signal xn[]*/
    Syn_filt(Ap2, xn, xn, L_SUBFR, mem_w0, 0);

    return;

}
