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



 Filename: calc_en.cpp
 Funtions: calc_unfilt_energies
           calc_filt_energies
           calc_target_energy

------------------------------------------------------------------------------
 MODULE DESCRIPTION

 This file contains the functions that calculate the energy coefficients
 for unfiltered and filtered excitation signals, the LTP coding gain, and
 the target energy.

------------------------------------------------------------------------------
*/


/*----------------------------------------------------------------------------
; INCLUDES
----------------------------------------------------------------------------*/

#include "calc_en.h"
#include "typedef.h"
#include "basicop_malloc.h"
#include "l_comp.h"
#include "cnst.h"
#include "log2.h"
#include "basic_op.h"

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


/*
------------------------------------------------------------------------------
 FUNCTION NAME: calc_unfilt_energies
------------------------------------------------------------------------------
 INPUT AND OUTPUT DEFINITIONS

 Inputs:
    res      = LP residual, buffer type Word16
    exc      = LTP excitation (unfiltered), buffer type Word16
    code     = CB innovation (unfiltered), buffer type Word16
    gain_pit = pitch gain,  type Word16
    L_subfr  = Subframe length, type Word16
    frac_en  = energy coefficients (4), fraction part, buffer type Word16
    exp_en   = energy coefficients (4), exponent part, buffer type Word16
    ltpg     = LTP coding gain (log2()), pointer to type Word16
    pOverflow= pointer to value indicating existence of overflow (Flag)

 Outputs:
    frac_en buffer containing new fractional parts of energy coefficients
    exp_en buffer containing new exponential parts of energy coefficients
    ltpg points to new LTP coding gain
    pOverflow = 1 if there is an overflow else it is zero.

 Returns:
    None.

 Global Variables Used:
    None

 Local Variables Needed:
    None

------------------------------------------------------------------------------
 FUNCTION DESCRIPTION

 This function calculates several energy coefficients for unfiltered
 excitation signals and the LTP coding gain

    frac_en[0]*2^exp_en[0] = <res res>    LP residual energy
    frac_en[1]*2^exp_en[1] = <exc exc>    LTP residual energy
    frac_en[2]*2^exp_en[2] = <exc code>   LTP/CB innovation dot product
    frac_en[3]*2^exp_en[3] = <lres lres>  LTP residual energy
    (lres = res - gain_pit*exc)
    ltpg = log2(LP_res_en / LTP_res_en)

------------------------------------------------------------------------------
 REQUIREMENTS

  None.

------------------------------------------------------------------------------
 REFERENCES

 calc_en.c, UMTS GSM AMR speech codec, R99 - Version 3.2.0, March 2, 2001

------------------------------------------------------------------------------
 PSEUDO-CODE

void
calc_unfilt_energies(
    Word16 res[],     // i  : LP residual,                               Q0
    Word16 exc[],     // i  : LTP excitation (unfiltered),               Q0
    Word16 code[],    // i  : CB innovation (unfiltered),                Q13
    Word16 gain_pit,  // i  : pitch gain,                                Q14
    Word16 L_subfr,   // i  : Subframe length

    Word16 frac_en[], // o  : energy coefficients (4), fraction part,    Q15
    Word16 exp_en[],  // o  : energy coefficients (4), exponent part,    Q0
    Word16 *ltpg      // o  : LTP coding gain (log2()),                  Q13
)
{
    Word32 s, L_temp;
    Word16 i, exp, tmp;
    Word16 ltp_res_en, pred_gain;
    Word16 ltpg_exp, ltpg_frac;

    // Compute residual energy
    s = L_mac((Word32) 0, res[0], res[0]);
    for (i = 1; i < L_subfr; i++)
        s = L_mac(s, res[i], res[i]);

    // ResEn := 0 if ResEn < 200.0 (= 400 Q1)
    if (L_sub (s, 400L) < 0)
    {
        frac_en[0] = 0;
        exp_en[0] = -15;
    }
    else
    {
        exp = norm_l(s);
        frac_en[0] = extract_h(L_shl(s, exp));
        exp_en[0] = sub(15, exp);
    }

    // Compute ltp excitation energy
    s = L_mac((Word32) 0, exc[0], exc[0]);
    for (i = 1; i < L_subfr; i++)
        s = L_mac(s, exc[i], exc[i]);

    exp = norm_l(s);
    frac_en[1] = extract_h(L_shl(s, exp));
    exp_en[1] = sub(15, exp);

    // Compute scalar product <exc[],code[]>
    s = L_mac((Word32) 0, exc[0], code[0]);
    for (i = 1; i < L_subfr; i++)
        s = L_mac(s, exc[i], code[i]);

    exp = norm_l(s);
    frac_en[2] = extract_h(L_shl(s, exp));
    exp_en[2] = sub(16-14, exp);

    // Compute energy of LTP residual
    s = 0L;
    for (i = 0; i < L_subfr; i++)
    {
        L_temp = L_mult(exc[i], gain_pit);
        L_temp = L_shl(L_temp, 1);
        tmp = sub(res[i], pv_round(L_temp)); // LTP residual, Q0
        s = L_mac (s, tmp, tmp);
    }

    exp = norm_l(s);
    ltp_res_en = extract_h (L_shl (s, exp));
    exp = sub (15, exp);

    frac_en[3] = ltp_res_en;
    exp_en[3] = exp;

    // calculate LTP coding gain, i.e. energy reduction LP res -> LTP res
    if (ltp_res_en > 0 && frac_en[0] != 0)
    {
        // gain = ResEn / LTPResEn
        pred_gain = div_s (shr (frac_en[0], 1), ltp_res_en);
        exp = sub (exp, exp_en[0]);

        // L_temp = ltpGain * 2^(30 + exp)
        L_temp = L_deposit_h (pred_gain);
        // L_temp = ltpGain * 2^27
        L_temp = L_shr (L_temp, add (exp, 3));

        // Log2 = log2() + 27
        Log2(L_temp, &ltpg_exp, &ltpg_frac);

        // ltpg = log2(LtpGain) * 2^13 --> range: +- 4 = +- 12 dB
        L_temp = L_Comp (sub (ltpg_exp, 27), ltpg_frac);
        *ltpg = pv_round (L_shl (L_temp, 13)); // Q13
    }
    else
    {
        *ltpg = 0;
    }
}


------------------------------------------------------------------------------
 CAUTION [optional]
 [State any special notes, constraints or cautions for users of this function]

------------------------------------------------------------------------------
*/

void calc_unfilt_energies(
    Word16 res[],     /* i  : LP residual,                               Q0  */
    Word16 exc[],     /* i  : LTP excitation (unfiltered),               Q0  */
    Word16 code[],    /* i  : CB innovation (unfiltered),                Q13 */
    Word16 gain_pit,  /* i  : pitch gain,                                Q14 */
    Word16 L_subfr,   /* i  : Subframe length                                */

    Word16 frac_en[], /* o  : energy coefficients (4), fraction part,    Q15 */
    Word16 exp_en[],  /* o  : energy coefficients (4), exponent part,    Q0  */
    Word16 *ltpg,     /* o  : LTP coding gain (log2()),                  Q13 */
    Flag   *pOverflow
)
{
    Word32 s1;      /* Intermediate energy accumulator */
    Word32 s2;      /* Intermediate energy accumulator */
    Word32 s3;      /* Intermediate energy accumulator */
    Word32 s4;      /* Intermediate energy accumulator */
    Word32 L_temp;      /* temporal 32 bits storage */

    Word16 i;       /* index used in all loops */
    Word16 exp;     /* nunmber of '0's or '1's before MSB != 0 */
    Word16 tmp1;        /* temporal storage */
    Word16 tmp2;        /* temporal storage */
    Word16 ltp_res_en;
    Word16 pred_gain;   /* predictor gain */
    Word16 ltpg_exp;    /* LTP gain (exponent) */
    Word16 ltpg_frac;   /* LTP gain (mantissa or fractional part) */

    s1 = 0;
    s2 = 0;
    s3 = 0;
    s4 = 0;

    /*----------------------------------------------------------------------------
    NOTE: Overflow is expected as a result of multiply and accumulated without
        scale down the inputs. This modification is not made at this point
        to have bit exact results with the pre-optimization code. (JT 6/20/00)

    ----------------------------------------------------------------------------*/

    for (i = 0; i < L_subfr; i++)
    {
        tmp1 = res[i];              /* avoid multiple accesses to memory */
        tmp2 = exc[i];

        s1 = amrnb_fxp_mac_16_by_16bb((Word32) tmp1, (Word32) tmp1, s1);   /* Compute residual energy */
        s2 = amrnb_fxp_mac_16_by_16bb((Word32) tmp2, (Word32) tmp2, s2);   /* Compute ltp excitation energy */
        s3 = amrnb_fxp_mac_16_by_16bb((Word32) tmp2, (Word32) code[i], s3);/* Compute scalar product */
        /* <exc[],code[]>         */

        L_temp = L_mult(tmp2, gain_pit, pOverflow);
        L_temp = L_shl(L_temp, 1, pOverflow);
        tmp2   = sub(tmp1, pv_round(L_temp, pOverflow), pOverflow);
        /* LTP residual, Q0 */
        s4     = L_mac(s4, tmp2, tmp2, pOverflow);
        /* Compute energy of LTP residual */
    }
    s1 = s1 << 1;
    s2 = s2 << 1;
    s3 = s3 << 1;

    if (s1 & MIN_32)
    {
        s1 = MAX_32;
        *pOverflow = 1;
    }

    /* ResEn := 0 if ResEn < 200.0 (= 400 Q1) */
    if (s1 < 400L)
    {
        frac_en[0] = 0;
        exp_en[0] = -15;
    }
    else
    {
        exp = norm_l(s1);
        frac_en[0] = (Word16)(L_shl(s1, exp, pOverflow) >> 16);
        exp_en[0] = (15 - exp);
    }

    if (s2 & MIN_32)
    {
        s2 = MAX_32;
        *pOverflow = 1;
    }

    exp = norm_l(s2);
    frac_en[1] = (Word16)(L_shl(s2, exp, pOverflow) >> 16);
    exp_en[1] = 15 - exp;

    /*  s3 is not always sum of squares */
    exp = norm_l(s3);
    frac_en[2] = (Word16)(L_shl(s3, exp, pOverflow) >> 16);
    exp_en[2]  = 2 - exp;

    exp = norm_l(s4);
    ltp_res_en = (Word16)(L_shl(s4, exp, pOverflow) >> 16);
    exp = 15 - exp;

    frac_en[3] = ltp_res_en;
    exp_en[3] = exp;

    /* calculate LTP coding gain, i.e. energy reduction LP res -> LTP res */

    if (ltp_res_en > 0 && frac_en[0] != 0)
    {
        /* gain = ResEn / LTPResEn */
        pred_gain = div_s(shr(frac_en[0], 1, pOverflow), ltp_res_en);
        exp = sub(exp, exp_en[0], pOverflow);

        /* L_temp = ltpGain * 2^(30 + exp) */
        L_temp = (Word32) pred_gain << 16;
        /* L_temp = ltpGain * 2^27 */
        L_temp = L_shr(L_temp, (Word16)(exp + 3), pOverflow);

        /* Log2 = log2() + 27 */
        Log2(L_temp, &ltpg_exp, &ltpg_frac, pOverflow);

        /* ltpg = log2(LtpGain) * 2^13 --> range: +- 4 = +- 12 dB */
        L_temp = L_Comp((ltpg_exp - 27), ltpg_frac, pOverflow);
        *ltpg = pv_round(L_shl(L_temp, 13, pOverflow), pOverflow);   /* Q13 */
    }
    else
    {
        *ltpg = 0;
    }

    return;
}

/****************************************************************************/


/*
------------------------------------------------------------------------------
 FUNCTION NAME: calc_filt_energies
------------------------------------------------------------------------------
 INPUT AND OUTPUT DEFINITIONS

 Inputs:
    mode = coder mode, type Mode
    xn = LTP target vector, buffer type Word16
    xn2 = CB target vector,  buffer type Word16
    y1 = Adaptive codebook,  buffer type Word16
    Y2 = Filtered innovative vector,  buffer type Word16
    g_coeff = Correlations <xn y1> <y1 y1>
    computed in G_pitch()  buffer type Word16
    frac_coeff = energy coefficients (5), fraction part, buffer type Word16
    exp_coeff = energy coefficients (5), exponent part, buffer type Word16
    cod_gain_frac = optimum codebook gain (fraction part), pointer type Word16
    cod_gain_exp = optimum codebook gain (exponent part), pointer type Word16
    pOverflow    = pointer to overflow indicator (Flag)

 Outputs:
    frac_coeff contains new fraction part energy coefficients
    exp_coeff contains new exponent part energy coefficients
    cod_gain_frac points to the new optimum codebook gain (fraction part)
    cod_gain_exp points to the new optimum codebook gain (exponent part)
    pOverflow = 1 if there is an overflow else it is zero.

 Returns:
    None.

 Global Variables Used:
    None

 Local Variables Needed:
    None

------------------------------------------------------------------------------
 FUNCTION DESCRIPTION

 This function calculates several energy coefficients for filtered
 excitation signals

 Compute coefficients need for the quantization and the optimum
 codebook gain gcu (for MR475 only).

    coeff[0] =    y1 y1
    coeff[1] = -2 xn y1
    coeff[2] =    y2 y2
    coeff[3] = -2 xn y2
    coeff[4] =  2 y1 y2

    gcu = <xn2, y2> / <y2, y2> (0 if <xn2, y2> <= 0)

 Product <y1 y1> and <xn y1> have been computed in G_pitch() and
 are in vector g_coeff[].

------------------------------------------------------------------------------
 REQUIREMENTS

 None.

------------------------------------------------------------------------------
 REFERENCES

 calc_en.c, UMTS GSM AMR speech codec, R99 - Version 3.2.0, March 2, 2001

------------------------------------------------------------------------------
 PSEUDO-CODE

void
calc_filt_energies(
    enum Mode mode,     // i  : coder mode
    Word16 xn[],        // i  : LTP target vector,                       Q0
    Word16 xn2[],       // i  : CB target vector,                        Q0
    Word16 y1[],        // i  : Adaptive codebook,                       Q0
    Word16 Y2[],        // i  : Filtered innovative vector,              Q12
    Word16 g_coeff[],   // i  : Correlations <xn y1> <y1 y1>
                        //      computed in G_pitch()

    Word16 frac_coeff[],// o  : energy coefficients (5), fraction part,  Q15
    Word16 exp_coeff[], // o  : energy coefficients (5), exponent part,  Q0
    Word16 *cod_gain_frac,// o: optimum codebook gain (fraction part),   Q15
    Word16 *cod_gain_exp  // o: optimum codebook gain (exponent part),   Q0
)
{
    Word32 s, ener_init;
    Word16 i, exp, frac;
    Word16 y2[L_SUBFR];

    if (sub(mode, MR795) == 0 || sub(mode, MR475) == 0)
    {
        ener_init = 0L;
    }
    else
    {
        ener_init = 1L;
    }

    for (i = 0; i < L_SUBFR; i++) {
        y2[i] = shr(Y2[i], 3);
    }

    frac_coeff[0] = g_coeff[0];
    exp_coeff[0] = g_coeff[1];
    frac_coeff[1] = negate(g_coeff[2]); // coeff[1] = -2 xn y1
    exp_coeff[1] = add(g_coeff[3], 1);


    // Compute scalar product <y2[],y2[]>

    s = L_mac(ener_init, y2[0], y2[0]);
    for (i = 1; i < L_SUBFR; i++)
        s = L_mac(s, y2[i], y2[i]);

    exp = norm_l(s);
    frac_coeff[2] = extract_h(L_shl(s, exp));
    exp_coeff[2] = sub(15 - 18, exp);

    // Compute scalar product -2*<xn[],y2[]>

    s = L_mac(ener_init, xn[0], y2[0]);
    for (i = 1; i < L_SUBFR; i++)
        s = L_mac(s, xn[i], y2[i]);

    exp = norm_l(s);
    frac_coeff[3] = negate(extract_h(L_shl(s, exp)));
    exp_coeff[3] = sub(15 - 9 + 1, exp);


    // Compute scalar product 2*<y1[],y2[]>

    s = L_mac(ener_init, y1[0], y2[0]);
    for (i = 1; i < L_SUBFR; i++)
        s = L_mac(s, y1[i], y2[i]);

    exp = norm_l(s);
    frac_coeff[4] = extract_h(L_shl(s, exp));
    exp_coeff[4] = sub(15 - 9 + 1, exp);

    if (sub(mode, MR475) == 0 || sub(mode, MR795) == 0)
    {
        // Compute scalar product <xn2[],y2[]>

        s = L_mac(ener_init, xn2[0], y2[0]);
        for (i = 1; i < L_SUBFR; i++)
            s = L_mac(s, xn2[i], y2[i]);

        exp = norm_l(s);
        frac = extract_h(L_shl(s, exp));
        exp = sub(15 - 9, exp);


        if (frac <= 0)
        {
            *cod_gain_frac = 0;
            *cod_gain_exp = 0;
        }
        else
        {
            //
              gcu = <xn2, y2> / c[2]
                  = (frac>>1)/frac[2]             * 2^(exp+1-exp[2])
                  = div_s(frac>>1, frac[2])*2^-15 * 2^(exp+1-exp[2])
                  = div_s * 2^(exp-exp[2]-14)

            *cod_gain_frac = div_s (shr (frac,1), frac_coeff[2]);
            *cod_gain_exp = sub (sub (exp, exp_coeff[2]), 14);

        }
    }
}

------------------------------------------------------------------------------
 CAUTION [optional]
 [State any special notes, constraints or cautions for users of this function]

------------------------------------------------------------------------------
*/

void calc_filt_energies(
    enum Mode mode,     /* i  : coder mode                                   */
    Word16 xn[],        /* i  : LTP target vector,                       Q0  */
    Word16 xn2[],       /* i  : CB target vector,                        Q0  */
    Word16 y1[],        /* i  : Adaptive codebook,                       Q0  */
    Word16 Y2[],        /* i  : Filtered innovative vector,              Q12 */
    Word16 g_coeff[],   /* i  : Correlations <xn y1> <y1 y1>                 */
    /*      computed in G_pitch()                        */
    Word16 frac_coeff[], /* o  : energy coefficients (5), fraction part, Q15 */
    Word16 exp_coeff[], /* o  : energy coefficients (5), exponent part,  Q0  */
    Word16 *cod_gain_frac, /* o  : optimum codebook gain (fraction part),Q15 */
    Word16 *cod_gain_exp, /* o  : optimum codebook gain (exponent part), Q0  */
    Flag   *pOverflow
)
{
    Word32 s1;      /* Intermediate energy accumulator  */
    Word32 s2;      /* Intermediate energy accumulator  */
    Word32 s3;      /* Intermediate energy accumulator  */

    Word16 i;       /* index used in all loops  */
    Word16 exp;     /* number of '0's or '1's before MSB != 0   */
    Word16 frac;        /* fractional part  */
    Word16 tmp;     /* temporal storage */
    Word16 scaled_y2[L_SUBFR];


    frac_coeff[0] = g_coeff[0];
    exp_coeff[0]  = g_coeff[1];
    frac_coeff[1] = negate(g_coeff[2]);    /* coeff[1] = -2 xn y1 */
    exp_coeff[1]  = g_coeff[3] + 1;

    if ((mode == MR795) || (mode == MR475))
    {
        s1 = 0L;
        s2 = 0L;
        s3 = 0L;
    }
    else
    {
        s1 = 1L;
        s2 = 1L;
        s3 = 1L;
    }

    for (i = 0; i < L_SUBFR; i++)
    {
        /* avoid multiple accesses to memory  */
        tmp   = (Y2[i] >> 3);
        scaled_y2[i] = tmp;

        /* Compute scalar product <scaled_y2[],scaled_y2[]> */
        s1 = L_mac(s1, tmp, tmp, pOverflow);

        /* Compute scalar product -2*<xn[],scaled_y2[]> */
        s2 = L_mac(s2, xn[i], tmp, pOverflow);

        /* Compute scalar product 2*<y1[],scaled_y2[]> */
        s3 = L_mac(s3, y1[i], tmp, pOverflow);
    }

    exp = norm_l(s1);
    frac_coeff[2] = (Word16)(L_shl(s1, exp, pOverflow) >> 16);
    exp_coeff[2] = (-3 - exp);

    exp = norm_l(s2);
    frac_coeff[3] = negate((Word16)(L_shl(s2, exp, pOverflow) >> 16));
    exp_coeff[3] = (7 - exp);

    exp = norm_l(s3);
    frac_coeff[4] = (Word16)(L_shl(s3, exp, pOverflow) >> 16);
    exp_coeff[4] = (7 - exp);


    if ((mode == MR795) || (mode == MR475))
    {
        /* Compute scalar product <xn2[],scaled_y2[]> */
        s1 = 0L;

        for (i = 0; i < L_SUBFR; i++)
        {
            s1 = amrnb_fxp_mac_16_by_16bb((Word32) xn2[i], (Word32)scaled_y2[i], s1);
        }

        s1 = s1 << 1;

        exp = norm_l(s1);
        frac = (Word16)(L_shl(s1, exp, pOverflow) >> 16);
        exp = (6 - exp);

        if (frac <= 0)
        {
            *cod_gain_frac = 0;
            *cod_gain_exp = 0;
        }
        else
        {
            /*
            gcu = <xn2, scaled_y2> / c[2]
                = (frac>>1)/frac[2]             * 2^(exp+1-exp[2])
                = div_s(frac>>1, frac[2])*2^-15 * 2^(exp+1-exp[2])
                = div_s * 2^(exp-exp[2]-14)
            */
            *cod_gain_frac = div_s(shr(frac, 1, pOverflow), frac_coeff[2]);
            *cod_gain_exp = ((exp - exp_coeff[2]) - 14);
        }
    }

    return;
}

/****************************************************************************/

/*
------------------------------------------------------------------------------
 FUNCTION NAME: calc_target_energy
------------------------------------------------------------------------------
 INPUT AND OUTPUT DEFINITIONS

 Inputs:
    xn =  LTP target vector, buffer to type Word16  Q0
    en_exp = optimum codebook gain (exponent part) pointer to type Word16
    en_frac = optimum codebook gain (fraction part) pointer to type Word16
    pOverflow = pointer to overflow indicator (Flag)

 Outputs:
    en_exp points to new optimum codebook gain (exponent part)
    en_frac points to new optimum codebook gain (fraction part)
    pOverflow = 1 if there is an overflow else it is zero.

 Returns:
    None.

 Global Variables Used:
    None

 Local Variables Needed:
    None

------------------------------------------------------------------------------
 FUNCTION DESCRIPTION

 This function calculates the target energy using the formula,
 en = <xn, xn>

------------------------------------------------------------------------------
 REQUIREMENTS

 None.

------------------------------------------------------------------------------
 REFERENCES

 calc_en.c, UMTS GSM AMR speech codec, R99 - Version 3.2.0, March 2, 2001

------------------------------------------------------------------------------
 PSEUDO-CODE

void
calc_target_energy(
    Word16 xn[],     // i: LTP target vector,                       Q0
    Word16 *en_exp,  // o: optimum codebook gain (exponent part),   Q0
    Word16 *en_frac  // o: optimum codebook gain (fraction part),   Q15
)
{
    Word32 s;
    Word16 i, exp;

    // Compute scalar product <xn[], xn[]>
    s = L_mac(0L, xn[0], xn[0]);
    for (i = 1; i < L_SUBFR; i++)
        s = L_mac(s, xn[i], xn[i]);

    // s = SUM 2*xn(i) * xn(i) = <xn xn> * 2
    exp = norm_l(s);
    *en_frac = extract_h(L_shl(s, exp));
    *en_exp = sub(16, exp);
}

------------------------------------------------------------------------------
 CAUTION [optional]
 [State any special notes, constraints or cautions for users of this function]

------------------------------------------------------------------------------
*/

void calc_target_energy(
    Word16 xn[],     /* i: LTP target vector,                       Q0  */
    Word16 *en_exp,  /* o: optimum codebook gain (exponent part),   Q0  */
    Word16 *en_frac, /* o: optimum codebook gain (fraction part),   Q15 */
    Flag   *pOverflow
)
{
    Word32 s;       /* Intermediate energy accumulator  */
    Word16 i;       /* index used in all loops  */
    Word16 exp;

    /* Compute scalar product <xn[], xn[]> */
    s = 0;
    for (i = 0; i < L_SUBFR; i++)
    {
        s = amrnb_fxp_mac_16_by_16bb((Word32) xn[i], (Word32) xn[i], s);
    }

    if (s < 0)
    {
        *pOverflow = 1;
        s = MAX_32;
    }

    /* s = SUM 2*xn(i) * xn(i) = <xn xn> * 2 */
    exp = norm_l(s);
    *en_frac = (Word16)(L_shl(s, exp, pOverflow) >> 16);
    *en_exp = (16 - exp);

    return;
}


