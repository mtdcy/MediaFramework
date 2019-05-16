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

#define LOG_TAG   "Libavformat"
#define LOG_NDEBUG 0
#include <MediaFramework/MediaFile.h>

#include <FFmpeg.h>

#include "mpeg4/Audio.h"
#include "mpeg4/Video.h"

__BEGIN_NAMESPACE_MPX

struct {
    const char *        name;
    MediaFile::eFormat  format;
} kFormatMap[] = {
    { "mov",        MediaFile::Mp4  },
    { "mp4",        MediaFile::Mp4  },
    { "mp3",        MediaFile::Mp3  },
    // END OF LIST
    { NULL,         MediaFile::Invalid }
};

static MediaFile::eFormat GetFormat(const String& name) {
    for (size_t i = 0; kFormatMap[i].name; ++i) {
        if (name.startsWith(kFormatMap[i].name)) {
            return kFormatMap[i].format;
        }
    }
    return MediaFile::Invalid;
}

struct {
    AVCodecID           id;
    eCodecFormat        format;
} kCodecMap[] = {
    // AUDIO
    { AV_CODEC_ID_PCM_F16LE,    kAudioCodecFormatPCM},
    { AV_CODEC_ID_FLAC,         kAudioCodecFormatFLAC},
    { AV_CODEC_ID_MP3,          kAudioCodecFormatMP3},
    { AV_CODEC_ID_VORBIS,       kAudioCodecFormatVorbis},
    { AV_CODEC_ID_AAC,          kAudioCodecFormatAAC},
    { AV_CODEC_ID_AC3,          kAudioCodecFormatAC3},
    { AV_CODEC_ID_WMV2,         kAudioCodecFormatWMA},
    { AV_CODEC_ID_APE,          kAudioCodecFormatAPE},
    { AV_CODEC_ID_DTS,          kAudioCodecFormatDTS},
    // video
    { AV_CODEC_ID_H264,         kVideoCodecFormatH264},
    { AV_CODEC_ID_H265,         kVideoCodecFormatHEVC},
    { AV_CODEC_ID_MPEG4,        kVideoCodecFormatMPEG4},
    { AV_CODEC_ID_VC1,          kVideoCodecFormatVC1},
    { AV_CODEC_ID_H263,         kVideoCodecFormatH263},
    // image
    { AV_CODEC_ID_PNG,          kImageCodecFormatPNG},
    { AV_CODEC_ID_MJPEG,        kImageCodecFormatJPEG},
    { AV_CODEC_ID_BMP,          kImageCodecFormatBMP},
    { AV_CODEC_ID_GIF,          kImageCodecFormatGIF},
    // END OF LIST
    { AV_CODEC_ID_NONE,         kCodecFormatUnknown}
};

static eCodecFormat GetCodecFormat(AVCodecID id) {
    for (size_t i = 0; kCodecMap[i].id != AV_CODEC_ID_NONE; ++i) {
        if (kCodecMap[i].id == id) return kCodecMap[i].format;
    }
    return kCodecFormatUnknown;
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
    AVStream *              avs;
    List<sp<MediaPacket> >  packets;
    
    AVStreamObject(AVStream * s) : avs(s) {
    }
    
    ~AVStreamObject() {
    }
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
        if (avio) avio_context_free(&avio);
    }
};

static sp<AVFormatObject> openFile(sp<Content>& pipe) {
    sp<AVFormatObject> object = new AVFormatObject;
    object->pipe = pipe;    // keep a ref
    
    object->avio = content_bridge(pipe);
    CHECK_NULL(object->avio);
    
    object->context = avformat_alloc_context();
    object->context->pb = object->avio;
    
    int ret = avformat_open_input(&object->context, NULL, NULL, NULL);
    if (ret < 0) {
        ERROR("open file failed.");
        return NIL;
    }
    av_dump_format(object->context, 0, NULL, 0);
    
#if 0
    // avformat_find_stream_info not always necessary, it decoding extra info, like
    // image pixel format (yuv420p), SAR, DAR ...
    // audio sample format, channel map ...
    ret = avformat_find_stream_info(object->context, NULL);
    if (ret < 0) {
        ERROR("find stream info failed.");
        return NIL;
    }
    
    av_dump_format(object->context, 0, NULL, 0);
#endif
    
    for (size_t i = 0; i < object->context->nb_streams; ++i) {
        object->streams.push(new AVStreamObject(object->context->streams[i]));
    }
    
    return object;
}

// TODO: we perfer packet in dts order, reorder if out of order
struct AVMediaPacket : public MediaPacket {
    AVPacket *  pkt;
    
    AVMediaPacket(AVStream * st, AVPacket * ref) {
        pkt = av_packet_alloc();
        av_packet_ref(pkt, ref);
        
        data    = pkt->data;
        size    = pkt->size;
        //index   = 0;
        format  = GetCodecFormat(st->codecpar->codec_id);
        flags   = 0;
        if (pkt->flags & AV_PKT_FLAG_KEY)           flags |= kFrameFlagSync;
        if (pkt->flags & AV_PKT_FLAG_DISCARD)       flags |= kFrameFlagReference;
        if (pkt->flags & AV_PKT_FLAG_DISPOSABLE)    flags |= kFrameFlagDisposal;
        
        if (pkt->dts == AV_NOPTS_VALUE)
            dts = kTimeInvalid;
        else
            dts = MediaTime(pkt->dts * st->time_base.num, st->time_base.den);
        
        if (pkt->pts == AV_NOPTS_VALUE)
            pts = kTimeInvalid;
        else
            pts = MediaTime(pkt->pts * st->time_base.num, st->time_base.den);
        
        // opaque
        opaque  = pkt;
    }
    
    ~AVMediaPacket() {
        av_packet_unref(pkt);
        av_packet_free(&pkt);
    }
};

struct AVFormat : public MediaFile {
    sp<AVFormatObject>  mObject;
    
    AVFormat() : mObject(NIL) { }
    
    MediaError open(sp<Content>& pipe) {
        mObject = openFile(pipe);
        return mObject.isNIL() ? kMediaErrorUnknown : kMediaNoError;
    }
    
    virtual String string() const { return ""; }
    
    virtual sp<Message> formats() const {
        sp<Message> info = new Message;
        info->setInt32(kKeyFormat, GetFormat(mObject->context->iformat->name));
        info->setInt64(kKeyDuration, mObject->context->duration);
        info->setInt32(kKeyCount, mObject->context->nb_streams);
        
        for (size_t i = 0; i < mObject->context->nb_streams; ++i) {
            String name = String::format("track-%zu", i);
            sp<Message> trak = new Message;
            
            AVStream * st = mObject->context->streams[i];
            
            trak->setInt32(kKeyFormat, GetCodecFormat(st->codecpar->codec_id));
            trak->setInt64(kKeyDuration, st->duration);
            switch (st->codecpar->codec_type) {
                case AVMEDIA_TYPE_AUDIO:
                    trak->setInt32(kKeySampleRate, st->codecpar->sample_rate);
                    trak->setInt32(kKeyChannels, st->codecpar->channels);
                    break;
                case AVMEDIA_TYPE_VIDEO:
                    trak->setInt32(kKeyWidth, st->codecpar->width);
                    trak->setInt32(kKeyHeight, st->codecpar->height);
                    break;
                default:
                    break;
            }
            
            switch (st->codecpar->codec_id) {
                case AV_CODEC_ID_AAC: {
                    // AudioSpecificConfig
                    // TODO: if csd is not exists, make one
                    BitReader br((const char *)st->codecpar->extradata,
                                 st->codecpar->extradata_size);
                    MPEG4::AudioSpecificConfig asc(br);
                    if (asc.valid) {
                        MPEG4::ES_Descriptor esd = MakeESDescriptor(asc);
                        esd.decConfigDescr.decSpecificInfo.csd = new Buffer((const char *)st->codecpar->extradata,
                                                                            st->codecpar->extradata_size);
                        trak->setObject(kKeyESDS, MPEG4::MakeESDS(esd));
                    } else {
                        ERROR("bad AudioSpecificConfig");
                    }
                } break;
                case AV_CODEC_ID_H264: {
                    BitReader br((const char *)st->codecpar->extradata,
                                 st->codecpar->extradata_size);
                    MPEG4::AVCDecoderConfigurationRecord avcC(br);
                    if (avcC.valid) {
                        trak->setObject(kKeyavcC,
                                        new Buffer((const char *)st->codecpar->extradata,
                                                   st->codecpar->extradata_size));
                    } else {
                        ERROR("bad avcC");
                    }
                } break;
                    
                default:
                    break;
            }
            
            info->setObject(name, trak);
        }
        return info;
    }
    
    virtual MediaError configure(const sp<Message>& options) {
        return kMediaErrorInvalidOperation;
    }
    
    virtual sp<MediaPacket> read(size_t index,
                                 eModeReadType mode,
                                 const MediaTime& ts) {
        sp<AVStreamObject> st = mObject->streams[index];
        
        if (st->packets.size()) {
            sp<MediaPacket> packet = st->packets.front();
            st->packets.pop();
            
            return packet;
        }
        
        for (;;) {
            AVPacket pkt;
            av_init_packet(&pkt);
            pkt.data = NULL; pkt.size = 0;
            
            int ret = av_read_frame(mObject->context, &pkt);
            
            if (ret < 0) {
                switch (ret) {
                    case AVERROR_EOF:
                        INFO("End Of File...");
                        return NIL;
                    default:
                        ERROR("av_read_frame error %s", av_err2str(ret));
                        break;
                }
            }
            
            if (pkt.flags & AV_PKT_FLAG_CORRUPT) {
                WARN("corrupt packet");
                continue;
            }
            
            sp<AVStreamObject> st = mObject->streams[pkt.stream_index];
            sp<MediaPacket> packet = new AVMediaPacket(st->avs, &pkt);
            
            if (pkt.stream_index == index) {
                return packet;
            } else {
                // cache the packet.
                st->packets.push(packet);
            }
        }
        
        return NIL;
    }
};

sp<MediaFile> CreateLibavformat(sp<Content>& pipe) {
    sp<AVFormat> file = new AVFormat;
    if (file->open(pipe) == kMediaNoError) return file;
    return NIL;
}

__END_NAMESPACE_MPX
