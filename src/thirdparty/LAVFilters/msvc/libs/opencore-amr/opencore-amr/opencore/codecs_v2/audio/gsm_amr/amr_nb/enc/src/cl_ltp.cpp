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



 Filename: cl_ltp.cpp
 Funtions: cl_ltp_init
           cl_ltp_reset
           cl_ltp_exit
           cl_ltp

------------------------------------------------------------------------------
 MODULE DESCRIPTION

 This file contains functions that perform closed-loop fractional pitch
 search.

------------------------------------------------------------------------------
*/


/*----------------------------------------------------------------------------
; INCLUDES
----------------------------------------------------------------------------*/
#include "cl_ltp.h"
#include "basicop_malloc.h"
#include "cnst.h"
#include "convolve.h"
#include "g_pitch.h"
#include "pred_lt.h"
#include "pitch_fr.h"
#include "enc_lag3.h"
#include "enc_lag6.h"
#include "q_gain_p.h"
#include "ton_stab.h"
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
 FUNCTION NAME: cl_ltp_init
------------------------------------------------------------------------------
 INPUT AND OUTPUT DEFINITIONS

 Inputs:
    state = Pointer to a pointer to a clLtpState structure

 Outputs:
    state points to the newly created clLtpState structure.

 Returns:
    This function returns 0 upon success and -1 upon failure.

 Global Variables Used:
    None

 Local Variables Needed:
    None

------------------------------------------------------------------------------
 FUNCTION DESCRIPTION

 Allocates state memory and initializes state memory

------------------------------------------------------------------------------
 REQUIREMENTS

 None.

------------------------------------------------------------------------------
 REFERENCES

 cl_ltp.c, UMTS GSM AMR speech codec, R99 - Version 3.2.0, March 2, 2001

------------------------------------------------------------------------------
 PSEUDO-CODE

int cl_ltp_init (clLtpState **state)
{
    clLtpState* s;

    if (state == (clLtpState **) NULL){
        fprintf(stderr, "cl_ltp_init: invalid parameter\n");
        return -1;
    }
    *state = NULL;

    // allocate memory
    if ((s= (clLtpState *) malloc(sizeof(clLtpState))) == NULL){
        fprintf(stderr, "cl_ltp_init: can not malloc state structure\n");
        return -1;
  }

    // init the sub state
    if (Pitch_fr_init(&s->pitchSt)) {
        cl_ltp_exit(&s);
        return -1;
    }

    cl_ltp_reset(s);

    *state = s;

    return 0;
}


------------------------------------------------------------------------------
 CAUTION [optional]
 [State any special notes, constraints or cautions for users of this function]

------------------------------------------------------------------------------
*/

Word16 cl_ltp_init(clLtpState **state)
{
    clLtpState* s;

    if (state == (clLtpState **) NULL)
    {
        /*fprint(stderr, "cl_ltp_init: invalid parameter\n");*/
        return(-1);
    }
    *state = NULL;

    /* allocate memory */
    if ((s = (clLtpState *) oscl_malloc(sizeof(clLtpState))) == NULL)
    {
        /*fprint(stderr, "cl_ltp_init: can not malloc state structure\n");*/
        return(-1);
    }

    /* init the sub state */
    if (Pitch_fr_init(&s->pitchSt))
    {
        cl_ltp_exit(&s);
        return(-1);
    }

    cl_ltp_reset(s);

    *state = s;

    return(0);
}

/****************************************************************************/

/*
------------------------------------------------------------------------------
 FUNCTION NAME: cl_ltp_reset
------------------------------------------------------------------------------
 INPUT AND OUTPUT DEFINITIONS

 Inputs:
    state = pointer to the clLtpState structure to be reset

 Outputs:
    The state structure pointed to by clLtpState *state is reset.

 Returns:
    The function returns int 0 if successful, -1 otherwise.

 Global Variables Used:
    None

 Local Variables Needed:
    None

------------------------------------------------------------------------------
 FUNCTION DESCRIPTION

 Initializes state memory to zero.

------------------------------------------------------------------------------
 REQUIREMENTS

------------------------------------------------------------------------------
 REFERENCES

 cl_ltp.c, UMTS GSM AMR speech codec, R99 - Version 3.2.0, March 2, 2001

 ------------------------------------------------------------------------------
 PSEUDO-CODE

int cl_ltp_reset (clLtpState *state)
{
    if (state == (clLtpState *) NULL){
        fprintf(stderr, "cl_ltp_reset: invalid parameter\n");
        return -1;
    }

    // Reset pitch search states
    Pitch_fr_reset (state->pitchSt);

    return 0;
}

------------------------------------------------------------------------------
 CAUTION [optional]
 [State any special notes, constraints or cautions for users of this function]

------------------------------------------------------------------------------
*/

Word16 cl_ltp_reset(clLtpState *state)
{
    if (state == (clLtpState *) NULL)
    {
        /*fprint(stderr, "cl_ltp_reset: invalid parameter\n");  */
        return(-1);
    }

    /* Reset pitch search states */
    Pitch_fr_reset(state->pitchSt);

    return(0);
}

/****************************************************************************/

/*
------------------------------------------------------------------------------
 FUNCTION NAME: cl_ltp_exit
------------------------------------------------------------------------------
 INPUT AND OUTPUT DEFINITIONS

 Inputs:
    clLtpState **state = Reference to the state object to be freed.

 Outputs:
    The memory used by the structure which is pointed to by 'state'
      is freed.

 Returns:
    None

 Global Variables Used:
    None

 Local Variables Needed:
    None

------------------------------------------------------------------------------
 FUNCTION DESCRIPTION

 The memory used for state memory is freed


------------------------------------------------------------------------------
 REQUIREMENTS

 None

------------------------------------------------------------------------------
 REFERENCES

 cl_ltp.c, UMTS GSM AMR speech codec, R99 - Version 3.2.0, March 2, 2001

------------------------------------------------------------------------------
 PSEUDO-CODE

void cl_ltp_exit (clLtpState **state)
{
    if (state == NULL || *state == NULL)
        return;

    // dealloc members
    Pitch_fr_exit(&(*state)->pitchSt);

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

void cl_ltp_exit(clLtpState **state)
{
    if (state == NULL || *state == NULL)
    {
        return;
    }

    /* dealloc members */
    Pitch_fr_exit(&(*state)->pitchSt);

    /* deallocate memory */
    oscl_free(*state);
    *state = NULL;

    return;
}

/****************************************************************************/

/*
------------------------------------------------------------------------------
 FUNCTION NAME: cl_ltp
------------------------------------------------------------------------------
 INPUT AND OUTPUT DEFINITIONS

 Inputs:
    clSt = pointer to the clLtpState struct
    tonSt = pointer to the tonStabState structure
    mode = codec mode value, of type enum Mode
    frameOffset = offset to subframe (Word16)
    T_op = pointer to buffer of open loop pitch lags (Word16)
    h1 = pointer to impulse response vector (Word16)
    exc = pointer to excitation vector (Word16)
    res2 = pointer to long term prediction residual (Word16)
    xn = pointer to target vector for pitch search (Word16)
    lsp_flag = LSP resonance flag (Word16)

 Outputs:
    clSt = pointer to the clLtpState struct
    tonSt = pointer to the tonStabState structure
    exc = pointer to excitation vector (Word16)
    res2 = pointer to long term prediction residual (Word16)
    xn2 = pointer to target vector for codebook search (Word16)
    yl = pointer to buffer of filtered adaptive excitation (Word16)
    T0 = pointer to pitch delay (integer part) (Word16)
    T0_frac = pointer to pitch delay (fractional part) (Word16)
    gain_pit = pointer to pitch gain (Word16)
    g_coeff = pointer to array of correlations between xn, y1, & y2 (Word16)
    anap = pointer to pointer to analysis parameters (Word16)
    gp_limit = pointer to the pitch gain limit (Word16)
    pOverflow = pointer to overflow indicator (Flag)

 Returns:
    return_value = 0 (int)

 Global Variables Used:
    None

 Local Variables Needed:
    None

------------------------------------------------------------------------------
 FUNCTION DESCRIPTION

 This function performs closed-loop fractional pitch search.

------------------------------------------------------------------------------
 REQUIREMENTS

 None.

------------------------------------------------------------------------------
 REFERENCES

 cl_ltp.c, UMTS GSM AMR speech codec, R99 - Version 3.2.0, March 2, 2001

------------------------------------------------------------------------------
 PSEUDO-CODE FOR cl_ltp

int cl_ltp (
    clLtpState *clSt,    // i/o : State struct
    tonStabState *tonSt, // i/o : State struct
    enum Mode mode,      // i   : coder mode
    Word16 frameOffset,  // i   : Offset to subframe
    Word16 T_op[],       // i   : Open loop pitch lags
    Word16 *h1,          // i   : Impulse response vector               Q12
    Word16 *exc,         // i/o : Excitation vector                      Q0
    Word16 res2[],       // i/o : Long term prediction residual          Q0
    Word16 xn[],         // i   : Target vector for pitch search         Q0
    Word16 lsp_flag,     // i   : LSP resonance flag
    Word16 xn2[],        // o   : Target vector for codebook search      Q0
    Word16 y1[],         // o   : Filtered adaptive excitation           Q0
    Word16 *T0,          // o   : Pitch delay (integer part)
    Word16 *T0_frac,     // o   : Pitch delay (fractional part)
    Word16 *gain_pit,    // o   : Pitch gain                            Q14
    Word16 g_coeff[],    // o   : Correlations between xn, y1, & y2
    Word16 **anap,       // o   : Analysis parameters
    Word16 *gp_limit     // o   : pitch gain limit
)
{
    Word16 i;
    Word16 index;
    Word32 L_temp;     // temporarily variable
    Word16 resu3;      // flag for upsample resolution
    Word16 gpc_flag;

    *----------------------------------------------------------------------*
    *                 Closed-loop fractional pitch search                  *
    *----------------------------------------------------------------------*
   *T0 = Pitch_fr(clSt->pitchSt,
                  mode, T_op, exc, xn, h1,
                  L_SUBFR, frameOffset,
                  T0_frac, &resu3, &index);

   *(*anap)++ = index;

    *-----------------------------------------------------------------*
    *   - find unity gain pitch excitation (adapitve codebook entry)  *
    *     with fractional interpolation.                              *
    *   - find filtered pitch exc. y1[]=exc[] convolve with h1[])     *
    *   - compute pitch gain and limit between 0 and 1.2              *
    *   - update target vector for codebook search                    *
    *   - find LTP residual.                                          *
    *-----------------------------------------------------------------*

   Pred_lt_3or6(exc, *T0, *T0_frac, L_SUBFR, resu3);

   Convolve(exc, h1, y1, L_SUBFR);

   // gain_pit is Q14 for all modes
   *gain_pit = G_pitch(mode, xn, y1, g_coeff, L_SUBFR);


   // check if the pitch gain should be limit due to resonance in LPC filter
   gpc_flag = 0;
   *gp_limit = MAX_16;
   if ((lsp_flag != 0) &&
       (sub(*gain_pit, GP_CLIP) > 0))
   {
       gpc_flag = check_gp_clipping(tonSt, *gain_pit);
   }

   // special for the MR475, MR515 mode; limit the gain to 0.85 to
   // cope with bit errors in the decoder in a better way.
   if ((sub (mode, MR475) == 0) || (sub (mode, MR515) == 0)) {
      if ( sub (*gain_pit, 13926) > 0) {
         *gain_pit = 13926;   // 0.85 in Q14
      }

      if (gpc_flag != 0) {
          *gp_limit = GP_CLIP;
      }
   }
   else
   {
       if (gpc_flag != 0)
       {
           *gp_limit = GP_CLIP;
           *gain_pit = GP_CLIP;
       }
       // For MR122, gain_pit is quantized here and not in gainQuant
       if (sub(mode, MR122)==0)
       {
           *(*anap)++ = q_gain_pitch(MR122, *gp_limit, gain_pit,
                                     NULL, NULL);
       }
   }

   // update target vector und evaluate LTP residual
   for (i = 0; i < L_SUBFR; i++) {
       L_temp = L_mult(y1[i], *gain_pit);
       L_temp = L_shl(L_temp, 1);
       xn2[i] = sub(xn[i], extract_h(L_temp));

       L_temp = L_mult(exc[i], *gain_pit);
       L_temp = L_shl(L_temp, 1);
       res2[i] = sub(res2[i], extract_h(L_temp));
   }

   return 0;
}

------------------------------------------------------------------------------
 CAUTION [optional]
 [State any special notes, constraints or cautions for users of this function]

------------------------------------------------------------------------------
*/

void cl_ltp(
    clLtpState *clSt,    /* i/o : State struct                              */
    tonStabState *tonSt, /* i/o : State struct                              */
    enum Mode mode,      /* i   : coder mode                                */
    Word16 frameOffset,  /* i   : Offset to subframe                        */
    Word16 T_op[],       /* i   : Open loop pitch lags                      */
    Word16 *h1,          /* i   : Impulse response vector               Q12 */
    Word16 *exc,         /* i/o : Excitation vector                      Q0 */
    Word16 res2[],       /* i/o : Long term prediction residual          Q0 */
    Word16 xn[],         /* i   : Target vector for pitch search         Q0 */
    Word16 lsp_flag,     /* i   : LSP resonance flag                        */
    Word16 xn2[],        /* o   : Target vector for codebook search      Q0 */
    Word16 yl[],         /* o   : Filtered adaptive excitation           Q0 */
    Word16 *T0,          /* o   : Pitch delay (integer part)                */
    Word16 *T0_frac,     /* o   : Pitch delay (fractional part)             */
    Word16 *gain_pit,    /* o   : Pitch gain                            Q14 */
    Word16 g_coeff[],    /* o   : Correlations between xn, y1, & y2         */
    Word16 **anap,       /* o   : Analysis parameters                       */
    Word16 *gp_limit,    /* o   : pitch gain limit                          */
    const Word16* qua_gain_pitch_ptr, /* i : ptr to read-only table         */
    Flag   *pOverflow    /* o   : overflow indicator                        */
)
{
    register Word16 i;
    Word16 index;
    Word32 L_temp;     /* temporarily variable */
    Word16 resu3;      /* flag for upsample resolution */
    Word16 gpc_flag;

    Word16 temp;
    Word16 *p_exc;
    Word16 *p_xn;
    Word16 *p_xn2;
    Word16 *p_yl;

    /*----------------------------------------------------------------------*
     *                 Closed-loop fractional pitch search                  *
     *----------------------------------------------------------------------*/
    *T0 =
        Pitch_fr(
            clSt->pitchSt,
            mode,
            T_op,
            exc,
            xn,
            h1,
            L_SUBFR,
            frameOffset,
            T0_frac,
            &resu3,
            &index,
            pOverflow);

    *(*anap)++ = index;

    /*-----------------------------------------------------------------*
     *   - find unity gain pitch excitation (adapitve codebook entry)  *
     *     with fractional interpolation.                              *
     *   - find filtered pitch exc. y1[]=exc[] convolve with h1[])     *
     *   - compute pitch gain and limit between 0 and 1.2              *
     *   - update target vector for codebook search                    *
     *   - find LTP residual.                                          *
     *-----------------------------------------------------------------*/

    Pred_lt_3or6(
        exc,
        *T0,
        *T0_frac,
        L_SUBFR,
        resu3,
        pOverflow);

    Convolve(exc, h1, yl, L_SUBFR);

    /* gain_pit is Q14 for all modes */
    *gain_pit =
        G_pitch(
            mode,
            xn,
            yl,
            g_coeff,
            L_SUBFR,
            pOverflow);


    /* check if the pitch gain should be limit due to resonance in LPC filter */
    gpc_flag = 0;
    *gp_limit = MAX_16;

    if ((lsp_flag != 0) && ((Word32)(*gain_pit) > GP_CLIP))
    {
        gpc_flag = check_gp_clipping(tonSt, *gain_pit, pOverflow);
    }

    /* special for the MR475, MR515 mode; limit the gain to 0.85 to */
    /* cope with bit errors in the decoder in a better way.         */

    if ((mode == MR475) || (mode == MR515))
    {
        *gain_pit = ((Word32) * gain_pit > 13926) ? 13926 : *gain_pit;

        if (gpc_flag != 0)
        {
            *gp_limit = GP_CLIP;
        }
    }
    else
    {
        if (gpc_flag != 0)
        {
            *gp_limit = GP_CLIP;
            *gain_pit = GP_CLIP;
        }
        /* For MR122, gain_pit is quantized here and not in gainQuant */
        if (mode == MR122)
        {
            *(*anap)++ =
                q_gain_pitch(
                    MR122,
                    *gp_limit,
                    gain_pit,
                    NULL,
                    NULL,
                    qua_gain_pitch_ptr,
                    pOverflow);
        }
    }


    p_exc  = &exc[0];
    p_xn   =  &xn[0];
    p_xn2  = &xn2[0];
    p_yl   =  &yl[0];

    temp = *gain_pit;

    /* update target vector und evaluate LTP residual */
    for (i = 0; i < L_SUBFR; i++)
    {
        L_temp = ((Word32) * (p_yl++) * temp) >> 14;
        *(p_xn2++) = *(p_xn++) - (Word16)L_temp;

        L_temp   = ((Word32) * (p_exc++) * temp) >> 14;
        res2[i] -= (Word16)L_temp;
    }

}
