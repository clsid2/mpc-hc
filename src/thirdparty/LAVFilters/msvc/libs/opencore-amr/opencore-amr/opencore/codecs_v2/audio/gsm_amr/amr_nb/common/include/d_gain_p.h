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
********************************************************************************
*
*      GSM AMR-NB speech codec   R98   Version 7.5.0   March 2, 2001
*                                R99   Version 3.2.0
*                                REL-4 Version 4.0.0
*
********************************************************************************
*
*      File             : d_gain_p.h
*      Purpose          : Decodes the pitch gain using the received index.
*
********************************************************************************
*/
#ifndef d_gain_p_h
#define d_gain_p_h "$Id $"

/*
********************************************************************************
*                         INCLUDE FILES
********************************************************************************
*/
#include "typedef.h"
#include "mode.h"

#ifdef __cplusplus
extern "C"
{
#endif

    /*
    **************************************************************************
    *
    *  Function    : d_gain_pitch
    *  Purpose     : Decodes the pitch gain using the received index.
    *  Description : In case of no frame erasure, the gain is obtained
    *                from the quantization table at the given index;
    *                otherwise, a downscaled past gain is used.
    *  Returns     : Quantized pitch gain
    *
    **************************************************************************
    */
    Word16 d_gain_pitch(       /* return value: gain (Q14)                */
        enum Mode mode,        /* i : AMR mode                            */
        Word16 index,          /* i   : index of quantization             */
        const Word16* qua_gain_pitch_ptr /* i : pointer to read-only tables   */
    );

#ifdef __cplusplus
}
#endif

#endif
