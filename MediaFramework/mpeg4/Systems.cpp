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

eCodecFormat translateObjectTypeIndication(eObjectTypeIndication objectTypeIndication) {
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
            return kCodecFormatUnknown;
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

static size_t DecoderSpecificInfoLength(const sp<DecoderSpecificInfo>& dsi) {
    CHECK_FALSE(dsi.isNIL());
    if (dsi->csd.isNIL()) return 0; // invalid
    return dsi->csd->size();
}

// DecSpecificInfoTag
static sp<DecoderSpecificInfo> ReadDecoderSpecificInfo(const BitReader& br) {
    if (br.r8() != DecSpecificInfoTag) {
        ERROR("bad DecoderSpecificInfo");
        return NIL;
    }
    sp<DecoderSpecificInfo> dsi = new DecoderSpecificInfo;
    uint32_t descrLength    = GetObjectDescriptorSize(br);
    DEBUG("DecSpecificInfoTag: Length %" PRIu32, descrLength);
    dsi->csd = br.readB(descrLength);
    DEBUG("csd %s", dsi->csd->string(true).c_str());
    return dsi;
}

static void WriteDecoderSpecificInfo(BitWriter& bw, const sp<DecoderSpecificInfo>& dsi) {
    CHECK_FALSE(dsi->csd.isNIL());
    bw.w8(dsi->descrTag);    // DecSpecificInfoTag
    WriteObjectDescriptorSize(bw, DecoderSpecificInfoLength(dsi));
    bw.writeB(*dsi->csd);
}

static FORCE_INLINE size_t FullLength(size_t length) {
    return 1 + GetObjectDescriptorSizeLength(length) + length;
}

static size_t DecoderConfigDescriptorLength(const sp<DecoderConfigDescriptor>& dcd) {
    CHECK_FALSE(dcd.isNIL());
    size_t length = 13;
    if (dcd->decSpecificInfo.isNIL()) return length;
    return length + FullLength(DecoderSpecificInfoLength(dcd->decSpecificInfo));
}

static sp<DecoderConfigDescriptor> ReadDecoderConfigDescriptor(const BitReader& br) {
    if (br.r8() != DecoderConfigDescrTag) {
        ERROR("bad DecoderConfigDescriptor");
        return NIL;
    }
    sp<DecoderConfigDescriptor> dcd = new DecoderConfigDescriptor;
    uint32_t descrLength    = GetObjectDescriptorSize(br);
    DEBUG("DecoderConfigDescrTag: Length %" PRIu32, descrLength);
    
    // 2 + 3 + 4 + 4 = 13
    dcd->objectTypeIndication    = (eObjectTypeIndication)br.r8();
    dcd->streamType              = (eStreamType)br.read(6);
    dcd->upStream                = br.read(1);
    br.skip(1);    //reserved
    dcd->bufferSizeDB            = br.rb24();
    dcd->maxBitrate              = br.rb32();
    dcd->avgBitrate              = br.rb32();
    
    DEBUG("objectTypeIndication 0x%" PRIx8 ", streamType 0x%" PRIx8
          ", upStream %" PRIu8 ", bufferSizeDB %" PRIu32
          ", maxBitrate %" PRIu32 ", avgBitrate %" PRIu32,
          dcd->objectTypeIndication, dcd->streamType, dcd->upStream,
          dcd->bufferSizeDB, dcd->maxBitrate, dcd->avgBitrate);
    
    if (descrLength > 13) {
        uint8_t descrTag = br.show(8);
        if (br.show(8) == DecSpecificInfoTag) {
            dcd->decSpecificInfo = ReadDecoderSpecificInfo(br);
        } else {
            DEBUG("unkown tag 0x%" PRIx8, descrTag);
        }
    }
    return dcd;
}

static void WriteDecoderConfigDescriptor(BitWriter& bw, const sp<DecoderConfigDescriptor>& dcd) {
    bw.w8(dcd->descrTag);                    // DecoderConfigDescrTag
    WriteObjectDescriptorSize(bw, DecoderConfigDescriptorLength(dcd));
    bw.w8(dcd->objectTypeIndication);
    bw.write(dcd->streamType, 6);
    bw.write(dcd->upStream, 1);
    bw.write(0, 1);  // reserved
    bw.wb24(dcd->bufferSizeDB);
    bw.wb32(dcd->maxBitrate);
    bw.wb32(dcd->avgBitrate);
    
    if (dcd->decSpecificInfo.isNIL()) return;
    WriteDecoderSpecificInfo(bw, dcd->decSpecificInfo);
}

static size_t ESDescriptorLength(const sp<ESDescriptor>& esd) {
    CHECK_FALSE(esd.isNIL());
    
    size_t length =  3
    + (esd->streamDependenceFlag ? 2 : 0)
    + (esd->URL_Flag ? esd->URLstring.size() + 1 : 0)
    + (esd->OCRstreamFlag ? 2 : 0);
    
    if (esd->decConfigDescr.isNIL()) return length;
    return length + FullLength(DecoderConfigDescriptorLength(esd->decConfigDescr));
}

static sp<ESDescriptor> ReadESDescriptor(const BitReader& br) {
    if (br.r8() != ESDescrTag) {
        ERROR("bad ESDescriptor");
        return NIL;
    }
    
    sp<ESDescriptor> esd = new ESDescriptor;
    uint32_t descrLength    = GetObjectDescriptorSize(br);
    DEBUG("ESDescrTag: Length %" PRIu32, descrLength);
    
    if (descrLength > br.remianBytes()) {
        ERROR("bad esd data");
        return NIL;
    }
    
    // 2 + 1 = 3
    esd->ES_ID                   = br.rb16();
    esd->streamDependenceFlag    = br.read(1);
    esd->URL_Flag                = br.read(1);
    esd->OCRstreamFlag           = br.read(1);
    esd->streamPriority          = br.read(5);
    if (esd->streamDependenceFlag) {
        esd->dependsOn_ES_ID     = br.rb16();
    }
    if (esd->URL_Flag) {
        uint8_t URLlength   = br.r8();
        esd->URLstring           = br.readS(URLlength);
    }
    if (esd->OCRstreamFlag) {
        esd->OCR_ES_Id           = br.rb16();
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
    if (br.show(8) != DecoderConfigDescrTag) {
        ERROR("missing DecoderConfigDescriptor");
        return NIL;
    }
    
    esd->decConfigDescr     = ReadDecoderConfigDescriptor(br);
    if (esd->decConfigDescr.isNIL()) return NIL;
    
    return esd;
}

static void WriteESDescriptor(BitWriter& bw, const sp<ESDescriptor>& esd) {
    CHECK_FALSE(esd.isNIL());
    bw.w8(esd->descrTag);     // ESDescrTag
    WriteObjectDescriptorSize(bw, ESDescriptorLength(esd));
    bw.wb16(esd->ES_ID);
    bw.write(esd->streamDependenceFlag, 1);
    bw.write(esd->URL_Flag, 1);
    bw.write(esd->OCRstreamFlag, 1);
    bw.write(esd->streamPriority, 5);
    if (esd->streamDependenceFlag) bw.wb16(esd->dependsOn_ES_ID);
    if (esd->URL_Flag) {
        bw.w8(esd->URLstring.size());
        bw.writeS(esd->URLstring);
    }
    if (esd->OCRstreamFlag) bw.wb16(esd->OCR_ES_Id);
    
    CHECK_FALSE(esd->decConfigDescr.isNIL());
    WriteDecoderConfigDescriptor(bw, esd->decConfigDescr);
}

sp<Buffer> MakeESDS(const sp<ESDescriptor>& esd) {
    CHECK_FALSE(esd.isNIL());
    if (esd->decConfigDescr.isNIL()) {
        ERROR("DecoderConfigDescriptor missing");
        return NIL;
    }
    
    const size_t length = FullLength(ESDescriptorLength(esd));
    sp<Buffer> esds = new Buffer(length);
    BitWriter bw(esds->data(), esds->capacity());
    WriteESDescriptor(bw, esd);
    bw.write();     // write padding bits
    esds->step(bw.size());
    return esds;
}

sp<ESDescriptor> ReadESDS(const sp<Buffer>& esds) {
    CHECK_FALSE(esds.isNIL());
    BitReader br(esds->data(), esds->size());
    return ReadESDescriptor(br);
}

__END_NAMESPACE(MPEG4)
__END_NAMESPACE_MPX

