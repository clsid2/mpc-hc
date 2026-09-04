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



 Filename: isp_isf.cpp

------------------------------------------------------------------------------
 INPUT AND OUTPUT DEFINITIONS

     int16 isf[],       (i) Q15 : isf[m] normalized (range: 0.0<=val<=0.5)
     int16 isp[],       (o) Q15 : isp[m] (range: -1<=val<1)
     int16 m            (i)     : LPC order


------------------------------------------------------------------------------
 FUNCTION DESCRIPTION

     Isf_isp   Transformation isf to isp

   The transformation from isf[i] to isp[i] is
   approximated by a look-up table and interpolation.



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
; LOCAL STORE/BUFFER/POINTER DEFINITIONS
; Variable declaration - defined here and used outside this module
----------------------------------------------------------------------------*/

/* Look-up table for transformations */

/* table of cos(x) in Q15 */

static const int16 table[129] =
{
    32767,
    32758,  32729,  32679,  32610,  32522,  32413,  32286,  32138,
    31972,  31786,  31581,  31357,  31114,  30853,  30572,  30274,
    29957,  29622,  29269,  28899,  28511,  28106,  27684,  27246,
    26791,  26320,  25833,  25330,  24812,  24279,  23732,  23170,
    22595,  22006,  21403,  20788,  20160,  19520,  18868,  18205,
    17531,  16846,  16151,  15447,  14733,  14010,  13279,  12540,
    11793,  11039,  10279,   9512,   8740,   7962,   7180,   6393,
    5602,   4808,   4011,   3212,   2411,   1608,    804,      0,
    -804,  -1608,  -2411,  -3212,  -4011,  -4808,  -5602,  -6393,
    -7180,  -7962,  -8740,  -9512, -10279, -11039, -11793, -12540,
    -13279, -14010, -14733, -15447, -16151, -16846, -17531, -18205,
    -18868, -19520, -20160, -20788, -21403, -22006, -22595, -23170,
    -23732, -24279, -24812, -25330, -25833, -26320, -26791, -27246,
    -27684, -28106, -28511, -28899, -29269, -29622, -29957, -30274,
    -30572, -30853, -31114, -31357, -31581, -31786, -31972, -32138,
    -32286, -32413, -32522, -32610, -32679, -32729, -32758, -32768
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


void Isf_isp(
    int16 isf[],     /* (i) Q15 : isf[m] normalized (range: 0.0<=val<=0.5) */
    int16 isp[],     /* (o) Q15 : isp[m] (range: -1<=val<1)                */
    int16 m          /* (i)     : LPC order                                */
)
{
    int16 i, ind, offset;
    int32 L_tmp;

    for (i = 0; i < m - 1; i++)
    {
        isp[i] = isf[i];
    }
    isp[m - 1] = shl_int16(isf[m - 1], 1);

    for (i = 0; i < m; i++)
    {
        ind = isp[i] >> 7;            /* ind    = b7-b15 of isf[i] */
        offset = (isp[i] & 0x007f);       /* offset = b0-b6  of isf[i] */

        /* isp[i] = table[ind]+ ((table[ind+1]-table[ind])*offset) / 128 */

        L_tmp = mul_16by16_to_int32(table[ind + 1] - table[ind], offset);
        isp[i] = add_int16(table[ind], (int16)(L_tmp >> 8));
    }

    return;
}
