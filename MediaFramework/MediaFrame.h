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

#define MEDIA_FRAME_NB_PLANES   (8)

__END_DECLS 

#ifdef __cplusplus
namespace mtdcy {
#endif
    
    /**
     * media frame structure for decompressed audio and video frames
     * the properties inside this structure have to make sure this
     * frame can be renderred properly without additional informations.
     */
    struct MediaFrame {
        MediaTime               pts;        ///< display time in us
        MediaTime               duration;   ///< duration of this frame
        /**
         * plane data struct.
         * for planar frame, multi planes must exist. the backend memory may be
         * or may not be continueslly.
         * for packed frame, only one plane exists.
         */
        struct {
            uint8_t *           data;       ///< plane data
            size_t              size;       ///< data size in bytes
        } planes[MEDIA_FRAME_NB_PLANES];    ///< for packed frame, only one plane exists
        
        union {
            int32_t             format;     ///< sample format, @see ePixelFormat, @see eSampleFormat
            struct {
                eSampleFormat   format;     ///< audio sample format, @see eSampleFormat
                int32_t         channels;   ///< channel count
                int32_t         freq;       ///< sampling rate
                int32_t         samples;    ///< samples per channel
            } a;
            struct {
                ePixelFormat    format;     ///< video pixel format, @see ePixelFormat
                int32_t         width;      ///< plane width
                int32_t         height;     ///< plane height
                struct {
                    int32_t     x, y, w, h;
                } rect;                     ///< display rectangle
            } v;
            struct {
                // TODO
            } s;
        };
        void                    *opaque;    ///< opaque

#ifdef __cplusplus
        MediaFrame();
        virtual ~MediaFrame() { }
#endif
    };
    
    /**
     * create a video frame backend by Buffer
     */
    sp<MediaFrame>  MediaFrameCreate(ePixelFormat format, int32_t width, int32_t height);
    
    /**
     * create a audio frame
     */
    sp<MediaFrame>  MediaFrameCreate(eSampleFormat format, bool planar, int32_t channels, int32_t freq, int32_t samples);
  
#ifdef __cplusplus
};
#endif

#endif // _MEDIA_FRAME_H 
