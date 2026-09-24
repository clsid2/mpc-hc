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



 Filename: agc2_amr_wb.cpp

------------------------------------------------------------------------------
 INPUT AND OUTPUT DEFINITIONS

     int16 * sig_in,            (i)     : postfilter input signal
     int16 * sig_out,           (i/o)   : postfilter output signal
     int16 l_trm                (i)     : subframe size


------------------------------------------------------------------------------
 FUNCTION DESCRIPTION

    Performs adaptive gain control

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

#include "pvamrwbdecoder_cnst.h"
#include "pvamrwbdecoder_acelp.h"
#include "pv_amr_wb_type_defs.h"
#include "pvamrwbdecoder_basic_op.h"
#include "pvamrwb_math_op.h"

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

void agc2_amr_wb(
    int16 * sig_in,          /* (i)     : postfilter input signal  */
    int16 * sig_out,         /* (i/o)   : postfilter output signal */
    int16 l_trm              /* (i)     : subframe size            */
)
{

    int16 i, exp;
    int16 gain_in, gain_out, g0;
    int32 s;

    int16 temp;

    /* calculate gain_out with exponent */

    temp = sig_out[0] >> 2;
    s = fxp_mul_16by16(temp, temp) << 1;
    for (i = 1; i < l_trm; i++)
    {
        temp = sig_out[i] >> 2;
        s = mac_16by16_to_int32(s, temp, temp);
    }


    if (s == 0)
    {
        return;
    }
    exp = normalize_amr_wb(s) - 1;
    gain_out = amr_wb_round(s << exp);

    /* calculate gain_in with exponent */

    temp = sig_in[0] >> 2;
    s = mul_16by16_to_int32(temp, temp);
    for (i = 1; i < l_trm; i++)
    {
        temp = sig_in[i] >> 2;
        s = mac_16by16_to_int32(s, temp, temp);
    }


    if (s == 0)
    {
        g0 = 0;
    }
    else
    {
        i = normalize_amr_wb(s);
        gain_in = amr_wb_round(s << i);
        exp -= i;

        /*
         *  g0 = sqrt(gain_in/gain_out)
         */

        s = div_16by16(gain_out, gain_in);
        s = shl_int32(s, 7);                   /* s = gain_out / gain_in */
        s = shr_int32(s, exp);                 /* add exponent */

        s = one_ov_sqrt(s);
        g0 = amr_wb_round(shl_int32(s, 9));
    }
    /* sig_out(n) = gain(n) sig_out(n) */

    for (i = 0; i < l_trm; i++)
    {
        sig_out[i] = extract_h(shl_int32(fxp_mul_16by16(sig_out[i], g0), 3));

    }

    return;
}

