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



 Filename: ton_stab.h

------------------------------------------------------------------------------
 INCLUDE DESCRIPTION

     File             : ton_stab.h
     Purpose          : Tone stabilization routines

------------------------------------------------------------------------------
*/

#ifndef TON_STAB_H
#define TON_STAB_H
#define ton_stab_h "$Id $"

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
    /* state variable */
    typedef struct
    {
        /* counters */
        Word16 count;
        /* gain history Q11 */
        Word16 gp[N_FRAME];
    } tonStabState;

    /*----------------------------------------------------------------------------
    ; ENUMERATED TYPEDEF'S
    ----------------------------------------------------------------------------*/

    /*----------------------------------------------------------------------------
    ; STRUCTURES TYPEDEF'S
    ----------------------------------------------------------------------------*/

    /*----------------------------------------------------------------------------
    ; GLOBAL FUNCTION DEFINITIONS
    ; Function Prototype declaration
    ----------------------------------------------------------------------------*/
    Word16 ton_stab_init(tonStabState **st);
    /* initialize one instance of the pre processing state.
       Stores pointer to filter status struct in *st. This pointer has to
       be passed to ton_stab in each call.
       returns 0 on success
     */

    Word16 ton_stab_reset(tonStabState *st);
    /* reset of pre processing state (i.e. set state memory to zero)
       returns 0 on success
     */

    void ton_stab_exit(tonStabState **st);
    /* de-initialize pre processing state (i.e. free status struct)
       stores NULL in *st
     */

    Word16 check_lsp(tonStabState *st, /* i/o : State struct            */
                     Word16 *lsp,      /* i   : unquantized LSP's       */
                     Flag   *pOverflow
                    );

    Word16 check_gp_clipping(tonStabState *st, /* i/o : State struct          */
                             Word16 g_pitch,   /* i   : pitch gain            */
                             Flag  *pOverflow
                            );

    void update_gp_clipping(tonStabState *st, /* i/o : State struct            */
                            Word16 g_pitch,   /* i   : pitch gain              */
                            Flag  *pOverflow
                           );

    /*----------------------------------------------------------------------------
    ; END
    ----------------------------------------------------------------------------*/
#ifdef __cplusplus
}
#endif

#endif /* _TON_STAB_H_ */




