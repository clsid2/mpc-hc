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
------------------------------------------------------------------------------



 Filename: spstproc.cpp
 Functions: subframePostProc

------------------------------------------------------------------------------
 MODULE DESCRIPTION

    Subframe post processing
------------------------------------------------------------------------------
*/

/*----------------------------------------------------------------------------
; INCLUDES
----------------------------------------------------------------------------*/
#include "spstproc.h"
#include "syn_filt.h"
#include "cnst.h"

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
 FUNCTION NAME: subframePostProc
------------------------------------------------------------------------------
 INPUT AND OUTPUT DEFINITIONS


 Inputs:
    speech    -- Pointer to Word16 -- speech segment
    mode      -- enum Mode         -- coder mode
    i_subfr   -- Word16 -- Subframe nr
    gain_pit  -- Word16 -- Pitch gain  Q14
    gain_code -- Word16 -- Decoded innovation gain
    Aq        -- Pointer to Word16 -- A(z) quantized for the 4 subframes
    synth     -- Word16 Array -- Local synthesis
    xn        -- Word16 Array -- Target vector for pitch search
    code      -- Word16 Array -- Fixed codebook exitation
    y1        -- Word16 Array -- Filtered adaptive exitation
    y2        -- Word16 Array -- Filtered fixed codebook excitation
    mem_syn   -- Pointer to Word16 -- memory of synthesis filter

 Outputs:
    mem_syn -- Pointer to Word16 -- memory of synthesis filter
    mem_err -- Pointer to Word16 -- pointer to error signal
    mem_w0  -- Pointer to Word16 -- memory of weighting filter
    exc     -- Pointer to Word16 -- long term prediction residual
    sharp   -- Pointer to Word16 -- pitch sharpening value
    pOverflow -- Pointer to Flag -- overflow indicator

 Returns:
    None

 Global Variables Used:
    None

 Local Variables Needed:
    None

------------------------------------------------------------------------------
 FUNCTION DESCRIPTION


------------------------------------------------------------------------------
 REQUIREMENTS

 None

------------------------------------------------------------------------------
 REFERENCES

 spstproc.c, UMTS GSM AMR speech codec, R99 - Version 3.2.0, March 2, 2001

------------------------------------------------------------------------------
 PSEUDO-CODE


------------------------------------------------------------------------------
 CAUTION [optional]
 [State any special notes, constraints or cautions for users of this function]

------------------------------------------------------------------------------
*/

void subframePostProc(
    Word16 *speech,   /* i   : speech segment                        */
    enum Mode mode,   /* i   : coder mode                            */
    Word16 i_subfr,   /* i   : Subframe nr                           */
    Word16 gain_pit,  /* i   : Pitch gain                       Q14  */
    Word16 gain_code, /* i   : Decoded innovation gain               */
    Word16 *Aq,       /* i   : A(z) quantized for the 4 subframes    */
    Word16 synth[],   /* i   : Local snthesis                        */
    Word16 xn[],      /* i   : Target vector for pitch search        */
    Word16 code[],    /* i   : Fixed codebook exitation              */
    Word16 y1[],      /* i   : Filtered adaptive exitation           */
    Word16 y2[],      /* i   : Filtered fixed codebook excitation    */
    Word16 *mem_syn,  /* i/o : memory of synthesis filter            */
    Word16 *mem_err,  /* o   : pointer to error signal               */
    Word16 *mem_w0,   /* o   : memory of weighting filter            */
    Word16 *exc,      /* o   : long term prediction residual         */
    Word16 *sharp,    /* o   : pitch sharpening value                */
    Flag   *pOverflow /* o   : overflow indicator                    */
)
{
    Word16 i;
    Word16 j;
    Word16 temp;
    Word32 L_temp;
    Word32 L_temp2;
    Word16 tempShift;
    Word16 kShift;
    Word16 pitch_fac;
    Word16 *p_exc;
    Word16 *p_code;

    OSCL_UNUSED_ARG(pOverflow);

    if (mode != MR122)
    {
        tempShift = 1;
        kShift = 16 - 2 - 1;
        pitch_fac = gain_pit;
    }
    else
    {
        tempShift = 2;
        kShift = 16 - 4 - 1;
        pitch_fac = gain_pit >> 1;
    }

    /*------------------------------------------------------------*
     * - Update pitch sharpening "sharp" with quantized gain_pit  *
     *------------------------------------------------------------*/

    if (gain_pit < SHARPMAX)
    {
        *sharp = gain_pit;
    }
    else
    {
        *sharp = SHARPMAX;
    }

    /*------------------------------------------------------*
     * - Find the total excitation                          *
     * - find synthesis speech corresponding to exc[]       *
     * - update filters memories for finding the target     *
     *   vector in the next subframe                        *
     *   (update error[-m..-1] and mem_w_err[])             *
     *------------------------------------------------------*/

    p_exc  = &exc[ i_subfr];
    p_code = &code[0];

    for (i = L_SUBFR >> 1; i != 0 ; i--)
    {
        /* exc[i] = gain_pit*exc[i] + gain_code*code[i]; */

        /*
         *                      12k2  others
         * ---------------------------------
         * exc                   Q0      Q0
         * gain_pit              Q14     Q14
         * pitch_fac             Q13     Q14
         *    product:           Q14     Q15
         *
         * code                  Q12     Q13
         * gain_code             Q1      Q1
         *    product            Q14     Q15
         *    sum                Q14     Q15
         *
         * tempShift             2       1
         *    sum<<tempShift     Q16     Q16
         * result -> exc         Q0      Q0
         */
        L_temp     = ((Word32) * (p_exc++) * pitch_fac) << 1;
        L_temp2    = ((Word32) * (p_exc--) * pitch_fac) << 1;
        L_temp    += ((Word32) * (p_code++) * gain_code) << 1;
        L_temp2   += ((Word32) * (p_code++) * gain_code) << 1;
        L_temp   <<=  tempShift;
        L_temp2  <<=  tempShift;
        *(p_exc++) = (Word16)((L_temp  + 0x08000L) >> 16);
        *(p_exc++) = (Word16)((L_temp2 + 0x08000L) >> 16);

    }

    Syn_filt(
        Aq,
        &exc[i_subfr],
        &synth[i_subfr],
        L_SUBFR,
        mem_syn,
        1);

    for (i = L_SUBFR - M, j = 0; i < L_SUBFR; i++, j++)
    {
        mem_err[j] = speech[i_subfr + i] - synth[i_subfr + i];

        /*
         *                      12k2  others
         * ---------------------------------
         * y1                    Q0      Q0
         * gain_pit              Q14     Q14
         *    product            Q15     Q15
         *    shifted prod.      Q16     Q16
         * temp                  Q0      Q0
         *
         * y2                    Q10     Q12
         * gain_code             Q1      Q1
         *    product            Q12     Q14
         * kshift                 4       2
         *    shifted prod.      Q16     Q16
         * k                     Q0      Q0
         * mem_w0,xn,sum         Q0      Q0
         */

        L_temp = ((Word32)y1[i] * gain_pit);
        temp  = (Word16)(L_temp >> 14);

        L_temp = ((Word32)y2[i] * gain_code);
        temp += (Word16)(L_temp >> kShift);

        mem_w0[j] = xn[i] - temp;
    }

    return;
}
