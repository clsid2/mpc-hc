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
 Filename: l_deposit_l.cpp

------------------------------------------------------------------------------
 INPUT AND OUTPUT DEFINITIONS

 Inputs:
    var1 = 16 bit short signed integer (Word16) whose value falls in
           the range : 0xffff 8000 <= var1 <= 0x0000 7fff.

 Local Stores/Buffers/Pointers Needed:
    None

 Global Stores/Buffers/Pointers Needed:
    None

 Outputs:
    var1 = deposit of var1 into LSWord of 32 bit value (Word32)

 Pointers and Buffers Modified:
    None

 Local Stores Modified:
    None

 Global Stores Modified:
    None

------------------------------------------------------------------------------
 FUNCTION DESCRIPTION

 This function deposits the 16 bit var1 into the 16 LS bits of the 32 bit
 output. The 16 MS bits of the output are sign extended.

------------------------------------------------------------------------------
 REQUIREMENTS

 None

------------------------------------------------------------------------------
 REFERENCES

 [1] basicop2.c, ETS Version 2.0.0, February 8, 1999

------------------------------------------------------------------------------
 PSEUDO-CODE

Word32 L_deposit_l (Word16 var1)
{
    Word32 L_var_out;

    L_var_out = (Word32) var1;
#if (WMOPS)
    multiCounter[currCounter].L_deposit_l++;
#endif
    return (L_var_out);
}

------------------------------------------------------------------------------
*/


/*----------------------------------------------------------------------------
; INCLUDES
----------------------------------------------------------------------------*/
#include    "basic_op.h"

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
Word32 L_deposit_l(Word16 var1)
{
    /*----------------------------------------------------------------------------
    ; Define all local variables
    ----------------------------------------------------------------------------*/

    /*----------------------------------------------------------------------------
    ; Function body here
    ----------------------------------------------------------------------------*/

    /*----------------------------------------------------------------------------
    ; Return nothing or data or data pointer
    ----------------------------------------------------------------------------*/
    return ((Word32) var1);
}
