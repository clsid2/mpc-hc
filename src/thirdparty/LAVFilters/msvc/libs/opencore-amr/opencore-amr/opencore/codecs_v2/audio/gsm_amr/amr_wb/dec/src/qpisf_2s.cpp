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



 Filename: qpisf_2s.cpp

------------------------------------------------------------------------------
 INPUT AND OUTPUT DEFINITIONS

    int16 * seed          seed for the random ng

------------------------------------------------------------------------------
 FUNCTION DESCRIPTION

   Coding/Decoding of ISF parameters  with prediction.

   The ISF vector is quantized using two-stage VQ with split-by-2
   in 1st stage and split-by-5 (or 3)in the second stage.

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

#include "qisf_ns.h"
#include "qpisf_2s.h"

/*----------------------------------------------------------------------------
; MACROS
; Define module specific macros here
----------------------------------------------------------------------------*/


/*----------------------------------------------------------------------------
; DEFINES
; Include all pre-processor statements here. Include conditional
; compile variables also.
----------------------------------------------------------------------------*/
#define MU         10923           /* Prediction factor   (1.0/3.0) in Q15 */
#define N_SURV_MAX 4               /* 4 survivors max */
#define ALPHA      29491           /* 0. 9 in Q15     */
#define ONE_ALPHA (32768-ALPHA)    /* (1.0 - ALPHA) in Q15 */

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


/*-------------------------------------------------------------------*
 * routine:   Disf_2s_46b()                                          *
 *            ~~~~~~~~~                                              *
 * Decoding of ISF parameters                                        *
 *-------------------------------------------------------------------*/

void Dpisf_2s_46b(
    int16 * indice,   /* input:  quantization indices                       */
    int16 * isf_q,    /* output: quantized ISF in frequency domain (0..0.5) */
    int16 * past_isfq,/* i/0   : past ISF quantizer                    */
    int16 * isfold,   /* input : past quantized ISF                    */
    int16 * isf_buf,  /* input : isf buffer                            */
    int16 bfi,        /* input : Bad frame indicator                   */
    int16 enc_dec
)
{
    int16 ref_isf[M];
    int16 i, j, tmp;
    int32 L_tmp;


    if (bfi == 0)                          /* Good frame */
    {
        for (i = 0; i < 9; i++)
        {
            isf_q[i] = dico1_isf[(indice[0] << 3) + indice[0] + i];
        }
        for (i = 0; i < 7; i++)
        {
            isf_q[i + 9] = dico2_isf[(indice[1] << 3) - indice[1] + i];
        }

        for (i = 0; i < 3; i++)
        {
            isf_q[i]      += dico21_isf[indice[2] * 3 + i];
            isf_q[i + 3]  += dico22_isf[indice[3] * 3 + i];
            isf_q[i + 6]  += dico23_isf[indice[4] * 3 + i];
            isf_q[i + 9]  += dico24_isf[indice[5] * 3 + i];
            isf_q[i + 12] += dico25_isf[(indice[6] << 2) + i];
        }

        isf_q[i + 12] += dico25_isf[(indice[6] << 2) + i];

        for (i = 0; i < ORDER; i++)
        {
            tmp = isf_q[i];
            isf_q[i] += mean_isf[i];
            isf_q[i] += ((int32)MU * past_isfq[i]) >> 15;
            past_isfq[i] = tmp;
        }


        if (enc_dec)
        {
            for (i = 0; i < M; i++)
            {
                for (j = (L_MEANBUF - 1); j > 0; j--)
                {
                    isf_buf[j * M + i] = isf_buf[(j - 1) * M + i];
                }
                isf_buf[i] = isf_q[i];
            }
        }
    }
    else
    {                                      /* bad frame */
        for (i = 0; i < M; i++)
        {
            L_tmp = mul_16by16_to_int32(mean_isf[i], 8192);
            for (j = 0; j < L_MEANBUF; j++)
            {
                L_tmp = mac_16by16_to_int32(L_tmp, isf_buf[j * M + i], 8192);
            }
            ref_isf[i] = amr_wb_round(L_tmp);
        }

        /* use the past ISFs slightly shifted towards their mean */
        for (i = 0; i < ORDER; i++)
        {
            isf_q[i] = add_int16(mult_int16(ALPHA, isfold[i]), mult_int16(ONE_ALPHA, ref_isf[i]));
        }

        /* estimate past quantized residual to be used in next frame */

        for (i = 0; i < ORDER; i++)
        {
            tmp = add_int16(ref_isf[i], mult_int16(past_isfq[i], MU));      /* predicted ISF */
            past_isfq[i] = sub_int16(isf_q[i], tmp);
            past_isfq[i] >>= 1;           /* past_isfq[i] *= 0.5 */
        }

    }

    Reorder_isf(isf_q, ISF_GAP, ORDER);
}

/*
 * routine:   Disf_2s_36b()
 *            ~~~~~~~~~
 * Decoding of ISF parameters
 */

void Dpisf_2s_36b(
    int16 * indice,    /* input:  quantization indices                       */
    int16 * isf_q,     /* output: quantized ISF in frequency domain (0..0.5) */
    int16 * past_isfq, /* i/0   : past ISF quantizer                    */
    int16 * isfold,    /* input : past quantized ISF                    */
    int16 * isf_buf,   /* input : isf buffer                            */
    int16 bfi,         /* input : Bad frame indicator                   */
    int16 enc_dec
)
{
    int16 ref_isf[M];
    int16 i, j, tmp;
    int32 L_tmp;


    if (bfi == 0)                          /* Good frame */
    {
        for (i = 0; i < 9; i++)
        {
            isf_q[i] = dico1_isf[indice[0] * 9 + i];
        }
        for (i = 0; i < 7; i++)
        {
            isf_q[i + 9] = add_int16(dico2_isf[indice[1] * 7 + i], dico23_isf_36b[indice[4] * 7 + i]);
        }

        for (i = 0; i < 5; i++)
        {
            isf_q[i] = add_int16(isf_q[i], dico21_isf_36b[indice[2] * 5 + i]);
        }
        for (i = 0; i < 4; i++)
        {
            isf_q[i + 5] = add_int16(isf_q[i + 5], dico22_isf_36b[(indice[3] << 2) + i]);
        }

        for (i = 0; i < ORDER; i++)
        {
            tmp = isf_q[i];
            isf_q[i] = add_int16(tmp, mean_isf[i]);
            isf_q[i] = add_int16(isf_q[i], mult_int16(MU, past_isfq[i]));
            past_isfq[i] = tmp;
        }


        if (enc_dec)
        {
            for (i = 0; i < M; i++)
            {
                for (j = (L_MEANBUF - 1); j > 0; j--)
                {
                    isf_buf[j * M + i] = isf_buf[(j - 1) * M + i];
                }
                isf_buf[i] = isf_q[i];
            }
        }
    }
    else
    {                                      /* bad frame */
        for (i = 0; i < M; i++)
        {
            L_tmp = mul_16by16_to_int32(mean_isf[i], 8192);
            for (j = 0; j < L_MEANBUF; j++)
            {
                L_tmp = mac_16by16_to_int32(L_tmp, isf_buf[j * M + i], 8192);
            }

            ref_isf[i] = amr_wb_round(L_tmp);
        }

        /* use the past ISFs slightly shifted towards their mean */
        for (i = 0; i < ORDER; i++)
        {
            isf_q[i] = add_int16(mult_int16(ALPHA, isfold[i]), mult_int16(ONE_ALPHA, ref_isf[i]));
        }

        /* estimate past quantized residual to be used in next frame */

        for (i = 0; i < ORDER; i++)
        {
            tmp = add_int16(ref_isf[i], mult_int16(past_isfq[i], MU));      /* predicted ISF */
            past_isfq[i] = sub_int16(isf_q[i], tmp);
            past_isfq[i] >>=  1;           /* past_isfq[i] *= 0.5 */
        }
    }

    Reorder_isf(isf_q, ISF_GAP, ORDER);

    return;
}

/*
 * procedure  Reorder_isf()
 *            ~~~~~~~~~~~~~
 * To make sure that the  isfs are properly order and to keep a certain
 * minimum distance between consecutive isfs.
 *
 *    Argument         description                     in/out
 *    ~~~~~~~~         ~~~~~~~~~~~                     ~~~~~~
 *     isf[]           vector of isfs                    i/o
 *     min_dist        minimum required distance         i
 *     n               LPC order                         i
 */

void Reorder_isf(
    int16 * isf,                         /* (i/o) Q15: ISF in the frequency domain (0..0.5) */
    int16 min_dist,                      /* (i) Q15  : minimum distance to keep             */
    int16 n                              /* (i)      : number of ISF                        */
)
{
    int16 i, isf_min;

    isf_min = min_dist;

    for (i = 0; i < n - 1; i++)
    {
        if (isf[i] < isf_min)
        {
            isf[i] = isf_min;
        }
        isf_min = add_int16(isf[i], min_dist);
    }

    return;
}

