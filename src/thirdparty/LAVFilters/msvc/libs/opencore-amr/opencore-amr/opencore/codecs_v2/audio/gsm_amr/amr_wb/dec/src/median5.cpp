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

    3GPP TS 26.173
    ANSI-C code for the Adaptive Multi-Rate - Wideband (AMR-WB) speech codec
    Available from http://www.3gpp.org

(C) 2007, 3GPP Organizational Partners (ARIB, ATIS, CCSA, ETSI, TTA, TTC)
Permission to distribute, modify and use this file under the standard license
terms listed above has been obtained from the copyright holder.
****************************************************************************************/
/*
------------------------------------------------------------------------------



 Filename: median5.cpp

------------------------------------------------------------------------------
 INPUT AND OUTPUT DEFINITIONS

   INPUT
       X[-2:2]   16-bit integers.

   RETURN VALUE
       The median of {X[-2], X[-1],..., X[2]}.

------------------------------------------------------------------------------
 FUNCTION DESCRIPTION

      Returns the median of the set {X[-2], X[-1],..., X[2]},
      whose elements are 16-bit integers.

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
#include "pvamrwbdecoder_basic_op.h"
#include "pvamrwbdecoder_acelp.h"
#include "pvamrwb_math_op.h"

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

int16 median5(int16 x[])
{
    int16 x1, x2, x3, x4, x5;
    int16 tmp;

    x1 = x[-2];
    x2 = x[-1];
    x3 = x[0];
    x4 = x[1];
    x5 = x[2];



    if (x2 < x1)
    {
        tmp = x1;
        x1 = x2;
        x2 = tmp;
    }
    if (x3 < x1)
    {
        tmp = x1;
        x1 = x3;
        x3 = tmp;
    }
    if (x4 < x1)
    {
        tmp = x1;
        x1 = x4;
        x4 = tmp;
    }
    if (x5 < x1)
    {
        x5 = x1;
    }
    if (x3 < x2)
    {
        tmp = x2;
        x2 = x3;
        x3 = tmp;
    }
    if (x4 < x2)
    {
        tmp = x2;
        x2 = x4;
        x4 = tmp;
    }
    if (x5 < x2)
    {
        x5 = x2;
    }
    if (x4 < x3)
    {
        x3 = x4;
    }
    if (x5 < x3)
    {
        x3 = x5;
    }
    return (x3);
}


