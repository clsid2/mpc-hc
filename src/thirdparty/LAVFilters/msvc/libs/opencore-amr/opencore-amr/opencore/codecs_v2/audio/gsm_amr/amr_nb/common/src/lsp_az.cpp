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

 Filename: lsp_az.cpp
 Functions: Get_lsp_pol
           Lsp_Az

------------------------------------------------------------------------------
 MODULE DESCRIPTION

 This file contains functions that convert line spectral pairs (LSP) to
 linear predictive (LP) coefficients (filter order = 10). The functions
 included in this file include Get_lsp_pol, which finds the coefficients of
 F1(z) and F2(z), and Lsp_Az, which converts LSP to LPC by multiplying
 F1(z) by 1+z^(-1) and F2(z) by 1-z^(-1), then calculating A(z) = (F1(z) +
 F2(z))/2.

------------------------------------------------------------------------------
*/

/*----------------------------------------------------------------------------
; INCLUDES
----------------------------------------------------------------------------*/
#include "lsp_az.h"

/*----------------------------------------------------------------------------
; MACROS
; Define module specific macros here
----------------------------------------------------------------------------*/

/*----------------------------------------------------------------------------
; DEFINES
; Include all pre-processor statements here. Include conditional
; compile variables also.
----------------------------------------------------------------------------*/

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
 FUNCTION NAME: Get_lsp_pol
------------------------------------------------------------------------------
 INPUT AND OUTPUT DEFINITIONS

 Inputs:
    lsp = pointer to the buffer containing the line spectral pairs (LSP)
          of type Word16
    f = pointer to the polynomial of type Word32 to be generated

    pOverflow  = pointer set in case where one of the operations overflows.
                 [data type Pointer to Flag]

 Outputs:
    buffer pointed to by f contains the polynomial generated

    pOverflow  = pointer set in case where one of the operations overflows.
                 [data type Pointer to Flag]

 Returns:
    None

 Global Variables Used:
    None

 Local Variables Needed:
    None

------------------------------------------------------------------------------
 FUNCTION DESCRIPTION

 This function finds the polynomial F1(z) or F2(z) from the LSPs. If the LSP
 vector is passed at address 0, F1(z) is computed and if it is passed at
 address 1, F2(z) is computed.

 This is performed by expanding the product polynomials:

    F1(z) =   product   ( 1 - 2 lsp[i] z^-1 + z^-2 )
        i=0,2,4,6,8
    F2(z) =   product   ( 1 - 2 lsp[i] z^-1 + z^-2 )
        i=1,3,5,7,9

 where lsp[] is the LSP vector in the cosine domain.

 The expansion is performed using the following recursion:

    f[0] = 1
    b = -2.0 * lsp[0]
    f[1] = b
    for i=2 to 5 do
        b = -2.0 * lsp[2*i-2];
        for j=i-1 down to 2 do
            f[j] = f[j] + b*f[j-1] + f[j-2];
        f[1] = f[1] + b;

------------------------------------------------------------------------------
 REQUIREMENTS

 None

------------------------------------------------------------------------------
 REFERENCES

 lsp_az.c, UMTS GSM AMR speech codec, R99 - Version 3.2.0, March 2, 2001

------------------------------------------------------------------------------
 PSEUDO-CODE

static void Get_lsp_pol (Word16 *lsp, Word32 *f)
{
    Word16 i, j, hi, lo;
    Word32 t0;

    // f[0] = 1.0;
    *f = L_mult (4096, 2048);
    f++;
    *f = L_msu ((Word32) 0, *lsp, 512);    // f[1] =  -2.0 * lsp[0];
    f++;
    lsp += 2;                              // Advance lsp pointer

    for (i = 2; i <= 5; i++)
    {
        *f = f[-2];

        for (j = 1; j < i; j++, f--)
        {
            L_Extract (f[-1], &hi, &lo);
            t0 = Mpy_32_16 (hi, lo, *lsp); // t0 = f[-1] * lsp
            t0 = L_shl (t0, 1);
            *f = L_add (*f, f[-2]); // *f += f[-2]
            *f = L_sub (*f, t0); // *f -= t0
        }
        *f = L_msu (*f, *lsp, 512); // *f -= lsp<<9
        f += i;                            // Advance f pointer
        lsp += 2;                          // Advance lsp pointer
    }

    return;
}

------------------------------------------------------------------------------
 CAUTION [optional]
 [State any special notes, constraints or cautions for users of this function]

------------------------------------------------------------------------------
*/

static void Get_lsp_pol(
    Word16 *lsp,
    Word32 *f,
    Flag   *pOverflow)
{
    register Word16 i;
    register Word16 j;

    Word16 hi;
    Word16 lo;
    Word32 t0;
    OSCL_UNUSED_ARG(pOverflow);

    /* f[0] = 1.0;             */
    *f++ = (Word32) 0x01000000;
    *f++ = (Word32) - *(lsp++) << 10;       /* f[1] =  -2.0 * lsp[0];  */
    lsp++;                                  /* Advance lsp pointer     */

    for (i = 2; i <= 5; i++)
    {
        *f = *(f - 2);

        for (j = 1; j < i; j++)
        {
            hi = (Word16)(*(f - 1) >> 16);

            lo = (Word16)((*(f - 1) >> 1) - ((Word32) hi << 15));

            t0  = ((Word32)hi * *lsp);
            t0 += ((Word32)lo * *lsp) >> 15;

            *(f) +=  *(f - 2);          /*      *f += f[-2]      */
            *(f--) -=  t0 << 2;         /*      *f -= t0         */

        }

        *f -= (Word32)(*lsp++) << 10;

        f  += i;
        lsp++;
    }

    return;
}

/****************************************************************************/




/*
------------------------------------------------------------------------------
 FUNCTION NAME: Lsp_Az
------------------------------------------------------------------------------
 INPUT AND OUTPUT DEFINITIONS

 Inputs:
    lsp = pointer to the buffer containing the line spectral pairs (LSP)
          of type Word16

      a = pointer to the buffer containing Linear Predictive (LP)
          coefficients of type Word16 to be generated

    pOverflow  = pointer set in case where one of the operations overflows.
                 [data type Pointer to Flag]

 Local Stores/Buffers/Pointers Needed:
    None

 Global Stores/Buffers/Pointers Needed:
    None

 Outputs:
    pOverflow  = pointer set in case where one of the operations overflows.
                 [data type Pointer to Flag]

 Pointers and Buffers Modified:
    a buffer contains the generated Linear Predictive (LP) coefficients

 Local Stores Modified:
    None

 Global Stores Modified:
        None

------------------------------------------------------------------------------
 FUNCTION DESCRIPTION

 This function converts from the line spectral pairs (LSP) to LP coefficients
 for a 10th order filter.

 This is done by:
    (1) Find the coefficients of F1(z) and F2(z) (see Get_lsp_pol)
    (2) Multiply F1(z) by 1+z^{-1} and F2(z) by 1-z^{-1}
    (3) A(z) = ( F1(z) + F2(z) ) / 2

------------------------------------------------------------------------------
 REQUIREMENTS

 None

------------------------------------------------------------------------------
 REFERENCES

 lsp_az.c, UMTS GSM AMR speech codec, R99 - Version 3.2.0, March 2, 2001

------------------------------------------------------------------------------
 PSEUDO-CODE

void Lsp_Az (
    Word16 lsp[],        // (i)  : line spectral frequencies
    Word16 a[]           // (o)  : predictor coefficients (order = 10)
)
{
    Word16 i, j;
    Word32 f1[6], f2[6];
    Word32 t0;

    Get_lsp_pol (&lsp[0], f1);
    Get_lsp_pol (&lsp[1], f2);

    for (i = 5; i > 0; i--)
    {
        f1[i] = L_add (f1[i], f1[i - 1]); // f1[i] += f1[i-1];
        f2[i] = L_sub (f2[i], f2[i - 1]); // f2[i] -= f2[i-1];
    }

    a[0] = 4096;
    for (i = 1, j = 10; i <= 5; i++, j--)
    {
        t0 = L_add (f1[i], f2[i]);           // f1[i] + f2[i]
        a[i] = extract_l (L_shr_r (t0, 13));
        t0 = L_sub (f1[i], f2[i]);           // f1[i] - f2[i]
        a[j] = extract_l (L_shr_r (t0, 13));
    }

    return;
}

------------------------------------------------------------------------------
 CAUTION [optional]
 [State any special notes, constraints or cautions for users of this function]

------------------------------------------------------------------------------
*/

OSCL_EXPORT_REF void Lsp_Az(
    Word16 lsp[],        /* (i)  : line spectral frequencies            */
    Word16 a[],          /* (o)  : predictor coefficients (order = 10)  */
    Flag  *pOverflow     /* (o)  : overflow flag                        */
)
{
    register Word16 i;
    register Word16 j;

    Word32 f1[6];
    Word32 f2[6];
    Word32 t0;
    Word32 t1;
    Word16 *p_a = &a[0];
    Word32 *p_f1;
    Word32 *p_f2;

    Get_lsp_pol(&lsp[0], f1, pOverflow);

    Get_lsp_pol(&lsp[1], f2, pOverflow);

    p_f1 = &f1[5];
    p_f2 = &f2[5];

    for (i = 5; i > 0; i--)
    {
        *(p_f1--) += f1[i-1];
        *(p_f2--) -= f2[i-1];
    }

    *(p_a++) = 4096;
    p_f1 = &f1[1];
    p_f2 = &f2[1];

    for (i = 1, j = 10; i <= 5; i++, j--)
    {
        t0 = *(p_f1) + *(p_f2);               /* f1[i] + f2[i] */
        t1 = *(p_f1++) - *(p_f2++);           /* f1[i] - f2[i] */

        t0 = t0 + ((Word32) 1 << 12);
        t1 = t1 + ((Word32) 1 << 12);

        *(p_a++) = (Word16)(t0 >> 13);
        a[j]     = (Word16)(t1 >> 13);
    }

    return;
}
