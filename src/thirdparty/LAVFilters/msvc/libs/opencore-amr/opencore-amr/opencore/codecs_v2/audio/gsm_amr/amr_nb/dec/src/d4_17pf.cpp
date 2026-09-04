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



 Filename: d4_17pf.cpp
 Functions: decode_4i40_17bits

------------------------------------------------------------------------------
 MODULE DESCRIPTION


 FUNCTION:  decode_4i40_17bits (decod_ACELP())

 PURPOSE:   Algebraic codebook decoder. For details about the encoding see
            c4_17pf.c
*/

/*----------------------------------------------------------------------------
; INCLUDES
----------------------------------------------------------------------------*/
#include "typedef.h"
#include "basic_op.h"
#include "cnst.h"
#include "d4_17pf.h"

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
#define NB_PULSE 4           /* number of pulses  */


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
 FUNCTION NAME: decode_4i40_17bits
------------------------------------------------------------------------------
 INPUT AND OUTPUT DEFINITIONS

 Inputs:
    sign  -- Word16 -- signs of 3 pulses.
    index -- Word16 -- Positions of the 3 pulses.

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

 d4_17pf.c, UMTS GSM AMR speech codec, R99 - Version 3.2.0, March 2, 2001

------------------------------------------------------------------------------
 PSEUDO-CODE


------------------------------------------------------------------------------
 CAUTION [optional]
 [State any special notes, constraints or cautions for users of this function]

------------------------------------------------------------------------------
*/

void decode_4i40_17bits(
    Word16 sign,   /* i : signs of 4 pulses.                       */
    Word16 index,  /* i : Positions of the 4 pulses.               */
    const Word16* dgray_ptr, /* i : Pointer to read-only table         */
    Word16 cod[]   /* o : algebraic (fixed) codebook excitation    */
)
{
    Word16 i;
    Word16 j;

    Word16 pos[NB_PULSE];

    /* Index is a 13-bit value.  3 bits each correspond to the
     * positions 0-2, with 4 bits allocated for position 3.
     *
     *
     * [][][][] [][][] [][][] [][][]
     *    |       |      |     |
     *    |       |      |     |
     *   pos3    pos2   pos1  pos0
     */

    /* Decode the positions */

    i = index & 0x7;

    i = dgray_ptr[i];

    pos[0] = i * 5; /* pos0 =i*5 */


    index >>= 3;

    i = index & 0x7;

    i = dgray_ptr[i];

    pos[1] = i * 5 + 1;  /* pos1 =i*5+1 */



    index >>= 3;

    i = index & 0x7;

    i = dgray_ptr[i];

    pos[2] = i * 5 + 2; /* pos2 =i*5+2 */





    index >>= 3;

    j = index & 0x1;

    index >>= 1;

    i = index & 0x7;

    i = dgray_ptr[i];

    pos[3] = i * 5 + 3 + j; /* pos3 =i*5+3+j */


    /* decode the signs  and build the codeword */

    for (i = 0; i < L_SUBFR; i++)
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
