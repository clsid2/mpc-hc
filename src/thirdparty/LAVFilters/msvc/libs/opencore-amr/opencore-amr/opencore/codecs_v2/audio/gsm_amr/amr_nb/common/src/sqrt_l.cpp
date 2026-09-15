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

 Filename: sqrt_l.cpp

------------------------------------------------------------------------------
*/

/*----------------------------------------------------------------------------
; INCLUDES
----------------------------------------------------------------------------*/
#include    "sqrt_l.h"
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
 FUNCTION NAME: sqrt_l_exp
------------------------------------------------------------------------------
 INPUT AND OUTPUT DEFINITIONS

 Inputs:
    L_x = input value (Word32)
    pExp = pointer to right shift to be applied to result
    pOverflow = pointer to overflow flag

 Outputs:
    pOverflow -> if the Inv_sqrt operation resulted in an overflow.

 Returns:
    L_y = squareroot of L_x (Word32)

 Global Variables Used:
    None.

 Local Variables Needed:
    None.

------------------------------------------------------------------------------
 FUNCTION DESCRIPTION

 This function computes sqrt(L_x),  where  L_x is positive.
 If L_var is negative or zero, the result is 0

 The function sqrt(L_x) is approximated by a table and linear
 interpolation. The square root is computed using the
 following steps:
    1- Normalization of L_x.
    2- If exponent is even then shift right once.
    3- exponent = exponent/2
    4- i = bit25-b31 of L_x;  16<=i<=63  because of normalization.
    5- a = bit10-b24
    6- i -=16
    7- L_y = table[i]<<16 - (table[i] - table[i+1]) * a * 2
    8- return L_y and exponent so caller can do denormalization

------------------------------------------------------------------------------
 REQUIREMENTS

 None.

------------------------------------------------------------------------------
 REFERENCES

 sqrt_l.c, UMTS GSM AMR speech codec, R99 - Version 3.2.0, March 2, 2001

------------------------------------------------------------------------------
 PSEUDO-CODE

Word32 sqrt_l_exp (     // o : output value
    Word32 L_x,         // i : input value
    Word16 *exp         // o : right shift to be applied to result
)
{

//          y = sqrt(x)
//          x = f * 2^-e,   0.5 <= f < 1   (normalization)
//          y = sqrt(f) * 2^(-e/2)
//
//          a) e = 2k   --> y = sqrt(f)   * 2^-k  (k = e div 2,
//                                                 0.707 <= sqrt(f) < 1)
//          b) e = 2k+1 --> y = sqrt(f/2) * 2^-k  (k = e div 2,
                                                 0.5 <= sqrt(f/2) < 0.707)


    Word16 e, i, a, tmp;
    Word32 L_y;

    if (L_x <= (Word32) 0)
    {
        *exp = 0;
        return (Word32) 0;
    }

* The reference ETSI code uses a global Overflow flag. In the actual
* implementation a pointer to the overflow flag is passed into the function.
* This pointer is in turn passed into the basic math functions such as add(),
* L_shl(), L_shr(), sub() called by this module.

    e = norm_l (L_x) & 0xFFFE;              // get next lower EVEN norm. exp
    L_x = L_shl (L_x, e);                   // L_x is normalized to [0.25..1)
    *exp = e;                               // return 2*exponent (or Q1)

    L_x = L_shr (L_x, 9);
    i = extract_h (L_x);                    // Extract b25-b31, 16 <= i <= 63
                                                because of normalization
    L_x = L_shr (L_x, 1);
    a = extract_l (L_x);                    // Extract b10-b24
    a = a & (Word16) 0x7fff;

    i = sub (i, 16);                        // 0 <= i <= 47

    L_y = L_deposit_h (table[i]);           // table[i] << 16
    tmp = sub (table[i], table[i + 1]);     // table[i] - table[i+1])
    L_y = L_msu (L_y, tmp, a);              // L_y -= tmp*a*2

    return (L_y);
}

------------------------------------------------------------------------------
 CAUTION [optional]
 [State any special notes, constraints or cautions for users of this function]

------------------------------------------------------------------------------
*/

OSCL_EXPORT_REF Word32 sqrt_l_exp(      /* o : output value,          Q31 */
    Word32 L_x,         /* i : input value,                           Q31 */
    Word16 *pExp,       /* o : right shift to be applied to result,   Q1  */
    Flag   *pOverflow   /* i : pointer to overflow flag */
)

{
    Word16 e;
    Word16 i;
    Word16 a;
    Word16 tmp;
    Word32 L_y;

    /*
          y = sqrt(x)
          x = f * 2^-e,   0.5 <= f < 1   (normalization)
          y = sqrt(f) * 2^(-e/2)
          a) e = 2k   --> y = sqrt(f)   * 2^-k  (k = e div 2,
                                                 0.707 <= sqrt(f) < 1)
          b) e = 2k+1 --> y = sqrt(f/2) * 2^-k  (k = e div 2,
                                                 0.5 <= sqrt(f/2) < 0.707)
     */

    if (L_x <= (Word32) 0)
    {
        *pExp = 0;
        return (Word32) 0;
    }

    e = norm_l(L_x) & 0xFFFE;               /* get next lower EVEN norm. exp  */
    L_x = L_shl(L_x, e, pOverflow);         /* L_x is normalized to [0.25..1) */
    *pExp = e;                              /* return 2*exponent (or Q1)      */

    L_x >>= 10;
    i = (Word16)(L_x >> 15) & 63;            /* Extract b25-b31, 16<= i <=63  */
    /* because of normalization       */

    a = (Word16)(L_x);                      /* Extract b10-b24 */
    a &= (Word16) 0x7fff;

    if (i > 15)
    {
        i -= 16;                              /* 0 <= i <= 47                   */
    }

    L_y = ((Word32) sqrt_l_tbl[i] << 16);      /* sqrt_l_tbl[i] << 16            */

    /* sqrt_l_tbl[i] - sqrt_l_tbl[i+1]) */
    tmp = sqrt_l_tbl[i] - sqrt_l_tbl[i + 1];

    L_y = L_msu(L_y, tmp, a, pOverflow);    /* L_y -= tmp*a*2                 */

    /* L_y = L_shr (L_y, *exp); */          /* denormalization done by caller */

    return (L_y);
}

