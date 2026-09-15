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



 Filename: ton_stab.cpp

------------------------------------------------------------------------------
*/


/*----------------------------------------------------------------------------
; INCLUDES
----------------------------------------------------------------------------*/
#include "ton_stab.h"
#include "oper_32b.h"
#include "cnst.h"
#include "set_zero.h"
#include "basic_op.h"
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
 FUNCTION NAME: ton_stab_init
------------------------------------------------------------------------------
 INPUT AND OUTPUT DEFINITIONS

 Inputs:
    state = pointer to pointer to structure type tonStabState.

 Outputs:
    None

 Returns:
    None.

 Global Variables Used:
    None

 Local Variables Needed:
    None

------------------------------------------------------------------------------
 FUNCTION DESCRIPTION

  Function:   ton_stab_init
  Purpose:    Allocates state memory and initializes state memory

------------------------------------------------------------------------------
 REQUIREMENTS

  None.

------------------------------------------------------------------------------
 REFERENCES

 ton_stab.c, UMTS GSM AMR speech codec, R99 - Version 3.2.0, March 2, 2001

------------------------------------------------------------------------------
 PSEUDO-CODE

int ton_stab_init (tonStabState **state)
{
    tonStabState* s;

    if (state == (tonStabState **) NULL){
        // fprintf(stderr, "ton_stab_init: invalid parameter\n");
        return -1;
    }
    *state = NULL;

    // allocate memory
    if ((s= (tonStabState *) malloc(sizeof(tonStabState))) == NULL){
        // fprintf(stderr, "ton_stab_init: can not malloc state structure\n");
        return -1;
    }

    ton_stab_reset(s);

    *state = s;

    return 0;
}


------------------------------------------------------------------------------
 CAUTION [optional]
 [State any special notes, constraints or cautions for users of this function]

------------------------------------------------------------------------------
*/

Word16 ton_stab_init(tonStabState **state)
{
    tonStabState* s;

    if (state == (tonStabState **) NULL)
    {
        /* fprintf(stderr, "ton_stab_init: invalid parameter\n"); */
        return -1;
    }
    *state = NULL;

    /* allocate memory */
    if ((s = (tonStabState *) oscl_malloc(sizeof(tonStabState))) == NULL)
    {
        /* fprintf(stderr, "ton_stab_init: can not malloc state structure\n"); */
        return -1;
    }

    ton_stab_reset(s);

    *state = s;

    return 0;
}

/****************************************************************************/


/*
------------------------------------------------------------------------------
 FUNCTION NAME: ton_stab_reset
------------------------------------------------------------------------------
 INPUT AND OUTPUT DEFINITIONS

 Inputs:
    st = pointer to pointer to structure type tonStabState.

 Outputs:
    None

 Returns:
    None.

 Global Variables Used:
    None

 Local Variables Needed:
    None

------------------------------------------------------------------------------
 FUNCTION DESCRIPTION

  Function:   ton_stab_reset
  Purpose:    Initializes state memory to zero

------------------------------------------------------------------------------
 REQUIREMENTS

 None.

------------------------------------------------------------------------------
 REFERENCES

 ton_stab.c, UMTS GSM AMR speech codec, R99 - Version 3.2.0, March 2, 2001

------------------------------------------------------------------------------
 PSEUDO-CODE

int ton_stab_reset (tonStabState *st)
{
    if (st == (tonStabState *) NULL){
        // fprintf(stderr, "ton_stab_init: invalid parameter\n");
        return -1;
    }

    // initialize tone stabilizer state
    st->count = 0;
    Set_zero(st->gp, N_FRAME);    // Init Gp_Clipping

    return 0;
}

------------------------------------------------------------------------------
 CAUTION [optional]
 [State any special notes, constraints or cautions for users of this function]

------------------------------------------------------------------------------
*/

Word16 ton_stab_reset(tonStabState *st)
{
    if (st == (tonStabState *) NULL)
    {
        /* fprintf(stderr, "ton_stab_init: invalid parameter\n"); */
        return -1;
    }

    /* initialize tone stabilizer state */
    st->count = 0;
    /* Init Gp_Clipping */
    oscl_memset((void *)st->gp,  0, N_FRAME*sizeof(*st->gp));


    return 0;
}

/****************************************************************************/

/*
------------------------------------------------------------------------------
 FUNCTION NAME: ton_stab_exit
------------------------------------------------------------------------------
 INPUT AND OUTPUT DEFINITIONS

 Inputs:
    state = pointer to pointer to structure type tonStabState.

 Outputs:
    None

 Returns:
    None.

 Global Variables Used:
    None

 Local Variables Needed:
    None

------------------------------------------------------------------------------
 FUNCTION DESCRIPTION

  Function:   ton_stab_exit
  Purpose:    The memory used for state memory is freed

------------------------------------------------------------------------------
 REQUIREMENTS

 None.

------------------------------------------------------------------------------
 REFERENCES

 ton_stab.c, UMTS GSM AMR speech codec, R99 - Version 3.2.0, March 2, 2001

------------------------------------------------------------------------------
 PSEUDO-CODE

void ton_stab_exit (tonStabState **state)
{
    if (state == NULL || *state == NULL)
        return;

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

void ton_stab_exit(tonStabState **state)
{
    if (state == NULL || *state == NULL)
        return;

    /* deallocate memory */
    oscl_free(*state);
    *state = NULL;

    return;
}

/****************************************************************************/

/*
------------------------------------------------------------------------------
 FUNCTION NAME: check_lsp
------------------------------------------------------------------------------
 INPUT AND OUTPUT DEFINITIONS

 Inputs:
    state = pointer to pointer to structure type tonStabState.
    lsp   = pointer to unquantized LSPs of type Word16

 Outputs:
    pOverflow = 1 if there is an overflow else it is zero.

 Returns:
    None.

 Global Variables Used:
    None

 Local Variables Needed:
    None

------------------------------------------------------------------------------
 FUNCTION DESCRIPTION

  Function:  check_lsp()
  Purpose:   Check the LSP's to detect resonances

------------------------------------------------------------------------------
 REQUIREMENTS

 None.

------------------------------------------------------------------------------
 REFERENCES

 ton_stab.c, UMTS GSM AMR speech codec, R99 - Version 3.2.0, March 2, 2001

------------------------------------------------------------------------------
 PSEUDO-CODE

Word16 check_lsp(tonStabState *st, // i/o : State struct
                 Word16 *lsp       // i   : unquantized LSP's
)
{
   Word16 i, dist, dist_min1, dist_min2, dist_th;

   // Check for a resonance:
   // Find minimum distance between lsp[i] and lsp[i+1]

   dist_min1 = MAX_16;
   for (i = 3; i < M-2; i++)
   {
      dist = sub(lsp[i], lsp[i+1]);

      if (sub(dist, dist_min1) < 0)
      {
         dist_min1 = dist;
      }
   }

   dist_min2 = MAX_16;
   for (i = 1; i < 3; i++)
   {
      dist = sub(lsp[i], lsp[i+1]);

      if (sub(dist, dist_min2) < 0)
      {
         dist_min2 = dist;
      }
   }

   if (sub(lsp[1], 32000) > 0)
   {
      dist_th = 600;
   }
   else if (sub(lsp[1], 30500) > 0)
   {
      dist_th = 800;
   }
   else
   {
      dist_th = 1100;
   }

   if (sub(dist_min1, 1500) < 0 ||
       sub(dist_min2, dist_th) < 0)
   {
      st->count = add(st->count, 1);
   }
   else
   {
      st->count = 0;
   }

   // Need 12 consecutive frames to set the flag
   if (sub(st->count, 12) >= 0)
   {
      st->count = 12;
      return 1;
   }
   else
   {
      return 0;
   }
}

------------------------------------------------------------------------------
 CAUTION [optional]
 [State any special notes, constraints or cautions for users of this function]

------------------------------------------------------------------------------
*/

Word16 check_lsp(tonStabState *st, /* i/o : State struct            */
                 Word16 *lsp,      /* i   : unquantized LSP's       */
                 Flag  *pOverflow
                )
{
    Word16 i;
    Word16 dist;
    Word16 dist_min1;
    Word16 dist_min2;
    Word16 dist_th;
    Word16 *p_lsp   = &lsp[3];
    Word16 *p_lsp_1 = &lsp[4];

    OSCL_UNUSED_ARG(pOverflow);
    /* Check for a resonance:                             */
    /* Find minimum distance between lsp[i] and lsp[i+1]  */

    dist_min1 = MAX_16;
    for (i = 3; i < M - 2; i++)
    {
        dist = *(p_lsp++) - *(p_lsp_1++);

        if (dist < dist_min1)
        {
            dist_min1 = dist;
        }
    }

    dist_min2 = MAX_16;
    p_lsp   = &lsp[1];
    p_lsp_1 = &lsp[2];

    for (i = 1; i < 3; i++)
    {
        dist = *(p_lsp++) - *(p_lsp_1++);

        if (dist < dist_min2)
        {
            dist_min2 = dist;
        }
    }

    if (lsp[1] > 32000)
    {
        dist_th = 600;
    }
    else if (lsp[1] > 30500)
    {
        dist_th = 800;
    }
    else
    {
        dist_th = 1100;
    }

    if ((dist_min1 < 1500) || (dist_min2 < dist_th))
    {
        st->count++;
    }
    else
    {
        st->count = 0;
    }

    /* Need 12 consecutive frames to set the flag */
    if (st->count >= 12)
    {
        st->count = 12;
        return 1;
    }
    else
    {
        return 0;
    }
}

/****************************************************************************/

/*
------------------------------------------------------------------------------
 FUNCTION NAME: check_gp_clipping
------------------------------------------------------------------------------
 INPUT AND OUTPUT DEFINITIONS

 Inputs:
    state = pointer to pointer to structure type tonStabState.
    g_pitch = pitch gain of type Word16

 Outputs:
    pOverflow = 1 if there is an overflow else it is zero.

 Returns:
    None.

 Global Variables Used:
    None

 Local Variables Needed:
    None

------------------------------------------------------------------------------
 FUNCTION DESCRIPTION

  Function:   Check_Gp_Clipping()
  Purpose:    Verify that the sum of the last (N_FRAME+1) pitch
              gains is under a certain threshold.

------------------------------------------------------------------------------
 REQUIREMENTS

 None.

------------------------------------------------------------------------------
 REFERENCES

 ton_stab.c, UMTS GSM AMR speech codec, R99 - Version 3.2.0, March 2, 2001

------------------------------------------------------------------------------
 PSEUDO-CODE

Word16 check_gp_clipping(tonStabState *st, // i/o : State struct
                         Word16 g_pitch    // i   : pitch gain
)
{
   Word16 i, sum;

   sum = shr(g_pitch, 3);          // Division by 8
   for (i = 0; i < N_FRAME; i++)
   {
      sum = add(sum, st->gp[i]);
   }

   if (sub(sum, GP_CLIP) > 0)
   {
      return 1;
   }
   else
   {
      return 0;
   }
}

------------------------------------------------------------------------------
 CAUTION [optional]
 [State any special notes, constraints or cautions for users of this function]

------------------------------------------------------------------------------
*/

Word16 check_gp_clipping(tonStabState *st, /* i/o : State struct            */
                         Word16 g_pitch,   /* i   : pitch gain              */
                         Flag   *pOverflow
                        )
{
    Word16 i;
    Word16 sum;

    sum = shr(g_pitch, 3, pOverflow);        /* Division by 8 */
    for (i = 0; i < N_FRAME; i++)
    {
        sum = add_16(sum, st->gp[i], pOverflow);
    }

    if (sum > GP_CLIP)
    {
        return 1;
    }
    else
    {
        return 0;
    }
}

/****************************************************************************/

/*
------------------------------------------------------------------------------
 FUNCTION NAME: update_gp_clipping
------------------------------------------------------------------------------
 INPUT AND OUTPUT DEFINITIONS

 Inputs:
    state = pointer to pointer to structure type tonStabState.
    g_pitch = pitch gain of type Word16

 Outputs:
    pOverflow = 1 if there is an overflow else it is zero.

 Returns:
    None.

 Global Variables Used:
    None

 Local Variables Needed:
    None

------------------------------------------------------------------------------
 FUNCTION DESCRIPTION

  Function:  Update_Gp_Clipping()
  Purpose:   Update past pitch gain memory

------------------------------------------------------------------------------
 REQUIREMENTS

 None.

------------------------------------------------------------------------------
 REFERENCES

 ton_stab.c, UMTS GSM AMR speech codec, R99 - Version 3.2.0, March 2, 2001

------------------------------------------------------------------------------
 PSEUDO-CODE

void update_gp_clipping(tonStabState *st, // i/o : State struct
                        Word16 g_pitch    // i   : pitch gain
)
{
   Copy(&st->gp[1], &st->gp[0], N_FRAME-1);
   st->gp[N_FRAME-1] = shr(g_pitch, 3);
}

------------------------------------------------------------------------------
 CAUTION [optional]
 [State any special notes, constraints or cautions for users of this function]

------------------------------------------------------------------------------
*/

void update_gp_clipping(tonStabState *st, /* i/o : State struct            */
                        Word16 g_pitch,   /* i   : pitch gain              */
                        Flag   *pOverflow
                       )
{
    OSCL_UNUSED_ARG(pOverflow);
    for (int i = 0; i < N_FRAME - 1; i++)
    {
        st->gp[i] = st->gp[i+1];
    }
    st->gp[N_FRAME-1] =  g_pitch >> 3;
}

