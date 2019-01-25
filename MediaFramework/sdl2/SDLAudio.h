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
 * 3. Neither the name of the copyright holder nor the names of its 
 *    contributors may be used to endorse or promote products derived from 
 *    this software without specific prior written permission.
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


// File:    SDLAudio.h
// Author:  mtdcy.chen
// Changes: 
//          1. 20160701     initial version
//
// Notes:
//  SDL is always NOT the best way, using platform method instead

#ifndef MODULES_SDL_AUDIO_H
#define MODULES_SDL_AUDIO_H

#include <MediaFramework/MediaDefs.h>
#include <MediaFramework/MediaOut.h>

namespace mtdcy {

    class SDLAudio : public MediaOut {
        public:
            SDLAudio();
            virtual ~SDLAudio();

        public:
            virtual String          string() const;
            virtual status_t        prepare(const Message& options);
            virtual status_t        status() const;
            virtual Message         formats() const;
            virtual status_t        configure(const Message& options);
            virtual status_t        write(const sp<MediaFrame>& input);
            virtual status_t        flush();

        private:
            static void             onSDLCallback(void *opaque, uint8_t *stream, int len);
            void                    eatFrame(uint8_t *buffer, size_t len);

            bool                    mInitByUs;
            sp<Message>             mFormat;
            uint16_t                mSampleFormat;  // SDL_AudioFormat
            Mutex                   mLock;
            Condition               mWait;
            sp<Buffer>              mPendingFrame;
            bool                    mFlushing;
            size_t                  mSilence;

        private:
            DISALLOW_EVILS(SDLAudio);
    };
};


#endif
