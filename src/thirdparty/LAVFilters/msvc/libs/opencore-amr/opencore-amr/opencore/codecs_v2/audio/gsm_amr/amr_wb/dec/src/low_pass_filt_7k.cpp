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



 Filename: low_pass_filt_7k.cpp

------------------------------------------------------------------------------
 INPUT AND OUTPUT DEFINITIONS

     int16 signal[],             input signal / output is divided by 16
     int16 lg,                   lenght of signal
     int16 mem[]                 in/out: memory (size=30)
     int16 x[]                   scratch mem ( size= 60)

------------------------------------------------------------------------------
 FUNCTION DESCRIPTION

        15th order high pass 7kHz FIR filter


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

/*----------------------------------------------------------------------------
; MACROS
; Define module specific macros here
----------------------------------------------------------------------------*/


/*----------------------------------------------------------------------------
; DEFINES
; Include all pre-processor statements here. Include conditional
; compile variables also.
----------------------------------------------------------------------------*/
#define L_FIR 30

/*----------------------------------------------------------------------------
; LOCAL FUNCTION DEFINITIONS
; Function Prototype declaration
----------------------------------------------------------------------------*/

/*----------------------------------------------------------------------------
; LOCAL STORE/BUFFER/POINTER DEFINITIONS
; Variable declaration - defined here and used outside this module
----------------------------------------------------------------------------*/
const int16 fir_7k[L_FIR+1] =
{
    -21, 47, -89, 146, -203,
    229, -177, 0, 335, -839,
    1485, -2211, 2931, -3542, 3953,
    28682, 3953, -3542, 2931, -2211,
    1485, -839, 335, 0, -177,
    229, -203, 146, -89, 47,
    -21
};

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


void low_pass_filt_7k_init(int16 mem[])            /* mem[30] */
{
    pv_memset((void *)mem, 0, (L_FIR)*sizeof(*mem));

    return;
}


/*----------------------------------------------------------------------------
; FUNCTION CODE
----------------------------------------------------------------------------*/


void low_pass_filt_7k(
    int16 signal[],                      /* input:  signal                  */
    int16 lg,                            /* input:  length of input         */
    int16 mem[],                         /* in/out: memory (size=30)        */
    int16 x[]
)
{
    int16 i, j;
    int32 L_tmp1;
    int32 L_tmp2;
    int32 L_tmp3;
    int32 L_tmp4;

    pv_memcpy((void *)x, (void *)mem, (L_FIR)*sizeof(*x));

    for (i = 0; i < lg >> 2; i++)
    {
        x[(i<<2) + L_FIR    ] = signal[(i<<2)];
        x[(i<<2) + L_FIR + 1] = signal[(i<<2)+1];
        x[(i<<2) + L_FIR + 2] = signal[(i<<2)+2];
        x[(i<<2) + L_FIR + 3] = signal[(i<<2)+3];

        L_tmp1 = fxp_mac_16by16(x[(i<<2)] + signal[(i<<2)], fir_7k[0], 0x00004000);
        L_tmp2 = fxp_mac_16by16(x[(i<<2)+1] + signal[(i<<2)+1], fir_7k[0], 0x00004000);
        L_tmp3 = fxp_mac_16by16(x[(i<<2)+2] + signal[(i<<2)+2], fir_7k[0], 0x00004000);
        L_tmp4 = fxp_mac_16by16(x[(i<<2)+3] + signal[(i<<2)+3], fir_7k[0], 0x00004000);

        for (j = 1; j < L_FIR - 1; j += 4)
        {


            int16 tmp1 = x[(i<<2)+j  ];
            int16 tmp2 = x[(i<<2)+j+1];
            int16 tmp3 = x[(i<<2)+j+2];

            L_tmp1 = fxp_mac_16by16(tmp1, fir_7k[j  ], L_tmp1);
            L_tmp2 = fxp_mac_16by16(tmp2, fir_7k[j  ], L_tmp2);
            L_tmp1 = fxp_mac_16by16(tmp2, fir_7k[j+1], L_tmp1);
            L_tmp2 = fxp_mac_16by16(tmp3, fir_7k[j+1], L_tmp2);
            L_tmp3 = fxp_mac_16by16(tmp3, fir_7k[j  ], L_tmp3);
            L_tmp1 = fxp_mac_16by16(tmp3, fir_7k[j+2], L_tmp1);

            tmp1 = x[(i<<2)+j+3];
            tmp2 = x[(i<<2)+j+4];

            L_tmp2 = fxp_mac_16by16(tmp1, fir_7k[j+2], L_tmp2);
            L_tmp4 = fxp_mac_16by16(tmp1, fir_7k[j  ], L_tmp4);
            L_tmp3 = fxp_mac_16by16(tmp1, fir_7k[j+1], L_tmp3);
            L_tmp1 = fxp_mac_16by16(tmp1, fir_7k[j+3], L_tmp1);
            L_tmp2 = fxp_mac_16by16(tmp2, fir_7k[j+3], L_tmp2);
            L_tmp4 = fxp_mac_16by16(tmp2, fir_7k[j+1], L_tmp4);
            L_tmp3 = fxp_mac_16by16(tmp2, fir_7k[j+2], L_tmp3);

            tmp1 = x[(i<<2)+j+5];
            tmp2 = x[(i<<2)+j+6];

            L_tmp4 = fxp_mac_16by16(tmp1, fir_7k[j+2], L_tmp4);
            L_tmp3 = fxp_mac_16by16(tmp1, fir_7k[j+3], L_tmp3);
            L_tmp4 = fxp_mac_16by16(tmp2, fir_7k[j+3], L_tmp4);

        }

        L_tmp1 = fxp_mac_16by16(x[(i<<2)+j  ], fir_7k[j  ], L_tmp1);
        L_tmp2 = fxp_mac_16by16(x[(i<<2)+j+1], fir_7k[j  ], L_tmp2);
        L_tmp3 = fxp_mac_16by16(x[(i<<2)+j+2], fir_7k[j  ], L_tmp3);
        L_tmp4 = fxp_mac_16by16(x[(i<<2)+j+3], fir_7k[j  ], L_tmp4);

        signal[(i<<2)] = (int16)(L_tmp1 >> 15);
        signal[(i<<2)+1] = (int16)(L_tmp2 >> 15);
        signal[(i<<2)+2] = (int16)(L_tmp3 >> 15);
        signal[(i<<2)+3] = (int16)(L_tmp4 >> 15);

    }

    pv_memcpy((void *)mem, (void *)(x + lg), (L_FIR)*sizeof(*mem));

    return;
}

