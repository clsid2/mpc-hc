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



 Filename: post_pro.h

------------------------------------------------------------------------------
 INCLUDE DESCRIPTION

      File             : post_pro.h
      Purpose          : Postprocessing of output speech.

                         - 2nd order high pass filtering with cut
                           off frequency at 60 Hz.
                         - Multiplication of output by two.
------------------------------------------------------------------------------
*/

#ifndef _POST_PRO_H_
#define _POST_PRO_H_
#define post_pro_h "$Id $"

/*----------------------------------------------------------------------------
; INCLUDES
----------------------------------------------------------------------------*/
#include "typedef.h"

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
        Word16 y2_hi;
        Word16 y2_lo;
        Word16 y1_hi;
        Word16 y1_lo;
        Word16 x0;
        Word16 x1;
    } Post_ProcessState;

    /*----------------------------------------------------------------------------
    ; GLOBAL FUNCTION DEFINITIONS
    ; [List function prototypes here]
    ----------------------------------------------------------------------------*/

    Word16 Post_Process_reset(Post_ProcessState *st);
    /* reset of Post processing state (i.e. set state memory to zero)
       returns 0 on success
     */
    void Post_Process_exit(Post_ProcessState **st);
    /* de-initialize Post processing state (i.e. free status struct)
       stores NULL in *st
     */

    void Post_Process(
        Post_ProcessState *st,  /* i/o : post process state                   */
        Word16 signal[],        /* i/o : signal                               */
        Word16 lg,              /* i   : lenght of signal                     */
        Flag *pOverflow
    );

#ifdef __cplusplus
}
#endif

#endif  /* _POST_PRO_H_ */





