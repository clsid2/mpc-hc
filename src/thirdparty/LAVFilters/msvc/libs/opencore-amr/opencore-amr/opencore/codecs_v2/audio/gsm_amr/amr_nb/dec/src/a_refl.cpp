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



 Filename: a_refl.cpp
 Functions: a_refl

------------------------------------------------------------------------------
*/


/*----------------------------------------------------------------------------
; INCLUDES
----------------------------------------------------------------------------*/
#include "a_refl.h"
#include "typedef.h"
#include "cnst.h"
#include "basic_op.h"

/*----------------------------------------------------------------------------
; MACROS [optional]
; [Define module specific macros here]
----------------------------------------------------------------------------*/

/*----------------------------------------------------------------------------
; DEFINES [optional]
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
 FUNCTION NAME: AMREncode
------------------------------------------------------------------------------
 INPUT AND OUTPUT DEFINITIONS

 Inputs:
    a[] = pointer to directform coefficients of type Word16
    refl[] = pointer to reflection coefficients of type Word16

 Outputs:
    pOverflow = 1 if overflow exists in the math operations else zero.

 Returns:
    None

 Global Variables Used:
    None

 Local Variables Needed:
    None

------------------------------------------------------------------------------
 FUNCTION DESCRIPTION

     File             : a_refl.c
     Purpose          : Convert from direct form coefficients to
                        reflection coefficients

------------------------------------------------------------------------------
 REQUIREMENTS

 None

------------------------------------------------------------------------------
 REFERENCES

 [1] a_refl.c , 3GPP TS 26.101 version 4.1.0 Release 4, June 2001

------------------------------------------------------------------------------
 PSEUDO-CODE


void A_Refl(
   Word16 a[],        // i   : Directform coefficients
   Word16 refl[]      // o   : Reflection coefficients
)
{
   // local variables
   Word16 i,j;
   Word16 aState[M];
   Word16 bState[M];
   Word16 normShift;
   Word16 normProd;
   Word32 L_acc;
   Word16 scale;
   Word32 L_temp;
   Word16 temp;
   Word16 mult;

   // initialize states
   for (i = 0; i < M; i++)
   {
      aState[i] = a[i];
   }

   // backward Levinson recursion
   for (i = M-1; i >= 0; i--)
   {
      if (sub(abs_s(aState[i]), 4096) >= 0)
      {
         goto ExitRefl;
      }

      refl[i] = shl(aState[i], 3);

      L_temp = L_mult(refl[i], refl[i]);
      L_acc = L_sub(MAX_32, L_temp);

      normShift = norm_l(L_acc);
      scale = sub(15, normShift);

      L_acc = L_shl(L_acc, normShift);
      normProd = pv_round(L_acc);

      mult = div_s(16384, normProd);

      for (j = 0; j < i; j++)
      {
         L_acc = L_deposit_h(aState[j]);
         L_acc = L_msu(L_acc, refl[i], aState[i-j-1]);

         temp = pv_round(L_acc);
         L_temp = L_mult(mult, temp);
         L_temp = L_shr_r(L_temp, scale);

         if (L_sub(L_abs(L_temp), 32767) > 0)
         {
            goto ExitRefl;
         }

         bState[j] = extract_l(L_temp);
      }

      for (j = 0; j < i; j++)
      {
         aState[j] = bState[j];
      }
   }
   return;

ExitRefl:
   for (i = 0; i < M; i++)
   {
      refl[i] = 0;
   }
}

------------------------------------------------------------------------------
 CAUTION [optional]
 [State any special notes, constraints or cautions for users of this function]

------------------------------------------------------------------------------
*/

void A_Refl(
    Word16 a[],        /* i   : Directform coefficients */
    Word16 refl[],     /* o   : Reflection coefficients */
    Flag   *pOverflow
)
{
    /* local variables */
    Word16 i;
    Word16 j;
    Word16 k;
    Word16 aState[M];
    Word16 bState[M];
    Word16 normShift;
    Word16 normProd;
    Word32 L_acc;
    Word16 scale;
    Word32 L_temp;
    Word16 temp;
    Word16 mult;

    /* initialize states */
    for (i = 0; i < M; i++)
    {
        aState[i] = a[i];
    }

    /* backward Levinson recursion */
    for (i = M - 1; i >= 0; i--)
    {
        if (abs_s(aState[i]) >= 4096)
        {
            for (j = 0; j < M; j++)
            {
                refl[j] = 0;
            }
            break;
        }

        refl[i] = shl(aState[i], 3, pOverflow);

        L_temp = L_mult(refl[i], refl[i], pOverflow);
        L_acc = L_sub(MAX_32, L_temp, pOverflow);

        normShift = norm_l(L_acc);
        scale = 15 - normShift;

        L_acc = L_shl(L_acc, normShift, pOverflow);
        normProd = pv_round(L_acc, pOverflow);

        mult = div_s(16384, normProd);

        for (j = 0; j < i; j++)
        {
            L_acc = ((Word32)aState[j] << 16);
            L_acc = L_msu(L_acc, refl[i], aState[i-j-1], pOverflow);

            temp = pv_round(L_acc, pOverflow);
            L_temp = L_mult(mult, temp, pOverflow);
            L_temp = L_shr_r(L_temp, scale, pOverflow);


            Word32 L_tmp_abs = L_temp - (L_temp < 0);
            L_tmp_abs = L_tmp_abs ^(L_tmp_abs >> 31);

            if (L_tmp_abs > 32767)
            {
                for (k = 0; k < M; k++)
                {
                    refl[k] = 0;
                }
                break;
            }

            bState[j] = (Word16)(L_temp);
        }

        for (j = 0; j < i; j++)
        {
            aState[j] = bState[j];
        }
    }
    return;
}







