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



 Filename: p_ol_wgh.cpp
 Functions: p_ol_wgh_init
           p_ol_wgh_reset
           p_ol_wgh_exit
           Lag_max
           Pitch_ol_wgh

------------------------------------------------------------------------------
 MODULE DESCRIPTION

 The modules in this file compute the open loop pitch lag with weighting.
------------------------------------------------------------------------------
*/


/*----------------------------------------------------------------------------
; INCLUDES
----------------------------------------------------------------------------*/
#include "p_ol_wgh.h"
#include "typedef.h"
#include "cnst.h"
#include "basic_op.h"
#include "gmed_n.h"
#include "inv_sqrt.h"
#include "vad1.h"
#include "calc_cor.h"
#include "hp_max.h"
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
 FUNCTION NAME: p_ol_wgh_init
------------------------------------------------------------------------------
 INPUT AND OUTPUT DEFINITIONS

 Inputs
    state = pointer to a pointer of structure type pitchOLWghtState

 Outputs:
    None

 Returns:
    0 if the memory allocation is a success
    -1 if the memory allocation fails

 Global Variables Used:
    None.

 Local Variables Needed:
    None.

------------------------------------------------------------------------------
 FUNCTION DESCRIPTION

 This function allocates state memory and initializes state memory

------------------------------------------------------------------------------
 REQUIREMENTS

 None.

------------------------------------------------------------------------------
 REFERENCES

 p_ol_wgh.c, UMTS GSM AMR speech codec, R99 - Version 3.2.0, March 2, 2001

------------------------------------------------------------------------------
 PSEUDO-CODE

int p_ol_wgh_init (pitchOLWghtState **state)
{
    pitchOLWghtState* s;

    if (state == (pitchOLWghtState **) NULL){
        // fprintf(stderr, "p_ol_wgh_init: invalid parameter\n");
        return -1;
    }
    *state = NULL;

    // allocate memory
    if ((s= (pitchOLWghtState *) malloc(sizeof(pitchOLWghtState))) == NULL){
        // fprintf(stderr, "p_ol_wgh_init: can not malloc state structure\n");
        return -1;
    }

    p_ol_wgh_reset(s);

    *state = s;

    return 0;
}

------------------------------------------------------------------------------
 CAUTION [optional]
 [State any special notes, constraints or cautions for users of this function]

------------------------------------------------------------------------------
*/

Word16 p_ol_wgh_init(pitchOLWghtState **state)
{
    pitchOLWghtState* s;

    if (state == (pitchOLWghtState **) NULL)
    {
        /* fprintf(stderr, "p_ol_wgh_init: invalid parameter\n"); */
        return -1;
    }
    *state = NULL;

    /* allocate memory */
    if ((s = (pitchOLWghtState *) oscl_malloc(sizeof(pitchOLWghtState))) == NULL)
    {
        /* fprintf(stderr, "p_ol_wgh_init: can not malloc state structure\n"); */
        return -1;
    }

    p_ol_wgh_reset(s);

    *state = s;

    return 0;
}

/*----------------------------------------------------------------------------
; End Function: p_ol_wgh_init
----------------------------------------------------------------------------*/

/*
------------------------------------------------------------------------------
 FUNCTION NAME: p_ol_wgh_reset
------------------------------------------------------------------------------
 INPUT AND OUTPUT DEFINITIONS

 Inputs
    st = pointer to structure type pitchOLWghtState

 Outputs:
    None

 Returns:
    0 if the memory initialization is a success
    -1 if the memory initialization fails

 Global Variables Used:
    None.

 Local Variables Needed:
    None.

------------------------------------------------------------------------------
 FUNCTION DESCRIPTION

 This function initializes state memory to zero

------------------------------------------------------------------------------
 REQUIREMENTS

 None.

------------------------------------------------------------------------------
 REFERENCES

 p_ol_wgh.c, UMTS GSM AMR speech codec, R99 - Version 3.2.0, March 2, 2001

------------------------------------------------------------------------------
 PSEUDO-CODE

int p_ol_wgh_reset (pitchOLWghtState *st)
{
   if (st == (pitchOLWghtState *) NULL){
      // fprintf(stderr, "p_ol_wgh_reset: invalid parameter\n");
      return -1;
   }

   // Reset pitch search states
   st->old_T0_med = 40;
   st->ada_w = 0;
   st->wght_flg = 0;

   return 0;
}

------------------------------------------------------------------------------
 CAUTION [optional]
 [State any special notes, constraints or cautions for users of this function]

------------------------------------------------------------------------------
*/

Word16 p_ol_wgh_reset(pitchOLWghtState *st)
{
    if (st == (pitchOLWghtState *) NULL)
    {
        /* fprintf(stderr, "p_ol_wgh_reset: invalid parameter\n"); */
        return -1;
    }

    /* Reset pitch search states */
    st->old_T0_med = 40;
    st->ada_w = 0;
    st->wght_flg = 0;

    return 0;
}

/*----------------------------------------------------------------------------
; End Function: p_ol_wgh_reset
----------------------------------------------------------------------------*/

/*
------------------------------------------------------------------------------
 FUNCTION NAME: p_ol_wgh_exit
------------------------------------------------------------------------------
 INPUT AND OUTPUT DEFINITIONS

 Inputs
    st = pointer to a pointer of structure type pitchOLWghtState

 Outputs:
    None

 Returns:
    0 if the memory initialization is a success
    -1 if the memory initialization fails

 Global Variables Used:
    None.

 Local Variables Needed:
    None.

------------------------------------------------------------------------------
 FUNCTION DESCRIPTION

 This function frees the memory used for state memory

------------------------------------------------------------------------------
 REQUIREMENTS

 None.

------------------------------------------------------------------------------
 REFERENCES

 p_ol_wgh.c, UMTS GSM AMR speech codec, R99 - Version 3.2.0, March 2, 2001

------------------------------------------------------------------------------
 PSEUDO-CODE

void p_ol_wgh_exit (pitchOLWghtState **state)
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

void p_ol_wgh_exit(pitchOLWghtState **state)
{
    if (state == NULL || *state == NULL)
        return;

    /* deallocate memory */
    oscl_free(*state);
    *state = NULL;

    return;
}


/*----------------------------------------------------------------------------
; End Function: p_ol_wgh_exit
----------------------------------------------------------------------------*/
/*
------------------------------------------------------------------------------
 FUNCTION NAME: Lag_max
------------------------------------------------------------------------------
 INPUT AND OUTPUT DEFINITIONS

 Inputs:
    corr = pointer to buffer of correlation values (Word32)
    scal_sig = pointer to buffer of scaled signal values (Word16)
    scal_fac = scaled signal factor (Word16)
    scal_flag = EFR compatible scaling flag (Word16)
    L_frame = length of frame to compute pitch (Word16)
    lag_max = maximum lag (Word16)
    lag_min = minimum lag (Word16)
    cor_max = pointer to the normalized correlation of selected lag (Word16)
    rmax = pointer to max(<s[i]*s[j]>), (Word32)
    r0 = pointer to the residual energy (Word32)
    dtx  = dtx flag; equal to 1, if dtx is enabled, 0, otherwise (Flag)
    pOverflow = Pointer to overflow (Flag)

 Outputs:
    cor_max contains the newly calculated normalized correlation of the
      selected lag
    rmax contains the newly calculated max(<s[i]*s[j]>)
    r0 contains the newly calculated residual energy
    pOverflow -> 1 if the math functions called by this routine saturate.

 Returns:
    p_max = lag of the max correlation found (Word16)

 Global Variables Used:
    None.

 Local Variables Needed:
    None.

------------------------------------------------------------------------------
 FUNCTION DESCRIPTION

 This function finds the lag that has maximum correlation of scal_sig[] in a
 given delay range.
 The correlation is given by
    cor[t] = <scal_sig[n],scal_sig[n-t]>,  t=lag_min,...,lag_max
 The functions outputs the maximum correlation after normalization and the
 corresponding lag.

------------------------------------------------------------------------------
 REQUIREMENTS

 None.

------------------------------------------------------------------------------
 REFERENCES

 p_ol_wgh.c, UMTS GSM AMR speech codec, R99 - Version 3.2.0, March 2, 2001

------------------------------------------------------------------------------
 PSEUDO-CODE

static Word16 Lag_max ( // o : lag found
    vadState *vadSt,    // i/o : VAD state struct
    Word32 corr[],      // i   : correlation vector.
    Word16 scal_sig[],  // i : scaled signal.
    Word16 L_frame,     // i : length of frame to compute pitch
    Word16 lag_max,     // i : maximum lag
    Word16 lag_min,     // i : minimum lag
    Word16 old_lag,     // i : old open-loop lag
    Word16 *cor_max,    // o : normalized correlation of selected lag
    Word16 wght_flg,    // i : is weighting function used
    Word16 *gain_flg,   // o : open-loop flag
    Flag dtx            // i   : dtx flag; use dtx=1, do not use dtx=0
    )
{
    Word16 i, j;
    Word16 *p, *p1;
    Word32 max, t0;
    Word16 t0_h, t0_l;
    Word16 p_max;
    const Word16 *ww, *we;
    Word32 t1;

    ww = &corrweight[250];
    we = &corrweight[123 + lag_max - old_lag];

    max = MIN_32;
    p_max = lag_max;

    for (i = lag_max; i >= lag_min; i--)
    {
       t0 = corr[-i];

       // Weighting of the correlation function.
       L_Extract (corr[-i], &t0_h, &t0_l);
       t0 = Mpy_32_16 (t0_h, t0_l, *ww);
       ww--;
       if (wght_flg > 0) {
          // Weight the neighbourhood of the old lag
          L_Extract (t0, &t0_h, &t0_l);
          t0 = Mpy_32_16 (t0_h, t0_l, *we);
          we--;
       }

       if (L_sub (t0, max) >= 0)
       {
          max = t0;
          p_max = i;
       }
    }

    p  = &scal_sig[0];
    p1 = &scal_sig[-p_max];
    t0 = 0;
    t1 = 0;

    for (j = 0; j < L_frame; j++, p++, p1++)
    {
       t0 = L_mac (t0, *p, *p1);
       t1 = L_mac (t1, *p1, *p1);
    }

    if (dtx)
    {  // no test() call since this if is only in simulation env
#ifdef VAD2
       vadSt->L_Rmax = L_add(vadSt->L_Rmax, t0);   // Save max correlation
       vadSt->L_R0 =   L_add(vadSt->L_R0, t1);        // Save max energy
#else
       // update and detect tone
       vad_tone_detection_update (vadSt, 0);
       vad_tone_detection (vadSt, t0, t1);
#endif
    }

    // gain flag is set according to the open_loop gain
    // is t2/t1 > 0.4 ?
    *gain_flg = pv_round(L_msu(t0, pv_round(t1), 13107));

    *cor_max = 0;

    return (p_max);
}

------------------------------------------------------------------------------
 CAUTION [optional]
 [State any special notes, constraints or cautions for users of this function]

------------------------------------------------------------------------------
*/

static Word16 Lag_max(  /* o : lag found                               */
    vadState *vadSt,    /* i/o : VAD state struct                      */
    Word32 corr[],      /* i   : correlation vector.                   */
    Word16 scal_sig[],  /* i : scaled signal.                          */
    Word16 L_frame,     /* i : length of frame to compute pitch        */
    Word16 lag_max,     /* i : maximum lag                             */
    Word16 lag_min,     /* i : minimum lag                             */
    Word16 old_lag,     /* i : old open-loop lag                       */
    Word16 *cor_max,    /* o : normalized correlation of selected lag  */
    Word16 wght_flg,    /* i : is weighting function used              */
    Word16 *gain_flg,   /* o : open-loop flag                          */
    Flag dtx,           /* i : dtx flag; use dtx=1, do not use dtx=0   */
    Flag   *pOverflow   /* o : overflow flag                           */
)
{
    Word16 i;
    Word16 j;
    Word16 *p;
    Word16 *p1;
    Word32 max;
    Word32 t0;
    Word16 t0_h;
    Word16 t0_l;
    Word16 p_max;
    const Word16 *ww;
    const Word16 *we;
    Word32 t1;
    Word16 temp;

    ww = &corrweight[250];
    we = &corrweight[123 + lag_max - old_lag];

    max = MIN_32;
    p_max = lag_max;

    for (i = lag_max; i >= lag_min; i--)
    {
        t0 = corr[-i];

        /* Weighting of the correlation function.   */
        L_Extract(corr[-i], &t0_h, &t0_l, pOverflow);
        t0 = Mpy_32_16(t0_h, t0_l, *ww, pOverflow);
        ww--;
        if (wght_flg > 0)
        {
            /* Weight the neighbourhood of the old lag. */
            L_Extract(t0, &t0_h, &t0_l, pOverflow);
            t0 = Mpy_32_16(t0_h, t0_l, *we, pOverflow);
            we--;
        }

        /*       if (L_sub (t0, max) >= 0) */
        if (t0 >= max)
        {
            max = t0;
            p_max = i;
        }
    }
    p  = &scal_sig[0];
    p1 = &scal_sig[-p_max];
    t0 = 0;
    t1 = 0;

    for (j = 0; j < L_frame; j++, p++, p1++)
    {
        t0 = L_mac(t0, *p, *p1, pOverflow);
        t1 = L_mac(t1, *p1, *p1, pOverflow);
    }

    if (dtx)
    {  /* no test() call since this if is only in simulation env */
#ifdef VAD2
        /* Save max correlation */
        vadSt->L_Rmax = L_add(vadSt->L_Rmax, t0, pOverflow);
        /* Save max energy */
        vadSt->L_R0 =   L_add(vadSt->L_R0, t1, pOverflow);
#else
        /* update and detect tone */
        vad_tone_detection_update(vadSt, 0, pOverflow);
        vad_tone_detection(vadSt, t0, t1, pOverflow);
#endif
    }

    /* gain flag is set according to the open_loop gain */
    /* is t2/t1 > 0.4 ? */
    temp = pv_round(t1, pOverflow);
    t1 = L_msu(t0, temp, 13107, pOverflow);
    *gain_flg = pv_round(t1, pOverflow);

    *cor_max = 0;

    return (p_max);
}
/*----------------------------------------------------------------------------
; End Function: Lag_max
----------------------------------------------------------------------------*/

/*
------------------------------------------------------------------------------
 FUNCTION NAME: Pitch_ol_wgh
------------------------------------------------------------------------------
 INPUT AND OUTPUT DEFINITIONS

 Inputs:
    st = pointer to pitchOLWghtState structure
    vadSt = pointer to a vadState structure
    signal = pointer to buffer of signal used to compute the open loop
         pitch where signal[-pit_max] to signal[-1] should be known
    pit_min = 16 bit value specifies the minimum pitch lag
    pit_max = 16 bit value specifies the maximum pitch lag
    L_frame = 16 bit value specifies the length of frame to compute pitch
    old_lags = pointer to history with old stored Cl lags (Word16)
    ol_gain_flg = pointer to OL gain flag (Word16)
    idx = 16 bit value specifies the frame index
    dtx = Data of type 'Flag' used for dtx. Use dtx=1, do not use dtx=0
    pOverflow = pointer to Overflow indicator (Flag)
 Outputs
    st = The pitchOLWghtState may be modified
    vadSt = The vadSt state structure may be modified.
    pOverflow -> 1 if the math functions invoked by this routine saturate.

 Returns:
    p_max1 = 16 bit value representing the open loop pitch lag.

 Global Variables Used:
    None.

 Local Variables Needed:
    None.

------------------------------------------------------------------------------
 FUNCTION DESCRIPTION

 This function performs an open-loop pitch search with weighting
------------------------------------------------------------------------------
 REQUIREMENTS

 None.

------------------------------------------------------------------------------
 REFERENCES

 pitch_ol.c, UMTS GSM AMR speech codec, R99 - Version 3.2.0, March 2, 2001

------------------------------------------------------------------------------
 PSEUDO-CODE

Word16 Pitch_ol_wgh (     // o   : open loop pitch lag
    pitchOLWghtState *st, // i/o : State struct
    vadState *vadSt,      // i/o : VAD state struct/
    Word16 signal[],      // i   : signal used to compute the open loop pitch
                          //       signal[-pit_max] to signal[-1] should be known
    Word16 pit_min,       // i   : minimum pitch lag
    Word16 pit_max,       // i   : maximum pitch lag
    Word16 L_frame,       // i   : length of frame to compute pitch
    Word16 old_lags[],    // i   : history with old stored Cl lags
    Word16 ol_gain_flg[], // i   : OL gain flag
    Word16 idx,           // i   : index
    Flag dtx              // i   : dtx flag; use dtx=1, do not use dtx=0
    )
{
    Word16 i;
    Word16 max1;
    Word16 p_max1;
    Word32 t0;
#ifndef VAD2
    Word16 corr_hp_max;
#endif
    Word32 corr[PIT_MAX+1], *corr_ptr;

    // Scaled signal
    Word16 scaled_signal[PIT_MAX + L_FRAME];
    Word16 *scal_sig;

    scal_sig = &scaled_signal[pit_max];

    t0 = 0L;
    for (i = -pit_max; i < L_frame; i++)
    {
        t0 = L_mac (t0, signal[i], signal[i]);
    }
    //
    // Scaling of input signal
    //
    //   if Overflow        -> scal_sig[i] = signal[i]>>2
    //   else if t0 < 1^22  -> scal_sig[i] = signal[i]<<2
    //   else               -> scal_sig[i] = signal[i]

    //
    //  Verification for risk of overflow.
    //

    // Test for overflow
    if (L_sub (t0, MAX_32) == 0L)
    {
        for (i = -pit_max; i < L_frame; i++)
        {
            scal_sig[i] = shr (signal[i], 3);
        }
    }
    else if (L_sub (t0, (Word32) 1048576L) < (Word32) 0)
    {
        for (i = -pit_max; i < L_frame; i++)
        {
            scal_sig[i] = shl (signal[i], 3);
        }
    }
    else
    {
        for (i = -pit_max; i < L_frame; i++)
        {
            scal_sig[i] = signal[i];
        }
    }

    // calculate all coreelations of scal_sig, from pit_min to pit_max
    corr_ptr = &corr[pit_max];
    comp_corr (scal_sig, L_frame, pit_max, pit_min, corr_ptr);

    p_max1 = Lag_max (vadSt, corr_ptr, scal_sig, L_frame, pit_max, pit_min,
                      st->old_T0_med, &max1, st->wght_flg, &ol_gain_flg[idx],
                      dtx);

    if (ol_gain_flg[idx] > 0)
    {
       // Calculate 5-point median of previous lag
       for (i = 4; i > 0; i--) // Shift buffer
       {
          old_lags[i] = old_lags[i-1];
       }
       old_lags[0] = p_max1;
       st->old_T0_med = gmed_n (old_lags, 5);
       st->ada_w = 32767; // Q15 = 1.0
    }
    else
    {
       st->old_T0_med = p_max1;
       st->ada_w = mult(st->ada_w, 29491);      // = ada_w = ada_w * 0.9
    }

    if (sub(st->ada_w, 9830) < 0)  // ada_w - 0.3
    {
       st->wght_flg = 0;
    }
    else
    {
       st->wght_flg = 1;
    }

#ifndef VAD2
    if (dtx)
    {  // no test() call since this if is only in simulation env
       if (sub(idx, 1) == 0)
       {
          // calculate max high-passed filtered correlation of all lags
          hp_max (corr_ptr, scal_sig, L_frame, pit_max, pit_min, &corr_hp_max);

          // update complex background detector
          vad_complex_detection_update(vadSt, corr_hp_max);
       }
    }
#endif

    return (p_max1);
}

------------------------------------------------------------------------------
 CAUTION [optional]
 [State any special notes, constraints or cautions for users of this function]

------------------------------------------------------------------------------
*/

Word16 Pitch_ol_wgh(      /* o   : open loop pitch lag                            */
    pitchOLWghtState *st, /* i/o : State struct                                   */
    vadState *vadSt,      /* i/o : VAD state struct                               */
    Word16 signal[],      /* i   : signal used to compute the open loop pitch     */
    /*       signal[-pit_max] to signal[-1] should be known */
    Word16 pit_min,       /* i   : minimum pitch lag                              */
    Word16 pit_max,       /* i   : maximum pitch lag                              */
    Word16 L_frame,       /* i   : length of frame to compute pitch               */
    Word16 old_lags[],    /* i   : history with old stored Cl lags                */
    Word16 ol_gain_flg[], /* i   : OL gain flag                                   */
    Word16 idx,           /* i   : index                                          */
    Flag dtx,             /* i   : dtx flag; use dtx=1, do not use dtx=0          */
    Flag   *pOverflow     /* o   : overflow flag                                  */
)
{
    Word16 i;
    Word16 max1;
    Word16 p_max1;
    Word32 t0;
#ifndef VAD2
    Word16 corr_hp_max;
#endif
    Word32 corr[PIT_MAX+1], *corr_ptr;

    /* Scaled signal */
    Word16 scaled_signal[PIT_MAX + L_FRAME];
    Word16 *scal_sig;

    scal_sig = &scaled_signal[pit_max];

    t0 = 0L;
    for (i = -pit_max; i < L_frame; i++)
    {
        t0 = L_mac(t0, signal[i], signal[i], pOverflow);
    }
    /*--------------------------------------------------------*
     * Scaling of input signal.                               *
     *                                                        *
     *   if Overflow        -> scal_sig[i] = signal[i]>>2     *
     *   else if t0 < 1^22  -> scal_sig[i] = signal[i]<<2     *
     *   else               -> scal_sig[i] = signal[i]        *
     *--------------------------------------------------------*/

    /*--------------------------------------------------------*
     *  Verification for risk of overflow.                    *
     *--------------------------------------------------------*/

    /* Test for overflow */
    if (L_sub(t0, MAX_32, pOverflow) == 0L)
    {
        for (i = -pit_max; i < L_frame; i++)
        {
            scal_sig[i] = shr(signal[i], 3, pOverflow);
        }
    }
    else if (L_sub(t0, (Word32) 1048576L, pOverflow) < (Word32) 0)
    {
        for (i = -pit_max; i < L_frame; i++)
        {
            scal_sig[i] = shl(signal[i], 3, pOverflow);
        }
    }
    else
    {
        for (i = -pit_max; i < L_frame; i++)
        {
            scal_sig[i] = signal[i];
        }
    }

    /* calculate all coreelations of scal_sig, from pit_min to pit_max */
    corr_ptr = &corr[pit_max];
    comp_corr(scal_sig, L_frame, pit_max, pit_min, corr_ptr);

    p_max1 = Lag_max(vadSt, corr_ptr, scal_sig, L_frame, pit_max, pit_min,
                     st->old_T0_med, &max1, st->wght_flg, &ol_gain_flg[idx],
                     dtx, pOverflow);

    if (ol_gain_flg[idx] > 0)
    {
        /* Calculate 5-point median of previous lags */
        for (i = 4; i > 0; i--) /* Shift buffer */
        {
            old_lags[i] = old_lags[i-1];
        }
        old_lags[0] = p_max1;
        st->old_T0_med = gmed_n(old_lags, 5);
        st->ada_w = 32767; /* Q15 = 1.0 */
    }
    else
    {
        st->old_T0_med = p_max1;
        /* = ada_w = ada_w * 0.9 */
        st->ada_w = (Word16)((Word32)(st->ada_w * 29491) >> 15);
    }

    if (sub(st->ada_w, 9830, pOverflow) < 0)  /* ada_w - 0.3 */
    {
        st->wght_flg = 0;
    }
    else
    {
        st->wght_flg = 1;
    }

#ifndef VAD2
    if (dtx)
    {  /* no test() call since this if is only in simulation env */
        if (sub(idx, 1, pOverflow) == 0)
        {
            /* calculate max high-passed filtered correlation of all lags */
            hp_max(corr_ptr, scal_sig, L_frame, pit_max, pit_min, &corr_hp_max, pOverflow);

            /* update complex background detector */
            vad_complex_detection_update(vadSt, corr_hp_max);
        }
    }
#endif

    return (p_max1);
}

/*----------------------------------------------------------------------------
; End Function: Pitch_ol_wgh
----------------------------------------------------------------------------*/





