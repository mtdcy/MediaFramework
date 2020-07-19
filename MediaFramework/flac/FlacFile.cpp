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

#define LOG_TAG   "FlacFile"
#define LOG_NDEBUG 0
#include "MediaDevice.h"
#include "Flac.h"

__BEGIN_NAMESPACE_MFWK
__USING_NAMESPACE(Flac)

struct FlacFile : public MediaDevice {
    sp<ABuffer>     mSource;
    sp<Stream>      mFlac;
    sp<StreamInfo>  mStreamInfo;
    UInt64          mFrameOffset;
    
    FlacFile() : MediaDevice(FOURCC('fLaC')) { }
    
    MediaError init(const sp<ABuffer>& buffer) {
        mFlac = ParseFlacStream(buffer);
        if (mFlac.isNil()) {
            ERROR("parse flac failed.");
            return kMediaErrorBadContent;
        }
        
        for (UInt8 i = 0; i < mFlac->mBlocks.size(); ++i) {
            if (mFlac->mBlocks[i]->mType == kBlockStreamInfo) {
                mStreamInfo = mFlac->mBlocks[i];
                break;
            }
        }
        
        if (mStreamInfo.isNil()) {
            ERROR("missing stream info");
            return kMediaErrorBadFormat;
        }
        
        mSource = buffer;
        return kMediaNoError;
    }
    
    virtual sp<Message> formats() const {
        sp<Message> info = new Message;
        info->setInt32(kKeyFormat, kFileFormatFlac);
        
        sp<Message> audio = new Message;
        audio->setInt32(kKeyFormat, kAudioCodecFLAC);
        audio->setInt32(kKeyType, kCodecTypeAudio);
        audio->setInt32(kKeySampleRate, mStreamInfo->mSampleRate);
        audio->setInt32(kKeyChannels, mStreamInfo->mChannels);
        info->setObject(kKeyTrack, audio);
        
        return info;
    }
    
    virtual MediaError configure(const sp<Message>& options) {
        if (options->contains(kKeySeek)) {
            UInt64 time = options->findInt64(kKeySeek);
            // TODO
        }
        
        return kMediaErrorInvalidOperation;
    }
    
    virtual MediaError push(const sp<MediaFrame>&) {
        return kMediaErrorInvalidOperation;
    }
    
    virtual sp<MediaFrame> pull() {
        
        UInt16 sync = mSource->rb16();
        while ((sync & 0xFFFE) != 0xFFF8) {
            sync = (sync << 8) | mSource->r8();
        }
        
        UInt64 start = mSource->offset() - 2;
        
        mSource->skipBytes(mStreamInfo->mMinFrameSize -2);
        sync = mSource->rb16();
        while ((sync & 0xFFFE) != 0xFFF8) {
            sync = (sync << 8) | mSource->r8();
        }
        
        UInt64 end = mSource->offset() - 2;
        
        mSource->skipBytes(-(end - start + 2));
        sp<Buffer> data = mSource->readBytes(end - start);
        sp<MediaFrame> frame = MediaFrame::Create(data);
        
        // parse flac frame header
        sp<Frame> flac = new Frame;
        if (flac->parse(data) != kMediaNoError) {
            ERROR("parse flac frame failed");
            return Nil;
        }
        
        MediaTime duration  = MediaTime(flac->mSamples, flac->mSampleRate);
        MediaTime timecode;
        if (flac->mFixed) {
            timecode        = MediaTime((UInt64)flac->mSamples * flac->mFrameNumber, flac->mSampleRate);
        } else {
            timecode        = MediaTime(flac->mSampleNumber, flac->mSampleRate);
        }
        
        frame->audio.format     = kAudioCodecFLAC;
        frame->audio.channels   = flac->mChannels;
        frame->audio.freq       = flac->mSampleRate;
        frame->audio.samples    = flac->mSamples;
        
        frame->id               = 0;
        frame->flags            = kFrameTypeSync;
        frame->timecode         = timecode;
        frame->duration         = duration;

        DEBUG("pull %s", frame->string().c_str());
        return frame;
    }
    
    virtual MediaError reset() {
        return kMediaNoError;
    }
};

sp<MediaDevice> CreateFlacFile(const sp<ABuffer>& buffer) {
    sp<FlacFile> flac = new FlacFile;
    if (flac->init(buffer) == kMediaNoError)
        return flac;
    return Nil;
}

__END_NAMESPACE_MFWK
