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



 Filename: ph_disp.cpp
 Functions:
            ph_disp_reset
            ph_disp_lock
            ph_disp_release
            ph_disp

------------------------------------------------------------------------------
 MODULE DESCRIPTION

 This file contains the function that performs adaptive phase dispersion of
 the excitation signal. The phase dispersion initialization, reset, and
 exit functions are included in this file, as well as, the phase dispersion
 lock and release functions.

------------------------------------------------------------------------------
*/

/*----------------------------------------------------------------------------
; INCLUDES
----------------------------------------------------------------------------*/
#include "ph_disp.h"
#include "typedef.h"
#include "basic_op.h"
#include "cnst.h"

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
 FUNCTION NAME: ph_disp_reset
------------------------------------------------------------------------------
 INPUT AND OUTPUT DEFINITIONS

 Inputs:
    state = pointer to a structure of type ph_dispState

 Outputs:
    Structure pointed to by state is initialized to zeros

 Returns:
    return_value = 0, if reset was successful; -1, otherwise (int)

 Global Variables Used:
    None

 Local Variables Needed:
    None

------------------------------------------------------------------------------
 FUNCTION DESCRIPTION

 This function resets the variables used by the phase dispersion function.

------------------------------------------------------------------------------
 REQUIREMENTS

 None

------------------------------------------------------------------------------
 REFERENCES

 ph_disp.c, UMTS GSM AMR speech codec, R99 - Version 3.2.0, March 2, 2001

------------------------------------------------------------------------------
 PSEUDO-CODE

int ph_disp_reset (ph_dispState *state)
{
  Word16 i;

   if (state == (ph_dispState *) NULL){
      fprint(stderr, "ph_disp_reset: invalid parameter\n");
      return -1;
   }
   for (i=0; i<PHDGAINMEMSIZE; i++)
   {
       state->gainMem[i] = 0;
   }
   state->prevState = 0;
   state->prevCbGain = 0;
   state->lockFull = 0;
   state->onset = 0;          // assume no onset in start

   return 0;
}

------------------------------------------------------------------------------
 CAUTION [optional]
 [State any special notes, constraints or cautions for users of this function]

------------------------------------------------------------------------------
*/

Word16 ph_disp_reset(ph_dispState *state)
{
    register Word16 i;

    if (state == (ph_dispState *) NULL)
    {
        /*  fprint(stderr, "ph_disp_reset: invalid parameter\n");  */
        return(-1);
    }
    for (i = 0; i < PHDGAINMEMSIZE; i++)
    {
        state->gainMem[i] = 0;
    }
    state->prevState = 0;
    state->prevCbGain = 0;
    state->lockFull = 0;
    state->onset = 0;          /* assume no onset in start */

    return(0);
}

/****************************************************************************/

/*
------------------------------------------------------------------------------
 FUNCTION NAME: ph_disp_lock
------------------------------------------------------------------------------
 INPUT AND OUTPUT DEFINITIONS

 Inputs:
    state = pointer to a structure of type ph_dispState

 Outputs:
    lockFull field of the structure pointed to by state is set to 1

 Returns:
    None

 Global Variables Used:
    None

 Local Variables Needed:
    None

------------------------------------------------------------------------------
 FUNCTION DESCRIPTION

 This function sets the lockFull flag to indicate a lock condition.

------------------------------------------------------------------------------
 REQUIREMENTS

 None

------------------------------------------------------------------------------
 REFERENCES

 ph_disp.c, UMTS GSM AMR speech codec, R99 - Version 3.2.0, March 2, 2001

------------------------------------------------------------------------------
 PSEUDO-CODE

void ph_disp_lock (ph_dispState *state)
{
  state->lockFull = 1;
  return;
}

------------------------------------------------------------------------------
 CAUTION [optional]
 [State any special notes, constraints or cautions for users of this function]

------------------------------------------------------------------------------
*/

void ph_disp_lock(ph_dispState *state)
{
    state->lockFull = 1;

    return;
}

/****************************************************************************/

/*
------------------------------------------------------------------------------
 FUNCTION NAME: ph_disp_release
------------------------------------------------------------------------------
 INPUT AND OUTPUT DEFINITIONS

 Inputs:
    state = pointer to a structure of type ph_dispState

 Outputs:
    lockFull field of the structure pointed to by state is set to 0

 Returns:
    None

 Global Variables Used:
    None

 Local Variables Needed:
    None

------------------------------------------------------------------------------
 FUNCTION DESCRIPTION

 This function clears the lockFull flag to indicate an unlocked state.

------------------------------------------------------------------------------
 REQUIREMENTS

 None

------------------------------------------------------------------------------
 REFERENCES

 ph_disp.c, UMTS GSM AMR speech codec, R99 - Version 3.2.0, March 2, 2001

------------------------------------------------------------------------------
 PSEUDO-CODE

void ph_disp_release (ph_dispState *state)
{
  state->lockFull = 0;
  return;
}

------------------------------------------------------------------------------
 CAUTION [optional]
 [State any special notes, constraints or cautions for users of this function]

------------------------------------------------------------------------------
*/

void ph_disp_release(ph_dispState *state)
{
    state->lockFull = 0;

    return;
}

/****************************************************************************/

/*
------------------------------------------------------------------------------
 FUNCTION NAME: ph_disp
------------------------------------------------------------------------------
 INPUT AND OUTPUT DEFINITIONS

 Inputs:
    state = pointer to a structure of type ph_dispState
    mode = codec mode (enum Mode)
    x = LTP excitation signal buffer (Word16)
    cbGain = codebook gain (Word16)
    ltpGain = LTP gain (Word16)
    inno = innovation buffer (Word16)
    pitch_fac = pitch factor used to scale the LTP excitation (Word16)
    tmp_shift = shift factor applied to sum of scaled LTP excitation and
                innovation before rounding (Word16)
    pOverflow = pointer to overflow indicator (Flag)

 Outputs:
    structure pointed to by state contains the updated gainMem array,
      prevState, prevCbGain, and onset fields
    x buffer contains the new excitation signal
    inno buffer contains the new innovation signal
    pOverflow -> 1 if there is overflow

 Returns:
    None

 Global Variables Used:
    None

 Local Variables Needed:
    None

------------------------------------------------------------------------------
 FUNCTION DESCRIPTION

 This function performs adaptive phase dispersion, i.e., forming of total
 excitation for the synthesis part of the decoder.

------------------------------------------------------------------------------
 REQUIREMENTS

 None

------------------------------------------------------------------------------
 REFERENCES

 ph_disp.c, UMTS GSM AMR speech codec, R99 - Version 3.2.0, March 2, 2001

------------------------------------------------------------------------------
 PSEUDO-CODE

void ph_disp (
      ph_dispState *state, // i/o     : State struct
      enum Mode mode,      // i       : codec mode
      Word16 x[],          // i/o Q0  : in:  LTP excitation signal
                           //           out: total excitation signal
      Word16 cbGain,       // i   Q1  : Codebook gain
      Word16 ltpGain,      // i   Q14 : LTP gain
      Word16 inno[],       // i/o Q13 : Innovation vector (Q12 for 12.2)
      Word16 pitch_fac,    // i   Q14 : pitch factor used to scale the
                                        LTP excitation (Q13 for 12.2)
      Word16 tmp_shift     // i   Q0  : shift factor applied to sum of
                                        scaled LTP ex & innov. before
                                        rounding
)
{
   Word16 i, i1;
   Word16 tmp1;
   Word32 L_temp;
   Word16 impNr;           // indicator for amount of disp./filter used

   Word16 inno_sav[L_SUBFR];
   Word16 ps_poss[L_SUBFR];
   Word16 j, nze, nPulse, ppos;
   const Word16 *ph_imp;   // Pointer to phase dispersion filter

   // Update LTP gain memory
   for (i = PHDGAINMEMSIZE-1; i > 0; i--)
   {
       state->gainMem[i] = state->gainMem[i-1];
   }
   state->gainMem[0] = ltpGain;

   // basic adaption of phase dispersion
   if (sub(ltpGain, PHDTHR2LTP) < 0) {    // if (ltpGain < 0.9)
       if (sub(ltpGain, PHDTHR1LTP) > 0)
       {  // if (ltpGain > 0.6
          impNr = 1; // medium dispersion
       }
       else
       {
          impNr = 0; // maximum dispersion
       }
   }
   else
   {
      impNr = 2; // no dispersion
   }

   // onset indicator
   // onset = (cbGain  > onFact * cbGainMem[0])
   tmp1 = pv_round(L_shl(L_mult(state->prevCbGain, ONFACTPLUS1), 2));
   if (sub(cbGain, tmp1) > 0)
   {
       state->onset = ONLENGTH;
   }
   else
   {
       if (state->onset > 0)
       {
           state->onset = sub (state->onset, 1);
       }
   }

   // if not onset, check ltpGain buffer and use max phase dispersion if
      half or more of the ltpGain-parameters say so
   if (state->onset == 0)
   {
       // Check LTP gain memory and set filter accordingly
       i1 = 0;
       for (i = 0; i < PHDGAINMEMSIZE; i++)
       {
           if (sub(state->gainMem[i], PHDTHR1LTP) < 0)
           {
               i1 = add (i1, 1);
           }
       }
       if (sub(i1, 2) > 0)
       {
           impNr = 0;
       }

   }
   // Restrict decrease in phase dispersion to one step if not onset
   if ((sub(impNr, add(state->prevState, 1)) > 0) && (state->onset == 0))
   {
       impNr = sub (impNr, 1);
   }
   // if onset, use one step less phase dispersion
   if((sub(impNr, 2) < 0) && (state->onset > 0))
   {
       impNr = add (impNr, 1);
   }

   // disable for very low levels
   if(sub(cbGain, 10) < 0)
   {
       impNr = 2;
   }

   if(sub(state->lockFull, 1) == 0)
   {
       impNr = 0;
   }

   // update static memory
   state->prevState = impNr;
   state->prevCbGain = cbGain;

   // do phase dispersion for all modes but 12.2 and 7.4;
   // don't modify the innovation if impNr >=2 (= no phase disp)
   if (sub(mode, MR122) != 0 &&
       sub(mode, MR102) != 0 &&
       sub(mode, MR74) != 0 &&
       sub(impNr, 2) < 0)
   {
       // track pulse positions, save innovation,
          and initialize new innovation
       nze = 0;
       for (i = 0; i < L_SUBFR; i++)
       {
           if (inno[i] != 0)
           {
               ps_poss[nze] = i;
               nze = add (nze, 1);
           }
           inno_sav[i] = inno[i];
           inno[i] = 0;
       }
       // Choose filter corresponding to codec mode and dispersion criterium
       if (sub (mode, MR795) == 0)
       {
           if (impNr == 0)
           {
               ph_imp = ph_imp_low_MR795;
           }
           else
           {
               ph_imp = ph_imp_mid_MR795;
           }
       }
       else
       {
           if (impNr == 0)
           {
               ph_imp = ph_imp_low;
           }
           else
           {
               ph_imp = ph_imp_mid;
           }
       }

       // Do phase dispersion of innovation
       for (nPulse = 0; nPulse < nze; nPulse++)
       {
           ppos = ps_poss[nPulse];

           // circular convolution with impulse response
           j = 0;
           for (i = ppos; i < L_SUBFR; i++)
           {
               // inno[i1] += inno_sav[ppos] * ph_imp[i1-ppos]
               tmp1 = mult(inno_sav[ppos], ph_imp[j++]);
               inno[i] = add(inno[i], tmp1);
           }

           for (i = 0; i < ppos; i++)
           {
               // inno[i] += inno_sav[ppos] * ph_imp[L_SUBFR-ppos+i]
               tmp1 = mult(inno_sav[ppos], ph_imp[j++]);
               inno[i] = add(inno[i], tmp1);
           }
       }
   }

   // compute total excitation for synthesis part of decoder
   // (using modified innovation if phase dispersion is active)
   for (i = 0; i < L_SUBFR; i++)
   {
       // x[i] = gain_pit*x[i] + cbGain*code[i];
       L_temp = L_mult (        x[i],    pitch_fac);
                                                // 12.2: Q0 * Q13
                                                //  7.4: Q0 * Q14
       L_temp = L_mac  (L_temp, inno[i], cbGain);
                                                // 12.2: Q12 * Q1
                                                //  7.4: Q13 * Q1
       L_temp = L_shl (L_temp, tmp_shift);                 // Q16
       x[i] = pv_round (L_temp);
   }

   return;
}

------------------------------------------------------------------------------
 CAUTION [optional]
 [State any special notes, constraints or cautions for users of this function]

------------------------------------------------------------------------------
*/

void ph_disp(
    ph_dispState *state,    /* i/o     : State struct                       */
    enum Mode mode,         /* i       : codec mode                         */
    Word16 x[],             /* i/o Q0  : in:  LTP excitation signal         */
    /*           out: total excitation signal       */
    Word16 cbGain,          /* i   Q1  : Codebook gain                      */
    Word16 ltpGain,         /* i   Q14 : LTP gain                           */
    Word16 inno[],          /* i/o Q13 : Innovation vector (Q12 for 12.2)   */
    Word16 pitch_fac,       /* i   Q14 : pitch factor used to scale the
                                         LTP excitation (Q13 for 12.2)      */
    Word16 tmp_shift,       /* i   Q0  : shift factor applied to sum of
                                         scaled LTP ex & innov. before
                                         rounding                           */
    CommonAmrTbls* common_amr_tbls, /* i : ptr to struct of table ptrs      */
    Flag   *pOverflow       /* i/o     : oveflow indicator                  */
)
{
    register Word16 i, i1;
    register Word16 tmp1;
    Word32 L_temp;
    Word32 L_temp2;
    Word16 impNr;           /* indicator for amount of disp./filter used */

    Word16 inno_sav[L_SUBFR];
    Word16 ps_poss[L_SUBFR];
    register Word16 nze, nPulse;
    Word16 ppos;
    const Word16 *ph_imp;   /* Pointer to phase dispersion filter */

    Word16 *p_inno;
    Word16 *p_inno_sav;
    Word16 *p_x;
    const Word16 *p_ph_imp;
    Word16 c_inno_sav;

    const Word16* ph_imp_low_MR795_ptr = common_amr_tbls->ph_imp_low_MR795_ptr;
    const Word16* ph_imp_mid_MR795_ptr = common_amr_tbls->ph_imp_mid_MR795_ptr;
    const Word16* ph_imp_low_ptr = common_amr_tbls->ph_imp_low_ptr;
    const Word16* ph_imp_mid_ptr = common_amr_tbls->ph_imp_mid_ptr;

    /* Update LTP gain memory */
    /* Unrolled FOR loop below since PHDGAINMEMSIZE is assumed to stay */
    /* the same.                                                       */
    /* for (i = PHDGAINMEMSIZE-1; i > 0; i--)                          */
    /* {                                                               */
    /*    state->gainMem[i] = state->gainMem[i-1];                     */
    /* }                                                               */
    state->gainMem[4] = state->gainMem[3];
    state->gainMem[3] = state->gainMem[2];
    state->gainMem[2] = state->gainMem[1];
    state->gainMem[1] = state->gainMem[0];
    state->gainMem[0] = ltpGain;

    /* basic adaption of phase dispersion */

    if (ltpGain < PHDTHR2LTP)    /* if (ltpGain < 0.9) */
    {
        if (ltpGain > PHDTHR1LTP)
        {  /* if (ltpGain > 0.6 */
            impNr = 1; /* medium dispersion */
        }
        else
        {
            impNr = 0; /* maximum dispersion */
        }
    }
    else
    {
        impNr = 2; /* no dispersion */
    }

    /* onset indicator */
    /* onset = (cbGain  > onFact * cbGainMem[0]) */

    L_temp = ((Word32) state->prevCbGain * ONFACTPLUS1) << 1;

    /* (L_temp << 2) calculation with saturation check */
    if (L_temp > (Word32) 0X1fffffffL)
    {
        *pOverflow = 1;
        L_temp = MAX_32;
    }
    else if (L_temp < (Word32) - 536870912)
    {
        *pOverflow = 1;
        L_temp = MIN_32;
    }
    else
    {
        L_temp <<= 2;
    }

    tmp1 = pv_round(L_temp, pOverflow);

    if (cbGain > tmp1)
    {
        state->onset = ONLENGTH;
    }
    else
    {

        if (state->onset > 0)
        {
            state->onset -= 1;
        }
    }

    /* if not onset, check ltpGain buffer and use max phase dispersion if
       half or more of the ltpGain-parameters say so */
    if (state->onset == 0)
    {
        /* Check LTP gain memory and set filter accordingly */
        i1 = 0;
        for (i = 0; i < PHDGAINMEMSIZE; i++)
        {
            if (state->gainMem[i] < PHDTHR1LTP)
            {
                i1 += 1;
            }
        }

        if (i1 > 2)
        {
            impNr = 0;
        }
    }
    /* Restrict decrease in phase dispersion to one step if not onset */
    if ((impNr > ((state->prevState) + 1)) && (state->onset == 0))
    {
        impNr -= 1;
    }

    /* if onset, use one step less phase dispersion */
    if ((impNr < 2) && (state->onset > 0))
    {
        impNr += 1;
    }

    /* disable for very low levels */
    if (cbGain < 10)
    {
        impNr = 2;
    }

    if (state->lockFull == 1)
    {
        impNr = 0;
    }

    /* update static memory */
    state->prevState = impNr;
    state->prevCbGain = cbGain;

    /* do phase dispersion for all modes but 12.2 and 7.4;
       don't modify the innovation if impNr >=2 (= no phase disp) */
    if ((mode != MR122) && (mode != MR102) && (mode != MR74) && (impNr < 2))
    {
        /* track pulse positions, save innovation,
           and initialize new innovation          */
        nze = 0;
        p_inno = &inno[0];
        p_inno_sav = &inno_sav[0];

        for (i = 0; i < L_SUBFR; i++)
        {
            if (*(p_inno) != 0)
            {
                ps_poss[nze] = i;
                nze += 1;
            }
            *(p_inno_sav++) = *(p_inno);
            *(p_inno++) = 0;
        }

        /* Choose filter corresponding to codec mode and dispersion criterium */
        if (mode == MR795)
        {
            if (impNr == 0)
            {
                ph_imp = ph_imp_low_MR795_ptr;
            }
            else
            {
                ph_imp = ph_imp_mid_MR795_ptr;
            }
        }
        else
        {
            if (impNr == 0)
            {
                ph_imp = ph_imp_low_ptr;
            }
            else
            {
                ph_imp = ph_imp_mid_ptr;
            }
        }

        /* Do phase dispersion of innovation */
        for (nPulse = 0; nPulse < nze; nPulse++)
        {
            ppos = ps_poss[nPulse];

            /* circular convolution with impulse response */
            c_inno_sav = inno_sav[ppos];
            p_inno = &inno[ppos];
            p_ph_imp = ph_imp;

            for (i = ppos; i < L_SUBFR; i++)
            {
                /* inno[i1] += inno_sav[ppos] * ph_imp[i1-ppos] */
                L_temp = ((Word32) c_inno_sav * *(p_ph_imp++)) >> 15;
                tmp1 = (Word16) L_temp;
                *(p_inno) = add_16(*(p_inno), tmp1, pOverflow);
                p_inno += 1;
            }

            p_inno = &inno[0];

            for (i = 0; i < ppos; i++)
            {
                /* inno[i] += inno_sav[ppos] * ph_imp[L_SUBFR-ppos+i] */
                L_temp = ((Word32) c_inno_sav * *(p_ph_imp++)) >> 15;
                tmp1 = (Word16) L_temp;
                *(p_inno) = add_16(*(p_inno), tmp1, pOverflow);
                p_inno += 1;
            }
        }
    }

    /* compute total excitation for synthesis part of decoder
       (using modified innovation if phase dispersion is active) */
    p_inno = &inno[0];
    p_x = &x[0];

    for (i = 0; i < L_SUBFR; i++)
    {
        /* x[i] = gain_pit*x[i] + cbGain*code[i]; */
        L_temp = L_mult(x[i], pitch_fac, pOverflow);
        /* 12.2: Q0 * Q13 */
        /*  7.4: Q0 * Q14 */
        L_temp2 = ((Word32) * (p_inno++) * cbGain) << 1;
        L_temp = L_add(L_temp, L_temp2, pOverflow);
        /* 12.2: Q12 * Q1 */
        /*  7.4: Q13 * Q1 */
        L_temp = L_shl(L_temp, tmp_shift, pOverflow);                  /* Q16 */
        *(p_x++) = pv_round(L_temp, pOverflow);
    }

    return;
}
