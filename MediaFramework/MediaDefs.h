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
#define FORCE_INLINE    ABE_INLINE
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
#define API_DEPRECATED  ABE_DEPRECATED
#endif

__BEGIN_DECLS

/**
 * constrants & types that should export
 * @note framework internal constrants & types should define inside class
 */

// nobody really care about file format
typedef enum eFileFormat {
    kFileFormatInvalid  = 0,
    kFileFormatAny      = '****',       ///< it is a usefull file without details
    // audio
    kFileFormatWave     = 'WAVE',
    kFileFormatMp3      = 'mp3 ',
    kFileFormatFlac     = 'fLaC',
    kFileFormatApe      = 'APE ',
    // video
    kFileFormatMp4      = 'Mp4 ',       ///< mp4 & m4a
    kFileFormatMkv      = 'mkv ',
    kFileFormatAvi      = 'avi ',
    // images
    kFileFormatJpeg     = 'jpeg',
    kFileFormatGif      = 'gif ',
    kFileFormatPng      = 'png ',
} eFileFormat;

typedef enum eCodecFormat {
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
    kAudioCodecFormatFFmpeg = kAudioCodecFormatLast - 1,    // extend our capability using ffmpeg
    // video
    kVideoCodecFormatFirst  = 0x200,
    kVideoCodecFormatH264,
    kVideoCodecFormatHEVC,
    kVideoCodecFormatMPEG4,
    kVideoCodecFormatVC1,
    kVideoCodecFormatH263,
    kVideoCodecFormatLast   = 0x300 - 1,
    kVideoCodecFormatFFmpeg = kVideoCodecFormatLast - 1,    // extend our capability using ffmpeg
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

typedef enum eCodecType {
    kCodecTypeUnknown       = 0,
    kCodecTypeVideo         = 1,
    kCodecTypeAudio         = 2,
    kCodecTypeSubtitle      = 3,
    kCodecTypeImage         = 4,
    kCodecTypeMax,
} eCodecType;

API_EXPORT eCodecType GetCodecType(eCodecFormat format);

/**
 * about byte-order and word-order of pixels:
 * @note we use letters to represent byte-order instead of word-order
 * @note byte-order is commonly used in files & network & opengl
 * @note word-order is commonly used in os and libraries for rgb
 * @note libyuv using word-order pixels
 * @note ffmpeg using byte-order pixels
 * @note yuv pixels usually in byte-order
 * https://en.wikipedia.org/wiki/RGBA_color_space
 * https://en.wikipedia.org/wiki/YCbCr
 *
 */
typedef enum ePixelFormat {
    kPixelFormatUnknown     = 0,            ///< Unknown

    /** Y'CbCr color space section **/
    /** Y'CbCr 420 family **/
    kPixelFormat420YpCbCrPlanar = 0x100,    ///< Planar Y'CbCr 8-bit 4:2:0, 12bpp, 3 planes: Y'/Cb/Cr,
    kPixelFormat420YpCrCbPlanar,            ///< Planar Y'CbCr 8-bit 4:2:0, 12bpp, 3 planes: Y'/Cr/Cb, aka yv12
    kPixelFormat420YpCbCrSemiPlanar,        ///< Planar Y'CbCr 8-bit 4:2:0, 12bpp, 2 planes: Y'/Cb&Cr(interleaved), aka nv12
    kPixelFormat420YpCrCbSemiPlanar,        ///< Planar Y'CbCr 8-bit 4:2:0, 12bpp, 2 planes: Y'/Cr&Cb(interleaved), aka nv21
    
    /** Y'CbCr 422 family **/
    kPixelFormat422YpCbCrPlanar = 0x110,    ///< Planar Y'CbCr 8-bit 4:2:2, 16bpp, 3 planes: Y'/Cb/Cr
    kPixelFormat422YpCrCbPlanar,            ///< Planar Y'CbCr 8-bit 4:2:2, 16bpp, 3 planes: Y'/Cr/Cb
    kPixelFormat422YpCbCr,                  ///< Packed Y'CbCr 8-bit 4:2:2, 16bpp, Y'0 Cb Y'1 Cr
    kPixelFormat422YpCrCb,                  ///< Packed Y'CbCr 8-bit 4:2:2, 16bpp, Y'0 Cr Y'1 Cb
    
    /** Y'CbCr 444 family **/
    kPixelFormat444YpCbCrPlanar = 0x120,    ///< Planar Y'CbCr 8-bit 4:4:4, 24bpp, 3 planes: Y'/Cb/Cr
    kPixelFormat444YpCbCr,                  ///< Packed Y'CbCr 8-bit 4:4:4, 24bpp,
    
    /** Y'CbCr others **/

    /** Y'CbCr 10-bit family **/
    kPixelFormat420YpCbCr10Planar = 0x140,  ///< Planar Y'CbCr 10-bit 4:2:0, 15bpp, 3 planes: Y'/Cb/Cr
    
    /** RGB color space section **/
    kPixelFormatRGB565 = 0x200,             ///< packed RGB 5:6:5, 16 bpp,
    kPixelFormatBGR565,                     ///< packed BGR 5:6:5, 16 bpp, RGB565 in word-order
    kPixelFormatRGB,                        ///< packed RGB 8:8:8, 24 bpp, byte-order
    kPixelFormatBGR,                        ///< packed BGR 8:8:8, 24 bpp, RGB in word-order
    kPixelFormatARGB,                       ///< packed ARGB, 32 bpp, AARRGGBB, byte-order
    kPixelFormatBGRA,                       ///< packed BGRA, 32 bpp, BBGGRRAA, ARGB in word-order
    kPixelFormatRGBA,                       ///< packed RGBA, 32 bpp, RRGGBBAA, byte-order
    kPixelFormatABGR,                       ///< packed ABGR, 32 bpp, AABBGGRR, RGBA in word-order
    
    /** hardware pixel format section **/
    kPixelFormatVideoToolbox = 0x300,       ///< hardware frame from video toolbox
    
    /** alias section. TODO: set alias to platform preferred **/
    kPixelFormatRGB16   = kPixelFormatBGR565,
    KPixelFormatRGB24   = kPixelFormatBGR,
    kPixelFormatRGB32   = kPixelFormatBGRA, ///< ARGB in word-order, application usally using this
} ePixelFormat;

typedef enum eColorSpace {
    kColorUnknown,
    kColorYpCbCr,
    kColorRGB
} eColorSpace;

typedef struct PixelDescriptor {
    const char *        name;       ///< pixel format name
    ePixelFormat        format;     ///< pixel format value
    eColorSpace         color;      ///< color space
    size_t              bpp;        ///< avg pixel bpp => image size
    size_t              planes;     ///< number planes => image size
    struct {
        size_t          bpp;        ///< plane pixel bpp
        size_t          hss;        ///< horizontal subsampling
        size_t          vss;        ///< vertical subsampling
    } plane[4];
} PixelDescriptor;

// get information about pixel format
API_EXPORT const PixelDescriptor *  GetPixelFormatDescriptor(ePixelFormat);

/*
 * about full range & video range
 * @note Y'CbCr full range luma=[0, 255], chroma=[1, 255], JFIF
 * @note Y'CbCr video range luma=[16, 235], chroma=[16,240], ITU-R BT.601
 * @note don't put range infomation into pixel format
 */
typedef enum eYpCbCrRange {
    kYpCbCrFullRange    = 0,
    kYpCbCrVideoRange   = 1,
} eYpCbCrRange;

typedef enum eRotate {
    kRotate0,
    kRotate90,
    kRotate180,
    kRotate270
} eRotate;

typedef struct ImageRect {
    int32_t             x;
    int32_t             y;
    int32_t             w;
    int32_t             h;
} ImageRect;

typedef struct ImageFormat {
    ePixelFormat        format;         ///< image pixel format
    int32_t             width;          ///< plane width
    int32_t             height;         ///< plane height
    ImageRect           rect;           ///< display rectangle
} ImageFormat;

// FIXME: code sample infomation into format
/**
 * we always use planar data instead of interleaved,
 * which is very common in audio processing
 */
typedef enum eSampleFormat {
    kSampleFormatUnknown    = 0,
    kSampleFormatU8,
    kSampleFormatS16,
    kSampleFormatS32,
    kSampleFormatFLT,
    kSampleFormatDBL,
    kSampleFormatLast = 0x100 - 1   ///< make sure: audio sample format < pixel format
} eSampleFormat;

typedef struct SampleDescriptor {
    const char *    name;
    eSampleFormat   format;
    
} SampleDescriptor;

API_EXPORT const SampleDescriptor * GetSampleFormatDescriptor(eSampleFormat);

API_EXPORT const char * GetSampleFormatString(eSampleFormat);
API_EXPORT size_t       GetSampleFormatBytes(eSampleFormat);

/**
 * read behavior modes
 */
typedef enum eReadMode {
    kReadModeNormal,            ///< read next packet, -ts
    kReadModeNextSync,          ///< read next sync packet, +ts
    kReadModeLastSync,          ///< read last sync packet, +ts
    kReadModeClosestSync,       ///< read closest sync packet, +ts
    ///< @note special read mode
    kReadModeDefault = kReadModeNormal
} eReadMode;

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

typedef enum {
    kMediaNoError                   = 0,
    kMediaErrorUnknown              = -10000,
    kMediaErrorNotSupported         = -10001,
    kMediaErrorInvalidOperation     = -10002,
    kMediaErrorNotInitialied        = -10003,
    kMediaErrorBadValue             = -10004,
    kMediaErrorTryAgain             = -10005,
    kMediaErrorResourceBusy         = -10006,
    kMediaErrorBadFormat            = -10007,   ///<
    kMediaErrorNoMoreData           = -10008,   ///<
    kMediaErrorOutOfMemory          = -10009,   ///< Out Of Memory
} MediaError;

typedef struct AudioFormat {
    eSampleFormat       format;         ///< audio sample format @see eSampleFormat
    int32_t             channels;       ///< audio channels
    int32_t             freq;           ///< audio sample rate
    int32_t             samples;        ///< samples per channel
} AudioFormat;

#define kTimeValueBegin     (0)
#define kTimeValueInvalid   (-1)
#define kTimeValueEnd       (-2)

__END_DECLS

#ifdef __cplusplus
#define __BEGIN_NAMESPACE_MPX   __BEGIN_NAMESPACE(mpx)
#define __END_NAMESPACE_MPX     __END_NAMESPACE(mpx)
#define __USING_NAMESPACE_MPX   __USING_NAMESPACE(mpx)

USING_NAMESPACE_ABE

__BEGIN_NAMESPACE_MPX

// AudioFormat
static FORCE_INLINE bool operator==(const AudioFormat& lhs, const AudioFormat& rhs) {
    return lhs.format == rhs.format && lhs.channels == rhs.channels && lhs.freq == rhs.freq;
}

static FORCE_INLINE bool operator!=(const AudioFormat& lhs, const AudioFormat& rhs) { return !operator==(lhs, rhs); }

// ImageFormat
static FORCE_INLINE bool operator==(const ImageFormat& lhs, const ImageFormat& rhs) {
    return lhs.format == rhs.format && lhs.width == rhs.width && lhs.height == rhs.height;
}

static FORCE_INLINE bool operator!=(const ImageFormat& lhs, const ImageFormat& rhs) { return !operator==(lhs, rhs); }

/**
 * time struct for represent decoding and presentation time
 * @note we prefer int64_t(us) for our framework, but files and decoders prefer
 * time value & scale, so we using MediaTime for MediaPacket and MediaFrame, but int64_t
 * for reset of the framework.
 * @note MediaTime should only be used inside, no export, using int64_t(us) export
 * time related properties. so MediaTime will not derive from SharedObject
 */
struct MediaTime {
    int64_t     value;
    int64_t     timescale;

    FORCE_INLINE MediaTime() : value(kTimeValueBegin), timescale(1000000LL) { }
    FORCE_INLINE MediaTime(int64_t _value, int64_t _timescale) : value(_value), timescale(_timescale) { }
    FORCE_INLINE MediaTime(int64_t us) : value(us), timescale(1000000LL) { }

    FORCE_INLINE MediaTime& scale(int64_t _timescale) {
        if (timescale != _timescale) {
            value = (value * _timescale) / timescale;
            timescale = _timescale;
        }
        return *this;
    }
    
    // the least common multiple
    static int64_t LCM(int64_t a, int64_t b) {
        if (a % b == 0) return a;
        if (b % a == 0) return b;
        int64_t c, gcd;
        // FIXME: if a or b is very big, LCM will take a lot time.
        for (c = 1; c <= a && c <= b; ++c) {
            if (a % c == 0 && b % c == 0) gcd = c;
        }
        return (a * b) / gcd;
    }

    FORCE_INLINE double seconds() const {
        return (double)value / timescale;
    }

    FORCE_INLINE int64_t useconds() const {
        return (value * 1000000LL) / timescale;
    }

    FORCE_INLINE MediaTime operator+(const MediaTime& rhs) const {
        int64_t lcd = LCM(timescale, rhs.timescale);
        return MediaTime(value * (lcd / timescale) + rhs.value * (lcd / rhs.timescale), lcd);
    }

    FORCE_INLINE MediaTime operator-(const MediaTime& rhs) const {
        int64_t lcd = LCM(timescale, rhs.timescale);
        return MediaTime(value * (lcd / timescale) - rhs.value * (lcd / rhs.timescale), lcd);
    }

    FORCE_INLINE MediaTime& operator+=(const MediaTime& rhs) {
        int64_t lcd = LCM(timescale, rhs.timescale);
        value *= (lcd / timescale); value += rhs.value * (lcd / rhs.timescale);
        timescale = lcd;
        return *this;
    }

    FORCE_INLINE MediaTime& operator-=(const MediaTime& rhs) {
        int64_t lcd = LCM(timescale, rhs.timescale);
        value *= (lcd / timescale); value -= rhs.value * (lcd / rhs.timescale);
        timescale = lcd;
        return *this;
    }

#define COMPARE(op) FORCE_INLINE bool operator op(const MediaTime& rhs) const \
    { return (double)value/timescale op (double)rhs.value/rhs.timescale; }

    COMPARE(<);
    COMPARE(<=);
    COMPARE(==);
    COMPARE(!=);
    COMPARE(>);
    COMPARE(>=);

#undef COMPARE

};

API_EXPORT const MediaTime  kMediaTimeInvalid   ( kTimeValueInvalid, 1 );
API_EXPORT const MediaTime  kMediaTimeBegin     ( kTimeValueBegin, 1 );
API_EXPORT const MediaTime  kMediaTimeEnd       ( kTimeValueEnd, 1 );

/**
 * media packet class for compressed audio and video packets
 */
struct API_EXPORT MediaPacket : public SharedObject {
    uint8_t *           data;       ///< packet data
    size_t              size;       ///< data size in bytes

    size_t              index;      ///< track index, 0 based value
    eCodecFormat        format;     ///< packet format @see eCodecFormat
    uint32_t            flags;      ///< @see kFrameFlag*
    MediaTime           dts;        ///< packet decoding time, mandatory
    MediaTime           pts;        ///< packet presentation time, mandatory if decoding order != presentation order
    MediaTime           duration;   ///< packet duration time

    sp<Message>         properties; ///< extra properties of current frame
    void *              opaque;     ///< opaque

    MediaPacket() : data(NULL), size(0), index(0),
        format(kCodecFormatUnknown),
        flags(kFrameFlagNone),
        dts(kMediaTimeInvalid),
        pts(kMediaTimeInvalid),
        duration(kMediaTimeInvalid),
        opaque(NULL) { }
        
    virtual ~MediaPacket() { }
};

/**
 * create a packet backend by Buffer
 */
API_EXPORT sp<MediaPacket> MediaPacketCreate(size_t size);
API_EXPORT sp<MediaPacket> MediaPacketCreate(sp<Buffer>&);

__END_NAMESPACE_MPX

__BEGIN_NAMESPACE_MPX

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
            virtual MediaError parse(const Buffer& data) = 0;
            FORCE_INLINE const sp<Message>& values() const { return mValues; }

        protected:
            sp<Message>     mValues;
    };

    class API_EXPORT Writter {
        public:
            FORCE_INLINE Writter() { }
            FORCE_INLINE virtual ~Writter() { }
            virtual MediaError synth(const Message& data) = 0;
            //const Buffer&       values() const {  }

        protected:
            //Buffer              mBuffer;
    };
};

template <typename T>
class ABE_EXPORT MediaEvent : public Job {
    public:
        MediaEvent(const Object<Looper>& lp) : Job() { bind(lp); }
        virtual ~MediaEvent() { }
        
        virtual size_t fire(const T& value) {
            mValues.push(value);
            size_t id = Job::run();
            return id;
        }
        
    protected:
        virtual void onEvent(const T& value) = 0;
        
    private:
        LockFree::Queue<T> mValues;
        
        virtual void onJob() {
            T value; mValues.pop(value);
            onEvent(value);
        }
};

template <typename T, typename U>
class ABE_EXPORT MediaEvent2 : public Job {
    public:
        MediaEvent2(const Object<Looper>& lp) : Job() { bind(lp); }
        virtual ~MediaEvent2() { }
        
        virtual size_t fire(const T& a, const U& b) {
            Pair p; p.a = a; p.b = b;
            mValues.push(p);
            size_t id = Job::run();
            return id;
        }
        
    protected:
        virtual void onEvent(const T& a, const U& b) = 0;
        
    private:
        struct Pair { T a; U b; };
        LockFree::Queue<Pair> mValues;
        
        virtual void onJob() {
            Pair p; mValues.pop(p);
            onEvent(p.a, p.b);
        }
};

__END_NAMESPACE_MPX

#endif // __cplusplus

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
 * @see eReadMode
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
#define kKeyChannelMap  "channel-map"   ///< int32_t, optional for audio

/**
 * video display width and height
 */
#define kKeyWidth       "width"     ///< int32_t, mandatory for video
#define kKeyHeight      "height"    ///< int32_t, mandatory for video

/**
 * comman keys
 */
#define kKeyCount       "count"     ///< int32_t
#define kKeyResult      "result"    ///< int32_t, MediaError


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

/**
 * for MediaOut
 */
#define kKeyRotate              "rotate"    // eRotate

#endif // _MEDIA_MODULES_MEDIADEFS_H

