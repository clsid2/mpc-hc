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

 Filename: lsp.h

------------------------------------------------------------------------------
 INCLUDE DESCRIPTION

 This file contains all the constant definitions and prototype definitions
 needed by the lsp.c

------------------------------------------------------------------------------
*/

/*----------------------------------------------------------------------------
; CONTINUE ONLY IF NOT ALREADY DEFINED
----------------------------------------------------------------------------*/
#ifndef lsp_h
#define lsp_h "$Id $"

/*----------------------------------------------------------------------------
; INCLUDES
----------------------------------------------------------------------------*/
#include "typedef.h"
#include "q_plsf.h"
#include "mode.h"

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

        /* Past LSPs */
        Word16 lsp_old[M];
        Word16 lsp_old_q[M];

        /* Quantization state */
        Q_plsfState *qSt;

    } lspState;

    /*----------------------------------------------------------------------------
    ; GLOBAL FUNCTION DEFINITIONS
    ; Function Prototype declaration
    ----------------------------------------------------------------------------*/
    /*
    **************************************************************************
    *
    *  Function    : lsp_init
    *  Purpose     : Allocates memory and initializes state variables
    *  Description : Stores pointer to filter status struct in *st. This
    *                pointer has to be passed to lsp in each call.
    *  Returns     : 0 on success
    *
    **************************************************************************
    */
    OSCL_IMPORT_REF Word16 lsp_init(lspState **st);

    /*
    **************************************************************************
    *
    *  Function    : lsp_reset
    *  Purpose     : Resets state memory
    *  Returns     : 0 on success
    *
    **************************************************************************
    */
    OSCL_IMPORT_REF Word16 lsp_reset(lspState *st);

    /*
    **************************************************************************
    *
    *  Function    : lsp_exit
    *  Purpose     : The memory used for state memory is freed
    *  Description : Stores NULL in *st
    *
    **************************************************************************
    */
    OSCL_IMPORT_REF void lsp_exit(lspState **st);

    /*
    **************************************************************************
    *
    *  Function    : lsp
    *  Purpose     : Conversion from LP coefficients to LSPs.
    *                Quantization of LSPs.
    *  Description : Generates 2 sets of LSPs from 2 sets of
    *                LP coefficients for mode 12.2. For the other
    *                modes 1 set of LSPs is generated from 1 set of
    *                LP coefficients. These LSPs are quantized with
    *                Matrix/Vector quantization (depending on the mode)
    *                and interpolated for the subframes not yet having
    *                their own LSPs.
    *
    **************************************************************************
    */
    OSCL_IMPORT_REF void lsp(lspState *st,  /* i/o : State struct                 */
                             enum Mode req_mode, /* i   : requested coder mode                    */
                             enum Mode used_mode,/* i   : used coder mode                         */
                             Word16 az[],        /* i/o : interpolated LP parameters Q12          */
                             Word16 azQ[],       /* o   : quantization interpol. LP parameters Q12*/
                             Word16 lsp_new[],   /* o   : new lsp vector                          */
                             Word16 **anap,      /* o   : analysis parameters                     */
                             Flag   *pOverflow   /* o   : Flag set when overflow occurs           */
                            );

    /*----------------------------------------------------------------------------
    ; END
    ----------------------------------------------------------------------------*/
#ifdef __cplusplus
}
#endif

#endif /* _LSP_H_ */


