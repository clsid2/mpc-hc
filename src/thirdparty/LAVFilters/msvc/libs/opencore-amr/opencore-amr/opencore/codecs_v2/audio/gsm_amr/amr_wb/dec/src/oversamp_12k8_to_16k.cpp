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



 Filename: oversamp_12k8_to_16k.cpp

------------------------------------------------------------------------------
 INPUT AND OUTPUT DEFINITIONS

     int16 signal[],             input signal / output is divided by 16
     int16 lg,                   lenght of signal
     int16 mem[]                 in/out: memory (size=30)
     int16 x[]                   scratch mem ( size= 60)

------------------------------------------------------------------------------
 FUNCTION DESCRIPTION

 Oversamp_16k : oversampling from 12.8kHz to 16kHz.


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
#include "pvamrwbdecoder_cnst.h"

/*----------------------------------------------------------------------------
; MACROS
; Define module specific macros here
----------------------------------------------------------------------------*/


/*----------------------------------------------------------------------------
; DEFINES
; Include all pre-processor statements here. Include conditional
; compile variables also.
----------------------------------------------------------------------------*/

#define FAC4   4
#define FAC5   5
#define INV_FAC5   6554                    /* 1/5 in Q15 */
#define DOWN_FAC  26215                    /* 4/5 in Q15 */
#define UP_FAC    20480                    /* 5/4 in Q14 */
#define NB_COEF_DOWN  15
#define NB_COEF_UP    12
#define N_LOOP_COEF_UP    4

/*----------------------------------------------------------------------------
; LOCAL FUNCTION DEFINITIONS
; Function Prototype declaration
----------------------------------------------------------------------------*/

#ifdef __cplusplus
extern "C"
{
#endif


    /* Local functions */

    void AmrWbUp_samp(
        int16 * sig_d,                       /* input:  signal to oversampling  */
        int16 * sig_u,                       /* output: oversampled signal      */
        int16 L_frame                        /* input:  length of output        */
    );


    int16 AmrWbInterpol(                      /* return result of interpolation */
        int16 * x,                           /* input vector                   */
        const int16 * fir,                   /* filter coefficient             */
        int16 nb_coef                        /* number of coefficients         */
    );


#ifdef __cplusplus
}
#endif

/*----------------------------------------------------------------------------
; LOCAL STORE/BUFFER/POINTER DEFINITIONS
; Variable declaration - defined here and used outside this module
----------------------------------------------------------------------------*/


/* 1/5 resolution interpolation filter  (in Q14)  */
/* -1.5dB @ 6kHz,    -6dB @ 6.4kHz, -10dB @ 6.6kHz,
    -20dB @ 6.9kHz, -25dB @ 7kHz,   -55dB @ 8kHz  */


const int16 fir_up[4][24] =
{

    {
        -1,        12,       -33,       68,       -119,       191,
        -291,       430,      -634,       963,     -1616,      3792,
        15317,     -2496,      1288,      -809,       542,      -369,
        247,      -160,        96,       -52,        23,        -6,
    },
    {
        -4,        24,       -62,       124,      -213,       338,
        -510,       752,     -1111,      1708,     -2974,      8219,
        12368,     -3432,      1881,     -1204,       812,      -552,
        368,      -235,       139,       -73,        30,        -7,
    },
    {
        -7,        30,       -73,       139,      -235,       368,
        -552,       812,     -1204,      1881,     -3432,     12368,
        8219,     -2974,      1708,     -1111,       752,      -510,
        338,      -213,       124,       -62,        24,        -4,
    },
    {
        -6,        23,       -52,        96,      -160,       247,
        -369,       542,      -809,      1288,     -2496,     15317,
        3792,     -1616,       963,      -634,       430,      -291,
        191,      -119,        68,       -33,        12,        -1,
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


/* output: memory (2*NB_COEF_UP) set to zeros  */
void oversamp_12k8_to_16k_init(int16 mem[])
{
    pv_memset((void *)mem, 0, (2*NB_COEF_UP)*sizeof(*mem));

}

/*----------------------------------------------------------------------------
; FUNCTION CODE
----------------------------------------------------------------------------*/

void oversamp_12k8_to_16k(
    int16 sig12k8[],                     /* input:  signal to oversampling  */
    int16 lg,                            /* input:  length of input         */
    int16 sig16k[],                      /* output: oversampled signal      */
    int16 mem[],                         /* in/out: memory (2*NB_COEF_UP)   */
    int16 signal[]
)
{
    int16 lg_up;

    pv_memcpy((void *)signal,
              (void *)mem,
              (2*NB_COEF_UP)*sizeof(*mem));

    pv_memcpy((void *)(signal + (2*NB_COEF_UP)),
              (void *)sig12k8,
              lg*sizeof(*sig12k8));

    lg_up = lg + (lg >> 2); /* 5/4 of lg */

    AmrWbUp_samp(signal + NB_COEF_UP, sig16k, lg_up);

    pv_memcpy((void *)mem,
              (void *)(signal + lg),
              (2*NB_COEF_UP)*sizeof(*signal));

    return;
}



/*----------------------------------------------------------------------------
; FUNCTION CODE
----------------------------------------------------------------------------*/


void AmrWbUp_samp(
    int16 * sig_d,                       /* input:  signal to oversampling  */
    int16 * sig_u,                       /* output: oversampled signal      */
    int16 L_frame                        /* input:  length of output        */
)
{

    int32 i;
    int16 frac;
    int16 * pt_sig_u = sig_u;

    frac = 1;
    for (int16 j = 0; j < L_frame; j++)
    {
        i = ((int32)j * INV_FAC5) >> 13;       /* integer part = pos * 1/5 */

        frac--;
        if (frac)
        {
            *(pt_sig_u++) = AmrWbInterpol(&sig_d[i],
                                          fir_up[(FAC5-1) - frac],
                                          N_LOOP_COEF_UP);
        }
        else
        {
            *(pt_sig_u++) = sig_d[i+12 - NB_COEF_UP ];
            frac = FAC5;
        }
    }

}

/*----------------------------------------------------------------------------
; FUNCTION CODE
----------------------------------------------------------------------------*/


/* Fractional interpolation of signal at position (frac/resol) */


int16 AmrWbInterpol(                      /* return result of interpolation */
    int16 * x,                           /* input vector                   */
    const int16 *fir,                    /* filter coefficient             */
    int16 nb_coef                        /* number of coefficients         */
)
{
    int32 L_sum;
    const int16 *pt_fir = fir;

    int16 tmp1, tmp2, tmp3, tmp4;
    int16 *pt_x = x - nb_coef - (nb_coef << 1) + 1;


    tmp1 = *(pt_x++);
    tmp2 = *(pt_x++);
    tmp3 = *(pt_x++);
    tmp4 = *(pt_x++);
    L_sum = fxp_mac_16by16(tmp1, *(pt_fir++), 0x00002000L);
    L_sum = fxp_mac_16by16(tmp2, *(pt_fir++), L_sum);
    L_sum = fxp_mac_16by16(tmp3, *(pt_fir++), L_sum);
    L_sum = fxp_mac_16by16(tmp4, *(pt_fir++), L_sum);
    tmp1 = *(pt_x++);
    tmp2 = *(pt_x++);
    tmp3 = *(pt_x++);
    tmp4 = *(pt_x++);
    L_sum = fxp_mac_16by16(tmp1, *(pt_fir++), L_sum);
    L_sum = fxp_mac_16by16(tmp2, *(pt_fir++), L_sum);
    L_sum = fxp_mac_16by16(tmp3, *(pt_fir++), L_sum);
    L_sum = fxp_mac_16by16(tmp4, *(pt_fir++), L_sum);
    tmp1 = *(pt_x++);
    tmp2 = *(pt_x++);
    tmp3 = *(pt_x++);
    tmp4 = *(pt_x++);
    L_sum = fxp_mac_16by16(tmp1, *(pt_fir++), L_sum);
    L_sum = fxp_mac_16by16(tmp2, *(pt_fir++), L_sum);
    L_sum = fxp_mac_16by16(tmp3, *(pt_fir++), L_sum);
    L_sum = fxp_mac_16by16(tmp4, *(pt_fir++), L_sum);
    tmp1 = *(pt_x++);
    tmp2 = *(pt_x++);
    tmp3 = *(pt_x++);
    tmp4 = *(pt_x++);
    L_sum = fxp_mac_16by16(tmp1, *(pt_fir++), L_sum);
    L_sum = fxp_mac_16by16(tmp2, *(pt_fir++), L_sum);
    L_sum = fxp_mac_16by16(tmp3, *(pt_fir++), L_sum);
    L_sum = fxp_mac_16by16(tmp4, *(pt_fir++), L_sum);
    tmp1 = *(pt_x++);
    tmp2 = *(pt_x++);
    tmp3 = *(pt_x++);
    tmp4 = *(pt_x++);
    L_sum = fxp_mac_16by16(tmp1, *(pt_fir++), L_sum);
    L_sum = fxp_mac_16by16(tmp2, *(pt_fir++), L_sum);
    L_sum = fxp_mac_16by16(tmp3, *(pt_fir++), L_sum);
    L_sum = fxp_mac_16by16(tmp4, *(pt_fir++), L_sum);
    tmp1 = *(pt_x++);
    tmp2 = *(pt_x++);
    tmp3 = *(pt_x++);
    tmp4 = *(pt_x++);
    L_sum = fxp_mac_16by16(tmp1, *(pt_fir++), L_sum);
    L_sum = fxp_mac_16by16(tmp2, *(pt_fir++), L_sum);
    L_sum = fxp_mac_16by16(tmp3, *(pt_fir++), L_sum);
    L_sum = fxp_mac_16by16(tmp4, *(pt_fir++), L_sum);


    L_sum = shl_int32(L_sum, 2);               /* saturation can occur here */

    return ((int16)(L_sum >> 16));
}

