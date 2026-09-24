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

 Filename: bgnscd.cpp
 Functions:
           Bgn_scd_reset
           Bgn_scd

------------------------------------------------------------------------------
 MODULE DESCRIPTION

 Background noise source characteristic detector (SCD)

------------------------------------------------------------------------------
*/


/*----------------------------------------------------------------------------
; INCLUDES
----------------------------------------------------------------------------*/
#include    "bgnscd.h"
#include    "typedef.h"
#include    "basic_op.h"
#include    "cnst.h"
#include    "gmed_n.h"
#include    "sqrt_l.h"
#include    "oscl_mem.h"

/*----------------------------------------------------------------------------
; MACROS
; Define module specific macros here
----------------------------------------------------------------------------*/


/*----------------------------------------------------------------------------
; DEFINES
; Include all pre-processor statements here. Include conditional
; compile variables also.
----------------------------------------------------------------------------*/
#define TRUE  1
#define FALSE 0

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
 FUNCTION NAME: Bgn_scd_reset
------------------------------------------------------------------------------
 INPUT AND OUTPUT DEFINITIONS

 Inputs:
    state = points to memory of type Bgn_scdState.

 Outputs:
    The memory of type Bgn_scdState pointed to by state is set to all
        zeros.

 Returns:
    Returns 0 if memory was successfully initialized,
        otherwise returns -1.

 Global Variables Used:
    None.

 Local Variables Needed:
    None.

------------------------------------------------------------------------------
 FUNCTION DESCRIPTION

 Resets state memory.

------------------------------------------------------------------------------
 REQUIREMENTS

 None

------------------------------------------------------------------------------
 REFERENCES

 bgnscd.c, UMTS GSM AMR speech codec, R99 - Version 3.2.0, March 2, 2001

------------------------------------------------------------------------------
 PSEUDO-CODE

Word16 Bgn_scd_reset (Bgn_scdState *state)
{
   if (state == (Bgn_scdState *) NULL){
      fprintf(stderr, "Bgn_scd_reset: invalid parameter\n");
      return -1;
   }

   // Static vectors to zero
   Set_zero (state->frameEnergyHist, L_ENERGYHIST);

   // Initialize hangover handling
   state->bgHangover = 0;

   return 0;
}

------------------------------------------------------------------------------
 CAUTION [optional]
 [State any special notes, constraints or cautions for users of this function]

------------------------------------------------------------------------------
*/

Word16  Bgn_scd_reset(Bgn_scdState *state)
{
    if (state == (Bgn_scdState *) NULL)
    {
        /* fprintf(stderr, "Bgn_scd_reset: invalid parameter\n");  */
        return(-1);
    }

    /* Static vectors to zero */
    oscl_memset(state->frameEnergyHist, 0, L_ENERGYHIST*sizeof(Word16));

    /* Initialize hangover handling */
    state->bgHangover = 0;

    return(0);
}

/****************************************************************************/

/*
------------------------------------------------------------------------------
 FUNCTION NAME: Bgn_scd
------------------------------------------------------------------------------
 INPUT AND OUTPUT DEFINITIONS

 Inputs:
    st = pointer to state variables of type Bgn_scdState
    ltpGainHist[] = LTP gain history (Word16)
    speech[] = synthesis speech frame (Word16)
    voicedHangover = pointer to # of frames after last voiced frame (Word16)
    pOverflow      = pointer to overflow indicator (Flag)

 Outputs:
    st = function updates the state variables of type Bgn_scdState
        pointed to by st.
    voicedHangover = function updates the # of frames after last voiced
        frame pointed to by voicedHangover.
    pOverflow = 1 if the basic math function L_add() results in saturation.
                  else pOverflow is zero.

 Returns:
    inbgNoise = flag if background noise is present (Word16)

 Global Variables Used:
    None.

 Local Variables Needed:
    None.

------------------------------------------------------------------------------
 FUNCTION DESCRIPTION

 Characterize synthesis speech and detect background noise.

------------------------------------------------------------------------------
 REQUIREMENTS

 None

------------------------------------------------------------------------------
 REFERENCES

 bgnscd.c, UMTS GSM AMR speech codec, R99 - Version 3.2.0, March 2, 2001

------------------------------------------------------------------------------
 PSEUDO-CODE

Word16 Bgn_scd (Bgn_scdState *st,      // i : State variables for bgn SCD
                Word16 ltpGainHist[],  // i : LTP gain history
                Word16 speech[],       // o : synthesis speech frame
                Word16 *voicedHangover // o : # of frames after last
                                              voiced frame
                )
{
   Word16 i;
   Word16 prevVoiced, inbgNoise;
   Word16 temp;
   Word16 ltpLimit, frameEnergyMin;
   Word16 currEnergy, noiseFloor, maxEnergy, maxEnergyLastPart;
   Word32 s;

   // Update the inBackgroundNoise flag (valid for use in next frame if BFI)
   // it now works as a energy detector floating on top
   // not as good as a VAD.

   currEnergy = 0;
   s = (Word32) 0;

   for (i = 0; i < L_FRAME; i++)
   {
       s = L_mac (s, speech[i], speech[i]);
   }

   s = L_shl(s, 2);

   currEnergy = extract_h (s);

   frameEnergyMin = 32767;

   for (i = 0; i < L_ENERGYHIST; i++)
   {
      if (sub(st->frameEnergyHist[i], frameEnergyMin) < 0)
         frameEnergyMin = st->frameEnergyHist[i];
   }

   noiseFloor = shl (frameEnergyMin, 4); // Frame Energy Margin of 16

   maxEnergy = st->frameEnergyHist[0];
   for (i = 1; i < L_ENERGYHIST-4; i++)
   {
      if ( sub (maxEnergy, st->frameEnergyHist[i]) < 0)
      {
         maxEnergy = st->frameEnergyHist[i];
      }
   }

   maxEnergyLastPart = st->frameEnergyHist[2*L_ENERGYHIST/3];
   for (i = 2*L_ENERGYHIST/3+1; i < L_ENERGYHIST; i++)
   {
      if ( sub (maxEnergyLastPart, st->frameEnergyHist[i] ) < 0)
      {
         maxEnergyLastPart = st->frameEnergyHist[i];
      }
   }

   inbgNoise = 0;        // false

   // Do not consider silence as noise
   // Do not consider continuous high volume as noise
   // Or if the current noise level is very low
   // Mark as noise if under current noise limit
   // OR if the maximum energy is below the upper limit

   if ( (sub(maxEnergy, LOWERNOISELIMIT) > 0) &&
        (sub(currEnergy, FRAMEENERGYLIMIT) < 0) &&
        (sub(currEnergy, LOWERNOISELIMIT) > 0) &&
        ( (sub(currEnergy, noiseFloor) < 0) ||
          (sub(maxEnergyLastPart, UPPERNOISELIMIT) < 0)))
   {
      if (sub(add(st->bgHangover, 1), 30) > 0)
      {
         st->bgHangover = 30;
      } else
      {
         st->bgHangover = add(st->bgHangover, 1);
      }
   }
   else
   {
      st->bgHangover = 0;
   }

   // make final decision about frame state , act somewhat cautiosly
   if (sub(st->bgHangover,1) > 0)
      inbgNoise = 1;       // true

   for (i = 0; i < L_ENERGYHIST-1; i++)
   {
      st->frameEnergyHist[i] = st->frameEnergyHist[i+1];
   }
   st->frameEnergyHist[L_ENERGYHIST-1] = currEnergy;

   // prepare for voicing decision; tighten the threshold after some
      time in noise
   ltpLimit = 13926;             // 0.85  Q14
   if (sub(st->bgHangover, 8) > 0)
   {
      ltpLimit = 15565;          // 0.95  Q14
   }
   if (sub(st->bgHangover, 15) > 0)
   {
      ltpLimit = 16383;          // 1.00  Q14
   }

   // weak sort of voicing indication.
   prevVoiced = 0;        // false

   if (sub(gmed_n(&ltpGainHist[4], 5), ltpLimit) > 0)
   {
      prevVoiced = 1;     // true
   }
   if (sub(st->bgHangover, 20) > 0) {
      if (sub(gmed_n(ltpGainHist, 9), ltpLimit) > 0)
      {
         prevVoiced = 1;  // true
      }
      else
      {
         prevVoiced = 0;  // false
      }
   }

   if (prevVoiced)
   {
      *voicedHangover = 0;
   }
   else
   {
      temp = add(*voicedHangover, 1);
      if (sub(temp, 10) > 0)
      {
         *voicedHangover = 10;
      }
      else
      {
         *voicedHangover = temp;
      }
   }

   return inbgNoise;
}

------------------------------------------------------------------------------
 CAUTION [optional]
 [State any special notes, constraints or cautions for users of this function]

------------------------------------------------------------------------------
*/

Word16  Bgn_scd(Bgn_scdState *st,       /* i : State variables for bgn SCD  */
                Word16 ltpGainHist[],  /* i : LTP gain history             */
                Word16 speech[],       /* o : synthesis speech frame       */
                Word16 *voicedHangover,/* o : # of frames after last
                                               voiced frame                 */
                Flag   *pOverflow
               )
{
    Word16  i;
    Word16  prevVoiced, inbgNoise;
    Word16  temp;
    Word16  ltpLimit, frameEnergyMin;
    Word16  currEnergy, noiseFloor, maxEnergy, maxEnergyLastPart;
    Word32  s, L_temp;


    /* Update the inBackgroundNoise flag (valid for use in next frame if BFI)   */
    /* it now works as a energy detector floating on top                        */
    /* not as good as a VAD.                                                    */

    s = (Word32) 0;

    for (i = L_FRAME - 1; i >= 0; i--)
    {
        L_temp = ((Word32) speech[i]) * speech[i];
        if (L_temp != (Word32) 0x40000000L)
        {
            L_temp = L_temp << 1;
        }
        else
        {
            L_temp = MAX_32;
        }
        s = L_add(s, L_temp, pOverflow);
    }

    /* s is a sum of squares, so don't need to check for neg overflow */
    if (s > (Word32)0x1fffffffL)
    {
        currEnergy = MAX_16;
    }
    else
    {
        currEnergy = (Word16)(s >> 14);
    }

    frameEnergyMin = 32767;
    for (i = L_ENERGYHIST - 1; i >= 0; i--)
    {
        if (st->frameEnergyHist[i] < frameEnergyMin)
        {
            frameEnergyMin = st->frameEnergyHist[i];
        }
    }

    /* Frame Energy Margin of 16 */
    L_temp = (Word32)frameEnergyMin << 4;
    if (L_temp != (Word32)((Word16) L_temp))
    {
        if (L_temp > 0)
        {
            noiseFloor = MAX_16;
        }
        else
        {
            noiseFloor = MIN_16;
        }
    }
    else
    {
        noiseFloor = (Word16)(L_temp);
    }

    maxEnergy = st->frameEnergyHist[0];
    for (i = L_ENERGYHIST - 5; i >= 1; i--)
    {
        if (maxEnergy < st->frameEnergyHist[i])
        {
            maxEnergy = st->frameEnergyHist[i];
        }
    }

    maxEnergyLastPart = st->frameEnergyHist[2*L_ENERGYHIST/3];
    for (i = 2 * L_ENERGYHIST / 3 + 1; i < L_ENERGYHIST; i++)
    {
        if (maxEnergyLastPart < st->frameEnergyHist[i])
        {
            maxEnergyLastPart = st->frameEnergyHist[i];
        }
    }

    /* Do not consider silence as noise */
    /* Do not consider continuous high volume as noise */
    /* Or if the current noise level is very low */
    /* Mark as noise if under current noise limit */
    /* OR if the maximum energy is below the upper limit */

    if ((maxEnergy > LOWERNOISELIMIT) &&
            (currEnergy < FRAMEENERGYLIMIT) &&
            (currEnergy > LOWERNOISELIMIT) &&
            ((currEnergy < noiseFloor) ||
             (maxEnergyLastPart < UPPERNOISELIMIT)))
    {
        if ((st->bgHangover + 1) > 30)
        {
            st->bgHangover = 30;
        }
        else
        {
            st->bgHangover += 1;
        }
    }
    else
    {
        st->bgHangover = 0;
    }

    /* make final decision about frame state , act somewhat cautiosly */

    if (st->bgHangover > 1)
    {
        inbgNoise = TRUE;
    }
    else
    {
        inbgNoise = FALSE;
    }

    for (i = 0; i < L_ENERGYHIST - 1; i++)
    {
        st->frameEnergyHist[i] = st->frameEnergyHist[i+1];
    }
    st->frameEnergyHist[L_ENERGYHIST-1] = currEnergy;

    /* prepare for voicing decision; tighten the threshold after some
       time in noise */

    if (st->bgHangover > 15)
    {
        ltpLimit = 16383;       /* 1.00  Q14 */
    }
    else if (st->bgHangover > 8)
    {
        ltpLimit = 15565;       /* 0.95  Q14 */
    }
    else
    {
        ltpLimit = 13926;       /* 0.85  Q14 */
    }

    /* weak sort of voicing indication. */
    prevVoiced = FALSE;

    if (gmed_n(&ltpGainHist[4], 5) > ltpLimit)
    {
        prevVoiced = TRUE;
    }

    if (st->bgHangover > 20)
    {
        if (gmed_n(ltpGainHist, 9) > ltpLimit)
        {
            prevVoiced = TRUE;
        }
        else
        {
            prevVoiced = FALSE;
        }
    }


    if (prevVoiced)
    {
        *voicedHangover = 0;
    }
    else
    {
        temp = *voicedHangover + 1;

        if (temp > 10)
        {
            *voicedHangover = 10;
        }
        else
        {
            *voicedHangover = temp;
        }
    }

    return(inbgNoise);
}
