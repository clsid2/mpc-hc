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
/*--------------------------------------------------------------------------*
 *                       get_amr_wb_bits.h                                  *
 *--------------------------------------------------------------------------*
 *       Number of bits for different modes                                 *
 *--------------------------------------------------------------------------*/

#ifndef GET_AMR_WB_BITS_H
#define GET_AMR_WB_BITS_H


#include "pv_amr_wb_type_defs.h"

#ifdef __cplusplus
extern "C"
{
#endif


    int16 Serial_parm(                        /* Return the parameter    */
        int16 no_of_bits,                    /* input : number of bits  */
        int16 ** prms
    );

    int16 Serial_parm_1bit(                        /* Return the parameter    */
        int16 ** prms
    );

#ifdef __cplusplus
}
#endif

#endif
