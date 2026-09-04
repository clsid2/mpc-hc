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

    3GPP TS 26.173
    ANSI-C code for the Adaptive Multi-Rate - Wideband (AMR-WB) speech codec
    Available from http://www.3gpp.org

(C) 2007, 3GPP Organizational Partners (ARIB, ATIS, CCSA, ETSI, TTA, TTC)
Permission to distribute, modify and use this file under the standard license
terms listed above has been obtained from the copyright holder.
****************************************************************************************/
/*
------------------------------------------------------------------------------



 Pathname: ./cpp/include/q_pulse.h

------------------------------------------------------------------------------
 INCLUDE DESCRIPTION

  Coding and decoding of algebraic codebook
------------------------------------------------------------------------------
*/

#ifndef Q_PULSE_H
#define Q_PULSE_H


/*----------------------------------------------------------------------------
; INCLUDES
----------------------------------------------------------------------------*/

#include "pv_amr_wb_type_defs.h"


#ifdef __cplusplus
extern "C"
{
#endif


    void dec_1p_N1(int32 index, int16 N, int16 offset, int16 pos[]);
    void dec_2p_2N1(int32 index, int16 N, int16 offset, int16 pos[]);
    void dec_3p_3N1(int32 index, int16 N, int16 offset, int16 pos[]);
    void dec_4p_4N1(int32 index, int16 N, int16 offset, int16 pos[]);
    void dec_4p_4N(int32 index, int16 N, int16 offset, int16 pos[]);
    void dec_5p_5N(int32 index, int16 N, int16 offset, int16 pos[]);
    void dec_6p_6N_2(int32 index, int16 N, int16 offset, int16 pos[]);


#ifdef __cplusplus
}
#endif

#endif  /* Q_PULSE_H */
