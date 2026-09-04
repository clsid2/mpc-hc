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



 Filename: calc_en.h

------------------------------------------------------------------------------
 INCLUDE DESCRIPTION

      File             : calc_en.h
      Purpose          : calculation of energy coefficients for quantizers

------------------------------------------------------------------------------
*/

#ifndef _CALC_EN_H_
#define _CALC_EN_H_
#define calc_en_h "$Id $"

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


    /*----------------------------------------------------------------------------
    ; GLOBAL FUNCTION DEFINITIONS
    ; [List function prototypes here]
    ----------------------------------------------------------------------------*/
    /*
     * FUNCTION: calc_unfilt_energies
     *
     * PURPOSE:  calculation of several energy coefficients for unfiltered
     *           excitation signals and the LTP coding gain
     *
     *       frac_en[0]*2^exp_en[0] = <res res>   // LP residual energy
     *       frac_en[1]*2^exp_en[1] = <exc exc>   // LTP residual energy
     *       frac_en[2]*2^exp_en[2] = <exc code>  // LTP/CB innovation dot product
     *       frac_en[3]*2^exp_en[3] = <lres lres> // LTP residual energy
     *                                            // (lres = res - gain_pit*exc)
     *       ltpg = log2(LP_res_en / LTP_res_en)
     */
    void
    calc_unfilt_energies(
        Word16 res[],     /* i  : LP residual,                               Q0  */
        Word16 exc[],     /* i  : LTP excitation (unfiltered),               Q0  */
        Word16 code[],    /* i  : CB innovation (unfiltered),                Q13 */
        Word16 gain_pit,  /* i  : pitch gain,                                Q14 */
        Word16 L_subfr,   /* i  : Subframe length                                */

        Word16 frac_en[], /* o  : energy coefficients (3), fraction part,    Q15 */
        Word16 exp_en[],  /* o  : energy coefficients (3), exponent part,    Q0  */
        Word16 *ltpg,     /* o  : LTP coding gain (log2()),                  Q13 */
        Flag   *pOverflow
    );

    /*
     * FUNCTION: calc_filt_energies
     *
     * PURPOSE:  calculation of several energy coefficients for filtered
     *           excitation signals
     *
     *     Compute coefficients need for the quantization and the optimum
     *     codebook gain gcu (for MR475 only).
     *
     *      coeff[0] =    y1 y1
     *      coeff[1] = -2 xn y1
     *      coeff[2] =    y2 y2
     *      coeff[3] = -2 xn y2
     *      coeff[4] =  2 y1 y2
     *
     *
     *      gcu = <xn2, y2> / <y2, y2> (0 if <xn2, y2> <= 0)
     *
     *     Product <y1 y1> and <xn y1> have been computed in G_pitch() and
     *     are in vector g_coeff[].
     */
    void
    calc_filt_energies(
        enum Mode mode,     /* i  : coder mode                                   */
        Word16 xn[],        /* i  : LTP target vector,                       Q0  */
        Word16 xn2[],       /* i  : CB target vector,                        Q0  */
        Word16 y1[],        /* i  : Adaptive codebook,                       Q0  */
        Word16 Y2[],        /* i  : Filtered innovative vector,              Q12 */
        Word16 g_coeff[],   /* i  : Correlations <xn y1> <y1 y1>                 */
        /*      computed in G_pitch()                        */

        Word16 frac_coeff[],/* o  : energy coefficients (5), fraction part,  Q15 */
        Word16 exp_coeff[], /* o  : energy coefficients (5), exponent part,  Q0  */
        Word16 *cod_gain_frac,/* o: optimum codebook gain (fraction part),   Q15 */
        Word16 *cod_gain_exp, /* o: optimum codebook gain (exponent part),   Q0  */
        Flag   *pOverflow
    );

    /*
     * FUNCTION: calc_target_energy
     *
     * PURPOSE:  calculation of target energy
     *
     *      en = <xn, xn>
     */
    void
    calc_target_energy(
        Word16 xn[],     /* i: LTP target vector,                       Q0  */
        Word16 *en_exp,  /* o: optimum codebook gain (exponent part),   Q0  */
        Word16 *en_frac,  /* o: optimum codebook gain (fraction part),   Q15 */
        Flag   *pOverflow
    );

#ifdef __cplusplus
}
#endif

#endif  /* _CALC_EN_H_ */





