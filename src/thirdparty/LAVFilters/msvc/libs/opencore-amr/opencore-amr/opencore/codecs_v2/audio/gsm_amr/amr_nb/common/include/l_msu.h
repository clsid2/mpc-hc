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
 Filename: l_msu.h

------------------------------------------------------------------------------
 INCLUDE DESCRIPTION

 This file contains all the constant definitions and prototype definitions
 needed by the L_msu function.

------------------------------------------------------------------------------
*/

/*----------------------------------------------------------------------------
; CONTINUE ONLY IF NOT ALREADY DEFINED
----------------------------------------------------------------------------*/
#ifndef L_MSU_H
#define L_MSU_H

/*----------------------------------------------------------------------------
; INCLUDES
----------------------------------------------------------------------------*/
#include    "basicop_malloc.h"
#include    "l_mult.h"
#include    "l_sub.h"

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

    __inline Word32 L_msu(Word32 L_var3, Word16 var1, Word16 var2, Flag *pOverflow)
    {
        register Word32 ra = L_var3;
        register Word32 rb = var1;
        register Word32 rc = var2;
        Word32 product;
        Word32 result;

        OSCL_UNUSED_ARG(pOverflow);

        asm volatile("smulbb %0, %1, %2"
             : "=r"(product)
                             : "r"(rb), "r"(rc)
                            );

        asm volatile("qdsub %0, %1, %2"
             : "=r"(result)
                             : "r"(ra), "r"(product)
                            );

        return (result);
    }

#else /* C EQUIVALENT */

    static inline Word32 L_msu(Word32 L_var3, Word16 var1, Word16 var2, Flag *pOverflow)
    {
        Word32 result;

        result = L_mult(var1, var2, pOverflow);
        result = L_sub(L_var3, result, pOverflow);

        return (result);
    }

#endif
    /*----------------------------------------------------------------------------
    ; END
    ----------------------------------------------------------------------------*/

#ifdef __cplusplus
}
#endif

#endif /* _L_MSU_H_ */
