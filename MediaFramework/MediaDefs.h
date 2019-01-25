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


// File:    MediaDefs.h
// Author:  mtdcy.chen
// Changes: 
//          1. 20160701     initial version
//

#ifndef _MEDIA_MODULES_MEDIADEFS_H
#define _MEDIA_MODULES_MEDIADEFS_H 

#include <MediaToolkit/Toolkit.h>

__BEGIN_DECLS

// common constants and basic types
enum eFileFormat {
    kFileFormatUnknown      = 0,
    kFileFormatMP3,
    kFileFormatMP4,
    kFileFormatFlac,
    kFileFormatWave,
    kFileFormatAVI,
    kFileFormatMKV,
    kFileFormatAAC,
};

enum eCodecFormat {
    kCodecFormatUnknown     = 0,
    // audio
    kAudioCodecFormatFirst  = 0x100,
    kAudioCodecFormatPCM,
    kAudioCodecFormatFLAC,
    kAudioCodecFormatMP3,
    kAudioCodecFormatVorbis,
    kAudioCodecFormatAAC,
    kAudioCodecFormatWMA,
    kAudioCodecFormatAPE,
    kAudioCodecFormatLast   = 0x200 - 1,
    // video
    kVideoCodecFormatFirst  = 0x200,
    kVideoCodecFormatH264,
    kVideoCodecFormatHEVC,
    kVideoCodecFormatMPEG4,
    kVideoCodecFormatVC1,
    kVideoCodecFormatH263,
    kVideoCodecFormatLast   = 0x300 - 1,
    // subtitle
    kSubtitleFormatFirst    = 0x300,
    kSubtitleFormatLast     = 0x400 - 1,
    // image
    kImageCodecFormatFirst  = 0x400,
    kImageCodecFormatPNG,
    kImageCodecFormatJPEG,
    kImageCodecFormatBMP,
    kImageCodecFormatGIF,
    kImageCodecFormatLast   = 0x500 - 1,
};

enum eCodecType {
    kCodecTypeUnknown       = 0,
    kCodecTypeAudio         = 1,
    kCodecTypeVideo         = 2,
    kCodecTypeSubtitle      = 3,
    kCodecTypeImage         = 4,
    kCodecTypeMax,
};

eCodecType GetCodecType(eCodecFormat format);

enum ePixelFormat {
    kPixelFormatUnknown     = 0,    ///< Unknown
    
    kPixelFormatYUV420P     = 0x100,///< Planar YUV 4:2:0, 12bpp
    kPixelFormatYUV422P,            ///< Planar YUV 4:2:2, 16bpp
    kPixelFormatNV12,               ///< 2 planes Y/UV, 12 bpp
                                    ///< 1 plane for Y, 1 plane for UV(interleaved)
    kPixelFormatNV21,               ///< 2 planes Y/VU, 12 bpp
    kPixelFormatRGB565      = 0x200,///< packed RGB 5:6:5, 16 bpp
    kPixelFormatRGB888,             ///< packed RGB 8:8:8, 24 bpp
    kPixelFormatARGB,               ///< packed ARGB, 32 bpp, AARRGGBB
    kPixelFormatRGBA,               ///< packed RGBA, 32 bpp, RRGGBBAA
    
    // hardware format
#ifdef __APPLE__
    kPixelFormatVideoToolbox    = 0x10000,
#endif
};

// FIXME: code sample infomation into format
enum eSampleFormat {
    kSampleFormatUnknown    = 0,
    kSampleFormatS16,
    kSampleFormatS24,
    kSampleFormatS32,
    kSampleFormatFLT,
};

enum eModeType {
    kModeTypeNormal     = 0,                ///< use hardware accel if available
    kModeTypeSoftware,                      ///< no hardware accel.
    kModeTypePreview,
    kModeTypeDefault    = kModeTypeNormal
};

/**
 * read behavior modes
 */
enum eModeReadType {
    kModeReadFirst      = 0,    ///< read first sync packet, -ts
    kModeReadNext,              ///< read next packet, -ts
    kModeReadLast,              ///< read last packet, -ts
    kModeReadCurrent,           ///< read current packet again, -ts
    kModeReadNextSync,          ///< read next sync packet, -ts/+ts
    kModeReadLastSync,          ///< read last sync packet, -ts/+ts
    kModeReadClosestSync,       ///< read closest sync packet, +ts
                                ///< @note only for seek, as direction can NOT be predict
    kModeReadIndex,             ///< read sample of index, +ts as sample index
    kModeReadPeek,              ///< peek packet, +ts
    kModeReadDefault = kModeReadNext
};

enum {
    kFrameFlagNone      = 0,
    kFrameFlagSync      = (1<<0),   ///< I frame, sync frame
    kFrameFlagPred      = (1<<1),   ///< P frame
    kFrameFlagBi        = (1<<2),   ///< B frame
    kFrameFlagReference = (1<<3),   ///< ref frame, no output
    kFrameFlagLeading   = (1<<4),   ///< leading frame, depends on frame before I frame
    kFrameFlagDisposal  = (1<<5),   ///< not be depended on, disposal
    kFrameFlagRedundant = (1<<6),   ///< multiple (redundant) encodings
};


/**
 * key of format
 * @see eFileFormat
 * @see eCodecFormat
 * @see ePixelFormat
 * @see eSampleFormat
 */
#define kKeyFormat      "format"    ///< int32_t

/**
 * key of mode
 * @see eModeType
 * @see eModeReadType
 */
#define kKeyMode        "mode"      ///< int32_t

/**
 * key of time and duration
 * @see MediaTime
 */
#define kKeyTime        "time"      ///< MediaTime
#define kKeyDuration    "duration"  ///< MediaTime
#define kKeyLatency     "latency"   ///< MediaTime

/**
 * audio channel count and sample rate
 */
#define kKeyChannels    "channels"      ///< int32_t
#define kKeySampleRate  "sample-rate"   ///< int32_t

/**
 * video display width and height
 */
#define kKeyWidth       "width"     ///< int32_t
#define kKeyHeight      "height"    ///< int32_t

/**
 * comman keys
 */
#define kKeyCount       "count"     ///< int32_t
#define kKeyResult      "result"    ///< int32_t, status_t


/**
 * configure keys
 * kKeyRealTime - decode not faster than 1x realtime, power saving
 */
#define kKeyRealTime    "realtime"  ///< int32_t, bool

/**
 *
 */
#define kKeyMask        "mask"      ///< int32_t

/**
 * some device need to pause/unpause implicit
 */
#define kKeyPause       "pause"     ///< int32_t, bool

/**
 * hardware frame
 */
#define kKeyHardwareFrame   "hw-frame"  ///< int32_t, bool 

__END_DECLS

#ifdef __cplusplus
namespace mtdcy {

    // FIXME: use macro to replace these constants
    namespace Media {
        ///////////////////////////////////////////////////////////////////////////
        // for media meta data
        //!>> strings
        static const char *Album                = "album";
        static const char *Artist               = "artist";
        static const char *Author               = "author";
        static const char *Composer             = "composer";
        static const char *Performer            = "performer";
        static const char *Title                = "title";
        static const char *Genre                = "genre";
        static const char *Writer               = "writer";
        static const char *Date                 = "date";
        static const char *Year                 = "year";
        static const char *AlbumArtist          = "album-artist";
        static const char *Compilation          = "compilation";
        static const char *Comment              = "comment";
        static const char *CDTrackNum           = "cd-track-number";
        static const char *DiskNum              = "disc-number";
        static const char *TextLang             = "text-language";  // string
        static const char *Location             = "location";       // string
        static const char *Copyright            = "copyright";
        static const char *License              = "licenses";
        //!>> buffer
        static const char *AlbumArt             = "pic-album-art";  //!>> buffer.

        static const char *ID3v1                = "id3v1";          // Message
        static const char *ID3v2                = "id3v2";          // Message 
        static const char *APEv1                = "apev1";          // Message 
        static const char *APEv2                = "apev2";          // Message 
        static const char *VorbisComment        = "vorbis";         // Message
        
        ///////////////////////////////////////////////////////////////////////////
        // For Mp3 & AAC [gapless playback]
        static const char *EncoderDelay         = "encoder-delay";  // int32_t
        static const char *EncoderPadding       = "encoder-padding";// int32_t 
    };

    ///////////////////////////////////////////////////////////////////////////
    class Packetizer {
        public:
            Packetizer() {}
            virtual ~Packetizer() {}

        public:
            // return true on success, false otherwise 
            virtual bool        enqueue(const Buffer& in)   = 0;
            virtual bool        dequeue(Buffer& out)   = 0;
            virtual void        flush() = 0;

        private:
            DISALLOW_EVILS(Packetizer);
    };

    ///////////////////////////////////////////////////////////////////////////
    namespace Tag {
        class Parser {
            public:
                Parser() { }
                virtual ~Parser() { }
                virtual status_t    parse(const Buffer& data) = 0;
                const Message&      values() const { return mValues; }

            protected:
                Message             mValues;
        };

        class Writter {
            public:
                Writter() { }
                virtual ~Writter() { }
                virtual status_t    synth(const Message& data) = 0;
                //const Buffer&       values() const {  }

            protected:
                //Buffer              mBuffer;
        };
    };
    
    
    ///////////////////////////////////////////////////////////////////////////
};
#endif
#endif // _MEDIA_MODULES_MEDIADEFS_H
