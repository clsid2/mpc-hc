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



 Filename: dtx_decoder_amr_wb.cpp

------------------------------------------------------------------------------
 INPUT AND OUTPUT DEFINITIONS


------------------------------------------------------------------------------
 FUNCTION DESCRIPTION

    DTX functions

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
#include "pvamrwbdecoder_basic_op.h"
#include "pvamrwb_math_op.h"
#include "pvamrwbdecoder_cnst.h"
#include "pvamrwbdecoder_acelp.h"  /* prototype of functions    */
#include "get_amr_wb_bits.h"
#include "dtx.h"

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
; LOCAL STORE/BUFFER/POINTER DEFINITIONS
; Variable declaration - defined here and used outside this module
----------------------------------------------------------------------------*/

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
/*
 * Function    : dtx_dec_amr_wb_reset
 */
int16 dtx_dec_amr_wb_reset(dtx_decState * st, const int16 isf_init[])
{
    int16 i;


    if (st == (dtx_decState *) NULL)
    {
        /* dtx_dec_amr_wb_reset invalid parameter */
        return (-1);
    }
    st->since_last_sid = 0;
    st->true_sid_period_inv = (1 << 13);      /* 0.25 in Q15 */

    st->log_en = 3500;
    st->old_log_en = 3500;
    /* low level noise for better performance in  DTX handover cases */

    st->cng_seed = RANDOM_INITSEED;

    st->hist_ptr = 0;

    /* Init isf_hist[] and decoder log frame energy */
    pv_memcpy((void *)st->isf, (void *)isf_init, M*sizeof(*isf_init));

    pv_memcpy((void *)st->isf_old, (void *)isf_init, M*sizeof(*isf_init));

    for (i = 0; i < DTX_HIST_SIZE; i++)
    {
        pv_memcpy((void *)&st->isf_hist[i * M], (void *)isf_init, M*sizeof(*isf_init));
        st->log_en_hist[i] = st->log_en;
    }

    st->dtxHangoverCount = DTX_HANG_CONST;
    st->decAnaElapsedCount = 32767;

    st->sid_frame = 0;
    st->valid_data = 0;
    st->dtxHangoverAdded = 0;

    st->dtxGlobalState = SPEECH;
    st->data_updated = 0;

    st->dither_seed = RANDOM_INITSEED;
    st->CN_dith = 0;

    return 0;
}


/*
     Table of new SPD synthesis states

                           |     previous SPD_synthesis_state
     Incoming              |
     frame_type            | SPEECH       | DTX           | DTX_MUTE
     ---------------------------------------------------------------
     RX_SPEECH_GOOD ,      |              |               |
     RX_SPEECH_PR_DEGRADED | SPEECH       | SPEECH        | SPEECH
     ----------------------------------------------------------------
     RX_SPEECH_BAD,        | SPEECH       | DTX           | DTX_MUTE
     ----------------------------------------------------------------
     RX_SID_FIRST,         | DTX          | DTX/(DTX_MUTE)| DTX_MUTE
     ----------------------------------------------------------------
     RX_SID_UPDATE,        | DTX          | DTX           | DTX
     ----------------------------------------------------------------
     RX_SID_BAD,           | DTX          | DTX/(DTX_MUTE)| DTX_MUTE
     ----------------------------------------------------------------
     RX_NO_DATA,           | SPEECH       | DTX/(DTX_MUTE)| DTX_MUTE
     RX_SPARE              |(class2 garb.)|               |
     ----------------------------------------------------------------
*/

/*----------------------------------------------------------------------------
; FUNCTION CODE
----------------------------------------------------------------------------*/

/*
 * Function    : dtx_dec_amr_wb
 */
int16 dtx_dec_amr_wb(
    dtx_decState * st,                    /* i/o : State struct         */
    int16 * exc2,                        /* o   : CN excitation        */
    int16 new_state,                     /* i   : New DTX state        */
    int16 isf[],                         /* o   : CN ISF vector        */
    int16 ** prms
)
{
    int16 log_en_index;
    int16 ind[7];
    int16 i, j;
    int16 int_fac;
    int16 gain;

    int32 L_isf[M], L_log_en_int, level32, ener32;
    int16 ptr;
    int16 tmp_int_length;
    int16 tmp, exp, exp0, log_en_int_e, log_en_int_m, level;

    /* This function is called if synthesis state is not SPEECH the globally passed  inputs to this function
     * are st->sid_frame st->valid_data st->dtxHangoverAdded new_state  (SPEECH, DTX, DTX_MUTE) */

    if ((st->dtxHangoverAdded != 0) &&
            (st->sid_frame != 0))
    {
        /* sid_first after dtx hangover period */
        /* or sid_upd after dtxhangover        */

        /* consider  twice the last frame */
        ptr = st->hist_ptr + 1;

        if (ptr == DTX_HIST_SIZE)
            ptr = 0;

        pv_memcpy((void *)&st->isf_hist[ptr * M], (void *)&st->isf_hist[st->hist_ptr * M], M*sizeof(*st->isf_hist));

        st->log_en_hist[ptr] = st->log_en_hist[st->hist_ptr];

        /* compute mean log energy and isf from decoded signal (SID_FIRST) */
        st->log_en = 0;
        for (i = 0; i < M; i++)
        {
            L_isf[i] = 0;
        }

        /* average energy and isf */
        for (i = 0; i < DTX_HIST_SIZE; i++)
        {
            /* Division by DTX_HIST_SIZE = 8 has been done in dtx_buffer log_en is in Q10 */
            st->log_en = add_int16(st->log_en, st->log_en_hist[i]);

            for (j = 0; j < M; j++)
            {
                L_isf[j] = add_int32(L_isf[j], (int32)(st->isf_hist[i * M + j]));
            }
        }

        /* st->log_en in Q9 */
        st->log_en >>=  1;

        /* Add 2 in Q9, in order to have only positive values for Pow2 */
        /* this value is subtracted back after Pow2 function */
        st->log_en += 1024;

        if (st->log_en < 0)
            st->log_en = 0;

        for (j = 0; j < M; j++)
        {
            st->isf[j] = (int16)(L_isf[j] >> 3);  /* divide by 8 */
        }

    }

    if (st->sid_frame != 0)
    {
        /* Set old SID parameters, always shift */
        /* even if there is no new valid_data   */

        pv_memcpy((void *)st->isf_old, (void *)st->isf, M*sizeof(*st->isf));

        st->old_log_en = st->log_en;

        if (st->valid_data != 0)           /* new data available (no CRC) */
        {
            /* st->true_sid_period_inv = 1.0f/st->since_last_sid; */
            /* Compute interpolation factor, since the division only works * for values of since_last_sid <
             * 32 we have to limit the      * interpolation to 32 frames                                  */
            tmp_int_length = st->since_last_sid;


            if (tmp_int_length > 32)
            {
                tmp_int_length = 32;
            }

            if (tmp_int_length >= 2)
            {
                st->true_sid_period_inv = div_16by16(1 << 10, shl_int16(tmp_int_length, 10));
            }
            else
            {
                st->true_sid_period_inv = 1 << 14;      /* 0.5 it Q15 */
            }

            ind[0] = Serial_parm(6, prms);
            ind[1] = Serial_parm(6, prms);
            ind[2] = Serial_parm(6, prms);
            ind[3] = Serial_parm(5, prms);
            ind[4] = Serial_parm(5, prms);

            Disf_ns(ind, st->isf);

            log_en_index = Serial_parm(6, prms);

            /* read background noise stationarity information */
            st->CN_dith = Serial_parm_1bit(prms);

            /* st->log_en = (float)log_en_index / 2.625 - 2.0;  */
            /* log2(E) in Q9 (log2(E) lies in between -2:22) */
            st->log_en = shl_int16(log_en_index, 15 - 6);

            /* Divide by 2.625  */
            st->log_en = mult_int16(st->log_en, 12483);
            /* Subtract 2 in Q9 is done later, after Pow2 function  */

            /* no interpolation at startup after coder reset        */
            /* or when SID_UPD has been received right after SPEECH */

            if ((st->data_updated == 0) || (st->dtxGlobalState == SPEECH))
            {
                pv_memcpy((void *)st->isf_old, (void *)st->isf, M*sizeof(*st->isf));

                st->old_log_en = st->log_en;
            }
        }                                  /* endif valid_data */
    }                                      /* endif sid_frame */


    if ((st->sid_frame != 0) && (st->valid_data != 0))
    {
        st->since_last_sid = 0;
    }
    /* Interpolate SID info */
    int_fac = shl_int16(st->since_last_sid, 10); /* Q10 */
    int_fac = mult_int16(int_fac, st->true_sid_period_inv);   /* Q10 * Q15 -> Q10 */

    /* Maximize to 1.0 in Q10 */

    if (int_fac > 1024)
    {
        int_fac = 1024;
    }
    int_fac = shl_int16(int_fac, 4);             /* Q10 -> Q14 */

    L_log_en_int = mul_16by16_to_int32(int_fac, st->log_en); /* Q14 * Q9 -> Q24 */

    for (i = 0; i < M; i++)
    {
        isf[i] = mult_int16(int_fac, st->isf[i]);/* Q14 * Q15 -> Q14 */
    }

    int_fac = 16384 - int_fac;         /* 1-k in Q14 */

    /* ( Q14 * Q9 -> Q24 ) + Q24 -> Q24 */
    L_log_en_int = mac_16by16_to_int32(L_log_en_int, int_fac, st->old_log_en);

    for (i = 0; i < M; i++)
    {
        /* Q14 + (Q14 * Q15 -> Q14) -> Q14 */
        isf[i] = add_int16(isf[i], mult_int16(int_fac, st->isf_old[i]));
        isf[i] = shl_int16(isf[i], 1);           /* Q14 -> Q15 */
    }

    /* If background noise is non-stationary, insert comfort noise dithering */
    if (st->CN_dith != 0)
    {
        CN_dithering(isf, &L_log_en_int, &st->dither_seed);
    }
    /* L_log_en_int corresponds to log2(E)+2 in Q24, i.e log2(gain)+1 in Q25 */
    /* Q25 -> Q16 */
    L_log_en_int >>= 9;

    /* Find integer part  */
    log_en_int_e = extract_h(L_log_en_int);

    /* Find fractional part */
    log_en_int_m = (int16)(sub_int32(L_log_en_int, L_deposit_h(log_en_int_e)) >> 1);

    /* Subtract 2 from L_log_en_int in Q9, i.e divide the gain by 2 (energy by 4) */
    /* Add 16 in order to have the result of pow2 in Q16 */
    log_en_int_e += 15;

    /* level = (float)( pow( 2.0f, log_en ) );  */
    level32 = power_of_2(log_en_int_e, log_en_int_m); /* Q16 */

    exp0 = normalize_amr_wb(level32);
    level32 <<= exp0;        /* level in Q31 */
    exp0 = 15 - exp0;
    level = (int16)(level32 >> 16);          /* level in Q15 */

    /* generate white noise vector */
    for (i = 0; i < L_FRAME; i++)
    {
        exc2[i] = noise_gen_amrwb(&(st->cng_seed)) >> 4;
    }

    /* gain = level / sqrt(ener) * sqrt(L_FRAME) */

    /* energy of generated excitation */
    ener32 = Dot_product12(exc2, exc2, L_FRAME, &exp);

    one_ov_sqrt_norm(&ener32, &exp);

    gain = extract_h(ener32);

    gain = mult_int16(level, gain);              /* gain in Q15 */

    exp += exp0;

    /* Multiply by sqrt(L_FRAME)=16, i.e. shift left by 4 */
    exp += 4;

    for (i = 0; i < L_FRAME; i++)
    {
        tmp = mult_int16(exc2[i], gain);         /* Q0 * Q15 */
        exc2[i] = shl_int16(tmp, exp);
    }


    if (new_state == DTX_MUTE)
    {
        /* mute comfort noise as it has been quite a long time since last SID update  was performed                            */

        tmp_int_length = st->since_last_sid;

        if (tmp_int_length > 32)
        {
            tmp_int_length = 32;
        }

        st->true_sid_period_inv = div_16by16(1 << 10, shl_int16(tmp_int_length, 10));

        st->since_last_sid = 0;
        st->old_log_en = st->log_en;
        /* subtract 1/8 in Q9 (energy), i.e -3/8 dB */
        st->log_en -= 64;
        if (st->log_en < 0)
            st->log_en = 0;
    }
    /* reset interpolation length timer if data has been updated.        */

    if ((st->sid_frame != 0) &&
            ((st->valid_data != 0) ||
             ((st->valid_data == 0) && (st->dtxHangoverAdded) != 0)))
    {
        st->since_last_sid = 0;
        st->data_updated = 1;
    }
    return 0;
}


/*----------------------------------------------------------------------------
; FUNCTION CODE
----------------------------------------------------------------------------*/

void dtx_dec_amr_wb_activity_update(
    dtx_decState * st,
    int16 isf[],
    int16 exc[])
{
    int16 i;

    int32 L_frame_en;
    int16 log_en_e, log_en_m, log_en;


    st->hist_ptr++;

    if (st->hist_ptr == DTX_HIST_SIZE)
    {
        st->hist_ptr = 0;
    }
    pv_memcpy((void *)&st->isf_hist[st->hist_ptr * M], (void *)isf, M*sizeof(*isf));


    /* compute log energy based on excitation frame energy in Q0 */
    L_frame_en = 0;
    for (i = 0; i < L_FRAME; i++)
    {
        L_frame_en = mac_16by16_to_int32(L_frame_en, exc[i], exc[i]);
    }
    L_frame_en >>= 1;

    /* log_en = (float)log10(L_frame_en/(float)L_FRAME)/(float)log10(2.0f); */
    amrwb_log_2(L_frame_en, &log_en_e, &log_en_m);

    /* convert exponent and mantissa to int16 Q7. Q7 is used to simplify averaging in dtx_enc */
    log_en = shl_int16(log_en_e, 7);             /* Q7 */
    log_en += log_en_m >> 8;

    /* Divide by L_FRAME = 256, i.e subtract 8 in Q7 = 1024 */
    log_en -= 1024;

    /* insert into log energy buffer */
    st->log_en_hist[st->hist_ptr] = log_en;

    return;
}


/*
     Table of new SPD synthesis states

                           |     previous SPD_synthesis_state
     Incoming              |
     frame_type            | SPEECH       | DTX           | DTX_MUTE
     ---------------------------------------------------------------
     RX_SPEECH_GOOD ,      |              |               |
     RX_SPEECH_PR_DEGRADED | SPEECH       | SPEECH        | SPEECH
     ----------------------------------------------------------------
     RX_SPEECH_BAD,        | SPEECH       | DTX           | DTX_MUTE
     ----------------------------------------------------------------
     RX_SID_FIRST,         | DTX          | DTX/(DTX_MUTE)| DTX_MUTE
     ----------------------------------------------------------------
     RX_SID_UPDATE,        | DTX          | DTX           | DTX
     ----------------------------------------------------------------
     RX_SID_BAD,           | DTX          | DTX/(DTX_MUTE)| DTX_MUTE
     ----------------------------------------------------------------
     RX_NO_DATA,           | SPEECH       | DTX/(DTX_MUTE)| DTX_MUTE
     RX_SPARE              |(class2 garb.)|               |
     ----------------------------------------------------------------
*/


/*----------------------------------------------------------------------------
; FUNCTION CODE
----------------------------------------------------------------------------*/

int16 rx_amr_wb_dtx_handler(
    dtx_decState * st,                    /* i/o : State struct     */
    int16 frame_type                     /* i   : Frame type       */
)
{
    int16 newState;
    int16 encState;

    /* DTX if SID frame or previously in DTX{_MUTE} and (NO_RX OR BAD_SPEECH) */



    if ((frame_type == RX_SID_FIRST)  ||
            (frame_type == RX_SID_UPDATE) ||
            (frame_type == RX_SID_BAD)    ||
            (((st->dtxGlobalState == DTX) ||
              (st->dtxGlobalState == DTX_MUTE)) &&
             ((frame_type == RX_NO_DATA)    ||
              (frame_type == RX_SPEECH_BAD) ||
              (frame_type == RX_SPEECH_LOST))))
    {
        newState = DTX;

        /* stay in mute for these input types */

        if ((st->dtxGlobalState == DTX_MUTE) &&
                ((frame_type == RX_SID_BAD) ||
                 (frame_type == RX_SID_FIRST) ||
                 (frame_type == RX_SPEECH_LOST) ||
                 (frame_type == RX_NO_DATA)))
        {
            newState = DTX_MUTE;
        }
        /* evaluate if noise parameters are too old                     */
        /* since_last_sid is reset when CN parameters have been updated */
        st->since_last_sid = add_int16(st->since_last_sid, 1);

        /* no update of sid parameters in DTX for a long while */

        if (st->since_last_sid > DTX_MAX_EMPTY_THRESH)
        {
            newState = DTX_MUTE;
        }
    }
    else
    {
        newState = SPEECH;
        st->since_last_sid = 0;
    }

    /* reset the decAnaElapsed Counter when receiving CNI data the first time, to robustify counter missmatch
     * after handover this might delay the bwd CNI analysis in the new decoder slightly. */

    if ((st->data_updated == 0) &&
            (frame_type == RX_SID_UPDATE))
    {
        st->decAnaElapsedCount = 0;
    }
    /* update the SPE-SPD DTX hangover synchronization */
    /* to know when SPE has added dtx hangover         */
    st->decAnaElapsedCount = add_int16(st->decAnaElapsedCount, 1);
    st->dtxHangoverAdded = 0;


    if ((frame_type == RX_SID_FIRST) ||
            (frame_type == RX_SID_UPDATE) ||
            (frame_type == RX_SID_BAD) ||
            (frame_type == RX_NO_DATA))
    {
        encState = DTX;
    }
    else
    {
        encState = SPEECH;
    }


    if (encState == SPEECH)
    {
        st->dtxHangoverCount = DTX_HANG_CONST;
    }
    else
    {

        if (st->decAnaElapsedCount > DTX_ELAPSED_FRAMES_THRESH)
        {
            st->dtxHangoverAdded = 1;
            st->decAnaElapsedCount = 0;
            st->dtxHangoverCount = 0;
        }
        else if (st->dtxHangoverCount == 0)
        {
            st->decAnaElapsedCount = 0;
        }
        else
        {
            st->dtxHangoverCount--;
        }
    }

    if (newState != SPEECH)
    {
        /* DTX or DTX_MUTE CN data is not in a first SID, first SIDs are marked as SID_BAD but will do
         * backwards analysis if a hangover period has been added according to the state machine above */

        st->sid_frame = 0;
        st->valid_data = 0;


        if (frame_type == RX_SID_FIRST)
        {
            st->sid_frame = 1;
        }
        else if (frame_type == RX_SID_UPDATE)
        {
            st->sid_frame = 1;
            st->valid_data = 1;
        }
        else if (frame_type == RX_SID_BAD)
        {
            st->sid_frame = 1;
            st->dtxHangoverAdded = 0;      /* use old data */
        }
    }
    return newState;
    /* newState is used by both SPEECH AND DTX synthesis routines */
}


/*----------------------------------------------------------------------------
; FUNCTION CODE
----------------------------------------------------------------------------*/

void aver_isf_history(
    int16 isf_old[],
    int16 indices[],
    int32 isf_aver[]
)
{
    int16 i, j, k;
    int16 isf_tmp[2 * M];
    int32 L_tmp;

    /* Memorize in isf_tmp[][] the ISF vectors to be replaced by */
    /* the median ISF vector prior to the averaging               */
    for (k = 0; k < 2; k++)
    {

        if (indices[k] + 1 != 0)
        {
            for (i = 0; i < M; i++)
            {
                isf_tmp[k * M + i] = isf_old[indices[k] * M + i];
                isf_old[indices[k] * M + i] = isf_old[indices[2] * M + i];
            }
        }
    }

    /* Perform the ISF averaging */
    for (j = 0; j < M; j++)
    {
        L_tmp = 0;

        for (i = 0; i < DTX_HIST_SIZE; i++)
        {
            L_tmp = add_int32(L_tmp, (int32)(isf_old[i * M + j]));
        }
        isf_aver[j] = L_tmp;
    }

    /* Retrieve from isf_tmp[][] the ISF vectors saved prior to averaging */
    for (k = 0; k < 2; k++)
    {

        if (indices[k] + 1 != 0)
        {
            for (i = 0; i < M; i++)
            {
                isf_old[indices[k] * M + i] = isf_tmp[k * M + i];
            }
        }
    }

    return;
}


/*----------------------------------------------------------------------------
; FUNCTION CODE
----------------------------------------------------------------------------*/

void find_frame_indices(
    int16 isf_old_tx[],
    int16 indices[],
    dtx_encState * st
)
{
    int32 L_tmp, summin, summax, summax2nd;
    int16 i, j, tmp;
    int16 ptr;

    /* Remove the effect of the oldest frame from the column */
    /* sum sumD[0..DTX_HIST_SIZE-1]. sumD[DTX_HIST_SIZE] is    */
    /* not updated since it will be removed later.           */

    tmp = DTX_HIST_SIZE_MIN_ONE;
    j = -1;
    for (i = 0; i < DTX_HIST_SIZE_MIN_ONE; i++)
    {
        j += tmp;
        st->sumD[i] = sub_int32(st->sumD[i], st->D[j]);
        tmp--;
    }

    /* Shift the column sum sumD. The element sumD[DTX_HIST_SIZE-1]    */
    /* corresponding to the oldest frame is removed. The sum of     */
    /* the distances between the latest isf and other isfs, */
    /* i.e. the element sumD[0], will be computed during this call. */
    /* Hence this element is initialized to zero.                   */

    for (i = DTX_HIST_SIZE_MIN_ONE; i > 0; i--)
    {
        st->sumD[i] = st->sumD[i - 1];
    }
    st->sumD[0] = 0;

    /* Remove the oldest frame from the distance matrix.           */
    /* Note that the distance matrix is replaced by a one-         */
    /* dimensional array to save static memory.                    */

    tmp = 0;
    for (i = 27; i >= 12; i -= tmp)
    {
        tmp++;
        for (j = tmp; j > 0; j--)
        {
            st->D[i - j + 1] = st->D[i - j - tmp];
        }
    }

    /* Compute the first column of the distance matrix D            */
    /* (squared Euclidean distances from isf1[] to isf_old_tx[][]). */

    ptr = st->hist_ptr;
    for (i = 1; i < DTX_HIST_SIZE; i++)
    {
        /* Compute the distance between the latest isf and the other isfs. */
        ptr--;

        if (ptr < 0)
        {
            ptr = DTX_HIST_SIZE_MIN_ONE;
        }
        L_tmp = 0;
        for (j = 0; j < M; j++)
        {
            tmp = sub_int16(isf_old_tx[st->hist_ptr * M + j], isf_old_tx[ptr * M + j]);
            L_tmp = mac_16by16_to_int32(L_tmp, tmp, tmp);
        }
        st->D[i - 1] = L_tmp;

        /* Update also the column sums. */
        st->sumD[0] = add_int32(st->sumD[0], st->D[i - 1]);
        st->sumD[i] = add_int32(st->sumD[i], st->D[i - 1]);
    }

    /* Find the minimum and maximum distances */
    summax = st->sumD[0];
    summin = st->sumD[0];
    indices[0] = 0;
    indices[2] = 0;
    for (i = 1; i < DTX_HIST_SIZE; i++)
    {

        if (st->sumD[i] > summax)
        {
            indices[0] = i;
            summax = st->sumD[i];
        }

        if (st->sumD[i] < summin)
        {
            indices[2] = i;
            summin = st->sumD[i];
        }
    }

    /* Find the second largest distance */
    summax2nd = -2147483647L;
    indices[1] = -1;
    for (i = 0; i < DTX_HIST_SIZE; i++)
    {

        if ((st->sumD[i] > summax2nd) && (i != indices[0]))
        {
            indices[1] = i;
            summax2nd = st->sumD[i];
        }
    }

    for (i = 0; i < 3; i++)
    {
        indices[i] = sub_int16(st->hist_ptr, indices[i]);

        if (indices[i] < 0)
        {
            indices[i] = add_int16(indices[i], DTX_HIST_SIZE);
        }
    }

    /* If maximum distance/MED_THRESH is smaller than minimum distance */
    /* then the median ISF vector replacement is not performed         */
    tmp = normalize_amr_wb(summax);
    summax <<= tmp;
    summin <<= tmp;
    L_tmp = mul_16by16_to_int32(amr_wb_round(summax), INV_MED_THRESH);

    if (L_tmp <= summin)
    {
        indices[0] = -1;
    }
    /* If second largest distance/MED_THRESH is smaller than     */
    /* minimum distance then the median ISF vector replacement is    */
    /* not performed                                                 */
    summax2nd = shl_int32(summax2nd, tmp);
    L_tmp = mul_16by16_to_int32(amr_wb_round(summax2nd), INV_MED_THRESH);

    if (L_tmp <= summin)
    {
        indices[1] = -1;
    }
    return;
}


/*----------------------------------------------------------------------------
; FUNCTION CODE
----------------------------------------------------------------------------*/

int16 dithering_control(dtx_encState * st)
{
    int16 i, tmp, mean, CN_dith, gain_diff;
    int32 ISF_diff;

    /* determine how stationary the spectrum of background noise is */
    ISF_diff = 0;
    for (i = 0; i < 8; i++)
    {
        ISF_diff = add_int32(ISF_diff, st->sumD[i]);
    }
    if ((ISF_diff >> 26) > 0)
    {
        CN_dith = 1;
    }
    else
    {
        CN_dith = 0;
    }

    /* determine how stationary the energy of background noise is */
    mean = 0;
    for (i = 0; i < DTX_HIST_SIZE; i++)
    {
        mean = add_int16(mean, st->log_en_hist[i]);
    }
    mean >>= 3;
    gain_diff = 0;
    for (i = 0; i < DTX_HIST_SIZE; i++)
    {
        tmp = sub_int16(st->log_en_hist[i], mean);
        tmp = tmp - (tmp < 0);

        gain_diff += tmp ^(tmp >> 15);    /*  tmp ^sign(tmp)  */;
    }
    if (gain_diff > GAIN_THR)
    {
        CN_dith = 1;
    }
    return CN_dith;
}


/*----------------------------------------------------------------------------
; FUNCTION CODE
----------------------------------------------------------------------------*/

void CN_dithering(
    int16 isf[M],
    int32 * L_log_en_int,
    int16 * dither_seed
)
{
    int16 temp, temp1, i, dither_fac, rand_dith;
    int16 rand_dith2;

    /* Insert comfort noise dithering for energy parameter */
    rand_dith = noise_gen_amrwb(dither_seed) >> 1;
    rand_dith2 = noise_gen_amrwb(dither_seed) >> 1;
    rand_dith += rand_dith2;
    *L_log_en_int = add_int32(*L_log_en_int, mul_16by16_to_int32(rand_dith, GAIN_FACTOR));

    if (*L_log_en_int < 0)
    {
        *L_log_en_int = 0;
    }
    /* Insert comfort noise dithering for spectral parameters (ISF-vector) */
    dither_fac = ISF_FACTOR_LOW;

    rand_dith = noise_gen_amrwb(dither_seed) >> 1;
    rand_dith2 = noise_gen_amrwb(dither_seed) >> 1;
    rand_dith +=  rand_dith2;
    temp = add_int16(isf[0], mult_int16_r(rand_dith, dither_fac));

    /* Make sure that isf[0] will not get negative values */
    if (temp < ISF_GAP)
    {
        isf[0] = ISF_GAP;
    }
    else
    {
        isf[0] = temp;
    }

    for (i = 1; i < M - 1; i++)
    {
        dither_fac = add_int16(dither_fac, ISF_FACTOR_STEP);

        rand_dith = noise_gen_amrwb(dither_seed) >> 1;
        rand_dith2 = noise_gen_amrwb(dither_seed) >> 1;
        rand_dith +=  rand_dith2;
        temp = add_int16(isf[i], mult_int16_r(rand_dith, dither_fac));
        temp1 = sub_int16(temp, isf[i - 1]);

        /* Make sure that isf spacing remains at least ISF_DITH_GAP Hz */
        if (temp1 < ISF_DITH_GAP)
        {
            isf[i] = isf[i - 1] + ISF_DITH_GAP;
        }
        else
        {
            isf[i] = temp;
        }
    }

    /* Make sure that isf[M-2] will not get values above 16384 */
    if (isf[M - 2] > 16384)
    {
        isf[M - 2] = 16384;
    }
    return;
}
