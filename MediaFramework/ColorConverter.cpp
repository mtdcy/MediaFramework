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

static PixelDescriptor pixel420YpCbCrPlanar = {
    .name           = "420p",
    .format         = kPixelFormat420YpCbCrPlanar,
    .color          = kColorYpCbCr,
    .bpp            = 12,
    .planes         = 3,
    .plane          = {
        PLANE_VALUE_SS1x1,
        PLANE_VALUE_SS2x2,
        PLANE_VALUE_SS2x2,
    }
};

static PixelDescriptor pixel422YpCbCrPlanar = {
    .name           = "422p",
    .format         = kPixelFormat422YpCbCrPlanar,
    .color          = kColorYpCbCr,
    .bpp            = 16,
    .planes         = 3,
    .plane          = {
        PLANE_VALUE_SS1x1,
        PLANE_VALUE_SS2x1,
        PLANE_VALUE_SS2x1,
    }
};

static PixelDescriptor pixel444YpCbCrPlanar = {
    .name           = "444p",
    .format         = kPixelFormat444YpCbCrPlanar,
    .color          = kColorYpCbCr,
    .bpp            = 24,
    .planes         = 3,
    .plane          = {
        PLANE_VALUE_SS1x1,
        PLANE_VALUE_SS1x1,
        PLANE_VALUE_SS1x1,
    }
};

static PixelDescriptor pixel420YpCbCrSemiPlanar = {
    .name           = "nv12",
    .format         = kPixelFormat420YpCbCrSemiPlanar,
    .color          = kColorYpCbCr,
    .bpp            = 12,
    .planes         = 2,
    .plane          = {
        PLANE_VALUE_SS1x1,
        PLANE_VALUE_SS2x2_2,    // u & v
    }
};

static PixelDescriptor pixel420YpCrCbSemiPlanar = {
    .name           = "nv21",
    .format         = kPixelFormat420YpCrCbSemiPlanar,
    .color          = kColorYpCbCr,
    .bpp            = 12,
    .planes         = 2,
    .plane          = {
        PLANE_VALUE_SS1x1,
        PLANE_VALUE_SS2x2_2,    // v & u
    }
};

static PixelDescriptor pixel422YpCbCr = {
    .name           = "yuyv",
    .format         = kPixelFormat422YpCbCr,
    .color          = kColorYpCbCr,
    .bpp            = 16,
    .planes         = 1,
    .plane          = {
        PLANE_VALUE_SS1x1_2,
    }
};

static PixelDescriptor pixel422YpCrCb = {
    .name           = "yvyu",
    .format         = kPixelFormat422YpCrCb,
    .color          = kColorYpCbCr,
    .bpp            = 16,
    .planes         = 1,
    .plane          = {
        PLANE_VALUE_SS1x1_2,
    }
};

static PixelDescriptor pixel444YpCbCr = {
    .name           = "yuv444",
    .format         = kPixelFormat444YpCbCr,
    .color          = kColorYpCbCr,
    .bpp            = 24,
    .planes         = 1,
    .plane          = {
        PLANE_VALUE_SS1x1_3,
    }
};

static PixelDescriptor pixelRGB565 = {
    .name           = "RGB565",
    .format         = kPixelFormatRGB565,
    .color          = kColorRGB,
    .bpp            = 16,
    .planes         = 1,
    .plane          = {
        PLANE_VALUE_SS1x1_2,
    }
};

static PixelDescriptor pixelBGR565 = {
    .name           = "BGR565",
    .format         = kPixelFormatBGR565,
    .color          = kColorRGB,
    .bpp            = 16,
    .planes         = 1,
    .plane          = {
        PLANE_VALUE_SS1x1_2,
    }
};

static PixelDescriptor pixelRGB = {
    .name           = "RGB24",
    .format         = kPixelFormatRGB,
    .color          = kColorRGB,
    .bpp            = 24,
    .planes         = 1,
    .plane          = {
        PLANE_VALUE_SS1x1_3,
    }
};

static PixelDescriptor pixelBGR = {
    .name           = "BGR24",
    .format         = kPixelFormatBGR,
    .color          = kColorRGB,
    .bpp            = 24,
    .planes         = 1,
    .plane          = {
        PLANE_VALUE_SS1x1_3,
    }
};

static PixelDescriptor pixelARGB = {
    .name           = "ARGB",
    .format         = kPixelFormatARGB,
    .color          = kColorRGB,
    .bpp            = 32,
    .planes         = 1,
    .plane          = {
        PLANE_VALUE_SS1x1_4,
    }
};

static PixelDescriptor pixelBGRA = {
    .name           = "BGRA",
    .format         = kPixelFormatBGRA,
    .color          = kColorRGB,
    .bpp            = 32,
    .planes         = 1,
    .plane          = {
        PLANE_VALUE_SS1x1_4,
    }
};

static PixelDescriptor pixelRGBA = {
    .name           = "RGBA",
    .format         = kPixelFormatRGBA,
    .color          = kColorRGB,
    .bpp            = 32,
    .planes         = 1,
    .plane          = {
        PLANE_VALUE_SS1x1_4,
    }
};

static PixelDescriptor pixelABGR = {
    .name           = "ABGR",
    .format         = kPixelFormatABGR,
    .color          = kColorRGB,
    .bpp            = 32,
    .planes         = 1,
    .plane          = {
        PLANE_VALUE_SS1x1_4,
    }
};

static const PixelDescriptor * LIST[] = {
    // tri-planar YpCbCr
    &pixel420YpCbCrPlanar,
    &pixel422YpCbCrPlanar,
    &pixel444YpCbCrPlanar,
    // bi-planar YpCbCr
    &pixel420YpCbCrSemiPlanar,
    &pixel420YpCrCbSemiPlanar,
    // packed YpCbCr
    &pixel422YpCbCr,
    &pixel422YpCrCb,
    &pixel444YpCbCr,
    // rgb
    &pixelRGB565,
    &pixelBGR565,
    &pixelRGB,
    &pixelBGR,
    &pixelARGB,
    &pixelBGRA,
    &pixelRGBA,
    &pixelABGR,
    // END OF LIST
    NULL
};

const PixelDescriptor * GetPixelFormatDescriptor(ePixelFormat pixel) {
    for (size_t i = 0; LIST[i] != NULL; ++i) {
        const PixelDescriptor * desc = LIST[i];
        if (desc->format == pixel) {
            return desc;
        }
    }
    ERROR("missing pixel descriptor for %.4s", (const char *)&pixel);
    return NULL;
}

static const MediaFormat sPlanarYUV[] = {
    { .format = kPixelFormat420YpCbCrPlanar     },
    { .format = kPixelFormat420YpCrCbPlanar     },
    { .format = kPixelFormat422YpCbCrPlanar     },
    { .format = kPixelFormat422YpCrCbPlanar     },
    { .format = kPixelFormat444YpCbCrPlanar     },
    // END OF LIST
    { .format = kPixelFormatUnknown             },
};

static bool IsPlanarYUV(const ePixelFormat& pixel) {
    for (size_t i = 0; sPlanarYUV[i].format != kPixelFormatUnknown; ++i) {
        if (sPlanarYUV[i].format == pixel)
            return true;
    }
    return false;
}

static const MediaFormat sSemiPlanarYUV[] = {
    { .format = kPixelFormat420YpCbCrSemiPlanar },
    { .format = kPixelFormat420YpCrCbSemiPlanar },
    // END OF LIST
    { .format = kPixelFormatUnknown             },
};

static bool IsSemiPlanarYUV(const ePixelFormat& pixel) {
    for (size_t i = 0; sSemiPlanarYUV[i].format != kPixelFormatUnknown; ++i) {
        if (sSemiPlanarYUV[i].format == pixel)
            return true;
    }
    return false;
}

static const MediaFormat sPackedYUV[] = {
    { .format = kPixelFormat422YpCbCr           },
    { .format = kPixelFormat422YpCrCb           },
    { .format = kPixelFormat444YpCbCr           },
    // END OF LIST
    { .format = kPixelFormatUnknown             },
};

static bool IsPackedYUV(const ePixelFormat& pixel) {
    for (size_t i = 0; sPackedYUV[i].format != kPixelFormatUnknown; ++i) {
        if (sPackedYUV[i].format == pixel)
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

static const MediaFormat sRGB[] = {
    { .format = kPixelFormatRGB565              },
    { .format = kPixelFormatBGR565              },
    { .format = kPixelFormatRGB                 },
    { .format = kPixelFormatBGR                 },
    { .format = kPixelFormatARGB                },
    { .format = kPixelFormatBGRA                },
    { .format = kPixelFormatRGBA                },
    { .format = kPixelFormatABGR                },
    // END OF LIST
    { .format = kPixelFormatUnknown             },
};

static bool IsRGB(const ePixelFormat& pixel) {
    for (size_t i = 0; sRGB[i].format != kPixelFormatUnknown; ++i) {
        if (sRGB[i].format == pixel)
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
    for (size_t i = 0; i < src_bytes; ++i) {
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

static struct {
    ePixelFormat        source;
    ePixelFormat        target;
    Planar2Planar_t     hnd;
} kPlanar2PlanarMap[] = {
    // -> YpCbCr
    { kPixelFormat422YpCbCrPlanar,  kPixelFormat420YpCbCrPlanar,    libyuv::I422ToI420  },
    { kPixelFormat444YpCbCrPlanar,  kPixelFormat420YpCbCrPlanar,    libyuv::I444ToI420  },
    { kPixelFormat420YpCbCrPlanar,  kPixelFormat422YpCbCrPlanar,    libyuv::I420ToI422  },
    { kPixelFormat420YpCbCrPlanar,  kPixelFormat444YpCbCrPlanar,    libyuv::I420ToI444  },
    // END OF LIST
    { kPixelFormatUnknown }
};
static Planar2Planar_t hnd_323(ePixelFormat source, ePixelFormat target) {
    for (size_t i = 0; kPlanar2PlanarMap[i].source != kPixelFormatUnknown; ++i) {
        if (kPlanar2PlanarMap[i].source == source && kPlanar2PlanarMap[i].target == target) {
            return kPlanar2PlanarMap[i].hnd;
        }
    }
    return NULL;
}

static int I420ToNV12(const uint8_t* src_y, int src_stride_y,
        const uint8_t* src_u, int src_stride_u,
        const uint8_t* src_v, int src_stride_v,
        uint8_t* dst_y, int dst_stride_y,
        uint8_t* dst_vu, int dst_stride_vu,
        int width, int height) {
    return libyuv::I420ToNV21(src_y, src_stride_y,
            src_v, src_stride_v,
            src_u, src_stride_u,
            dst_y, dst_stride_y,
            dst_vu, dst_stride_vu,
            width, height);
}

static int I422ToNV12(const uint8_t* src_y, int src_stride_y,
        const uint8_t* src_u, int src_stride_u,
        const uint8_t* src_v, int src_stride_v,
        uint8_t* dst_y, int dst_stride_y,
        uint8_t* dst_vu, int dst_stride_vu,
        int width, int height) {
    return libyuv::I422ToNV21(src_y, src_stride_y,
            src_v, src_stride_v,
            src_u, src_stride_u,
            dst_y, dst_stride_y,
            dst_vu, dst_stride_vu,
            width, height);
}

static int I444ToNV12(const uint8_t* src_y, int src_stride_y,
        const uint8_t* src_u, int src_stride_u,
        const uint8_t* src_v, int src_stride_v,
        uint8_t* dst_y, int dst_stride_y,
        uint8_t* dst_vu, int dst_stride_vu,
        int width, int height) {
    return libyuv::I444ToNV21(src_y, src_stride_y,
            src_v, src_stride_v,
            src_u, src_stride_u,
            dst_y, dst_stride_y,
            dst_vu, dst_stride_vu,
            width, height);
}

static struct {
    ePixelFormat        source;
    ePixelFormat        target;
    Planar2SemiPlanar_t hnd;
} kPlanar2SemiPlanarMap[] = {
    // -> YpCbCrSemiPlanar (NV12)
    { kPixelFormat420YpCbCrPlanar,  kPixelFormat420YpCbCrSemiPlanar,    I420ToNV12          },
    { kPixelFormat422YpCbCrPlanar,  kPixelFormat420YpCbCrSemiPlanar,    I422ToNV12          },
    { kPixelFormat444YpCbCrPlanar,  kPixelFormat420YpCbCrSemiPlanar,    I444ToNV12          },
    { kPixelFormat420YpCrCbPlanar,  kPixelFormat420YpCbCrSemiPlanar,    libyuv::I420ToNV21  },
    { kPixelFormat422YpCrCbPlanar,  kPixelFormat420YpCbCrSemiPlanar,    libyuv::I422ToNV21  },
    // -> YpCrCbSemiPlanar (NV21)
    { kPixelFormat420YpCbCrPlanar,  kPixelFormat420YpCrCbSemiPlanar,    libyuv::I420ToNV21  },
    { kPixelFormat422YpCbCrPlanar,  kPixelFormat420YpCrCbSemiPlanar,    libyuv::I422ToNV21  },
    { kPixelFormat444YpCbCrPlanar,  kPixelFormat420YpCrCbSemiPlanar,    libyuv::I444ToNV21  },
    { kPixelFormat420YpCrCbPlanar,  kPixelFormat420YpCrCbSemiPlanar,    I420ToNV12          },
    { kPixelFormat422YpCrCbPlanar,  kPixelFormat420YpCrCbSemiPlanar,    I422ToNV12          },
    // END OF LIST
    { kPixelFormatUnknown }
};
static Planar2SemiPlanar_t hnd_322(ePixelFormat source, ePixelFormat target) {
    for (size_t i = 0; kPlanar2SemiPlanarMap[i].source != kPixelFormatUnknown; ++i) {
        if (kPlanar2SemiPlanarMap[i].source == source && kPlanar2SemiPlanarMap[i].target == target) {
            return kPlanar2SemiPlanarMap[i].hnd;
        }
    }
    return NULL;
}

static struct {
    ePixelFormat    source;
    ePixelFormat    target;
    Planar2Packed_t hnd;
} kPlanar2PackedMap[] = {
    // YpCbCrPlanar -> RGBA (ABGR in word-order)
    { kPixelFormat420YpCbCrPlanar,      kPixelFormatRGBA,       libyuv::I420ToABGR  },
    { kPixelFormat422YpCbCrPlanar,      kPixelFormatRGBA,       libyuv::I422ToABGR  },
    { kPixelFormat444YpCbCrPlanar,      kPixelFormatRGBA,       libyuv::I444ToABGR  },
    // YpCbCrPlanar -> ABGR (RGBA in word-order)
    { kPixelFormat420YpCbCrPlanar,      kPixelFormatABGR,       libyuv::I420ToRGBA  },
    { kPixelFormat422YpCbCrPlanar,      kPixelFormatABGR,       libyuv::I422ToRGBA  },
    //{ kPixelFormat444YpCbCrPlanar,      kPixelFormatABGR,       NULL                },
    // YpCbCrPlanar -> ARGB (BGRA in word-order)
    { kPixelFormat420YpCbCrPlanar,      kPixelFormatARGB,       libyuv::I420ToBGRA  },
    { kPixelFormat422YpCbCrPlanar,      kPixelFormatARGB,       libyuv::I422ToBGRA  },
    //{ kPixelFormat444YpCbCrPlanar,      kPixelFormatARGB,       NULL                },
    // YpCbCrPlanar -> BGRA (ARGB in word-order)
    { kPixelFormat420YpCbCrPlanar,      kPixelFormatBGRA,       libyuv::I420ToARGB  },
    { kPixelFormat422YpCbCrPlanar,      kPixelFormatBGRA,       libyuv::I422ToARGB  },
    { kPixelFormat444YpCbCrPlanar,      kPixelFormatBGRA,       libyuv::I444ToARGB  },
    // YpCbCrPlanar -> BGR (RGB in word-order)
    { kPixelFormat420YpCbCrPlanar,      kPixelFormatBGR,        libyuv::I420ToRGB24 },
    //{ kPixelFormat422YpCbCrPlanar,      kPixelFormatBGR,        NULL                },
    //{ kPixelFormat444YpCbCrPlanar,      kPixelFormatBGR,        NULL                },
    // YpCbCrPlanar -> BGR565 (RGB in word-order)
    { kPixelFormat420YpCbCrPlanar,      kPixelFormatBGR565,     libyuv::I420ToRGB565},
    { kPixelFormat422YpCbCrPlanar,      kPixelFormatBGR565,     libyuv::I422ToRGB565},
    //{ kPixelFormat444YpCbCrPlanar,      kPixelFormatBGR565,     NULL                },
    // END OF LIST
    { kPixelFormatUnknown }
};
static Planar2Packed_t hnd_321(ePixelFormat source, ePixelFormat target) {
    for (size_t i = 0; kPlanar2PackedMap[i].source != kPixelFormatUnknown; ++i) {
        if (kPlanar2PackedMap[i].source == source && kPlanar2PackedMap[i].target == target) {
            return kPlanar2PackedMap[i].hnd;
        }
    }
    return NULL;
}

static struct {
    ePixelFormat        source;
    ePixelFormat        target;
    SemiPlanar2Planar_t hnd;
} kSemiPlanar2PlanarMap[] = {
    { kPixelFormat420YpCbCrSemiPlanar,  kPixelFormat420YpCbCrPlanar,    libyuv::NV12ToI420  },
    { kPixelFormat420YpCrCbSemiPlanar,  kPixelFormat420YpCbCrPlanar,    libyuv::NV21ToI420  },
    // END OF LIST
    { kPixelFormatUnknown }
};
static SemiPlanar2Planar_t hnd_223(ePixelFormat source, ePixelFormat target) {
    for (size_t i = 0; kSemiPlanar2PlanarMap[i].source != kPixelFormatUnknown; ++i) {
        if (kSemiPlanar2PlanarMap[i].source == source && kSemiPlanar2PlanarMap[i].target == target) {
            return kSemiPlanar2PlanarMap[i].hnd;
        }
    }
    return NULL;
}

static struct {
    ePixelFormat        source;
    ePixelFormat        target;
    SemiPlanar2SemiPlanar_t hnd;
} kSemiPlanar2SemiPlanarMap[] = {
    { kPixelFormat420YpCbCrSemiPlanar,  kPixelFormat420YpCrCbSemiPlanar,    libyuv::NV21ToNV12  },
    { kPixelFormat420YpCrCbSemiPlanar,  kPixelFormat420YpCbCrSemiPlanar,    libyuv::NV21ToNV12  },
    // END OF LIST
    { kPixelFormatUnknown }
};
static SemiPlanar2SemiPlanar_t hnd_222(ePixelFormat source, ePixelFormat target) {
    for (size_t i = 0; kSemiPlanar2SemiPlanarMap[i].source != kPixelFormatUnknown; ++i) {
        if (kSemiPlanar2SemiPlanarMap[i].source == source && kSemiPlanar2SemiPlanarMap[i].target == target) {
            return kSemiPlanar2SemiPlanarMap[i].hnd;
        }
    }
    return NULL;
}

static struct {
    ePixelFormat        source;
    ePixelFormat        target;
    SemiPlanar2Packed_t hnd;
} kSemiPlanar2PackedMap[] = {
    // YpCbCrPlanar -> BGRA (ARGB in word-order)
    { kPixelFormat420YpCbCrSemiPlanar,  kPixelFormatBGRA,   libyuv::NV12ToARGB  },
    { kPixelFormat420YpCrCbSemiPlanar,  kPixelFormatBGRA,   libyuv::NV21ToARGB  },
    // YpCbCrPlanar -> ARGB (BGRA in word-order)
    //{ kPixelFormat420YpCbCrSemiPlanar,  kPixelFormatARGB,   NULL                },
    //{ kPixelFormat420YpCrCbSemiPlanar,  kPixelFormatARGB,   NULL                },
    // YpCbCrPlanar -> RGBA (ABGR in word-order)
    { kPixelFormat420YpCbCrSemiPlanar,  kPixelFormatRGBA,   libyuv::NV12ToABGR  },
    { kPixelFormat420YpCrCbSemiPlanar,  kPixelFormatRGBA,   libyuv::NV21ToABGR  },
    // YpCbCrPlanar -> ARGB (BGRA in word-order)
    //{ kPixelFormat420YpCbCrSemiPlanar,  kPixelFormatARGB,   NULL                },
    //{ kPixelFormat420YpCrCbSemiPlanar,  kPixelFormatARGB,   NULL                },
    // YpCbCrPlanar -> BGR (RGB in word-order)
    { kPixelFormat420YpCbCrSemiPlanar,  kPixelFormatBGR,    libyuv::NV12ToRGB24 },
    { kPixelFormat420YpCrCbSemiPlanar,  kPixelFormatBGR,    libyuv::NV21ToRGB24 },
    // YpCbCrPlanar -> BGR565 (RGB565 in word-order)
    { kPixelFormat420YpCbCrSemiPlanar,  kPixelFormatBGR565, libyuv::NV12ToRGB565},
    //{ kPixelFormat420YpCrCbSemiPlanar,  kPixelFormatBGR565, NULL                },
    // END OF LIST
    { kPixelFormatUnknown }
};
static SemiPlanar2Packed_t hnd_221(ePixelFormat source, ePixelFormat target) {
    for (size_t i = 0; kSemiPlanar2PackedMap[i].source != kPixelFormatUnknown; ++i) {
        if (kSemiPlanar2PackedMap[i].source == source && kSemiPlanar2PackedMap[i].target == target) {
            return kSemiPlanar2PackedMap[i].hnd;
        }
    }
    return NULL;
}

static struct {
    ePixelFormat        source;
    ePixelFormat        target;
    Packed2Planar_t     hnd;
} kPacked2PlanarMap[] = {
    { kPixelFormat422YpCbCr,        kPixelFormat422YpCbCrPlanar,    libyuv::YUY2ToI422      },
    { kPixelFormat422YpCrCb,        kPixelFormat422YpCrCbPlanar,    libyuv::YUY2ToI422      },
    { kPixelFormat444YpCbCr,        kPixelFormat444YpCbCrPlanar,    (Packed2Planar_t)libyuv::SplitRGBPlane   },
    // -> YpCbCrPlanar
    { kPixelFormatBGRA,             kPixelFormat420YpCbCrPlanar,    libyuv::ARGBToI420      },
    { kPixelFormatARGB,             kPixelFormat420YpCbCrPlanar,    libyuv::BGRAToI420      },
    { kPixelFormatRGBA,             kPixelFormat420YpCbCrPlanar,    libyuv::ABGRToI420      },
    { kPixelFormatABGR,             kPixelFormat420YpCbCrPlanar,    libyuv::RGBAToI420      },
    { kPixelFormatBGR,              kPixelFormat420YpCbCrPlanar,    libyuv::RGB24ToI420     },
    { kPixelFormatBGR565,           kPixelFormat420YpCbCrPlanar,    libyuv::RGB565ToI420    },
    { kPixelFormatBGRA,             kPixelFormat422YpCbCrPlanar,    libyuv::ARGBToI422      },
    { kPixelFormatBGRA,             kPixelFormat444YpCbCrPlanar,    libyuv::ARGBToI444      },
    // END OF LIST
    { kPixelFormatUnknown }
};
static Packed2Planar_t hnd_123(ePixelFormat source, ePixelFormat target) {
    for (size_t i = 0; kPacked2PlanarMap[i].source != kPixelFormatUnknown; ++i) {
        if (kPacked2PlanarMap[i].source == source && kPacked2PlanarMap[i].target == target) {
            return kPacked2PlanarMap[i].hnd;
        }
    }
    return NULL;
}

static struct {
    ePixelFormat        source;
    ePixelFormat        target;
    Packed2SemiPlanar_t hnd;
} kPacked2SemiPlanarMap[] = {
    { kPixelFormatBGRA,     kPixelFormat420YpCbCrSemiPlanar,    libyuv::ARGBToNV12  },
    { kPixelFormat422YpCbCr, kPixelFormat420YpCbCrSemiPlanar,   libyuv::YUY2ToNV12  },
    { kPixelFormatBGRA,     kPixelFormat420YpCrCbSemiPlanar,    libyuv::ARGBToNV21  },
    { kPixelFormatBGRA,     kPixelFormat420YpCrCbSemiPlanar,    libyuv::ARGBToNV21  },
    // END OF LIST
    { kPixelFormatUnknown }
};
static Packed2SemiPlanar_t hnd_122(ePixelFormat source, ePixelFormat target) {
    for (size_t i = 0; kPacked2SemiPlanarMap[i].source != kPixelFormatUnknown; ++i) {
        if (kPacked2SemiPlanarMap[i].source == source && kPacked2SemiPlanarMap[i].target == target) {
            return kPacked2SemiPlanarMap[i].hnd;
        }
    }
    return NULL;
}

static struct {
    ePixelFormat        source;
    ePixelFormat        target;
    Packed2Packed_t     hnd;
} kPacked2PackedMap[] = {
    // YpCbCr -> BGRA (ARGB in word-order)
    { kPixelFormat422YpCbCr,            kPixelFormatBGRA,   libyuv::YUY2ToARGB  },
    //{ kPixelFormat422YpCrCb,            kPixelFormatBGRA,   NULL                },
    // BGR (RGB in word-order) ->
    { kPixelFormatBGR,                  kPixelFormatBGRA,   libyuv::RGB24ToARGB },
    // BGR565 (RGB565 in word-order) ->
    { kPixelFormatBGR565,               kPixelFormatBGRA,   libyuv::RGB565ToARGB},
    // ABGR (RGBA in word-order) ->
    { kPixelFormatABGR,                 kPixelFormatBGRA,   libyuv::RGBAToARGB  },
    // RGBA (ABGR in word-order) ->
    { kPixelFormatRGBA,                 kPixelFormatBGRA,   libyuv::ABGRToARGB  },
    // BGRA (ARGB in word-order) ->
    { kPixelFormatBGRA,                 kPixelFormatARGB,   libyuv::ARGBToBGRA  },
    { kPixelFormatBGRA,                 kPixelFormatRGBA,   libyuv::ARGBToABGR  },
    { kPixelFormatBGRA,                 kPixelFormatABGR,   libyuv::ARGBToRGBA  },
    // ARGB (BGRA in word-order) ->
    { kPixelFormatARGB,                 kPixelFormatBGRA,   libyuv::BGRAToARGB  },
    // END OF LIST
    { kPixelFormatUnknown }
};
static Packed2Packed_t hnd_121(ePixelFormat source, ePixelFormat target) {
    for (size_t i = 0; kPacked2PackedMap[i].source != kPixelFormatUnknown; ++i) {
        if (kPacked2PackedMap[i].source == source && kPacked2PackedMap[i].target == target) {
            return kPacked2PackedMap[i].hnd;
        }
    }
    return NULL;
}

__END_DECLS

__BEGIN_NAMESPACE_MPX

struct ColorConvertorContext : public SharedObject {
    MediaFormat             iFormat;
    MediaFormat             oFormat;
    const PixelDescriptor * iPixelDesc;
    const PixelDescriptor * oPixelDesc;
    union {
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
};

static MediaUnitContext colorconvertor_alloc() {
    sp<ColorConvertorContext> context = new ColorConvertorContext;
    return context->RetainObject();
}

static void colorconvertor_dealloc(MediaUnitContext ref) {
    sp<ColorConvertorContext> context = ref;
    context->ReleaseObject();
}

static MediaError colorconvertor_init_common(sp<ColorConvertorContext>& context, const MediaFormat * iformat, const MediaFormat * oformat) {
    // input & output are the same pixel format
    if (iformat->format == oformat->format) {
        return kMediaErrorBadParameters;
    }
    // input & output SHOULD have the same width & height
    if (iformat->video.width == 0 || iformat->video.height == 0 ||
            iformat->video.width != oformat->video.width ||
            iformat->video.height != iformat->video.height) {
        return kMediaErrorBadParameters;
    }

    context->iFormat    = *iformat;
    context->oFormat    = *oformat;
    context->iPixelDesc = GetPixelFormatDescriptor(iformat->format);
    context->oPixelDesc = GetPixelFormatDescriptor(oformat->format);
    return kMediaNoError;
}

static MediaError colorconvertor_init(MediaUnitContext ref, const MediaFormat * iformat, const MediaFormat * oformat) {
    sp<ColorConvertorContext> context = ref;
    if (colorconvertor_init_common(context, iformat, oformat) != kMediaNoError) {
        return kMediaErrorBadParameters;
    }

    const ePixelFormat iPixel = iformat->format;
    const ePixelFormat oPixel = oformat->format;
    switch (context->iPixelDesc->planes) {
        case 3:
            switch (context->oPixelDesc->planes) {
                case 3:     context->planar2planar          = hnd_323(iPixel, oPixel);  break;
                case 2:     context->planar2semiplanar      = hnd_322(iPixel, oPixel);  break;
                case 1:     context->planar2packed          = hnd_321(iPixel, oPixel);  break;
                default:    break;
            } break;
        case 2:
            switch (context->oPixelDesc->planes) {
                case 3:     context->semiplanar2planar      = hnd_223(iPixel, oPixel);  break;
                case 2:     context->semiplanar2semiplanar  = hnd_222(iPixel, oPixel);  break;
                case 1:     context->semiplanar2packed      = hnd_221(iPixel, oPixel);  break;
                default:    break;
            } break;
        case 1:
            switch (context->oPixelDesc->planes) {
                case 3:     context->packed2planar          = hnd_123(iPixel, oPixel);  break;
                case 2:     context->packed2semiplanar      = hnd_122(iPixel, oPixel);  break;
                case 1:     context->packed2packed          = hnd_121(iPixel, oPixel);  break;
                default:    break;
            } break;
        default:    break;
    }

    if (context->planar2planar == NULL) {
        return kMediaErrorNotSupported;
    }
    return kMediaNoError;
}

MediaError colorconvertor_process(MediaUnitContext ref, const MediaBufferList * input, MediaBufferList * output) {
    sp<ColorConvertorContext> context = ref;
    const PixelDescriptor * iPixelDesc = context->iPixelDesc; 
    const PixelDescriptor * oPixelDesc = context->oPixelDesc;
    const MediaFormat& iFormat = context->iFormat;

    if (input->count != iPixelDesc->planes || output->count != oPixelDesc->planes) {
        return kMediaErrorBadParameters;
    }
    
    const uint32_t width = iFormat.video.width;
    const uint32_t height = iFormat.video.height;
    // check input size
    for (size_t i = 0; i < iPixelDesc->planes; ++i) {
        const size_t size = (width * height * iPixelDesc->plane[i].bpp) / (8 * iPixelDesc->plane[i].hss * iPixelDesc->plane[i].vss);
        if (input->buffers[i].size < size) {
            ERROR("bad input buffer, size mismatch.");
            return kMediaErrorBadParameters;
        }
    }
    
    // check output capacity
    for (size_t i = 0; i < oPixelDesc->planes; ++i) {
        const size_t size = (width * height * oPixelDesc->plane[i].bpp) / (8 * oPixelDesc->plane[i].hss * oPixelDesc->plane[i].vss);
        if (output->buffers[i].capacity < size) {
            ERROR("bad output buffer, capacity mismatch");
            return kMediaErrorBadParameters;
        }
        output->buffers[i].size = size; // set output size
    }

    switch (iPixelDesc->planes) {
        case 3:
            switch (oPixelDesc->planes) {
                case 3:
                    context->planar2planar(input->buffers[0].data, (width * iPixelDesc->plane[0].bpp) / (8 * iPixelDesc->plane[0].hss),
                                           input->buffers[1].data, (width * iPixelDesc->plane[1].bpp) / (8 * iPixelDesc->plane[1].hss),
                                           input->buffers[2].data, (width * iPixelDesc->plane[2].bpp) / (8 * iPixelDesc->plane[2].hss),
                                           output->buffers[0].data, (width * oPixelDesc->plane[0].bpp) / (8 * oPixelDesc->plane[0].hss),
                                           output->buffers[1].data, (width * oPixelDesc->plane[1].bpp) / (8 * oPixelDesc->plane[1].hss),
                                           output->buffers[2].data, (width * oPixelDesc->plane[2].bpp) / (8 * oPixelDesc->plane[2].hss),
                                           width, height);
                    break;
                case 2:
                    context->planar2semiplanar(input->buffers[0].data, (width * iPixelDesc->plane[0].bpp) / (8 * iPixelDesc->plane[0].hss),
                                               input->buffers[1].data, (width * iPixelDesc->plane[1].bpp) / (8 * iPixelDesc->plane[1].hss),
                                               input->buffers[2].data, (width * iPixelDesc->plane[2].bpp) / (8 * iPixelDesc->plane[2].hss),
                                               output->buffers[0].data, (width * oPixelDesc->plane[0].bpp) / (8 * oPixelDesc->plane[0].hss),
                                               output->buffers[1].data, (width * oPixelDesc->plane[1].bpp) / (8 * oPixelDesc->plane[1].hss),
                                               width, height);
                    break;
                case 1:
                    context->planar2packed(input->buffers[0].data, (width * iPixelDesc->plane[0].bpp) / (8 * iPixelDesc->plane[0].hss),
                                           input->buffers[1].data, (width * iPixelDesc->plane[1].bpp) / (8 * iPixelDesc->plane[1].hss),
                                           input->buffers[2].data, (width * iPixelDesc->plane[2].bpp) / (8 * iPixelDesc->plane[2].hss),
                                           output->buffers[0].data, (width * oPixelDesc->bpp) / 8,
                                           width, height);
                    break;
                default:    break;
            } break;
        case 2:
            switch (oPixelDesc->planes) {
                case 3:
                    context->semiplanar2planar(input->buffers[0].data, (width * iPixelDesc->plane[0].bpp) / (8 * iPixelDesc->plane[0].hss),
                                               input->buffers[1].data, (width * iPixelDesc->plane[1].bpp) / (8 * iPixelDesc->plane[1].hss),
                                               output->buffers[0].data, (width * oPixelDesc->plane[0].bpp) / (8 * oPixelDesc->plane[0].hss),
                                               output->buffers[1].data, (width * oPixelDesc->plane[1].bpp) / (8 * oPixelDesc->plane[1].hss),
                                               output->buffers[2].data, (width * oPixelDesc->plane[2].bpp) / (8 * oPixelDesc->plane[2].hss),
                                               width, height);
                    break;
                case 2:
                    context->semiplanar2semiplanar(input->buffers[0].data, (width * iPixelDesc->plane[0].bpp) / (8 * iPixelDesc->plane[0].hss),
                                                   input->buffers[1].data, (width * iPixelDesc->plane[1].bpp) / (8 * iPixelDesc->plane[1].hss),
                                                   output->buffers[0].data, (width * oPixelDesc->plane[0].bpp) / (8 * oPixelDesc->plane[0].hss),
                                                   output->buffers[1].data, (width * oPixelDesc->plane[1].bpp) / (8 * oPixelDesc->plane[1].hss),
                                                   width, height);
                    break;
                case 1:
                    context->semiplanar2packed(input->buffers[0].data, (width * iPixelDesc->plane[0].bpp) / (8 * iPixelDesc->plane[0].hss),
                                               input->buffers[1].data, (width * iPixelDesc->plane[1].bpp) / (8 * iPixelDesc->plane[1].hss),
                                               output->buffers[0].data, (width * oPixelDesc->bpp) / 8,
                                               width, height);
                    break;
                default:    break;
            } break;
        case 1:
            switch (oPixelDesc->planes) {
                case 3:
                    context->packed2planar(input->buffers[0].data, (width * iPixelDesc->bpp) / 8,
                                           output->buffers[0].data, (width * oPixelDesc->plane[0].bpp) / (8 * oPixelDesc->plane[0].hss),
                                           output->buffers[1].data, (width * oPixelDesc->plane[1].bpp) / (8 * oPixelDesc->plane[1].hss),
                                           output->buffers[2].data, (width * oPixelDesc->plane[2].bpp) / (8 * oPixelDesc->plane[2].hss),
                                           width, height);
                    break;
                case 2:
                    context->packed2semiplanar(input->buffers[0].data, (width * iPixelDesc->bpp) / 8,
                                               output->buffers[0].data, (width * oPixelDesc->plane[0].bpp) / (8 * oPixelDesc->plane[0].hss),
                                               output->buffers[1].data, (width * oPixelDesc->plane[1].bpp) / (8 * oPixelDesc->plane[1].hss),
                                               width, height);
                    break;
                case 1:
                    context->packed2packed(input->buffers[0].data, (width * iPixelDesc->bpp) / 8,
                                           output->buffers[0].data, (width * oPixelDesc->bpp) / 8,
                                           width, height);
                    break;
                default:    break;
            } break;
        default:    break;
    }

    return kMediaNoError;
}

static const MediaUnit sPlanar2Planar = {
    .name       = "Planar2Planar",
    .flags      = 0,
    .iformats   = sPlanarYUV,
    .oformats   = sPlanarYUV,
    .alloc      = colorconvertor_alloc,
    .dealloc    = colorconvertor_dealloc,
    .init       = colorconvertor_init,
    .process    = colorconvertor_process,
    .reset      = NULL,
};

static const MediaUnit sPlanar2SemiPlanar = {
    .name       = "Planar2SemiPlanar",
    .flags      = 0,
    .iformats   = sPlanarYUV,
    .oformats   = sSemiPlanarYUV,
    .alloc      = colorconvertor_alloc,
    .dealloc    = colorconvertor_dealloc,
    .init       = colorconvertor_init,
    .process    = colorconvertor_process,
    .reset      = NULL,
};

static const MediaUnit sPlanar2PackedYUV = {
    .name       = "Planar2Packed",
    .flags      = 0,
    .iformats   = sPlanarYUV,
    .oformats   = sPackedYUV,
    .alloc      = colorconvertor_alloc,
    .dealloc    = colorconvertor_dealloc,
    .init       = colorconvertor_init,
    .process    = colorconvertor_process,
    .reset      = NULL,
};

static const MediaUnit sPlanar2PackedRGB = {
    .name       = "Planar2Packed",
    .flags      = 0,
    .iformats   = sPlanarYUV,
    .oformats   = sRGB,
    .alloc      = colorconvertor_alloc,
    .dealloc    = colorconvertor_dealloc,
    .init       = colorconvertor_init,
    .process    = colorconvertor_process,
    .reset      = NULL,
};

static const MediaUnit sSemiPlanar2Planar = {
    .name       = "SemiPlanar2Planar",
    .flags      = 0,
    .iformats   = sSemiPlanarYUV,
    .oformats   = sPlanarYUV,
    .alloc      = colorconvertor_alloc,
    .dealloc    = colorconvertor_dealloc,
    .init       = colorconvertor_init,
    .process    = colorconvertor_process,
    .reset      = NULL,
};

static const MediaUnit sSemiPlanar2SemiPlanar = {
    .name       = "SemiPlanar2SemiPlanar",
    .flags      = 0,
    .iformats   = sSemiPlanarYUV,
    .oformats   = sSemiPlanarYUV,
    .alloc      = colorconvertor_alloc,
    .dealloc    = colorconvertor_dealloc,
    .init       = colorconvertor_init,
    .process    = colorconvertor_process,
    .reset      = NULL,
};

static const MediaUnit sSemiPlanar2PackedYUV = {
    .name       = "SemiPlanar2PackedYUV",
    .flags      = 0,
    .iformats   = sSemiPlanarYUV,
    .oformats   = sPackedYUV,
    .alloc      = colorconvertor_alloc,
    .dealloc    = colorconvertor_dealloc,
    .init       = colorconvertor_init,
    .process    = colorconvertor_process,
    .reset      = NULL,
};

static const MediaUnit sSemiPlanar2PackedRGB = {
    .name       = "SemiPlanar2PackedRGB",
    .flags      = 0,
    .iformats   = sSemiPlanarYUV,
    .oformats   = sRGB,
    .alloc      = colorconvertor_alloc,
    .dealloc    = colorconvertor_dealloc,
    .init       = colorconvertor_init,
    .process    = colorconvertor_process,
    .reset      = NULL,
};

static const MediaUnit sPacked2Planar = {
    .name       = "Packed2Planar",
    .flags      = 0,
    .iformats   = sPackedYUV,
    .oformats   = sPlanarYUV,
    .alloc      = colorconvertor_alloc,
    .dealloc    = colorconvertor_dealloc,
    .init       = colorconvertor_init,
    .process    = colorconvertor_process,
    .reset      = NULL,
};

static const MediaUnit sPacked2SemiPlanar = {
    .name       = "Packed2SemiPlanar",
    .flags      = 0,
    .iformats   = sPackedYUV,
    .oformats   = sSemiPlanarYUV,
    .alloc      = colorconvertor_alloc,
    .dealloc    = colorconvertor_dealloc,
    .init       = colorconvertor_init,
    .process    = colorconvertor_process,
    .reset      = NULL,
};

static const MediaUnit sPackedYUV2PackedYUV = {
    .name       = "PackedYUV2PackedYUV",
    .flags      = 0,
    .iformats   = sPackedYUV,
    .oformats   = sPackedYUV,
    .alloc      = colorconvertor_alloc,
    .dealloc    = colorconvertor_dealloc,
    .init       = colorconvertor_init,
    .process    = colorconvertor_process,
    .reset      = NULL,
};

static const MediaUnit sRGB2RGB = {
    .name       = "RGB2RGB",
    .flags      = 0,
    .iformats   = sRGB,
    .oformats   = sRGB,
    .alloc      = colorconvertor_alloc,
    .dealloc    = colorconvertor_dealloc,
    .init       = colorconvertor_init,
    .process    = colorconvertor_process,
    .reset      = NULL,
};

static const MediaUnit sPackedYUV2RGB = {
    .name       = "PackedYUV2RGB",
    .flags      = 0,
    .iformats   = sPackedYUV,
    .oformats   = sRGB,
    .alloc      = colorconvertor_alloc,
    .dealloc    = colorconvertor_dealloc,
    .init       = colorconvertor_init,
    .process    = colorconvertor_process,
    .reset      = NULL,
};

static const MediaUnit sRGB2PackedYUV = {
    .name       = "RGB2PackedYUV",
    .flags      = 0,
    .iformats   = sRGB,
    .oformats   = sPackedYUV,
    .alloc      = colorconvertor_alloc,
    .dealloc    = colorconvertor_dealloc,
    .init       = colorconvertor_init,
    .process    = colorconvertor_process,
    .reset      = NULL,
};

static MediaError planeswap_init(MediaUnitContext ref, const MediaFormat * iformat, const MediaFormat * oformat) {
    sp<ColorConvertorContext> context = ref;
    if (colorconvertor_init_common(context, iformat, oformat) != kMediaNoError) {
        return kMediaErrorBadParameters;
    }
    if (!IsPlanarYUV(iformat->format)) {
        return kMediaErrorBadParameters;
    }
    // input & output SHOULD have the same plane count
    if (context->iPixelDesc->planes < 3 ||
            context->iPixelDesc->planes != context->oPixelDesc->planes) {
        return kMediaErrorBadParameters;
    }
    return kMediaNoError;
}

static MediaError planeswap_process(MediaUnitContext ref, const MediaBufferList * input, MediaBufferList * output) {
    sp<ColorConvertorContext> context = ref;
    if (input->count != 3 || input->count != output->count) {
        return kMediaErrorBadParameters;
    }
    for (size_t i = 0; i < input->count; ++i) {
        if (input->buffers[i].size > output->buffers[i].capacity)
            return kMediaErrorBadParameters;
    }
    // support inplace process
    MediaBuffer a       = input->buffers[1];
    MediaBuffer b       = input->buffers[2];
    output->buffers[1]  = b;
    output->buffers[2]  = a;
    return kMediaNoError;
}

static const MediaUnit sPlaneSwap = {
    .name       = "PlaneSwap",
    .flags      = kMediaUnitProcessInplace,
    .iformats   = sPlanarYUV,
    .oformats   = sPlanarYUV,
    .alloc      = colorconvertor_alloc,
    .dealloc    = colorconvertor_dealloc,
    .init       = planeswap_init,
    .process    = planeswap_process,
    .reset      = NULL,
};

static MediaError byteswap_init(MediaUnitContext ref, const MediaFormat * iformat, const MediaFormat * oformat) {
    sp<ColorConvertorContext> context = ref;
    if (colorconvertor_init_common(context, iformat, oformat) != kMediaNoError) {
        return kMediaErrorBadParameters;
    }
    // input & output SHOULD have the same plane count
    if (context->iPixelDesc->planes != context->oPixelDesc->planes) {
        return kMediaErrorBadParameters;
    }
    switch (iformat->format) {
        case kPixelFormat420YpCbCrSemiPlanar:
        case kPixelFormat420YpCrCbSemiPlanar:
            context->byteswap = swap16;
            break;
        case kPixelFormat422YpCbCr:
        case kPixelFormat422YpCrCb:
            // swap u & v bytes
            context->byteswap = swap32l;
            break;
        case kPixelFormatRGB:
        case kPixelFormatBGR:
        case kPixelFormat444YpCbCr:
            context->byteswap = swap24;
            break;
        case kPixelFormatRGBA:
        case kPixelFormatABGR:
        case kPixelFormatARGB:
        case kPixelFormatBGRA:
            context->byteswap = swap32;
            break;
        default:
            return kMediaErrorBadParameters;
    }
    return kMediaNoError;
}

static MediaError byteswap_process(MediaUnitContext ref, const MediaBufferList * input, MediaBufferList * output) {
    sp<ColorConvertorContext> context = ref;
    if (input->count != 1 ||
            input->count != output->count) {
        return kMediaErrorBadParameters;
    }
    if (input->buffers[0].size > output->buffers[0].capacity) {
        return kMediaErrorBadParameters;
    }
    if (IsSemiPlanarYUV(context->iFormat.format)) {
        if (output->buffers[0].data != input->buffers[0].data) {
            libyuv::CopyPlane(input->buffers[0].data,
                    (context->iFormat.video.width * context->iPixelDesc->plane[0].bpp) / (8 * context->iPixelDesc->plane[0].hss),
                    output->buffers[0].data,
                    (context->iFormat.video.width * context->oPixelDesc->plane[0].bpp) / (8 * context->oPixelDesc->plane[0].hss),
                    context->iFormat.video.width,
                    context->iFormat.video.height);
            output->buffers[0].size = input->buffers[0].size;
        } else { // in-place processing
            output->buffers[0] = input->buffers[0];
        }
        context->byteswap(input->buffers[1].data, input->buffers[1].size,
                output->buffers[1].data, output->buffers[1].capacity);
        output->buffers[1].size = input->buffers[1].size;
    } else {
        context->byteswap(input->buffers[0].data, input->buffers[0].size,
                output->buffers[0].data, output->buffers[0].capacity);
        output->buffers[0].size = input->buffers[0].size;
    }
    return kMediaNoError;
}

static const MediaUnit sByteSwapYUV = {
    .name       = "ByteSwapYUV",
    .flags      = 0,
    .iformats   = sPackedYUV,
    .oformats   = sPackedYUV,
    .alloc      = colorconvertor_alloc,
    .dealloc    = colorconvertor_dealloc,
    .init       = byteswap_init,
    .process    = byteswap_process,
    .reset      = NULL,
};

static const MediaUnit sByteSwapRGB = {
    .name       = "ByteSwapRGB",
    .flags      = 0,
    .iformats   = sRGB,
    .oformats   = sRGB,
    .alloc      = colorconvertor_alloc,
    .dealloc    = colorconvertor_dealloc,
    .init       = byteswap_init,
    .process    = byteswap_process,
    .reset      = NULL,
};

static const MediaUnit * sMediaUnits[] = {
    &sPlanar2Planar,
    &sPlanar2SemiPlanar,
    &sPlanar2PackedYUV,
    &sPlanar2PackedRGB,
    &sSemiPlanar2Planar,
    &sSemiPlanar2SemiPlanar,
    &sSemiPlanar2PackedYUV,
    &sSemiPlanar2PackedRGB,
    &sPacked2Planar,
    &sPacked2SemiPlanar,
    &sPackedYUV2PackedYUV,
    &sPackedYUV2RGB,
    &sRGB2RGB,
    &sRGB2PackedYUV,
    &sPlaneSwap,
    &sByteSwapYUV,
    &sByteSwapRGB,
    // END OF LIST
    NULL
};

struct DefaultColorConverter : public ColorConverter {
    Vector<const MediaUnit *>   mUnits;
    Vector<MediaUnitContext>    mInstances;

    DefaultColorConverter() : ColorConverter() { }
    
    virtual ~DefaultColorConverter() { }

    virtual sp<MediaFrame> convert(const sp<MediaFrame>&);
};

static FORCE_INLINE MediaUnitContext MediaUnitInstance(const MediaUnit * unit, const MediaFormat& iformat, const MediaFormat& oformat) {
    MediaUnitContext instance = unit->alloc();
    if (unit->init(instance, &iformat, &oformat) != kMediaNoError) {
        ERROR("init MediaUnit failed: %.4s [%dx%d] -> %.4s [%dx%d]",
                (const char *)&iformat.format, iformat.video.width, iformat.video.height,
                (const char *)&oformat.format, oformat.video.width, oformat.video.height);
        return NULL;
    }
    return instance;
}

sp<ColorConverter> ColorConverter::create(const ImageFormat& iformat, const ImageFormat& oformat) {
    if (iformat.width != oformat.width ||
            iformat.height != oformat.width) {
        ERROR("bad parameters: %.4s [%dx%d] -> %.4s [%dx%d]",
                (const char *)&iformat.format, iformat.width, iformat.height,
                (const char *)&oformat.format, oformat.width, oformat.height);
        return NULL;
    }

    MediaFormat iFormat, oFormat;
    iFormat.video   = iformat;
    oFormat.video   = oformat;

    List<const MediaUnit *> possible;
    // direct convert: using only one MediaUnit
    const MediaUnit * current = ColorUnitFindNext(NULL, iFormat.video.format);
    do {
        for (size_t i = 0; current->oformats[i].format != kPixelFormatUnknown; ++i) {
            if (current->oformats[i].format == oformat.format) {
                sp<DefaultColorConverter> cc = new DefaultColorConverter;
                cc->mUnits.push(current);
                MediaUnitContext instance = MediaUnitInstance(current, iFormat, oFormat);
                cc->mInstances.push(instance);
                return cc;
            }
            possible.push(current);
        }
        current = ColorUnitFindNext(current, iFormat.video.format);
    } while(1);

    // TODO

    return NULL;
}

sp<MediaFrame> DefaultColorConverter::convert(const sp<MediaFrame>& input) {
    // TODO
}

__END_NAMESPACE_MPX

__BEGIN_DECLS

const MediaUnit * ColorUnitFindNext(const MediaUnit * current, const ePixelFormat pixel) {
    if (current == NULL) current = mpx::sMediaUnits[0];
    else ++current;

    while (current != NULL) {
        for (size_t j = 0; current->iformats[j].format != kPixelFormatUnknown; ++j) {
            if (current->iformats[j].format == pixel) {
                return current;
            }
        }
        ++current;
    }
    return NULL;
}

__END_DECLS
