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


// File:    MediaFrame.h
// Author:  mtdcy.chen
// Changes: 
//          1. 20160701     initial version
//

#ifndef _MEDIA_FRAME_H
#define _MEDIA_FRAME_H

#include <MediaToolkit/Toolkit.h>
#include <MediaFramework/MediaTime.h>

__BEGIN_DECLS 

__END_DECLS 

#ifdef __cplusplus
namespace mtdcy {
    /**
     * media frame class for decompressed audio and video frames
     */
    struct MediaFrame {
        MediaFrame() : pts(kTimeInvalid), duration(kTimeInvalid) { }

        sp<Buffer>          data[8];        ///< frame data&size, packed or plannar
        MediaTime           pts;            ///< display time in us
        MediaTime           duration;       ///< duration of this frame
        union {
            int32_t             format;     ///< sample format, @see ePixelFormat, @see eSampleFormat
            struct {
                eSampleFormat   format;     ///< audio sample format, @see eSampleFormat
                int32_t         channels;   ///< channel count
                int32_t         rate;       ///< sampling rate
                int32_t         samples;    ///< samples per channel
            } a;
            struct {
                ePixelFormat    format;     ///< video pixel format, @see ePixelFormat
                int32_t         width;      ///< display width
                int32_t         height;     ///< display height
                int32_t         strideWidth;///< stride/slice width >= display width
                int32_t         sliceHeight;///< slice height >= display height
            } v;
            struct {
                // TODO
            } s;
        };
    };
};
#endif

#endif // _MEDIA_FRAME_H 
