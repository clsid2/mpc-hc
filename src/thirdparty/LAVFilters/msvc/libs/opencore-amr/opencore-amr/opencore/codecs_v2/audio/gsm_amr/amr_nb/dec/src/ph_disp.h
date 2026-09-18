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



 Filename: ph_disp.h

------------------------------------------------------------------------------
 INCLUDE DESCRIPTION

 This file contains all the constant definitions and prototype definitions
 needed by the Phase dispersion of excitation signal ph_disp() function.

------------------------------------------------------------------------------
*/

#ifndef PH_DISP_H
#define PH_DISP_H "$Id $"

/*----------------------------------------------------------------------------
; INCLUDES
----------------------------------------------------------------------------*/
#include    "typedef.h"
#include    "mode.h"
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
#define PHDGAINMEMSIZE 5
#define PHDTHR1LTP     9830  /* 0.6 in Q14 */
#define PHDTHR2LTP     14746 /* 0.9 in Q14 */
#define ONFACTPLUS1    16384 /* 2.0 in Q13   */
#define ONLENGTH 2

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
        Word16 gainMem[PHDGAINMEMSIZE];
        Word16 prevState;
        Word16 prevCbGain;
        Word16 lockFull;
        Word16 onset;
    } ph_dispState;

    /*----------------------------------------------------------------------------
    ; GLOBAL FUNCTION DEFINITIONS
    ; Function Prototype declaration
    ----------------------------------------------------------------------------*/

    /*----------------------------------------------------------------------------
    ;
    ;  Function:   ph_disp_reset
    ;  Purpose:    Initializes state memory
    ;
    ----------------------------------------------------------------------------*/
    Word16 ph_disp_reset(ph_dispState *state);

    /*----------------------------------------------------------------------------
    ;
    ;  Function:   ph_disp_exit
    ;  Purpose:    The memory used for state memory is freed
    ;
    ----------------------------------------------------------------------------*/
    void ph_disp_exit(ph_dispState **state);

    /*----------------------------------------------------------------------------
    ;
    ;  Function:   ph_disp_lock
    ;  Purpose:    mark phase dispersion as locked in state struct
    ;
    ----------------------------------------------------------------------------*/
    void ph_disp_lock(ph_dispState *state);

    /*----------------------------------------------------------------------------
    ;
    ;  Function:   ph_disp_release
    ;  Purpose:    mark phase dispersion as unlocked in state struct
    ;
    ----------------------------------------------------------------------------*/

    void ph_disp_release(ph_dispState *state);

    /*----------------------------------------------------------------------------
    ;
    ;  Function:   ph_disp
    ;  Purpose:    perform phase dispersion according to the specified codec
    ;              mode and computes total excitation for synthesis part
    ;              if decoder
    ;
    ----------------------------------------------------------------------------*/

    void ph_disp(
        ph_dispState *state,    /* i/o     : State struct                       */
        enum Mode mode,         /* i       : codec mode                         */
        Word16 x[],             /* i/o Q0  : in:  LTP excitation signal         */
        /*           out: total excitation signal       */
        Word16 cbGain,          /* i   Q1  : Codebook gain                      */
        Word16 ltpGain,         /* i   Q14 : LTP gain                           */
        Word16 inno[],          /* i/o Q13 : Innovation vector (Q12 for 12.2)   */
        Word16 pitch_fac,       /* i   Q14 : pitch factor used to scale the
                                         LTP excitation (Q13 for 12.2)      */
        Word16 tmp_shift,       /* i   Q0  : shift factor applied to sum of
                                         scaled LTP ex & innov. before
                                         rounding                           */
        CommonAmrTbls* common_amr_tbls, /* i : ptr to struct of table ptrs      */
        Flag   *pOverflow       /* i/o     : oveflow indicator                  */
    );

    /*----------------------------------------------------------------------------
    ; END
    ----------------------------------------------------------------------------*/
#ifdef __cplusplus
}
#endif

#endif /* _PH_DISP_H_ */

