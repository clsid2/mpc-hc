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

 Name: pvamrwbdecoder_cnst.h

------------------------------------------------------------------------------
 INCLUDE DESCRIPTION

 Main header file for the Packet Video AMR Wide  Band  decoder library. The
 constants, structures, and functions defined within this file, along with
 a basic data types header file, is all that is needed to use and communicate
 with the library. The internal data structures within the library are
 purposely hidden.


------------------------------------------------------------------------------
 REFERENCES

  (Normally header files do not have a reference section)

------------------------------------------------------------------------------
*/

/*----------------------------------------------------------------------------
; CONTINUE ONLY IF NOT ALREADY DEFINED
----------------------------------------------------------------------------*/

#ifndef PVAMRWBDECODER_CNST_H
#define PVAMRWBDECODER_CNST_H

/*----------------------------------------------------------------------------
; INCLUDES
----------------------------------------------------------------------------*/

/*----------------------------------------------------------------------------
; MACROS
; Define module specific macros here
----------------------------------------------------------------------------*/

/*----------------------------------------------------------------------------
; DEFINES
; Include all pre-processor statements here.
----------------------------------------------------------------------------*/

#define L_FRAME      256                   /* Frame size                                 */
#define L_SUBFR16k   80                    /* Subframe size at 16kHz                     */

#define L_SUBFR      64                    /* Subframe size                              */
#define NB_SUBFR     4                     /* Number of subframe per frame               */

#define L_NEXT       64                    /* Overhead in LP analysis                    */
#define L_WINDOW     384                   /* window size in LP analysis                 */
#define L_TOTAL      384                   /* Total size of speech buffer.               */
#define M            16                    /* Order of LP filter                         */
#define M16k             20

#define L_FILT16k    15                    /* Delay of down-sampling filter              */
#define L_FILT       12                    /* Delay of up-sampling filter                */

#define GP_CLIP      15565                 /* Pitch gain clipping = 0.95 Q14             */
#define PIT_SHARP    27853                 /* pitch sharpening factor = 0.85 Q15         */

#define PIT_MIN      34                    /* Minimum pitch lag with resolution 1/4      */
#define PIT_FR2      128                   /* Minimum pitch lag with resolution 1/2      */
#define PIT_FR1_9b   160                   /* Minimum pitch lag with resolution 1        */
#define PIT_FR1_8b   92                    /* Minimum pitch lag with resolution 1        */
#define PIT_MAX      231                   /* Maximum pitch lag                          */
#define L_INTERPOL   (16+1)                /* Length of filter for interpolation         */

#define OPL_DECIM    2                     /* Decimation in open-loop pitch analysis     */

#define PREEMPH_FAC  22282                 /* preemphasis factor (0.68 in Q15)           */
#define GAMMA1       30147                 /* Weighting factor (numerator) (0.92 in Q15) */
#define TILT_FAC     22282                 /* tilt factor (denominator) (0.68 in Q15)    */

#define Q_MAX        8                     /* scaling max for signal (see syn_filt_32)   */

#define RANDOM_INITSEED  21845             /* own random init value                      */

#define L_MEANBUF        3
#define ONE_PER_MEANBUF 10923

#define MODE_7k       0
#define MODE_9k       1
#define MODE_12k      2
#define MODE_14k      3
#define MODE_16k      4
#define MODE_18k      5
#define MODE_20k      6
#define MODE_23k      7
#define MODE_24k      8
#define MRDTX         9
//#define NUM_OF_MODES  10                   /* see bits.h for bits definition             */

#define EHF_MASK (int16)0x0008            /* homing frame pattern                       */

#define BIT_0     (int16)-127
#define BIT_1     (int16)127
#define BIT_0_ITU (int16)0x007F
#define BIT_1_ITU (int16)0x0081

/*----------------------------------------------------------------------------
; END
----------------------------------------------------------------------------*/

#endif
