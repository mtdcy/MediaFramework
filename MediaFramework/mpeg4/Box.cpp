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


// File:    Mp4Box.cpp
// Author:  mtdcy.chen
// Changes: 
//          1. 20160701     initial version
//

#define LOG_TAG   "MPEG4.Box"
//#define LOG_NDEBUG 0
#include "MediaTypes.h"
#include "Box.h"

//#define VERBOSE
#ifdef VERBOSE
#define DEBUGV(fmt, ...) DEBUG(fmt, ##__VA_ARGS__)
#else
#define DEBUGV(fmt, ...) do {} while(0)
#endif

__BEGIN_NAMESPACE_MPX
__BEGIN_NAMESPACE(MPEG4)

static FORCE_INLINE const char * BOXNAME(uint32_t x) {
    static char tmp[5];
    tmp[0]  = (x >> 24) & 0xff;
    tmp[1]  = (x >> 16) & 0xff;
    tmp[2]  = (x >> 8) & 0xff;
    tmp[3]  = x & 0xff;
    tmp[4]  = '\0';
    return &tmp[0];
}

const char * BoxName(uint32_t x) {
    return BOXNAME(x);
}

FileTypeBox::FileTypeBox(const BitReader& br, size_t size) {
    CHECK_GE(size, 12);
    major_brand     = br.rb32();
    minor_version   = br.rb32();
    INFO("major: %s, minor: 0x%" PRIx32, BOXNAME(major_brand), minor_version);

    for (size_t i = 8; i < size; i += 4) {
        uint32_t brand = br.rb32();
        INFO("compatible brand: %s", BOXNAME(brand));
        compatibles.push(brand);
    }
}

MediaError Box::parse(const BitReader& br, size_t sz, const FileTypeBox& ftyp) {
    if (full) {
        version     = br.r8();
        flags       = br.rb24();
    } else {
        version     = 0;
        flags       = 0;
    }
    DEBUG("box %s #%zu version = %" PRIu32 " flags = %" PRIx32,
            BOXNAME(type), sz, version, flags);
    return kMediaNoError;
}

void Box::compose(BitWriter& bw, const FileTypeBox& ftyp) {
}

MediaError ContainerBox::parse(const BitReader& br, size_t sz, const FileTypeBox& ftyp) {
    DEBUGV("box %s %zu", BOXNAME(type), sz);
    Box::parse(br, sz, ftyp);
    return _parse(br, sz - Box::size(), ftyp);
}
MediaError ContainerBox::_parse(const BitReader& br, size_t sz, const FileTypeBox& ftyp) {
    // FIXME: count is not used
    size_t next = 0;
    if (counted) {
        uint32_t count = br.rb32();     next += 4;
        DEBUG("box %s: count = %" PRIu32, BOXNAME(type), count);
    }

    while (next + 8 <= sz) {
        // 8 bytes
        uint32_t boxSize    = br.rb32();
        uint32_t boxType    = br.rb32();
        DEBUG("box %s:  + %s %" PRIu32, BOXNAME(type),
                BOXNAME(type), boxSize);

        // mov terminator box
        if (boxType == kBoxTerminator && boxSize == 8) {
            DEBUGV("box %s:  + terminator box %s", 
                    BOXNAME(type), BOXNAME(boxType));
            next += 8;
            // XXX: terminator is not terminator
            break;
            //continue;
        }

        next += boxSize;
#if LOG_NDEBUG == 0
        CHECK_GE(boxSize, 8);
        CHECK_LE(next, sz);
#else
        if (next > sz) {
            ERROR("box %s:  + skip broken box %s", 
                    BOXNAME(type), BOXNAME(boxType));
            break;
        }
#endif

        boxSize     -= 8;
        // this exists in mov
        if (boxSize == 0) {
            DEBUG("box %s:  + skip empty box %s", 
                    BOXNAME(type), BOXNAME(boxType));
            continue;
        }

        const size_t offset = br.offset();
        sp<Box> box = MakeBoxByType(boxType);
        if (box == NULL) {
            ERROR("box %s:  + skip unknown box %s %" PRIu32, 
                    BOXNAME(type), BOXNAME(boxType), boxSize);
#if LOG_NDEBUG == 0
            sp<Buffer> boxData = br.readB(boxSize);
            DEBUG("%s", boxData->string().c_str());
#else
            br.skipBytes(boxSize);
#endif
        } else if (box->parse(br, boxSize, ftyp) == kMediaNoError) {
            child.push(box);
        }
        const size_t delta = br.offset() - offset;
#if LOG_NDEBUG == 0
        CHECK_EQ(delta, boxSize * 8);
#else
        CHECK_LE(delta, boxSize * 8);
        if (delta != boxSize * 8) {
            br.skip(boxSize * 8 - delta);
        }
#endif
    }

    // qtff.pdf, Section "User Data Atoms", Page 37
    // For historical reasons, the data list is optionally 
    // terminated by a 32-bit integer set to 0. If you are 
    // writing a program to read user data atoms, you should 
    // allow for the terminating 0. However, 
    // if you are writing a program to create user data atoms, 
    // you can safely leave out the trailing 0.
    if (next < sz) {
#if LOG_NDEBUG == 0
#if 0
        sp<Buffer> trailing = br.readB(sz - next);
        DEBUG("box %s:  + %zu trailing bytes.\n%s", 
                BOXNAME(type),
                sz - next,
                trailing->string().c_str());
#else
        CHECK_EQ(next + 4, sz);
        CHECK_EQ(br.rb32(), 0);
#endif
#else
        br.skip(sz - next);
#endif
    }

    DEBUGV("box %s: child.size() = %zu", BOXNAME(type), child.size());
    return kMediaNoError;
}
void ContainerBox::compose(BitWriter&, const FileTypeBox&) { }

typedef sp<Box> (*create_t)();
static HashTable<uint32_t, create_t> sRegister;
struct RegisterHelper {
    RegisterHelper(uint32_t TYPE, create_t callback) {
        sRegister.insert(TYPE, callback);
    }
};

sp<Box> MakeBoxByType(uint32_t type) {
    if (sRegister.find(type)) {
        return sRegister[type]();
    }
    ERROR("can NOT find register for %s ..............", BOXNAME(type));
    return NULL;
}

// isom and quicktime using different semantics for MetaBox
static const bool isQuickTime(const FileTypeBox& ftyp) {
    if (ftyp.major_brand == kBrandTypeQuickTime) {
        return true;
    }
    for (size_t i = 0; i < ftyp.compatibles.size(); ++i) {
        if (ftyp.compatibles[i] == kBrandTypeQuickTime)
            return true;
    }
    return false;
}

MediaError MetaBox::parse(const BitReader& br, size_t sz, const FileTypeBox& ftyp) {
    Box::parse(br, sz, ftyp);
    size_t next = Box::size();
    if (!isQuickTime(ftyp)) {
        version     = br.r8();
        flags       = br.rb24();
        next        += 4;
    }

    ContainerBox::_parse(br, sz - next, ftyp);
    return kMediaNoError;
}
void MetaBox::compose(BitWriter& bw, const FileTypeBox& ftyp) { }

MediaError MovieHeaderBox::parse(const BitReader& br, size_t sz, const FileTypeBox& ftyp) {
    DEBUGV("box %s %zu", BOXNAME(type), sz);
    Box::parse(br, sz, ftyp);
    CHECK_EQ(sz, 4 + ((version == 0) ? 96 : 108));

    if (version == 1) {
        creation_time       = br.rb64();
        modification_time   = br.rb64();
        timescale           = br.rb32();
        duration            = br.rb64();
    } else {
        creation_time       = br.rb32();
        modification_time   = br.rb32();
        timescale           = br.rb32();
        duration            = br.rb32();
    }

    DEBUGV("box %s: creation %" PRIu64 ", modification %" PRIu64 
            ", timescale %" PRIu32 ", duration %" PRIu64, 
            BOXNAME(type), creation_time, modification_time, timescale, duration);

    rate            = br.rb32();
    volume          = br.rb16();
    br.skip(16);        // reserved
    br.skip(32 * 2);    // reserved
    br.skip(32 * 9);    // matrix 
    br.skip(32 * 6);    // pre_defined
    next_track_ID   = br.rb32();

    DEBUGV("box %s: rate %" PRIu32 ", volume %" PRIu16 ", next %" PRIu32,
            BOXNAME(type), rate, volume, next_track_ID);
    return kMediaNoError;
}

void MovieHeaderBox::compose(BitWriter& bw, const FileTypeBox& ftyp) { }

MediaError TrackHeaderBox::parse(const BitReader& br, size_t sz, const FileTypeBox& ftyp) {
    Box::parse(br, sz, ftyp);
    if (version == 1) {
        creation_time       = br.rb64();
        modification_time   = br.rb64();
        track_ID            = br.rb32();
        br.skip(32);
        duration            = br.rb64();
    } else {
        creation_time       = br.rb32();
        modification_time   = br.rb32();
        track_ID            = br.rb32();
        br.skip(32);
        duration            = br.rb32();
    }

    DEBUGV("box %s: creation_time %" PRIu64 ", modification_time %" PRIu64
            ", track id %" PRIu32 ", duration %" PRIu64, 
            BOXNAME(type), creation_time, modification_time, track_ID, duration);

    br.skip(32 * 2);
    layer           = br.rb16();
    alternate_group = br.rb16();
    volume          = br.rb16();
    br.skip(16);
    br.skip(32 * 9);
    width           = br.rb32();
    height          = br.rb32();

    DEBUGV("box %s: layer %" PRIu16 ", alternate_group %" PRIu16 
            ", volume %" PRIu16 ", width %" PRIu32 ", height %" PRIu32, 
            BOXNAME(type), layer, alternate_group, volume, width, height);

    return kMediaNoError;
}

void TrackHeaderBox::compose(BitWriter& bw, const FileTypeBox& ftyp) { }

MediaError TrackReferenceTypeBox::parse(const BitReader& br, size_t sz, const FileTypeBox& ftyp) {
    Box::parse(br, sz, ftyp);
    CHECK_FALSE(sz % 4);
    size_t count = sz / 4;
    while (count--) {
        uint32_t id = br.rb32();
        DEBUGV("box %s: %" PRIu32, BOXNAME(type), id);
        track_IDs.push(id);
    }
    return kMediaNoError;
}
void TrackReferenceTypeBox::compose(BitWriter& bw, const FileTypeBox& ftyp) { }

// ISO-639-2/T language code
static inline String languageCode(const BitReader& br) {
    br.skip(1);
    char lang[3];
    // Each character is packed as the difference between its ASCII value and 0x60
    for (size_t i = 0; i < 3; i++) lang[i] = br.read(5) + 0x60;
    return String(&lang[0], 3);
}

MediaError MediaHeaderBox::parse(const BitReader& br, size_t sz, const FileTypeBox& ftyp) {
    Box::parse(br, sz, ftyp);
    if (version == 1) {
        creation_time           = br.rb64();
        modification_time       = br.rb64();
        timescale               = br.rb32();
        duration                = br.rb64();
    } else {
        creation_time           = br.rb32();
        modification_time       = br.rb32();
        timescale               = br.rb32();
        duration                = br.rb32();
    }

    DEBUGV("box %s: creation %" PRIu64 ", modifcation %" PRIu64 
            ", timescale %" PRIu32 ", duration %" PRIu64,
            BOXNAME(type), creation_time, modification_time, timescale, duration);

    language = languageCode(br);
    DEBUGV("box %s: lang %s", BOXNAME(type), language.c_str());

    br.skip(16);    // pre_defined
    return kMediaNoError;
}
void MediaHeaderBox::compose(BitWriter& bw, const FileTypeBox& ftyp) { }

MediaError HandlerBox::parse(const BitReader& br, size_t sz, const FileTypeBox& ftyp) {
    Box::parse(br, sz, ftyp);
    CHECK_GE(sz, 4 + 20 + 1);

    br.skip(32); // pre_defined
    handler_type    = br.rb32();
    br.skip(32 * 3); // reserved
    handler_name    = br.readS(sz - Box::size() - 20);

    DEBUGV("box %s: type %s name %s", BOXNAME(type),
            BOXNAME(handler_type), handler_name.c_str());
    return kMediaNoError;
}
void HandlerBox::compose(BitWriter& bw, const FileTypeBox& ftyp) { }

MediaError VideoMediaHeaderBox::parse(const BitReader& br, size_t sz, const FileTypeBox& ftyp) {
    Box::parse(br, sz, ftyp);
    graphicsmode        = br.rb16();
    DEBUGV("box %s: graphicsmode %" PRIu16, 
            BOXNAME(type), graphicsmode);
    for (size_t i = 0; i < 3; i++) {
        uint16_t color = br.rb16();
        DEBUGV("box %s: color %" PRIu16, BOXNAME(type), color);
        opcolor.push(color);
    }
    return kMediaNoError;
}
void VideoMediaHeaderBox::compose(BitWriter& bw, const FileTypeBox& ftyp) { }

MediaError SoundMediaHeaderBox::parse(const BitReader& br, size_t sz, const FileTypeBox& ftyp) {
    Box::parse(br, sz, ftyp);
    balance         = br.rb16();
    br.skip(16);    // reserved

    DEBUGV("box %s: balance %" PRIu16, BOXNAME(type), balance);
    return kMediaNoError;
}
void SoundMediaHeaderBox::compose(BitWriter& bw, const FileTypeBox& ftyp) { }

MediaError HintMediaHeaderBox::parse(const BitReader& br, size_t sz, const FileTypeBox& ftyp) {
    Box::parse(br, sz, ftyp);
    maxPDUsize          = br.rb16();
    avgPDUsize          = br.rb16();
    maxbitrate          = br.rb32();
    avgbitrate          = br.rb32();
    br.skip(32);

    DEBUGV("box %s: maxPDUsize %" PRIu16 ", avgPDUsize %" PRIu16 
            ", maxbitrate %" PRIu32 ", avgbitrate %" PRIu32,
            BOXNAME(type), maxPDUsize, avgPDUsize, maxbitrate, avgbitrate);
    return kMediaNoError;
}
void HintMediaHeaderBox::compose(BitWriter& bw, const FileTypeBox& ftyp) { }

MediaError DataEntryUrlBox::parse(const BitReader& br, size_t sz, const FileTypeBox& ftyp) {
    Box::parse(br, sz, ftyp);
    if (sz > 4) {
        location    = br.readS(sz - 4);
        DEBUGV("box %s: location %s", BOXNAME(type), location.c_str());
    }
    return kMediaNoError;
}
void DataEntryUrlBox::compose(BitWriter& bw, const FileTypeBox& ftyp) { }

MediaError DataEntryUrnBox::parse(const BitReader& br, size_t sz, const FileTypeBox& ftyp) {
    Box::parse(br, sz, ftyp);
    char c;
    while ((c = (char)br.r8()) != '\0') urntype.append(String(c));
    location        = br.readS(sz - 4 - urntype.size());

    DEBUGV("box %s: name %s location %s", 
            BOXNAME(type), urntype.c_str(), location.c_str());
    return kMediaNoError;
}
void DataEntryUrnBox::compose(BitWriter& bw, const FileTypeBox& ftyp) { }

MediaError TimeToSampleBox::parse(const BitReader& br, size_t sz, const FileTypeBox& ftyp) {
    Box::parse(br, sz, ftyp);
    uint32_t count  = br.rb32();
    for (uint32_t i = 0; i < count; i++) {
        Entry e = { br.rb32(), br.rb32() };
        DEBUGV("box %s: entry %" PRIu32 " %" PRIu32, 
                BOXNAME(type), e.sample_count, e.sample_delta);
        entries.push(e);
    }
    return kMediaNoError;
}

void TimeToSampleBox::compose(BitWriter& bw, const FileTypeBox& ftyp) { }

MediaError CompositionOffsetBox::parse(const BitReader& br, size_t sz, const FileTypeBox& ftyp) {
    Box::parse(br, sz, ftyp);
    uint32_t count          = br.rb32();
    for (uint32_t i = 0; i < count; i++) {
        // version 0, sample_offset is uint32_t
        // version 1, sample_offset is int32_t
        // some writer ignore this rule, always write in int32_t
        // and sample_offset will no be very big,
        // so it is ok to always read sample_offset as int32_t
        Entry e = { br.rb32(), (int32_t)br.rb32() };
        DEBUGV("box %s: entry %" PRIu32 " %" PRIu32,
                BOXNAME(type), e.sample_count, e.sample_offset);
        entries.push(e);
    }
    return kMediaNoError;
}
void CompositionOffsetBox::compose(BitWriter& bw, const FileTypeBox& ftyp) { }

MediaError CompositionToDecodeBox::parse(const BitReader& br, size_t sz, const FileTypeBox& ftyp) {
    FullBox::parse(br, sz, ftyp);
    if (version == 0) {
        compositionToDTSShift           = br.rb32();
        leastDecodeToDisplayDelta       = br.rb32();
        greatestDecodeToDisplayDelta    = br.rb32();
        compositionStartTime            = br.rb32();
        compositionEndTime              = br.rb32();
    } else {
        compositionToDTSShift           = br.rb64();
        leastDecodeToDisplayDelta       = br.rb64();
        greatestDecodeToDisplayDelta    = br.rb64();
        compositionStartTime            = br.rb64();
        compositionEndTime              = br.rb64();
    }
    return kMediaNoError;
}
void CompositionToDecodeBox::compose(BitWriter& bw, const FileTypeBox& ftyp) { }

MediaError SampleDependencyTypeBox::parse(const BitReader& br, size_t sz, const FileTypeBox& ftyp) {
    Box::parse(br, sz, ftyp);
    for (size_t i = 0; i < sz - Box::size(); ++i) {
        dependency.push(br.r8());
    }
    return kMediaNoError;
}
void SampleDependencyTypeBox::compose(BitWriter& bw, const FileTypeBox& ftyp) { }

// AudioSampleEntry is different for isom and mov
MediaError SampleEntry::parse(const BitReader& br, size_t sz, const FileTypeBox& ftyp) {
    const size_t offset = br.offset();
    Box::parse(br, sz, ftyp);
    _parse(br, sz - Box::size(), ftyp);
    const size_t n = (br.offset() - offset) / 8;
    ContainerBox::_parse(br, sz - n, ftyp);
    DEBUGV("box %s: child.size() = %zu", BOXNAME(type), child.size());
    return kMediaNoError;
}
MediaError SampleEntry::_parse(const BitReader& br, size_t sz, const FileTypeBox& ftyp) {
    // 8 bytes
    br.skip(8 * 6);
    data_reference_index    = br.rb16();

    if (media_type == kMediaTypeVideo) {
        // 70 bytes
        br.skip(16);
        br.skip(16);
        br.skip(32 * 3);
        visual.width            = br.rb16();
        visual.height           = br.rb16();
        visual.horizresolution  = br.rb32();
        visual.vertresolution   = br.rb32();
        br.skip(32);
        visual.frame_count      = br.rb16();
        compressorname          = br.readS(32);
        visual.depth            = br.rb16();
        br.skip(16);

        DEBUGV("box %s: width %" PRIu16 ", height %" PRIu16 
                ", horizresolution %" PRIu32 ", vertresolution %" PRIu32
                ", frame_count %" PRIu16 ", compressorname %s"
                ", depth %" PRIu16, 
                BOXNAME(type),
                visual.width, visual.height, 
                visual.horizresolution, visual.vertresolution,
                visual.frame_count, 
                compressorname.c_str(), 
                visual.depth);
    } else if (media_type == kMediaTypeSound) {
        // 20 bytes
        sound.version           = br.rb16();       // mov
        br.skip(16);           // revision level
        br.skip(32);           // vendor
        sound.channelcount      = br.rb16();
        sound.samplesize        = br.rb16();
        sound.compressionID     = br.rb16();       // mov
        sound.packetsize        = br.rb16();       // mov
        sound.samplerate        = br.rb32() >> 16; 
        DEBUGV("box %s: channelcount %" PRIu16 ", samplesize %" PRIu16
                ", samplerate %" PRIu32,
                BOXNAME(type),
                sound.channelcount, 
                sound.samplesize, 
                sound.samplerate);

        if (isQuickTime(ftyp)) {
            // qtff.pdf Section "Sound Sample Description (Version 1)"
            // Page 120
            // 16 bytes
            sound.mov           = true;
            if (sound.version == 1) {
                sound.samplesPerPacket  = br.rb32();
                sound.bytesPerPacket    = br.rb32();
                sound.bytesPerFrame     = br.rb32();
                sound.bytesPerSample    = br.rb32();

                DEBUGV("box %s: samplesPerPacket %" PRIu32
                        ", bytesPerPacket %" PRIu32
                        ", bytesPerFrame %" PRIu32
                        ", bytesPerSample %" PRIu32,
                        BOXNAME(type),
                        sound.samplesPerPacket,
                        sound.bytesPerPacket,
                        sound.bytesPerFrame,
                        sound.bytesPerSample);
            } else {
                CHECK_EQ(version, 0);
            }
        } else {
            sound.mov           = false;
        }
    } else if (media_type == kMediaTypeHint) {
        // NOTHING
    } else {
        // NOTHING
    }
    return kMediaNoError;
}
void SampleEntry::compose(BitWriter& bw, const FileTypeBox& ftyp) { }

MediaError RollRecoveryEntry::parse(const BitReader& br, size_t sz, const FileTypeBox& ftyp) {
    roll_distance   = br.rb16();
    return kMediaNoError;
}

MediaError SampleGroupDescriptionBox::parse(const BitReader& br, size_t sz, const FileTypeBox& ftyp) {
    FullBox::parse(br, sz, ftyp);
    grouping_type           = br.rb32();
    if (version == 1)
        default_length      = br.rb32();
    else if (version >= 2)
        default_sample_description_index = br.rb32();
    uint32_t entry_count    = br.rb32();
    INFO("grouping_type %4s, entry_count %" PRIu32,
            (const char *)&grouping_type, entry_count);
    for (uint32_t i = 0; i < entry_count; ++i) {
        if (version == 1 && default_length == 0) {
            uint32_t description_length = br.rb32();
            // TODO
        }
        sp<SampleGroupEntry> entry;
        switch (grouping_type) {
            case 'roll':
            case 'prol':
                entry = new RollRecoveryEntry(grouping_type);
                break;
            default:
                break;
        }
        if (entry.isNIL()) {
            ERROR("unknown entry %4s", (const char *)&grouping_type);
            break;
        }
        if (entry->parse(br, sz, ftyp) != kMediaNoError) {
            ERROR("entry parse failed");
            break;
        }
        entries.push(entry);
    }
    return kMediaNoError;
}
void SampleGroupDescriptionBox::compose(BitWriter& bw, const FileTypeBox& ftyp) { }

MediaError ALACAudioSampleEntry::parse(const BitReader& br, size_t sz, const FileTypeBox& ftyp) {
    const size_t offset = br.offset();
    Box::parse(br, sz, ftyp);
    SampleEntry::_parse(br, sz - Box::size(), ftyp);
    extra = br.readB(sz - (br.offset()- offset) / 8);
    DEBUG("box %s: %s", BOXNAME(type), extra->string(true).c_str());
    return kMediaNoError;
}
void ALACAudioSampleEntry::compose(BitWriter& bw, const FileTypeBox& ftyp) { }

MediaError SamplingRateBox::parse(const BitReader& br, size_t sz, const FileTypeBox& ftyp) {
    Box::parse(br, sz, ftyp);
    sampling_rate   = br.rb32();
    return kMediaNoError;
}
void SamplingRateBox::compose(BitWriter& bw, const FileTypeBox& ftyp) { }

MediaError CommonBox::parse(const BitReader& br, size_t sz, const FileTypeBox& ftyp) {
    Box::parse(br, sz, ftyp);
    data = br.readB(sz - Box::size());
    DEBUG("box %s: %s", BOXNAME(type), data->string(true).c_str());
    return kMediaNoError;
}
void CommonBox::compose(BitWriter& bw, const FileTypeBox& ftyp) { }

MediaError BitRateBox::parse(const BitReader& br, size_t sz, const FileTypeBox& ftyp) {
    Box::parse(br, sz, ftyp);
    bufferSizeDB        = br.rb32();
    maxBitrate          = br.rb32();
    avgBitrate          = br.rb32();
    return kMediaNoError;
}
void BitRateBox::compose(BitWriter& bw, const FileTypeBox& ftyp) { }

MediaError SampleSizeBox::parse(const BitReader& br, size_t sz, const FileTypeBox& ftyp) {
    Box::parse(br, sz, ftyp);
    if (type == kBoxTypeSTZ2) {
        br.skip(24);            //reserved
        uint8_t field_size      = br.r8();
        sample_size             = 0; // always 0
        size_t sample_count     = br.rb32();
        for (size_t i = 0; i < sample_count; i++) {
            entries.push(br.read(field_size));
        }
    } else {
        sample_size             = br.rb32();
        size_t sample_count     = br.rb32();
        if (sample_size == 0) {
            for (size_t i = 0; i < sample_count; i++) {
                entries.push(br.rb32());
            }
        } else {
            DEBUGV("box %s: fixed sample size %" PRIu32, 
                    BOXNAME(type), sample_size);
        }
    }

#if 1
    for (size_t i = 0; i < entries.size(); ++i) {
        DEBUGV("box %s: %" PRIu64, BOXNAME(type), entries[i]);
    }
#endif 
    return kMediaNoError;
}
void SampleSizeBox::compose(BitWriter& bw, const FileTypeBox& ftyp) { }

MediaError SampleToChunkBox::parse(const BitReader& br, size_t sz, const FileTypeBox& ftyp) {
    Box::parse(br, sz, ftyp);
    uint32_t count          = br.rb32();
    for (uint32_t i = 0; i < count; i++) {
        Entry e;
        e.first_chunk       = br.rb32();
        e.samples_per_chunk = br.rb32();
        e.sample_description_index  = br.rb32();

        DEBUGV("box %s: %" PRIu32 ", %" PRIu32 ", %" PRIu32, 
                BOXNAME(type), e.first_chunk, e.samples_per_chunk, e.sample_description_index);

        entries.push(e);
    }
    return kMediaNoError;
}
void SampleToChunkBox::compose(BitWriter& bw, const FileTypeBox& ftyp) { }

MediaError ChunkOffsetBox::parse(const BitReader& br, size_t sz, const FileTypeBox& ftyp) {
    Box::parse(br, sz, ftyp);
    uint32_t count          = br.rb32();
    if (type == kBoxTypeCO64) { // offset64
        for (uint32_t i = 0; i < count; ++i) {
            uint64_t e          = br.rb64();
            DEBUGV("box %s: %" PRIu64, BOXNAME(type), e);
            entries.push(e);
        }
    } else {
        for (uint32_t i = 0; i < count; ++i) {
            uint32_t e          = br.rb32();
            DEBUGV("box %s: %" PRIu32, BOXNAME(type), e);
            entries.push(e);
        }
    }
    return kMediaNoError;
}
void ChunkOffsetBox::compose(BitWriter& bw, const FileTypeBox& ftyp) { }

MediaError SyncSampleBox::parse(const BitReader& br, size_t sz, const FileTypeBox& ftyp) {
    Box::parse(br, sz, ftyp);
    uint32_t count      = br.rb32();
    for (uint32_t i = 0; i < count; i++) {
        uint32_t e      = br.rb32();
        DEBUGV("box %s: %" PRIu32, BOXNAME(type), e);
        entries.push(e);
    }
    return kMediaNoError;
}
void SyncSampleBox::compose(BitWriter& bw, const FileTypeBox& ftyp) { }

MediaError ShadowSyncSampleBox::parse(const BitReader& br, size_t sz, const FileTypeBox& ftyp) {
    Box::parse(br, sz, ftyp);
    uint32_t count              = br.rb32();
    for (uint32_t i = 0; i < count; ++i) {
        Entry e;
        e.shadowed_sample_number    = br.rb32();
        e.sync_sample_number        = br.rb32();

        DEBUGV("box %s: %" PRIu32 " %" PRIu32, 
                BOXNAME(type), e.shadowed_sample_number, e.sync_sample_number);
        entries.push(e);
    }
    return kMediaNoError;
}
void ShadowSyncSampleBox::compose(BitWriter& bw, const FileTypeBox& ftyp) { }

MediaError DegradationPriorityBox::parse(const BitReader& br, size_t sz, const FileTypeBox& ftyp) {
    Box::parse(br, sz, ftyp);
    CHECK_GT(sz, Box::size());
    size_t count = (sz - Box::size()) / 2;
    while (count--) { entries.push(br.rb16()); }
    return kMediaNoError;
}
void DegradationPriorityBox::compose(BitWriter& bw, const FileTypeBox& ftyp) { }

MediaError PaddingBitsBox::parse(const BitReader& br, size_t sz, const FileTypeBox& ftyp) {
    Box::parse(br, sz, ftyp);
    uint32_t count  = br.rb32();
    for (uint32_t i = 0; i < (count + 1) / 2; i++) {
        Entry e;
        br.skip(1);
        e.pad1  = br.read(3);
        br.skip(1);
        e.pad2  = br.read(3);
        entries.push(e);
    }
    return kMediaNoError;
}
void PaddingBitsBox::compose(BitWriter& bw, const FileTypeBox& ftyp) { }

MediaError FreeSpaceBox::parse(const BitReader& br, size_t sz, const FileTypeBox& ftyp) {
    Box::parse(br, sz, ftyp);
    br.skipBytes(sz);
    return kMediaNoError;
}
void FreeSpaceBox::compose(BitWriter& bw, const FileTypeBox& ftyp) { }

MediaError EditListBox::parse(const BitReader& br, size_t sz, const FileTypeBox& ftyp) {
    Box::parse(br, sz, ftyp);
    uint32_t count = br.rb32();
    for (uint32_t i = 0; i < count; i++) {
        Entry e;
        if (version == 1) {
            e.segment_duration    = br.rb64();
            e.media_time          = br.rb64();
        } else {
            e.segment_duration    = br.rb32();
            e.media_time          = br.rb32();
        }
        e.media_rate_integer      = br.rb16();
        e.media_rate_fraction     = br.rb16();
        DEBUGV("box %s: %" PRIu64 ", %" PRId64 ", %" PRIu16 ", %" PRIu16, 
                BOXNAME(type), e.segment_duration, e.media_time,
                e.media_rate_integer, e.media_rate_fraction);
    };
    return kMediaNoError;
}
void EditListBox::compose(BitWriter& bw, const FileTypeBox& ftyp) { }

MediaError NoticeBox::parse(const BitReader& br, size_t sz, const FileTypeBox& ftyp) {
    Box::parse(br, sz, ftyp);
    CHECK_GE(sz, 4 + 2);
    language    = languageCode(br);
    // maybe empty
    if (sz > 4 + 2) {
        value       = br.readS(sz - 4 - 2);
    }
    DEBUGV("box %s: value = %s", BOXNAME(type), value.c_str());
    return kMediaNoError;
}
void NoticeBox::compose(BitWriter& bw, const FileTypeBox& ftyp) { }

MediaError MovieExtendsHeaderBox::parse(const BitReader& br, size_t sz, const FileTypeBox& ftyp) {
    Box::parse(br, sz, ftyp);
    if (version == 1) {
        fragment_duration   = br.rb64();
    } else {
        fragment_duration   = br.rb32();
    }
    return kMediaNoError;
}
void MovieExtendsHeaderBox::compose(BitWriter& bw, const FileTypeBox& ftyp) { }

MediaError TrackExtendsBox::parse(const BitReader& br, size_t sz, const FileTypeBox& ftyp) {
    Box::parse(br, sz, ftyp);
    track_ID                        = br.rb32();
    default_sample_description_index    = br.rb32();
    default_sample_duration         = br.rb32();
    default_sample_size             = br.rb32();
    default_sample_flags            = br.rb32();
    return kMediaNoError;
}
void TrackExtendsBox::compose(BitWriter& bw, const FileTypeBox& ftyp) { }

MediaError MovieFragmentHeaderBox::parse(const BitReader& br, size_t sz, const FileTypeBox& ftyp) {
    Box::parse(br, sz, ftyp);
    sequence_number         = br.rb32();
    return kMediaNoError;
}
void MovieFragmentHeaderBox::compose(BitWriter& bw, const FileTypeBox& ftyp) { }

MediaError TrackFragmentHeaderBox::parse(const BitReader& br, size_t sz, const FileTypeBox& ftyp) {
    Box::parse(br, sz, ftyp);
    track_ID    = br.rb32();
    if (flags & 0x1)    base_data_offset            = br.rb32();
    if (flags & 0x2)    sample_description_index    = br.rb64();
    if (flags & 0x8)    default_sample_duration     = br.rb32();
    if (flags & 0x10)   default_sample_size         = br.rb32();
    if (flags & 0x20)   default_sample_flags        = br.rb32();
    return kMediaNoError;
}
void TrackFragmentHeaderBox::compose(BitWriter& bw, const FileTypeBox& ftyp) { }

MediaError PrimaryItemBox::parse(const BitReader& br, size_t sz, const FileTypeBox& ftyp) {
    Box::parse(br, sz, ftyp);
    item_ID     = br.rb16();
    return kMediaNoError;
}
void PrimaryItemBox::compose(BitWriter& bw, const FileTypeBox& ftyp) { }

MediaError ColourInformationBox::parse(const BitReader& br, size_t sz, const FileTypeBox& ftyp) {
    Box::parse(br, sz, ftyp);
    colour_type     = br.rb32();
    if (colour_type == kColourTypeNCLX) {
        colour_primaries            = br.rb16();
        transfer_characteristics    = br.rb16();
        matrix_coefficients         = br.rb16();
        full_range_flag             = br.read(1);
        br.skip(7); 
    } else if (colour_type == kColourTypeNCLC) {     // mov
        colour_primaries            = br.rb16();
        transfer_characteristics    = br.rb16();
        matrix_coefficients         = br.rb16();
    } else {
        // ICC_profile
        ERROR("box %s: TODO ICC_profile", BOXNAME(type));
        br.skipBytes(sz - Box::size() - 4);
    }
    return kMediaNoError;
}
void ColourInformationBox::compose(BitWriter& bw, const FileTypeBox& ftyp) { }

#if 1
MediaError siDecompressionParam::parse(const BitReader& br, size_t sz, const FileTypeBox& ftyp) {
    Box::parse(br, sz, ftyp);
    size_t next = Box::size();

    while (next + 8 <= sz) {
        // 8 bytes
        uint32_t boxSize    = br.rb32();
        uint32_t boxType    = br.rb32();
        DEBUG("box %s:  + %s %" PRIu32, BOXNAME(type), BOXNAME(type), boxSize);

        // terminator box
        if (boxType == kBoxTerminator || boxSize == 8) {
            next += 8;
            break;
        }

        next += boxSize;
#if LOG_NDEBUG == 0
        CHECK_GE(boxSize, 8);
        CHECK_LE(next, sz);
#else
        if (next > sz) {
            ERROR("box %s:  + skip broken box %s", BOXNAME(type), BOXNAME(boxType));
            break;
        }
#endif
        boxSize     -= 8;

        // 'mp4a' in 'wave' has different semantics
        if (boxType == kBoxTypeMP4A) {
            br.skip(boxSize * 8);
            continue;
        }

        const size_t offset = br.offset();
        sp<Box> box = MakeBoxByType(boxType);
        if (box == NULL) {
            ERROR("box %s:  + skip unknown box %s %" PRIu32,
                    BOXNAME(type), BOXNAME(boxType), boxSize);
#if LOG_NDEBUG == 0
            sp<Buffer> boxData = br.readB(boxSize);
            DEBUG("%s", boxData->string().c_str());
#else
            br.skipBytes(boxSize);
#endif
        } else if (box->parse(br, boxSize, ftyp) == kMediaNoError) {
            child.push(box);
        }
        const size_t delta = br.offset() - offset;
#if LOG_NDEBUG == 0
        CHECK_EQ(delta, boxSize * 8);
#else
        CHECK_LE(delta, boxSize * 8);
        if (delta != boxSize * 8) {
            br.skip(boxSize * 8 - delta);
        }
#endif
    }

    // skip junk
    if (next < sz) {
        DEBUG("box %s: skip %zu bytes", BOXNAME(type), sz - next);
        br.skipBytes(sz - next);
    }
    return kMediaNoError;
}
void siDecompressionParam::compose(BitWriter& bw, const FileTypeBox& ftyp) { }
#endif

//////////////////////////////////////////////////////////////////////////////
// iTunes MetaData 
//
// meta 
//  |- hdlr
//  |- mhdr
//  |- keys
//  |- ilst
//  |   |- '\xa9nam' 
//  |   |       |- data
//  |   |       |- itif
//  |   |       |- name
//  |- ctry
//  |- lang
MediaError iTunesHeaderBox::parse(const BitReader& br, size_t sz, const FileTypeBox& ftyp) {
    Box::parse(br, sz, ftyp);
    nextItemID  = br.rb32();
    return kMediaNoError;
}
void iTunesHeaderBox::compose(BitWriter& bw, const FileTypeBox& ftyp) { }

MediaError CountryListBox::parse(const BitReader& br, size_t sz, const FileTypeBox& ftyp) {
    Box::parse(br, sz, ftyp);
    uint32_t Entry_count    = br.rb32();
    uint16_t Country_count  = br.rb16();
    for (size_t i = 0; i < Country_count; ++i) {
        Entry e;
        for (size_t j = 0; j < Entry_count; ++j) {
            e.Countries.push(br.rb16());
        }
        entries.push(e);
    }
    return kMediaNoError;
}
void CountryListBox::compose(BitWriter& bw, const FileTypeBox& ftyp) { }

MediaError LanguageListBox::parse(const BitReader& br, size_t sz, const FileTypeBox& ftyp) {
    Box::parse(br, sz, ftyp);
    uint32_t Entry_count    = br.rb32();
    uint16_t Language_count = br.rb16();
    for (size_t i = 0; i < Language_count; ++i) {
        Entry e;
        for (size_t j = 0; j < Entry_count; ++j) {
            e.Languages.push(br.rb16());
        }
        entries.push(e);
    }
    return kMediaNoError;
}
void LanguageListBox::compose(BitWriter& bw, const FileTypeBox& ftyp) { }

MediaError iTunesStringBox::parse(const BitReader& br, size_t sz, const FileTypeBox& ftyp) {
    Box::parse(br, sz, ftyp);
    value       = br.readS(sz - Box::size());
    DEBUGV("box %s: %s", BOXNAME(type), value.c_str());
    return kMediaNoError;
}
void iTunesStringBox::compose(BitWriter& bw, const FileTypeBox& ftyp) { }

MediaError iTunesDataBox::parse(const BitReader& br, size_t sz, const FileTypeBox& ftyp) {
    Box::parse(br, sz, ftyp);
    CHECK_GE(sz, Box::size() + 8);
    // 8 bytes 
    CHECK_EQ(br.r8(), 0);            // type indicator byte
    Type_indicator      = br.rb24();
    Country_indicator   = br.rb16();
    Language_indicator  = br.rb16();
    DEBUGV("box %s: [%" PRIu32 "] [%" PRIx16 "-%" PRIx16 "]",
            BOXNAME(type), Type_indicator, Country_indicator, Language_indicator);
    if (sz > Box::size() + 8) {
        Value           = br.readB(sz - Box::size() - 8);
        DEBUGV("box %s: %s", BOXNAME(type), Value->string().c_str());
    }
    return kMediaNoError;
}
void iTunesDataBox::compose(BitWriter& bw, const FileTypeBox& ftyp) { }

MediaError iTunesInfomationBox::parse(const BitReader& br, size_t sz, const FileTypeBox& ftyp) {
    Box::parse(br, sz, ftyp);
    Item_ID     = br.rb32();
    return kMediaNoError;
}
void iTunesInfomationBox::compose(BitWriter& bw, const FileTypeBox& ftyp) { }

MediaError iTunesNameBox::parse(const BitReader& br, size_t sz, const FileTypeBox& ftyp) {
    Box::parse(br, sz, ftyp);
    Name = br.readS(sz - Box::size());
    DEBUGV("box %s: %s", BOXNAME(type), Name.c_str());
    return kMediaNoError;
}
void iTunesNameBox::compose(BitWriter& bw, const FileTypeBox& ftyp) { }

MediaError iTunesMeanBox::parse(const BitReader& br, size_t sz, const FileTypeBox& ftyp) {
    Box::parse(br, sz, ftyp);
    Mean = br.readS(sz - Box::size());
    DEBUGV("box %s: %s", BOXNAME(type), Mean.c_str());
    return kMediaNoError;
}
void iTunesMeanBox::compose(BitWriter& bw, const FileTypeBox& ftyp) { }

#if 1
MediaError iTunesItemListBox::parse(const BitReader& br, size_t sz, const FileTypeBox& ftyp) {
    if (!isQuickTime(ftyp)) {
        return ContainerBox::parse(br, sz, ftyp);
    } else {
        Box::parse(br, sz, ftyp);
        size_t next = Box::size();
        while (next + 8 < sz) {
            uint32_t _size  = br.rb32();
            uint32_t index  = br.rb32();
            DEBUG("box %s: size %" PRIu32 ", index %" PRIu32,
                    BOXNAME(type), _size, index);
            next            += _size;
            _size           -= 8;
            sp<Box> box     = new ContainerBox(kiTunesBoxTypeCustom);
            if (box->parse(br, _size, ftyp) == kMediaNoError) {
                key_index.push(index);
                child.push(box);
            }
        }
        return kMediaNoError;
    }
}
void iTunesItemListBox::compose(BitWriter& bw, const FileTypeBox& ftyp) { }
#endif

MediaError iTunesKeyDecBox::parse(const BitReader& br, size_t sz, const FileTypeBox& ftyp) {
    Box::parse(br, sz, ftyp);
    Keytypespace    = br.rb32();
    Key_value       = br.readB(sz - Box::size() - 4);
    DEBUG("box %s: %s\n%s", 
            BOXNAME(type), String(Keytypespace).c_str(), Key_value->string().c_str());
    return kMediaNoError;
}
void iTunesKeyDecBox::compose(BitWriter& bw, const FileTypeBox& ftyp) { }

///////////////////////////////////////////////////////////////////////////
// ID3v2
MediaError ID3v2Box::parse(const BitReader& br, size_t sz, const FileTypeBox& ftyp) {
    Box::parse(br, sz, ftyp);
    CHECK_GT(sz, Box::size() + 2);
    language    = languageCode(br);
    ID3v2data   = br.readB(sz - 2 - Box::size());
    return kMediaNoError;
}
void ID3v2Box::compose(BitWriter& bw, const FileTypeBox& ftyp) { }

MediaError ObjectDescriptorBox::parse(const BitReader& br, size_t sz, const FileTypeBox& ftyp) {
    Box::parse(br, sz, ftyp);
    iods = br.readB(sz - Box::size());
    DEBUGV("box %s: %s", BOXNAME(type), iods->string().c_str());
    return kMediaNoError;
}
void ObjectDescriptorBox::compose(BitWriter& bw, const FileTypeBox& ftyp) { }

///////////////////////////////////////////////////////////////////////////
template <class BoxType> static sp<Box> Create() { return new BoxType; }
#define RegisterBox(NAME, BoxType) \
    static RegisterHelper Reg##BoxType(NAME, Create<BoxType>)

RegisterBox(kBoxTypeMOOV, MovieBox);
RegisterBox(kBoxTypeMVHD, MovieHeaderBox);
RegisterBox(kBoxTypeTKHD, TrackHeaderBox);
RegisterBox(kBoxTypeMDHD, MediaHeaderBox);
RegisterBox(kBoxTypeHDLR, HandlerBox);
RegisterBox(kBoxTypeVMHD, VideoMediaHeaderBox);
RegisterBox(kBoxTypeSMHD, SoundMediaHeaderBox);
RegisterBox(kBoxTypeHMHD, HintMediaHeaderBox);
RegisterBox(kBoxTypeURL, DataEntryUrlBox);
RegisterBox(kBoxTypeURN, DataEntryUrnBox);
RegisterBox(kBoxTypeSTTS, TimeToSampleBox);
RegisterBox(kBoxTypeCTTS, CompositionOffsetBox);
RegisterBox(kBoxTypeCSLG, CompositionToDecodeBox);
RegisterBox(kBoxTypeSTSC, SampleToChunkBox);
RegisterBox(kBoxTypeSTSS, SyncSampleBox);
RegisterBox(kBoxTypeSTSH, ShadowSyncSampleBox);
RegisterBox(kBoxTypeSTDP, DegradationPriorityBox);
RegisterBox(kBoxTypeSDTP, SampleDependencyTypeBox);
RegisterBox(kBoxTypeBTRT, BitRateBox);
RegisterBox(kBoxTypePADB, PaddingBitsBox);
RegisterBox(kBoxTypeELST, EditListBox);
RegisterBox(kBoxTypeMEHD, MovieExtendsHeaderBox);
RegisterBox(kBoxTypeTREX, TrackExtendsBox);
RegisterBox(kBoxTypeMFHD, MovieFragmentHeaderBox);
RegisterBox(kBoxTypeTFHD, TrackFragmentHeaderBox);
RegisterBox(kBoxTypePITM, PrimaryItemBox);
RegisterBox(kBoxTypeCTRY, CountryListBox);
RegisterBox(kBoxTypeLANG, LanguageListBox);
RegisterBox(kBoxTypeIODS, ObjectDescriptorBox);
RegisterBox(kBoxTypeID32, ID3v2Box);
RegisterBox(kBoxTypeTRAK, TrackBox);
RegisterBox(kBoxTypeTREF, TrackReferenceBox);
RegisterBox(kBoxTypeMDIA, MediaBox);
RegisterBox(kBoxTypeMINF, MediaInformationBox);
RegisterBox(kBoxTypeDINF, DataInformationBox);
RegisterBox(kBoxTypeDREF, DataReferenceBox);
RegisterBox(kBoxTypeSTBL, SampleTableBox);
RegisterBox(kBoxTypeEDTS, EditBox);
RegisterBox(kBoxTypeUDTA, UserDataBox);
RegisterBox(kBoxTypeMVEX, MovieExtendsBox);
RegisterBox(kBoxTypeMOOF, MovieFragmentBox);
RegisterBox(kBoxTypeTRAF, TrackFragmentBox);
RegisterBox(kBoxTypeSTSD, SampleDescriptionBox);
RegisterBox(kBoxTypeHINT, TrackReferenceHintBox);
RegisterBox(kBoxTypeCDSC, TrackReferenceCdscBox);
RegisterBox(kBoxTypeDPND, TrackReferenceDpndBox);
RegisterBox(kBoxTypeIPIR, TrackReferenceIpirBox);
RegisterBox(kBoxTypeMPOD, TrackReferenceMpodBox);
RegisterBox(kBoxTypeSYNC, TrackReferenceSyncBox);
RegisterBox(kBoxTypeCHAP, TrackReferenceChapBox);
RegisterBox(kBoxTypeNMHB, NullMediaHeaderBox);
RegisterBox(kBoxTypeMP4V, MP4VisualSampleEntry);
RegisterBox(kBoxTypeAVC1, AVC1SampleEntry);
RegisterBox(kBoxTypeAVC2, AVC2SampleEntry);
RegisterBox(kBoxTypeHVC1, HVC1SampleEntry);
RegisterBox(kBoxTypeHEV1, HEV1SampleEntry);
RegisterBox(kBoxTypeRAW, RawAudioSampleEntry);
RegisterBox(kBoxTypeTWOS, TwosAudioSampleEntry);
RegisterBox(kBoxTypeMP4A, MP4AudioSampleEntry);
RegisterBox(kBoxTypeALAC, ALACAudioSampleEntry);
RegisterBox(kBoxTypeMP4S, MpegSampleEntry);
RegisterBox(kBoxTypeS263, H263SampleEntry);
RegisterBox(kBoxTypeESDS, ESDBox);
RegisterBox(kBoxTypeWAVE, siDecompressionParam);      // mov
RegisterBox(kBoxTypeAVCC, AVCConfigurationBox);
RegisterBox(kBoxTypeHVCC, HVCConfigurationBox);
RegisterBox(kBoxTypeD263, H263SpecificBox);
RegisterBox(kBoxTypeSAMR, AMRSampleEntry);
RegisterBox(kBoxTypeDAMR, AMRSpecificBox);
RegisterBox(kBoxTypeSGPD, SampleGroupDescriptionBox);
RegisterBox(kBoxTypeSTSZ, PreferredSampleSizeBox);
RegisterBox(kBoxTypeSTZ2, CompactSampleSizeBox);
RegisterBox(kBoxTypeSTCO, PreferredChunkOffsetBox);
RegisterBox(kBoxTypeCO64, LargeChunkOffsetBox);
RegisterBox(kBoxTypeFREE, FreeBox);
RegisterBox(kBoxTypeSKIP, SkipBox);
RegisterBox(kBoxTypeSRAT, SamplingRateBox);
// meta
RegisterBox(kBoxTypeMETA, MetaBox);
RegisterBox(kBoxTypeCPRT, CopyrightBox);
RegisterBox(kBoxTypeTITL, TitleBox);
RegisterBox(kBoxTypeDSCP, DescriptionBox);
RegisterBox(kBoxTypePERF, PerformerBox);
RegisterBox(kBoxTypeGNRE, GenreBox);
RegisterBox(kBoxTypeALBM, AlbumBox);
RegisterBox(kBoxTypeYRRC, YearBox);
RegisterBox(kBoxTypeLOCI, LocationBox);
RegisterBox(kBoxTypeAUTH, AuthorBox);
RegisterBox(kBoxTypeCOLR, ColourInformationBox);
// iTunes
RegisterBox(kiTunesBoxTypeILST, iTunesItemListBox);
RegisterBox(kiTunesBoxTypeTitle, iTunesTitleItemBox);
RegisterBox(kiTunesBoxTypeEncoder, iTunesEncoderItemBox);
RegisterBox(kiTunesBoxTypeAlbum, iTunesAlbumItemBox);
RegisterBox(kiTunesBoxTypeArtist, iTunesArtistItemBox);
RegisterBox(kiTunesBoxTypeComment, iTunesCommentItemBox);
RegisterBox(kiTunesBoxTypeGenre, iTunesGenreItemBox);
RegisterBox(kiTunesBoxTypeComposer, iTunesComposerItemBox);
RegisterBox(kiTunesBoxTypeYear, iTunesYearItemBox);
RegisterBox(kiTunesBoxTypeTrackNum, iTunesTrackNumItemBox);
RegisterBox(kiTunesBoxTypeDiskNum, iTunesDiskNumItemBox);
RegisterBox(kiTunesBoxTypeCompilation, iTunesCompilationItemBox);
RegisterBox(kiTunesBoxTypeBPM, iTunesBPMItemBox);
RegisterBox(kiTunesBoxTypeGaplessPlayback, iTunesGaplessPlaybackBox);
RegisterBox(kiTunesBoxTypeMHDR, iTunesHeaderBox);
RegisterBox(kiTunesBoxTypeKEYS, iTunesItemKeysBox);
RegisterBox(kiTunesBoxTypeDATA, iTunesDataBox);
RegisterBox(kiTunesBoxTypeInfomation, iTunesInfomationBox);
RegisterBox(kiTunesBoxTypeName, iTunesNameBox);
RegisterBox(kiTunesBoxTypeMean, iTunesMeanBox);
RegisterBox(kiTunesBoxTypeMDTA, iTunesMediaDataBox);
RegisterBox(kiTunesBoxTypeCustom, iTunesCustomBox);
// there is a 'keys' atom inside 'mebx', but it has different semantics
// than the one in 'meta'
//RegisterBox("mebx", TimedMetadataSampleDescriptionBox);
RegisterBox(kiTunesBoxTypeKeyDec, iTunesKeyDecBox);

#define IgnoreBox(NAME, BoxType)                                            \
    struct BoxType : public Box {                              \
        FORCE_INLINE BoxType() : Box(NAME) { }                              \
        FORCE_INLINE virtual MediaError parse(const BitReader& br, size_t sz, \
                const FileTypeBox& ftyp) {                                  \
            br.skipBytes(sz - Box::size());                                 \
            return kMediaNoError;                                                      \
        }                                                                   \
        virtual void compose(BitWriter& bw, const FileTypeBox& ftyp) {}     \
    };                                                                      \
RegisterBox(NAME, BoxType);
// Aperture box
IgnoreBox('tapt', TrackApertureModeDimensions);
IgnoreBox('alis', ApertureAliasDataReferenceBox);
IgnoreBox('chan', AudioChannelLayoutBox);
IgnoreBox('frma', SoundFormatBox);
IgnoreBox('mebx', TimedMetadataSampleDescriptionBox);
#undef IgnoreBox

///////////////////////////////////////////////////////////////////////////
//  |- trak *  <audio|video|hint>
//  |   |- tkhd *
//  |   |- mdia *
//  |   |   |- mdhd *
//  |   |   |- hdlr *
//  |   |   |- minf *
//  |   |   |   |- dinf *
//  |   |   |   |   |- dref *
//  |   |   |   |- stbl *
//  |   |   |   |   |- stsd *
//  |   |   |   |   |- stts *
//  |   |   |   |   |- stsc *
//  |   |   |   |   |- stco *
//  |   |   |   |   |- co64
//  |   |   |   |   |- stsz *
//  |   |   |   |   |- stz2
//  |   |   |   |   |- ctts
//  |   |   |   |   |- stss
bool CheckTrackBox(const sp<TrackBox>& trak) {
    if (!trak->container) return false;

    sp<TrackHeaderBox> tkhd = FindBox(trak, kBoxTypeTKHD);
    if (tkhd == NULL) return false;

    sp<MediaBox> mdia = FindBox(trak, kBoxTypeMDIA);
    if (mdia == NULL) return false;

    sp<MediaHeaderBox> mdhd = FindBox(mdia, kBoxTypeMDHD);
    if (mdhd == NULL) return false;

    sp<MediaInformationBox> minf = FindBox(mdia, kBoxTypeMINF);
    if (minf == NULL) return false;

    sp<DataInformationBox> dinf = FindBox(minf, kBoxTypeDINF);
    if (dinf == NULL) return false;

    sp<DataReferenceBox> dref = FindBox(dinf, kBoxTypeDREF);
    if (dref == NULL) return false;

    sp<SampleTableBox> stbl = FindBox(minf, kBoxTypeSTBL);
    if (stbl == NULL) return false;

    sp<SampleDescriptionBox> stsd = FindBox(stbl, kBoxTypeSTSD);
    if (stsd == NULL) return false;

    sp<TimeToSampleBox> stts    = FindBox(stbl, kBoxTypeSTTS);
    sp<SampleToChunkBox> stsc   = FindBox(stbl, kBoxTypeSTSC);
    sp<ChunkOffsetBox> stco     = FindBox2(stbl, kBoxTypeSTCO, kBoxTypeCO64);
    sp<SampleSizeBox> stsz      = FindBox2(stbl, kBoxTypeSTSZ, kBoxTypeSTZ2);

    if (stts == NULL || stsc == NULL || stco == NULL || stsz == NULL) {
        return false;
    }

    return true;
}

// find box in current container only
sp<Box> FindBox(const sp<ContainerBox>& root, uint32_t boxType, size_t index) {
    for (size_t i = 0; i < root->child.size(); i++) {
        sp<Box> box = root->child[i];
        if (box->type == boxType) {
            if (index == 0) return box;
            else --index;
        }
    }
    return NULL;
}

sp<Box> FindBox2(const sp<ContainerBox>& root, uint32_t first, uint32_t second) {
    for (size_t i = 0; i < root->child.size(); i++) {
        sp<Box> box = root->child[i];
        if (box->type == first || box->type == second) {
            return box;
        }
    }
    return NULL;
}

sp<Box> FindBoxInside(const sp<ContainerBox>& root, uint32_t sub, uint32_t target) {
    sp<Box> box = FindBox(root, sub);
    if (box == 0) return NULL;
    return FindBox(box, target);
}

static FORCE_INLINE void PrintBox(const sp<Box>& box, size_t n) {
    String line;
    if (n) {
        for (size_t i = 0; i < n - 2; ++i) line.append(" ");
        line.append("|- ");
    }
    line.append(BOXNAME(box->type));
    INFO("%s", line.c_str());
    n += 2;
    if (box->container) {
        sp<ContainerBox> c = box;
        for (size_t i = 0; i < c->child.size(); ++i) {
            PrintBox(c->child[i], n);
        }
    }
}
void PrintBox(const sp<Box>& root) {
    PrintBox(root, 0);
}

__END_NAMESPACE(MPEG4)
__END_NAMESPACE_MPX
