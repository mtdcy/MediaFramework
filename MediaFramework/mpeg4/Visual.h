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


// File:    Video.h
// Author:  mtdcy.chen
// Changes: 
//          1. 20181126     initial version
//

#ifndef _MEDIA_MODULES_MPEG4_VISUAL_H
#define _MEDIA_MODULES_MPEG4_VISUAL_h

#include <MediaToolkit/Toolkit.h>

// ISO/IEC 14496-2 Visual
namespace mtdcy { namespace MPEG4 {

    enum eVisualCode {
        kVideoObjectFirstStartCode      = 0,
        kVideoObjectLastStartCode       = 0x1F,
        kVideoObjectLayerFirstStartCode = 0x20,
        kVideoObjectLayerLastStartCode  = 0x2F,
        kVisualObjectSequenceStartCode  = 0xB0,
        kVisualObjectSequenceEndCode    = 0xB1,
        kUserDataStartCode              = 0xB2,
        kGroupOfVOPStartCode            = 0xB3,
        kVideoSessionErrorCode          = 0xB4,
        kVisualObjectStartCode          = 0xB5,
        kVOPStartCode                   = 0xB6,
        kSliceStartCode                 = 0xB7,
        kExtensionStartCode             = 0xB8,
        kFgsVopStartCode                = 0xB9,
        kFbaObjectStartCode             = 0xBA,
        kFbaObjectPlaneStartCode        = 0xBB,
        kMeshObjectStartCode            = 0xBC,
        kMeshObjectPlaneStartCode       = 0xBD,
        kStillTextureObjectStartCode    = 0xBE,
        kTextureSpatialLayerStartCode   = 0xBF,
        kTextureSnrLayerStartCode       = 0xC0,
        kTextureTileStartCode           = 0xC1,
        kTextureShapeLayerStartCode     = 0xC2,
        kStuffingStartCode              = 0xC3,
    };

}; };
#endif // _MEDIA_MODULES_MPEG4_VISUAL.h
