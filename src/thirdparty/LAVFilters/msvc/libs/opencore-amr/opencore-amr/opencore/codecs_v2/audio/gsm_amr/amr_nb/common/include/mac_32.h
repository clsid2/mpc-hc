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

 Filename: mac_32.h

------------------------------------------------------------------------------
 INCLUDE DESCRIPTION

 This file contains all the constant definitions and prototype definitions
 needed by the Mac_32 and Mac_32_16 functions

------------------------------------------------------------------------------
*/

/*----------------------------------------------------------------------------
; CONTINUE ONLY IF NOT ALREADY DEFINED
----------------------------------------------------------------------------*/
#ifndef MAC_32_H
#define MAC_32_H

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

    static inline Word32 Mac_32(Word32 L_var3,
    Word16 L_var1_hi,
    Word16 L_var1_lo,
    Word16 L_var2_hi,
    Word16 L_var2_lo,
    Flag *pOverflow)
    {
        Word16  product;

        L_var3 = L_mac(L_var3, L_var1_hi, L_var2_hi, pOverflow);

        product = mult(L_var1_hi, L_var2_lo, pOverflow);
        L_var3 = L_mac(L_var3, product, 1, pOverflow);

        product = mult(L_var1_lo, L_var2_hi, pOverflow);
        L_var3 = L_mac(L_var3, product, 1, pOverflow);

        return (L_var3);
    }

    static inline Word32 Mac_32_16(Word32 L_var3,
                                   Word16 L_var1_hi,
                                   Word16 L_var1_lo,
                                   Word16 var2,
                                   Flag  *pOverflow)
    {
        Word16  product;

        L_var3 = L_mac(L_var3, L_var1_hi, var2, pOverflow);

        product = mult(L_var1_lo, var2, pOverflow);
        L_var3 = L_mac(L_var3, product, 1, pOverflow);

        return (L_var3);
    }

    /*----------------------------------------------------------------------------
    ; END
    ----------------------------------------------------------------------------*/
#ifdef __cplusplus
}
#endif

#endif /* _MAC_32_H_ */


