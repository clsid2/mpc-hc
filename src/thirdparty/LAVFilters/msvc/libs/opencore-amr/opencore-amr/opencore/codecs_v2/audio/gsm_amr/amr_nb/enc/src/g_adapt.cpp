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



 Filename: g_adapt.cpp
 Functions:

------------------------------------------------------------------------------
 MODULE DESCRIPTION


------------------------------------------------------------------------------
*/

/*----------------------------------------------------------------------------
; INCLUDES
----------------------------------------------------------------------------*/
#include "g_adapt.h"
#include "typedef.h"
#include "basic_op.h"
#include "oper_32b.h"
#include "cnst.h"
#include "gmed_n.h"
#include "oscl_mem.h"

/*----------------------------------------------------------------------------
; MACROS
; Define module specific macros here
----------------------------------------------------------------------------*/

/*----------------------------------------------------------------------------
; DEFINES
; Include all pre-processor statements here. Include conditional
; compile variables also.
----------------------------------------------------------------------------*/
#define LTP_GAIN_THR1 2721 /* 2721 Q13 = 0.3322 ~= 1.0 / (10*log10(2)) */
#define LTP_GAIN_THR2 5443 /* 5443 Q13 = 0.6644 ~= 2.0 / (10*log10(2)) */

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
 FUNCTION NAME: gain_adapt_init
------------------------------------------------------------------------------
 INPUT AND OUTPUT DEFINITIONS

 Inputs:
    st -- double pointer to GainAdaptState

 Outputs:
    st -- double ponter to GainAdaptState

 Returns:
    -1 if an error occurs during memory initialization
     0 if OK

 Global Variables Used:
    None

 Local Variables Needed:
    None

------------------------------------------------------------------------------
 FUNCTION DESCRIPTION

    Allocates state memory and initializes state memory

------------------------------------------------------------------------------
 REQUIREMENTS

 None

------------------------------------------------------------------------------
 REFERENCES

 g_adapt.c, UMTS GSM AMR speech codec, R99 - Version 3.2.0, March 2, 2001

------------------------------------------------------------------------------
 PSEUDO-CODE


------------------------------------------------------------------------------
 CAUTION [optional]
 [State any special notes, constraints or cautions for users of this function]

------------------------------------------------------------------------------
*/

Word16 gain_adapt_init(GainAdaptState **st)
{
    GainAdaptState* s;

    if (st == (GainAdaptState **) NULL)
    {
        /* fprintf(stderr, "gain_adapt_init: invalid parameter\n"); */
        return -1;
    }
    *st = NULL;

    /* allocate memory */
    if ((s = (GainAdaptState *) oscl_malloc(sizeof(GainAdaptState))) == NULL)
    {
        /* fprintf(stderr, "gain_adapt_init: can't malloc state structure\n"); */
        return -1;
    }
    gain_adapt_reset(s);
    *st = s;

    return 0;
}

/*
------------------------------------------------------------------------------
 FUNCTION NAME: gain_adapt_reset
------------------------------------------------------------------------------
 INPUT AND OUTPUT DEFINITIONS

 Inputs:
    st -- double pointer to GainAdaptState

 Outputs:
    st -- double ponter to GainAdaptState

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

 g_adapt.c, UMTS GSM AMR speech codec, R99 - Version 3.2.0, March 2, 2001

------------------------------------------------------------------------------
 PSEUDO-CODE


------------------------------------------------------------------------------
 CAUTION [optional]
 [State any special notes, constraints or cautions for users of this function]

------------------------------------------------------------------------------
*/

Word16 gain_adapt_reset(GainAdaptState *st)
{
    Word16 i;

    if (st == (GainAdaptState *) NULL)
    {
        /* fprintf(stderr, "gain_adapt_reset: invalid parameter\n"); */
        return -1;
    }

    st->onset = 0;
    st->prev_alpha = 0;
    st->prev_gc = 0;

    for (i = 0; i < LTPG_MEM_SIZE; i++)
    {
        st->ltpg_mem[i] = 0;
    }

    return 0;
}


/*
------------------------------------------------------------------------------
 FUNCTION NAME: gain_adapt_exit
------------------------------------------------------------------------------
 INPUT AND OUTPUT DEFINITIONS

 Inputs:
    st -- double pointer to GainAdaptState

 Outputs:
    None

 Returns:
    None

 Global Variables Used:
    None

 Local Variables Needed:
    None

------------------------------------------------------------------------------
 FUNCTION DESCRIPTION

    The memory used for state memory is freed
------------------------------------------------------------------------------
 REQUIREMENTS

 None

------------------------------------------------------------------------------
 REFERENCES

 g_adapt.c, UMTS GSM AMR speech codec, R99 - Version 3.2.0, March 2, 2001

------------------------------------------------------------------------------
 PSEUDO-CODE


------------------------------------------------------------------------------
 CAUTION [optional]
 [State any special notes, constraints or cautions for users of this function]

------------------------------------------------------------------------------
*/

void gain_adapt_exit(GainAdaptState **st)
{
    if (st == NULL || *st == NULL)
        return;

    /* deallocate memory */
    oscl_free(*st);
    *st = NULL;

    return;
}

/*
------------------------------------------------------------------------------
 FUNCTION NAME: gain_adapt
------------------------------------------------------------------------------
 INPUT AND OUTPUT DEFINITIONS

 Inputs:
    st -- double pointer to GainAdaptState
    ltpg -- Word16 -- ltp coding gain (log2()), Q13
    gain_cod -- Word16 -- code gain, Q1

 Outputs:
    alpha -- Pointer to Word16 --  gain adaptation factor,   Q15
    pOverflow -- Pointer to Flag -- overflow indicator

 Returns:
    None

 Global Variables Used:
    None

 Local Variables Needed:
    None

------------------------------------------------------------------------------
 FUNCTION DESCRIPTION

 Purpose:    calculate pitch/codebook gain adaptation factor alpha
             (and update the adaptor state)

------------------------------------------------------------------------------
 REQUIREMENTS

 None

------------------------------------------------------------------------------
 REFERENCES

 g_adapt.c, UMTS GSM AMR speech codec, R99 - Version 3.2.0, March 2, 2001

------------------------------------------------------------------------------
 PSEUDO-CODE


------------------------------------------------------------------------------
 CAUTION [optional]
 [State any special notes, constraints or cautions for users of this function]

------------------------------------------------------------------------------
*/

void gain_adapt(
    GainAdaptState *st,  /* i  : state struct                  */
    Word16 ltpg,         /* i  : ltp coding gain (log2()), Q13 */
    Word16 gain_cod,     /* i  : code gain,                Q1  */
    Word16 *alpha,       /* o  : gain adaptation factor,   Q15 */
    Flag   *pOverflow
)
{
    Word16 adapt;      /* adaptdation status; 0, 1, or 2       */
    Word16 result;     /* alpha factor, Q13                    */
    Word16 filt;       /* median-filtered LTP coding gain, Q13 */
    Word16 tmp;
    Word16 i;

    /* basic adaptation */
    if (ltpg <= LTP_GAIN_THR1)
    {
        adapt = 0;
    }
    else
    {
        if (ltpg <= LTP_GAIN_THR2)
        {
            adapt = 1;
        }
        else
        {
            adapt = 2;
        }
    }

    /*
     * // onset indicator
     * if ((cbGain > onFact * cbGainMem[0]) && (cbGain > 100.0))
     *     onset = 8;
     * else
     *     if (onset)
     *         onset--;
     */
    /* tmp = cbGain / onFact; onFact = 2.0; 200 Q1 = 100.0 */
    tmp = shr_r(gain_cod, 1, pOverflow);

    if ((tmp > st->prev_gc) && (gain_cod > 200))
    {
        st->onset = 8;
    }
    else
    {
        if (st->onset != 0)
        {
            st->onset--;
        }
    }

    /*
     *  // if onset, increase adaptor state
     *  if (onset && (gainAdapt < 2)) gainAdapt++;
     */
    if ((st->onset != 0) && (adapt < 2))
    {
        adapt += 1;
    }

    st->ltpg_mem[0] = ltpg;
    filt = gmed_n(st->ltpg_mem, 5);  /* function result */

    if (adapt == 0)
    {
        if (filt > 5443) /* 5443 Q13 = 0.66443... */
        {
            result = 0;
        }
        else
        {
            if (filt < 0)
            {
                result = 16384;  /* 16384 Q15 = 0.5 */
            }
            else
            {   /* result       =   0.5 - 0.75257499*filt     */
                /* result (Q15) = 16384 - 24660 * (filt << 2) */
                filt = shl(filt, 2, pOverflow);  /* Q15 */
                result = mult(24660, filt, pOverflow);
                result = 16384 - result;
            }
        }
    }
    else
    {
        result = 0;
    }
    /*
     *  if (prevAlpha == 0.0) result = 0.5 * (result + prevAlpha);
     */
    if (st->prev_alpha == 0)
    {
        result = shr(result, 1, pOverflow);
    }

    /* store the result */
    *alpha = result;

    /* update adapter state memory */
    st->prev_alpha = result;
    st->prev_gc = gain_cod;

    for (i = LTPG_MEM_SIZE - 1; i > 0; i--)
    {
        st->ltpg_mem[i] = st->ltpg_mem[i-1];
    }
    /* mem[0] is just present for convenience in calling the gmed_n[5]
     * function above. The memory depth is really LTPG_MEM_SIZE-1.
     */
}
