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
 Filename: log2.cpp

------------------------------------------------------------------------------
*/

/*----------------------------------------------------------------------------
; INCLUDES
----------------------------------------------------------------------------*/
#include "log2.h"
#include "basic_op.h"
#include "log2_norm.h"

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


/*
------------------------------------------------------------------------------
 FUNCTION NAME: log2()
------------------------------------------------------------------------------
 INPUT AND OUTPUT DEFINITIONS

 Inputs:
    L_x = input value of type Word32
    pExponent = pointer to the integer part of Log2 of type Word16 whose
           valid range is: 0 <= value <= 30
    pFraction = pointer to the fractional part of Log2 of type Word16
           whose valid range is: 0 <= value < 1
    pOverflow = pointer to overflow flag


 Outputs:
    pExponent -> integer part of the newly calculated Log2
    pFraction -> fractional part of the newly calculated Log2
    pOverflow -> 1 if the log2() operation resulted in saturation

 Returns:
    None

 Global Variables Used:
    None

 Local Variables Needed:
    None

------------------------------------------------------------------------------
 FUNCTION DESCRIPTION

 This function computes logarithm (base2) of the input L_x, where L_x is
 positive. If L_x is negative or zero, the result is 0.

 This function first normalizes the input L_x and calls the function Log2_norm
 to calculate the logarithm.

------------------------------------------------------------------------------
 REQUIREMENTS

 None

------------------------------------------------------------------------------
 REFERENCES

 [1] log2.c,  UMTS GSM AMR speech codec, R99 - Version 3.2.0, March 2, 2001

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
OSCL_EXPORT_REF void Log2(
    Word32 L_x,         /* (i) : input value                                */
    Word16 *pExponent,  /* (o) : Integer part of Log2.   (range: 0<=val<=30)*/
    Word16 *pFraction,  /* (o) : Fractional part of Log2. (range: 0<=val<1) */
    Flag   *pOverflow   /* (i/o) : overflow flag                            */
)
{
    Word16 exp;
    Word32 result;
    OSCL_UNUSED_ARG(pOverflow);

    exp = norm_l(L_x);
    result = L_x << exp;
    Log2_norm(result, exp, pExponent, pFraction);

    return;
}
