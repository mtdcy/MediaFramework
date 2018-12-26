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

#define LOG_TAG "Libav.Decoder"
//#define LOG_NDEBUG 0
#include <toolkit/Toolkit.h>

#include "Decoder.h"

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
    
#ifdef __APPLE__
#include <libavcodec/videotoolbox.h>
#endif
}

#include "mpeg4/Systems.h"
#include "mpeg4/Audio.h"


// 57.35
// avcodec_send_packet && avcodec_receive_frame
#if LIBAVCODEC_VERSION_INT < AV_VERSION_INT(57,35,100)
#pragma message("add support for version " LIBAVCODEC_IDENT)
#error "add support for old version"
#endif

namespace mtdcy { namespace Lavc {

    struct AVFrameMemory : public Memory {
        AVFrameMemory(AVFrame *frame, size_t);
        virtual ~AVFrameMemory();
        virtual size_t  resize(size_t newCapacity);
        AVFrame *mFrame;
    };

    AVFrameMemory::AVFrameMemory(AVFrame * frame, size_t index) :
        Memory(),
        mFrame(av_frame_clone(frame)) 
    {
        mData = mFrame->data[index];
        if (index == 0) 
            mCapacity = mFrame->linesize[index] * mFrame->height;
        else
            mCapacity = mFrame->linesize[index] * mFrame->height / 2;

        //DEBUG("%p %zu", mData, mCapacity);
    }

    AVFrameMemory::~AVFrameMemory() {
        av_frame_free(&mFrame);
    }

    size_t AVFrameMemory::resize(size_t newCapacity) {
        FATAL("FIXME: resize");
        //mCapacity = newCapacity;
        return mCapacity;
    }

#if 0
    // KEEP THESE SYNC WITH LavfModules
    static inline AVCodecID fixRealDecoderCodecID(const Buffer& csd) {
        if (csd.size() < sizeof(ASF::BITMAPINFOHEADER)) {
            ERROR("invalid ASF::BITMAPINFOHEADER.");
            return AV_CODEC_ID_RV40;
        }

        ASF::BITMAPINFOHEADER *bi = (ASF::BITMAPINFOHEADER*)csd.data();

        return ff_codec_get_id(ff_rm_codec_tags, bi->biCompression);
    }

    static inline AVCodecID fixWindowsDecoderCodecID(const Buffer& csd) {
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
    
    struct CodecContext {
        
    };
    
    static AVPixelFormat get_format(AVCodecContext *avcc, const AVPixelFormat *pix_fmts) {
        const AVPixelFormat *pix;
        for (pix = pix_fmts; *pix != AV_PIX_FMT_NONE; ++pix) {
            INFO("pix fmt %s", av_get_pix_fmt_name(*pix));
            const AVPixFmtDescriptor *desc = av_pix_fmt_desc_get(*pix);
            if (!(desc->flags & AV_PIX_FMT_FLAG_HWACCEL)) continue;
            
            const AVCodecHWConfig *hw = NULL;
            for (size_t i = 0;; ++i) {
                hw = avcodec_get_hw_config(avcc->codec, i);
                if (!hw) break;
                if (!(hw->methods & AV_CODEC_HW_CONFIG_METHOD_HW_DEVICE_CTX)) continue;
                if (hw->pix_fmt == *pix) break;
            }
            
            if (hw) {
                INFO("hwaccel %#x", hw->methods);
                CHECK_EQ((int)hw->device_type, (int)AV_HWDEVICE_TYPE_VIDEOTOOLBOX);
                int rt = av_videotoolbox_default_init(avcc);
                if (rt < 0) {
                    char err_str[64];
                    FATAL("init hw accel context failed. %s", av_make_error_string(err_str, 64, rt));
                }
                return *pix;
            }
        }
        return avcodec_default_get_format(avcc, pix_fmts);
    }
    
    static int get_buffer(AVCodecContext *avcc, AVFrame *frame, int flags) {
        // TODO
        return avcodec_default_get_buffer2(avcc, frame, flags);
    }

    static AVCodecContext* allocCodecContext(const Message& format) {
        int32_t codec = format.findInt32(kFormat, kCodecFormatUnknown);
        int32_t mode = format.findInt32(kMode, kModeTypeNormal);
        eCodecType type = (eCodecType)format.findInt32(kType);
        
        SetupFFmpeg();

        AVCodecID id = TranslateMediaFormat2AVCodecID(codec);

        Buffer *csd = NULL;
        String csdName = Media::CSD;
        if (codec == kVideoCodecFormatH264) {
            csdName     = "avcC";
        } else if (codec == kAudioCodecFormatAAC) {
            csdName     = "esds";
        }

        if (!format.find<Buffer>(csdName, &csd)) {
            DEBUG("no csd exists");
        } else {
            DEBUG("csd %s", csd->string(true).c_str());
        }

        if (csd != NULL) {
            // fix format
            switch (id) {
#if 0
                case AV_CODEC_ID_WMAV1:
                case AV_CODEC_ID_WMAV2:
                case AV_CODEC_ID_WMAPRO:
                case AV_CODEC_ID_WMALOSSLESS:
                    id = fixWmaFormat(*csd, id);
                    break;
                case AV_CODEC_ID_RV40:
                    id = fixRealDecoderCodecID(*csd);
                    break;
                case AV_CODEC_ID_VC1:
                    id = fixWindowsDecoderCodecID(*csd);
                    break;
#endif
                default:
                    break;
            }
        }

        INFO("[%#x] -> [%s]", codec, avcodec_get_name(id));

        AVCodec *avc = avcodec_find_decoder(id);
        if (!codec) {
            ERROR("can't find codec for %s", avcodec_get_name(id));
            return NULL;
        }

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

        AVCodecContext *avcc = avcodec_alloc_context3(avc);
        if (!avcc) {
            ERROR("[OOM] alloc context failed");
            return NULL;
        }
        
        if (mode != kModeTypePreview) {
            avcc->get_format = get_format;
        }

        if (type == kCodecTypeAudio) {
            avcc->channels              = format.findInt32(Media::Channels);
            avcc->sample_rate           = format.findInt32(Media::SampleRate);
            avcc->bits_per_coded_sample = format.findInt32(Media::BitsPerSample);
            avcc->bit_rate              = format.findInt32(Media::Bitrate);
        } else if (type == kCodecTypeVideo) {
            avcc->width                 = format.findInt32(Media::Width);
            avcc->height                = format.findInt32(Media::Height);
            // FIXME: set right pixel format
            avcc->pix_fmt               = AV_PIX_FMT_YUV420P;
        } else {
            FATAL("unknown codec type %#x", type);
        }
        int32_t timescale = format.findInt32(Media::Timescale, 1000000LL);
        avcc->pkt_timebase.num = 1;
        avcc->pkt_timebase.den = timescale;

        // after all other parameters. 
        // information in csd may override others.
        if (csd != NULL) { prepareESDS(*csd, avcc); }
        // FIXME: set proper csd data for macOS DecoderToolbox

        avcc->workaround_bugs       = FF_BUG_AUTODETECT; 
        avcc->lowres                = 0; 
        avcc->idct_algo             = FF_IDCT_AUTO; 
        avcc->skip_frame            = AVDISCARD_DEFAULT;
        avcc->skip_idct             = AVDISCARD_DEFAULT;
        avcc->skip_loop_filter      = AVDISCARD_DEFAULT;
        avcc->error_concealment     = 3;

        AVDictionary *opts = NULL; 
        av_dict_set_int(&opts, "refcounted_frames", 1, 0);

        //av_dict_set_int(&opts, "thread_type", 0, 0);
        //av_dict_set_int(&opts, "threads", 1, 0);
        if (mode == kModeTypeNormal) {
            av_dict_set(&opts, "threads", "auto", 0);
        } else {
            av_dict_set_int(&opts, "threads", 1, 0);
            av_dict_set_int(&opts, "lowres", 2, 0);
        }
        
        if (mode == kModeTypeNormal) {
            // setup hw acce
            AVBufferRef *device;
            AVHWDeviceType type = AV_HWDEVICE_TYPE_VIDEOTOOLBOX;
            int rt = av_hwdevice_ctx_create(&device, type, NULL, NULL, 0);
            if (rt < 0) {
                char err_str[64];
                FATAL("create hw device failed. %s", av_make_error_string(err_str, 64, rt));
            }
            avcc->hw_device_ctx = av_buffer_ref(device);
        }

        if (type == kCodecTypeAudio) {
            if (codec == kAudioCodecFormatAPE ||
                codec == kAudioCodecFormatFLAC) {
                av_dict_set_int(&opts, "request_sample_fmt", 
                        AV_SAMPLE_FMT_S32, 0);
            } else {
                av_dict_set_int(&opts, "request_sample_fmt", 
                        AV_SAMPLE_FMT_S16, 0);
            }
        } else if (type == kCodecTypeVideo) {
#ifdef __APPLE__
            // enable hwaccel for video
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
        if (codecName.startsWith("audio/")) {
        } else if (codecName.startsWith("video/")) {
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

    Decoder::Decoder(const Message& format) :
        MediaCodec(),
        mMode(kModeTypeNormal),
        mFormats(NULL),
        mContext(NULL), 
        mSwsContext(NULL),
        mInputCount(0),
        mOutputCount(0),
        mHook(NULL), mHook2(NULL)
    {
        mMode = format.findInt32(kMode, kModeTypeNormal);
        CHECK_TRUE(format.contains(kType));
        mType = (eCodecType)format.findInt32(kType);
        mContext = allocCodecContext(format);

        if (!mContext) {
            ERROR("failed to alloc codec context.");
            return;
        }

        if (mType == kCodecTypeAudio)
            mFormats = buildAudioFormat();
        else
            mFormats = buildVideoFormat();
    }

    Decoder::~Decoder() {
        DEBUG("~Decoder");
        if (mContext) {
            if (mContext->hw_device_ctx) {
                av_videotoolbox_default_free(mContext);
            }
            avcodec_free_context(&mContext);
        }
    }

    String Decoder::string() const {
        return mFormats != 0 ? mFormats->string() : String("Decoder");
    }

    status_t Decoder::status() const {
    }

    const Message& Decoder::formats() const {
        return *mFormats; 
    }

    status_t Decoder::configure(const Message& options) {
        return INVALID_OPERATION;
    }

    sp<Message> Decoder::buildVideoFormat() {
        sp<Message> video = new Message;

        // XXX: update width/height/stride/slice_height

        video->setInt32(kFormat,         kPixelFormatYUV420P);
        video->setInt32(Media::Width,           mContext->width); 
        video->setInt32(Media::Height,          mContext->height);
        //video->setInt32(Media::StrideWidth,     mContext->stride);
        //video->setInt32(Media::SliceHeight,     mContext->slice_height);

        return video;
    }
    
    sp<Message> Decoder::buildAudioFormat() {
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
        audio->setInt32(kFormat, kAudioCodecFormatPCM);
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

    status_t Decoder::flush() {
        if (mContext && avcodec_is_open(mContext)) {
            avcodec_flush_buffers(mContext);
        }
        mInputCount = mOutputCount = 0;
        return OK;
    }

    status_t Decoder::write(const sp<MediaPacket>& input) {
        if (input != NULL && input->data != NULL) {
            ++mInputCount;

            AVPacket *pkt   = av_packet_alloc();
            pkt->data   = (uint8_t*)input->data->data();
            pkt->size   = input->data->size();
            pkt->pts    = input->pts;
            pkt->dts    = input->dts;
            pkt->flags  = 0;

            if (input->flags & kFrameFlagSync)
                pkt->flags |= AV_PKT_FLAG_KEY;

            if (input->flags & kFrameFlagReference) {
                INFO("reference frame, disposable");
                pkt->flags |= AV_PKT_FLAG_DISCARD;
                pkt->flags |= AV_PKT_FLAG_DISPOSABLE;
            }

            int ret = avcodec_send_packet(mContext, pkt);
            status_t status = OK;
            if (ret == AVERROR(EAGAIN)) {
                DEBUG("%s: try to read frame", avcodec_get_name(mContext->codec_id));
                status = TRY_AGAIN;
            } else if (ret == AVERROR(EINVAL)) {
                FATAL("%s: codec is not opened", avcodec_get_name(mContext->codec_id));
            } else if (ret < 0) {
                char err_str[64];
                FATAL("%s: decode return error %s", avcodec_get_name(mContext->codec_id),
                        av_make_error_string(err_str, 64, ret));
                status = UNKNOWN_ERROR;
            }

            av_packet_free(&pkt);
            return status;
        } else {
            // codec enter draining mode
            DEBUG("%s: flushing...", avcodec_get_name(mContext->codec_id));
            avcodec_send_packet(mContext, NULL);
            return OK;
        }
    }
    
    static sp<MediaFrame> videotoolbox_frame(AVCodecContext *avcc, AVFrame *raw) {
        CVPixelBufferRef pixbuf = (CVPixelBufferRef)raw->data[3];
        OSType native_fmt = CVPixelBufferGetPixelFormatType(pixbuf);
        CVReturn err;
        
        sp<MediaFrame> frame = new MediaFrame;
        switch (native_fmt) {
            case kCVPixelFormatType_420YpCbCr8Planar:
                frame->v.fmt = kPixelFormatYUV420P;
                break;
#if 0
            case kCVPixelFormatType_422YpCbCr8:
                frame->fmt = Media::Codec::YUV422;
                break;
            case kCVPixelFormatType_32BGRA:
                frame->fmt = Media::Codec::BGRA;
                break;
#endif
#ifdef kCFCoreFoundationVersionNumber10_7
            case kCVPixelFormatType_420YpCbCr8BiPlanarVideoRange:
                frame->v.fmt = kPixelFormatNV12;
                break;
#endif
            default:
                FATAL("Unsupported pixel format: %#x", native_fmt);
        }
        
        err = CVPixelBufferLockBaseAddress(pixbuf, kCVPixelBufferLock_ReadOnly);
        if (err != kCVReturnSuccess) {
            ERROR("Error locking the pixel buffer");
            return NULL;
        }
        
        size_t left, right, top, bottom;
        CVPixelBufferGetExtendedPixels(pixbuf, &left, &right, &top, &bottom);
        DEBUG("paddings %zu %zu %zu %zu", left, right, top, bottom);
        
        if (CVPixelBufferIsPlanar(pixbuf)) {
            // as we have to copy the data, copy to continueslly space
            DEBUG("CVPixelBufferGetDataSize %zu", CVPixelBufferGetDataSize(pixbuf));
            DEBUG("CVPixelBufferGetPlaneCount %zu", CVPixelBufferGetPlaneCount(pixbuf));
            
            sp<Buffer> data = new Buffer(CVPixelBufferGetDataSize(pixbuf));
            size_t planes = CVPixelBufferGetPlaneCount(pixbuf);
            for (size_t i = 0; i < planes; i++) {
                DEBUG("CVPixelBufferGetBaseAddressOfPlane %p", CVPixelBufferGetBaseAddressOfPlane(pixbuf, i));
                DEBUG("CVPixelBufferGetBytesPerRowOfPlane %zu", CVPixelBufferGetBytesPerRowOfPlane(pixbuf, i));
                DEBUG("CVPixelBufferGetWidthOfPlane %zu", CVPixelBufferGetWidthOfPlane(pixbuf, i));
                DEBUG("CVPixelBufferGetHeightOfPlane %zu", CVPixelBufferGetHeightOfPlane(pixbuf, i));
                data->write((const char *)CVPixelBufferGetBaseAddressOfPlane(pixbuf, i),
                            CVPixelBufferGetBytesPerRowOfPlane(pixbuf, i) *
                            CVPixelBufferGetHeightOfPlane(pixbuf, i));
            }
            frame->data[0] = data;
        } else {
            frame->data[0] = new Buffer(
            (const char*)CVPixelBufferGetBaseAddress(pixbuf),
                                        CVPixelBufferGetBytesPerRow(pixbuf));
        }
        
        frame->v.width          = raw->width;
        frame->v.height         = raw->height;
        frame->v.strideWidth    = CVPixelBufferGetWidth(pixbuf);
        frame->v.sliceHeight    = CVPixelBufferGetHeight(pixbuf);
        frame->pts              = raw->pts;
        
        CVPixelBufferUnlockBaseAddress(pixbuf, kCVPixelBufferLock_ReadOnly);
        DEBUG("%s => %d x %d => %d x %d => %zu bytes",
             frame->fmt.c_str(),
             frame->v.width, frame->v.height,
             frame->v.strideWidth, frame->v.sliceHeight,
             frame->data[0]->size());
        
        // FIXME: something wrong with SDL2 + NV12
        sp<ColorConvert> cc = new ColorConvert(kPixelFormatNV12,
                                               kPixelFormatYUV420P);
        return cc->convert(frame);
        //return frame;
    }

    // FIXME: no color convert here, let client do the job
    sp<MediaFrame> Decoder::readVideo() {
        AVFrame *raw = av_frame_alloc();

        // receive frame from avcodec
        int ret = avcodec_receive_frame(mContext, raw);
        if (ret == AVERROR(EAGAIN)) {
            INFO("%s: need more input %zu", 
                    avcodec_get_name(mContext->codec_id), mInputCount);
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

        // use sws. easy but very slow
        if (raw->format == AV_PIX_FMT_VIDEOTOOLBOX) {
            DEBUG("frame from DecoderToolbox");
        } else if (!mSwsContext && raw->format != AV_PIX_FMT_YUV420P) {
            WARN("swscale %s -> YUV420", 
                    av_get_pix_fmt_name((AVPixelFormat)raw->format));
            mSwsContext = sws_getContext(
                    raw->width, raw->height, (enum AVPixelFormat)raw->format,
                    raw->width, raw->height, AV_PIX_FMT_YUV420P,
                    0,
                    NULL, NULL, NULL);
            DEBUG("width %d height %d, linesize %d %d %d", 
                    raw->width, raw->height,
                    raw->linesize[0], raw->linesize[1], raw->linesize[2]);
        }

        sp<MediaFrame> out;

        if (raw->format == AV_PIX_FMT_VIDEOTOOLBOX) {
            // TODO:
            out = videotoolbox_frame(mContext, raw);
            if (out == NULL) return NULL;
        } else if (mSwsContext) {
            out = new MediaFrame;
            size_t frameSize = (raw->width * raw->height * 3) / 2;
            sp<Buffer> frame = new Buffer(frameSize);
            uint8_t* data[4];
            int linesizes[4];
            av_image_fill_arrays(data, linesizes, 
                    (uint8_t*)frame->data(), AV_PIX_FMT_YUV420P, 
                    raw->width, raw->height, 1);

            sws_scale((SwsContext*)mSwsContext, 
                    raw->data/*slices*/, raw->linesize/*stride*/,
                    0, raw->height, /*slice range*/
                    data, linesizes);
            frame->step(frameSize);

            out->data[0] = frame;
            out->v.strideWidth = raw->width;
            out->v.sliceHeight = raw->height;
        } else {
            out = new MediaFrame;
            out->data[0] = new Buffer(new AVFrameMemory(raw, 0));
            out->data[1] = new Buffer(new AVFrameMemory(raw, 1));
            out->data[2] = new Buffer(new AVFrameMemory(raw, 2));
            out->v.strideWidth = raw->linesize[0];
            out->v.sliceHeight = raw->height;  // FIXME
        }

        out->v.fmt      = kPixelFormatYUV420P;
        out->v.width    = raw->width;
        out->v.height   = raw->height;
        out->pts        = raw->pts;
        av_frame_free(&raw);
        ++mOutputCount;
        return out;
    }
    
    sp<MediaFrame> Decoder::readAudio() {
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
    
    sp<MediaFrame> Decoder::read() {
        if (mType == kCodecTypeAudio)
            return readAudio();
        else
            return readVideo();
    }
}; };
