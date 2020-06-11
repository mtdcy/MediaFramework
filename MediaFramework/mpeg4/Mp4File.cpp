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
    const uint32_t      type;
    const uint32_t      format;
} kCodecMap[] = {
    {kBoxTypeMP4A,      kAudioCodecAAC     },
    {kBoxTypeAVC1,      kVideoCodecH264    },
    {kBoxTypeAVC2,      kVideoCodecH264    },
    {kBoxTypeHVC1,      kVideoCodecHEVC    },
    // END OF LIST
    {'    ',            kAudioCodecUnknown },
};

static uint32_t get_codec_format(uint32_t type) {
    for (size_t i = 0; kCodecMap[i].format != kAudioCodecUnknown; ++i) {
        if (type == kCodecMap[i].type)
            return kCodecMap[i].format;
    }
    return kAudioCodecUnknown;
}

// TODO:
static MediaError prepareMetaData(const sp<Box>& meta, const sp<Message>& target) {
    if (meta == 0 || target == 0) {
        ERROR("bad parameters.");
        return kMediaErrorUnknown;
    }

    sp<iTunesItemKeysBox> keys = FindBox(meta, kiTunesBoxTypeKEYS);
    sp<iTunesItemListBox> ilst = FindBox(meta, kiTunesBoxTypeILST);
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
    Mp4Track() : enabled(true), type(kCodecTypeUnknown), codec(0),
    sampleIndex(0), duration(kMediaTimeInvalid),
    startTime(0),
    bitReate(0), samplesRead(0) { }

    bool                enabled;    // enabled by default
    eCodecType          type;
    uint32_t            codec;  // eAudioCodec|eVideoCodec
    size_t              sampleIndex;
    MediaTime           duration;
    MediaTime           startTime;
    Vector<Sample>      sampleTable;
    int32_t             bitReate;

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
    
    // statistics
    size_t              samplesRead;
};

static sp<Mp4Track> prepareTrack(const sp<TrackBox>& trak, const sp<MovieHeaderBox>& mvhd) {
    if (CheckTrackBox(trak) == false) {
        ERROR("bad TrackBox");
        return NULL;
    }

    sp<TrackHeaderBox> tkhd             = FindBox(trak, kBoxTypeTKHD);
    sp<MediaBox> mdia                   = FindBox(trak, kBoxTypeMDIA);
    sp<MediaHeaderBox> mdhd             = FindBox(mdia, kBoxTypeMDHD);
    sp<HandlerBox> hdlr                 = FindBox(mdia, kBoxTypeHDLR);
    sp<MediaInformationBox> minf        = FindBox(mdia, kBoxTypeMINF);
    sp<DataReferenceBox> dinf           = FindBox(minf, kBoxTypeDINF);
    sp<SampleTableBox> stbl             = FindBox(minf, kBoxTypeSTBL);
    sp<SampleDescriptionBox> stsd       = FindBox(stbl, kBoxTypeSTSD);
    sp<TimeToSampleBox> stts            = FindBox(stbl, kBoxTypeSTTS);    // dts
    sp<SampleToChunkBox> stsc           = FindBox(stbl, kBoxTypeSTSC);    // sample to chunk
    sp<ChunkOffsetBox> stco             = FindBox2(stbl, kBoxTypeSTCO, kBoxTypeCO64);    // chunk offset
    sp<SampleSizeBox> stsz              = FindBox2(stbl, kBoxTypeSTSZ, kBoxTypeSTZ2);
    // optional
    sp<SyncSampleBox> stss              = FindBox(stbl, kBoxTypeSTSS);
    sp<ShadowSyncSampleBox> stsh        = FindBox(stbl, kBoxTypeSTSH);
    sp<CompositionOffsetBox> ctts       = FindBox(stbl, kBoxTypeCTTS);
    sp<TrackReferenceBox> tref          = FindBox(trak, kBoxTypeTREF);
    sp<SampleDependencyTypeBox> sdtp    = FindBox(stbl, kBoxTypeSDTP);

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

    DEBUG("handler: [%4s] %s", (const char*)&hdlr->handler_type,
            (const char *)&hdlr->handler_name);

    // find sample infomations
    sp<SampleEntry> sampleEntry = stsd->child[0];
    track->codec = get_codec_format(sampleEntry->type);
    if (track->codec == kAudioCodecUnknown) {
        ERROR("unsupported track sample '%s'", (const char *)&sampleEntry->type);
        return track;
    }

    if (hdlr->handler_type == kMediaTypeSound) {
        track->type = kCodecTypeAudio;
        track->audio.channelCount = sampleEntry->sound.channelcount;
        if (sampleEntry->sound.samplerate) {
            track->audio.sampleRate = sampleEntry->sound.samplerate;
        } else {
            ERROR("sample rate missing from sample entry box, using timescale in mdhd");
            track->audio.sampleRate = mdhd->timescale;
        }
    } else if (hdlr->handler_type == kMediaTypeVideo) {
        track->type = kCodecTypeVideo;
        track->video.width = sampleEntry->visual.width;
        track->video.height = sampleEntry->visual.height;
    }

    for (size_t i = 0; i < sampleEntry->child.size(); ++i) {
        sp<Box> box = sampleEntry->child[i];
        if (box->type == kBoxTypeESDS ||
            box->type == kBoxTypeAVCC ||
            box->type == kBoxTypeHVCC) {
            track->esds = box;
        }
        // esds in mov.
        else if (box->type == kBoxTypeWAVE) {
            track->esds = FindBox(box, kBoxTypeESDS);
        } else if (box->type == kBoxTypeBTRT) {
            sp<BitRateBox> btrt = box;
            track->bitReate = btrt->avgBitrate;
        } else {
            INFO("ignore box %s", (const char *)&box->type);
        }
    }

    // FIXME: no-output sample
    const uint64_t now = SystemTimeUs();
    
    // init sampleTable with dts
    uint64_t dts = 0;
    for (size_t i = 0; i < stts->entries.size(); ++i) {
        for (size_t j = 0; j < stts->entries[i].sample_count; ++j) {
            // ISO/IEC 14496-12:2015 Section 8.6.2.1
            //  If the sync sample box is not present, every sample is a sync sample.
            Sample s = { 0/*offset*/, 0/*size*/,
                dts, dts/*init pts with dts*/,
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
    }
    
    if (hdlr->handler_type == kMediaTypeVideo && ctts == NULL) {
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
            // 3: depends only on I-frame.
            if (is_leading == 3)            s.flags |= kFrameTypeDepended;
            
#if 0       // FIXME: handle is_leading & sample_has_redundacy properly
            if (is_leading == 2)            s.flags |= kFrameFlagLeading;
            if (sample_has_redundancy == 1) s.flags |= kFrameFlagRedundant;
#endif
        }
    } else {
        // we don't have enough infomation to seperate P-frame & B-frame,
        // so mark unknown frames as being depended.
        for (size_t i = 0; i < track->sampleTable.size(); ++i) {
            Sample& s = track->sampleTable[i];
            if (s.flags & kFrameTypeSync) continue;
            s.flags |= kFrameTypeDepended;
        }
    }

#if 1
    if (tref != 0) {
        CHECK_EQ(tref->child.size(), 1);
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

    track->sampleIndex  = result;   // key sample index
    track->startTime    = MediaTime(tbl[mid].dts, track->duration.timescale);

    INFO("seek %.3f(s)[%.3f(s)] => [%zu - %zu - %zu] => %zu # %zu",
            ts.seconds(), track->startTime.seconds(),
            first, mid, second, result,
            search_count);

    return kMediaNoError;
}

struct Mp4File : public MediaFile {
    sp<Content>             mContent;
    Vector<sp<Mp4Track > >  mTracks;
    MediaTime               mDuration;
    struct {
        size_t              offset;
        size_t              length;
    } meta;
    
    // statistics
    size_t                  mNumPacketsRead;

    Mp4File() : MediaFile(), mContent(NULL),
    mDuration(kMediaTimeInvalid),
    mNumPacketsRead(0)
    { }

    virtual ~Mp4File() { }

    virtual sp<Message> formats() const {
        sp<Message> info = new Message;
        info->setInt32(kKeyFormat, kFileFormatMp4);
        info->setInt64(kKeyDuration, mDuration.useconds());
        info->setInt32(kKeyCount, mTracks.size());

        for (size_t i = 0; i < mTracks.size(); ++i) {
            const sp<Mp4Track>& trak = mTracks[i];

            sp<Message> trakInfo = new Message;
            trakInfo->setInt32(kKeyType, trak->type);
            trakInfo->setInt32(kKeyFormat, trak->codec);
            trakInfo->setInt64(kKeyDuration, trak->duration.useconds());
            if (trak->bitReate)
                trakInfo->setInt32(kKeyBitrate, trak->bitReate);

            if (trak->type == kCodecTypeAudio) {
                trakInfo->setInt32(kKeySampleRate, trak->audio.sampleRate);
                trakInfo->setInt32(kKeyChannels, trak->audio.channelCount);
            } else if (trak->type == kCodecTypeVideo) {
                trakInfo->setInt32(kKeyWidth, trak->video.width);
                trakInfo->setInt32(kKeyHeight, trak->video.height);
            }

            if (trak->esds != NULL) {
                trakInfo->setObject(FOURCC(trak->esds->type), trak->esds->data);
            }

#if 0
            // start time & encode delay/padding
            uint64_t startTime = 0;     // FIXME: learn more about start time.
            int64_t encodeDelay = 0;
            int64_t encodePadding = 0;
            sp<EditListBox> elst = FindBoxInside(trak, kBoxTypeEDTS, kBoxTypeELST);
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
            info->setObject(kKeyTrack + i, trakInfo);
        }

#if 0
        // TODO: meta
        // id3v2
        sp<ID3v2Box> id32 = FindBoxInside(mMovieBox, kBoxTypeMETA, kBoxTypeID32);
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
        int64_t startPosition = 0;  // where mdat start

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
            uint64_t boxSize    = br.rb32();
            uint32_t boxType    = br.rb32();

            if (boxSize == 1) {
                sp<Buffer> large    = pipe->read(8);
                BitReader br(large->data(), large->size());
                boxSize             = br.rb64();
                boxHeadLength       = 16;
            }

            DEBUG("file: %4s %" PRIu64, BoxName(boxType), boxSize);

            if (boxType == kBoxTypeMDAT) {
                mdat = true;
                startPosition = pipe->tell();
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
                DEBUG("empty top level box %4s", (const char *)&boxType);
                continue;
            }

            if (boxType == kBoxTypeMETA) {
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

            if (boxType == kBoxTypeFTYP) {
                // ISO/IEC 14496-12: Section 4.3 Page 12
                DEBUG("file type box");
                BitReader _br(boxPayload->data(), boxPayload->size());
                ftyp = FileTypeBox(_br, boxSize);
            } else if (boxType == kBoxTypeMOOV) {
                // ISO/IEC 14496-12: Section 8.1 Page 22
                DEBUG("movie box");
                moovData = boxPayload;
            } else {
                ERROR("unknown top level box: %s", BoxName(boxType));
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

        sp<MovieHeaderBox> mvhd = FindBox(moov, kBoxTypeMVHD);
        if (mvhd == 0) {
            ERROR("can not find mvhd.");
            return kMediaErrorBadFormat;
        }
        mDuration = MediaTime(mvhd->duration, mvhd->timescale);

        for (size_t i = 0; ; ++i) {
            sp<TrackBox> trak = FindBox(moov, kBoxTypeTRAK, i);
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

        pipe->seek(startPosition);
        mContent = pipe;
        return kMediaNoError;
    }
    
    virtual MediaError configure(const sp<Message>& options) {
        INFO("configure << %s", options->string().c_str());
        MediaError status = kMediaErrorInvalidOperation;
        if (options->contains(kKeyTracks)) {
            BitSet mask = options->findInt32(kKeyTracks);
            CHECK_FALSE(mask.empty());
            for (size_t i = 0; i < mTracks.size(); ++i) {
                sp<Mp4Track>& track = mTracks[i];
                track->enabled = mask.test(i);
            }
            status = kMediaNoError;
        }
        return status;
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

        for (;;) {
        // find the lowest pos
        size_t trackIndex = mTracks.size();
        int64_t los = mContent->length();

        for (size_t i = 0; i < mTracks.size(); ++i) {
            sp<Mp4Track>& track = mTracks[i];
            if (!track->enabled) continue;
            if (track->sampleIndex >= track->sampleTable.size()) continue;
            
            int64_t pos = track->sampleTable[track->sampleIndex].offset;
            if (pos <= los) {
                los = pos;
                trackIndex = i;
            }
        }
        
        if (trackIndex >= mTracks.size()) {
            INFO("eos...");
            return NULL;
        }

        sp<Mp4Track>& track = mTracks[trackIndex];
        Vector<Sample>& tbl = track->sampleTable;
        size_t sampleIndex = track->sampleIndex++;

        // read sample data
        Sample& s = tbl[sampleIndex];
        
        DEBUG("[%zu] read sample @%" PRId64 "(%" PRId64 "), %zu bytes, dts %" PRId64 ", pts %" PRId64,
              trackIndex, s.offset, mContent->tell(),
              s.size, s.dts, s.pts);
        
        mContent->seek(s.offset);

        sp<Buffer> sample = mContent->read(s.size);

        if (sample == 0 || sample->size() < s.size) {
            ERROR("read return error, corrupt file?.");
            return NULL;
        }
        
        // statistics
        ++mNumPacketsRead;
        ++track->samplesRead;

        // setup flags
        uint32_t flags  = s.flags;
#if 1
        MediaTime dts( s.dts, track->duration.timescale);
        if (dts < track->startTime) {
            // we should only output the I-frame and closest P-frame
            // FIXME: find out the closest P-frame
            if (flags & (kFrameTypeSync|kFrameTypeDepended)) {
                flags |= kFrameTypeReference;
            } else {
                INFO("track %zu: drop frame", trackIndex);
                continue;
            }
        } else if (dts == track->startTime) {
            INFO("track %zu: hit starting...", trackIndex);
        }
#endif
        // init MediaPacket context
        sp<MediaPacket> packet  = MediaPacket::Create(sample);
        packet->index           = trackIndex;
        packet->type            = flags;
        if (s.pts < 0)
            packet->pts =       kMediaTimeInvalid;
        else
            packet->pts =       MediaTime(s.pts, track->duration.timescale);
        packet->dts     =       MediaTime(s.dts, track->duration.timescale);
        
        if (ts != kMediaTimeInvalid) {
            INFO("track %zu: read @ %.3fs", trackIndex, packet->dts.seconds());
        }
        return packet;
        }
        
        return NULL;
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
        uint64_t boxSize    = br.rb32();
        uint32_t boxType    = br.rb32();

        if (boxSize == 1) {
            if (br.remianBytes() < 8) break;

            boxSize             = br.rb64();
            boxHeadLength       = 16;
        }

        DEBUG("file: %4s %" PRIu64, BoxName(boxType), boxSize);
        
        // mdat may show before moov, give mdat more scores
        if (boxType == kBoxTypeFTYP ||
            boxType == kBoxTypeMDAT) {
            score += 50;
        } else if (boxType == kBoxTypeMOOV ||
                   boxType == kBoxTypeTRAK ||
                   boxType == kBoxTypeMDIA) {
            score += 20;    // this is a container box
            continue;
        } else if (boxType == kBoxTypeMETA ||
                   boxType == kBoxTypeMVHD ||
                   boxType == kBoxTypeTKHD ||
                   boxType == kBoxTypeTREF ||
                   boxType == kBoxTypeEDTS ||
                   boxType == kBoxTypeMDHD ||
                   boxType == kBoxTypeHDLR ||
                   boxType == kBoxTypeMINF) {
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
