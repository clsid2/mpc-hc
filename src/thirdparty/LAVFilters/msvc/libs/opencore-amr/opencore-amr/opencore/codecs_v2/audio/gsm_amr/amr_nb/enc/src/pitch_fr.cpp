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



 Filename: pitch_fr.cpp

------------------------------------------------------------------------------
 MODULE DESCRIPTION

      File             : pitch_fr.c
      Purpose          : Find the pitch period with 1/3 or 1/6 subsample
                       : resolution (closed loop).

------------------------------------------------------------------------------
*/

/*----------------------------------------------------------------------------
; INCLUDES
----------------------------------------------------------------------------*/
#include "pitch_fr.h"
#include "oper_32b.h"
#include "cnst.h"
#include "enc_lag3.h"
#include "enc_lag6.h"
#include "inter_36.h"
#include "inv_sqrt.h"
#include "convolve.h"

#include "basic_op.h"
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
 * mode dependent parameters used in Pitch_fr()
 * Note: order of MRxx in 'enum Mode' is important!
 */
static const struct
{
    Word16 max_frac_lag;     /* lag up to which fractional lags are used    */
    Word16 flag3;            /* enable 1/3 instead of 1/6 fract. resolution */
    Word16 first_frac;       /* first fractional to check                   */
    Word16 last_frac;        /* last fractional to check                    */
    Word16 delta_int_low;    /* integer lag below TO to start search from   */
    Word16 delta_int_range;  /* integer range around T0                     */
    Word16 delta_frc_low;    /* fractional below T0                         */
    Word16 delta_frc_range;  /* fractional range around T0                  */
    Word16 pit_min;          /* minimum pitch                               */
} mode_dep_parm[N_MODES] =
{
    /* MR475 */  { 84,  1, -2,  2,  5, 10,  5,  9, PIT_MIN },
    /* MR515 */  { 84,  1, -2,  2,  5, 10,  5,  9, PIT_MIN },
    /* MR59  */  { 84,  1, -2,  2,  3,  6,  5,  9, PIT_MIN },
    /* MR67  */  { 84,  1, -2,  2,  3,  6,  5,  9, PIT_MIN },
    /* MR74  */  { 84,  1, -2,  2,  3,  6,  5,  9, PIT_MIN },
    /* MR795 */  { 84,  1, -2,  2,  3,  6, 10, 19, PIT_MIN },
    /* MR102 */  { 84,  1, -2,  2,  3,  6,  5,  9, PIT_MIN },
    /* MR122 */  { 94,  0, -3,  3,  3,  6,  5,  9, PIT_MIN_MR122 }
};

/*
------------------------------------------------------------------------------
 FUNCTION NAME: Norm_Corr
------------------------------------------------------------------------------
 INPUT AND OUTPUT DEFINITIONS

 Inputs:
    exc[] = pointer to buffer of type Word16
    xn[]  = pointer to buffer of type Word16
    h[]   = pointer to buffer of type Word16
    L_subfr = length of sub frame (Word16)
    t_min  = the minimum table value of type Word16
    t_max = the maximum table value of type Word16
    corr_norm[] = pointer to buffer of type Word16

 Outputs:
    pOverflow = 1 if the math functions called result in overflow else zero.

 Returns:
    None

 Global Variables Used:
    None

 Local Variables Needed:
    None

------------------------------------------------------------------------------
 FUNCTION DESCRIPTION

  FUNCTION:   Norm_Corr()

  PURPOSE: Find the normalized correlation between the target vector
           and the filtered past excitation.

  DESCRIPTION:
     The normalized correlation is given by the correlation between the
     target and filtered past excitation divided by the square root of
     the energy of filtered excitation.
                   corr[k] = <x[], y_k[]>/sqrt(y_k[],y_k[])
     where x[] is the target vector and y_k[] is the filtered past
     excitation at delay k.


------------------------------------------------------------------------------
 REQUIREMENTS

 None

------------------------------------------------------------------------------
 REFERENCES

 pitch_fr.c, UMTS GSM AMR speech codec, R99 - Version 3.2.0, March 2, 2001

------------------------------------------------------------------------------
 PSEUDO-CODE

static void Norm_Corr (Word16 exc[], Word16 xn[], Word16 h[], Word16 L_subfr,
                       Word16 t_min, Word16 t_max, Word16 corr_norm[])
{
    Word16 i, j, k;
    Word16 corr_h, corr_l, norm_h, norm_l;
    Word32 s;

    // Usally dynamic allocation of (L_subfr)
    Word16 excf[L_SUBFR];
    Word16 scaling, h_fac, *s_excf, scaled_excf[L_SUBFR];

    k = -t_min;

    // compute the filtered excitation for the first delay t_min

    Convolve (&exc[k], h, excf, L_subfr);

    // scale "excf[]" to avoid overflow

    for (j = 0; j < L_subfr; j++) {
        scaled_excf[j] = shr (excf[j], 2);
    }

    // Compute 1/sqrt(energy of excf[])

    s = 0;
    for (j = 0; j < L_subfr; j++) {
        s = L_mac (s, excf[j], excf[j]);
    }
    if (L_sub (s, 67108864L) <= 0) {            // if (s <= 2^26)
        s_excf = excf;
        h_fac = 15 - 12;
        scaling = 0;
    }
    else {
        // "excf[]" is divided by 2
        s_excf = scaled_excf;
        h_fac = 15 - 12 - 2;
        scaling = 2;
    }

    // loop for every possible period

    for (i = t_min; i <= t_max; i++) {
        // Compute 1/sqrt(energy of excf[])

        s = 0;
        for (j = 0; j < L_subfr; j++) {
            s = L_mac (s, s_excf[j], s_excf[j]);
        }

        s = Inv_sqrt (s);
        L_Extract (s, &norm_h, &norm_l);

        // Compute correlation between xn[] and excf[]

        s = 0;
        for (j = 0; j < L_subfr; j++) {
            s = L_mac (s, xn[j], s_excf[j]);
        }
        L_Extract (s, &corr_h, &corr_l);

        // Normalize correlation = correlation * (1/sqrt(energy))

        s = Mpy_32 (corr_h, corr_l, norm_h, norm_l);

        corr_norm[i] = extract_h (L_shl (s, 16));

            // modify the filtered excitation excf[] for the next iteration

        if (sub (i, t_max) != 0) {
            k--;
            for (j = L_subfr - 1; j > 0; j--) {
                s = L_mult (exc[k], h[j]);
                s = L_shl (s, h_fac);
                s_excf[j] = add (extract_h (s), s_excf[j - 1]);
            }
            s_excf[0] = shr (exc[k], scaling);
        }
    }
    return;
}

------------------------------------------------------------------------------
 CAUTION [optional]
 [State any special notes, constraints or cautions for users of this function]

------------------------------------------------------------------------------
*/

static void Norm_Corr(Word16 exc[],
                      Word16 xn[],
                      Word16 h[],
                      Word16 L_subfr,
                      Word16 t_min,
                      Word16 t_max,
                      Word16 corr_norm[],
                      Flag *pOverflow)
{
    Word16 i;
    Word16 j;
    Word16 k;
    Word16 corr_h;
    Word16 corr_l;
    Word16 norm_h;
    Word16 norm_l;
    Word32 s;
    Word32 s2;
    Word16 excf[L_SUBFR];
    Word16 scaling;
    Word16 h_fac;
    Word16 *s_excf;
    Word16 scaled_excf[L_SUBFR];
    Word16 *p_s_excf;
    Word16 *p_excf;
    Word16  temp;
    Word16 *p_x;
    Word16 *p_h;

    k = -t_min;

    /* compute the filtered excitation for the first delay t_min */

    Convolve(&exc[k], h, excf, L_subfr);

    /* scale "excf[]" to avoid overflow */
    s = 0;
    p_s_excf = scaled_excf;
    p_excf   = excf;

    for (j = (L_subfr >> 1); j != 0; j--)
    {
        temp = *(p_excf++);
        *(p_s_excf++) = temp >> 2;
        s += (Word32) temp * temp;
        temp = *(p_excf++);
        *(p_s_excf++) = temp >> 2;
        s += (Word32) temp * temp;
    }


    if (s <= (67108864L >> 1))
    {
        s_excf = excf;
        h_fac = 12;
        scaling = 0;
    }
    else
    {
        /* "excf[]" is divided by 2 */
        s_excf = scaled_excf;
        h_fac = 14;
        scaling = 2;
    }

    /* loop for every possible period */

    for (i = t_min; i <= t_max; i++)
    {
        /* Compute 1/sqrt(energy of excf[]) */

        s   = s2 = 0;
        p_x      = xn;
        p_s_excf = s_excf;
        j        = L_subfr >> 1;

        while (j--)
        {
            s  += (Word32) * (p_x++) * *(p_s_excf);
            s2 += ((Word32)(*(p_s_excf)) * (*(p_s_excf)));
            p_s_excf++;
            s  += (Word32) * (p_x++) * *(p_s_excf);
            s2 += ((Word32)(*(p_s_excf)) * (*(p_s_excf)));
            p_s_excf++;
        }

        s2     = s2 << 1;
        s2     = Inv_sqrt(s2, pOverflow);
        norm_h = (Word16)(s2 >> 16);
        norm_l = (Word16)((s2 >> 1) - (norm_h << 15));
        corr_h = (Word16)(s >> 15);
        corr_l = (Word16)((s) - (corr_h << 15));

        /* Normalize correlation = correlation * (1/sqrt(energy)) */

        s = Mpy_32(corr_h, corr_l, norm_h, norm_l, pOverflow);

        corr_norm[i] = (Word16) s ;

        /* modify the filtered excitation excf[] for the next iteration */
        if (i != t_max)
        {
            k--;
            temp = exc[k];
            p_s_excf = &s_excf[L_subfr - 1];
            p_h = &h[L_subfr - 1];

            p_excf = &s_excf[L_subfr - 2];
            for (j = (L_subfr - 1) >> 1; j != 0; j--)
            {
                s = ((Word32) temp * *(p_h--)) >> h_fac;
                *(p_s_excf--) = (Word16) s  + *(p_excf--);
                s = ((Word32) temp * *(p_h--)) >> h_fac;
                *(p_s_excf--) = (Word16) s  + *(p_excf--);
            }

            s = ((Word32) temp * *(p_h)) >> h_fac;
            *(p_s_excf--) = (Word16) s  + *(p_excf);

            *(p_s_excf) = temp >> scaling;
        }

    }
    return;
}

/****************************************************************************/


/*
------------------------------------------------------------------------------
 FUNCTION NAME: searchFrac
------------------------------------------------------------------------------
 INPUT AND OUTPUT DEFINITIONS

 Inputs:
    lag = pointer to integer pitch of type Word16
    frac = pointer to starting point of search fractional pitch of type Word16
    last_frac = endpoint of search  of type Word16
    corr[] = pointer to normalized correlation of type Word16
    flag3 = subsample resolution (3: =1 / 6: =0) of type Word16

 Outputs:
    None

 Returns:
    None

 Global Variables Used:
    None

 Local Variables Needed:
    None

------------------------------------------------------------------------------
 FUNCTION DESCRIPTION

   FUNCTION:   searchFrac()

   PURPOSE: Find fractional pitch

   DESCRIPTION:
      The function interpolates the normalized correlation at the
      fractional positions around lag T0. The position at which the
      interpolation function reaches its maximum is the fractional pitch.
      Starting point of the search is frac, end point is last_frac.
      frac is overwritten with the fractional pitch.

------------------------------------------------------------------------------
 REQUIREMENTS

 None

------------------------------------------------------------------------------
 REFERENCES

 pitch_fr.c, UMTS GSM AMR speech codec, R99 - Version 3.2.0, March 2, 2001

------------------------------------------------------------------------------
 PSEUDO-CODE

static void searchFrac (
    Word16 *lag,       // i/o : integer pitch
    Word16 *frac,      // i/o : start point of search -
                               fractional pitch
    Word16 last_frac,  // i   : endpoint of search
    Word16 corr[],     // i   : normalized correlation
    Word16 flag3       // i   : subsample resolution
                                (3: =1 / 6: =0)
)
{
    Word16 i;
    Word16 max;
    Word16 corr_int;

    // Test the fractions around T0 and choose the one which maximizes
    // the interpolated normalized correlation.

    max = Interpol_3or6 (&corr[*lag], *frac, flag3); // function result

    for (i = add (*frac, 1); i <= last_frac; i++) {
        corr_int = Interpol_3or6 (&corr[*lag], i, flag3);
        if (sub (corr_int, max) > 0) {
            max = corr_int;
            *frac = i;
        }
    }

    if (flag3 == 0) {
        // Limit the fraction value in the interval [-2,-1,0,1,2,3]

        if (sub (*frac, -3) == 0) {
            *frac = 3;
            *lag = sub (*lag, 1);
        }
    }
    else {
        // limit the fraction value between -1 and 1

        if (sub (*frac, -2) == 0) {
            *frac = 1;
            *lag = sub (*lag, 1);
        }
        if (sub (*frac, 2) == 0) {
            *frac = -1;
            *lag = add (*lag, 1);
        }
    }
}

------------------------------------------------------------------------------
 CAUTION [optional]
 [State any special notes, constraints or cautions for users of this function]

------------------------------------------------------------------------------
*/

static void searchFrac(
    Word16 *lag,       /* i/o : integer pitch           */
    Word16 *frac,      /* i/o : start point of search -
                                fractional pitch        */
    Word16 last_frac,  /* i   : endpoint of search      */
    Word16 corr[],     /* i   : normalized correlation  */
    Word16 flag3,      /* i   : subsample resolution
                                (3: =1 / 6: =0)         */
    Flag   *pOverflow
)
{
    Word16 i;
    Word16 max;
    Word16 corr_int;

    /* Test the fractions around T0 and choose the one which maximizes   */
    /* the interpolated normalized correlation.                          */

    max = Interpol_3or6(&corr[*lag], *frac, flag3, pOverflow);
    /* function result */

    for (i = *frac + 1; i <= last_frac; i++)
    {
        corr_int = Interpol_3or6(&corr[*lag], i, flag3, pOverflow);
        if (corr_int > max)
        {
            max = corr_int;
            *frac = i;
        }
    }

    if (flag3 == 0)
    {
        /* Limit the fraction value in the interval [-2,-1,0,1,2,3] */

        if (*frac == -3)
        {
            *frac = 3;
            (*lag)--;
        }
    }
    else
    {
        /* limit the fraction value between -1 and 1 */

        if (*frac == -2)
        {
            *frac = 1;
            (*lag)--;
        }
        if (*frac == 2)
        {
            *frac = -1;
            (*lag)++;
        }
    }
}

/****************************************************************************/


/*
------------------------------------------------------------------------------
 FUNCTION NAME: getRange
------------------------------------------------------------------------------
 INPUT AND OUTPUT DEFINITIONS

 Inputs:
    T0 = integer pitch of type Word16
    delta_low = search start offset of type Word16
    delta_range = search range of type Word16
    pitmin = minimum pitch of type Word16
    pitmax = maximum pitch of type Word16
    t0_min = search range minimum of type Word16
    t0_max = search range maximum of type Word16

 Outputs:
    pOverflow = 1 if the math functions called result in overflow else zero.

 Returns:
    None

 Global Variables Used:
    None

 Local Variables Needed:
    None

------------------------------------------------------------------------------
 FUNCTION DESCRIPTION

   FUNCTION:   getRange()

   PURPOSE: Sets range around open-loop pitch or integer pitch of last subframe

   DESCRIPTION:
      Takes integer pitch T0 and calculates a range around it with
        t0_min = T0-delta_low  and t0_max = (T0-delta_low) + delta_range
      t0_min and t0_max are bounded by pitmin and pitmax
------------------------------------------------------------------------------
 REQUIREMENTS

 None

------------------------------------------------------------------------------
 REFERENCES

 pitch_fr.c, UMTS GSM AMR speech codec, R99 - Version 3.2.0, March 2, 2001

------------------------------------------------------------------------------
 PSEUDO-CODE

static void getRange (
    Word16 T0,           // i : integer pitch
    Word16 delta_low,    // i : search start offset
    Word16 delta_range,  // i : search range
    Word16 pitmin,       // i : minimum pitch
    Word16 pitmax,       // i : maximum pitch
    Word16 *t0_min,      // o : search range minimum
    Word16 *t0_max)      // o : search range maximum
{
    *t0_min = sub(T0, delta_low);
    if (sub(*t0_min, pitmin) < 0) {
        *t0_min = pitmin;
    }
    *t0_max = add(*t0_min, delta_range);
    if (sub(*t0_max, pitmax) > 0) {
        *t0_max = pitmax;
        *t0_min = sub(*t0_max, delta_range);
    }
}

------------------------------------------------------------------------------
 CAUTION [optional]
 [State any special notes, constraints or cautions for users of this function]

------------------------------------------------------------------------------
*/
static void getRange(
    Word16 T0,           /* i : integer pitch          */
    Word16 delta_low,    /* i : search start offset    */
    Word16 delta_range,  /* i : search range           */
    Word16 pitmin,       /* i : minimum pitch          */
    Word16 pitmax,       /* i : maximum pitch          */
    Word16 *t0_min,      /* o : search range minimum   */
    Word16 *t0_max,      /* o : search range maximum   */
    Flag   *pOverflow)
{

    Word16 temp;
    OSCL_UNUSED_ARG(pOverflow);

    temp = *t0_min;
    temp = T0 - delta_low;
    if (temp < pitmin)
    {
        temp = pitmin;
    }
    *t0_min = temp;

    temp +=  delta_range;
    if (temp > pitmax)
    {
        temp = pitmax;
        *t0_min = pitmax - delta_range;
    }
    *t0_max = temp;

}


/****************************************************************************/


/*
------------------------------------------------------------------------------
 FUNCTION NAME: Pitch_fr_init
------------------------------------------------------------------------------
 INPUT AND OUTPUT DEFINITIONS

 Inputs:
    state = pointer to a pointer of structure type Pitch_fr_State.

 Outputs:
    None

 Returns:
    Returns a zero if successful and -1 if not successful.

 Global Variables Used:
    None

 Local Variables Needed:
    None

------------------------------------------------------------------------------
 FUNCTION DESCRIPTION

  Function:   Pitch_fr_init
  Purpose:    Allocates state memory and initializes state memory

------------------------------------------------------------------------------
 REQUIREMENTS

 None

------------------------------------------------------------------------------
 REFERENCES

 pitch_fr.c, UMTS GSM AMR speech codec, R99 - Version 3.2.0, March 2, 2001

------------------------------------------------------------------------------
 PSEUDO-CODE

int Pitch_fr_init (Pitch_frState **state)
{
    Pitch_frState* s;

    if (state == (Pitch_frState **) NULL){
        // fprintf(stderr, "Pitch_fr_init: invalid parameter\n");
        return -1;
    }
    *state = NULL;

    // allocate memory
    if ((s= (Pitch_frState *) malloc(sizeof(Pitch_frState))) == NULL){
        // fprintf(stderr, "Pitch_fr_init: can not malloc state structure\n");
        return -1;
    }

    Pitch_fr_reset(s);
    *state = s;

    return 0;
}

------------------------------------------------------------------------------
 CAUTION [optional]
 [State any special notes, constraints or cautions for users of this function]

------------------------------------------------------------------------------
*/
Word16 Pitch_fr_init(Pitch_frState **state)
{
    Pitch_frState* s;

    if (state == (Pitch_frState **) NULL)
    {
        /* fprintf(stderr, "Pitch_fr_init: invalid parameter\n"); */
        return -1;
    }
    *state = NULL;

    /* allocate memory */
    if ((s = (Pitch_frState *) oscl_malloc(sizeof(Pitch_frState))) == NULL)
    {
        /* fprintf(stderr, "Pitch_fr_init: can not malloc state structure\n"); */
        return -1;
    }

    Pitch_fr_reset(s);
    *state = s;

    return 0;
}


/****************************************************************************/


/*
------------------------------------------------------------------------------
 FUNCTION NAME: Pitch_fr_reset
------------------------------------------------------------------------------
 INPUT AND OUTPUT DEFINITIONS

 Inputs:
    state = pointer to a pointer of structure type Pitch_fr_State.

 Outputs:
    None

 Returns:
    Returns a zero if successful and -1 if not successful.

 Global Variables Used:
    None

 Local Variables Needed:
    None

------------------------------------------------------------------------------
 FUNCTION DESCRIPTION

  Function:   Pitch_fr_reset
  Purpose:    Initializes state memory to zero

------------------------------------------------------------------------------
 REQUIREMENTS

 None

------------------------------------------------------------------------------
 REFERENCES

 pitch_fr.c, UMTS GSM AMR speech codec, R99 - Version 3.2.0, March 2, 2001

------------------------------------------------------------------------------
 PSEUDO-CODE

int Pitch_fr_reset (Pitch_frState *state)
{

    if (state == (Pitch_frState *) NULL){
        // fprintf(stderr, "Pitch_fr_reset: invalid parameter\n");
        return -1;
    }

    state->T0_prev_subframe = 0;

    return 0;
}

------------------------------------------------------------------------------
 CAUTION [optional]
 [State any special notes, constraints or cautions for users of this function]

------------------------------------------------------------------------------
*/
Word16 Pitch_fr_reset(Pitch_frState *state)
{

    if (state == (Pitch_frState *) NULL)
    {
        /* fprintf(stderr, "Pitch_fr_reset: invalid parameter\n"); */
        return -1;
    }

    state->T0_prev_subframe = 0;

    return 0;
}


/****************************************************************************/


/*
------------------------------------------------------------------------------
 FUNCTION NAME: Pitch_fr_exit
------------------------------------------------------------------------------
 INPUT AND OUTPUT DEFINITIONS

 Inputs:
    state = pointer to a pointer of structure type Pitch_fr_State.

 Outputs:
    None

 Returns:
    None

 Global Variables Used:
    None

 Local Variables Needed:
    None

------------------------------------------------------------------------------
 FUNCTION DESCRIPTION

  Function:   Pitch_fr_exit
  Purpose:    The memory for state is freed.

------------------------------------------------------------------------------
 REQUIREMENTS

 None

------------------------------------------------------------------------------
 REFERENCES

 pitch_fr.c, UMTS GSM AMR speech codec, R99 - Version 3.2.0, March 2, 2001

------------------------------------------------------------------------------
 PSEUDO-CODE

void Pitch_fr_exit (Pitch_frState **state)
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
void Pitch_fr_exit(Pitch_frState **state)
{
    if (state == NULL || *state == NULL)
        return;

    /* deallocate memory */
    oscl_free(*state);
    *state = NULL;

    return;
}

/****************************************************************************/


/*
------------------------------------------------------------------------------
 FUNCTION NAME: Pitch_fr
------------------------------------------------------------------------------
 INPUT AND OUTPUT DEFINITIONS

 Inputs:
    st = pointer to stat structure of type Pitch_frState
    mode = codec mode of type enum Mode
    T_op[] = pointer to open loop pitch lags of type Word16
    exc[] = pointer to excitation buffer of type Word16
    xn[] = pointer to target vector of type Word16
    h[] = pointer to impulse response of synthesis and weighting filters
          of type Word16
    L_subfr = length of subframe of type Word16
    i_subfr = subframe offset of type Word16

 Outputs:
    pit_frac = pointer to pitch period (fractional) of type Word16
    resu3 = pointer to subsample resolution of type Word16
    ana_index = pointer to index of encoding of type Word16

 Returns:
    None

 Global Variables Used:
    None

 Local Variables Needed:
    None

------------------------------------------------------------------------------
 FUNCTION DESCRIPTION

   FUNCTION:   Pitch_fr()

   PURPOSE: Find the pitch period with 1/3 or 1/6 subsample resolution
            (closed loop).

   DESCRIPTION:
         - find the normalized correlation between the target and filtered
           past excitation in the search range.
         - select the delay with maximum normalized correlation.
         - interpolate the normalized correlation at fractions -3/6 to 3/6
           with step 1/6 around the chosen delay.
         - The fraction which gives the maximum interpolated value is chosen.

------------------------------------------------------------------------------
 REQUIREMENTS

 None

------------------------------------------------------------------------------
 REFERENCES

 pitch_fr.c, UMTS GSM AMR speech codec, R99 - Version 3.2.0, March 2, 2001

------------------------------------------------------------------------------
 PSEUDO-CODE

Word16 Pitch_fr (        // o   : pitch period (integer)
    Pitch_frState *st,   // i/o : State struct
    enum Mode mode,      // i   : codec mode
    Word16 T_op[],       // i   : open loop pitch lags
    Word16 exc[],        // i   : excitation buffer                      Q0
    Word16 xn[],         // i   : target vector                          Q0
    Word16 h[],          // i   : impulse response of synthesis and
                                  weighting filters                     Q12
    Word16 L_subfr,      // i   : Length of subframe
    Word16 i_subfr,      // i   : subframe offset
    Word16 *pit_frac,    // o   : pitch period (fractional)
    Word16 *resu3,       // o   : subsample resolution 1/3 (=1) or 1/6 (=0)
    Word16 *ana_index    // o   : index of encoding
)
{
    Word16 i;
    Word16 t_min, t_max;
    Word16 t0_min, t0_max;
    Word16 max, lag, frac;
    Word16 tmp_lag;
    Word16 *corr;
    Word16 corr_v[40];    // Total length = t0_max-t0_min+1+2*L_INTER_SRCH

    Word16 max_frac_lag;
    Word16 flag3, flag4;
    Word16 last_frac;
    Word16 delta_int_low, delta_int_range;
    Word16 delta_frc_low, delta_frc_range;
    Word16 pit_min;
    Word16 frame_offset;
    Word16 delta_search;

    //-----------------------------------------------------------------------
     //                      set mode specific variables
     //----------------------------------------------------------------------

    max_frac_lag    = mode_dep_parm[mode].max_frac_lag;
    flag3           = mode_dep_parm[mode].flag3;
    frac            = mode_dep_parm[mode].first_frac;
    last_frac       = mode_dep_parm[mode].last_frac;
    delta_int_low   = mode_dep_parm[mode].delta_int_low;
    delta_int_range = mode_dep_parm[mode].delta_int_range;

    delta_frc_low   = mode_dep_parm[mode].delta_frc_low;
    delta_frc_range = mode_dep_parm[mode].delta_frc_range;
    pit_min         = mode_dep_parm[mode].pit_min;

    //-----------------------------------------------------------------------
    //                 decide upon full or differential search
    //-----------------------------------------------------------------------

    delta_search = 1;

    if ((i_subfr == 0) || (sub(i_subfr,L_FRAME_BY2) == 0)) {

        // Subframe 1 and 3

        if (((sub((Word16)mode, (Word16)MR475) != 0) && (sub((Word16)mode,
            (Word16)MR515) != 0)) ||
            (sub(i_subfr,L_FRAME_BY2) != 0)) {

            // set t0_min, t0_max for full search
            // this is *not* done for mode MR475, MR515 in subframe 3

            delta_search = 0; // no differential search

            // calculate index into T_op which contains the open-loop
            // pitch estimations for the 2 big subframes

            frame_offset = 1;
            if (i_subfr == 0)
                frame_offset = 0;

            // get T_op from the corresponding half frame and
            // set t0_min, t0_max

            getRange (T_op[frame_offset], delta_int_low, delta_int_range,
                      pit_min, PIT_MAX, &t0_min, &t0_max);
        }
        else {

            // mode MR475, MR515 and 3. Subframe: delta search as well
            getRange (st->T0_prev_subframe, delta_frc_low, delta_frc_range,
                      pit_min, PIT_MAX, &t0_min, &t0_max);
        }
    }
    else {

        // for Subframe 2 and 4
        // get range around T0 of previous subframe for delta search

        getRange (st->T0_prev_subframe, delta_frc_low, delta_frc_range,
                  pit_min, PIT_MAX, &t0_min, &t0_max);
    }

    //-----------------------------------------------------------------------
                Find interval to compute normalized correlation
     -----------------------------------------------------------------------

    t_min = sub (t0_min, L_INTER_SRCH);
    t_max = add (t0_max, L_INTER_SRCH);

    corr = &corr_v[-t_min];

    //-----------------------------------------------------------------------
      Compute normalized correlation between target and filtered excitation
     -----------------------------------------------------------------------

    Norm_Corr (exc, xn, h, L_subfr, t_min, t_max, corr);

    //-----------------------------------------------------------------------
                                Find integer pitch
     -----------------------------------------------------------------------

    max = corr[t0_min];
    lag = t0_min;

    for (i = t0_min + 1; i <= t0_max; i++) {
        if (sub (corr[i], max) >= 0) {
            max = corr[i];
            lag = i;
        }
    }

    //-----------------------------------------------------------------------
                             Find fractional pitch
     -----------------------------------------------------------------------
    if ((delta_search == 0) && (sub (lag, max_frac_lag) > 0)) {

        // full search and integer pitch greater than max_frac_lag
        // fractional search is not needed, set fractional to zero

        frac = 0;
    }
    else {

        // if differential search AND mode MR475 OR MR515 OR MR59 OR MR67
        // then search fractional with 4 bits resolution

       if ((delta_search != 0) &&
           ((sub ((Word16)mode, (Word16)MR475) == 0) ||
            (sub ((Word16)mode, (Word16)MR515) == 0) ||
            (sub ((Word16)mode, (Word16)MR59) == 0) ||
            (sub ((Word16)mode, (Word16)MR67) == 0))) {

          // modify frac or last_frac according to position of last
          // integer pitch: either search around integer pitch,
          // or only on left or right side

          tmp_lag = st->T0_prev_subframe;
          if ( sub( sub(tmp_lag, t0_min), 5) > 0)
             tmp_lag = add (t0_min, 5);
          if ( sub( sub(t0_max, tmp_lag), 4) > 0)
               tmp_lag = sub (t0_max, 4);

          if ((sub (lag, tmp_lag) == 0) ||
              (sub (lag, sub(tmp_lag, 1)) == 0)) {

             // normal search in fractions around T0

             searchFrac (&lag, &frac, last_frac, corr, flag3);

          }
          else if (sub (lag, sub (tmp_lag, 2)) == 0) {
             // limit search around T0 to the right side
             frac = 0;
             searchFrac (&lag, &frac, last_frac, corr, flag3);
          }
          else if (sub (lag, add(tmp_lag, 1)) == 0) {
             // limit search around T0 to the left side
             last_frac = 0;
             searchFrac (&lag, &frac, last_frac, corr, flag3);
          }
          else {
             // no fractional search
             frac = 0;
            }
       }
       else
          // test the fractions around T0
          searchFrac (&lag, &frac, last_frac, corr, flag3);
    }

    //-----------------------------------------------------------------------
     //                           encode pitch
     //-----------------------------------------------------------------------

    if (flag3 != 0) {
       // flag4 indicates encoding with 4 bit resolution;
       // this is needed for mode MR475, MR515 and MR59

       flag4 = 0;
       if ( (sub ((Word16)mode, (Word16)MR475) == 0) ||
            (sub ((Word16)mode, (Word16)MR515) == 0) ||
            (sub ((Word16)mode, (Word16)MR59) == 0) ||
            (sub ((Word16)mode, (Word16)MR67) == 0) ) {
          flag4 = 1;
       }

       // encode with 1/3 subsample resolution

       *ana_index = Enc_lag3(lag, frac, st->T0_prev_subframe,
                             t0_min, t0_max, delta_search, flag4);
       // function result

    }
    else
    {
       // encode with 1/6 subsample resolution

       *ana_index = Enc_lag6(lag, frac, t0_min, delta_search);
       // function result
    }

     //-----------------------------------------------------------------------
     //                          update state variables
     //-----------------------------------------------------------------------

    st->T0_prev_subframe = lag;

     //-----------------------------------------------------------------------
     //                      update output variables
     //-----------------------------------------------------------------------

    *resu3    = flag3;

    *pit_frac = frac;

    return (lag);
}


------------------------------------------------------------------------------
 CAUTION [optional]
 [State any special notes, constraints or cautions for users of this function]

------------------------------------------------------------------------------
*/
Word16 Pitch_fr(         /* o   : pitch period (integer)                    */
    Pitch_frState *st,   /* i/o : State struct                              */
    enum Mode mode,      /* i   : codec mode                                */
    Word16 T_op[],       /* i   : open loop pitch lags                      */
    Word16 exc[],        /* i   : excitation buffer                      Q0 */
    Word16 xn[],         /* i   : target vector                          Q0 */
    Word16 h[],          /* i   : impulse response of synthesis and
                                  weighting filters                     Q12 */
    Word16 L_subfr,      /* i   : Length of subframe                        */
    Word16 i_subfr,      /* i   : subframe offset                           */
    Word16 *pit_frac,    /* o   : pitch period (fractional)                 */
    Word16 *resu3,       /* o   : subsample resolution 1/3 (=1) or 1/6 (=0) */
    Word16 *ana_index,   /* o   : index of encoding                         */
    Flag   *pOverflow
)
{
    Word16 i;
    Word16 t_min;
    Word16 t_max;
    Word16 t0_min = 0;
    Word16 t0_max;
    Word16 max;
    Word16 lag;
    Word16 frac;
    Word16 tmp_lag;
    Word16 *corr;
    Word16 corr_v[40];    /* Total length = t0_max-t0_min+1+2*L_INTER_SRCH */

    Word16 max_frac_lag;
    Word16 flag3;
    Word16 flag4;
    Word16 last_frac;
    Word16 delta_int_low;
    Word16 delta_int_range;
    Word16 delta_frc_low;
    Word16 delta_frc_range;
    Word16 pit_min;
    Word16 frame_offset;
    Word16 delta_search;

    /*-----------------------------------------------------------------------*
     *                      set mode specific variables                      *
     *-----------------------------------------------------------------------*/

    max_frac_lag    = mode_dep_parm[mode].max_frac_lag;
    flag3           = mode_dep_parm[mode].flag3;
    frac            = mode_dep_parm[mode].first_frac;
    last_frac       = mode_dep_parm[mode].last_frac;
    delta_int_low   = mode_dep_parm[mode].delta_int_low;
    delta_int_range = mode_dep_parm[mode].delta_int_range;

    delta_frc_low   = mode_dep_parm[mode].delta_frc_low;
    delta_frc_range = mode_dep_parm[mode].delta_frc_range;
    pit_min         = mode_dep_parm[mode].pit_min;

    /*-----------------------------------------------------------------------*
     *                 decide upon full or differential search               *
     *-----------------------------------------------------------------------*/

    delta_search = 1;

    if ((i_subfr == 0) || (i_subfr == L_FRAME_BY2))
    {

        /* Subframe 1 and 3 */

        if (((mode != MR475) && (mode != MR515)) || (i_subfr != L_FRAME_BY2))
        {

            /* set t0_min, t0_max for full search */
            /* this is *not* done for mode MR475, MR515 in subframe 3 */

            delta_search = 0; /* no differential search */

            /* calculate index into T_op which contains the open-loop */
            /* pitch estimations for the 2 big subframes */

            frame_offset = 1;
            if (i_subfr == 0)
                frame_offset = 0;

            /* get T_op from the corresponding half frame and */
            /* set t0_min, t0_max */

            getRange(T_op[frame_offset], delta_int_low, delta_int_range,
                     pit_min, PIT_MAX, &t0_min, &t0_max, pOverflow);
        }
        else
        {

            /* mode MR475, MR515 and 3. Subframe: delta search as well */
            getRange(st->T0_prev_subframe, delta_frc_low, delta_frc_range,
                     pit_min, PIT_MAX, &t0_min, &t0_max, pOverflow);
        }
    }
    else
    {

        /* for Subframe 2 and 4 */
        /* get range around T0 of previous subframe for delta search */

        getRange(st->T0_prev_subframe, delta_frc_low, delta_frc_range,
                 pit_min, PIT_MAX, &t0_min, &t0_max, pOverflow);
    }

    /*-----------------------------------------------------------------------*
     *           Find interval to compute normalized correlation             *
     *-----------------------------------------------------------------------*/

    t_min = t0_min - L_INTER_SRCH;
    t_max = t0_max + L_INTER_SRCH;

    corr = &corr_v[-t_min];

    /*-----------------------------------------------------------------------*
     * Compute normalized correlation between target and filtered excitation *
     *-----------------------------------------------------------------------*/

    Norm_Corr(exc, xn, h, L_subfr, t_min, t_max, corr, pOverflow);

    /*-----------------------------------------------------------------------*
     *                           Find integer pitch                          *
     *-----------------------------------------------------------------------*/

    max = corr[t0_min];
    lag = t0_min;

    for (i = t0_min + 1; i <= t0_max; i++)
    {
        if (corr[i] >= max)
        {
            max = corr[i];
            lag = i;
        }
    }

    /*-----------------------------------------------------------------------*
     *                        Find fractional pitch                          *
     *-----------------------------------------------------------------------*/
    if ((delta_search == 0) && (lag > max_frac_lag))
    {

        /* full search and integer pitch greater than max_frac_lag */
        /* fractional search is not needed, set fractional to zero */

        frac = 0;
    }
    else
    {

        /* if differential search AND mode MR475 OR MR515 OR MR59 OR MR67   */
        /* then search fractional with 4 bits resolution           */

        if ((delta_search != 0) &&
                ((mode == MR475) || (mode == MR515) ||
                 (mode == MR59) || (mode == MR67)))
        {

            /* modify frac or last_frac according to position of last */
            /* integer pitch: either search around integer pitch, */
            /* or only on left or right side */

            tmp_lag = st->T0_prev_subframe;
            if ((tmp_lag - t0_min) > 5)
            {
                tmp_lag = t0_min + 5;
            }
            if ((t0_max - tmp_lag) > 4)
            {
                tmp_lag = t0_max - 4;
            }

            if ((lag == tmp_lag) || (lag == (tmp_lag - 1)))
            {

                /* normal search in fractions around T0 */

                searchFrac(&lag, &frac, last_frac, corr, flag3, pOverflow);

            }
            else if (lag == (tmp_lag - 2))
            {
                /* limit search around T0 to the right side */
                frac = 0;
                searchFrac(&lag, &frac, last_frac, corr, flag3, pOverflow);
            }
            else if (lag == (tmp_lag + 1))
            {
                /* limit search around T0 to the left side */
                last_frac = 0;
                searchFrac(&lag, &frac, last_frac, corr, flag3, pOverflow);
            }
            else
            {
                /* no fractional search */
                frac = 0;
            }
        }
        else
            /* test the fractions around T0 */
            searchFrac(&lag, &frac, last_frac, corr, flag3, pOverflow);
    }

    /*-----------------------------------------------------------------------*
     *                           encode pitch                                *
     *-----------------------------------------------------------------------*/

    if (flag3 != 0)
    {
        /* flag4 indicates encoding with 4 bit resolution;         */
        /* this is needed for mode MR475, MR515 and MR59           */

        flag4 = 0;
        if ((mode == MR475) || (mode == MR515) ||
                (mode == MR59) || (mode == MR67))
        {
            flag4 = 1;
        }

        /* encode with 1/3 subsample resolution */

        *ana_index = Enc_lag3(lag, frac, st->T0_prev_subframe,
                              t0_min, t0_max, delta_search, flag4, pOverflow);
        /* function result */

    }
    else
    {
        /* encode with 1/6 subsample resolution */

        *ana_index = Enc_lag6(lag, frac, t0_min, delta_search, pOverflow);
        /* function result */
    }

    /*-----------------------------------------------------------------------*
     *                          update state variables                       *
     *-----------------------------------------------------------------------*/

    st->T0_prev_subframe = lag;

    /*-----------------------------------------------------------------------*
     *                      update output variables                          *
     *-----------------------------------------------------------------------*/

    *resu3    = flag3;

    *pit_frac = frac;

    return (lag);
}

