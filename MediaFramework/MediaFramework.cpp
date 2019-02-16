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


// File:    Module.h
// Author:  mtdcy.chen
// Changes: 
//          1. 20160701     initial version
//

#define tkLOG_TAG "Module"
#include <MediaToolkit/Toolkit.h>

#include "MediaDefs.h"
#include "ColorConvertor.h"
#include "MediaDecoder.h"
#include "MediaExtractor.h"
#include "MediaPacketizer.h"
#include "sdl2/SDLAudio.h"
#include "opengl/GLVideo.h"

extern "C" {
    eCodecType GetCodecType(eCodecFormat format) {
        if (format > kAudioCodecFormatFirst && format < kAudioCodecFormatLast)
            return kCodecTypeAudio;
        else if (format > kVideoCodecFormatFirst && format < kVideoCodecFormatLast)
            return kCodecTypeVideo;
        else if (format > kSubtitleFormatFirst && format < kSubtitleFormatLast)
            return kCodecTypeSubtitle;
        else if (format > kImageCodecFormatFirst && format < kImageCodecFormatLast)
            return kCodecTypeImage;
        else
            return kCodecTypeUnknown;
    }
}

namespace mtdcy {
    
    sp<MediaExtractor> CreateMp3File();
    sp<MediaExtractor> CreateMp4File();
    sp<MediaExtractor> CreateMatroskaFile();
    sp<MediaExtractor> MediaExtractor::Create(eFileFormat format) {
        switch (format) {
            case kFileFormatMP3:
                return CreateMp3File();
            case kFileFormatMP4:
                return CreateMp4File();
            case kFileFormatMKV:
                return CreateMatroskaFile();
            default:
                break;
        }
    }

#ifdef __APPLE__
    sp<MediaDecoder> CreateVideoToolboxDecoder();
    bool IsVideoToolboxSupported(eCodecFormat format);
#endif
    sp<MediaDecoder> CreateLavcDecoder(eModeType mode);
    sp<MediaDecoder> MediaDecoder::Create(eCodecFormat format, eModeType mode) {
        sp<MediaDecoder> codec;
        eCodecType type = GetCodecType(format);
        
#ifdef __APPLE__
        if (type == kCodecTypeVideo && mode != kModeTypeSoftware && IsVideoToolboxSupported(format)) {
            codec = CreateVideoToolboxDecoder();
        }
        // FALL BACK TO SOFTWARE DECODER
#endif
        if (codec == NULL) {
            codec = CreateLavcDecoder(mode);
        }

        return codec;
    }
    
    sp<MediaPacketizer> CreateMp3Packetizer();
    sp<MediaPacketizer> MediaPacketizer::Create(eCodecFormat format) {
        switch (format) {
            case kAudioCodecFormatMP3:
                return CreateMp3Packetizer();
            default:
                INFO("no packetizer for %d", format);
                return NULL;
        }
    }
};

using namespace mtdcy;

struct PixelFormatDesc {
    const char *    desc;
    const size_t    n_planes;
    const float     n_size[MEDIA_FRAME_NB_PLANES];
};

static PixelFormatDesc s_yuv[] = {
    {   // kPixelFormatUnknown
        .desc       = "unknown",
        .n_planes   = 0,
    },
    {   // kPixelFormatYUV420P
        .desc       = "planar yuv 4:2:0, 3 planes Y/U/V",
        .n_planes   = 3,
        .n_size     = { 1, 0.25, 0.25 },
    },  // kPixelFormatYUV422P
    {
        .desc       = "planar yuv 4:2:2, 3 planes Y/U/V",
        .n_planes   = 3,
        .n_size     = { 1, 0.5, 0.5 },
    }
};

struct SampleFormatDesc {
    const char *    desc;
    const size_t    n_size;     // sample size in bytes
};

static SampleFormatDesc s_samples[] = {
    {   // kSampleFormatUnknown
        .desc       = "unknown",
    },
    {   // kSampleFormatU8
        .desc       = "unsigned 8 bit sample",
        .n_size     = 1,
    },
    {   // kSampleFormatS16
        .desc       = "signed 16 bit sample",
        .n_size     = 2,
    },
    {   // kSampleFormatS24
        .desc       = "signed 24 bit sample",
        .n_size     = 3,
    },
    {   // kSampleFormatS32
        .desc       = "signed 32 bit sample",
        .n_size     = 4,
    },
    {   // kSampleFormatFLT
        .desc       = "float sample",
        .n_size     = sizeof(float),
    },
};

MediaFrame::MediaFrame() : pts(kTimeInvalid), duration(kTimeInvalid) {
    for (size_t i = 0; i < MEDIA_FRAME_NB_PLANES; ++i) {
        planes[i].data = NULL;
        planes[i].size = 0;
    }
    format = 0;
    opaque = NULL;
}

struct DefaultMediaFrame : public MediaFrame {
    DefaultMediaFrame() : MediaFrame() { }
    virtual ~DefaultMediaFrame() { }
    sp<Buffer> buffer[MEDIA_FRAME_NB_PLANES];
};

namespace mtdcy {
    sp<MediaFrame> MediaFrameCreate(ePixelFormat format, int32_t w, int32_t h) {
        sp<DefaultMediaFrame> frame = new DefaultMediaFrame;
        const size_t size = w * h;
        
        if (format > kPixelFormatUnknown && format < kPixelFormatYUYV422) {
            const PixelFormatDesc *desc = &s_yuv[format];
            for (size_t i = 0; i < desc->n_planes; ++i) {
                frame->planes[i].size   = size * desc->n_size[i];
                frame->buffer[i] = new Buffer(frame->planes[i].size);
                frame->planes[i].data = (uint8_t*)frame->buffer[i]->data();
            }
        } else {
            FATAL("FIXME");
        }
        frame->v.format     = format;
        frame->v.width      = w;
        frame->v.height     = h;
        frame->v.rect.x     = 0;
        frame->v.rect.y     = 0;
        frame->v.rect.w     = w;
        frame->v.rect.h     = h;
        return frame;
    }
    
    sp<MediaFrame> MediaFrameCreate(eSampleFormat format, bool planar, int32_t channels, int32_t freq, int32_t samples) {
        sp<DefaultMediaFrame> frame = new DefaultMediaFrame;
        const SampleFormatDesc *desc = &s_samples[format];
        if (planar) {
            for (size_t i = 0; i < channels; ++i) {
                frame->planes[i].size   = desc->n_size * samples;
                frame->buffer[i]        = new Buffer(frame->planes[i].size);
                frame->planes[i].data   = (uint8_t*)frame->buffer[i]->data();
            }
        } else {
            frame->planes[0].size       = desc->n_size * samples * channels;
            frame->buffer[0]            = new Buffer(frame->planes[0].size);
            frame->planes[0].data       = (uint8_t*)frame->buffer[0]->data();
        }
        frame->a.format     = format;
        frame->a.channels   = channels;
        frame->a.freq       = freq;
        frame->a.samples    = samples;
        return frame;
    }
}

struct DefaultMediaPacket : public MediaPacket {
    DefaultMediaPacket() : MediaPacket() { }
    virtual ~DefaultMediaPacket() { }
    sp<Buffer> buffer;
};

namespace mtdcy {
    sp<MediaPacket> MediaPacketCreate(size_t size) {
        sp<Buffer> buffer = new Buffer(size);
        sp<DefaultMediaPacket> packet = new DefaultMediaPacket;
        packet->buffer  = buffer;
        packet->data    = (uint8_t*)buffer->data();
        packet->size    = buffer->capacity();
        return packet;
    }
    
    sp<MediaPacket> MediaPacketCreate(sp<Buffer>& data) {
        sp<DefaultMediaPacket> packet = new DefaultMediaPacket;
        packet->buffer  = data;
        packet->data    = (uint8_t*)data->data();
        packet->size    = data->size();
        return packet;
    }
}

#include <libyuv.h>
namespace mtdcy {
    
    ColorConvertor::ColorConvertor(ePixelFormat pixel) :
    mFormat(pixel)
    {
        INFO("=> %#x", pixel);
    }
    
    ColorConvertor::~ColorConvertor() {
    }
    
    struct {
        ePixelFormat    a;
        uint32_t        b;
    } kMap[] = {
        { kPixelFormatNV12,     libyuv::FOURCC_NV12 },
        // END OF LIST
        { kPixelFormatUnknown,  0}
    };
    
    uint32_t get_libyuv_pixel_format(ePixelFormat a) {
        for (size_t i = 0; kMap[i].a != kPixelFormatUnknown; ++i) {
            if (kMap[i].a == a) return kMap[i].b;
        }
        FATAL("FIXME");
        return libyuv::FOURCC_ANY;
    }
    
    sp<MediaFrame> ColorConvertor::convert(const sp<MediaFrame>& input) {
        if (input->v.format == mFormat) return input;
        
        sp<MediaFrame> out = MediaFrameCreate(mFormat, input->v.width, input->v.height);
        out->v              = input->v;
        out->v.format       = mFormat;
        out->pts            = input->pts;
        out->duration       = input->duration;
        
        if (mFormat == kPixelFormatYUV420P) {
            switch (input->v.format) {
                case kPixelFormatNV12:
                {
                    libyuv::NV12ToI420(
                        input->planes[0].data,  input->v.width,
                        input->planes[1].data, input->v.width,
                        out->planes[0].data,  out->v.width,
                        out->planes[1].data,  out->v.width / 2,
                        out->planes[2].data,  out->v.width / 2,
                        input->v.width,
                        input->v.height
                    );
                } break;
                default:
                    FATAL("FIXME");
                    break;
            }
        } else {
            FATAL("FIXME");
        }
        return out;
    }
};
