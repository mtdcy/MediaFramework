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

#include "Mp4File.h"
#include "Systems.h"
#include "tags/id3/ID3.h"
#include "Box.h"

// reference: 
// ISO/IEC 14496-12: ISO base media file format
// ISO/IEC 14496-14: mp4 file format
// http://atomicparsley.sourceforge.net/mpeg-4files.html
// http://www.mp4ra.org/atoms.html
namespace mtdcy {
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

    eCodecFormat get_codec_format(const String& name) {
        for (size_t i = 0; kCodecMap[i].format != kCodecFormatUnknown; ++i) {
            if (name == kCodecMap[i].name)
                return kCodecMap[i].format;
        }
        return kCodecFormatUnknown;
    }

    // TODO:
    status_t prepareMetaData(const sp<Box>& meta, const sp<Message>& target) {
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

    ///////////////////////////////////////////////////////////////////////////
    Mp4File::Mp4File(sp<Content>& pipe, const Message& params) :
        MediaExtractor(),
        mContent(pipe), mStatus(NO_INIT)
    {
        bool mdat = false;
        sp<Buffer> moov;
        sp<Buffer> meta;

        FileTypeBox ftyp;
        while (!mdat || moov == 0) {
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
                sp<Buffer> largesize    = pipe->read(8);
                BitReader br(*largesize);
                boxSize         = br.rb64();
                boxHeadLength   = 16;
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

            boxSize     -= boxHeadLength;
            sp<Buffer> boxPayload   = pipe->read(boxSize);
            if (boxPayload == 0 || boxPayload->size() != boxSize) {
                ERROR("truncated file ?");
                break;
            }

            BitReader _br(*boxPayload);
            if (boxType == "ftyp") {
                // ISO/IEC 14496-12: Section 4.3 Page 12
                DEBUG("file type box");
                ftyp = FileTypeBox(_br, boxSize);
            } else if (boxType == "moov") {
                // ISO/IEC 14496-12: Section 8.1 Page 22
                DEBUG("movie box");
                moov = boxPayload;
            } else if (boxType == "meta") {
                DEBUG("meta box");
                meta = boxPayload;
            } else {
                ERROR("unknown top level box: %s", boxType.c_str());
            }
        }

        if (moov != 0 && mdat) {
            BitReader br(*moov);
            sp<MovieBox> box = new MovieBox;
            if (box->parse(br, br.size(), ftyp) == OK) {
#if 1 // LOG_NDEBUG == 0
                PrintBox(box);
#endif
                mStatus = prepare(box);
            }
            mMovieBox = box;
        } else {
            ERROR("moov/mdat is missing.");
        }

        if (mStatus == OK && meta != NULL) {
            BitReader br(*meta);
            sp<MetaBox> box = new MetaBox();
            if (box->parse(br, br.size(), ftyp) == OK) {
#if LOG_NDEBUG == 0
                PrintBox(box);
#endif
                // TODO
            }
        }
    }

    status_t Mp4File::prepare(const sp<MovieBox>& moov) {
        sp<MovieHeaderBox> mvhd = FindBox(moov, "mvhd");
        if (mvhd == 0) {
            ERROR("can not find mvhd.");
            return NO_INIT;
        }

        for (size_t i = 0; ; ++i) {
            sp<TrackBox> trak = FindBox(moov, "trak", i);
            if (trak == 0) break;

            sp<Track> track = prepareTrack(trak, mvhd);
            
            if (track == NULL) continue;
            
            mTracks.push(track);
        }

        if (mTracks.empty()) {
            ERROR("no valid track present.");
            return NO_INIT;
        }
        
        INFO("%zu tracks ready", mTracks.size());
        return OK;
    }

    sp<Mp4File::Track> Mp4File::prepareTrack(const sp<TrackBox>& trak, const sp<MovieHeaderBox>& mvhd) {
        if (CheckTrackBox(trak) == false) {
            ERROR("bad TrackBox");
            return NULL;
        }
        
        sp<Track> track = new Track;
        track->mIndex = mTracks.size();

        sp<TrackHeaderBox> tkhd = FindBox(trak, "tkhd");
        if (tkhd == 0) {
            ERROR("tkhd is missing.");
            return NULL;
        }
        // TODO: learn more from tkhd
        
        // duration
        sp<MediaBox> mdia = FindBox(trak, "mdia");
        if (mdia == 0) {
            ERROR("mdia is missing.");
            return NULL;
        }

        sp<MediaHeaderBox> mdhd = FindBox(mdia, "mdhd");
        if (mdhd == 0) {
            ERROR("mdhd is missing");
            return NULL;
        }

        track->mDuration = MediaTime(mdhd->duration, mdhd->timescale);

        sp<HandlerBox> hdlr = FindBox(mdia, "hdlr");
        if (hdlr == 0) {
            ERROR("hdlr is missing.");
            return NULL;
        }

        DEBUG("handler: [%s] %s", hdlr->handler_type.c_str(),
                hdlr->handler_name.c_str());

        track->mIsAudio = hdlr->handler_type == "soun";
        
        sp<MediaInformationBox> minf = FindBox(mdia, "minf");
        if (minf == 0) {
            ERROR("minf is missing.");
            return NULL;
        }

        sp<DataReferenceBox> dinf = FindBox(minf, "dinf");
        if (dinf == 0) {
            ERROR("dinf is missing.");
            return NULL;
        }
        // XXX: handle dinf

        sp<SampleTableBox> stbl = FindBox(minf, "stbl");
        if (stbl == 0) {
            ERROR("stbl is missing.");
            return NULL;
        }

        sp<SampleDescriptionBox> stsd = FindBox(stbl, "stsd");
        if (stsd == 0) {
            ERROR("stsd is missing.");
            return NULL;
        }
        if (stsd->child.size() == 0) {
            ERROR("SampleEntry is missing from stsb.");
            return NULL;
        }

        if (stsd->child.size() > 1) {
            WARN("stsd with multi SampleEntry");
        }

        sp<SampleEntry> sampleEntry = stsd->child[0];
        track->mCodec = get_codec_format(sampleEntry->name);

        sp<TimeToSampleBox> stts    = FindBox(stbl, "stts");    // dts
        sp<SampleToChunkBox> stsc   = FindBox(stbl, "stsc");    // sample to chunk
        sp<ChunkOffsetBox> stco     = FindBox(stbl, "stco", "co64");    // chunk offset
        sp<SampleSizeBox> stsz      = FindBox(stbl, "stsz", "stz2");
        if (stts == 0 || stsc == 0 || stco == 0 || stsz == 0) {
            ERROR("stts/stsc/stco/stsz is missing.");
            return NULL;
        }

        sp<SyncSampleBox> stss = FindBox(stbl, "stss");
        sp<ShadowSyncSampleBox> stsh = FindBox(stbl, "stsh");

        // FIXME: no-output sample
        const uint64_t now = SystemTimeUs();
        uint64_t dts = 0;
        for (size_t i = 0; i < stts->entries.size(); ++i) {
            for (size_t j = 0; j < stts->entries[i].sample_count; ++j) {
                dts += stts->entries[i].sample_delta;
                Sample s = { 0/*offset*/, 0/*size*/,
                    dts, 0/*pts*/,
                    stss != NULL ? kFrameFlagNone : kFrameFlagSync};
                track->mSampleTable.push(s);
            }
        }

        // ctts => pts
        sp<CompositionOffsetBox> ctts = FindBox(stbl, "ctts");
        if (ctts != 0) {
            size_t sampleIndex = 0;
            for (size_t i = 0; i < ctts->entries.size(); ++i) {
                for (size_t j = 0; j < ctts->entries[i].sample_count; ++j && ++sampleIndex) {
                    Sample& s = track->mSampleTable[sampleIndex];
                    s.pts = s.dts + ctts->entries[i].sample_offset;
                }
            }
        } else {
            DEBUG("ctts is not present.");
            for (size_t i = 0; i < track->mSampleTable.size(); ++i) {
                Sample& s = track->mSampleTable[i];
                s.pts = s.dts;
            }
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
                Sample& s = track->mSampleTable[sampleIndex];

                s.offset = offset;
                s.size = stsz->sample_size ?  stsz->sample_size : stsz->entries[sampleIndex];

                offset += s.size;
            }
        }

        // stss => key frames
        if (stss != NULL) {
            for (size_t i = 0; i < stss->entries.size(); ++i) {
                Sample& s = track->mSampleTable[stss->entries[i] - 1];
                s.flags |= kFrameFlagSync;
                //INFO("sync frame %d", stss->entries[i]);
            }
            INFO("every %zu frame has one sync frame",
                    track->mSampleTable.size() / stss->entries.size());
        }

        // ISO/IEC 14496-12:2015 Section 8.6.4
        sp<SampleDependencyTypeBox> sdtp = FindBox(stbl, "sdtp");
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
                Sample& s = track->mSampleTable[i];
                if (sample_depends_on == 2)     s.flags |= kFrameFlagSync;
                if (is_leading == 2)            s.flags |= kFrameFlagLeading;
                if (sample_is_depended_on == 2) s.flags |= kFrameFlagDisposal;
                if (sample_has_redundancy == 1) s.flags |= kFrameFlagRedundant;
            }
        }

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
        DEBUG("num samples %zu total %zu", track.mSampleTable.size(),
                track.mSampleTable.size() * sizeof(Sample));

#if 1
        sp<TrackReferenceBox> tref = FindBox(trak, "tref");
        if (tref != 0) {
            CHECK_EQ(tref->child.size(), 1);
            sp<TrackReferenceTypeBox> type = tref->child[0];
            DEBUG("tref %s", type->name.c_str());
            // TODO
        }
#endif

        track->mTrackBox = trak;
        return track;
    }
    
    String Mp4File::string() const {
        return String("Mp4File");
    }

    status_t Mp4File::status() const {
        return mStatus;
    }

    // FIXME: build format messagge at runtime, as we don't need to
    // hold the message for whole life time, it is waste of memory,
    // and it is not good structure for frequently access.
    Message Mp4File::formats() const {
        Message info;
        info.setInt32(kKeyFormat, kFileFormatMP4);
        
        sp<MovieHeaderBox> mvhd = FindBox(mMovieBox, "mvhd");
        info.set<MediaTime>(kKeyDuration, MediaTime(mvhd->duration, mvhd->timescale));
        
        info.setInt32(kKeyCount, mTracks.size());
        
        for (size_t i = 0; i < mTracks.size(); ++i) {
            sp<TrackBox> trak = mTracks[i]->mTrackBox;
            
            Message trakInfo;
            
            sp<MediaBox> mdia = FindBox(trak, "mdia");
            sp<MediaHeaderBox> mdhd = FindBox(mdia, "mdhd");
            trakInfo.set<MediaTime>(kKeyDuration, MediaTime( mdhd->duration, mdhd->timescale ));
            
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
            
            sp<SampleTableBox> stbl = FindBoxInside(mdia, "minf", "stbl");
            sp<SampleDescriptionBox> stsd = FindBox(stbl, "stsd");
            
            sp<SampleEntry> sampleEntry = stsd->child[0];
            eCodecFormat codec = get_codec_format(sampleEntry->name);
            trakInfo.setInt32(kKeyFormat, codec);
            
            for (size_t i = 0; i < sampleEntry->child.size(); ++i) {
                sp<Box> box = sampleEntry->child[i];
                String knownBox = "esds avcC hvcC";
                if (knownBox.indexOf(box->name) >= 0) {
                    sp<CommonBox> csd = box;
                    trakInfo.set<Buffer>(box->name, *csd->data);
                }
                // esds in mov.
                else if (box->name == "wave") {
                    sp<CommonBox> esds = FindBox(box, "esds");
                    trakInfo.set<Buffer>(esds->name, *esds->data);
                } else {
                    INFO("ignore box %s", box->name.c_str());
                }
            }
            
            eCodecType type = GetCodecType(codec);
            if (type == kCodecTypeAudio) {
                if (sampleEntry->sound.samplerate) {
                    trakInfo.setInt32(kKeySampleRate, sampleEntry->sound.samplerate);
                } else {
                    ERROR("sample rate missing from sample entry box, using timescale in mdhd");
                    trakInfo.setInt32(kKeySampleRate, mdhd->timescale);
                }
                trakInfo.setInt32(kKeyChannels, sampleEntry->sound.channelcount);
            } else if (type == kCodecTypeVideo) {
                trakInfo.setInt32(kKeyWidth, sampleEntry->visual.width);
                trakInfo.setInt32(kKeyHeight, sampleEntry->visual.height);
            }
            
            String name = String::format("track-%zu", i);
            info.set<Message>(name, trakInfo);
        }
 
        // TODO: meta
        // id3v2
        sp<ID3v2Box> id32 = FindBoxInside(mMovieBox, "meta", "ID32");
        if (id32 != NULL) {
            ID3::ID3v2 parser;
            if (parser.parse(*id32->ID3v2data) == OK) {
                info.set<Message>(Media::ID3v2, parser.values());
            }
        }
        
        return info;
    }

    status_t Mp4File::configure(const Message& options) {
        return INVALID_OPERATION;
    }

    size_t Mp4File::seek(size_t index, int64_t ts, eModeReadType mode, size_t *match) const {
        const Track& track = *mTracks[index];
        const Vector<Sample>& tbl = track.mSampleTable;

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
                index, (double)ts / track.mDuration.timescale,
                first, mid, second, result,
                search_count);

        if (match) *match = mid;
        return result;
    }

    sp<MediaPacket> Mp4File::read(size_t index,
            eModeReadType mode,
            const MediaTime& _ts) {
        Track& track = *mTracks[index];
        Vector<Sample>& tbl = track.mSampleTable;

        MediaTime ts = _ts;

        // first read, force mode = kModeReadFirst;
        if (track.mStartTime == kTimeInvalid) {
            INFO("track %zu: read first pakcet", index);
            mode = kModeReadFirst;
            track.mStartTime = kTimeBegin.scale(track.mDuration.timescale);
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
        int sampleIndex = track.mSampleIndex;
        if (ts != kTimeInvalid) {
            // if ts exists, seek directly to new position,
            // seek() will take direction into account
            ts = ts.scale(track.mDuration.timescale);

            size_t match;
            sampleIndex = seek(index, ts.value, mode, &match);

            if (mode != kModeReadPeek) {
                track.mStartTime = MediaTime(tbl[sampleIndex].dts,
                        track.mDuration.timescale);
                if (sampleIndex < match) {
                    track.mStartTime = MediaTime(tbl[match].dts,
                            track.mDuration.timescale);
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
            track.mSampleIndex = sampleIndex;
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
        MediaTime dts( s.dts, track.mStartTime.timescale);
        if (dts < track.mStartTime) {
            if (flags & kFrameFlagDisposal) {
                INFO("track %zu: drop frame", index);
                return read(index, mode);
            } else {
                INFO("track %zu: reference frame", index);
                flags |= kFrameFlagReference;
            }
        } else if (dts == track.mStartTime) {
            INFO("track %zu: hit starting...", index);
        }
#endif
        // init MediaPacket context
        sp<MediaPacket> packet = new MediaPacket;
        packet->format  = track.mCodec;
        packet->data    = sample;
        packet->index   = sampleIndex;
        packet->flags   = flags;
        packet->pts     = MediaTime(s.pts, track.mDuration.timescale);
        packet->dts     = MediaTime(s.dts, track.mDuration.timescale);
        return packet;
    }
};
