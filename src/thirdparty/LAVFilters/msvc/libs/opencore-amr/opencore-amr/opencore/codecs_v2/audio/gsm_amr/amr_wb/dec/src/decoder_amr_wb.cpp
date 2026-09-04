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
//////////////////////////////////////////////////////////////////////////////////
//                                                                              //
//  File: decoder_amr_wb.cpp                                                   //
//                                                                              //
//////////////////////////////////////////////////////////////////////////////////

#include "decoder_amr_wb.h"
#include "pvamrwbdecoder_api.h"
#include "pvamrwbdecoder.h"
#include "pvamrwbdecoder_cnst.h"
#include "dtx.h"


// Use default DLL entry point
#include "oscl_dll.h"
#include "oscl_error_codes.h"
#include "oscl_exception.h"
#include "oscl_mem.h"


#define KCAI_CODEC_INIT_FAILURE -1


OSCL_DLL_ENTRY_POINT_DEFAULT()

OSCL_EXPORT_REF CDecoder_AMR_WB *CDecoder_AMR_WB::NewL()
{
    CDecoder_AMR_WB *dec = new CDecoder_AMR_WB;
    if (dec == NULL)
        OSCL_LEAVE(OsclErrNoMemory);
    else
        dec->ConstructL();
    return dec;
}

OSCL_EXPORT_REF void CDecoder_AMR_WB::ConstructL()
{
    st = NULL;
    pt_st = NULL;
    ScratchMem = NULL;
    iInputBuf = NULL;
    iOutputBuf = NULL;
}


/*
-----------------------------------------------------------------------------

    CDecoder_AMR_WB

    ~CDecoder_AMR_WB

    Empty decoder destructor.

    Parameters:     none

    Return Values:  none

-----------------------------------------------------------------------------
*/
OSCL_EXPORT_REF CDecoder_AMR_WB::~CDecoder_AMR_WB()
{
    st = NULL;
    ScratchMem = NULL;

    if (pt_st != NULL)
    {
        OSCL_ARRAY_DELETE((uint8*)pt_st);
        pt_st = NULL;
    }

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

    CDecoder_AMR_WB

    StartL

    Start decoder object. Initialize codec status.

    Parameters:     none

    Return Values:  status

-----------------------------------------------------------------------------
*/
OSCL_EXPORT_REF int32 CDecoder_AMR_WB::StartL(tPVAmrDecoderExternal * pExt,
        bool aAllocateInputBuffer,
        bool aAllocateOutputBuffer)
{

    /*
     *  Allocate Input bitstream buffer
     */
    if (aAllocateInputBuffer)
    {
        iInputBuf = OSCL_ARRAY_NEW(uint8, KAMRWB_NB_BYTES_MAX);
        if (iInputBuf == NULL)
        {
            return KCAI_CODEC_INIT_FAILURE;
        }
    }
    else
    {
        iInputBuf = NULL;
    }
    pExt->pInputBuffer = iInputBuf;

    iInputSampleBuf = OSCL_ARRAY_NEW(int16, KAMRWB_NB_BITS_MAX);
    if (iInputSampleBuf == NULL)
    {
        return KCAI_CODEC_INIT_FAILURE;
    }
    pExt->pInputSampleBuffer = iInputSampleBuf;

    /*
     *  Allocate Output PCM buffer
     */
    if (aAllocateOutputBuffer)
    {
        iOutputBuf = OSCL_ARRAY_NEW(int16, AMR_WB_PCM_FRAME);

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

    pExt->samplingRate = 16000;
    pExt->desiredChannels = 1;

    pExt->reset_flag = 0;
    pExt->reset_flag_old = 1;
    pExt->mode_old = 0;
    pExt->rx_state.prev_ft = RX_SPEECH_GOOD;
    pExt->rx_state.prev_mode = 0;


    int32 memreq = pvDecoder_AmrWbMemRequirements();

    pt_st = OSCL_ARRAY_NEW(uint8, memreq);

    if (pt_st == 0)
    {
        return(KCAI_CODEC_INIT_FAILURE);
    }

    pvDecoder_AmrWb_Init(&st, pt_st, &ScratchMem);

    return 0;
}


/*
-----------------------------------------------------------------------------

    CDecoder_AMR_WB

    ExecuteL

    Execute decoder object. Read one encoded speech frame from the input
    stream,  decode it and write the decoded frame to output stream.

    Parameters:

    Return Values:  status


-----------------------------------------------------------------------------
*/


OSCL_EXPORT_REF int32 CDecoder_AMR_WB::ExecuteL(tPVAmrDecoderExternal * pExt)
{

    if (pExt->input_format == MIME_IETF)  /* MIME/storage file format */
    {
        mime_unsorting(pExt->pInputBuffer,
                       pExt->pInputSampleBuffer,
                       &pExt->frame_type,
                       &pExt->mode,
                       pExt->quality,
                       &pExt->rx_state);
    }


    if ((pExt->frame_type == RX_NO_DATA) | (pExt->frame_type == RX_SPEECH_LOST))
    {
        pExt->mode = pExt->mode_old;
        pExt->reset_flag = 0;
    }
    else
    {
        pExt->mode_old = pExt->mode;

        /* if homed: check if this frame is another homing frame */
        if (pExt->reset_flag_old == 1)
        {
            /* only check until end of first subframe */
            pExt->reset_flag = pvDecoder_AmrWb_homing_frame_test_first(pExt->pInputSampleBuffer,
                               pExt->mode);
        }
    }

    /* produce encoder homing frame if homed & input=decoder homing frame */
    if ((pExt->reset_flag != 0) && (pExt->reset_flag_old != 0))
    {
        /* set homing sequence ( no need to decode anything */

        for (int16 i = 0; i < AMR_WB_PCM_FRAME; i++)
        {
            pExt->pOutputBuffer[i] = EHF_MASK;
        }
    }
    else
    {
        pExt->status = pvDecoder_AmrWb(pExt->mode,
                                       pExt->pInputSampleBuffer,
                                       pExt->pOutputBuffer,
                                       &pExt->frameLength,
                                       st,
                                       pExt->frame_type,
                                       ScratchMem);
    }

    for (int16 i = 0; i < AMR_WB_PCM_FRAME; i++)   /* Delete the 2 LSBs (14-bit output) */
    {
        pExt->pOutputBuffer[i] &= 0xfffC;
    }


    /* if not homed: check whether current frame is a homing frame */
    if (pExt->reset_flag_old == 0)
    {
        /* check whole frame */
        pExt->reset_flag = pvDecoder_AmrWb_homing_frame_test(pExt->pInputSampleBuffer,
                           pExt->mode);
    }
    /* reset decoder if current frame is a homing frame */
    if (pExt->reset_flag != 0)
    {
        pvDecoder_AmrWb_Reset(st, 1);;
    }
    pExt->reset_flag_old = pExt->reset_flag;

    return pExt->status;

}

/*
-----------------------------------------------------------------------------

    CDecoder_AMR_WB

    StopL

    Stop decoder object.

    Parameters:     none

    Return Values:  none

-----------------------------------------------------------------------------
*/
OSCL_EXPORT_REF void CDecoder_AMR_WB::StopL()
{
}

/*
-----------------------------------------------------------------------------

    CDecoder_AMR_WB

    ResetDecoderL

    Stop decoder object. Reset decoder.

    Parameters:     none

    Return Values:  status

-----------------------------------------------------------------------------
*/
OSCL_EXPORT_REF int32 CDecoder_AMR_WB::ResetDecoderL()
{
    pvDecoder_AmrWb_Reset(st, 1);
    return 0;
}


/*
-----------------------------------------------------------------------------

    CDecoder_AMR_WB

    TerminateDecoderL

    Stop decoder object. close decoder.

    Parameters:     none

    Return Values:  none

-----------------------------------------------------------------------------
*/
OSCL_EXPORT_REF void CDecoder_AMR_WB::TerminateDecoderL()
{
    st = NULL;
    ScratchMem = NULL;

    if (pt_st != NULL)
    {
        OSCL_ARRAY_DELETE((uint8*)pt_st);
        pt_st = NULL;
    }

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

    if (iInputSampleBuf != NULL)
    {
        OSCL_ARRAY_DELETE(iInputSampleBuf);
        iInputSampleBuf = NULL;
    }

}

