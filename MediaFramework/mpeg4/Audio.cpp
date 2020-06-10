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
#include "MediaTypes.h"

#include "Audio.h"
#include "MediaTypes.h"

__BEGIN_NAMESPACE_MPX
__BEGIN_NAMESPACE(MPEG4)

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

static FORCE_INLINE eAudioObjectType objectType(const BitReader& br) {
    eAudioObjectType aot = (eAudioObjectType)br.read(5);
    DEBUG("aot %" PRIu8, aot);
    if (aot == 31) {
        aot = (eAudioObjectType)(32 + br.read(6));
        DEBUG("aot %" PRIu8, aot);
    }
    return aot;
}

static FORCE_INLINE uint8_t samplingFrequencyIndex(uint32_t freq) {
    for (size_t i = 0; i < 16; ++i) {
        if (kM4ASamplingRate[i] == freq) return i;
    }
    return 15;  // ext value
}

static FORCE_INLINE uint8_t channelIndex(uint8_t channels) {
    for (size_t i = 0; i < 8; ++i) {
        if (kM4AChannels[i] == channels) return i;
    }
    return 0;   // invalid value
}

GASpecificConfig::GASpecificConfig() {
    frameLength     = 1024;
    coreCoderDelay  = 0;
    layerNr         = 0;
}

static void program_config_element(const BitReader& br) {
    uint8_t element_instance_tag    = br.read(4);
    uint8_t object_type             = br.read(2);
    uint8_t sampling_frequency_index = br.read(4);
    uint8_t num_front_channel_elements = br.read(4);
    uint8_t num_side_channel_elements = br.read(4);
    uint8_t num_back_channel_elements = br.read(4);
    uint8_t num_lfe_channel_elements = br.read(2);
    uint8_t num_assoc_data_elements = br.read(3);
    uint8_t num_valid_cc_elements = br.read(4);
    
    uint8_t mono_mixdown_present = br.read(1);
    if (mono_mixdown_present) {
        uint8_t mono_mixdown_element_number = br.read(4);
    }
    uint8_t stereo_mixdown_present = br.read(1);
    if (stereo_mixdown_present) {
        uint8_t stereo_mixdown_element_number = br.read(4);
    }
    uint8_t matrix_mixdown_idx_present = br.read(1);
    if (matrix_mixdown_idx_present) {
        uint8_t matrix_mixdown_idx = br.read(2);
        uint8_t pseudo_surround_enable = br.read(1);
    }
    
    if (num_front_channel_elements > 0) br.skip(num_front_channel_elements * 5);
    if (num_side_channel_elements > 0) br.skip(num_side_channel_elements * 5);
    if (num_back_channel_elements > 0) br.skip(num_back_channel_elements * 5);
    if (num_lfe_channel_elements > 0) br.skip(num_lfe_channel_elements * 4);
    if (num_assoc_data_elements > 0) br.skip(num_assoc_data_elements * 4);
    if (num_valid_cc_elements > 0) br.skip(num_valid_cc_elements * 5);
    br.skip();
    uint8_t comment_field_bytes = br.read(8);
    if (comment_field_bytes > 0) br.skip(comment_field_bytes * 8);
}

static GASpecificConfig GASpecificConfigRead(const BitReader& br, eAudioObjectType audioObjectType,
                                 uint8_t samplingFrequencyIndex,
                                 uint8_t channelConfiguration) {
    GASpecificConfig ga;
    ga.frameLength          = br.read(1) ? 960 : 1024;
    ga.coreCoderDelay       = br.read(1) ? br.read(14) : 0;
    if (!channelConfiguration) {
        program_config_element(br);
    }
    uint8_t extensionFlag   = br.read(1);
    if (audioObjectType == 6 || audioObjectType == 20) {
        ga.layerNr = br.read(3);
    }
    if (extensionFlag) {
        if (audioObjectType == 22) {
            ga.numOfSubFrame    = br.read(5);
            ga.layerLength      = br.read(11);
        }
        if (audioObjectType == 17 || audioObjectType == 19 || audioObjectType == 20 || audioObjectType == 23) {
            ga.aacSectionDataResilienceFlag     = br.read(1);
            ga.aacScalefactorDataResilienceFlag = br.read(1);
            ga.aacSpectralDataResilienceFlag    = br.read(1);
        }
        uint8_t extensionFlag3  = br.read(1);   // version 3;
        if (extensionFlag3) {
            FATAL("GASpecificConfig version3");
        }
    }
    return ga;
}

AudioSpecificConfig::AudioSpecificConfig(eAudioObjectType aot, uint32_t freq, uint8_t chn) {
    audioObjectType     = aot;
    samplingFrequency   = freq;
    channels            = chn;
    sbr                 = false;
    extAudioObjectType  = AOT_NULL;
    extSamplingFrquency = 0;
    valid               = true;
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
    CHECK_GE(br.remains(), 2 * 8); // 2 bytes at least
    audioObjectType         = objectType(br);
    
    uint8_t samplingFrequencyIndex = br.read(4);
    if (samplingFrequencyIndex == 0xf)
        samplingFrequency   = br.rb24();
    else
        samplingFrequency   = kM4ASamplingRate[samplingFrequencyIndex];
    
    uint8_t channelConfiguration = br.read(4);
    channels                = kM4AChannels[channelConfiguration];
    INFO("AOT %" PRIu8 ", samplingFrequency %" PRIu32 ", channels %" PRIu8,
         audioObjectType, samplingFrequency, channels);
    
    // AOT Specific Config
    // AOT_SBR
    bool psPresentFlag = false;
    if (audioObjectType == AOT_SBR || audioObjectType == 29) {
        sbr                 = true;
        extAudioObjectType  = AOT_SBR;
        psPresentFlag       = audioObjectType == 29;
        
        uint8_t extSamplingFrequencyIndex = br.read(4);
        if (extSamplingFrequencyIndex == 0xf)
            extSamplingFrquency = br.rb24();
        else
            extSamplingFrquency = kM4ASamplingRate[extSamplingFrequencyIndex];
        audioObjectType     = objectType(br);
        if (audioObjectType == 22) {
            uint8_t extChannelConfiguration = br.read(4);
        }
    } else {
        sbr                 = false;
        extAudioObjectType  = AOT_NULL;
    }
    
    switch (audioObjectType) {
        case 1:     // AOT_AAC_MAIN
        case 2:     // AOT_AAC_LC
        case 3:     // AOT_AAC_SSR
        case 4:     // AOT_AAC_LTP
        case 6:     // AOT_AAC_SCALABLE
        case 7:
        case 17:
        case 19:
        case 20:
        case 21:
        case 22:
        case 23:
            gasc = GASpecificConfigRead(br, audioObjectType, samplingFrequencyIndex, channelConfiguration);
            break;
        default:
            break;
    }
    
    // HE-AAC extension
    if (br.remains() > 15 && br.show(11) == 0x2b7) {    // syncExtensionType
        DEBUG("syncExtensionType");
        br.skip(11);
        extAudioObjectType      = objectType(br);
        if (extAudioObjectType == AOT_SBR) {
            sbr                 = br.read(1);
            
            uint8_t extSamplingFrequencyIndex = br.read(4);
            if (extSamplingFrequencyIndex == 0xf)
                extSamplingFrquency = br.rb24();
            else
                extSamplingFrquency = kM4ASamplingRate[extSamplingFrequencyIndex];

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
// ==> max bytes: 5 + 6 + 4 + 24 + 4 => 6 bytes
static sp<Buffer> MakeCSD(const AudioSpecificConfig& asc) {
    sp<Buffer> csd = new Buffer(32);    // 32 bytes
    BitWriter bw(csd->data(), csd->capacity());
    if (asc.audioObjectType >= 31) {
        bw.write(31, 5);
        bw.write(asc.audioObjectType - 32, 6);
    } else {
        bw.write(asc.audioObjectType, 5);
    }
    uint8_t samplingIndex = samplingFrequencyIndex(asc.samplingFrequency);
    bw.write(samplingIndex, 4);
    if (samplingIndex == 15) {
        bw.wb24(asc.samplingFrequency);
    }
    uint8_t channelConfig = channelIndex(asc.channels);
    CHECK_NE(channelConfig, 0);
    bw.write(channelConfig, 4);
    
    // TODO
    if (asc.sbr) {
        if (asc.extAudioObjectType >= 31) {
            bw.write(31, 5);
            bw.write(asc.extAudioObjectType - 32, 6);
        } else {
            bw.write(asc.audioObjectType, 5);
        }
        
    }
    
    bw.write();
    csd->step(bw.size());
    return csd;
}

sp<Buffer> MakeAudioESDS(const AudioSpecificConfig& asc) {
    CHECK_TRUE(asc.valid);
    sp<ESDescriptor> esd = new ESDescriptor(ISO_IEC_14496_3);
    esd->decConfigDescr->decSpecificInfo = new DecoderSpecificInfo(MakeCSD(asc));
    return MPEG4::MakeESDS(esd);
}

sp<Buffer> MakeAudioESDS(const char * csd, size_t length) {
    // AudioSpecificConfig
    BitReader br(csd, length);
    MPEG4::AudioSpecificConfig asc(br);
    if (asc.valid) {
        sp<ESDescriptor> esd = new ESDescriptor(ISO_IEC_14496_3);
        esd->decConfigDescr->decSpecificInfo = new DecoderSpecificInfo(new Buffer(csd, length));
        return MPEG4::MakeESDS(esd);
    } else {
        ERROR("bad AudioSpecificConfig");
        return NIL;
    }
}

__END_NAMESPACE(MPEG4)
__END_NAMESPACE_MPX

