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


// File:    MediaTypes.h
// Author:  mtdcy.chen
// Changes:
//          1. 20160701     initial version
//

#ifndef _MPX_MEDIA_TYPES_H
#define _MPX_MEDIA_TYPES_H

#include <ABE/ABE.h>

#pragma mark Basic Macros
#ifndef FORCE_INLINE 
#define FORCE_INLINE    ABE_INLINE
#endif

#ifndef API_EXPORT
#define API_EXPORT      ABE_EXPORT
#endif

#ifndef API_DEPRECATED
#define API_DEPRECATED  ABE_DEPRECATED
#endif

#ifdef __cplusplus 
#define __BEGIN_NAMESPACE_MPX   __BEGIN_NAMESPACE(mpx)
#define __END_NAMESPACE_MPX     __END_NAMESPACE(mpx)
#define __USING_NAMESPACE_MPX   __USING_NAMESPACE(mpx)
USING_NAMESPACE_ABE
#endif

#pragma mark Basic Values
/**
 * constrants & types that should export
 * @note framework internal constrants & types should define inside class
 */
__BEGIN_DECLS

#undef FOURCC // FOURCC may be defined by others
#define FOURCC(x) ((((uint32_t)(x) >> 24) & 0xff)       \
                | (((uint32_t)(x) >> 8) & 0xff00)       \
                | (((uint32_t)(x) << 8) & 0xff0000)     \
                | (((uint32_t)(x) << 24) & 0xff000000))

#define MEDIA_ENUM_MAX              (0xFFFFFFFF)
enum {
    kMediaNoError                   = 0,
    kMediaErrorUnknown              = FOURCC('unkn'),
    kMediaErrorNotSupported         = FOURCC('?sup'),
    kMediaErrorInvalidOperation     = FOURCC('inva'),
    kMediaErrorBadContent           = FOURCC('badc'),
    kMediaErrorBadParameters        = FOURCC('badp'),
    kMediaErrorBadFormat            = FOURCC('badf'),
    kMediaErrorBadValue             = FOURCC('badv'),
    kMediaErrorTryAgain             = FOURCC('agan'),
    kMediaErrorResourceBusy         = FOURCC('busy'),
    kMediaErrorOutOfMemory          = FOURCC('!oom'),   ///< Out Of Memory
    kMediaErrorSystemError          = FOURCC('!sys'),   ///< system error except OOM
    
    kMediaErrorMax                  = MEDIA_ENUM_MAX
};
typedef uint32_t MediaError;

enum {
    kFormatUnknown      = FOURCC('----'),       ///< unknown format
};

// nobody really care about file format
enum {
    kFileFormatUnknown  = kFormatUnknown,
    kFileFormatAny      = FOURCC('****'),       ///< it is a usefull file without details
    // audio
    kFileFormatWave     = FOURCC('WAVE'),
    kFileFormatMp3      = FOURCC('Fmp3'),
    kFileFormatFlac     = FOURCC('FLaC'),
    kFileFormatApe      = FOURCC('FAPE'),
    // video
    kFileFormatMp4      = FOURCC('mp4 '),       ///< mp4 & m4a
    kFileFormatMkv      = FOURCC('mkv '),
    kFileFormatAvi      = FOURCC('avi '),
    // images
    kFileFormatJpeg     = FOURCC('Fjpg'),
    kFileFormatGif      = FOURCC('Fgif'),
    kFileFormatPng      = FOURCC('Fpng'),
    //
    kFileFormatLAVF     = FOURCC('lavf'),   // extend our capability by libavformat
    
    kFileFormatMax      = MEDIA_ENUM_MAX
};
typedef uint32_t eFileFormat;

// compressed audio codec format
enum {
    kAudioCodecUnknown  = kFormatUnknown,
    kAudioCodecFLAC     = FOURCC('fLaC'),
    kAudioCodecMP3      = FOURCC('mp3 '),
    kAudioCodecVorbis   = FOURCC('Vorb'),
    kAudioCodecAAC      = FOURCC('aac '),
    kAudioCodecAC3      = FOURCC('ac-3'),
    kAudioCodecWMA      = FOURCC('wma '),
    kAudioCodecAPE      = FOURCC('APE '),
    kAudioCodecDTS      = FOURCC('DTS '),
    kAudioCodecLAVC     = FOURCC('FFau'),   // extend our capability by libavcodec
    
    kAudioCodecMax      = MEDIA_ENUM_MAX
};
typedef uint32_t eAudioCodec;

// compressed video codec format
enum {
    kVideoCodecUnknown  = kFormatUnknown,
    kVideoCodecMPEG4    = FOURCC('mp4v'),   // MPEG-4, Part 2: Visual
    kVideoCodecH263     = FOURCC('s263'),   //
    kVideoCodecH264     = FOURCC('avc1'),   // MPEG-4, Part 10: Advanced Video Coding
    kVideoCodecHEVC     = FOURCC('HEVC'),   // MPEG-H, Part 2: High Efficiency Video Coding
    kVideoCodecVP8      = FOURCC('VP80'),   // free codec by On2
    kVideoCodecVP9      = FOURCC('VP90'),   // free codec by Google
    kVideoCodecVC1      = FOURCC('vc1 '),   // SMPTE 421M, Microsoft WMV9
    kVideoCodecLAVC     = FOURCC('FFvi'),   // extend our capability by libavcodec
    
    // Microsoft Version Codecs
    // support by ffmpeg, including different sub codecs, need MVCM to distinguish them
    // https://wiki.multimedia.cx/index.php/Microsoft_MPEG-4
    kVideoCodecMicrosoftMPEG4   = FOURCC('MPG4'),   // Microsoft MPEG4, @see kKeyMVCM
    
    kVideoCodecMax      = MEDIA_ENUM_MAX
};
typedef uint32_t eVideoCodec;

#if 0
enum {
    // TODO
};
typedef uint32_t eTextFormat;
#endif

// image codec format
enum {
    kImageCodecUnknown  = kFormatUnknown,
    kImageCodecPNG      = FOURCC('png '),
    kImageCodecJPEG     = FOURCC('jpeg'),
    kImageCodecBMP      = FOURCC('bmp '),
    kImageCodecGIF      = FOURCC('gif '),
    
    kImageCodecMax      = MEDIA_ENUM_MAX
};
typedef uint32_t eImageCodec;

// codec type
enum {
    kCodecTypeUnknown       = kFormatUnknown,
    kCodecTypeVideo         = FOURCC('vide'),
    kCodecTypeAudio         = FOURCC('audi'),
    kCodecTypeSubtitle      = FOURCC('subt'),
    kCodecTypeImage         = FOURCC('imag'),
    
    kCodecTypeMax           = MEDIA_ENUM_MAX
};
typedef uint32_t eCodecType;

/**
 * uncompressed audio format
 * we always use planar data instead of interleaved,
 * which is very common in audio processing, but not in HAL
 */
enum {
    // plannar sample formats
    // we prefer plannar samples inside framework
    kSampleFormatUnknown    = kFormatUnknown,
    kSampleFormatU8         = FOURCC(' u8p'),
    kSampleFormatS16        = FOURCC('s16p'),
    kSampleFormatS32        = FOURCC('s32p'),
    kSampleFormatFLT        = FOURCC('fltp'),
    kSampleFormatDBL        = FOURCC('dblp'),
    // packed/interleaved sample formats.
    // samples for each channel are interleaved
    // for audio input/output device only
    // most audio HAL only support interleaved samples
    kSampleFormatU8Packed   = FOURCC(' u8i'),
    kSampleFormatS16Packed  = FOURCC('s16i'),
    kSampleFormatS32Packed  = FOURCC('s32i'),
    kSampleFormatFLTPacked  = FOURCC('flti'),
    kSampleFormatDBLPacked  = FOURCC('dbli'),
    
    kSampleFormatMax        = MEDIA_ENUM_MAX
};
typedef uint32_t eSampleFormat;

/**
 * uncompressed pixel format
 * about byte-order and word-order of pixels:
 * @note we use letters to represent byte-order instead of word-order
 * @note byte-order is commonly used in files & network & opengl
 * @note word-order is commonly used in os and libraries for rgb
 * @note libyuv using word-order pixels
 * @note ffmpeg using byte-order pixels
 * @note yuv pixels usually in represent in byte-order
 * https://en.wikipedia.org/wiki/RGBA_color_space
 * https://en.wikipedia.org/wiki/YCbCr
 * https://www.fourcc.org/yuv.php
 */
enum {
    kPixelFormatUnknown                 = kFormatUnknown,

    /** Y'CbCr color space section **/
    /** Y'CbCr 420 family **/
    kPixelFormat420YpCbCrPlanar         = FOURCC('I420'),   ///< Planar Y'CbCr 8-bit 4:2:0, 12bpp, 3 planes: Y'/Cb/Cr,
    kPixelFormat420YpCrCbPlanar         = FOURCC('YV12'),   ///< Planar Y'CbCr 8-bit 4:2:0, 12bpp, 3 planes: Y'/Cr/Cb, aka yv12
    kPixelFormat420YpCbCrSemiPlanar     = FOURCC('NV12'),   ///< Planar Y'CbCr 8-bit 4:2:0, 12bpp, 2 planes: Y'/Cb&Cr(interleaved), aka nv12
    kPixelFormat420YpCrCbSemiPlanar     = FOURCC('NV21'),   ///< Planar Y'CbCr 8-bit 4:2:0, 12bpp, 2 planes: Y'/Cr&Cb(interleaved), aka nv21

    /** Y'CbCr 422 family **/
    kPixelFormat422YpCbCrPlanar         = FOURCC('I422'),   ///< Planar Y'CbCr 8-bit 4:2:2, 16bpp, 3 planes: Y'/Cb/Cr
    kPixelFormat422YpCrCbPlanar         = FOURCC('YV16'),   ///< Planar Y'CbCr 8-bit 4:2:2, 16bpp, 3 planes: Y'/Cr/Cb, aka yv16
    kPixelFormat422YpCbCr               = FOURCC('YUY2'),   ///< Packed Y'CbCr 8-bit 4:2:2, 16bpp, Y'0 Cb Y'1 Cr
    kPixelFormat422YpCrCb               = FOURCC('YVYU'),   ///< Packed Y'CbCr 8-bit 4:2:2, 16bpp, Y'0 Cr Y'1 Cb
    kPixelFormat422YpCbCrWO             = FOURCC('VYUY'),   ///< Packed Y'CbCr 8-bit 4:2:2, 16bpp, Cr Y'0 Cb Y'1 (Y'0 Cb Y'1 Cr in word-order)
    kPixelFormat422YpCrCbWO             = FOURCC('UYVY'),   ///< Packed Y'CbCr 8-bit 4:2:2, 16bpp, Cb Y'0 Cr Y'1 (Y'0 Cr Y'1 Cb in word-order)

    /** Y'CbCr 444 family, few codecs or platform support these formats **/
    kPixelFormat444YpCbCrPlanar         = FOURCC('I444'),   ///< Planar Y'CbCr 8-bit 4:4:4, 24bpp, 3 planes: Y'/Cb/Cr
    kPixelFormat444YpCrCbPlanar         = FOURCC('YV24'),   ///< Planar Y'CbCr 8-bit 4:4:4, 24bpp, 3 planes: Y'/Cr/Cb
    kPixelFormat444YpCbCr               = FOURCC('P444'),   ///< Packed Y'CbCr 8-bit 4:4:4, 24bpp, Y'CbCr(interleaved)

    /** Y'CbCr others **/

    /** Y'CbCr 10-bit family **/
    kPixelFormat420YpCbCr10Planar       = FOURCC('v210'),  ///< Planar Y'CbCr 10-bit 4:2:0, 15bpp, 3 planes: Y'/Cb/Cr

    /** RGB color space section **/
    kPixelFormatRGB565                  = FOURCC('16RG'),   ///< packed RGB 5:6:5, 16 bpp, RGB565 in byte-order
    kPixelFormatBGR565                  = FOURCC('16BG'),   ///< packed BGR 5:6:5, 16 bpp, RGB565 in word-order
    kPixelFormatRGB                     = FOURCC('24RG'),   ///< packed RGB 8:8:8, 24 bpp, RGB byte-order
    kPixelFormatBGR                     = FOURCC('24BG'),   ///< packed BGR 8:8:8, 24 bpp, RGB in word-order
    kPixelFormatARGB                    = FOURCC('ARGB'),   ///< packed ARGB, 32 bpp, AARRGGBB, ARGB in byte-order
    kPixelFormatBGRA                    = FOURCC('BGRA'),   ///< packed BGRA, 32 bpp, BBGGRRAA, ARGB in word-order
    kPixelFormatRGBA                    = FOURCC('RGBA'),   ///< packed RGBA, 32 bpp, RRGGBBAA, RGBA in byte-order
    kPixelFormatABGR                    = FOURCC('ABGR'),   ///< packed ABGR, 32 bpp, AABBGGRR, RGBA in word-order

    /** alias section. set alias to platform preferred **/
    kPixelFormatRGB16                   = kPixelFormatBGR565,   ///< RGB565 in word-order
    kPixelFormatRGB24                   = kPixelFormatBGR,      ///< RGB in word-order
    kPixelFormatRGB32                   = kPixelFormatBGRA,     ///< ARGB in word-order, application usally using this

    /** hardware pixel format section **/
    kPixelFormatVideoToolbox            = FOURCC('vtbx'),   ///< hardware frame from video toolbox
    
    /** custom pixel format, need custom PixelDescriptor */
    kPixelFormatCustom                  = FOURCC('?pix'),
    
    kPixelFormatMax                     = MEDIA_ENUM_MAX
};
typedef uint32_t ePixelFormat;

enum {
    kColorUnknown       = kFormatUnknown,
    kColorYpCbCr        = FOURCC('Cyuv'),
    kColorRGB           = FOURCC('Crgb'),
    
    kColorMax           = MEDIA_ENUM_MAX
};
typedef uint32_t eColorSpace;
/*
 * about full range & video range
 * @note Y'CbCr full range luma=[0, 255], chroma=[1, 255], JFIF
 * @note Y'CbCr video range luma=[16, 235], chroma=[16,240], ITU-R BT.601
 * @note don't put range infomation into pixel format
 */
enum {
    kYpCbCrFullRange    = FOURCC('CRfu'),
    kYpCbCrVideoRange   = FOURCC('CRvi'),
    
    kYpCbCrRangeMax     = MEDIA_ENUM_MAX
};
typedef uint32_t eYpCbCrRange;

enum {
    kRotate0            = FOURCC('R000'),
    kRotate90           = FOURCC('R090'),
    kRotate180          = FOURCC('R180'),
    kRotate270          = FOURCC('R270'),
    
    kRotateMax          = MEDIA_ENUM_MAX
};
typedef uint32_t eRotate;

/**
 * kFrameTypeSync: sync frame, depends on nobody.
 *      @note seek can only seek at this kind of frame.
 * kFrameTypeDisposal: none sync frame and not be depened by others. e.g. B-frame
 *      @note this kind of frame can be discard, decoder make the choice
 *      @note some B-frame be depended by others, in this case, no type should set.
 * kFrameTypeReference: frames that should be decoded but no output.
 *      @note can combine with kFrameTypeSync or kFrameTypeDepended
 */
enum {
    kFrameTypeUnknown       = 0,
    kFrameTypeSync          = (1<<0),
    kFrameTypeDisposal      = (1<<2),
    kFrameTypeReference     = (1<<3),
    
    kFrameTypeMax           = MEDIA_ENUM_MAX
};
typedef uint32_t    eFrameType;

#pragma mark Basic Types

enum { kPlanar = 0, kPacked = 0, kUVSwap, kWordOrder, kSimilarMax };
typedef struct PixelDescriptor {
    const char * const  name;                   ///< pixel format name
    const ePixelFormat  format;                 ///< pixel format value
    const ePixelFormat  similar[kSimilarMax];   ///< similar format
    const eColorSpace   color;                  ///< color space
    const size_t        bpp;                    ///< avg pixel bpp => image size
    const size_t        nb_planes;              ///< number planes => image size
    struct {
        const size_t    bpp;                    ///< plane pixel bpp
        const size_t    hss;                    ///< horizontal subsampling
        const size_t    vss;                    ///< vertical subsampling
    } const planes[4];
} PixelDescriptor;

// get information about pixel format
API_EXPORT const PixelDescriptor *  GetPixelFormatDescriptor(ePixelFormat);
API_EXPORT const PixelDescriptor *  GetPixelFormatDescriptorByName(const char *);
API_EXPORT bool                     IsPlanarPixelFormat(ePixelFormat);
API_EXPORT bool                     IsSemiPlanarPixelFormat(ePixelFormat);

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

typedef struct SampleDescriptor {
    const char * const  name;           ///< sample format name
    const eSampleFormat format;         ///< sample format
    const eSampleFormat similar;        ///< corresponding planar/interleaved format
    const bool          planar;         ///< is planar
    const size_t        bytes;          ///< sample bytes
} SampleDescriptor;

API_EXPORT const SampleDescriptor * GetSampleFormatDescriptor(eSampleFormat);
API_EXPORT eSampleFormat            GetSimilarSampleFormat(eSampleFormat);      ///< plannar <-> packed/interleaved
API_EXPORT size_t                   GetSampleFormatBytes(eSampleFormat);
API_EXPORT bool                     IsPlanarSampleFormat(eSampleFormat);

typedef struct AudioFormat {
    eSampleFormat       format;         ///< audio sample format @see eSampleFormat
    int32_t             freq;           ///< audio sample rate
    size_t              channels;       ///< audio channels
    size_t              samples;        ///< samples per channel
} AudioFormat;

/**
 * time struct for represent decoding and presentation time
 * @note we prefer int64_t(us) for our framework, but files and decoders prefer
 * time value & scale, so we using MediaTime for MediaFrame, but int64_t
 * for rest of the framework.
 * @note MediaTime should only be used inside, no export, using int64_t(us) export
 * time related properties. so MediaTime will not derive from SharedObject
 */
typedef struct MediaTime {
    int64_t     value;                  ///< numerator
    int64_t     scale;                  ///< denominator
#ifdef __cplusplus
    MediaTime() : value(0), scale(0) { }    ///< invalid media time
    MediaTime(int64_t us) : value(us), scale(1000000LL) { }
    MediaTime(int64_t num, int64_t den) : value(num), scale(den) { }
    double seconds() const          { return (double)value / scale;                 }
    int64_t useconds() const        { return (1000000LL * value) / scale;           }
    MediaTime& rescale(int64_t den) { value = (value * den) / scale; return *this;  }
#endif
} MediaTime;

__END_DECLS

#pragma mark C++ Accesories
#ifdef __cplusplus
__BEGIN_NAMESPACE_MPX

// AudioFormat
API_EXPORT String   GetAudioFormatString(const AudioFormat&);

// ePixelFormat
API_EXPORT String   GetPixelFormatString(const ePixelFormat&);
API_EXPORT String   GetImageFormatString(const ImageFormat&);

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

// MediaTime
// valid MediaTime always has scale != 0, so we make scale == 0
API_EXPORT static const MediaTime kMediaTimeInvalid = MediaTime(-1, 0);

#define _MT_COMPARE(op) FORCE_INLINE bool operator op(const MediaTime& lhs, const MediaTime& rhs) \
{ return lhs.value * rhs.scale op rhs.value * lhs.scale; }

_MT_COMPARE(<);
_MT_COMPARE(<=);
_MT_COMPARE(>);
_MT_COMPARE(>=);
_MT_COMPARE(==);
_MT_COMPARE(!=);

#undef _MT_COMPARE

// the least common multiple
static FORCE_INLINE int64_t LCM(int64_t a, int64_t b) {
    if (a % b == 0) return a;
    if (b % a == 0) return b;
    int64_t c, gcd;
    // FIXME: if a or b is very big, LCM will take a lot time.
    for (c = 1; c <= a && c <= b; ++c) {
        if (a % c == 0 && b % c == 0) gcd = c;
    }
    return (a * b) / gcd;
}

FORCE_INLINE MediaTime operator+(const MediaTime& lhs, const MediaTime& rhs) {
    const int64_t lcd = LCM(lhs.scale, rhs.scale);
    return MediaTime(lhs.value * (lcd / lhs.scale) + rhs.value * (lcd / rhs.scale), lcd);
}

FORCE_INLINE MediaTime operator-(const MediaTime& lhs, const MediaTime& rhs) {
    const int64_t lcd = LCM(lhs.scale, rhs.scale);
    return MediaTime(lhs.value * (lcd / lhs.scale) - rhs.value * (lcd / rhs.scale), lcd);
}

FORCE_INLINE MediaTime& operator+=(MediaTime& lhs, const MediaTime& rhs) {
    const int64_t lcd = LCM(lhs.scale, rhs.scale);
    lhs.value = lhs.value * (lcd / lhs.scale) + rhs.value * (lcd / rhs.scale);
    lhs.scale = lcd;
    return lhs;
}

FORCE_INLINE MediaTime& operator-=(MediaTime& lhs, const MediaTime& rhs) {
    const int64_t lcd = LCM(lhs.scale, rhs.scale);
    lhs.value = lhs.value * (lcd / lhs.scale) - rhs.value * (lcd / rhs.scale);
    lhs.scale = lcd;
    return lhs;
}

#pragma mark MediaEvent/MediaEvent2
template <typename T>
class ABE_EXPORT MediaEvent : public Job {
    public:
        MediaEvent(const sp<Looper>& lp) : Job(lp) { }
        MediaEvent(const sp<DispatchQueue>& disp) : Job(disp) { }
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
        MediaEvent2(const sp<Looper>& lp) : Job(lp) { }
        MediaEvent2(const sp<DispatchQueue>& disp) : Job(disp) { }
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

#endif // _MPX_MEDIA_TYPES_H

