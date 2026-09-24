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



 Filename: dec_gain2_amr_wb.cpp

------------------------------------------------------------------------------
 INPUT AND OUTPUT DEFINITIONS

     int16 index,                 (i)     : index of quantization.
     int16 nbits,                 (i)     : number of bits (6 or 7)
     int16 code[],                (i) Q9  : Innovative vector.
     int16 L_subfr,               (i)     : Subframe lenght.
     int16 * gain_pit,            (o) Q14 : Pitch gain.
     int32 * gain_cod,            (o) Q16 : Code gain.
     int16 bfi,                   (i)     : bad frame indicator
     int16 prev_bfi,              (i)     : Previous BF indicator
     int16 state,                 (i)     : State of BFH
     int16 unusable_frame,        (i)     : UF indicator
     int16 vad_hist,              (i)     : number of non-speech frames
     int16 * mem                  (i/o)   : static memory (4 words)

------------------------------------------------------------------------------
 FUNCTION DESCRIPTION

    Decode the pitch and codebook gains

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

#include "qisf_ns.h"

/*----------------------------------------------------------------------------
; MACROS
; Define module specific macros here
----------------------------------------------------------------------------*/


/*----------------------------------------------------------------------------
; DEFINES
; Include all pre-processor statements here. Include conditional
; compile variables also.
----------------------------------------------------------------------------*/

#define MEAN_ENER    30
#define PRED_ORDER   4

#define L_LTPHIST 5

/*----------------------------------------------------------------------------
; LOCAL FUNCTION DEFINITIONS
; Function Prototype declaration
----------------------------------------------------------------------------*/

/*----------------------------------------------------------------------------
; LOCAL STORE/BUFFER/POINTER DEFINITIONS
; Variable declaration - defined here and used outside this module
----------------------------------------------------------------------------*/

const int16 pdown_unusable[7] = {32767, 31130, 29491, 24576, 7537, 1638, 328};
const int16 cdown_unusable[7] = {32767, 16384, 8192, 8192, 8192, 4915, 3277};

const int16 pdown_usable[7] = {32767, 32113, 31457, 24576, 7537, 1638, 328};
const int16 cdown_usable[7] = {32767, 32113, 32113, 32113, 32113, 32113, 22938};


/* MA prediction coeff ={0.5, 0.4, 0.3, 0.2} in Q13 */
const int16 pred[PRED_ORDER] = {4096, 3277, 2458, 1638};

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


/* output  :static memory (4 words)      */
void dec_gain2_amr_wb_init(int16 * mem)
{

    /* 4nd order quantizer energy predictor (init to -14.0 in Q10) */
    mem[0] = -14336;                          /* past_qua_en[0] */
    mem[1] = -14336;                          /* past_qua_en[1] */
    mem[2] = -14336;                          /* past_qua_en[2] */
    mem[3] = -14336;                          /* past_qua_en[3] */
    /* 4  *past_gain_pit  */
    /* 5  *past_gain_code  */
    /* 6  *prev_gc  */
    /* next 5  pbuf[]  */
    /* next 5  gbuf[]  */
    /* next 5  pbuf2[]  */
    pv_memset((void *)&mem[4], 0, 18*sizeof(*mem));

    mem[22] = 21845;

}

/*----------------------------------------------------------------------------
; FUNCTION CODE
----------------------------------------------------------------------------*/

void dec_gain2_amr_wb(
    int16 index,               /* (i)     : index of quantization.      */
    int16 nbits,               /* (i)     : number of bits (6 or 7)     */
    int16 code[],              /* (i) Q9  : Innovative vector.          */
    int16 L_subfr,             /* (i)     : Subframe lenght.            */
    int16 * gain_pit,          /* (o) Q14 : Pitch gain.                 */
    int32 * gain_cod,          /* (o) Q16 : Code gain.                  */
    int16 bfi,                 /* (i)     : bad frame indicator         */
    int16 prev_bfi,            /* (i)     : Previous BF indicator       */
    int16 state,               /* (i)     : State of BFH                */
    int16 unusable_frame,      /* (i)     : UF indicator                */
    int16 vad_hist,            /* (i)     : number of non-speech frames */
    int16 * mem                /* (i/o)   : static memory (4 words)     */
)
{
    const int16 *p;
    int16 *past_gain_pit, *past_gain_code, *past_qua_en, *gbuf, *pbuf, *prev_gc;
    int16 *pbuf2;
    int16 i, tmp, exp, frac, gcode0, exp_gcode0, qua_ener, gcode_inov;
    int16 tmp1, g_code;
    int16 tmp2;
    int32 L_tmp;

    past_qua_en = mem;
    past_gain_pit = mem + 4;
    past_gain_code = mem + 5;
    prev_gc = mem + 6;
    pbuf = mem + 7;
    gbuf = mem + 12;
    pbuf2 = mem + 17;

    /*
     *  Find energy of code and compute:
     *
     *    L_tmp = 1.0 / sqrt(energy of code/ L_subfr)
     */

    L_tmp = Dot_product12(code, code, L_subfr, &exp);
    exp -= 24;                /* exp: -18 (code in Q9), -6 (/L_subfr) */

    one_ov_sqrt_norm(&L_tmp, &exp);

    gcode_inov = extract_h(shl_int32(L_tmp, exp - 3));  /* g_code_inov in Q12 */

    /*
     * Case of erasure.
     */

    if (bfi != 0)
    {
        tmp = median5(&pbuf[2]);
        *past_gain_pit = tmp;

        if (*past_gain_pit > 15565)
        {
            *past_gain_pit = 15565;        /* 0.95 in Q14 */

        }

        if (unusable_frame != 0)
        {
            *gain_pit = mult_int16(pdown_unusable[state], *past_gain_pit);
        }
        else
        {
            *gain_pit = mult_int16(pdown_usable[state], *past_gain_pit);
        }
        tmp = median5(&gbuf[2]);

        if (vad_hist > 2)
        {
            *past_gain_code = tmp;
        }
        else
        {

            if (unusable_frame != 0)
            {
                *past_gain_code = mult_int16(cdown_unusable[state], tmp);
            }
            else
            {
                *past_gain_code = mult_int16(cdown_usable[state], tmp);
            }
        }

        /* update table of past quantized energies */

        tmp  = past_qua_en[3];
        tmp1 = past_qua_en[2];
        L_tmp  = tmp;
        L_tmp += tmp1;
        past_qua_en[3] = tmp;
        tmp  = past_qua_en[1];
        tmp1 = past_qua_en[0];
        L_tmp += tmp;
        L_tmp += tmp1;
        past_qua_en[2] = tmp;
        qua_ener = (int16)(L_tmp >> 3);
        past_qua_en[1] = tmp1;


        qua_ener -= 3072;    /* -3 in Q10 */

        if (qua_ener < -14336)
        {
            qua_ener = -14336;                /* -14 in Q10 */
        }

        past_qua_en[0] = qua_ener;


        for (i = 1; i < 5; i++)
        {
            gbuf[i - 1] = gbuf[i];
            pbuf[i - 1] = pbuf[i];
        }
        gbuf[4] = *past_gain_code;
        pbuf[4] = *past_gain_pit;


        /* adjust gain according to energy of code */
        /* past_gain_code(Q3) * gcode_inov(Q12) => Q16 */
        *gain_cod = mul_16by16_to_int32(*past_gain_code, gcode_inov);

        return;
    }
    /*
     * Compute gcode0
     *  = Sum(i=0,1) pred[i]*past_qua_en[i] + mean_ener - ener_code
     */

    L_tmp = L_deposit_h(MEAN_ENER);        /* MEAN_ENER in Q16 */
    L_tmp = shl_int32(L_tmp, 8);               /* From Q16 to Q24 */
    L_tmp = mac_16by16_to_int32(L_tmp, pred[0], past_qua_en[0]);      /* Q13*Q10 -> Q24 */
    L_tmp = mac_16by16_to_int32(L_tmp, pred[1], past_qua_en[1]);      /* Q13*Q10 -> Q24 */
    L_tmp = mac_16by16_to_int32(L_tmp, pred[2], past_qua_en[2]);      /* Q13*Q10 -> Q24 */
    L_tmp = mac_16by16_to_int32(L_tmp, pred[3], past_qua_en[3]);      /* Q13*Q10 -> Q24 */

    gcode0 = extract_h(L_tmp);             /* From Q24 to Q8  */

    /*
     * gcode0 = pow(10.0, gcode0/20)
     *        = pow(2, 3.321928*gcode0/20)
     *        = pow(2, 0.166096*gcode0)
     */

    L_tmp = ((int32)gcode0 * 5443) >> 7;      /* *0.166096 in Q15 -> Q24     */

    int32_to_dpf(L_tmp, &exp_gcode0, &frac);  /* Extract exponant of gcode0  */

    gcode0 = (int16)(power_of_2(14, frac));    /* Put 14 as exponant so that  */
    /* output of Pow2() will be:   */
    /* 16384 < Pow2() <= 32767     */
    exp_gcode0 -= 14;

    /* Read the quantized gains */

    if (nbits == 6)
    {
        p = &t_qua_gain6b[index<<1];
    }
    else
    {
        p = &t_qua_gain7b[index<<1];
    }
    *gain_pit = *p++;                         /* selected pitch gain in Q14 */
    g_code = *p++;                            /* selected code gain in Q11  */

    L_tmp = mul_16by16_to_int32(g_code, gcode0);        /* Q11*Q0 -> Q12 */
    L_tmp = shl_int32(L_tmp, exp_gcode0 + 4);   /* Q12 -> Q16 */

    *gain_cod = L_tmp;                        /* gain of code in Q16 */

    if (prev_bfi == 1)
    {
        L_tmp = mul_16by16_to_int32(*prev_gc, 5120);    /* prev_gc(Q3) * 1.25(Q12) = Q16 */
        /* if((*gain_cod > ((*prev_gc) * 1.25)) && (*gain_cod > 100.0)) */

        if ((*gain_cod > L_tmp) && (*gain_cod > 6553600))
        {
            *gain_cod = L_tmp;
        }
    }
    /* keep past gain code in Q3 for frame erasure (can saturate) */
    *past_gain_code = amr_wb_round(shl_int32(*gain_cod, 3));
    *past_gain_pit = *gain_pit;


    *prev_gc = *past_gain_code;
    tmp  = gbuf[1];
    tmp1 = pbuf[1];
    tmp2 = pbuf2[1];
    for (i = 1; i < 5; i++)
    {
        gbuf[i - 1]  = tmp;
        pbuf[i - 1]  = tmp1;
        pbuf2[i - 1] = tmp2;
        tmp  = gbuf[i];
        tmp1 = pbuf[i];
        tmp2 = pbuf2[i];
    }
    gbuf[4] = *past_gain_code;
    pbuf[4] = *past_gain_pit;
    pbuf2[4] = *past_gain_pit;


    /* adjust gain according to energy of code */
    int32_to_dpf(*gain_cod, &exp, &frac);
    L_tmp = mul_32by16(exp, frac, gcode_inov);

    *gain_cod = shl_int32(L_tmp, 3);              /* gcode_inov in Q12 */


    past_qua_en[3] = past_qua_en[2];
    past_qua_en[2] = past_qua_en[1];
    past_qua_en[1] = past_qua_en[0];

    /*
     * qua_ener = 20*log10(g_code)
     *          = 6.0206*log2(g_code)
     *          = 6.0206*(log2(g_codeQ11) - 11)
     */
    L_tmp = (int32)g_code;
    amrwb_log_2(L_tmp, &exp, &frac);
    exp -= 11;
    L_tmp = mul_32by16(exp, frac, 24660);   /* x 6.0206 in Q12 */

    /* update table of past quantized energies */

    past_qua_en[0] = (int16)(L_tmp >> 3); /* result in Q10 */

    return;
}


