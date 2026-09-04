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

 Filename: shr_r.cpp

------------------------------------------------------------------------------
 INPUT AND OUTPUT DEFINITIONS

 Inputs:
    var1 = 16 bit short signed integer (Word16) whose value falls in
           the range : 0xffff 8000 <= var1 <= 0x0000 7fff.
    var2 = 16 bit short signed integer (Word16) whose value falls in
           the range : 0xffff 8000 <= var2 <= 0x0000 7fff.

 Local Stores/Buffers/Pointers Needed:
    None

 Global Stores/Buffers/Pointers Needed:
    None

 Outputs:
    var_out = shifted input w/ rounding (Word16)

 Pointers and Buffers Modified:
    None

 Local Stores Modified:
    None

 Global Stores Modified:
    None

------------------------------------------------------------------------------
 FUNCTION DESCRIPTION

 This function arithmetically shifts the 16 bit input var1 right var2 positions
 with rounding. If var2 is negative, arithmetically shift var1 left by
 -var2 with rounding. Saturate the result in case of underflows or
 overflows.

    - If var2 is greater than zero :
        if (sub(shl(shr(var1,var2),1),shr(var1,sub(var2,1))))
        is equal to zero
        then
            shr_r(var1,var2) = shr(var1,var2)
        else
            shr_r(var1,var2) = add(shr(var1,var2),1)
    - If var2 is less than or equal to zero :
        shr_r(var1,var2) = shr(var1,var2).

------------------------------------------------------------------------------
 REQUIREMENTS

 None

------------------------------------------------------------------------------
 REFERENCES

 [1] basicop2.c, ETS Version 2.0.0, February 8, 1999

------------------------------------------------------------------------------
 PSEUDO-CODE

Word16 shr_r (Word16 var1, Word16 var2)
{
    Word16 var_out;

    if (var2 > 15)
    {
        var_out = 0;
    }
    else
    {
        var_out = shr (var1, var2);
#if (WMOPS)
        multiCounter[currCounter].shr--;
#endif

        if (var2 > 0)
        {
            if ((var1 & ((Word16) 1 << (var2 - 1))) != 0)
            {
                var_out++;
            }
        }
    }
#if (WMOPS)
    multiCounter[currCounter].shr_r++;
#endif
    return (var_out);
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
OSCL_EXPORT_REF Word16 shr_r(register Word16 var1, register Word16 var2, Flag *pOverflow)
{
    /*----------------------------------------------------------------------------
    ; Define all local variables
    ----------------------------------------------------------------------------*/
    Word16 var_out;

    /*----------------------------------------------------------------------------
    ; Function body here
    ----------------------------------------------------------------------------*/
    if (var2 > 15)
    {
        var_out = 0;
    }
    else
    {
        var_out = shr(var1, var2, pOverflow);
        if (var2 > 0)
        {
            if ((var1 & ((Word16) 1 << (var2 - 1))) != 0)
            {
                var_out++;
            }
        }
    }

    /*----------------------------------------------------------------------------
    ; Return nothing or data or data pointer
    ----------------------------------------------------------------------------*/
    return (var_out);
}
