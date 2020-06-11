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

enum {
    kMediaNoError                   = 0,
    kMediaErrorUnknown              = 0xA000,
    kMediaErrorNotSupported         = 0xA001,
    kMediaErrorInvalidOperation     = 0xA002,
    kMediaErrorBadContent           = 0xA003,
    kMediaErrorBadFormat            = 0xA004,
    kMediaErrorBadValue             = 0xA005,
    kMediaErrorTryAgain             = 0xA006,
    kMediaErrorOutOfMemory          = 0xA007,   ///< Out Of Memory
    kMediaErrorSystemError          = 0xA008,   ///< system error except OOM
    kMediaErrorResourceBusy         = 0xA009,
};
typedef uint32_t MediaError;

// nobody really care about file format
enum {
    kFileFormatUnknown,
    kFileFormatAny      = FOURCC('****'),       ///< it is a usefull file without details
    // audio
    kFileFormatWave     = FOURCC('WAVE'),
    kFileFormatMp3      = FOURCC('mp3 '),
    kFileFormatFlac     = FOURCC('fLaC'),
    kFileFormatApe      = FOURCC('APE '),
    // video
    kFileFormatMp4      = FOURCC('mp4 '),       ///< mp4 & m4a
    kFileFormatMkv      = FOURCC('mkv '),
    kFileFormatAvi      = FOURCC('avi '),
    // images
    kFileFormatJpeg     = FOURCC('jpeg'),
    kFileFormatGif      = FOURCC('gif '),
    kFileFormatPng      = FOURCC('png '),
};
typedef uint32_t eFileFormat;

enum {
    kAudioCodecUnknown,
    kAudioCodecPCM      = FOURCC('PCM '),
    kAudioCodecFLAC     = FOURCC('fLaC'),
    kAudioCodecMP3      = FOURCC('mp3 '),
    kAudioCodecVorbis   = FOURCC('Vorb'),
    kAudioCodecAAC      = FOURCC('aac '),
    kAudioCodecAC3      = FOURCC('ac-3'),
    kAudioCodecWMA      = FOURCC('wma '),
    kAudioCodecAPE      = FOURCC('APE '),
    kAudioCodecDTS      = FOURCC('DTS '),
    kAudioCodecFFmpeg   = FOURCC('lavc'),   // extend our capability by ffmpeg
};
typedef uint32_t eAudioCodec;

enum {
    kVideoCodecUnknown,
    kVideoCodecH264     = FOURCC('avc1'),
    kVideoCodecHEVC     = FOURCC('HEVC'),
    kVideoCodecMPEG4    = FOURCC('mp4v'),
    kVideoCodecVP8      = FOURCC('VP80'),
    kVideoCodecVP9      = FOURCC('VP90'),
    kVideoCodecVC1      = FOURCC('vc1 '),
    kVideoCodecH263     = FOURCC('s263'),
    kVideoCodecMP42     = FOURCC('MP42'),   // Microsoft version MPEG4 v2
    kVideoCodecFFmpeg   = FOURCC('lavc'),   // extend our capability by ffmpeg
};
typedef uint32_t eVideoCodec;

#if 0
enum {
    // TODO
};
typedef uint32_t eTextFormat;
#endif

enum {
    kImageCodecUnknown,
    kImageCodecPNG      = FOURCC('png '),
    kImageCodecJPEG     = FOURCC('jpeg'),
    kImageCodecBMP      = FOURCC('bmp '),
    kImageCodecGIF      = FOURCC('gif '),
};
typedef uint32_t eImageCodec;

enum {
    kCodecTypeUnknown,
    kCodecTypeVideo         = FOURCC('vide'),
    kCodecTypeAudio         = FOURCC('audi'),
    kCodecTypeSubtitle      = FOURCC('subt'),
    kCodecTypeImage         = FOURCC('imag'),
};
typedef uint32_t eCodecType;

/**
 * we always use planar data instead of interleaved,
 * which is very common in audio processing, but not in HAL
 */
enum {
    kSampleFormatUnknown,
    kSampleFormatU8         = FOURCC('u8  '),
    kSampleFormatS16        = FOURCC('s16 '),
    kSampleFormatS32        = FOURCC('s32 '),
    kSampleFormatFLT        = FOURCC('flt '),
    kSampleFormatDBL        = FOURCC('dbl '),
    // packed sample formats.
    // samples for each channel are interleaved
    // for audio input/output device only
    // most audio HAL only support interleaved samples
    kSampleFormatU8Packed   = FOURCC('u8p '),
    kSampleFormatS16Packed  = FOURCC('s16p'),
    kSampleFormatS32Packed  = FOURCC('s32p'),
    kSampleFormatFLTPacked  = FOURCC('fltp'),
    kSampleFormatDBLPacked  = FOURCC('dblp'),
};
typedef uint32_t eSampleFormat;

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
enum {
    kPixelFormatUnknown,                ///< Unknown

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

    /** Y'CbCr 444 family **/
    kPixelFormat444YpCbCrPlanar         = FOURCC('I444'),   ///< Planar Y'CbCr 8-bit 4:4:4, 24bpp, 3 planes: Y'/Cb/Cr
    kPixelFormat444YpCbCr               = FOURCC('P444'),   ///< Packed Y'CbCr 8-bit 4:4:4, 24bpp, Y'CbCr(interleaved)

    /** Y'CbCr others **/

    /** Y'CbCr 10-bit family **/
    kPixelFormat420YpCbCr10Planar       = FOURCC('v210'),  ///< Planar Y'CbCr 10-bit 4:2:0, 15bpp, 3 planes: Y'/Cb/Cr

    /** RGB color space section **/
    kPixelFormatRGB565                  = FOURCC('RGB '),   ///< packed RGB 5:6:5, 16 bpp,
    kPixelFormatBGR565                  = FOURCC('BGR '),   ///< packed BGR 5:6:5, 16 bpp, RGB565 in word-order
    kPixelFormatRGB                     = FOURCC('24RG'),   ///< packed RGB 8:8:8, 24 bpp, byte-order
    kPixelFormatBGR                     = FOURCC('24BG'),   ///< packed BGR 8:8:8, 24 bpp, RGB in word-order
    kPixelFormatARGB                    = FOURCC('ARGB'),   ///< packed ARGB, 32 bpp, AARRGGBB, byte-order
    kPixelFormatBGRA                    = FOURCC('BGRA'),   ///< packed BGRA, 32 bpp, BBGGRRAA, ARGB in word-order
    kPixelFormatRGBA                    = FOURCC('RGBA'),   ///< packed RGBA, 32 bpp, RRGGBBAA, byte-order
    kPixelFormatABGR                    = FOURCC('ABGR'),   ///< packed ABGR, 32 bpp, AABBGGRR, RGBA in word-order

    /** hardware pixel format section **/
    kPixelFormatVideoToolbox            = FOURCC('vt  '),   ///< hardware frame from video toolbox

    /** alias section. TODO: set alias to platform preferred **/
    kPixelFormatRGB16                   = kPixelFormatBGR565,
    KPixelFormatRGB24                   = kPixelFormatBGR,
    kPixelFormatRGB32                   = kPixelFormatBGRA, ///< ARGB in word-order, application usally using this
};
typedef uint32_t ePixelFormat;

enum {
    kColorUnknown,
    kColorYpCbCr        = FOURCC('Cyuv'),
    kColorRGB           = FOURCC('Crgb')
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
};
typedef uint32_t eYpCbCrRange;

enum {
    kRotate0            = FOURCC('R000'),
    kRotate90           = FOURCC('R090'),
    kRotate180          = FOURCC('R180'),
    kRotate270          = FOURCC('R270')
};
typedef uint32_t eRotate;

/**
 * read behavior modes
 */
enum {
    kReadModeNormal,            ///< read next packet, -ts
    kReadModeNextSync,          ///< read next sync packet, +ts
    kReadModeLastSync,          ///< read last sync packet, +ts
    kReadModeClosestSync,       ///< read closest sync packet, +ts
    ///< @note special read mode
    kReadModeDefault = kReadModeNormal
};
typedef uint32_t eReadMode;

/**
 * kFrameTypeSync: sync frame, depends on nobody.
 *      @note seek can only seek at this kind of frame.
 * kFrameTypeDepended: none sync frame and be depended by others. e.g. P-frame
 *      @note when combine with kFrameTypeSync, this one will be ignored.
 * kFrameTypeDisposal: none sync frame and not be depened by others. e.g. B-frame
 *      @note this kind of frame can be discard, decoder make the choice
 *      @note some B-frame be depended by others, in this case, no type should set.
 * kFrameTypeReference: frames that should be decoded but no output.
 *      @note can combine with kFrameTypeSync or kFrameTypeDepended
 */
enum {
    kFrameTypeUnknown       = 0,
    kFrameTypeSync          = (1<<0),
    kFrameTypeDepended      = (1<<1),
    kFrameTypeDisposal      = (1<<2),
    kFrameTypeReference     = (1<<3),
};
typedef uint32_t    eFrameType;

enum {
    // common keys
    kKeyFormat          = FOURCC(' fmt'),       ///< int32_t, @see eFileFormat/eAudioCodec/eVideoCodec/eSampleFormat/ePixelFormat
    kKeyRequestFormat   = FOURCC('!fmt'),       ///< int32_t
    kKeyType            = FOURCC('type'),       ///< int32_t, @see eCodecType
    kKeyMode            = FOURCC('mode'),       ///< int32_t, @see eReadMode
    kKeyTime            = FOURCC('time'),       ///< int64_t, us
    kKeyDuration        = FOURCC('dura'),       ///< int64_t, us
    kKeyLatency         = FOURCC('late'),       ///< int64_t, us
    kKeyChannels        = FOURCC('chan'),       ///< int32_t
    kKeySampleRate      = FOURCC('srat'),       ///< int32_t
    kKeySampleBits      = FOURCC('#bit'),       ///< int32_t
    kKeyChannelMap      = FOURCC('cmap'),       ///< int32_t
    kKeyWidth           = FOURCC('widt'),       ///< int32_t
    kKeyHeight          = FOURCC('heig'),       ///< int32_t
    kKeyRotate          = FOURCC('?rot'),       ///< int32_t, eRotate
    kKeyCount           = FOURCC('#cnt'),       ///< int32_t
    kKeyError           = FOURCC('!err'),       ///< int32_t, MediaError
    kKeyBitrate         = FOURCC('btrt'),       ///< int32_t
    kKeyTracks          = FOURCC('trak'),       ///< int32_t, bit mask
    kKeyURL             = FOURCC(' url'),       ///< String
    kKeyLooper          = FOURCC('lper'),       ///< sp<Looper>
    kKeyTrack           = FOURCC('0trk'),       ///< int32_t
    kKeyOpenGLContext   = FOURCC('oglt'),       ///< void *
    kKeyPause           = FOURCC('paus'),       ///< int32_t, bool
    kKeyESDS            = FOURCC('esds'),       ///< sp<Buffer>
    kKeyavcC            = FOURCC('avcC'),       ///< sp<Buffer>
    kKeyhvcC            = FOURCC('hvcC'),       ///< sp<Buffer>
    kKeyMVCM            = FOURCC('MVCM'),       ///< sp<Buffer>, Microsoft VCM
    kKeyCodecSpecData   = FOURCC('#csd'),       ///< sp<Buffer>
    kKeyTags            = FOURCC('tag0'),       ///< sp<Message>
    kKeyEncoderDelay    = FOURCC('edly'),       ///< int32_t
    kKeyEncoderPadding  = FOURCC('epad'),       ///< int32_t
};

enum {
    // meta data keys, default value type string
    kKeyTitle           = FOURCC('0tit'),   ///< titles
    kKeyAlbum           = FOURCC(' alb'),
    kKeyComposer        = FOURCC(' wrt'),
    kKeyGenre           = FOURCC(' gen'),
    kKeyEncoder         = FOURCC(' enc'),
    kKeyArtist          = FOURCC(' art'),
    kKeyAlbumArtist     = FOURCC('aart'),   // what's this
    kKeyPerformer       = FOURCC('perf'),
    kKeyAuthor          = FOURCC('auth'),
    kKeyDate            = FOURCC('date'),
    kKeyYear            = FOURCC('year'),
    kKeyCompilation     = FOURCC('cpil'),
    kKeyComment         = FOURCC('comm'),
    kKeyTrackNum        = FOURCC('trkn'),
    kKeyDiskNum         = FOURCC('disk'),
    kKeyLanguage        = FOURCC('lang'),
    kKeyLocation        = FOURCC('loci'),
    kKeyCopyright       = FOURCC('cprt'),
    kKeyLicense         = FOURCC('lics'),
    kKeyBPM             = FOURCC(' BPM'),
    kKeyCustom          = FOURCC('0000'),   ///< user defined texts
    kKeyAlbumArt        = FOURCC('alma'),   ///< sp<Buffer>

    // special meta data
    kKeyiTunSMPB        = FOURCC('smpb'),   // String, m4a only
};

#pragma mark Basic Types
typedef struct PixelDescriptor {
    const char * const  name;       ///< pixel format name
    const ePixelFormat  format;     ///< pixel format value
    const eColorSpace   color;      ///< color space
    const size_t        bpp;        ///< avg pixel bpp => image size
    const size_t        planes;     ///< number planes => image size
    struct {
        const size_t    bpp;        ///< plane pixel bpp
        const size_t    hss;        ///< horizontal subsampling
        const size_t    vss;        ///< vertical subsampling
    } const plane[4];
} PixelDescriptor;

// get information about pixel format
API_EXPORT const PixelDescriptor *  GetPixelFormatDescriptor(ePixelFormat);

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
    const char * const  name;
    const eSampleFormat format;
    const size_t        planes;         ///<
    // TODO
} SampleDescriptor;

// TODO
API_EXPORT const SampleDescriptor * GetSampleFormatDescriptor(eSampleFormat);

API_EXPORT size_t       GetSampleFormatBytes(eSampleFormat);
API_EXPORT bool         IsSampleFormatPacked(eSampleFormat);

typedef struct AudioFormat {
    eSampleFormat       format;         ///< audio sample format @see eSampleFormat
    int32_t             freq;           ///< audio sample rate
    size_t              channels;       ///< audio channels
    size_t              samples;        ///< samples per channel
} AudioFormat;

__END_DECLS

#pragma mark Basic C++ Types
#ifdef __cplusplus
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
 * for rest of the framework.
 * @note MediaTime should only be used inside, no export, using int64_t(us) export
 * time related properties. so MediaTime will not derive from SharedObject
 */

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

#define COMPARE(op) FORCE_INLINE bool operator op(const MediaTime& rhs) const \
{ return (double)value/timescale op (double)rhs.value/rhs.timescale; }
typedef struct MediaTime {
    int64_t     value;
    int64_t     timescale;

    FORCE_INLINE MediaTime() : value(0), timescale(1000000LL) { }
    FORCE_INLINE MediaTime(int64_t num, int64_t den) : value(num), timescale(den) { }
    FORCE_INLINE MediaTime(int64_t us) : value(us), timescale(1000000LL) { }

    FORCE_INLINE MediaTime& scale(int64_t den) {
        if (timescale != den) {
            value = (value * den) / timescale;
            timescale = den;
        }
        return *this;
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

    COMPARE(<);
    COMPARE(<=);
    COMPARE(==);
    COMPARE(!=);
    COMPARE(>);
    COMPARE(>=);
} MediaTime;
#undef COMPARE

API_EXPORT const MediaTime kMediaTimeInvalid( -1, 1 );

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

/**
 * media packet class for compressed audio and video packets
 */
struct API_EXPORT MediaPacket : public SharedObject {
    uint8_t * const     data;       ///< packet data
    const size_t        capacity;   ///< buffer capacity in bytes
    size_t              size;       ///< data size in bytes

    size_t              index;      ///< track index, 0 based value
    eFrameType          type;       ///< @see eFrameType
    MediaTime           dts;        ///< packet decoding time, mandatory
    MediaTime           pts;        ///< packet presentation time, mandatory if decoding order != presentation order
    MediaTime           duration;   ///< packet duration time

    void *              opaque;     ///< opaque

    MediaPacket(uint8_t * const p, size_t length) :
        data(p), capacity(length), size(0), index(0),
        dts(kMediaTimeInvalid), pts(kMediaTimeInvalid),
        duration(kMediaTimeInvalid), opaque(NULL) { }

    virtual ~MediaPacket() { }
    
    /**
    * create a packet backend by Buffer
    */
    static sp<MediaPacket> Create(size_t size);
    static sp<MediaPacket> Create(sp<Buffer>&);
};

/**
 * media frame structure for decompressed audio and video frames
 * the properties inside this structure have to make sure this
 * frame can be renderred properly without additional informations.
 */
#define MEDIA_FRAME_NB_PLANES   (8)
struct API_EXPORT MediaFrame : public SharedObject {
    MediaTime               timecode;   ///< frame display timestamp
    MediaTime               duration;   ///< duration of this frame
    /**
     * plane data struct.
     * for planar frame, multi planes must exist. the backend memory may be
     * or may not be continueslly.
     * for packed frame, only one plane exists.
     */
    struct {
        uint8_t *           data;       ///< plane data
        size_t              size;       ///< data size in bytes
    } planes[MEDIA_FRAME_NB_PLANES];    ///< for packed frame, only one plane exists

    union {
        uint32_t            format;     ///< sample format, @see ePixelFormat, @see eSampleFormat
        AudioFormat         a;
        ImageFormat         v;
    };
    void                    *opaque;    ///< opaque

    /**
     * create a media frame backend by Buffer
     * the underlying buffer is always continues, a single buffer for all planes
     */
    static sp<MediaFrame>   Create(const ImageFormat&);
    static sp<MediaFrame>   Create(const ImageFormat&, const sp<Buffer>&);
    static sp<MediaFrame>   Create(const AudioFormat&);

    /** features below is not designed for realtime playback **/

    /**
     * read backend buffer of hwaccel frame
     * @return should return NULL if plane is not exists
     * @note default implementation: read directly from planes
     */
    virtual sp<Buffer> readPlane(size_t) const;

    /**
     * keep luma component and swap two chroma components of Y'CbCr image
     * @return return kMediaErrorInvalidOperation if source is not Y'CbCr
     * @return return kMediaErrorNotSupported if no implementation
     */
    virtual MediaError swapCbCr();

    /**
     * convert pixel bytes-order <-> word-order, like rgba -> abgr
     * @return return kMediaErrorInvalidOperation if source is planar
     * @return return kMediaErrorNotSupported if no implementation
     */
    virtual MediaError reversePixel();

    /**
     * convert to planar pixel format
     * @return return kMediaNoError on success or source is planar
     * @return return kMediaErrorNotSupported if no implementation
     * @note planarization may or may NOT be in place convert
     * @note target pixel format is variant based on the implementation
     */
    virtual MediaError planarization();

    /**
     * convert yuv -> rgb
     * @return return kMediaErrorInvalidOperation if source is rgb or target is not rgb
     * @return return kMediaErrorNotSupported if no implementation
     * @return target pixel is rgba by default, but no guarentee.
     */
    enum eConversion { kBT601, kBT709, kJFIF };
    virtual MediaError yuv2rgb(const ePixelFormat& = kPixelFormatRGB32, const eConversion& = kBT601);

    /**
     * rotate image
     * @return kMediaErrorNotSupported if no implementation
     */
    enum eRotation { kRotate0, kRotate90, kRotate180, kRotate270 };
    virtual MediaError rotate(const eRotation&) { return kMediaErrorNotSupported; }

    protected:
    MediaFrame();
    virtual ~MediaFrame() { }
    sp<Buffer>  mBuffer;
};

// ePixelFormat
API_EXPORT String   GetPixelFormatString(const ePixelFormat&);
API_EXPORT String   GetImageFormatString(const ImageFormat&);
API_EXPORT String   GetImageFrameString(const sp<MediaFrame>&);
API_EXPORT size_t   GetImageFormatPlaneLength(const ImageFormat&, size_t);
API_EXPORT size_t   GetImageFormatBufferLength(const ImageFormat& image);

// AudioFormat
API_EXPORT String   GetAudioFormatString(const AudioFormat&);

// get MediaFrame human readable string, for debug
API_EXPORT String   GetAudioFrameString(const sp<MediaFrame>&);

__END_NAMESPACE_MPX
#endif // __cplusplus

#endif // _MPX_MEDIA_TYPES_H

