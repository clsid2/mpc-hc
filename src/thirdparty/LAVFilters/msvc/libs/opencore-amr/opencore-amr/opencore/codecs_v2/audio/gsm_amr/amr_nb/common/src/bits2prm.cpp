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

 Filename: bits2prm.cpp

------------------------------------------------------------------------------
*/

/*----------------------------------------------------------------------------
; INCLUDES
----------------------------------------------------------------------------*/
#include "bits2prm.h"
#include "typedef.h"
#include "mode.h"
#include "bitno_tab.h"

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
 FUNCTION NAME: Bin2int
------------------------------------------------------------------------------
 INPUT AND OUTPUT DEFINITIONS

 Inputs:
    no_of_bits = number of bits associated with value
    bitstream = pointer to buffer where bits are read

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

  Function    : Bin2int
  Purpose     : Read "no_of_bits" bits from the array bitstream[]
                and convert to integer.

------------------------------------------------------------------------------
 REQUIREMENTS

 None

------------------------------------------------------------------------------
 REFERENCES

 bits2prm.c, UMTS GSM AMR speech codec, R99 - Version 3.2.0, March 2, 2001

------------------------------------------------------------------------------
 PSEUDO-CODE

static Word16 Bin2int ( // Reconstructed parameter
    Word16 no_of_bits,  // input : number of bits associated with value
    Word16 *bitstream   // output: address where bits are written
)
{
    Word16 value, i, bit;

    value = 0;
    for (i = 0; i < no_of_bits; i++)
    {
        value = shl (value, 1);
        bit = *bitstream++;
        if (sub (bit, BIT_1) == 0)
            value = add (value, 1);
    }
    return (value);
}

------------------------------------------------------------------------------
 CAUTION [optional]
 [State any special notes, constraints or cautions for users of this function]

------------------------------------------------------------------------------
*/

/*----------------------------------------------------------------------------
; FUNCTION CODE
----------------------------------------------------------------------------*/
static Word16 Bin2int(  /* Reconstructed parameter                      */
    Word16 no_of_bits,  /* input : number of bits associated with value */
    Word16 *bitstream   /* input: address where bits are read from      */
)
{
    Word16 value;
    Word16 i;
    Word16 single_bit;

    value = 0;
    for (i = 0; i < no_of_bits; i++)
    {
        value <<= 1;
        single_bit = *(bitstream++);
        value |= single_bit;
    }
    return (value);
}


/*
------------------------------------------------------------------------------
 FUNCTION NAME: bits2prm
------------------------------------------------------------------------------
 INPUT AND OUTPUT DEFINITIONS

 Inputs:
    mode = AMR mode of type enum Mode
    bits[] = pointer to serial bits of type Word16
    prm[] = pointer to analysis parameters of type Word16

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

  Function    : Bits2prm
  Purpose     : Retrieves the vector of encoder parameters from
                the received serial bits in a frame.

------------------------------------------------------------------------------
 REQUIREMENTS

 None

------------------------------------------------------------------------------
 REFERENCES

 bits2prm.c, UMTS GSM AMR speech codec, R99 - Version 3.2.0, March 2, 2001

------------------------------------------------------------------------------
 PSEUDO-CODE

void Bits2prm (
    enum Mode mode,     // i : AMR mode
    Word16 bits[],      // i : serial bits       (size <= MAX_SERIAL_SIZE)
    Word16 prm[]        // o : analysis parameters  (size <= MAX_PRM_SIZE)
)
{
    Word16 i;

    for (i = 0; i < prmno[mode]; i++)
    {
        prm[i] = Bin2int (bitno[mode][i], bits);
        bits += bitno[mode][i];
        add(0,0);       // account for above pointer update
    }

   return;
}

------------------------------------------------------------------------------
 CAUTION [optional]
 [State any special notes, constraints or cautions for users of this function]

------------------------------------------------------------------------------
*/

/*----------------------------------------------------------------------------
; FUNCTION CODE
----------------------------------------------------------------------------*/
OSCL_EXPORT_REF void Bits2prm(
    enum Mode mode,     /* i : AMR mode                                    */
    Word16 bits[],      /* i : serial bits       (size <= MAX_SERIAL_SIZE) */
    Word16 prm[]        /* o : analysis parameters  (size <= MAX_PRM_SIZE) */
)
{
    Word16 i;

    for (i = 0; i < prmno[mode]; i++)
    {
        prm[i] = Bin2int(bitno[mode][i], bits);
        bits += bitno[mode][i];
    }

    return;
}








