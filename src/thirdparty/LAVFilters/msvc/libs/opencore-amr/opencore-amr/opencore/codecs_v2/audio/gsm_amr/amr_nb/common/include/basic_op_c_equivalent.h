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

 Filename: basic_op_c_equivalent.h

------------------------------------------------------------------------------
 INCLUDE DESCRIPTION

 This file includes all the C-Equivalent basicop.c functions.

------------------------------------------------------------------------------
*/

/*----------------------------------------------------------------------------
; CONTINUE ONLY IF NOT ALREADY DEFINED
----------------------------------------------------------------------------*/
#ifndef BASIC_OP_C_EQUIVALENT_H
#define BASIC_OP_C_EQUIVALENT_H

/*----------------------------------------------------------------------------
; INCLUDES
----------------------------------------------------------------------------*/
#include    "basicop_malloc.h"

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

    /*----------------------------------------------------------------------------
    ; GLOBAL FUNCTION DEFINITIONS
    ; Function Prototype declaration
    ----------------------------------------------------------------------------*/


    /*
    ------------------------------------------------------------------------------
     FUNCTION NAME: L_add
    ------------------------------------------------------------------------------
     INPUT AND OUTPUT DEFINITIONS

     Inputs:
        L_var1 = 32 bit long signed integer (Word32) whose value falls
                 in the range : 0x8000 0000 <= L_var1 <= 0x7fff ffff.

        L_var2 = 32 bit long signed integer (Word32) whose value falls
                 in the range : 0x8000 0000 <= L_var1 <= 0x7fff ffff.

        pOverflow = pointer to overflow (Flag)

     Outputs:
        pOverflow -> 1 if the 32 bit add operation resulted in overflow

     Returns:
        L_sum = 32-bit sum of L_var1 and L_var2 (Word32)
    */
    static inline Word32 L_add(register Word32 L_var1, register Word32 L_var2, Flag *pOverflow)
    {
        Word32 L_sum;

        L_sum = L_var1 + L_var2;

        if ((L_var1 ^ L_var2) >= 0)
        {
            if ((L_sum ^ L_var1) >> 31)
            {
                L_sum = (L_var1 >> 31) ? MIN_32 : MAX_32;
                *pOverflow = 1;
            }
        }

        return (L_sum);
    }

    /*
    ------------------------------------------------------------------------------
     FUNCTION NAME: L_sub
    ------------------------------------------------------------------------------
     INPUT AND OUTPUT DEFINITIONS

     Inputs:
        L_var1 = 32 bit long signed integer (Word32) whose value falls
                 in the range : 0x8000 0000 <= L_var1 <= 0x7fff ffff.

        L_var2 = 32 bit long signed integer (Word32) whose value falls
                 in the range : 0x8000 0000 <= L_var1 <= 0x7fff ffff.

        pOverflow = pointer to overflow (Flag)

     Outputs:
        pOverflow -> 1 if the 32 bit add operation resulted in overflow

     Returns:
        L_diff = 32-bit difference of L_var1 and L_var2 (Word32)
    */
    static inline Word32 L_sub(register Word32 L_var1, register Word32 L_var2,
                               register Flag *pOverflow)
    {
        Word32 L_diff;

        L_diff = L_var1 - L_var2;

        if ((L_var1 ^ L_var2) >> 31)
        {
            if ((L_diff ^ L_var1) & MIN_32)
            {
                L_diff = (L_var1 >> 31) ? MIN_32 : MAX_32;
                *pOverflow = 1;
            }
        }

        return (L_diff);
    }


    /*
    ------------------------------------------------------------------------------
     FUNCTION NAME: L_mac
    ------------------------------------------------------------------------------
     INPUT AND OUTPUT DEFINITIONS

     Inputs:
        L_var3 = 32 bit long signed integer (Word32) whose value falls
                 in the range : 0x8000 0000 <= L_var3 <= 0x7fff ffff.
        var1 = 16 bit short signed integer (Word16) whose value falls in
               the range : 0xffff 8000 <= var1 <= 0x0000 7fff.
        var2 = 16 bit short signed integer (Word16) whose value falls in
               the range : 0xffff 8000 <= var2 <= 0x0000 7fff.

        pOverflow = pointer to overflow (Flag)

     Outputs:
        pOverflow -> 1 if the 32 bit add operation resulted in overflow

     Returns:
        result = 32-bit result of L_var3 + (var1 * var2)(Word32)
    */
    static inline Word32 L_mac(Word32 L_var3, Word16 var1, Word16 var2, Flag *pOverflow)
    {
        Word32 result;
        Word32 L_sum;
        result = (Word32) var1 * var2;
        if (result != (Word32) 0x40000000L)
        {
            L_sum = (result << 1) + L_var3;

            /* Check if L_sum and L_var_3 share the same sign */
            if ((L_var3 ^ result) > 0)
            {
                if ((L_sum ^ L_var3) >> 31)
                {
                    L_sum = (L_var3 >> 31) ? MIN_32 : MAX_32;
                    *pOverflow = 1;
                }
            }
        }
        else
        {
            *pOverflow = 1;
            L_sum = MAX_32;
        }
        return (L_sum);
    }

    /*
    ------------------------------------------------------------------------------
     FUNCTION NAME: L_mult
    ------------------------------------------------------------------------------
     INPUT AND OUTPUT DEFINITIONS

     Inputs:
        L_var1 = 16 bit short signed integer (Word16) whose value falls in
               the range : 0xffff 8000 <= var1 <= 0x0000 7fff.

        L_var2 = 16 bit short signed integer (Word16) whose value falls in
               the range : 0xffff 8000 <= var1 <= 0x0000 7fff.

        pOverflow = pointer to overflow (Flag)

     Outputs:
        pOverflow -> 1 if the 32 bit add operation resulted in overflow

     Returns:
        L_product = 32-bit product of L_var1 and L_var2 (Word32)
    */
    static inline Word32 L_mult(Word16 var1, Word16 var2, Flag *pOverflow)
    {
        register Word32 L_product;

        L_product = (Word32) var1 * var2;

        if (L_product != (Word32) 0x40000000L)
        {
            L_product <<= 1;          /* Multiply by 2 */
        }
        else
        {
            *pOverflow = 1;
            L_product = MAX_32;
        }

        return (L_product);
    }


    /*
    ------------------------------------------------------------------------------
     FUNCTION NAME: L_msu
    ------------------------------------------------------------------------------
     INPUT AND OUTPUT DEFINITIONS

     Inputs:
        L_var3 = 32 bit long signed integer (Word32) whose value falls
                 in the range : 0x8000 0000 <= L_var3 <= 0x7fff ffff.

        var1 = 16 bit short signed integer (Word16) whose value falls in
               the range : 0xffff 8000 <= var1 <= 0x0000 7fff.
        var2 = 16 bit short signed integer (Word16) whose value falls in
               the range : 0xffff 8000 <= var2 <= 0x0000 7fff.

        pOverflow = pointer to overflow (Flag)

     Outputs:
        pOverflow -> 1 if the 32 bit operation resulted in overflow

     Returns:
        result = 32-bit result of L_var3 - (var1 * var2)
    */

    static inline Word32 L_msu(Word32 L_var3, Word16 var1, Word16 var2, Flag *pOverflow)
    {
        Word32 result;

        result = L_mult(var1, var2, pOverflow);
        result = L_sub(L_var3, result, pOverflow);

        return (result);
    }

    /*
    ------------------------------------------------------------------------------
     FUNCTION NAME: Mpy_32
    ------------------------------------------------------------------------------
     INPUT AND OUTPUT DEFINITIONS

     Inputs:
        L_var1_hi = most significant word of first input (Word16).
        L_var1_lo = least significant word of first input (Word16).
        L_var2_hi = most significant word of second input (Word16).
        L_var2_lo = least significant word of second input (Word16).

        pOverflow = pointer to overflow (Flag)

     Outputs:
        pOverflow -> 1 if the 32 bit multiply operation resulted in overflow

     Returns:
        L_product = 32-bit product of L_var1 and L_var2 (Word32)
    */
    static inline Word32 Mpy_32(Word16 L_var1_hi,
                                Word16 L_var1_lo,
                                Word16 L_var2_hi,
                                Word16 L_var2_lo,
                                Flag   *pOverflow)
    {
        Word32 L_product;
        Word32 L_sum;
        Word32 product32;

        OSCL_UNUSED_ARG(pOverflow);
        L_product = (Word32) L_var1_hi * L_var2_hi;

        if (L_product != (Word32) 0x40000000L)
        {
            L_product <<= 1;
        }
        else
        {
            L_product = MAX_32;
        }

        /* result = mult (L_var1_hi, L_var2_lo, pOverflow); */
        product32 = ((Word32) L_var1_hi * L_var2_lo) >> 15;

        /* L_product = L_mac (L_product, result, 1, pOverflow); */
        L_sum = L_product + (product32 << 1);

        if ((L_product ^ product32) > 0)
        {
            if ((L_sum ^ L_product) >> 31)
            {
                L_sum = (L_product >> 31) ? MIN_32 : MAX_32;
            }
        }

        L_product = L_sum;

        /* result = mult (L_var1_lo, L_var2_hi, pOverflow); */
        product32 = ((Word32) L_var1_lo * L_var2_hi) >> 15;

        /* L_product = L_mac (L_product, result, 1, pOverflow); */
        L_sum = L_product + (product32 << 1);

        if ((L_product ^ product32) > 0)
        {
            if ((L_sum ^ L_product) >> 31)
            {
                L_sum = (L_product >> 31) ? MIN_32 : MAX_32;
            }
        }
        return (L_sum);
    }

    /*
    ------------------------------------------------------------------------------
     FUNCTION NAME: Mpy_32_16
    ------------------------------------------------------------------------------
     INPUT AND OUTPUT DEFINITIONS

     Inputs:
        L_var1_hi = most significant 16 bits of 32-bit input (Word16).
        L_var1_lo = least significant 16 bits of 32-bit input (Word16).
        var2  = 16-bit signed integer (Word16).

        pOverflow = pointer to overflow (Flag)

     Outputs:
        pOverflow -> 1 if the 32 bit product operation resulted in overflow

     Returns:
        product = 32-bit product of the 32-bit L_var1 and 16-bit var1 (Word32)
    */

    static inline Word32 Mpy_32_16(Word16 L_var1_hi,
                                   Word16 L_var1_lo,
                                   Word16 var2,
                                   Flag *pOverflow)
    {

        Word32 L_product;
        Word32 L_sum;
        Word32 result;
        L_product = (Word32) L_var1_hi * var2;

        if (L_product != (Word32) 0x40000000L)
        {
            L_product <<= 1;
        }
        else
        {
            *pOverflow = 1;
            L_product = MAX_32;
        }

        result = ((Word32)L_var1_lo * var2) >> 15;

        L_sum  =  L_product + (result << 1);

        if ((L_product ^ result) > 0)
        {
            if ((L_sum ^ L_product) >> 31)
            {
                L_sum = (L_product >> 31) ? MIN_32 : MAX_32;
                *pOverflow = 1;
            }
        }
        return (L_sum);

    }

    /*
    ------------------------------------------------------------------------------
     FUNCTION NAME: mult
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
        product = 16-bit limited product of var1 and var2 (Word16)
    */
    static inline Word16 mult(Word16 var1, Word16 var2, Flag *pOverflow)
    {
        register Word32 product;

        product = ((Word32) var1 * var2) >> 15;

        /* Saturate result (if necessary). */
        /* var1 * var2 >0x00007fff is the only case */
        /* that saturation occurs. */

        if (product > 0x00007fffL)
        {
            *pOverflow = 1;
            product = (Word32) MAX_16;
        }


        /* Return the product as a 16 bit value by type casting Word32 to Word16 */

        return ((Word16) product);
    }


    static inline Word32 amrnb_fxp_mac_16_by_16bb(Word32 L_var1, Word32 L_var2, Word32 L_var3)
    {
        Word32 result;

        result = L_var3 + L_var1 * L_var2;

        return result;
    }

    static inline Word32 amrnb_fxp_msu_16_by_16bb(Word32 L_var1, Word32 L_var2, Word32 L_var3)
    {
        Word32 result;

        result = L_var3 - L_var1 * L_var2;

        return result;
    }


    /*----------------------------------------------------------------------------
    ; END
    ----------------------------------------------------------------------------*/
#ifdef __cplusplus
}
#endif

#endif /* BASIC_OP_C_EQUIVALENT_H */



