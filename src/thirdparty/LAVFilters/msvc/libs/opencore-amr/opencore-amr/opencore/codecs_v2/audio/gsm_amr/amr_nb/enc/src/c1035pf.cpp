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



 Filename: c1035pf.cpp
 Functions: q_p
            build_code
            code_10i40_35bits

------------------------------------------------------------------------------
 MODULE DESCRIPTION

 This file contains the function that searches a 35 bit algebraic codebook
 containing 10 pulses in a frame of 40 samples.

------------------------------------------------------------------------------
*/

/*----------------------------------------------------------------------------
; INCLUDES
----------------------------------------------------------------------------*/
#include "c1035pf.h"
#include "cnst.h"
#include "basic_op.h"
#include "inv_sqrt.h"
#include "set_sign.h"
#include "cor_h.h"
#include "cor_h_x.h"
#include "s10_8pf.h"
#include "oscl_mem.h"

/*----------------------------------------------------------------------------
; MACROS
; [Define module specific macros here]
----------------------------------------------------------------------------*/


/*----------------------------------------------------------------------------
; DEFINES
; [Include all pre-processor statements here. Include conditional
; compile variables also.]
----------------------------------------------------------------------------*/
#define NB_PULSE  10

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
 FUNCTION NAME: q_p
------------------------------------------------------------------------------
 INPUT AND OUTPUT DEFINITIONS

 Inputs:
    pShift_reg = pointer to Old CN generator shift register state (Word32)
    no_bits = Number of bits (Word16)

 Outputs:
    pShift_reg -> Updated CN generator shift register state

 Returns:
    noise_bits = Generated random integer value (Word16)

 Global Variables Used:
    None

 Local Variables Needed:
    None

------------------------------------------------------------------------------
 FUNCTION DESCRIPTION

 This is a local function that determnes the index of the pulses by looking up
 the gray encoder table

------------------------------------------------------------------------------
 REQUIREMENTS

 None

------------------------------------------------------------------------------
 REFERENCES

 c1035pf.c, UMTS GSM AMR speech codec, R99 - Version 3.2.0, March 2, 2001

------------------------------------------------------------------------------
 PSEUDO-CODE

void q_p (
    Word16 *ind,        // Pulse position
    Word16 n            // Pulse number
)
{
    Word16 tmp;

    tmp = *ind;

    if (sub (n, 5) < 0)
    {
        *ind = (tmp & 0x8) | gray[tmp & 0x7];
    }
    else
    {
        *ind = gray[tmp & 0x7];
    }
}

------------------------------------------------------------------------------
 CAUTION [optional]
 [State any special notes, constraints or cautions for users of this function]

------------------------------------------------------------------------------
*/

/*----------------------------------------------------------------------------
; FUNCTION CODE
----------------------------------------------------------------------------*/

void q_p(
    Word16 *pInd,       /* Pulse position */
    Word16 n,            /* Pulse number   */
    const Word16* gray_ptr
)
{
    Word16 tmp;

    tmp = *pInd;

    if (n < 5)
    {
        *pInd = (tmp & 0x8) | gray_ptr[tmp & 0x7];
    }
    else
    {
        *pInd = gray_ptr[tmp & 0x7];
    }
}

/*
------------------------------------------------------------------------------
 FUNCTION NAME: build_code
------------------------------------------------------------------------------
 INPUT AND OUTPUT DEFINITIONS

 Inputs:
    pSeed = pointer to the Old CN generator shift register state (Word32)
    n_param = Number of parameters to randomize (Word16)
    param_size_table = table holding paameter sizes (Word16)
    param[] = array to hold CN generated paramters (Word16)
    pOverflow = pointer to overflow flag (Flag)

 Outputs:
    param[] = CN generated parameters (Word16)
    pSeed = Updated CN generator shift register state (Word16)
    pOverflow -> 1 if overflow occured

 Returns:
    None

 Global Variables Used:
    None

 Local Variables Needed:
    None

------------------------------------------------------------------------------
 FUNCTION DESCRIPTION

 This function builds the codeword, the filtered codeword and index of the
 codevector, based on the signs and positions of 10 pulses.
------------------------------------------------------------------------------
 REQUIREMENTS

 None

------------------------------------------------------------------------------
 REFERENCES

 c1035pf.c, UMTS GSM AMR speech codec, R99 - Version 3.2.0, March 2, 2001

------------------------------------------------------------------------------
 PSEUDO-CODE
static void build_code (
    Word16 codvec[],    // (i)  : position of pulses
    Word16 sign[],      // (i)  : sign of d[n]
    Word16 cod[],       // (o)  : innovative code vector
    Word16 h[],         // (i)  : impulse response of weighted synthesis filter
    Word16 y[],         // (o)  : filtered innovative code
    Word16 indx[]       // (o)  : index of 10 pulses (sign+position)
)
{
    Word16 i, j, k, track, index, _sign[NB_PULSE];
    Word16 *p0, *p1, *p2, *p3, *p4, *p5, *p6, *p7, *p8, *p9;
    Word32 s;

    for (i = 0; i < L_CODE; i++)
    {
        cod[i] = 0;
    }
    for (i = 0; i < NB_TRACK; i++)
    {
        indx[i] = -1;
    }

    for (k = 0; k < NB_PULSE; k++)
    {
        // read pulse position
        i = codvec[k];
        // read sign
        j = sign[i];

        index = mult (i, 6554);                  // index = pos/5
        // track = pos%5
        track = sub (i, extract_l (L_shr (L_mult (index, 5), 1)));

        if (j > 0)
        {
            cod[i] = add (cod[i], 4096);
            _sign[k] = 8192;

        }
        else
        {
            cod[i] = sub (cod[i], 4096);
            _sign[k] = -8192;
            index = add (index, 8);
        }

        if (indx[track] < 0)
        {
            indx[track] = index;
        }
        else
        {
            if (((index ^ indx[track]) & 8) == 0)
            {
                // sign of 1st pulse == sign of 2nd pulse

                if (sub (indx[track], index) <= 0)
                {
                    indx[track + 5] = index;
                }
                else
                {
                    indx[track + 5] = indx[track];
                    indx[track] = index;
                }
            }
            else
            {
                // sign of 1st pulse != sign of 2nd pulse

                if (sub ((Word16)(indx[track] & 7), (Word16)(index & 7)) <= 0)
                {
                    indx[track + 5] = indx[track];
                    indx[track] = index;
                }
                else
                {
                    indx[track + 5] = index;
                }
            }
        }
    }

    p0 = h - codvec[0];
    p1 = h - codvec[1];
    p2 = h - codvec[2];
    p3 = h - codvec[3];
    p4 = h - codvec[4];
    p5 = h - codvec[5];
    p6 = h - codvec[6];
    p7 = h - codvec[7];
    p8 = h - codvec[8];
    p9 = h - codvec[9];

    for (i = 0; i < L_CODE; i++)
    {
        s = 0;
        s = L_mac (s, *p0++, _sign[0]);
        s = L_mac (s, *p1++, _sign[1]);
        s = L_mac (s, *p2++, _sign[2]);
        s = L_mac (s, *p3++, _sign[3]);
        s = L_mac (s, *p4++, _sign[4]);
        s = L_mac (s, *p5++, _sign[5]);
        s = L_mac (s, *p6++, _sign[6]);
        s = L_mac (s, *p7++, _sign[7]);
        s = L_mac (s, *p8++, _sign[8]);
        s = L_mac (s, *p9++, _sign[9]);
        y[i] = pv_round (s);
    }
}

------------------------------------------------------------------------------
 CAUTION [optional]
 [State any special notes, constraints or cautions for users of this function]

------------------------------------------------------------------------------
*/

/*----------------------------------------------------------------------------
; FUNCTION CODE
----------------------------------------------------------------------------*/
static void build_code(
    Word16 codvec[],    /* (i)  : position of pulses                        */
    Word16 sign[],      /* (i)  : sign of d[n]                              */
    Word16 cod[],       /* (o)  : innovative code vector                    */
    Word16 h[],         /* (i)  : impulse response of weighted synthesis filter*/
    Word16 y[],         /* (o)  : filtered innovative code                  */
    Word16 indx[],      /* (o)  : index of 10 pulses (sign+position)        */
    Flag   *pOverflow   /* i/o  : overflow Flag                             */
)
{
    Word16 i, k, track, index, _sign[NB_PULSE];
    Word16 *p0, *p1, *p2, *p3, *p4, *p5, *p6, *p7, *p8, *p9;
    Word32 s;
    Word16 temp;
    Word16 *p__sign;
    Word16 *p_y;
    Word16 *p_codvec;

    OSCL_UNUSED_ARG(pOverflow);

    oscl_memset(cod, 0, L_CODE*sizeof(*cod));
    oscl_memset(indx, 0xFF, NB_TRACK*sizeof(*indx));

    p__sign = _sign;

    p0 = &codvec[0];

    for (k = 0; k < NB_PULSE; k++)
    {
        /* read pulse position */
        i = *(p0++);
        /* read sign           */

        index = ((Word32)i * 6554) >> 15;       /* index = pos/5    */

        /* track = pos%5 */
        /* track = sub (i, extract_l (L_shr (L_mult (index, 5), 1))); */
        track = i - (index + (index << 2));

        if (sign[i] > 0)
        {
            cod[i] +=  4096;
            *(p__sign++) = 8192;

        }
        else
        {
            cod[i] -=  4096;
            *(p__sign++) = -8192;
            /* index = add (index, 8); */
            index += 8;
        }

        p1 = &indx[track];

        temp = *p1;

        if (temp < 0)
        {
            *p1 = index;
        }
        else
        {
            if (((index ^ temp) & 8) == 0)
            {
                /* sign of 1st pulse == sign of 2nd pulse */

                /* if (sub (indx[track], index) <= 0) */
                if (temp <= index)
                {
                    *(p1 + 5) = index;
                }
                else
                {
                    *(p1 + 5) = temp;
                    *p1 = index;
                }
            }
            else
            {
                /* sign of 1st pulse != sign of 2nd pulse */

                /* if (sub ((Word16)(indx[track] & 7), (Word16)(index & 7)) <= 0) */
                if ((temp & 7) <= (index & 7))
                {
                    *(p1 + 5) = temp;
                    *p1 = index;
                }
                else
                {
                    *(p1 + 5) = index;
                }
            }
        }
    }

    p_codvec = &codvec[0];

    p0 = h - *(p_codvec++);
    p1 = h - *(p_codvec++);
    p2 = h - *(p_codvec++);
    p3 = h - *(p_codvec++);
    p4 = h - *(p_codvec++);
    p5 = h - *(p_codvec++);
    p6 = h - *(p_codvec++);
    p7 = h - *(p_codvec++);
    p8 = h - *(p_codvec++);
    p9 = h - *(p_codvec++);

    p_y = y;

    for (i = L_CODE; i != 0; i--)
    {
        p__sign = _sign;

        s  = (*p0++ * *(p__sign++)) >> 7;
        s += (*p1++ * *(p__sign++)) >> 7;
        s += (*p2++ * *(p__sign++)) >> 7;
        s += (*p3++ * *(p__sign++)) >> 7;
        s += (*p4++ * *(p__sign++)) >> 7;
        s += (*p5++ * *(p__sign++)) >> 7;
        s += (*p6++ * *(p__sign++)) >> 7;
        s += (*p7++ * *(p__sign++)) >> 7;
        s += (*p8++ * *(p__sign++)) >> 7;
        s += (*p9++ * *(p__sign++)) >> 7;

        *(p_y++) = (s + 0x080) >> 8;
    }

}

/*
------------------------------------------------------------------------------
 FUNCTION NAME: code_10i40_35bits
------------------------------------------------------------------------------
 INPUT AND OUTPUT DEFINITIONS

 Inputs:
    pSeed = pointer to the Old CN generator shift register state (Word32)
    n_param = Number of parameters to randomize (Word16)
    param_size_table = table holding paameter sizes (Word16)
    param[] = array to hold CN generated paramters (Word16)
    pOverflow = pointer to overflow flag (Flag)

 Outputs:
    param[] = CN generated parameters (Word16)
    pSeed = Updated CN generator shift register state (Word16)
    pOverflow -> 1 if overflow occured

 Returns:
    None

 Global Variables Used:
    None

 Local Variables Needed:
    None

------------------------------------------------------------------------------
 FUNCTION DESCRIPTION

 This function searches a 35 bit algebraic codebook containing 10 pulses in a
 frame of 40 samples.

 The code contains 10 nonzero pulses: i0...i9.
 All pulses can have two possible amplitudes: +1 or -1.
 The 40 positions in a subframe are divided into 5 tracks of
 interleaved positions. Each track contains two pulses.
 The pulses can have the following possible positions:

    i0, i5 :  0, 5, 10, 15, 20, 25, 30, 35.
    i1, i6 :  1, 6, 11, 16, 21, 26, 31, 36.
    i2, i7 :  2, 7, 12, 17, 22, 27, 32, 37.
    i3, i8 :  3, 8, 13, 18, 23, 28, 33, 38.
    i4, i9 :  4, 9, 14, 19, 24, 29, 34, 39.

 Each pair of pulses require 1 bit for their signs and 6 bits for their
 positions (3 bits + 3 bits). This results in a 35 bit codebook.
 The function determines the optimal pulse signs and positions, builds
 the codevector, and computes the filtered codevector.

------------------------------------------------------------------------------
 REQUIREMENTS

 None

------------------------------------------------------------------------------
 REFERENCES

 c1035pf.c, UMTS GSM AMR speech codec, R99 - Version 3.2.0, March 2, 2001

------------------------------------------------------------------------------
 PSEUDO-CODE
void code_10i40_35bits (
    Word16 x[],   // (i)   : target vector
    Word16 cn[],  // (i)   : residual after long term prediction
    Word16 h[],   // (i)   : impulse response of weighted synthesis filter
                           // h[-L_subfr..-1] must be set to zero
    Word16 cod[], // (o)   : algebraic (fixed) codebook excitation
    Word16 y[],   // (o)   : filtered fixed codebook excitation
    Word16 indx[] // (o)   : index of 10 pulses (sign + position)
)
{
    Word16 ipos[NB_PULSE], pos_max[NB_TRACK], codvec[NB_PULSE];
    Word16 dn[L_CODE], sign[L_CODE];
    Word16 rr[L_CODE][L_CODE], i;

    cor_h_x (h, x, dn, 2);
    set_sign12k2 (dn, cn, sign, pos_max, NB_TRACK, ipos, STEP);
    cor_h (h, sign, rr);

    search_10and8i40 (NB_PULSE, STEP, NB_TRACK,
                      dn, rr, ipos, pos_max, codvec);

    build_code (codvec, sign, cod, h, y, indx);
    for (i = 0; i < 10; i++)
    {
        q_p (&indx[i], i);
    }
    return;
}

------------------------------------------------------------------------------
 CAUTION [optional]
 [State any special notes, constraints or cautions for users of this function]

------------------------------------------------------------------------------
*/

/*----------------------------------------------------------------------------
; FUNCTION CODE
----------------------------------------------------------------------------*/
void code_10i40_35bits(
    Word16 x[],     /* (i)   : target vector                                */
    Word16 cn[],    /* (i)   : residual after long term prediction          */
    Word16 h[],     /* (i)   : impulse response of weighted synthesis filter
                             h[-L_subfr..-1] must be set to zero            */
    Word16 cod[],   /* (o)   : algebraic (fixed) codebook excitation        */
    Word16 y[],     /* (o)   : filtered fixed codebook excitation           */
    Word16 indx[],  /* (o)   : index of 10 pulses (sign + position)         */
    const Word16* gray_ptr, /* (i) : ptr to read-only table                 */
    Flag *pOverflow /* (i/o) : overflow Flag                                */
)
{
    Word16 ipos[NB_PULSE], pos_max[NB_TRACK], codvec[NB_PULSE];
    Word16 dn[L_CODE], sign[L_CODE];
    Word16 rr[L_CODE][L_CODE], i;

    cor_h_x(h, x, dn, 2, pOverflow);
    set_sign12k2(dn, cn, sign, pos_max, NB_TRACK, ipos, STEP, pOverflow);
    cor_h(h, sign, rr, pOverflow);

    search_10and8i40(NB_PULSE, STEP, NB_TRACK,
                     dn, rr, ipos, pos_max, codvec, pOverflow);

    build_code(codvec, sign, cod, h, y, indx, pOverflow);
    for (i = 0; i < 10; i++)
    {
        q_p(&indx[i], i, gray_ptr);
    }
    return;
}

