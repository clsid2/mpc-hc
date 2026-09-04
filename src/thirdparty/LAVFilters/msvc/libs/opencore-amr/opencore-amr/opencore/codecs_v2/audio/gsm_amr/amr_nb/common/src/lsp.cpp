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
 Filename: lsp.cpp
 Functions:

------------------------------------------------------------------------------
 MODULE DESCRIPTION


------------------------------------------------------------------------------
*/

/*----------------------------------------------------------------------------
; INCLUDES
----------------------------------------------------------------------------*/
#include "lsp.h"
#include "typedef.h"
#include "q_plsf.h"
#include "az_lsp.h"
#include "int_lpc.h"
#include "lsp_tab.h"
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
 FUNCTION NAME: lsp_init (lspState **st)
------------------------------------------------------------------------------
 INPUT AND OUTPUT DEFINITIONS

 Inputs:
    st = Pointer to type lspState

 Outputs:
    st = Pointer to type lspState -- values are initialized.

 Returns:
    None

 Global Variables Used:
    lsp_init_data = Word16 array.


 Local Variables Needed:
    None

------------------------------------------------------------------------------
 FUNCTION DESCRIPTION

    Initializes lsp state data.

------------------------------------------------------------------------------
 REQUIREMENTS

 None

------------------------------------------------------------------------------
 REFERENCES

 lsp.c, UMTS GSM AMR speech codec, R99 - Version 3.2.0, March 2, 2001

------------------------------------------------------------------------------
 PSEUDO-CODE


------------------------------------------------------------------------------
 CAUTION [optional]
 [State any special notes, constraints or cautions for users of this function]

------------------------------------------------------------------------------
*/

OSCL_EXPORT_REF Word16 lsp_init(lspState **st)
{
    lspState* s;

    if (st == (lspState **) NULL)
    {
        /* fprintf(stderr, "lsp_init: invalid parameter\n"); */
        return -1;
    }

    *st = NULL;

    /* allocate memory */
    if ((s = (lspState *) oscl_malloc(sizeof(lspState))) == NULL)
    {
        /* fprintf(stderr, "lsp_init: can not malloc state structure\n"); */
        return -1;
    }

    /* Initialize quantization state */
    if (0 != Q_plsf_init(&s->qSt))
    {
        return -1;
    }

    if (0 != lsp_reset(s))
    {
        return -1;
    }

    *st = s;

    return 0;
}





/*
------------------------------------------------------------------------------
 FUNCTION NAME: lsp_reset
------------------------------------------------------------------------------
 INPUT AND OUTPUT DEFINITIONS

 Inputs:
    st = Pointer to type lspState

 Outputs:
    st = Pointer to type lspState -- values are reset.

 Returns:
    None

 Global Variables Used:
    None

 Local Variables Needed:
    None

------------------------------------------------------------------------------
 FUNCTION DESCRIPTION

    resets lsp_state data
------------------------------------------------------------------------------
 REQUIREMENTS

 None

------------------------------------------------------------------------------
 REFERENCES

 lsp.c, UMTS GSM AMR speech codec, R99 - Version 3.2.0, March 2, 2001

------------------------------------------------------------------------------
 PSEUDO-CODE


------------------------------------------------------------------------------
 CAUTION [optional]
 [State any special notes, constraints or cautions for users of this function]

------------------------------------------------------------------------------
*/
OSCL_EXPORT_REF Word16 lsp_reset(lspState *st)
{

    if (st == (lspState *) NULL)
    {
        /* fprintf(stderr, "lsp_reset: invalid parameter\n"); */
        return -1;
    }

    /* Init lsp_old[] */
    oscl_memcpy(st->lsp_old,   lsp_init_data,   M*sizeof(Word16));

    /* Initialize lsp_old_q[] */
    oscl_memcpy(st->lsp_old_q,   st->lsp_old,  M*sizeof(Word16));

    /* Reset quantization state */
    Q_plsf_reset(st->qSt);

    return 0;
}







/*
------------------------------------------------------------------------------
 FUNCTION NAME: lsp_exit
------------------------------------------------------------------------------
 INPUT AND OUTPUT DEFINITIONS

 Inputs:
    st = Pointer to type lspState

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

    Frees memory used by lspState.

------------------------------------------------------------------------------
 REQUIREMENTS

 None

------------------------------------------------------------------------------
 REFERENCES

 lsp.c, UMTS GSM AMR speech codec, R99 - Version 3.2.0, March 2, 2001

------------------------------------------------------------------------------
 PSEUDO-CODE


------------------------------------------------------------------------------
 CAUTION [optional]
 [State any special notes, constraints or cautions for users of this function]

------------------------------------------------------------------------------
*/
OSCL_EXPORT_REF void lsp_exit(lspState **st)
{
    if (st == NULL || *st == NULL)
        return;

    /* Deallocate members */
    Q_plsf_exit(&(*st)->qSt);

    /* deallocate memory */
    oscl_free(*st);
    *st = NULL;

    return;
}



/*
------------------------------------------------------------------------------
 FUNCTION NAME: lsp
------------------------------------------------------------------------------
 INPUT AND OUTPUT DEFINITIONS



 Inputs:
    st = Pointer to type lspState -- State struct
    req_mode = enum Mode -- requested coder mode
    used_mode = enum Mode -- used coder mode
    az = array of type Word16 -- interpolated LP parameters Q12

 Outputs:
    azQ = array of type Word16 -- quantization interpol. LP parameters Q12
    lsp_new = array of type Word16 -- new lsp vector
    anap = Double pointer of type Word16 -- analysis parameters
    pOverflow = Pointer to type Flag -- Flag set when overflow occurs
    st = Pointer to type lspState -- State struct
    az = array of type Word16 -- interpolated LP parameters Q12

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

 lsp.c, UMTS GSM AMR speech codec, R99 - Version 3.2.0, March 2, 2001

------------------------------------------------------------------------------
 PSEUDO-CODE


------------------------------------------------------------------------------
 CAUTION [optional]
 [State any special notes, constraints or cautions for users of this function]

------------------------------------------------------------------------------
*/
OSCL_EXPORT_REF void lsp(lspState *st,       /* i/o : State struct            */
                         enum Mode req_mode, /* i   : requested coder mode                    */
                         enum Mode used_mode,/* i   : used coder mode                         */
                         Word16 az[],        /* i/o : interpolated LP parameters Q12          */
                         Word16 azQ[],       /* o   : quantization interpol. LP parameters Q12*/
                         Word16 lsp_new[],   /* o   : new lsp vector                          */
                         Word16 **anap,      /* o   : analysis parameters                     */
                         Flag   *pOverflow)  /* o   : Flag set when overflow occurs           */

{
    Word16 lsp_new_q[M];    /* LSPs at 4th subframe           */
    Word16 lsp_mid[M], lsp_mid_q[M];    /* LSPs at 2nd subframe           */

    Word16 pred_init_i; /* init index for MA prediction in DTX mode */

    if (req_mode == MR122)
    {
        Az_lsp(&az[MP1], lsp_mid, st->lsp_old, pOverflow);
        Az_lsp(&az[MP1 * 3], lsp_new, lsp_mid, pOverflow);

        /*--------------------------------------------------------------------*
         * Find interpolated LPC parameters in all subframes (both quantized  *
         * and unquantized).                                                  *
         * The interpolated parameters are in array A_t[] of size (M+1)*4     *
         * and the quantized interpolated parameters are in array Aq_t[]      *
         *--------------------------------------------------------------------*/
        Int_lpc_1and3_2(st->lsp_old, lsp_mid, lsp_new, az, pOverflow);

        if (used_mode != MRDTX)
        {
            /* LSP quantization (lsp_mid[] and lsp_new[] jointly quantized) */
            Q_plsf_5(
                st->qSt,
                lsp_mid,
                lsp_new,
                lsp_mid_q,
                lsp_new_q,
                *anap,
                pOverflow);

            Int_lpc_1and3(st->lsp_old_q, lsp_mid_q, lsp_new_q, azQ, pOverflow);

            /* Advance analysis parameters pointer */
            (*anap) += 5;
        }
    }
    else
    {
        Az_lsp(&az[MP1 * 3], lsp_new, st->lsp_old, pOverflow);  /* From A(z) to lsp  */

        /*--------------------------------------------------------------------*
         * Find interpolated LPC parameters in all subframes (both quantized  *
         * and unquantized).                                                  *
         * The interpolated parameters are in array A_t[] of size (M+1)*4     *
         * and the quantized interpolated parameters are in array Aq_t[]      *
         *--------------------------------------------------------------------*/

        Int_lpc_1to3_2(st->lsp_old, lsp_new, az, pOverflow);

        if (used_mode != MRDTX)
        {
            /* LSP quantization */
            Q_plsf_3(
                st->qSt,
                req_mode,
                lsp_new,
                lsp_new_q,
                *anap,
                &pred_init_i,
                pOverflow);

            Int_lpc_1to3(
                st->lsp_old_q,
                lsp_new_q,
                azQ,
                pOverflow);

            /* Advance analysis parameters pointer */
            (*anap) += 3;
        }
    }

    /* update the LSPs for the next frame */
    oscl_memcpy(st->lsp_old,   lsp_new,   M*sizeof(Word16));

    if (used_mode != MRDTX)
    {
        oscl_memcpy(st->lsp_old_q, lsp_new_q, M*sizeof(Word16));
    }
}

