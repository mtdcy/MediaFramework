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
#include "MediaDefs.h"

#include "Systems.h"
#include "MediaDefs.h"

__BEGIN_NAMESPACE_MPX
__BEGIN_NAMESPACE(MPEG4)

eCodecFormat translateObjectTypeIndication(uint8_t objectTypeIndication) {
    
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

// ISO/IEC 14496-1:2010 Section 8.3.3 Page 117
static FORCE_INLINE size_t GetObjectDescriptorSize(const BitReader& br) {
    bool nextBytes  = br.read(1);
    size_t size     = br.read(7);
    while (nextBytes) {
        nextBytes   = br.read(1);
        size        = (size << 7) | br.read(7);
    }
    return size;
}

static FORCE_INLINE size_t GetObjectDescriptorSizeLength(size_t size) {
    size_t length = 1;
    while (size > 127) {
        size >>= 7;
        ++length;
    }
    CHECK_LE(length, 4);    // logical check
    return length;
}

static FORCE_INLINE size_t WriteObjectDescriptorSize(BitWriter& bw, size_t size) {
    size_t length = GetObjectDescriptorSizeLength(size);
    for (size_t i = 0; i < length - 1; ++i) {
        uint8_t u8 = (size >> (7 * (length - i - 1))) & 0x7F;
        u8 |= 0x80;
        bw.w8(u8);
    }
    bw.w8(size & 0x7F);
    return length;
}

DecoderSpecificInfo::DecoderSpecificInfo() : BaseDescriptor(DecSpecificInfoTag),
csd(NULL)
{
    valid = true;
}

DecoderSpecificInfo::DecoderSpecificInfo(const BitReader& br) :
BaseDescriptor(DecSpecificInfoTag)
{
    if (br.r8() != DecSpecificInfoTag) {
        ERROR("bad DecoderSpecificInfo");
        return;
    }
    
    uint32_t descrLength    = GetObjectDescriptorSize(br);
    DEBUG("Tag 0x%" PRIx8 ", Length %" PRIu32,
          descrTag, descrLength);
    if (descrTag != DecSpecificInfoTag) return;
    
    csd = br.readB(descrLength);
    DEBUG("csd %s", csd->string(true).c_str());
    valid   = true;     // always
}

status_t DecoderSpecificInfo::compose(BitWriter &bw) const {
    bw.w8(DecSpecificInfoTag);
    WriteObjectDescriptorSize(bw, size());
    if (csd != NULL) bw.writeB(*csd);
    return OK;
}

size_t DecoderSpecificInfo::size() const {
    return valid ? (csd != NULL ? csd->size() : 0) : 0;
}

DecoderConfigDescriptor::DecoderConfigDescriptor() : BaseDescriptor(DecoderConfigDescrTag),
objectTypeIndication(ISO_IEC_14496_2),  // MPEG4VIDEO
streamType(0), upStream(0), bufferSizeDB(0), maxBitrate(0), avgBitrate(0)
{
    valid = true;
}

DecoderConfigDescriptor::DecoderConfigDescriptor(const BitReader& br) :
BaseDescriptor(DecoderConfigDescrTag)
{
    if (br.r8() != DecoderConfigDescrTag) {
        ERROR("bad DecoderConfigDescriptor");
        return;
    }
    
    uint32_t descrLength    = GetObjectDescriptorSize(br);
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
        if (br.show(8) == DecSpecificInfoTag) {
            decSpecificInfo = DecoderSpecificInfo(br);
            valid           = decSpecificInfo.valid;
        } else {
            DEBUG("unkown tag 0x%" PRIx8, descrTag);
        }
    } else {
        valid               = true;
    }
}

status_t DecoderConfigDescriptor::compose(BitWriter &bw) const {
    bw.w8(DecoderConfigDescrTag);
    WriteObjectDescriptorSize(bw, size());
    bw.w8(objectTypeIndication);
    bw.write(streamType, 6);
    bw.write(upStream, 1);
    bw.write(0, 1);  // reserved
    bw.wb24(bufferSizeDB);
    bw.wb32(maxBitrate);
    bw.wb32(avgBitrate);
    
    if (decSpecificInfo.valid) {
        return decSpecificInfo.compose(bw);
    }
    return OK;
}

size_t DecoderConfigDescriptor::size() const {
    return 13 + (decSpecificInfo.valid
                 ? 1 + GetObjectDescriptorSizeLength(decSpecificInfo.size()) + decSpecificInfo.size()
                 : 0);
}

// set default values, useful for create esds
ES_Descriptor::ES_Descriptor() : BaseDescriptor(ES_DescrTag),
ES_ID(0), streamDependenceFlag(0), URL_Flag(0), OCRstreamFlag(0),
streamPriority(0), dependsOn_ES_ID(0), URLstring(), OCR_ES_Id(0)
{
    valid = true;
}

ES_Descriptor::ES_Descriptor(const BitReader& br) :
BaseDescriptor(ES_DescrTag)
{
    if (br.r8() != ES_DescrTag) {
        ERROR("bad ES_Descriptor");
        return;
    }
    uint32_t descrLength    = GetObjectDescriptorSize(br);
    DEBUG("Tag 0x%" PRIx8 ", Length %" PRIu32,
          descrTag, descrLength);
    
    if (descrLength * 8 > br.remains()) {
        ERROR("bad esd data");
        return;
    }
    
    // 2 + 1 = 3
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
          streamPriority, dependsOn_ES_ID,
          URLstring != String::Null ? URLstring.c_str() : "null",
          OCR_ES_Id);
    
    // mandatory DecoderConfigDescriptor
    if (br.show(8) != DecoderConfigDescrTag) {
        ERROR("missing DecoderConfigDescriptor");
        valid               = false;
    } else {
        decConfigDescr      = DecoderConfigDescriptor(br);
        valid               = decConfigDescr.valid;
    }
}

status_t ES_Descriptor::compose(BitWriter &bw) const {
    // ES_DescrTag
    bw.w8(ES_DescrTag);
    WriteObjectDescriptorSize(bw, size());
    bw.wb16(ES_ID);
    bw.write(streamDependenceFlag, 1);
    bw.write(URL_Flag, 1);
    bw.write(OCRstreamFlag, 1);
    bw.write(streamPriority, 5);
    if (streamDependenceFlag) bw.wb16(dependsOn_ES_ID);
    if (URL_Flag) {
        bw.w8(URLstring.size());
        bw.writeS(URLstring);
    }
    if (OCRstreamFlag) bw.wb16(OCR_ES_Id);
    
    if (decConfigDescr.valid) {
        return decConfigDescr.compose(bw);
    }
    return OK;
}

size_t ES_Descriptor::size() const {
    return 3
    + (streamDependenceFlag ? 2 : 0)
    + (URL_Flag ? URLstring.size() + 1 : 0)
    + (OCRstreamFlag ? 2 : 0)
    + (decConfigDescr.valid
       ? 1 + GetObjectDescriptorSizeLength(decConfigDescr.size()) + decConfigDescr.size()
       : 0);
}

sp<Buffer> MakeESDS(ES_Descriptor& esd) {
    if (!esd.valid || !esd.decConfigDescr.valid || !esd.decConfigDescr.decSpecificInfo.valid) {
        ERROR("bad ES_Descriptor");
        return NULL;
    }
    
    sp<Buffer> buffer = new Buffer(1 + GetObjectDescriptorSizeLength(esd.size()) + esd.size());
    BitWriter bw(*buffer);
    esd.compose(bw);
    bw.write();     // write to byte boundary
    buffer->step(bw.size());
    return buffer;
}

__END_NAMESPACE(MPEG4)
__END_NAMESPACE_MPX

