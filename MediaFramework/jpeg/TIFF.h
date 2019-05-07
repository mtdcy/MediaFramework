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


// File:    TIFF.h
// Author:  mtdcy.chen
// Changes:
//          1. 20160701     initial version
//

#ifndef _MEDIA_JPEG_TIFF_H
#define _MEDIA_JPEG_TIFF_H

#include "JIF.h"

__BEGIN_NAMESPACE_MPX

__BEGIN_NAMESPACE(TIFF)

/**
 * TIFF:
 * http://www.fileformat.info/format/tiff/corion.htm
 * https://sno.phy.queensu.ca/~phil/exiftool/TagNames/EXIF.html
 */
typedef uint16_t eTag;

enum {
    kNewSubfileType         = 0xFE,     ///< long, N = 1, def:0
    kSubfileType            = 0xFF,     ///< short, N = 1

    kImageWidth             = 0x100,    ///< short or long, N = 1, no def, number of pixels per scanline
    kImageHeight            = 0x101,    ///< short or long, N = 1, no def, number of scanlines
    kBitsPerSample          = 0x102,    ///< short, N = SamplesPerPixel, def:1
    kCompression            = 0x103,    ///< short, N = 1, def:1
                                        ///< 1 - no compression, 2 - CCITT Group 3, 32773 - PackBits compression
    kPhotometricInterpretation  = 0x106,///< short, N = 1, no def
                                        ///< 0 - WhiteIsZero, 1 - BlackIsZero
    kThreshholding          = 0x107,    ///< short, N = 1, def:?
    kDocumentName           = 0x10D,    ///< ASCII
    kImageDescription       = 0x10E,    ///< ASCII
    kManufacturer           = 0x10F,    ///< ASCII
    
    kModel                  = 0x110,    ///< ASCII
    kStripOffsets           = 0x111,    ///< short or long, N = ?
    kOrientation            = 0x112,    ///< short, N = 1, def:1
    kSamplesPerPixel        = 0x115,    ///< short, N = 1, def:1
    kRowsPerStrip           = 0x116,    ///< short or long, N = 1
    kStripByteCounts        = 0x117,    ///< short or long, N = ?
    kXResolution            = 0x11A,    ///< rational, N = 1, number of pixels per ResolutionUnit in X
    kYResolution            = 0x11B,    ///< rational, N = 1, number of pixels per ResolutionUnit in Y
    kPlanarConfiguration    = 0x11C,    ///< short, N = 1, def:1
    kPageName               = 0x11D,    ///< ASCII
    kXPosition              = 0x11E,    ///< rational, the X offset of the left side of the image
    kYPosition              = 0x11F,    ///< rational, the Y offset of the top of the image
    
    kGrayResponseUnit       = 0x122,    ///< short, N = 1
    kGrayResponseCurve      = 0x123,    ///< short, N = 2**BitsPerSample
    kGroup3Options          = 0x124,    ///< long, N = 1, def:?
    kGroup4Options          = 0x125,    ///< long, N = 1, def:?
    kResolutionUnit         = 0x128,    ///< short, N = 1, def:2,
                                        ///< 1 - no absolute unit of measurement, 2 - inch, 3 - centimeter
    kPageNumber             = 0x129,    ///< short, N = 2, def:?
    kColorResponseCurves    = 0x12D,    ///< short, N = 3 * (2**BitsPerSample)
    
    kSoftware               = 0x131,    ///< ASCII
    kDateTime               = 0x132,    ///< ASCII
    kArtist                 = 0x13B,    ///< ASCII
    kHostComputer           = 0x13C,    ///< ASCII
    kPredictor              = 0x13D,    ///< short, N = 1, def:1
    kWhitePoint             = 0x13E,    ///< rational, N = 2, def:SMPTE white point, D65: x = 0.313, y = 0.329
    kPrimaryChromaticities  = 0x13F,    ///< rational, N = 6, def:SMPTE primary color chromaticities:
                                        ///<                        Red:    x = 0.635, y = 0.340
                                        ///<                        Green:  x = 0.305, y = 0.595
                                        ///<                        Blue:   x = 0.155, y = 0.070
    //kColorImageType         = 0x13E,    ///< short, N = 1, def:1
    //kColorList              = 0x13F,    ///< byte or short, N = ?
    
    kColorMap               = 0x140,    ///< short, N = 3 * (2**BitsPerSample)
    
    kJPEGInterchangeFormat  = 0x201,    ///< long, N = 1,
    kJPEGInterchangeFormatLength = 0x202, ///< long, N = 1,
    
    kYCbCrCoefficients      = 0x211,    ///< rational, N = 3
    kYCbCrSubSampling       = 0x212,    ///< short, N = 2, [2,1] - YCbCr422, [2,2] - YCbCr420
    kYCbCrPositioning       = 0x213,    ///< short, N = 1,
    kReferenceBlackWhite    = 0x214,    ///< rational, N = 6
    
    kCopyright              = 0x8298,   ///< ascii, N = ?
    
};

const char * TagName(eTag);

enum eType {
    kBYTE       = 1,    ///< uint8_t
    kASCII      = 2,    ///< null-terminated ascii string
    kSHORT      = 3,    ///< uint16_t
    kLONG       = 4,    ///< uint32_t
    kRATIONAL   = 5,    ///< uint32_t numerator & denominator
    kSBYTE      = 6,    ///< int8_t
    kUNDEFINED  = 7,    ///< bytes
    kSSHORT     = 8,    ///< int16_t
    kSLONG      = 9,    ///< int32_t
    kSRATIONAL  = 10,   ///< int32_t numerator & denominator
    kFLOAT      = 11,   ///< float (4-bytes)
    kDOUBLE     = 12,   ///< double (8-bytes)
};

const char *    TypeName(eType);
size_t          TypeLength(eType);

struct Rational {
    uint32_t    num;
    uint32_t    den;
};

struct SRational {
    int32_t     num;
    int32_t     den;
};

union EntryData {
    uint8_t     u8;
    uint16_t    u16;
    uint32_t    u32;
    int8_t      s8;
    int16_t     s16;
    int32_t     s32;
    Rational    rat;
    SRational   srat;
    float       flt;
    double      dbl;
};

struct Entry {
    eTag        tag;        ///< @see eTag
    eType       type;       ///< unit type, @see eType
    uint32_t    count;      ///< number of units
    uint32_t    offset;     ///< as the data may be anywhere in the file, so record offset instead read data
                            ///< if data < 4-bytes, data is within offset space
    EntryData   value[0];
};

Entry * allocateEntry(eTag, eType, size_t);
String  printEntry(const Entry *);

struct ImageFileDirectory : public SharedObject {
    ImageFileDirectory() { }
    ~ImageFileDirectory();
    
    List<Entry *>   mEntries;
};

// 8-byte TIFF image file header
struct Header : public SharedObject {
    char     byteOrder[2];  // "II" or "MM"
    uint16_t version;       // 0x2a
    uint32_t offset;        // offset of the first IFD. usally 8
};

struct TIFFObject : public SharedObject {
    sp<Header>                      header;
    List<sp<ImageFileDirectory> >   IFDs;
};

void fillEntry(Entry * e, const BitReader&);

sp<ImageFileDirectory> readImageFileDirectory(const BitReader&, size_t * next = NULL);

void printImageFileDirectory(const sp<ImageFileDirectory>&);

void printImageFileDirectory(const sp<ImageFileDirectory>&, const char * (*GetTagName)(eTag));

sp<TIFFObject> openTIFF(const sp<Content>&);

__END_NAMESPACE(TIFF)
__END_NAMESPACE_MPX
#endif // _MEDIA_JPEG_TIFF_H
