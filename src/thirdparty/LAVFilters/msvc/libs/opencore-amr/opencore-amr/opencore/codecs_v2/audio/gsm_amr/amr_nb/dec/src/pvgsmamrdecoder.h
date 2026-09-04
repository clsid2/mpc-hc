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

#ifndef __PVGSMAMRDECODER_H
#define __PVGSMAMRDECODER_H

#include "oscl_base.h"

#include "gsmamr_dec.h"

// PVGSMAMRDecoder
class CPVGSMAMRDecoder
{
    public:
        OSCL_IMPORT_REF CPVGSMAMRDecoder();
        OSCL_IMPORT_REF ~CPVGSMAMRDecoder();

        OSCL_IMPORT_REF int32 InitDecoder(void);
        OSCL_IMPORT_REF int32 DecodeFrame(Frame_Type_3GPP aType,
                                          uint8* aCompressedBlock,
                                          uint8* aAudioBuffer,
                                          int32 aFormat);
        OSCL_IMPORT_REF int32 ResetDecoder(void);
        OSCL_IMPORT_REF void TerminateDecoder(void);

    private:
        void* iDecState;
};

#endif

