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



 Filename: c8_31pf.cpp

------------------------------------------------------------------------------
 MODULE DESCRIPTION

 Purpose          : Searches a 31 bit algebraic codebook containing
                  : 8 pulses in a frame of 40 samples.
                  : in the same manner as GSM-EFR
------------------------------------------------------------------------------
*/

/*----------------------------------------------------------------------------
; INCLUDES
----------------------------------------------------------------------------*/
#include "c8_31pf.h"
#include "typedef.h"
#include "cnst.h"
#include "inv_sqrt.h"
#include "cor_h.h"
#include "cor_h_x2.h"
#include "set_sign.h"
#include "s10_8pf.h"
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
#define NB_PULSE 8

/* define values/representation for output codevector and sign */
#define POS_CODE  8191
#define NEG_CODE  8191
#define POS_SIGN  32767
#define NEG_SIGN  (Word16) (-32768L)

/*----------------------------------------------------------------------------
; LOCAL FUNCTION DEFINITIONS
; Function Prototype declaration
----------------------------------------------------------------------------*/

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
 FUNCTION NAME:
------------------------------------------------------------------------------
 INPUT AND OUTPUT DEFINITIONS

 Inputs:
    codvec[]   Array of type Word16 -- position of pulses
    sign[]     Array of type Word16 -- sign of pulses
    h[]        Array of type Word16 -- impulse response of
                                       weighted synthesis filter
 Outputs:
    cod[]       Array of type Word16 -- innovative code vector
    y[]         Array of type Word16 -- filtered innovative code
    sign_indx[] Array of type Word16 -- signs of 4 pulses (signs only)
    pos_indx[]  Array of type Word16 --
                             position index of 8 pulses(position only)

    pOverflow  Pointer to Flag  -- set when overflow occurs

 Returns:
    indx

 Global Variables Used:
    None

 Local Variables Needed:

------------------------------------------------------------------------------
 FUNCTION DESCRIPTION


------------------------------------------------------------------------------
 REQUIREMENTS

 None

------------------------------------------------------------------------------
 REFERENCES

 [1] c8_31pf.c, UMTS GSM AMR speech codec, R99 - Version 3.2.0, March 2, 2001

------------------------------------------------------------------------------
 PSEUDO-CODE

------------------------------------------------------------------------------
 CAUTION [optional]
 [State any special notes, constraints or cautions for users of this function]

------------------------------------------------------------------------------
*/

/*************************************************************************
 *
 *  FUNCTION:  build_code()
 *
 *  PURPOSE: Builds the codeword, the filtered codeword and a
 *   linear uncombined version of  the index of the
 *           codevector, based on the signs and positions of 8  pulses.
 *
 *************************************************************************/

static void build_code(
    Word16 codvec[],    /* i : position of pulses                           */
    Word16 sign[],      /* i : sign of d[n]                                 */
    Word16 cod[],       /* o : innovative code vector                       */
    Word16 h[],         /* i : impulse response of weighted synthesis filter*/
    Word16 y[],         /* o : filtered innovative code                     */
    Word16 sign_indx[], /* o : signs of 4  pulses (signs only)              */
    Word16 pos_indx[],  /* o : position index of 8 pulses(position only)    */
    Flag   * pOverflow  /* o : Flag set when overflow occurs                */
)
{
    Word16 i;
    Word16 j;
    Word16 k;
    Word16 track;
    Word16 sign_index;
    Word16 pos_index;
    Word16 _sign[NB_PULSE];

    Word16 *p0;
    Word16 *p1;
    Word16 *p2;
    Word16 *p3;
    Word16 *p4;
    Word16 *p5;
    Word16 *p6;
    Word16 *p7;

    Word16 *p_cod = &cod[0];
    Word16 *p_codvec = &codvec[0];

    Word32 s;

    for (i = 0; i < L_CODE; i++)
    {
        *(p_cod++) = 0;
    }

    for (i = 0; i < NB_TRACK_MR102; i++)
    {
        pos_indx[i] = -1;
        sign_indx[i] = -1;
    }

    for (k = 0; k < NB_PULSE; k++)
    {
        /* read pulse position */
        i = codvec[k];
        /* read sign           */
        j = sign[i];

        pos_index = i >> 2; /* index = pos/4 */

        track = i & 3;     /* track = pos%4 */

        if (j > 0)
        {
            cod[i] = (Word16)((Word32) cod[i] + POS_CODE);

            _sign[k] = POS_SIGN;
            sign_index = 0;  /* bit=0 -> positive pulse */
        }
        else
        {
            cod[i] = (Word16)((Word32) cod[i] - NEG_CODE);

            _sign[k] = NEG_SIGN;
            sign_index = 1; /* bit=1 => negative pulse */
            /* index = add (index, 8); 1 = negative  old code */
        }

        if (pos_indx[track] < 0)
        {   /* first set first NB_TRACK pulses  */
            pos_indx[track] = pos_index;
            sign_indx[track] = sign_index;
        }
        else
        {   /* 2nd row of pulses , test if positions needs to be switched */
            if (((sign_index ^ sign_indx[track]) & 1) == 0)
            {
                /* sign of 1st pulse == sign of 2nd pulse */

                if (pos_indx[track] <= pos_index)
                {   /* no swap */
                    pos_indx[track + NB_TRACK_MR102] = pos_index;
                }
                else
                {   /* swap*/
                    pos_indx[track + NB_TRACK_MR102] = pos_indx[track];

                    pos_indx[track] = pos_index;
                    sign_indx[track] = sign_index;
                }
            }
            else
            {
                /* sign of 1st pulse != sign of 2nd pulse */

                if (pos_indx[track] <= pos_index)
                {  /*swap*/
                    pos_indx[track + NB_TRACK_MR102] = pos_indx[track];

                    pos_indx[track] = pos_index;
                    sign_indx[track] = sign_index;
                }
                else
                {   /*no swap */
                    pos_indx[track + NB_TRACK_MR102] = pos_index;
                }
            }
        }
    }

    p0 = h - *(p_codvec++);
    p1 = h - *(p_codvec++);
    p2 = h - *(p_codvec++);
    p3 = h - *(p_codvec++);
    p4 = h - *(p_codvec++);
    p5 = h - *(p_codvec++);
    p6 = h - *(p_codvec++);
    p7 = h - *(p_codvec);

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
        s =
            L_mac(
                s,
                *p3++,
                _sign[3],
                pOverflow);
        s =
            L_mac(
                s,
                *p4++,
                _sign[4],
                pOverflow);
        s =
            L_mac(
                s,
                *p5++,
                _sign[5],
                pOverflow);
        s =
            L_mac(
                s,
                *p6++,
                _sign[6],
                pOverflow);
        s =
            L_mac(
                s,
                *p7++,
                _sign[7],
                pOverflow);

        y[i] =
            pv_round(
                s,
                pOverflow);

    } /* for (i = 0; i < L_CODE; i++) */

} /* build_code */

/****************************************************************************/

/*
------------------------------------------------------------------------------
 FUNCTION NAME: compress_code()
------------------------------------------------------------------------------
 INPUT AND OUTPUT DEFINITIONS

 Inputs:

 Outputs:

 Returns:
    None

 Global Variables Used:
    None

 Local Variables Needed:

------------------------------------------------------------------------------
 FUNCTION DESCRIPTION

 FUNCTION:

 PURPOSE: compression of three indeces [0..9] to one 10 bit index
          minimizing the phase shift of a bit error.

------------------------------------------------------------------------------
 REQUIREMENTS

 None

------------------------------------------------------------------------------
 REFERENCES

 [1] c8_31pf.c, UMTS GSM AMR speech codec, R99 - Version 3.2.0, March 2, 2001

------------------------------------------------------------------------------
 PSEUDO-CODE

------------------------------------------------------------------------------
 CAUTION [optional]
 [State any special notes, constraints or cautions for users of this function]

------------------------------------------------------------------------------
*/

static Word16 compress10(
    Word16 pos_indxA,  /* i : signs of 4 pulses (signs only)             */
    Word16 pos_indxB,  /* i : position index of 8 pulses (pos only)      */
    Word16 pos_indxC,  /* i : position and sign of 8 pulses (compressed) */
    Flag  *pOverflow)  /* o : Flag set when overflow occurs              */
{
    Word16 indx;
    Word16 ia;
    Word16 ib;
    Word16 ic;

    Word32 tempWord32;

    OSCL_UNUSED_ARG(pOverflow);

    ia = pos_indxA >> 1;

    ib = pos_indxB >> 1;

    tempWord32 = ((Word32) ib * 5) << 1;

    tempWord32 = tempWord32 >> 1;

    ib = (Word16) tempWord32;

    ic = pos_indxC >> 1;

    tempWord32 = ((Word32) ic * 25) << 1;

    tempWord32 = tempWord32 >> 1;

    ic = (Word16) tempWord32;

    ib += ic;

    ib += ia;

    indx = ib << 3;

    ia = pos_indxA & 1;

    ib = ((Word16)(pos_indxB & 1)) << 1;

    ic = ((Word16)(pos_indxC & 1)) << 2;

    ib += ic;

    ib += ia;

    indx += ib;

    return indx;

}

/****************************************************************************/

/*
------------------------------------------------------------------------------
 FUNCTION NAME: compress_code()
------------------------------------------------------------------------------
 INPUT AND OUTPUT DEFINITIONS

 Inputs:
    sign_indx   Array of type Word16 -- signs of 4 pulses (signs only)
    pos_indx    Array of type Word16 -- position index of 8 pulses
                                            (position only)

 Outputs:
    indx         Array of type Word16 -- position and sign of 8 pulses
                                            (compressed)
    pOverflow    Pointer to Flag      -- set when overflow occurs

 Returns:
    None

 Global Variables Used:
    None

 Local Variables Needed:
    None

------------------------------------------------------------------------------
 FUNCTION DESCRIPTION

 PURPOSE: compression of the linear codewords to 4+three indeces
          one bit from each pulse is made robust to errors by
          minimizing the phase shift of a bit error.
          4 signs (one for each track)
          i0,i4,i1 => one index (7+3) bits, 3   LSBs more robust
          i2,i6,i5 => one index (7+3) bits, 3   LSBs more robust
          i3,i7    => one index (5+2) bits, 2-3 LSbs more robust

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

static void compress_code(
    Word16 sign_indx[], /* i : signs of 4 pulses (signs only)             */
    Word16 pos_indx[],  /* i : position index of 8 pulses (position only) */
    Word16 indx[],      /* o : position and sign of 8 pulses (compressed) */
    Flag  *pOverflow)   /* o : Flag set when overflow occurs              */
{
    Word16 i;
    Word16 ia;
    Word16 ib;
    Word16 ic;

    Word16 *p_indx = &indx[0];
    Word16 *p_sign_indx = &sign_indx[0];

    Word32 tempWord32;

    for (i = 0; i < NB_TRACK_MR102; i++)
    {
        *(p_indx++) = *(p_sign_indx++);
    }

    /* First index
      indx[NB_TRACK] = (ia/2+(ib/2)*5 +(ic/2)*25)*8 + ia%2 + (ib%2)*2 + (ic%2)*4; */

    indx[NB_TRACK_MR102] =
        compress10(
            pos_indx[0],
            pos_indx[4],
            pos_indx[1],
            pOverflow);

    /* Second index
      indx[NB_TRACK+1] = (ia/2+(ib/2)*5 +(ic/2)*25)*8 + ia%2 + (ib%2)*2 + (ic%2)*4; */

    indx[NB_TRACK_MR102+1] =
        compress10(
            pos_indx[2],
            pos_indx[6],
            pos_indx[5],
            pOverflow);

    /*
      Third index
      if ((ib/2)%2 == 1)
        indx[NB_TRACK+2] = ((((4-ia/2) + (ib/2)*5)*32+12)/25)*4 + ia%2 + (ib%2)*2;
      else
        indx[NB_TRACK+2] = ((((ia/2) +   (ib/2)*5)*32+12)/25)*4 + ia%2 + (ib%2)*2;
        */

    ib = pos_indx[7] >> 1;

    ib &= 1;

    ia = pos_indx[3] >> 1;

    if (ib == 1)
    {
        ia = 4 - ia;
    }

    ib = pos_indx[7] >> 1;

    tempWord32 = ((Word32) ib * 5) << 1;

    tempWord32 = tempWord32 >> 1;

    ib = (Word16) tempWord32;

    ib += ia;

    ib <<= 5;

    ib += 12;

    ic = (Word16)(((Word32) ib * 1311) >> 15);

    ic <<= 2;

    ia = pos_indx[3] & 1;

    ib = ((Word16)(pos_indx[7] & 1)) << 1;

    ib += ic;

    ib += ia;

    indx[NB_TRACK_MR102+2] = ib;

} /* compress_code */


/****************************************************************************/

/*
------------------------------------------------------------------------------
 FUNCTION NAME: code_8i40_31bits()
------------------------------------------------------------------------------
 INPUT AND OUTPUT DEFINITIONS

 Inputs:
    x   Array of type Word16 -- target vector
    cn  Array of type Word16 -- residual after long term prediction
    h   Array of type Word16 -- impulse response of weighted synthesis filter


 Outputs:
    cod Array of type Word16 -- algebraic (fixed) codebook excitation
    y   Array of type Word16 -- filtered fixed codebook excitation
    indx Array of type Word16 -- index of 8 pulses (signs+positions)
    pOverflow    Pointer to Flag      -- set when overflow occurs

 Returns:
    None

 Global Variables Used:
    None

 Local Variables Needed:
    None

------------------------------------------------------------------------------
 FUNCTION DESCRIPTION

 FUNCTION:

 PURPOSE:  Searches a 31 bit algebraic codebook containing 8 pulses
           in a frame of 40 samples.

 DESCRIPTION:
   The code contains 8 nonzero pulses: i0...i7.
   All pulses can have two possible amplitudes: +1 or -1.
   The 40 positions in a subframe are divided into 4 tracks of
   interleaved positions. Each track contains two pulses.
   The pulses can have the following possible positions:

      i0, i4 :  0, 4, 8,  12, 16, 20, 24, 28, 32, 36
      i1, i5 :  1, 5, 9,  13, 17, 21, 25, 29, 33, 37
      i2, i6 :  2, 6, 10, 14, 18, 22, 26, 30, 34, 38
      i3, i7 :  3, 7, 11, 15, 19, 23, 27, 31, 35, 39

   Each pair of pulses require 1 bit for their signs. The positions
   are encoded together 3,3 and 2 resulting in
   (7+3) + (7+3) + (5+2) bits for their
   positions. This results in a 31 (4 sign and 27 pos) bit codebook.
   The function determines the optimal pulse signs and positions, builds
   the codevector, and computes the filtered codevector.

------------------------------------------------------------------------------
 REQUIREMENTS

 None

------------------------------------------------------------------------------
 REFERENCES

 [1] c8_31pf.c, UMTS GSM AMR speech codec, R99 - Version 3.2.0, March 2, 2001

------------------------------------------------------------------------------
 PSEUDO-CODE

------------------------------------------------------------------------------
 CAUTION [optional]
 [State any special notes, constraints or cautions for users of this function]

------------------------------------------------------------------------------
*/
void code_8i40_31bits(
    Word16 x[],        /* i : target vector                                  */
    Word16 cn[],       /* i : residual after long term prediction            */
    Word16 h[],        /* i : impulse response of weighted synthesis
                             filter                                         */
    Word16 cod[],      /* o : algebraic (fixed) codebook excitation          */
    Word16 y[],        /* o : filtered fixed codebook excitation             */
    Word16 indx[],     /* o : 7 Word16, index of 8 pulses (signs+positions)  */
    Flag  *pOverflow   /* o : Flag set when overflow occurs                  */
)
{
    Word16 ipos[NB_PULSE];
    Word16 pos_max[NB_TRACK_MR102];
    Word16 codvec[NB_PULSE];

    Word16 dn[L_CODE];
    Word16 sign[L_CODE];

    Word16 rr[L_CODE][L_CODE];
    Word16 linear_signs[NB_TRACK_MR102];
    Word16 linear_codewords[NB_PULSE];

    cor_h_x2(
        h,
        x,
        dn,
        2,
        NB_TRACK_MR102,
        STEP_MR102,
        pOverflow);

    /* 2 = use GSMEFR scaling */

    set_sign12k2(
        dn,
        cn,
        sign,
        pos_max,
        NB_TRACK_MR102,
        ipos,
        STEP_MR102,
        pOverflow);

    /* same setsign alg as GSM-EFR new constants though*/

    cor_h(
        h,
        sign,
        rr,
        pOverflow);

    search_10and8i40(
        NB_PULSE,
        STEP_MR102,
        NB_TRACK_MR102,
        dn,
        rr,
        ipos,
        pos_max,
        codvec,
        pOverflow);

    build_code(
        codvec,
        sign,
        cod,
        h,
        y,
        linear_signs,
        linear_codewords,
        pOverflow);

    compress_code(
        linear_signs,
        linear_codewords,
        indx,
        pOverflow);

} /* code_8i40_31bits */



