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



 Filename: c2_9pf.cpp
 Functions: code_2i40_9bits
           search_2i40
           Test_search_2i40
           build_code
           Test_build_code

------------------------------------------------------------------------------
 MODULE DESCRIPTION

 This file contains the functions that search a 9 bit algebraic codebook
 containing 2 pulses in a frame of 40 samples.

------------------------------------------------------------------------------
*/

/*----------------------------------------------------------------------------
; INCLUDES
----------------------------------------------------------------------------*/
#include "c2_9pf.h"
#include "typedef.h"
#include "basic_op.h"
#include "inv_sqrt.h"
#include "cnst.h"
#include "cor_h.h"
#include "cor_h_x.h"
#include "set_sign.h"

/*--------------------------------------------------------------------------*/
#ifdef __cplusplus
extern "C"
{
#endif

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

    /*----------------------------------------------------------------------------
    ; LOCAL FUNCTION DEFINITIONS
    ; Function Prototype declaration
    ----------------------------------------------------------------------------*/
    static void search_2i40(
        Word16 subNr,       /* i : subframe number                               */
        Word16 dn[],        /* i : correlation between target and h[]            */
        Word16 rr[][L_CODE],/* i : matrix of autocorrelation                     */
        const Word16* startPos_ptr, /* i: ptr to read only table                 */
        Word16 codvec[],    /* o : algebraic codebook vector                     */
        Flag   * pOverflow  /* o : Flag set when overflow occurs                 */
    );

    static Word16 build_code(
        Word16 subNr,       /* i : subframe number                               */
        Word16 codvec[],    /* i : algebraic codebook vector                     */
        Word16 dn_sign[],   /* i : sign of dn[]                                  */
        Word16 cod[],       /* o : algebraic (fixed) codebook excitation         */
        Word16 h[],         /* i : impulse response of weighted synthesis filter */
        Word16 y[],         /* o : filtered fixed codebook excitation            */
        Word16 sign[],      /* o : sign of 2 pulses                              */
        Flag   * pOverflow  /* o : Flag set when overflow occurs                 */
    );

    /*----------------------------------------------------------------------------
    ; LOCAL VARIABLE DEFINITIONS
    ; Variable declaration - defined here and used outside this module
    ----------------------------------------------------------------------------*/

    const Word16 trackTable[4*5] =
    {
        0, 1, 0, 1, -1, /* subframe 1; track to code;
                         * -1 do not code this position
                         */
        0, -1, 1, 0, 1, /* subframe 2 */
        0, 1, 0, -1, 1, /* subframe 3 */
        0, 1, -1, 0, 1
    };/* subframe 4 */


    /*----------------------------------------------------------------------------
    ; EXTERNAL GLOBAL STORE/BUFFER/POINTER REFERENCES
    ; Declare variables used in this module but defined elsewhere
    ----------------------------------------------------------------------------*/

    /*
    ------------------------------------------------------------------------------
     FUNCTION NAME: code_2i40_9bits
    ------------------------------------------------------------------------------
     INPUT AND OUTPUT DEFINITIONS

     Inputs:
        subNr = subframe number (Word16)
        x  = target buffer (Word16)
        h  = buffer containing the impulse response of the
             weighted synthesis filter; h[-L_subfr .. -1] must be
             set to zero (Word16)
        T0 = pitch lag (Word16)
        pitch_sharp = last quantized pitch gain (Word16)
        code = buffer containing the innovative codebook (Word16)
        y = buffer containing the filtered fixed codebook excitation (Word16)
        sign = pointer to the signs of 2 pulses (Word16)

     Outputs:
        code buffer contains the new innovation vector gains

     Returns:
        index = code index (Word16)

     Global Variables Used:
        Overflow = overflow flag (Flag)

     Local Variables Needed:
        None

    ------------------------------------------------------------------------------
     FUNCTION DESCRIPTION

     This function searches a 9 bit algebraic codebook containing 2 pulses in a
     frame of 40 samples.

     The code length is 40, containing 2 nonzero pulses: i0...i1. All pulses can
     have two possible amplitudes: +1 or -1. Pulse i0 can have 8 possible positions,
     pulse i1 can have 8 positions. Also coded is which track pair should be used,
     i.e. first or second pair. Where each pair contains 2 tracks.

        First subframe:
        first   i0 :  0, 5, 10, 15, 20, 25, 30, 35.
            i1 :  2, 7, 12, 17, 22, 27, 32, 37.
        second  i0 :  1, 6, 11, 16, 21, 26, 31, 36.
                    i1 :  3, 8, 13, 18, 23, 28, 33, 38.

        Second subframe:
        first   i0 :  0, 5, 10, 15, 20, 25, 30, 35.
                    i1 :  3, 8, 13, 18, 23, 28, 33, 38.
        second  i0 :  2, 7, 12, 17, 22, 27, 32, 37.
                    i1 :  4, 9, 14, 19, 24, 29, 34, 39.

        Third subframe:
        first   i0 :  0, 5, 10, 15, 20, 25, 30, 35.
                    i1 :  2, 7, 12, 17, 22, 27, 32, 37.
        second  i0 :  1, 6, 11, 16, 21, 26, 31, 36.
                    i1 :  4, 9, 14, 19, 24, 29, 34, 39.

        Fourth subframe:
        first   i0 :  0, 5, 10, 15, 20, 25, 30, 35.
                    i1 :  3, 8, 13, 18, 23, 28, 33, 38.
        second  i0 :  1, 6, 11, 16, 21, 26, 31, 36.
                    i1 :  4, 9, 14, 19, 24, 29, 34, 39.

    ------------------------------------------------------------------------------
     REQUIREMENTS

     None

    ------------------------------------------------------------------------------
     REFERENCES

     [1] c2_9pf.c, UMTS GSM AMR speech codec, R99 - Version 3.2.0, March 2, 2001

    ------------------------------------------------------------------------------
     PSEUDO-CODE

    Word16 code_2i40_9bits(
        Word16 subNr,       // i : subframe number
        Word16 x[],         // i : target vector
        Word16 h[],         // i : impulse response of weighted synthesis filter
                            //     h[-L_subfr..-1] must be set to zero.
        Word16 T0,          // i : Pitch lag
        Word16 pitch_sharp, // i : Last quantized pitch gain
        Word16 code[],      // o : Innovative codebook
        Word16 y[],         // o : filtered fixed codebook excitation
        Word16 * sign       // o : Signs of 2 pulses
    )
    {
        Word16 codvec[NB_PULSE];
        Word16 dn[L_CODE], dn2[L_CODE], dn_sign[L_CODE];
        Word16 rr[L_CODE][L_CODE];
        Word16 i, index, sharp;

        sharp = shl(pitch_sharp, 1);
        if (sub(T0, L_CODE) < 0)
           for (i = T0; i < L_CODE; i++) {
              h[i] = add(h[i], mult(h[i - T0], sharp));
           }
        cor_h_x(h, x, dn, 1);
        set_sign(dn, dn_sign, dn2, 8); // dn2[] not used in this codebook search
        cor_h(h, dn_sign, rr);
        search_2i40(subNr, dn, rr, codvec);
        index = build_code(subNr, codvec, dn_sign, code, h, y, sign);

       *-----------------------------------------------------------------*
       * Compute innovation vector gain.                                 *
       * Include fixed-gain pitch contribution into code[].              *
       *-----------------------------------------------------------------*

        if (sub(T0, L_CODE) < 0)
           for (i = T0; i < L_CODE; i++) {
              code[i] = add(code[i], mult(code[i - T0], sharp));
           }
        return index;
    }

    ------------------------------------------------------------------------------
     CAUTION [optional]
     [State any special notes, constraints or cautions for users of this function]

    ------------------------------------------------------------------------------
    */

    Word16 code_2i40_9bits(
        Word16 subNr,       /* i : subframe number                          */
        Word16 x[],         /* i : target vector                            */
        Word16 h[],         /* i : impulse response of weighted synthesis   */
        /*     filter h[-L_subfr..-1] must be set to 0. */
        Word16 T0,          /* i : Pitch lag                                */
        Word16 pitch_sharp, /* i : Last quantized pitch gain                */
        Word16 code[],      /* o : Innovative codebook                      */
        Word16 y[],         /* o : filtered fixed codebook excitation       */
        Word16 * sign,      /* o : Signs of 2 pulses                        */
        const Word16* startPos_ptr, /* ptr to read-only table              */
        Flag   * pOverflow  /* o : Flag set when overflow occurs            */
    )
    {
        Word16 codvec[NB_PULSE];
        Word16 dn[L_CODE];
        Word16 dn2[L_CODE];
        Word16 dn_sign[L_CODE];
        Word16 rr[L_CODE][L_CODE];

        register Word16 i;

        Word16 index;
        Word16 sharp;
        Word16 temp;
        Word32 L_temp;

        L_temp = ((Word32) pitch_sharp) << 1;

        /* Check for overflow condition */
        if (L_temp != (Word32)((Word16) L_temp))
        {
            *(pOverflow) = 1;
            sharp = (pitch_sharp > 0) ? MAX_16 : MIN_16;
        }
        else
        {
            sharp = (Word16) L_temp;
        }

        if (T0 < L_CODE)
        {
            for (i = T0; i < L_CODE; i++)
            {
                temp =
                    mult(
                        *(h + i - T0),
                        sharp,
                        pOverflow);

                *(h + i) =
                    add_16(
                        *(h + i),
                        temp,
                        pOverflow);
            }
        }

        cor_h_x(
            h,
            x,
            dn,
            1,
            pOverflow);

        /* dn2[] not used in this codebook search */

        set_sign(
            dn,
            dn_sign,
            dn2,
            8);

        cor_h(
            h,
            dn_sign,
            rr,
            pOverflow);

        search_2i40(
            subNr,
            dn,
            rr,
            startPos_ptr,
            codvec,
            pOverflow);

        index =
            build_code(
                subNr,
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
                temp =
                    mult(
                        *(code + i - T0),
                        sharp,
                        pOverflow);

                *(code + i) =
                    add_16(
                        *(code + i),
                        temp,
                        pOverflow);
            }
        }

        return(index);
    }

    /****************************************************************************/


    /*
    ------------------------------------------------------------------------------
     FUNCTION NAME: search_2i40
    ------------------------------------------------------------------------------
     INPUT AND OUTPUT DEFINITIONS

     Inputs:
        subNr = subframe number (Word16)
        dn = vector containing the correlation between target and the impulse
             response of the weighted synthesis filter (Word16)
        rr = autocorrelation matrix (Word16)
        codvec = algebraic codebook vector (Word16)

     Outputs:
        codvec contains the newly calculated codevectors

     Returns:
        None

     Global Variables Used:
        None

     Local Variables Needed:
        startPos = table containing the start positions used by fixed codebook
                   routines (const Word16)

    ------------------------------------------------------------------------------
     FUNCTION DESCRIPTION

     This function searches the best codevector and determines the positions of
     the 2 pulses in the 40-sample frame.

    ------------------------------------------------------------------------------
     REQUIREMENTS

     None

    ------------------------------------------------------------------------------
     REFERENCES

     [1] c2_9pf.c, UMTS GSM AMR speech codec, R99 - Version 3.2.0, March 2, 2001

    ------------------------------------------------------------------------------
     PSEUDO-CODE

    static void search_2i40(
        Word16 subNr,        // i : subframe number
        Word16 dn[],         // i : correlation between target and h[]
        Word16 rr[][L_CODE], // i : matrix of autocorrelation
        Word16 codvec[]      // o : algebraic codebook vector
    )
    {
        Word16 i0, i1;
        Word16 ix = 0; // initialization only needed to keep gcc silent
        Word16  track1, ipos[NB_PULSE];
        Word16 psk, ps0, ps1, sq, sq1;
        Word16 alpk, alp, alp_16;
        Word32 s, alp0, alp1;
        Word16 i;

        psk = -1;
        alpk = 1;
        for (i = 0; i < NB_PULSE; i++)
        {
           codvec[i] = i;
        }

        for (track1 = 0; track1 < 2; track1++) {
           // fix starting position

           ipos[0] = startPos[subNr*2+8*track1];
           ipos[1] = startPos[subNr*2+1+8*track1];


               *----------------------------------------------------------------*
               * i0 loop: try 8 positions.                                      *
               *----------------------------------------------------------------*

              for (i0 = ipos[0]; i0 < L_CODE; i0 += STEP) {

                 ps0 = dn[i0];
                 alp0 = L_mult(rr[i0][i0], _1_4);

               *----------------------------------------------------------------*
               * i1 loop: 8 positions.                                          *
               *----------------------------------------------------------------*

                 sq = -1;
                 alp = 1;
                 ix = ipos[1];

            *-------------------------------------------------------------------*
            *  These index have low complexity address computation because      *
            *  they are, in fact, pointers with fixed increment.  For example,  *
            *  "rr[i0][i2]" is a pointer initialized to "&rr[i0][ipos[2]]"      *
            *  and incremented by "STEP".                                       *
            *-------------------------------------------------------------------*

                 for (i1 = ipos[1]; i1 < L_CODE; i1 += STEP) {
                    ps1 = add(ps0, dn[i1]);   // idx increment = STEP

                    // alp1 = alp0 + rr[i0][i1] + 1/2*rr[i1][i1];

                    alp1 = L_mac(alp0, rr[i1][i1], _1_4); // idx incr = STEP
                    alp1 = L_mac(alp1, rr[i0][i1], _1_2); // idx incr = STEP

                    sq1 = mult(ps1, ps1);

                    alp_16 = pv_round(alp1);

                    s = L_msu(L_mult(alp, sq1), sq, alp_16);

                    if (s > 0) {
                       sq = sq1;
                       alp = alp_16;
                       ix = i1;
                    }
                 }

               *----------------------------------------------------------------*
               * memorise codevector if this one is better than the last one.   *
               *----------------------------------------------------------------*

                 s = L_msu(L_mult(alpk, sq), psk, alp);

                 if (s > 0) {
                    psk = sq;
                    alpk = alp;
                    codvec[0] = i0;
                    codvec[1] = ix;
                 }
              }
        }

        return;
    }

    ------------------------------------------------------------------------------
     CAUTION [optional]
     [State any special notes, constraints or cautions for users of this function]

    ------------------------------------------------------------------------------
    */

    static void search_2i40(
        Word16 subNr,        /* i : subframe number                    */
        Word16 dn[],         /* i : correlation between target and h[] */
        Word16 rr[][L_CODE], /* i : matrix of autocorrelation          */
        const Word16* startPos_ptr, /* i: ptr to read only table       */
        Word16 codvec[],     /* o : algebraic codebook vector          */
        Flag   * pOverflow   /* o : Flag set when overflow occurs      */
    )
    {
        register Word16 i0;
        register Word16 i1;
        Word16 ix = 0; /* initialization only needed to keep gcc silent */
        register Word16  track1;
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
        register Word16 i;
        Word32 L_temp;
        Word16 *p_codvec = &codvec[0];

        OSCL_UNUSED_ARG(pOverflow);

        psk = -1;
        alpk = 1;

        /* Unrolled the following FOR loop to save MIPS */
        /* for (i = 0; i < NB_PULSE; i++)           */
        /* {                            */
        /*  *(codvec + i) = i;          */
        /* }                        */

        *(p_codvec++) = 0;
        *(p_codvec) = 1;

        for (track1 = 0; track1 < 2; track1++)
        {
            /* fix starting position */

            i = (subNr << 1) + (track1 << 3);
            *ipos = *(startPos_ptr + i);
            *(ipos + 1) = *(startPos_ptr + i + 1);


            /*----------------------------------------------------------*
             * i0 loop: try 8 positions.                                *
             *----------------------------------------------------------*/

            for (i0 = *ipos; i0 < L_CODE; i0 += STEP)
            {
                ps0 = *(dn + i0);

                /* Left shift by 1 converts integer product to */
                /* fractional product.                 */
                alp0 = (Word32) rr[i0][i0] << 14;

                /*--------------------------------------------------*
                 * i1 loop: 8 positions.                            *
                 *--------------------------------------------------*/

                sq = -1;
                alp = 1;
                ix = *(ipos + 1);

                /*--------------------------------------------------*
                 * These index have low complexity address          *
                 * computation because they are, in fact, pointers  *
                 * with fixed increment. For example, "rr[i0][i2]"  *
                 * is a pointer initialized to "&rr[i0][ipos[2]]"   *
                *  and incremented by "STEP".                       *
                *---------------------------------------------------*/

                for (i1 = *(ipos + 1); i1 < L_CODE; i1 += STEP)
                {
                    /* idx increment = STEP */
                    /* ps1 = add(ps0, *(dn + i1), pOverflow); */
                    ps1 = ps0 + dn[i1];

                    /* alp1 = alp0+rr[i0][i1]+1/2*rr[i1][i1]; */

                    /* idx incr = STEP */
                    /* Extra left shift by 1 converts integer  */
                    /* product to fractional product     */
                    /* alp1 = L_add(alp0, s, pOverflow); */
                    alp1 = alp0 + ((Word32) rr[i1][i1] << 14);

                    /* idx incr = STEP */
                    /* Extra left shift by 1 converts integer  */
                    /* product to fractional product     */
                    /* alp1 = L_add(alp1, s, pOverflow); */
                    alp1 += (Word32) rr[i0][i1] << 15;

                    /* sq1 = mult(ps1, ps1, pOverflow); */
                    sq1 = (Word16)(((Word32) ps1 * ps1) >> 15);

                    /* alp_16 = pv_round(alp1, pOverflow); */
                    alp_16 = (Word16)((alp1 + (Word32) 0x00008000L) >> 16);

                    /* L_temp = L_mult(alp, sq1, pOverflow); */
                    L_temp = ((Word32) alp * sq1) << 1;

                    /* s = L_msu(L_temp, sq, alp_16, pOverflow); */
                    s = L_temp - (((Word32) sq * alp_16) << 1);

                    if (s > 0)
                    {
                        sq = sq1;
                        alp = alp_16;
                        ix = i1;
                    }
                }

                /* memorize codevector if this one is better than the last one. */

                /* L_temp = L_mult(alpk, sq, pOverflow); */
                L_temp = ((Word32) alpk * sq) << 1;

                /* s = L_msu(L_temp, psk, alp, pOverflow); */
                s = L_temp - (((Word32) psk * alp) << 1);

                if (s > 0)
                {
                    psk = sq;
                    alpk = alp;
                    p_codvec = &codvec[0];
                    *(p_codvec++) = i0;
                    *(p_codvec) = ix;
                }
            }
        }

        return;
    }

    /****************************************************************************/

    /*
    ------------------------------------------------------------------------------
     FUNCTION NAME: Test_search_2i40
    ------------------------------------------------------------------------------
     INPUT AND OUTPUT DEFINITIONS

     Inputs:
        subNr = subframe number (Word16)
        dn = vector containing the correlation between target and the impulse
             response of the weighted synthesis filter (Word16)
        rr = autocorrelation matrix (Word16)
        codvec = algebraic codebook vector (Word16)

     Outputs:
        codvec contains the newly calculated codevectors

     Returns:
        None

     Global Variables Used:
        None

     Local Variables Needed:
        startPos = table containing the start positions used by fixed codebook
                   routines (const Word16)

    ------------------------------------------------------------------------------
     FUNCTION DESCRIPTION

     This function provides external access to the local function search_2i40.

    ------------------------------------------------------------------------------
     REQUIREMENTS

     None

    ------------------------------------------------------------------------------
     REFERENCES

     [1] c2_9pf.c, UMTS GSM AMR speech codec, R99 - Version 3.2.0, March 2, 2001

    ------------------------------------------------------------------------------
     PSEUDO-CODE

     CALL search_2i40 ( subNr = subNr
                dn = dn
                rr = rr
                codvec = codvec )
       MODIFYING(nothing)
       RETURNING(nothing)

    ------------------------------------------------------------------------------
     CAUTION [optional]
     [State any special notes, constraints or cautions for users of this function]

    ------------------------------------------------------------------------------
    */

    void Test_search_2i40(
        Word16 subNr,        /* i : subframe number                    */
        Word16 dn[],         /* i : correlation between target and h[] */
        Word16 rr[][L_CODE], /* i : matrix of autocorrelation          */
        const Word16* startPos_ptr, /* i : ptr to read-only table      */
        Word16 codvec[],     /* o : algebraic codebook vector          */
        Flag   * pOverflow   /* o : Flag set when overflow occurs      */
    )
    {
        /*----------------------------------------------------------------------------
         CALL search_2i40 ( subNr = subNr
                    dn = dn
                    rr = rr
                    codvec = codvec )
           MODIFYING(nothing)
           RETURNING(nothing)
        ----------------------------------------------------------------------------*/
        search_2i40(
            subNr,
            dn,
            rr,
            startPos_ptr,
            codvec,
            pOverflow);

        return;
    }

    /****************************************************************************/

    /*
    ------------------------------------------------------------------------------
     FUNCTION NAME: build_code
    ------------------------------------------------------------------------------
     INPUT AND OUTPUT DEFINITIONS

     Inputs:
        subNr = subframe number (Word16)
        codvec = vector containing the position of pulses (Word16)
        dn_sign = vector containing the sign of pulses (Word16)
        cod = innovative code vector (Word16)
        h = vector containing the impulse response of the weighted
            synthesis filter (Word16)
        y = vector containing the filtered innovative code (Word16)
        sign = vector containing the sign of 2 pulses (Word16)

     Outputs:
        cod vector contains the new innovative code
        y vector contains the new filtered innovative code
        sign vector contains the sign of 2 pulses

     Returns:
        indx = codebook index (Word16)

     Global Variables Used:
        None

     Local Variables Needed:
        trackTable = table used for tracking codewords (Word16)

    ------------------------------------------------------------------------------
     FUNCTION DESCRIPTION

     This function builds the codeword, the filtered codeword and index of the
     codevector, based on the signs and positions of 2 pulses.

    ------------------------------------------------------------------------------
     REQUIREMENTS

     None

    ------------------------------------------------------------------------------
     REFERENCES

     [1] c2_9pf.c, UMTS GSM AMR speech codec, R99 - Version 3.2.0, March 2, 2001

    ------------------------------------------------------------------------------
     PSEUDO-CODE

    static Word16 build_code(
        Word16 subNr,     // i : subframe number
        Word16 codvec[],  // i : position of pulses
        Word16 dn_sign[], // i : sign of pulses
        Word16 cod[],     // o : innovative code vector
        Word16 h[],       // i : impulse response of weighted synthesis filter
        Word16 y[],       // o : filtered innovative code
        Word16 sign[]     // o : sign of 2 pulses
    )
    {
        Word16 i, j, k, track, first, index, _sign[NB_PULSE], indx, rsign;
        Word16 *p0, *p1, *pt;
        Word32 s;
        static Word16 trackTable[4*5] = {
           0, 1, 0, 1, -1, // subframe 1; track to code; -1 do not code this position
           0, -1, 1, 0, 1, // subframe 2
           0, 1, 0, -1, 1, // subframe 3
           0, 1, -1, 0, 1};// subframe 4

        pt = &trackTable[add(subNr, shl(subNr, 2))];

        for (i = 0; i < L_CODE; i++) {
            cod[i] = 0;
        }

        indx = 0;
        rsign = 0;
        for (k = 0; k < NB_PULSE; k++) {
           i = codvec[k];    // read pulse position
           j = dn_sign[i];   // read sign

           index = mult(i, 6554);    // index = pos/5
                                     // track = pos%5
           track = sub(i, extract_l(L_shr(L_mult(index, 5), 1)));

           first = pt[track];

           if (first == 0) {
              if (k == 0) {
                 track = 0;
              } else {
                 track = 1;
                 index = shl(index, 3);
              }
           } else {
              if (k == 0) {
                 track = 0;
                 index = add(index, 64);  // table bit is MSB
              } else {
                 track = 1;
                 index = shl(index, 3);
              }
           }

           if (j > 0) {
              cod[i] = 8191;
              _sign[k] = 32767;
              rsign = add(rsign, shl(1, track));
           } else {
              cod[i] = -8192;
              _sign[k] = (Word16) - 32768L;
            }

           indx = add(indx, index);
        }
        *sign = rsign;

        p0 = h - codvec[0];
        p1 = h - codvec[1];

        for (i = 0; i < L_CODE; i++) {
           s = 0;
           s = L_mac(s, *p0++, _sign[0]);
           s = L_mac(s, *p1++, _sign[1]);
           y[i] = pv_round(s);
        }

        return indx;
    }

    ------------------------------------------------------------------------------
     CAUTION [optional]
     [State any special notes, constraints or cautions for users of this function]

    ------------------------------------------------------------------------------
    */

    static Word16 build_code(
        Word16 subNr,     /* i : subframe number                            */
        Word16 codvec[],  /* i : position of pulses                         */
        Word16 dn_sign[], /* i : sign of pulses                             */
        Word16 cod[],     /* o : innovative code vector                     */
        Word16 h[],       /* i : impulse response of weighted synthesis     */
        /*     filter                                     */
        Word16 y[],       /* o : filtered innovative code                   */
        Word16 sign[],    /* o : sign of 2 pulses                           */
        Flag  *pOverflow  /* o : Flag set when overflow occurs              */
    )
    {
        register Word16 i;
        register Word16 j;
        register Word16 k;
        register Word16 track;
        register Word16 first;
        register Word16 index;
        register Word16 rsign;
        Word16 indx;
        Word16 _sign[NB_PULSE];
        Word16 *p0;
        Word16 *p1;

        const Word16 *pt;

        Word32 s;

        pt = trackTable + subNr + (subNr << 2);

        for (i = 0; i < L_CODE; i++)
        {
            *(cod + i) = 0;
        }

        indx = 0;
        rsign = 0;

        for (k = 0; k < NB_PULSE; k++)
        {
            i = *(codvec + k);  /* read pulse position */
            j = *(dn_sign + i); /* read sign           */

            s = ((Word32)(i * 6554)) >> 15;
            index = (Word16) s; /* index = pos/5 */

            track = i - (5 * index);    /* track = pos%5 */

            first = *(pt + track);


            if (k == 0)
            {
                track = 0;

                if (first != 0)
                {
                    index += 64;  /* table bit is MSB */
                }
            }
            else
            {
                track = 1;
                index <<= 3;
            }

            if (j > 0)
            {
                *(cod + i) = 8191;
                *(_sign + k) = 32767;
                rsign += (1 << track);
            }
            else
            {
                *(cod + i) = ~(8192) + 1;
                *(_sign + k) = (Word16)(~(32768) + 1);
            }

            indx += index;
        }

        *sign = rsign;

        p0 = h - *codvec;
        p1 = h - *(codvec + 1);

        for (i = 0; i < L_CODE; i++)
        {
            s = 0;
            s =
                L_mult(
                    *p0++,
                    *_sign,
                    pOverflow);

            s =
                L_mac(
                    s,
                    *p1++,
                    *(_sign + 1),
                    pOverflow);

            *(y + i) =
                pv_round(
                    s,
                    pOverflow);
        }

        return(indx);
    }

    /****************************************************************************/

    /*
    ------------------------------------------------------------------------------
     FUNCTION NAME: Test_build_code
    ------------------------------------------------------------------------------
     INPUT AND OUTPUT DEFINITIONS

     Inputs:
        subNr = subframe number (Word16)
        codvec = vector containing the position of pulses (Word16)
        dn_sign = vector containing the sign of pulses (Word16)
        cod = innovative code vector (Word16)
        h = vector containing the impulse response of the weighted
            synthesis filter (Word16)
        y = vector containing the filtered innovative code (Word16)
        sign = vector containing the sign of 2 pulses (Word16)

     Outputs:
        cod vector contains the new innovative code
        y vector contains the new filtered innovative code
        sign vector contains the sign of 2 pulses

     Returns:
        indx = codebook index (Word16)

     Global Variables Used:
        None

     Local Variables Needed:
        trackTable = table used for tracking codewords (Word16)

    ------------------------------------------------------------------------------
     FUNCTION DESCRIPTION

     This function provides external access to the local function build_code.

    ------------------------------------------------------------------------------
     REQUIREMENTS

     None

    ------------------------------------------------------------------------------
     REFERENCES

     [1] c2_9pf.c, UMTS GSM AMR speech codec, R99 - Version 3.2.0, March 2, 2001

    ------------------------------------------------------------------------------
     PSEUDO-CODE

     CALL build_code ( subNr = subNr
               codvec = codvec
               dn_sign = dn_sign
               cod = cod
               h = h
               y = y
               sign = sign )
       MODIFYING(nothing)
       RETURNING(indx)

    ------------------------------------------------------------------------------
     CAUTION [optional]
     [State any special notes, constraints or cautions for users of this function]

    ------------------------------------------------------------------------------
    */

    Word16 Test_build_code(
        Word16 subNr,      /* i : subframe number                            */
        Word16 codvec[],   /* i : position of pulses                         */
        Word16 dn_sign[],  /* i : sign of pulses                             */
        Word16 cod[],      /* o : innovative code vector                     */
        Word16 h[],        /* i : impulse response of weighted synthesis     */
        /*     filter                                     */
        Word16 y[],        /* o : filtered innovative code                   */
        Word16 sign[],     /* o : sign of 2 pulses                           */
        Flag   * pOverflow /* o : Flag set when overflow occurs              */
    )
    {
        Word16  test_index;

        /*----------------------------------------------------------------------------
         CALL build_code ( subNr = subNr
                   codvec = codvec
                   dn_sign = dn_sign
                   cod = cod
                   h = h
                   y = y
                   sign = sign )
           MODIFYING(nothing)
           RETURNING(indx)
        ----------------------------------------------------------------------------*/
        test_index =
            build_code(
                subNr,
                codvec,
                dn_sign,
                cod,
                h,
                y,
                sign,
                pOverflow);

        return(test_index);
    }

#ifdef __cplusplus
}
#endif
