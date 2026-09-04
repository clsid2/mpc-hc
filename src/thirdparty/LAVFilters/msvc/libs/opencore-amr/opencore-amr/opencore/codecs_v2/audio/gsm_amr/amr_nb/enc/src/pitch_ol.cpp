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



 Filename: pitch_ol.cpp
 Funtions: Pitch_ol
           Lag_max

------------------------------------------------------------------------------
 MODULE DESCRIPTION

 The modules in this file compute the open loop pitch lag.
------------------------------------------------------------------------------
*/


/*----------------------------------------------------------------------------
; INCLUDES
----------------------------------------------------------------------------*/
#include "pitch_ol.h"
#include "typedef.h"
#include "basicop_malloc.h"
#include "cnst.h"
#include "inv_sqrt.h"
#include "vad.h"
#include "calc_cor.h"
#include "hp_max.h"
#include "oscl_mem.h"
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
#define THRESHOLD 27853

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
 FUNCTION NAME: Lag_max
------------------------------------------------------------------------------
 INPUT AND OUTPUT DEFINITIONS (If VAD2 is defined)

 Inputs
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

 Outputs:
    cor_max contains the newly calculated normalized correlation of the
      selected lag
    rmax contains the newly calculated max(<s[i]*s[j]>)
    r0 contains the newly calculated residual energy

 Returns:
    p_max = lag of the max correlation found (Word16)

 Global Variables Used:
    None.

 Local Variables Needed:
    None.

------------------------------------------------------------------------------
 INPUT AND OUTPUT DEFINITIONS (If VAD2 is not defined)

 Inputs
    vadSt = pointer to a vadState structure
    corr = pointer to buffer of correlation values (Word32)
    scal_sig = pointer to buffer of scaled signal values (Word16)
    scal_fac = scaled signal factor (Word16)
    scal_flag = EFR compatible scaling flag (Word16)
    L_frame = length of frame to compute pitch (Word16)
    lag_max = maximum lag (Word16)
    lag_min = minimum lag (Word16)
    cor_max = pointer to the normalized correlation of selected lag (Word16)
    dtx  = dtx flag; equal to 1, if dtx is enabled, 0, otherwise (Flag)
    pOverflow = pointer to overflow indicator (Flag)

 Outputs:
    cor_max contains the newly calculated normalized correlation of the
      selected lag
    vadSt contains the updated VAD state parameters
    pOverflow -> 1 if the math operations called by this routine saturate

 Returns:
    p_max = lag of the max correlation found (Word16)

 Global Variables Used:
    None.

 Local Variables Needed:
    None.

------------------------------------------------------------------------------
 FUNCTION DESCRIPTION

 Find the lag that has maximum correlation of scal_sig in a given delay range.
 The correlation is given by:

         cor[t] = <scal_sig[n],scal_sig[n-t]>,  t=lag_min,...,lag_max

 The function returns the maximum correlation after normalization and the
 corresponding lag.

------------------------------------------------------------------------------
 REQUIREMENTS

 None.

------------------------------------------------------------------------------
 REFERENCES

 pitch_ol.c, UMTS GSM AMR speech codec, R99 - Version 3.2.0, March 2, 2001

------------------------------------------------------------------------------
 PSEUDO-CODE

#ifdef VAD2
static Word16 Lag_max ( // o   : lag found
    Word32 corr[],      // i   : correlation vector.
    Word16 scal_sig[],  // i   : scaled signal.
    Word16 scal_fac,    // i   : scaled signal factor.
    Word16 scal_flag,   // i   : if 1 use EFR compatible scaling
    Word16 L_frame,     // i   : length of frame to compute pitch
    Word16 lag_max,     // i   : maximum lag
    Word16 lag_min,     // i   : minimum lag
    Word16 *cor_max,    // o   : normalized correlation of selected lag
    Word32 *rmax,       // o   : max(<s[i]*s[j]>)
    Word32 *r0,         // o   : residual energy
    Flag dtx            // i   : dtx flag; use dtx=1, do not use dtx=0
    )
#else
static Word16 Lag_max ( // o   : lag found
    vadState *vadSt,    // i/o : VAD state struct
    Word32 corr[],      // i   : correlation vector.
    Word16 scal_sig[],  // i   : scaled signal.
    Word16 scal_fac,    // i   : scaled signal factor.
    Word16 scal_flag,   // i   : if 1 use EFR compatible scaling
    Word16 L_frame,     // i   : length of frame to compute pitch
    Word16 lag_max,     // i   : maximum lag
    Word16 lag_min,     // i   : minimum lag
    Word16 *cor_max,    // o   : normalized correlation of selected lag
    Flag dtx            // i   : dtx flag; use dtx=1, do not use dtx=0
    )
#endif
{
    Word16 i, j;
    Word16 *p;
    Word32 max, t0;
    Word16 max_h, max_l, ener_h, ener_l;
    Word16 p_max = 0; // initialization only needed to keep gcc silent

    max = MIN_32;
    p_max = lag_max;

    for (i = lag_max, j = (PIT_MAX-lag_max-1); i >= lag_min; i--, j--)
    {
       if (L_sub (corr[-i], max) >= 0)
       {
          max = corr[-i];
          p_max = i;
       }
    }

    // compute energy

    t0 = 0;
    p = &scal_sig[-p_max];
    for (i = 0; i < L_frame; i++, p++)
    {
        t0 = L_mac (t0, *p, *p);
    }
    // 1/sqrt(energy)

    if (dtx)
    {  // no test() call since this if is only in simulation env
#ifdef VAD2
       *rmax = max;
       *r0 = t0;
#else
       // check tone
       vad_tone_detection (vadSt, max, t0);
#endif
    }

    t0 = Inv_sqrt (t0);

    if (scal_flag)
    {
       t0 = L_shl (t0, 1);
    }

    // max = max/sqrt(energy)

    L_Extract (max, &max_h, &max_l);
    L_Extract (t0, &ener_h, &ener_l);

    t0 = Mpy_32 (max_h, max_l, ener_h, ener_l);

    if (scal_flag)
    {
      t0 = L_shr (t0, scal_fac);
      *cor_max = extract_h (L_shl (t0, 15)); // divide by 2
    }
    else
    {
      *cor_max = extract_l(t0);
    }

    return (p_max);
}

------------------------------------------------------------------------------
 CAUTION [optional]
 [State any special notes, constraints or cautions for users of this function]

------------------------------------------------------------------------------
*/

#ifdef VAD2
static Word16 Lag_max(  /* o   : lag found                               */
    Word32 corr[],      /* i   : correlation vector.                     */
    Word16 scal_sig[],  /* i   : scaled signal.                          */
    Word16 scal_fac,    /* i   : scaled signal factor.                   */
    Word16 scal_flag,   /* i   : if 1 use EFR compatible scaling         */
    Word16 L_frame,     /* i   : length of frame to compute pitch        */
    Word16 lag_max,     /* i   : maximum lag                             */
    Word16 lag_min,     /* i   : minimum lag                             */
    Word16 *cor_max,    /* o   : normalized correlation of selected lag  */
    Word32 *rmax,       /* o   : max(<s[i]*s[j]>)                        */
    Word32 *r0,         /* o   : residual energy                         */
    Flag dtx,           /* i   : dtx flag; use dtx=1, do not use dtx=0   */
    Flag *pOverflow     /* i/o : overflow Flag                           */
)
#else
static Word16 Lag_max(  /* o   : lag found                               */
    vadState *vadSt,    /* i/o : VAD state struct                        */
    Word32 corr[],      /* i   : correlation vector.                     */
    Word16 scal_sig[],  /* i   : scaled signal.                          */
    Word16 scal_fac,    /* i   : scaled signal factor.                   */
    Word16 scal_flag,   /* i   : if 1 use EFR compatible scaling         */
    Word16 L_frame,     /* i   : length of frame to compute pitch        */
    Word16 lag_max,     /* i   : maximum lag                             */
    Word16 lag_min,     /* i   : minimum lag                             */
    Word16 *cor_max,    /* o   : normalized correlation of selected lag  */
    Flag dtx,           /* i   : dtx flag; use dtx=1, do not use dtx=0   */
    Flag *pOverflow     /* i/o : overflow Flag                           */
)
#endif
{
    register Word16 i;
    Word16 *p;
    Word32 max;
    Word32 t0;
    Word16 max_h;
    Word16 max_l;
    Word16 ener_h;
    Word16 ener_l;
    Word16 p_max = 0; /* initialization only needed to keep gcc silent */
    Word32 L_temp;
    Word32 L_temp_2;
    Word32 L_temp_3;
    Word32  *p_corr = &corr[-lag_max];

    max = MIN_32;
    p_max = lag_max;

    for (i = lag_max; i >= lag_min; i--)
    {
        /* The negative array index is equivalent to a negative */
        /* address offset, i.e., corr[-i] == *(corr - i)        */
        if (*(p_corr++) >= max)
        {
            p_corr--;
            max = *(p_corr++);
            p_max = i;
        }
    }

    /* compute energy */

    t0 = 0;

    /* The negative array index is equivalent to a negative          */
    /* address offset, i.e., scal_sig[-p_max] == *(scal_sig - p_max) */
    p = &scal_sig[-p_max];
    for (i = (L_frame >> 2); i != 0; i--)
    {
        t0 = amrnb_fxp_mac_16_by_16bb((Word32) * (p), (Word32) * (p), t0);
        p++;
        t0 = amrnb_fxp_mac_16_by_16bb((Word32) * (p), (Word32) * (p), t0);
        p++;
        t0 = amrnb_fxp_mac_16_by_16bb((Word32) * (p), (Word32) * (p), t0);
        p++;
        t0 = amrnb_fxp_mac_16_by_16bb((Word32) * (p), (Word32) * (p), t0);
        p++;
    }

    t0 <<= 1;
    /* 1/sqrt(energy) */

    if (dtx)
    {  /* no test() call since this if is only in simulation env */
        /* check tone */
#ifdef VAD2
        *rmax = max;
        *r0 = t0;
#else
        /* check tone */
        vad_tone_detection(vadSt, max, t0, pOverflow);
#endif
    }

    t0 = Inv_sqrt(t0, pOverflow);

    if (scal_flag)
    {
        if (t0 > (Word32) 0x3fffffffL)
        {
            t0 = MAX_32;
        }
        else
        {
            t0 = t0 << 1;
        }
    }

    /* max = max/sqrt(energy)  */
    /* The following code is an inlined version of */
    /* L_Extract (max, &max_h, &max_l), i.e.       */
    /*                                             */
    /* *max_h = extract_h (max);                   */
    max_h = (Word16)(max >> 16);

    /* L_temp_2 = L_shr(max,1), which is used in      */
    /* the calculation of *max_l (see next operation) */
    L_temp_2 = max >> 1;

    /* *max_l = extract_l (L_msu (L_shr (max, 1), *max_h, 16384)); */
    L_temp_3 = (Word32)(max_h << 15);

    L_temp = L_temp_2 - L_temp_3;

    max_l = (Word16)L_temp;

    /* The following code is an inlined version of */
    /* L_Extract (t0, &ener_h, &ener_l), i.e.      */
    /*                                             */
    /* *ener_h = extract_h (t0);                   */
    ener_h = (Word16)(t0 >> 16);

    /* L_temp_2 = L_shr(t0,1), which is used in        */
    /* the calculation of *ener_l (see next operation) */

    L_temp_2 = t0 >> 1;

    L_temp_3 = (Word32)(ener_h << 15);

    L_temp = L_temp_2 - L_temp_3;

    ener_l = (Word16)L_temp;

    t0 = Mpy_32(max_h, max_l, ener_h, ener_l, pOverflow);

    if (scal_flag)
    {
        t0 = L_shr(t0, scal_fac, pOverflow);

        if (t0 > (Word32) 0X0000FFFFL)
        {
            *cor_max = MAX_16;
        }
        else if (t0 < (Word32) 0xFFFF0000L)
        {
            *cor_max = MIN_16;
        }
        else
        {
            *cor_max = (Word16)(t0 >> 1);
        }
    }
    else
    {
        *cor_max = (Word16)t0;
    }

    return (p_max);
}

/*----------------------------------------------------------------------------
; End Function: Lag_max
----------------------------------------------------------------------------*/


/*
------------------------------------------------------------------------------
 FUNCTION NAME: Lag_max_wrapper
------------------------------------------------------------------------------
 INPUT AND OUTPUT DEFINITIONS

 Inputs
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
    pOverflow = pointer to overflow indicator (Flag)

 Outputs:
    cor_max contains the newly calculated normalized correlation of the
      selected lag
    rmax contains the newly calculated max(<s[i]*s[j]>)
    r0 contains the newly calculated residual energy
    pOverflow -> 1 if the math operations called by this routine saturate

 Returns:
    p_max = lag of the max correlation found (Word16)

 Global Variables Used:
    None.

 Local Variables Needed:
    None.

------------------------------------------------------------------------------
 INPUT AND OUTPUT DEFINITIONS (If VAD2 is not defined)

 Inputs
    vadSt = pointer to a vadState structure
    corr = pointer to buffer of correlation values (Word32)
    scal_sig = pointer to buffer of scaled signal values (Word16)
    scal_fac = scaled signal factor (Word16)
    scal_flag = EFR compatible scaling flag (Word16)
    L_frame = length of frame to compute pitch (Word16)
    lag_max = maximum lag (Word16)
    lag_min = minimum lag (Word16)
    cor_max = pointer to the normalized correlation of selected lag (Word16)
    dtx  = dtx flag; equal to 1, if dtx is enabled, 0, otherwise (Flag)
    pOverflow = pointer to overflow indicator (Flag)

 Outputs:
    cor_max contains the newly calculated normalized correlation of the
      selected lag
    vadSt contains the updated VAD state parameters
    pOverflow -> 1 if the math operations called by this routine saturate

 Returns:
    p_max = lag of the max correlation found (Word16)

 Global Variables Used:
    None.

 Local Variables Needed:
    None.

------------------------------------------------------------------------------
 FUNCTION DESCRIPTION

 This function provides external access to the local function Lag_max.

------------------------------------------------------------------------------
 REQUIREMENTS

 None

------------------------------------------------------------------------------
 REFERENCES

 pitch_ol.c, UMTS GSM AMR speech codec, R99 - Version 3.2.0, March 2, 2001

------------------------------------------------------------------------------
 PSEUDO-CODE

#ifdef VAD2
 CALL Lag_max(corr = corr
          scal_sig = scal_sig
          scal_fac = scal_fac
          scal_flag = scal_flag
          L_frame = L_frame
          lag_max = lag_max
          lag_min = lag_min
          cor_max = cor_max
          rmax = rmax
          r0 = r0
          dtx = dtx
          pOverflow = pOverflow)
   MODIFYING(nothing)
   RETURNING(temp)

#else
 CALL Lag_max(vadSt = vadSt
          corr = corr
          scal_sig = scal_sig
          scal_fac = scal_fac
          scal_flag = scal_flag
          L_frame = L_frame
          lag_max = lag_max
          lag_min = lag_min
          cor_max = cor_max
          dtx = dtx
          pOverflow = pOverflow)
   MODIFYING(nothing)
   RETURNING(temp)

#endif

------------------------------------------------------------------------------
 CAUTION [optional]
 [State any special notes, constraints or cautions for users of this function]

------------------------------------------------------------------------------
*/

#ifdef VAD2
Word16 Lag_max_wrapper(  /* o   : lag found                          */
    Word32 corr[],      /* i   : correlation vector.                     */
    Word16 scal_sig[],  /* i   : scaled signal.                          */
    Word16 scal_fac,    /* i   : scaled signal factor.                   */
    Word16 scal_flag,   /* i   : if 1 use EFR compatible scaling         */
    Word16 L_frame,     /* i   : length of frame to compute pitch        */
    Word16 lag_max,     /* i   : maximum lag                             */
    Word16 lag_min,     /* i   : minimum lag                             */
    Word16 *cor_max,    /* o   : normalized correlation of selected lag  */
    Word32 *rmax,       /* o   : max(<s[i]*s[j]>)                        */
    Word32 *r0,         /* o   : residual energy                         */
    Flag dtx,           /* i   : dtx flag; use dtx=1, do not use dtx=0   */
    Flag *pOverflow     /* i/o : overflow Flag                           */
)
{
    Word16 temp;

    temp = Lag_max(corr, scal_sig, scal_fac, scal_flag, L_frame, lag_max,
                   lag_min, cor_max, rmax, r0, dtx, pOverflow);

    return(temp);
}

#else
Word16 Lag_max_wrapper(  /* o   : lag found                          */
    vadState *vadSt,    /* i/o : VAD state struct                        */
    Word32 corr[],      /* i   : correlation vector.                     */
    Word16 scal_sig[],  /* i   : scaled signal.                          */
    Word16 scal_fac,    /* i   : scaled signal factor.                   */
    Word16 scal_flag,   /* i   : if 1 use EFR compatible scaling         */
    Word16 L_frame,     /* i   : length of frame to compute pitch        */
    Word16 lag_max,     /* i   : maximum lag                             */
    Word16 lag_min,     /* i   : minimum lag                             */
    Word16 *cor_max,    /* o   : normalized correlation of selected lag  */
    Flag dtx,           /* i   : dtx flag; use dtx=1, do not use dtx=0   */
    Flag *pOverflow     /* i/o : overflow Flag                           */
)
{
    Word16 temp;

    temp = Lag_max(vadSt, corr, scal_sig, scal_fac, scal_flag, L_frame,
                   lag_max, lag_min, cor_max, dtx, pOverflow);

    return(temp);
}

#endif

/*----------------------------------------------------------------------------
; End Function: Lag_max_wrapper
----------------------------------------------------------------------------*/

/*
------------------------------------------------------------------------------
 FUNCTION NAME: Pitch_ol
------------------------------------------------------------------------------
 INPUT AND OUTPUT DEFINITIONS

 Inputs:
    vadSt = pointer to a vadState structure
    mode =  data of type enum Mode specifies the mode.
    signal = pointer to buffer of signal used to compute the open loop
         pitch
    where signal[-pit_max] to signal[-1] should be known
    pit_min = 16 bit value specifies the minimum pitch lag
    pit_max = 16 bit value specifies the maximum pitch lag
    L_frame = 16 bit value specifies the length of frame to compute pitch
    idx = 16 bit value specifies the frame index
    dtx = Data of type 'Flag' used for dtx. Use dtx=1, do not use dtx=0
    pOverflow = pointer to overflow indicator (Flag)

 Outputs
    vadSt = The vadSt state structure may be modified.
    pOverflow -> 1 if the math operations called by this routine saturate

 Returns:
    p_max1 = 16 bit value representing the open loop pitch lag.

 Global Variables Used:
    None.

 Local Variables Needed:
    None.

------------------------------------------------------------------------------
 FUNCTION DESCRIPTION

 This function computes the open loop pitch lag based on the perceptually
 weighted speech signal. This is done in the following steps:
       - find three maxima of the correlation <sw[n],sw[n-T]>,
         dividing the search range into three parts:
              pit_min ... 2*pit_min-1
            2*pit_min ... 4*pit_min-1
            4*pit_min ...   pit_max
       - divide each maximum by <sw[n-t], sw[n-t]> where t is the delay at
         that maximum correlation.
       - select the delay of maximum normalized correlation (among the
         three candidates) while favoring the lower delay ranges.


------------------------------------------------------------------------------
 REQUIREMENTS

 None.

------------------------------------------------------------------------------
 REFERENCES

 pitch_ol.c, UMTS GSM AMR speech codec, R99 - Version 3.2.0, March 2, 2001

------------------------------------------------------------------------------
 PSEUDO-CODE

Word16 Pitch_ol (      // o   : open loop pitch lag
    vadState *vadSt,   // i/o : VAD state struct
    enum Mode mode,    // i   : coder mode
    Word16 signal[],   // i   : signal used to compute the open loop pitch
                       //    signal[-pit_max] to signal[-1] should be known
    Word16 pit_min,    // i   : minimum pitch lag
    Word16 pit_max,    // i   : maximum pitch lag
    Word16 L_frame,    // i   : length of frame to compute pitch
    Word16 idx,        // i   : frame index
    Flag dtx           // i   : dtx flag; use dtx=1, do not use dtx=0
    )
{
    Word16 i, j;
    Word16 max1, max2, max3;
    Word16 p_max1, p_max2, p_max3;
    Word16 scal_flag = 0;
    Word32 t0;
#ifdef VAD2
    Word32  r01, r02, r03;
    Word32  rmax1, rmax2, rmax3;
#else
    Word16 corr_hp_max;
#endif
    Word32 corr[PIT_MAX+1], *corr_ptr;

    // Scaled signal

    Word16 scaled_signal[L_FRAME + PIT_MAX];
    Word16 *scal_sig, scal_fac;

#ifndef VAD2
    if (dtx)
    {  // no test() call since this if is only in simulation env
       // update tone detection
       if ((sub(mode, MR475) == 0) || (sub(mode, MR515) == 0))
       {
          vad_tone_detection_update (vadSt, 1);
       }
       else
       {
          vad_tone_detection_update (vadSt, 0);
       }
    }
#endif

    scal_sig = &scaled_signal[pit_max];

    t0 = 0L;
    for (i = -pit_max; i < L_frame; i++)
    {
        t0 = L_mac (t0, signal[i], signal[i]);
    }

     *--------------------------------------------------------*
     * Scaling of input signal.                               *
     *                                                        *
     *   if Overflow        -> scal_sig[i] = signal[i]>>3     *
     *   else if t0 < 1^20  -> scal_sig[i] = signal[i]<<3     *
     *   else               -> scal_sig[i] = signal[i]        *
     *--------------------------------------------------------*

     *--------------------------------------------------------*
     *  Verification for risk of overflow.                    *
     *--------------------------------------------------------*

    if (L_sub (t0, MAX_32) == 0L)               // Test for overflow
    {
        for (i = -pit_max; i < L_frame; i++)
        {
            scal_sig[i] = shr (signal[i], 3);
        }
        scal_fac = 3;
    }
    else if (L_sub (t0, (Word32) 1048576L) < (Word32) 0)
        // if (t0 < 2^20)
    {
        for (i = -pit_max; i < L_frame; i++)
        {
            scal_sig[i] = shl (signal[i], 3);
        }
        scal_fac = -3;
    }
    else
    {
        for (i = -pit_max; i < L_frame; i++)
        {
            scal_sig[i] = signal[i];
        }
        scal_fac = 0;
    }

    // calculate all coreelations of scal_sig, from pit_min to pit_max
    corr_ptr = &corr[pit_max];
    comp_corr (scal_sig, L_frame, pit_max, pit_min, corr_ptr);

     *--------------------------------------------------------------------*
     *  The pitch lag search is divided in three sections.                *
     *  Each section cannot have a pitch multiple.                        *
     *  We find a maximum for each section.                               *
     *  We compare the maximum of each section by favoring small lags.    *
     *                                                                    *
     *  First section:  lag delay = pit_max     downto 4*pit_min          *
     *  Second section: lag delay = 4*pit_min-1 downto 2*pit_min          *
     *  Third section:  lag delay = 2*pit_min-1 downto pit_min            *
     *--------------------------------------------------------------------*

    // mode dependent scaling in Lag_max
    if (sub(mode, MR122) == 0)
    {
       scal_flag = 1;
    }
    else
    {
       scal_flag = 0;
    }

#ifdef VAD2
    j = shl (pit_min, 2);
    p_max1 = Lag_max (corr_ptr, scal_sig, scal_fac, scal_flag, L_frame,
                      pit_max, j, &max1, &rmax1, &r01, dtx);

    i = sub (j, 1);
    j = shl (pit_min, 1);
    p_max2 = Lag_max (corr_ptr, scal_sig, scal_fac, scal_flag, L_frame,
                      i, j, &max2, &rmax2, &r02, dtx);

    i = sub (j, 1);
    p_max3 = Lag_max (corr_ptr, scal_sig, scal_fac, scal_flag, L_frame,
                      i, pit_min, &max3, &rmax3, &r03, dtx);
#else
    j = shl (pit_min, 2);
    p_max1 = Lag_max (vadSt, corr_ptr, scal_sig, scal_fac, scal_flag, L_frame,
                      pit_max, j, &max1, dtx);

    i = sub (j, 1);
    j = shl (pit_min, 1);
    p_max2 = Lag_max (vadSt, corr_ptr, scal_sig, scal_fac, scal_flag, L_frame,
                      i, j, &max2, dtx);

    i = sub (j, 1);
    p_max3 = Lag_max (vadSt, corr_ptr, scal_sig, scal_fac, scal_flag, L_frame,
                      i, pit_min, &max3, dtx);

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

     *--------------------------------------------------------------------*
     * Compare the 3 sections maximum, and favor small lag.               *
     *--------------------------------------------------------------------*

    if (sub (mult (max1, THRESHOLD), max2) < 0)
    {
        max1 = max2;
        p_max1 = p_max2;
#ifdef VAD2
        if (dtx)
        {
            rmax1 = rmax2;
            r01 = r02;
#endif
    }
    if (sub (mult (max1, THRESHOLD), max3) < 0)
    {
        p_max1 = p_max3;
#ifdef VAD2
        if (dtx)
        {
            rmax1 = rmax3;
            r01 = r03;
        }
#endif
    }

#ifdef VAD2
    if (dtx)
    {
        vadSt->L_Rmax = L_add(vadSt->L_Rmax, rmax1);   // Save max correlation
        vadSt->L_R0 =   L_add(vadSt->L_R0, r01);        // Save max energy
    }
#endif

    return (p_max1);
}

------------------------------------------------------------------------------
 CAUTION [optional]
 [State any special notes, constraints or cautions for users of this function]

------------------------------------------------------------------------------
*/

Word16 Pitch_ol(       /* o   : open loop pitch lag                         */
    vadState *vadSt,   /* i/o : VAD state struct                            */
    enum Mode mode,    /* i   : coder mode                                  */
    Word16 signal[],   /* i   : signal used to compute the open loop pitch  */
    /*    signal[-pit_max] to signal[-1] should be known */
    Word16 pit_min,    /* i   : minimum pitch lag                           */
    Word16 pit_max,    /* i   : maximum pitch lag                           */
    Word16 L_frame,    /* i   : length of frame to compute pitch            */
    Word16 idx,        /* i   : frame index                                 */
    Flag dtx,          /* i   : dtx flag; use dtx=1, do not use dtx=0       */
    Flag *pOverflow    /* i/o : overflow Flag                               */
)
{
    Word16 i;
    Word16 j;
    Word16 max1;
    Word16 max2;
    Word16 max3;
    Word16 p_max1;
    Word16 p_max2;
    Word16 p_max3;
    Word16 scal_flag = 0;
    Word32 t0;

#ifdef VAD2
    Word32 r01;
    Word32 r02;
    Word32 r03;
    Word32 rmax1;
    Word32 rmax2;
    Word32 rmax3;
#else
    Word16 corr_hp_max;
#endif
    Word32 corr[PIT_MAX+1];
    Word32 *corr_ptr;

    /* Scaled signal */

    Word16 scaled_signal[L_FRAME + PIT_MAX];
    Word16 *scal_sig;
    Word16 *p_signal;
    Word16 scal_fac;
    Word32 L_temp;

#ifndef VAD2
    if (dtx)
    {   /* no test() call since this if is only in simulation env */
        /* update tone detection */
        if ((mode == MR475) || (mode == MR515))
        {
            vad_tone_detection_update(vadSt, 1, pOverflow);
        }
        else
        {
            vad_tone_detection_update(vadSt, 0, pOverflow);
        }
    }
#endif


    t0 = 0L;
    p_signal = &signal[-pit_max];

    for (i = -pit_max; i < L_frame; i++)
    {
        t0 += (((Word32) * (p_signal)) * *(p_signal)) << 1;
        p_signal++;
        if (t0 < 0)
        {
            t0 = MAX_32;
            break;
        }

    }

    /*--------------------------------------------------------*
     * Scaling of input signal.                               *
     *                                                        *
     *   if Overflow        -> scal_sig[i] = signal[i]>>3     *
     *   else if t0 < 1^20  -> scal_sig[i] = signal[i]<<3     *
     *   else               -> scal_sig[i] = signal[i]        *
     *--------------------------------------------------------*/

    /*--------------------------------------------------------*
     *  Verification for risk of overflow.                    *
     *--------------------------------------------------------*/

    scal_sig = &scaled_signal[0];
    p_signal = &signal[-pit_max];

    if (t0 == MAX_32)     /* Test for overflow */
    {

        for (i = (pit_max + L_frame) >> 1; i != 0; i--)
        {
            *(scal_sig++) = (Word16)(((Word32) * (p_signal++) >> 3));
            *(scal_sig++) = (Word16)(((Word32) * (p_signal++) >> 3));
        }

        if ((pit_max + L_frame) & 1)
        {
            *(scal_sig) = (Word16)(((Word32) * (p_signal) >> 3));
        }

        scal_fac = 3;
    }
    else if (t0 < (Word32)1048576L)
        /* if (t0 < 2^20) */
    {
        for (i = (pit_max + L_frame) >> 1; i != 0; i--)
        {
            *(scal_sig++) = (Word16)(((Word32) * (p_signal++) << 3));
            *(scal_sig++) = (Word16)(((Word32) * (p_signal++) << 3));
        }

        if ((pit_max + L_frame) & 1)
        {
            *(scal_sig) = (Word16)(((Word32) * (p_signal) << 3));
        }
        scal_fac = -3;
    }
    else
    {

        oscl_memcpy(scal_sig, p_signal, (L_frame + pit_max)*sizeof(*signal));
        scal_fac = 0;
    }

    /* calculate all coreelations of scal_sig, from pit_min to pit_max */
    corr_ptr = &corr[pit_max];

    scal_sig = &scaled_signal[pit_max];

    comp_corr(scal_sig, L_frame, pit_max, pit_min, corr_ptr);

    /*--------------------------------------------------------------------*
     *  The pitch lag search is divided in three sections.                *
     *  Each section cannot have a pitch multiple.                        *
     *  We find a maximum for each section.                               *
     *  We compare the maximum of each section by favoring small lags.    *
     *                                                                    *
     *  First section:  lag delay = pit_max     downto 4*pit_min          *
     *  Second section: lag delay = 4*pit_min-1 downto 2*pit_min          *
     *  Third section:  lag delay = 2*pit_min-1 downto pit_min            *
     *--------------------------------------------------------------------*/

    /* mode dependent scaling in Lag_max */

    if (mode == MR122)
    {
        scal_flag = 1;
    }
    else
    {
        scal_flag = 0;
    }

#ifdef VAD2
    L_temp = ((Word32)pit_min) << 2;
    if (L_temp != (Word32)((Word16) L_temp))
    {
        *pOverflow = 1;
        j = (pit_min > 0) ? MAX_16 : MIN_16;
    }
    else
    {
        j = (Word16)L_temp;
    }

    p_max1 = Lag_max(corr_ptr, scal_sig, scal_fac, scal_flag, L_frame,
                     pit_max, j, &max1, &rmax1, &r01, dtx, pOverflow);

    i = j - 1;

    j = pit_min << 1;

    p_max2 = Lag_max(corr_ptr, scal_sig, scal_fac, scal_flag, L_frame,
                     i, j, &max2, &rmax2, &r02, dtx, pOverflow);

    i = j - 1;

    p_max3 = Lag_max(corr_ptr, scal_sig, scal_fac, scal_flag, L_frame,
                     i, pit_min, &max3, &rmax3, &r03, dtx, pOverflow);

#else
    L_temp = ((Word32)pit_min) << 2;
    if (L_temp != (Word32)((Word16) L_temp))
    {
        *pOverflow = 1;
        j = (pit_min > 0) ? MAX_16 : MIN_16;
    }
    else
    {
        j = (Word16)L_temp;
    }

    p_max1 = Lag_max(vadSt, corr_ptr, scal_sig, scal_fac, scal_flag, L_frame,
                     pit_max, j, &max1, dtx, pOverflow);

    i = j - 1;


    j = pit_min << 1;


    p_max2 = Lag_max(vadSt, corr_ptr, scal_sig, scal_fac, scal_flag, L_frame,
                     i, j, &max2, dtx, pOverflow);

    i = j - 1;
    p_max3 = Lag_max(vadSt, corr_ptr, scal_sig, scal_fac, scal_flag, L_frame,
                     i, pit_min, &max3, dtx, pOverflow);

    if (dtx)
    {  /* no test() call since this if is only in simulation env */

        if (idx == 1)
        {
            /* calculate max high-passed filtered correlation of all lags */
            hp_max(corr_ptr, scal_sig, L_frame, pit_max, pit_min, &corr_hp_max,
                   pOverflow);

            /* update complex background detector */
            vad_complex_detection_update(vadSt, corr_hp_max);
        }
    }
#endif

    /*--------------------------------------------------------------------*
     * Compare the 3 sections maximum, and favor small lag.               *
     *--------------------------------------------------------------------*/

    i = (Word16)(((Word32)max1 * THRESHOLD) >> 15);

    if (i < max2)
    {
        max1 = max2;
        p_max1 = p_max2;

#ifdef VAD2
        if (dtx)
        {
            rmax1 = rmax2;
            r01 = r02;
        }
#endif
    }

    i = (Word16)(((Word32)max1 * THRESHOLD) >> 15);


    if (i < max3)
    {
        p_max1 = p_max3;

#ifdef VAD2
        if (dtx)
        {
            rmax1 = rmax3;
            r01 = r03;
        }
#endif
    }

#ifdef VAD2
    if (dtx)
    {
        /* Save max correlation */
        vadSt->L_Rmax = L_add(vadSt->L_Rmax, rmax1, pOverflow);
        /* Save max energy */
        vadSt->L_R0 =   L_add(vadSt->L_R0, r01, pOverflow);
    }
#endif

    return (p_max1);
}
