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


// File:    Systems.h
// Author:  mtdcy.chen
// Changes: 
//          1. 20160701     initial version
//

#ifndef _MEDIA_MODULES_MPEG4_SYSTEMS_H
#define _MEDIA_MODULES_MPEG4_SYSTEMS_H 


#include <MediaToolkit/Toolkit.h>

// ISO/IEC 14496-1 System
namespace mtdcy { namespace MPEG4 {
    // ISO/IEC 14496-1:2010 Section 8.3.3 Page 117
    size_t GetObjectDescriptorSize(const BitReader& br);

    // ISO/IEC 14496-1:2010(E), Section 7.2.2.1, Page 31, Table 1
    // Object Description Tag
    enum {
        ObjectDescrTag                          = 0x1,
        InitialObjectDescrTag                   = 0x2,
        ES_DescrTag                             = 0x3,
        DecoderConfigDescrTag                   = 0x4,
        DecSpecificInfoTag                      = 0x5,
        SLConfigDescrTag                        = 0x6,
        ContentIdentDescrTag                    = 0x7,
        SupplContentIdentDescrTag               = 0x8,
        IPI_DescrPointerTag                     = 0x9,
        IPMP_DescrPointerTag                    = 0xa,
        IPMP_DescrTag                           = 0xb,
        QoS_DescrTag                            = 0xc,
        RegistrationDescrTag                    = 0xd,
        ES_ID_IncTag                            = 0xe,
        ES_ID_RefTag                            = 0xf,
        MP4_IOD_Tag                             = 0x10,
        MP4_OD_Tag                              = 0x11,
        IPL_DescrPointerRefTag                  = 0x12,
        ExtensionProfileLevelDescrTag           = 0x13,
        ProfileLevelIndicationIndexDescrTag     = 0x14,
        ContentClassificationDescrTag           = 0x40,
        KeyWordDescrTag                         = 0x41,
        RatingDescrTag                          = 0x42,
        LanguageDescrTag                        = 0x43,
        ShortTextualDescrTag                    = 0x44,
        ExpandedTextualDescrTag                 = 0x45,
        ContentCreatorNameDescrTag              = 0x46,
        ContentCreationDateDescrTag             = 0x47,
        OCICreatorNameDescrTag                  = 0x48,
        OCICreationDateDescrTag                 = 0x49,
        SmpteCameraPositionDescrTag             = 0x4a,
        SegmentDescrTag                         = 0x4b,
        MediaTimeDescrTag                       = 0x4c,
        IPMP_ToolsListDescrTag                  = 0x60,
        IPMP_ToolTag                            = 0x61,
        M4MuxTimingDescrTag                     = 0x62,
        M4MuxCodeTableDescrTag                  = 0x63,
        ExtSLConfigDescrTag                     = 0x64,
        M4MuxBufferSizeDescrTag                 = 0x65,
        M4MuxIdentDescrTag                      = 0x66,
        DependencyPointerTag                    = 0x67,
        DependencyMarkerTag                     = 0x68,
        M4MuxChannelDescrTag                    = 0x69,
    };

    // ObjectTypeIndication
    // ISO/IEC 14496-1:2010(E), Section 7.2.6.6.2, Page 49, Table 5
    // http://www.mp4ra.org/object.html
    enum {
        ISO_IEC_14496_2             = 0x20,         // MPEG4 visual
        ISO_IEC_14496_10            = 0x21,         // H264
        ISO_IEC_23008_2             = 0x22,         // H265
        ISO_IEC_14496_3             = 0x40,         // MPEG4 sound / AAC
        ISO_IEC_13818_2_Simple      = 0x60,         // MPEG2 Simple Profile
        ISO_IEC_13818_2_Main        = 0x61,         // MPEG2 Main Profile
        ISO_IEC_13818_2_SNR         = 0x62,         // MPEG2 SNR Profile 
        ISO_IEC_13818_2_Spatial     = 0x63,         // MPEG2 Spatial Profile 
        ISO_IEC_13818_2_High        = 0x64,         // MPEG2 High Profile 
        ISO_IEC_13818_2_422         = 0x65,         // MPEG2 422 Profile 
        ISO_IEC_13818_7_Main        = 0x66,         // MPEG2 AAC Main 
        ISO_IEC_13818_7_LC          = 0x67,         // MPEG2 AAC LC
        ISO_IEC_13818_7_SSR         = 0x68,         // MPEG2 AAC SSR
        ISO_IEC_13818_3             = 0x69,         // MP3 
        ISO_IEC_11172_2             = 0x6a,         // MPEG1VIDEO
        ISO_IEC_11172_3             = 0x6b,         // MP2 
        ISO_IEC_10918_1             = 0x6c,         // MJPEG
    };
    int32_t translateObjectTypeIndication(uint8_t objectTypeIndication);

    // streamType
    // ISO/IEC 14496-1:2010(E), Section 7.2.6.6.2, Page 51, Table 6
    // http://www.mp4ra.org/object.html
    enum {
        ObjectDescriptorStream      = 0x1,
        ClockReferenceStream        = 0x2,
        SceneDescriptionStream      = 0x3,
        VisualStream                = 0x4,
        AudioStream                 = 0x5,
        MPEG7Stream                 = 0x6,
        IPMPStream                  = 0x7,
        ObjectContentInfoStream     = 0x8,
        MPEGJStream                 = 0x9,
        InteractionStream           = 0xa,
        IPMPToolStream              = 0xb,
    };

    struct BaseDescriptor {
        BaseDescriptor() : valid(false) { }
        BaseDescriptor(const uint8_t tag) : valid(false), descrTag(tag) { }
        virtual ~BaseDescriptor() { }
        bool            valid;
        uint8_t         descrTag;
    };

    // ISO/IEC 14496-1:2010(E), Section 7.2.6.7 Page 51
    // The existence and semantics of decoder specific information depends on 
    // the values of DecoderConfigDescriptor.streamType and 
    // DecoderConfigDescriptor.objectTypeIndication.
    // Note: decoder usually need this information, for demuxer, there may be 
    // no need to parse this information
    struct DecoderSpecificInfo : public BaseDescriptor {
        DecoderSpecificInfo() : BaseDescriptor(DecSpecificInfoTag) { }
        DecoderSpecificInfo(const BitReader& br);
        sp<Buffer>  csd;
    };

    struct DecoderConfigDescriptor : public BaseDescriptor {
        DecoderConfigDescriptor() : BaseDescriptor(DecoderConfigDescrTag) { }
        DecoderConfigDescriptor(const BitReader& br);

        uint8_t                 objectTypeIndication;
        uint8_t                 streamType;
        uint8_t                 upStream;
        uint32_t                bufferSizeDB;
        uint32_t                maxBitrate;
        uint32_t                avgBitrate;
        DecoderSpecificInfo     decSpecificInfo;    // optional
        // profileLevelIndicationIndexDescriptor
    };

    // ISO/IEC 14496-1:2010(E), Section 7.2.6.5, Page 47
    struct ES_Descriptor : public BaseDescriptor {
        ES_Descriptor() : BaseDescriptor(ES_DescrTag) {}
        ES_Descriptor(const BitReader& br);

        uint8_t                 tag;
        uint8_t                 length;
        uint16_t                ES_ID;
        uint8_t                 streamDependenceFlag;
        uint8_t                 URL_Flag;
        uint8_t                 OCRstreamFlag;
        uint8_t                 streamPriority;
        uint16_t                dependsOn_ES_ID;
        String                  URLstring;
        uint16_t                OCR_ES_Id;
        DecoderConfigDescriptor decConfigDescr;     // mandatory
        sp<Buffer>              extraBytes;
    };
}; };

#endif // _MEDIA_MODULES_MPEG4_SYSTEMS_H
