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
#include <ABE/ABE.h>

#include "Systems.h"
#include "tags/id3/ID3.h"
#include "Box.h"

#include <MediaFramework/MediaFile.h>


// reference: 
// ISO/IEC 14496-12: ISO base media file format
// ISO/IEC 14496-14: mp4 file format
// http://atomicparsley.sourceforge.net/mpeg-4files.html
// http://www.mp4ra.org/atoms.html

__BEGIN_NAMESPACE_MPX

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
static MediaError prepareMetaData(const sp<Box>& meta, const sp<Message>& target) {
    if (meta == 0 || target == 0) {
        ERROR("bad parameters.");
        return kMediaErrorUnknown;
    }

    sp<iTunesItemKeysBox> keys = FindBox(meta, "keys");
    sp<iTunesItemListBox> ilst = FindBox(meta, "ilst");
    if (ilst == 0) {
        ERROR("ilst is missing.");
        return kMediaErrorUnknown;
    }

    return kMediaNoError;
}

struct Sample {
    uint64_t            offset;
    size_t              size;   // in bytes
    int64_t             dts;
    int64_t             pts;
    uint32_t            flags;
};

struct Mp4Track : public SharedObject {
    Mp4Track() : codec(kCodecFormatUnknown),
    sampleIndex(0), duration(kMediaTimeInvalid),
    startTime(kMediaTimeBegin) { }

    eCodecFormat        codec;
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
    
    // init sampleTable with dts
    for (size_t i = 0; i < stts->entries.size(); ++i) {
        uint64_t dts = 0;
        for (size_t j = 0; j < stts->entries[i].sample_count; ++j) {
            // ISO/IEC 14496-12:2015 Section 8.6.2.1
            //  If the sync sample box is not present, every sample is a sync sample.
            Sample s = { 0/*offset*/, 0/*size*/,
                dts, kTimeValueInvalid/*pts*/,
                stss != NULL ? kFrameTypeUnknown : kFrameTypeSync};
            track->sampleTable.push(s);
            dts += stts->entries[i].sample_delta;
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
            s.flags |= kFrameTypeSync;
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
            // does this sample depends others (e.g. is it an I‐picture)?
            if (sample_depends_on == 2)     s.flags |= kFrameTypeSync;
            // do no other samples depend on this one?
            if (sample_is_depended_on == 2) s.flags |= kFrameTypeDisposal;
            
#if 0       // FIXME: handle is_leading & sample_has_redundacy properly
            if (is_leading == 2)            s.flags |= kFrameFlagLeading;
            if (sample_has_redundancy == 1) s.flags |= kFrameFlagRedundant;
#endif
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

static MediaError seekTrack(sp<Mp4Track>& track,
        const MediaTime& _ts,
        const eReadMode& mode) {
    const Vector<Sample>& tbl = track->sampleTable;
    // dts&pts in tbl using duration's timescale
    MediaTime ts = _ts;
    ts.scale(track->duration.timescale);

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
        if (s0.dts <= ts.value && s1.dts > ts.value) first = second = mid;
        else if (s0.dts > ts.value) second = mid;
        else first = mid;
        ++search_count;
    }

    // find sync sample index
    do {
        const Sample& s = tbl[first];
        if (s.flags & kFrameTypeSync) break;
        if (first == 0) {
            WARN("track %zu: no sync at start");
            break;
        }
        --first;
    } while (first > 0);

    while (second < tbl.size()) {
        const Sample& s = tbl[second];
        if (s.flags & kFrameTypeSync) break;
        ++second;
    }

    size_t result;
    if (mode == kReadModeLastSync) {
        result = first;
    } else if (mode == kReadModeNextSync) {
        result = second;
    } else { // closest
        if (mid - first > second - mid)
            result = second;
        else
            result = first;
    }

    INFO("seek %.3f(s) => [%zu - %zu - %zu] => %zu # %zu",
            ts.seconds(),
            first, mid, second, result,
            search_count);

    track->sampleIndex  = result;   // key sample index
    track->startTime    = MediaTime(tbl[mid].dts, track->duration.timescale);

    return kMediaNoError;
}

struct Mp4Packet : public MediaPacket {
    sp<Buffer> buffer;
    Mp4Packet(const sp<Buffer>& _buffer) : MediaPacket(), buffer(_buffer) {
        data    = (uint8_t*)buffer->data();
        size    = buffer->size();
    }
};

struct Mp4File : public MediaFile {
    sp<Content>             mContent;
    Vector<sp<Mp4Track > >  mTracks;
    MediaTime               mDuration;
    struct {
        size_t              offset;
        size_t              length;
    } meta;

    Mp4File() : MediaFile(), mContent(NULL) { }

    virtual ~Mp4File() { }

    virtual sp<Message> formats() const {
        sp<Message> info = new Message;
        info->setInt32(kKeyFormat, kFileFormatMp4);
        info->setInt64(kKeyDuration, mDuration.useconds());
        info->setInt32(kKeyCount, mTracks.size());

        for (size_t i = 0; i < mTracks.size(); ++i) {
            const sp<Mp4Track>& trak = mTracks[i];

            sp<Message> trakInfo = new Message;
            trakInfo->setInt32(kKeyFormat, trak->codec);
            trakInfo->setInt64(kKeyDuration, trak->duration.useconds());

            eCodecType type = GetCodecType(trak->codec);
            if (type == kCodecTypeAudio) {
                trakInfo->setInt32(kKeySampleRate, trak->audio.sampleRate);
                trakInfo->setInt32(kKeyChannels, trak->audio.channelCount);
            } else if (type == kCodecTypeVideo) {
                trakInfo->setInt32(kKeyWidth, trak->video.width);
                trakInfo->setInt32(kKeyHeight, trak->video.height);
            }

            if (trak->esds != NULL) {
                trakInfo->setObject(trak->esds->name, trak->esds->data);
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
            info->setObject(name, trakInfo);
        }

#if 0
        // TODO: meta
        // id3v2
        sp<ID3v2Box> id32 = FindBoxInside(mMovieBox, "meta", "ID32");
        if (id32 != NULL) {
            ID3::ID3v2 parser;
            if (parser.parse(*id32->ID3v2data) == kMediaNoError) {
                info.set<Message>(Media::ID3v2, parser.values());
            }
        }
#endif

        return info;
    }

    MediaError init(sp<Content>& pipe) {
        CHECK_TRUE(pipe != NULL);

        FileTypeBox ftyp;
        sp<Buffer>  moovData;  // this is our target
        bool mdat = false;

        while (!mdat || moovData == NULL) {
            sp<Buffer> boxHeader    = pipe->read(8);
            if (boxHeader == 0 || boxHeader->size() < 8) {
                DEBUG("lack of data. ");
                break;
            }

            BitReader br(boxHeader->data(), boxHeader->size());

            size_t boxHeadLength    = 8;
            // if size is 1 then the actual size is in the field largesize;
            // if size is 0, then this box is the last one in the file
            uint64_t boxSize        = br.rb32();
            const String boxType    = br.readS(4);

            if (boxSize == 1) {
                sp<Buffer> large    = pipe->read(8);
                BitReader br(large->data(), large->size());
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
                BitReader _br(boxPayload->data(), boxPayload->size());
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
        if (moov->parse(BitReader(moovData->data(), moovData->size()), moovData->size(), ftyp) != kMediaNoError) {
            ERROR("bad moov box?");
            return kMediaErrorBadFormat;
        }
        PrintBox(moov);

        sp<MovieHeaderBox> mvhd = FindBox(moov, "mvhd");
        if (mvhd == 0) {
            ERROR("can not find mvhd.");
            return kMediaErrorBadFormat;
        }
        mDuration = MediaTime(mvhd->duration, mvhd->timescale);

        for (size_t i = 0; ; ++i) {
            sp<TrackBox> trak = FindBox(moov, "trak", i);
            if (trak == 0) break;

            sp<Mp4Track> track = prepareTrack(trak, mvhd);

            if (track == NULL) continue;

            mTracks.push(track);
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

    virtual sp<MediaPacket> read(const eReadMode& mode,
            const MediaTime& ts = kMediaTimeInvalid) {

        if (ts != kMediaTimeInvalid) {
            // seeking
            for (size_t i = 0; i < mTracks.size(); ++i) {
                sp<Mp4Track>& track = mTracks[i];
                // find new sample index
                seekTrack(track, ts, mode);
            }
        }

        // find the lowest pos
        size_t trackIndex = 0;
        int64_t los = mTracks[0]->sampleTable[mTracks[0]->sampleIndex].offset;

        for (size_t i = 1; i < mTracks.size(); ++i) {
            sp<Mp4Track>& track = mTracks[i];
            int64_t pos = track->sampleTable[track->sampleIndex].offset;
            if (pos < los) {
                los = pos;
                trackIndex = i;
            }
        }

        sp<Mp4Track>& track = mTracks[trackIndex];
        Vector<Sample>& tbl = track->sampleTable;
        size_t sampleIndex = track->sampleIndex++;

        // eos check
        if (sampleIndex >= tbl.size() || sampleIndex < 0) {
            INFO("eos...");
            return NULL;
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
        MediaTime dts( s.dts, track->startTime.timescale);
        if (dts < track->startTime) {
            if (flags & kFrameTypeDisposal) {
                INFO("track %zu: drop frame", trackIndex);
                return read(mode, kMediaTimeInvalid);
            } else {
                INFO("track %zu: reference frame", trackIndex);
                flags |= kFrameTypeReference;
            }
        } else if (dts == track->startTime) {
            INFO("track %zu: hit starting...", trackIndex);
        }
#endif
        // init MediaPacket context
        sp<MediaPacket> packet  = new Mp4Packet(sample);
        packet->index           = trackIndex;
        packet->format          = track->codec;
        packet->type            = flags;
        if (s.pts == kTimeValueInvalid)
            packet->pts =       kMediaTimeInvalid;
        else
            packet->pts =       MediaTime(s.pts, track->duration.timescale);
        packet->dts     =       MediaTime(s.dts, track->duration.timescale);
        return packet;
    }
};

sp<MediaFile> CreateMp4File(sp<Content>& pipe) {
    sp<Mp4File> file = new Mp4File;
    if (file->init(pipe) == kMediaNoError) return file;
    return NIL;
}

int IsMp4File(const sp<Buffer>& data) {
    BitReader br(data->data(), data->size());

    int score = 0;
    while (br.remianBytes() > 8 && score < 100) {
        size_t boxHeadLength    = 8;
        // if size is 1 then the actual size is in the field largesize;
        // if size is 0, then this box is the last one in the file
        uint64_t boxSize        = br.rb32();
        const String boxType    = br.readS(4);

        if (boxSize == 1) {
            if (br.remianBytes() < 8) break;

            boxSize             = br.rb64();
            boxHeadLength       = 16;
        }

        DEBUG("file: %s %" PRIu64, boxType.c_str(), boxSize);
        if (boxType == "ftyp" || boxType == "moov" || boxType == "mdat") {
            score += 40;
        } else if (boxType == "ftyp" ||
                boxType == "mdat" ||
                boxType == "pnot" || /* detect movs with preview pics like ew.mov and april.mov */
                boxType == "udat" || /* Packet Video PVAuthor adds this and a lot of more junk */
                boxType == "wide" ||
                boxType == "ediw" || /* xdcam files have reverted first tags */
                boxType == "free" ||
                boxType == "junk" ||
                boxType == "pict") {
            score += 10;
        }

        if (boxSize - boxHeadLength > 0) {
            if (br.remianBytes() < boxSize - boxHeadLength) break;
            br.skipBytes(boxSize - boxHeadLength);
        }
    }

    return score > 100 ? 100 : score;
}

__END_NAMESPACE_MPX
