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



 Filename: dtx_enc.cpp
 Functions: dtx_enc_init
           dtx_enc_reset
           dtx_enc_exit
           dtx_enc
           dtx_buffer
           tx_dtx_handler

------------------------------------------------------------------------------
 MODULE DESCRIPTION

 This file contains the various functions that perform the computation of the
 Silence Indicator (SID) parameters when in Discontinuous Transmission (DTX)
 mode.

------------------------------------------------------------------------------
*/


/*----------------------------------------------------------------------------
; INCLUDES
----------------------------------------------------------------------------*/
#include "dtx_enc.h"
#include "q_plsf.h"
#include "typedef.h"
#include "mode.h"
#include "basic_op.h"
#include "log2.h"
#include "lsp_lsf.h"
#include "reorder.h"
#include "oscl_mem.h"

/*----------------------------------------------------------------------------
; MACROS
; Define module specific macros here
----------------------------------------------------------------------------*/
extern Word32 L_add(register Word32 L_var1, register Word32 L_var2, Flag *pOverflow);

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
 FUNCTION NAME: dtx_enc_init
------------------------------------------------------------------------------
 INPUT AND OUTPUT DEFINITIONS

 Inputs:
    st = pointer to an array of pointers to structures of type
         dtx_encState

 Outputs:
    pointer pointed to by st is set to the address of the allocated
      memory

 Returns:
    return_value = 0, if initialization was successful; -1, otherwise (int)

 Global Variables Used:
    None

 Local Variables Needed:
    None

------------------------------------------------------------------------------
 FUNCTION DESCRIPTION

 This function allocates the state memory used by the dtx_enc function.

------------------------------------------------------------------------------
 REQUIREMENTS

 None

------------------------------------------------------------------------------
 REFERENCES

 dtx_enc.c, UMTS GSM AMR speech codec, R99 - Version 3.2.0, March 2, 2001

------------------------------------------------------------------------------
 PSEUDO-CODE

int dtx_enc_init (dtx_encState **st)
{
  dtx_encState* s;

  if (st == (dtx_encState **) NULL){
    fprintf(stderr, "dtx_enc_init: invalid parameter\n");
    return -1;
  }

  *st = NULL;

  // allocate memory
  if ((s= (dtx_encState *) malloc(sizeof(dtx_encState))) == NULL){
    fprintf(stderr, "dtx_enc_init: can not malloc state structure\n");
    return -1;
  }

  dtx_enc_reset(s);
  *st = s;

  return 0;
}

------------------------------------------------------------------------------
 CAUTION [optional]
 [State any special notes, constraints or cautions for users of this function]

------------------------------------------------------------------------------
*/

Word16 dtx_enc_init(dtx_encState **st, const Word16* lsp_init_data_ptr)
{
    dtx_encState* s;

    if (st == (dtx_encState **) NULL)
    {
        return(-1);
    }

    *st = NULL;

    /* allocate memory */
    if ((s = (dtx_encState *) oscl_malloc(sizeof(dtx_encState))) == NULL)
    {
        return(-1);
    }

    dtx_enc_reset(s, lsp_init_data_ptr);
    *st = s;

    return(0);
}

/****************************************************************************/

/*
------------------------------------------------------------------------------
 FUNCTION NAME: dtx_enc_reset
------------------------------------------------------------------------------
 INPUT AND OUTPUT DEFINITIONS

 Inputs:
    st = pointer to structures of type dtx_encState

 Outputs:
    structure pointed to by st is initialized to its reset value

 Returns:
    return_value = 1, if reset was successful; -1, otherwise (int)

 Global Variables Used:
    None

 Local Variables Needed:
    lsp_init_data = table containing LSP initialization values;
            table elements are constants of type Word16;
            table length is M

------------------------------------------------------------------------------
 FUNCTION DESCRIPTION

 This function initializes the fields of the state memory used by dtx_enc
 to their reset values.

------------------------------------------------------------------------------
 REQUIREMENTS

 None

------------------------------------------------------------------------------
 REFERENCES

 dtx_enc.c, UMTS GSM AMR speech codec, R99 - Version 3.2.0, March 2, 2001

------------------------------------------------------------------------------
 PSEUDO-CODE

int dtx_enc_reset (dtx_encState *st)
{
  Word16 i;

  if (st == (dtx_encState *) NULL){
    fprintf(stderr, "dtx_enc_reset: invalid parameter\n");
    return -1;
  }

  st->hist_ptr = 0;
  st->log_en_index = 0;
  st->init_lsf_vq_index = 0;
  st->lsp_index[0] = 0;
  st->lsp_index[1] = 0;
  st->lsp_index[2] = 0;

  // Init lsp_hist[]
  for(i = 0; i < DTX_HIST_SIZE; i++)
  {
    Copy(lsp_init_data, &st->lsp_hist[i * M], M);
  }

  // Reset energy history
  Set_zero(st->log_en_hist, M);

  st->dtxHangoverCount = DTX_HANG_CONST;
  st->decAnaElapsedCount = 32767;

  return 1;
}

------------------------------------------------------------------------------
 CAUTION [optional]
 [State any special notes, constraints or cautions for users of this function]

------------------------------------------------------------------------------
*/

Word16 dtx_enc_reset(dtx_encState *st, const Word16* lsp_init_data_ptr)
{
    Word16 i;

    if (st == (dtx_encState *) NULL)
    {
        return(-1);
    }

    st->hist_ptr = 0;
    st->log_en_index = 0;
    st->init_lsf_vq_index = 0;
    st->lsp_index[0] = 0;
    st->lsp_index[1] = 0;
    st->lsp_index[2] = 0;

    /* Init lsp_hist[] */
    for (i = 0; i < DTX_HIST_SIZE; i++)
    {
        oscl_memcpy(&st->lsp_hist[i * M], lsp_init_data_ptr, M*sizeof(Word16));
    }

    /* Reset energy history */
    oscl_memset(st->log_en_hist, 0, sizeof(Word16)*M);
    st->dtxHangoverCount = DTX_HANG_CONST;
    st->decAnaElapsedCount = 32767;

    return(1);
}

/****************************************************************************/

/*
------------------------------------------------------------------------------
 FUNCTION NAME: dtx_enc_exit
------------------------------------------------------------------------------
 INPUT AND OUTPUT DEFINITIONS

 Inputs:
    st = pointer to an array of pointers to structures of type
         dtx_encState

 Outputs:
    st points to the NULL address

 Returns:
    None

 Global Variables Used:
    None

 Local Variables Needed:
    None

------------------------------------------------------------------------------
 FUNCTION DESCRIPTION

 This function deallocates the state memory used by dtx_enc function.

------------------------------------------------------------------------------
 REQUIREMENTS

 None

------------------------------------------------------------------------------
 REFERENCES

 dtx_enc.c, UMTS GSM AMR speech codec, R99 - Version 3.2.0, March 2, 2001

------------------------------------------------------------------------------
 PSEUDO-CODE

void dtx_enc_exit (dtx_encState **st)
{
   if (st == NULL || *st == NULL)
      return;

   // deallocate memory
   free(*st);
   *st = NULL;

   return;
}

------------------------------------------------------------------------------
 CAUTION [optional]
 [State any special notes, constraints or cautions for users of this function]

------------------------------------------------------------------------------
*/

void dtx_enc_exit(dtx_encState **st)
{
    if (st == NULL || *st == NULL)
    {
        return;
    }

    /* deallocate memory */
    oscl_free(*st);
    *st = NULL;

    return;
}

/****************************************************************************/

/*
------------------------------------------------------------------------------
 FUNCTION NAME: dtx_enc
------------------------------------------------------------------------------
 INPUT AND OUTPUT DEFINITIONS

 Inputs:
    st = pointer to structures of type dtx_encState
    computeSidFlag = compute SID flag of type Word16
    qSt = pointer to structures of type Q_plsfState
    predState = pointer to structures of type gc_predState
    anap = pointer to an array of pointers to analysis parameters of
           type Word16

 Outputs:
    structure pointed to by st contains the newly calculated SID
      parameters
    structure pointed to by predState contains the new logarithmic frame
      energy
    pointer pointed to by anap points to the location of the new
      logarithmic frame energy and new LSPs

 Returns:
    return_value = 0 (int)

 Global Variables Used:
    None

 Local Variables Needed:
    None

------------------------------------------------------------------------------
 FUNCTION DESCRIPTION

 This function calculates the SID parameters when in the DTX mode.

------------------------------------------------------------------------------
 REQUIREMENTS

 None

------------------------------------------------------------------------------
 REFERENCES

 dtx_enc.c, UMTS GSM AMR speech codec, R99 - Version 3.2.0, March 2, 2001

------------------------------------------------------------------------------
 PSEUDO-CODE

int dtx_enc(dtx_encState *st,        // i/o : State struct
            Word16 computeSidFlag,   // i   : compute SID
            Q_plsfState *qSt,        // i/o : Qunatizer state struct
            gc_predState* predState, // i/o : State struct
        Word16 **anap            // o   : analysis parameters
        )
{
   Word16 i,j;
   Word16 log_en;
   Word16 lsf[M];
   Word16 lsp[M];
   Word16 lsp_q[M];
   Word32 L_lsp[M];

   // VOX mode computation of SID parameters
   if ((computeSidFlag != 0)  ||
        (st->log_en_index == 0))
   {
      // compute new SID frame if safe i.e don't
      // compute immediately after a talk spurt
      log_en = 0;
      for (i = 0; i < M; i++)
      {
         L_lsp[i] = 0;
      }

      // average energy and lsp
      for (i = 0; i < DTX_HIST_SIZE; i++)
      {
         log_en = add(log_en,
                      shr(st->log_en_hist[i],2));

         for (j = 0; j < M; j++)
         {
            L_lsp[j] = L_add(L_lsp[j],
                             L_deposit_l(st->lsp_hist[i * M + j]));
         }
      }

      log_en = shr(log_en, 1);
      for (j = 0; j < M; j++)
      {
         lsp[j] = extract_l(L_shr(L_lsp[j], 3));   // divide by 8
      }

      //  quantize logarithmic energy to 6 bits
      st->log_en_index = add(log_en, 2560);          // +2.5 in Q10
      st->log_en_index = add(st->log_en_index, 128); // add 0.5/4 in Q10
      st->log_en_index = shr(st->log_en_index, 8);

      if (sub(st->log_en_index, 63) > 0)
      {
         st->log_en_index = 63;
      }
      if (st->log_en_index < 0)
      {
         st->log_en_index = 0;
      }

      // update gain predictor memory
      log_en = shl(st->log_en_index, -2+10); // Q11 and divide by 4
      log_en = sub(log_en, 2560);            // add 2.5 in Q11

      log_en = sub(log_en, 9000);
      if (log_en > 0)
      {
         log_en = 0;
      }
      if (sub(log_en, -14436) < 0)
      {
         log_en = -14436;
      }

      // past_qua_en for other modes than MR122
      predState->past_qua_en[0] = log_en;
      predState->past_qua_en[1] = log_en;
      predState->past_qua_en[2] = log_en;
      predState->past_qua_en[3] = log_en;

      // scale down by factor 20*log10(2) in Q15
      log_en = mult(5443, log_en);

      // past_qua_en for mode MR122
      predState->past_qua_en_MR122[0] = log_en;
      predState->past_qua_en_MR122[1] = log_en;
      predState->past_qua_en_MR122[2] = log_en;
      predState->past_qua_en_MR122[3] = log_en;

      // make sure that LSP's are ordered
      Lsp_lsf(lsp, lsf, M);
      Reorder_lsf(lsf, LSF_GAP, M);
      Lsf_lsp(lsf, lsp, M);

      // Quantize lsp and put on parameter list
      Q_plsf_3(qSt, MRDTX, lsp, lsp_q, st->lsp_index,
               &st->init_lsf_vq_index);
   }

   *(*anap)++ = st->init_lsf_vq_index; // 3 bits

   *(*anap)++ = st->lsp_index[0];      // 8 bits
   *(*anap)++ = st->lsp_index[1];      // 9 bits
   *(*anap)++ = st->lsp_index[2];      // 9 bits


   *(*anap)++ = st->log_en_index;      // 6 bits
                                       // = 35 bits

   return 0;
}

------------------------------------------------------------------------------
 CAUTION [optional]
 [State any special notes, constraints or cautions for users of this function]

------------------------------------------------------------------------------
*/

void dtx_enc(dtx_encState *st,        /* i/o : State struct                  */
             Word16 computeSidFlag,   /* i   : compute SID                   */
             Q_plsfState *qSt,        /* i/o : Qunatizer state struct        */
             gc_predState* predState, /* i/o : State struct                  */
             Word16 **anap,           /* o   : analysis parameters           */
             Flag   *pOverflow        /* i/o : overflow indicator            */
            )
{
    register Word16 i, j;
    Word16 temp;
    Word16 log_en;
    Word16 lsf[M];
    Word16 lsp[M];
    Word16 lsp_q[M];
    Word32 L_lsp[M];

    /* VOX mode computation of SID parameters */

    if ((computeSidFlag != 0)  ||
            (st->log_en_index == 0))
    {
        /* compute new SID frame if safe i.e don't
         * compute immediately after a talk spurt  */
        log_en = 0;
        for (i = M - 1; i >= 0; i--)
        {
            L_lsp[i] = 0;
        }

        /* average energy and lsp */
        for (i = DTX_HIST_SIZE - 1; i >= 0; i--)
        {
            if (st->log_en_hist[i] < 0)
            {
                temp = ~((~(st->log_en_hist[i])) >> 2);
            }
            else
            {
                temp = st->log_en_hist[i] >> 2;
            }
            log_en = add_16(log_en, temp, pOverflow);

            for (j = M - 1; j >= 0; j--)
            {
                L_lsp[j] = L_add(L_lsp[j],
                                 (Word32)(st->lsp_hist[i * M + j]),
                                 pOverflow);
            }
        }

        if (log_en < 0)
        {
            log_en = ~((~log_en) >> 1);
        }
        else
        {
            log_en = log_en >> 1;
        }

        for (j = M - 1; j >= 0; j--)
        {
            /* divide by 8 */
            if (L_lsp[j] < 0)
            {
                lsp[j] = (Word16)(~((~L_lsp[j]) >> 3));
            }
            else
            {
                lsp[j] = (Word16)(L_lsp[j] >> 3);
            }
        }

        /*  quantize logarithmic energy to 6 bits */
        /* +2.5 in Q10 */
        st->log_en_index = log_en + 2560;
        /* add 0.5/4 in Q10 */
        st->log_en_index += 128;
        if (st->log_en_index < 0)
        {
            st->log_en_index = ~((~st->log_en_index) >> 8);
        }
        else
        {
            st->log_en_index = st->log_en_index >> 8;
        }

        /*---------------------------------------------*/
        /* Limit to max and min allowable 6-bit values */
        /* Note: For assembly implementation, use the  */
        /*       following:                            */
        /*       if(st->long_en_index >> 6 != 0)       */
        /*       {                                     */
        /*           if(st->long_en_index < 0)         */
        /*           {                                 */
        /*               st->long_en_index = 0         */
        /*           }                                 */
        /*           else                              */
        /*           {                                 */
        /*               st->long_en_index = 63        */
        /*           }                                 */
        /*       }                                     */
        /*---------------------------------------------*/
        if (st->log_en_index > 63)
        {
            st->log_en_index = 63;
        }
        else if (st->log_en_index < 0)
        {
            st->log_en_index = 0;
        }

        /* update gain predictor memory */
        /* Q11 and divide by 4 */
        log_en = (Word16)(((Word32) st->log_en_index) << (-2 + 10));

        log_en = sub(log_en, 11560, pOverflow);

        if (log_en > 0)
        {
            log_en = 0;
        }
        else if (log_en < -14436)
        {
            log_en = -14436;
        }

        /* past_qua_en for other modes than MR122 */
        predState->past_qua_en[0] = log_en;
        predState->past_qua_en[1] = log_en;
        predState->past_qua_en[2] = log_en;
        predState->past_qua_en[3] = log_en;

        /* scale down by factor 20*log10(2) in Q15 */
        log_en = (Word16)(((Word32)(5443 * log_en)) >> 15);

        /* past_qua_en for mode MR122 */
        predState->past_qua_en_MR122[0] = log_en;
        predState->past_qua_en_MR122[1] = log_en;
        predState->past_qua_en_MR122[2] = log_en;
        predState->past_qua_en_MR122[3] = log_en;

        /* make sure that LSP's are ordered */
        Lsp_lsf(lsp, lsf, M, pOverflow);
        Reorder_lsf(lsf, LSF_GAP, M, pOverflow);
        Lsf_lsp(lsf, lsp, M, pOverflow);

        /* Quantize lsp and put on parameter list */
        Q_plsf_3(qSt, MRDTX, lsp, lsp_q, st->lsp_index,
                 &st->init_lsf_vq_index, pOverflow);
    }

    *(*anap)++ = st->init_lsf_vq_index; /* 3 bits */
    *(*anap)++ = st->lsp_index[0];      /* 8 bits */
    *(*anap)++ = st->lsp_index[1];      /* 9 bits */
    *(*anap)++ = st->lsp_index[2];      /* 9 bits */
    *(*anap)++ = st->log_en_index;      /* 6 bits    */
    /* = 35 bits */

}

/****************************************************************************/


/*
------------------------------------------------------------------------------
 FUNCTION NAME: dtx_buffer
------------------------------------------------------------------------------
 INPUT AND OUTPUT DEFINITIONS

 Inputs:
    st = pointer to structures of type dtx_encState
    lsp_new = LSP vector whose elements are of type Word16; vector
          length is M
    speech = vector of speech samples of type Word16; vector length is
         BFR_SIZE_GSM

 Outputs:
    structure pointed to by st contains the new LSPs and logarithmic
      frame energy

 Returns:
    return_value = 0 (int)

 Global Variables Used:
    None

 Local Variables Needed:
    None

------------------------------------------------------------------------------
 FUNCTION DESCRIPTION

 This function handles the DTX buffer.

------------------------------------------------------------------------------
 REQUIREMENTS

 None

------------------------------------------------------------------------------
 REFERENCES

 dtx_enc.c, UMTS GSM AMR speech codec, R99 - Version 3.2.0, March 2, 2001

------------------------------------------------------------------------------
 PSEUDO-CODE

int dtx_buffer(dtx_encState *st,   // i/o : State struct
               Word16 lsp_new[],   // i   : LSP vector
               Word16 speech[]     // i   : speech samples
)
{
   Word16 i;
   Word32 L_frame_en;
   Word16 log_en_e;
   Word16 log_en_m;
   Word16 log_en;

   // update pointer to circular buffer
   st->hist_ptr = add(st->hist_ptr, 1);
   if (sub(st->hist_ptr, DTX_HIST_SIZE) == 0)
   {
      st->hist_ptr = 0;
   }

   // copy lsp vector into buffer
   Copy(lsp_new, &st->lsp_hist[st->hist_ptr * M], M);

   // compute log energy based on frame energy
   L_frame_en = 0;     // Q0
   for (i=0; i < L_FRAME; i++)
   {
      L_frame_en = L_mac(L_frame_en, speech[i], speech[i]);
   }
   Log2(L_frame_en, &log_en_e, &log_en_m);

   // convert exponent and mantissa to Word16 Q10
   log_en = shl(log_en_e, 10);  // Q10
   log_en = add(log_en, shr(log_en_m, 15-10));

   // divide with L_FRAME i.e subtract with log2(L_FRAME) = 7.32193
   log_en = sub(log_en, 8521);

   // insert into log energy buffer with division by 2
   log_en = shr(log_en, 1);
   st->log_en_hist[st->hist_ptr] = log_en; // Q10

   return 0;
}

------------------------------------------------------------------------------
 CAUTION [optional]
 [State any special notes, constraints or cautions for users of this function]

------------------------------------------------------------------------------
*/

void dtx_buffer(dtx_encState *st,   /* i/o : State struct                    */
                Word16 lsp_new[],   /* i   : LSP vector                      */
                Word16 speech[],    /* i   : speech samples                  */
                Flag   *pOverflow   /* i/o : overflow indicator              */
               )
{

    register Word16 i;
    Word32 L_frame_en;
    Word32 L_temp;
    Word16 log_en_e;
    Word16 log_en_m;
    Word16 log_en;
    Word16 *p_speech = &speech[0];

    /* update pointer to circular buffer      */
    st->hist_ptr += 1;

    if (st->hist_ptr == DTX_HIST_SIZE)
    {
        st->hist_ptr = 0;
    }

    /* copy lsp vector into buffer */
    oscl_memcpy(&st->lsp_hist[st->hist_ptr * M], lsp_new, M*sizeof(Word16));

    /* compute log energy based on frame energy */
    L_frame_en = 0;     /* Q0 */

    for (i = L_FRAME; i != 0; i--)
    {
        L_frame_en += (((Word32) * p_speech) * *(p_speech)) << 1;
        p_speech++;
        if (L_frame_en < 0)
        {
            L_frame_en = MAX_32;
            break;
        }
    }

    Log2(L_frame_en, &log_en_e, &log_en_m, pOverflow);

    /* convert exponent and mantissa to Word16 Q10 */
    /* Q10 */
    L_temp = ((Word32) log_en_e) << 10;
    if (L_temp != (Word32)((Word16) L_temp))
    {
        *pOverflow = 1;
        log_en = (log_en_e > 0) ? MAX_16 : MIN_16;
    }
    else
    {
        log_en = (Word16) L_temp;
    }

    log_en += log_en_m >> (15 - 10);

    /* divide with L_FRAME i.e subtract with log2(L_FRAME) = 7.32193 */
    log_en -= 8521;

    /* insert into log energy buffer with division by 2 */

    st->log_en_hist[st->hist_ptr] = log_en >> 1; /* Q10 */

}

/****************************************************************************/

/*
------------------------------------------------------------------------------
 FUNCTION NAME: tx_dtx_handler
------------------------------------------------------------------------------
 INPUT AND OUTPUT DEFINITIONS

 Inputs:
    st = pointer to structures of type dtx_encState
    vad_flag = VAD decision flag of type Word16
    usedMode = pointer to the currently used mode of type enum Mode

 Outputs:
    structure pointed to by st contains the newly calculated speech
      hangover

 Returns:
    compute_new_sid_possible = flag to indicate a change in the
                   used mode; store type is Word16

 Global Variables Used:
    None

 Local Variables Needed:
    None

------------------------------------------------------------------------------
 FUNCTION DESCRIPTION

 This function adds extra speech hangover to analyze speech on the decoding
 side.

------------------------------------------------------------------------------
 REQUIREMENTS

 None

------------------------------------------------------------------------------
 REFERENCES

 dtx_enc.c, UMTS GSM AMR speech codec, R99 - Version 3.2.0, March 2, 2001

------------------------------------------------------------------------------
 PSEUDO-CODE

Word16 tx_dtx_handler(dtx_encState *st,      // i/o : State struct
                      Word16 vad_flag,       // i   : vad decision
                      enum Mode *usedMode    // i/o : mode changed or not
                      )
{
   Word16 compute_new_sid_possible;

   // this state machine is in synch with the GSMEFR txDtx machine
   st->decAnaElapsedCount = add(st->decAnaElapsedCount, 1);

   compute_new_sid_possible = 0;

   if (vad_flag != 0)
   {
      st->dtxHangoverCount = DTX_HANG_CONST;
   }
   else
   {  // non-speech
      if (st->dtxHangoverCount == 0)
      {  // out of decoder analysis hangover
         st->decAnaElapsedCount = 0;
         *usedMode = MRDTX;
         compute_new_sid_possible = 1;
      }
      else
      { // in possible analysis hangover
         st->dtxHangoverCount = sub(st->dtxHangoverCount, 1);

         // decAnaElapsedCount + dtxHangoverCount < DTX_ELAPSED_FRAMES_THRESH
         if (sub(add(st->decAnaElapsedCount, st->dtxHangoverCount),
                 DTX_ELAPSED_FRAMES_THRESH) < 0)
         {
            *usedMode = MRDTX;
            // if short time since decoder update, do not add extra HO
         }
         // else
         //   override VAD and stay in
         //   speech mode *usedMode
         //   and add extra hangover
      }
   }

   return compute_new_sid_possible;
}

------------------------------------------------------------------------------
 CAUTION [optional]
 [State any special notes, constraints or cautions for users of this function]

------------------------------------------------------------------------------
*/

Word16 tx_dtx_handler(dtx_encState *st,      /* i/o : State struct           */
                      Word16 vad_flag,       /* i   : vad decision           */
                      enum Mode *usedMode,   /* i/o : mode changed or not    */
                      Flag   *pOverflow      /* i/o : overflow indicator     */
                     )
{
    Word16 compute_new_sid_possible;
    Word16 count;

    /* this state machine is in synch with the GSMEFR txDtx machine */
    st->decAnaElapsedCount = add_16(st->decAnaElapsedCount, 1, pOverflow);

    compute_new_sid_possible = 0;

    if (vad_flag != 0)
    {
        st->dtxHangoverCount = DTX_HANG_CONST;
    }
    else
    {  /* non-speech */
        if (st->dtxHangoverCount == 0)
        {  /* out of decoder analysis hangover  */
            st->decAnaElapsedCount = 0;
            *usedMode = MRDTX;
            compute_new_sid_possible = 1;
        }
        else
        { /* in possible analysis hangover */
            st->dtxHangoverCount -= 1;

            /* decAnaElapsedCount + dtxHangoverCount < */
            /* DTX_ELAPSED_FRAMES_THRESH               */
            count = add_16(st->decAnaElapsedCount, st->dtxHangoverCount,
                           pOverflow);
            if (count < DTX_ELAPSED_FRAMES_THRESH)
            {
                *usedMode = MRDTX;
                /* if short time since decoder update, */
                /* do not add extra HO                 */
            }
        }
    }

    return(compute_new_sid_possible);
}
