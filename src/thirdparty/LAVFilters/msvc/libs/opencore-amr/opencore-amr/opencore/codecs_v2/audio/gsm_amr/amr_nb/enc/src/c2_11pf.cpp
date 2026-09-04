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



 Filename: c2_11pf.cpp
 Functions:
            code_2i40_11bits
            search_2i40
            build_code

------------------------------------------------------------------------------
 MODULE DESCRIPTION

 *************************************************************************
 *
 *  FUNCTION:  code_2i40_11bits()
 *
 *  PURPOSE:  Searches a 11 bit algebraic codebook containing 2 pulses
 *            in a frame of 40 samples.
 *
 *  DESCRIPTION:
 *    The code length is 40, containing 2 nonzero pulses: i0...i1.
 *    All pulses can have two possible amplitudes: +1 or -1.
 *    Pulse i0 can have 2x8=16 possible positions, pulse i1 can have
 *    4x8=32 positions.
 *
 *       i0 :  1, 6, 11, 16, 21, 26, 31, 36.
 *             3, 8, 13, 18, 23, 28, 33, 38.
 *       i1 :  0, 5, 10, 15, 20, 25, 30, 35.
 *             1, 6, 11, 16, 21, 26, 31, 36.
 *             2, 7, 12, 17, 22, 27, 32, 37.
 *             4, 9, 14, 19, 24, 29, 34, 39.
 *
 *************************************************************************
------------------------------------------------------------------------------
*/

/*----------------------------------------------------------------------------
; INCLUDES
----------------------------------------------------------------------------*/
#include "c2_11pf.h"
#include "typedef.h"
#include "basic_op.h"
#include "inv_sqrt.h"
#include "cnst.h"
#include "cor_h.h"
#include "set_sign.h"

/*----------------------------------------------------------------------------
; MACROS
; Define module specific macros here
----------------------------------------------------------------------------*/

/*----------------------------------------------------------------------------
; DEFINES
; Include all pre-processor statements here. Include conditional
; compile variables also.
----------------------------------------------------------------------------*/
#define NB_PULSE  2

#define _1_2    (Word16)(32768L/2)
#define _1_4    (Word16)(32768L/4)
#define _1_8    (Word16)(32768L/8)
#define _1_16   (Word16)(32768L/16)

/*----------------------------------------------------------------------------
; LOCAL FUNCTION DEFINITIONS
; Function Prototype declaration
----------------------------------------------------------------------------*/
static void search_2i40(
    Word16 dn[],        /* i : correlation between target and h[]            */
    Word16 rr[][L_CODE],/* i : matrix of autocorrelation                     */
    Word16 codvec[],    /* o : algebraic codebook vector                     */
    Flag   * pOverflow
);

static Word16 build_code(
    Word16 codvec[],    /* i : algebraic codebook vector                     */
    Word16 dn_sign[],   /* i : sign of dn[]                                  */
    Word16 cod[],       /* o : algebraic (fixed) codebook excitation         */
    Word16 h[],         /* i : impulse response of weighted synthesis filter */
    Word16 y[],         /* o : filtered fixed codebook excitation            */
    Word16 sign[],      /* o : sign of 2 pulses                              */
    Flag   * pOverflow
);

/*----------------------------------------------------------------------------
; LOCAL VARIABLE DEFINITIONS
; Variable declaration - defined here and used outside this module
----------------------------------------------------------------------------*/

const Word16 startPos1[2] = {1, 3};
const Word16 startPos2[4] = {0, 1, 2, 4};

/*
------------------------------------------------------------------------------
 FUNCTION NAME: code_2i40_11bits
------------------------------------------------------------------------------
 INPUT AND OUTPUT DEFINITIONS

 Inputs:
    x,  target vector, array of type Word16
    h,  impulse response of weighted synthesis filter, array of type Word16
    T0, Pitch lag, variable of type Word16
    pitch_sharp, Last quantized pitch gain, variable of type Word16

 Outputs:
    code[], Innovative codebook, array of type Word16
    y[],    filtered fixed codebook excitation, array of type Word16
    sign,   Signs of 2 pulses, pointer of type Word16 *
    pOverflow  Flag set when overflow occurs, pointer of type Flag *

 Returns:
    index

 Global Variables Used:
    None

 Local Variables Needed:
    None

------------------------------------------------------------------------------
 FUNCTION DESCRIPTION

     Searches a 11 bit algebraic codebook containing 2 pulses
     in a frame of 40 samples.

     The code length is 40, containing 2 nonzero pulses: i0...i1.
     All pulses can have two possible amplitudes: +1 or -1.
     Pulse i0 can have 2x8=16 possible positions, pulse i1 can have
     4x8=32 positions.

        i0 :  1, 6, 11, 16, 21, 26, 31, 36.
              3, 8, 13, 18, 23, 28, 33, 38.
        i1 :  0, 5, 10, 15, 20, 25, 30, 35.
              1, 6, 11, 16, 21, 26, 31, 36.
              2, 7, 12, 17, 22, 27, 32, 37.
              4, 9, 14, 19, 24, 29, 34, 39.

------------------------------------------------------------------------------
 REQUIREMENTS

 None

------------------------------------------------------------------------------
 REFERENCES

 c2_11pf.c, UMTS GSM AMR speech codec, R99 - Version 3.2.0, March 2, 2001

------------------------------------------------------------------------------
 PSEUDO-CODE


------------------------------------------------------------------------------
 CAUTION [optional]
 [State any special notes, constraints or cautions for users of this function]

------------------------------------------------------------------------------
*/
Word16 code_2i40_11bits(
    Word16 x[],         /* i : target vector                                 */
    Word16 h[],         /* i : impulse response of weighted synthesis filter */
    /*     h[-L_subfr..-1] must be set to zero.          */
    Word16 T0,          /* i : Pitch lag                                     */
    Word16 pitch_sharp, /* i : Last quantized pitch gain                     */
    Word16 code[],      /* o : Innovative codebook                           */
    Word16 y[],         /* o : filtered fixed codebook excitation            */
    Word16 * sign,      /* o : Signs of 2 pulses                             */
    Flag   * pOverflow  /* o : Flag set when overflow occurs                 */
)
{
    Word16 codvec[NB_PULSE];
    Word16 dn[L_CODE];
    Word16 dn2[L_CODE];
    Word16 dn_sign[L_CODE];

    Word16 rr[L_CODE][L_CODE];

    Word16 i;
    Word16 index;
    Word16 sharp;
    Word16 tempWord;

    sharp = pitch_sharp << 1;

    if (T0 < L_CODE)
    {
        for (i = T0; i < L_CODE; i++)
        {
            tempWord =
                mult(
                    h[i - T0],
                    sharp,
                    pOverflow);

            h[i] =
                add_16(
                    h[i],
                    tempWord,
                    pOverflow);
        }

    }

    cor_h_x(
        h,
        x,
        dn,
        1,
        pOverflow);

    set_sign(
        dn,
        dn_sign,
        dn2,
        8); /* dn2[] not used in this codebook search */

    cor_h(
        h,
        dn_sign,
        rr,
        pOverflow);

    search_2i40(
        dn,
        rr,
        codvec,
        pOverflow);

    /* function result */

    index =
        build_code(
            codvec,
            dn_sign,
            code,
            h,
            y,
            sign,
            pOverflow);

    /*
    * Compute innovation vector gain.
    * Include fixed-gain pitch contribution into code[].
    */

    if (T0 < L_CODE)
    {
        for (i = T0; i < L_CODE; i++)
        {
            tempWord =
                mult(
                    code[i - T0],
                    sharp,
                    pOverflow);

            code[i] =
                add_16(
                    code[i],
                    tempWord,
                    pOverflow);
        }
    }

    return index;
}

/****************************************************************************/


/*
------------------------------------------------------------------------------
 FUNCTION NAME: search_2i40
------------------------------------------------------------------------------
 INPUT AND OUTPUT DEFINITIONS

 Inputs:
    dn, correlation between target and h[], array of type Word16
    rr, matrix of autocorrelation, double-array of type Word16

 Outputs:
    codvec[],  algebraic codebook vector, array of type Word16
    pOverflow, Flag set when overflow occurs, pointer of type Flag *

 Returns:
    None

 Global Variables Used:
    None

 Local Variables Needed:
    None

------------------------------------------------------------------------------
 FUNCTION DESCRIPTION

 Search the best codevector; determine positions of the 2 pulses
 in the 40-sample frame.

------------------------------------------------------------------------------
 REQUIREMENTS

 None

------------------------------------------------------------------------------
 REFERENCES

 c2_11pf.c, UMTS GSM AMR speech codec, R99 - Version 3.2.0, March 2, 2001

------------------------------------------------------------------------------
 PSEUDO-CODE


------------------------------------------------------------------------------
 CAUTION [optional]
 [State any special notes, constraints or cautions for users of this function]

------------------------------------------------------------------------------
*/

static void search_2i40(
    Word16 dn[],         /* i : correlation between target and h[] */
    Word16 rr[][L_CODE], /* i : matrix of autocorrelation          */
    Word16 codvec[],     /* o : algebraic codebook vector          */
    Flag   * pOverflow   /* o : Flag set when overflow occurs      */
)
{
    Word16 i0;
    Word16 i1;
    Word16 ix = 0; /* initialization only needed to keep gcc silent */
    Word16 track1;
    Word16 track2;
    Word16 ipos[NB_PULSE];

    Word16 psk;
    Word16 ps0;
    Word16 ps1;
    Word16 sq;
    Word16 sq1;

    Word16 alpk;
    Word16 alp;
    Word16 alp_16;

    Word32 s;
    Word32 alp0;
    Word32 alp1;

    Word16 i;
    Word16 *p_codvec = &codvec[0];

    psk = -1;
    alpk = 1;

    for (i = 0; i < NB_PULSE; i++)
    {
        *(p_codvec++) = i;
    }

    /*------------------------------------------------------------------*
    * main loop: try 2x4  tracks.                                      *
    *------------------------------------------------------------------*/

    for (track1 = 0; track1 < 2; track1++)
    {
        for (track2 = 0; track2 < 4; track2++)
        {
            /* fix starting position */
            ipos[0] = startPos1[track1];
            ipos[1] = startPos2[track2];

            /*----------------------------------------------------------------*
            * i0 loop: try 8 positions.                                      *
            *----------------------------------------------------------------*/
            for (i0 = ipos[0]; i0 < L_CODE; i0 += STEP)
            {
                ps0 = dn[i0];

                /* alp0 = L_mult(rr[i0][i0], _1_4, pOverflow); */
                alp0 = (Word32) rr[i0][i0] << 14;

                /*-------------------------------------------------------------*
                * i1 loop: 8 positions.                                       *
                *-------------------------------------------------------------*/

                sq = -1;
                alp = 1;
                ix = ipos[1];

                /*---------------------------------------------------------------*
                * These index have low complexity address computation because   *
                * they are, in fact, pointers with fixed increment. For example,*
                * "rr[i0][i2]" is a pointer initialized to "&rr[i0][ipos[2]]"   *
                * and incremented by "STEP".                                    *
                *---------------------------------------------------------------*/

                for (i1 = ipos[1]; i1 < L_CODE; i1 += STEP)
                {
                    /* idx increment = STEP */
                    ps1 = add_16(ps0, dn[i1], pOverflow);

                    /* alp1 = alp0 + rr[i0][i1] + 1/2*rr[i1][i1]; */

                    /* idx incr = STEP */
                    /* alp1 = L_mac(alp0, rr[i1][i1], _1_4, pOverflow); */
                    alp1 = alp0 + ((Word32) rr[i1][i1] << 14);

                    /* idx incr = STEP */
                    /* alp1 = L_mac(alp1, rr[i0][i1], _1_2, pOverflow); */
                    alp1 += (Word32) rr[i0][i1] << 15;

                    /* sq1 = mult(ps1, ps1, pOverflow); */
                    sq1 = (Word16)(((Word32) ps1 * ps1) >> 15);

                    /* alp_16 = pv_round(alp1, pOverflow); */
                    alp_16 = (Word16)((alp1 + (Word32) 0x00008000L) >> 16);

                    /* s = L_mult(alp, sq1, pOverflow); */
                    s = ((Word32) alp * sq1) << 1;

                    /* s =L_msu(s, sq, alp_16, pOverflow); */
                    s -= (((Word32) sq * alp_16) << 1);

                    if (s > 0)
                    {
                        sq = sq1;
                        alp = alp_16;
                        ix = i1;
                    }

                } /* for (i1 = ipos[1]; i1 < L_CODE; i1 += STEP) */

                /* memorize codevector if this one is better than the last one. */

                /* s = L_mult(alpk, sq, pOverflow); */
                s = ((Word32) alpk * sq) << 1;

                /* s = L_msu(s, psk, alp, pOverflow); */
                s -= (((Word32) psk * alp) << 1);

                if (s > 0)
                {
                    psk = sq;
                    alpk = alp;
                    p_codvec = &codvec[0];

                    *(p_codvec++) = i0;
                    *(p_codvec) = ix;
                }

            } /* for (i0 = ipos[0]; i0 < L_CODE; i0 += STEP) */

        } /* for (track2 = 0; track2 < 4; track2++) */

    } /* for (track1 = 0; track1 < 2; track1++) */

    return;

} /* search_2i40 */

/****************************************************************************/


/*
------------------------------------------------------------------------------
 FUNCTION NAME: build_code
------------------------------------------------------------------------------
 INPUT AND OUTPUT DEFINITIONS

 Inputs:
    codvec,  position of pulses, array of type Word16
    dn_sign, sign of pulses, array of type Word16
    h,       impulse response of weighted synthesis filter, Word16 array

 Outputs:

    cod,       innovative code vector, array of type Word16
    y[],       filtered innovative code, array of type Word16
    sign[],    sign of 2 pulses, array of type Word16
    pOverflow, Flag set when overflow occurs, pointer of type Flag *

 Returns:

 Global Variables Used:
    None

 Local Variables Needed:
    None

------------------------------------------------------------------------------
 FUNCTION DESCRIPTION

 Builds the codeword, the filtered codeword and index of the
 codevector, based on the signs and positions of 2 pulses.

------------------------------------------------------------------------------
 REQUIREMENTS

 None

------------------------------------------------------------------------------
 REFERENCES

 c2_11pf.c, UMTS GSM AMR speech codec, R99 - Version 3.2.0, March 2, 2001

------------------------------------------------------------------------------
 PSEUDO-CODE

------------------------------------------------------------------------------
 CAUTION [optional]
 [State any special notes, constraints or cautions for users of this function]

------------------------------------------------------------------------------
*/
static Word16 build_code(
    Word16 codvec[],    /* i : position of pulses                            */
    Word16 dn_sign[],   /* i : sign of pulses                                */
    Word16 cod[],       /* o : innovative code vector                        */
    Word16 h[],         /* i : impulse response of weighted synthesis filter */
    Word16 y[],         /* o : filtered innovative code                      */
    Word16 sign[],      /* o : sign of 2 pulses                              */
    Flag   * pOverflow  /* o : Flag set when overflow occurs                 */
)
{
    Word16 i;
    Word16 j;
    Word16 k;
    Word16 track;
    Word16 index;
    Word16 _sign[NB_PULSE];
    Word16 indx;
    Word16 rsign;
    Word16 tempWord;

    Word16 *p0;
    Word16 *p1;

    Word32 s;

    for (i = 0; i < L_CODE; i++)
    {
        cod[i] = 0;
    }

    indx = 0;
    rsign = 0;

    for (k = 0; k < NB_PULSE; k++)
    {
        i = codvec[k];      /* read pulse position */
        j = dn_sign[i];     /* read sign           */

        /* index = pos/5 */
        /* index = mult(i, 6554, pOverflow); */
        index = (Word16)(((Word32) i * 6554) >> 15);

        /* track = pos%5 */
        /* tempWord =
            L_mult(
            index,
            5,
            pOverflow); */
        tempWord = (index << 3) + (index << 1);

        /* tempWord =
            L_shr(
            tempWord,
            1,
            pOverflow); */
        tempWord >>= 1;


        /* track =
            sub(
            i,
            tempWord,
            pOverflow); */
        track = i - tempWord;

        tempWord = track;

        if (tempWord == 0)
        {
            track = 1;

            /* index =
                shl(
                index,
                6,
                pOverflow); */
            index <<= 6;
        }
        else if (track == 1)
        {
            tempWord = k;

            if (tempWord == 0)
            {
                track = 0;
                /* index =
                    shl(
                    index,
                    1,
                    pOverflow); */
                index <<= 1;
            }
            else
            {
                track = 1;

                /* tempWord =
                    shl(
                    index,
                    6,
                    pOverflow); */
                tempWord = index << 6;

                /* index =
                    add(
                    tempWord,
                    16,
                    pOverflow); */
                index = tempWord + 16;
            }
        }
        else if (track == 2)
        {
            track = 1;

            /* tempWord =
                shl(
                index,
                6,
                pOverflow); */
            tempWord = index << 6;

            /* index =
                add(
                tempWord,
                32,
                pOverflow); */
            index = tempWord + 32;
        }
        else if (track == 3)
        {
            track = 0;

            /* tempWord =
                shl(
                index,
                1,
                pOverflow); */
            tempWord = index << 1;

            /* index =
                add(
                tempWord,
                1,
                pOverflow); */
            index = tempWord + 1;
        }
        else if (track == 4)
        {
            track = 1;

            /* tempWord =
                shl(
                index,
                6,
                pOverflow); */
            tempWord = index << 6;

            /* index =
                add(
                tempWord,
                48,
                pOverflow); */
            index = tempWord + 48;
        }

        if (j > 0)
        {
            cod[i] = 8191;
            _sign[k] = 32767;

            tempWord =
                shl(
                    1,
                    track,
                    pOverflow);

            rsign =
                add_16(
                    rsign,
                    tempWord,
                    pOverflow);
        }
        else
        {
            cod[i] = -8192;
            _sign[k] = (Word16) - 32768L;
        }

        indx =
            add_16(
                indx,
                index,
                pOverflow);
    }
    *sign = rsign;

    p0 = h - codvec[0];
    p1 = h - codvec[1];

    for (i = 0; i < L_CODE; i++)
    {
        s = 0;

        s =
            L_mac(
                s,
                *p0++,
                _sign[0],
                pOverflow);

        s =
            L_mac(
                s,
                *p1++,
                _sign[1],
                pOverflow);

        y[i] =
            pv_round(
                s,
                pOverflow);
    }

    return indx;
}


