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


// File:    Decoder.cpp
// Author:  mtdcy.chen
// Changes: 
//          1. 20160701     initial version
//

#include <FFmpeg.h>

#define LOG_TAG "Lavc"
//#define LOG_NDEBUG 0
#include "MediaTypes.h"
#include "MediaDevice.h"

#include <mpeg4/Systems.h>
#include <mpeg4/Audio.h>
#include "microsoft/Microsoft.h"

// 57.35
// avcodec_send_packet && avcodec_receive_frame
#if LIBAVCODEC_VERSION_INT < AV_VERSION_INT(57,35,100)
#pragma message("add support for version " LIBAVCODEC_IDENT)
#error "add support for old version"
#endif

__BEGIN_NAMESPACE_MPX

struct {
    UInt32        a;
    AVCodecID       b;
} kCodecMap[] = {
    // audio
    {kAudioCodecAAC,            AV_CODEC_ID_AAC         },
    {kAudioCodecMP3,            AV_CODEC_ID_MP3         },
    {kAudioCodecAPE,            AV_CODEC_ID_APE         },
    {kAudioCodecFLAC,           AV_CODEC_ID_FLAC        },
    {kAudioCodecWMA,            AV_CODEC_ID_WMAV2       },
    {kAudioCodecVorbis,         AV_CODEC_ID_VORBIS      },
    {kAudioCodecDTS,            AV_CODEC_ID_DTS         },
    {kAudioCodecAC3,            AV_CODEC_ID_AC3         },

    // video
    {kVideoCodecH263,           AV_CODEC_ID_H263        },
    {kVideoCodecH264,           AV_CODEC_ID_H264        },
    {kVideoCodecHEVC,           AV_CODEC_ID_H265        },
    {kVideoCodecMPEG4,          AV_CODEC_ID_MPEG4       },
    {kVideoCodecVC1,            AV_CODEC_ID_VC1         },
    {kVideoCodecMicrosoftMPEG4, AV_CODEC_ID_MSMPEG4V2   },

    // END OF LIST
    {kAudioCodecUnknown,        AV_CODEC_ID_NONE        }
};

static UInt32 get_codec_format(AVCodecID b) {
    for (UInt32 i = 0; kCodecMap[i].b != AV_CODEC_ID_NONE; ++i) {
        if (kCodecMap[i].b == b)
            return kCodecMap[i].a;
    }
    FATAL("fix the map <= %s", avcodec_get_name(b));
    return kAudioCodecUnknown;
}

static AVCodecID get_av_codec_id(UInt32 a) {
    for (UInt32 i = 0; kCodecMap[i].b != AV_CODEC_ID_NONE; ++i) {
        if (kCodecMap[i].a == a)
            return kCodecMap[i].b;
    }
    FATAL("fix the map <= %#x", a);
    return AV_CODEC_ID_NONE;
}

#ifdef __APPLE__
#define HWACCEL_PIX_FMT     AV_PIX_FMT_VIDEOTOOLBOX
#else
#define HWACCEL_PIX_FMT     AV_PIX_FMT_NONE     // TODO
#endif
struct {
    ePixelFormat    a;
    AVPixelFormat   b;
} kPixelMap[] = {
    {kPixelFormat420YpCbCrPlanar,       AV_PIX_FMT_YUV420P},
    {kPixelFormat420YpCbCrSemiPlanar,   AV_PIX_FMT_NV12},
    {kPixelFormat420YpCrCbSemiPlanar,   AV_PIX_FMT_NV21},
    {kPixelFormatRGB565,                AV_PIX_FMT_RGB565},
    {kPixelFormatRGB,                   AV_PIX_FMT_RGB24},
    // Hardware accel
#ifdef __APPLE__
    {kPixelFormat420YpCbCrSemiPlanar,   AV_PIX_FMT_VIDEOTOOLBOX},
#endif
    // END OF LIST
    {kPixelFormatUnknown,               AV_PIX_FMT_NONE}
};

static ePixelFormat get_pix_format(AVPixelFormat b) {
    for (UInt32 i = 0; kPixelMap[i].b != AV_PIX_FMT_NONE; ++i) {
        if (kPixelMap[i].b == b)
            return kPixelMap[i].a;
    }
    ERROR("fix the map <= %s", av_get_pix_fmt_name(b));
    return kPixelFormatUnknown;
}

static AVPixelFormat get_av_pix_format(ePixelFormat a) {
    for (UInt32 i = 0; kPixelMap[i].a != kPixelFormatUnknown; ++i) {
        if (kPixelMap[i].a == a)
            return kPixelMap[i].b;
    }
    FATAL("fix the map");
    return AV_PIX_FMT_NONE;
}

struct {
    eSampleFormat   a;
    AVSampleFormat  b;
} kSampleMap[] = {
    // plannar goes first as we prefer plannar
    {kSampleFormatS16,          AV_SAMPLE_FMT_S16P},
    {kSampleFormatS32,          AV_SAMPLE_FMT_S32P},
    {kSampleFormatF32,          AV_SAMPLE_FMT_FLTP},
    {kSampleFormatF64,          AV_SAMPLE_FMT_DBLP},
    // packed
    {kSampleFormatS16,          AV_SAMPLE_FMT_S16},
    {kSampleFormatS32,          AV_SAMPLE_FMT_S32},
    {kSampleFormatF32,          AV_SAMPLE_FMT_FLT},
    {kSampleFormatF64,          AV_SAMPLE_FMT_DBL},
    // END OF LIST
    {kSampleFormatUnknown,      AV_SAMPLE_FMT_NONE},
};

static eSampleFormat get_sample_format(AVSampleFormat b) {
    for (UInt32 i = 0; kSampleMap[i].b != AV_SAMPLE_FMT_NONE; ++i) {
        if (kSampleMap[i].b == b) return kSampleMap[i].a;
    }
    //FATAL("fix the map");
    //return kSampleFormatUnknown;
    return kSampleFormatS16;    // default one
}

static AVSampleFormat get_av_sample_format(eSampleFormat a) {
    for (UInt32 i = 0; kSampleMap[i].a != kSampleFormatUnknown; ++i) {
        if (kSampleMap[i].a == a) return kSampleMap[i].b;
    }
    FATAL("fix the map");
    return AV_SAMPLE_FMT_NONE;
}

template <typename TYPE>
static FORCE_INLINE UInt32 unpack(AVFrame *frame, sp<MediaFrame>& out) {
    const TYPE * in = (const TYPE *)frame->data[0];
    for (UInt32 i = 0; i < frame->nb_samples; ++i) {
        for (UInt32 ch = 0; ch < frame->channels; ++ch) {
            ((TYPE *)out->planes.buffers[ch].data)[i]   = *in++;
        }
    }
    return frame->nb_samples;
}

static FORCE_INLINE sp<MediaFrame> unpack(AVFrame * frame, AVCodecContext* avcc) {
    sp<MediaFrame> out;
    AudioFormat format;
    format.channels     = frame->channels;
    format.freq         = frame->sample_rate;
    format.samples      = frame->nb_samples;
    switch (frame->format) {
        case AV_SAMPLE_FMT_U8:
            format.format = kSampleFormatU8;
            out = MediaFrame::Create(format);
            out->audio.samples = unpack<UInt8>(frame, out);
            break;
        case AV_SAMPLE_FMT_S16:
            format.format = kSampleFormatS16;
            out = MediaFrame::Create(format);
            out->audio.samples = unpack<Int16>(frame, out);
            break;
        case AV_SAMPLE_FMT_S32:
            format.format = kSampleFormatS32;
            out = MediaFrame::Create(format);
            out->audio.samples = unpack<Int32>(frame, out);
            break;
        case AV_SAMPLE_FMT_FLT:
            format.format = kSampleFormatF32;
            out = MediaFrame::Create(format);
            out->audio.samples = unpack<Float32>(frame, out);
            break;
        case AV_SAMPLE_FMT_DBL:
            format.format = kSampleFormatF64;
            out = MediaFrame::Create(format);
            out->audio.samples = unpack<Float64>(frame, out);
            break;
        default:
            FATAL("FIXME");
            break;
    }
    out->timecode   = MediaTime(frame->pts * avcc->pkt_timebase.num, avcc->pkt_timebase.den);
    out->duration   = kMediaTimeInvalid;
    return out;
}

// map AVFrame to MediaFrame, so we don't have to realloc memory again
struct AVMediaFrame : public MediaFrame {
    MediaBuffer     extended_buffers[AV_NUM_DATA_POINTERS-1];   // placeholder
    
    AVMediaFrame(AVCodecContext *avcc, AVFrame *frame) : MediaFrame() {
        opaque = av_frame_alloc();
        av_frame_ref((AVFrame*)opaque, frame);

        if (avcc->codec_type == AVMEDIA_TYPE_AUDIO) {
            audio.format        = get_sample_format((AVSampleFormat)frame->format);
            audio.channels      = frame->channels;
            audio.freq          = frame->sample_rate;
            audio.samples       = frame->nb_samples;
            if (av_sample_fmt_is_planar((AVSampleFormat)frame->format)) {
                for (UInt32 i = 0; i < frame->channels; ++i) {
                    planes.buffers[i].data  = frame->data[i];
                    // linesize may have extra bytes.
                    planes.buffers[i].size  = frame->nb_samples * av_get_bytes_per_sample((AVSampleFormat)frame->format);
                }
            } else {
                planes.buffers[0].data  = frame->data[0];
                planes.buffers[0].size  = frame->linesize[0];
            }
        } else if (avcc->codec_type == AVMEDIA_TYPE_VIDEO) {
            video.format        = get_pix_format((AVPixelFormat)frame->format);
            const PixelDescriptor * desc = GetPixelFormatDescriptor(video.format);
            
            video.width         = frame->linesize[0];
            video.height        = frame->height;
            video.rect.x        = 0;
            video.rect.y        = 0;
            video.rect.w        = avcc->width;
            video.rect.h        = avcc->height;
            for (UInt32 i = 0; frame->data[i] != Nil; ++i) {
                planes.buffers[i].data  = frame->data[i];
                // frame->linesize[i] is wired, can not used to calc plane bytes
                planes.buffers[i].capacity =
                planes.buffers[i].size  = (video.width * video.height * desc->planes[i].bpp) / (8 * desc->planes[i].hss * desc->planes[i].vss);
            }
        } else {
            FATAL("FIXME");
        }
        // this may be wrong
        timecode    = MediaTime(frame->pts * avcc->pkt_timebase.num, avcc->pkt_timebase.den);
        duration    = kMediaTimeInvalid;
    }

    virtual ~AVMediaFrame() {
        av_frame_free((AVFrame**)&opaque);
    }
};

static AVPixelFormat get_format(AVCodecContext *avcc, const AVPixelFormat *pix_fmts) {
    const AVPixelFormat *pix;
    for (pix = pix_fmts; *pix != AV_PIX_FMT_NONE; ++pix) {
        INFO("pix %s", av_get_pix_fmt_name(*pix));
        const AVPixFmtDescriptor *desc = av_pix_fmt_desc_get(*pix);
        // find the pix for hwaccel
        if (!(desc->flags & AV_PIX_FMT_FLAG_HWACCEL)) continue;

        // find the right hwaccel config
        const AVCodecHWConfig *hw = Nil;
        for (UInt32 i = 0;; ++i) {
            hw = avcodec_get_hw_config(avcc->codec, i);
            if (!hw) break;
            if (!(hw->methods & AV_CODEC_HW_CONFIG_METHOD_HW_DEVICE_CTX)) continue;
            if (hw->pix_fmt == *pix) break;
        }

        // init hwaccel context
        if (hw) {
            INFO("hwaccel %#x for pix %s, type %s",
                    hw->methods,
                    av_get_pix_fmt_name(*pix),
                    av_hwdevice_get_type_name(hw->device_type));

            return *pix;

            // no need to alloc hwaccel_context manually
#if 0 // def __APPLE__
            CHECK_EQ((Int)hw->device_type, (Int)AV_HWDEVICE_TYPE_VIDEOTOOLBOX);
            Int rt = av_videotoolbox_default_init(avcc);
            if (rt < 0) {
                Char err_str[64];
                ERROR("init hw accel context failed. %s", av_make_error_string(err_str, 64, rt));
                av_buffer_unref(&avcc->hw_device_ctx);
                av_videotoolbox_default_free(avcc);
                break;  // using default methold
            }
            if (avcc->hwaccel_context) return *pix;
#endif
        }
    }
    INFO("can't enable hwaccel");
    return avcodec_default_get_format(avcc, pix_fmts);
}

static Int get_buffer(AVCodecContext *avcc, AVFrame *frame, Int flags) {
    // TODO
    return avcodec_default_get_buffer2(avcc, frame, flags);
}

static MediaError setupHwAccelContext(AVCodecContext *avcc) {

    INFO("supported hwaccel: ");
    AVBufferRef *hw_device_ctx = Nil;
    for (Int i = 0; ; ++i) {
        const AVCodecHWConfig *hwc = avcodec_get_hw_config(avcc->codec, i);
        if (hwc == Nil) break;
        INFO("\tpix_fmt %s, method %#x, device type %s",
                av_get_pix_fmt_name(hwc->pix_fmt),
                hwc->methods,
                av_hwdevice_get_type_name(hwc->device_type));

#ifdef __APPLE__
        if (hwc->device_type != AV_HWDEVICE_TYPE_VIDEOTOOLBOX) continue;
#endif

        if (hwc->methods & AV_CODEC_HW_CONFIG_METHOD_HW_DEVICE_CTX) {
            Int rt = av_hwdevice_ctx_create(&hw_device_ctx,
                    hwc->device_type,
                    Nil, Nil, 0);
            if (rt < 0) {
                Char err_str[64];
                ERROR("create hw device failed. %s", av_make_error_string(err_str, 64, rt));
            } else {
                INFO("\t => hw_device_ctx init successful");
                break;
            }
        }
    }

    if (hw_device_ctx) {
        avcc->hw_device_ctx = av_buffer_ref(hw_device_ctx);
        avcc->get_format = get_format;
        av_buffer_unref(&hw_device_ctx);
    }
    return kMediaNoError;
}

static void parseAudioSpecificConfig(AVCodecContext *avcc, const sp<Buffer>& csd) {
    MPEG4::AudioSpecificConfig asc(csd);
    csd->resetBytes();
    if (asc.valid) {
        avcc->extradata_size = csd->size();
        CHECK_GE(avcc->extradata_size, 2);
        avcc->extradata = (UInt8*)av_mallocz(avcc->extradata_size +
                AV_INPUT_BUFFER_PADDING_SIZE);
        memcpy(avcc->extradata, csd->data(),
                avcc->extradata_size);
    } else {
        ERROR("bad AudioSpecificConfig");
    }
}

// parse AudioSpecificConfig from esds, for who needs AudioSpecificConfig
static void parseESDS(AVCodecContext *avcc, const sp<Buffer>& esds) {
    sp<MPEG4::ESDescriptor> esd = MPEG4::ReadESDS(esds);
    if (esd.isNil() || esd->decConfigDescr->decSpecificInfo.isNil()) {
        ERROR("bad esds");
        return;
    }
    parseAudioSpecificConfig(avcc, esd->decConfigDescr->decSpecificInfo->csd);
}

static MediaError setupExtraData(AVCodecContext *avcc, const sp<Message>& formats) {
    // different extra data for  different decoder
    switch (avcc->codec->id) {
        case AV_CODEC_ID_AAC:
            if (formats->contains(kKeyESDS)) {
                sp<Buffer> esds = formats->findObject(kKeyESDS);
                parseESDS(avcc, esds->cloneBytes());
                // aac sbr have real sample rate in AudioSpecificConfig
                // but, DON'T fix avcc->sample_rate here
            } else if (formats->contains(kKeyCodecSpecData)) {
                sp<Buffer> csd = formats->findObject(kKeyCodecSpecData);
                parseAudioSpecificConfig(avcc, csd->cloneBytes());
            } else {
                ERROR("missing esds|csd for aac");
                return kMediaErrorUnknown;
            }
            break;
        case AV_CODEC_ID_H264:
            if (formats->contains(kKeyavcC)) {
                sp<Buffer> avcC = formats->findObject(kKeyavcC);
                // h264 decoder
                avcc->extradata_size = avcC->size();
                avcc->extradata = (UInt8*)av_mallocz(avcc->extradata_size +
                        AV_INPUT_BUFFER_PADDING_SIZE);
                memcpy(avcc->extradata, avcC->data(), avcc->extradata_size);
            } else {
                ERROR("missing avcC for h264");
                return kMediaErrorUnknown;
            }
            break;
        case AV_CODEC_ID_HEVC:
            if (formats->contains(kKeyhvcC)) {
                sp<Buffer> hvcC = formats->findObject(kKeyhvcC);
                avcc->extradata_size = hvcC->size();
                avcc->extradata = (UInt8*)av_mallocz(avcc->extradata_size +
                        AV_INPUT_BUFFER_PADDING_SIZE);
                memcpy(avcc->extradata, hvcC->data(), avcc->extradata_size);
            } else {
                ERROR("missing hvcC for hevc");
                return kMediaErrorUnknown;
            } break;
        default:
            break;
    }

    // FIXME: ffmpeg have different codec id for different profile/version

    return kMediaNoError;
}

void av_log_callback(void *avcl, Int level, const Char *fmt, va_list vl) {

    level &= 0xff;

#if LOG_NDEBUG == 0
    // NOTHING
#else
    if (level > AV_LOG_ERROR) return;
#endif

    static Int print_prefix = 1;
    Char line[1024] = { 0 };
    AVClass* avc = avcl ? *(AVClass **) avcl : Nil;

    if (print_prefix && avc) {
        if (avc->parent_log_context_offset) {
            AVClass** parent = *(AVClass ***) (((UInt8 *) avcl) +
                    avc->parent_log_context_offset);
            if (parent && *parent) {
                snprintf(line, sizeof(line), "[%s @ %p] ",
                        (*parent)->item_name(parent), parent);
            }
        }
        snprintf(line + strlen(line), sizeof(line) - strlen(line), "[%s @ %p] ",
                avc->item_name(avcl), avcl);
    }

    vsnprintf(line + strlen(line), sizeof(line) - strlen(line), fmt, vl);

    // we will add \n automatically
    line[strlen(line)-1] = '\0';

    INFO("%s", line);
}

static FORCE_INLINE void releaseContext(AVCodecContext * avcc) {
    if (avcc) {
#if 0 // no need to free hwaccel_context manually
        if (avcc->hwaccel_context) {
#ifdef __APPLE__
            av_videotoolbox_default_free(avcc);
#else
            FATAL("FIXME: free hwaccel context");
#endif
        }
#endif
        avcodec_free_context(&avcc);
    }
}

static FORCE_INLINE MediaError openAudio(AVCodecContext * avcc, const sp<Message>& formats, const sp<Message>& options) {
    AVSampleFormat best_match   = AV_SAMPLE_FMT_NONE;
    eSampleFormat requested     = (eSampleFormat)options->findInt32(kKeyRequestFormat, kSampleFormatS16);
    const AVSampleFormat request_sample_fmt = get_av_sample_format(requested);
    CHECK_TRUE(av_sample_fmt_is_planar(request_sample_fmt));
    
    // find best sample format
    if (avcc->codec->sample_fmts) {
        Bool match = False;
        // match planar ?
        for (UInt32 i = 0; avcc->codec->sample_fmts[i] != AV_SAMPLE_FMT_NONE; ++i) {
            // match the first planar format
            if (best_match == AV_SAMPLE_FMT_NONE && av_sample_fmt_is_planar(avcc->codec->sample_fmts[i])) {
                best_match = avcc->codec->sample_fmts[i];
            }
            // match the request format ?
            if (avcc->codec->sample_fmts[i] == request_sample_fmt) {
                best_match = avcc->codec->sample_fmts[i];
                match = True;
                break;
            }
        }
        
        // planar -> packed
        if (!match) {
            // take the first format
            if (best_match == AV_SAMPLE_FMT_NONE) best_match = avcc->codec->sample_fmts[0];
            const AVSampleFormat packed = av_get_alt_sample_fmt(request_sample_fmt, False);
            CHECK_FALSE(av_sample_fmt_is_planar(packed));
            for (UInt32 i = 0; avcc->codec->sample_fmts[i] != AV_SAMPLE_FMT_NONE; ++i) {
                if (avcc->codec->sample_fmts[i] == packed) {
                    best_match = avcc->codec->sample_fmts[i];
                    break;
                }
            }
        }
    } else {
        best_match = request_sample_fmt;
    }
    
    // TODO: request channel layout
    
    avcc->channels              = formats->findInt32(kKeyChannels);
    avcc->sample_rate           = formats->findInt32(kKeySampleRate);
    avcc->request_channel_layout = AV_CH_LAYOUT_STEREO;
    avcc->request_sample_fmt    = best_match;
    avcc->pkt_timebase.num      = 1;
    avcc->pkt_timebase.den      = avcc->sample_rate;
    INFO("request_sample_fmt %s", av_get_sample_fmt_name(avcc->request_sample_fmt));
    
    Int ret = avcodec_open2(avcc, Nil, Nil);
    
    if (ret < 0) {
        Char err_str[64];
        ERROR("%s: avcodec_open2 failed, error %s", avcodec_get_name(avcc->codec_id),
              av_make_error_string(err_str, 64, ret));
        return kMediaErrorUnknown;
    }
    
    INFO("codec %s open success with %d threads, type %d",
         avcc->codec->name,
         avcc->thread_count,
         avcc->active_thread_type);
    
    INFO("sample_fmt %s", av_get_sample_fmt_name(avcc->sample_fmt));
    INFO("channels %d, channel layout %d", avcc->channels, avcc->channel_layout);
    INFO("sample_rate %d", avcc->sample_rate);
    
    return kMediaNoError;
}

MediaError openVideo(AVCodecContext * avcc, eModeType mode, const sp<Message>& formats, const sp<Message>& options) {
    Bool hwaccel = mode != kModeTypeSoftware;
    
    // find best pixel format
    AVPixelFormat best_match = AV_PIX_FMT_YUV420P;
    AVPixelFormat pix_fmt   = get_av_pix_format((ePixelFormat)options->findInt32(kKeyRequestFormat, kPixelFormat420YpCbCrPlanar));
    if (avcc->codec->pix_fmts) {
        best_match = avcc->codec->pix_fmts[0];
        for (UInt32 i = 0; avcc->codec->pix_fmts[i] != AV_PIX_FMT_NONE; ++i) {
            if (avcc->codec->pix_fmts[i] == pix_fmt) {
                best_match = pix_fmt;
                break;
            }
        }
    } else {
        struct {
            AVCodecID       codec;
            AVPixelFormat   pixel;
        } kMap[] = {
            {AV_CODEC_ID_H264,      AV_PIX_FMT_YUV420P  },
            {AV_CODEC_ID_NONE,      AV_PIX_FMT_NONE     },
        };
        for (UInt32 i = 0; kMap[i].codec != AV_CODEC_ID_NONE; ++i) {
            if (kMap[i].codec == avcc->codec->id) {
                best_match = kMap[i].pixel;
                break;
            }
        }
    }
    
    // setup context
    avcc->width                 = formats->findInt32(kKeyWidth);
    avcc->height                = formats->findInt32(kKeyHeight);
    avcc->coded_width           = avcc->width;
    avcc->coded_height          = avcc->height;
    avcc->pix_fmt               = best_match;
    
    if (hwaccel) setupHwAccelContext(avcc);
    
    Int ret = avcodec_open2(avcc, Nil, Nil);
    
    if (ret < 0) {
        Char err_str[64];
        ERROR("%s: avcodec_open2 failed, error %s", avcodec_get_name(avcc->codec_id),
              av_make_error_string(err_str, 64, ret));
        return kMediaErrorUnknown;
    }
    
    INFO("codec %s open success with %d threads, type %d",
         avcc->codec->name,
         avcc->thread_count,
         avcc->active_thread_type);
    
    INFO("w %d h %d, codec w %d h %d", avcc->width, avcc->height,
         avcc->coded_width, avcc->coded_height);
    INFO("pix_fmt %s", av_get_pix_fmt_name(avcc->pix_fmt));
#if 0
    // XXX: avcc->hwaccel is init after avcodec_open by libavcodec
    // FIXME: how to known wether hw init successful or not
    if (avcc->hwaccel) {
        INFO("hwaccel: %s %s %s",
             avcc->hwaccel,
             avcodec_get_name(avcc->hwaccel->id),
             av_get_pix_fmt_name(avcc->hwaccel->pix_fmt));
    } else {
        if (hwaccel) {
            WARN("initial hwaccel failed");
        } else {
            INFO("no hwaccel");
        }
    }
#endif
    // FIXME:
    if (get_pix_format(avcc->hw_device_ctx ? HWACCEL_PIX_FMT : avcc->pix_fmt) == kPixelFormatUnknown) {
        ERROR("pixel format %s is not supported", av_get_pix_fmt_name(avcc->pix_fmt));
        return kMediaErrorNotSupported;
    }
    
    return kMediaNoError;
}

static AVCodecContext * initContext(eModeType mode, const sp<Message>& formats, const sp<Message>& options) {
    CHECK_TRUE(formats->contains(kKeyFormat));
    
#if LOG_NDEBUG == 0
    av_log_set_level(AV_LOG_VERBOSE);
    av_log_set_callback(av_log_callback);
#endif
    
    CHECK_TRUE(formats->contains(kKeyType));
    eCodecType type = (eCodecType)formats->findInt32(kKeyType);
    UInt32 codec = formats->findInt32(kKeyFormat);
    AVCodecID id = get_av_codec_id(codec);
    
#if 1
    // distinguish difference sub codecs of micorsoft MPEG4
    if (codec == kVideoCodecMicrosoftMPEG4) {
        CHECK_TRUE(formats->contains(kKeyMicrosoftVCM));
        sp<Buffer> vcm = formats->findObject(kKeyMicrosoftVCM);
        Microsoft::BITMAPINFOHEADER biHEAD;
        if (biHEAD.parse(vcm->cloneBytes()) != kMediaNoError) {
            ERROR("bad BITMAPINFOHEADER");
        } else {
            // DO THINGS HERE
        }
    }
#endif
    
    AVCodec *avc = avcodec_find_decoder(id);
    
#if 1 // force fixed decoder if available
    String name = avc->name;
    AVCodec * fixed = Nil;
    if (name.endsWith("Float32")) {
        name.replace("Float32", "");
        fixed = avcodec_find_decoder_by_name(name.c_str());
        if (!fixed) {
            name.append("fixed");
            fixed = avcodec_find_decoder_by_name(name.c_str());
        }
    } else {
        name.append("_fixed");
        fixed = avcodec_find_decoder_by_name(name.c_str());
    }
    if (fixed) {
        INFO("force to fixed AVCodec %s", name.c_str());
        avc = fixed;
    }
#endif
    INFO("[%.4s] -> [%s][%s]", (const Char*)&codec, avcodec_get_name(id), avc->name);
    
    if (!avc) {
        ERROR("can't find codec for %s", avcodec_get_name(id));
        return Nil;
    }
    
    AVCodecContext *avcc = avcodec_alloc_context3(avc);
    if (!avcc) {
        ERROR("[OOM] alloc context failed");
        return Nil;
    }
    
    setupExtraData(avcc, formats);
    
    // setup ffmpeg common context
    avcc->workaround_bugs       = FF_BUG_AUTODETECT;
    avcc->lowres                = 0;
    avcc->idct_algo             = FF_IDCT_AUTO;
    avcc->skip_frame            = AVDISCARD_DEFAULT;
    avcc->skip_idct             = AVDISCARD_DEFAULT;
    avcc->skip_loop_filter      = AVDISCARD_DEFAULT;
    avcc->error_concealment     = 3;
#if LIBAVCODEC_VERSION_INT < AV_VERSION_INT(57,106,102)
    avcc->refcounted_frames     = 1;
#endif
    if (mode != kModeTypePreview) {
        avcc->thread_count      = 4;    // more threads means big waiting time.
        avcc->thread_type       = FF_THREAD_FRAME | FF_THREAD_SLICE;
    } else {
        avcc->thread_count      = 1;
    }
    avcc->pkt_timebase.num      = 1;
    avcc->pkt_timebase.den      = 1000000LL;
    
    MediaError st = kMediaNoError;
    if (type == kCodecTypeAudio) {
        st = openAudio(avcc, formats, options);
    } else if (type == kCodecTypeVideo) {
        st = openVideo(avcc, mode, formats, options);
    } else {
        ERROR("unknown codec type %#x", type);
        st = kMediaErrorNotSupported;
    }
    
    if (st != kMediaNoError) {
        releaseContext(avcc);
        return Nil;
    }
    
    return avcc;
}

#ifdef __APPLE__
sp<MediaFrame> readVideoToolboxFrame(CVPixelBufferRef);
#endif

struct LavcDecoder : public MediaDevice {
    AVCodecContext *        mContext;

    // statistics
    UInt32                  mInputCount;
    UInt32                  mOutputCount;

    LavcDecoder() : MediaDevice(),
    mContext(Nil),
    mInputCount(0),
    mOutputCount(0) { }

    virtual ~LavcDecoder() {
        releaseContext(mContext);
        mContext = Nil;
    }

    virtual MediaError init(const sp<Message>& formats, const sp<Message>& options) {
        INFO("create lavc for %s", formats->string().c_str());
        eModeType mode = (eModeType)options->findInt32(kKeyMode, kModeTypeDefault);
        mContext = initContext(mode, formats, options);
        if (mContext)   return kMediaNoError;
        else            return kMediaErrorNotSupported;
    }

    virtual sp<Message> formats() const {
        sp<Message> info = new Message;
        AVCodecContext* avcc = mContext;
        if (avcc->codec_type == AVMEDIA_TYPE_AUDIO) {
            info->setInt32(kKeyFormat, get_sample_format(avcc->sample_fmt));
            info->setInt32(kKeyChannels, avcc->channels);
            info->setInt32(kKeySampleRate, avcc->sample_rate);

            // FIX sample rate of AAC SBR
            if (avcc->codec_id == AV_CODEC_ID_AAC &&
                    avcc->extradata_size >= 2) {
                sp<Buffer> csd = new Buffer((const Char *)avcc->extradata,
                        (UInt32)avcc->extradata_size);
                MPEG4::AudioSpecificConfig config(csd);
                if (config.valid && config.sbr) {
                    INFO("fix sample rate %d => %d",
                            avcc->sample_rate,
                            config.extSamplingFrquency);
                    info->setInt32(kKeySampleRate,
                            config.extSamplingFrquency);
                }
            }
        } else if (avcc->codec_type == AVMEDIA_TYPE_VIDEO) {
            info->setInt32(kKeyFormat, get_pix_format(avcc->hw_device_ctx ? HWACCEL_PIX_FMT : avcc->pix_fmt));
            info->setInt32(kKeyWidth, avcc->width);
            info->setInt32(kKeyHeight, avcc->height);
        } else {
            FATAL("unknown codec type %#x", avcc->codec_type);
        }
        return info;
    }

    virtual MediaError configure(const sp<Message>& options) {
        return kMediaErrorNotSupported;
    }

    virtual MediaError push(const sp<MediaFrame>& input) {
        AVCodecContext *avcc = mContext;
        if (input != Nil && input->planes.buffers[0].data != Nil) {
            ++mInputCount;

            DEBUG("push %s", input->string().c_str());

            AVPacket *pkt   = av_packet_alloc();
            pkt->data       = input->planes.buffers[0].data;
            pkt->size       = input->planes.buffers[0].size;

            CHECK_TRUE(input->timecode != kMediaTimeInvalid);
            pkt->pts        = MediaTime(input->timecode).rescale(avcc->pkt_timebase.den).value;
            pkt->dts        = AV_NOPTS_VALUE;

            pkt->flags      = 0;
            if (input->flags & kFrameTypeSync) {
                pkt->flags |= AV_PKT_FLAG_KEY;
            }

            if (input->flags & kFrameTypeReference) {
                pkt->flags |= AV_PKT_FLAG_DISCARD;
            }
            
            if (input->flags & kFrameTypeDisposal) {
                pkt->flags |= AV_PKT_FLAG_DISPOSABLE;
            }

            Int ret = avcodec_send_packet(avcc, pkt);
            MediaError err = kMediaNoError;
            if (ret == AVERROR(EAGAIN)) {
                DEBUG("%s: try to read frame", avcodec_get_name(avcc->codec_id));
                err = kMediaErrorResourceBusy;
            } else if (ret == AVERROR(EINVAL)) {
                FATAL("%s: codec is not opened", avcodec_get_name(avcc->codec_id));
            } else if (ret < 0) {
                Char err_str[64];
                FATAL("%s: decode return error %s", avcodec_get_name(avcc->codec_id),
                        av_make_error_string(err_str, 64, ret));
                err = kMediaErrorUnknown;
            }

            av_packet_free(&pkt);
            return err;
        } else {
            // codec enter draining mode
            DEBUG("%s: flushing...", avcodec_get_name(avcc->codec_id));
            avcodec_send_packet(avcc, Nil);
            return kMediaNoError;
        }
    }

    virtual sp<MediaFrame> pull() {
        AVFrame *internal = av_frame_alloc();
        AVCodecContext *avcc = mContext;

        // receive frame from avcodec
        Int ret = avcodec_receive_frame(avcc, internal);
        if (ret == AVERROR(EAGAIN)) {
            INFO("%s: need more input %zu",
                    avcodec_get_name(avcc->codec_id), mInputCount);
            return Nil;
        } else if (ret == AVERROR_EOF) {
            INFO("%s: eos...", avcodec_get_name(avcc->codec_id));
            return Nil;
        } else if (ret == AVERROR(EINVAL)) {
            FATAL("%s: codec is not opened", avcodec_get_name(avcc->codec_id));
        } else if (ret < 0) {
            Char err_str[64];
            ERROR("%s: decode return error %s", avcodec_get_name(avcc->codec_id),
                    av_make_error_string(err_str, 64, ret));
            return Nil;
        }

        sp<MediaFrame> out;
        
        // unpack interleaved -> planar
        if (avcc->codec_type == AVMEDIA_TYPE_AUDIO && !av_sample_fmt_is_planar((AVSampleFormat)internal->format)) {
            out = unpack(internal, avcc);
        } else
#ifdef __APPLE__
        if (internal->format == AV_PIX_FMT_VIDEOTOOLBOX) {
            out = readVideoToolboxFrame((CVPixelBufferRef)internal->data[3]);
            // fix timecode
            out->timecode   = MediaTime(internal->pts * avcc->time_base.num, avcc->time_base.den);
            out->duration   = kMediaTimeInvalid;
        } else
#endif
        {
            out = new AVMediaFrame(avcc, internal);
        }

#if LOG_NDEBUG == 0
        if (avcc->codec_type == AVMEDIA_TYPE_VIDEO) {
            CHECK_EQ(avcc->pix_fmt, internal->format);
            DEBUG("frame %s %.3f(s) => %d x %d => {%d %d %d %d}",
                    av_get_pix_fmt_name((AVPixelFormat)internal->format),
                    out->timecode.seconds(),
                    out->v.width,
                    out->v.height,
                    out->v.rect.x,
                    out->v.rect.y,
                    out->v.rect.w,
                    out->v.rect.h);
        } else {
            DEBUG("frame %s %.3f(s), %d %d nb_samples %d",
                    av_get_sample_fmt_name((AVSampleFormat)internal->format),
                    out->timecode.seconds(),
                    out->a.channels,
                    out->a.freq,
                    internal->nb_samples);
        }
#endif
        if (mOutputCount == 0) {
            INFO("first frame @ %.3f(s)", out->timecode.seconds());
        }

        av_frame_free(&internal);
        ++mOutputCount;
        DEBUG("pull %s", out->string().c_str());
        return out;
    }

    virtual MediaError reset() {
        AVCodecContext *avcc = mContext;
        if (avcc && avcodec_is_open(avcc)) {
            avcodec_flush_buffers(avcc);
        }
        mInputCount = mOutputCount = 0;
        return kMediaNoError;
    }
};

sp<MediaDevice> CreateLavcDecoder(const sp<Message>& formats, const sp<Message>& options) {
    sp<LavcDecoder> lavc = new LavcDecoder;
    if (lavc->init(formats, options) == kMediaNoError) return lavc;
    return Nil;
}
__END_NAMESPACE_MPX
