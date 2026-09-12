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



 Filename: lsp_avg.cpp

------------------------------------------------------------------------------
 MODULE DESCRIPTION

    LSP averaging and history
------------------------------------------------------------------------------
*/

/*----------------------------------------------------------------------------
; INCLUDES
----------------------------------------------------------------------------*/
#include "lsp_avg.h"
#include "basic_op.h"
#include "oper_32b.h"
#include "oscl_mem.h"
#include "q_plsf_5_tbl.h"

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
; EXTERNAL FUNCTION REFERENCES
; Declare functions defined elsewhere and referenced in this module
----------------------------------------------------------------------------*/

/*----------------------------------------------------------------------------
; EXTERNAL VARIABLES REFERENCES
; Declare variables used in this module but defined elsewhere
----------------------------------------------------------------------------*/

/*
------------------------------------------------------------------------------
 FUNCTION NAME: lsp_avg_reset
------------------------------------------------------------------------------
 INPUT AND OUTPUT DEFINITIONS

 Inputs:
    st = pointer to structure of type lsp_avgState

 Outputs:
    fields of the structure pointed to by state are initialized.

 Returns:
    return_value = 0, if reset was successful; -1, otherwise (int)

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

lsp_avg.c, UMTS GSM AMR speech codec, R99 - Version 3.2.0, March 2, 2001

------------------------------------------------------------------------------
 PSEUDO-CODE

int lsp_avg_reset (lsp_avgState *st)
{
  if (st == (lsp_avgState *) NULL){
      // fprintf(stderr, "lsp_avg_reset: invalid parameter\n");
      return -1;
  }

  Copy(mean_lsf, &st->lsp_meanSave[0], M);

  return 0;
}

------------------------------------------------------------------------------
 CAUTION [optional]
 [State any special notes, constraints or cautions for users of this function]

------------------------------------------------------------------------------
*/

Word16 lsp_avg_reset(lsp_avgState *st, const Word16* mean_lsf_5_ptr)
{
    if (st == (lsp_avgState *) NULL)
    {
        /* fprintf(stderr, "lsp_avg_reset: invalid parameter\n"); */
        return -1;
    }

    oscl_memmove((void *)&st->lsp_meanSave[0], mean_lsf_5_ptr, M*sizeof(*mean_lsf_5_ptr));

    return 0;
}


/*
------------------------------------------------------------------------------
 FUNCTION NAME: lsp_avg
------------------------------------------------------------------------------
 INPUT AND OUTPUT DEFINITIONS

 Inputs:
    st  = pointer to structure of type lsp_avgState
    lsp = pointer to Word16, which reflects the state of the state machine

 Outputs:
    st = pointer to structure of type lsp_avgState
    pOverflow = pointer to type Flag -- overflow indicator

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

lsp_avg.c, UMTS GSM AMR speech codec, R99 - Version 3.2.0, March 2, 2001

------------------------------------------------------------------------------
 PSEUDO-CODE


void lsp_avg (
    lsp_avgState *st,         // i/o : State struct                 Q15
    Word16 *lsp               // i   : state of the state machine   Q15
)
{
    Word16 i;
    Word32 L_tmp;            // Q31

    for (i = 0; i < M; i++) {

       // mean = 0.84*mean
       L_tmp = L_deposit_h(st->lsp_meanSave[i]);
       L_tmp = L_msu(L_tmp, EXPCONST, st->lsp_meanSave[i]);

       // Add 0.16 of newest LSPs to mean
       L_tmp = L_mac(L_tmp, EXPCONST, lsp[i]);

       // Save means
       st->lsp_meanSave[i] = pv_round(L_tmp);   // Q15
    }

    return;
}

------------------------------------------------------------------------------
 CAUTION [optional]
 [State any special notes, constraints or cautions for users of this function]

------------------------------------------------------------------------------
*/

void lsp_avg(
    lsp_avgState *st,         /* i/o : State struct                 Q15 */
    Word16 *lsp,              /* i   : state of the state machine   Q15 */
    Flag   *pOverflow         /* o   : Flag set when overflow occurs    */
)
{
    Word16 i;
    Word32 L_tmp;            /* Q31 */

    for (i = 0; i < M; i++)
    {

        /* mean = 0.84*mean */
        L_tmp = ((Word32)st->lsp_meanSave[i] << 16);
        L_tmp = L_msu(L_tmp, EXPCONST, st->lsp_meanSave[i], pOverflow);

        /* Add 0.16 of newest LSPs to mean */
        L_tmp = L_mac(L_tmp, EXPCONST, lsp[i], pOverflow);

        /* Save means */
        st->lsp_meanSave[i] = pv_round(L_tmp, pOverflow);   /* Q15 */
    }

    return;
}
