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



 Filename: convolve.cpp

------------------------------------------------------------------------------
*/

/*----------------------------------------------------------------------------
; INCLUDES
----------------------------------------------------------------------------*/
#include "typedef.h"
#include "convolve.h"
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
 FUNCTION NAME: Convolve
------------------------------------------------------------------------------
 INPUT AND OUTPUT DEFINITIONS

 Inputs:
    x = pointer to input vector of L elements of type Word16
    h = pointer to the filter's impulse response vector of L elements
        of type Word16
    y = pointer to the output vector of L elements of type Word16 used for
        storing the convolution of x and h;
    L = Length of the convolution; type definition is Word16

 Outputs:
    y buffer contains the new convolution output

 Returns:
    None

 Global Variables Used:
    None

 Local Variables Needed:
    None

------------------------------------------------------------------------------
 FUNCTION DESCRIPTION

 Perform the convolution between two vectors x[] and h[] and write the result
 in the vector y[]. All vectors are of length L and only the first L samples
 of the convolution are computed.

 The convolution is given by:

    y[n] = sum_{i=0}^{n} x[i] h[n-i],        n=0,...,L-1

------------------------------------------------------------------------------
 REQUIREMENTS

 None

------------------------------------------------------------------------------
 REFERENCES

 convolve.c, UMTS GSM AMR speech codec, R99 - Version 3.2.0, March 2, 2001

------------------------------------------------------------------------------
 PSEUDO-CODE

void Convolve (
    Word16 x[],        // (i)     : input vector
    Word16 h[],        // (i)     : impulse response
    Word16 y[],        // (o)     : output vector
    Word16 L           // (i)     : vector size
)
{
    Word16 i, n;
    Word32 s;

    for (n = 0; n < L; n++)
    {
        s = 0;                  move32 ();
        for (i = 0; i <= n; i++)
        {
            s = L_mac (s, x[i], h[n - i]);
        }
        s = L_shl (s, 3);
        y[n] = extract_h (s);   move16 ();
    }

    return;
}

------------------------------------------------------------------------------
 CAUTION [optional]
 [State any special notes, constraints or cautions for users of this function]

------------------------------------------------------------------------------
*/

void Convolve(
    Word16 x[],        /* (i)     : input vector                           */
    Word16 h[],        /* (i)     : impulse response                       */
    Word16 y[],        /* (o)     : output vector                          */
    Word16 L           /* (i)     : vector size                            */
)
{
    register Word16 i, n;
    Word32 s1, s2;


    for (n = 1; n < L; n = n + 2)
    {

        h = h + n;

        s2 = ((Word32) * (x)) * *(h--);
        s1 = ((Word32) * (x++)) * *(h);

        for (i = (n - 1) >> 1; i != 0; i--)
        {
            s2 = amrnb_fxp_mac_16_by_16bb((Word32) * (x), (Word32) * (h--), s2);
            s1 = amrnb_fxp_mac_16_by_16bb((Word32) * (x++), (Word32) * (h), s1);
            s2 = amrnb_fxp_mac_16_by_16bb((Word32) * (x), (Word32) * (h--), s2);
            s1 = amrnb_fxp_mac_16_by_16bb((Word32) * (x++), (Word32) * (h), s1);
        }

        s2 = amrnb_fxp_mac_16_by_16bb((Word32) * (x), (Word32) * (h), s2);

        *(y++) = (Word16)(s1 >> 12);
        *(y++) = (Word16)(s2 >> 12);

        x = x - n;

    }

    return;
}
