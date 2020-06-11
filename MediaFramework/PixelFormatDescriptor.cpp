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

#define LOG_TAG "PixelDescriptor"
#include "MediaTypes.h"


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

__END_DECLS
