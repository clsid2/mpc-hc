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

 Filename: bitno_tab.cpp

------------------------------------------------------------------------------
 INPUT AND OUTPUT DEFINITIONS

 Inputs:
    None

 Local Stores/Buffers/Pointers Needed:
    None

 Global Stores/Buffers/Pointers Needed:
    None

 Outputs:
    None

 Pointers and Buffers Modified:
    None

 Local Stores Modified:
    None

 Global Stores Modified:
    None

------------------------------------------------------------------------------
 FUNCTION DESCRIPTION

      File             : bitno.tab
      Purpose          : Tables for bit2prm and prm2bit

------------------------------------------------------------------------------
 REQUIREMENTS

 None

------------------------------------------------------------------------------
 REFERENCES

 None

------------------------------------------------------------------------------
 PSEUDO-CODE


------------------------------------------------------------------------------
*/


/*----------------------------------------------------------------------------
; INCLUDES
----------------------------------------------------------------------------*/
#include "typedef.h"
#include "cnst.h"   /* parameter sizes: MAX_PRM_SIZE */
#include "mode.h"   /* N_MODES */
#include "bitno_tab.h"


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
    ; Include all pre-processor statements here. Include conditional
    ; compile variables also.
    ----------------------------------------------------------------------------*/

    /*----------------------------------------------------------------------------
    ; LOCAL FUNCTION DEFINITIONS
    ; Function Prototype declaration
    ----------------------------------------------------------------------------*/


    /*----------------------------------------------------------------------------
    ; LOCAL STORE/BUFFER/POINTER DEFINITIONS
    ; Variable declaration - defined here and used outside this module
    ----------------------------------------------------------------------------*/
    /* number of parameters per modes (values must be <= MAX_PRM_SIZE!) */
    const Word16 prmno[N_MODES] =
    {
        PRMNO_MR475,
        PRMNO_MR515,
        PRMNO_MR59,
        PRMNO_MR67,
        PRMNO_MR74,
        PRMNO_MR795,
        PRMNO_MR102,
        PRMNO_MR122,
        PRMNO_MRDTX
    };

    /* number of parameters to first subframe per modes */
    const Word16 prmnofsf[N_MODES - 1] =
    {
        PRMNOFSF_MR475,
        PRMNOFSF_MR515,
        PRMNOFSF_MR59,
        PRMNOFSF_MR67,
        PRMNOFSF_MR74,
        PRMNOFSF_MR795,
        PRMNOFSF_MR102,
        PRMNOFSF_MR122
    };

    /* parameter sizes (# of bits), one table per mode */
    const Word16 bitno_MR475[PRMNO_MR475] =
    {
        8, 8, 7,                                 /* LSP VQ          */
        8, 7, 2, 8,                              /* first subframe  */
        4, 7, 2,                                 /* second subframe */
        4, 7, 2, 8,                              /* third subframe  */
        4, 7, 2,                                 /* fourth subframe */
    };

    const Word16 bitno_MR515[PRMNO_MR515] =
    {
        8, 8, 7,                                 /* LSP VQ          */
        8, 7, 2, 6,                              /* first subframe  */
        4, 7, 2, 6,                              /* second subframe */
        4, 7, 2, 6,                              /* third subframe  */
        4, 7, 2, 6,                              /* fourth subframe */
    };

    const Word16 bitno_MR59[PRMNO_MR59] =
    {
        8, 9, 9,                                 /* LSP VQ          */
        8, 9, 2, 6,                              /* first subframe  */
        4, 9, 2, 6,                              /* second subframe */
        8, 9, 2, 6,                              /* third subframe  */
        4, 9, 2, 6,                              /* fourth subframe */
    };

    const Word16 bitno_MR67[PRMNO_MR67] =
    {
        8, 9, 9,                                 /* LSP VQ          */
        8, 11, 3, 7,                             /* first subframe  */
        4, 11, 3, 7,                             /* second subframe */
        8, 11, 3, 7,                             /* third subframe  */
        4, 11, 3, 7,                             /* fourth subframe */
    };

    const Word16 bitno_MR74[PRMNO_MR74] =
    {
        8, 9, 9,                                 /* LSP VQ          */
        8, 13, 4, 7,                             /* first subframe  */
        5, 13, 4, 7,                             /* second subframe */
        8, 13, 4, 7,                             /* third subframe  */
        5, 13, 4, 7,                             /* fourth subframe */
    };

    const Word16 bitno_MR795[PRMNO_MR795] =
    {
        9, 9, 9,                                 /* LSP VQ          */
        8, 13, 4, 4, 5,                          /* first subframe  */
        6, 13, 4, 4, 5,                          /* second subframe */
        8, 13, 4, 4, 5,                          /* third subframe  */
        6, 13, 4, 4, 5,                          /* fourth subframe */
    };

    const Word16 bitno_MR102[PRMNO_MR102] =
    {
        8, 9, 9,                                 /* LSP VQ          */
        8, 1, 1, 1, 1, 10, 10, 7, 7,             /* first subframe  */
        5, 1, 1, 1, 1, 10, 10, 7, 7,             /* second subframe */
        8, 1, 1, 1, 1, 10, 10, 7, 7,             /* third subframe  */
        5, 1, 1, 1, 1, 10, 10, 7, 7,             /* fourth subframe */
    };

    const Word16 bitno_MR122[PRMNO_MR122] =
    {
        7, 8, 9, 8, 6,                           /* LSP VQ          */
        9, 4, 4, 4, 4, 4, 4, 3, 3, 3, 3, 3, 5,   /* first subframe  */
        6, 4, 4, 4, 4, 4, 4, 3, 3, 3, 3, 3, 5,   /* second subframe */
        9, 4, 4, 4, 4, 4, 4, 3, 3, 3, 3, 3, 5,   /* third subframe  */
        6, 4, 4, 4, 4, 4, 4, 3, 3, 3, 3, 3, 5    /* fourth subframe */
    };

    const Word16 bitno_MRDTX[PRMNO_MRDTX] =
    {
        3,
        8, 9, 9,
        6
    };

    /* overall table with all parameter sizes for all modes */
    const Word16 * const bitno[N_MODES] =
    {
        bitno_MR475,
        bitno_MR515,
        bitno_MR59,
        bitno_MR67,
        bitno_MR74,
        bitno_MR795,
        bitno_MR102,
        bitno_MR122,
        bitno_MRDTX
    };
    /*----------------------------------------------------------------------------
    ; EXTERNAL FUNCTION REFERENCES
    ; Declare functions defined elsewhere and referenced in this module
    ----------------------------------------------------------------------------*/


    /*----------------------------------------------------------------------------
    ; EXTERNAL GLOBAL STORE/BUFFER/POINTER REFERENCES
    ; Declare variables used in this module but defined elsewhere
    ----------------------------------------------------------------------------*/


    /*--------------------------------------------------------------------------*/
#ifdef __cplusplus
}
#endif

/*----------------------------------------------------------------------------
; FUNCTION CODE
----------------------------------------------------------------------------*/

/*----------------------------------------------------------------------------
; Define all local variables
----------------------------------------------------------------------------*/


/*----------------------------------------------------------------------------
; Function body here
----------------------------------------------------------------------------*/


/*----------------------------------------------------------------------------
; Return nothing or data or data pointer
----------------------------------------------------------------------------*/


