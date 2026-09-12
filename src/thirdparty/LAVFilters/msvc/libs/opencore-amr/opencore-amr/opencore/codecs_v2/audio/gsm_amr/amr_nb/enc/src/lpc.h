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



 Filename: lpc.h

------------------------------------------------------------------------------
 INCLUDE DESCRIPTION

      File             : lpc.h
      Purpose          : 2 LP analyses centered at 2nd and 4th subframe
                         for mode 12.2. For all other modes a
                         LP analysis centered at 4th subframe is
                         performed.

------------------------------------------------------------------------------
*/

#ifndef _LPC_H_
#define _LPC_H_
#define lpc_h "$Id $"

/*----------------------------------------------------------------------------
; INCLUDES
----------------------------------------------------------------------------*/
#include "typedef.h"
#include "levinson.h"
#include "mode.h"
#include "get_const_tbls.h"


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
    typedef struct
    {
        LevinsonState *levinsonSt;
    } lpcState;


    /*----------------------------------------------------------------------------
    ; GLOBAL FUNCTION DEFINITIONS
    ; [List function prototypes here]
    ----------------------------------------------------------------------------*/
    Word16 lpc_init(lpcState **st);
    /* initialize one instance of the pre processing state.
       Stores pointer to filter status struct in *st. This pointer has to
       be passed to lpc in each call.
       returns 0 on success
     */

    Word16 lpc_reset(lpcState *st);
    /* reset of pre processing state (i.e. set state memory to zero)
       returns 0 on success
     */
    void lpc_exit(lpcState **st);
    /* de-initialize pre processing state (i.e. free status struct)
       stores NULL in *st
     */

    void lpc(
        lpcState *st,     /* i/o: State struct                */
        enum Mode mode,   /* i  : coder mode                  */
        Word16 x[],       /* i  : Input signal           Q15  */
        Word16 x_12k2[],  /* i  : Input signal (EFR)     Q15  */
        Word16 a[],       /* o  : predictor coefficients Q12  */
        CommonAmrTbls* common_amr_tbls, /* i : ptr to struct with table ptrs */
        Flag   *pOverflow
    );


#ifdef __cplusplus
}
#endif

#endif  /* _LPC_H_ */




