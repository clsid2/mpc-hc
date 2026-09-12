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

 Filename: q_plsf_3_tbl.h

------------------------------------------------------------------------------
 MODULE DESCRIPTION

------------------------------------------------------------------------------
*/

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
    ; [Define module specific macros here]
    ----------------------------------------------------------------------------*/

    /*----------------------------------------------------------------------------
    ; DEFINES
    ; [Include all pre-processor statements here. Include conditional
    ; compile variables also.]
    ----------------------------------------------------------------------------*/
#define MR795_1_SIZE  512
#define PAST_RQ_INIT_SIZE 8

#define DICO1_SIZE  256
#define DICO2_SIZE  512
#define DICO3_SIZE  512

#define MR515_3_SIZE  128

    /*----------------------------------------------------------------------------
    ; LOCAL FUNCTION DEFINITIONS
    ; [List function prototypes here]
    ----------------------------------------------------------------------------*/

    /*----------------------------------------------------------------------------
    ; LOCAL VARIABLE DEFINITIONS
    ; [Variable declaration - defined here and used outside this module]
    ----------------------------------------------------------------------------*/

    /*----------------------------------------------------------------------------
    ; EXTERNAL VARIABLES REFERENCES
    ; Declare variables used in this module but defined elsewhere
    ----------------------------------------------------------------------------*/

    /* Codebooks of LSF prediction residual */
    extern const Word16 mean_lsf_3[];

    extern const Word16 pred_fac_3[];

    extern const Word16 dico1_lsf_3[];
    extern const Word16 dico2_lsf_3[];
    extern const Word16 dico3_lsf_3[];

    extern const Word16 mr515_3_lsf[];
    extern const Word16 mr795_1_lsf[];

    extern const Word16 past_rq_init[];
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

