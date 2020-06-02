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
__USING_NAMESPACE_MPX

int main(int argc, char **argv) {
    CHECK_GT(argc, 1);
    const String url = argv[1];
    
    // how many packet to dump
    size_t count = 3;
    if (argc > 2) count = String(argv[2]).toInt32();
    
    sp<Content> pipe = Content::Create(url);
    
    sp<MediaFile> file = MediaFile::Create(pipe);
    
    if (file.isNIL()) {
        ERROR("create MediaFile for %s failed", url.c_str());
        return 1;
    }
    
    sp<Message> formats = file->formats();
    
    size_t numTracks = formats->findInt32(kKeyCount);
    for (size_t i = 0; i < numTracks; ++i) {
        
    }
    
    for (size_t i = 0; i < count; ++i) {
        sp<MediaPacket> packet = file->read();
        if (packet.isNIL()) {
            INFO("eos or error, exit.");
            return 1;
        }
        
        String pathname = String::format("%zu_%07" PRId64 "_%07" PRId64 "@%zu",
                                         packet->index,
                                         packet->dts.useconds(),
                                         packet->pts.useconds(),
                                         packet->size);
        
        sp<Content> out = Content::Create(pathname, Content::Write);
        CHECK_EQ(out->write((const char *)packet->data, packet->size), packet->size);
    }

    return 0;
}
