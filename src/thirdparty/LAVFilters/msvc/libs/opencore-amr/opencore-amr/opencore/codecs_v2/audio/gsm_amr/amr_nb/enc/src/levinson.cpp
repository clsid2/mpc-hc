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



 Filename: levinson.cpp
 Funtions: Levinson_init
           Levinson_reset
           Levinson_exit
           Levinson

------------------------------------------------------------------------------
 MODULE DESCRIPTION

 This file contains the function the implements the Levinson-Durbin algorithm
 using double-precision arithmetic. This file also includes functions to
 initialize, allocate, and deallocate memory used by the Levinson function.

------------------------------------------------------------------------------
*/

/*----------------------------------------------------------------------------
; INCLUDES
----------------------------------------------------------------------------*/
#include "levinson.h"
#include "basicop_malloc.h"
#include "basic_op.h"
#include "l_abs.h"
#include "div_32.h"
#include "cnst.h"
#include "oscl_mem.h"

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
 FUNCTION NAME: Levinson_init
------------------------------------------------------------------------------
 INPUT AND OUTPUT DEFINITIONS

 Inputs:
    state = pointer to an array of pointers to structures of type
            LevinsonState

 Outputs:
    pointer pointed to by state points to the newly allocated memory to
      be used by Levinson function

 Returns:
    return_value = 0, if initialization was successful; -1, otherwise (int)

 Global Variables Used:
    None

 Local Variables Needed:
    None

------------------------------------------------------------------------------
 FUNCTION DESCRIPTION

 This function allocates and initializes the state memory used by the
 Levinson function.

------------------------------------------------------------------------------
 REQUIREMENTS

 None

------------------------------------------------------------------------------
 REFERENCES

 levinson.c, UMTS GSM AMR speech codec, R99 - Version 3.2.0, March 2, 2001

------------------------------------------------------------------------------
 PSEUDO-CODE

int Levinson_init (LevinsonState **state)
{
  LevinsonState* s;

  if (state == (LevinsonState **) NULL){
      //fprint(stderr, "Levinson_init: invalid parameter\n");
      return -1;
  }
  *state = NULL;

  // allocate memory
  if ((s= (LevinsonState *) malloc(sizeof(LevinsonState))) == NULL){
      //fprint(stderr, "Levinson_init: can not malloc state structure\n");
      return -1;
  }

  Levinson_reset(s);
  *state = s;

  return 0;
}

------------------------------------------------------------------------------
 CAUTION [optional]
 [State any special notes, constraints or cautions for users of this function]

------------------------------------------------------------------------------
*/

Word16 Levinson_init(LevinsonState **state)
{
    LevinsonState* s;

    if (state == (LevinsonState **) NULL)
    {
        /*  fprint(stderr, "Levinson_init: invalid parameter\n");  */
        return(-1);
    }
    *state = NULL;

    /* allocate memory */
    if ((s = (LevinsonState *) oscl_malloc(sizeof(LevinsonState))) == NULL)
    {
        /*  fprint(stderr, "Levinson_init:
                            can not malloc state structure\n");  */
        return(-1);
    }

    Levinson_reset(s);
    *state = s;

    return(0);
}

/****************************************************************************/


/*
------------------------------------------------------------------------------
 FUNCTION NAME: Levinson_reset
------------------------------------------------------------------------------
 INPUT AND OUTPUT DEFINITIONS

 Inputs:
    state = pointer to structures of type LevinsonState

 Outputs:
    old_A field of structure pointed to by state is initialized to 4096
      (first location) and the rest to zeros

 Returns:
    return_value = 0, if reset was successful; -1, otherwise (int)

 Global Variables Used:
    None

 Local Variables Needed:
    None

------------------------------------------------------------------------------
 FUNCTION DESCRIPTION

 This function initializes the state memory used by the Levinson function to
 zero.

------------------------------------------------------------------------------
 REQUIREMENTS

 None

------------------------------------------------------------------------------
 REFERENCES

 levinson.c, UMTS GSM AMR speech codec, R99 - Version 3.2.0, March 2, 2001

------------------------------------------------------------------------------
 PSEUDO-CODE

int Levinson_reset (LevinsonState *state)
{
  Word16 i;

  if (state == (LevinsonState *) NULL){
      fprint(stderr, "Levinson_reset: invalid parameter\n");
      return -1;
  }

  state->old_A[0] = 4096;
  for(i = 1; i < M + 1; i++)
      state->old_A[i] = 0;

  return 0;
}

------------------------------------------------------------------------------
 CAUTION [optional]
 [State any special notes, constraints or cautions for users of this function]

------------------------------------------------------------------------------
*/

Word16 Levinson_reset(LevinsonState *state)
{
    Word16 i;

    if (state == (LevinsonState *) NULL)
    {
        /*  fprint(stderr, "Levinson_reset: invalid parameter\n");  */
        return(-1);
    }

    state->old_A[0] = 4096;
    for (i = 1; i < M + 1; i++)
    {
        state->old_A[i] = 0;
    }

    return(0);
}

/****************************************************************************/

/*
------------------------------------------------------------------------------
 FUNCTION NAME: Levinson_exit
------------------------------------------------------------------------------
 INPUT AND OUTPUT DEFINITIONS

 Inputs:
    state = pointer to an array of pointers to structures of type
            LevinsonState

 Outputs:
    pointer pointed to by state is set to the NULL address

 Returns:
    None

 Global Variables Used:
    None

 Local Variables Needed:
    None

------------------------------------------------------------------------------
 FUNCTION DESCRIPTION

 This function deallocates the state memory used by the Levinson function.

------------------------------------------------------------------------------
 REQUIREMENTS

 None

------------------------------------------------------------------------------
 REFERENCES

 levinson.c, UMTS GSM AMR speech codec, R99 - Version 3.2.0, March 2, 2001

------------------------------------------------------------------------------
 PSEUDO-CODE

void Levinson_exit (LevinsonState **state)
{
  if (state == NULL || *state == NULL)
      return;

  // deallocate memory
  free(*state);
  *state = NULL;

  return;
}

------------------------------------------------------------------------------
 CAUTION [optional]
 [State any special notes, constraints or cautions for users of this function]

------------------------------------------------------------------------------
*/

void Levinson_exit(LevinsonState **state)
{
    if (state == NULL || *state == NULL)
    {
        return;
    }

    /* deallocate memory */
    oscl_free(*state);
    *state = NULL;

    return;
}

/****************************************************************************/


/*
------------------------------------------------------------------------------
 FUNCTION NAME: Levinson
------------------------------------------------------------------------------
 INPUT AND OUTPUT DEFINITIONS

 Inputs:
    st = pointer to structures of type LevinsonState
    Rh = vector containing most significant byte of
         autocorrelation values (Word16)
    Rl = vector containing least significant byte of
         autocorrelation values (Word16)
    A = vector of LPC coefficients (10th order) (Word16)
    rc = vector containing first four reflection coefficients (Word16)
    pOverflow = pointer to overflow indicator (Flag)

 Outputs:
    A contains the newly calculated LPC coefficients
    rc contains the newly calculated reflection coefficients

 Returns:
    return_value = 0 (int)

 Global Variables Used:
    None

 Local Variables Needed:
    None

------------------------------------------------------------------------------
 FUNCTION DESCRIPTION

 This function implements the Levinson-Durbin algorithm using double-
 precision arithmetic. This is used to compute the Linear Predictive (LP)
 filter parameters from the speech autocorrelation values.

 The algorithm implemented is as follows:
    A[0] = 1
    K    = -R[1]/R[0]
    A[1] = K
    Alpha = R[0] * (1-K**2]

    FOR  i = 2 to M

        S =  SUM ( R[j]*A[i-j] ,j=1,i-1 ) +  R[i]
        K = -S / Alpha

        FOR j = 1 to  i-1
            An[j] = A[j] + K*A[i-j]  where   An[i] = new A[i]
        ENDFOR

        An[i]=K
        Alpha=Alpha * (1-K**2)

    END

 where:
    R[i] = autocorrelations
    A[i] = filter coefficients
    K = reflection coefficient
    Alpha = prediction gain

------------------------------------------------------------------------------
 REQUIREMENTS

 None

------------------------------------------------------------------------------
 REFERENCES

 levinson.c, UMTS GSM AMR speech codec, R99 - Version 3.2.0, March 2, 2001

------------------------------------------------------------------------------
 PSEUDO-CODE

int Levinson (
    LevinsonState *st,
    Word16 Rh[],       // i : Rh[m+1] Vector of autocorrelations (msb)
    Word16 Rl[],       // i : Rl[m+1] Vector of autocorrelations (lsb)
    Word16 A[],        // o : A[m]    LPC coefficients  (m = 10)
    Word16 rc[]        // o : rc[4]   First 4 reflection coefficients
)
{
    Word16 i, j;
    Word16 hi, lo;
    Word16 Kh, Kl;                // reflexion coefficient; hi and lo
    Word16 alp_h, alp_l, alp_exp; // Prediction gain; hi lo and exponent
    Word16 Ah[M + 1], Al[M + 1];  // LPC coef. in double prec.
    Word16 Anh[M + 1], Anl[M + 1];// LPC coef.for next iteration in double
                                     prec.
    Word32 t0, t1, t2;            // temporary variable

    // K = A[1] = -R[1] / R[0]

    t1 = L_Comp (Rh[1], Rl[1]);
    t2 = L_abs (t1);                    // abs R[1]
    t0 = Div_32 (t2, Rh[0], Rl[0]);     // R[1]/R[0]
    if (t1 > 0)
       t0 = L_negate (t0);             // -R[1]/R[0]
    L_Extract (t0, &Kh, &Kl);           // K in DPF

    rc[0] = pv_round (t0);

    t0 = L_shr (t0, 4);                 // A[1] in
    L_Extract (t0, &Ah[1], &Al[1]);     // A[1] in DPF

    //  Alpha = R[0] * (1-K**2)

    t0 = Mpy_32 (Kh, Kl, Kh, Kl);       // K*K
    t0 = L_abs (t0);                    // Some case <0 !!
    t0 = L_sub ((Word32) 0x7fffffffL, t0); // 1 - K*K
    L_Extract (t0, &hi, &lo);           // DPF format
    t0 = Mpy_32 (Rh[0], Rl[0], hi, lo); // Alpha in

    // Normalize Alpha

    alp_exp = norm_l (t0);
    t0 = L_shl (t0, alp_exp);
    L_Extract (t0, &alp_h, &alp_l);     // DPF format

     *--------------------------------------*
     * ITERATIONS  I=2 to M                 *
     *--------------------------------------*

    for (i = 2; i <= M; i++)
    {
       // t0 = SUM ( R[j]*A[i-j] ,j=1,i-1 ) +  R[i]

       t0 = 0;
       for (j = 1; j < i; j++)
       {
          t0 = L_add (t0, Mpy_32 (Rh[j], Rl[j], Ah[i - j], Al[i - j]));
       }
       t0 = L_shl (t0, 4);

       t1 = L_Comp (Rh[i], Rl[i]);
       t0 = L_add (t0, t1);            // add R[i]

       // K = -t0 / Alpha

       t1 = L_abs (t0);
       t2 = Div_32 (t1, alp_h, alp_l); // abs(t0)/Alpha
       if (t0 > 0)
          t2 = L_negate (t2);         // K =-t0/Alpha
       t2 = L_shl (t2, alp_exp);       // denormalize; compare to Alpha
       L_Extract (t2, &Kh, &Kl);       // K in DPF

       if (sub (i, 5) < 0)
       {
          rc[i - 1] = pv_round (t2);
       }
       // Test for unstable filter. If unstable keep old A(z)

       if (sub (abs_s (Kh), 32750) > 0)
       {
          for (j = 0; j <= M; j++)
          {
             A[j] = st->old_A[j];
          }

          for (j = 0; j < 4; j++)
          {
             rc[j] = 0;
          }

          return 0;
       }
        *------------------------------------------*
        *  Compute new LPC coeff. -> An[i]         *
        *  An[j]= A[j] + K*A[i-j]     , j=1 to i-1 *
        *  An[i]= K                                *
        *------------------------------------------*

       for (j = 1; j < i; j++)
       {
          t0 = Mpy_32 (Kh, Kl, Ah[i - j], Al[i - j]);
          t0 = L_add(t0, L_Comp(Ah[j], Al[j]));
          L_Extract (t0, &Anh[j], &Anl[j]);
       }
       t2 = L_shr (t2, 4);
       L_Extract (t2, &Anh[i], &Anl[i]);

       //  Alpha = Alpha * (1-K**2)

       t0 = Mpy_32 (Kh, Kl, Kh, Kl);           // K*K
       t0 = L_abs (t0);                        // Some case <0 !!
       t0 = L_sub ((Word32) 0x7fffffffL, t0);  // 1 - K*K
       L_Extract (t0, &hi, &lo);               // DPF format
       t0 = Mpy_32 (alp_h, alp_l, hi, lo);

       // Normalize Alpha

       j = norm_l (t0);
       t0 = L_shl (t0, j);
       L_Extract (t0, &alp_h, &alp_l);         // DPF format
       alp_exp = add (alp_exp, j);             // Add normalization to
                                                  alp_exp

       // A[j] = An[j]

       for (j = 1; j <= i; j++)
       {
          Ah[j] = Anh[j];
          Al[j] = Anl[j];
       }
    }

    A[0] = 4096;
    for (i = 1; i <= M; i++)
    {
       t0 = L_Comp (Ah[i], Al[i]);
       st->old_A[i] = A[i] = pv_round (L_shl (t0, 1));
    }

    return 0;
}

------------------------------------------------------------------------------
 CAUTION [optional]
 [State any special notes, constraints or cautions for users of this function]

------------------------------------------------------------------------------
*/

Word16 Levinson(
    LevinsonState *st,
    Word16 Rh[],       /* i : Rh[m+1] Vector of autocorrelations (msb) */
    Word16 Rl[],       /* i : Rl[m+1] Vector of autocorrelations (lsb) */
    Word16 A[],        /* o : A[m]    LPC coefficients  (m = 10)       */
    Word16 rc[],       /* o : rc[4]   First 4 reflection coefficients  */
    Flag   *pOverflow
)
{
    register Word16 i;
    register Word16 j;
    Word16 hi;
    Word16 lo;
    Word16 Kh;                    /* reflexion coefficient; hi and lo   */
    Word16 Kl;
    Word16 alp_h;                 /* Prediction gain; hi lo and exponent*/
    Word16 alp_l;
    Word16 alp_exp;
    Word16 Ah[M + 1];             /* LPC coef. in double prec.          */
    Word16 Al[M + 1];
    Word16 Anh[M + 1];            /* LPC coef.for next iteration in     */
    Word16 Anl[M + 1];            /* double prec.                       */
    register Word32 t0;           /* temporary variable                 */
    register Word32 t1;           /* temporary variable                 */
    register Word32 t2;           /* temporary variable                 */

    Word16 *p_Rh;
    Word16 *p_Rl;
    Word16 *p_Ah;
    Word16 *p_Al;
    Word16 *p_Anh;
    Word16 *p_Anl;
    Word16 *p_A;

    /* K = A[1] = -R[1] / R[0] */
    t1 = ((Word32) * (Rh + 1)) << 16;
    t1 += *(Rl + 1) << 1;

    t2 = L_abs(t1);         /* abs R[1] - required by Div_32 */
    t0 = Div_32(t2, *Rh, *Rl, pOverflow);  /* R[1]/R[0]        */

    if (t1 > 0)
    {
        t0 = L_negate(t0);  /* -R[1]/R[0]       */
    }

    /* K in DPF         */
    Kh = (Word16)(t0 >> 16);
    Kl = (Word16)((t0 >> 1) - ((Word32)(Kh) << 15));

    *rc = pv_round(t0, pOverflow);

    t0 = t0 >> 4;

    /* A[1] in DPF      */
    *(Ah + 1) = (Word16)(t0 >> 16);

    *(Al + 1) = (Word16)((t0 >> 1) - ((Word32)(*(Ah + 1)) << 15));

    /*  Alpha = R[0] * (1-K**2) */
    t0 = Mpy_32(Kh, Kl, Kh, Kl, pOverflow);         /* K*K              */
    t0 = L_abs(t0);                                 /* Some case <0 !!  */
    t0 = 0x7fffffffL - t0;                          /* 1 - K*K          */

    /* DPF format       */
    hi = (Word16)(t0 >> 16);
    lo = (Word16)((t0 >> 1) - ((Word32)(hi) << 15));

    t0 = Mpy_32(*Rh, *Rl, hi, lo, pOverflow);      /* Alpha in         */

    /* Normalize Alpha */

    alp_exp = norm_l(t0);
    t0 = t0 << alp_exp;

    /* DPF format       */
    alp_h = (Word16)(t0 >> 16);
    alp_l = (Word16)((t0 >> 1) - ((Word32)(alp_h) << 15));

    /*--------------------------------------*
    * ITERATIONS  I=2 to M                 *
    *--------------------------------------*/

    for (i = 2; i <= M; i++)
    {
        /* t0 = SUM ( R[j]*A[i-j] ,j=1,i-1 ) +  R[i] */

        t0 = 0;
        p_Rh = &Rh[1];
        p_Rl = &Rl[1];
        p_Ah = &Ah[i-1];
        p_Al = &Al[i-1];
        for (j = 1; j < i; j++)
        {
            t0 += (((Word32) * (p_Rh)* *(p_Al--)) >> 15);
            t0 += (((Word32) * (p_Rl++)* *(p_Ah)) >> 15);
            t0 += ((Word32) * (p_Rh++)* *(p_Ah--));
        }

        t0 = t0 << 5;

        t1 = ((Word32) * (Rh + i) << 16) + ((Word32)(*(Rl + i)) << 1);
        t0 += t1;

        /* K = -t0 / Alpha */

        t1 = L_abs(t0);
        t2 = Div_32(t1, alp_h, alp_l, pOverflow);  /* abs(t0)/Alpha        */

        if (t0 > 0)
        {
            t2 = L_negate(t2);          /* K =-t0/Alpha     */
        }

        t2 = L_shl(t2, alp_exp, pOverflow);  /* denormalize; compare to Alpha */
        Kh = (Word16)(t2 >> 16);
        Kl = (Word16)((t2 >> 1) - ((Word32)(Kh) << 15));

        if (i < 5)
        {
            *(rc + i - 1) = (Word16)((t2 + 0x00008000L) >> 16);
        }
        /* Test for unstable filter. If unstable keep old A(z) */
        if ((abs_s(Kh)) > 32750)
        {
            oscl_memcpy(A, &(st->old_A[0]), sizeof(Word16)*(M + 1));
            oscl_memset(rc, 0, sizeof(Word16)*4);
            return(0);
        }
        /*------------------------------------------*
        *  Compute new LPC coeff. -> An[i]         *
        *  An[j]= A[j] + K*A[i-j]     , j=1 to i-1 *
        *  An[i]= K                                *
        *------------------------------------------*/
        p_Ah = &Ah[i-1];
        p_Al = &Al[i-1];
        p_Anh = &Anh[1];
        p_Anl = &Anl[1];
        for (j = 1; j < i; j++)
        {
            t0  = (((Word32)Kh* *(p_Al--)) >> 15);
            t0 += (((Word32)Kl* *(p_Ah)) >> 15);
            t0 += ((Word32)Kh* *(p_Ah--));

            t0 += (Ah[j] << 15) + Al[j];

            *(p_Anh) = (Word16)(t0 >> 15);
            *(p_Anl++) = (Word16)(t0 - ((Word32)(*(p_Anh++)) << 15));
        }

        *(p_Anh) = (Word16)(t2 >> 20);
        *(p_Anl) = (Word16)((t2 >> 5) - ((Word32)(*(Anh + i)) << 15));

        /*  Alpha = Alpha * (1-K**2) */

        t0 = Mpy_32(Kh, Kl, Kh, Kl, pOverflow);  /* K*K             */
        t0 = L_abs(t0);                          /* Some case <0 !! */
        t0 = 0x7fffffffL - t0;                   /* 1 - K*K          */

        hi = (Word16)(t0 >> 16);
        lo = (Word16)((t0 >> 1) - ((Word32)(hi) << 15));

        t0  = (((Word32)alp_h * lo) >> 15);
        t0 += (((Word32)alp_l * hi) >> 15);
        t0 += ((Word32)alp_h * hi);

        t0 <<= 1;
        /* Normalize Alpha */

        j = norm_l(t0);
        t0 <<= j;
        alp_h = (Word16)(t0 >> 16);
        alp_l = (Word16)((t0 >> 1) - ((Word32)(alp_h) << 15));
        alp_exp += j;             /* Add normalization to alp_exp */

        /* A[j] = An[j] */
        oscl_memcpy(&Ah[1], &Anh[1], sizeof(Word16)*i);
        oscl_memcpy(&Al[1], &Anl[1], sizeof(Word16)*i);
    }

    p_A = &A[0];
    *(p_A++) = 4096;
    p_Ah = &Ah[1];
    p_Al = &Al[1];

    for (i = 1; i <= M; i++)
    {
        t0 = ((Word32) * (p_Ah++) << 15) + *(p_Al++);
        st->old_A[i] = *(p_A++) = (Word16)((t0 + 0x00002000) >> 14);
    }

    return(0);
}
