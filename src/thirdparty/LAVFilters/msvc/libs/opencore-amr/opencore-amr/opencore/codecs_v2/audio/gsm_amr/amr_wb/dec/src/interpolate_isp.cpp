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



 Filename: interpolate_isp.cpp

------------------------------------------------------------------------------
 INPUT AND OUTPUT DEFINITIONS

     int16 isp_old[],         input : isps from past frame
     int16 isp_new[],         input : isps from present frame
     const int16 frac[],      input : fraction for 3 first subfr (Q15)
     int16 Az[]               output: LP coefficients in 4 subframes


------------------------------------------------------------------------------
 FUNCTION DESCRIPTION

    Interpolation of the LP parameters in 4 subframes


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
#include "pvamrwbdecoder_cnst.h"
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

#define MP1 (M+1)

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

void interpolate_isp(
    int16 isp_old[],       /* input : isps from past frame              */
    int16 isp_new[],       /* input : isps from present frame           */
    const int16 frac[],    /* input : fraction for 3 first subfr (Q15)  */
    int16 Az[]             /* output: LP coefficients in 4 subframes    */
)
{
    int16 i, k, fac_old, fac_new;
    int16 isp[M];
    int32 L_tmp;

    for (k = 0; k < 3; k++)
    {
        fac_new = frac[k];
        fac_old = add_int16(sub_int16(32767, fac_new), 1);  /* 1.0 - fac_new */

        for (i = 0; i < M; i++)
        {
            L_tmp = mul_16by16_to_int32(isp_old[i], fac_old);
            L_tmp = mac_16by16_to_int32(L_tmp, isp_new[i], fac_new);
            isp[i] = amr_wb_round(L_tmp);
        }
        Isp_Az(isp, Az, M, 0);
        Az += MP1;
    }

    /* 4th subframe: isp_new (frac=1.0) */

    Isp_Az(isp_new, Az, M, 0);

    return;
}
