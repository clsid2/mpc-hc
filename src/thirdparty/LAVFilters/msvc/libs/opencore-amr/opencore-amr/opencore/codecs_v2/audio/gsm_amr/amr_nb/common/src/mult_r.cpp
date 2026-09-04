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

 Filename: mult_r.cpp

------------------------------------------------------------------------------
 MODULE DESCRIPTION

 Multiplication function with rounding and overflow control

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
 FUNCTION NAME: mult_r
------------------------------------------------------------------------------
 INPUT AND OUTPUT DEFINITIONS

 Inputs:
    var1 = 16 bit short signed integer (Word16) whose value falls in
           the range : 0xffff 8000 <= var1 <= 0x0000 7fff.

    var2 = 16 bit short signed integer (Word16) whose value falls in
           the range : 0xffff 8000 <= var2 <= 0x0000 7fff.

    pOverflow = pointer to overflow (Flag)

 Outputs:
    pOverflow -> 1 if the add operation resulted in overflow

 Returns:
    L_product_arr = 16-bit limited product of var1 and var2 (Word16)

 Global Variables Used:
    None

 Local Variables Needed:
    None

------------------------------------------------------------------------------
 FUNCTION DESCRIPTION

 This function performs the multiplication of var1 by var2 with rounding, and
 gives a 16 bit result which is scaled, i.e.:
    mult_r(var1,var2) = extract_l(L_shr(((var1 * var2) + 16384),15)) and  |
    mult_r(-32768,-32768) = 32767

------------------------------------------------------------------------------
 REQUIREMENTS

 None

------------------------------------------------------------------------------
 REFERENCES

 [1] mult_r() function in basicop2.c, UMTS GSM AMR speech codec, R99 -
 Version 3.2.0, March 2, 2001

------------------------------------------------------------------------------
 PSEUDO-CODE

Word16 mult_r (Word16 var1, Word16 var2)
{
    Word16 var_out;
    Word32 L_product_arr;

    L_product_arr = (Word32) var1 *(Word32) var2;
    L_product_arr += (Word32) 0x00004000L;
    L_product_arr &= (Word32) 0xffff8000L;
    L_product_arr >>= 15;

    if (L_product_arr & (Word32) 0x00010000L)
    {
        L_product_arr |= (Word32) 0xffff0000L;
    }
* The reference ETSI code uses a global flag for Overflow inside the function
* saturate(). In the actual implementation a pointer to Overflow flag is passed in
* as a parameter to the function

    var_out = saturate (L_product_arr);

#if (WMOPS)
    multiCounter[currCounter].mult_r++;
#endif

    return (var_out);
}

------------------------------------------------------------------------------
 CAUTION [optional]
 [State any special notes, constraints or cautions for users of this function]

------------------------------------------------------------------------------
*/

/*----------------------------------------------------------------------------
; FUNCTION CODE
----------------------------------------------------------------------------*/

OSCL_EXPORT_REF Word16 mult_r(Word16 var1, Word16 var2, Flag *pOverflow)
{

    register Word32 L_product_arr;

    L_product_arr = ((Word32) var1) * var2;              /* product */
    L_product_arr += (Word32) 0x00004000L;               /* round */
    L_product_arr >>= 15;                                /* shift */

    /* sign extend when necessary */
    L_product_arr |= (Word32) - (L_product_arr & (Word32) 0x00010000L);

    /* Saturate result (if necessary). */
    /* Replaced function call with in-line code to conserve MIPS, */
    /* i.e., var_out = saturate (L_product_arr)  */

    if (L_product_arr > 0X00007fffL)
    {
        *pOverflow = 1;
        L_product_arr = MAX_16;
    }
    else if (L_product_arr < (Word32) - 32768)
    {
        *pOverflow = 1;
        L_product_arr = MIN_16;
    }

    return ((Word16) L_product_arr);
}
