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


// File:    Module.h
// Author:  mtdcy.chen
// Changes:
//          1. 20160701     initial version
//

#define LOG_TAG "MediaFramework"
#include "MediaDefs.h"
#include "MediaDecoder.h"
#include "MediaFile.h"
#include "MediaPacketizer.h"
#include "MediaOut.h"
#include "ImageFile.h"
#include <libyuv.h>

__BEGIN_DECLS

size_t GetSampleFormatBytes(eSampleFormat format) {
    switch (format) {
        case kSampleFormatU8:       return sizeof(uint8_t);
        case kSampleFormatS16:      return sizeof(int16_t);
        case kSampleFormatS32:      return sizeof(int32_t);
        case kSampleFormatFLT:      return sizeof(float);
        case kSampleFormatDBL:      return sizeof(double);
        case kSampleFormatUnknown:  return 0;
        default:                    break;
    }
    FATAL("FIXME");
    return 0;
}

const char * GetSampleFormatString(eSampleFormat format) {
    switch (format) {
        case kSampleFormatU8:
            return "u8";
        case kSampleFormatS16:
            return "s16";
        case kSampleFormatS32:
            return "s32";
        case kSampleFormatFLT:
            return "flt";
        case kSampleFormatDBL:
            return "dbl";
        case kSampleFormatUnknown:
            return "unknown";
        default:
            break;
    }
    FATAL("FIXME");
    return 0;
}

__END_DECLS

__BEGIN_NAMESPACE_MPX

#ifdef __APPLE__
sp<MediaDecoder> CreateVideoToolboxDecoder(const sp<Message>& formats, const sp<Message>& options);
bool IsVideoToolboxSupported(eVideoCodec format);
#endif
#ifdef WITH_FFMPEG
sp<MediaDecoder> CreateLavcDecoder(const sp<Message>& formats, const sp<Message>& options);
#endif

sp<MediaDecoder> MediaDecoder::Create(const sp<Message>& formats, const sp<Message>& options) {
    CHECK_TRUE(formats->contains(kKeyCodecType));
    eCodecType type = (eCodecType)formats->findInt32(kKeyCodecType);
    eModeType mode = (eModeType)options->findInt32(kKeyMode, kModeTypeNormal);
    
    String env = GetEnvironmentValue("FORCE_AVCODEC");
    bool force = env.equals("1") || env.lower().equals("yes");
    
    if (force) mode = kModeTypeSoftware;

#ifdef WITH_FFMPEG
    if (mode == kModeTypeSoftware) {
        return CreateLavcDecoder(formats, options);
    }
#endif
    
#ifdef __APPLE__
    if (type == kCodecTypeVideo && mode != kModeTypeSoftware) {
        eVideoCodec codec = (eVideoCodec)formats->findInt32(kKeyFormat);
        if (!IsVideoToolboxSupported(codec)) {
            INFO("codec %#x is not supported by VideoToolbox", codec);
        } else {
            return CreateVideoToolboxDecoder(formats, options);
        }
    }
#endif
    
    // FALLBACK
#ifdef WITH_FFMPEG
    return CreateLavcDecoder(formats, options);
#else
    return NULL;
#endif
}

sp<MediaPacketizer> CreateMp3Packetizer();

sp<MediaPacketizer> MediaPacketizer::Create(uint32_t format) {
    switch (format) {
        case kAudioCodecMP3:
            return CreateMp3Packetizer();
        default:
            INFO("no packetizer for codec %d", format);
            return NULL;
    }
}

sp<ImageFile> CreateJPEG();
sp<ImageFile> ImageFile::Create() {
    return CreateJPEG();
}

sp<MediaOut> CreateOpenALOut(const sp<Message>& formats, const sp<Message>& options);
sp<MediaOut> CreateOpenGLOut(const sp<Message>& formats, const sp<Message>& options);
#ifdef WITH_SDL
sp<MediaOut> CreateSDLAudio(const sp<Message>& formats, const sp<Message>& options);
#endif
sp<MediaOut> MediaOut::Create(const sp<Message>& formats, const sp<Message>& options) {
    CHECK_TRUE(formats->contains(kKeyCodecType));
    eCodecType type = (eCodecType)formats->findInt32(kKeyCodecType);
    switch (type) {
        case kCodecTypeAudio:
            //return CreateOpenALOut(formats, options);
#ifdef WITH_SDL
            return CreateSDLAudio(formats, options);
#else
            return NULL;
#endif
        case kCodecTypeVideo:
            return CreateOpenGLOut(formats, options);
        default:
            return NULL;
    }
}

struct DefaultMediaPacket : public MediaPacket {
    sp<Buffer> buffer;

    FORCE_INLINE DefaultMediaPacket() : MediaPacket() { }
};

sp<MediaPacket> MediaPacketCreate(size_t size) {
    sp<Buffer> buffer = new Buffer(size);
    buffer->step(size);
    sp<DefaultMediaPacket> packet = new DefaultMediaPacket;
    packet->buffer  = buffer;
    packet->data    = (uint8_t*)buffer->data();
    packet->size    = buffer->capacity();
    return packet;
}

sp<MediaPacket> MediaPacketCreate(sp<Buffer>& data) {
    sp<DefaultMediaPacket> packet = new DefaultMediaPacket;
    packet->buffer  = data;
    packet->data    = (uint8_t*)data->data();
    packet->size    = data->size();
    return packet;
}

__END_NAMESPACE_MPX
