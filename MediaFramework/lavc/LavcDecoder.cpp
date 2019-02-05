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

#define LOG_TAG "Lavc.Decoder"
//#define LOG_NDEBUG 0
#include <MediaToolkit/Toolkit.h>

#include <MediaFramework/MediaDefs.h>
#include <MediaFramework/MediaDecoder.h>
#include <FFmpeg/FFmpeg.h>

#include <mpeg4/Systems.h>
#include <mpeg4/Audio.h>

// 57.35
// avcodec_send_packet && avcodec_receive_frame
#if LIBAVCODEC_VERSION_INT < AV_VERSION_INT(57,35,100)
#pragma message("add support for version " LIBAVCODEC_IDENT)
#error "add support for old version"
#endif

using namespace mtdcy;

struct {
    eCodecFormat    a;
    AVCodecID       b;
} kCodecMap[] = {
    // audio
    {kAudioCodecFormatAAC,      AV_CODEC_ID_AAC     },
    {kAudioCodecFormatMP3,      AV_CODEC_ID_MP3     },
    {kAudioCodecFormatAPE,      AV_CODEC_ID_APE     },
    {kAudioCodecFormatFLAC,     AV_CODEC_ID_FLAC    },
    {kAudioCodecFormatWMA,      AV_CODEC_ID_WMAV2   },
    {kAudioCodecFormatVorbis,   AV_CODEC_ID_VORBIS  },
    {kAudioCodecFormatDTS,      AV_CODEC_ID_DTS     },

    // video
    {kVideoCodecFormatH264,     AV_CODEC_ID_H264    },
    {kVideoCodecFormatHEVC,     AV_CODEC_ID_H265    },
    {kVideoCodecFormatMPEG4,    AV_CODEC_ID_MPEG4   },
    {kVideoCodecFormatVC1,      AV_CODEC_ID_VC1     },

    // END OF LIST
    {kCodecFormatUnknown,       AV_CODEC_ID_NONE}
};

eCodecFormat get_codec_format(AVCodecID b) {
    for (size_t i = 0; kCodecMap[i].b != AV_CODEC_ID_NONE; ++i) {
        if (kCodecMap[i].b == b)
            return kCodecMap[i].a;
    }
    FATAL("fix the map <= %s", avcodec_get_name(b));
    return kCodecFormatUnknown;
}

AVCodecID get_av_codec_id(eCodecFormat a) {
    for (size_t i = 0; kCodecMap[i].a != kCodecFormatUnknown; ++i) {
        if (kCodecMap[i].a == a)
            return kCodecMap[i].b;
    }
    FATAL("fix the map <= %#x", a);
    return AV_CODEC_ID_NONE;
}

struct {
    ePixelFormat    a;
    AVPixelFormat   b;
} kPixelMap[] = {
    {kPixelFormatYUV420P,       AV_PIX_FMT_YUV420P},
    {kPixelFormatNV12,          AV_PIX_FMT_NV12},
    {kPixelFormatNV21,          AV_PIX_FMT_NV21},
    {kPixelFormatRGB565,        AV_PIX_FMT_RGB565},
    {kPixelFormatRGB888,        AV_PIX_FMT_RGB24},
    // Hardware accel
    {kPixelFormatNV12,          AV_PIX_FMT_VIDEOTOOLBOX},
    // END OF LIST
    {kPixelFormatUnknown,       AV_PIX_FMT_NONE}
};

ePixelFormat get_pix_format(AVPixelFormat b) {
    for (size_t i = 0; kPixelMap[i].b != AV_PIX_FMT_NONE; ++i) {
        if (kPixelMap[i].b == b)
            return kPixelMap[i].a;
    }
    FATAL("fix the map <= %s", av_get_pix_fmt_name(b));
    return kPixelFormatUnknown;
}

AVPixelFormat get_av_pix_format(ePixelFormat a) {
    for (size_t i = 0; kPixelMap[i].a != kPixelFormatUnknown; ++i) {
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
    {kSampleFormatS16,          AV_SAMPLE_FMT_S16},
    {kSampleFormatS32,          AV_SAMPLE_FMT_S32},
    {kSampleFormatFLT,          AV_SAMPLE_FMT_FLT},
    {kSampleFormatDBL,          AV_SAMPLE_FMT_DBL},
    // plannar
    {kSampleFormatS16,          AV_SAMPLE_FMT_S16P},
    {kSampleFormatS32,          AV_SAMPLE_FMT_S32P},
    {kSampleFormatFLT,          AV_SAMPLE_FMT_FLTP},
    {kSampleFormatDBL,          AV_SAMPLE_FMT_DBLP},
    // END OF LIST
    {kSampleFormatUnknown,      AV_SAMPLE_FMT_NONE},
};

eSampleFormat get_sample_format(AVSampleFormat b) {
    for (size_t i = 0; kSampleMap[i].b != AV_SAMPLE_FMT_NONE; ++i) {
        if (kSampleMap[i].b == b) return kSampleMap[i].a;
    }
    FATAL("fix the map");
    return kSampleFormatUnknown;
}

AVSampleFormat get_av_sample_format(eSampleFormat a) {
    for (size_t i = 0; kSampleMap[i].a != kSampleFormatUnknown; ++i) {
        if (kSampleMap[i].a == a) return kSampleMap[i].b;
    }
    FATAL("fix the map");
    return AV_SAMPLE_FMT_NONE;
}

// map AVFrame to MediaFrame, so we don't have to realloc memory again
struct AVMediaFrame : public MediaFrame {
    AVMediaFrame(AVCodecContext *avcc, AVFrame *frame) : MediaFrame() {
        opaque = av_frame_alloc();
        av_frame_ref((AVFrame*)opaque, frame);
        
        for (size_t i = 0; i < MEDIA_FRAME_NB_PLANES; ++i) {
            planes[i].data = NULL;
        }
        
        if (avcc->codec_type == AVMEDIA_TYPE_AUDIO) {
            a.format        = get_sample_format((AVSampleFormat)frame->format);
            a.channels      = frame->channels;
            a.freq          = frame->sample_rate;
            a.samples       = frame->nb_samples;
            if (av_sample_fmt_is_planar((AVSampleFormat)frame->format)) {
                for (size_t i = 0; i < frame->channels; ++i) {
                    planes[i].data  = frame->data[i];
                    // linesize may have extra bytes.
                    planes[i].size  = frame->nb_samples * av_get_bytes_per_sample((AVSampleFormat)frame->format);
                }
            } else {
                planes[0].data  = frame->data[0];
                planes[0].size  = frame->linesize[0];
            }
        } else if (avcc->codec_type == AVMEDIA_TYPE_VIDEO) {
            v.format        = get_pix_format((AVPixelFormat)frame->format);
            v.width         = frame->width;
            v.height        = frame->height;
            v.rect.x        = 0;
            v.rect.y        = 0;
            v.rect.w        = avcc->width;
            v.rect.h        = avcc->height;
            for (size_t i = 0; frame->data[i] != NULL; ++i) {
                planes[i].data  = frame->data[i];
                planes[i].size  = frame->buf[i]->size; 
            }
        } else {
            FATAL("FIXME");
        }
        // this may be wrong
        pts         = MediaTime(frame->pts * avcc->time_base.num, avcc->time_base.den);
        duration    = kTimeInvalid;
    }
    
    virtual ~AVMediaFrame() {
        av_frame_free((AVFrame**)&opaque);
    }
};

struct CodecContext {
};

static AVPixelFormat get_format(AVCodecContext *avcc, const AVPixelFormat *pix_fmts) {
    const AVPixelFormat *pix;
    for (pix = pix_fmts; *pix != AV_PIX_FMT_NONE; ++pix) {
        INFO("pix %s", av_get_pix_fmt_name(*pix));
        const AVPixFmtDescriptor *desc = av_pix_fmt_desc_get(*pix);
        // find the pix for hwaccel
        if (!(desc->flags & AV_PIX_FMT_FLAG_HWACCEL)) continue;

        // find the right hwaccel config
        const AVCodecHWConfig *hw = NULL;
        for (size_t i = 0;; ++i) {
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
            CHECK_EQ((int)hw->device_type, (int)AV_HWDEVICE_TYPE_VIDEOTOOLBOX);
            int rt = av_videotoolbox_default_init(avcc);
            if (rt < 0) {
                char err_str[64];
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

static int get_buffer(AVCodecContext *avcc, AVFrame *frame, int flags) {
    // TODO
    return avcodec_default_get_buffer2(avcc, frame, flags);
}

static status_t setupHwAccelContext(AVCodecContext *avcc) {

    INFO("supported hwaccel: ");
    AVBufferRef *hw_device_ctx = NULL;
    AVBufferRef *hw_frame_ctx = NULL;
    for (int i = 0; ; ++i) {
        const AVCodecHWConfig *hwc = avcodec_get_hw_config(avcc->codec, i);
        if (hwc == NULL) break;
        INFO("\tpix_fmt %s, method %#x, device type %s",
                av_get_pix_fmt_name(hwc->pix_fmt),
                hwc->methods,
                av_hwdevice_get_type_name(hwc->device_type));

#ifdef __APPLE__
        if (hwc->device_type != AV_HWDEVICE_TYPE_VIDEOTOOLBOX) continue;
#endif

        if (hwc->methods & AV_CODEC_HW_CONFIG_METHOD_HW_DEVICE_CTX) {
            int rt = av_hwdevice_ctx_create(&hw_device_ctx,
                    hwc->device_type,
                    NULL, NULL, 0);
            if (rt < 0) {
                char err_str[64];
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
    return OK;
}

static void parseAudioSpecificConfig(AVCodecContext *avcc, const Buffer& csd) {
    BitReader br(csd);
    MPEG4::AudioSpecificConfig asc(br);
    if (asc.valid) {
        avcc->extradata_size = csd.size();
        CHECK_GE(avcc->extradata_size, 2);
        avcc->extradata = (uint8_t*)av_mallocz(avcc->extradata_size +
                                               AV_INPUT_BUFFER_PADDING_SIZE);
        memcpy(avcc->extradata, csd.data(),
               avcc->extradata_size);
    } else {
        ERROR("bad AudioSpecificConfig");
    }
}

// parse AudioSpecificConfig from esds, for who needs AudioSpecificConfig
static void parseESDS(AVCodecContext *avcc, const Buffer& esds) {
    BitReader br(esds);
    MPEG4::ES_Descriptor esd(br);
    // client have make sure it is in the right form
    if (esd.valid) {
        CHECK_TRUE(esd.valid);
        parseAudioSpecificConfig(avcc, *esd.decConfigDescr.decSpecificInfo.csd);
    } else {
        ERROR("bad esds");
    }
}

static status_t setupExtraData(AVCodecContext *avcc, const Message& formats) {
    // different extra data for  different decoder
    switch (avcc->codec->id) {
        case AV_CODEC_ID_AAC:
            if (formats.contains("esds")) {
                Buffer *esds;
                formats.find<Buffer>("esds", &esds);
                parseESDS(avcc, *esds);
                // aac sbr have real sample rate in AudioSpecificConfig
                // but, DON'T fix avcc->sample_rate here
            } else if (formats.contains("csd")) {
                Buffer *csd;
                formats.find<Buffer>("csd", &csd);
                parseAudioSpecificConfig(avcc, *csd);
            } else {
                ERROR("missing esds|csd for aac");
                return UNKNOWN_ERROR;
            }
            break;
        case AV_CODEC_ID_H264:
            if (formats.contains("avcC")) {
                Buffer *avcC;
                formats.find<Buffer>("avcC", &avcC);
                // h264 decoder
                avcc->extradata_size = avcC->size();
                avcc->extradata = (uint8_t*)av_mallocz(avcc->extradata_size +
                        AV_INPUT_BUFFER_PADDING_SIZE);
                memcpy(avcc->extradata, avcC->data(),
                        avcc->extradata_size);
            } else {
                ERROR("missing avcC for h264");
                return UNKNOWN_ERROR;
            }
            break;
        case AV_CODEC_ID_HEVC:
            if (formats.contains("hvcC")) {
                Buffer *hvcC;
                formats.find<Buffer>("hvcC", &hvcC);
                avcc->extradata_size = hvcC->size();
                avcc->extradata = (uint8_t*)av_mallocz(avcc->extradata_size +
                        AV_INPUT_BUFFER_PADDING_SIZE);
                memcpy(avcc->extradata, hvcC->data(),
                        avcc->extradata_size);
            } else {
                ERROR("missing hvcC for hevc");
                return UNKNOWN_ERROR;
            } break;
        default:
            break;
    }

    // FIXME: ffmpeg have different codec id for different profile/version

    return OK;
}

static AVCodecContext* allocCodecContext(eModeType mode, const Message& formats, const Message& options) {
    CHECK_TRUE(formats.contains(kKeyFormat));

    eCodecFormat codec = (eCodecFormat)formats.findInt32(kKeyFormat);
    eCodecType type = GetCodecType(codec);

    bool hwaccel = mode != kModeTypeSoftware;

    AVCodecID id = get_av_codec_id(codec);

    INFO("[%#x] -> [%s]", codec, avcodec_get_name(id));

    AVCodec *avc = avcodec_find_decoder(id);
#if 0
    if (id == AV_CODEC_ID_AAC) {
        avc = avcodec_find_decoder_by_name("aac_fixed");
    }
#endif
    if (!avc) {
        ERROR("can't find codec for %s", avcodec_get_name(id));
        return NULL;
    }

    AVCodecContext *avcc = avcodec_alloc_context3(avc);
    if (!avcc) {
        ERROR("[OOM] alloc context failed");
        return NULL;
    }

    if (type == kCodecTypeAudio) {
        avcc->channels              = formats.findInt32(kKeyChannels);
        avcc->sample_rate           = formats.findInt32(kKeySampleRate);
        //avcc->bit_rate              = formats.findInt32(Media::Bitrate);
    } else if (type == kCodecTypeVideo) {
        avcc->width                 = formats.findInt32(kKeyWidth);
        avcc->height                = formats.findInt32(kKeyHeight);
        // FIXME: set right pixel format
        avcc->pix_fmt               = AV_PIX_FMT_YUV420P;
    } else {
        ERROR("unknown codec type %#x", type);
        avcodec_free_context(&avcc);
        return NULL;
    }

    // FIXME:
    //int32_t timescale = formats.findInt32(Media::Timescale, 1000000LL);
    //avcc->pkt_timebase.num = 1;
    //avcc->pkt_timebase.den = timescale;

    // after all other parameters.
    // information in csd may override others.
    setupExtraData(avcc, formats);

    // setup ffmpeg context
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
        avcc->thread_count      = GetCpuCount() + 1;
        avcc->thread_type       = FF_THREAD_FRAME | FF_THREAD_SLICE;
    } else {
        avcc->thread_count      = 1;
    }

    if (type == kCodecTypeAudio) {
        avcc->request_sample_fmt    = AV_SAMPLE_FMT_S16;
    } else if (type == kCodecTypeVideo) {
        if (hwaccel) {
            setupHwAccelContext(avcc);
        }
    }

    int ret = avcodec_open2(avcc, NULL, NULL);

    if (ret < 0) {
        char err_str[64];
        ERROR("%s: avcodec_open2 failed, error %s", avcodec_get_name(avcc->codec_id),
                av_make_error_string(err_str, 64, ret));
        avcodec_free_context(&avcc);
        return NULL;
    }

#if LOG_NDEBUG == 0
    if (type == kCodecTypeVideo) {
        DEBUG("w %d h %d, codec w %d h %d", avcc->width, avcc->height,
                avcc->coded_width, avcc->coded_height);
        DEBUG("gop %d", avcc->gop_size);
        DEBUG("pix_fmt %s", av_get_pix_fmt_name(avcc->pix_fmt));
        if (avcc->hwaccel) {
            DEBUG("%s %s %s",
                    avcc->hwaccel,
                    avcodec_get_name(avcc->hwaccel->id),
                    av_get_pix_fmt_name(avcc->hwaccel->pix_fmt));
        }
    }
#endif
    INFO("codec %s open success with %d threads, type %d",
            avcc->codec->name,
            avcc->thread_count,
            avcc->active_thread_type);

    return avcc;
}

#ifdef __APPLE__
sp<MediaFrame> readVideoToolboxFrame(CVPixelBufferRef);
#endif

struct LavcDecoder : public MediaDecoder {
    eModeType               mMode;
    AVCodecContext *        mContext;   // AVCodecContext, hide from caller
    List<MediaTime>         mTimestamps;

    // statistics
    size_t                  mInputCount;
    size_t                  mOutputCount;

    LavcDecoder(eModeType mode) : MediaDecoder(),
    mMode(mode),
    mContext(NULL),
    mInputCount(0),
    mOutputCount(0) { }

    virtual ~LavcDecoder() {
        AVCodecContext *avcc = mContext;
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

    virtual String string() const { return ""; }

    virtual Message formats() const {
        Message formats;
        AVCodecContext* avcc = mContext;
        if (avcc->codec_type == AVMEDIA_TYPE_AUDIO) {
            formats.setInt32(kKeyFormat, get_sample_format(avcc->sample_fmt));
            formats.setInt32(kKeyChannels, avcc->channels);
            formats.setInt32(kKeySampleRate, avcc->sample_rate);

            // FIX sample rate of AAC SBR
            if (avcc->codec_id == AV_CODEC_ID_AAC &&
                    avcc->extradata_size >= 2) {
                Buffer csd((const char *)avcc->extradata,
                        (size_t)avcc->extradata_size);
                BitReader br(csd);
                MPEG4::AudioSpecificConfig config(br);
                if (config.valid && config.sbr) {
                    INFO("fix sample rate %d => %d",
                            avcc->sample_rate,
                            config.extSamplingFrquency);
                    formats.setInt32(kKeySampleRate,
                            config.extSamplingFrquency);
                }
            }
        } else if (avcc->codec_type == AVMEDIA_TYPE_VIDEO) {
            formats.setInt32(kKeyFormat, get_pix_format(avcc->pix_fmt));
            formats.setInt32(kKeyWidth, avcc->width);
            formats.setInt32(kKeyHeight, avcc->height);
        } else {
            FATAL("unknown codec type %#x", avcc->codec_type);
        }
        return formats;
    }
    
    virtual MediaError configure(const Message& options) {
        return kMediaErrorNotSupported;
    }

    virtual MediaError init(const Message& format, const Message& options) {
        mContext = allocCodecContext(mMode, format, options);

        if (!mContext) {
            ERROR("failed to alloc codec context.");
            return kMediaErrorUnknown;
        }
        return kMediaNoError;
    }
    
    virtual MediaError write(const sp<MediaPacket>& input) {
        AVCodecContext *avcc = mContext;
        if (input != NULL && input->data != NULL) {
            ++mInputCount;

            DEBUG("%s: %.3f(s)|%.3f(s) flags %#x",
                    avcodec_get_name(avcc->codec_id),
                    input->pts * av_q2d(avcc->pkt_timebase),
                    input->dts * av_q2d(avcc->pkt_timebase),
                    input->flags);

            AVPacket *pkt   = av_packet_alloc();
            pkt->data   = input->data;
            pkt->size   = input->size;
            // FIXME:
            pkt->pts    = input->pts.value;
            pkt->dts    = input->dts.value;
            pkt->flags  = 0;

            if (input->flags & kFrameFlagSync)
                pkt->flags |= AV_PKT_FLAG_KEY;

            if (input->flags & kFrameFlagReference) {
                INFO("reference frame, disposable");
                pkt->flags |= AV_PKT_FLAG_DISCARD;
                pkt->flags |= AV_PKT_FLAG_DISPOSABLE;
            } else {
                mTimestamps.push(input->dts);
                mTimestamps.sort();
            }

            int ret = avcodec_send_packet(avcc, pkt);
            MediaError err = kMediaNoError;
            if (ret == AVERROR(EAGAIN)) {
                DEBUG("%s: try to read frame", avcodec_get_name(avcc->codec_id));
                err = kMediaErrorResourceBusy;
            } else if (ret == AVERROR(EINVAL)) {
                FATAL("%s: codec is not opened", avcodec_get_name(avcc->codec_id));
            } else if (ret < 0) {
                char err_str[64];
                FATAL("%s: decode return error %s", avcodec_get_name(avcc->codec_id),
                        av_make_error_string(err_str, 64, ret));
                err = kMediaErrorUnknown;
            }

            av_packet_free(&pkt);
            return err;
        } else {
            // codec enter draining mode
            DEBUG("%s: flushing...", avcodec_get_name(avcc->codec_id));
            avcodec_send_packet(avcc, NULL);
            return kMediaNoError;
        }
    }
    
    virtual sp<MediaFrame> read() {
        AVFrame *internal = av_frame_alloc();
        AVCodecContext *avcc = mContext;

        // receive frame from avcodec
        int ret = avcodec_receive_frame(avcc, internal);
        if (ret == AVERROR(EAGAIN)) {
            INFO("%s: need more input %zu",
                    avcodec_get_name(avcc->codec_id), mInputCount);
            return NULL;
        } else if (ret == AVERROR_EOF) {
            INFO("%s: eos...", avcodec_get_name(avcc->codec_id));
            return NULL;
        } else if (ret == AVERROR(EINVAL)) {
            FATAL("%s: codec is not opened", avcodec_get_name(avcc->codec_id));
        } else if (ret < 0) {
            char err_str[64];
            ERROR("%s: decode return error %s", avcodec_get_name(avcc->codec_id),
                    av_make_error_string(err_str, 64, ret));
            return NULL;
        }
        
        sp<MediaFrame> out;
#ifdef __APPLE__
        if (internal->format == AV_PIX_FMT_VIDEOTOOLBOX) {
            out = readVideoToolboxFrame((CVPixelBufferRef)internal->data[3]);
        } else
#endif
        {
            out = new AVMediaFrame(avcc, internal);
        }
        
#if 1
        out->pts                = *mTimestamps.begin();
        mTimestamps.pop();
#endif

#if LOG_NDEBUG == 0
        if (avcc->codec_type == AVMEDIA_TYPE_VIDEO) {
            DEBUG("frame %s %.3f(s) => %d x %d => %d x %d",
                    av_get_pix_fmt_name((AVPixelFormat)internal->format),
                    out->pts * av_q2d(avcc->pkt_timebase),
                    out->v.width,
                    out->v.height,
                    out->v.strideWidth,
                    out->v.sliceHeight);
        } else {
            DEBUG("frame %s %.3f(s), %d %d nb_samples %d",
                    av_get_sample_fmt_name((AVSampleFormat)internal->format),
                    out->pts * av_q2d(avcc->pkt_timebase),
                    out->a.channels,
                    out->a.rate,
                    internal->nb_samples);
        }
#endif

        av_frame_free(&internal);
        ++mOutputCount;
        return out;
    }

    virtual MediaError flush() {
        AVCodecContext *avcc = mContext;
        if (avcc && avcodec_is_open(avcc)) {
            avcodec_flush_buffers(avcc);
        }
        mInputCount = mOutputCount = 0;
        return kMediaNoError;
    }
};

namespace mtdcy {
    sp<MediaDecoder> CreateLavcDecoder(eModeType mode) {
        return new LavcDecoder(mode);
    }
};
