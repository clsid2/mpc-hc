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



 Filename: pstfilt.cpp
 Functions:
            Post_Filter_reset
            Post_Filter

------------------------------------------------------------------------------
 MODULE DESCRIPTION

 This file contains the function that performs adaptive post-filtering on the
 synthesized speech. It also contains the functions that initialize, reset,
 and exit the post-filtering function.

------------------------------------------------------------------------------
*/

/*----------------------------------------------------------------------------
; INCLUDES
----------------------------------------------------------------------------*/
#include "pstfilt.h"
#include "typedef.h"
#include "mode.h"
#include "basicop_malloc.h"
#include "basic_op.h"
#include "weight_a.h"
#include "residu.h"
#include "syn_filt.h"
#include "preemph.h"
#include "cnst.h"
#include "oscl_mem.h"

/*----------------------------------------------------------------------------
; MACROS
; Define module specific macros here
----------------------------------------------------------------------------*/

/*----------------------------------------------------------------------------
; DEFINES
; Include all pre-processor statements here. Include conditional
; compile variables also.
----------------------------------------------------------------------------*/
#define L_H 22  /* size of truncated impulse response of A(z/g1)/A(z/g2) */

/*----------------------------------------------------------------------------
; LOCAL FUNCTION DEFINITIONS
; Function Prototype declaration
----------------------------------------------------------------------------*/

/*----------------------------------------------------------------------------
; LOCAL VARIABLE DEFINITIONS
; Variable declaration - defined here and used outside this module
----------------------------------------------------------------------------*/

/* Spectral expansion factors */
static const Word16 gamma3_MR122[M] =
{
    22938, 16057, 11240, 7868, 5508,
    3856, 2699, 1889, 1322, 925
};

static const Word16 gamma3[M] =
{
    18022, 9912, 5451, 2998, 1649, 907, 499, 274, 151, 83
};

static const Word16 gamma4_MR122[M] =
{
    24576, 18432, 13824, 10368, 7776,
    5832, 4374, 3281, 2461, 1846
};

static const Word16 gamma4[M] =
{
    22938, 16057, 11240, 7868, 5508, 3856, 2699, 1889, 1322, 925
};

/*----------------------------------------------------------------------------
; EXTERNAL FUNCTION REFERENCES
; Declare functions defined elsewhere and referenced in this module
----------------------------------------------------------------------------*/

/*----------------------------------------------------------------------------
; EXTERNAL VARIABLES REFERENCES
; Declare variables used in this module but defined elsewhere
----------------------------------------------------------------------------*/

/*
------------------------------------------------------------------------------
 FUNCTION NAME: Post_Filter_reset
------------------------------------------------------------------------------
 INPUT AND OUTPUT DEFINITIONS

 Inputs:
    state = pointer to structure of type Post_FilterState

 Outputs:
    fields of the structure pointed to by state is initialized to zero

 Returns:
    return_value = 0, if reset was successful; -1, otherwise (int)

 Global Variables Used:
    None

 Local Variables Needed:
    None

------------------------------------------------------------------------------
 FUNCTION DESCRIPTION

 This function initializes the state memory used by the Post_Filter function
 to zero.

------------------------------------------------------------------------------
 REQUIREMENTS

 None

------------------------------------------------------------------------------
 REFERENCES

 pstfilt.c, UMTS GSM AMR speech codec, R99 - Version 3.2.0, March 2, 2001

------------------------------------------------------------------------------
 PSEUDO-CODE

int Post_Filter_reset (Post_FilterState *state)
{
  if (state == (Post_FilterState *) NULL){
      fprintf(stderr, "Post_Filter_reset: invalid parameter\n");
      return -1;
  }

  Set_zero (state->mem_syn_pst, M);
  Set_zero (state->res2, L_SUBFR);
  Set_zero (state->synth_buf, L_FRAME + M);
  agc_reset(state->agc_state);
  preemphasis_reset(state->preemph_state);

  return 0;
}

------------------------------------------------------------------------------
 CAUTION [optional]
 [State any special notes, constraints or cautions for users of this function]

------------------------------------------------------------------------------
*/

Word16 Post_Filter_reset(Post_FilterState *state)
{
    if (state == (Post_FilterState *) NULL)
    {
        /*fprintf(stderr, "Post_Filter_reset: invalid parameter\n");  */
        return(-1);
    }

    oscl_memset(state->mem_syn_pst, 0, sizeof(Word16)*M);
    oscl_memset(state->res2, 0, sizeof(Word16)*L_SUBFR);
    oscl_memset(state->synth_buf, 0, sizeof(Word16)*(L_FRAME + M));
    agc_reset(&(state->agc_state));
    preemphasis_reset(&(state->preemph_state));

    return(0);
}

/****************************************************************************/

/*
------------------------------------------------------------------------------
 FUNCTION NAME: Post_Filter
------------------------------------------------------------------------------
 INPUT AND OUTPUT DEFINITIONS

 Inputs:
    st = pointer to a structure of type Post_FilterState
    mode = AMR mode
    syn = pointer to a buffer containing synthesized speech; upon
          exiting this function, it will contain the post-filtered
          synthesized speech
    Az_4 = pointer to the interpolated LPC parameters for all subframes
    pOverflow = pointer to overflow indicator of type Flag

 Outputs:
    fields of the structure pointed to by st contains the updated field
      values
    syn buffer contains the post-filtered synthesized speech
    pOverflow = 1 if overflow occurrs in the math functions called else
                it is zero.

 Returns:
    return_value = 0 (int)

 Global Variables Used:
    None

 Local Variables Needed:
    None

------------------------------------------------------------------------------
 FUNCTION DESCRIPTION

 This function performs the post-filtering on the synthesized speech. The
 post-filtering process is described as follows:
 (1) inverse filtering of syn[] through A(z/0.7) to get res2[]
 (2) tilt compensation filtering; 1 - MU*k*z^-1
 (3) synthesis filtering through 1/A(z/0.75)
 (4) adaptive gain control

------------------------------------------------------------------------------
 REQUIREMENTS

 None

------------------------------------------------------------------------------
 REFERENCES

 pstfilt.c, UMTS GSM AMR speech codec, R99 - Version 3.2.0, March 2, 2001

------------------------------------------------------------------------------
 PSEUDO-CODE

int Post_Filter (
    Post_FilterState *st, // i/o : post filter states
    enum Mode mode,       // i   : AMR mode
    Word16 *syn,          // i/o : synthesis speech (postfiltered is output)
    Word16 *Az_4          // i   : interpolated LPC parameters in all subfr.
)
{
     *-------------------------------------------------------------------*
     *           Declaration of parameters                               *
     *-------------------------------------------------------------------*

    Word16 Ap3[MP1], Ap4[MP1];  // bandwidth expanded LP parameters
    Word16 *Az;                 // pointer to Az_4:
                                //  LPC parameters in each subframe
    Word16 i_subfr;             // index for beginning of subframe
    Word16 h[L_H];

    Word16 i;
    Word16 temp1, temp2;
    Word32 L_tmp;
    Word16 *syn_work = &st->synth_buf[M];


     *-----------------------------------------------------*
     * Post filtering                                      *
     *-----------------------------------------------------*

    Copy (syn, syn_work , L_FRAME);

    Az = Az_4;

    for (i_subfr = 0; i_subfr < L_FRAME; i_subfr += L_SUBFR)
    {
       // Find weighted filter coefficients Ap3[] and ap[4]

       if (sub(mode, MR122) == 0 || sub(mode, MR102) == 0)
       {
          Weight_Ai (Az, gamma3_MR122, Ap3);
          Weight_Ai (Az, gamma4_MR122, Ap4);
       }
       else
       {
          Weight_Ai (Az, gamma3, Ap3);
          Weight_Ai (Az, gamma4, Ap4);
       }

       // filtering of synthesis speech by A(z/0.7) to find res2[]

       Residu (Ap3, &syn_work[i_subfr], st->res2, L_SUBFR);

       // tilt compensation filter

       // impulse response of A(z/0.7)/A(z/0.75)

       Copy (Ap3, h, M + 1);
       Set_zero (&h[M + 1], L_H - M - 1);
       Syn_filt (Ap4, h, h, L_H, &h[M + 1], 0);

       // 1st correlation of h[]

       L_tmp = L_mult (h[0], h[0]);
       for (i = 1; i < L_H; i++)
       {
          L_tmp = L_mac (L_tmp, h[i], h[i]);
       }
       temp1 = extract_h (L_tmp);

       L_tmp = L_mult (h[0], h[1]);
       for (i = 1; i < L_H - 1; i++)
       {
          L_tmp = L_mac (L_tmp, h[i], h[i + 1]);
       }
       temp2 = extract_h (L_tmp);

       if (temp2 <= 0)
       {
          temp2 = 0;
       }
       else
       {
          temp2 = mult (temp2, MU);
          temp2 = div_s (temp2, temp1);
       }

       preemphasis (st->preemph_state, st->res2, temp2, L_SUBFR);

       // filtering through  1/A(z/0.75)

       Syn_filt (Ap4, st->res2, &syn[i_subfr], L_SUBFR, st->mem_syn_pst, 1);

       // scale output to input

       agc (st->agc_state, &syn_work[i_subfr], &syn[i_subfr],
            AGC_FAC, L_SUBFR);

       Az += MP1;
    }

    // update syn_work[] buffer

    Copy (&syn_work[L_FRAME - M], &syn_work[-M], M);

    return 0;
}

------------------------------------------------------------------------------
 CAUTION [optional]
 [State any special notes, constraints or cautions for users of this function]

------------------------------------------------------------------------------
*/

void Post_Filter(
    Post_FilterState *st, /* i/o : post filter states                        */
    enum Mode mode,       /* i   : AMR mode                                  */
    Word16 *syn,          /* i/o : synthesis speech (postfiltered is output) */
    Word16 *Az_4,         /* i   : interpolated LPC parameters in all subfr. */
    Flag   *pOverflow
)
{
    Word16 Ap3[MP1];
    Word16 Ap4[MP1];            /* bandwidth expanded LP parameters */
    Word16 *Az;                 /* pointer to Az_4:                 */
    /*  LPC parameters in each subframe */
    register Word16 i_subfr;    /* index for beginning of subframe  */
    Word16 h[L_H];

    register Word16 i;
    Word16 temp1;
    Word16 temp2;
    Word32 L_tmp;
    Word32 L_tmp2;
    Word16 *syn_work = &st->synth_buf[M];


    /*-----------------------------------------------------*
     * Post filtering                                      *
     *-----------------------------------------------------*/

    oscl_memmove((void *)syn_work , syn, L_FRAME*sizeof(*syn));

    Az = Az_4;

    for (i_subfr = 0; i_subfr < L_FRAME; i_subfr += L_SUBFR)
    {
        /* Find weighted filter coefficients Ap3[] and ap[4] */

        if (mode == MR122 || mode == MR102)
        {
            Weight_Ai(Az, gamma3_MR122, Ap3);
            Weight_Ai(Az, gamma4_MR122, Ap4);
        }
        else
        {
            Weight_Ai(Az, gamma3, Ap3);
            Weight_Ai(Az, gamma4, Ap4);
        }

        /* filtering of synthesis speech by A(z/0.7) to find res2[] */

        Residu(Ap3, &syn_work[i_subfr], st->res2, L_SUBFR);

        /* tilt compensation filter */

        /* impulse response of A(z/0.7)/A(z/0.75) */

        oscl_memmove((void *)h, Ap3, (M + 1)*sizeof(*Ap3));
        oscl_memset(&h[M + 1], 0, sizeof(Word16)*(L_H - M - 1));
        Syn_filt(Ap4, h, h, L_H, &h[M + 1], 0);

        /* 1st correlation of h[] */

        L_tmp = 0;

        for (i = L_H - 1; i >= 0; i--)
        {
            L_tmp2 = ((Word32) h[i]) * h[i];

            if (L_tmp2 != (Word32) 0x40000000L)
            {
                L_tmp2 = L_tmp2 << 1;
            }
            else
            {
                *pOverflow = 1;
                L_tmp2 = MAX_32;
                break;
            }

            L_tmp = L_add(L_tmp, L_tmp2, pOverflow);
        }
        temp1 = (Word16)(L_tmp >> 16);

        L_tmp = 0;

        for (i = L_H - 2; i >= 0; i--)
        {
            L_tmp2 = ((Word32) h[i]) * h[i + 1];

            if (L_tmp2 != (Word32) 0x40000000L)
            {
                L_tmp2 = L_tmp2 << 1;
            }
            else
            {
                *pOverflow = 1;
                L_tmp2 = MAX_32;
                break;
            }

            L_tmp = L_add(L_tmp, L_tmp2, pOverflow);
        }
        temp2 = (Word16)(L_tmp >> 16);

        if (temp2 <= 0)
        {
            temp2 = 0;
        }
        else
        {
            L_tmp = (((Word32) temp2) * MU) >> 15;

            /* Sign-extend product */
            if (L_tmp & (Word32) 0x00010000L)
            {
                L_tmp = L_tmp | (Word32) 0xffff0000L;
            }
            temp2 = (Word16) L_tmp;

            temp2 = div_s(temp2, temp1);
        }

        preemphasis(&(st->preemph_state), st->res2, temp2, L_SUBFR, pOverflow);

        /* filtering through  1/A(z/0.75) */

        Syn_filt(Ap4, st->res2, &syn[i_subfr], L_SUBFR, st->mem_syn_pst, 1);

        /* scale output to input */

        agc(&(st->agc_state), &syn_work[i_subfr], &syn[i_subfr],
            AGC_FAC, L_SUBFR, pOverflow);

        Az += MP1;
    }

    /* update syn_work[] buffer */

    oscl_memmove((void *)&syn_work[-M], &syn_work[L_FRAME - M], M*sizeof(*syn_work));

    return;
}
