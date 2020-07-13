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


// File:    Exif.cpp
// Author:  mtdcy.chen
// Changes:
//          1. 20160701     initial version
//

#define LOG_TAG "TIFF"
#define LOG_NDEBUG 0
#include "TIFF.h"

__BEGIN_NAMESPACE_MPX
__BEGIN_NAMESPACE(TIFF)

const Char * TagName(eTag tag) {
    switch (tag) {
        case kNewSubfileType:   return "NewSubfileType";
        case kSubfileType:      return "SubfileType";
        case kImageWidth:       return "ImageWidth";
        case kImageHeight:      return "ImageHeight";
        case kBitsPerSample:    return "BitsPerSample";
        case kCompression:      return "Compression";
        case kPhotometricInterpretation: return "PhotometricInterp";
        case kThreshholding:    return "Threshholding";
        case kDocumentName:     return "DocumentName";
        case kImageDescription: return "ImageDescription";
        case kManufacturer:     return "Manufacturer";
        case kModel:            return "Model";
        case kStripOffsets:     return "StripOffsets";
        case kOrientation:      return "Orientation";
        case kSamplesPerPixel:  return "SamplesPerPixel";
        case kRowsPerStrip:     return "RowsPerStrip";
        case kStripByteCounts:  return "StripByteCounts";
        case kXResolution:      return "XResolution";
        case kYResolution:      return "YResolution";
        case kPlanarConfiguration:  return "PlanarConfig";
        case kPageName:         return "PageName";
        case kXPosition:        return "XPosition";
        case kYPosition:        return "YPosition";
        case kGrayResponseUnit: return "GrayResponseUnit";
        case kGrayResponseCurve:return "GrayResponseCurve";
        case kGroup3Options:    return "Group3Options";
        case kGroup4Options:    return "Group4Options";
        case kResolutionUnit:   return "ResolutionUnit";
        case kPageNumber:       return "PageNumber";
        case kColorResponseCurves:  return "ColorResponseCurves";
        case kSoftware:         return "Software";
        case kDateTime:         return "DateTime";
        case kArtist:           return "Artist";
        case kHostComputer:     return "HostComputer";
        case kPredictor:        return "Predictor";
        case kWhitePoint:       return "WhitePoint";
        case kPrimaryChromaticities:    return "PrimaryChromaticities";
        case kColorMap:         return "ColorMap";
            
        case kJPEGInterchangeFormat:    return "JPEGInterchangeFormat";
        case kJPEGInterchangeFormatLength: return "JPEGInterchangeFormatLength";
        case kYCbCrCoefficients:    return "YCbCrCoefficients";
        case kYCbCrSubSampling:     return "YCbCrSubSampling";
        case kYCbCrPositioning:     return "YCbCrPositioning";
        case kReferenceBlackWhite:  return "ReferenceBlackWhite";
            
        case kCopyright:            return "Copyright";
            
        // ...
        default:                return Nil;
    }
}

const Char * TypeName(eType type) {
    switch (type) {
        case kBYTE:         return "BYTE";
        case kASCII:        return "ASCII";
        case kSHORT:        return "SHORT";
        case kLONG:         return "LONG";
        case kRATIONAL:     return "RATIONAL";
        case kUNDEFINED:    return "UNDEFINED";
        case kSLONG:        return "SLONG";
        case kSRATIONAL:    return "SRATIONAL";
        case kFLOAT:        return "FLOAT";
        case kDOUBLE:       return "DOUBLE";
        default:            FATAL("FIXME"); return Nil;
    }
}

UInt32 TypeLength(eType type) {
    switch (type) {
        case kUNDEFINED:
        case kSBYTE:
        case kBYTE:
        case kASCII:        return 1;
        case kSSHORT:
        case kSHORT:        return 2;
        case kSLONG:
        case kLONG:         return 4;
        case kRATIONAL:
        case kSRATIONAL:    return 2 * 4;
        case kFLOAT:        return 4;
        case kDOUBLE:       return 8;
        default:            FATAL("FIXME"); return 0;
    }
}

Entry * allocateEntry(eTag tag, eType type, UInt32 count) {
    const UInt32 bytes = sizeof(Entry) + sizeof(EntryData) * count;
    void * p = kAllocatorDefault->allocate(bytes);
    memset(p, 0, bytes);
    Entry * e = static_cast<Entry *>(p);
    e->tag      = tag;
    e->type     = type;
    e->count    = count;
    return e;
}

static void freeEntry(Entry * e) {
    kAllocatorDefault->deallocate(e);
}

void fillEntry(Entry * e, const sp<ABuffer>& buffer) {
    for (UInt32 i = 0; i < e->count; ++i) {
        switch (e->type) {
            case kSHORT:        e->value[i].u16 = buffer->r16(); break;
            case kSSHORT:       e->value[i].s16 = buffer->r16(); break;
            case kLONG:         e->value[i].u32 = buffer->r32(); break;
            case kSLONG:        e->value[i].s32 = buffer->r32(); break;
            case kRATIONAL:     e->value[i].rat.num = buffer->r32(); e->value[i].rat.den = buffer->r32(); break;
            case kSRATIONAL:    e->value[i].srat.num = buffer->r32(); e->value[i].srat.den = buffer->r32(); break;
            case kUNDEFINED:
            case kBYTE:
            case kASCII:        e->value[i].u8 = buffer->r8(); break;
            case kSBYTE:        e->value[i].s8 = buffer->r8(); break;
            default:            FATAL("FIXME..."); break;
        }
    }
}

ImageFileDirectory::~ImageFileDirectory() {
    List<Entry *>::iterator it = mEntries.begin();
    while (it != mEntries.end()) {
        Entry * e = *it;
        it = mEntries.erase(it);
        freeEntry(e);
    }
}

sp<ImageFileDirectory> readImageFileDirectory(const sp<ABuffer>& buffer, UInt32 * next) {
    sp<ImageFileDirectory> IFD = new ImageFileDirectory;
    
    UInt16 fields = buffer->r16();     // number of fields
    DEBUG("%u fields", fields);
    
    for (UInt32 i = 0; i < fields; ++i) {
        const eTag tag      = (eTag)buffer->r16();
        const eType type    = (eType)buffer->r16();
        const UInt32 length = buffer->r32();
        
        Entry * e   = allocateEntry(tag, type, length);
        
        const UInt32 bytes  = length * TypeLength(type);
        if (bytes > 4) {
            e->offset   = buffer->r32();
            //DEBUG("  tag %#x, type %u, length %u, offset %u", e->tag, e->type, e->count, e->offset);
        } else {
            // read data directly
            fillEntry(e, buffer);
            if (bytes < 4) buffer->skipBytes(4 - bytes);   // skip reset
            //DEBUG("  tag %#x, type %u, length %u, data %u", e->tag, e->type, e->count, e->value[0].u32);
        }
        
        IFD->mEntries.push(e);
    }
    
    // next IFD offset
    if (next) *next = buffer->r32();
    else buffer->skipBytes(4);
    
    return IFD;
}

static String printEntry(const Entry * e, const Char * name) {
    String line;
    if (name) {
        line = String::format("entry %s: ", name);
    } else {
        line = String::format("entry %#x: ", e->tag);
    }
    line.append(String::format("type %s, %zu units [", TypeName(e->type), e->count));
    for (UInt32 i = 0; i < e->count; ++i) {
        if (i != 0) line.append(", ");
        switch (e->type) {
            case kUNDEFINED:
            case kBYTE:         line.append(String::format("%" PRIu8,  e->value[i].u8));  break;
            case kSBYTE:        line.append(String::format("%" PRId8,  e->value[i].s8));  break;
            case kASCII:        line.append(String::format("%c",       e->value[i].u8));  break;
            case kSHORT:        line.append(String::format("%" PRIu16, e->value[i].u16)); break;
            case kSSHORT:       line.append(String::format("%" PRId16, e->value[i].s16)); break;
            case kLONG:         line.append(String::format("%" PRIu32, e->value[i].u32)); break;
            case kSLONG:        line.append(String::format("%" PRId32, e->value[i].s32)); break;
            case kRATIONAL:     line.append(String::format("%" PRIu32 "/%" PRIu32, e->value[i].rat.num, e->value[i].rat.den));   break;
            case kSRATIONAL:    line.append(String::format("%" PRId32 "/%" PRId32, e->value[i].srat.num, e->value[i].srat.den)); break;
            default:            FATAL("FIXME"); break;
        }
    }
    line.append("]");
    return line;
}

String printEntry(const Entry * e) {
    return printEntry(e, TagName(e->tag));
}

void printImageFileDirectory(const sp<ImageFileDirectory>& IFD) {
    List<Entry *>::const_iterator it = IFD->mEntries.cbegin();
    for (; it != IFD->mEntries.cend(); ++it) {
        INFO("\t%s", printEntry(*it).c_str());
    }
}

void printImageFileDirectory(const sp<ImageFileDirectory>& IFD, const Char * (*GetTagName)(eTag)) {
    List<Entry *>::const_iterator it = IFD->mEntries.cbegin();
    for (; it != IFD->mEntries.cend(); ++it) {
        INFO("\t%s", printEntry(*it, GetTagName((*it)->tag)).c_str());
    }
}

__END_NAMESPACE(TIFF)
__END_NAMESPACE_MPX
