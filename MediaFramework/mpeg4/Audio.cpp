/******************************************************************************
 * Copyright (c) 2016, Chen Fang <mtdcy.chen@gmail.com>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without 
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice, 
 *    this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation 
 *    and/or other materials provided with the distribution.
 * 
 *  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 *  AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE 
 *  IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE 
 *  ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE 
 *  LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR 
 *  CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF 
 *  SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS 
 *  INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN 
 *  CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) 
 *  ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE 
 *  POSSIBILITY OF SUCH DAMAGE.
 ******************************************************************************/


// File:    Systems.cpp
// Author:  mtdcy.chen
// Changes: 
//          1. 20160701     initial version
//

#define LOG_TAG "MPEG4.Audio"
//#define LOG_NDEBUG 0 
#include "MediaDefs.h"

#include "Audio.h" 
#include "MediaDefs.h"

__BEGIN_NAMESPACE_MPX

namespace MPEG4 {

    // samplingFrequencyIndex 
    const uint32_t kM4ASamplingRate[16] = {
        96000,  88200,  64000,  48000,
        44100,  32000,  24000,  22050,
        16000,  12000,  11025,  8000,
        7350,   0,      0,      0/*escape value*/
    };
    const uint8_t kM4AChannels[8] = {
        0,      1,      2,      3, 
        4,      5,      5+1,    7+1
    };

    static uint8_t objectType(const BitReader& br) {
        uint8_t aot = br.read(5);
        DEBUG("aot %" PRIu8, aot);
        if (aot == 31) {
            aot = 32 + br.read(6);
            DEBUG("aot %" PRIu8, aot);
        }
        return aot;
    }

    static uint32_t samplingRate(const BitReader& br) {
        uint8_t samplingFrequencyIndex = br.read(4);
        DEBUG("samplingFrequencyIndex %" PRIu8, samplingFrequencyIndex);
        if (samplingFrequencyIndex == 15)
            return br.rb24();
        else 
            return kM4ASamplingRate[samplingFrequencyIndex];
    }

    // AAC has two kind header format, StreamMuxConfig & AudioSpecificConfig
    // oooo offf fccc cdef
    // 5 bits: object type
    // if (object type == 31)
    //     6 bits + 32: object type
    // 4 bits: frequency index
    // if (frequency index == 15)
    //     24 bits: frequency
    // 4 bits: channel configuration
    // var bits: AOT Specific Config
    AudioSpecificConfig::AudioSpecificConfig(const BitReader& br) {
        CHECK_GE(br.numBitsLeft(), 2 * 8); // 2 bytes at least
        audioObjectType         = objectType(br);
        samplingFrequency       = samplingRate(br);
        uint8_t channelConfig   = br.read(4);
        channels                = kM4AChannels[channelConfig];
        INFO("AOT %" PRIu8 ", samplingFrequency %" PRIu32 ", channels %" PRIu8,
                audioObjectType, samplingFrequency, channels);

        // AOT Specific Config
        // AOT_SBR
        if (audioObjectType == AOT_SBR) {
            sbr                 = true;
            extAudioObjectType  = audioObjectType;
            extSamplingFrquency = samplingRate(br);
            audioObjectType     = objectType(br);
        } else {
            sbr                 = false;
        }

        // GASpecificConfig
        if (audioObjectType == AOT_AAC_MAIN ||
                audioObjectType == AOT_AAC_LC ||
                audioObjectType == AOT_AAC_SSR ||
                audioObjectType == AOT_AAC_LTP ||
                audioObjectType == AOT_AAC_SCALABLE) {
            frameLength         = br.read(1) ? 960 : 1024;      // frameLengthFlag
            coreCoderDelay      = br.read(1) ? br.read(14) : 0; // dependsOnCoreCoder
            bool extensionFlag  = br.read(1);                   // extensionFlag;
            DEBUG("frameLength %" PRIu16 ", coreCoderDelay %" PRIu16,
                    frameLength, coreCoderDelay);

            if (channelConfig == 0) {                           // 
                FATAL("TODO: channel config == 0");
            }

            if (audioObjectType == AOT_AAC_SCALABLE) {          // layerNr
                br.skip(3);
            }
        }

        // HE-AAC extension
        if (br.numBitsLeft() > 15 && br.show(11) == 0x2b7) {    // syncExtensionType
            DEBUG("syncExtensionType");
            br.skip(11);
            extAudioObjectType      = objectType(br);
            if (extAudioObjectType == AOT_SBR) {
                sbr                 = br.read(1);
                extSamplingFrquency = samplingRate(br);
                if (extSamplingFrquency == samplingFrequency) {
                    sbr             = false;
                }
                DEBUG("sbr %d, extAudioObjectType %" PRIu16 ", extSamplingFrquency %" PRIu32,
                        sbr, extAudioObjectType, extSamplingFrquency);
            }
            //if (br.numBitsLeft() > 11 && br.read(11) == 0x548) {
            //    ps              = br.read(1);                   // ps
            //}
        }

        valid = true;
    }

    ES_Descriptor MakeESDescriptor(AudioSpecificConfig& asc) {
        ES_Descriptor esd;
        esd.decConfigDescr.objectTypeIndication = ISO_IEC_14496_3;
        esd.decConfigDescr.streamType = AudioStream;
        switch (asc.audioObjectType) {
            case AOT_AAC_MAIN:
                esd.decConfigDescr.objectTypeIndication = ISO_IEC_13818_7_Main;
                break;
            case AOT_AAC_LC:
                esd.decConfigDescr.objectTypeIndication = ISO_IEC_13818_7_LC;
                break;
            case AOT_AAC_SSR:
                esd.decConfigDescr.objectTypeIndication = ISO_IEC_13818_7_SSR;
                break;
            case AOT_MPEG_L2:
                esd.decConfigDescr.objectTypeIndication = ISO_IEC_11172_3;
                break;
            case AOT_MPEG_L3:
                esd.decConfigDescr.objectTypeIndication = ISO_IEC_13818_3;
                break;
            default:
                break;
        }
        return esd;
    }
}
__END_NAMESPACE_MPX
