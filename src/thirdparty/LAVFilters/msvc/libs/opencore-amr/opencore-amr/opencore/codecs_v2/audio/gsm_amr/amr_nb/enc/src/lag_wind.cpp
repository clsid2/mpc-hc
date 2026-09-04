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



 Filename: lag_wind.cpp

------------------------------------------------------------------------------
*/

/*----------------------------------------------------------------------------
; INCLUDES
----------------------------------------------------------------------------*/
#include "lag_wind.h"
#include "lag_wind_tab.h"
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
; LOCAL STORE/BUFFER/POINTER DEFINITIONS
; Variable declaration - defined here and used outside this module
----------------------------------------------------------------------------*/


/*
------------------------------------------------------------------------------
 FUNCTION NAME: lag_wind
------------------------------------------------------------------------------
 INPUT AND OUTPUT DEFINITIONS

 Inputs:
    m = LPC order of type Word16
    r_h[] = pointer to autocorrelations (msb) of type Word16
    r_l[] = pointer to autocorrelations (lsb) of type Word16
    pOverflow = pointer to overflow flag

 Outputs:
    None

 Returns:
    None

 Global Variables Used:
    None.

 Local Variables Needed:
    None.

------------------------------------------------------------------------------
 FUNCTION DESCRIPTION

      File             : lag_wind.c
      Purpose          : Lag windowing of autocorrelations.

    FUNCTION:  Lag_window()

    PURPOSE:  Lag windowing of autocorrelations.

    DESCRIPTION:
          r[i] = r[i]*lag_wind[i],   i=1,...,10

      r[i] and lag_wind[i] are in special double precision format.
      See "oper_32b.c" for the format.

------------------------------------------------------------------------------
 REQUIREMENTS

 None.

------------------------------------------------------------------------------
 REFERENCES

 lag_wind.c, UMTS GSM AMR speech codec, R99 - Version 3.2.0, March 2, 2001

------------------------------------------------------------------------------
 PSEUDO-CODE

    Word16 i;
    Word32 x;

    for (i = 1; i <= m; i++)
    {
        x = Mpy_32 (r_h[i], r_l[i], lag_h[i - 1], lag_l[i - 1], pOverflow);
        L_Extract (x, &r_h[i], &r_l[i], pOverflow);
    }
    return;

------------------------------------------------------------------------------
 CAUTION [optional]
 [State any special notes, constraints or cautions for users of this function]

------------------------------------------------------------------------------
*/
void Lag_window(
    Word16 m,           /* (i)     : LPC order                        */
    Word16 r_h[],       /* (i/o)   : Autocorrelations  (msb)          */
    Word16 r_l[],       /* (i/o)   : Autocorrelations  (lsb)          */
    Flag   *pOverflow
)
{
    Word16 i;
    Word32 x;
    const Word16 *p_lag_h = &lag_h[0];
    const Word16 *p_lag_l = &lag_l[0];
    Word16 *p_r_h = &r_h[1];
    Word16 *p_r_l = &r_l[1];

    for (i = m; i != 0 ; i--)
    {
        x = Mpy_32(*(p_r_h), *(p_r_l), *(p_lag_h++), *(p_lag_l++), pOverflow);
        *(p_r_h) = (Word16)(x >> 16);
        *(p_r_l++) = (x >> 1) - (*(p_r_h++) << 15);
    }

    return;
}

