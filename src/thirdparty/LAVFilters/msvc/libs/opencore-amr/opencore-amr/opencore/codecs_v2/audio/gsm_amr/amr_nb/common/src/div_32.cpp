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

 Filename: div_32.cpp

------------------------------------------------------------------------------
*/

/*----------------------------------------------------------------------------
; INCLUDES
----------------------------------------------------------------------------*/
#include    "basic_op.h"

/*----------------------------------------------------------------------------
; MACROS
; [Define module specific macros here]
----------------------------------------------------------------------------*/

/*----------------------------------------------------------------------------
; DEFINES
; [Include all pre-processor statements here. Include conditional
; compile variables also.]
----------------------------------------------------------------------------*/

/*----------------------------------------------------------------------------
; LOCAL FUNCTION DEFINITIONS
; [List function prototypes here]
----------------------------------------------------------------------------*/

/*----------------------------------------------------------------------------
; LOCAL VARIABLE DEFINITIONS
; [Variable declaration - defined here and used outside this module]
----------------------------------------------------------------------------*/


/*
------------------------------------------------------------------------------
 FUNCTION NAME: div_32
------------------------------------------------------------------------------
 INPUT AND OUTPUT DEFINITIONS

 Inputs:
    L_num = 32 bit signed integer (Word32) whose value falls in the
                range : 0x0000 0000 < L_num < L_denom
    L_denom_hi = 16 bit positive normalized integer whose value falls in
               the range : 0x4000 < hi < 0x7fff
    L_denom_lo = 16 bit positive integer whose value falls in the range :
               0 < lo < 0x7fff

    pOverflow = pointer to overflow (Flag)

 Outputs:
    pOverflow -> 1 if the 32 bit divide operation resulted in overflow

 Returns:
    result = 32-bit quotient of of the division of two 32 bit integers
                L_num / L_denom (Word32)

 Global Variables Used:
    None

 Local Variables Needed:
    None

------------------------------------------------------------------------------
 FUNCTION DESCRIPTION

 This function is a fractional integer division of two 32 bit numbers, the
 numerator L_num and the denominator L_denom. The denominator is formed by
 combining denom_hi and denom_lo. Note that denom_hi is a normalized numbers.
 The numerator and denominator must be positive and the numerator must be
 less than the denominator.

 The division is done as follows:
 1. Find 1/L_denom by first approximating: approx = 1 / denom_hi.
 2. 1/L_denom = approx * (2.0 - L_denom * approx ).
 3. result = L_num * (1/L_denom).

------------------------------------------------------------------------------
 REQUIREMENTS

 None

------------------------------------------------------------------------------
 REFERENCES

 [1] div_32() function in oper_32b.c,  UMTS GSM AMR speech codec, R99 -
 Version 3.2.0, March 2, 2001

------------------------------------------------------------------------------
 PSEUDO-CODE


------------------------------------------------------------------------------
 CAUTION [optional]
 [State any special notes, constraints or cautions for users of this function]

------------------------------------------------------------------------------
*/

/*----------------------------------------------------------------------------
; FUNCTION CODE
----------------------------------------------------------------------------*/
Word32 Div_32(Word32 L_num,
              Word16 L_denom_hi,
              Word16 L_denom_lo,
              Flag   *pOverflow)
{

    Word16 approx;
    Word16 hi;
    Word16 lo;
    Word16 n_hi;
    Word16 n_lo;
    Word32 result;

    /* First approximation: 1 / L_denom = 1/L_denom_hi */

    approx = div_s((Word16) 0x3fff, L_denom_hi);

    /* 1/L_denom = approx * (2.0 - L_denom * approx) */

    result = Mpy_32_16(L_denom_hi, L_denom_lo, approx, pOverflow);
    /* result is > 0 , and less than 1.0 */
    result =  0x7fffffffL - result;

    hi = (Word16)(result >> 16);
    lo = (result >> 1) - (hi << 15);

    result = Mpy_32_16(hi, lo, approx, pOverflow);

    /* L_num * (1/L_denom) */

    hi = (Word16)(result >> 16);
    lo = (result >> 1) - (hi << 15);

    n_hi = (Word16)(L_num >> 16);
    n_lo = (L_num >> 1) - (n_hi << 15);

    result = Mpy_32(n_hi, n_lo, hi, lo, pOverflow);
    result = L_shl(result, 2, pOverflow);

    return (result);
}

