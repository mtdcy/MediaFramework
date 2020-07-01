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

__BEGIN_NAMESPACE_MPX

const static size_t kCommonHeadLength = 32;
const static size_t kScanLength = 32 * 1024ll;

// return score
int IsMp4File(const sp<ABuffer>&);
int IsWaveFile(const sp<ABuffer>&);
int IsMp3File(const sp<ABuffer>&);
static eFileFormat GetFormat(const sp<ABuffer>& data) {
    int score = 0;

    const int64_t startPos = data->offset();
    DEBUG("startPos = %" PRId64, startPos);

    // formats with 100 score by check fourcc
    {
        static struct {
            const char *        head;
            const size_t        skip;
            const char *        ext;        // extra text
            eFileFormat         format;
        } kFourccMap[] = {
            {"fLaC",    0,  NULL,       kFileFormatFlac     },
            {"RIFF",    4,  "WAVE",     kFileFormatWave     },
            {"RIFF",    4,  "AVI ",     kFileFormatAvi      },
            {"RIFF",    4,  "AVIX",     kFileFormatAvi      },
            {"RIFF",    4,  "AVI\x19",  kFileFormatAvi      },
            {"RIFF",    4,  "AMV ",     kFileFormatAvi      },
            // END OF LIST
            {NULL,      0,  NULL,       kFileFormatUnknown  },
        };
        
        eFileFormat format = kFileFormatUnknown;
        for (size_t i = 0; kFourccMap[i].head; i++) {
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
        int (*scanner)(const sp<ABuffer>&);
        eFileFormat format;
    } kScanners[] = {
        { IsWaveFile,           kFileFormatWave     },
        { IsMp4File,            kFileFormatMp4      },
        { EBML::IsMatroskaFile, kFileFormatMkv      },
        { IsMp3File,            kFileFormatMp3      },  // this one should locate at end
        { NULL,                 kFileFormatUnknown  }
    };

    eFileFormat format = kFileFormatUnknown;
    for (size_t i = 0; kScanners[i].scanner; i++) {
        // reset buffer to its begin
        data->resetBytes();
        int c = kScanners[i].scanner(data);
        DEBUG("%4s, score = %d", (const char *)&kScanners[i].format, c);
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
sp<MediaDevice> CreateLibavformat(const sp<ABuffer>&);
sp<MediaDevice> CreateWaveFile(const sp<ABuffer>&);

#ifdef __APPLE__
sp<MediaDevice> CreateVideoToolboxDecoder(const sp<Message>& formats, const sp<Message>& options);
sp<MediaDevice> CreateAudioToolbox(const sp<Message>& formats, const sp<Message>& options);
bool IsVideoToolboxSupported(eVideoCodec format);
#endif
#ifdef WITH_FFMPEG
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
    bool FORCE_AVFORMAT = env0.equals("1") || env0.lower().equals("yes");
    bool FORCE_AVCODEC = env1.equals("1") || env1.lower().equals("yes");
    
    uint32_t format = formats->findInt32(kKeyFormat, 0);
    sp<ABuffer> buffer = formats->findObject(kKeyContent);
    if (format == 0 && !buffer.isNIL()) {
        sp<ABuffer> head = buffer->readBytes(kScanLength);
        buffer->skipBytes(-head->size());   // reset our buffer read pointer
        
        // skip id3v2, id3v2 is bad for file format detection
        if (ID3::SkipID3v2(head) == kMediaNoError) {
            head = head->readBytes(head->size());
        }
        
        format = GetFormat(head);
    }
    
    if (format == 0) {
        ERROR("create device failed, unknown format");
        return NULL;
    }
    
    uint32_t mode = kModeTypeDefault;
    if (!options.isNIL()) {
        mode = options->findInt32(kKeyMode, kModeTypeDefault);
    }
    
#ifdef WITH_FFMPEG
    if (mode == kModeTypeSoftware) {
        FORCE_AVCODEC = true;
    }
#endif
    
    switch (format) {
        case kFileFormatWave:
            return FORCE_AVFORMAT ? CreateLibavformat(buffer) : CreateWaveFile(buffer);
        case kFileFormatMp3:
            return FORCE_AVFORMAT ? CreateLibavformat(buffer) : CreateMp3File(buffer);
        case kFileFormatMp4:
            return FORCE_AVFORMAT ? CreateLibavformat(buffer) : CreateMp4File(buffer);
        case kFileFormatMkv:
            return FORCE_AVFORMAT ? CreateLibavformat(buffer) : CreateMatroskaFile(buffer);
        case kFileFormatApe:
        case kFileFormatFlac:
        case kFileFormatAvi:
        case kFileFormatLAVF:
            return CreateLibavformat(buffer);
        case kAudioCodecAAC:
        case kAudioCodecAC3:
#ifdef __APPLE__
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
        case kPixelFormat420YpCrCbPlanar:
        case kPixelFormat420YpCbCrSemiPlanar:
        case kPixelFormat420YpCrCbSemiPlanar:
        case kPixelFormatVideoToolbox:
            return CreateOpenGLOut(formats, options);
        default:
            break;
    }
    
    ERROR("no media device for [%.4s]", (const char *)&format);
    return NULL;
}

__END_NAMESPACE_MPX
