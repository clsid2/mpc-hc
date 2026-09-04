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

 Filename: shr.cpp

------------------------------------------------------------------------------
 MODULE DESCRIPTION
 Shift right function with overflow control
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
 FUNCTION NAME: shr
------------------------------------------------------------------------------
 INPUT AND OUTPUT DEFINITIONS

 Inputs:
    var1 = 16 bit short signed integer (Word16) whose value falls in
           the range : 0xffff 8000 <= var1 <= 0x0000 7fff.

    var2 = 16 bit short signed integer (Word16) whose value falls in
           the range : 0xffff 8000 <= var2 <= 0x0000 7fff.

    pOverflow = pointer to overflow (Flag)

 Outputs:
    pOverflow -> 1 if the shift operation resulted in overflow

 Returns:
    product = Shifted result limited to 16 bits (Word16)

 Global Variables Used:
    None

 Local Variables Needed:
    None

------------------------------------------------------------------------------
 FUNCTION DESCRIPTION

 This function arithmetically shifts the 16 bit input var1 right var2 positions
 with sign extension. If var2 is negative, arithmetically shift var1 left by
 -var2 with sign extension. Saturate the result in case of underflows or
 overflows.

------------------------------------------------------------------------------
 REQUIREMENTS
 None
------------------------------------------------------------------------------
 REFERENCES

 [1] shr() function in basic_op2.c,  UMTS GSM AMR speech codec, R99 -
 Version 3.2.0, March 2, 2001

------------------------------------------------------------------------------
 PSEUDO-CODE

Word16 shr_std (Word16 var1, Word16 var2)
{
    Word16 var_out;

    if (var2 < 0)
    {
        if (var2 < -16)
            var2 = -16;
        var_out = shl_std (var1, -var2);
#if (WMOPS)
        mult_stdiCounter[currCounter].shl_std--;
#endif
    }
    else
    {
        if (var2 >= 15)
        {
            var_out = (var1 < 0) ? -1 : 0;
        }
        else
        {
            if (var1 < 0)
            {
                var_out = ~((~var1) >> var2);
            }
            else
            {
                var_out = var1 >> var2;
            }
        }
    }

#if (WMOPS)
    mult_stdiCounter[currCounter].shr_std++;
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
OSCL_EXPORT_REF Word16 shr(register Word16 var1, register Word16 var2, Flag *pOverflow)
{
    register Word16 result;

    if (var2 != 0)
    {
        if (var2 > 0)
        {
            if (var2 > 15)
            {
                var2 = 15;
            }

            result = var1 >> var2;

        }
        else
        {
            var2 = -var2;   /* Shift right negative is equivalent */

            if (var2 > 15)
            {
                var2 = 15;
            }

            result = (var1 << var2);
            if ((result >> var2) != var1)
            {
                *pOverflow = 1;
                result = ((var1 > 0) ? MAX_16 : MIN_16);
            }
        }

    }
    else
    {
        result = var1;
    }

    return (result);
}

