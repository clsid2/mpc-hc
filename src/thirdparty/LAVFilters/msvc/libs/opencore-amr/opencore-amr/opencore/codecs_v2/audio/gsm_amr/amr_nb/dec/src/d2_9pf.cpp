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



 Filename: d2_9pf.cpp
 Functions: decode_2i40_9bits

------------------------------------------------------------------------------
 MODULE DESCRIPTION


 FUNCTION:  decode_2i40_9bits (decod_ACELP())

 PURPOSE:   Algebraic codebook decoder. For details about the encoding see
            c2_9pf.c
*/

/*----------------------------------------------------------------------------
; INCLUDES
----------------------------------------------------------------------------*/
#include "d2_9pf.h"
#include "typedef.h"
#include "basic_op.h"
#include "cnst.h"


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
#define NB_PULSE  2


    /*----------------------------------------------------------------------------
    ; LOCAL FUNCTION DEFINITIONS
    ; Function Prototype declaration
    ----------------------------------------------------------------------------*/

    /*----------------------------------------------------------------------------
    ; LOCAL VARIABLE DEFINITIONS
    ; Variable declaration - defined here and used outside this module
    ----------------------------------------------------------------------------*/

    /*--------------------------------------------------------------------------*/
#ifdef __cplusplus
}
#endif

/*
------------------------------------------------------------------------------
 FUNCTION NAME: decode_2i40_11bits
------------------------------------------------------------------------------
 INPUT AND OUTPUT DEFINITIONS

 Inputs:
    sign  -- Word16 -- signs of 2 pulses.
    index -- Word16 -- Positions of the 2 pulses.

 Outputs:
    cod[] -- array of type Word16 -- algebraic (fixed) codebook excitation
    pOverflow = pointer to overflow flag

 Returns:
    None

 Global Variables Used:
    None

 Local Variables Needed:
    None

------------------------------------------------------------------------------
 FUNCTION DESCRIPTION


------------------------------------------------------------------------------
 REQUIREMENTS

 None

------------------------------------------------------------------------------
 REFERENCES

 d2_9pf.c, UMTS GSM AMR speech codec, R99 - Version 3.2.0, March 2, 2001

------------------------------------------------------------------------------
 PSEUDO-CODE


------------------------------------------------------------------------------
 CAUTION [optional]
 [State any special notes, constraints or cautions for users of this function]

------------------------------------------------------------------------------
*/

void decode_2i40_9bits(
    Word16 subNr,  /* i : subframe number                          */
    Word16 sign,   /* i : signs of 2 pulses.                       */
    Word16 index,  /* i : Positions of the 2 pulses.               */
    const Word16* startPos_ptr, /*  i: ptr to read only table          */
    Word16 cod[],  /* o : algebraic (fixed) codebook excitation    */
    Flag  *pOverflow  /* o : Flag set when overflow occurs         */
)
{
    Word16 i;
    Word16 j;
    Word16 k;

    Word16 pos[NB_PULSE];

    /* Decode the positions */
    /* table bit  is the MSB */

    j = (Word16)(index & 64);

    j >>= 3;

    i = index & 7;

    k =
        shl(
            subNr,
            1,
            pOverflow);

    k += j;

    /* pos0 =i*5+startPos_ptr[j*8+subNr*2] */
    pos[0] = i * 5 + startPos_ptr[k++];


    index >>= 3;

    i = index & 7;

    /* pos1 =i*5+startPos_ptr[j*8+subNr*2 + 1] */
    pos[1] = i * 5 + startPos_ptr[k];


    /* decode the signs  and build the codeword */

    for (i = L_SUBFR - 1; i >= 0; i--)
    {
        cod[i] = 0;
    }

    for (j = 0; j < NB_PULSE; j++)
    {
        i = sign & 0x1;

        /* This line is equivalent to...
         *
         *
         *  if (i == 1)
         *  {
         *      cod[pos[j]] = 8191;
         *  }
         *  if (i == 0)
         *  {
         *      cod[pos[j]] = -8192;
         *  }
         */

        cod[pos[j]] = i * 16383 - 8192;

        sign >>= 1;
    }

    return;
}
