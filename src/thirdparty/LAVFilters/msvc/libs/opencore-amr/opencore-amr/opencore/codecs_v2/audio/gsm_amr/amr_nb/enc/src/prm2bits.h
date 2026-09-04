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
*      File             : prm2bits.h
*      Purpose          : Converts the encoder parameter vector into a
*                       : vector of serial bits.
*
********************************************************************************
*/
#ifndef prm2bits_h
#define prm2bits_h "$Id $"

/*
********************************************************************************
*                         INCLUDE FILES
********************************************************************************
*/
#include "typedef.h"
#include "mode.h"
#include "get_const_tbls.h"

#ifdef __cplusplus
extern "C"
{
#endif

    /*
    ********************************************************************************
    *                         DEFINITION OF DATA TYPES
    ********************************************************************************
    */

    /*
    ********************************************************************************
    *                         DECLARATION OF PROTOTYPES
    ********************************************************************************
    */
    void Prm2bits(
        enum Mode mode,    /* i : AMR mode */
        Word16 prm[],      /* input : analysis parameters                       */
        Word16 bits[],      /* output: serial bits                              */
        CommonAmrTbls* common_amr_tbls /* i : ptr to strcut of table ptrs       */
    );

#ifdef __cplusplus
}
#endif

#endif
