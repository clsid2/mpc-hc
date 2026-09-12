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



 Filename: isf_extrapolation.cpp

------------------------------------------------------------------------------
 INPUT AND OUTPUT DEFINITIONS

    int16 HfIsf[]    (i/o)  isf vector

------------------------------------------------------------------------------
 FUNCTION DESCRIPTION

    Conversion of 16th-order 12.8kHz ISF vector
    into 20th-order 16kHz ISF vector

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
#include "pvamrwbdecoder_cnst.h"
#include "pvamrwbdecoder_acelp.h"
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
#define INV_LENGTH 2731                    /* 1/12 */

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


void isf_extrapolation(int16 HfIsf[])
{
    int16 IsfDiff[M - 2];
    int32 IsfCorr[3];
    int32 L_tmp;
    int16 coeff, mean, tmp, tmp2, tmp3;
    int16 exp, exp2, hi, lo;
    int16 i, MaxCorr;

    HfIsf[M16k - 1] = HfIsf[M - 1];

    /* Difference vector */
    for (i = 1; i < (M - 1); i++)
    {
        IsfDiff[i - 1] = sub_int16(HfIsf[i], HfIsf[i - 1]);
    }
    L_tmp = 0;

    /* Mean of difference vector */
    for (i = 3; i < (M - 1); i++)
    {
        L_tmp = mac_16by16_to_int32(L_tmp, IsfDiff[i - 1], INV_LENGTH);

    }
    mean = amr_wb_round(L_tmp);

    IsfCorr[0] = 0;

    tmp = 0;
    for (i = 0; i < (M - 2); i++)
    {
        if (IsfDiff[i] > tmp)
        {
            tmp = IsfDiff[i];
        }
    }
    exp = norm_s(tmp);
    for (i = 0; i < (M - 2); i++)
    {
        IsfDiff[i] = shl_int16(IsfDiff[i], exp);
    }
    mean = shl_int16(mean, exp);
    for (i = 7; i < (M - 2); i++)
    {
        tmp2 = sub_int16(IsfDiff[i], mean);
        tmp3 = sub_int16(IsfDiff[i - 2], mean);
        L_tmp = mul_16by16_to_int32(tmp2, tmp3);
        int32_to_dpf(L_tmp, &hi, &lo);
        L_tmp = mpy_dpf_32(hi, lo, hi, lo);
        IsfCorr[0] = add_int32(IsfCorr[0], L_tmp);
    }
    IsfCorr[1] = 0;
    for (i = 7; i < (M - 2); i++)
    {
        tmp2 = sub_int16(IsfDiff[i], mean);
        tmp3 = sub_int16(IsfDiff[i - 3], mean);
        L_tmp = mul_16by16_to_int32(tmp2, tmp3);
        int32_to_dpf(L_tmp, &hi, &lo);
        L_tmp = mpy_dpf_32(hi, lo, hi, lo);
        IsfCorr[1] = add_int32(IsfCorr[1], L_tmp);
    }
    IsfCorr[2] = 0;
    for (i = 7; i < (M - 2); i++)
    {
        tmp2 = sub_int16(IsfDiff[i], mean);
        tmp3 = sub_int16(IsfDiff[i - 4], mean);
        L_tmp = mul_16by16_to_int32(tmp2, tmp3);
        int32_to_dpf(L_tmp, &hi, &lo);
        L_tmp = mpy_dpf_32(hi, lo, hi, lo);
        IsfCorr[2] = add_int32(IsfCorr[2], L_tmp);
    }

    if (IsfCorr[0] > IsfCorr[1])
    {
        MaxCorr = 0;
    }
    else
    {
        MaxCorr = 1;
    }


    if (IsfCorr[2] > IsfCorr[MaxCorr])
    {
        MaxCorr = 2;
    }

    MaxCorr++;             /* Maximum correlation of difference vector */

    for (i = M - 1; i < (M16k - 1); i++)
    {
        tmp = sub_int16(HfIsf[i - 1 - MaxCorr], HfIsf[i - 2 - MaxCorr]);
        HfIsf[i] = add_int16(HfIsf[i - 1], tmp);
    }

    /* tmp=7965+(HfIsf[2]-HfIsf[3]-HfIsf[4])/6; */
    tmp = add_int16(HfIsf[4], HfIsf[3]);
    tmp = sub_int16(HfIsf[2], tmp);
    tmp = mult_int16(tmp, 5461);
    tmp += 20390;


    if (tmp > 19456)
    {                                      /* Maximum value of ISF should be at most 7600 Hz */
        tmp = 19456;
    }
    tmp = sub_int16(tmp, HfIsf[M - 2]);
    tmp2 = sub_int16(HfIsf[M16k - 2], HfIsf[M - 2]);

    exp2 = norm_s(tmp2);
    exp = norm_s(tmp);
    exp--;
    tmp <<= exp;
    tmp2 <<= exp2;
    coeff = div_16by16(tmp, tmp2);              /* Coefficient for stretching the ISF vector */
    exp = exp2 - exp;

    for (i = M - 1; i < (M16k - 1); i++)
    {
        tmp = mult_int16(sub_int16(HfIsf[i], HfIsf[i - 1]), coeff);
        IsfDiff[i - (M - 1)] = shl_int16(tmp, exp);
    }

    for (i = M; i < (M16k - 1); i++)
    {
        /* The difference between ISF(n) and ISF(n-2) should be at least 500 Hz */
        tmp = IsfDiff[i - (M - 1)] + IsfDiff[i - M] - 1280;

        if (tmp < 0)
        {

            if (IsfDiff[i - (M - 1)] > IsfDiff[i - M])
            {
                IsfDiff[i - M] = 1280 - IsfDiff[i - (M - 1)];
            }
            else
            {
                IsfDiff[i - (M - 1)] = 1280 - IsfDiff[i - M];
            }
        }
    }

    for (i = M - 1; i < (M16k - 1); i++)
    {
        HfIsf[i] = add_int16(HfIsf[i - 1], IsfDiff[i - (M - 1)]);
    }

    for (i = 0; i < (M16k - 1); i++)
    {
        HfIsf[i] = mult_int16(HfIsf[i], 26214);  /* Scale the ISF vector correctly for 16000 kHz */
    }

    Isf_isp(HfIsf, HfIsf, M16k);

    return;
}


