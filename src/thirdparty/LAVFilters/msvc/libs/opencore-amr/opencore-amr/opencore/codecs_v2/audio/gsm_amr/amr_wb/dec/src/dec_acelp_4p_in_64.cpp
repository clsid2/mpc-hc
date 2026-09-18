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



 Filename: dec_acelp_4p_in_64.cpp

------------------------------------------------------------------------------
 INPUT AND OUTPUT DEFINITIONS

     int16 index[],    (i) : index (20): 5+5+5+5 = 20 bits.
                       (i) : index (36): 9+9+9+9 = 36 bits.
                       (i) : index (44): 13+9+13+9 = 44 bits.
                       (i) : index (52): 13+13+13+13 = 52 bits.
                       (i) : index (64): 2+2+2+2+14+14+14+14 = 64 bits.
                       (i) : index (72): 10+2+10+2+10+14+10+14 = 72 bits.
                       (i) : index (88): 11+11+11+11+11+11+11+11 = 88 bits.
     int16 nbbits,     (i) : 20, 36, 44, 52, 64, 72 or 88 bits
     int16 code[]      (o) Q9: algebraic (fixed) codebook excitation

------------------------------------------------------------------------------
 FUNCTION DESCRIPTION

   20, 36, 44, 52, 64, 72, 88 bits algebraic codebook decoder.
   4 tracks x 16 positions per track = 64 samples.

   20 bits --> 4 pulses in a frame of 64 samples.
   36 bits --> 8 pulses in a frame of 64 samples.
   44 bits --> 10 pulses in a frame of 64 samples.
   52 bits --> 12 pulses in a frame of 64 samples.
   64 bits --> 16 pulses in a frame of 64 samples.
   72 bits --> 18 pulses in a frame of 64 samples.
   88 bits --> 24 pulses in a frame of 64 samples.

   All pulses can have two (2) possible amplitudes: +1 or -1.
   Each pulse can have sixteen (16) possible positions.

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

#include "q_pulse.h"

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
#define NB_TRACK  4                        /* number of track    */
#define NB_POS    16                       /* number of position */

/*----------------------------------------------------------------------------
; LOCAL FUNCTION DEFINITIONS
; Function Prototype declaration
----------------------------------------------------------------------------*/
#ifdef __cplusplus
extern "C"
{
#endif

    void add_pulses(int16 pos[], int16 nb_pulse, int16 track, int16 code[]);

#ifdef __cplusplus
}
#endif

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

void dec_acelp_4p_in_64(
    int16 index[],  /* (i) : index (20): 5+5+5+5 = 20 bits.                 */
    /* (i) : index (36): 9+9+9+9 = 36 bits.                 */
    /* (i) : index (44): 13+9+13+9 = 44 bits.               */
    /* (i) : index (52): 13+13+13+13 = 52 bits.             */
    /* (i) : index (64): 2+2+2+2+14+14+14+14 = 64 bits.     */
    /* (i) : index (72): 10+2+10+2+10+14+10+14 = 72 bits.   */
    /* (i) : index (88): 11+11+11+11+11+11+11+11 = 88 bits. */
    int16 nbbits,   /* (i) : 20, 36, 44, 52, 64, 72 or 88 bits              */
    int16 code[]    /* (o) Q9: algebraic (fixed) codebook excitation        */
)
{
    int16 k, pos[6];
    int32 L_index;
    pv_memset(code, 0, L_CODE*sizeof(*code));

    /* decode the positions and signs of pulses and build the codeword */


    switch (nbbits)
    {
        case 20:
            for (k = 0; k < NB_TRACK; k++)
            {
                L_index = index[k];
                dec_1p_N1(L_index, 4, 0, pos);
                add_pulses(pos, 1, k, code);
            }
            break;

        case  36:
            for (k = 0; k < NB_TRACK; k++)
            {
                L_index = index[k];
                dec_2p_2N1(L_index, 4, 0, pos);
                add_pulses(pos, 2, k, code);
            }
            break;
        case 44:
            for (k = 0; k < NB_TRACK - 2; k++)
            {
                L_index = index[k];
                dec_3p_3N1(L_index, 4, 0, pos);
                add_pulses(pos, 3, k, code);
            }
            for (k = 2; k < NB_TRACK; k++)
            {
                L_index = index[k];
                dec_2p_2N1(L_index, 4, 0, pos);
                add_pulses(pos, 2, k, code);
            }
            break;
        case 52:
            for (k = 0; k < NB_TRACK; k++)
            {
                L_index = index[k];
                dec_3p_3N1(L_index, 4, 0, pos);
                add_pulses(pos, 3, k, code);
            }
            break;
        case 64:
            for (k = 0; k < NB_TRACK; k++)
            {
                L_index = ((int32)index[k] << 14) + index[k + NB_TRACK];
                dec_4p_4N(L_index, 4, 0, pos);
                add_pulses(pos, 4, k, code);
            }
            break;
        case 72:
            for (k = 0; k < NB_TRACK - 2; k++)
            {
                L_index = ((int32)index[k] << 10) + index[k + NB_TRACK];
                dec_5p_5N(L_index, 4, 0, pos);
                add_pulses(pos, 5, k, code);
            }
            for (k = 2; k < NB_TRACK; k++)
            {
                L_index = ((int32)index[k] << 14) + index[k + NB_TRACK];
                dec_4p_4N(L_index, 4, 0, pos);
                add_pulses(pos, 4, k, code);
            }
            break;
        case 88:
            for (k = 0; k < NB_TRACK; k++)
            {
                L_index = ((int32)index[k] << 11) + index[k + NB_TRACK];
                dec_6p_6N_2(L_index, 4, 0, pos);
                add_pulses(pos, 6, k, code);
            }
        default:
            break;
    }


}



void add_pulses(int16 pos[], int16 nb_pulse, int16 track, int16 code[])
{
    int16 i, k;

    for (k = 0; k < nb_pulse; k++)
    {
        /* i = ((pos[k] & (NB_POS-1))*NB_TRACK) + track; */
        i = ((pos[k] & (NB_POS - 1)) << 2) + track;

        if ((pos[k] & NB_POS) == 0)
        {
            code[i] +=  512;
        }
        else
        {
            code[i] -=  512;
        }
    }

}
