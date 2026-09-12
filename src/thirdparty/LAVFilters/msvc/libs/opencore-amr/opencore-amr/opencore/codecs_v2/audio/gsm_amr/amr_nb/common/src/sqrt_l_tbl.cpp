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

 Filename: sqrt_l_tbl.cpp

------------------------------------------------------------------------------
 MODULE DESCRIPTION

 This file contains the declaration for sqrt_l_table[] used by the sqrt_l_exp
 function.

    sqrt_l_tbl[i] = sqrt((i+16)*2^-6) * 2^15, i.e. sqrt(x) scaled Q15

 ------------------------------------------------------------------------------
*/

/*----------------------------------------------------------------------------
; INCLUDES
----------------------------------------------------------------------------*/
#include "sqrt_l.h"

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

    /*----------------------------------------------------------------------------
    ; LOCAL FUNCTION DEFINITIONS
    ; [List function prototypes here]
    ----------------------------------------------------------------------------*/

    /*----------------------------------------------------------------------------
    ; LOCAL VARIABLE DEFINITIONS
    ; [Variable declaration - defined here and used outside this module]
    ----------------------------------------------------------------------------*/
    const Word16 sqrt_l_tbl[50] =
    {
        16384, 16888, 17378, 17854, 18318, 18770, 19212, 19644, 20066, 20480,
        20886, 21283, 21674, 22058, 22435, 22806, 23170, 23530, 23884, 24232,
        24576, 24915, 25249, 25580, 25905, 26227, 26545, 26859, 27170, 27477,
        27780, 28081, 28378, 28672, 28963, 29251, 29537, 29819, 30099, 30377,
        30652, 30924, 31194, 31462, 31727, 31991, 32252, 32511, 32767, 32767
    };


    /*--------------------------------------------------------------------------*/
#ifdef __cplusplus
}
#endif

/*
------------------------------------------------------------------------------
 FUNCTION NAME:
------------------------------------------------------------------------------
 INPUT AND OUTPUT DEFINITIONS

 Inputs:
    None

 Outputs:
    None

 Returns:
    None

 Global Variables Used:
    None

 Local Variables Needed:
    None

------------------------------------------------------------------------------
 FUNCTION DESCRIPTION

 None

------------------------------------------------------------------------------
 REQUIREMENTS

 None

------------------------------------------------------------------------------
 REFERENCES

 [1] inv_sqrt.tab file,  UMTS GSM AMR speech codec, R99 - Version 3.2.0,
 March 2, 2001

------------------------------------------------------------------------------
 PSEUDO-CODE


------------------------------------------------------------------------------
 CAUTION [optional]
 [State any special notes, constraints or cautions for users of this function]

------------------------------------------------------------------------------
*/

/*----------------------------------------------------------------------------
; FUNCTION CODE
----------------------------------------------------------------------------*/

