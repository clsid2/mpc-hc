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



 Filename: g_code.cpp

------------------------------------------------------------------------------
*/

/*----------------------------------------------------------------------------
; INCLUDES
----------------------------------------------------------------------------*/
#include    "g_code.h"
#include    "cnst.h"
#include    "basic_op.h"
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
 FUNCTION NAME: G_code
------------------------------------------------------------------------------
 INPUT AND OUTPUT DEFINITIONS

 Inputs:
    xn2[] = target vector (Word16)
    y2[] = filtered innovation vector
    pOverflow = pointer to overflow (Flag)

 Outputs:
    pOverflow -> 1 if the innovative gain calculation resulted in overflow

 Returns:
    gain = Gain of Innovation code (Word16)

 Global Variables Used:
    None

 Local Variables Needed:
    None

------------------------------------------------------------------------------
 FUNCTION DESCRIPTION

 This function computes the innovative codebook gain.

 The innovative codebook gain is given by
    g = <x[], y[]> / <y[], y[]>

 where x[] is the target vector, y[] is the filtered innovative codevector,
 and <> denotes dot product.

------------------------------------------------------------------------------
 REQUIREMENTS

 None

------------------------------------------------------------------------------
 REFERENCES

 [1] g_code.c, UMTS GSM AMR speech codec, R99 - Version 3.2.0, March 2, 2001

------------------------------------------------------------------------------
 PSEUDO-CODE

Word16 G_code (         // out   : Gain of innovation code
    Word16 xn2[],       // in    : target vector
    Word16 y2[]         // in    : filtered innovation vector
)
{
    Word16 i;
    Word16 xy, yy, exp_xy, exp_yy, gain;
    Word16 scal_y2[L_SUBFR];
    Word32 s;

// The original ETSI implementation uses a global overflow flag. However in
// actual implementation a pointer to Overflow flag is passed into the
// function for access by the low level math functions.

    // Scale down Y[] by 2 to avoid overflow

    for (i = 0; i < L_SUBFR; i++)
    {
        scal_y2[i] = shr (y2[i], 1);
    }

    // Compute scalar product <X[],Y[]>

    s = 1L; // Avoid case of all zeros
    for (i = 0; i < L_SUBFR; i++)
    {
        s = L_mac (s, xn2[i], scal_y2[i]);
    }
    exp_xy = norm_l (s);
    xy = extract_h (L_shl (s, exp_xy));

    // If (xy < 0) gain = 0

    if (xy <= 0)
        return ((Word16) 0);

    // Compute scalar product <Y[],Y[]>

    s = 0L;
    for (i = 0; i < L_SUBFR; i++)
    {
        s = L_mac (s, scal_y2[i], scal_y2[i]);
    }
    exp_yy = norm_l (s);
    yy = extract_h (L_shl (s, exp_yy));

    // compute gain = xy/yy

    xy = shr (xy, 1);                 // Be sure xy < yy
    gain = div_s (xy, yy);

    // Denormalization of division
    i = add (exp_xy, 5);              // 15-1+9-18 = 5
    i = sub (i, exp_yy);

    gain = shl (shr (gain, i), 1);    // Q0 -> Q1/

    return (gain);
}


------------------------------------------------------------------------------
 CAUTION [optional]
 [State any special notes, constraints or cautions for users of this function]

------------------------------------------------------------------------------
*/

/*----------------------------------------------------------------------------
; FUNCTION CODE
----------------------------------------------------------------------------*/
Word16 G_code(          /* o     : Gain of innovation code         */
    Word16 xn2[],       /* i     : target vector                   */
    Word16 y2[],        /* i     : filtered innovation vector      */
    Flag   *pOverflow   /* i/o   : overflow flag                   */
)
{
    Word16 i;
    Word16 xy, yy, exp_xy, exp_yy, gain;
    Word32 s;

    Word16 *p_xn2 = xn2;
    Word16 *p_y2  = y2;
    Word16 temp;
    Word32 temp2;

    OSCL_UNUSED_ARG(pOverflow);

    /* Compute scalar product <X[],Y[]> */
    s = 0;

    for (i = (L_SUBFR >> 2); i != 0 ; i--)
    {
        temp2 = (Word32)(*(p_y2++) >> 1);
        s = amrnb_fxp_mac_16_by_16bb((Word32) * (p_xn2++), temp2, s);
        temp2 = (Word32)(*(p_y2++) >> 1);
        s = amrnb_fxp_mac_16_by_16bb((Word32) * (p_xn2++), temp2, s);
        temp2 = (Word32)(*(p_y2++) >> 1);
        s = amrnb_fxp_mac_16_by_16bb((Word32) * (p_xn2++), temp2, s);
        temp2 = (Word32)(*(p_y2++) >> 1);
        s = amrnb_fxp_mac_16_by_16bb((Word32) * (p_xn2++), temp2, s);
    }
    s <<= 1;
    exp_xy = norm_l(s + 1); /* Avoid case of all zeros, add 1 */

    if (exp_xy < 17)        /* extra right shift to be sure xy < yy */
    {
        xy = (Word16)(s >> (17 - exp_xy));
    }
    else
    {
        xy = (Word16)(s << (exp_xy - 17));
    }

    /* If (xy < 0) gain = 0  */

    if (xy <= 0)
    {
        return ((Word16) 0);
    }

    /* Compute scalar product <Y[],Y[]> */

    s = 0L;
    p_y2  = y2;

    for (i = (L_SUBFR >> 1); i != 0 ; i--)
    {
        temp = *(p_y2++) >> 1;
        s += ((Word32) temp * temp) >> 2;
        temp = *(p_y2++) >> 1;
        s += ((Word32) temp * temp) >> 2;
    }
    s <<= 3;
    exp_yy = norm_l(s);

    if (exp_yy < 16)
    {
        yy = (Word16)(s >> (16 - exp_yy));
    }
    else
    {
        yy = (Word16)(s << (exp_yy - 16));
    }

    gain = div_s(xy, yy);

    /* Denormalization of division */
    i  = exp_xy + 5;                                /* 15-1+9-18 = 5 */
    i -= exp_yy;

    // gain = shl (shr (gain, i), 1);    /* Q0 -> Q1 */

    if (i > 1)
    {
        gain >>= i - 1;
    }
    else
    {
        gain <<= 1 - i;
    }


    return (gain);
}
