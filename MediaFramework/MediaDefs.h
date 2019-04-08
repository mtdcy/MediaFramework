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

#include <ABE/ABE.h>

#ifndef FORCE_INLINE 
#define FORCE_INLINE    __ABE_INLINE
#endif

#ifndef API_EXPORT
#if defined(_WIN32) || defined(__MINGW32__)
#ifdef BUILD_DLL
#define API_EXPORT      __declspec(dllexport)
#else
#define API_EXPORT      __declspec(dllimport)
#endif
#else
#define API_EXPORT      __attribute__ ((__visibility__("default")))
#endif
#endif

#ifndef API_DEPRECATED
#define API_DEPRECATED __ABE_DEPRECATED
#endif

__BEGIN_DECLS

// common constants and basic types
typedef enum {
    kFileFormatUnknown      = 0,
    kFileFormatMP3,
    kFileFormatMP4,
    kFileFormatFlac,
    kFileFormatWave,
    kFileFormatAVI,
    kFileFormatMKV,
    kFileFormatAAC,
} eFileFormat;

typedef enum {
    kCodecFormatUnknown     = 0,
    // audio
    kAudioCodecFormatFirst  = 0x100,
    kAudioCodecFormatPCM,
    kAudioCodecFormatFLAC,
    kAudioCodecFormatMP3,
    kAudioCodecFormatVorbis,
    kAudioCodecFormatAAC,
    kAudioCodecFormatAC3,
    kAudioCodecFormatWMA,
    kAudioCodecFormatAPE,
    kAudioCodecFormatDTS,
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
} eCodecFormat;

typedef enum {
    kCodecTypeUnknown       = 0,
    kCodecTypeVideo         = 1,
    kCodecTypeAudio         = 2,
    kCodecTypeSubtitle      = 3,
    kCodecTypeImage         = 4,
    kCodecTypeMax,
} eCodecType;

eCodecType GetCodecType(eCodecFormat format);

typedef enum {
    kPixelFormatUnknown     = 0,        ///< Unknown

    // about yuv: http://www.fourcc.org/yuv.php
    // planar yuv
    kPixelFormatYUV420P,                ///< Planar YUV 4:2:0, 12bpp, 3 planes Y/U/V
    kPixelFormatYUV422P,                ///< Planar YUV 4:2:2, 16bpp, 3 planes Y/U/V
    kPixelFormatYUV444P,                ///< Planar YUV 4:4:4, 24bpp, 3 planes Y/U/V
    kPixelFormatNV12,                   ///< Planar YUV 4:2:0, 12bpp, 2 planes Y/UV(interleaved)
    kPixelFormatNV21,                   ///< same as nv12, but uv are swapped
    // packed yuv
    kPixelFormatYUYV422     = 0x100,    ///< Packed YUV 4:2:2, 16bpp, Y0 Cb Y1 Cr
    kPixelFormatUYVY422,                ///< Packed YUV 4:2:2, 16bpp, Cb Y0 Cr Y1
    // packed rgb
    kPixelFormatRGB565      = 0x200,    ///< packed RGB 5:6:5, 16 bpp
    kPixelFormatRGB888,                 ///< packed RGB 8:8:8, 24 bpp
    kPixelFormatARGB,                   ///< packed ARGB, 32 bpp, AARRGGBB
    kPixelFormatRGBA,                   ///< packed RGBA, 32 bpp, RRGGBBAA
} ePixelFormat;

// FIXME: code sample infomation into format
typedef enum {
    kSampleFormatUnknown    = 0,
    kSampleFormatU8,
    kSampleFormatS16,
    kSampleFormatS24,
    kSampleFormatS32,
    kSampleFormatFLT,
    kSampleFormatDBL,
} eSampleFormat;

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
#define kKeyFormat      "format"    ///< int32_t, mandatory
#define kKeyRequestFormat   "request-format"    ///< int32_t

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
#define kKeyChannels    "channels"      ///< int32_t, mandatory for audio
#define kKeySampleRate  "sample-rate"   ///< int32_t, mandatory for audio

/**
 * video display width and height
 */
#define kKeyWidth       "width"     ///< int32_t, mandatory for video
#define kKeyHeight      "height"    ///< int32_t, mandatory for video

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
 * opengl compatible frame
 * used for passing opengl compatible memory to MediaOut,
 * to avoid memory copy
 */
#define kKeyOpenGLCompatible    "opengl"  ///< int32_t, bool

/**
 * csd keys
 * preferred format of csd
 */
#define kKeyESDS                "esds"  ///< Buffer
#define kKeyavcC                "avcC"  ///< Buffer
#define kKeyhvcC                "hvcC"  ///< Buffer
/**
 * raw codec specific data
 */
#define kKeyCodecSpecificData   "csd"   ///< Buffer

enum MediaError {
    kMediaNoError                   = 0,
    kMediaErrorUnknown              = -10000,
    kMediaErrorNotSupported         = -10001,
    kMediaErrorInvalidOperation     = -10002,
    kMediaErrorNotInitialied        = -10003,
    kMediaErrorBadValue             = -10004,
    kMediaErrorTryAgain             = -10005,
    kMediaErrorResourceBusy         = -10006,
    kMediaErrorBadFormat            = -10007,   ///<
    kMediaErrorNoMoreData           = -10008,
};

__END_DECLS

#ifdef __cplusplus
#define __BEGIN_NAMESPACE_MPX   __BEGIN_NAMESPACE(mpx)
#define __END_NAMESPACE_MPX     __END_NAMESPACE(mpx)
#define __USING_NAMESPACE_MPX   __USING_NAMESPACE(mpx)

USING_NAMESPACE_ABE

__BEGIN_NAMESPACE_MPX

struct AudioFormat {
    eSampleFormat       format;         ///< audio sample format @see eSampleFormat
    int32_t             channels;       ///< audio channels
    int32_t             freq;           ///< audio sample rate
    int32_t             samples;        ///< samples per channel
};

struct Rect {
    int32_t             x;
    int32_t             y;
    int32_t             w;
    int32_t             h;
};

struct ImageFormat {
    ePixelFormat        format;         ///< image pixel format
    int32_t             width;          ///< plane width
    int32_t             height;         ///< plane height
    Rect                rect;           ///< display rectangle
};


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
namespace Tag {
    class API_EXPORT Parser {
        public:
            FORCE_INLINE Parser() { }
            FORCE_INLINE virtual ~Parser() { }
            virtual status_t parse(const Buffer& data) = 0;
            FORCE_INLINE const Message& values() const { return mValues; }

        protected:
            Message             mValues;
    };

    class API_EXPORT Writter {
        public:
            FORCE_INLINE Writter() { }
            FORCE_INLINE virtual ~Writter() { }
            virtual status_t synth(const Message& data) = 0;
            //const Buffer&       values() const {  }

        protected:
            //Buffer              mBuffer;
    };
};

__END_NAMESPACE_MPX

#endif // __cplusplus
#endif // _MEDIA_MODULES_MEDIADEFS_H

