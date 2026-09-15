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

 Filename: residu.cpp

------------------------------------------------------------------------------
*/

/*----------------------------------------------------------------------------
; INCLUDES
----------------------------------------------------------------------------*/
#include "residu.h"
#include "typedef.h"
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
; LOCAL STORE/BUFFER/POINTER DEFINITIONS
; Variable declaration - defined here and used outside this module
----------------------------------------------------------------------------*/


/*
------------------------------------------------------------------------------
 FUNCTION NAME: Residu
------------------------------------------------------------------------------
 INPUT AND OUTPUT DEFINITIONS

 Inputs:
    coef_ptr = pointer to buffer containing the prediction coefficients
    input_ptr = pointer to buffer containing the speech signal
    input_len = filter order
    residual_ptr = pointer to buffer of residual signal

 Outputs:
    residual_ptr buffer contains the newly calculated the residual signal

 Returns:
    None

 Global Variables Used:
    None

 Local Variables Needed:
    None

------------------------------------------------------------------------------
 FUNCTION DESCRIPTION

 This function computes the LP residual by filtering the input speech through
 the LP inverse filter A(z).

------------------------------------------------------------------------------
 REQUIREMENTS

 None

------------------------------------------------------------------------------
 REFERENCES

 residu.c, UMTS GSM AMR speech codec, R99 - Version 3.2.0, March 2, 2001

------------------------------------------------------------------------------
 PSEUDO-CODE

 Note: Input argument names were changed to be more descriptive. Shown below
       are the original names. Shown below are the name changes:
           a[]  <-->  coef_ptr[]
           x[]  <-->  input_ptr[]
           y[]  <-->  residual_ptr[]
           lg   <-->  input_len


void Residu (
    Word16 a[], // (i)     : prediction coefficients
    Word16 x[], // (i)     : speech signal
    Word16 y[], // (o)     : residual signal
    Word16 lg   // (i)     : size of filtering
)
{
    Word16 i, j;
    Word32 s;

    for (i = 0; i < lg; i++)
    {
        s = L_mult (x[i], a[0]);
        for (j = 1; j <= M; j++)
        {
            s = L_mac (s, a[j], x[i - j]);
        }
        s = L_shl (s, 3);
        y[i] = pv_round (s);
    }
    return;
}

------------------------------------------------------------------------------
 CAUTION [optional]
 [State any special notes, constraints or cautions for users of this function]

------------------------------------------------------------------------------
*/

OSCL_EXPORT_REF void Residu(
    Word16 coef_ptr[],      /* (i)     : prediction coefficients*/
    Word16 input_ptr[],     /* (i)     : speech signal          */
    Word16 residual_ptr[],  /* (o)     : residual signal        */
    Word16 input_len        /* (i)     : size of filtering      */
)
{


    register Word16 i, j;
    Word32 s1;
    Word32 s2;
    Word32 s3;
    Word32 s4;
    Word16 *p_input1;
    Word16 *p_input2;
    Word16 *p_input3;
    Word16 *p_input4;
    Word16 *p_coef;
    Word16 *p_residual_ptr = &residual_ptr[input_len-1];
    Word16 *p_input_ptr    = &input_ptr[input_len-1-M];

    for (i = input_len >> 2; i != 0; i--)
    {
        s1       = 0x0000800L;
        s2       = 0x0000800L;
        s3       = 0x0000800L;
        s4       = 0x0000800L;
        p_coef  = &coef_ptr[M];
        p_input1 = p_input_ptr--;
        p_input2 = p_input_ptr--;
        p_input3 = p_input_ptr--;
        p_input4 = p_input_ptr--;

        for (j = M >> 1; j != 0; j--)
        {
            s1 += ((Word32) * (p_coef) * *(p_input1++));
            s2 += ((Word32) * (p_coef) * *(p_input2++));
            s3 += ((Word32) * (p_coef) * *(p_input3++));
            s4 += ((Word32) * (p_coef--) * *(p_input4++));
            s1 += ((Word32) * (p_coef) * *(p_input1++));
            s2 += ((Word32) * (p_coef) * *(p_input2++));
            s3 += ((Word32) * (p_coef) * *(p_input3++));
            s4 += ((Word32) * (p_coef--) * *(p_input4++));
        }

        s1 += (((Word32) * (p_coef)) * *(p_input1));
        s2 += (((Word32) * (p_coef)) * *(p_input2));
        s3 += (((Word32) * (p_coef)) * *(p_input3));
        s4 += (((Word32) * (p_coef)) * *(p_input4));

        *(p_residual_ptr--) = (Word16)(s1 >> 12);
        *(p_residual_ptr--) = (Word16)(s2 >> 12);
        *(p_residual_ptr--) = (Word16)(s3 >> 12);
        *(p_residual_ptr--) = (Word16)(s4 >> 12);

    }

    return;
}
