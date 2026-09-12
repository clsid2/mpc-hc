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

 Filename: gc_pred.cpp
 Functions:
            gc_pred_reset
            gc_pred
            gc_pred_update
            gc_pred_average_limited

------------------------------------------------------------------------------
 MODULE DESCRIPTION

 This file contains the functions that perform codebook gain MA prediction.

------------------------------------------------------------------------------
*/

/*----------------------------------------------------------------------------
; INCLUDES
----------------------------------------------------------------------------*/
#include "gc_pred.h"
#include "basicop_malloc.h"
#include "basic_op.h"
#include "cnst.h"
#include "log2.h"

/*----------------------------------------------------------------------------
; MACROS
; Define module specific macros here
----------------------------------------------------------------------------*/

/*----------------------------------------------------------------------------
; DEFINES
; Include all pre-processor statements here. Include conditional
; compile variables also.
----------------------------------------------------------------------------*/
#define NPRED 4  /* number of prediction taps */

/* average innovation energy.                               */
/* MEAN_ENER  = 36.0/constant, constant = 20*Log10(2)       */
#define MEAN_ENER_MR122  783741L  /* 36/(20*log10(2)) (Q17) */

/* minimum quantized energy: -14 dB */
#define MIN_ENERGY       -14336       /* 14                 Q10 */
#define MIN_ENERGY_MR122  -2381       /* 14 / (20*log10(2)) Q10 */

/*----------------------------------------------------------------------------
; LOCAL FUNCTION DEFINITIONS
; Function Prototype declaration
----------------------------------------------------------------------------*/

/*----------------------------------------------------------------------------
; LOCAL VARIABLE DEFINITIONS
; Variable declaration - defined here and used outside this module
----------------------------------------------------------------------------*/

/* MA prediction coefficients (Q13) */
static const Word16 pred[NPRED] = {5571, 4751, 2785, 1556};

/* MA prediction coefficients (Q6)  */
static const Word16 pred_MR122[NPRED] = {44, 37, 22, 12};

/*
------------------------------------------------------------------------------
 FUNCTION NAME: gc_pred_reset
------------------------------------------------------------------------------
 INPUT AND OUTPUT DEFINITIONS

 Inputs:
    state = pointer to a structure of type gc_predState

 Outputs:
    past_qua_en field in the structure pointed to by state is initialized
      to MIN_ENERGY
    past_qua_en_MR122 field in the structure pointed to by state is
      initialized to MIN_ENERGY_MR122

 Returns:
    return_value = 0, if reset was successful; -1, otherwise (int)

 Global Variables Used:
    None

 Local Variables Needed:
    None

------------------------------------------------------------------------------
 FUNCTION DESCRIPTION

 This function initializes the state memory used by gc_pred to zero.

------------------------------------------------------------------------------
 REQUIREMENTS

 None

------------------------------------------------------------------------------
 REFERENCES

 gc_pred.c, UMTS GSM AMR speech codec, R99 - Version 3.2.0, March 2, 2001

------------------------------------------------------------------------------
 PSEUDO-CODE

int gc_pred_reset (gc_predState *state)
{
   Word16 i;

   if (state == (gc_predState *) NULL){
      fprintf(stderr, "gc_pred_reset: invalid parameter\n");
      return -1;
   }

   for(i = 0; i < NPRED; i++)
   {
      state->past_qua_en[i] = MIN_ENERGY;
      state->past_qua_en_MR122[i] = MIN_ENERGY_MR122;
   }
  return 0;
}

------------------------------------------------------------------------------
 CAUTION [optional]
 [State any special notes, constraints or cautions for users of this function]

------------------------------------------------------------------------------
*/

OSCL_EXPORT_REF Word16 gc_pred_reset(gc_predState *state)
{
    Word16 i;

    if (state == (gc_predState *) NULL)
    {
        /* fprintf(stderr, "gc_pred_reset: invalid parameter\n"); */
        return -1;
    }

    for (i = 0; i < NPRED; i++)
    {
        state->past_qua_en[i] = MIN_ENERGY;
        state->past_qua_en_MR122[i] = MIN_ENERGY_MR122;
    }

    return(0);
}

/****************************************************************************/

/*
------------------------------------------------------------------------------
 FUNCTION NAME: gc_pred
------------------------------------------------------------------------------
 INPUT AND OUTPUT DEFINITIONS

 Inputs:
    st = pointer to a structure of type gc_predState
    mode = AMR mode (enum Mode)
    code = pointer to the innovative codebook vector; Q12 in MR122 mode,
           otherwise, Q13 (Word16)
    exp_gcode0 = pointer to the exponent part of predicted gain factor
             (Q0) (Word16)
    frac_gcode0 = pointer to the fractional part of predicted gain factor
              (Q15) (Word16)
    exp_en = pointer to the exponent part of the innovation energy; this
         is calculated for MR795 mode, Q0 (Word16)
    frac_en = pointer to the fractional part of the innovation energy;
          this is calculated for MR795 mode, Q15 (Word16)
    pOverflow = pointer to overflow (Flag)

 Outputs:
    store pointed to by exp_gcode0 contains the exponent part of the
      recently calculated predicted gain factor
    store pointed to by frac_gcode0 contains the fractional part of the
      recently calculated predicted gain factor
    store pointed to by exp_en contains the exponent part of the
      recently calculated innovation energy
    store pointed to by frac_en contains the fractional part of the
      recently calculated innovation energy
    pOverflow = 1 if the math functions called by gc_pred
                results in overflow else zero.

 Returns:
    None

 Global Variables Used:
    None

 Local Variables Needed:
    pred = table of MA prediction coefficients (Q13) (Word16)
    pred_MR122 = table of MA prediction coefficients (Q6) (Word16)

------------------------------------------------------------------------------
 FUNCTION DESCRIPTION

 This function performs the MA prediction of the innovation energy (in
 dB/(20*log10(2))), with the mean removed.

------------------------------------------------------------------------------
 REQUIREMENTS

 None

------------------------------------------------------------------------------
 REFERENCES

 gc_pred.c, UMTS GSM AMR speech codec, R99 - Version 3.2.0, March 2, 2001

------------------------------------------------------------------------------
 PSEUDO-CODE

The original etsi reference code uses a global flag Overflow. However, in the
actual implementation a pointer to a the overflow flag is passed in.

void
gc_pred(
    gc_predState *st,   // i/o: State struct
    enum Mode mode,     // i  : AMR mode
    Word16 *code,       // i  : innovative codebook vector (L_SUBFR)
                        //      MR122: Q12, other modes: Q13
    Word16 *exp_gcode0, // o  : exponent of predicted gain factor, Q0
    Word16 *frac_gcode0,// o  : fraction of predicted gain factor  Q15
    Word16 *exp_en,     // o  : exponent of innovation energy,     Q0
                        //      (only calculated for MR795)
    Word16 *frac_en     // o  : fraction of innovation energy,     Q15
                        //      (only calculated for MR795)
)
{
    Word16 i;
    Word32 ener_code;
    Word16 exp, frac;

     *-------------------------------------------------------------------*
     *  energy of code:                                                  *
     *  ~~~~~~~~~~~~~~~                                                  *
     *  ener_code = sum(code[i]^2)                                       *
     *-------------------------------------------------------------------*
    ener_code = L_mac((Word32) 0, code[0], code[0]);
                                                 // MR122:  Q12*Q12 -> Q25
                                                 // others: Q13*Q13 -> Q27
    for (i = 1; i < L_SUBFR; i++)
        ener_code = L_mac(ener_code, code[i], code[i]);

    if (sub (mode, MR122) == 0)
    {
        Word32 ener;

        // ener_code = ener_code / lcode; lcode = 40; 1/40 = 26214 Q20
        ener_code = L_mult (pv_round (ener_code), 26214);   // Q9  * Q20 -> Q30

         *-------------------------------------------------------------------*
         *  energy of code:                                                  *
         *  ~~~~~~~~~~~~~~~                                                  *
         *  ener_code(Q17) = 10 * Log10(energy) / constant                   *
         *                 = 1/2 * Log2(energy)                              *
         *                                           constant = 20*Log10(2)  *
         *-------------------------------------------------------------------*
        // ener_code = 1/2 * Log2(ener_code); Note: Log2=log2+30
        Log2(ener_code, &exp, &frac);
        ener_code = L_Comp (sub (exp, 30), frac);     // Q16 for log()
                                                    // ->Q17 for 1/2 log()

         *-------------------------------------------------------------------*
         *  predicted energy:                                                *
         *  ~~~~~~~~~~~~~~~~~                                                *
         *  ener(Q24) = (Emean + sum{pred[i]*past_en[i]})/constant           *
         *            = MEAN_ENER + sum(pred[i]*past_qua_en[i])              *
         *                                           constant = 20*Log10(2)  *
         *-------------------------------------------------------------------*

        ener = MEAN_ENER_MR122;                      // Q24 (Q17)
        for (i = 0; i < NPRED; i++)
        {
            ener = L_mac (ener, st->past_qua_en_MR122[i], pred_MR122[i]);
                                                     // Q10 * Q13 -> Q24
                                                     // Q10 * Q6  -> Q17
        }

         *-------------------------------------------------------------------*
         *  predicted codebook gain                                          *
         *  ~~~~~~~~~~~~~~~~~~~~~~~                                          *
         *  gc0     = Pow10( (ener*constant - ener_code*constant) / 20 )     *
         *          = Pow2(ener-ener_code)                                   *
         *          = Pow2(int(d)+frac(d))                                   *
         *                                                                   *
         *  (store exp and frac for pow2())                                  *
         *-------------------------------------------------------------------*

        ener = L_shr (L_sub (ener, ener_code), 1);                // Q16
        L_Extract(ener, exp_gcode0, frac_gcode0);
    }
    else // all modes except 12.2
    {
        Word32 L_tmp;
        Word16 exp_code, gcode0;

         *-----------------------------------------------------------------*
         *  Compute: means_ener - 10log10(ener_code/ L_sufr)               *
         *-----------------------------------------------------------------*

        exp_code = norm_l (ener_code);
        ener_code = L_shl (ener_code, exp_code);

        // Log2 = log2 + 27
        Log2_norm (ener_code, exp_code, &exp, &frac);

        // fact = 10/log2(10) = 3.01 = 24660 Q13
        L_tmp = Mpy_32_16(exp, frac, -24660); // Q0.Q15 * Q13 -> Q14

         *   L_tmp = means_ener - 10log10(ener_code/L_SUBFR)
         *         = means_ener - 10log10(ener_code) + 10log10(L_SUBFR)
         *         = K - fact * Log2(ener_code)
         *         = K - fact * log2(ener_code) - fact*27
         *
         *   ==> K = means_ener + fact*27 + 10log10(L_SUBFR)
         *
         *   means_ener =       33    =  540672    Q14  (MR475, MR515, MR59)
         *   means_ener =       28.75 =  471040    Q14  (MR67)
         *   means_ener =       30    =  491520    Q14  (MR74)
         *   means_ener =       36    =  589824    Q14  (MR795)
         *   means_ener =       33    =  540672    Q14  (MR102)
         *   10log10(L_SUBFR) = 16.02 =  262481.51 Q14
         *   fact * 27                = 1331640    Q14
         *   -----------------------------------------
         *   (MR475, MR515, MR59)   K = 2134793.51 Q14 ~= 16678 * 64 * 2
         *   (MR67)                 K = 2065161.51 Q14 ~= 32268 * 32 * 2
         *   (MR74)                 K = 2085641.51 Q14 ~= 32588 * 32 * 2
         *   (MR795)                K = 2183945.51 Q14 ~= 17062 * 64 * 2
         *   (MR102)                K = 2134793.51 Q14 ~= 16678 * 64 * 2


        if (sub (mode, MR102) == 0)
        {
            // mean = 33 dB
            L_tmp = L_mac(L_tmp, 16678, 64);     // Q14
        }
        else if (sub (mode, MR795) == 0)
        {
            // ener_code  = <xn xn> * 2^27*2^exp_code
            // frac_en    = ener_code / 2^16
            //            = <xn xn> * 2^11*2^exp_code
            // <xn xn>    = <xn xn>*2^11*2^exp * 2^exp_en
            //           := frac_en            * 2^exp_en

            // ==> exp_en = -11-exp_code;

            *frac_en = extract_h (ener_code);
            *exp_en = sub (-11, exp_code);

            // mean = 36 dB
            L_tmp = L_mac(L_tmp, 17062, 64);     // Q14
        }
        else if (sub (mode, MR74) == 0)
        {
            // mean = 30 dB
            L_tmp = L_mac(L_tmp, 32588, 32);     // Q14
        }
        else if (sub (mode, MR67) == 0)
        {
            // mean = 28.75 dB
            L_tmp = L_mac(L_tmp, 32268, 32);     // Q14
        }
        else // MR59, MR515, MR475
        {
            // mean = 33 dB
            L_tmp = L_mac(L_tmp, 16678, 64);     // Q14
        }

         *-----------------------------------------------------------------*
         * Compute gcode0.                                                 *
         *  = Sum(i=0,3) pred[i]*past_qua_en[i] - ener_code + mean_ener    *
         *-----------------------------------------------------------------*

        L_tmp = L_shl(L_tmp, 10);                // Q24
        for (i = 0; i < 4; i++)
            L_tmp = L_mac(L_tmp, pred[i], st->past_qua_en[i]);
                                                 // Q13 * Q10 -> Q24

        gcode0 = extract_h(L_tmp);               // Q8

         *-----------------------------------------------------------------*
         * gcode0 = pow(10.0, gcode0/20)                                   *
         *        = pow(2, 3.3219*gcode0/20)                               *
         *        = pow(2, 0.166*gcode0)                                   *
         *-----------------------------------------------------------------*

        // 5439 Q15 = 0.165985
        // (correct: 1/(20*log10(2)) 0.166096 = 5443 Q15)
        if (sub (mode, MR74) == 0) // For IS641 bitexactness
            L_tmp = L_mult(gcode0, 5439);  // Q8 * Q15 -> Q24
        else
            L_tmp = L_mult(gcode0, 5443);  // Q8 * Q15 -> Q24

        L_tmp = L_shr(L_tmp, 8);                   //          -> Q16
        L_Extract(L_tmp, exp_gcode0, frac_gcode0); //       -> Q0.Q15
    }
}

------------------------------------------------------------------------------
 CAUTION [optional]
 [State any special notes, constraints or cautions for users of this function]

------------------------------------------------------------------------------
*/

OSCL_EXPORT_REF void gc_pred(
    gc_predState *st,   /* i/o: State struct                           */
    enum Mode mode,     /* i  : AMR mode                               */
    Word16 *code,       /* i  : innovative codebook vector (L_SUBFR)   */
    /*      MR122: Q12, other modes: Q13           */
    Word16 *exp_gcode0, /* o  : exponent of predicted gain factor, Q0  */
    Word16 *frac_gcode0,/* o  : fraction of predicted gain factor  Q15 */
    Word16 *exp_en,     /* o  : exponent of innovation energy,     Q0  */
    /*      (only calculated for MR795)            */
    Word16 *frac_en,    /* o  : fraction of innovation energy,     Q15 */
    /*      (only calculated for MR795)            */
    Flag   *pOverflow
)
{
    register Word16 i;
    register Word32 L_temp1, L_temp2;
    register Word32 L_tmp;
    Word32 ener_code;
    Word32 ener;
    Word16 exp, frac;
    Word16 exp_code, gcode0;
    Word16 tmp;
    Word16 *p_code = &code[0];

    /*-------------------------------------------------------------------*
     *  energy of code:                                                  *
     *  ~~~~~~~~~~~~~~~                                                  *
     *  ener_code = sum(code[i]^2)                                       *
     *-------------------------------------------------------------------*/
    ener_code = 0;

    /* MR122:  Q12*Q12 -> Q25 */
    /* others: Q13*Q13 -> Q27 */

    for (i = L_SUBFR >> 2; i != 0; i--)
    {
        tmp = *(p_code++);
        ener_code += ((Word32) tmp * tmp) >> 3;
        tmp = *(p_code++);
        ener_code += ((Word32) tmp * tmp) >> 3;
        tmp = *(p_code++);
        ener_code += ((Word32) tmp * tmp) >> 3;
        tmp = *(p_code++);
        ener_code += ((Word32) tmp * tmp) >> 3;
    }

    ener_code <<= 4;

    if ((ener_code >> 31))     /*  Check for saturation */
    {
        ener_code = MAX_32;
    }

    if (mode == MR122)
    {
        /* ener_code = ener_code / lcode; lcode = 40; 1/40 = 26214 Q20 */
        /* Q9  * Q20 -> Q30 */

        ener_code = ((Word32)(pv_round(ener_code, pOverflow) * 26214)) << 1;

        /*-------------------------------------------------------------*
         *  energy of code:                                            *
         *  ~~~~~~~~~~~~~~~                                            *
         *  ener_code(Q17) = 10 * Log10(energy) / constant             *
         *                 = 1/2 * Log2(energy)                        *
         *  constant = 20*Log10(2)                                     *
         *-------------------------------------------------------------*/
        /* ener_code = 1/2 * Log2(ener_code); Note: Log2=log2+30 */
        Log2(ener_code, &exp, &frac, pOverflow);

        /* Q16 for log()    */
        /* ->Q17 for 1/2 log()*/

        L_temp1 = (Word32)(exp - 30) << 16;
        ener_code = L_temp1 + ((Word32)frac << 1);

        /*-------------------------------------------------------------*
         *  predicted energy:                                          *
         *  ~~~~~~~~~~~~~~~~~                                          *
         *  ener(Q24) = (Emean + sum{pred[i]*past_en[i]})/constant     *
         *            = MEAN_ENER + sum(pred[i]*past_qua_en[i])        *
         *  constant = 20*Log10(2)                                     *
         *-------------------------------------------------------------*/

        ener = MEAN_ENER_MR122;                   /* Q24 (Q17) */
        for (i = 0; i < NPRED; i++)
        {
            L_temp1 = (((Word32) st->past_qua_en_MR122[i]) *
                       pred_MR122[i]) << 1;
            ener = L_add(ener, L_temp1, pOverflow);

            /* Q10 * Q13 -> Q24 */
            /* Q10 * Q6  -> Q17 */
        }

        /*---------------------------------------------------------------*
         *  predicted codebook gain                                      *
         *  ~~~~~~~~~~~~~~~~~~~~~~~                                      *
         *  gc0     = Pow10( (ener*constant - ener_code*constant) / 20 ) *
         *          = Pow2(ener-ener_code)                               *
         *          = Pow2(int(d)+frac(d))                               *
         *                                                               *
         *  (store exp and frac for pow2())                              *
         *---------------------------------------------------------------*/
        /* Q16 */

        L_temp1 = L_sub(ener, ener_code, pOverflow);


        *exp_gcode0 = (Word16)(L_temp1 >> 17);

        L_temp2 = (Word32) * exp_gcode0 << 15;
        L_temp1 >>= 2;

        *frac_gcode0 = (Word16)(L_temp1 - L_temp2);

    }
    else /* all modes except 12.2 */
    {
        /*-----------------------------------------------------------------*
         *  Compute: means_ener - 10log10(ener_code/ L_sufr)               *
         *-----------------------------------------------------------------*/

        exp_code = norm_l(ener_code);
        ener_code = L_shl(ener_code, exp_code, pOverflow);

        /* Log2 = log2 + 27 */
        Log2_norm(ener_code, exp_code, &exp, &frac);

        /* fact = 10/log2(10) = 3.01 = 24660 Q13 */
        /* Q0.Q15 * Q13 -> Q14 */

        L_temp2 = (((Word32) exp) * -24660) << 1;
        L_tmp = (((Word32) frac) * -24660) >> 15;

        /* Sign-extend resulting product */
        if (L_tmp & (Word32) 0x00010000L)
        {
            L_tmp = L_tmp | (Word32) 0xffff0000L;
        }

        L_tmp = L_tmp << 1;
        L_tmp = L_add(L_tmp, L_temp2, pOverflow);


        /*   L_tmp = means_ener - 10log10(ener_code/L_SUBFR)
         *         = means_ener - 10log10(ener_code) + 10log10(L_SUBFR)
         *         = K - fact * Log2(ener_code)
         *         = K - fact * log2(ener_code) - fact*27
         *
         *   ==> K = means_ener + fact*27 + 10log10(L_SUBFR)
         *
         *   means_ener =       33    =  540672    Q14  (MR475, MR515, MR59)
         *   means_ener =       28.75 =  471040    Q14  (MR67)
         *   means_ener =       30    =  491520    Q14  (MR74)
         *   means_ener =       36    =  589824    Q14  (MR795)
         *   means_ener =       33    =  540672    Q14  (MR102)
         *   10log10(L_SUBFR) = 16.02 =  262481.51 Q14
         *   fact * 27                = 1331640    Q14
         *   -----------------------------------------
         *   (MR475, MR515, MR59)   K = 2134793.51 Q14 ~= 16678 * 64 * 2
         *   (MR67)                 K = 2065161.51 Q14 ~= 32268 * 32 * 2
         *   (MR74)                 K = 2085641.51 Q14 ~= 32588 * 32 * 2
         *   (MR795)                K = 2183945.51 Q14 ~= 17062 * 64 * 2
         *   (MR102)                K = 2134793.51 Q14 ~= 16678 * 64 * 2
         */

        if (mode == MR102)
        {
            /* mean = 33 dB */
            L_temp2 = (Word32) 16678 << 7;
            L_tmp = L_add(L_tmp, L_temp2, pOverflow);     /* Q14 */
        }
        else if (mode == MR795)
        {
            /* ener_code  = <xn xn> * 2^27*2^exp_code
               frac_en    = ener_code / 2^16
                          = <xn xn> * 2^11*2^exp_code
               <xn xn>    = <xn xn>*2^11*2^exp * 2^exp_en
            :                 = frac_en            * 2^exp_en
                          ==> exp_en = -11-exp_code;      */
            *frac_en = (Word16)(ener_code >> 16);
            *exp_en = -11 - exp_code;

            /* mean = 36 dB */
            L_temp2 = (Word32) 17062 << 7;
            L_tmp = L_add(L_tmp, L_temp2, pOverflow);     /* Q14 */
        }
        else if (mode == MR74)
        {
            /* mean = 30 dB */
            L_temp2 = (Word32) 32588 << 6;
            L_tmp = L_add(L_tmp, L_temp2, pOverflow);     /* Q14 */
        }
        else if (mode == MR67)
        {
            /* mean = 28.75 dB */
            L_temp2 = (Word32) 32268 << 6;
            L_tmp = L_add(L_tmp, L_temp2, pOverflow);     /* Q14 */
        }
        else /* MR59, MR515, MR475 */
        {
            /* mean = 33 dB */
            L_temp2 = (Word32) 16678 << 7;
            L_tmp = L_add(L_tmp, L_temp2, pOverflow);     /* Q14 */
        }

        /*-------------------------------------------------------------*
         * Compute gcode0.                                              *
         *  = Sum(i=0,3) pred[i]*past_qua_en[i] - ener_code + mean_ener *
         *--------------------------------------------------------------*/
        /* Q24 */
        if (L_tmp > (Word32) 0X001fffffL)
        {
            *pOverflow = 1;
            L_tmp = MAX_32;
        }
        else if (L_tmp < -2097152)
        {
            *pOverflow = 1;
            L_tmp = MIN_32;
        }
        else
        {
            L_tmp = L_tmp << 10;
        }

        for (i = 0; i < 4; i++)
        {
            L_temp2 = ((((Word32) pred[i]) * st->past_qua_en[i]) << 1);
            L_tmp = L_add(L_tmp, L_temp2, pOverflow);  /* Q13 * Q10 -> Q24 */
        }

        gcode0 = (Word16)(L_tmp >> 16);               /* Q8  */

        /*-----------------------------------------------------------*
         * gcode0 = pow(10.0, gcode0/20)                             *
         *        = pow(2, 3.3219*gcode0/20)                         *
         *        = pow(2, 0.166*gcode0)                             *
         *-----------------------------------------------------------*/

        /* 5439 Q15 = 0.165985                                       */
        /* (correct: 1/(20*log10(2)) 0.166096 = 5443 Q15)            */

        if (mode == MR74) /* For IS641 bitexactness */
        {
            L_tmp = (((Word32) gcode0) * 5439) << 1;  /* Q8 * Q15 -> Q24 */
        }
        else
        {
            L_tmp = (((Word32) gcode0) * 5443) << 1;  /* Q8 * Q15 -> Q24 */
        }

        if (L_tmp < 0)
        {
            L_tmp = ~((~L_tmp) >> 8);
        }
        else
        {
            L_tmp = L_tmp >> 8;     /* -> Q16 */
        }

        *exp_gcode0 = (Word16)(L_tmp >> 16);
        if (L_tmp < 0)
        {
            L_temp1 = ~((~L_tmp) >> 1);
        }
        else
        {
            L_temp1 = L_tmp >> 1;
        }
        L_temp2 = (Word32) * exp_gcode0 << 15;
        *frac_gcode0 = (Word16)(L_sub(L_temp1, L_temp2, pOverflow));
        /* -> Q0.Q15 */
    }

    return;
}

/****************************************************************************/

/*
------------------------------------------------------------------------------
 FUNCTION NAME: gc_pred_update
------------------------------------------------------------------------------
 INPUT AND OUTPUT DEFINITIONS

 Inputs:
    st = pointer to a structure of type gc_predState
    qua_ener_MR122 = quantized energy for update (Q10); calculated as
             (log2(qua_err)) (Word16)
    qua_ener = quantized energy for update (Q10); calculated as
           (20*log10(qua_err)) (Word16)

 Outputs:
    structure pointed to by st contains the calculated quantized energy
      for update

 Returns:
    None

 Global Variables Used:
    None

 Local Variables Needed:
    None

------------------------------------------------------------------------------
 FUNCTION DESCRIPTION

 This function updates the MA predictor with the last quantized energy.

------------------------------------------------------------------------------
 REQUIREMENTS

 None

------------------------------------------------------------------------------
 REFERENCES

 gc_pred.c, UMTS GSM AMR speech codec, R99 - Version 3.2.0, March 2, 2001

------------------------------------------------------------------------------
 PSEUDO-CODE

void gc_pred_update(
    gc_predState *st,      // i/o: State struct
    Word16 qua_ener_MR122, // i  : quantized energy for update, Q10
                           //      (log2(qua_err))
    Word16 qua_ener        // i  : quantized energy for update, Q10
                           //      (20*log10(qua_err))
)
{
    Word16 i;

    for (i = 3; i > 0; i--)
    {
        st->past_qua_en[i] = st->past_qua_en[i - 1];
        st->past_qua_en_MR122[i] = st->past_qua_en_MR122[i - 1];
    }

    st->past_qua_en_MR122[0] = qua_ener_MR122;  //    log2 (qua_err), Q10

    st->past_qua_en[0] = qua_ener;              // 20*log10(qua_err), Q10

}

------------------------------------------------------------------------------
 CAUTION [optional]
 [State any special notes, constraints or cautions for users of this function]

------------------------------------------------------------------------------
*/

OSCL_EXPORT_REF void gc_pred_update(
    gc_predState *st,      /* i/o: State struct                     */
    Word16 qua_ener_MR122, /* i  : quantized energy for update, Q10 */
    /*      (log2(qua_err))                  */
    Word16 qua_ener        /* i  : quantized energy for update, Q10 */
    /*      (20*log10(qua_err))              */
)
{
    st->past_qua_en[3] = st->past_qua_en[2];
    st->past_qua_en_MR122[3] = st->past_qua_en_MR122[2];

    st->past_qua_en[2] = st->past_qua_en[1];
    st->past_qua_en_MR122[2] = st->past_qua_en_MR122[1];

    st->past_qua_en[1] = st->past_qua_en[0];
    st->past_qua_en_MR122[1] = st->past_qua_en_MR122[0];

    st->past_qua_en_MR122[0] = qua_ener_MR122; /*    log2 (qua_err), Q10 */

    st->past_qua_en[0] = qua_ener;            /* 20*log10(qua_err), Q10 */

    return;
}

/****************************************************************************/

/*
------------------------------------------------------------------------------
 FUNCTION NAME: gc_pred_average_limited
------------------------------------------------------------------------------
 INPUT AND OUTPUT DEFINITIONS

 Inputs:
    st = pointer to a structure of type gc_predState
    ener_avg_MR122 = pointer to the averaged quantized energy (Q10);
             calculated as (log2(qua_err)) (Word16)
    ener_avg = pointer to the averaged quantized energy (Q10); calculated
           as (20*log10(qua_err)) (Word16)
    pOverflow = pointer to overflow (Flag)

 Outputs:
    store pointed to by ener_avg_MR122 contains the new averaged quantized
      energy
    store pointed to by ener_avg contains the new averaged quantized
      energy
    pOverflow = 1 if the math functions called by gc_pred_average_limited
            results in overflow else zero.

 Returns:
    None

 Global Variables Used:
    None

 Local Variables Needed:
    None

------------------------------------------------------------------------------
 FUNCTION DESCRIPTION

 This function calculates the average of MA predictor state values (with a
 lower limit) used in error concealment.

------------------------------------------------------------------------------
 REQUIREMENTS

 None

------------------------------------------------------------------------------
 REFERENCES

 gc_pred.c, UMTS GSM AMR speech codec, R99 - Version 3.2.0, March 2, 2001

------------------------------------------------------------------------------
 PSEUDO-CODE

The original etsi reference code uses a global flag Overflow. However, in the
actual implementation a pointer to a the overflow flag is passed in.

void gc_pred_average_limited(
    gc_predState *st,       // i: State struct
    Word16 *ener_avg_MR122, // o: everaged quantized energy,  Q10
                            //    (log2(qua_err))
    Word16 *ener_avg        // o: averaged quantized energy,  Q10
                            //    (20*log10(qua_err))
)
{
    Word16 av_pred_en;
    Word16 i;

    // do average in MR122 mode (log2() domain)
    av_pred_en = 0;
    for (i = 0; i < NPRED; i++)
    {
        av_pred_en = add (av_pred_en, st->past_qua_en_MR122[i]);
    }

    // av_pred_en = 0.25*av_pred_en
    av_pred_en = mult (av_pred_en, 8192);

    // if (av_pred_en < -14/(20Log10(2))) av_pred_en = ..

    if (sub (av_pred_en, MIN_ENERGY_MR122) < 0)
    {
        av_pred_en = MIN_ENERGY_MR122;
    }
    *ener_avg_MR122 = av_pred_en;

    // do average for other modes (20*log10() domain)
    av_pred_en = 0;
    for (i = 0; i < NPRED; i++)
    {
        av_pred_en = add (av_pred_en, st->past_qua_en[i]);
    }

    // av_pred_en = 0.25*av_pred_en
    av_pred_en = mult (av_pred_en, 8192);

    // if (av_pred_en < -14) av_pred_en = ..

    if (sub (av_pred_en, MIN_ENERGY) < 0)
    {
        av_pred_en = MIN_ENERGY;
    }
    *ener_avg = av_pred_en;
}

------------------------------------------------------------------------------
 CAUTION [optional]
 [State any special notes, constraints or cautions for users of this function]

------------------------------------------------------------------------------
*/

OSCL_EXPORT_REF void gc_pred_average_limited(
    gc_predState *st,       /* i: State struct                    */
    Word16 *ener_avg_MR122, /* o: everaged quantized energy,  Q10 */
    /*    (log2(qua_err))                 */
    Word16 *ener_avg,       /* o: averaged quantized energy,  Q10 */
    /*    (20*log10(qua_err))             */
    Flag *pOverflow
)
{
    Word16 av_pred_en;
    register Word16 i;

    /* do average in MR122 mode (log2() domain) */
    av_pred_en = 0;
    for (i = 0; i < NPRED; i++)
    {
        av_pred_en =
            add_16(av_pred_en, st->past_qua_en_MR122[i], pOverflow);
    }

    /* av_pred_en = 0.25*av_pred_en  (with sign-extension)*/
    if (av_pred_en < 0)
    {
        av_pred_en = (av_pred_en >> 2) | 0xc000;
    }
    else
    {
        av_pred_en >>= 2;
    }

    /* if (av_pred_en < -14/(20Log10(2))) av_pred_en = .. */
    if (av_pred_en < MIN_ENERGY_MR122)
    {
        av_pred_en = MIN_ENERGY_MR122;
    }
    *ener_avg_MR122 = av_pred_en;

    /* do average for other modes (20*log10() domain) */
    av_pred_en = 0;
    for (i = 0; i < NPRED; i++)
    {
        av_pred_en = add_16(av_pred_en, st->past_qua_en[i], pOverflow);
    }

    /* av_pred_en = 0.25*av_pred_en  (with sign-extension)*/
    if (av_pred_en < 0)
    {
        av_pred_en = (av_pred_en >> 2) | 0xc000;
    }
    else
    {
        av_pred_en >>= 2;
    }

    /* if (av_pred_en < -14) av_pred_en = .. */
    if (av_pred_en < MIN_ENERGY)
    {
        av_pred_en = MIN_ENERGY;
    }
    *ener_avg = av_pred_en;
}
