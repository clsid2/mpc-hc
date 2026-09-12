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



 Filename: d2_11pf.cpp

------------------------------------------------------------------------------
 MODULE DESCRIPTION
*/

/*----------------------------------------------------------------------------
; INCLUDES
----------------------------------------------------------------------------*/
#include "d2_11pf.h"
#include "typedef.h"
#include "cnst.h"


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

 d2_11pf.c, UMTS GSM AMR speech codec, R99 - Version 3.2.0, March 2, 2001

------------------------------------------------------------------------------
 PSEUDO-CODE


------------------------------------------------------------------------------
 CAUTION [optional]
 [State any special notes, constraints or cautions for users of this function]

------------------------------------------------------------------------------
*/

void decode_2i40_11bits(
    Word16 sign,   /* i : signs of 2 pulses.                       */
    Word16 index,  /* i : Positions of the 2 pulses.               */
    Word16 cod[]   /* o : algebraic (fixed) codebook excitation    */
)

{
    Word16 i;
    Word16 j;

    Word16 pos[NB_PULSE];

    /* Decode the positions */

    j = index & 0x1;

    index >>= 1;

    i = index & 0x7;

    pos[0] = i * 5 + j * 2 + 1;




    index >>= 3;

    j = index & 0x3;

    index >>= 2;

    i = index & 0x7;

    if (j == 3)
    {
        pos[1] = i * 5 + 4;
    }
    else
    {
        pos[1] = i * 5 + j;
    }




    /* decode the signs  and build the codeword */
    for (i = 0; i < L_SUBFR; i++)
    {
        cod[i] = 0;
    }

    for (j = 0; j < NB_PULSE; j++)
    {
        i = sign & 1;

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
