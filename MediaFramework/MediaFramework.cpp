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
#include "MediaExtractor.h"
#include "MediaPacketizer.h"
#include "MediaOut.h"
#include <libyuv.h>

__BEGIN_DECLS

eCodecType GetCodecType(eCodecFormat format) {
    if (format > kAudioCodecFormatFirst && format < kAudioCodecFormatLast)
        return kCodecTypeAudio;
    else if (format > kVideoCodecFormatFirst && format < kVideoCodecFormatLast)
        return kCodecTypeVideo;
    else if (format > kSubtitleFormatFirst && format < kSubtitleFormatLast)
        return kCodecTypeSubtitle;
    else if (format > kImageCodecFormatFirst && format < kImageCodecFormatLast)
        return kCodecTypeImage;
    else
        return kCodecTypeUnknown;
}

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

sp<MediaExtractor> CreateMp3File();
sp<MediaExtractor> CreateMp4File();
sp<MediaExtractor> CreateMatroskaFile();

sp<MediaExtractor> MediaExtractor::Create(eFileFormat format) {
    switch (format) {
        case kFileFormatMP3:
            return CreateMp3File();
        case kFileFormatMP4:
            return CreateMp4File();
        case kFileFormatMKV:
            return CreateMatroskaFile();
        default:
            return NULL;
    }
}

#ifdef __APPLE__
sp<MediaDecoder> CreateVideoToolboxDecoder();
bool IsVideoToolboxSupported(eCodecFormat format);
#endif
#ifdef WITH_FFMPEG
sp<MediaDecoder> CreateLavcDecoder(eModeType mode);
#endif

sp<MediaDecoder> MediaDecoder::Create(eCodecFormat format, eModeType mode) {
    sp<MediaDecoder> codec;
    eCodecType type = GetCodecType(format);

#ifdef __APPLE__
    if (type == kCodecTypeVideo && mode != kModeTypeSoftware) {
        if (!IsVideoToolboxSupported(format)) {
            INFO("codec %d is not supported by VideoToolbox", format);
        } else {
            codec = CreateVideoToolboxDecoder();
        }
    }
    // FALL BACK TO SOFTWARE DECODER
#endif
    if (codec == NULL) {
#ifdef WITH_FFMPEG
        codec = CreateLavcDecoder(mode);
#endif
    }

    return codec;
}

sp<MediaPacketizer> CreateMp3Packetizer();

sp<MediaPacketizer> MediaPacketizer::Create(eCodecFormat format) {
    switch (format) {
        case kAudioCodecFormatMP3:
            return CreateMp3Packetizer();
        default:
            INFO("no packetizer for codec %d", format);
            return NULL;
    }
}

sp<MediaOut> CreateGLVideo();
#ifdef WITH_SDL
sp<MediaOut> CreateSDLAudio();
#endif
sp<MediaOut> MediaOut::Create(eCodecType type) {
    switch (type) {
        case kCodecTypeAudio:
#ifdef WITH_SDL
            return CreateSDLAudio();
#else
            return NULL;
#endif
        case kCodecTypeVideo:
            return CreateGLVideo();
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
