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

 Filename: ph_disp_tab.cpp

------------------------------------------------------------------------------
 MODULE DESCRIPTION

 This file contains the table of impulse responses of the phase dispersion
 filters. All impulse responses are in Q15

------------------------------------------------------------------------------
*/

/*----------------------------------------------------------------------------
; INCLUDES
----------------------------------------------------------------------------*/
#include    "typedef.h"

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

    /*----------------------------------------------------------------------------
    ; LOCAL FUNCTION DEFINITIONS
    ; [List function prototypes here]
    ----------------------------------------------------------------------------*/

    /*----------------------------------------------------------------------------
    ; LOCAL VARIABLE DEFINITIONS
    ; [Variable declaration - defined here and used outside this module]
    ----------------------------------------------------------------------------*/
    extern const Word16 ph_imp_low_MR795[];
    const Word16 ph_imp_low_MR795[40] =
    {
        26777,    801,   2505,   -683,  -1382,    582,    604,  -1274,   3511,  -5894,
        4534,   -499,  -1940,   3011,  -5058,   5614,  -1990,  -1061,  -1459,   4442,
        -700,  -5335,   4609,    452,   -589,  -3352,   2953,   1267,  -1212,  -2590,
        1731,   3670,  -4475,   -975,   4391,  -2537,    949,  -1363,   -979,   5734
    };
    extern const Word16 ph_imp_mid_MR795[];
    const Word16 ph_imp_mid_MR795[40] =
    {
        30274,   3831,  -4036,   2972,  -1048,  -1002,   2477,  -3043,   2815,  -2231,
        1753,  -1611,   1714,  -1775,   1543,  -1008,    429,   -169,    472,  -1264,
        2176,  -2706,   2523,  -1621,    344,    826,  -1529,   1724,  -1657,   1701,
        -2063,   2644,  -3060,   2897,  -1978,    557,    780,  -1369,    842,    655
    };

    extern const Word16 ph_imp_low[];
    const Word16 ph_imp_low[40] =
    {
        14690,  11518,   1268,  -2761,  -5671,   7514,    -35,  -2807,  -3040,   4823,
        2952,  -8424,   3785,   1455,   2179,  -8637,   8051,  -2103,  -1454,    777,
        1108,  -2385,   2254,   -363,   -674,  -2103,   6046,  -5681,   1072,   3123,
        -5058,   5312,  -2329,  -3728,   6924,  -3889,    675,  -1775,     29,  10145
    };
    extern const Word16 ph_imp_mid[];
    const Word16 ph_imp_mid[40] =
    {
        30274,   3831,  -4036,   2972,  -1048,  -1002,   2477,  -3043,   2815,  -2231,
        1753,  -1611,   1714,  -1775,   1543,  -1008,    429,   -169,    472,  -1264,
        2176,  -2706,   2523,  -1621,    344,    826,  -1529,   1724,  -1657,   1701,
        -2063,   2644,  -3060,   2897,  -1978,    557,    780,  -1369,    842,    655
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

 [1] ph_disp.tab, UMTS GSM AMR speech codec, R99 - Version 3.2.0, March 2, 2001

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




