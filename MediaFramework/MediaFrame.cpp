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

sp<Buffer> MediaFrame::getData(size_t index) const {
    if (planes[index].data == NULL) return NULL;
    return new Buffer((const char *)planes[index].data, planes[index].size);
}

sp<MediaFrame> MediaFrame::Create(const ImageFormat& image) {
    const size_t bytes = GetImageFormatBytes(&image);
    Object<Buffer> buffer = new Buffer(bytes);
    return MediaFrame::Create(image, buffer);
}

sp<MediaFrame> MediaFrame::Create(const ImageFormat& image, const sp<Buffer>& buffer) {
    const size_t bytes = GetImageFormatBytes(&image);
    if (buffer->capacity() < bytes) return NULL;
    
    Object<MediaFrame> frame = new MediaFrame;
    frame->mBuffer = buffer;
    
    if (GetPixelFormatIsPlanar(image.format)) {
        uint8_t * next = (uint8_t*)frame->mBuffer->data();
        for (size_t i = 0; i < GetPixelFormatPlanes(image.format); ++i) {
            const size_t bpp = GetPixelFormatPlaneBPP(image.format, i);
            const size_t bytes = (image.width * image.height * bpp) / 8;
            frame->planes[i].data   = next;
            frame->planes[i].size   = bytes;
            next += bytes;
        }
    } else {
        frame->planes[0].data   = (uint8_t*)frame->mBuffer->data();
        frame->planes[0].size   = bytes;
    }
    
    frame->v    = image;
    return frame;
}

static FORCE_INLINE void swapPlanes(MediaFrame * image, size_t a, size_t b) {
    uint8_t * tmp0 = image->planes[a].data;
    size_t tmp1 = image->planes[a].size;
    image->planes[a] = image->planes[b];
    image->planes[b].data = tmp0;
    image->planes[b].size = tmp1;
}

// TODO: borrow some code from ffmpeg
static FORCE_INLINE uint16_t swap16(uint16_t x) {
    return (x >> 8) | (x << 8);
}

static FORCE_INLINE void swapBytes(MediaFrame * image, size_t index) {
    uint16_t * data = (uint16_t *)image->planes[index].data;
    for (size_t j = 0; j < image->planes[index].size / 2; ++j) {
        *data = swap16(*data);
        ++data;
    }
}

MediaError MediaFrame::swapUVChroma() {
    switch (v.format) {
        case kPixelFormatYUV420P:
        case kPixelFormatYUV422P:
        case kPixelFormatYUV444P:
            swapPlanes(this, 1, 2);
            return kMediaNoError;
            
        case kPixelFormatNV12:
        case kPixelFormatNV21:
            swapBytes(this, 1);
            return kMediaNoError;
            
        case kPixelFormatRGB565:
        case kPixelFormatRGB888:
        case kPixelFormatRGBA:
        case kPixelFormatARGB:
            return kMediaErrorInvalidOperation;
            
        default:
            break;
    }
    return kMediaErrorNotSupported;
}

static FORCE_INLINE void reverse3(uint8_t * u8, size_t size) {
    CHECK_EQ(size % 3, 0);
    for (size_t i = 0; i < size / 3; ++i) {
        uint8_t tmp = u8[0];
        u8[0] = u8[2];
        u8[2] = tmp;
        u8 += 3;
    }
}

static FORCE_INLINE uint32_t reverse32(uint32_t x) {
    return ((x >> 24) & 0xff) | ((x >> 8) & 0xff00) | ((x << 8) & 0xff0000) | ((x << 24) & 0xff000000);
}
static FORCE_INLINE void reverse4(uint8_t * u8, size_t size) {
    uint32_t *u32 = (uint32_t *)u8;
    CHECK_EQ(size % 4, 0);
    for (size_t i = 0; i < size / 4; ++i) {
        *u32 = reverse32(*u32);
        ++u32;
    }
}

MediaError MediaFrame::reverseBytes() {
    if (GetPixelFormatIsPlanar(v.format)) {
        return kMediaErrorInvalidOperation;
    }
    
    switch (v.format) {
        case kPixelFormatRGB888:
        case kPixelFormatYUV444:
            reverse3(planes[0].data, planes[0].size);
            return kMediaNoError;
            
        case kPixelFormatRGBA:
        case kPixelFormatARGB:
        case kPixelFormatYUYV422:
        case kPixelFormatYVYU422:
            reverse4(planes[0].data, planes[0].size);
            return kMediaNoError;
            
        case kPixelFormatRGB565:
        default:
            break;
    }
    return kMediaErrorNotSupported;
}

MediaError MediaFrame::planarization() {
    INFO("planarization @ %s", GetPixelFormatString(v.format));
    if (GetPixelFormatIsPlanar(v.format)) {
        return kMediaNoError;
    }
    
    switch (v.format) {
        case kPixelFormatYVYU422:
        case kPixelFormatYUYV422: {
            const size_t plane0 = v.width * v.height;
            sp<Buffer> dest = new Buffer(plane0 * 2);
            CHECK_EQ(dest->capacity(), planes[0].size);
            
            uint8_t * dst_y = (uint8_t *)dest->data();
            uint8_t * dst_u = dst_y + plane0;
            uint8_t * dst_v = dst_u + plane0 / 2;
            libyuv::YUY2ToI422(planes[0].data, v.width * 2,
                               dst_y, v.width,
                               dst_u, v.width / 2,
                               dst_v, v.width / 2,
                               v.width, v.height);
            
            const bool uv   = v.format == kPixelFormatYVYU422;
            planes[0].data  = dst_y;
            planes[0].size  = plane0;
            planes[1].data  = uv ? dst_v : dst_u;
            planes[1].size  = plane0 / 2;
            planes[2].data  = uv ? dst_u : dst_v;
            planes[2].size  = plane0 / 2;
            v.format        = kPixelFormatYUV422P;
            mBuffer         = dest;
            return kMediaNoError;
        }
        default:
            break;
    }
    return kMediaErrorNotSupported;
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