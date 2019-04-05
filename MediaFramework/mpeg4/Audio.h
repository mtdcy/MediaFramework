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


// File:    Audio.h
// Author:  mtdcy.chen
// Changes: 
//          1. 20181126     initial version
//

#ifndef _MEDIA_MODULES_MPEG4_AUDIO_H
#define _MEDIA_MODULES_MPEG4_AUDIO_H

#include "MediaDefs.h"
#include "Systems.h"

__BEGIN_NAMESPACE_MPX

// ISO/IEC 14496-3 Audio
namespace MPEG4 {

    // ISO/IEC 14496-3:2001
    struct AudioSpecificConfig {
        AudioSpecificConfig(const BitReader& br);
        bool                    valid;
        uint8_t                 audioObjectType;
        uint32_t                samplingFrequency;
        uint8_t                 channels;
        // AOT Specific Config
        // AOT_SBR
        bool                    sbr;
        uint8_t                 extAudioObjectType;
        uint32_t                extSamplingFrquency;
        // AOT_AAC_*
        // GASpecificConfig
        uint16_t                frameLength;
        uint16_t                coreCoderDelay;
    };

    // MPEG-4 Audio Object Types
    enum {
        AOT_NULL,
        AOT_AAC_MAIN            = 1,
        AOT_AAC_LC              = 2,
        AOT_AAC_SSR             = 3,
        AOT_AAC_LTP             = 4,
        AOT_SBR                 = 5,
        AOT_AAC_SCALABLE        = 6,
        AOT_MPEG_L1             = 32,
        AOT_MPEG_L2             = 33,
        AOT_MPEG_L3             = 34,
    }; 

    static const char * kAOTNames[] = {
        "Unknown",
        "Main",
        "LC",
        "SSR",
        "LTP",
        "SBR",
        "Scalable"
    };

    ES_Descriptor MakeESDescriptor(AudioSpecificConfig& asc);

}

__END_NAMESPACE_MPX

#endif // 
