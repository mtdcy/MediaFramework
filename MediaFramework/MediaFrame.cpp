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


// File:    MediaFrame.h
// Author:  mtdcy.chen
// Changes:
//          1. 20160701     initial version
//

#define LOG_TAG "MediaFrame"
#define LOG_NDEBUG 0
#include "MediaFrame.h"
#include <libyuv.h>

__BEGIN_NAMESPACE_MPX

MediaFrame::MediaFrame() : pts(kTimeInvalid), duration(kTimeInvalid) {
    for (size_t i = 0; i < MEDIA_FRAME_NB_PLANES; ++i) {
        planes[i].data = NULL;
        planes[i].size = 0;
    }
    format = 0;
    opaque = NULL;
}

sp<Buffer> MediaFrame::readPlane(size_t index) const {
    if (planes[index].data == NULL) return NULL;
    return new Buffer((const char *)planes[index].data, planes[index].size);
}

sp<MediaFrame> MediaFrame::Create(const ImageFormat& image) {
    const PixelDescriptor * desc = GetPixelFormatDescriptor(image.format);
    CHECK_NULL(desc);
    const size_t bytes = (image.width * image.height * desc->bpp) / 8;
    
    Object<Buffer> buffer = new Buffer(bytes);
    return MediaFrame::Create(image, buffer);
}

sp<MediaFrame> MediaFrame::Create(const ImageFormat& image, const sp<Buffer>& buffer) {
    const PixelDescriptor * desc = GetPixelFormatDescriptor(image.format);
    CHECK_NULL(desc);
    const size_t bytes = (image.width * image.height * desc->bpp) / 8;
    if (buffer->capacity() < bytes) return NULL;
    
    Object<MediaFrame> frame = new MediaFrame;
    frame->mBuffer = buffer;
    
    if (desc->planes > 1) {
        uint8_t * next = (uint8_t*)frame->mBuffer->data();
        for (size_t i = 0; i < desc->planes; ++i) {
            const size_t bytes = (image.width * image.height * desc->plane[i].bpp) / 8;
            frame->planes[i].data   = next;
            frame->planes[i].size   = bytes;
            next += bytes;
        }
    } else {
        frame->planes[0].data   = (uint8_t*)frame->mBuffer->data();
        frame->planes[0].size   = bytes;
    }
    
    frame->v    = image;
    
    DEBUG("create: %s", GetImageFrameString(frame).c_str());
    return frame;
}

// TODO: borrow some code from ffmpeg
static FORCE_INLINE uint16_t _swap16(uint16_t x) {
    return (x >> 8) | (x << 8);
}
static FORCE_INLINE void swap16(uint8_t * u8, size_t size) {
    uint16_t * u16 = (uint16_t *)u8;
    CHECK_EQ(size % 2, 0);
    for (size_t j = 0; j < size / 2; ++j) {
        uint16_t x = *u16;
        *u16++ = _swap16(x);
    }
}

// aabbccdd -> ddccbbaa
static FORCE_INLINE uint32_t _swap32(uint32_t x) {
    return ((x >> 24) & 0xff) | ((x >> 8) & 0xff00) | ((x << 8) & 0xff0000) | ((x << 24) & 0xff000000);
}
static FORCE_INLINE void swap32(uint8_t * u8, size_t size) {
    uint32_t *u32 = (uint32_t *)u8;
    CHECK_EQ(size % 4, 0);
    for (size_t i = 0; i < size / 4; ++i) {
        *u32 = _swap32(*u32);
        ++u32;
    }
}

// only swap low bytes of uint32_t
// aabbccdd -> aaddccbb
static FORCE_INLINE uint32_t _swap32l(uint32_t x) {
    return (x & 0xff000000) | (x & 0xff00) | ((x >> 16) & 0xff) | ((x << 16) & 0xff0000);
}
static FORCE_INLINE void swap32l(uint8_t * u8, size_t size) {
    uint32_t *u32 = (uint32_t *)u8;
    CHECK_EQ(size % 4, 0);
    for (size_t i = 0; i < size / 4; ++i) {
        *u32 = _swap32l(*u32);
        ++u32;
    }
}

MediaError MediaFrame::swapCbCr() {
    DEBUG("swap u/v: %s", GetImageFrameString(this).c_str());
    switch (v.format) {
        case kPixelFormat420YpCbCrPlanar:
        case kPixelFormat422YpCbCrPlanar:
        case kPixelFormat444YpCbCrPlanar: {
            // swap u & v planes
            uint8_t * tmp0  = planes[1].data;
            size_t tmp1     = planes[1].size;
            planes[1]       = planes[2];
            planes[2].data  = tmp0;
            planes[2].size  = tmp1;
        } return kMediaNoError;
            
        case kPixelFormat420YpCbCrSemiPlanar:
        case kPixelFormat420YpCrCbSemiPlanar:
            // swap hi & low bytes of uv plane
            swap16(planes[1].data, planes[1].size);
            return kMediaNoError;
            
        case kPixelFormat422YpCbCr:
        case kPixelFormat422YpCrCb:
            // swap u & v bytes
            swap32l(planes[0].data, planes[0].size);
            return kMediaNoError;
            
        case kPixelFormatRGB565:
        case kPixelFormatBGR565:
        case kPixelFormatRGB:
        case kPixelFormatBGR:
        case kPixelFormatRGBA:
        case kPixelFormatABGR:
        case kPixelFormatARGB:
        case kPixelFormatBGRA:
            return kMediaErrorInvalidOperation;
            
        default:
            break;
    }
    return kMediaErrorNotSupported;
}

// aabbcc -> ccbbaa
static FORCE_INLINE void swap24(uint8_t * u8, size_t size) {
    CHECK_EQ(size % 3, 0);
    for (size_t i = 0; i < size / 3; ++i) {
        uint8_t tmp = u8[0];
        u8[0] = u8[2];
        u8[2] = tmp;
        u8 += 3;
    }
}

MediaError MediaFrame::reversePixel() {
    DEBUG("reverse bytes: %s", GetImageFrameString(this).c_str());
    if (planes[1].data) {   // is planar
        return kMediaErrorInvalidOperation;
    }
    
    switch (v.format) {
        case kPixelFormatRGB:
        case kPixelFormatBGR:
        case kPixelFormat444YpCbCr:
            swap24(planes[0].data, planes[0].size);
            return kMediaNoError;
            
        case kPixelFormatRGBA:
        case kPixelFormatABGR:
        case kPixelFormatARGB:
        case kPixelFormatBGRA:
        case kPixelFormat422YpCbCr:
        case kPixelFormat422YpCrCb:
            swap32(planes[0].data, planes[0].size);
            return kMediaNoError;
            
        case kPixelFormatRGB565:
        case kPixelFormatBGR565:
        default:
            break;
    }
    return kMediaErrorNotSupported;
}

static struct {
    ePixelFormat    packed;
    ePixelFormat    planar;
} kMap[] = {
    { kPixelFormat422YpCbCr,        kPixelFormat422YpCbCrPlanar     },
    { kPixelFormat422YpCrCb,        kPixelFormat422YpCrCbPlanar     },
    { kPixelFormat444YpCbCr,        kPixelFormat444YpCbCrPlanar     },
    // END OF LIST
    { kPixelFormatUnknown }
};
static ePixelFormat GetPlanar(ePixelFormat packed) {
    for (size_t i = 0; kMap[i].packed != kPixelFormatUnknown; ++i) {
        if (kMap[i].packed == packed) return kMap[i].planar;
    }
    return packed;
}

typedef int (*Packed2Planar_t)(const uint8_t* src,
                                int src_stride,
                                uint8_t* dst_y,
                                int dst_stride_y,
                                uint8_t* dst_u,
                                int dst_stride_u,
                                uint8_t* dst_v,
                                int dst_stride_v,
                                int width,
                                int height);

MediaError MediaFrame::planarization() {
    ePixelFormat planar = GetPlanar(v.format);
    if (v.format == planar) return kMediaNoError;
    const PixelDescriptor * a = GetPixelFormatDescriptor(v.format);
    const PixelDescriptor * b = GetPixelFormatDescriptor(planar);
    Packed2Planar_t hnd = NULL;
    switch (v.format) {
        case kPixelFormat422YpCrCb:
        case kPixelFormat422YpCbCr:
            hnd = libyuv::YUY2ToI422;
            break;
        case kPixelFormat444YpCbCr:
            hnd = (Packed2Planar_t)libyuv::SplitRGBPlane;
            break;
        default:
            break;
    }
    
    if (hnd == NULL) {
        return kMediaErrorNotSupported;
    }
    
    const size_t plane0 = v.width * v.height;
    sp<Buffer> dest = new Buffer((plane0 * b->bpp) / 8);
    const size_t size_y = (plane0 * b->plane[0].bpp) / 8;
    uint8_t * dst_y     = (uint8_t *)dest->data();
    const size_t size_u = (plane0 * b->plane[1].bpp) / 8;
    uint8_t * dst_u     = dst_y + size_y;
    const size_t size_v = (plane0 * b->plane[2].bpp) / 8;
    uint8_t * dst_v     = dst_u + size_u;
    
    hnd((const uint8_t *)planes[0].data, (v.width * a->bpp) / 8,
        dst_y, v.width / b->plane[0].hss,
        dst_u, v.width / b->plane[1].hss,
        dst_v, v.width / b->plane[2].hss,
        v.width, v.height);
    
    const bool vu = v.format == kPixelFormat422YpCrCb;
    planes[0].data  = dst_y;
    planes[0].size  = size_y;
    planes[1].data  = vu ? dst_v : dst_u;
    planes[1].size  = vu ? size_v : size_u;
    planes[2].data  = vu ? dst_u : dst_v;
    planes[2].size  = vu ? size_u : size_v;
    v.format        = b->format;
    mBuffer         = dest;
    
    return kMediaNoError;
}

typedef int (*Planar2Packed_t)(const uint8_t *src_y, int src_stride_y,
                            const uint8_t *src_u, int src_stride_u,
                            const uint8_t *src_v, int src_stride_v,
                            uint8_t *dst, int dst_stride,
                            int width, int height);

typedef int (*SemiPlanar2Packed_t)(const uint8_t *src_y, int src_stride_y,
                            const uint8_t *src_uv, int src_stride_uv,
                            uint8_t *dst, int dst_stride,
                            int width, int height);


typedef int (*Packed2Packed_t)(const uint8_t *src, int src_stride,
                            uint8_t *dst, int dst_stride,
                            int width, int height);

static Planar2Packed_t get321(ePixelFormat pixel, ePixelFormat target) {
    if (target == kPixelFormatRGBA) {
        if (pixel == kPixelFormat444YpCbCrPlanar)   return libyuv::I444ToABGR;
        else                                        return libyuv::I420ToABGR;
    }
    return NULL;
}

static SemiPlanar2Packed_t get221(ePixelFormat pixel, ePixelFormat target) {
    if (target == kPixelFormatRGBA) {
        if (pixel == kPixelFormat420YpCbCrSemiPlanar)   return libyuv::NV12ToABGR;
        else                                            return libyuv::NV21ToABGR;
    }
    return NULL;
}

static Packed2Packed_t get121(ePixelFormat pixel, ePixelFormat target) {
    if (target == kPixelFormatRGBA) {
        if (pixel == kPixelFormat422YpCbCr)         return libyuv::YUY2ToARGB;
    }
    return NULL;
}

MediaError MediaFrame::yuv2rgb(const ePixelFormat& target, const eConversion&) {
    DEBUG("yuv2rgb: %s", GetImageFrameString(this).c_str());
    const PixelDescriptor * a = GetPixelFormatDescriptor(v.format);
    const PixelDescriptor * b = GetPixelFormatDescriptor(target);
    if (a->color != kColorYpCbCr || b->color != kColorRGB) {
        ERROR("invalid yuv2rgb operation: %s -> %s", a->name, b->name);
        return kMediaErrorInvalidOperation;
    }
    CHECK_EQ(b->planes, 1);
    
    switch (a->planes) {
        case 3: {
            // tri-planar -> rgb
            // LIBYUV USING word-order
            Planar2Packed_t hnd = get321(v.format, target);
            if (hnd == NULL) break;
            
            const size_t plane0 = v.width * v.height;
            sp<Buffer> rgb = new Buffer((plane0 * b->bpp) / 8);
            
            hnd(planes[0].data, v.width / a->plane[0].hss,
                planes[1].data, v.width / a->plane[1].hss,
                planes[2].data, v.width / a->plane[2].hss,
                (uint8_t *)rgb->data(), (v.width * b->bpp) / 8,
                v.width, v.height);
            
            planes[0].data  = (uint8_t *)rgb->data();
            planes[0].size  = rgb->capacity();
            planes[1].data  = NULL;
            planes[1].size  = 0;
            planes[2].data  = NULL;
            planes[2].size  = 0;
            v.format        = target;
            mBuffer         = rgb;
        } return kMediaNoError;

        case 2: {
            SemiPlanar2Packed_t hnd = get221(v.format, target);
            if (hnd == NULL) break;
            
            const size_t plane0 = v.width * v.height;
            sp<Buffer> rgb = new Buffer((plane0 * b->bpp) / 8);
            
            hnd(planes[0].data, v.width / a->plane[0].hss,
                planes[1].data, v.width / a->plane[1].hss,
                (uint8_t *)rgb->data(), (v.width * b->bpp) / 8,
                v.width, v.height);
            
            planes[0].data  = (uint8_t *)rgb->data();
            planes[0].size  = rgb->capacity();
            planes[1].data  = NULL;
            planes[1].size  = 0;
            v.format        = target;
            mBuffer         = rgb;
        } return kMediaNoError;
    
        default: {
            Packed2Packed_t hnd = get121(v.format, target);
            if (hnd == NULL) break;
            
            const size_t plane0 = v.width * v.height;
            sp<Buffer> rgb = new Buffer((plane0 * b->bpp) / 8);
            
            hnd(planes[0].data, v.width / a->plane[0].hss,
                (uint8_t *)rgb->data(), (v.width * b->bpp) / 8,
                v.width, v.height);
            
            planes[0].data  = (uint8_t *)rgb->data();
            planes[0].size  = rgb->capacity();
            v.format        = target;
            mBuffer         = rgb;
        } return kMediaNoError;
    }
    
    ERROR("yuv2rgb is not supported: %s -> %s", a->name, b->name);
    return kMediaErrorNotSupported;
}

String GetAudioFormatString(const AudioFormat& a) {
    return String::format("audio %s: ch %d, freq %d, samples %d",
                          GetSampleFormatString(a.format),
                          a.channels,
                          a.freq,
                          a.samples);
}

String GetPixelFormatString(const ePixelFormat& pixel) {
    const PixelDescriptor * desc = GetPixelFormatDescriptor(pixel);
    if (desc == NULL) return "unknown pixel";
    
    String line = String::format("pixel %s: [%zu planes %zu bpp]",
                                 desc->name, desc->planes, desc->bpp);
    return line;
}

String GetImageFormatString(const ImageFormat& image) {
    String line = GetPixelFormatString(image.format);
    line += String::format(" [%d x %d] [%d, %d, %d, %d]",
                           image.width, image.height,
                           image.rect.x, image.rect.y, image.rect.w, image.rect.h);
    return line;
}

String GetImageFrameString(const sp<MediaFrame>& frame) {
    String line = GetImageFormatString(frame->v);
    for (size_t i = 0; i < MEDIA_FRAME_NB_PLANES; ++i) {
        if (frame->planes[i].data == NULL) break;
        line += String::format(" %zu@[%zu@%p]", i, frame->planes[i].size, frame->planes[i].data);
    }
    line += String::format(", pts %" PRId64 "/%" PRId64,
                           frame->pts.value, frame->pts.timescale);
    return line;
}

sp<MediaFrame> MediaFrame::Create(const AudioFormat& a) {
    const size_t bytes = GetSampleFormatBytes(a.format);
    const size_t total = bytes * a.channels * a.samples;
    sp<MediaFrame> frame = new MediaFrame;
    frame->mBuffer = new Buffer(total);
    
    uint8_t * next = (uint8_t*)frame->mBuffer->data();
    for (size_t i = 0; i < a.channels; ++i) {
        frame->planes[i].size   = bytes * a.samples;
        frame->planes[i].data   = next;
        next += frame->planes[i].size;
    }
    
    frame->a            = a;
    return frame;
}

__END_NAMESPACE_MPX
