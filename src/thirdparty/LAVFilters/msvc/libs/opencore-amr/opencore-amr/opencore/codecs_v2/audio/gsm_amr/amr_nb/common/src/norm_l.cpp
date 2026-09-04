/* ------------------------------------------------------------------
 * Copyright (C) 1998-2010 PacketVideo
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
 Filename: norm_l.cpp

------------------------------------------------------------------------------
 INPUT AND OUTPUT DEFINITIONS

 Inputs:
    L_var1 = 32 bit long signed integer (Word32) whose value falls
             in the range : 0x8000 0000 <= var1 <= 0x7fff ffff.

 Local Stores/Buffers/Pointers Needed:
    None

 Global Stores/Buffers/Pointers Needed:
    None

 Outputs:
    var_out = number of left shifts need to normalize input (Word16)

 Pointers and Buffers Modified:
    None

 Local Stores Modified:
    None

 Global Stores Modified:
    None

------------------------------------------------------------------------------
 FUNCTION DESCRIPTION

 This function produces the number of left shifts needed to normalize the 32
 bit variable L_var1 for positive values on the interval with minimum of
 0x40000000 and maximum of 0x7fffffff, and for negative values on the interval
 with minimum of 0x80000000 and maximum of 0xc0000000. Note that when L_var1
 is equal to zero, the output var_out is set to zero.

------------------------------------------------------------------------------
 REQUIREMENTS

 None

------------------------------------------------------------------------------
 REFERENCES

 [1] basicop2.c, ETS Version 2.0.0, February 8, 1999

------------------------------------------------------------------------------
 PSEUDO-CODE

Word16 norm_l (Word32 L_var1)
{
    Word16 var_out;

    if (L_var1 == 0)
    {
        var_out = 0;
    }
    else
    {
        if (L_var1 == (Word32) 0xffffffffL)
        {
            var_out = 31;
        }
        else
        {
            if (L_var1 < 0)
            {
                L_var1 = ~L_var1;
            }
            for (var_out = 0; L_var1 < (Word32) 0x40000000L; var_out++)
            {
                L_var1 <<= 1;
            }
        }
    }

#if (WMOPS)
    multiCounter[currCounter].norm_l++;
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
#if !((PV_CPU_ARCH_VERSION >=5) && ((PV_COMPILER == EPV_ARM_GNUC) || (PV_COMPILER == EPV_ARM_RVCT)))
OSCL_EXPORT_REF Word16 norm_l(register Word32 L_var1)
{
    /*----------------------------------------------------------------------------
    ; Define all local variables
    ----------------------------------------------------------------------------*/
    register Word16 var_out = 0;

    /*----------------------------------------------------------------------------
    ; Function body here
    ----------------------------------------------------------------------------*/

    if (L_var1)
    {

        Word32 y = L_var1 - (L_var1 < 0);
        L_var1 = y ^(y >> 31);


        while (!(0x40000000L & L_var1))
        {
            var_out++;
            if ((0x20000000L & L_var1))
            {
                break;
            }
            var_out++;
            if ((0x10000000L & L_var1))
            {
                break;
            }
            var_out++;
            if ((0x08000000L & L_var1))
            {
                break;
            }
            var_out++;
            L_var1 <<= 4;
        }
    }

    /*----------------------------------------------------------------------------
    ; Return nothing or data or data pointer
    ----------------------------------------------------------------------------*/


    return (var_out);
}
#endif
