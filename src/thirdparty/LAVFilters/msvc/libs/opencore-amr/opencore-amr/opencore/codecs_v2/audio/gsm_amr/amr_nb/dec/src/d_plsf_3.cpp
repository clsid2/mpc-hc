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



 Filename: d_plsf_3.cpp
 Functions: D_plsf_3

 ------------------------------------------------------------------------------
 INPUT AND OUTPUT DEFINITIONS

 Inputs:
    st -- Pointer to type struct D_plsfState
    mode -- enum Mode -- coder mode
    bfi -- Word16 -- bad frame indicator (set to 1 if a bad frame is received)
    indice -- Pointer to type Word16 -- quantization indices of
                                        3 submatrices, Q0

 Outputs:
    st -- Pointer to type struct D_plsfState
    lsp1_q -- Pointer to type Word16 -- quantized 1st LSP vector Q15
    pOverflow -- Pointer to type Flag -- Flag set when overflow occurs

 Returns:
    None.

 Global Variables Used:
    None

 Local Variables Needed:
    None

------------------------------------------------------------------------------
 FUNCTION DESCRIPTION

 PURPOSE: Decodes the LSP parameters using the received quantization
          indices.1st order MA prediction and split by 3 vector
          quantization (split-VQ)

------------------------------------------------------------------------------
 REQUIREMENTS



------------------------------------------------------------------------------
 REFERENCES

 d_plsf_3.c, UMTS GSM AMR speech codec, R99 - Version 3.2.0, March 2, 2001

------------------------------------------------------------------------------
 PSEUDO-CODE



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
#include "oscl_mem.h"
#include "q_plsf_3_tbl.h"


/*----------------------------------------------------------------------------
; MACROS
; Define module specific macros here
----------------------------------------------------------------------------*/


/*----------------------------------------------------------------------------
; DEFINES
; Include all pre-processor statements here. Include conditional
; compile variables also.
----------------------------------------------------------------------------*/
#define ALPHA     29491     /* ALPHA    ->  0.9                            */
#define ONE_ALPHA 3277      /* ONE_ALPHA-> (1.0-ALPHA)                     */


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

void D_plsf_3(
    D_plsfState *st,   /* i/o: State struct                               */
    enum Mode mode,    /* i  : coder mode                                 */
    Word16 bfi,        /* i  : bad frame indicator (set to 1 if a         */
    /*      bad frame is received)                     */
    Word16 * indice,   /* i  : quantization indices of 3 submatrices, Q0  */
    CommonAmrTbls* common_amr_tbls, /* i : structure containing ptrs to read-only tables */
    Word16 * lsp1_q,   /* o  : quantized 1st LSP vector,              Q15 */
    Flag  *pOverflow   /* o : Flag set when overflow occurs               */
)
{
    Word16 i;
    Word16 temp;
    Word16 index;

    Word16 lsf1_r[M];
    Word16 lsf1_q[M];

    const Word16* mean_lsf_3_ptr = common_amr_tbls->mean_lsf_3_ptr;
    const Word16* pred_fac_3_ptr = common_amr_tbls->pred_fac_3_ptr;
    const Word16* dico1_lsf_3_ptr = common_amr_tbls->dico1_lsf_3_ptr;
    const Word16* dico2_lsf_3_ptr = common_amr_tbls->dico2_lsf_3_ptr;
    const Word16* dico3_lsf_3_ptr = common_amr_tbls->dico3_lsf_3_ptr;
    const Word16* mr515_3_lsf_ptr = common_amr_tbls->mr515_3_lsf_ptr;
    const Word16* mr795_1_lsf_ptr = common_amr_tbls->mr795_1_lsf_ptr;

    if (bfi != 0)   /* if bad frame */
    {
        /* use the past LSFs slightly shifted towards their mean */

        for (i = 0; i < M; i++)
        {
            /* lsfi_q[i] = ALPHA*past_lsf_q[i] + ONE_ALPHA*mean_lsf[i]; */
            temp =
                mult(
                    st->past_lsf_q[i],
                    ALPHA,
                    pOverflow);

            index =
                mult(
                    mean_lsf_3_ptr[i],
                    ONE_ALPHA,
                    pOverflow);

            lsf1_q[i] =
                add_16(
                    index,
                    temp,
                    pOverflow);
        }

        /* estimate past quantized residual to be used in next frame */
        if (mode != MRDTX)
        {
            for (i = 0; i < M; i++)
            {
                /* temp  = mean_lsf[i] +  past_r2_q[i] * PRED_FAC; */

                temp =
                    mult(
                        st->past_r_q[i],
                        pred_fac_3_ptr[i],
                        pOverflow);

                temp =
                    add_16(
                        mean_lsf_3_ptr[i],
                        temp,
                        pOverflow);

                st->past_r_q[i] =
                    sub(
                        lsf1_q[i],
                        temp,
                        pOverflow);
            }

        } /* if (mode == MRDTX) */
        else
        {
            for (i = 0; i < M; i++)
            {
                /* temp  = mean_lsf[i] +  past_r2_q[i]; */

                temp =
                    add_16(
                        mean_lsf_3_ptr[i],
                        st->past_r_q[i],
                        pOverflow);

                st->past_r_q[i] =
                    sub(
                        lsf1_q[i],
                        temp,
                        pOverflow);
            }
        }

    } /* if (bfi != 0) */

    else  /* if good LSFs received */
    {

        Word16 index_limit_1 = 0;
        Word16 index_limit_2 = (DICO2_SIZE - 1) * 3;
        Word16 index_limit_3 = 0;

        const Word16 *p_cb1;
        const Word16 *p_cb2;
        const Word16 *p_cb3;
        const Word16 *p_dico;


        p_cb2 = dico2_lsf_3_ptr;    /* size DICO2_SIZE*3 */

        if ((mode == MR475) || (mode == MR515))
        {   /* MR475, MR515 */
            p_cb1 = dico1_lsf_3_ptr;    /* size DICO1_SIZE*3 */
            p_cb3 = mr515_3_lsf_ptr;    /* size MR515_3_SIZE*4 */

            index_limit_1 = (DICO1_SIZE - 1) * 3;
            index_limit_3 = (MR515_3_SIZE - 1) * 4;

        }
        else if (mode == MR795)
        {   /* MR795 */
            p_cb1 = mr795_1_lsf_ptr;    /* size MR795_1_SIZE*3 */
            p_cb3 = dico3_lsf_3_ptr;    /* size DICO3_SIZE*4 */

            index_limit_1 = (MR795_1_SIZE - 1) * 3;
            index_limit_3 = (DICO3_SIZE - 1) * 4;

        }
        else
        {   /* MR59, MR67, MR74, MR102, MRDTX */
            p_cb1 = dico1_lsf_3_ptr;    /* size DICO1_SIZE*3 */
            p_cb3 = dico3_lsf_3_ptr;    /* size DICO3_SIZE*4 */

            index_limit_1 = (DICO1_SIZE - 1) * 3;
            index_limit_3 = (DICO3_SIZE - 1) * 4;

        }

        /* decode prediction residuals from 3 received indices */

        index = *indice++;

        /* temp = 3*index; */
        temp = index + (index << 1);

        if (temp > index_limit_1)
        {
            temp = index_limit_1;  /* avoid buffer overrun */
        }

        p_dico = &p_cb1[temp];

        lsf1_r[0] = *p_dico++;
        lsf1_r[1] = *p_dico++;
        lsf1_r[2] = *p_dico++;

        index = *indice++;

        if (mode == MR475 || mode == MR515)
        {   /* MR475, MR515 only using every second entry */
            index <<= 1;
        }

        /* temp = 3*index */
        temp = index + (index << 1);

        if (temp > index_limit_2)
        {
            temp = index_limit_2;  /* avoid buffer overrun */
        }

        p_dico = &p_cb2[temp];

        lsf1_r[3] = *p_dico++;
        lsf1_r[4] = *p_dico++;
        lsf1_r[5] = *p_dico++;

        index = *indice++;

        temp = index << 2;

        if (temp > index_limit_3)
        {
            temp = index_limit_3;  /* avoid buffer overrun */
        }


        p_dico = &p_cb3[temp];

        lsf1_r[6] = *p_dico++;
        lsf1_r[7] = *p_dico++;
        lsf1_r[8] = *p_dico++;
        lsf1_r[9] = *p_dico++;

        /* Compute quantized LSFs and update the past quantized residual */

        if (mode != MRDTX)
        {
            for (i = 0; i < M; i++)
            {
                temp =
                    mult(
                        st->past_r_q[i],
                        pred_fac_3_ptr[i],
                        pOverflow);

                temp =
                    add_16(
                        mean_lsf_3_ptr[i],
                        temp,
                        pOverflow);

                lsf1_q[i] =
                    add_16(
                        lsf1_r[i],
                        temp,
                        pOverflow);

                st->past_r_q[i] = lsf1_r[i];
            }
        }
        else
        {
            for (i = 0; i < M; i++)
            {
                temp =
                    add_16(
                        mean_lsf_3_ptr[i],
                        st->past_r_q[i],
                        pOverflow);

                lsf1_q[i] =
                    add_16(
                        lsf1_r[i],
                        temp,
                        pOverflow);

                st->past_r_q[i] = lsf1_r[i];
            }
        }

    }

    /* verification that LSFs has minimum distance of LSF_GAP Hz */

    Reorder_lsf(
        lsf1_q,
        LSF_GAP,
        M,
        pOverflow);

    oscl_memmove(
        (void *)st->past_lsf_q,
        lsf1_q,
        M*sizeof(*lsf1_q));

    /*  convert LSFs to the cosine domain */

    Lsf_lsp(
        lsf1_q,
        lsp1_q,
        M,
        pOverflow);

    return;
}

/*
------------------------------------------------------------------------------
 FUNCTION NAME: Init_D_plsf_3
------------------------------------------------------------------------------
 INPUT AND OUTPUT DEFINITIONS

 Inputs:
    st = pointer to a structure of type D_plsfState
    index = Word16, past_rq_init[] index [0, 7]

 Outputs:
    st = pointer to a structure of type D_plsfState

 Returns:
    None

 Global Variables Used:
    None.

 Local Variables Needed:
    None.

------------------------------------------------------------------------------
 FUNCTION DESCRIPTION

 This function initializes the D_plsfState structure.

------------------------------------------------------------------------------
 REQUIREMENTS

 None.

------------------------------------------------------------------------------
 REFERENCES

 d_plsf_3.c, UMTS GSM AMR speech codec, R99 - Version 3.2.0, March 2, 2001

------------------------------------------------------------------------------
 PSEUDO-CODE

------------------------------------------------------------------------------
 CAUTION [optional]
 [State any special notes, constraints or cautions for users of this function]

------------------------------------------------------------------------------
*/
void Init_D_plsf_3(
    D_plsfState *st,      /* i/o: State struct                */
    Word16       index,   /* i  : past_rq_init[] index [0, 7] */
    const Word16* past_rq_init_ptr /* ptr to read-only table */)
{
    oscl_memmove(
        (void *)st->past_r_q,
        &past_rq_init_ptr[index * M],
        M*sizeof(*past_rq_init_ptr));
}
