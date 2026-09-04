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



 Filename: dec_acelp_2p_in_64.cpp

------------------------------------------------------------------------------
 INPUT AND OUTPUT DEFINITIONS

     int16 index,          (i):   12 bits index
     int16 code[]          (o): Q9 algebraic (fixed) codebook excitation

------------------------------------------------------------------------------
 FUNCTION DESCRIPTION

   12 bits algebraic codebook decoder.
   2 tracks x 32 positions per track = 64 samples.

   12 bits --> 2 pulses in a frame of 64 samples.

   All pulses can have two (2) possible amplitudes: +1 or -1.
   Each pulse can have 32 possible positions.

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
#define L_CODE    64                       /* codevector length  */
#define NB_TRACK  2                        /* number of track    */
#define NB_POS    32                       /* number of position */


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

void dec_acelp_2p_in_64(
    int16 index,        /* (i):   12 bits index                          */
    int16 code[]        /* (o): Q9 algebraic (fixed) codebook excitation */
)
{
    int16 i;

    pv_memset(code, 0, L_CODE*sizeof(*code));

    /* decode the positions and signs of pulses and build the codeword */

    i = (index >> 5) & 0x003E;

    if (((index >> 6) & NB_POS) == 0)
    {
        code[i] = 512;
    }
    else
    {
        code[i] = -512;
    }

    i = ((index & 0x001F) << 1) + 1;

    if ((index & NB_POS) == 0)
    {
        code[i] = 512;
    }
    else
    {
        code[i] = -512;
    }

}
