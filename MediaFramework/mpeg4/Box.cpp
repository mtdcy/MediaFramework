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

#define VERBOSE
#ifdef VERBOSE
#define DEBUGV(fmt, ...) DEBUG(fmt, ##__VA_ARGS__)
#else
#define DEBUGV(fmt, ...) do {} while(0)
#endif

__BEGIN_NAMESPACE_MPX
__BEGIN_NAMESPACE(MPEG4)

static FORCE_INLINE const Char * BOXNAME(UInt32 x) {
    static Char tmp[5];
    tmp[0]  = (x >> 24) & 0xff;
    tmp[1]  = (x >> 16) & 0xff;
    tmp[2]  = (x >> 8) & 0xff;
    tmp[3]  = x & 0xff;
    tmp[4]  = '\0';
    return &tmp[0];
}

const Char * BoxName(UInt32 x) {
    return BOXNAME(x);
}

MediaError FileTypeBox::parse(const sp<ABuffer>& buffer, const sp<FileTypeBox>&) {
    CHECK_GE(buffer->size(), 12);
    major_brand     = buffer->rb32();
    minor_version   = buffer->rb32();
    INFO("major: %s, minor: 0x%" PRIx32, BOXNAME(major_brand), minor_version);

    while (buffer->size() >= 4) {
        UInt32 brand = buffer->rb32();
        INFO("compatible brand: %s", BOXNAME(brand));
        compatibles.push(brand);
    }
    return kMediaNoError;
}

Box::Box(UInt32 type, UInt8 cls) : Name(BOXNAME(type)), Type(type), Class(cls),
Version(0), Flags(0) {
    
}

MediaError Box::parse(const sp<ABuffer>& buffer, const sp<FileTypeBox>& ftyp) {
    if (Class & kBoxFull) {
        Version     = buffer->r8();
        Flags       = buffer->rb24();
    }
    DEBUG("box %s Version = %" PRIu32 " flags = %" PRIx32,
          Name.c_str(), Version, Flags);
    return kMediaNoError;
}

void Box::compose(sp<ABuffer>& buffer, const sp<FileTypeBox>& ftyp) {
}

MediaError ContainerBox::parse(const sp<ABuffer>& buffer, const sp<FileTypeBox>& ftyp) {
    Box::parse(buffer, ftyp);
    return _parse(buffer, ftyp);
}
MediaError ContainerBox::_parse(const sp<ABuffer>& buffer, const sp<FileTypeBox>& ftyp) {
    if (counted) {
        // FIXME: count is not used
        UInt32 count = buffer->rb32();
    }

    while (buffer->size()) {
        if (buffer->size() < 8) {
            ERROR("box %s:  + broken box", Name.c_str());
            break;
        }
        // 8 bytes
        UInt32 boxSize    = buffer->rb32();
        UInt32 boxType    = buffer->rb32();
        DEBUG("box %s:  + %s %" PRIu32, Name.c_str(), BOXNAME(boxType), boxSize);

        // mov terminator box
        if (boxType == kBoxTerminator && boxSize == 8) {
            DEBUG("box %s:  + terminator box [%s], remain %zu bytes",
                    Name.c_str(), BOXNAME(boxType), (UInt32)buffer->size());
            // XXX: terminator is not terminator
            break;
            //continue;
        }
        
        if (boxSize < 8) {
            ERROR("box %s:  + broken box %s", Name.c_str(), BOXNAME(boxType));
            break;
        }

        boxSize     -= 8;
        // this exists in mov
        if (boxSize == 0) {
            DEBUG("box %s:  + skip empty box %s", Name.c_str(), BOXNAME(boxType));
            continue;
        }
        
        // 'mp4a' in 'wave' has different semantics
        if (Type == kBoxTypeWAVE && boxType == kBoxTypeMP4A) {
            // TODO
            buffer->skipBytes(boxSize);
            continue;
        }

        sp<ABuffer> boxData = buffer->readBytes(boxSize);
        sp<Box> box = MakeBoxByType(boxType);
        if (box == Nil) {
            ERROR("box %s:  + skip unknown box %s %" PRIu32, Name.c_str(), BOXNAME(boxType), boxSize);
        } else if (box->parse(boxData, ftyp) == kMediaNoError) {
            child.push(box);
        }
    }

    // qtff.pdf, Section "User Data Atoms", Page 37
    // For historical reasons, the data list is optionally 
    // terminated by a 32-bit integer set to 0. If you are 
    // writing a program to read user data atoms, you should 
    // allow for the terminating 0. However, 
    // if you are writing a program to create user data atoms, 
    // you can safely leave out the trailing 0.

    DEBUGV("box %s: + child.size() = %zu", Name.c_str(), child.size());
    return kMediaNoError;
}
void ContainerBox::compose(sp<ABuffer>&, const sp<FileTypeBox>&) { }

typedef sp<Box> (*create_t)();
static HashTable<UInt32, create_t> sRegister;
struct RegisterHelper {
    RegisterHelper(UInt32 TYPE, create_t callback) {
        sRegister.insert(TYPE, callback);
    }
};

sp<Box> MakeBoxByType(UInt32 type) {
    if (sRegister.find(type)) {
        return sRegister[type]();
    }
    ERROR("can NOT find register for %s ..............", BOXNAME(type));
    return Nil;
}

// isom and quicktime using different semantics for MetaBox
static const Bool isQuickTime(const sp<FileTypeBox>& ftyp) {
    if (ftyp->major_brand == kBrandTypeQuickTime) {
        return True;
    }
    for (UInt32 i = 0; i < ftyp->compatibles.size(); ++i) {
        if (ftyp->compatibles[i] == kBrandTypeQuickTime)
            return True;
    }
    return False;
}

MediaError MetaBox::parse(const sp<ABuffer>& buffer, const sp<FileTypeBox>& ftyp) {
    Box::parse(buffer, ftyp);
    UInt32 next = Box::size();
    if (!isQuickTime(ftyp)) {
        Version     = buffer->r8();
        Flags       = buffer->rb24();
        next        += 4;
    }

    ContainerBox::_parse(buffer, ftyp);
    return kMediaNoError;
}
void MetaBox::compose(sp<ABuffer>& buffer, const sp<FileTypeBox>& ftyp) { }

MediaError MovieHeaderBox::parse(const sp<ABuffer>& buffer, const sp<FileTypeBox>& ftyp) {
    Box::parse(buffer, ftyp);
    CHECK_EQ(buffer->size(), ((Version == 0) ? 96 : 108));

    if (Version == 1) {
        creation_time       = buffer->rb64();
        modification_time   = buffer->rb64();
        timescale           = buffer->rb32();
        duration            = buffer->rb64();
    } else {
        creation_time       = buffer->rb32();
        modification_time   = buffer->rb32();
        timescale           = buffer->rb32();
        duration            = buffer->rb32();
    }

    DEBUGV("box %s: creation %" PRIu64 ", modification %" PRIu64 
            ", timescale %" PRIu32 ", duration %" PRIu64, 
            Name.c_str(), creation_time, modification_time, timescale, duration);

    rate            = buffer->rb32();
    volume          = buffer->rb16();
    buffer->skip(16);        // reserved
    buffer->skip(32 * 2);    // reserved
    buffer->skip(32 * 9);    // matrix
    buffer->skip(32 * 6);    // pre_defined
    next_track_ID   = buffer->rb32();

    DEBUGV("box %s: rate %" PRIu32 ", volume %" PRIu16 ", next %" PRIu32,
            Name.c_str(), rate, volume, next_track_ID);
    return kMediaNoError;
}

void MovieHeaderBox::compose(sp<ABuffer>& buffer, const sp<FileTypeBox>& ftyp) { }

MediaError TrackHeaderBox::parse(const sp<ABuffer>& buffer, const sp<FileTypeBox>& ftyp) {
    Box::parse(buffer, ftyp);
    if (Version == 1) {
        creation_time       = buffer->rb64();
        modification_time   = buffer->rb64();
        track_ID            = buffer->rb32();
        buffer->skip(32);
        duration            = buffer->rb64();
    } else {
        creation_time       = buffer->rb32();
        modification_time   = buffer->rb32();
        track_ID            = buffer->rb32();
        buffer->skip(32);
        duration            = buffer->rb32();
    }

    DEBUGV("box %s: creation_time %" PRIu64 ", modification_time %" PRIu64
            ", track id %" PRIu32 ", duration %" PRIu64, 
            Name.c_str(), creation_time, modification_time, track_ID, duration);

    buffer->skip(32 * 2);
    layer           = buffer->rb16();
    alternate_group = buffer->rb16();
    volume          = buffer->rb16();
    buffer->skip(16);
    buffer->skip(32 * 9);
    width           = buffer->rb32();
    height          = buffer->rb32();

    DEBUGV("box %s: layer %" PRIu16 ", alternate_group %" PRIu16 
            ", volume %" PRIu16 ", width %" PRIu32 ", height %" PRIu32, 
            Name.c_str(), layer, alternate_group, volume, width, height);

    return kMediaNoError;
}

void TrackHeaderBox::compose(sp<ABuffer>& buffer, const sp<FileTypeBox>& ftyp) { }

MediaError TrackReferenceTypeBox::parse(const sp<ABuffer>& buffer, const sp<FileTypeBox>& ftyp) {
    Box::parse(buffer, ftyp);
    UInt32 count = buffer->size() / 4;
    while (count--) {
        UInt32 id = buffer->rb32();
        DEBUGV("box %s: %" PRIu32, Name.c_str(), id);
        track_IDs.push(id);
    }
    return kMediaNoError;
}
void TrackReferenceTypeBox::compose(sp<ABuffer>& buffer, const sp<FileTypeBox>& ftyp) { }

// ISO-639-2/T language code
static inline String languageCode(const sp<ABuffer>& buffer) {
    buffer->skip(1);
    Char lang[3];
    // Each character is packed as the difference between its ASCII value and 0x60
    for (UInt32 i = 0; i < 3; i++) lang[i] = buffer->read(5) + 0x60;
    return String(&lang[0], 3);
}

MediaError MediaHeaderBox::parse(const sp<ABuffer>& buffer, const sp<FileTypeBox>& ftyp) {
    Box::parse(buffer, ftyp);
    if (Version == 1) {
        creation_time           = buffer->rb64();
        modification_time       = buffer->rb64();
        timescale               = buffer->rb32();
        duration                = buffer->rb64();
    } else {
        creation_time           = buffer->rb32();
        modification_time       = buffer->rb32();
        timescale               = buffer->rb32();
        duration                = buffer->rb32();
    }

    DEBUGV("box %s: creation %" PRIu64 ", modifcation %" PRIu64 
            ", timescale %" PRIu32 ", duration %" PRIu64,
            Name.c_str(), creation_time, modification_time, timescale, duration);

    language = languageCode(buffer);
    DEBUGV("box %s: lang %s", Name.c_str(), language.c_str());

    buffer->skip(16);    // pre_defined
    return kMediaNoError;
}
void MediaHeaderBox::compose(sp<ABuffer>& buffer, const sp<FileTypeBox>& ftyp) { }

MediaError HandlerBox::parse(const sp<ABuffer>& buffer, const sp<FileTypeBox>& ftyp) {
    Box::parse(buffer, ftyp);
    CHECK_GE(buffer->size(), 20 + 1);

    buffer->skip(32); // pre_defined
    handler_type    = buffer->rb32();
    buffer->skip(32 * 3); // reserved
    handler_name    = buffer->rs(buffer->size());

    DEBUGV("box %s: type %s name %s", Name.c_str(),
            BOXNAME(handler_type), handler_name.c_str());
    return kMediaNoError;
}
void HandlerBox::compose(sp<ABuffer>& buffer, const sp<FileTypeBox>& ftyp) { }

MediaError VideoMediaHeaderBox::parse(const sp<ABuffer>& buffer, const sp<FileTypeBox>& ftyp) {
    Box::parse(buffer, ftyp);
    graphicsmode        = buffer->rb16();
    DEBUGV("box %s: graphicsmode %" PRIu16, 
            Name.c_str(), graphicsmode);
    for (UInt32 i = 0; i < 3; i++) {
        UInt16 color = buffer->rb16();
        DEBUGV("box %s: color %" PRIu16, Name.c_str(), color);
        opcolor.push(color);
    }
    return kMediaNoError;
}
void VideoMediaHeaderBox::compose(sp<ABuffer>& buffer, const sp<FileTypeBox>& ftyp) { }

MediaError SoundMediaHeaderBox::parse(const sp<ABuffer>& buffer, const sp<FileTypeBox>& ftyp) {
    Box::parse(buffer, ftyp);
    balance         = buffer->rb16();
    buffer->skip(16);    // reserved

    DEBUGV("box %s: balance %" PRIu16, Name.c_str(), balance);
    return kMediaNoError;
}
void SoundMediaHeaderBox::compose(sp<ABuffer>& buffer, const sp<FileTypeBox>& ftyp) { }

MediaError HintMediaHeaderBox::parse(const sp<ABuffer>& buffer, const sp<FileTypeBox>& ftyp) {
    Box::parse(buffer, ftyp);
    maxPDUsize          = buffer->rb16();
    avgPDUsize          = buffer->rb16();
    maxbitrate          = buffer->rb32();
    avgbitrate          = buffer->rb32();
    buffer->skip(32);

    DEBUGV("box %s: maxPDUsize %" PRIu16 ", avgPDUsize %" PRIu16 
            ", maxbitrate %" PRIu32 ", avgbitrate %" PRIu32,
            Name.c_str(), maxPDUsize, avgPDUsize, maxbitrate, avgbitrate);
    return kMediaNoError;
}
void HintMediaHeaderBox::compose(sp<ABuffer>& buffer, const sp<FileTypeBox>& ftyp) { }

MediaError DataEntryUrlBox::parse(const sp<ABuffer>& buffer, const sp<FileTypeBox>& ftyp) {
    Box::parse(buffer, ftyp);
    if (buffer->size() > 0) {
        location    = buffer->rs(buffer->size());
        DEBUGV("box %s: location %s", Name.c_str(), location.c_str());
    }
    return kMediaNoError;
}
void DataEntryUrlBox::compose(sp<ABuffer>& buffer, const sp<FileTypeBox>& ftyp) { }

MediaError DataEntryUrnBox::parse(const sp<ABuffer>& buffer, const sp<FileTypeBox>& ftyp) {
    Box::parse(buffer, ftyp);
    Char c;
    while ((c = (Char)buffer->r8()) != '\0') urntype.append(String(c));
    location        = buffer->rs(buffer->size());

    DEBUGV("box %s: name %s location %s", 
            Name.c_str(), urntype.c_str(), location.c_str());
    return kMediaNoError;
}
void DataEntryUrnBox::compose(sp<ABuffer>& buffer, const sp<FileTypeBox>& ftyp) { }

MediaError TimeToSampleBox::parse(const sp<ABuffer>& buffer, const sp<FileTypeBox>& ftyp) {
    Box::parse(buffer, ftyp);
    UInt32 count  = buffer->rb32();
    for (UInt32 i = 0; i < count; i++) {
        Entry e = { buffer->rb32(), buffer->rb32() };
        DEBUGV("box %s: entry %" PRIu32 " %" PRIu32, 
                Name.c_str(), e.sample_count, e.sample_delta);
        entries.push(e);
    }
    return kMediaNoError;
}

void TimeToSampleBox::compose(sp<ABuffer>& buffer, const sp<FileTypeBox>& ftyp) { }

MediaError CompositionOffsetBox::parse(const sp<ABuffer>& buffer, const sp<FileTypeBox>& ftyp) {
    Box::parse(buffer, ftyp);
    UInt32 count          = buffer->rb32();
    for (UInt32 i = 0; i < count; i++) {
        // Version 0, sample_offset is UInt32
        // Version 1, sample_offset is Int32
        // some writer ignore this rule, always write in Int32
        // and sample_offset will no be very big,
        // so it is ok to always read sample_offset as Int32
        Entry e = { buffer->rb32(), (Int32)buffer->rb32() };
        DEBUGV("box %s: entry %" PRIu32 " %" PRIu32,
                Name.c_str(), e.sample_count, e.sample_offset);
        entries.push(e);
    }
    return kMediaNoError;
}
void CompositionOffsetBox::compose(sp<ABuffer>& buffer, const sp<FileTypeBox>& ftyp) { }

MediaError CompositionToDecodeBox::parse(const sp<ABuffer>& buffer, const sp<FileTypeBox>& ftyp) {
    FullBox::parse(buffer, ftyp);
    if (Version == 0) {
        compositionToDTSShift           = buffer->rb32();
        leastDecodeToDisplayDelta       = buffer->rb32();
        greatestDecodeToDisplayDelta    = buffer->rb32();
        compositionStartTime            = buffer->rb32();
        compositionEndTime              = buffer->rb32();
    } else {
        compositionToDTSShift           = buffer->rb64();
        leastDecodeToDisplayDelta       = buffer->rb64();
        greatestDecodeToDisplayDelta    = buffer->rb64();
        compositionStartTime            = buffer->rb64();
        compositionEndTime              = buffer->rb64();
    }
    return kMediaNoError;
}
void CompositionToDecodeBox::compose(sp<ABuffer>& buffer, const sp<FileTypeBox>& ftyp) { }

MediaError SampleDependencyTypeBox::parse(const sp<ABuffer>& buffer, const sp<FileTypeBox>& ftyp) {
    Box::parse(buffer, ftyp);
    while (buffer->size()) {
        dependency.push(buffer->r8());
    }
    return kMediaNoError;
}
void SampleDependencyTypeBox::compose(sp<ABuffer>& buffer, const sp<FileTypeBox>& ftyp) { }

// AudioSampleEntry is different for isom and mov
MediaError SampleEntry::parse(const sp<ABuffer>& buffer, const sp<FileTypeBox>& ftyp) {
    const UInt32 offset = buffer->offset();
    Box::parse(buffer, ftyp);
    _parse(buffer, ftyp);
    ContainerBox::_parse(buffer, ftyp);
    DEBUGV("box %s: child.size() = %zu", Name.c_str(), child.size());
    return kMediaNoError;
}
MediaError SampleEntry::_parse(const sp<ABuffer>& buffer, const sp<FileTypeBox>& ftyp) {
    // 8 bytes
    buffer->skip(8 * 6);
    data_reference_index    = buffer->rb16();

    if (media_type == kMediaTypeVideo) {
        // 70 bytes
        buffer->skip(16);
        buffer->skip(16);
        buffer->skip(32 * 3);
        visual.width            = buffer->rb16();
        visual.height           = buffer->rb16();
        visual.horizresolution  = buffer->rb32();
        visual.vertresolution   = buffer->rb32();
        buffer->skip(32);
        visual.frame_count      = buffer->rb16();
        compressorname          = buffer->rs(32);
        visual.depth            = buffer->rb16();
        buffer->skip(16);

        DEBUGV("box %s: width %" PRIu16 ", height %" PRIu16 
                ", horizresolution %" PRIu32 ", vertresolution %" PRIu32
                ", frame_count %" PRIu16 ", compressorname %s"
                ", depth %" PRIu16, 
                Name.c_str(),
                visual.width, visual.height, 
                visual.horizresolution, visual.vertresolution,
                visual.frame_count, 
                compressorname.c_str(), 
                visual.depth);
    } else if (media_type == kMediaTypeSound) {
        // 20 bytes
        sound.version           = buffer->rb16();       // mov
        buffer->skip(16);           // revision level
        buffer->skip(32);           // vendor
        sound.channelcount      = buffer->rb16();
        sound.samplesize        = buffer->rb16();
        sound.compressionID     = buffer->rb16();       // mov
        sound.packetsize        = buffer->rb16();       // mov
        sound.samplerate        = buffer->rb32() >> 16;
        DEBUGV("box %s: channelcount %" PRIu16 ", samplesize %" PRIu16
                ", samplerate %" PRIu32,
                Name.c_str(),
                sound.channelcount, 
                sound.samplesize, 
                sound.samplerate);

        if (isQuickTime(ftyp)) {
            // qtff.pdf Section "Sound Sample Description (Version 1)"
            // Page 120
            // 16 bytes
            sound.mov           = True;
            if (sound.version == 1) {
                sound.samplesPerPacket  = buffer->rb32();
                sound.bytesPerPacket    = buffer->rb32();
                sound.bytesPerFrame     = buffer->rb32();
                sound.bytesPerSample    = buffer->rb32();

                DEBUGV("box %s: samplesPerPacket %" PRIu32
                        ", bytesPerPacket %" PRIu32
                        ", bytesPerFrame %" PRIu32
                        ", bytesPerSample %" PRIu32,
                        Name.c_str(),
                        sound.samplesPerPacket,
                        sound.bytesPerPacket,
                        sound.bytesPerFrame,
                        sound.bytesPerSample);
            } else {
                CHECK_EQ(Version, 0);
            }
        } else {
            sound.mov           = False;
        }
    } else if (media_type == kMediaTypeHint) {
        // NOTHING
    } else {
        // NOTHING
    }
    return kMediaNoError;
}
void SampleEntry::compose(sp<ABuffer>& buffer, const sp<FileTypeBox>& ftyp) { }

MediaError RollRecoveryEntry::parse(const sp<ABuffer>& buffer, const sp<FileTypeBox>& ftyp) {
    roll_distance   = buffer->rb16();
    return kMediaNoError;
}

MediaError SampleGroupDescriptionBox::parse(const sp<ABuffer>& buffer, const sp<FileTypeBox>& ftyp) {
    FullBox::parse(buffer, ftyp);
    grouping_type           = buffer->rb32();
    if (Version == 1)
        default_length      = buffer->rb32();
    else if (Version >= 2)
        default_sample_description_index = buffer->rb32();
    UInt32 entry_count    = buffer->rb32();
    INFO("grouping_type %4s, entry_count %" PRIu32,
            (const Char *)&grouping_type, entry_count);
    for (UInt32 i = 0; i < entry_count; ++i) {
        if (Version == 1 && default_length == 0) {
            UInt32 description_length = buffer->rb32();
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
        if (entry.isNil()) {
            ERROR("unknown entry %4s", (const Char *)&grouping_type);
            break;
        }
        if (entry->parse(buffer, ftyp) != kMediaNoError) {
            ERROR("entry parse failed");
            break;
        }
        entries.push(entry);
    }
    return kMediaNoError;
}
void SampleGroupDescriptionBox::compose(sp<ABuffer>& buffer, const sp<FileTypeBox>& ftyp) { }

MediaError ALACAudioSampleEntry::parse(const sp<ABuffer>& buffer, const sp<FileTypeBox>& ftyp) {
    const UInt32 offset = buffer->offset();
    Box::parse(buffer, ftyp);
    SampleEntry::_parse(buffer, ftyp);
    extra = buffer->readBytes(buffer->size());
    DEBUG("box %s: %s", Name.c_str(), extra->string(True).c_str());
    return kMediaNoError;
}
void ALACAudioSampleEntry::compose(sp<ABuffer>& buffer, const sp<FileTypeBox>& ftyp) { }

MediaError SamplingRateBox::parse(const sp<ABuffer>& buffer, const sp<FileTypeBox>& ftyp) {
    Box::parse(buffer, ftyp);
    sampling_rate   = buffer->rb32();
    return kMediaNoError;
}
void SamplingRateBox::compose(sp<ABuffer>& buffer, const sp<FileTypeBox>& ftyp) { }

MediaError CommonBox::parse(const sp<ABuffer>& buffer, const sp<FileTypeBox>& ftyp) {
    Box::parse(buffer, ftyp);
    data = buffer->readBytes(buffer->size());
    DEBUG("box %s: %s", Name.c_str(), data->string(True).c_str());
    return kMediaNoError;
}
void CommonBox::compose(sp<ABuffer>& buffer, const sp<FileTypeBox>& ftyp) { }

MediaError BitRateBox::parse(const sp<ABuffer>& buffer, const sp<FileTypeBox>& ftyp) {
    Box::parse(buffer, ftyp);
    bufferSizeDB        = buffer->rb32();
    maxBitrate          = buffer->rb32();
    avgBitrate          = buffer->rb32();
    return kMediaNoError;
}
void BitRateBox::compose(sp<ABuffer>& buffer, const sp<FileTypeBox>& ftyp) { }

MediaError SampleSizeBox::parse(const sp<ABuffer>& buffer, const sp<FileTypeBox>& ftyp) {
    Box::parse(buffer, ftyp);
    if (Type == kBoxTypeSTZ2) {
        buffer->skip(24);            //reserved
        UInt8 field_size      = buffer->r8();
        sample_size             = 0; // always 0
        UInt32 sample_count     = buffer->rb32();
        for (UInt32 i = 0; i < sample_count; i++) {
            entries.push(buffer->read(field_size));
        }
    } else {
        sample_size             = buffer->rb32();
        UInt32 sample_count     = buffer->rb32();
        if (sample_size == 0) {
            for (UInt32 i = 0; i < sample_count; i++) {
                entries.push(buffer->rb32());
            }
        } else {
            DEBUGV("box %s: fixed sample size %" PRIu32, 
                    Name.c_str(), sample_size);
        }
    }

#if 1
    for (UInt32 i = 0; i < entries.size(); ++i) {
        DEBUGV("box %s: %" PRIu64, Name.c_str(), entries[i]);
    }
#endif 
    return kMediaNoError;
}
void SampleSizeBox::compose(sp<ABuffer>& buffer, const sp<FileTypeBox>& ftyp) { }

MediaError SampleToChunkBox::parse(const sp<ABuffer>& buffer, const sp<FileTypeBox>& ftyp) {
    Box::parse(buffer, ftyp);
    UInt32 count          = buffer->rb32();
    for (UInt32 i = 0; i < count; i++) {
        Entry e;
        e.first_chunk       = buffer->rb32();
        e.samples_per_chunk = buffer->rb32();
        e.sample_description_index  = buffer->rb32();

        DEBUGV("box %s: %" PRIu32 ", %" PRIu32 ", %" PRIu32, 
                Name.c_str(), e.first_chunk, e.samples_per_chunk, e.sample_description_index);

        entries.push(e);
    }
    return kMediaNoError;
}
void SampleToChunkBox::compose(sp<ABuffer>& buffer, const sp<FileTypeBox>& ftyp) { }

MediaError ChunkOffsetBox::parse(const sp<ABuffer>& buffer, const sp<FileTypeBox>& ftyp) {
    Box::parse(buffer, ftyp);
    UInt32 count          = buffer->rb32();
    if (Type == kBoxTypeCO64) { // offset64
        for (UInt32 i = 0; i < count; ++i) {
            UInt64 e          = buffer->rb64();
            DEBUGV("box %s: %" PRIu64, Name.c_str(), e);
            entries.push(e);
        }
    } else {
        for (UInt32 i = 0; i < count; ++i) {
            UInt32 e          = buffer->rb32();
            DEBUGV("box %s: %" PRIu32, Name.c_str(), e);
            entries.push(e);
        }
    }
    return kMediaNoError;
}
void ChunkOffsetBox::compose(sp<ABuffer>& buffer, const sp<FileTypeBox>& ftyp) { }

MediaError SyncSampleBox::parse(const sp<ABuffer>& buffer, const sp<FileTypeBox>& ftyp) {
    Box::parse(buffer, ftyp);
    UInt32 count      = buffer->rb32();
    for (UInt32 i = 0; i < count; i++) {
        UInt32 e      = buffer->rb32();
        DEBUGV("box %s: %" PRIu32, Name.c_str(), e);
        entries.push(e);
    }
    return kMediaNoError;
}
void SyncSampleBox::compose(sp<ABuffer>& buffer, const sp<FileTypeBox>& ftyp) { }

MediaError ShadowSyncSampleBox::parse(const sp<ABuffer>& buffer, const sp<FileTypeBox>& ftyp) {
    Box::parse(buffer, ftyp);
    UInt32 count              = buffer->rb32();
    for (UInt32 i = 0; i < count; ++i) {
        Entry e;
        e.shadowed_sample_number    = buffer->rb32();
        e.sync_sample_number        = buffer->rb32();

        DEBUGV("box %s: %" PRIu32 " %" PRIu32, 
                Name.c_str(), e.shadowed_sample_number, e.sync_sample_number);
        entries.push(e);
    }
    return kMediaNoError;
}
void ShadowSyncSampleBox::compose(sp<ABuffer>& buffer, const sp<FileTypeBox>& ftyp) { }

MediaError DegradationPriorityBox::parse(const sp<ABuffer>& buffer, const sp<FileTypeBox>& ftyp) {
    Box::parse(buffer, ftyp);
    UInt32 count = buffer->size() / 2;
    while (count--) { entries.push(buffer->rb16()); }
    return kMediaNoError;
}
void DegradationPriorityBox::compose(sp<ABuffer>& buffer, const sp<FileTypeBox>& ftyp) { }

MediaError PaddingBitsBox::parse(const sp<ABuffer>& buffer, const sp<FileTypeBox>& ftyp) {
    Box::parse(buffer, ftyp);
    UInt32 count  = buffer->rb32();
    for (UInt32 i = 0; i < (count + 1) / 2; i++) {
        Entry e;
        buffer->skip(1);
        e.pad1  = buffer->read(3);
        buffer->skip(1);
        e.pad2  = buffer->read(3);
        entries.push(e);
    }
    return kMediaNoError;
}
void PaddingBitsBox::compose(sp<ABuffer>& buffer, const sp<FileTypeBox>& ftyp) { }

MediaError FreeSpaceBox::parse(const sp<ABuffer>& buffer, const sp<FileTypeBox>& ftyp) {
    Box::parse(buffer, ftyp);
    return kMediaNoError;
}
void FreeSpaceBox::compose(sp<ABuffer>& buffer, const sp<FileTypeBox>& ftyp) { }

MediaError EditListBox::parse(const sp<ABuffer>& buffer, const sp<FileTypeBox>& ftyp) {
    Box::parse(buffer, ftyp);
    UInt32 count = buffer->rb32();
    for (UInt32 i = 0; i < count; i++) {
        Entry e;
        if (Version == 1) {
            e.segment_duration    = buffer->rb64();
            e.media_time          = buffer->rb64();
        } else {
            e.segment_duration    = buffer->rb32();
            e.media_time          = buffer->rb32();
        }
        e.media_rate_integer      = buffer->rb16();
        e.media_rate_fraction     = buffer->rb16();
        DEBUGV("box %s: %" PRIu64 ", %" PRId64 ", %" PRIu16 ", %" PRIu16, 
                Name.c_str(), e.segment_duration, e.media_time,
                e.media_rate_integer, e.media_rate_fraction);
    };
    return kMediaNoError;
}
void EditListBox::compose(sp<ABuffer>& buffer, const sp<FileTypeBox>& ftyp) { }

MediaError NoticeBox::parse(const sp<ABuffer>& buffer, const sp<FileTypeBox>& ftyp) {
    Box::parse(buffer, ftyp);
    language    = languageCode(buffer);
    // maybe empty
    if (buffer->size()) {
        value       = buffer->rs(buffer->size());
    }
    DEBUGV("box %s: value = %s", Name.c_str(), value.c_str());
    return kMediaNoError;
}
void NoticeBox::compose(sp<ABuffer>& buffer, const sp<FileTypeBox>& ftyp) { }

MediaError MovieExtendsHeaderBox::parse(const sp<ABuffer>& buffer, const sp<FileTypeBox>& ftyp) {
    Box::parse(buffer, ftyp);
    if (Version == 1) {
        fragment_duration   = buffer->rb64();
    } else {
        fragment_duration   = buffer->rb32();
    }
    return kMediaNoError;
}
void MovieExtendsHeaderBox::compose(sp<ABuffer>& buffer, const sp<FileTypeBox>& ftyp) { }

MediaError TrackExtendsBox::parse(const sp<ABuffer>& buffer, const sp<FileTypeBox>& ftyp) {
    Box::parse(buffer, ftyp);
    track_ID                        = buffer->rb32();
    default_sample_description_index    = buffer->rb32();
    default_sample_duration         = buffer->rb32();
    default_sample_size             = buffer->rb32();
    default_sample_flags            = buffer->rb32();
    return kMediaNoError;
}
void TrackExtendsBox::compose(sp<ABuffer>& buffer, const sp<FileTypeBox>& ftyp) { }

MediaError MovieFragmentHeaderBox::parse(const sp<ABuffer>& buffer, const sp<FileTypeBox>& ftyp) {
    Box::parse(buffer, ftyp);
    sequence_number         = buffer->rb32();
    return kMediaNoError;
}
void MovieFragmentHeaderBox::compose(sp<ABuffer>& buffer, const sp<FileTypeBox>& ftyp) { }

MediaError TrackFragmentHeaderBox::parse(const sp<ABuffer>& buffer, const sp<FileTypeBox>& ftyp) {
    Box::parse(buffer, ftyp);
    track_ID    = buffer->rb32();
    if (Flags & 0x1)    base_data_offset            = buffer->rb32();
    if (Flags & 0x2)    sample_description_index    = buffer->rb64();
    if (Flags & 0x8)    default_sample_duration     = buffer->rb32();
    if (Flags & 0x10)   default_sample_size         = buffer->rb32();
    if (Flags & 0x20)   default_sample_flags        = buffer->rb32();
    return kMediaNoError;
}
void TrackFragmentHeaderBox::compose(sp<ABuffer>& buffer, const sp<FileTypeBox>& ftyp) { }

MediaError PrimaryItemBox::parse(const sp<ABuffer>& buffer, const sp<FileTypeBox>& ftyp) {
    Box::parse(buffer, ftyp);
    item_ID     = buffer->rb16();
    return kMediaNoError;
}
void PrimaryItemBox::compose(sp<ABuffer>& buffer, const sp<FileTypeBox>& ftyp) { }

MediaError ColourInformationBox::parse(const sp<ABuffer>& buffer, const sp<FileTypeBox>& ftyp) {
    Box::parse(buffer, ftyp);
    colour_type     = buffer->rb32();
    if (colour_type == kColourTypeNCLX) {
        colour_primaries            = buffer->rb16();
        transfer_characteristics    = buffer->rb16();
        matrix_coefficients         = buffer->rb16();
        full_range_flag             = buffer->read(1);
        buffer->skip(7);
    } else if (colour_type == kColourTypeNCLC) {     // mov
        colour_primaries            = buffer->rb16();
        transfer_characteristics    = buffer->rb16();
        matrix_coefficients         = buffer->rb16();
    } else {
        // ICC_profile
        ERROR("box %s: TODO ICC_profile", Name.c_str());
    }
    return kMediaNoError;
}
void ColourInformationBox::compose(sp<ABuffer>& buffer, const sp<FileTypeBox>& ftyp) { }

#if 1
MediaError siDecompressionParam::parse(const sp<ABuffer>& buffer, const sp<FileTypeBox>& ftyp) {
    Box::parse(buffer, ftyp);
    return ContainerBox::_parse(buffer, ftyp);
}
void siDecompressionParam::compose(sp<ABuffer>& buffer, const sp<FileTypeBox>& ftyp) { }
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
MediaError iTunesHeaderBox::parse(const sp<ABuffer>& buffer, const sp<FileTypeBox>& ftyp) {
    Box::parse(buffer, ftyp);
    nextItemID  = buffer->rb32();
    return kMediaNoError;
}
void iTunesHeaderBox::compose(sp<ABuffer>& buffer, const sp<FileTypeBox>& ftyp) { }

MediaError CountryListBox::parse(const sp<ABuffer>& buffer, const sp<FileTypeBox>& ftyp) {
    Box::parse(buffer, ftyp);
    UInt32 Entry_count    = buffer->rb32();
    UInt16 Country_count  = buffer->rb16();
    for (UInt32 i = 0; i < Country_count; ++i) {
        Entry e;
        for (UInt32 j = 0; j < Entry_count; ++j) {
            e.Countries.push(buffer->rb16());
        }
        entries.push(e);
    }
    return kMediaNoError;
}
void CountryListBox::compose(sp<ABuffer>& buffer, const sp<FileTypeBox>& ftyp) { }

MediaError LanguageListBox::parse(const sp<ABuffer>& buffer, const sp<FileTypeBox>& ftyp) {
    Box::parse(buffer, ftyp);
    UInt32 Entry_count    = buffer->rb32();
    UInt16 Language_count = buffer->rb16();
    for (UInt32 i = 0; i < Language_count; ++i) {
        Entry e;
        for (UInt32 j = 0; j < Entry_count; ++j) {
            e.Languages.push(buffer->rb16());
        }
        entries.push(e);
    }
    return kMediaNoError;
}
void LanguageListBox::compose(sp<ABuffer>& buffer, const sp<FileTypeBox>& ftyp) { }

MediaError iTunesStringBox::parse(const sp<ABuffer>& buffer, const sp<FileTypeBox>& ftyp) {
    Box::parse(buffer, ftyp);
    value       = buffer->rs(buffer->size());
    DEBUGV("box %s: %s", Name.c_str(), value.c_str());
    return kMediaNoError;
}
void iTunesStringBox::compose(sp<ABuffer>& buffer, const sp<FileTypeBox>& ftyp) { }

MediaError iTunesDataBox::parse(const sp<ABuffer>& buffer, const sp<FileTypeBox>& ftyp) {
    Box::parse(buffer, ftyp);
    // 8 bytes
    CHECK_EQ(buffer->r8(), 0);            // type indicator byte
    Type_indicator      = buffer->rb24();
    Country_indicator   = buffer->rb16();
    Language_indicator  = buffer->rb16();
    DEBUGV("box %s: [%" PRIu32 "] [%" PRIx16 "-%" PRIx16 "]",
            Name.c_str(), Type_indicator, Country_indicator, Language_indicator);
    if (buffer->size()) {
        Value           = buffer->readBytes(buffer->size());
        DEBUGV("box %s: %s", Name.c_str(), Value->string().c_str());
    }
    return kMediaNoError;
}
void iTunesDataBox::compose(sp<ABuffer>& buffer, const sp<FileTypeBox>& ftyp) { }

MediaError iTunesInfomationBox::parse(const sp<ABuffer>& buffer, const sp<FileTypeBox>& ftyp) {
    Box::parse(buffer, ftyp);
    Item_ID     = buffer->rb32();
    return kMediaNoError;
}
void iTunesInfomationBox::compose(sp<ABuffer>& buffer, const sp<FileTypeBox>& ftyp) { }

MediaError iTunesNameBox::parse(const sp<ABuffer>& buffer, const sp<FileTypeBox>& ftyp) {
    Box::parse(buffer, ftyp);
    Name = buffer->rs(buffer->size());
    DEBUGV("box %s: %s", Name.c_str(), Name.c_str());
    return kMediaNoError;
}
void iTunesNameBox::compose(sp<ABuffer>& buffer, const sp<FileTypeBox>& ftyp) { }

MediaError iTunesMeanBox::parse(const sp<ABuffer>& buffer, const sp<FileTypeBox>& ftyp) {
    Box::parse(buffer, ftyp);
    Mean = buffer->rs(buffer->size());
    DEBUGV("box %s: %s", Name.c_str(), Mean.c_str());
    return kMediaNoError;
}
void iTunesMeanBox::compose(sp<ABuffer>& buffer, const sp<FileTypeBox>& ftyp) { }

#if 1
MediaError iTunesItemListBox::parse(const sp<ABuffer>& buffer, const sp<FileTypeBox>& ftyp) {
    Box::parse(buffer, ftyp);
    ContainerBox::_parse(buffer, ftyp);
}
void iTunesItemListBox::compose(sp<ABuffer>& buffer, const sp<FileTypeBox>& ftyp) { }
#endif

MediaError iTunesKeyDecBox::parse(const sp<ABuffer>& buffer, const sp<FileTypeBox>& ftyp) {
    Box::parse(buffer, ftyp);
    Keytypespace    = buffer->rb32();
    Key_value       = buffer->readBytes(buffer->size());
    DEBUG("box %s: %s\n%s", 
            Name.c_str(), String(Keytypespace).c_str(), Key_value->string().c_str());
    return kMediaNoError;
}
void iTunesKeyDecBox::compose(sp<ABuffer>& buffer, const sp<FileTypeBox>& ftyp) { }

///////////////////////////////////////////////////////////////////////////
// ID3v2
MediaError ID3v2Box::parse(const sp<ABuffer>& buffer, const sp<FileTypeBox>& ftyp) {
    Box::parse(buffer, ftyp);
    language    = languageCode(buffer);
    ID3v2data   = buffer->readBytes(buffer->size());
    return kMediaNoError;
}
void ID3v2Box::compose(sp<ABuffer>& buffer, const sp<FileTypeBox>& ftyp) { }

MediaError ObjectDescriptorBox::parse(const sp<ABuffer>& buffer, const sp<FileTypeBox>& ftyp) {
    Box::parse(buffer, ftyp);
    iods = buffer->readBytes(buffer->size());
    DEBUGV("box %s: %s", Name.c_str(), iods->string().c_str());
    return kMediaNoError;
}
void ObjectDescriptorBox::compose(sp<ABuffer>& buffer, const sp<FileTypeBox>& ftyp) { }

///////////////////////////////////////////////////////////////////////////
template <class BoxType> static sp<Box> Create() { return new BoxType; }
#define RegisterBox(NAME, BoxType) \
    static RegisterHelper Reg##BoxType(NAME, Create<BoxType>)

RegisterBox(kBoxTypeFTYP, FileTypeBox);
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
    struct BoxType : public Box {                                           \
        FORCE_INLINE BoxType() : Box(NAME) { }                              \
        FORCE_INLINE virtual MediaError parse(const sp<ABuffer>& buffer,    \
                const sp<FileTypeBox>& ftyp) {                                  \
            buffer->skipBytes(buffer->size());                              \
            return kMediaNoError;                                           \
        }                                                                   \
        virtual void compose(sp<ABuffer>& buffer, const sp<FileTypeBox>& ftyp) {}     \
    };                                                                      \
RegisterBox(NAME, BoxType);
// Aperture box
IgnoreBox('tapt', TrackApertureModeDimensions);
IgnoreBox('alis', ApertureAliasDataReferenceBox);
IgnoreBox('chan', AudioChannelLayoutBox);
IgnoreBox('frma', SoundFormatBox);
IgnoreBox('mebx', TimedMetadataSampleDescriptionBox);
IgnoreBox('wide', WideBox);
IgnoreBox('uuid', UUIDBox);
#undef IgnoreBox

RegisterBox(kBoxTypeMDAT, MediaDataBox);

sp<Box> ReadBox(const sp<ABuffer>& buffer, const sp<FileTypeBox>& ftyp) {
    UInt32 boxHeadLength = 8;
    if (buffer->size() < boxHeadLength) {
        ERROR("ABuffer is too small");
        return Nil;
    }
    
    UInt32 boxSize    = buffer->rb32();
    UInt32 boxType    = buffer->rb32();
    
    if (boxSize == 1) {
        if (buffer->size() < boxHeadLength) {
            ERROR("ABuffer is too small");
            return Nil;
        }
        boxSize         = buffer->rb64();
        boxHeadLength   = 16;
    }
    
    DEBUG("found box %s %zu bytes", BOXNAME(boxType), (UInt32)boxSize);
    sp<Box> box = MakeBoxByType(boxType);
    if (box.isNil()) {
        ERROR("unknown box %s", BOXNAME(boxType));
        buffer->skipBytes(boxSize - boxHeadLength);
        return Nil;
    }
    
    if (boxType == kBoxTypeMDAT) {
        sp<MediaDataBox> mdat = box;
        mdat->offset = buffer->offset();
        mdat->length = boxSize - boxHeadLength;
        return mdat;
    }
    
    if (boxSize == boxHeadLength) {
        INFO("box %s: empty box", box->Name.c_str());
        return box;
    }
    
    sp<ABuffer> boxData = buffer->readBytes(boxSize - boxHeadLength);
        
    if (box->parse(boxData, ftyp) != kMediaNoError) {
        ERROR("box %s: parse failed.", box->Name.c_str());
        return Nil;
    }
    return box;
}

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
Bool CheckTrackBox(const sp<TrackBox>& trak) {
    if (!(trak->Class & kBoxContainer)) return False;

    sp<TrackHeaderBox> tkhd = FindBox(trak, kBoxTypeTKHD);
    if (tkhd == Nil) return False;

    sp<MediaBox> mdia = FindBox(trak, kBoxTypeMDIA);
    if (mdia == Nil) return False;

    sp<MediaHeaderBox> mdhd = FindBox(mdia, kBoxTypeMDHD);
    if (mdhd == Nil) return False;

    sp<MediaInformationBox> minf = FindBox(mdia, kBoxTypeMINF);
    if (minf == Nil) return False;

    sp<DataInformationBox> dinf = FindBox(minf, kBoxTypeDINF);
    if (dinf == Nil) return False;

    sp<DataReferenceBox> dref = FindBox(dinf, kBoxTypeDREF);
    if (dref == Nil) return False;

    sp<SampleTableBox> stbl = FindBox(minf, kBoxTypeSTBL);
    if (stbl == Nil) return False;

    sp<SampleDescriptionBox> stsd = FindBox(stbl, kBoxTypeSTSD);
    if (stsd == Nil) return False;

    sp<TimeToSampleBox> stts    = FindBox(stbl, kBoxTypeSTTS);
    sp<SampleToChunkBox> stsc   = FindBox(stbl, kBoxTypeSTSC);
    sp<ChunkOffsetBox> stco     = FindBox2(stbl, kBoxTypeSTCO, kBoxTypeCO64);
    sp<SampleSizeBox> stsz      = FindBox2(stbl, kBoxTypeSTSZ, kBoxTypeSTZ2);

    if (stts == Nil || stsc == Nil || stco == Nil || stsz == Nil) {
        return False;
    }

    return True;
}

// find box in current container only
sp<Box> FindBox(const sp<ContainerBox>& root, UInt32 boxType, UInt32 index) {
    for (UInt32 i = 0; i < root->child.size(); i++) {
        sp<Box> box = root->child[i];
        if (box->Type == boxType) {
            if (index == 0) return box;
            else --index;
        }
    }
    return Nil;
}

sp<Box> FindBox2(const sp<ContainerBox>& root, UInt32 first, UInt32 second) {
    for (UInt32 i = 0; i < root->child.size(); i++) {
        sp<Box> box = root->child[i];
        if (box->Type == first || box->Type == second) {
            return box;
        }
    }
    return Nil;
}

sp<Box> FindBoxInside(const sp<ContainerBox>& root, UInt32 sub, UInt32 target) {
    sp<Box> box = FindBox(root, sub);
    if (box == 0) return Nil;
    return FindBox(box, target);
}

static void PrintBox(const sp<Box>& box, UInt32 n) {
    String line;
    if (n) {
        for (UInt32 i = 0; i < n - 2; ++i) line.append(" ");
        line.append("|- ");
    }
    line.append(box->Name.c_str());
    INFO("%s", line.c_str());
    n += 2;
    if (box->Class & kBoxContainer) {
        sp<ContainerBox> c = box;
        for (UInt32 i = 0; i < c->child.size(); ++i) {
            PrintBox(c->child[i], n);
        }
    }
}
void PrintBox(const sp<Box>& root) {
    PrintBox(root, 0);
}

__END_NAMESPACE(MPEG4)
__END_NAMESPACE_MPX
