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


// File:    Exif.h
// Author:  mtdcy.chen
// Changes:
//          1. 20160701     initial version
//

#ifndef MFWK_JPEG_EXIF_H
#define MFWK_JPEG_EXIF_H

#include "JPEG.h"
#include "TIFF.h"

__BEGIN_NAMESPACE_MFWK
__BEGIN_NAMESPACE(EXIF)

/**
 * @note TIFF & Exif have different values for the same tag.
 */

/**
 * The pixel composition. In JPEG compressed data, use marker instread.
 * @see kPhotometricInterpretation
 */
typedef UInt16    PixelComposition;
enum { RGB = 2, YCbCr = 6 };

/**
 * The image orientation viewed in terms of rows and columns
 * the first letter: 0th row, the second leter: 0th column
 * @see kOrientation
 */
typedef UInt16    ImageOrientation;
enum {
    TopLeft = 1,
    TopRight = 2,
    BottomRight = 3,
    BottomLeft = 4,
    LeftTop = 5,
    RightTop = 6,
    RightBottom = 7,
    LeftBottom = 8
};

/**
 * The compression scheme used for image data
 * @see kCompression
 */
typedef UInt16    CompressionScheme;
enum {
    uncompressed = 1,
    JPEG = 6            ///< thumbnails only
};

/**
 * @note about image width & height
 * @note width: kImageWidth (uncompressed) | kPixelXDimension(compressed)
 * @note height: kImageHeight (uncompressed) | kPixelYDimension(compressed)
 */

/**
 * pixel components are recorded in packed or planar
 * @see kPlanarConfiguration
 */
typedef UInt16    PlanarConfiguration;
enum { PACKED = 1, PLANAR = 2 };

/**
 * The position of chroma components in relation to luminace
 * @see kYCbCrPositioning
 */
typedef UInt16    ChromaPosition;
enum { CENTER = 1, COSITE = 2 };

/**
 * The unit for measuring XResolution & YResolution
 * @see kResolutionUnit
 */
typedef UInt16    MeasuringUnit;
enum { INCH = 2, CENTIMETER = 3 };

/**
 * The color space information
 * @see kExifColorSpace
 */
typedef UInt16    ColorSpace;
enum { sRGB = 1, Uncalibrated = 0xffff };

/**
 * The channels of each component
 * @see kExifComponentsConfiguration
 * @note RGB - [ 4 5 6 0 ]; others - [ 1 2 3 0 ]
 */
typedef UInt8     ChannelComponent;
enum { CC0 = 0 /* no defined */, Y, Cb, Cr, R, G, B };

/**
 * The class of the program used by the camera to set exposure
 * @see kExifExposureProgram
 */
typedef UInt16    ExposureProgram;
enum { EP0 = 0 /*unknown*/, Manual, Normal, Aperture, Shutter, Creative, Action, Portrait, Landscape };

/**
 * the parameters of ISO12232
 * @see kExifSensitivityType
 */
typedef UInt16    SensitivityType;
enum { ST0 = 0 /*unknown*/, SOS, REI, ISO, SOS_REI, SOS_ISO, REI_ISO, SOS_REI_ISO };

/**
 * the metering mode
 * @see kExifMeteringMode
 */
typedef UInt16    MeteringMode;
enum { MM_Unknown = 0 /**/, Average, CenterAverage, Spot, MultiSpot, Pattern, Partial, other = 255};

/**
 * The kind of light source
 * @see kExifLightSource
 */
typedef UInt16    LightSource;
enum {
    LS_Unknown = 0,
    Daylight = 1, Fluorescent, Tungsten, Flash,
    Fine = 9, Cloudy, Shade,
    Daylight_fluorescent    = 12,   ///< D 5700 - 7100K
    Day_white_fluorescent   = 13,   ///< N 4600 - 5500K
    Cool_white_fluorescent  = 14,   ///< W 3800 - 4500K
    White_fluorescent       = 15,   ///< WW 3250 - 3800K
    Warm_white_fluorescent  = 16,   ///< L 2600 - 3250K
    Standard_light_A        = 17,
    Standard_light_B        = 18,
    Standard_light_C        = 19,
    D55 = 20, D65, D75, D50,
    ISO_studio_tungsten     = 24,
    other_light_source      = 255,
};

/**
 * Flash status
 * @see kExifFlash
 */
typedef UInt16    FlashStatus;
enum {
    // lsb bit 0
    Flash_fired                     = 0x1,
    // lsb bit 2-1
    Flash_return_no_strobe          = 0x6,  ///< 100b
    Flash_return_strobe             = 0x6,  ///< 110b
    // lsb bit 3-4
    Flash_mode_compulsory           = 0x8,  ///< 0,1000b
    Flash_mode_no_compulsory        = 0x10, ///< 1,0000b
    Flash_mode_auto                 = 0x18, ///< 1,1000b
    // lsb bit 5
    Flash_function_no               = 0x20, ///< 10,0000b
    // lsb bit 6
    Flash_red_eye                   = 0x40, ///< 100,0000b
};

/**
 * exposure mode
 * @see kExifExposureMode
 */
typedef UInt16    ExposureMode;
enum { Auto_exposure = 0, Manual_exposure = 1, Auto_bracket = 2 };

/**
 * while balance mode
 * @see kExifWhiteBalance
 */
typedef UInt16    WhiteBalance;
enum { AutoWhiteBalance = 0, ManualWhiteBalance = 1 };

/**
 * capture scene
 * @see kExifSceneCaptureType
 */
typedef UInt16    CaptureScene;
enum { StandardScene = 0, LandscapeScene, PortraitScene, NightScene };

/**
 * the direction of contrast processing
 * @see kExifContrast
 */
typedef UInt16    ContrastProcessing;
enum { NormalContrast = 0, SoftContrast, HardContrast };

/**
 * the degree of overall image gain adjustment
 * @see kExifGainControl
 */
typedef UInt16    GainControl;
enum { NoGainControl = 0, LowGainUp, HighGainUp, LowGainDown, HighGainDown };

/**
 * the direction of saturation processing
 * @see kExifSaturation
 */
typedef UInt16    SaturationProcessing;
enum { NormalSaturation = 0, LowSaturation, HighSaturation };

/**
 * the direction of sharpness processing
 * @see kExifSharpness
 */
typedef UInt16    SharpnessProcessing;
enum { NormalSharpness = 0, SoftSharpness, HardSharpness };

/**
 * the distance to the subject
 * @see kExifSubjectDistanceRange
 */
typedef UInt16    SubjectDistanceRange;
enum { UnknownDistance = 0, MacroDistance, CloseViewDistance, DistantViewDistance };

/**
 * Exif Private Tag
 */
enum {
    // Section 4.6.3 Exif specific IFD
    kExifExposureTime       = 0x829a,   ///< rational, N = 1, no def
    kExifFNumber            = 0x829d,   ///< rational, N = 1
    kExifIFD                = 0x8769,   ///< long, N = 1, no def - exif IFD1
    kExifExposureProgram    = 0x8822,   ///< short, N = 1, def:0
    ///< 0 - not defined, 1 - manual, 2 - normal program, 3 - aperture priority, 4 - shutter priority
    ///< 5 - creative program, 6 -
    kExifSpectralSensitivity = 0x8824,  ///< ascii, N = ?
    kExifGPSIFD             = 0x8825,   ///< long, N = 1, no def
    kExifISOSpeedRatings    = 0x8827,   ///< short, N = ?
    kExifOECF               = 0x8828,   ///< undefined, N = ?
    kExifSensitivityType    = 0x8830,   ///< short, N = 1
    
    kExifVersion            = 0x9000,   ///< undefined, N = 4
    kExifDateTimeOriginal   = 0x9003,   ///< ascii, N = 20
    kExifDateTimeDigitized  = 0x9004,   ///< ascii, N = 20
    kExifComponentsConfiguration    = 0x9101, ///< undefined, N = 4
    kExifCompressedBitsPerPixel     = 0x9102, ///< rational, N = 1
    
    kExifShutterSpeedValue  = 0x9201,   ///< srational, N = 1
    kExifApertureValue      = 0x9202,   ///< rational, N = 1
    kExifBrightnessValue    = 0x9203,   ///< srational, N = 1
    kExifExposureBiasValue  = 0x9204,   ///< srational, N = 1
    kExifMaxApertureValue   = 0x9205,   ///< rational, N = 1
    kExifSubjectDistance    = 0x9206,   ///< rational, N = 1
    kExifMeteringMode       = 0x9207,   ///< short, N = 1, def:0,
    ///< 0 - unknown, 1 - average, ...
    kExifLightSource        = 0x9208,   ///< short, N = 1
    kExifFlash              = 0x9209,   ///< short, N = 1
    kExifFocalLength        = 0x920a,   ///< rational, N = 1
    kExifSubjectArea        = 0x9214,   ///< short, N = 2-4
    
    kExifMakerNote          = 0x927c,   ///< undefined, N = ?
    kExifUserComment        = 0x9286,   ///< undefined, N = ?
    
    kExifSubsecTime         = 0x9290,   ///< ascii, N = ?
    kExifSubsecTimeOriginal = 0x9291,   ///< ascii, N = ?
    kExifSubsecTimeDigitized = 0x9292,  ///< ascii, N = ?
    
    kExifFlashpixVersion    = 0xA000,   ///< undefined, N = 4
    kExifColorSpace         = 0xA001,   ///< short, N = 1,
    kExifPixelXDimension    = 0xA002,   ///< short or long, N = 1,
    kExifPixelYDimension    = 0xA003,   ///< short or long, N = 1,
    kExifRelatedSoundFile   = 0xA004,   ///< ascii, N = 13
    kExifInteroperability   = 0xA005,   ///< long, N = 1, no def
    
    kExifFlashEnergy        = 0xa20b,   ///< rational, N = 1
    kExifSpatialFrequencyResponse = 0xa20c, ///< undefined, N = ?
    kExifFocalPlaneXResolution = 0xa20e,    ///< rational, N = 1
    kExifFocalPlaneYResolution = 0xa20f,    ///< rational, N = 1
    kExifFocalPlaneResolutionUnit = 0xa210, ///< short, N = 1
    kExifSubjectLocation    = 0xa214,   ///< short, N = 2
    kExifExposureIndex      = 0xa215,   ///< rational, N = 1
    kExifSensingMethod      = 0xa217,   ///< short, N = 1
    kExifFileSource         = 0xa300,   ///< undefined, N = 1, def:3
    kExifSceneType          = 0xa301,   ///< undefined, N = 1, def:1
    kExifCFAPattern         = 0xa302,   ///< undefined, N = 1
    kExifCustomRendered         = 0xa401,   ///< short, N = 1
    kExifExposureMode       = 0xa402,   ///< short, N = 1
    kExifWhiteBalance       = 0xa403,   ///< short, N = 1,
    kExifDigitalZoomRatio   = 0xa404,   ///< rational, N = 1,
    kExifFocalLengthIn35mmFilm = 0xa405,    ///< short, N = 1
    kExifSceneCaptureType   = 0xa406,   ///< short, N = 1, def:0
    kExifGainControl        = 0xa407,   ///< short, N = 1
    kExifContrast           = 0xa408,   ///< short, N = 1,
    kExifSaturation         = 0xa409,   ///< short, N = 1,
    kExifSharpness          = 0xa40a,   ///< short, N = 1,
    kExifDeviceSettingDescription = 0xa40b, ///< undefined, N = ?
    kExifSubjectDistanceRange = 0xa40c,     ///< short, N = 1
    kExifImageUniqueID      = 0xa420,   ///< ascii, N = 33
    
    // http://www.cipa.jp/std/documents/e/DC-010-2012_E.pdf
    kExifCameraOwnerName    = 0xA430,   ///< ascii, N = ?
    kExifBodySerialNumber   = 0xA431,   ///< ascii, N = ?
    kExifLensSpecification  = 0xA432,   ///< rational, N = ?
    kExifLensMake           = 0xA433,   ///< ascii, N = ?
    kExifLensModel          = 0xA434,   ///< ascii, N = ?
    kExifLensSerialNumber   = 0xA435,   ///< ascii, N = ?
};

const Char * ExifTagName(TIFF::eTag);

/**
 * GPS Attribute Information
 * @see kExifGPSIFD
 */
enum {
    kGPSVersionID   = 0x0,      ///< byte, N = 4
    kGPSLatitudeRef,            ///< ascii, N = 2
    kGPSLatitude,               ///< rational, N = 3,
    kGPSLongtitudeRef,          ///< ascii, N = 2,
    kGPSLongtitude,             ///< rational, N = 3
    kGPSAltitudeRef,            ///< byte, N = 1
    kGPSAltitude,               ///< rational, N = 1
    kGPSTimeStamp,              ///< rational, N = 3
    kGPSSatellites,             ///< ascii, N = ?
    kGPSStatus,                 ///< ascii, N = 2
    kGPSMeasureMode,            ///< ascii, N = 2
    kGPSDOP,                    ///< rational, N = 1
    kGPSSpeedRef,               ///< ascii, N = 2,
    kGPSSpeed,                  ///< rational, N = 1
    kGPSTrackRef,               ///< ascii, N = 2
    kGPSTrack,                  ///< rational, N = 1
    kGPSImgDirectionRef,        ///< ascii, N = 2,
    kGPSImgDirection,           ///< rational, N = 1
    kGPSMapDatum,               ///< ascii, n = ?
    kGPSDestLatitudeRef,        ///< ascii, N = 2,
    kGPSDestLatitude,           ///< rational, N = 3,
    kGPSDestLongitudeRef,       ///< ascii, N = 2,
    kGPSDestLongitude,          ///< rational, N = 3,
    kGPSDestBearingRef,         ///< ascii, N = 2,
    kGPSDestBearing,            ///< rational, N = 2,
    kGPSDestDistanceRef,        ///< ascii, N = 2
    kGPSDestDistance,           ///< rational, N = 1
    kGPSProcessingMethod,       ///< undefined, N = ?
    kGPSAreaInformation,        ///< undefined, N = ?
    kGPSDateStamp,              ///< ascii, N = 11,
    kGPSDifferential,           ///< short, N = 1,
    kGPSHPositioningError,      ///< rational, N = 1
};

const Char * GPSTagName(TIFF::eTag);

/**
 * https://www.exif.org
 * http://www.cipa.jp/std/documents/e/DC-008-Translation-2016-E.pdf
 */
struct AttributeInformation : public JPEG::Segment {
    AttributeInformation() : JPEG::Segment(JPEG::APP1) { }
    
    sp<TIFF::ImageFileDirectory>    IFD0;   // IFD0 for primary image data
    sp<TIFF::ImageFileDirectory>    Exif;   // Exif private tags
    sp<TIFF::ImageFileDirectory>    GPS;    // gps info tag
    sp<TIFF::ImageFileDirectory>    IFD1;   // IFD1 for thumbnail data
    sp<JPEG::JIFObject>             Thumb;  // compressd thumbnail in JIF
};

struct FlashpixExtension : public JPEG::Segment {
    FlashpixExtension() : JPEG::Segment(JPEG::APP2) { }
};

/**
 * BitReader: without marker & length field
 */
sp<AttributeInformation> readAttributeInformation(const sp<ABuffer>&, UInt32);

sp<FlashpixExtension> readFlashpixExtension(const sp<ABuffer>&);

void printAttributeInformation(const sp<AttributeInformation>&);

__END_NAMESPACE(EXIF)
__END_NAMESPACE_MFWK

#endif // MFWK_JPEG_EXIF_H
