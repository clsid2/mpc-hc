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



 Filename: dtx_enc.h

------------------------------------------------------------------------------
 INCLUDE DESCRIPTION

      File             : dtx_enc.h
      Purpose          : DTX mode computation of SID parameters

------------------------------------------------------------------------------
*/

#ifndef dtx_enc_h
#define dtx_enc_h "$Id $"

/*----------------------------------------------------------------------------
; INCLUDES
----------------------------------------------------------------------------*/
#include "typedef.h"
#include "cnst.h"
#include "q_plsf.h"
#include "gc_pred.h"
#include "mode.h"
#include "dtx_common_def.h"

/*--------------------------------------------------------------------------*/
#ifdef __cplusplus
extern "C"
{
#endif

    /*----------------------------------------------------------------------------
    ; MACROS
    ; [Define module specific macros here]
    ----------------------------------------------------------------------------*/

    /*----------------------------------------------------------------------------
    ; DEFINES
    ; [Include all pre-processor statements here.]
    ----------------------------------------------------------------------------*/

    /*----------------------------------------------------------------------------
    ; EXTERNAL VARIABLES REFERENCES
    ; [Declare variables used in this module but defined elsewhere]
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
        Word16 lsp_hist[M * DTX_HIST_SIZE];
        Word16 log_en_hist[DTX_HIST_SIZE];
        Word16 hist_ptr;
        Word16 log_en_index;
        Word16 init_lsf_vq_index;
        Word16 lsp_index[3];

        /* DTX handler stuff */
        Word16 dtxHangoverCount;
        Word16 decAnaElapsedCount;

    } dtx_encState;

    /*----------------------------------------------------------------------------
    ; GLOBAL FUNCTION DEFINITIONS
    ; [List function prototypes here]
    ----------------------------------------------------------------------------*/
    /*
    **************************************************************************
    *  Function    : dtx_enc_init
    *  Purpose     : Allocates memory and initializes state variables
    *  Description : Stores pointer to filter status struct in *st. This
    *                pointer has to be passed to dtx_enc in each call.
    *  Returns     : 0 on success
    *
    **************************************************************************
    */
    Word16 dtx_enc_init(dtx_encState **st, const Word16* lsp_init_data_ptr);

    /*
    **************************************************************************
    *
    *  Function    : dtx_enc_reset
    *  Purpose     : Resets state memory
    *  Returns     : 0 on success
    *
    **************************************************************************
    */
    Word16 dtx_enc_reset(dtx_encState *st, const Word16* lsp_init_data_ptr);

    /*
    **************************************************************************
    *
    *  Function    : dtx_enc_exit
    *  Purpose     : The memory used for state memory is freed
    *  Description : Stores NULL in *st
    *
    **************************************************************************
    */
    void dtx_enc_exit(dtx_encState **st);

    /*
    **************************************************************************
    *
    *  Function    : dtx_enc
    *  Purpose     :
    *  Description :
    *
    **************************************************************************
    */
    void dtx_enc(dtx_encState *st,        /* i/o : State struct                  */
                 Word16 computeSidFlag,   /* i   : compute SID                   */
                 Q_plsfState *qSt,        /* i/o : Qunatizer state struct        */
                 gc_predState* predState, /* i/o : State struct                  */
                 Word16 **anap,           /* o   : analysis parameters           */
                 Flag   *pOverflow        /* i/o : overflow indicator            */
                );

    /*
    **************************************************************************
    *
    *  Function    : dtx_buffer
    *  Purpose     : handles the DTX buffer
    *
    **************************************************************************
    */
    void dtx_buffer(dtx_encState *st,   /* i/o : State struct                    */
                    Word16 lsp_new[],   /* i   : LSP vector                      */
                    Word16 speech[],    /* i   : speech samples                  */
                    Flag   *pOverflow   /* i/o : overflow indicator              */
                   );

    /*
    **************************************************************************
    *
    *  Function    : tx_dtx_handler
    *  Purpose     : adds extra speech hangover to analyze speech on the decoding side.
    *  Description : returns 1 when a new SID analysis may be made
    *                otherwise it adds the appropriate hangover after a sequence
    *                with out updates of SID parameters .
    *
    **************************************************************************
    */
    Word16 tx_dtx_handler(dtx_encState *st,      /* i/o : State struct           */
                          Word16 vad_flag,       /* i   : vad decision           */
                          enum Mode *usedMode,   /* i/o : mode changed or not    */
                          Flag   *pOverflow      /* i/o : overflow indicator     */
                         );


#ifdef __cplusplus
}
#endif

#endif  /* _dtx_enc_h_ */


