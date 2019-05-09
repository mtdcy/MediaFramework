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

#define LOG_TAG "JPEG"
#define LOG_NDEBUG 0
#include "JPEG.h"

// JPEG, image/jpeg, ISO 10918-1,
#include <stdio.h>
#define JPEG_INTERNALS
#include "jpeglib.h"

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
    return header;
}

void printFrameHeader(const sp<FrameHeader>& header) {
    INFO("\tSOF: bpp %" PRIu8 ", height %" PRIu16 ", width %" PRIu16 ", %" PRIu8 " planes",
         header->bpp, header->height, header->width, header->planes);
    for (size_t i = 0; i < header->planes; ++i) {
        INFO("\t\t plane %" PRIu8 ", hss %d, vss %d, qt %" PRIu8,
             header->plane[i].id,
             (header->plane[i].ss & 0xf0) >> 4,
             (header->plane[i].ss & 0xf),
             header->plane[i].qt);
    }
}

sp<ScanHeader> readScanHeader(const BitReader& br, size_t length) {
    sp<ScanHeader> header = new ScanHeader;
    
    header->components  = br.r8();
    for (size_t i = 0; i < header->components; ++i) {
        header->component[i].id = br.r8();
        header->component[i].tb = br.r8();
    }
    return header;
}

void printScanHeader(const sp<ScanHeader>& header) {
    INFO("\tSOS: %d components", header->components);
    for (size_t i = 0; i < header->components; ++i) {
        INFO("\t\t component %u, AC %u, DC %u",
             header->component[i].id,
             (header->component[i].tb & 0xf0) >> 4,
             header->component[i].tb & 0xf);
    }
}

sp<HuffmanTable> readHuffmanTable(const BitReader& br, size_t length) {
    sp<HuffmanTable> huff = new HuffmanTable;
    huff->type  = br.read(4);
    huff->id    = br.read(4);
    huff->table = br.readB(length - 1);
    return huff;
}

void printHuffmanTable(const sp<HuffmanTable>& huff) {
    INFO("\tDHT: %s, id %u, table length %zu",
         huff->type ? "AC" : "DC", huff->id, huff->table->size());
}

sp<QuantizationTable> readQuantizationTable(const BitReader& br, size_t length) {
    sp<QuantizationTable> quan = new QuantizationTable;
    quan->precision = br.read(4);
    quan->id        = br.read(4);
    quan->table     = br.readB(length - 1);
    return quan;
}

void printQuantizationTable(const sp<QuantizationTable>& quan) {
    INFO("\tDQT: precision %u bit, id %u, table length %zu",
         quan->precision ? 16u : 8u, quan->id, quan->table->size());
}

sp<RestartInterval> readRestartInterval(const BitReader& br, size_t length) {
    sp<RestartInterval> ri = new RestartInterval;
    ri->interval    = br.rb16();
    return ri;
}

void printRestartInterval(const sp<RestartInterval>& ri) {
    INFO("\tDRI: interval %u", ri->interval);
}

sp<JIFObject> readJIF(const BitReader& br, size_t length) {
    const size_t start = br.offset() / 8;
    eMarker marker = (eMarker)br.r16();
    if (marker != SOI) {
        ERROR("missing JIF SOI segment, unexpected marker %s", JPEG::MarkerName(SOI));
        return NIL;
    }
    
    sp<JIFObject> JIF = new JIFObject(false);
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

// read only SOF & store [SOI, EOI] to data
sp<JIFObject> readJIFLazy(const BitReader& br, size_t length) {
    const size_t start = br.offset() / 8;
    eMarker marker = (eMarker)br.r16();
    if (marker != SOI) {
        ERROR("missing JIF SOI segment, unexpected marker %s", JPEG::MarkerName(SOI));
        return NIL;
    }
    
    sp<JIFObject> JIF = new JIFObject(true);
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
            break;
        } else {
            //INFO("ignore marker %#x", marker);
            br.skipBytes(size);
        }
    }
    
    if (JIF->mFrameHeader.isNIL()) {
        ERROR("JIF: missing SOF0");
        return NIL;
    }
    
    br.seekBytes(start);
    JIF->mData = br.readB(length);
    
    return JIF;
}

void printJIFObject(const sp<JIFObject>& jif) {
    if (jif->mFrameHeader != NIL) {
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
    
    if (jif->mRestartInterval != NIL) {
        printRestartInterval(jif->mRestartInterval);
    }
    
    if (jif->mScanHeader != NIL) {
        printScanHeader(jif->mScanHeader);
    }
    
    if (jif->mData != NIL) {
        INFO("JIF: image length %zu", jif->mData->size());
    }
}

/**
 * decode JIFObject using libjpeg-turbo
 */
sp<MediaFrame> decodeJIFObject(const sp<JIFObject>& jif) {
    CHECK_EQ(jif->mHeadOnly, true);     // only support head only now
    
    ImageFormat format;
    format.format   = kPixelFormatRGB;
    format.width    = jif->mFrameHeader->width;
    format.height   = jif->mFrameHeader->height;
    sp<MediaFrame> frame = MediaFrame::Create(format);
    
#if 1
    jpeg_decompress_struct jpeg;
    jpeg_error_mgr jerr;
    jpeg.err = jpeg_std_error(&jerr);
    jpeg_create_decompress(&jpeg);
    
    jpeg_mem_src(&jpeg, (const unsigned char *)jif->mData->data(), jif->mData->size());
    
    jpeg_read_header(&jpeg, TRUE);
    
    jpeg_start_decompress(&jpeg);
    
    jpeg_destroy_decompress(&jpeg);
#endif
    return frame;
}

__END_NAMESPACE(JPEG)

__END_NAMESPACE_MPX
