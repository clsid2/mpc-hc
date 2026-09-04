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



 Filename: b_cn_cod.cpp
 Functions: pseudonoise
            build_CN_code
            build_CN_param

------------------------------------------------------------------------------
 MODULE DESCRIPTION

 This module contains functions for comfort noise(CN) generation.

------------------------------------------------------------------------------
*/


/*----------------------------------------------------------------------------
; INCLUDES
----------------------------------------------------------------------------*/
#include "b_cn_cod.h"
#include "basic_op.h"
#include "cnst.h"

/*----------------------------------------------------------------------------
; MACROS
; [Define module specific macros here]
----------------------------------------------------------------------------*/


/*----------------------------------------------------------------------------
; DEFINES
; [Include all pre-processor statements here. Include conditional
; compile variables also.]
----------------------------------------------------------------------------*/
#define  NB_PULSE 10        /* number of random pulses in DTX operation   */

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
 FUNCTION NAME: pseudonoise
------------------------------------------------------------------------------
 INPUT AND OUTPUT DEFINITIONS

 Inputs:
    pShift_reg = pointer to Old CN generator shift register state (Word32)
    no_bits = Number of bits (Word16)

 Outputs:
    pShift_reg -> Updated CN generator shift register state

 Returns:
    noise_bits = Generated random integer value (Word16)

 Global Variables Used:
    None

 Local Variables Needed:
    None

------------------------------------------------------------------------------
 FUNCTION DESCRIPTION

 Generate a random integer value to use in comfort noise generation. The
 algorithm uses polynomial x^31 + x^3 + 1. Length of the PN sequence
 is 2^31 - 1

------------------------------------------------------------------------------
 REQUIREMENTS

 None

------------------------------------------------------------------------------
 REFERENCES

 b_cn_cod.c, UMTS GSM AMR speech codec, R99 - Version 3.2.0, March 2, 2001

------------------------------------------------------------------------------
 PSEUDO-CODE

Word16 pseudonoise (
    Word32 *shift_reg, // i/o : Old CN generator shift register state
    Word16 no_bits     // i   : Number of bits
)
{
   Word16 noise_bits, Sn, i;

   noise_bits = 0;
   for (i = 0; i < no_bits; i++)
   {
      // State n == 31
      if ((*shift_reg & 0x00000001L) != 0)
      {
         Sn = 1;
      }
      else
      {
         Sn = 0;
      }

      // State n == 3
      if ((*shift_reg & 0x10000000L) != 0)
      {
         Sn = Sn ^ 1;
      }
      else
      {
         Sn = Sn ^ 0;
      }

      noise_bits = shl (noise_bits, 1);
      noise_bits = noise_bits | (extract_l (*shift_reg) & 1);

      *shift_reg = L_shr (*shift_reg, 1);
      if (Sn & 1)
      {
         *shift_reg = *shift_reg | 0x40000000L;
      }
   }
   return noise_bits;
}

------------------------------------------------------------------------------
 CAUTION [optional]
 [State any special notes, constraints or cautions for users of this function]

------------------------------------------------------------------------------
*/

/*----------------------------------------------------------------------------
; FUNCTION CODE
----------------------------------------------------------------------------*/

Word16 pseudonoise(
    Word32 *pShift_reg,     /* i/o : Old CN generator shift register state */
    Word16 no_bits          /* i   : Number of bits                        */
)
{
    Word16 noise_bits;
    Word16 Sn;
    Word16 i;
    Word16 temp;

    noise_bits = 0;

    for (i = 0; i < no_bits; i++)
    {
        /* State n == 31 */
        if ((*pShift_reg & 0x00000001L) != 0)
        {
            Sn = 1;
        }
        else
        {
            Sn = 0;
        }

        /* State n == 3 */
        if ((*pShift_reg & 0x10000000L) != 0)
        {
            Sn ^= 1;
        }
        else
        {
            Sn ^= 0;
        }

        noise_bits <<= 1;

        temp = (Word16)((*pShift_reg) & 1);
        noise_bits |= temp;

        *pShift_reg >>= 1;
        if (Sn & 1)
        {
            *pShift_reg |= 0x40000000L;
        }
    }
    return noise_bits;
}

/*
------------------------------------------------------------------------------
 FUNCTION NAME: build_CN_code
------------------------------------------------------------------------------
 INPUT AND OUTPUT DEFINITIONS

 Inputs:
    pSeed = pointer to the Old CN generator shift register state (Word32)
    cod[] = array to hold the generated CN fixed code vector (Word16)
    pOverflow = pointer to overflow flag (Flag)

 Outputs:
    cod[] = generated CN fixed code vector (Word16)
    pSeed = Updated CN generator shift register state (Word16)
    pOverflow -> 1 if overflow occured

 Returns:
    None

 Global Variables Used:
    None

 Local Variables Needed:
    None

------------------------------------------------------------------------------
 FUNCTION DESCRIPTION

This function computes the comfort noise fixed codebook excitation. The gains
of the pulses are always +/-1.

------------------------------------------------------------------------------
 REQUIREMENTS

 None

------------------------------------------------------------------------------
 REFERENCES

 b_cn_cod.c, UMTS GSM AMR speech codec, R99 - Version 3.2.0, March 2, 2001

------------------------------------------------------------------------------
 PSEUDO-CODE

void build_CN_code (
    Word32 *seed,         // i/o : Old CN generator shift register state
    Word16 cod[]          // o   : Generated CN fixed codebook vector
)
{
   Word16 i, j, k;

   for (i = 0; i < L_SUBFR; i++)
   {
      cod[i] = 0;
   }

// The reference ETSI code uses a global flag for Overflow. However in the
// actual implementation a pointer to the overflow flag is passed into the
// function so that it can be passed into the basic math functions L_mult()
// and add()

   for (k = 0; k < NB_PULSE; k++)
   {
      i = pseudonoise (seed, 2);      // generate pulse position
      i = shr (extract_l (L_mult (i, 10)), 1);
      i = add (i, k);

      j = pseudonoise (seed, 1);      // generate sign

      if (j > 0)
      {
         cod[i] = 4096;
      }
      else
      {
         cod[i] = -4096;
      }
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
void build_CN_code(
    Word32 *pSeed,          /* i/o : Old CN generator shift register state  */
    Word16 cod[],           /* o   : Generated CN fixed codebook vector     */
    Flag   *pOverflow       /* i/o : Overflow flag                          */
)
{
    Word16 i, j, k;
    Word16 temp;

    for (i = 0; i < L_SUBFR; i++)
    {
        cod[i] = 0;
    }

    for (k = 0; k < NB_PULSE; k++)
    {
        i = pseudonoise(pSeed, 2);       /* generate pulse position */

        temp = (Word16)(L_mult(i, 10, pOverflow));
        i = temp >> 1;
        i = add_16(i, k, pOverflow);

        j = pseudonoise(pSeed, 1);       /* generate sign */

        if (j > 0)
        {
            cod[i] = 4096;
        }
        else
        {
            cod[i] = -4096;
        }
    }

    return;
}

/*
------------------------------------------------------------------------------
 FUNCTION NAME: build_CN_param
------------------------------------------------------------------------------
 INPUT AND OUTPUT DEFINITIONS

 Inputs:
    pSeed = pointer to the Old CN generator shift register state (Word32)
    n_param = Number of parameters to randomize (Word16)
    param_size_table = table holding paameter sizes (Word16)
    param[] = array to hold CN generated paramters (Word16)
    pOverflow = pointer to overflow flag (Flag)

 Outputs:
    param[] = CN generated parameters (Word16)
    pSeed = Updated CN generator shift register state (Word16)
    pOverflow -> 1 if overflow occured

 Returns:
    None

 Global Variables Used:
    None

 Local Variables Needed:
    None

------------------------------------------------------------------------------
 FUNCTION DESCRIPTION

This function randomizes the speech parameters, so that they do not produce
tonal artifacts if used by ECU.

------------------------------------------------------------------------------
 REQUIREMENTS

 None

------------------------------------------------------------------------------
 REFERENCES

 b_cn_cod.c, UMTS GSM AMR speech codec, R99 - Version 3.2.0, March 2, 2001

------------------------------------------------------------------------------
 PSEUDO-CODE
void build_CN_param (
    Word16 *seed,             // i/o : Old CN generator shift register state
    const Word16 n_param,           // i  : number of params
    const Word16 param_size_table[],// i : size of params
    Word16 parm[]                   // o : CN Generated params
    )
{
   Word16 i;
   const Word16 *p;

// The reference ETSI code uses a global flag for Overflow. However in the
// actual implementation a pointer to the overflow flag is passed into the
// function so that it can be passed into the basic math functions L_add()
// and L_mult()

   *seed = extract_l(L_add(L_shr(L_mult(*seed, 31821), 1), 13849L));

   p = &window_200_40[*seed & 0x7F];
   for(i=0; i< n_param;i++){
     parm[i] = *p++ & ~(0xFFFF<<param_size_table[i]);
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
void build_CN_param(
    Word16 *pSeed,          /* i/o : Old CN generator shift register state  */
    const Word16 n_param,           /* i  : number of params                */
    const Word16 param_size_table[],/* i : size of params                   */
    Word16 parm[],                  /* o : CN Generated params              */
    const Word16* window_200_40_ptr, /* i : ptr to read-only table          */
    Flag  *pOverflow                /* i/o : Overflow Flag                  */
)

{
    Word16 i;
    const Word16 *pTemp;
    Word32 L_temp;
    Word16 temp;

    L_temp = L_mult(*pSeed, 31821, pOverflow);
    L_temp >>= 1;

    *pSeed = (Word16)(L_add(L_temp, 13849L, pOverflow));

    pTemp = &window_200_40_ptr[*pSeed & 0x7F];

    for (i = 0; i < n_param; i++)
    {
        temp = ~(0xFFFF << param_size_table[i]);
        parm[i] = *pTemp++ & temp;
    }
}

