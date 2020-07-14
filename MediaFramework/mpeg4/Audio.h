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

#ifndef MFWK_MPEG4_AUDIO_H
#define MFWK_MPEG4_AUDIO_H

#include "MediaTypes.h"
#include "Systems.h"

__BEGIN_NAMESPACE_MFWK

// ISO/IEC 14496-3 Audio
namespace MPEG4 {
    
    // ISO/IEC 14496-3
    // Section 1.5.1MPEG-4 audio object types
    enum eAudioObjectType {
        AOT_NULL,
        AOT_AAC_MAIN            = 1,        ///< ISO/IEC 13818-7 Main & PNS
        AOT_AAC_LC              = 2,        ///< ISO/IEC 13818-7 LC & PNS
        AOT_AAC_SSR             = 3,        ///< ISO/IEC 13818-7 SSR & PNS
        AOT_AAC_LTP             = 4,        ///< ISO/IEC 13818-7 LC & PNS & LTP
        AOT_SBR                 = 5,        ///<
        AOT_AAC_SCALABLE        = 6,        ///< ISO/IEC 13818-7 LC
        AOT_MPEG_L1             = 32,
        AOT_MPEG_L2             = 33,
        AOT_MPEG_L3             = 34,
        AOT_MAX                 = 0xff
    };
    
    static const Char * kAOTNames[] = {
        "Unknown",
        "Main",
        "LC",
        "SSR",
        "LTP",
        "SBR",
        "Scalable"
    };
    
    struct GASpecificConfig {
        GASpecificConfig();
        
        UInt16      frameLength;    // 960 or 1024
        UInt16      coreCoderDelay; //
        UInt8       layerNr;
        // extensionFlag
        //
        UInt8       numOfSubFrame;
        UInt16      layerLength;
        //
        UInt8       aacSectionDataResilienceFlag;
        UInt8       aacScalefactorDataResilienceFlag;
        UInt8       aacSpectralDataResilienceFlag;
    };
    
    struct CelpSpecificConfig {
    };

    // ISO/IEC 14496-3:2001
    // Section 1.6 Interface to ISO/IEC 14496-1
    struct AudioSpecificConfig {
        AudioSpecificConfig(eAudioObjectType, UInt32 freq, UInt8 channels);
        AudioSpecificConfig(const sp<ABuffer>&);
        
        Bool                    valid;
        eAudioObjectType        audioObjectType;
        UInt32                  samplingFrequency;
        UInt8                   channels;
        // AOT Specific Config
        // AOT_SBR
        Bool                    sbr;
        eAudioObjectType        extAudioObjectType;
        UInt32                  extSamplingFrquency;
        UInt8                   extChannels;
        // 
        GASpecificConfig        gasc;
    };

    sp<Buffer> MakeAudioESDS(const AudioSpecificConfig&);
    
    sp<Buffer> MakeAudioESDS(const sp<ABuffer>&);
}

__END_NAMESPACE_MFWK

#endif // 
