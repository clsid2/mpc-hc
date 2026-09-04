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
#include "decoder_gsm_amr.h"
#include "sp_dec.h"
#include "amrdecode.h"
#include "pvamrnbdecoder_api.h"

// Use default DLL entry point
#include "oscl_dll.h"
#include "oscl_error_codes.h"
#include "oscl_exception.h"
#include "oscl_mem.h"

#define KCAI_CODEC_INIT_FAILURE -1

OSCL_DLL_ENTRY_POINT_DEFAULT()

OSCL_EXPORT_REF CDecoder_AMR_NB *CDecoder_AMR_NB::NewL()
{
    CDecoder_AMR_NB *dec = new CDecoder_AMR_NB;
    if (dec == NULL)
        OSCL_LEAVE(OsclErrNoMemory);
    else
        dec->ConstructL();
    return dec;
}

OSCL_EXPORT_REF void CDecoder_AMR_NB::ConstructL()
{
    iDecState = NULL;
}


/*
-----------------------------------------------------------------------------

    CDecoder_AMR_NB

    ~CDecoder_AMR_NB

    Empty decoder destructor.

    Parameters:     none

    Return Values:  none

-----------------------------------------------------------------------------
*/
OSCL_EXPORT_REF CDecoder_AMR_NB::~CDecoder_AMR_NB()
{
    if (iDecState)
        oscl_free(iDecState);
    iDecState = NULL;

    if (iInputBuf)
    {
        OSCL_ARRAY_DELETE(iInputBuf);
        iInputBuf = NULL;
    }

    if (iOutputBuf)
    {
        OSCL_ARRAY_DELETE(iOutputBuf);
        iOutputBuf = NULL;
    }
}


/*
-----------------------------------------------------------------------------

    CDecoder_AMR_NB

    StartL

    Start decoder object. Initialize codec status.

    Parameters:     none

    Return Values:  status

-----------------------------------------------------------------------------
*/
OSCL_EXPORT_REF int32 CDecoder_AMR_NB::StartL(tPVAmrDecoderExternal * pExt,
        bool aAllocateInputBuffer,
        bool aAllocateOutputBuffer)
{

    if (aAllocateInputBuffer)
    {
        iInputBuf = OSCL_ARRAY_NEW(int16, MAX_NUM_PACKED_INPUT_BYTES);
        if (iInputBuf == NULL)
        {
            return KCAI_CODEC_INIT_FAILURE;
        }
    }
    else
    {
        iInputBuf = NULL;
    }
    pExt->pInputBuffer = (uint8 *)iInputBuf;

    if (aAllocateOutputBuffer)
    {
        iOutputBuf = OSCL_ARRAY_NEW(int16, L_FRAME);

        if (iOutputBuf == NULL)
        {
            return KCAI_CODEC_INIT_FAILURE;
        }
    }
    else
    {
        iOutputBuf = NULL;
    }
    pExt->pOutputBuffer = iOutputBuf;

    pExt->samplingRate = 8000;
    pExt->desiredChannels = 1;

    pExt->reset_flag = 0;
    pExt->reset_flag_old = 1;
    pExt->mode_old = 0;

    return GSMInitDecode(&iDecState, (int8*)"Decoder");
}


/*
-----------------------------------------------------------------------------

    CDecoder_AMR_NB

    ExecuteL

    Execute decoder object. Read one encoded speech frame from the input
    stream,  decode it and write the decoded frame to output stream.

    Parameters:

    Return Values:  status


-----------------------------------------------------------------------------
*/

OSCL_EXPORT_REF int32 CDecoder_AMR_NB::ExecuteL(tPVAmrDecoderExternal * pExt)
{


    if (pExt->input_format == WMF)
        pExt->input_format = MIME_IETF;

    return AMRDecode(iDecState,
                     (enum Frame_Type_3GPP)pExt->mode,
                     (uint8*) pExt->pInputBuffer,
                     (int16*) pExt->pOutputBuffer,
                     pExt->input_format);

}

/*
-----------------------------------------------------------------------------

    CDecoder_AMR_NB

    StopL

    Stop decoder object. Flush out last frames, if necessary.

    Parameters:     none

    Return Values:  none

-----------------------------------------------------------------------------
*/
OSCL_EXPORT_REF void CDecoder_AMR_NB::StopL()
{
}

/*
-----------------------------------------------------------------------------

    CDecoder_AMR_NB

    ResetDecoderL

    Stop decoder object. Reset decoder.

    Parameters:     none

    Return Values:  status

-----------------------------------------------------------------------------
*/
OSCL_EXPORT_REF int32 CDecoder_AMR_NB::ResetDecoderL()
{
    return Speech_Decode_Frame_reset(iDecState);
}


/*
-----------------------------------------------------------------------------

    CDecoder_AMR_NB

    TerminateDecoderL

    Stop decoder object. close decoder.

    Parameters:     none

    Return Values:  none

-----------------------------------------------------------------------------
*/
OSCL_EXPORT_REF void CDecoder_AMR_NB::TerminateDecoderL()
{
    GSMDecodeFrameExit(&iDecState);
    iDecState = NULL;

    if (iInputBuf)
    {
        OSCL_ARRAY_DELETE(iInputBuf);
        iInputBuf = NULL;
    }

    if (iOutputBuf)
    {
        OSCL_ARRAY_DELETE(iOutputBuf);
        iOutputBuf = NULL;
    }
}



