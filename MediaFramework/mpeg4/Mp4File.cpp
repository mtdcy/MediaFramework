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


// File:    Mp4File.cpp
// Author:  mtdcy.chen
// Changes: 
//          1. 20160701     initial version
//

#define LOG_TAG   "Mp4File"
//#define LOG_NDEBUG 0
#include <MediaToolkit/Toolkit.h>

#include "Systems.h"
#include "tags/id3/ID3.h"
#include "Box.h"
#include <MediaFramework/MediaTime.h>

// reference: 
// ISO/IEC 14496-12: ISO base media file format
// ISO/IEC 14496-14: mp4 file format
// http://atomicparsley.sourceforge.net/mpeg-4files.html
// http://www.mp4ra.org/atoms.html
using namespace mtdcy;
using namespace MPEG4;

struct {
    const char *        name;
    const eCodecFormat  format;
} kCodecMap[] = {
    {"mp4a",        kAudioCodecFormatAAC},
    {"avc1",        kVideoCodecFormatH264},
    {"avc2",        kVideoCodecFormatH264},
    {"hvc1",        kVideoCodecFormatHEVC},
    // END OF LIST
    {"",            kCodecFormatUnknown},
};

static eCodecFormat get_codec_format(const String& name) {
    for (size_t i = 0; kCodecMap[i].format != kCodecFormatUnknown; ++i) {
        if (name == kCodecMap[i].name)
            return kCodecMap[i].format;
    }
    return kCodecFormatUnknown;
}

// TODO:
static status_t prepareMetaData(const sp<Box>& meta, const sp<Message>& target) {
    if (meta == 0 || target == 0) {
        ERROR("bad parameters.");
        return BAD_VALUE;
    }

    sp<iTunesItemKeysBox> keys = FindBox(meta, "keys");
    sp<iTunesItemListBox> ilst = FindBox(meta, "ilst");
    if (ilst == 0) {
        ERROR("ilst is missing.");
        return NO_INIT;
    }

}

struct Sample {
    uint64_t            offset;
    size_t              size;   // in bytes
    int64_t             dts;
    int64_t             pts;
    uint32_t            flags;
};

struct Mp4Track {
    Mp4Track() : codec(kCodecFormatUnknown), trackIndex(0),
    sampleIndex(0), duration(kTimeInvalid),
    startTime(kTimeInvalid) { }

    eCodecFormat        codec;
    size_t              trackIndex;
    size_t              sampleIndex;
    MediaTime           duration;
    MediaTime           startTime;
    Vector<Sample>      sampleTable;

    union {
        struct {
            int32_t     width;
            int32_t     height;
        } video;
        struct {
            int32_t     sampleRate;
            int32_t     channelCount;
        } audio;
    };
    sp<CommonBox>       esds;
};

static sp<Mp4Track> prepareTrack(const sp<TrackBox>& trak, const sp<MovieHeaderBox>& mvhd) {
    if (CheckTrackBox(trak) == false) {
        ERROR("bad TrackBox");
        return NULL;
    }

    sp<TrackHeaderBox> tkhd             = FindBox(trak, "tkhd");
    sp<MediaBox> mdia                   = FindBox(trak, "mdia");
    sp<MediaHeaderBox> mdhd             = FindBox(mdia, "mdhd");
    sp<HandlerBox> hdlr                 = FindBox(mdia, "hdlr");
    sp<MediaInformationBox> minf        = FindBox(mdia, "minf");
    sp<DataReferenceBox> dinf           = FindBox(minf, "dinf");
    sp<SampleTableBox> stbl             = FindBox(minf, "stbl");
    sp<SampleDescriptionBox> stsd       = FindBox(stbl, "stsd");
    sp<TimeToSampleBox> stts            = FindBox(stbl, "stts");    // dts
    sp<SampleToChunkBox> stsc           = FindBox(stbl, "stsc");    // sample to chunk
    sp<ChunkOffsetBox> stco             = FindBox(stbl, "stco", "co64");    // chunk offset
    sp<SampleSizeBox> stsz              = FindBox(stbl, "stsz", "stz2");
    // optional
    sp<SyncSampleBox> stss              = FindBox(stbl, "stss");
    sp<ShadowSyncSampleBox> stsh        = FindBox(stbl, "stsh");
    sp<CompositionOffsetBox> ctts       = FindBox(stbl, "ctts");
    sp<TrackReferenceBox> tref          = FindBox(trak, "tref");
    sp<SampleDependencyTypeBox> sdtp    = FindBox(stbl, "sdtp");

    // check
    if (stsd->child.size() == 0) {
        ERROR("SampleEntry is missing from stsb.");
        return NULL;
    }

    if (stsd->child.size() > 1) {
        WARN("stsd with multi SampleEntry");
    }
    
    sp<Mp4Track> track = new Mp4Track;
    track->duration = MediaTime(mdhd->duration, mdhd->timescale);
    
    DEBUG("handler: [%s] %s", hdlr->handler_type.c_str(),
          hdlr->handler_name.c_str());
    
    // find sample infomations
    sp<SampleEntry> sampleEntry = stsd->child[0];
    track->codec = get_codec_format(sampleEntry->name);
    if (track->codec == kCodecFormatUnknown) {
        ERROR("unsupported track sample '%s'", sampleEntry->name.c_str());
        return track;
    }

    if (hdlr->handler_type == "soun") {
        track->audio.channelCount = sampleEntry->sound.channelcount;
        if (sampleEntry->sound.samplerate) {
            track->audio.sampleRate = sampleEntry->sound.samplerate;
        } else {
            ERROR("sample rate missing from sample entry box, using timescale in mdhd");
            track->audio.sampleRate = mdhd->timescale;
        }
    } else if (hdlr->handler_type == "vide") {
        track->video.width = sampleEntry->visual.width;
        track->video.height = sampleEntry->visual.height;
    }

    for (size_t i = 0; i < sampleEntry->child.size(); ++i) {
        sp<Box> box = sampleEntry->child[i];
        String knownBox = "esds avcC hvcC";
        if (knownBox.indexOf(box->name) >= 0) {
            track->esds = box;
        }
        // esds in mov.
        else if (box->name == "wave") {
            track->esds = FindBox(box, "esds");
        } else {
            INFO("ignore box %s", box->name.c_str());
        }
    }

    // FIXME: no-output sample
    const uint64_t now = SystemTimeUs();
    uint64_t dts = 0;
    for (size_t i = 0; i < stts->entries.size(); ++i) {
        for (size_t j = 0; j < stts->entries[i].sample_count; ++j) {
            dts += stts->entries[i].sample_delta;
            Sample s = { 0/*offset*/, 0/*size*/,
                dts, kTimeValueInvalid/*pts*/,
                stss != NULL ? kFrameFlagNone : kFrameFlagSync};
            track->sampleTable.push(s);
        }
    }

    // ctts => pts
    if (ctts != 0) {
        size_t sampleIndex = 0;
        for (size_t i = 0; i < ctts->entries.size(); ++i) {
            for (size_t j = 0; j < ctts->entries[i].sample_count; ++j) {
                Sample& s = track->sampleTable[sampleIndex++];
                s.pts = s.dts + ctts->entries[i].sample_offset;
            }
        }
    } else if (hdlr->handler_type == "soun") {
        // for audio, pts == dts
        for (size_t i = 0; i < track->sampleTable.size(); ++i) {
            Sample& s = track->sampleTable[i];
            s.pts = s.dts;
        }
    } else {
        ERROR("ctts is not present. pts will be missing");
    }

    // stco + stsc + stsz => sample size & offset
    CHECK_EQ((size_t)stsc->entries[0].first_chunk, 1);
    size_t sampleIndex = 0;
    size_t stscIndex = 0;
    // first, go through each chunk
    for (size_t chunkIndex = 0; chunkIndex < stco->entries.size(); ++chunkIndex) {
        // find out how many samples in this chunk
        while (stscIndex + 1 < stsc->entries.size() &&
                stsc->entries[stscIndex + 1].first_chunk <= chunkIndex + 1) {
            ++stscIndex;
        }
        const size_t numSamples = stsc->entries[stscIndex].samples_per_chunk;

        // set each samples offset and size
        uint64_t offset = stco->entries[chunkIndex];
        for (size_t i = 0; i < numSamples; ++i && ++sampleIndex) {
            Sample& s = track->sampleTable[sampleIndex];

            s.offset = offset;
            s.size = stsz->sample_size ?  stsz->sample_size : stsz->entries[sampleIndex];

            offset += s.size;
        }
    }

    // stss => key frames
    if (stss != NULL) {
        for (size_t i = 0; i < stss->entries.size(); ++i) {
            Sample& s = track->sampleTable[stss->entries[i] - 1];
            s.flags |= kFrameFlagSync;
            //INFO("sync frame %d", stss->entries[i]);
        }
        INFO("every %zu frame has one sync frame",
                track->sampleTable.size() / stss->entries.size());
    }

    // ISO/IEC 14496-12:2015 Section 8.6.4
    if (sdtp != NULL) {
        for (size_t i = 0; i < sdtp->dependency.size(); ++i) {
            uint8_t dep = sdtp->dependency[i];
            uint8_t is_leading = (dep & 0xc0) >> 6;
            uint8_t sample_depends_on = (dep & 0x30) >> 4;
            uint8_t sample_is_depended_on = (dep & 0xc) >> 2;
            uint8_t sample_has_redundancy = (dep & 0x3);
#if 1
            DEBUG("is leading %d, sample_depends_on %d, "
                    "sample_is_depended_on %d, sample_has_redundancy %d",
                    is_leading,
                    sample_depends_on,
                    sample_is_depended_on,
                    sample_has_redundancy);
#endif
            Sample& s = track->sampleTable[i];
            if (sample_depends_on == 2)     s.flags |= kFrameFlagSync;
            if (is_leading == 2)            s.flags |= kFrameFlagLeading;
            if (sample_is_depended_on == 2) s.flags |= kFrameFlagDisposal;
            if (sample_has_redundancy == 1) s.flags |= kFrameFlagRedundant;
        }
    }

#if 1
    if (tref != 0) {
        CHECK_EQ(tref->child.size(), 1);
        sp<TrackReferenceTypeBox> type = tref->child[0];
        DEBUG("tref %s", type->name.c_str());
        // TODO
    }
#endif
    
    DEBUG("init sample table takes %.2f", (SystemTimeUs() - now) / 1E6);
#if 0
    for (size_t i = 0; i < track.mSampleTable.size(); ++i) {
        Sample& s   = track.mSampleTable[i];
        DEBUG("sample %zu: %" PRIu64 " %zu %" PRIu64 " %" PRIu64,
                i, s.offset, s.size,
                s.dts, s.pts);
    }
    DEBUG("stsz size %zu", stsz->entries.size());
#endif
    DEBUG("num samples %zu total %zu", track->sampleTable.size(),
            track->sampleTable.size() * sizeof(Sample));

    return track;
}

static size_t findSampleIndex(Mp4Track& track,
        int64_t ts,
        eModeReadType mode,
        size_t *match = NULL) {
    const Vector<Sample>& tbl = track.sampleTable;

    size_t first = 0;
    size_t second = tbl.size() - 1;
    size_t mid = 0;

    // using binary search to find sample index
    // closest search
    size_t search_count = 0;
    while (first < second) {
        mid = (first + second) / 2; // truncated happens
        const Sample& s0 = tbl[mid];
        const Sample& s1 = tbl[mid + 1];
        if (s0.dts <= ts && s1.dts > ts) first = second = mid;
        else if (s0.dts > ts) second = mid;
        else first = mid;
        ++search_count;
    }

    // find sync sample index
    do {
        const Sample& s = tbl[first];
        if (s.flags & kFrameFlagSync) break;
        if (first == 0) {
            WARN("track %zu: no sync at start");
            mode = kModeReadNextSync;
            break;
        }
        --first;
    } while (first > 0);

    while (second < tbl.size()) {
        const Sample& s = tbl[second];
        if (s.flags & kFrameFlagSync) break;
        ++second;
    }
    // force last sync, if second sync pointer not exists
    if (second == tbl.size()) mode = kModeReadLastSync;

    size_t result;
    if (mode == kModeReadLastSync) {
        result = first;
    } else if (mode == kModeReadNextSync) {
        result = second;
    } else { // closest
        if (mid - first > second - mid)
            result = second;
        else
            result = first;
    }


    INFO("track %zu: seek %.3f(s) => [%zu - %zu - %zu] => %zu # %zu",
            track.trackIndex, (double)ts / track.duration.timescale,
            first, mid, second, result,
            search_count);

    if (match) *match = mid;
    return result;
}

#include <MediaFramework/MediaPacket.h>
#include <MediaFramework/MediaExtractor.h>

struct Mp4Packet : public MediaPacket {
    sp<Buffer> buffer;
    Mp4Packet(const sp<Buffer>& _buffer) : MediaPacket(), buffer(_buffer) {
        data    = (uint8_t*)buffer->data();
        size    = buffer->size();
    }
};

struct Mp4File : public MediaExtractor {
    sp<Content>             mContent;
    Vector<Mp4Track>        mTracks;
    MediaTime               mDuration;
    struct {
        size_t              offset;
        size_t              length;
    } meta;

    Mp4File() : MediaExtractor(), mContent(NULL) { }

    virtual ~Mp4File() { }

    virtual Message formats() const {
        Message info;
        info.setInt32(kKeyFormat, kFileFormatMP4);
        info.set<MediaTime>(kKeyDuration, mDuration);
        info.setInt32(kKeyCount, mTracks.size());

        for (size_t i = 0; i < mTracks.size(); ++i) {
            const Mp4Track& trak = mTracks[i];

            Message trakInfo;
            trakInfo.setInt32(kKeyFormat, trak.codec);
            trakInfo.set<MediaTime>(kKeyDuration, trak.duration);

            eCodecType type = GetCodecType(trak.codec);
            if (type == kCodecTypeAudio) {
                trakInfo.setInt32(kKeySampleRate, trak.audio.sampleRate);
                trakInfo.setInt32(kKeyChannels, trak.audio.channelCount);
            } else if (type == kCodecTypeVideo) {
                trakInfo.setInt32(kKeyWidth, trak.video.width);
                trakInfo.setInt32(kKeyHeight, trak.video.height);
            }

            if (trak.esds != NULL) {
                trakInfo.set<Buffer>(trak.esds->name, *trak.esds->data);
            }

#if 0
            // start time & encode delay/padding
            uint64_t startTime = 0;     // FIXME: learn more about start time.
            int64_t encodeDelay = 0;
            int64_t encodePadding = 0;
            sp<EditListBox> elst = FindBoxInside(trak, "edts", "elst");
            if (elst != NULL && elst->entries.size()) {
                size_t i = 0;
                if (elst->entries[0].media_time == -1) {
                    startTime = elst->entries[0].segment_duration;
                    DEBUG("startTime = %" PRId64, startTime);
                    ++i;
                }

                // we only support one non-empty edit.
                if (elst->entries.size() == i + 1) {
                    uint64_t media_time = elst->entries[i].media_time;
                    uint64_t segment_duration = elst->entries[i].segment_duration;

                    encodeDelay     = media_time;
                    // XXX: borrow from android, is it right???
                    encodePadding   = mdhd->duration - (media_time + segment_duration);
                    if (encodePadding < 0) encodePadding = 0;

                    trakInfo.setInt64(Media::EncoderDelay, encodeDelay);
                    trakInfo.setInt64(Media::EncoderPadding, encodePadding);
                }
            }
#endif

            String name = String::format("track-%zu", i);
            info.set<Message>(name, trakInfo);
        }

#if 0
        // TODO: meta
        // id3v2
        sp<ID3v2Box> id32 = FindBoxInside(mMovieBox, "meta", "ID32");
        if (id32 != NULL) {
            ID3::ID3v2 parser;
            if (parser.parse(*id32->ID3v2data) == OK) {
                info.set<Message>(Media::ID3v2, parser.values());
            }
        }
#endif

        return info;
    }

    virtual MediaError init(sp<Content>& pipe, const Message& options) {
        CHECK_TRUE(pipe != NULL);
        pipe->reset();
    
        FileTypeBox ftyp;
        sp<Buffer>  moovData;  // this is our target
        bool mdat = false;
        
        while (!mdat || moovData == NULL) {
            sp<Buffer> boxHeader    = pipe->read(8);
            if (boxHeader == 0 || boxHeader->size() < 8) {
                DEBUG("lack of data. ");
                break;
            }
            
            BitReader br(*boxHeader);
            
            size_t boxHeadLength    = 8;
            // if size is 1 then the actual size is in the field largesize;
            // if size is 0, then this box is the last one in the file
            uint64_t boxSize        = br.rb32();
            const String boxType    = br.readS(4);
            
            if (boxSize == 1) {
                sp<Buffer> large    = pipe->read(8);
                BitReader br(*large);
                boxSize             = br.rb64();
                boxHeadLength       = 16;
            }
            
            DEBUG("file: %s %" PRIu64, boxType.c_str(), boxSize);
            
            if (boxType == "mdat") {
                mdat = true;
                // ISO/IEC 14496-12: Section 8.2 Page 23
                DEBUG("skip media data box");
                pipe->skip(boxSize - boxHeadLength);
                continue;
            }
            
            if (boxSize == 0) {
                DEBUG("box extend to the end.");
                break;
            }
            
            CHECK_GE((size_t)boxSize, boxHeadLength);
            
            // empty box
            if (boxSize == boxHeadLength) {
                DEBUG("empty top level box %s", boxType.c_str());
                continue;
            }
            
            if (boxType == "meta") {
                meta.offset     = pipe->tell();
                meta.length     = boxSize;
                INFO("find meta box @ %zu(%zu)", meta.offset, meta.length);
                pipe->skip(boxSize);
                continue;
            }
            
            boxSize     -= boxHeadLength;
            sp<Buffer> boxPayload   = pipe->read(boxSize);
            if (boxPayload == 0 || boxPayload->size() != boxSize) {
                ERROR("truncated file ?");
                break;
            }
            
            if (boxType == "ftyp") {
                // ISO/IEC 14496-12: Section 4.3 Page 12
                DEBUG("file type box");
                BitReader _br(*boxPayload);
                ftyp = FileTypeBox(_br, boxSize);
            } else if (boxType == "moov") {
                // ISO/IEC 14496-12: Section 8.1 Page 22
                DEBUG("movie box");
                moovData = boxPayload;
            } else {
                ERROR("unknown top level box: %s", boxType.c_str());
            }
        }
        
        // ftyp maybe missing from mp4
        if (moovData == NULL) {
            ERROR("moov is missing");
            return kMediaErrorBadFormat;
        }
        
        if (mdat == false) {
            ERROR("mdat is missing");
            return kMediaErrorBadFormat;
        }
        
        sp<MovieBox> moov = new MovieBox;
        if (moov->parse(BitReader(*moovData), moovData->size(), ftyp) != OK) {
            ERROR("bad moov box?");
            return kMediaErrorBadFormat;
        }
        PrintBox(moov);
        
        sp<MovieHeaderBox> mvhd = FindBox(moov, "mvhd");
        if (mvhd == 0) {
            ERROR("can not find mvhd.");
            return kMediaErrorBadFormat;
        }
        
        for (size_t i = 0; ; ++i) {
            sp<TrackBox> trak = FindBox(moov, "trak", i);
            if (trak == 0) break;
            
            sp<Mp4Track> track = prepareTrack(trak, mvhd);
            
            if (track == NULL) continue;
            
            track->trackIndex = mTracks.size();
            mTracks.push(*track);
        }
        
        if (mTracks.empty()) {
            ERROR("no valid track present.");
            return kMediaErrorBadFormat;
        }
        
        // TODO: handle meta box
        
        INFO("%zu tracks ready", mTracks.size());
        
        mContent = pipe;
        return kMediaNoError;
    }

    virtual sp<MediaPacket> read(size_t index,
            eModeReadType mode,
            const MediaTime& _ts = kTimeInvalid) {
        Mp4Track& track = mTracks[index];
        Vector<Sample>& tbl = track.sampleTable;

        MediaTime ts = _ts;

        // first read, force mode = kModeReadFirst;
        if (track.startTime == kTimeInvalid) {
            INFO("track %zu: read first pakcet", index);
            mode = kModeReadFirst;
            track.startTime = kTimeBegin.scale(track.duration.timescale);
        }

        // ts will be ignored for these modes
        if (mode == kModeReadFirst ||
                mode == kModeReadNext ||
                mode == kModeReadLast ||
                mode == kModeReadCurrent) {
            ts = kTimeInvalid;
        }

        // calc sample index before read sample
        // determine direction and sample index based on mode
        int sampleIndex = track.sampleIndex;
        if (ts != kTimeInvalid) {
            // if ts exists, seek directly to new position,
            // seek() will take direction into account
            ts = ts.scale(track.duration.timescale);

            size_t match;
            sampleIndex = findSampleIndex(track, ts.value, mode, &match);

            if (mode != kModeReadPeek) {
                track.startTime = MediaTime(tbl[sampleIndex].dts,
                        track.duration.timescale);
                if (sampleIndex < match) {
                    track.startTime = MediaTime(tbl[match].dts,
                            track.duration.timescale);
                }
            }
        } else if (mode == kModeReadNextSync) {
            while (sampleIndex < tbl.size()) {
                Sample& s = tbl[sampleIndex];
                if (s.flags & kFrameFlagSync) break;
                else ++sampleIndex;
            }
        } else if (mode == kModeReadLastSync) {
            do {
                Sample& s = tbl[sampleIndex];
                if (s.flags & kFrameFlagSync) break;
                else --sampleIndex;
            } while (sampleIndex > 0);
        } else if (mode == kModeReadNext) {
            ++sampleIndex;
        } else if (mode == kModeReadLast) {
            --sampleIndex;
        } else if (mode == kModeReadFirst) {
            sampleIndex = 0;
        }

        // eos check
        if (sampleIndex >= tbl.size() || sampleIndex < 0) {
            INFO("eos...");
            return NULL;
        }

        // save sample index
        if (mode != kModeReadPeek) {
            track.sampleIndex = sampleIndex;
        }

        // read sample data
        Sample& s = tbl[sampleIndex];
        mContent->seek(s.offset);

        sp<Buffer> sample = mContent->read(s.size);

        if (sample == 0 || sample->size() < s.size) {
            ERROR("EOS or error.");
            return NULL;
        }

        // setup flags
        uint32_t flags  = s.flags;
#if 1
        MediaTime dts( s.dts, track.startTime.timescale);
        if (dts < track.startTime) {
            if (flags & kFrameFlagDisposal) {
                INFO("track %zu: drop frame", index);
                return read(index, mode, kTimeInvalid);
            } else {
                INFO("track %zu: reference frame", index);
                flags |= kFrameFlagReference;
            }
        } else if (dts == track.startTime) {
            INFO("track %zu: hit starting...", index);
        }
#endif
        // init MediaPacket context
        sp<MediaPacket> packet = new Mp4Packet(sample);
        packet->index   = sampleIndex;
        packet->format  = track.codec;
        packet->flags   = flags;
        if (s.pts == kTimeValueInvalid)
            packet->pts = kTimeInvalid;
        else
            packet->pts = MediaTime(s.pts, track.duration.timescale);
        packet->dts     = MediaTime(s.dts, track.duration.timescale);
        return packet;
    }
};

namespace mtdcy {
    sp<MediaExtractor> CreateMp4File() {
        return new Mp4File;
    }
}
