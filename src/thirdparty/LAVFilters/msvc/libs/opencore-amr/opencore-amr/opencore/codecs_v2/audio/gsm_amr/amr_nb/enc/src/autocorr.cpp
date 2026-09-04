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



 Filename: autocorr.cpp

----------------------------------------------------------------------------*/

/*----------------------------------------------------------------------------
; INCLUDES
----------------------------------------------------------------------------*/
#include "autocorr.h"
#include "typedef.h"
#include "basic_op.h"
#include "oper_32b.h"
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

/*
------------------------------------------------------------------------------
 FUNCTION NAME: Autocorr
----------------------------------------------------------------------------
 INPUT AND OUTPUT DEFINITIONS

 Inputs:
    x = buffer of input signals of type Word16
    m = LPC order of type Word16
    wind = buffer of window signals of type Word16
    r_h = buffer containing the high word of the autocorrelation values
          of type Word16
    r_l = buffer containing the low word of the autocorrelation values
          of type Word16

    pOverflow = pointer to variable of type Flag *, which indicates if
                overflow occurs.

 Outputs:
    r_h buffer contains the high word of the new autocorrelation values
    r_l buffer contains the low word of the new autocorrelation values
    pOverflow -> 1 if overflow occurs.

 Returns:
    norm = normalized autocorrelation at lag zero of type Word16

 Global Variables Used:
    None

 Local Variables Needed:
    None

------------------------------------------------------------------------------
 FUNCTION DESCRIPTION

 This function windows the input signal with the provided window
 then calculates the autocorrelation values for lags of 0,1,...m,
 where m is the passed in LPC order.

------------------------------------------------------------------------------
 REQUIREMENTS

 None.

------------------------------------------------------------------------------
 REFERENCES

 autocorr.c, UMTS GSM AMR speech codec, R99 - Version 3.2.0, March 2, 2001

------------------------------------------------------------------------------
 PSEUDO-CODE

Word16 Autocorr (
    Word16 x[],            // (i)    : Input signal (L_WINDOW)
    Word16 m,              // (i)    : LPC order
    Word16 r_h[],          // (o)    : Autocorrelations  (msb)
    Word16 r_l[],          // (o)    : Autocorrelations  (lsb)
    const Word16 wind[]    // (i)    : window for LPC analysis (L_WINDOW)
)
{
    Word16 i, j, norm;
    Word16 y[L_WINDOW];
    Word32 sum;
    Word16 overfl, overfl_shft;

    // Windowing of signal

    for (i = 0; i < L_WINDOW; i++)
    {
        y[i] = mult_r (x[i], wind[i]);
    }

    // Compute r[0] and test for overflow

    overfl_shft = 0;

    do
    {
        overfl = 0;
        sum = 0L;

        for (i = 0; i < L_WINDOW; i++)
        {
            sum = L_mac (sum, y[i], y[i]);
        }

        // If overflow divide y[] by 4

        if (L_sub (sum, MAX_32) == 0L)
        {
            overfl_shft = add (overfl_shft, 4);
            overfl = 1; // Set the overflow flag

            for (i = 0; i < L_WINDOW; i++)
            {
                y[i] = shr (y[i], 2);
            }
        }
    }
    while (overfl != 0);

    sum = L_add (sum, 1L);             // Avoid the case of all zeros

    // Normalization of r[0]

    norm = norm_l (sum);
    sum = L_shl (sum, norm);
    L_Extract (sum, &r_h[0], &r_l[0]); // Put in DPF format (see oper_32b)

    // r[1] to r[m]

    for (i = 1; i <= m; i++)
    {
        sum = 0;

        for (j = 0; j < L_WINDOW - i; j++)
        {
            sum = L_mac (sum, y[j], y[j + i]);
        }

        sum = L_shl (sum, norm);
        L_Extract (sum, &r_h[i], &r_l[i]);
    }

    norm = sub (norm, overfl_shft);

    return norm;
}


------------------------------------------------------------------------------
 CAUTION [optional]
 [State any special notes, constraints or cautions for users of this function]

------------------------------------------------------------------------------
*/

Word16 Autocorr(
    Word16 x[],            /* (i)    : Input signal (L_WINDOW)            */
    Word16 m,              /* (i)    : LPC order                          */
    Word16 r_h[],          /* (o)    : Autocorrelations  (msb)            */
    Word16 r_l[],          /* (o)    : Autocorrelations  (lsb)            */
    const Word16 wind[],   /* (i)    : window for LPC analysis (L_WINDOW) */
    Flag  *pOverflow       /* (o)    : indicates overflow                 */
)
{
    register Word16 i;
    register Word16 j;
    register Word16 norm;

    Word16 y[L_WINDOW];
    Word32 sum;
    Word16 overfl_shft;


    /* Added for optimization  */


    Word16 temp;
    Word16 *p_x;
    Word16 *p_y;
    Word16 *p_y_1;
    Word16 *p_y_ref;
    Word16 *p_rh;
    Word16 *p_rl;
    const Word16 *p_wind;
    p_y = y;
    p_x = x;
    p_wind = wind;
    /*
     *  Windowing of the signal
     */

    OSCL_UNUSED_ARG(pOverflow);

    sum = 0L;
    j = 0;

    for (i = L_WINDOW; i != 0; i--)
    {
        temp = (Word16)((amrnb_fxp_mac_16_by_16bb((Word32) * (p_x++), (Word32) * (p_wind++), 0x04000)) >> 15);
        *(p_y++) = temp;

        sum += ((Word32)temp * temp) << 1;
        if (sum < 0)
        {
            /*
             * if oveflow exist, then stop accumulation
             */
            j = 1;
            break;
        }

    }
    /*
     * if oveflow existed, complete  windowing operation
     * without computing energy
     */

    if (j)
    {
        p_y = &y[L_WINDOW-i];
        p_x = &x[L_WINDOW-i];
        p_wind = &wind[L_WINDOW-i];

        for (; i != 0; i--)
        {
            temp = (Word16)((amrnb_fxp_mac_16_by_16bb((Word32) * (p_x++), (Word32) * (p_wind++), 0x04000)) >> 15);
            *(p_y++) = temp;
        }
    }


    /*
     *  Compute r[0] and test for overflow
     */

    overfl_shft = 0;

    /*
     * scale down by 1/4 only when needed
     */
    while (j == 1)
    {
        /* If overflow divide y[] by 4          */
        /* FYI: For better resolution, we could */
        /*      divide y[] by 2                 */
        overfl_shft += 4;
        p_y   = &y[0];
        sum = 0L;

        for (i = (L_WINDOW >> 1); i != 0 ; i--)
        {
            temp = *p_y >> 2;
            *(p_y++) = temp;
            sum += ((Word32)temp * temp) << 1;
            temp = *p_y >> 2;
            *(p_y++) = temp;
            sum += ((Word32)temp * temp) << 1;
        }
        if (sum > 0)
        {
            j = 0;
        }

    }

    sum += 1L;              /* Avoid the case of all zeros */

    /* Normalization of r[0] */

    norm = norm_l(sum);

    sum <<= norm;

    /* Put in DPF format (see oper_32b) */
    r_h[0] = (Word16)(sum >> 16);
    r_l[0] = (Word16)((sum >> 1) - ((Word32)(r_h[0]) << 15));

    /* r[1] to r[m] */

    p_y_ref = &y[L_WINDOW - 1 ];
    p_rh = &r_h[m];
    p_rl = &r_l[m];

    for (i = m; i > 0; i--)
    {
        sum  = 0;

        p_y   = &y[L_WINDOW - i - 1];
        p_y_1 = p_y_ref;

        for (j = (L_WINDOW - i - 1) >> 1; j != 0; j--)
        {
            sum = amrnb_fxp_mac_16_by_16bb((Word32) * (p_y--), (Word32) * (p_y_1--), sum);
            sum = amrnb_fxp_mac_16_by_16bb((Word32) * (p_y--), (Word32) * (p_y_1--), sum);
        }

        sum = amrnb_fxp_mac_16_by_16bb((Word32) * (p_y--), (Word32) * (p_y_1--), sum);

        if (((L_WINDOW - i - 1) & 1))
        {
            sum = amrnb_fxp_mac_16_by_16bb((Word32) * (p_y--), (Word32) * (p_y_1--), sum);
        }

        sum  <<= (norm + 1);

        *(p_rh)   = (Word16)(sum >> 16);
        *(p_rl--) = (Word16)((sum >> 1) - ((Word32) * (p_rh--) << 15));

    }

    norm -= overfl_shft;

    return (norm);

} /* Autocorr */
