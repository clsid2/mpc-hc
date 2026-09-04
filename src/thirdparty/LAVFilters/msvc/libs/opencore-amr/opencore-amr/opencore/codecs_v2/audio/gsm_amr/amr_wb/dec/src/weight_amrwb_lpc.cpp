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



 Filename: weight_amrwb_lpc.cpp

------------------------------------------------------------------------------
 INPUT AND OUTPUT DEFINITIONS

     int16 a[],            (i) Q12 : a[m+1]  LPC coefficients
     int16 ap[],           (o) Q12 : Spectral expanded LPC coefficients
     int16 gamma,          (i) Q15 : Spectral expansion factor.
     int16 m               (i)     : LPC order.

------------------------------------------------------------------------------
 FUNCTION DESCRIPTION

   Weighting of LPC coefficients.
     ap[i]  =  a[i]   (gamma    i)

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

void weight_amrwb_lpc(
    int16 a[],          /* (i) Q12 : a[m+1]  LPC coefficients             */
    int16 ap[],         /* (o) Q12 : Spectral expanded LPC coefficients   */
    int16 gamma,        /* (i) Q15 : Spectral expansion factor.           */
    int16 m             /* (i)     : LPC order.                           */
)
{
    int16 i, fac;
    int32 roundFactor = 0x00004000L;
    ap[0] = a[0];
    fac = gamma;
    for (i = 1; i < m; i++)
    {
        ap[i] = (int16)(fxp_mac_16by16(a[i], fac, roundFactor) >> 15);
        fac   = (int16)(fxp_mac_16by16(fac, gamma, roundFactor) >> 15);
    }
    ap[i] = (int16)(fxp_mac_16by16(a[i], fac, roundFactor) >> 15);

    return;
}
