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


// File:    EBML.cpp
// Author:  mtdcy.chen
// Changes:
//          1. 20160701     initial version
//

#define LOG_TAG   "EBML"
//#define LOG_NDEBUG 0
#include "MediaTypes.h"

#include "EBML.h"

//#define VERBOSE
#ifdef VERBOSE
#define DEBUGV(fmt, ...) DEBUG(fmt, ##__VA_ARGS__)
#else
#define DEBUGV(fmt, ...) do {} while(0)
#endif

// https://github.com/cellar-wg/ebml-specification/blob/master/specification.markdown#data-size-values
#define UNKNOWN_DATA_SIZE   0x7f

// reference:
// https://matroska.org/technical/specs/index.html
// https://matroska.org/files/matroska.pdf

__BEGIN_NAMESPACE_MPX
__BEGIN_NAMESPACE(EBML)

static FORCE_INLINE bool EBMLIsValidID(uint64_t x) {
    if (x >= 0x81 && x <= 0xFE) return true;
    if (x >= 0x407F && x <= 0x7FFE) return true;
    if (x >= 0x203FFF && x <= 0x3FFFFE) return true;
    if (x >= 0x101FFFFF && x <= 0x1FFFFFFE) return true;
    return false;
}

static FORCE_INLINE size_t EBMLCodeBytesLength(uint64_t x) {
    size_t n = 0;
    while (x) { ++n; x >>= 8; }
    return n;
}

static FORCE_INLINE uint64_t EBMLCodeInteger(uint64_t x) {
    size_t n = EBMLCodeBytesLength(x);
    x |= (1 << (7 * n));
    return x;
}

EBMLInteger::EBMLInteger(uint64_t x) : u64(x), length(EBMLCodeBytesLength(x)) { }

static EBMLInteger EBMLIntegerNull(0);     // with value & length == 0

static FORCE_INLINE bool operator==(EBMLInteger& lhs, EBMLInteger& rhs) {
    return lhs.u64 == rhs.u64 && lhs.length == rhs.length;
}

#define MASK(n) ((1ULL<<(n))-1)
static FORCE_INLINE uint8_t EBMLGetBytesLength(uint8_t v) {
    return __builtin_clz((unsigned int)v) - 24 + 1;
}

// vint with leading 1-bit
static FORCE_INLINE EBMLInteger EBMLGetCodedInteger(BitReader& br) {
    if (br.remianBytes() == 0) return EBMLIntegerNull;
    
    EBMLInteger vint;
    vint.u8     = br.r8();
    vint.length = EBMLGetBytesLength(vint.u8);
    CHECK_GT(vint.length, 0);
    CHECK_LE(vint.length, 8);
    if (br.remianBytes() < vint.length) {
        return EBMLIntegerNull;
    }

    if (vint.length > 1) {
        for (size_t i = 1; i < vint.length; ++i) {
            vint.u64 = (vint.u64 << 8) | br.r8();
        }
    }
    return vint;
}

static FORCE_INLINE uint64_t EBMLClearLeadingBit(uint64_t x, size_t length) {
    return x & MASK(7 * length);
}

// vint without leading 1-bit removed
static FORCE_INLINE EBMLInteger EBMLGetInteger(BitReader& br) {
    if (br.remianBytes() == 0) return EBMLIntegerNull;
    EBMLInteger vint = EBMLGetCodedInteger(br);
    vint.u64 = EBMLClearLeadingBit(vint.u64, vint.length);
    return vint;
}

// vint with or without leading 1-bit
static FORCE_INLINE EBMLInteger EBMLGetInteger(BitReader& br, size_t n) {
    if (br.remianBytes() < n) return EBMLIntegerNull;
    
    CHECK_GT(n, 0);
    CHECK_LE(n, 8);
    EBMLInteger vint;
    vint.u8 = br.r8();
    vint.length = n;
    if (n > 1) {
        for (size_t i = 1; i < n; ++i) {
            vint.u64 = (vint.u64 << 8) | br.r8();
        }
    }
    return vint;
}

// vint -> element length
static FORCE_INLINE EBMLInteger EBMLGetLength(BitReader& br) {
    return EBMLGetInteger(br);
}

union Int2Float {
    float       flt;
    uint32_t    u32;
};

union Int2Double {
    double      dbl;
    uint64_t    u64;
};

static FORCE_INLINE double EBMLGetFloat(BitReader& br, size_t n) {
    if (n == 8) {
        CHECK_EQ(sizeof(double), 8);
        Int2Double a;
        a.u64 = br.rb64();
        return a.dbl;
    } else {
        CHECK_EQ(n, 4);
        Int2Float a;
        a.u32 = br.rb32();
        return a.flt;
    }
    return 0;
}

#define EBMLGetLength   EBMLGetInteger

uint64_t vsint_subtr[] = {
    0x3F, 0x1FFF, 0x0FFFFF, 0x07FFFFFF,
    0x03FFFFFFFF, 0x01FFFFFFFFFF,
    0x00FFFFFFFFFFFF, 0x007FFFFFFFFFFFFF
};

static FORCE_INLINE EBMLSignedInteger EBMLGetSignedInteger(BitReader& br) {
    EBMLSignedInteger svint;
    EBMLInteger vint    = EBMLGetInteger(br);
    svint.i64           = vint.u64 - vsint_subtr[vint.length - 1];
    svint.length        = vint.length;
    return svint;
}

static FORCE_INLINE EBMLSignedInteger EBMLGetSignedInteger(BitReader& br, size_t n) {
    EBMLSignedInteger svint;
    EBMLInteger vint    = EBMLGetInteger(br, n);
    svint.i64           = vint.u64 - vsint_subtr[vint.length - 1];
    svint.length        = vint.length;
    return svint;
}

MediaError EBMLIntegerElement::parse(BitReader& br, size_t size) {
    vint = EBMLGetInteger(br, size);
    DEBUGV("integer %#x", vint.u64);
    return kMediaNoError;
}

String EBMLIntegerElement::string() const {
    return String::format("%#x", vint.u64);

}

MediaError EBMLSignedIntegerElement::parse(BitReader& br, size_t size) {
    svint = EBMLGetSignedInteger(br, size);
    DEBUGV("integer %#x", svint.i64);
    return kMediaNoError;
}

String EBMLSignedIntegerElement::string() const {
    return String::format("%#x", svint.i64);

}

MediaError EBMLStringElement::parse(BitReader& br, size_t size) {
    str = br.readS(size);
    DEBUGV("string %s", str.c_str());
    return kMediaNoError;
}

String EBMLStringElement::string() const { return str; }

MediaError EBMLUTF8Element::parse(BitReader& br, size_t size) {
    utf8 = br.readS(size);
    DEBUGV("utf8 %s", utf8.c_str());
    return kMediaNoError;
}

String EBMLUTF8Element::string() const { return utf8; }

MediaError EBMLBinaryElement::parse(BitReader& br, size_t size) {
    data = br.readB(size);
    DEBUGV("binary %s", data->string(true).c_str());
    return kMediaNoError;
}

String EBMLBinaryElement::string() const {
    return String::format("%zu bytes binary", data->size());
}

MediaError EBMLFloatElement::parse(BitReader& br, size_t size) {
    flt = EBMLGetFloat(br, size);
    DEBUGV("float %f", flt);
    return kMediaNoError;
}

String EBMLFloatElement::string() const {
    return String(flt);
}

MediaError EBMLMasterElement::parse(BitReader& br, size_t size) {
    // NOTHING
    return kMediaNoError;
}

String EBMLMasterElement::string() const {
    return String::format("%zu children", children.size());
}

MediaError EBMLSkipElement::parse(BitReader& br, size_t size) {
    if (size) br.skipBytes(size);
    return kMediaNoError;
}

String EBMLSkipElement::string() const { return "*"; }

MediaError EBMLSimpleBlockElement::parse(BitReader& br, size_t size) {
    const size_t offset = br.offset();
    TrackNumber = EBMLGetInteger(br);
    TimeCode    = br.rb16();
    Flags       = br.r8();
    DEBUGV("[%zu] block size %zu, %#x", TrackNumber.u32, size, Flags);
    if (Flags & kBlockFlagLace) {
        size_t count = 1 + br.r8();     // frame count
        CHECK_GT(count, 1);
        size_t frame[count - 1];
        if ((Flags & kBlockFlagLace) == kBlockFlagEBML) {
            EBMLInteger vint = EBMLGetInteger(br);
            frame[0] = vint.u32;
            for (size_t i = 1; i < count - 1; ++i) {
                EBMLSignedInteger svint = EBMLGetSignedInteger(br);
                frame[i] = frame[i-1] + svint.i32;
                //DEBUG("frame %zu", frame[i]);
            }
        } else if ((Flags & kBlockFlagLace) == kBlockFlagXiph) {
            for (size_t i = 0; i < count - 1; ++i) {
                uint8_t u8 = br.r8();
                frame[i] = u8;
                while (u8 == 255) {
                    u8 = br.r8();
                    frame[i] += u8;
                }
            }
        } else if ((Flags & kBlockFlagLace) == kBlockFlagFixed) {
            const size_t total = size - (br.offset() - offset) / 8;
            CHECK_EQ(total % count, 0);
            const size_t length = total / count;
            for (size_t i = 0; i < count - 1; ++i) {
                frame[i] = length;
            }
        }

        for (size_t i = 0; i < count - 1; ++i) {
            //CHECK_GE(size, (br.offset() - offset) / 8 + frame[i]);
            data.push(br.readB(frame[i]));
        }
    }

    //CHECK_GT(size, (br.offset() - offset) / 8);
    data.push(br.readB(size - (br.offset() - offset) / 8)); // last frame length is not stored

    return kMediaNoError;
}

String EBMLSimpleBlockElement::string() const {
    return String::format("[%zu] % " PRId16 " Flags %#x, blocks %zu",
            (size_t)TrackNumber.u64, TimeCode, Flags, data.size());
}

#define ITEM(X, T)  { .NAME = #X, .ID = ID_##X, .TYPE = T   }
static const struct {
    const char *        NAME;
    uint64_t            ID;
    eEBMLElementType    TYPE;
} ELEMENTS[] = {
    // top level elements
    ITEM(   EBMLHEADER,                 kEBMLElementMaster      ),
    ITEM(   SEGMENT,                    kEBMLElementMaster      ),
    // top level EBML element's children
    ITEM(   EBMLVERSION,                kEBMLElementInteger     ),
    ITEM(   EBMLREADVERSION,            kEBMLElementInteger     ),
    ITEM(   EBMLMAXIDLENGTH,            kEBMLElementInteger     ),
    ITEM(   EBMLMAXSIZELENGTH,          kEBMLElementInteger     ),
    ITEM(   DOCTYPEVERSION,             kEBMLElementInteger     ),
    ITEM(   DOCTYPEREADVERSION,         kEBMLElementInteger     ),
    ITEM(   DOCTYPE,                    kEBMLElementString      ),
    // SEGMENT
    ITEM(   SEGMENTINFO,                kEBMLElementMaster      ),
    ITEM(   SEEKHEAD,                   kEBMLElementMaster      ),
    ITEM(   CLUSTER,                    kEBMLElementMaster      ),
    ITEM(   TRACKS,                     kEBMLElementMaster      ),
    ITEM(   CUES,                       kEBMLElementMaster      ),
    ITEM(   ATTACHMENTS,                kEBMLElementMaster      ),
    ITEM(   CHAPTERS,                   kEBMLElementMaster      ),
    ITEM(   TAGS,                       kEBMLElementMaster      ),
    // SEGMENTINFO
    ITEM(   SEGMENTUID,                 kEBMLElementBinary      ),
    ITEM(   SEGMENTFILENAME,            kEBMLElementUTF8        ),
    ITEM(   PREVUID,                    kEBMLElementBinary      ),
    ITEM(   PREVFILENAME,               kEBMLElementUTF8        ),
    ITEM(   NEXTUID,                    kEBMLElementBinary      ),
    ITEM(   NEXTFILENAME,               kEBMLElementUTF8        ),
    ITEM(   TIMECODESCALE,              kEBMLElementInteger     ),
    ITEM(   DURATION,                   kEBMLElementFloat       ),
    ITEM(   TITLE,                      kEBMLElementUTF8        ),
    ITEM(   MUXINGAPP,                  kEBMLElementString      ),
    ITEM(   WRITINGAPP,                 kEBMLElementUTF8        ),
    ITEM(   DATEUTC,                    kEBMLElementSignedInteger   ),
    // SEEKHEAD
    ITEM(   SEEK,                       kEBMLElementMaster      ),
    ITEM(   SEEKID,                     kEBMLElementInteger     ),
    ITEM(   SEEKPOSITION,               kEBMLElementInteger     ),
    // TRACKS
    ITEM(   TRACKENTRY,                 kEBMLElementMaster      ),
    ITEM(   TRACKNUMBER,                kEBMLElementInteger     ),
    ITEM(   TRACKUID,                   kEBMLElementInteger     ),
    ITEM(   TRACKTYPE,                  kEBMLElementInteger     ),
    ITEM(   FLAGENABLED,                kEBMLElementInteger     ),
    ITEM(   FLAGDEFAULT,                kEBMLElementInteger     ),
    ITEM(   FLAGFORCED,                 kEBMLElementInteger     ),
    ITEM(   FLAGLACING,                 kEBMLElementInteger     ),
    ITEM(   MINCACHE,                   kEBMLElementInteger     ),
    ITEM(   MAXCACHE,                   kEBMLElementInteger     ),
    ITEM(   DEFAULTDURATION,            kEBMLElementInteger     ),
    ITEM(   TRACKTIMECODESCALE,         kEBMLElementFloat       ),
    ITEM(   NAME,                       kEBMLElementUTF8        ),
    ITEM(   LANGUAGE,                   kEBMLElementString      ),
    ITEM(   CODECID,                    kEBMLElementString      ),
    ITEM(   CODECPRIVATE,               kEBMLElementBinary      ),
    ITEM(   CODECNAME,                  kEBMLElementUTF8        ),
    ITEM(   ATTACHMENTLINK,             kEBMLElementInteger     ),
    ITEM(   CONTENTENCODING,            kEBMLElementMaster      ),
    ITEM(   CONTENTENCODINGORDER,       kEBMLElementInteger     ),
    ITEM(   CONTENTENCODINGSCOPE,       kEBMLElementInteger     ),
    ITEM(   CONTENTENCODINGTYPE,        kEBMLElementInteger     ),
    ITEM(   CONTENTCOMPRESSION,         kEBMLElementMaster      ),
    ITEM(   CONTENTCOMPALGO,            kEBMLElementInteger     ),
    ITEM(   CONTENTCOMPSETTINGS,        kEBMLElementBinary      ),
    // VIDEO
    ITEM(   VIDEO,                      kEBMLElementMaster      ),
    ITEM(   PIXELWIDTH,                 kEBMLElementInteger     ),
    ITEM(   PIXELHEIGHT,                kEBMLElementInteger     ),
    ITEM(   PIXELCROPBOTTOM,            kEBMLElementInteger     ),
    ITEM(   PIXELCROPTOP,               kEBMLElementInteger     ),
    ITEM(   PIXELCROPLEFT,              kEBMLElementInteger     ),
    ITEM(   PIXELCROPRIGHT,             kEBMLElementInteger     ),
    ITEM(   DISPLAYWIDTH,               kEBMLElementInteger     ),
    ITEM(   DISPLAYHEIGHT,              kEBMLElementInteger     ),
    ITEM(   DISPLAYUNIT,                kEBMLElementInteger     ),
    ITEM(   FLAGINTERLACED,             kEBMLElementInteger     ),
    // AUDIO
    ITEM(   AUDIO,                      kEBMLElementMaster      ),
    ITEM(   SAMPLINGFREQUENCY,          kEBMLElementFloat       ),
    ITEM(   OUTPUTSAMPLINGFREQUENCY,    kEBMLElementFloat       ),
    ITEM(   CHANNELS,                   kEBMLElementInteger     ),
    ITEM(   BITDEPTH,                   kEBMLElementInteger     ),
    // CONTENTENCODINGS
    ITEM(   CONTENTENCODINGS,           kEBMLElementMaster      ),
    // SEEKHEAD
    ITEM(   SEEK,                       kEBMLElementMaster      ),
    ITEM(   SEEKID,                     kEBMLElementInteger     ),
    ITEM(   SEEKPOSITION,               kEBMLElementInteger     ),
    // CLUSTER
    ITEM(   CRC32,                      kEBMLElementInteger     ),
    ITEM(   TIMECODE,                   kEBMLElementInteger     ),
    ITEM(   POSITION,                   kEBMLElementInteger     ),
    ITEM(   PREVSIZE,                   kEBMLElementInteger     ),
    ITEM(   BLOCKGROUP,                 kEBMLElementMaster      ),
    ITEM(   SIMPLEBLOCK,                kEBMLElementBlock       ),  // kEBMLElementBinary
    // BLOCKGROUP
    ITEM(   BLOCK,                      kEBMLElementBlock       ),  // kEBMLElementBinary
    ITEM(   REFERENCEBLOCK,             kEBMLElementSignedInteger   ),
    ITEM(   REFERENCEPRIORITY,          kEBMLElementInteger     ),
    ITEM(   BLOCKDURATION,              kEBMLElementInteger     ),
    ITEM(   BLOCKVIRTUAL,               kEBMLElementBinary      ),
    ITEM(   BLOCKADDITIONS,             kEBMLElementMaster      ),
    ITEM(   CODECSTATE,                 kEBMLElementBinary      ),
    ITEM(   DISCARDPADDING,             kEBMLElementSignedInteger   ),
    // CUES
    ITEM(   CUEPOINT,                   kEBMLElementMaster      ),
    // CUEPOINT
    ITEM(   CUETIME,                    kEBMLElementInteger     ),
    ITEM(   CUETRACKPOSITIONS,          kEBMLElementMaster      ),
    // CUETRACKPOSITIONS
    ITEM(   CUETRACK,                   kEBMLElementInteger     ),
    ITEM(   CUECLUSTERPOSITION,         kEBMLElementInteger     ),
    ITEM(   CUERELATIVEPOSITION,        kEBMLElementInteger     ),
    ITEM(   CUEBLOCKNUMBER,             kEBMLElementInteger     ),
    ITEM(   CUECODECSTATE,              kEBMLElementInteger     ),
    ITEM(   CUEREFERENCE,               kEBMLElementMaster      ),
    // CUEREFERENCE
    ITEM(   CUEREFTIME,                 kEBMLElementInteger     ),
    ITEM(   CUEREFCLUSTER,              kEBMLElementInteger     ),
    ITEM(   CUEREFNUMBER,               kEBMLElementInteger     ),
    ITEM(   CUEREFCODECSTATE,           kEBMLElementInteger     ),
    // TAGS
    ITEM(   TAG,                        kEBMLElementMaster      ),
    ITEM(   TARGETS,                    kEBMLElementMaster      ),
    ITEM(   SIMPLETAG,                  kEBMLElementMaster      ),
    // TARGETS
    ITEM(   TARGETTYPEVALUE,            kEBMLElementInteger     ),
    ITEM(   TARGETTYPE,                 kEBMLElementUTF8        ),
    ITEM(   TARGETTRACKUID,             kEBMLElementInteger     ),
    ITEM(   TARGETEDITIONUID,           kEBMLElementInteger     ),
    ITEM(   TARGETCHAPTERUID,           kEBMLElementInteger     ),
    ITEM(   ATTACHMENTUID,              kEBMLElementInteger     ),
    // SIMPLETAG
    ITEM(   TAGNAME,                    kEBMLElementUTF8        ),
    ITEM(   TAGLANGUAGE,                kEBMLElementString      ),
    ITEM(   TAGORIGINAL,                kEBMLElementInteger     ),
    ITEM(   TAGSTRING,                  kEBMLElementString      ),
    ITEM(   TAGBINARY,                  kEBMLElementBinary      ),
    // CHAPTERS
    ITEM(   EDITIONENTRY,               kEBMLElementMaster      ),
    ITEM(   EDITIONUID,                 kEBMLElementInteger     ),
    ITEM(   EDITIONFLAGHIDDEN,          kEBMLElementInteger     ),
    ITEM(   EDITIONFLAGDEFAULT,         kEBMLElementInteger     ),
    ITEM(   EDITIONFLAGORDERED,         kEBMLElementInteger     ),
    ITEM(   CHAPTERATOM,                kEBMLElementMaster      ),
    // CHAPTERATOM
    ITEM(   CHAPTERUID,                 kEBMLElementInteger     ),
    ITEM(   CHAPTERTIMESTART,           kEBMLElementInteger     ),
    ITEM(   CHAPTERTIMEEND,             kEBMLElementInteger     ),
    ITEM(   CHAPTERFLAGHIDDEN,          kEBMLElementInteger     ),
    ITEM(   CHAPTERFLAGENABLED,         kEBMLElementInteger     ),
    ITEM(   CHAPTERSEGMENTUID,          kEBMLElementBinary      ),
    ITEM(   CHAPTERSEGMENTEDITIONUID,   kEBMLElementInteger     ),
    ITEM(   CHAPTERTRACKS,              kEBMLElementMaster      ),
    ITEM(   CHAPTERDISPLAY,             kEBMLElementMaster      ),
    // CHAPTERTRACKS
    ITEM(   CHAPTERTRACKNUMBER,         kEBMLElementInteger     ),
    // CHAPTERDISPLAY
    ITEM(   CHAPSTRING,                 kEBMLElementUTF8        ),
    ITEM(   CHAPLANGUAGE,               kEBMLElementString      ),
    ITEM(   CHAPCOUNTRY,                kEBMLElementUTF8        ),
    ITEM(   VOID,                       kEBMLElementSkip        ),
    ITEM(   ATTACHEDFILE,               kEBMLElementMaster      ),
    ITEM(   FILEDESCRIPTION,            kEBMLElementUTF8        ),
    ITEM(   FILENAME,                   kEBMLElementUTF8        ),
    ITEM(   FILEMIMETYPE,               kEBMLElementString      ),
    ITEM(   FILEDATA,                   kEBMLElementBinary      ),
    ITEM(   FILEUID,                    kEBMLElementInteger     ),
    
    // END OF LIST
};
#define NELEM(x)    sizeof(x)/sizeof(x[0])

sp<EBMLElement> MakeEBMLElement(const EBMLInteger& id, EBMLInteger& size) {
    for (size_t i = 0; i < NELEM(ELEMENTS); ++i) {
        if (ELEMENTS[i].ID == id.u64) {
            //DEBUG("make element %s[%#x]", ELEMENTS[i].NAME, ELEMENTS[i].ID);
            switch (ELEMENTS[i].TYPE) {
                case kEBMLElementInteger:
                    return new EBMLIntegerElement(ELEMENTS[i].NAME, id, size);
                case kEBMLElementSignedInteger:
                    return new EBMLSignedIntegerElement(ELEMENTS[i].NAME, id, size);
                case kEBMLElementString:
                    return new EBMLStringElement(ELEMENTS[i].NAME, id, size);
                case kEBMLElementUTF8:
                    return new EBMLUTF8Element(ELEMENTS[i].NAME, id, size);
                case kEBMLElementFloat:
                    return new EBMLFloatElement(ELEMENTS[i].NAME, id, size);
                case kEBMLElementMaster:
                    return new EBMLMasterElement(ELEMENTS[i].NAME, id, size);
                case kEBMLElementBinary:
                    return new EBMLBinaryElement(ELEMENTS[i].NAME, id, size);
                case kEBMLElementSkip:
                    return new EBMLSkipElement(ELEMENTS[i].NAME, id, size);
                case kEBMLElementBlock:
                    return new EBMLSimpleBlockElement(ELEMENTS[i].NAME, id, size);
                default:
                    FATAL("FIXME");
                    break;
            }
        }
    }
#if LOG_NDEBUG == 0
    FATAL("unknown element %#x", id.u64);
    return NULL;
#else
    ERROR("unknown element %#x", id.u64);
    return new EBMLSkipElement("UNKNOWN", id, size);
#endif
}

sp<EBMLElement> FindEBMLElement(const sp<EBMLMasterElement>& master, uint64_t id) {
    CHECK_EQ(master->type, kEBMLElementMaster);
    List<EBMLMasterElement::Entry>::const_iterator it = master->children.cbegin();
    for (; it != master->children.cend(); ++it) {
        const EBMLMasterElement::Entry& e = *it;
        if (e.element->id.u64 == id)
            return e.element;
    }
    return NULL;
}

sp<EBMLElement> FindEBMLElementInside(const sp<EBMLMasterElement>& master, uint64_t inside, uint64_t target) {
    sp<EBMLMasterElement> target1 = FindEBMLElement(master, inside);
    if (target1 == NULL) return NULL;
    CHECK_EQ(target1->type, kEBMLElementMaster);
    return FindEBMLElement(target1, target);
}

static FORCE_INLINE void PrintEBMLElement(const sp<EBMLElement>& e, const int64_t pos, size_t level) {
    String line;
    for (size_t i = 0; i < level; ++i) line.append(" ");
    line.append("+");
    line.append(e->name);
    line.append(String::format("\t @ 0x%" PRIx64 ", ", pos));
    line.append(e->string());
    INFO("%s", line.c_str());

    if (e->type == kEBMLElementMaster) {
        sp<EBMLMasterElement> master = e;
        List<EBMLMasterElement::Entry>::const_iterator it = master->children.cbegin();
        for (; it != master->children.cend(); ++it) {
            const EBMLMasterElement::Entry& e = *it;
            PrintEBMLElement(e.element, e.position, level + 1);
        }
    }
}

void PrintEBMLElements(const sp<EBMLElement>& e) {
    INFO("+%s\t@ 0x0 %s", e->name, e->string().c_str());

    if (e->type == kEBMLElementMaster) {
        sp<EBMLMasterElement> master = e;
        List<EBMLMasterElement::Entry>::const_iterator it = master->children.cbegin();
        for (; it != master->children.cend(); ++it) {
            const EBMLMasterElement::Entry& e = *it;
            PrintEBMLElement(e.element, e.position, 1);
        }
    }
}

struct Entry {
    sp<EBMLMasterElement> element;
    int64_t length;
    Entry(const sp<EBMLMasterElement>& e, int64_t n) : element(e), length(n) { }
};

// TODO: handle UNKNOWN_DATA_SIZE
// when unknown data size exists, we should read elements one by one
sp<EBMLElement> ReadEBMLElement(sp<Content>& pipe, uint32_t flags) {
    DEBUG("EnumRootElements %#x", flags);
    size_t offset = pipe->tell();
    size_t idLengthMax = 8;
    size_t sizeLengthMax = 8;
    
    sp<Buffer> buffer = new Buffer(16, Buffer::Ring);
    sp<Buffer> data = pipe->read(16);
    buffer->write(*data);
    BitReader br(buffer->data(), buffer->size());
    
    // create root element
    EBMLInteger id              = EBMLGetCodedInteger(br);
    EBMLInteger size            = EBMLGetLength(br);
    sp<EBMLMasterElement> root  = MakeEBMLElement(id, size);
    CHECK_FALSE(root.isNIL());
    buffer->skip(id.length + size.length);
    
    const size_t elementLength = id.length + size.length + size.u64;
    DEBUG("found root element %s @ %" PRIu64 " length = %zu[%zu]",
         root->name, offset, (size_t)size.u64, 
         (size_t)(id.length + size.length + size.u64));
    
    if (root->type != kEBMLElementMaster) {
        DEBUG("leaf element");
        if (buffer->size() < size.u64) {
            if (buffer->capacity() < size.u64) {
                CHECK_TRUE(buffer->resize(size.u64));
            }
            sp<Buffer> data = pipe->read(size.u64 - buffer->size());
            buffer->write(*data);
        }
        
        // ignore all flags
        BitReader br0 (buffer->data(), buffer->size());
        
        if (root->parse(br, size.u64) != kMediaNoError) {
            ERROR("[%s @ %" PRId64 "] parse failed", root->name, offset);
            return NULL;
        }
        
        buffer->skip(size.u64);
        if (buffer->size()) {
            // put content @ the right postion
            pipe->skip(-(int64_t)size.u64);
        }
        
        return root;
    }
    
    sp<EBMLMasterElement> master = root;
    int64_t masterLength = size.u64;
    List<Entry> parents;
    offset += id.length + size.length;
    const int64_t end = pipe->length();
    while (offset < end) {
        if (buffer->size() < idLengthMax + sizeLengthMax) {
            sp<Buffer> data = pipe->read(idLengthMax + sizeLengthMax - buffer->size());
            if (data.isNIL() && buffer->empty()) break;
            if (!data.isNIL()) buffer->write(*data);
        }
        
        BitReader br0(buffer->data(), buffer->size());
        id      = EBMLGetCodedInteger(br0);
        size    = EBMLGetLength(br0);

        if (id == EBMLIntegerNull || size == EBMLIntegerNull) {
            break;
        }
        
        buffer->skip(id.length + size.length);
        
        sp<EBMLElement> element = MakeEBMLElement(id, size);
        if (element.isNIL()) {
            ERROR("unknown element %#x, broken file?", id.u64);
            break;
        }
        
        const size_t elementLength = id.length + size.length + size.u64;
        DEBUG("found level %zu element %s @ %" PRIu64 " length = %zu[%zu]",
             parents.size(), element->name, offset, (size_t)size.u64, elementLength);
        
        CHECK_LE(elementLength, masterLength);
        masterLength -= elementLength;
        
        // handle master element
        if (element->type == kEBMLElementMaster) {
            // stop @ cluster
            if (id.u64 == ID_CLUSTER && (flags & kEnumStopCluster)) {
                // put content @ cluster begin position
                INFO("cluster @ 0x%" PRIx64, offset);
                pipe->seek(offset);
                break;
            }
            master->children.push(EBMLMasterElement::Entry(offset, element));
            
            if (id.u64 == ID_CLUSTER && (flags & kEnumSkipCluster)) {
                if (size.u64 > buffer->size()) {
                    pipe->skip(size.u64 - buffer->size());
                    buffer->reset();
                } else {
                    buffer->skip(size.u64);
                }
                offset += id.length + size.length + size.u64;
            } else {
                parents.push(Entry(master, masterLength));
                master = element;
                masterLength = size.u64;
                offset += id.length + size.length;
            }
            continue;
        }
        
        if ((flags & kEnumSkipBlocks) && (id.u64 == ID_SIMPLEBLOCK || id.u64 == ID_BLOCK)) {
            // skip element content
            if (size.u64 > buffer->size()) {
                pipe->skip(size.u64 - buffer->size());
                buffer->reset();
            } else {
                buffer->skip(size.u64);
            }
        } else {
            // prepare element data
            if (buffer->size() < size.u64) {
                if (buffer->capacity() < size.u64) {
                    CHECK_TRUE(buffer->resize(size.u64));
                }
                sp<Buffer> data = pipe->read(size.u64 - buffer->size());
                buffer->write(*data);
            }
            
            BitReader br1 (buffer->data(), buffer->size());
            if (element->parse(br1, size.u64) != kMediaNoError) {
                ERROR("[%s @ %" PRId64 "] parse failed", element->name, offset);
            }
            if (size.u64) buffer->skip(size.u64);
        }
        
        master->children.push(EBMLMasterElement::Entry(offset, element));
        offset += elementLength;
        
        // handle terminator box
        if (id.u64 == ID_VOID) {
            // void element will extend the master element length and
            // will terminate multi master elements except level root element
            while (size.u64 && parents.size()) {
                size.u64 -= masterLength;
                Entry e = parents.back();
                parents.pop_back();
                master = e.element;
                masterLength = e.length;
            }
        } else {
            while (masterLength == 0 && parents.size()) {
                Entry e = parents.back();
                parents.pop_back();
                master = e.element;
                masterLength = e.length;
            }
        }
        
        if (master == root && masterLength == 0) break;
    }
    
    // put pipe @ the right position
    if (buffer->size()) {
        pipe->seek(offset);
    }
    
    return root;
}

int IsMatroskaFile(const sp<Buffer>& data) {
    BitReader br(data->data(), data->size());
    
    // detect each element without parse its content
    // if failed, add parse()
    int score = 0;
    while (score < 100) {
        EBMLInteger id      = EBMLGetCodedInteger(br);
        EBMLInteger size    = EBMLGetLength(br);
        
        if (id == EBMLIntegerNull || size == EBMLIntegerNull) {
            break;
        }
        
        sp<EBMLElement> element = MakeEBMLElement(id, size);
        if (element.isNIL()) {
            score = 0;
            break;
        }
        
        DEBUG("found element %s", element->name);
        
        // EBML Header has 7 children, make sure the score < 100.
        // as SEGMENT must exists.
        score += 10;
        if (element->type == kEBMLElementMaster) {
            continue;
        }
        
        if (size.length == 1 && size.u64 == UNKNOWN_DATA_SIZE) {
            INFO("found element with unknown size");
        } else {
            if (br.remianBytes() < size.u64) break;
            br.skipBytes(size.u64);
        }
    }
    INFO("IsMatroskaFile return with score %d", score);
    return score;
}

__END_NAMESPACE(EBML)
__END_NAMESPACE_MPX
