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
    .name           = "yv16",
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
    .similar        = { kPixelFormat444YpCbCr, kPixelFormat444YpCrCbPlanar, kPixelFormatUnknown },
    .color          = kColorYpCbCr,
    .bpp            = 24,
    .nb_planes      = 3,
    .planes         = {
        PLANE_VALUE_SS1x1,
        PLANE_VALUE_SS1x1,
        PLANE_VALUE_SS1x1,
    }
};

static const PixelDescriptor kPixel444YpCrCbPlanar = {
    .name           = "yv24",
    .format         = kPixelFormat444YpCrCbPlanar,
    .similar        = { kPixelFormatUnknown, kPixelFormat444YpCbCrPlanar, kPixelFormatUnknown },
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
    .similar        = { kPixelFormat422YpCrCbPlanar, kPixelFormat422YpCbCr, kPixelFormat422YpCrCbWO },
    .color          = kColorYpCbCr,
    .bpp            = 16,
    .nb_planes      = 1,
    .planes         = {
        PLANE_VALUE_SS1x1_2,
    }
};

static const PixelDescriptor kPixel422YpCbCrWO = {
    .name           = "vyuy",
    .format         = kPixelFormat422YpCbCrWO,
    .similar        = { kPixelFormat422YpCbCrPlanar, kPixelFormat422YpCrCbWO, kPixelFormat422YpCbCr },
    .color          = kColorYpCbCr,
    .bpp            = 16,
    .nb_planes      = 1,
    .planes         = {
        PLANE_VALUE_SS1x1_2,
    }
};

static const PixelDescriptor kPixel422YpCrCbWO = {
    .name           = "uyvy",
    .format         = kPixelFormat422YpCrCbWO,
    .similar        = { kPixelFormat422YpCrCbPlanar, kPixelFormat422YpCbCrWO, kPixelFormat422YpCrCb },
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
    .name           = "RGB888",
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
    .name           = "BGR888",
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
    &kPixel422YpCbCrWO,
    &kPixel422YpCrCbWO,
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
    Nil
};

const PixelDescriptor * GetPixelFormatDescriptor(ePixelFormat pixel) {
    for (UInt32 i = 0; kPixelDescriptorList[i] != Nil; ++i) {
        const PixelDescriptor * desc = kPixelDescriptorList[i];
        if (desc->format == pixel) {
            return desc;
        }
    }
    ERROR("missing pixel descriptor for %.4s", (const Char *)&pixel);
    return Nil;
}

const PixelDescriptor * GetPixelFormatDescriptorByName(const Char * _name) {
    const String name = _name;
    for (UInt32 i = 0; kPixelDescriptorList[i] != Nil; ++i) {
        const PixelDescriptor * desc = kPixelDescriptorList[i];
        if (name == desc->name) {
            return desc;
        }
    }
    ERROR("no pixel descriptor named %s", _name);
    return Nil;
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

static const ePixelFormat kSemiPlanarYUV[] = {
    kPixelFormat420YpCbCrSemiPlanar,
    kPixelFormat420YpCrCbSemiPlanar,
    // END OF LIST
    kPixelFormatUnknown            ,
};

static const ePixelFormat kPackedYUV[] = {
    kPixelFormat422YpCbCr,
    kPixelFormat422YpCrCb,
    kPixelFormat422YpCbCrWO,
    kPixelFormat422YpCrCbWO,
    kPixelFormat444YpCbCr,
    // END OF LIST
    kPixelFormatUnknown  ,
};

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

static Bool ContainsPixelFormat(const ePixelFormat list[], ePixelFormat pixel) {
    for (UInt32 i = 0; list[i] != kPixelFormatUnknown; ++i) {
        if (list[i] == pixel)
            return True;
    }
    return False;
}

static Bool IsPlanarYUV(ePixelFormat pixel) {
    return ContainsPixelFormat(kPlanarYUV, pixel);
}

static Bool IsSemiPlanarYUV(ePixelFormat pixel) {
    return ContainsPixelFormat(kSemiPlanarYUV, pixel);
}

static Bool IsPackedYUV(ePixelFormat pixel) {
    return ContainsPixelFormat(kPackedYUV, pixel);
}

static Bool IsRGB(ePixelFormat pixel) {
    return ContainsPixelFormat(kRGB, pixel);
}

// return 0 on success
typedef Int (*Planar2Planar_t)(const UInt8* src_y, Int src_stride_y,
        const UInt8* src_u, Int src_stride_u,
        const UInt8* src_v, Int src_stride_v,
        UInt8* dst_y, Int dst_stride_y,
        UInt8* dst_u, Int dst_stride_u,
        UInt8* dst_v, Int dst_stride_v,
        Int width, Int height);

typedef Int (*Planar2SemiPlanar_t)(const UInt8* src_y, Int src_stride_y,
        const UInt8* src_u, Int src_stride_u,
        const UInt8* src_v, Int src_stride_v,
        UInt8* dst_y, Int dst_stride_y,
        UInt8* dst_vu, Int dst_stride_vu,
        Int width, Int height);

typedef Int (*Planar2Packed_t)( const UInt8 *src_y, Int src_stride_y,
        const UInt8 *src_u, Int src_stride_u,
        const UInt8 *src_v, Int src_stride_v,
        UInt8 *dst, Int dst_stride,
        Int width, Int height);

typedef Int (*Planar2PackedMatrix_t)(const UInt8* src_y, Int src_stride_y,
        const UInt8* src_u, Int src_stride_u,
        const UInt8* src_v, Int src_stride_v,
        UInt8* dst_argb, Int dst_stride_argb,
        const struct libyuv::YuvConstants* yuvconstants,
        Int width, Int height);

typedef Int (*SemiPlanar2Planar_t)(const UInt8* src_y, Int src_stride_y,
        const UInt8* src_uv, Int src_stride_uv,
        UInt8* dst_y, Int dst_stride_y,
        UInt8* dst_u, Int dst_stride_u,
        UInt8* dst_v, Int dst_stride_v,
        Int width, Int height);

typedef Int (*SemiPlanar2SemiPlanar_t)(const UInt8* src_y, Int src_stride_y,
        const UInt8* src_vu, Int src_stride_vu,
        UInt8* dst_y, Int dst_stride_y,
        UInt8* dst_uv, Int dst_stride_uv,
        Int width, Int height);

typedef Int (*SemiPlanar2Packed_t)(const UInt8 *src_y, Int src_stride_y,
        const UInt8 *src_uv, Int src_stride_uv,
        UInt8 *dst, Int dst_stride,
        Int width, Int height);

typedef Int (*SemiPlanar2PackedMatrix_t)(const UInt8* src_y, Int src_stride_y,
        const UInt8* src_uv, Int src_stride_uv,
        UInt8* dst_argb, Int dst_stride_argb,
        const struct libyuv::YuvConstants* yuvconstants,
        Int width, Int height);

typedef Int (*Packed2Packed_t)(const UInt8 *src, Int src_stride,
        UInt8 *dst, Int dst_stride,
        Int width, Int height);

typedef Int (*Packed2PackedMatrix_t)(const UInt8 *src, Int src_stride,
        UInt8 *dst, Int dst_stride,
        const struct libyuv::YuvConstants* yuvconstants,
        Int width, Int height);

typedef Int (*Packed2Planar_t)(const UInt8* src, Int src_stride,
        UInt8* dst_y, Int dst_stride_y,
        UInt8* dst_u, Int dst_stride_u,
        UInt8* dst_v, Int dst_stride_v,
        Int width, Int height);

typedef Int (*Packed2SemiPlanar_t)(const UInt8* src_argb, Int src_stride_argb,
        UInt8* dst_y, Int dst_stride_y,
        UInt8* dst_uv, Int dst_stride_uv,
        Int width, Int height);

union hnd_t {
    Planar2Planar_t             planar2planar;
    Planar2SemiPlanar_t         planar2semiplanar;
    Planar2Packed_t             planar2packed;
    Planar2PackedMatrix_t       planar2packedMAT;
    SemiPlanar2Planar_t         semiplanar2planar;
    SemiPlanar2SemiPlanar_t     semiplanar2semiplanar;
    SemiPlanar2Packed_t         semiplanar2packed;
    SemiPlanar2PackedMatrix_t   semiplanar2packedMAT;
    Packed2Planar_t             packed2planar;
    Packed2SemiPlanar_t         packed2semiplanar;
    Packed2Packed_t             packed2packed;
    Packed2PackedMatrix_t       packed2packedMAT;
};

// aabb -> bbaa
static Int swap16(const UInt8 * src, UInt32 src_bytes,
        UInt8 * dst, UInt32 dst_bytes) {
    UInt16 * from = (UInt16 *)src;
    UInt16 * to = (UInt16 *)dst;
    for (UInt32 i = 0; i < src_bytes; i += sizeof(UInt16)) {
        *to++ = bswap16(*from++);
    }
    return 0;
}

static Int swap16_565(const UInt8 * src, UInt32 src_bytes,
        UInt8 * dst, UInt32 dst_bytes) {
    UInt16 * from = (UInt16 *)src;
    UInt16 * to = (UInt16 *)dst;
    for (UInt32 i = 0; i < src_bytes; i += sizeof(UInt16)) {
        UInt16 x = *from++;
        UInt8 r, g, b;
        r = (x & 0xf800) >> 11;
        g = (x & 0x7E0) >> 5;
        b = (x & 0x1F);
        *to++ = ((b << 11) | (g << 5) | r);
    }
    return 0;
}

// aabbcc -> ccbbaa
static Int swap24(const UInt8 * src, UInt32 src_bytes,
        UInt8 * dst, UInt32 dst_bytes) {
    for (UInt32 i = 0; i < src_bytes; i += 3) {
        // support inplace swap
        UInt8 a   = src[0];
        UInt8 b   = src[2];
        dst[0]      = b;
        dst[2]      = a;
        src         += 3;
        dst         += 3;
    }
    return 0;
}

// aabbccdd -> ddccbbaa
static Int swap32(const UInt8 * src, UInt32 src_bytes,
        UInt8 * dst, UInt32 dst_bytes) {
    UInt32 * from = (UInt32 *)src;
    UInt32 * to = (UInt32 *)dst;
    for (UInt32 i = 0; i < src_bytes; i += sizeof(UInt32)) {
        *to++ = bswap32(*from++);
    }
    return 0;
}

// aabbccdd -> aaddccbb
static Int swap32l(const UInt8 * src, UInt32 src_bytes,
        UInt8 * dst, UInt32 dst_bytes) {
    UInt32 * from = (UInt32 *)src;
    UInt32 * to = (UInt32 *)dst;
    for (UInt32 i = 0; i < src_bytes; i += sizeof(UInt32)) {
        *to++ = bswap32l(*from++);
    }
    return 0;
}

__END_DECLS

__BEGIN_NAMESPACE_MPX

enum {
    SWAP_UV         = (1<<0),   // swap uv planes before/after process, only work for planar pixels
    BSWAP_INPUT     = (1<<1),   // do byte swap on input, @see swap16/swap24/swap32
    BSWAP_OUTPUT    = (1<<2),   // do byte swap on output, @see swap16/swap24/swap32
    BSWAP_32L       = (1<<3),   // do byte swap on low byte, @see swap32l
    BSWAP_32H       = (1<<4),   // do byte swap on high byte, @see swap32h
    
    //
    COLOR_MATRIX    = (1<<8),   // handle support color matrix
};

typedef struct convert_t {
    const ePixelFormat      source;
    const hnd_t             hnd;
    const UInt32          flags;
} convert_t;

// ? -> 420YpCbCr (420p)
// this one SHOULD have full capability
static const convert_t kTo420YpCbCrPlanar[] = {
    { kPixelFormat420YpCrCbPlanar,      .hnd.planar2planar = libyuv::I420Copy,      .flags = SWAP_UV    },
    { kPixelFormat422YpCbCrPlanar,      .hnd.planar2planar = libyuv::I422ToI420,    .flags = 0          },
    { kPixelFormat422YpCrCbPlanar,      .hnd.planar2planar = libyuv::I422ToI420,    .flags = SWAP_UV    },
    { kPixelFormat444YpCbCrPlanar,      .hnd.planar2planar = libyuv::I444ToI420,    .flags = 0          },
    { kPixelFormat444YpCrCbPlanar,      .hnd.planar2planar = libyuv::I444ToI420,    .flags = SWAP_UV    },
    { kPixelFormat420YpCbCrSemiPlanar,  .hnd.semiplanar2planar = libyuv::NV12ToI420                     },
    { kPixelFormat420YpCrCbSemiPlanar,  .hnd.semiplanar2planar = libyuv::NV21ToI420                     },
    { kPixelFormat422YpCbCr,            .hnd.packed2planar = libyuv::YUY2ToI420                         },
    { kPixelFormat422YpCrCb,            .hnd.packed2planar = libyuv::YUY2ToI420,    .flags = SWAP_UV    },
    { kPixelFormat422YpCrCbWO,          .hnd.packed2planar = libyuv::UYVYToI420                         },
    { kPixelFormat422YpCbCrWO,          .hnd.packed2planar = libyuv::UYVYToI420,    .flags = SWAP_UV    },
    { kPixelFormatBGRA,                 .hnd.packed2planar = libyuv::ARGBToI420                         },
    { kPixelFormatARGB,                 .hnd.packed2planar = libyuv::BGRAToI420                         },
    { kPixelFormatRGBA,                 .hnd.packed2planar = libyuv::ABGRToI420                         },
    { kPixelFormatABGR,                 .hnd.packed2planar = libyuv::RGBAToI420                         },
    { kPixelFormatBGR,                  .hnd.packed2planar = libyuv::RGB24ToI420                        },
    { kPixelFormatRGB,                  .hnd.packed2planar = libyuv::RGB24ToI420,   .flags = SWAP_UV    },
    { kPixelFormatBGR565,               .hnd.packed2planar = libyuv::RGB565ToI420                       },
    { kPixelFormatRGB565,               .hnd.packed2planar = libyuv::RGB565ToI420,  .flags = SWAP_UV    },
    // END OF LIST
    { kPixelFormatUnknown }
};

// ? -> ARGB in word-order
// this one SHOULD have full capability
static const convert_t kToBGRA[] = {
    { kPixelFormat420YpCbCrPlanar,      .hnd.planar2packedMAT = libyuv::I420ToARGBMatrix,   .flags = COLOR_MATRIX           },
    { kPixelFormat420YpCrCbPlanar,      .hnd.planar2packedMAT = libyuv::I420ToARGBMatrix,   .flags = COLOR_MATRIX|SWAP_UV   },
    { kPixelFormat422YpCbCrPlanar,      .hnd.planar2packedMAT = libyuv::I422ToARGBMatrix,   .flags = COLOR_MATRIX           },
    { kPixelFormat422YpCrCbPlanar,      .hnd.planar2packedMAT = libyuv::I422ToARGBMatrix,   .flags = COLOR_MATRIX|SWAP_UV   },
    { kPixelFormat444YpCbCrPlanar,      .hnd.planar2packedMAT = libyuv::I444ToARGBMatrix,   .flags = COLOR_MATRIX           },
    { kPixelFormat444YpCrCbPlanar,      .hnd.planar2packedMAT = libyuv::I444ToARGBMatrix,   .flags = COLOR_MATRIX|SWAP_UV   },
    { kPixelFormat420YpCbCrSemiPlanar,  .hnd.semiplanar2packedMAT = libyuv::NV12ToARGBMatrix,   .flags = COLOR_MATRIX       },
    { kPixelFormat420YpCrCbSemiPlanar,  .hnd.semiplanar2packedMAT = libyuv::NV21ToARGBMatrix,   .flags = COLOR_MATRIX       },
    { kPixelFormat422YpCbCr,            .hnd.packed2packedMAT = libyuv::YUY2ToARGBMatrix,   .flags = COLOR_MATRIX           },
    { kPixelFormat422YpCrCb,            .hnd.packed2packedMAT = libyuv::YUY2ToARGBMatrix,   .flags = COLOR_MATRIX|BSWAP_32L },
    { kPixelFormat422YpCrCbWO,          .hnd.packed2packedMAT = libyuv::UYVYToARGBMatrix,   .flags = COLOR_MATRIX           },
    { kPixelFormat422YpCbCrWO,          .hnd.packed2packedMAT = libyuv::UYVYToARGBMatrix,   .flags = COLOR_MATRIX|BSWAP_32L },
    { kPixelFormat444YpCbCr,            .hnd.packed2packed = Nil                                       },  // does this format real exist?
    { kPixelFormatBGRA,                 .hnd.packed2packed = libyuv::ARGBCopy                           },
    { kPixelFormatARGB,                 .hnd.packed2packed = libyuv::BGRAToARGB                         },
    { kPixelFormatRGBA,                 .hnd.packed2packed = libyuv::ABGRToARGB                         },
    { kPixelFormatABGR,                 .hnd.packed2packed = libyuv::RGBAToARGB                         },
    { kPixelFormatBGR,                  .hnd.packed2packed = libyuv::RGB24ToARGB                        },
    { kPixelFormatRGB,                  .hnd.packed2packed = libyuv::RGB24ToARGB,   .flags = BSWAP_32L  },
    { kPixelFormatBGR565,               .hnd.packed2packed = libyuv::RGB565ToARGB                       },
    { kPixelFormatRGB565,               .hnd.packed2packed = libyuv::RGB565ToARGB,  .flags = BSWAP_32L  },
    // END OF LIST
    { kPixelFormatUnknown }
};

// ? -> RGBA in byte-order
// XXXToARGB can be used in here by swap u/v or r/b components of input & output
#define YVYUToABGR      YUY2ToARGB
#define VYUYToABGR      UYVYToARGB
#define BGRAToABGR      RGBAToARGB
#define RGBAToABGR      BGRAToARGB
#define BGR24ToABGR     RGB24ToARGB
#define BGR565ToABGR    RGB565ToARGB
static const convert_t kToRGBA[] = {
    { kPixelFormat420YpCbCrPlanar,      .hnd.planar2packed = libyuv::I420ToABGR                             },
    { kPixelFormat420YpCrCbPlanar,      .hnd.planar2packed = libyuv::I420ToABGR,        .flags = SWAP_UV    },
    { kPixelFormat422YpCbCrPlanar,      .hnd.planar2packed = libyuv::I422ToABGR                             },
    { kPixelFormat422YpCrCbPlanar,      .hnd.planar2packed = libyuv::I422ToABGR,        .flags = SWAP_UV    },
    { kPixelFormat444YpCbCrPlanar,      .hnd.planar2packed = libyuv::I444ToABGR                             },
    { kPixelFormat444YpCbCrPlanar,      .hnd.planar2packed = libyuv::I444ToABGR,        .flags = SWAP_UV    },
    { kPixelFormat420YpCbCrSemiPlanar,  .hnd.semiplanar2packed = libyuv::NV12ToABGR                         },
    { kPixelFormat420YpCrCbSemiPlanar,  .hnd.semiplanar2packed = libyuv::NV21ToABGR                         },
    { kPixelFormat422YpCrCb,            .hnd.packed2packed = libyuv::YVYUToABGR                             },
    { kPixelFormat422YpCbCr,            .hnd.packed2packed = libyuv::YVYUToABGR,        .flags = BSWAP_32L  },
    { kPixelFormat422YpCbCrWO,          .hnd.packed2packed = libyuv::VYUYToABGR                             },
    { kPixelFormat422YpCrCbWO,          .hnd.packed2packed = libyuv::VYUYToABGR,        .flags = BSWAP_32L  },
    { kPixelFormatBGRA,                 .hnd.packed2packed = libyuv::ARGBToABGR                             },
    { kPixelFormatARGB,                 .hnd.packed2packed = libyuv::BGRAToABGR                             },
    { kPixelFormatABGR,                 .hnd.packed2packed = libyuv::RGBAToABGR                             },
    { kPixelFormatRGB,                  .hnd.packed2packed = libyuv::BGR24ToABGR                            },
    { kPixelFormatBGR,                  .hnd.packed2packed = libyuv::BGR24ToABGR,       .flags = BSWAP_32L  },
    { kPixelFormatRGB565,               .hnd.packed2packed = libyuv::BGR565ToABGR                           },
    { kPixelFormatRGB565,               .hnd.packed2packed = libyuv::BGR565ToABGR,      .flags = BSWAP_32L  },
    // END OF LIST
    { kPixelFormatUnknown }
};

// RGB in word-order
static const convert_t kToBGR[] = {
    { kPixelFormat420YpCbCrPlanar,      .hnd.planar2packed = libyuv::I420ToRGB24        },
    { kPixelFormat420YpCbCrSemiPlanar,  .hnd.semiplanar2packed = libyuv::NV12ToRGB24    },
    { kPixelFormat420YpCrCbSemiPlanar,  .hnd.semiplanar2packed = libyuv::NV21ToRGB24    },
    // END OF LIST
    { kPixelFormatUnknown }
};

//
static const convert_t kToRGB16[] = {
    { kPixelFormat420YpCbCrPlanar,      .hnd.planar2packed = libyuv::I420ToRGB565       },
    { kPixelFormat420YpCbCrSemiPlanar,  .hnd.semiplanar2packed = libyuv::NV12ToRGB565   },
    { kPixelFormat420YpCrCbSemiPlanar,  .hnd.semiplanar2packed = libyuv::NV12ToRGB565   },
    // END OF LIST
    { kPixelFormatUnknown }
};

static hnd_t get_convert_hnd(const convert_t list[], const ePixelFormat& source, UInt32 * flags) {
    for (UInt32 i = 0; list[i].source != kPixelFormatUnknown; ++i) {
        if (list[i].source == source) {
            *flags = list[i].flags;
            return list[i].hnd;
        }
    }
    hnd_t hnd = { .planar2planar = Nil };
    return hnd;
}

static const libyuv::YuvConstants * GetLibyuvMatrix(eColorMatrix matrix) {
    switch (matrix) {
        case kColorMatrixJPEG:      return &libyuv::kYuvJPEGConstants;
        case kColorMatrixBT601:     return &libyuv::kYuvI601Constants;
        case kColorMatrixBT709:     return &libyuv::kYuvH709Constants;
        case kColorMatrixBT2020:    return &libyuv::kYuv2020Constants;
        default: break;
    }
    return Nil;
}

struct ColorConvertorContext : public SharedObject {
    ImageFormat             ipf;
    ImageFormat             opf;
    const PixelDescriptor * ipd;
    const PixelDescriptor * opd;
    hnd_t                   hnd;
    UInt32                flags;
};

static MediaUnitContext colorconvertor_alloc() {
    sp<ColorConvertorContext> ccc = new ColorConvertorContext;
    return ccc->RetainObject();
}

static void colorconvertor_dealloc(MediaUnitContext ref) {
    sp<ColorConvertorContext> ccc = static_cast<ColorConvertorContext *>(ref);
    ccc->ReleaseObject();
}

static MediaError colorconvertor_init_common(sp<ColorConvertorContext>& ccc, const MediaFormat * iformat, const MediaFormat * oformat) {
    // input & output are the same pixel format
    if (iformat->format == oformat->format) {
        INFO("same pixel format");
        // support pixel copy & cropping
        //return kMediaErrorBadParameters;
    }
    // input & output SHOULD have the same width & height
    // IGNORE output rectangle, set after process
    if (iformat->image.width == 0 || iformat->image.height == 0 ||
        iformat->image.rect.w < oformat->image.width ||
        iformat->image.rect.h < oformat->image.height) {
        ERROR("bad pixel dimention");
        return kMediaErrorBadParameters;
    }

    ccc->ipf    = iformat->image;
    ccc->opf    = oformat->image;
    ccc->ipd    = GetPixelFormatDescriptor(ccc->ipf.format);
    ccc->opd    = GetPixelFormatDescriptor(ccc->opf.format);
    return kMediaNoError;
}

static MediaError colorconvertor_init_420p(MediaUnitContext ref, const MediaFormat * iformat, const MediaFormat * oformat) {
    DEBUG("cc init %s => %s", GetImageFormatString(iformat->video).c_str(), GetImageFormatString(oformat->video).c_str());
    sp<ColorConvertorContext> ccc = static_cast<ColorConvertorContext *>(ref);
    if (colorconvertor_init_common(ccc, iformat, oformat) != kMediaNoError) {
        return kMediaErrorBadParameters;
    }
    if (ccc->opf.format != kPixelFormat420YpCbCrPlanar) {
        ERROR("bad output format %s", GetImageFormatString(ccc->opf).c_str());
        return kMediaErrorBadParameters;
    }
    
    ccc->hnd = get_convert_hnd(kTo420YpCbCrPlanar, ccc->ipf.format, &ccc->flags);
    return ccc->hnd.planar2packed != Nil ? kMediaNoError : kMediaErrorNotSupported;
}

static MediaError colorconvertor_init_bgra(MediaUnitContext ref, const MediaFormat * iformat, const MediaFormat * oformat) {
    DEBUG("cc init %s => %s", GetImageFormatString(iformat->image).c_str(), GetImageFormatString(oformat->image).c_str());
    sp<ColorConvertorContext> ccc = static_cast<ColorConvertorContext *>(ref);
    if (colorconvertor_init_common(ccc, iformat, oformat) != kMediaNoError) {
        return kMediaErrorBadParameters;
    }
    if (ccc->opf.format != kPixelFormatBGRA) {
        ERROR("bad output format %s", GetImageFormatString(ccc->opf).c_str());
        return kMediaErrorBadParameters;
    }
    
    ccc->hnd = get_convert_hnd(kToBGRA, ccc->ipf.format, &ccc->flags);
    if (ccc->hnd.planar2packed == Nil) {
        return kMediaErrorNotSupported;
    }
    if ((ccc->ipf.matrix && ccc->ipf.matrix != kColorMatrixBT601) && !(ccc->flags & COLOR_MATRIX)) {
        ERROR("color matrix is not supported");
        return kMediaErrorNotSupported;
    }
    
    // set default color matrix
    if (ccc->ipf.matrix == kColorMatrixNull && (ccc->flags & COLOR_MATRIX)) {
        ccc->ipf.matrix = kColorMatrixBT601;
    }
    return ccc->hnd.planar2packed != Nil ? kMediaNoError : kMediaErrorNotSupported;
}

static MediaError colorconvertor_init_rgba(MediaUnitContext ref, const MediaFormat * iformat, const MediaFormat * oformat) {
    DEBUG("cc init %s => %s", GetImageFormatString(iformat->video).c_str(), GetImageFormatString(oformat->video).c_str());
    sp<ColorConvertorContext> ccc = static_cast<ColorConvertorContext *>(ref);
    if (colorconvertor_init_common(ccc, iformat, oformat) != kMediaNoError) {
        return kMediaErrorBadParameters;
    }
    if (ccc->opf.format != kPixelFormatRGBA) {
        ERROR("bad output format %s", GetImageFormatString(ccc->opf).c_str());
        return kMediaErrorBadParameters;
    }
    
    ccc->hnd = get_convert_hnd(kToRGBA, ccc->ipf.format, &ccc->flags);
    return ccc->hnd.planar2packed != Nil ? kMediaNoError : kMediaErrorNotSupported;
}

static MediaError colorconvertor_init_rgb24(MediaUnitContext ref, const MediaFormat * iformat, const MediaFormat * oformat) {
    DEBUG("cc init %s => %s", GetImageFormatString(iformat->video).c_str(), GetImageFormatString(oformat->video).c_str());
    sp<ColorConvertorContext> ccc = static_cast<ColorConvertorContext *>(ref);
    if (colorconvertor_init_common(ccc, iformat, oformat) != kMediaNoError) {
        return kMediaErrorBadParameters;
    }
    if (ccc->opf.format != kPixelFormatBGR) {
        ERROR("bad output format %s", GetImageFormatString(ccc->opf).c_str());
        return kMediaErrorBadParameters;
    }
    
    ccc->hnd = get_convert_hnd(kToBGR, ccc->ipf.format, &ccc->flags);
    return ccc->hnd.planar2packed != Nil ? kMediaNoError : kMediaErrorNotSupported;
}

static MediaError colorconvertor_init_rgb16(MediaUnitContext ref, const MediaFormat * iformat, const MediaFormat * oformat) {
    DEBUG("cc init %s => %s", GetImageFormatString(iformat->video).c_str(), GetImageFormatString(oformat->video).c_str());
    sp<ColorConvertorContext> ccc = static_cast<ColorConvertorContext *>(ref);
    if (colorconvertor_init_common(ccc, iformat, oformat) != kMediaNoError) {
        return kMediaErrorBadParameters;
    }
    if (ccc->opf.format != kPixelFormatRGB16) {
        ERROR("bad output format %s", GetImageFormatString(ccc->opf).c_str());
        return kMediaErrorBadParameters;
    }
    
    ccc->hnd = get_convert_hnd(kToRGB16, ccc->ipf.format, &ccc->flags);
    return ccc->hnd.planar2packed != Nil ? kMediaNoError : kMediaErrorNotSupported;
}

MediaError colorconvertor_process(MediaUnitContext ref, const MediaBufferList * input, MediaBufferList * output) {
    DEBUG("process: %s", GetMediaBufferListString(*input).c_str());
    sp<ColorConvertorContext> ccc = static_cast<ColorConvertorContext *>(ref);
    const PixelDescriptor * ipd = ccc->ipd;
    const PixelDescriptor * opd = ccc->opd;
    const ImageFormat& ipf      = ccc->ipf;
    const ImageFormat& opf      = ccc->opf;
    
    if (input->count != ipd->nb_planes || output->count != opd->nb_planes) {
        return kMediaErrorBadParameters;
    }
    
    // check input size
    for (UInt32 i = 0; i < ipd->nb_planes; ++i) {
        const UInt32 size = (ipf.width * ipf.height * ipd->planes[i].bpp) / (8 * ipd->planes[i].hss * ipd->planes[i].vss);
        if (input->buffers[i].size < size) {
            ERROR("bad input buffer, size mismatch.");
            return kMediaErrorBadParameters;
        }
    }
    
    // check output capacity
    for (UInt32 i = 0; i < opd->nb_planes; ++i) {
        const UInt32 size = (opf.width * opf.height * opd->planes[i].bpp) / (8 * opd->planes[i].hss * opd->planes[i].vss);
        if (output->buffers[i].capacity < size) {
            ERROR("bad output buffer, capacity mismatch");
            return kMediaErrorBadParameters;
        }
        output->buffers[i].size = size; // set output size
    }
    
    // create shadows of input&output buffers for uv swap
    MediaBuffer ibf[input->count];
    MediaBuffer obf[output->count];
    for (UInt32 i = 0; i < input->count; ++i)   ibf[i] = input->buffers[i];
    for (UInt32 i = 0; i < output->count; ++i)  obf[i] = output->buffers[i];
    if (ccc->flags & SWAP_UV) {
        DEBUG("swap uv");
        if (IsPlanarYUV(ccc->ipf.format)) {    // prefer swap uv on input
            ibf[1] = input->buffers[2];
            ibf[2] = input->buffers[1];
        } else {
            CHECK_TRUE(IsPlanarYUV(ccc->opf.format));
            obf[1] = output->buffers[2];
            obf[2] = output->buffers[1];
        }
    }
    
    UInt32 offset[ipd->nb_planes];
    for (UInt32 i = 0; i < ipd->nb_planes; ++i) {
        const UInt32 y = ipf.rect.y / ipd->planes[i].vss;
        offset[i] = ((ipf.width * y + ipf.rect.x) * ipd->planes[i].bpp) / (8 * ipd->planes[i].hss);
        //DEBUG("offset[%zu]: %zu", i, offset[i]);
    }
    switch (ipd->nb_planes) {
        case 3: switch (opd->nb_planes) {
            case 3:
                ccc->hnd.planar2planar(ibf[0].data + offset[0],
                                       (ipf.width * ipd->planes[0].bpp) / (8 * ipd->planes[0].hss),
                                       ibf[1].data + offset[1],
                                       (ipf.width * ipd->planes[1].bpp) / (8 * ipd->planes[1].hss),
                                       ibf[2].data + offset[2],
                                       (ipf.width * ipd->planes[2].bpp) / (8 * ipd->planes[2].hss),
                                       obf[0].data,
                                       (opf.width * opd->planes[0].bpp) / (8 * opd->planes[0].hss),
                                       obf[1].data,
                                       (opf.width * opd->planes[1].bpp) / (8 * opd->planes[1].hss),
                                       obf[2].data,
                                       (opf.width * opd->planes[2].bpp) / (8 * opd->planes[2].hss),
                                       opf.width,
                                       opf.height);
                break;
            case 2:
                ccc->hnd.planar2semiplanar(ibf[0].data + offset[0],
                                           (ipf.width * ipd->planes[0].bpp) / (8 * ipd->planes[0].hss),
                                           ibf[1].data + offset[1],
                                           (ipf.width * ipd->planes[1].bpp) / (8 * ipd->planes[1].hss),
                                           ibf[2].data + offset[2],
                                           (ipf.width * ipd->planes[2].bpp) / (8 * ipd->planes[2].hss),
                                           obf[0].data,
                                           (opf.width * opd->planes[0].bpp) / (8 * opd->planes[0].hss),
                                           obf[1].data,
                                           (opf.width * opd->planes[1].bpp) / (8 * opd->planes[1].hss),
                                           opf.width,
                                           opf.height);
                break;
            case 1:
                if (ipf.matrix) {
                    ccc->hnd.planar2packedMAT(ibf[0].data + offset[0],
                                              (ipf.width * ipd->planes[0].bpp) / (8 * ipd->planes[0].hss),
                                              ibf[1].data + offset[1],
                                              (ipf.width * ipd->planes[1].bpp) / (8 * ipd->planes[1].hss),
                                              ibf[2].data + offset[2],
                                              (ipf.width * ipd->planes[2].bpp) / (8 * ipd->planes[2].hss),
                                              obf[0].data,
                                              (opf.width * opd->bpp) / 8,
                                              GetLibyuvMatrix(ipf.matrix),
                                              opf.width,
                                              opf.height);
                } else {
                    ccc->hnd.planar2packed(ibf[0].data + offset[0],
                                           (ipf.width * ipd->planes[0].bpp) / (8 * ipd->planes[0].hss),
                                           ibf[1].data + offset[1],
                                           (ipf.width * ipd->planes[1].bpp) / (8 * ipd->planes[1].hss),
                                           ibf[2].data + offset[2],
                                           (ipf.width * ipd->planes[2].bpp) / (8 * ipd->planes[2].hss),
                                           obf[0].data,
                                           (opf.width * opd->bpp) / 8,
                                           opf.width,
                                           opf.height);
                }
                break;
            default:
                break;
        } break;
        case 2: switch (opd->nb_planes) {
            case 3:
                ccc->hnd.semiplanar2planar(ibf[0].data + offset[0],
                                           (ipf.width * ipd->planes[0].bpp) / (8 * ipd->planes[0].hss),
                                           ibf[1].data + offset[1],
                                           (ipf.width * ipd->planes[1].bpp) / (8 * ipd->planes[1].hss),
                                           obf[0].data,
                                           (opf.width * opd->planes[0].bpp) / (8 * opd->planes[0].hss),
                                           obf[1].data,
                                           (opf.width * opd->planes[1].bpp) / (8 * opd->planes[1].hss),
                                           obf[2].data,
                                           (opf.width * opd->planes[2].bpp) / (8 * opd->planes[2].hss),
                                           opf.width,
                                           opf.height);
                break;
            case 2:
                ccc->hnd.semiplanar2semiplanar(ibf[0].data + offset[0],
                                               (ipf.width * ipd->planes[0].bpp) / (8 * ipd->planes[0].hss),
                                               ibf[1].data + offset[1],
                                               (ipf.width * ipd->planes[1].bpp) / (8 * ipd->planes[1].hss),
                                               obf[0].data,
                                               (opf.width * opd->planes[0].bpp) / (8 * opd->planes[0].hss),
                                               obf[1].data,
                                               (opf.width * opd->planes[1].bpp) / (8 * opd->planes[1].hss),
                                               opf.width,
                                               opf.height);
                break;
            case 1:
                if (ipf.matrix) {
                    ccc->hnd.semiplanar2packedMAT(ibf[0].data + offset[0],
                                                  (ipf.width * ipd->planes[0].bpp) / (8 * ipd->planes[0].hss),
                                                  ibf[1].data + offset[1],
                                                  (ipf.width * ipd->planes[1].bpp) / (8 * ipd->planes[1].hss),
                                                  obf[0].data,
                                                  (opf.width * opd->bpp) / 8,
                                                  GetLibyuvMatrix(ipf.matrix),
                                                  opf.width,
                                                  opf.height);
                } else {
                    ccc->hnd.semiplanar2packed(ibf[0].data + offset[0],
                                               (ipf.width * ipd->planes[0].bpp) / (8 * ipd->planes[0].hss),
                                               ibf[1].data + offset[1],
                                               (ipf.width * ipd->planes[1].bpp) / (8 * ipd->planes[1].hss),
                                               obf[0].data,
                                               (opf.width * opd->bpp) / 8,
                                               opf.width,
                                               opf.height);
                }
                break;
            default:
                break;
        } break;
        case 1: switch (opd->nb_planes) {
            case 3:
                ccc->hnd.packed2planar(ibf[0].data + offset[0],
                                       (ipf.width * ipd->bpp) / 8,
                                       obf[0].data,
                                       (opf.width * opd->planes[0].bpp) / (8 * opd->planes[0].hss),
                                       obf[1].data,
                                       (opf.width * opd->planes[1].bpp) / (8 * opd->planes[1].hss),
                                       obf[2].data,
                                       (opf.width * opd->planes[2].bpp) / (8 * opd->planes[2].hss),
                                       opf.width,
                                       opf.height);
                break;
            case 2:
                ccc->hnd.packed2semiplanar(ibf[0].data + offset[0],
                                           (ipf.width * ipd->bpp) / 8,
                                           obf[0].data,
                                           (opf.width * opd->planes[0].bpp) / (8 * opd->planes[0].hss),
                                           obf[1].data,
                                           (opf.width * opd->planes[1].bpp) / (8 * opd->planes[1].hss),
                                           opf.width,
                                           opf.height);
                break;
            case 1:
                if (ipf.matrix) {
                    ccc->hnd.packed2packedMAT(ibf[0].data + offset[0],
                                              (ipf.width * ipd->bpp) / 8,
                                              obf[0].data,
                                              (opf.width * opd->bpp) / 8,
                                              GetLibyuvMatrix(ipf.matrix),
                                              opf.width, opf.height);
                } else {
                    ccc->hnd.packed2packed(ibf[0].data + offset[0],
                                           (ipf.width * ipd->bpp) / 8,
                                           obf[0].data,
                                           (opf.width * opd->bpp) / 8,
                                           opf.width, opf.height);
                }
                break;
            default:
                break;
        } break;
        default:    break;
    }
    
    if (ccc->flags & BSWAP_32L) {
        DEBUG("swap32l");
        swap32l(obf[0].data, obf[0].size,
                obf[0].data, obf[0].size);
    }
    
    DEBUG("process: => %s", GetMediaBufferListString(*output).c_str());
    return kMediaNoError;
}

static const ePixelFormat kPixelFormatList[] = {
    kPixelFormat420YpCbCrPlanar,
    kPixelFormat420YpCrCbPlanar,
    kPixelFormat422YpCbCrPlanar,
    kPixelFormat422YpCrCbPlanar,
    kPixelFormat444YpCbCrPlanar,
    kPixelFormat444YpCrCbPlanar,
    kPixelFormat420YpCbCrSemiPlanar,
    kPixelFormat420YpCrCbSemiPlanar,
    kPixelFormat422YpCbCr,
    kPixelFormat422YpCrCb,
    kPixelFormat422YpCbCrWO,
    kPixelFormat422YpCrCbWO,
    kPixelFormat444YpCbCr,
    kPixelFormatRGB565,
    kPixelFormatBGR565,
    kPixelFormatRGB,
    kPixelFormatBGR,
    kPixelFormatARGB,
    kPixelFormatBGRA,
    kPixelFormatRGBA,
    kPixelFormatABGR,
    kPixelFormatUnknown
};

static const MediaUnit kConvertTo420p = {
    .name       = "color converter 420p",
    .flags      = 0,
    .iformats   = kPixelFormatList,
    .oformats   = (const ePixelFormat[]){ kPixelFormat420YpCbCrPlanar, kPixelFormatUnknown },
    .alloc      = colorconvertor_alloc,
    .dealloc    = colorconvertor_dealloc,
    .init       = colorconvertor_init_420p,
    .process    = colorconvertor_process,
    .reset      = Nil,
};

// ? -> ARGB in word-order
static const MediaUnit kConvertToBGRA = {
    .name       = "color converter BGRA",
    .flags      = 0,
    .iformats   = kPixelFormatList,
    .oformats   = (const ePixelFormat[]){ kPixelFormatBGRA, kPixelFormatUnknown },
    .alloc      = colorconvertor_alloc,
    .dealloc    = colorconvertor_dealloc,
    .init       = colorconvertor_init_bgra,
    .process    = colorconvertor_process,
    .reset      = Nil,
};

static const MediaUnit kConvertToRGBA = {
    .name       = "color converter RGBA",
    .flags      = 0,
    .iformats   = kPixelFormatList,
    .oformats   = (const ePixelFormat[]){ kPixelFormatRGBA, kPixelFormatUnknown },
    .alloc      = colorconvertor_alloc,
    .dealloc    = colorconvertor_dealloc,
    .init       = colorconvertor_init_rgba,
    .process    = colorconvertor_process,
    .reset      = Nil,
};

// ? -> RGB in word-order
static const MediaUnit kConvertToBGR = {
    .name       = "color converter BGR",
    .flags      = 0,
    .iformats   = kPixelFormatList,
    .oformats   = (const ePixelFormat[]){ kPixelFormatBGR, kPixelFormatUnknown },
    .alloc      = colorconvertor_alloc,
    .dealloc    = colorconvertor_dealloc,
    .init       = colorconvertor_init_rgb24,
    .process    = colorconvertor_process,
    .reset      = Nil,
};

// ? -> BGR16
static const MediaUnit kConvertToBGR565 = {
    .name       = "color converter BGR565",
    .flags      = 0,
    .iformats   = kPixelFormatList,
    .oformats   = (const ePixelFormat[]){ kPixelFormatRGB16, kPixelFormatUnknown },
    .alloc      = colorconvertor_alloc,
    .dealloc    = colorconvertor_dealloc,
    .init       = colorconvertor_init_rgb16,
    .process    = colorconvertor_process,
    .reset      = Nil,
};

static const MediaUnit * kColorUnitList[] = {
    &kConvertTo420p,
    &kConvertToBGRA,
    &kConvertToRGBA,
    &kConvertToBGR,
    &kConvertToBGR565,
    // END OF LIST
    Nil
};

static const MediaUnit * ColorUnitFind(const MediaUnit * list[],
                                       const ePixelFormat& iformat,
                                       const ePixelFormat& oformat) {
    for (UInt32 i = 0; list[i] != Nil; ++i) {
        if (ContainsPixelFormat(list[i]->iformats, iformat) &&
            ContainsPixelFormat(list[i]->oformats, oformat)) {
            return list[i];
        }
    }
    ERROR("no unit for %.4s => %.4s", (const Char *)&iformat, (const Char *)&oformat);
    return Nil;
}

static const MediaUnit * ColorUnitNew(const MediaUnit * list[],
                                      const ImageFormat& iformat,
                                      const ImageFormat& oformat,
                                      MediaUnitContext * p) {
    CHECK_NULL(p);
    const MediaUnit * unit = ColorUnitFind(list, iformat.format, oformat.format);
    if (!unit) return Nil;
    
    DEBUG("found unit %s", unit->name);
    MediaUnitContext instance = unit->alloc();
    MediaError st = unit->init(instance, (const MediaFormat*)&iformat, (const MediaFormat*)&oformat);
 
    if (st != kMediaNoError) {
        unit->dealloc(instance);
        ERROR("unit init failed for %s => %s",
              GetImageFormatString(iformat).c_str(), GetImageFormatString(oformat).c_str());
        return Nil;
    }
    *p = instance;
    return unit;
}

struct ColorConverter : public MediaDevice {
    ImageFormat                 mInput;
    ImageFormat                 mOutput;
    
    const MediaUnit *           mUnit;
    MediaUnitContext            mInstance;
    sp<MediaFrame>              mFrame;

    ColorConverter() : MediaDevice() { }
    
    virtual ~ColorConverter() {
        if (mUnit) {
            mUnit->dealloc(mInstance);
            mUnit = Nil;
            mInstance = Nil;
        }
    }
    
    MediaError init(const ImageFormat& iformat, const ImageFormat& oformat, const sp<Message>&) {
        DEBUG("init ColorConverter: %s => %s", GetImageFormatString(iformat).c_str(), GetImageFormatString(oformat).c_str());
        mInput      = iformat;
        mOutput     = oformat;
        
        mUnit = ColorUnitNew(kColorUnitList, iformat, oformat, &mInstance);
        if (mUnit) {
            DEBUG("new unit %.4s => %.4s", (const Char *)&iformat.format, (const Char *)&oformat.format);
            return kMediaNoError;
        }
        
        return kMediaErrorNotSupported;
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
        if (input.isNil()) return kMediaNoError;
        
        if (mFrame != Nil) return kMediaErrorResourceBusy;
        
        sp<MediaFrame> output   = MediaFrame::Create(mOutput);
        
        MediaError st = mUnit->process(mInstance,
                                       &input->planes,
                                       &output->planes);
        
        if (st != kMediaNoError) {
            ERROR("push %s failed", input->string().c_str());
            return kMediaErrorUnknown;
        }
        
        // copy input properties
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
        if (mUnit && mUnit->reset) {
            mUnit->reset(mInstance);
        }
        return kMediaNoError;
    }
};

sp<MediaDevice> CreateColorConverter(const ImageFormat& iformat, const ImageFormat& oformat, const sp<Message>& options) {
    sp<ColorConverter> cc = new ColorConverter;
    if (cc->init(iformat, oformat, options) != kMediaNoError) {
        return Nil;
    }
    return cc;
}

__END_NAMESPACE_MPX

__BEGIN_DECLS

__END_DECLS
