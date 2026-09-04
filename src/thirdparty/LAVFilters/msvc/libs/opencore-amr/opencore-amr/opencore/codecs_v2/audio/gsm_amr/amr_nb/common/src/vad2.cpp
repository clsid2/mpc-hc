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
 Filename: vad2.cpp
 Functions:

------------------------------------------------------------------------------
 MODULE DESCRIPTION


------------------------------------------------------------------------------
*/

/*----------------------------------------------------------------------------
; INCLUDES
----------------------------------------------------------------------------*/
#include "typedef.h"
#include "cnst.h"
#include "log2.h"
#include "pow2.h"
#include "sub.h"
#include "l_shr_r.h"
#include "abs_s.h"
#include "norm_s.h"
#include "shl.h"
#include "l_add.h"
#include "shr_r.h"
#include "add.h"
#include "mult.h"
#include "l_shr.h"
#include "mpy_32_16.h"
#include "l_mac.h"
#include "l_extract.h"
#include "l_sub.h"
#include "l_mult.h"
#include "round.h"
#include "shr.h"
#include "l_shl.h"
#include "mult_r.h"
#include "div_s.h"
#include "oscl_mem.h"


#include "vad2.h"

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
; LOCAL VARIABLE DEFINITIONS
; Variable declaration - defined here and used outside this module
----------------------------------------------------------------------------*/

/*
 * The channel table is defined below.  In this table, the
 * lower and higher frequency coefficients for each of the 16
 * channels are specified.  The table excludes the coefficients
 * with numbers 0 (DC), 1, and 64 (Foldover frequency).
 */

const Word16 ch_tbl[NUM_CHAN][2] =
{

    {2, 3},
    {4, 5},
    {6, 7},
    {8, 9},
    {10, 11},
    {12, 13},
    {14, 16},
    {17, 19},
    {20, 22},
    {23, 26},
    {27, 30},
    {31, 35},
    {36, 41},
    {42, 48},
    {49, 55},
    {56, 63}

};

/* channel energy scaling table - allows efficient division by number
     * of DFT bins in the channel: 1/2, 1/3, 1/4, etc.
 */

const Word16 ch_tbl_sh[NUM_CHAN] =
{
    16384, 16384, 16384, 16384, 16384, 16384, 10923, 10923,
    10923, 8192, 8192, 6554, 5461, 4681, 4681, 4096
};

/*
 * The voice metric table is defined below.  It is a non-
 * linear table with a deadband near zero.  It maps the SNR
 * index (quantized SNR value) to a number that is a measure
 * of voice quality.
 */

const Word16 vm_tbl[90] =
{
    2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,
    3, 3, 3, 3, 3, 4, 4, 4, 5, 5, 5, 6, 6, 7, 7, 7,
    8, 8, 9, 9, 10, 10, 11, 12, 12, 13, 13, 14, 15,
    15, 16, 17, 17, 18, 19, 20, 20, 21, 22, 23, 24,
    24, 25, 26, 27, 28, 28, 29, 30, 31, 32, 33, 34,
    35, 36, 37, 37, 38, 39, 40, 41, 42, 43, 44, 45,
    46, 47, 48, 49, 50, 50, 50, 50, 50, 50, 50, 50,
    50, 50
};

/* hangover as a function of peak SNR (3 dB steps) */
const Word16 hangover_table[20] =
{
    30, 30, 30, 30, 30, 30, 28, 26, 24, 22, 20, 18, 16, 14, 12, 10, 8, 8, 8, 8
};

/* burst sensitivity as a function of peak SNR (3 dB steps) */
const Word16 burstcount_table[20] =
{
    8, 8, 8, 8, 8, 8, 8, 8, 7, 6, 5, 4, 4, 4, 4, 4, 4, 4, 4, 4
};

/* voice metric sensitivity as a function of peak SNR (3 dB steps) */
const Word16 vm_threshold_table[20] =
{
    34, 34, 34, 34, 34, 34, 34, 34, 34, 34,
    34, 40, 51, 71, 100, 139, 191, 257, 337, 432
};

/*
------------------------------------------------------------------------------
 FUNCTION NAME: fn10Log10
------------------------------------------------------------------------------
 INPUT AND OUTPUT DEFINITIONS

 Inputs:
   L_Input -- Word32 -- (scaled as 31-fbits,fbits)
   fbits   -- Word16 -- number of fractional bits on input

 Outputs:
   pOverflow -- pointer to type Flag -- overflow indicator

 Returns:
   output -- Word16 -- (scaled as 7,8)

 Global Variables Used:
    None

 Local Variables Needed:
    None

------------------------------------------------------------------------------
 FUNCTION DESCRIPTION

 PURPOSE:
   The purpose of this function is to take the 10*log base 10 of input and
   divide by 128 and return; i.e. output = 10*log10(input)/128 (scaled as 7,8)

 DESCRIPTION:

   10*log10(x)/128 = 10*(log10(2) * (log2(x<<fbits)-log2(1<<fbits)) >> 7
                   = 3.0103 * (log2(x<<fbits) - fbits) >> 7
                   = ((3.0103/4.0 * (log2(x<<fbits) - fbits) << 2) >> 7
                   = (3.0103/4.0 * (log2(x<<fbits) - fbits) >> 5

------------------------------------------------------------------------------
 REQUIREMENTS

 None

------------------------------------------------------------------------------
 REFERENCES

 vad2.c, UMTS GSM AMR speech codec, R99 - Version 3.2.0, March 2, 2001

------------------------------------------------------------------------------
 PSEUDO-CODE


------------------------------------------------------------------------------
 CAUTION [optional]
 [State any special notes, constraints or cautions for users of this function]

------------------------------------------------------------------------------
*/

Word16 fn10Log10(Word32 L_Input, Word16 fbits, Flag *pOverflow)
{

    Word16 integer;     /* Integer part of Log2.   (range: 0<=val<=30) */
    Word16 fraction;    /* Fractional part of Log2. (range: 0<=val<1) */

    Word32 Ltmp;
    Word16 tmp;

    Log2(L_Input, &integer, &fraction, pOverflow);

    integer = sub(integer, fbits, pOverflow);

    /* 24660 = 10*log10(2)/4 scaled 0,15 */
    Ltmp = Mpy_32_16(integer, fraction, 24660, pOverflow);

    /* extra shift for 30,1 => 15,0 extract correction */
    Ltmp = L_shr_r(Ltmp, 5 + 1, pOverflow);

    tmp = (Word16) Ltmp;

    return (tmp);
}

/*
------------------------------------------------------------------------------
 FUNCTION NAME: block_norm
------------------------------------------------------------------------------
 INPUT AND OUTPUT DEFINITIONS

 Inputs:
    in -- array of type Word16 -- pointer to data sequence to be normalised
    length -- Word16 -- number of elements in data sequence
    headroom -- Word16 -- number of headroom bits

 Outputs:
    out -- array of type Word16 -- normalised output data sequence
    pOverflow -- pointer to type Flag -- overflow indicator

 Returns:
    Word16 -- number of bits sequence was left shifted

 Global Variables Used:
    None

 Local Variables Needed:
    None

------------------------------------------------------------------------------
 FUNCTION DESCRIPTION

 The purpose of this function is block normalise the input data sequence

 1) Search for maximum absolute valued data element
 2) Normalise the max element with "headroom"
 3) Transfer/shift the input sequence to the output buffer
 4) Return the number of left shifts

------------------------------------------------------------------------------
 REQUIREMENTS

 None

------------------------------------------------------------------------------
 REFERENCES

 vad2.c, UMTS GSM AMR speech codec, R99 - Version 3.2.0, March 2, 2001

------------------------------------------------------------------------------
 PSEUDO-CODE


------------------------------------------------------------------------------
 CAUTION

     An input sequence of all zeros will return the maximum
     number of left shifts allowed, NOT the value returned
     by a norm_s(0) call, since it desired to associate an
     all zeros sequence with low energy.
------------------------------------------------------------------------------
*/

Word16 block_norm(
    Word16 * in,
    Word16 * out,
    Word16 length,
    Word16 headroom,
    Flag *pOverflow)
{

    Word16 i;
    Word16 max;
    Word16 scnt;
    Word16 adata;

    max = abs_s(in[0]);

    for (i = 1; i < length; i++)
    {
        adata = abs_s(in[i]);

        if (adata > max)
        {
            max = adata;
        }
    }
    if (max != 0)
    {
        scnt = sub(norm_s(max), headroom, pOverflow);
        for (i = 0; i < length; i++)
        {
            out[i] = shl(in[i], scnt, pOverflow);
        }
    }
    else
    {
        scnt = sub(16, headroom, pOverflow);
        for (i = 0; i < length; i++)
        {
            out[i] = 0;
        }
    }
    return (scnt);
}




/*
------------------------------------------------------------------------------
 FUNCTION NAME: vad2
------------------------------------------------------------------------------
 INPUT AND OUTPUT DEFINITIONS

 Inputs:
    farray_ptr -- array of type Word16, length 80 (input array)
    vadState2 -- pointer to vadState2 state structure

 Outputs:
    vadState2 -- pointer to vadState2 state structure --
                        state variables are updated
   pOverflow -- pointer to type Flag -- overflow indicator

 Returns:
    Word16
                 VAD(m) - two successive calls to vad2() yield
                 the VAD decision for the 20 ms frame:
                 VAD_flag = VAD(m-1) || VAD(m)

 Global Variables Used:


 Local Variables Needed:
    None

------------------------------------------------------------------------------
 FUNCTION DESCRIPTION

 This function provides the Voice Activity Detection function option 2
 for the Adaptive Multi-rate (AMR) codec.

------------------------------------------------------------------------------
 REQUIREMENTS

 None

------------------------------------------------------------------------------
 REFERENCES

 vad2.c, UMTS GSM AMR speech codec, R99 - Version 3.2.0, March 2, 2001

------------------------------------------------------------------------------
 PSEUDO-CODE


------------------------------------------------------------------------------
 CAUTION [optional]
 [State any special notes, constraints or cautions for users of this function]

------------------------------------------------------------------------------
*/

Word16 vad2(Word16 * farray_ptr, vadState2 * st, Flag *pOverflow)
{

    /* State tables that use 22,9 or 27,4 scaling for ch_enrg[] */

    const Word16 noise_floor_chan[2] = {NOISE_FLOOR_CHAN_0, NOISE_FLOOR_CHAN_1};
    const Word16 min_chan_enrg[2] =    {MIN_CHAN_ENRG_0, MIN_CHAN_ENRG_1};
    const Word16 ine_noise[2] =        {INE_NOISE_0, INE_NOISE_1};
    const Word16 fbits[2] =        {FRACTIONAL_BITS_0, FRACTIONAL_BITS_1};
    const Word16 state_change_shift_r[2] = {STATE_1_TO_0_SHIFT_R, STATE_0_TO_1_SHIFT_R};

    /* Energy scale table given 30,1 input scaling (also account for -6 dB shift on input) */
    const Word16 enrg_norm_shift[2] =  {(FRACTIONAL_BITS_0 - 1 + 2), (FRACTIONAL_BITS_1 - 1 + 2)};


    /* Automatic variables */

    Word32 Lenrg;               /* scaled as 30,1 */
    Word32 Ltne;                /* scaled as 22,9 */
    Word32 Ltce;                /* scaled as 22,9 or 27,4 */

    Word16 tne_db;              /* scaled as 7,8 */
    Word16 tce_db;              /* scaled as 7,8 */

    Word16 input_buffer[FRM_LEN];       /* used for block normalising input data */
    Word16 data_buffer[FFT_LEN];        /* used for in-place FFT */

    Word16 ch_snr[NUM_CHAN];        /* scaled as 7,8 */
    Word16 ch_snrq;             /* scaled as 15,0 (in 0.375 dB steps) */
    Word16 vm_sum;              /* scaled as 15,0 */
    Word16 ch_enrg_dev;         /* scaled as 7,8 */

    Word32 Lpeak;               /* maximum channel energy */
    Word16 p2a_flag;            /* flag to indicate spectral peak-to-average ratio > 10 dB */

    Word16 ch_enrg_db[NUM_CHAN];        /* scaled as 7,8 */
    Word16 ch_noise_db;         /* scaled as 7,8 */

    Word16 alpha;               /* scaled as 0,15 */
    Word16 one_m_alpha;         /* scaled as 0,15 */
    Word16 update_flag;         /* set to indicate a background noise estimate update */

    Word16 i;
    Word16 j;
    Word16 j1;
    Word16 j2;            /* Scratch variables */

    Word16 hi1;
    Word16 lo1;

    Word32 Ltmp;
    Word32 Ltmp1;
    Word32 Ltmp2;
    Word16 tmp;

    Word16 normb_shift;     /* block norm shift count */

    Word16 ivad;            /* intermediate VAD decision (return value) */
    Word16 tsnrq;           /* total signal-to-noise ratio (quantized 3 dB steps) scaled as 15,0 */
    Word16 xt;          /* instantaneous frame SNR in dB, scaled as 7,8 */

    Word16 state_change;


    /* Increment frame counter */
    st->Lframe_cnt = L_add(st->Lframe_cnt, 1, pOverflow);

    /* Block normalize the input */
    normb_shift = block_norm(farray_ptr, input_buffer, FRM_LEN, FFT_HEADROOM, pOverflow);

    /* Pre-emphasize the input data and store in the data buffer with the appropriate offset */
    for (i = 0; i < DELAY; i++)
    {
        data_buffer[i] = 0;
    }

    st->pre_emp_mem = shr_r(st->pre_emp_mem, sub(st->last_normb_shift, normb_shift, pOverflow), pOverflow);
    st->last_normb_shift = normb_shift;

    data_buffer[DELAY] = add(input_buffer[0], mult(PRE_EMP_FAC, st->pre_emp_mem, pOverflow), pOverflow);

    for (i = DELAY + 1, j = 1; i < DELAY + FRM_LEN; i++, j++)
    {
        data_buffer[i] = add(input_buffer[j], mult(PRE_EMP_FAC, input_buffer[j-1], pOverflow), pOverflow);
    }
    st->pre_emp_mem = input_buffer[FRM_LEN-1];

    for (i = DELAY + FRM_LEN; i < FFT_LEN; i++)
    {
        data_buffer[i] = 0;
    }


    /* Perform FFT on the data buffer */
    r_fft(data_buffer, pOverflow);


    /* Use normb_shift factor to determine the scaling of the energy estimates */
    state_change = 0;
    if (st->shift_state == 0)
    {
        if (normb_shift <= (-FFT_HEADROOM + 2))
        {
            state_change = 1;
            st->shift_state = 1;
        }
    }
    else
    {
        if (normb_shift >= (-FFT_HEADROOM + 5))
        {
            state_change = 1;
            st->shift_state = 0;
        }
    }

    /* Scale channel energy estimate */
    if (state_change)
    {
        for (i = LO_CHAN; i <= HI_CHAN; i++)
        {
            st->Lch_enrg[i] =
                L_shr(
                    st->Lch_enrg[i],
                    state_change_shift_r[st->shift_state],
                    pOverflow);
        }
    }


    /* Estimate the energy in each channel */
    if (st->Lframe_cnt == 1)
    {
        alpha = 32767;
        one_m_alpha = 0;
    }
    else
    {
        alpha = CEE_SM_FAC;
        one_m_alpha = ONE_MINUS_CEE_SM_FAC;
    }

    for (i = LO_CHAN; i <= HI_CHAN; i++)
    {
        Lenrg = 0;
        j1 = ch_tbl[i][0];
        j2 = ch_tbl[i][1];

        for (j = j1; j <= j2; j++)
        {
            Lenrg = L_mac(
                        Lenrg,
                        data_buffer[2 * j],
                        data_buffer[2 * j],
                        pOverflow);

            Lenrg = L_mac(
                        Lenrg,
                        data_buffer[2 * j + 1],
                        data_buffer[2 * j + 1],
                        pOverflow);
        }

        /* Denorm energy & scale 30,1 according to the state */
        tmp = shl(normb_shift, 1, pOverflow);
        tmp = sub(tmp, enrg_norm_shift[st->shift_state], pOverflow);
        Lenrg = L_shr_r(Lenrg, tmp, pOverflow);

        /* integrate over time:
         * e[i] = (1-alpha)*e[i] + alpha*enrg/num_bins_in_chan
         */
        tmp = mult(alpha, ch_tbl_sh[i], pOverflow);
        L_Extract(Lenrg, &hi1, &lo1, pOverflow);
        Ltmp = Mpy_32_16(hi1, lo1, tmp, pOverflow);

        L_Extract(st->Lch_enrg[i], &hi1, &lo1, pOverflow);

        Ltmp1 = Mpy_32_16(hi1, lo1, one_m_alpha, pOverflow);
        st->Lch_enrg[i] = L_add(Ltmp, Ltmp1, pOverflow);

        if (st->Lch_enrg[i] < min_chan_enrg[st->shift_state])
        {
            st->Lch_enrg[i] = min_chan_enrg[st->shift_state];
        }

    }


    /* Compute the total channel energy estimate (Ltce) */
    Ltce = 0;
    for (i = LO_CHAN; i <= HI_CHAN; i++)
    {
        Ltce =
            L_add(
                Ltce,
                st->Lch_enrg[i],
                pOverflow);
    }


    /* Calculate spectral peak-to-average ratio, set flag if p2a > 10 dB */
    Lpeak = 0;

    /* Sine waves not valid for low frequencies */
    for (i = LO_CHAN + 2; i <= HI_CHAN; i++)
    {
        if (L_sub(st->Lch_enrg [i], Lpeak, pOverflow) > 0)
        {
            Lpeak = st->Lch_enrg [i];
        }
    }

    /* Set p2a_flag if peak (dB) > average channel energy (dB) + 10 dB */
    /*   Lpeak > Ltce/num_channels * 10^(10/10)                        */
    /*   Lpeak > (10/16)*Ltce                                          */

    L_Extract(Ltce, &hi1, &lo1, pOverflow);
    Ltmp = Mpy_32_16(hi1, lo1, 20480, pOverflow);
    if (L_sub(Lpeak, Ltmp, pOverflow) > 0)
    {
        p2a_flag = TRUE;
    }
    else
    {
        p2a_flag = FALSE;
    }


    /* Initialize channel noise estimate to either the channel energy or fixed level  */
    /*   Scale the energy appropriately to yield state 0 (22,9) scaling for noise */
    if (st->Lframe_cnt <= 4)
    {
        if (p2a_flag == TRUE)
        {
            for (i = LO_CHAN; i <= HI_CHAN; i++)
            {
                st->Lch_noise[i] = INE_NOISE_0;
            }
        }
        else
        {
            for (i = LO_CHAN; i <= HI_CHAN; i++)
            {
                if (st->Lch_enrg[i] < ine_noise[st->shift_state])
                {
                    st->Lch_noise[i] = INE_NOISE_0;
                }
                else
                {
                    if (st->shift_state == 1)
                    {
                        st->Lch_noise[i] =
                            L_shr(
                                st->Lch_enrg[i],
                                state_change_shift_r[0],
                                pOverflow);
                    }
                    else
                    {
                        st->Lch_noise[i] = st->Lch_enrg[i];
                    }
                }
            }
        }
    }


    /* Compute the channel energy (in dB), the channel SNRs, and the sum of voice metrics */
    vm_sum = 0;
    for (i = LO_CHAN; i <= HI_CHAN; i++)
    {
        ch_enrg_db[i] =
            fn10Log10(
                st->Lch_enrg[i],
                fbits[st->shift_state],
                pOverflow);

        ch_noise_db =
            fn10Log10(
                st->Lch_noise[i],
                FRACTIONAL_BITS_0,
                pOverflow);

        ch_snr[i] = sub(ch_enrg_db[i], ch_noise_db, pOverflow);

        /* quantize channel SNR in 3/8 dB steps (scaled 7,8 => 15,0) */
        /*   ch_snr = pv_round((snr/(3/8))>>8)                          */
        /*          = pv_round(((0.6667*snr)<<2)>>8)                    */
        /*          = pv_round((0.6667*snr)>>6)                         */

        tmp = mult(21845, ch_snr[i], pOverflow);

        ch_snrq = shr_r(tmp, 6, pOverflow);

        /* Accumulate the sum of voice metrics  */
        if (ch_snrq < 89)
        {
            if (ch_snrq > 0)
            {
                j = ch_snrq;
            }
            else
            {
                j = 0;
            }
        }
        else
        {
            j = 89;
        }
        vm_sum = add(vm_sum, vm_tbl[j], pOverflow);
    }


    /* Initialize NOMINAL peak voice energy and average noise energy, calculate instantaneous SNR */
    if (st->Lframe_cnt <= 4 || st->fupdate_flag == TRUE)
    {
        /* tce_db = (96 - 22 - 10*log10(64) (due to FFT)) scaled as 7,8 */
        tce_db = 14320;
        st->negSNRvar = 0;
        st->negSNRbias = 0;

        /* Compute the total noise estimate (Ltne) */
        Ltne = 0;
        for (i = LO_CHAN; i <= HI_CHAN; i++)
        {
            Ltne = L_add(Ltne, st->Lch_noise[i], pOverflow);
        }

        /* Get total noise in dB */
        tne_db =
            fn10Log10(
                Ltne,
                FRACTIONAL_BITS_0,
                pOverflow);

        /* Initialise instantaneous and long-term peak signal-to-noise ratios */
        xt = sub(tce_db, tne_db, pOverflow);
        st->tsnr = xt;
    }
    else
    {
        /* Calculate instantaneous frame signal-to-noise ratio */
        /* xt = 10*log10( sum(2.^(ch_snr*0.1*log2(10)))/length(ch_snr) ) */
        Ltmp1 = 0;
        for (i = LO_CHAN; i <= HI_CHAN; i++)
        {
            /* Ltmp2 = ch_snr[i] * 0.1 * log2(10); (ch_snr scaled as 7,8) */
            Ltmp2 = L_mult(ch_snr[i], 10885, pOverflow);
            Ltmp2 = L_shr(Ltmp2, 8, pOverflow);

            L_Extract(Ltmp2, &hi1, &lo1, pOverflow);
            hi1 = add(hi1, 3, pOverflow);  /* 2^3 to compensate for negative SNR */

            Ltmp2 = Pow2(hi1, lo1, pOverflow);

            Ltmp1 = L_add(Ltmp1, Ltmp2, pOverflow);
        }
        xt =
            fn10Log10(
                Ltmp1,
                4 + 3,
                pOverflow);     /* average by 16, inverse compensation 2^3 */

        /* Estimate long-term "peak" SNR */
        if (xt > st->tsnr)
        {
            Ltmp1 = L_mult(29491, st->tsnr, pOverflow);
            Ltmp2 = L_mult(3277, xt, pOverflow);
            Ltmp1 = L_add(Ltmp1, Ltmp2, pOverflow);

            /* tsnr = 0.9*tsnr + 0.1*xt; */
            st->tsnr = pv_round(Ltmp1, pOverflow);
        }
        /* else if (xt > 0.625*tsnr) */
        else
        {
            tmp = mult(20480, st->tsnr, pOverflow);
            tmp = sub(xt, tmp, pOverflow);

            if (tmp > 0)
            {
                /* tsnr = 0.998*tsnr + 0.002*xt; */
                Ltmp1 = L_mult(32702, st->tsnr, pOverflow);
                Ltmp2 = L_mult(66, xt, pOverflow);
                Ltmp1 = L_add(Ltmp1, Ltmp2, pOverflow);

                st->tsnr = pv_round(Ltmp1, pOverflow);
            }
        }
    }

    /* Quantize the long-term SNR in 3 dB steps, limit to 0 <= tsnrq <= 19 */
    tmp = mult(st->tsnr, 10923, pOverflow);
    tsnrq = shr(tmp, 8, pOverflow);

    /* tsnrq = min(19, max(0, tsnrq)); */
    if (tsnrq > 19)
    {
        tsnrq = 19;
    }
    else if (tsnrq < 0)
    {
        tsnrq = 0;
    }

    /* Calculate the negative SNR sensitivity bias */
    if (xt < 0)
    {
        /* negSNRvar = 0.99*negSNRvar + 0.01*xt*xt; */
        /*   xt scaled as 7,8 => xt*xt scaled as 14,17, shift to 7,8 and round */
        Ltmp1 = L_mult(xt, xt, pOverflow);
        Ltmp1 = L_shl(Ltmp1, 7, pOverflow);
        tmp = pv_round(Ltmp1, pOverflow);

        Ltmp1 = L_mult(32440, st->negSNRvar, pOverflow);
        Ltmp2 = L_mult(328, tmp, pOverflow);
        Ltmp1 = L_add(Ltmp1, Ltmp2, pOverflow);

        st->negSNRvar = pv_round(Ltmp1, pOverflow);

        /* if (negSNRvar > 4.0) negSNRvar = 4.0;  */
        if (st->negSNRvar > 1024)
        {
            st->negSNRvar = 1024;
        }

        /* negSNRbias = max(12.0*(negSNRvar - 0.65), 0.0); */
        tmp = sub(st->negSNRvar, 166, pOverflow);
        tmp = shl(tmp, 4, pOverflow);
        tmp = mult_r(tmp, 24576, pOverflow);

        if (tmp < 0)
        {
            st->negSNRbias = 0;
        }
        else
        {
            st->negSNRbias = shr(tmp, 8, pOverflow);
        }
    }


    /* Determine VAD as a function of the voice metric sum and quantized SNR */

    tmp = add(vm_threshold_table[tsnrq], st->negSNRbias, pOverflow);

    if (vm_sum > tmp)
    {
        ivad = 1;
        st->burstcount = add(st->burstcount, 1, pOverflow);
        if (st->burstcount > burstcount_table[tsnrq])
        {
            st->hangover = hangover_table[tsnrq];
        }
    }
    else
    {
        st->burstcount = 0;
        st->hangover = sub(st->hangover, 1, pOverflow);
        if (st->hangover <= 0)
        {
            ivad = 0;
            st->hangover = 0;
        }
        else
        {
            ivad = 1;
        }
    }


    /* Calculate log spectral deviation */
    ch_enrg_dev = 0;
    if (st->Lframe_cnt == 1)
    {
        for (i = LO_CHAN; i <= HI_CHAN; i++)
        {
            st->ch_enrg_long_db[i] = ch_enrg_db[i];
        }
    }
    else
    {
        for (i = LO_CHAN; i <= HI_CHAN; i++)
        {
            tmp = sub(st->ch_enrg_long_db[i], ch_enrg_db[i], pOverflow);
            tmp = abs_s(tmp);

            ch_enrg_dev = add(ch_enrg_dev, tmp, pOverflow);
        }
    }

    /*
     * Calculate long term integration constant as
     * a function of instantaneous SNR
     * (i.e., high SNR (tsnr dB) -> slower integration (alpha = HIGH_ALPHA),
     *         low SNR (0 dB) -> faster integration (alpha = LOW_ALPHA)
     */

    /* alpha = HIGH_ALPHA - ALPHA_RANGE * (tsnr - xt)
     * ----------------------------------------------
     *         tsnr, low <= alpha <= high
     */
    tmp = sub(st->tsnr, xt, pOverflow);
    if (tmp <= 0 || st->tsnr <= 0)
    {
        alpha = HIGH_ALPHA;
        one_m_alpha = 32768L - HIGH_ALPHA;
    }
    else if (tmp > st->tsnr)
    {
        alpha = LOW_ALPHA;
        one_m_alpha = 32768L - LOW_ALPHA;
    }
    else
    {
        tmp = div_s(tmp, st->tsnr);
        tmp = mult(ALPHA_RANGE, tmp, pOverflow);
        alpha = sub(HIGH_ALPHA, tmp, pOverflow);
        one_m_alpha = sub(32767, alpha, pOverflow);
    }

    /* Calc long term log spectral energy */
    for (i = LO_CHAN; i <= HI_CHAN; i++)
    {
        Ltmp1 = L_mult(one_m_alpha, ch_enrg_db[i], pOverflow);
        Ltmp2 = L_mult(alpha, st->ch_enrg_long_db[i], pOverflow);

        Ltmp1 = L_add(Ltmp1, Ltmp2, pOverflow);
        st->ch_enrg_long_db[i] = pv_round(Ltmp1, pOverflow);
    }


    /* Set or clear the noise update flags */
    update_flag = FALSE;
    st->fupdate_flag = FALSE;
    if (vm_sum <= UPDATE_THLD)
    {
        if (st->burstcount == 0)
        {
            update_flag = TRUE;
            st->update_cnt = 0;
        }
    }
    else if (L_sub(Ltce, noise_floor_chan[st->shift_state], pOverflow) > 0)
    {
        if (ch_enrg_dev < DEV_THLD)
        {
            if (p2a_flag == FALSE)
            {
                if (st->LTP_flag == FALSE)
                {
                    st->update_cnt = add(st->update_cnt, 1, pOverflow);
                    if (st->update_cnt >= UPDATE_CNT_THLD)
                    {
                        update_flag = TRUE;
                        st->fupdate_flag = TRUE;
                    }
                }
            }
        }
    }
    if (st->update_cnt == st->last_update_cnt)
    {
        st->hyster_cnt = add(st->hyster_cnt, 1, pOverflow);
    }
    else
    {
        st->hyster_cnt = 0;
    }

    st->last_update_cnt = st->update_cnt;

    if (st->hyster_cnt > HYSTER_CNT_THLD)
    {
        st->update_cnt = 0;
    }


    /* Conditionally update the channel noise estimates */
    if (update_flag == TRUE)
    {
        /* Check shift state */
        if (st->shift_state == 1)
        {
            /* get factor to shift ch_enrg[]
             * from state 1 to 0 (noise always state 0)
             */
            tmp = state_change_shift_r[0];
        }
        else
        {
            /* No shift if already state 0 */
            tmp = 0;
        }

        /* Update noise energy estimate */
        for (i = LO_CHAN; i <= HI_CHAN; i++)
        {
            /* integrate over time: en[i] = (1-alpha)*en[i] + alpha*e[n] */
            /* (extract with shift compensation for state 1) */

            Ltmp1 = L_shr(st->Lch_enrg[i], tmp, pOverflow);
            L_Extract(Ltmp1, &hi1, &lo1, pOverflow);

            Ltmp = Mpy_32_16(hi1, lo1, CNE_SM_FAC, pOverflow);

            L_Extract(st->Lch_noise[i], &hi1, &lo1, pOverflow);

            Ltmp1 = Mpy_32_16(hi1, lo1, ONE_MINUS_CNE_SM_FAC, pOverflow);
            st->Lch_noise[i] = L_add(Ltmp, Ltmp1, pOverflow);

            /* Limit low level noise */
            if (st->Lch_noise[i] <= MIN_NOISE_ENRG_0)
            {
                st->Lch_noise[i] = MIN_NOISE_ENRG_0;
            }
        }
    }

    return(ivad);
}                               /* end of vad2 () */


/*
------------------------------------------------------------------------------
 FUNCTION NAME: vad2_init
------------------------------------------------------------------------------
 INPUT AND OUTPUT DEFINITIONS

 Inputs:
    state -- double pointer to type vadState2 -- pointer to memory to
                                                 be initialized.

 Outputs:
    state -- points to initalized area in memory.

 Returns:
    None

 Global Variables Used:
    None

 Local Variables Needed:
    None

------------------------------------------------------------------------------
 FUNCTION DESCRIPTION

 Allocates state memory and initializes state memory

------------------------------------------------------------------------------
 REQUIREMENTS

 None

------------------------------------------------------------------------------
 REFERENCES

 vad2.c, UMTS GSM AMR speech codec, R99 - Version 3.2.0, March 2, 2001

------------------------------------------------------------------------------
 PSEUDO-CODE


------------------------------------------------------------------------------
 CAUTION [optional]
 [State any special notes, constraints or cautions for users of this function]

------------------------------------------------------------------------------
*/
Word16 vad2_init(vadState2 **state)
{
    vadState2* s;

    if (state == (vadState2 **) NULL)
    {
        return -1;
    }
    *state = NULL;

    /* allocate memory */
    if ((s = (vadState2 *) oscl_malloc(sizeof(vadState2))) == NULL)
    {
        return -1;
    }

    vad2_reset(s);

    *state = s;

    return 0;
}


/*
------------------------------------------------------------------------------
 FUNCTION NAME: vad2_reset
------------------------------------------------------------------------------
 INPUT AND OUTPUT DEFINITIONS

 Inputs:
    state -- pointer to type vadState1 --  State struct

 Outputs:
    state -- pointer to type vadState1 --  State struct

 Returns:
    None

 Global Variables Used:
    None

 Local Variables Needed:
    None

------------------------------------------------------------------------------
 FUNCTION DESCRIPTION

 Purpose:    Resets state memory to zero

------------------------------------------------------------------------------
 REQUIREMENTS

 None

------------------------------------------------------------------------------
 REFERENCES

 vad2.c, UMTS GSM AMR speech codec, R99 - Version 3.2.0, March 2, 2001

------------------------------------------------------------------------------
 PSEUDO-CODE


------------------------------------------------------------------------------
 CAUTION [optional]
 [State any special notes, constraints or cautions for users of this function]

------------------------------------------------------------------------------
*/

Word16 vad2_reset(vadState2 * st)
{
    Word16  i;
    Word16  *ptr;

    if (st == (vadState2 *) NULL)
    {
        return -1;
    }
    ptr = (Word16 *)st;

    for (i = 0; i < sizeof(vadState2) / 2; i++)
    {
        *ptr++ = 0;
    }

    return 0;
}                       /* end of vad2_reset () */


/*
------------------------------------------------------------------------------
 FUNCTION NAME: vad2_exit
------------------------------------------------------------------------------
 INPUT AND OUTPUT DEFINITIONS

 Inputs:
    state -- pointer to type vadState1 --  State struct

 Outputs:
    None

 Returns:
    None

 Global Variables Used:
    None

 Local Variables Needed:
    None

------------------------------------------------------------------------------
 FUNCTION DESCRIPTION

    The memory used for state memory is freed

------------------------------------------------------------------------------
 REQUIREMENTS

 None

------------------------------------------------------------------------------
 REFERENCES

 vad2.c, UMTS GSM AMR speech codec, R99 - Version 3.2.0, March 2, 2001

------------------------------------------------------------------------------
 PSEUDO-CODE


------------------------------------------------------------------------------
 CAUTION [optional]
 [State any special notes, constraints or cautions for users of this function]

------------------------------------------------------------------------------
*/

void vad2_exit(vadState2 **state)
{
    if (state == NULL || *state == NULL)
    {
        return;
    }

    /* deallocate memory */
    oscl_free(*state);
    *state = NULL;

    return;
}

