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

 Filename: gc_pred.h

------------------------------------------------------------------------------
 INCLUDE DESCRIPTION

      File             : gc_pred.h
      Purpose          : codebook gain MA prediction

------------------------------------------------------------------------------
*/

#ifndef _GC_PRED_H_
#define _GC_PRED_H_
#define gc_pred_h "$Id $"

/*----------------------------------------------------------------------------
; INCLUDES
----------------------------------------------------------------------------*/
#include "typedef.h"
#include "mode.h"


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
        Word16 past_qua_en[4];         /* normal MA predictor memory,         Q10 */
        /* (contains 20*log10(qua_err))            */
        Word16 past_qua_en_MR122[4];   /* MA predictor memory for MR122 mode, Q10 */
        /* (contains log2(qua_err))                */
    } gc_predState;

    /*----------------------------------------------------------------------------
    ; GLOBAL FUNCTION DEFINITIONS
    ; [List function prototypes here]
    ----------------------------------------------------------------------------*/

    OSCL_IMPORT_REF Word16 gc_pred_reset(gc_predState *st);
    /* reset of codebook gain MA predictor state (i.e. set state memory to zero)
       returns 0 on success
     */
    void gc_pred_exit(gc_predState **st);
    /* de-initialize codebook gain MA predictor state (i.e. free state struct)
       stores NULL in *st
     */

    void
    gc_pred_copy(
        gc_predState *st_src,  /* i : State struct                           */
        gc_predState *st_dest  /* o : State struct                           */
    );

    /*
     * FUNCTION:  gc_pred()
     * PURPOSE: MA prediction of the innovation energy
     *          (in dB/(20*log10(2))) with mean  removed).
     */
    OSCL_IMPORT_REF void gc_pred(
        gc_predState *st,   /* i/o: State struct                           */
        enum Mode mode,     /* i  : AMR mode                               */
        Word16 *code,       /* i  : innovative codebook vector (L_SUBFR)   */
        /*      MR122: Q12, other modes: Q13           */
        Word16 *exp_gcode0, /* o  : exponent of predicted gain factor, Q0  */
        Word16 *frac_gcode0,/* o  : fraction of predicted gain factor  Q15 */
        Word16 *exp_en,     /* o  : exponent of innovation energy,     Q0  */
        /*      (only calculated for MR795)            */
        Word16 *frac_en,    /* o  : fraction of innovation energy,     Q15 */
        /*      (only calculated for MR795)            */
        Flag   *pOverflow
    );

    /*
     * FUNCTION:  gc_pred_update()
     * PURPOSE: update MA predictor with last quantized energy
     */
    OSCL_IMPORT_REF void gc_pred_update(
        gc_predState *st,      /* i/o: State struct                     */
        Word16 qua_ener_MR122, /* i  : quantized energy for update, Q10 */
        /*      (log2(qua_err))                  */
        Word16 qua_ener        /* i  : quantized energy for update, Q10 */
        /*      (20*log10(qua_err))              */
    );

    /*
     * FUNCTION:  gc_pred_average_limited()
     * PURPOSE: get average of MA predictor state values (with a lower limit)
     *          [used in error concealment]
     */
    OSCL_IMPORT_REF void gc_pred_average_limited(
        gc_predState *st,       /* i: State struct                    */
        Word16 *ener_avg_MR122, /* o: averaged quantized energy,  Q10 */
        /*    (log2(qua_err))                 */
        Word16 *ener_avg,       /* o: averaged quantized energy,  Q10 */
        /*    (20*log10(qua_err))             */
        Flag   *pOverflow
    );


#ifdef __cplusplus
}
#endif

#endif  /* _GC_PRED_H_ */



