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


// File:    Audio.cpp
// Author:  mtdcy.chen
// Changes: 
//          1. 20160701     initial version
//

#define LOG_TAG "Libav.Audio"
#define LOG_NDEBUG 0
#include <toolkit/Toolkit.h>

#include "Audio.h"

#include "FFmpeg.h"

#include "asf/Asf.h"

extern "C" {
#include "libavformat/avformat.h"
#include "libavutil/frame.h"
#include "libavutil/imgutils.h"
#include <libswscale/swscale.h>         // FIXME: don't use this one.
    // FIXME: these are internal apis
    extern const AVCodecTag ff_rm_codec_tags[];
    extern const AVCodecTag ff_codec_bmp_tags[];
    enum AVCodecID ff_codec_get_id(const AVCodecTag *tags, unsigned int tag);
}

#include "mpeg4/Systems.h"
#include "mpeg4/Audio.h"

// 57.35
// avcodec_send_packet && avcodec_receive_frame
#if LIBAVCODEC_VERSION_INT < AV_VERSION_INT(57,35,100)
#pragma message("add support for old version " LIBAVCODEC_IDENT)
#error "add support for old version"
#endif

namespace mtdcy { namespace Libavcodec {
    static inline AVCodecID fixWmaFormat(const Buffer& csd, AVCodecID def) {
        if (csd.size() >= sizeof(ASF::WAVEFORMATEX)) {
            ASF::WAVEFORMATEX *wav = (ASF::WAVEFORMATEX*)csd.data();
            switch ((uint16_t)wav->wFormatTag) {
                case 0x160:
                    return AV_CODEC_ID_WMAV1;
                case 0x161:
                    return AV_CODEC_ID_WMAV2;
                case 0x162:
                    return AV_CODEC_ID_WMAPRO;
                case 0x163:
                    return AV_CODEC_ID_WMALOSSLESS;
                case 0xfffe:
                    {
                        ASF::WAVEFORMATEXTENSIBLE *ex = (ASF::WAVEFORMATEXTENSIBLE*)csd.data();
                        if (memcmp(ex->subFormat + 4, ASF::subformat_base_guid, 12)) {
                            FATAL("FIXME: add guid support.");
                            return def;
                        }

                        uint32_t formatTag = *(uint32_t*)ex->subFormat;
                        switch (formatTag) {
                            case 0x160:
                                return AV_CODEC_ID_WMAV1;
                            case 0x161:
                                return AV_CODEC_ID_WMAV2;
                            case 0x162:
                                return AV_CODEC_ID_WMAPRO;
                            case 0x163:
                                return AV_CODEC_ID_WMALOSSLESS;
                            default:
                                ERROR("ASF::WAVEFORMATEXTENSIBLE: %#x", formatTag);
                                break;
                        }
                        return def;
                    } break;
                default:
                    ERROR("add support for %#x", wav->wFormatTag);
                    return def;
            }
        } else {
            ERROR("not a ASF::WAVEFORMATEX");
            return def;
        }
    }

#if 0
    // KEEP THESE SYNC WITH LavfModules
    static inline AVCodecID fixRealVideoCodecID(const Buffer& csd) {
        if (csd.size() < sizeof(ASF::BITMAPINFOHEADER)) {
            ERROR("invalid ASF::BITMAPINFOHEADER.");
            return AV_CODEC_ID_RV40;
        }

        ASF::BITMAPINFOHEADER *bi = (ASF::BITMAPINFOHEADER*)csd.data();

        return ff_codec_get_id(ff_rm_codec_tags, bi->biCompression);
    }

    static inline AVCodecID fixWindowsVideoCodecID(const Buffer& csd) {
        if (csd.size() < sizeof(ASF::BITMAPINFOHEADER)) {
            ERROR("invalid ASF::BITMAPINFOHEADER.");
            return AV_CODEC_ID_VC1;
        }

        ASF::BITMAPINFOHEADER *bi = (ASF::BITMAPINFOHEADER*)csd.data();

        return ff_codec_get_id(avformat_get_riff_video_tags(), bi->biCompression);
    }
#endif

    static inline void parseWAVEFORMATEX(const Buffer& csd, AVCodecContext *cc) {
        const char *extradata = csd.data();
        int extradata_size = csd.size();

        if (csd.size() >= sizeof(ASF::WAVEFORMATEX)) {
            ASF::WAVEFORMATEX *wav = (ASF::WAVEFORMATEX*)csd.data();
            CHECK_LE(wav->cbSize + sizeof(ASF::WAVEFORMATEX), csd.size());

            // FIXME: test wFormatTag
            if ((uint16_t)wav->wFormatTag == 0xfffe) {
                INFO("parse ASF::WAVEFORMATEXTENSIBLE");
                ASF::WAVEFORMATEXTENSIBLE *ex = (ASF::WAVEFORMATEXTENSIBLE*)csd.data();
                cc->channels        = ex->nChannels;
                cc->sample_rate     = ex->nSamplesPerSec;
                cc->bit_rate        = ex->nAvgBytesPerSec;
                cc->block_align     = ex->nBlockAlign;
                //cc->bits_per_coded_sample   = ex->wBitsPerSample;
                cc->bits_per_coded_sample   = ex->wValidBitsPerSample;
                cc->channel_layout  = ex->dwChannelMask;

                if (!memcmp(ex->subFormat + 4, ASF::subformat_base_guid, 12)) {
                    cc->codec_tag   = *(uint32_t*)ex->subFormat;
                } else {
                    FATAL("FIXME: add guid support.");
                }

                extradata_size      = ex->cbSize;
                extradata           = csd.data() + sizeof(ASF::WAVEFORMATEXTENSIBLE);
                INFO("cbSize = %d", ex->cbSize);
            } else {
                INFO("parse ASF::WAVEFORMATEX");
                cc->codec_tag       = wav->wFormatTag;
                cc->channels        = wav->nChannels;
                cc->sample_rate     = wav->nSamplesPerSec;
                cc->bit_rate        = wav->nAvgBytesPerSec;
                cc->block_align     = wav->nBlockAlign;
                cc->bits_per_coded_sample   = wav->wBitsPerSample;

                extradata_size      = wav->cbSize;
                extradata           = csd.data() + sizeof(ASF::WAVEFORMATEX);
                INFO("cbSize = %d", wav->cbSize);
            }
        }

        cc->extradata         = 
            (uint8_t*)av_mallocz(extradata_size + AV_INPUT_BUFFER_PADDING_SIZE);

        CHECK_NULL(cc->extradata);
        memcpy(cc->extradata, extradata, extradata_size);

        cc->extradata_size = extradata_size;
    }

#if 0 // move this into omxcore
    static const uint8_t kStaticESDS[] = {
        0x03, 22,
        0x00, 0x00,     // ES_ID
        0x00,           // streamDependenceFlag, URL_Flag, OCRstreamFlag

        0x04, 17,
        0x40,                       // Audio ISO/IEC 14496-3
        0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00,

        0x05, 2,
        // AudioSpecificIn follows
    };

    static inline void parseESDS(const Buffer& csd, AVCodecContext *cc) {
        INFO("parseESDS %s", csd.string().c_str());

        // android put an esds header before audio specific config
        if (csd.size() > sizeof(kStaticESDS) && 
                !memcmp(csd.data(), kStaticESDS, sizeof(kStaticESDS))) {
            cc->extradata_size = csd.size() - sizeof(kStaticESDS);
            cc->extradata   = (uint8_t*)av_malloc(cc->extradata_size
                    + AV_INPUT_BUFFER_PADDING_SIZE);
            CHECK_NULL(cc->extradata);
            memcpy(cc->extradata, csd.data() + sizeof(kStaticESDS), 
                    cc->extradata_size);
        } else {
            // TODO
        }
    }
#endif

    static void prepareESDS(const Buffer& csd, AVCodecContext *cc) {
        if (cc->codec_id == AV_CODEC_ID_AAC) {
            // AudioSpecificConfig
            // ISO/IEC 14496-3 
            // 2 bytes setup data for aac
            // 5 bits: object type
            // 4 bits: frequency index
            // if (frequency index == 15)
            //    24 bits: frequency
            // 4 bits: channel configuration
            // 1 bit: frame length flag
            // 1 bit: dependsOnCoreCoder
            // 1 bit: extensionFlag
            BitReader br(csd);
            if (br.r8() == MPEG4::ES_DescrTag) {
                MPEG4::ES_Descriptor esd(br);
                if (esd.valid && esd.decConfigDescr.decSpecificInfo.valid) {
                    cc->extradata_size = esd.decConfigDescr.decSpecificInfo.csd->size();
                    cc->extradata = (uint8_t*)av_mallocz(cc->extradata_size 
                            + AV_INPUT_BUFFER_PADDING_SIZE);
                    memcpy(cc->extradata, esd.decConfigDescr.decSpecificInfo.csd->data(),
                            cc->extradata_size);
                    return;
                } else {
                    ERROR("bad esd");
                }
            }
        }

        cc->extradata_size = csd.size();
        cc->extradata = (uint8_t*)av_mallocz(cc->extradata_size + AV_INPUT_BUFFER_PADDING_SIZE);
        CHECK_NULL(cc->extradata);
        memcpy(cc->extradata, csd.data(), cc->extradata_size);
    }

    static AVCodecContext* allocCodecContext(int32_t codecType,
            const Message* params) {
        SetupFFmpeg();
        
        eCodecType type = (eCodecType)params->findInt32(kType);

        AVCodecID id = TranslateMediaFormat2AVCodecID(codecType);

        Buffer *csd = NULL;
        String csdName = Media::CSD;
        if (codecType == kVideoCodecTypeH264) {
            csdName     = "avcC";
        } else if (codecType == kAudioCodecTypeAAC) {
            csdName     = "esds";
        }
        if (!params->find<Buffer>(csdName, &csd)) {
            DEBUG("no csd exists");
        } else {
            DEBUG("csd %s", csd->string(true).c_str());
        }

        if (csd != NULL) {
            // fix format
            switch (id) {
                case AV_CODEC_ID_WMAV1:
                case AV_CODEC_ID_WMAV2:
                case AV_CODEC_ID_WMAPRO:
                case AV_CODEC_ID_WMALOSSLESS:
                    id = fixWmaFormat(*csd, id);
                    break;
#if 0
                case AV_CODEC_ID_RV40:
                    id = fixRealVideoCodecID(*csd);
                    break;
                case AV_CODEC_ID_VC1:
                    id = fixWindowsVideoCodecID(*csd);
                    break;
#endif
                default:
                    break;
            }
        }

        INFO("[#x] -> [%s]", codecType, avcodec_get_name(id));

        AVCodec *codec = avcodec_find_decoder(id);
        if (!codec) {
            ERROR("can't find codec for %s", avcodec_get_name(id));
            return NULL;
        }

#ifdef __APPLE__
        if (id == AV_CODEC_ID_H264) {
            AVCodec *hwcodec = avcodec_find_decoder_by_name("h264_vda");
            if (hwcodec) {
                DEBUG("using h264_vda");
                codec = hwcodec;
            }
        }
#endif

#if LOG_NDEBUG == 0
        for (int i = 0; ; ++i) {
            const AVCodecHWConfig *hwc = avcodec_get_hw_config(codec, i);
            if (hwc == NULL) break;
            DEBUG("pix_fmt %s, method %#x, device type %s",
                    av_get_pix_fmt_name(hwc->pix_fmt),
                    hwc->methods,
                    av_hwdevice_get_type_name(hwc->device_type));
        }
#endif

        AVCodecContext *avcc = avcodec_alloc_context3(codec);
        if (!avcc) {
            ERROR("[OOM] alloc context failed");
            return NULL;
        }

        if (type == kCodecTypeAudio) {
            avcc->channels              = params->findInt32(Media::Channels);
            avcc->sample_rate           = params->findInt32(Media::SampleRate);
            avcc->bits_per_coded_sample = params->findInt32(Media::BitsPerSample);
            avcc->bit_rate              = params->findInt32(Media::Bitrate);
        } else if (type == kCodecTypeVideo) {
            avcc->width                 = params->findInt32(Media::Width);
            avcc->height                = params->findInt32(Media::Height);
            // FIXME: set right pixel format
            avcc->pix_fmt               = AV_PIX_FMT_YUV420P;
        } else {
            FATAL("unknown codec %#x", type);
        }

        // after all other parameters. 
        // information in csd may override others.
        if (csd != NULL) { prepareESDS(*csd, avcc); }

        avcc->workaround_bugs       = FF_BUG_AUTODETECT; 
        avcc->lowres                = 0; 
        avcc->idct_algo             = FF_IDCT_AUTO; 
        avcc->skip_frame            = AVDISCARD_DEFAULT;
        avcc->skip_idct             = AVDISCARD_DEFAULT;
        avcc->skip_loop_filter      = AVDISCARD_DEFAULT;
        avcc->error_concealment     = 3;

        AVDictionary *opts = NULL; 
        //av_dict_set_int(&opts, "refcounted_frames", 1, 0);

        //av_dict_set_int(&opts, "thread_type", 0, 0);
        //av_dict_set_int(&opts, "threads", 1, 0);
        av_dict_set_int(&opts, "threads", GetCpuCount(), 0);

        if (type == kCodecTypeAudio) {
            if (codecType == kAudioCodecTypeAPE ||
                codecType == kAudioCodecTypeFLAC)
                av_dict_set_int(&opts, "request_sample_fmt", 
                        AV_SAMPLE_FMT_S32, 0);
            else 
                av_dict_set_int(&opts, "request_sample_fmt", 
                        AV_SAMPLE_FMT_S16, 0);
        } else if (type == kCodecTypeVideo) {
#ifdef __APPLE__ 
#endif
        }

        LockFFmpeg();
        int ret = avcodec_open2(avcc, NULL, &opts);
        UnlockFFmpeg();

        if (opts) av_dict_free(&opts);

        if (ret < 0) {
            char err_str[64];
            ERROR("%s: avcodec_open2 failed, error %s", avcodec_get_name(avcc->codec_id),
                    av_make_error_string(err_str, 64, ret));
            avcodec_free_context(&avcc);
            return NULL;
        }

#if LOG_NDEBUG == 0
        if (type == kCodecTypeAudio) {
        } else if (type == kCodecTypeVideo) {
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
        DEBUG("codec %s open success with %d threads, type %d",
                avcc->codec->name, 
                avcc->thread_count,
                avcc->thread_type);
#endif

        return avcc;
    }

    Audio::Audio(int32_t codec, const Message* params) :
        MediaCodec(),
        mCodecName(codec),
        mStatus(NO_INIT),
        mFormats(NULL),
        mContext(NULL), 
        mHook(NULL), mHook2(NULL)
    {
        mContext = allocCodecContext(codec, params);

        if (!mContext) {
            ERROR("failed to alloc codec context.");
            return;
        }

        mFormats = buildAudioFormat();
        mStatus = mFormats != NULL ? OK : NO_INIT;
    }

    Audio::~Audio() {
        DEBUG("~Audio");
        if (mContext) {
            avcodec_free_context(&mContext);
        }
    }

    String Audio::string() const {
        return mFormats != 0 ? mFormats->string() : String("Audio");
    }

    status_t Audio::status() const {
        return mStatus;
    }

    const Message& Audio::formats() const {
        return *mFormats; 
    }

    status_t Audio::configure(const Message& options) {
        return INVALID_OPERATION;
    }

    sp<Message> Audio::buildAudioFormat() {
        sp<Message> audio = new Message;

        // FIXME:
        int32_t bps = 16;

        // audio converter.
        switch (mContext->sample_fmt) {
            case AV_SAMPLE_FMT_U8:
                mHook   = (bps == 32 ? PCM_HOOK_NAME(u8, s32) :
                        PCM_HOOK_NAME(u8, s16));
                break;
            case AV_SAMPLE_FMT_S16:
                mHook   = (bps == 32 ? PCM_HOOK_NAME(s16, s32) : 
                        PCM_HOOK_NAME(s16, s16));
                break;
            case AV_SAMPLE_FMT_S32:
                mHook   = (bps == 32 ? PCM_HOOK_NAME(s32, s32) :
                        PCM_HOOK_NAME(s32, s16));
                break;
            case AV_SAMPLE_FMT_FLT:
                mHook   = (bps == 32 ? PCM_HOOK_NAME(flt, s32) :
                        PCM_HOOK_NAME(flt, s16));
                break;
            case AV_SAMPLE_FMT_DBL:
                mHook   = (bps == 32 ? PCM_HOOK_NAME(dbl, s32) :
                        PCM_HOOK_NAME(dbl, s16));
                break;
            case AV_SAMPLE_FMT_U8P:
                mHook2  = (bps == 32 ? PCM_INTERLEAVE2_HOOK_NAME(u8, s32) : 
                        PCM_INTERLEAVE2_HOOK_NAME(u8, s16));
                break;
            case AV_SAMPLE_FMT_S16P:
                mHook2  = (bps == 32 ? PCM_INTERLEAVE2_HOOK_NAME(s16, s32) : 
                        PCM_INTERLEAVE_HOOK_NAME(s16));
                break;
            case AV_SAMPLE_FMT_S32P:
                mHook2  = (bps == 32 ? PCM_INTERLEAVE_HOOK_NAME(s32) :
                        PCM_INTERLEAVE2_HOOK_NAME(s32, s16));
                break;
            case AV_SAMPLE_FMT_FLTP:
                mHook2  = (bps == 32 ? PCM_INTERLEAVE2_HOOK_NAME(flt, s32) : 
                        PCM_INTERLEAVE2_HOOK_NAME(flt, s16));
                break;
            case AV_SAMPLE_FMT_DBLP:
                mHook2  = (bps == 32 ? PCM_INTERLEAVE2_HOOK_NAME(dbl, s32) : 
                        PCM_INTERLEAVE2_HOOK_NAME(dbl, s16));
                break;
            default:
                ERROR("unknown sample_fmt %#x", mContext->sample_fmt);
                return NULL;
        }
        audio->setInt32(kFormat, kAudioCodecTypePCM);
        audio->setInt32(Media::Channels,        mContext->channels);

        // FIX sample rate of AAC SBR
        if (mContext->codec_id == AV_CODEC_ID_AAC && 
                mContext->extradata_size >= 2) {
            Buffer csd((const char *)mContext->extradata, 
                    (size_t)mContext->extradata_size);
            BitReader br(csd);
            MPEG4::AudioSpecificConfig config(br);
            if (config.valid && config.sbr) {
                audio->setInt32(Media::SampleRate,  config.extSamplingFrquency);
            }
        } else {
            audio->setInt32(Media::SampleRate,      mContext->sample_rate);
        }
        //audio->setInt32(Media::BitsPerSample,   bps);

        return audio;
    }

    status_t Audio::flush() {
        if (mContext && avcodec_is_open(mContext)) {
            avcodec_flush_buffers(mContext);
        }
        return OK;
    }

    status_t Audio::write(const sp<MediaPacket>& input) {
        if (input != NULL && input->data != NULL) {
            AVPacket *pkt   = av_packet_alloc();
            pkt->data   = (uint8_t*)input->data->data();
            pkt->size   = input->data->size();
            pkt->pts    = input->pts;
            pkt->dts    = input->dts;

            if (input->flags & kFrameFlagSync)
                pkt->flags |= AV_PKT_FLAG_KEY;
            if (input->flags & kFrameFlagReference)
                pkt->flags |= AV_PKT_FLAG_DISCARD;

            int ret = avcodec_send_packet(mContext, pkt);
            status_t status = OK;
            if (ret == AVERROR(EAGAIN)) {
                DEBUG("%s: try to read frame", avcodec_get_name(mContext->codec_id));
                status = TRY_AGAIN;
            } else if (ret == AVERROR(EINVAL)) {
                FATAL("%s: codec is not opened", avcodec_get_name(mContext->codec_id));
            } else if (ret < 0) {
                char err_str[64];
                ERROR("%s: decode return error %s", avcodec_get_name(mContext->codec_id),
                        av_make_error_string(err_str, 64, ret));
                status = UNKNOWN_ERROR;
            }

            av_packet_free(&pkt);
            return status;
        } else {
            DEBUG("%s: flushing...", avcodec_get_name(mContext->codec_id));
            avcodec_send_packet(mContext, NULL);
            return OK;
        }
    }

    sp<MediaFrame> Audio::read() {
        AVFrame *raw = av_frame_alloc();

        int ret = avcodec_receive_frame(mContext, raw);

        if (ret == AVERROR(EAGAIN)) {
            DEBUG("%s: need more input", avcodec_get_name(mContext->codec_id));
            return NULL;
        } else if (ret == AVERROR_EOF) {
            INFO("%s: eos...", avcodec_get_name(mContext->codec_id));
            return NULL;
        } else if (ret == AVERROR(EINVAL)) {
            FATAL("%s: codec is not opened", avcodec_get_name(mContext->codec_id));
        } else if (ret < 0) {
            char err_str[64];
            ERROR("%s: decode return error %s", avcodec_get_name(mContext->codec_id),
                    av_make_error_string(err_str, 64, ret));
            return NULL;
        }

        size_t frameSize = raw->nb_samples * raw->channels * sizeof(int16_t); // FIXME
        sp<Buffer> frame = new Buffer(frameSize);
        if (mHook) {
            frameSize = mHook(raw->data[0],
                    raw->nb_samples * raw->channels * 
                    av_get_bytes_per_sample((AVSampleFormat)raw->format),
                    frame->data());
            frame->step(frameSize);
        } else if (mHook2) {
            frameSize = mHook2((void**)raw->data,
                    raw->nb_samples * 
                    av_get_bytes_per_sample((AVSampleFormat)raw->format),
                    raw->channels,
                    frame->data());
            frame->step(frameSize);
        } else {
            frame->write((const char *)raw->data[0], frameSize);
        }

        sp<MediaFrame> out = new MediaFrame;
        out->data[0]        = frame;
        out->a.bps          = 16; // FIXME
        out->a.channels     = raw->channels;
        out->a.sampleRate   = mContext->sample_rate;
        out->pts            = raw->pts;
        av_frame_free(&raw);
        return out;
    }
}; };
