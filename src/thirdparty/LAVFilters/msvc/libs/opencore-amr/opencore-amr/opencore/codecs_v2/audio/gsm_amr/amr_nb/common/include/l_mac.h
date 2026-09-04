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
 Filename: l_mac.h

------------------------------------------------------------------------------
 INCLUDE DESCRIPTION

 This file contains all the constant definitions and prototype definitions
 needed by the L_mac function.

------------------------------------------------------------------------------
*/

/*----------------------------------------------------------------------------
; CONTINUE ONLY IF NOT ALREADY DEFINED
----------------------------------------------------------------------------*/
#ifndef L_MAC_H
#define L_MAC_H

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

    static inline Word32 L_mac(Word32 L_var3, Word16 var1, Word16 var2, Flag *pOverflow)
    {
        register Word32 ra = L_var3;
        register Word32 rb = var1;
        register Word32 rc = var2;
        Word32 result;

        OSCL_UNUSED_ARG(pOverflow);

        asm volatile("smulbb %0, %1, %2"
             : "=r"(result)
                             : "r"(rb), "r"(rc)
                            );

        asm volatile("qdadd %0, %1, %2"
             : "=r"(rc)
                             : "r"(ra), "r"(result)
                            );

        return (rc);
    }

#else /* C_EQUIVALENT */

    __inline Word32 L_mac(Word32 L_var3, Word16 var1, Word16 var2, Flag *pOverflow)
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
                if ((L_sum ^ L_var3) < 0)
                {
                    L_sum = (L_var3 < 0) ? MIN_32 : MAX_32;
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

#endif
    /*----------------------------------------------------------------------------
    ; END
    ----------------------------------------------------------------------------*/
#ifdef __cplusplus
}
#endif

#endif /* _L_MAC_H_ */


