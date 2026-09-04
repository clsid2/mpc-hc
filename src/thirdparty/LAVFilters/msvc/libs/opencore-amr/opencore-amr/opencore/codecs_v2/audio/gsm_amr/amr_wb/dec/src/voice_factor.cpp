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



 Filename: voice_factor.cpp

------------------------------------------------------------------------------
 INPUT AND OUTPUT DEFINITIONS

     int16 exc[],        (i) Q_exc : pitch excitation
     int16 Q_exc,        (i)       : exc format
     int16 gain_pit,     (i) Q14   : gain of pitch
     int16 code[],       (i) Q9    : Fixed codebook excitation
     int16 gain_code,    (i) Q0    : gain of code
     int16 L_subfr       (i)       : subframe length

------------------------------------------------------------------------------
 FUNCTION DESCRIPTION

    Find the voicing factor (1=voice to -1=unvoiced).

------------------------------------------------------------------------------
 REQUIREMENTS


------------------------------------------------------------------------------
 REFERENCES

------------------------------------------------------------------------------
 PSEUDO-CODE

------------------------------------------------------------------------------
*/


/*----------------------------------------------------------------------------
; INCLUDES
----------------------------------------------------------------------------*/

#include "pv_amr_wb_type_defs.h"
#include "pvamrwbdecoder_basic_op.h"
#include "pvamrwb_math_op.h"
#include "pvamrwbdecoder_acelp.h"

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

/*----------------------------------------------------------------------------
; EXTERNAL FUNCTION REFERENCES
; Declare functions defined elsewhere and referenced in this module
----------------------------------------------------------------------------*/

/*----------------------------------------------------------------------------
; EXTERNAL GLOBAL STORE/BUFFER/POINTER REFERENCES
; Declare variables used in this module but defined elsewhere
----------------------------------------------------------------------------*/

/*----------------------------------------------------------------------------
; FUNCTION CODE
----------------------------------------------------------------------------*/

int16 voice_factor(    /* (o) Q15   : factor (-1=unvoiced to 1=voiced) */
    int16 exc[],      /* (i) Q_exc : pitch excitation                 */
    int16 Q_exc,      /* (i)       : exc format                       */
    int16 gain_pit,   /* (i) Q14   : gain of pitch                    */
    int16 code[],     /* (i) Q9    : Fixed codebook excitation        */
    int16 gain_code,  /* (i) Q0    : gain of code                     */
    int16 L_subfr     /* (i)       : subframe length                  */
)
{
    int16 i, tmp, exp, ener1, exp1, ener2, exp2;
    int32 L_tmp;

    ener1 = extract_h(Dot_product12(exc, exc, L_subfr, &exp1));
    exp1 = sub_int16(exp1, Q_exc << 1);
    L_tmp = mul_16by16_to_int32(gain_pit, gain_pit);
    exp = normalize_amr_wb(L_tmp);

    tmp = (int16)((L_tmp << exp) >> 16);
    ener1 = mult_int16(ener1, tmp);
    exp1 -= (exp + 10);        /* 10 -> gain_pit Q14 to Q9 */

    ener2 = extract_h(Dot_product12(code, code, L_subfr, &exp2));

    exp = norm_s(gain_code);
    tmp = shl_int16(gain_code, exp);
    tmp = mult_int16(tmp, tmp);
    ener2 = mult_int16(ener2, tmp);
    exp2 -= (exp << 1);

    i = exp1 - exp2;


    if (i >= 0)
    {
        ener1 >>=  1;
        ener2 >>= (i + 1);
    }
    else
    {
        ener1 >>= (1 - i);
        ener2 >>= 1;
    }

    tmp = ener1 - ener2;
    ener1 += ener2 + 1;


    if (tmp >= 0)
    {
        tmp = div_16by16(tmp, ener1);
    }
    else
    {
        tmp = negate_int16(div_16by16(negate_int16(tmp), ener1));
    }

    return (tmp);
}
