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



 Filename: qgain475.cpp
 Funtions: MR475_quant_store_results
           MR475_update_unq_pred
           MR475_gain_quant

------------------------------------------------------------------------------
 MODULE DESCRIPTION

 These modules handle the quantization of pitch and codebook gains for MR475.

------------------------------------------------------------------------------
*/


/*----------------------------------------------------------------------------
; INCLUDES
----------------------------------------------------------------------------*/
#include "qgain475.h"
#include "typedef.h"
#include "basic_op.h"
#include "mode.h"
#include "cnst.h"
#include "pow2.h"
#include "log2.h"

/*----------------------------------------------------------------------------
; MACROS
; Define module specific macros here
----------------------------------------------------------------------------*/


/*----------------------------------------------------------------------------
; DEFINES
; Include all pre-processor statements here. Include conditional
; compile variables also.
----------------------------------------------------------------------------*/
#define MR475_VQ_SIZE 256

/*----------------------------------------------------------------------------
; LOCAL FUNCTION DEFINITIONS
; Function Prototype declaration
----------------------------------------------------------------------------*/

/*----------------------------------------------------------------------------
; LOCAL VARIABLE DEFINITIONS
; Variable declaration - defined here and used outside this module
----------------------------------------------------------------------------*/

/* The table contains the following data:
 *
 *    g_pitch(0)        (Q14) // for sub-
 *    g_fac(0)          (Q12) // frame 0 and 2
 *    g_pitch(1)        (Q14) // for sub-
 *    g_fac(2)          (Q12) // frame 1 and 3
 *
 */
static const Word16 table_gain_MR475[MR475_VQ_SIZE*4] =
{
    /*g_pit(0), g_fac(0),      g_pit(1), g_fac(1) */
    812,          128,           542,      140,
    2873,         1135,          2266,     3402,
    2067,          563,         12677,      647,
    4132,         1798,          5601,     5285,
    7689,          374,          3735,      441,
    10912,         2638,         11807,     2494,
    20490,          797,          5218,      675,
    6724,         8354,          5282,     1696,
    1488,          428,          5882,      452,
    5332,         4072,          3583,     1268,
    2469,          901,         15894,     1005,
    14982,         3271,         10331,     4858,
    3635,         2021,          2596,      835,
    12360,         4892,         12206,     1704,
    13432,         1604,          9118,     2341,
    3968,         1538,          5479,     9936,
    3795,          417,          1359,      414,
    3640,         1569,          7995,     3541,
    11405,          645,          8552,      635,
    4056,         1377,         16608,     6124,
    11420,          700,          2007,      607,
    12415,         1578,         11119,     4654,
    13680,         1708,         11990,     1229,
    7996,         7297,         13231,     5715,
    2428,         1159,          2073,     1941,
    6218,         6121,          3546,     1804,
    8925,         1802,          8679,     1580,
    13935,         3576,         13313,     6237,
    6142,         1130,          5994,     1734,
    14141,         4662,         11271,     3321,
    12226,         1551,         13931,     3015,
    5081,        10464,          9444,     6706,
    1689,          683,          1436,     1306,
    7212,         3933,          4082,     2713,
    7793,          704,         15070,      802,
    6299,         5212,          4337,     5357,
    6676,          541,          6062,      626,
    13651,         3700,         11498,     2408,
    16156,          716,         12177,      751,
    8065,        11489,          6314,     2256,
    4466,          496,          7293,      523,
    10213,         3833,          8394,     3037,
    8403,          966,         14228,     1880,
    8703,         5409,         16395,     4863,
    7420,         1979,          6089,     1230,
    9371,         4398,         14558,     3363,
    13559,         2873,         13163,     1465,
    5534,         1678,         13138,    14771,
    7338,          600,          1318,      548,
    4252,         3539,         10044,     2364,
    10587,          622,         13088,      669,
    14126,         3526,          5039,     9784,
    15338,          619,          3115,      590,
    16442,         3013,         15542,     4168,
    15537,         1611,         15405,     1228,
    16023,         9299,          7534,     4976,
    1990,         1213,         11447,     1157,
    12512,         5519,          9475,     2644,
    7716,         2034,         13280,     2239,
    16011,         5093,          8066,     6761,
    10083,         1413,          5002,     2347,
    12523,         5975,         15126,     2899,
    18264,         2289,         15827,     2527,
    16265,        10254,         14651,    11319,
    1797,          337,          3115,      397,
    3510,         2928,          4592,     2670,
    7519,          628,         11415,      656,
    5946,         2435,          6544,     7367,
    8238,          829,          4000,      863,
    10032,         2492,         16057,     3551,
    18204,         1054,          6103,     1454,
    5884,         7900,         18752,     3468,
    1864,          544,          9198,      683,
    11623,         4160,          4594,     1644,
    3158,         1157,         15953,     2560,
    12349,         3733,         17420,     5260,
    6106,         2004,          2917,     1742,
    16467,         5257,         16787,     1680,
    17205,         1759,          4773,     3231,
    7386,         6035,         14342,    10012,
    4035,          442,          4194,      458,
    9214,         2242,          7427,     4217,
    12860,          801,         11186,      825,
    12648,         2084,         12956,     6554,
    9505,          996,          6629,      985,
    10537,         2502,         15289,     5006,
    12602,         2055,         15484,     1653,
    16194,         6921,         14231,     5790,
    2626,          828,          5615,     1686,
    13663,         5778,          3668,     1554,
    11313,         2633,          9770,     1459,
    14003,         4733,         15897,     6291,
    6278,         1870,          7910,     2285,
    16978,         4571,         16576,     3849,
    15248,         2311,         16023,     3244,
    14459,        17808,         11847,     2763,
    1981,         1407,          1400,      876,
    4335,         3547,          4391,     4210,
    5405,          680,         17461,      781,
    6501,         5118,          8091,     7677,
    7355,          794,          8333,     1182,
    15041,         3160,         14928,     3039,
    20421,          880,         14545,      852,
    12337,        14708,          6904,     1920,
    4225,          933,          8218,     1087,
    10659,         4084,         10082,     4533,
    2735,          840,         20657,     1081,
    16711,         5966,         15873,     4578,
    10871,         2574,          3773,     1166,
    14519,         4044,         20699,     2627,
    15219,         2734,         15274,     2186,
    6257,         3226,         13125,    19480,
    7196,          930,          2462,     1618,
    4515,         3092,         13852,     4277,
    10460,          833,         17339,      810,
    16891,         2289,         15546,     8217,
    13603,         1684,          3197,     1834,
    15948,         2820,         15812,     5327,
    17006,         2438,         16788,     1326,
    15671,         8156,         11726,     8556,
    3762,         2053,          9563,     1317,
    13561,         6790,         12227,     1936,
    8180,         3550,         13287,     1778,
    16299,         6599,         16291,     7758,
    8521,         2551,          7225,     2645,
    18269,         7489,         16885,     2248,
    17882,         2884,         17265,     3328,
    9417,        20162,         11042,     8320,
    1286,          620,          1431,      583,
    5993,         2289,          3978,     3626,
    5144,          752,         13409,      830,
    5553,         2860,         11764,     5908,
    10737,          560,          5446,      564,
    13321,         3008,         11946,     3683,
    19887,          798,          9825,      728,
    13663,         8748,          7391,     3053,
    2515,          778,          6050,      833,
    6469,         5074,          8305,     2463,
    6141,         1865,         15308,     1262,
    14408,         4547,         13663,     4515,
    3137,         2983,          2479,     1259,
    15088,         4647,         15382,     2607,
    14492,         2392,         12462,     2537,
    7539,         2949,         12909,    12060,
    5468,          684,          3141,      722,
    5081,         1274,         12732,     4200,
    15302,          681,          7819,      592,
    6534,         2021,         16478,     8737,
    13364,          882,          5397,      899,
    14656,         2178,         14741,     4227,
    14270,         1298,         13929,     2029,
    15477,         7482,         15815,     4572,
    2521,         2013,          5062,     1804,
    5159,         6582,          7130,     3597,
    10920,         1611,         11729,     1708,
    16903,         3455,         16268,     6640,
    9306,         1007,          9369,     2106,
    19182,         5037,         12441,     4269,
    15919,         1332,         15357,     3512,
    11898,        14141,         16101,     6854,
    2010,          737,          3779,      861,
    11454,         2880,          3564,     3540,
    9057,         1241,         12391,      896,
    8546,         4629,         11561,     5776,
    8129,          589,          8218,      588,
    18728,         3755,         12973,     3149,
    15729,          758,         16634,      754,
    15222,        11138,         15871,     2208,
    4673,          610,         10218,      678,
    15257,         4146,          5729,     3327,
    8377,         1670,         19862,     2321,
    15450,         5511,         14054,     5481,
    5728,         2888,          7580,     1346,
    14384,         5325,         16236,     3950,
    15118,         3744,         15306,     1435,
    14597,         4070,         12301,    15696,
    7617,         1699,          2170,      884,
    4459,         4567,         18094,     3306,
    12742,          815,         14926,      907,
    15016,         4281,         15518,     8368,
    17994,         1087,          2358,      865,
    16281,         3787,         15679,     4596,
    16356,         1534,         16584,     2210,
    16833,         9697,         15929,     4513,
    3277,         1085,          9643,     2187,
    11973,         6068,          9199,     4462,
    8955,         1629,         10289,     3062,
    16481,         5155,         15466,     7066,
    13678,         2543,          5273,     2277,
    16746,         6213,         16655,     3408,
    20304,         3363,         18688,     1985,
    14172,        12867,         15154,    15703,
    4473,         1020,          1681,      886,
    4311,         4301,          8952,     3657,
    5893,         1147,         11647,     1452,
    15886,         2227,          4582,     6644,
    6929,         1205,          6220,      799,
    12415,         3409,         15968,     3877,
    19859,         2109,          9689,     2141,
    14742,         8830,         14480,     2599,
    1817,         1238,          7771,      813,
    19079,         4410,          5554,     2064,
    3687,         2844,         17435,     2256,
    16697,         4486,         16199,     5388,
    8028,         2763,          3405,     2119,
    17426,         5477,         13698,     2786,
    19879,         2720,          9098,     3880,
    18172,         4833,         17336,    12207,
    5116,          996,          4935,      988,
    9888,         3081,          6014,     5371,
    15881,         1667,          8405,     1183,
    15087,         2366,         19777,     7002,
    11963,         1562,          7279,     1128,
    16859,         1532,         15762,     5381,
    14708,         2065,         20105,     2155,
    17158,         8245,         17911,     6318,
    5467,         1504,          4100,     2574,
    17421,         6810,          5673,     2888,
    16636,         3382,          8975,     1831,
    20159,         4737,         19550,     7294,
    6658,         2781,         11472,     3321,
    19397,         5054,         18878,     4722,
    16439,         2373,         20430,     4386,
    11353,        26526,         11593,     3068,
    2866,         1566,          5108,     1070,
    9614,         4915,          4939,     3536,
    7541,          878,         20717,      851,
    6938,         4395,         16799,     7733,
    10137,         1019,          9845,      964,
    15494,         3955,         15459,     3430,
    18863,          982,         20120,      963,
    16876,        12887,         14334,     4200,
    6599,         1220,          9222,      814,
    16942,         5134,          5661,     4898,
    5488,         1798,         20258,     3962,
    17005,         6178,         17929,     5929,
    9365,         3420,          7474,     1971,
    19537,         5177,         19003,     3006,
    16454,         3788,         16070,     2367,
    8664,         2743,          9445,    26358,
    10856,         1287,          3555,     1009,
    5606,         3622,         19453,     5512,
    12453,          797,         20634,      911,
    15427,         3066,         17037,    10275,
    18883,         2633,          3913,     1268,
    19519,         3371,         18052,     5230,
    19291,         1678,         19508,     3172,
    18072,        10754,         16625,     6845,
    3134,         2298,         10869,     2437,
    15580,         6913,         12597,     3381,
    11116,         3297,         16762,     2424,
    18853,         6715,         17171,     9887,
    12743,         2605,          8937,     3140,
    19033,         7764,         18347,     3880,
    20475,         3682,         19602,     3380,
    13044,        19373,         10526,    23124
};

/*
------------------------------------------------------------------------------
 FUNCTION NAME: MR475_quant_store_results
------------------------------------------------------------------------------
 INPUT AND OUTPUT DEFINITIONS

 Inputs:
    pred_st = pointer to structure of type gc_predState
    p = pointer to selected quantizer table entry (const Word16)
    gcode0 = predicted CB gain (Word16)
    exp_gcode0 = exponent of predicted CB gain (Word16)
    gain_pit = pointer to Pitch gain (Word16)
    gain_cod = pointer to Code gain (Word16)

 Outputs:
    pred_st points to the updated structure of type gc_predState
    gain_pit points to Pitch gain
    gain_cod points to Code gain
    pOverflow points to overflow indicator (Flag)

 Returns:
    None.

 Global Variables Used:
    None.

 Local Variables Needed:
    None.

------------------------------------------------------------------------------
 FUNCTION DESCRIPTION

 This function calculates the final fixed codebook gain and the predictor
 update values, and updates the gain predictor.

------------------------------------------------------------------------------
 REQUIREMENTS

 None.

------------------------------------------------------------------------------
 REFERENCES

 qgain475.c, UMTS GSM AMR speech codec, R99 - Version 3.2.0, March 2, 2001

------------------------------------------------------------------------------
 PSEUDO-CODE

static void MR475_quant_store_results(

    gc_predState *pred_st, // i/o: gain predictor state struct
    const Word16 *p,       // i  : pointer to selected quantizer table entry
    Word16 gcode0,         // i  : predicted CB gain,     Q(14 - exp_gcode0)
    Word16 exp_gcode0,     // i  : exponent of predicted CB gain,        Q0
    Word16 *gain_pit,      // o  : Pitch gain,                           Q14
    Word16 *gain_cod       // o  : Code gain,                            Q1
)
{

    Word16 g_code, exp, frac, tmp;
    Word32 L_tmp;

    Word16 qua_ener_MR122; // o  : quantized energy error, MR122 version Q10
    Word16 qua_ener;       // o  : quantized energy error,               Q10

    // Read the quantized gains
    *gain_pit = *p++;
    g_code = *p++;

    //------------------------------------------------------------------*
     *  calculate final fixed codebook gain:                            *
     *  ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~                            *
     *                                                                  *
     *   gc = gc0 * g                                                   *
     *------------------------------------------------------------------

    L_tmp = L_mult(g_code, gcode0);
    L_tmp = L_shr(L_tmp, sub(10, exp_gcode0));
    *gain_cod = extract_h(L_tmp);

    //------------------------------------------------------------------*
     *  calculate predictor update values and update gain predictor:    *
     *  ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~    *
     *                                                                  *
     *   qua_ener       = log2(g)                                       *
     *   qua_ener_MR122 = 20*log10(g)                                   *
     *------------------------------------------------------------------

    Log2 (L_deposit_l (g_code), &exp, &frac); // Log2(x Q12) = log2(x) + 12
    exp = sub(exp, 12);

    tmp = shr_r (frac, 5);
    qua_ener_MR122 = add (tmp, shl (exp, 10));

    L_tmp = Mpy_32_16(exp, frac, 24660); // 24660 Q12 ~= 6.0206 = 20*log10(2)
    qua_ener = pv_round (L_shl (L_tmp, 13)); // Q12 * Q0 = Q13 -> Q10

    gc_pred_update(pred_st, qua_ener_MR122, qua_ener);
}

------------------------------------------------------------------------------
 CAUTION [optional]
 [State any special notes, constraints or cautions for users of this function]

------------------------------------------------------------------------------
*/

static void MR475_quant_store_results(
    gc_predState *pred_st, /* i/o: gain predictor state struct               */
    const Word16 *p,       /* i  : pointer to selected quantizer table entry */
    Word16 gcode0,         /* i  : predicted CB gain,     Q(14 - exp_gcode0) */
    Word16 exp_gcode0,     /* i  : exponent of predicted CB gain,        Q0  */
    Word16 *gain_pit,      /* o  : Pitch gain,                           Q14 */
    Word16 *gain_cod,      /* o  : Code gain,                            Q1  */
    Flag   *pOverflow      /* o  : overflow indicator                        */
)
{
    Word16 g_code;
    Word16 exp;
    Word16 frac;
    Word16 tmp;
    Word32 L_tmp;

    Word16 qua_ener_MR122; /* o  : quantized energy error, MR122 version Q10 */
    Word16 qua_ener;       /* o  : quantized energy error,               Q10 */


    /* Read the quantized gains */
    *gain_pit = *p++;
    g_code = *p++;

    /*------------------------------------------------------------------*
     *  calculate final fixed codebook gain:                            *
     *  ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~                            *
     *                                                                  *
     *   gc = gc0 * g                                                   *
     *------------------------------------------------------------------*/

    L_tmp = ((Word32) g_code * gcode0) << 1;
    tmp   = 10 - exp_gcode0;
    L_tmp = L_shr(L_tmp, tmp, pOverflow);
    *gain_cod = (Word16)(L_tmp >> 16);

    /*------------------------------------------------------------------*
     *  calculate predictor update values and update gain predictor:    *
     *  ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~    *
     *                                                                  *
     *   qua_ener       = log2(g)                                       *
     *   qua_ener_MR122 = 20*log10(g)                                   *
     *------------------------------------------------------------------*/

    /* Log2(x Q12) = log2(x) + 12 */
    Log2((Word32) g_code, &exp, &frac, pOverflow);
    exp -= 12;

    tmp = shr_r(frac, 5, pOverflow);
    qua_ener_MR122 = exp << 10;
    qua_ener_MR122 = tmp + qua_ener_MR122;

    /* 24660 Q12 ~= 6.0206 = 20*log10(2) */
    L_tmp = Mpy_32_16(exp, frac, 24660, pOverflow);
    L_tmp = L_tmp << 13;

    /* Q12 * Q0 = Q13 -> Q10 */
    qua_ener = (Word16)((L_tmp + (Word32) 0x00008000L) >> 16);

    gc_pred_update(pred_st, qua_ener_MR122, qua_ener);

    return;
}

/****************************************************************************/


/*
------------------------------------------------------------------------------
 FUNCTION NAME: MR475_update_unq_pred
------------------------------------------------------------------------------
 INPUT AND OUTPUT DEFINITIONS

 Inputs:
    pred_st = pointer to structure of type gc_predState
    exp_gcode0 = predicted CB gain (exponent MSW) (Word16)
    frac_gcode0 = predicted CB gain (exponent LSW) (Word16)
    cod_gain_exp = optimum codebook gain (exponent)(Word16)
    cod_gain_frac = optimum codebook gain (fraction) (Word16)

 Outputs:
    pred_st points to the updated structure of type gc_predState
    pOverflow points to overflow indicator (Flag)

 Returns:
    None.

 Global Variables Used:
    None.

 Local Variables Needed:
    None.

------------------------------------------------------------------------------
 FUNCTION DESCRIPTION

 This module uses the optimum codebook gain and updates the "unquantized"
 gain predictor with the (bounded) prediction error.

------------------------------------------------------------------------------
 REQUIREMENTS

 None.

------------------------------------------------------------------------------
 REFERENCES

 qgain475.c, UMTS GSM AMR speech codec, R99 - Version 3.2.0, March 2, 2001

------------------------------------------------------------------------------
 PSEUDO-CODE

void
MR475_update_unq_pred(
    gc_predState *pred_st, // i/o: gain predictor state struct
    Word16 exp_gcode0,     // i  : predicted CB gain (exponent MSW),  Q0
    Word16 frac_gcode0,    // i  : predicted CB gain (exponent LSW),  Q15
    Word16 cod_gain_exp,   // i  : optimum codebook gain (exponent),  Q0
    Word16 cod_gain_frac   // i  : optimum codebook gain (fraction),  Q15
)
{
    Word16 tmp, exp, frac;
    Word16 qua_ener, qua_ener_MR122;
    Word32 L_tmp;

    // calculate prediction error factor (given optimum CB gain gcu):
    //   predErrFact = gcu / gcode0
    //   (limit to MIN_PRED_ERR_FACT <= predErrFact <= MAX_PRED_ERR_FACT
    //    -> limit qua_ener*)
    //
    // calculate prediction error (log):
    //
    //   qua_ener_MR122 = log2(predErrFact)
    //   qua_ener       = 20*log10(predErrFact)

    if (cod_gain_frac <= 0)
    {
        // if gcu <= 0 -> predErrFact = 0 < MIN_PRED_ERR_FACT
        // -> set qua_ener(_MR122) directly
        qua_ener = MIN_QUA_ENER;
        qua_ener_MR122 = MIN_QUA_ENER_MR122;
    }
    else
    {
        // convert gcode0 from DPF to standard fraction/exponent format
        // with normalized frac, i.e. 16384 <= frac <= 32767
        // Note: exponent correction (exp=exp-14) is done after div_s
        frac_gcode0 = extract_l (Pow2 (14, frac_gcode0));

        // make sure cod_gain_frac < frac_gcode0  for div_s
        if (sub(cod_gain_frac, frac_gcode0) >= 0)
        {
            cod_gain_frac = shr (cod_gain_frac, 1);
            cod_gain_exp = add (cod_gain_exp, 1);
        }

        // predErrFact
        //   = gcu / gcode0
        //   = cod_gain_frac/frac_gcode0 * 2^(cod_gain_exp-(exp_gcode0-14))
        //   = div_s (c_g_f, frac_gcode0)*2^-15 * 2^(c_g_e-exp_gcode0+14)
        //   = div_s * 2^(cod_gain_exp-exp_gcode0 - 1)

        frac = div_s (cod_gain_frac, frac_gcode0);
        tmp = sub (sub (cod_gain_exp, exp_gcode0), 1);

        Log2 (L_deposit_l (frac), &exp, &frac);
        exp = add (exp, tmp);

        // calculate prediction error (log2, Q10)
        qua_ener_MR122 = shr_r (frac, 5);
        qua_ener_MR122 = add (qua_ener_MR122, shl (exp, 10));

        if (sub(qua_ener_MR122, MIN_QUA_ENER_MR122) < 0)
        {
            qua_ener = MIN_QUA_ENER;
            qua_ener_MR122 = MIN_QUA_ENER_MR122;
        }
        else if (sub(qua_ener_MR122, MAX_QUA_ENER_MR122) > 0)
        {
            qua_ener = MAX_QUA_ENER;
            qua_ener_MR122 = MAX_QUA_ENER_MR122;
        }
        else
        {
            // calculate prediction error (20*log10, Q10)
            L_tmp = Mpy_32_16(exp, frac, 24660);
            // 24660 Q12 ~= 6.0206 = 20*log10(2)
            qua_ener = pv_round (L_shl (L_tmp, 13));
            // Q12 * Q0 = Q13 -> Q26 -> Q10
        }
    }

    // update MA predictor memory
    gc_pred_update(pred_st, qua_ener_MR122, qua_ener);
}

------------------------------------------------------------------------------
 CAUTION [optional]
 [State any special notes, constraints or cautions for users of this function]

------------------------------------------------------------------------------
*/

void MR475_update_unq_pred(
    gc_predState *pred_st, /* i/o: gain predictor state struct            */
    Word16 exp_gcode0,     /* i  : predicted CB gain (exponent MSW),  Q0  */
    Word16 frac_gcode0,    /* i  : predicted CB gain (exponent LSW),  Q15 */
    Word16 cod_gain_exp,   /* i  : optimum codebook gain (exponent),  Q0  */
    Word16 cod_gain_frac,  /* i  : optimum codebook gain (fraction),  Q15 */
    Flag   *pOverflow      /* o  : overflow indicator                     */
)
{
    Word16 tmp;
    Word16 exp;
    Word16 frac;
    Word16 qua_ener;
    Word16 qua_ener_MR122;
    Word32 L_tmp;

    /* calculate prediction error factor (given optimum CB gain gcu):
     *
     *   predErrFact = gcu / gcode0
     *   (limit to MIN_PRED_ERR_FACT <= predErrFact <= MAX_PRED_ERR_FACT
     *    -> limit qua_ener*)
     *
     * calculate prediction error (log):
     *
     *   qua_ener_MR122 = log2(predErrFact)
     *   qua_ener       = 20*log10(predErrFact)
     *
     */

    if (cod_gain_frac <= 0)
    {
        /* if gcu <= 0 -> predErrFact = 0 < MIN_PRED_ERR_FACT */
        /* -> set qua_ener(_MR122) directly                   */
        qua_ener = MIN_QUA_ENER;
        qua_ener_MR122 = MIN_QUA_ENER_MR122;
    }
    else
    {
        /* convert gcode0 from DPF to standard fraction/exponent format */
        /* with normalized frac, i.e. 16384 <= frac <= 32767            */
        /* Note: exponent correction (exp=exp-14) is done after div_s   */
        frac_gcode0 = (Word16)(Pow2(14, frac_gcode0, pOverflow));

        /* make sure cod_gain_frac < frac_gcode0  for div_s */
        if (cod_gain_frac >= frac_gcode0)
        {
            cod_gain_frac >>= 1;
            cod_gain_exp += 1;
        }

        /*
          predErrFact
             = gcu / gcode0
             = cod_gain_frac/frac_gcode0 * 2^(cod_gain_exp-(exp_gcode0-14))
             = div_s (c_g_f, frac_gcode0)*2^-15 * 2^(c_g_e-exp_gcode0+14)
             = div_s * 2^(cod_gain_exp-exp_gcode0 - 1)
        */
        frac = div_s(cod_gain_frac, frac_gcode0);
        tmp = cod_gain_exp - exp_gcode0;
        tmp -= 1;

        Log2((Word32) frac, &exp, &frac, pOverflow);
        exp += tmp;

        /* calculate prediction error (log2, Q10) */
        qua_ener_MR122 = shr_r(frac, 5, pOverflow);
        tmp = exp << 10;
        qua_ener_MR122 += tmp;

        if (qua_ener_MR122 > MAX_QUA_ENER_MR122)
        {
            qua_ener = MAX_QUA_ENER;
            qua_ener_MR122 = MAX_QUA_ENER_MR122;
        }
        else
        {
            /* calculate prediction error (20*log10, Q10) */
            L_tmp = Mpy_32_16(exp, frac, 24660, pOverflow);
            /* 24660 Q12 ~= 6.0206 = 20*log10(2) */
            L_tmp =  L_shl(L_tmp, 13, pOverflow);
            qua_ener = pv_round(L_tmp, pOverflow);

            /* Q12 * Q0 = Q13 -> Q26 -> Q10     */
        }
    }

    /* update MA predictor memory */
    gc_pred_update(pred_st, qua_ener_MR122, qua_ener);


    return;
}

/****************************************************************************/


/*
------------------------------------------------------------------------------
 FUNCTION NAME: MR475_gain_quant
------------------------------------------------------------------------------
 INPUT AND OUTPUT DEFINITIONS

 Inputs:
    pred_st = pointer to structure of type gc_predState
    sf0_exp_gcode0 = predicted CB gain (exponent) (Word16)
    f0_frac_gcode0 = predicted CB gain (fraction) (Word16)
    sf0_exp_coeff = energy coeff. (exponent part) (Word16)
    sf0_frac_coeff = energy coeff. ((fraction part) (Word16)
    sf0_exp_target_en = exponent of target energy (Word16)
    sf0_frac_target_en = fraction of target energy (Word16)
    sf1_code_nosharp = innovative codebook vector  (Word16)
    sf1_exp_gcode0 = predicted CB gain (exponent) (Word16)
    sf1_frac_gcode0 = predicted CB gain (fraction) (Word16)
    sf1_exp_coeff = energy coeff. (exponent part) (Word16)
    sf1_frac_coeff = energy coeff. (fraction part) (Word16)
    sf1_exp_target_en = exponent of target energy (Word16)
    sf1_frac_target_en = fraction of target energy (Word16)
    gp_limit = pitch gain limit (Word16)
    sf0_gain_pit = pointer to Pitch gain (Word16)
    sf0_gain_cod = pointer to Code gain (Word16)
    sf1_gain_pit = pointer to Pitch gain (Word16)
    sf1_gain_cod = pointer to Code gain (Word16)

 Outputs:
    pred_st points to the updated structure of type gc_predState
    sf0_gain_pit points to Pitch gain
    sf0_gain_cod points to Code gain
    sf1_gain_pit points to Pitch gain
    sf1_gain_cod points to Code gain

 Returns:
    index = index of quantization

 Global Variables Used:
    None.

 Local Variables Needed:
    None.

------------------------------------------------------------------------------
 FUNCTION DESCRIPTION

 This module provides quantization of pitch and codebook gains for two
 subframes using the predicted codebook gain.

------------------------------------------------------------------------------
 REQUIREMENTS

 None.

------------------------------------------------------------------------------
 REFERENCES

 qgain475.c, UMTS GSM AMR speech codec, R99 - Version 3.2.0, March 2, 2001

------------------------------------------------------------------------------
 PSEUDO-CODE

Word16
MR475_gain_quant(              // o  : index of quantization.
    gc_predState *pred_st,     // i/o: gain predictor state struct

                               // data from subframe 0 (or 2)
    Word16 sf0_exp_gcode0,     // i  : predicted CB gain (exponent),      Q0
    Word16 sf0_frac_gcode0,    // i  : predicted CB gain (fraction),      Q15
    Word16 sf0_exp_coeff[],    // i  : energy coeff. (5), exponent part,  Q0
    Word16 sf0_frac_coeff[],   // i  : energy coeff. (5), fraction part,  Q15
                               //      (frac_coeff and exp_coeff computed in
                               //       calc_filt_energies())
    Word16 sf0_exp_target_en,  // i  : exponent of target energy,         Q0
    Word16 sf0_frac_target_en, // i  : fraction of target energy,         Q15

                               // data from subframe 1 (or 3)
    Word16 sf1_code_nosharp[], // i  : innovative codebook vector (L_SUBFR)
                               //      (whithout pitch sharpening)
    Word16 sf1_exp_gcode0,     // i  : predicted CB gain (exponent),      Q0
    Word16 sf1_frac_gcode0,    // i  : predicted CB gain (fraction),      Q15
    Word16 sf1_exp_coeff[],    // i  : energy coeff. (5), exponent part,  Q0
    Word16 sf1_frac_coeff[],   // i  : energy coeff. (5), fraction part,  Q15
                               //      (frac_coeff and exp_coeff computed in
                               //       calc_filt_energies())
    Word16 sf1_exp_target_en,  // i  : exponent of target energy,         Q0
    Word16 sf1_frac_target_en, // i  : fraction of target energy,         Q15

    Word16 gp_limit,           // i  : pitch gain limit

    Word16 *sf0_gain_pit,      // o  : Pitch gain,                        Q14
    Word16 *sf0_gain_cod,      // o  : Code gain,                         Q1

    Word16 *sf1_gain_pit,      // o  : Pitch gain,                        Q14
    Word16 *sf1_gain_cod       // o  : Code gain,                         Q1
)
{
    const Word16 *p;
    Word16 i, index = 0;
    Word16 tmp;
    Word16 exp;
    Word16 sf0_gcode0, sf1_gcode0;
    Word16 g_pitch, g2_pitch, g_code, g2_code, g_pit_cod;
    Word16 coeff[10], coeff_lo[10], exp_max[10];  // 0..4: sf0; 5..9: sf1
    Word32 L_tmp, dist_min;

     *-------------------------------------------------------------------*
     *  predicted codebook gain                                          *
     *  ~~~~~~~~~~~~~~~~~~~~~~~                                          *
     *  gc0     = 2^exp_gcode0 + 2^frac_gcode0                           *
     *                                                                   *
     *  gcode0 (Q14) = 2^14*2^frac_gcode0 = gc0 * 2^(14-exp_gcode0)      *
     *-------------------------------------------------------------------*

    sf0_gcode0 = extract_l(Pow2(14, sf0_frac_gcode0));
    sf1_gcode0 = extract_l(Pow2(14, sf1_frac_gcode0));

     * For each subframe, the error energy (sum) to be minimized consists
     * of five terms, t[0..4].
     *
     *                      t[0] =    gp^2  * <y1 y1>
     *                      t[1] = -2*gp    * <xn y1>
     *                      t[2] =    gc^2  * <y2 y2>
     *                      t[3] = -2*gc    * <xn y2>
     *                      t[4] =  2*gp*gc * <y1 y2>
     *

    // sf 0
    // determine the scaling exponent for g_code: ec = ec0 - 11
    exp = sub(sf0_exp_gcode0, 11);

    // calculate exp_max[i] = s[i]-1
    exp_max[0] = sub(sf0_exp_coeff[0], 13);
    exp_max[1] = sub(sf0_exp_coeff[1], 14);
    exp_max[2] = add(sf0_exp_coeff[2], add(15, shl(exp, 1)));
    exp_max[3] = add(sf0_exp_coeff[3], exp);
    exp_max[4] = add(sf0_exp_coeff[4], add(1, exp));

    // sf 1
    // determine the scaling exponent for g_code: ec = ec0 - 11
    exp = sub(sf1_exp_gcode0, 11);

    // calculate exp_max[i] = s[i]-1
    exp_max[5] = sub(sf1_exp_coeff[0], 13);
    exp_max[6] = sub(sf1_exp_coeff[1], 14);
    exp_max[7] = add(sf1_exp_coeff[2], add(15, shl(exp, 1)));
    exp_max[8] = add(sf1_exp_coeff[3], exp);
    exp_max[9] = add(sf1_exp_coeff[4], add(1, exp));

     *-------------------------------------------------------------------*
     *  Gain search equalisation:                                        *
     *  ~~~~~~~~~~~~~~~~~~~~~~~~~                                        *
     *  The MSE for the two subframes is weighted differently if there   *
     *  is a big difference in the corresponding target energies         *
     *-------------------------------------------------------------------*

    // make the target energy exponents the same by de-normalizing the
    // fraction of the smaller one. This is necessary to be able to compare
    // them

    exp = sf0_exp_target_en - sf1_exp_target_en;
    if (exp > 0)
    {
        sf1_frac_target_en = shr (sf1_frac_target_en, exp);
    }
    else
    {
        sf0_frac_target_en = shl (sf0_frac_target_en, exp);
    }

    // assume no change of exponents
    exp = 0;

    // test for target energy difference; set exp to +1 or -1 to scale
    // up/down coefficients for sf 1

    tmp = shr_r (sf1_frac_target_en, 1);   // tmp = ceil(0.5*en(sf1))
    if (sub (tmp, sf0_frac_target_en) > 0) // tmp > en(sf0)?
    {
        // target_energy(sf1) > 2*target_energy(sf0)
        //   -> scale up MSE(sf0) by 2 by adding 1 to exponents 0..4
        exp = 1;
    }
    else
    {
        tmp = shr (add (sf0_frac_target_en, 3), 2); // tmp=ceil(0.25*en(sf0))
        if (sub (tmp, sf1_frac_target_en) > 0)      // tmp > en(sf1)?
        {
            // target_energy(sf1) < 0.25*target_energy(sf0)
            //   -> scale down MSE(sf0) by 0.5 by subtracting 1 from
            //      coefficients 0..4
            exp = -1;
        }
    }

    for (i = 0; i < 5; i++)
    {
        exp_max[i] = add (exp_max[i], exp);
    }

     *-------------------------------------------------------------------*
     *  Find maximum exponent:                                           *
     *  ~~~~~~~~~~~~~~~~~~~~~~                                           *
     *                                                                   *
     *  For the sum operation, all terms must have the same scaling;     *
     *  that scaling should be low enough to prevent overflow. There-    *
     *  fore, the maximum scale is determined and all coefficients are   *
     *  re-scaled:                                                       *
     *                                                                   *
     *    exp = max(exp_max[i]) + 1;                                     *
     *    e = exp_max[i]-exp;         e <= 0!                            *
     *    c[i] = c[i]*2^e                                                *
     *-------------------------------------------------------------------*

    exp = exp_max[0];
    for (i = 1; i < 10; i++)
    {
        if (sub(exp_max[i], exp) > 0)
        {
            exp = exp_max[i];
        }
    }
    exp = add(exp, 1);      // To avoid overflow

    p = &sf0_frac_coeff[0];
    for (i = 0; i < 5; i++) {
        tmp = sub(exp, exp_max[i]);
        L_tmp = L_deposit_h(*p++);
        L_tmp = L_shr(L_tmp, tmp);
        L_Extract(L_tmp, &coeff[i], &coeff_lo[i]);
    }
    p = &sf1_frac_coeff[0];
    for (; i < 10; i++) {
        tmp = sub(exp, exp_max[i]);
        L_tmp = L_deposit_h(*p++);
        L_tmp = L_shr(L_tmp, tmp);
        L_Extract(L_tmp, &coeff[i], &coeff_lo[i]);
    }

    //-------------------------------------------------------------------*
     *  Codebook search:                                                 *
     *  ~~~~~~~~~~~~~~~~                                                 *
     *                                                                   *
     *  For each pair (g_pitch, g_fac) in the table calculate the        *
     *  terms t[0..4] and sum them up; the result is the mean squared    *
     *  error for the quantized gains from the table. The index for the  *
     *  minimum MSE is stored and finally used to retrieve the quantized *
     *  gains                                                            *
     *-------------------------------------------------------------------

    // start with "infinite" MSE
    dist_min = MAX_32;

    p = &table_gain_MR475[0];

    for (i = 0; i < MR475_VQ_SIZE; i++)
    {
        // subframe 0 (and 2) calculations
        g_pitch = *p++;
        g_code = *p++;

        g_code = mult(g_code, sf0_gcode0);
        g2_pitch = mult(g_pitch, g_pitch);
        g2_code = mult(g_code, g_code);
        g_pit_cod = mult(g_code, g_pitch);

        L_tmp = Mpy_32_16(       coeff[0], coeff_lo[0], g2_pitch);
        L_tmp = Mac_32_16(L_tmp, coeff[1], coeff_lo[1], g_pitch);
        L_tmp = Mac_32_16(L_tmp, coeff[2], coeff_lo[2], g2_code);
        L_tmp = Mac_32_16(L_tmp, coeff[3], coeff_lo[3], g_code);
        L_tmp = Mac_32_16(L_tmp, coeff[4], coeff_lo[4], g_pit_cod);

        tmp = sub (g_pitch, gp_limit);

        // subframe 1 (and 3) calculations
        g_pitch = *p++;
        g_code = *p++;

        if (tmp <= 0 && sub(g_pitch, gp_limit) <= 0)
        {
            g_code = mult(g_code, sf1_gcode0);
            g2_pitch = mult(g_pitch, g_pitch);
            g2_code = mult(g_code, g_code);
            g_pit_cod = mult(g_code, g_pitch);

            L_tmp = Mac_32_16(L_tmp, coeff[5], coeff_lo[5], g2_pitch);
            L_tmp = Mac_32_16(L_tmp, coeff[6], coeff_lo[6], g_pitch);
            L_tmp = Mac_32_16(L_tmp, coeff[7], coeff_lo[7], g2_code);
            L_tmp = Mac_32_16(L_tmp, coeff[8], coeff_lo[8], g_code);
            L_tmp = Mac_32_16(L_tmp, coeff[9], coeff_lo[9], g_pit_cod);

            // store table index if MSE for this index is lower
               than the minimum MSE seen so far
            if (L_sub(L_tmp, dist_min) < (Word32) 0)
            {
                dist_min = L_tmp;
                index = i;
            }
        }
    }

     *------------------------------------------------------------------*
     *  read quantized gains and update MA predictor memories           *
     *  ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~           *
     *------------------------------------------------------------------*

    // for subframe 0, the pre-calculated gcode0/exp_gcode0 are the same
    // as those calculated from the "real" predictor using quantized gains
    tmp = shl(index, 2);
    MR475_quant_store_results(pred_st,
                              &table_gain_MR475[tmp],
                              sf0_gcode0,
                              sf0_exp_gcode0,
                              sf0_gain_pit,
                              sf0_gain_cod);

    // calculate new predicted gain for subframe 1 (this time using
    // the real, quantized gains)
    gc_pred(pred_st, MR475, sf1_code_nosharp,
            &sf1_exp_gcode0, &sf1_frac_gcode0,
            &sf0_exp_gcode0, &sf0_gcode0); // last two args are dummy
    sf1_gcode0 = extract_l(Pow2(14, sf1_frac_gcode0));

    tmp = add (tmp, 2);
    MR475_quant_store_results(pred_st,
                              &table_gain_MR475[tmp],
                              sf1_gcode0,
                              sf1_exp_gcode0,
                              sf1_gain_pit,
                              sf1_gain_cod);

    return index;
}

------------------------------------------------------------------------------
 CAUTION [optional]
 [State any special notes, constraints or cautions for users of this function]

------------------------------------------------------------------------------
*/

Word16 MR475_gain_quant(       /* o  : index of quantization.                 */
    gc_predState *pred_st,     /* i/o: gain predictor state struct            */

    /* data from subframe 0 (or 2) */
    Word16 sf0_exp_gcode0,     /* i  : predicted CB gain (exponent),      Q0  */
    Word16 sf0_frac_gcode0,    /* i  : predicted CB gain (fraction),      Q15 */
    Word16 sf0_exp_coeff[],    /* i  : energy coeff. (5), exponent part,  Q0  */
    Word16 sf0_frac_coeff[],   /* i  : energy coeff. (5), fraction part,  Q15 */
    /*      (frac_coeff and exp_coeff computed in  */
    /*       calc_filt_energies())                 */
    Word16 sf0_exp_target_en,  /* i  : exponent of target energy,         Q0  */
    Word16 sf0_frac_target_en, /* i  : fraction of target energy,         Q15 */

    /* data from subframe 1 (or 3) */
    Word16 sf1_code_nosharp[], /* i  : innovative codebook vector (L_SUBFR)   */
    /*      (whithout pitch sharpening)            */
    Word16 sf1_exp_gcode0,     /* i  : predicted CB gain (exponent),      Q0  */
    Word16 sf1_frac_gcode0,    /* i  : predicted CB gain (fraction),      Q15 */
    Word16 sf1_exp_coeff[],    /* i  : energy coeff. (5), exponent part,  Q0  */
    Word16 sf1_frac_coeff[],   /* i  : energy coeff. (5), fraction part,  Q15 */
    /*      (frac_coeff and exp_coeff computed in  */
    /*       calc_filt_energies())                 */
    Word16 sf1_exp_target_en,  /* i  : exponent of target energy,         Q0  */
    Word16 sf1_frac_target_en, /* i  : fraction of target energy,         Q15 */

    Word16 gp_limit,           /* i  : pitch gain limit                       */

    Word16 *sf0_gain_pit,      /* o  : Pitch gain,                        Q14 */
    Word16 *sf0_gain_cod,      /* o  : Code gain,                         Q1  */

    Word16 *sf1_gain_pit,      /* o  : Pitch gain,                        Q14 */
    Word16 *sf1_gain_cod,      /* o  : Code gain,                         Q1  */
    Flag   *pOverflow          /* o  : overflow indicator                     */
)
{
    const Word16 *p;
    Word16 i;
    Word16 index = 0;
    Word16 tmp;
    Word16 exp;
    Word16 sf0_gcode0;
    Word16 sf1_gcode0;
    Word16 g_pitch;
    Word16 g2_pitch;
    Word16 g_code;
    Word16 g2_code;
    Word16 g_pit_cod;
    Word16 coeff[10];
    Word16 coeff_lo[10];
    Word16 exp_max[10];  /* 0..4: sf0; 5..9: sf1 */
    Word32 L_tmp;
    Word32 dist_min;

    /*-------------------------------------------------------------------*
     *  predicted codebook gain                                          *
     *  ~~~~~~~~~~~~~~~~~~~~~~~                                          *
     *  gc0     = 2^exp_gcode0 + 2^frac_gcode0                           *
     *                                                                   *
     *  gcode0 (Q14) = 2^14*2^frac_gcode0 = gc0 * 2^(14-exp_gcode0)      *
     *-------------------------------------------------------------------*/

    sf0_gcode0 = (Word16)(Pow2(14, sf0_frac_gcode0, pOverflow));
    sf1_gcode0 = (Word16)(Pow2(14, sf1_frac_gcode0, pOverflow));

    /*
     * For each subframe, the error energy (sum) to be minimized consists
     * of five terms, t[0..4].
     *
     *                      t[0] =    gp^2  * <y1 y1>
     *                      t[1] = -2*gp    * <xn y1>
     *                      t[2] =    gc^2  * <y2 y2>
     *                      t[3] = -2*gc    * <xn y2>
     *                      t[4] =  2*gp*gc * <y1 y2>
     *
     */

    /* sf 0 */
    /* determine the scaling exponent for g_code: ec = ec0 - 11 */
    exp = sf0_exp_gcode0 - 11;

    /* calculate exp_max[i] = s[i]-1 */
    exp_max[0] = (sf0_exp_coeff[0] - 13);
    exp_max[1] = (sf0_exp_coeff[1] - 14);
    exp_max[2] = (sf0_exp_coeff[2] + (15 + (exp << 1)));
    exp_max[3] = (sf0_exp_coeff[3] + exp);
    exp_max[4] = (sf0_exp_coeff[4] + (1 + exp));

    /* sf 1 */
    /* determine the scaling exponent for g_code: ec = ec0 - 11 */
    exp = sf1_exp_gcode0 - 11;

    /* calculate exp_max[i] = s[i]-1 */
    exp_max[5] = (sf1_exp_coeff[0] - 13);
    exp_max[6] = (sf1_exp_coeff[1] - 14);
    exp_max[7] = (sf1_exp_coeff[2] + (15 + (exp << 1)));
    exp_max[8] = (sf1_exp_coeff[3] + exp);
    exp_max[9] = (sf1_exp_coeff[4] + (1 + exp));

    /*-------------------------------------------------------------------*
     *  Gain search equalisation:                                        *
     *  ~~~~~~~~~~~~~~~~~~~~~~~~~                                        *
     *  The MSE for the two subframes is weighted differently if there   *
     *  is a big difference in the corresponding target energies         *
     *-------------------------------------------------------------------*/

    /* make the target energy exponents the same by de-normalizing the
       fraction of the smaller one. This is necessary to be able to compare
       them
     */
    exp = sf0_exp_target_en - sf1_exp_target_en;
    if (exp > 0)
    {
        sf1_frac_target_en >>= exp;
    }
    else
    {
        sf0_frac_target_en >>= (-exp);
    }

    /* assume no change of exponents */
    exp = 0;

    /* test for target energy difference; set exp to +1 or -1 to scale
     * up/down coefficients for sf 1
     */
    tmp = shr_r(sf1_frac_target_en, 1, pOverflow);  /* tmp = ceil(0.5*en(sf1)) */

    if (tmp > sf0_frac_target_en)          /* tmp > en(sf0)? */
    {
        /*
         * target_energy(sf1) > 2*target_energy(sf0)
         *   -> scale up MSE(sf0) by 2 by adding 1 to exponents 0..4
         */
        exp = 1;
    }
    else
    {
        tmp = ((sf0_frac_target_en + 3) >> 2); /* tmp=ceil(0.25*en(sf0)) */

        if (tmp > sf1_frac_target_en)      /* tmp > en(sf1)? */
        {
            /*
             * target_energy(sf1) < 0.25*target_energy(sf0)
             *   -> scale down MSE(sf0) by 0.5 by subtracting 1 from
             *      coefficients 0..4
             */
            exp = -1;
        }
    }

    for (i = 0; i < 5; i++)
    {
        exp_max[i] += exp;
    }

    /*-------------------------------------------------------------------*
     *  Find maximum exponent:                                           *
     *  ~~~~~~~~~~~~~~~~~~~~~~                                           *
     *                                                                   *
     *  For the sum operation, all terms must have the same scaling;     *
     *  that scaling should be low enough to prevent overflow. There-    *
     *  fore, the maximum scale is determined and all coefficients are   *
     *  re-scaled:                                                       *
     *                                                                   *
     *    exp = max(exp_max[i]) + 1;                                     *
     *    e = exp_max[i]-exp;         e <= 0!                            *
     *    c[i] = c[i]*2^e                                                *
     *-------------------------------------------------------------------*/

    exp = exp_max[0];
    for (i = 9; i > 0; i--)
    {
        if (exp_max[i] > exp)
        {
            exp = exp_max[i];
        }
    }
    exp++;      /* To avoid overflow */

    p = &sf0_frac_coeff[0];
    for (i = 0; i < 5; i++)
    {
        tmp = (exp - exp_max[i]);
        L_tmp = ((Word32)(*p++) << 16);
        L_tmp = L_shr(L_tmp, tmp, pOverflow);
        coeff[i] = (Word16)(L_tmp >> 16);
        coeff_lo[i] = (Word16)((L_tmp >> 1) - ((L_tmp >> 16) << 15));
    }
    p = &sf1_frac_coeff[0];
    for (; i < 10; i++)
    {
        tmp = exp - exp_max[i];
        L_tmp = ((Word32)(*p++) << 16);
        L_tmp = L_shr(L_tmp, tmp, pOverflow);
        coeff[i] = (Word16)(L_tmp >> 16);
        coeff_lo[i] = (Word16)((L_tmp >> 1) - ((L_tmp >> 16) << 15));
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

    p = &table_gain_MR475[0];

    for (i = 0; i < MR475_VQ_SIZE; i++)
    {
        /* subframe 0 (and 2) calculations */
        g_pitch = *p++;
        g_code = *p++;

        /* Need to be there OKA */
        g_code    = (Word16)(((Word32) g_code * sf0_gcode0) >> 15);
        g2_pitch  = (Word16)(((Word32) g_pitch * g_pitch) >> 15);
        g2_code   = (Word16)(((Word32) g_code * g_code) >> 15);
        g_pit_cod = (Word16)(((Word32) g_code * g_pitch) >> 15);


        L_tmp = Mpy_32_16(coeff[0], coeff_lo[0], g2_pitch, pOverflow) +
                Mpy_32_16(coeff[1], coeff_lo[1], g_pitch, pOverflow) +
                Mpy_32_16(coeff[2], coeff_lo[2], g2_code, pOverflow) +
                Mpy_32_16(coeff[3], coeff_lo[3], g_code, pOverflow) +
                Mpy_32_16(coeff[4], coeff_lo[4], g_pit_cod, pOverflow);

        tmp = (g_pitch - gp_limit);

        /* subframe 1 (and 3) calculations */
        g_pitch = *p++;
        g_code = *p++;

        if ((tmp <= 0) && (g_pitch <= gp_limit))
        {
            g_code = (Word16)(((Word32) g_code * sf1_gcode0) >> 15);
            g2_pitch  = (Word16)(((Word32) g_pitch * g_pitch) >> 15);
            g2_code   = (Word16)(((Word32) g_code * g_code) >> 15);
            g_pit_cod = (Word16)(((Word32) g_code * g_pitch) >> 15);

            L_tmp += (Mpy_32_16(coeff[5], coeff_lo[5], g2_pitch, pOverflow) +
                      Mpy_32_16(coeff[6], coeff_lo[6], g_pitch, pOverflow) +
                      Mpy_32_16(coeff[7], coeff_lo[7], g2_code, pOverflow) +
                      Mpy_32_16(coeff[8], coeff_lo[8], g_code, pOverflow) +
                      Mpy_32_16(coeff[9], coeff_lo[9], g_pit_cod, pOverflow));

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
     *  read quantized gains and update MA predictor memories           *
     *  ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~           *
     *------------------------------------------------------------------*/

    /* for subframe 0, the pre-calculated gcode0/exp_gcode0 are the same
       as those calculated from the "real" predictor using quantized gains */
    tmp = index << 2;
    MR475_quant_store_results(pred_st,
                              &table_gain_MR475[tmp],
                              sf0_gcode0,
                              sf0_exp_gcode0,
                              sf0_gain_pit,
                              sf0_gain_cod,
                              pOverflow);

    /* calculate new predicted gain for subframe 1 (this time using
       the real, quantized gains)                                   */
    gc_pred(pred_st, MR475, sf1_code_nosharp,
            &sf1_exp_gcode0, &sf1_frac_gcode0,
            &sf0_exp_gcode0, &sf0_gcode0, /* dummy args */
            pOverflow);

    sf1_gcode0 = (Word16)(Pow2(14, sf1_frac_gcode0, pOverflow));

    tmp += 2;
    MR475_quant_store_results(
        pred_st,
        &table_gain_MR475[tmp],
        sf1_gcode0,
        sf1_exp_gcode0,
        sf1_gain_pit,
        sf1_gain_cod,
        pOverflow);

    return(index);
}
