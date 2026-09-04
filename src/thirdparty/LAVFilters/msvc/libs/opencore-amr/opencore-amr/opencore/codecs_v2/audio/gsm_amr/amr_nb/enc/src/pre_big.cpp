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



 Filename: pre_big.cpp

------------------------------------------------------------------------------
 MODULE DESCRIPTION

    Big subframe (2 subframes) preprocessing
------------------------------------------------------------------------------
*/

/*----------------------------------------------------------------------------
; INCLUDES
----------------------------------------------------------------------------*/
#include "pre_big.h"
#include "typedef.h"
#include "basic_op.h"
#include "syn_filt.h"
#include "weight_a.h"
#include "residu.h"
#include "cnst.h"

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
 FUNCTION NAME: pre_big
------------------------------------------------------------------------------
 INPUT AND OUTPUT DEFINITIONS

 Inputs:
    mode = enum Mode -- coder mode
    gamma1 = array of type const Word16 -- spectral exp. factor 1
    gamma1_12k2 = array of type const Word16 -- spectral exp. factor 1 for EFR
    gamma2 = array of type const Word16 -- spectral exp. factor 2
    A_t = array of type Word16 -- A(z) unquantized, for 4 subframes, Q12
    frameOffset = Word16 -- Start position in speech vector,   Q0
    speech[] = array of type Word16 -- speech,                            Q0

 Outputs:
    mem_w = array of type Word16 -- synthesis filter memory state,     Q0
    wsp   = array of type Word16 -- weighted speech                    Q0
    pOverflow = pointer of type Flag -- overflow indicator

 Returns:
    None

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

 pre_big.c, UMTS GSM AMR speech codec, R99 - Version 3.2.0, March 2, 2001

------------------------------------------------------------------------------
 PSEUDO-CODE


------------------------------------------------------------------------------
 CAUTION [optional]
 [State any special notes, constraints or cautions for users of this function]

------------------------------------------------------------------------------
*/

void pre_big(
    enum Mode mode,            /* i  : coder mode                             */
    const Word16 gamma1[],     /* i  : spectral exp. factor 1                 */
    const Word16 gamma1_12k2[],/* i  : spectral exp. factor 1 for EFR         */
    const Word16 gamma2[],     /* i  : spectral exp. factor 2                 */
    Word16 A_t[],              /* i  : A(z) unquantized, for 4 subframes, Q12 */
    Word16 frameOffset,        /* i  : Start position in speech vector,   Q0  */
    Word16 speech[],           /* i  : speech,                            Q0  */
    Word16 mem_w[],            /* i/o: synthesis filter memory state,     Q0  */
    Word16 wsp[],              /* o  : weighted speech                    Q0  */
    Flag   *pOverflow          /* o  : overflow indicator                     */
)
{
    Word16 Ap1[MP1];            /* A(z) with spectral expansion         */
    Word16 Ap2[MP1];            /* A(z) with spectral expansion         */
    const Word16 *g1;           /* Pointer to correct gammma1 vector    */
    Word16 aOffset;
    Word16 i;

    if (mode <= MR795)
    {
        g1 = gamma1;
    }
    else
    {
        g1 = gamma1_12k2;
    }

    if (frameOffset > 0)
    {
        aOffset = MP1 << 1;
    }
    else
    {
        aOffset = 0;
    }

    /* process two subframes (which form the "big" subframe) */
    for (i = 0; i < 2; i++)
    {
        Weight_Ai(&A_t[aOffset], g1, Ap1);
        Weight_Ai(&A_t[aOffset], gamma2, Ap2);
        Residu(Ap1, &speech[frameOffset], &wsp[frameOffset], L_SUBFR);

        Syn_filt(Ap2, &wsp[frameOffset], &wsp[frameOffset], L_SUBFR, mem_w, 1);

        aOffset += MP1;

        frameOffset += L_SUBFR;
    }

    return;
}
