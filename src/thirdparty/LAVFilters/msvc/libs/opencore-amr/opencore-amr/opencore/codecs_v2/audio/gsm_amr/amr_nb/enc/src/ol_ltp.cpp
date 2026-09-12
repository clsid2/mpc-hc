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



 Filename: ol_ltp.cpp
 Functions: ol_ltp

*/

/*----------------------------------------------------------------------------
; INCLUDES
----------------------------------------------------------------------------*/
#include "ol_ltp.h"
#include "cnst.h"
#include "pitch_ol.h"
#include "p_ol_wgh.h"

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
 FUNCTION NAME: ol_ltp
------------------------------------------------------------------------------
 INPUT AND OUTPUT DEFINITIONS

 Inputs:
    st = pointer to pitchOLWghtState structure
    vadSt = pointer to a vadState structure
    mode = coder mode (Mode)
    wsp = pointer to buffer of signal used to compute the Open loop pitch
    T_op = pointer to open loop pitch lag
    old_lags = pointer to history with old stored Cl lags (Word16)
    ol_gain_flg = pointer to OL gain flag (Word16)
    idx = 16 bit value specifies the frame index
    dtx = Data of type 'Flag' used for dtx. Use dtx=1, do not use dtx=0
    pOverflow = pointer to Overflow indicator (Flag)

 Outputs:
    pOverflow -> 1 if processing this funvction results in satuaration

 Returns:
    Zero

 Global Variables Used:
    None

 Local Variables Needed:
    None

------------------------------------------------------------------------------
 FUNCTION DESCRIPTION

 This function computes the open loop pitch lag.

------------------------------------------------------------------------------
 REQUIREMENTS

 None

------------------------------------------------------------------------------
 REFERENCES

 ol_ltp.c, UMTS GSM AMR speech codec, R99 - Version 3.2.0, March 2, 2001

------------------------------------------------------------------------------
 PSEUDO-CODE

int ol_ltp(
    pitchOLWghtState *st, // i/o : State struct
    vadState *vadSt,      // i/o : VAD state struct
    enum Mode mode,       // i   : coder mode
    Word16 wsp[],         // i   : signal used to compute the OL pitch, Q0
                          //       uses signal[-pit_max] to signal[-1]
    Word16 *T_op,         // o   : open loop pitch lag,                 Q0
    Word16 old_lags[],    // i   : history with old stored Cl lags
    Word16 ol_gain_flg[], // i   : OL gain flag
    Word16 idx,           // i   : index
    Flag dtx              // i   : dtx flag; use dtx=1, do not use dtx=0
    )
{
   if (sub ((Word16)mode, (Word16)MR102) != 0 )
   {
      ol_gain_flg[0] = 0;
      ol_gain_flg[1] = 0;
   }

   if (sub ((Word16)mode, (Word16)MR475) == 0 || sub ((Word16)mode, (Word16)MR515) == 0 )
   {
      *T_op = Pitch_ol(vadSt, mode, wsp, PIT_MIN, PIT_MAX, L_FRAME, idx, dtx);
   }
   else
   {
      if ( sub ((Word16)mode, (Word16)MR795) <= 0 )
      {
         *T_op = Pitch_ol(vadSt, mode, wsp, PIT_MIN, PIT_MAX, L_FRAME_BY2,
                          idx, dtx);
      }
      else if ( sub ((Word16)mode, (Word16)MR102) == 0 )
      {
         *T_op = Pitch_ol_wgh(st, vadSt, wsp, PIT_MIN, PIT_MAX, L_FRAME_BY2,
                              old_lags, ol_gain_flg, idx, dtx);
      }
      else
      {
         *T_op = Pitch_ol(vadSt, mode, wsp, PIT_MIN_MR122, PIT_MAX,
                          L_FRAME_BY2, idx, dtx);
      }
   }
   return 0;
}

------------------------------------------------------------------------------
 CAUTION [optional]
 [State any special notes, constraints or cautions for users of this function]

------------------------------------------------------------------------------
*/


/*----------------------------------------------------------------------------
; FUNCTION CODE
----------------------------------------------------------------------------*/
void ol_ltp(
    pitchOLWghtState *st, /* i/o : State struct                            */
    vadState *vadSt,      /* i/o : VAD state struct                        */
    enum Mode mode,       /* i   : coder mode                              */
    Word16 wsp[],         /* i   : signal used to compute the OL pitch, Q0 */
    /*       uses signal[-pit_max] to signal[-1]     */
    Word16 *T_op,         /* o   : open loop pitch lag,                 Q0 */
    Word16 old_lags[],    /* i   : history with old stored Cl lags         */
    Word16 ol_gain_flg[], /* i   : OL gain flag                            */
    Word16 idx,           /* i   : index                                   */
    Flag dtx,             /* i   : dtx flag; use dtx=1, do not use dtx=0   */
    Flag *pOverflow       /* i/o : overflow indicator                      */
)
{
    if ((mode != MR102))
    {
        ol_gain_flg[0] = 0;
        ol_gain_flg[1] = 0;
    }

    if ((mode == MR475) || (mode == MR515))
    {
        *T_op = Pitch_ol(vadSt, mode, wsp, PIT_MIN, PIT_MAX, L_FRAME, idx, dtx,
                         pOverflow);
    }
    else
    {
        if (mode <= MR795)
        {
            *T_op = Pitch_ol(vadSt, mode, wsp, PIT_MIN, PIT_MAX, L_FRAME_BY2,
                             idx, dtx, pOverflow);
        }
        else if (mode == MR102)
        {
            *T_op = Pitch_ol_wgh(st, vadSt, wsp, PIT_MIN, PIT_MAX, L_FRAME_BY2,
                                 old_lags, ol_gain_flg, idx, dtx, pOverflow);
        }
        else
        {
            *T_op = Pitch_ol(vadSt, mode, wsp, PIT_MIN_MR122, PIT_MAX,
                             L_FRAME_BY2, idx, dtx, pOverflow);
        }
    }

}

