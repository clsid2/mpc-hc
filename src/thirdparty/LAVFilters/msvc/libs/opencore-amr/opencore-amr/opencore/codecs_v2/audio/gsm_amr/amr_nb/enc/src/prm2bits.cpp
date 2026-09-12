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




 Filename: prm2bits.cpp

------------------------------------------------------------------------------
*/

/*----------------------------------------------------------------------------
; INCLUDES
----------------------------------------------------------------------------*/
#include "prm2bits.h"
#include "mode.h"


/*----------------------------------------------------------------------------
; MACROS
; [Define module specific macros here]
----------------------------------------------------------------------------*/

/*----------------------------------------------------------------------------
; DEFINES
; [Include all pre-processor statements here. Include conditional
; compile variables also.]
----------------------------------------------------------------------------*/
#define MASK      0x0001
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
 FUNCTION NAME: Int2bin
------------------------------------------------------------------------------
 INPUT AND OUTPUT DEFINITIONS

 Inputs:
    value = value to be converted to binary of type Word16
    no_of_bits = number of bits associated with value of type Word16

 Outputs:
    bitstream = pointer to address where bits are written of type Word16

 Returns:
    None

 Global Variables Used:
    None

 Local Variables Needed:
    None

------------------------------------------------------------------------------
 FUNCTION DESCRIPTION

  FUNCTION:  Int2bin

  PURPOSE:  convert integer to binary and write the bits to the array
            bitstream[]. The most significant bits are written first.

------------------------------------------------------------------------------
 REQUIREMENTS

 None

------------------------------------------------------------------------------
 REFERENCES

 prm2bits.c, UMTS GSM AMR speech codec, R99 - Version 3.2.0, March 2, 2001

------------------------------------------------------------------------------
 PSEUDO-CODE

static void Int2bin (
    Word16 value,       // input : value to be converted to binary
    Word16 no_of_bits,  // input : number of bits associated with value
    Word16 *bitstream   // output: address where bits are written
)
{
    Word16 *pt_bitstream, i, bit;

    pt_bitstream = &bitstream[no_of_bits];

    for (i = 0; i < no_of_bits; i++)
    {
        bit = value & MASK;
        if (bit == 0)
        {
            *--pt_bitstream = BIT_0;
        }
        else
        {
            *--pt_bitstream = BIT_1;
        }
        value = shr (value, 1);
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
static void Int2bin(
    Word16 value,       /* input : value to be converted to binary      */
    Word16 no_of_bits,  /* input : number of bits associated with value */
    Word16 *bitstream   /* output: address where bits are written       */
)
{
    Word16 *pt_bitstream;
    Word16 i;

    pt_bitstream = &bitstream[no_of_bits-1];

    for (i = no_of_bits; i != 0; i--)
    {
        *(pt_bitstream--) = value & MASK;
        value >>= 1;
    }

}


/*
------------------------------------------------------------------------------
 FUNCTION NAME: prm2bits
------------------------------------------------------------------------------
 INPUT AND OUTPUT DEFINITIONS

 Inputs:
    mode  = AMR mode of type enum Mode
    prm[] = pointer to analysis parameters of type Word16

 Outputs:
    bits[] = pointer to serial bits of type Word16

 Returns:
    None

 Global Variables Used:
    None

 Local Variables Needed:
    None

------------------------------------------------------------------------------
 FUNCTION DESCRIPTION

  FUNCTION:    Prm2bits

  PURPOSE:     converts the encoder parameter vector into a vector of serial
               bits.

  DESCRIPTION: depending on the mode, different numbers of parameters
               (with differing numbers of bits) are processed. Details
               are found in bitno.tab

------------------------------------------------------------------------------
 REQUIREMENTS

 None

------------------------------------------------------------------------------
 REFERENCES

 prm2bits.c, UMTS GSM AMR speech codec, R99 - Version 3.2.0, March 2, 2001

------------------------------------------------------------------------------
 PSEUDO-CODE

void Prm2bits (
    enum Mode mode,    // i : AMR mode
    Word16 prm[],      // i : analysis parameters (size <= MAX_PRM_SIZE)
    Word16 bits[]      // o : serial bits         (size <= MAX_SERIAL_SIZE)
)
{
   Word16 i;

   for (i = 0; i < prmno[mode]; i++)
   {
       Int2bin (prm[i], bitno[mode][i], bits);
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
void Prm2bits(
    enum Mode mode,    /* i : AMR mode                                      */
    Word16 prm[],      /* i : analysis parameters (size <= MAX_PRM_SIZE)    */
    Word16 bits[],      /* o : serial bits         (size <= MAX_SERIAL_SIZE) */
    CommonAmrTbls* common_amr_tbls /* i : ptr to strcut of table ptrs        */
)
{
    Word16 i;
    const Word16 *p_mode;
    Word16 *p_prm;
    const Word16* prmno_ptr = common_amr_tbls->prmno_ptr;

    p_mode = &common_amr_tbls->bitno_ptr[mode][0];
    p_prm  = &prm[0];

    for (i = prmno_ptr[mode]; i != 0; i--)
    {
        Int2bin(*(p_prm++), *(p_mode), bits);
        bits += *(p_mode++);
    }

    return;
}

