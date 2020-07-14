/******************************************************************************
 * Copyright (c) 2020, Chen Fang <mtdcy.chen@gmail.com>
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
#include "MediaTypes.h"

#include "Systems.h"

__BEGIN_NAMESPACE_MFWK
__BEGIN_NAMESPACE(MPEG4)

UInt32 translateObjectTypeIndication(eObjectTypeIndication objectTypeIndication) {
    switch (objectTypeIndication) {
        case ISO_IEC_14496_2             :        // MPEG4 visual
            return kVideoCodecMPEG4;
        case ISO_IEC_14496_10            :        // H264
            return kVideoCodecH264;
        case ISO_IEC_23008_2             :        // H265
            return kVideoCodecHEVC;
        case ISO_IEC_14496_3             :        // MPEG4 sound / AAC
            return kAudioCodecAAC;
        case ISO_IEC_13818_7_Main        :        // MPEG2 AAC Main
        case ISO_IEC_13818_7_LC          :        // MPEG2 AAC LC
        case ISO_IEC_13818_7_SSR         :        // MPEG2 AAC SSR
            return kAudioCodecAAC;
        case ISO_IEC_13818_3             :        // MP3
            return kAudioCodecMP3;
        default:
            ERROR("unknown objectTypeIndication 0x%" PRIx8, objectTypeIndication);
            return kAudioCodecUnknown;
    }
}

eStreamType ObjectType2StreamType(eObjectTypeIndication objectTypeIndication) {
    switch (objectTypeIndication) {
        case ISO_IEC_14496_2             :        // MPEG4 visual
        case ISO_IEC_14496_10            :        // H264
        case ISO_IEC_23008_2             :        // H265
            return VisualStream;
        case ISO_IEC_14496_3             :        // MPEG4 sound / AAC
        case ISO_IEC_13818_7_Main        :        // MPEG2 AAC Main
        case ISO_IEC_13818_7_LC          :        // MPEG2 AAC LC
        case ISO_IEC_13818_7_SSR         :        // MPEG2 AAC SSR
        case ISO_IEC_13818_3             :        // MP3
            return AudioStream;
        default:
            FATAL("unknown objectTypeIndication 0x%" PRIx8, objectTypeIndication);
            return AudioStream;
    }
}

// ISO/IEC 14496-1:2010 Section 8.3.3 Page 117
static FORCE_INLINE UInt32 GetObjectDescriptorSize(const sp<ABuffer>& buffer) {
    Bool nextBytes  = buffer->read(1);
    UInt32 size     = buffer->read(7);
    while (nextBytes) {
        nextBytes   = buffer->read(1);
        size        = (size << 7) | buffer->read(7);
    }
    return size;
}

static FORCE_INLINE UInt32 GetObjectDescriptorSizeLength(UInt32 size) {
    UInt32 length = 1;
    while (size > 127) {
        size >>= 7;
        ++length;
    }
    CHECK_LE(length, 4);    // logical check
    return length;
}

static FORCE_INLINE UInt32 WriteObjectDescriptorSize(sp<ABuffer>& buffer, UInt32 size) {
    UInt32 length = GetObjectDescriptorSizeLength(size);
    for (UInt32 i = 0; i < length - 1; ++i) {
        UInt8 u8 = (size >> (7 * (length - i - 1))) & 0x7F;
        u8 |= 0x80;
        buffer->w8(u8);
    }
    buffer->w8(size & 0x7F);
    return length;
}

static UInt32 DecoderSpecificInfoLength(const sp<DecoderSpecificInfo>& dsi) {
    CHECK_FALSE(dsi.isNil());
    if (dsi->csd.isNil()) return 0; // invalid
    return dsi->csd->size();
}

// DecSpecificInfoTag
static sp<DecoderSpecificInfo> ReadDecoderSpecificInfo(const sp<ABuffer>& buffer) {
    sp<DecoderSpecificInfo> dsi = new DecoderSpecificInfo;
    UInt32 descrLength    = GetObjectDescriptorSize(buffer);
    DEBUG("DecSpecificInfoTag: Length %" PRIu32, descrLength);
    dsi->csd = buffer->readBytes(descrLength);
    DEBUG("csd %s", dsi->csd->string(True).c_str());
    return dsi;
}

static void WriteDecoderSpecificInfo(sp<ABuffer>& buffer, const sp<DecoderSpecificInfo>& dsi) {
    CHECK_FALSE(dsi->csd.isNil());
    buffer->w8(dsi->descrTag);    // DecSpecificInfoTag
    WriteObjectDescriptorSize(buffer, DecoderSpecificInfoLength(dsi));
    buffer->writeBytes(dsi->csd);
}

static FORCE_INLINE UInt32 FullLength(UInt32 length) {
    return 1 + GetObjectDescriptorSizeLength(length) + length;
}

static UInt32 DecoderConfigDescriptorLength(const sp<DecoderConfigDescriptor>& dcd) {
    CHECK_FALSE(dcd.isNil());
    UInt32 length = 13;
    if (dcd->decSpecificInfo.isNil()) return length;
    return length + FullLength(DecoderSpecificInfoLength(dcd->decSpecificInfo));
}

static sp<DecoderConfigDescriptor> ReadDecoderConfigDescriptor(const sp<ABuffer>& buffer) {
    sp<DecoderConfigDescriptor> dcd = new DecoderConfigDescriptor;
    UInt32 descrLength    = GetObjectDescriptorSize(buffer);
    DEBUG("DecoderConfigDescrTag: Length %" PRIu32, descrLength);
    
    // 2 + 3 + 4 + 4 = 13
    dcd->objectTypeIndication    = (eObjectTypeIndication)buffer->r8();
    dcd->streamType              = (eStreamType)buffer->read(6);
    dcd->upStream                = buffer->read(1);
    buffer->skip(1);    //reserved
    dcd->bufferSizeDB            = buffer->rb24();
    dcd->maxBitrate              = buffer->rb32();
    dcd->avgBitrate              = buffer->rb32();
    
    DEBUG("objectTypeIndication 0x%" PRIx8 ", streamType 0x%" PRIx8
          ", upStream %" PRIu8 ", bufferSizeDB %" PRIu32
          ", maxBitrate %" PRIu32 ", avgBitrate %" PRIu32,
          dcd->objectTypeIndication, dcd->streamType, dcd->upStream,
          dcd->bufferSizeDB, dcd->maxBitrate, dcd->avgBitrate);
    
    if (descrLength > 13) {
        UInt8 descrTag = buffer->r8();
        if (descrTag == DecSpecificInfoTag) {
            dcd->decSpecificInfo = ReadDecoderSpecificInfo(buffer);
        } else {
            DEBUG("unkown tag 0x%" PRIx8, descrTag);
        }
    }
    return dcd;
}

static void WriteDecoderConfigDescriptor(sp<ABuffer>& buffer, const sp<DecoderConfigDescriptor>& dcd) {
    buffer->w8(dcd->descrTag);                    // DecoderConfigDescrTag
    WriteObjectDescriptorSize(buffer, DecoderConfigDescriptorLength(dcd));
    buffer->w8(dcd->objectTypeIndication);
    buffer->write(dcd->streamType, 6);
    buffer->write(dcd->upStream, 1);
    buffer->write(0, 1);  // reserved
    buffer->wb24(dcd->bufferSizeDB);
    buffer->wb32(dcd->maxBitrate);
    buffer->wb32(dcd->avgBitrate);
    
    if (dcd->decSpecificInfo.isNil()) return;
    WriteDecoderSpecificInfo(buffer, dcd->decSpecificInfo);
}

static UInt32 ESDescriptorLength(const sp<ESDescriptor>& esd) {
    CHECK_FALSE(esd.isNil());
    
    UInt32 length =  3
    + (esd->streamDependenceFlag ? 2 : 0)
    + (esd->URL_Flag ? esd->URLstring.size() + 1 : 0)
    + (esd->OCRstreamFlag ? 2 : 0);
    
    if (esd->decConfigDescr.isNil()) return length;
    return length + FullLength(DecoderConfigDescriptorLength(esd->decConfigDescr));
}

static sp<ESDescriptor> ReadESDescriptor(const sp<ABuffer>& buffer) {
    if (buffer->r8() != ESDescrTag) {
        ERROR("bad ESDescriptor");
        return Nil;
    }
    
    sp<ESDescriptor> esd = new ESDescriptor;
    UInt32 descrLength    = GetObjectDescriptorSize(buffer);
    DEBUG("ESDescrTag: Length %" PRIu32, descrLength);
    
    if (descrLength > buffer->size()) {
        ERROR("bad esd data");
        return Nil;
    }
    
    // 2 + 1 = 3
    esd->ES_ID                   = buffer->rb16();
    esd->streamDependenceFlag    = buffer->read(1);
    esd->URL_Flag                = buffer->read(1);
    esd->OCRstreamFlag           = buffer->read(1);
    esd->streamPriority          = buffer->read(5);
    if (esd->streamDependenceFlag) {
        esd->dependsOn_ES_ID     = buffer->rb16();
    }
    if (esd->URL_Flag) {
        UInt8 URLlength        = buffer->r8();
        esd->URLstring           = buffer->rs(URLlength);
    }
    if (esd->OCRstreamFlag) {
        esd->OCR_ES_Id           = buffer->rb16();
    }
    
    DEBUG("ES_ID %" PRIu16 ", streamDependenceFlag %" PRIu8
          ", URL_Flag %" PRIu8 ", OCRstreamFlag %" PRIu8
          ", streamPriority %" PRIu8 ", dependsOn_ES_ID %" PRIu16
          ", URLstring %s, OCR_ES_Id %" PRIu16,
          esd->ES_ID, esd->streamDependenceFlag, esd->URL_Flag, esd->OCRstreamFlag,
          esd->streamPriority, esd->dependsOn_ES_ID,
          esd->URLstring != String::Null ? esd->URLstring.c_str() : "null",
          esd->OCR_ES_Id);
    
    // mandatory DecoderConfigDescriptor
    if (buffer->r8() != DecoderConfigDescrTag) {
        ERROR("missing DecoderConfigDescriptor");
        return Nil;
    }
    
    esd->decConfigDescr     = ReadDecoderConfigDescriptor(buffer);
    if (esd->decConfigDescr.isNil()) return Nil;
    
    return esd;
}

static void WriteESDescriptor(sp<ABuffer>& buffer, const sp<ESDescriptor>& esd) {
    CHECK_FALSE(esd.isNil());
    buffer->w8(esd->descrTag);     // ESDescrTag
    WriteObjectDescriptorSize(buffer, ESDescriptorLength(esd));
    buffer->wb16(esd->ES_ID);
    buffer->write(esd->streamDependenceFlag, 1);
    buffer->write(esd->URL_Flag, 1);
    buffer->write(esd->OCRstreamFlag, 1);
    buffer->write(esd->streamPriority, 5);
    if (esd->streamDependenceFlag) buffer->wb16(esd->dependsOn_ES_ID);
    if (esd->URL_Flag) {
        buffer->w8(esd->URLstring.size());
        buffer->ws(esd->URLstring);
    }
    if (esd->OCRstreamFlag) buffer->wb16(esd->OCR_ES_Id);
    
    CHECK_FALSE(esd->decConfigDescr.isNil());
    WriteDecoderConfigDescriptor(buffer, esd->decConfigDescr);
}

sp<Buffer> MakeESDS(const sp<ESDescriptor>& esd) {
    CHECK_FALSE(esd.isNil());
    if (esd->decConfigDescr.isNil()) {
        ERROR("DecoderConfigDescriptor missing");
        return Nil;
    }
    
    const UInt32 length = FullLength(ESDescriptorLength(esd));
    sp<ABuffer> esds = new Buffer(length);
    WriteESDescriptor(esds, esd);
    esds->write();     // write padding bits
    return esds;
}

sp<ESDescriptor> ReadESDS(const sp<Buffer>& esds) {
    CHECK_FALSE(esds.isNil());
    return ReadESDescriptor(esds);
}

__END_NAMESPACE(MPEG4)
__END_NAMESPACE_MFWK

