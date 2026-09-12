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
 Filename: inv_sqrt.cpp

------------------------------------------------------------------------------
*/

/*----------------------------------------------------------------------------
; INCLUDES
----------------------------------------------------------------------------*/
#include    "inv_sqrt.h"
#include    "typedef.h"
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
 FUNCTION NAME: Inv_sqrt
------------------------------------------------------------------------------
 INPUT AND OUTPUT DEFINITIONS

 Inputs:
    L_x = input value (Word32)
    pOverflow = pointer to overflow flag

 Outputs:
    pOverflow -> if the Inv_sqrt operation resulted in an overflow.

 Returns:
    L_y = inverse squareroot of L_x (Word32)

 Global Variables Used:
    None.

 Local Variables Needed:
    None.

------------------------------------------------------------------------------
 FUNCTION DESCRIPTION

 This function computes 1/sqrt(L_x), where L_x is positive.
 If L_x is negative or zero, the result is 1 (3fff ffff).

 The function 1/sqrt(L_x) is approximated by a table and linear
 interpolation. The inverse square root is computed using the
 following steps:
    1- Normalization of L_x.
    2- If (30-exponent) is even then shift right once.
    3- exponent = (30-exponent)/2  +1
    4- i = bit25-b31 of L_x;  16<=i<=63  because of normalization.
    5- a = bit10-b24
    6- i -=16
    7- L_y = table[i]<<16 - (table[i] - table[i+1]) * a * 2
    8- L_y >>= exponent

------------------------------------------------------------------------------
 REQUIREMENTS

 None.

------------------------------------------------------------------------------
 REFERENCES

 inv_sqrt.c, UMTS GSM AMR speech codec, R99 - Version 3.2.0, March 2, 2001

------------------------------------------------------------------------------
 PSEUDO-CODE

Word32 Inv_sqrt (       // (o) : output value
    Word32 L_x          // (i) : input value
)
{
    Word16 exp, i, a, tmp;
    Word32 L_y;

* The reference ETSI code uses a global Overflow flag. In the actual
* implementation a pointer to the overflow flag is passed into the function.
* This pointer is in turn passed into the basic math functions such as add(),
* L_shl(), L_shr(), sub() called by this module.

    if (L_x <= (Word32) 0)
        return ((Word32) 0x3fffffffL);

    exp = norm_l (L_x);
    L_x = L_shl (L_x, exp);     // L_x is normalize

    exp = sub (30, exp);

    if ((exp & 1) == 0)         // If exponent even -> shift right
    {
        L_x = L_shr (L_x, 1);
    }
    exp = shr (exp, 1);
    exp = add (exp, 1);

    L_x = L_shr (L_x, 9);
    i = extract_h (L_x);        // Extract b25-b31
    L_x = L_shr (L_x, 1);
    a = extract_l (L_x);        // Extract b10-b24
    a = a & (Word16) 0x7fff;

    i = sub (i, 16);

    L_y = L_deposit_h (table[i]);       // table[i] << 16
    tmp = sub (table[i], table[i + 1]); // table[i] - table[i+1])
    L_y = L_msu (L_y, tmp, a);  // L_y -=  tmp*a*2

    L_y = L_shr (L_y, exp);     // denormalization

    return (L_y);
}

------------------------------------------------------------------------------
 CAUTION [optional]
 [State any special notes, constraints or cautions for users of this function]

------------------------------------------------------------------------------
*/

OSCL_EXPORT_REF Word32 Inv_sqrt(        /* (o) : output value   */
    Word32 L_x,         /* (i) : input value    */
    Flag   * pOverflow  /* (i) : pointer to overflow flag */
)
{
    Word16 exp;
    Word16 i;
    Word16 a;
    Word16 tmp;
    Word32 L_y;
    OSCL_UNUSED_ARG(pOverflow);

    if (L_x <= (Word32) 0)
    {
        return ((Word32) 0x3fffffffL);
    }

    exp = norm_l(L_x);
    L_x <<= exp;         /* L_x is normalize */

    exp = 30 - exp;

    if ((exp & 1) == 0)             /* If exponent even -> shift right */
    {
        L_x >>= 1;
    }
    exp >>= 1;
    exp += 1;

    L_x >>= 9;
    i = (Word16)(L_x >> 16);        /* Extract b25-b31 */
    a = (Word16)(L_x >> 1);         /* Extract b10-b24 */
    a &= (Word16) 0x7fff;

    i -= 16;

    L_y = (Word32)inv_sqrt_tbl[i] << 16;    /* inv_sqrt_tbl[i] << 16    */

    /* inv_sqrt_tbl[i] - inv_sqrt_tbl[i+1])  */
    tmp =  inv_sqrt_tbl[i] - inv_sqrt_tbl[i + 1];
    /* always a positive number less than 200 */

    L_y -= ((Word32)tmp * a) << 1;        /* L_y -=  tmp*a*2         */
    /* always a positive minus a small negative number */

    L_y >>= exp;                /* denormalization, exp always 0< exp < 31 */

    return (L_y);
}

