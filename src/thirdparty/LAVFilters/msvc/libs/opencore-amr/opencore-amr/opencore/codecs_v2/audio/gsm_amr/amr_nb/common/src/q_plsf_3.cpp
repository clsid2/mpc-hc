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

 Filename: q_plsf_3.cpp
 Funtions: Vq_subvec4
           Test_Vq_subvec4
           Vq_subvec3
           Test_Vq_subvec3
           Q_plsf_3

------------------------------------------------------------------------------
 MODULE DESCRIPTION

 This file contains the functions that perform the quantization of LSF
 parameters with first order MA prediction and split by 3 vector
 quantization (split-VQ).

------------------------------------------------------------------------------
*/

/*----------------------------------------------------------------------------
; INCLUDES
----------------------------------------------------------------------------*/
#include "q_plsf.h"
#include "typedef.h"
#include "lsp_lsf.h"
#include "reorder.h"
#include "lsfwt.h"
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
#define PAST_RQ_INIT_SIZE 8

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
    extern const Word16 mean_lsf_3[];

    extern const Word16 pred_fac_3[];

    extern const Word16 dico1_lsf_3[];
    extern const Word16 dico2_lsf_3[];
    extern const Word16 dico3_lsf_3[];

    extern const Word16 mr515_3_lsf[];
    extern const Word16 mr795_1_lsf[];

    extern const Word16 past_rq_init[];

    /*--------------------------------------------------------------------------*/
#ifdef __cplusplus
}
#endif

/*
------------------------------------------------------------------------------
 FUNCTION NAME: Vq_subvec4
------------------------------------------------------------------------------
 INPUT AND OUTPUT DEFINITIONS

 Inputs:
    lsf_r1 = pointer to the first LSF residual vector (Q15) (Word16)
    dico = pointer to the quantization codebook (Q15) (const Word16)
    wf1 = pointer to the first LSF weighting factor (Q13) (Word16)
    dico_size = size of quantization codebook (Q0) (Word16)

 Outputs:
    buffer pointed to by lsf_r1 contains the selected vector
    pOverflow -- pointer to Flag -- Flag set when overflow occurs

 Returns:
    index = quantization index (Q0) (Word16)

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

 q_plsf_3.c, UMTS GSM AMR speech codec, R99 - Version 3.2.0, March 2, 2001

------------------------------------------------------------------------------
 PSEUDO-CODE

static Word16
Vq_subvec4(             // o: quantization index,            Q0
    Word16 * lsf_r1,    // i: 1st LSF residual vector,       Q15
    Word16 * dico,      // i: quantization codebook,         Q15
    Word16 * wf1,       // i: 1st LSF weighting factors,     Q13
    Word16 dico_size)   // i: size of quantization codebook, Q0
{
    Word16 i, index = 0;
    Word16 *p_dico, temp;
    Word32 dist_min, dist;

    dist_min = MAX_32;
    p_dico = dico;

    for (i = 0; i < dico_size; i++)
    {
        temp = sub (lsf_r1[0], *p_dico++);
        temp = mult (wf1[0], temp);
        dist = L_mult (temp, temp);

        temp = sub (lsf_r1[1], *p_dico++);
        temp = mult (wf1[1], temp);
        dist = L_mac (dist, temp, temp);

        temp = sub (lsf_r1[2], *p_dico++);
        temp = mult (wf1[2], temp);
        dist = L_mac (dist, temp, temp);

        temp = sub (lsf_r1[3], *p_dico++);
        temp = mult (wf1[3], temp);
        dist = L_mac (dist, temp, temp);


        if (L_sub (dist, dist_min) < (Word32) 0)
        {
            dist_min = dist;
            index = i;
        }
    }

    // Reading the selected vector

    p_dico = &dico[shl (index, 2)];
    lsf_r1[0] = *p_dico++;
    lsf_r1[1] = *p_dico++;
    lsf_r1[2] = *p_dico++;
    lsf_r1[3] = *p_dico;

    return index;

}

------------------------------------------------------------------------------
 CAUTION [optional]
 [State any special notes, constraints or cautions for users of this function]

------------------------------------------------------------------------------
*/

static Word16 Vq_subvec4( /* o: quantization index,            Q0  */
    Word16 * lsf_r1,      /* i: 1st LSF residual vector,       Q15 */
    const Word16 * dico,  /* i: quantization codebook,         Q15 */
    Word16 * wf1,         /* i: 1st LSF weighting factors,     Q13 */
    Word16 dico_size,     /* i: size of quantization codebook, Q0  */
    Flag  *pOverflow      /* o : Flag set when overflow occurs     */
)
{
    register Word16 i;
    Word16 temp;
    const Word16 *p_dico;
    Word16 index = 0;
    Word32 dist_min;
    Word32 dist;

    Word16 lsf_r1_0;
    Word16 lsf_r1_1;
    Word16 lsf_r1_2;
    Word16 lsf_r1_3;

    Word16 wf1_0;
    Word16 wf1_1;
    Word16 wf1_2;
    Word16 wf1_3;

    OSCL_UNUSED_ARG(pOverflow);

    dist_min = MAX_32;
    p_dico = dico;

    lsf_r1_0 = lsf_r1[0];
    lsf_r1_1 = lsf_r1[1];
    lsf_r1_2 = lsf_r1[2];
    lsf_r1_3 = lsf_r1[3];

    wf1_0 = wf1[0];
    wf1_1 = wf1[1];
    wf1_2 = wf1[2];
    wf1_3 = wf1[3];

    for (i = 0; i < dico_size; i++)
    {
        temp = lsf_r1_0 - (*p_dico++);
        temp = (Word16)((((Word32) wf1_0) * temp) >> 15);
        dist = ((Word32) temp) * temp;

        temp = lsf_r1_1 - (*p_dico++);
        temp = (Word16)((((Word32) wf1_1) * temp) >> 15);
        dist += ((Word32) temp) * temp;

        temp = lsf_r1_2 - (*p_dico++);
        temp = (Word16)((((Word32) wf1_2) * temp) >> 15);
        dist += ((Word32) temp) * temp;

        temp = lsf_r1_3 - (*p_dico++);
        temp = (Word16)((((Word32) wf1_3) * temp) >> 15);
        dist += ((Word32) temp) * temp;

        if (dist < dist_min)
        {
            dist_min = dist;
            index = i;
        }
    }

    /* Reading the selected vector */

    p_dico = dico + (index << 2);
    *lsf_r1++ = *p_dico++;
    *lsf_r1++ = *p_dico++;
    *lsf_r1++ = *p_dico++;
    *lsf_r1 = *p_dico;

    return(index);

}

/****************************************************************************/


/*
------------------------------------------------------------------------------
 FUNCTION NAME: Test_Vq_subvec4
------------------------------------------------------------------------------
 INPUT AND OUTPUT DEFINITIONS

 Inputs:
    lsf_r1 = pointer to the first LSF residual vector (Q15) (Word16)
    dico = pointer to the quantization codebook (Q15) (const Word16)
    wf1 = pointer to the first LSF weighting factor (Q13) (Word16)
    dico_size = size of quantization codebook (Q0) (Word16)

 Outputs:
    buffer pointed to by lsf_r1 contains the selected vector
    pOverflow -- pointer to Flag -- Flag set when overflow occurs

 Returns:
    index = quantization index (Q0) (Word16)

 Global Variables Used:
    None

 Local Variables Needed:
    None

------------------------------------------------------------------------------
 FUNCTION DESCRIPTION

 This function calls the static function Vq_subvec4. It is used for testing
 purposes only

------------------------------------------------------------------------------
 REQUIREMENTS

 None

------------------------------------------------------------------------------
 REFERENCES

 None

------------------------------------------------------------------------------
 PSEUDO-CODE


 CALL Vq_subvec4(lsf_r1 = lsf_r1
                 dico = dico
                 wf1 = wf1
                 dico_size = dico_size)
   MODIFYING(nothing)
   RETURNING(index = tst_index4)

------------------------------------------------------------------------------
 CAUTION [optional]
 [State any special notes, constraints or cautions for users of this function]

------------------------------------------------------------------------------
*/

Word16 Test_Vq_subvec4(
    Word16 * lsf_r1,
    const Word16 * dico,
    Word16 * wf1,
    Word16 dico_size,
    Flag   *pOverflow)
{
    Word16  tst_index4 = 0;

    /*------------------------------------------------------------------------
     CALL Vq_subvec4(lsf_r1 = lsf_r1
                     dico = dico
                     wf1 = wf1
                     dico_size = dico_size)
       MODIFYING(nothing)
       RETURNING(index = index)
    ------------------------------------------------------------------------*/
    tst_index4 =
        Vq_subvec4(
            lsf_r1,
            dico,
            wf1,
            dico_size,
            pOverflow);

    return(tst_index4);

}

/****************************************************************************/

/*
------------------------------------------------------------------------------
 FUNCTION NAME: Vq_subvec3
------------------------------------------------------------------------------
 INPUT AND OUTPUT DEFINITIONS

 Inputs:
    lsf_r1 = pointer to the first LSF residual vector (Q15) (Word16)
    dico = pointer to the quantization codebook (Q15) (const Word16)
    wf1 = pointer to the first LSF weighting factor (Q13) (Word16)
    dico_size = size of quantization codebook (Q0) (Word16)
    use_half = flag to indicate use of every second entry in the
               codebook (Flag)

 Outputs:
    buffer pointed to by lsf_r1 contains the selected vector
    pOverflow -- pointer to Flag -- Flag set when overflow occurs

 Returns:
    index = quantization index (Q0) (Word16)

 Global Variables Used:
    None

 Local Variables Needed:
    None

------------------------------------------------------------------------------
 FUNCTION DESCRIPTION

 This function performs the quantization of a 3 dimensional subvector.

------------------------------------------------------------------------------
 REQUIREMENTS

 None

------------------------------------------------------------------------------
 REFERENCES

 q_plsf_3.c, UMTS GSM AMR speech codec, R99 - Version 3.2.0, March 2, 2001

------------------------------------------------------------------------------
 PSEUDO-CODE

static Word16
Vq_subvec3(             // o: quantization index,            Q0
    Word16 * lsf_r1,    // i: 1st LSF residual vector,       Q15
    Word16 * dico,      // i: quantization codebook,         Q15
    Word16 * wf1,       // i: 1st LSF weighting factors,     Q13
    Word16 dico_size,   // i: size of quantization codebook, Q0
    Flag use_half)      // i: use every second entry in codebook
{
    Word16 i, index = 0;
    Word16 *p_dico, temp;
    Word32 dist_min, dist;

    dist_min = MAX_32;
    p_dico = dico;

    if (use_half == 0) {
       for (i = 0; i < dico_size; i++)
       {
          temp = sub(lsf_r1[0], *p_dico++);
          temp = mult(wf1[0], temp);
          dist = L_mult(temp, temp);

          temp = sub(lsf_r1[1], *p_dico++);
          temp = mult(wf1[1], temp);
          dist = L_mac(dist, temp, temp);

          temp = sub(lsf_r1[2], *p_dico++);
          temp = mult(wf1[2], temp);
          dist = L_mac(dist, temp, temp);

          if (L_sub(dist, dist_min) < (Word32) 0) {
             dist_min = dist;
             index = i;
          }
       }
       p_dico = &dico[add(index, add(index, index))];
    }
    else
    {
       for (i = 0; i < dico_size; i++)
       {
          temp = sub(lsf_r1[0], *p_dico++);
          temp = mult(wf1[0], temp);
          dist = L_mult(temp, temp);

          temp = sub(lsf_r1[1], *p_dico++);
          temp = mult(wf1[1], temp);
          dist = L_mac(dist, temp, temp);

          temp = sub(lsf_r1[2], *p_dico++);
          temp = mult(wf1[2], temp);
          dist = L_mac(dist, temp, temp);

          if (L_sub(dist, dist_min) < (Word32) 0)
          {
             dist_min = dist;
             index = i;
          }
          p_dico = p_dico + 3; add(0,0);
       }
       p_dico = &dico[shl(add(index, add(index, index)),1)];
    }


    // Reading the selected vector
    lsf_r1[0] = *p_dico++;
    lsf_r1[1] = *p_dico++;
    lsf_r1[2] = *p_dico++;

    return index;
}

------------------------------------------------------------------------------
 CAUTION [optional]
 [State any special notes, constraints or cautions for users of this function]

------------------------------------------------------------------------------
*/

static Word16 Vq_subvec3( /* o: quantization index,            Q0  */
    Word16 * lsf_r1,      /* i: 1st LSF residual vector,       Q15 */
    const Word16 * dico,  /* i: quantization codebook,         Q15 */
    Word16 * wf1,         /* i: 1st LSF weighting factors,     Q13 */
    Word16 dico_size,     /* i: size of quantization codebook, Q0  */
    Flag use_half,        /* i: use every second entry in codebook */
    Flag  *pOverflow)     /* o : Flag set when overflow occurs     */
{
    register Word16 i;
    Word16 temp;

    const Word16 *p_dico;

    Word16 p_dico_index = 0;
    Word16 index = 0;

    Word32 dist_min;
    Word32 dist;

    Word16 lsf_r1_0;
    Word16 lsf_r1_1;
    Word16 lsf_r1_2;

    Word16 wf1_0;
    Word16 wf1_1;
    Word16 wf1_2;

    OSCL_UNUSED_ARG(pOverflow);

    dist_min = MAX_32;
    p_dico = dico;

    lsf_r1_0 = lsf_r1[0];
    lsf_r1_1 = lsf_r1[1];
    lsf_r1_2 = lsf_r1[2];

    wf1_0 = wf1[0];
    wf1_1 = wf1[1];
    wf1_2 = wf1[2];

    if (use_half != 0)
    {
        p_dico_index = 3;
    }

    for (i = 0; i < dico_size; i++)
    {
        temp = lsf_r1_0 - (*p_dico++);
        temp = (Word16)((((Word32) wf1_0) * temp) >> 15);
        dist = ((Word32) temp) * temp;

        temp = lsf_r1_1 - (*p_dico++);
        temp = (Word16)((((Word32) wf1_1) * temp) >> 15);
        dist += ((Word32) temp) * temp;

        temp = lsf_r1_2 - (*p_dico++);
        temp = (Word16)((((Word32) wf1_2) * temp) >> 15);
        dist += ((Word32) temp) * temp;

        if (dist < dist_min)
        {
            dist_min = dist;
            index = i;
        }

        p_dico = p_dico + p_dico_index;
    }

    p_dico = dico + (3 * index);

    if (use_half != 0)
    {
        p_dico += (3 * index);
    }

    /* Reading the selected vector */
    *lsf_r1++ = *p_dico++;
    *lsf_r1++ = *p_dico++;
    *lsf_r1 = *p_dico;

    return(index);
}

/****************************************************************************/


/*
------------------------------------------------------------------------------
 FUNCTION NAME: Test_Vq_subvec3
------------------------------------------------------------------------------
 INPUT AND OUTPUT DEFINITIONS

 Inputs:
    lsf_r1 = pointer to the first LSF residual vector (Q15) (Word16)
    dico = pointer to the quantization codebook (Q15) (const Word16)
    wf1 = pointer to the first LSF weighting factor (Q13) (Word16)
    dico_size = size of quantization codebook (Q0) (Word16)
    use_half = flag to indicate use of every second entry in the
               codebook (Flag)

 Outputs:
    buffer pointed to by lsf_r1 contains the selected vector
    pOverflow -- pointer to Flag -- Flag set when overflow occurs

 Returns:
    index = quantization index (Q0) (Word16)

 Global Variables Used:
    None

 Local Variables Needed:
    None

------------------------------------------------------------------------------
 FUNCTION DESCRIPTION

 This function calls the static function Vq_subvec3. It is used for testing
 purposes only

------------------------------------------------------------------------------
 REQUIREMENTS

 None

------------------------------------------------------------------------------
 REFERENCES

 None

------------------------------------------------------------------------------
 PSEUDO-CODE

 CALL Vq_subvec3(lsf_r1 = lsf_r1
                 dico = dico
                 wf1 = wf1
                 dico_size = dico_size
                 use_half = use_half)
   MODIFYING(nothing)
   RETURNING(index = tst_index3)

------------------------------------------------------------------------------
 CAUTION [optional]
 [State any special notes, constraints or cautions for users of this function]

------------------------------------------------------------------------------
*/

Word16 Test_Vq_subvec3(
    Word16 * lsf_r1,
    const Word16 * dico,
    Word16 * wf1,
    Word16 dico_size,
    Flag use_half,
    Flag *pOverflow)
{
    Word16  tst_index3 = 0;

    /*------------------------------------------------------------------------
     CALL Vq_subvec3(lsf_r1 = lsf_r1
                     dico = dico
                     wf1 = wf1
                     dico_size = dico_size
                     use_half = use_half)
       MODIFYING(nothing)
       RETURNING(index = index)
    ------------------------------------------------------------------------*/
    tst_index3 =
        Vq_subvec3(
            lsf_r1,
            dico,
            wf1,
            dico_size,
            use_half,
            pOverflow);

    return(tst_index3);

}

/****************************************************************************/


/*
------------------------------------------------------------------------------
 FUNCTION NAME: Q_plsf_3
------------------------------------------------------------------------------
 INPUT AND OUTPUT DEFINITIONS

 Inputs:
    st = pointer to structures of type Q_plsfState (Q_plsfState)
    mode = coder mode (enum)
    lsp1 = pointer to the first LSP vector (Word16)
    lsp1_q = pointer to the quantized first LSP vector (Word16)
    indice = pointer to the quantization indices of 3 vectors (Word16)
    pred_init_i = pointer to the index of the initial value for
                  MA prediction in DTX mode (Word16)

 Outputs:
    lsp1_q points to a vector containing the new quantized LSPs
    indice points to the new quantization indices of 3 vectors
    pred_init_i points to the new initial index for MA prediction
      in DTX mode
    past_rq field of structure pointed to by st contains the current
      quantized LSF parameters
    pOverflow -- pointer to Flag -- Flag set when overflow occurs

 Returns:
    None

 Global Variables Used:
    pred_fac = table containing prediction factors (const Word16)
    dico1_lsf = quantization table for split_MQ of 2 sets of LSFs
                in a 20 ms frame (const Word16)
    dico2_lsf = quantization table for split_MQ of 2 sets of LSFs
                in a 20 ms frame (const Word16)
    dico3_lsf = quantization table for split_MQ of 2 sets of LSFs
                in a 20 ms frame (const Word16)
    mr515_3_lsf = third codebook for MR475 and MR515 modes (const Word16)
    mr795_1_lsf = first codebook for MR795 mode (const Word16)
    mean_lsf = table of mean LSFs (const Word16)
    past_rq_init = initalization table for MA predictor in DTX mode
                   (const Word16)


 Local Variables Needed:
    None

------------------------------------------------------------------------------
 FUNCTION DESCRIPTION

 This function performs quantization of LSF parameters with 1st order MA
 prediction and split by 3 vector quantization (split-VQ)

------------------------------------------------------------------------------
 REQUIREMENTS

 None

------------------------------------------------------------------------------
 REFERENCES

 q_plsf_3.c, UMTS GSM AMR speech codec, R99 - Version 3.2.0, March 2, 2001

------------------------------------------------------------------------------
 PSEUDO-CODE

void Q_plsf_3(
    Q_plsfState *st,    // i/o: state struct
    enum Mode mode,     // i  : coder mode
    Word16 *lsp1,       // i  : 1st LSP vector                      Q15
    Word16 *lsp1_q,     // o  : quantized 1st LSP vector            Q15
    Word16 *indice,     // o  : quantization indices of 3 vectors   Q0
    Word16 *pred_init_i // o  : init index for MA prediction in DTX mode
)
{
    Word16 i, j;
    Word16 lsf1[M], wf1[M], lsf_p[M], lsf_r1[M];
    Word16 lsf1_q[M];

    Word32 L_pred_init_err;
    Word32 L_min_pred_init_err;
    Word16 temp_r1[M];
    Word16 temp_p[M];

    // convert LSFs to normalize frequency domain 0..16384

    Lsp_lsf(lsp1, lsf1, M);

    // compute LSF weighting factors (Q13)

    Lsf_wt(lsf1, wf1);

    // Compute predicted LSF and prediction error
    if (test(), sub(mode, MRDTX) != 0)
    {
       for (i = 0; i < M; i++)
       {
          lsf_p[i] = add(mean_lsf[i],
                         mult(st->past_rq[i],
                              pred_fac[i]));
          lsf_r1[i] = sub(lsf1[i], lsf_p[i]);
      }
    }
    else
    {
       // DTX mode, search the init vector that yields
       // lowest prediction resuidual energy
       *pred_init_i = 0;
       L_min_pred_init_err = 0x7fffffff; // 2^31 - 1
       for (j = 0; j < PAST_RQ_INIT_SIZE; j++)
       {
          L_pred_init_err = 0;
          for (i = 0; i < M; i++)
          {
             temp_p[i] = add(mean_lsf[i], past_rq_init[j*M+i]);
             temp_r1[i] = sub(lsf1[i],temp_p[i]);
             L_pred_init_err = L_mac(L_pred_init_err, temp_r1[i], temp_r1[i]);
          }  // next i


          if (L_sub(L_pred_init_err, L_min_pred_init_err) < (Word32) 0)
          {
             L_min_pred_init_err = L_pred_init_err;
             Copy(temp_r1, lsf_r1, M);
             Copy(temp_p, lsf_p, M);
             // Set zerom
             Copy(&past_rq_init[j*M], st->past_rq, M);
             *pred_init_i = j;
          } // endif
       } // next j
    } // endif MRDTX

    //---- Split-VQ of prediction error ----
    if (sub (mode, MR475) == 0 || sub (mode, MR515) == 0)
    {   // MR475, MR515


      indice[0] = Vq_subvec3(&lsf_r1[0], dico1_lsf, &wf1[0], DICO1_SIZE, 0);

      indice[1] = Vq_subvec3(&lsf_r1[3], dico2_lsf, &wf1[3], DICO2_SIZE/2, 1);

      indice[2] = Vq_subvec4(&lsf_r1[6], mr515_3_lsf, &wf1[6], MR515_3_SIZE);

    }
    else if (sub (mode, MR795) == 0)
    {   // MR795


      indice[0] = Vq_subvec3(&lsf_r1[0], mr795_1_lsf, &wf1[0], MR795_1_SIZE, 0);

      indice[1] = Vq_subvec3(&lsf_r1[3], dico2_lsf, &wf1[3], DICO2_SIZE, 0);

      indice[2] = Vq_subvec4(&lsf_r1[6], dico3_lsf, &wf1[6], DICO3_SIZE);

    }
    else
    {   // MR59, MR67, MR74, MR102 , MRDTX


      indice[0] = Vq_subvec3(&lsf_r1[0], dico1_lsf, &wf1[0], DICO1_SIZE, 0);

      indice[1] = Vq_subvec3(&lsf_r1[3], dico2_lsf, &wf1[3], DICO2_SIZE, 0);

      indice[2] = Vq_subvec4(&lsf_r1[6], dico3_lsf, &wf1[6], DICO3_SIZE);

    }


    // Compute quantized LSFs and update the past quantized residual

    for (i = 0; i < M; i++)
    {
        lsf1_q[i] = add(lsf_r1[i], lsf_p[i]);
        st->past_rq[i] = lsf_r1[i];
    }

    // verification that LSFs has mimimum distance of LSF_GAP Hz

    Reorder_lsf(lsf1_q, LSF_GAP, M);

    //  convert LSFs to the cosine domain

    Lsf_lsp(lsf1_q, lsp1_q, M);
}

------------------------------------------------------------------------------
 CAUTION [optional]
 [State any special notes, constraints or cautions for users of this function]

------------------------------------------------------------------------------
*/

OSCL_EXPORT_REF void Q_plsf_3(
    Q_plsfState *st,    /* i/o: state struct                             */
    enum Mode mode,     /* i  : coder mode                               */
    Word16 *lsp1,       /* i  : 1st LSP vector                      Q15  */
    Word16 *lsp1_q,     /* o  : quantized 1st LSP vector            Q15  */
    Word16 *indice,     /* o  : quantization indices of 3 vectors   Q0   */
    Word16 *pred_init_i,/* o  : init index for MA prediction in DTX mode */
    Flag  *pOverflow    /* o : Flag set when overflow occurs             */
)
{
    register Word16 i, j;
    Word16 lsf1[M];
    Word16 wf1[M];
    Word16 lsf_p[M];
    Word16 lsf_r1[M];
    Word16 lsf1_q[M];

    Word32 L_pred_init_err;
    Word32 L_min_pred_init_err;
    Word32 L_temp;
    Word16 temp_r1[M];
    Word16 temp_p[M];
    Word16 temp;

    /* convert LSFs to normalize frequency domain 0..16384 */

    Lsp_lsf(
        lsp1,
        lsf1,
        M,
        pOverflow);

    /* compute LSF weighting factors (Q13) */

    Lsf_wt(
        lsf1,
        wf1,
        pOverflow);

    /* Compute predicted LSF and prediction error */
    if (mode != MRDTX)
    {
        for (i = 0; i < M; i++)
        {
            temp = (Word16)((((Word32) st->past_rq[i]) *
                             (*(pred_fac_3 + i))) >> 15);

            *(lsf_p + i) = *(mean_lsf_3 + i) + temp;

            *(lsf_r1 + i) = *(lsf1 + i) - *(lsf_p + i);
        }
    }
    else
    {
        /* DTX mode, search the init vector that yields */
        /* lowest prediction resuidual energy           */
        *pred_init_i = 0;
        L_min_pred_init_err = 0x7fffffff; /* 2^31 - 1 */

        for (j = 0; j < PAST_RQ_INIT_SIZE; j++)
        {
            L_pred_init_err = 0;
            for (i = 0; i < M; i++)
            {
                *(temp_p + i) = *(mean_lsf_3 + i) + *(past_rq_init + j * M + i);

                *(temp_r1 + i) = *(lsf1 + i) - *(temp_p + i);

                L_temp = ((Word32) * (temp_r1 + i)) * *(temp_r1 + i);

                L_pred_init_err = L_pred_init_err + (L_temp << 1);

            }  /* next i */


            if (L_pred_init_err < L_min_pred_init_err)
            {
                L_min_pred_init_err = L_pred_init_err;

                oscl_memcpy(
                    lsf_r1,
                    temp_r1,
                    M*sizeof(Word16));

                oscl_memcpy(
                    lsf_p,
                    temp_p,
                    M*sizeof(Word16));

                /* Set zerom */
                oscl_memcpy(
                    st->past_rq,
                    &past_rq_init[j*M],
                    M*sizeof(Word16));

                *pred_init_i = j;

            } /* endif */
        } /* next j */
    } /* endif MRDTX */

    /*---- Split-VQ of prediction error ----*/
    if ((mode == MR475) || (mode == MR515))
    {   /* MR475, MR515 */

        *indice =
            Vq_subvec3(
                lsf_r1,
                dico1_lsf_3,
                wf1,
                DICO1_SIZE,
                0,
                pOverflow);

        *(indice + 1) =
            Vq_subvec3(
                lsf_r1 + 3,
                dico2_lsf_3,
                wf1 + 3,
                DICO2_SIZE / 2,
                1,
                pOverflow);

        *(indice + 2) =
            Vq_subvec4(
                lsf_r1 + 6,
                mr515_3_lsf,
                wf1 + 6,
                MR515_3_SIZE,
                pOverflow);

    }
    else if (mode == MR795)
    {   /* MR795 */

        *indice =
            Vq_subvec3(
                lsf_r1,
                mr795_1_lsf,
                wf1,
                MR795_1_SIZE,
                0,
                pOverflow);

        *(indice + 1) =
            Vq_subvec3(
                lsf_r1 + 3,
                dico2_lsf_3,
                wf1 + 3,
                DICO2_SIZE,
                0,
                pOverflow);

        *(indice + 2) =
            Vq_subvec4(
                lsf_r1 + 6,
                dico3_lsf_3,
                wf1 + 6,
                DICO3_SIZE,
                pOverflow);

    }
    else
    {   /* MR59, MR67, MR74, MR102 , MRDTX */

        *indice =
            Vq_subvec3(
                lsf_r1,
                dico1_lsf_3,
                wf1,
                DICO1_SIZE,
                0,
                pOverflow);

        *(indice + 1) =
            Vq_subvec3(
                lsf_r1 + 3,
                dico2_lsf_3,
                wf1 + 3,
                DICO2_SIZE,
                0,
                pOverflow);

        *(indice + 2) =
            Vq_subvec4(
                lsf_r1 + 6,
                dico3_lsf_3,
                wf1 + 6,
                DICO3_SIZE,
                pOverflow);

    }


    /* Compute quantized LSFs and update the past quantized residual */

    for (i = 0; i < M; i++)
    {
        *(lsf1_q + i) = *(lsf_r1 + i) + *(lsf_p + i);
        st->past_rq[i] = *(lsf_r1 + i);
    }

    /* verification that LSFs has mimimum distance of LSF_GAP Hz */

    Reorder_lsf(
        lsf1_q,
        LSF_GAP,
        M,
        pOverflow);

    /*  convert LSFs to the cosine domain */

    Lsf_lsp(
        lsf1_q,
        lsp1_q,
        M,
        pOverflow);

    return;

}
