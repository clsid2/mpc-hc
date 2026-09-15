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

 Filename: mult.h

------------------------------------------------------------------------------
 INCLUDE DESCRIPTION

 This file contains all the constant definitions and prototype definitions
 needed by the mult function.

------------------------------------------------------------------------------
*/

#ifndef MULT_H
#define MULT_H

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
#if   ((PV_CPU_ARCH_VERSION >=5) && (PV_COMPILER == EPV_ARM_GNUC))

    __inline Word16 mult(Word16 var1, Word16 var2, Flag *pOverflow)
    {
        register Word32 ra = var1;
        register Word32 rb = var2;
        Word32 product;
        Word32 temp = 0x7FFF;

        OSCL_UNUSED_ARG(pOverflow);

        asm volatile("smulbb %0, %1, %2"
             : "=r"(product)
                             : "r"(ra), "r"(rb)
                            );
        asm volatile("mov %0, %1, ASR #15"
             : "=r"(product)
                             : "r"(product)
                            );
        asm volatile("cmp %0, %1"
             : "=r"(product)
                             : "r"(temp)
                            );
        asm volatile("movge %0, %1"
             : "=r"(product)
                             : "r"(temp)
                            );

        return ((Word16) product);
    }

#else /* C EQUIVALENT */

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

#endif
    /*----------------------------------------------------------------------------
    ; END
    ----------------------------------------------------------------------------*/
#ifdef __cplusplus
}
#endif

#endif  /* _MULT_H_ */

