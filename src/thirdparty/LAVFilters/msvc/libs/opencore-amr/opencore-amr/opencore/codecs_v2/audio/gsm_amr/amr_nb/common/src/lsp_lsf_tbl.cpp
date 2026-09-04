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
 Filename: lsp_lsf_tbl.cpp

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

    /*----------------------------------------------------------------------------
    ; LOCAL FUNCTION DEFINITIONS
    ; [List function prototypes here]
    ----------------------------------------------------------------------------*/

    /*----------------------------------------------------------------------------
    ; LOCAL VARIABLE DEFINITIONS
    ; [Variable declaration - defined here and used outside this module]
    ----------------------------------------------------------------------------*/

    extern const Word16 table[];
    const Word16 table[65] =
    {
        32767, 32729, 32610, 32413, 32138, 31786, 31357, 30853,
        30274, 29622, 28899, 28106, 27246, 26320, 25330, 24279,
        23170, 22006, 20788, 19520, 18205, 16846, 15447, 14010,
        12540, 11039, 9512, 7962, 6393, 4808, 3212, 1608,
        0, -1608, -3212, -4808, -6393, -7962, -9512, -11039,
        -12540, -14010, -15447, -16846, -18205, -19520, -20788, -22006,
        -23170, -24279, -25330, -26320, -27246, -28106, -28899, -29622,
        -30274, -30853, -31357, -31786, -32138, -32413, -32610, -32729,
        (Word16) 0x8000
    };

    /* 0x8000 = -32768 (used to silence the compiler) */

    /* slope used to compute y = acos(x) */

    extern const Word16 slope[];
    const Word16 slope[64] =
    {
        -26887, -8812, -5323, -3813, -2979, -2444, -2081, -1811,
        -1608, -1450, -1322, -1219, -1132, -1059, -998, -946,
        -901, -861, -827, -797, -772, -750, -730, -713,
        -699, -687, -677, -668, -662, -657, -654, -652,
        -652, -654, -657, -662, -668, -677, -687, -699,
        -713, -730, -750, -772, -797, -827, -861, -901,
        -946, -998, -1059, -1132, -1219, -1322, -1450, -1608,
        -1811, -2081, -2444, -2979, -3813, -5323, -8812, -26887
    };

    /*--------------------------------------------------------------------------*/
#ifdef __cplusplus
}
#endif

