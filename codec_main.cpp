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


// File:    mpx.cpp
// Author:  mtdcy.chen
// Changes: 
//          1. 20181126     initial version
//

#define LOG_TAG "file.main"
#include <MediaFramework/MediaFramework.h>
USING_NAMESPACE_MFWK

int main(int argc, char **argv) {
    CHECK_GT(argc, 1);
    const String url = argv[1];
    
    // how many packet to dump
    UInt32 count = 3;
    if (argc > 2) count = String(argv[2]).toInt32();
    
    INFO("%s %zu", url.c_str(), count);
    
    sp<ABuffer> content = Content::Create(url);
    sp<Message> format = new Message;
    format->setObject(kKeyContent, content);
    sp<MediaDevice> file = MediaDevice::create(format, NULL);
    
    if (file.isNil()) {
        ERROR("create MediaFile for %s failed", url.c_str());
        return 1;
    }
    
    sp<Message> formats = file->formats();
    
    UInt32 numTracks = formats->findInt32(kKeyCount);
    
    Vector<sp<ABuffer> > contents;
    Vector<sp<MediaDevice> > codecs;
    for (UInt32 i = 0; i < numTracks; ++i) {
        sp<Message> trackFormat = formats->findObject(kKeyTrack + i);
        INFO("track %u: %s", i, trackFormat->string().c_str());
        
        sp<Message> options = new Message;
        options->setInt32(kKeyMode, kModeTypeSoftware);
        sp<MediaDevice> codec = MediaDevice::create(trackFormat, options);
        
        if (codec.isNil()) {
            ERROR("create codec failed for %s", trackFormat->string().c_str());
            return 1;
        }
        
        codecs.push(codec);
        
        contents.push(Content::Create(String::format("track-%zu.raw", i), Protocol::Write));
    }
    
    for (size_t i = 0; i < count; ++i) {
        sp<MediaFrame> packet = file->pull();
        if (packet.isNil()) {
            INFO("eos or error, exit.");
            return 1;
        }
        
        if (codecs[packet->id]->push(packet) != kMediaNoError) {
            ERROR("track-%u write failed", packet->id);
            return 1;
        }
        
        sp<MediaFrame> frame = codecs[packet->id]->pull();
        if (frame.isNil()) continue;
        
        for (size_t i = 0; i < frame->planes.count; ++i) {
            contents[packet->id]->writeBytes((const char *)frame->planes.buffers[i].data,
                                             frame->planes.buffers[i].size);
        }
    }

    return 0;
}
