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

#ifndef _MEDIA_JPEG_H
#define _MEDIA_JPEG_H

#include "MediaDefs.h"

__BEGIN_NAMESPACE_MPX
__BEGIN_NAMESPACE(JPEG)

/**
 * ISO/IEC 10918-1
 * ITU-T Rec. T.81
 * Annex B: JPEG Interchange Format (JIF)
 */

/**
 * https://en.wikipedia.org/wiki/JPEG#Syntax_and_structure
 */
enum eMarker {
    SOI     = 0xFFD8,   // Start Of Image. payload: none
    SOF0    = 0xFFC0,   // Baseline DCT. payload: variable
    SOF1    = 0xFFC1,   // Extended sequential DCT, Huffman. payload: variable
    SOF2    = 0xFFC2,   // Progressive DCT, Huffman. payload: variable
    SOF3    = 0xFFC3,   // Lossless (sequential), Huffman. payload: variable
    SOF9    = 0xFFC9,   // Extended sequential DCT, arithmetic coding. payload: variable
    SOF10   = 0xFFCA,   // Progressive DCT, arithmetic coding. payload: variable
    SOF11   = 0xFFCB,   // Lossless (sequential), arithmetic coding. payload: variable
    DHT     = 0xFFC4,   // Huffman tables. payload: variable
    DQT     = 0xFFDB,   // Quantization tables. payload: variable
    DRI     = 0xFFDD,   // interval between RSTn markers, in Minimum Coded Units (MCUs). payload: 4bytes
    SOS     = 0xFFDA,   // Start Of Scan. payload: variable
    RST0    = 0xFFD0,   // multi value: [0xFFD0, 0xFFD7]. payload: none
    APP0    = 0xFFE0,   // Application specific. payload: variable
    APP1    = 0XFFE1,   // Exif APP1 & ...
    APP2    = 0XFFE2,   // Exif Flashpix Extension & ...
    COM     = 0xFFFE,   // a text comment. payload: variable
    EOI     = 0xFFD9,   // End Of Image. payload: none
};

static const char * MarkerName(eMarker marker) {
    switch (marker) {
        case SOI:       return "SOI";
        case SOF0:      return "SOF0";
        case SOF1:      return "SOF1";
        case SOF2:      return "SOF2";
        case SOF3:      return "SOF3";
        case SOF9:      return "SOF9";
        case SOF10:     return "SOF10";
        case SOF11:     return "SOF11";
        case DHT:       return "DHT";
        case DQT:       return "DQT";
        case DRI:       return "DRI";
        case SOS:       return "SOS";
        case RST0:      return "RST0";
        case APP0:      return "APP0";
        case APP1:      return "APP1";
        case APP2:      return "APP2";
        case COM:       return "COM";
        case EOI:       return "EOI";
        default:        return "???";
    }
}

enum eFrameType {
    Baseline,
};

struct Segment : public SharedObject {
    Segment(eMarker x) : marker(x) { }
    eMarker     marker;
};

/**
 * Annex B.2.2
 * Frame Header with marker SOFn
 *
 */
struct FrameHeader : public Segment {
    FrameHeader() : Segment(SOF0) { }
    
    // SOF0 : uint16_t
    // length : uint16_t, exclude markder
    uint8_t     bpp;    ///< bits per sample, @note not average bpp
    uint16_t    height; ///< image height
    uint16_t    width;  ///< image width
    uint8_t     planes; ///< number planes/components
    struct {
        uint8_t id;     ///< plane/component id
        uint8_t ss;     ///< sub sampling factor [vss|hss], 4 bits for each
        uint8_t qt;     ///< quantization table number
    } plane[4];
};

struct HuffmanTable : public Segment {
    HuffmanTable() : Segment(DHT) { }
    
    // huffman table information,
    // bit 0..3: type, bit 4..7: id
    uint8_t     type;
    uint8_t     id;
    
    sp<Buffer>  table;
};

struct QuantizationTable : public Segment {
    QuantizationTable() : Segment(DQT) { }
    
    // quantization table information,
    // bit 0..3: precision, bit 4..7: id
    uint8_t     precision;      ///< 0 - 8 bit, 1 - 16 bit
    uint8_t     id;
    sp<Buffer>  table;
};

struct RestartInterval : public Segment {
    RestartInterval() : Segment(DRI) { }
    
    uint16_t    interval;   ///<
};

struct ScanHeader : public Segment {
    ScanHeader() : Segment(SOS) { }
    
    // SOS : uint16_t
    // length : uint16_t
    uint8_t     components;     // number components in scan
    struct {
        uint8_t id;     // component id
        uint8_t tb;     // Huffman table, [AC|DC], 4 bits each
    } component[4];
};

/**
 * BitReader without marker
 */
sp<FrameHeader> readFrameHeader(const BitReader&, size_t);
sp<ScanHeader> readScanHeader(const BitReader&, size_t);
sp<HuffmanTable> readHuffmanTable(const BitReader&, size_t);
sp<QuantizationTable> readQuantizationTable(const BitReader&, size_t);
sp<RestartInterval> readRestartInterval(const BitReader&, size_t);

struct JIFObject : public SharedObject {
    List<sp<HuffmanTable> >         mHuffmanTables;         // DHT
    List<sp<QuantizationTable> >    mQuantizationTables;    // DQT
    sp<FrameHeader>                 mFrameHeader;           // SOF
    sp<ScanHeader>                  mScanHeader;            // SOS
    sp<RestartInterval>             mRestartInterval;       // DRI
    sp<Buffer>                      mData;                  // Compressed Data
};

/**
 * BitReader with full JIF
 */
sp<JIFObject> readJIF(const BitReader&, size_t);

__END_NAMESPACE(JPEG)

__END_NAMESPACE_MPX

#endif // _MEDIA_JPEG_H
