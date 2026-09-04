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

 Filename: ets_to_if2.cpp
 Funtions: ets_to_if2

*/

/*----------------------------------------------------------------------------
; INCLUDES
----------------------------------------------------------------------------*/
#include "frame_type_3gpp.h"
#include "ets_to_if2.h"
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
 FUNCTION NAME: ets_to_if2
------------------------------------------------------------------------------
 INPUT AND OUTPUT DEFINITIONS

 Inputs:
    frame_type_3gpp = decoder speech bit rate (enum Frame_Type_3GPP)
    ets_input_ptr   = pointer to input encoded speech bits in ETS format (Word16)
    if2_output_ptr  = pointer to output encoded speech bits in IF2 format (UWord8)

 Outputs:
    if2_output_ptr  = pointer to encoded speech bits in the IF2 format (UWord8)

 Returns:
    None

 Global Variables Used:
    None

 Local Variables Needed:
    None

------------------------------------------------------------------------------
 FUNCTION DESCRIPTION

 This function performs a transformation on the data buffers. It converts the
 data format from ETS (European Telecommunication Standard) to IF2. ETS format
 has the encoded speech bits each separate with only one bit stored in each
 word. IF2 is the storage format where the frame type is in the first four bits
 of the first byte. The upper four bits of that byte contain the first four
 encoded speech bits for the frame. The following bytes contain the rest of
 the encoded speech bits. The final byte has padded zeros  to make the frame
 byte aligned.
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

void ets_to_if2(
    enum Frame_Type_3GPP frame_type_3gpp,
    Word16 *ets_input_ptr,
    UWord8 *if2_output_ptr,
    CommonAmrTbls* common_amr_tbls)
{
    Word16  i;
    Word16  k;
    Word16  j = 0;
    Word16 *ptr_temp;
    Word16  bits_left;
    UWord8  accum;
    const Word16* const* reorderBits_ptr = common_amr_tbls->reorderBits_ptr;
    const Word16* numOfBits_ptr = common_amr_tbls->numOfBits_ptr;

    if (frame_type_3gpp < AMR_SID)
    {
        if2_output_ptr[j++] = (UWord8)(frame_type_3gpp) |
                              (ets_input_ptr[reorderBits_ptr[frame_type_3gpp][0]] << 4) |
                              (ets_input_ptr[reorderBits_ptr[frame_type_3gpp][1]] << 5) |
                              (ets_input_ptr[reorderBits_ptr[frame_type_3gpp][2]] << 6) |
                              (ets_input_ptr[reorderBits_ptr[frame_type_3gpp][3]] << 7);

        for (i = 4; i < numOfBits_ptr[frame_type_3gpp] - 7;)
        {
            if2_output_ptr[j]  =
                (UWord8) ets_input_ptr[reorderBits_ptr[frame_type_3gpp][i++]];
            if2_output_ptr[j] |=
                (UWord8) ets_input_ptr[reorderBits_ptr[frame_type_3gpp][i++]] << 1;
            if2_output_ptr[j] |=
                (UWord8) ets_input_ptr[reorderBits_ptr[frame_type_3gpp][i++]] << 2;
            if2_output_ptr[j] |=
                (UWord8) ets_input_ptr[reorderBits_ptr[frame_type_3gpp][i++]] << 3;
            if2_output_ptr[j] |=
                (UWord8) ets_input_ptr[reorderBits_ptr[frame_type_3gpp][i++]] << 4;
            if2_output_ptr[j] |=
                (UWord8) ets_input_ptr[reorderBits_ptr[frame_type_3gpp][i++]] << 5;
            if2_output_ptr[j] |=
                (UWord8) ets_input_ptr[reorderBits_ptr[frame_type_3gpp][i++]] << 6;
            if2_output_ptr[j++] |=
                (UWord8) ets_input_ptr[reorderBits_ptr[frame_type_3gpp][i++]] << 7;
        }

        bits_left = 4 + numOfBits_ptr[frame_type_3gpp] -
                    ((4 + numOfBits_ptr[frame_type_3gpp]) & 0xFFF8);

        if (bits_left != 0)
        {
            if2_output_ptr[j] = 0;

            for (k = 0; k < bits_left; k++)
            {
                if2_output_ptr[j] |=
                    (UWord8) ets_input_ptr[reorderBits_ptr[frame_type_3gpp][i++]] << k;
            }
        }
    }
    else
    {
        if (frame_type_3gpp != AMR_NO_DATA)
        {
            /* First octet contains 3GPP frame type and */
            /* first 4 bits of encoded parameters       */
            if2_output_ptr[j++] = (UWord8)(frame_type_3gpp) |
                                  (ets_input_ptr[0] << 4) | (ets_input_ptr[1] << 5) |
                                  (ets_input_ptr[2] << 6) | (ets_input_ptr[3] << 7);
            ptr_temp = &ets_input_ptr[4];

            bits_left = ((4 + numOfBits_ptr[frame_type_3gpp]) & 0xFFF8);

            for (i = (bits_left - 7) >> 3; i > 0; i--)
            {
                accum  = (UWord8) * (ptr_temp++);
                accum |= (UWord8) * (ptr_temp++) << 1;
                accum |= (UWord8) * (ptr_temp++) << 2;
                accum |= (UWord8) * (ptr_temp++) << 3;
                accum |= (UWord8) * (ptr_temp++) << 4;
                accum |= (UWord8) * (ptr_temp++) << 5;
                accum |= (UWord8) * (ptr_temp++) << 6;
                accum |= (UWord8) * (ptr_temp++) << 7;

                if2_output_ptr[j++] = accum;
            }

            bits_left = 4 + numOfBits_ptr[frame_type_3gpp] - bits_left;

            if (bits_left != 0)
            {
                if2_output_ptr[j] = 0;

                for (i = 0; i < bits_left; i++)
                {
                    if2_output_ptr[j] |= (ptr_temp[i] << i);
                }
            }
        }
        else
        {
            /* When there is no data, LSnibble of first octet */
            /* is the 3GPP frame type, MSnibble is zeroed out */
            if2_output_ptr[j++] = (UWord8)(frame_type_3gpp);
        }

    }

    return;
}
