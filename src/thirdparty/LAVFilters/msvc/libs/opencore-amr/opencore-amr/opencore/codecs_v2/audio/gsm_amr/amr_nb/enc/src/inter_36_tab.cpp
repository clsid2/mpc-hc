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
------------------------------------------------------------------------------

 Filename: inter_36_tab.cpp

------------------------------------------------------------------------------
 INPUT AND OUTPUT DEFINITIONS

 Inputs:
    None

 Local Stores/Buffers/Pointers Needed:
    None

 Global Stores/Buffers/Pointers Needed:
    None

 Outputs:
    None

 Pointers and Buffers Modified:
    None

 Local Stores Modified:
    None

 Global Stores Modified:
    None

------------------------------------------------------------------------------
 FUNCTION DESCRIPTION

      File             : inter_36.tab
      Purpose          : Tables for interpolating the normalized correlation
                         with 1/3 or 1/6 resolution.


------------------------------------------------------------------------------
 REQUIREMENTS

 None

------------------------------------------------------------------------------
 REFERENCES

 None

------------------------------------------------------------------------------
 PSEUDO-CODE


------------------------------------------------------------------------------
*/


/*----------------------------------------------------------------------------
; INCLUDES
----------------------------------------------------------------------------*/
#include "inter_36_tab.h"

/*--------------------------------------------------------------------------*/
#ifdef __cplusplus
extern "C"
{
#endif

    /*----------------------------------------------------------------------------
    ; MACROS
    ; Define module specific macros here
    ----------------------------------------------------------------------------*/


    /*----------------------------------------------------------------------------
    ; DEFINES
    ; Include all pre-processor statements here. Include conditional
    ; compile variables also.
    ----------------------------------------------------------------------------*/
#define UP_SAMP_MAX  6
#define FIR_SIZE     (UP_SAMP_MAX*L_INTER_SRCH+1)

    /*----------------------------------------------------------------------------
    ; LOCAL FUNCTION DEFINITIONS
    ; Function Prototype declaration
    ----------------------------------------------------------------------------*/


    /*----------------------------------------------------------------------------
    ; LOCAL STORE/BUFFER/POINTER DEFINITIONS
    ; Variable declaration - defined here and used outside this module
    ----------------------------------------------------------------------------*/
    /* 1/6 resolution interpolation filter  (-3 dB at 3600 Hz) */
    /* Note: The IS641 (7.4) 1/3 resolution filter is simply a subsampled
             version of the 1/6 resolution filter, i.e. it uses
             every second coefficient:

             inter_3[k] = inter_6[2*k], 0 <= k <= 3*L_INTER_SRCH
     */

    const Word16 inter_6[FIR_SIZE] =
    {
        29519,
        28316, 24906, 19838, 13896, 7945, 2755,
        -1127, -3459, -4304, -3969, -2899, -1561,
        -336, 534, 970, 1023, 823, 516,
        220, 0, -131, -194, -215, 0
    };

    /*----------------------------------------------------------------------------
    ; EXTERNAL FUNCTION REFERENCES
    ; Declare functions defined elsewhere and referenced in this module
    ----------------------------------------------------------------------------*/


    /*----------------------------------------------------------------------------
    ; EXTERNAL GLOBAL STORE/BUFFER/POINTER REFERENCES
    ; Declare variables used in this module but defined elsewhere
    ----------------------------------------------------------------------------*/


    /*--------------------------------------------------------------------------*/
#ifdef __cplusplus
}
#endif

/*----------------------------------------------------------------------------
; FUNCTION CODE
----------------------------------------------------------------------------*/

/*----------------------------------------------------------------------------
; Define all local variables
----------------------------------------------------------------------------*/


/*----------------------------------------------------------------------------
; Function body here
----------------------------------------------------------------------------*/


/*----------------------------------------------------------------------------
; Return nothing or data or data pointer
----------------------------------------------------------------------------*/





