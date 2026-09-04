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



 Filename: c3_14pf.cpp

------------------------------------------------------------------------------
 MODULE DESCRIPTION

------------------------------------------------------------------------------
*/

/*----------------------------------------------------------------------------
; INCLUDES
----------------------------------------------------------------------------*/
#include "c3_14pf.h"
#include "typedef.h"
#include "inv_sqrt.h"
#include "cnst.h"
#include "cor_h.h"
#include "set_sign.h"
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

#define NB_PULSE  3

/*----------------------------------------------------------------------------
; LOCAL FUNCTION DEFINITIONS
; Function Prototype declaration
----------------------------------------------------------------------------*/
static void search_3i40(
    Word16 dn[],        /* i : correlation between target and h[]            */
    Word16 dn2[],       /* i : maximum of corr. in each track.               */
    Word16 rr[][L_CODE],/* i : matrix of autocorrelation                     */
    Word16 codvec[],    /* o : algebraic codebook vector                     */
    Flag   * pOverflow  /* o : Flag set when overflow occurs                 */
);

static Word16 build_code(
    Word16 codvec[],    /* i : algebraic codebook vector                     */
    Word16 dn_sign[],   /* i : sign of dn[]                                  */
    Word16 cod[],       /* o : algebraic (fixed) codebook excitation         */
    Word16 h[],         /* i : impulse response of weighted synthesis filter */
    Word16 y[],         /* o : filtered fixed codebook excitation            */
    Word16 sign[],      /* o : sign of 3 pulses                              */
    Flag   * pOverflow  /* o : Flag set when overflow occurs                 */
);

/*----------------------------------------------------------------------------
; LOCAL VARIABLE DEFINITIONS
; Variable declaration - defined here and used outside this module
----------------------------------------------------------------------------*/

/*----------------------------------------------------------------------------
; EXTERNAL GLOBAL STORE/BUFFER/POINTER REFERENCES
; Declare variables used in this module but defined elsewhere
----------------------------------------------------------------------------*/

/*
------------------------------------------------------------------------------
 FUNCTION NAME: code_3i40_14bits
------------------------------------------------------------------------------
 INPUT AND OUTPUT DEFINITIONS

 Inputs:
    x[]   Array of type Word16 -- target vector
    h[]   Array of type Word16 -- impulse response of weighted synthesis filter
                                  h[-L_subfr..-1] must be set to zero.

    T0           Array of type Word16 -- Pitch lag
    pitch_sharp, Array of type Word16 --  Last quantized pitch gain

 Outputs:
    code[]  Array of type Word16 -- Innovative codebook
    y[]     Array of type Word16 -- filtered fixed codebook excitation
    * sign  Pointer of type Word16 -- Pointer to the signs of 3 pulses
    pOverflow    Pointer to Flag      -- set when overflow occurs

 Returns:
    index

 Global Variables Used:
    None

 Local Variables Needed:
    None

------------------------------------------------------------------------------
 FUNCTION DESCRIPTION

 PURPOSE:  Searches a 14 bit algebraic codebook containing 3 pulses
           in a frame of 40 samples.

 DESCRIPTION:
    The code length is 40, containing 3 nonzero pulses: i0...i2.
    All pulses can have two possible amplitudes: +1 or -1.
    Pulse i0 can have 8 possible positions, pulses i1 and i2 can have
    2x8=16 positions.

        i0 :  0, 5, 10, 15, 20, 25, 30, 35.
        i1 :  1, 6, 11, 16, 21, 26, 31, 36.
              3, 8, 13, 18, 23, 28, 33, 38.
        i2 :  2, 7, 12, 17, 22, 27, 32, 37.
              4, 9, 14, 19, 24, 29, 34, 39.

------------------------------------------------------------------------------
 REQUIREMENTS

 None

------------------------------------------------------------------------------
 REFERENCES

 [1] c3_14pf.c, UMTS GSM AMR speech codec, R99 - Version 3.2.0, March 2, 2001

------------------------------------------------------------------------------
 PSEUDO-CODE

------------------------------------------------------------------------------
 CAUTION [optional]
 [State any special notes, constraints or cautions for users of this function]

------------------------------------------------------------------------------
*/

Word16 code_3i40_14bits(
    Word16 x[],         /* i : target vector                                 */
    Word16 h[],         /* i : impulse response of weighted synthesis filter */
    /*     h[-L_subfr..-1] must be set to zero.          */
    Word16 T0,          /* i : Pitch lag                                     */
    Word16 pitch_sharp, /* i : Last quantized pitch gain                     */
    Word16 code[],      /* o : Innovative codebook                           */
    Word16 y[],         /* o : filtered fixed codebook excitation            */
    Word16 * sign,      /* o : Signs of 3 pulses                             */
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

    /* sharp = shl(pitch_sharp, 1, pOverflow); */
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
        6);

    cor_h(
        h,
        dn_sign,
        rr,
        pOverflow);

    search_3i40(
        dn,
        dn2,
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

    /*-----------------------------------------------------------------*
    * Compute innovation vector gain.                                 *
    * Include fixed-gain pitch contribution into code[].              *
    *-----------------------------------------------------------------*/

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
 FUNCTION NAME: search_3i40
------------------------------------------------------------------------------
 INPUT AND OUTPUT DEFINITIONS

 Inputs:
    dn[]         Array of type Word16 -- correlation between target and h[]
    dn2[]        Array of type Word16 -- maximum of corr. in each track.
    rr[][L_CODE] Double Array of type Word16 -- autocorrelation matrix

 Outputs:
    codvec[]     Array of type Word16 -- algebraic codebook vector
    pOverflow    Pointer to Flag      -- set when overflow occurs

 Returns:
    None

 Global Variables Used:
    None

 Local Variables Needed:
    None

------------------------------------------------------------------------------
 FUNCTION DESCRIPTION

 PURPOSE: Search the best codevector; determine positions of the 3 pulses
          in the 40-sample frame.
------------------------------------------------------------------------------
 REQUIREMENTS

 None

------------------------------------------------------------------------------
 REFERENCES

 [1] c3_14pf.c, UMTS GSM AMR speech codec, R99 - Version 3.2.0, March 2, 2001

------------------------------------------------------------------------------
 PSEUDO-CODE

------------------------------------------------------------------------------
 CAUTION [optional]
 [State any special notes, constraints or cautions for users of this function]

------------------------------------------------------------------------------
*/
static void search_3i40(
    Word16 dn[],         /* i : correlation between target and h[] */
    Word16 dn2[],        /* i : maximum of corr. in each track.    */
    Word16 rr[][L_CODE], /* i : matrix of autocorrelation          */
    Word16 codvec[],     /* o : algebraic codebook vector          */
    Flag   * pOverflow   /* o : Flag set when overflow occurs      */
)
{
    Word16 i0;
    Word16 i1;
    Word16 i2;

    Word16 ix = 0; /* initialization only needed to keep gcc silent */
    Word16 ps = 0; /* initialization only needed to keep gcc silent */

    Word16 i;
    Word16 pos;
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

    Word16 *p_codvec = &codvec[0];

    Word32 s;
    Word32 alp0;
    Word32 alp1;

    psk = -1;
    alpk = 1;

    for (i = 0; i < NB_PULSE; i++)
    {
        *(p_codvec++) = i;
    }

    for (track1 = 1; track1 < 4; track1 += 2)
    {
        for (track2 = 2; track2 < 5; track2 += 2)
        {
            /* fix starting position */

            ipos[0] = 0;
            ipos[1] = track1;
            ipos[2] = track2;

            /*------------------------------------------------------------------*
             * main loop: try 3 tracks.                                         *
             *------------------------------------------------------------------*/

            for (i = 0; i < NB_PULSE; i++)
            {
                /*----------------------------------------------------------------*
                 * i0 loop: try 8 positions.                                      *
                 *----------------------------------------------------------------*/

                /* account for ptr. init. (rr[io]) */
                for (i0 = ipos[0]; i0 < L_CODE; i0 += STEP)
                {
                    if (dn2[i0] >= 0)
                    {
                        ps0 = dn[i0];

                        /* alp0 = L_mult(rr[i0][i0],_1_4, pOverflow); */
                        alp0 = (Word32) rr[i0][i0] << 14;

                        /*----------------------------------------------------------------*
                         * i1 loop: 8 positions.                                          *
                         *----------------------------------------------------------------*/

                        sq = -1;
                        alp = 1;
                        ps = 0;
                        ix = ipos[1];

                        /* initialize 4 index for next loop. */
                        /*-------------------------------------------------------------------*
                         *  These index have low complexity address computation because      *
                         *  they are, in fact, pointers with fixed increment.  For example,  *
                         *  "rr[i0][i2]" is a pointer initialized to "&rr[i0][ipos[2]]"      *
                         *  and incremented by "STEP".                                       *
                         *-------------------------------------------------------------------*/

                        for (i1 = ipos[1]; i1 < L_CODE; i1 += STEP)
                        {
                            /* idx increment = STEP */
                            /* ps1 = add(ps0, dn[i1], pOverflow); */
                            ps1 = ps0 + dn[i1];

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

                            /* s = L_msu(s, sq, alp_16, pOverflow); */
                            s -= (((Word32) sq * alp_16) << 1);

                            if (s > 0)
                            {
                                sq = sq1;
                                ps = ps1;
                                alp = alp_16;
                                ix = i1;
                            }
                        }
                        i1 = ix;

                        /*----------------------------------------------------------------*
                         * i2 loop: 8 positions.                                          *
                         *----------------------------------------------------------------*/

                        ps0 = ps;

                        /* alp0 = L_mult(alp, _1_4, pOverflow); */
                        alp0 = (Word32) alp << 14;

                        sq = -1;
                        alp = 1;
                        ps = 0;
                        ix = ipos[2];

                        /* initialize 4 index for next loop (see i1 loop) */

                        for (i2 = ipos[2]; i2 < L_CODE; i2 += STEP)
                        {
                            /* index increment = STEP */
                            /* ps1 = add(ps0, dn[i2], pOverflow); */
                            ps1 = ps0 + dn[i2];

                            /* alp1 = alp0 + rr[i0][i2] + rr[i1][i2] + 1/2*rr[i2][i2]; */

                            /* idx incr = STEP */
                            /* alp1 = L_mac(alp0, rr[i2][i2], _1_16, pOverflow); */
                            alp1 = alp0 + ((Word32) rr[i2][i2] << 12);

                            /* idx incr = STEP */
                            /* alp1 = L_mac(alp1, rr[i1][i2], _1_8, pOverflow); */
                            alp1 += (Word32) rr[i1][i2] << 13;

                            /* idx incr = STEP */
                            /* alp1 = L_mac(alp1,rr[i0][i2], _1_8, pOverflow); */
                            alp1 += (Word32) rr[i0][i2] << 13;

                            /* sq1 = mult(ps1, ps1, pOverflow); */
                            sq1 = (Word16)(((Word32) ps1 * ps1) >> 15);

                            /* alp_16 = pv_round(alp1, pOverflow); */
                            alp_16 = (Word16)((alp1 + (Word32) 0x00008000L) >> 16);

                            /* s = L_mult(alp, sq1, pOverflow); */
                            s = ((Word32) alp * sq1) << 1;

                            /* s = L_msu(s, sq, alp_16, pOverflow); */
                            s -= (((Word32) sq * alp_16) << 1);

                            if (s > 0)
                            {
                                sq = sq1;
                                ps = ps1;
                                alp = alp_16;
                                ix = i2;
                            }
                        }
                        i2 = ix;

                        /* memorize codevector if this one
                         * is better than the last one.
                         */

                        s = L_mult(alpk, sq, pOverflow);
                        //s = ((Word32) alpk * sq) << 1;

                        s = L_msu(s, psk, alp, pOverflow);
                        //s -= (((Word32) psk * alp) << 1);

                        if (s > 0)
                        {
                            psk = sq;
                            alpk = alp;
                            p_codvec = &codvec[0];

                            *(p_codvec++) = i0;
                            *(p_codvec++) = i1;
                            *(p_codvec) = i2;
                        }
                    }
                }
                /*----------------------------------------------------------------*
                 * Cyclic permutation of i0, i1 and i2.                           *
                 *----------------------------------------------------------------*/

                pos = ipos[2];
                ipos[2] = ipos[1];
                ipos[1] = ipos[0];
                ipos[0] = pos;
            }
        }
    }
    return;
}

/****************************************************************************/

/*
------------------------------------------------------------------------------
 FUNCTION NAME:  build_code()
------------------------------------------------------------------------------
 INPUT AND OUTPUT DEFINITIONS

 Inputs:
    codvec[]   Array of type Word16 -- position of pulses
    dn_sign[]  Array of type Word16 -- sign of pulses
    h[]        Array of type Word16 -- impulse response of
                                       weighted synthesis filter

 Outputs:
    cod[]  Array of type Word16 -- innovative code vector
    y[]    Array of type Word16 -- filtered innovative code
    sign[] Array of type Word16 -- sign of 3 pulses
    pOverflow  Pointer to Flag  -- set when overflow occurs

 Returns:
    indx

 Global Variables Used:
    None

 Local Variables Needed:
    None

------------------------------------------------------------------------------
 FUNCTION DESCRIPTION

 PURPOSE: Builds the codeword, the filtered codeword and index of the
          codevector, based on the signs and positions of 3 pulses.

------------------------------------------------------------------------------
 REQUIREMENTS

 None

------------------------------------------------------------------------------
 REFERENCES

 [1] c3_14pf.c, UMTS GSM AMR speech codec, R99 - Version 3.2.0, March 2, 2001

------------------------------------------------------------------------------
 PSEUDO-CODE

------------------------------------------------------------------------------
 CAUTION [optional]
 [State any special notes, constraints or cautions for users of this function]

------------------------------------------------------------------------------
*/

static Word16
build_code(
    Word16 codvec[],  /* i : position of pulses                            */
    Word16 dn_sign[], /* i : sign of pulses                                */
    Word16 cod[],     /* o : innovative code vector                        */
    Word16 h[],       /* i : impulse response of weighted synthesis filter */
    Word16 y[],       /* o : filtered innovative code                      */
    Word16 sign[],    /* o : sign of 3 pulses                              */
    Flag  *pOverflow  /* o : Flag set when overflow occurs                 */
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

    Word16 *p0;
    Word16 *p1;
    Word16 *p2;

    Word32 s;

    for (i = 0; i < L_CODE; i++)
    {
        cod[i] = 0;
    }

    indx = 0;
    rsign = 0;

    for (k = 0; k < NB_PULSE; k++)
    {
        i = codvec[k];  /* read pulse position */
        j = dn_sign[i];  /* read sign           */

        /* index = pos/5 */
        /* index = mult(i, 6554, pOverflow); */
        index = (Word16)(((Word32) i * 6554) >> 15);

        /* track = pos%5 */
        /* s = L_mult(index, 5, pOverflow); */
        s = ((Word32) index * 5) << 1;

        /* s = L_shr(s, 1, pOverflow); */
        s >>= 1;

        /* track = sub(i, (Word16) s, pOverflow); */
        track = i - (Word16) s;

        if (track == 1)
        {
            /* index = shl(index, 4, pOverflow); */
            index <<= 4;
        }
        else if (track == 2)
        {
            track = 2;

            /* index = shl(index, 8, pOverflow); */
            index <<= 8;
        }
        else if (track == 3)
        {
            track = 1;

            /* index = shl(index, 4, pOverflow); */
            index <<= 4;

            /* index = add(index, 8, pOverflow); */
            index += 8;
        }
        else if (track == 4)
        {
            track = 2;

            /* index = shl(index, 8, pOverflow); */
            index <<= 8;

            /* index = add(index, 128, pOverflow); */
            index += 128;
        }

        if (j > 0)
        {
            cod[i] = 8191;
            _sign[k] = 32767;

            /* track = shl(1, track, pOverflow); */
            track = 1 << track;

            /* rsign = add(rsign, track, pOverflow); */
            rsign += track;
        }
        else
        {
            cod[i] = -8192;
            _sign[k] = (Word16) - 32768L;
        }

        /* indx = add(indx, index, pOverflow); */
        indx += index;
    }
    *sign = rsign;

    p0 = h - codvec[0];
    p1 = h - codvec[1];
    p2 = h - codvec[2];

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

        s =
            L_mac(
                s,
                *p2++,
                _sign[2],
                pOverflow);

        y[i] =
            pv_round(
                s,
                pOverflow);
    }

    return indx;
}







