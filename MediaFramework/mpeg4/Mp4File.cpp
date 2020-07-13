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
#include "Video.h"
#include "id3/ID3.h"
#include "Box.h"
#include "MediaDevice.h"


// reference: 
// ISO/IEC 14496-12: ISO base media file format
// ISO/IEC 14496-14: mp4 file format
// http://atomicparsley.sourceforge.net/mpeg-4files.html
// http://www.mp4ra.org/atoms.html

__BEGIN_NAMESPACE_MPX

using namespace MPEG4;

struct {
    const UInt32      type;
    const UInt32      format;
} kCodecMap[] = {
    {kBoxTypeMP4A,      kAudioCodecAAC     },
    {kBoxTypeAVC1,      kVideoCodecH264    },
    {kBoxTypeAVC2,      kVideoCodecH264    },
    {kBoxTypeHVC1,      kVideoCodecHEVC    },
    // END OF LIST
    {'    ',            kAudioCodecUnknown },
};

static UInt32 get_codec_format(UInt32 type) {
    for (UInt32 i = 0; kCodecMap[i].format != kAudioCodecUnknown; ++i) {
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
    UInt64            offset;
    UInt32              size;   // in bytes
    Int64             dts;
    Int64             pts;
    UInt32            flags;
};

struct Mp4Track : public SharedObject {
    Mp4Track() : enabled(True), type(kCodecTypeUnknown), codec(0), duration(kMediaTimeInvalid),
    sampleIndex(0), startIndex(0), bitReate(0), samplesRead(0) { }

    Bool                enabled;    // enabled by default
    eCodecType          type;
    UInt32            codec;  // eAudioCodec|eVideoCodec
    MediaTime           duration;
    UInt32              sampleIndex;
    UInt32              startIndex;
    Vector<Sample>      sampleTable;
    Int32             bitReate;

    union {
        struct {
            Int32     width;
            Int32     height;
        } video;
        struct {
            Int32     sampleRate;
            Int32     channelCount;
        } audio;
    };
    sp<CommonBox>       esds;
    
    union {
        UInt8         lengthSizeMinusOne;     // for h264, @see AVCDecoderConfigurationRecord.lengthSizeMinusOne
    };
    
    // statistics
    UInt32              samplesRead;
};

static sp<Mp4Track> prepareTrack(const sp<TrackBox>& trak, const sp<MovieHeaderBox>& mvhd) {
    if (CheckTrackBox(trak) == False) {
        ERROR("bad TrackBox");
        return Nil;
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
        return Nil;
    }

    if (stsd->child.size() > 1) {
        WARN("stsd with multi SampleEntry");
    }

    sp<Mp4Track> track = new Mp4Track;
    track->duration = MediaTime(mdhd->duration, mdhd->timescale);

    DEBUG("handler: [%.4s] %s", BoxName(hdlr->handler_type),
          hdlr->handler_name.c_str());

    // find sample infomations
    sp<SampleEntry> sampleEntry = stsd->child[0];
    track->codec = get_codec_format(sampleEntry->Type);
    if (track->codec == kAudioCodecUnknown) {
        ERROR("unsupported track sample '%s'", (const Char *)&sampleEntry->Type);
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

    for (UInt32 i = 0; i < sampleEntry->child.size(); ++i) {
        sp<Box> box = sampleEntry->child[i];
        if (box->Type == kBoxTypeESDS ||
            box->Type == kBoxTypeAVCC ||
            box->Type == kBoxTypeHVCC) {
            track->esds = box;
        }
        // esds in mov.
        else if (box->Type == kBoxTypeWAVE) {
            track->esds = FindBox(box, kBoxTypeESDS);
        } else if (box->Type == kBoxTypeBTRT) {
            sp<BitRateBox> btrt = box;
            track->bitReate = btrt->avgBitrate;
        } else {
            INFO("ignore box %s", (const Char *)&box->Type);
        }
    }
    
    if (track->codec == kVideoCodecH264) {
        if (track->esds != Nil) {
            AVCDecoderConfigurationRecord avcC;
            if (avcC.parse(track->esds->data->cloneBytes()) == kMediaNoError) {
                track->lengthSizeMinusOne = avcC.lengthSizeMinusOne;
            }
        }
    }

    // FIXME: no-output sample
    const Time now = Time::Now();
    
    // init sampleTable with dts
    UInt64 dts = 0;
    for (UInt32 i = 0; i < stts->entries.size(); ++i) {
        for (UInt32 j = 0; j < stts->entries[i].sample_count; ++j) {
            // ISO/IEC 14496-12:2015 Section 8.6.2.1
            //  If the sync sample box is not present, every sample is a sync sample.
            Sample s = { 0/*offset*/, 0/*size*/,
                dts, dts/*init pts with dts*/,
                stss != Nil ? kFrameTypeUnknown : kFrameTypeSync};
            track->sampleTable.push(s);
            dts += stts->entries[i].sample_delta;
        }
    }

    // ctts => pts
    if (ctts != 0) {
        UInt32 sampleIndex = 0;
        for (UInt32 i = 0; i < ctts->entries.size(); ++i) {
            for (UInt32 j = 0; j < ctts->entries[i].sample_count; ++j) {
                Sample& s = track->sampleTable[sampleIndex++];
                s.pts = s.dts + ctts->entries[i].sample_offset;
            }
        }
    }
    
    if (hdlr->handler_type == kMediaTypeVideo && ctts == Nil) {
        ERROR("ctts is not present. pts will be missing");
    }

    // stco + stsc + stsz => sample size & offset
    CHECK_EQ((UInt32)stsc->entries[0].first_chunk, 1);
    UInt32 sampleIndex = 0;
    UInt32 stscIndex = 0;
    // first, go through each chunk
    for (UInt32 chunkIndex = 0; chunkIndex < stco->entries.size(); ++chunkIndex) {
        // find out how many samples in this chunk
        while (stscIndex + 1 < stsc->entries.size() &&
                stsc->entries[stscIndex + 1].first_chunk <= chunkIndex + 1) {
            ++stscIndex;
        }
        const UInt32 numSamples = stsc->entries[stscIndex].samples_per_chunk;

        // set each samples offset and size
        UInt64 offset = stco->entries[chunkIndex];
        for (UInt32 i = 0; i < numSamples; ++i && ++sampleIndex) {
            Sample& s = track->sampleTable[sampleIndex];

            s.offset = offset;
            s.size = stsz->sample_size ?  stsz->sample_size : stsz->entries[sampleIndex];

            offset += s.size;
        }
    }

    // stss => key frames
    if (stss != Nil) {
        for (UInt32 i = 0; i < stss->entries.size(); ++i) {
            Sample& s = track->sampleTable[stss->entries[i] - 1];
            s.flags |= kFrameTypeSync;
            //INFO("sync frame %d", stss->entries[i]);
        }
        INFO("every %zu frame has one sync frame",
                track->sampleTable.size() / stss->entries.size());
    }

    // ISO/IEC 14496-12:2015 Section 8.6.4
    if (sdtp != Nil) {
        for (UInt32 i = 0; i < sdtp->dependency.size(); ++i) {
            UInt8 dep = sdtp->dependency[i];
            UInt8 is_leading = (dep & 0xc0) >> 6;
            UInt8 sample_depends_on = (dep & 0x30) >> 4;
            UInt8 sample_is_depended_on = (dep & 0xc) >> 2;
            UInt8 sample_has_redundancy = (dep & 0x3);
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
            
#if 0       // FIXME: handle is_leading & sample_has_redundacy properly
            if (is_leading == 2)            s.flags |= kFrameFlagLeading;
            if (sample_has_redundancy == 1) s.flags |= kFrameFlagRedundant;
#endif
        }
    } else {
        // we don't have enough infomation to seperate P-frame & B-frame,
        // so mark unknown frames as being depended.
        for (UInt32 i = 0; i < track->sampleTable.size(); ++i) {
            Sample& s = track->sampleTable[i];
            if (s.flags & kFrameTypeSync) continue;
        }
    }

#if 1
    if (tref != 0) {
        CHECK_EQ(tref->child.size(), 1);
        // TODO
    }
#endif

    DEBUG("init sample table takes %.2f", (Time::Now() - now) / 1E6);
#if 0
    for (UInt32 i = 0; i < track.mSampleTable.size(); ++i) {
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

static MediaError seekTrack(sp<Mp4Track>& track, Int64 us) {
    const Vector<Sample>& tbl = track->sampleTable;
    // dts&pts in tbl using duration's timescale
    us = (us * track->duration.scale) / 1000000LL;

    UInt32 first = 0;
    UInt32 second = tbl.size() - 1;
    UInt32 mid = 0;

    // using binary search to find sample index
    // closest search
    UInt32 search_count = 0;
    while (first < second) {
        mid = (first + second) / 2; // truncated happens
        const Sample& s0 = tbl[mid];
        const Sample& s1 = tbl[mid + 1];
        if (s0.dts <= us && s1.dts > us) first = second = mid;
        else if (s0.dts > us) second = mid;
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

    const UInt32 result = first;
    track->sampleIndex  = result;   // key sample index
    track->startIndex   = mid;

    INFO("seek %.3f(s) => [%zu - %zu - %zu] => %zu # %zu",
            us / 1E6,
            first, mid, second, result,
            search_count);

    return kMediaNoError;
}

struct Mp4File : public MediaDevice {
    sp<ABuffer>             mContent;
    Vector<sp<Mp4Track > >  mTracks;
    MediaTime               mDuration;
    struct {
        UInt32              offset;
        UInt32              length;
    } meta;
    
    // statistics
    UInt32                  mNumPacketsRead;

    Mp4File() : MediaDevice(), mContent(Nil),
    mDuration(kMediaTimeInvalid), mNumPacketsRead(0) {
    }

    virtual ~Mp4File() { }

    virtual sp<Message> formats() const {
        sp<Message> info = new Message;
        info->setInt32(kKeyFormat, kFileFormatMp4);
        info->setInt64(kKeyDuration, mDuration.useconds());
        info->setInt32(kKeyCount, mTracks.size());

        for (UInt32 i = 0; i < mTracks.size(); ++i) {
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

            if (trak->esds != Nil) {
                DEBUG("esds: %s", trak->esds->data->string(True).c_str());
                trakInfo->setObject(FOURCC(trak->esds->Type), trak->esds->data);
            }

#if 0
            // start time & encode delay/padding
            UInt64 startTime = 0;     // FIXME: learn more about start time.
            Int64 encodeDelay = 0;
            Int64 encodePadding = 0;
            sp<EditListBox> elst = FindBoxInside(trak, kBoxTypeEDTS, kBoxTypeELST);
            if (elst != Nil && elst->entries.size()) {
                UInt32 i = 0;
                if (elst->entries[0].media_time == -1) {
                    startTime = elst->entries[0].segment_duration;
                    DEBUG("startTime = %" PRId64, startTime);
                    ++i;
                }

                // we only support one non-empty edit.
                if (elst->entries.size() == i + 1) {
                    UInt64 media_time = elst->entries[i].media_time;
                    UInt64 segment_duration = elst->entries[i].segment_duration;

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
        if (id32 != Nil) {
            ID3::ID3v2 parser;
            if (parser.parse(*id32->ID3v2data) == kMediaNoError) {
                info.set<Message>(Media::ID3v2, parser.values());
            }
        }
#endif

        return info;
    }

    MediaError init(const sp<ABuffer>& buffer) {
        CHECK_TRUE(buffer != Nil);

        sp<FileTypeBox> ftyp = ReadBox(buffer);
        if (ftyp->Type != kBoxTypeFTYP) {
            ERROR("missing ftyp box at buffer head");
            return kMediaErrorBadContent;
        }
        
        sp<MovieBox> moov;
        sp<MediaDataBox> mdat;
        sp<MetaBox> meta;
        while (moov.isNil() || mdat.isNil()) {
            sp<Box> box = ReadBox(buffer, ftyp);
            if (box.isNil()) break;
            
            if (box->Type == kBoxTypeMDAT) {
                mdat = box;
                if (moov.isNil()) {
                    // skip mdat and search for moov
                    buffer->skipBytes(mdat->length);
                }
            } else if (box->Type == kBoxTypeMOOV) {
                moov = box;
            } else {
                INFO("box %s before moov/mdat", box->Name.c_str());
            }
        }
        
        if (moov.isNil()) {
            ERROR("missing moov box");
            return kMediaErrorBadContent;
        }
        if (mdat.isNil()) {
            ERROR("missing mdat box");
            return kMediaErrorBadContent;
        }
        
        PrintBox(moov);

        sp<MovieHeaderBox> mvhd = FindBox(moov, kBoxTypeMVHD);
        if (mvhd == 0) {
            ERROR("can not find mvhd.");
            return kMediaErrorBadFormat;
        }
        mDuration = MediaTime(mvhd->duration, mvhd->timescale);

        for (UInt32 i = 0; ; ++i) {
            sp<TrackBox> trak = FindBox(moov, kBoxTypeTRAK, i);
            if (trak == 0) break;

            sp<Mp4Track> track = prepareTrack(trak, mvhd);

            if (track == Nil) continue;

            mTracks.push(track);
        }

        if (mTracks.empty()) {
            ERROR("no valid track present.");
            return kMediaErrorBadFormat;
        }

        // TODO: handle meta box

        INFO("%zu tracks ready", mTracks.size());

        buffer->resetBytes();
        buffer->skipBytes(mdat->offset);
        mContent = buffer;
        return kMediaNoError;
    }
    
    virtual MediaError configure(const sp<Message>& options) {
        INFO("configure << %s", options->string().c_str());
        MediaError status = kMediaErrorNotSupported;
        if (options->contains(kKeyTracks)) {
            Bits<UInt32> mask = options->findInt32(kKeyTracks);
            CHECK_FALSE(mask.empty());
            for (UInt32 i = 0; i < mTracks.size(); ++i) {
                sp<Mp4Track>& track = mTracks[i];
                track->enabled = mask.test(i);
            }
            status = kMediaNoError;
        }
        
        if (options->contains(kKeySeek)) {
            seek(options->findInt64(kKeySeek));
            status = kMediaNoError;
        }
        return status;
    }
    
    void seek(Int64 us) {
        for (UInt32 i = 0; i < mTracks.size(); ++i) {
            sp<Mp4Track>& track = mTracks[i];
            // find new sample index
            seekTrack(track, us);
        }
    }
    
    virtual MediaError push(const sp<MediaFrame>&) {
        return kMediaErrorInvalidOperation;
    }

    virtual sp<MediaFrame> pull() {
        for (;;) {
            // find the lowest pos
            UInt32 trackIndex = mTracks.size();
            Int64 los = mContent->capacity();

            for (UInt32 i = 0; i < mTracks.size(); ++i) {
                sp<Mp4Track>& track = mTracks[i];
                if (!track->enabled) continue;
                if (track->sampleIndex >= track->sampleTable.size()) continue;

                Int64 pos = track->sampleTable[track->sampleIndex].offset;
                if (pos <= los) {
                    los = pos;
                    trackIndex = i;
                }
            }

            if (trackIndex >= mTracks.size()) {
                //CHECK_TRUE(mContent->size() == 0, "FIXME: report eos with data exists");
                INFO("eos @ %" PRId64 "[%" PRId64 "]", mContent->offset(), mContent->size());
                return Nil;
            }

            sp<Mp4Track>& track = mTracks[trackIndex];
            Vector<Sample>& tbl = track->sampleTable;
            UInt32 sampleIndex = track->sampleIndex++;

            // read sample data
            Sample& s = tbl[sampleIndex];

            mContent->skipBytes(s.offset - mContent->offset());

            sp<Buffer> sample = mContent->readBytes(s.size);

            if (sample == 0 || sample->size() < s.size) {
                ERROR("read return error or corrupt file?.");
                ERROR("report eos...");
                return Nil;
            }

            DEBUG("[%zu] read sample @%" PRId64 "(%" PRId64 "), %zu bytes, dts %" PRId64 ", pts %" PRId64,
                    trackIndex, s.offset, mContent->offset(), s.size, s.dts, s.pts);

            // statistics
            ++mNumPacketsRead;
            ++track->samplesRead;

            // setup flags
            UInt32 flags  = s.flags;

            if (track->codec == kVideoCodecH264) {
                if (sampleIndex < track->startIndex) {
                    MPEG4::NALU nalu;
                    sp<ABuffer> clone = sample->cloneBytes();
                    clone->skipBytes(track->lengthSizeMinusOne + 1);
                    if (nalu.parse(clone) == kMediaNoError) {
                        DEBUG("[%zu] h264, type %#x ref %#x, falgs %#x",
                                trackIndex,
                                nalu.nal_unit_type,
                                nalu.nal_ref_idc,
                                s.flags);
                    }
                    // DROP orphan B-frames before start time.
                    if (nalu.nal_unit_type == NALU_TYPE_SLICE &&
                        nalu.nal_ref_idc == 0 && /* no other B-frame depends on us */
                        (nalu.slice_header.slice_type == SLICE_TYPE_B ||
                        nalu.slice_header.slice_type == SLICE_TYPE_B2)) {
                        INFO("track %zu: drop frame", trackIndex);
                        continue;
                    } else {
                        flags |= kFrameTypeReference;
                    }
                }
            } else {
                if (sampleIndex < track->startIndex) {
                    if (flags & kFrameTypeSync) {
                        flags |= kFrameTypeReference;
                    } else {
                        INFO("track %zu: drop frame", trackIndex);
                        continue;
                    }
                }
            }
            
            // init MediaFrame context
            sp<MediaFrame> packet   = MediaFrame::Create(sample);
            packet->id              = trackIndex;
            packet->flags           = flags;
            if (s.pts < 0)
                packet->timecode    = MediaTime(s.dts, track->duration.scale);
            else
                packet->timecode    = MediaTime(s.pts, track->duration.scale);
            
            DEBUG("pull %s", packet->string().c_str());
            return packet;
        }

        return Nil;
    }
    
    virtual MediaError reset() {
        return kMediaNoError;
    }
};

sp<MediaDevice> CreateMp4File(const sp<ABuffer>& buffer) {
    sp<Mp4File> file = new Mp4File;
    if (file->init(buffer) == kMediaNoError) return file;
    return Nil;
}

Int IsMp4File(const sp<ABuffer>& buffer) {
    Int score = 0;
    while (buffer->size() > 8 && score < 100) {
        UInt32 boxHeadLength    = 8;
        // if size is 1 then the actual size is in the field largesize;
        // if size is 0, then this box is the last one in the file
        UInt64 boxSize    = buffer->rb32();
        UInt32 boxType    = buffer->rb32();

        if (boxSize == 1) {
            if (buffer->size() < 8) break;

            boxSize             = buffer->rb64();
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
            if (buffer->size() < boxSize - boxHeadLength) break;
            buffer->skipBytes(boxSize - boxHeadLength);
        }
    }

    return score > 100 ? 100 : score;
}

__END_NAMESPACE_MPX
