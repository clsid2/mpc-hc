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
 Filename: div_s.cpp

------------------------------------------------------------------------------
 INPUT AND OUTPUT DEFINITIONS

 Inputs:
    var1 = 16 bit signed integer (Word16) whose value falls in
           the range : 0x0000 <= var1 <= 0x7fff.
    var2 = 16 bit signed integer (Word16) whose value falls in
           the range : 0x0000 <= var1 <= 0x7fff.

 Local Stores/Buffers/Pointers Needed:
    None

 Global Stores/Buffers/Pointers Needed:
    None

 Outputs:
    var_out = quotient of var1 divided by var2 (Word16)

 Pointers and Buffers Modified:
    None

 Local Stores Modified:
    None

 Global Stores Modified:
    None

------------------------------------------------------------------------------
 FUNCTION DESCRIPTION

 This function produces a result which is the fractional integer division of
 var1 by var2; var1 and var2 must be positive and var2 must be greater or equal
 to var1; the result is positive (leading bit equal to 0) and truncated to 16
 bits. If var1 = var2 then div(var1,var2) = 32767.

------------------------------------------------------------------------------
 REQUIREMENTS

 None

------------------------------------------------------------------------------
 REFERENCES

 [1] basicop2.c, ETS Version 2.0.0, February 8, 1999

------------------------------------------------------------------------------
 PSEUDO-CODE

Word16 div_s (Word16 var1, Word16 var2)
{
    Word16 var_out = 0;
    Word16 iteration;
    Word32 L_num;
    Word32 L_denom;
    Word16 abort_flag = 0;

    if ((var1 > var2) || (var1 < 0))
    {
        printf ("Division Error var1=%d  var2=%d\n", var1, var2);
        abort_flag = 1;
        exit(0);
    }
    if ((var1 != 0) && (abort_flag == 0))
    {
        if (var1 == var2)
        {
            var_out = MAX_16;
        }
        else
        {
            L_num = (Word32) var1;
            L_denom = (Word32) var2;

            for (iteration = 15; iteration > 0; iteration--)
            {
                var_out <<= 1;
                L_num <<= 1;

                if (L_num >= L_denom)
                {
                    L_num -= L_denom;
                    var_out += 1;
                }
            }
        }
    }

#if (WMOPS)
    multiCounter[currCounter].div_s++;
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
OSCL_EXPORT_REF Word16 div_s(register Word16 var1, register Word16 var2)
{
    /*----------------------------------------------------------------------------
    ; Define all local variables
    ----------------------------------------------------------------------------*/
    Word16 var_out = 0;
    register Word16 iteration;
    Word32 L_num;
    Word32 L_denom;
    Word32 L_denom_by_2;
    Word32 L_denom_by_4;

    /*----------------------------------------------------------------------------
    ; Function body here
    ----------------------------------------------------------------------------*/
    if ((var1 > var2) || (var1 < 0))
    {
        return 0; // used to exit(0);
    }
    if (var1)
    {
        if (var1 != var2)
        {

            L_num = (Word32) var1;
            L_denom = (Word32) var2;
            L_denom_by_2 = (L_denom << 1);
            L_denom_by_4 = (L_denom << 2);
            for (iteration = 5; iteration > 0; iteration--)
            {
                var_out <<= 3;
                L_num   <<= 3;

                if (L_num >= L_denom_by_4)
                {
                    L_num -= L_denom_by_4;
                    var_out |= 4;
                }

                if (L_num >= L_denom_by_2)
                {
                    L_num -= L_denom_by_2;
                    var_out |=  2;
                }

                if (L_num >= (L_denom))
                {
                    L_num -= (L_denom);
                    var_out |=  1;
                }

            }
        }
        else
        {
            var_out = MAX_16;
        }
    }

#if (WMOPS)
    multiCounter[currCounter].div_s++;
#endif

    /*----------------------------------------------------------------------------
    ; Return nothing or data or data pointer
    ----------------------------------------------------------------------------*/
    return (var_out);
}
