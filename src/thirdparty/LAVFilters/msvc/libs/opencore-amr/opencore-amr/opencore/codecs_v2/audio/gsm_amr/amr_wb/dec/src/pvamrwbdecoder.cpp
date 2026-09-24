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

    3GPP TS 26.173
    ANSI-C code for the Adaptive Multi-Rate - Wideband (AMR-WB) speech codec
    Available from http://www.3gpp.org

(C) 2007, 3GPP Organizational Partners (ARIB, ATIS, CCSA, ETSI, TTA, TTC)
Permission to distribute, modify and use this file under the standard license
terms listed above has been obtained from the copyright holder.
****************************************************************************************/
/*
------------------------------------------------------------------------------



 Filename: pvamrwbdecoder.cpp

------------------------------------------------------------------------------
 INPUT AND OUTPUT DEFINITIONS

     int16 mode,                      input : used mode
     int16 prms[],                    input : parameter vector
     int16 synth16k[],                output: synthesis speech
     int16 * frame_length,            output:  lenght of the frame
     void *spd_state,                 i/o   : State structure
     int16 frame_type,                input : received frame type
     int16 ScratchMem[]

------------------------------------------------------------------------------
 FUNCTION DESCRIPTION

   Performs the main decoder routine AMR WB ACELP coding algorithm with 20 ms
   speech frames for wideband speech signals.


------------------------------------------------------------------------------
 REQUIREMENTS


------------------------------------------------------------------------------
 REFERENCES

------------------------------------------------------------------------------
 PSEUDO-CODE

------------------------------------------------------------------------------
*/


/*----------------------------------------------------------------------------
; INCLUDES
----------------------------------------------------------------------------*/

#include "pv_amr_wb_type_defs.h"
#include "pvamrwbdecoder_mem_funcs.h"
#include "pvamrwbdecoder_basic_op.h"
#include "pvamrwbdecoder_cnst.h"
#include "pvamrwbdecoder_acelp.h"
#include "e_pv_amrwbdec.h"
#include "get_amr_wb_bits.h"
#include "pvamrwb_math_op.h"
#include "pvamrwbdecoder_api.h"
#include "pvamrwbdecoder.h"
#include "synthesis_amr_wb.h"


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
; LOCAL STORE/BUFFER/POINTER DEFINITIONS
; Variable declaration - defined here and used outside this module
----------------------------------------------------------------------------*/

/* LPC interpolation coef {0.45, 0.8, 0.96, 1.0}; in Q15 */
static const int16 interpol_frac[NB_SUBFR] = {14746, 26214, 31457, 32767};


/* isp tables for initialization */

static const int16 isp_init[M] =
{
    32138, 30274, 27246, 23170, 18205, 12540, 6393, 0,
    -6393, -12540, -18205, -23170, -27246, -30274, -32138, 1475
};

static const int16 isf_init[M] =
{
    1024, 2048, 3072, 4096, 5120, 6144, 7168, 8192,
    9216, 10240, 11264, 12288, 13312, 14336, 15360, 3840
};

/*----------------------------------------------------------------------------
; EXTERNAL FUNCTION REFERENCES
; Declare functions defined elsewhere and referenced in this module
----------------------------------------------------------------------------*/

/*----------------------------------------------------------------------------
; EXTERNAL GLOBAL STORE/BUFFER/POINTER REFERENCES
; Declare variables used in this module but defined elsewhere
----------------------------------------------------------------------------*/

/*----------------------------------------------------------------------------
; FUNCTION CODE
----------------------------------------------------------------------------*/

/*----------------------------------------------------------------------------
 FUNCTION DESCRIPTION   pvDecoder_AmrWb_Init

   Initialization of variables for the decoder section.

----------------------------------------------------------------------------*/




void pvDecoder_AmrWb_Init(void **spd_state, void *pt_st, int16 **ScratchMem)
{
    /* Decoder states */
    Decoder_State *st = &(((PV_AmrWbDec *)pt_st)->state);

    *ScratchMem = ((PV_AmrWbDec *)pt_st)->ScratchMem;
    /*
     *  Init dtx decoding
     */
    dtx_dec_amr_wb_reset(&(st->dtx_decSt), isf_init);

    pvDecoder_AmrWb_Reset((void *) st, 1);

    *spd_state = (void *) st;

    return;
}

/*----------------------------------------------------------------------------
; FUNCTION CODE
----------------------------------------------------------------------------*/

void pvDecoder_AmrWb_Reset(void *st, int16 reset_all)
{
    int16 i;

    Decoder_State *dec_state;

    dec_state = (Decoder_State *) st;

    pv_memset((void *)dec_state->old_exc,
              0,
              (PIT_MAX + L_INTERPOL)*sizeof(*dec_state->old_exc));

    pv_memset((void *)dec_state->past_isfq,
              0,
              M*sizeof(*dec_state->past_isfq));


    dec_state->old_T0_frac = 0;               /* old pitch value = 64.0 */
    dec_state->old_T0 = 64;
    dec_state->first_frame = 1;
    dec_state->L_gc_thres = 0;
    dec_state->tilt_code = 0;

    pv_memset((void *)dec_state->disp_mem,
              0,
              8*sizeof(*dec_state->disp_mem));


    /* scaling memories for excitation */
    dec_state->Q_old = Q_MAX;
    dec_state->Qsubfr[3] = Q_MAX;
    dec_state->Qsubfr[2] = Q_MAX;
    dec_state->Qsubfr[1] = Q_MAX;
    dec_state->Qsubfr[0] = Q_MAX;

    if (reset_all != 0)
    {
        /* routines initialization */

        dec_gain2_amr_wb_init(dec_state->dec_gain);
        oversamp_12k8_to_16k_init(dec_state->mem_oversamp);
        band_pass_6k_7k_init(dec_state->mem_hf);
        low_pass_filt_7k_init(dec_state->mem_hf3);
        highpass_50Hz_at_12k8_init(dec_state->mem_sig_out);
        highpass_400Hz_at_12k8_init(dec_state->mem_hp400);
        Init_Lagconc(dec_state->lag_hist);

        /* isp initialization */

        pv_memcpy((void *)dec_state->ispold, (void *)isp_init, M*sizeof(*isp_init));

        pv_memcpy((void *)dec_state->isfold, (void *)isf_init, M*sizeof(*isf_init));
        for (i = 0; i < L_MEANBUF; i++)
        {
            pv_memcpy((void *)&dec_state->isf_buf[i * M],
                      (void *)isf_init,
                      M*sizeof(*isf_init));
        }
        /* variable initialization */

        dec_state->mem_deemph = 0;

        dec_state->seed  = 21845;              /* init random with 21845 */
        dec_state->seed2 = 21845;
        dec_state->seed3 = 21845;

        dec_state->state = 0;
        dec_state->prev_bfi = 0;

        /* Static vectors to zero */

        pv_memset((void *)dec_state->mem_syn_hf,
                  0,
                  M16k*sizeof(*dec_state->mem_syn_hf));

        pv_memset((void *)dec_state->mem_syn_hi,
                  0,
                  M*sizeof(*dec_state->mem_syn_hi));

        pv_memset((void *)dec_state->mem_syn_lo,
                  0,
                  M*sizeof(*dec_state->mem_syn_lo));


        dtx_dec_amr_wb_reset(&(dec_state->dtx_decSt), isf_init);
        dec_state->vad_hist = 0;

    }
    return;
}

/*----------------------------------------------------------------------------
; FUNCTION CODE
----------------------------------------------------------------------------*/

int32 pvDecoder_AmrWbMemRequirements()
{
    return(sizeof(PV_AmrWbDec));
}


/*----------------------------------------------------------------------------
; FUNCTION CODE
----------------------------------------------------------------------------*/

/*              Main decoder routine.                                       */

int32 pvDecoder_AmrWb(
    int16 mode,              /* input : used mode                     */
    int16 prms[],            /* input : parameter vector              */
    int16 synth16k[],        /* output: synthesis speech              */
    int16 * frame_length,    /* output:  lenght of the frame          */
    void *spd_state,         /* i/o   : State structure               */
    int16 frame_type,        /* input : received frame type           */
    int16 ScratchMem[]
)
{

    /* Decoder states */
    Decoder_State *st;

    int16 *ScratchMem2 = &ScratchMem[ L_SUBFR + L_SUBFR16k + ((L_SUBFR + M + M16k +1)<<1)];


    /* Excitation vector */


    int16 *old_exc = ScratchMem2;

    int16 *Aq = &old_exc[(L_FRAME + 1) + PIT_MAX + L_INTERPOL];/* A(z)   quantized for the 4 subframes */

    int16 *ispnew  = &Aq[NB_SUBFR * (M + 1)];/* immittance spectral pairs at 4nd sfr */
    int16 *isf     = &ispnew[M];             /* ISF (frequency domain) at 4nd sfr    */
    int16 *isf_tmp = &isf[M];
    int16 *code    = &isf_tmp[M];             /* algebraic codevector                 */
    int16 *excp    = &code[L_SUBFR];
    int16 *exc2    = &excp[L_SUBFR];         /* excitation vector                    */
    int16 *HfIsf   = &exc2[L_FRAME];


    int16 *exc;

    /* LPC coefficients */

    int16 *p_Aq;                          /* ptr to A(z) for the 4 subframes      */



    int16 fac, stab_fac, voice_fac, Q_new = 0;
    int32 L_tmp, L_gain_code;

    /* Scalars */

    int16 i, j, i_subfr, index, ind[8], tmp;
    int32 max;
    int16 T0, T0_frac, pit_flag, T0_max, select, T0_min = 0;
    int16 gain_pit, gain_code;
    int16 newDTXState, bfi, unusable_frame, nb_bits;
    int16 vad_flag;
    int16 pit_sharp;

    int16 corr_gain = 0;

    st = (Decoder_State *) spd_state;

    /* mode verification */

    nb_bits = AMR_WB_COMPRESSED[mode];

    *frame_length = AMR_WB_PCM_FRAME;

    /* find the new  DTX state  SPEECH OR DTX */
    newDTXState = rx_amr_wb_dtx_handler(&(st->dtx_decSt), frame_type);


    if (newDTXState != SPEECH)
    {
        dtx_dec_amr_wb(&(st->dtx_decSt), exc2, newDTXState, isf, &prms);
    }
    /* SPEECH action state machine  */

    if ((frame_type == RX_SPEECH_BAD) ||
            (frame_type == RX_SPEECH_PROBABLY_DEGRADED))
    {
        /* bfi for all index, bits are not usable */
        bfi = 1;
        unusable_frame = 0;
    }
    else if ((frame_type == RX_NO_DATA) ||
             (frame_type == RX_SPEECH_LOST))
    {
        /* bfi only for lsf, gains and pitch period */
        bfi = 1;
        unusable_frame = 1;
    }
    else
    {
        bfi = 0;
        unusable_frame = 0;
    }

    if (bfi != 0)
    {
        st->state += 1;

        if (st->state > 6)
        {
            st->state = 6;
        }
    }
    else
    {
        st->state >>=  1;
    }

    /* If this frame is the first speech frame after CNI period,
     * set the BFH state machine to an appropriate state depending
     * on whether there was DTX muting before start of speech or not
     * If there was DTX muting, the first speech frame is muted.
     * If there was no DTX muting, the first speech frame is not
     * muted. The BFH state machine starts from state 5, however, to
     * keep the audible noise resulting from a SID frame which is
     * erroneously interpreted as a good speech frame as small as
     * possible (the decoder output in this case is quickly muted)
     */

    if (st->dtx_decSt.dtxGlobalState == DTX)
    {
        st->state = 5;
        st->prev_bfi = 0;
    }
    else if (st->dtx_decSt.dtxGlobalState == DTX_MUTE)
    {
        st->state = 5;
        st->prev_bfi = 1;
    }

    if (newDTXState == SPEECH)
    {
        vad_flag = Serial_parm_1bit(&prms);

        if (bfi == 0)
        {
            if (vad_flag == 0)
            {
                st->vad_hist = add_int16(st->vad_hist, 1);
            }
            else
            {
                st->vad_hist = 0;
            }
        }
    }
    /*
     *  DTX-CNG
     */

    if (newDTXState != SPEECH)     /* CNG mode */
    {
        /* increase slightly energy of noise below 200 Hz */

        /* Convert ISFs to the cosine domain */
        Isf_isp(isf, ispnew, M);

        Isp_Az(ispnew, Aq, M, 1);

        pv_memcpy((void *)isf_tmp, (void *)st->isfold, M*sizeof(*isf_tmp));


        for (i_subfr = 0; i_subfr < L_FRAME; i_subfr += L_SUBFR)
        {
            j = i_subfr >> 6;

            for (i = 0; i < M; i++)
            {
                L_tmp = mul_16by16_to_int32(isf_tmp[i], sub_int16(32767, interpol_frac[j]));
                L_tmp = mac_16by16_to_int32(L_tmp, isf[i], interpol_frac[j]);
                HfIsf[i] = amr_wb_round(L_tmp);
            }

            synthesis_amr_wb(Aq,
                             &exc2[i_subfr],
                             0,
                             &synth16k[i_subfr *5/4],
                             (short) 1,
                             HfIsf,
                             nb_bits,
                             newDTXState,
                             st,
                             bfi,
                             ScratchMem);
        }

        /* reset speech coder memories */
        pvDecoder_AmrWb_Reset(st, 0);

        pv_memcpy((void *)st->isfold, (void *)isf, M*sizeof(*isf));

        st->prev_bfi = bfi;
        st->dtx_decSt.dtxGlobalState = newDTXState;

        return 0;
    }
    /*
     *  ACELP
     */

    /* copy coder memory state into working space (internal memory for DSP) */

    pv_memcpy((void *)old_exc, (void *)st->old_exc, (PIT_MAX + L_INTERPOL)*sizeof(*old_exc));

    exc = old_exc + PIT_MAX + L_INTERPOL;

    /* Decode the ISFs */

    if (nb_bits > NBBITS_7k)        /* all rates but 6.6 Kbps */
    {
        ind[0] = Serial_parm(8, &prms);     /* index of 1st ISP subvector */
        ind[1] = Serial_parm(8, &prms);     /* index of 2nd ISP subvector */
        ind[2] = Serial_parm(6, &prms);     /* index of 3rd ISP subvector */
        ind[3] = Serial_parm(7, &prms);     /* index of 4th ISP subvector */
        ind[4] = Serial_parm(7, &prms);     /* index of 5th ISP subvector */
        ind[5] = Serial_parm(5, &prms);     /* index of 6th ISP subvector */
        ind[6] = Serial_parm(5, &prms);     /* index of 7th ISP subvector */

        Dpisf_2s_46b(ind, isf, st->past_isfq, st->isfold, st->isf_buf, bfi, 1);
    }
    else
    {
        ind[0] = Serial_parm(8, &prms);
        ind[1] = Serial_parm(8, &prms);
        ind[2] = Serial_parm(14, &prms);
        ind[3] = ind[2] & 0x007F;
        ind[2] >>= 7;
        ind[4] = Serial_parm(6, &prms);

        Dpisf_2s_36b(ind, isf, st->past_isfq, st->isfold, st->isf_buf, bfi, 1);
    }

    /* Convert ISFs to the cosine domain */

    Isf_isp(isf, ispnew, M);

    if (st->first_frame != 0)
    {
        st->first_frame = 0;
        pv_memcpy((void *)st->ispold, (void *)ispnew, M*sizeof(*ispnew));

    }
    /* Find the interpolated ISPs and convert to a[] for all subframes */
    interpolate_isp(st->ispold, ispnew, interpol_frac, Aq);

    /* update ispold[] for the next frame */
    pv_memcpy((void *)st->ispold, (void *)ispnew, M*sizeof(*ispnew));

    /* Check stability on isf : distance between old isf and current isf */

    L_tmp = 0;
    for (i = 0; i < M - 1; i++)
    {
        tmp = sub_int16(isf[i], st->isfold[i]);
        L_tmp = mac_16by16_to_int32(L_tmp, tmp, tmp);
    }
    tmp = extract_h(shl_int32(L_tmp, 8));
    tmp = mult_int16(tmp, 26214);                /* tmp = L_tmp*0.8/256 */

    tmp = 20480 - tmp;                 /* 1.25 - tmp */
    stab_fac = shl_int16(tmp, 1);                /* Q14 -> Q15 with saturation */

    if (stab_fac < 0)
    {
        stab_fac = 0;
    }
    pv_memcpy((void *)isf_tmp, (void *)st->isfold, M*sizeof(*isf_tmp));

    pv_memcpy((void *)st->isfold, (void *)isf, M*sizeof(*isf));

    /*
     *          Loop for every subframe in the analysis frame
     *
     * The subframe size is L_SUBFR and the loop is repeated L_FRAME/L_SUBFR
     *  times
     *     - decode the pitch delay and filter mode
     *     - decode algebraic code
     *     - decode pitch and codebook gains
     *     - find voicing factor and tilt of code for next subframe.
     *     - find the excitation and compute synthesis speech
     */

    p_Aq = Aq;                                /* pointer to interpolated LPC parameters */


    /*
     *   Sub process next 3 subframes
     */


    for (i_subfr = 0; i_subfr < L_FRAME; i_subfr += L_SUBFR)
    {
        pit_flag = i_subfr;


        if ((i_subfr == 2*L_SUBFR) && (nb_bits > NBBITS_7k))
        {
            pit_flag = 0;        /* set to 0 for 3rd subframe, <=> is not 6.6 kbps */
        }
        /*-------------------------------------------------*
         * - Decode pitch lag                              *
         * Lag indeces received also in case of BFI,       *
         * so that the parameter pointer stays in sync.    *
         *-------------------------------------------------*/

        if (pit_flag == 0)
        {

            if (nb_bits <= NBBITS_9k)
            {
                index = Serial_parm(8, &prms);

                if (index < (PIT_FR1_8b - PIT_MIN) * 2)
                {
                    T0 = PIT_MIN + (index >> 1);
                    T0_frac = sub_int16(index, shl_int16(sub_int16(T0, PIT_MIN), 1));
                    T0_frac = shl_int16(T0_frac, 1);
                }
                else
                {
                    T0 = add_int16(index, PIT_FR1_8b - ((PIT_FR1_8b - PIT_MIN) * 2));
                    T0_frac = 0;
                }
            }
            else
            {
                index = Serial_parm(9, &prms);

                if (index < (PIT_FR2 - PIT_MIN) * 4)
                {
                    T0 = PIT_MIN + (index >> 2);
                    T0_frac = sub_int16(index, shl_int16(sub_int16(T0, PIT_MIN), 2));
                }
                else if (index < (((PIT_FR2 - PIT_MIN) << 2) + ((PIT_FR1_9b - PIT_FR2) << 1)))
                {
                    index -= (PIT_FR2 - PIT_MIN) << 2;
                    T0 = PIT_FR2 + (index >> 1);
                    T0_frac = sub_int16(index, shl_int16(sub_int16(T0, PIT_FR2), 1));
                    T0_frac = shl_int16(T0_frac, 1);
                }
                else
                {
                    T0 = add_int16(index, (PIT_FR1_9b - ((PIT_FR2 - PIT_MIN) * 4) - ((PIT_FR1_9b - PIT_FR2) * 2)));
                    T0_frac = 0;
                }
            }

            /* find T0_min and T0_max for subframe 2 and 4 */

            T0_min = T0 - 8;

            if (T0_min < PIT_MIN)
            {
                T0_min = PIT_MIN;
            }
            T0_max = T0_min + 15;

            if (T0_max > PIT_MAX)
            {
                T0_max = PIT_MAX;
                T0_min = PIT_MAX - 15;
            }
        }
        else
        {                                  /* if subframe 2 or 4 */

            if (nb_bits <= NBBITS_9k)
            {
                index = Serial_parm(5, &prms);

                T0 = T0_min + (index >> 1);
                T0_frac = sub_int16(index, shl_int16(T0 - T0_min, 1));
                T0_frac = shl_int16(T0_frac, 1);
            }
            else
            {
                index = Serial_parm(6, &prms);

                T0 = T0_min + (index >> 2);
                T0_frac = sub_int16(index, shl_int16(T0 - T0_min, 2));
            }
        }

        /* check BFI after pitch lag decoding */

        if (bfi != 0)                      /* if frame erasure */
        {
            lagconceal(&(st->dec_gain[17]), st->lag_hist, &T0, &(st->old_T0), &(st->seed3), unusable_frame);
            T0_frac = 0;
        }
        /*
         *  Find the pitch gain, the interpolation filter
         *  and the adaptive codebook vector.
         */

        Pred_lt4(&exc[i_subfr], T0, T0_frac, L_SUBFR + 1);


        if (unusable_frame)
        {
            select = 1;
        }
        else
        {

            if (nb_bits <= NBBITS_9k)
            {
                select = 0;
            }
            else
            {
                select = Serial_parm_1bit(&prms);
            }
        }


        if (select == 0)
        {
            /* find pitch excitation with lp filter */
            for (i = 0; i < L_SUBFR; i++)
            {
                L_tmp  = ((int32) exc[i-1+i_subfr] + exc[i+1+i_subfr]);
                L_tmp *= 5898;
                L_tmp += ((int32) exc[i+i_subfr] * 20972);

                code[i] = amr_wb_round(L_tmp << 1);
            }
            pv_memcpy((void *)&exc[i_subfr], (void *)code, L_SUBFR*sizeof(*code));

        }
        /*
         * Decode innovative codebook.
         * Add the fixed-gain pitch contribution to code[].
         */

        if (unusable_frame != 0)
        {
            /* the innovative code doesn't need to be scaled (see Q_gain2) */
            for (i = 0; i < L_SUBFR; i++)
            {
                code[i] = noise_gen_amrwb(&(st->seed)) >> 3;
            }
        }
        else if (nb_bits <= NBBITS_7k)
        {
            ind[0] = Serial_parm(12, &prms);
            dec_acelp_2p_in_64(ind[0], code);
        }
        else if (nb_bits <= NBBITS_9k)
        {
            for (i = 0; i < 4; i++)
            {
                ind[i] = Serial_parm(5, &prms);
            }
            dec_acelp_4p_in_64(ind, 20, code);
        }
        else if (nb_bits <= NBBITS_12k)
        {
            for (i = 0; i < 4; i++)
            {
                ind[i] = Serial_parm(9, &prms);
            }
            dec_acelp_4p_in_64(ind, 36, code);
        }
        else if (nb_bits <= NBBITS_14k)
        {
            ind[0] = Serial_parm(13, &prms);
            ind[1] = Serial_parm(13, &prms);
            ind[2] = Serial_parm(9, &prms);
            ind[3] = Serial_parm(9, &prms);
            dec_acelp_4p_in_64(ind, 44, code);
        }
        else if (nb_bits <= NBBITS_16k)
        {
            for (i = 0; i < 4; i++)
            {
                ind[i] = Serial_parm(13, &prms);
            }
            dec_acelp_4p_in_64(ind, 52, code);
        }
        else if (nb_bits <= NBBITS_18k)
        {
            for (i = 0; i < 4; i++)
            {
                ind[i] = Serial_parm(2, &prms);
            }
            for (i = 4; i < 8; i++)
            {
                ind[i] = Serial_parm(14, &prms);
            }
            dec_acelp_4p_in_64(ind, 64, code);
        }
        else if (nb_bits <= NBBITS_20k)
        {
            ind[0] = Serial_parm(10, &prms);
            ind[1] = Serial_parm(10, &prms);
            ind[2] = Serial_parm(2, &prms);
            ind[3] = Serial_parm(2, &prms);
            ind[4] = Serial_parm(10, &prms);
            ind[5] = Serial_parm(10, &prms);
            ind[6] = Serial_parm(14, &prms);
            ind[7] = Serial_parm(14, &prms);
            dec_acelp_4p_in_64(ind, 72, code);
        }
        else
        {
            for (i = 0; i < 8; i++)
            {
                ind[i] = Serial_parm(11, &prms);
            }

            dec_acelp_4p_in_64(ind, 88, code);
        }

        preemph_amrwb_dec(code, st->tilt_code, L_SUBFR);

        tmp = T0;

        if (T0_frac > 2)
        {
            tmp++;
        }
        Pit_shrp(code, tmp, PIT_SHARP, L_SUBFR);

        /*
         *  Decode codebooks gains.
         */

        if (nb_bits <= NBBITS_9k)
        {
            index = Serial_parm(6, &prms); /* codebook gain index */

            dec_gain2_amr_wb(index,
                             6,
                             code,
                             L_SUBFR,
                             &gain_pit,
                             &L_gain_code,
                             bfi,
                             st->prev_bfi,
                             st->state,
                             unusable_frame,
                             st->vad_hist,
                             st->dec_gain);
        }
        else
        {
            index = Serial_parm(7, &prms); /* codebook gain index */

            dec_gain2_amr_wb(index,
                             7,
                             code,
                             L_SUBFR,
                             &gain_pit,
                             &L_gain_code,
                             bfi,
                             st->prev_bfi,
                             st->state,
                             unusable_frame,
                             st->vad_hist,
                             st->dec_gain);
        }

        /* find best scaling to perform on excitation (Q_new) */

        tmp = st->Qsubfr[0];
        for (i = 1; i < 4; i++)
        {
            if (st->Qsubfr[i] < tmp)
            {
                tmp = st->Qsubfr[i];
            }
        }

        /* limit scaling (Q_new) to Q_MAX: see pv_amr_wb_cnst.h and syn_filt_32() */

        if (tmp > Q_MAX)
        {
            tmp = Q_MAX;
        }
        Q_new = 0;
        L_tmp = L_gain_code;                  /* L_gain_code in Q16 */


        while ((L_tmp < 0x08000000L) && (Q_new < tmp))
        {
            L_tmp <<= 1;
            Q_new += 1;

        }
        gain_code = amr_wb_round(L_tmp);          /* scaled gain_code with Qnew */

        scale_signal(exc + i_subfr - (PIT_MAX + L_INTERPOL),
                     PIT_MAX + L_INTERPOL + L_SUBFR,
                     (int16)(Q_new - st->Q_old));

        st->Q_old = Q_new;


        /*
         * Update parameters for the next subframe.
         * - tilt of code: 0.0 (unvoiced) to 0.5 (voiced)
         */


        if (bfi == 0)
        {
            /* LTP-Lag history update */
            for (i = 4; i > 0; i--)
            {
                st->lag_hist[i] = st->lag_hist[i - 1];
            }
            st->lag_hist[0] = T0;

            st->old_T0 = T0;
            st->old_T0_frac = 0;              /* Remove fraction in case of BFI */
        }
        /* find voice factor in Q15 (1=voiced, -1=unvoiced) */

        /*
         * Scale down by 1/8
         */
        for (i = L_SUBFR - 1; i >= 0; i--)
        {
            exc2[i] = (exc[i_subfr + i] + (0x0004 * (exc[i_subfr + i] != MAX_16))) >> 3;
        }


        /* post processing of excitation elements */

        if (nb_bits <= NBBITS_9k)
        {
            pit_sharp = shl_int16(gain_pit, 1);

            if (pit_sharp > 16384)
            {
                for (i = 0; i < L_SUBFR; i++)
                {
                    tmp = mult_int16(exc2[i], pit_sharp);
                    L_tmp = mul_16by16_to_int32(tmp, gain_pit);
                    L_tmp >>= 1;
                    excp[i] = amr_wb_round(L_tmp);
                }
            }
        }
        else
        {
            pit_sharp = 0;
        }

        voice_fac = voice_factor(exc2, -3, gain_pit, code, gain_code, L_SUBFR);

        /* tilt of code for next subframe: 0.5=voiced, 0=unvoiced */

        st->tilt_code = (voice_fac >> 2) + 8192;

        /*
         * - Find the total excitation.
         * - Find synthesis speech corresponding to exc[].
         * - Find maximum value of excitation for next scaling
         */

        pv_memcpy((void *)exc2, (void *)&exc[i_subfr], L_SUBFR*sizeof(*exc2));
        max = 1;

        for (i = 0; i < L_SUBFR; i++)
        {
            L_tmp = mul_16by16_to_int32(code[i], gain_code);
            L_tmp = shl_int32(L_tmp, 5);
            L_tmp = mac_16by16_to_int32(L_tmp, exc[i + i_subfr], gain_pit);
            L_tmp = shl_int32(L_tmp, 1);
            tmp = amr_wb_round(L_tmp);
            exc[i + i_subfr] = tmp;
            tmp = tmp - (tmp < 0);
            max |= tmp ^(tmp >> 15);  /* |= tmp ^sign(tmp) */
        }


        /* tmp = scaling possible according to max value of excitation */
        tmp = add_int16(norm_s(max), Q_new) - 1;

        st->Qsubfr[3] = st->Qsubfr[2];
        st->Qsubfr[2] = st->Qsubfr[1];
        st->Qsubfr[1] = st->Qsubfr[0];
        st->Qsubfr[0] = tmp;

        /*
         * phase dispersion to enhance noise in low bit rate
         */


        if (nb_bits <= NBBITS_7k)
        {
            j = 0;      /* high dispersion for rate <= 7.5 kbit/s */
        }
        else if (nb_bits <= NBBITS_9k)
        {
            j = 1;      /* low dispersion for rate <= 9.6 kbit/s */
        }
        else
        {
            j = 2;      /* no dispersion for rate > 9.6 kbit/s */
        }

        /* L_gain_code in Q16 */

        phase_dispersion((int16)(L_gain_code >> 16),
                         gain_pit,
                         code,
                         j,
                         st->disp_mem,
                         ScratchMem);

        /*
         * noise enhancer
         * - Enhance excitation on noise. (modify gain of code)
         *   If signal is noisy and LPC filter is stable, move gain
         *   of code 1.5 dB toward gain of code threshold.
         *   This decrease by 3 dB noise energy variation.
         */

        tmp = 16384 - (voice_fac >> 1);  /* 1=unvoiced, 0=voiced */
        fac = mult_int16(stab_fac, tmp);

        L_tmp = L_gain_code;

        if (L_tmp < st->L_gc_thres)
        {
            L_tmp += fxp_mul32_by_16b(L_gain_code, 6226) << 1;

            if (L_tmp > st->L_gc_thres)
            {
                L_tmp = st->L_gc_thres;
            }
        }
        else
        {
            L_tmp = fxp_mul32_by_16b(L_gain_code, 27536) << 1;

            if (L_tmp < st->L_gc_thres)
            {
                L_tmp = st->L_gc_thres;
            }
        }
        st->L_gc_thres = L_tmp;

        L_gain_code = fxp_mul32_by_16b(L_gain_code, (32767 - fac)) << 1;


        L_gain_code = add_int32(L_gain_code, fxp_mul32_by_16b(L_tmp, fac) << 1);

        /*
         * pitch enhancer
         * - Enhance excitation on voice. (HP filtering of code)
         *   On voiced signal, filtering of code by a smooth fir HP
         *   filter to decrease energy of code in low frequency.
         */

        tmp = (voice_fac >> 3) + 4096;/* 0.25=voiced, 0=unvoiced */

        /* build excitation */

        gain_code = amr_wb_round(shl_int32(L_gain_code, Q_new));

        L_tmp = (int32)(code[0] << 16);
        L_tmp = msu_16by16_from_int32(L_tmp, code[1], tmp);
        L_tmp = mul_16by16_to_int32(amr_wb_round(L_tmp), gain_code);
        L_tmp = shl_int32(L_tmp, 5);
        L_tmp = mac_16by16_to_int32(L_tmp, exc2[0], gain_pit);
        L_tmp = shl_int32(L_tmp, 1);       /* saturation can occur here */
        exc2[0] = amr_wb_round(L_tmp);


        for (i = 1; i < L_SUBFR - 1; i++)
        {
            L_tmp = (int32)(code[i] << 16);
            L_tmp = msu_16by16_from_int32(L_tmp, (code[i + 1] + code[i - 1]), tmp);
            L_tmp = mul_16by16_to_int32(amr_wb_round(L_tmp), gain_code);
            L_tmp = shl_int32(L_tmp, 5);
            L_tmp = mac_16by16_to_int32(L_tmp, exc2[i], gain_pit);
            L_tmp = shl_int32(L_tmp, 1);       /* saturation can occur here */
            exc2[i] = amr_wb_round(L_tmp);
        }

        L_tmp = (int32)(code[L_SUBFR - 1] << 16);
        L_tmp = msu_16by16_from_int32(L_tmp, code[L_SUBFR - 2], tmp);
        L_tmp = mul_16by16_to_int32(amr_wb_round(L_tmp), gain_code);
        L_tmp = shl_int32(L_tmp, 5);
        L_tmp = mac_16by16_to_int32(L_tmp, exc2[L_SUBFR - 1], gain_pit);
        L_tmp = shl_int32(L_tmp, 1);       /* saturation can occur here */
        exc2[L_SUBFR - 1] = amr_wb_round(L_tmp);



        if (nb_bits <= NBBITS_9k)
        {
            if (pit_sharp > 16384)
            {
                for (i = 0; i < L_SUBFR; i++)
                {
                    excp[i] = add_int16(excp[i], exc2[i]);
                }
                agc2_amr_wb(exc2, excp, L_SUBFR);
                pv_memcpy((void *)exc2, (void *)excp, L_SUBFR*sizeof(*exc2));

            }
        }
        if (nb_bits <= NBBITS_7k)
        {
            j = i_subfr >> 6;
            for (i = 0; i < M; i++)
            {
                L_tmp = mul_16by16_to_int32(isf_tmp[i], sub_int16(32767, interpol_frac[j]));
                L_tmp = mac_16by16_to_int32(L_tmp, isf[i], interpol_frac[j]);
                HfIsf[i] = amr_wb_round(L_tmp);
            }
        }
        else
        {
            pv_memset((void *)st->mem_syn_hf,
                      0,
                      (M16k - M)*sizeof(*st->mem_syn_hf));
        }

        if (nb_bits >= NBBITS_24k)
        {
            corr_gain = Serial_parm(4, &prms);
        }
        else
        {
            corr_gain = 0;
        }

        synthesis_amr_wb(p_Aq,
                         exc2,
                         Q_new,
                         &synth16k[i_subfr + (i_subfr>>2)],
                         corr_gain,
                         HfIsf,
                         nb_bits,
                         newDTXState,
                         st,
                         bfi,
                         ScratchMem);

        p_Aq += (M + 1);                   /* interpolated LPC parameters for next subframe */
    }

    /*
     *   Update signal for next frame.
     *   -> save past of exc[]
     *   -> save pitch parameters
     */

    pv_memcpy((void *)st->old_exc,
              (void *)&old_exc[L_FRAME],
              (PIT_MAX + L_INTERPOL)*sizeof(*old_exc));

    scale_signal(exc, L_FRAME, (int16)(-Q_new));

    dtx_dec_amr_wb_activity_update(&(st->dtx_decSt), isf, exc);

    st->dtx_decSt.dtxGlobalState = newDTXState;

    st->prev_bfi = bfi;

    return 0;
}

