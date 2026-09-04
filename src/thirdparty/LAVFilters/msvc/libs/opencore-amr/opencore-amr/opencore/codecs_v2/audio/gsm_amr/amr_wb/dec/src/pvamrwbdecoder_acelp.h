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



 Pathname: ./cpp/include/pvamrwbdecoder_acelp.h

------------------------------------------------------------------------------
 INCLUDE DESCRIPTION

------------------------------------------------------------------------------
*/

#ifndef PVAMRWBDECODER_ACELP_H
#define PVAMRWBDECODER_ACELP_H


/*----------------------------------------------------------------------------
; INCLUDES
----------------------------------------------------------------------------*/

#include "pv_amr_wb_type_defs.h"
#include "pvamrwbdecoder_mem_funcs.h"

#ifdef __cplusplus
extern "C"
{
#endif

    /*-----------------------------------------------------------------*
     *                        LPC prototypes                           *
     *-----------------------------------------------------------------*/

    void isf_extrapolation(int16 HfIsf[]);

    void Init_Lagconc(int16 lag_hist[]);
    void lagconceal(
        int16 gain_hist[],                   /* (i) : Gain history     */
        int16 lag_hist[],                    /* (i) : Subframe size         */
        int16 * T0,
        int16 * old_T0,
        int16 * seed,
        int16 unusable_frame
    );

    void agc2_amr_wb(
        int16 * sig_in,                      /* input : postfilter input signal  */
        int16 * sig_out,                     /* in/out: postfilter output signal */
        int16 l_trm                          /* input : subframe size            */
    );

    void low_pass_filt_7k_init(int16 mem[]);
    void low_pass_filt_7k(
        int16 signal[],                      /* input:  signal                  */
        int16 lg,                            /* input:  length of input         */
        int16 mem[],                         /* in/out: memory (size=30)        */
        int16 x[]
    );

    int16 median5(int16 x[]);

    void Isp_Az(
        int16 isp[],                         /* (i) Q15 : Immittance spectral pairs            */
        int16 a[],                           /* (o) Q12 : predictor coefficients (order = M)   */
        int16 m,
        int16 adaptive_scaling               /* (i) 0   : adaptive scaling disabled */
        /*     1   : adaptive scaling enabled  */
    );
    void Isf_isp(
        int16 isf[],                         /* (i) Q15 : isf[m] normalized (range: 0.0<=val<=0.5) */
        int16 isp[],                         /* (o) Q15 : isp[m] (range: -1<=val<1)                */
        int16 m                              /* (i)     : LPC order                                */
    );
    void interpolate_isp(
        int16 isp_old[],                     /* input : isps from past frame              */
        int16 isp_new[],                     /* input : isps from present frame           */
        const int16 frac[],                  /* input : fraction for 3 first subfr (Q15)  */
        int16 Az[]                           /* output: LP coefficients in 4 subframes    */
    );
    void weight_amrwb_lpc(
        int16 a[],                           /* (i) Q12 : a[m+1]  LPC coefficients             */
        int16 ap[],                          /* (o) Q12 : Spectral expanded LPC coefficients   */
        int16 gamma,                         /* (i) Q15 : Spectral expansion factor.           */
        int16 m                              /* (i)     : LPC order.                           */
    );


    /*-----------------------------------------------------------------*
     *                        isf quantizers                           *
     *-----------------------------------------------------------------*/

    void Disf_ns(
        int16 * indice,                      /* input:  quantization indices                  */
        int16 * isf_q                        /* input : ISF in the frequency domain (0..0.5)  */
    );

    void Dpisf_2s_46b(
        int16 * indice,                      /* input:  quantization indices                       */
        int16 * isf_q,                       /* output: quantized ISF in frequency domain (0..0.5) */
        int16 * past_isfq,                   /* i/0   : past ISF quantizer                    */
        int16 * isfold,                      /* input : past quantized ISF                    */
        int16 * isf_buf,                     /* input : isf buffer                                                        */
        int16 bfi,                           /* input : Bad frame indicator                   */
        int16 enc_dec
    );
    void Dpisf_2s_36b(
        int16 * indice,                      /* input:  quantization indices                       */
        int16 * isf_q,                       /* output: quantized ISF in frequency domain (0..0.5) */
        int16 * past_isfq,                   /* i/0   : past ISF quantizer                    */
        int16 * isfold,                      /* input : past quantized ISF                    */
        int16 * isf_buf,                     /* input : isf buffer                                                        */
        int16 bfi,                           /* input : Bad frame indicator                   */
        int16 enc_dec
    );


    void Reorder_isf(
        int16 * isf,                         /* (i/o) Q15: ISF in the frequency domain (0..0.5) */
        int16 min_dist,                      /* (i) Q15  : minimum distance to keep             */
        int16 n                              /* (i)      : number of ISF                        */
    );

    /*-----------------------------------------------------------------*
     *                       filter prototypes                         *
     *-----------------------------------------------------------------*/

    void oversamp_12k8_to_16k_init(
        int16 mem[]                          /* output: memory (2*NB_COEF_UP) set to zeros  */
    );
    void oversamp_12k8_to_16k(
        int16 sig12k8[],                     /* input:  signal to oversampling  */
        int16 lg,                            /* input:  length of input         */
        int16 sig16k[],                      /* output: oversampled signal      */
        int16 mem[],                         /* in/out: memory (2*NB_COEF_UP)   */
        int16 signal[]
    );

    void highpass_50Hz_at_12k8_init(int16 mem[]);
    void highpass_50Hz_at_12k8(
        int16 signal[],                      /* input/output signal */
        int16 lg,                            /* lenght of signal    */
        int16 mem[]                          /* filter memory [6]   */
    );
    void highpass_400Hz_at_12k8_init(int16 mem[]);
    void highpass_400Hz_at_12k8(
        int16 signal[],                      /* input/output signal */
        int16 lg,                            /* lenght of signal    */
        int16 mem[]                          /* filter memory [6]   */
    );

    void band_pass_6k_7k_init(int16 mem[]);
    void band_pass_6k_7k(
        int16 signal[],                      /* input:  signal                  */
        int16 lg,                            /* input:  length of input         */
        int16 mem[],                         /* in/out: memory (size=30)        */
        int16 x[]
    );


    void preemph_amrwb_dec(
        int16 x[],                           /* (i/o)   : input signal overwritten by the output */
        int16 mu,                            /* (i) Q15 : preemphasis coefficient                */
        int16 lg                             /* (i)     : lenght of filtering                    */
    );

    void deemphasis_32(
        int16 x_hi[],                        /* (i)     : input signal (bit31..16) */
        int16 x_lo[],                        /* (i)     : input signal (bit15..4)  */
        int16 y[],                           /* (o)     : output signal (x16)      */
        int16 mu,                            /* (i) Q15 : deemphasis factor        */
        int16 L,                             /* (i)     : vector size              */
        int16 * mem                          /* (i/o)   : memory (y[-1])           */
    );


    void wb_syn_filt(
        int16 a[],                           /* (i) Q12 : a[m+1] prediction coefficients           */
        int16 m,                             /* (i)     : order of LP filter                       */
        int16 x[],                           /* (i)     : input signal                             */
        int16 y[],                           /* (o)     : output signal                            */
        int16 lg,                            /* (i)     : size of filtering                        */
        int16 mem[],                         /* (i/o)   : memory associated with this filtering.   */
        int16 update,                        /* (i)     : 0=no update, 1=update of memory.         */
        int16 y_buf[]
    );
    void Syn_filt_32(
        int16 a[],                           /* (i) Q12 : a[m+1] prediction coefficients */
        int16 m,                             /* (i)     : order of LP filter             */
        int16 exc[],                         /* (i) Qnew: excitation (exc[i] >> Qnew)    */
        int16 Qnew,                          /* (i)     : exc scaling = 0(min) to 8(max) */
        int16 sig_hi[],                      /* (o) /16 : synthesis high                 */
        int16 sig_lo[],                      /* (o) /16 : synthesis low                  */
        int16 lg                             /* (i)     : size of filtering              */
    );

    /*-----------------------------------------------------------------*
     *                       pitch prototypes                          *
     *-----------------------------------------------------------------*/


    void Pred_lt4(
        int16 exc[],                         /* in/out: excitation buffer */
        int16 T0,                            /* input : integer pitch lag */
        int16 frac,                          /* input : fraction of lag   */
        int16 L_subfr                        /* input : subframe size     */
    );

    /*-----------------------------------------------------------------*
     *                       gain prototypes                           *
     *-----------------------------------------------------------------*/


    void dec_gain2_amr_wb_init(
        int16 * mem                          /* output  : memory (4 words)      */
    );
    void dec_gain2_amr_wb(
        int16 index,                         /* (i)     :index of quantization.       */
        int16 nbits,                         /* (i)     : number of bits (6 or 7)     */
        int16 code[],                        /* (i) Q9  :Innovative vector.           */
        int16 L_subfr,                       /* (i)     :Subframe lenght.             */
        int16 * gain_pit,                    /* (o) Q14 :Pitch gain.                  */
        int32 * gain_cod,                    /* (o) Q16 :Code gain.                   */
        int16 bfi,                           /* (i)     :bad frame indicator          */
        int16 prev_bfi,                      /* (i) : Previous BF indicator      */
        int16 state,                         /* (i) : State of BFH               */
        int16 unusable_frame,                /* (i) : UF indicator            */
        int16 vad_hist,                      /* (i)         :number of non-speech frames  */
        int16 * mem                          /* (i/o)   : memory (4 words)      */
    );

    /*-----------------------------------------------------------------*
     *                       acelp prototypes                          *
     *-----------------------------------------------------------------*/

    void dec_acelp_2p_in_64(
        int16 index,                         /* (i) :    12 bits index                                  */
        int16 code[]                         /* (o) :Q9  algebraic (fixed) codebook excitation          */
    );

    void dec_acelp_4p_in_64(
        int16 index[],                       /* (i) : index (20): 5+5+5+5 = 20 bits.                 */
        /* (i) : index (36): 9+9+9+9 = 36 bits.                 */
        /* (i) : index (44): 13+9+13+9 = 44 bits.               */
        /* (i) : index (52): 13+13+13+13 = 52 bits.             */
        /* (i) : index (64): 2+2+2+2+14+14+14+14 = 64 bits.     */
        /* (i) : index (72): 10+2+10+2+10+14+10+14 = 72 bits.   */
        /* (i) : index (88): 11+11+11+11+11+11+11+11 = 88 bits. */
        int16 nbbits,                        /* (i) : 20, 36, 44, 52, 64, 72 or 88 bits              */
        int16 code[]                         /* (o) Q9: algebraic (fixed) codebook excitation        */
    );
    void Pit_shrp(
        int16 * x,                           /* in/out: impulse response (or algebraic code) */
        int16 pit_lag,                       /* input : pitch lag                            */
        int16 sharp,                         /* input : pitch sharpening factor (Q15)        */
        int16 L_subfr                        /* input : subframe size                        */
    );


    /*-----------------------------------------------------------------*
     *                        others prototypes                        *
     *-----------------------------------------------------------------*/

    int16 voice_factor(                       /* (o) Q15 : factor (-1=unvoiced to 1=voiced) */
        int16 exc[],                         /* (i) Q_exc: pitch excitation                */
        int16 Q_exc,                         /* (i)     : exc format                       */
        int16 gain_pit,                      /* (i) Q14 : gain of pitch                    */
        int16 code[],                        /* (i) Q9  : Fixed codebook excitation        */
        int16 gain_code,                     /* (i) Q0  : gain of code                     */
        int16 L_subfr                        /* (i)     : subframe length                  */
    );

    void scale_signal(
        int16 x[],                           /* (i/o) : signal to scale               */
        int16 lg,                            /* (i)   : size of x[]                   */
        int16 exp                            /* (i)   : exponent: x = round(x << exp) */
    );

    int16 noise_gen_amrwb(int16 * seed);


    void phase_dispersion(
        int16 gain_code,                     /* (i) Q0  : gain of code             */
        int16 gain_pit,                      /* (i) Q14 : gain of pitch            */
        int16 code[],                        /* (i/o)   : code vector              */
        int16 mode,                          /* (i)     : level, 0=hi, 1=lo, 2=off */
        int16 disp_mem[],                    /* (i/o)   :  memory (size = 8) */
        int16 ScratchMem[]
    );

#ifdef __cplusplus
}
#endif

#endif  /* ACELP_H */

