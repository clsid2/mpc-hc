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



 Filename: agc.cpp
 Funtions: energy_old
           energy_new
           agc_init
           agc_reset
           agc_exit
           agc
           agc2

------------------------------------------------------------------------------
 MODULE DESCRIPTION

 This set of modules scale the excitation level and output of the speech
 signals.

------------------------------------------------------------------------------
*/


/*----------------------------------------------------------------------------
; INCLUDES
----------------------------------------------------------------------------*/

#include    "agc.h"
#include    "cnst.h"
#include    "inv_sqrt.h"
#include    "basic_op.h"

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
 FUNCTION NAME: energy_old
------------------------------------------------------------------------------
 INPUT AND OUTPUT DEFINITIONS

 Inputs:
    in = input signal (Word16)
    l_trm = input signal length (Word16)
    pOverflow = address of overflow (Flag)

 Outputs:
    pOverflow -> 1 if the energy computation saturates

 Returns:
    s = return energy of signal (Word32)

 Global Variables Used:
    None.

 Local Variables Needed:
    None.

------------------------------------------------------------------------------
 FUNCTION DESCRIPTION

 Returns the energy of the signal.

------------------------------------------------------------------------------
 REQUIREMENTS

 None.

------------------------------------------------------------------------------
 REFERENCES

 agc.c, UMTS GSM AMR speech codec, R99 - Version 3.2.0, March 2, 2001

------------------------------------------------------------------------------
 PSEUDO-CODE

static Word32 energy_old( // o : return energy of signal
    Word16 in[],          // i : input signal (length l_trm)
    Word16 l_trm          // i : signal length
)
{
    Word32 s;
    Word16 i, temp;

    temp = shr (in[0], 2);
    s = L_mult (temp, temp);

    for (i = 1; i < l_trm; i++)
    {
        temp = shr (in[i], 2);
        s = L_mac (s, temp, temp);
    }

    return s;
}

------------------------------------------------------------------------------
 CAUTION [optional]
 [State any special notes, constraints or cautions for users of this function]

------------------------------------------------------------------------------
*/

static Word32 energy_old(       /* o : return energy of signal      */
    Word16 in[],        /* i : input signal (length l_trm)  */
    Word16 l_trm,       /* i : signal length                */
    Flag   *pOverflow   /* overflow: flag to indicate overflow */
)

{
    Word32  s = 0;
    Word16  i;
    Word16  temp;

    for (i = 0; i < l_trm; i++)
    {
        temp = in[i] >> 2;
        s = L_mac(s, temp, temp, pOverflow);
    }

    return(s);
}

/*----------------------------------------------------------------------------*/
/*
------------------------------------------------------------------------------
 FUNCTION NAME: energy_old__Wrapper
------------------------------------------------------------------------------
 INPUT AND OUTPUT DEFINITIONS

 Inputs:
    in = input signal (Word16)
    l_trm = input signal length (Word16)
    pOverflow = address of overflow (Flag)
 Outputs:
    pOverflow -> 1 if the energy computation saturates

 Returns:
    s = return energy of signal (Word32)

 Global Variables Used:
    None.

 Local Variables Needed:
    None.

------------------------------------------------------------------------------
 FUNCTION DESCRIPTION

 This function provides external access to the static function energy_old.

------------------------------------------------------------------------------
 REQUIREMENTS

 None

------------------------------------------------------------------------------
 REFERENCES

 None

------------------------------------------------------------------------------
 PSEUDO-CODE

 CALL energy_old (  in = in
            l_trm = l_trm
            pOverflow = pOverflow )
   MODIFYING(nothing)
   RETURNING(energy_old_value = s)

------------------------------------------------------------------------------
 CAUTION [optional]
 [State any special notes, constraints or cautions for users of this function]

------------------------------------------------------------------------------
*/

Word32 energy_old_Wrapper(Word16 in[], Word16 l_trm, Flag *pOverflow)
{
    Word32 energy_old_value;

    /*----------------------------------------------------------------------------
     CALL energy_old (  in = in
                l_trm = l_trm
                pOverflow = pOverflow )

      MODIFYING(nothing)
       RETURNING(energy_old_value = s)
    ----------------------------------------------------------------------------*/
    energy_old_value = energy_old(in, l_trm, pOverflow);
    return(energy_old_value);
}
/*--------------------------------------------------------------------------*/

/*
-----------------------------------------------------------------------------
 FUNCTION NAME: energy_new
------------------------------------------------------------------------------
 INPUT AND OUTPUT DEFINITIONS

 Inputs:
    in = input signal
    l_trm = input signal length
    pOverflow = address of overflow (Flag)

 Outputs:
    pOverflow -> 1 if the energy computation saturates

 Returns:
    s = return energy of signal

 Global Variables Used:
    None.

 Local Variables Needed:
    None.

------------------------------------------------------------------------------
 FUNCTION DESCRIPTION

 Returns the energy of the signal.

------------------------------------------------------------------------------
 REQUIREMENTS

 None.

------------------------------------------------------------------------------
 REFERENCES

 agc.c, UMTS GSM AMR speech codec, R99 - Version 3.2.0, March 2, 2001

------------------------------------------------------------------------------
 PSEUDO-CODE

static Word32 energy_new( // o : return energy of signal
    Word16 in[],          // i : input signal (length l_trm)
    Word16 l_trm )        // i : signal length

{
    Word32 s;
    Word16 i;
    Flag ov_save;

    ov_save = Overflow;            //save overflow flag in case energy_old
                                   // must be called
    s = L_mult(in[0], in[0]);
    for (i = 1; i < l_trm; i++)
    {
        s = L_mac(s, in[i], in[i]);
    }

    // check for overflow
    if (L_sub (s, MAX_32) == 0L)
    {
        Overflow = ov_save; // restore overflow flag
        s = energy_old (in, l_trm); // function result
    }
    else
    {
       s = L_shr(s, 4);
    }

    return(s);
}

------------------------------------------------------------------------------
 CAUTION [optional]
 [State any special notes, constraints or cautions for users of this function]

------------------------------------------------------------------------------
*/

static Word32 energy_new(       /* o : return energy of signal      */
    Word16 in[],        /* i : input signal (length l_trm)  */
    Word16 l_trm,       /* i : signal length                */
    Flag *pOverflow     /* i : overflow flag                */
)

{
    Word32  s = 0;
    Word16  i;
    Flag    ov_save;

    ov_save = *(pOverflow);  /* save overflow flag in case energy_old */
    /* must be called                        */


    for (i = 0; i < l_trm; i++)
    {
        s = L_mac(s, in[i], in[i], pOverflow);
    }

    /* check for overflow */
    if (s != MAX_32)
    {
        /* s is a sum of squares, so it won't be negative */
        s = s >> 4;
    }
    else
    {
        *(pOverflow) = ov_save;  /* restore overflow flag */
        s = energy_old(in, l_trm, pOverflow);   /* function result */
    }

    return (s);
}

/*--------------------------------------------------------------------------*/
/*
------------------------------------------------------------------------------
 FUNCTION NAME: energy_new__Wrapper
------------------------------------------------------------------------------
 INPUT AND OUTPUT DEFINITIONS

 Inputs:
    in = input signal (Word16)
    l_trm = input signal length (Word16)
    overflow = address of overflow (Flag)

 Outputs:
    pOverflow -> 1 if the energy computation saturates

 Returns:
    s = return energy of signal (Word32)

 Global Variables Used:
    None.

 Local Variables Needed:
    None.

------------------------------------------------------------------------------
 FUNCTION DESCRIPTION

 This function provides external access to the static function energy_new.

------------------------------------------------------------------------------
 REQUIREMENTS

 None

------------------------------------------------------------------------------
 REFERENCES

 None

------------------------------------------------------------------------------
 PSEUDO-CODE

 CALL energy_new (  in = in
            l_trm = l_trm
            pOverflow = pOverflow )

   MODIFYING(nothing)

   RETURNING(energy_new_value = s)

------------------------------------------------------------------------------
 CAUTION [optional]
 [State any special notes, constraints or cautions for users of this function]

------------------------------------------------------------------------------
*/

Word32 energy_new_Wrapper(Word16 in[], Word16 l_trm, Flag *pOverflow)
{
    Word32 energy_new_value;

    /*----------------------------------------------------------------------------
     CALL energy_new (  in = in
                l_trm = l_trm
                pOverflow = pOverflow )

       MODIFYING(nothing)
       RETURNING(energy_new_value = s)

    ----------------------------------------------------------------------------*/
    energy_new_value = energy_new(in, l_trm, pOverflow);

    return(energy_new_value);

}

/*--------------------------------------------------------------------------*/



/*
------------------------------------------------------------------------------
 FUNCTION NAME: agc_reset
------------------------------------------------------------------------------
 INPUT AND OUTPUT DEFINITIONS

 Inputs:
    state = pointer to a structure of type agcState

 Outputs:
    Structure pointed to by state is initialized to zeros

 Returns:
    Returns 0 if memory was successfully initialized,
        otherwise returns -1.

 Global Variables Used:
    None.

 Local Variables Needed:
    None.

------------------------------------------------------------------------------
 FUNCTION DESCRIPTION

 Reset of agc (i.e. set state memory to 1.0).

------------------------------------------------------------------------------
 REQUIREMENTS

 None.

------------------------------------------------------------------------------
 REFERENCES

 agc.c, UMTS GSM AMR speech codec, R99 - Version 3.2.0, March 2, 2001

------------------------------------------------------------------------------
 PSEUDO-CODE

int agc_reset (agcState *state)
{
  if (state == (agcState *) NULL)
  {
      fprintf(stderr, "agc_reset: invalid parameter\n");
      return -1;
  }

  state->past_gain = 4096;   // initial value of past_gain = 1.0

  return 0;
}

------------------------------------------------------------------------------
 CAUTION [optional]
 [State any special notes, constraints or cautions for users of this function]

------------------------------------------------------------------------------
*/

Word16 agc_reset(agcState *state)
{
    if (state == (agcState *) NULL)
    {
        /* fprintf(stderr, "agc_reset: invalid parameter\n"); */
        return(-1);
    }

    state->past_gain = 4096;   /* initial value of past_gain = 1.0  */

    return(0);
}

/*--------------------------------------------------------------------------*/

/*
------------------------------------------------------------------------------
 FUNCTION NAME: agc
------------------------------------------------------------------------------
 INPUT AND OUTPUT DEFINITIONS

 Inputs:
    st = pointer to agc state
    sig_in = pointer to a buffer containing the postfilter input signal
    sig_out = pointer to a buffer containing the postfilter output signal
    agc_fac = AGC factor
    l_trm = subframe size
    pOverflow = pointer to the overflow flag

 Outputs:
    st->past_gain = gain
    buffer pointed to by sig_out contains the new postfilter output signal
    pOverflow -> 1 if the agc computation saturates

 Returns:
    return = 0

 Global Variables Used:
    none.

 Local Variables Needed:
    none.

------------------------------------------------------------------------------
 FUNCTION DESCRIPTION

 Scales the postfilter output on a subframe basis using:

     sig_out[n] = sig_out[n] * gain[n]
     gain[n] = agc_fac * gain[n-1] + (1 - agc_fac) g_in/g_out

 where: gain[n] = gain at the nth sample given by
        g_in/g_out = square root of the ratio of energy at
                     the input and output of the postfilter.

------------------------------------------------------------------------------
 REQUIREMENTS

 None.

------------------------------------------------------------------------------
 REFERENCES

 agc.c, UMTS GSM AMR speech codec, R99 - Version 3.2.0, March 2, 2001

------------------------------------------------------------------------------
 PSEUDO-CODE

int agc (
    agcState *st,      // i/o : agc state
    Word16 *sig_in,    // i   : postfilter input signal  (l_trm)
    Word16 *sig_out,   // i/o : postfilter output signal (l_trm)
    Word16 agc_fac,    // i   : AGC factor
    Word16 l_trm       // i   : subframe size
)
{
    Word16 i, exp;
    Word16 gain_in, gain_out, g0, gain;
    Word32 s;

    // calculate gain_out with exponent
    s = energy_new(sig_out, l_trm); // function result

    if (s == 0)
    {
        st->past_gain = 0;
        return 0;
    }
    exp = sub (norm_l (s), 1);
    gain_out = pv_round (L_shl (s, exp));

    // calculate gain_in with exponent
    s = energy_new(sig_in, l_trm); // function result

    if (s == 0)
    {
        g0 = 0;
    }
    else
    {
        i = norm_l (s);
        gain_in = pv_round (L_shl (s, i));
        exp = sub (exp, i);

         *---------------------------------------------------*
         *  g0 = (1-agc_fac) * sqrt(gain_in/gain_out);       *
         *---------------------------------------------------*

        s = L_deposit_l (div_s (gain_out, gain_in));
        s = L_shl (s, 7);       // s = gain_out / gain_in
        s = L_shr (s, exp);     // add exponent

        s = Inv_sqrt (s); // function result
        i = pv_round (L_shl (s, 9));

        // g0 = i * (1-agc_fac)
        g0 = mult (i, sub (32767, agc_fac));
    }

    // compute gain[n] = agc_fac * gain[n-1]
                        + (1-agc_fac) * sqrt(gain_in/gain_out)
    // sig_out[n] = gain[n] * sig_out[n]

    gain = st->past_gain;

    for (i = 0; i < l_trm; i++)
    {
        gain = mult (gain, agc_fac);
        gain = add (gain, g0);
        sig_out[i] = extract_h (L_shl (L_mult (sig_out[i], gain), 3));
    }

    st->past_gain = gain;

    return 0;
}

------------------------------------------------------------------------------
 CAUTION [optional]
 [State any special notes, constraints or cautions for users of this function]

------------------------------------------------------------------------------
*/

void agc(
    agcState *st,      /* i/o : agc state                        */
    Word16 *sig_in,    /* i   : postfilter input signal  (l_trm) */
    Word16 *sig_out,   /* i/o : postfilter output signal (l_trm) */
    Word16 agc_fac,    /* i   : AGC factor                       */
    Word16 l_trm,      /* i   : subframe size                    */
    Flag *pOverflow    /* i   : overflow Flag                    */

)

{
    Word16  i;
    Word16  exp;
    Word16  gain_in;
    Word16  gain_out;
    Word16  g0;
    Word16  gain;
    Word32  s;
    Word32  L_temp;
    Word16  temp;

    Word16 *p_sig_out;

    /* calculate gain_out with exponent */
    s = energy_new(sig_out, l_trm, pOverflow);  /* function result */

    if (s == 0)
    {
        st->past_gain = 0;
        return;
    }
    exp = norm_l(s) - 1;

    L_temp = L_shl(s, exp, pOverflow);
    gain_out = pv_round(L_temp, pOverflow);

    /* calculate gain_in with exponent */
    s = energy_new(sig_in, l_trm, pOverflow);    /* function result */

    if (s == 0)
    {
        g0 = 0;
    }
    else
    {
        i = norm_l(s);

        /* L_temp = L_shl(s, i, pOverflow); */
        L_temp = s << i;

        gain_in = pv_round(L_temp, pOverflow);

        exp -= i;

        /*---------------------------------------------------*
         *  g0 = (1-agc_fac) * sqrt(gain_in/gain_out);       *
         *---------------------------------------------------*/

        /* s = gain_out / gain_in */
        temp = div_s(gain_out, gain_in);

        /* s = L_deposit_l (temp); */
        s = (Word32) temp;
        s = s << 7;
        s = L_shr(s, exp, pOverflow);      /* add exponent */

        s = Inv_sqrt(s, pOverflow);    /* function result */
        L_temp = s << 9;

        i = (Word16)((L_temp + (Word32) 0x00008000L) >> 16);

        /* g0 = i * (1-agc_fac) */
        temp = 32767 - agc_fac;

        g0 = (Word16)(((Word32) i * temp) >> 15);

    }

    /* compute gain[n] = agc_fac * gain[n-1]
                        + (1-agc_fac) * sqrt(gain_in/gain_out) */
    /* sig_out[n] = gain[n] * sig_out[n]                        */

    gain = st->past_gain;
    p_sig_out = sig_out;

    for (i = 0; i < l_trm; i++)
    {
        /* gain = mult (gain, agc_fac, pOverflow); */
        gain = (Word16)(((Word32) gain * agc_fac) >> 15);

        /* gain = add (gain, g0, pOverflow); */
        gain += g0;

        /* L_temp = L_mult (sig_out[i], gain, pOverflow); */
        L_temp = ((Word32)(*(p_sig_out)) * gain) << 1;

        *(p_sig_out++) = (Word16)(L_temp >> 13);
    }

    st->past_gain = gain;

    return;
}

/*--------------------------------------------------------------------------*/

/*
------------------------------------------------------------------------------
 FUNCTION NAME: agc2
------------------------------------------------------------------------------
 INPUT AND OUTPUT DEFINITIONS

 Inputs:
    sig_in = pointer to a buffer containing the postfilter input signal
    sig_out = pointer to a buffer containing the postfilter output signal
    l_trm = subframe size
    pOverflow = pointer to overflow flag

 Outputs:
    sig_out points to a buffer containing the new scaled output signal.
    pOverflow -> 1 if the agc computation saturates

 Returns:
    None.

 Global Variables Used:
    None.

 Local Variables Needed:
    None.

------------------------------------------------------------------------------
 FUNCTION DESCRIPTION

 Scales the excitation on a subframe basis.

------------------------------------------------------------------------------
 REQUIREMENTS

 None.

------------------------------------------------------------------------------
 REFERENCES

 agc.c, UMTS GSM AMR speech codec, R99 - Version 3.2.0, March 2, 2001

------------------------------------------------------------------------------
 PSEUDO-CODE

void agc2 (
 Word16 *sig_in,        // i   : postfilter input signal
 Word16 *sig_out,       // i/o : postfilter output signal
 Word16 l_trm           // i   : subframe size
)
{
    Word16 i, exp;
    Word16 gain_in, gain_out, g0;
    Word32 s;

    // calculate gain_out with exponent
    s = energy_new(sig_out, l_trm); // function result

    if (s == 0)
    {
        return;
    }
    exp = sub (norm_l (s), 1);
    gain_out = pv_round (L_shl (s, exp));

    // calculate gain_in with exponent
    s = energy_new(sig_in, l_trm); // function result

    if (s == 0)
    {
        g0 = 0;
    }
    else
    {
        i = norm_l (s);
        gain_in = pv_round (L_shl (s, i));
        exp = sub (exp, i);

         *---------------------------------------------------*
         *  g0 = sqrt(gain_in/gain_out);                     *
         *---------------------------------------------------*

        s = L_deposit_l (div_s (gain_out, gain_in));
        s = L_shl (s, 7);       // s = gain_out / gain_in
        s = L_shr (s, exp);     // add exponent

        s = Inv_sqrt (s); // function result
        g0 = pv_round (L_shl (s, 9));
    }

    // sig_out(n) = gain(n) sig_out(n)

    for (i = 0; i < l_trm; i++)
    {
        sig_out[i] = extract_h (L_shl (L_mult (sig_out[i], g0), 3));
    }

    return;
}
------------------------------------------------------------------------------
 CAUTION [optional]
 [State any special notes, constraints or cautions for users of this function]

------------------------------------------------------------------------------
*/

void agc2(
    Word16 *sig_in,        /* i   : postfilter input signal  */
    Word16 *sig_out,       /* i/o : postfilter output signal */
    Word16 l_trm,          /* i   : subframe size            */
    Flag   *pOverflow      /* i   : overflow flag            */
)

{
    Word16  i;
    Word16  exp;
    Word16  gain_in;
    Word16  gain_out;
    Word16  g0;
    Word32  s;
    Word32  L_temp;
    Word16  temp;

    /* calculate gain_out with exponent */
    s = energy_new(sig_out, l_trm, pOverflow); /* function result */

    if (s == 0)
    {
        return;
    }
    exp = norm_l(s) - 1;
    L_temp = L_shl(s, exp, pOverflow);
    gain_out = pv_round(L_temp, pOverflow);

    /* calculate gain_in with exponent */
    s = energy_new(sig_in, l_trm, pOverflow); /* function result */

    if (s == 0)
    {
        g0 = 0;
    }
    else
    {
        i = norm_l(s);
        L_temp = L_shl(s, i, pOverflow);
        gain_in = pv_round(L_temp, pOverflow);
        exp -= i;

        /*---------------------------------------------------*
         *  g0 = sqrt(gain_in/gain_out);                     *
         *---------------------------------------------------*/

        /* s = gain_out / gain_in */
        temp = div_s(gain_out, gain_in);

        /* s = L_deposit_l (temp); */
        s = (Word32)temp;

        if (s > (Word32) 0x00FFFFFFL)
        {
            s = MAX_32;
        }
        else if (s < (Word32) - 16777216)
        {
            s = MIN_32;
        }
        else
        {
            s = s << 7;
        }
        s = L_shr(s, exp, pOverflow);      /* add exponent */

        s = Inv_sqrt(s, pOverflow);    /* function result */

        if (s > (Word32) 0x003FFFFFL)
        {
            L_temp = MAX_32;
        }
        else if (s < (Word32) - 4194304)
        {
            L_temp = MIN_32;
        }
        else
        {
            L_temp = s << 9;
        }
        g0 = pv_round(L_temp, pOverflow);
    }

    /* sig_out(n) = gain(n) sig_out(n) */

    for (i = l_trm - 1; i >= 0; i--)
    {
        L_temp = L_mult(sig_out[i], g0, pOverflow);
        if (L_temp > (Word32) 0x0FFFFFFFL)
        {
            sig_out[i] = MAX_16;
        }
        else if (L_temp < (Word32) - 268435456)
        {
            sig_out[i] = MIN_16;
        }
        else
        {
            sig_out[i] = (Word16)(L_temp >> 13);
        }
    }

    return;
}
