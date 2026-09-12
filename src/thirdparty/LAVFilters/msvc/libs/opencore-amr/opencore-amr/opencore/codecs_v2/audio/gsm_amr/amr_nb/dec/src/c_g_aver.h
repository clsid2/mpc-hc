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



 Filename: c_g_aver.h

------------------------------------------------------------------------------
 INCLUDE DESCRIPTION

      File             : c_g_aver.h
      Purpose          : Background noise source charateristic detector (SCD)

------------------------------------------------------------------------------
*/

#ifndef _C_G_AVER_H_
#define _C_G_AVER_H_

/*----------------------------------------------------------------------------
; INCLUDES
----------------------------------------------------------------------------*/
#include "typedef.h"
#include "mode.h"
#include "cnst.h"

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
#define L_CBGAINHIST 7

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
        /* history vector of past synthesis speech energy */
        Word16 cbGainHistory[L_CBGAINHIST];

        /* state flags */
        Word16 hangVar;       /* counter; */
        Word16 hangCount;     /* counter; */

    } Cb_gain_averageState;

    /*----------------------------------------------------------------------------
    ; GLOBAL FUNCTION DEFINITIONS
    ; [List function prototypes here]
    ----------------------------------------------------------------------------*/
    /*
     *  Function    : Cb_gain_average_init
     *  Purpose     : Allocates initializes state memory
     *  Description : Stores pointer to filter status struct in *st. This
     *                pointer has to be passed to Cb_gain_average in each call.
     *  Returns     : 0 on success
     */
    Word16 Cb_gain_average_init(Cb_gain_averageState **st);

    /*
     *  Function    : Cb_gain_average_reset
     *  Purpose     : Resets state memory
     *  Returns     : 0 on success
     */
    Word16 Cb_gain_average_reset(Cb_gain_averageState *st);

    /*
     *  Function    : Cb_gain_average_exit
     *  Purpose     : The memory used for state memory is freed
     *  Description : Stores NULL in *s
     *  Returns     : void
     */
    void Cb_gain_average_exit(Cb_gain_averageState **st);

    /*
     *  Function    : Cb_gain_average
     *  Purpose     : Charaterice synthesis speech and detect background noise
     *  Returns     : background noise decision; 0 = bgn, 1 = no bgn
     */
    Word16 Cb_gain_average(
        Cb_gain_averageState *st, /* i/o : State variables for CB gain avergeing   */
        enum Mode mode,           /* i   : AMR mode                                */
        Word16 gain_code,         /* i   : CB gain                              Q1 */
        Word16 lsp[],             /* i   : The LSP for the current frame       Q15 */
        Word16 lspAver[],         /* i   : The average of LSP for 8 frames     Q15 */
        Word16 bfi,               /* i   : bad frame indication flag               */
        Word16 prev_bf,           /* i   : previous bad frame indication flag      */
        Word16 pdfi,              /* i   : potential degraded bad frame ind flag   */
        Word16 prev_pdf,          /* i   : prev pot. degraded bad frame ind flag   */
        Word16 inBackgroundNoise, /* i   : background noise decision               */
        Word16 voicedHangover,    /* i   : # of frames after last voiced frame     */
        Flag   *pOverflow
    );


#ifdef __cplusplus
}
#endif

#endif  /* _C_G_AVER_H_ */


