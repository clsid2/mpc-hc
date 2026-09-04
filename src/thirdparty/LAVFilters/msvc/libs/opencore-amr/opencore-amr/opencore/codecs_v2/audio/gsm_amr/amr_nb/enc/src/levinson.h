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



 Filename: levinson.h

------------------------------------------------------------------------------
 INCLUDE DESCRIPTION

      File             : lag_wind.h
      Purpose          : Lag windowing of autocorrelations.

------------------------------------------------------------------------------
*/

#ifndef _LEVINSON_H_
#define _LEVINSON_H_
#define levinson_h "$Id $"

/*----------------------------------------------------------------------------
; INCLUDES
----------------------------------------------------------------------------*/
#include "typedef.h"
#include "cnst.h"

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
        Word16 old_A[M + 1];     /* Last A(z) for case of unstable filter */
    } LevinsonState;

    /*----------------------------------------------------------------------------
    ; GLOBAL FUNCTION DEFINITIONS
    ; [List function prototypes here]
    ----------------------------------------------------------------------------*/
    Word16 Levinson_init(LevinsonState **st);
    /* initialize one instance of the pre processing state.
       Stores pointer to filter status struct in *st. This pointer has to
       be passed to Levinson in each call.
       returns 0 on success
     */

    Word16 Levinson_reset(LevinsonState *st);
    /* reset of pre processing state (i.e. set state memory to zero)
       returns 0 on success
     */
    void Levinson_exit(LevinsonState **st);
    /* de-initialize pre processing state (i.e. free status struct)
       stores NULL in *st
     */

    Word16 Levinson(
        LevinsonState *st,
        Word16 Rh[],       /* i : Rh[m+1] Vector of autocorrelations (msb) */
        Word16 Rl[],       /* i : Rl[m+1] Vector of autocorrelations (lsb) */
        Word16 A[],        /* o : A[m]    LPC coefficients  (m = 10)       */
        Word16 rc[],        /* o : rc[4]   First 4 reflection coefficients  */
        Flag   *pOverflow
    );



#ifdef __cplusplus
}
#endif

#endif  /* _LEVINSON_H_ */




