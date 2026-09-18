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
*****************************************************************************
*
*      GSM AMR-NB speech codec   R98   Version 7.5.0   March 2, 2001
*                                R99   Version 3.2.0
*                                REL-4 Version 4.0.0
*
*****************************************************************************
*
*      File             : frame.h
*      Purpose          : Declaration of received and transmitted frame types
*
*****************************************************************************
*/
#ifndef frame_h
#define frame_h "$Id $"

/*
*****************************************************************************
*                         INCLUDE FILES
*****************************************************************************
*/


#ifdef __cplusplus
extern "C"
{
#endif

    /*
    *****************************************************************************
    *                         DEFINITION OF DATA TYPES
    *****************************************************************************
    * Note: The order of the TX and RX_Type identifiers has been chosen in
    *       the way below to be compatible to an earlier version of the
    *       AMR-NB C reference program.
    *****************************************************************************
    */

    enum RXFrameType { RX_SPEECH_GOOD = 0,
        RX_SPEECH_DEGRADED,
        RX_ONSET,
        RX_SPEECH_BAD,
        RX_SID_FIRST,
        RX_SID_UPDATE,
        RX_SID_BAD,
        RX_NO_DATA,
        RX_N_FRAMETYPES     /* number of frame types */
    };

    enum TXFrameType { TX_SPEECH_GOOD = 0,
                       TX_SID_FIRST,
                       TX_SID_UPDATE,
                       TX_NO_DATA,
                       TX_SPEECH_DEGRADED,
                       TX_SPEECH_BAD,
                       TX_SID_BAD,
                       TX_ONSET,
                       TX_N_FRAMETYPES     /* number of frame types */
                     };


    /* Channel decoded frame type */
    enum CHDECFrameType { CHDEC_SID_FIRST = 0,
                          CHDEC_SID_FIRST_INCOMPLETE,
                          CHDEC_SID_UPDATE_INCOMPLETE,
                          CHDEC_SID_UPDATE,
                          CHDEC_SPEECH,
                          CHDEC_SPEECH_ONSET,
                          CHDEC_ESCAPE_MARKER,
                          CHDEC_ESCAPE_DATA,
                          CHDEC_NO_DATA
                        };

    /* Channel decoded frame quality */
    enum CHDECFrameQuality { CHDEC_GOOD = 0,
                             CHDEC_PROBABLY_DEGRADED,
                             CHDEC_PROBABLY_BAD,
                             CHDEC_BAD
                           };

#ifdef __cplusplus
}
#endif

#endif
