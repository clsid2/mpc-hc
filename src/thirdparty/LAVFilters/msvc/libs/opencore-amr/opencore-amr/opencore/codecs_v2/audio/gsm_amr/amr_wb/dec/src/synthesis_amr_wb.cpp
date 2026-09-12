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



 Filename: synthesis_amr_wb.cpp

------------------------------------------------------------------------------
 INPUT AND OUTPUT DEFINITIONS

     int16 Aq[],                           A(z)  : quantized Az
     int16 exc[],                          (i)   : excitation at 12kHz
     int16 Q_new,                          (i)   : scaling performed on exc
     int16 synth16k[],                     (o)   : 16kHz synthesis signal
     int16 prms,                           (i)   : compressed amr wb
     int16 HfIsf[],
     int16 nb_bits,
     int16 newDTXState,
     Decoder_State * st,                   (i/o) : State structure
     int16 bfi,                            (i)   : bad frame indicator
     int16 *ScratchMem


------------------------------------------------------------------------------
 FUNCTION DESCRIPTION

    Synthesis of signal at 16kHz with HF extension

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
; LOCAL FUNCTION DEFINITIONS
; Function Prototype declaration
----------------------------------------------------------------------------*/

/*----------------------------------------------------------------------------
; LOCAL STORE/BUFFER/POINTER DEFINITIONS
; Variable declaration - defined here and used outside this module
----------------------------------------------------------------------------*/
/* High Band encoding */
const int16 HP_gain[16] =
{
    3624, 4673, 5597, 6479, 7425, 8378, 9324, 10264,
    11210, 12206, 13391, 14844, 16770, 19655, 24289, 32728
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

void synthesis_amr_wb(
    int16 Aq[],              /* A(z)  : quantized Az               */
    int16 exc[],             /* (i)   : excitation at 12kHz        */
    int16 Q_new,             /* (i)   : scaling performed on exc   */
    int16 synth16k[],        /* (o)   : 16kHz synthesis signal     */
    int16 prms,              /* (i)   : parameter                  */
    int16 HfIsf[],
    int16 nb_bits,
    int16 newDTXState,
    Decoder_State * st,      /* (i/o) : State structure            */
    int16 bfi,               /* (i)   : bad frame indicator        */
    int16 *ScratchMem
)
{
    int16 i, fac, exp;
    int16 tmp;
    int16 ener, exp_ener;
    int32 L_tmp;
    int32 L_tmp2;

    int16 HF_corr_gain;
    int16 HF_gain_ind;
    int16 gain1, gain2;

    int16 *pt_synth;
    int16 *pt_HF;
    int16 *synth_hi =  ScratchMem;
    int16 *synth_lo = &ScratchMem[M + L_SUBFR];
    int16 *synth    = &synth_lo[M + L_SUBFR];
    int16 *HF       = &synth[L_SUBFR];
    int16 *Ap       = &HF[L_SUBFR16k];       /* High Frequency vector   */
    int16 *HfA      = &Ap[M16k + 1];
    int16 *pt_tmp;

    /*------------------------------------------------------------*
     * speech synthesis                                           *
     * ~~~~~~~~~~~~~~~~                                           *
     * - Find synthesis speech corresponding to exc2[].           *
     * - Perform fixed deemphasis and hp 50hz filtering.          *
     * - Oversampling from 12.8kHz to 16kHz.                      *
     *------------------------------------------------------------*/

    pv_memcpy((void *)synth_hi,
              (void *)st->mem_syn_hi,
              M*sizeof(*synth_hi));

    pv_memcpy((void *)synth_lo,
              (void *)st->mem_syn_lo,
              M*sizeof(*synth_lo));

    Syn_filt_32(Aq, M, exc, Q_new, synth_hi + M, synth_lo + M, L_SUBFR);

    pv_memcpy((void *)st->mem_syn_hi,
              (void *)(synth_hi + L_SUBFR),
              M*sizeof(*st->mem_syn_hi));

    pv_memcpy((void *)st->mem_syn_lo,
              (void *)(synth_lo + L_SUBFR),
              M*sizeof(*st->mem_syn_lo));

    deemphasis_32(synth_hi + M,
                  synth_lo + M,
                  synth,
                  PREEMPH_FAC,
                  L_SUBFR,
                  &(st->mem_deemph));

    highpass_50Hz_at_12k8(synth,
                          L_SUBFR,
                          st->mem_sig_out);

    oversamp_12k8_to_16k(synth,
                         L_SUBFR,
                         synth16k,
                         st->mem_oversamp,
                         ScratchMem);

    /*
     * HF noise synthesis
     * - Generate HF noise between 5.5 and 7.5 kHz.
     * - Set energy of noise according to synthesis tilt.
     *     tilt > 0.8 ==> - 14 dB (voiced)
     *     tilt   0.5 ==> - 6 dB  (voiced or noise)
     *     tilt < 0.0 ==>   0 dB  (noise)
     */

    /* generate white noise vector */
    pt_tmp = HF;
    for (i = L_SUBFR16k >> 2; i != 0 ; i--)
    {
        *(pt_tmp++) = noise_gen_amrwb(&(st->seed2)) >> 3;
        *(pt_tmp++) = noise_gen_amrwb(&(st->seed2)) >> 3;
        *(pt_tmp++) = noise_gen_amrwb(&(st->seed2)) >> 3;
        *(pt_tmp++) = noise_gen_amrwb(&(st->seed2)) >> 3;
    }
    /* energy of excitation */

    pt_tmp = exc;

    for (i = L_SUBFR >> 2; i != 0; i--)
    {
        *(pt_tmp) = add_int16(*(pt_tmp), 0x0004) >> 3;
        pt_tmp++;
        *(pt_tmp) = add_int16(*(pt_tmp), 0x0004) >> 3;
        pt_tmp++;
        *(pt_tmp) = add_int16(*(pt_tmp), 0x0004) >> 3;
        pt_tmp++;
        *(pt_tmp) = add_int16(*(pt_tmp), 0x0004) >> 3;
        pt_tmp++;
    }


    Q_new -= 3;

    ener = extract_h(Dot_product12(exc, exc, L_SUBFR, &exp_ener));
    exp_ener -= Q_new << 1;

    /* set energy of white noise to energy of excitation */

    tmp = extract_h(Dot_product12(HF, HF, L_SUBFR16k, &exp));

    if (tmp > ener)
    {
        tmp >>=  1;                 /* Be sure tmp < ener */
        exp += 1;
    }
    L_tmp = L_deposit_h(div_16by16(tmp, ener)); /* result is normalized */
    exp -= exp_ener;
    one_ov_sqrt_norm(&L_tmp, &exp);
    L_tmp = shl_int32(L_tmp, exp + 1); /* L_tmp x 2, L_tmp in Q31 */

    tmp = (int16)(L_tmp >> 16);    /* tmp = 2 x sqrt(ener_exc/ener_hf) */



    pt_tmp = HF;
    for (i = L_SUBFR16k >> 2; i != 0 ; i--)
    {
        *(pt_tmp) = (int16)(fxp_mul_16by16(*(pt_tmp), tmp) >> 15);
        pt_tmp++;
        *(pt_tmp) = (int16)(fxp_mul_16by16(*(pt_tmp), tmp) >> 15);
        pt_tmp++;
        *(pt_tmp) = (int16)(fxp_mul_16by16(*(pt_tmp), tmp) >> 15);
        pt_tmp++;
        *(pt_tmp) = (int16)(fxp_mul_16by16(*(pt_tmp), tmp) >> 15);
        pt_tmp++;
    }

    /* find tilt of synthesis speech (tilt: 1=voiced, -1=unvoiced) */

    highpass_400Hz_at_12k8(synth, L_SUBFR, st->mem_hp400);

    L_tmp = 1L;
    L_tmp2 = 1L;


    L_tmp = mac_16by16_to_int32(L_tmp, synth[0], synth[0]);

    for (i = 1; i < L_SUBFR; i++)
    {
        L_tmp  = mac_16by16_to_int32(L_tmp,  synth[i], synth[i    ]);
        L_tmp2 = mac_16by16_to_int32(L_tmp2, synth[i], synth[i - 1]);
    }


    exp = normalize_amr_wb(L_tmp);

    ener = (int16)((L_tmp << exp) >> 16);   /* ener = r[0] */
    tmp  = (int16)((L_tmp2 << exp) >> 16);    /* tmp = r[1] */

    if (tmp > 0)
    {
        fac = div_16by16(tmp, ener);
    }
    else
    {
        fac = 0;
    }

    /* modify energy of white noise according to synthesis tilt */
    gain1 = 32767 - fac;
    gain2 = mult_int16(gain1, 20480);
    gain2 = shl_int16(gain2, 1);

    if (st->vad_hist > 0)
    {
        tmp  = gain2 - 1;
    }
    else
    {
        tmp  = gain1 - 1;
    }


    if (tmp != 0)
    {
        tmp++;
    }

    if (tmp < 3277)
    {
        tmp = 3277;                        /* 0.1 in Q15 */

    }


    if ((nb_bits >= NBBITS_24k) && (bfi == 0))
    {
        /* HF correction gain */
        HF_gain_ind = prms;
        HF_corr_gain = HP_gain[HF_gain_ind];

        pt_tmp = HF;
        for (i = L_SUBFR16k >> 2; i != 0 ; i--)
        {
            *(pt_tmp) = mult_int16(*(pt_tmp), HF_corr_gain) << 1;
            pt_tmp++;
            *(pt_tmp) = mult_int16(*(pt_tmp), HF_corr_gain) << 1;
            pt_tmp++;
            *(pt_tmp) = mult_int16(*(pt_tmp), HF_corr_gain) << 1;
            pt_tmp++;
            *(pt_tmp) = mult_int16(*(pt_tmp), HF_corr_gain) << 1;
            pt_tmp++;
        }

        /* HF gain */
    }
    else
    {
        pt_tmp = HF;
        for (i = L_SUBFR16k >> 2; i != 0 ; i--)
        {
            *(pt_tmp) = mult_int16(*(pt_tmp), tmp);
            pt_tmp++;
            *(pt_tmp) = mult_int16(*(pt_tmp), tmp);
            pt_tmp++;
            *(pt_tmp) = mult_int16(*(pt_tmp), tmp);
            pt_tmp++;
            *(pt_tmp) = mult_int16(*(pt_tmp), tmp);
            pt_tmp++;
        }
    }


    if ((nb_bits <= NBBITS_7k) && (newDTXState == SPEECH))
    {
        isf_extrapolation(HfIsf);
        Isp_Az(HfIsf, HfA, M16k, 0);

        weight_amrwb_lpc(HfA, Ap, 29491, M16k);     /* fac=0.9 */

        wb_syn_filt(Ap,
                    M16k,
                    HF,
                    HF,
                    L_SUBFR16k,
                    st->mem_syn_hf,
                    1,
                    ScratchMem);
    }
    else
    {
        /* synthesis of noise: 4.8kHz..5.6kHz --> 6kHz..7kHz */
        weight_amrwb_lpc(Aq, Ap, 19661, M);         /* fac=0.6 */

        wb_syn_filt(Ap,
                    M,
                    HF,
                    HF,
                    L_SUBFR16k,
                    st->mem_syn_hf + (M16k - M),
                    1,
                    ScratchMem);
    }

    /* noise Band Pass filtering (1ms of delay) */
    band_pass_6k_7k(HF,
                    L_SUBFR16k,
                    st->mem_hf,
                    ScratchMem);


    if (nb_bits >= NBBITS_24k)
    {
        /* Low Pass filtering (7 kHz) */
        low_pass_filt_7k(HF,
                         L_SUBFR16k,
                         st->mem_hf3,
                         ScratchMem);
    }
    /* add filtered HF noise to speech synthesis */

    pt_synth = synth16k;
    pt_HF = HF;

    for (i = L_SUBFR16k >> 1; i != 0; i--)
    {
        *(pt_synth) = add_int16(*(pt_synth), *(pt_HF++)); /* check 16 bit saturation */
        pt_synth++;
        *(pt_synth) = add_int16(*(pt_synth), *(pt_HF++));
        pt_synth++;
    }

}

