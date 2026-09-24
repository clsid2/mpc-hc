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



 Pathname: ./cpp/include/synthesis_amr_wb.h

------------------------------------------------------------------------------
 INCLUDE DESCRIPTION

------------------------------------------------------------------------------
*/

#ifndef SYNTHESIS_AMR_WB_H
#define SYNTHESIS_AMR_WB_H


/*----------------------------------------------------------------------------
; INCLUDES
----------------------------------------------------------------------------*/

#include "pv_amr_wb_type_defs.h"

/*----------------------------------------------------------------------------
; MACROS
; Define module specific macros here
----------------------------------------------------------------------------*/

/*----------------------------------------------------------------------------
; EXTERNAL VARIABLES REFERENCES
----------------------------------------------------------------------------*/

/*----------------------------------------------------------------------------
; DEFINES AND SIMPLE TYPEDEF'S
----------------------------------------------------------------------------*/

#ifdef __cplusplus
extern "C"
{
#endif

    void synthesis_amr_wb(
        int16 Aq[],                          /* A(z)  : quantized Az               */
        int16 exc[],                         /* (i)   : excitation at 12kHz        */
        int16 Q_new,                         /* (i)   : scaling performed on exc   */
        int16 synth16k[],                    /* (o)   : 16kHz synthesis signal     */
        int16 prms,                          /* (i)   : parameter                  */
        int16 HfIsf[],
        int16 nb_bits,
        int16 newDTXState,
        Decoder_State * st,                   /* (i/o) : State structure            */
        int16 bfi,                           /* (i)   : bad frame indicator        */
        int16 * ScratchMemory
    );

#ifdef __cplusplus
}
#endif



#endif  /* PV_NORMALIZE_H */
