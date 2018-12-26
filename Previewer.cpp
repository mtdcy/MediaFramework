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


// File:    Track.h
// Author:  mtdcy.chen
// Changes:
//          1. 20181126     initial version
//

#define LOG_TAG "mpx.Previewer"
#define LOG_NDEBUG 0
#include <toolkit/Toolkit.h>

#include "mpx.h"

namespace mtdcy {

    Previewer::Previewer(const Message& formats, const Message& options) :
        PacketQueue(),
        mID(0), mCodec(NULL),
        mHandler(NULL), mRenderer(NULL),
        mFlushed(false)
    {
        // always use external out device for previewer
        CHECK_TRUE(options.contains("RenderEvent"));
        mRenderer = options.find<sp<RenderEvent> >("RenderEvent");

        eCodecFormat codec = (eCodecFormat)formats.findInt32(kKeyFormat);

        Message dup = formats;

        // TODO: let client control this
        dup.setInt32(kKeyMode, kModeTypeSoftware);

        mCodec = MediaCodec::Create(dup);

        if (mCodec == NULL || mCodec->status() != OK) {
            ERROR("create preview codec failed");
        } else {
            mHandler = new PQHandler("previewer", this);
        }
    }

    Previewer::~Previewer() {
        mHandler->removeMessages();
        mHandler->terminate();
    }

#if 0
    status_t Previewer::enqueue(const sp<MediaPacket>& packet) {
        sp<Message> cmd = new Message(kCommandPacketReady);
        cmd->set<sp<MediaPacket> >("packet", packet);
        mHandler->postMessage(cmd);
        return OK;
    }
#endif

    void Previewer::onPacketReady(const sp<MediaPacket>& packet) {
        DEBUG("packet(ts %.3fs) ready", packet->pts.seconds());
        // remove flush cmd from queue
        mHandler->removeMessages(kCommandFlushTimeout);
        if (mFlushed) {
            mCodec->flush();
        }

        CHECK_TRUE(mCodec->write(packet) == OK);

        sp<MediaFrame> frame = mCodec->read();

        if (frame != NULL) {
            mRenderer->fire(frame);
        }

        mHandler->postMessage(kCommandFlushTimeout, 200 * 1000LL);
    }

    void Previewer::onFlush() {
        DEBUG("end of preview sequence? flush");
        // enter draining mode
        mCodec->write(NULL);

        for (;;) {
            sp<MediaFrame> frame = mCodec->read();
            if (frame != NULL)  mRenderer->fire(frame);
            else break;
        }

        // flush complete
        mFlushed = true;
    }

    void Previewer::handleMessage(const sp<Message>& msg) {
        switch (msg->what()) {
            case kCommandPacketReady:
                {
                    sp<MediaPacket> packet = msg->find<sp<MediaPacket> >("packet");
                    onPacketReady(packet);
                } break;
            case kCommandFlushTimeout:
                onFlush();
                break;
            default:
                FATAL("should NOT be here");
                break;
        }
    }
}
