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
#ifndef __PVGSMAMR_H
#define __PVGSMAMR_H


// includes
#include <e32std.h>
#include <e32base.h>

#include "sp_dec.h"
#include "pvglobals.h"


// PVGsmDecoder AO
class CPVGsmDecoder : public CBase
{
    public:
        IMPORT_C static CPVGsmDecoder* NewL(void);
        IMPORT_C ~CPVGsmDecoder();
        IMPORT_C TInt StartL(void);

        // only port the API's used in PVPlayer 2.0
        IMPORT_C TInt DecodeFrame(enum Mode mode, unsigned char* compressedBlock, unsigned char* audioBuffer);
        IMPORT_C TInt InitDecoder(void);
        IMPORT_C void ExitDecoder(void);

    private:
        CPVGsmDecoder();
        void ConstructL(void);

        Speech_Decode_FrameState* decState;
        enum RXFrameType rx_type;
        struct globalDataStruct *gds;
};

#endif
