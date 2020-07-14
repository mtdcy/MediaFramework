/******************************************************************************
 * Copyright (c) 2020, Chen Fang <mtdcy.chen@gmail.com>
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

#define LOG_TAG "JPEG"
#define LOG_NDEBUG 0
#include "JPEG.h"

// JPEG, image/jpeg, ISO 10918-1,
#include <stdio.h>

__BEGIN_NAMESPACE_MFWK
__BEGIN_NAMESPACE(JPEG)

sp<FrameHeader> readFrameHeader(const sp<ABuffer>& buffer, UInt32 length) {
    sp<FrameHeader> header = new FrameHeader;
    
    header->bpp     = buffer->r8();
    header->height  = buffer->rb16();
    header->width   = buffer->rb16();
    header->planes  = buffer->r8();
    for (UInt32 i = 0; i < header->planes; ++i) {
        header->plane[i].id = buffer->r8();
        header->plane[i].ss = buffer->r8();
        header->plane[i].qt = buffer->r8();
    }
    return header;
}

void printFrameHeader(const sp<FrameHeader>& header) {
    INFO("\tSOF: bpp %" PRIu8 ", height %" PRIu16 ", width %" PRIu16 ", %" PRIu8 " planes",
         header->bpp, header->height, header->width, header->planes);
    for (UInt32 i = 0; i < header->planes; ++i) {
        INFO("\t\t plane %" PRIu8 ", hss %d, vss %d, qt %" PRIu8,
             header->plane[i].id,
             (header->plane[i].ss & 0xf0) >> 4,
             (header->plane[i].ss & 0xf),
             header->plane[i].qt);
    }
}

sp<ScanHeader> readScanHeader(const sp<ABuffer>& buffer, UInt32 length) {
    sp<ScanHeader> header = new ScanHeader;
    
    header->components  = buffer->r8();
    for (UInt32 i = 0; i < header->components; ++i) {
        header->component[i].id = buffer->r8();
        header->component[i].tb = buffer->r8();
    }
    return header;
}

void printScanHeader(const sp<ScanHeader>& header) {
    INFO("\tSOS: %d components", header->components);
    for (UInt32 i = 0; i < header->components; ++i) {
        INFO("\t\t component %u, AC %u, DC %u",
             header->component[i].id,
             (header->component[i].tb & 0xf0) >> 4,
             header->component[i].tb & 0xf);
    }
}

sp<HuffmanTable> readHuffmanTable(const sp<ABuffer>& buffer, UInt32 length) {
    sp<HuffmanTable> huff = new HuffmanTable;
    huff->type  = buffer->read(4);
    huff->id    = buffer->read(4);
    huff->table = buffer->readBytes(length - 1);
    return huff;
}

void printHuffmanTable(const sp<HuffmanTable>& huff) {
    INFO("\tDHT: %s, id %u, table length %zu",
         huff->type ? "AC" : "DC", huff->id, huff->table->size());
}

sp<QuantizationTable> readQuantizationTable(const sp<ABuffer>& buffer, UInt32 length) {
    sp<QuantizationTable> quan = new QuantizationTable;
    quan->precision = buffer->read(4);
    quan->id        = buffer->read(4);
    quan->table     = buffer->readBytes(length - 1);
    return quan;
}

void printQuantizationTable(const sp<QuantizationTable>& quan) {
    INFO("\tDQT: precision %u bit, id %u, table length %zu",
         quan->precision ? 16u : 8u, quan->id, quan->table->size());
}

sp<RestartInterval> readRestartInterval(const sp<ABuffer>& buffer, UInt32 length) {
    sp<RestartInterval> ri = new RestartInterval;
    ri->interval    = buffer->rb16();
    return ri;
}

void printRestartInterval(const sp<RestartInterval>& ri) {
    INFO("\tDRI: interval %u", ri->interval);
}

sp<JIFObject> readJIF(const sp<Buffer>& buffer, UInt32 length) {
    const UInt32 start = buffer->offset();
    eMarker marker = (eMarker)buffer->r16();
    if (marker != SOI) {
        ERROR("missing JIF SOI segment, unexpected marker %s", JPEG::MarkerName(SOI));
        return Nil;
    }
    
    sp<JIFObject> JIF = new JIFObject(False);
    while ((marker = (eMarker)buffer->r16()) != EOI) {
        if ((marker & 0xff00) != 0xff00) {
            ERROR("JIF bad marker %#x", marker);
            break;
        }
        
        UInt32 size = buffer->r16();
        DEBUG("JIF %s: length %zu", MarkerName(marker), size);
        
        size -= 2;
        if (marker == SOF0) {
            JIF->mFrameHeader = readFrameHeader(buffer, size);
        } else if (marker == DHT) {
            JIF->mHuffmanTables.push(readHuffmanTable(buffer, size));
        } else if (marker == DQT) {
            JIF->mQuantizationTables.push(readQuantizationTable(buffer, size));
        } else if (marker == DRI) {
            JIF->mRestartInterval = readRestartInterval(buffer, size);
        } else if (marker == SOS) {
            JIF->mScanHeader = readScanHeader(buffer, size);
            JIF->mData = buffer->readBytes(length - 2 - (buffer->offset() - start));
            break;
        } else {
            INFO("ignore marker %#x", marker);
            buffer->skipBytes(size);
        }
    }
    
    if (JIF->mData == Nil) {
        ERROR("missing SOS");
        return Nil;
    }
    
    marker = (eMarker)buffer->r16();
    if (marker != EOI) {
        ERROR("JIF missing EOI, image may be broken");
    }
    return JIF;
}

// read only SOF & store [SOI, EOI] to data
sp<JIFObject> readJIFLazy(const sp<ABuffer>& buffer, UInt32 length) {
    const UInt32 start = buffer->offset();
    eMarker marker = (eMarker)buffer->r16();
    if (marker != SOI) {
        ERROR("missing JIF SOI segment, unexpected marker %s", JPEG::MarkerName(SOI));
        return Nil;
    }
    
    sp<JIFObject> JIF = new JIFObject(True);
    while ((marker = (eMarker)buffer->r16()) != EOI) {
        if ((marker & 0xff00) != 0xff00) {
            ERROR("JIF bad marker %#x", marker);
            break;
        }
        
        UInt32 size = buffer->r16();
        DEBUG("JIF %s: length %zu", MarkerName(marker), size);
        
        size -= 2;
        if (marker == SOF0) {
            JIF->mFrameHeader = readFrameHeader(buffer, size);
            break;
        } else {
            //INFO("ignore marker %#x", marker);
            buffer->skipBytes(size);
        }
    }
    
    if (JIF->mFrameHeader.isNil()) {
        ERROR("JIF: missing SOF0");
        return Nil;
    }
    
    buffer->resetBytes();
    buffer->skipBytes(start);
    JIF->mData = buffer->readBytes(length);
    
    return JIF;
}

void printJIFObject(const sp<JIFObject>& jif) {
    if (jif->mFrameHeader != Nil) {
        printFrameHeader(jif->mFrameHeader);
    }
    
    if (jif->mHuffmanTables.size()) {
        List<sp<HuffmanTable> >::const_iterator it = jif->mHuffmanTables.cbegin();
        for (; it != jif->mHuffmanTables.cend(); ++it) {
            printHuffmanTable(*it);
        }
    }
    
    if (jif->mQuantizationTables.size()) {
        List<sp<QuantizationTable> >::const_iterator it = jif->mQuantizationTables.cbegin();
        for (; it != jif->mQuantizationTables.cend(); ++it) {
            printQuantizationTable(*it);
        }
    }
    
    if (jif->mRestartInterval != Nil) {
        printRestartInterval(jif->mRestartInterval);
    }
    
    if (jif->mScanHeader != Nil) {
        printScanHeader(jif->mScanHeader);
    }
    
    if (jif->mData != Nil) {
        INFO("JIF: image length %zu", jif->mData->size());
    }
}

/**
 * decode JIFObject using libjpeg-turbo
 */
sp<MediaFrame> decodeJIFObject(const sp<JIFObject>& jif) {
    CHECK_EQ(jif->mHeadOnly, True);     // only support head only now
    
    ImageFormat format;
    format.format   = kPixelFormatRGB;
    format.width    = jif->mFrameHeader->width;
    format.height   = jif->mFrameHeader->height;
    sp<MediaFrame> frame = MediaFrame::Create(format);
    return frame;
}

__END_NAMESPACE(JPEG)

__END_NAMESPACE_MFWK
