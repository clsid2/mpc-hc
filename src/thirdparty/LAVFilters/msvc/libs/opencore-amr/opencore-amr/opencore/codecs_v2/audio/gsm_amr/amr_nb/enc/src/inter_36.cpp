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



 Filename: inter_36.cpp

------------------------------------------------------------------------------
*/

/*----------------------------------------------------------------------------
; INCLUDES
----------------------------------------------------------------------------*/
#include "inter_36.h"
#include "cnst.h"
#include "inter_36_tab.h"
#include "basic_op.h"

/*----------------------------------------------------------------------------
; MACROS
; Define module specific macros here
----------------------------------------------------------------------------*/


/*----------------------------------------------------------------------------
; DEFINES
; Include all pre-processor statements here. Include conditional
; compile variables also.
----------------------------------------------------------------------------*/
#define UP_SAMP_MAX  6

/*----------------------------------------------------------------------------
; LOCAL FUNCTION DEFINITIONS
; Function Prototype declaration
----------------------------------------------------------------------------*/


/*----------------------------------------------------------------------------
; LOCAL STORE/BUFFER/POINTER DEFINITIONS
; Variable declaration - defined here and used outside this module
----------------------------------------------------------------------------*/


/*
------------------------------------------------------------------------------
 FUNCTION NAME: inter_36
------------------------------------------------------------------------------
 INPUT AND OUTPUT DEFINITIONS

 Inputs:
    pX    = pointer to input vector of type Word16
    frac  = fraction  (-2..2 for 3*, -3..3 for 6*)  of type Word16
    flag3 = if set, upsampling rate = 3 (6 otherwise) of type Word16
    pOverflow = pointer to overflow flag

 Outputs:
    None

 Returns:
    None

 Global Variables Used:
    None.

 Local Variables Needed:
    None.

------------------------------------------------------------------------------
 FUNCTION DESCRIPTION

      File             : inter_36.c
      Purpose          : Interpolating the normalized correlation
                       : with 1/3 or 1/6 resolution.

------------------------------------------------------------------------------
 REQUIREMENTS

 None.

------------------------------------------------------------------------------
 REFERENCES

 inter_36.c, UMTS GSM AMR speech codec, R99 - Version 3.2.0, March 2, 2001

------------------------------------------------------------------------------
 PSEUDO-CODE

    Word16 i, k;
    Word16 *x1, *x2;
    const Word16 *c1, *c2;
    Word32 s;

    if (flag3 != 0)
    {
      frac = shl (frac, 1);   // inter_3[k] = inter_6[2*k] -> k' = 2*k
    }

    if (frac < 0)
    {
        frac = add (frac, UP_SAMP_MAX);
        x--;
    }

    x1 = &x[0];
    x2 = &x[1];
    c1 = &inter_6[frac];
    c2 = &inter_6[sub (UP_SAMP_MAX, frac)];

    s = 0;
    for (i = 0, k = 0; i < L_INTER_SRCH; i++, k += UP_SAMP_MAX)
    {
        s = L_mac (s, x1[-i], c1[k]);
        s = L_mac (s, x2[i], c2[k]);
    }

    return pv_round (s);

------------------------------------------------------------------------------
 CAUTION [optional]
 [State any special notes, constraints or cautions for users of this function]

------------------------------------------------------------------------------
*/
Word16 Interpol_3or6(   /* o : interpolated value                        */
    Word16 *pX,         /* i : input vector                              */
    Word16 frac,        /* i : fraction  (-2..2 for 3*, -3..3 for 6*)    */
    Word16 flag3,       /* i : if set, upsampling rate = 3 (6 otherwise) */
    Flag   *pOverflow
)
{
    Word16 i;
    Word16 k;
    Word16 *pX1;
    Word16 *pX2;
    const Word16 *pC1;
    const Word16 *pC2;
    Word32 s;
    Word16 temp1;

    OSCL_UNUSED_ARG(pOverflow);

    if (flag3 != 0)
    {
        frac <<= 1;
        /* inter_3[k] = inter_6[2*k] -> k' = 2*k */
    }

    if (frac < 0)
    {
        frac += UP_SAMP_MAX;
        pX--;
    }

    pX1   = &pX[0];
    pX2   = &pX[1];
    pC1   = &inter_6[frac];
    temp1 = UP_SAMP_MAX - frac;
    pC2   = &inter_6[temp1];

    s = 0x04000;
    k = 0;

    for (i = (L_INTER_SRCH >> 1); i != 0; i--)
    {
        s = amrnb_fxp_mac_16_by_16bb((Word32) * (pX1--), (Word32) pC1[k], s);
        s = amrnb_fxp_mac_16_by_16bb((Word32) * (pX2++), (Word32) pC2[k], s);
        k += UP_SAMP_MAX;
        s = amrnb_fxp_mac_16_by_16bb((Word32) * (pX1--), (Word32) pC1[k], s);
        s = amrnb_fxp_mac_16_by_16bb((Word32) * (pX2++), (Word32) pC2[k], s);
        k <<= 1;
    }

    return((Word16)(s >> 15));
}






