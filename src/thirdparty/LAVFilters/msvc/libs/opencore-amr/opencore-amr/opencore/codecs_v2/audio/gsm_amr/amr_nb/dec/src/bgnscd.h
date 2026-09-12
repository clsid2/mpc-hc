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



 Filename: bgnscd.h

------------------------------------------------------------------------------
 INCLUDE DESCRIPTION

      File             : bgnscd.h
      Purpose          : Background noise source charateristic detector (SCD)

------------------------------------------------------------------------------
*/

#ifndef _BGNSCD_H_
#define _BGNSCD_H_

/*----------------------------------------------------------------------------
; INCLUDES
----------------------------------------------------------------------------*/
#include "typedef.h"
#include "cnst.h"

/*--------------------------------------------------------------------------*/
#ifdef __cplusplus
extern "C"
{
#endif

    /*----------------------------------------------------------------------------
    ; MACROS
    ; [Define module specific macros here]
    ----------------------------------------------------------------------------*/

    /*----------------------------------------------------------------------------
    ; DEFINES
    ; [Include all pre-processor statements here.]
    ----------------------------------------------------------------------------*/
#define L_ENERGYHIST 60
#define INV_L_FRAME 102


    /* 2*(160*x)^2 / 65536  where x is FLP values 150,5 and 50 */
#define FRAMEENERGYLIMIT  17578         /* 150 */
#define LOWERNOISELIMIT      20         /*   5 */
#define UPPERNOISELIMIT    1953         /*  50 */


    /*----------------------------------------------------------------------------
    ; EXTERNAL VARIABLES REFERENCES
    ; [Declare variables used in this module but defined elsewhere]
    ----------------------------------------------------------------------------*/

    /*----------------------------------------------------------------------------
    ; SIMPLE TYPEDEF'S
    ----------------------------------------------------------------------------*/

    /*----------------------------------------------------------------------------
    ; ENUMERATED TYPEDEF'S
    ----------------------------------------------------------------------------*/

    /*----------------------------------------------------------------------------
    ; STRUCTURES TYPEDEF'S
    ----------------------------------------------------------------------------*/
    typedef struct
    {
        /* history vector of past synthesis speech energy */
        Word16 frameEnergyHist[L_ENERGYHIST];

        /* state flags */
        Word16 bgHangover; /* counter; number of frames after last speech frame */

    } Bgn_scdState;

    /*----------------------------------------------------------------------------
    ; GLOBAL FUNCTION DEFINITIONS
    ; [List function prototypes here]
    ----------------------------------------------------------------------------*/
    /*
     *  Function    : Bgn_scd_init
     *  Purpose     : Allocates initializes state memory
     *  Description : Stores pointer to filter status struct in *st. This
     *                pointer has to be passed to Bgn_scd in each call.
     *  Returns     : 0 on success
     */
    Word16 Bgn_scd_init(Bgn_scdState **st);

    /*
     *  Function    : Bgn_scd_reset
     *  Purpose     : Resets state memory
     *  Returns     : 0 on success
     */
    Word16 Bgn_scd_reset(Bgn_scdState *st);

    /*
     *  Function    : Bgn_scd_exit
     *  Purpose     : The memory used for state memory is freed
     *  Description : Stores NULL in *s
     *  Returns     : void
     */
    void Bgn_scd_exit(Bgn_scdState **st);

    /*
     *  Function    : Bgn_scd
     *  Purpose     : Charaterice synthesis speech and detect background noise
     *  Returns     : background noise decision; 0 = bgn, 1 = no bgn
     */
    Word16 Bgn_scd(Bgn_scdState *st,       /* i : State variables for bgn SCD         */
                   Word16 ltpGainHist[],  /* i : LTP gain history                    */
                   Word16 speech[],       /* o : synthesis speech frame              */
                   Word16 *voicedHangover,/* o : # of frames after last voiced frame */
                   Flag   *pOverflow
                  );


#ifdef __cplusplus
}
#endif

#endif  /* _BGNSCD_H_ */
