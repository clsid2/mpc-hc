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



 Filename: gain_q.h

------------------------------------------------------------------------------
 INCLUDE DESCRIPTION

 This file contains all the constant definitions and prototype definitions
 needed by the file, gain_q.c

------------------------------------------------------------------------------
*/

/*----------------------------------------------------------------------------
; CONTINUE ONLY IF NOT ALREADY DEFINED
----------------------------------------------------------------------------*/
#ifndef gain_q_h
#define gain_q_h "$Id $"

/*----------------------------------------------------------------------------
; INCLUDES
----------------------------------------------------------------------------*/
#include "typedef.h"
#include "mode.h"
#include "gc_pred.h"
#include "g_adapt.h"
#include "get_const_tbls.h"

/*--------------------------------------------------------------------------*/
#ifdef __cplusplus
extern "C"
{
#endif

    /*----------------------------------------------------------------------------
    ; MACROS
    ; Define module specific macros here
    ----------------------------------------------------------------------------*/

    /*----------------------------------------------------------------------------
    ; DEFINES
    ; Include all pre-processor statements here.
    ----------------------------------------------------------------------------*/

    /*----------------------------------------------------------------------------
    ; EXTERNAL VARIABLES REFERENCES
    ; Declare variables used in this module but defined elsewhere
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
        Word16 sf0_exp_gcode0;
        Word16 sf0_frac_gcode0;
        Word16 sf0_exp_target_en;
        Word16 sf0_frac_target_en;
        Word16 sf0_exp_coeff[5];
        Word16 sf0_frac_coeff[5];
        Word16 *gain_idx_ptr;

        gc_predState     gc_predSt;
        gc_predState     gc_predUnqSt;
        GainAdaptState   *adaptSt;
    } gainQuantState;

    /*----------------------------------------------------------------------------
    ; GLOBAL FUNCTION DEFINITIONS
    ; Function Prototype declaration
    ----------------------------------------------------------------------------*/
    Word16 gainQuant_init(gainQuantState **st);
    /* initialize one instance of the pre processing state.
       Stores pointer to filter status struct in *st. This pointer has to
       be passed to gainQuant in each call.
       returns 0 on success
     */
    Word16 gainQuant_reset(gainQuantState *st);
    /* reset of pre processing state (i.e. set state memory to zero)
       returns 0 on success
     */
    void gainQuant_exit(gainQuantState **st);
    /* de-initialize pre processing state (i.e. free status struct)
       stores NULL in *st
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
    );

    /*----------------------------------------------------------------------------
    ; END
    ----------------------------------------------------------------------------*/
#ifdef __cplusplus
}
#endif

#endif /* gain_q_h */


