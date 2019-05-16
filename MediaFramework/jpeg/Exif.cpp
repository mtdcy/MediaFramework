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


// File:    Exif.cpp
// Author:  mtdcy.chen
// Changes:
//          1. 20160701     initial version
//

#define LOG_TAG "Exif"
#define LOG_NDEBUG 0
#include "Exif.h"

__BEGIN_NAMESPACE_MPX
__BEGIN_NAMESPACE(EXIF)

const char * ExifTagName(TIFF::eTag tag) {
    switch (tag) {
        case kExifExposureTime:     return "ExifExposureTime";
        case kExifFNumber:          return "ExifFNumber";
        case kExifIFD:              return "ExifIFD";
        case kExifExposureProgram:  return "ExifExposureProgram";
        case kExifGPSIFD:           return "ExifGPSIFD";
        case kExifSpectralSensitivity:  return "ExifSpectralSensitivity";
        case kExifISOSpeedRatings:  return "ExifISOSpeedRatings";
        case kExifOECF:             return "ExifOECF";
            
        case kExifVersion:          return "ExifVersion";
        case kExifDateTimeOriginal: return "ExifDateTimeOriginal";
        case kExifDateTimeDigitized:return "ExifDateTimeDigitized";
        case kExifComponentsConfiguration:  return "ExifComponentsConfiguration";
        case kExifCompressedBitsPerPixel:   return "ExifCompressedBitsPerPixel";
            
        case kExifShutterSpeedValue:    return "ExifShutterSpeedValue";
        case kExifApertureValue    :    return "ExifApertureValue";
        case kExifBrightnessValue  :    return "ExifBrightnessValue";
        case kExifExposureBiasValue:    return "ExifExposureBiasValue";
        case kExifMaxApertureValue :    return "ExifMaxApertureValue";
        case kExifSubjectDistance  :    return "ExifSubjectDistance";
        case kExifMeteringMode     :    return "ExifMeteringMode";
        case kExifLightSource      :    return "ExifLightSource";
        case kExifFlash            :    return "ExifFlash";
        case kExifFocalLength      :    return "ExifFocalLength";
        case kExifFlashEnergy      :    return "ExifFlashEnergy";
        case kExifSubjectArea      :    return "ExifSubjectArea";
            
        case kExifMakerNote:            return "ExifMakerNote";
        case kExifUserComment:          return "ExifUserComment";
            
        case kExifSubsecTime:           return "ExifSubsecTime";
        case kExifSubsecTimeOriginal:   return "ExifSubsecTimeOriginal";
        case kExifSubsecTimeDigitized:  return "ExifSubsecTimeDigitized";
            
        case kExifFlashpixVersion:  return "ExifFlashpixVersion";
        case kExifColorSpace:       return "ExifColorSpace";
        case kExifPixelXDimension:  return "ExifPixelXDimension";
        case kExifPixelYDimension:  return "ExifPixelYDimension";
        case kExifInteroperability: return "ExifInteroperability";
        case kExifRelatedSoundFile: return "ExifRelatedSoundFile";
            
        case kExifSpatialFrequencyResponse:     return "ExifSpatialFrequencyResponse";
        case kExifFocalPlaneXResolution:        return "ExifFocalPlaneXResolution";
        case kExifFocalPlaneYResolution:        return "ExifFocalPlaneYResolution";
        case kExifFocalPlaneResolutionUnit:     return "ExifFocalPlaneResolutionUnit";
        case kExifSubjectLocation:              return "ExifSubjectLocation";
        case kExifExposureIndex:                return "ExifExposureIndex";
        case kExifSensingMethod:                return "ExifSensingMethod";
        case kExifFileSource:                   return "ExifFileSource";
        case kExifSceneType:                    return "ExifSceneType";
        case kExifCFAPattern:                   return "ExifCFAPattern";
        case kExifCustomRendered:               return "ExifCustomRendered";
        case kExifExposureMode:                 return "ExifExposureMode";
        case kExifWhiteBalance:                 return "ExifWhiteBalance";
        case kExifDigitalZoomRatio:             return "ExifDigitalZoomRatio";
        case kExifFocalLengthIn35mmFilm:        return "ExifFocalLengthIn35mmFilm";
        case kExifSceneCaptureType:             return "ExifSceneCaptureType";
        case kExifGainControl:                  return "ExifGainControl";
        case kExifContrast:                     return "ExifContrast";
        case kExifSaturation:                   return "ExifSaturation";
        case kExifSharpness:                    return "ExifSharpness";
        case kExifDeviceSettingDescription:     return "ExifDeviceSettingDescription";
        case kExifSubjectDistanceRange:         return "ExifSubjectDistanceRange";
        case kExifImageUniqueID:                return "ExifImageUniqueID";
            
        case kExifCameraOwnerName:      return "ExifCameraOwnerName";
        case kExifBodySerialNumber:     return "ExifBodySerialNumber";
        case kExifLensSpecification:    return "ExifLensSpecification";
        case kExifLensMake:             return "ExifLensMake";
        case kExifLensSerialNumber:     return "ExifLensSerialNumber";
        case kExifLensModel:            return "ExifLensModel";
            
        default:                        break;
    }
    
    return TIFF::TagName(tag);
}

const char * GPSTagName(TIFF::eTag tag) {
    switch (tag) {
        case kGPSVersionID:         return "gps.VersionID";
        case kGPSLatitudeRef:       return "gps.LatitudeRef";
        case kGPSLatitude:          return "gps.Latitude";
        case kGPSLongtitudeRef:     return "gps.LongtitudeRef";
        case kGPSLongtitude:        return "gps.Longtitude";
        case kGPSAltitudeRef:       return "gps.AltitudeRef";
        case kGPSAltitude:          return "gps.Altitude";
        case kGPSTimeStamp:         return "gps.TimeStamp";
        case kGPSSatellites:        return "gps.Satellites";
        case kGPSStatus:            return "gps.Status";
        case kGPSMeasureMode:       return "gps.MeasureMode";
        case kGPSDOP:               return "gps.DOP";
        case kGPSSpeedRef:          return "gps.SpeedRef";
        case kGPSSpeed:             return "gps.Speed";
        case kGPSTrackRef:          return "gps.TrackRef";
        case kGPSTrack:             return "gps.Track";
        case kGPSImgDirectionRef:   return "gps.ImgDirectionRef";
        case kGPSImgDirection:      return "gps.ImgDirection";
        case kGPSMapDatum:          return "gps.MapDatum";
        case kGPSDestLatitudeRef:   return "gps.DestLatitudeRef";
        case kGPSDestLatitude:      return "gps.DestLatitude";
        case kGPSDestLongitudeRef:  return "gps.DestLongitudeRef";
        case kGPSDestLongitude:     return "gps.DestLongitude";
        case kGPSDestBearingRef:    return "gps.DestBearingRef";
        case kGPSDestBearing:       return "gps.DestBearing";
        case kGPSDestDistanceRef:   return "gps.DestDistanceRef";
        case kGPSDestDistance:      return "gps.DestDistance";
        case kGPSProcessingMethod:  return "gps.ProcessingMethod";
        case kGPSAreaInformation:   return "gps.AreaInformation";
        case kGPSDateStamp:         return "gps.DateStamp";
        case kGPSDifferential:      return "gps.Differential";
        case kGPSHPositioningError: return "gps.HPositioningError";
        default:                    return NULL;
    }
}

// http://www.exif.org/Exif2-2.PDF
sp<AttributeInformation> readAttributeInformation(const BitReader& br, size_t length) {
    sp<AttributeInformation> attr = new AttributeInformation;
    
    // Exif
    if (br.readS(6) != "Exif") {
        ERROR("bad Exif Attribute Information");
        return NIL;
    }
    
    // TIFF Header
    const size_t start  = br.offset() / 8;
    String id           = br.readS(2);  // 'II' - intel byte order, 'MM' - motorola byte order
    if (id == "II")     br.setByteOrder(BitReader::Little);
    else                br.setByteOrder(BitReader::Big);

    uint16_t version    = br.r16();     //
    uint32_t offset     = br.r32();     // offset of first image directory
    DEBUG("id %s, version %#x, offset %u", id.c_str(), version, offset);
    
    br.seekBytes(start + offset);
    
    // 0th IFD
    size_t next = 0;
    attr->IFD0 = TIFF::readImageFileDirectory(br, &next);
    
    // 0th IFD value
    List<TIFF::Entry *>::iterator it = attr->IFD0->mEntries.begin();
    for (; it != attr->IFD0->mEntries.end(); ++it) {
        TIFF::Entry * e = *it;
        if (e->offset != 0) {
            br.seekBytes(start + e->offset);
            TIFF::fillEntry(e, br);
        }
        
        if (e->tag == kExifIFD) {
            br.seekBytes(start + e->value[0].u32);
            attr->Exif = TIFF::readImageFileDirectory(br);
        } else if (e->tag == kExifGPSIFD) {
            br.seekBytes(start + e->value[0].u32);
            attr->GPS = TIFF::readImageFileDirectory(br);
        }
    }
    DEBUG("IFD0 end - %u @ %u", br.offset() / 8, br.length() / 8);
    
    if (next) {
        // 1th IFD
        br.seekBytes(start + next);
        attr->IFD1 = TIFF::readImageFileDirectory(br);
        
        // 1th IFD value
        uint32_t jif = 0;
        uint32_t jifLength = 0;
        List<TIFF::Entry *>::iterator it = attr->IFD1->mEntries.begin();
        for (; it != attr->IFD1->mEntries.end(); ++it) {
            TIFF::Entry * e = *it;
            if (e->offset != 0) {
                br.seekBytes(start + e->offset);
                TIFF::fillEntry(e, br);
            }
            
            if (e->tag == TIFF::kJPEGInterchangeFormat) {
                jif = e->value[0].u32;
            } else if (e->tag == TIFF::kJPEGInterchangeFormatLength) {
                jifLength = e->value[0].u32;
            }
        }
        
        // 1th IFD image data
        if (jif && jifLength) {
            DEBUG("thumbnail @ %zu, length %zu", start + jif, jifLength);
            br.seekBytes(start + jif);
            CHECK_GE(br.remains() / 8, jifLength);
            attr->Thumb = JPEG::readJIFLazy(br, jifLength);
        }
        DEBUG("IFD1 end - %u @ %u", br.offset() / 8, br.length() / 8);
        // FIXME: there are two bytes after IFD1
    }
    
    DEBUG("offset - %u @ %u", br.offset() / 8, br.length() / 8);
    
    return attr;
}

void printAttributeInformation(const sp<AttributeInformation>& attr) {
    INFO("IFD0 For Primary Image Data:");
    TIFF::printImageFileDirectory(attr->IFD0, ExifTagName);
    if (attr->Exif != NIL) {
        INFO("Exif IFD:");
        TIFF::printImageFileDirectory(attr->Exif, ExifTagName);
    }
    if (attr->GPS != NIL) {
        INFO("GPS IFD:");
        TIFF::printImageFileDirectory(attr->GPS, GPSTagName);
    }
    if (attr->IFD1 != NIL) {
        INFO("IFD1 For Thumbnail Data:");
        TIFF::printImageFileDirectory(attr->IFD1);
    }
    if (attr->Thumb != NIL) {
        INFO("Exif Thumbnail:");
        JPEG::printJIFObject(attr->Thumb);
    }
    
}

__END_NAMESPACE(EXIF)
__END_NAMESPACE_MPX