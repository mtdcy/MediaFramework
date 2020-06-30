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

/**
 * File Device:
 *
 *
 * Codec Device:
 *
 *
 * Output Device:
 *
 *
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
    kKeyTrack           = FOURCC('0trk'),       ///< int32_t
    kKeyOpenGLContext   = FOURCC('oglt'),       ///< void *
    kKeyPause           = FOURCC('paus'),       ///< int32_t, bool
    kKeyESDS            = FOURCC('esds'),       ///< sp<Buffer>
    kKeyavcC            = FOURCC('avcC'),       ///< sp<Buffer>
    kKeyhvcC            = FOURCC('hvcC'),       ///< sp<Buffer>
    kKeyCodecSpecData   = FOURCC('#csd'),       ///< sp<Buffer>
    kKeyTags            = FOURCC('tag0'),       ///< sp<Message>
    kKeyEncoderDelay    = FOURCC('edly'),       ///< int32_t
    kKeyEncoderPadding  = FOURCC('epad'),       ///< int32_t
    
    // Microsoft codec manager data
    kKeyMicrosoftVCM    = FOURCC('MVCM'),       ///< sp<Buffer>, Microsoft VCM, exists in matroska, @see BITMAPINFOHEADER
    kKeyMicorsoftACM    = FOURCC('MACM'),       ///< sp<Buffer>, Microsoft ACM, exists in matroska, @see WAVEFORMATEX
};

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
};

#pragma mark Values
// key - kKeyMode
enum eModeType {
    kModeTypeNormal     = 0,                ///< use hardware accel if available
    kModeTypeSoftware,                      ///< no hardware accel.
    kModeTypePreview,
    kModeTypeDefault    = kModeTypeNormal
};

// key - kKeyMode
enum {
    kModeBlock      = FOURCC('!blk'),
    kModeNonBlock   = FOURCC('~blk'),
};

__END_DECLS

#ifdef __cplusplus
__BEGIN_NAMESPACE_MPX

class MediaDevice : public SharedObject {
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
