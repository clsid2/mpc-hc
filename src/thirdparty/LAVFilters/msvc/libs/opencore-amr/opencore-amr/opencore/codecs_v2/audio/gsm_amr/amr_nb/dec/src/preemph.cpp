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



 Filename: preemph.cpp
 Functions:

------------------------------------------------------------------------------
 MODULE DESCRIPTION

 Purpose          : Preemphasis filtering
 Description      : Filtering through 1 - g z^-1

------------------------------------------------------------------------------
*/

/*----------------------------------------------------------------------------
; INCLUDES
----------------------------------------------------------------------------*/
#include "preemph.h"
#include "typedef.h"
#include "basic_op.h"

/*----------------------------------------------------------------------------
; MACROS
; Define module specific macros here
----------------------------------------------------------------------------*/

/*----------------------------------------------------------------------------
; DEFINES
; Include all pre-processor statements here. Include conditional
; compile variables also.
----------------------------------------------------------------------------*/

/*----------------------------------------------------------------------------
; LOCAL FUNCTION DEFINITIONS
; Function Prototype declaration
----------------------------------------------------------------------------*/

/*----------------------------------------------------------------------------
; LOCAL VARIABLE DEFINITIONS
; Variable declaration - defined here and used outside this module
----------------------------------------------------------------------------*/


/*
------------------------------------------------------------------------------
 FUNCTION NAME:  preemphasis_reset
------------------------------------------------------------------------------
 INPUT AND OUTPUT DEFINITIONS

 Inputs:
    st -- double pointer to preemphasisState

 Outputs:
    st -- double ponter to preemphasisState

 Returns:
    -1 if an error occurs
     0 if OK

 Global Variables Used:
    None

 Local Variables Needed:
    None

------------------------------------------------------------------------------
 FUNCTION DESCRIPTION

    Initializes state memory to zero
------------------------------------------------------------------------------
 REQUIREMENTS

 None

------------------------------------------------------------------------------
 REFERENCES

 preemph.c, UMTS GSM AMR speech codec, R99 - Version 3.2.0, March 2, 2001

------------------------------------------------------------------------------
 PSEUDO-CODE


------------------------------------------------------------------------------
 CAUTION [optional]
 [State any special notes, constraints or cautions for users of this function]

------------------------------------------------------------------------------
*/

Word16 preemphasis_reset(preemphasisState *state)
{
    if (state == (preemphasisState *) NULL)
    {
        /* fprintf(stderr, "preemphasis_reset: invalid parameter\n"); */
        return -1;
    }

    state->mem_pre = 0;

    return 0;
}

/*
------------------------------------------------------------------------------
 FUNCTION NAME:  preemphasis
------------------------------------------------------------------------------
 INPUT AND OUTPUT DEFINITIONS

 Inputs:
    st -- Pointer to preemphasisState -- preemphasis filter state
    signal -- array of type Word16 -- input signal overwritten by the output
    g -- Word16 -- preemphasis coefficient
    L -- Word16 -- size of filtering

 Outputs:
    st -- Pointer to preemphasisState -- preemphasis filter state
    signal -- array of type Word16 -- input signal overwritten by the output
    pOverflow -- pointer to type Flag -- overflow indicator
 Returns:
    None

 Global Variables Used:
    None

 Local Variables Needed:
    None

------------------------------------------------------------------------------
 FUNCTION DESCRIPTION

    Filtering through 1 - g z^-1
------------------------------------------------------------------------------
 REQUIREMENTS

 None

------------------------------------------------------------------------------
 REFERENCES

 preemph.c, UMTS GSM AMR speech codec, R99 - Version 3.2.0, March 2, 2001

------------------------------------------------------------------------------
 PSEUDO-CODE


------------------------------------------------------------------------------
 CAUTION [optional]
 [State any special notes, constraints or cautions for users of this function]

------------------------------------------------------------------------------
*/


void preemphasis(
    preemphasisState *st, /* (i/o) : preemphasis filter state               */
    Word16 *signal,       /* (i/o) : input signal overwritten by the output */
    Word16 g,             /* (i)   : preemphasis coefficient                */
    Word16 L,             /* (i)   : size of filtering                      */
    Flag  *pOverflow      /* (o)   : overflow indicator                     */
)
{
    Word16 *p1;
    Word16 *p2;
    Word16 temp;
    Word16 temp2;
    Word16 i;

    p1 = signal + L - 1;
    p2 = p1 - 1;
    temp = *p1;

    for (i = 0; i <= L - 2; i++)
    {
        temp2 = mult(g, *(p2--), pOverflow);
        *p1 = sub(*p1, temp2, pOverflow);

        p1--;
    }

    temp2 = mult(g, st->mem_pre, pOverflow);

    *p1 = sub(*p1, temp2, pOverflow);

    st->mem_pre = temp;

    return;
}



