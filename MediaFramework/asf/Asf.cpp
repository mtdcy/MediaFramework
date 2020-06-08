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


// File:    Asf.cpp
// Author:  mtdcy.chen
// Changes:
//          1. 20160701     initial version
//

#define LOG_TAG "Asf"
#define LOG_NDEBUG 0

#include "Asf.h"

__BEGIN_NAMESPACE_MPX
__BEGIN_NAMESPACE(ASF)

WAVEFORMATEX::WAVEFORMATEX() {
    // set default values
    wFormatTag          = WAVE_FORMAT_PCM;
    nChannels           = 2;
    nSamplesPerSec      = 8000;
    nAvgBytesPerSec     = 32000;
    nBlockAlign         = 4;
    wBitsPerSample      = 16;
    cbSize              = 0;
}

MediaError WAVEFORMATEX::parse(BitReader& br) {
    if (br.remianBytes() < WAVEFORMATEX_MIN_LENGTH)
    return kMediaErrorBadValue;
    
    // valid chunk length: 16, 18, 40
    // but we have saw chunk length 50
    wFormatTag          = br.rl16();
    nChannels           = br.rl16();
    nSamplesPerSec      = br.rl32();
    nAvgBytesPerSec     = br.rl32();
    nBlockAlign         = br.rl16();
    wBitsPerSample      = br.rl16();
    // 16 bytes
    
    if (br.remianBytes() > 2) {     // >= 18
        cbSize          = br.rl16();
    }
    // 18 bytes

    DEBUG("audio format %u.",     wFormatTag);
    DEBUG("number channels %u.",  nChannels);
    DEBUG("sample rate %u.",      nSamplesPerSec);
    DEBUG("byte rate %u.",        nAvgBytesPerSec);
    DEBUG("block align %u.",      nBlockAlign);
    DEBUG("bits per sample %u.",  wBitsPerSample);

    if (wFormatTag == WAVE_FORMAT_EXTENSIBLE) {
        DEBUG("fmt chunk with extensible format.");
        // 40 - 18 == 22
        if (br.remianBytes() < 22) {
            DEBUG("invalid fmt chunk length %zu.", 18 + br.remianBytes());
            return kMediaErrorBadValue;
        }

        wValidBitsPerSample     = br.rl16();
        dwChannelMask           = br.rl32();
        wSubFormat              = br.rl16();
        for (size_t i = 0; i < 16; ++i)
            subFormat[i]        = br.r8();
        // 18 + 22 = 40 bytes

        DEBUG("valid bits per sample %u.",    wValidBitsPerSample);
        DEBUG("dwChannelMask %#x.",           dwChannelMask);
        DEBUG("sub format %u.",               wSubFormat);

        if (memcmp(subFormat, subformat_base_guid, 12)) {
            ERROR("invalid extensible format %16s.", (char *)subFormat);
            return kMediaErrorBadValue;
        }

        if (dwChannelMask != 0 && __builtin_popcount(dwChannelMask) != nChannels) {
            WARN("channels mask mismatch with channel count.");
            dwChannelMask = 0;
        }
        
        // Fix Format
        wFormatTag  = wSubFormat;
    }

    // sanity check.
    if (wFormatTag == WAVE_FORMAT_PCM || wFormatTag == WAVE_FORMAT_IEEE_FLOAT) {
        // wBitsPerSample/nChannels/nSamplesPerSec, we have to trust these values
        if (wBitsPerSample % 8) {
            ERROR("wBitsPerSample(%u) is wrong.", wBitsPerSample);
            return kMediaErrorBadValue;
        }

        if (!nChannels || !nSamplesPerSec) {
            ERROR("nChannels %d/nSamplesPerSec %d is wrong", nChannels, nSamplesPerSec);
            return kMediaErrorBadValue;
        }

        uint32_t nBytesPerFrame = (nChannels * wBitsPerSample) / 8;
        if (nBlockAlign % nBytesPerFrame) {
            ERROR("nBlockAlign is wrong.");
            //format.nBlockAlign = a;
            return kMediaErrorBadValue;
        }

        // these values we have to trust. and no one will put these wrong.
        uint32_t nBytesPerSec = nSamplesPerSec * nBytesPerFrame;
        if (nBytesPerSec != nAvgBytesPerSec) {
            WARN("nAvgBytesPerSec correction %d -> %d.", nAvgBytesPerSec, nBytesPerSec);
            nAvgBytesPerSec = nBytesPerSec;
        }
    } else {
        if (wBitsPerSample == 1) {
            // means: don't case about this field.
            DEBUG("clear wBitsPerSample(%u).", wBitsPerSample);
            wBitsPerSample = 0;   // set default value
        }
        
        if (nBlockAlign == 0) {
            DEBUG("fix nBlockAlign => 1");
            nBlockAlign = 1;
        }
    }

    return kMediaNoError;
}

BITMAPINFOHEADER::BITMAPINFOHEADER() {
    // TODO: set default values
}

MediaError BITMAPINFOHEADER::parse(BitReader& br) {
    CHECK_GE(br.remianBytes(), BITMAPINFOHEADER_MIN_LENGTH);
    biSize          = br.rl32();
    biWidth         = br.rl32();
    biHeight        = br.rl32();
    biPlanes        = br.rl16();
    biBitCount      = br.rl16();
    biCompression   = br.rl32();
    biSizeImage     = br.rl32();
    biXPelsPerMeter = br.rl32();
    biYPelsPerMeter = br.rl32();
    biClrUsed       = br.rl32();
    biClrImportant  = br.rl32();
    return kMediaNoError;
}

static struct {
    const uint32_t      fourcc;
    const eVideoCodec   format;
} kFourCCMap[] = {
    {FOURCC('MP42'),    kVideoCodecMP42    },
    // END OF LIST
    {0,                 kVideoCodecUnknown }
};

eVideoCodec GetVideoCodec(uint32_t fourcc) {
    for (size_t i = 0; kFourCCMap[i].format != kVideoCodecUnknown; ++i) {
        if (kFourCCMap[i].fourcc == fourcc)
            return kFourCCMap[i].format;
    }
    return kVideoCodecUnknown;
}

__END_NAMESPACE(ASF)
__END_NAMESPACE_MPX
