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

 Filename: d_plsf.h

------------------------------------------------------------------------------
 INCLUDE DESCRIPTION

 This file contains all the constant definitions and prototype definitions
 needed by the d_plsf_3.c and d_plsf_5.c

------------------------------------------------------------------------------
*/

/*----------------------------------------------------------------------------
; CONTINUE ONLY IF NOT ALREADY DEFINED
----------------------------------------------------------------------------*/
#ifndef d_plsf_h
#define d_plsf_h "$Id $"

/*----------------------------------------------------------------------------
; INCLUDES
----------------------------------------------------------------------------*/
#include "typedef.h"
#include "cnst.h"
#include "mode.h"
#include "get_const_tbls.h"

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
    ; Include all pre-processor statements here.
    ----------------------------------------------------------------------------*/

    /*----------------------------------------------------------------------------
    ; EXTERNAL VARIABLES REFERENCES
    ; Declare variables used in this module but defined elsewhere
    ----------------------------------------------------------------------------*/

    /*----------------------------------------------------------------------------
    ; SIMPLE TYPEDEF'S
    ----------------------------------------------------------------------------*/

    /*----------------------------------------------------------------------------
    ; ENUMERATED TYPEDEF'S
    ----------------------------------------------------------------------------*/

    /*----------------------------------------------------------------------------
    ; STRUCTURES TYPEDEF'S
    ----------------------------------------------------------------------------*/
    typedef struct
    {
        Word16 past_r_q[M];   /* Past quantized prediction error, Q15 */
        Word16 past_lsf_q[M]; /* Past dequantized lsfs,           Q15 */
    } D_plsfState;

    /*----------------------------------------------------------------------------
    ; GLOBAL FUNCTION DEFINITIONS
    ; Function Prototype declaration
    ----------------------------------------------------------------------------*/

    /*
    **************************************************************************
    *
    *  Function    : D_plsf_reset
    *  Purpose     : Resets state memory
    *  Returns     : 0 on success
    *
    **************************************************************************
    */
    Word16 D_plsf_reset(D_plsfState *st, const Word16* mean_lsf_5_ptr);

    /*
    **************************************************************************
    *
    *  Function    : D_plsf_exit
    *  Purpose     : The memory used for state memory is freed
    *  Description : Stores NULL in *st
    *  Returns     : void
    *
    **************************************************************************
    */
    void D_plsf_exit(D_plsfState **st);

    /*
    **************************************************************************
    *
    *  Function    : D_plsf_5
    *  Purpose     : Decodes the 2 sets of LSP parameters in a frame
    *                using the received quantization indices.
    *  Description : The two sets of LSFs are quantized using split by
    *                5 matrix quantization (split-MQ) with 1st order MA
    *                prediction.
    *                See "q_plsf_5.c" for more details about the
    *                quantization procedure
    *  Returns     : 0
    *
    **************************************************************************
    */
    void D_plsf_5(
        D_plsfState *st,  /* i/o: State variables                            */
        Word16 bfi,       /* i  : bad frame indicator (set to 1 if a bad
                              frame is received)                         */
        Word16 *indice,   /* i  : quantization indices of 5 submatrices, Q0  */
        CommonAmrTbls* common_amr_tbls, /* i : structure containing ptrs to read-only tables */
        Word16 *lsp1_q,   /* o  : quantized 1st LSP vector (M)           Q15 */
        Word16 *lsp2_q,   /* o  : quantized 2nd LSP vector (M)           Q15 */
        Flag  *pOverflow  /* o : Flag set when overflow occurs               */
    );

    /*************************************************************************
     *
     *  FUNCTION:   D_plsf_3()
     *
     *  PURPOSE: Decodes the LSP parameters using the received quantization
     *           indices.1st order MA prediction and split by 3 matrix
     *           quantization (split-MQ)
     *
     *************************************************************************/

    void D_plsf_3(
        D_plsfState *st,  /* i/o: State struct                               */
        enum Mode mode,   /* i  : coder mode                                 */
        Word16 bfi,       /* i  : bad frame indicator (set to 1 if a         */
        /*      bad frame is received)                     */
        Word16 * indice,  /* i  : quantization indices of 3 submatrices, Q0  */
        CommonAmrTbls* common_amr_tbls, /* i : structure containing ptrs to read-only tables */
        Word16 * lsp1_q,  /* o  : quantized 1st LSP vector,              Q15 */
        Flag  *pOverflow  /* o : Flag set when overflow occurs               */
    );

    /*************************************************************************
     *
     *  FUNCTION:   Init_D_plsf_3()
     *
     *  PURPOSE: Set the past_r_q[M] vector to one of the eight
     *           past_rq_init vectors.
     *
     *************************************************************************/
    void Init_D_plsf_3(D_plsfState *st,  /* i/o: State struct                */
                       Word16 index,     /* i  : past_rq_init[] index [0, 7] */
                       const Word16* past_rq_init_ptr /* ptr to read-only table */
                      );

    /*----------------------------------------------------------------------------
    ; END
    ----------------------------------------------------------------------------*/
#ifdef __cplusplus
}
#endif

#endif /* _Q_PLSF_H_ */

