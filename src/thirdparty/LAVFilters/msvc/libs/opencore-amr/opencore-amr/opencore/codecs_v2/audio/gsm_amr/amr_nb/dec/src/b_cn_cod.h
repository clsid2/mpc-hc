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
------------------------------------------------------------------------------



 Filename: b_cn_cod.h

------------------------------------------------------------------------------
 INCLUDE DESCRIPTION

 This file contains all the constant definitions and prototype definitions
 needed by the comfort noise(CN) generator functions

------------------------------------------------------------------------------
*/

#ifndef B_CN_COD_H
#define B_CN_COD_H

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

    /*----------------------------------------------------------------------------

    ; FUNCTION NAME: pseudonoise
    ;
    ; PURPOSE: Generate a random integer value to use in comfort noise
    ;          generation. The algorithm uses polynomial x^31 + x^3 + 1
    ;          (length of PN sequence is 2^31 - 1).
    ;
    ----------------------------------------------------------------------------*/

    Word16 pseudonoise(
        Word32 *pShift_reg,     /* i/o : Old CN generator shift register state */
        Word16 no_bits          /* i   : Number of bits                        */
    );

    /*----------------------------------------------------------------------------

    ; FUNCTION NAME: build_CN_code
    ;
    ; PURPOSE: Compute the comfort noise fixed codebook excitation. The
    ;          gains of the pulses are always +/-1.
    ;
    ----------------------------------------------------------------------------*/

    void build_CN_code(
        Word32 *pSeed,          /* i/o : Old CN generator shift register state  */
        Word16 cod[],           /* o   : Generated CN fixed codebook vector     */
        Flag   *pOverflow       /* i/o : Overflow flag                          */
    );

    /*----------------------------------------------------------------------------

    ; FUNCTION NAME: build_CN_param
    ;
    ; PURPOSE: Randomize the speech parameters. So that they
    ;          do not produce tonal artifacts if used by ECU.
    ;
    ----------------------------------------------------------------------------*/

    void build_CN_param(
        Word16 *pSeed,          /* i/o : Old CN generator shift register state  */
        const Word16 n_param,               /* i  : number of params            */
        const Word16 param_size_table[],    /* i : size of params               */
        Word16 parm[],                  /* o : CN Generated params              */
        const Word16* window_200_40_ptr, /* i : ptr to read-only table          */
        Flag  *pOverflow                /* i/o : Overflow Flag                  */
    );

    /*----------------------------------------------------------------------------
    ; END
    ----------------------------------------------------------------------------*/
#ifdef __cplusplus
}
#endif

#endif /* _B_CN_COD_H_ */

