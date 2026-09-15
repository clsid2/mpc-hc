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



 Filename: set_sign.cpp
 Funtions: set_sign
           set_sign12k2

------------------------------------------------------------------------------
 MODULE DESCRIPTION

 This module contains the functions set_sign and set_sign12k2.
 These functions are used to build a sign vector according
 to the values in the input arrays.  These functions also
 find the position in the input codes of the maximum correlation
 and the starting position for each pulse.

------------------------------------------------------------------------------
*/


/*----------------------------------------------------------------------------
; INCLUDES
----------------------------------------------------------------------------*/
#include "set_sign.h"
#include "basic_op.h"
#include "inv_sqrt.h"
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
 FUNCTION NAME: set_sign
------------------------------------------------------------------------------
 INPUT AND OUTPUT DEFINITIONS

 Inputs:
    dn = buffer of correlation values (Word16)
    sign = buffer containing sign of dn elements (Word16)
    dn2 = buffer containing the maximum of correlation in each track.(Word16)
    n = number of maximum correlations in dn2 (Word16)

 Returns:
    None

 Outputs:
    dn buffer is modified to contain the absolute value of its input
    sign buffer is modified to contain the sign information for the
      values in dn buffer
    dn2 buffer is modified to denote the location of the maximum
      correlation for each track.

 Global Variables Used:
    None

 Local Variables Needed:
    None

------------------------------------------------------------------------------
 FUNCTION DESCRIPTION


 This function builds sign vector according to dn buffer It also finds
 the position of maximum of correlation in each track and the starting
 position for each pulse.

------------------------------------------------------------------------------
 REQUIREMENTS

 None.

------------------------------------------------------------------------------
 REFERENCES

 set_sign.c, UMTS GSM AMR speech codec, R99 - Version 3.2.0, March 2, 2001

------------------------------------------------------------------------------
 PSEUDO-CODE

void set_sign(Word16 dn[],    i/o : correlation between target and h[]
              Word16 sign[],  o   : sign of dn[]
              Word16 dn2[],   o   : maximum of correlation in each track.
              Word16 n        i   : # of maximum correlations in dn2[]
)
{
   Word16 i, j, k;
   Word16 val, min;
   Word16 pos = 0;    //initialization only needed to keep gcc silent

   // set sign according to dn[]

   for (i = 0; i < L_CODE; i++) {
      val = dn[i];

      if (val >= 0) {
         sign[i] = 32767;
      } else {
         sign[i] = -32767;
         val = negate(val);
      }
      dn[i] = val;     // modify dn[] according to the fixed sign
      dn2[i] = val;
   }

   // keep 8-n maximum positions/8 of each track and store it in dn2[]

   for (i = 0; i < NB_TRACK; i++)
   {
      for (k = 0; k < (8-n); k++)
      {
         min = 0x7fff;
         for (j = i; j < L_CODE; j += STEP)
         {
            if (dn2[j] >= 0)
            {
               val = sub(dn2[j], min);

               if (val < 0)
               {
                  min = dn2[j];
                  pos = j;
               }
            }
         }
         dn2[pos] = -1;
      }
   }

   return;
}

------------------------------------------------------------------------------
 CAUTION [optional]
 [State any special notes, constraints or cautions for users of this function]

------------------------------------------------------------------------------
*/

void set_sign(Word16 dn[],   /* i/o : correlation between target and h[]    */
              Word16 sign[], /* o   : sign of dn[]                          */
              Word16 dn2[],  /* o   : maximum of correlation in each track. */
              Word16 n       /* i   : # of maximum correlations in dn2[]    */
             )
{
    register Word16 i, j, k;
    Word16 val, min;
    Word16 pos = 0; /* initialization only needed to keep gcc silent */

    /* set sign according to dn[] */
    for (i = L_CODE - 1; i >= 0; i--)
    {
        val = dn[i];

        if (val >= 0)
        {
            sign[i] = 32767;
        }
        else
        {
            sign[i] = -32767;
            val = negate(val);
            dn[i] = val;     /* modify dn[] according to the fixed sign */
        }

        dn2[i] = val;
    }

    /* keep 8-n maximum positions/8 of each track and store it in dn2[] */

    for (i = 0; i < NB_TRACK; i++)
    {
        for (k = 0; k < (8 - n); k++)
        {
            min = 0x7fff;
            for (j = i; j < L_CODE; j += STEP)
            {
                if (dn2[j] >= 0)
                {
                    if (dn2[j] < min)
                    {
                        min = dn2[j];
                        pos = j;
                    }
                }
            }
            dn2[pos] = -1;
        }
    }

    return;
}

/****************************************************************************/


/*
------------------------------------------------------------------------------
 FUNCTION NAME: set_sign12k2()
------------------------------------------------------------------------------
 INPUT AND OUTPUT DEFINITIONS

 Inputs:
    dn = buffer of correlation values (Word16)
    cn = buffer of residual after long term prediction (Word16)
    sign = sign of correlation buffer elements (Word16)
    pos_max = buffer containing position of maximum correlation (Word16)
    nb_track = number of tracks (Word16)
    ipos = buffer containing the starting position for each pulse (Word16)
    step = step size in the tracks (Word16)
    pOverflow = pointer to Overflow flag (Flag)

 Outputs:
    sign buffer contains the sign of correlation values
    dn buffer contains the sign-adjusted correlation values
    pos_max buffer contains the maximum correlation position
    ipos buffer contains the starting position of each pulse
    pOverflow -> 1 if the math operations called by this function result in
    saturation


 Returns:
    None

 Global Variables Used:
    None

 Local Variables Needed:
    None

------------------------------------------------------------------------------
 FUNCTION DESCRIPTION

 This function builds the sign vector according to dn and cn, and modifies
 dn to include the sign information (dn[i]=sign[i]*dn[i]). It also finds
 the position of maximum of correlation in each track and the starting
 position for each pulse.

------------------------------------------------------------------------------
 REQUIREMENTS

 None

------------------------------------------------------------------------------
 REFERENCES

 set_sign.c, UMTS GSM AMR speech codec, R99 - Version 3.2.0, March 2, 2001

------------------------------------------------------------------------------
 PSEUDO-CODE

void set_sign12k2 (
    Word16 dn[],       //i/o : correlation between target and h[]
    Word16 cn[],       //i   : residual after long term prediction
    Word16 sign[],     //o   : sign of d[n]
    Word16 pos_max[],  //o   : position of maximum correlation
    Word16 nb_track,   //i   : number of tracks tracks
    Word16 ipos[],     //o   : starting position for each pulse
    Word16 step        //i   : the step size in the tracks
)
{
    Word16 i, j;
    Word16 val, cor, k_cn, k_dn, max, max_of_all;
    Word16 pos = 0;      // initialization only needed to keep gcc silent
    Word16 en[L_CODE];                  // correlation vector
    Word32 s;

    // The reference ETSI code uses a global flag for Overflow. However in the
    // actual implementation a pointer to the overflow flag is passed in. This
    // pointer is passed into the basic math functions called by this routine.

    // calculate energy for normalization of cn[] and dn[]

    s = 256;
    for (i = 0; i < L_CODE; i++)
    {
        s = L_mac (s, cn[i], cn[i]);
    }
    s = Inv_sqrt (s);
    k_cn = extract_h (L_shl (s, 5));

    s = 256;
    for (i = 0; i < L_CODE; i++)
    {
        s = L_mac (s, dn[i], dn[i]);
    }
    s = Inv_sqrt (s);
    k_dn = extract_h (L_shl (s, 5));

    for (i = 0; i < L_CODE; i++)
    {
        val = dn[i];
        cor = pv_round (L_shl (L_mac (L_mult (k_cn, cn[i]), k_dn, val), 10));

        if (cor >= 0)
        {
            sign[i] = 32767;                      // sign = +1
        }
        else
        {
            sign[i] = -32767;                     // sign = -1
            cor = negate (cor);
            val = negate (val);
        }
        // modify dn[] according to the fixed sign
        dn[i] = val;
        en[i] = cor;
    }

    max_of_all = -1;
    for (i = 0; i < nb_track; i++)
    {
        max = -1;

        for (j = i; j < L_CODE; j += step)
        {
            cor = en[j];
            val = sub (cor, max);

            if (val > 0)
            {
                max = cor;
                pos = j;
            }
        }
        // store maximum correlation position
        pos_max[i] = pos;
        val = sub (max, max_of_all);

        if (val > 0)
        {
            max_of_all = max;
            // starting position for i0
            ipos[0] = i;
        }
    }

    //
    //     Set starting position of each pulse.
    //

    pos = ipos[0];
    ipos[nb_track] = pos;

    for (i = 1; i < nb_track; i++)
    {
        pos = add (pos, 1);

        if (sub (pos, nb_track) >= 0)
        {
           pos = 0;
        }
        ipos[i] = pos;
        ipos[add(i, nb_track)] = pos;
    }
}

------------------------------------------------------------------------------
 CAUTION [optional]
 [State any special notes, constraints or cautions for users of this function]

------------------------------------------------------------------------------
*/

void set_sign12k2(
    Word16 dn[],        /* i/o : correlation between target and h[]         */
    Word16 cn[],        /* i   : residual after long term prediction        */
    Word16 sign[],      /* o   : sign of d[n]                               */
    Word16 pos_max[],   /* o   : position of maximum correlation            */
    Word16 nb_track,    /* i   : number of tracks tracks                    */
    Word16 ipos[],      /* o   : starting position for each pulse           */
    Word16 step,        /* i   : the step size in the tracks                */
    Flag   *pOverflow   /* i/o: overflow flag                               */
)
{
    Word16 i, j;
    Word16 val;
    Word16 cor;
    Word16 k_cn;
    Word16 k_dn;
    Word16 max;
    Word16 max_of_all;
    Word16 pos = 0; /* initialization only needed to keep gcc silent */
    Word16 en[L_CODE];                  /* correlation vector */
    Word32 s;
    Word32 t;
    Word32 L_temp;
    Word16 *p_cn;
    Word16 *p_dn;
    Word16 *p_sign;
    Word16 *p_en;

    /* calculate energy for normalization of cn[] and dn[] */

    s = 256;
    t = 256;
    p_cn = cn;
    p_dn = dn;      /* crosscorrelation values do not have strong peaks, so
                       scaling applied in cor_h_x (sf=2) guaranteed that the
                       mac of the energy for this vector will not overflow */

    for (i = L_CODE; i != 0; i--)
    {
        val = *(p_cn++);
        s = L_mac(s, val, val, pOverflow);
        val = *(p_dn++);
        t += ((Word32) val * val) << 1;
    }
    s = Inv_sqrt(s, pOverflow);
    k_cn = (Word16)((L_shl(s, 5, pOverflow)) >> 16);

    t = Inv_sqrt(t, pOverflow);
    k_dn = (Word16)(t >> 11);

    p_cn   = &cn[L_CODE-1];
    p_sign = &sign[L_CODE-1];
    p_en   = &en[L_CODE-1];

    for (i = L_CODE - 1; i >= 0; i--)
    {
        L_temp = ((Word32)k_cn * *(p_cn--)) << 1;
        val = dn[i];
        s = L_mac(L_temp, k_dn, val, pOverflow);
        L_temp = L_shl(s, 10, pOverflow);
        cor = pv_round(L_temp, pOverflow);

        if (cor >= 0)
        {
            *(p_sign--) = 32767;                      /* sign = +1 */
        }
        else
        {
            *(p_sign--) = -32767;                     /* sign = -1 */
            cor = negate(cor);

            /* modify dn[] according to the fixed sign */
            dn[i] = negate(val);
        }

        *(p_en--) = cor;
    }

    max_of_all = -1;
    for (i = 0; i < nb_track; i++)
    {
        max = -1;

        for (j = i; j < L_CODE; j += step)
        {
            cor = en[j];
            if (cor > max)
            {
                max = cor;
                pos = j;
            }
        }
        /* store maximum correlation position */
        pos_max[i] = pos;
        if (max > max_of_all)
        {
            max_of_all = max;
            /* starting position for i0 */
            ipos[0] = i;
        }
    }

    /*----------------------------------------------------------------*
     *     Set starting position of each pulse.                       *
     *----------------------------------------------------------------*/

    pos = ipos[0];
    ipos[nb_track] = pos;

    for (i = 1; i < nb_track; i++)
    {
        pos++;

        if (pos >= nb_track)
        {
            pos = 0;
        }
        ipos[ i] = pos;
        ipos[ i + nb_track] = pos;
    }

    return;
}

