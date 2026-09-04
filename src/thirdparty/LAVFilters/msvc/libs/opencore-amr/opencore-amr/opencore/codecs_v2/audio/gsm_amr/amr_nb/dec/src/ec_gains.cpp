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




 Filename: ec_gains.cpp

------------------------------------------------------------------------------
 MODULE DESCRIPTION

 These modules execute the code book gains for error concealment. This module
 contains the init, reset, exit, and "main" functions in this process.

------------------------------------------------------------------------------
*/


/*----------------------------------------------------------------------------
; INCLUDES
----------------------------------------------------------------------------*/
#include "ec_gains.h"
#include "typedef.h"
#include "cnst.h"
#include "gmed_n.h"
#include "gc_pred.h"
#include "basic_op.h"


/*--------------------------------------------------------------------------*/
#ifdef __cplusplus
extern "C"
{
#endif

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

    /*--------------------------------------------------------------------------*/
#ifdef __cplusplus
}
#endif

/*
------------------------------------------------------------------------------
 FUNCTION NAME: ec_gain_code_reset
------------------------------------------------------------------------------
 INPUT AND OUTPUT DEFINITIONS

 Inputs:
  state = pointer to a pointer to a structure containing code state data of
          stucture type ec_gain_codeState

 Outputs:
    None.

 Returns:
    None

 Global Variables Used:
    None.

 Local Variables Needed:
    None.

------------------------------------------------------------------------------
 FUNCTION DESCRIPTION

 This function resets the state data for the ec_gain module.

------------------------------------------------------------------------------
 REQUIREMENTS

 None

------------------------------------------------------------------------------
 REFERENCES

 None

------------------------------------------------------------------------------
 PSEUDO-CODE

int ec_gain_code_reset (ec_gain_codeState *state)
{
  Word16 i;

  if (state == (ec_gain_codeState *) NULL){
      // fprintf(stderr, "ec_gain_code_reset: invalid parameter\n");
      return -1;
  }

  for ( i = 0; i < 5; i++)
      state->gbuf[i] = 1;
  state->past_gain_code = 0;
  state->prev_gc = 1;

  return 0;
}

------------------------------------------------------------------------------
 CAUTION [optional]
 [State any special notes, constraints or cautions for users of this function]

------------------------------------------------------------------------------
*/

Word16 ec_gain_code_reset(ec_gain_codeState *state)
{
    Word16 i;

    if (state == (ec_gain_codeState *) NULL)
    {
        /* fprintf(stderr, "ec_gain_code_reset: invalid parameter\n"); */
        return -1;
    }

    for (i = 0; i < 5; i++)
        state->gbuf[i] = 1;
    state->past_gain_code = 0;
    state->prev_gc = 1;

    return 0;
}

/*
------------------------------------------------------------------------------
 FUNCTION NAME: ec_gain_code
------------------------------------------------------------------------------
 INPUT AND OUTPUT DEFINITIONS

 Inputs:
  st = pointer to a pointer to a structure containing code state data of
       stucture type ec_gain_codeState
  pred_state = pointer to MA predictor state of type gc_predState
  state  = state of the state machine of type Word16
  gain_code = pointer to decoded innovation gain of type Word16
  pOverflow = pointer to overflow indicator of type Flag

 Outputs:
  st = pointer to a pointer to a structure containing code state data of
       stucture type ec_gain_codeState
  pred_state = pointer to MA predictor state of type gc_predState
  pOverflow = 1 if there is an overflow else it is zero.

 Returns:
    None.

 Global Variables Used:
    None.

 Local Variables Needed:
    None.

------------------------------------------------------------------------------
 FUNCTION DESCRIPTION

This function does error concealment using the codebook. Call this function
only in BFI (instead of normal gain decoding function).

------------------------------------------------------------------------------
 REQUIREMENTS

 None.

------------------------------------------------------------------------------
 REFERENCES

 ec_gain.c, UMTS GSM AMR speech codec, R99 - Version 3.2.0, March 2, 2001

------------------------------------------------------------------------------
 PSEUDO-CODE

    static const Word16 cdown[7] =
    {
        32767, 32112, 32112, 32112,
        32112, 32112, 22937
    };

    Word16 tmp;
    Word16 qua_ener_MR122;
    Word16 qua_ener;

    // calculate median of last five gain values
    tmp = gmed_n (st->gbuf,5);

    // new gain = minimum(median, past_gain) * cdown[state]
    if (sub (tmp, st->past_gain_code) > 0)
    {
        tmp = st->past_gain_code;
    }
    tmp = mult (tmp, cdown[state]);
    *gain_code = tmp;

    // update table of past quantized energies with average of
    // current values

    gc_pred_average_limited(pred_state, &qua_ener_MR122, &qua_ener);
    gc_pred_update(pred_state, qua_ener_MR122, qua_ener);
}

------------------------------------------------------------------------------
 CAUTION [optional]
 [State any special notes, constraints or cautions for users of this function]

------------------------------------------------------------------------------
*/
void ec_gain_code(
    ec_gain_codeState *st,    /* i/o : State struct                     */
    gc_predState *pred_state, /* i/o : MA predictor state               */
    Word16 state,             /* i   : state of the state machine       */
    Word16 *gain_code,        /* o   : decoded innovation gain          */
    Flag   *pOverflow
)
{
    static const Word16 cdown[7] =
    {
        32767, 32112, 32112, 32112,
        32112, 32112, 22937
    };

    Word16 tmp;
    Word16 qua_ener_MR122;
    Word16 qua_ener;

    /* calculate median of last five gain values */
    tmp = gmed_n(st->gbuf, 5);

    /* new gain = minimum(median, past_gain) * cdown[state] */
    if (sub(tmp, st->past_gain_code, pOverflow) > 0)
    {
        tmp = st->past_gain_code;
    }
    tmp = mult(tmp, cdown[state], pOverflow);
    *gain_code = tmp;

    /* update table of past quantized energies with average of
     * current values
     */
    gc_pred_average_limited(pred_state, &qua_ener_MR122, &qua_ener, pOverflow);
    gc_pred_update(pred_state, qua_ener_MR122, qua_ener);
}

/****************************************************************************/

/*
------------------------------------------------------------------------------
 FUNCTION NAME: ec_gain_code_update
------------------------------------------------------------------------------
 INPUT AND OUTPUT DEFINITIONS

 Inputs:
  st = pointer to a pointer to a structure containing code state data of
       stucture type ec_gain_codeState
  bfi = a flag that indicates if the frame is bad of type Word16
  prev_bf = a flag that indicates if the previous frame was bad of type Word16
  gain_code = pointer to decoded innovation gain of type Word16
  pOverflow = pointer to overflow indicator of type Flag

 Outputs:
  st = pointer to a pointer to a structure containing code state data of
       stucture type ec_gain_codeState
  gain_code = pointer to decoded innovation gain of type Word16
  pOverflow = 1 if there is an overflow else it is zero.

 Returns:
    None.

 Global Variables Used:
    None.

 Local Variables Needed:
    None.

------------------------------------------------------------------------------
 FUNCTION DESCRIPTION

  Purpose     : update the codebook gain concealment state;
                limit gain_code if the previous frame was bad
                Call this function always after decoding (or concealing)
                the gain

------------------------------------------------------------------------------
 REQUIREMENTS

 None.

------------------------------------------------------------------------------
 REFERENCES

 ec_gain.c, UMTS GSM AMR speech codec, R99 - Version 3.2.0, March 2, 2001

------------------------------------------------------------------------------
 PSEUDO-CODE

    Word16 i;

    // limit gain_code by previous good gain if previous frame was bad
    if (bfi == 0)
    {
        if (prev_bf != 0)
        {
            if (sub (*gain_code, st->prev_gc) > 0)
            {
                *gain_code = st->prev_gc;
            }
        }
        st->prev_gc = *gain_code;
    }

    // update EC states: previous gain, gain buffer
    st->past_gain_code = *gain_code;

    for (i = 1; i < 5; i++)
    {
        st->gbuf[i - 1] = st->gbuf[i];
    }
    st->gbuf[4] = *gain_code;

    return;
}

------------------------------------------------------------------------------
 CAUTION [optional]
 [State any special notes, constraints or cautions for users of this function]

------------------------------------------------------------------------------
*/
void ec_gain_code_update(
    ec_gain_codeState *st,    /* i/o : State struct                     */
    Word16 bfi,               /* i   : flag: frame is bad               */
    Word16 prev_bf,           /* i   : flag: previous frame was bad     */
    Word16 *gain_code,        /* i/o : decoded innovation gain          */
    Flag   *pOverflow
)
{
    Word16 i;

    /* limit gain_code by previous good gain if previous frame was bad */
    if (bfi == 0)
    {
        if (prev_bf != 0)
        {
            if (sub(*gain_code, st->prev_gc, pOverflow) > 0)
            {
                *gain_code = st->prev_gc;
            }
        }
        st->prev_gc = *gain_code;
    }

    /* update EC states: previous gain, gain buffer */
    st->past_gain_code = *gain_code;

    for (i = 1; i < 5; i++)
    {
        st->gbuf[i - 1] = st->gbuf[i];
    }
    st->gbuf[4] = *gain_code;

    return;
}

/****************************************************************************/

/*
------------------------------------------------------------------------------
 FUNCTION NAME: ec_gain_pitch
------------------------------------------------------------------------------
 INPUT AND OUTPUT DEFINITIONS

 Inputs:
  st = pointer to a pointer to a structure containing code
       state data of stucture type ec_gain_pitchState
  state = state of the state machine of type Word16
  pOverflow = pointer to overflow indicator of type Flag

  Outputs:
  state = pointer to a pointer to a structure containing code
          state data of stucture type ec_gain_pitchState
  gain_pitch = pointer to pitch gain (Q14) of type Word16
  pOverflow = 1 if there is an overflow else it is zero.

 Returns:
    None.

 Global Variables Used:
    None.

 Local Variables Needed:
    None.

------------------------------------------------------------------------------
 FUNCTION DESCRIPTION

 This function conceals the error using code gain implementation in this
 function.

------------------------------------------------------------------------------
 REQUIREMENTS

 None.

------------------------------------------------------------------------------
 REFERENCES

 ec_gain.c, UMTS GSM AMR speech codec, R99 - Version 3.2.0, March 2, 2001

------------------------------------------------------------------------------
 PSEUDO-CODE


    static const Word16 pdown[7] =
    {
        32767, 32112, 32112, 26214,
        9830, 6553, 6553
    };

    Word16 tmp;

    // calculate median of last five gains
    tmp = gmed_n (st->pbuf, 5);

    // new gain = minimum(median, past_gain) * pdown[state]
    if (sub (tmp, st->past_gain_pit) > 0)
    {
        tmp = st->past_gain_pit;
    }
    *gain_pitch = mult (tmp, pdown[state]);


------------------------------------------------------------------------------
 CAUTION [optional]
 [State any special notes, constraints or cautions for users of this function]

------------------------------------------------------------------------------
*/
void ec_gain_pitch(
    ec_gain_pitchState *st, /* i/o : state variables                   */
    Word16 state,           /* i   : state of the state machine        */
    Word16 *gain_pitch,     /* o   : pitch gain (Q14)                  */
    Flag   *pOverflow
)
{
    static const Word16 pdown[7] =
    {
        32767, 32112, 32112, 26214,
        9830, 6553, 6553
    };

    Word16 tmp;

    /* calculate median of last five gains */
    tmp = gmed_n(st->pbuf, 5);

    /* new gain = minimum(median, past_gain) * pdown[state] */
    if (sub(tmp, st->past_gain_pit, pOverflow) > 0)
    {
        tmp = st->past_gain_pit;
    }
    *gain_pitch = mult(tmp, pdown[state], pOverflow);
}

/****************************************************************************/
/*
------------------------------------------------------------------------------
 FUNCTION NAME: ec_gain_pitch_reset
------------------------------------------------------------------------------
 INPUT AND OUTPUT DEFINITIONS

 Inputs:
  state = state of the state machine of type Word16
  pOverflow = pointer to overflow indicator of type Flag

  Outputs:
  state = pointer to a pointer to a structure containing code
          state data of stucture type ec_gain_pitchState
  pOverflow = 1 if there is an overflow else it is zero.

 Returns:
    None.

 Global Variables Used:
    None.

 Local Variables Needed:
    None.

------------------------------------------------------------------------------
 FUNCTION DESCRIPTION

 Function:   ec_gain_pitch_reset
 Purpose:    Resets state memory

------------------------------------------------------------------------------
 REQUIREMENTS

 None.

------------------------------------------------------------------------------
 REFERENCES

 ec_gain.c, UMTS GSM AMR speech codec, R99 - Version 3.2.0, March 2, 2001

------------------------------------------------------------------------------
 PSEUDO-CODE

int ec_gain_pitch_reset (ec_gain_pitchState *state)
{
  Word16 i;

  if (state == (ec_gain_pitchState *) NULL){
      // fprintf(stderr, "ec_gain_pitch_reset: invalid parameter\n");
      return -1;
  }

  for(i = 0; i < 5; i++)
      state->pbuf[i] = 1640;
  state->past_gain_pit = 0;
  state->prev_gp = 16384;

  return 0;
}

------------------------------------------------------------------------------
 CAUTION [optional]
 [State any special notes, constraints or cautions for users of this function]

------------------------------------------------------------------------------
*/
Word16 ec_gain_pitch_reset(ec_gain_pitchState *state)
{
    Word16 i;

    if (state == (ec_gain_pitchState *) NULL)
    {
        /* fprintf(stderr, "ec_gain_pitch_reset: invalid parameter\n"); */
        return -1;
    }

    for (i = 0; i < 5; i++)
        state->pbuf[i] = 1640;
    state->past_gain_pit = 0;
    state->prev_gp = 16384;

    return 0;
}

/****************************************************************************/


/*
------------------------------------------------------------------------------
 FUNCTION NAME: ec_gain_pitch_update
------------------------------------------------------------------------------
 INPUT AND OUTPUT DEFINITIONS

 Inputs:
  st = pointer to a pointer to a structure containing code
       state data of stucture type ec_gain_pitchState
  bfi = flag indicating the frame is bad of type Word16
  prev_bf = flag indicating the previous frame was bad of type Word16
  gain_pitch = pointer to pitch gain of type Word16
  pOverflow = pointer to overflow indicator of type Flag

  Outputs:
  state = pointer to a pointer to a structure containing code
          state data of stucture type ec_gain_pitchState
  gain_pitch = pointer to pitch gain of type Word16
  pOverflow = 1 if there is an overflow else it is zero.

 Returns:
    None.

 Global Variables Used:
    None.

 Local Variables Needed:
    None.

------------------------------------------------------------------------------
 FUNCTION DESCRIPTION

  Purpose     : update the pitch gain concealment state;
                limit gain_pitch if the previous frame was bad
                Call this function always after decoding (or concealing)
                the gain

------------------------------------------------------------------------------
 REQUIREMENTS

 None.

------------------------------------------------------------------------------
 REFERENCES

 ec_gain.c, UMTS GSM AMR speech codec, R99 - Version 3.2.0, March 2, 2001

------------------------------------------------------------------------------
 PSEUDO-CODE

    Word16 i;

    if (bfi == 0)
    {
        if (prev_bf != 0)
        {
            if (sub (*gain_pitch, st->prev_gp) > 0)
            {
                *gain_pitch = st->prev_gp;
            }
        }
        st->prev_gp = *gain_pitch;
    }

    st->past_gain_pit = *gain_pitch;

    if (sub (st->past_gain_pit, 16384) > 0)  // if (st->past_gain_pit > 1.0)
    {
        st->past_gain_pit = 16384;
    }
    for (i = 1; i < 5; i++)
    {
        st->pbuf[i - 1] = st->pbuf[i];
    }
    st->pbuf[4] = st->past_gain_pit;


------------------------------------------------------------------------------
 CAUTION [optional]
 [State any special notes, constraints or cautions for users of this function]

------------------------------------------------------------------------------
*/
void ec_gain_pitch_update(
    ec_gain_pitchState *st, /* i/o : state variables                   */
    Word16 bfi,             /* i   : flag: frame is bad                */
    Word16 prev_bf,         /* i   : flag: previous frame was bad      */
    Word16 *gain_pitch,     /* i/o : pitch gain                        */
    Flag   *pOverflow
)
{
    Word16 i;

    if (bfi == 0)
    {
        if (prev_bf != 0)
        {
            if (sub(*gain_pitch, st->prev_gp, pOverflow) > 0)
            {
                *gain_pitch = st->prev_gp;
            }
        }
        st->prev_gp = *gain_pitch;
    }

    st->past_gain_pit = *gain_pitch;

    if (sub(st->past_gain_pit, 16384, pOverflow) > 0)
        /* if (st->past_gain_pit > 1.0) */
    {
        st->past_gain_pit = 16384;
    }
    for (i = 1; i < 5; i++)
    {
        st->pbuf[i - 1] = st->pbuf[i];
    }
    st->pbuf[4] = st->past_gain_pit;
}


