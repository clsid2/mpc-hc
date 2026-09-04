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



 Filename: d_plsf.cpp

------------------------------------------------------------------------------
 MODULE DESCRIPTION

 common part (reset) of LSF decoder
 module (rest in d_plsf_3.c and d_plsf_5.c)
------------------------------------------------------------------------------
*/

/*----------------------------------------------------------------------------
; INCLUDES
----------------------------------------------------------------------------*/
#include "typedef.h"
#include "basic_op.h"
#include "cnst.h"
#include "oscl_mem.h"
#include "d_plsf.h"
#include "q_plsf_5_tbl.h"


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

/*----------------------------------------------------------------------------
; EXTERNAL FUNCTION REFERENCES
; Declare functions defined elsewhere and referenced in this module
----------------------------------------------------------------------------*/

/*----------------------------------------------------------------------------
; EXTERNAL VARIABLES REFERENCES
; Declare variables used in this module but defined elsewhere
----------------------------------------------------------------------------*/

/*
------------------------------------------------------------------------------
 FUNCTION NAME: D_plsf_reset
------------------------------------------------------------------------------
 INPUT AND OUTPUT DEFINITIONS

 Inputs:
    state = pointer to structure of type D_plsf_reset

 Outputs:
    fields of the structure pointed to by state is initialized to zero

 Returns:
    return_value = 0, if reset was successful; -1, otherwise (int)

 Global Variables Used:
    None

 Local Variables Needed:
    None

------------------------------------------------------------------------------
 FUNCTION DESCRIPTION

 Resets state memory

------------------------------------------------------------------------------
 REQUIREMENTS

 None

------------------------------------------------------------------------------
 REFERENCES

 d_plsf.c, UMTS GSM AMR speech codec, R99 - Version 3.2.0, March 2, 2001

------------------------------------------------------------------------------
 PSEUDO-CODE

int D_plsf_reset (D_plsfState *state)
{
  Word16 i;

  if (state == (D_plsfState *) NULL){
      // fprintf(stderr, "D_plsf_reset: invalid parameter\n");
      return -1;
  }

  for (i = 0; i < M; i++){
      state->past_r_q[i] = 0;             // Past quantized prediction error
  }

  // Past dequantized lsfs
  Copy(mean_lsf, &state->past_lsf_q[0], M);

  return 0;
}
------------------------------------------------------------------------------
 CAUTION [optional]
 [State any special notes, constraints or cautions for users of this function]

------------------------------------------------------------------------------
*/

Word16 D_plsf_reset(D_plsfState *state, const Word16* mean_lsf_5_ptr)
{
    Word16 i;

    if (state == (D_plsfState *) NULL)
    {
        /* fprintf(stderr, "D_plsf_reset: invalid parameter\n"); */
        return -1;
    }

    for (i = 0; i < M; i++)
    {
        state->past_r_q[i] = 0;             /* Past quantized prediction error */
    }

    /* Past dequantized lsfs */
    oscl_memmove((void *)&state->past_lsf_q[0], mean_lsf_5_ptr, M*sizeof(*mean_lsf_5_ptr));

    return 0;

}
