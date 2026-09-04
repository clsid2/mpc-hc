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



 Filename: g_pitch.cpp

------------------------------------------------------------------------------
*/

/*----------------------------------------------------------------------------
; INCLUDES
----------------------------------------------------------------------------*/
#include "g_pitch.h"
#include "mode.h"
#include "cnst.h"
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
; LOCAL STORE/BUFFER/POINTER DEFINITIONS
; Variable declaration - defined here and used outside this module
----------------------------------------------------------------------------*/


/*
------------------------------------------------------------------------------
 FUNCTION NAME: G_pitch
------------------------------------------------------------------------------
 INPUT AND OUTPUT DEFINITIONS

 Inputs:
    mode = AMR mode (enum Mode)
    xn = pointer to pitch target buffer (Word16)
    y1 = pointer to filtered adaptive codebook buffer (Word16)
    g_coeff  = pointer to buffer of correlations needed for gain quantization
               (Word16)
    L_subfr = length of subframe (Word16)
    pOverflow = pointer to overflow flag (Flag)

 Outputs:
    g_coeff contains the mantissa and exponent of the two dot products.
    pOverflow -> 1 if an overflow occurs

 Returns:
    gain =  ratio of dot products.(Word16)

 Global Variables Used:
    None.

 Local Variables Needed:
    None.

------------------------------------------------------------------------------
 FUNCTION DESCRIPTION

 This function computes the pitch (adaptive codebook) gain. The adaptive
 codebook gain is given by

    g = <x[], y[]> / <y[], y[]>

    where:  x[] is the target vector
            y[] is the filtered adaptive codevector
            <> denotes dot product.

 The gain is limited to the range [0,1.2] (=0..19661 Q14)

------------------------------------------------------------------------------
 REQUIREMENTS

 None.

------------------------------------------------------------------------------
 REFERENCES

 g_pitch.c, UMTS GSM AMR speech codec, R99 - Version 3.2.0, March 2, 2001

------------------------------------------------------------------------------
 PSEUDO-CODE

Word16 G_pitch     (    // o : Gain of pitch lag saturated to 1.2
    enum Mode mode,     // i : AMR mode
    Word16 xn[],        // i : Pitch target.
    Word16 y1[],        // i : Filtered adaptive codebook.
    Word16 g_coeff[],   // i : Correlations need for gain quantization
    Word16 L_subfr      // i : Length of subframe.
)
{
    Word16 i;
    Word16 xy, yy, exp_xy, exp_yy, gain;
    Word32 s;

    Word16 scaled_y1[L_SUBFR];   // Usually dynamic allocation of (L_subfr)

    // divide "y1[]" by 4 to avoid overflow

// The reference ETSI code uses a global overflow Flag. However in the actual
// implementation a pointer to the overflow flag is passed into the function.

    for (i = 0; i < L_subfr; i++)
    {
        scaled_y1[i] = shr (y1[i], 2);
    }

    // Compute scalar product <y1[],y1[]>

    // Q12 scaling / MR122
    Overflow = 0;
    s = 1L; // Avoid case of all zeros
    for (i = 0; i < L_subfr; i++)
    {
        s = L_mac (s, y1[i], y1[i]);
    }
    if (Overflow == 0)       // Test for overflow
    {
        exp_yy = norm_l (s);
        yy = pv_round (L_shl (s, exp_yy));
    }
    else
    {
        s = 1L; // Avoid case of all zeros
        for (i = 0; i < L_subfr; i++)
        {
            s = L_mac (s, scaled_y1[i], scaled_y1[i]);
        }
        exp_yy = norm_l (s);
        yy = pv_round (L_shl (s, exp_yy));
        exp_yy = sub (exp_yy, 4);
    }

    // Compute scalar product <xn[],y1[]>

    Overflow = 0;
    s = 1L; // Avoid case of all zeros

    for (i = 0; i < L_subfr; i++)
    {
        s = L_mac(s, xn[i], y1[i]);
    }
    if (Overflow == 0)
    {
        exp_xy = norm_l (s);
        xy = pv_round (L_shl (s, exp_xy));
    }
    else
    {
        s = 1L; // Avoid case of all zeros
        for (i = 0; i < L_subfr; i++)
        {
            s = L_mac (s, xn[i], scaled_y1[i]);
        }
        exp_xy = norm_l (s);
        xy = pv_round (L_shl (s, exp_xy));
        exp_xy = sub (exp_xy, 2);
    }

    g_coeff[0] = yy;
    g_coeff[1] = sub (15, exp_yy);
    g_coeff[2] = xy;
    g_coeff[3] = sub (15, exp_xy);

    // If (xy < 4) gain = 0

    i = sub (xy, 4);

    if (i < 0)
        return ((Word16) 0);

    // compute gain = xy/yy

    xy = shr (xy, 1);                  // Be sure xy < yy
    gain = div_s (xy, yy);

    i = sub (exp_xy, exp_yy);      // Denormalization of division
    gain = shr (gain, i);

    // if(gain >1.2) gain = 1.2

    if (sub (gain, 19661) > 0)
    {
        gain = 19661;
    }

    if (sub(mode, MR122) == 0)
    {
       // clear 2 LSBits
       gain = gain & 0xfffC;
    }

    return (gain);
}

------------------------------------------------------------------------------
 CAUTION [optional]
 [State any special notes, constraints or cautions for users of this function]

------------------------------------------------------------------------------
*/

Word16 G_pitch(         /* o : Gain of pitch lag saturated to 1.2       */
    enum Mode mode,     /* i : AMR mode                                 */
    Word16 xn[],        /* i : Pitch target.                            Q0  */
    Word16 y1[],        /* i : Filtered adaptive codebook.              Q12 */
    Word16 g_coeff[],   /* i : Correlations need for gain quantization  */
    Word16 L_subfr,     /* i : Length of subframe.                      */
    Flag   *pOverflow   /* i/o : Overflow flag                          */
)
{

    Word16 i;
    Word16 xy;
    Word16 yy;
    Word16 exp_xy;
    Word16 exp_yy;
    Word16 gain;
    Word16 tmp;
    Word32 s;
    Word32 s1;
    Word32 L_temp;                      /* Use this as an intermediate value */
    Word16 *p_xn = &xn[0];
    Word16 *p_y1 = &y1[0];

    /* Compute scalar product <y1[],y1[]> */

    /* Q12 scaling / MR122 */
    *pOverflow = 0;
    s = 0;

    for (i = L_subfr >> 2; i != 0; i--)
    {
        s = amrnb_fxp_mac_16_by_16bb((Word32) * (p_y1), (Word32) * (p_y1), s);
        p_y1++;
        s = amrnb_fxp_mac_16_by_16bb((Word32) * (p_y1), (Word32) * (p_y1), s);
        p_y1++;
        s = amrnb_fxp_mac_16_by_16bb((Word32) * (p_y1), (Word32) * (p_y1), s);
        p_y1++;
        s = amrnb_fxp_mac_16_by_16bb((Word32) * (p_y1), (Word32) * (p_y1), s);
        p_y1++;
    }
    if ((s >= 0) & (s < 0x40000000))
    {
        s <<= 1;
        s  += 1;            /* Avoid case of all zeros */

        exp_yy = norm_l(s);             /* Note 0<=exp_yy <= 31 */
        L_temp = s << exp_yy;
        yy = pv_round(L_temp, pOverflow);
    }
    else
    {
        s = 0;                      /* Avoid case of all zeros */
        p_y1 = &y1[0];
        for (i = (L_subfr >> 1); i != 0; i--)
        {
            tmp = *(p_y1++) >> 2;
            s = amrnb_fxp_mac_16_by_16bb((Word32) tmp, (Word32) tmp, s);
            tmp = *(p_y1++) >> 2;
            s = amrnb_fxp_mac_16_by_16bb((Word32) tmp, (Word32) tmp, s);
        }

        s <<= 1;
        s  += 1;            /* Avoid case of all zeros */

        exp_yy = norm_l(s);
        L_temp = s << exp_yy;
        yy = pv_round(L_temp, pOverflow);
        exp_yy = exp_yy - 4;

    }

    /* Compute scalar product <xn[],y1[]> */

    s = 0;
    p_y1 = &y1[0];
    *pOverflow = 0;

    for (i = L_subfr; i != 0; i--)
    {
        L_temp = ((Word32) * (p_xn++) * *(p_y1++));
        s1 = s;
        s = s1 + L_temp;

        if ((s1 ^ L_temp) > 0)
        {
            if ((s1 ^ s) < 0)
            {
                *pOverflow = 1;
                break;
            }
        }
    }

    if (!(*pOverflow))
    {

        s <<= 1;
        s  += 1;            /* Avoid case of all zeros */

        exp_xy = norm_l(s);             /* Note 0<=exp_yy <= 31 */
        L_temp = s << exp_xy;
        xy = pv_round(L_temp, pOverflow);
    }
    else
    {
        s = 0;  /* re-initialize calculations */
        p_y1 = &y1[0];
        p_xn = &xn[0];

        for (i = (L_subfr >> 2); i != 0; i--)
        {
            L_temp = (Word32)(*(p_y1++) >> 2);
            s = amrnb_fxp_mac_16_by_16bb((Word32) * (p_xn++), L_temp, s);
            L_temp = (Word32)(*(p_y1++) >> 2);
            s = amrnb_fxp_mac_16_by_16bb((Word32) * (p_xn++), L_temp, s);
            L_temp = (Word32)(*(p_y1++) >> 2);
            s = amrnb_fxp_mac_16_by_16bb((Word32) * (p_xn++), L_temp, s);
            L_temp = (Word32)(*(p_y1++) >> 2);
            s = amrnb_fxp_mac_16_by_16bb((Word32) * (p_xn++), L_temp, s);
        }

        s <<= 1;
        s  += 1;            /* Avoid case of all zeros */

        exp_xy = norm_l(s);
        L_temp = s << exp_xy;
        xy = pv_round(L_temp, pOverflow);
        exp_xy = exp_xy - 4;

    }

    g_coeff[0] = yy;
    g_coeff[1] = 15 - exp_yy;
    g_coeff[2] = xy;
    g_coeff[3] = 15 - exp_xy;

    /* If (xy < 4) gain = 0 */
    if (xy < 4)
    {
        return ((Word16) 0);
    }

    /* compute gain = xy/yy */
    /* Be sure xy < yy */

    xy = xy >> 1;

    gain = div_s(xy, yy);

    i = exp_xy - exp_yy;               /* Denormalization of division */

    gain = shr(gain, i, pOverflow);


    /* if(gain >1.2) gain = 1.2 */
    if (gain > 19661)
    {
        gain = 19661;
    }

    if (mode == MR122)
    {
        /* clear 2 LSBits */
        gain = gain & 0xfffC;
    }

    return(gain);

}
