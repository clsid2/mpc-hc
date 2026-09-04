/* ------------------------------------------------------------------
 * Copyright (C) 1998-2010 PacketVideo
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

    3GPP TS 26.173
    ANSI-C code for the Adaptive Multi-Rate - Wideband (AMR-WB) speech codec
    Available from http://www.3gpp.org

(C) 2007, 3GPP Organizational Partners (ARIB, ATIS, CCSA, ETSI, TTA, TTC)
Permission to distribute, modify and use this file under the standard license
terms listed above has been obtained from the copyright holder.
****************************************************************************************/
/*
------------------------------------------------------------------------------



 Filename: isp_az.cpp

------------------------------------------------------------------------------
 INPUT AND OUTPUT DEFINITIONS

     int16 isp[],              (i) Q15 : Immittance spectral pairs
     int16 a[],                (o) Q12 : predictor coefficients (order=M)
     int16 m,                  (i)     : order
     int16 adaptive_scaling    (i) 0   : adaptive scaling disabled
                                   1   : adaptive scaling enabled


------------------------------------------------------------------------------
 FUNCTION DESCRIPTION

    Compute the LPC coefficients from isp (order=M)
------------------------------------------------------------------------------
 REQUIREMENTS


------------------------------------------------------------------------------
 REFERENCES

------------------------------------------------------------------------------
 PSEUDO-CODE

------------------------------------------------------------------------------
*/


/*----------------------------------------------------------------------------
; INCLUDES
----------------------------------------------------------------------------*/

#include "pv_amr_wb_type_defs.h"
#include "pvamrwbdecoder_basic_op.h"
#include "pvamrwbdecoder_cnst.h"
#include "pvamrwbdecoder_acelp.h"
#include "pvamrwb_math_op.h"

/*----------------------------------------------------------------------------
; MACROS
; Define module specific macros here
----------------------------------------------------------------------------*/


/*----------------------------------------------------------------------------
; DEFINES
; Include all pre-processor statements here. Include conditional
; compile variables also.
----------------------------------------------------------------------------*/
#define NC (M/2)
#define NC16k (M16k/2)

/*----------------------------------------------------------------------------
; LOCAL FUNCTION DEFINITIONS
; Function Prototype declaration
----------------------------------------------------------------------------*/


#ifdef __cplusplus
extern "C"
{
#endif

    void Get_isp_pol(int16 * isp, int32 * f, int16 n);
    void Get_isp_pol_16kHz(int16 * isp, int32 * f, int16 n);

#ifdef __cplusplus
}
#endif

/*----------------------------------------------------------------------------
; LOCAL STORE/BUFFER/POINTER DEFINITIONS
; Variable declaration - defined here and used outside this module
----------------------------------------------------------------------------*/

/*----------------------------------------------------------------------------
; EXTERNAL FUNCTION REFERENCES
; Declare functions defined elsewhere and referenced in this module
----------------------------------------------------------------------------*/

/*----------------------------------------------------------------------------
; EXTERNAL GLOBAL STORE/BUFFER/POINTER REFERENCES
; Declare variables used in this module but defined elsewhere
----------------------------------------------------------------------------*/

/*----------------------------------------------------------------------------
; FUNCTION CODE
----------------------------------------------------------------------------*/

void Isp_Az(
    int16 isp[],            /* (i) Q15 : Immittance spectral pairs         */
    int16 a[],              /* (o) Q12 : predictor coefficients (order=M)  */
    int16 m,                /* (i)     : order                     */
    int16 adaptive_scaling  /* (i) 0   : adaptive scaling disabled */
    /*     1   : adaptive scaling enabled  */
)
{
    int16 i, j;
    int32 f1[NC16k + 1], f2[NC16k];
    int16 nc;
    int32 t0;
    int32 t1;
    int16 q, q_sug;
    int32 tmax;

    nc = m >> 1;


    if (nc > 8)
    {
        Get_isp_pol_16kHz(&isp[0], f1, nc);
        for (i = 0; i <= nc; i++)
        {
            f1[i] = shl_int32(f1[i], 2);
        }
        Get_isp_pol_16kHz(&isp[1], f2, nc - 1);
        for (i = 0; i <= nc - 1; i++)
        {
            f2[i] = shl_int32(f2[i], 2);
        }
    }
    else
    {
        Get_isp_pol(&isp[0], f1, nc);
        Get_isp_pol(&isp[1], f2, nc - 1);
    }

    /*
     *  Multiply F2(z) by (1 - z^-2)
     */

    for (i = nc - 1; i > 1; i--)
    {
        f2[i] -= f2[i - 2];      /* f2[i] -= f2[i-2]; */
    }

    /*
     *  Scale F1(z) by (1+isp[m-1])  and  F2(z) by (1-isp[m-1])
     */

    for (i = 0; i < nc; i++)
    {
        /* f1[i] *= (1.0 + isp[M-1]); */

        /* f2[i] *= (1.0 - isp[M-1]); */
        t0 = f1[i];
        t1 = f2[i];
        t0 = fxp_mul32_by_16b(t0, isp[m - 1]) << 1;
        t1 = fxp_mul32_by_16b(t1, isp[m - 1]) << 1;
        f1[i] += t0;
        f2[i] -= t1;

    }

    /*
     *  A(z) = (F1(z)+F2(z))/2
     *  F1(z) is symmetric and F2(z) is antisymmetric
     */

    /* a[0] = 1.0; */
    a[0] = 4096;
    tmax = 1;
    j = m - 1;
    for (i = 1;  i < nc; i++)
    {
        /* a[i] = 0.5*(f1[i] + f2[i]); */

        t0 = add_int32(f1[i], f2[i]);          /* f1[i] + f2[i]             */
        /* compute t1 = abs(t0) */
        t1 = t0 - (t0 < 0);
        t1 = t1 ^(t1 >> 31);  /* t1 = t1 ^sign(t1) */

        tmax |= t1;
        /* from Q23 to Q12 and * 0.5 */
        a[i] = (int16)((t0 >> 12) + ((t0 >> 11) & 1));


        /* a[j] = 0.5*(f1[i] - f2[i]); */

        t0 = sub_int32(f1[i], f2[i]);          /* f1[i] - f2[i]             */
        /* compute t1 = abs(t0) */
        t1 = t0 - (t0 < 0);
        t1 = t1 ^(t1 >> 31);  /* t1 = t1 ^sign(t1) */

        tmax |= t1;

        /* from Q23 to Q12 and * 0.5 */
        a[j--] = (int16)((t0 >> 12) + ((t0 >> 11) & 1));

    }

    /* rescale data if overflow has occured and reprocess the loop */


    if (adaptive_scaling == 1)
    {
        q = 4 - normalize_amr_wb(tmax);        /* adaptive scaling enabled */
    }
    else
    {
        q = 0;                   /* adaptive scaling disabled */
    }


    if (q > 0)
    {
        q_sug = 12 + q;
        for (i = 1, j = m - 1; i < nc; i++, j--)
        {
            /* a[i] = 0.5*(f1[i] + f2[i]); */

            t0 = add_int32(f1[i], f2[i]);          /* f1[i] + f2[i]             */
            /* from Q23 to Q12 and * 0.5 */
            a[i] = (int16)((t0 >> q_sug) + ((t0 >> (q_sug - 1)) & 1));


            /* a[j] = 0.5*(f1[i] - f2[i]); */

            t0 = sub_int32(f1[i], f2[i]);          /* f1[i] - f2[i]             */
            /* from Q23 to Q12 and * 0.5 */
            a[j] = (int16)((t0 >> q_sug) + ((t0 >> (q_sug - 1)) & 1));

        }
        a[0] >>=  q;
    }
    else
    {
        q_sug = 12;
        q     = 0;
    }

    /* a[NC] = 0.5*f1[NC]*(1.0 + isp[M-1]); */
    t0 = (int32)(((int64)f1[nc] * isp[m - 1]) >> 16) << 1;
    t0 = add_int32(f1[nc], t0);

    /* from Q23 to Q12 and * 0.5 */
    a[nc] = (int16)((t0 >> q_sug) + ((t0 >> (q_sug - 1)) & 1));
    a[m] = shr_rnd(isp[m - 1], (3 + q));           /* from Q15 to Q12          */

    /* a[m] = isp[m-1]; */


    return;
}



/*
Get_isp_pol
------------------------------------------------------------------------------
 INPUT AND OUTPUT DEFINITIONS

   isp[]   : isp vector (cosine domaine)         in Q15
   f[]     : the coefficients of F1 or F2        in Q23
   n       : == NC for F1(z); == NC-1 for F2(z)


------------------------------------------------------------------------------
 FUNCTION DESCRIPTION

    Find the polynomial F1(z) or F2(z) from the ISPs.
  This is performed by expanding the product polynomials:

  F1(z) =   product   ( 1 - 2 isp_i z^-1 + z^-2 )
          i=0,2,4,6,8
  F2(z) =   product   ( 1 - 2 isp_i z^-1 + z^-2 )
          i=1,3,5,7

  where isp_i are the ISPs in the cosine domain.
------------------------------------------------------------------------------
 REQUIREMENTS


------------------------------------------------------------------------------
 REFERENCES

------------------------------------------------------------------------------
 PSEUDO-CODE

----------------------------------------------------------------------------*/

/*----------------------------------------------------------------------------
; FUNCTION CODE
----------------------------------------------------------------------------*/


void Get_isp_pol(int16 * isp, int32 * f, int16 n)
{
    int16 i, j;
    int32 t0;


    /* All computation in Q23 */

    f[0] = 0x00800000;                        /* f[0] = 1.0;        in Q23  */
    f[1] = -isp[0] << 9;                      /* f[1] = -2.0*isp[0] in Q23  */

    f += 2;                                   /* Advance f pointer          */
    isp += 2;                                 /* Advance isp pointer        */

    for (i = 2; i <= n; i++)
    {
        *f = f[-2];

        for (j = 1; j < i; j++)
        {

            t0 = fxp_mul32_by_16b(f[-1], *isp);
            t0 = shl_int32(t0, 2);

            *f -= t0;                      /* *f -= t0            */
            *(f) += f[-2];                 /* *f += f[-2]         */
            f--;


        }
        *f -= *isp << 9;

        f += i;                            /* Advance f pointer   */
        isp += 2;                          /* Advance isp pointer */
    }
}

void Get_isp_pol_16kHz(int16 * isp, int32 * f, int16 n)
{
    int16 i, j;
    int32 t0;

    /* All computation in Q23 */

    f[0] = 0x00200000;                        /* f[0] = 0.25;        in Q23  */

    f[1] = -isp[0] << 7;                      /* f[1] = -0.5*isp[0] in Q23  */

    f += 2;                                   /* Advance f pointer          */
    isp += 2;                                 /* Advance isp pointer        */

    for (i = 2; i <= n; i++)
    {
        *f = f[-2];

        for (j = 1; j < i; j++, f--)
        {
            t0 = fxp_mul32_by_16b(f[-1], *isp);
            t0 = shl_int32(t0, 2);

            *f -= t0;                      /* *f -= t0            */
            *f += f[-2];                   /* *f += f[-2]         */
        }
        *f -= *isp << 7;
        f += i;                            /* Advance f pointer   */
        isp += 2;                          /* Advance isp pointer */
    }
    return;
}

