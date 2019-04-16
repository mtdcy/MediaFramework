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
#include "ColorConvertor.h"
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

const char * GetPixelFormatString(ePixelFormat pixel) {
    switch (pixel) {
            // planar yuv
        case kPixelFormatYUV420P:           return "yuv420p";
        case kPixelFormatYUV422P:           return "yuv422p";
        case kPixelFormatYUV444P:           return "yuv444p";
        case kPixelFormatNV12:              return "nv12";
        case kPixelFormatNV21:              return "nv21";
            
            // packed yuv
        case kPixelFormatYUYV422:           return "yuyv422";
        case kPixelFormatUYVY422:           return "uyvy422";
            
            // rgb
        case kPixelFormatRGB565:            return "rgb565";
        case kPixelFormatRGB888:            return "rgb888";
        case kPixelFormatARGB:              return "argb";
        case kPixelFormatRGBA:              return "rgba";
            
        case kPixelFormatUnknown:           return "unknown";
            
        case kPixelFormatVideoToolbox:      return "mac-vt";
            
        default:
            FATAL("FIXME: missing case for %d", pixel);
            return "";
    }
}

size_t GetPixelFormatBPP(ePixelFormat pixel) {
    switch (pixel) {
            // planar yuv
        case kPixelFormatYUV420P:           return 12;
        case kPixelFormatYUV422P:           return 16;
        case kPixelFormatYUV444P:           return 24;
        case kPixelFormatNV12:              return 12;
        case kPixelFormatNV21:              return 12;
            
            // packed yuv
        case kPixelFormatYUYV422:           return 16;
        case kPixelFormatUYVY422:           return 16;
            
            // rgb
        case kPixelFormatRGB565:            return 16;
        case kPixelFormatRGB888:            return 24;
        case kPixelFormatARGB:              return 32;
        case kPixelFormatRGBA:              return 32;
            
        case kPixelFormatUnknown:           return 0;
            
        case kPixelFormatVideoToolbox:      return 0;
            
        default:
            FATAL("FIXME: missing case for %d", pixel);
            return 0;
    }
}

bool GetPixelFormatIsPlanar(ePixelFormat pixel) {
    switch (pixel) {
        case kPixelFormatYUV420P:
        case kPixelFormatYUV422P:
        case kPixelFormatYUV444P:
        case kPixelFormatNV12:
        case kPixelFormatNV21:      return true;
        default:                    return false;
    }
}

size_t GetPixelFormatPlanes(ePixelFormat pixel) {
    switch (pixel) {
            // planar yuv
        case kPixelFormatYUV420P:
        case kPixelFormatYUV422P:
        case kPixelFormatYUV444P:   return 3;
        case kPixelFormatNV12:
        case kPixelFormatNV21:      return 2;
        case kPixelFormatUnknown:   return 0;
        default:                    break;
    }
    
    if (GetPixelFormatIsPlanar(pixel) == false) {
        return 1;
    } else {
        FATAL("FIXME: missing case for %d", pixel);
        return 0;
    }
}

size_t GetPixelFormatPlaneBPP(ePixelFormat pixel, size_t plane) {
    CHECK_LT(plane, GetPixelFormatPlanes(pixel));
    if (GetPixelFormatIsPlanar(pixel) == false) {
        return GetPixelFormatBPP(pixel);
    }
    
    switch (pixel) {
        case kPixelFormatYUV420P:   return plane == 0 ? 8 : 2;
        case kPixelFormatYUV422P:   return plane == 0 ? 8 : 4;
        case kPixelFormatYUV444P:   return 8;
        case kPixelFormatNV12:      return plane == 0 ? 8 : 4;
        case kPixelFormatNV21:      return plane == 0 ? 8 : 4;

        case kPixelFormatUnknown:   return 0;
            
        default:
            FATAL("FIXME: missing case for %d", pixel);
            return 0;
    }
}

size_t GetImageFormatBytes(const ImageFormat * image) {
    return (image->width * image->height * GetPixelFormatBPP(image->format)) / 8;
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
sp<MediaDecoder> CreateLavcDecoder(eModeType mode);

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
        codec = CreateLavcDecoder(mode);
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
sp<MediaOut> CreateSDLAudio();

sp<MediaOut> MediaOut::Create(eCodecType type) {
    switch (type) {
        case kCodecTypeAudio:
            return CreateSDLAudio();
        case kCodecTypeVideo:
            return CreateGLVideo();
        default:
            return NULL;
    }
}

MediaFrame::MediaFrame() : pts(kTimeInvalid), duration(kTimeInvalid) {
    for (size_t i = 0; i < MEDIA_FRAME_NB_PLANES; ++i) {
        planes[i].data = NULL;
        planes[i].size = 0;
    }
    format = 0;
    opaque = NULL;
}

sp<Buffer> MediaFrame::getData(size_t index) const {
    if (planes[index].data == NULL) return NULL;
    return new Buffer((const char *)planes[index].data, planes[index].size);
}

struct _MediaFrame : public MediaFrame {
    // one continues buffer for all planes
    sp<Buffer> buffer;
};

sp<MediaFrame> MediaFrameCreate(const ImageFormat& image) {
    const size_t bytes = GetImageFormatBytes(&image);
    Object<Buffer> buffer = new Buffer(bytes);
    return MediaFrameCreate(image, buffer);
}

sp<MediaFrame> MediaFrameCreate(const ImageFormat& image, const sp<Buffer>& buffer) {
    const size_t bytes = GetImageFormatBytes(&image);
    if (buffer->capacity() < bytes) return NULL;
    
    Object<_MediaFrame> frame = new _MediaFrame;
    frame->buffer = buffer;
    
    if (GetPixelFormatIsPlanar(image.format)) {
        uint8_t * next = (uint8_t*)frame->buffer->data();
        for (size_t i = 0; i < GetPixelFormatPlanes(image.format); ++i) {
            const size_t bpp = GetPixelFormatPlaneBPP(image.format, i);
            const size_t bytes = (image.width * image.height * bpp) / 8;
            frame->planes[i].data   = next;
            frame->planes[i].size   = bytes;
            next += bytes;
        }
    } else {
        frame->planes[0].data   = (uint8_t*)frame->buffer->data();
        frame->planes[0].size   = bytes;
    }
    
    frame->v    = image;
    return frame;
}

String GetAudioFormatString(const AudioFormat& a) {
    return String::format("audio %s: ch %d, freq %d, samples %d",
                          GetSampleFormatString(a.format),
                          a.channels,
                          a.freq,
                          a.samples);
}

String GetImageFrameString(const sp<MediaFrame>& frame) {
    String desc = String::format("image %s [%s %zu planes %zu bpp]: resolution %d x %d[%d, %d, %d, %d], pts %" PRId64 "/%" PRId64,
                                 GetPixelFormatString(frame->v.format),
                                 GetPixelFormatIsPlanar(frame->v.format) ? "planar" : "packed",
                                 GetPixelFormatPlanes(frame->v.format),
                                 GetPixelFormatBPP(frame->v.format),
                                 frame->v.width, frame->v.height,
                                 frame->v.rect.x, frame->v.rect.y, frame->v.rect.w, frame->v.rect.h,
                                 frame->pts.value, frame->pts.timescale);
    for (size_t i = 0; i < GetPixelFormatPlanes(frame->v.format); ++i) {
        desc.append("\n");
        desc.append(String::format("\t plane %zu: %p bytes %zu", i, frame->planes[i].data, frame->planes[i].size));
    }
    return desc;
}

sp<MediaFrame> MediaFrameCreate(const AudioFormat& a) {
    const size_t bytes = GetSampleFormatBytes(a.format);
    const size_t total = bytes * a.channels * a.samples;
    sp<_MediaFrame> frame = new _MediaFrame;
    frame->buffer = new Buffer(total);
    
    uint8_t * next = (uint8_t*)frame->buffer->data();
    for (size_t i = 0; i < a.channels; ++i) {
        frame->planes[i].size   = bytes * a.samples;
        frame->planes[i].data   = next;
        next += frame->planes[i].size;
    }
    
    frame->a            = a;
    return frame;
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

ColorConvertor::ColorConvertor(ePixelFormat pixel) :
    mFormat(pixel)
{
    INFO("=> %#x", pixel);
}

ColorConvertor::~ColorConvertor() {
}

struct {
    ePixelFormat    a;
    uint32_t        b;
} kMap[] = {
    { kPixelFormatNV12,     libyuv::FOURCC_NV12 },
    // END OF LIST
    { kPixelFormatUnknown,  0}
};

uint32_t get_libyuv_pixel_format(ePixelFormat a) {
    for (size_t i = 0; kMap[i].a != kPixelFormatUnknown; ++i) {
        if (kMap[i].a == a) return kMap[i].b;
    }
    FATAL("FIXME");
    return libyuv::FOURCC_ANY;
}

sp<MediaFrame> ColorConvertor::convert(const sp<MediaFrame>& input) {
    if (input->v.format == mFormat) return input;
    
    ImageFormat format = input->v;
    format.format = mFormat;
    sp<MediaFrame> out  = MediaFrameCreate(format);
    out->pts            = input->pts;
    out->duration       = input->duration;

    if (mFormat == kPixelFormatYUV420P) {
        switch (input->v.format) {
            case kPixelFormatNV12:
                {
                    libyuv::NV12ToI420(
                            input->planes[0].data,  input->v.width,
                            input->planes[1].data, input->v.width,
                            out->planes[0].data,  out->v.width,
                            out->planes[1].data,  out->v.width / 2,
                            out->planes[2].data,  out->v.width / 2,
                            input->v.width,
                            input->v.height
                            );
                } break;
            default:
                FATAL("FIXME");
                break;
        }
    } else {
        FATAL("FIXME");
    }
    return out;
}
__END_NAMESPACE_MPX
