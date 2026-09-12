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
*      File             : q_plsf.c
*      Purpose          : common part (init, exit, reset) of LSF quantization
*                         module (rest in q_plsf_3.c and q_plsf_5.c)
*
********************************************************************************
*/

/*
********************************************************************************
*                         MODULE INCLUDE FILE AND VERSION ID
********************************************************************************
*/
#include "q_plsf.h"

/*
********************************************************************************
*                         INCLUDE FILES
********************************************************************************
*/
#include "typedef.h"
#include "basic_op.h"
#include "oscl_mem.h"


/*
********************************************************************************
*                         PUBLIC PROGRAM CODE
********************************************************************************
*/

/*
**************************************************************************
*
*  Function    : Q_plsf_init
*  Purpose     : Allocates memory and initializes state variables
*
**************************************************************************
*/
Word16 Q_plsf_init(Q_plsfState **state)
{
    Q_plsfState* s;

    if (state == (Q_plsfState **) NULL)
    {
        /* fprintf(stderr, "Q_plsf_init: invalid parameter\n"); */
        return -1;
    }
    *state = NULL;

    /* allocate memory */
    if ((s = (Q_plsfState *) oscl_malloc(sizeof(Q_plsfState))) == NULL)
    {
        /* fprintf(stderr, "Q_plsf_init: can not malloc state structure\n"); */
        return -1;
    }

    Q_plsf_reset(s);
    *state = s;

    return 0;
}

/*
**************************************************************************
*
*  Function    : Q_plsf_reset
*  Purpose     : Resets state memory
*
**************************************************************************
*/
Word16 Q_plsf_reset(Q_plsfState *state)
{
    Word16 i;

    if (state == (Q_plsfState *) NULL)
    {
        /* fprintf(stderr, "Q_plsf_reset: invalid parameter\n"); */
        return -1;
    }

    for (i = 0; i < M; i++)
        state->past_rq[i] = 0;

    return 0;
}

/*
**************************************************************************
*
*  Function    : Q_plsf_exit
*  Purpose     : The memory used for state memory is freed
*
**************************************************************************
*/
void Q_plsf_exit(Q_plsfState **state)
{
    if (state == NULL || *state == NULL)
        return;

    /* deallocate memory */
    oscl_free(*state);
    *state = NULL;

    return;
}
