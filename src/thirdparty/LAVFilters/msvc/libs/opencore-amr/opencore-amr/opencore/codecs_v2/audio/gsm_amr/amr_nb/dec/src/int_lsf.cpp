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



 Filename: int_lsf.cpp

------------------------------------------------------------------------------
*/

/*----------------------------------------------------------------------------
; INCLUDES
----------------------------------------------------------------------------*/
#include    "int_lsf.h"
#include    "typedef.h"
#include    "basic_op.h"
#include    "cnst.h"

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
 FUNCTION NAME: Int_lsf
------------------------------------------------------------------------------
 INPUT AND OUTPUT DEFINITIONS

 Inputs:
    lsf_old = LSF vector at the 4th SF of past frame (Word16)
    lsf_new = LSF vector at the 4th SF of present frame (Word16)
    i_subfr = Current subframe (equal to 0,40,80 or 120) (Word16)
    lsf_out = interpolated LSF parameters for current subframe (Word16)

 Outputs:
    lsf_out   = new interpolated LSF parameters for current subframe
    pOverflow = pointer of type Flag * to overflow indicator.

 Returns:
    None.

 Global Variables Used:
    None.

 Local Variables Needed:
    None.

------------------------------------------------------------------------------
 FUNCTION DESCRIPTION

 This function interpolates the LSFs for selected subframe.
 The 20 ms speech frame is divided into 4 subframes. The LSFs are
 interpolated at the 1st, 2nd and 3rd subframe and only forwarded
 at the 4th subframe.

                      |------|------|------|------|
                         sf1    sf2    sf3    sf4
                   F0                          F1

                 sf1:   3/4 F0 + 1/4 F1         sf3:   1/4 F0 + 3/4 F1
                 sf2:   1/2 F0 + 1/2 F1         sf4:       F1

------------------------------------------------------------------------------
 REQUIREMENTS

 None.

------------------------------------------------------------------------------
 REFERENCES

 int_lsf.c, UMTS GSM AMR speech codec, R99 - Version 3.2.0, March 2, 2001

------------------------------------------------------------------------------
 PSEUDO-CODE

void Int_lsf(
    Word16 lsf_old[], // i : LSF vector at the 4th SF of past frame
    Word16 lsf_new[], // i : LSF vector at the 4th SF of present frame
    Word16 i_subfr,   // i : Pointer to current sf (equal to 0,40,80 or 120)
    Word16 lsf_out[]  // o : interpolated LSF parameters for current sf
)
{
    Word16 i;

    if ( i_subfr == 0 )
    {
       for (i = 0; i < M; i++) {
          lsf_out[i] = add(sub(lsf_old[i], shr(lsf_old[i], 2)),
                           shr(lsf_new[i], 2));
       }
    }
    else if ( sub(i_subfr, 40) == 0 )
    {
       for (i = 0; i < M; i++) {
          lsf_out[i] = add(shr(lsf_old[i],1), shr(lsf_new[i], 1) );
       }
    }
    else if ( sub(i_subfr, 80) == 0 )
    {
       for (i = 0; i < M; i++) {
          lsf_out[i] = add(shr(lsf_old[i], 2),
                           sub(lsf_new[i], shr(lsf_new[i], 2)));
       }
    }
    else if ( sub(i_subfr, 120) == 0 )
    {
       for (i = 0; i < M; i++) {
          lsf_out[i] = lsf_new[i];
       }
    }

    return;
}

------------------------------------------------------------------------------
 CAUTION [optional]
 [State any special notes, constraints or cautions for users of this function]

------------------------------------------------------------------------------
*/

void Int_lsf(
    Word16 lsf_old[], /* i : LSF vector at the 4th SF of past frame         */
    Word16 lsf_new[], /* i : LSF vector at the 4th SF of present frame      */
    Word16 i_subfr,   /* i : Current sf (equal to 0,40,80 or 120)           */
    Word16 lsf_out[], /* o : interpolated LSF parameters for current sf     */
    Flag  *pOverflow  /* o : flag set if overflow occurs                    */
)
{
    register Word16 i;
    register Word16 temp1;
    register Word16 temp2;

    if (i_subfr == 0)
    {
        for (i = M - 1; i >= 0; i--)
        {
            if (*(lsf_old + i) < 0)
            {
                temp1 = ~(~(*(lsf_old + i)) >> 2);
            }
            else
            {
                temp1 = *(lsf_old + i) >> 2;
            }
            if (*(lsf_new + i) < 0)
            {
                temp2 = ~(~(*(lsf_new + i)) >> 2);
            }
            else
            {
                temp2 = *(lsf_new + i) >> 2;
            }
            *(lsf_out + i) = add_16((Word16)(*(lsf_old + i) - temp1),
                                    (Word16)temp2,
                                    pOverflow);
        }
    }

    else if (i_subfr == 40)
    {
        for (i = M - 1; i >= 0; i--)
        {
            if (*(lsf_old + i) < 0)
            {
                temp1 = ~(~(*(lsf_old + i)) >> 1);
            }
            else
            {
                temp1 = *(lsf_old + i) >> 1;
            }
            if (*(lsf_new + i) < 0)
            {
                temp2 = ~(~(*(lsf_new + i)) >> 1);
            }
            else
            {
                temp2 = *(lsf_new + i) >> 1;
            }
            *(lsf_out + i) = temp1 + temp2;
        }
    }

    else if (i_subfr == 80)
    {
        for (i = M - 1; i >= 0; i--)
        {
            if (*(lsf_old + i) < 0)
            {
                temp1 = ~(~(*(lsf_old + i)) >> 2);
            }
            else
            {
                temp1 = *(lsf_old + i) >> 2;
            }
            if (*(lsf_new + i) < 0)
            {
                temp2 = ~(~(*(lsf_new + i)) >> 2);
            }
            else
            {
                temp2 = *(lsf_new + i) >> 2;
            }
            *(lsf_out + i) = add_16((Word16)temp1,
                                    (Word16)(*(lsf_new + i) - temp2),
                                    pOverflow);

        }
    }

    else if (i_subfr == 120)
    {
        for (i = M - 1; i >= 0; i--)
        {
            *(lsf_out + i) = *(lsf_new + i);
        }
    }

    return;
}

