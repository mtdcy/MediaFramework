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


/**
 * File:    ColorConverter.cpp
 * Author:  mtdcy.chen
 * Changes:
 *          1. 20200630     initial version
 *
 */

/**
 * Color Converter:
 *  Unit Types:
 *   1. packed <-> planar
 *   2. swap planes
 *   3. swap bytes
 *
 *  For YUV -> YUV
 *   cases:
 *    1. planar <-> packed.
 *    2. pixel format convertion.
 *    3. swap planes.
 *
 *  For RGB -> RGB:
 *   cases:
 *    1. pixel format convertion.
 *    2. swap bytes.
 *
 *  For YUV -> RGB:
 *   steps:
 *    1. do planes swap for planar pixel samples if neccesary.
 *    2. do packed/planar(YUV) -> packed(RGB) convertion.
 *    3. do bytes swap for packed pixel samples if neccesary.
 *
 *  For RGB -> YUV
 *   steps:
 *    1. do bytes swap for packed pixel samples if neccesary.
 *    2. do packed(RGB) -> packed/planar(YUV) convertion.
 *    3. do planes swap for planar pixel samples if neccesary.
 */

#define LOG_TAG "ColorConverter"
#define LOG_NDEBUG 0
#include "MediaTypes.h"
#include "MediaUnit.h"
#include "ColorConverter.h"
#include <libyuv.h>
#include "primitive/bswap.h"

__BEGIN_DECLS

#define PLANE_VALUE_SS1x1   { .bpp = 8,  .hss = 1, .vss = 1 }
#define PLANE_VALUE_SS2x2   { .bpp = 8,  .hss = 2, .vss = 2 }   // 2x2 subsampling, subsample both horizontal & vertical
#define PLANE_VALUE_SS2x1   { .bpp = 8,  .hss = 2, .vss = 1 }   // 2x1 subsampling, subsample only horizontal
#define PLANE_VALUE_SS1x2   { .bpp = 8,  .hss = 1, .vss = 2 }   // 1x2 subsampling, subsample only vertical

// 2 chroma in one plane
#define PLANE_VALUE_SS1x1_2 { .bpp = 16, .hss = 1, .vss = 1 }
#define PLANE_VALUE_SS2x2_2 { .bpp = 16, .hss = 2, .vss = 2 }
#define PLANE_VALUE_SS2x1_2 { .bpp = 16, .hss = 1, .vss = 2 }
#define PLANE_VALUE_SS1x2_1 { .bpp = 16, .hss = 2, .vss = 1 }

// 3 chroma in one plane
#define PLANE_VALUE_SS1x1_3 { .bpp = 24, .hss = 1, .vss = 1 }

// 4 chroma in one plane
#define PLANE_VALUE_SS1x1_4 { .bpp = 32, .hss = 1, .vss = 1 }

static const PixelDescriptor kPixel420YpCbCrPlanar = {
    .name           = "420p",
    .format         = kPixelFormat420YpCbCrPlanar,
    .similar        = { kPixelFormatUnknown, kPixelFormat420YpCrCbPlanar, kPixelFormatUnknown},
    .color          = kColorYpCbCr,
    .bpp            = 12,
    .nb_planes      = 3,
    .planes         = {
        PLANE_VALUE_SS1x1,
        PLANE_VALUE_SS2x2,
        PLANE_VALUE_SS2x2,
    }
};

static const PixelDescriptor kPixel420YpCrCbPlanar = {
    .name           = "yv12",
    .format         = kPixelFormat420YpCrCbPlanar,
    .similar        = { kPixelFormatUnknown, kPixelFormat420YpCbCrPlanar, kPixelFormatUnknown},
    .color          = kColorYpCbCr,
    .bpp            = 12,
    .nb_planes      = 3,
    .planes         = {
        PLANE_VALUE_SS1x1,
        PLANE_VALUE_SS2x2,
        PLANE_VALUE_SS2x2,
    }
};

static const PixelDescriptor kPixel422YpCbCrPlanar = {
    .name           = "422p",
    .format         = kPixelFormat422YpCbCrPlanar,
    .similar        = { kPixelFormat422YpCbCr, kPixelFormat422YpCrCbPlanar, kPixelFormatUnknown },
    .color          = kColorYpCbCr,
    .bpp            = 16,
    .nb_planes      = 3,
    .planes         = {
        PLANE_VALUE_SS1x1,
        PLANE_VALUE_SS2x1,
        PLANE_VALUE_SS2x1,
    }
};

static const PixelDescriptor kPixel422YpCrCbPlanar = {
    .name           = "422p",
    .format         = kPixelFormat422YpCrCbPlanar,
    .similar        = { kPixelFormat422YpCrCb, kPixelFormat422YpCbCrPlanar, kPixelFormatUnknown },
    .color          = kColorYpCbCr,
    .bpp            = 16,
    .nb_planes      = 3,
    .planes         = {
        PLANE_VALUE_SS1x1,
        PLANE_VALUE_SS2x1,
        PLANE_VALUE_SS2x1,
    }
};

static const PixelDescriptor kPixel444YpCbCrPlanar = {
    .name           = "444p",
    .format         = kPixelFormat444YpCbCrPlanar,
    .similar        = { kPixelFormat444YpCbCr, kPixelFormatUnknown, kPixelFormatUnknown },
    .color          = kColorYpCbCr,
    .bpp            = 24,
    .nb_planes      = 3,
    .planes         = {
        PLANE_VALUE_SS1x1,
        PLANE_VALUE_SS1x1,
        PLANE_VALUE_SS1x1,
    }
};

static const PixelDescriptor kPixel420YpCbCrSemiPlanar = {
    .name           = "nv12",
    .format         = kPixelFormat420YpCbCrSemiPlanar,
    .similar        = { kPixelFormatUnknown, kPixelFormat420YpCrCbSemiPlanar, kPixelFormatUnknown },
    .color          = kColorYpCbCr,
    .bpp            = 12,
    .nb_planes      = 2,
    .planes         = {
        PLANE_VALUE_SS1x1,
        PLANE_VALUE_SS2x2_2,    // u & v
    }
};

static const PixelDescriptor kPixel420YpCrCbSemiPlanar = {
    .name           = "nv21",
    .format         = kPixelFormat420YpCrCbSemiPlanar,
    .similar        = { kPixelFormatUnknown, kPixelFormat420YpCbCrSemiPlanar, kPixelFormatUnknown },
    .color          = kColorYpCbCr,
    .bpp            = 12,
    .nb_planes      = 2,
    .planes         = {
        PLANE_VALUE_SS1x1,
        PLANE_VALUE_SS2x2_2,    // v & u
    }
};

static const PixelDescriptor kPixel422YpCbCr = {
    .name           = "yuyv",
    .format         = kPixelFormat422YpCbCr,
    .similar        = { kPixelFormat422YpCbCrPlanar, kPixelFormat422YpCrCb, kPixelFormatUnknown },
    .color          = kColorYpCbCr,
    .bpp            = 16,
    .nb_planes      = 1,
    .planes         = {
        PLANE_VALUE_SS1x1_2,
    }
};

static const PixelDescriptor kPixel422YpCrCb = {
    .name           = "yvyu",
    .format         = kPixelFormat422YpCrCb,
    .similar        = { kPixelFormat422YpCrCbPlanar, kPixelFormat422YpCbCr, kPixelFormatUnknown },
    .color          = kColorYpCbCr,
    .bpp            = 16,
    .nb_planes      = 1,
    .planes         = {
        PLANE_VALUE_SS1x1_2,
    }
};

static const PixelDescriptor kPixel444YpCbCr = {
    .name           = "yuv444",
    .format         = kPixelFormat444YpCbCr,
    .similar        = { kPixelFormat444YpCbCrPlanar, kPixelFormatUnknown, kPixelFormatUnknown },
    .color          = kColorYpCbCr,
    .bpp            = 24,
    .nb_planes      = 1,
    .planes         = {
        PLANE_VALUE_SS1x1_3,
    }
};

static const PixelDescriptor kPixelRGB565 = {
    .name           = "RGB565",
    .format         = kPixelFormatRGB565,
    .similar        = { kPixelFormatUnknown, kPixelFormatUnknown, kPixelFormatBGR565 },
    .color          = kColorRGB,
    .bpp            = 16,
    .nb_planes      = 1,
    .planes         = {
        PLANE_VALUE_SS1x1_2,
    }
};

static const PixelDescriptor kPixelBGR565 = {
    .name           = "BGR565",
    .format         = kPixelFormatBGR565,
    .similar        = { kPixelFormatUnknown, kPixelFormatUnknown, kPixelFormatRGB565 },
    .color          = kColorRGB,
    .bpp            = 16,
    .nb_planes      = 1,
    .planes         = {
        PLANE_VALUE_SS1x1_2,
    }
};

static const PixelDescriptor kPixelRGB = {
    .name           = "RGB24",
    .format         = kPixelFormatRGB,
    .similar        = { kPixelFormatUnknown, kPixelFormatUnknown, kPixelFormatBGR },
    .color          = kColorRGB,
    .bpp            = 24,
    .nb_planes      = 1,
    .planes         = {
        PLANE_VALUE_SS1x1_3,
    }
};

static const PixelDescriptor kPixelBGR = {
    .name           = "BGR24",
    .format         = kPixelFormatBGR,
    .similar        = { kPixelFormatUnknown, kPixelFormatUnknown, kPixelFormatRGB },
    .color          = kColorRGB,
    .bpp            = 24,
    .nb_planes      = 1,
    .planes         = {
        PLANE_VALUE_SS1x1_3,
    }
};

static const PixelDescriptor kPixelARGB = {
    .name           = "ARGB",
    .format         = kPixelFormatARGB,
    .similar        = { kPixelFormatUnknown, kPixelFormatUnknown, kPixelFormatBGRA },
    .color          = kColorRGB,
    .bpp            = 32,
    .nb_planes      = 1,
    .planes         = {
        PLANE_VALUE_SS1x1_4,
    }
};

static const PixelDescriptor kPixelBGRA = {
    .name           = "BGRA",
    .format         = kPixelFormatBGRA,
    .similar        = { kPixelFormatUnknown, kPixelFormatUnknown, kPixelFormatARGB },
    .color          = kColorRGB,
    .bpp            = 32,
    .nb_planes      = 1,
    .planes         = {
        PLANE_VALUE_SS1x1_4,
    }
};

static const PixelDescriptor kPixelRGBA = {
    .name           = "RGBA",
    .format         = kPixelFormatRGBA,
    .similar        = { kPixelFormatUnknown, kPixelFormatUnknown, kPixelFormatABGR },
    .color          = kColorRGB,
    .bpp            = 32,
    .nb_planes      = 1,
    .planes         = {
        PLANE_VALUE_SS1x1_4,
    }
};

static const PixelDescriptor kPixelABGR = {
    .name           = "ABGR",
    .format         = kPixelFormatABGR,
    .similar        = { kPixelFormatUnknown, kPixelFormatUnknown, kPixelFormatRGBA },
    .color          = kColorRGB,
    .bpp            = 32,
    .nb_planes      = 1,
    .planes         = {
        PLANE_VALUE_SS1x1_4,
    }
};

static const PixelDescriptor * kPixelDescriptorList[] = {
    // tri-planar YpCbCr
    &kPixel420YpCbCrPlanar,
    &kPixel420YpCrCbPlanar,
    &kPixel422YpCbCrPlanar,
    &kPixel422YpCrCbPlanar,
    &kPixel444YpCbCrPlanar,
    // bi-planar YpCbCr
    &kPixel420YpCbCrSemiPlanar,
    &kPixel420YpCrCbSemiPlanar,
    // packed YpCbCr
    &kPixel422YpCbCr,
    &kPixel422YpCrCb,
    &kPixel444YpCbCr,
    // rgb
    &kPixelRGB565,
    &kPixelBGR565,
    &kPixelRGB,
    &kPixelBGR,
    &kPixelARGB,
    &kPixelBGRA,
    &kPixelRGBA,
    &kPixelABGR,
    // END OF LIST
    NULL
};

const PixelDescriptor * GetPixelFormatDescriptor(ePixelFormat pixel) {
    for (size_t i = 0; kPixelDescriptorList[i] != NULL; ++i) {
        const PixelDescriptor * desc = kPixelDescriptorList[i];
        if (desc->format == pixel) {
            return desc;
        }
    }
    ERROR("missing pixel descriptor for %.4s", (const char *)&pixel);
    return NULL;
}

static const ePixelFormat kPlanarYUV[] = {
    kPixelFormat420YpCbCrPlanar,
    kPixelFormat420YpCrCbPlanar,
    kPixelFormat422YpCbCrPlanar,
    kPixelFormat422YpCrCbPlanar,
    kPixelFormat444YpCbCrPlanar,
    // END OF LIST
    kPixelFormatUnknown        ,
};

static bool IsPlanarYUV(const ePixelFormat& pixel) {
    for (size_t i = 0; kPlanarYUV[i] != kPixelFormatUnknown; ++i) {
        if (kPlanarYUV[i] == pixel)
            return true;
    }
    return false;
}

static const ePixelFormat kSemiPlanarYUV[] = {
    kPixelFormat420YpCbCrSemiPlanar,
    kPixelFormat420YpCrCbSemiPlanar,
    // END OF LIST
    kPixelFormatUnknown            ,
};

static bool IsSemiPlanarYUV(const ePixelFormat& pixel) {
    for (size_t i = 0; kSemiPlanarYUV[i] != kPixelFormatUnknown; ++i) {
        if (kSemiPlanarYUV[i] == pixel)
            return true;
    }
    return false;
}

static const ePixelFormat kPackedYUV[] = {
    kPixelFormat422YpCbCr,
    kPixelFormat422YpCrCb,
    kPixelFormat444YpCbCr,
    // END OF LIST
    kPixelFormatUnknown  ,
};

static bool IsPackedYUV(const ePixelFormat& pixel) {
    for (size_t i = 0; kPackedYUV[i] != kPixelFormatUnknown; ++i) {
        if (kPackedYUV[i] == pixel)
            return true;
    }
    return false;
}

static bool IsYUV(const ePixelFormat& pixel) {
    if (IsPlanarYUV(pixel) || IsSemiPlanarYUV(pixel) || IsPackedYUV(pixel)) {
        return true;
    }
    return false;
}

static const ePixelFormat kRGB[] = {
    kPixelFormatRGB565  ,
    kPixelFormatBGR565  ,
    kPixelFormatRGB     ,
    kPixelFormatBGR     ,
    kPixelFormatARGB    ,
    kPixelFormatBGRA    ,
    kPixelFormatRGBA    ,
    kPixelFormatABGR    ,
    // END OF LIST
    kPixelFormatUnknown ,
};

static bool IsRGB(const ePixelFormat& pixel) {
    for (size_t i = 0; kRGB[i] != kPixelFormatUnknown; ++i) {
        if (kRGB[i] == pixel)
            return true;
    }
    return false;
}

// return 0 on success
typedef int (*Planar2Planar_t)(const uint8_t* src_y, int src_stride_y,
        const uint8_t* src_u, int src_stride_u,
        const uint8_t* src_v, int src_stride_v,
        uint8_t* dst_y, int dst_stride_y,
        uint8_t* dst_u, int dst_stride_u,
        uint8_t* dst_v, int dst_stride_v,
        int width, int height);

typedef int (*Planar2SemiPlanar_t)(const uint8_t* src_y, int src_stride_y,
        const uint8_t* src_u, int src_stride_u,
        const uint8_t* src_v, int src_stride_v,
        uint8_t* dst_y, int dst_stride_y,
        uint8_t* dst_vu, int dst_stride_vu,
        int width, int height);

typedef int (*Planar2Packed_t)( const uint8_t *src_y, int src_stride_y,
        const uint8_t *src_u, int src_stride_u,
        const uint8_t *src_v, int src_stride_v,
        uint8_t *dst, int dst_stride,
        int width, int height);

typedef int (*SemiPlanar2Planar_t)(const uint8_t* src_y, int src_stride_y,
        const uint8_t* src_uv, int src_stride_uv,
        uint8_t* dst_y, int dst_stride_y,
        uint8_t* dst_u, int dst_stride_u,
        uint8_t* dst_v, int dst_stride_v,
        int width, int height);

typedef int (*SemiPlanar2SemiPlanar_t)(const uint8_t* src_y, int src_stride_y,
        const uint8_t* src_vu, int src_stride_vu,
        uint8_t* dst_y, int dst_stride_y,
        uint8_t* dst_uv, int dst_stride_uv,
        int width, int height);

typedef int (*SemiPlanar2Packed_t)(const uint8_t *src_y, int src_stride_y,
        const uint8_t *src_uv, int src_stride_uv,
        uint8_t *dst, int dst_stride,
        int width, int height);

typedef int (*Packed2Packed_t)(const uint8_t *src, int src_stride,
        uint8_t *dst, int dst_stride,
        int width, int height);

typedef int (*Packed2Planar_t)(const uint8_t* src, int src_stride,
        uint8_t* dst_y, int dst_stride_y,
        uint8_t* dst_u, int dst_stride_u,
        uint8_t* dst_v, int dst_stride_v,
        int width, int height);

typedef int (*Packed2SemiPlanar_t)(const uint8_t* src_argb, int src_stride_argb,
        uint8_t* dst_y, int dst_stride_y,
        uint8_t* dst_uv, int dst_stride_uv,
        int width, int height);

typedef int (*ByteSwap_t)(const uint8_t * src, size_t src_bytes,
        uint8_t * dst, size_t dst_bytes);

union hnd_t {
    Planar2Planar_t         planar2planar;
    Planar2SemiPlanar_t     planar2semiplanar;
    Planar2Packed_t         planar2packed;
    SemiPlanar2Planar_t     semiplanar2planar;
    SemiPlanar2SemiPlanar_t semiplanar2semiplanar;
    SemiPlanar2Packed_t     semiplanar2packed;
    Packed2Planar_t         packed2planar;
    Packed2SemiPlanar_t     packed2semiplanar;
    Packed2Packed_t         packed2packed;
    ByteSwap_t              byteswap;
};

// aabb -> bbaa
static int swap16(const uint8_t * src, size_t src_bytes,
        uint8_t * dst, size_t dst_bytes) {
    uint16_t * from = (uint16_t *)src;
    uint16_t * to = (uint16_t *)dst;
    for (size_t i = 0; i < src_bytes; i += sizeof(uint16_t)) {
        *to++ = bswap16(*from++);
    }
    return 0;
}

static int swap16_565(const uint8_t * src, size_t src_bytes,
        uint8_t * dst, size_t dst_bytes) {
    uint16_t * from = (uint16_t *)src;
    uint16_t * to = (uint16_t *)dst;
    for (size_t i = 0; i < src_bytes; i += sizeof(uint16_t)) {
        uint16_t x = *from++;
        uint8_t r, g, b;
        r = (x & 0xf800) >> 11;
        g = (x & 0x7E0) >> 5;
        b = (x & 0x1F);
        *to++ = ((b << 11) | (g << 5) | r);
    }
    return 0;
}

// aabbcc -> ccbbaa
static int swap24(const uint8_t * src, size_t src_bytes,
        uint8_t * dst, size_t dst_bytes) {
    for (size_t i = 0; i < src_bytes; i += 3) {
        // support inplace swap
        uint8_t a   = src[0];
        uint8_t b   = src[2];
        dst[0]      = b;
        dst[2]      = a;
        src         += 3;
        dst         += 3;
    }
    return 0;
}

// aabbccdd -> ddccbbaa
static int swap32(const uint8_t * src, size_t src_bytes,
        uint8_t * dst, size_t dst_bytes) {
    uint32_t * from = (uint32_t *)src;
    uint32_t * to = (uint32_t *)dst;
    for (size_t i = 0; i < src_bytes; i += sizeof(uint32_t)) {
        *to++ = bswap32(*from++);
    }
    return 0;
}

// aabbccdd -> aaddccbb
static int swap32l(const uint8_t * src, size_t src_bytes,
        uint8_t * dst, size_t dst_bytes) {
    uint32_t * from = (uint32_t *)src;
    uint32_t * to = (uint32_t *)dst;
    for (size_t i = 0; i < src_bytes; i += sizeof(uint32_t)) {
        *to++ = bswap32l(*from++);
    }
    return 0;
}

__END_DECLS

__BEGIN_NAMESPACE_MPX

typedef struct convert_t {
    const ePixelFormat      source;
    const ePixelFormat      target;
    const hnd_t             hnd;
} convert_t;

static const convert_t kTo420YpCbCrPlanar[] = {
    { kPixelFormat422YpCbCrPlanar,      kPixelFormat420YpCbCrPlanar,    .hnd.planar2planar = libyuv::I422ToI420         },
    { kPixelFormat444YpCbCrPlanar,      kPixelFormat420YpCbCrPlanar,    .hnd.planar2planar = libyuv::I444ToI420         },
    { kPixelFormat420YpCbCrSemiPlanar,  kPixelFormat420YpCbCrPlanar,    .hnd.semiplanar2planar = libyuv::NV12ToI420     },
    { kPixelFormat420YpCrCbSemiPlanar,  kPixelFormat420YpCbCrPlanar,    .hnd.semiplanar2planar = libyuv::NV21ToI420     },
    { kPixelFormat422YpCbCr,            kPixelFormat420YpCbCrPlanar,    .hnd.packed2planar = libyuv::YUY2ToI420         },
    { kPixelFormatBGRA,                 kPixelFormat420YpCbCrPlanar,    .hnd.packed2planar = libyuv::ARGBToI420         },
    { kPixelFormatARGB,                 kPixelFormat420YpCbCrPlanar,    .hnd.packed2planar = libyuv::BGRAToI420         },
    { kPixelFormatRGBA,                 kPixelFormat420YpCbCrPlanar,    .hnd.packed2planar = libyuv::ABGRToI420         },
    { kPixelFormatABGR,                 kPixelFormat420YpCbCrPlanar,    .hnd.packed2planar = libyuv::RGBAToI420         },
    { kPixelFormatBGR,                  kPixelFormat420YpCbCrPlanar,    .hnd.packed2planar = libyuv::RGB24ToI420        },
    { kPixelFormatBGR565,               kPixelFormat420YpCbCrPlanar,    .hnd.packed2planar = libyuv::RGB565ToI420       },
    // END OF LIST
    { kPixelFormatUnknown }
};

static const convert_t kToRGB32[] = {
    { kPixelFormat420YpCbCrPlanar,      kPixelFormatRGB32,              .hnd.planar2packed = libyuv::I420ToARGB         },
    { kPixelFormat422YpCbCrPlanar,      kPixelFormatRGB32,              .hnd.planar2packed = libyuv::I422ToARGB         },
    { kPixelFormat444YpCbCrPlanar,      kPixelFormatRGB32,              .hnd.planar2packed = libyuv::I444ToARGB         },
    { kPixelFormat420YpCbCrSemiPlanar,  kPixelFormatRGB32,              .hnd.semiplanar2packed = libyuv::NV12ToARGB     },
    { kPixelFormat420YpCrCbSemiPlanar,  kPixelFormatRGB32,              .hnd.semiplanar2packed = libyuv::NV21ToARGB     },
    { kPixelFormat422YpCbCr,            kPixelFormatRGB32,              .hnd.packed2packed = libyuv::YUY2ToARGB         },
    { kPixelFormatARGB,                 kPixelFormatRGB32,              .hnd.packed2packed = libyuv::BGRAToARGB         },
    { kPixelFormatRGBA,                 kPixelFormatRGB32,              .hnd.packed2packed = libyuv::ABGRToARGB         },
    { kPixelFormatABGR,                 kPixelFormatRGB32,              .hnd.packed2packed = libyuv::RGBAToARGB         },
    { kPixelFormatBGR,                  kPixelFormatRGB32,              .hnd.packed2packed = libyuv::RGB24ToARGB        },
    { kPixelFormatBGR565,               kPixelFormatRGB32,              .hnd.packed2packed = libyuv::RGB565ToARGB       },
    // END OF LIST
    { kPixelFormatUnknown }
};

static const convert_t kToRGB24[] = {
    { kPixelFormat420YpCbCrPlanar,      kPixelFormatRGB24,              .hnd.planar2packed = libyuv::I420ToRGB24        },
    { kPixelFormat420YpCbCrSemiPlanar,  kPixelFormatRGB24,              .hnd.semiplanar2packed = libyuv::NV12ToRGB24    },
    { kPixelFormat420YpCrCbSemiPlanar,  kPixelFormatRGB24,              .hnd.semiplanar2packed = libyuv::NV21ToRGB24    },
    // END OF LIST
    { kPixelFormatUnknown }
};

static const convert_t kToRGB16[] = {
    { kPixelFormat420YpCbCrPlanar,      kPixelFormatRGB16,              .hnd.planar2packed = libyuv::I420ToRGB565       },
    { kPixelFormat420YpCbCrSemiPlanar,  kPixelFormatRGB16,              .hnd.semiplanar2packed = libyuv::NV12ToRGB565   },
    { kPixelFormat420YpCrCbSemiPlanar,  kPixelFormatRGB16,              .hnd.semiplanar2packed = libyuv::NV12ToRGB565   },
    // END OF LIST
    { kPixelFormatUnknown }
};

static hnd_t get_convert_hnd(const convert_t list[], const ePixelFormat& source, const ePixelFormat& target) {
    for (size_t i = 0; list[i].source != kPixelFormatUnknown; ++i) {
        if (list[i].source == source && list[i].target == target) {
            return list[i].hnd;
        }
    }
    hnd_t hnd = { .planar2planar = NULL };
    return hnd;
}

struct ColorConvertorContext : public SharedObject {
    MediaFormat             ipf;
    MediaFormat             opf;
    const PixelDescriptor * ipd;
    const PixelDescriptor * opd;
    hnd_t                   hnd;
};

static MediaUnitContext colorconvertor_alloc() {
    sp<ColorConvertorContext> ccc = new ColorConvertorContext;
    return ccc->RetainObject();
}

static void colorconvertor_dealloc(MediaUnitContext ref) {
    sp<ColorConvertorContext> ccc = ref;
    ccc->ReleaseObject();
}

static MediaError colorconvertor_init_common(sp<ColorConvertorContext>& ccc, const MediaFormat * iformat, const MediaFormat * oformat) {
    // input & output are the same pixel format
    if (iformat->format == oformat->format) {
        ERROR("same pixel format");
        return kMediaErrorBadParameters;
    }
    // input & output SHOULD have the same width & height
    if (iformat->video.width == 0 || iformat->video.height == 0 ||
            iformat->video.width != oformat->video.width ||
            iformat->video.height != iformat->video.height) {
        ERROR("bad pixel dimention");
        return kMediaErrorBadParameters;
    }

    ccc->ipf    = *iformat;
    ccc->opf    = *oformat;
    ccc->ipd    = GetPixelFormatDescriptor(iformat->format);
    ccc->opd    = GetPixelFormatDescriptor(oformat->format);
    return kMediaNoError;
}

static MediaError colorconvertor_init_420p(MediaUnitContext ref, const MediaFormat * iformat, const MediaFormat * oformat) {
    DEBUG("cc init %s => %s", GetImageFormatString(iformat->video).c_str(), GetImageFormatString(oformat->video).c_str());
    sp<ColorConvertorContext> ccc = ref;
    if (colorconvertor_init_common(ccc, iformat, oformat) != kMediaNoError) {
        return kMediaErrorBadParameters;
    }
    
    ccc->hnd = get_convert_hnd(kTo420YpCbCrPlanar, iformat->format, oformat->format);
    return kMediaNoError;
}

static MediaError colorconvertor_init_rgb32(MediaUnitContext ref, const MediaFormat * iformat, const MediaFormat * oformat) {
    DEBUG("cc init %s => %s", GetImageFormatString(iformat->video).c_str(), GetImageFormatString(oformat->video).c_str());
    sp<ColorConvertorContext> ccc = ref;
    if (colorconvertor_init_common(ccc, iformat, oformat) != kMediaNoError) {
        return kMediaErrorBadParameters;
    }
    
    ccc->hnd = get_convert_hnd(kToRGB32, iformat->format, oformat->format);
    return kMediaNoError;
}

static MediaError colorconvertor_init_rgb24(MediaUnitContext ref, const MediaFormat * iformat, const MediaFormat * oformat) {
    DEBUG("cc init %s => %s", GetImageFormatString(iformat->video).c_str(), GetImageFormatString(oformat->video).c_str());
    sp<ColorConvertorContext> ccc = ref;
    if (colorconvertor_init_common(ccc, iformat, oformat) != kMediaNoError) {
        return kMediaErrorBadParameters;
    }
    
    ccc->hnd = get_convert_hnd(kToRGB24, iformat->format, oformat->format);
    return kMediaNoError;
}

static MediaError colorconvertor_init_rgb16(MediaUnitContext ref, const MediaFormat * iformat, const MediaFormat * oformat) {
    DEBUG("cc init %s => %s", GetImageFormatString(iformat->video).c_str(), GetImageFormatString(oformat->video).c_str());
    sp<ColorConvertorContext> ccc = ref;
    if (colorconvertor_init_common(ccc, iformat, oformat) != kMediaNoError) {
        return kMediaErrorBadParameters;
    }
    
    ccc->hnd = get_convert_hnd(kToRGB16, iformat->format, oformat->format);
    return kMediaNoError;
}

MediaError colorconvertor_process(MediaUnitContext ref, const MediaBufferList * input, MediaBufferList * output) {
    DEBUG("process: %s", GetMediaBufferListString(*input).c_str());
    sp<ColorConvertorContext> ccc = ref;
    const PixelDescriptor * ipd = ccc->ipd;
    const PixelDescriptor * opd = ccc->opd;
    const MediaFormat& ipf      = ccc->ipf;
    
    if (input->count != ipd->nb_planes || output->count != opd->nb_planes) {
        return kMediaErrorBadParameters;
    }
    
    const uint32_t width    = ipf.video.width;
    const uint32_t height   = ipf.video.height;
    // check input size
    for (size_t i = 0; i < ipd->nb_planes; ++i) {
        const size_t size = (width * height * ipd->planes[i].bpp) / (8 * ipd->planes[i].hss * ipd->planes[i].vss);
        if (input->buffers[i].size < size) {
            ERROR("bad input buffer, size mismatch.");
            return kMediaErrorBadParameters;
        }
    }
    
    // check output capacity
    for (size_t i = 0; i < opd->nb_planes; ++i) {
        const size_t size = (width * height * opd->planes[i].bpp) / (8 * opd->planes[i].hss * opd->planes[i].vss);
        if (output->buffers[i].capacity < size) {
            ERROR("bad output buffer, capacity mismatch");
            return kMediaErrorBadParameters;
        }
        output->buffers[i].size = size; // set output size
    }
    
    switch (ipd->nb_planes) {
        case 3:
            switch (opd->nb_planes) {
                case 3:
                    ccc->hnd.planar2planar(input->buffers[0].data, (width * ipd->planes[0].bpp) / (8 * ipd->planes[0].hss),
                                           input->buffers[1].data, (width * ipd->planes[1].bpp) / (8 * ipd->planes[1].hss),
                                           input->buffers[2].data, (width * ipd->planes[2].bpp) / (8 * ipd->planes[2].hss),
                                           output->buffers[0].data, (width * opd->planes[0].bpp) / (8 * opd->planes[0].hss),
                                           output->buffers[1].data, (width * opd->planes[1].bpp) / (8 * opd->planes[1].hss),
                                           output->buffers[2].data, (width * opd->planes[2].bpp) / (8 * opd->planes[2].hss),
                                           width, height);
                    break;
                case 2:
                    ccc->hnd.planar2semiplanar(input->buffers[0].data, (width * ipd->planes[0].bpp) / (8 * ipd->planes[0].hss),
                                               input->buffers[1].data, (width * ipd->planes[1].bpp) / (8 * ipd->planes[1].hss),
                                               input->buffers[2].data, (width * ipd->planes[2].bpp) / (8 * ipd->planes[2].hss),
                                               output->buffers[0].data, (width * opd->planes[0].bpp) / (8 * opd->planes[0].hss),
                                               output->buffers[1].data, (width * opd->planes[1].bpp) / (8 * opd->planes[1].hss),
                                               width, height);
                    break;
                case 1:
                    ccc->hnd.planar2packed(input->buffers[0].data, (width * ipd->planes[0].bpp) / (8 * ipd->planes[0].hss),
                                           input->buffers[1].data, (width * ipd->planes[1].bpp) / (8 * ipd->planes[1].hss),
                                           input->buffers[2].data, (width * ipd->planes[2].bpp) / (8 * ipd->planes[2].hss),
                                           output->buffers[0].data, (width * opd->bpp) / 8,
                                           width, height);
                    break;
                default:
                    break;
            } break;
        case 2:
            switch (opd->nb_planes) {
                case 3:
                    ccc->hnd.semiplanar2planar(input->buffers[0].data, (width * ipd->planes[0].bpp) / (8 * ipd->planes[0].hss),
                                               input->buffers[1].data, (width * ipd->planes[1].bpp) / (8 * ipd->planes[1].hss),
                                               output->buffers[0].data, (width * opd->planes[0].bpp) / (8 * opd->planes[0].hss),
                                               output->buffers[1].data, (width * opd->planes[1].bpp) / (8 * opd->planes[1].hss),
                                               output->buffers[2].data, (width * opd->planes[2].bpp) / (8 * opd->planes[2].hss),
                                               width, height);
                    break;
                case 2:
                    ccc->hnd.semiplanar2semiplanar(input->buffers[0].data, (width * ipd->planes[0].bpp) / (8 * ipd->planes[0].hss),
                                                   input->buffers[1].data, (width * ipd->planes[1].bpp) / (8 * ipd->planes[1].hss),
                                                   output->buffers[0].data, (width * opd->planes[0].bpp) / (8 * opd->planes[0].hss),
                                                   output->buffers[1].data, (width * opd->planes[1].bpp) / (8 * opd->planes[1].hss),
                                                   width, height);
                    break;
                case 1:
                    ccc->hnd.semiplanar2packed(input->buffers[0].data, (width * ipd->planes[0].bpp) / (8 * ipd->planes[0].hss),
                                               input->buffers[1].data, (width * ipd->planes[1].bpp) / (8 * ipd->planes[1].hss),
                                               output->buffers[0].data, (width * opd->bpp) / 8,
                                               width, height);
                    break;
                default:
                    break;
            } break;
        case 1:
            switch (opd->nb_planes) {
                case 3:
                    ccc->hnd.packed2planar(input->buffers[0].data, (width * ipd->bpp) / 8,
                                           output->buffers[0].data, (width * opd->planes[0].bpp) / (8 * opd->planes[0].hss),
                                           output->buffers[1].data, (width * opd->planes[1].bpp) / (8 * opd->planes[1].hss),
                                           output->buffers[2].data, (width * opd->planes[2].bpp) / (8 * opd->planes[2].hss),
                                           width, height);
                    break;
                case 2:
                    ccc->hnd.packed2semiplanar(input->buffers[0].data, (width * ipd->bpp) / 8,
                                               output->buffers[0].data, (width * opd->planes[0].bpp) / (8 * opd->planes[0].hss),
                                               output->buffers[1].data, (width * opd->planes[1].bpp) / (8 * opd->planes[1].hss),
                                               width, height);
                    break;
                case 1:
                    ccc->hnd.packed2packed(input->buffers[0].data, (width * ipd->bpp) / 8,
                                           output->buffers[0].data, (width * opd->bpp) / 8,
                                           width, height);
                    break;
                default:
                    break;
            } break;
        default:    break;
    }
    
    DEBUG("process: => %s", GetMediaBufferListString(*output).c_str());
    return kMediaNoError;
}

#define COLORCONVERTER420P(TYPE)                                                                \
static const MediaUnit kConvert##TYPE##To420p = {                                               \
    .name       = #TYPE">420p",                                                                 \
    .flags      = 0,                                                                            \
    .iformats   = (const ePixelFormat[]){ kPixelFormat##TYPE, kPixelFormatUnknown },            \
    .oformats   = (const ePixelFormat[]){ kPixelFormat420YpCbCrPlanar, kPixelFormatUnknown },   \
    .alloc      = colorconvertor_alloc,                                                         \
    .dealloc    = colorconvertor_dealloc,                                                       \
    .init       = colorconvertor_init_420p,                                                     \
    .process    = colorconvertor_process,                                                       \
    .reset      = NULL,                                                                         \
};
COLORCONVERTER420P(422YpCbCrPlanar)
COLORCONVERTER420P(444YpCbCrPlanar)
COLORCONVERTER420P(420YpCbCrSemiPlanar)
COLORCONVERTER420P(420YpCrCbSemiPlanar)
COLORCONVERTER420P(422YpCbCr)
COLORCONVERTER420P(BGRA)
COLORCONVERTER420P(ARGB)
COLORCONVERTER420P(RGBA)
COLORCONVERTER420P(ABGR)
COLORCONVERTER420P(BGR)
COLORCONVERTER420P(BGR565)

#define COLORCONVERTERRGB32(TYPE)                                                               \
static const MediaUnit kConvert##TYPE##ToRGB32 = {                                              \
    .name       = #TYPE">RGB32",                                                                \
    .flags      = 0,                                                                            \
    .iformats   = (const ePixelFormat[]){ kPixelFormat##TYPE, kPixelFormatUnknown },            \
    .oformats   = (const ePixelFormat[]){ kPixelFormatRGB32, kPixelFormatUnknown },             \
    .alloc      = colorconvertor_alloc,                                                         \
    .dealloc    = colorconvertor_dealloc,                                                       \
    .init       = colorconvertor_init_rgb32,                                                    \
    .process    = colorconvertor_process,                                                       \
    .reset      = NULL,                                                                         \
};
COLORCONVERTERRGB32(420YpCbCrPlanar)
COLORCONVERTERRGB32(422YpCbCrPlanar)
COLORCONVERTERRGB32(444YpCbCrPlanar)
COLORCONVERTERRGB32(420YpCbCrSemiPlanar)
COLORCONVERTERRGB32(420YpCrCbSemiPlanar)
COLORCONVERTERRGB32(422YpCbCr)
COLORCONVERTERRGB32(ARGB)
COLORCONVERTERRGB32(RGBA)
COLORCONVERTERRGB32(ABGR)
COLORCONVERTERRGB32(BGR)
COLORCONVERTERRGB32(BGR565)

#define COLORCONVERTERRGB24(TYPE)                                                               \
static const MediaUnit kConvert##TYPE##ToRGB24 = {                                              \
    .name       = #TYPE">RGB24",                                                                \
    .flags      = 0,                                                                            \
    .iformats   = (const ePixelFormat[]){ kPixelFormat##TYPE, kPixelFormatUnknown },            \
    .oformats   = (const ePixelFormat[]){ kPixelFormatRGB24, kPixelFormatUnknown },             \
    .alloc      = colorconvertor_alloc,                                                         \
    .dealloc    = colorconvertor_dealloc,                                                       \
    .init       = colorconvertor_init_rgb24,                                                    \
    .process    = colorconvertor_process,                                                       \
    .reset      = NULL,                                                                         \
};
COLORCONVERTERRGB24(420YpCbCrPlanar)
COLORCONVERTERRGB24(420YpCbCrSemiPlanar)
COLORCONVERTERRGB24(420YpCrCbSemiPlanar)

#define COLORCONVERTERRGB16(TYPE)                                                               \
static const MediaUnit kConvert##TYPE##ToRGB16 = {                                              \
    .name       = #TYPE">RGB16",                                                                \
    .flags      = 0,                                                                            \
    .iformats   = (const ePixelFormat[]){ kPixelFormat##TYPE, kPixelFormatUnknown },            \
    .oformats   = (const ePixelFormat[]){ kPixelFormatRGB16, kPixelFormatUnknown },             \
    .alloc      = colorconvertor_alloc,                                                         \
    .dealloc    = colorconvertor_dealloc,                                                       \
    .init       = colorconvertor_init_rgb16,                                                    \
    .process    = colorconvertor_process,                                                       \
    .reset      = NULL,                                                                         \
};
COLORCONVERTERRGB16(420YpCbCrPlanar)
COLORCONVERTERRGB16(420YpCbCrSemiPlanar)
COLORCONVERTERRGB16(420YpCrCbSemiPlanar)

static MediaError planeswap_init(MediaUnitContext ref, const MediaFormat * iformat, const MediaFormat * oformat) {
    sp<ColorConvertorContext> ccc = ref;
    if (colorconvertor_init_common(ccc, iformat, oformat) != kMediaNoError) {
        return kMediaErrorBadParameters;
    }
    if (!IsPlanarYUV(iformat->format)) {
        ERROR("try plane swap on non-planar yuv");
        return kMediaErrorBadParameters;
    }
    // input & output SHOULD have the same planes count
    if (ccc->ipd->nb_planes < 3 || ccc->ipd->nb_planes != ccc->opd->nb_planes) {
        ERROR("bad pixel plane");
        return kMediaErrorBadParameters;
    }
    return kMediaNoError;
}

static MediaError planeswap_process(MediaUnitContext ref, const MediaBufferList * input, MediaBufferList * output) {
    sp<ColorConvertorContext> ccc = ref;
    if (input->count != 3 || input->count != output->count) {
        return kMediaErrorBadParameters;
    }
    for (size_t i = 0; i < input->count; ++i) {
        if (input->buffers[i].size > output->buffers[i].capacity)
            return kMediaErrorBadParameters;
    }
    // support inplace process
    // swap u/v
    MediaBuffer a       = input->buffers[1];
    MediaBuffer b       = input->buffers[2];
    output->buffers[1]  = b;
    output->buffers[2]  = a;
    return kMediaNoError;
}

#define PLANESWAP(FROM, TO)                                                         \
static const MediaUnit kPlaneSwap##FROM##To##TO = {                                 \
    .name       = "PlaneSwap "#FROM">"#TO,                                          \
    .flags      = kMediaUnitProcessInplace,                                         \
    .iformats   = (const ePixelFormat[]){ kPixelFormat##FROM, kPixelFormatUnknown },\
    .oformats   = (const ePixelFormat[]){ kPixelFormat##TO, kPixelFormatUnknown },  \
    .alloc      = colorconvertor_alloc,                                             \
    .dealloc    = colorconvertor_dealloc,                                           \
    .init       = planeswap_init,                                                   \
    .process    = planeswap_process,                                                \
    .reset      = NULL,                                                             \
};
PLANESWAP(420YpCbCrPlanar, 420YpCrCbPlanar)
PLANESWAP(420YpCrCbPlanar, 420YpCbCrPlanar)
PLANESWAP(422YpCbCrPlanar, 422YpCrCbPlanar)
PLANESWAP(422YpCrCbPlanar, 422YpCbCrPlanar)

static const struct {
    const ePixelFormat  source;
    const ePixelFormat  target;
    const ByteSwap_t    hnd;
} kByteSwapMap[] = {
    { kPixelFormat420YpCbCrSemiPlanar,  kPixelFormat420YpCrCbSemiPlanar,    swap16      },
    { kPixelFormat420YpCrCbSemiPlanar,  kPixelFormat420YpCbCrSemiPlanar,    swap16      },
    { kPixelFormat422YpCbCr,            kPixelFormat422YpCrCb,              swap32l     },
    { kPixelFormat422YpCrCb,            kPixelFormat422YpCbCr,              swap32l     },
    { kPixelFormatRGB565,               kPixelFormatBGR565,                 swap16_565  },
    { kPixelFormatBGR565,               kPixelFormatRGB565,                 swap16_565  },
    { kPixelFormatRGB,                  kPixelFormatBGR,                    swap24      },
    { kPixelFormatBGR,                  kPixelFormatRGB,                    swap24      },
    { kPixelFormatRGBA,                 kPixelFormatABGR,                   swap32      },
    { kPixelFormatABGR,                 kPixelFormatRGBA,                   swap32      },
    { kPixelFormatARGB,                 kPixelFormatBGRA,                   swap32      },
    { kPixelFormatBGRA,                 kPixelFormatARGB,                   swap32      },
    // END OF LIST
    { kPixelFormatUnknown }
};
static ByteSwap_t hnd_byteswap(ePixelFormat source, ePixelFormat target) {
    for (size_t i = 0; kByteSwapMap[i].source != kPixelFormatUnknown; ++i) {
        if (kByteSwapMap[i].source == source && kByteSwapMap[i].target == target) {
            return kByteSwapMap[i].hnd;
        }
    }
    return NULL;
}

static MediaError byteswap_init(MediaUnitContext ref, const MediaFormat * iformat, const MediaFormat * oformat) {
    sp<ColorConvertorContext> ccc = ref;
    if (colorconvertor_init_common(ccc, iformat, oformat) != kMediaNoError) {
        return kMediaErrorBadParameters;
    }
    // input & output SHOULD have the same planes count
    if (ccc->ipd->nb_planes != ccc->opd->nb_planes) {
        return kMediaErrorBadParameters;
    }
    ccc->hnd.byteswap = hnd_byteswap(iformat->format, oformat->format);
    if (ccc->hnd.byteswap == NULL) {
        return kMediaErrorNotSupported;
    }
    return kMediaNoError;
}

static MediaError byteswap_process(MediaUnitContext ref, const MediaBufferList * input, MediaBufferList * output) {
    sp<ColorConvertorContext> ccc = ref;
    if (input->count != output->count) {
        ERROR("bad %s => %s", GetMediaBufferListString(*input).c_str(), GetMediaBufferListString(*output).c_str());
        return kMediaErrorBadParameters;
    }
    if (input->buffers[0].size > output->buffers[0].capacity) {
        ERROR("bad %s => %s", GetMediaBufferListString(*input).c_str(), GetMediaBufferListString(*output).c_str());
        return kMediaErrorBadParameters;
    }
    if (IsSemiPlanarYUV(ccc->ipf.format)) {
        // swap u&v bytes in semi-planar yuv
        if (output->buffers[0].data != input->buffers[0].data) {
            libyuv::CopyPlane(input->buffers[0].data,
                    (ccc->ipf.video.width * ccc->ipd->planes[0].bpp) / (8 * ccc->ipd->planes[0].hss),
                    output->buffers[0].data,
                    (ccc->ipf.video.width * ccc->opd->planes[0].bpp) / (8 * ccc->opd->planes[0].hss),
                    ccc->ipf.video.width,
                    ccc->ipf.video.height);
            output->buffers[0].size = input->buffers[0].size;
        } else { // in-place processing
            output->buffers[0] = input->buffers[0];
        }
        ccc->hnd.byteswap(input->buffers[1].data, input->buffers[1].size,
                output->buffers[1].data, output->buffers[1].capacity);
        output->buffers[1].size = input->buffers[1].size;
    } else {
        ccc->hnd.byteswap(input->buffers[0].data, input->buffers[0].size,
                output->buffers[0].data, output->buffers[0].capacity);
        output->buffers[0].size = input->buffers[0].size;
    }
    return kMediaNoError;
}

#define BYTESWAP(FROM, TO)                                                          \
static const MediaUnit kByteSwap##FROM##To##TO = {                                  \
    .name       = "ByteSwap "#FROM">"#TO,                                           \
    .flags      = kMediaUnitProcessInplace,                                         \
    .iformats   = (const ePixelFormat[]){ kPixelFormat##FROM, kPixelFormatUnknown}, \
    .oformats   = (const ePixelFormat[]){ kPixelFormat##TO, kPixelFormatUnknown},   \
    .alloc      = colorconvertor_alloc,                                             \
    .dealloc    = colorconvertor_dealloc,                                           \
    .init       = byteswap_init,                                                    \
    .process    = byteswap_process,                                                 \
    .reset      = NULL,                                                             \
};
BYTESWAP(420YpCbCrSemiPlanar, 420YpCrCbSemiPlanar)  // nv12 -> nv21
BYTESWAP(420YpCrCbSemiPlanar, 420YpCbCrSemiPlanar)  // nv21 -> nv12
BYTESWAP(422YpCbCr, 422YpCrCb)
BYTESWAP(422YpCrCb, 422YpCbCr)
BYTESWAP(RGB565, BGR565)
BYTESWAP(BGR565, RGB565)
BYTESWAP(RGB, BGR)
BYTESWAP(BGR, RGB)
BYTESWAP(RGBA, ABGR)
BYTESWAP(ABGR, RGBA)
BYTESWAP(ARGB, BGRA)
BYTESWAP(BGRA, ARGB)

static const MediaUnit * kColorUnitList[] = {
    // direct convert; these units MUST place at HEAD
    &kConvert422YpCbCrTo420p,
    &kConvert444YpCbCrPlanarTo420p,
    &kConvert420YpCbCrSemiPlanarTo420p,
    &kConvert420YpCrCbSemiPlanarTo420p,
    &kConvert422YpCbCrTo420p,
    &kConvertBGRATo420p,
    &kConvertARGBTo420p,
    &kConvertRGBATo420p,
    &kConvertABGRTo420p,
    &kConvertBGRTo420p,
    &kConvertBGR565To420p,
    &kConvert420YpCbCrPlanarToRGB32,
    &kConvert422YpCbCrPlanarToRGB32,
    &kConvert444YpCbCrPlanarToRGB32,
    &kConvert420YpCbCrSemiPlanarToRGB32,
    &kConvert420YpCrCbSemiPlanarToRGB32,
    &kConvert422YpCbCrToRGB32,
    &kConvertARGBToRGB32,
    &kConvertRGBAToRGB32,
    &kConvertABGRToRGB32,
    &kConvertBGRToRGB32,
    &kConvertBGR565ToRGB32,
    &kConvert420YpCbCrPlanarToRGB24,
    &kConvert420YpCbCrSemiPlanarToRGB24,
    &kConvert420YpCrCbSemiPlanarToRGB24,
    &kConvert420YpCbCrPlanarToRGB16,
    &kConvert420YpCbCrSemiPlanarToRGB16,
    &kConvert420YpCrCbSemiPlanarToRGB16,
    // plane swap
    &kPlaneSwap420YpCbCrPlanarTo420YpCrCbPlanar,
    &kPlaneSwap420YpCrCbPlanarTo420YpCbCrPlanar,
    &kPlaneSwap422YpCbCrPlanarTo422YpCrCbPlanar,
    &kPlaneSwap422YpCrCbPlanarTo422YpCbCrPlanar,
    // byte swap
    &kByteSwap420YpCbCrSemiPlanarTo420YpCrCbSemiPlanar,
    &kByteSwap420YpCrCbSemiPlanarTo420YpCbCrSemiPlanar,
    &kByteSwap422YpCbCrTo422YpCrCb,
    &kByteSwap422YpCrCbTo422YpCbCr,
    &kByteSwapRGB565ToBGR565,
    &kByteSwapBGR565ToRGB565,
    &kByteSwapRGBToBGR,
    &kByteSwapBGRToRGB,
    &kByteSwapRGBAToABGR,
    &kByteSwapABGRToRGBA,
    &kByteSwapARGBToBGRA,
    &kByteSwapBGRAToARGB,
    // END OF LIST
    NULL
};

static FORCE_INLINE bool PixelFormatContains(const ePixelFormat * list, const ePixelFormat& pixel) {
    for (size_t i = 0; list[i] != kPixelFormatUnknown; ++i) {
        if (list[i] == pixel) {
            return true;
        }
    }
    return false;
}

static const MediaUnit * ColorUnitFind(const MediaUnit * list[],
                                       const ePixelFormat& iformat,
                                       const ePixelFormat& oformat) {
    for (size_t i = 0; list[i] != NULL; ++i) {
        if (PixelFormatContains(list[i]->iformats, iformat) &&
            PixelFormatContains(list[i]->oformats, oformat)) {
            return list[i];
        }
    }
    ERROR("no unit for %.4s => %.4s", (const char *)&iformat, (const char *)&oformat);
    return NULL;
}

static const MediaUnit * ColorUnitNew(const MediaUnit * list[],
                                      const ImageFormat& iformat,
                                      const ImageFormat& oformat,
                                      MediaUnitContext * p) {
    CHECK_NULL(p);
    const MediaUnit * unit = ColorUnitFind(list, iformat.format, oformat.format);
    if (!unit) return NULL;
    
    DEBUG("found unit %s", unit->name);
    MediaUnitContext instance = unit->alloc();
    MediaError st = unit->init(instance, (const MediaFormat*)&iformat, (const MediaFormat*)&oformat);
 
    if (st != kMediaNoError) {
        unit->dealloc(instance);
        ERROR("unit init failed for %s => %s",
              GetImageFormatString(iformat).c_str(), GetImageFormatString(oformat).c_str());
        return NULL;
    }
    *p = instance;
    return unit;
}

// predefined color unit graph
static const ePixelFormat * kColorUnitGraphList[] = {
    (const ePixelFormat[]){
        kPixelFormat420YpCrCbPlanar,
        kPixelFormat420YpCbCrPlanar,
        kPixelFormatBGRA,
        kPixelFormatUnknown
    },
    (const ePixelFormat[]){
        kPixelFormat422YpCrCbPlanar,    // -> plane swap
        kPixelFormat422YpCbCrPlanar,
        kPixelFormat420YpCbCrPlanar,
        kPixelFormatUnknown     // END
    },
};

static const ePixelFormat * ColorUnitGraphFind(const ePixelFormat& iformat, const ePixelFormat& oformat) {
    for (size_t i = 0; kColorUnitGraphList[i] != NULL; ++i) {
        if (kColorUnitGraphList[i][0] == iformat) {
            // possible graph
            size_t j = 0;
            while (kColorUnitGraphList[i][j+1] != kPixelFormatUnknown) ++j;
            if (kColorUnitGraphList[i][j] == oformat) {
                return kColorUnitGraphList[i];
            }
        }
    }
    return NULL;
}

struct ColorConverter : public MediaDevice {
    ImageFormat                 mInput;
    ImageFormat                 mOutput;
    
    struct Node {
        const MediaUnit *       unit;
        MediaUnitContext        instance;
        ePixelFormat            input;
        ePixelFormat            output;
    };
    Vector<Node>                mUnits;
    sp<MediaFrame>              mFrame;

    ColorConverter() : MediaDevice() { }
    
    virtual ~ColorConverter() {
        for (size_t i = 0; i < mUnits.size(); ++i) {
            mUnits[i].unit->dealloc(mUnits[i].instance);
        }
        mUnits.clear();
    }
    
    MediaError init(const ImageFormat& iformat, const ImageFormat& oformat, const sp<Message>&) {
        DEBUG("init ColorConverter: %s => %s", GetImageFormatString(iformat).c_str(), GetImageFormatString(oformat).c_str());
        mInput      = iformat;
        mOutput     = oformat;
        
        // direct convert
        MediaUnitContext instance;
        const MediaUnit * unit = ColorUnitNew(kColorUnitList, iformat, oformat, &instance);
        if (unit) {
            DEBUG("new unit %.4s => %.4s", (const char *)&iformat.format, (const char *)&oformat.format);
            Node node = { unit, instance, iformat.format, oformat.format };
            mUnits.push(node);
            return kMediaNoError;
        }
        
        // complex color unit graph
        const ePixelFormat * graph = ColorUnitGraphFind(iformat.format, oformat.format);
        if (!graph) {
            return kMediaErrorBadParameters;
        }
        ImageFormat format0 = iformat;
        for (size_t i = 1; graph[i] != kPixelFormatUnknown; ++i) {
            ImageFormat format1 = format0;
            format1.format      = graph[i];
            DEBUG("init unit %.4s => %.4s", (const char *)&format0.format, (const char *)&format1.format);
            
            MediaUnitContext instance;
            const MediaUnit * unit = ColorUnitNew(kColorUnitList,
                                                  format0,
                                                  format1,
                                                  &instance);
            if (!unit) {
                ERROR("init color unit graph failed");
                return kMediaErrorUnknown;
            }
            
            Node node = { unit, instance, format0.format, format1.format };
            mUnits.push(node);
            format0 = format1;
        }
        
        ERROR("init ColorConverter failed");
        return kMediaErrorBadParameters;
    }
    
    virtual sp<Message> formats() const {
        sp<Message> format = new Message;
        format->setInt32(kKeyFormat, mOutput.format);
        format->setInt32(kKeyWidth, mOutput.width);
        format->setInt32(kKeyHeight, mOutput.width);
        return format;
    }
    
    virtual MediaError configure(const sp<Message>& options) {
        return kMediaErrorNotSupported;
    }
    
    virtual MediaError push(const sp<MediaFrame>& input) {
        if (input.isNIL()) return kMediaNoError;
        
        if (mFrame != NULL) return kMediaErrorResourceBusy;
        
        // copy input properties
        ImageFormat image       = mOutput;
        image.width             = input->video.width;
        image.height            = input->video.height;
        image.rect              = input->video.rect;
        
        sp<MediaFrame> output   = MediaFrame::Create(image);
        
        MediaError st = mUnits[0].unit->process(mUnits[0].instance,
                                                &input->planes,
                                                &output->planes);
        
        if (st != kMediaNoError) {
            ERROR("push %s failed", input->string().c_str());
            return kMediaErrorUnknown;
        }
        
        output->id          = input->id;
        output->flags       = input->flags;
        output->timecode    = input->timecode;
        output->duration    = input->duration;
        mFrame              = output;
        return kMediaNoError;
    }
    
    virtual sp<MediaFrame> pull() {
        sp<MediaFrame> frame = mFrame;
        mFrame.clear();
        return frame;
    }
    
    virtual MediaError reset() {
        for (size_t i = 0; i < mUnits.size(); ++i) {
            if (mUnits[i].unit->reset) {
                mUnits[i].unit->reset(mUnits[i].instance);
            }
        }
        return kMediaNoError;
    }
};

sp<MediaDevice> CreateColorConverter(const ImageFormat& iformat, const ImageFormat& oformat, const sp<Message>& options) {
    sp<ColorConverter> cc = new ColorConverter;
    if (cc->init(iformat, oformat, options) != kMediaNoError) {
        return NULL;
    }
    return cc;
}

__END_NAMESPACE_MPX

__BEGIN_DECLS

__END_DECLS
