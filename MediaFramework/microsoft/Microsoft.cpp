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

#define LOG_TAG "Microsoft"
#define LOG_NDEBUG 0

#include "Microsoft.h"

__BEGIN_NAMESPACE_MPX
__BEGIN_NAMESPACE(Microsoft)

static uint8_t subformat_base_guid[12] = {
    0x00, 0x00, 0x10, 0x00, 0x80, 0x00, 0x00, 0xAA, 0x00, 0x38, 0x9B, 0x71
};

WAVEFORMATEX::WAVEFORMATEX() {
    // set default values
    wFormat             = WAVE_FORMAT_PCM;
    nChannels           = 2;
    nSamplesPerSec      = 8000;
    nAvgBytesPerSec     = 32000;
    nBlockAlign         = 4;
    wBitsPerSample      = 16;
    cbSize              = 0;
    wValidBitsPerSample = 0;
    dwChannelMask       = 0;
    wSubFormat          = 0;
}

MediaError WAVEFORMATEX::parse(const sp<ABuffer>& buffer) {
    if (buffer->size() < WAVEFORMATEX_MIN_LENGTH)
    return kMediaErrorBadValue;
    
    // valid chunk length: 16, 18, 40
    // but we have saw chunk length 50
    wFormat             = buffer->rl16();
    nChannels           = buffer->rl16();
    nSamplesPerSec      = buffer->rl32();
    nAvgBytesPerSec     = buffer->rl32();
    nBlockAlign         = buffer->rl16();
    wBitsPerSample      = buffer->rl16();
    // 16 bytes
    
    if (buffer->size() > 2) {     // >= 18
        cbSize          = buffer->rl16();
    }
    if (cbSize == 0)    return kMediaNoError;
    // 18 bytes

    DEBUG("audio format %u.",     wFormat);
    DEBUG("number channels %u.",  nChannels);
    DEBUG("sample rate %u.",      nSamplesPerSec);
    DEBUG("byte rate %u.",        nAvgBytesPerSec);
    DEBUG("block align %u.",      nBlockAlign);
    DEBUG("bits per sample %u.",  wBitsPerSample);

    if (wFormat == WAVE_FORMAT_EXTENSIBLE) {
        DEBUG("fmt chunk with extensible format.");
        // 40 - 18 == 22
        if (buffer->size() < 22) {
            DEBUG("invalid fmt chunk length %zu.", 18 + buffer->size());
            return kMediaErrorBadValue;
        }

        wValidBitsPerSample     = buffer->rl16();
        dwChannelMask           = buffer->rl32();
        
        uint8_t subFormat[16];
        for (size_t i = 0; i < 16; ++i)
            subFormat[i]        = buffer->r8();
        // 18 + 22 = 40 bytes

        DEBUG("valid bits per sample %u.",    wValidBitsPerSample);
        DEBUG("dwChannelMask %#x.",           dwChannelMask);

        if (memcmp(subFormat, subformat_base_guid, 12)) {
            ERROR("invalid extensible format %16s.", (char *)subFormat);
            return kMediaErrorBadValue;
        }
        
        // TODO: parse subFormat GUID
        DEBUG("sub format %u.",               wSubFormat);

        if (dwChannelMask != 0 && __builtin_popcount(dwChannelMask) != nChannels) {
            WARN("channels mask mismatch with channel count.");
            dwChannelMask = 0;
        }
    }

    // sanity check.
    if (wFormat == WAVE_FORMAT_PCM || wFormat == WAVE_FORMAT_IEEE_FLOAT) {
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

MediaError WAVEFORMATEX::compose(sp<ABuffer>& buffer) const {
    CHECK_GE(buffer->capacity(), WAVEFORMATEX_MAX_LENGTH);
    buffer->wl16(wFormat);
    buffer->wl16(nChannels);
    buffer->wl32(nSamplesPerSec);
    buffer->wl32(nAvgBytesPerSec);
    buffer->wl16(nBlockAlign);
    buffer->wl16(wBitsPerSample);
    // 16 bytes
    buffer->wl16(cbSize);    // always write cbSize, even it is 0
    // 18 bytes
    
    if (cbSize == 22) {
        buffer->wl16(wValidBitsPerSample);
        buffer->wl32(dwChannelMask);
        // TODO write subFormat GUID
    }
    return kMediaNoError;
}

BITMAPINFOHEADER::BITMAPINFOHEADER() {
    // TODO: set default values
}

MediaError BITMAPINFOHEADER::parse(const sp<ABuffer>& buffer) {
    CHECK_GE(buffer->size(), BITMAPINFOHEADER_MIN_LENGTH);
    biSize          = buffer->rl32();
    biWidth         = buffer->rl32();
    biHeight        = buffer->rl32();
    biPlanes        = buffer->rl16();
    biBitCount      = buffer->rl16();
    biCompression   = buffer->rl32();
    biSizeImage     = buffer->rl32();
    biXPelsPerMeter = buffer->rl32();
    biYPelsPerMeter = buffer->rl32();
    biClrUsed       = buffer->rl32();
    biClrImportant  = buffer->rl32();
    return kMediaNoError;
}

__END_NAMESPACE(Microsoft)
__END_NAMESPACE_MPX
