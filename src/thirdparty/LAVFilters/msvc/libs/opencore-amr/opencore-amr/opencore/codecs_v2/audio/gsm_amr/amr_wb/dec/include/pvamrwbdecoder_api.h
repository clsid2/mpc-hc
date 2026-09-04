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

 Name: pvamrwbdecoder_api.h

------------------------------------------------------------------------------
 INCLUDE DESCRIPTION

 Main header file for the Packet Video AMR Wide  Band  decoder library. The
 constants, structures, and functions defined within this file, along with
 a basic data types header file, is all that is needed to use and communicate
 with the library. The internal data structures within the library are
 purposely hidden.

 ---* Need description of the input buffering. *-------

 ---* Need an example of calling the library here *----

------------------------------------------------------------------------------
 REFERENCES

  (Normally header files do not have a reference section)

------------------------------------------------------------------------------
*/

/*----------------------------------------------------------------------------
; CONTINUE ONLY IF NOT ALREADY DEFINED
----------------------------------------------------------------------------*/
#ifndef _PVAMRWBDECODER_API_H
#define _PVAMRWBDECODER_API_H

#include    "oscl_base.h"    /* Basic data types used within the lib */


/*----------------------------------------------------------------------------
; INCLUDES
----------------------------------------------------------------------------*/
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
    ; Include all pre-processor statements here.
    ----------------------------------------------------------------------------*/

#define AMR_WB_PCM_FRAME   320             /* Frame size at 16kHz     */


#define NBBITS_7k     132                  /* 6.60k  */
#define NBBITS_9k     177                  /* 8.85k  */
#define NBBITS_12k    253                  /* 12.65k */
#define NBBITS_14k    285                  /* 14.25k */
#define NBBITS_16k    317                  /* 15.85k */
#define NBBITS_18k    365                  /* 18.25k */
#define NBBITS_20k    397                  /* 19.85k */
#define NBBITS_23k    461                  /* 23.05k */
#define NBBITS_24k    477                  /* 23.85k */

#define NBBITS_SID    35

#define KAMRWB_NB_BITS_MAX   NBBITS_24k
#define KAMRWB_NB_BYTES_MAX  ((KAMRWB_NB_BITS_MAX>>3)+1)

#define NUM_OF_MODES  10


    static const int16 AMR_WB_COMPRESSED[NUM_OF_MODES] =
    {
        NBBITS_7k,
        NBBITS_9k,
        NBBITS_12k,
        NBBITS_14k,
        NBBITS_16k,
        NBBITS_18k,
        NBBITS_20k,
        NBBITS_23k,
        NBBITS_24k,
        NBBITS_SID
    };

    /*----------------------------------------------------------------------------
    ; EXTERNAL VARIABLES REFERENCES
    ; Declare variables used in this module but defined elsewhere
    ----------------------------------------------------------------------------*/

    /*----------------------------------------------------------------------------
    ; SIMPLE TYPEDEF'S
    ----------------------------------------------------------------------------*/

    /*----------------------------------------------------------------------------
    ; ENUMERATED TYPEDEF'S
    ----------------------------------------------------------------------------*/


    /*----------------------------------------------------------------------------
    ; GLOBAL FUNCTION DEFINITIONS
    ; Function Prototype declaration
    ----------------------------------------------------------------------------*/


    /*----------------------------------------------------------------------------
    ; END
    ----------------------------------------------------------------------------*/

#ifdef __cplusplus
}
#endif


#endif  /* PVMP4AUDIODECODER_API_H */


