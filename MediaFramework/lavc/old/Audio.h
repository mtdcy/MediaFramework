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


// File:    Audio.h
// Author:  mtdcy.chen
// Changes: 
//          1. 20160701     initial version
//

#ifndef _MEDIA_MODULES_LAVC_AUDIO_H
#define _MEDIA_MODULES_LAVC_AUDIO_H

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

    class Audio: public MediaCodec {
        public:
            Audio(int32_t codec, const Message* params = NULL);

            virtual ~Audio();

            virtual String          string() const;
            virtual status_t        status() const;

            virtual const Message&  formats() const;
            virtual status_t        configure(const Message& options);

            virtual status_t        write(const sp<MediaPacket>& input);
            virtual sp<MediaFrame>  read();

            virtual status_t        flush();

        private:
            sp<Message>             buildAudioFormat();

        private:
            const String            mCodecName;
            status_t                mStatus;
            sp<Message>             mFormats;

            AVCodecContext          *mContext;
            pcm_hook_t              mHook;
            pcm_interleave_hook_t   mHook2;

        private:
            DISALLOW_EVILS(Audio);
    };
}; };

#endif 