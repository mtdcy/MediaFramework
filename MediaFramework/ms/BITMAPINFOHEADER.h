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

#ifndef _MPX_MEDIA_MS_BITMAPINFOHEADER_H
#define _MPX_MEDIA_MS_BITMAPINFOHEADER_H

#include <ABE/ABE.h>
#include "MediaDefs.h"

__BEGIN_NAMESPACE_MPX
__BEGIN_NAMESPACE(MS)

#define BITMAPINFOHEADER_MIN_LENGTH (40)

struct BITMAPINFOHEADER {
    uint32_t     biSize;
    uint32_t     biWidth;
    uint32_t     biHeight;
    uint16_t     biPlanes;
    uint16_t     biBitCount;
    uint32_t     biCompression;
    uint32_t     biSizeImage;
    uint32_t     biXPelsPerMeter;
    uint32_t     biYPelsPerMeter;
    uint32_t     biClrUsed;
    uint32_t     biClrImportant;
    // 40 bytes
    
    BITMAPINFOHEADER(BitReader& br);
};

__END_NAMESPACE(MS)
__END_NAMESPACE_MPX

#endif
