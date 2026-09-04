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
 INPUT AND OUTPUT DEFINITIONS

 Inputs:
    hi = 16 bit signed integer (Word16) whose value falls in
         the range : 0x8000 <= hi <= 0x7fff.
    lo = 16 bit signed integer (Word16) whose value falls in
         the range : 0x8000 <= lo <= 0x7fff.

 Outputs:
    pOverflow = 1 if overflow happens in a math function called by this function.
    L_out = 32-bit result of (hi<<16 + lo<<1).

 Returns:
    None

 Local Stores Modified:
    None

 Global Stores Modified:
        None

------------------------------------------------------------------------------
 FUNCTION DESCRIPTION

 This function composes a 32 bit integer from two 16 bit double precision
 format (DPF) numbers hi and lo by the following operation:
 1. Deposit hi into the 16 MS bits of the 32 bit output L_out.
 2. Shift lo left by 1.
 3. Add results from 1 and 2 with saturation to return the 32 bit result
    L_out.

------------------------------------------------------------------------------
 REQUIREMENTS

 None

------------------------------------------------------------------------------
 REFERENCES

 [1] oper_32b.c, ETS Version 2.0.0, February 8, 1999

------------------------------------------------------------------------------
 PSEUDO-CODE

------------------------------------------------------------------------------
*/


/*----------------------------------------------------------------------------
; INCLUDES
----------------------------------------------------------------------------*/
#include "typedef.h"
#include "l_comp.h"
#include "basic_op.h"
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

/*----------------------------------------------------------------------------
; EXTERNAL FUNCTION REFERENCES
; Declare functions defined elsewhere and referenced in this module
----------------------------------------------------------------------------*/

/*----------------------------------------------------------------------------
; EXTERNAL GLOBAL STORE/BUFFER/POINTER REFERENCES
; Declare variables used in this module but defined elsewhere
----------------------------------------------------------------------------*/

/*----------------------------------------------------------------------------
; FUNCTION CODE
----------------------------------------------------------------------------*/

Word32 L_Comp(Word16 hi, Word16 lo, Flag *pOverflow)
{

    /*----------------------------------------------------------------------------
    ; Define all local variables
    ----------------------------------------------------------------------------*/
    Word32 L_32;
    Word32 temp32;
    /*----------------------------------------------------------------------------
    ; Function body here
    ----------------------------------------------------------------------------*/

    L_32 = ((Word32)hi << 16);

    temp32 = L_mac(L_32, lo, 1, pOverflow);
    /*----------------------------------------------------------------------------
    ; Return nothing or data or data pointer
    ----------------------------------------------------------------------------*/

    return (temp32);       /* = hi<<16 + lo<<1 */
}
