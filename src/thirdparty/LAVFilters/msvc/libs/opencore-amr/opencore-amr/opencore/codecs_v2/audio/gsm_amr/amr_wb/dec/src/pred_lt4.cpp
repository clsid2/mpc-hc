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



 Filename: pred_lt4.cpp

------------------------------------------------------------------------------
 INPUT AND OUTPUT DEFINITIONS

     int16 signal[],             input signal / output is divided by 16
     int16 lg,                   lenght of signal
     int16 mem[]                 in/out: memory (size=30)
     int16 x[]                   scratch mem ( size= 60)

------------------------------------------------------------------------------
 FUNCTION DESCRIPTION

   Compute the result of long term prediction with fractionnal
   interpolation of resolution 1/4.

   On return exc[0..L_subfr-1] contains the interpolated signal
     (adaptive codebook excitation)


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

#define UP_SAMP      4
#define L_INTERPOL2  16

/*----------------------------------------------------------------------------
; LOCAL FUNCTION DEFINITIONS
; Function Prototype declaration
----------------------------------------------------------------------------*/

/*----------------------------------------------------------------------------
; LOCAL STORE/BUFFER/POINTER DEFINITIONS
; Variable declaration - defined here and used outside this module
----------------------------------------------------------------------------*/

/* 1/4 resolution interpolation filter (-3 dB at 0.856*fs/2) in Q14 */


const int16 inter4_2[UP_SAMP][ 2*L_INTERPOL2] =
{
    {
        0,       -2,        4,       -2,      -10,       38,
        -88,      165,     -275,      424,     -619,      871,
        -1207,     1699,    -2598,     5531,    14031,    -2147,
        780,     -249,      -16,      153,     -213,      226,
        -209,      175,     -133,       91,      -55,       28,
        -10,        2
    },
    {
        1,       -7,       19,      -33,       47,      -52,
        43,       -9,      -60,      175,     -355,      626,
        -1044,     1749,    -3267,    10359,    10359,    -3267,
        1749,    -1044,      626,     -355,      175,      -60,
        -9,       43,      -52,       47,      -33,       19,
        -7,        1
    },
    {
        2,      -10,       28,      -55,       91,     -133,
        175,     -209,      226,     -213,      153,      -16,
        -249,      780,    -2147,    14031,     5531,    -2598,
        1699,    -1207,      871,     -619,      424,     -275,
        165,      -88,       38,      -10,       -2,        4,
        -2,        0
    },
    {
        1,       -7,       22,      -49,       92,     -153,
        231,     -325,      431,     -544,      656,     -762,
        853,     -923,      968,    15401,      968,     -923,
        853,     -762,      656,     -544,      431,     -325,
        231,     -153,       92,      -49,       22,       -7,
        1,        0
    }
};

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

void Pred_lt4(
    int16 exc[],                         /* in/out: excitation buffer */
    int16 T0,                            /* input : integer pitch lag */
    int16 frac,                          /* input : fraction of lag   */
    int16 L_subfr                        /* input : subframe size     */
)
{
    int16 i, j, *pt_exc;
    int32 L_sum1;
    int32 L_sum2;
    int32 L_sum3;
    int32 L_sum4;
    pt_exc = &exc[-T0];

    const int16 *pt_inter4_2;

    frac = -frac;

    if (frac < 0)
    {
        frac += UP_SAMP;
        pt_exc--;

    }
    pt_exc -= (L_INTERPOL2 - 1);

    pt_inter4_2 = inter4_2[UP_SAMP-1 - frac];

    for (j = 0; j < (L_subfr >> 2); j++)
    {

        L_sum1 = 0x00002000;  /* pre-roundig */
        L_sum2 = 0x00002000;
        L_sum3 = 0x00002000;
        L_sum4 = 0x00002000;

        for (i = 0; i < L_INTERPOL2 << 1; i += 4)
        {
            int16 tmp1 = pt_exc[i  ];
            int16 tmp2 = pt_exc[i+1];
            int16 tmp3 = pt_exc[i+2];


            L_sum1 = fxp_mac_16by16(tmp1, pt_inter4_2[i  ], L_sum1);
            L_sum2 = fxp_mac_16by16(tmp2, pt_inter4_2[i  ], L_sum2);
            L_sum1 = fxp_mac_16by16(tmp2, pt_inter4_2[i+1], L_sum1);
            L_sum2 = fxp_mac_16by16(tmp3, pt_inter4_2[i+1], L_sum2);
            L_sum3 = fxp_mac_16by16(tmp3, pt_inter4_2[i  ], L_sum3);
            L_sum1 = fxp_mac_16by16(tmp3, pt_inter4_2[i+2], L_sum1);

            tmp1 = pt_exc[i+3];
            tmp2 = pt_exc[i+4];

            L_sum4 = fxp_mac_16by16(tmp1, pt_inter4_2[i  ], L_sum4);
            L_sum3 = fxp_mac_16by16(tmp1, pt_inter4_2[i+1], L_sum3);
            L_sum2 = fxp_mac_16by16(tmp1, pt_inter4_2[i+2], L_sum2);
            L_sum1 = fxp_mac_16by16(tmp1, pt_inter4_2[i+3], L_sum1);
            L_sum4 = fxp_mac_16by16(tmp2, pt_inter4_2[i+1], L_sum4);
            L_sum2 = fxp_mac_16by16(tmp2, pt_inter4_2[i+3], L_sum2);
            L_sum3 = fxp_mac_16by16(tmp2, pt_inter4_2[i+2], L_sum3);

            tmp1 = pt_exc[i+5];
            tmp2 = pt_exc[i+6];

            L_sum4 = fxp_mac_16by16(tmp1, pt_inter4_2[i+2], L_sum4);
            L_sum3 = fxp_mac_16by16(tmp1, pt_inter4_2[i+3], L_sum3);
            L_sum4 = fxp_mac_16by16(tmp2, pt_inter4_2[i+3], L_sum4);

        }



        exc[(j<<2)] = (int16)(L_sum1 >> 14);
        exc[(j<<2)+1] = (int16)(L_sum2 >> 14);
        exc[(j<<2)+2] = (int16)(L_sum3 >> 14);
        exc[(j<<2)+3] = (int16)(L_sum4 >> 14);

        pt_exc += 4;

    }

    if (L_subfr&1)
    {
        L_sum1 = 0x00002000;

        for (i = 0; i < 2*L_INTERPOL2; i += 4)
        {
            int16 tmp1 = pt_exc[i  ];
            int16 tmp2 = pt_exc[i+1];
            L_sum1 = fxp_mac_16by16(tmp1, pt_inter4_2[i  ], L_sum1);
            L_sum1 = fxp_mac_16by16(tmp2, pt_inter4_2[i+1], L_sum1);
            tmp1 = pt_exc[i+2];
            tmp2 = pt_exc[i+3];
            L_sum1 = fxp_mac_16by16(tmp1, pt_inter4_2[i+2], L_sum1);
            L_sum1 = fxp_mac_16by16(tmp2, pt_inter4_2[i+3], L_sum1);

        }

        exc[(j<<2)] = (int16)((L_sum1) >> 14);

    }


    return;
}

