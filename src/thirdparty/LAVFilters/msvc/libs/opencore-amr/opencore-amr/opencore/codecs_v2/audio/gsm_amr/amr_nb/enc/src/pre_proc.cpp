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



 Filename: pre_proc.cpp
 Funtions: Pre_Process_init
           Pre_Process_reset
           Pre_Process_exit
           Pre_Process

------------------------------------------------------------------------------
 MODULE DESCRIPTION

 These modules handle the preprocessing of input speech.

------------------------------------------------------------------------------
*/


/*----------------------------------------------------------------------------
; INCLUDES
----------------------------------------------------------------------------*/
#include "pre_proc.h"
#include "typedef.h"
#include "oscl_mem.h"

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
 FUNCTION NAME: Pre_Process_init
------------------------------------------------------------------------------
 INPUT AND OUTPUT DEFINITIONS

 Inputs:
    state = pointer to an array of pointer to structures of type
        Pre_ProcessState

 Outputs:
    Structure pointed to by the pointer pointed to by state is
      initialized to its reset value
    state points to the allocated memory

 Returns:
    return_value = 0 if memory was successfully initialized,
                   otherwise returns -1.

 Global Variables Used:
    None.

 Local Variables Needed:
    None.

------------------------------------------------------------------------------
 FUNCTION DESCRIPTION

 Allocates state memory and initializes state memory.

------------------------------------------------------------------------------
 REQUIREMENTS

 None.

------------------------------------------------------------------------------
 REFERENCES

 pre_proc.c, UMTS GSM AMR speech codec, R99 - Version 3.2.0, March 2, 2001

------------------------------------------------------------------------------
 PSEUDO-CODE

int Pre_Process_init (Pre_ProcessState **state)
{
  Pre_ProcessState* s;

  if (state == (Pre_ProcessState **) NULL){
      fprintf(stderr, "Pre_Process_init: invalid parameter\n");
      return -1;
  }
  *state = NULL;

  // allocate memory
  if ((s= (Pre_ProcessState *) malloc(sizeof(Pre_ProcessState))) == NULL){
      fprintf(stderr, "Pre_Process_init: can not malloc state structure\n");
      return -1;
  }

  Pre_Process_reset(s);
  *state = s;

  return 0;
}

------------------------------------------------------------------------------
 CAUTION [optional]
 [State any special notes, constraints or cautions for users of this function]

------------------------------------------------------------------------------
*/

Word16 Pre_Process_init(Pre_ProcessState **state)
{
    Pre_ProcessState* s;

    if (state == (Pre_ProcessState **) NULL)
    {
        /*  fprintf(stderr, "Pre_Process_init: invalid parameter\n");  */
        return(-1);
    }
    *state = NULL;

    /* allocate memory */
    if ((s = (Pre_ProcessState *) oscl_malloc(sizeof(Pre_ProcessState))) == NULL)
    {
        /*  fprintf(stderr, "Pre_Process_init:
            can not malloc state structure\n");  */
        return(-1);
    }

    Pre_Process_reset(s);
    *state = s;

    return(0);
}

/****************************************************************************/


/*
------------------------------------------------------------------------------
 FUNCTION NAME: Pre_Process_reset
------------------------------------------------------------------------------
 INPUT AND OUTPUT DEFINITIONS

 Inputs:
    state = pointer to structure of type Pre_ProcessState

 Outputs:
    Structure pointed to by state is initialized to zero.

 Returns:
    return_value = 0 if memory was successfully reset,
                   otherwise returns -1.

 Global Variables Used:
    None.

 Local Variables Needed:
    None.

------------------------------------------------------------------------------
 FUNCTION DESCRIPTION

 Initializes state memory to zero.

------------------------------------------------------------------------------
 REQUIREMENTS

 None.

------------------------------------------------------------------------------
 REFERENCES

 pre_proc.c, UMTS GSM AMR speech codec, R99 - Version 3.2.0, March 2, 2001

------------------------------------------------------------------------------
 PSEUDO-CODE

int Pre_Process_reset (Pre_ProcessState *state)
{
  if (state == (Pre_ProcessState *) NULL){
      fprintf(stderr, "Pre_Process_reset: invalid parameter\n");
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

Word16 Pre_Process_reset(Pre_ProcessState *state)
{
    if (state == (Pre_ProcessState *) NULL)
    {
        /*  fprintf(stderr, "Pre_Process_reset: invalid parameter\n");  */
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
 FUNCTION NAME: Pre_Process_exit
------------------------------------------------------------------------------
 INPUT AND OUTPUT DEFINITIONS

 Inputs:
    state = a pointer to an array of pointers to structures of
        type Pre_ProcessState

 Outputs:
    state points to a NULL address

 Returns:
    None.

 Global Variables Used:
    None.

 Local Variables Needed:
    None.

------------------------------------------------------------------------------
 FUNCTION DESCRIPTION

 The memory used for state memory is freed.

------------------------------------------------------------------------------
 REQUIREMENTS

 None.

------------------------------------------------------------------------------
 REFERENCES

 pre_proc.c, UMTS GSM AMR speech codec, R99 - Version 3.2.0, March 2, 2001

------------------------------------------------------------------------------
 PSEUDO-CODE

void Pre_Process_exit (Pre_ProcessState **state)
{
  if (state == NULL || *state == NULL)
      return;

  // deallocate memory
  free(*state);
  *state = NULL;

  return;
}

------------------------------------------------------------------------------
 CAUTION [optional]
 [State any special notes, constraints or cautions for users of this function]

------------------------------------------------------------------------------
*/

void Pre_Process_exit(Pre_ProcessState **state)
{
    if (state == NULL || *state == NULL)
    {
        return;
    }

    /* deallocate memory */
    oscl_free(*state);
    *state = NULL;

    return;
}

/****************************************************************************/

/*
------------------------------------------------------------------------------
 FUNCTION NAME: Pre_Process
------------------------------------------------------------------------------
 INPUT AND OUTPUT DEFINITIONS

 Inputs:
    st = a pointer to a structure of type Pre_ProcessState
    signal = input/output signal (Word16)
    lg = length of signal (Word16)

 Outputs:
    st points to the updated structure

 Returns:
    return_value = 0 (int)

 Global Variables Used:
    a = points to a buffer of filter coefficients
    b = points to a buffer of filter coefficients

 Local Variables Needed:
    None.

------------------------------------------------------------------------------
 FUNCTION DESCRIPTION

 This module performs the preprocessing of the input speech.
 The signal is passed through a 2nd order high pass filtering with cut off
 frequency at 80 Hz. The input is divided by two in the filtering process.

    y[i] = b[0]*x[i]/2 + b[1]*x[i-1]/2 + b[2]*x[i-2]/2
                     + a[1]*y[i-1] + a[2]*y[i-2];

------------------------------------------------------------------------------
 REQUIREMENTS

 None.

------------------------------------------------------------------------------
 REFERENCES

 pre_proc.c, UMTS GSM AMR speech codec, R99 - Version 3.2.0, March 2, 2001

------------------------------------------------------------------------------
 PSEUDO-CODE

int Pre_Process (
    Pre_ProcessState *st,
    Word16 signal[], // input/output signal
    Word16 lg)       // lenght of signal
{
    Word16 i, x2;
    Word32 L_tmp;

    for (i = 0; i < lg; i++)
    {
        x2 = st->x1;
        st->x1 = st->x0;
        st->x0 = signal[i];

        //  y[i] = b[0]*x[i]/2 + b[1]*x[i-1]/2 + b140[2]*x[i-2]/2
        //                     + a[1]*y[i-1] + a[2] * y[i-2];

        L_tmp = Mpy_32_16 (st->y1_hi, st->y1_lo, a[1]);
        L_tmp = L_add (L_tmp, Mpy_32_16 (st->y2_hi, st->y2_lo, a[2]));
        L_tmp = L_mac (L_tmp, st->x0, b[0]);
        L_tmp = L_mac (L_tmp, st->x1, b[1]);
        L_tmp = L_mac (L_tmp, x2, b[2]);
        L_tmp = L_shl (L_tmp, 3);
        signal[i] = pv_round (L_tmp);

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
/*
    filter coefficients (fc = 80 Hz, coeff. b[] is divided by 2)
    const Word16 b[3] = {1899, -3798, 1899};
    const Word16 a[3] = {4096, 7807, -3733};

*/

void Pre_Process(
    Pre_ProcessState *st,
    Word16 signal[], /* input/output signal */
    Word16 lg)       /* length of signal    */
{
    register Word16 i;
    Word16 x_n_2;
    Word16 x_n_1;
    Word32 L_tmp;
    Word16 *p_signal = signal;

    x_n_2 = st->x1;
    x_n_1 = st->x0;

    for (i = lg; i != 0; i--)
    {


        /*  y[i] = b[0]*x[i]/2 + b[1]*x[i-1]/2 + b140[2]*x[i-2]/2  */
        /*                     + a[1]*y[i-1] + a[2] * y[i-2];      */

        L_tmp     = ((Word32) st->y1_hi) * 7807;
        L_tmp    += (Word32)(((Word32) st->y1_lo * 7807) >> 15);

        L_tmp    += ((Word32) st->y2_hi) * (-3733);
        st->y2_hi =  st->y1_hi;
        L_tmp    += (Word32)(((Word32) st->y2_lo * (-3733)) >> 15);
        st->y2_lo =  st->y1_lo;

        L_tmp    += ((Word32) x_n_2) * 1899;
        x_n_2     =  x_n_1;
        L_tmp    += ((Word32) x_n_1) * (-3798);
        x_n_1     = *(p_signal);
        L_tmp    += ((Word32) x_n_1) * 1899;


        *(p_signal++) = (Word16)((L_tmp + 0x0000800L) >> 12);

        st->y1_hi = (Word16)(L_tmp >> 12);
        st->y1_lo = (Word16)((L_tmp << 3) - ((Word32)(st->y1_hi) << 15));

    }

    st->x1 = x_n_2;
    st->x0 = x_n_1;

    return;
}


