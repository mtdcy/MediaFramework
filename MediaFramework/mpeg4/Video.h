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


// File:    Video.h
// Author:  mtdcy.chen
// Changes: 
//          1. 20181126     initial version
//

#ifndef _MEDIA_MODULES_MPEG4_VIDEO_H
#define _MEDIA_MODULES_MPEG4_VIDEO_H

#include <MediaFramework/MediaTypes.h>

// ISO/IEC 14496-10 Advanced Video Coding

__BEGIN_NAMESPACE_MPX

namespace MPEG4 {
    // ISO/IEC 14496-10 Section 7.4.1 Table 7.1
    enum eNALUnitType {
        NALU_TYPE_UNKNOWN       = 0,    ///<
        // VCL
        NALU_TYPE_SLICE         = 1,    ///< coded slice of a non-IDR pitrue
        NALU_TYPE_DPA           = 2,    ///< coded slice data partition A
        NALU_TYPE_DPB           = 3,    ///< ..
        NALU_TYPE_DPC           = 4,    ///<
        NALU_TYPE_IDR           = 5,    ///< coded slice of an IDR piture
        // NON-VCL
        NALU_TYPE_SEI           = 6,    ///< supplement enhance information
        NALU_TYPE_SPS           = 7,    ///< sequence parameter set
        NALU_TYPE_PPS           = 8,    ///< picture parameter set
        NALU_TYPE_AUD           = 9,    ///< access unit delimiter
        NALU_TYPE_EOSEQ         = 10,   ///< end of sequence
        NALU_TYPE_EOS           = 11,   ///< end of stream
        NALU_TYPE_FILL          = 12,   ///< filler data
        NALU_TYPE_SPSE          = 13,   ///< sequence parameter set extension
        NALU_TYPE_PREFIX        = 14,   ///< prefix nal unit
        NALU_TYPE_SSPS          = 15,   ///< subset sequence parameter set
        NALU_TYPE_RESERVED16    = 16,   ///< reserved
        NALU_TYPE_RESERVED17    = 17,   ///< reserved
        NALU_TYPE_RESERVED18    = 18,   ///< reserved
        NALU_TYPE_AUX           = 19,   ///< coded slice of an auxiliary coded picture without partition
        NALU_TYPE_EXT           = 20,   ///< coded slice extension
    };

    enum eSliceType {
        SLICE_TYPE_P            = 0,    ///<
        SLICE_TYPE_B            = 1,
        SLICE_TYPE_I            = 2,
        SLICE_TYPE_SP           = 3,    ///<
        SLICE_TYPE_SI           = 4,    ///< intra prediction only
        SLICE_TYPE_P2           = 5,
        SLICE_TYPE_B2           = 6,
        SLICE_TYPE_I2           = 7,
        SLICE_TYPE_SP2          = 8,
        SLICE_TYPE_SI2          = 9,
    };

    // ISO/IEC 14496-10 Section 7.3.1
    // The bitstream can be in one of two formats:
    //  a. the NAL unit stream format (basic), -> NALU
    //  b. the byte stream format, start code prefix + NALU -> Annex B
    //
    // NALU = HEADER + RBSP(Raw Byte Sequence Payload)
    struct NALU {
        uint8_t         nal_ref_idc;    // 0: non-referenced picture
        eNALUnitType    nal_unit_type;
        union {
            struct {
                uint32_t    first_mb_in_slice;
                eSliceType  slice_type;
                // ...
            } slice_header;  // SLICE | DPA | IDR
        };
        MediaError      parse(const sp<ABuffer>&);
    };

    // https://stackoverflow.com/questions/24884827/possible-locations-for-sequence-picture-parameter-sets-for-h-264-stream
    // NAL is the basic unit, the are two h264 stream format
    // a. Annexb format: SC + NALU + SC + NALU + ...
    // b. avcc format: [avcC + ] length + NALU + length + NALU
    //     where length depends on NALULengthSizeMinusOne in avcC
    // and in both Annexb and avcc format, NALUs are no difference
    enum eH264StreamFormat {
        kH264AnnexBFormat,
        kH264AvccFormat,
    };
    eH264StreamFormat GetH264StreamFormat(const sp<ABuffer>&);

    // ISO/IEC 14496-10 Annex A
    enum eAVCProfile {
        kAVCProfileBaseline     = 0,
        kAVCProfileMain,
        kAVCProfileExtended,
        kAVCProfileHigh,
        kAVCProfileHigh10,
        kAVCProfileHigh422,
        kAVCProfileHigh444Predictive,
        kAVCProfileHigh10Intra,
        kAVCProfileHigh422Intra,
        kAVCProfileHigh444Intra,
        kAVCProfileCAVLC444Intra,
    };

    // ISO/IEC 14495-15, 'avcC'
    struct AVCDecoderConfigurationRecord {
        AVCDecoderConfigurationRecord() {}
        MediaError parse(const sp<ABuffer>&);
        MediaError compose(sp<ABuffer>&) const;
        size_t size() const;

        uint8_t     AVCProfileIndication;
        uint8_t     AVCLevelIndication;
        uint8_t     lengthSizeMinusOne;
        List<sp<Buffer> >   SPSs;
        List<sp<Buffer> >   PPSs;
    };

}

__END_NAMESPACE_MPX

#endif // _MEDIA_MODULES_MPEG4_VIDEO_H
