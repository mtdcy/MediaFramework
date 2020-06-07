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


// File:    Libavformat.cpp
// Author:  mtdcy.chen
// Changes:
//          1. 20160701     initial version
//

#define LOG_TAG   "Lavf"
//#define LOG_NDEBUG 0
#include <ABE/ABE.h>
#include <MediaFramework/MediaFile.h>

#include <FFmpeg.h>

#include "mpeg4/Audio.h"
#include "mpeg4/Video.h"

static FORCE_INLINE const char * av_error_string(int err) {
    static char s[AV_ERROR_MAX_STRING_SIZE];
    return av_make_error_string(s, AV_ERROR_MAX_STRING_SIZE, err);
}

__BEGIN_NAMESPACE_MPX

struct {
    const char *        name;
    eFileFormat         format;
} kFormatMap[] = {
    { "wav",        kFileFormatWave },
    { "mp3",        kFileFormatMp3  },
    { "flac",       kFileFormatFlac },
    { "ape",        kFileFormatApe  },
    // video
    { "mov",        kFileFormatMp4  },
    { "mp4",        kFileFormatMp4  },  // & m4a
    { "matroska",   kFileFormatMkv  },  // matroska & webm
    // END OF LIST
    { NULL,         kFileFormatInvalid }
};

static eFileFormat GetFormat(const String& name) {
    for (size_t i = 0; kFormatMap[i].name; ++i) {
        if (name.startsWith(kFormatMap[i].name)) {
            return kFormatMap[i].format;
        }
    }
    
    ERROR("unknown format %s", name.c_str());
    return kFileFormatAny;
}

struct {
    AVCodecID           id;
    uint32_t            format;
} kCodecMap[] = {
    // AUDIO
    { AV_CODEC_ID_PCM_F16LE,    kAudioCodecPCM},
    { AV_CODEC_ID_FLAC,         kAudioCodecFLAC},
    { AV_CODEC_ID_MP3,          kAudioCodecMP3},
    { AV_CODEC_ID_MP2,          kAudioCodecMP3},  // mpeg layer info store in packet head, client don't need to known
    { AV_CODEC_ID_MP1,          kAudioCodecMP3},
    { AV_CODEC_ID_VORBIS,       kAudioCodecVorbis},
    { AV_CODEC_ID_AAC,          kAudioCodecAAC},
    { AV_CODEC_ID_AC3,          kAudioCodecAC3},
    { AV_CODEC_ID_WMV2,         kAudioCodecWMA},
    { AV_CODEC_ID_APE,          kAudioCodecAPE},
    { AV_CODEC_ID_DTS,          kAudioCodecDTS},
    // video
    { AV_CODEC_ID_H264,         kVideoCodecH264},
    { AV_CODEC_ID_H265,         kVideoCodecHEVC},
    { AV_CODEC_ID_MPEG4,        kVideoCodecMPEG4},
    { AV_CODEC_ID_VC1,          kVideoCodecVC1},
    { AV_CODEC_ID_H263,         kVideoCodecH263},
    // image
    { AV_CODEC_ID_PNG,          kImageCodecPNG},
    { AV_CODEC_ID_MJPEG,        kImageCodecJPEG},
    { AV_CODEC_ID_BMP,          kImageCodecBMP},
    { AV_CODEC_ID_GIF,          kImageCodecGIF},
    // END OF LIST
    { AV_CODEC_ID_NONE,         kAudioCodecUnknown}
};

static uint32_t GetCodecFormat(AVCodecID id) {
    for (size_t i = 0; kCodecMap[i].id != AV_CODEC_ID_NONE; ++i) {
        if (kCodecMap[i].id == id) return kCodecMap[i].format;
    }
    
    ERROR("unknown codec %s", avcodec_get_name(id));
    return kAudioCodecUnknown;
}

static int content_bridge_read_packet(void * opaque, uint8_t * buf, int length) {
    sp<Content> pipe = opaque;
    // TODO: change Content behavior -> read directly
    sp<Buffer> data = pipe->read(length);
    memcpy(buf, data->data(), data->size());
    return data->size();
}

static int64_t content_bridge_seek(void * opaque, int64_t offset, int whence) {
    sp<Content> pipe = opaque;
    switch (whence) {
        case SEEK_CUR:
            return pipe->skip(offset);
        case SEEK_END:
            return pipe->seek(pipe->length());
        case SEEK_SET:
            return pipe->seek(offset);
        case AVSEEK_SIZE:
            return pipe->length();
        default:
            FATAL("FIXME...");
    }
    return 0;
}

AVIOContext * content_bridge(sp<Content>& pipe) {
    AVIOContext * avio = avio_alloc_context((unsigned char *)av_mallocz(4096),
                                            4096,
                                            0,                  // write flag
                                            pipe.get(),         // opaque
                                            content_bridge_read_packet,
                                            NULL,
                                            content_bridge_seek);
    return avio;
}


struct AVStreamObject : public SharedObject {
    AVStream *      stream;
    bool            enabled;
    bool            calc_dts;
    MediaTime       last_dts;
    
    AVStreamObject() : stream(NULL), enabled(true),
    calc_dts(false), last_dts(0) { }
};

struct AVFormatObject : public SharedObject {
    sp<Content>                 pipe;
    AVIOContext *               avio;
    AVFormatContext *           context;
    Vector<sp<AVStreamObject> > streams;
    
    AVFormatObject() : avio(NULL), context(NULL) { }
    
    ~AVFormatObject() {
        if (context) {
            avformat_close_input(&context);
            //avformat_free_context(context);
        }
        if (avio) {
            avio_context_free(&avio);
        }
    }
};

static AVDictionary * prepareOptions(const AVInputFormat * fmt) {
    AVDictionary * dict = NULL;
    // common options, @see libavformat/options_table.h
    av_dict_set_int(&dict, "avioflags", AVIO_SEEKABLE_NORMAL | AVIO_FLAG_DIRECT, 0);
    
    int fflags = AVFMT_FLAG_CUSTOM_IO | AVFMT_FLAG_AUTO_BSF;
    fflags |= AVFMT_FLAG_SORT_DTS;
    fflags |= AVFMT_FLAG_GENPTS;
    av_dict_set_int(&dict, "fflags", fflags, 0);
    
    String name = fmt->name;
    if (name.startsWith("mov")) {
        av_dict_set_int(&dict, "seek_streams_individually", 0, 0);
    } else if (name.startsWith("mp3")) {
        av_dict_set_int(&dict, "usetoc", 1, 0);
    }
    return dict;
}

static sp<AVFormatObject> openInput(sp<Content>& pipe, bool find_stream_info = false) {
    sp<AVFormatObject> object = new AVFormatObject;
    object->pipe = pipe;    // keep a ref
    
    object->avio = content_bridge(pipe);
    CHECK_NULL(object->avio);
    
    AVInputFormat * fmt = NULL;
    int ret = av_probe_input_buffer(object->avio, &fmt, NULL, NULL, 0, 0);
    if (ret < 0) {
        ERROR("probe input failed");
        return NIL;
    }
    DEBUG("input: %s, flags %#x", fmt->name, fmt->flags);
    String name = fmt->name;
    
    object->context = avformat_alloc_context();
    object->context->pb = object->avio;
    
    AVDictionary * options = prepareOptions(fmt);
    ret = avformat_open_input(&object->context, NULL, fmt, &options);
    if (options) av_dict_free(&options);
    
    if (ret < 0) {
        ERROR("open file failed.");
        return NIL;
    }
    av_dump_format(object->context, 0, NULL, 0);
    
    if (find_stream_info) {
        // avformat_find_stream_info not always necessary, it decoding extra info, like
        // image pixel format (yuv420p), SAR, DAR ...
        // audio sample format, channel map ...
        ret = avformat_find_stream_info(object->context, NULL);
        if (ret < 0) {
            ERROR("find stream info failed.");
            return NIL;
        }
        
        av_dump_format(object->context, 0, NULL, 0);
    }
    
    // workarounds
    for (size_t i = 0; i < object->context->nb_streams; ++i) {
        sp<AVStreamObject> st = new AVStreamObject;
        st->stream = object->context->streams[i];
        
        if (name.startsWith("matroska") && st->stream->codecpar->codec_type == AVMEDIA_TYPE_VIDEO) {
            st->calc_dts = true;
        }
        
        object->streams.push(st);
    }
    
    return object;
}

// TODO: we perfer packet in dts order, reorder if out of order
struct AVMediaPacket : public MediaPacket {
    AVPacket *  pkt;
    
    AVMediaPacket(sp<AVStreamObject>& st, AVPacket * ref) {
        pkt = av_packet_alloc();
        av_packet_ref(pkt, ref);
        
        CHECK_EQ(st->stream->index, pkt->stream_index);
        data    = pkt->data;
        size    = pkt->size;
        index   = pkt->stream_index;
        type    = kFrameTypeUnknown;
        if (pkt->flags & AV_PKT_FLAG_KEY)           type |= kFrameTypeSync;
        if (pkt->flags & AV_PKT_FLAG_DISCARD)       type |= kFrameTypeReference;
        if (pkt->flags & AV_PKT_FLAG_DISPOSABLE)    type |= kFrameTypeDisposal;
        
        if (st->calc_dts) {
            if (pkt->duration == 0) {
                ERROR("stream %d: missing duration");
            }
            
            st->last_dts += MediaTime(pkt->duration * st->stream->time_base.num, st->stream->time_base.den);
            dts = st->last_dts;
        } else {
            if (pkt->dts == AV_NOPTS_VALUE) {
                ERROR("stream %d: missing dts", st->stream->index);
                dts = MediaTime(pkt->pts * st->stream->time_base.num, st->stream->time_base.den);
            } else {
                dts = MediaTime(pkt->dts * st->stream->time_base.num, st->stream->time_base.den);
            }
        }
        
        if (pkt->pts == AV_NOPTS_VALUE) {
            pts = dts;
        } else {
            pts = MediaTime(pkt->pts * st->stream->time_base.num, st->stream->time_base.den);
        }
        
        if (pkt->duration > 0) {
            duration = MediaTime(pkt->duration * st->stream->time_base.num, st->stream->time_base.den);
        } else {
            duration = kMediaTimeInvalid;
        }
        
        // opaque
        opaque  = pkt;
    }
    
    virtual ~AVMediaPacket() {
        av_packet_free(&pkt);
    }
};

static sp<Buffer> prepareAVCC(const AVStream * st) {
    BitReader br((const char *)st->codecpar->extradata,
                 st->codecpar->extradata_size);
    MPEG4::AVCDecoderConfigurationRecord avcC(br);
    if (avcC.valid) {
        return new Buffer((const char *)st->codecpar->extradata,
                          st->codecpar->extradata_size);
    } else {
        ERROR("bad avcC");
        return NIL;
    }
}

static sp<Buffer> prepareHVCC(const AVStream * st) {
    // TODO: validation the extradata
    return new Buffer((const char *)st->codecpar->extradata,
                      st->codecpar->extradata_size);
}

struct AVFormat : public MediaFile {
    sp<AVFormatObject>  mObject;
    
    AVFormat() : mObject(NIL) { }
    
    MediaError open(sp<Content>& pipe) {
        mObject = openInput(pipe);
        return mObject.isNIL() ? kMediaErrorUnknown : kMediaNoError;
    }
    
    virtual sp<Message> formats() const {
        sp<Message> info = new Message;
        info->setInt32(kKeyFormat, GetFormat(mObject->context->iformat->name));
        if (mObject->context->duration != AV_NOPTS_VALUE) {
            info->setInt64(kKeyDuration, mObject->context->duration);
        } else {
            ERROR("duration is not available");
        }
        
        info->setInt32(kKeyCount, mObject->context->nb_streams);
        for (size_t i = 0; i < mObject->context->nb_streams; ++i) {
            String name = String::format("track-%zu", i);
            sp<Message> trak = new Message;
            
            AVStream * st = mObject->context->streams[i];
            
            trak->setInt32(kKeyFormat, GetCodecFormat(st->codecpar->codec_id));
            
            if (st->duration != AV_NOPTS_VALUE) {
                trak->setInt64(kKeyDuration, st->duration);
            }
            switch (st->codecpar->codec_type) {
                case AVMEDIA_TYPE_AUDIO:
                    trak->setInt32(kKeyCodecType, kCodecTypeAudio);
                    trak->setInt32(kKeySampleRate, st->codecpar->sample_rate);
                    trak->setInt32(kKeyChannels, st->codecpar->channels);
                    break;
                case AVMEDIA_TYPE_VIDEO:
                    trak->setInt32(kKeyCodecType, kCodecTypeVideo);
                    trak->setInt32(kKeyWidth, st->codecpar->width);
                    trak->setInt32(kKeyHeight, st->codecpar->height);
                    break;
                default:
                    break;
            }
            
            switch (st->codecpar->codec_id) {
                case AV_CODEC_ID_AAC: {
                    sp<Buffer> esds = MPEG4::MakeAudioESDS((const char *)st->codecpar->extradata,
                                                           st->codecpar->extradata_size);
                    if (esds.isNIL()) {
                        MPEG4::eAudioObjectType aot = MPEG4::AOT_AAC_MAIN;
                        switch (st->codecpar->profile) {
                            case FF_PROFILE_AAC_SSR:
                                aot = MPEG4::AOT_AAC_SSR;
                                break;
                            case FF_PROFILE_AAC_LOW:
                                aot = MPEG4::AOT_AAC_LC;
                                break;
                            case FF_PROFILE_AAC_LTP:
                                aot = MPEG4::AOT_AAC_LTP;
                                break;
                            case FF_PROFILE_AAC_MAIN:
                            default:
                                break;
                        }
                        MPEG4::AudioSpecificConfig asc (aot,
                                                        st->codecpar->sample_rate,
                                                        st->codecpar->channels);
                        esds = MPEG4::MakeAudioESDS(asc);
                    }
                    trak->setObject(kKeyESDS, esds);
                } break;
                case AV_CODEC_ID_H264:
                    trak->setObject(kKeyavcC, prepareAVCC(st));
                    break;
                case AV_CODEC_ID_HEVC:
                    trak->setObject(kKeyhvcC, prepareHVCC(st));
                    break;
                default:
                    if (st->codecpar->extradata) {
                        trak->setObject(kKeyCodecSpecificData,
                                        new Buffer((const char *)st->codecpar->extradata,
                                                   st->codecpar->extradata_size));
                    }
                    break;
            }
            
            info->setObject(name, trak);
        }
        return info;
    }
    
    virtual MediaError configure(const sp<Message>& options) {
        INFO("configure << %s", options->string().c_str());
        MediaError status = kMediaErrorInvalidOperation;
        if (options->contains(kKeyTracks)) {
            abe::BitSet mask = options->findInt32(kKeyTracks);
            CHECK_FALSE(mask.empty());
            for (size_t i = 0; i < mObject->streams.size(); ++i) {
                sp<AVStreamObject> st = mObject->streams[i]; // FIXME
                st->enabled = mask.test(i);
            }
            status = kMediaNoError;
        }
        return status;
    }
    
    virtual sp<MediaPacket> read(const eReadMode& mode, const MediaTime& ts) {
        if (ts != kMediaTimeInvalid) {
            INFO("seek to %.3f(s)", ts.seconds());
            avformat_seek_file(mObject->context, -1,
                               0,
                               ts.useconds(),
                               mObject->context->duration,
                               0);
        }
        
        AVPacket * pkt = av_packet_alloc();
        pkt->data = NULL; pkt->size = 0;
        
        int ret = av_read_frame(mObject->context, pkt);
        
        if (ret < 0) {
            ERROR("av_read_frame error %s", av_error_string(ret));
            av_packet_free(&pkt);
            return NIL;
        }
        
        if (pkt->flags & AV_PKT_FLAG_CORRUPT) {
            WARN("corrupt packet");
            return read(mode, kMediaTimeInvalid);
        }
        
        sp<AVStreamObject> st = mObject->streams[pkt->stream_index];
        if (st->enabled == false) return read(mode, kMediaTimeInvalid);
        
        sp<MediaPacket> packet = new AVMediaPacket(st, pkt);
        av_packet_free(&pkt);
        
        DEBUG("trak %zu: %zu bytes@%p, %.3f(s)", packet->index, packet->size, packet->data, packet->dts.seconds());
        
        return packet;
    }
};

sp<MediaFile> CreateLibavformat(sp<Content>& pipe) {
    sp<AVFormat> file = new AVFormat;
    if (file->open(pipe) == kMediaNoError) return file;
    return NIL;
}

__END_NAMESPACE_MPX
