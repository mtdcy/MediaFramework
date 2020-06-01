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
#include "MediaDefs.h"
#include "Box.h"

//#define VERBOSE
#ifdef VERBOSE 
#define DEBUGV(fmt, ...) DEBUG(fmt, ##__VA_ARGS__)
#else
#define DEBUGV(fmt, ...) do {} while(0)
#endif

__BEGIN_NAMESPACE_MPX
__BEGIN_NAMESPACE(MPEG4)

FileTypeBox::FileTypeBox(const BitReader& br, size_t size) {
    CHECK_GE(size, 12);
    major_brand     = br.readS(4);
    minor_version   = br.rb32();
    INFO("major: %s, minor: 0x%" PRIx32,
            major_brand.c_str(), minor_version);

    compatibles     = br.readS(size - 8);
    INFO("compatible brands: %s", compatibles.c_str());
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
            name.c_str(), sz, version, flags);
    return kMediaNoError;
}

void Box::compose(BitWriter& bw, const FileTypeBox& ftyp) {
}

MediaError ContainerBox::parse(const BitReader& br, size_t sz, const FileTypeBox& ftyp) {
    DEBUGV("box %s %zu", name.c_str(), sz);
    Box::parse(br, sz, ftyp);
    return _parse(br, sz - Box::size(), ftyp);
}
MediaError ContainerBox::_parse(const BitReader& br, size_t sz, const FileTypeBox& ftyp) {
    // FIXME: count is not used
    size_t next = 0;
    if (counted) {
        uint32_t count = br.rb32();     next += 4;
        DEBUG("box %s: count = %" PRIu32, name.c_str(), count);
    }

    while (next + 8 <= sz) {
        // 8 bytes
        uint32_t boxSize        = br.rb32();
        const String boxType    = br.readS(4);
        DEBUG("box %s:  + %s %" PRIu32, name.c_str(),
                boxType.c_str(), boxSize);

        // mov terminator box
        if (boxType == "" && boxSize == 8) {
            DEBUGV("box %s:  + terminator box %s", 
                    name.c_str(),
                    boxType.c_str());
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
                    name.c_str(),
                    boxType.c_str());
            break;
        }
#endif

        boxSize     -= 8;
        // this exists in mov
        if (boxSize == 0) {
            DEBUG("box %s:  + skip empty box %s", 
                    name.c_str(),
                    boxType.c_str());
            continue;
        }

        const size_t offset = br.offset();
        sp<Box> box = MakeBoxByName(boxType);
        if (box == NULL) {
            ERROR("box %s:  + skip unknown box %s %" PRIu32, 
                    name.c_str(),
                    boxType.c_str(), boxSize);
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
                name.c_str(),
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

    DEBUGV("box %s: child.size() = %zu", name.c_str(), child.size());
    return kMediaNoError;
}
void ContainerBox::compose(BitWriter&, const FileTypeBox&) { }

typedef sp<Box> (*create_t)();
static HashTable<String, create_t> sRegister;
struct RegisterHelper {
    RegisterHelper(const String& NAME, create_t callback) {
        sRegister.insert(NAME, callback);
    }
};

sp<Box> MakeBoxByName(const String& name) {
    if (sRegister.find(name)) {
        return sRegister[name]();
    }
    ERROR("can NOT find register for %s ..............", name.c_str());
    return NULL;
}

// isom and quicktime using different semantics for MetaBox
static const bool isQuickTime(const FileTypeBox& ftyp) {
    if (ftyp.major_brand == "qt  " ||
            ftyp.compatibles.indexOf("qt  ") >= 0) {
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
    DEBUGV("box %s %zu", name.c_str(), sz);
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
            name.c_str(),
            creation_time, modification_time, timescale, duration);

    rate            = br.rb32();
    volume          = br.rb16();
    br.skip(16);        // reserved
    br.skip(32 * 2);    // reserved
    br.skip(32 * 9);    // matrix 
    br.skip(32 * 6);    // pre_defined
    next_track_ID   = br.rb32();

    DEBUGV("box %s: rate %" PRIu32 ", volume %" PRIu16 ", next %" PRIu32,
            name.c_str(),
            rate, volume, next_track_ID);
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
            name.c_str(), 
            creation_time, modification_time,
            track_ID, duration);

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
            name.c_str(),
            layer, alternate_group, volume,
            width, height);

    return kMediaNoError;
}

void TrackHeaderBox::compose(BitWriter& bw, const FileTypeBox& ftyp) { }

MediaError TrackReferenceTypeBox::parse(const BitReader& br, size_t sz, const FileTypeBox& ftyp) {
    Box::parse(br, sz, ftyp);
    CHECK_FALSE(sz % 4);
    size_t count = sz / 4;
    while (count--) {
        uint32_t id = br.rb32();
        DEBUGV("box %s: %" PRIu32, name.c_str(), id);
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
            name.c_str(),
            creation_time, modification_time, timescale, duration);

    language = languageCode(br);
    DEBUGV("box %s: lang %s", name.c_str(), language.c_str());

    br.skip(16);    // pre_defined
    return kMediaNoError;
}
void MediaHeaderBox::compose(BitWriter& bw, const FileTypeBox& ftyp) { }

MediaError HandlerBox::parse(const BitReader& br, size_t sz, const FileTypeBox& ftyp) {
    Box::parse(br, sz, ftyp);
    CHECK_GE(sz, 4 + 20 + 1);

    br.skip(32); // pre_defined
    handler_type        = br.readS(4);
    br.skip(32 * 3); // reserved
    handler_name        = br.readS(sz - Box::size() - 20);

    DEBUGV("box %s: type %s name %s", name.c_str(),
            handler_type.c_str(), handler_name.c_str());
    return kMediaNoError;
}
void HandlerBox::compose(BitWriter& bw, const FileTypeBox& ftyp) { }

MediaError VideoMediaHeaderBox::parse(const BitReader& br, size_t sz, const FileTypeBox& ftyp) {
    Box::parse(br, sz, ftyp);
    graphicsmode        = br.rb16();
    DEBUGV("box %s: graphicsmode %" PRIu16, 
            name.c_str(), graphicsmode);
    for (size_t i = 0; i < 3; i++) {
        uint16_t color = br.rb16();
        DEBUGV("box %s: color %" PRIu16, name.c_str(), color);
        opcolor.push(color);
    }
    return kMediaNoError;
}
void VideoMediaHeaderBox::compose(BitWriter& bw, const FileTypeBox& ftyp) { }

MediaError SoundMediaHeaderBox::parse(const BitReader& br, size_t sz, const FileTypeBox& ftyp) {
    Box::parse(br, sz, ftyp);
    balance         = br.rb16();
    br.skip(16);    // reserved

    DEBUGV("box %s: balance %" PRIu16, name.c_str(), balance);
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
            name.c_str(),
            maxPDUsize, avgPDUsize, maxbitrate, avgbitrate);
    return kMediaNoError;
}
void HintMediaHeaderBox::compose(BitWriter& bw, const FileTypeBox& ftyp) { }

MediaError DataEntryUrlBox::parse(const BitReader& br, size_t sz, const FileTypeBox& ftyp) {
    Box::parse(br, sz, ftyp);
    if (sz > 4) {
        location    = br.readS(sz - 4);
        DEBUGV("box %s: location %s", 
                name.c_str(), location.c_str());
    }
    return kMediaNoError;
}
void DataEntryUrlBox::compose(BitWriter& bw, const FileTypeBox& ftyp) { }

MediaError DataEntryUrnBox::parse(const BitReader& br, size_t sz, const FileTypeBox& ftyp) {
    Box::parse(br, sz, ftyp);
    char c;
    while ((c = (char)br.r8()) != '\0') urn_name.append(String(c));
    location        = br.readS(sz - 4 - urn_name.size());

    DEBUGV("box %s: name %s location %s", 
            name.c_str(), 
            urn_name.c_str(),
            location.c_str());
    return kMediaNoError;
}
void DataEntryUrnBox::compose(BitWriter& bw, const FileTypeBox& ftyp) { }

MediaError TimeToSampleBox::parse(const BitReader& br, size_t sz, const FileTypeBox& ftyp) {
    Box::parse(br, sz, ftyp);
    uint32_t count  = br.rb32();
    for (uint32_t i = 0; i < count; i++) {
        Entry e = { br.rb32(), br.rb32() };
        DEBUGV("box %s: entry %" PRIu32 " %" PRIu32, name.c_str(),
                e.sample_count, e.sample_delta);
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
                name.c_str(),
                e.sample_count, e.sample_offset);
        entries.push(e);
    }
    return kMediaNoError;
}
void CompositionOffsetBox::compose(BitWriter& bw, const FileTypeBox& ftyp) { }

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
    DEBUGV("box %s: child.size() = %zu", name.c_str(), child.size());
    return kMediaNoError;
}
MediaError SampleEntry::_parse(const BitReader& br, size_t sz, const FileTypeBox& ftyp) {
    // 8 bytes
    br.skip(8 * 6);
    data_reference_index    = br.rb16();

    if (type == "vide") {
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
                name.c_str(), 
                visual.width, visual.height, 
                visual.horizresolution, visual.vertresolution,
                visual.frame_count, 
                compressorname.c_str(), 
                visual.depth);
    } else if (type == "soun") {
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
                name.c_str(),
                sound.channelcount, 
                sound.samplesize, 
                sound.samplerate);

        if (ftyp.major_brand == "qt  " ||
                ftyp.compatibles.indexOf("qt  ") >= 0) {
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
                        name.c_str(),
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
    } else if (type == "hint") {
        // NOTHING
    } else {
        // NOTHING
    }
    return kMediaNoError;
}
void SampleEntry::compose(BitWriter& bw, const FileTypeBox& ftyp) { }

MediaError ALACAudioSampleEntry::parse(const BitReader& br, size_t sz, const FileTypeBox& ftyp) {
    const size_t offset = br.offset();
    Box::parse(br, sz, ftyp);
    SampleEntry::_parse(br, sz - Box::size(), ftyp);
    extra = br.readB(sz - (br.offset()- offset) / 8);
    DEBUG("box %s: %s", extra->string(true).c_str());
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
    DEBUG("box %s: %s", data->string(true).c_str());
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
    if (name == "stz2") {
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
                    name.c_str(), sample_size);
        }
    }

#if 1
    for (size_t i = 0; i < entries.size(); ++i) {
        DEBUGV("box %s: %" PRIu64, name.c_str(), entries[i]);
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
                name.c_str(),
                e.first_chunk, e.samples_per_chunk, e.sample_description_index);

        entries.push(e);
    }
    return kMediaNoError;
}
void SampleToChunkBox::compose(BitWriter& bw, const FileTypeBox& ftyp) { }

MediaError ChunkOffsetBox::parse(const BitReader& br, size_t sz, const FileTypeBox& ftyp) {
    Box::parse(br, sz, ftyp);
    uint32_t count          = br.rb32();
    if (name == "co64") { // offset64
        for (uint32_t i = 0; i < count; ++i) {
            uint64_t e          = br.rb64();
            DEBUGV("box %s: %" PRIu64, name.c_str(), e);
            entries.push(e);
        }
    } else {
        for (uint32_t i = 0; i < count; ++i) {
            uint32_t e          = br.rb32();
            DEBUGV("box %s: %" PRIu32, name.c_str(), e);
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
        DEBUGV("box %s: %" PRIu32, name.c_str(), e);
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
                name.c_str(),
                e.shadowed_sample_number, e.sync_sample_number);
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
                name.c_str(),
                e.segment_duration, e.media_time,
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
    DEBUGV("box %s: value = %s", name.c_str(), value.c_str());
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
    colour_type     = br.readS(4);
    if (colour_type == "nclx") {
        colour_primaries            = br.rb16();
        transfer_characteristics    = br.rb16();
        matrix_coefficients         = br.rb16();
        full_range_flag             = br.read(1);
        br.skip(7); 
    } else if (colour_type == "nclc") {     // mov
        colour_primaries            = br.rb16();
        transfer_characteristics    = br.rb16();
        matrix_coefficients         = br.rb16();
    } else {
        // ICC_profile
        ERROR("box %s: TODO ICC_profile", name.c_str());
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
        uint32_t boxSize        = br.rb32();
        const String boxType    = br.readS(4);
        DEBUG("box %s:  + %s %" PRIu32, name.c_str(),
                boxType.c_str(), boxSize);

        // terminator box
        if (boxType == "" || boxSize == 8) {
            next += 8;
            break;
        }

        next += boxSize;
#if LOG_NDEBUG == 0
        CHECK_GE(boxSize, 8);
        CHECK_LE(next, sz);
#else
        if (next > sz) {
            ERROR("box %s:  + skip broken box %s",
                    name.c_str(),
                    boxType.c_str());
            break;
        }
#endif
        boxSize     -= 8;

        // 'mp4a' in 'wave' has different semantics
        if (boxType == "mp4a") {
            br.skip(boxSize * 8);
            continue;
        }

        const size_t offset = br.offset();
        sp<Box> box = MakeBoxByName(boxType);
        if (box == NULL) {
            ERROR("box %s:  + skip unknown box %s %" PRIu32,
                    name.c_str(),
                    boxType.c_str(), boxSize);
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
        DEBUG("box %s: skip %zu bytes", name.c_str(), sz - next);
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
    DEBUGV("box %s: %s", name.c_str(), value.c_str());
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
            name.c_str(),
            Type_indicator, Country_indicator, Language_indicator);
    if (sz > Box::size() + 8) {
        Value           = br.readB(sz - Box::size() - 8);
        DEBUGV("box %s: %s", name.c_str(), Value->string().c_str());
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
    DEBUGV("box %s: %s", name.c_str(), Name.c_str());
    return kMediaNoError;
}
void iTunesNameBox::compose(BitWriter& bw, const FileTypeBox& ftyp) { }

MediaError iTunesMeanBox::parse(const BitReader& br, size_t sz, const FileTypeBox& ftyp) {
    Box::parse(br, sz, ftyp);
    Mean = br.readS(sz - Box::size());
    DEBUGV("box %s: %s", name.c_str(), Mean.c_str());
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
                    name.c_str(), _size, index);
            next            += _size;
            _size           -= 8;
            sp<Box> box      = new ContainerBox("*");
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
    Key_namespace   = br.readS(4);
    Key_value       = br.readB(sz - Box::size() - 4);
    DEBUG("box %s: %s\n%s", name.c_str(), 
            Key_namespace.c_str(), Key_value->string().c_str());
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
    DEBUGV("box %s: %s", name.c_str(), iods->string().c_str());
    return kMediaNoError;
}
void ObjectDescriptorBox::compose(BitWriter& bw, const FileTypeBox& ftyp) { }

///////////////////////////////////////////////////////////////////////////
template <class BoxType> static sp<Box> Create() { return new BoxType; }
#define RegisterBox(NAME, BoxType) \
    static RegisterHelper Reg##BoxType(NAME, Create<BoxType>)

RegisterBox("moov", MovieBox);
RegisterBox("mvhd", MovieHeaderBox);
RegisterBox("tkhd", TrackHeaderBox);
RegisterBox("mdhd", MediaHeaderBox);
RegisterBox("hdlr", HandlerBox);
RegisterBox("vmhd", VideoMediaHeaderBox);
RegisterBox("smhd", SoundMediaHeaderBox);
RegisterBox("hmhd", HintMediaHeaderBox);
RegisterBox("url ", DataEntryUrlBox);
RegisterBox("urn ", DataEntryUrnBox);
RegisterBox("stts", TimeToSampleBox);
RegisterBox("ctts", CompositionOffsetBox);
RegisterBox("stsc", SampleToChunkBox);
RegisterBox("stss", SyncSampleBox);
RegisterBox("stsh", ShadowSyncSampleBox);
RegisterBox("stdp", DegradationPriorityBox);
RegisterBox("sdtp", SampleDependencyTypeBox);
RegisterBox("btrt", BitRateBox);
RegisterBox("padb", PaddingBitsBox);
RegisterBox("elst", EditListBox);
RegisterBox("mehd", MovieExtendsHeaderBox);
RegisterBox("trex", TrackExtendsBox);
RegisterBox("mfhd", MovieFragmentHeaderBox);
RegisterBox("tfhd", TrackFragmentHeaderBox);
RegisterBox("pitm", PrimaryItemBox);
RegisterBox("ctry", CountryListBox);
RegisterBox("lang", LanguageListBox);
RegisterBox("iods", ObjectDescriptorBox);
RegisterBox("ID32", ID3v2Box);
RegisterBox("trak", TrackBox);
RegisterBox("tref", TrackReferenceBox);
RegisterBox("mdia", MediaBox);
RegisterBox("minf", MediaInformationBox);
RegisterBox("dinf", DataInformationBox);
RegisterBox("dref", DataReferenceBox);
RegisterBox("stbl", SampleTableBox);
RegisterBox("edts", EditBox);
RegisterBox("udta", UserDataBox);
RegisterBox("mvex", MovieExtendsBox);
RegisterBox("moof", MovieFragmentBox);
RegisterBox("traf", TrackFragmentBox);
RegisterBox("stsd", SampleDescriptionBox);
RegisterBox("hint", TrackReferenceHintBox);
RegisterBox("cdsc", TrackReferenceCdscBox);
RegisterBox("dpnd", TrackReferenceDpndBox);
RegisterBox("ipir", TrackReferenceIpirBox);
RegisterBox("mpod", TrackReferenceMpodBox);
RegisterBox("sync", TrackReferenceSyncBox);
RegisterBox("chap", TrackReferenceChapBox);
RegisterBox("nmhb", NullMediaHeaderBox);
RegisterBox("mp4v", MP4VisualSampleEntry);
RegisterBox("avc1", AVC1SampleEntry);
RegisterBox("avc2", AVC2SampleEntry);
RegisterBox("hvc1", HVC1SampleEntry);
RegisterBox("hev1", HEV1SampleEntry);
RegisterBox("raw ", RawAudioSampleEntry);
RegisterBox("twos", TwosAudioSampleEntry);
RegisterBox("mp4a", MP4AudioSampleEntry);
RegisterBox("alac", ALACAudioSampleEntry);
RegisterBox("mp4s", MpegSampleEntry);
RegisterBox("s263", H263SampleEntry);
RegisterBox("esds", ESDBox);
RegisterBox("wave", siDecompressionParam);      // mov
RegisterBox("avcC", AVCConfigurationBox);
RegisterBox("hvcC", HVCConfigurationBox);
RegisterBox("d263", H263SpecificBox);
RegisterBox("samr", AMRSampleEntry);
RegisterBox("damr", AMRSpecificBox);
RegisterBox("stsz", PreferredSampleSizeBox);
RegisterBox("stz2", CompactSampleSizeBox);
RegisterBox("stco", PreferredChunkOffsetBox);
RegisterBox("co64", LargeChunkOffsetBox);
RegisterBox("free", FreeBox);
RegisterBox("skip", SkipBox);
RegisterBox("srat", SamplingRateBox);
// meta
RegisterBox("meta", MetaBox);
RegisterBox("cprt", CopyrightBox);
RegisterBox("titl", TitleBox);
RegisterBox("dscp", DescriptionBox);
RegisterBox("perf", PerformerBox);
RegisterBox("gnre", GenreBox);
RegisterBox("albm", AlbumBox);
RegisterBox("yrrc", YearBox);
RegisterBox("loci", LocationBox);
RegisterBox("auth", AuthorBox);
RegisterBox("colr", ColourInformationBox);
// iTunes
RegisterBox("ilst", iTunesItemListBox);
RegisterBox("\xa9nam", iTunesTitleItemBox);
RegisterBox("\xa9too", iTunesEncoderItemBox);
RegisterBox("\xa9""alb", iTunesAlbumItemBox);
RegisterBox("\xa9""ART", iTunesArtistItemBox);
RegisterBox("\xa9""cmt", iTunesCommentItemBox);
RegisterBox("\xa9gen", iTunesGenreItemBox);
RegisterBox("\xa9wrt", iTunesComposerItemBox);
RegisterBox("\xa9""day", iTunesYearItemBox);
RegisterBox("trkn", iTunesTrackNumItemBox);
RegisterBox("disk", iTunesDiskNumItemBox);
RegisterBox("cpil", iTunesCompilationItemBox);
RegisterBox("tmpo", iTunesBPMItemBox);
RegisterBox("pgap", iTunesGaplessPlaybackBox);
RegisterBox("mhdr", iTunesHeaderBox);
RegisterBox("keys", iTunesItemKeysBox);
RegisterBox("data", iTunesDataBox);
RegisterBox("itif", iTunesInfomationBox);
RegisterBox("name", iTunesNameBox);
RegisterBox("mean", iTunesMeanBox);
RegisterBox("mdta", iTunesMediaDataBox);
RegisterBox("----", iTunesCustomBox);
// there is a 'keys' atom inside 'mebx', but it has different semantics
// than the one in 'meta'
//RegisterBox("mebx", TimedMetadataSampleDescriptionBox);
RegisterBox("keyd", iTunesKeyDecBox);

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
IgnoreBox("tapt", TrackApertureModeDimensions);
IgnoreBox("alis", ApertureAliasDataReferenceBox);
IgnoreBox("chan", AudioChannelLayoutBox);
IgnoreBox("frma", SoundFormatBox);
IgnoreBox("mebx", TimedMetadataSampleDescriptionBox);
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

    sp<TrackHeaderBox> tkhd = FindBox(trak, "tkhd");
    if (tkhd == NULL) return false;

    sp<MediaBox> mdia = FindBox(trak, "mdia");
    if (mdia == NULL) return false;

    sp<MediaHeaderBox> mdhd = FindBox(mdia, "mdhd");
    if (mdhd == NULL) return false;

    sp<MediaInformationBox> minf = FindBox(mdia, "minf");
    if (minf == NULL) return false;

    sp<DataInformationBox> dinf = FindBox(minf, "dinf");
    if (dinf == NULL) return false;

    sp<DataReferenceBox> dref = FindBox(dinf, "dref");
    if (dref == NULL) return false;

    sp<SampleTableBox> stbl = FindBox(minf, "stbl");
    if (stbl == NULL) return false;

    sp<SampleDescriptionBox> stsd = FindBox(stbl, "stsd");
    if (stsd == NULL) return false;

    sp<TimeToSampleBox> stts    = FindBox(stbl, "stts");
    sp<SampleToChunkBox> stsc   = FindBox(stbl, "stsc");
    sp<ChunkOffsetBox> stco     = FindBox(stbl, "stco", "co64");
    sp<SampleSizeBox> stsz      = FindBox(stbl, "stsz", "stz2");

    if (stts == NULL || stsc == NULL || stco == NULL || stsz == NULL) {
        return false;
    }

    return true;
}

// find box in current container only
sp<Box> FindBox(const sp<ContainerBox>& root,
        const String& boxType, size_t index) {
    for (size_t i = 0; i < root->child.size(); i++) {
        sp<Box> box = root->child[i];
        if (box->name == boxType) {
            if (index == 0) return box;
            else --index;
        }
    }
    return NULL;
}

sp<Box> FindBox(const sp<ContainerBox>& root, 
        const String& first, const String& second) {
    for (size_t i = 0; i < root->child.size(); i++) {
        sp<Box> box = root->child[i];
        if (box->name == first ||
                box->name == second) {
            return box;
        }
    }
    return NULL;
}

sp<Box> FindBoxInside(const sp<ContainerBox>& root, 
        const String& sub, const String& target) {
    sp<Box> box = FindBox(root, sub);
    if (box == 0) return NULL;
    return FindBox(box, target);
}

void PrintBox(const sp<Box>& box, size_t n) {
    String line;
    if (n) {
        for (size_t i = 0; i < n - 2; ++i) line.append(" ");
        line.append("|- ");
    }
    line.append(box->name);
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
