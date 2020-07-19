/******************************************************************************
 * Copyright (c) 2020, Chen Fang <mtdcy.chen@gmail.com>
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
 * 3. Neither the name of the copyright holder nor the names of its 
 *    contributors may be used to endorse or promote products derived from 
 *    this software without specific prior written permission.
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


// File:    MediaDevice.cpp
// Author:  mtdcy.chen
// Changes: 
//          1. 20160701     initial version
//

#define LOG_TAG "MediaDevice"
#define LOG_NDEBUG 0

#include "MediaTypes.h"
#include "MediaDevice.h"
#include "id3/ID3.h"
#include "matroska/EBML.h"

#define MAX(a, b) (a > b ? a : b)
#define MIN(a, b) (a > b ? b : a)
#define MASK(n) ((1<<(n))-1)

__BEGIN_NAMESPACE_MFWK

const static UInt32 kCommonHeadLength = 32;
const static UInt32 kScanLength = 32 * 1024ll;

// return score
Int IsMp4File(const sp<ABuffer>&);
Int IsWaveFile(const sp<ABuffer>&);
Int IsMp3File(const sp<ABuffer>&);
static eFileFormat GetFormat(const sp<ABuffer>& data) {
    Int score = 0;

    const Int64 startPos = data->offset();
    DEBUG("startPos = %" PRId64, startPos);

    // formats with 100 score by check fourcc
    {
        static struct {
            const Char *        head;
            const UInt32        skip;
            const Char *        ext;        // extra text
            eFileFormat         format;
        } kFourccMap[] = {
            {"fLaC",    0,  Nil,       kFileFormatFlac     },
            {"RIFF",    4,  "WAVE",     kFileFormatWave     },
            {"RIFF",    4,  "AVI ",     kFileFormatAvi      },
            {"RIFF",    4,  "AVIX",     kFileFormatAvi      },
            {"RIFF",    4,  "AVI\x19",  kFileFormatAvi      },
            {"RIFF",    4,  "AMV ",     kFileFormatAvi      },
            // END OF LIST
            {Nil,      0,  Nil,       kFileFormatUnknown  },
        };
        
        eFileFormat format = kFileFormatUnknown;
        for (UInt32 i = 0; kFourccMap[i].head; i++) {
            const String head = data->rs(strlen(kFourccMap[i].head));
            if (kFourccMap[i].ext) {
                if (kFourccMap[i].skip) data->skipBytes(kFourccMap[i].skip);
                const String ext = data->rs(strlen(kFourccMap[i].ext));
                if (head == kFourccMap[i].head &&
                    ext == kFourccMap[i].ext) {
                    format = kFourccMap[i].format;
                    break;
                }
            } else {
                if (head == kFourccMap[i].head) {
                    format = kFourccMap[i].format;
                    break;
                }
            }
            
            data->resetBytes();
        }
        
        // put buffer back to its begin
        if (format != kFileFormatUnknown) return format;
    }

    // formats with lower score by scanning header
    struct {
        Int (*scanner)(const sp<ABuffer>&);
        eFileFormat format;
    } kScanners[] = {
        { IsWaveFile,           kFileFormatWave     },
        { IsMp4File,            kFileFormatMp4      },
        { EBML::IsMatroskaFile, kFileFormatMkv      },
        { IsMp3File,            kFileFormatMp3      },  // this one should locate at end
        { Nil,                 kFileFormatUnknown  }
    };

    eFileFormat format = kFileFormatUnknown;
    for (UInt32 i = 0; kScanners[i].scanner; i++) {
        // reset buffer to its begin
        data->resetBytes();
        Int c = kScanners[i].scanner(data);
        DEBUG("%4s, score = %d", (const Char *)&kScanners[i].format, c);
        if (c > score) {
            score = c;
            format = kScanners[i].format;

            if (score >= 100) break;
        }
    }

    return format;
}

sp<MediaDevice> CreateMp3File(const sp<ABuffer>&);
sp<MediaDevice> CreateMp4File(const sp<ABuffer>&);
sp<MediaDevice> CreateMatroskaFile(const sp<ABuffer>&);
sp<MediaDevice> CreateWaveFile(const sp<ABuffer>&);
sp<MediaDevice> CreateFlacFile(const sp<ABuffer>&);
#ifdef __APPLE__
sp<MediaDevice> CreateVideoToolboxDecoder(const sp<Message>& formats, const sp<Message>& options);
sp<MediaDevice> CreateAudioToolbox(const sp<Message>& formats, const sp<Message>& options);
Bool IsVideoToolboxSupported(eVideoCodec format);
#endif
#ifdef WITH_FFMPEG
sp<MediaDevice> CreateLibavformat(const sp<ABuffer>&);
sp<MediaDevice> CreateLavcDecoder(const sp<Message>& formats, const sp<Message>& options);
#endif

sp<MediaDevice> CreateOpenALOut(const sp<Message>& formats, const sp<Message>& options);
sp<MediaDevice> CreateOpenGLOut(const sp<Message>& formats, const sp<Message>& options);
#ifdef WITH_SDL
sp<MediaDevice> CreateSDLAudio(const sp<Message>& formats, const sp<Message>& options);
#endif

sp<MediaDevice> CreateMp3Packetizer();

sp<MediaDevice> MediaDevice::create(const sp<Message>& formats, const sp<Message>& options) {
    // ENV
    String env0 = GetEnvironmentValue("FORCE_AVFORMAT");
    String env1 = GetEnvironmentValue("FORCE_AVCODEC");
    Bool FORCE_AVFORMAT = env0.equals("1") || env0.lower().equals("yes");
    Bool FORCE_AVCODEC = env1.equals("1") || env1.lower().equals("yes");
    
    UInt32 format = formats->findInt32(kKeyFormat, kFormatUnknown);
    sp<ABuffer> buffer = formats->findObject(kKeyContent);
    if (format == kFormatUnknown && !buffer.isNil()) {
        sp<ABuffer> head = buffer->readBytes(kScanLength);
        buffer->skipBytes(-head->size());   // reset our buffer read pointer
        
        // skip id3v2, id3v2 is bad for file format detection
        if (ID3::SkipID3v2(head) == kMediaNoError) {
            head = head->readBytes(head->size());
        } else {
            head->resetBytes();
        }
        
        format = GetFormat(head);
    }
    
    if (format == 0) {
        ERROR("create device failed, unknown format");
        return Nil;
    }
    
    UInt32 mode = kModeTypeDefault;
    if (!options.isNil()) {
        mode = options->findInt32(kKeyMode, kModeTypeDefault);
    }
    
#ifdef WITH_FFMPEG
    if (mode == kModeTypeSoftware) {
        FORCE_AVCODEC = True;
    }
#endif
    
    switch (format) {
        case kFileFormatWave:
            return CreateWaveFile(buffer);
        case kFileFormatMp3:
            return CreateMp3File(buffer);
        case kFileFormatMp4:
            return CreateMp4File(buffer);
        case kFileFormatMkv:
            return CreateMatroskaFile(buffer);
        case kFileFormatApe:
        case kFileFormatFlac:
        case kFileFormatAvi:
        case kFileFormatLAVF:
#ifdef WITH_FFMPEG
            return CreateLibavformat(buffer);
#else
            return Nil;
#endif
        case kAudioCodecAAC:
        case kAudioCodecAC3:
#if 0 //def __APPLE__
            return CreateAudioToolbox(formats, options);
#elif defined(WITH_FFMPEG)
            return CreateLavcDecoder(formats, options);
#else
            break;
#endif
        case kAudioCodecMP3:
        case kAudioCodecAPE:
        case kAudioCodecWMA:
        case kAudioCodecDTS:
        case kAudioCodecFLAC:
        case kAudioCodecLAVC:
#ifdef WITH_FFMPEG
            return CreateLavcDecoder(formats, options);
#else
            break;
#endif
        case kVideoCodecMPEG4:
        case kVideoCodecH263:
        case kVideoCodecH264:
        case kVideoCodecHEVC:
        case kVideoCodecVP8:
        case kVideoCodecVP9:
        case kVideoCodecVC1:
        case kVideoCodecLAVC:
        case kVideoCodecMicrosoftMPEG4:
#ifdef WITH_FFMPEG
            if (FORCE_AVCODEC) {
                return CreateLavcDecoder(formats, options);
            }
#endif
            
#ifdef __APPLE__
            if (IsVideoToolboxSupported(format)) {
                return CreateVideoToolboxDecoder(formats, options);
            }
#endif

#ifdef WITH_FFMPEG
            return CreateLavcDecoder(formats, options);
#else
            break;
#endif
            
        case kImageCodecBMP:
        case kImageCodecJPEG:
        case kImageCodecGIF:
        case kImageCodecPNG:
            // TODO
            break;

        case kSampleFormatS16:
        case kSampleFormatS16Packed:
        case kSampleFormatS32:
        case kSampleFormatS32Packed:
            // only s16 & s32 sample format
#if defined(WITH_SDL) && !defined(__APPLE__)
            return CreateSDLAudio(formats, options);
#else
            return CreateOpenALOut(formats, options);
#endif
        case kPixelFormat420YpCbCrPlanar:
        case kPixelFormat420YpCbCrSemiPlanar:
        case kPixelFormat420YpCrCbSemiPlanar:
        case kPixelFormatARGB:
        case kPixelFormatRGBA:
        case kPixelFormatBGRA:
        case kPixelFormatABGR:
        case kPixelFormatRGB:
        case kPixelFormatBGR:
        case kPixelFormatRGB565:
        case kPixelFormatBGR565:
        case kPixelFormatVideoToolbox:
            return CreateOpenGLOut(formats, options);
        default:
            break;
    }
    
    ERROR("no media device for [%.4s]", (const Char *)&format);
    return Nil;
}

void MediaDevice::onFirstRetain() {
    
}

void MediaDevice::onLastRetain() {
    
}

__END_NAMESPACE_MFWK
