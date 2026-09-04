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



 Pathname: ./cpp/include/qisf_ns.h

------------------------------------------------------------------------------
 INCLUDE DESCRIPTION

------------------------------------------------------------------------------
*/

#ifndef QISF_NS_H
#define QISF_NS_H


/*----------------------------------------------------------------------------
; INCLUDES
----------------------------------------------------------------------------*/

#include "pv_amr_wb_type_defs.h"

/*----------------------------------------------------------------------------
; DEFINES
----------------------------------------------------------------------------*/


#define ORDER   16            /* order of linear prediction filter */
#define ISF_GAP 128

#define SIZE_BK_NOISE1  64
#define SIZE_BK_NOISE2  64
#define SIZE_BK_NOISE3  64
#define SIZE_BK_NOISE4  32
#define SIZE_BK_NOISE5  32

#define NB_QUA_GAIN6B  64     /* Number of quantization level */
#define NB_QUA_GAIN7B  128    /* Number of quantization level */

/*----------------------------------------------------------------------------
; MACROS
; Define module specific macros here
----------------------------------------------------------------------------*/

/*----------------------------------------------------------------------------
; EXTERNAL VARIABLES REFERENCES
----------------------------------------------------------------------------*/
extern const int16 mean_isf_noise[ORDER];
extern const int16 dico1_isf_noise[SIZE_BK_NOISE1*2];
extern const int16 dico2_isf_noise[SIZE_BK_NOISE2*3];
extern const int16 dico3_isf_noise[SIZE_BK_NOISE3*3];
extern const int16 dico4_isf_noise[SIZE_BK_NOISE4*4];
extern const int16 dico5_isf_noise[SIZE_BK_NOISE5*4];

extern const int16 t_qua_gain6b[NB_QUA_GAIN6B*2];
extern const int16 t_qua_gain7b[NB_QUA_GAIN7B*2];

/*----------------------------------------------------------------------------
; SIMPLE TYPEDEF'S
----------------------------------------------------------------------------*/




#ifdef __cplusplus
extern "C"
{
#endif


#ifdef __cplusplus
}
#endif




#endif  /* QISF_NS_H */
