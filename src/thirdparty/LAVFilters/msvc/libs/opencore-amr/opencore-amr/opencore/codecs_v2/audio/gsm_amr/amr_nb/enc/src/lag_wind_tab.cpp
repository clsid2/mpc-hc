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

 Filename: lag_wind_tab.cpp

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

      File             : lag_wind.tab
      Purpose          : Table of lag_window for autocorrelation.

 *-----------------------------------------------------*
 | Table of lag_window for autocorrelation.            |
 |                                                     |
 | noise floor = 1.0001   = (0.9999  on r[1] ..r[10])  |
 | Bandwitdh expansion = 60 Hz                         |
 |                                                     |
 |                                                     |
 | lag_wind[0] =  1.00000000    (not stored)           |
 | lag_wind[1] =  0.99879038                           |
 | lag_wind[2] =  0.99546897                           |
 | lag_wind[3] =  0.98995781                           |
 | lag_wind[4] =  0.98229337                           |
 | lag_wind[5] =  0.97252619                           |
 | lag_wind[6] =  0.96072036                           |
 | lag_wind[7] =  0.94695264                           |
 | lag_wind[8] =  0.93131179                           |
 | lag_wind[9] =  0.91389757                           |
 | lag_wind[10]=  0.89481968                           |
 -------------------------------------------------------

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
#include "lag_wind_tab.h"

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

    /*----------------------------------------------------------------------------
    ; LOCAL FUNCTION DEFINITIONS
    ; Function Prototype declaration
    ----------------------------------------------------------------------------*/


    /*----------------------------------------------------------------------------
    ; LOCAL STORE/BUFFER/POINTER DEFINITIONS
    ; Variable declaration - defined here and used outside this module
    ----------------------------------------------------------------------------*/
    const Word16 lag_h[10] =
    {
        32728,
        32619,
        32438,
        32187,
        31867,
        31480,
        31029,
        30517,
        29946,
        29321
    };

    const Word16 lag_l[10] =
    {
        11904,
        17280,
        30720,
        25856,
        24192,
        28992,
        24384,
        7360,
        19520,
        14784
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

