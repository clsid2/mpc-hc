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



 Pathname: ./cpp/include/qpisf_2s.h

------------------------------------------------------------------------------
 INCLUDE DESCRIPTION

------------------------------------------------------------------------------
*/

#ifndef QPISF_2S_H
#define QPISF_2S_H


/*----------------------------------------------------------------------------
; INCLUDES
----------------------------------------------------------------------------*/

#include "qisf_ns.h"

/*----------------------------------------------------------------------------
; MACROS
; Define module specific macros here
----------------------------------------------------------------------------*/


/*----------------------------------------------------------------------------
; DEFINES
----------------------------------------------------------------------------*/

#define N_SURV  4

#define SIZE_BK1  256
#define SIZE_BK2  256
#define SIZE_BK21 64
#define SIZE_BK22 128
#define SIZE_BK23 128
#define SIZE_BK24 32
#define SIZE_BK25 32

#define SIZE_BK21_36b 128
#define SIZE_BK22_36b 128
#define SIZE_BK23_36b 64


/*----------------------------------------------------------------------------
; EXTERNAL VARIABLES REFERENCES
----------------------------------------------------------------------------*/
extern const int16 mean_isf[ORDER];
extern const int16 dico1_isf[SIZE_BK1*9];
extern const int16 dico2_isf[SIZE_BK2*7];
extern const int16 dico21_isf[SIZE_BK21*3];
extern const int16 dico22_isf[SIZE_BK22*3];
extern const int16 dico23_isf[SIZE_BK23*3];
extern const int16 dico24_isf[SIZE_BK24*3];
extern const int16 dico25_isf[SIZE_BK25*4];
extern const int16 dico21_isf_36b[SIZE_BK21_36b*5];
extern const int16 dico22_isf_36b[SIZE_BK22_36b*4];
extern const int16 dico23_isf_36b[SIZE_BK23_36b*7];

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




#endif  /* QPISF_2S_H */
