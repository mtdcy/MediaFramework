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


// File:    MediaFile.cpp
// Author:  mtdcy.chen
// Changes:
//          1. 20181126     initial version
//

#define LOG_TAG "MediaFile"
//#define LOG_NDEBUG 0
#include "MediaTypes.h"
#include "MediaDevice.h"
#include "MediaSession.h"

__BEGIN_NAMESPACE_MFWK

struct MediaFile : public IMediaSession {
    // external static context
    sp<SessionInfoEvent>    mInfoEvent;
    // internal mutable context
    sp<MediaDevice>         mMediaFile;
    typedef List<sp<MediaFrame> > PacketList;
    Vector<PacketList>      mPackets;
    MediaTime               mLastReadTime;  //< avoid seek multi times by different track
    Bits<UInt32>          mTrackMask;
    struct OnPacketRequest;
    List<sp<OnPacketRequest> > mRequestEvents;
    Bool                    mEndOfSource;
    
    MediaFile(const sp<Looper>& lp) : IMediaSession(lp),
    mMediaFile(Nil), mLastReadTime(kMediaTimeInvalid), mEndOfSource(False)
    {
        
    }
    
    void notify(const eSessionInfoType& info, const sp<Message>& payload) {
        if (mInfoEvent.isNil()) return;
        
        mInfoEvent->fire(info, payload);
    }
    
    virtual void onInit(const sp<Message>& media, const sp<Message>& options) {
        DEBUG("onInit...");
        if (!options.isNil()) {
            mInfoEvent = options->findObject(kKeySessionInfoEvent);
        }
        
        String url = media->findString(kKeyURL);
        sp<ABuffer> pipe = Content::Create(url);
        if (pipe == Nil) {
            ERROR("create pipe failed");
            notify(kSessionInfoError, Nil);
            return;
        }
        
        sp<Message> formats = new Message;
        formats->setObject(kKeyContent, pipe);
        mMediaFile = MediaDevice::create(formats, Nil);
        if (mMediaFile.isNil()) {
            ERROR("create file failed");
            notify(kSessionInfoError, Nil);
            return;
        }
        
        formats = mMediaFile->formats();
        UInt32 numTracks = formats->findInt32(kKeyCount, 1);
        for (UInt32 i = 0; i < numTracks; ++i) {
            sp<Message> trackFormat = formats->findObject(kKeyTrack + i);
            sp<OnPacketRequest> event = new OnPacketRequest(this, i);
            trackFormat->setObject(kKeyPacketRequestEvent, event);
            // init packet queues
            mPackets.push();
            mRequestEvents.push(event);
            mTrackMask.set(i);
        }
        
        formats->setObject(kKeyTrackSelectEvent, new OnTrackSelect(this));
        notify(kSessionInfoReady, formats);
        
        // make sure each track has at least one packet
        fillPacket();
        DEBUG("we are ready...");
    }
    
    void fillPacket(const MediaTime& time = kMediaTimeInvalid) {
        Bits<UInt32> trackMask;
        
        // avoid seek multitimes by different track
        Bool seek = time != kMediaTimeInvalid && time != mLastReadTime;
        if (seek) {
            // flush packet list before seek
            for (UInt32 i = 0; i < mPackets.size(); ++i) {
                mPackets[i].clear();
                if (mTrackMask.test(i)) trackMask.set(i);
            }
        } else {
            for (UInt32 i = 0; i < mPackets.size(); ++i) {
                if (mPackets[i].empty() && mTrackMask.test(i))
                    trackMask.set(i);
            }
        }
                
        while (!trackMask.empty()) {
            sp<MediaFrame> packet;
            if (ABE_UNLIKELY(seek)) {
                INFO("seek to %.3f", time.seconds());
                sp<Message> options = new Message;
                options->setInt64(kKeySeek, time.useconds());
                if (mMediaFile->configure(options) != kMediaNoError) {
                    ERROR("seek failed");
                }
                mLastReadTime   = time;
                seek            = False;
            }
            packet = mMediaFile->pull();
            
            if (packet.isNil()) {
                INFO("End Of File...");
                break;
            }
            
            PacketList& list = mPackets[packet->id];
            list.push(packet);
            DEBUG("[%zu] fill one packet, total %zu", packet->index, list.size());
            
            trackMask.clear(packet->id);
        }
        
        if (trackMask.empty()) DEBUG("packet lists are ready");
      
#if 0
        String string = "packet list:";
        for (UInt32 i = 0; i < mPackets.size(); ++i) {
            string += String::format(" [%zu] %zu,", i, mPackets[i].size());
        }
        INFO("%s", string.c_str());
#endif
    }
    
    virtual void onRelease() {
        DEBUG("onRelease...");
        mDispatch->flush();
        mMediaFile.clear();
        List<sp<OnPacketRequest> >::iterator it = mRequestEvents.begin();
        for (; it != mRequestEvents.end(); ++it) {
            (*it)->invalidate();
        }
        mRequestEvents.clear();
    }
    
    struct OnTrackSelect : public TrackSelectEvent {
        MediaFile *thiz;
        OnTrackSelect(MediaFile *p) :
        TrackSelectEvent(p->mDispatch),
        thiz(p) { }
        
        virtual void onEvent(const UInt32& tracks) {
            if (thiz == Nil) return;
            thiz->onTrackSelect(tracks);
        }
        
        void invalidate() { thiz = Nil; }
    };
    
    void onTrackSelect(const UInt32& mask) {
        mTrackMask = mask;
        sp<Message> options = new Message;
        options->setInt32(kKeyTracks, mask);
        mMediaFile->configure(options);
        
        // clear packet list
        for (UInt32 i = 0; i < mPackets.size(); ++i) {
            if (mTrackMask.test(i)) continue;;
            mPackets[i].clear();
        }
    }

    struct OnPacketRequest : public PacketRequestEvent {
        MediaFile *thiz;
        const UInt32 trackIndex;
        
        OnPacketRequest(MediaFile *p, const UInt32 index) :
        PacketRequestEvent(p->mDispatch),
        thiz(p), trackIndex(index) { }
        
        virtual void onEvent(const sp<PacketReadyEvent>& event, const MediaTime& time) {
            if (thiz == Nil) {
                WARN("request packet after invalid()");
                return;
            }
            thiz->onRequestPacket(trackIndex, event, time);
        }
        
        void invalidate() { thiz = Nil; }
        
        // when all reference gone, we have to disable the track
        virtual void onLastRetain() {
            if (thiz == Nil) return;
            INFO("disable track on PacketRequestEvent GONE");
            thiz->onDisableTrack(trackIndex);
        }
    };
    
    void onRequestPacket(const UInt32 index, sp<PacketReadyEvent> event, const MediaTime& time) {
        DEBUG("onRequestPacket [%zu] @ %.3f", index, time.seconds());
        
        if (time != kMediaTimeInvalid) {
            INFO("onRequestPacket [%zu] @ %.3f", index, time.seconds());
            mEndOfSource = False;
            fillPacket(time);
        }
        
        PacketList& list = mPackets[index];
        
        if (list.empty()) {
            INFO("[%zu] End Of Stream", index);
            mEndOfSource = True;
            event->fire(Nil);
            return;
        }
        
        sp<MediaFrame> packet = list.front();
        list.pop();
        
        if (time != kMediaTimeInvalid) {
            INFO("first packet @ %.3fs", packet->timecode.seconds());
        }
        
        DEBUG("send %s", packet->string().c_str());
        event->fire(packet);
        
        if (!mEndOfSource) fillPacket();
    }
    
    void onDisableTrack(const UInt32 index) {
        mTrackMask.clear(index);
        onTrackSelect(mTrackMask.value());
    }
};

sp<IMediaSession> CreateMediaFile(const sp<Looper>& lp) {
    return new MediaFile(lp);
}

__END_NAMESPACE_MFWK
