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
 INCLUDE DESCRIPTION

 This file contains the Speech code (encoder, decoder, and postfilter)
 constant parameters.

 NOTE: This file must be synchronized with /gsm-amr/asm/include/cnst.inc file.

------------------------------------------------------------------------------
*/

/*----------------------------------------------------------------------------
; CONTINUE ONLY IF NOT ALREADY DEFINED
----------------------------------------------------------------------------*/
#ifndef _CNST_H_
#define _CNST_H_

/*----------------------------------------------------------------------------
; INCLUDES
----------------------------------------------------------------------------*/

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
    ; Include all pre-processor statements here.
    ----------------------------------------------------------------------------*/
#define L_TOTAL      320       /* Total size of speech buffer.             */
#define L_WINDOW     240       /* Window size in LP analysis               */
#define L_FRAME      160       /* Frame size                               */
#define L_FRAME_BY2  80        /* Frame size divided by 2                  */
#define L_SUBFR      40        /* Subframe size                            */
#define L_CODE       40        /* codevector length                        */
#define NB_TRACK     5         /* number of tracks                         */
#define STEP         5         /* codebook step size                       */
#define NB_TRACK_MR102  4      /* number of tracks mode mr102              */
#define STEP_MR102      4      /* codebook step size mode mr102            */
#define M            10        /* Order of LP filter                       */
#define MP1          (M+1)     /* Order of LP filter + 1                   */
#define LSF_GAP      205       /* Minimum distance between LSF after quan- */
    /* tization; 50 Hz = 205                    */
#define LSP_PRED_FAC_MR122 21299 /* MR122 LSP prediction factor (0.65 Q15) */
#define AZ_SIZE       (4*M+4)  /* Size of array of LP filters in 4 subfr.s */
#define PIT_MIN_MR122 18       /* Minimum pitch lag (MR122 mode)           */
#define PIT_MIN       20       /* Minimum pitch lag (all other modes)      */
#define PIT_MAX       143      /* Maximum pitch lag                        */
#define L_INTERPOL    (10+1)   /* Length of filter for interpolation       */
#define L_INTER_SRCH  4        /* Length of filter for CL LTP search       */
    /* interpolation                            */

#define MU       26214         /* Factor for tilt compensation filter 0.8  */
#define AGC_FAC  29491         /* Factor for automatic gain control 0.9    */

#define L_NEXT       40        /* Overhead in LP analysis                  */
#define SHARPMAX  13017        /* Maximum value of pitch sharpening        */
#define SHARPMIN  0            /* Minimum value of pitch sharpening        */


#define MAX_PRM_SIZE    57     /* max. num. of params                      */
#define MAX_SERIAL_SIZE 244    /* max. num. of serial bits                 */

#define GP_CLIP   15565        /* Pitch gain clipping = 0.95               */
#define N_FRAME   7            /* old pitch gains in average calculation   */

#define EHF_MASK 0x0008        /* encoder homing frame pattern             */

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
#ifdef __cplusplus
}
#endif

#endif  /* _CNST_H_ */
