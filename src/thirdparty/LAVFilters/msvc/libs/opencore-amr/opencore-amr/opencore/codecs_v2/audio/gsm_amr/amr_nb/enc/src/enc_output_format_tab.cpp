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


 Filename: enc_output_format_tab.cpp

------------------------------------------------------------------------------
 INPUT AND OUTPUT DEFINITIONS

 Inputs:
    None

 Outputs:
    None

 Returns:
    None

 Global Variables Used:
    None

 Local Variables Needed:
    None

------------------------------------------------------------------------------
 FUNCTION DESCRIPTION

 This file contains the tables of the number of data bytes per codec mode in
 both WMF and IF2 output formats.

------------------------------------------------------------------------------
 REQUIREMENTS

 None

------------------------------------------------------------------------------
 REFERENCES

 [1] AMR Speech Codec Frame Structure, 3GPP TS 26.101 version 4.1.0 Release 4,
     June 2001

------------------------------------------------------------------------------
 PSEUDO-CODE


------------------------------------------------------------------------------
*/


/*----------------------------------------------------------------------------
; INCLUDES
----------------------------------------------------------------------------*/
#include "amrencode.h"

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
    /* Number of data bytes in an encoder frame for each codec mode */
    /* for WMF output format.                                       */
    /* Each entry is the sum of the 3GPP frame type byte and the    */
    /* number of packed core AMR data bytes                         */
    const Word16 WmfEncBytesPerFrame[16] =
    {
        13, /* 4.75 */
        14, /* 5.15 */
        16, /* 5.90 */
        18, /* 6.70 */
        20, /* 7.40 */
        21, /* 7.95 */
        27, /* 10.2 */
        32, /* 12.2 */
        6,  /* GsmAmr comfort noise */
        7,  /* Gsm-Efr comfort noise */
        6,  /* IS-641 comfort noise */
        6,  /* Pdc-Efr comfort noise */
        0,  /* future use */
        0,  /* future use */
        0,  /* future use */
        1   /* No transmission */
    };


    /* Number of data bytes in an encoder frame for each codec mode */
    /* for IF2 output format                                        */
    const Word16 If2EncBytesPerFrame[16] =
    {
        13, /* 4.75 */
        14, /* 5.15 */
        16, /* 5.90 */
        18, /* 6.70 */
        19, /* 7.40 */
        21, /* 7.95 */
        26, /* 10.2 */
        31, /* 12.2 */
        6, /* GsmAmr comfort noise */
        6, /* Gsm-Efr comfort noise */
        6, /* IS-641 comfort noise */
        6, /* Pdc-Efr comfort noise */
        0, /* future use */
        0, /* future use */
        0, /* future use */
        1  /* No transmission */
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


