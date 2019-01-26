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

//#include "mp3/Mp3File.h"
#include "mpeg4/Mp4File.h"
#include "lavc/LavcDecoder.h"
#ifdef __APPLE__
#include "videotoolbox/VTDecoder.h"
#endif
#include "sdl2/SDLAudio.h"
#include "sdl2/SDLVideo.h"
#include "opengl/GLVideo.h"

#if 0
#include "wave/WaveFile.h" 
#include "ape/ApeFile.h"
#include "dsd/DsdFile.h"
#include "aac/AacFile.h"
#include "flac/FlacFile.h"
#endif

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
    

    sp<MediaExtractor> MediaExtractor::Create(sp<Content>& pipe, const Message& params) {
        eFileFormat fileType = (eFileFormat)params.findInt32(kKeyFormat, kFileFormatUnknown);
        // if someone want to force format.
        if (fileType == kFileFormatUnknown) {
            fileType = MediaFormatDetect(*pipe);
            pipe->seek(0);  // reset pipe
        }

        sp<MediaExtractor> module;
        if (fileType == kFileFormatMP3) {
            //module = new Mp3File(pipe, params);
        } else if (fileType == kFileFormatMP4)
            module = new Mp4File(pipe, params);
#if 0
        if (fileType.equals(Media::File::APE)) 
            module = new ApeFile(pipe, params);
        else if (fileType.equals(Media::File::MP3))
            module = new Mp3File(pipe, params);
        else if (fileType.equals(Media::File::WAVE))
            module = new WaveFile(pipe, params);
        else if (fileType.equals(Media::File::DSF) || fileType.equals(Media::File::DFF))
            module = new DsdFile(pipe, params);
        else if (fileType.equals(Media::File::AAC))
            module = new AacFile(pipe, params);
        else if (fileType.equals(Media::File::FLAC))
            module = new FlacFile(pipe, params);
        else if (fileType == Media::File::MP4) 
            module = new Mp4File(pipe, params);
#endif

        if (module == 0 || module->status() != OK) {
            return NULL;
        }

        return module;
    }

    sp<MediaDecoder> MediaDecoder::Create(const Message& formats, const Message& options) {
        sp<MediaDecoder> codec;
        eCodecFormat format = (eCodecFormat)formats.findInt32(kKeyFormat);
        eCodecType type = GetCodecType(format);
        eModeType mode = (eModeType)options.findInt32(kKeyMode, kModeTypeDefault);
        
#ifdef __APPLE__
        if (type == kCodecTypeVideo && mode != kModeTypeSoftware) {
            codec = new VideoToolbox::VTDecoder(formats, options);
            if (codec->status() != OK) codec.clear();
        }
        // FALL BACK TO SOFTWARE DECODER
#endif
        if (codec == NULL) {
            codec = new Lavc::LavcDecoder(formats, options);
        }
        
        if (codec == NULL || codec->status() != OK) {
            return NULL;
        }

        return codec;
    }
};

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
        
        sp<MediaFrame> out = new MediaFrame;
        
        out->v.width        = input->v.width;
        out->v.height       = input->v.height;
        out->v.strideWidth  = input->v.strideWidth;
        out->v.sliceHeight  = input->v.sliceHeight;
        out->pts            = input->pts;
        
        if (mFormat == kPixelFormatYUV420P) {
            const size_t size_y = input->v.strideWidth * input->v.sliceHeight;

            switch (input->v.format) {
                case kPixelFormatNV12:
                {
                    const uint8_t *src_y = (const uint8_t *)input->data[0]->data();
                    const uint8_t *src_uv;
                    if (input->data[1] != NULL) {
                        src_uv = (const uint8_t *)input->data[1]->data();
                    } else {
                        src_uv = src_y + size_y;
                    }
                    out->data[0] = new Buffer((size_y * 3) / 2);
                    uint8_t *dst_y = (uint8_t*)out->data[0]->data();
                    uint8_t *dst_u = dst_y + size_y;
                    uint8_t *dst_v = dst_u + size_y / 4;
                    libyuv::NV12ToI420(
                        src_y,  input->v.strideWidth,
                        src_uv, input->v.strideWidth,
                        dst_y,  input->v.strideWidth,
                        dst_u,  input->v.strideWidth / 2,
                        dst_v,  input->v.strideWidth / 2,
                        input->v.strideWidth,
                        input->v.sliceHeight
                    );
                } break;
                default:
                    FATAL("FIXME");
                    break;
            }
            
            out->data[0]->step((size_y * 3) / 2);
            out->format = kPixelFormatYUV420P;
        } else {
            FATAL("FIXME");
        }
        return out;
    }
};
