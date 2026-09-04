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

 Filename: pow2.cpp

------------------------------------------------------------------------------
*/

/*----------------------------------------------------------------------------
; INCLUDES
----------------------------------------------------------------------------*/
#include    "pow2.h"
#include    "basic_op.h"

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
; LOCAL STORE/BUFFER/POINTER DEFINITIONS
; Variable declaration - defined here and used outside this module
----------------------------------------------------------------------------*/


/*
------------------------------------------------------------------------------
 FUNCTION NAME: Pow2
------------------------------------------------------------------------------
 INPUT AND OUTPUT DEFINITIONS

 Inputs:
    exponent = Integer part whose valid range is: 0 <= value <= 30 (Word16)
    fraction = Fractional part whose valid range is 0 <= value < 1

    pOverflow = pointer to overflow flag

 Outputs:
    L_x = Result of the Pow2() computation (Word32)
    pOverflow -> 1 if the Pow2() function results in saturation

 Returns:
    None

 Global Variables Used:
    None

 Local Variables Needed:
    None

------------------------------------------------------------------------------
 FUNCTION DESCRIPTION

 This function computes  L_x = pow(2.0, exponent.fraction)

 The function Pow2(L_x) is approximated by a table and linear interpolation.

 1- i = bit10-b15 of fraction,   0 <= i <= 31
 2- a = bit0-b9   of fraction
 3- L_x = table[i]<<16 - (table[i] - table[i+1]) * a * 2
 4- L_x = L_x >> (30-exponent)     (with rounding)

------------------------------------------------------------------------------
 REQUIREMENTS

 None

------------------------------------------------------------------------------
 REFERENCES

 pow2.c, UMTS GSM AMR speech codec, R99 - Version 3.2.0, March 2, 2001

------------------------------------------------------------------------------
 PSEUDO-CODE

Word32 Pow2 (           // (o)  : result       (range: 0<=val<=0x7fffffff)
    Word16 exponent,    // (i)  : Integer part.      (range: 0<=val<=30)
    Word16 fraction     // (i)  : Fractional part.  (range: 0.0<=val<1.0)
)
{
    Word16 exp, i, a, tmp;
    Word32 L_x;

    L_x = L_mult (fraction, 32);        // L_x = fraction<<6
    i = extract_h (L_x);                // Extract b10-b16 of fraction
    L_x = L_shr (L_x, 1);
    a = extract_l (L_x);                // Extract b0-b9   of fraction
    a = a & (Word16) 0x7fff;

    L_x = L_deposit_h (table[i]);       // table[i] << 16
    tmp = sub (table[i], table[i + 1]); // table[i] - table[i+1]
    L_x = L_msu (L_x, tmp, a);          // L_x -= tmp*a*2

    exp = sub (30, exponent);
    L_x = L_shr_r (L_x, exp);

    return (L_x);
}

------------------------------------------------------------------------------
 CAUTION [optional]
 [State any special notes, constraints or cautions for users of this function]

------------------------------------------------------------------------------
*/

/*----------------------------------------------------------------------------
; FUNCTION CODE
----------------------------------------------------------------------------*/

OSCL_EXPORT_REF Word32 Pow2(            /* (o)  : result       (range: 0<=val<=0x7fffffff) */
    Word16 exponent,    /* (i)  : Integer part.      (range: 0<=val<=30)   */
    Word16 fraction,    /* (i)  : Fractional part.  (range: 0.0<=val<1.0)  */
    Flag *pOverflow
)
{
    Word16 exp, i, a, tmp;
    Word32 L_x;

    L_x = L_mult(fraction, 32, pOverflow);      /* L_x = fraction<<6    */

    /* Extract b0-b16 of fraction */

    i = ((Word16)(L_x >> 16)) & 31;             /* ensure index i is bounded */
    a = (Word16)((L_x >> 1) & 0x7fff);

    L_x = ((Word32) pow2_tbl[i] << 16);             /* pow2_tbl[i] << 16       */

    /* pow2_tbl[i] - pow2_tbl[i+1] */
    tmp = pow2_tbl[i] - pow2_tbl[i + 1];
    L_x = L_msu(L_x, tmp, a, pOverflow);        /* L_x -= tmp*a*2        */

    exp = 30 - exponent;
    L_x = L_shr_r(L_x, exp, pOverflow);

    return (L_x);
}
