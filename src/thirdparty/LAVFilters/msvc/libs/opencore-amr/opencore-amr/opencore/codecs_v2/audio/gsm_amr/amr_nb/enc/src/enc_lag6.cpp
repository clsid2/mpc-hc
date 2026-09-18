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



 Filename: enc_lag6.cpp

------------------------------------------------------------------------------
 MODULE DESCRIPTION


------------------------------------------------------------------------------
*/

/*----------------------------------------------------------------------------
; INCLUDES
----------------------------------------------------------------------------*/
#include "enc_lag6.h"
#include "typedef.h"
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
 FUNCTION NAME: Enc_lag6
------------------------------------------------------------------------------
 INPUT AND OUTPUT DEFINITIONS

 Inputs:
    T0 -- Word16 -- Pitch delay
    T0_frac -- Word16 -- Fractional pitch delay
    T0_min -- Word16 -- minimum of search range
    delta_flag -- Word16 -- Flag for 1st (or 3rd) subframe

 Outputs:
    pOverflow -- Pointer to Flag -- overflow indicator

 Returns:
    Word16 -- Return index of encoding

 Global Variables Used:
    None

 Local Variables Needed:
    None

------------------------------------------------------------------------------
 FUNCTION DESCRIPTION

 PURPOSE:  Encoding of fractional pitch lag with 1/6 resolution.

 DESCRIPTION:
                  First and third subframes:
                  --------------------------
 The pitch range is divided as follows:
         17 3/6  to   94 3/6   resolution 1/6
         95      to   143      resolution 1

 The period is encoded with 9 bits.
 For the range with fractions:
   index = (T-17)*6 + frac - 3;
                       where T=[17..94] and frac=[-2,-1,0,1,2,3]
 and for the integer only range
   index = (T - 95) + 463;        where T=[95..143]

                  Second and fourth subframes:
                  ----------------------------
 For the 2nd and 4th subframes a resolution of 1/6 is always used,
 and the search range is relative to the lag in previous subframe.
 If t0 is the lag in the previous subframe then
 t_min=t0-5   and  t_max=t0+4   and  the range is given by
     (t_min-1) 3/6   to  (t_max) 3/6

 The period in the 2nd (and 4th) subframe is encoded with 6 bits:
   index = (T-(t_min-1))*6 + frac - 3;
               where T=[t_min-1..t_max] and frac=[-2,-1,0,1,2,3]

 Note that only 61 values are used. If the decoder receives 61, 62,
 or 63 as the relative pitch index, it means that a transmission
 error occurred and the pitch from previous subframe should be used.

------------------------------------------------------------------------------
 REQUIREMENTS

 None

------------------------------------------------------------------------------
 REFERENCES

 enc_lag6.c, UMTS GSM AMR speech codec, R99 - Version 3.2.0, March 2, 2001

------------------------------------------------------------------------------
 PSEUDO-CODE


------------------------------------------------------------------------------
 CAUTION [optional]
 [State any special notes, constraints or cautions for users of this function]

------------------------------------------------------------------------------
*/

Word16 Enc_lag6(         /* o : Return index of encoding             */
    Word16 T0,           /* i : Pitch delay                          */
    Word16 T0_frac,      /* i : Fractional pitch delay               */
    Word16 T0_min,       /* i : minimum of search range              */
    Word16 delta_flag,   /* i : Flag for 1st (or 3rd) subframe       */
    Flag   *pOverflow    /* o : overflow indicator                   */
)
{
    Word16 index;
    Word16 i;
    Word16 temp;

    if (delta_flag == 0)          /* if 1st or 3rd subframe */
    {
        /* encode pitch delay (with fraction) */
        if (T0 <= 94)
        {
            /* index = T0*6 - 105 + T0_frac */
            i = (T0 << 3) - (T0 << 1) - 105;

            index = i + T0_frac;
        }
        else
        {
            index = T0 + 368;
        }

    }
    else
        /* if second or fourth subframe */
    {
        /* index = 6*(T0-T0_min) + 3 + T0_frac  */
        temp = (T0 - T0_min);

        i = (temp << 3) - (temp << 1);
        i += 3;

        index = i + T0_frac;
    }

    return index;
}
