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



 Filename: pit_shrp.cpp

------------------------------------------------------------------------------
 INPUT AND OUTPUT DEFINITIONS

     int16 * x,           in/out: impulse response (or algebraic code)
     int16 pit_lag,       input : pitch lag
     int16 sharp,         input : pitch sharpening factor (Q15)
     int16 L_subfr        input : subframe size

------------------------------------------------------------------------------
 FUNCTION DESCRIPTION

    Performs Pitch sharpening routine

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

void Pit_shrp(
    int16 * x,         /* in/out: impulse response (or algebraic code) */
    int16 pit_lag,     /* input : pitch lag                            */
    int16 sharp,       /* input : pitch sharpening factor (Q15)        */
    int16 L_subfr      /* input : subframe size                        */
)
{
    int16 i;
    int32 L_tmp;

    for (i = pit_lag; i < L_subfr; i++)
    {
        L_tmp = mac_16by16_to_int32((int32)x[i] << 16, x[i - pit_lag], sharp);
        x[i] = amr_wb_round(L_tmp);

    }

    return;
}
