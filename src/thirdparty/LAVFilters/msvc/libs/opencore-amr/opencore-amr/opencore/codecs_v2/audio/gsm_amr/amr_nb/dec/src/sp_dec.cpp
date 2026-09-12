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



 Filename: sp_dec.cpp
 Functions: GSMInitDecode
            Speech_Decode_Frame_reset
            GSMDecodeFrameExit
            GSMFrameDecode

------------------------------------------------------------------------------
 MODULE DESCRIPTION

 This file contains the functions that initialize, invoke, reset, and exit
 the GSM AMR decoder.

------------------------------------------------------------------------------
*/

/*----------------------------------------------------------------------------
; INCLUDES
----------------------------------------------------------------------------*/
#include "sp_dec.h"
#include "typedef.h"
#include "cnst.h"
#include "dec_amr.h"
#include "pstfilt.h"
#include "mode.h"
#include "post_pro.h"
#include "oscl_mem.h"
#include "bitno_tab.h"


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
 FUNCTION NAME: Bin2int
------------------------------------------------------------------------------
 INPUT AND OUTPUT DEFINITIONS

 Inputs:
    no_of_bits = number of bits associated with value
    bitstream = pointer to buffer where bits are read

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

  Function    : Bin2int
  Purpose     : Read "no_of_bits" bits from the array bitstream[]
                and convert to integer.

------------------------------------------------------------------------------
 REQUIREMENTS

 None

------------------------------------------------------------------------------
 REFERENCES

 bits2prm.c, UMTS GSM AMR speech codec, R99 - Version 3.2.0, March 2, 2001

------------------------------------------------------------------------------
 PSEUDO-CODE

static Word16 Bin2int ( // Reconstructed parameter
    Word16 no_of_bits,  // input : number of bits associated with value
    Word16 *bitstream   // output: address where bits are written
)
{
    Word16 value, i, bit;

    value = 0;
    for (i = 0; i < no_of_bits; i++)
    {
        value = shl (value, 1);
        bit = *bitstream++;
        if (sub (bit, BIT_1) == 0)
            value = add (value, 1);
    }
    return (value);
}

------------------------------------------------------------------------------
 CAUTION [optional]
 [State any special notes, constraints or cautions for users of this function]

------------------------------------------------------------------------------
*/

/*----------------------------------------------------------------------------
; FUNCTION CODE
----------------------------------------------------------------------------*/
static Word16 Bin2int(  /* Reconstructed parameter                      */
    Word16 no_of_bits,  /* input : number of bits associated with value */
    Word16 *bitstream   /* input: address where bits are read from      */
)
{
    Word16 value;
    Word16 i;
    Word16 single_bit;

    value = 0;
    for (i = 0; i < no_of_bits; i++)
    {
        value <<= 1;
        single_bit = *(bitstream++);
        value |= single_bit;
    }
    return (value);
}


/*
------------------------------------------------------------------------------
 FUNCTION NAME: bits2prm
------------------------------------------------------------------------------
 INPUT AND OUTPUT DEFINITIONS

 Inputs:
    mode = AMR mode of type enum Mode
    bits[] = pointer to serial bits of type Word16
    prm[] = pointer to analysis parameters of type Word16

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

  Function    : Bits2prm
  Purpose     : Retrieves the vector of encoder parameters from
                the received serial bits in a frame.

------------------------------------------------------------------------------
 REQUIREMENTS

 None

------------------------------------------------------------------------------
 REFERENCES

 bits2prm.c, UMTS GSM AMR speech codec, R99 - Version 3.2.0, March 2, 2001

------------------------------------------------------------------------------
 PSEUDO-CODE

void Bits2prm (
    enum Mode mode,     // i : AMR mode
    Word16 bits[],      // i : serial bits       (size <= MAX_SERIAL_SIZE)
    Word16 prm[]        // o : analysis parameters  (size <= MAX_PRM_SIZE)
)
{
    Word16 i;

    for (i = 0; i < prmno[mode]; i++)
    {
        prm[i] = Bin2int (bitno[mode][i], bits);
        bits += bitno[mode][i];
        add(0,0);       // account for above pointer update
    }

   return;
}

------------------------------------------------------------------------------
 CAUTION [optional]
 [State any special notes, constraints or cautions for users of this function]

------------------------------------------------------------------------------
*/

/*----------------------------------------------------------------------------
; FUNCTION CODE
----------------------------------------------------------------------------*/
void Bits2prm(
    enum Mode mode,     /* i : AMR mode                                    */
    Word16 bits[],      /* i : serial bits       (size <= MAX_SERIAL_SIZE) */
    Word16 prm[],        /* o : analysis parameters  (size <= MAX_PRM_SIZE) */
    CommonAmrTbls* common_amr_tbls /* i : ptr to strcut of table ptrs        */
)
{
    Word16 i;
    const Word16* prmno_ptr = common_amr_tbls->prmno_ptr;
    const Word16* const* bitno_ptr = common_amr_tbls->bitno_ptr;


    for (i = 0; i < prmno_ptr[mode]; i++)
    {
        prm[i] = Bin2int(bitno_ptr[mode][i], bits);
        bits += bitno_ptr[mode][i];
    }

    return;
}




/*
------------------------------------------------------------------------------
 FUNCTION NAME: GSMInitDecode
------------------------------------------------------------------------------
 INPUT AND OUTPUT DEFINITIONS

 Inputs:
    state = pointer to an array of pointers to structures of type
            Speech_Decode_FrameState
    no_hp_post_MR122 = flag to turn off high-pass post filter for 12.2 kbps
                       mode (Flag)
    id = pointer to an array whose contents are of type char

 Outputs:
    decoder_amrState field of the structure pointed to by the pointer pointed
       to by state is set to NULL
    post_state field of the structure pointed to by the pointer pointed to
      by state is set to NULL
    postHP_state field of the structure pointed to by the pointer pointed to
      by state is set to NULL
    no_hp_post_MR122 field of the structure pointed to by the pointer pointed
      to by state is set to the input no_hp_post_MR122

 Returns:
    return_value = set to zero, if initialization was successful; -1,
                   otherwise (int)

 Global Variables Used:
    None

 Local Variables Needed:
    None

------------------------------------------------------------------------------
 FUNCTION DESCRIPTION

 This function allocates memory for filter structure and initializes state
 memory used by the GSM AMR decoder.

------------------------------------------------------------------------------
 REQUIREMENTS

 None

------------------------------------------------------------------------------
 REFERENCES

 sp_dec.c, UMTS GSM AMR speech codec, R99 - Version 3.2.0, March 2, 2001

------------------------------------------------------------------------------
 PSEUDO-CODE

 Note: Original function name of Speech_Decode_Frame_init was changed to
       GSMInitDecode in the Code section.

int Speech_Decode_Frame_init (Speech_Decode_FrameState **state,
                              char *id)
{
  Speech_Decode_FrameState* s;

  if (state == (Speech_Decode_FrameState **) NULL){
      fprintf(stderr, "Speech_Decode_Frame_init: invalid parameter\n");
      return -1;
  }
  *state = NULL;

  // allocate memory
  if ((s= (Speech_Decode_FrameState *)
          malloc(sizeof(Speech_Decode_FrameState))) == NULL) {
      fprintf(stderr, "Speech_Decode_Frame_init: can not malloc state "
              "structure\n");
      return -1;
  }
  s->decoder_amrState = NULL;
  s->post_state = NULL;
  s->postHP_state = NULL;

  if (Decoder_amr_init(&s->decoder_amrState) ||
      Post_Filter_init(&s->post_state) ||
      Post_Process_init(&s->postHP_state) ) {
      Speech_Decode_Frame_exit(&s);
      return -1;
  }

  s->complexityCounter = getCounterId(id);

  Speech_Decode_Frame_reset(s);
  *state = s;

  return 0;
}

------------------------------------------------------------------------------
 CAUTION [optional]
 [State any special notes, constraints or cautions for users of this function]

------------------------------------------------------------------------------
*/

Word16 GSMInitDecode(void **state_data,
                     Word8 * id)
{
    Speech_Decode_FrameState* s;
    OSCL_UNUSED_ARG(id);

    if (state_data == NULL)
    {
        /*  fprintf(stderr, "Speech_Decode_Frame_init:
                             invalid parameter\n");  */
        return (-1);
    }
    *state_data = NULL;

    /* allocate memory */
    if ((s = (Speech_Decode_FrameState *)
             oscl_malloc(sizeof(Speech_Decode_FrameState))) == NULL)
    {
        /*  fprintf(stderr, "Speech_Decode_Frame_init: can not malloc state "
            "structure\n");  */
        return (-1);
    }

    if (Decoder_amr_init(&s->decoder_amrState)
            || Post_Process_reset(&s->postHP_state))
    {
        Speech_Decode_FrameState *tmp = s;
        /*
         *  dereferencing type-punned pointer avoid
         *  breaking strict-aliasing rules
         */
        void** tempVoid = (void**) tmp;
        GSMDecodeFrameExit(tempVoid);
        return (-1);
    }


    Speech_Decode_Frame_reset(s);
    *state_data = (void *)s;

    return (0);
}


/****************************************************************************/


/*
------------------------------------------------------------------------------
 FUNCTION NAME: Speech_Decode_Frame_reset
------------------------------------------------------------------------------
 INPUT AND OUTPUT DEFINITIONS

 Inputs:
    state = pointer to structures of type Speech_Decode_FrameState

 Outputs:
    None

 Returns:
    return_value = set to zero if reset was successful; -1, otherwise (int)

 Global Variables Used:
    None

 Local Variables Needed:
    None

------------------------------------------------------------------------------
 FUNCTION DESCRIPTION

 This function resets the state memory used by the GSM AMR decoder.

------------------------------------------------------------------------------
 REQUIREMENTS

 None

------------------------------------------------------------------------------
 REFERENCES

 sp_dec.c, UMTS GSM AMR speech codec, R99 - Version 3.2.0, March 2, 2001

------------------------------------------------------------------------------
 PSEUDO-CODE

int Speech_Decode_Frame_reset (Speech_Decode_FrameState *state)
{
  if (state == (Speech_Decode_FrameState *) NULL){
      fprintf(stderr, "Speech_Decode_Frame_reset: invalid parameter\n");
      return -1;
  }

  Decoder_amr_reset(state->decoder_amrState, (enum Mode)0);
  Post_Filter_reset(state->post_state);
  Post_Process_reset(state->postHP_state);

  state->prev_mode = (enum Mode)0;

  setCounter(state->complexityCounter);
  Init_WMOPS_counter();
  setCounter(0); // set counter to global counter

  return 0;
}

------------------------------------------------------------------------------
 CAUTION [optional]
 [State any special notes, constraints or cautions for users of this function]

------------------------------------------------------------------------------
*/
Word16 Speech_Decode_Frame_reset(void *state_data)
{

    Speech_Decode_FrameState *state =
        (Speech_Decode_FrameState *) state_data;

    if (state_data ==  NULL)
    {
        /*  fprintf(stderr, "Speech_Decode_Frame_reset:
                             invalid parameter\n");  */
        return (-1);
    }

    Decoder_amr_reset(&(state->decoder_amrState), MR475);
    Post_Filter_reset(&(state->post_state));
    Post_Process_reset(&(state->postHP_state));

    state->prev_mode = MR475;

    return (0);
}

/****************************************************************************/

/*
------------------------------------------------------------------------------
 FUNCTION NAME: GSMDecodeFrameExit
------------------------------------------------------------------------------
 INPUT AND OUTPUT DEFINITIONS

 Inputs:
    state = pointer to an array of pointers to structures of type
            Speech_Decode_FrameState

 Outputs:
    state contents is set to NULL

 Returns:
    None

 Global Variables Used:
    None

 Local Variables Needed:
    None

------------------------------------------------------------------------------
 FUNCTION DESCRIPTION

 This function frees up the memory used for the state memory of the GSM AMR
 decoder.

------------------------------------------------------------------------------
 REQUIREMENTS

 None

------------------------------------------------------------------------------
 REFERENCES

 sp_dec.c, UMTS GSM AMR speech codec, R99 - Version 3.2.0, March 2, 2001

------------------------------------------------------------------------------
 PSEUDO-CODE

 Note: The original function name of Speech_Decode_Frame_exit was changed to
       GSMDecodeFrameExit in the Code section.

void Speech_Decode_Frame_exit (Speech_Decode_FrameState **state)
{
  if (state == NULL || *state == NULL)
      return;

  Decoder_amr_exit(&(*state)->decoder_amrState);
  Post_Filter_exit(&(*state)->post_state);
  Post_Process_exit(&(*state)->postHP_state);

  setCounter((*state)->complexityCounter);
  WMOPS_output(0);
  setCounter(0); // set counter to global counter

  // deallocate memory
  free(*state);
  *state = NULL;

  return;
}

------------------------------------------------------------------------------
 CAUTION [optional]
 [State any special notes, constraints or cautions for users of this function]

------------------------------------------------------------------------------
*/

void GSMDecodeFrameExit(void **state_data)
{

    Speech_Decode_FrameState **state =
        (Speech_Decode_FrameState **) state_data;

    if (state == NULL || *state == NULL)
    {
        return;
    }

    /* deallocate memory */
    oscl_free(*state);
    *state = NULL;

    return;
}

/****************************************************************************/

/*
------------------------------------------------------------------------------
 FUNCTION NAME: GSMFrameDecode
------------------------------------------------------------------------------
 INPUT AND OUTPUT DEFINITIONS

 Inputs:
    st = pointer to structures of type Speech_Decode_FrameState
    mode = GSM AMR codec mode (enum Mode)
    serial = pointer to the serial bit stream buffer (unsigned char)
    frame_type = GSM AMR receive frame type (enum RXFrameType)
    synth = pointer to the output synthesis speech buffer (Word16)

 Outputs:
    synth contents are truncated to 13 bits if NO13BIT is not defined,
      otherwise, its contents are left at 16 bits

 Returns:
    return_value = set to zero (int)

 Global Variables Used:
    None

 Local Variables Needed:
    None

------------------------------------------------------------------------------
 FUNCTION DESCRIPTION

 This function is the entry point to the GSM AMR decoder. The following
 operations are performed on one received frame: First, the codec
 parameters are parsed from the buffer pointed to by serial according to
 frame_type. Then the AMR decoder is invoked via a call to Decoder_amr. Post
 filtering of the decoded data is done via a call to Post_Filter function.
 Lastly, the decoded data is post-processed via a call to Post_Process
 function. If NO13BIT is not defined, the contents of the buffer pointed to
 by synth is truncated to 13 bits. It remains unchanged otherwise.

------------------------------------------------------------------------------
 REQUIREMENTS

 None

------------------------------------------------------------------------------
 REFERENCES

 sp_dec.c, UMTS GSM AMR speech codec, R99 - Version 3.2.0, March 2, 2001

------------------------------------------------------------------------------
 PSEUDO-CODE

 Note: The original function name of Speech_Decode_Frame_exit was changed to
       GSMFrameDecode in the Code section.

int Speech_Decode_Frame (
    Speech_Decode_FrameState *st, // io: post filter states
    enum Mode mode,               // i : AMR mode
    Word16 *serial,               // i : serial bit stream
    enum RXFrameType frame_type,  // i : Frame type
    Word16 *synth                 // o : synthesis speech (postfiltered
                                  //     output)
)
{
  Word16 parm[MAX_PRM_SIZE + 1];  // Synthesis parameters
  Word16 Az_dec[AZ_SIZE];         // Decoded Az for post-filter
                                  // in 4 subframes

#if !defined(NO13BIT)
  Word16 i;
#endif

  setCounter(st->complexityCounter);
  Reset_WMOPS_counter ();          // reset WMOPS counter for the new frame

  // Serial to parameters
  if ((frame_type == RX_SID_BAD) ||
      (frame_type == RX_SID_UPDATE)) {
    // Override mode to MRDTX
    Bits2prm (MRDTX, serial, parm);
  } else {
    Bits2prm (mode, serial, parm);
  }

  // Synthesis
  Decoder_amr(st->decoder_amrState, mode, parm, frame_type,
              synth, Az_dec);

  Post_Filter(st->post_state, mode, synth, Az_dec);   // Post-filter

  // post HP filter, and 15->16 bits
  Post_Process(st->postHP_state, synth, L_FRAME);

#if !defined(NO13BIT)
  // Truncate to 13 bits
  for (i = 0; i < L_FRAME; i++)
  {
     synth[i] = synth[i] & 0xfff8;
  }
#endif

  setCounter(0); // set counter to global counter

  return 0;
}


------------------------------------------------------------------------------
 CAUTION [optional]
 [State any special notes, constraints or cautions for users of this function]

------------------------------------------------------------------------------
*/

void GSMFrameDecode(
    Speech_Decode_FrameState *st, /* io: post filter states                */
    enum Mode mode,               /* i : AMR mode                          */
    Word16 *serial,               /* i : serial bit stream                 */
    enum RXFrameType frame_type,  /* i : Frame type                        */
    Word16 *synth)                /* o : synthesis speech (postfiltered    */
/*     output)                           */

{
    Word16 parm[MAX_PRM_SIZE + 1];  /* Synthesis parameters                */
    Word16 Az_dec[AZ_SIZE];         /* Decoded Az for post-filter          */
    /* in 4 subframes                      */
    Flag *pOverflow = &(st->decoder_amrState.overflow);  /* Overflow flag  */

#if !defined(NO13BIT)
    Word16 i;
#endif

    /* Serial to parameters   */
    if ((frame_type == RX_SID_BAD) ||
            (frame_type == RX_SID_UPDATE))
    {
        /* Override mode to MRDTX */
        Bits2prm(MRDTX, serial, parm, &st->decoder_amrState.common_amr_tbls);
    }
    else
    {
        Bits2prm(mode, serial, parm, &st->decoder_amrState.common_amr_tbls);
    }

    /* Synthesis */
    Decoder_amr(
        &(st->decoder_amrState),
        mode,
        parm,
        frame_type,
        synth,
        Az_dec);

    /* Post-filter */
    Post_Filter(
        &(st->post_state),
        mode,
        synth,
        Az_dec,
        pOverflow);

    /* post HP filter, and 15->16 bits */
    Post_Process(
        &(st->postHP_state),
        synth,
        L_FRAME,
        pOverflow);

#if !defined(NO13BIT)
    /* Truncate to 13 bits */
    for (i = 0; i < L_FRAME; i++)
    {
        synth[i] = synth[i] & 0xfff8;
    }
#endif

    return;
}



