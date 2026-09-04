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

 Filename: log2_norm.cpp

------------------------------------------------------------------------------
*/

/*----------------------------------------------------------------------------
; INCLUDES
----------------------------------------------------------------------------*/
#include    "log2_norm.h"

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
 FUNCTION NAME: Log2_norm
------------------------------------------------------------------------------
 INPUT AND OUTPUT DEFINITIONS

 Inputs:
    L_x = normalized input value of type Word32
    exp = number of shifts required to normalize L_x; it is of type Word16
    exponent = pointer to the integer part of Log2 (of type Word16)
           whose valid range is: 0 <= value <= 30
    fraction = pointer to the fractional part of Log2 (of type Word16)
           whose valid range is: 0 <= value < 1

 Outputs:
    exponent points to the newly calculated integer part of Log2
    fraction points to the newly calculated fractional part of Log2

 Returns:
    None

 Global Variables Used:
    None

 Local Variables Needed:
    table = Log2 table of constants of type Word16

------------------------------------------------------------------------------
 FUNCTION DESCRIPTION

 The function Log2(L_x) calculates the logarithm of the normalized input
 buffer L_x. The logarithm is approximated by a table and linear
 interpolation. The following steps are used to compute Log2(L_x):

 1. exponent = 30 - norm_exponent
 2. i = bit25-b31 of L_x;  32<=i<=63  (because of normalization).
 3. a = bit10-b24
 4. i = i - 32
 5. fraction = table[i]<<16 - (table[i] - table[i+1]) * a * 2

------------------------------------------------------------------------------
 REQUIREMENTS

 None

------------------------------------------------------------------------------
 REFERENCES

 log2.c, UMTS GSM AMR speech codec, R99 - Version 3.2.0, March 2, 2001

------------------------------------------------------------------------------
 PSEUDO-CODE

void Log2_norm (
    Word32 L_x,         // (i) : input value (normalized)
    Word16 exp,         // (i) : norm_l (L_x)
    Word16 *exponent,   // (o) : Integer part of Log2.   (range: 0<=val<=30)
    Word16 *fraction    // (o) : Fractional part of Log2. (range: 0<=val<1)
)
{
    Word16 i, a, tmp;
    Word32 L_y;

    if (L_x <= (Word32) 0)
    {
        *exponent = 0;
        *fraction = 0;
        return;
    }

    *exponent = sub (30, exp);

    L_x = L_shr (L_x, 9);
    i = extract_h (L_x);                // Extract b25-b31
    L_x = L_shr (L_x, 1);
    a = extract_l (L_x);                // Extract b10-b24 of fraction
    a = a & (Word16) 0x7fff;

    i = sub (i, 32);

    L_y = L_deposit_h (table[i]);       // table[i] << 16
    tmp = sub (table[i], table[i + 1]); // table[i] - table[i+1]
    L_y = L_msu (L_y, tmp, a);          // L_y -= tmp*a*2

    *fraction = extract_h (L_y);

    return;
}

------------------------------------------------------------------------------
 CAUTION [optional]
 [State any special notes, constraints or cautions for users of this function]

------------------------------------------------------------------------------
*/

void Log2_norm(
    Word32 L_x,         /* (i) : input value (normalized)                   */
    Word16 exp,         /* (i) : norm_l (L_x)                               */
    Word16 *exponent,   /* (o) : Integer part of Log2.   (range: 0<=val<=30)*/
    Word16 *fraction    /* (o) : Fractional part of Log2. (range: 0<=val<1) */
)
{
    Word16 i, a, tmp;
    Word32 L_y;

    if (L_x <= (Word32) 0)
    {
        *exponent = 0;
        *fraction = 0;
    }
    else
    {
        /* Calculate exponent portion of Log2 */
        *exponent = 30 - exp;

        /* At this point, L_x > 0       */
        /* Shift L_x to the right by 10 to extract bits 10-31,  */
        /* which is needed to calculate fractional part of Log2 */
        L_x >>= 10;
        i = (Word16)(L_x >> 15);    /* Extract b25-b31 */
        a = L_x & 0x7fff;           /* Extract b10-b24 of fraction */

        /* Calculate table index -> subtract by 32 is done for           */
        /* proper table indexing, since 32<=i<=63 (due to normalization) */
        i -= 32;

        /* Fraction part of Log2 is approximated by using table[]    */
        /* and linear interpolation, i.e.,                           */
        /* fraction = table[i]<<16 - (table[i] - table[i+1]) * a * 2 */
        L_y = (Word32) log2_tbl[i] << 16;  /* table[i] << 16        */
        tmp = log2_tbl[i] - log2_tbl[i + 1];  /* table[i] - table[i+1] */
        L_y -= (((Word32) tmp) * a) << 1; /* L_y -= tmp*a*2        */

        *fraction = (Word16)(L_y >> 16);
    }

    return;
}
