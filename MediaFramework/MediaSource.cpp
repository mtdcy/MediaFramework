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


// File:    MediaSource.cpp
// Author:  mtdcy.chen
// Changes:
//          1. 20181126     initial version
//

#define LOG_TAG "MediaSource"
//#define LOG_NDEBUG 0
#include "MediaSession.h"
#include "MediaFile.h"

__USING_NAMESPACE_MPX

// for MediaSession request packet, which always run in player's looper
// TODO: seek

struct MediaSource : public IMediaSession {
    // external static context
    String                  mUrl;
    sp<SessionInfoEvent>    mInfoEvent;
    // internal mutable context
    sp<MediaFile>           mMediaFile;
    typedef List<sp<MediaPacket> > PacketList;
    Vector<PacketList>      mPackets;
    MediaTime               mLastReadTime;  //< avoid seek multi times by different track
    BitSet                  mTrackMask;
    
    MediaSource(const sp<Message>& media, const sp<Message>& options) :
    IMediaSession(new Looper("source")),
    mUrl(media->findString("url")),
    mMediaFile(NULL), mLastReadTime(kMediaTimeBegin)
    {
        mInfoEvent = options->findObject("SessionInfoEvent");
    }
    
    void notify(const eSessionInfoType& info, const sp<Message>& payload) {
        if (mInfoEvent.isNIL()) return;
        
        mInfoEvent->fire(info, payload);
    }
    
    virtual void onInit() {
        DEBUG("onInit...");
        sp<Content> pipe = Content::Create(mUrl);
        if (pipe == NULL) {
            ERROR("create pipe failed");
            notify(kSessionInfoError, NULL);
            return;
        }
        
        mMediaFile = MediaFile::Create(pipe);
        if (mMediaFile.isNIL()) {
            ERROR("create file failed");
            notify(kSessionInfoError, NULL);
            return;
        }
        
        sp<Message> formats = mMediaFile->formats()->dup();
        size_t numTracks = formats->findInt32(kKeyCount, 1);
        for (size_t i = 0; i < numTracks; ++i) {
            String trackName = String::format("track-%zu", i);
            sp<Message> trackFormat = formats->findObject(trackName);
            trackFormat->setObject("PacketRequestEvent", new OnPacketRequest(this, i));
            // init packet queues
            mPackets.push();
            
            mTrackMask.set(i);
        }
        
        notify(kSessionInfoReady, formats);
    }
    
    virtual void onRelease() {
        DEBUG("onRelease...");
        mMediaFile.clear();
    }
    

    struct OnPacketRequest : public PacketRequestEvent {
        MediaSource *thiz;
        const size_t trackIndex;
        
        OnPacketRequest(MediaSource *p, const size_t index) :
        PacketRequestEvent(Looper::Current()),
        thiz(p), trackIndex(index) { }
        
        virtual void onEvent(const sp<PacketReadyEvent>& event, const MediaTime& time) {
            thiz->onRequestPacket(trackIndex, event, time);
        }
        
        virtual void onFirstRetain() {
            thiz->onEnableTrack(trackIndex);
        }
        
        virtual void onLastRetain() {
            thiz->onDisableTrack(trackIndex);
        }
    };
    
    void onRequestPacket(const size_t index, sp<PacketReadyEvent> event, const MediaTime& time) {
        DEBUG("onRequestPacket [%zu] @ %.3f", index, time.seconds());
        sp<MediaPacket> packet = read(index, time);
        if (packet.isNIL()) {
            INFO("End Of Input");
            notify(kSessionInfoEnd, NULL);
        }
        event->fire(packet);
    }
    
    void onEnableTrack(const size_t index) {
        mTrackMask.set(index);
        sp<Message> options = new Message;
    }
    
    void onDisableTrack(const size_t index) {
        mTrackMask.clear(index);
    }
    
    sp<MediaPacket> read(size_t index, const MediaTime& time) {
        if (time != kMediaTimeInvalid && time != mLastReadTime) {
            INFO("performing seek by track %zu", index);
            // FIXME: seek only once
            for (size_t i = 0; i < mPackets.size(); ++i) {
                mPackets[i].clear();
            }
            
            sp<MediaPacket> packet = mMediaFile->read(kReadModeClosestSync, time);
            if (packet.isNIL()) {
                INFO("eos");
                return NIL;
            }
            
            mPackets[packet->index].push(packet);
            mLastReadTime = time;
        }
        
        PacketList& list = mPackets[index];
        if (list.size()) {
            sp<MediaPacket> packet = list.front();
            list.pop();
            return packet;
        }
        
        for (;;) {
            sp<MediaPacket> packet = mMediaFile->read();
            if (packet.isNIL()) {
                INFO("eos");
                break;
            }
            
            if (packet->index == index) {
                return packet;
            }
            
            mPackets[packet->index].push(packet);
        }
        
        return NIL;
    }
};

sp<IMediaSession> CreateMediaSource(const sp<Message>& media, const sp<Message>& options) {
    return new MediaSource(media, options);
}
