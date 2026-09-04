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



 Filename: cbsearch.cpp
 Functions: D_plsf_3

 ------------------------------------------------------------------------------
 INPUT AND OUTPUT DEFINITIONS

 Inputs:
    x[] -- array of type Word16 -- target vector, Q0
    h[] -- array of type Word16 -- impulse response of weighted synthesis
                                   filter h[-L_subfr..-1] must be set to
                                   zero. Q12
    T0  -- Word16 -- Pitch lag
    pitch_sharp -- Word16 -- Last quantized pitch gain, Q14
    gain_pit --  Word16 gain_pit -- Pitch gain, Q14
    res2[] -- array of type Word16 -- Long term prediction residual, Q0
    mode -- enum Mode --  coder mode
    subNr -- Word16 -- subframe number

 Outputs:
    code[] -- array of type Word16 -- Innovative codebook, Q13
    y[] -- array of type Word16 -- filtered fixed codebook excitation
                                   Q12

    anap -- Double pointer to Word16 -- Signs of the pulses


    pOverflow -- pointer to Flag -- Flag set when overflow occurs

 Returns:
    Zero

 Global Variables Used:
    None

 Local Variables Needed:
    None

------------------------------------------------------------------------------
 FUNCTION DESCRIPTION

    Purpose          : Inovative codebook search (find index and gain)

------------------------------------------------------------------------------
 REQUIREMENTS



------------------------------------------------------------------------------
 REFERENCES

 cbsearch.c, UMTS GSM AMR speech codec, R99 - Version 3.2.0, March 2, 2001

------------------------------------------------------------------------------
 PSEUDO-CODE



------------------------------------------------------------------------------
*/


/*----------------------------------------------------------------------------
; INCLUDES
----------------------------------------------------------------------------*/
#include "cbsearch.h"

#include "typedef.h"
#include "c2_9pf.h"
#include "c2_11pf.h"
#include "c3_14pf.h"
#include "c4_17pf.h"
#include "c8_31pf.h"
#include "c1035pf.h"
#include "mode.h"
#include "basic_op.h"
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

void cbsearch(Word16 x[],        /* i : target vector, Q0                     */
              Word16 h[],        /* i : impulse response of weighted synthesis*/
              /*     filter h[-L_subfr..-1] must be set to */
              /*     zero. Q12                             */
              Word16 T0,         /* i : Pitch lag                             */
              Word16 pitch_sharp,/* i : Last quantized pitch gain, Q14        */
              Word16 gain_pit,   /* i : Pitch gain, Q14                       */
              Word16 res2[],     /* i : Long term prediction residual, Q0     */
              Word16 code[],     /* o : Innovative codebook, Q13              */
              Word16 y[],        /* o : filtered fixed codebook excitation    */
              /*     Q12                                   */
              Word16 **anap,     /* o : Signs of the pulses                   */
              enum Mode mode,    /* i : coder mode                            */
              Word16 subNr,      /* i : subframe number                       */
              CommonAmrTbls* common_amr_tbls, /* ptr to struct of tables    */
              Flag  *pOverflow)  /* o : Flag set when overflow occurs         */
{
    Word16 index;
    Word16 i;
    Word16 temp;
    Word16 pit_sharpTmp;

    /* For MR74, the pre and post CB pitch sharpening is included in the
     * codebook search routine, while for MR122 is it not.
     */

    if ((mode == MR475) || (mode == MR515))
    {
        /* MR475, MR515 */
        *(*anap)++ =
            code_2i40_9bits(
                subNr,
                x,
                h,
                T0,
                pitch_sharp,
                code,
                y,
                &index,
                common_amr_tbls->startPos_ptr,
                pOverflow);

        *(*anap)++ = index;    /* sign index */
    }
    else if (mode == MR59)
    {   /* MR59 */
        *(*anap)++ =
            code_2i40_11bits(
                x,
                h,
                T0,
                pitch_sharp,
                code,
                y,
                &index,
                pOverflow);

        *(*anap)++ = index;    /* sign index */
    }
    else if (mode == MR67)
    {   /* MR67 */
        *(*anap)++ =
            code_3i40_14bits(
                x,
                h,
                T0,
                pitch_sharp,
                code,
                y,
                &index,
                pOverflow);

        *(*anap)++ = index;    /* sign index */
    }
    else if ((mode == MR74) || (mode == MR795))
    {   /* MR74, MR795 */
        *(*anap)++ =
            code_4i40_17bits(
                x,
                h,
                T0,
                pitch_sharp,
                code,
                y,
                &index,
                common_amr_tbls->gray_ptr,
                pOverflow);

        *(*anap)++ = index;    /* sign index */
    }
    else if (mode == MR102)
    {   /* MR102 */
        /*-------------------------------------------------------------*
         * - include pitch contribution into impulse resp. h1[]        *
         *-------------------------------------------------------------*/
        /* pit_sharpTmp = pit_sharp;                     */
        /* if (pit_sharpTmp > 1.0) pit_sharpTmp = 1.0;   */

        pit_sharpTmp =
            shl(
                pitch_sharp,
                1,
                pOverflow);

        for (i = T0; i < L_SUBFR; i++)
        {
            temp =
                mult(
                    h[i - T0],
                    pit_sharpTmp,
                    pOverflow);

            h[i] =
                add_16(
                    h[i],
                    temp,
                    pOverflow);
        }

        /*--------------------------------------------------------------*
         * - Innovative codebook search (find index and gain)           *
         *--------------------------------------------------------------*/
        code_8i40_31bits(
            x,
            res2,
            h,
            code,
            y,
            *anap,
            pOverflow);

        *anap += 7;

        /*-------------------------------------------------------*
         * - Add the pitch contribution to code[].               *
         *-------------------------------------------------------*/
        for (i = T0; i < L_SUBFR; i++)
        {
            temp =
                mult(
                    code[i - T0],
                    pit_sharpTmp,
                    pOverflow);

            code[i] =
                add_16(
                    code[i],
                    temp,
                    pOverflow);
        }
    }
    else
    {  /* MR122 */
        /*-------------------------------------------------------------*
         * - include pitch contribution into impulse resp. h1[]        *
         *-------------------------------------------------------------*/

        /* pit_sharpTmp = gain_pit;                      */
        /* if (pit_sharpTmp > 1.0) pit_sharpTmp = 1.0;   */

        pit_sharpTmp = shl(gain_pit, 1, pOverflow);

        for (i = T0; i < L_SUBFR; i++)
        {
            temp = ((Word32)h[i - T0] * pit_sharpTmp) >> 15;
            /*
                     mult(
                            h[i - T0],
                            ,
                            pOverflow);
            */
            h[i] =
                add_16(
                    h[i],
                    temp,
                    pOverflow);
        }
        /*--------------------------------------------------------------*
         * - Innovative codebook search (find index and gain)           *
         *--------------------------------------------------------------*/

        code_10i40_35bits(
            x,
            res2,
            h,
            code,
            y,
            *anap,
            common_amr_tbls->gray_ptr,
            pOverflow);

        *anap += 10;

        /*-------------------------------------------------------*
         * - Add the pitch contribution to code[].               *
         *-------------------------------------------------------*/
        for (i = T0; i < L_SUBFR; i++)
        {
            temp =
                mult(
                    code[i - T0],
                    pit_sharpTmp,
                    pOverflow);

            code[i] =
                add_16(
                    code[i],
                    temp,
                    pOverflow);
        }
    }

}
