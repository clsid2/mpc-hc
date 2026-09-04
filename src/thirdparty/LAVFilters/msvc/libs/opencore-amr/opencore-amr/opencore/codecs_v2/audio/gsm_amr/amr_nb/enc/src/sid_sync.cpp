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



 Filename: sid_sync.cpp
 Functions: sid_sync_init
            sid_sync_reset
            sid_sync_exit
            sid_sync_set_handover_debt
            sid_sync

------------------------------------------------------------------------------
 MODULE DESCRIPTION

 This file contains the functions that initialize, reset, exit, and perform
 SID synchronization.

------------------------------------------------------------------------------
*/


/*----------------------------------------------------------------------------
; INCLUDES
----------------------------------------------------------------------------*/
#include "typedef.h"
#include "basic_op.h"
#include "mode.h"
#include "sid_sync.h"
#include "oscl_mem.h"

/*----------------------------------------------------------------------------
; MACROS
; [Define module specific macros here]
----------------------------------------------------------------------------*/


/*----------------------------------------------------------------------------
; DEFINES
; [Include all pre-processor statements here. Include conditional
; compile variables also.]
----------------------------------------------------------------------------*/

/*----------------------------------------------------------------------------
; LOCAL FUNCTION DEFINITIONS
; [List function prototypes here]
----------------------------------------------------------------------------*/

/*----------------------------------------------------------------------------
; LOCAL VARIABLE DEFINITIONS
; [Variable declaration - defined here and used outside this module]
----------------------------------------------------------------------------*/


/*
------------------------------------------------------------------------------
 FUNCTION NAME: sid_sync_init
------------------------------------------------------------------------------
 INPUT AND OUTPUT DEFINITIONS

 Inputs:
    state = pointer containing a pointer to the state structure used for
            SID synchronization (void)

 Outputs:
    None

 Returns:
    return_value = status of sid_sync_reset function; -1, if state is pointing
                   to a NULL address (int)

 Global Variables Used:
    None

 Local Variables Needed:
    None

------------------------------------------------------------------------------
 FUNCTION DESCRIPTION

 This function initialize one instance of the sid_sync module. It stores
 the pointer to state struct in *st. This pointer has to be passed to sid_sync
 in each call. This function returns 0 on success, otherwise, -1.

------------------------------------------------------------------------------
 REQUIREMENTS

 None

------------------------------------------------------------------------------
 REFERENCES

 sid_sync.c, UMTS GSM AMR speech codec, R99 - Version 3.2.0, March 2, 2001

------------------------------------------------------------------------------
 PSEUDO-CODE

------------------------------------------------------------------------------
 CAUTION [optional]
 [State any special notes, constraints or cautions for users of this function]

------------------------------------------------------------------------------
*/

Word16 sid_sync_init(void **state)
{
    sid_syncState* s;

    if (state == NULL)
    {
        /* fprintf(stderr, "sid_sync_init:invalid state parameter\n"); */
        return -1;
    }

    *state = NULL;

    /* allocate memory */
    if ((s = (sid_syncState *)
             oscl_malloc(sizeof(sid_syncState))) == NULL)
    {
        /* fprintf(stderr,
                "sid_sync_init: "
                "can not malloc state structure\n"); */
        return -1;
    }
    s->sid_update_rate = 8;

    *state = (void *)s;

    return(sid_sync_reset(s));
}

/****************************************************************************/

/*
------------------------------------------------------------------------------
 FUNCTION NAME: sid_sync_reset
------------------------------------------------------------------------------
 INPUT AND OUTPUT DEFINITIONS

 Inputs:
    state = pointer to the state structure used for SID synchronization (void)

 Outputs:
    None

 Returns:
    return_value = 0 (int)

 Global Variables Used:
    None

 Local Variables Needed:
    None

------------------------------------------------------------------------------
 FUNCTION DESCRIPTION

 This function performs a reset of the sid_sync module by setting the state
 memory to zero.

------------------------------------------------------------------------------
 REQUIREMENTS

 None

------------------------------------------------------------------------------
 REFERENCES

 sid_sync.c, UMTS GSM AMR speech codec, R99 - Version 3.2.0, March 2, 2001

------------------------------------------------------------------------------
 PSEUDO-CODE

------------------------------------------------------------------------------
 CAUTION [optional]
 [State any special notes, constraints or cautions for users of this function]

------------------------------------------------------------------------------
*/
Word16 sid_sync_reset(void *st)
{
    sid_syncState *state = (sid_syncState *) st;

    state->sid_update_counter = 3;
    state->sid_handover_debt = 0;
    state->prev_ft = TX_SPEECH_GOOD;

    return 0;
}


/****************************************************************************/

/*
------------------------------------------------------------------------------
 FUNCTION NAME: sid_sync_exit
------------------------------------------------------------------------------
 INPUT AND OUTPUT DEFINITIONS

 Inputs:
    state = pointer containing a pointer to the state structure used for
            SID synchronization (void)

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

 This function frees up the state structure used by sid_sync function. It
 stores NULL in *state.

------------------------------------------------------------------------------
 REQUIREMENTS

 None

------------------------------------------------------------------------------
 REFERENCES

 sid_sync.c, UMTS GSM AMR speech codec, R99 - Version 3.2.0, March 2, 2001

------------------------------------------------------------------------------
 PSEUDO-CODE

------------------------------------------------------------------------------
 CAUTION [optional]
 [State any special notes, constraints or cautions for users of this function]

------------------------------------------------------------------------------
*/
void sid_sync_exit(void **state)
{
    sid_syncState **st = (sid_syncState **) state;

    if (st == NULL || *st == NULL)
    {
        return;
    }

    /* deallocate memory */
    oscl_free(*st);
    *st = NULL;

    return;

}

/****************************************************************************/

/*
------------------------------------------------------------------------------
 FUNCTION NAME: sid_sync_set_handover_debt
------------------------------------------------------------------------------
 INPUT AND OUTPUT DEFINITIONS

 Inputs:
    st = pointer to the state structure used for SID synchronization
         (sid_syncState)
    debtFrames = number of handover debt frames (Word16)

 Outputs:
    st->sid_handover_debt is set to debtFrames

 Returns:
    return_value = 0

 Global Variables Used:
    None

 Local Variables Needed:
    None

------------------------------------------------------------------------------
 FUNCTION DESCRIPTION

 This function updates the handover debt to debtFrames. Extra SID_UPD are
 scheduled to update remote decoder CNI states, right after an handover.
 This is primarily for use on MS UL side.

------------------------------------------------------------------------------
 REQUIREMENTS

 None

------------------------------------------------------------------------------
 REFERENCES

 sid_sync.c, UMTS GSM AMR speech codec, R99 - Version 3.2.0, March 2, 2001

------------------------------------------------------------------------------
 PSEUDO-CODE

------------------------------------------------------------------------------
 CAUTION [optional]
 [State any special notes, constraints or cautions for users of this function]

------------------------------------------------------------------------------
*/
void sid_sync_set_handover_debt(sid_syncState *st,
                                Word16 debtFrames)
{
    /* debtFrames >= 0 */
    st->sid_handover_debt = debtFrames;
    return;
}


/****************************************************************************/

/*
------------------------------------------------------------------------------
 FUNCTION NAME: sid_sync
------------------------------------------------------------------------------
 INPUT AND OUTPUT DEFINITIONS

 Inputs:
    state = pointer to the state structure used for SID synchronization
            (sid_syncState)
    mode = codec mode (enum Mode)
    tx_frame_type = pointer to TX frame type store (enum TXFrameType)

 Outputs:
    tx_frame_type contains the new TX frame type

 Returns:
    None

 Global Variables Used:
    None

 Local Variables Needed:
    None

------------------------------------------------------------------------------
 FUNCTION DESCRIPTION

 This function performs SID frame synchronization to ensure that the mode
 only switches to a neighbouring mode.

------------------------------------------------------------------------------
 REQUIREMENTS

 None

------------------------------------------------------------------------------
 REFERENCES

 sid_sync.c, UMTS GSM AMR speech codec, R99 - Version 3.2.0, March 2, 2001

------------------------------------------------------------------------------
 PSEUDO-CODE

------------------------------------------------------------------------------
 CAUTION [optional]
 [State any special notes, constraints or cautions for users of this function]

------------------------------------------------------------------------------
*/
void sid_sync(void *state,
              enum Mode mode,
              enum TXFrameType *tx_frame_type)
{

    sid_syncState *st = (sid_syncState *) state;

    if (mode == MRDTX)
    {

        st->sid_update_counter--;

        if (st->prev_ft == TX_SPEECH_GOOD)
        {
            *tx_frame_type = TX_SID_FIRST;
            st->sid_update_counter = 3;
        }
        else
        {
            /* TX_SID_UPDATE or TX_NO_DATA */
            if ((st->sid_handover_debt > 0) &&
                    (st->sid_update_counter > 2))
            {
                /* ensure extra updates are  properly delayed after
                   a possible SID_FIRST */
                *tx_frame_type = TX_SID_UPDATE;
                st->sid_handover_debt--;
            }
            else
            {
                if (st->sid_update_counter == 0)
                {
                    *tx_frame_type = TX_SID_UPDATE;
                    st->sid_update_counter = st->sid_update_rate;
                }
                else
                {
                    *tx_frame_type = TX_NO_DATA;
                }
            }
        }
    }
    else
    {
        st->sid_update_counter = st->sid_update_rate ;
        *tx_frame_type = TX_SPEECH_GOOD;
    }
    st->prev_ft = *tx_frame_type;
}

