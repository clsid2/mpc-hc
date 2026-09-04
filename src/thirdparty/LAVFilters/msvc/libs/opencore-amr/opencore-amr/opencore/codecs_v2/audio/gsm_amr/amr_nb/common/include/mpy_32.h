/* ------------------------------------------------------------------
 * Copyright (C) 1998-2010 PacketVideo
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

 Filename: mpy_32.h

------------------------------------------------------------------------------
 INCLUDE DESCRIPTION

 This file contains all the constant definitions and prototype definitions
 needed by the Mpy_32 function.

------------------------------------------------------------------------------
*/

/*----------------------------------------------------------------------------
; CONTINUE ONLY IF NOT ALREADY DEFINED
----------------------------------------------------------------------------*/
#ifndef MPY_32_H
#define MPY_32_H

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
#if   ((PV_CPU_ARCH_VERSION >=5) && (PV_COMPILER == EPV_ARM_GNUC))/* Instructions for ARM-linux cross-compiler*/

    static inline Word32 Mpy_32(Word16 L_var1_hi,
    Word16 L_var1_lo,
    Word16 L_var2_hi,
    Word16 L_var2_lo,
    Flag   *pOverflow)
    {
        register Word32 product32;
        register Word32 L_sum;
        register Word32 L_product, result;
        register Word32 ra = L_var1_hi;
        register Word32 rb = L_var1_lo;
        register Word32 rc = L_var2_hi;
        register Word32 rd = L_var2_lo;



        OSCL_UNUSED_ARG(pOverflow);

        asm volatile("smulbb %0, %1, %2"
             : "=r"(L_product)
                             : "r"(ra), "r"(rc)
                            );
        asm volatile("mov %0, #0"
             : "=r"(result)
                    );

        asm volatile("qdadd %0, %1, %2"
             : "=r"(L_sum)
                             : "r"(result), "r"(L_product)
                            );

        asm volatile("smulbb %0, %1, %2"
             : "=r"(product32)
                             : "r"(ra), "r"(rd)
                            );

        asm volatile("mov %0, %1, ASR #15"
             : "=r"(ra)
                             : "r"(product32)
                            );
        asm volatile("qdadd %0, %1, %2"
             : "=r"(L_product)
                             : "r"(L_sum), "r"(ra)
                            );

        asm volatile("smulbb %0, %1, %2"
             : "=r"(product32)
                             : "r"(rb), "r"(rc)
                            );

        asm volatile("mov %0, %1, ASR #15"
             : "=r"(rb)
                             : "r"(product32)
                            );

        asm volatile("qdadd %0, %1, %2"
             : "=r"(L_sum)
                             : "r"(L_product), "r"(rb)
                            );

        return (L_sum);
    }

#else /* C_EQUIVALENT */

    __inline Word32 Mpy_32(Word16 L_var1_hi,
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
            if ((L_sum ^ L_product) < 0)
            {
                L_sum = (L_product < 0) ? MIN_32 : MAX_32;
            }
        }

        L_product = L_sum;

        /* result = mult (L_var1_lo, L_var2_hi, pOverflow); */
        product32 = ((Word32) L_var1_lo * L_var2_hi) >> 15;

        /* L_product = L_mac (L_product, result, 1, pOverflow); */
        L_sum = L_product + (product32 << 1);

        if ((L_product ^ product32) > 0)
        {
            if ((L_sum ^ L_product) < 0)
            {
                L_sum = (L_product < 0) ? MIN_32 : MAX_32;
            }
        }

        /*----------------------------------------------------------------------------
        ; Return nothing or data or data pointer
        ----------------------------------------------------------------------------*/
        return (L_sum);
    }

#endif
    /*----------------------------------------------------------------------------
    ; END
    ----------------------------------------------------------------------------*/
#ifdef __cplusplus
}
#endif

#endif /* _MPY_32_H_ */
