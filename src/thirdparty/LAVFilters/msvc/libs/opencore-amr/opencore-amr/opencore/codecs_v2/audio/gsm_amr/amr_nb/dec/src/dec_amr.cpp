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

 Filename: dec_amr.cpp
 Funtions: Decoder_amr_init
           Decoder_amr_reset
           Decoder_amr

------------------------------------------------------------------------------
 MODULE DESCRIPTION

 This file contains the function used to decode one speech frame using a given
 codec mode. The functions used to initialize, reset, and exit are also
 included in this file.

------------------------------------------------------------------------------
*/

/*----------------------------------------------------------------------------
; INCLUDES
----------------------------------------------------------------------------*/
#include "dec_amr.h"
#include "typedef.h"
#include "cnst.h"
#include "set_zero.h"
#include "syn_filt.h"
#include "d_plsf.h"
#include "agc.h"
#include "int_lpc.h"
#include "dec_gain.h"
#include "dec_lag3.h"
#include "dec_lag6.h"
#include "d2_9pf.h"
#include "d2_11pf.h"
#include "d3_14pf.h"
#include "d4_17pf.h"
#include "d8_31pf.h"
#include "d1035pf.h"
#include "pred_lt.h"
#include "d_gain_p.h"
#include "d_gain_c.h"
#include "dec_gain.h"
#include "ec_gains.h"
#include "ph_disp.h"
#include "c_g_aver.h"
#include "int_lsf.h"
#include "lsp_lsf.h"
#include "lsp_avg.h"
#include "bgnscd.h"
#include "ex_ctrl.h"
#include "sqrt_l.h"
#include "frame.h"
#include "b_cn_cod.h"
#include "basic_op.h"
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
 FUNCTION NAME: Decoder_amr_init
------------------------------------------------------------------------------
 INPUT AND OUTPUT DEFINITIONS

 Inputs:
    state = pointer to a pointer to structures of type Decoder_amrState

 Outputs:
    structure pointed to by the pointer which is pointed to by state is
      initialized to each field's initial values

    state pointer points to the address of the memory allocated by
      Decoder_amr_init function

 Returns:
    return_value = 0, if the initialization was successful; -1, otherwise (int)

 Global Variables Used:
    None

 Local Variables Needed:
    None

------------------------------------------------------------------------------
 FUNCTION DESCRIPTION

 This function allocates and initializes state memory used by the Decoder_amr
 function. It stores the pointer to the filter status structure in state. This
 pointer has to be passed to Decoder_amr in each call. The function returns
 0, if initialization was successful and -1, otherwise.

------------------------------------------------------------------------------
 REQUIREMENTS

 None

------------------------------------------------------------------------------
 REFERENCES

 dec_amr.c, UMTS GSM AMR speech codec, R99 - Version 3.2.0, March 2, 2001

------------------------------------------------------------------------------
 PSEUDO-CODE

int Decoder_amr_init (Decoder_amrState **state)
{
  Decoder_amrState* s;
  Word16 i;

  if (state == (Decoder_amrState **) NULL){
      fprintf(stderr, "Decoder_amr_init: invalid parameter\n");
      return -1;
  }
  *state = NULL;

  // allocate memory
  if ((s= (Decoder_amrState *) malloc(sizeof(Decoder_amrState))) == NULL){
      fprintf(stderr, "Decoder_amr_init: can not malloc state structure\n");
      return -1;
  }

  s->T0_lagBuff = 40;
  s->inBackgroundNoise = 0;
  s->voicedHangover = 0;
  for (i = 0; i < 9; i++)
     s->ltpGainHistory[i] = 0;

  s->lsfState = NULL;
  s->ec_gain_p_st = NULL;
  s->ec_gain_c_st = NULL;
  s->pred_state = NULL;
  s->ph_disp_st = NULL;
  s->dtxDecoderState = NULL;

  if (D_plsf_init(&s->lsfState) ||
      ec_gain_pitch_init(&s->ec_gain_p_st) ||
      ec_gain_code_init(&s->ec_gain_c_st) ||
      gc_pred_init(&s->pred_state) ||
      Cb_gain_average_init(&s->Cb_gain_averState) ||
      lsp_avg_init(&s->lsp_avg_st) ||
      Bgn_scd_init(&s->background_state) ||
      ph_disp_init(&s->ph_disp_st) ||
      dtx_dec_init(&s->dtxDecoderState)) {
      Decoder_amr_exit(&s);
      return -1;
  }

  Decoder_amr_reset(s, (enum Mode)0);
  *state = s;

  return 0;
}

------------------------------------------------------------------------------
 CAUTION [optional]
 [State any special notes, constraints or cautions for users of this function]

------------------------------------------------------------------------------
*/

Word16 Decoder_amr_init(Decoder_amrState *s)
{
    Word16 i;

    if (s == (Decoder_amrState *) NULL)
    {
        /* fprint(stderr, "Decoder_amr_init: invalid parameter\n");  */
        return(-1);
    }

    get_const_tbls(&s->common_amr_tbls);

    s->T0_lagBuff = 40;
    s->inBackgroundNoise = 0;
    s->voicedHangover = 0;

    /* Initialize overflow Flag */

    s->overflow = 0;

    for (i = 0; i < LTP_GAIN_HISTORY_LEN; i++)
    {
        s->ltpGainHistory[i] = 0;
    }

    D_plsf_reset(&s->lsfState, s->common_amr_tbls.mean_lsf_5_ptr);
    ec_gain_pitch_reset(&s->ec_gain_p_st);
    ec_gain_code_reset(&s->ec_gain_c_st);
    Cb_gain_average_reset(&s->Cb_gain_averState);
    lsp_avg_reset(&s->lsp_avg_st, s->common_amr_tbls.mean_lsf_5_ptr);
    Bgn_scd_reset(&s->background_state);
    ph_disp_reset(&s->ph_disp_st);
    dtx_dec_reset(&s->dtxDecoderState);
    gc_pred_reset(&s->pred_state);

    Decoder_amr_reset(s, MR475);

    return(0);
}

/****************************************************************************/

/*
------------------------------------------------------------------------------
 FUNCTION NAME: Decoder_amr_reset
------------------------------------------------------------------------------
 INPUT AND OUTPUT DEFINITIONS

 Inputs:
    state = pointer to a structure of type Decoder_amrState
    mode = codec mode (enum Mode)

 Outputs:
    structure pointed to by state is initialized to its reset value

 Returns:
    return_value = 0, if reset was successful; -1, otherwise (int)

 Global Variables Used:
    None

 Local Variables Needed:
    None

------------------------------------------------------------------------------
 FUNCTION DESCRIPTION

 This function resets the state memory used by the Decoder_amr function. It
 returns a 0, if reset was successful and -1, otherwise.

------------------------------------------------------------------------------
 REQUIREMENTS

 None

------------------------------------------------------------------------------
 REFERENCES

 dec_amr.c, UMTS GSM AMR speech codec, R99 - Version 3.2.0, March 2, 2001

------------------------------------------------------------------------------
 PSEUDO-CODE

int Decoder_amr_reset (Decoder_amrState *state, enum Mode mode)
{
  Word16 i;

  if (state == (Decoder_amrState *) NULL){
      fprintf(stderr, "Decoder_amr_reset: invalid parameter\n");
      return -1;
  }

  // Initialize static pointer
  state->exc = state->old_exc + PIT_MAX + L_INTERPOL;

  // Static vectors to zero
  Set_zero (state->old_exc, PIT_MAX + L_INTERPOL);

  if (mode != MRDTX)
     Set_zero (state->mem_syn, M);

  // initialize pitch sharpening
  state->sharp = SHARPMIN;
  state->old_T0 = 40;

  // Initialize state->lsp_old []

  if (mode != MRDTX) {
      Copy(lsp_init_data, &state->lsp_old[0], M);
  }

  // Initialize memories of bad frame handling
  state->prev_bf = 0;
  state->prev_pdf = 0;
  state->state = 0;

  state->T0_lagBuff = 40;
  state->inBackgroundNoise = 0;
  state->voicedHangover = 0;
  if (mode != MRDTX) {
      for (i=0;i<9;i++)
          state->excEnergyHist[i] = 0;
  }

  for (i = 0; i < 9; i++)
     state->ltpGainHistory[i] = 0;

  Cb_gain_average_reset(state->Cb_gain_averState);
  if (mode != MRDTX)
     lsp_avg_reset(state->lsp_avg_st);
  D_plsf_reset(state->lsfState);
  ec_gain_pitch_reset(state->ec_gain_p_st);
  ec_gain_code_reset(state->ec_gain_c_st);

  if (mode != MRDTX)
     gc_pred_reset(state->pred_state);

  Bgn_scd_reset(state->background_state);
  state->nodataSeed = 21845;
  ph_disp_reset(state->ph_disp_st);
  if (mode != MRDTX)
     dtx_dec_reset(state->dtxDecoderState);

  return 0;
}

------------------------------------------------------------------------------
 CAUTION [optional]
 [State any special notes, constraints or cautions for users of this function]

------------------------------------------------------------------------------
*/

Word16 Decoder_amr_reset(Decoder_amrState *state, enum Mode mode)
{
    Word16 i;

    if (state == (Decoder_amrState *) NULL)
    {
        /* fprint(stderr, "Decoder_amr_reset: invalid parameter\n");  */
        return(-1);
    }

    /* Initialize static pointer */
    state->exc = state->old_exc + PIT_MAX + L_INTERPOL;

    /* Static vectors to zero */
    oscl_memset(state->old_exc, 0, sizeof(Word16)*(PIT_MAX + L_INTERPOL));

    if (mode != MRDTX)
    {
        oscl_memset(state->mem_syn, 0, sizeof(Word16)*M);
    }
    /* initialize pitch sharpening */
    state->sharp = SHARPMIN;
    state->old_T0 = 40;

    /* Initialize overflow Flag */

    state->overflow = 0;

    /* Initialize state->lsp_old [] */

    if (mode != MRDTX)
    {
        state->lsp_old[0] = 30000;
        state->lsp_old[1] = 26000;
        state->lsp_old[2] = 21000;
        state->lsp_old[3] = 15000;
        state->lsp_old[4] = 8000;
        state->lsp_old[5] = 0;
        state->lsp_old[6] = -8000;
        state->lsp_old[7] = -15000;
        state->lsp_old[8] = -21000;
        state->lsp_old[9] = -26000;
    }

    /* Initialize memories of bad frame handling */
    state->prev_bf = 0;
    state->prev_pdf = 0;
    state->state = 0;

    state->T0_lagBuff = 40;
    state->inBackgroundNoise = 0;
    state->voicedHangover = 0;
    if (mode != MRDTX)
    {
        for (i = 0; i < EXC_ENERGY_HIST_LEN; i++)
        {
            state->excEnergyHist[i] = 0;
        }
    }

    for (i = 0; i < LTP_GAIN_HISTORY_LEN; i++)
    {
        state->ltpGainHistory[i] = 0;
    }

    Cb_gain_average_reset(&(state->Cb_gain_averState));
    if (mode != MRDTX)
    {
        lsp_avg_reset(&(state->lsp_avg_st), state->common_amr_tbls.mean_lsf_5_ptr);
    }
    D_plsf_reset(&(state->lsfState), state->common_amr_tbls.mean_lsf_5_ptr);
    ec_gain_pitch_reset(&(state->ec_gain_p_st));
    ec_gain_code_reset(&(state->ec_gain_c_st));

    if (mode != MRDTX)
    {
        gc_pred_reset(&(state->pred_state));
    }

    Bgn_scd_reset(&(state->background_state));
    state->nodataSeed = 21845;
    ph_disp_reset(&(state->ph_disp_st));
    if (mode != MRDTX)
    {
        dtx_dec_reset(&(state->dtxDecoderState));
    }

    return(0);
}

/****************************************************************************/

/*
------------------------------------------------------------------------------
 FUNCTION NAME: Decoder_amr
------------------------------------------------------------------------------
 INPUT AND OUTPUT DEFINITIONS

 Inputs:
    st = pointer to a structure of type Decoder_amrState
    mode = codec mode (enum Mode)
    parm = buffer of synthesis parameters (Word16)
    frame_type = received frame type (enum RXFrameType)
    synth = buffer containing synthetic speech (Word16)
    A_t = buffer containing decoded LP filter in 4 subframes (Word16)

 Outputs:
    structure pointed to by st contains the newly calculated decoder
      parameters
    synth buffer contains the decoded speech samples
    A_t buffer contains the decoded LP filter parameters

 Returns:
    return_value = 0 (int)

 Global Variables Used:
    None

 Local Variables Needed:
    None

------------------------------------------------------------------------------
 FUNCTION DESCRIPTION

 This function performs the decoding of one speech frame for a given codec
 mode.

------------------------------------------------------------------------------
 REQUIREMENTS

 None

------------------------------------------------------------------------------
 REFERENCES

 dec_amr.c, UMTS GSM AMR speech codec, R99 - Version 3.2.0, March 2, 2001

------------------------------------------------------------------------------
 PSEUDO-CODE

int Decoder_amr (
    Decoder_amrState *st,      // i/o : State variables
    enum Mode mode,            // i   : AMR mode
    Word16 parm[],             // i   : vector of synthesis parameters
                                        (PRM_SIZE)
    enum RXFrameType frame_type, // i   : received frame type
    Word16 synth[],            // o   : synthesis speech (L_FRAME)
    Word16 A_t[]               // o   : decoded LP filter in 4 subframes
                                        (AZ_SIZE)
)
{
    // LPC coefficients

    Word16 *Az;                // Pointer on A_t

    // LSPs

    Word16 lsp_new[M];
    Word16 lsp_mid[M];

    // LSFs

    Word16 prev_lsf[M];
    Word16 lsf_i[M];

    // Algebraic codevector

    Word16 code[L_SUBFR];

    // excitation

    Word16 excp[L_SUBFR];
    Word16 exc_enhanced[L_SUBFR];

    // Scalars

    Word16 i, i_subfr;
    Word16 T0, T0_frac, index, index_mr475 = 0;
    Word16 gain_pit, gain_code, gain_code_mix, pit_sharp, pit_flag, pitch_fac;
    Word16 t0_min, t0_max;
    Word16 delta_frc_low, delta_frc_range;
    Word16 tmp_shift;
    Word16 temp;
    Word32 L_temp;
    Word16 flag4;
    Word16 carefulFlag;
    Word16 excEnergy;
    Word16 subfrNr;
    Word16 evenSubfr = 0;

    Word16 bfi = 0;   // bad frame indication flag
    Word16 pdfi = 0;  // potential degraded bad frame flag

    enum DTXStateType newDTXState;  // SPEECH , DTX, DTX_MUTE

    // find the new  DTX state  SPEECH OR DTX
    newDTXState = rx_dtx_handler(st->dtxDecoderState, frame_type);

    // DTX actions
    if (sub(newDTXState, SPEECH) != 0 )
    {
       Decoder_amr_reset (st, MRDTX);

       dtx_dec(st->dtxDecoderState,
               st->mem_syn,
               st->lsfState,
               st->pred_state,
               st->Cb_gain_averState,
               newDTXState,
               mode,
               parm, synth, A_t);
       // update average lsp

       Lsf_lsp(st->lsfState->past_lsf_q, st->lsp_old, M);
       lsp_avg(st->lsp_avg_st, st->lsfState->past_lsf_q);
       goto the_end;
    }

    // SPEECH action state machine
    if ((sub(frame_type, RX_SPEECH_BAD) == 0) ||
        (sub(frame_type, RX_NO_DATA) == 0) ||
        (sub(frame_type, RX_ONSET) == 0))
    {
       bfi = 1;
       if ((sub(frame_type, RX_NO_DATA) == 0) ||
           (sub(frame_type, RX_ONSET) == 0))
       {
      build_CN_param(&st->nodataSeed,
             prmno[mode],
             bitno[mode],
             parm);
       }
    }
    else if (sub(frame_type, RX_SPEECH_DEGRADED) == 0)
    {
       pdfi = 1;
    }

    if (bfi != 0)
    {
        st->state = add (st->state, 1);
    }
    else if (sub (st->state, 6) == 0)

    {
        st->state = 5;
    }
    else
    {
        st->state = 0;
    }

    if (sub (st->state, 6) > 0)
    {
        st->state = 6;
    }

    // If this frame is the first speech frame after CNI period,
    // set the BFH state machine to an appropriate state depending
    // on whether there was DTX muting before start of speech or not
    // If there was DTX muting, the first speech frame is muted.
    // If there was no DTX muting, the first speech frame is not
    // muted. The BFH state machine starts from state 5, however, to
    // keep the audible noise resulting from a SID frame which is
    // erroneously interpreted as a good speech frame as small as
    // possible (the decoder output in this case is quickly muted)

    if (sub(st->dtxDecoderState->dtxGlobalState, DTX) == 0)
    {
       st->state = 5;
       st->prev_bf = 0;
    }
    else if (sub(st->dtxDecoderState->dtxGlobalState, DTX_MUTE) == 0)
    {
       st->state = 5;
       st->prev_bf = 1;
    }

    // save old LSFs for CB gain smoothing
    Copy (st->lsfState->past_lsf_q, prev_lsf, M);

    // decode LSF parameters and generate interpolated lpc coefficients
       for the 4 subframes
    if (sub (mode, MR122) != 0)
    {
       D_plsf_3(st->lsfState, mode, bfi, parm, lsp_new);

       // Advance synthesis parameters pointer
       parm += 3;

       Int_lpc_1to3(st->lsp_old, lsp_new, A_t);
    }
    else
    {
       D_plsf_5 (st->lsfState, bfi, parm, lsp_mid, lsp_new);

       // Advance synthesis parameters pointer
       parm += 5;

       Int_lpc_1and3 (st->lsp_old, lsp_mid, lsp_new, A_t);
    }

    // update the LSPs for the next frame
    for (i = 0; i < M; i++)
    {
       st->lsp_old[i] = lsp_new[i];
    }

    *------------------------------------------------------------------------*
    *          Loop for every subframe in the analysis frame                 *
    *------------------------------------------------------------------------*
    * The subframe size is L_SUBFR and the loop is repeated L_FRAME/L_SUBFR  *
    *  times                                                                 *
    *     - decode the pitch delay                                           *
    *     - decode algebraic code                                            *
    *     - decode pitch and codebook gains                                  *
    *     - find the excitation and compute synthesis speech                 *
    *------------------------------------------------------------------------*

    // pointer to interpolated LPC parameters
    Az = A_t;

    evenSubfr = 0;
    subfrNr = -1;
    for (i_subfr = 0; i_subfr < L_FRAME; i_subfr += L_SUBFR)
    {
       subfrNr = add(subfrNr, 1);
       evenSubfr = sub(1, evenSubfr);

       // flag for first and 3th subframe
       pit_flag = i_subfr;

       if (sub (i_subfr, L_FRAME_BY2) == 0)
       {
          if (sub(mode, MR475) != 0 && sub(mode, MR515) != 0)
          {
             pit_flag = 0;
          }
       }

       // pitch index
       index = *parm++;

        *-------------------------------------------------------*
        * - decode pitch lag and find adaptive codebook vector. *
        *-------------------------------------------------------*

       if (sub(mode, MR122) != 0)
       {
          // flag4 indicates encoding with 4 bit resolution;
          // this is needed for mode MR475, MR515, MR59 and MR67

          flag4 = 0;
          if ((sub (mode, MR475) == 0) ||
              (sub (mode, MR515) == 0) ||
              (sub (mode, MR59) == 0) ||
              (sub (mode, MR67) == 0) ) {
             flag4 = 1;
          }

           *-------------------------------------------------------*
           * - get ranges for the t0_min and t0_max                *
           * - only needed in delta decoding                       *
           *-------------------------------------------------------*

          delta_frc_low = 5;
          delta_frc_range = 9;

          if ( sub(mode, MR795) == 0 )
          {
             delta_frc_low = 10;
             delta_frc_range = 19;
          }

          t0_min = sub(st->old_T0, delta_frc_low);
          if (sub(t0_min, PIT_MIN) < 0)
          {
             t0_min = PIT_MIN;
          }
          t0_max = add(t0_min, delta_frc_range);
          if (sub(t0_max, PIT_MAX) > 0)
          {
             t0_max = PIT_MAX;
             t0_min = sub(t0_max, delta_frc_range);
          }

          Dec_lag3 (index, t0_min, t0_max, pit_flag, st->old_T0,
                    &T0, &T0_frac, flag4);

          st->T0_lagBuff = T0;

          if (bfi != 0)
          {
             if (sub (st->old_T0, PIT_MAX) < 0)
             {                                      // Graceful pitch
                st->old_T0 = add(st->old_T0, 1);    // degradation
             }
             T0 = st->old_T0;
             T0_frac = 0;

             if ( st->inBackgroundNoise != 0 &&
                  sub(st->voicedHangover, 4) > 0 &&
                  ((sub(mode, MR475) == 0 ) ||
                   (sub(mode, MR515) == 0 ) ||
                   (sub(mode, MR59) == 0) )
                  )
             {
                T0 = st->T0_lagBuff;
             }
          }

          Pred_lt_3or6 (st->exc, T0, T0_frac, L_SUBFR, 1);
       }
       else
       {
          Dec_lag6 (index, PIT_MIN_MR122,
                    PIT_MAX, pit_flag, &T0, &T0_frac);

          if ( bfi == 0 && (pit_flag == 0 || sub (index, 61) < 0))
          {
          }
          else
          {
             st->T0_lagBuff = T0;
             T0 = st->old_T0;
             T0_frac = 0;
          }

          Pred_lt_3or6 (st->exc, T0, T0_frac, L_SUBFR, 0);
       }

        *-------------------------------------------------------*
        * - (MR122 only: Decode pitch gain.)                    *
        * - Decode innovative codebook.                         *
        * - set pitch sharpening factor                         *
        *-------------------------------------------------------*

        if (sub (mode, MR475) == 0 || sub (mode, MR515) == 0)
        {   // MR475, MR515
           index = *parm++;        // index of position
           i = *parm++;            // signs

           decode_2i40_9bits (subfrNr, i, index, code);

           pit_sharp = shl (st->sharp, 1);
        }
        else if (sub (mode, MR59) == 0)
        {   // MR59
           index = *parm++;        // index of position
           i = *parm++;            // signs

           decode_2i40_11bits (i, index, code);

           pit_sharp = shl (st->sharp, 1);
        }
        else if (sub (mode, MR67) == 0)
        {   // MR67
           index = *parm++;        // index of position
           i = *parm++;            // signs

           decode_3i40_14bits (i, index, code);

           pit_sharp = shl (st->sharp, 1);
        }
        else if (sub (mode, MR795) <= 0)
        {   // MR74, MR795
           index = *parm++;        // index of position
           i = *parm++;            // signs

           decode_4i40_17bits (i, index, code);

           pit_sharp = shl (st->sharp, 1);
        }
        else if (sub (mode, MR102) == 0)
        {  // MR102
           dec_8i40_31bits (parm, code);
           parm += 7;

           pit_sharp = shl (st->sharp, 1);
        }
        else
        {  // MR122
           index = *parm++;
           if (bfi != 0)
           {
              ec_gain_pitch (st->ec_gain_p_st, st->state, &gain_pit);
           }
           else
           {
              gain_pit = d_gain_pitch (mode, index);
           }
           ec_gain_pitch_update (st->ec_gain_p_st, bfi, st->prev_bf,
                                 &gain_pit);

           dec_10i40_35bits (parm, code);
           parm += 10;

           // pit_sharp = gain_pit;
           // if (pit_sharp > 1.0) pit_sharp = 1.0;

           pit_sharp = shl (gain_pit, 1);
        }

         *-------------------------------------------------------*
         * - Add the pitch contribution to code[].               *
         *-------------------------------------------------------*
        for (i = T0; i < L_SUBFR; i++)
        {
           temp = mult (code[i - T0], pit_sharp);
           code[i] = add (code[i], temp);
        }

         *------------------------------------------------------------*
         * - Decode codebook gain (MR122) or both pitch               *
         *   gain and codebook gain (all others)                      *
         * - Update pitch sharpening "sharp" with quantized gain_pit  *
         *------------------------------------------------------------*

        if (sub (mode, MR475) == 0)
        {
           // read and decode pitch and code gain
           if (evenSubfr != 0)
           {
              index_mr475 = *parm++; // index of gain(s)
           }

           if (bfi == 0)
           {
              Dec_gain(st->pred_state, mode, index_mr475, code,
                       evenSubfr, &gain_pit, &gain_code);
           }
           else
           {
              ec_gain_pitch (st->ec_gain_p_st, st->state, &gain_pit);
              ec_gain_code (st->ec_gain_c_st, st->pred_state, st->state,
                            &gain_code);
           }
           ec_gain_pitch_update (st->ec_gain_p_st, bfi, st->prev_bf,
                                 &gain_pit);
           ec_gain_code_update (st->ec_gain_c_st, bfi, st->prev_bf,
                                &gain_code);

           pit_sharp = gain_pit;
           if (sub (pit_sharp, SHARPMAX) > 0)
           {
               pit_sharp = SHARPMAX;
           }

        }
        else if ((sub (mode, MR74) <= 0) ||
                 (sub (mode, MR102) == 0))
        {
            // read and decode pitch and code gain
            index = *parm++; // index of gain(s)

            if (bfi == 0)
            {
               Dec_gain(st->pred_state, mode, index, code,
                        evenSubfr, &gain_pit, &gain_code);
            }
            else
            {
               ec_gain_pitch (st->ec_gain_p_st, st->state, &gain_pit);
               ec_gain_code (st->ec_gain_c_st, st->pred_state, st->state,
                             &gain_code);
            }
            ec_gain_pitch_update (st->ec_gain_p_st, bfi, st->prev_bf,
                                  &gain_pit);
            ec_gain_code_update (st->ec_gain_c_st, bfi, st->prev_bf,
                                 &gain_code);

            pit_sharp = gain_pit;
            if (sub (pit_sharp, SHARPMAX) > 0)
            {
               pit_sharp = SHARPMAX;
            }

            if (sub (mode, MR102) == 0)
            {
               if (sub (st->old_T0, add(L_SUBFR, 5)) > 0)
               {
                  pit_sharp = shr(pit_sharp, 2);
               }
            }
        }
        else
        {
           // read and decode pitch gain
           index = *parm++; // index of gain(s)

           if (sub (mode, MR795) == 0)
           {
              // decode pitch gain
              if (bfi != 0)
              {
                 ec_gain_pitch (st->ec_gain_p_st, st->state, &gain_pit);
              }
              else
              {
                 gain_pit = d_gain_pitch (mode, index);
              }
              ec_gain_pitch_update (st->ec_gain_p_st, bfi, st->prev_bf,
                                    &gain_pit);

              // read and decode code gain
              index = *parm++;
              if (bfi == 0)
              {
                 d_gain_code (st->pred_state, mode, index, code, &gain_code);
              }
              else
              {
                 ec_gain_code (st->ec_gain_c_st, st->pred_state, st->state,
                               &gain_code);
              }
              ec_gain_code_update (st->ec_gain_c_st, bfi, st->prev_bf,
                                   &gain_code);

              pit_sharp = gain_pit;
              if (sub (pit_sharp, SHARPMAX) > 0)
              {
                 pit_sharp = SHARPMAX;
              }
           }
           else
           { // MR122
              if (bfi == 0)
              {
                 d_gain_code (st->pred_state, mode, index, code, &gain_code);
              }
              else
              {
                 ec_gain_code (st->ec_gain_c_st, st->pred_state, st->state,
                               &gain_code);
              }
              ec_gain_code_update (st->ec_gain_c_st, bfi, st->prev_bf,
                                   &gain_code);

              pit_sharp = gain_pit;
           }
        }

        // store pitch sharpening for next subframe
        // (for modes which use the previous pitch gain for
        // pitch sharpening in the search phase)
        // do not update sharpening in even subframes for MR475
        if (sub(mode, MR475) != 0 || evenSubfr == 0)
        {
            st->sharp = gain_pit;
            if (sub (st->sharp, SHARPMAX) > 0)
            {
                st->sharp = SHARPMAX;
            }
        }

        pit_sharp = shl (pit_sharp, 1);
        if (sub (pit_sharp, 16384) > 0)
        {
           for (i = 0; i < L_SUBFR; i++)
            {
               temp = mult (st->exc[i], pit_sharp);
               L_temp = L_mult (temp, gain_pit);
               if (sub(mode, MR122)==0)
               {
                  L_temp = L_shr (L_temp, 1);
               }
               excp[i] = pv_round (L_temp);
            }
        }

         *-------------------------------------------------------*
         * - Store list of LTP gains needed in the source        *
         *   characteristic detector (SCD)                       *
         *-------------------------------------------------------*
        if ( bfi == 0 )
        {
           for (i = 0; i < 8; i++)
           {
              st->ltpGainHistory[i] = st->ltpGainHistory[i+1];
           }
           st->ltpGainHistory[8] = gain_pit;
        }

         *-------------------------------------------------------*
         * - Limit gain_pit if in background noise and BFI       *
         *   for MR475, MR515, MR59                              *
         *-------------------------------------------------------*

        if ( (st->prev_bf != 0 || bfi != 0) && st->inBackgroundNoise != 0 &&
             ((sub(mode, MR475) == 0) ||
              (sub(mode, MR515) == 0) ||
              (sub(mode, MR59) == 0))
             )
        {
           if ( sub (gain_pit, 12288) > 0)    // if (gain_pit > 0.75) in Q14
              gain_pit = add( shr( sub(gain_pit, 12288), 1 ), 12288 );
              // gain_pit = (gain_pit-0.75)/2.0 + 0.75;

           if ( sub (gain_pit, 14745) > 0)    // if (gain_pit > 0.90) in Q14
           {
              gain_pit = 14745;
           }
        }

         *-------------------------------------------------------*
         *  Calculate CB mixed gain                              *
         *-------------------------------------------------------*
        Int_lsf(prev_lsf, st->lsfState->past_lsf_q, i_subfr, lsf_i);
        gain_code_mix = Cb_gain_average(
            st->Cb_gain_averState, mode, gain_code,
            lsf_i, st->lsp_avg_st->lsp_meanSave, bfi,
            st->prev_bf, pdfi, st->prev_pdf,
            st->inBackgroundNoise, st->voicedHangover);

        // make sure that MR74, MR795, MR122 have original code_gain
        if ((sub(mode, MR67) > 0) && (sub(mode, MR102) != 0) )
           // MR74, MR795, MR122
        {
           gain_code_mix = gain_code;
        }

         *-------------------------------------------------------*
         * - Find the total excitation.                          *
         * - Find synthesis speech corresponding to st->exc[].   *
         *-------------------------------------------------------*
        if (sub(mode, MR102) <= 0) // MR475, MR515, MR59, MR67, MR74, MR795, MR102
        {
           pitch_fac = gain_pit;
           tmp_shift = 1;
        }
        else       // MR122
        {
           pitch_fac = shr (gain_pit, 1);
           tmp_shift = 2;
        }

        // copy unscaled LTP excitation to exc_enhanced (used in phase
         * dispersion below) and compute total excitation for LTP feedback

        for (i = 0; i < L_SUBFR; i++)
        {
           exc_enhanced[i] = st->exc[i];

           // st->exc[i] = gain_pit*st->exc[i] + gain_code*code[i];
           L_temp = L_mult (st->exc[i], pitch_fac);
                                                      // 12.2: Q0 * Q13
                                                      //  7.4: Q0 * Q14
           L_temp = L_mac (L_temp, code[i], gain_code);
                                                      // 12.2: Q12 * Q1
                                                      //  7.4: Q13 * Q1
           L_temp = L_shl (L_temp, tmp_shift);                   // Q16
           st->exc[i] = pv_round (L_temp);
        }

         *-------------------------------------------------------*
         * - Adaptive phase dispersion                           *
         *-------------------------------------------------------*
        ph_disp_release(st->ph_disp_st); // free phase dispersion adaption

        if ( ((sub(mode, MR475) == 0) ||
              (sub(mode, MR515) == 0) ||
              (sub(mode, MR59) == 0))   &&
             sub(st->voicedHangover, 3) > 0 &&
             st->inBackgroundNoise != 0 &&
             bfi != 0 )
        {
           ph_disp_lock(st->ph_disp_st); // Always Use full Phase Disp.
        }                                // if error in bg noise

        // apply phase dispersion to innovation (if enabled) and
           compute total excitation for synthesis part
        ph_disp(st->ph_disp_st, mode,
                exc_enhanced, gain_code_mix, gain_pit, code,
                pitch_fac, tmp_shift);

         *-------------------------------------------------------*
         * - The Excitation control module are active during BFI.*
         * - Conceal drops in signal energy if in bg noise.      *
         *-------------------------------------------------------*

        L_temp = 0;
        for (i = 0; i < L_SUBFR; i++)
        {
            L_temp = L_mac (L_temp, exc_enhanced[i], exc_enhanced[i] );
        }

        L_temp = L_shr (L_temp, 1);     // excEnergy = sqrt(L_temp) in Q0
        L_temp = sqrt_l_exp(L_temp, &temp); // function result
        L_temp = L_shr(L_temp, add( shr(temp, 1), 15));
        L_temp = L_shr(L_temp, 2);       // To cope with 16-bit and
        excEnergy = extract_l(L_temp);   // scaling in ex_ctrl()

        if ( ((sub (mode, MR475) == 0) ||
              (sub (mode, MR515) == 0) ||
              (sub (mode, MR59) == 0))  &&
             sub(st->voicedHangover, 5) > 0 &&
             st->inBackgroundNoise != 0 &&
             sub(st->state, 4) < 0 &&
             ( (pdfi != 0 && st->prev_pdf != 0) ||
                bfi != 0 ||
                st->prev_bf != 0) )
        {
           carefulFlag = 0;
           if ( pdfi != 0 && bfi == 0 )
           {
              carefulFlag = 1;
           }

           Ex_ctrl(exc_enhanced,
                   excEnergy,
                   st->excEnergyHist,
                   st->voicedHangover,
                   st->prev_bf,
                   carefulFlag);
        }

        if ( st->inBackgroundNoise != 0 &&
             ( bfi != 0 || st->prev_bf != 0 ) &&
             sub(st->state, 4) < 0 )
        {
           ; // do nothing!
        }
        else
        {
           // Update energy history for all modes
           for (i = 0; i < 8; i++)
           {
              st->excEnergyHist[i] = st->excEnergyHist[i+1];
           }
           st->excEnergyHist[8] = excEnergy;
        }
         *-------------------------------------------------------*
         * Excitation control module end.                        *
         *-------------------------------------------------------*

        if (sub (pit_sharp, 16384) > 0)
        {
           for (i = 0; i < L_SUBFR; i++)
           {
              excp[i] = add (excp[i], exc_enhanced[i]);
           }
           agc2 (exc_enhanced, excp, L_SUBFR);
           Overflow = 0;
           Syn_filt (Az, excp, &synth[i_subfr], L_SUBFR,
                     st->mem_syn, 0);
        }
        else
        {
           Overflow = 0;
           Syn_filt (Az, exc_enhanced, &synth[i_subfr], L_SUBFR,
                     st->mem_syn, 0);
        }

        if (Overflow != 0)    // Test for overflow
        {
           for (i = 0; i < PIT_MAX + L_INTERPOL + L_SUBFR; i++)
           {
              st->old_exc[i] = shr(st->old_exc[i], 2);
           }
           for (i = 0; i < L_SUBFR; i++)
           {
              exc_enhanced[i] = shr(exc_enhanced[i], 2);
           }
           Syn_filt(Az, exc_enhanced, &synth[i_subfr], L_SUBFR, st->mem_syn, 1);
        }
        else
        {
           Copy(&synth[i_subfr+L_SUBFR-M], st->mem_syn, M);
        }

         *--------------------------------------------------*
         * Update signal for next frame.                    *
         * -> shift to the left by L_SUBFR  st->exc[]       *
         *--------------------------------------------------*

        Copy (&st->old_exc[L_SUBFR], &st->old_exc[0], PIT_MAX + L_INTERPOL);

        // interpolated LPC parameters for next subframe
        Az += MP1;

        // store T0 for next subframe
        st->old_T0 = T0;
    }

     *-------------------------------------------------------*
     * Call the Source Characteristic Detector which updates *
     * st->inBackgroundNoise and st->voicedHangover.         *
     *-------------------------------------------------------*

    st->inBackgroundNoise = Bgn_scd(st->background_state,
                                    &(st->ltpGainHistory[0]),
                                    &(synth[0]),
                                    &(st->voicedHangover) );

    dtx_dec_activity_update(st->dtxDecoderState,
                            st->lsfState->past_lsf_q,
                            synth);

    // store bfi for next subframe
    st->prev_bf = bfi;
    st->prev_pdf = pdfi;

     *--------------------------------------------------*
     * Calculate the LSF averages on the eight          *
     * previous frames                                  *
     *--------------------------------------------------*

    lsp_avg(st->lsp_avg_st, st->lsfState->past_lsf_q);

the_end:
    st->dtxDecoderState->dtxGlobalState = newDTXState;

    return 0;
}

------------------------------------------------------------------------------
 CAUTION [optional]
 [State any special notes, constraints or cautions for users of this function]

------------------------------------------------------------------------------
*/

void Decoder_amr(
    Decoder_amrState *st,      /* i/o : State variables                   */
    enum Mode mode,            /* i   : AMR mode                          */
    Word16 parm[],             /* i   : vector of synthesis parameters
                                        (PRM_SIZE)                        */
    enum RXFrameType frame_type, /* i   : received frame type             */
    Word16 synth[],            /* o   : synthesis speech (L_FRAME)        */
    Word16 A_t[]               /* o   : decoded LP filter in 4 subframes
                                        (AZ_SIZE)                         */
)
{
    /* LPC coefficients */

    Word16 *Az;                /* Pointer on A_t */

    /* LSPs */

    Word16 lsp_new[M];
    Word16 lsp_mid[M];

    /* LSFs */

    Word16 prev_lsf[M];
    Word16 lsf_i[M];

    /* Algebraic codevector */

    Word16 code[L_SUBFR];

    /* excitation */

    Word16 excp[L_SUBFR];
    Word16 exc_enhanced[L_SUBFR];

    /* Scalars */

    Word16 i;
    Word16 i_subfr;
    Word16 T0;
    Word16 T0_frac;
    Word16 index;
    Word16 index_mr475 = 0;
    Word16 gain_pit;
    Word16 gain_code;
    Word16 gain_code_mix;
    Word16 pit_sharp;
    Word16 pit_flag;
    Word16 pitch_fac;
    Word16 t0_min;
    Word16 t0_max;
    Word16 delta_frc_low;
    Word16 delta_frc_range;
    Word16 tmp_shift;
    Word16 temp;
    Word32 L_temp;
    Word16 flag4;
    Word16 carefulFlag;
    Word16 excEnergy;
    Word16 subfrNr;
    Word16 evenSubfr = 0;

    Word16 bfi = 0;   /* bad frame indication flag                          */
    Word16 pdfi = 0;  /* potential degraded bad frame flag                  */

    enum DTXStateType newDTXState;  /* SPEECH , DTX, DTX_MUTE */
    Flag   *pOverflow = &(st->overflow);     /* Overflow flag            */


    /* find the new  DTX state  SPEECH OR DTX */
    newDTXState = rx_dtx_handler(&(st->dtxDecoderState), frame_type, pOverflow);

    /* DTX actions */

    if (newDTXState != SPEECH)
    {
        Decoder_amr_reset(st, MRDTX);

        dtx_dec(&(st->dtxDecoderState),
                st->mem_syn,
                &(st->lsfState),
                &(st->pred_state),
                &(st->Cb_gain_averState),
                newDTXState,
                mode,
                parm, &(st->common_amr_tbls), synth, A_t, pOverflow);

        /* update average lsp */
        Lsf_lsp(
            st->lsfState.past_lsf_q,
            st->lsp_old,
            M,
            pOverflow);

        lsp_avg(
            &(st->lsp_avg_st),
            st->lsfState.past_lsf_q,
            pOverflow);

        goto the_end;
    }

    /* SPEECH action state machine  */
    if ((frame_type == RX_SPEECH_BAD) || (frame_type == RX_NO_DATA) ||
            (frame_type == RX_ONSET))
    {
        bfi = 1;

        if ((frame_type == RX_NO_DATA) || (frame_type == RX_ONSET))
        {
            build_CN_param(&st->nodataSeed,
                           st->common_amr_tbls.prmno_ptr[mode],
                           st->common_amr_tbls.bitno_ptr[mode],
                           parm,
                           st->common_amr_tbls.window_200_40_ptr,
                           pOverflow);
        }
    }
    else if (frame_type == RX_SPEECH_DEGRADED)
    {
        pdfi = 1;
    }

    if (bfi != 0)
    {
        st->state += 1;
    }
    else if (st->state == 6)

    {
        st->state = 5;
    }
    else
    {
        st->state = 0;
    }


    if (st->state > 6)
    {
        st->state = 6;
    }

    /* If this frame is the first speech frame after CNI period,     */
    /* set the BFH state machine to an appropriate state depending   */
    /* on whether there was DTX muting before start of speech or not */
    /* If there was DTX muting, the first speech frame is muted.     */
    /* If there was no DTX muting, the first speech frame is not     */
    /* muted. The BFH state machine starts from state 5, however, to */
    /* keep the audible noise resulting from a SID frame which is    */
    /* erroneously interpreted as a good speech frame as small as    */
    /* possible (the decoder output in this case is quickly muted)   */

    if (st->dtxDecoderState.dtxGlobalState == DTX)
    {
        st->state = 5;
        st->prev_bf = 0;
    }
    else if (st->dtxDecoderState.dtxGlobalState == DTX_MUTE)
    {
        st->state = 5;
        st->prev_bf = 1;
    }

    /* save old LSFs for CB gain smoothing */
    oscl_memmove((void *)prev_lsf, st->lsfState.past_lsf_q, M*sizeof(*st->lsfState.past_lsf_q));

    /* decode LSF parameters and generate interpolated lpc coefficients
       for the 4 subframes */

    if (mode != MR122)
    {
        D_plsf_3(
            &(st->lsfState),
            mode,
            bfi,
            parm,
            &st->common_amr_tbls,
            lsp_new,
            pOverflow);

        /* Advance synthesis parameters pointer */
        parm += 3;

        Int_lpc_1to3(
            st->lsp_old,
            lsp_new,
            A_t,
            pOverflow);
    }
    else
    {
        D_plsf_5(
            &(st->lsfState),
            bfi,
            parm,
            &(st->common_amr_tbls),
            lsp_mid,
            lsp_new,
            pOverflow);

        /* Advance synthesis parameters pointer */
        parm += 5;

        Int_lpc_1and3(
            st->lsp_old,
            lsp_mid,
            lsp_new,
            A_t,
            pOverflow);
    }

    /* update the LSPs for the next frame */
    for (i = 0; i < M; i++)
    {
        st->lsp_old[i] = lsp_new[i];
    }

    /*------------------------------------------------------------------------*
     *          Loop for every subframe in the analysis frame                 *
     *------------------------------------------------------------------------*
     * The subframe size is L_SUBFR and the loop is repeated L_FRAME/L_SUBFR  *
     *  times                                                                 *
     *     - decode the pitch delay                                           *
     *     - decode algebraic code                                            *
     *     - decode pitch and codebook gains                                  *
     *     - find the excitation and compute synthesis speech                 *
     *------------------------------------------------------------------------*/

    /* pointer to interpolated LPC parameters */
    Az = A_t;

    evenSubfr = 0;
    subfrNr = -1;
    for (i_subfr = 0; i_subfr < L_FRAME; i_subfr += L_SUBFR)
    {
        subfrNr += 1;
        evenSubfr = 1 - evenSubfr;

        /* flag for first and 3th subframe */
        pit_flag = i_subfr;


        if (i_subfr == L_FRAME_BY2)
        {
            if ((mode != MR475) && (mode != MR515))
            {
                pit_flag = 0;
            }
        }

        /* pitch index */
        index = *parm++;

        /*-------------------------------------------------------*
        * - decode pitch lag and find adaptive codebook vector. *
        *-------------------------------------------------------*/

        if (mode != MR122)
        {
            /* flag4 indicates encoding with 4 bit resolution;     */
            /* this is needed for mode MR475, MR515, MR59 and MR67 */

            flag4 = 0;

            if ((mode == MR475) || (mode == MR515) || (mode == MR59) ||
                    (mode == MR67))
            {
                flag4 = 1;
            }

            /*-------------------------------------------------------*
            * - get ranges for the t0_min and t0_max                *
            * - only needed in delta decoding                       *
            *-------------------------------------------------------*/

            delta_frc_low = 5;
            delta_frc_range = 9;

            if (mode == MR795)
            {
                delta_frc_low = 10;
                delta_frc_range = 19;
            }

            t0_min = st->old_T0 - delta_frc_low;

            if (t0_min < PIT_MIN)
            {
                t0_min = PIT_MIN;
            }
            t0_max = t0_min + delta_frc_range;

            if (t0_max > PIT_MAX)
            {
                t0_max = PIT_MAX;
                t0_min = t0_max - delta_frc_range;
            }

            Dec_lag3(index, t0_min, t0_max, pit_flag, st->old_T0,
                     &T0, &T0_frac, flag4, pOverflow);

            st->T0_lagBuff = T0;

            if (bfi != 0)
            {
                if (st->old_T0 < PIT_MAX)
                {                               /* Graceful pitch */
                    st->old_T0 += 1;            /* degradation    */
                }
                T0 = st->old_T0;
                T0_frac = 0;

                if ((st->inBackgroundNoise != 0) && (st->voicedHangover > 4) &&
                        ((mode == MR475) || (mode == MR515) || (mode == MR59)))
                {
                    T0 = st->T0_lagBuff;
                }
            }

            Pred_lt_3or6(st->exc, T0, T0_frac, L_SUBFR, 1, pOverflow);
        }
        else
        {
            Dec_lag6(index, PIT_MIN_MR122,
                     PIT_MAX, pit_flag, &T0, &T0_frac, pOverflow);


            if (!(bfi == 0 && (pit_flag == 0 || index < 61)))
            {
                st->T0_lagBuff = T0;
                T0 = st->old_T0;
                T0_frac = 0;
            }

            Pred_lt_3or6(st->exc, T0, T0_frac, L_SUBFR, 0, pOverflow);
        }

        /*-------------------------------------------------------*
         * - (MR122 only: Decode pitch gain.)                    *
         * - Decode innovative codebook.                         *
         * - set pitch sharpening factor                         *
         *-------------------------------------------------------*/
        if ((mode == MR475) || (mode == MR515))
        {   /* MR475, MR515 */
            index = *parm++;        /* index of position */
            i = *parm++;            /* signs             */

            decode_2i40_9bits(subfrNr, i, index, st->common_amr_tbls.startPos_ptr, code, pOverflow);

            L_temp = (Word32)st->sharp << 1;
            if (L_temp != (Word32)((Word16) L_temp))
            {
                pit_sharp = (st->sharp > 0) ? MAX_16 : MIN_16;
            }
            else
            {
                pit_sharp = (Word16) L_temp;
            }
        }
        else if (mode == MR59)
        {   /* MR59 */
            index = *parm++;        /* index of position */
            i = *parm++;            /* signs             */

            decode_2i40_11bits(i, index, code);

            L_temp = (Word32)st->sharp << 1;
            if (L_temp != (Word32)((Word16) L_temp))
            {
                pit_sharp = (st->sharp > 0) ? MAX_16 : MIN_16;
            }
            else
            {
                pit_sharp = (Word16) L_temp;
            }
        }
        else if (mode == MR67)
        {   /* MR67 */
            index = *parm++;        /* index of position */
            i = *parm++;            /* signs             */

            decode_3i40_14bits(i, index, code);

            L_temp = (Word32)st->sharp << 1;
            if (L_temp != (Word32)((Word16) L_temp))
            {
                pit_sharp = (st->sharp > 0) ? MAX_16 : MIN_16;
            }
            else
            {
                pit_sharp = (Word16) L_temp;
            }
        }
        else if (mode <= MR795)
        {   /* MR74, MR795 */
            index = *parm++;        /* index of position */
            i = *parm++;            /* signs             */

            decode_4i40_17bits(i, index, st->common_amr_tbls.dgray_ptr, code);

            L_temp = (Word32)st->sharp << 1;
            if (L_temp != (Word32)((Word16) L_temp))
            {
                pit_sharp = (st->sharp > 0) ? MAX_16 : MIN_16;
            }
            else
            {
                pit_sharp = (Word16) L_temp;
            }
        }
        else if (mode == MR102)
        {  /* MR102 */
            dec_8i40_31bits(parm, code, pOverflow);
            parm += 7;

            L_temp = (Word32)st->sharp << 1;
            if (L_temp != (Word32)((Word16) L_temp))
            {
                pit_sharp = (st->sharp > 0) ? MAX_16 : MIN_16;
            }
            else
            {
                pit_sharp = (Word16) L_temp;
            }
        }
        else
        {  /* MR122 */
            index = *parm++;

            if (bfi != 0)
            {
                ec_gain_pitch(
                    &(st->ec_gain_p_st),
                    st->state,
                    &gain_pit,
                    pOverflow);
            }
            else
            {
                gain_pit = d_gain_pitch(mode, index, st->common_amr_tbls.qua_gain_pitch_ptr);
            }
            ec_gain_pitch_update(
                &(st->ec_gain_p_st),
                bfi,
                st->prev_bf,
                &gain_pit,
                pOverflow);


            dec_10i40_35bits(parm, code, st->common_amr_tbls.dgray_ptr);
            parm += 10;

            /* pit_sharp = gain_pit;                   */
            /* if (pit_sharp > 1.0) pit_sharp = 1.0;   */

            L_temp = (Word32)gain_pit << 1;
            if (L_temp != (Word32)((Word16) L_temp))
            {
                pit_sharp = (gain_pit > 0) ? MAX_16 : MIN_16;
            }
            else
            {
                pit_sharp = (Word16) L_temp;
            }
        }
        /*-------------------------------------------------------*
         * - Add the pitch contribution to code[].               *
         *-------------------------------------------------------*/
        for (i = T0; i < L_SUBFR; i++)
        {
            temp = mult(*(code + i - T0), pit_sharp, pOverflow);
            *(code + i) = add_16(*(code + i), temp, pOverflow);

        }

        /*------------------------------------------------------------*
         * - Decode codebook gain (MR122) or both pitch               *
         *   gain and codebook gain (all others)                      *
         * - Update pitch sharpening "sharp" with quantized gain_pit  *
         *------------------------------------------------------------*/
        if (mode == MR475)
        {
            /* read and decode pitch and code gain */

            if (evenSubfr != 0)
            {
                index_mr475 = *parm++;         /* index of gain(s) */
            }

            if (bfi == 0)
            {
                Dec_gain(
                    &(st->pred_state),
                    mode,
                    index_mr475,
                    code,
                    evenSubfr,
                    &gain_pit,
                    &gain_code,
                    &(st->common_amr_tbls),
                    pOverflow);
            }
            else
            {
                ec_gain_pitch(
                    &(st->ec_gain_p_st),
                    st->state,
                    &gain_pit,
                    pOverflow);

                ec_gain_code(
                    &(st->ec_gain_c_st),
                    &(st->pred_state),
                    st->state,
                    &gain_code,
                    pOverflow);
            }
            ec_gain_pitch_update(
                &st->ec_gain_p_st,
                bfi,
                st->prev_bf,
                &gain_pit,
                pOverflow);

            ec_gain_code_update(
                &st->ec_gain_c_st,
                bfi,
                st->prev_bf,
                &gain_code,
                pOverflow);

            pit_sharp = gain_pit;

            if (pit_sharp > SHARPMAX)
            {
                pit_sharp = SHARPMAX;
            }

        }
        else if ((mode <= MR74) || (mode == MR102))
        {
            /* read and decode pitch and code gain */
            index = *parm++;                 /* index of gain(s) */

            if (bfi == 0)
            {
                Dec_gain(
                    &(st->pred_state),
                    mode,
                    index,
                    code,
                    evenSubfr,
                    &gain_pit,
                    &gain_code,
                    &(st->common_amr_tbls),
                    pOverflow);
            }
            else
            {
                ec_gain_pitch(
                    &(st->ec_gain_p_st),
                    st->state,
                    &gain_pit,
                    pOverflow);

                ec_gain_code(
                    &(st->ec_gain_c_st),
                    &(st->pred_state),
                    st->state,
                    &gain_code,
                    pOverflow);
            }

            ec_gain_pitch_update(
                &(st->ec_gain_p_st),
                bfi,
                st->prev_bf,
                &gain_pit,
                pOverflow);

            ec_gain_code_update(
                &(st->ec_gain_c_st),
                bfi,
                st->prev_bf,
                &gain_code,
                pOverflow);

            pit_sharp = gain_pit;

            if (pit_sharp > SHARPMAX)
            {
                pit_sharp = SHARPMAX;
            }

            if (mode == MR102)
            {
                if (st->old_T0 > (L_SUBFR + 5))
                {
                    if (pit_sharp < 0)
                    {
                        pit_sharp = ~((~pit_sharp) >> 2);
                    }
                    else
                    {
                        pit_sharp = pit_sharp >> 2;
                    }
                }
            }
        }
        else
        {
            /* read and decode pitch gain */
            index = *parm++;                 /* index of gain(s) */

            if (mode == MR795)
            {
                /* decode pitch gain */
                if (bfi != 0)
                {
                    ec_gain_pitch(
                        &(st->ec_gain_p_st),
                        st->state,
                        &gain_pit,
                        pOverflow);
                }
                else
                {
                    gain_pit = d_gain_pitch(mode, index, st->common_amr_tbls.qua_gain_pitch_ptr);
                }
                ec_gain_pitch_update(
                    &(st->ec_gain_p_st),
                    bfi,
                    st->prev_bf,
                    &gain_pit,
                    pOverflow);

                /* read and decode code gain */
                index = *parm++;

                if (bfi == 0)
                {
                    d_gain_code(
                        &(st->pred_state),
                        mode,
                        index,
                        code,
                        st->common_amr_tbls.qua_gain_code_ptr,
                        &gain_code,
                        pOverflow);
                }
                else
                {
                    ec_gain_code(
                        &(st->ec_gain_c_st),
                        &(st->pred_state),
                        st->state,
                        &gain_code,
                        pOverflow);
                }

                ec_gain_code_update(
                    &(st->ec_gain_c_st),
                    bfi,
                    st->prev_bf,
                    &gain_code,
                    pOverflow);

                pit_sharp = gain_pit;

                if (pit_sharp > SHARPMAX)
                {
                    pit_sharp = SHARPMAX;
                }
            }
            else
            { /* MR122 */

                if (bfi == 0)
                {
                    d_gain_code(
                        &(st->pred_state),
                        mode,
                        index,
                        code,
                        st->common_amr_tbls.qua_gain_code_ptr,
                        &gain_code,
                        pOverflow);
                }
                else
                {
                    ec_gain_code(
                        &(st->ec_gain_c_st),
                        &(st->pred_state),
                        st->state,
                        &gain_code,
                        pOverflow);
                }

                ec_gain_code_update(
                    &(st->ec_gain_c_st),
                    bfi,
                    st->prev_bf,
                    &gain_code,
                    pOverflow);

                pit_sharp = gain_pit;
            }
        }

        /* store pitch sharpening for next subframe             */
        /* (for modes which use the previous pitch gain for     */
        /* pitch sharpening in the search phase)                */
        /* do not update sharpening in even subframes for MR475 */
        if ((mode != MR475) || (evenSubfr == 0))
        {
            st->sharp = gain_pit;

            if (st->sharp > SHARPMAX)
            {
                st->sharp = SHARPMAX;
            }
        }

        pit_sharp = shl(pit_sharp, 1, pOverflow);

        if (pit_sharp > 16384)
        {
            for (i = 0; i < L_SUBFR; i++)
            {
                temp = mult(st->exc[i], pit_sharp, pOverflow);
                L_temp = L_mult(temp, gain_pit, pOverflow);

                if (mode == MR122)
                {
                    if (L_temp < 0)
                    {
                        L_temp = ~((~L_temp) >> 1);
                    }
                    else
                    {
                        L_temp = L_temp >> 1;
                    }
                }
                *(excp + i) = pv_round(L_temp, pOverflow);
            }
        }

        /*-------------------------------------------------------*
         * - Store list of LTP gains needed in the source        *
         *   characteristic detector (SCD)                       *
         *-------------------------------------------------------*/

        if (bfi == 0)
        {
            for (i = 0; i < 8; i++)
            {
                st->ltpGainHistory[i] = st->ltpGainHistory[i+1];
            }
            st->ltpGainHistory[8] = gain_pit;
        }

        /*-------------------------------------------------------*
        * - Limit gain_pit if in background noise and BFI       *
        *   for MR475, MR515, MR59                              *
        *-------------------------------------------------------*/


        if ((st->prev_bf != 0 || bfi != 0) && st->inBackgroundNoise != 0 &&
                ((mode == MR475) || (mode == MR515) || (mode == MR59)))
        {

            if (gain_pit > 12288)    /* if (gain_pit > 0.75) in Q14*/
            {
                gain_pit = ((gain_pit - 12288) >> 1) + 12288;
                /* gain_pit = (gain_pit-0.75)/2.0 + 0.75; */
            }

            if (gain_pit > 14745)    /* if (gain_pit > 0.90) in Q14*/
            {
                gain_pit = 14745;
            }
        }

        /*-------------------------------------------------------*
         *  Calculate CB mixed gain                              *
         *-------------------------------------------------------*/
        Int_lsf(
            prev_lsf,
            st->lsfState.past_lsf_q,
            i_subfr,
            lsf_i,
            pOverflow);

        gain_code_mix =
            Cb_gain_average(
                &(st->Cb_gain_averState),
                mode,
                gain_code,
                lsf_i,
                st->lsp_avg_st.lsp_meanSave,
                bfi,
                st->prev_bf,
                pdfi,
                st->prev_pdf,
                st->inBackgroundNoise,
                st->voicedHangover,
                pOverflow);

        /* make sure that MR74, MR795, MR122 have original code_gain*/
        if ((mode > MR67) && (mode != MR102))
            /* MR74, MR795, MR122 */
        {
            gain_code_mix = gain_code;
        }

        /*-------------------------------------------------------*
         * - Find the total excitation.                          *
         * - Find synthesis speech corresponding to st->exc[].   *
         *-------------------------------------------------------*/
        if (mode <= MR102) /* MR475, MR515, MR59, MR67, MR74, MR795, MR102*/
        {
            pitch_fac = gain_pit;
            tmp_shift = 1;
        }
        else       /* MR122 */
        {
            if (gain_pit < 0)
            {
                pitch_fac = ~((~gain_pit) >> 1);
            }
            else
            {
                pitch_fac = gain_pit >> 1;
            }
            tmp_shift = 2;
        }

        /* copy unscaled LTP excitation to exc_enhanced (used in phase
         * dispersion below) and compute total excitation for LTP feedback
         */
        for (i = 0; i < L_SUBFR; i++)
        {
            exc_enhanced[i] = st->exc[i];

            /* st->exc[i] = gain_pit*st->exc[i] + gain_code*code[i]; */
            L_temp = L_mult(st->exc[i], pitch_fac, pOverflow);
            /* 12.2: Q0 * Q13 */
            /*  7.4: Q0 * Q14 */
            L_temp = L_mac(L_temp, code[i], gain_code, pOverflow);
            /* 12.2: Q12 * Q1 */
            /*  7.4: Q13 * Q1 */
            L_temp = L_shl(L_temp, tmp_shift, pOverflow);     /* Q16 */
            st->exc[i] = pv_round(L_temp, pOverflow);
        }

        /*-------------------------------------------------------*
         * - Adaptive phase dispersion                           *
         *-------------------------------------------------------*/
        ph_disp_release(&(st->ph_disp_st)); /* free phase dispersion adaption */


        if (((mode == MR475) || (mode == MR515) || (mode == MR59)) &&
                (st->voicedHangover > 3) && (st->inBackgroundNoise != 0) &&
                (bfi != 0))
        {
            ph_disp_lock(&(st->ph_disp_st)); /* Always Use full Phase Disp. */
        }                                 /* if error in bg noise       */

        /* apply phase dispersion to innovation (if enabled) and
           compute total excitation for synthesis part           */
        ph_disp(
            &(st->ph_disp_st),
            mode,
            exc_enhanced,
            gain_code_mix,
            gain_pit,
            code,
            pitch_fac,
            tmp_shift,
            &(st->common_amr_tbls),
            pOverflow);

        /*-------------------------------------------------------*
         * - The Excitation control module are active during BFI.*
         * - Conceal drops in signal energy if in bg noise.      *
         *-------------------------------------------------------*/
        L_temp = 0;
        for (i = 0; i < L_SUBFR; i++)
        {
            L_temp = L_mac(L_temp, *(exc_enhanced + i), *(exc_enhanced + i), pOverflow);
        }

        /* excEnergy = sqrt(L_temp) in Q0 */
        if (L_temp < 0)
        {
            L_temp = ~((~L_temp) >> 1);
        }
        else
        {
            L_temp = L_temp >> 1;
        }

        L_temp = sqrt_l_exp(L_temp, &temp, pOverflow);
        /* To cope with 16-bit and scaling in ex_ctrl() */
        L_temp = L_shr(L_temp, (Word16)((temp >> 1) + 15), pOverflow);
        if (L_temp < 0)
        {
            excEnergy = (Word16)(~((~L_temp) >> 2));
        }
        else
        {
            excEnergy = (Word16)(L_temp >> 2);
        }

        if (((mode == MR475) || (mode == MR515) || (mode == MR59))  &&
                (st->voicedHangover > 5) && (st->inBackgroundNoise != 0) &&
                (st->state < 4) &&
                ((pdfi != 0 && st->prev_pdf != 0) || bfi != 0 || st->prev_bf != 0))
        {
            carefulFlag = 0;

            if (pdfi != 0 && bfi == 0)
            {
                carefulFlag = 1;
            }

            Ex_ctrl(exc_enhanced,
                    excEnergy,
                    st->excEnergyHist,
                    st->voicedHangover,
                    st->prev_bf,
                    carefulFlag, pOverflow);
        }

        if (!((st->inBackgroundNoise != 0) && (bfi != 0 || st->prev_bf != 0) &&
                (st->state < 4)))
        {
            /* Update energy history for all modes */
            for (i = 0; i < 8; i++)
            {
                st->excEnergyHist[i] = st->excEnergyHist[i+1];
            }
            st->excEnergyHist[8] = excEnergy;
        }
        /*-------------------------------------------------------*
         * Excitation control module end.                        *
         *-------------------------------------------------------*/
        if (pit_sharp > 16384)
        {
            for (i = 0; i < L_SUBFR; i++)
            {
                *(excp + i) = add_16(*(excp + i), *(exc_enhanced + i), pOverflow);

            }
            agc2(exc_enhanced, excp, L_SUBFR, pOverflow);
            *pOverflow = 0;
            Syn_filt(Az, excp, &synth[i_subfr], L_SUBFR,
                     st->mem_syn, 0);
        }
        else
        {
            *pOverflow = 0;
            Syn_filt(Az, exc_enhanced, &synth[i_subfr], L_SUBFR,
                     st->mem_syn, 0);
        }

        if (*pOverflow != 0)    /* Test for overflow */
        {
            for (i = PIT_MAX + L_INTERPOL + L_SUBFR - 1; i >= 0; i--)
            {
                if (st->old_exc[i] < 0)
                {
                    st->old_exc[i] = ~((~st->old_exc[i]) >> 2);
                }
                else
                {
                    st->old_exc[i] = st->old_exc[i] >> 2;
                }

            }

            for (i = L_SUBFR - 1; i >= 0; i--)
            {
                if (*(exc_enhanced + i) < 0)
                {
                    *(exc_enhanced + i) = ~((~(*(exc_enhanced + i))) >> 2);
                }
                else
                {
                    *(exc_enhanced + i) = *(exc_enhanced + i) >> 2;
                }
            }
            Syn_filt(Az, exc_enhanced, &synth[i_subfr], L_SUBFR, st->mem_syn, 1);
        }
        else
        {
            oscl_memmove((void *)st->mem_syn, &synth[i_subfr+L_SUBFR-M], M*sizeof(synth[0]));
        }

        /*--------------------------------------------------*
         * Update signal for next frame.                    *
         * -> shift to the left by L_SUBFR  st->exc[]       *
         *--------------------------------------------------*/

        oscl_memmove((void *)&st->old_exc[0], &st->old_exc[L_SUBFR], (PIT_MAX + L_INTERPOL)*sizeof(st->old_exc[0]));

        /* interpolated LPC parameters for next subframe */
        Az += MP1;

        /* store T0 for next subframe */
        st->old_T0 = T0;
    }

    /*-------------------------------------------------------*
     * Call the Source Characteristic Detector which updates *
     * st->inBackgroundNoise and st->voicedHangover.         *
     *-------------------------------------------------------*/

    st->inBackgroundNoise =
        Bgn_scd(
            &(st->background_state),
            &(st->ltpGainHistory[0]),
            &(synth[0]),
            &(st->voicedHangover),
            pOverflow);

    dtx_dec_activity_update(
        &(st->dtxDecoderState),
        st->lsfState.past_lsf_q,
        synth,
        pOverflow);

    /* store bfi for next subframe */
    st->prev_bf = bfi;
    st->prev_pdf = pdfi;

    /*--------------------------------------------------*
     * Calculate the LSF averages on the eight          *
     * previous frames                                  *
     *--------------------------------------------------*/
    lsp_avg(
        &(st->lsp_avg_st),
        st->lsfState.past_lsf_q,
        pOverflow);

the_end:
    st->dtxDecoderState.dtxGlobalState = newDTXState;

//    return(0);
}
