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
********************************************************************************
**-------------------------------------------------------------------------**
**                                                                         **
**     GSM AMR-NB speech codec   R98   Version 7.5.0   March 2, 2001       **
**                               R99   Version 3.2.0                       **
**                               REL-4 Version 4.0.0                       **
**                                                                         **
**-------------------------------------------------------------------------**
********************************************************************************
*
*      File             : cnst_vad.h
*      Purpose          : Constants and definitions for VAD
*
********************************************************************************
*/
#ifndef cnst_vad_h
#define cnst_vad_h "$Id $"

#define FRAME_LEN 160    /* Length (samples) of the input frame          */
#define COMPLEN 9        /* Number of sub-bands used by VAD              */
#define INV_COMPLEN 3641 /* 1.0/COMPLEN*2^15                             */
#define LOOKAHEAD 40     /* length of the lookahead used by speech coder */

#define UNITY 512        /* Scaling used with SNR calculation            */
#define UNIRSHFT 6       /* = log2(MAX_16/UNITY)                         */

#define TONE_THR (Word16)(0.65*MAX_16) /* Threshold for tone detection   */

/* Constants for background spectrum update */
#define ALPHA_UP1   (Word16)((1.0 - 0.95)*MAX_16)  /* Normal update, upwards:   */
#define ALPHA_DOWN1 (Word16)((1.0 - 0.936)*MAX_16) /* Normal update, downwards  */
#define ALPHA_UP2   (Word16)((1.0 - 0.985)*MAX_16) /* Forced update, upwards    */
#define ALPHA_DOWN2 (Word16)((1.0 - 0.943)*MAX_16) /* Forced update, downwards  */
#define ALPHA3      (Word16)((1.0 - 0.95)*MAX_16)  /* Update downwards          */
#define ALPHA4      (Word16)((1.0 - 0.9)*MAX_16)   /* For stationary estimation */
#define ALPHA5      (Word16)((1.0 - 0.5)*MAX_16)   /* For stationary estimation */

/* Constants for VAD threshold */
#define VAD_THR_HIGH 1260 /* Highest threshold                 */
#define VAD_THR_LOW  720  /* Lowest threshold                  */
#define VAD_P1 0          /* Noise level for highest threshold */
#define VAD_P2 6300       /* Noise level for lowest threshold  */
#define VAD_SLOPE (Word16)(MAX_16*(float)(VAD_THR_LOW-VAD_THR_HIGH)/(float)(VAD_P2-VAD_P1))

/* Parameters for background spectrum recovery function */
#define STAT_COUNT 20         /* threshold of stationary detection counter         */
#define STAT_COUNT_BY_2 10    /* threshold of stationary detection counter         */
#define CAD_MIN_STAT_COUNT 5  /* threshold of stationary detection counter         */

#define STAT_THR_LEVEL 184    /* Threshold level for stationarity detection        */
#define STAT_THR 1000         /* Threshold for stationarity detection              */

/* Limits for background noise estimate */
#define NOISE_MIN 40          /* minimum */
#define NOISE_MAX 16000       /* maximum */
#define NOISE_INIT 150        /* initial */

/* Constants for VAD hangover addition */
#define HANG_NOISE_THR 100
#define BURST_LEN_HIGH_NOISE 4
#define HANG_LEN_HIGH_NOISE 7
#define BURST_LEN_LOW_NOISE 5
#define HANG_LEN_LOW_NOISE 4

/* Thresholds for signal power */
#define VAD_POW_LOW (Word32)15000     /* If input power is lower,                    */
/*     VAD is set to 0                         */
#define POW_PITCH_THR (Word32)343040  /* If input power is lower, pitch              */
/*     detection is ignored                    */

#define POW_COMPLEX_THR (Word32)15000 /* If input power is lower, complex            */
/* flags  value for previous frame  is un-set  */


/* Constants for the filter bank */
#define LEVEL_SHIFT 0      /* scaling                                  */
#define COEFF3   13363     /* coefficient for the 3rd order filter     */
#define COEFF5_1 21955     /* 1st coefficient the for 5th order filter */
#define COEFF5_2 6390      /* 2nd coefficient the for 5th order filter */

/* Constants for pitch detection */
#define LTHRESH 4
#define NTHRESH 4

/* Constants for complex signal VAD  */
#define CVAD_THRESH_ADAPT_HIGH  (Word16)(0.6 * MAX_16) /* threshold for adapt stopping high    */
#define CVAD_THRESH_ADAPT_LOW  (Word16)(0.5 * MAX_16)  /* threshold for adapt stopping low     */
#define CVAD_THRESH_IN_NOISE  (Word16)(0.65 * MAX_16)  /* threshold going into speech on       */
/* a short term basis                   */

#define CVAD_THRESH_HANG  (Word16)(0.70 * MAX_16)      /* threshold                            */
#define CVAD_HANG_LIMIT  (Word16)(100)                 /* 2 second estimation time             */
#define CVAD_HANG_LENGTH  (Word16)(250)                /* 5 second hangover                    */

#define CVAD_LOWPOW_RESET (Word16) (0.40 * MAX_16)     /* init in low power segment            */
#define CVAD_MIN_CORR (Word16) (0.40 * MAX_16)         /* lowest adaptation value              */

#define CVAD_BURST 20                                  /* speech burst length for speech reset */
#define CVAD_ADAPT_SLOW (Word16)(( 1.0 - 0.98) * MAX_16)        /* threshold for slow adaption */
#define CVAD_ADAPT_FAST (Word16)((1.0 - 0.92) * MAX_16)         /* threshold for fast adaption */
#define CVAD_ADAPT_REALLY_FAST (Word16)((1.0 - 0.80) * MAX_16)  /* threshold for really fast   */
/* adaption                    */

#endif
