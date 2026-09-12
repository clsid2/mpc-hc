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



 Filename: d_plsf_5.cpp

------------------------------------------------------------------------------
*/

/*----------------------------------------------------------------------------
; INCLUDES
----------------------------------------------------------------------------*/
#include "d_plsf.h"
#include "typedef.h"
#include "basic_op.h"
#include "lsp_lsf.h"
#include "reorder.h"
#include "cnst.h"
#include "oscl_mem.h"

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
    ; Include all pre-processor statements here. Include conditional
    ; compile variables also.
    ----------------------------------------------------------------------------*/
    /* ALPHA    ->  0.95       */
    /* ONE_ALPHA-> (1.0-ALPHA) */
#define ALPHA     31128
#define ONE_ALPHA 1639

    /*----------------------------------------------------------------------------
    ; LOCAL FUNCTION DEFINITIONS
    ; Function Prototype declaration
    ----------------------------------------------------------------------------*/

    /*----------------------------------------------------------------------------
    ; LOCAL STORE/BUFFER/POINTER DEFINITIONS
    ; Variable declaration - defined here and used outside this module
    ----------------------------------------------------------------------------*/

    /*--------------------------------------------------------------------------*/
#ifdef __cplusplus
}
#endif

/*
------------------------------------------------------------------------------
 FUNCTION NAME: D_plsf_5
------------------------------------------------------------------------------
 INPUT AND OUTPUT DEFINITIONS

 Inputs:
    st = pointer to a structure of type D_plsfState
    bfi = bad frame indicator; set to 1 if a bad frame is received (Word16)
    indice = pointer to quantization indices of 5 submatrices (Word16)
    lsp1_q = pointer to the quantized 1st LSP vector (Word16)
    lsp2_q = pointer to the quantized 2nd LSP vector (Word16)

 Outputs:
    lsp1_q points to the updated quantized 1st LSP vector
    lsp2_q points to the updated quantized 2nd LSP vector
    Flag  *pOverflow  -- Flag set when overflow occurs.

 Returns:
    return_value = 0 (int)

 Global Variables Used:
    None.

 Local Variables Needed:
    None.

------------------------------------------------------------------------------
 FUNCTION DESCRIPTION

 This function decodes the 2 sets of LSP parameters in a frame using the
 received quantization indices.

------------------------------------------------------------------------------
 REQUIREMENTS

 None.

------------------------------------------------------------------------------
 REFERENCES

 d_plsf_5.c, UMTS GSM AMR speech codec, R99 - Version 3.2.0, March 2, 2001

------------------------------------------------------------------------------
 PSEUDO-CODE

int D_plsf_5 (
    D_plsfState *st,    // i/o: State variables
    Word16 bfi,         // i  : bad frame indicator (set to 1 if a bad
                                frame is received)
    Word16 *indice,     // i  : quantization indices of 5 submatrices, Q0
    Word16 *lsp1_q,     // o  : quantized 1st LSP vector (M),          Q15
    Word16 *lsp2_q      // o  : quantized 2nd LSP vector (M),          Q15
)
{
    Word16 i;
    const Word16 *p_dico;
    Word16 temp, sign;
    Word16 lsf1_r[M], lsf2_r[M];
    Word16 lsf1_q[M], lsf2_q[M];

    if (bfi != 0)                               // if bad frame
    {
        // use the past LSFs slightly shifted towards their mean

        for (i = 0; i < M; i++)
        {
            // lsfi_q[i] = ALPHA*st->past_lsf_q[i] + ONE_ALPHA*mean_lsf[i];

            lsf1_q[i] = add (mult (st->past_lsf_q[i], ALPHA),
                             mult (mean_lsf[i], ONE_ALPHA));

            lsf2_q[i] = lsf1_q[i];
        }

        // estimate past quantized residual to be used in next frame

        for (i = 0; i < M; i++)
        {
            // temp  = mean_lsf[i] +  st->past_r_q[i] * LSP_PRED_FAC_MR122;

            temp = add (mean_lsf[i], mult (st->past_r_q[i],
                                           LSP_PRED_FAC_MR122));

            st->past_r_q[i] = sub (lsf2_q[i], temp);
        }
    }
    else
        // if good LSFs received
    {
        // decode prediction residuals from 5 received indices

        p_dico = &dico1_lsf[shl (indice[0], 2)];
        lsf1_r[0] = *p_dico++;
        lsf1_r[1] = *p_dico++;
        lsf2_r[0] = *p_dico++;
        lsf2_r[1] = *p_dico++;

        p_dico = &dico2_lsf[shl (indice[1], 2)];
        lsf1_r[2] = *p_dico++;
        lsf1_r[3] = *p_dico++;
        lsf2_r[2] = *p_dico++;
        lsf2_r[3] = *p_dico++;

        sign = indice[2] & 1;
        i = shr (indice[2], 1);
        p_dico = &dico3_lsf[shl (i, 2)];

        if (sign == 0)
        {
            lsf1_r[4] = *p_dico++;
            lsf1_r[5] = *p_dico++;
            lsf2_r[4] = *p_dico++;
            lsf2_r[5] = *p_dico++;
        }
        else
        {
            lsf1_r[4] = negate (*p_dico++);
            lsf1_r[5] = negate (*p_dico++);
            lsf2_r[4] = negate (*p_dico++);
            lsf2_r[5] = negate (*p_dico++);
        }

        p_dico = &dico4_lsf[shl (indice[3], 2)];
        lsf1_r[6] = *p_dico++;
        lsf1_r[7] = *p_dico++;
        lsf2_r[6] = *p_dico++;
        lsf2_r[7] = *p_dico++;

        p_dico = &dico5_lsf[shl (indice[4], 2)];
        lsf1_r[8] = *p_dico++;
        lsf1_r[9] = *p_dico++;
        lsf2_r[8] = *p_dico++;
        lsf2_r[9] = *p_dico++;

        // Compute quantized LSFs and update the past quantized residual
        for (i = 0; i < M; i++)
        {
            temp = add (mean_lsf[i], mult (st->past_r_q[i],
                                           LSP_PRED_FAC_MR122));
            lsf1_q[i] = add (lsf1_r[i], temp);
            lsf2_q[i] = add (lsf2_r[i], temp);
            st->past_r_q[i] = lsf2_r[i];
        }
    }

    // verification that LSFs have minimum distance of LSF_GAP Hz

    Reorder_lsf (lsf1_q, LSF_GAP, M);
    Reorder_lsf (lsf2_q, LSF_GAP, M);

    Copy (lsf2_q, st->past_lsf_q, M);

    //  convert LSFs to the cosine domain

    Lsf_lsp (lsf1_q, lsp1_q, M);
    Lsf_lsp (lsf2_q, lsp2_q, M);

    return 0;
}

------------------------------------------------------------------------------
 CAUTION [optional]
 [State any special notes, constraints or cautions for users of this function]

------------------------------------------------------------------------------
*/

void D_plsf_5(
    D_plsfState *st,    /* i/o: State variables                             */
    Word16 bfi,         /* i  : bad frame indicator (set to 1 if a bad
                                frame is received)                          */
    Word16 *indice,     /* i  : quantization indices of 5 submatrices, Q0   */
    CommonAmrTbls* common_amr_tbls, /* i : structure containing ptrs to read-only tables */
    Word16 *lsp1_q,     /* o  : quantized 1st LSP vector (M),          Q15  */
    Word16 *lsp2_q,     /* o  : quantized 2nd LSP vector (M),          Q15  */
    Flag  *pOverflow    /* o : Flag set when overflow occurs                */
)
{
    register Word16 i;
    Word16 temp;
    Word16 sign;

    const Word16 *p_dico;

    Word16 lsf1_r[M];
    Word16 lsf2_r[M];
    Word16 lsf1_q[M];
    Word16 lsf2_q[M];

    /* These tables are defined in q_plsf_5_tbl.c */
    const Word16* mean_lsf_5_ptr = common_amr_tbls->mean_lsf_5_ptr;
    const Word16* dico1_lsf_5_ptr = common_amr_tbls->dico1_lsf_5_ptr;
    const Word16* dico2_lsf_5_ptr = common_amr_tbls->dico2_lsf_5_ptr;
    const Word16* dico3_lsf_5_ptr = common_amr_tbls->dico3_lsf_5_ptr;
    const Word16* dico4_lsf_5_ptr = common_amr_tbls->dico4_lsf_5_ptr;
    const Word16* dico5_lsf_5_ptr = common_amr_tbls->dico5_lsf_5_ptr;

    if (bfi != 0)                               /* if bad frame */
    {
        /* use the past LSFs slightly shifted towards their mean */

        for (i = 0; i < M; i++)
        {
            /*
             *  lsfi_q[i] = ALPHA*st->past_lsf_q[i] +
             *  ONE_ALPHA*mean_lsf[i];
             */

            temp = (Word16)(((Word32)  st->past_lsf_q[i] * ALPHA) >> 15);

            sign = (Word16)(((Word32)  * (mean_lsf_5_ptr + i) * ONE_ALPHA) >> 15);

            *(lsf1_q + i) = add_16(sign, temp,  pOverflow);

            *(lsf2_q + i) = *(lsf1_q + i);

            /*
             * estimate past quantized residual to be used in
             * next frame
             */

            /*
             * temp  = mean_lsf[i] +
             * st->past_r_q[i] * LSP_PRED_FAC_MR122;
             */

            temp = (Word16)(((Word32)  st->past_r_q[i] * LSP_PRED_FAC_MR122) >> 15);

            temp = add_16(*(mean_lsf_5_ptr + i), temp, pOverflow);

            st->past_r_q[i] = sub(*(lsf2_q + i), temp, pOverflow);
        }
    }
    else
        /* if good LSFs received */
    {
        /* decode prediction residuals from 5 received indices */

        temp =
            shl(
                *(indice),
                2,
                pOverflow);

        p_dico = &dico1_lsf_5_ptr[temp];

        *(lsf1_r + 0) = *p_dico++;
        *(lsf1_r + 1) = *p_dico++;
        *(lsf2_r + 0) = *p_dico++;
        *(lsf2_r + 1) = *p_dico++;

        temp = shl(*(indice + 1), 2, pOverflow);

        p_dico = &dico2_lsf_5_ptr[temp];

        *(lsf1_r + 2) = *p_dico++;
        *(lsf1_r + 3) = *p_dico++;
        *(lsf2_r + 2) = *p_dico++;
        *(lsf2_r + 3) = *p_dico++;

        sign = *(indice + 2) & 1;

        if (*(indice + 2) < 0)
        {
            i = ~(~(*(indice + 2)) >> 1);
        }
        else
        {
            i = *(indice + 2) >> 1;
        }

        temp = shl(i, 2, pOverflow);

        p_dico = &dico3_lsf_5_ptr[temp];

        if (sign == 0)
        {
            *(lsf1_r + 4) = *p_dico++;
            *(lsf1_r + 5) = *p_dico++;
            *(lsf2_r + 4) = *p_dico++;
            *(lsf2_r + 5) = *p_dico++;
        }
        else
        {
            *(lsf1_r + 4) = negate(*p_dico++);
            *(lsf1_r + 5) = negate(*p_dico++);
            *(lsf2_r + 4) = negate(*p_dico++);
            *(lsf2_r + 5) = negate(*p_dico++);
        }

        temp = shl(*(indice + 3), 2, pOverflow);

        p_dico = &dico4_lsf_5_ptr[temp];

        *(lsf1_r + 6) = *p_dico++;
        *(lsf1_r + 7) = *p_dico++;
        *(lsf2_r + 6) = *p_dico++;
        *(lsf2_r + 7) = *p_dico++;

        temp = shl(*(indice + 4), 2, pOverflow);

        p_dico = &dico5_lsf_5_ptr[temp];

        *(lsf1_r + 8) = *p_dico++;
        *(lsf1_r + 9) = *p_dico++;
        *(lsf2_r + 8) = *p_dico++;
        *(lsf2_r + 9) = *p_dico++;

        /* Compute quantized LSFs and update the past quantized
        residual */
        for (i = 0; i < M; i++)
        {
            temp =
                mult(
                    st->past_r_q[i],
                    LSP_PRED_FAC_MR122,
                    pOverflow);

            temp =
                add_16(
                    *(mean_lsf_5_ptr + i),
                    temp,
                    pOverflow);

            *(lsf1_q + i) =
                add_16(
                    *(lsf1_r + i),
                    temp,
                    pOverflow);

            *(lsf2_q + i) =
                add_16(
                    *(lsf2_r + i),
                    temp,
                    pOverflow);

            st->past_r_q[i] = *(lsf2_r + i);
        }
    }

    /* verification that LSFs have minimum distance of LSF_GAP Hz */

    Reorder_lsf(
        lsf1_q,
        LSF_GAP,
        M,
        pOverflow);

    Reorder_lsf(
        lsf2_q,
        LSF_GAP,
        M,
        pOverflow);

    oscl_memmove((void *)st->past_lsf_q, lsf2_q, M*sizeof(*lsf2_q));

    /*  convert LSFs to the cosine domain */

    Lsf_lsp(
        lsf1_q,
        lsp1_q,
        M,
        pOverflow);

    Lsf_lsp(
        lsf2_q,
        lsp2_q,
        M,
        pOverflow);

    return;
}
