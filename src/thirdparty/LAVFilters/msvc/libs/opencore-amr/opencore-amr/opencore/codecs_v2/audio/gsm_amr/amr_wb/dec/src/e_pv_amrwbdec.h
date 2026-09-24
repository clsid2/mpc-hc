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

 Filename: e_pv_amrwbdec.h

*/

/*----------------------------------------------------------------------------
; CONTINUE ONLY IF NOT ALREADY DEFINED
----------------------------------------------------------------------------*/
#ifndef E_PV_AMRWBDEC_H
#define E_PV_AMRWBDEC_H

/*----------------------------------------------------------------------------
; INCLUDES
----------------------------------------------------------------------------*/

#include "pvamrwbdecoder_cnst.h"             /* coder constant parameters */
#include "dtx.h"

/*----------------------------------------------------------------------------
; MACROS
; Define module specific macros here
----------------------------------------------------------------------------*/

/*----------------------------------------------------------------------------
; DEFINES
; Include all pre-processor statements here.
----------------------------------------------------------------------------*/

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
    int16 old_exc[PIT_MAX + L_INTERPOL];  /* old excitation vector */
    int16 ispold[M];                      /* old isp (immittance spectral pairs)*/
    int16 isfold[M];                      /* old isf (frequency domain) */
    int16 isf_buf[L_MEANBUF * M];         /* isf buffer(frequency domain) */
    int16 past_isfq[M];                   /* past isf quantizer */
    int16 tilt_code;                      /* tilt of code */
    int16 Q_old;                          /* old scaling factor */
    int16 Qsubfr[4];                      /* old maximum scaling factor */
    int32 L_gc_thres;                     /* threshold for noise enhancer */
    int16 mem_syn_hi[M];                  /* modified synthesis memory (MSB) */
    int16 mem_syn_lo[M];                  /* modified synthesis memory (LSB) */
    int16 mem_deemph;                     /* speech deemph filter memory */
    int16 mem_sig_out[6];                 /* hp50 filter memory for synthesis */
    int16 mem_oversamp[2 * L_FILT];       /* synthesis oversampled filter memory */
    int16 mem_syn_hf[M16k];               /* HF synthesis memory */
    int16 mem_hf[2 * L_FILT16k];          /* HF band-pass filter memory */
    int16 mem_hf2[2 * L_FILT16k];         /* HF band-pass filter memory */
    int16 mem_hf3[2 * L_FILT16k];         /* HF band-pass filter memory */
    int16 seed;                           /* random memory for frame erasure */
    int16 seed2;                          /* random memory for HF generation */
    int16 old_T0;                         /* old pitch lag */
    int16 old_T0_frac;                    /* old pitch fraction lag */
    int16 lag_hist[5];
    int16 dec_gain[23];                   /* gain decoder memory */
    int16 seed3;                          /* random memory for lag concealment */
    int16 disp_mem[8];                    /* phase dispersion memory */
    int16 mem_hp400[6];                   /* hp400 filter memory for synthesis */

    int16 prev_bfi;
    int16 state;
    int16 first_frame;
    dtx_decState dtx_decSt;
    int16 vad_hist;

} Decoder_State;

typedef struct
{
    Decoder_State state;
    int16 ScratchMem[L_SUBFR + L_SUBFR16k + ((L_SUBFR + M + M16k +1)<<1) + \
                     (2*L_FRAME + 1) + PIT_MAX + L_INTERPOL + NB_SUBFR*(M+1) \
                     + 3*(M+L_SUBFR) + M16k];
} PV_AmrWbDec;


/*----------------------------------------------------------------------------
; END
----------------------------------------------------------------------------*/
#endif
