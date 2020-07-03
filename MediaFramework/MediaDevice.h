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


// File:    MediaDevice.h
// Author:  mtdcy.chen
// Changes:
//          1. 20200630     initial version
//

#ifndef _MEDIA_MODULES_DEVICE_H
#define _MEDIA_MODULES_DEVICE_H

#include <MediaFramework/MediaTypes.h>
#include <MediaFramework/MediaFrame.h>

/**
 * Formats
 *  audio track formats:
 *   kKeyFormat:        eAudioCodec     [*] audio codec
 *   kKeyType:          eCodecType      [ ] codec type, default:kCodecTypeAudio
 *   kKeyChannels:      uint32_t        [*] audio channels
 *   kKeySampleRate:    uint32_t        [*] audio sample rate
 *   kKeyChannelMap:    uint32_t        [ ] audio channel map
 *   kKeyESDS:          sp<Buffer>      [ ] audio magic data
 *
 *  audio sample formats:
 *   kKeyFormat:        eSampleFormat   [*] sample format
 *   kKeyType:          eCodecType      [ ] codec type, default:kCodecTypeAudio
 *   kKeyChannels:      uint32_t        [*] audio channels
 *   kKeySampleRate:    uint32_t        [*] audio sample rate
 *
 *  video track formats:
 *   kKeyFormat:        eVideoCodec     [*] video codec
 *   kKeyType:          eCodecType      [*] codec type, default:kCodecTypeVideo
 *   kKeyWidth:         uint32_t        [*] video width
 *   kKeyHeight:        uint32_t        [*] video height
 *   kKeyavcC:          sp<Buffer>      [ ] h264 avcC data
 *   kKeyhvcC:          sp<Buffer>      [ ] hevc hvcC data
 *
 *  video pixel formats:
 *   kKeyFormat:        ePixelFormat    [*] pixel format
 *   kKeyType:          eCodecType      [*] codec type, default:kCodecTypeVideo   
 *   kKeyWidth:         uint32_t        [*] video width
 *   kKeyHeight:        uint32_t        [*] video height
 *
 *  common track formats:
 *   kKeyBitrate:       uint32_t        [ ] bit rate
 */

/**
 * File Device:
 *  input formats:
 *   kKeyContent:       sp<ABuffer>     [*] media content
 *
 *  input options:
 *
 *  output formats:
 *   kKeyFormat:        eFileFormat     [*] file format
 *   kKeyDuration:      int64_t         [ ] file duration in us
 *   kKeyBitrate:       uint32_t        [ ] bit rate
 *   kKeyMetaData:      sp<Message>     [ ] file meta data
 *   kKeyCount:         uint32_t        [ ] track count, default:1
 *   kKeyTrack + i:     sp<Message>     [*] audio/video/subtitle track format
 *
 *  configure options:
 *   kKeySeek:          int64_t         [ ] perform seek
 *   kKeyTracks:        uint32_t        [ ] perform track select based on track mask
 *
 * Codec Device:
 *  input formats:
 *   ... audio/video track formats
 *   kKeyRequestFormat: uint32_t        [ ] track request format
 *
 *  input options:
 *   kKeyMode:          eModeType       [ ] codec mode
 *   kKeyPause:         bool            [ ] pause/unpause codec, some codec may need this
 *
 *  output formats:
 *   ... sample formats/pixel formats
 *
 * Output Device:
 *  input formats:
 *   ... sample formats/pixel formats
 *
 *  input options:
 *
 *  output formats:
 *   ... sample formats/pixel formats
 *   kKeyLatency:       int64_t         [ ] device push latency in us
 *   kKeyMode:          eBlockModeType  [ ] device push mode, default:kModeBlock
 *
 *  configure options:
 *   kKeyPause:         bool            [ ] pause/unpause device
 */

__BEGIN_DECLS

#pragma mark Common Keys
// common keys for media device
enum {
    // common keys
    kKeyContent         = FOURCC('cont'),       ///< sp<ABuffer>
    kKeyFormat          = FOURCC(' fmt'),       ///< uint32_t, @see eFileFormat/eAudioCodec/eVideoCodec/eSampleFormat/ePixelFormat
    kKeyRequestFormat   = FOURCC('!fmt'),       ///< uint32_t
    kKeyType            = FOURCC('type'),       ///< uint32_t, @see eCodecType
    kKeyMode            = FOURCC('mode'),       ///< uint32_t,
    kKeySeek            = FOURCC('seek'),       ///< int64_t, us
    kKeyDuration        = FOURCC('dura'),       ///< int64_t, us
    kKeyLatency         = FOURCC('late'),       ///< int64_t, us
    kKeyChannels        = FOURCC('chan'),       ///< uint32_t
    kKeySampleRate      = FOURCC('srat'),       ///< uint32_t
    kKeyChannelMap      = FOURCC('cmap'),       ///< uint32_t
    kKeyWidth           = FOURCC('widt'),       ///< uint32_t
    kKeyHeight          = FOURCC('heig'),       ///< uint32_t
    kKeyRotate          = FOURCC('?rot'),       ///< uint32_t, eRotate
    kKeyCount           = FOURCC('#cnt'),       ///< uint32_t
    kKeyBitrate         = FOURCC('btrt'),       ///< uint32_t
    kKeyTracks          = FOURCC('trak'),       ///< int32_t, bit mask
    kKeyTrack           = FOURCC('0trk'),       ///< sp<Message>
    kKeyError           = FOURCC('!err'),       ///< int32_t, MediaError
    kKeyOpenGLContext   = FOURCC('oglt'),       ///< void *
    kKeyPause           = FOURCC('paus'),       ///< int32_t, bool
    kKeyESDS            = FOURCC('esds'),       ///< sp<Buffer>
    kKeyavcC            = FOURCC('avcC'),       ///< sp<Buffer>
    kKeyhvcC            = FOURCC('hvcC'),       ///< sp<Buffer>
    kKeyCodecSpecData   = FOURCC('#csd'),       ///< sp<Buffer>
    kKeyMetaData        = FOURCC('meta'),       ///< sp<Message>
    kKeyEncoderDelay    = FOURCC('edly'),       ///< int32_t
    kKeyEncoderPadding  = FOURCC('epad'),       ///< int32_t
    
    // Microsoft codec manager data
    kKeyMicrosoftVCM    = FOURCC('MVCM'),       ///< sp<Buffer>, Microsoft VCM, exists in matroska, @see BITMAPINFOHEADER
    kKeyMicorsoftACM    = FOURCC('MACM'),       ///< sp<Buffer>, Microsoft ACM, exists in matroska, @see WAVEFORMATEX
    
    kKeyMax             = MEDIA_ENUM_MAX
};
typedef uint32_t eKeyType;

#pragma mark Meta Data Keys
// meta data keys, default value type is string
enum {
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
    
    kMetaKeyMax         = MEDIA_ENUM_MAX
};

#pragma mark Values
// key - kKeyMode
enum {
    kModeTypeNormal     = 0,                ///< use hardware accel if available
    kModeTypeSoftware,                      ///< no hardware accel.
    kModeTypePreview,
    kModeTypeDefault    = kModeTypeNormal
};
typedef uint32_t eModeType;

// key - kKeyMode
enum {
    kModeBlock      = FOURCC('!blk'),
    kModeNonBlock   = FOURCC('~blk'),
};
typedef uint32_t eBlockModeType;

__END_DECLS

#ifdef __cplusplus
__BEGIN_NAMESPACE_MPX

class API_EXPORT MediaDevice : public SharedObject {
    public:
        /**
         * create a media device with given formats and options
         * @return return reference to media device on success, or NULL on failure
         */
        static sp<MediaDevice>  create(const sp<Message>&, const sp<Message>&);
        /**
         * get formats of this media device
         * @return return message reference of this media device
         * @note device should not keep reference to this message, as client
         *       may modify its content
         */
        virtual sp<Message>     formats() const             = 0;
        /**
         * configure this media device
         * @param a message reference contains options and parameters
         * @return return kMediaNoError on success
         *         return kMediaErrorNotSupported if option is not supported
         *         return MediaError code on other cases
         */
        virtual MediaError      configure(const sp<Message>&)   = 0;
        /**
         * push a MediaFrame into this media device
         * @param a MediaFrame reference will be pushed in
         * @return return kMediaNoError on success
         *         return kMediaErrorResourceBusy if device is busy, pull and push again.
         *         return kMediaErrorInvalidOperation if push is not available
         *         return MediaError code when push after pull failed.
         * @note push a NULL MediaFrame to notify end of stream.
         * @note push maybe in block or non-block way, block by default
         */
        virtual MediaError      push(const sp<MediaFrame>&) = 0;
        /**
         * pull a MediaFrame from this media device
         * @return return MediaFrame reference on success or NULL
         * @note some devices has delays on output and will return NULL on first few pull.
         * @note pull return NULL on failure too, but next push will return MediaError code.
         */
        virtual sp<MediaFrame>  pull()                      = 0;
        /**
         * reset this media device
         * @return return kMediaNoError on success, otherwise MediaError code
         */
        virtual MediaError      reset()                     = 0;

    protected:
        MediaDevice() : SharedObject() { }
        virtual ~MediaDevice() { }

        DISALLOW_EVILS(MediaDevice);
};

__END_NAMESPACE_MPX
#endif

#endif // _MEDIA_MODULES_DEVICE_H
