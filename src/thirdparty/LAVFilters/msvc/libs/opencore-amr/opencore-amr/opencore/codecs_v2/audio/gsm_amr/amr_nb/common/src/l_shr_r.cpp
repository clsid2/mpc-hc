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
 Filename: l_shr_r.cpp

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
 FUNCTION NAME: L_shr_r
------------------------------------------------------------------------------
 INPUT AND OUTPUT DEFINITIONS

 Inputs:
    L_var1 = 32 bit long signed integer (Word32 ) whose value falls
             in the range : 0x8000 0000 <= L_var1 <= 0x7fff ffff.
    var2 = 16 bit short signed integer (Word16) whose value falls in
           the range : 0xffff 8000 <= var2 <= 0x0000 7fff.

    pOverflow = pointer to overflow (Flag)

 Outputs:
    pOverflow -> 1 if the 32 bit shift operation resulted in overflow

 Returns:
    result = Shifted result w/ rounding (Word32)

 Global Variables Used:
    None

 Local Variables Needed:
    None

------------------------------------------------------------------------------
 FUNCTION DESCRIPTION

 This function arithmetically shifts the 32 bit input L_var1 right var2
 positions with rounding. If var2 is negative, the function
 arithmetically shifts L_var1 left by -var2 and zero fills the -var2 LSB of
 the result. The result is saturated in case of underflows or overflows, i.e.,

 - If var2 is greater than zero :
    if (L_sub(L_shl(L_shr(L_var1,var2),1),L_shr(L_var1,sub(var2,1))))
    is equal to zero
    then
        L_shr_r(L_var1,var2) = L_shr(L_var1,var2)
    else
        L_shr_r(L_var1,var2) = L_add(L_shr(L_var1,var2),1)
 - If var2 is less than or equal to zero :
    L_shr_r(L_var1,var2) = L_shr(L_var1,var2).

------------------------------------------------------------------------------
 REQUIREMENTS

 None

------------------------------------------------------------------------------
 REFERENCES

 [1] L_shr_r() function in basic_op2.c,  UMTS GSM AMR speech codec, R99 -
 Version 3.2.0, March 2, 2001

------------------------------------------------------------------------------
 PSEUDO-CODE

Word32 L_shr_r (Word32 L_var1, Word16 var2)
{
    Word32 L_var_out;

* The reference ETSI code uses a global flag for Overflow. In the actual
* implementation a pointer to Overflow flag is passed in as a parameter to the
* function L_shr()

    if (var2 > 31)
    {
        L_var_out = 0;
    }
    else
    {
        L_var_out = L_shr (L_var1, var2);
#if (WMOPS)
        multiCounter[currCounter].L_shr--;
#endif
        if (var2 > 0)
        {
            if ((L_var1 & ((Word32) 1 << (var2 - 1))) != 0)
            {
                L_var_out++;
            }
        }
    }
#if (WMOPS)
    multiCounter[currCounter].L_shr_r++;
#endif
    return (L_var_out);
}

------------------------------------------------------------------------------
 CAUTION [optional]
 [State any special notes, constraints or cautions for users of this function]

------------------------------------------------------------------------------
*/

/*----------------------------------------------------------------------------
; FUNCTION CODE
----------------------------------------------------------------------------*/
OSCL_EXPORT_REF Word32 L_shr_r(register Word32 L_var1, register Word16 var2, Flag *pOverflow)
{
    Word32 result;

    if (var2 > 31)
    {
        result = 0;
    }
    else
    {
        result = L_shr(L_var1, var2, pOverflow);

        if (var2 > 0)
        {
            if ((L_var1 & ((Word32) 1 << (var2 - 1))) != 0)
            {
                result++;
            }
        }
    }
    return (result);
}
