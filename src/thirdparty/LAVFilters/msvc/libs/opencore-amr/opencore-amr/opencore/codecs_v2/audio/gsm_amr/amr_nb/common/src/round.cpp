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

 Filename: round.cpp

------------------------------------------------------------------------------
 MODULE DESCRIPTION

 Rounding function with saturation.

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
 FUNCTION NAME: pv_round
------------------------------------------------------------------------------
 INPUT AND OUTPUT DEFINITIONS

 Inputs:
    L_var1 = 32 bit signed integer (Word32) whose value falls
             in the range : 0x8000 0000 <= L_var1 <= 0x7fff ffff.

    pOverflow = pointer to overflow (Flag)

 Outputs:
    None

 Returns:
        result = MS 16 bits of rounded input L_var1.

 Global Variables Used:
    None

 Local Variables Needed:
    None

------------------------------------------------------------------------------
 FUNCTION DESCRIPTION

 This function rounds the lower 16 bits of the 32 bit input number into the
 MS 16 bits with saturation. Shift the resulting bits right by 16 and return
 the 16 bit number:
    pv_round(L_var1) = extract_h(L_add(L_var1,32768))

------------------------------------------------------------------------------
 REQUIREMENTS

 None

------------------------------------------------------------------------------
 REFERENCES

 [1] round() function in basic_op2.c,  UMTS GSM AMR speech codec, R99 -
 Version 3.2.0, March 2, 2001

------------------------------------------------------------------------------
 PSEUDO-CODE

Word16 pv_round (Word32 L_var1)
{
    Word16 var_out;
    Word32 L_rounded;

* The reference ETSI code uses a global flag for Overflow in the L_add() function.
* In the actual implementation a pointer to Overflow flag is passed in as a
* parameter to the function.

    L_rounded = L_add (L_var1, (Word32) 0x00008000L);
#if (WMOPS)
    multiCounter[currCounter].L_add--;
#endif
    var_out = extract_h (L_rounded);
#if (WMOPS)
    multiCounter[currCounter].extract_h--;
    multiCounter[currCounter].round++;
#endif
    return (var_out);
}

------------------------------------------------------------------------------
 CAUTION [optional]
 [State any special notes, constraints or cautions for users of this function]

------------------------------------------------------------------------------
*/

/*----------------------------------------------------------------------------
; FUNCTION CODE
----------------------------------------------------------------------------*/
OSCL_EXPORT_REF Word16 pv_round(register Word32 L_var1, Flag *pOverflow)
{
    Word16  result;

    L_var1 = L_add(L_var1, (Word32) 0x00008000L, pOverflow);
    result = (Word16)(L_var1 >> 16);

    return (result);
}
