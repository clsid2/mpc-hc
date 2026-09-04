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
 Filename: reorder.cpp

------------------------------------------------------------------------------
*/


/*----------------------------------------------------------------------------
; INCLUDES
----------------------------------------------------------------------------*/
#include    "reorder.h"

/*----------------------------------------------------------------------------
; MACROS
; [Define module specific macros here]
----------------------------------------------------------------------------*/

/*----------------------------------------------------------------------------
; DEFINES
; [Include all pre-processor statements here. Include conditional
; compile variables also.]
----------------------------------------------------------------------------*/

/*----------------------------------------------------------------------------
; LOCAL FUNCTION DEFINITIONS
; [List function prototypes here]
----------------------------------------------------------------------------*/

/*----------------------------------------------------------------------------
; LOCAL VARIABLE DEFINITIONS
; [Variable declaration - defined here and used outside this module]
----------------------------------------------------------------------------*/


/*
------------------------------------------------------------------------------
 FUNCTION NAME: Reorder_lsf
------------------------------------------------------------------------------
 INPUT AND OUTPUT DEFINITIONS

 Inputs:
    lsf = vector of LSFs   (range: 0<=val<=0.5)(Word16)
    min_dist = minimum required distance (Word16)
    n = LPC order (Word16)
    pOverflow = pointer to overflow (Flag)

 Outputs:
    pOverflow -> 1 if the add operation called by Reorder_lsf() results in
     overflow
    lsf -> reordered vector of LSFs (Word16)

 Returns:
    None

 Global Variables Used:
    None

 Local Variables Needed:
    None

------------------------------------------------------------------------------
 FUNCTION DESCRIPTION

 This function makes sure that the LSFs are properly ordered keeps a certain
 minimum distance between adjacent LSFs.

------------------------------------------------------------------------------
 REQUIREMENTS

 None

------------------------------------------------------------------------------
 REFERENCES

 [1] reorder.c, UMTS GSM AMR speech codec, R99 - Version 3.2.0, March 2, 2001

------------------------------------------------------------------------------
 PSEUDO-CODE

void Reorder_lsf (
    Word16 *lsf,        // (i/o)     : vector of LSFs   (range: 0<=val<=0.5)
    Word16 min_dist,    // (i)       : minimum required distance
    Word16 n            // (i)       : LPC order
)
{
    Word16 i;
    Word16 lsf_min;

// The reference ETSI code uses a global flag for Overflow. In the actual
// implementation a pointer to Overflow flag is passed into the function
// for use by the math functions add() and sub()

    lsf_min = min_dist;
    for (i = 0; i < n; i++)
    {
        if (sub (lsf[i], lsf_min) < 0)
        {
            lsf[i] = lsf_min;
        }
        lsf_min = add (lsf[i], min_dist);
    }
}

------------------------------------------------------------------------------
 CAUTION [optional]
 [State any special notes, constraints or cautions for users of this function]

------------------------------------------------------------------------------
*/

/*----------------------------------------------------------------------------
; FUNCTION CODE
----------------------------------------------------------------------------*/
OSCL_EXPORT_REF void Reorder_lsf(
    Word16 *lsf,        /* (i/o)    : vector of LSFs   (range: 0<=val<=0.5) */
    Word16 min_dist,    /* (i)      : minimum required distance             */
    Word16 n,           /* (i)      : LPC order                             */
    Flag   *pOverflow   /* (i/o)    : Overflow flag                         */
)
{
    Word16 i;
    Word16 lsf_min;
    Word16 *p_lsf = &lsf[0];
    OSCL_UNUSED_ARG(pOverflow);

    lsf_min = min_dist;
    for (i = 0; i < n; i++)
    {
        if (*(p_lsf) < lsf_min)
        {
            *(p_lsf++) = lsf_min;
            lsf_min +=  min_dist;
        }
        else
        {
            lsf_min = *(p_lsf++) + min_dist;
        }
    }
}

