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

 Filename: lflg_upd.cpp
 Functions: LTP_flag_update

------------------------------------------------------------------------------
 MODULE DESCRIPTION

 LTP_flag update for AMR VAD option 2
------------------------------------------------------------------------------
*/

/*----------------------------------------------------------------------------
; INCLUDES
----------------------------------------------------------------------------*/
#include "typedef.h"
#include "cnst.h"
#include "l_extract.h"
#include "mpy_32_16.h"

#include "vad2.h"
#include "mode.h"

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
 FUNCTION NAME:
------------------------------------------------------------------------------
 INPUT AND OUTPUT DEFINITIONS

 Inputs:
    st -- Pointer to vadState2
    mode -- Word16 -- AMR mode

 Outputs:
    st -- Pointer to vadState2
    pOverflow -- Pointer to Flag -- overflow indicator

 Returns:
    None

 Global Variables Used:
    None

 Local Variables Needed:
    None

------------------------------------------------------------------------------
 FUNCTION DESCRIPTION

 LTP_flag update for AMR VAD option 2


 PURPOSE:
   Set LTP_flag if the LTP gain > LTP_THRESHOLD, where the value of
   LTP_THRESHOLD depends on the LTP analysis window length.

 INPUTS:

   mode
                   AMR mode
   vadState->L_R0
                   LTP energy
   vadState->L_Rmax
                   LTP maximum autocorrelation
 OUTPUTS:

   vadState->LTP_flag
                   Set if LTP gain > LTP_THRESHOLD

 RETURN VALUE:

   none


------------------------------------------------------------------------------
 REQUIREMENTS

 None

------------------------------------------------------------------------------
 REFERENCES

 lflg_upd.c, UMTS GSM AMR speech codec, R99 - Version 3.2.0, March 2, 2001

------------------------------------------------------------------------------
 PSEUDO-CODE


------------------------------------------------------------------------------
 CAUTION [optional]
 [State any special notes, constraints or cautions for users of this function]

------------------------------------------------------------------------------
*/

void LTP_flag_update(
    vadState2 * st,
    Word16 mode,
    Flag   *pOverflow)
{
    Word16 thresh;
    Word16 hi1;
    Word16 lo1;
    Word32 Ltmp;

    if ((mode == MR475) || (mode == MR515))
    {
        thresh = 18022; /* (Word16)(32768.0*0.55); */
    }
    else if (mode == MR102)
    {
        thresh = 19660; /* (Word16)(32768.0*0.60); */
    }
    else
    {
        thresh = 21299; /* (Word16)(32768.0*0.65); */
    }

    L_Extract(st->L_R0, &hi1, &lo1, pOverflow);

    Ltmp = Mpy_32_16(hi1, lo1, thresh, pOverflow);

    if (st->L_Rmax > Ltmp)
    {
        st->LTP_flag = TRUE;
    }
    else
    {
        st->LTP_flag = FALSE;
    }

    return;
}
