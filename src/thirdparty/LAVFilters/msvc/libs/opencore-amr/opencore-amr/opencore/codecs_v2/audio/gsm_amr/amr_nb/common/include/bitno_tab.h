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

 Filename: bitno_tab.h

------------------------------------------------------------------------------
 INCLUDE DESCRIPTION

 This file declares a tables in bitno_tab.c.

------------------------------------------------------------------------------
*/

/*----------------------------------------------------------------------------
; CONTINUE ONLY IF NOT ALREADY DEFINED
----------------------------------------------------------------------------*/
#ifndef BITNO_TAB_H
#define BITNO_TAB_H

/*----------------------------------------------------------------------------
; INCLUDES
----------------------------------------------------------------------------*/
#include "typedef.h"

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
#define BIT_0      0
#define BIT_1      1

#define PRMNO_MR475 17
#define PRMNO_MR515 19
#define PRMNO_MR59  19
#define PRMNO_MR67  19
#define PRMNO_MR74  19
#define PRMNO_MR795 23
#define PRMNO_MR102 39
#define PRMNO_MR122 57
#define PRMNO_MRDTX 5

    /* number of parameters to first subframe */
#define PRMNOFSF_MR475 7
#define PRMNOFSF_MR515 7
#define PRMNOFSF_MR59  7
#define PRMNOFSF_MR67  7
#define PRMNOFSF_MR74  7
#define PRMNOFSF_MR795 8
#define PRMNOFSF_MR102 12
#define PRMNOFSF_MR122 18
    /*----------------------------------------------------------------------------
    ; EXTERNAL VARIABLES REFERENCES
    ; Declare variables used in this module but defined elsewhere
    ----------------------------------------------------------------------------*/
    extern const Word16 prmno[];
    extern const Word16 prmnofsf[];
    extern const Word16 bitno_MR475[];
    extern const Word16 bitno_MR515[];
    extern const Word16 bitno_MR59[];
    extern const Word16 bitno_MR67[];
    extern const Word16 bitno_MR74[];
    extern const Word16 bitno_MR95[];
    extern const Word16 bitno_MR102[];
    extern const Word16 bitno_MR122[];
    extern const Word16 bitno_MRDTX[];
    extern const Word16 *const bitno[];

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
    ; END
    ----------------------------------------------------------------------------*/
#ifdef __cplusplus
}
#endif

#endif


