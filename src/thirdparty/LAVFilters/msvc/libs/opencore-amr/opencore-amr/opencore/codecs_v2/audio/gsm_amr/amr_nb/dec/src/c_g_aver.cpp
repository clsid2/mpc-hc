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



 Filename: c_g_aver.cpp
 Functions:
            Cb_gain_average_reset
            Cb_gain_average

------------------------------------------------------------------------------
 MODULE DESCRIPTION

 This file contains functions that reset and perform
 codebook gain calculations.

------------------------------------------------------------------------------
*/


/*----------------------------------------------------------------------------
; INCLUDES
----------------------------------------------------------------------------*/
#include    "c_g_aver.h"
#include    "typedef.h"
#include    "mode.h"
#include    "cnst.h"

#include    "basic_op.h"
#include    "oscl_mem.h"

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
; LOCAL VARIABLE DEFINITIONS
; Variable declaration - defined here and used outside this module
----------------------------------------------------------------------------*/

/*
------------------------------------------------------------------------------
 FUNCTION NAME: Cb_gain_average_reset
------------------------------------------------------------------------------
 INPUT AND OUTPUT DEFINITIONS

 Inputs:
    state = pointer to a structure of type Cb_gain_averageState

 Outputs:
    Structure pointed to by state is initialized to zeros

 Returns:
    Returns 0 if memory was successfully initialized,
        otherwise returns -1.

 Global Variables Used:
    None.

 Local Variables Needed:
    None.

------------------------------------------------------------------------------
 FUNCTION DESCRIPTION

 Resets state memory

------------------------------------------------------------------------------
 REQUIREMENTS

 None.

------------------------------------------------------------------------------
 REFERENCES

 c_g_aver.c, UMTS GSM AMR speech codec, R99 - Version 3.2.0, March 2, 2001

------------------------------------------------------------------------------
 PSEUDO-CODE

Word16 Cb_gain_average_reset (Cb_gain_averageState *state)
{
   if (state == (Cb_gain_averageState *) NULL){
      fprintf(stderr, "Cb_gain_average_reset: invalid parameter\n");
      return -1;
   }

   // Static vectors to zero
   Set_zero (state->cbGainHistory, L_CBGAINHIST);

   // Initialize hangover handling
   state->hangVar = 0;
   state->hangCount= 0;

   return 0;
}

------------------------------------------------------------------------------
 CAUTION [optional]
 [State any special notes, constraints or cautions for users of this function]

------------------------------------------------------------------------------
*/

Word16  Cb_gain_average_reset(Cb_gain_averageState *state)
{
    if (state == (Cb_gain_averageState *) NULL)
    {
        /* fprint(stderr, "Cb_gain_average_reset: invalid parameter\n");  */
        return(-1);
    }

    /* Static vectors to zero */
    oscl_memset(state->cbGainHistory, 0, L_CBGAINHIST*sizeof(Word16));

    /* Initialize hangover handling */
    state->hangVar = 0;
    state->hangCount = 0;

    return(0);
}

/****************************************************************************/

/*
------------------------------------------------------------------------------
 FUNCTION NAME: Cb_gain_average
------------------------------------------------------------------------------
 INPUT AND OUTPUT DEFINITIONS

 Inputs:
    st = pointer to structure of type Cb_gain_averageState
    mode = AMR mode (enum Mode)
    gain_code = CB gain (Word16)
    lsp = the LSP for the current frame (Word16)
    lspAver = the average of LSP for 8 frames (Word16)
    bfi = bad frame indication flag (Word16)
    prev_bf = previous bad frame indication flag (Word16)
    pdfi = potential degraded bad frame ind flag (Word16)
    prev_pdf = prev pot. degraded bad frame ind flag (Word16)
    inBackgroundNoise = background noise decision (Word16)
    voicedHangover = # of frames after last voiced frame (Word16)
    pOverflow = address of overflow (Flag)

 Returns:
    cbGainMix = codebook gain (Word16)

 Outputs:
    None.

 Global Variables Used:
    None.

 Local Variables Needed:
    None.

------------------------------------------------------------------------------
 FUNCTION DESCRIPTION

 The mix cb gains for MR475, MR515, MR59, MR67, MR102; gain_code other modes

------------------------------------------------------------------------------
 REQUIREMENTS

 None.

------------------------------------------------------------------------------
 REFERENCES

 c_g_aver.c, UMTS GSM AMR speech codec, R99 - Version 3.2.0, March 2, 2001

------------------------------------------------------------------------------
 PSEUDO-CODE

Word16 Cb_gain_average (
   Cb_gain_averageState *st, // i/o : State variables for CB gain avergeing
   enum Mode mode,           // i   : AMR mode
   Word16 gain_code,         // i   : CB gain                              Q1
   Word16 lsp[],             // i   : The LSP for the current frame       Q15
   Word16 lspAver[],         // i   : The average of LSP for 8 frames     Q15
   Word16 bfi,               // i   : bad frame indication flag
   Word16 prev_bf,           // i   : previous bad frame indication flag
   Word16 pdfi,              // i   : potential degraded bad frame ind flag
   Word16 prev_pdf,          // i   : prev pot. degraded bad frame ind flag
   Word16 inBackgroundNoise, // i   : background noise decision
   Word16 voicedHangover     // i   : # of frames after last voiced frame
   )
{
   //---------------------------------------------------------*
    * Compute mixed cb gain, used to make cb gain more        *
    * smooth in background noise for modes 5.15, 5.9 and 6.7  *
    * states that needs to be updated by all                  *
    *---------------------------------------------------------
   Word16 i;
   Word16 cbGainMix, diff, tmp_diff, bgMix, cbGainMean;
   Word32 L_sum;
   Word16 tmp[M], tmp1, tmp2, shift1, shift2, shift;

   // set correct cbGainMix for MR74, MR795, MR122
   cbGainMix = gain_code;

    *-------------------------------------------------------*
    *   Store list of CB gain needed in the CB gain         *
    *   averaging                                           *
    *-------------------------------------------------------*
   for (i = 0; i < (L_CBGAINHIST-1); i++)
   {
      st->cbGainHistory[i] = st->cbGainHistory[i+1];
   }
   st->cbGainHistory[L_CBGAINHIST-1] = gain_code;

   // compute lsp difference
   for (i = 0; i < M; i++) {
      tmp1 = abs_s(sub(lspAver[i], lsp[i]));  // Q15
      shift1 = sub(norm_s(tmp1), 1);          // Qn
      tmp1 = shl(tmp1, shift1);               // Q15+Qn
      shift2 = norm_s(lspAver[i]);            // Qm
      tmp2 = shl(lspAver[i], shift2);         // Q15+Qm
      tmp[i] = div_s(tmp1, tmp2);             // Q15+(Q15+Qn)-(Q15+Qm)
      shift = sub(add(2, shift1), shift2);
      if (shift >= 0)
      {
         tmp[i] = shr(tmp[i], shift); // Q15+Qn-Qm-Qx=Q13
      }
      else
      {
         tmp[i] = shl(tmp[i], negate(shift)); // Q15+Qn-Qm-Qx=Q13
      }
   }

   diff = tmp[0];
   for (i = 1; i < M; i++) {
      diff = add(diff, tmp[i]);       // Q13
   }

   // Compute hangover
   if (sub(diff, 5325) > 0)  // 0.65 in Q11
   {
      st->hangVar = add(st->hangVar, 1);
   }
   else
   {
      st->hangVar = 0;
   }

   if (sub(st->hangVar, 10) > 0)
   {
      st->hangCount = 0;  // Speech period, reset hangover variable
   }

   // Compute mix constant (bgMix)
   bgMix = 8192;    // 1 in Q13
   if ((sub(mode, MR67) <= 0) || (sub(mode, MR102) == 0))
      // MR475, MR515, MR59, MR67, MR102
   {
      // if errors and presumed noise make smoothing probability stronger
      if (((((pdfi != 0) && (prev_pdf != 0)) || (bfi != 0) || (prev_bf != 0)) &&
          (sub(voicedHangover, 1) > 0) && (inBackgroundNoise != 0) &&
          ((sub(mode, MR475) == 0) ||
           (sub(mode, MR515) == 0) ||
           (sub(mode, MR59) == 0)) ))
      {
         // bgMix = min(0.25, max(0.0, diff-0.55)) / 0.25;
         tmp_diff = sub(diff, 4506);   // 0.55 in Q13

         // max(0.0, diff-0.55)
         if (tmp_diff > 0)
         {
            tmp1 = tmp_diff;
         }
         else
         {
            tmp1 = 0;
         }

         // min(0.25, tmp1)
         if (sub(2048, tmp1) < 0)
         {
            bgMix = 8192;
         }
         else
         {
            bgMix = shl(tmp1, 2);
         }
      }
      else
      {
         // bgMix = min(0.25, max(0.0, diff-0.40)) / 0.25;
         tmp_diff = sub(diff, 3277); // 0.4 in Q13

         // max(0.0, diff-0.40)
         if (tmp_diff > 0)
         {
            tmp1 = tmp_diff;
         }
         else
         {
            tmp1 = 0;
         }

         // min(0.25, tmp1)
         if (sub(2048, tmp1) < 0)
         {
            bgMix = 8192;
         }
         else
         {
            bgMix = shl(tmp1, 2);
         }
      }

      if ((sub(st->hangCount, 40) < 0) || (sub(diff, 5325) > 0)) // 0.65 in Q13
      {
         bgMix = 8192;  // disable mix if too short time since
      }

      // Smoothen the cb gain trajectory
      // smoothing depends on mix constant bgMix
      L_sum = L_mult(6554, st->cbGainHistory[2]); // 0.2 in Q15; L_sum in Q17
      for (i = 3; i < L_CBGAINHIST; i++)
      {
         L_sum = L_mac(L_sum, 6554, st->cbGainHistory[i]);
      }
      cbGainMean = pv_round(L_sum);                      // Q1

      // more smoothing in error and bg noise (NB no DFI used  here)
      if (((bfi != 0) || (prev_bf != 0)) && (inBackgroundNoise != 0) &&
          ((sub(mode, MR475) == 0) ||
           (sub(mode, MR515) == 0) ||
           (sub(mode, MR59) == 0)) )
      {
         L_sum = L_mult(4681, st->cbGainHistory[0]); // 0.143 in Q15; L_sum in Q17
         for (i = 1; i < L_CBGAINHIST; i++)
         {
            L_sum = L_mac(L_sum, 4681, st->cbGainHistory[i]);
         }
         cbGainMean = pv_round(L_sum);                   // Q1
      }

      // cbGainMix = bgMix*cbGainMix + (1-bgMix)*cbGainMean;
      L_sum = L_mult(bgMix, cbGainMix);               // L_sum in Q15
      L_sum = L_mac(L_sum, 8192, cbGainMean);
      L_sum = L_msu(L_sum, bgMix, cbGainMean);
      cbGainMix = pv_round(L_shl(L_sum, 2));             // Q1
   }

   st->hangCount = add(st->hangCount, 1);
   return cbGainMix;
}

------------------------------------------------------------------------------
 CAUTION [optional]
 [State any special notes, constraints or cautions for users of this function]

------------------------------------------------------------------------------
*/

Word16 Cb_gain_average(
    Cb_gain_averageState *st, /* i/o : State variables for CB gain averaging */
    enum Mode mode,           /* i   : AMR mode                              */
    Word16 gain_code,         /* i   : CB gain                            Q1 */
    Word16 lsp[],             /* i   : The LSP for the current frame     Q15 */
    Word16 lspAver[],         /* i   : The average of LSP for 8 frames   Q15 */
    Word16 bfi,               /* i   : bad frame indication flag             */
    Word16 prev_bf,           /* i   : previous bad frame indication flag    */
    Word16 pdfi,              /* i   : potential degraded bad frame ind flag */
    Word16 prev_pdf,          /* i   : prev pot. degraded bad frame ind flag */
    Word16 inBackgroundNoise, /* i   : background noise decision             */
    Word16 voicedHangover,    /* i   : # of frames after last voiced frame   */
    Flag   *pOverflow
)
{
    Word16 i;
    Word16 cbGainMix;
    Word16 diff;
    Word16 tmp_diff;
    Word16 bgMix;
    Word16 cbGainMean;
    Word32 L_sum;
    Word16 tmp[M];
    Word16 tmp1;
    Word16 tmp2;
    Word16 shift1;
    Word16 shift2;
    Word16 shift;

    /*---------------------------------------------------------*
     * Compute mixed cb gain, used to make cb gain more        *
     * smooth in background noise for modes 5.15, 5.9 and 6.7  *
     * states that needs to be updated by all                  *
     *---------------------------------------------------------*/

    /* set correct cbGainMix for MR74, MR795, MR122 */
    cbGainMix = gain_code;

    /*-------------------------------------------------------*
     *   Store list of CB gain needed in the CB gain         *
     *   averaging                                           *
     *-------------------------------------------------------*/
    for (i = 0; i < (L_CBGAINHIST - 1); i++)
    {
        st->cbGainHistory[i] = st->cbGainHistory[i+1];
    }
    st->cbGainHistory[L_CBGAINHIST-1] = gain_code;

    diff = 0;

    /* compute lsp difference */
    for (i = 0; i < M; i++)
    {
        tmp1 = abs_s(sub(*(lspAver + i), *(lsp + i), pOverflow));
        /* Q15      */
        shift1 = norm_s(tmp1) - 1 ;                     /* Qn       */
        tmp1 = shl(tmp1, shift1, pOverflow);            /* Q15+Qn   */
        shift2 = norm_s(*(lspAver + i));                /* Qm       */
        tmp2 = shl(*(lspAver + i), shift2, pOverflow);  /* Q15+Qm   */
        tmp[i] = div_s(tmp1, tmp2);        /* Q15+(Q15+Qn)-(Q15+Qm) */

        shift = 2 + shift1 - shift2;

        if (shift >= 0)
        {
            *(tmp + i) = shr(*(tmp + i), shift, pOverflow);
            /* Q15+Qn-Qm-Qx=Q13 */
        }
        else
        {
            *(tmp + i) = shl(*(tmp + i), negate(shift), pOverflow);
            /* Q15+Qn-Qm-Qx=Q13 */
        }

        diff = add_16(diff, *(tmp + i), pOverflow);           /* Q13 */
    }

    /* Compute hangover */

    if (diff > 5325)                /* 0.65 in Q11 */
    {
        st->hangVar += 1;
    }
    else
    {
        st->hangVar = 0;
    }


    if (st->hangVar > 10)
    {
        /* Speech period, reset hangover variable */
        st->hangCount = 0;
    }

    /* Compute mix constant (bgMix) */
    bgMix = 8192;    /* 1 in Q13 */

    if ((mode <= MR67) || (mode == MR102))
        /* MR475, MR515, MR59, MR67, MR102 */
    {
        /* if errors and presumed noise make smoothing probability stronger */

        if (((((pdfi != 0) && (prev_pdf != 0)) || (bfi != 0) ||
                (prev_bf != 0))
                && (voicedHangover > 1)
                && (inBackgroundNoise != 0)
                && ((mode == MR475) || (mode == MR515) ||
                    (mode == MR59))))
        {
            /* bgMix = min(0.25, max(0.0, diff-0.55)) / 0.25; */
            tmp_diff = diff - 4506;   /* 0.55 in Q13 */
        }
        else
        {
            /* bgMix = min(0.25, max(0.0, diff-0.40)) / 0.25; */
            tmp_diff = diff - 3277; /* 0.4 in Q13 */
        }

        /* max(0.0, diff-0.55)  or  */
        /* max(0.0, diff-0.40) */
        if (tmp_diff > 0)
        {
            tmp1 = tmp_diff;
        }
        else
        {
            tmp1 = 0;
        }

        /* min(0.25, tmp1) */
        if (2048 < tmp1)
        {
            bgMix = 8192;
        }
        else
        {
            bgMix = shl(tmp1, 2, pOverflow);
        }

        if ((st->hangCount < 40) || (diff > 5325)) /* 0.65 in Q13 */
        {
            /* disable mix if too short time since */
            bgMix = 8192;
        }

        /* Smoothen the cb gain trajectory  */
        /* smoothing depends on mix constant bgMix */
        L_sum = L_mult(6554, st->cbGainHistory[2], pOverflow);
        /* 0.2 in Q15; L_sum in Q17 */

        for (i = 3; i < L_CBGAINHIST; i++)
        {
            L_sum = L_mac(L_sum, 6554, st->cbGainHistory[i], pOverflow);
        }
        cbGainMean = pv_round(L_sum, pOverflow);               /* Q1 */

        /* more smoothing in error and bg noise (NB no DFI used here) */

        if (((bfi != 0) || (prev_bf != 0)) && (inBackgroundNoise != 0)
                && ((mode == MR475) || (mode == MR515)
                    || (mode == MR59)))
        {
            /* 0.143 in Q15; L_sum in Q17    */
            L_sum = L_mult(4681, st->cbGainHistory[0], pOverflow);
            for (i = 1; i < L_CBGAINHIST; i++)
            {
                L_sum =
                    L_mac(L_sum, 4681, st->cbGainHistory[i], pOverflow);
            }
            cbGainMean = pv_round(L_sum, pOverflow);              /* Q1 */
        }

        /* cbGainMix = bgMix*cbGainMix + (1-bgMix)*cbGainMean; */
        /* L_sum in Q15 */
        L_sum = L_mult(bgMix, cbGainMix, pOverflow);
        L_sum = L_mac(L_sum, 8192, cbGainMean, pOverflow);
        L_sum = L_msu(L_sum, bgMix, cbGainMean, pOverflow);
        cbGainMix = pv_round(L_shl(L_sum, 2, pOverflow), pOverflow);  /* Q1 */
    }

    st->hangCount += 1;

    return (cbGainMix);
}

