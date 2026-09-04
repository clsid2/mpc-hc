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
//////////////////////////////////////////////////////////////////////////////////
//                                                                              //
//  File: decoder_amr_nb.h                                                      //
//                                                                              //
//////////////////////////////////////////////////////////////////////////////////

#ifndef _DECODER_AMR_NB_H
#define _DECODER_AMR_NB_H

#include "oscl_base.h"
#include "pvgsmamrdecoderinterface.h"

// CDecoder_AMR_WB
class CDecoder_AMRInterface;
class CDecoder_AMR_NB: public CDecoder_AMRInterface
{
    public:
        OSCL_IMPORT_REF void ConstructL();
        OSCL_IMPORT_REF static CDecoder_AMR_NB *NewL();
        OSCL_IMPORT_REF virtual ~CDecoder_AMR_NB();

        OSCL_IMPORT_REF virtual int32 StartL(tPVAmrDecoderExternal * pExt,
                                             bool aAllocateInputBuffer  = false,
                                             bool aAllocateOutputBuffer = false);

        OSCL_IMPORT_REF virtual int32 ExecuteL(tPVAmrDecoderExternal * pExt);

        OSCL_IMPORT_REF virtual int32 ResetDecoderL();
        OSCL_IMPORT_REF virtual void StopL();
        OSCL_IMPORT_REF virtual void TerminateDecoderL();

    private:
        void* iDecState;

        int16* iInputBuf;
        int16* iOutputBuf;


};


#endif

