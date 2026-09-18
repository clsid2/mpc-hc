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

    3GPP TS 26.173
    ANSI-C code for the Adaptive Multi-Rate - Wideband (AMR-WB) speech codec
    Available from http://www.3gpp.org

(C) 2007, 3GPP Organizational Partners (ARIB, ATIS, CCSA, ETSI, TTA, TTC)
Permission to distribute, modify and use this file under the standard license
terms listed above has been obtained from the copyright holder.
****************************************************************************************/
/*
------------------------------------------------------------------------------



 Pathname: ./src/pvamrwb_math_op.h

------------------------------------------------------------------------------
 INCLUDE DESCRIPTION

------------------------------------------------------------------------------
*/

#ifndef PVAMRWB_MATH_OP_H
#define PVAMRWB_MATH_OP_H


/*----------------------------------------------------------------------------
; INCLUDES
----------------------------------------------------------------------------*/

#include "pv_amr_wb_type_defs.h"

/*----------------------------------------------------------------------------
; DEFINES
----------------------------------------------------------------------------*/

/*----------------------------------------------------------------------------
; MACROS
; Define module specific macros here
----------------------------------------------------------------------------*/

/*----------------------------------------------------------------------------
; EXTERNAL VARIABLES REFERENCES
----------------------------------------------------------------------------*/
#ifdef __cplusplus
extern "C"
{
#endif


    int32 one_ov_sqrt(      /* (o) Q31 : output value (range: 0<=val<1)         */
        int32 L_x          /* (i) Q0  : input value  (range: 0<=val<=7fffffff) */
    );
    void one_ov_sqrt_norm(
        int32 * frac,      /* (i/o) Q31: normalized value (1.0 < frac <= 0.5) */
        int16 * exp        /* (i/o)    : exponent (value = frac x 2^exponent) */
    );
    int32 power_of_2(       /* (o) Q0  : result       (range: 0<=val<=0x7fffffff) */
        int16 exponant,    /* (i) Q0  : Integer part.      (range: 0<=val<=30)   */
        int16 fraction     /* (i) Q15 : Fractionnal part.  (range: 0.0<=val<1.0) */
    );
    int32 Dot_product12(    /* (o) Q31: normalized result (1 < val <= -1) */
        int16 x[],         /* (i) 12bits: x vector                       */
        int16 y[],         /* (i) 12bits: y vector                       */
        int16 lg,          /* (i)    : vector length                     */
        int16 * exp        /* (o)    : exponent of result (0..+30)       */
    );


    void amrwb_log_2(
        int32 L_x,        /* (i) : input value                                 */
        int16 *exponent,  /* (o) : Integer part of Log2.   (range: 0<=val<=30) */
        int16 *fraction   /* (o) : Fractional part of Log2. (range: 0<=val<1)*/
    );

    void Lg2_normalized(
        int32 L_x,         /* (i) : input value (normalized)                    */
        int16 exp,         /* (i) : norm_l (L_x)                                */
        int16 *exponent,   /* (o) : Integer part of Log2.   (range: 0<=val<=30) */
        int16 *fraction    /* (o) : Fractional part of Log2. (range: 0<=val<1)  */
    );


    int16 mult_int16_r(int16 var1, int16 var2);          /* Mult with round, 2 */
    int16 shr_rnd(int16 var1, int16 var2);         /* Shift right with round, 2  */

    int16 div_16by16(int16 var1, int16 var2);       /* Short division,       18  */


    void int32_to_dpf(int32 L_32, int16 *hi, int16 *lo);
    int32 mpy_dpf_32(int16 hi1, int16 lo1, int16 hi2, int16 lo2);


#define norm_s( x)    (normalize_amr_wb(  x) - 16)


#define extract_h( x)    (int16)(x>>16)
#define L_deposit_h( x)    (int32)(x<<16)


#ifdef __cplusplus
}
#endif


#endif /*  PVAMRWB_MATH_OP_H */

