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


// File:    ASF.h
// Author:  mtdcy.chen
// Changes: 
//          1. 20160701     initial version
//

#ifndef _MEDIA_MODULES_ASF_UTILS_H
#define _MEDIA_MODULES_ASF_UTILS_H

#include <toolkit/Toolkit.h>

#ifdef __cplusplus 

namespace mtdcy {
    namespace ASF {

        //! refer to ffmpeg::riff.c::ff_codec_wav_tags
        enum {
            TAG_WMA_V1              = 0x160,
            TAG_WMA_V2              = 0x161,
            TAG_WMA_PRO             = 0x162,
            TAG_WMA_LOSSLESS        = 0x163,
        };

        static uint8_t subformat_base_guid[] = {
            0x00, 0x00, 0x10, 0x00, 0x80, 0x00, 0x00, 0xAA, 0x00, 0x38, 0x9B, 0x71
        };

        // refer to:
        // 1. http://wiki.multimedia.cx/index.php?title=WAVEFORMATEX
        typedef struct { 
            uint16_t wFormatTag; 
            uint16_t nChannels; 
            uint32_t nSamplesPerSec; 
            uint32_t nAvgBytesPerSec; 
            uint16_t nBlockAlign; 
            uint16_t wBitsPerSample; 
            uint16_t cbSize; 
        } WAVEFORMATEX; 

        // refer to:
        // 1. http://wiki.multimedia.cx/index.php?title=WAVEFORMATEXTENSIBLE
        typedef struct {
            uint16_t wFormatTag; 
            uint16_t nChannels; 
            uint32_t nSamplesPerSec; 
            uint32_t nAvgBytesPerSec; 
            uint16_t nBlockAlign; 
            uint16_t wBitsPerSample; 
            uint16_t cbSize; 
            union {
                uint16_t wValidBitsPerSample;
                uint16_t wSamplesPerBlock;
                uint16_t wReserved;
            };
            uint32_t dwChannelMask; 
            uint8_t subFormat[16];
        } WAVEFORMATEXTENSIBLE;

        // refer to:
        // 1. http://msdn.microsoft.com/en-us/library/dd183376.aspx
        struct BITMAPINFOHEADER {
            uint32_t biSize;
            uint32_t biWidth;
            uint32_t biHeight;
            uint16_t biPlanes;
            uint16_t biBitCount;
            uint32_t biCompression;
            uint32_t biSizeImage;
            uint32_t biXPelsPerMeter;
            uint32_t biYPelsPerMeter;
            uint32_t biClrUsed;
            uint32_t biClrImportant;
        };
    };
};
#endif // __cplusplus 

#endif // tkMODULES_ASF_UTILS_H;
