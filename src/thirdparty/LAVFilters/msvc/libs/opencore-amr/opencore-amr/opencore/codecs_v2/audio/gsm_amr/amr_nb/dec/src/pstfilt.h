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



 Filename: pstfilt.h

------------------------------------------------------------------------------
 INCLUDE DESCRIPTION

      File             : pstfilt.h
      Purpose          : Performs adaptive postfiltering on the synthesis
                       : speech

------------------------------------------------------------------------------
*/

#ifndef _PSTFILT_H_
#define _PSTFILT_H_
#define pstfilt_h "$Id $"

/*----------------------------------------------------------------------------
; INCLUDES
----------------------------------------------------------------------------*/
#include "typedef.h"
#include "mode.h"
#include "cnst.h"
#include "preemph.h"
#include "agc.h"

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
        Word16 res2[L_SUBFR];
        Word16 mem_syn_pst[M];
        preemphasisState preemph_state;
        agcState agc_state;
        Word16 synth_buf[M + L_FRAME];
    } Post_FilterState;

    /*----------------------------------------------------------------------------
    ; GLOBAL FUNCTION DEFINITIONS
    ; [List function prototypes here]
    ----------------------------------------------------------------------------*/
    Word16 Post_Filter_reset(Post_FilterState *st);
    /* reset post filter (i.e. set state memory to zero)
       returns 0 on success
     */

    void Post_Filter(
        Post_FilterState *st, /* i/o : post filter states                        */
        enum Mode mode,       /* i   : AMR mode                                  */
        Word16 *syn,          /* i/o : synthesis speech (postfiltered is output) */
        Word16 *Az_4,         /* i   : interpolated LPC parameters in all subfr. */
        Flag   *pOverflow
    );
    /* filters the signal syn using the parameters in Az_4 to calculate filter
       coefficients.
       The filter must be set up using Post_Filter_init prior to the first call
       to Post_Filter. Post_FilterState is updated to mirror the current state
       of the filter

       return 0 on success
     */

#ifdef __cplusplus
}
#endif

#endif  /* _PSTFILT_H_ */



