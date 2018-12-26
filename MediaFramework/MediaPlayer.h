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


/** 
 * File:    mpx.h 
 * Author:  mtdcy.chen
 * Changes: 
 *          1. 20181214     initial version
 *
 */

#ifndef _MPX_H
#define _MPX_H 

#include <MediaToolkit/Toolkit.h>
#include <MediaFramework/MediaClock.h>
#include <MediaFramework/MediaSession.h>

namespace mtdcy {
    /**
     * For handle error.
     */ 
    typedef Event<status_t> ErrorEvent;

    // for video preview or thumbnail
    class Previewer {
        public:
            Previewer(const Message&, const Message&);
            virtual ~Previewer();
            virtual status_t status() const { return OK; }
            virtual status_t prepare()  { return OK; }
            virtual status_t start()    { return OK; }
            virtual status_t stop()     { return OK; }
            virtual status_t flush()    { return OK; }
            virtual status_t seek(int64_t ts) { return OK; }

        private:
            size_t                  mID;
            sp<MediaDecoder>          mCodec;
            sp<Handler>             mHandler;
            sp<RenderEvent>         mRenderer;
            bool                    mFlushed;

        private:
            virtual void handleMessage(const sp<Message>&);
            enum {
                kCommandPacketReady,
                kCommandFlushTimeout,
            };
            void onPacketReady(const sp<MediaPacket>&);
            void onFlush();

        private:
            DISALLOW_EVILS(Previewer);
    };

    class MediaPlayer {
        public:
            /**
             * create a engine with options
             * about options:
             *  "RenderPositionEvent"   - [sp<RenderPositionEvent>] - optional
             * @param options   option and parameter for engine
             */
            MediaPlayer(const Message& options);
            virtual ~MediaPlayer();

        public:
            /**
             * add a media to engine.
             * about options:
             *  "RenderEvent"   - [sp<RenderEvent>]     - optional
             *  "SDL_Window"    - [pointer|void *]      - optional
             *  "StartTime"     - [double|seconds]      - optional
             *  "EndTime"       - [double|seconds]      - optional
             * if RenderEvent exists, external renderer will be used.
             * if RenderEvent not exists, SDL_Window must exists for
             * init internal renderer.
             * @param url   url of the media
             * @param options option and parameter for this media
             * @return return OK on success, otherwise error code
             */
            status_t addMedia(const String& url, const Message& options);
            /**
             * prepare engine after add media
             * @return return OK on success, otherwise error code
             */
            status_t prepare();
            /**
             * return status of current engine. can be called in any stage
             * as it will check different conditions on different stage.
             * @return return OK on success, otherwise error code
             */
            status_t status() const { return mStatus; }
            /**
             * start this engine.
             * @return return OK on success, otherwise error code.
             * @note there must be at least one media exists and start success.
             */
            status_t start();
            /**
             * pause this engine.
             * @return return OK on success, otherwise error code.
             */
            status_t pause();
            /**
             * stop this engine.
             * @return return OK on success, otherwise error code.
             */
            status_t stop();
            /**
             * seek to a new position
             * @param ts timestamp in us.
             * @return return OK on success, otherwise error code.
             */
            status_t seek(int64_t ts);
        
            /**
             * the state machine's value
             */
            enum eStateType {
                kStateInvalid,
                kStateInit,
                kStateReady,
                kStatePlaying,
                kStatePaused,
                kStateStopped,
            };
            /**
             * return current state
             * @return @see eStateType
             */
            eStateType state() const;

        private:
            void setError_l(status_t);

        private:
            // for PacketQueue request packet
            friend class OnRequestPacket;
            void onRequestPacket(size_t, PacketRequestPayload);

            // for PacketQueue update render position
            friend class OnUpdateRenderPosition;
            void onUpdateRenderPosition(size_t, const MediaTime&);

            // update render position to client
            friend class UpdateRenderPosition;
            void updateRenderPosition();

        private:
            // external static context
            sp<RenderPositionEvent> mPositionEvent;
        

            // internal context
            // static context
            sp<Looper> mLooper;
        
            // volatile context
            volatile status_t mStatus;
            volatile eStateType mState;

            mutable Mutex mLock;
            struct SessionContext;
            HashTable<size_t, sp<SessionContext> > mContext;
            size_t mNextId;

            bool mHasAudio;

            sp<SharedClock> mClock;
            sp<Runnable>    mUpdateRenderPosition;

        private:
            DISALLOW_EVILS(MediaPlayer);
    };
}

#endif //
