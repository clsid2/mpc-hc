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
 Filename: lsfwt.cpp
 Functions: Lsf_wt

 ------------------------------------------------------------------------------
 INPUT AND OUTPUT DEFINITIONS

 Inputs:
    lsf -- Pointer to Word16 -- LSF vector

 Outputs:
    wf -- Pointer to Word16 -- square of weighting factors
    pOverflow -- Pointer to type Flag -- Flag set when overflow occurs

 Returns:
    None

 Global Variables Used:
    None

 Local Variables Needed:
    None

------------------------------------------------------------------------------
 FUNCTION DESCRIPTION

Compute LSF weighting factors

 d[i] = lsf[i+1] - lsf[i-1]

 The weighting factors are approximated by two line segment

 First segment passes by the following 2 points:

    d[i] = 0Hz     wf[i] = 3.347
    d[i] = 450Hz   wf[i] = 1.8

 Second segment passes by the following 2 points:

    d[i] = 450Hz   wf[i] = 1.8
    d[i] = 1500Hz  wf[i] = 1.0

 if( d[i] < 450Hz )
   wf[i] = 3.347 - ( (3.347-1.8) / (450-0)) *  d[i]
 else
   wf[i] = 1.8 - ( (1.8-1.0) / (1500-450)) *  (d[i] - 450)


 if( d[i] < 1843)
   wf[i] = 3427 - (28160*d[i])>>15
 else
   wf[i] = 1843 - (6242*(d[i]-1843))>>15

------------------------------------------------------------------------------
 REQUIREMENTS



------------------------------------------------------------------------------
 REFERENCES

 lsfwt.c, UMTS GSM AMR speech codec, R99 - Version 3.2.0, March 2, 2001

------------------------------------------------------------------------------
 PSEUDO-CODE



------------------------------------------------------------------------------
*/


/*----------------------------------------------------------------------------
; INCLUDES
----------------------------------------------------------------------------*/
#include "lsfwt.h"
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


/*----------------------------------------------------------------------------
; FUNCTION CODE
----------------------------------------------------------------------------*/

void Lsf_wt(
    Word16 *lsf,         /* input : LSF vector                  */
    Word16 *wf,          /* output: square of weighting factors */
    Flag   *pOverflow
)
{
    Word16 temp;
    Word16 wgt_fct;
    Word16 i;
    Word16 *p_wf = wf;
    Word16 *p_lsf   = &lsf[0];
    Word16 *p_lsf_2 = &lsf[1];

    OSCL_UNUSED_ARG(pOverflow);

    /* wf[0] = lsf[1] - 0  */
    *(p_wf++) = *(p_lsf_2++);

    for (i = 4; i != 0 ; i--)
    {
        *(p_wf++) = *(p_lsf_2++) - *(p_lsf++);
        *(p_wf++) = *(p_lsf_2++) - *(p_lsf++);
    }
    /*
     *  wf[9] = 4000 - lsf[8]
     */
    *(p_wf) = 16384 - *(p_lsf);

    p_wf = wf;

    for (i = 10; i != 0; i--)
    {
        /*
         *  (wf[i] - 450);
         *  1843 == 450 Hz (Q15 considering 7FFF = 8000 Hz)
         */
        wgt_fct = *p_wf;
        temp =  wgt_fct - 1843;

        if (temp > 0)
        {
            temp = (Word16)(((Word32)temp * 6242) >> 15);
            wgt_fct = 1843 - temp;
        }
        else
        {
            temp = (Word16)(((Word32)wgt_fct * 28160) >> 15);
            wgt_fct = 3427 - temp;
        }

        *(p_wf++) = wgt_fct << 3;

    } /* for (i = 10; i != 0; i--) */

    return;

} /* Lsf_wt() */
