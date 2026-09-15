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

    3GPP TS 26.173
    ANSI-C code for the Adaptive Multi-Rate - Wideband (AMR-WB) speech codec
    Available from http://www.3gpp.org

(C) 2007, 3GPP Organizational Partners (ARIB, ATIS, CCSA, ETSI, TTA, TTC)
Permission to distribute, modify and use this file under the standard license
terms listed above has been obtained from the copyright holder.
****************************************************************************************/
/*
------------------------------------------------------------------------------



 Filename: normalize_amr_wb.cpp

------------------------------------------------------------------------------
 INPUT AND OUTPUT DEFINITIONS

Input
    Int32 x             32-bit integer non-zero input
Returns
    Int16 i             number of leading zeros on x


------------------------------------------------------------------------------
 FUNCTION DESCRIPTION

    Returns number of leading zeros on the non-zero input

------------------------------------------------------------------------------
 REQUIREMENTS


------------------------------------------------------------------------------
 REFERENCES

------------------------------------------------------------------------------
 PSEUDO-CODE

------------------------------------------------------------------------------
*/


/*----------------------------------------------------------------------------
; INCLUDES
----------------------------------------------------------------------------*/

#include "pv_amr_wb_type_defs.h"
#include "normalize_amr_wb.h"

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

#if ((PV_CPU_ARCH_VERSION >=5) && ((PV_COMPILER == EPV_ARM_RVCT) || (PV_COMPILER == EPV_ARM_GNUC)))
/* function is inlined in header file */


#else

int16 normalize_amr_wb(int32 x)
{
    /*----------------------------------------------------------------------------
    ; Define all local variables
    ----------------------------------------------------------------------------*/
    int16 i;


    if (x > 0x0FFFFFFF)
    {
        i = 0;  /* most likely case */
    }
    else if (x > 0x00FFFFFF)
    {
        i = 3;  /* second most likely case */
    }
    else if (x > 0x0000FFFF)
    {
        i  = x > 0x000FFFFF ?  7 :  11;
    }
    else
    {
        if (x > 0x000000FF)
        {
            i  = x > 0x00000FFF ?  15 :  19;
        }
        else
        {
            i  = x > 0x0000000F ?  23 :  27;
        }
    }


    x <<= i;

    switch (x & 0x78000000)
    {
        case 0x08000000:
            i += 3;
            break;

        case 0x18000000:
        case 0x10000000:
            i += 2;
            break;
        case 0x28000000:
        case 0x20000000:
        case 0x38000000:
        case 0x30000000:
            i++;

        default:
            ;
    }

    return i;

}

#endif

