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
#ifndef __GSMAMR_ENCODER_WRAPPER_H__
#define __GSMAMR_ENCODER_WRAPPER_H__

//----------------------------------------------------------------------------
// INCLUDES
//----------------------------------------------------------------------------
#include "oscl_base.h"

class TInputAudioStream
{
    public:
        // Pointer to buffer containing the audio samples.
        // The application is required to allocate this buffer.
        uint8* iSampleBuffer;

        // Number of samples in bytes contained in iSampleBuffer
        int32 iSampleLength;

        // Mode of operation (the same as bit rate)
        // For example, "GSM_AMR_4_75" (for 4.75 kbps) for GSM-AMR
        int32 iMode;

        // Start time when samples were taken
        uint64 iStartTime;

        // End time when samples were taken
        uint64 iStopTime;
};


class TOutputAudioStream
{
    public:
        // Pointer to buffer containing encoded audio samples
        uint8* iBitStreamBuffer;

        // Number of sample frames encoded and are contained in the buffer
        int32 iNumSampleFrames;

        // Size in bytes of each encoded sample frame;
        // This variable may point to an array if the sample frame sizes are
        // variable. For example:
        //    iSampleFrameSize[0] = 23   (frame size of frame #1)
        //    iSampleFrameSize[1] = 12   (frame size of frame #2)
        //    . . .
        //    iSampleFrameSize[iNumSampleFrames] = 10 (frame size of last frame)
        int32* iSampleFrameSize;

        // Start time of the encoded samples contained in the bit stream buffer
        uint32 iStartTime;

        // Stop time of encoded samples contained in the bit stream buffer
        uint32 iStopTime;
};

class TEncodeProperties
{
    public:
        // /////////////////////////////////////////////
        // Input stream properties (uncompressed audio)
        // /////////////////////////////////////////////

        enum EInterleaveMode
        {
            EINTERLEAVE_LR,        // interleaved left-right
            EGROUPED_LR,           // non-interleaved left-right
            ENUM_INTERLEAVE_MODES  // number of modes supported
        };

        // DESCRIPTION: Number of bits per sample. For example, set it to "16"
        //              bits for PCM.
        // USAGE:       The authoring application is required to fill this in.
        //              The CAEI uses the value for encoding.
        int32 iInBitsPerSample;

        // DESCRIPTION: Sampling rate of the input samples in Hz.
        //              For example, set it to "22050" Hz.
        // USAGE:       The authoring application is required to fill this in.
        //              If sampling rate is not known until CAEI is initialized,
        //              use '0'. The CAEI uses the value for encoding.
        uint32 iInSamplingRate;

        // DESCRIPTION: Clock rate or time scale to be used for the input timestamps
        //              (ticks per secs). For example, "22050" ticks/sec.
        // USAGE:       The authoring application is required to fill this in.
        //              If sampling rate is not known until CAEI is initialized,
        //              use '0'. The CAEI uses the value for encoding.
        uint32 iInClockRate;

        // DESCRIPTION: Number of input channels:1=Mono,2=Stereo.(Mono uses 1 channel;
        //              Stereo uses 2 channels).
        // USAGE:       The authoring application is required to fill this in.
        //              The CAEI uses the value for encoding.
        uint8 iInNumChannels;

        // DESCRIPTION: Whether to interleave or not the multi-channel input samples:
        //              EINTERLEAVE_LR  = LRLRLRLRLR    (interleaved left-right)
        //              EGROUPED_LR = LLLLLL...RRRRRR   (non-interleaved left-right)
        // USAGE:       The authoring application is required to fill this in.
        //              The CAEI uses the value for encoding.
        EInterleaveMode iInInterleaveMode;

        // DESCRIPTION: Desired Sampling rate for a given bitrate combination:
        //              For example, set it to "16000" Hz if the encoding 16kbps
        //              mono/stereo or 24 kbps stereo
        // USAGE:       The authoring application is required to fill this in.
        //              The CAEI uses the value for encoding.
        uint32 iDesiredSamplingRate;

    public:
        // ////////////////////////////////////////////
        // Output stream properties (compressed audio)
        // ////////////////////////////////////////////

        // DESCRIPTION: Mode of operation (the same as bit rate). For example,
        //              "GSM_AMR_4_75" (for 4.75 kbps).
        // USAGE:       The authoring application is required to fill this in.
        //              The CAEI uses the value to configure the codec library.
        int32 iMode;

        // DESCRIPTION: Bit order format:
        //              TRUE  = MSB..................LSB
        //                       d7 d6 d5 d4 d3 d2 d1 d0
        //              FALSE = MSB..................LSB
        //                      d0 d1 d2 d3 d4 d5 d6 d7
        // USAGE:       The authoring application is required to fill this in.
        //              The CAEI will use the value to setup the codec library.
        int32 iBitStreamFormat;

        // DESCRIPTION: Audio object type for the output bitstream; only applies
        //              to AAC codec
        // USAGE:       The application is required to fill this in.
        //              The CADI will use the value to setup the codec library.
        int32 iAudioObjectType;

        // DESCRIPTION: Final sampling frequency used when encoding in Hz.
        //              For example, "44100" Hz.
        // USAGE:       If the input sampling rate is not appropriate (e.g.,
        //              the codec requires a different sampling frequency),
        //              the CAEI will fill this in with the final sampling
        //              rate. The CAEI will perform resampling if the
        //              input sampling frequency is not the same as the output
        //              sampling frequency.
        uint32 iOutSamplingRate;

        // DESCRIPTION: Number of output channels:1=Mono,2=Stereo. (Mono uses 1
        //              channel; Stereo uses 2 channels).
        // USAGE:       The CAEI will fill this in if it needs to convert
        //              the input samples to what is required by the codec.
        uint8 iOutNumChannels;

        // DESCRIPTION: Clock rate or time scale used for the timestamps (ticks per secs)
        //              For example, "8000" ticks/sec.
        // USAGE:       The CAEI will fill this in if the input data will be
        //              resampled.
        uint32 iOutClockRate;
};



/**
**   This class encodes audio samples using the GSM-AMR algorithm.
**   This codec operates on a 20-msec frame duration corresponding to 160
**   samples at the sampling frequency of 8000 samples/sec. The size of a frame
**   is 320 bytes. For each 20-ms frame, a bit-rate of 4.75, 5.15, 5.90, 6.70,
**   7.40, 7.95, 10.2, or 12.2 kbits/sec can be produced.
**
**   Sample usage:
**   ------------
**       // create a GSM-AMR encoder object
**       CGsmAmrEncoder* myAppEncoder = OSCL_NEW(CGsmAmrEncoder, ());
**       // set input parameters
**       TEncodeProperties myProps;
**       myProps.iInSamplingRate = 8000;
**       myProps.iInBitsPerSample = 16;
**       myProps.iOutBitRate = CGsmAmrEncoder::GSM_AMR_12_2;
**       myAppEncoder->InitializeEncoder(myProps, 2000);
**
**       // encode a sample block
**       myAppEncoder->Encode(myInput, myOutput);
**
//       // done encoding so cleanup
**       myAppEncoder->CleanupEncoder();
**       OSCL_DELETE(myAppEncoder);
**
*/

class CPvGsmAmrEncoder
{
    public:
        //! Constructor -- creates a GSM-AMR encoder object
        OSCL_IMPORT_REF CPvGsmAmrEncoder();

        //! Destructor -- destroys the GSM-AMR encoder object
        OSCL_IMPORT_REF ~CPvGsmAmrEncoder();

        /**
        *  This function initializes the GSM-AMR encoder.
        * @param "aMaxOutputBufferSize" "the maximum buffer size for the output buffer when Encode() gets called"
        * @param "aProps" "TEncodeProperties based pointer for the input encoding setting. if aProps=NULL, then
        *        default settings will be set"
        * @return 0 for the correct operation, and -1 for the wrong operation
        */
        OSCL_IMPORT_REF int32 InitializeEncoder(int32 aMaxOutputBufferSize,
                                                TEncodeProperties* aProps = NULL);

        /**
        *  This function initializes the GSM-AMR encoder.
        * @param "aInStream" "TInputAudioStream based reference object that contains the input buffer and buffer size and timestamp info"
        * @param "aOutStream" "TOutputAudioStream based reference object that contains the output buffer for compressed data
        * @return 0 for the correct operation, and -1 for the wrong operation
        */
        OSCL_IMPORT_REF int32 Encode(TInputAudioStream& aInStream,
                                     TOutputAudioStream& aOutStream);

        /**
        *  This function cleans up the encoder workspace when done encoding.
        */
        OSCL_IMPORT_REF int32 CleanupEncoder();

        /**
        *  This function reset the encoder workspace.
        */
        OSCL_IMPORT_REF int32 Reset();


    public:
        // GSM AMR modes
        // ** values should be the same as the Mode enum specified by AMR library
        enum GSM_AMR_MODES
        {
            GSM_AMR_4_75,
            GSM_AMR_5_15,
            GSM_AMR_5_90,
            GSM_AMR_6_70,
            GSM_AMR_7_40,
            GSM_AMR_7_95,
            GSM_AMR_10_2,
            GSM_AMR_12_2,
            GSM_AMR_DTX,
            GSM_AMR_N_MODES      /* number of (SPC) modes */
        };

    private:

        /**
        *  This inline function checks whether the specified mode is valid or not.
        * @param "aMode" "input the current mode to be used in encoding"
        * @return true for the valid mode, and false for the wrong mode
        */
        inline bool IsModeValid(int32 aMode)
        {
            return((aMode < GSM_AMR_N_MODES) && (aMode >= 0));
        }

    private:

        // GSM AMR encoder state variables
        void* iEncState;
        void* iSidState;

        // contains the current mode of GSM AMR
        GSM_AMR_MODES iGsmAmrMode;

        // last mode used
        int32 iLastModeUsed;

        // number of samples per frame (granulity)
        int32 iNumSamplesPerFrame;
        // number of bytes per sample
        int32 iBytesPerSample;

        // maximum size allowed for output buffer
        int32 iMaxOutputBufferSize;

        // bit stream format
        int32 iBitStreamFormat;

};

typedef enum
{
    GSMAMR_ENC_NO_ERROR                 = 0,
    GSMAMR_ENC_NO_MEMORY_ERROR          = -1,
    GSMAMR_ENC_CODEC_INIT_FAILURE       = -2,
    GSMAMR_ENC_CODEC_NOT_INITIALIZED    = -3,
    GSMAMR_ENC_INVALID_PARAM            = -4,
    GSMAMR_ENC_INVALID_MODE             = -5,
    GSMAMR_ENC_CODEC_ENCODE_FAILURE     = -6,
    GSMAMR_ENC_MEMORY_OVERFLOW          = -7
} GSMAMR_ENC_STATUS;

#endif  // __GSMAMR_ENCODER_WRAPPER_H__


