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
#ifndef GSMAMR_ENC_H_INCLUDED
#include "gsmamr_enc.h"
#endif

#ifndef GSMAMR_ENCODER_WRAPPER_H_INCLUDED
#include "gsmamr_encoder_wrapper.h"
#endif

#ifndef OSCL_DLL_H_INCLUDED
#include "oscl_dll.h"
#endif

// Define entry point for this DLL
OSCL_DLL_ENTRY_POINT_DEFAULT()

//----------------------------------------------------------------------------
// CONSTANTS
//----------------------------------------------------------------------------

// Number of samples per frame
#define KGAMR_NUM_SAMPLES_PER_FRAME   160

// Default mode
#define KDFLT_GAMR_MODE               MR475

// Default bits per sample for input audio
#define KDFLT_GAMR_BITS_PER_SAMPLE    16

// Default sampling rate for input audio (in Hz)
#define KDFLT_GAMR_SAMPLING_RATE      8000

// Default input clock rate for input audio (in ticks/sec)
#define KDFLT_GAMR_CLOCK_RATE         8000

// Default number of channels
#define KDFLT_GAMR_NUM_CHANNELS       1

// length of uncompressed audio frame in bytes
// Formula: (num_samples_per_frame * bits_per_sample) / num_bits_per_byte
#define PV_GSM_AMR_20_MSEC_SIZE       \
        ((KGAMR_NUM_SAMPLES_PER_FRAME * KDFLT_GAMR_BITS_PER_SAMPLE) / 8)


///////////////////////////////////////////////////////////////////////////////
OSCL_EXPORT_REF
CPvGsmAmrEncoder::CPvGsmAmrEncoder()
{
    // initialize member data to default values
    iEncState = NULL;
    iSidState = NULL;
    iGsmAmrMode = (GSM_AMR_MODES)KDFLT_GAMR_MODE;
    iLastModeUsed = 0;
    iBitStreamFormat = AMR_TX_WMF;
    iNumSamplesPerFrame = KGAMR_NUM_SAMPLES_PER_FRAME;
}
///////////////////////////////////////////////////////////////////////////////
OSCL_EXPORT_REF CPvGsmAmrEncoder::~CPvGsmAmrEncoder()
{
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////
OSCL_EXPORT_REF int32 CPvGsmAmrEncoder::InitializeEncoder(int32 aMaxOutputBufferSize, TEncodeProperties* aProps)
{
    if (aProps == NULL)
    {
        // use default parameters
        TEncodeProperties dfltProps;
        aProps = &dfltProps;
        dfltProps.iInBitsPerSample = KDFLT_GAMR_BITS_PER_SAMPLE;
        dfltProps.iInSamplingRate = KDFLT_GAMR_SAMPLING_RATE;
        dfltProps.iInClockRate = dfltProps.iInSamplingRate;
        dfltProps.iInNumChannels = KDFLT_GAMR_NUM_CHANNELS;
        iGsmAmrMode = (GSM_AMR_MODES)KDFLT_GAMR_MODE;
        iBitStreamFormat = AMR_TX_WMF;
    }
    else
    {
        // check first if input parameters are valid
        if ((IsModeValid(aProps->iMode) == false) ||
                (aProps->iInBitsPerSample == 0) ||
                (aProps->iInClockRate == 0) ||
                (aProps->iInSamplingRate == 0) ||
                (aProps->iInNumChannels == 0))
        {
            return GSMAMR_ENC_INVALID_PARAM;
        }
        // set AMR mode (bits per second)
        iGsmAmrMode = (GSM_AMR_MODES)aProps->iMode;
        if (aProps->iBitStreamFormat == AMR_TX_WMF)
        {
            iBitStreamFormat = AMR_TX_WMF;
        }
        else if (aProps->iBitStreamFormat == AMR_TX_IF2)
        {
            iBitStreamFormat = AMR_TX_IF2;
        }
        else if (aProps->iBitStreamFormat == AMR_TX_IETF)
        {
            iBitStreamFormat = AMR_TX_IETF;
        }
        else
        {
            iBitStreamFormat = AMR_TX_ETS;
        }
    }

    iBytesPerSample = aProps->iInBitsPerSample / 8;

    // set maximum buffer size for encoded data
    iMaxOutputBufferSize = aMaxOutputBufferSize;
    // return output parameters that will be used
    aProps->iOutSamplingRate = KDFLT_GAMR_SAMPLING_RATE;
    aProps->iOutNumChannels = KDFLT_GAMR_NUM_CHANNELS;
    aProps->iOutClockRate = aProps->iOutSamplingRate;

    // initialize AMR encoder
    int32 nResult = AMREncodeInit(&iEncState, &iSidState, false);
    if (nResult < 0) return(GSMAMR_ENC_CODEC_INIT_FAILURE);

    return GSMAMR_ENC_NO_ERROR;
}


////////////////////////////////////////////////////////////////////////////
OSCL_EXPORT_REF int32 CPvGsmAmrEncoder::Encode(TInputAudioStream& aInStream,
        TOutputAudioStream& aOutStream)
{
    // check first if mode specified is invalid
    if (IsModeValid(aInStream.iMode) == false)
        return GSMAMR_ENC_INVALID_MODE;

    // set AMR mode for this set of samples
    iGsmAmrMode = (GSM_AMR_MODES)aInStream.iMode;

    // get the maximum number of frames // BX
    int32 bytesPerFrame = iNumSamplesPerFrame * iBytesPerSample;
    int32 maxNumFrames = aInStream.iSampleLength / bytesPerFrame;
    uint8 *pFrameIn  = aInStream.iSampleBuffer;
    uint8 *pFrameOut = aOutStream.iBitStreamBuffer;
    int32 i;

    // encode samples
    for (i = 0; i < maxNumFrames; i++)
    {

        // //////////////////////////////////////////
        // encode this frame
        // //////////////////////////////////////////
        int32 * temp = & iLastModeUsed;
        Word16 nStatus = AMREncode(iEncState, iSidState,    // BX, Word16 instead of int32 to avoid wierd case(IF2 format): the function returns 31, but nStatus ends up with a big wierd number
                                   (Mode)iGsmAmrMode,
                                   (Word16 *)pFrameIn,
                                   (unsigned char *)pFrameOut,
                                   (Frame_Type_3GPP*) temp,
                                   iBitStreamFormat);

        if (nStatus < 0)
        {
            // an error when encoding was received, so quit
            return(GSMAMR_ENC_CODEC_ENCODE_FAILURE);
        }

        // save nStatus as this indicates the encoded frame size
        int32 encFrameSize = (int32)nStatus;
        aOutStream.iSampleFrameSize[i] = encFrameSize;
        pFrameOut += encFrameSize;
        pFrameIn  += bytesPerFrame;
    }

    // set other values to be returned
    aOutStream.iNumSampleFrames = maxNumFrames;
    return(GSMAMR_ENC_NO_ERROR);
}

////////////////////////////////////////////////////////////////////////////
OSCL_EXPORT_REF int32 CPvGsmAmrEncoder::CleanupEncoder()
{
    // call terminate function of GSM AMR encoder
    AMREncodeExit(&iEncState, &iSidState);

    iEncState = NULL;
    iSidState = NULL;

    return GSMAMR_ENC_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////
OSCL_EXPORT_REF int32 CPvGsmAmrEncoder::Reset()
{
    // reset GSM AMR encoder (state memory and SID sync function.)
    Word16 nStatus = AMREncodeReset(&iEncState, &iSidState);

    if (nStatus < 0)
    {
        return GSMAMR_ENC_CODEC_ENCODE_FAILURE;
    }
    return GSMAMR_ENC_NO_ERROR;
}


