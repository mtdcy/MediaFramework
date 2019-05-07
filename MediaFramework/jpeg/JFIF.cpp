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


// File:    JPEG.cpp
// Author:  mtdcy.chen
// Changes:
//          1. 20160701     initial version
//

#define LOG_TAG "JFIF"
#define LOG_NDEBUG 0
#include "JFIF.h"
#include "JIF.h"
#include "Exif.h"

__BEGIN_NAMESPACE_MPX
__BEGIN_NAMESPACE(JFIF)

sp<AppHeader> readAppHeader(const BitReader& br, size_t length) {
    String id = br.readS(5);
    DEBUG("APP0 identifier %s", id.c_str());
    if (id != "JFIF") {
        ERROR("bad identifier %s", id.c_str());
        return NIL;
    }
    
    sp<AppHeader> segment = new AppHeader;
    segment->version    = br.rb16();
    segment->units      = br.r8();
    segment->x          = br.rb16();
    segment->y          = br.rb16();
    
    if (br.numBitsLeft() > 2 * 8) {
        segment->width0     = br.r8();
        segment->height0    = br.r8();
        segment->thunmbnail = br.readB(length - 5 - 7);
    } else {
        segment->width0     = 0;
        segment->height0    = 0;
    }
    
    DEBUG("APP0: version %#x, units: %" PRIu8 ", [%" PRIu16 " x %" PRIu16 "]",
          segment->version, segment->units, segment->x, segment->y);
    if (!segment->thunmbnail.isNIL()) {
        DEBUG("\tthumbnail: [%" PRIu8 " x %" PRIu8 "], length %zu",
              segment->width0, segment->height0, segment->thunmbnail->size());
    }
    return segment;
}

__END_NAMESPACE(JFIF)


#define RB16(pipe)          (BitReader(*(pipe->read(2))).rb16())
#define readMarker(pipe)    ((JPEG::eMarker)RB16(pipe))
#define readLength(pipe)    ((size_t)RB16(pipe))
sp<JFIFObject> openJFIF(sp<Content>& pipe) {
    sp<JFIFObject> JFIF = new JFIFObject;
    
    JPEG::eMarker SOI = readMarker(pipe);
    if (SOI != JPEG::SOI) {
        ERROR("missing JFIF SOI segment, unexpected marker %s", JPEG::MarkerName(SOI));
        return NIL;
    }
    
    JPEG::eMarker marker;
    while ((marker = readMarker(pipe)) != JPEG::EOI) {
        if ((marker & 0xff00) != 0xff00) {
            ERROR("JFIF bad marker %#x", marker);
            break;
        }
        
        size_t length = readLength(pipe);
        DEBUG("JFIF %s: length %zu", JPEG::MarkerName(marker), length);
        
        length -= 2;
        sp<Buffer> data = pipe->read(length);
        BitReader br (data->data(), data->size());
        
        if (marker == JPEG::APP0) {
            JFIF->mAppHeader = JFIF::readAppHeader(br, length);
        } else if (marker == JPEG::APP1) {
            JFIF->mAttributeInformation = EXIF::readAttributeInformation(br, length);
        } else if (marker == JPEG::SOF0) {
            JFIF->mFrameHeader = JPEG::readFrameHeader(br, length);
        } else if (marker == JPEG::DHT) {
            JFIF->mHuffmanTables.push(JPEG::readHuffmanTable(br, length));
        } else if (marker == JPEG::DQT) {
            JFIF->mQuantizationTables.push(JPEG::readQuantizationTable(br, length));
        } else if (marker == JPEG::DRI) {
            JFIF->mRestartInterval = JPEG::readRestartInterval(br, length);
        } else if (marker == JPEG::SOS) {
            JFIF->mScanHeader = JPEG::readScanHeader(br, length);
            // followed by image data
            break;
        } else {
            INFO("ignore marker %#x", marker);
        }
    }
    
    if (marker != JPEG::EOI) {
        ERROR("JFIF missing EOI");
    }
    
    DEBUG("pos: %" PRId64 "/%" PRId64, pipe->tell(), pipe->size());
    return JFIF;
}


__END_NAMESPACE_MPX
