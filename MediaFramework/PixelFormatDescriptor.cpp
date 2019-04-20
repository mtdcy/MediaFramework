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
#include "MediaDefs.h"


__BEGIN_DECLS

static PixelDescriptor pixel420YpCbCrPlanar = {
    .name           = "420p",
    .format         = kPixelFormat420YpCbCrPlanar,
    .color          = kColorYpCbCr,
    .bpp            = 12,
    .planes         = 3,
    .plane          = {
        {   8,      1,      1 },
        {   2,      2,      2 },    // subsampling horizontal & vertical
        {   2,      2,      2 },    // subsampling horizontal & vertical
    }
};

static PixelDescriptor pixel422YpCbCrPlanar = {
    .name           = "422p",
    .format         = kPixelFormat422YpCbCrPlanar,
    .color          = kColorYpCbCr,
    .bpp            = 16,
    .planes         = 3,
    .plane          = {
        {   8,      1,      1 },
        {   4,      2,      1 },    // only subsampling horizontal
        {   4,      2,      1 },    // only subsampling horizontal
    }
};

static PixelDescriptor pixel444YpCbCrPlanar = {
    .name           = "444p",
    .format         = kPixelFormat444YpCbCrPlanar,
    .color          = kColorYpCbCr,
    .bpp            = 24,
    .planes         = 3,
    .plane          = {
        {   8,      1,      1 },
        {   8,      1,      1 },
        {   8,      1,      1 },
    }
};

static PixelDescriptor pixel420YpCbCrSemiPlanar = {
    .name           = "nv12",
    .format         = kPixelFormat420YpCbCrSemiPlanar,
    .color          = kColorYpCbCr,
    .bpp            = 12,
    .planes         = 2,
    .plane          = {
        {   8,      1,      1 },
        {   4,      2,      2 },    // u & v
    }
};

static PixelDescriptor pixel420YpCrCbSemiPlanar = {
    .name           = "nv21",
    .format         = kPixelFormat420YpCrCbSemiPlanar,
    .color          = kColorYpCbCr,
    .bpp            = 12,
    .planes         = 2,
    .plane          = {
        {   8,      1,      1 },
        {   4,      2,      2 },    // v & u
    }
};

static PixelDescriptor pixel422YpCbCr = {
    .name           = "yuyv",
    .format         = kPixelFormat422YpCbCr,
    .color          = kColorYpCbCr,
    .bpp            = 16,
    .planes         = 1,
    .plane          = {
        {   16,     1,      1 },
    }
};

static PixelDescriptor pixel422YpCrCb = {
    .name           = "yvyu",
    .format         = kPixelFormat422YpCrCb,
    .color          = kColorYpCbCr,
    .bpp            = 16,
    .planes         = 1,
    .plane          = {
        {   16,     1,      1 },
    }
};

static PixelDescriptor pixel444YpCbCr = {
    .name           = "yuv444",
    .format         = kPixelFormat444YpCbCr,
    .color          = kColorYpCbCr,
    .bpp            = 24,
    .planes         = 1,
    .plane          = {
        {   24,     1,      1 },
    }
};

static PixelDescriptor pixelRGB565 = {
    .name           = "RGB565",
    .format         = kPixelFormatRGB565,
    .color          = kColorRGB,
    .bpp            = 16,
    .planes         = 1,
    .plane          = {
        {   16,     1,      1 },
    }
};

static PixelDescriptor pixelBGR565 = {
    .name           = "BGR565",
    .format         = kPixelFormatBGR565,
    .color          = kColorRGB,
    .bpp            = 16,
    .planes         = 1,
    .plane          = {
        {   16,     1,      1 },
    }
};

static PixelDescriptor pixelRGB = {
    .name           = "RGB24",
    .format         = kPixelFormatRGB,
    .color          = kColorRGB,
    .bpp            = 24,
    .planes         = 1,
    .plane          = {
        {   24,     1,      1 },
    }
};

static PixelDescriptor pixelBGR = {
    .name           = "BGR24",
    .format         = kPixelFormatBGR,
    .color          = kColorRGB,
    .bpp            = 24,
    .planes         = 1,
    .plane          = {
        {   24,     1,      1 },
    }
};

static PixelDescriptor pixelARGB = {
    .name           = "ARGB",
    .format         = kPixelFormatARGB,
    .color          = kColorRGB,
    .bpp            = 32,
    .planes         = 1,
    .plane          = {
        {   32,     1,      1 },
    }
};

static PixelDescriptor pixelBGRA = {
    .name           = "BGRA",
    .format         = kPixelFormatBGRA,
    .color          = kColorRGB,
    .bpp            = 32,
    .planes         = 1,
    .plane          = {
        {   32,     1,      1 },
    }
};

static PixelDescriptor pixelRGBA = {
    .name           = "RGBA",
    .format         = kPixelFormatRGBA,
    .color          = kColorRGB,
    .bpp            = 32,
    .planes         = 1,
    .plane          = {
        {   32,     1,      1 },
    }
};

static PixelDescriptor pixelABGR = {
    .name           = "ABGR",
    .format         = kPixelFormatABGR,
    .color          = kColorRGB,
    .bpp            = 32,
    .planes         = 1,
    .plane          = {
        {   32,     1,      1 },
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
    for (size_t i = 0; ; ++i) {
        const PixelDescriptor * desc = LIST[i];
        if (desc->format == pixel) {
            return desc;
        }
    }
    ERROR("missing pixel descriptor for %#x", pixel);
    return NULL;
}

__END_DECLS
