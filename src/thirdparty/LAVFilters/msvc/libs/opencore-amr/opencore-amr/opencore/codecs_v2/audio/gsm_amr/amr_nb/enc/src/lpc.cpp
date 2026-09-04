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



 Filename: lpc.cpp

------------------------------------------------------------------------------
*/

/*----------------------------------------------------------------------------
; INCLUDES
----------------------------------------------------------------------------*/
#include "lpc.h"
#include "typedef.h"
#include "oper_32b.h"
#include "autocorr.h"
#include "lag_wind.h"
#include "levinson.h"
#include "cnst.h"
#include "mode.h"
#include "sub.h"
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
; LOCAL STORE/BUFFER/POINTER DEFINITIONS
; Variable declaration - defined here and used outside this module
----------------------------------------------------------------------------*/

/*
------------------------------------------------------------------------------
 FUNCTION NAME: lpc_init
------------------------------------------------------------------------------
 INPUT AND OUTPUT DEFINITIONS

 Inputs:
    state = pointer to pointer of state data of type lpcState

 Outputs:
    None

 Returns:
    None

 Global Variables Used:
    None.

 Local Variables Needed:
    None.

------------------------------------------------------------------------------
 FUNCTION DESCRIPTION

 This function initializes the state data for the LPC module.

------------------------------------------------------------------------------
 REQUIREMENTS

 None.

------------------------------------------------------------------------------
 REFERENCES

 lpc.c, UMTS GSM AMR speech codec, R99 - Version 3.2.0, March 2, 2001

------------------------------------------------------------------------------
 PSEUDO-CODE


  lpcState* s;

  if (state == (lpcState **) NULL){
      // fprintf(stderr, "lpc_init: invalid parameter\n");
      return -1;
  }
  *state = NULL;

  // allocate memory
  if ((s= (lpcState *) malloc(sizeof(lpcState))) == NULL){
      // fprintf(stderr, "lpc_init: can not malloc state structure\n");
      return -1;
  }

  s->levinsonSt = NULL;

  // Init sub states
  if (Levinson_init(&s->levinsonSt)) {
     lpc_exit(&s);
     return -1;
  }


  lpc_reset(s);
  *state = s;

  return 0;

------------------------------------------------------------------------------
 CAUTION [optional]
 [State any special notes, constraints or cautions for users of this function]

------------------------------------------------------------------------------
*/
Word16 lpc_init(lpcState **state)
{
    lpcState* s;

    if (state == (lpcState **) NULL)
    {
        /* fprintf(stderr, "lpc_init: invalid parameter\n"); */
        return -1;
    }
    *state = NULL;

    /* allocate memory */
    if ((s = (lpcState *) oscl_malloc(sizeof(lpcState))) == NULL)
    {
        /* fprintf(stderr, "lpc_init: can not malloc state structure\n"); */
        return -1;
    }

    s->levinsonSt = NULL;

    /* Init sub states */
    if (Levinson_init(&s->levinsonSt))
    {
        lpc_exit(&s);
        return -1;
    }

    lpc_reset(s);
    *state = s;

    return 0;
}

/*
------------------------------------------------------------------------------
 FUNCTION NAME: lpc_reset
------------------------------------------------------------------------------
 INPUT AND OUTPUT DEFINITIONS

 Inputs:
    state = pointer to pointer of state data of type lpcState

 Outputs:
    None

 Returns:
    None

 Global Variables Used:
    None.

 Local Variables Needed:
    None.

------------------------------------------------------------------------------
 FUNCTION DESCRIPTION

 This function resets the state data for the LPC module.

------------------------------------------------------------------------------
 REQUIREMENTS

 None.

------------------------------------------------------------------------------
 REFERENCES

 lpc.c, UMTS GSM AMR speech codec, R99 - Version 3.2.0, March 2, 2001

------------------------------------------------------------------------------
 PSEUDO-CODE

  if (state == (lpcState *) NULL){
      // fprintf(stderr, "lpc_reset: invalid parameter\n");
      return -1;
  }

  Levinson_reset(state->levinsonSt);

  return 0;

------------------------------------------------------------------------------
 CAUTION [optional]
 [State any special notes, constraints or cautions for users of this function]

------------------------------------------------------------------------------
*/
Word16 lpc_reset(lpcState *state)
{

    if (state == (lpcState *) NULL)
    {
        /* fprintf(stderr, "lpc_reset: invalid parameter\n"); */
        return -1;
    }

    Levinson_reset(state->levinsonSt);

    return 0;
}

/*
------------------------------------------------------------------------------
 FUNCTION NAME: lpc_exit
------------------------------------------------------------------------------
 INPUT AND OUTPUT DEFINITIONS

 Inputs:
    state = pointer to pointer of state data of type lpcState

 Outputs:
    None

 Returns:
    None

 Global Variables Used:
    None.

 Local Variables Needed:
    None.

------------------------------------------------------------------------------
 FUNCTION DESCRIPTION

 This function frees the state data for the LPC module.

------------------------------------------------------------------------------
 REQUIREMENTS

 None.

------------------------------------------------------------------------------
 REFERENCES

 lpc.c, UMTS GSM AMR speech codec, R99 - Version 3.2.0, March 2, 2001

------------------------------------------------------------------------------
 PSEUDO-CODE


  if (state == NULL || *state == NULL)
      return;

  Levinson_exit(&(*state)->levinsonSt);

  // deallocate memory
  free(*state);
  *state = NULL;

  return;


------------------------------------------------------------------------------
 CAUTION [optional]
 [State any special notes, constraints or cautions for users of this function]

------------------------------------------------------------------------------
*/
void lpc_exit(lpcState **state)
{
    if (state == NULL || *state == NULL)
        return;

    Levinson_exit(&(*state)->levinsonSt);

    /* deallocate memory */
    oscl_free(*state);
    *state = NULL;

    return;
}


/*
------------------------------------------------------------------------------
 FUNCTION NAME: lpc
------------------------------------------------------------------------------
 INPUT AND OUTPUT DEFINITIONS

 Inputs:
    state = pointer to state data of type lpcState
    mode  = coder mode of type enum Mode
    x[]   = pointer to input signal (Q15) of type Word16
    x_12k2[] = pointer to input signal (EFR) (Q15) of type Word16
    pOverflow = pointer to overflow indicator of type Flag

 Outputs:
    a[]   = pointer to predictor coefficients (Q12) of type Word16

 Returns:
    None

 Global Variables Used:
    None.

 Local Variables Needed:
    None.

------------------------------------------------------------------------------
 FUNCTION DESCRIPTION

 This function executes the LPC functionality for GSM AMR.

------------------------------------------------------------------------------
 REQUIREMENTS

 None.

------------------------------------------------------------------------------
 REFERENCES

 lpc.c, UMTS GSM AMR speech codec, R99 - Version 3.2.0, March 2, 2001

------------------------------------------------------------------------------
 PSEUDO-CODE

   Word16 rc[4];                  // First 4 reflection coefficients Q15
   Word16 rLow[MP1], rHigh[MP1];  // Autocorrelations low and hi
                                  // No fixed Q value but normalized
                                  // so that overflow is avoided

   if ( sub ((Word16)mode, (Word16)MR122) == 0)
   {
       // Autocorrelations
       Autocorr(x_12k2, M, rHigh, rLow, window_160_80);
       // Lag windowing
       Lag_window(M, rHigh, rLow);
       // Levinson Durbin
       Levinson(st->levinsonSt, rHigh, rLow, &a[MP1], rc);

       // Autocorrelations
       Autocorr(x_12k2, M, rHigh, rLow, window_232_8);
       // Lag windowing
       Lag_window(M, rHigh, rLow);
       // Levinson Durbin
       Levinson(st->levinsonSt, rHigh, rLow, &a[MP1 * 3], rc);
   }
   else
   {
       // Autocorrelations
       Autocorr(x, M, rHigh, rLow, window_200_40);
       // Lag windowing
       Lag_window(M, rHigh, rLow);
       // Levinson Durbin
       Levinson(st->levinsonSt, rHigh, rLow, &a[MP1 * 3], rc);
   }

   return 0;

------------------------------------------------------------------------------
 CAUTION [optional]
 [State any special notes, constraints or cautions for users of this function]

------------------------------------------------------------------------------
*/
void lpc(
    lpcState *st,     /* i/o: State struct                */
    enum Mode mode,   /* i  : coder mode                  */
    Word16 x[],       /* i  : Input signal           Q15  */
    Word16 x_12k2[],  /* i  : Input signal (EFR)     Q15  */
    Word16 a[],       /* o  : predictor coefficients Q12  */
    CommonAmrTbls* common_amr_tbls, /* i : ptr to struct with table ptrs */
    Flag   *pOverflow
)
{
    Word16 rc[4];                  /* First 4 reflection coefficients Q15 */
    Word16 rLow[MP1], rHigh[MP1];  /* Autocorrelations low and hi      */
    /* No fixed Q value but normalized  */
    /* so that overflow is avoided      */

    const Word16* window_160_80_ptr = common_amr_tbls->window_160_80_ptr;
    const Word16* window_232_8_ptr = common_amr_tbls->window_232_8_ptr;
    const Word16* window_200_40_ptr = common_amr_tbls->window_200_40_ptr;

    if (mode == MR122)
    {
        /* Autocorrelations */
        Autocorr(x_12k2, M, rHigh, rLow, window_160_80_ptr, pOverflow);
        /* Lag windowing    */
        Lag_window(M, rHigh, rLow, pOverflow);
        /* Levinson Durbin  */
        Levinson(st->levinsonSt, rHigh, rLow, &a[MP1], rc, pOverflow);

        /* Autocorrelations */
        Autocorr(x_12k2, M, rHigh, rLow, window_232_8_ptr, pOverflow);
        /* Lag windowing    */
        Lag_window(M, rHigh, rLow, pOverflow);
        /* Levinson Durbin  */
        Levinson(st->levinsonSt, rHigh, rLow, &a[MP1 * 3], rc, pOverflow);
    }
    else
    {
        /* Autocorrelations */
        Autocorr(x, M, rHigh, rLow, window_200_40_ptr, pOverflow);
        /* Lag windowing    */
        Lag_window(M, rHigh, rLow, pOverflow);
        /* Levinson Durbin  */
        Levinson(st->levinsonSt, rHigh, rLow, &a[MP1 * 3], rc, pOverflow);
    }

}















