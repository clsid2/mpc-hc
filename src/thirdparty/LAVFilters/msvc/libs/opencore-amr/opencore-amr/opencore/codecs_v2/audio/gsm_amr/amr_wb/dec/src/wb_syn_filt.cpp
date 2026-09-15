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



 Filename: wb_syn_filt.cpp

------------------------------------------------------------------------------
 INPUT AND OUTPUT DEFINITIONS

wb_syn_filt

     int16 a[],               (i) Q12 : a[m+1] prediction coefficients
     int16 m,                 (i)     : order of LP filter
     int16 x[],               (i)     : input signal
     int16 y[],               (o)     : output signal
     int16 lg,                (i)     : size of filtering
     int16 mem[],             (i/o)   : memory associated with this filtering.
     int16 update,            (i)     : 0=no update, 1=update of memory.
     int16 y_buf[]

Syn_filt_32

     int16 a[],              (i) Q12 : a[m+1] prediction coefficients
     int16 m,                (i)     : order of LP filter
     int16 exc[],            (i) Qnew: excitation (exc[i] >> Qnew)
     int16 Qnew,             (i)     : exc scaling = 0(min) to 8(max)
     int16 sig_hi[],         (o) /16 : synthesis high
     int16 sig_lo[],         (o) /16 : synthesis low
     int16 lg                (i)     : size of filtering

------------------------------------------------------------------------------
 FUNCTION DESCRIPTION

    Do the synthesis filtering 1/A(z)  16 and 32-bits version

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
#include "pvamrwbdecoder_mem_funcs.h"
#include "pvamrwbdecoder_basic_op.h"
#include "pvamrwb_math_op.h"
#include "pvamrwbdecoder_cnst.h"
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

void wb_syn_filt(
    int16 a[],       /* (i) Q12 : a[m+1] prediction coefficients           */
    int16 m,         /* (i)     : order of LP filter                       */
    int16 x[],       /* (i)     : input signal                             */
    int16 y[],       /* (o)     : output signal                            */
    int16 lg,        /* (i)     : size of filtering                        */
    int16 mem[],     /* (i/o)   : memory associated with this filtering.   */
    int16 update,    /* (i)     : 0=no update, 1=update of memory.         */
    int16 y_buf[]
)
{

    int16 i, j;
    int32 L_tmp1;
    int32 L_tmp2;
    int32 L_tmp3;
    int32 L_tmp4;
    int16 *yy;

    /* copy initial filter states into synthesis buffer */
    pv_memcpy(y_buf, mem, m*sizeof(*yy));

    yy = &y_buf[m];

    /* Do the filtering. */

    for (i = 0; i < lg >> 2; i++)
    {
        L_tmp1 = -((int32)x[(i<<2)] << 11);
        L_tmp2 = -((int32)x[(i<<2)+1] << 11);
        L_tmp3 = -((int32)x[(i<<2)+2] << 11);
        L_tmp4 = -((int32)x[(i<<2)+3] << 11);

        /* a[] uses Q12 and abs(a) =< 1 */

        L_tmp1  = fxp_mac_16by16(yy[(i<<2) -3], a[3], L_tmp1);
        L_tmp2  = fxp_mac_16by16(yy[(i<<2) -2], a[3], L_tmp2);
        L_tmp1  = fxp_mac_16by16(yy[(i<<2) -2], a[2], L_tmp1);
        L_tmp2  = fxp_mac_16by16(yy[(i<<2) -1], a[2], L_tmp2);
        L_tmp1  = fxp_mac_16by16(yy[(i<<2) -1], a[1], L_tmp1);

        for (j = 4; j < m; j += 2)
        {
            L_tmp1  = fxp_mac_16by16(yy[(i<<2)-1  - j], a[j+1], L_tmp1);
            L_tmp2  = fxp_mac_16by16(yy[(i<<2)    - j], a[j+1], L_tmp2);
            L_tmp1  = fxp_mac_16by16(yy[(i<<2)    - j], a[j  ], L_tmp1);
            L_tmp2  = fxp_mac_16by16(yy[(i<<2)+1  - j], a[j  ], L_tmp2);
            L_tmp3  = fxp_mac_16by16(yy[(i<<2)+1  - j], a[j+1], L_tmp3);
            L_tmp4  = fxp_mac_16by16(yy[(i<<2)+2  - j], a[j+1], L_tmp4);
            L_tmp3  = fxp_mac_16by16(yy[(i<<2)+2  - j], a[j  ], L_tmp3);
            L_tmp4  = fxp_mac_16by16(yy[(i<<2)+3  - j], a[j  ], L_tmp4);
        }

        L_tmp1  = fxp_mac_16by16(yy[(i<<2)    - j], a[j], L_tmp1);
        L_tmp2  = fxp_mac_16by16(yy[(i<<2)+1  - j], a[j], L_tmp2);
        L_tmp3  = fxp_mac_16by16(yy[(i<<2)+2  - j], a[j], L_tmp3);
        L_tmp4  = fxp_mac_16by16(yy[(i<<2)+3  - j], a[j], L_tmp4);

        L_tmp1 = shl_int32(L_tmp1, 4);

        y[(i<<2)] = yy[(i<<2)] = amr_wb_round(-L_tmp1);

        L_tmp2  = fxp_mac_16by16(yy[(i<<2)], a[1], L_tmp2);

        L_tmp2 = shl_int32(L_tmp2, 4);

        y[(i<<2)+1] = yy[(i<<2)+1] = amr_wb_round(-L_tmp2);

        L_tmp3  = fxp_mac_16by16(yy[(i<<2) - 1], a[3], L_tmp3);
        L_tmp4  = fxp_mac_16by16(yy[(i<<2)], a[3], L_tmp4);
        L_tmp3  = fxp_mac_16by16(yy[(i<<2)], a[2], L_tmp3);
        L_tmp4  = fxp_mac_16by16(yy[(i<<2) + 1], a[2], L_tmp4);
        L_tmp3  = fxp_mac_16by16(yy[(i<<2) + 1], a[1], L_tmp3);

        L_tmp3 = shl_int32(L_tmp3, 4);

        y[(i<<2)+2] = yy[(i<<2)+2] = amr_wb_round(-L_tmp3);

        L_tmp4  = fxp_mac_16by16(yy[(i<<2)+2], a[1], L_tmp4);

        L_tmp4 = shl_int32(L_tmp4, 4);

        y[(i<<2)+3] = yy[(i<<2)+3] = amr_wb_round(-L_tmp4);
    }


    /* Update memory if required */

    if (update)
    {
        pv_memcpy(mem, &y[lg - m], m*sizeof(*y));
    }

    return;
}

/*----------------------------------------------------------------------------
; FUNCTION CODE
----------------------------------------------------------------------------*/

void Syn_filt_32(
    int16 a[],              /* (i) Q12 : a[m+1] prediction coefficients */
    int16 m,                /* (i)     : order of LP filter             */
    int16 exc[],            /* (i) Qnew: excitation (exc[i] >> Qnew)    */
    int16 Qnew,             /* (i)     : exc scaling = 0(min) to 8(max) */
    int16 sig_hi[],         /* (o) /16 : synthesis high                 */
    int16 sig_lo[],         /* (o) /16 : synthesis low                  */
    int16 lg                /* (i)     : size of filtering              */
)
{
    int16 i, k, a0;
    int32 L_tmp1;
    int32 L_tmp2;
    int32 L_tmp3;
    int32 L_tmp4;

    a0 = 9 - Qnew;        /* input / 16 and >>Qnew */

    /* Do the filtering. */

    for (i = 0; i < lg >> 1; i++)
    {

        L_tmp3 = 0;
        L_tmp4 = 0;

        L_tmp1 = fxp_mul_16by16(sig_lo[(i<<1) - 1], a[1]);
        L_tmp2 = fxp_mul_16by16(sig_hi[(i<<1) - 1], a[1]);

        for (k = 2; k < m; k += 2)
        {

            L_tmp1 = fxp_mac_16by16(sig_lo[(i<<1)-1 - k], a[k+1], L_tmp1);
            L_tmp2 = fxp_mac_16by16(sig_hi[(i<<1)-1 - k], a[k+1], L_tmp2);
            L_tmp1 = fxp_mac_16by16(sig_lo[(i<<1)   - k], a[k  ], L_tmp1);
            L_tmp2 = fxp_mac_16by16(sig_hi[(i<<1)   - k], a[k  ], L_tmp2);
            L_tmp3 = fxp_mac_16by16(sig_lo[(i<<1)   - k], a[k+1], L_tmp3);
            L_tmp4 = fxp_mac_16by16(sig_hi[(i<<1)   - k], a[k+1], L_tmp4);
            L_tmp3 = fxp_mac_16by16(sig_lo[(i<<1)+1 - k], a[k  ], L_tmp3);
            L_tmp4 = fxp_mac_16by16(sig_hi[(i<<1)+1 - k], a[k  ], L_tmp4);
        }

        L_tmp1 = -fxp_mac_16by16(sig_lo[(i<<1)   - k], a[k], L_tmp1);
        L_tmp3 =  fxp_mac_16by16(sig_lo[(i<<1)+1 - k], a[k], L_tmp3);
        L_tmp2 =  fxp_mac_16by16(sig_hi[(i<<1)   - k], a[k], L_tmp2);
        L_tmp4 =  fxp_mac_16by16(sig_hi[(i<<1)+1 - k], a[k], L_tmp4);



        L_tmp1 >>= 11;      /* -4 : sig_lo[i] << 4 */

        L_tmp1 += (int32)exc[(i<<1)] << a0;

        L_tmp1 -= (L_tmp2 << 1);
        /* sig_hi = bit16 to bit31 of synthesis */
        L_tmp1 = shl_int32(L_tmp1, 3);           /* ai in Q12 */

        sig_hi[(i<<1)] = (int16)(L_tmp1 >> 16);

        L_tmp4 = fxp_mac_16by16((int16)(L_tmp1 >> 16), a[1], L_tmp4);

        /* sig_lo = bit4 to bit15 of synthesis */
        /* L_tmp1 >>= 4 : sig_lo[i] >> 4 */
        sig_lo[(i<<1)] = (int16)((L_tmp1 >> 4) - ((L_tmp1 >> 16) << 12));

        L_tmp3 = fxp_mac_16by16(sig_lo[(i<<1)], a[1], L_tmp3);
        L_tmp3 = -L_tmp3 >> 11;

        L_tmp3 += (int32)exc[(i<<1)+1] << a0;

        L_tmp3 -= (L_tmp4 << 1);
        /* sig_hi = bit16 to bit31 of synthesis */
        L_tmp3 = shl_int32(L_tmp3, 3);           /* ai in Q12 */
        sig_hi[(i<<1)+1] = (int16)(L_tmp3 >> 16);

        /* sig_lo = bit4 to bit15 of synthesis */
        /* L_tmp1 >>= 4 : sig_lo[i] >> 4 */
        sig_lo[(i<<1)+1] = (int16)((L_tmp3 >> 4) - (sig_hi[(i<<1)+1] << 12));
    }

}


