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



 Filename: qgain475.h

------------------------------------------------------------------------------
 INCLUDE DESCRIPTION

 This file contains the defines and function prototypes used in the
 quantization of pitch and codebook gains for MR475.

------------------------------------------------------------------------------
*/
#ifndef _QGAIN475_H_
#define _QGAIN475_H_
#define qgain475_h "$Id $"

/*----------------------------------------------------------------------------
; INCLUDES
----------------------------------------------------------------------------*/
#include "typedef.h"
#include "gc_pred.h"
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
    /* minimum allowed gain code prediction error: 102.887/4096 = 0.0251189 */
#define MIN_QUA_ENER         ( -5443) /* Q10 <->    log2 (0.0251189) */
#define MIN_QUA_ENER_MR122   (-32768) /* Q10 <-> 20*log10(0.0251189) */

    /* minimum allowed gain code prediction error: 32000/4096 = 7.8125 */
#define MAX_QUA_ENER         (  3037) /* Q10 <->    log2 (7.8125)    */
#define MAX_QUA_ENER_MR122   ( 18284) /* Q10 <-> 20*log10(7.8125)    */

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

    /*************************************************************************
     *
     * FUNCTION:  MR475_update_unq_pred()
     *
     * PURPOSE:   use optimum codebook gain and update "unquantized"
     *            gain predictor with the (bounded) prediction error
     *
     *************************************************************************/
    void
    MR475_update_unq_pred(
        gc_predState *pred_st, /* i/o: gain predictor state struct            */
        Word16 exp_gcode0,     /* i  : predicted CB gain (exponent),      Q0  */
        Word16 frac_gcode0,    /* i  : predicted CB gain (fraction),      Q15 */
        Word16 cod_gain_exp,   /* i  : optimum codebook gain (exponent),  Q0  */
        Word16 cod_gain_frac,  /* i  : optimum codebook gain (fraction),  Q15 */
        Flag   *pOverflow      /* o  : overflow indicator                     */
    );

    /*************************************************************************
     *
     * FUNCTION:  MR475_gain_quant()
     *
     * PURPOSE: Quantization of pitch and codebook gains for two subframes
     *          (using predicted codebook gain)
     *
     *************************************************************************/

    Word16
    MR475_gain_quant(              /* o  : index of quantization.                 */
        gc_predState *pred_st,     /* i/o: gain predictor state struct            */

        /* data from subframe 0 (or 2) */
        Word16 sf0_exp_gcode0,     /* i  : predicted CB gain (exponent),      Q0  */
        Word16 sf0_frac_gcode0,    /* i  : predicted CB gain (fraction),      Q15 */
        Word16 sf0_exp_coeff[],    /* i  : energy coeff. (5), exponent part,  Q0  */
        Word16 sf0_frac_coeff[],   /* i  : energy coeff. (5), fraction part,  Q15 */
        /*      (frac_coeff and exp_coeff computed in  */
        /*       calc_filt_energies())                 */
        Word16 sf0_exp_target_en,  /* i  : exponent of target energy,         Q0  */
        Word16 sf0_frac_target_en, /* i  : fraction of target energy,         Q15 */

        /* data from subframe 1 (or 3) */
        Word16 sf1_code_nosharp[], /* i  : innovative codebook vector (L_SUBFR)   */
        /*      (whithout pitch sharpening)            */
        Word16 sf1_exp_gcode0,     /* i  : predicted CB gain (exponent),      Q0  */
        Word16 sf1_frac_gcode0,    /* i  : predicted CB gain (fraction),      Q15 */
        Word16 sf1_exp_coeff[],    /* i  : energy coeff. (5), exponent part,  Q0  */
        Word16 sf1_frac_coeff[],   /* i  : energy coeff. (5), fraction part,  Q15 */
        /*      (frac_coeff and exp_coeff computed in  */
        /*       calc_filt_energies())                 */
        Word16 sf1_exp_target_en,  /* i  : exponent of target energy,         Q0  */
        Word16 sf1_frac_target_en, /* i  : fraction of target energy,         Q15 */

        Word16 gp_limit,           /* i  : pitch gain limit                       */

        Word16 *sf0_gain_pit,      /* o  : Pitch gain,                        Q14 */
        Word16 *sf0_gain_cod,      /* o  : Code gain,                         Q1  */

        Word16 *sf1_gain_pit,      /* o  : Pitch gain,                        Q14 */
        Word16 *sf1_gain_cod,      /* o  : Code gain,                         Q1  */
        Flag   *pOverflow          /* o  : overflow indicator                     */
    );
#ifdef __cplusplus
}
#endif

#endif  /* _QGAIN475_H_ */
