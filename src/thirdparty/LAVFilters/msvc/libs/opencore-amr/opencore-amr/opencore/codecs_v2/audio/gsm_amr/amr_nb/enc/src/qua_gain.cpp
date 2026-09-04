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

    3GPP TS 26.073
    ANSI-C code for the Adaptive Multi-Rate (AMR) speech codec
    Available from http://www.3gpp.org

(C) 2004, 3GPP Organizational Partners (ARIB, ATIS, CCSA, ETSI, TTA, TTC)
Permission to distribute, modify and use this file under the standard license
terms listed above has been obtained from the copyright holder.
****************************************************************************************/
/*
------------------------------------------------------------------------------



 Filename: qua_gain.cpp

------------------------------------------------------------------------------
 MODULE DESCRIPTION

    Quantization of pitch and codebook gains.
------------------------------------------------------------------------------
*/

/*----------------------------------------------------------------------------
; INCLUDES
----------------------------------------------------------------------------*/
#include "qua_gain.h"
#include "typedef.h"
#include "basic_op.h"

#include "mode.h"
#include "cnst.h"
#include "pow2.h"
#include "gc_pred.h"

/*--------------------------------------------------------------------------*/
#ifdef __cplusplus
extern "C"
{
#endif

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
    ; LOCAL VARIABLE DEFINITIONS
    ; Variable declaration - defined here and used outside this module
    ----------------------------------------------------------------------------*/

    /*----------------------------------------------------------------------------
    ; EXTERNAL GLOBAL STORE/BUFFER/POINTER REFERENCES
    ; Declare variables used in this module but defined elsewhere
    ----------------------------------------------------------------------------*/

    /*--------------------------------------------------------------------------*/
#ifdef __cplusplus
}
#endif

/*
------------------------------------------------------------------------------
 FUNCTION NAME:
------------------------------------------------------------------------------
 INPUT AND OUTPUT DEFINITIONS


 Inputs:
    mode -- enum Mode -- AMR mode
    Word16 exp_gcode0  -- Word16 -- predicted CB gain (exponent),       Q0
    Word16 frac_gcode0 -- Word16 -- predicted CB gain (fraction),      Q15
    Word16 frac_coeff -- Word16 Array -- energy coeff. (5), fraction part, Q15
    Word16 exp_coeff  -- Word16 Array -- energy coeff. (5), exponent part,  Q0
                                    (frac_coeff and exp_coeff computed in
                                    calc_filt_energies())

    Word16 gp_limit -- Word16 --  pitch gain limit

 Outputs:
    Word16 *gain_pit -- Pointer to Word16 -- Pitch gain,               Q14
    Word16 *gain_cod -- Pointer to Word16 -- Code gain,                Q1
    Word16 *qua_ener_MR122 -- Pointer to Word16 -- quantized energy error,  Q10
                                                (for MR122 MA predictor update)
    Word16 *qua_ener -- Pointer to Word16 -- quantized energy error,        Q10
                                                (for other MA predictor update)
    Flag   *pOverflow -- Pointer to Flag -- overflow indicator

 Returns:
    Word16 -- index of quantization.

 Global Variables Used:


 Local Variables Needed:
    None

------------------------------------------------------------------------------
 FUNCTION DESCRIPTION

    Quantization of pitch and codebook gains.
------------------------------------------------------------------------------
 REQUIREMENTS

 None

------------------------------------------------------------------------------
 REFERENCES

 qua_gain.c, UMTS GSM AMR speech codec, R99 - Version 3.2.0, March 2, 2001

------------------------------------------------------------------------------
 PSEUDO-CODE


------------------------------------------------------------------------------
 CAUTION [optional]
 [State any special notes, constraints or cautions for users of this function]

------------------------------------------------------------------------------
*/


Word16
Qua_gain(                   /* o  : index of quantization.                 */
    enum Mode mode,         /* i  : AMR mode                               */
    Word16 exp_gcode0,      /* i  : predicted CB gain (exponent),      Q0  */
    Word16 frac_gcode0,     /* i  : predicted CB gain (fraction),      Q15 */
    Word16 frac_coeff[],    /* i  : energy coeff. (5), fraction part,  Q15 */
    Word16 exp_coeff[],     /* i  : energy coeff. (5), exponent part,  Q0  */
    /*      (frac_coeff and exp_coeff computed in  */
    /*       calc_filt_energies())                 */
    Word16 gp_limit,        /* i  : pitch gain limit                       */
    Word16 *gain_pit,       /* o  : Pitch gain,                        Q14 */
    Word16 *gain_cod,       /* o  : Code gain,                         Q1  */
    Word16 *qua_ener_MR122, /* o  : quantized energy error,            Q10 */
    /*      (for MR122 MA predictor update)        */
    Word16 *qua_ener,       /* o  : quantized energy error,            Q10 */
    /*      (for other MA predictor update)        */
    CommonAmrTbls* common_amr_tbls, /* i : ptr to struct of tables ptrs    */
    Flag   *pOverflow       /* o  : overflow indicator                     */
)
{
    const Word16 *p;
    Word16 i;
    Word16 j;
    Word16 index = 0;
    Word16 gcode0;
    Word16 e_max;
    Word16 temp;
    Word16 exp_code;
    Word16 g_pitch;
    Word16 g2_pitch;
    Word16 g_code;
    Word16 g2_code;
    Word16 g_pit_cod;
    Word16 coeff[5];
    Word16 coeff_lo[5];
    Word16 exp_max[5];
    Word32 L_tmp;
    Word32 L_tmp2;
    Word32 dist_min;
    const Word16 *table_gain;
    Word16 table_len;

    if (mode == MR102 || mode == MR74 || mode == MR67)
    {
        table_len = VQ_SIZE_HIGHRATES;
        table_gain = common_amr_tbls->table_gain_highrates_ptr;
    }
    else
    {
        table_len = VQ_SIZE_LOWRATES;
        table_gain = common_amr_tbls->table_gain_lowrates_ptr;
    }

    /*-------------------------------------------------------------------*
     *  predicted codebook gain                                          *
     *  ~~~~~~~~~~~~~~~~~~~~~~~                                          *
     *  gc0     = 2^exp_gcode0 + 2^frac_gcode0                           *
     *                                                                   *
     *  gcode0 (Q14) = 2^14*2^frac_gcode0 = gc0 * 2^(14-exp_gcode0)      *
     *-------------------------------------------------------------------*/

    gcode0 = (Word16)(Pow2(14, frac_gcode0, pOverflow));

    /*-------------------------------------------------------------------*
     *  Scaling considerations:                                          *
     *  ~~~~~~~~~~~~~~~~~~~~~~~                                          *
     *-------------------------------------------------------------------*/

    /*
     * The error energy (sum) to be minimized consists of five terms, t[0..4].
     *
     *                      t[0] =    gp^2  * <y1 y1>
     *                      t[1] = -2*gp    * <xn y1>
     *                      t[2] =    gc^2  * <y2 y2>
     *                      t[3] = -2*gc    * <xn y2>
     *                      t[4] =  2*gp*gc * <y1 y2>
     *
     */

    /* determine the scaling exponent for g_code: ec = ec0 - 11 */
    exp_code = exp_gcode0 - 11;

    /* calculate exp_max[i] = s[i]-1 */
    exp_max[0] = exp_coeff[0] - 13;
    exp_max[1] = exp_coeff[1] - 14;

    temp = shl(exp_code, 1, pOverflow);
    temp += 15;
    exp_max[2] = add_16(exp_coeff[2], temp, pOverflow);

    exp_max[3] = add_16(exp_coeff[3], exp_code, pOverflow);

    temp = exp_code + 1;
    exp_max[4] = add_16(exp_coeff[4], temp, pOverflow);


    /*-------------------------------------------------------------------*
     *  Find maximum exponent:                                           *
     *  ~~~~~~~~~~~~~~~~~~~~~~                                           *
     *                                                                   *
     *  For the sum operation, all terms must have the same scaling;     *
     *  that scaling should be low enough to prevent overflow. There-    *
     *  fore, the maximum scale is determined and all coefficients are   *
     *  re-scaled:                                                       *
     *                                                                   *
     *    e_max = max(exp_max[i]) + 1;                                   *
     *    e = exp_max[i]-e_max;         e <= 0!                          *
     *    c[i] = c[i]*2^e                                                *
     *-------------------------------------------------------------------*/

    e_max = exp_max[0];
    for (i = 1; i < 5; i++)
    {
        if (exp_max[i] > e_max)
        {
            e_max = exp_max[i];
        }
    }

    e_max++;

    for (i = 0; i < 5; i++)
    {
        j = e_max - exp_max[i];
        L_tmp = ((Word32)frac_coeff[i] << 16);
        L_tmp = L_shr(L_tmp, j, pOverflow);
        L_Extract(L_tmp, &coeff[i], &coeff_lo[i], pOverflow);
    }


    /*-------------------------------------------------------------------*
     *  Codebook search:                                                 *
     *  ~~~~~~~~~~~~~~~~                                                 *
     *                                                                   *
     *  For each pair (g_pitch, g_fac) in the table calculate the        *
     *  terms t[0..4] and sum them up; the result is the mean squared    *
     *  error for the quantized gains from the table. The index for the  *
     *  minimum MSE is stored and finally used to retrieve the quantized *
     *  gains                                                            *
     *-------------------------------------------------------------------*/

    /* start with "infinite" MSE */
    dist_min = MAX_32;

    p = &table_gain[0];

    for (i = 0; i < table_len; i++)
    {
        g_pitch = *p++;
        g_code = *p++;                   /* this is g_fac        */
        p++;                             /* skip log2(g_fac)     */
        p++;                             /* skip 20*log10(g_fac) */

        if (g_pitch <= gp_limit)
        {
            g_code = mult(g_code, gcode0, pOverflow);
            g2_pitch = mult(g_pitch, g_pitch, pOverflow);
            g2_code = mult(g_code, g_code, pOverflow);
            g_pit_cod = mult(g_code, g_pitch, pOverflow);

            L_tmp = Mpy_32_16(coeff[0], coeff_lo[0], g2_pitch, pOverflow);
            L_tmp2 = Mpy_32_16(coeff[1], coeff_lo[1], g_pitch, pOverflow);
            L_tmp = L_add(L_tmp, L_tmp2, pOverflow);

            L_tmp2 = Mpy_32_16(coeff[2], coeff_lo[2], g2_code, pOverflow);
            L_tmp = L_add(L_tmp, L_tmp2, pOverflow);

            L_tmp2 =  Mpy_32_16(coeff[3], coeff_lo[3], g_code, pOverflow);
            L_tmp = L_add(L_tmp, L_tmp2, pOverflow);

            L_tmp2 = Mpy_32_16(coeff[4], coeff_lo[4], g_pit_cod, pOverflow);
            L_tmp = L_add(L_tmp, L_tmp2, pOverflow);

            /* store table index if MSE for this index is lower
               than the minimum MSE seen so far */
            if (L_tmp < dist_min)
            {
                dist_min = L_tmp;
                index = i;
            }
        }
    }

    /*------------------------------------------------------------------*
     *  read quantized gains and new values for MA predictor memories   *
     *  ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~   *
     *------------------------------------------------------------------*/

    /* Read the quantized gains */
    p = &table_gain[shl(index, 2, pOverflow)];
    *gain_pit = *p++;
    g_code = *p++;
    *qua_ener_MR122 = *p++;
    *qua_ener = *p;

    /*------------------------------------------------------------------*
     *  calculate final fixed codebook gain:                            *
     *  ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~                            *
     *                                                                  *
     *   gc = gc0 * g                                                   *
     *------------------------------------------------------------------*/

    L_tmp = L_mult(g_code, gcode0, pOverflow);
    temp  = 10 - exp_gcode0;
    L_tmp = L_shr(L_tmp, temp, pOverflow);

    *gain_cod = (Word16)(L_tmp >> 16);

    return index;
}
