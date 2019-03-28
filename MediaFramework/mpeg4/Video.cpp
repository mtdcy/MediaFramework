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


// File:    Video.cpp
// Author:  mtdcy.chen
// Changes:
//          1. 20160701     initial version
//

#define LOG_TAG "MPEG4.Video"
#define LOG_NDEBUG 0
#include "MediaDefs.h"

#include "Video.h"

__BEGIN_NAMESPACE_MPX
__BEGIN_NAMESPACE(MPEG4)

// AnnexB格式通常用于实时的流格式，比如说传输流，通过无线传输的广播、DVD等。
// 在这些格式中通常会周期性的重复SPS和PPS包，经常是在每一个关键帧之前，
// 因此据此建立解码器可以一个随机访问的点，这样就可以加入一个正在进行的流，及播放一个已经在传输的流。
// AVCC格式的一个优点是在开始配置解码器的时候可以跳到流的中间播放
// 这种格式通常用于可以被随机访问的多媒体数据，如存储在硬盘的文件。
// 也因为这个特性，MP4、MKV通常用AVCC格式来存储
eH264StreamFormat GetH264StreamFormat(sp<Buffer>& stream) {
    BitReader br(*stream);
    if (br.show(8) == 1) {     // avcc format with avcC
        AVCDecoderConfigurationRecord avcC(br);
        if (avcC.valid) {
            return kH264AvccFormat;
        }
        br.reset();
    }
    
    // AnnexB format
    if (br.show(24) == 0x1 || br.show(32) == 0x1) {
        return kH264AnnexBFormat;
    }
    
    // avcc format without avcC
    return kH264AvccFormat;
}

//! avcC:
//!
//! refers to ISO/IEC 14496-15 5.2.4 Decoder configuration information
//! aligned(8) class AVCDecoderConfigurationRecord {
//!     unsigned int(8) configurationVersion = 1;
//!     unsigned int(8) AVCProfileIndication;
//!     unsigned int(8) profile_compatibility;
//!     unsigned int(8) AVCLevelIndication;
//!     bit(6) reserved = ‘111111’b;
//!     unsigned int(2) lengthSizeMinusOne;     //--> nal length size - 1
//!     bit(3) reserved = ‘111’b;
//!     unsigned int(5) numOfSequenceParameterSets;
//!     for (i=0; i< numOfSequenceParameterSets; i++) {
//!         unsigned int(16) sequenceParameterSetLength ;
//!         bit(8*sequenceParameterSetLength) sequenceParameterSetNALUnit;
//!     }
//!     unsigned int(8) numOfPictureParameterSets;
//!     for (i=0; i< numOfPictureParameterSets; i++) {
//!         unsigned int(16) pictureParameterSetLength;
//!         bit(8*pictureParameterSetLength) pictureParameterSetNALUnit;
//!     }
//! }
AVCDecoderConfigurationRecord::AVCDecoderConfigurationRecord(const BitReader& br) :
valid(false) {
    // 1
    if (br.r8() != 1) return;           // configurationVersion = 1
    // 3
    AVCProfileIndication = br.r8();
    uint8_t profile_compatibility = br.r8();
    AVCLevelIndication = br.r8();
    // (6 + 2 + 3 + 5) / 8 = 2
    if (br.read(6) != 0x3f) return;     // bit(6) reserved = ‘111111’b;
    lengthSizeMinusOne = br.read(2);    //
    if (br.read(3) != 0x7) return;      // bit(3) reserved = ‘111’b;
    size_t numOfSequenceParameterSets = br.read(5);
    // n * (2 + x)
    for (size_t i = 0; i < numOfSequenceParameterSets; ++i) {
        size_t sequenceParameterSetLength = br.rb16();
        SPSs.push(br.readB(sequenceParameterSetLength));
    }
    // 1
    size_t numOfPictureParameterSets = br.r8();
    // n * (2 + x)
    for (size_t i = 0; i < numOfPictureParameterSets; ++i) {
        size_t pictureParameterSetLength = br.rb16();
        PPSs.push(br.readB(pictureParameterSetLength));
    }
    valid = true;
}

status_t AVCDecoderConfigurationRecord::compose(BitWriter& bw) const {
    CHECK_GE(bw.size(), size());
    bw.w8(1);                           // configurationVersion
    // TODO: get profile and level from sps
    bw.w8(AVCProfileIndication);        // AVCProfileIndication
    bw.w8(0);                           // profile_compatibility
    bw.w8(AVCLevelIndication);          // AVCLevelIndication
    bw.write(0x3f, 6);
    bw.write(lengthSizeMinusOne, 2);    // lengthSizeMinusOne
    bw.write(0x7, 3);                   //
    bw.write(SPSs.size(), 5);           // numOfSequenceParameterSets
    List<sp<Buffer> >::const_iterator it = SPSs.cbegin();
    for (; it != SPSs.cend(); ++it) {
        const sp<Buffer>& sps = *it;
        bw.wb16(sps->size());           // sequenceParameterSetLength
        bw.writeB(*sps);                // sps
    }
    bw.w8(PPSs.size());                 // numOfPictureParameterSets
    for (; it != PPSs.cend(); ++it) {
        const sp<Buffer>& pps = *it;
        bw.wb16(pps->size());           // pictureParameterSetLength
        bw.writeB(*pps);                // pps
    }
    bw.write();
    return OK;
}

size_t AVCDecoderConfigurationRecord::size() const {
    size_t n = 1 + 3 + 2;
    // sps
    List<sp<Buffer> >::const_iterator it = SPSs.cbegin();
    for (; it != SPSs.cend(); ++it) {
        n += (2 + (*it)->size());
    }
    n += 1;     // numOfPictureParameterSets
    // pps
    it = PPSs.cbegin();
    for (; it != PPSs.cend(); ++it) {
        n += (2 + (*it)->size());
    }
    return n;
}

__END_NAMESPACE(MPEG4)
__END_NAMESPACE_MPX

