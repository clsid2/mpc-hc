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



 Filename: ol_ltp.h

------------------------------------------------------------------------------
 INCLUDE DESCRIPTION

       File             : ol_ltp.h
       Purpose          : Compute the open loop pitch lag.

------------------------------------------------------------------------------
*/

#ifndef OL_LTP_H
#define OL_LTP_H "$Id $"

/*----------------------------------------------------------------------------
; INCLUDES
----------------------------------------------------------------------------*/
#include "typedef.h"
#include "mode.h"
#include "p_ol_wgh.h"

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


    /*----------------------------------------------------------------------------
    ; GLOBAL FUNCTION DEFINITIONS
    ; [List function prototypes here]
    ----------------------------------------------------------------------------*/
    void ol_ltp(
        pitchOLWghtState *st, /* i/o : State struct                            */
        vadState *vadSt,      /* i/o : VAD state struct                        */
        enum Mode mode,       /* i   : coder mode                              */
        Word16 wsp[],         /* i   : signal used to compute the OL pitch, Q0 */
        /*       uses signal[-pit_max] to signal[-1]     */
        Word16 *T_op,         /* o   : open loop pitch lag,                 Q0 */
        Word16 old_lags[],    /* i   : history with old stored Cl lags         */
        Word16 ol_gain_flg[], /* i   : OL gain flag                            */
        Word16 idx,           /* i   : index                                   */
        Flag dtx,             /* i   : dtx flag; use dtx=1, do not use dtx=0   */
        Flag *pOverflow       /* i/o : overflow Flag                           */
    );


#ifdef __cplusplus
}
#endif

#endif  /* _OL_LTP_H_ */


