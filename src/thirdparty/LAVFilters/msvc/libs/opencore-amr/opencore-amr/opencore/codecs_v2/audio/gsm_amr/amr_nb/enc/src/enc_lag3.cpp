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



 Filename: enc_lag3.cpp

------------------------------------------------------------------------------
*/

/*----------------------------------------------------------------------------
; INCLUDES
----------------------------------------------------------------------------*/
#include "enc_lag3.h"
#include "typedef.h"
#include "basic_op.h"
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
 FUNCTION NAME: enc_lag3
------------------------------------------------------------------------------
 INPUT AND OUTPUT DEFINITIONS

 Inputs:
  T0 = Pitch delay of type Word16
  T0_frac = Fractional pitch delay of type Word16
  T0_prev = Integer pitch delay of last subframe of type Word16
  T0_min  = minimum of search range of type Word16
  T0_max  = maximum of search range of type Word16
  delta_flag = Flag for 1st (or 3rd) subframe of type Word16
  flag4   = Flag for encoding with 4 bits of type Word16
  pOverflow = pointer indicating overflow of type Flag

 Outputs:
  pOverflow = 1 if there is an overflow else it is zero.

 Returns:
  None

 Global Variables Used:
  None

 Local Variables Needed:
  None

------------------------------------------------------------------------------
 FUNCTION DESCRIPTION

 This function implements the encoding of fractional pitch lag with
 1/3 resolution.

 *   FUNCTION:  Enc_lag3
 *
 *   PURPOSE:  Encoding of fractional pitch lag with 1/3 resolution.
 *
 *   DESCRIPTION:
 *                    First and third subframes:
 *                    --------------------------
 *   The pitch range is divided as follows:
 *           19 1/3  to   84 2/3   resolution 1/3
 *           85      to   143      resolution 1
 *
 *   The period is encoded with 8 bits.
 *   For the range with fractions:
 *     index = (T-19)*3 + frac - 1;
 *                         where T=[19..85] and frac=[-1,0,1]
 *   and for the integer only range
 *     index = (T - 85) + 197;        where T=[86..143]
 *
 *                    Second and fourth subframes:
 *                    ----------------------------
 *   For the 2nd and 4th subframes a resolution of 1/3 is always used,
 *   and the search range is relative to the lag in previous subframe.
 *   If t0 is the lag in the previous subframe then
 *   t_min=t0-5   and  t_max=t0+4   and  the range is given by
 *        t_min - 2/3   to  t_max + 2/3
 *
 *   The period in the 2nd (and 4th) subframe is encoded with 5 bits:
 *     index = (T-(t_min-1))*3 + frac - 1;
 *                 where T=[t_min-1..t_max+1]

------------------------------------------------------------------------------
 REQUIREMENTS

 None

------------------------------------------------------------------------------
 REFERENCES

 enc_lag3.c, UMTS GSM AMR speech codec, R99 - Version 3.2.0, March 2, 2001

------------------------------------------------------------------------------
 PSEUDO-CODE

   Word16 index, i, tmp_ind, uplag;
   Word16 tmp_lag;

   if (delta_flag == 0)
   {  // if 1st or 3rd subframe

      // encode pitch delay (with fraction)

      if (sub (T0, 85) <= 0)
      {
         // index = T0*3 - 58 + T0_frac
         i = add (add (T0, T0), T0);
         index = add (sub (i, 58), T0_frac);
      }
      else
      {
         index = add (T0, 112);
      }
   }
   else
   {   // if second or fourth subframe
      if (flag4 == 0) {

         // 'normal' encoding: either with 5 or 6 bit resolution

         // index = 3*(T0 - T0_min) + 2 + T0_frac
         i = sub (T0, T0_min);
         i = add (add (i, i), i);
         index = add (add (i, 2), T0_frac);
      }
      else {

         // encoding with 4 bit resolution

         tmp_lag = T0_prev;

         if ( sub( sub(tmp_lag, T0_min), 5) > 0)
            tmp_lag = add (T0_min, 5);
         if ( sub( sub(T0_max, tmp_lag), 4) > 0)
            tmp_lag = sub (T0_max, 4);

         uplag = add (add (add (T0, T0), T0), T0_frac);

         i = sub (tmp_lag, 2);
         tmp_ind = add (add (i, i), i);

         if (sub (tmp_ind, uplag) >= 0) {
            index = add (sub (T0, tmp_lag), 5);
         }
         else {

            i = add (tmp_lag, 1);
            i = add (add (i, i), i);

            if (sub (i, uplag) > 0) {

                index = add ( sub (uplag, tmp_ind), 3);
            }
            else {

               index = add (sub (T0, tmp_lag), 11);
            }
         }

      } // end if (encoding with 4 bit resolution)
   }   // end if (second of fourth subframe)

   return index;
}

------------------------------------------------------------------------------
 CAUTION [optional]
 [State any special notes, constraints or cautions for users of this function]

------------------------------------------------------------------------------
*/


Word16 Enc_lag3(         /* o  : Return index of encoding             */
    Word16 T0,           /* i  : Pitch delay                          */
    Word16 T0_frac,      /* i  : Fractional pitch delay               */
    Word16 T0_prev,      /* i  : Integer pitch delay of last subframe */
    Word16 T0_min,       /* i  : minimum of search range              */
    Word16 T0_max,       /* i  : maximum of search range              */
    Word16 delta_flag,   /* i  : Flag for 1st (or 3rd) subframe       */
    Word16 flag4,        /* i  : Flag for encoding with 4 bits        */
    Flag   *pOverflow
)
{
    Word16 index, i, tmp_ind, uplag;
    Word16 tmp_lag;
    Word16 temp1;
    Word16 temp2;



    if (delta_flag == 0)
    {  /* if 1st or 3rd subframe */

        /* encode pitch delay (with fraction) */
        temp1 = T0 - 85;
        if (temp1 <= 0)
        {
            /* index = T0*3 - 58 + T0_frac   */
            index = (T0 << 1) + T0 -  58 + T0_frac;
        }
        else
        {
            index = T0 + 112;
        }
    }
    else
    {   /* if second or fourth subframe */
        if (flag4 == 0)
        {

            /* 'normal' encoding: either with 5 or 6 bit resolution */

            /* index = 3*(T0 - T0_min) + 2 + T0_frac */
            i = T0 - T0_min;
            index = i + (i << 1) + 2 + T0_frac;
        }
        else
        {

            /* encoding with 4 bit resolution */

            tmp_lag = T0_prev;
            temp1 = tmp_lag - T0_min;
            temp2 = temp1 - 5;
            if (temp2 > 0)
            {
                tmp_lag = T0_min + 5;
            }
            temp1 = T0_max - tmp_lag;
            temp2 = temp1 - 4;
            if (temp2 > 0)
            {
                tmp_lag = T0_max - 4;
            }
            uplag  = T0 + (T0 << 1);
            uplag += T0_frac;
            i = tmp_lag - 2;

            tmp_ind  = i + (i << 1);
            temp1    = tmp_ind - uplag;

            if (temp1 >= 0)
            {
                index = T0 - tmp_lag + 5;
            }
            else
            {
                i = tmp_lag + 1;

                i += i << 1;

                if (i > uplag)
                {
                    index = uplag - tmp_ind + 3;
                }
                else
                {
                    index = T0 - tmp_lag + 11;
                }
            }

        } /* end if (encoding with 4 bit resolution) */
    }   /* end if (second of fourth subframe) */

    return index;
}



