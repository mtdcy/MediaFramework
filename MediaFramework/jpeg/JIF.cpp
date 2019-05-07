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


// File:    JIF.h
// Author:  mtdcy.chen
// Changes:
//          1. 20160701     initial version
//

#define LOG_TAG "JIF"
#define LOG_NDEBUG 0
#include "JIF.h"


__BEGIN_NAMESPACE_MPX
__BEGIN_NAMESPACE(JPEG)

sp<FrameHeader> readFrameHeader(const BitReader& br, size_t length) {
    sp<FrameHeader> header = new FrameHeader;
    
    header->bpp     = br.r8();
    header->height  = br.rb16();
    header->width   = br.rb16();
    header->planes  = br.r8();
    for (size_t i = 0; i < header->planes; ++i) {
        header->plane[i].id = br.r8();
        header->plane[i].ss = br.r8();
        header->plane[i].qt = br.r8();
    }
    
    DEBUG("SOF: bpp %" PRIu8 ", height %" PRIu16 ", width %" PRIu16 ", %" PRIu8 " planes",
          header->bpp, header->height, header->width, header->planes);
    for (size_t i = 0; i < header->planes; ++i) {
        DEBUG("\t plane %" PRIu8 ", hss %d, vss %d, qt %" PRIu8,
              header->plane[i].id,
              (header->plane[i].ss & 0xf0) >> 4,
              (header->plane[i].ss & 0xf),
              header->plane[i].qt);
    }
    return header;
}

sp<ScanHeader> readScanHeader(const BitReader& br, size_t length) {
    sp<ScanHeader> header = new ScanHeader;
    
    header->components  = br.r8();
    for (size_t i = 0; i < header->components; ++i) {
        header->component[i].id = br.r8();
        header->component[i].tb = br.r8();
    }
    
    DEBUG("SOS: %d components", header->components);
    for (size_t i = 0; i < header->components; ++i) {
        DEBUG("\t component %u, AC %u, DC %u",
              header->component[i].id,
              (header->component[i].tb & 0xf0) >> 4,
              header->component[i].tb & 0xf);
    }
    return header;
}

sp<HuffmanTable> readHuffmanTable(const BitReader& br, size_t length) {
    sp<HuffmanTable> huff = new HuffmanTable;
    huff->type  = br.read(4);
    huff->id    = br.read(4);
    huff->table = br.readB(length - 1);
    
    DEBUG("DHT: %s, id %u, table length %zu",
          huff->type ? "AC" : "DC", huff->id, huff->table->size());
    
    return huff;
}

sp<QuantizationTable> readQuantizationTable(const BitReader& br, size_t length) {
    sp<QuantizationTable> quan = new QuantizationTable;
    quan->precision = br.read(4);
    quan->id        = br.read(4);
    quan->table     = br.readB(length - 1);
    
    DEBUG("DHT: precision %u bit, id %u, table length %zu",
          quan->precision ? 16u : 8u, quan->id, quan->table->size());
    return quan;
}

sp<RestartInterval> readRestartInterval(const BitReader& br, size_t length) {
    sp<RestartInterval> ri = new RestartInterval;
    ri->interval    = br.rb16();
    
    DEBUG("DRI: interval %u", ri->interval);
    return ri;
}

sp<JIFObject> readJIF(const BitReader& br, size_t length) {
    const size_t start = br.offset() / 8;
    eMarker marker = (eMarker)br.r16();
    if (marker != SOI) {
        ERROR("missing JIF SOI segment, unexpected marker %s", JPEG::MarkerName(SOI));
        return NIL;
    }
    
    sp<JIFObject> JIF = new JIFObject;
    while ((marker = (eMarker)br.r16()) != EOI) {
        if ((marker & 0xff00) != 0xff00) {
            ERROR("JIF bad marker %#x", marker);
            break;
        }
        
        size_t size = br.r16();
        DEBUG("JIF %s: length %zu", MarkerName(marker), size);
        
        size -= 2;
        if (marker == SOF0) {
            JIF->mFrameHeader = readFrameHeader(br, size);
        } else if (marker == DHT) {
            JIF->mHuffmanTables.push(readHuffmanTable(br, size));
        } else if (marker == DQT) {
            JIF->mQuantizationTables.push(readQuantizationTable(br, size));
        } else if (marker == DRI) {
            JIF->mRestartInterval = readRestartInterval(br, size);
        } else if (marker == SOS) {
            JIF->mScanHeader = readScanHeader(br, size);
            JIF->mData = br.readB(length - 2 - (br.offset() / 8 - start));
            break;
        } else {
            INFO("ignore marker %#x", marker);
            br.skipBytes(size);
        }
    }
    
    if (JIF->mData == NIL) {
        ERROR("missing SOS");
        return NIL;
    }
    
    marker = (eMarker)br.r16();
    if (marker != EOI) {
        ERROR("JIF missing EOI, image may be broken");
    }
    return JIF;
}

__END_NAMESPACE(JPEG)
__END_NAMESPACE_MPX

