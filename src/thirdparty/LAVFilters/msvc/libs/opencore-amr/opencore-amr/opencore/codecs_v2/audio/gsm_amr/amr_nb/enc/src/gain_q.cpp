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



 Filename: gain_q.cpp

------------------------------------------------------------------------------
 MODULE DESCRIPTION

    Quantazation of gains
------------------------------------------------------------------------------
*/

/*----------------------------------------------------------------------------
; INCLUDES
----------------------------------------------------------------------------*/
#include "gain_q.h"
#include "typedef.h"
#include "basic_op.h"
#include "qua_gain.h"
#include "cnst.h"
#include "mode.h"
#include "g_code.h"
#include "q_gain_c.h"
#include "calc_en.h"
#include "qgain795.h"
#include "qgain475.h"
#include "set_zero.h"
#include "oscl_mem.h"


/*----------------------------------------------------------------------------
; MACROS
; Define module specific macros here
----------------------------------------------------------------------------*/

/*----------------------------------------------------------------------------
; DEFINES
; Include all pre-processor statements here. Include conditional
; compile variables also.
----------------------------------------------------------------------------*/
#define NPRED 4  /* number of prediction taps */

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
 FUNCTION NAME: gainQuant_init
------------------------------------------------------------------------------
 INPUT AND OUTPUT DEFINITIONS

 Inputs:
    st -- double pointer to gainQuantState

 Outputs:
    st -- double ponter to gainQuantState

 Returns:
    -1 if an error occurs during memory initialization
     0 if OK

 Global Variables Used:
    None

 Local Variables Needed:
    None

------------------------------------------------------------------------------
 FUNCTION DESCRIPTION

    Allocates state memory and initializes state memory

------------------------------------------------------------------------------
 REQUIREMENTS

 None

------------------------------------------------------------------------------
 REFERENCES

 gain_q.c, UMTS GSM AMR speech codec, R99 - Version 3.2.0, March 2, 2001

------------------------------------------------------------------------------
 PSEUDO-CODE


------------------------------------------------------------------------------
 CAUTION [optional]
 [State any special notes, constraints or cautions for users of this function]

------------------------------------------------------------------------------
*/

Word16 gainQuant_init(gainQuantState **state)
{
    gainQuantState* s;

    if (state == (gainQuantState **) NULL)
    {
        /* fprintf(stderr, "gainQuant_init: invalid parameter\n"); */
        return -1;
    }
    *state = NULL;

    /* allocate memory */
    if ((s = (gainQuantState *) oscl_malloc(sizeof(gainQuantState))) == NULL)
    {
        /* fprintf(stderr, "gainQuant_init: can not malloc state structure\n"); */
        return -1;
    }

    s->gain_idx_ptr = NULL;

    s->adaptSt = NULL;

    /* Init sub states */
    if (gc_pred_reset(&s->gc_predSt)
            || gc_pred_reset(&s->gc_predUnqSt)
            || gain_adapt_init(&s->adaptSt))
    {
        gainQuant_exit(&s);
        return -1;
    }

    gainQuant_reset(s);
    *state = s;

    return 0;
}

/*
------------------------------------------------------------------------------
 FUNCTION NAME: gainQuant_reset
------------------------------------------------------------------------------
 INPUT AND OUTPUT DEFINITIONS

 Inputs:
    st -- double pointer to gainQuantState

 Outputs:
    st -- double ponter to gainQuantState

 Returns:
    -1 if an error occurs
     0 if OK

 Global Variables Used:
    None

 Local Variables Needed:
    None

------------------------------------------------------------------------------
 FUNCTION DESCRIPTION

    Initializes state memory to zero
------------------------------------------------------------------------------
 REQUIREMENTS

 None

------------------------------------------------------------------------------
 REFERENCES

 gain_q.c, UMTS GSM AMR speech codec, R99 - Version 3.2.0, March 2, 2001

------------------------------------------------------------------------------
 PSEUDO-CODE


------------------------------------------------------------------------------
 CAUTION [optional]
 [State any special notes, constraints or cautions for users of this function]

------------------------------------------------------------------------------
*/

Word16 gainQuant_reset(gainQuantState *state)
{

    if (state == (gainQuantState *) NULL)
    {
        /* fprintf(stderr, "gainQuant_reset: invalid parameter\n"); */
        return -1;
    }

    state->sf0_exp_gcode0 = 0;
    state->sf0_frac_gcode0 = 0;
    state->sf0_exp_target_en = 0;
    state->sf0_frac_target_en = 0;


    oscl_memset((void *)state->sf0_exp_coeff,  0, 5*sizeof(*state->sf0_exp_coeff));
    oscl_memset((void *)state->sf0_frac_coeff, 0, 5*sizeof(*state->sf0_frac_coeff));

    state->gain_idx_ptr = NULL;

    gc_pred_reset(&(state->gc_predSt));
    gc_pred_reset(&(state->gc_predUnqSt));
    gain_adapt_reset(state->adaptSt);

    return 0;
}

/*
------------------------------------------------------------------------------
 FUNCTION NAME: gainQuant_exit
------------------------------------------------------------------------------
 INPUT AND OUTPUT DEFINITIONS

 Inputs:
    st -- double pointer to gainQuantState

 Outputs:
    None

 Returns:
    None

 Global Variables Used:
    None

 Local Variables Needed:
    None

------------------------------------------------------------------------------
 FUNCTION DESCRIPTION

    The memory used for state memory is freed
------------------------------------------------------------------------------
 REQUIREMENTS

 None

------------------------------------------------------------------------------
 REFERENCES

 gain_q.c, UMTS GSM AMR speech codec, R99 - Version 3.2.0, March 2, 2001

------------------------------------------------------------------------------
 PSEUDO-CODE


------------------------------------------------------------------------------
 CAUTION [optional]
 [State any special notes, constraints or cautions for users of this function]

------------------------------------------------------------------------------
*/

void gainQuant_exit(gainQuantState **state)
{
    if (state == NULL || *state == NULL)
        return;

    gain_adapt_exit(&(*state)->adaptSt);

    /* deallocate memory */
    oscl_free(*state);
    *state = NULL;

    return;
}



/*
------------------------------------------------------------------------------
 FUNCTION NAME: gainQuant
------------------------------------------------------------------------------
 INPUT AND OUTPUT DEFINITIONS

 Inputs:
    st   -- pointer to gainQuantState
    mode -- enum Mode -- coder mode
    res  -- Word16 array -- LP residual,                 Q0
    exc  -- Word16 array -- LTP excitation (unfiltered), Q0
    code -- Word16 array -- CB innovation (unfiltered),  Q13
                            (unsharpened for MR475)
    xn  -- Word16 array -- Target vector.
    xn2 -- Word16 array -- Target vector.
    y1  -- Word16 array -- Adaptive codebook.
    Y2  -- Word16 array -- Filtered innovative vector.
    g_coeff -- Word16 array -- Correlations <xn y1> <y1 y1>
                               Compute in G_pitch().

    even_subframe -- Word16 -- even subframe indicator flag
    gp_limit -- Word16 -- pitch gain limit
    gain_pit -- Word16 Pointer -- Pitch gain.

 Outputs:
    st -- pointer to gainQuantState
    sf0_gain_pit -- Word16 Pointer -- Pitch gain sf 0.   MR475
    sf0_gain_cod -- Word16 Pointer -- Code gain sf 0.    MR475
    gain_pit -- Word16 Pointer -- Pitch gain.
    gain_cod -- Word16 Pointer -- Code gain.
                                  MR475: gain_* unquantized in even
                                  subframes, quantized otherwise

    anap -- Word16 Double Pointer -- Index of quantization

    pOverflow -- Flag Pointer -- overflow indicator

 Returns:
    Zero

 Global Variables Used:
    None

 Local Variables Needed:
    None

------------------------------------------------------------------------------
 FUNCTION DESCRIPTION

    Quantazation of gains

------------------------------------------------------------------------------
 REQUIREMENTS

 None

------------------------------------------------------------------------------
 REFERENCES

 gain_q.c, UMTS GSM AMR speech codec, R99 - Version 3.2.0, March 2, 2001

------------------------------------------------------------------------------
 PSEUDO-CODE


------------------------------------------------------------------------------
 CAUTION [optional]
 [State any special notes, constraints or cautions for users of this function]

------------------------------------------------------------------------------
*/



void gainQuant(
    gainQuantState *st,   /* i/o : State struct                      */
    enum Mode mode,       /* i   : coder mode                        */
    Word16 res[],         /* i   : LP residual,                 Q0   */
    Word16 exc[],         /* i   : LTP excitation (unfiltered), Q0   */
    Word16 code[],        /* i   : CB innovation (unfiltered),  Q13  */
    /*       (unsharpened for MR475)           */
    Word16 xn[],          /* i   : Target vector.                    */
    Word16 xn2[],         /* i   : Target vector.                    */
    Word16 y1[],          /* i   : Adaptive codebook.                */
    Word16 Y2[],          /* i   : Filtered innovative vector.       */
    Word16 g_coeff[],     /* i   : Correlations <xn y1> <y1 y1>      */
    /*       Compute in G_pitch().             */
    Word16 even_subframe, /* i   : even subframe indicator flag      */
    Word16 gp_limit,      /* i   : pitch gain limit                  */
    Word16 *sf0_gain_pit, /* o   : Pitch gain sf 0.   MR475          */
    Word16 *sf0_gain_cod, /* o   : Code gain sf 0.    MR475          */
    Word16 *gain_pit,     /* i/o : Pitch gain.                       */
    Word16 *gain_cod,     /* o   : Code gain.                        */
    /*       MR475: gain_* unquantized in even */
    /*       subframes, quantized otherwise    */
    Word16 **anap,        /* o   : Index of quantization             */
    CommonAmrTbls* common_amr_tbls, /* i : ptr to struct of tbl ptrs */
    Flag   *pOverflow     /* o   : overflow indicator                */
)
{
    Word16 exp_gcode0;
    Word16 frac_gcode0;
    Word16 qua_ener_MR122;
    Word16 qua_ener;
    Word16 frac_coeff[5];
    Word16 exp_coeff[5];
    Word16 exp_en;
    Word16 frac_en;
    Word16 cod_gain_exp;
    Word16 cod_gain_frac;
    Word16 temp;

    if (mode == MR475)
    {
        if (even_subframe != 0)
        {
            /* save position in output parameter stream and current
               state of codebook gain predictor */
            st->gain_idx_ptr = (*anap)++;

//            gc_pred_copy(&(st->gc_predSt), &(st->gc_predUnqSt));

            oscl_memcpy(st->gc_predUnqSt.past_qua_en,
                        st->gc_predSt.past_qua_en,
                        NPRED*sizeof(Word16));
            oscl_memcpy(st->gc_predUnqSt.past_qua_en_MR122,
                        st->gc_predSt.past_qua_en_MR122,
                        NPRED*sizeof(Word16));


            /* predict codebook gain (using "unquantized" predictor)*/
            /* (note that code[] is unsharpened in MR475)           */
            gc_pred(
                &(st->gc_predUnqSt),
                mode,
                code,
                &st->sf0_exp_gcode0,
                &st->sf0_frac_gcode0,
                &exp_en,
                &frac_en,
                pOverflow);

            /* calculate energy coefficients for quantization
               and store them in state structure (will be used
               in next subframe when real quantizer is run) */
            calc_filt_energies(
                mode,
                xn,
                xn2,
                y1,
                Y2,
                g_coeff,
                st->sf0_frac_coeff,
                st->sf0_exp_coeff,
                &cod_gain_frac,
                &cod_gain_exp,
                pOverflow);

            /* store optimum codebook gain (Q1) */
            temp = cod_gain_exp + 1;

            *gain_cod =
                shl(
                    cod_gain_frac,
                    temp,
                    pOverflow);

            calc_target_energy(
                xn,
                &st->sf0_exp_target_en,
                &st->sf0_frac_target_en,
                pOverflow);

            /* calculate optimum codebook gain and update
               "unquantized" predictor                    */
            MR475_update_unq_pred(
                &(st->gc_predUnqSt),
                st->sf0_exp_gcode0,
                st->sf0_frac_gcode0,
                cod_gain_exp,
                cod_gain_frac,
                pOverflow);

            /* the real quantizer is not run here... */
        }
        else
        {
            /* predict codebook gain (using "unquantized" predictor) */
            /* (note that code[] is unsharpened in MR475)            */
            gc_pred(
                &(st->gc_predUnqSt),
                mode,
                code,
                &exp_gcode0,
                &frac_gcode0,
                &exp_en,
                &frac_en,
                pOverflow);

            /* calculate energy coefficients for quantization */
            calc_filt_energies(
                mode,
                xn,
                xn2,
                y1,
                Y2,
                g_coeff,
                frac_coeff,
                exp_coeff,
                &cod_gain_frac,
                &cod_gain_exp,
                pOverflow);

            calc_target_energy(
                xn,
                &exp_en,
                &frac_en,
                pOverflow);

            /* run real (4-dim) quantizer and update real gain predictor */
            *st->gain_idx_ptr =
                MR475_gain_quant(
                    &(st->gc_predSt),
                    st->sf0_exp_gcode0,
                    st->sf0_frac_gcode0,
                    st->sf0_exp_coeff,
                    st->sf0_frac_coeff,
                    st->sf0_exp_target_en,
                    st->sf0_frac_target_en,
                    code,
                    exp_gcode0,
                    frac_gcode0,
                    exp_coeff,
                    frac_coeff,
                    exp_en,
                    frac_en,
                    gp_limit,
                    sf0_gain_pit,
                    sf0_gain_cod,
                    gain_pit,
                    gain_cod,
                    pOverflow);
        }
    }
    else
    {
        /*-------------------------------------------------------------------*
         *  predict codebook gain and quantize                               *
         *  (also compute normalized CB innovation energy for MR795)         *
         *-------------------------------------------------------------------*/
        gc_pred(
            &(st->gc_predSt),
            mode,
            code,
            &exp_gcode0,
            &frac_gcode0,
            &exp_en,
            &frac_en,
            pOverflow);

        if (mode == MR122)
        {
            *gain_cod =
                G_code(
                    xn2,
                    Y2,
                    pOverflow);

            *(*anap)++ =
                q_gain_code(
                    mode,
                    exp_gcode0,
                    frac_gcode0,
                    gain_cod,
                    &qua_ener_MR122,
                    &qua_ener,
                    common_amr_tbls->qua_gain_code_ptr,
                    pOverflow);
        }
        else
        {
            /* calculate energy coefficients for quantization */
            calc_filt_energies(
                mode,
                xn,
                xn2,
                y1,
                Y2,
                g_coeff,
                frac_coeff,
                exp_coeff,
                &cod_gain_frac,
                &cod_gain_exp,
                pOverflow);

            if (mode == MR795)
            {
                MR795_gain_quant(
                    st->adaptSt,
                    res,
                    exc,
                    code,
                    frac_coeff,
                    exp_coeff,
                    exp_en,
                    frac_en,
                    exp_gcode0,
                    frac_gcode0,
                    L_SUBFR,
                    cod_gain_frac,
                    cod_gain_exp,
                    gp_limit,
                    gain_pit,
                    gain_cod,
                    &qua_ener_MR122,
                    &qua_ener,
                    anap,
                    common_amr_tbls,
                    pOverflow);
            }
            else
            {
                *(*anap)++ =
                    Qua_gain(
                        mode,
                        exp_gcode0,
                        frac_gcode0,
                        frac_coeff,
                        exp_coeff,
                        gp_limit,
                        gain_pit,
                        gain_cod,
                        &qua_ener_MR122,
                        &qua_ener,
                        common_amr_tbls,
                        pOverflow);
            }
        }

        /*------------------------------------------------------------------*
         *  update table of past quantized energies                         *
         *  ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~                         *
         *  st->past_qua_en(Q10) = 20 * Log10(qua_gain_code) / constant     *
         *                       = Log2(qua_gain_code)                      *
         *                       = qua_ener                                 *
         *                                           constant = 20*Log10(2) *
         *------------------------------------------------------------------*/
        gc_pred_update(
            &(st->gc_predSt),
            qua_ener_MR122,
            qua_ener);
    }

    return;
}
