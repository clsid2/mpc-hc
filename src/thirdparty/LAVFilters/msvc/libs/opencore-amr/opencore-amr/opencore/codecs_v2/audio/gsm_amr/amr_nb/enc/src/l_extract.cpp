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




 Filename: l_extract.cpp

------------------------------------------------------------------------------
*/

/*----------------------------------------------------------------------------
; INCLUDES
----------------------------------------------------------------------------*/
#include    "basic_op.h"

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
 FUNCTION NAME: L_extract
------------------------------------------------------------------------------
 INPUT AND OUTPUT DEFINITIONS

 Inputs:
    L_var = 32 bit signed integer (Word32) whose value falls
           in the range : 0x8000 0000 <= L_32 <= 0x7fff ffff.

    pL_var_hi =  pointer to the most significant word of L_var (Word16).

    pL_var_lo =  pointer to the least significant word of L_var shifted
              to the left by 1 (Word16).

    pOverflow = pointer to overflow (Flag)

 Outputs:
    pOverflow -> 1 if the 32 bit add operation resulted in overflow
    pL_var_hi -> MS word of L_32.
    pL_var_lo -> LS word of L_32 shifted left by 1.

 Returns:
    None

 Global Variables Used:
    None

 Local Variables Needed:
    None

------------------------------------------------------------------------------
 FUNCTION DESCRIPTION

 This function extracts two 16-bit double precision format (DPF) numbers
 from a 32-bit integer. The MS word of L_var will be stored in the location
 pointed to by pL_var_hi and the shifted LS word of L_var will be stored in
 the location pointed to by pL_var_lo.

------------------------------------------------------------------------------
 REQUIREMENTS

 None

------------------------------------------------------------------------------
 REFERENCES

 [1] L_extract() function in oper_32b.c,  UMTS GSM AMR speech codec, R99 -
 Version 3.2.0, March 2, 2001

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
void L_Extract(Word32 L_var,
               Word16 *pL_var_hi,
               Word16 *pL_var_lo,
               Flag   *pOverflow)
{

    Word32  temp;

    OSCL_UNUSED_ARG(pOverflow);

    temp = (L_var >> 16);

    *(pL_var_hi) = (Word16) temp;
    *(pL_var_lo) = (Word16)((L_var >> 1) - (temp << 15));

    return;
}
