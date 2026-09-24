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

 Filename: gains_tbl.cpp

------------------------------------------------------------------------------
 MODULE DESCRIPTION

------------------------------------------------------------------------------
*/

/*----------------------------------------------------------------------------
; INCLUDES
----------------------------------------------------------------------------*/
#include "typedef.h"

/*--------------------------------------------------------------------------*/
#ifdef __cplusplus
extern "C"
{
#endif

    /*----------------------------------------------------------------------------
    ; MACROS
    ; [Define module specific macros here]
    ----------------------------------------------------------------------------*/

    /*----------------------------------------------------------------------------
    ; DEFINES
    ; [Include all pre-processor statements here. Include conditional
    ; compile variables also.]
    ----------------------------------------------------------------------------*/
#define NB_QUA_PITCH 16
#define NB_QUA_CODE 32

    /*----------------------------------------------------------------------------
    ; LOCAL FUNCTION DEFINITIONS
    ; [List function prototypes here]
    ----------------------------------------------------------------------------*/

    /*----------------------------------------------------------------------------
    ; LOCAL VARIABLE DEFINITIONS
    ; [Variable declaration - defined here and used outside this module]
    ----------------------------------------------------------------------------*/


    extern const Word16 qua_gain_pitch[];
    const Word16 qua_gain_pitch[NB_QUA_PITCH] =
    {
        0, 3277, 6556, 8192, 9830, 11469, 12288, 13107,
        13926, 14746, 15565, 16384, 17203, 18022, 18842, 19661
    };


    extern const Word16 qua_gain_code[];
    const Word16 qua_gain_code[(NB_QUA_CODE+1)*3] =
    {
        /* gain factor (g_fac) and quantized energy error (qua_ener_MR122, qua_ener)
         * are stored:
         *
         * qua_ener_MR122 = log2(g_fac)      (not the rounded floating point value, but
         *                                    the value the original EFR algorithm
         *                                    calculates from g_fac [using Log2])
         * qua_ener       = 20*log10(g_fac); (rounded floating point value)
         *
         *
         * g_fac (Q11), qua_ener_MR122 (Q10), qua_ener (Q10)
         */
        159,                -3776,          -22731,
        206,                -3394,          -20428,
        268,                -3005,          -18088,
        349,                -2615,          -15739,
        419,                -2345,          -14113,
        482,                -2138,          -12867,
        554,                -1932,          -11629,
        637,                -1726,          -10387,
        733,                -1518,           -9139,
        842,                -1314,           -7906,
        969,                -1106,           -6656,
        1114,                 -900,           -5416,
        1281,                 -694,           -4173,
        1473,                 -487,           -2931,
        1694,                 -281,           -1688,
        1948,                  -75,            -445,
        2241,                  133,             801,
        2577,                  339,            2044,
        2963,                  545,            3285,
        3408,                  752,            4530,
        3919,                  958,            5772,
        4507,                 1165,            7016,
        5183,                 1371,            8259,
        5960,                 1577,            9501,
        6855,                 1784,           10745,
        7883,                 1991,           11988,
        9065,                 2197,           13231,
        10425,                 2404,           14474,
        12510,                 2673,           16096,
        16263,                 3060,           18429,
        21142,                 3448,           20763,
        27485,                 3836,           23097,
        27485,                 3836,           23097
    };

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
    None

 Outputs:
    None

 Returns:
    None

 Global Variables Used:
    None

 Local Variables Needed:
    None

------------------------------------------------------------------------------
 FUNCTION DESCRIPTION

 None

------------------------------------------------------------------------------
 REQUIREMENTS

 None

------------------------------------------------------------------------------
 REFERENCES

 [1] gains.tab,  UMTS GSM AMR speech codec, R99 - Version 3.2.0, March 2, 2001

------------------------------------------------------------------------------
 PSEUDO-CODE


------------------------------------------------------------------------------
 CAUTION [optional]
 [State any special notes, constraints or cautions for users of this function]

------------------------------------------------------------------------------
*/

/*----------------------------------------------------------------------------
; FUNCTION CODE
----------------------------------------------------------------------------*/



