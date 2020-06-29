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
#include "MediaTypes.h"
#include <libyuv.h>
#ifdef __APPLE__
#include <VideoToolbox/VideoToolbox.h>
#endif

__BEGIN_DECLS

struct {
    eSampleFormat   plannar;
    eSampleFormat   packed;
} kSampleFormatMap[] = {
    { kSampleFormatU8,      kSampleFormatU8Packed   },
    { kSampleFormatS16,     kSampleFormatS16Packed  },
    { kSampleFormatS32,     kSampleFormatS32Packed  },
    { kSampleFormatFLT,     kSampleFormatFLTPacked  },
    { kSampleFormatDBL,     kSampleFormatDBLPacked  },
    // END OF LIST
    { kSampleFormatUnknown, kSampleFormatUnknown    },
};

eSampleFormat GetSimilarSampleFormat(eSampleFormat format) {
    for (size_t i = 0; kSampleFormatMap[i].plannar != kSampleFormatUnknown; ++i) {
        if (format == kSampleFormatMap[i].plannar)
            return kSampleFormatMap[i].packed;
        else if (format == kSampleFormatMap[i].packed)
            return kSampleFormatMap[i].plannar;
    }
    FATAL("SHOULD NOT BE here");
    return kSampleFormatUnknown;
};

size_t GetSampleFormatBytes(eSampleFormat format) {
    switch (format) {
        case kSampleFormatU8Packed:
        case kSampleFormatU8:       return sizeof(uint8_t);
        case kSampleFormatS16Packed:
        case kSampleFormatS16:      return sizeof(int16_t);
        case kSampleFormatS32Packed:
        case kSampleFormatS32:      return sizeof(int32_t);
        case kSampleFormatFLTPacked:
        case kSampleFormatFLT:      return sizeof(float);
        case kSampleFormatDBLPacked:
        case kSampleFormatDBL:      return sizeof(double);
        case kSampleFormatUnknown:  return 0;
        default:                    break;
    }
    FATAL("FIXME");
    return 0;
}

bool IsPackedSampleFormat(eSampleFormat format) {
    switch (format) {
        case kSampleFormatU8Packed:
        case kSampleFormatS16Packed:
        case kSampleFormatS32Packed:
        case kSampleFormatFLTPacked:
        case kSampleFormatDBLPacked:
            return true;
        default:
            return false;;
    }
}
__END_DECLS

__BEGIN_NAMESPACE_MPX

#define NB_PLANES   (8)
struct FullPlaneMediaFrame : public MediaFrame {
    MediaBuffer     extend_planes[NB_PLANES -1];    // placeholder
    sp<Buffer>      underlyingBuffer;
    
    FullPlaneMediaFrame(sp<Buffer>& buffer) : MediaFrame(), underlyingBuffer(buffer) {
        // pollute only the first plane
        planes.count                = 1;
        planes.buffers[0].capacity  = underlyingBuffer->capacity();
        planes.buffers[0].size      = underlyingBuffer->size();
        planes.buffers[0].data      = (uint8_t *)underlyingBuffer->data();  // FIXME: unsafe cast
    }
};

MediaFrame::MediaFrame() : SharedObject(), id(0),
timecode(kMediaTimeInvalid), duration(kMediaTimeInvalid),
format(0), opaque(NULL) {
    
}

sp<MediaFrame> MediaFrame::Create(size_t n) {
    sp<Buffer> buffer = new Buffer(n);
    sp<MediaFrame> frame = new FullPlaneMediaFrame(buffer);
    return frame;
}

sp<MediaFrame> MediaFrame::Create(sp<Buffer>& buffer) {
    sp<MediaFrame> frame = new FullPlaneMediaFrame(buffer);
    return frame;
}

sp<MediaFrame> MediaFrame::Create(const AudioFormat& audio) {
    const size_t bytes = GetSampleFormatBytes(audio.format);
    const size_t total = bytes * audio.channels * audio.samples;
    sp<Buffer> buffer = new Buffer(total);
    sp<MediaFrame> frame = new FullPlaneMediaFrame(buffer);
    
    if (!IsPackedSampleFormat(audio.format)) {
        // plannar samples
        frame->planes.buffers[0].size = bytes * audio.samples;
        uint8_t * next = frame->planes.buffers[0].data + frame->planes.buffers[0].size;
        for (size_t i = 1; i < audio.channels; ++i) {
            frame->planes.buffers[i].size   = frame->planes.buffers[0].size;
            frame->planes.buffers[i].data   = next;
            next += frame->planes.buffers[i].size;
        }
        frame->planes.count = audio.channels;
    }
    
    frame->audio    = audio;
    return frame;
}

sp<MediaFrame> MediaFrame::Create(const ImageFormat& image, sp<Buffer>& buffer) {
    const PixelDescriptor * desc = GetPixelFormatDescriptor(image.format);
    CHECK_NULL(desc);
    const size_t bytes = (image.width * image.height * desc->bpp) / 8;
    if (buffer->capacity() < bytes) return NULL;
    
    sp<MediaFrame> frame = new FullPlaneMediaFrame(buffer);
    
    if (desc->planes > 1) {
        frame->planes.buffers[0].size = (image.width * image.height * desc->plane[0].bpp) /
        (8 * desc->plane[0].hss * desc->plane[0].vss);
        uint8_t * next = frame->planes.buffers[0].data + frame->planes.buffers[0].size;
        for (size_t i = 1; i < desc->planes; ++i) {
            const size_t bytes = (image.width * image.height * desc->plane[i].bpp) /
                                 (8 * desc->plane[i].hss * desc->plane[i].vss);
            frame->planes.buffers[i].data   = next;
            frame->planes.buffers[i].size   = bytes;
            next += bytes;
        }
        frame->planes.count = desc->planes;
    }
    
    frame->video    = image;
    
    DEBUG("create: %s", GetImageFrameString(frame).c_str());
    return frame;
}

sp<MediaFrame> MediaFrame::Create(const ImageFormat& image) {
    const PixelDescriptor * desc = GetPixelFormatDescriptor(image.format);
    CHECK_NULL(desc);
    const size_t bytes = (image.width * image.height * desc->bpp) / 8;
    
    sp<Buffer> buffer = new Buffer(bytes);
    return MediaFrame::Create(image, buffer);
}

sp<ABuffer> MediaFrame::readPlane(size_t index) const {
    CHECK_LT(index, planes.count);
    if (planes.buffers[index].data == NULL) return NULL;
    return new Buffer((const char *)planes.buffers[index].data, planes.buffers[index].size);
}

size_t GetImageFormatPlaneLength(const ImageFormat& image, size_t i) {
    const PixelDescriptor * desc = GetPixelFormatDescriptor(image.format);
    CHECK_NULL(desc);
    return (image.width * image.height * desc->plane[i].bpp) /
            (8 * desc->plane[i].hss * desc->plane[i].vss);
}

size_t GetImageFormatBufferLength(const ImageFormat& image) {
    const PixelDescriptor * desc = GetPixelFormatDescriptor(image.format);
    CHECK_NULL(desc);
    return (image.width * image.height * desc->bpp) / 8;;
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
    switch (video.format) {
        case kPixelFormat420YpCbCrPlanar:
        case kPixelFormat422YpCbCrPlanar:
        case kPixelFormat444YpCbCrPlanar: {
            // swap u & v planes
            uint8_t * tmp0  = planes.buffers[1].data;
            size_t tmp1     = planes.buffers[1].size;
            planes.buffers[1]       = planes.buffers[2];
            planes.buffers[2].data  = tmp0;
            planes.buffers[2].size  = tmp1;
        } return kMediaNoError;
            
        case kPixelFormat420YpCbCrSemiPlanar:
        case kPixelFormat420YpCrCbSemiPlanar:
            // swap hi & low bytes of uv plane
            swap16(planes.buffers[1].data, planes.buffers[1].size);
            return kMediaNoError;
            
        case kPixelFormat422YpCbCr:
        case kPixelFormat422YpCrCb:
            // swap u & v bytes
            swap32l(planes.buffers[0].data, planes.buffers[0].size);
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
    if (planes.buffers[1].data) {   // is planar
        return kMediaErrorInvalidOperation;
    }
    
    switch (video.format) {
        case kPixelFormatRGB:
        case kPixelFormatBGR:
        case kPixelFormat444YpCbCr:
            swap24(planes.buffers[0].data, planes.buffers[0].size);
            return kMediaNoError;
            
        case kPixelFormatRGBA:
        case kPixelFormatABGR:
        case kPixelFormatARGB:
        case kPixelFormatBGRA:
        case kPixelFormat422YpCbCr:
        case kPixelFormat422YpCrCb:
            swap32(planes.buffers[0].data, planes.buffers[0].size);
            return kMediaNoError;
            
        case kPixelFormatRGB565:
        case kPixelFormatBGR565:
        default:
            break;
    }
    return kMediaErrorNotSupported;
}

static ePixelFormat GetPlanar(ePixelFormat packed) {
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
#if 0
MediaError MediaFrame::planarization() {
    ePixelFormat planar = GetPlanar(video.format);
    if (video.format == planar) return kMediaNoError;
    const PixelDescriptor * a = GetPixelFormatDescriptor(video.format);
    const PixelDescriptor * b = GetPixelFormatDescriptor(planar);
    Packed2Planar_t hnd = NULL;
    switch (video.format) {
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
    
    const size_t plane0 = video.width * video.height;
    sp<Buffer> dest = new Buffer((plane0 * b->bpp) / 8);
    const size_t size_y = (plane0 * b->plane[0].bpp) / (8 * b->plane[0].hss * b->plane[0].vss);
    uint8_t * dst_y     = (uint8_t *)dest->data();
    const size_t size_u = (plane0 * b->plane[1].bpp) / (8 * b->plane[1].hss * b->plane[1].vss);
    uint8_t * dst_u     = dst_y + size_y;
    const size_t size_v = (plane0 * b->plane[2].bpp) / (8 * b->plane[2].hss * b->plane[2].vss);
    uint8_t * dst_v     = dst_u + size_u;
    
    hnd((const uint8_t *)planes.buffers[0].data, (video.width * a->bpp) / 8,
        dst_y, (video.width * b->plane[0].bpp) / (8 * b->plane[0].hss),
        dst_u, (video.width * b->plane[1].bpp) / (8 * b->plane[1].hss),
        dst_v, (video.width * b->plane[2].bpp) / (8 * b->plane[2].hss),
        video.width, video.height);
    
    const bool vu = video.format == kPixelFormat422YpCrCb;
    planes.buffers[0].data  = dst_y;
    planes.buffers[0].size  = size_y;
    planes.buffers[1].data  = vu ? dst_v : dst_u;
    planes.buffers[1].size  = vu ? size_v : size_u;
    planes.buffers[2].data  = vu ? dst_u : dst_v;
    planes.buffers[2].size  = vu ? size_u : size_v;
    video.format        = b->format;
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

static Planar2Packed_t get321(ePixelFormat source, ePixelFormat target) {
    static struct {
        ePixelFormat    source;
        ePixelFormat    target;
        Planar2Packed_t hnd;
    } kMap[] = {
        // YpCbCrPlanar -> RGBA
        { kPixelFormat420YpCbCrPlanar,      kPixelFormatRGBA,       libyuv::I420ToABGR  },
        { kPixelFormat422YpCbCrPlanar,      kPixelFormatRGBA,       libyuv::I422ToABGR  },
        { kPixelFormat444YpCbCrPlanar,      kPixelFormatRGBA,       libyuv::I444ToABGR  },
        // YpCbCrPlanar -> ABGR (RGBA in word-order)
        { kPixelFormat420YpCbCrPlanar,      kPixelFormatABGR,       libyuv::I420ToRGBA  },
        { kPixelFormat422YpCbCrPlanar,      kPixelFormatABGR,       libyuv::I422ToRGBA  },
        { kPixelFormat444YpCbCrPlanar,      kPixelFormatABGR,       NULL                },
        // YpCbCrPlanar -> ARGB
        { kPixelFormat420YpCbCrPlanar,      kPixelFormatARGB,       libyuv::I420ToBGRA  },
        { kPixelFormat422YpCbCrPlanar,      kPixelFormatARGB,       libyuv::I422ToBGRA  },
        { kPixelFormat444YpCbCrPlanar,      kPixelFormatARGB,       NULL                },
        // YpCbCrPlanar -> BGRA (ARGB in word-order)
        { kPixelFormat420YpCbCrPlanar,      kPixelFormatBGRA,       libyuv::I420ToARGB  },
        { kPixelFormat422YpCbCrPlanar,      kPixelFormatBGRA,       libyuv::I422ToARGB  },
        { kPixelFormat444YpCbCrPlanar,      kPixelFormatBGRA,       libyuv::I444ToARGB  },
        // YpCbCrPlanar -> BGR (RGB in word-order)
        { kPixelFormat420YpCbCrPlanar,      kPixelFormatBGR,        libyuv::I420ToRGB24 },
        { kPixelFormat422YpCbCrPlanar,      kPixelFormatBGR,        NULL                },
        { kPixelFormat444YpCbCrPlanar,      kPixelFormatBGR,        NULL                },
        // YpCbCrPlanar -> BGR565 (RGB in word-order)
        { kPixelFormat420YpCbCrPlanar,      kPixelFormatBGR565,     libyuv::I420ToRGB565},
        { kPixelFormat422YpCbCrPlanar,      kPixelFormatBGR565,     libyuv::I422ToRGB565},
        { kPixelFormat444YpCbCrPlanar,      kPixelFormatBGR565,     NULL                },
        // END OF LIST
        { kPixelFormatUnknown }
    };
    
    for (size_t i = 0; kMap[i].source != kPixelFormatUnknown; ++i) {
        if (kMap[i].source == source && kMap[i].target == target) {
            return kMap[i].hnd;
        }
    }
    
    return NULL;
}

static SemiPlanar2Packed_t get221(ePixelFormat source, ePixelFormat target) {
    static struct {
        ePixelFormat        source;
        ePixelFormat        target;
        SemiPlanar2Packed_t hnd;
    } kMap[] = {
        // YpCbCrPlanar -> BGRA (ARGB in word-order)
        { kPixelFormat420YpCbCrSemiPlanar,  kPixelFormatBGRA,   libyuv::NV12ToARGB  },
        { kPixelFormat420YpCrCbSemiPlanar,  kPixelFormatBGRA,   libyuv::NV12ToARGB  },
        // YpCbCrPlanar -> ARGB (BGRA in word-order)
        { kPixelFormat420YpCbCrSemiPlanar,  kPixelFormatARGB,   NULL                },
        { kPixelFormat420YpCrCbSemiPlanar,  kPixelFormatARGB,   NULL                },
        // YpCbCrPlanar -> RGBA (ABGR in word-order)
        { kPixelFormat420YpCbCrSemiPlanar,  kPixelFormatRGBA,   libyuv::NV12ToABGR  },
        { kPixelFormat420YpCrCbSemiPlanar,  kPixelFormatRGBA,   libyuv::NV12ToABGR  },
        // YpCbCrPlanar -> ARGB (BGRA in word-order)
        { kPixelFormat420YpCbCrSemiPlanar,  kPixelFormatARGB,   NULL                },
        { kPixelFormat420YpCrCbSemiPlanar,  kPixelFormatARGB,   NULL                },
        // END OF LIST
        { kPixelFormatUnknown }
    };
    
    for (size_t i = 0; kMap[i].source != kPixelFormatUnknown; ++i) {
        if (kMap[i].source == source && kMap[i].target == target) {
            return kMap[i].hnd;
        }
    }
    return NULL;
}

static Packed2Packed_t get121(ePixelFormat source, ePixelFormat target) {
    static struct {
        ePixelFormat        source;
        ePixelFormat        target;
        Packed2Packed_t     hnd;
    } kMap[] = {
        // YpCbCrPlanar -> BGRA (ARGB in word-order)
        { kPixelFormat422YpCbCr,            kPixelFormatBGRA,   libyuv::YUY2ToARGB  },
        { kPixelFormat422YpCrCb,            kPixelFormatBGRA,   libyuv::UYVYToARGB  },
        // END OF LIST
        { kPixelFormatUnknown }
    };
    
    for (size_t i = 0; kMap[i].source != kPixelFormatUnknown; ++i) {
        if (kMap[i].source == source && kMap[i].target == target) {
            return kMap[i].hnd;
        }
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
            
            hnd(planes[0].data, (v.width * a->plane[0].bpp) / (8 * a->plane[0].hss),
                planes[1].data, (v.width * a->plane[1].bpp) / (8 * a->plane[1].hss),
                planes[2].data, (v.width * a->plane[2].bpp) / (8 * a->plane[2].hss),
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
            
            hnd(planes[0].data, (v.width * a->plane[0].bpp) / (8 * a->plane[0].hss),
                planes[1].data, (v.width * a->plane[1].bpp) / (8 * a->plane[1].hss),
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
            
            hnd(planes[0].data, (v.width * a->bpp) / 8,
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
#endif

String GetAudioFormatString(const AudioFormat& a) {
    return String::format("audio %.4s: ch %d, freq %d, samples %d",
                          (const char *)&a.format,
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
    String line = GetImageFormatString(frame->video);
    for (size_t i = 0; i < frame->planes.count; ++i) {
        if (frame->planes.buffers[i].data == NULL) break;
        line += String::format(" %zu@[%zu@%p]", i,
                               frame->planes.buffers[i].size, frame->planes.buffers[i].data);
    }
    line += String::format(", timecode %" PRId64 "/%" PRId64,
                           frame->timecode.value, frame->timecode.scale);
    return line;
}

__END_NAMESPACE_MPX
