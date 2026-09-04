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



 Filename: q_gain_p.cpp
 Functions: q_gain_pitch

------------------------------------------------------------------------------
 MODULE DESCRIPTION


------------------------------------------------------------------------------
*/

/*----------------------------------------------------------------------------
; INCLUDES
----------------------------------------------------------------------------*/
#include "q_gain_p.h"
#include "typedef.h"
#include "oper_32b.h"
#include "cnst.h"
#include "basic_op.h"


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
    ; Include all pre-processor statements here. Include conditional
    ; compile variables also.
    ----------------------------------------------------------------------------*/
#define NB_QUA_PITCH 16

    /*----------------------------------------------------------------------------
    ; LOCAL FUNCTION DEFINITIONS
    ; Function Prototype declaration
    ----------------------------------------------------------------------------*/

    /*----------------------------------------------------------------------------
    ; LOCAL VARIABLE DEFINITIONS
    ; Variable declaration - defined here and used outside this module
    ----------------------------------------------------------------------------*/

    /*----------------------------------------------------------------------------
    ; EXTERNAL GLOBAL STORE/BUFFER/POINTER REFERENCES
    ; Declare variables used in this module but defined elsewhere
    ----------------------------------------------------------------------------*/

    /*--------------------------------------------------------------------------*/
#ifdef __cplusplus
}
#endif

/*
------------------------------------------------------------------------------
 FUNCTION NAME: q_gain_pitch
------------------------------------------------------------------------------
 INPUT AND OUTPUT DEFINITIONS

 Inputs:
    mode -- enum Mode -- AMR mode
    gp_limit -- Word16 -- pitch gain limit
    gain -- Pointer to Word16 -- Pitch gain (unquant/quant),              Q14

 Outputs:
    gain -- Pointer to Word16 -- Pitch gain (unquant/quant),              Q14

    gain_cand -- Array of type Word16 -- pitch gain candidates (3),
                                         MR795 only, Q14

    gain_cind -- Array of type Word16 -- pitch gain cand. indices (3),
                                         MR795 only, Q0

    pOverflow -- Pointer to Flag -- overflow indicator

 Returns:
    Word16 -- index of quantization

 Global Variables Used:
    qua_gain_pitch

 Local Variables Needed:
    None

------------------------------------------------------------------------------
 FUNCTION DESCRIPTION


------------------------------------------------------------------------------
 REQUIREMENTS

 None

------------------------------------------------------------------------------
 REFERENCES

 q_gain_p.c, UMTS GSM AMR speech codec, R99 - Version 3.2.0, March 2, 2001

------------------------------------------------------------------------------
 PSEUDO-CODE


------------------------------------------------------------------------------
 CAUTION [optional]
 [State any special notes, constraints or cautions for users of this function]

------------------------------------------------------------------------------
*/

Word16 q_gain_pitch(    /* Return index of quantization                      */
    enum Mode mode,     /* i  : AMR mode                                     */
    Word16 gp_limit,    /* i  : pitch gain limit                             */
    Word16 *gain,       /* i/o: Pitch gain (unquant/quant),              Q14 */
    Word16 gain_cand[], /* o  : pitch gain candidates (3),   MR795 only, Q14 */
    Word16 gain_cind[], /* o  : pitch gain cand. indices (3),MR795 only, Q0  */
    const Word16* qua_gain_pitch_ptr, /* i : ptr to read-only table          */
    Flag   *pOverflow
)
{
    Word16 i;
    Word16 index;
    Word16 err;
    Word16 err_min;

    err_min = sub(*gain, qua_gain_pitch_ptr[0], pOverflow);
    err_min = abs_s(err_min);

    index = 0;

    for (i = 1; i < NB_QUA_PITCH; i++)
    {
        if (qua_gain_pitch_ptr[i] <= gp_limit)
        {
            err = sub(*gain, qua_gain_pitch_ptr[i], pOverflow);
            err = abs_s(err);

            if (err < err_min)
            {
                err_min = err;
                index = i;
            }
        }
    }

    if (mode == MR795)
    {
        /* in MR795 mode, compute three gain_pit candidates around the index
         * found in the quantization loop: the index found and the two direct
         * neighbours, except for the extreme cases (i=0 or i=NB_QUA_PITCH-1),
         * where the direct neighbour and the neighbour to that is used.
         */
        Word16 ii;

        if (index == 0)
        {
            ii = index;
        }
        else
        {
            if (index == (NB_QUA_PITCH - 1) ||
                    (qua_gain_pitch_ptr[index+1] > gp_limit))
            {
                ii = index - 2;
            }
            else
            {
                ii = index - 1;
            }
        }

        /* store candidate indices and values */
        for (i = 0; i < 3; i++)
        {
            gain_cind[i] = ii;
            gain_cand[i] = qua_gain_pitch_ptr[ii];

            ii += 1;
        }

        *gain = qua_gain_pitch_ptr[index];
    }
    else
    {
        /* in MR122 mode, just return the index and gain pitch found.
         * If bitexactness is required, mask away the two LSBs (because
         * in the original EFR, gain_pit was scaled Q12)
         */
        if (mode == MR122)
        {
            /* clear 2 LSBits */
            *gain = qua_gain_pitch_ptr[index] & 0xFFFC;
        }
        else
        {
            *gain = qua_gain_pitch_ptr[index];
        }
    }
    return index;
}
