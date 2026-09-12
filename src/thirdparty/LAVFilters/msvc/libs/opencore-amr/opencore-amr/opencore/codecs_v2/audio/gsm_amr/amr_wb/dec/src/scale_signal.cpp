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



 Filename: scale_signal.cpp

------------------------------------------------------------------------------
 INPUT AND OUTPUT DEFINITIONS

     int16 signal[],             (i/o) : signal to scale
     int16 lg,                   (i)   : size of x[]
     int16 exp                   (i)   : exponent: x = round(x << exp)

------------------------------------------------------------------------------
 FUNCTION DESCRIPTION

   Scale signal to get maximum of dynamic range


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

void scale_signal(
    int16 x[],              /* (i/o) : signal to scale               */
    int16 lg,               /* (i)   : size of x[]                   */
    int16 exp               /* (i)   : exponent: x = round(x << exp) */
)
{
    int16 i;
    int16 tmp;
    int16 *pt_x;


    int32 L_tmp;


    if (exp > 0)
    {
        for (i = 0; i < lg; i++)
        {
            L_tmp = shl_int32(((int32)x[i] << 16), exp);       /* saturation can occur here */
            x[i] = amr_wb_round(L_tmp);
        }
    }
    else if (exp < 0)
    {
        exp = -exp;
        exp &= 0xf;
        tmp = (int16)(0x00008000 >> (16 - exp));
        pt_x = x;

        for (i = lg >> 1; i != 0; i--)
        {
            *(pt_x)   = add_int16(*(pt_x), tmp) >> exp;
            pt_x++;
            *(pt_x)   = add_int16(*(pt_x), tmp) >> exp;
            pt_x++;
        }

    }
    return;
}
