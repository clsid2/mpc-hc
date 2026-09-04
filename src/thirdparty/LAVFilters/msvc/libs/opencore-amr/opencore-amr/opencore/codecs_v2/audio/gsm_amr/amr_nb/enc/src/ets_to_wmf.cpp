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



 Filename: ets_to_wmf.cpp
 Functions: ets_to_wmf

------------------------------------------------------------------------------
*/

/*----------------------------------------------------------------------------
; INCLUDES
----------------------------------------------------------------------------*/
#include "ets_to_wmf.h"
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
 FUNCTION NAME: ets_to_wmf
------------------------------------------------------------------------------
 INPUT AND OUTPUT DEFINITIONS

 Inputs:
    frame_type_3gpp = decoder speech bit rate (enum Frame_Type_3GPP)
    ets_input_ptr   = pointer to input encoded speech bits in ETS format (Word16)
    wmf_output_ptr  = pointer to output encoded speech bits in WMF format(UWord8)

 Outputs:
    wmf_output_ptr  = pointer to encoded speech bits in the WMF format (UWord8)

 Returns:
    None

 Global Variables Used:
    numOfBits = table of values that describe the number of bits per frame for
                each 3GPP frame type mode. The table is type const Word16 and has
                NUM_MODES elements. This table is located in bitreorder_tab.c.
    reorderBits = table of pointers that point to tables used to reorder the
                  encoded speech bits when converting from ETS to WMF or IF2
                  format. The table is of type const Word16 * and contains
                  NUM_MODES-1 elements. This table is located in bitreorder_tab.c.

 Local Variables Needed:
    None

------------------------------------------------------------------------------
 FUNCTION DESCRIPTION

 This function performs a transformation on the data buffers. It converts the
 data format from ETS (European Telecommunication Standard) to WMF (wireless
 multimedia forum). ETS format has the encoded speech bits each separate with
 only one bit stored in each word. WMF is the storage format where the frame
 type is in the first four bits of the first byte. This first byte has the
 upper four bits as padded zeroes. The following bytes contain the rest of the
 encoded speech bits. The final byte has padded zeros to make the frame byte
 aligned.
------------------------------------------------------------------------------
 REQUIREMENTS

 None

------------------------------------------------------------------------------
 REFERENCES

 None

------------------------------------------------------------------------------
 PSEUDO-CODE



------------------------------------------------------------------------------
 CAUTION [optional]
 [State any special notes, constraints or cautions for users of this function]

------------------------------------------------------------------------------
*/

void ets_to_wmf(
    enum Frame_Type_3GPP frame_type_3gpp,
    Word16 *ets_input_ptr,
    UWord8 *wmf_output_ptr,
    CommonAmrTbls* common_amr_tbls)
{
    Word16  i;
    Word16  k = 0;
    Word16  j = 0;
    Word16 *ptr_temp;
    Word16  bits_left;
    UWord8  accum;
    const Word16* const* reorderBits_ptr = common_amr_tbls->reorderBits_ptr;
    const Word16* numOfBits_ptr = common_amr_tbls->numOfBits_ptr;

    wmf_output_ptr[j++] = (UWord8)(frame_type_3gpp) & 0x0f;

    if (frame_type_3gpp < AMR_SID)
    {

        for (i = 0; i < numOfBits_ptr[frame_type_3gpp] - 7;)
        {
            wmf_output_ptr[j]  =
                (UWord8) ets_input_ptr[reorderBits_ptr[frame_type_3gpp][i++]] << 7;
            wmf_output_ptr[j] |=
                (UWord8) ets_input_ptr[reorderBits_ptr[frame_type_3gpp][i++]] << 6;
            wmf_output_ptr[j] |=
                (UWord8) ets_input_ptr[reorderBits_ptr[frame_type_3gpp][i++]] << 5;
            wmf_output_ptr[j] |=
                (UWord8) ets_input_ptr[reorderBits_ptr[frame_type_3gpp][i++]] << 4;
            wmf_output_ptr[j] |=
                (UWord8) ets_input_ptr[reorderBits_ptr[frame_type_3gpp][i++]] << 3;
            wmf_output_ptr[j] |=
                (UWord8) ets_input_ptr[reorderBits_ptr[frame_type_3gpp][i++]] << 2;
            wmf_output_ptr[j] |=
                (UWord8) ets_input_ptr[reorderBits_ptr[frame_type_3gpp][i++]] << 1;
            wmf_output_ptr[j++] |=
                (UWord8) ets_input_ptr[reorderBits_ptr[frame_type_3gpp][i++]];
        }

        bits_left = numOfBits_ptr[frame_type_3gpp] -
                    (numOfBits_ptr[frame_type_3gpp] & 0xFFF8);

        wmf_output_ptr[j] = 0;

        for (k = 0; k < bits_left; k++)
        {
            wmf_output_ptr[j] |=
                (UWord8) ets_input_ptr[reorderBits_ptr[frame_type_3gpp][i++]] << (7 - k);

        }
    }
    else
    {

        ptr_temp = &ets_input_ptr[0];

        for (i = numOfBits_ptr[frame_type_3gpp] - 7; i > 0; i -= 8)
        {
            accum  = (UWord8) * (ptr_temp++) << 7;
            accum |= (UWord8) * (ptr_temp++) << 6;
            accum |= (UWord8) * (ptr_temp++) << 5;
            accum |= (UWord8) * (ptr_temp++) << 4;
            accum |= (UWord8) * (ptr_temp++) << 3;
            accum |= (UWord8) * (ptr_temp++) << 2;
            accum |= (UWord8) * (ptr_temp++) << 1;
            accum |= (UWord8) * (ptr_temp++);

            wmf_output_ptr[j++] = accum;
        }

        bits_left = numOfBits_ptr[frame_type_3gpp] -
                    (numOfBits_ptr[frame_type_3gpp] & 0xFFF8);

        wmf_output_ptr[j] = 0;

        for (i = 0; i < bits_left; i++)
        {
            wmf_output_ptr[j] |= *(ptr_temp++) << (7 - i);
        }
    }

    return;
}



void ets_to_ietf(
    enum Frame_Type_3GPP frame_type_3gpp,
    Word16 *ets_input_ptr,
    UWord8 *ietf_output_ptr,
    CommonAmrTbls* common_amr_tbls)
{
    Word16  i;
    Word16  k = 0;
    Word16  j = 0;
    Word16 *ptr_temp;
    Word16  bits_left;
    UWord8  accum;
    const Word16* const* reorderBits_ptr = common_amr_tbls->reorderBits_ptr;
    const Word16* numOfBits_ptr = common_amr_tbls->numOfBits_ptr;

    ietf_output_ptr[j++] = (UWord8)(frame_type_3gpp << 3);

    if (frame_type_3gpp < AMR_SID)
    {
        for (i = 0; i < numOfBits_ptr[frame_type_3gpp] - 7;)
        {
            ietf_output_ptr[j]  =
                (UWord8) ets_input_ptr[reorderBits_ptr[frame_type_3gpp][i++]] << 7;
            ietf_output_ptr[j] |=
                (UWord8) ets_input_ptr[reorderBits_ptr[frame_type_3gpp][i++]] << 6;
            ietf_output_ptr[j] |=
                (UWord8) ets_input_ptr[reorderBits_ptr[frame_type_3gpp][i++]] << 5;
            ietf_output_ptr[j] |=
                (UWord8) ets_input_ptr[reorderBits_ptr[frame_type_3gpp][i++]] << 4;
            ietf_output_ptr[j] |=
                (UWord8) ets_input_ptr[reorderBits_ptr[frame_type_3gpp][i++]] << 3;
            ietf_output_ptr[j] |=
                (UWord8) ets_input_ptr[reorderBits_ptr[frame_type_3gpp][i++]] << 2;
            ietf_output_ptr[j] |=
                (UWord8) ets_input_ptr[reorderBits_ptr[frame_type_3gpp][i++]] << 1;
            ietf_output_ptr[j++] |=
                (UWord8) ets_input_ptr[reorderBits_ptr[frame_type_3gpp][i++]];
        }

        bits_left = numOfBits_ptr[frame_type_3gpp] -
                    (numOfBits_ptr[frame_type_3gpp] & 0xFFF8);

        ietf_output_ptr[j] = 0;

        for (k = 0; k < bits_left; k++)
        {
            ietf_output_ptr[j] |=
                (UWord8) ets_input_ptr[reorderBits_ptr[frame_type_3gpp][i++]] << (7 - k);

        }
    }
    else
    {

        ptr_temp = &ets_input_ptr[0];

        for (i = numOfBits_ptr[frame_type_3gpp] - 7; i > 0; i -= 8)
        {
            accum  = (UWord8) * (ptr_temp++) << 7;
            accum |= (UWord8) * (ptr_temp++) << 6;
            accum |= (UWord8) * (ptr_temp++) << 5;
            accum |= (UWord8) * (ptr_temp++) << 4;
            accum |= (UWord8) * (ptr_temp++) << 3;
            accum |= (UWord8) * (ptr_temp++) << 2;
            accum |= (UWord8) * (ptr_temp++) << 1;
            accum |= (UWord8) * (ptr_temp++);

            ietf_output_ptr[j++] = accum;
        }

        bits_left = numOfBits_ptr[frame_type_3gpp] -
                    (numOfBits_ptr[frame_type_3gpp] & 0xFFF8);

        ietf_output_ptr[j] = 0;

        for (i = 0; i < bits_left; i++)
        {
            ietf_output_ptr[j] |= *(ptr_temp++) << (7 - i);
        }
    }

    return;
}
