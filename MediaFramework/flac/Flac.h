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


// File:    Libavformat.cpp
// Author:  mtdcy.chen
// Changes:
//          1. 20200715     initial version
//

#ifndef MFWK_FLAC_H
#define MFWK_FLAC_H

#include "MediaTypes.h"

__BEGIN_NAMESPACE_MFWK
__BEGIN_NAMESPACE(Flac)

struct Block;
struct Frame;
struct Stream : public SharedObject {
    Vector<sp<Block> >    mBlocks;
    //Vector<sp<Frame> >    mFrames;
    UInt64              mFrameOffset;
};

enum eBlockType {
    kBlockStreamInfo = 0,
    kBlockPadding,
    kBlockApplication,
    kBlockSeekTable,
    kBlockVorbisComment,
    kBlockCUESheet,
    kBlockPicture,
    kBlockReservedFirst,
    kBlockInvalid = 127
};

struct Block : public SharedObject {
    const eBlockType    mType;
    Block(eBlockType type) : mType(type) { }
    virtual MediaError parse(const sp<ABuffer>&) = 0;
};

struct StreamInfo : public Block {
    UInt16          mMinBlockSize;  // in samples
    UInt16          mMaxBlockSize;
    UInt32          mMinFrameSize;  // in bytes
    UInt32          mMaxFrameSize;
    UInt32          mSampleRate;    // in Hz
    UInt8           mChannels;      // up to 8 channels
    UInt8           mBitsPerSample; // [4, 32]
    UInt64          mTotalSamples;  // inter-channel
    UInt8           mMD5[16];
    
    StreamInfo() : Block(kBlockStreamInfo) { }
    virtual MediaError parse(const sp<ABuffer>&);
};

struct Padding : public Block {
    Padding() : Block(kBlockPadding) { }
    virtual MediaError parse(const sp<ABuffer>&);
};

struct Application : public Block {
    UInt32          mID;
    sp<Buffer>      mData;
    
    Application() : Block(kBlockApplication) { }
    virtual MediaError parse(const sp<ABuffer>&);
};

struct SeekTable : public Block {
    struct SeekPoint {
        UInt64      mSampleNumber;
        UInt64      mOffset;
        UInt16      mSamples;
    };
    Vector<SeekPoint>   mSeekPoints;
    
    SeekTable() : Block(kBlockSeekTable) { }
    virtual MediaError parse(const sp<ABuffer>&);
};

// Flac tags
struct VorbisComment : public Block {
    
    VorbisComment() : Block(kBlockVorbisComment) { }
    virtual MediaError parse(const sp<ABuffer>&);
};

struct CUESheet : public Block {
    CUESheet() : Block(kBlockCUESheet) { }
    virtual MediaError parse(const sp<ABuffer>&);
};

enum ePictureType {
    kPictureOther = 0,
    kPicturePNGIcon32x32,
    kPictureOtherIcon,
    kPictureFrontCover,
    kPictureBackCover,
    kPictureLeafletPage,
    kPictureMedia,
    kPictureLeadArtist,
    kPictureArtist,
    kPictureConductor,
    kPictureBand,
    kPictureComposer,
    kPictureLyricist,
    kPictureRecodingLocation,
    kPictureDuringRecording,
    kPictureDuringPerformance,
    kPictureMovieScreenCapture,
    kPictureBrightColouredFish,
    kPictureIllustration,
    kPictureBandLogotype,
    kPicturePublisherLogotype,
    kPictureReservedFirst = 21
};

struct Picture : public Block {
    ePictureType    mType;
    String          mMIME;
    String          mDescription;
    UInt32          mWidth;
    UInt32          mHeight;
    UInt32          mBitsPerPixel;
    UInt32          mNumberColors;
    sp<Buffer>      mData;
    
    Picture() : Block(kBlockPicture) { }
    virtual MediaError parse(const sp<ABuffer>&);
};

enum eSubFrameType {
    kSubFrameConstant   = 0,
    kSubFrameVerbatim   = 0x1,
    kSubFrameFixed      = 0x8,
    kSubFrameLPC        = 0x20,
};

struct SubFrame {
    
};

struct Frame : public SharedObject {
    // Header
    Bool            mFixed;
    UInt16          mSamples;
    UInt32          mSampleRate;
    UInt16          mChannels;
    UInt16          mBitsPerSample;
    union {
        UInt32      mFrameNumber;   // mFixed == True
        UInt64      mSampleNumber;  // mFixed == False
    };
    UInt8           mCRC8;
    
    // Footer
    UInt16          mCRC16;
    
    Frame() { }
    virtual MediaError parse(const sp<ABuffer>&);
};

sp<Stream> ParseFlacStream(const sp<ABuffer>&);

__END_NAMESPACE(Flac)
__END_NAMESPACE_MFWK

#endif // MFWK_FLAC_H
