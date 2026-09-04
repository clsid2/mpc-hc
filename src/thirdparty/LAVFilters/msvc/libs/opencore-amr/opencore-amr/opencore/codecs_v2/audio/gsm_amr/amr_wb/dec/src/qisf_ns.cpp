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



 Filename: qisf_ns.cpp

------------------------------------------------------------------------------
 INPUT AND OUTPUT DEFINITIONS

     int16 indice[] : indices of the selected codebook entries
     int16 isf[]    : quantized ISFs (in frequency domain)

------------------------------------------------------------------------------
 FUNCTION DESCRIPTION

   Coding/Decoding of ISF parameters for background noise.

   The ISF vector is quantized using VQ with split-by-5

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
#include "pvamrwbdecoder_acelp.h"
#include "qisf_ns.h"

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

void Disf_ns(
    int16 * indice,   /* input:  quantization indices               */
    int16 * isf_q     /* input: ISF in the frequency domain (0..0.5)*/
)
{
    int16 i;

    isf_q[0] = dico1_isf_noise[(indice[0] << 1)];
    isf_q[1] = dico1_isf_noise[(indice[0] << 1) + 1];

    for (i = 0; i < 3; i++)
    {
        isf_q[i + 2] = dico2_isf_noise[(indice[1] << 1) + indice[1] + i];
        isf_q[i + 5] = dico3_isf_noise[(indice[2] << 1) + indice[2] + i];
    }

    for (i = 0; i < 4; i++)
    {
        isf_q[i +  8] = dico4_isf_noise[(indice[3] << 2) + i];
        isf_q[i + 12] = dico5_isf_noise[(indice[4] << 2) + i];
    }

    for (i = 0; i < ORDER; i++)
    {
        isf_q[i] = add_int16(isf_q[i], mean_isf_noise[i]);
    }

    Reorder_isf(isf_q, ISF_GAP, ORDER);

}
