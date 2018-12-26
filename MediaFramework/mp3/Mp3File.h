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


// File:    Mp3File.h
// Author:  mtdcy.chen
// Changes: 
//          1. 20160701     initial version
//

#ifndef _MEDIA_MODULES_MP3_FILE_H
#define _MEDIA_MODULES_MP3_FILE_H

#include <MediaFramework/MediaDefs.h>
#include <MediaFramework/MediaExtractor.h>

namespace mtdcy {
    class RingBuffer;
    class Mp3Packetizer : public Packetizer {
        public:
            Mp3Packetizer();

            virtual ~Mp3Packetizer();

            virtual String          string() const;

        public:
            virtual bool            enqueue(const Buffer&);
            virtual bool            dequeue(Buffer&);
            virtual void            flush();

        private:
            Buffer                  mInternalBuffer;
            uint32_t                mCommonHead;

            bool                    mNeedMoreData;
            bool                    mFlushing;

        private:
            DISALLOW_EVILS(Mp3Packetizer);
    };

    class Mp3File : public MediaExtractor {
        public:
            Mp3File(const sp<Content>& pipe, const Message& params);

            virtual ~Mp3File();

            // locate the first MPEG audio frame in data, and return frame length
            // and offset in bytes. NO guarantee
            static ssize_t          locateFirstFrame(const Buffer& data, size_t *frameLength);

            // decode frame header and return frame length in bytes. NO guarantee
            static ssize_t          decodeFrameHeader(uint32_t head);

        public:
            virtual String          string() const;
            virtual status_t        status() const;
            virtual Message         formats() const;
            virtual status_t        configure(const Message& options);
            virtual sp<MediaPacket> read(size_t,
                                         eModeReadType,
                                         const MediaTime& ts = kTimeInvalid);

        private:
            bool                    parseXingHeader(const Buffer& firstFrame, Message& meta);
            bool                    parseVBRIHeader(const Buffer& firstFrame);

            sp<Content>             mContent;
            sp<Message>             mFormats;
            int64_t                 mFirstFrameOffset;

            bool                    mVBR;

            int32_t                 mNumFrames;
            int64_t                 mNumBytes;
            int32_t                 mBitRate;
            int64_t                 mDuration; // us

            Vector<int64_t>         mTOC;

            int64_t                 mAnchorTime;
            int32_t                 mSamplesRead;

            sp<Buffer>              mPartialFrame;
            sp<Packetizer>          mPacketizer;

        private:
            DISALLOW_EVILS(Mp3File);
    };
};

#endif // _MEDIA_MODULES_MP3_FILE_H
