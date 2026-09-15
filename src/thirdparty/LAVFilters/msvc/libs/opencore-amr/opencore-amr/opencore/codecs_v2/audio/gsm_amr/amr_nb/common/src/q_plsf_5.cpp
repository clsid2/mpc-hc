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
 Filename: q_plsf_5.cpp

------------------------------------------------------------------------------
 MODULE DESCRIPTION


------------------------------------------------------------------------------
*/

/*----------------------------------------------------------------------------
; INCLUDES
----------------------------------------------------------------------------*/
#include "q_plsf.h"
#include "typedef.h"
#include "basic_op.h"
#include "lsp_lsf.h"
#include "reorder.h"
#include "lsfwt.h"

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

    /*----------------------------------------------------------------------------
    ; LOCAL FUNCTION DEFINITIONS
    ; Function Prototype declaration
    ----------------------------------------------------------------------------*/

    /*----------------------------------------------------------------------------
    ; LOCAL VARIABLE DEFINITIONS
    ; Variable declaration - defined here and used outside this module
    ----------------------------------------------------------------------------*/

    /*----------------------------------------------------------------------------
    ; EXTERNAL GLOBAL STORE/BUFFER/POINTER REFERENCES
    ; Declare variables used in this module but defined elsewhere
    ----------------------------------------------------------------------------*/
    /* Codebooks of LSF prediction residual */
    extern const Word16 mean_lsf_5[];

    extern const Word16 dico1_lsf_5[];
    extern const Word16 dico2_lsf_5[];
    extern const Word16 dico3_lsf_5[];
    extern const Word16 dico4_lsf_5[];
    extern const Word16 dico5_lsf_5[];

    /*--------------------------------------------------------------------------*/
#ifdef __cplusplus
}
#endif

/*
------------------------------------------------------------------------------
 FUNCTION NAME: Vq_subvec
------------------------------------------------------------------------------
 INPUT AND OUTPUT DEFINITIONS

 Inputs:
    lsf_r1 -- array of type Word16 -- 1st LSF residual vector,  Q15
    lsf_r2 -- array of type Word16 -- 2nd LSF residual vector,  Q15
    dico -- pointer to const Word16 -- quantization codebook,   Q15
    wf1 -- array of type Word16 -- 1st LSF weighting factors,   Q13
    wf2 -- array of type Word16 -- 2nd LSF weighting factors,   Q13
    dico_size -- Word16 -- size of quantization codebook,       Q0
 Outputs:
    pOverflow -- pointer to type Flag -- overflow indicator

 Returns:
    Word16 -- quantization index, Q0

 Global Variables Used:
    None

 Local Variables Needed:
    None

------------------------------------------------------------------------------
 FUNCTION DESCRIPTION

 This function performs the quantization of a 4-dimensional subvector.

------------------------------------------------------------------------------
 REQUIREMENTS

 None

------------------------------------------------------------------------------
 REFERENCES

 q_plsf_5.c, UMTS GSM AMR speech codec, R99 - Version 3.2.0, March 2, 2001

------------------------------------------------------------------------------
 PSEUDO-CODE


------------------------------------------------------------------------------
 CAUTION [optional]
 [State any special notes, constraints or cautions for users of this function]

------------------------------------------------------------------------------
*/
/* Quantization of a 4 dimensional subvector */

static Word16 Vq_subvec( /* o : quantization index,            Q0  */
    Word16 *lsf_r1,      /* i : 1st LSF residual vector,       Q15 */
    Word16 *lsf_r2,      /* i : 2nd LSF residual vector,       Q15 */
    const Word16 *dico,  /* i : quantization codebook,         Q15 */
    Word16 *wf1,         /* i : 1st LSF weighting factors      Q13 */
    Word16 *wf2,         /* i : 2nd LSF weighting factors      Q13 */
    Word16 dico_size,    /* i : size of quantization codebook, Q0  */
    Flag   *pOverflow    /* o : overflow indicator                 */
)
{
    Word16 index = 0; /* initialization only needed to keep gcc silent */
    Word16 i;
    Word16 temp;
    const Word16 *p_dico;
    Word32 dist_min;
    Word32 dist;
    Word16 wf1_0;
    Word16 wf1_1;
    Word16 wf2_0;
    Word16 wf2_1;
    Word32 aux1;
    Word32 aux2;
    Word32 aux3;
    Word32 aux4;

    OSCL_UNUSED_ARG(pOverflow);

    dist_min = MAX_32;
    p_dico = dico;

    wf1_0 = wf1[0];
    wf1_1 = wf1[1];
    wf2_0 = wf2[0];
    wf2_1 = wf2[1];

    aux1 = ((Word32) lsf_r1[0] * wf1_0);
    aux2 = ((Word32) lsf_r1[1] * wf1_1);
    aux3 = ((Word32) lsf_r2[0] * wf2_0);
    aux4 = ((Word32) lsf_r2[1] * wf2_1);

    for (i = 0; i < dico_size; i++)
    {
        temp  = (aux1 - ((Word32)wf1_0 * *(p_dico++))) >> 15;
        dist  = ((Word32)temp * temp);

        if (dist >= dist_min)
        {
            p_dico += 3;
            continue;
        }

        temp  = (aux2 - ((Word32)wf1_1 * *(p_dico++))) >> 15;
        dist += ((Word32)temp * temp);

        if (dist >= dist_min)
        {
            p_dico += 2;
            continue;
        }

        temp  = (aux3 - ((Word32)wf2_0 * *(p_dico++))) >> 15;
        dist += ((Word32)temp * temp);

        if (dist >= dist_min)
        {
            p_dico += 1;
            continue;
        }

        temp  = (aux4 - ((Word32)wf2_1 * *(p_dico++))) >> 15;
        dist += ((Word32)temp * temp);


        if (dist < dist_min)
        {
            dist_min = dist;
            index = i;
        }
    }



    /* Reading the selected vector */

    p_dico = &dico[ index<<2];
    lsf_r1[0] = *p_dico++;
    lsf_r1[1] = *p_dico++;
    lsf_r2[0] = *p_dico++;
    lsf_r2[1] = *p_dico;

    return index;

}


/*
------------------------------------------------------------------------------
 FUNCTION NAME: Vq_subvec_s
------------------------------------------------------------------------------
 INPUT AND OUTPUT DEFINITIONS

 Inputs:
    lsf_r1 -- array of type Word16 -- 1st LSF residual vector,  Q15
    lsf_r2 -- array of type Word16 -- 2nd LSF residual vector,  Q15
    dico -- pointer to const Word16 -- quantization codebook,   Q15
    wf1 -- array of type Word16 -- 1st LSF weighting factors,   Q13
    wf2 -- array of type Word16 -- 2nd LSF weighting factors,   Q13
    dico_size -- Word16 -- size of quantization codebook,       Q0

 Outputs:
    pOverflow -- pointer to type Flag -- overflow indicator

 Returns:
    Word16 -- quantization index, Q0

 Global Variables Used:
    None

 Local Variables Needed:
    None

------------------------------------------------------------------------------
 FUNCTION DESCRIPTION

 This function performs the quantization of a 4-dimensional subvector.

------------------------------------------------------------------------------
 REQUIREMENTS

 None

------------------------------------------------------------------------------
 REFERENCES

 q_plsf_5.c, UMTS GSM AMR speech codec, R99 - Version 3.2.0, March 2, 2001

------------------------------------------------------------------------------
 PSEUDO-CODE


------------------------------------------------------------------------------
 CAUTION [optional]
 [State any special notes, constraints or cautions for users of this function]

------------------------------------------------------------------------------
*/


/* Quantization of a 4 dimensional subvector with a signed codebook */

static Word16 Vq_subvec_s(  /* o : quantization index            Q0  */
    Word16 *lsf_r1,         /* i : 1st LSF residual vector       Q15 */
    Word16 *lsf_r2,         /* i : and LSF residual vector       Q15 */
    const Word16 *dico,     /* i : quantization codebook         Q15 */
    Word16 *wf1,            /* i : 1st LSF weighting factors     Q13 */
    Word16 *wf2,            /* i : 2nd LSF weighting factors     Q13 */
    Word16 dico_size,       /* i : size of quantization codebook Q0  */
    Flag   *pOverflow)      /* o : overflow indicator                */
{
    Word16 index = 0;  /* initialization only needed to keep gcc silent */
    Word16 sign = 0;   /* initialization only needed to keep gcc silent */
    Word16 i;
    Word16 temp;
    Word16 temp1;
    Word16 temp2;
    const Word16 *p_dico;
    Word32 dist_min;
    Word32 dist1;
    Word32 dist2;

    Word16 lsf_r1_0;
    Word16 lsf_r1_1;
    Word16 lsf_r2_0;
    Word16 lsf_r2_1;

    Word16 wf1_0;
    Word16 wf1_1;
    Word16 wf2_0;
    Word16 wf2_1;

    OSCL_UNUSED_ARG(pOverflow);

    dist_min = MAX_32;
    p_dico = dico;


    lsf_r1_0 = lsf_r1[0];
    lsf_r1_1 = lsf_r1[1];
    lsf_r2_0 = lsf_r2[0];
    lsf_r2_1 = lsf_r2[1];

    wf1_0 = wf1[0];
    wf1_1 = wf1[1];
    wf2_0 = wf2[0];
    wf2_1 = wf2[1];

    for (i = 0; i < dico_size; i++)
    {
        /* test positive */
        temp = *p_dico++;
        temp1 = lsf_r1_0 - temp;
        temp2 = lsf_r1_0 + temp;
        temp1 = ((Word32)wf1_0 * temp1) >> 15;
        temp2 = ((Word32)wf1_0 * temp2) >> 15;
        dist1 = ((Word32)temp1 * temp1);
        dist2 = ((Word32)temp2 * temp2);

        temp = *p_dico++;
        temp1 = lsf_r1_1 - temp;
        temp2 = lsf_r1_1 + temp;
        temp1 = ((Word32)wf1_1 * temp1) >> 15;
        temp2 = ((Word32)wf1_1 * temp2) >> 15;
        dist1 += ((Word32)temp1 * temp1);
        dist2 += ((Word32)temp2 * temp2);

        if ((dist1 >= dist_min) && (dist2 >= dist_min))
        {
            p_dico += 2;
            continue;
        }

        temp = *p_dico++;
        temp1 = lsf_r2_0 - temp;
        temp2 = lsf_r2_0 + temp;
        temp1 = ((Word32)wf2_0 * temp1) >> 15;
        temp2 = ((Word32)wf2_0 * temp2) >> 15;
        dist1 += ((Word32)temp1 * temp1);
        dist2 += ((Word32)temp2 * temp2);

        temp = *p_dico++;
        temp1 = lsf_r2_1 - temp;
        temp2 = lsf_r2_1 + temp;
        temp1 = ((Word32)wf2_1 * temp1) >> 15;
        temp2 = ((Word32)wf2_1 * temp2) >> 15;
        dist1 += ((Word32)temp1 * temp1);
        dist2 += ((Word32)temp2 * temp2);

        if (dist1 < dist_min)
        {
            dist_min = dist1;
            index = i;
            sign = 0;
        }

        /* test negative */

        if (dist2 < dist_min)
        {
            dist_min = dist2;
            index = i;
            sign = 1;
        }
    }

    /* Reading the selected vector */

    p_dico = &dico[index<<2];
    index <<= 1;
    if (sign)
    {
        lsf_r1[0] = - (*p_dico++);
        lsf_r1[1] = - (*p_dico++);
        lsf_r2[0] = - (*p_dico++);
        lsf_r2[1] = - (*p_dico);
        index +=  1;
    }
    else
    {
        lsf_r1[0] = *p_dico++;
        lsf_r1[1] = *p_dico++;
        lsf_r2[0] = *p_dico++;
        lsf_r2[1] = *p_dico;
    }

    return index;

}

/*
------------------------------------------------------------------------------
 FUNCTION NAME: Q_plsf_5
------------------------------------------------------------------------------
 INPUT AND OUTPUT DEFINITIONS


 Inputs:
    st   -- pointer to Q_plsfState -- state information
    lsp1 -- array of type Word16 -- 1st LSP vector,  Q15
    lsp2 -- array of type Word16 -- 2nd LSP vector,  Q15

 Outputs:
    lps1_q -- array of type Word16 -- quantized 1st LSP vector,   Q15
    lps2_q -- array of type Word16 -- quantized 2nd LSP vector,   Q15
    indices -- array of type Word16 -- quantization indices of 5 matrics, Q0
    pOverflow -- pointer to type Flag -- overflow indicator

 Returns:
    None

 Global Variables Used:
    mean_lsf_5[];

    dico1_lsf_5[];
    dico2_lsf_5[];
    dico3_lsf_5[];
    dico4_lsf_5[];
    dico5_lsf_5[];

 Local Variables Needed:
    None

------------------------------------------------------------------------------
 FUNCTION DESCRIPTION

 PURPOSE:  Quantization of 2 sets of LSF parameters using 1st order MA
           prediction and split by 5 matrix quantization (split-MQ)

 DESCRIPTION:

      p[i] = pred_factor*past_rq[i];   i=0,...,m-1
      r1[i]= lsf1[i] - p[i];           i=0,...,m-1
      r2[i]= lsf2[i] - p[i];           i=0,...,m-1
 where:
      lsf1[i]           1st mean-removed LSF vector.
      lsf2[i]           2nd mean-removed LSF vector.
      r1[i]             1st residual prediction vector.
      r2[i]             2nd residual prediction vector.
      past_r2q[i]       Past quantized residual (2nd vector).

 The residual vectors r1[i] and r2[i] are jointly quantized using
 split-MQ with 5 codebooks. Each 4th dimension submatrix contains 2
 elements from each residual vector. The 5 submatrices are as follows:
   {r1[0], r1[1], r2[0], r2[1]};  {r1[2], r1[3], r2[2], r2[3]};
   {r1[4], r1[5], r2[4], r2[5]};  {r1[6], r1[7], r2[6], r2[7]};
                  {r1[8], r1[9], r2[8], r2[9]}

------------------------------------------------------------------------------
 REQUIREMENTS

 None

------------------------------------------------------------------------------
 REFERENCES

 q_plsf_5.c, UMTS GSM AMR speech codec, R99 - Version 3.2.0, March 2, 2001

------------------------------------------------------------------------------
 PSEUDO-CODE


------------------------------------------------------------------------------
 CAUTION [optional]
 [State any special notes, constraints or cautions for users of this function]

------------------------------------------------------------------------------
*/

OSCL_EXPORT_REF void Q_plsf_5(
    Q_plsfState *st,
    Word16 *lsp1,      /* i : 1st LSP vector,                     Q15 */
    Word16 *lsp2,      /* i : 2nd LSP vector,                     Q15 */
    Word16 *lsp1_q,    /* o : quantized 1st LSP vector,           Q15 */
    Word16 *lsp2_q,    /* o : quantized 2nd LSP vector,           Q15 */
    Word16 *indice,    /* o : quantization indices of 5 matrices, Q0  */
    Flag   *pOverflow  /* o : overflow indicator                      */
)
{
    Word16 i;
    Word16 lsf1[M];
    Word16 lsf2[M];
    Word16 wf1[M];
    Word16 wf2[M];
    Word16 lsf_p[M];
    Word16 lsf_r1[M];
    Word16 lsf_r2[M];
    Word16 lsf1_q[M];
    Word16 lsf2_q[M];

    Word16 *p_lsf_p;
    Word16 *p_lsf1;
    Word16 *p_lsf2;
    Word16 *p_lsf_r1;
    Word16 *p_lsf_r2;

    /* convert LSFs to normalize frequency domain 0..16384  */

    Lsp_lsf(lsp1, lsf1, M, pOverflow);
    Lsp_lsf(lsp2, lsf2, M, pOverflow);

    /* Compute LSF weighting factors (Q13) */

    Lsf_wt(lsf1, wf1, pOverflow);
    Lsf_wt(lsf2, wf2, pOverflow);

    /* Compute predicted LSF and prediction error */

    p_lsf_p  = &lsf_p[0];
    p_lsf1   = &lsf1[0];
    p_lsf2   = &lsf2[0];
    p_lsf_r1 = &lsf_r1[0];
    p_lsf_r2 = &lsf_r2[0];

    for (i = 0; i < M; i++)
    {
        *(p_lsf_p) = mean_lsf_5[i] +
                     (((Word32)st->past_rq[i] * LSP_PRED_FAC_MR122) >> 15);

        *(p_lsf_r1++) = *(p_lsf1++) - *(p_lsf_p);
        *(p_lsf_r2++) = *(p_lsf2++) - *(p_lsf_p++);
    }

    /*---- Split-MQ of prediction error ----*/

    indice[0] = Vq_subvec(&lsf_r1[0], &lsf_r2[0], dico1_lsf_5,
                          &wf1[0], &wf2[0], DICO1_5_SIZE, pOverflow);

    indice[1] = Vq_subvec(&lsf_r1[2], &lsf_r2[2], dico2_lsf_5,
                          &wf1[2], &wf2[2], DICO2_5_SIZE, pOverflow);

    indice[2] = Vq_subvec_s(&lsf_r1[4], &lsf_r2[4], dico3_lsf_5,
                            &wf1[4], &wf2[4], DICO3_5_SIZE, pOverflow);

    indice[3] = Vq_subvec(&lsf_r1[6], &lsf_r2[6], dico4_lsf_5,
                          &wf1[6], &wf2[6], DICO4_5_SIZE, pOverflow);

    indice[4] = Vq_subvec(&lsf_r1[8], &lsf_r2[8], dico5_lsf_5,
                          &wf1[8], &wf2[8], DICO5_5_SIZE, pOverflow);

    /* Compute quantized LSFs and update the past quantized residual */

    p_lsf_r1 = &lsf_r1[0];
    p_lsf_r2 = &lsf_r2[0];
    p_lsf_p  = &lsf_p[0];
    p_lsf1   = &lsf1_q[0];
    p_lsf2   = &lsf2_q[0];


    for (i = 0; i < M; i++)
    {
        *(p_lsf1++) = *(p_lsf_r1++) + *(p_lsf_p);
        *(p_lsf2++) = *(p_lsf_r2) + *(p_lsf_p++);
        st->past_rq[i] = *(p_lsf_r2++);
    }

    /* verification that LSFs has minimum distance of LSF_GAP */

    Reorder_lsf(lsf1_q, LSF_GAP, M, pOverflow);
    Reorder_lsf(lsf2_q, LSF_GAP, M, pOverflow);

    /*  convert LSFs to the cosine domain */

    Lsf_lsp(lsf1_q, lsp1_q, M, pOverflow);
    Lsf_lsp(lsf2_q, lsp2_q, M, pOverflow);
}

