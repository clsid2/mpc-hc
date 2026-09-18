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

 Filename: if2_to_ets.cpp
 Funtions: if2_to_ets

*/

/*----------------------------------------------------------------------------
; INCLUDES
----------------------------------------------------------------------------*/
#include "frame_type_3gpp.h"
#include "if2_to_ets.h"
#include "typedef.h"
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
; LOCAL VARIABLE DEFINITIONS
; Variable declaration - defined here and used outside this module
----------------------------------------------------------------------------*/



/*
------------------------------------------------------------------------------
 FUNCTION NAME: if2_to_ets
------------------------------------------------------------------------------
 INPUT AND OUTPUT DEFINITIONS

 Inputs:
    frame_type_3gpp = decoder speech bit rate (enum Frame_Type_3GPP)
    if2_input_ptr   = pointer to input encoded speech bits in IF2 format (Word8)
    ets_output_ptr  = pointer to output encoded speech bits in ETS format (Word16)

 Outputs:
    ets_output_ptr  = pointer to encoded speech bits in the ETS format (Word16)

 Returns:
    None

 Global Variables Used:
    None

 Local Variables Needed:
    None

------------------------------------------------------------------------------
 FUNCTION DESCRIPTION

 This function performs a transformation on the data buffers. It converts the
 data format from IF2 to ETS. IF2 is the storage format where the frame type
 is in the first four bits of the first byte. The upper four bits of that byte
 contain the first four encoded speech bits for the frame. The following bytes
 contain the rest of the encoded speech bits. The final byte has padded zeros
 to make the frame byte aligned. ETS format has the encoded speech
 bits each separate with only one bit stored in each word.

------------------------------------------------------------------------------
 REQUIREMENTS

 None

------------------------------------------------------------------------------
 REFERENCES

AMR Speech Codec Frame Structure", 3GPP TS 26.101 version 4.1.0 Release 4, June 2001

------------------------------------------------------------------------------
 PSEUDO-CODE



------------------------------------------------------------------------------
 CAUTION [optional]
 [State any special notes, constraints or cautions for users of this function]

------------------------------------------------------------------------------
*/

void if2_to_ets(
    enum Frame_Type_3GPP frame_type_3gpp,
    UWord8   *if2_input_ptr,
    Word16   *ets_output_ptr,
    CommonAmrTbls* common_amr_tbls)
{

    Word16 i;
    Word16 j;
    Word16 x = 0;
    const Word16* numCompressedBytes_ptr = common_amr_tbls->numCompressedBytes_ptr;
    const Word16* numOfBits_ptr = common_amr_tbls->numOfBits_ptr;
    const Word16* const* reorderBits_ptr = common_amr_tbls->reorderBits_ptr;

    /*
     * The following section of code accesses bits in the IF2 method of
     * bit ordering. Each bit is given its own location in the buffer pointed
     * to by ets_output_ptr. The bits (for modes less than AMR_SID) are
     * reordered using the tables in bitreorder.c before the data is stored
     * into the buffer pointed to by ets_output_ptr.
     */

    if (frame_type_3gpp < AMR_SID)
    {
        for (j = 4; j < 8; j++)
        {
            ets_output_ptr[reorderBits_ptr[frame_type_3gpp][x++]] =
                (if2_input_ptr[0] >> j) & 0x01;
        }
        for (i = 1; i < numCompressedBytes_ptr[frame_type_3gpp]; i++)
        {
            for (j = 0; j < 8; j++)
            {
                if (x >= numOfBits_ptr[frame_type_3gpp])
                {
                    break;
                }
                ets_output_ptr[reorderBits_ptr[frame_type_3gpp][x++]] =
                    (if2_input_ptr[i] >> j) & 0x01;
            }
        }
    }
    else
    {
        for (j = 4; j < 8; j++)
        {
            ets_output_ptr[x++] =
                (if2_input_ptr[0] >> j) & 0x01;
        }
        for (i = 1; i < numCompressedBytes_ptr[frame_type_3gpp]; i++)
        {
            for (j = 0; j < 8; j++)
            {
                ets_output_ptr[x++] =
                    (if2_input_ptr[i] >> j) & 0x01;
            }
        }
    }

    return;
}
