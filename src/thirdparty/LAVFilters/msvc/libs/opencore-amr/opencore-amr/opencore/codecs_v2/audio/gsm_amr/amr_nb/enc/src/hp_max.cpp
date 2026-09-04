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



 Filename: hp_max.cpp

------------------------------------------------------------------------------
*/

/*----------------------------------------------------------------------------
; INCLUDES
----------------------------------------------------------------------------*/
#include    "hp_max.h"
#include    "basic_op.h"
#include    "cnst.h"
#include    "l_abs.h"

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

/*
------------------------------------------------------------------------------
 FUNCTION NAME: hp_max
------------------------------------------------------------------------------
 INPUT AND OUTPUT DEFINITIONS

 Inputs:
    corr[] = correlation vector (Word16)
    scal_sig[] = scaled signal vector (Word16)
    L_frame = length of frame to compute pitch (Word16
    lag_max = maximum lag (Word16)
    lag_min = minimum lag (Word16)
    cor_hp_max = pointer to max high-pass filtered norm. correlation (Word16)
    pOverflow = pointer to overflow (Flag)

 Outputs:
    cor_hp_max contains max high-pass filtered norm. correlation (Word16)
    pOverflow -> 1 if the maximum correlation computation resulted in overflow

 Returns:
    0 (Word16)

 Global Variables Used:
    None

 Local Variables Needed:
    None

------------------------------------------------------------------------------
 FUNCTION DESCRIPTION

 This function finds the maximum high-pass filtered correlation of scal_sig[]
 in a given delay range.

 The correlation is given by
    corr[t] = <scal_sig[n],scal_sig[n-t]>,  t=lag_min,...,lag_max
 The functions outputs the maximum high-pass filtered correlation after
 normalization.

------------------------------------------------------------------------------
 REQUIREMENTS

 None

------------------------------------------------------------------------------
 REFERENCES

 [1] hp_max.c, UMTS GSM AMR speech codec, R99 - Version 3.2.0, March 2, 2001

------------------------------------------------------------------------------
 PSEUDO-CODE

Word16 hp_max (
    Word32 corr[],      // i   : correlation vector
    Word16 scal_sig[],  // i   : scaled signal
    Word16 L_frame,     // i   : length of frame to compute pitch
    Word16 lag_max,     // i   : maximum lag
    Word16 lag_min,     // i   : minimum lag
    Word16 *cor_hp_max) // o   : max high-pass filtered norm. correlation
{
    Word16 i;
    Word16 *p, *p1;
    Word32 max, t0, t1;
    Word16 max16, t016, cor_max;
    Word16 shift, shift1, shift2;

    max = MIN_32;
    t0 = 0L;
* The reference ETSI code uses a global flag for Overflow inside the math functions
* saturate(). In the actual implementation a pointer to Overflow flag is passed in
* as a parameter to the function

    for (i = lag_max-1; i > lag_min; i--)
    {
       // high-pass filtering
       t0 = L_sub (L_sub(L_shl(corr[-i], 1), corr[-i-1]), corr[-i+1]);
       t0 = L_abs (t0);

       if (L_sub (t0, max) >= 0)
       {
          max = t0;
       }
    }

    // compute energy
    p = scal_sig;
    p1 = &scal_sig[0];
    t0 = 0L;
    for (i = 0; i < L_frame; i++, p++, p1++)
    {
       t0 = L_mac (t0, *p, *p1);
    }

    p = scal_sig;
    p1 = &scal_sig[-1];
    t1 = 0L;
    for (i = 0; i < L_frame; i++, p++, p1++)
    {
       t1 = L_mac (t1, *p, *p1);
    }

    // high-pass filtering
    t0 = L_sub(L_shl(t0, 1), L_shl(t1, 1));
    t0 = L_abs (t0);

    // max/t0
    shift1 = sub(norm_l(max), 1);
    max16  = extract_h(L_shl(max, shift1));
    shift2 = norm_l(t0);
    t016 =  extract_h(L_shl(t0, shift2));

    if (t016 != 0)
    {
       cor_max = div_s(max16, t016);
    }
    else
    {
       cor_max = 0;
    }

    shift = sub(shift1, shift2);

    if (shift >= 0)
    {
       *cor_hp_max = shr(cor_max, shift); // Q15
    }
    else
    {
       *cor_hp_max = shl(cor_max, negate(shift)); // Q15
    }

    return 0;
}


------------------------------------------------------------------------------
 CAUTION [optional]
 [State any special notes, constraints or cautions for users of this function]

------------------------------------------------------------------------------
*/

/*----------------------------------------------------------------------------
; FUNCTION CODE
----------------------------------------------------------------------------*/
Word16 hp_max(
    Word32 corr[],      /* i   : correlation vector.                      */
    Word16 scal_sig[],  /* i   : scaled signal.                           */
    Word16 L_frame,     /* i   : length of frame to compute pitch         */
    Word16 lag_max,     /* i   : maximum lag                              */
    Word16 lag_min,     /* i   : minimum lag                              */
    Word16 *cor_hp_max, /* o   : max high-pass filtered norm. correlation */
    Flag   *pOverflow   /* i/o : overflow Flag                            */
)
{
    Word16 i;
    Word16 *p, *p1;
    Word32 max, t0, t1;
    Word16 max16, t016, cor_max;
    Word16 shift, shift1, shift2;
    Word32 L_temp;

    max = MIN_32;
    t0 = 0L;

    for (i = lag_max - 1; i > lag_min; i--)
    {
        /* high-pass filtering */
        t0 = L_shl(corr[-i], 1, pOverflow);
        L_temp = L_sub(t0, corr[-i-1], pOverflow);
        t0 = L_sub(L_temp, corr[-i+1], pOverflow);
        t0 = L_abs(t0);

        if (t0 >= max)
        {
            max = t0;
        }
    }

    /* compute energy */
    p = scal_sig;
    p1 = &scal_sig[0];
    t0 = 0L;
    for (i = 0; i < L_frame; i++, p++, p1++)
    {
        t0 = L_mac(t0, *p, *p1, pOverflow);
    }

    p = scal_sig;
    p1 = &scal_sig[-1];
    t1 = 0L;
    for (i = 0; i < L_frame; i++, p++, p1++)
    {
        t1 = L_mac(t1, *p, *p1, pOverflow);
    }

    /* high-pass filtering */
    L_temp = L_shl(t0, 1, pOverflow);
    t1 = L_shl(t1, 1, pOverflow);
    t0 = L_sub(L_temp, t1, pOverflow);
    t0 = L_abs(t0);

    /* max/t0 */
    /*  shift1 = sub(norm_l(max), 1);
        max16  = extract_h(L_shl(max, shift1));
        shift2 = norm_l(t0);
        t016 =  extract_h(L_shl(t0, shift2));   */

    t016 = norm_l(max);
    shift1 = t016 - 1;

    L_temp = L_shl(max, shift1, pOverflow);
    max16  = (Word16)(L_temp >> 16);

    shift2 = norm_l(t0);
    L_temp = L_shl(t0, shift2, pOverflow);
    t016 = (Word16)(L_temp >> 16);

    if (t016 != 0)
    {
        cor_max = div_s(max16, t016);
    }
    else
    {
        cor_max = 0;
    }

    shift = shift1 - shift2;

    if (shift >= 0)
    {
        *cor_hp_max = shr(cor_max, shift, pOverflow); /* Q15 */
    }
    else
    {
        *cor_hp_max = shl(cor_max, negate(shift), pOverflow); /* Q15 */
    }

    return 0;
}

