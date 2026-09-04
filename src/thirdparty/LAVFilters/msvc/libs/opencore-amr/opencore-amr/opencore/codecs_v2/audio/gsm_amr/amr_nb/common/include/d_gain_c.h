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

 Filename: d_gain_c.h

------------------------------------------------------------------------------
 INCLUDE DESCRIPTION

      File          : d_gain_c.h
      Purpose       : Decode the fixed codebook gain using the received index.

------------------------------------------------------------------------------
*/

#ifndef _D_GAIN_C_H_
#define _D_GAIN_C_H_
#define d_gain_c_h "$Id $"

/*----------------------------------------------------------------------------
; INCLUDES
----------------------------------------------------------------------------*/
#include "typedef.h"
#include "mode.h"
#include "gc_pred.h"

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
    /*
     *  Function    : d_gain_code
     *  Purpose     : Decode the fixed codebook gain using the received index.
     *  Description : The received index gives the gain correction factor
     *                gamma. The quantized gain is given by   g_q = g0 * gamma
     *                where g0 is the predicted gain. To find g0, 4th order
     *                MA prediction is applied to the mean-removed innovation
     *                energy in dB.
     *  Returns     : void
     */
    void d_gain_code(
        gc_predState *pred_state, /* i/o : MA predictor state               */
        enum Mode mode,           /* i   : AMR mode                         */
        Word16 index,             /* i   : received quantization index      */
        Word16 code[],            /* i   : innovation codevector            */
        const Word16* qua_gain_code_ptr, /* i : Pointer to read-only table      */
        Word16 *gain_code,        /* o   : decoded innovation gain          */
        Flag   *pOverflow
    );

#ifdef __cplusplus
}
#endif

#endif  /* _D_GAIN_C_H_ */


