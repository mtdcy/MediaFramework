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


// File:    Libavcodec.h
// Author:  mtdcy.chen
// Changes: 
//          1. 20160701     initial version
//

#ifndef _MEDIA_MODULES_LAVC_VIDEO_H
#define _MEDIA_MODULES_LAVC_VIDEO_H

#include <modules/MediaDefs.h>
extern "C" {
#include <libavcodec/avcodec.h>
}

#include "pcm/pcm_convert.h"
#include "pcm/pcm_interleave.h"
#include "pcm/pcm_utils.h"

// TODO:
//  only ffmpeg native codec here
namespace mtdcy { namespace Lavc {

    class Decoder : public MediaCodec {
        public:
            Decoder(const Message& params);

            virtual ~Decoder();

            virtual String          string() const;
            virtual status_t        status() const;

            virtual const Message&  formats() const;
            virtual status_t        configure(const Message& options);

            virtual status_t        write(const sp<MediaPacket>& input);
            virtual sp<MediaFrame>  read();

            virtual status_t        flush();

        private:
            sp<Message>             buildVideoFormat();
            sp<Message>             buildAudioFormat();
            sp<Buffer>              postProcessing(AVFrame *raw);

            sp<MediaFrame>  readAudio();
            sp<MediaFrame>  readVideo();

        private:
            eCodecType              mType;
            int32_t                 mMode;
            AVCodecContext *        mContext;

            // statistics
            size_t                  mInputCount;
            size_t                  mOutputCount;

        private:
            DISALLOW_EVILS(Decoder);
    };
}; };

#endif 
