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

 Name: pvamrwbdecoder.h

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
#ifndef _PVAMRWBDECODER_H
#define _PVAMRWBDECODER_H

#include    "oscl_base.h"    /* Basic data types used within the lib */
#include    "pvamrwbdecoder_api.h"
#include    "pvgsmamrdecoderinterface.h"

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
    ; STRUCTURES TYPEDEF'S
    ----------------------------------------------------------------------------*/

    /*----------------------------------------------------------------------------
    ; GLOBAL FUNCTION DEFINITIONS
    ; Function Prototype declaration
    ----------------------------------------------------------------------------*/

    void pvDecoder_AmrWb_Init(void **spd_state, void *st, int16 ** ScratchMem);

    int32 pvDecoder_AmrWb(
        int16 mode,                          /* input : used mode             */
        int16 prms[],                        /* input : parameter vector      */
        int16 synth16k[],                    /* output: synthesis speech      */
        int16 * frame_length,                /* output:  lenght of the frame  */
        void *spd_state,                     /* i/o   : State structure       */
        int16 frame_type,                    /* input : received frame type   */
        int16 ScratchMem[]
    );

    void pvDecoder_AmrWb_Reset(void *st, int16 reset_all);

    int16 pvDecoder_AmrWb_homing_frame_test(int16 input_frame[], int16 mode);

    int16 pvDecoder_AmrWb_homing_frame_test_first(int16 input_frame[], int16 mode);

    int32 pvDecoder_AmrWbMemRequirements();

    void mime_unsorting(uint8 packet[],
                        int16 compressed_data[],
                        int16 *frame_type,
                        int16 *mode,
                        uint8 q,
                        RX_State *st);


    /*----------------------------------------------------------------------------
    ; END
    ----------------------------------------------------------------------------*/

#ifdef __cplusplus
}
#endif


#endif  /* PVMP4AUDIODECODER_API_H */


