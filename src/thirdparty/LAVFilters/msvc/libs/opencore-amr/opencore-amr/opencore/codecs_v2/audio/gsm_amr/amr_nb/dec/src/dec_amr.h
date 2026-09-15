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



 Filename: dec_amr.h

------------------------------------------------------------------------------
 INCLUDE DESCRIPTION

     File             : dec_amr.h
     Purpose          : Speech decoder routine.

------------------------------------------------------------------------------
*/

/*----------------------------------------------------------------------------
; CONTINUE ONLY IF NOT ALREADY DEFINED
----------------------------------------------------------------------------*/
#ifndef DEC_AMR_H
#define DEC_AMR_H "$Id $"

/*----------------------------------------------------------------------------
; INCLUDES
----------------------------------------------------------------------------*/
#include "typedef.h"
#include "cnst.h"
#include "mode.h"
#include "dtx_dec.h"
#include "d_plsf.h"
#include "gc_pred.h"
#include "ec_gains.h"
#include "ph_disp.h"
#include "c_g_aver.h"
#include "bgnscd.h"
#include "lsp_avg.h"
#include "frame.h"
#include "get_const_tbls.h"

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
    ; Include all pre-processor statements here.
    ----------------------------------------------------------------------------*/
#define EXC_ENERGY_HIST_LEN  9
#define LTP_GAIN_HISTORY_LEN 9
    /*----------------------------------------------------------------------------
    ; EXTERNAL VARIABLES REFERENCES
    ; Declare variables used in this module but defined elsewhere
    ----------------------------------------------------------------------------*/

    /*----------------------------------------------------------------------------
    ; SIMPLE TYPEDEF'S
    ----------------------------------------------------------------------------*/

    /*----------------------------------------------------------------------------
    ; ENUMERATED TYPEDEF'S
    ----------------------------------------------------------------------------*/

    /*----------------------------------------------------------------------------
    ; STRUCTURES TYPEDEF'S
    ----------------------------------------------------------------------------*/
    typedef struct
    {
        /* Excitation vector */
        Word16 old_exc[L_SUBFR + PIT_MAX + L_INTERPOL];
        Word16 *exc;

        /* Lsp (Line spectral pairs) */
        /* Word16 lsp[M]; */      /* Used by CN codec */
        Word16 lsp_old[M];

        /* Filter's memory */
        Word16 mem_syn[M];

        /* pitch sharpening */
        Word16 sharp;
        Word16 old_T0;

        /* Memories for bad frame handling */
        Word16 prev_bf;
        Word16 prev_pdf;
        Word16 state;
        Word16 excEnergyHist[EXC_ENERGY_HIST_LEN];

        /* Variable holding received ltpLag, used in background noise and BFI */
        Word16 T0_lagBuff;

        /* Variables for the source characteristic detector (SCD) */
        Word16 inBackgroundNoise;
        Word16 voicedHangover;
        Word16 ltpGainHistory[LTP_GAIN_HISTORY_LEN];

        Bgn_scdState background_state;
        Word16 nodataSeed;

        Cb_gain_averageState Cb_gain_averState;
        lsp_avgState lsp_avg_st;

        D_plsfState lsfState;
        ec_gain_pitchState ec_gain_p_st;
        ec_gain_codeState ec_gain_c_st;
        gc_predState pred_state;
        ph_dispState ph_disp_st;
        dtx_decState dtxDecoderState;
        Flag overflow;
        CommonAmrTbls common_amr_tbls;
    } Decoder_amrState;

    /*----------------------------------------------------------------------------
    ; GLOBAL FUNCTION DEFINITIONS
    ; Function Prototype declaration
    ----------------------------------------------------------------------------*/
    /*
     *  Function    : Decoder_amr_init
     *  Purpose     : Allocates initializes state memory
     *  Description : Stores pointer to filter status struct in *st. This
     *                pointer has to be passed to Decoder_amr in each call.
     *  Returns     : 0 on success
     */
    Word16 Decoder_amr_init(Decoder_amrState *st);

    /*
     *  Function    : Decoder_amr_reset
     *  Purpose     : Resets state memory
     *  Returns     : 0 on success
     */
    Word16 Decoder_amr_reset(Decoder_amrState *st, enum Mode mode);

    /*
     *  Function    : Decoder_amr_exit
     *  Purpose     : The memory used for state memory is freed
     *  Description : Stores NULL in *s
     *  Returns     : void
     */
    void Decoder_amr_exit(Decoder_amrState **st);

    /*
     *  Function    : Decoder_amr
     *  Purpose     : Speech decoder routine.
     *  Returns     : 0
     */
    void Decoder_amr(
        Decoder_amrState *st,  /* i/o : State variables                       */
        enum Mode mode,        /* i   : AMR mode                              */
        Word16 parm[],         /* i   : vector of synthesis parameters
                                    (PRM_SIZE)                            */
        enum RXFrameType frame_type, /* i   : received frame type               */
        Word16 synth[],        /* o   : synthesis speech (L_FRAME)            */
        Word16 A_t[]           /* o   : decoded LP filter in 4 subframes
                                    (AZ_SIZE)                             */
    );

    /*----------------------------------------------------------------------------
    ; END
    ----------------------------------------------------------------------------*/
#ifdef __cplusplus
}
#endif

#endif /* DEC_AMR_H_ */



