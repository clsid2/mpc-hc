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
 Filename: vad1.cpp
 Functions:

------------------------------------------------------------------------------
 MODULE DESCRIPTION


------------------------------------------------------------------------------
*/

/*----------------------------------------------------------------------------
; INCLUDES
----------------------------------------------------------------------------*/
#include "vad.h"
#include "typedef.h"
#include "shr.h"
#include "basic_op.h"
#include "cnst_vad.h"
#include "oscl_mem.h"

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
------------------------------------------------------------------------------
 FUNCTION NAME: first_filter_stage
------------------------------------------------------------------------------
 INPUT AND OUTPUT DEFINITIONS

 Inputs:
    data -- array of type Word16 -- filter memory
    in   -- array of type Word16 -- input signal

 Outputs:
    data -- array of type Word16 -- filter memory
    out  -- array of type Word16 -- output values, every other
                                    output is low-pass part and
                                    other is high-pass part every

    pOverflow -- pointer to type Flag -- overflow indicator

 Returns:
    None

 Global Variables Used:
    None

 Local Variables Needed:
    None

------------------------------------------------------------------------------
 FUNCTION DESCRIPTION

 Purpose      : Scale input down by one bit. Calculate 5th order
                half-band lowpass/highpass filter pair with
                decimation.
------------------------------------------------------------------------------
 REQUIREMENTS

 None

------------------------------------------------------------------------------
 REFERENCES

 vad1.c, UMTS GSM AMR speech codec, R99 - Version 3.2.0, March 2, 2001

------------------------------------------------------------------------------
 PSEUDO-CODE


------------------------------------------------------------------------------
 CAUTION [optional]
 [State any special notes, constraints or cautions for users of this function]

------------------------------------------------------------------------------
*/

static void first_filter_stage(
    Word16 in[],      /* i   : input signal                  */
    Word16 out[],     /* o   : output values, every other    */
    /*       output is low-pass part and   */
    /*       other is high-pass part every */
    Word16 data[],    /* i/o : filter memory                 */
    Flag  *pOverflow  /* o : Flag set when overflow occurs   */
)
{
    Word16 temp0;
    Word16 temp1;
    Word16 temp2;
    Word16 temp3;
    Word16 i;
    Word16 data0;
    Word16 data1;

    data0 = data[0];
    data1 = data[1];

    for (i = 0; i < FRAME_LEN / 4; i++)
    {
//        temp0 = mult(COEFF5_1, data0, pOverflow);
        temp0 = (Word16)(((Word32)COEFF5_1 * data0) >> 15);
        temp1 = in[4*i+0] >> 2;
        temp0 = sub(temp1, temp0, pOverflow);

//        temp1 = mult(COEFF5_1, temp0, pOverflow);
        temp1 = (Word16)(((Word32)COEFF5_1 * temp0) >> 15);
        temp1 = add_16(data0, temp1, pOverflow);

//        temp3 = mult(COEFF5_2, data1, pOverflow);
        temp3 = (Word16)(((Word32)COEFF5_2 * data1) >> 15);

        temp2 = in[4*i+1] >> 2;

        temp3 = sub(temp2, temp3, pOverflow);

//        temp2 = mult(COEFF5_2, temp3, pOverflow);
        temp2 = (Word16)(((Word32)COEFF5_2 * temp3) >> 15);

        temp2 = add_16(data1, temp2, pOverflow);

        out[4*i+0] = add_16(temp1, temp2, pOverflow);
        out[4*i+1] = sub(temp1, temp2, pOverflow);

//        temp1 = mult(COEFF5_1, temp0, pOverflow);
        temp1 = (Word16)(((Word32)COEFF5_1 * temp0) >> 15);

        temp2 = in[4*i+2] >> 2;
        data0 = sub(temp2, temp1, pOverflow);

//        temp1 = mult(COEFF5_1, data0, pOverflow);
        temp1 = (Word16)(((Word32)COEFF5_1 * data0) >> 15);

        temp1 = add_16(temp0, temp1, pOverflow);

//        data1 = mult(COEFF5_2, temp3, pOverflow);
        data1 = (Word16)(((Word32)COEFF5_2 * temp3) >> 15);
        temp2 = in[4*i+3] >> 2;
        data1 = sub(temp2, data1, pOverflow);

//        temp2 = mult(COEFF5_2, data1, pOverflow);
        temp2 = (Word16)(((Word32)COEFF5_2 * data1) >> 15);
        temp2 = add_16(temp3, temp2, pOverflow);

        out[4*i+2] = add_16(temp1, temp2, pOverflow);
        out[4*i+3] = sub(temp1, temp2, pOverflow);
    }

    data[0] = data0;
    data[1] = data1;
}


/*
------------------------------------------------------------------------------
 FUNCTION NAME: filter5
------------------------------------------------------------------------------
 INPUT AND OUTPUT DEFINITIONS

 Inputs:
    in0 -- array of type Word16 -- input values; output low-pass part
    in1 -- array of type Word16 -- input values; output high-pass part
    data -- array of type Word16 -- updated filter memory

 Outputs:
    in0 -- array of type Word16 -- input values; output low-pass part
    in1 -- array of type Word16 -- input values; output high-pass part
    data -- array of type Word16 -- updated filter memory
    pOverflow -- pointer to type Flag -- overflow indicator

 Returns:
    None

 Global Variables Used:
    None

 Local Variables Needed:
    None

------------------------------------------------------------------------------
 FUNCTION DESCRIPTION

 Purpose      : Fifth-order half-band lowpass/highpass filter pair with
                decimation.
------------------------------------------------------------------------------
 REQUIREMENTS

 None

------------------------------------------------------------------------------
 REFERENCES

 vad1.c, UMTS GSM AMR speech codec, R99 - Version 3.2.0, March 2, 2001

------------------------------------------------------------------------------
 PSEUDO-CODE


------------------------------------------------------------------------------
 CAUTION [optional]
 [State any special notes, constraints or cautions for users of this function]

------------------------------------------------------------------------------
*/

static void filter5(Word16 *in0,    /* i/o : input values; output low-pass part  */
                    Word16 *in1,    /* i/o : input values; output high-pass part */
                    Word16 data[],  /* i/o : updated filter memory               */
                    Flag  *pOverflow  /* o : Flag set when overflow occurs       */
                   )
{
    Word16 temp0;
    Word16 temp1;
    Word16 temp2;

    temp0 = mult(COEFF5_1, data[0], pOverflow);
    temp0 = sub(*in0, temp0, pOverflow);

    temp1 = mult(COEFF5_1, temp0, pOverflow);
    temp1 = add_16(data[0], temp1, pOverflow);
    data[0] = temp0;

    temp0 = mult(COEFF5_2, data[1], pOverflow);
    temp0 = sub(*in1, temp0, pOverflow);

    temp2 = mult(COEFF5_2, temp0, pOverflow);
    temp2 = add_16(data[1], temp2, pOverflow);

    data[1] = temp0;

    temp0 = add_16(temp1, temp2, pOverflow);
    *in0 = shr(temp0, 1, pOverflow);

    temp0 = sub(temp1, temp2, pOverflow);
    *in1 = shr(temp0, 1, pOverflow);
}




/*
------------------------------------------------------------------------------
 FUNCTION NAME: filter3
------------------------------------------------------------------------------
 INPUT AND OUTPUT DEFINITIONS


 Inputs:
    in0 -- array of type Word16 -- input values; output low-pass part
    in1 -- array of type Word16 -- input values; output high-pass part
    data -- array of type Word16 -- updated filter memory

 Outputs:
    in0 -- array of type Word16 -- input values; output low-pass part
    in1 -- array of type Word16 -- input values; output high-pass part
    data -- array of type Word16 -- updated filter memory
    pOverflow -- pointer to type Flag -- overflow indicator

 Returns:
    None

 Global Variables Used:
    None

 Local Variables Needed:
    None

------------------------------------------------------------------------------
 FUNCTION DESCRIPTION

 Purpose      : Third-order half-band lowpass/highpass filter pair with
                decimation.
------------------------------------------------------------------------------
 REQUIREMENTS

 None

------------------------------------------------------------------------------
 REFERENCES

 vad1.c, UMTS GSM AMR speech codec, R99 - Version 3.2.0, March 2, 2001

------------------------------------------------------------------------------
 PSEUDO-CODE


------------------------------------------------------------------------------
 CAUTION [optional]
 [State any special notes, constraints or cautions for users of this function]

------------------------------------------------------------------------------
*/

static void filter3(
    Word16 *in0,      /* i/o : input values; output low-pass part  */
    Word16 *in1,      /* i/o : input values; output high-pass part */
    Word16 *data,     /* i/o : updated filter memory               */
    Flag  *pOverflow  /* o : Flag set when overflow occurs         */
)
{
    Word16 temp1;
    Word16 temp2;

    temp1 = mult(COEFF3, *data, pOverflow);
    temp1 = sub(*in1, temp1, pOverflow);

    temp2 = mult(COEFF3, temp1, pOverflow);
    temp2 = add_16(*data, temp2, pOverflow);

    *data = temp1;

    temp1 = sub(*in0, temp2, pOverflow);

    *in1 = shr(temp1, 1, pOverflow);

    temp1 = add_16(*in0, temp2, pOverflow);

    *in0 = shr(temp1, 1, pOverflow);
}




/*
------------------------------------------------------------------------------
 FUNCTION NAME: level_calculation
------------------------------------------------------------------------------
 INPUT AND OUTPUT DEFINITIONS

 Inputs:
    data -- array of type Word16 -- signal buffer
    sub_level -- pointer to type Word16 -- level calculated at the end of
                                           the previous frame

    count1 -- Word16 -- number of samples to be counted
    count2 -- Word16 -- number of samples to be counted
    ind_m  -- Word16 -- step size for the index of the data buffer
    ind_a  -- Word16 -- starting index of the data buffer
    scale  -- Word16 -- scaling for the level calculation

 Outputs:
    sub_level -- pointer to tyep Word16 -- level of signal calculated from the
                                           last (count2 - count1) samples.
    pOverflow -- pointer to type Flag -- overflow indicator

 Returns:
    signal level

 Global Variables Used:


 Local Variables Needed:
    None

------------------------------------------------------------------------------
 FUNCTION DESCRIPTION

 Purpose      : Calculate signal level in a sub-band. Level is calculated
                by summing absolute values of the input data.

------------------------------------------------------------------------------
 REQUIREMENTS

 None

------------------------------------------------------------------------------
 REFERENCES

 vad1.c, UMTS GSM AMR speech codec, R99 - Version 3.2.0, March 2, 2001

------------------------------------------------------------------------------
 PSEUDO-CODE


------------------------------------------------------------------------------
 CAUTION [optional]
 [State any special notes, constraints or cautions for users of this function]

------------------------------------------------------------------------------
*/

static Word16 level_calculation(
    Word16 data[],     /* i   : signal buffer                                */
    Word16 *sub_level, /* i   : level calculate at the end of                */
    /*       the previous frame                           */
    /* o   : level of signal calculated from the last     */
    /*       (count2 - count1) samples                    */
    Word16 count1,     /* i   : number of samples to be counted              */
    Word16 count2,     /* i   : number of samples to be counted              */
    Word16 ind_m,      /* i   : step size for the index of the data buffer   */
    Word16 ind_a,      /* i   : starting index of the data buffer            */
    Word16 scale,      /* i   : scaling for the level calculation            */
    Flag  *pOverflow   /* o : Flag set when overflow occurs                  */
)
{
    Word32 l_temp1;
    Word32 l_temp2;
    Word16 level;
    Word16 i;

    l_temp1 = 0L;

    for (i = count1; i < count2; i++)
    {
        l_temp1 = L_mac(l_temp1, 1, abs_s(data[ind_m*i+ind_a]), pOverflow);
    }

    l_temp2 = L_add(l_temp1, L_shl(*sub_level, sub(16, scale, pOverflow), pOverflow), pOverflow);
    *sub_level = (Word16)(L_shl(l_temp1, scale, pOverflow) >> 16);

    for (i = 0; i < count1; i++)
    {
        l_temp2 = L_mac(l_temp2, 1, abs_s(data[ind_m*i+ind_a]), pOverflow);
    }
    level = (Word16)(L_shl(l_temp2, scale, pOverflow) >> 16);

    return level;
}




/*
------------------------------------------------------------------------------
 FUNCTION NAME: filter_bank
------------------------------------------------------------------------------
 INPUT AND OUTPUT DEFINITIONS

 Inputs:
    st -- pointer to type vadState1 --  State struct
    in -- array of type Word16 -- input frame

 Outputs:
    level -- array of type Word16 -- signal levels at each band
    st -- pointer to type vadState1 --  State struct
    pOverflow -- pointer to type Flag -- overflow indicator

 Returns:
    None

 Global Variables Used:
    None

 Local Variables Needed:
    None

------------------------------------------------------------------------------
 FUNCTION DESCRIPTION

 Purpose      : Divides input signal into 9-bands and calculas level of
                the signal in each band

------------------------------------------------------------------------------
 REQUIREMENTS

 None

------------------------------------------------------------------------------
 REFERENCES

 vad1.c, UMTS GSM AMR speech codec, R99 - Version 3.2.0, March 2, 2001

------------------------------------------------------------------------------
 PSEUDO-CODE


------------------------------------------------------------------------------
 CAUTION [optional]
 [State any special notes, constraints or cautions for users of this function]

------------------------------------------------------------------------------
*/

static void filter_bank(
    vadState1 *st,    /* i/o : State struct                    */
    Word16 in[],      /* i   : input frame                     */
    Word16 level[],   /* 0   : signal levels at each band      */
    Flag  *pOverflow  /* o   : Flag set when overflow occurs   */
)
{
    Word16 i;
    Word16 tmp_buf[FRAME_LEN];

    /* calculate the filter bank */

    first_filter_stage(in, tmp_buf, st->a_data5[0], pOverflow);

    for (i = 0; i < FRAME_LEN / 4; i++)
    {
        filter5(&tmp_buf[4*i], &tmp_buf[4*i+2], st->a_data5[1], pOverflow);
        filter5(&tmp_buf[4*i+1], &tmp_buf[4*i+3], st->a_data5[2], pOverflow);
    }
    for (i = 0; i < FRAME_LEN / 8; i++)
    {
        filter3(&tmp_buf[8*i+0], &tmp_buf[8*i+4], &st->a_data3[0], pOverflow);
        filter3(&tmp_buf[8*i+2], &tmp_buf[8*i+6], &st->a_data3[1], pOverflow);
        filter3(&tmp_buf[8*i+3], &tmp_buf[8*i+7], &st->a_data3[4], pOverflow);
    }

    for (i = 0; i < FRAME_LEN / 16; i++)
    {
        filter3(&tmp_buf[16*i+0], &tmp_buf[16*i+8], &st->a_data3[2], pOverflow);
        filter3(&tmp_buf[16*i+4], &tmp_buf[16*i+12], &st->a_data3[3], pOverflow);
    }

    /* calculate levels in each frequency band */

    /* 3000 - 4000 Hz*/
    level[8] = level_calculation(tmp_buf, &st->sub_level[8], FRAME_LEN / 4 - 8,
                                 FRAME_LEN / 4, 4, 1, 15, pOverflow);
    /* 2500 - 3000 Hz*/
    level[7] = level_calculation(tmp_buf, &st->sub_level[7], FRAME_LEN / 8 - 4,
                                 FRAME_LEN / 8, 8, 7, 16, pOverflow);
    /* 2000 - 2500 Hz*/
    level[6] = level_calculation(tmp_buf, &st->sub_level[6], FRAME_LEN / 8 - 4,
                                 FRAME_LEN / 8, 8, 3, 16, pOverflow);
    /* 1500 - 2000 Hz*/
    level[5] = level_calculation(tmp_buf, &st->sub_level[5], FRAME_LEN / 8 - 4,
                                 FRAME_LEN / 8, 8, 2, 16, pOverflow);
    /* 1000 - 1500 Hz*/
    level[4] = level_calculation(tmp_buf, &st->sub_level[4], FRAME_LEN / 8 - 4,
                                 FRAME_LEN / 8, 8, 6, 16, pOverflow);
    /* 750 - 1000 Hz*/
    level[3] = level_calculation(tmp_buf, &st->sub_level[3], FRAME_LEN / 16 - 2,
                                 FRAME_LEN / 16, 16, 4, 16, pOverflow);
    /* 500 - 750 Hz*/
    level[2] = level_calculation(tmp_buf, &st->sub_level[2], FRAME_LEN / 16 - 2,
                                 FRAME_LEN / 16, 16, 12, 16, pOverflow);
    /* 250 - 500 Hz*/
    level[1] = level_calculation(tmp_buf, &st->sub_level[1], FRAME_LEN / 16 - 2,
                                 FRAME_LEN / 16, 16, 8, 16, pOverflow);
    /* 0 - 250 Hz*/
    level[0] = level_calculation(tmp_buf, &st->sub_level[0], FRAME_LEN / 16 - 2,
                                 FRAME_LEN / 16, 16, 0, 16, pOverflow);
}



/*
------------------------------------------------------------------------------
 FUNCTION NAME: update_cntrl
------------------------------------------------------------------------------
 INPUT AND OUTPUT DEFINITIONS

 Inputs:
    st -- pointer to type vadState1 --  State struct
    level -- array of type Word16 -- sub-band levels of the input frame

 Outputs:
    st -- pointer to type vadState1 --  State struct
    pOverflow -- pointer to type Flag -- overflow indicator

 Returns:
    None

 Global Variables Used:
    None

 Local Variables Needed:
    None

------------------------------------------------------------------------------
 FUNCTION DESCRIPTION

 Purpose    : Control update of the background noise estimate.
 Inputs     : pitch:      flags for pitch detection
              stat_count: stationary counter
              tone:       flags indicating presence of a tone
              complex:      flags for complex  detection
              vadreg:     intermediate VAD flags
 Output     : stat_count: stationary counter


------------------------------------------------------------------------------
 REQUIREMENTS

 None

------------------------------------------------------------------------------
 REFERENCES

 vad1.c, UMTS GSM AMR speech codec, R99 - Version 3.2.0, March 2, 2001

------------------------------------------------------------------------------
 PSEUDO-CODE


------------------------------------------------------------------------------
 CAUTION [optional]
 [State any special notes, constraints or cautions for users of this function]

------------------------------------------------------------------------------
*/

static void update_cntrl(
    vadState1 *st,   /* i/o : State struct                       */
    Word16 level[],  /* i   : sub-band levels of the input frame */
    Flag  *pOverflow /* o   : Flag set when overflow occurs      */
)
{
    Word16 i;
    Word16 temp;
    Word16 stat_rat;
    Word16 exp;
    Word16 num;
    Word16 denom;
    Word16 alpha;

    /* handle highband complex signal input  separately       */
    /* if ther has been highband correlation for some time    */
    /* make sure that the VAD update speed is low for a while */
    if (st->complex_warning != 0)
    {
        if (st->stat_count < CAD_MIN_STAT_COUNT)
        {
            st->stat_count = CAD_MIN_STAT_COUNT;
        }
    }
    /* NB stat_count is allowed to be decreased by one below again  */
    /* deadlock in speech is not possible unless the signal is very */
    /* complex and need a high rate                                 */

    /* if fullband pitch or tone have been detected for a while, initialize stat_count */
    if (((Word16)(st->pitch & 0x6000) == 0x6000) ||
            ((Word16)(st->tone & 0x7c00) == 0x7c00))
    {
        st->stat_count = STAT_COUNT;
    }
    else
    {
        /* if 8 last vad-decisions have been "0", reinitialize stat_count */
        if ((st->vadreg & 0x7f80) == 0)
        {
            st->stat_count = STAT_COUNT;
        }
        else
        {
            stat_rat = 0;
            for (i = 0; i < COMPLEN; i++)
            {
                if (level[i] > st->ave_level[i])
                {
                    num = level[i];
                    denom = st->ave_level[i];
                }
                else
                {
                    num = st->ave_level[i];
                    denom = level[i];
                }
                /* Limit nimimum value of num and denom to STAT_THR_LEVEL */
                if (num < STAT_THR_LEVEL)
                {
                    num = STAT_THR_LEVEL;
                }
                if (denom < STAT_THR_LEVEL)
                {
                    denom = STAT_THR_LEVEL;
                }

                exp = norm_s(denom);

                denom = shl(denom, exp, pOverflow);

                /* stat_rat = num/denom * 64 */
                temp = shr(num, 1, pOverflow);
                temp = div_s(temp, denom);

                stat_rat = add_16(stat_rat, shr(temp, sub(8, exp, pOverflow), pOverflow), pOverflow);
            }

            /* compare stat_rat with a threshold and update stat_count */
            if (stat_rat > STAT_THR)
            {
                st->stat_count = STAT_COUNT;
            }
            else
            {
                if ((st->vadreg & 0x4000) != 0)
                {
                    if (st->stat_count != 0)
                    {
                        st->stat_count = sub(st->stat_count, 1, pOverflow);
                    }
                }
            }
        }
    }

    /* Update average amplitude estimate for stationarity estimation */
    alpha = ALPHA4;
    if (st->stat_count == STAT_COUNT)
    {
        alpha = 32767;
    }
    else if ((st->vadreg & 0x4000) == 0)
    {
        alpha = ALPHA5;
    }

    for (i = 0; i < COMPLEN; i++)
    {
        temp = sub(level[i], st->ave_level[i], pOverflow);
        temp = mult_r(alpha, temp, pOverflow);

        st->ave_level[i] =
            add_16(
                st->ave_level[i],
                temp,
                pOverflow);
    }
}



/*
------------------------------------------------------------------------------
 FUNCTION NAME: hangover_addition
------------------------------------------------------------------------------
 INPUT AND OUTPUT DEFINITIONS

 Inputs:
    noise_level -- Word16 -- average level of the noise estimates
    low_power   -- Word16 -- flag power of the input frame

 Outputs:
    st -- pointer to type vadState1 --  State struct
    pOverflow -- pointer to type Flag -- overflow indicato

 Returns:
    VAD_flag indicating final VAD decision (Word16)

 Global Variables Used:
    None

 Local Variables Needed:
    None

------------------------------------------------------------------------------
 FUNCTION DESCRIPTION

 Function     : hangover_addition
 Purpose      : Add hangover for complex signal or after speech bursts
 Inputs       : burst_count:  counter for the length of speech bursts
                hang_count:   hangover counter
                vadreg:       intermediate VAD decision
 Outputs      : burst_count:  counter for the length of speech bursts
                hang_count:   hangover counter
 Return value : VAD_flag indicating final VAD decision


------------------------------------------------------------------------------
 REQUIREMENTS

 None

------------------------------------------------------------------------------
 REFERENCES

 vad1.c, UMTS GSM AMR speech codec, R99 - Version 3.2.0, March 2, 2001

------------------------------------------------------------------------------
 PSEUDO-CODE


------------------------------------------------------------------------------
 CAUTION [optional]
 [State any special notes, constraints or cautions for users of this function]

------------------------------------------------------------------------------
*/

static Word16 hangover_addition(
    vadState1 *st,      /* i/o : State struct                     */
    Word16 noise_level, /* i   : average level of the noise       */
    /*       estimates                        */
    Word16 low_power,   /* i   : flag power of the input frame    */
    Flag  *pOverflow    /* o   : Flag set when overflow occurs    */
)
{
    Word16 hang_len;
    Word16 burst_len;

    /*
       Calculate burst_len and hang_len
       burst_len: number of consecutive intermediate vad flags with "1"-decision
                  required for hangover addition
       hang_len:  length of the hangover
       */

    if (noise_level > HANG_NOISE_THR)
    {
        burst_len = BURST_LEN_HIGH_NOISE;
        hang_len = HANG_LEN_HIGH_NOISE;
    }
    else
    {
        burst_len = BURST_LEN_LOW_NOISE;
        hang_len = HANG_LEN_LOW_NOISE;
    }

    /* if the input power (pow_sum) is lower than a threshold, clear
       counters and set VAD_flag to "0"  "fast exit"                 */
    if (low_power != 0)
    {
        st->burst_count = 0;
        st->hang_count = 0;
        st->complex_hang_count = 0;
        st->complex_hang_timer = 0;
        return 0;
    }

    if (st->complex_hang_timer > CVAD_HANG_LIMIT)
    {
        if (st->complex_hang_count < CVAD_HANG_LENGTH)
        {
            st->complex_hang_count = CVAD_HANG_LENGTH;
        }
    }

    /* long time very complex signal override VAD output function */
    if (st->complex_hang_count != 0)
    {
        st->burst_count = BURST_LEN_HIGH_NOISE;
        st->complex_hang_count = sub(st->complex_hang_count, 1, pOverflow);
        return 1;
    }
    else
    {
        /* let hp_corr work in from a noise_period indicated by the VAD */
        if (((st->vadreg & 0x3ff0) == 0) &&
                (st->corr_hp_fast > CVAD_THRESH_IN_NOISE))
        {
            return 1;
        }
    }

    /* update the counters (hang_count, burst_count) */
    if ((st->vadreg & 0x4000) != 0)
    {
        st->burst_count = add_16(st->burst_count, 1, pOverflow);

        if (st->burst_count >= burst_len)
        {
            st->hang_count = hang_len;
        }
        return 1;
    }
    else
    {
        st->burst_count = 0;
        if (st->hang_count > 0)
        {
            st->hang_count = sub(st->hang_count, 1, pOverflow);
            return 1;
        }
    }
    return 0;
}



/*
------------------------------------------------------------------------------
 FUNCTION NAME: noise_estimate_update
------------------------------------------------------------------------------
 INPUT AND OUTPUT DEFINITIONS

 Inputs:
    st -- pointer to type vadState1 --  State struct
    level -- array of type Word16 -- sub-band levels of the input frame

 Outputs:
    st -- pointer to type vadState1 --  State struct
    pOverflow -- pointer to type Flag -- overflow indicator

 Returns:
    None

 Global Variables Used:
    None

 Local Variables Needed:
    None

------------------------------------------------------------------------------
 FUNCTION DESCRIPTION

 Purpose    : Update of background noise estimate
 Inputs     : bckr_est:   background noise estimate
              pitch:      flags for pitch detection
              stat_count: stationary counter
 Outputs    : bckr_est:   background noise estimate

------------------------------------------------------------------------------
 REQUIREMENTS

 None

------------------------------------------------------------------------------
 REFERENCES

 vad1.c, UMTS GSM AMR speech codec, R99 - Version 3.2.0, March 2, 2001

------------------------------------------------------------------------------
 PSEUDO-CODE


------------------------------------------------------------------------------
 CAUTION [optional]
 [State any special notes, constraints or cautions for users of this function]

------------------------------------------------------------------------------
*/

static void noise_estimate_update(
    vadState1 *st,    /* i/o : State struct                       */
    Word16 level[],   /* i   : sub-band levels of the input frame */
    Flag  *pOverflow  /* o : Flag set when overflow occurs        */
)
{
    Word16 i;
    Word16 alpha_up;
    Word16 alpha_down;
    Word16 bckr_add;

    /* Control update of bckr_est[] */
    update_cntrl(st, level, pOverflow);

    /* Choose update speed */
    bckr_add = 2;

    if (((0x7800 & st->vadreg) == 0) &&
            ((st->pitch & 0x7800) == 0)
            && (st->complex_hang_count == 0))
    {
        alpha_up = ALPHA_UP1;
        alpha_down = ALPHA_DOWN1;
    }
    else
    {
        if ((st->stat_count == 0)
                && (st->complex_hang_count == 0))
        {
            alpha_up = ALPHA_UP2;
            alpha_down = ALPHA_DOWN2;
        }
        else
        {
            alpha_up = 0;
            alpha_down = ALPHA3;
            bckr_add = 0;
        }
    }

    /* Update noise estimate (bckr_est) */
    for (i = 0; i < COMPLEN; i++)
    {
        Word16 temp;

        temp = sub(st->old_level[i], st->bckr_est[i], pOverflow);

        if (temp < 0)
        { /* update downwards*/
            temp = mult_r(alpha_down, temp, pOverflow);
            temp = add_16(st->bckr_est[i], temp, pOverflow);

            st->bckr_est[i] = add_16(-2, temp, pOverflow);

            /* limit minimum value of the noise estimate to NOISE_MIN */
            if (st->bckr_est[i] < NOISE_MIN)
            {
                st->bckr_est[i] = NOISE_MIN;
            }
        }
        else
        { /* update upwards */
            temp = mult_r(alpha_up, temp, pOverflow);
            temp = add_16(st->bckr_est[i], temp, pOverflow);
            st->bckr_est[i] = add_16(bckr_add, temp, pOverflow);

            /* limit maximum value of the noise estimate to NOISE_MAX */
            if (st->bckr_est[i] > NOISE_MAX)
            {
                st->bckr_est[i] = NOISE_MAX;
            }
        }
    }

    /* Update signal levels of the previous frame (old_level) */
    for (i = 0; i < COMPLEN; i++)
    {
        st->old_level[i] = level[i];
    }
}


/*
------------------------------------------------------------------------------
 FUNCTION NAME: complex_estimate_adapt
------------------------------------------------------------------------------
 INPUT AND OUTPUT DEFINITIONS

 Inputs:
    st -- pointer to type vadState1 --  State struct
    low_power -- Word16 -- very low level flag of the input frame

 Outputs:
    st -- pointer to type vadState1 --  State struct
    pOverflow -- pointer to type Flag -- overflow indicator

 Returns:
    None

 Global Variables Used:
    None

 Local Variables Needed:
    None

------------------------------------------------------------------------------
 FUNCTION DESCRIPTION

 Function   : complex_estimate_adapt
 Purpose    : Update/adapt of complex signal estimate
 Inputs     : low_power:   low signal power flag
 Outputs    : st->corr_hp_fast:   long term complex signal estimate

------------------------------------------------------------------------------
 REQUIREMENTS

 None

------------------------------------------------------------------------------
 REFERENCES

 vad1.c, UMTS GSM AMR speech codec, R99 - Version 3.2.0, March 2, 2001

------------------------------------------------------------------------------
 PSEUDO-CODE


------------------------------------------------------------------------------
 CAUTION [optional]
 [State any special notes, constraints or cautions for users of this function]

------------------------------------------------------------------------------
*/

static void complex_estimate_adapt(
    vadState1 *st,      /* i/o : VAD state struct                       */
    Word16 low_power,   /* i   : very low level flag of the input frame */
    Flag  *pOverflow    /* o : Flag set when overflow occurs            */
)
{
    Word16 alpha;            /* Q15 */
    Word32 L_tmp;            /* Q31 */


    /* adapt speed on own state */
    if (st->best_corr_hp < st->corr_hp_fast) /* decrease */
    {
        if (st->corr_hp_fast < CVAD_THRESH_ADAPT_HIGH)
        {  /* low state  */
            alpha = CVAD_ADAPT_FAST;
        }
        else
        {  /* high state */
            alpha = CVAD_ADAPT_REALLY_FAST;
        }
    }
    else  /* increase */
    {
        if (st->corr_hp_fast < CVAD_THRESH_ADAPT_HIGH)
        {
            alpha = CVAD_ADAPT_FAST;
        }
        else
        {
            alpha = CVAD_ADAPT_SLOW;
        }
    }

    L_tmp = ((Word32)st->corr_hp_fast << 16);
    L_tmp = L_msu(L_tmp, alpha, st->corr_hp_fast, pOverflow);
    L_tmp = L_mac(L_tmp, alpha, st->best_corr_hp, pOverflow);
    st->corr_hp_fast = pv_round(L_tmp, pOverflow);           /* Q15 */

    if (st->corr_hp_fast < CVAD_MIN_CORR)
    {
        st->corr_hp_fast = CVAD_MIN_CORR;
    }

    if (low_power != 0)
    {
        st->corr_hp_fast = CVAD_MIN_CORR;
    }
}


/*
------------------------------------------------------------------------------
 FUNCTION NAME: complex_vad
------------------------------------------------------------------------------
 INPUT AND OUTPUT DEFINITIONS

 Inputs:
    st -- pointer to type vadState1 --  State struct
    low_power -- Word16 -- flag power of the input frame

 Outputs:
    st -- pointer to type vadState1 --  State struct
    pOverflow -- pointer to type Flag -- overflow indicator


 Returns:
    the complex background decision

 Global Variables Used:
    None

 Local Variables Needed:
    None

------------------------------------------------------------------------------
 FUNCTION DESCRIPTION

 Purpose      : complex background decision
 Return value : the complex background decision

------------------------------------------------------------------------------
 REQUIREMENTS

 None

------------------------------------------------------------------------------
 REFERENCES

 vad1.c, UMTS GSM AMR speech codec, R99 - Version 3.2.0, March 2, 2001

------------------------------------------------------------------------------
 PSEUDO-CODE


------------------------------------------------------------------------------
 CAUTION [optional]
 [State any special notes, constraints or cautions for users of this function]

------------------------------------------------------------------------------
*/

static Word16 complex_vad(
    vadState1 *st,    /* i/o : VAD state struct              */
    Word16 low_power, /* i   : flag power of the input frame */
    Flag  *pOverflow  /* o : Flag set when overflow occurs   */
)
{
    st->complex_high = shr(st->complex_high, 1, pOverflow);
    st->complex_low = shr(st->complex_low, 1, pOverflow);

    if (low_power == 0)
    {
        if (st->corr_hp_fast > CVAD_THRESH_ADAPT_HIGH)
        {
            st->complex_high |= 0x4000;
        }

        if (st->corr_hp_fast > CVAD_THRESH_ADAPT_LOW)
        {
            st->complex_low |= 0x4000;
        }
    }

    if (st->corr_hp_fast > CVAD_THRESH_HANG)
    {
        st->complex_hang_timer = add_16(st->complex_hang_timer, 1, pOverflow);
    }
    else
    {
        st->complex_hang_timer =  0;
    }

    return ((Word16)(st->complex_high & 0x7f80) == 0x7f80 ||
            (Word16)(st->complex_low & 0x7fff) == 0x7fff);
}


/*
------------------------------------------------------------------------------
 FUNCTION NAME: vad_decision
------------------------------------------------------------------------------
 INPUT AND OUTPUT DEFINITIONS

 Inputs:
    st -- pointer to type vadState1 --  State struct
    level -- array of type Word16 -- sub-band levels of the input frame
    pow_sum -- Word32 -- power of the input frame

 Outputs:
    st -- pointer to type vadState1 --  State struct
    pOverflow -- pointer to type Flag -- overflow indicator

 Returns:
    VAD_flag (Word16)

 Global Variables Used:
    None

 Local Variables Needed:
    None

------------------------------------------------------------------------------
 FUNCTION DESCRIPTION

 Purpose      : Calculates VAD_flag
 Inputs       : bckr_est:    background noise estimate
                vadreg:      intermediate VAD flags
 Outputs      : noise_level: average level of the noise estimates
                vadreg:      intermediate VAD flags
 Return value : VAD_flag

------------------------------------------------------------------------------
 REQUIREMENTS

 None

------------------------------------------------------------------------------
 REFERENCES

 vad1.c, UMTS GSM AMR speech codec, R99 - Version 3.2.0, March 2, 2001

------------------------------------------------------------------------------
 PSEUDO-CODE


------------------------------------------------------------------------------
 CAUTION [optional]
 [State any special notes, constraints or cautions for users of this function]

------------------------------------------------------------------------------
*/

static Word16 vad_decision(
    vadState1 *st,         /* i/o : State struct                       */
    Word16 level[COMPLEN], /* i   : sub-band levels of the input frame */
    Word32 pow_sum,        /* i   : power of the input frame           */
    Flag  *pOverflow       /* o : Flag set when overflow occurs        */
)
{
    Word16 i;
    Word16 snr_sum;
    Word32 L_temp;
    Word16 vad_thr;
    Word16 temp;
    Word16 noise_level;
    Word16 low_power_flag;
    Word16 temp1;

    /*
       Calculate squared sum of the input levels (level)
       divided by the background noise components (bckr_est).
       */
    L_temp = 0;

    for (i = 0; i < COMPLEN; i++)
    {
        Word16 exp;

        exp = norm_s(st->bckr_est[i]);
        temp = shl(st->bckr_est[i], exp, pOverflow);
        temp = div_s(shr(level[i], 1, pOverflow), temp);
        temp = shl(temp, sub(exp, UNIRSHFT - 1, pOverflow), pOverflow);
        L_temp = L_mac(L_temp, temp, temp, pOverflow);
    }

    snr_sum = (Word16)(L_shl(L_temp, 6, pOverflow) >> 16);
    snr_sum = mult(snr_sum, INV_COMPLEN, pOverflow);

    /* Calculate average level of estimated background noise */
    L_temp = 0;
    for (i = 0; i < COMPLEN; i++)
    {
        L_temp = L_add(L_temp, st->bckr_est[i], pOverflow);
    }

    noise_level = (Word16)(L_shl(L_temp, 13, pOverflow) >> 16);

    /* Calculate VAD threshold */
    temp1 = sub(noise_level, VAD_P1, pOverflow);
    temp1 = mult(VAD_SLOPE, temp1, pOverflow);
    vad_thr = add_16(temp1, VAD_THR_HIGH, pOverflow);

    if (vad_thr < VAD_THR_LOW)
    {
        vad_thr = VAD_THR_LOW;
    }

    /* Shift VAD decision register */
    st->vadreg = shr(st->vadreg, 1, pOverflow);

    /* Make intermediate VAD decision */
    if (snr_sum > vad_thr)
    {
        st->vadreg |= 0x4000;
    }
    /* primary vad decsion made */

    /* check if the input power (pow_sum) is lower than a threshold" */
    if (L_sub(pow_sum, VAD_POW_LOW, pOverflow) < 0)
    {
        low_power_flag = 1;
    }
    else
    {
        low_power_flag = 0;
    }

    /* update complex signal estimate st->corr_hp_fast and hangover reset timer using */
    /* low_power_flag and corr_hp_fast  and various adaptation speeds                 */
    complex_estimate_adapt(st, low_power_flag, pOverflow);

    /* check multiple thresholds of the st->corr_hp_fast value */
    st->complex_warning = complex_vad(st, low_power_flag, pOverflow);

    /* Update speech subband vad background noise estimates */
    noise_estimate_update(st, level, pOverflow);

    /*  Add speech and complex hangover and return speech VAD_flag */
    /*  long term complex hangover may be added */
    st->speech_vad_decision = hangover_addition(st, noise_level, low_power_flag, pOverflow);

    return (st->speech_vad_decision);
}


/*
------------------------------------------------------------------------------
 FUNCTION NAME: vad1_init
------------------------------------------------------------------------------
 INPUT AND OUTPUT DEFINITIONS

 Inputs:
    state -- double pointer to type vadState1 -- pointer to memory to
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

 vad1.c, UMTS GSM AMR speech codec, R99 - Version 3.2.0, March 2, 2001

------------------------------------------------------------------------------
 PSEUDO-CODE


------------------------------------------------------------------------------
 CAUTION [optional]
 [State any special notes, constraints or cautions for users of this function]

------------------------------------------------------------------------------
*/

Word16 vad1_init(vadState1 **state)
{
    vadState1* s;

    if (state == (vadState1 **) NULL)
    {
        return -1;
    }
    *state = NULL;

    /* allocate memory */
    if ((s = (vadState1 *) oscl_malloc(sizeof(vadState1))) == NULL)
    {
        return -1;
    }

    vad1_reset(s);

    *state = s;

    return 0;
}

/*
------------------------------------------------------------------------------
 FUNCTION NAME: vad1_reset
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

 vad1.c, UMTS GSM AMR speech codec, R99 - Version 3.2.0, March 2, 2001

------------------------------------------------------------------------------
 PSEUDO-CODE


------------------------------------------------------------------------------
 CAUTION [optional]
 [State any special notes, constraints or cautions for users of this function]

------------------------------------------------------------------------------
*/

Word16 vad1_reset(vadState1 *state)
{
    Word16 i;
    Word16 j;

    if (state == (vadState1 *) NULL)
    {
        return -1;
    }

    /* Initialize pitch detection variables */
    state->oldlag_count = 0;
    state->oldlag = 0;
    state->pitch = 0;
    state->tone = 0;

    state->complex_high = 0;
    state->complex_low = 0;
    state->complex_hang_timer = 0;

    state->vadreg = 0;

    state->stat_count = 0;
    state->burst_count = 0;
    state->hang_count = 0;
    state->complex_hang_count = 0;

    /* initialize memory used by the filter bank */
    for (i = 0; i < 3; i++)
    {
        for (j = 0; j < 2; j++)
        {
            state->a_data5[i][j] = 0;
        }
    }

    for (i = 0; i < 5; i++)
    {
        state->a_data3[i] = 0;
    }

    /* initialize the rest of the memory */
    for (i = 0; i < COMPLEN; i++)
    {
        state->bckr_est[i] = NOISE_INIT;
        state->old_level[i] = NOISE_INIT;
        state->ave_level[i] = NOISE_INIT;
        state->sub_level[i] = 0;
    }

    state->best_corr_hp = CVAD_LOWPOW_RESET;

    state->speech_vad_decision = 0;
    state->complex_warning = 0;
    state->sp_burst_count = 0;

    state->corr_hp_fast = CVAD_LOWPOW_RESET;

    return 0;
}


/*
------------------------------------------------------------------------------
 FUNCTION NAME: vad1_exit
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

 vad1.c, UMTS GSM AMR speech codec, R99 - Version 3.2.0, March 2, 2001

------------------------------------------------------------------------------
 PSEUDO-CODE


------------------------------------------------------------------------------
 CAUTION [optional]
 [State any special notes, constraints or cautions for users of this function]

------------------------------------------------------------------------------
*/

void vad1_exit(vadState1 **state)
{
    if (state == NULL || *state == NULL)
        return;

    /* deallocate memory */
    oscl_free(*state);
    *state = NULL;

    return;
}


/*
------------------------------------------------------------------------------
 FUNCTION NAME: vad_complex_detection_update
------------------------------------------------------------------------------
 INPUT AND OUTPUT DEFINITIONS

 Inputs:
    best_corr_hp -- Word16 -- best Corr
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

 Purpose      : update vad->bestCorr_hp  complex signal feature state
------------------------------------------------------------------------------
 REQUIREMENTS

 None

------------------------------------------------------------------------------
 REFERENCES

 vad1.c, UMTS GSM AMR speech codec, R99 - Version 3.2.0, March 2, 2001

------------------------------------------------------------------------------
 PSEUDO-CODE


------------------------------------------------------------------------------
 CAUTION [optional]
 [State any special notes, constraints or cautions for users of this function]

------------------------------------------------------------------------------
*/

void vad_complex_detection_update(
    vadState1 *st,       /* i/o : State struct */
    Word16 best_corr_hp) /* i   : best Corr    */
{
    st->best_corr_hp = best_corr_hp;
}



/*
------------------------------------------------------------------------------
 FUNCTION NAME: vad_tone_detection
------------------------------------------------------------------------------
 INPUT AND OUTPUT DEFINITIONS

 Inputs:
    st -- pointer to type vadState1 --  State struct
    t0 -- Word32 -- autocorrelation maxima
    t1 -- Word32 -- energy

 Outputs:
    st -- pointer to type vadState1 --  State struct
    pOverflow -- pointer to type Flag -- overflow indicator

 Returns:
    None

 Global Variables Used:
    None

 Local Variables Needed:
    None

------------------------------------------------------------------------------
 FUNCTION DESCRIPTION

 Purpose      : Set tone flag if pitch gain is high. This is used to detect
                signaling tones and other signals with high pitch gain.
 Inputs       : tone: flags indicating presence of a tone
 Outputs      : tone: flags indicating presence of a tone
------------------------------------------------------------------------------
 REQUIREMENTS

 None

------------------------------------------------------------------------------
 REFERENCES

 vad1.c, UMTS GSM AMR speech codec, R99 - Version 3.2.0, March 2, 2001

------------------------------------------------------------------------------
 PSEUDO-CODE


------------------------------------------------------------------------------
 CAUTION [optional]
 [State any special notes, constraints or cautions for users of this function]

------------------------------------------------------------------------------
*/

void vad_tone_detection(
    vadState1 *st,    /* i/o : State struct                       */
    Word32 t0,        /* i   : autocorrelation maxima             */
    Word32 t1,        /* i   : energy                             */
    Flag  *pOverflow  /* o : Flag set when overflow occurs        */
)
{
    Word16 temp;
    /*
       if (t0 > TONE_THR * t1)
       set tone flag
       */
    temp = pv_round(t1, pOverflow);

    if ((temp > 0) && (L_msu(t0, temp, TONE_THR, pOverflow) > 0))
    {
        st->tone |= 0x4000;
    }
}


/*
------------------------------------------------------------------------------
 FUNCTION NAME: vad_tone_detection_update
------------------------------------------------------------------------------
 INPUT AND OUTPUT DEFINITIONS

 Inputs:
    one_lag_per_frame -- Word16 -- 1 if one open-loop lag is calculated per
                                   each frame, otherwise 0
    st -- pointer to type vadState1 --  State struct

 Outputs:
    st -- pointer to type vadState1 --  State struct
    pOverflow -- pointer to type Flag -- overflow indicator

 Returns:
    None

 Global Variables Used:
    None

 Local Variables Needed:
    None

------------------------------------------------------------------------------
 FUNCTION DESCRIPTION

 Purpose      : Update the tone flag register. Tone flags are shifted right
                by one bit. This function should be called from the speech
                encoder before call to Vad_tone_detection() function.

------------------------------------------------------------------------------
 REQUIREMENTS

 None

------------------------------------------------------------------------------
 REFERENCES

 vad1.c, UMTS GSM AMR speech codec, R99 - Version 3.2.0, March 2, 2001

------------------------------------------------------------------------------
 PSEUDO-CODE


------------------------------------------------------------------------------
 CAUTION [optional]
 [State any special notes, constraints or cautions for users of this function]

------------------------------------------------------------------------------
*/

void vad_tone_detection_update(
    vadState1 *st,              /* i/o : State struct           */
    Word16 one_lag_per_frame,   /* i   : 1 if one open-loop lag */
    /*       is calculated per each */
    /*       frame, otherwise 0     */
    Flag *pOverflow             /* o   : Flags overflow         */
)
{
    /* Shift tone flags right by one bit */
    st->tone = shr(st->tone, 1, pOverflow);

    /* If open-loop lag is calculated only once in each frame, do extra update
       and assume that the other tone flag of the frame is one. */
    if (one_lag_per_frame != 0)
    {
        st->tone = shr(st->tone, 1, pOverflow);
        st->tone |= 0x2000;
    }
}


/*
------------------------------------------------------------------------------
 FUNCTION NAME: vad_pitch_detection
------------------------------------------------------------------------------
 INPUT AND OUTPUT DEFINITIONS

 Inputs:
    T_op -- array of type Word16 -- speech encoder open loop lags
    st -- pointer to type vadState1 --  State struct

 Outputs:
    st -- pointer to type vadState1 --  State struct
    pOverflow -- pointer to type Flag -- overflow indicator

 Returns:
    None

 Global Variables Used:
    None

 Local Variables Needed:
    None

------------------------------------------------------------------------------
 FUNCTION DESCRIPTION

 Purpose      : Test whether signal contains pitch or other periodic
                component.
 Return value : Boolean voiced / unvoiced decision in state variable

------------------------------------------------------------------------------
 REQUIREMENTS

 None

------------------------------------------------------------------------------
 REFERENCES

 vad1.c, UMTS GSM AMR speech codec, R99 - Version 3.2.0, March 2, 2001

------------------------------------------------------------------------------
 PSEUDO-CODE


------------------------------------------------------------------------------
 CAUTION [optional]
 [State any special notes, constraints or cautions for users of this function]

------------------------------------------------------------------------------
*/

void vad_pitch_detection(
    vadState1 *st,    /* i/o : State struct                  */
    Word16 T_op[],    /* i   : speech encoder open loop lags */
    Flag  *pOverflow  /* o : Flag set when overflow occurs   */
)
{
    Word16 lagcount;
    Word16 i;
    Word16 temp;

    lagcount = 0;

    for (i = 0; i < 2; i++)
    {
        temp = sub(st->oldlag, T_op[i], pOverflow);
        temp = abs_s(temp);

        if (temp < LTHRESH)
        {
            lagcount += 1;
        }

        /* Save the current LTP lag */
        st->oldlag = T_op[i];
    }

    /* Make pitch decision.
       Save flag of the pitch detection to the variable pitch.
       */
    st->pitch = shr(st->pitch, 1, pOverflow);

    temp =
        add_16(
            st->oldlag_count,
            lagcount,
            pOverflow);

    if (temp >= NTHRESH)
    {
        st->pitch |= 0x4000;
    }

    /* Update oldlagcount */
    st->oldlag_count = lagcount;
}

/*
------------------------------------------------------------------------------
 FUNCTION NAME: vad1
------------------------------------------------------------------------------
 INPUT AND OUTPUT DEFINITIONS

 Inputs:
    st -- pointer to type vadState1 --  State struct
    in_buf -- array of type Word16 -- samples of the input frame

 Outputs:
    st -- pointer to type vadState1 --  State struct
    pOverflow -- pointer to type Flag -- overflow indicator

 Returns:
    VAD Decision, 1 = speech, 0 = noise

 Global Variables Used:
    None

 Local Variables Needed:
    None

------------------------------------------------------------------------------
 FUNCTION DESCRIPTION

 Purpose      : Main program for Voice Activity Detection (VAD) for AMR
 Return value : VAD Decision, 1 = speech, 0 = noise

------------------------------------------------------------------------------
 REQUIREMENTS

 None

------------------------------------------------------------------------------
 REFERENCES

 vad1.c, UMTS GSM AMR speech codec, R99 - Version 3.2.0, March 2, 2001

------------------------------------------------------------------------------
 PSEUDO-CODE


------------------------------------------------------------------------------
 CAUTION [optional]
 [State any special notes, constraints or cautions for users of this function]

------------------------------------------------------------------------------
*/

Word16 vad1(
    vadState1 *st,    /* i/o : State struct                       */
    Word16 in_buf[],  /* i   : samples of the input frame         */
    Flag  *pOverflow  /* o   : Flag set when overflow occurs      */
)
{
    Word16 level[COMPLEN];
    Word32 pow_sum;
    Word16 i;

    /* Calculate power of the input frame. */
    pow_sum = 0L;

    for (i = 0; i < FRAME_LEN; i++)
    {
        pow_sum = L_mac(pow_sum, in_buf[i-LOOKAHEAD], in_buf[i-LOOKAHEAD], pOverflow);
    }

    /*
      If input power is very low, clear pitch flag of the current frame
      */
    if (L_sub(pow_sum, POW_PITCH_THR, pOverflow) < 0)
    {
        st->pitch = st->pitch & 0x3fff;
    }

    /*
      If input power is very low, clear complex flag of the "current" frame
      */
    if (L_sub(pow_sum, POW_COMPLEX_THR, pOverflow) < 0)
    {
        st->complex_low = st->complex_low & 0x3fff;
    }

    /*
      Run the filter bank which calculates signal levels at each band
      */
    filter_bank(st, in_buf, level, pOverflow);

    return (vad_decision(st, level, pow_sum, pOverflow));
}


