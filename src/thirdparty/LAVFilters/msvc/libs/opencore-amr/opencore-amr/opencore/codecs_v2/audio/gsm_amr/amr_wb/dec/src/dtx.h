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



 Pathname: ./cpp/include/dtx.h

------------------------------------------------------------------------------
 INCLUDE DESCRIPTION
    Static memory, constants and frametypes for the DTX
------------------------------------------------------------------------------
*/
#ifndef DTX_H
#define DTX_H


/*----------------------------------------------------------------------------
; INCLUDES
----------------------------------------------------------------------------*/

#ifdef __cplusplus
extern "C"
{
#endif

    /*----------------------------------------------------------------------------
    ; MACROS
    ; Define module specific macros here
    ----------------------------------------------------------------------------*/

    /*----------------------------------------------------------------------------
    ; EXTERNAL VARIABLES REFERENCES
    ----------------------------------------------------------------------------*/

    /*----------------------------------------------------------------------------
    ; DEFINES AND SIMPLE TYPEDEF'S
    ----------------------------------------------------------------------------*/

#define DTX_MAX_EMPTY_THRESH 50
#define DTX_HIST_SIZE 8
#define DTX_HIST_SIZE_MIN_ONE 7
#define DTX_ELAPSED_FRAMES_THRESH (24 + 7 -1)
#define DTX_HANG_CONST 7                   /* yields eight frames of SP HANGOVER  */
#define INV_MED_THRESH 14564
#define ISF_GAP  128                       /* 50 */
#define ONE_MINUS_ISF_GAP 16384 - ISF_GAP

#define ISF_GAP   128
#define ISF_DITH_GAP   448
#define ISF_FACTOR_LOW 256
#define ISF_FACTOR_STEP 2

#define GAIN_THR 180
#define GAIN_FACTOR 75

    typedef struct
    {
        int16 isf_hist[M * DTX_HIST_SIZE];
        int16 log_en_hist[DTX_HIST_SIZE];
        int16 hist_ptr;
        int16 log_en_index;
        int16 cng_seed;

        /* DTX handler stuff */
        int16 dtxHangoverCount;
        int16 decAnaElapsedCount;
        int32 D[28];
        int32 sumD[DTX_HIST_SIZE];
    } dtx_encState;

#define SPEECH 0
#define DTX 1
#define DTX_MUTE 2

#define TX_SPEECH 0
#define TX_SID_FIRST 1
#define TX_SID_UPDATE 2
#define TX_NO_DATA 3

#define RX_SPEECH_GOOD 0
#define RX_SPEECH_PROBABLY_DEGRADED 1
#define RX_SPEECH_LOST 2
#define RX_SPEECH_BAD 3
#define RX_SID_FIRST 4
#define RX_SID_UPDATE 5
#define RX_SID_BAD 6
#define RX_NO_DATA 7

    /*****************************************************************************
     *
     * DEFINITION OF DATA TYPES
     *****************************************************************************/

    typedef struct
    {
        int16 since_last_sid;
        int16 true_sid_period_inv;
        int16 log_en;
        int16 old_log_en;
        int16 level;
        int16 isf[M];
        int16 isf_old[M];
        int16 cng_seed;

        int16 isf_hist[M * DTX_HIST_SIZE];
        int16 log_en_hist[DTX_HIST_SIZE];
        int16 hist_ptr;

        int16 dtxHangoverCount;
        int16 decAnaElapsedCount;

        int16 sid_frame;
        int16 valid_data;
        int16 dtxHangoverAdded;

        int16 dtxGlobalState;                 /* contains previous state */
        /* updated in main decoder */

        int16 data_updated;                   /* marker to know if CNI data is ever renewed */

        int16 dither_seed;
        int16 CN_dith;

    } dtx_decState;

    int16 dtx_enc_init(dtx_encState ** st, int16 isf_init[]);
    int16 dtx_enc_reset(dtx_encState * st, int16 isf_init[]);
    void dtx_enc_exit(dtx_encState ** st);

    int16 dtx_enc(
        dtx_encState * st,                    /* i/o : State struct                                         */
        int16 isf[M],                        /* o   : CN ISF vector                                        */
        int16 * exc2,                        /* o   : CN excitation                                        */
        int16 ** prms
    );

    int16 dtx_buffer(
        dtx_encState * st,                    /* i/o : State struct                    */
        int16 isf_new[],                     /* i   : isf vector                      */
        int32 enr,                           /* i   : residual energy (in L_FRAME)    */
        int16 codec_mode
    );

    void tx_dtx_handler(dtx_encState * st,     /* i/o : State struct           */
                        int16 vad_flag,                      /* i   : vad decision           */
                        int16 * usedMode                     /* i/o : mode changed or not    */
                       );

    void Qisf_ns(
        int16 * isf1,                        /* input : ISF in the frequency domain (0..0.5) */
        int16 * isf_q,                       /* output: quantized ISF                        */
        int16 * indice                       /* output: quantization indices                 */
    );


    int16 dtx_dec_amr_wb_reset(dtx_decState * st, const int16 isf_init[]);

    int16 dtx_dec_amr_wb(
        dtx_decState * st,                    /* i/o : State struct                                          */
        int16 * exc2,                        /* o   : CN excitation                                          */
        int16 new_state,                     /* i   : New DTX state                                          */
        int16 isf[],                         /* o   : CN ISF vector                                          */
        int16 ** prms
    );

    void dtx_dec_amr_wb_activity_update(
        dtx_decState * st,
        int16 isf[],
        int16 exc[]);


    int16 rx_amr_wb_dtx_handler(
        dtx_decState * st,                    /* i/o : State struct     */
        int16 frame_type                     /* i   : Frame type       */
    );

    void Disf_ns(
        int16 * indice,                      /* input:  quantization indices                  */
        int16 * isf_q                        /* input : ISF in the frequency domain (0..0.5)  */
    );

    void aver_isf_history(
        int16 isf_old[],
        int16 indices[],
        int32 isf_aver[]
    );
    void find_frame_indices(
        int16 isf_old_tx[],
        int16 indices[],
        dtx_encState * st
    );

    int16 dithering_control(
        dtx_encState * st
    );
    void CN_dithering(
        int16 isf[M],
        int32 * L_log_en_int,
        int16 * dither_seed
    );

#ifdef __cplusplus
}
#endif

#endif  /*  DTX_H  */


