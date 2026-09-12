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
********************************************************************************
*
*      GSM AMR-NB speech codec   R98   Version 7.5.0   March 2, 2001
*                                R99   Version 3.2.0
*                                REL-4 Version 4.0.0
*
********************************************************************************
*
*      File             : pre_proc.h
*      Purpose          : Preprocessing of input speech.
*

 Description:  Replaced "int" and/or "char" with OSCL defined types.

********************************************************************************
*/
#ifndef pre_proc_h
#define pre_proc_h "$Id $"

/*
********************************************************************************
*                         INCLUDE FILES
********************************************************************************
*/
#include "typedef.h"

#ifdef __cplusplus
extern "C"
{
#endif

    /*
    ********************************************************************************
    *                         LOCAL VARIABLES AND TABLES
    ********************************************************************************
    */

    /*
    ********************************************************************************
    *                         DEFINITION OF DATA TYPES
    ********************************************************************************
    */
    typedef struct
    {
        Word16 y2_hi;
        Word16 y2_lo;
        Word16 y1_hi;
        Word16 y1_lo;
        Word16 x0;
        Word16 x1;
    } Pre_ProcessState;

    /*
    ********************************************************************************
    *                         DECLARATION OF PROTOTYPES
    ********************************************************************************
    */

    Word16 Pre_Process_init(Pre_ProcessState **st);
    /* initialize one instance of the pre processing state.
       Stores pointer to filter status struct in *st. This pointer has to
       be passed to Pre_Process in each call.
       returns 0 on success
     */

    Word16 Pre_Process_reset(Pre_ProcessState *st);
    /* reset of pre processing state (i.e. set state memory to zero)
       returns 0 on success
     */
    void Pre_Process_exit(Pre_ProcessState **st);
    /* de-initialize pre processing state (i.e. free status struct)
       stores NULL in *st
     */

    void Pre_Process(
        Pre_ProcessState *st,
        Word16 signal[],   /* Input/output signal                               */
        Word16 lg          /* Lenght of signal                                  */
    );

#ifdef __cplusplus
}
#endif

#endif
