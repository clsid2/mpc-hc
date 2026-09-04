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



 Filename: dec_gain.cpp
 Functions: dec_gain

------------------------------------------------------------------------------
*/


/*----------------------------------------------------------------------------
; INCLUDES
----------------------------------------------------------------------------*/

#include "dec_gain.h"
#include "typedef.h"
#include "mode.h"
#include "cnst.h"
#include "pow2.h"
#include "log2.h"
#include "gc_pred.h"
#include "basic_op.h"
#include "qua_gain_tbl.h"
#include "qgain475_tab.h"

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
 FUNCTION NAME: dec_gain
------------------------------------------------------------------------------
 INPUT AND OUTPUT DEFINITIONS

 Inputs:
    pred_state = pointer to MA predictor state of type gc_predState
    index = AMR mode of type enum Mode
    code[] = pointer to innovative vector of type Word16
    evenSubfr = Flag for even subframes of type Word16
    pOverflow = pointer to overflow flag


 Outputs:
    pred_state = pointer to MA predictor state of type gc_predState
    gain_pit = pointer to pitch gain of type Word16
    gain_cod = pointer to code gain of type Word16

 Returns:
    None.

 Global Variables Used:
    None.

 Local Variables Needed:
    None.

------------------------------------------------------------------------------
 FUNCTION DESCRIPTION

      File             : dec_gain.c
      Purpose          : Decode the pitch and codebook gains

------------------------------------------------------------------------------
 REQUIREMENTS

 None.

------------------------------------------------------------------------------
 REFERENCES

 agc.c, UMTS GSM AMR speech codec, R99 - Version 3.2.0, March 2, 2001

------------------------------------------------------------------------------
 PSEUDO-CODE








------------------------------------------------------------------------------
 CAUTION [optional]
 [State any special notes, constraints or cautions for users of this function]

------------------------------------------------------------------------------
*/


void Dec_gain(
    gc_predState *pred_state, /* i/o: MA predictor state           */
    enum Mode mode,           /* i  : AMR mode                     */
    Word16 index,             /* i  : index of quantization.       */
    Word16 code[],            /* i  : Innovative vector.           */
    Word16 evenSubfr,         /* i  : Flag for even subframes      */
    Word16 * gain_pit,        /* o  : Pitch gain.                  */
    Word16 * gain_cod,        /* o  : Code gain.                   */
    CommonAmrTbls* common_amr_tbls, /* i : ptr to struct of tbls ptrs */
    Flag   * pOverflow
)
{
    const Word16 *p;
    Word16 frac;
    Word16 gcode0;
    Word16 exp;
    Word16 qua_ener;
    Word16 qua_ener_MR122;
    Word16 g_code;
    Word32 L_tmp;
    Word16 temp1;
    Word16 temp2;

    /* Read the quantized gains (table depends on mode) */
    index = shl(index, 2, pOverflow);

    if (mode == MR102 || mode == MR74 || mode == MR67)
    {
        p = &(common_amr_tbls->table_gain_highrates_ptr[index]);

        *gain_pit = *p++;
        g_code = *p++;
        qua_ener_MR122 = *p++;
        qua_ener = *p;
    }
    else
    {
        if (mode == MR475)
        {
            index += (1 ^ evenSubfr) << 1; /* evenSubfr is 0 or 1 */

            if (index > (MR475_VQ_SIZE*4 - 2))
            {
                index = (MR475_VQ_SIZE * 4 - 2); /* avoid possible buffer overflow */
            }

            p = &table_gain_MR475[index];

            *gain_pit = *p++;
            g_code = *p++;

            /*---------------------------------------------------------*
             *  calculate predictor update values (not stored in 4.75  *
             *  quantizer table to save space):                        *
             *  ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~  *
             *                                                         *
             *   qua_ener       = log2(g)                              *
             *   qua_ener_MR122 = 20*log10(g)                          *
             *---------------------------------------------------------*/

            /* Log2(x Q12) = log2(x) + 12 */
            temp1 = g_code;
            Log2(temp1, &exp, &frac, pOverflow);
            exp -= 12;

            temp1 = shr_r(frac, 5, pOverflow);
            temp2 = shl(exp, 10, pOverflow);
            qua_ener_MR122 = add_16(temp1, temp2, pOverflow);

            /* 24660 Q12 ~= 6.0206 = 20*log10(2) */
            L_tmp = Mpy_32_16(exp, frac, 24660, pOverflow);
            L_tmp = L_shl(L_tmp, 13, pOverflow);
            qua_ener = pv_round(L_tmp, pOverflow);
            /* Q12 * Q0 = Q13 -> Q10 */
        }
        else
        {
            p = &(common_amr_tbls->table_gain_lowrates_ptr[index]);

            *gain_pit = *p++;
            g_code = *p++;
            qua_ener_MR122 = *p++;
            qua_ener = *p;
        }
    }

    /*-------------------------------------------------------------------*
     *  predict codebook gain                                            *
     *  ~~~~~~~~~~~~~~~~~~~~~                                            *
     *  gc0     = Pow2(int(d)+frac(d))                                   *
     *          = 2^exp + 2^frac                                         *
     *                                                                   *
     *  gcode0 (Q14) = 2^14*2^frac = gc0 * 2^(14-exp)                    *
     *-------------------------------------------------------------------*/

    gc_pred(pred_state, mode, code, &exp, &frac, NULL, NULL, pOverflow);

    gcode0 = (Word16) Pow2(14, frac, pOverflow);

    /*------------------------------------------------------------------*
     *  read quantized gains, update table of past quantized energies   *
     *  ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~   *
     *  st->past_qua_en(Q10) = 20 * Log10(g_fac) / constant             *
     *                       = Log2(g_fac)                              *
     *                       = qua_ener                                 *
     *                                           constant = 20*Log10(2) *
     *------------------------------------------------------------------*/

    L_tmp = L_mult(g_code, gcode0, pOverflow);
    temp1 = 10 - exp;
    L_tmp = L_shr(L_tmp, temp1, pOverflow);
    *gain_cod = (Word16)(L_tmp >> 16);

    /* update table of past quantized energies */

    gc_pred_update(pred_state, qua_ener_MR122, qua_ener);

    return;
}







