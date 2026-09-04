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



 Filename: phase_dispersion.cpp

------------------------------------------------------------------------------
 INPUT AND OUTPUT DEFINITIONS

     int16 gain_code,               (i) Q0  : gain of code
     int16 gain_pit,                (i) Q14 : gain of pitch
     int16 code[],                  (i/o)   : code vector
     int16 mode,                    (i)     : level, 0=hi, 1=lo, 2=off
     int16 disp_mem[],              (i/o)   : static memory (size = 8)
     int16 ScratchMem[]

------------------------------------------------------------------------------
 FUNCTION DESCRIPTION

    post-processing to enhance noise in low bit rate.

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
#include "pvamrwbdecoder_mem_funcs.h"
#include "pvamrwbdecoder_acelp.h"
#include "pvamrwb_math_op.h"


/*----------------------------------------------------------------------------
; MACROS
; Define module specific macros here
----------------------------------------------------------------------------*/


/*----------------------------------------------------------------------------
; DEFINES
; Include all pre-processor statements here. Include conditional
; compile variables also.
----------------------------------------------------------------------------*/
#define pitch_0_9  14746                   /* 0.9 in Q14 */
#define pitch_0_6  9830                    /* 0.6 in Q14 */
#define L_SUBFR 64

/*----------------------------------------------------------------------------
; LOCAL FUNCTION DEFINITIONS
; Function Prototype declaration
----------------------------------------------------------------------------*/

/*----------------------------------------------------------------------------
; LOCAL STORE/BUFFER/POINTER DEFINITIONS
; Variable declaration - defined here and used outside this module
----------------------------------------------------------------------------*/
/* impulse response with phase dispersion */

/* 2.0 - 6.4 kHz phase dispersion */
static const int16 ph_imp_low[L_SUBFR] =
{
    20182,  9693,  3270, -3437, 2864, -5240,  1589, -1357,
    600,  3893, -1497,  -698, 1203, -5249,  1199,  5371,
    -1488,  -705, -2887,  1976,  898,   721, -3876,  4227,
    -5112,  6400, -1032, -4725, 4093, -4352,  3205,  2130,
    -1996, -1835,  2648, -1786, -406,   573,  2484, -3608,
    3139, -1363, -2566,  3808, -639, -2051,  -541,  2376,
    3932, -6262,  1432, -3601, 4889,   370,   567, -1163,
    -2854,  1914,    39, -2418, 3454,  2975, -4021,  3431
};

/* 3.2 - 6.4 kHz phase dispersion */
static const int16 ph_imp_mid[L_SUBFR] =
{
    24098, 10460, -5263,  -763,  2048,  -927,  1753, -3323,
    2212,   652, -2146,  2487, -3539,  4109, -2107,  -374,
    -626,  4270, -5485,  2235,  1858, -2769,   744,  1140,
    -763, -1615,  4060, -4574,  2982, -1163,   731, -1098,
    803,   167,  -714,   606,  -560,   639,    43, -1766,
    3228, -2782,   665,   763,   233, -2002,  1291,  1871,
    -3470,  1032,  2710, -4040,  3624, -4214,  5292, -4270,
    1563,   108,  -580,  1642, -2458,   957,  544,   2540
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

void phase_dispersion(
    int16 gain_code,             /* (i) Q0  : gain of code             */
    int16 gain_pit,              /* (i) Q14 : gain of pitch            */
    int16 code[],                /* (i/o)   : code vector              */
    int16 mode,                  /* (i)     : level, 0=hi, 1=lo, 2=off */
    int16 disp_mem[],            /* (i/o)   : static memory (size = 8) */
    int16 ScratchMem[]
)
{
    int16 i, j, state;
    int16 *prev_gain_pit, *prev_gain_code, *prev_state;
    int16 *code2 = ScratchMem;

    prev_state = disp_mem;
    prev_gain_code = disp_mem + 1;
    prev_gain_pit = disp_mem + 2;

    pv_memset((void *)code2, 0, (2*L_SUBFR)*sizeof(*code2));


    if (gain_pit < pitch_0_6)
    {
        state = 0;
    }
    else if (gain_pit < pitch_0_9)
    {
        state = 1;
    }
    else
    {
        state = 2;
    }

    for (i = 5; i > 0; i--)
    {
        prev_gain_pit[i] = prev_gain_pit[i - 1];
    }
    prev_gain_pit[0] = gain_pit;

    if (sub_int16(gain_code, *prev_gain_code) > shl_int16(*prev_gain_code, 1))
    {
        /* onset */
        if (state < 2)
        {
            state++;
        }
    }
    else
    {
        j = 0;
        for (i = 0; i < 6; i++)
        {
            if (prev_gain_pit[i] < pitch_0_6)
            {
                j++;
            }
        }

        if (j > 2)
        {
            state = 0;
        }
        if (state > *prev_state + 1)
        {
            state--;
        }
    }

    *prev_gain_code = gain_code;
    *prev_state = state;

    /* circular convolution */

    state += mode;              /* level of dispersion */

    if (state == 0)
    {
        for (i = 0; i < L_SUBFR; i++)
        {
            if (code[i] != 0)
            {
                for (j = 0; j < L_SUBFR; j++)
                {
                    code2[i + j] = add_int16(code2[i + j], mult_int16_r(code[i], ph_imp_low[j]));
                }
            }
        }
    }
    else if (state == 1)
    {
        for (i = 0; i < L_SUBFR; i++)
        {
            if (code[i] != 0)
            {
                for (j = 0; j < L_SUBFR; j++)
                {
                    code2[i + j] = add_int16(code2[i + j], mult_int16_r(code[i], ph_imp_mid[j]));
                }
            }
        }
    }
    if (state < 2)
    {
        for (i = 0; i < L_SUBFR; i++)
        {
            code[i] = add_int16(code2[i], code2[i + L_SUBFR]);
        }
    }
    return;
}

