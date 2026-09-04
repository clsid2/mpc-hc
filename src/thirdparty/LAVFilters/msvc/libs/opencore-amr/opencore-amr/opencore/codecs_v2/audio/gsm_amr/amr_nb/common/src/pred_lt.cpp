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
 Filename: pred_lt.cpp

------------------------------------------------------------------------------
*/

/*----------------------------------------------------------------------------
; INCLUDES
----------------------------------------------------------------------------*/
#include "pred_lt.h"
#include "cnst.h"

/*----------------------------------------------------------------------------
; MACROS
; Define module specific macros here
----------------------------------------------------------------------------*/

/*----------------------------------------------------------------------------
; DEFINES
; Include all pre-processor statements here. Include conditional
; compile variables also.
----------------------------------------------------------------------------*/
#define UP_SAMP_MAX  6
#define L_INTER10    (L_INTERPOL-1)
#define FIR_SIZE     (UP_SAMP_MAX*L_INTER10+1)

/*----------------------------------------------------------------------------
; LOCAL FUNCTION DEFINITIONS
; Function Prototype declaration
----------------------------------------------------------------------------*/

/*----------------------------------------------------------------------------
; LOCAL STORE/BUFFER/POINTER DEFINITIONS
; Variable declaration - defined here and used outside this module
----------------------------------------------------------------------------*/

/* 1/6 resolution interpolation filter  (-3 dB at 3600 Hz) */
/* Note: the 1/3 resolution filter is simply a subsampled
 *       version of the 1/6 resolution filter, i.e. it uses
 *       every second coefficient:
 *
 *          inter_3l[k] = inter_6[2*k], 0 <= k <= 3*L_INTER10
 */

const Word16 inter_6_pred_lt[FIR_SIZE] =
{
    29443,
    28346, 25207, 20449, 14701,  8693,  3143,
    -1352, -4402, -5865, -5850, -4673, -2783,
    -672,  1211,  2536,  3130,  2991,  2259,
    1170,     0, -1001, -1652, -1868, -1666,
    -1147,  -464,   218,   756,  1060,  1099,
    904,   550,   135,  -245,  -514,  -634,
    -602,  -451,  -231,     0,   191,   308,
    340,   296,   198,    78,   -36,  -120,
    -163,  -165,  -132,   -79,   -19,    34,
    73,    91,    89,    70,    38,     0
};


/*
------------------------------------------------------------------------------
 FUNCTION NAME: Pred_lt_3or6
------------------------------------------------------------------------------
 INPUT AND OUTPUT DEFINITIONS

 Inputs:
    exc = buffer containing the excitation (Word16)
    T0 = integer pitch lag (Word16)
    frac = fraction of lag (Word16)
    L_subfr = number of samples per subframe (Word16)
    flag3 = flag to indicate the upsampling rate; if set, upsampling
            rate is 3, otherwise, upsampling rate is 6 (Word16)

    pOverflow = pointer to overflow (Flag)

 Returns:
    None

 Outputs:
    exc buffer contains the newly formed adaptive codebook excitation
    pOverflow -> 1 if the add operation resulted in overflow

 Global Variables Used:
    inter_6_pred_lt = (1/6) resolution interpolation filter table (Word16)

 Local Variables Needed:
    None

------------------------------------------------------------------------------
 FUNCTION DESCRIPTION

 This function computes the result of long term prediction with fractional
 interpolation of resolution 1/3 or 1/6. (Interpolated past excitation).

 The past excitation signal at the given delay is interpolated at
 the given fraction to build the adaptive codebook excitation.
 On return exc[0..L_subfr-1] contains the interpolated signal
 (adaptive codebook excitation).

------------------------------------------------------------------------------
 REQUIREMENTS

 None

------------------------------------------------------------------------------
 REFERENCES

 pred_lt.c, UMTS GSM AMR speech codec, R99 - Version 3.2.0, March 2, 2001

------------------------------------------------------------------------------
 PSEUDO-CODE

void Pred_lt_3or6 (
    Word16 exc[],     // in/out: excitation buffer
    Word16 T0,        // input : integer pitch lag
    Word16 frac,      // input : fraction of lag
    Word16 L_subfr,   // input : subframe size
    Word16 flag3      // input : if set, upsampling rate = 3 (6 otherwise)
)
{
    Word16 i, j, k;
    Word16 *pX0, *pX1, *pX2;
    const Word16 *pC1, *pC2;
    Word32 s;

    pX0 = &exc[-T0];

    frac = negate (frac);
    if (flag3 != 0)
    {
      frac = shl (frac, 1);   // inter_3l[k] = inter_6[2*k] -> k' = 2*k
    }

    if (frac < 0)
    {
        frac = add (frac, UP_SAMP_MAX);
        pX0--;
    }

    for (j = 0; j < L_subfr; j++)
    {
        pX1 = pX0++;
        pX2 = pX0;
        pC1 = &inter_6[frac];
        pC2 = &inter_6[sub (UP_SAMP_MAX, frac)];

        s = 0;
        for (i = 0, k = 0; i < L_INTER10; i++, k += UP_SAMP_MAX)
        {
            s = L_mac (s, pX1[-i], pC1[k]);
            s = L_mac (s, pX2[i], pC2[k]);
        }

        exc[j] = pv_round (s);
    }

    return;
}

------------------------------------------------------------------------------
 CAUTION [optional]
 [State any special notes, constraints or cautions for users of this function]

------------------------------------------------------------------------------
*/

OSCL_EXPORT_REF void Pred_lt_3or6(
    Word16 exc[],     /* in/out: excitation buffer                          */
    Word16 T0,        /* input : integer pitch lag                          */
    Word16 frac,      /* input : fraction of lag                            */
    Word16 L_subfr,   /* input : subframe size                              */
    Word16 flag3,     /* input : if set, upsampling rate = 3 (6 otherwise)  */
    Flag  *pOverflow  /* output: if set, overflow occurred in this function */
)
{
    register Word16 i;
    register Word16 j;
    register Word16 k;

    Word16 *pX0;
    Word16 *pX2;
    Word16 *pX3;
    Word16 *p_exc;
    Word16 *pC1;
    const Word16 *pC1_ref;
    const Word16 *pC2_ref;

    Word16 Coeff_1[(L_INTER10<<1)];

    Word32 s1;
    Word32 s2;
    OSCL_UNUSED_ARG(pOverflow);

    pX0 = &(exc[-T0]);

    /* frac goes between -3 and 3 */

    frac = -frac;

    if (flag3 != 0)
    {
        frac <<= 1;   /* inter_3l[k] = inter_6[2*k] -> k' = 2*k */
    }

    if (frac < 0)
    {
        frac += UP_SAMP_MAX;
        pX0--;
    }

    pC1_ref = &inter_6_pred_lt[frac];
    pC2_ref = &inter_6_pred_lt[UP_SAMP_MAX-frac];


    pC1 = Coeff_1;

    k = 0;

    for (i = L_INTER10 >> 1; i > 0; i--)
    {
        *(pC1++) = pC1_ref[k];
        *(pC1++) = pC2_ref[k];
        k += UP_SAMP_MAX;
        *(pC1++) = pC1_ref[k];
        *(pC1++) = pC2_ref[k];
        k += UP_SAMP_MAX;

    }

    p_exc = exc;

    for (j = (L_subfr >> 1); j != 0 ; j--)
    {
        pX0++;
        pX2 = pX0;
        pX3 = pX0++;

        pC1 = Coeff_1;

        s1  = 0x00004000L;
        s2  = 0x00004000L;

        for (i = L_INTER10 >> 1; i > 0; i--)
        {
            s2 += ((Word32) * (pX3--)) * *(pC1);
            s1 += ((Word32) * (pX3)) * *(pC1++);
            s1 += ((Word32) * (pX2++)) * *(pC1);
            s2 += ((Word32) * (pX2)) * *(pC1++);
            s2 += ((Word32) * (pX3--)) * *(pC1);
            s1 += ((Word32) * (pX3)) * *(pC1++);
            s1 += ((Word32) * (pX2++)) * *(pC1);
            s2 += ((Word32) * (pX2)) * *(pC1++);

        } /* for (i = L_INTER10>>1; i > 0; i--) */

        *(p_exc++) = (Word16)(s1 >> 15);
        *(p_exc++) = (Word16)(s2 >> 15);

    } /* for (j = (L_subfr>>1); j != 0 ; j--) */

    return;
}
