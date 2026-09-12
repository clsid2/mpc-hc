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
 Filename: vad2.h

------------------------------------------------------------------------------
 INCLUDE DESCRIPTION

 This file contains all the constant definitions, prototype and structure
 definitions needed by vad_2.c

------------------------------------------------------------------------------
*/

/*----------------------------------------------------------------------------
; CONTINUE ONLY IF NOT ALREADY DEFINED
----------------------------------------------------------------------------*/
#ifndef vad_2_h
#define vad_2_h "$Id $"

/*----------------------------------------------------------------------------
; INCLUDES
----------------------------------------------------------------------------*/
#include "typedef.h"

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

#define     YES     1
#define     NO      0
#define     ON      1
#define     OFF     0
#define     TRUE        1
#define     FALSE       0

#define     FRM_LEN                 80
#define     DELAY                   24
#define     FFT_LEN                 128

#define     NUM_CHAN                16
#define     LO_CHAN                 0
#define     HI_CHAN                 15

#define     UPDATE_THLD             35
#define     HYSTER_CNT_THLD         6
#define     UPDATE_CNT_THLD         50

#define     SHIFT_STATE_0       0       /* channel energy scaled as 22,9 */
#define     SHIFT_STATE_1       1       /* channel energy scaled as 27,4 */

#define     NOISE_FLOOR_CHAN_0  512     /* 1.0    scaled as 22,9 */
#define     MIN_CHAN_ENRG_0     32      /* 0.0625 scaled as 22,9 */
#define     MIN_NOISE_ENRG_0    32      /* 0.0625 scaled as 22,9 */
#define     INE_NOISE_0     8192        /* 16.0   scaled as 22,9 */
#define     FRACTIONAL_BITS_0   9       /* used as input to fn10Log10() */

#define     NOISE_FLOOR_CHAN_1  16      /* 1.0    scaled as 27,4 */
#define     MIN_CHAN_ENRG_1     1       /* 0.0625 scaled as 27,4 */
#define     MIN_NOISE_ENRG_1    1       /* 0.0625 scaled as 27,4 */
#define     INE_NOISE_1     256     /* 16.0   scaled as 27,4 */
#define     FRACTIONAL_BITS_1   4       /* used as input to fn10Log10() */

#define     STATE_1_TO_0_SHIFT_R    (FRACTIONAL_BITS_1-FRACTIONAL_BITS_0)   /* state correction factor */
#define     STATE_0_TO_1_SHIFT_R    (FRACTIONAL_BITS_0-FRACTIONAL_BITS_1)   /* state correction factor */

#define         HIGH_ALPHA              29491       /* 0.9 scaled as 0,15 */
#define         LOW_ALPHA               22938       /* 0.7 scaled as 0,15 */
#define         ALPHA_RANGE             (HIGH_ALPHA - LOW_ALPHA)
#define         DEV_THLD                7168        /* 28.0 scaled as 7,8 */

#define         PRE_EMP_FAC             (-26214)    /* -0.8 scaled as 0,15 */

#define         CEE_SM_FAC              18022       /* 0.55 scaled as 0,15 */
#define         ONE_MINUS_CEE_SM_FAC    14746       /* 0.45 scaled as 0,15 */

#define         CNE_SM_FAC              3277        /* 0.1 scaled as 0,15 */
#define         ONE_MINUS_CNE_SM_FAC    29491       /* 0.9 scaled as 0,15 */

#define         FFT_HEADROOM            2
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
        Word16 pre_emp_mem;
        Word16 update_cnt;
        Word16 hyster_cnt;
        Word16 last_update_cnt;
        Word16 ch_enrg_long_db[NUM_CHAN];   /* scaled as 7,8  */

        Word32 Lframe_cnt;
        Word32 Lch_enrg[NUM_CHAN];  /* scaled as 22,9 or 27,4 */
        Word32 Lch_noise[NUM_CHAN]; /* scaled as 22,9 */

        Word16 last_normb_shift;    /* last block norm shift count */

        Word16 tsnr;            /* total signal-to-noise ratio in dB (scaled as 7,8) */
        Word16 hangover;
        Word16 burstcount;
        Word16 fupdate_flag;        /* forced update flag from previous frame */
        Word16 negSNRvar;       /* Negative SNR variance (scaled as 7,8) */
        Word16 negSNRbias;      /* sensitivity bias from negative SNR variance (scaled as 15,0) */

        Word16 shift_state;     /* use 22,9 or 27,4 scaling for ch_enrg[] */

        Word32 L_R0;
        Word32 L_Rmax;
        Flag   LTP_flag;        /* Use to indicate the the LTP gain is > LTP_THRESH */

    } vadState2;
    /*----------------------------------------------------------------------------
    ; GLOBAL FUNCTION DEFINITIONS
    ; Function Prototype declaration
    ----------------------------------------------------------------------------*/
    Word16  vad2(Word16 *farray_ptr, vadState2 *st, Flag *pOverflow);
    Word16 vad2_init(vadState2 **st);
    Word16 vad2_reset(vadState2 *st);
    void    vad2_exit(vadState2 **state);

    void    r_fft(Word16 *farray_ptr, Flag *pOverflow);

    void    LTP_flag_update(vadState2 *st, Word16 mode, Flag *pOverflow);

    /*----------------------------------------------------------------------------
    ; END
    ----------------------------------------------------------------------------*/
#ifdef __cplusplus
}
#endif

#endif /* _VAD2_H_ */


