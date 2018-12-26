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


#define LOG_TAG "MPEG4.Systems"
#define LOG_NDEBUG 0 
#include <MediaToolkit/Toolkit.h>

#include "Systems.h" 
#include "MediaDefs.h"

namespace mtdcy { namespace MPEG4 {

    int32_t translateObjectTypeIndication(uint8_t objectTypeIndication) {

        if (objectTypeIndication >= 0xc0 && objectTypeIndication <= 0xe0) 
            return kCodecFormatUnknown;

        switch (objectTypeIndication) {
            case ISO_IEC_14496_2             :        // MPEG4 visual
                return kVideoCodecFormatMPEG4;
            case ISO_IEC_14496_10            :        // H264
                return kVideoCodecFormatH264;
            case ISO_IEC_23008_2             :        // H265
                return kVideoCodecFormatHEVC;
            case ISO_IEC_14496_3             :        // MPEG4 sound / AAC
                return kAudioCodecFormatAAC;
            case ISO_IEC_13818_7_Main        :        // MPEG2 AAC Main 
            case ISO_IEC_13818_7_LC          :        // MPEG2 AAC LC
            case ISO_IEC_13818_7_SSR         :        // MPEG2 AAC SSR
                return kAudioCodecFormatAAC;
            case ISO_IEC_13818_3             :        // MP3 
                return kAudioCodecFormatMP3;
            default:
                ERROR("unknown objectTypeIndication 0x%" PRIx8, objectTypeIndication);
                //return Media::Codec::Unknown;
                FATAL("FIXME: missing objectTypeIndication");
                break;
        }
    }

    static inline size_t _GetObjectDescriptorSize(const BitReader& br) {
        bool nextBytes  = br.read(1);
        size_t size     = br.read(7);
        while (nextBytes) {
            nextBytes   = br.read(1);
            size        = (size << 7) | br.read(7);
        }
        return size;
    }
    size_t GetObjectDescriptorSize(const BitReader& br) {
        return _GetObjectDescriptorSize(br);
    }

    DecoderSpecificInfo::DecoderSpecificInfo(const BitReader& br) :
        BaseDescriptor(DecSpecificInfoTag)
    {
        uint32_t descrLength    = _GetObjectDescriptorSize(br);
        DEBUG("Tag 0x%" PRIx8 ", Length %" PRIu32, 
                descrTag, descrLength);
        if (descrTag != DecSpecificInfoTag) return;

        csd = br.readB(descrLength);
        DEBUG("csd %s", csd->string(true).c_str());
        valid   = true;     // always
    }

    DecoderConfigDescriptor::DecoderConfigDescriptor(const BitReader& br) :
        BaseDescriptor(DecoderConfigDescrTag)
    {
        uint32_t descrLength    = _GetObjectDescriptorSize(br);
        DEBUG("Tag 0x%" PRIx8 ", Length %" PRIu32, 
                descrTag, descrLength);
        if (descrTag != DecoderConfigDescrTag) return;

        // 2 + 3 + 4 + 4 = 13
        objectTypeIndication    = br.r8();
        streamType              = br.read(6);
        upStream                = br.read(1); 
        br.skip(1);    //reserved 
        bufferSizeDB            = br.rb24();
        maxBitrate              = br.rb32();
        avgBitrate              = br.rb32();

        DEBUG("objectTypeIndication 0x%" PRIx8 ", streamType 0x%" PRIx8 
                ", upStream %" PRIu8 ", bufferSizeDB %" PRIu32
                ", maxBitrate %" PRIu32 ", avgBitrate %" PRIu32,
                objectTypeIndication, streamType, upStream,
                bufferSizeDB, maxBitrate, avgBitrate);

        if (descrLength > 13) {
            uint32_t descrTag   = br.r8();
            // optional 
            if (descrTag == DecSpecificInfoTag) {
                decSpecificInfo = DecoderSpecificInfo(br);
                valid           = decSpecificInfo.valid;
            } else {
                DEBUG("unkown tag 0x%" PRIx8, descrTag);
            }
        } else {
            valid               = true;
        }
    }

    ES_Descriptor::ES_Descriptor(const BitReader& br) :
        BaseDescriptor(ES_DescrTag)
    {
        if (br.r8() != ES_DescrTag) {
            ERROR("bad esd data");
            return;
        }
        uint32_t descrLength    = _GetObjectDescriptorSize(br);
        DEBUG("Tag 0x%" PRIx8 ", Length %" PRIu32, 
                descrTag, descrLength);

        if (descrLength * 8 > br.numBitsLeft()) {
            ERROR("bad esd data");
            return;
        }

        ES_ID                   = br.rb16();
        streamDependenceFlag    = br.read(1);
        URL_Flag                = br.read(1);
        OCRstreamFlag           = br.read(1);
        streamPriority          = br.read(5);
        if (streamDependenceFlag) {
            dependsOn_ES_ID     = br.rb16();
        }
        if (URL_Flag) {
            uint8_t URLlength   = br.r8();
            URLstring           = br.readS(URLlength);
        }
        if (OCRstreamFlag) {
            OCR_ES_Id           = br.rb16();
        }

        DEBUG("ES_ID %" PRIu16 ", streamDependenceFlag %" PRIu8 
                ", URL_Flag %" PRIu8 ", OCRstreamFlag %" PRIu8
                ", streamPriority %" PRIu8 ", dependsOn_ES_ID %" PRIu16 
                ", URLstring %s, OCR_ES_Id %" PRIu16, 
                ES_ID, streamDependenceFlag, URL_Flag, OCRstreamFlag,
                streamPriority, dependsOn_ES_ID, URLstring.c_str(),
                OCR_ES_Id);

        // mandatory DecoderConfigDescriptor
        uint32_t descrTag       = br.r8();
        if (descrTag != DecoderConfigDescrTag) {
            ERROR("missing DecoderConfigDescriptor");
            valid               = false;
        } else {
            decConfigDescr      = DecoderConfigDescriptor(br);
            valid               = decConfigDescr.valid;
        }
    }
}; };
