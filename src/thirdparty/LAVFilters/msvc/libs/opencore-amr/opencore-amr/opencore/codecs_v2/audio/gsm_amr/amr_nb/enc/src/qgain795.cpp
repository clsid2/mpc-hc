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



 Filename: qgain795.cpp
 Functions: MR795_gain_code_quant3
            MR795_gain_code_quant_mod
            MR795_gain_quant

------------------------------------------------------------------------------
 MODULE DESCRIPTION


------------------------------------------------------------------------------
*/

/*----------------------------------------------------------------------------
; INCLUDES
----------------------------------------------------------------------------*/
#include "qgain795.h"
#include "typedef.h"
#include "basic_op.h"
#include "cnst.h"
#include "log2.h"
#include "pow2.h"
#include "sqrt_l.h"
#include "g_adapt.h"
#include "calc_en.h"
#include "q_gain_p.h"


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
#define NB_QUA_CODE 32

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
 FUNCTION NAME: MR795_gain_code_quant3
------------------------------------------------------------------------------
 INPUT AND OUTPUT DEFINITIONS

 Inputs:
    exp_gcode0     -- Word16       -- predicted CB gain (exponent), Q0
    gcode0         -- Word16       -- predicted CB gain (norm.)
    g_pitch_cand[] -- Word16 array -- Pitch gain candidates (3),    Q14
    g_pitch_cind[] -- Word16 array -- Pitch gain cand. indices (3), Q0
    frac_coeff[]   -- Word16 array -- coefficients (5),             Q15
    exp_coeff[]    -- Word16 array -- energy coefficients (5),      Q0
                                      coefficients from calc_filt_ener()

 Outputs:
    gain_pit       -- Pointer to Word16 -- Pitch gain,                     Q14
    gain_pit_ind   -- Pointer to Word16 -- Pitch gain index,               Q0
    gain_cod       -- Pointer to Word16 -- Code gain,                      Q1
    gain_cod_ind   -- Pointer to Word16 -- Code gain index,                Q0
    qua_ener_MR122 -- Pointer to Word16 -- quantized energy error,         Q10
                                          (for MR122 MA predictor update)

    qua_ener -- Pointer to Word16 -- quantized energy error,       Q10
                                     (for other MA predictor update)

    pOverflow -- Pointer to Flag --  overflow indicator

 Returns:
    None

 Global Variables Used:
    None

 Local Variables Needed:
    None

------------------------------------------------------------------------------
 FUNCTION DESCRIPTION

 PURPOSE: Pre-quantization of codebook gains, given three possible
          LTP gains (using predicted codebook gain)
------------------------------------------------------------------------------
 REQUIREMENTS

 None

------------------------------------------------------------------------------
 REFERENCES

 qgain795.c, UMTS GSM AMR speech codec, R99 - Version 3.2.0, March 2, 2001

------------------------------------------------------------------------------
 PSEUDO-CODE


------------------------------------------------------------------------------
 CAUTION [optional]
 [State any special notes, constraints or cautions for users of this function]

------------------------------------------------------------------------------
*/

static void
MR795_gain_code_quant3(
    Word16 exp_gcode0,        /* i  : predicted CB gain (exponent), Q0  */
    Word16 gcode0,            /* i  : predicted CB gain (norm.),    Q14 */
    Word16 g_pitch_cand[],    /* i  : Pitch gain candidates (3),    Q14 */
    Word16 g_pitch_cind[],    /* i  : Pitch gain cand. indices (3), Q0  */
    Word16 frac_coeff[],      /* i  : coefficients (5),             Q15 */
    Word16 exp_coeff[],       /* i  : energy coefficients (5),      Q0  */
    /*      coefficients from calc_filt_ener()*/
    Word16 *gain_pit,         /* o  : Pitch gain,                   Q14 */
    Word16 *gain_pit_ind,     /* o  : Pitch gain index,             Q0  */
    Word16 *gain_cod,         /* o  : Code gain,                    Q1  */
    Word16 *gain_cod_ind,     /* o  : Code gain index,              Q0  */
    Word16 *qua_ener_MR122,   /* o  : quantized energy error,       Q10 */
    /*      (for MR122 MA predictor update)   */
    Word16 *qua_ener,         /* o  : quantized energy error,       Q10 */
    /*      (for other MA predictor update)   */
    const Word16* qua_gain_code_ptr, /* i : ptr to read-only table      */
    Flag   *pOverflow         /* o  : overflow indicator                */
)
{
    const Word16 *p;
    Word16 i;
    Word16 j;
    Word16 cod_ind;
    Word16 pit_ind;
    Word16 e_max;
    Word16 exp_code;
    Word16 g_pitch;
    Word16 g2_pitch;
    Word16 g_code;
    Word16 g2_code_h;
    Word16 g2_code_l;
    Word16 g_pit_cod_h;
    Word16 g_pit_cod_l;
    Word16 coeff[5];
    Word16 coeff_lo[5];
    Word16 exp_max[5];
    Word32 L_tmp;
    Word32 L_tmp0;
    Word32 dist_min;

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

    /* determine the scaling exponent for g_code: ec = ec0 - 10 */
    exp_code = exp_gcode0 - 10;

    /* calculate exp_max[i] = s[i]-1 */
    exp_max[0] = exp_coeff[0] - 13;
    exp_max[1] = exp_coeff[1] - 14;
    exp_max[2] = exp_coeff[2] + shl(exp_code, 1, pOverflow) + 15;
    exp_max[3] = exp_coeff[3] + exp_code;
    exp_max[4] = exp_coeff[4] + (exp_code + 1);


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
    for (i = 1; i < 5; i++)     /* implemented flattened */
    {
        if (exp_max[i] > e_max)
        {
            e_max = exp_max[i];
        }
    }

    e_max = add_16(e_max, 1, pOverflow);      /* To avoid overflow */

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
     *  For each of the candiates LTP gains in g_pitch_cand[], the terms *
     *  t[0..4] are calculated from the values in the table (and the     *
     *  pitch gain candidate) and summed up; the result is the mean      *
     *  squared error for the LPT/CB gain pair. The index for the mini-  *
     *  mum MSE is stored and finally used to retrieve the quantized CB  *
     *  gain                                                             *
     *-------------------------------------------------------------------*/

    /* start with "infinite" MSE */
    dist_min = MAX_32;
    cod_ind = 0;
    pit_ind = 0;

    /* loop through LTP gain candidates */
    for (j = 0; j < 3; j++)
    {
        /* pre-calculate terms only dependent on pitch gain */
        g_pitch = g_pitch_cand[j];
        g2_pitch = mult(g_pitch, g_pitch, pOverflow);
        L_tmp0 = Mpy_32_16(coeff[0], coeff_lo[0], g2_pitch, pOverflow);
        L_tmp0 = Mac_32_16(L_tmp0, coeff[1], coeff_lo[1], g_pitch, pOverflow);

        p = &qua_gain_code_ptr[0];
        for (i = 0; i < NB_QUA_CODE; i++)
        {
            g_code = *p++;                   /* this is g_fac        Q11 */
            p++;                             /* skip log2(g_fac)         */
            p++;                             /* skip 20*log10(g_fac)     */

            g_code = mult(g_code, gcode0, pOverflow);

            L_tmp = L_mult(g_code, g_code, pOverflow);
            L_Extract(L_tmp, &g2_code_h, &g2_code_l, pOverflow);

            L_tmp = L_mult(g_code, g_pitch, pOverflow);
            L_Extract(L_tmp, &g_pit_cod_h, &g_pit_cod_l, pOverflow);

            L_tmp = Mac_32(L_tmp0, coeff[2], coeff_lo[2],
                           g2_code_h, g2_code_l, pOverflow);
            L_tmp = Mac_32_16(L_tmp, coeff[3], coeff_lo[3],
                              g_code, pOverflow);
            L_tmp = Mac_32(L_tmp, coeff[4], coeff_lo[4],
                           g_pit_cod_h, g_pit_cod_l, pOverflow);

            /* store table index if MSE for this index is lower
               than the minimum MSE seen so far; also store the
               pitch gain for this (so far) lowest MSE          */
            if (L_tmp < dist_min)
            {
                dist_min = L_tmp;
                cod_ind = i;
                pit_ind = j;
            }
        }
    }

    /*------------------------------------------------------------------*
     *  read quantized gains and new values for MA predictor memories   *
     *  ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~   *
     *------------------------------------------------------------------*/

    /* Read the quantized gains */
    p = &qua_gain_code_ptr[(cod_ind<<2) - cod_ind];

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
    L_tmp = L_shr(L_tmp, 9 - exp_gcode0, pOverflow);
    *gain_cod = (Word16)(L_tmp >> 16);
    *gain_cod_ind = cod_ind;
    *gain_pit = g_pitch_cand[pit_ind];
    *gain_pit_ind = g_pitch_cind[pit_ind];
}


/*
------------------------------------------------------------------------------
 FUNCTION NAME: MR795_gain_code_quant_mod
------------------------------------------------------------------------------
 INPUT AND OUTPUT DEFINITIONS

 Inputs:
    gain_pit     -- Word16 -- pitch gain,                                   Q14
    exp_gcode0   -- Word16 -- predicted CB gain (exponent),                 Q0
    gcode0       -- Word16 -- predicted CB gain (norm.),                    Q14
    frac_en[]    -- Word16 array -- energy coefficients (4), fraction part, Q15
    exp_en[]     -- Word16 array -- energy coefficients (4), exponent part, Q0
    alpha        -- Word16 -- gain adaptor factor (>0),                     Q15

    gain_cod_unq -- Word16 -- Code gain (unquantized)
                              (scaling: Q10 - exp_gcode0)

    gain_cod     -- Pointer to Word16 -- Code gain (pre-/quantized),        Q1

 Outputs:
    qua_ener_MR122 -- Pointer to Word16 -- quantized energy error,       Q10
                                           (for MR122 MA predictor update)
    qua_ener       -- Pointer to Word16 -- quantized energy error,       Q10
                                           (for other MA predictor update)
    pOverflow      -- Pointer to Flag -- overflow indicator

 Returns:
    index of quantization (Word16)

 Global Variables Used:
    None

 Local Variables Needed:
    None

------------------------------------------------------------------------------
 FUNCTION DESCRIPTION

 PURPOSE: Modified quantization of the MR795 codebook gain

 Uses pre-computed energy coefficients in frac_en[]/exp_en[]

       frac_en[0]*2^exp_en[0] = <res res>   // LP residual energy
       frac_en[1]*2^exp_en[1] = <exc exc>   // LTP residual energy
       frac_en[2]*2^exp_en[2] = <exc code>  // LTP/CB innovation dot product
       frac_en[3]*2^exp_en[3] = <code code> // CB innovation energy
------------------------------------------------------------------------------
 REQUIREMENTS

 None

------------------------------------------------------------------------------
 REFERENCES

 qgain795.c, UMTS GSM AMR speech codec, R99 - Version 3.2.0, March 2, 2001

------------------------------------------------------------------------------
 PSEUDO-CODE


------------------------------------------------------------------------------
 CAUTION [optional]
 [State any special notes, constraints or cautions for users of this function]

------------------------------------------------------------------------------
*/

static Word16
MR795_gain_code_quant_mod(  /* o  : index of quantization.            */
    Word16 gain_pit,        /* i  : pitch gain,                   Q14 */
    Word16 exp_gcode0,      /* i  : predicted CB gain (exponent), Q0  */
    Word16 gcode0,          /* i  : predicted CB gain (norm.),    Q14 */
    Word16 frac_en[],       /* i  : energy coefficients (4),
                                    fraction part,                Q15 */
    Word16 exp_en[],        /* i  : energy coefficients (4),
                                    eponent part,                 Q0  */
    Word16 alpha,           /* i  : gain adaptor factor (>0),     Q15 */
    Word16 gain_cod_unq,    /* i  : Code gain (unquantized)           */
    /*      (scaling: Q10 - exp_gcode0)       */
    Word16 *gain_cod,       /* i/o: Code gain (pre-/quantized),   Q1  */
    Word16 *qua_ener_MR122, /* o  : quantized energy error,       Q10 */
    /*      (for MR122 MA predictor update)   */
    Word16 *qua_ener,       /* o  : quantized energy error,       Q10 */
    /*      (for other MA predictor update)   */
    const Word16* qua_gain_code_ptr, /* i : ptr to read-only ptr      */
    Flag   *pOverflow       /* o  : overflow indicator                */
)
{
    const Word16 *p;
    Word16 i;
    Word16 index;
    Word16 tmp;
    Word16 one_alpha;
    Word16 exp;
    Word16 e_max;

    Word16 g2_pitch;
    Word16 g_code;
    Word16 g2_code_h;
    Word16 g2_code_l;
    Word16 d2_code_h;
    Word16 d2_code_l;
    Word16 coeff[5];
    Word16 coeff_lo[5];
    Word16 exp_coeff[5];
    Word32 L_tmp;
    Word32 L_t0;
    Word32 L_t1;
    Word32 dist_min;
    Word16 gain_code;

    /*
      Steps in calculation of the error criterion (dist):
      ---------------------------------------------------

      underlined = constant; alp = FLP value of alpha, alpha = FIP
      ----------


        ExEn = gp^2 * LtpEn + 2.0*gp*gc[i] * XC + gc[i]^2 * InnEn;
               ------------   ------         --             -----

        aExEn= alp * ExEn
             = alp*gp^2*LtpEn + 2.0*alp*gp*XC* gc[i] + alp*InnEn* gc[i]^2
               --------------   -------------          ---------

             =         t[1]   +              t[2]    +          t[3]

        dist = d1 + d2;

          d1 = (1.0 - alp) * InnEn * (gcu - gc[i])^2 = t[4]
               -------------------    ---

          d2 =        alp  * (ResEn - 2.0 * sqrt(ResEn*ExEn) + ExEn);
                      ---     -----   ---        -----

             =        alp  * (sqrt(ExEn) - sqrt(ResEn))^2
                      ---                  -----------

             =               (sqrt(aExEn) - sqrt(alp*ResEn))^2
                                            ---------------

             =               (sqrt(aExEn) -       t[0]     )^2
                                                  ----

     */

    /*
     * calculate scalings of the constant terms
     */
    gain_code = shl(*gain_cod, (10 - exp_gcode0), pOverflow);   /* Q1  -> Q11 (-ec0) */
    g2_pitch = mult(gain_pit, gain_pit, pOverflow);               /* Q14 -> Q13        */
    /* 0 < alpha <= 0.5 => 0.5 <= 1-alpha < 1, i.e one_alpha is normalized  */
    one_alpha = add_16((32767 - alpha), 1, pOverflow);   /* 32768 - alpha */


    /*  alpha <= 0.5 -> mult. by 2 to keep precision; compensate in exponent */
    L_t1 = L_mult(alpha, frac_en[1], pOverflow);
    L_t1 = L_shl(L_t1, 1, pOverflow);
    tmp = (Word16)(L_t1 >> 16);

    /* directly store in 32 bit variable because no further mult. required */
    L_t1 = L_mult(tmp, g2_pitch, pOverflow);
    exp_coeff[1] = exp_en[1] - 15;


    tmp = (Word16)(L_shl(L_mult(alpha, frac_en[2], pOverflow), 1, pOverflow) >> 16);
    coeff[2] = mult(tmp, gain_pit, pOverflow);
    exp = exp_gcode0 - 10;
    exp_coeff[2] = add_16(exp_en[2], exp, pOverflow);


    /* alpha <= 0.5 -> mult. by 2 to keep precision; compensate in exponent */
    coeff[3] = (Word16)(L_shl(L_mult(alpha, frac_en[3], pOverflow), 1, pOverflow) >> 16);
    exp = shl(exp_gcode0, 1, pOverflow) - 7;
    exp_coeff[3] = add_16(exp_en[3], exp, pOverflow);


    coeff[4] = mult(one_alpha, frac_en[3], pOverflow);
    exp_coeff[4] = add_16(exp_coeff[3], 1, pOverflow);


    L_tmp = L_mult(alpha, frac_en[0], pOverflow);
    /* sqrt_l returns normalized value and 2*exponent
       -> result = val >> (exp/2)
       exp_coeff holds 2*exponent for c[0]            */
    /* directly store in 32 bit variable because no further mult. required */
    L_t0 = sqrt_l_exp(L_tmp, &exp, pOverflow);  /* normalization included in sqrt_l_exp */
    exp += 47;
    exp_coeff[0] = exp_en[0] - exp;

    /*
     * Determine the maximum exponent occuring in the distance calculation
     * and adjust all fractions accordingly (including a safety margin)
     *
     */

    /* find max(e[1..4],e[0]+31) */
    e_max = exp_coeff[0] + 31;
    for (i = 1; i <= 4; i++)
    {
        if (exp_coeff[i] > e_max)
        {
            e_max = exp_coeff[i];
        }
    }

    /* scale c[1]         (requires no further multiplication) */
    tmp = e_max - exp_coeff[1];
    L_t1 = L_shr(L_t1, tmp, pOverflow);

    /* scale c[2..4] (used in Mpy_32_16 in the quantizer loop) */
    for (i = 2; i <= 4; i++)
    {
        tmp = e_max - exp_coeff[i];
        L_tmp = ((Word32)coeff[i] << 16);
        L_tmp = L_shr(L_tmp, tmp, pOverflow);
        L_Extract(L_tmp, &coeff[i], &coeff_lo[i], pOverflow);
    }

    /* scale c[0]         (requires no further multiplication) */
    exp = e_max - 31;              /* new exponent */
    tmp = exp - exp_coeff[0];
    L_t0 = L_shr(L_t0, shr(tmp, 1, pOverflow), pOverflow);
    /* perform correction by 1/sqrt(2) if exponent difference is odd */
    if ((tmp & 0x1) != 0)
    {
        L_Extract(L_t0, &coeff[0], &coeff_lo[0], pOverflow);
        L_t0 = Mpy_32_16(coeff[0], coeff_lo[0],
                         23170, pOverflow);                    /* 23170 Q15 = 1/sqrt(2)*/
    }

    /* search the quantizer table for the lowest value
       of the search criterion                           */
    dist_min = MAX_32;
    index = 0;
    p = &qua_gain_code_ptr[0];

    for (i = 0; i < NB_QUA_CODE; i++)
    {
        g_code = *p++;                   /* this is g_fac (Q11)  */
        p++;                             /* skip log2(g_fac)     */
        p++;                             /* skip 20*log10(g_fac) */
        g_code = mult(g_code, gcode0, pOverflow);

        /* only continue if    gc[i]            < 2.0*gc
           which is equiv. to  g_code (Q10-ec0) < gain_code (Q11-ec0) */

        if (g_code >= gain_code)
        {
            break;
        }

        L_tmp = L_mult(g_code, g_code, pOverflow);
        L_Extract(L_tmp, &g2_code_h, &g2_code_l, pOverflow);

        tmp = sub(g_code, gain_cod_unq, pOverflow);
        L_tmp = L_mult(tmp, tmp, pOverflow);
        L_Extract(L_tmp, &d2_code_h, &d2_code_l, pOverflow);

        /* t2, t3, t4 */
        L_tmp = Mac_32_16(L_t1, coeff[2], coeff_lo[2], g_code, pOverflow);
        L_tmp = Mac_32(L_tmp,    coeff[3], coeff_lo[3], g2_code_h, g2_code_l, pOverflow);

        L_tmp = sqrt_l_exp(L_tmp, &exp, pOverflow);
        L_tmp = L_shr(L_tmp, shr(exp, 1, pOverflow), pOverflow);

        /* d2 */
        tmp = pv_round(L_sub(L_tmp, L_t0, pOverflow), pOverflow);
        L_tmp = L_mult(tmp, tmp, pOverflow);

        /* dist */
        L_tmp = Mac_32(L_tmp, coeff[4], coeff_lo[4], d2_code_h, d2_code_l, pOverflow);

        /* store table index if distance measure for this
            index is lower than the minimum seen so far   */
        if (L_tmp < dist_min)
        {
            dist_min = L_tmp;
            index = i;
        }
    }

    /*------------------------------------------------------------------*
     *  read quantized gains and new values for MA predictor memories   *
     *  ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~   *
     *------------------------------------------------------------------*/

    /* Read the quantized gains */
    p = &qua_gain_code_ptr[(index<<2) - index];
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
    L_tmp = L_shr(L_tmp, 9 - exp_gcode0, pOverflow);
    *gain_cod = (Word16)(L_tmp >> 16);

    return index;
}

/*
------------------------------------------------------------------------------
 FUNCTION NAME: MR795_gain_quant
------------------------------------------------------------------------------
 INPUT AND OUTPUT DEFINITIONS
MR795_gain_quant(


 Inputs:
    adapt_st      -- Pointer to GainAdaptState -- gain adapter state structure
    res           -- Word16 array -- LP residual,                  Q0
    exc           -- Word16 array -- LTP excitation (unfiltered),  Q0
    code          -- Word16 array -- CB innovation (unfiltered),   Q13
    frac_coeff    -- Word16 array -- coefficients (5),             Q15
    exp_coeff     -- Word16 array -- energy coefficients (5),      Q0
                                    coefficients from calc_filt_ener()
    exp_code_en   -- Word16 -- innovation energy (exponent), Q0
    frac_code_en  -- Word16 -- innovation energy (fraction), Q15
    exp_gcode0    -- Word16 -- predicted CB gain (exponent), Q0
    frac_gcode0   -- Word16 -- predicted CB gain (fraction), Q15
    L_subfr       -- Word16 -- Subframe length
    cod_gain_frac -- Word16 -- opt. codebook gain (fraction),Q15
    cod_gain_exp  -- Word16 -- opt. codebook gain (exponent), Q0
    gp_limit      -- Word16 -- pitch gain limit
    gain_pit      -- Pointer to Word16 -- Pitch gain,              Q14

 Output
    adapt_st       -- Pointer to GainAdaptState -- gain adapter state structure
    gain_pit       -- Pointer to Word16 -- Pitch gain,              Q14

    gain_pit       -- Pointer to Word16 -- Pitch gain,                   Q14
    gain_cod       -- Pointer to Word16 -- Code gain,                    Q1
    qua_ener_MR122 -- Pointer to Word16 -- quantized energy error,       Q10
                                           (for MR122 MA predictor update)

    qua_ener       -- Pointer to Word16 -- quantized energy error,       Q10
                                           (for other MA predictor update)

    anap           -- Double Pointer to Word16 -- Index of quantization
                                           (first gain pitch, then code pitch)

    pOverflow      -- Pointer to Flag -- overflow indicator

 Returns:
    None

 Global Variables Used:
    None

 Local Variables Needed:
    None

------------------------------------------------------------------------------
 FUNCTION DESCRIPTION

 pitch and codebook quantization for MR795
------------------------------------------------------------------------------
 REQUIREMENTS

 None

------------------------------------------------------------------------------
 REFERENCES

 qgain795.c, UMTS GSM AMR speech codec, R99 - Version 3.2.0, March 2, 2001

------------------------------------------------------------------------------
 PSEUDO-CODE


------------------------------------------------------------------------------
 CAUTION [optional]
 [State any special notes, constraints or cautions for users of this function]

------------------------------------------------------------------------------
*/

void
MR795_gain_quant(
    GainAdaptState *adapt_st, /* i/o: gain adapter state structure       */
    Word16 res[],             /* i  : LP residual,                  Q0   */
    Word16 exc[],             /* i  : LTP excitation (unfiltered),  Q0   */
    Word16 code[],            /* i  : CB innovation (unfiltered),   Q13  */
    Word16 frac_coeff[],      /* i  : coefficients (5),             Q15  */
    Word16 exp_coeff[],       /* i  : energy coefficients (5),      Q0   */
    /*      coefficients from calc_filt_ener() */
    Word16 exp_code_en,       /* i  : innovation energy (exponent), Q0   */
    Word16 frac_code_en,      /* i  : innovation energy (fraction), Q15  */
    Word16 exp_gcode0,        /* i  : predicted CB gain (exponent), Q0   */
    Word16 frac_gcode0,       /* i  : predicted CB gain (fraction), Q15  */
    Word16 L_subfr,           /* i  : Subframe length                    */
    Word16 cod_gain_frac,     /* i  : opt. codebook gain (fraction),Q15  */
    Word16 cod_gain_exp,      /* i  : opt. codebook gain (exponent), Q0  */
    Word16 gp_limit,          /* i  : pitch gain limit                   */
    Word16 *gain_pit,         /* i/o: Pitch gain,                   Q14  */
    Word16 *gain_cod,         /* o  : Code gain,                    Q1   */
    Word16 *qua_ener_MR122,   /* o  : quantized energy error,       Q10  */
    /*      (for MR122 MA predictor update)    */
    Word16 *qua_ener,         /* o  : quantized energy error,       Q10  */
    /*      (for other MA predictor update)    */
    Word16 **anap,            /* o  : Index of quantization              */
    /*      (first gain pitch, then code pitch)*/
    CommonAmrTbls* common_amr_tbls, /* i : ptr to struct of table ptrs   */
    Flag   *pOverflow         /* o  : overflow indicator                */
)
{
    Word16 frac_en[4];
    Word16 exp_en[4];
    Word16 ltpg, alpha, gcode0;
    Word16 g_pitch_cand[3];      /* pitch gain candidates   Q14 */
    Word16 g_pitch_cind[3];      /* pitch gain indices      Q0  */
    Word16 gain_pit_index;
    Word16 gain_cod_index;
    Word16 exp;
    Word16 gain_cod_unq;         /* code gain (unq.) Q(10-exp_gcode0)  */


    /* get list of candidate quantized pitch gain values
     * and corresponding quantization indices
     */
    gain_pit_index = q_gain_pitch(MR795, gp_limit, gain_pit,
                                  g_pitch_cand, g_pitch_cind, common_amr_tbls->qua_gain_pitch_ptr, pOverflow);

    /*-------------------------------------------------------------------*
     *  predicted codebook gain                                          *
     *  ~~~~~~~~~~~~~~~~~~~~~~~                                          *
     *  gc0     = 2^exp_gcode0 + 2^frac_gcode0                           *
     *                                                                   *
     *  gcode0 (Q14) = 2^14*2^frac_gcode0 = gc0 * 2^(14-exp_gcode0)      *
     *-------------------------------------------------------------------*/
    gcode0 = (Word16)(Pow2(14, frac_gcode0, pOverflow));           /* Q14 */

    /* pre-quantization of codebook gain
     * (using three pitch gain candidates);
     * result: best guess of pitch gain and code gain
     */
    MR795_gain_code_quant3(
        exp_gcode0, gcode0, g_pitch_cand, g_pitch_cind,
        frac_coeff, exp_coeff,
        gain_pit, &gain_pit_index, gain_cod, &gain_cod_index,
        qua_ener_MR122, qua_ener, common_amr_tbls->qua_gain_code_ptr, pOverflow);

    /* calculation of energy coefficients and LTP coding gain */
    calc_unfilt_energies(res, exc, code, *gain_pit, L_subfr,
                         frac_en, exp_en, &ltpg, pOverflow);

    /* run gain adaptor, calculate alpha factor to balance LTP/CB gain
     * (this includes the gain adaptor update)
     * Note: ltpg = 0 if frac_en[0] == 0, so the update is OK in that case
     */
    gain_adapt(adapt_st, ltpg, *gain_cod, &alpha, pOverflow);

    /* if this is a very low energy signal (threshold: see
     * calc_unfilt_energies) or alpha <= 0 then don't run the modified quantizer
     */
    if (frac_en[0] != 0 && alpha > 0)
    {
        /* innovation energy <cod cod> was already computed in gc_pred() */
        /* (this overwrites the LtpResEn which is no longer needed)      */
        frac_en[3] = frac_code_en;
        exp_en[3] = exp_code_en;

        /* store optimum codebook gain in Q(10-exp_gcode0) */
        exp = sub(cod_gain_exp, exp_gcode0, pOverflow) + 10;
        gain_cod_unq = shl(cod_gain_frac, exp, pOverflow);

        /* run quantization with modified criterion */
        gain_cod_index = MR795_gain_code_quant_mod(
                             *gain_pit, exp_gcode0, gcode0,
                             frac_en, exp_en, alpha, gain_cod_unq,
                             gain_cod, qua_ener_MR122, qua_ener, common_amr_tbls->qua_gain_code_ptr,
                             pOverflow); /* function result */
    }

    *(*anap)++ = gain_pit_index;
    *(*anap)++ = gain_cod_index;
}
