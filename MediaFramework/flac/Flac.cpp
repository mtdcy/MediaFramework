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


// File:    Flac.cpp
// Author:  mtdcy.chen
// Changes:
//          1. 20200715     initial version
//

#define LOG_TAG   "Flac"
#define LOG_NDEBUG 0
#include "Flac.h"
#include "algo/CRC.h"

__BEGIN_NAMESPACE_MFWK
__BEGIN_NAMESPACE(Flac)

MediaError StreamInfo::parse(const sp<ABuffer>& buffer) {
    if (buffer->size() < 34) {
        ERROR("Flac stream info expect 34 bytes, but got %" PRIu32, buffer->size());
        return kMediaErrorBadContent;
    }
    mMinBlockSize   = buffer->rb16();
    mMaxBlockSize   = buffer->rb16();
    mMinFrameSize   = buffer->rb24();
    mMaxFrameSize   = buffer->rb24();
    mSampleRate     = buffer->read(20);
    mChannels       = buffer->read(3) + 1;
    mBitsPerSample  = buffer->read(5) + 1;
    mTotalSamples   = buffer->read(32);
    mTotalSamples   = (mTotalSamples << 32) | buffer->read(4);
    //mTotalSamples   = buffer->read(36);
    for (UInt8 i = 0; i < 16; ++i) {
        mMD5[i]     = buffer->r8();
    }
    
    DEBUG("BlockSize %u/%u, FrameSize %u/%u, mSampleRate %u, mChannels %u, mBitsPerSample %u, mTotoalSamples %" PRIu64,
          mMinBlockSize, mMaxBlockSize, mMinFrameSize, mMaxFrameSize, mSampleRate, mChannels, mBitsPerSample, mTotalSamples);
    return kMediaNoError;
}

MediaError Padding::parse(const sp<ABuffer>&) {
    return kMediaNoError;
}

MediaError Application::parse(const sp<ABuffer>& buffer) {
    mID = buffer->rb32();
    mData = buffer->readBytes(buffer->size() - 4);
    return kMediaNoError;
}

MediaError SeekTable::parse(const sp<ABuffer>& buffer) {
    while (buffer->size() >= 18) {
        SeekPoint sp;
        sp.mSampleNumber    = buffer->rb64();
        sp.mOffset          = buffer->rb64();
        sp.mSamples         = buffer->rb16();
        
        // placeholder
        if (sp.mSampleNumber == 0xFFFFFFFFFFFFFFFF) continue;
        
        DEBUG("+SeekPoint: mSampleNumber = %" PRIu64 ", mOffset = %" PRIu64 ", mSamples = %u",
              sp.mSampleNumber, sp.mOffset, sp.mSamples);
        mSeekPoints.push(sp);
    }
    
    if (buffer->size()) {
        WARN("padding inside seek table");
    }
    return kMediaNoError;
}

MediaError VorbisComment::parse(const sp<ABuffer>&) {
    return kMediaNoError;
}

MediaError CUESheet::parse(const sp<ABuffer>&) {
    return kMediaNoError;
}

MediaError Picture::parse(const sp<ABuffer>& buffer) {
    mType           = (ePictureType)buffer->rb32();
    mMIME           = buffer->rs(buffer->rb32());
    mDescription    = buffer->rs(buffer->rb32());
    mWidth          = buffer->rb32();
    mHeight         = buffer->rb32();
    mBitsPerPixel   = buffer->rb32();
    mNumberColors   = buffer->rb32();
    mData           = buffer->readBytes(buffer->rb32());
    DEBUG("mType = %u, mMIME = %s, mDescription = %s, mWidth = %u, mHeight = %u, mBitsPerPixel = %u, mNumberColors = %u",
          mType, mMIME.c_str(), mDescription.c_str(), mWidth, mHeight, mBitsPerPixel, mNumberColors);
    return kMediaNoError;
}

static const UInt32 kFrameSampleRate[16] = {
    0 /*@see StreamInfo*/,
    88200, 176400, 192000,
    8000, 16000, 22050, 24000,
    32000, 44100, 48000, 96000,
    // value @ end of header
    12, 13, 14,
    // invalid
    0
};

static const UInt32 kFrameSamples[16] = {
    0,
    192,
    576, 1152, 2304, 4608,
    6, 7,   // 8/16 bits @ end of header
    256, 512, 1024, 2048, 4096, 8192, 16384, 32768
};

static const UInt32 kFrameChannels[16] = {
    1, 2, 3, 4, 5, 6, 7, 8,
    // special stereo
    2, 2, 2,
    // reserved
    0, 0, 0, 0, 0
};

static UInt64 UTF8CodedNumber(const sp<ABuffer>& buffer) {
    UInt64 v = buffer->r8();
    // bad value ?
    // min: 1 byte: 0xxx xxxx
    // max: 6 bytes: 1111 110x xx...
    if ((v & 0xC0) == 0x80 || v >= 0xFE) return 0;
    
    UInt64 leading = 0x80;
    while (v & leading) {
        UInt8 x = buffer->r8() - 0x80;
        if (x > 0x3F) return 0;
        v = (v << 6) | x;
        leading <<= (6 - 1);    // <<6, >>1
    }
    // leading meet the 0 bit after 1 bits.
    return (v & (leading - 1));
}

MediaError Frame::parse(const sp<ABuffer>& buffer) {
    if (buffer->read(15) != 0x7FFC) {
        ERROR("bad flac frame");
        return kMediaErrorBadContent;
    }
    mFixed          = buffer->read(1) == 0;
    mSamples        = kFrameSamples[buffer->read(4)];
    mSampleRate     = kFrameSampleRate[buffer->read(4)];
    mChannels       = kFrameChannels[buffer->read(4)];
    mBitsPerSample  = buffer->read(3);
    buffer->skip(1);
    if (mFixed)
        mFrameNumber = UTF8CodedNumber(buffer);
    else
        mSampleNumber = UTF8CodedNumber(buffer);
    
    // read samples
    if (mSamples == 6 || mSamples == 7) {
        mSamples    = buffer->read(8 << (mSamples - 6));
    }
    // read sample rate
    switch (mSampleRate) {
        case 12: mSampleRate = buffer->read(8) * 1000; break;
        case 13: mSampleRate = buffer->read(16); break;
        case 14: mSampleRate = buffer->read(16) * 10; break;
        default: break;
    }
    
    // TODO: check CRC8
    DEBUG("mFixed = %u, mSamples = %u, mSampleRate = %u, mChannels = %u, mBitsPerSample = %u",
          mFixed, mSamples, mSampleRate, mChannels, mBitsPerSample);
    return kMediaNoError;
}

static const Char * kBlockNames[7] = {
    "StreamInfo",
    "Padding",
    "Application",
    "SeekTable",
    "VorbisComment",
    "CUESheet",
    "Picture"
};

sp<Stream> ParseFlacStream(const sp<ABuffer>& buffer) {
    buffer->setByteOrder(ABuffer::Big);
    
    String marker = buffer->rs(4);
    if (marker != "fLaC") {
        ERROR("not flac");
        return Nil;
    }
    
    sp<Stream> flac = new Stream;
    while (buffer->size() > 4) {
        Bool last = buffer->read(1);
        UInt8 type = buffer->read(7);
        UInt32 length = buffer->r24();
        
        if (type >= kBlockReservedFirst) {
            ERROR("bad block type %" PRIu8, type);
            break;
        }
        DEBUG("found block %s, length %" PRIu32, kBlockNames[type], length);
        
        sp<Block> block;
        switch (type) {
            case kBlockStreamInfo:      block = new StreamInfo; break;
            case kBlockPadding:         block = new Padding; break;
            case kBlockApplication:     block = new Application; break;
            case kBlockSeekTable:       block = new SeekTable; break;
            case kBlockVorbisComment:   block = new VorbisComment; break;
            case kBlockCUESheet:        block = new CUESheet; break;
            case kBlockPicture:         block = new Picture; break;
            default: break;
        }
        
        if (block.isNil()) {
            ERROR("invalid block type %" PRIu8, type);
            break;
        }
        
        sp<Buffer> data = buffer->readBytes(length);
        if (data->size() < length) {
            ERROR("corrupt file ?");
            break;
        }
        
        if (block->parse(data) != kMediaNoError) {
            ERROR("parse block failed");
            break;
        }
        
        flac->mBlocks.push(block);
        if (last) break;
    }
    
    flac->mFrameOffset = buffer->offset();
    return flac;
}

__END_NAMESPACE(Flac)
__END_NAMESPACE_MFWK
