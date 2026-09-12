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

 Filename: az_lsp.cpp
 Funtions: Chebps
           Chebps_Wrapper
           Az_lsp

------------------------------------------------------------------------------
 MODULE DESCRIPTION

 These modules compute the LSPs from the LP coefficients.

------------------------------------------------------------------------------
*/


/*----------------------------------------------------------------------------
; INCLUDES
----------------------------------------------------------------------------*/
#include "az_lsp.h"
#include "cnst.h"
#include "basic_op.h"

/*----------------------------------------------------------------------------
; MACROS
; Define module specific macros here
----------------------------------------------------------------------------*/


/*----------------------------------------------------------------------------
; DEFINES
; Include all pre-processor statements here. Include conditional
; compile variables also.
----------------------------------------------------------------------------*/
#define NC   M/2                  /* M = LPC order, NC = M/2 */

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
 FUNCTION NAME: Chebps
------------------------------------------------------------------------------
 INPUT AND OUTPUT DEFINITIONS

 Inputs:
    x = input value (Word16)
    f = polynomial (Word16)
    n = polynomial order (Word16)

    pOverflow = pointer to overflow (Flag)

 Outputs:
    pOverflow -> 1 if the operations in the function resulted in saturation.

 Returns:
    cheb = Chebyshev polynomial for the input value x.(Word16)

 Global Variables Used:
    None.

 Local Variables Needed:
    None.

------------------------------------------------------------------------------
 FUNCTION DESCRIPTION

 This module evaluates the Chebyshev polynomial series.
    - The polynomial order is   n = m/2 = 5
    - The polynomial F(z) (F1(z) or F2(z)) is given by
        F(w) = 2 exp(-j5w) C(x)
        where
        C(x) = T_n(x) + f(1)T_n-1(x) + ... +f(n-1)T_1(x) + f(n)/2
        and T_m(x) = cos(mw) is the mth order Chebyshev
        polynomial ( x=cos(w) )
    - C(x) for the input x is returned.

------------------------------------------------------------------------------
 REQUIREMENTS

 None.

------------------------------------------------------------------------------
 REFERENCES

 az_lsp.c, UMTS GSM AMR speech codec, R99 - Version 3.2.0, March 2, 2001

------------------------------------------------------------------------------
 PSEUDO-CODE

static Word16 Chebps (Word16 x,
                      Word16 f[], // (n)
                      Word16 n)
{
    Word16 i, cheb;
    Word16 b0_h, b0_l, b1_h, b1_l, b2_h, b2_l;
    Word32 t0;

// The reference ETSI code uses a global flag for Overflow. However, in the
// actual implementation a pointer to Overflow flag is passed in as a
// parameter to the function. This pointer is passed into all the basic math
// functions invoked

    b2_h = 256; // b2 = 1.0
    b2_l = 0;

    t0 = L_mult (x, 512);          // 2*x
    t0 = L_mac (t0, f[1], 8192);   // + f[1]
    L_Extract (t0, &b1_h, &b1_l);  // b1 = 2*x + f[1]

    for (i = 2; i < n; i++)
    {
        t0 = Mpy_32_16 (b1_h, b1_l, x);         // t0 = 2.0*x*b1
        t0 = L_shl (t0, 1);
        t0 = L_mac (t0, b2_h, (Word16) 0x8000); // t0 = 2.0*x*b1 - b2
        t0 = L_msu (t0, b2_l, 1);
        t0 = L_mac (t0, f[i], 8192);            // t0 = 2.0*x*b1 - b2 + f[i]

        L_Extract (t0, &b0_h, &b0_l);           // b0 = 2.0*x*b1 - b2 + f[i]

        b2_l = b1_l; // b2 = b1;
        b2_h = b1_h;
        b1_l = b0_l; // b1 = b0;
        b1_h = b0_h;
    }

    t0 = Mpy_32_16 (b1_h, b1_l, x);             // t0 = x*b1;
    t0 = L_mac (t0, b2_h, (Word16) 0x8000);     // t0 = x*b1 - b2
    t0 = L_msu (t0, b2_l, 1);
    t0 = L_mac (t0, f[i], 4096);                // t0 = x*b1 - b2 + f[i]/2

    t0 = L_shl (t0, 6);

    cheb = extract_h (t0);

    return (cheb);
}

------------------------------------------------------------------------------
 CAUTION [optional]
 [State any special notes, constraints or cautions for users of this function]

------------------------------------------------------------------------------
*/

static Word16 Chebps(Word16 x,
                     Word16 f[], /* (n) */
                     Word16 n,
                     Flag *pOverflow)
{
    Word16 i;
    Word16 cheb;
    Word16 b1_h;
    Word16 b1_l;
    Word32 t0;
    Word32 L_temp;
    Word16 *p_f = &f[1];

    OSCL_UNUSED_ARG(pOverflow);

    /* L_temp = 1.0 */

    L_temp = 0x01000000L;

    t0 = ((Word32) x << 10) + ((Word32) * (p_f++) << 14);

    /* b1 = t0 = 2*x + f[1]  */

    b1_h = (Word16)(t0 >> 16);
    b1_l = (Word16)((t0 >> 1) - (b1_h << 15));


    for (i = 2; i < n; i++)
    {
        /* t0 = 2.0*x*b1    */
        t0  = ((Word32) b1_h * x);
        t0 += ((Word32) b1_l * x) >> 15;
        t0 <<= 2;

        /* t0 = 2.0*x*b1 - b2   */
        t0 -= L_temp;

        /* t0 = 2.0*x*b1 - b2 + f[i] */
        t0 += (Word32) * (p_f++) << 14;

        L_temp = ((Word32) b1_h << 16) + ((Word32) b1_l << 1);

        /* b0 = 2.0*x*b1 - b2 + f[i]*/
        b1_h = (Word16)(t0 >> 16);
        b1_l = (Word16)((t0 >> 1) - (b1_h << 15));

    }

    /* t0 = x*b1; */
    t0  = ((Word32) b1_h * x);
    t0 += ((Word32) b1_l * x) >> 15;
    t0 <<= 1;


    /* t0 = x*b1 - b2   */
    t0 -= L_temp;

    /* t0 = x*b1 - b2 + f[i]/2 */
    t0 += (Word32) * (p_f) << 13;


    if ((UWord32)(t0 + 33554432) < 67108863)
    {
        cheb = (Word16)(t0 >> 10);
    }
    else
    {
        if (t0 > (Word32) 0x01ffffffL)
        {
            cheb = MAX_16;

        }
        else
        {
            cheb = MIN_16;
        }
    }

    return (cheb);
}


/*
------------------------------------------------------------------------------
 FUNCTION NAME: Az_lsp
------------------------------------------------------------------------------
 INPUT AND OUTPUT DEFINITIONS FOR Az_lsp

 Inputs:
    a = predictor coefficients (Word16)
    lsp = line spectral pairs (Word16)
    old_lsp = old line spectral pairs (Word16)

    pOverflow = pointer to overflow (Flag)

 Outputs:
    pOverflow -> 1 if the operations in the function resulted in saturation.

 Returns:
    None.

 Global Variables Used:
    None.

 Local Variables Needed:
    None.

------------------------------------------------------------------------------
 FUNCTION DESCRIPTION

 This function computes the LSPs from the LP coefficients.

 The sum and difference filters are computed and divided by 1+z^{-1} and
 1-z^{-1}, respectively.

     f1[i] = a[i] + a[11-i] - f1[i-1] ;   i=1,...,5
     f2[i] = a[i] - a[11-i] + f2[i-1] ;   i=1,...,5

 The roots of F1(z) and F2(z) are found using Chebyshev polynomial evaluation.
 The polynomials are evaluated at 60 points regularly spaced in the
 frequency domain. The sign change interval is subdivided 4 times to better
 track the root. The LSPs are found in the cosine domain [1,-1].

 If less than 10 roots are found, the LSPs from the past frame are used.

------------------------------------------------------------------------------
 REQUIREMENTS

 None.

------------------------------------------------------------------------------
 REFERENCES

 az_lsp.c, UMTS GSM AMR speech codec, R99 - Version 3.2.0, March 2, 2001

------------------------------------------------------------------------------
 PSEUDO-CODE

void Az_lsp (
    Word16 a[],         // (i)  : predictor coefficients (MP1)
    Word16 lsp[],       // (o)  : line spectral pairs (M)
    Word16 old_lsp[]    // (i)  : old lsp[] (in case not found 10 roots) (M)
)
{
    Word16 i, j, nf, ip;
    Word16 xlow, ylow, xhigh, yhigh, xmid, ymid, xint;
    Word16 x, y, sign, exp;
    Word16 *coef;
    Word16 f1[M / 2 + 1], f2[M / 2 + 1];
    Word32 t0;

     *-------------------------------------------------------------*
     *  find the sum and diff. pol. F1(z) and F2(z)                *
     *    F1(z) <--- F1(z)/(1+z**-1) & F2(z) <--- F2(z)/(1-z**-1)  *
     *                                                             *
     * f1[0] = 1.0;                                                *
     * f2[0] = 1.0;                                                *
     *                                                             *
     * for (i = 0; i< NC; i++)                                     *
     * {                                                           *
     *   f1[i+1] = a[i+1] + a[M-i] - f1[i] ;                       *
     *   f2[i+1] = a[i+1] - a[M-i] + f2[i] ;                       *
     * }                                                           *
     *-------------------------------------------------------------*

    f1[0] = 1024; // f1[0] = 1.0
    f2[0] = 1024; // f2[0] = 1.0

// The reference ETSI code uses a global flag for Overflow. However, in the
// actual implementation a pointer to Overflow flag is passed in as a
// parameter to the function. This pointer is passed into all the basic math
// functions invoked

    for (i = 0; i < NC; i++)
    {
        t0 = L_mult (a[i + 1], 8192);   // x = (a[i+1] + a[M-i]) >> 2
        t0 = L_mac (t0, a[M - i], 8192);
        x = extract_h (t0);
        // f1[i+1] = a[i+1] + a[M-i] - f1[i]
        f1[i + 1] = sub (x, f1[i]);

        t0 = L_mult (a[i + 1], 8192);   // x = (a[i+1] - a[M-i]) >> 2
        t0 = L_msu (t0, a[M - i], 8192);
        x = extract_h (t0);
        // f2[i+1] = a[i+1] - a[M-i] + f2[i]
        f2[i + 1] = add (x, f2[i]);
    }

     *-------------------------------------------------------------*
     * find the LSPs using the Chebychev pol. evaluation           *
     *-------------------------------------------------------------*

    nf = 0; // number of found frequencies
    ip = 0; // indicator for f1 or f2

    coef = f1;

    xlow = grid[0];
    ylow = Chebps (xlow, coef, NC);

    j = 0;
    // while ( (nf < M) && (j < grid_points) )
    while ((sub (nf, M) < 0) && (sub (j, grid_points) < 0))
    {
        j++;
        xhigh = xlow;
        yhigh = ylow;
        xlow = grid[j];
        ylow = Chebps (xlow, coef, NC);

        if (L_mult (ylow, yhigh) <= (Word32) 0L)
        {

            // divide 4 times the interval

            for (i = 0; i < 4; i++)
            {
                // xmid = (xlow + xhigh)/2
                xmid = add (shr (xlow, 1), shr (xhigh, 1));
                ymid = Chebps (xmid, coef, NC);

                if (L_mult (ylow, ymid) <= (Word32) 0L)
                {
                    yhigh = ymid;
                    xhigh = xmid;
                }
                else
                {
                    ylow = ymid;
                    xlow = xmid;
                }
            }

             *-------------------------------------------------------------*
             * Linear interpolation                                        *
             *    xint = xlow - ylow*(xhigh-xlow)/(yhigh-ylow);            *
             *-------------------------------------------------------------*

            x = sub (xhigh, xlow);
            y = sub (yhigh, ylow);

            if (y == 0)
            {
                xint = xlow;
            }
            else
            {
                sign = y;
                y = abs_s (y);
                exp = norm_s (y);
                y = shl (y, exp);
                y = div_s ((Word16) 16383, y);
                t0 = L_mult (x, y);
                t0 = L_shr (t0, sub (20, exp));
                y = extract_l (t0);     // y= (xhigh-xlow)/(yhigh-ylow)

                if (sign < 0)
                    y = negate (y);

                t0 = L_mult (ylow, y);
                t0 = L_shr (t0, 11);
                xint = sub (xlow, extract_l (t0)); // xint = xlow - ylow*y
            }

            lsp[nf] = xint;
            xlow = xint;
            nf++;

            if (ip == 0)
            {
                ip = 1;
                coef = f2;
            }
            else
            {
                ip = 0;
                coef = f1;
            }
            ylow = Chebps (xlow, coef, NC);

        }
    }

    // Check if M roots found

    if (sub (nf, M) < 0)
    {
        for (i = 0; i < M; i++)
        {
            lsp[i] = old_lsp[i];
        }

    }
    return;
}

------------------------------------------------------------------------------
 CAUTION [optional]
 [State any special notes, constraints or cautions for users of this function]

------------------------------------------------------------------------------
*/

void Az_lsp(
    Word16 a[],         /* (i)  : predictor coefficients (MP1)               */
    Word16 lsp[],       /* (o)  : line spectral pairs (M)                    */
    Word16 old_lsp[],   /* (i)  : old lsp[] (in case not found 10 roots) (M) */
    Flag   *pOverflow   /* (i/o): overflow flag                              */
)
{
    register Word16 i;
    register Word16 j;
    register Word16 nf;
    register Word16 ip;
    Word16 xlow;
    Word16 ylow;
    Word16 xhigh;
    Word16 yhigh;
    Word16 xmid;
    Word16 ymid;
    Word16 xint;
    Word16 x;
    Word16 y;
    Word16 sign;
    Word16 exp;
    Word16 *coef;
    Word16 f1[NC + 1];
    Word16 f2[NC + 1];
    Word32 L_temp1;
    Word32 L_temp2;
    Word16 *p_f1 = f1;
    Word16 *p_f2 = f2;

    /*-------------------------------------------------------------*
     *  find the sum and diff. pol. F1(z) and F2(z)                *
     *    F1(z) <--- F1(z)/(1+z**-1) & F2(z) <--- F2(z)/(1-z**-1)  *
     *                                                             *
     * f1[0] = 1.0;                                                *
     * f2[0] = 1.0;                                                *
     *                                                             *
     * for (i = 0; i< NC; i++)                                     *
     * {                                                           *
     *   f1[i+1] = a[i+1] + a[M-i] - f1[i] ;                       *
     *   f2[i+1] = a[i+1] - a[M-i] + f2[i] ;                       *
     * }                                                           *
     *-------------------------------------------------------------*/

    *p_f1 = 1024;                       /* f1[0] = 1.0 */
    *p_f2 = 1024;                       /* f2[0] = 1.0 */

    for (i = 0; i < NC; i++)
    {
        L_temp1 = (Word32) * (a + i + 1);
        L_temp2 = (Word32) * (a + M - i);
        /* x = (a[i+1] + a[M-i]) >> 2  */
        x = (Word16)((L_temp1 + L_temp2) >> 2);
        /* y = (a[i+1] - a[M-i]) >> 2 */
        y = (Word16)((L_temp1 - L_temp2) >> 2);
        /* f1[i+1] = a[i+1] + a[M-i] - f1[i] */
        x -= *(p_f1++);
        *(p_f1) = x;
        /* f2[i+1] = a[i+1] - a[M-i] + f2[i] */
        y += *(p_f2++);
        *(p_f2) = y;
    }

    /*-------------------------------------------------------------*
     * find the LSPs using the Chebychev pol. evaluation           *
     *-------------------------------------------------------------*/

    nf = 0;                         /* number of found frequencies */
    ip = 0;                         /* indicator for f1 or f2      */

    coef = f1;

    xlow = *(grid);
    ylow = Chebps(xlow, coef, NC, pOverflow);

    j = 0;

    while ((nf < M) && (j < grid_points))
    {
        j++;
        xhigh = xlow;
        yhigh = ylow;
        xlow = *(grid + j);
        ylow = Chebps(xlow, coef, NC, pOverflow);

        if (((Word32)ylow*yhigh) <= 0)
        {
            /* divide 4 times the interval */
            for (i = 4; i != 0; i--)
            {
                /* xmid = (xlow + xhigh)/2 */
                x = xlow >> 1;
                y = xhigh >> 1;
                xmid = x + y;

                ymid = Chebps(xmid, coef, NC, pOverflow);

                if (((Word32)ylow*ymid) <= 0)
                {
                    yhigh = ymid;
                    xhigh = xmid;
                }
                else
                {
                    ylow = ymid;
                    xlow = xmid;
                }
            }

            /*-------------------------------------------------------------*
             * Linear interpolation                                        *
             *    xint = xlow - ylow*(xhigh-xlow)/(yhigh-ylow);            *
             *-------------------------------------------------------------*/

            x = xhigh - xlow;
            y = yhigh - ylow;

            if (y == 0)
            {
                xint = xlow;
            }
            else
            {
                sign = y;
                y = abs_s(y);
                exp = norm_s(y);
                y <<= exp;
                y = div_s((Word16) 16383, y);

                y = ((Word32)x * y) >> (19 - exp);

                if (sign < 0)
                {
                    y = -y;
                }

                /* xint = xlow - ylow*y */
                xint = xlow - (((Word32) ylow * y) >> 10);
            }

            *(lsp + nf) = xint;
            xlow = xint;
            nf++;

            if (ip == 0)
            {
                ip = 1;
                coef = f2;
            }
            else
            {
                ip = 0;
                coef = f1;
            }

            ylow = Chebps(xlow, coef, NC, pOverflow);

        }
    }

    /* Check if M roots found */

    if (nf < M)
    {
        for (i = NC; i != 0 ; i--)
        {
            *lsp++ = *old_lsp++;
            *lsp++ = *old_lsp++;
        }
    }

}

Word16 Chebps_Wrapper(Word16 x,
                      Word16 f[], /* (n) */
                      Word16 n,
                      Flag *pOverflow)
{
    return Chebps(x, f, n, pOverflow);
}

