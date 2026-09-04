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



 Filename: calc_cor.cpp

------------------------------------------------------------------------------
*/

/*----------------------------------------------------------------------------
; INCLUDES
----------------------------------------------------------------------------*/
#include "calc_cor.h"
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
 FUNCTION NAME: comp_corr
------------------------------------------------------------------------------
 INPUT AND OUTPUT DEFINITIONS

 Inputs:
    scal_sig = array of input samples. (Word16)
    L_frame = length of frame used to compute pitch(Word16)
    lag_max = maximum lag (Word16)
    lag_min = minimum lag (Word16)
    corr = pointer to array of correlations corresponding to the selected
        lags. (Word32)

 Outputs:
    corr = pointer to array of correlations corresponding to the selected
        lags. (Word32)

 Returns:
    none

 Global Variables Used:
    none

 Local Variables Needed:
    none

------------------------------------------------------------------------------
 FUNCTION DESCRIPTION

 This function calculates all correlations of scal_sig[] in a given delay
 range.

 The correlation is given by

         cor[t] = <scal_sig[n],scal_sig[n-t]>,  t=lag_min,...,lag_max

 The function outputs all of the correlations

------------------------------------------------------------------------------
 REQUIREMENTS

 none

------------------------------------------------------------------------------
 REFERENCES

 [1] calc_cor.c, UMTS GSM AMR speech codec, R99 - Version 3.2.0, March 2, 2001

------------------------------------------------------------------------------
 PSEUDO-CODE

void comp_corr (
    Word16 scal_sig[],  // i   : scaled signal.
    Word16 L_frame,     // i   : length of frame to compute pitch
    Word16 lag_max,     // i   : maximum lag
    Word16 lag_min,     // i   : minimum lag
    Word32 corr[])      // o   : correlation of selected lag
{
    Word16 i, j;
    Word16 *p, *p1;
    Word32 t0;

    for (i = lag_max; i >= lag_min; i--)
    {
       p = scal_sig;
       p1 = &scal_sig[-i];
       t0 = 0;

       for (j = 0; j < L_frame; j++, p++, p1++)
       {
          t0 = L_mac (t0, *p, *p1);
       }
       corr[-i] = t0;
    }

    return;
}

------------------------------------------------------------------------------
 CAUTION [optional]
 [State any special notes, constraints or cautions for users of this function]

------------------------------------------------------------------------------
*/

void comp_corr(
    Word16 scal_sig[],  /* i   : scaled signal.                     */
    Word16 L_frame,     /* i   : length of frame to compute pitch   */
    Word16 lag_max,     /* i   : maximum lag                        */
    Word16 lag_min,     /* i   : minimum lag                        */
    Word32 corr[])      /* o   : correlation of selected lag        */
{




    /*---------------------------------------------------
    ; lag_max and lag_min are typically negative numbers
    -----------------------------------------------------*/


    /* PIT_MIN_MR122 18        Minimum pitch lag (MR122 mode)           */
    /* PIT_MIN       20        Minimum pitch lag (all other modes)      */
    /* PIT_MAX       143       Maximum pitch lag                        */


    Word16 i;
    Word16 j;
    Word16 *p;
    Word16 *p1;
    Word16 *p2;
    Word16 *p_scal_sig;
    Word32 t1;
    Word32 t2;
    Word32 t3;
    Word32 t4;

    corr = corr - lag_max ;
    p_scal_sig = &scal_sig[-lag_max];

    for (i = ((lag_max - lag_min) >> 2) + 1; i > 0; i--)
    {
        t1 = 0;
        t2 = 0;
        t3 = 0;
        t4 = 0;
        p  = &scal_sig[0];
        p1 = p_scal_sig++;
        p_scal_sig++;
        p2 = p_scal_sig++;
        p_scal_sig++;
        for (j = (L_frame >> 1); j != 0; j--)
        {
            t1 = amrnb_fxp_mac_16_by_16bb((Word32) * (p), (Word32) * (p1++), t1);
            t2 = amrnb_fxp_mac_16_by_16bb((Word32) * (p), (Word32) * (p1), t2);
            t3 = amrnb_fxp_mac_16_by_16bb((Word32) * (p), (Word32) * (p2++), t3);
            t4 = amrnb_fxp_mac_16_by_16bb((Word32) * (p++), (Word32) * (p2), t4);

            t1 = amrnb_fxp_mac_16_by_16bb((Word32) * (p), (Word32) * (p1++), t1);
            t2 = amrnb_fxp_mac_16_by_16bb((Word32) * (p), (Word32) * (p1), t2);
            t3 = amrnb_fxp_mac_16_by_16bb((Word32) * (p), (Word32) * (p2++), t3);
            t4 = amrnb_fxp_mac_16_by_16bb((Word32) * (p++), (Word32) * (p2), t4);
        }

        *(corr++) = t1 << 1;
        *(corr++) = t2 << 1;
        *(corr++) = t3 << 1;
        *(corr++) = t4 << 1;

    }

    return;
}
