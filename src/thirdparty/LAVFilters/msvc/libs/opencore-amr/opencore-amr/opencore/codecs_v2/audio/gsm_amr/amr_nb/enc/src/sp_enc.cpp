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



 Filename: sp_enc.cpp
 Funtions: GSMInitEncode
           Speech_Encode_Frame_reset
           GSMEncodeFrameExit
           Speech_Encode_Frame_First
           GSMEncodeFrame

------------------------------------------------------------------------------
 MODULE DESCRIPTION

 These functions comprise the pre filtering and encoding of one speech frame.

------------------------------------------------------------------------------
*/


/*----------------------------------------------------------------------------
; INCLUDES
----------------------------------------------------------------------------*/
#include "sp_enc.h"
#include "typedef.h"
#include "cnst.h"
#include "set_zero.h"
#include "pre_proc.h"
#include "prm2bits.h"
#include "mode.h"
#include "cod_amr.h"
#include "oscl_mem.h"

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
 FUNCTION NAME: GSMInitEncode
------------------------------------------------------------------------------
 INPUT AND OUTPUT DEFINITIONS
 Inputs:
    state = pointer to an array of pointers to structures of type
            Speech_Decode_FrameState
    dtx = flag to turn off or turn on DTX (Flag)
    id = pointer to an array whose contents are of type char

 Outputs:
    pre_state field of the structure pointed to by the pointer pointed to
      by state is set to NULL
    cod_amr_state field of the structure pointed to by the pointer pointed to
      by state is set to NULL
    dtx field of the structure pointed to by the pointer pointed to by state
      is set to the input dtx

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
 memory

------------------------------------------------------------------------------
 REQUIREMENTS

 None.

------------------------------------------------------------------------------
 REFERENCES

 sp_enc.c, UMTS GSM AMR speech codec, R99 - Version 3.2.0, March 2, 2001

------------------------------------------------------------------------------
 PSEUDO-CODE
 Note: Original function name of Speech_Encode_Frame_init was changed to
       GSMInitEncode in the Code section.

int Speech_Encode_Frame_init (void **state_data,
                   Flag   dtx,
                   char  *id)
{
  Speech_Encode_FrameState* s;

  if (state_data == NULL){
      fprintf(stderr, "Speech_Encode_Frame_init: invalid parameter\n");
      return -1;
  }
  *state_data = NULL;

  // allocate memory
  if ((s= (Speech_Encode_FrameState *) malloc(sizeof(Speech_Encode_FrameState))) == NULL){
      fprintf(stderr, "Speech_Encode_Frame_init: can not malloc state "
                      "structure\n");
      return -1;
  }

  s->complexityCounter = getCounterId(id);

  s->pre_state = NULL;
  s->cod_amr_state = NULL;
  s->dtx = dtx;

  if (Pre_Process_init(&s->pre_state) ||
      cod_amr_init(&s->cod_amr_state, s->dtx)) {
      GSMEncodeFrameExit(&s);
      return -1;
  }

  Speech_Encode_Frame_reset(s);
  *state_data = (void *)s;

  return 0;
}


------------------------------------------------------------------------------
 CAUTION [optional]
 [State any special notes, constraints or cautions for users of this function]

------------------------------------------------------------------------------
*/

Word16 GSMInitEncode(void **state_data,
                     Flag   dtx,
                     Word8  *id)
{
    Speech_Encode_FrameState* s;

    OSCL_UNUSED_ARG(id);

    if (state_data == NULL)
    {
        /*  fprintf(stderr, "Speech_Encode_Frame_init: invalid parameter\n");  */
        return -1;
    }
    *state_data = NULL;

    /* allocate memory */
    if ((s = (Speech_Encode_FrameState *) oscl_malloc(sizeof(Speech_Encode_FrameState))) == NULL)
    {
        /*  fprintf(stderr, "Speech_Encode_Frame_init: can not malloc state "
                        "structure\n");  */
        return -1;
    }

    s->pre_state = NULL;
    s->cod_amr_state = NULL;
    s->dtx = dtx;

    if (Pre_Process_init(&s->pre_state) ||
            cod_amr_init(&s->cod_amr_state, s->dtx))
    {
        Speech_Encode_FrameState** temp = &s;
        GSMEncodeFrameExit((void**)temp);
        return -1;
    }

    Speech_Encode_Frame_reset(s);
    *state_data = (void *)s;

    return 0;
}


/*
------------------------------------------------------------------------------
 FUNCTION NAME: Speech_Encode_Frame_reset
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

 This function resets state memory

------------------------------------------------------------------------------
 REQUIREMENTS

 None.

------------------------------------------------------------------------------
 REFERENCES

 sp_enc.c, UMTS GSM AMR speech codec, R99 - Version 3.2.0, March 2, 2001

------------------------------------------------------------------------------
 PSEUDO-CODE

int Speech_Encode_Frame_reset (void *state_data)
{

  Speech_Encode_FrameState *state =
     (Speech_Encode_FrameState *) state_data;

  if (state_data == NULL){
        fprintf(stderr, "Speech_Encode_Frame_reset
                           : invalid parameter\n");
      return -1;
  }

  Pre_Process_reset(state->pre_state);
  cod_amr_reset(state->cod_amr_state);

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

Word16 Speech_Encode_Frame_reset(void *state_data)
{

    Speech_Encode_FrameState *state =
        (Speech_Encode_FrameState *) state_data;

    if (state_data == NULL)
    {
        /*  fprintf(stderr, "Speech_Encode_Frame_reset
                             : invalid parameter\n");  */
        return -1;
    }

    Pre_Process_reset(state->pre_state);
    cod_amr_reset(state->cod_amr_state);

    return 0;
}

/****************************************************************************/

/*
------------------------------------------------------------------------------
 FUNCTION NAME: GSMEncodeFrameExit
------------------------------------------------------------------------------
 INPUT AND OUTPUT DEFINITIONS

 Inputs:
    state = pointer to a pointer to a structure of type cod_amrState

 Outputs:
    state points to a NULL address

 Returns:
    None.

 Global Variables Used:
    None.

 Local Variables Needed:
    None.

------------------------------------------------------------------------------
 FUNCTION DESCRIPTION

 This function frees the memory used for state memory.

------------------------------------------------------------------------------
 REQUIREMENTS

 None.

------------------------------------------------------------------------------
 REFERENCES

 sp_enc.c, UMTS GSM AMR speech codec, R99 - Version 3.2.0, March 2, 2001

------------------------------------------------------------------------------
 PSEUDO-CODE

 Note: Original function name of Speech_Encode_Frame_exit was changed to
       GSMEncodeFrameExit in the Code section.

void Speech_Encode_Frame_exit (void **state_data)
{

    Speech_Encode_FrameState **state =
        (Speech_Encode_FrameState **) state_data;

  if (state == NULL || *state == NULL)
      return;

  Pre_Process_exit(&(*state)->pre_state);
  cod_amr_exit(&(*state)->cod_amr_state);

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

void GSMEncodeFrameExit(void **state_data)
{

    Speech_Encode_FrameState **state =
        (Speech_Encode_FrameState **) state_data;

    if (state == NULL || *state == NULL)
        return;

    Pre_Process_exit(&(*state)->pre_state);
    cod_amr_exit(&(*state)->cod_amr_state);

    /* deallocate memory */
    oscl_free(*state);
    *state = NULL;

    return;
}

/****************************************************************************/

/*
------------------------------------------------------------------------------
 FUNCTION NAME: Speech_Encode_Frame_First
------------------------------------------------------------------------------

 INPUT AND OUTPUT DEFINITIONS

 Inputs:
    st = pointer to a structure of type Speech_Encode_FrameState that contains
            the post filter states
    new_speech = pointer to buffer of length L_FRAME that contains
                 the speech input (Word16)

 Outputs:
    The structure of type Speech_Encode_FrameState pointed to by st is updated.

 Returns:
    return_value = 0 (int)

 Global Variables Used:
    None.

 Local Variables Needed:
    None.

------------------------------------------------------------------------------
 FUNCTION DESCRIPTION

 This function encodes the first frame of speech. It calls the pre-processing
 filter and the first frame encoder.

------------------------------------------------------------------------------
 REQUIREMENTS

 None.

------------------------------------------------------------------------------
 REFERENCES

 sp_enc.c, UMTS GSM AMR speech codec, R99 - Version 3.2.0, March 2, 2001

------------------------------------------------------------------------------
 PSEUDO-CODE

int Speech_Encode_Frame_First (
    Speech_Encode_FrameState *st,  // i/o : post filter states
    Word16 *new_speech)            // i   : speech input
{
#if !defined(NO13BIT)
   Word16 i;
#endif

   setCounter(st->complexityCounter);

#if !defined(NO13BIT)
  // Delete the 3 LSBs (13-bit input)
  for (i = 0; i < L_NEXT; i++)
  {
     new_speech[i] = new_speech[i] & 0xfff8;
  }
#endif

  // filter + downscaling
  Pre_Process (st->pre_state, new_speech, L_NEXT);

  cod_amr_first(st->cod_amr_state, new_speech);

  Init_WMOPS_counter (); // reset WMOPS counter for the new frame

  return 0;
}


------------------------------------------------------------------------------
 CAUTION [optional]
 [State any special notes, constraints or cautions for users of this function]

------------------------------------------------------------------------------
*/

void Speech_Encode_Frame_First(
    Speech_Encode_FrameState *st,  /* i/o : post filter states       */
    Word16 *new_speech)            /* i   : speech input             */
{
#if !defined(NO13BIT)
    Word16 i;
#endif

#if !defined(NO13BIT)
    /* Delete the 3 LSBs (13-bit input) */
    for (i = 0; i < L_NEXT; i++)
    {
        new_speech[i] = new_speech[i] & 0xfff8;
    }
#endif

    /* filter + downscaling */
    Pre_Process(st->pre_state, new_speech, L_NEXT);

    cod_amr_first(st->cod_amr_state, new_speech);

    return;
}

/*
------------------------------------------------------------------------------
 FUNCTION NAME: cod_amr
------------------------------------------------------------------------------
 INPUT AND OUTPUT DEFINITIONS

 Inputs:
    state_data = a void pointer to the post filter states
    mode = AMR mode of type enum Mode
    new_speech = pointer to buffer of length L_FRAME that contains
             the speech input of type Word16
    serial = pointer to the serial bit stream of type Word16
    usedMode = pointer to the used mode of type enum Mode

 Outputs:
    serial -> encoded serial bit stream
    The value pointed to by usedMode is updated.

 Returns:
    return_value = 0 (int)

 Global Variables Used:
    None.

 Local Variables Needed:
    None.

------------------------------------------------------------------------------
 FUNCTION DESCRIPTION

 This function is the entry point to the GSM AMR encoder. The following
 operations are performed to generate one encoded frame: First, the incoming
 audio samples are passed through the pre-processing filter where they are
 filtered and downscaled. A call is then made to the main encoder cod_amr().
 This generates the set of encoded parameters which include the LSP, adaptive
 codebook, and fixed codebook quantization indices (addresses and gains). The
 generated parameters are then converted to serial bits.

------------------------------------------------------------------------------
 REQUIREMENTS

 None.

------------------------------------------------------------------------------
 REFERENCES

 sp_enc.c, UMTS GSM AMR speech codec, R99 - Version 3.2.0, March 2, 2001

------------------------------------------------------------------------------
 PSEUDO-CODE
 Note: Original function name of Speech_Encode_Frame was changed to
       GSMEncodeFrame in the Code section.

int Speech_Encode_Frame (
    void *state_data,             // i/o : post filter states
    enum Mode mode,               // i   : speech coder mode
    Word16 *new_speech,           // i   : speech input
    Word16 *serial,               // o   : serial bit stream
    enum Mode *usedMode           // o   : used speech coder mode
    )
{

  Speech_Encode_FrameState *st =
     (Speech_Encode_FrameState *) state_data;

  Word16 prm[MAX_PRM_SIZE];   // Analysis parameters
  Word16 syn[L_FRAME];        // Buffer for synthesis speech
  Word16 i;

  setCounter(st->complexityCounter);
  Reset_WMOPS_counter (); // reset WMOPS counter for the new frame
  // initialize the serial output frame to zero
  for (i = 0; i < MAX_SERIAL_SIZE; i++)
  {
    serial[i] = 0;
  }
#if !defined(NO13BIT)
  // Delete the 3 LSBs (13-bit input)
  for (i = 0; i < L_FRAME; i++)
  {
     new_speech[i] = new_speech[i] & 0xfff8;


  }
#endif

  // filter + downscaling
  Pre_Process (st->pre_state, new_speech, L_FRAME);

  // Call the speech encoder
  cod_amr(st->cod_amr_state, mode, new_speech, prm, usedMode, syn);

  // Parameters to serial bits
  Prm2bits (*usedMode, prm, &serial[0]);

  fwc();
  setCounter(0); // set counter to global counter

  return 0;
}

------------------------------------------------------------------------------
 CAUTION [optional]
 [State any special notes, constraints or cautions for users of this function]

------------------------------------------------------------------------------
*/

void GSMEncodeFrame(
    void *state_data,             /* i/o : post filter states          */
    enum Mode mode,               /* i   : speech coder mode           */
    Word16 *new_speech,           /* i   : speech input                */
    Word16 *serial,               /* o   : serial bit stream           */
    enum Mode *usedMode           /* o   : used speech coder mode      */
)
{

    Speech_Encode_FrameState *st =
        (Speech_Encode_FrameState *) state_data;

    Word16 prm[MAX_PRM_SIZE];   /* Analysis parameters.                 */
    Word16 syn[L_FRAME];        /* Buffer for synthesis speech          */
    Word16 i;

    /* initialize the serial output frame to zero */
    for (i = 0; i < MAX_SERIAL_SIZE; i++)
    {
        serial[i] = 0;
    }
#if !defined(NO13BIT)
    /* Delete the 3 LSBs (13-bit input) */
    for (i = 0; i < L_FRAME; i++)
    {
        new_speech[i] = new_speech[i] & 0xfff8;
    }
#endif

    /* filter + downscaling */
    Pre_Process(st->pre_state, new_speech, L_FRAME);

    /* Call the speech encoder */
    cod_amr(st->cod_amr_state, mode, new_speech, prm, usedMode, syn);

    /* Parameters to serial bits */
    Prm2bits(*usedMode, prm, &serial[0], &(st->cod_amr_state->common_amr_tbls));

    return;
}
