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



 Filename: ex_ctrl.cpp

------------------------------------------------------------------------------
*/

/*----------------------------------------------------------------------------
; INCLUDES
----------------------------------------------------------------------------*/
#include "ex_ctrl.h"
#include "typedef.h"
#include "cnst.h"
#include "set_zero.h"
#include "gmed_n.h"
#include "sqrt_l.h"
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
 FUNCTION NAME: ex_ctrl
------------------------------------------------------------------------------
 INPUT AND OUTPUT DEFINITIONS

 Inputs:
 excitation = pointer to current subframe excitation of type Word16
 excEnergy = Exc. Energy, sqrt(totEx*totEx) of type Word16
 exEnergyHist = pointer to history of subframe energies of type Word16
 voicedHangover = # of fr. after last voiced fr  of type Word16
 carefulFlag = restrict dynamic in scaling of type Word16
 pOverflow = pointer to overflow indicator

 Outputs:
 pOverflow = 1 if overflow exists in the math functions called by this function.

 Returns:
    None

 Global Variables Used:
    None

 Local Variables Needed:
    None

------------------------------------------------------------------------------
 FUNCTION DESCRIPTION

 Function    : Ex_ctrl
 Purpose     : Charaterice synthesis speech and detect background noise
 Returns     : background noise decision; 0 = no bgn, 1 = bgn

------------------------------------------------------------------------------
 REQUIREMENTS

 None

------------------------------------------------------------------------------
 REFERENCES

 ex_ctrl.c, 3GPP TS 26.101 version 4.1.0 Release 4, June 2001

------------------------------------------------------------------------------
 PSEUDO-CODE



------------------------------------------------------------------------------
 CAUTION [optional]
 [State any special notes, constraints or cautions for users of this function]

------------------------------------------------------------------------------
*/
Word16 Ex_ctrl(Word16 excitation[],    /*i/o: Current subframe excitation   */
               Word16 excEnergy,      /* i : Exc. Energy, sqrt(totEx*totEx)*/
               Word16 exEnergyHist[], /* i : History of subframe energies  */
               Word16 voicedHangover, /* i : # of fr. after last voiced fr.*/
               Word16 prevBFI,        /* i : Set i previous BFI            */
               Word16 carefulFlag,    /* i : Restrict dymamic in scaling   */
               Flag   *pOverflow
              )
{
    Word16 i, exp;
    Word16 testEnergy, scaleFactor, avgEnergy, prevEnergy;
    Word32 t0;

    /* get target level */
    avgEnergy = gmed_n(exEnergyHist, 9);

    prevEnergy = (exEnergyHist[7] + exEnergyHist[8]) >> 1;
    if (exEnergyHist[8] < prevEnergy)
    {
        prevEnergy = exEnergyHist[8];
    }

    /* upscaling to avoid too rapid energy rises  for some cases */
    if ((excEnergy < avgEnergy) && (excEnergy > 5))
    {
        testEnergy = shl(prevEnergy, 2, pOverflow);  /* testEnergy = 4*prevEnergy; */

        if ((voicedHangover < 7) || prevBFI != 0)
        {
            /* testEnergy = 3*prevEnergy */
            testEnergy = sub(testEnergy, prevEnergy, pOverflow);
        }

        if (avgEnergy > testEnergy)
        {
            avgEnergy = testEnergy;
        }

        /* scaleFactor=avgEnergy/excEnergy in Q0 (const 29 below)*/
        exp = norm_s(excEnergy);
        excEnergy = shl(excEnergy, exp, pOverflow);
        excEnergy = div_s((Word16) 16383, excEnergy);
        t0 = L_mult(avgEnergy, excEnergy, pOverflow);
        t0 = L_shr(t0, sub(20, exp, pOverflow), pOverflow);
        /* const=30 for t0 in Q0, 20 for Q10 */
        if (t0 > 32767)
        {
            t0 = 32767; /* saturate  */
        }
        scaleFactor = (Word16)(t0);

        /* test if scaleFactor > 3.0 */
        if (carefulFlag != 0 && (scaleFactor > 3072))
        {
            scaleFactor = 3072;
        }

        /* scale the excitation by scaleFactor */
        for (i = 0; i < L_SUBFR; i++)
        {
            t0 = L_mult(scaleFactor, excitation[i], pOverflow);
            t0 = L_shr(t0, 11, pOverflow);
            excitation[i] = (Word16)(t0);
        }
    }

    return 0;
}
