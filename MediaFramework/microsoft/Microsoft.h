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

#ifndef _MPX_MEDIA_ASF_H
#define _MPX_MEDIA_ASF_H

#include "MediaTypes.h"

__BEGIN_NAMESPACE_MPX
__BEGIN_NAMESPACE(Microsoft)

// refer to: https://tools.ietf.org/html/rfc2361
enum {
    WAVE_FORMAT_PCM         = 0x0001,
    WAVE_FORMAT_IEEE_FLOAT  = 0x0003,
    WAVE_FORMAT_EXTENSIBLE  = 0xFFFE,
};

// refer to:
// 1. http://wiki.multimedia.cx/index.php?title=WAVEFORMATEX
// 2. http://wiki.multimedia.cx/index.php?title=WAVEFORMATEXTENSIBLE

#define WAVEFORMATEX_MIN_LENGTH (16)
#define WAVEFORMATEX_MAX_LENGTH (40)
struct WAVEFORMATEX {
    // >> 16 bytes
    UInt16 wFormat;
    UInt16 nChannels; 
    UInt32 nSamplesPerSec; 
    UInt32 nAvgBytesPerSec; 
    UInt16 nBlockAlign; 
    UInt16 wBitsPerSample;
    // < 16 bytes
    UInt16 cbSize;
    // < 18 bytes
    
    union {
        UInt16 wSamplesPerBlock;
        UInt16 wReserved;
        UInt16 wValidBitsPerSample;   // cbSize >= 22
    };
    UInt32 dwChannelMask;
    UInt16 wSubFormat;                // << parse from subFormat GUID(16 bytes)
    // < 40 bytes
    
    WAVEFORMATEX();
    
    MediaError  parse(const sp<ABuffer>&);
    // bw MUST have WAVEFORMATEX_MAX_LENGTH bytes
    MediaError  compose(sp<ABuffer>&) const;
};

// refer to:
// 1. http://msdn.microsoft.com/en-us/library/dd183376.aspx
#define BITMAPINFOHEADER_MIN_LENGTH (40)
struct BITMAPINFOHEADER {
    UInt32     biSize;
    UInt32     biWidth;
    UInt32     biHeight;
    UInt16     biPlanes;
    UInt16     biBitCount;
    UInt32     biCompression;
    UInt32     biSizeImage;
    UInt32     biXPelsPerMeter;
    UInt32     biYPelsPerMeter;
    UInt32     biClrUsed;
    UInt32     biClrImportant;
    // 40 bytes
    
    BITMAPINFOHEADER();
    
    MediaError parse(const sp<ABuffer>&);
};

__END_NAMESPACE(Microsoft)
__END_NAMESPACE_MPX

#endif // _MPX_MEDIA_ASF_H;
