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

 Filename: mpy_32_16.h

------------------------------------------------------------------------------
 INCLUDE DESCRIPTION

 This file contains all the constant definitions and prototype definitions
 needed by the Mpy_32_16 function.

------------------------------------------------------------------------------
*/

/*----------------------------------------------------------------------------
; CONTINUE ONLY IF NOT ALREADY DEFINED
----------------------------------------------------------------------------*/
#ifndef MPY_32_16_H
#define MPY_32_16_H

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

    static inline Word32 Mpy_32_16(Word16 L_var1_hi,
    Word16 L_var1_lo,
    Word16 var2,
    Flag *pOverflow)
    {

        register Word32 ra = L_var1_hi;
        register Word32 rb = L_var1_lo;
        register Word32 rc = var2;
        Word32 result, L_product;

        OSCL_UNUSED_ARG(pOverflow);

        __asm__ volatile("smulbb %0, %1, %2"
             : "=r"(L_product)
                             : "r"(ra), "r"(rc)
                            );
        __asm__ volatile("mov %0, #0"
             : "=r"(result)
                    );

        __asm__ volatile("qdadd %0, %1, %2"
             : "=r"(L_product)
                             : "r"(result), "r"(L_product)
                            );

        __asm__ volatile("smulbb %0, %1, %2"
             : "=r"(result)
                             : "r"(rb), "r"(rc)
                            );

        __asm__ volatile("mov %0, %1, ASR #15"
             : "=r"(ra)
                             : "r"(result)
                            );
        __asm__ volatile("qdadd %0, %1, %2"
             : "=r"(result)
                             : "r"(L_product), "r"(ra)
                            );

        return (result);
    }

#else /* C_EQUIVALENT */
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
            if ((L_sum ^ L_product) < 0)
            {
                L_sum = (L_product < 0) ? MIN_32 : MAX_32;
                *pOverflow = 1;
            }
        }
        return (L_sum);

    }

#endif
    /*----------------------------------------------------------------------------
    ; END
    ----------------------------------------------------------------------------*/
#ifdef __cplusplus
}
#endif

#endif /* _MPY_32_16_H_ */


