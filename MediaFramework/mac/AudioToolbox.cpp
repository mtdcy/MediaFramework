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


// File:    mac/AudioToolbox.cpp
// Author:  mtdcy.chen
// Changes:
//          1. 20200628     initial version
//

#define LOG_TAG "mac.AT"
//#define LOG_NDEBUG 0
#include <ABE/ABE.h>

#include <AudioToolbox/AudioToolbox.h>

#include <MediaFramework/MediaTypes.h>
#include <MediaFramework/MediaDecoder.h>


__BEGIN_NAMESPACE_MPX

struct {
    eAudioCodec         codec;
    AudioFormatID       id;
} kFormatMap[] = {
    { kAudioCodecPCM,       kAudioFormatLinearPCM   },
    { kAudioCodecAAC,       kAudioFormatMPEG4AAC    },
    // END OF LIST
    { kAudioCodecUnknown,   0                       }
};

static eAudioCodec GetAudioCodec(AudioFormatID id) {
    for (size_t i = 0; kFormatMap[i].codec != kAudioCodecUnknown; ++i) {
        if (kFormatMap[i].id == id)
            return kFormatMap[i].codec;
    }
    return kAudioCodecUnknown;
}

static AudioFormatID GetAudioFormatID(eAudioCodec codec) {
    for (size_t i = 0; kFormatMap[i].codec != kAudioCodecUnknown; ++i) {
        if (kFormatMap[i].codec == codec)
            return kFormatMap[i].id;
    }
    FATAL("FIXME: add entry for %#x[%.4s]", codec, (const char *)&codec);
    return 0;
}

// WHY NOT USING LIBAVCODEC DIRECTLY:
//  there are some codec need license which we or users don't have it.
//  but os or hardware may have certain codecs & license, so we have to
//  detect os & hardware features and use its codecs.
struct ATAC : public SharedObject {
    AudioConverterRef           atac;
    AudioStreamBasicDescription inFormat;
    AudioStreamBasicDescription outFormat;
    
    // compressed packet
    AudioStreamPacketDescription desc;
    sp<MediaPacket>             packet;
    
    // uncompressed frame
    sp<MediaFrame>              frame;
    
    ATAC() : atac(NULL) { }
};

static eSampleFormat GetSampleFormat(const AudioStreamBasicDescription& format) {
    const bool plannar = format.mFormatFlags & kAudioFormatFlagIsNonInterleaved;
    switch (format.mBitsPerChannel) {
        case 16:
            return plannar ? kSampleFormatS16 : kSampleFormatS16Packed;
        case 32:
            return plannar ? kSampleFormatS32 : kSampleFormatS32Packed;
        default:
            FATAL("FIXME");
            break;
    }
    return kSampleFormatUnknown;
}

static void printErrorCode(OSStatus errcode) {
    switch (errcode) {
        case kAudioConverterErr_FormatNotSupported:     ERROR("format not supported");  break;
        case kAudioConverterErr_OperationNotSupported:  ERROR("operation not supported"); break;
        case kAudioConverterErr_PropertyNotSupported:   ERROR("property not supported"); break;
        case kAudioConverterErr_InvalidInputSize:       ERROR("invalid input size"); break;
        case kAudioConverterErr_InvalidOutputSize:      ERROR("invalid output size"); break;
        case kAudioConverterErr_UnspecifiedError:       ERROR("unspecified error"); break;
        case kAudioConverterErr_BadPropertySizeError:   ERROR("bad property size"); break;
        case kAudioConverterErr_RequiresPacketDescriptionsError:    ERROR("requires packet descriptions"); break;
        case kAudioConverterErr_InputSampleRateOutOfRange:          ERROR("input sample rate out of range"); break;
        case kAudioConverterErr_OutputSampleRateOutOfRange:         ERROR("output sample rate out of range"); break;
#if TARGET_IS_IPHONE
        case kAudioConverterErr_HardwareInUse:          ERROR("hardware in use"); break;
        case kAudioConverterErr_NoHardwarePermission:   ERROR("no hardware permission"); break;
#endif
        default:    ERROR("error %#x[%.4s]", errcode, (const char *)&errcode); break;
    }
}

static bool refineInputFormat(AudioStreamBasicDescription& desc) {
    desc.mFormatFlags       = 0;
    desc.mFramesPerPacket   = 1024;
    desc.mBitsPerChannel    = 16;
    // calc others
    desc.mBytesPerFrame     = desc.mChannelsPerFrame * desc.mBitsPerChannel / 8;
    desc.mBytesPerPacket    = desc.mFramesPerPacket * desc.mBytesPerFrame;
    desc.mReserved          = 0;
    
    return true;
}

static bool refineOutputFormat(AudioStreamBasicDescription& desc) {
    desc.mFramesPerPacket   = 1;
    if (desc.mFormatFlags & kAudioFormatFlagIsNonInterleaved)
        desc.mBytesPerFrame = desc.mBitsPerChannel / 8;
    else
        desc.mBytesPerFrame = desc.mChannelsPerFrame * desc.mBitsPerChannel / 8;
    desc.mBytesPerPacket    = desc.mFramesPerPacket * desc.mBytesPerFrame;
    desc.mReserved          = 0;
    return true;
}

static sp<ATAC> openATAC(const sp<Message>& formats) {
    DEBUG("open AudioCodec << %s", formats->string().c_str());
    sp<ATAC> atac = new ATAC;
    AudioComponentDescription desc;
    
    AudioFormatID formatID  = GetAudioFormatID(formats->findInt32(kKeyFormat));
    
    atac->inFormat.mFormatID            = formatID;
    atac->inFormat.mSampleRate          = formats->findInt32(kKeySampleRate);
    atac->inFormat.mChannelsPerFrame    = formats->findInt32(kKeyChannels);
    if (!refineInputFormat(atac->inFormat)) {
        ERROR("no matching input format");
        return NULL;
    }
    
    sp<Buffer> esds = formats->findObject(kKeyESDS);
    if (!esds.isNIL()) {
        UInt32 propertyDataSize = sizeof(atac->inFormat);
        OSStatus st = AudioFormatGetProperty(kAudioFormatProperty_FormatInfo,
                               (UInt32)esds->size(),
                               esds->data(),
                               &propertyDataSize,
                               &atac->inFormat);
        if (st != 0) {
            ERROR("AudioFormatGetProperty return error %#x[%.4s]", st, (const char *)&st);
        }
    }

    atac->outFormat                     = atac->inFormat;
    atac->outFormat.mFormatID           = kAudioFormatLinearPCM;
    atac->outFormat.mFormatFlags        = kAudioFormatFlagIsSignedInteger | kAudioFormatFlagIsPacked | kAudioFormatFlagIsNonInterleaved;
    atac->outFormat.mBitsPerChannel     = formats->findInt32(kKeySampleBits, 16);
    if (!refineOutputFormat(atac->outFormat)) {
        ERROR("no matching output format");
        return NULL;
    }
    
    OSStatus st = AudioConverterNew(&atac->inFormat,
                                    &atac->outFormat,
                                    &atac->atac);
    
    if (st != 0) {
        printErrorCode(st);
        ERROR("init audio codec failed");
        return NULL;
    }
    
    if (!esds.isNIL()) {
        st = AudioConverterSetProperty(atac->atac,
                                       kAudioConverterDecompressionMagicCookie,
                                       (UInt32)esds->size(),
                                       esds->data());
        if (st != 0) {
            printErrorCode(st);
            ERROR("set magic cookie failed");
            return NULL;
        }
    }
    
    UInt32 propertyDataSize = sizeof(atac->inFormat);
    st = AudioConverterGetProperty(atac->atac,
                                   kAudioConverterCurrentInputStreamDescription,
                                   &propertyDataSize,
                                   &atac->inFormat);
    if (st != 0) {
        printErrorCode(st);
        ERROR("update input format failed");
        return NULL;
    }
    
    propertyDataSize = sizeof(atac->outFormat);
    st = AudioConverterGetProperty(atac->atac,
                                   kAudioConverterCurrentOutputStreamDescription,
                                   &propertyDataSize,
                                   &atac->outFormat);
    
    return atac;
}

static void closeATAC(sp<ATAC>& atac) {
    if (atac->atac) AudioConverterDispose(atac->atac);
}

static OSStatus DecodeCallback(AudioConverterRef               inAudioConverter,
                               UInt32 *                        ioNumberDataPackets,
                               AudioBufferList *               ioData,
                               AudioStreamPacketDescription ** outDataPacketDescription,
                               void * __nullable               inUserData) {
    sp<ATAC> atac = inUserData;
    CHECK_EQ(atac->atac, inAudioConverter);
    
    DEBUG("callback");
    
    if (atac->packet.isNIL()) {
        // eos
        *ioNumberDataPackets            = 0;
        if (outDataPacketDescription) {
            *outDataPacketDescription   = &atac->desc;
        }
        return 0;
    }
    
    ioData->mNumberBuffers              = 1;
    ioData->mBuffers[0].mNumberChannels = atac->inFormat.mChannelsPerFrame;
    ioData->mBuffers[0].mDataByteSize   = atac->packet->size;
    ioData->mBuffers[0].mData           = atac->packet->data;
    *ioNumberDataPackets                = 1;
    
    if (outDataPacketDescription) {
        *outDataPacketDescription       = &atac->desc;
    }
    return 0;
}

struct MyAudioBufferList {
    UInt32      mNumberBuffers;
    AudioBuffer mBuffers[MEDIA_FRAME_NB_PLANES];
};

static MediaError decode(sp<ATAC>& atac, const sp<MediaPacket>& packet) {
    if (packet.isNIL()) {
        INFO("eos...");
    } else {
        if (!atac->frame.isNIL()) {
            return kMediaErrorResourceBusy;
        }
        DEBUG("write packet %.3f(s)", packet->pts.seconds());
    }
    
    atac->packet                                = packet;
    atac->desc.mStartOffset                     = 0;
    atac->desc.mDataByteSize                    = packet.isNIL() ? 0 : packet->size;
    atac->desc.mVariableFramesInPacket          = 0;
    
    
    UInt32 ioOutputDataPacketSize               = atac->inFormat.mFramesPerPacket;
    
    AudioFormat audio;
    audio.format                                = GetSampleFormat(atac->outFormat);
    audio.channels                              = atac->outFormat.mChannelsPerFrame;
    audio.freq                                  = atac->outFormat.mSampleRate;
    audio.samples                               = atac->inFormat.mFramesPerPacket;
    sp<MediaFrame> frame                        = MediaFrame::Create(audio);
    
    const bool plannar                          = atac->outFormat.mFormatFlags & kAudioFormatFlagIsNonInterleaved;
    MyAudioBufferList outOutputData;
    if (plannar) {
        outOutputData.mNumberBuffers            = atac->outFormat.mChannelsPerFrame;
        for (size_t i = 0; i < outOutputData.mNumberBuffers; ++i) {
            outOutputData.mBuffers[i].mNumberChannels   = 0;
            outOutputData.mBuffers[i].mDataByteSize     = frame->planes[i].size;
            outOutputData.mBuffers[i].mData             = frame->planes[i].data;
        }
    } else {
        outOutputData.mNumberBuffers                = 1;
        outOutputData.mBuffers[0].mNumberChannels   = atac->inFormat.mChannelsPerFrame;
        outOutputData.mBuffers[0].mDataByteSize     = frame->planes[0].size;
        outOutputData.mBuffers[0].mData             = frame->planes[0].data;
    }
    
    AudioStreamPacketDescription desc;
    OSStatus st = AudioConverterFillComplexBuffer(atac->atac,
                                                  DecodeCallback,
                                                  atac.get(),
                                                  &ioOutputDataPacketSize,
                                                  (AudioBufferList*)&outOutputData,
                                                  &desc);

    if (st != 0) {
        CAShow(atac->atac);
        printErrorCode(st);
        ERROR("append input data failed");
        return kMediaErrorBadContent;
    }
    
    if (packet.isNIL()) {
        INFO("eos...");
        return kMediaNoError;
    }
    
    frame->timecode     = packet->pts;
    frame->duration     = MediaTime(frame->a.samples, frame->a.freq);
    atac->frame         = frame;
    return kMediaNoError;
}

struct AudioCodec : public MediaDecoder {
    sp<ATAC>            mATAC;
    
    AudioCodec() : MediaDecoder() { }
    
    virtual ~AudioCodec() {
        closeATAC(mATAC);
    }
    
    MediaError init(const sp<Message>& formats, const sp<Message>& options) {
        mATAC = openATAC(formats);
        return mATAC.isNIL() ? kMediaErrorBadFormat : kMediaNoError;
    }
    
    virtual sp<Message> formats() const {
        sp<Message> format = new Message;
        
        format->setInt32(kKeyFormat, GetSampleFormat(mATAC->outFormat));
        format->setInt32(kKeySampleRate, mATAC->outFormat.mSampleRate);
        format->setInt32(kKeyChannels, mATAC->outFormat.mChannelsPerFrame);
        return format;
    }
    
    virtual MediaError write(const sp<MediaPacket>& packet) {
        return decode(mATAC, packet);
    }
    
    virtual sp<MediaFrame> read() {
        sp<MediaFrame> frame = mATAC->frame;
        mATAC->frame.clear();
        
        if (frame.isNIL()) {
            INFO("eos...");
            return NULL;
        }
        
        DEBUG("read frame @ %.3f(s)", frame->timecode.seconds());
        return frame;
    }
    
    virtual MediaError flush() {
        AudioConverterReset(mATAC->atac);
        mATAC->packet.clear();
        mATAC->frame.clear();
        return kMediaNoError;
    }
};

sp<MediaDecoder> CreateAudioToolbox(const sp<Message>& formats, const sp<Message>& options) {
    sp<AudioCodec> codec = new AudioCodec;
    if (codec->init(formats, options) == kMediaNoError)
        return codec;
    return NULL;
}

__END_NAMESPACE_MPX
