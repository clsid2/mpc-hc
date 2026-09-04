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



 Filename: deemphasis_32.cpp

------------------------------------------------------------------------------
 INPUT AND OUTPUT DEFINITIONS

     int16 x_hi[],               (i)     : input signal (bit31..16)
     int16 x_lo[],               (i)     : input signal (bit15..4)
     int16 y[],                  (o)     : output signal (x16)
     int16 mu,                   (i) Q15 : deemphasis factor
     int16 L,                    (i)     : vector size
     int16 * mem                 (i/o)   : memory (y[-1])

------------------------------------------------------------------------------
 FUNCTION DESCRIPTION

    32-bits filtering through 1/(1-mu z^-1)

------------------------------------------------------------------------------
 REQUIREMENTS


------------------------------------------------------------------------------
 REFERENCES

------------------------------------------------------------------------------
 PSEUDO-CODE

    Deemphasis H(z) = 1/(1 - 0.68z^(-1))   where mu = 0.67999 in Q15

------------------------------------------------------------------------------
*/


/*----------------------------------------------------------------------------
; INCLUDES
----------------------------------------------------------------------------*/

#include "pv_amr_wb_type_defs.h"
#include "pvamrwbdecoder_basic_op.h"
#include "pvamrwb_math_op.h"
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

void deemphasis_32(
    int16 x_hi[],                        /* (i)     : input signal (bit31..16) */
    int16 x_lo[],                        /* (i)     : input signal (bit15..4)  */
    int16 y[],                           /* (o)     : output signal (x16)      */
    int16 mu,                            /* (i) Q15 : deemphasis factor        */
    int16 L,                             /* (i)     : vector size              */
    int16 * mem                          /* (i/o)   : memory (y[-1])           */
)
{
    int16 i;
    int32 L_tmp;
    int16 lo, hi;

    L_tmp  = ((int32)x_hi[0]) << 16;
    L_tmp += ((int32)x_lo[0]) << 4;
    L_tmp  = shl_int32(L_tmp, 3);

    L_tmp = fxp_mac_16by16(*mem, mu, L_tmp),

            L_tmp = shl_int32(L_tmp, 1);               /* saturation can occur here */
    y[0] = amr_wb_round(L_tmp);

    lo = x_lo[1];
    hi = x_hi[1];
    for (i = 1; i < L - 1; i++)
    {
        L_tmp  = ((int32)hi) << 16;
        L_tmp += ((int32)lo) << 4;
        L_tmp  = shl_int32(L_tmp, 3);
        L_tmp  = fxp_mac_16by16(y[i - 1], mu, L_tmp),
                 L_tmp  = shl_int32(L_tmp, 1);           /* saturation can occur here */
        y[i]   = amr_wb_round(L_tmp);
        lo     = x_lo[i+1];
        hi     = x_hi[i+1];
    }
    L_tmp  = ((int32)hi) << 16;
    L_tmp += ((int32)lo) << 4;
    L_tmp  = shl_int32(L_tmp, 3);
    L_tmp  = fxp_mac_16by16(y[i - 1], mu, L_tmp),
             L_tmp  = shl_int32(L_tmp, 1);           /* saturation can occur here */
    y[i]   = amr_wb_round(L_tmp);

    *mem = y[L - 1];

    return;
}

