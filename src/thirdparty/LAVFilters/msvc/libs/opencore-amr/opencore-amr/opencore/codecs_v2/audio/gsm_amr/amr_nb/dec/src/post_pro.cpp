/* ------------------------------------------------------------------
 * Copyright (C) 1998-2010 PacketVideo
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



 Filename: post_pro.cpp
 Functions:
           Post_Process_reset
           Post_Process

------------------------------------------------------------------------------
 MODULE DESCRIPTION

 This file contains the function that performs post-processing on the output
 speech. Post-processing include filtering the output speech through a second
 order high pass filter with cutoff frequency of 60 Hz, and up-scaling the
 output speech by a factor of 2. In addition to the post-processing function
 itself, a post-processing initialization function, post-processing reset
 function, and post-processing exit function are also included in this file.

------------------------------------------------------------------------------
*/


/*----------------------------------------------------------------------------
; INCLUDES
----------------------------------------------------------------------------*/
#include "post_pro.h"
#include "typedef.h"
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

/* filter coefficients (fc = 60 Hz) */
static const Word16 b[3] = {7699, -15398, 7699};
static const Word16 a[3] = {8192, 15836, -7667};

/*
------------------------------------------------------------------------------
 FUNCTION NAME: Post_Process_reset
------------------------------------------------------------------------------
 INPUT AND OUTPUT DEFINITIONS

 Inputs:
    state = pointer to a structure of type Post_ProcessState

 Outputs:
    structure pointed to by state will have all its fields initialized
      to zero

 Returns:
    return_value = 0, if reset was successful; -1, otherwise (int)

 Global Variables Used:
    None

 Local Variables Needed:
    None

------------------------------------------------------------------------------
 FUNCTION DESCRIPTION

 This function initializes state memory to zero.

------------------------------------------------------------------------------
 REQUIREMENTS

 None

------------------------------------------------------------------------------
 REFERENCES

 post_pro.c, UMTS GSM AMR speech codec, R99 - Version 3.2.0, March 2, 2001

------------------------------------------------------------------------------
 PSEUDO-CODE

int Post_Process_reset (Post_ProcessState *state)
{
  if (state == (Post_ProcessState *) NULL){
      fprint(stderr, "Post_Process_reset: invalid parameter\n");
      return -1;
  }

  state->y2_hi = 0;
  state->y2_lo = 0;
  state->y1_hi = 0;
  state->y1_lo = 0;
  state->x0 = 0;
  state->x1 = 0;

  return 0;
}
------------------------------------------------------------------------------
 CAUTION [optional]
 [State any special notes, constraints or cautions for users of this function]

------------------------------------------------------------------------------
*/

Word16 Post_Process_reset(Post_ProcessState *state)
{
    if (state == (Post_ProcessState *) NULL)
    {
        /*  fprint(stderr, "Post_Process_reset: invalid parameter\n");  */
        return(-1);
    }

    state->y2_hi = 0;
    state->y2_lo = 0;
    state->y1_hi = 0;
    state->y1_lo = 0;
    state->x0 = 0;
    state->x1 = 0;

    return(0);
}

/****************************************************************************/

/*
------------------------------------------------------------------------------
 FUNCTION NAME: Post_Process
------------------------------------------------------------------------------
 INPUT AND OUTPUT DEFINITIONS

 Inputs:
    st = pointer to a structure of type Post_ProcessState
    signal = buffer containing the input signal (Word16)
    lg = length of the input signal (Word16)
    pOverflow = pointer to overflow indicator of type Flag

 Outputs:
    structure pointed to by st contains new filter input and output values
    signal buffer contains the HP filtered and up-scaled input signal
    pOverflow points to 1 if overflow occurs in the math functions called
              else it points to 0.

 Returns:
    return_value = 0 (int)

 Global Variables Used:
    a = buffer containing filter coefficients
    b = buffer containing filter coefficients

 Local Variables Needed:
    None

------------------------------------------------------------------------------
 FUNCTION DESCRIPTION

 This function performs post-processing on the output speech signal. First,
 the output speech goes through a second order high pass filter with a
 cutoff frequency of 60 Hz. Then, the filtered output speech is multiplied
 by a factor of 2. The algorithm implemented follows the following difference
 equation:

 y[i] = b[0]*x[i]*2 + b[1]*x[i-1]*2 + b[2]*x[i-2]*2 + a[1]*y[i-1] + a[2]*y[i-2];

------------------------------------------------------------------------------
 REQUIREMENTS

 None

------------------------------------------------------------------------------
 REFERENCES

 post_pro.c, UMTS GSM AMR speech codec, R99 - Version 3.2.0, March 2, 2001

------------------------------------------------------------------------------
 PSEUDO-CODE

int Post_Process (
    Post_ProcessState *st,  //i/o : post process state
    Word16 signal[],        //i/o : signal
    Word16 lg               //i   : length of signal
    )
{
    Word16 i, x2;
    Word32 L_tmp;

    for (i = 0; i < lg; i++)
    {
        x2 = st->x1;
        st->x1 = st->x0;
        st->x0 = signal[i];

        // y[i] = b[0]*x[i]*2 + b[1]*x[i-1]*2 + b140[2]*x[i-2]/2
        //                    + a[1]*y[i-1] + a[2] * y[i-2];

        L_tmp = Mpy_32_16 (st->y1_hi, st->y1_lo, a[1]);
        L_tmp = L_add (L_tmp, Mpy_32_16 (st->y2_hi, st->y2_lo, a[2]));
        L_tmp = L_mac (L_tmp, st->x0, b[0]);
        L_tmp = L_mac (L_tmp, st->x1, b[1]);
        L_tmp = L_mac (L_tmp, x2, b[2]);
        L_tmp = L_shl (L_tmp, 2);

        //Multiplication by two of output speech with saturation.
        signal[i] = pv_round(L_shl(L_tmp, 1));

        st->y2_hi = st->y1_hi;
        st->y2_lo = st->y1_lo;
        L_Extract (L_tmp, &st->y1_hi, &st->y1_lo);
    }

    return 0;
}

------------------------------------------------------------------------------
 CAUTION [optional]
 [State any special notes, constraints or cautions for users of this function]

------------------------------------------------------------------------------
*/

void Post_Process(
    Post_ProcessState *st,  /* i/o : post process state                   */
    Word16 signal[],        /* i/o : signal                               */
    Word16 lg,              /* i   : length of signal                     */
    Flag   *pOverflow
)
{
    Word16 i, x2;
    Word32 L_tmp;

    Word16 *p_signal;
    Word16 c_a1 = a[1];
    Word16 c_a2 = a[2];
    Word16 c_b0 = b[0];
    Word16 c_b1 = b[1];
    Word16 c_b2 = b[2];

    p_signal = &signal[0];

    for (i = 0; i < lg; i++)
    {
        x2 = st->x1;
        st->x1 = st->x0;
        st->x0 = *(p_signal);

        /*  y[i] = b[0]*x[i]*2 + b[1]*x[i-1]*2 + b140[2]*x[i-2]/2  */
        /*                     + a[1]*y[i-1] + a[2] * y[i-2];      */

        L_tmp = ((Word32) st->y1_hi) * c_a1;
        L_tmp += (((Word32) st->y1_lo) * c_a1) >> 15;
        L_tmp += ((Word32) st->y2_hi) * c_a2;
        L_tmp += (((Word32) st->y2_lo) * c_a2) >> 15;
        L_tmp += ((Word32) st->x0) * c_b0;
        L_tmp += ((Word32) st->x1) * c_b1;
        L_tmp += ((Word32) x2) * c_b2;

        L_tmp = L_shl(L_tmp, 3, pOverflow);


        /* Multiplication by two of output speech with saturation. */

        *(p_signal++) = pv_round(L_shl(L_tmp, 1, pOverflow), pOverflow);

        st->y2_hi = st->y1_hi;
        st->y2_lo = st->y1_lo;

        st->y1_hi = (Word16)(L_tmp >> 16);
        st->y1_lo = (Word16)((L_tmp >> 1) - ((Word32) st->y1_hi << 15));

    }

    return;
}
