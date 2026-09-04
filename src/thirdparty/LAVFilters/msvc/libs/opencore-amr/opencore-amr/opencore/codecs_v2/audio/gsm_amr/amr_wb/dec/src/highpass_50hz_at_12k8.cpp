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



 Filename: highpass_50Hz_at_12k8.cpp

------------------------------------------------------------------------------
 INPUT AND OUTPUT DEFINITIONS

     int16 signal[],             input signal / output is divided by 16
     int16 lg,                   lenght of signal
     int16 mem[]                 filter memory [6]


------------------------------------------------------------------------------
 FUNCTION DESCRIPTION

   2nd order high pass filter with cut off frequency at 31 Hz.
   Designed with cheby2 function in MATLAB.
   Optimized for fixed-point to get the following frequency response:

    frequency:     0Hz    14Hz  24Hz   31Hz   37Hz   41Hz   47Hz
    dB loss:     -infdB  -15dB  -6dB   -3dB  -1.5dB  -1dB  -0.5dB

  Algorithm:

    y[i] = b[0]*x[i] + b[1]*x[i-1] + b[2]*x[i-2]
                     + a[1]*y[i-1] + a[2]*y[i-2];

    int16 b[3] = {4053, -8106, 4053};         in Q12
    int16 a[3] = {8192, 16211, -8021};        in Q12

    float -->   b[3] = {0.989501953, -1.979003906,  0.989501953};
                a[3] = {1.000000000,  1.978881836, -0.979125977};


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
#include "pvamrwb_math_op.h"
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

void highpass_50Hz_at_12k8_init(int16 mem[])
{
    pv_memset((void *)mem, 0, 6*sizeof(*mem));
}

/*----------------------------------------------------------------------------
; FUNCTION CODE
----------------------------------------------------------------------------*/

void highpass_50Hz_at_12k8(
    int16 signal[],                      /* input/output signal */
    int16 lg,                            /* lenght of signal    */
    int16 mem[]                          /* filter memory [6]   */
)
{
    int16 i, x2;
    int16 y2_hi, y2_lo, y1_hi, y1_lo, x0, x1;
    int32 L_tmp1;
    int32 L_tmp2;
    int16 *pt_sign = signal;

    y2_hi = mem[0];
    y2_lo = mem[1];
    y1_hi = mem[2];
    y1_lo = mem[3];
    x0    = mem[4];
    x1    = mem[5];


    for (i = lg; i != 0; i--)
    {

        /* y[i] = b[0]*x[i] + b[1]*x[i-1] + b[0]*x[i-2]  */
        /* + a[0]*y[i-1] + a[1] * y[i-2];  */

        L_tmp1 = fxp_mac_16by16(y1_lo, 16211, 8192L);
        L_tmp1 = fxp_mac_16by16(y2_lo, -8021, L_tmp1);
        L_tmp2 = fxp_mul_16by16(y1_hi, 32422);
        L_tmp2 = fxp_mac_16by16(y2_hi, -16042, L_tmp2);

        x2 = x1;
        x1 = x0;
        x0 = *pt_sign;
        L_tmp2 = fxp_mac_16by16(x2,  8106, L_tmp2);
        L_tmp2 = fxp_mac_16by16(x1, -16212, L_tmp2);
        L_tmp2 = fxp_mac_16by16(x0,  8106, L_tmp2);


        L_tmp1 = ((L_tmp1 >> 14) + L_tmp2) << 2;

        y2_hi = y1_hi;
        y2_lo = y1_lo;
        y1_hi = (int16)(L_tmp1 >> 16);
        y1_lo = (int16)((L_tmp1 - (y1_hi << 16)) >> 1);

        /* coeff Q14 --> Q15 with saturation */
        *(pt_sign++) = amr_wb_shl1_round(L_tmp1);

    }


    mem[0] = y2_hi;
    mem[1] = y2_lo;
    mem[2] = y1_hi;
    mem[3] = y1_lo;
    mem[4] = x0;
    mem[5] = x1;

}

