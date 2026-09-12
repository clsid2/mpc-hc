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



 Filename: g_adapt.h

------------------------------------------------------------------------------
 INCLUDE DESCRIPTION

 This file contains all the constant definitions and prototype definitions
 needed by the file, g_adapt.c

------------------------------------------------------------------------------
*/

/*----------------------------------------------------------------------------
; CONTINUE ONLY IF NOT ALREADY DEFINED
----------------------------------------------------------------------------*/
#ifndef g_adapt_h
#define g_adapt_h "$Id $"

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
    ; Define module specific macros here
    ----------------------------------------------------------------------------*/

    /*----------------------------------------------------------------------------
    ; DEFINES
    ; Include all pre-processor statements here.
    ----------------------------------------------------------------------------*/
#define LTPG_MEM_SIZE 5 /* number of stored past LTP coding gains + 1 */

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
        Word16 onset;                   /* onset state,                   Q0  */
        Word16 prev_alpha;              /* previous adaptor output,       Q15 */
        Word16 prev_gc;                 /* previous code gain,            Q1  */

        Word16 ltpg_mem[LTPG_MEM_SIZE]; /* LTP coding gain history,       Q13 */
        /* (ltpg_mem[0] not used for history) */
    } GainAdaptState;

    /*----------------------------------------------------------------------------
    ; GLOBAL FUNCTION DEFINITIONS
    ; Function Prototype declaration
    ----------------------------------------------------------------------------*/
    Word16 gain_adapt_init(GainAdaptState **st);
    /* initialize one instance of the gain adaptor
       Stores pointer to state struct in *st. This pointer has to
       be passed to gain_adapt and gain_adapt_update in each call.
       returns 0 on success
     */

    Word16 gain_adapt_reset(GainAdaptState *st);
    /* reset of gain adaptor state (i.e. set state memory to zero)
       returns 0 on success
     */

    void gain_adapt_exit(GainAdaptState **st);
    /* de-initialize gain adaptor state (i.e. free state struct)
       stores NULL in *st
     */

    /*************************************************************************
     *
     *  Function:   gain_adapt()
     *  Purpose:    calculate pitch/codebook gain adaptation factor alpha
     *              (and update the adaptor state)
     *
     **************************************************************************
     */
    void gain_adapt(
        GainAdaptState *st,  /* i  : state struct                  */
        Word16 ltpg,         /* i  : ltp coding gain (log2()), Q   */
        Word16 gain_cod,     /* i  : code gain,                Q13 */
        Word16 *alpha,       /* o  : gain adaptation factor,   Q15 */
        Flag   *pOverflow    /* o  : overflow indicator            */
    );

    /*----------------------------------------------------------------------------
    ; END
    ----------------------------------------------------------------------------*/
#ifdef __cplusplus
}
#endif

#endif /* _H_ */


